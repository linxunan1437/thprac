#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_th08_entries.py -- 自检 docs/logger/th08_checkpoints.csv 第 1 列（`时间轴位置`）

用途
----
A 代 th08 的 checkpoint 第 1 列记 **`时间值/0x偏移`**（= thprac `ECLWarp` 的**第 1 个 pair**，
见 `docs/th08/th08_ecl_re_report.md` §9.2）。本脚本只读，做三件事：

1. **CSV 形态**：数据行数 = 158 = 章节 37 + boss 121；每行恰 4 列；第 1 列非空者匹配
   `^\\d+/0x[0-9A-F]+$`；章节行 / boss 行的记录名形态；非第 1 列逐字未变（可选：与 git HEAD 比对）。
2. **取值与源码一致**：重新解析 `thprac/src/thprac/thprac_th08.cpp` 的 `THStageWarp`
   （道中，`stage` 实参 = `mStage+1`）与 `THPatch`（boss，按 `TH08_*` 枚举名），
   取 `ECLWarp` 的**第 1 个 pair**；`if (dlg) A else B` 的双值行**一律取非 dlg（后者）**。
3. **.ecl 字节复算**：按关定位 `../exes/ecl/th08/ecldata*.ecl`，从 ECL 头
   （`+0` magic `0x800`、`+4` i16 subCount、`+6` i16 slotCount、`+8` u32[slotCount] 车道初始命令、
   `+0x48` u32[subCount] sub 起始偏移表）出发：
   - 沿**时间轴车道**（slot）走命令链，命令头 = `i32 time @+0`、`i16 opcode @+4`、
     **`u8 size @+6`**（引擎 `0x42AD3D: movzx ecx, byte ptr [eax+6]`）；
   - 沿**各 sub** 走主脚本指令链，指令头 = `i32 time @+0`、`i16 opcode @+4`、`i16 length @+6`；
   然后检查 CSV 的每个 `offset` 是否正好是一条命令 / 指令的起点，并给出该处的 `time`。

口径与结论见 `docs/th08/th08_checkpoint_entry_report.md`。

用法
----
    python3 docs/logger/tools/verify_th08_entries.py [--git]

`--git` 额外用 `git show HEAD:docs/logger/th08_checkpoints.csv` 比对「非第 1 列是否被改动」。
需要仓库外只读素材 `../exes/ecl/th08/`（不进 git）。
"""

import argparse
import csv
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
CSV_PATH = os.path.join(ROOT, 'docs', 'logger', 'th08_checkpoints.csv')
SRC_PATH = os.path.join(ROOT, 'thprac', 'src', 'thprac', 'thprac_th08.cpp')
ECL_DIR = os.path.abspath(os.path.join(ROOT, '..', 'exes', 'ecl', 'th08'))

# mChapterSetup[10][2]（thprac_th08.cpp:799-810）
MCHAPTER_SETUP = [[1, 1], [4, 0], [2, 1], [4, 2], [4, 2], [3, 2], [2, 0], [2, 0], [3, 4], [0, 0]]
# TH08_STAGE_SELECT[11]（thprac_locale_def.cpp:11561）：mStage i ↔ 第 i 个 token
TOKENS = ['ST1', 'ST2', 'ST3', 'ST4A', 'ST4B', 'ST5', 'ST6A', 'ST6B', 'ST7', 'ST8']
# ECL 文件表（thprac_th08.cpp 的 stage 实参 = mStage+1；文件序 ecldata1..8，4a/4b 分列）
STAGE_FILE = {
    'ST1': 'ecldata1.ecl', 'ST2': 'ecldata2.ecl', 'ST3': 'ecldata3.ecl',
    'ST4A': 'ecldata4a.ecl', 'ST4B': 'ecldata4b.ecl', 'ST5': 'ecldata5.ecl',
    'ST6A': 'ecldata6.ecl', 'ST6B': 'ecldata7.ecl', 'ST7': 'ecldata8.ecl',
}
CHAP_RE = re.compile(r'^(ST\d+[A-B]?) (#\d+|前半 #\d+|后半 #\d+)$')
ENTRY_RE = re.compile(r'^\d+/0x[0-9A-F]+$')
# ECLWarp 的辅助 lane（第 2–4 个 pair / 各关车道初值）：一律不得登记为 checkpoint
AUX_OFFSETS = {0xad18, 0xc0cc, 0xb91c, 0xbb74, 0xc0ac, 0xbafc, 0xbed4,
               0xdd80, 0xe158, 0x1535c, 0x155c0, 0x15824, 0x15d60}


# --------------------------------------------------------------------------- #
# 源码：THStageWarp / THPatch → ECLWarp 第 1 个 pair
# --------------------------------------------------------------------------- #
def _line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def _eclewarp_calls(text, base, whole):
    out = []
    for m in re.finditer(r'ECLWarp\(\s*([^)]*)\)', text):
        args = [a.strip() for a in m.group(1).split(',')]
        out.append({'args': args, 'line': _line_of(whole, base + m.start())})
    return out


def _pick(calls):
    """非 dlg 值：`if (thPracParam.dlg) A else B` ⇒ 后者；单值 ⇒ 本身。"""
    return calls[-1] if len(calls) > 1 else (calls[0] if calls else None)


def parse_source():
    src = open(SRC_PATH, encoding='utf-8').read()
    i_stage = src.index('void THStageWarp(')
    i_patch = src.index('void THPatch(')
    i_sect = src.index('void THSectionPatch(')
    stage_txt, patch_txt = src[i_stage:i_patch], src[i_patch:i_sect]

    chap = {}
    marks = [(m.start(), int(m.group(1)))
             for m in re.finditer(r'(?:else )?if \(stage == (\d+)\)', stage_txt)]
    marks.append((len(stage_txt), None))
    for i in range(len(marks) - 1):
        stage = marks[i][1]
        body = stage_txt[marks[i][0]:marks[i + 1][0]]
        for cm in re.finditer(r'case (\d+):(.*?)break;', body, re.S):
            portion = int(cm.group(1))
            chap[(stage, portion)] = _eclewarp_calls(
                cm.group(2), i_stage + marks[i][0] + cm.start(2), src)

    boss = {}
    marks = [(m.start(), m.group(1))
             for m in re.finditer(r'case THPrac::TH08::(TH08_\w+):', patch_txt)]
    marks.append((len(patch_txt), None))
    for i in range(len(marks) - 1):
        name = marks[i][1]
        body = patch_txt[marks[i][0]:marks[i + 1][0]]
        boss[name] = _eclewarp_calls(body, i_patch + marks[i][0], src)
    return chap, boss


# --------------------------------------------------------------------------- #
# .ecl：车道命令链 + sub 指令链
# --------------------------------------------------------------------------- #
_CACHE = {}


def parse_ecl(fn):
    if fn in _CACHE:
        return _CACHE[fn]
    d = open(os.path.join(ECL_DIR, fn), 'rb').read()
    assert struct.unpack_from('<I', d, 0)[0] == 0x800, fn
    sub_count = struct.unpack_from('<h', d, 4)[0]
    slot_count = struct.unpack_from('<h', d, 6)[0]
    subs = list(struct.unpack_from('<%dI' % sub_count, d, 0x48))
    slot_init = [struct.unpack_from('<I', d, 8 + 4 * i)[0] for i in range(slot_count)]

    lanes = {}          # offset → 车道命令
    for si, start in enumerate(slot_init):
        o, guard = start, 0
        while 0 <= o < len(d) - 8 and guard < 200000:
            t = struct.unpack_from('<i', d, o)[0]
            if t < 0:
                break
            size = d[o + 6]
            lanes[o] = {'time': t, 'size': size, 'lane': si,
                        'opcode': struct.unpack_from('<h', d, o + 4)[0],
                        'arg0': struct.unpack_from('<i', d, o + 8)[0]}
            if size <= 0:
                break
            o += size
            guard += 1

    insns = {}          # offset → 主脚本指令
    for si, start in enumerate(subs):
        o, guard = start, 0
        while 0 <= o < len(d) - 8 and guard < 200000:
            t, _op, ln, _mask = struct.unpack_from('<Ihhi', d, o)
            if ln <= 0:
                break
            insns[o] = {'time': t, 'len': ln, 'sub': si}
            o += ln
            guard += 1
    _CACHE[fn] = {'data': d, 'lanes': lanes, 'insns': insns,
                  'slot_init': slot_init, 'subs': subs}
    return _CACHE[fn]


# --------------------------------------------------------------------------- #
# 主流程
# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--git', action='store_true', help='比对 git HEAD 的非第 1 列')
    opts = ap.parse_args()

    fail = []
    rows = list(csv.reader(open(CSV_PATH, encoding='utf-8')))
    header, data = rows[0], rows[1:]

    # 1) CSV 形态 ------------------------------------------------------------
    print('== CSV 形态 ==')
    print('表头            :', header)
    if header != ['时间轴位置', '所属游戏', '所属关卡', '记录名']:
        fail.append('表头不是 A 代口径')
    print('数据行          :', len(data), '(期望 158)')
    if len(data) != 158:
        fail.append('数据行数 != 158')
    if any(len(r) != 4 for r in data):
        fail.append('存在字段数 != 4 的行')
    chap = [r for r in data if CHAP_RE.match(r[3])]
    boss = [r for r in data if not CHAP_RE.match(r[3])]
    print('章节行 / boss 行 :', len(chap), '/', len(boss), '(期望 37 / 121)')
    if (len(chap), len(boss)) != (37, 121):
        fail.append('章节/boss 行数不是 37/121')
    filled = [r for r in data if r[0].strip()]
    print('第 1 列非空      :', len(filled), '(期望 141 = 37 章节 + 104 boss)')
    if len(filled) != 141:
        fail.append('非空行数 != 141')
    bad_fmt = [r[3] for r in filled if not ENTRY_RE.match(r[0])]
    if bad_fmt:
        fail.append('格式不符: %s' % bad_fmt)
    print('格式 ^\\d+/0x[0-9A-F]+$ :', 'OK' if not bad_fmt else 'FAIL')
    multi = [r[3] for r in data if r[0].count('/') > 1 or ' ' in r[0].strip()]
    if multi:
        fail.append('第 1 列含多余分隔/空格: %s' % multi)

    # 2) 源码取值 ------------------------------------------------------------
    chap_src, boss_src = parse_source()
    mstage_of = {tok: i for i, tok in enumerate(TOKENS)}
    expect = {}
    for i, (a, b) in enumerate(MCHAPTER_SETUP[:9]):
        tok = TOKENS[i]
        for k in range(1, a + b + 1):
            name = (f'{tok} #{k}' if b == 0 else
                    f'{tok} 前半 #{k}' if k <= a else f'{tok} 后半 #{k - a}')
            call = _pick(chap_src[(i + 1, k)])
            expect[name] = (int(call['args'][0]), int(call['args'][1], 16), call['line'], 'chapter')
    for r in boss:
        if r[3].startswith('LW '):
            # `TH08_LW_1..17` 在源码里是空 case（break），无 ECLWarp ⇒ 期望留空
            expect[r[3]] = None
            continue
        enum = 'TH08_' + r[3].replace(' ', '_', 1)
        call = _pick(boss_src[enum])
        if call:
            expect[r[3]] = (int(call['args'][0]), int(call['args'][1], 16), call['line'], 'boss')
        else:
            expect[r[3]] = None

    print('\n== 源码一致 ==')
    src_bad = []
    for r in data:
        name = r[3]
        if name not in expect:
            src_bad.append((name, 'not in source map'))
            continue
        e = expect[name]
        if e is None:
            if r[0].strip():
                src_bad.append((name, '源码无 ECLWarp，但 CSV 非空'))
            continue
        want = '%d/0x%X' % (e[0], e[1])
        if r[0].strip() != want:
            src_bad.append((name, 'CSV=%r 期望=%r' % (r[0], want)))
    print('逐行与 THStageWarp/THPatch 比对 :', 'OK' if not src_bad else 'FAIL')
    for x in src_bad:
        print('   ', x)
        fail.append('源码不一致 %s' % (x,))

    # 3) .ecl 复算 -----------------------------------------------------------
    print('\n== .ecl 复算 ==')
    hit = miss = 0
    time_diff = []
    lane_of_kind = {}
    for r in data:
        if not r[0].strip():
            continue
        tok = r[2]
        if tok == 'ST8':
            continue
        t, off = r[0].split('/')
        t, off = int(t), int(off, 16)
        e = parse_ecl(STAGE_FILE[tok])
        where = None
        if off in e['lanes']:
            where = 'lane%d' % e['lanes'][off]['lane']
            cmd_t = e['lanes'][off]['time']
        elif off in e['insns']:
            where = 'sub%d' % e['insns'][off]['sub']
            cmd_t = e['insns'][off]['time']
        if where is None:
            miss += 1
            print('   MISS %-14s %s/%s %s' % (r[3], t, hex(off), STAGE_FILE[tok]))
            fail.append('offset 非指令起点 %s' % r[3])
            continue
        hit += 1
        lane_of_kind.setdefault(where.split('0')[0], []).append(r[3])
        if cmd_t != t:
            time_diff.append((r[3], t, off, cmd_t))
        if off in AUX_OFFSETS:
            fail.append('辅助 lane 被登记: %s' % r[3])
    print('offset 反查命中   : %d/%d' % (hit, hit + miss))
    print('反查落点类别      :', {k: len(v) for k, v in lane_of_kind.items()})
    print('命令自身 time != CSV time : %d 行' % len(time_diff))
    for x in time_diff:
        print('   %-14s csv=%d off=0x%X cmd=%d (Δ=%+d)' % (x[0], x[1], x[2], x[3], x[3] - x[1]))
    print('辅助 lane 误登记  :', 'OK（无）' if not any('辅助 lane' in f for f in fail) else 'FAIL')

    # 车道命令链 vs sub 指令链：两套坐标系必须不相交（th08 A 代特征）
    print('\n-- 车道命令链 ∩ sub 指令链（9 关）--')
    for tok, fn in STAGE_FILE.items():
        e = parse_ecl(fn)
        inter = set(e['lanes']) & set(e['insns'])
        print('   %-5s %-14s lanes=%-4d insns=%-4d 交集=%d'
              % (tok, fn, len(e['lanes']), len(e['insns']), len(inter)))
        if inter:
            fail.append('%s 车道与 sub 指令链有交集' % fn)

    # 4) 非第 1 列未改动（可选）---------------------------------------------
    if opts.git:
        print('\n== git HEAD 比对（仅第 1 列可变）==')
        try:
            head = subprocess.run(['git', 'show', 'HEAD:docs/logger/th08_checkpoints.csv'],
                                  cwd=ROOT, capture_output=True, text=True, check=True).stdout
            old = list(csv.reader(head.splitlines()))[1:]
            changed = [i for i, (a, b) in enumerate(zip(old, data)) if a[1:] != b[1:]]
            print('非第 1 列被改动的行 :', changed if changed else '无')
            if changed:
                fail.append('非第 1 列被改动')
        except Exception as exc:                      # noqa: BLE001 - 只读自检，不致命
            print('   跳过（%s）' % exc)

    print('\n== 结论 ==')
    if fail:
        print('FAIL (%d 项)' % len(fail))
        for f in fail:
            print('  -', f)
        return 1
    print('PASS：158 行 = 章节 37 + boss 121；已填 141（章节 37 + boss 104）；'
          '17 行 LW 有意留空（用户 2026-10-22 决定）')
    return 0


if __name__ == '__main__':
    sys.exit(main())

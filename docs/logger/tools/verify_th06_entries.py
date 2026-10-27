#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_th06_entries.py -- 自检 docs/logger/th06_checkpoints.csv 第 1 列（`时间轴位置`）

用途
----
A 代 th06 的 checkpoint 第 1 列记 **`时间值/0x落点偏移`**（用户 2026-10-26 裁决，与 th08 同形）：

- `时间值`     = 该行对应的 `ECLWarp(...)` 的**非 dlg 入参**（十进制；源码 `0x…` 写法换算成十进制；
  `if (thPracParam.dlg) A; else B;` ⇒ 取 **B**，因为 `dlg` 默认 `false`，练习默认走非 dlg 路径）；
- `0x落点偏移` = **第一条 `time >= warp` 的时间轴指令**在 ECL 文件内的偏移（运行期即 `tlInstr − pCode`）。

本脚本只读，做五件事：

1. **CSV 形态**：数据行 = **109** = 章节 **39** + boss **70**；每行恰 4 列；第 1 列
   109/109 非空且匹配 `^\\d+/0x[0-9A-F]+$`；逐关合计 / 章节各关行数；第 2–4 列与 git HEAD 逐字一致（可选）。
2. **源码一致（时间值）**：重新解析 `thprac/src/thprac/thprac_th06.cpp` 的 `THStageWarp`
   （`stage` = `mStage+1`，`case` 序号 = portion）与 `THPatch`（按 `TH06_*` 枚举名，
   **brace-depth 感知**地取该 `case` 体，不会被 `ST4_BOOKS` / `ST5_BOSS6` / `ST6_BOSS9`
   内部的 `switch/break` 截断），取该 case 的 **`ECLWarp` 值**：`if (thPracParam.dlg) A; else B;`
   ⇒ **一律取 B（非 dlg / else 分支）** —— 依据：`thPracParam.dlg` 是普通 `GuiCheckBox`
   （`thprac_gui_components.h` 的 `bool mToggle = false`），**默认 `dlg = false`**，练习默认走非 dlg 路径；
   与 th07 / th08 的同代口径一致。**dlg 变体值只登记、不参与判定**（见步骤 2 输出里的「dlg 变体」段）。
   经 lambda 间接 warp 的（`s2b_nd` / `s3b_n1` / `s7b_n1`）**展开**（`s4b_time` 体内无 `ECLWarp`，登记为事实）。
3. **.ecl 复算（落点偏移）**：7 份 `../exes/ecl/th06/ecldataN.ecl`（N = 关卡号）离线解码 ——
   头 `+0x00` i16 subCount、`+0x04` u32 timelineOffset、`+0x10` u32[subCount] sub 偏移表；
   时间轴指令头 8 B `{i16 time; i16 arg0; i16 op; i16 size}`、参数自 `+8` 起、按 `+size` 前进、
   `time < 0` 结束。逐行算「第一条 `time >= warp`」，与 CSV 的偏移**逐行相等**；
   并校验该偏移是**某条时间轴指令的起点**、落在时间轴指令区内（不是指令中间）。
4. **与 E1 附录 C/D 逐行比对**：直接解析 `docs/th06/th06_ecl_re_report.md` §13（40 个 portion）
   / §14（70 个 boss section）的表格（warp / 落点 offset / 落点 time / 落点 op），与本次实算逐行比对；
   另单列被删除的 `ST4 前半 #4` 与保留的 `ST4 BOOKS`（同 `3392/0xCDD4`）。
5. **删行证据**：CSV 中确有 `ST4 BOOKS`、确无 `ST4 前半 #4`，且源码两处 warp 都是 `0x0D40` = 3392。

口径与结论见 `docs/logger/checkpoint_csv_workflow.md` §9/§10 与 `docs/th06/th06_ecl_re_report.md` §10/§13/§14。

用法
----
    python3 docs/logger/tools/verify_th06_entries.py [--git]

`--git` 额外用 `git show HEAD:docs/logger/th06_checkpoints.csv` 比对「非第 1 列是否被改动」
（比对前先把 HEAD 里的 `ST4 前半 #4` 行摘掉，以便与删行后的 109 行对齐）。
需要仓库外只读素材 `../exes/ecl/th06/`（不进 git）。
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
CSV_PATH = os.path.join(ROOT, 'docs', 'logger', 'th06_checkpoints.csv')
SRC_PATH = os.path.join(ROOT, 'thprac', 'src', 'thprac', 'thprac_th06.cpp')
REPORT_PATH = os.path.join(ROOT, 'docs', 'th06', 'th06_ecl_re_report.md')
ECL_DIR = os.path.abspath(os.path.join(ROOT, '..', 'exes', 'ecl', 'th06'))

# mChapterSetup[7][2]（thprac_th06.cpp:970-978）
MCHAPTER_SETUP = [[4, 2], [2, 2], [4, 3], [4, 5], [3, 2], [2, 0], [4, 3]]
EXPECT_ROWS = 109
EXPECT_CHAP = 39
EXPECT_BOSS = 70
EXPECT_PER_STAGE = {1: 12, 2: 10, 3: 16, 4: 17, 5: 13, 6: 13, 7: 28}
EXPECT_CHAP_PER_STAGE = [6, 4, 7, 8, 5, 2, 7]
ENTRY_RE = re.compile(r'^\d+/0x[0-9A-F]+$')
CHAP_RE = re.compile(r'^ST(\d+)\s+(?:(前半|后半)\s+)?#(\d+)$')
BOSS_RE = re.compile(r'^ST(\d+)\s+(\S+)$')
# 用户 2026-10-26 裁决：保留 ST4 BOOKS（boss 行），删除 ST4 前半 #4（章节行）——
# 二者都对应 ECLWarp(0x0d40) = 3392、落点 0xCDD4。
DROP_CHAPTER = 'ST4 前半 #4'
DROP_KEY = (4, 4)          # (stage, portion)
KEPT_BOOKS_ROW = 'ST4 BOOKS'


# --------------------------------------------------------------------------- #
# 源码：THStageWarp / THPatch → ECLWarp（非 dlg 口径）
# --------------------------------------------------------------------------- #
def _line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def _brace_block(src, pos):
    """pos 指向 '{'；返回整块文本（含首尾花括号）。"""
    depth = 0
    i = pos
    while i < len(src):
        if src[i] == '{':
            depth += 1
        elif src[i] == '}':
            depth -= 1
            if depth == 0:
                return src[pos:i + 1]
        i += 1
    raise AssertionError('unbalanced braces at %d' % pos)


class Source:
    """thprac_th06.cpp 的 warp 取值器。"""

    LAMBDAS = ('s2b_nd', 's3b_n1', 's4b_time', 's7b_n1', 's7b_call')

    def __init__(self, path):
        self.src = open(path, encoding='utf-8').read()
        self._lambda = {}
        self.chap = {}
        self.boss = {}
        self._parse()

    # -- lambda ---------------------------------------------------------- #
    def lambda_warps(self, name):
        """返回 lambda 体内全部 ECLWarp 值（空列表 = 体内不 warp，如 s4b_time）。"""
        if name in self._lambda:
            return self._lambda[name]
        m = re.search(r'auto %s = \[&\]\(\)\s*\{' % name, self.src)
        assert m, name
        body = _brace_block(self.src, m.end() - 1)
        vals = [int(t.strip(), 0) for t in re.findall(r'ECLWarp\(\s*([^)]*?)\s*\)', body)]
        self._lambda[name] = vals
        return vals

    # -- case 体 --------------------------------------------------------- #
    def _warps_in(self, body, base):
        """按源码顺序返回 [(pos, kind, value|lambda_name, line)]，kind ∈ {'warp', lambda}。"""
        ev = []
        for m in re.finditer(r'ECLWarp\(\s*([^)]*?)\s*\)', body):
            ev.append((m.start(), 'warp', int(m.group(1).strip(), 0),
                       _line_of(self.src, base + m.start())))
        for name in self.LAMBDAS:
            for m in re.finditer(r'\b%s\s*\(\s*\)' % name, body):
                ev.append((m.start(), name, None, _line_of(self.src, base + m.start())))
        ev.sort()
        return ev

    def _pick_warp(self, body, base):
        """按**非 dlg** 口径取 warp（lambda 展开）。

        `if (thPracParam.dlg) A; else B;` ⇒ 取 **B（else / 非 dlg）**：`thPracParam.dlg` 是普通
        `GuiCheckBox`（默认 `mToggle = false`），练习默认走非 dlg 路径；与 th07 / th08 同口径。
        返回 dict(value, line, how, paths, variants)：`variants` = 被让过的 dlg 分支 warp 路径
        （仅登记，不参与判定）。
        """
        ev = self._warps_in(body, base)
        paths = []          # 全部 warp 路径（literal + 带 warp 的 lambda），按源码顺序
        for pos, kind, v, ln in ev:
            if kind == 'warp':
                paths.append(('ECLWarp', v, ln))
            else:
                for lv in self.lambda_warps(kind):
                    paths.append(('%s()' % kind, lv, ln))
        assert paths, 'case 内没有 warp: %r' % body[:80]
        has_dlg = 'thPracParam.dlg' in body
        chosen = paths[-1] if (has_dlg and len(paths) > 1) else paths[0]
        kind, v, ln = chosen
        return {'value': v, 'line': ln, 'how': kind, 'paths': paths,
                'variants': [p for p in paths if p is not chosen]}

    # -- 两个函数 -------------------------------------------------------- #
    def _parse(self):
        src = self.src
        i_stage = src.index('void THStageWarp(')
        i_patch = src.index('void THPatch(')
        i_sect = src.index('void THSectionPatch()')
        stage_txt = src[i_stage:i_sect]
        patch_txt = src[i_patch:i_sect]

        marks = [(m.start(), int(m.group(1)))
                 for m in re.finditer(r'\bif \(stage == (\d+)\)', stage_txt)]
        marks.append((len(stage_txt), None))
        for k in range(len(marks) - 1):
            stage = marks[k][1]
            body = stage_txt[marks[k][0]:marks[k + 1][0]]
            base = i_stage + marks[k][0]
            for cm in re.finditer(r'case (\d+):(.*?)break;', body, re.S):
                self.chap[(stage, int(cm.group(1)))] = self._pick_warp(
                    cm.group(2), base + cm.start(2))

        for m in re.finditer(r'case THPrac::TH06::(TH06_\w+):', patch_txt):
            depth = 0
            i = m.end()
            while True:
                c = patch_txt[i]
                if c == '{':
                    depth += 1
                elif c == '}':
                    depth -= 1
                elif depth == 0 and patch_txt.startswith('break;', i):
                    break
                i += 1
            self.boss[m.group(1)] = self._pick_warp(patch_txt[m.end():i], i_patch + m.end())


# --------------------------------------------------------------------------- #
# .ecl：时间轴指令表
# --------------------------------------------------------------------------- #
_CACHE = {}


def parse_ecl(stage):
    if stage in _CACHE:
        return _CACHE[stage]
    fn = 'ecldata%d.ecl' % stage
    d = open(os.path.join(ECL_DIR, fn), 'rb').read()
    sub_count = struct.unpack_from('<h', d, 0)[0]
    tl_off = struct.unpack_from('<I', d, 4)[0]
    subs = list(struct.unpack_from('<%dI' % sub_count, d, 0x10)) if sub_count else []
    ins = []
    o = tl_off
    while 0 <= o < len(d) - 8:
        t, a0, op, size = struct.unpack_from('<hhhh', d, o)
        if t < 0:
            break
        ins.append({'off': o, 'time': t, 'arg0': a0, 'op': op, 'size': size})
        if size <= 0:
            break
        o += size
    _CACHE[stage] = {'file': fn, 'size': len(d), 'sub_count': sub_count,
                     'tl_off': tl_off, 'subs': subs, 'tl': ins,
                     'starts': {x['off'] for x in ins}}
    return _CACHE[stage]


def landing(stage, warp):
    """第一条 time >= warp 的时间轴指令；找不到返回 None。"""
    for x in parse_ecl(stage)['tl']:
        if x['time'] >= warp:
            return x
    return None


# --------------------------------------------------------------------------- #
# E1 报告附录 C / D 的表格
# --------------------------------------------------------------------------- #
def parse_report_tables(path):
    """返回 (appendix_c, appendix_d)：

    C: {(stage, portion): (warp, offset, time, op)}
    D: {enum_name: (warp, offset, time, op)}
    """
    txt = open(path, encoding='utf-8').read()

    def slab(title, nxt):
        # 2026-10-27：boss 附录（原 §14 附录 D）已按用户裁决从报告中删除；
        # 标题缺失时返回空串，调用方按空表处理（不再做 boss 交叉核对）。
        if title not in txt:
            return ''
        a = txt.index(title)
        b = txt.index(nxt) if nxt in txt else len(txt)
        return txt[a:b]

    def cells(line):
        return [c.strip() for c in line.strip().strip('|').split('|')]

    def num(s):
        m = re.search(r'(\d+)', s)
        return int(m.group(1)) if m else None

    def hx(s):
        m = re.search(r'`(0x[0-9A-Fa-f]+)`', s)
        return int(m.group(1), 16) if m else None

    c, d = {}, {}
    for line in slab('## 13. 附录 C', '## 14. 附录 D').splitlines():
        if not line.startswith('|'):
            continue
        cs = cells(line)
        if len(cs) < 5 or not re.match(r'^st\d+$', cs[0]):
            continue
        c[(int(cs[0][2:]), int(cs[1]))] = (num(cs[2]), hx(cs[3]), num(cs[4]), num(cs[5]))
    for line in slab('## 14. 附录 D', '## 15. 工具与复现').splitlines():
        if not line.startswith('|'):
            continue
        cs = cells(line)
        if len(cs) < 5:
            continue
        m = re.match(r'^`(TH06_\w+)`$', cs[0])
        if not m:
            continue
        d[m.group(1)] = (num(cs[1]), hx(cs[2]), num(cs[3]), num(cs[4]))
    return c, d


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
    src = Source(SRC_PATH)

    # 1) CSV 形态 ------------------------------------------------------------
    print('== CSV 形态 ==')
    print('表头            :', header)
    if header != ['时间轴位置', '所属游戏', '所属关卡', '记录名']:
        fail.append('表头不是 A 代口径')
    print('数据行          :', len(data), '(期望 %d)' % EXPECT_ROWS)
    if len(data) != EXPECT_ROWS:
        fail.append('数据行数 != %d' % EXPECT_ROWS)
    if any(len(r) != 4 for r in data):
        fail.append('存在字段数 != 4 的行')
    print('字段数          : %s（%d 行 × %d）'
          % ('OK' if all(len(r) == 4 for r in data) else 'FAIL',
             len(data), len(data[0]) if data else 0))

    chap = [r for r in data if CHAP_RE.match(r[3])]
    boss = [r for r in data if not CHAP_RE.match(r[3])]
    print('章节行 / boss 行 :', len(chap), '/', len(boss),
          '(期望 %d / %d)' % (EXPECT_CHAP, EXPECT_BOSS))
    if (len(chap), len(boss)) != (EXPECT_CHAP, EXPECT_BOSS):
        fail.append('章节/boss 行数不是 %d/%d' % (EXPECT_CHAP, EXPECT_BOSS))

    filled = [r for r in data if r[0].strip()]
    print('第 1 列非空      :', len(filled), '(期望 %d)' % EXPECT_ROWS)
    if len(filled) != EXPECT_ROWS:
        fail.append('非空行数 != %d' % EXPECT_ROWS)
    bad_fmt = [(r[3], r[0]) for r in data if not ENTRY_RE.match(r[0])]
    print('格式 ^\\d+/0x[0-9A-F]+$ :', 'OK' if not bad_fmt else 'FAIL')
    for x in bad_fmt:
        print('   格式不符：', x)
        fail.append('格式不符 %s' % (x,))
    dirty = [r[3] for r in data if r[0] != r[0].strip() or r[0].count('/') != 1
             or ' ' in r[0]]
    if dirty:
        print('   多余分隔/空格：', dirty)
        fail.append('第 1 列含多余分隔/空格')

    per_stage = {}
    chap_per_stage = {}
    for r in data:
        st = int(r[2][2:])
        per_stage[st] = per_stage.get(st, 0) + 1
        if CHAP_RE.match(r[3]):
            chap_per_stage[st] = chap_per_stage.get(st, 0) + 1
    print('逐关合计        :', ' / '.join('ST%d %d' % (i, per_stage.get(i, 0))
                                          for i in range(1, 8)))
    if per_stage != EXPECT_PER_STAGE:
        fail.append('逐关合计不符：%s' % per_stage)
    chap_counts = [chap_per_stage.get(i, 0) for i in range(1, 8)]
    print('章节各关行数    : %s = %d' % (','.join(map(str, chap_counts)), sum(chap_counts)))
    if chap_counts != EXPECT_CHAP_PER_STAGE:
        fail.append('章节各关行数不符：%s' % chap_counts)

    # 2) 源码一致（时间值）----------------------------------------------------
    print('\n== 源码一致（时间值 = ECLWarp 的非 dlg 入参）==')
    expect = {}
    used_chap, used_boss = {}, {}
    for r in data:
        rec = r[3]
        m = CHAP_RE.match(rec)
        if m:
            stage, half, k = int(m.group(1)), m.group(2) or '', int(m.group(3))
            a = MCHAPTER_SETUP[stage - 1][0]
            portion = a + k if half == '后半' else k
            info = src.chap[(stage, portion)]
            used_chap[(stage, portion)] = rec
        else:
            enum = 'TH06_' + rec.replace(' ', '_', 1)
            info = src.boss[enum]
            used_boss[enum] = rec
        expect[rec] = info

    src_bad = []
    for r in data:
        want_t = expect[r[3]]['value']
        got_t = int(r[0].split('/')[0])
        if got_t != want_t:
            src_bad.append((r[3], got_t, want_t, expect[r[3]]['line']))
    print('逐行与 THStageWarp/THPatch 比对 :', 'OK' if not src_bad else 'FAIL')
    for x in src_bad:
        print('   %-14s CSV=%d 源码=%d (源码行 %d)' % x)
        fail.append('时间值与源码不一致 %s' % (x,))

    missing = sorted(set(src.chap) - set(used_chap))
    extra = sorted(set(used_chap) - set(src.chap))
    print('章节行 ↔ THStageWarp case：用到 %d / 源码 %d；未用的 case = %s'
          % (len(used_chap), len(src.chap), missing))
    if set(used_chap) != set(src.chap) - {DROP_KEY}:
        fail.append('章节行与源码 case 不是「除去被删行」的一一对应（extra=%s）' % extra)
    miss_boss = sorted(set(src.boss) - set(used_boss))
    print('boss 行 ↔ THPatch case    ：用到 %d / 源码 %d；未用的 case = %s'
          % (len(used_boss), len(src.boss), miss_boss if miss_boss else '无'))
    if set(used_boss) != set(src.boss):
        fail.append('boss 行与 THPatch case 不是一一对应')

    dlg_rows = [(rec, i) for rec, i in expect.items() if i['variants']]
    print('dlg 双值 section：%d 个 —— CSV 一律取**非 dlg（else 分支）**值；dlg 变体仅登记如下：'
          % len(dlg_rows))
    for rec, i in sorted(dlg_rows, key=lambda x: x[1]['line']):
        stage = int(re.match(r'^ST(\d+)', rec).group(1))
        vname, var, vline = i['variants'][0]
        vins = landing(stage, var)
        print('   %-14s 采用(非dlg)=%d/0x%X（源码行 %d）   dlg变体=%s:%d/0x%X（源码行 %d）'
              % (rec, i['value'], landing(stage, i['value'])['off'], i['line'],
                 vname, var, vins['off'], vline))
    no_warp_lambda = [n for n in Source.LAMBDAS if not src.lambda_warps(n)]
    print('体内无 ECLWarp 的 lambda     :', no_warp_lambda if no_warp_lambda else '无')

    # 3) .ecl 复算（落点偏移）--------------------------------------------------
    print('\n== .ecl 复算（落点 = 第一条 time >= warp 的时间轴指令）==')
    hit = miss = 0
    off_bad = []
    align_bad = []
    for r in data:
        stage = int(r[2][2:])
        warp = int(r[0].split('/')[0])
        off = int(r[0].split('/')[1], 16)
        e = parse_ecl(stage)
        ins = landing(stage, warp)
        if ins is None:
            miss += 1
            print('   MISS %-14s warp=%d (%s)' % (r[3], warp, e['file']))
            fail.append('找不到落点 %s' % r[3])
            continue
        hit += 1
        if ins['off'] != off:
            off_bad.append((r[3], off, ins['off'], ins['time']))
        if off not in e['starts']:
            align_bad.append((r[3], off))
        elif not (e['tl_off'] <= off and off + 8 <= e['size'] - 4):
            align_bad.append((r[3], off))
    print('落点命中         : %d/%d' % (hit, hit + miss))
    print('offset 与离线复算逐行相等 :', 'OK' if not off_bad else 'FAIL')
    for x in off_bad:
        print('   %-14s CSV=0x%X 复算=0x%X (该指令 time=%d)' % x)
        fail.append('落点偏移不一致 %s' % (x,))
    print('offset 精确对齐某条时间轴指令起点 :', 'OK' if not align_bad else 'FAIL')
    for x in align_bad:
        print('   未对齐/越界：', x)
        fail.append('offset 非指令起点 %s' % (x,))
    print('各关时间轴区间   :', ' '.join(
        'ST%d[0x%X..0x%X]' % (n, parse_ecl(n)['tl_off'],
                              parse_ecl(n)['tl'][-1]['off'] + parse_ecl(n)['tl'][-1]['size'])
        for n in range(1, 8)))

    # 4) 与 E1 附录 C / D 逐行比对 --------------------------------------------
    print('\n== 与 E1 附录 C/D 逐行比对（docs/th06/th06_ecl_re_report.md）==')
    apc, apd = parse_report_tables(REPORT_PATH)
    has_d = '## 14. 附录 D' in open(REPORT_PATH, encoding='utf-8').read()
    if has_d:
        print('附录 C 行数 %d（期望 40）/ 附录 D 行数 %d（期望 70）' % (len(apc), len(apd)))
        if (len(apc), len(apd)) != (40, 70):
            fail.append('附录 C/D 表格行数不是 40/70')
    else:
        print('附录 C 行数 %d（期望 40）；附录 D 已于 2026-10-27 撤下，跳过 boss 交叉核对' % (len(apc),))
        if len(apc) != 40:
            fail.append('附录 C 表格行数不是 40')

    c_bad, c_ok = [], 0
    for (stage, portion), (w, off, t, op) in sorted(apc.items()):
        ins = landing(stage, w)
        if ins is None:
            c_bad.append(((stage, portion), 'no landing'))
            continue
        if (ins['off'], ins['time'], ins['op']) != (off, t, op):
            c_bad.append(((stage, portion), 'C表 %s vs 实算 %s' %
                          ((hex(off), t, op), (hex(ins['off']), ins['time'], ins['op']))))
        else:
            c_ok += 1
    print('附录 C：warp/落点/落点time/落点op 逐行相等 %d/%d' % (c_ok, len(apc)))
    for x in c_bad:
        print('   ', x)
        fail.append('附录 C 不一致 %s' % (x,))

    d_bad, d_ok = [], 0
    for enum, (w, off, t, op) in sorted(apd.items()):
        stage = int(re.search(r'ST(\d+)_', enum).group(1))
        ins = landing(stage, w)
        if ins is None:
            d_bad.append((enum, 'no landing'))
            continue
        if (ins['off'], ins['time'], ins['op']) != (off, t, op):
            d_bad.append((enum, 'D表 %s vs 实算 %s' %
                          ((hex(off), t, op), (hex(ins['off']), ins['time'], ins['op']))))
        else:
            d_ok += 1
    print('附录 D：warp/落点/落点time/落点op 逐行相等 %d/%d' % (d_ok, len(apd)))
    for x in d_bad:
        print('   ', x)
        fail.append('附录 D 不一致 %s' % (x,))

    # 附录 → CSV 的逐行映射（含被删的 portion 4）
    print('附录 C ↔ CSV：39 行在表、1 行（ST4 portion 4）按裁决删除')
    csv_by_rec = {r[3]: r[0] for r in data}
    c_map_bad = []
    for (stage, portion), (w, off, t, op) in sorted(apc.items()):
        rec = None
        a = MCHAPTER_SETUP[stage - 1][0]
        if (stage, portion) == DROP_KEY:
            if DROP_CHAPTER in csv_by_rec:
                c_map_bad.append((DROP_CHAPTER, '应已删除但仍在 CSV'))
            continue
        if a == 0 or (MCHAPTER_SETUP[stage - 1][1] == 0 and portion <= a):
            rec = 'ST%d #%d' % (stage, portion)
        elif portion <= a:
            rec = 'ST%d 前半 #%d' % (stage, portion)
        else:
            rec = 'ST%d 后半 #%d' % (stage, portion - a)
        if rec not in csv_by_rec:
            c_map_bad.append((rec, 'CSV 缺行'))
        elif csv_by_rec[rec] != '%d/0x%X' % (w, off):
            c_map_bad.append((rec, 'CSV=%s 表=%d/0x%X' % (csv_by_rec[rec], w, off)))
    print('附录 C → CSV 逐行（%d 行）：%s' % (len(apc) - 1,
                                            'OK' if not c_map_bad else 'FAIL'))
    for x in c_map_bad:
        print('   ', x)
        fail.append('附录 C→CSV 不一致 %s' % (x,))

    d_map_bad, dlg_diff, d_exact = [], [], 0
    for enum, (w, off, t, op) in sorted(apd.items()):
        rec = enum[len('TH06_'):].replace('_', ' ', 1)
        if rec not in csv_by_rec:
            d_map_bad.append((rec, 'CSV 缺行'))
            continue
        got = csv_by_rec[rec]
        exact = '%d/0x%X' % (w, off)          # 附录 D 自己记的值（E1 按「第一个 ECLWarp」口径）
        if got == exact:
            d_exact += 1
            continue
        # 附录 D 记的是 dlg 变体 ⇒ 登记为口径差异（不算失败），但 CSV 必须等于「非 dlg 落地值」。
        stage = int(re.search(r'ST(\d+)_', enum).group(1))
        info = src.boss[enum]
        ins = landing(stage, info['value'])
        want = '%d/0x%X' % (info['value'], ins['off'])
        if got == want and w == info['variants'][0][1]:
            dlg_diff.append((rec, w, off, got))
        else:
            d_map_bad.append((rec, 'CSV=%s 附录D=%s 期望(非dlg)=%s' % (got, exact, want)))
    print('附录 D → CSV 逐行（%d 行）：%s'
          % (len(apd), 'OK' if not d_map_bad else 'FAIL'))
    print('   逐字一致 %d 行 + 口径差异 %d 行（附录 D 记 dlg 变体、CSV 取非 dlg；登记、不算错）：'
          % (d_exact, len(dlg_diff)))
    for rec, w, off, got in dlg_diff:
        print('      %-14s 附录D=%d/0x%X（dlg）  CSV=%s（非 dlg）' % (rec, w, off, got))
    for x in d_map_bad:
        print('   ', x)
        fail.append('附录 D→CSV 不一致 %s' % (x,))

    # ST4 关底顺序：附录 D 按序号，CSV/源码为 BOSS1,2,4,3,…
    csv_st4 = [r[3] for r in data if r[2] == 'ST4' and r[3].startswith('ST4 BOSS')]
    print('ST4 关底顺序：CSV =', ' , '.join(x.split()[-1] for x in csv_st4),
          '（源码顺序，未按序号重排；与附录 D 的排列不同、集合相同）')

    # 5) 删行证据 ------------------------------------------------------------
    print('\n== 删行裁决证据（保留 ST4 BOOKS / 删除 ST4 前半 #4）==')
    books = [r for r in data if r[3] == KEPT_BOOKS_ROW]
    print('保留行 "%s" 在 CSV 中   : %s' % (KEPT_BOOKS_ROW, 'YES' if books else 'NO'))
    if not books:
        fail.append('CSV 缺少 ST4 BOOKS')
    else:
        print('   第 1 列 = %s' % books[0][0])
    print('删除行 "%s" 在 CSV 中 : %s'
          % (DROP_CHAPTER, 'YES（错）' if DROP_CHAPTER in csv_by_rec else 'NO（已删）'))
    if DROP_CHAPTER in csv_by_rec:
        fail.append('CSV 仍含 %s' % DROP_CHAPTER)
    for tag, info in (('TH06_ST4_BOOKS（boss 行）', src.boss['TH06_ST4_BOOKS']),
                      ('THStageWarp stage4 case4', src.chap[DROP_KEY])):
        ins = landing(4, info['value'])
        print('   %-26s 源码行 %-5d warp=%d (0x%X) 落点=0x%X (time=%d op=%d)'
              % (tag, info['line'], info['value'], info['value'],
                 ins['off'], ins['time'], ins['op']))
    if src.boss['TH06_ST4_BOOKS']['value'] != src.chap[DROP_KEY]['value']:
        fail.append('ST4 BOOKS 与 portion 4 的 warp 不再相同（裁决前提变了）')

    # 6) 非第 1 列未改动（可选）---------------------------------------------
    if opts.git:
        print('\n== git HEAD 比对（仅第 1 列可变；HEAD 含被删的 ST4 前半 #4）==')
        try:
            head = subprocess.run(['git', 'show', 'HEAD:docs/logger/th06_checkpoints.csv'],
                                  cwd=ROOT, capture_output=True, text=True, check=True).stdout
            old = list(csv.reader(head.splitlines()))[1:]
            old = [r for r in old if r[3] != DROP_CHAPTER]
            print('HEAD 行数（摘掉被删行后）:', len(old), '当前:', len(data))
            changed = [i for i, (a, b) in enumerate(zip(old, data)) if a[1:] != b[1:]]
            print('非第 1 列被改动的行 :', changed if changed else '无')
            if changed:
                fail.append('非第 1 列被改动')
            if len(old) != len(data):
                fail.append('git HEAD 与当前行数不同（摘掉被删行后）')
        except Exception as exc:                      # noqa: BLE001 - 只读自检，不致命
            print('   跳过（%s）' % exc)

    print('\n== 结论 ==')
    if fail:
        print('FAIL (%d 项)' % len(fail))
        for f in fail:
            print('  -', f)
        return 1
    print('PASS：109 行 = 章节 %d + boss %d（已填 109/109，格式 `时间值/0x偏移`，'
          '**同一 section 多 warp 时取非 dlg**）；逐关 12/10/16/17/13/13/28；'
          '章节各关 6,4,7,8,5,2,7；与 E1 附录 C(40) 逐行一致；'
          '附录 D（boss）已于 2026-10-27 撤下，未做交叉核对；'
          'ST4 BOOKS 保留、ST4 前半 #4 已删（同 3392/0xCDD4）'
          % (EXPECT_CHAP, EXPECT_BOSS))
    return 0


if __name__ == '__main__':
    sys.exit(main())

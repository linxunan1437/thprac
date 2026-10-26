#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""TH06 checkpoint 入口表 + BOSS 阶段副键（SubN）自检。

只读：`../exes/ecl/th06/ecldata{1..7}.ecl`、`thprac/src/thprac/thprac_th06.cpp`、
`docs/logger/th06_checkpoints.csv`、`docs/th06/th06_ecl_re_report.md`（附录 C/D 比对）。
不联网、不编译、不写任何文件。

用法（仓库根执行）：
    python3 docs/logger/tools/verify_th06_subkeys.py            # 全量自检
    python3 docs/logger/tools/verify_th06_subkeys.py --graph    # 附带每关相位 sub 链
    python3 docs/logger/tools/verify_th06_subkeys.py --table    # 附带 109 行入口表（Markdown）

判据（与 `docs/th06/th06_checkpoint_entry_report.md` §1 同口径）：
  * 时间值   = 该 section 体内**第 1 个** `ECLWarp(...)` 入参（lambda 内按调用点展开）；
  * 落点偏移 = 该关时间轴上**第一条 time >= 时间值** 的指令的文件偏移；
  * 副键     = boss 敌机 `*(u16*)(E+0x9D8)` 的当前 sub 索引（→ `SubN`），
               静态推导 = 「该 section 补丁落点最深处的 sub」；若该 sub 上有
               强制交接补丁（`ECLSetTime`/`ECLStall`/`ECLJump` 落在同一 sub），
               则取该 sub 的 0x74（`E+0xEB4`，超时切换目标）末值。
"""

import csv
import os
import re
import struct
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
ECL_DIR = os.path.join(ROOT, '..', 'exes', 'ecl', 'th06')
DECL_DIR = os.path.join(ROOT, '..', 'exes', 'ecl_dump', 'th06')
SRC = os.path.join(ROOT, 'thprac', 'src', 'thprac', 'thprac_th06.cpp')
CSV = os.path.join(ROOT, 'docs', 'logger', 'th06_checkpoints.csv')
E1 = os.path.join(ROOT, 'docs', 'th06', 'th06_ecl_re_report.md')

MCS = {1: (4, 2), 2: (2, 2), 3: (4, 3), 4: (4, 5), 5: (3, 2), 6: (2, 0), 7: (4, 3)}
DROP = {'ST4 前半 #4'}          # 2026-10-26 用户裁决：与 ST4 BOOKS 同落点 0xCDD4，删除章节行

# ── ECL ──────────────────────────────────────────────────────────────────────
class Ecl:
    def __init__(self, path):
        b = open(path, 'rb').read()
        self.buf = b
        self.size = len(b)
        self.sub_count = struct.unpack_from('<h', b, 0)[0]
        self.tl_off = struct.unpack_from('<I', b, 4)[0]
        self.subs = [struct.unpack_from('<I', b, 0x10 + 4 * i)[0] for i in range(self.sub_count)]
        tl, off = [], self.tl_off
        while off + 8 <= len(b):
            t, a0, op, sz = struct.unpack_from('<hhhh', b, off)
            if t < 0:
                break
            tl.append(dict(off=off, time=t, arg0=a0, op=op, size=sz, end=off + sz))
            if sz <= 0:
                break
            off += sz
        self.tl = tl
        starts = sorted((s, i) for i, s in enumerate(self.subs))
        self.ranges = [(s, starts[k + 1][0] if k + 1 < len(starts) else self.tl_off, i)
                       for k, (s, i) in enumerate(starts)]
        self.sub_ins = {}
        for i, s in enumerate(self.subs):
            ins, off = [], s
            while off + 12 <= len(b):
                t, op, sz = struct.unpack_from('<ihh', b, off)
                if t < 0 or sz <= 0:
                    break
                ins.append(dict(off=off, time=t, op=op, size=sz, args=b[off + 12: off + sz]))
                off += sz
            self.sub_ins[i] = ins

    def landing(self, warp):
        for x in self.tl:
            if x['time'] >= warp:
                return x
        return None

    def sub_at(self, off):
        for s, e, i in self.ranges:
            if s <= off < e:
                return i
        return None

    def first_spawn(self, off):
        on = False
        for x in self.tl:
            if x['off'] == off:
                on = True
            if on and x['op'] <= 7:
                return x['arg0']
        return None

    def tl_op10(self):
        return [x for x in self.tl if x['op'] == 10]


def i32(a, o=0):
    return struct.unpack_from('<i', a, o)[0] if len(a) >= o + 4 else None


def sub_facts(e, i):
    """相位 sub 的静态事实：death / irq / hp / bossPos / timeout(0x73) / next(0x74) 序列。"""
    f = dict(death=None, deaths=[], irq={}, hp=[], boss_pos=False, tmo=[], nxt=[], ops=[])
    for x in e.sub_ins[i]:
        a = x['args']
        if x['op'] == 0x6C:
            mask = struct.unpack_from('<B', e.buf, x['off'] + 9)[0]
            f['deaths'].append((x['off'], i32(a), mask))
            if f['death'] is None or mask == 0xFF:
                f['death'] = i32(a)
            f['ops'].append(('death', x['off'], x['time'], i32(a)))
        elif x['op'] == 0x6D:
            f['irq'][i32(a, 4)] = i32(a)
            f['ops'].append(('irq', x['off'], x['time'], (i32(a), i32(a, 4))))
        elif x['op'] == 0x6F:
            f['hp'].append(i32(a))
        elif x['op'] == 0x41:          # ins_65（十进制 65 = 0x41）: boss 位置矩形
            f['boss_pos'] = True
        elif x['op'] == 0x73:
            f['tmo'].append((x['time'], i32(a)))
            f['ops'].append(('tmo', x['off'], x['time'], i32(a)))
        elif x['op'] == 0x74:
            f['nxt'].append((x['time'], i32(a)))
            f['ops'].append(('next', x['off'], x['time'], i32(a)))
    return f


def handoff(e, i, upto=None):
    """超时切换目标 = 该 sub 内 **force 点之前**最后一个 0x74 的入参（E+0xEB4）。

    `upto=None` ⇒ 取整条 sub 的末值。`E+0xEB4` 由 0x74 逐条写入（`0x409A4F`），
    超时分支在 `0x411FF2` 读它并 `SetSub`，故「生效值」= 强制交接那一刻已执行过的最后一条 0x74。
    """
    f = sub_facts(e, i)
    cand = [n for (t, n), op in zip(f['nxt'], [o for o in f['ops'] if o[0] == 'next'])
            if upto is None or op[1] < upto]
    return cand[-1] if cand else None


# ── 源码 ─────────────────────────────────────────────────────────────────────
NUM = r'(0x[0-9a-fA-F]+|\d+)'
LAMBDAS = ('s2b_nd', 's3b_n1', 's4b_time', 's7b_call', 's7b_n1')


def num(s):
    return int(s, 0)


def read_src():
    return open(SRC, encoding='utf-8').read().split('\n')


def parse_lambdas(src):
    out = {}
    for i, line in enumerate(src, 1):
        m = re.match(r'\s*auto (\w+) = \[&\]\(\) \{', line)
        if not m:
            continue
        depth, j, body = line.count('{') - line.count('}'), i, []
        while j < len(src) and depth > 0:
            body.append((j + 1, src[j]))
            depth += src[j].count('{') - src[j].count('}')
            j += 1
        out[m.group(1)] = body
    return out


def extract_ops(lines):
    ops = []
    for ln, t in lines:
        for m in re.finditer(r'ECLWarp\(\s*' + NUM + r'\s*\)', t):
            ops.append(dict(kind='warp', val=num(m.group(1)), line=ln))
        for m in re.finditer(r'ECLSetTime\(ecl,\s*' + NUM, t):
            ops.append(dict(kind='settime', val=num(m.group(1)), line=ln))
        for m in re.finditer(r'ECLSetHealth\(ecl,\s*' + NUM, t):
            ops.append(dict(kind='sethealth', val=num(m.group(1)), line=ln))
        for m in re.finditer(r'ECLStall\(ecl,\s*' + NUM + r'\s*\)', t):
            ops.append(dict(kind='stall', val=num(m.group(1)), line=ln))
        for m in re.finditer(r'pair\s*\{\s*' + NUM + r'\s*,', t):
            ops.append(dict(kind='pair', val=num(m.group(1)), line=ln))
        for m in re.finditer(r'ECLJump\(ecl,\s*[^,]+,\s*' + NUM, t):
            ops.append(dict(kind='jump', val=num(m.group(1)), line=ln))
        for m in re.finditer(r'\b(' + '|'.join(LAMBDAS) + r')\(\)', t):
            ops.append(dict(kind='lambda', val=m.group(1), line=ln))
    return ops


def parse_patch(src):
    lams = parse_lambdas(src)
    start = end = None
    for i, l in enumerate(src, 1):
        if 'void THPatch(ECLHelper& ecl' in l:
            start = i
        if start and 'void THStageWarp(' in l:
            end = i
            break
    cases, cur, depth = [], None, 0
    for ln in range(start, end):
        t = src[ln - 1]
        m = re.match(r'\s*case THPrac::TH06::(TH06_\w+):', t)
        if m:
            cur = dict(name=m.group(1), start=ln, end=None, raw=[])
            cases.append(cur)
            depth = 0
            continue
        if cur is None:
            continue
        cur['raw'].append((ln, t))
        if re.match(r'\s*break;', t) and depth == 0 and cur['end'] is None:
            cur['end'] = ln
        depth += t.count('{') - t.count('}')
    for c in cases:
        if c['end'] is None:
            c['end'] = c['raw'][-1][0]
        exp, lams_used, calls, orig = [], set(), {}, {}
        for ln, t in c['raw']:
            ms = list(re.finditer(r'\b(' + '|'.join(LAMBDAS) + r')\(\)', t))
            if ms:
                for m in ms:
                    lams_used.add(m.group(1))
                    calls.setdefault(m.group(1), ln)
                    exp.extend(lams[m.group(1)])
                    for bl, _ in lams[m.group(1)]:
                        orig[bl] = (m.group(1), ln)
            else:
                exp.append((ln, t))
        c['calls'] = calls
        c['orig'] = orig
        c['xops'] = extract_ops(exp)
        c['lambdas'] = sorted(lams_used)
        warps = [o for o in c['xops'] if o['kind'] == 'warp']
        # 口径：取**非 dlg**（`if (thPracParam.dlg)` 分支在前，故取最后一条 ECLWarp）
        c['warp'] = warps[-1] if warps else None
        c['warp_dlg'] = warps[0] if len(warps) > 1 and warps[0]['val'] != warps[-1]['val'] else None
        c['pair_offs'] = sorted({o['val'] for o in c['xops'] if o['kind'] == 'pair'})
        c['force_offs'] = sorted({o['val'] for o in c['xops']
                                  if o['kind'] in ('settime', 'stall', 'jump')})
        c['all_offs'] = sorted(set(c['pair_offs']) | set(c['force_offs']))
    return cases


CBA_SRC = os.path.join(ROOT, 'thprac', 'src', 'thprac', 'thprac_locale_def.cpp')


def parse_cba():
    """th_sections_cba[11][2][19] 组 0..6 的关底列表（= CSV 的 boss 行顺序）。"""
    lines = open(CBA_SRC, encoding='utf-8').read().split('\n')
    start = next(i for i, l in enumerate(lines) if 'th_sections_cba[11][2][19]' in l)
    order, depth, group, which = [], 1, -1, -1   # depth 1 = 数组自身的 '{'
    for l in lines[start:start + 400]:
        if l.strip().startswith('{'):
            depth += 1
            if depth == 2:
                group += 1
                which = -1
            elif depth == 3:
                which += 1
        elif l.strip().startswith('}'):
            depth -= 1
        m = re.match(r'\s*(TH06_\w+),', l)
        if m and depth == 3 and group <= 6 and which in (0, 1):
            order.append(m.group(1))
    return order


def parse_stagewarp(src):
    start = end = None
    for i, l in enumerate(src, 1):
        if 'void THStageWarp(' in l:
            start = i
        if start and 'void THSectionPatch(' in l:
            end = i
            break
    out, stage, portion = [], None, None
    for ln in range(start, end):
        t = src[ln - 1]
        m = re.search(r'stage == (\d+)', t)
        if m:
            stage = int(m.group(1))
        m = re.match(r'\s*case (\d+):', t)
        if m:
            portion = int(m.group(1))
        m = re.search(r'ECLWarp\(\s*' + NUM + r'\s*\)', t)
        if m and stage and portion:
            out.append(dict(stage=stage, portion=portion, warp=num(m.group(1)), line=ln))
    return out


# ── 入口表 ───────────────────────────────────────────────────────────────────
def chapter_name(stage, portion):
    a, b = MCS[stage]
    if b == 0:
        return f'ST{stage} #{portion}'
    return f'ST{stage} 前半 #{portion}' if portion <= a else f'ST{stage} 后半 #{portion - a}'


def section_name(enum):
    s = enum[len('TH06_'):]
    return s.replace('_', ' ', 1)


def stage_of(enum):
    return int(enum.split('_')[1][2:])


def build_rows():
    src = read_src()
    rows = []
    # 章节
    for x in parse_stagewarp(src):
        nm = chapter_name(x['stage'], x['portion'])
        if nm in DROP:
            continue
        rows.append(dict(name=nm, stage=x['stage'], kind='chapter', warp=x['warp'],
                         src=f":{x['line']}", case=None))
    # boss（顺序 = th_sections_cba 组 0..6 的关底列表 = CSV 行序）
    cases = {c['name']: c for c in parse_patch(src)}
    for enum in parse_cba():
        c = cases[enum]
        if c['start'] <= c['warp']['line'] <= c['end']:
            src = f":{c['warp']['line']}"
        else:
            lam, call_line = c['orig'].get(c['warp']['line'], ('?', c['start']))
            src = f":{call_line}+λ{lam}:{c['warp']['line']}"
        rows.append(dict(name=section_name(enum), stage=stage_of(enum), kind='boss',
                         warp=c['warp']['val'], src=src, case=c))
    # 补落点 / spawn / 副键
    cache = {}
    for r in rows:
        if r['stage'] not in cache:
            cache[r['stage']] = Ecl(os.path.join(ECL_DIR, f'ecldata{r["stage"]}.ecl'))
        e = cache[r['stage']]
        land = e.landing(r['warp'])
        r['land'] = land
        r['dlg'] = None
        if r['kind'] == 'boss' and r['case'].get('warp_dlg'):
            dv = r['case']['warp_dlg']
            r['dlg'] = dict(warp=dv['val'], land=e.landing(dv['val']), line=dv['line'])
        r['spawn'] = e.first_spawn(land['off'])
        if r['kind'] == 'chapter':
            r['key'] = r['spawn']
            r['key_basis'] = 'spawn'
            r['conf'] = '🟡B'
            continue
        c = r['case']
        # 补丁落点 → sub
        by_sub = defaultdict(list)
        for off in c['pair_offs']:
            s = e.sub_at(off)
            if s is not None:
                by_sub[s].append(off)
        fsub = defaultdict(list)
        fkind = defaultdict(set)
        for o in c['xops']:
            if o['kind'] not in ('settime', 'stall', 'jump'):
                continue
            s = e.sub_at(o['val'])
            if s is not None:
                fsub[s].append(o['val'])
                fkind[s].add(o['kind'])
        r['patch_subs'] = dict(by_sub)
        r['force_subs'] = dict(fsub)
        if by_sub:
            deep = max(by_sub)
            # 只有 ECLSetTime（把 0x73 超时改成 0）才真正触发 0x74 交接；
            # ECLStall 是冻结、ECLJump 是改控制流，都不切 sub。
            if 'settime' in fkind.get(deep, ()):
                ho = handoff(e, deep, min(fsub[deep]))
                r['key'] = ho if ho is not None else deep
                r['key_basis'] = f'force@{deep}->next'
                r['conf'] = '🟡A'
            else:
                r['key'] = deep
                r['key_basis'] = f'deepest={deep}'
                r['conf'] = '🟡A'
        else:
            ctrl = r['spawn']
            irq = sub_facts(e, ctrl)['irq'] if ctrl is not None else {}
            r['key'] = irq.get(0, ctrl)
            # 有 0x6D ⇒ 控制器（副键 = irq[0] 目标）；无 0x6D ⇒ 该 sub 自身就是相位脚本
            r['key_basis'] = f'irq[{ctrl}]' if irq else f'spawn[{ctrl}]'
            r['conf'] = '🟡B'
    return rows, cache


# ── 与 E1 附录 C/D 比对 ──────────────────────────────────────────────────────
def parse_e1_tables():
    txt = open(E1, encoding='utf-8').read().split('\n')
    c_rows, d_rows = [], []
    in_c = in_d = False
    for l in txt:
        if l.startswith('## 13.'):
            in_c = True
            continue
        if l.startswith('## 14.'):
            in_c, in_d = False, True
            continue
        if l.startswith('## 15.'):
            in_d = False
        if not l.startswith('|'):
            continue
        cells = [c.strip() for c in l.strip('|').split('|')]
        if in_c and len(cells) == 7 and cells[0].startswith('st'):
            c_rows.append(dict(stage=int(cells[0][2:]), portion=int(cells[1]),
                               warp=cells[2], off=cells[3], t=cells[4], op=cells[5],
                               sub=cells[6]))
        if in_d and len(cells) == 7 and cells[0].startswith('`TH06_'):
            d_rows.append(dict(name=cells[0].strip('`'), warp=cells[1], off=cells[2],
                               t=cells[3], op=cells[4], sub=cells[5],
                               psub=cells[6]))
    return c_rows, d_rows


# ── 主流程 ───────────────────────────────────────────────────────────────────
def main():
    graph = '--graph' in sys.argv
    table = '--table' in sys.argv
    rows, cache = build_rows()
    rows.sort(key=lambda r: (r['stage'], 0 if r['kind'] == 'chapter' else 1))
    chap = [r for r in rows if r['kind'] == 'chapter']
    boss = [r for r in rows if r['kind'] == 'boss']
    print('== 行集合 ==')
    print(f'章节行 {len(chap)} / boss 行 {len(boss)} / 合计 {len(rows)}   (期望 39 / 70 / 109)')
    per = Counter(r['stage'] for r in chap)
    perb = Counter(r['stage'] for r in boss)
    print('逐关（章节/boss）:', {f'ST{k}': (per[k], perb[k]) for k in range(1, 8)})
    print(f"章节各关: {[per[k] for k in range(1,8)]}  (期望 [6,4,7,8,5,2,7])")
    print(f"boss 各关: {[perb[k] for k in range(1,8)]}  (期望 [6,6,9,9,8,11,21])")

    print('\n== dlg 变体（`if (thPracParam.dlg)` 分支；本表取非 dlg）==')
    for r in rows:
        if r.get('dlg'):
            d = r['dlg']
            print(f"  {r['name']:14s} dlg={d['warp']}/0x{d['land']['off']:X}  "
                  f"非dlg={r['warp']}/0x{r['land']['off']:X}  (源码 :{d['line']} / :{r['src'].lstrip(':')})")

    # 落点分组
    grp = defaultdict(list)
    for r in rows:
        grp[(r['stage'], r['land']['off'])].append(r)
    multi = {k: v for k, v in grp.items() if len(v) > 1}
    sep, unsep = [], []
    for k, v in multi.items():
        keys = {r['key'] for r in v}
        (sep if len(keys) == len(v) else unsep).append((k, v, keys))
    print('\n== 同落点分组 ==')
    print(f'不同落点 {len(grp)} 个；多行组 {len(multi)} 组，覆盖 {sum(len(v) for v in multi.values())} 行')
    print(f'  其中「组内 SubN 两两不同」= {len(sep)} 组；「有碰撞」= {len(unsep)} 组')
    for k, v, keys in sorted(unsep):
        c = Counter(r['key'] for r in v)
        dup = {s: [r['name'] for r in v if r['key'] == s] for s, n in c.items() if n > 1}
        print(f'  碰撞 0x{k[1]:04X} (ST{k[0]}): {dup}')
    print('组大小分布:', dict(sorted(Counter(len(v) for v in multi.values()).items())))
    print('\n逐组明细（落点 → 成员/副键）:')
    for k, v in sorted(grp.items()):
        if len(v) < 2:
            continue
        tag = '可分' if len({r['key'] for r in v}) == len(v) else '碰撞'
        mem = ', '.join(f"{r['name']}(Sub{r['key']})" for r in v)
        print(f"  0x{k[1]:04X} ST{k[0]} {len(v):2d}行 [{tag}] {mem}")
    print('单行落点:', sum(1 for v in grp.values() if len(v) == 1), '个')

    # CSV 比对
    print('\n== CSV 逐行比对（只读）==')
    data = list(csv.reader(open(CSV, encoding='utf-8')))[1:]
    print(f'CSV 数据行 {len(data)}  (期望 109)')
    bad = []
    for i, (r, d) in enumerate(zip(rows, data), 2):
        want = f"{r['warp']}/0x{r['land']['off']:X}"
        if d[3] != r['name'] or d[0] != want or d[1] != 'th06' or d[2] != f"ST{r['stage']}":
            bad.append((i, r['name'], want, d))
    print(f'逐行一致: {not bad and len(rows) == len(data)}'
          + ('' if not bad else f'\n  差异 {len(bad)} 处: {bad[:12]}'))
    fmt_bad = [d for d in data if not re.fullmatch(r'\d+/0x[0-9A-F]+', d[0])]
    print(f'格式 ^\\d+/0x[0-9A-F]+$ 违例: {len(fmt_bad)}')

    # E1 附录 C/D 比对
    print('\n== 与 E1 附录 C/D 比对 ==')
    c_rows, d_rows = parse_e1_tables()
    print(f'附录 C 行 {len(c_rows)} / 附录 D 行 {len(d_rows)}')
    cmap = {(r['stage'], r['portion']): r for r in c_rows}
    diffs = []
    chap_by = {}
    for x in parse_stagewarp(read_src()):
        chap_by[chapter_name(x['stage'], x['portion'])] = x
    for r in chap:
        x = chap_by[r['name']]
        ref = cmap.get((x['stage'], x['portion']))
        if not ref:
            diffs.append((r['name'], '附录 C 缺行', '', ''))
            continue
        if ref['off'].strip('`') != f"0x{r['land']['off']:X}":
            diffs.append((r['name'], '落点', ref['off'], f"0x{r['land']['off']:X}"))
        if ref['t'] != str(r['land']['time']):
            diffs.append((r['name'], '落点 time', ref['t'], str(r['land']['time'])))
        if ref['sub'].strip('`') != f"Sub{r['spawn']}":
            diffs.append((r['name'], '首个 spawn sub', ref['sub'], f"Sub{r['spawn']}"))
        if num(ref['warp'].split('(')[0]) != r['warp']:
            diffs.append((r['name'], 'warp', ref['warp'], str(r['warp'])))
    dmap = {x['name']: x for x in d_rows}
    for r in boss:
        enum = 'TH06_' + r['name'].replace(' ', '_', 1)
        ref = dmap.get(enum)
        if not ref:
            diffs.append((r['name'], '附录 D 缺行', '', ''))
            continue
        if ref['off'].strip('`') != f"0x{r['land']['off']:X}":
            diffs.append((r['name'], '落点', ref['off'], f"0x{r['land']['off']:X}"))
        if ref['t'] != str(r['land']['time']):
            diffs.append((r['name'], '落点 time', ref['t'], str(r['land']['time'])))
        if ref['op'] != str(r['land']['op']):
            diffs.append((r['name'], '落点 op', ref['op'], str(r['land']['op'])))
        if ref['sub'].strip('`') != f"Sub{r['spawn']}":
            diffs.append((r['name'], '首个 spawn sub', ref['sub'], f"Sub{r['spawn']}"))
        # 附录 D 第 6 列：补丁偏移落在的 sub（只计 pair{}）
        mine = sorted(r['patch_subs'])
        ref_subs = sorted(int(s[3:]) for s in re.findall(r'Sub\d+', ref['psub']))
        if mine != ref_subs:
            diffs.append((r['name'], '补丁归属 sub', ref['psub'], str([f'Sub{i}' for i in mine])))
    print(f'差异 {len(diffs)} 处')
    for d in diffs:
        print('  ', d)

    if graph:
        print('\n== 相位 sub 链（每关）==')
        for st in range(1, 8):
            e = cache[st]
            print(f'-- ST{st}')
            for x in e.tl_op10():
                print(f'   timeline op0xA @0x{x["off"]:04X} t={x["time"]}')
            for i in range(e.sub_count):
                f = sub_facts(e, i)
                if f['irq'] or f['boss_pos']:
                    print(f'   Sub{i:2d} @0x{e.subs[i]:04X} irq={f["irq"]} death={f["death"]} '
                          f'hp={f["hp"]} next={[n for _, n in f["nxt"]]} '
                          f'tmo={[n for _, n in f["tmo"]]}')
            # 相位链
            seen = set()
            for r in [r for r in rows if r['stage'] == st and r['kind'] == 'boss']:
                ctrl = r['spawn']
                if ctrl in seen:
                    continue
                seen.add(ctrl)
                irq = sub_facts(e, ctrl)['irq']
                cur, inherit = irq.get(0, ctrl), sub_facts(e, ctrl)['death']
                chain = []
                while cur is not None and cur not in [c[0] for c in chain] and len(chain) < 24:
                    f = sub_facts(e, cur)
                    chain.append((cur, f))
                    inherit = f['death'] if f['death'] is not None else inherit
                    cur = inherit
                print(f'   相位链(控制器 Sub{ctrl} irq={irq}): ' +
                      ' -> '.join(f'Sub{i}(hp={f["hp"] or "-"},next={[n for _, n in f["nxt"]] or "-"})'
                                  for i, f in chain))

    ok = (len(chap) == 39 and len(boss) == 70 and not fmt_bad
          and len(data) == len(rows) and not bad)
    if table:
        print('\n== 109 行入口表 ==')
        print('| 记录名 | 时间值 | 落点 | 来源 | 副键 | 依据 | 置信度 |')
        print('| --- | --- | --- | --- | --- | --- | --- |')
        for r in rows:
            print(f"| {r['name']} | **{r['warp']}** | `0x{r['land']['off']:X}` | `{r['src']}` | "
                  f"`Sub{r['key']}` | {r['key_basis']} | {r['conf']} |")
    print('\n== 结论 ==')
    print('PASS' if ok else 'FAIL', '：109 行 = 章节 39 + boss 70；格式/CSV 逐行一致' if ok
          else '：见上面的差异清单')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())

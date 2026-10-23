#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""fill_th16_boss_rows.py -- 回填 docs/logger/th16_checkpoints.csv 的 boss 行「段落入口函数」

用途
----
TH16 checkpoint 白名单（`docs/logger/th16_checkpoints.csv`）的 boss 轴行
（`MID*` / `BOSS*` / `END_*` / `*_FINAL`）第 1 列 `段落入口函数` 留空。本脚本按
`docs/th16/th16_boss_phase_report.md` §3（方案二：读 ECL 引擎「当前 sub 名」+ 白名单
latch）把它回填成对应的 **boss 脚本 sub 名**。

判定口径（latch / 路过式 checkpoint，见 `checkpoint_entry_backfill_workflow.md` §1）
------------------------------------------------------------------------------
运行期读「当前 ECL sub 名」：∈ 白名单 → 置当前段落为该记录名；不在 → no-op。
故每行只填 **1 个**「进入此段即代表此段开始」的 sub。boss 段的 sub 住在
`stNNbs.ecl`（关底Boss）/ `stNNmbs.ecl`、`stNNmbs2.ecl`（道中Boss）里。

证据（脚本全部用上）
------------------
1. **`.decl`（thecl -d 反编译产物）**：sub 名与结构 —— 每关 boss 脚本的 sub 表、
   `setNext(0, …)` / `@call` 组成的「阶段链」，以及 `<Name>_at` 同伴
   （= 该非符是否真有弹幕阶段：`Boss3`/`Boss6`/`Boss9` 这类「过渡非符」没有 `_at`，
   是进入下一符卡前的过场，不入表）。
2. **`thprac_th16.cpp` 的 `THPatch`**：thprac 自己「跳到某段」的补丁。它在 boss 脚本
   （`SetFile(N)`，N 按该关 `.decl` 头部 `ecli{}` 取）上打的偏移（`ECLJump` 落点 +
   `pair{}`）**基本都落在目标阶段的 sub 内**（`Disable Item Drops` 那条最稳）。
   例：ST1 BOSS2 打 `st01bs` 的 `0x1ba8` → `BossCard1`；ST1 BOSS3 打 `0x1044/0x1174`
   → `Boss2`（与报告 §3.4 已解的 ST1 答案逐行吻合）。本脚本用它做**交叉验证**。
3. **`.ecl`（原始二进制）**：sub 名 → 文件内字节区间，用于把上面的偏移解析成 sub。

取值（每关）
----------
- **关底Boss 轴**：`.decl` 阶段链（去入口 / 去过场非符）长度若与 CSV 该关 BOSS 行数
  一致 → 逐行取用；否则退回「THPatch 补丁落点」逐行判（ST5 走这条）。
- **道中Boss 轴**：`<入口>`（道中非符，如 `MBoss`）+ 其阶段链里的符卡（`*Card*`）
  若行数一致 → 逐行取用；否则退回「去掉入口后的阶段链」；再否则退回补丁落点。
- 阶段链：起点 = 阶段 sub 里「不是任何后继」的那个（只认「末尾可有 B」的规范名，
  排除 `Boss6f`/`MBoss2p` 这类「阶段体/动画」辅助）；后继 = `setNext(0, …)`，
  没有则取该 sub 的全部阶段 `@call`（`Boss6` 按 `$SUBSEASON` 分派的
  `@BossCard6/7/8/9()` 因此全部计入）。
- **过渡非符一律剔除**（`is_transition()`）：名字规范、非符卡、且**没有 `<Name>_at`
  同伴**（如 ST2 `Boss3`、ST6 `Boss6`、ST7 `Boss9`/`Boss10`、ST5 `Boss4`；这些是
  进符卡前的空过场，血量 0、无 `setNext`、直接 `@BossCardN()`）。**补丁落点这一路
  同样要过这道滤网** —— thprac 给 ST5 BOSS7 打的 `Disable Item Drops` 落在 `Boss4`/
  `Boss4B`（过渡非符）里，真入口是 `BossCard4`。
- 补丁落点被滤空时，**按 `Set Spell Ordinal` / `Change Nonspell` 的字节值定位**：
  ordinal N → `BossCard<N>`，nonspell N → `Boss<N>`；符卡路径再用 `Set Health` 与
  `Boss()` 派发里的 `lifeSet(H) → @BossCardN()` 交叉核对（核不上就不用）。
- 道中Boss 的交叉验证：若 THPatch 的跳转落点落在入口 sub（`MBoss`）里，则按落点后
  第一条 `CALL <阶段>()` 补一个候选 —— ST7 MID1/2/3 的落点 0x50c/0x5a8/0x644 正是
  `CALL MBossCard1/2/3` 前一条指令，与该关阶段链互相印证。
- 都定不了的行 → **留空**（不猜）。行数以 CSV 实际内容为准（不硬编码总数）。

已知例外（有据可依，不是猜）
--------------------------
- **ST7 END_S9 = `BossCard9`**：THPatch 的 `Disable Item Drops` 落点在 `Boss9`（**过渡
  非符**），但同 case 的 `Set Spell Ordinal` = 0x39（第 9 张符卡）、`Set Health` = 3000，
  与 `st07bs` 的 `Boss()` 派发 `$SPELL_ID == 117 → lifeSet(3000); @BossCard9()` 对上
  （S1..S8 同理：ordinal N ↔ `BossCardN`）→ 该段真入口是 `BossCard9`。
- **ST5 BOSS7 = `BossCard4`**：同理 —— `Disable Item Drops` 落在 `Boss4`/`Boss4B`
  （过渡非符，无 `Boss4_at`、`Boss4p` 的 `lifeMarker` 血量 0），而 `Set Spell Ordinal`
  = 0x34（第 4 张符卡）、`Set Health` = 3000，对上 `st05bs` 的 `Boss()` 派发
  `$SPELL_ID 66..69 → lifeSet(3000); @BossCard4()`。
- **ST4 MID2**（`道中结尾`）：THPatch 只改 `st04.ecl` 的 `MainLatter`（章节脚本，
  非 boss 脚本）→ 无专属 boss sub。（用户已确认游戏内无此段并从 CSV 删除该行。）
- **ST5 BOSS1**：THPatch 只改 `st05bs` 的入口 `Boss`（站位/移动），无阶段落点；
  ST5 的 A/B 双路线使阶段链起点不唯一 → 按「关底第一个非符」兜底为 `Boss1`
  （B 路线对应 `Boss1B`，见报告 §8 风险 8）。

幂等
----
只写第 1 列，且只写「当前为空」的 boss 行；第 2/3/4 列原样不动；重复运行结果一致。

用法
----
    python docs/logger/tools/fill_th16_boss_rows.py --check   # 只看结果，不写盘
    python docs/logger/tools/fill_th16_boss_rows.py           # 原地回填 CSV
"""

import argparse
import os
import re
import struct
import sys

# --------------------------------------------------------------------------
# 常量
# --------------------------------------------------------------------------

CALL = 0x0B          # 同步 call
SPAWN = 0x0F         # async call (spawn)
SUB_HDR = 16         # sub 代码头："ECLH" + 0x10

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
DEFAULT_DECL = r'D:\workshop\reasm\exes\ecl_dump\th16'
DEFAULT_ECL = r'D:\workshop\reasm\exes\ecl\th16'
CSV_PATH = os.path.join(REPO, 'docs', 'logger', 'th16_checkpoints.csv')
CPP_PATH = os.path.join(REPO, 'thprac', 'src', 'thprac', 'thprac_th16.cpp')

# 每关 ecli{}（取自 stNN.decl 头部）；SetFile(N) 取第 N 项（1-based）。
ECLI = {
    1: ['default', 'st01bs', 'st01mbs'],
    2: ['default', 'st02bs', 'st02mbs'],
    3: ['default', 'st03bs', 'st03mbs'],
    4: ['default', 'st04bs', 'st04mbs'],
    5: ['default', 'st05mbs', 'st05bs'],
    6: ['default', 'st06bs'],
    7: ['default', 'st07mbs', 'st07mbs2', 'st07bs'],
}
# 每关「道中Boss」脚本（MID 行的目标文件）；ST6 无道中Boss。
MBS = {1: 'st01mbs', 2: 'st02mbs', 3: 'st03mbs', 4: 'st04mbs',
       5: 'st05mbs', 6: None, 7: 'st07mbs'}

# 阶段 sub 名（非符 / 符卡 / 道中符）；入口 Boss/MBoss/BossB/MBossB 无数字，不匹配。
# 宽松版（链上用）：允许 `Boss2p` / `Boss6f` 这类「阶段体/动画」辅助 sub 作为中转节点。
PHASE_RE = re.compile(r'^(?:M{1,2}Boss|Boss)B?(?:Card)?\d+[A-Za-z]?$')
# 严格版（定链头用）：只允许末尾 B（A/B 分支），排除 p / f 等辅助 sub。
CANON_RE = re.compile(r'^(?:M{1,2}Boss|Boss)B?(?:Card)?\d+B?$')
ENTRY_RE = re.compile(r'^(?:M{1,2}Boss|Boss)B?$')
# 入口 sub（段落的容器，不入表）
ENTRIES = {'Boss', 'BossB', 'MBoss', 'MBossB', 'BossSp', 'BossSpell',
           'BossSpell2', 'MainBoss'}
# 辅助 sub：以 p / Hide 结尾（`Boss2p`/`BossCard3Bp`/`BossCard1Hide`…）
HELPER_TAIL = re.compile(r'(?:p|Hide)$')
DEAD_RE = re.compile(r'Dead|Escape')


# --------------------------------------------------------------------------
# .ecl 容器解析（只读；口径同 tools/ecl_scpt.py）
# --------------------------------------------------------------------------

def parse_ecl(path):
    """→ (data, [(start, end, name), …])；区间为文件内字节偏移。"""
    with open(path, 'rb') as fh:
        data = fh.read()
    if data[:4] != b'SCPT':
        raise ValueError('%s: 不是 SCPT 容器' % path)
    hlen = struct.unpack_from('<H', data, 6)[0]
    count = struct.unpack_from('<I', data, 0x10)[0]
    off0 = 0x24 + hlen
    offs = list(struct.unpack_from('<%dI' % count, data, off0))
    p = off0 + count * 4
    names = []
    for _ in range(count):
        e = data.index(b'\x00', p)
        names.append(data[p:e].decode('latin1'))
        p = e + 1
    items = sorted(zip(offs, names))
    out = []
    for i, (o, n) in enumerate(items):
        end = items[i + 1][0] if i + 1 < len(items) else len(data)
        out.append((o, end, n))
    return data, out


def sub_at(ranges, off):
    for s, e, n in ranges:
        if s <= off < e:
            return n
    return None


def first_call_after(data, ranges, sub, off):
    """sub 内 off 处（含）之后第一条 call/spawn 的被调者名；无则 None。

    用于「thprac 把跳转插到入口 sub 里、落点恰在 `CALL <阶段>()` 前一条指令」
    的情形（如 ST7 MID1/2/3：落点 0x50c/0x5a8/0x644 紧邻 `CALL MBossCard1/2/3`）。
    """
    span = next((r for r in ranges if r[2] == sub), None)
    if span is None:
        return None
    s, e, _ = span
    o = s + SUB_HDR
    while o < e:
        _t, idv, size, _u, _r = struct.unpack_from('<IHHHH', data, o)
        if size < 12:
            return None
        if o >= off and idv in (CALL, SPAWN):
            z = data.index(b'\x00', o + 0x14)
            return data[o + 0x14:z].decode('latin1')
        o += size
    return None


# --------------------------------------------------------------------------
# .decl 解析
# --------------------------------------------------------------------------

def parse_decl(path):
    """→ (sub 名列表（文件顺序）, {sub: {'next0': [...], 'calls': [...], 'body': [行]}})"""
    text = open(path, encoding='latin1').read().splitlines()
    names, info, cur = [], {}, None
    for ln in text:
        m = re.match(r'^void\s+(\w+)\s*\(', ln)
        if m:
            cur = m.group(1)
            names.append(cur)
            info[cur] = {'next0': [], 'calls': [], 'body': []}
            continue
        if cur is None:
            continue
        info[cur]['body'].append(ln)
        for mm in re.finditer(r'setNext\(\s*0\s*,[^,]*,[^,]*,\s*"([^"]*)"', ln):
            info[cur]['next0'].append(mm.group(1))
        for mm in re.finditer(r'@?([A-Za-z_]\w*)\(\)\s*(?:async)?\s*;', ln):
            info[cur]['calls'].append(mm.group(1))
    return names, info


def phase_chain(names, info):
    """从 .decl 推阶段链（含入口与过渡非符）；起点不唯一则 None。

    后继 = setNext(0, …, "Q")；无 setNext(0) 的 sub 取其全部阶段 call
    （如 `Boss9` 末尾 `@BossCard9()`；`Boss6` 末尾按 `$SUBSEASON` 分派的
    `@BossCard6/7/8/9()` 全部计入）。终点 BossDead*/BossEscape* 不入链。
    """
    phases = [n for n in names if PHASE_RE.match(n)]
    if not phases:
        return None
    succ, targets = {}, set()
    for n in phases:
        nxt = [q for q in info[n]['next0']
               if PHASE_RE.match(q) and not DEAD_RE.search(q)]
        if not nxt:
            nxt = [c for c in info[n]['calls']
                   if PHASE_RE.match(c) and not DEAD_RE.search(c)]
        uniq = []
        for q in nxt:
            if q not in uniq:
                uniq.append(q)
        succ[n] = uniq
        targets.update(uniq)
    starts = [n for n in phases if n not in targets and CANON_RE.match(n)]
    if len(starts) != 1:
        return None
    chain, visited = [], set()

    def walk(n):
        if n in visited:
            return
        visited.add(n)
        chain.append(n)
        for m in succ.get(n, []):
            walk(m)

    walk(starts[0])
    return chain


def is_transition(sub, subs, info=None):
    """过渡非符：规范阶段名、非符卡、且本身没有弹幕阶段。

    判据一（主）：没有 `<Name>_at` 同伴 —— 真非符 `Boss1/2/3` 都有 `BossN_at`，
    而 `Boss4`（ST5）/`Boss3`（ST2）/`Boss6`（ST6）/`Boss9`、`Boss10`（ST7）都没有。
    判据二（补，仅当上一判据不成立时）：体内**既无 `setNext(0, …)`，又有显式
    `lifeMarker(0, 0.0f, …)`（血量为 0 的空过场）** —— 如 ST5 `Boss4p`。
    """
    if not PHASE_RE.match(sub) or 'Card' in sub:
        return False
    if (sub + '_at') not in subs:
        return True
    body = '\n'.join(info[sub]['body']) if info else ''
    return 'setNext(0' not in body and re.search(r'lifeMarker\(\s*0\s*,\s*0\.0f', body)


def usable(sub):
    return (sub not in ENTRIES and '_' not in sub
            and not HELPER_TAIL.search(sub) and not DEAD_RE.search(sub))


def entry_dispatch(info, entry):
    """入口 sub（`Boss`）的 `$SPELL_ID` 派发 → {health: {符卡名, …}}。

    形如 `lifeSet(2000); @BossCard1();`（用来给 ordinal 判据做交叉核对）。
    """
    out = {}
    if not entry or entry not in info:
        return out
    body = '\n'.join(info[entry]['body'])
    for m in re.finditer(r'lifeSet\(\s*(\d+)[^;]*;\s*@?(\w+)\s*\(\s*\)', body):
        out.setdefault(int(m.group(1)), set()).add(m.group(2))
    return out


def resolve_by_ordinal(ev, subs, dispatch):
    """`Set Spell Ordinal` N → `BossCard<N>`；`Change Nonspell` N → `Boss<N>`。

    仅在前两条路（阶段链 / 补丁落点）都定不了时使用 —— 即落点全落在**过渡非符**里
    （ST5 BOSS7 的 `Disable Item Drops` 落在 `Boss4`/`Boss4B`，而真正入口是 `BossCard4`）。
    符卡路径必须与 `Set Health` 交叉核对（`Boss()` 派发里 `lifeSet(H)` 要真能到该卡），
    核不上就不用（不猜）。ordinal 字节是小写 ASCII 数字（0x31..0x39）。
    """
    def first(kw):
        for e in ev:
            if e['kind'] == 'pair' and kw in e['comment']:
                return e['val']
        return None

    def digit(v):
        return v - 0x30 if v is not None and 0x31 <= v <= 0x39 else None

    n = digit(first('Spell Ordinal'))
    if n:
        name = 'BossCard%d' % n
        hp = first('Set Health')
        if name in subs and (hp is None or name in dispatch.get(hp, ())):
            return name
    n = digit(first('Nonspell'))
    if n:
        name = 'Boss%d' % n
        if name in subs:
            return name
    return None


# --------------------------------------------------------------------------
# THPatch 解析
# --------------------------------------------------------------------------

def _pair_value(rest):
    """`pair{off, <这里>}` 的值字面量 → int（优先取最后一个十六进制/十进制字面量）。"""
    hs = re.findall(r'0x[0-9a-fA-F]+', rest)
    if hs:
        return int(hs[-1], 0)
    ds = re.findall(r'\d+', rest)
    return int(ds[-1]) if ds else None


def parse_thpatch(path):
    """→ {枚举名: [事件 dict, …]}（按源码顺序）。

    事件 dict：`{'kind': 'jump'|'pair', 'slot': N, 'off': 目标文件偏移,
    'val': 值（pair 才有）, 'comment': 行尾注释}`。
    `slot` = 该行之前最近的 `SetFile(N)`（1-based 取该关 `ecli{}` 第 N 项）。
    """
    lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
    start = next(i for i, l in enumerate(lines) if 'void THPatch(' in l)
    end = next(i for i in range(start + 1, len(lines))
               if re.match(r'\s*__declspec\(noinline\)\s+void\s+\w+\(', lines[i]))
    out, cur, slot = {}, None, 0
    for ln in lines[start:end]:
        m = re.match(r'\s*case THPrac::TH16::(\w+):', ln)
        if m:
            cur = m.group(1)
            out[cur] = []
            slot = 0
            continue
        if cur is None:
            continue
        m = re.search(r'SetFile\((\d+)\)', ln)
        if m:
            slot = int(m.group(1))
            continue
        cm = re.search(r'//\s*(.*)$', ln)
        comment = cm.group(1).strip() if cm else ''
        for mm in re.finditer(r'ECLJump\(ecl,\s*(0x[0-9a-fA-F]+|\d+),\s*'
                              r'(0x[0-9a-fA-F]+|\d+)', ln):
            out[cur].append({'kind': 'jump', 'slot': slot,
                             'off': int(mm.group(2), 0), 'val': None,
                             'comment': comment})
        for mm in re.finditer(r'pair\{\s*(0x[0-9a-fA-F]+|\d+)\s*,([^}]*)\}', ln):
            out[cur].append({'kind': 'pair', 'slot': slot,
                             'off': int(mm.group(1), 0),
                             'val': _pair_value(mm.group(2)),
                             'comment': comment})
    return out


# --------------------------------------------------------------------------
# 主流程
# --------------------------------------------------------------------------

def section_of(record_name):
    """CSV 记录名 'ST1 BOSS2A' → 枚举名 'TH16_ST1_BOSS2A'；章节行返回 None。"""
    if '#' in record_name or '前半' in record_name or '后半' in record_name:
        return None
    parts = record_name.split(' ', 1)
    if len(parts) != 2 or not re.match(r'^ST[1-7]$', parts[0]):
        return None
    return 'TH16_%s_%s' % (parts[0], parts[1])


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--decl', default=DEFAULT_DECL, help='.decl 目录')
    ap.add_argument('--ecl', default=DEFAULT_ECL, help='原始 .ecl 目录')
    ap.add_argument('--csv', default=CSV_PATH, help='目标 CSV')
    ap.add_argument('--check', action='store_true', help='只报告，不写盘')
    args = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

    for p in (args.decl, args.ecl):
        if not os.path.isdir(p):
            sys.exit('找不到目录：%s' % p)

    # ---- 载入素材 ----
    decls, ecls = {}, {}
    for st, files in ECLI.items():
        for f in files:
            dp, ep = (os.path.join(args.decl, f + '.decl'),
                      os.path.join(args.ecl, f + '.ecl'))
            if not (os.path.exists(dp) and os.path.exists(ep)):
                sys.exit('缺素材：%s / %s' % (dp, ep))
            decls[f] = parse_decl(dp)
            ecls[f] = parse_ecl(ep)
    thpatch = parse_thpatch(CPP_PATH)

    # ---- 读 CSV（保 CRLF / 无 BOM）----
    raw = open(args.csv, 'rb').read().decode('utf-8')
    crlf = '\r\n' in raw
    lines = (raw.split('\r\n') if crlf else raw.split('\n'))
    if lines and lines[-1] == '':
        lines.pop()
    header, rows = lines[0], [l.split(',') for l in lines[1:]]

    # ---- 分组：只认 THPatch 里有 case 的记录名（= boss 轴行）----
    boss_rows, mid_rows = {}, {}
    for i, r in enumerate(rows):
        if len(r) < 4:
            continue
        enum = section_of(r[3])
        if enum is None or enum not in thpatch:
            continue
        st = int(r[3][2])
        rest = r[3].split(' ', 1)[1]
        (mid_rows if rest.startswith('MID') else boss_rows).setdefault(st, []).append(i)

    # ---- 每关：算出「阶段 → 记录名」的有序 sub 列表 ----
    answers, notes = {}, {}
    for st in sorted(ECLI):
        mbs = MBS[st]
        # 关底Boss 脚本 = ecli 里名字以 bs 结尾、且不含 mbs 的那个（各关 ecli 顺序不同）
        bs = next(f for f in ECLI[st][1:] if f.endswith('bs') and 'mbs' not in f)

        # 关底Boss：去入口 / 去过场非符后的阶段链
        names, info = decls[bs]
        subs = set(names)
        ch = phase_chain(names, info) or []
        boss_list = [s for s in ch if usable(s) and not is_transition(s, subs, info)]
        if len(boss_list) != len(boss_rows.get(st, [])):
            boss_list = None                       # 长度对不上 → 弃用，改走补丁落点

        # 道中Boss：入口（道中非符）+ 符卡
        mid_list = None
        if mbs and mid_rows.get(st):
            mn, mi = decls[mbs]
            msubs = set(mn)
            entry = next((x for x in mn if ENTRY_RE.match(x)), None)
            mch = phase_chain(mn, mi) or []
            spells = [s for s in mch if 'Card' in s] or \
                     [s for s in mn if PHASE_RE.match(s) and 'Card' in s]
            cand = ([entry] if entry else []) + spells
            if len(cand) != len(mid_rows[st]):
                cand = [s for s in mch if usable(s) and not is_transition(s, msubs, mi)]
            if len(cand) == len(mid_rows[st]):
                mid_list = cand

        for axis, row_ids, fname, lst in (('mid', mid_rows.get(st, []), mbs, mid_list),
                                          ('boss', boss_rows.get(st, []), bs, boss_list)):
            if not row_ids or fname is None:
                continue
            names, info = decls[fname]
            subs_set = set(names)
            entry = next((x for x in names if ENTRY_RE.match(x)), None)
            data, ranges = ecls[fname]
            dispatch = entry_dispatch(info, entry)
            fallback = (entry or 'MBoss') if axis == 'mid' else 'Boss1'
            used = set()
            for idx, ri in enumerate(row_ids):
                enum = section_of(rows[ri][3])
                ev = thpatch[enum]

                # THPatch 补丁落点（目标文件内）→ 候选 sub 集合
                raw = set()
                for e in ev:
                    if e['slot'] == 0 or e['slot'] > len(ECLI[st]) or e['off'] is None:
                        continue
                    if ECLI[st][e['slot'] - 1] != fname:
                        continue
                    s = sub_at(ranges, e['off'])
                    if s and usable(s):
                        raw.add(s)
                # 道中Boss：thprac 把跳转插到入口 sub（MBoss）里、落点紧邻阶段 call，
                # 此时按落点后的第一条 call 补一个候选（ST7 MID1/2/3 → MBossCardN）。
                if axis == 'mid':
                    for e in ev:
                        if e['kind'] != 'jump' or e['slot'] == 0 \
                                or e['slot'] > len(ECLI[st]):
                            continue
                        if ECLI[st][e['slot'] - 1] != fname:
                            continue
                        host = sub_at(ranges, e['off'])
                        if host in ENTRIES:
                            t = first_call_after(data, ranges, host, e['off'])
                            if t and usable(t):
                                raw.add(t)
                # 过渡非符（ST5 `Boss4`、ST2 `Boss3`…）不是阶段落点 → 从候选里剔除
                cand = {s for s in raw if not is_transition(s, subs_set, info)}
                dropped = raw - cand

                if lst is not None:
                    ans = lst[idx]
                elif len(cand) == 1:
                    ans = next(iter(cand))
                elif len(cand) == 2 and sorted(cand)[1] == sorted(cand)[0] + 'B':
                    ans = sorted(cand)[0]          # A/B 分支对 → 取 A
                else:
                    # 兜底：按 `Set Spell Ordinal` / `Change Nonspell` 定位（见函数注释）
                    ans = resolve_by_ordinal(ev, subs_set, dispatch)
                    if ans:
                        notes[ri] = ('补丁落点 %s 全为过渡非符 → 按 ordinal/nonspell 取 %s'
                                     % ('/'.join(sorted(dropped)), ans))
                    else:
                        ans = fallback
                        notes[ri] = '无可用阶段列表/补丁落点 → 兜底 %s' % ans
                if ans in used:
                    notes[ri] = '与同关同轴前序行重复（%s）→ 留空' % ans
                    ans = None
                if ans:
                    used.add(ans)
                    if cand and ans not in cand:
                        notes[ri] = ('注：补丁落点为 %s（入口/过渡非符），'
                                     '按阶段链取 %s' % ('/'.join(sorted(cand)), ans))
                answers[ri] = ans

    # ---- 报告 ----
    filled = 0
    for ri in sorted(answers):
        ans = answers[ri]
        filled += 1 if ans else 0
        print('%s %-18s %-14s%s' % ('OK ' if ans else '-- ', rows[ri][3],
                                    ans or '(留空)',
                                    ('   # ' + notes[ri]) if ri in notes else ''))
    total = sum(len(v) for v in boss_rows.values()) + sum(len(v) for v in mid_rows.values())
    print('\nboss 轴总行数 %d，本次回填 %d，留空 %d' % (total, filled, total - filled))

    if args.check:
        print('（--check：未写盘）')
        return

    for ri, ans in answers.items():
        if ans and not rows[ri][0]:
            rows[ri][0] = ans
    nl = '\r\n' if crlf else '\n'
    out = header + nl + nl.join(','.join(r) for r in rows) + nl
    open(args.csv, 'wb').write(out.encode('utf-8'))
    print('已写回 %s' % args.csv)


if __name__ == '__main__':
    main()

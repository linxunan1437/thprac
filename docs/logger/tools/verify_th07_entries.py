import csv, re, sys
from collections import Counter
SRC = 'thprac/src/thprac/thprac_th07.cpp'
CSV = 'docs/logger/th07_checkpoints.csv'

rows = list(csv.reader(open(CSV, encoding='utf-8')))
hdr, data = rows[0], rows[1:]
print(f'表头            : {hdr}')
print(f'数据行数        : {len(data)}  (期望 140)')
print(f'字段数全为 4    : {all(len(r) == 4 for r in data)}')
nonempty = [r for r in data if r[0].strip()]
print(f'第 1 列非空     : {len(nonempty)}/{len(data)}')
print(f'全为纯十进制数字: {all(re.fullmatch(r"[0-9]+", r[0]) for r in nonempty)}')
print(f'含 0x / 空格    : {any("0x" in r[0] or " " in r[0] for r in nonempty)}')
CHAP = re.compile(r'^ST\d+ (#\d+|前半 #\d+|后半 #\d+)$')
chap = [r for r in data if CHAP.match(r[3])]
boss = [r for r in data if not CHAP.match(r[3])]
print(f'章节行 / boss 行: {len(chap)} / {len(boss)}  (期望 37 / 103)')
print('逐关行数        :', dict(sorted(Counter(r[2] for r in data).items())))

# ---- 从源码重算期望值（章节 = THStageWarp，boss = THPatch；dlg 取非 dlg；yuyuko() 取 0x9d7）
src = open(SRC, encoding='utf-8').read().split('\n')
MCS = {1: (2, 1), 2: (1, 1), 3: (2, 1), 4: (4, 4), 5: (3, 1), 6: (2, 0), 7: (5, 3), 8: (5, 3)}
want, stage, case = {}, None, None
for i, l in enumerate(src, 1):
    if i < 975 or i > 1834:
        continue
    m = re.search(r'if \(stage == (\d+)\)', l)
    if m:
        stage = int(m.group(1))
    m = re.match(r'\s*case (\d+):', l)
    if m:
        case = int(m.group(1))
    m = re.search(r'ECLTimeWarp\((\d+),\s*(0x[0-9a-fA-F]+|\d+)\)', l)
    if not m or i > 1134:          # THPatch 段单独处理
        continue
    a, b = MCS[stage]
    nm = (f'ST{stage} #{case}' if b == 0 else
          f'ST{stage} 前半 #{case}' if case <= a else f'ST{stage} 后半 #{case-a}')
    want[nm] = [int(m.group(2), 0)]
cur = None
for i, l in enumerate(src, 1):
    if i < 1134 or i > 1834:
        continue
    m = re.match(r'\s*case THPrac::TH07::(TH07_\w+):', l)
    if m:
        cur = m.group(1)
        want[cur[5:].replace('_', ' ', 1)] = []
        continue
    m = re.search(r'ECLTimeWarp\((\d+),\s*(0x[0-9a-fA-F]+|\d+)\)', l)
    if m and cur:
        want[cur[5:].replace('_', ' ', 1)].append(int(m.group(2), 0))
for k, v in want.items():          # 双值取第 2 个（非 dlg）；无值 = yuyuko() 行
    want[k] = [v[1] if len(v) == 2 else (v[0] if v else 0x9d7)]
# 2026-10-22 用户决定：ST6 #2 与 ST6 CHAOS 是同一 ECL 点（同为 1180、同一 ins_0），
# 保留 CHAOS、删除 ST6 #2 ⇒ 期望表里去掉该项。
want.pop('ST6 #2', None)
bad = [(r[3], r[0], want.get(r[3])) for r in data if want.get(r[3]) != [int(r[0])]]
print(f'与源码逐行一致  : {not bad}' + ('' if not bad else f'  差异 {bad}'))
sys.exit(1 if bad else 0)

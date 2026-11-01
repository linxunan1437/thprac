"""Read-only TH07 boss evidence audit. Run from repository root.

--write writes only repository audit CSVs; never modifies source material.
Requires pefile and capstone. Default material root: ../reasm/exes.
This inventories patch sites, NOT a validated phase-name mapping.
"""
import argparse
import csv
import hashlib
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser()
p.add_argument('--materials', type=Path, default=ROOT.parent / 'reasm/exes')
p.add_argument('--write', action='store_true')
p.add_argument('--disasm', action='store_true')
a = p.parse_args()
src = (ROOT / 'thprac/src/thprac/thprac_th07.cpp').read_text(encoding='utf-8')
ecls = {}
instructions = []
for stage in range(1, 9):
    path = a.materials / f'ecl/th07/ecldata{stage}.ecl'
    b = path.read_bytes()
    count = struct.unpack_from('<H', b)[0]
    starts = struct.unpack_from(f'<{count}I', b, 0x44)
    decl = (a.materials / f'ecl_dump/th07/ecldata{stage}.decl').read_bytes()
    assert len(re.findall(rb'^sub Sub\d+\(\)', decl, re.M)) == count
    byoff = {}
    for sub, off in enumerate(starts):
        while off + 12 <= len(b):
            time, op, size, mask, params = struct.unpack_from('<iHHHH', b, off)
            if time < 0 or size < 12:
                break
            assert off not in byoff, (stage, sub, off)
            args = list(struct.unpack_from(f'<{(size-12)//4}i', b, off+12)) if (size-12)%4 == 0 else []
            row = [stage, sub, f'0x{off:X}', time, op, f'0x{mask:04X}', f'0x{params:04X}', ';'.join(map(str,args))]
            byoff[off] = row
            instructions.append(row)
            off += size
    ecls[stage] = byoff
    print(path.name, 'subs', count, 'instructions', len(byoff), 'sha256', hashlib.sha256(b).hexdigest())

blocks = {}
matches = list(re.finditer(r'case THPrac::TH07::TH07_(\w+):', src))
for i,m in enumerate(matches):
    end = matches[i+1].start() if i+1 < len(matches) else src.find('default:',m.end())
    blocks[m[1].replace('_',' ',1)] = (m.start(),src[m.end():end])
rows = list(csv.DictReader((ROOT/'docs/logger/th07_checkpoints.csv').open(encoding='utf-8-sig')))
audit = []
for r in rows:
    name = list(r.values())[3]
    if re.fullmatch(r'ST\d+ (?:前半 |后半 )?#\d+',name):
        continue
    pos, block = blocks[name]
    stage = int(re.match(r'ST(\d+)',name)[1])
    sites = []
    for m in re.finditer(r'(ECLSetHealth|ECLSetTime|ECLCallSub|ECLForceSpell)\(ecl,\s*(0x[0-9a-fA-F]+|\d+)',block):
        off = int(m[2],0)
        orig = ecls[stage].get(off)
        assert orig, (name,hex(off))
        sites.append(f'{m[1]}@0x{off:X}:Sub{orig[1]}/op{orig[4]}/t{orig[3]}/args={orig[7]}')
    audit.append([name,stage,src.count('\n',0,pos)+1,' | '.join(sites),'PATCH_SITE_EVIDENCE_ONLY'])
assert len(audit)==103
print('boss input rows',len(audit),'helper-site rows',sum(bool(x[3]) for x in audit))
for x in audit[:6]: print(x)

# ST1 L chain: source patch points alone are insufficient; verify threshold,
# timeout, death callback and opcode133 (copy death callback to timeout).
for off,sub,op,args in [(0x3D58,31,108,'38;0'),(0x4A80,38,148,'0;1700;42'),
                        (0x4AC4,38,115,'42'),(0x4AE4,38,107,'39'),
                        (0x4E30,39,113,'55'),(0x4E70,39,115,'55'),
                        (0x4E90,39,107,'51'),(0x5960,42,133,''),
                        (0x82DC,55,133,'')]:
    row = ecls[1][off]
    assert (row[1],row[4],row[7]) == (sub,op,args)
for off,spell in [(0x59B0,5),(0x832C,9)]:
    assert int(ecls[1][off][7].split(';')[0]) >> 16 == spell
print('ST1 L four-phase chain instruction assertions passed')

import pefile
import capstone
exe = (a.materials/'th07.exe').read_bytes()
pe = pefile.PE(data=exe)
assert hashlib.sha256(exe).hexdigest() == 'f7d03ba1bf6733a60beb3935e0efbbf1436c3d28b4f2155e7e410ba806ce0b9f'
assert pe.get_data(0xe5a1,7).hex() == '66898214020000' # mov [edx+214],ax
print('PE fingerprint and ctx+0x214 subId store verified')
if a.disasm:
    md = capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    for va,n in [(0x40e530,126),(0x411f20,198),(0x413cfa,310),(0x414b54,160),(0x41fd70,263),(0x41ff80,320),(0x414c02,90),(0x415ee7,76),(0x421d7c,43)]:
        for x in md.disasm(pe.get_data(va-0x400000,n),va):
            print(f'VA 0x{x.address:X} / RVA 0x{x.address-0x400000:X}: {x.mnemonic} {x.op_str}')
relevant = [r for r in instructions if r[4] in [41,42,90,99,107,108,109,110,111,112,113,114,115,133,144,148]]
candidates = [
    ['ST1 BOSS1',1,'L','Sub38','0x4A70;0x4A80;0x4AC4;0x4AE4','STATIC_ONLY_NOT_RUNTIME_VERIFIED'],
    ['ST1 BOSS2',1,'L','Sub42','0x4A80;0x4AC4;0x59B0','STATIC_ONLY_NOT_RUNTIME_VERIFIED'],
    ['ST1 BOSS3',1,'L','Sub39','0x4AE4;0x4DC4;0x4E30;0x4E70;0x4E90','STATIC_ONLY_NOT_RUNTIME_VERIFIED'],
    ['ST1 BOSS4',1,'L','Sub55','0x4E30;0x4E70;0x832C','STATIC_ONLY_NOT_RUNTIME_VERIFIED'],
]
exports = [
    ('th07_boss_patch_audit.csv',['record_name','stage','source_case_line','original_helper_sites','status'],audit),
    ('th07_phase_instructions.csv',['stage','sub','file_offset','time','opcode','run_mask_u16','param_mask','args_i32'],relevant),
    ('th07_boss_phase_candidates.csv',['record_name','stage','difficulty','phase_sub','evidence_file_offsets','status'],candidates),
]
for name,header,data in exports:
    if a.write:
        with (ROOT/'docs/logger'/name).open('w',encoding='utf-8',newline='') as f:
            w=csv.writer(f); w.writerow(header); w.writerows(data)
    with (ROOT/'docs/logger'/name).open(encoding='utf-8',newline='') as f:
        actual = list(csv.reader(f))
    assert actual == [header] + [[str(v) for v in r] for r in data], f'{name}: stale output; review and rerun --write'
    print(name, 'matches recomputed evidence:', len(data), 'rows')

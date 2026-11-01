#!/usr/bin/env python3
"""Read-only TH08 phase evidence export. No game process access.

Run from repo: python docs/logger/tools/investigate_th08_boss.py --materials ../reasm/exes --write
CSV rows are static candidates, NOT a runtime-verified whitelist. Normal stages
use run-mask bit3 (L); Extra uses bit4. LW intentionally remains out of scope.
"""
import argparse
import csv
import hashlib
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FILES = dict(zip(['ST1','ST2','ST3','ST4A','ST4B','ST5','ST6A','ST6B','ST7'],
                 ['1','2','3','4a','4b','5','6','7','8']))
# Manually reviewed against THPatch and decl phase setup/transition scripts.
# Wrapper→spell candidates are deliberately retained as candidates until runtime QA.
PHASES = {
 'ST1': ([15,22], [26,38,33,44], [48]),
 'ST2': ([17,23], [27,38,29,44,52], [58]),
 'ST3': ([20,30], [35,44,47,38,62,67], [71]),
 'ST4A': ([], [17,28,19,30,25,33,26,41,46], [54]),
 'ST4B': ([], [17,36,22,41,28,47,31,55,62], [67]),
 'ST5': ([38,45], [51,62,53,66,56,63,75], [78]),
 'ST6A': ([17,21], [33,52,36,56,40,63,44,68,73], [78]),
 'ST6B': ([15,19], [27,50,31,55,34,61,38,67,71], [77,80,82,84,87]),
 'ST7': ([51,56,60], [66,68,70,72,74,76,78,80], [93,97,100,103,108,113,118,126,131,143,149]),
}
KEYOPS = {0x7f:'setBoss',0x83:'lifeSet',0x85:'lifeThreshold',0x86:'timerThreshold',
          0x34:'call',0x35:'ret',0x82:'setNext',0x7a:'spell'}

def decode(path):
    b = path.read_bytes()
    assert struct.unpack_from('<I',b)[0] == 0x800
    n = struct.unpack_from('<H',b,4)[0]
    starts = struct.unpack_from('<'+'I'*n,b,0x48)
    subs=[]
    for sid, start in enumerate(starts):
        off=start; items=[]
        while off+12 <= len(b):
            time,op,size = struct.unpack_from('<ihh',b,off)
            if op == -1: break
            assert size>=12 and off+size<=len(b),(path,sid,hex(off),size)
            items.append((off,time,op,size,b[off+9],b[off+12:off+size]))
            off+=size
        subs.append(items)
    return b,starts,subs

def run(materials):
    source=(ROOT/'thprac/src/thprac/thprac_th08.cpp').read_text(encoding='utf-8')
    rows=[]
    for stage, filenum in FILES.items():
        name='ecldata'+filenum
        path=materials/'ecl/th08'/ (name+'.ecl')
        b, starts, subs=decode(path)
        decl=(materials/'ecl_dump/th08'/(name+'.decl')).read_text(encoding='cp932',errors='replace')
        blocks={int(m[1]):m for m in re.finditer(r'sub Sub(\d+)\(\)\s*\{(.*?)\n\}',decl,re.S)}
        mids,boss,tail=PHASES[stage]
        if stage=='ST7':
            pairs=[(f'ST7 MID{i+1}',s) for i,s in enumerate(mids)]
            pairs += [(f'ST7 END_NS{i+1}',s) for i,s in enumerate(boss)]
            pairs += [(f'ST7 END_S{i+1}',s) for i,s in enumerate(tail[:-1])]
            pairs += [('ST7 END_LS',tail[-1])]
        else:
            pairs=[(f'{stage} MID{i+1}',s) for i,s in enumerate(mids)]
            pairs += [(f'{stage} BOSS{i+1}',s) for i,s in enumerate(boss)]
            pairs += [(stage+(' LS' if len(tail)==1 else f' LS{i+1}'),s) for i,s in enumerate(tail)]
        mask=16 if stage=='ST7' else 8
        for record,sid in pairs:
            m=blocks[sid]
            calls=[]
            for line in m[2].splitlines():
                # Some spell comments contain literal newlines inside strings.
                match=re.match(r'\s*(?:![A-Z*]+\s+)?(\w+)\((.*)',line)
                if match: calls.append((match[1],match[2]))
            assert len(calls)==len(subs[sid]),(name,sid,len(calls),len(subs[sid]))
            evidence=[]; spell=[]; synchronous=[]
            for (off,t,op,size,rank,args),(func,argtext) in zip(subs[sid],calls):
                if func in KEYOPS.values(): assert KEYOPS.get(op)==func,(name,sid,hex(off),op,func)
                if func in KEYOPS.values():
                    tokens=argtext.removesuffix(');').split(',')[:2 if func=='spell' else None]
                    for j,token in enumerate(tokens):
                        if re.fullmatch(r'-?\d+',token.strip()):
                            fmt,width=('<h',2) if func=='spell' else ('<i',4)
                            assert struct.unpack_from(fmt,args,j*width)[0]==int(token),(name,sid,hex(off),func,token)
                if not rank&mask: continue
                if func in ('lifeSet','lifeThreshold','timerThreshold','setNext','valSet','setBoss','spell'):
                    # Never emit copyrighted spell strings: numeric arguments only.
                    payload = ','.join(argtext.split(',')[:2]) if func=='spell' else argtext.removesuffix(');')
                    evidence.append(f'0x{off:X}@{t}:{func}({payload})[op=0x{op:X},mask=0x{rank:X}]')
                if func=='spell':spell.append(argtext.split(',')[1].strip())
                if func=='call':synchronous.append(argtext.removesuffix(');'))
            assert evidence,(name,sid)
            enum='TH08_'+record.replace(' ','_')
            assert enum in source,enum
            rows.append(dict(stage=stage,record=record,file=name+'.ecl',difficulty='Extra' if stage=='ST7' else 'L',
                candidate_sub='Sub'+str(sid),sub_file_offset=f'0x{starts[sid]:X}',
                spell_ids=';'.join(spell),sync_call_targets=';'.join(synchronous),
                evidence=' | '.join(evidence),decl_line=decl.count('\n',0,m.start())+1,
                ecl_sha256=hashlib.sha256(b).hexdigest(),status='static_candidate_unverified'))
    checkpoints=list(csv.DictReader((ROOT/'docs/logger/th08_checkpoints.csv').open(encoding='utf-8-sig')))
    expected={list(r.values())[-1] for r in checkpoints if not re.search(r'#\d+$',list(r.values())[-1]) and not list(r.values())[-1].startswith('LW ')}
    actual={r['record'] for r in rows}
    assert actual==expected,(expected-actual,actual-expected)
    assert len(rows)==104
    return rows

def disasm(materials):
    import pefile
    import capstone
    path=materials/'th08.exe'
    assert hashlib.sha256(path.read_bytes()).hexdigest()=='330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924'
    pe=pefile.PE(str(path));cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    # Every start is a previously decoded instruction boundary; VA + RVA printed.
    for start,end in [(0x41C4B0,0x41C4C0),(0x41C591,0x41C5D6),
                      (0x418450,0x4184A8),(0x421BD0,0x421CA5),
                      (0x41EB64,0x41EBA3),(0x41EC0D,0x41ECA4),
                      (0x42B4DA,0x42B56D),(0x42BA50,0x42BA8E),
                      (0x42DA59,0x42DB0B)]:
        for i in cs.disasm(pe.get_data(start-0x400000,end-start),start):
            print(f'VA 0x{i.address:08X} / RVA 0x{i.address-0x400000:X}: {i.mnemonic} {i.op_str}')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--materials',type=Path,default=ROOT.parent/'reasm/exes')
    ap.add_argument('--write',action='store_true')
    ap.add_argument('--disasm',action='store_true',help='also print fingerprint-checked PE anchors; requires pefile/capstone')
    a=ap.parse_args();rows=run(a.materials)
    out=ROOT/'docs/logger/th08_boss_phase_candidates.csv'
    if a.write:
        with out.open('w',encoding='utf-8-sig',newline='') as f:
            w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
    else:
        with out.open(encoding='utf-8-sig',newline='') as f:assert list(csv.DictReader(f))==[{k:str(v) for k,v in r.items()} for r in rows]
    print('104/104 non-LW rows: candidate sub exists, decl/raw instruction counts agree, run-mask evidence present; 17 LW excluded. NOT runtime validation.')
    for stage in FILES:print(stage,sum(r['stage']==stage for r in rows))
    if a.disasm:disasm(a.materials)

if __name__=='__main__':main()

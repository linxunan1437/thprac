"""Rebuild the reviewed TH07 L/Extra/Phantasm phase dictionary.

Source evidence: th07_boss_phase_re_report.md and th07_monitor_impl_report.md.
This is an explicit dictionary, never an ordinal/warp-time phase counter.
Default is read-only comparison; --write updates the repository CSV only.
"""
from pathlib import Path
import argparse, csv, re, struct
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser(); p.add_argument('--write',action='store_true'); a=p.parse_args()
rows=[]
def add(stage,name,sub,controller=0,slot=-1):
    rows.append([stage,3 if stage<=6 else stage-3,sub,controller,slot,f'ST{stage} {name}'])
def seq(stage,prefix,subs):
    for n,s in enumerate(subs,1): add(stage,f'{prefix}{n}',s)
seq(1,'MID',[20,29]); seq(1,'BOSS',[38,42,39,55])
seq(2,'MID',[40,45]); seq(2,'BOSS',[49,58,53,66,75])
seq(3,'MID',[13,22,27]); seq(3,'BOSS',[32,42,36,48,37,54,56])
add(4,'MID1',35)
for controller,phase,tokens in [(44,1,[58,76,95]),(45,2,[64,80,98]),(47,4,[68,83,103]),(48,5,[68,83,103])]:
    for slot,(name,token) in enumerate(zip(['LUNASA','MERLIN','LYRICA'],tokens),1):
        add(4,f'BOSS{phase}_{name}',token,controller,slot)
for phase,sub in [(3,46),(6,49),(7,51)]: add(4,f'BOSS{phase}',sub)
seq(5,'MID',[38,42]); seq(5,'BOSS',[48,57,52,62,55,75])
seq(6,'MID',[19,23]); seq(6,'BOSS',[29,43,34,50,36,52,38,55,58,62])
for stage,mids,ns,spells in [
    (7,[55,58,61],[68,70,71,73,75,77,79,81],[85,88,94,97,99,101,106,110,83,84]),
    (8,[57,60,63],[70,72,73,75,77,79,81,83],[88,93,97,100,102,104,109,114,85,86,87])]:
    seq(stage,'MID',mids); seq(stage,'END_NS',ns); seq(stage,'END_S',spells)
# The last Extra spell is invoked from Sub84; recognize its body too.
add(7,'END_S10',123)
checkpoints=list(csv.reader((ROOT/'docs/logger/th07_checkpoints.csv').open(encoding='utf-8-sig')))[1:]
boss={r[3] for r in checkpoints if not re.fullmatch(r'ST\d+ (?:前半 |后半 )?#\d+',r[3])}
assert {r[-1] for r in rows} == boss-{'ST6 CHAOS'}
assert len({(r[0],r[2],r[3],r[4]) for r in rows})==len(rows)
materials=ROOT.parent/'reasm/exes'
source=(ROOT/'thprac/src/thprac/thprac_th07.cpp').read_text(encoding='utf-8')
cases=list(re.finditer(r'case THPrac::TH07::TH07_(\w+):',source))
blocks={m[1].replace('_',' ',1):source[m.end():cases[i+1].start() if i+1<len(cases) else source.find('default:',m.end())] for i,m in enumerate(cases)}
raw={}
for stage in range(1,9):
    b=(materials/f'ecl/th07/ecldata{stage}.ecl').read_bytes()
    if stage==3: assert struct.unpack_from('<HHHH',b,0xB054)==(853,13,0,32)
    if stage==4: assert struct.unpack_from('<HHHH',b,0x1CEBC)==(7122,35,0,32)
    for sub in range(struct.unpack_from('<H',b)[0]):
        off=struct.unpack_from('<I',b,0x44+4*sub)[0]
        while off+12<=len(b):
            t,op,size,mask,param=struct.unpack_from('<iHHHH',b,off)
            if t<0 or size<12: break
            args=list(struct.unpack_from(f'<{(size-12)//4}i',b,off+12)) if (size-12)%4==0 else []
            raw[stage,off]=(sub,t,op,mask,args)
            off+=size
proofs=0
for stage,diff,sub,controller,slot,name in rows:
    if controller: continue
    block=blocks[name]
    helper=list(re.finditer(r'(ECLSetTime|ECLSetHealth)\(ecl,\s*(0x[\da-fA-F]+|\d+),\s*(0x[\da-fA-F]+|\d+),\s*(0x[\da-fA-F]+|\d+)\)',block))
    if helper:
        m=helper[-1]; off=int(m[2],0); owner=raw[stage,off][0]
        prefix=[r for (s,o),r in raw.items() if s==stage and o<off and r[0]==owner and r[3] & (1<<(diff+8))]
        if m[1]=='ECLSetTime':
            targets=[r[4][0] for r in prefix if r[2]==115]
            if name=='ST3 BOSS4':
                assert 'pair{0x42bc, 0x30}' in block and sub==48
            else: assert targets and targets[-1]==sub,(name,targets,sub)
        else:
            hp=int(m[4],0)
            thresholds=[]; threshold=None
            for r in prefix:
                if r[2]==112: threshold=r[4][0]
                if r[2]==113 and threshold is not None: thresholds.append((threshold,r[4][0]))
                if r[2]==148: thresholds.append((r[4][1],r[4][2]))
            assert (hp+1,sub) in thresholds,(name,hp,sub,thresholds)
        proofs+=1
    elif 'ECLForceSpell' in block:
        m=re.search(r'ECLForceSpell\(ecl,\s*(?:0x[\da-fA-F]+|\d+),\s*(?:0x[\da-fA-F]+|\d+),\s*(?:0x[\da-fA-F]+|\d+),\s*(?:0x[\da-fA-F]+|\d+),\s*(0x[\da-fA-F]+|\d+)',block)
        assert m and (int(m[1],0)&255)==sub,name
        proofs+=1
    else:
        call=list(re.finditer(r'ECLCallSub\(ecl,\s*(?:0x[\da-fA-F]+|\d+),\s*(?:0x[\da-fA-F]+|\d+),\s*(0x[\da-fA-F]+|\d+)\)',block))
        if call:
            assert int(call[-1][1],0)==sub,(name,sub)
            proofs+=1
print('Independent source/helper/byte target checks:',proofs)
evidence=[]
for stage,diff,sub,controller,slot,name in rows:
    b=(materials/f'ecl/th07/ecldata{stage}.ecl').read_bytes()
    count=struct.unpack_from('<H',b)[0]
    assert 0<=sub<count
    off=struct.unpack_from('<I',b,0x44+4*sub)[0]
    assert struct.unpack_from('<iHH',b,off)[0]>=0
    decl=(materials/f'ecl_dump/th07/ecldata{stage}.decl').read_bytes()
    assert re.search(rb'^sub Sub'+str(sub).encode()+rb'\(\)',decl,re.M)
    if controller:
        # Route markers must be installed as a real interrupt target by the
        # specified sister; controller is matched separately at runtime.
        owner={1:53,2:71,3:88}[slot]
        start=struct.unpack_from('<I',b,0x44+4*owner)[0]
        targets=[]
        while True:
            t,op,size=struct.unpack_from('<iHH',b,start)
            if t<0 or size<12: break
            if op==108: targets.append(struct.unpack_from('<i',b,start+12)[0])
            start+=size
        assert sub in targets
    evidence.append([stage,diff,sub,controller,slot,name,f'0x{off:X}','STATIC_UNVERIFIED'])
header=['stage','difficulty','sub','controller_sub','slot','record_name','sub_file_start','status']
out=ROOT/'docs/logger/th07_boss_phases.csv'
if a.write:
    with out.open('w',encoding='utf-8',newline='') as f:
        w=csv.writer(f);w.writerow(header);w.writerows(evidence)
with out.open(encoding='utf-8',newline='') as f: actual=list(csv.reader(f))
assert actual==[header]+[[str(v) for v in r] for r in evidence], 'stale phase map'
print(f'{len(rows)} sub rules / {len(set(r[-1] for r in rows))} Boss records + ST6 CHAOS timeline = {len(boss)} input records; static, not runtime verified')

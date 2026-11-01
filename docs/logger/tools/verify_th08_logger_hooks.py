#!/usr/bin/env python3
"""Fingerprint and whole-instruction validation for TH08 logger event hooks.

Only reads the supplied original PE. Does not attach to or launch the game.
"""
import argparse
import hashlib
import struct
from pathlib import Path
import capstone
import pefile

ROOT=Path(__file__).resolve().parents[3]
HOOKS={
    0x43BDD2:'bf34cf6401',       # existing new-run statistics reset hook
    0x44CA94:'8b15a8cf6401',     # normal bomb count
    0x44CA83:'8b0daccf6401',     # ALL deathbomb paths join here
    0x44CD29:'e832150000',       # only caller of miss/life adjustment
    0x439C1F:'c70540f5600101000000', # clear-state dispatch before practice/endings
}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--materials',type=Path,default=ROOT.parent/'reasm/exes');a=ap.parse_args()
    path=a.materials/'th08.exe';blob=path.read_bytes()
    assert hashlib.sha256(blob).hexdigest()=='330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924'
    pe=pefile.PE(data=blob);cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    for va,expected in HOOKS.items():
        raw=pe.get_data(va-0x400000,len(bytes.fromhex(expected)))
        assert raw.hex()==expected,(hex(va),raw.hex())
        instructions=list(cs.disasm(raw,va))
        assert sum(i.size for i in instructions)==len(raw)
        print(f'VA 0x{va:X} / RVA 0x{va-0x400000:X}: {raw.hex(" ")} ({len(raw)}B whole instructions)')
    text=pe.get_data(0x2000,0xB1C00)
    calls=[0x402000+i for i in range(len(text)-5) if text[i]==0xE8 and 0x402000+i+5+struct.unpack_from('<i',text,i+1)[0]==0x44E260]
    assert calls==[0x44CD29],calls
    # Miss function increments GM+0x3DA9C; GM=0x160F508 -> 0x164CFA4.
    assert 0x160F508+0x3DA9C==0x164CFA4
    assert pe.get_data(0x44E2B0-0x400000,12).hex()=='8b829cda030083c0018b4dfc'
    # Deathbomb count then unconditional jump over normal bomb count: mutually exclusive.
    jump=list(cs.disasm(pe.get_data(0x44CA92-0x400000,2),0x44CA92))[0]
    assert jump.mnemonic=='jmp' and jump.op_str=='0x44caaf'
    for branch in [0x44CA38,0x44CA68]:
        i=list(cs.disasm(pe.get_data(branch-0x400000,2),branch))[0]
        assert i.mnemonic=='jmp' and i.op_str=='0x44ca83'
    # File-index -> normal ECL filename mapping must match runtime stage routing.
    for index,suffix in enumerate(['1','2','3','4a','4b','5','6','7','8']):
        ptr=struct.unpack('<I',pe.get_data(0x4C73CC-0x400000+index*4,4))[0]
        assert pe.get_data(ptr-0x400000,24).split(b'\0')[0]==('ecldata'+suffix+'.ecl').encode()
    print('Verified: unique miss call; all deathbomb joins; normal/deathbomb exclusion; 9 ECL file indices.')

if __name__=='__main__':main()

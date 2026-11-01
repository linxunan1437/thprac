"""Compile the ACTUAL runtime route helpers in an isolated Windows harness.

No copied model of the helpers: their bodies are extracted from thprac_th07.cpp.
Temporary object/exe live in ignored artifacts/; no game is launched.
"""
from pathlib import Path
import subprocess, tempfile
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'thprac/src/thprac/thprac_th07.cpp').read_text(encoding='utf-8')
def balanced(start):
    brace=source.index('{',start); depth=1; end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]
active=balanced(source.index('static bool ActiveSister('))
begin=source.index('uint16_t controller = 0;',source.index('void TH07Logger::Update()'))
end=source.index('// A routed row',begin)
select=source[begin:end]
test='''
typedef unsigned short uint16_t;
struct BossView { uint16_t subs[16]; int count; };
'''+active+'''
static uint16_t Controller(BossView* views) { int stage=4;
'''+select+'''
return controller; }
int main() {
    BossView views[8]; for(int k=0;k<8;++k) views[k].count=0;
    views[0].count=3; views[0].subs[0]=47; views[0].subs[1]=46; views[0].subs[2]=44;
    if(Controller(views)!=47) return 1;
    views[0].subs[0]=48; views[0].subs[1]=47;
    if(Controller(views)!=48) return 2;
    views[0].subs[0]=46; views[0].subs[1]=44;
    if(Controller(views)!=46) return 3;
    BossView body; body.count=3; body.subs[0]=59; body.subs[1]=58; body.subs[2]=53;
    if(!ActiveSister(body,58,44)) return 4;
    body.subs[0]=63; body.subs[1]=58;
    if(ActiveSister(body,58,44)) return 5;
    body.subs[0]=68; body.subs[1]=58;
    if(!ActiveSister(body,68,47)) return 6;
    body.subs[0]=69; body.subs[1]=68;
    if(ActiveSister(body,68,47)) return 7;
    body.subs[0]=128; body.subs[1]=68;
    if(!ActiveSister(body,68,48)) return 8;
    body.subs[0]=85; body.subs[1]=83;
    if(ActiveSister(body,83,48)) return 9;
    body.subs[0]=106; body.subs[1]=103;
    if(ActiveSister(body,103,48)) return 10;
    body.subs[0]=138; body.subs[1]=83;
    if(!ActiveSister(body,83,48)) return 11;
    body.subs[0]=145; body.subs[1]=103;
    if(!ActiveSister(body,103,48)) return 12;
    return 0;
}
'''
vc=Path('D:/tools/VS/VC/Tools/MSVC')
version=max(vc.iterdir(),key=lambda x:tuple(map(int,x.name.split('.'))))
compiler=version/'bin/Hostx64/x86/cl.exe'; linker=version/'bin/Hostx64/x86/link.exe'
scratch=ROOT/'artifacts'; scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='th07-route-',dir=scratch) as temp:
    temp=Path(temp); cpp=temp/'route.cpp'; obj=temp/'route.obj'; exe=temp/'route.exe'
    assert temp.resolve().is_relative_to(scratch.resolve())
    cpp.write_text(test,encoding='utf-8')
    subprocess.run([str(compiler),'/nologo','/c','/GS-','/Zl','/Oi','/Fo'+str(obj),str(cpp)],check=True,capture_output=True)
    subprocess.run([str(linker),'/nologo','/entry:main','/subsystem:console','/nodefaultlib','/out:'+str(exe),str(obj)],check=True,capture_output=True)
    result=subprocess.run([str(exe)])
    assert result.returncode==0, f'actual runtime route regression case {result.returncode} failed'
print('12 actual C++ route regressions passed: nested controllers, support roots, spell-body ancestors')

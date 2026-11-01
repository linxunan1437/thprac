"""Exercise the production chapter selection/debounce code with frame sequences.

No game is launched. Includes a negative control using the old per-frame consume.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'thprac/src/thprac/thprac_th07.cpp').read_text(encoding='utf-8')
start = source.index('        int chapterTime = -1;', source.index('void TH07Logger::Update()'))
end = source.index('        mSession.Latch(', start)
block = source[start:end].replace(
    '*reinterpret_cast<const int32_t*>(0x12FE0FC)', 'time')
edge_start = source.index('        const bool bossEntered =', source.index('void TH07Logger::Update()'))
edge_end = source.index('        const int diff', edge_start)
edge = source[edge_start:edge_end]

harness = r'''
namespace std {
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a - *b;
}
int max(int a, int b) { return a > b ? a : b; }
}
struct Record { const char* value; const char* c_str() const { return value; } };
struct Chapter { int stage, time; Record record; };
struct Monitor {
    Chapter mChapters[3] = {{1,540,{"front"}}, {1,3107,{"back1"}}, {1,4000,{"back2"}}};
    const Chapter* mDeferredChapter = nullptr;
    const char* mSection = "MID2";
    const char* mPending = nullptr;
    int mPendingFrames = 0, mLastChapter = -1;
    bool mHadBoss = false, mStageClear = false;
    void frame(bool hasBoss, int time) {
        int stage = 1;
        const char* candidate = nullptr;
        EDGE
        BLOCK
    }
    bool is(const char* s) { return std::strcmp(mSection, s) == 0; }
};
int main() {
    // Late kill: chapter boundary passes while the midboss is alive.
    Monitor late;
    late.frame(true,2656); late.frame(true,3200); late.frame(true,3300);
    if (!late.is("MID2")) return 1;
    late.frame(false,3301);
    if (!late.is("MID2")) return 2;
    late.frame(false,3302);
    if (!late.is("back1")) return 3;
    // Early kill: retain MID2 until the actual next chapter boundary.
    Monitor early;
    early.frame(true,2656); early.frame(false,2800); early.frame(false,2801);
    if (!early.is("MID2")) return 4;
    early.frame(false,3107); early.frame(false,3108);
    if (!early.is("back1")) return 5;
    // Direct boss warp: chapters before entry must never replay on exit.
    Monitor warp;
    warp.frame(true,5043); warp.frame(false,5044); warp.frame(false,5045);
    if (!warp.is("MID2")) return 6;
    // Multiple boundaries passed: choose the latest; no chapter regression.
    Monitor multiple;
    multiple.frame(true,2656); multiple.frame(true,4100);
    // Cached boundary survives a lower timeline sample at handoff.
    multiple.frame(false,3900); multiple.frame(false,3901);
    if (!multiple.is("back2")) return 7;
    multiple.frame(false,4200); multiple.frame(false,4201);
    if (!multiple.is("back2")) return 8;
    // New boss window consumes the intervening chapters only once.
    multiple.mSection = "BOSS1";
    multiple.frame(true,5043); multiple.frame(false,5044); multiple.frame(false,5045);
    if (!multiple.is("BOSS1")) return 9;
    return 0;
}
'''.replace('EDGE', edge).replace('BLOCK', block)

version = max(Path('D:/tools/VS/VC/Tools/MSVC').iterdir(),
              key=lambda p: tuple(map(int, p.name.split('.'))))
compiler = version / 'bin/Hostx64/x86/cl.exe'
linker = version / 'bin/Hostx64/x86/link.exe'
scratch = ROOT / 'artifacts'
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='th07-handoff-', dir=scratch) as temp:
    temp = Path(temp)
    for name, body in [('fixed', harness), ('old', harness.replace('if (bossEntered)', 'if (hasBoss)'))]:
        cpp, obj, exe = (temp / (name + suffix) for suffix in ['.cpp', '.obj', '.exe'])
        cpp.write_text(body, encoding='utf-8')
        subprocess.run([str(compiler), '/nologo', '/c', '/GS-', '/Zl', '/Oi',
                        '/Fo' + str(obj), str(cpp)], check=True, capture_output=True)
        subprocess.run([str(linker), '/nologo', '/entry:main', '/subsystem:console',
                        '/nodefaultlib', '/out:' + str(exe), str(obj)], check=True, capture_output=True)
        result = subprocess.run([str(exe)])
        expected = 0 if name == 'fixed' else 3
        assert result.returncode == expected, (name, result.returncode, expected)
print('Production C++ handoff regressions passed; old code fails late-kill case as expected.')

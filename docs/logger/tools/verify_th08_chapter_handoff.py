"""Compile production TH08 chapter selection/debounce against frame sequences.

Uses real chapter CSV offsets for all nine normal file routes. No game is run.
The negative control consumes the floor during battle, like the TH07 bug.
"""
from pathlib import Path
import csv
import json
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'thprac/src/thprac/thprac_th08.cpp').read_text(encoding='utf-8')
start = source.index('        uint32_t chapterOffset = 0;', source.index('void TH08Logger::Update()'))
end = source.index('        mSession.Latch(', start)
block = source[start:end]
chapters = []
with (ROOT / 'docs/logger/th08_checkpoints.csv').open(encoding='utf-8-sig', newline='') as f:
    for row in csv.DictReader(f):
        if '#' in row['记录名']:
            chapters.append((row['所属关卡'], row['记录名'], int(row['时间轴位置'].split('/')[1], 16)))
chapters.sort(key=lambda r: r[2])
quote = lambda s: json.dumps(s, ensure_ascii=False)
entries = ',\n'.join('{' + quote(s) + ',' + quote(r) + ',' + str(o) + '}' for s, r, o in chapters)
tests = []
for stage in dict.fromkeys(r[0] for r in chapters):
    rows = [r for r in chapters if r[0] == stage]
    first, last = rows[0], rows[-1]
    assert len(rows) >= 2
    tests.append(f'''
    {{
        Monitor m;
        m.mStageKey = {quote(stage)};
        m.frame(false, {first[2]}); m.frame(false, {first[2]});
        if (!m.is({quote(first[1])})) return 1;
        m.mSection = "MID2";
        m.frame(true, {first[2]});
        // A live Boss must keep its section and leave the chapter floor alone.
        m.frame(true, {last[2]}); m.frame(true, {last[2]});
        if (!m.is("MID2") || m.mChapterFloor != {first[2]}) return 2;
        m.frame(false, {last[2]});
        if (!m.is("MID2")) return 3;
        m.frame(false, {last[2]});
        if (!m.is({quote(last[1])}) || m.mChapterFloor != {last[2]}) return 4;
        m.frame(false, {last[2]}); m.frame(false, {last[2]});
        if (!m.is({quote(last[1])})) return 5;
        // Early kill: wait until the next actual boundary.
        Monitor early;
        early.mStageKey = {quote(stage)};
        early.mChapterFloor = {first[2]}; early.mSection = "MID2";
        early.frame(true, {first[2]});
        early.frame(false, {first[2]}); early.frame(false, {first[2]});
        if (!early.is("MID2")) return 6;
        early.frame(false, {rows[1][2]}); early.frame(false, {rows[1][2]});
        if (!early.is({quote(rows[1][1])})) return 7;
    }}''')

harness = r'''
typedef unsigned int uint32_t;
namespace std {
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}
}
struct Text {
    const char* value;
    const char* c_str() const { return value; }
    bool operator==(const char* other) const { return std::strcmp(value, other) == 0; }
};
struct Chapter { Text stage, record; uint32_t offset; };
struct Monitor {
    Chapter mChapters[COUNT] = {ENTRIES};
    const char* mStageKey = "";
    const char* mSection = "main";
    const char* mPending = nullptr;
    int mPendingFrames = 0;
    uint32_t mChapterFloor = 0;
    void frame(bool bossPresent, uint32_t offset) {
        const char* candidate = bossPresent ? "MID2" : nullptr;
        NEGATIVE_CONTROL
        BLOCK
    }
    bool is(const char* text) const { return std::strcmp(mSection, text) == 0; }
};
int main() {
    TESTS
    return 0;
}
'''.replace('COUNT', str(len(chapters))).replace('ENTRIES', entries).replace('BLOCK', block).replace('TESTS', '\n'.join(tests))
version = max(Path('D:/tools/VS/VC/Tools/MSVC').iterdir(),
              key=lambda p: tuple(map(int, p.name.split('.'))))
scratch = ROOT / 'artifacts'
scratch.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='th08-handoff-', dir=scratch) as temp:
    temp = Path(temp)
    for name, injection in [('current', ''), ('bad_floor', 'if (bossPresent) mChapterFloor = offset;')]:
        cpp, obj, exe = (temp / (name + suffix) for suffix in ['.cpp', '.obj', '.exe'])
        cpp.write_text(harness.replace('NEGATIVE_CONTROL', injection), encoding='utf-8')
        subprocess.run([str(version / 'bin/Hostx64/x86/cl.exe'), '/nologo', '/c', '/GS-', '/Zl', '/Oi', '/O2', '/utf-8',
                        '/Fo' + str(obj), str(cpp)], check=True, capture_output=True)
        subprocess.run([str(version / 'bin/Hostx64/x86/link.exe'), '/nologo', '/entry:main', '/subsystem:console',
                        '/nodefaultlib', '/out:' + str(exe), str(obj)], check=True, capture_output=True)
        result = subprocess.run([str(exe)])
        assert result.returncode == (0 if name == 'current' else 2), (name, result.returncode)
print('TH08 production chapter handoff passed for 9 routes; battle-floor negative control rejected.')

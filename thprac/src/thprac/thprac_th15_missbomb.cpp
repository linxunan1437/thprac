#include "thprac_th15_missbomb.h"

#include <Windows.h>
#include <cstdio>
#include <cstring>

namespace THPrac {
namespace TH15 {

    TH15MissBombRecorder g_th15mb;

    // ===== 阶段名映射表（用户手动填写）=====
    // key   = "<stage>-<name>"：stage = 关卡号(1~7)，name = 换攻击钩缓存到的原始阶段名
    // value = 写入文件的阶段名
    // 查不到 key 时回退：直接记录原始 name。
    struct Th15MbMapEntry {
        const char* key;
        const char* output;
    };
    static const Th15MbMapEntry TH15_MB_PHASE_MAP[] = {
        { "1-BossDead",   "BossCard1" },   // 示例，待手动填写
        { "1-BossEscape", "BossCard1" },   // 示例，待手动填写
    };

    // ===== 道中章节映射表（原始 ECL sub 名 -> 练习菜单章节名）=====
    // key = "<stage>-<sub>"：sub 来自 GetEclSubName()，仅收录主流程 MainSubNN。
    // value = 练习菜单章节格式：前半/后半（ST6 {4,0} 无后半，用纯数字）。
    //   章节划分由 THStageWarp 各 portion 的 ECLJump 实际落点反推。
    // *b/*c 等 async sub、main/MainFront/MainLatter 等容器不建条目，
    // 命不中时回退记录原始 sub 名。
    static const Th15MbMapEntry TH15_MB_MID_MAP[] = {
        // ST1 {2,4}：p2→MainSub01b(前半2)，p3/p4→MainSub04/05(后1/2)，p5/p6→MainSub06/07(后3/4)
        { "1-MainSub00", "前半 #1" }, { "1-MainSub01", "前半 #2" },
        { "1-MainSub02", "前半 #2" }, { "1-MainSub03", "前半 #2" },
        { "1-MainSub04", "后半 #1" }, { "1-MainSub05", "后半 #2" },
        { "1-MainSub06", "后半 #3" }, { "1-MainSub07", "后半 #4" },
        // ST2 {3,3}
        { "2-MainSub00", "前半 #1" }, { "2-MainSub01", "前半 #1" },
        { "2-MainSub02", "前半 #2" }, { "2-MainSub03", "前半 #3" },
        { "2-MainSub04", "后半 #1" }, { "2-MainSub05", "后半 #2" },
        { "2-MainSub06", "后半 #3" },
        // ST3 {3,3}
        { "3-MainSub00", "前半 #1" }, { "3-MainSub01", "前半 #2" },
        { "3-MainSub02", "前半 #3" }, { "3-MainSub03", "后半 #1" },
        { "3-MainSub04", "后半 #2" }, { "3-MainSub05", "后半 #3" },
        // ST4 {3,3}
        { "4-MainSub00", "前半 #1" }, { "4-MainSub01", "前半 #2" },
        { "4-MainSub02", "前半 #3" }, { "4-MainSub03", "前半 #3" },
        { "4-MainSub04", "后半 #1" }, { "4-MainSub05", "后半 #2" },
        { "4-MainSub06", "后半 #3" },
        // ST5 {4,6}
        { "5-MainSub00", "前半 #1" }, { "5-MainSub01", "前半 #2" },
        { "5-MainSub02", "前半 #3" }, { "5-MainSub03", "前半 #4" },
        { "5-MainSub04", "后半 #1" }, { "5-MainSub05", "后半 #2" },
        { "5-MainSub06", "后半 #3" }, { "5-MainSub07", "后半 #4" },
        { "5-MainSub08", "后半 #5" }, { "5-MainSub09", "后半 #6" },
        // ST6 {4,0}：无后半，菜单用纯数字
        { "6-MainSub00", "1" }, { "6-MainSub01", "1" },
        { "6-MainSub02", "2" }, { "6-MainSub03", "3" },
        { "6-MainSub04", "4" },
    };

    // 读取一个以 '\0' 结尾的 C 字符串到定长缓冲；空串/空指针返回 false。
    static bool ReadCStr(char* dst, size_t dstSize, uintptr_t addr)
    {
        if (!dst || dstSize == 0)
            return false;
        dst[0] = '\0';
        if (!addr)
            return false;
        strncpy_s(dst, dstSize, (const char*)addr, _TRUNCATE);
        return dst[0] != '\0';
    }

    const char* TH15MissBombRecorder::LookupMappedName(int stage, const char* name) const
    {
        if (!name || !name[0])
            return nullptr;
        char key[80] = {};
        sprintf_s(key, "%d-%s", stage, name);
        for (const auto& entry : TH15_MB_PHASE_MAP) {
            if (strcmp(entry.key, key) == 0)
                return entry.output;
        }
        return nullptr;
    }

    const char* TH15MissBombRecorder::LookupMidStageName(int stage, const char* eclSubName) const
    {
        if (!eclSubName || !eclSubName[0])
            return nullptr;
        char key[80] = {};
        sprintf_s(key, "%d-%s", stage, eclSubName);
        for (const auto& entry : TH15_MB_MID_MAP) {
            if (strcmp(entry.key, key) == 0)
                return entry.output;
        }
        return nullptr;
    }

    const char* TH15MissBombRecorder::GetBossStageName() const
    {
        if (!mBossPtr)
            return nullptr;
        if (!(*(uint32_t*)(mBossPtr + TH15_MB_BOSS_GATE_OFF) & TH15_MB_BOSS_GATE_MASK))
            return nullptr;
        return mCurStageName[0] ? mCurStageName : nullptr;
    }

    void TH15MissBombRecorder::OnEclHostCaptured(uintptr_t host)
    {
        mEclHost = host;
    }

    const char* TH15MissBombRecorder::GetEclSubName() const
    {
        if (!mEclHost)
            return nullptr;
        uintptr_t thread = *(uintptr_t*)(mEclHost + 0x0C);
        if (!thread)
            return nullptr;
        int subIdx = *(int32_t*)(thread + 4);
        uintptr_t res = *(uintptr_t*)(mEclHost + 0x11F8);
        if (!res)
            return nullptr;
        int count = *(int32_t*)(res + 8);
        if (subIdx < 0 || subIdx >= count)
            return nullptr;
        uintptr_t table = *(uintptr_t*)(res + 0x8C);
        if (!table)
            return nullptr;
        const char* name = *(const char**)(table + (uintptr_t)subIdx * 8);
        return (name && name[0]) ? name : nullptr;
    }

    const char* TH15MissBombRecorder::GetLastEvent() const
    {
        return mLastEvent[0] ? mLastEvent : nullptr;
    }

    void TH15MissBombRecorder::OnGameStart(bool recordEnabled)
    {
        // 清 boss / ECL 状态，避免换关 / boss 退场后残留野指针。
        mBossPtr = 0;
        mCurStageName[0] = '\0';
        mEclHost = 0;
        mLastEvent[0] = '\0';

        if (!recordEnabled) {
            OnRunEnd();
            return;
        }
        if (mLineOpen)   // continue / FastRetry 属同一局，不另起行
            return;

        mLineOpen = true;
        mRunBodySeen = false;
        mLineLen = 0;
        mLineBuf[0] = '\0';

        SYSTEMTIME st;
        GetLocalTime(&st);
        sprintf_s(mStartTime, "[%04d-%02d-%02d %02d:%02d:%02d]",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    }

    void TH15MissBombRecorder::CacheBossPtr(uintptr_t bossPtr)
    {
        mBossPtr = bossPtr;
    }

    void TH15MissBombRecorder::OnBossEnter()
    {
        // 新 boss 进场：清空上一 boss 的原始阶段名缓存（不写死首阶段名）。
        mCurStageName[0] = '\0';
    }

    void TH15MissBombRecorder::OnPatternSwitch(uintptr_t newNamePtr)
    {
        if (!mBossPtr)
            return;
        // 缓存 0x202 本次新写入的名字（通常是“下一阶段”名）。落盘时由映射表把它
        // 换算成输出名；只在读到非空名字时更新，避免空指针清掉已有名。
        char name[64] = {};
        if (ReadCStr(name, sizeof(name), newNamePtr))
            strcpy_s(mCurStageName, name);
    }

    void TH15MissBombRecorder::Record(char kind, int stage)
    {
        if (!mLineOpen)
            return;

        const char* raw = GetBossStageName();
        const char* name = nullptr;
        if (raw) {
            // boss 战：查 <stage>-<name> 映射表，查不到记录原始名。
            name = LookupMappedName(stage, raw);
            if (!name)
                name = raw;
        } else {
            // 非 boss（道中）：用当前 ECL sub 名查道中段落映射，查不到记录原始 sub 名。
            const char* eclSub = GetEclSubName();
            if (eclSub) {
                name = LookupMidStageName(stage, eclSub);
                if (!name)
                    name = eclSub;
            }
        }

        // 缓存给 IGI：上一次 miss/bomb 的位置（boss 阶段名或道中段落名）。
        if (name)
            sprintf_s(mLastEvent, "%c(%s)", kind, name);
        else
            sprintf_s(mLastEvent, "%c(-)", kind);

        int remain = (int)sizeof(mLineBuf) - mLineLen;
        if (remain <= 1)
            return;

        int n;
        if (name)
            n = sprintf_s(mLineBuf + mLineLen, remain, " ST%d %c(%s)", stage, kind, name);
        else
            n = sprintf_s(mLineBuf + mLineLen, remain, " ST%d %c(-)", stage, kind);
        if (n > 0)
            mLineLen += n;
    }

    void TH15MissBombRecorder::MarkRunActive()
    {
        if (mLineOpen)
            mRunBodySeen = true;
    }

    void TH15MissBombRecorder::OnRunEnd()
    {
        // 清 boss / ECL 状态，避免残留野指针。
        mBossPtr = 0;
        mCurStageName[0] = '\0';
        mEclHost = 0;
        mLastEvent[0] = '\0';

        if (!mLineOpen)      // 没开着行（菜单里每帧被调用）→ 直接走
            return;
        if (!mRunBodySeen)   // 本局还没真正开始（player 尚未出现）→ 不结束、不落盘
            return;

        mLineOpen = false;
        mRunBodySeen = false;

        // 每一局都写一行，不论有没有 miss/bomb（空局这行只有时间戳）。
        // 用项目既有 Win32 方式追加写盘（游戏 exe 工作目录相对路径）。
        HANDLE hFile = CreateFileW(L"th15_missbomb.txt",
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return;

        DWORD written = 0;
        if (GetFileSize(hFile, nullptr) == 0) {
            const char* header = "# th15 miss/bomb record (M=miss, B=bomb)\r\n";
            WriteFile(hFile, header, (DWORD)strlen(header), &written, nullptr);
        }
        SetFilePointer(hFile, 0, nullptr, FILE_END);

        mLineBuf[mLineLen] = '\0';
        WriteFile(hFile, mStartTime, (DWORD)strlen(mStartTime), &written, nullptr);
        if (mLineLen > 0)
            WriteFile(hFile, mLineBuf, (DWORD)mLineLen, &written, nullptr);
        const char* newline = "\r\n";
        WriteFile(hFile, newline, 2, &written, nullptr);

        CloseHandle(hFile);

        mLineLen = 0;
        mLineBuf[0] = '\0';
    }

}
}

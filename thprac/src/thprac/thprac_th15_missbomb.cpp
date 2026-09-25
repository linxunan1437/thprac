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

    const char* TH15MissBombRecorder::GetBossStageName() const
    {
        if (!mBossPtr)
            return nullptr;
        if (!(*(uint32_t*)(mBossPtr + TH15_MB_BOSS_GATE_OFF) & TH15_MB_BOSS_GATE_MASK))
            return nullptr;
        return mCurStageName[0] ? mCurStageName : nullptr;
    }

    void TH15MissBombRecorder::OnGameStart(bool recordEnabled)
    {
        // 清 boss 状态，避免换关 / boss 退场后残留野指针。
        mBossPtr = 0;
        mCurStageName[0] = '\0';

        if (!recordEnabled) {
            OnRunEnd();
            return;
        }
        if (mLineOpen)   // continue / FastRetry 属同一局，不另起行
            return;

        mLineOpen = true;
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
        const char* name = raw ? LookupMappedName(stage, raw) : nullptr;
        if (!name)
            name = raw;   // 查不到 key（或非 boss 战）回退原始名，raw 为空则记 '-'

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

    void TH15MissBombRecorder::OnRunEnd()
    {
        // 清 boss 状态，避免残留野指针。
        mBossPtr = 0;
        mCurStageName[0] = '\0';

        if (!mLineOpen)
            return;
        mLineOpen = false;
        if (mLineLen == 0)
            return;

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
        WriteFile(hFile, mLineBuf, (DWORD)mLineLen, &written, nullptr);
        const char* newline = "\r\n";
        WriteFile(hFile, newline, 2, &written, nullptr);

        CloseHandle(hFile);

        mLineLen = 0;
        mLineBuf[0] = '\0';
    }

}
}

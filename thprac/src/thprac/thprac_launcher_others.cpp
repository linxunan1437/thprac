#include "thprac_launcher_others.h"
#include "thprac_launcher_utils.h"
#include "thprac_launcher_games.h"
#include "thprac_launcher_main.h"
#include "thprac_launcher_cfg.h"
#include "thprac_launcher_games_def.h"
#include "thprac_load_exe.h"
#include "thprac_main.h"
#include "thprac_gui_locale.h"
#include "thprac_utils.h"
#include "utils/utils.h"
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <numbers>
#include <fstream>
#include <sstream>
#include <format>

namespace THPrac {

bool CheckIfAnyGame2() // = THPrac_main.cpp: CheckIfAnyGame()
{
    static const char* mutexStrA = nullptr;
    static const WCHAR* mutexStrW = nullptr;
    if (mutexStrA) { // less CPU
        auto resultA = OpenMutexA(SYNCHRONIZE, FALSE, mutexStrA);
        if (resultA) {
            CloseHandle(resultA);
            return true;
        }
    }
    if (mutexStrW) {
        auto resultW = OpenMutexW(SYNCHRONIZE, FALSE, mutexStrW);
        if (resultW) {
            CloseHandle(resultW);
            return true;
        }
    }
    for (auto& gamesig : gGameDefs) {
        if (gamesig.mutexStr) {
            auto result = OpenMutexA(SYNCHRONIZE, FALSE, gamesig.mutexStr);
            if (result) {
                mutexStrA = gamesig.mutexStr;
                CloseHandle(result);
                return true;
            }
        } else if (gamesig.mutexWStr) {
            auto result = OpenMutexW(SYNCHRONIZE, FALSE, gamesig.mutexWStr);
            if (result) {
                mutexStrW = gamesig.mutexWStr;
                CloseHandle(result);
                return true;
            }
        }
    }
    mutexStrA = nullptr;
    mutexStrW = nullptr;
    return false;
}

#pragma region GameSessionRecorder
// 按局计时：门控表与判据出处见 docs/gametime/gametime_thxx_flags.md。

static bool ReadFlagValue(HANDLE hProc, uintptr_t base, const THGameTimeFlag& flag, uint32_t& out)
{
    if (!flag.rva || (flag.bytes != 1 && flag.bytes != 2 && flag.bytes != 4))
        return false;
    uint32_t value = 0;
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(hProc, (void*)(base + flag.rva), &value, flag.bytes, &bytesRead) || bytesRead != flag.bytes)
        return false;
    out = value;
    return true;
}

static bool EvalRaw(const THGameTimeFlag& flag, uint32_t value)
{
    switch (flag.cmp) {
    case GateCmp::Eq:
        return value == flag.value;
    case GateCmp::Ne:
        return value != flag.value;
    case GateCmp::BitClear:
        return (value & flag.value) == 0;
    }
    return false;
}

static bool EvalFlag(HANDLE hProc, uintptr_t base, const THGameTimeFlag& flag)
{
    uint32_t value = 0;
    if (!ReadFlagValue(hProc, base, flag, value))
        return false;
    return EvalRaw(flag, value);
}

// 短路顺序固定 gamemode → pause → replay：停在菜单里每 tick 只花 1 次 ReadProcessMemory。
// 任一读失败即本 tick 不计时（保守，避免游戏崩溃瞬间刷时间）。
static bool SampleGate(HANDLE hProc, uintptr_t base, const THGameTimeGate* gate)
{
    if (!gate)
        return false;
    return EvalFlag(hProc, base, gate->gamemode)
        && EvalFlag(hProc, base, gate->pause)
        && EvalFlag(hProc, base, gate->replay);
}

// exe 名快筛：逻辑与 thprac_launcher_games.cpp 的 CheckProcessOmni 一致（那函数是 static，复用不了）。
static bool ExeNameLooksLikeTouhou(const wchar_t* name)
{
    if (!wcscmp(L"東方紅魔郷.exe", name) || !wcscmp(L"alcostg.exe", name) || !wcscmp(L"搶曽峠杺嫿.exe", name))
        return true;
    if (name[0] != L't' || name[1] != L'h')
        return false;
    return name[2] >= L'0' && name[2] <= L'9' && name[3] >= L'0' && name[3] <= L'9';
}

// 一局一行追加到 <数据目录>gametime_sessions.jsonl。会话进行中不写盘，天然低频。
static void AppendGameSessionLine(const char* gameId, const SYSTEMTIME& startTime, const SYSTEMTIME& endTime, int64_t playNs)
{
    auto dir = LauncherGetDataDir();
    if (dir.empty())
        return;

    char line[256];
    int len = _snprintf_s(line, _TRUNCATE,
        "{\"game\":\"%s\",\"start\":\"%04u-%02u-%02u %02u:%02u:%02u\","
        "\"end\":\"%04u-%02u-%02u %02u:%02u:%02u\",\"durationNs\":%lld}",
        gameId,
        (unsigned)startTime.wYear, (unsigned)startTime.wMonth, (unsigned)startTime.wDay,
        (unsigned)startTime.wHour, (unsigned)startTime.wMinute, (unsigned)startTime.wSecond,
        (unsigned)endTime.wYear, (unsigned)endTime.wMonth, (unsigned)endTime.wDay,
        (unsigned)endTime.wHour, (unsigned)endTime.wMinute, (unsigned)endTime.wSecond,
        (long long)playNs);
    if (len <= 0)
        return;

    auto path = dir + L"gametime_sessions.jsonl";
    auto hFile = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    SetFilePointer(hFile, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(hFile, line, (DWORD)len, &written, nullptr);
    WriteFile(hFile, "\r\n", 2, &written, nullptr);
    CloseHandle(hFile);
}

// ---- 读回：统计详情页的输入 ----
struct GameSessionEntry {
    uint16_t year, month, day;
    int64_t durationNs;
    char game[16]; // 与 gGameDefs 的 idStr 对齐
};

// 逐行解析上面那个写盘格式。game/start 按既定字段顺序取，durationNs 用查找定位
// （plan §1.6 预告 P2 会加 gate / closed，按顺序读会被加字段打断）。
static bool ParseGameSessionLine(const char* line, GameSessionEntry& entry)
{
    char game[16] = {};
    unsigned year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (sscanf_s(line, "{\"game\":\"%15[^\"]\",\"start\":\"%u-%u-%u %u:%u:%u\"",
            game, (unsigned)sizeof(game),
            &year, &month, &day, &hour, &minute, &second) != 7)
        return false;

    auto durKey = strstr(line, "\"durationNs\":");
    if (!durKey)
        return false;

    entry.year = (uint16_t)year;
    entry.month = (uint16_t)month;
    entry.day = (uint16_t)day;
    entry.durationNs = _strtoi64(durKey + 13, nullptr, 10);
    memcpy(entry.game, game, sizeof(entry.game));
    return true;
}

// 读不到文件（还没玩过 / 被删 / 数据目录拿不到）= 空列表，不是错误。
static void LoadGameSessionEntries(std::vector<GameSessionEntry>& entries)
{
    entries.clear();

    auto dir = LauncherGetDataDir();
    if (dir.empty())
        return;

    void* buffer = nullptr;
    size_t size = 0;
    auto path = dir + L"gametime_sessions.jsonl";
    if (!LoadJsonFile(path, buffer, size))
        return;

    const char* cur = (const char*)buffer;
    const char* end = cur + size;
    while (cur < end) {
        auto nl = (const char*)memchr(cur, '\n', (size_t)(end - cur));
        auto lineEnd = nl ? nl : end;
        size_t len = (size_t)(lineEnd - cur);
        while (len && (cur[len - 1] == '\r' || cur[len - 1] == ' '))
            len--;

        char line[256];
        if (len && len < sizeof(line)) {
            memcpy(line, cur, len);
            line[len] = 0;
            GameSessionEntry entry {};
            if (ParseGameSessionLine(line, entry))
                entries.push_back(entry);
        }

        cur = nl ? nl + 1 : end;
    }
    free(buffer);
}

// idStr -> 人类可读标题（gGameDefs 的 refStr）；认不出来就退回 idStr 本身。
static const char* GameSessionGameName(const char* idStr)
{
    for (auto& gameDef : gGameDefs) {
        if (!strcmp(gameDef.idStr, idStr))
            return S(gameDef.refStr);
    }
    return idStr;
}

// 只到秒："H:MM:SS"。比 GetTime_HHMMSS 的 ms/us/ns 三段更适合表格。
static std::string FormatGameTimeShort(int64_t ns)
{
    int64_t seconds = ns / 1000000000ll;
    return std::format("{}:{:0>2}:{:0>2}", seconds / 3600, (seconds / 60) % 60, seconds % 60);
}
#pragma endregion

class THDrawLuck {
    THDrawLuck() { memset(name, 0, sizeof(name)); }
    SINGLETON(THDrawLuck)

private:
    int cur_year;
    int cur_month;
    int cur_day;

    int draw_luck_day_count=0;
    int draw_luck_Y = 0;
    int draw_luck_M = 0;
    int draw_luck_D = 0;

    bool is_drawed = 0;
    int luck_value = -1;
    int better1 = -1;
    int better2 = -1;
    int better3 = -1;
    int betternot1 = -1;
    int betternot2 = -1;
    int betternot3 = -1;
    char name[20];
    char name_orig[20];

    void LoadSave()
    {
        if (!LauncherSettingGet("draw_day_count", draw_luck_day_count))
            draw_luck_day_count = 0;
        std::string time_m;
        if (!LauncherSettingGet("draw_day_max", time_m)) {
            draw_luck_Y = draw_luck_M = draw_luck_D = 0;
        } else{
            std::stringstream ss(time_m);
            ss >> draw_luck_Y >> draw_luck_M >> draw_luck_D;
        }
        std::string namestr;
        memset(name_orig, 0, sizeof(name_orig));
        memset(name, 0, sizeof(name));
        if (LauncherSettingGet("luck_name", namestr)) {
            for (int i = 0; i < ((int)sizeof(name)) - 1 && i < std::ssize(namestr); i++)
            {
                name[i] = namestr[i];
                name_orig[i] = namestr[i];
            }
        }
        std::string luck_desc;
        if (LauncherSettingGet("luck_desc", luck_desc)) {
            std::stringstream ss(luck_desc);
            ss >> better1 >> better2 >> better3 >> betternot1 >> betternot2 >> betternot3;
        }
        LauncherSettingGet("luck_value", luck_value);
    }

    void SaveSave()
    {
        LauncherSettingSet("draw_day_count", draw_luck_day_count);
        std::stringstream ss;
        ss << draw_luck_Y << " " << draw_luck_M << " "<< draw_luck_D;
        LauncherSettingSet("draw_day_max", ss.str());
        ss=std::stringstream();
        ss << better1 << " " << better2 << " " << better3 << " " << betternot1 << " " << betternot2 << " " << betternot3;
        LauncherSettingSet("luck_desc", ss.str());
        LauncherSettingSet("luck_name", std::string(name));
        LauncherSettingSet("luck_value", luck_value);
    }
    
    bool TestIsDrawed(bool update)
    {
        time_t now = time(0);
        tm* currentDate = localtime(&now);
        cur_year = 1900 + currentDate->tm_year;
        cur_month = 1 + currentDate->tm_mon;
        cur_day = currentDate->tm_mday;
        if (cur_year == draw_luck_Y && cur_month == draw_luck_M && cur_day == draw_luck_D) {
            if (luck_value >= 0 && better1 >= 0 && better2 >= 0 && betternot1 >= 0 && betternot2 >= 0 && better3>=0 && betternot3>=0){
                return true;
            }
        }
        if (update) {
            draw_luck_Y = cur_year;
            draw_luck_M = cur_month;
            draw_luck_D = cur_day;
        }
        return false;
    }
private:
public:
    void Init(){
        LoadSave();
    }
    void DrawTextScaled(const char* ch, float relPos, float scale, const ImVec4& col, float space1 = 0.0f, float space2 = 0.0f)
    {
        if (space1 != 0.0f)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + space1 * ImGui::GetTextLineHeight());
        ImScaleStart();
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        if (relPos != 0.0f) {
            auto wndSize = ImGui::GetWindowSize();
            auto textSz = ImGui::CalcTextSize(ch);
            ImGui::SetCursorPosX(wndSize.x * relPos - textSz.x * 0.5f);
        }
        ImGui::TextUnformatted(ch);
        ImGui::PopStyleColor();
        ImScaleEnd(scale, scale);
        if (space2 != 0.0f)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + space2 * ImGui::GetTextLineHeight());
    }
    void DrawTextScaled2(const char* ch1, const char* ch2, float relPos, float scale, const ImVec4& col, float space1=0.0f, float space2 = 0.0f)
    {
        if (space1 != 0.0f)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + space1 * ImGui::GetTextLineHeight());
        ImScaleStart();
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        if (relPos != 0.0f) {
            auto wndSize = ImGui::GetWindowSize();
            auto textSz1 = ImGui::CalcTextSize(ch1);
            auto textSz2 = ImGui::CalcTextSize(ch2);
            auto textSz3 = ImGui::CalcTextSize("  ");
            ImGui::SetCursorPosX(wndSize.x * relPos - textSz1.x * 0.5f - textSz2.x * 0.5f - textSz3.x * 0.5f);
        }
        ImGui::Text("%s: %s", ch1,ch2);
        ImGui::PopStyleColor();
        ImScaleEnd(scale, scale);
        if (space2 != 0.0f)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + space2 * ImGui::GetTextLineHeight());
    }
    void Gui()
    {
        if (Gui::LocaleGet()==0) {// chinese special
            auto charfilter = [](ImGuiInputTextCallbackData* data) -> int {
                static std::string allowed = "+-=.,!?@:;[]()_/{}|~^#$%&* ";
                if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
                    if ((data->EventChar >= '0' && data->EventChar <= '9') || 
                        (data->EventChar >= 'A' && data->EventChar <= 'Z')|| 
                        (data->EventChar >= 'a' && data->EventChar <= 'z')
                    ){
                        return 0;
                    } else {
                        for (auto ch : allowed)
                            if (data->EventChar == ch)
                                return 0;
                        data->EventChar = 0;
                        return 1;
                    }
                }
                return 0;
            };
            is_drawed = TestIsDrawed(false);
            static char chs[255];

            sprintf_s(chs, S(THPRAC_OTHER_TODAY), cur_year, cur_month, cur_day);
            DrawTextScaled(chs, 0.5f, 2.0f, { 1, 1, 1, 1 }, 0.5f, 0.5f);

            if (!is_drawed) {
                ImGui::InputText(S(THPRAC_OTHER_NAME), name, sizeof(name) - 1, ImGuiInputTextFlags_::ImGuiInputTextFlags_CallbackCharFilter, charfilter);
                if (strnlen_s(name,sizeof(name))>0) {
                    auto wndSize = ImGui::GetWindowSize();
                    ImGui::SetCursorPosX(wndSize.x * (0.5f-0.15f));
                    if (ImGui::Button(S(THPRAC_OTHER_CLOCK_IN), { wndSize.x * 0.3f, 150.0f }))
                    {
                            is_drawed = TestIsDrawed(true);
                            int name_s = 0;
                            for (int i = 0; i < sizeof(name); i++) {
                                if (name[i] == 0)
                                    break;
                                name_s *= 114;
                                name_s ^= name[i] * name[i];
                            }
                            std::default_random_engine rand((draw_luck_D * draw_luck_D * 114) ^ (draw_luck_Y * 514) ^ (draw_luck_M * 1919) ^ (name_s * 810));

                            // idk why the dev tool made arr 1 element larger
                            std::uniform_int_distribution<int32_t> rand_luck_value(0, ARRAYSIZE(LUCK_RANGE) - 2);
                            std::vector<int> ld1, ld2;
                            for (int i = 0; i <= ARRAYSIZE(LUCK_DESC_1) - 2; i++)
                                ld1.push_back(i);
                            for (int i = 0; i <= ARRAYSIZE(LUCK_DESC_2) - 2; i++)
                                ld2.push_back(i);
                            std::shuffle(ld1.begin(), ld1.end(), rand);
                            std::shuffle(ld2.begin(), ld2.end(), rand);

                            luck_value = rand_luck_value(rand);
                            better1 = ld1[0];
                            betternot1 = ld1[1];
                            better3 = ld1[2];
                            betternot3 = ld1[3];

                            better2 = ld2[0];
                            betternot2 = ld2[1];
                            if (strcmp(name_orig, name) != 0)
                                draw_luck_day_count = 0;
                            draw_luck_day_count++;
                            SaveSave();
                    }
                }
            }
            if (is_drawed)
            {
                DrawTextScaled(std::format("{}{}",name,S(THPRAC_OTHER_DRAW_LUCK)).c_str(), 0.5f, 1.0f, { 1, 0.7, 0.7, 1 },0.0f,1.0f);
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(0.0f, 1.0f - (luck_value / (float)(ARRAYSIZE(LUCK_RANGE) - 2)), 1.0f - 0.6f*(luck_value / (float)(ARRAYSIZE(LUCK_RANGE) - 2)), r, g, b);
                if ((luck_value == ARRAYSIZE(LUCK_RANGE) - 2) || (luck_value == ARRAYSIZE(LUCK_RANGE) - 3))// xiong, da xiong
                {
                    int name_s = 0;
                    for (int i = 0; i < sizeof(name); i++) {
                        if (name[i] == 0)
                            break;
                        name_s *= 114;
                        name_s ^= name[i] * name[i];
                    }
                    std::default_random_engine rand((draw_luck_D * draw_luck_D * 114) ^ (draw_luck_Y * 514) ^ (draw_luck_M * 1919) ^ (name_s * 810));
                    std::uniform_int_distribution<int32_t> rand_maggot(1, 10);
                    if (rand_maggot(rand) <= 1)
                    {
                        if (luck_value == ARRAYSIZE(LUCK_RANGE) - 2)
                        {
                            DrawTextScaled(S(THPRAC_OTHER_LUCK_Q2), 0.5f, 2.0f, { r, g, b, 1 }, 0.125f);
                        }
                        else
                        {
                            DrawTextScaled(S(THPRAC_OTHER_LUCK_Q1), 0.5f, 2.0f, { r, g, b, 1 }, 0.125f);
                        }
                    } else {
                        DrawTextScaled(S(LUCK_RANGE[luck_value]), 0.5f, 2.0f, { r, g, b, 1 }, 0.125f);
                    }
                }
                else
                {
                    DrawTextScaled(S(LUCK_RANGE[luck_value]), 0.5f, 2.0f, { r, g, b, 1 }, 0.125f);
                }
                ImGui::NewLine();
                if (luck_value == 0)
                {
                    DrawTextScaled(S(THPRAC_OTHER_LUCK_BETTER_ALL), 0.5f, 1.25f, { r, g, b, 1 },0.25f,0.0f);
                }else if (luck_value == ARRAYSIZE(LUCK_RANGE) - 2){
                    DrawTextScaled(S(THPRAC_OTHER_LUCK_BETTER_NOT_ALL), 0.5f, 1.25f, { r, g, b, 1 }, 0.25f,0.0f);
                } else {
                    float r1, g1, b1, r2, g2, b2;
                    ImGui::Columns(2);
                    ImGui::ColorConvertHSVtoRGB(0.0f, 1.0f, 1.0f, r1, g1, b1);
                    ImGui::ColorConvertHSVtoRGB(0.0f, 0.0f, 0.95f, r2, g2, b2);
                    float sz_text1 = 1.25f;
                    float sz_text2 = 1.0f;
                    float space1 = 0.5f;
                    float space2 = 0.1f;
                    DrawTextScaled2(S(THPRAC_OTHER_LUCK_BETTER), S(LUCK_DESC_1[better1]), 0.25f, sz_text1, { r1, g1, b1, 1 }, space1);
                    ImGui::NextColumn();
                    DrawTextScaled2(S(THPRAC_OTHER_LUCK_BETTER_NOT), S(LUCK_DESC_1[betternot1]), 0.75f, sz_text1, { r2, g2, b2, 1 }, space1);
                    ImGui::NextColumn();

                    DrawTextScaled(S(LUCK_DESC_1A[better1]), 0.25f, sz_text2, { r1, g1, b1, 1 },0.0f, space2);
                    ImGui::NextColumn();
                    DrawTextScaled(S(LUCK_DESC_1B[betternot1]), 0.75f, sz_text2, { r2, g2, b2, 1 }, 0.0f, space2);
                    ImGui::NextColumn();

                    DrawTextScaled2(S(THPRAC_OTHER_LUCK_BETTER), S(LUCK_DESC_1[better3]), 0.25f, sz_text1, { r1, g1, b1, 1 }, space1);
                    ImGui::NextColumn();
                    DrawTextScaled2(S(THPRAC_OTHER_LUCK_BETTER_NOT), S(LUCK_DESC_1[betternot3]), 0.75f, sz_text1, { r2, g2, b2, 1 }, space1);
                    ImGui::NextColumn();

                    DrawTextScaled(S(LUCK_DESC_1A[better3]), 0.25f, sz_text2, { r1, g1, b1, 1 }, 0.0f, space2);
                    ImGui::NextColumn();
                    DrawTextScaled(S(LUCK_DESC_1B[betternot3]), 0.75f, sz_text2, { r2, g2, b2, 1 }, 0.0f, space2);
                    ImGui::NextColumn();

                    DrawTextScaled2(S(THPRAC_OTHER_LUCK_BETTER), S(LUCK_DESC_2[better2]), 0.25f, sz_text1, { r1, g1, b1, 1 }, space1);
                    ImGui::NextColumn();
                    DrawTextScaled2(S(THPRAC_OTHER_LUCK_BETTER_NOT), S(LUCK_DESC_2[betternot2]), 0.75f, sz_text1, { r2, g2, b2, 1 }, space1);
                    ImGui::NextColumn();

                    DrawTextScaled(S(LUCK_DESC_2A[better2]), 0.25f, sz_text2, { r1, g1, b1, 1 }, 0.0f, space2);
                    ImGui::NextColumn();
                    DrawTextScaled(S(LUCK_DESC_2B[betternot2]), 0.75f, sz_text2, { r2, g2, b2, 1 }, 0.0f, space2);
                    ImGui::NextColumn();
                    ImGui::Columns(1);
                }
                sprintf_s(chs, S(THPRAC_OTHER_CONTINUE), draw_luck_day_count);
                DrawTextScaled(chs, 0.5f, 1.0f, { 1, 1, 1, 1 }, 1.0f, 0.6f);
            }
            ImGui::Separator();
        } else {

        }
    }
};

class THGameTimeRecorder {
    THGameTimeRecorder() { }
    SINGLETON(THGameTimeRecorder)

private:
    int64_t mGameTime_ns = -1;
    int64_t mGameTimeCur_ns = -1;
    int64_t mGameTimeTestGameOpen_ns = -1;
    int64_t mGameTimeTooLongSE_ns = -1;
    bool mUpdateGameTime = true;

    int clock_id;

    float mTooLongGamePlay_hour = 3.0f;
    bool mEnableRecordGameTime = false;
    std::thread mUpdateThread;

    // 按局计时（与上面的"进程存活即计时"累加器完全独立，互不影响）
    HANDLE mSessionProc = nullptr;      // 游戏进程句柄，含 SYNCHRONIZE
    THGameSig* mSessionSig = nullptr;   // 当前绑定作品（指向 gGameDefs 静态条目）
    uintptr_t mSessionBase = 0;
    DWORD mSessionPid = 0;
    SYSTEMTIME mSessionStart {};
    int64_t mSessionPlayNs = 0;
    bool mBindTried = false;            // 每个"有游戏"周期只枚举一次，避免非计时作品被反复扫描

private:
    // 只在"探针说有游戏 且 尚未绑定"时枚举一次，稳态零成本。
    // 绑定是一次性的：认不出来就整局不记（宁可不记录，也不要一条起点错误的记录）。
    void TryBindSession()
    {
        auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return;

        PROCESSENTRY32W entry {};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (!ExeNameLooksLikeTouhou(entry.szExeFile))
                    continue;
                if (BindProcess(entry.th32ProcessID))
                    break;
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    // OpenProcess → 取基址 → PE 指纹比对 gGameDefs。只认挂了 gametimeGate 的条目，
    // 于是"该作是否参与按局计时"和"是否记录"是同一件事。
    bool BindProcess(DWORD pid)
    {
        auto hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid);
        if (!hProc)
            return false;

        auto base = GetGameModuleBase(hProc);
        if (!base) {
            CloseHandle(hProc);
            return false;
        }

        ExeSig sig {};
        if (GetExeInfoEx((uintptr_t)hProc, base, sig)) {
            for (auto& gameDef : gGameDefs) {
                if (!gameDef.gametimeGate)
                    continue;
                if (gameDef.exeSig.textSize != sig.textSize || gameDef.exeSig.timeStamp != sig.timeStamp)
                    continue;

                mSessionProc = hProc;
                mSessionSig = &gameDef;
                mSessionBase = base;
                mSessionPid = pid;
                mSessionPlayNs = 0;
                GetLocalTime(&mSessionStart);
                return true;
            }
        }

        CloseHandle(hProc);
        return false;
    }
    // 进程已退出 / launcher 被关：收尾并把本局落盘。
    void CloseSession()
    {
        if (!mSessionProc)
            return;

        SYSTEMTIME endTime {};
        GetLocalTime(&endTime);
        auto playNs = mSessionPlayNs;
        auto gameId = mSessionSig->idStr;

        CloseHandle(mSessionProc);
        mSessionProc = nullptr;
        mSessionSig = nullptr;
        mSessionBase = 0;
        mSessionPid = 0;
        mSessionPlayNs = 0;

        if (playNs > 0)
            AppendGameSessionLine(gameId, mSessionStart, endTime, playNs);
    }

public:
    static void UpdateGameTime(){
        THGameTimeRecorder& thiz = THGameTimeRecorder::singleton();
        while (thiz.mUpdateGameTime) {
            static bool is_game_open = false;
            Sleep(16);
            if (thiz.mGameTimeTestGameOpen_ns > 1000000000) { // test every second
                is_game_open = CheckIfAnyGame2();
                thiz.mGameTimeTestGameOpen_ns = 0;
                if (is_game_open) {
                    if (!thiz.mSessionProc && !thiz.mBindTried) {
                        thiz.mBindTried = true;
                        thiz.TryBindSession();
                    }
                } else {
                    thiz.mBindTried = false;
                }
            }
            double passed_time = ResetClock(thiz.clock_id)*1e9;

            if (is_game_open) {
                thiz.mGameTime_ns += static_cast<int64_t>(passed_time);
                thiz.mGameTimeCur_ns += static_cast<int64_t>(passed_time);
            }
            if (thiz.mSessionProc) {
                if (WaitForSingleObject(thiz.mSessionProc, 0) == WAIT_OBJECT_0) {
                    thiz.CloseSession(); // 正常退出 / 强杀 / 崩溃，进程对象必然 signal
                } else if (SampleGate(thiz.mSessionProc, thiz.mSessionBase, thiz.mSessionSig->gametimeGate)) {
                    thiz.mSessionPlayNs += static_cast<int64_t>(passed_time);
                }
            }
            thiz.mGameTimeTestGameOpen_ns += static_cast<int64_t>(passed_time);
            thiz.mGameTimeTooLongSE_ns += static_cast<int64_t>(passed_time);
        }
        thiz.CloseSession(); // launcher 先被关：仍写一行，end = 关闭时刻
        LauncherSetGameTime(thiz.mGameTime_ns);
    }
    bool IsEnabled()
    {
        return mEnableRecordGameTime;
    }
    void UpdateGameTimeRecord()
    {
        LauncherSettingGet("gameTimeTooLong_Time", mTooLongGamePlay_hour);
    }
    void StartGameTimeRecord()
    {
        
        LauncherSettingGet("recordGameTime", mEnableRecordGameTime);
        if (mEnableRecordGameTime) { // game counter
            mGameTime_ns = LauncherGetGameTime();
            if (mGameTime_ns < 0)
                mGameTime_ns = 0;
            mGameTimeCur_ns = 0;
            mGameTimeTestGameOpen_ns = 0;
            mGameTimeTooLongSE_ns = 0;
            mUpdateGameTime = true;

            clock_id = SetUpClock();
            mUpdateThread = std::thread(UpdateGameTime);
        }
    }
    void StopGameTimeRecord()
    {
        if (mEnableRecordGameTime) {
            mUpdateGameTime = false;
            mUpdateThread.join();
        }
    }
    
    bool Gui()
    {
        if (mEnableRecordGameTime) {
            ImGui::Text(S(THPRAC_GAME_TIME_TOTAL));
            ImGui::TextWrapped(GetTime_HHMMSS(mGameTime_ns > 0 ? mGameTime_ns : 0).c_str());
            ImGui::TextWrapped(GetTime_YYMMDD_HHMMSS(mGameTime_ns > 0 ? mGameTime_ns : 0).c_str());
            ImGui::TextWrapped("(%lld ns)", mGameTime_ns > 0 ? mGameTime_ns : 0);

            ImGui::NewLine();
            ImGui::Separator();

            float r, g, b;
            int gametimecur_hour = (mGameTimeCur_ns / (1000000000ll * 3600ll)) % 24;
            ImGui::ColorConvertHSVtoRGB(gametimecur_hour / 24.0f, 1, 1, r, g, b);
            ImGui::PushStyleColor(ImGuiCol_Text, { r, g, b, 1 });
            ImGui::Text(S(THPRAC_GAME_TIME_CURRENT));
            ImGui::PopStyleColor();

            int gametimecur_minute = (mGameTimeCur_ns / (1000000000ll * 60ll)) % 60;
            ImGui::ColorConvertHSVtoRGB(gametimecur_minute / 60.0f, 1, 1, r, g, b);
            ImGui::PushStyleColor(ImGuiCol_Text, { r, g, b, 1 });
            ImGui::TextWrapped(GetTime_HHMMSS(mGameTimeCur_ns > 0 ? mGameTimeCur_ns : 0).c_str());
            ImGui::PopStyleColor();

            int gametimecur_sec = (mGameTimeCur_ns / (1000000000ll)) % 60;
            ImGui::ColorConvertHSVtoRGB(gametimecur_sec / 60.0f, 1, 1, r, g, b);
            ImGui::PushStyleColor(ImGuiCol_Text, { r, g, b, 1 });
            ImGui::TextWrapped(GetTime_YYMMDD_HHMMSS(mGameTimeCur_ns > 0 ? mGameTimeCur_ns : 0).c_str());
            ImGui::PopStyleColor();

            int gametimecur_frame = (mGameTimeCur_ns / (16666666ll)) % 60;
            ImGui::ColorConvertHSVtoRGB(gametimecur_frame / 60.0f, 1, 1, r, g, b);
            ImGui::PushStyleColor(ImGuiCol_Text, { r, g, b, 1 });
            ImGui::TextWrapped("(%lld ns)", mGameTimeCur_ns);
            ImGui::PopStyleColor();
            if (ImGui::Button(S(THPRAC_GAME_TIME_RESET)))
            {
                mGameTimeCur_ns = 0;
            }
            ImGui::SameLine();
            bool showDetail = ImGui::Button(S(THPRAC_GAME_TIME_DETAIL));
            ImGui::NewLine();
            ImGui::Separator();

            // 按局计时调试读数（打样期验证门控 RVA 用；只用既有 locale 词条，不新增文案）
            auto* sessSig = mSessionSig;
            auto sessProc = mSessionProc;
            auto sessBase = mSessionBase;
            ImGui::Text("session: %s", sessSig ? sessSig->idStr : "-");
            if (sessSig && sessProc) {
                ImGui::Text("pid %u  base 0x%08X  play %.3fs", (unsigned)mSessionPid, (unsigned)sessBase, mSessionPlayNs / 1e9);
                const auto* gate = sessSig->gametimeGate;
                const THGameTimeFlag* flags[3] = { &gate->gamemode, &gate->pause, &gate->replay };
                const char* flagNames[3] = { "gamemode", "pause", "replay" };
                bool evaluated = true;
                for (int i = 0; i < 3 && evaluated; i++) {
                    uint32_t v = 0;
                    if (!ReadFlagValue(sessProc, sessBase, *flags[i], v)) {
                        ImGui::Text("%s: read failed (rva 0x%X)", flagNames[i], (unsigned)flags[i]->rva);
                        break;
                    }
                    evaluated = EvalRaw(*flags[i], v);
                    ImGui::Text("%s: 0x%X -> %s", flagNames[i], (unsigned)v, evaluated ? "ok" : "blocked");
                }
            }

            if (mGameTimeCur_ns >= 1000000000ll * 3600 * (double)mTooLongGamePlay_hour) {
                auto y_orig=ImGui::GetCursorPosY();
                ImGui::SetCursorPosY(ImGui::GetWindowHeight()*0.5f);
                static float h = 0.0f, vt = 0.0f, angle = 0.0f;
                h += 0.02f;
                if (h >= 1.0f)
                    h = 0.0f;
                vt += 0.01f;
                angle += 0.1f;
                ImScaleStart();
                ImRotateStart();
                ImGui::NewLine();
                ImGui::ColorConvertHSVtoRGB(h, sinf(vt) * 0.3f + 0.5f, 1.0f, r, g, b);
                ImGui::PushStyleColor(ImGuiCol_Text, { r, g, b, 1 });
                auto wndSize = ImGui::GetWindowSize();
                auto textSz = ImGui::CalcTextSize(S(THPRAC_GAME_TIME_LONG));
                ImGui::SetCursorPosX(wndSize.x * 0.5f - textSz.x * 0.5f);
                ImGui::TextUnformatted(S(THPRAC_GAME_TIME_LONG));
                ImGui::PopStyleColor();
                constexpr auto HALF_PI = static_cast<float>(std::numbers::pi / 2.0);
                ImRotateEnd(HALF_PI - sinf(-1.23f * angle + HALF_PI / 2.0f) * 0.12f);
                ImScaleEnd(2.3f + sinf(angle) * 0.8f, 2.3f + cosf(angle) * 0.8f);
                ImGui::SetCursorPosY(y_orig);
            }
            return showDetail;
        } else {
            ImGui::Text(S(THPRAC_ENABLE_GAMETIME_RECORD));
            return false;
        }
    }
};

class THOthersGui {
private:
    THOthersGui()
    {
        mGuiUpdFunc = [&]() { GuiMain(); };
    }
    SINGLETON(THOthersGui)

public:
    void GuiUpdate(){
        mGuiUpdFunc();
    }

private:
    void GuiMain(){
        THDrawLuck::singleton().Gui();
        if (THGameTimeRecorder::singleton().Gui()) {
            // 进详情页：读一次盘 + 复位筛选（默认本月 / 全部游戏）
            LoadGameSessionEntries(mSessionEntries);
            mSessionGameIds.clear();
            for (auto& gameDef : gGameDefs) { // 按 gGameDefs 顺序建列表，选项次序稳定
                for (auto& entry : mSessionEntries) {
                    if (!strcmp(entry.game, gameDef.idStr)) {
                        mSessionGameIds.emplace_back(gameDef.idStr);
                        break;
                    }
                }
            }
            mSessionGameFilter = -1;
            SYSTEMTIME now {};
            GetLocalTime(&now);
            mSessionYear = now.wYear;
            mSessionMonth = now.wMonth;
            mSessionLastMonth = now.wMonth; // 时间范围下拉只到"本年度至今"
            mGuiUpdFunc = [&]() { GuiGameTimeDetail(); };
        }
    }

    void GuiGameTimeDetail(){
        if (ImGui::Button(S(THPRAC_BACK))) {
            mGuiUpdFunc = [&]() { GuiMain(); };
            return;
        }
        ImGui::SameLine();
        GuiCenteredText(S(THPRAC_GAME_TIME_DETAIL));
        ImGui::Separator();

        // 两个筛选下拉：BeginCombo 的预览就是当前选中值，不需要可见 label
        const char* gamePreview = (mSessionGameFilter < 0)
            ? S(THPRAC_GAME_TIME_ALL)
            : GameSessionGameName(mSessionGameIds[mSessionGameFilter].c_str());
        if (ImGui::BeginCombo("##gametime_game", gamePreview)) {
            if (ImGui::Selectable(S(THPRAC_GAME_TIME_ALL), mSessionGameFilter < 0))
                mSessionGameFilter = -1;
            if (mSessionGameFilter < 0)
                ImGui::SetItemDefaultFocus();
            for (size_t i = 0; i < mSessionGameIds.size(); i++) {
                bool selected = (mSessionGameFilter == (int)i);
                if (ImGui::Selectable(GameSessionGameName(mSessionGameIds[i].c_str()), selected))
                    mSessionGameFilter = (int)i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        // 时间范围：本年 1 月..当月，纯数字标签（免文案）
        char monthPreview[16];
        sprintf_s(monthPreview, "%04d-%02d", mSessionYear, mSessionMonth);
        if (ImGui::BeginCombo("##gametime_month", monthPreview)) {
            for (int month = 1; month <= mSessionLastMonth; month++) {
                char label[16];
                sprintf_s(label, "%04d-%02d", mSessionYear, month);
                bool selected = (month == mSessionMonth);
                if (ImGui::Selectable(label, selected))
                    mSessionMonth = month;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        int64_t dayTotal[32] = {};
        int64_t total = 0;
        for (auto& entry : mSessionEntries) {
            if (entry.year != mSessionYear || entry.month != mSessionMonth || entry.day < 1 || entry.day > 31)
                continue;
            if (mSessionGameFilter >= 0 && strcmp(entry.game, mSessionGameIds[mSessionGameFilter].c_str()))
                continue;
            dayTotal[entry.day] += entry.durationNs;
            total += entry.durationNs;
        }

        ImGui::NewLine();
        if (ImGui::BeginTable("##gametime_days", 2,
                ImGuiTableFlags_::ImGuiTableFlags_Borders | ImGuiTableFlags_::ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn(S(THPRAC_GAME_TIME_DATE));
            ImGui::TableSetupColumn(S(THPRAC_GAME_TIME_DURATION));
            ImGui::TableHeadersRow();
            for (int day = 1; day <= 31; day++) {
                if (!dayTotal[day])
                    continue; // 只列有记录的天
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%04d-%02d-%02d", mSessionYear, mSessionMonth, day);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(FormatGameTimeShort(dayTotal[day]).c_str());
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(S(THPRAC_GAME_TIME_SUM));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(FormatGameTimeShort(total).c_str());
            ImGui::EndTable();
        }
    }

    std::function<void(void)> mGuiUpdFunc = []() {};
    std::vector<GameSessionEntry> mSessionEntries;
    std::vector<std::string> mSessionGameIds; // 数据里出现过的游戏
    int mSessionGameFilter = -1;              // -1 = 全部，否则索引进 mSessionGameIds
    int mSessionYear = 0;
    int mSessionMonth = 0;
    int mSessionLastMonth = 0;
};
bool THPrac::LauncherOthersGuiUpd()
{
    THOthersGui::singleton().GuiUpdate();
    return true;
}

void LauncherOthersInit()
{
    THGameTimeRecorder::singleton().StartGameTimeRecord();
    THDrawLuck::singleton().Init();
}

void LauncherOthersUpdate()
{
    THGameTimeRecorder::singleton().UpdateGameTimeRecord();
}

void LauncherOthersDestroy()
{
    THGameTimeRecorder::singleton().StopGameTimeRecord();
}

}
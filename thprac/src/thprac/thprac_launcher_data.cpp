#include "thprac_launcher_data.h"
#include "thprac_launcher_cfg.h"
#include "thprac_launcher_games.h"
#include "thprac_launcher_games_def.h"
#include "thprac_gui_locale.h"
#include "..\3rdParties\rapidcsv\rapidcsv.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <imgui.h>
#pragma warning(push)
#pragma warning(disable : 26451)
#pragma warning(disable : 26495)
#pragma warning(disable : 33010)
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#pragma warning(pop)

namespace THPrac {

// Checkpoint tables are compiled into the game translation units (see
// thprac_thXX_checkpoints_gen.cpp). The launcher reads the same verbatim CSV
// strings the in-game logger uses, so the two can never disagree.
namespace TH10 { extern const char kTh10CheckpointsCsv[]; }
namespace TH11 { extern const char kTh11CheckpointsCsv[]; }
namespace TH12 { extern const char kTh12CheckpointsCsv[]; }
namespace TH13 { extern const char kTh13CheckpointsCsv[]; }
namespace TH14 { extern const char kTh14CheckpointsCsv[]; }
namespace TH15 { extern const char kTh15CheckpointsCsv[]; }
namespace TH16 { extern const char kTh16CheckpointsCsv[]; }
namespace TH17 { extern const char kTh17CheckpointsCsv[]; }
namespace TH18 { extern const char kTh18CheckpointsCsv[]; }

namespace {


using SysDays = std::chrono::sys_days;

// ---- 练习量口径 ------------------------------------------------------------
// total > 0（单章节合并行 / 首条）优先取 total；否则取 checkpointCount。
// 记录含 interrupt（未通关退出）时 checkpointCount 再减 1，下限 0；多个
// interrupt 也只减一次（一份记录只对应一次退出）。
static int64_t PracticeAmount(int checkpointCount, int total, bool hasInterrupt)
{
    if (total > 0)
        return total;
    int64_t amount = (int64_t)checkpointCount - (hasInterrupt ? 1 : 0);
    return amount > 0 ? amount : 0;
}

// ---- 日期 ----------------------------------------------------------------
static bool IsLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int DaysInMonth(int year, int month)
{
    static constexpr int kDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && IsLeapYear(year))
        return 29;
    return kDays[month - 1];
}

static bool ParseYmdPrefix(std::string_view text, int& year, int& month, int& day)
{
    if (text.size() < 10)
        return false;
    for (int i : { 0, 1, 2, 3, 5, 6, 8, 9 }) {
        if (text[i] < '0' || text[i] > '9')
            return false;
    }
    if (text[4] != '-' || text[7] != '-')
        return false;
    year = (text[0] - '0') * 1000 + (text[1] - '0') * 100 + (text[2] - '0') * 10 + (text[3] - '0');
    month = (text[5] - '0') * 10 + (text[6] - '0');
    day = (text[8] - '0') * 10 + (text[9] - '0');
    return year >= 1 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 && day <= DaysInMonth(year, month);
}

static int64_t DateKey(int year, int month, int day)
{
    SysDays date { std::chrono::year { year } / std::chrono::month { (unsigned)month } / std::chrono::day { (unsigned)day } };
    return date.time_since_epoch().count();
}

static int64_t MondayKey(int64_t dayKey)
{
    SysDays date { std::chrono::days { dayKey } };
    int isoWeekday = (int)std::chrono::weekday { date }.iso_encoding(); // 1 = Monday
    return dayKey - (isoWeekday - 1);
}

static std::string FormatDate(int64_t dayKey)
{
    SysDays date { std::chrono::days { dayKey } };
    std::chrono::year_month_day ymd { date };
    return std::format("{:04}-{:02}-{:02}", (int)ymd.year(), (unsigned)ymd.month(), (unsigned)ymd.day());
}

static std::string FormatMonthDay(int64_t dayKey)
{
    return FormatDate(dayKey).substr(5);
}

static std::string FormatWeekRange(int64_t mondayKey)
{
    return FormatDate(mondayKey) + "~" + FormatDate(mondayKey + 6).substr(5);
}

static std::string FormatMonthLabel(const std::string& month)
{
    if (month.size() != 6)
        return month;
    return month.substr(0, 4) + "-" + month.substr(4, 2);
}

static std::vector<int64_t> MonthWeekKeys(const std::string& month)
{
    std::vector<int64_t> keys;
    if (month.size() != 6)
        return keys;
    int year = (month[0] - '0') * 1000 + (month[1] - '0') * 100 + (month[2] - '0') * 10 + (month[3] - '0');
    int monthValue = (month[4] - '0') * 10 + (month[5] - '0');
    if (monthValue < 1 || monthValue > 12)
        return keys;

    std::chrono::year_month_day firstYmd { std::chrono::year { year }, std::chrono::month { (unsigned)monthValue }, std::chrono::day { 1 } };
    int nextYear = (monthValue == 12) ? year + 1 : year;
    int nextMonth = (monthValue == 12) ? 1 : monthValue + 1;
    SysDays first { firstYmd };
    SysDays last { std::chrono::year_month_day { std::chrono::year { nextYear }, std::chrono::month { (unsigned)nextMonth }, std::chrono::day { 1 } } };
    last -= std::chrono::days { 1 };
    int64_t firstMonday = MondayKey(first.time_since_epoch().count());
    int64_t lastMonday = MondayKey(last.time_since_epoch().count());
    for (int64_t key = firstMonday; key <= lastMonday; key += 7)
        keys.push_back(key);
    return keys;
}

static const char* WeekdayName(int isoWeekday)
{
    static constexpr th_glossary_t kNames[7] = {
        THPRAC_DATA_WEEKDAY_MON,
        THPRAC_DATA_WEEKDAY_TUE,
        THPRAC_DATA_WEEKDAY_WED,
        THPRAC_DATA_WEEKDAY_THU,
        THPRAC_DATA_WEEKDAY_FRI,
        THPRAC_DATA_WEEKDAY_SAT,
        THPRAC_DATA_WEEKDAY_SUN,
    };
    return (isoWeekday >= 1 && isoWeekday <= 7) ? S(kNames[isoWeekday - 1]) : "";
}

static std::string FormatAmount(int64_t value)
{
    bool negative = value < 0;
    uint64_t magnitude = negative ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
    std::string text = std::to_string(magnitude);
    for (int i = (int)text.size() - 3; i > 0; i -= 3)
        text.insert(i, ",");
    return negative ? "-" + text : text;
}

// ---- 记录读取 ------------------------------------------------------------
struct PracticeRecord {
    std::string game;
    int year = 0;
    int month = 0;
    int day = 0;
    int64_t amount = 0;
};

static bool ParsePracticeRecord(const char* text, size_t length, PracticeRecord& record)
{
    rapidjson::Document doc;
    doc.Parse(text, length);
    if (doc.HasParseError() || !doc.IsObject())
        return false;

    auto game = doc.FindMember("game");
    auto start = doc.FindMember("start");
    auto checkpointCount = doc.FindMember("checkpointCount");
    if (game == doc.MemberEnd() || !game->value.IsString()
        || start == doc.MemberEnd() || !start->value.IsString()
        || checkpointCount == doc.MemberEnd() || !checkpointCount->value.IsInt())
        return false;

    int year = 0, month = 0, day = 0;
    if (!ParseYmdPrefix(std::string_view { start->value.GetString(), start->value.GetStringLength() }, year, month, day))
        return false;

    int total = 0;
    auto totalIt = doc.FindMember("total");
    if (totalIt != doc.MemberEnd() && totalIt->value.IsInt() && totalIt->value.GetInt() > 0)
        total = totalIt->value.GetInt();

    bool hasInterrupt = false;
    if (total <= 0) {
        auto events = doc.FindMember("events");
        if (events != doc.MemberEnd() && events->value.IsArray()) {
            for (auto& event : events->value.GetArray()) {
                if (!event.IsObject())
                    continue;
                auto type = event.FindMember("type");
                if (type != event.MemberEnd() && type->value.IsString()) {
                    std::string_view typeText { type->value.GetString(), type->value.GetStringLength() };
                    if (typeText == "interrupt") {
                        hasInterrupt = true;
                        break;
                    }
                }
            }
        }
    }

    record.game.assign(game->value.GetString(), game->value.GetStringLength());
    record.year = year;
    record.month = month;
    record.day = day;
    record.amount = PracticeAmount(checkpointCount->value.GetInt(), total, hasInterrupt);
    return true;
}

static bool ValidateMonthFileName(const wchar_t* name, std::string& month)
{
    constexpr size_t kPrefixLength = (sizeof(L"logger_sessions_") / sizeof(wchar_t)) - 1;
    for (size_t i = 0; i < kPrefixLength; i++) {
        if (name[i] != L"logger_sessions_"[i])
            return false;
    }
    for (size_t i = 0; i < 6; i++) {
        wchar_t digit = name[kPrefixLength + i];
        if (digit < L'0' || digit > L'9')
            return false;
    }
    if (std::wcscmp(name + kPrefixLength + 6, L".jsonl") != 0)
        return false;

    month.clear();
    for (size_t i = 0; i < 6; i++)
        month.push_back((char)name[kPrefixLength + i]);
    return true;
}

static std::wstring JoinDataDir(const std::wstring& dir)
{
    std::wstring path = dir;
    if (!path.empty() && path.back() != L'\\')
        path.push_back(L'\\');
    return path;
}

static void ListMonthFiles(const std::wstring& dataDir, std::vector<std::string>& months)
{
    months.clear();
    if (dataDir.empty())
        return;

    WIN32_FIND_DATAW findData {};
    HANDLE findHandle = FindFirstFileW((JoinDataDir(dataDir) + L"logger_sessions_*.jsonl").c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE)
        return;
    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        std::string month;
        if (ValidateMonthFileName(findData.cFileName, month))
            months.push_back(std::move(month));
    } while (FindNextFileW(findHandle, &findData));
    FindClose(findHandle);

    std::sort(months.begin(), months.end(), std::greater<>());
    months.erase(std::unique(months.begin(), months.end()), months.end());
}

template <typename Fn>
static bool ReadMonthLines(const std::wstring& dataDir, const std::string& month, Fn&& onLine)
{
    if (dataDir.empty() || month.size() != 6)
        return false;

    std::wstring fileName = L"logger_sessions_";
    for (char c : month)
        fileName.push_back((wchar_t)c);
    fileName += L".jsonl";

    std::wstring path = JoinDataDir(dataDir) + fileName;
    void* buffer = nullptr;
    size_t size = 0;
    if (!LoadJsonFile(path, buffer, size))
        return false;
    std::unique_ptr<void, decltype(&free)> bufferGuard(buffer, &free);

    const char* cursor = (const char*)buffer;
    const char* end = cursor + size;
    while (cursor < end) {
        const char* newline = (const char*)memchr(cursor, '\n', (size_t)(end - cursor));
        const char* lineEnd = newline ? newline : end;
        while (cursor < lineEnd && (*cursor == ' ' || *cursor == '\t'))
            ++cursor;
        size_t length = (size_t)(lineEnd - cursor);
        while (length && (cursor[length - 1] == '\r' || cursor[length - 1] == ' ' || cursor[length - 1] == '\t'))
            --length;

        if (length)
            onLine(cursor, length);

        if (!newline)
            break;
        cursor = newline + 1;
    }
    return true;
}

static bool ReadMonthRecords(const std::wstring& dataDir, const std::string& month,
    std::vector<PracticeRecord>& records, bool& readFailed)
{
    records.clear();
    readFailed = false;
    if (!ReadMonthLines(dataDir, month, [&](const char* text, size_t length) {
            PracticeRecord record;
            if (ParsePracticeRecord(text, length, record))
                records.push_back(std::move(record));
        })) {
        readFailed = true;
        return false;
    }
    return true;
}

// ---- 聚合 ----------------------------------------------------------------
struct DayAgg {
    int64_t total = 0;
    int count = 0;
};

struct PracticeDay {
    int64_t dayKey = 0;
    int64_t total = 0;
    int count = 0;
};

struct PracticeWeek {
    int64_t mondayKey = 0;
    std::string label;
    int64_t total = 0;
    std::vector<PracticeDay> days;
};

struct PracticeStats {
    int64_t total = 0;
    int recordCount = 0;
    int dayCount = 0;
    std::vector<PracticeWeek> weeks;
};

static void BuildGameList(const std::vector<PracticeRecord>& records, std::vector<std::string>& gameIds)
{
    std::set<std::string> present;
    for (auto& record : records)
        present.insert(record.game);

    gameIds.clear();
    for (auto& gameDef : gGameDefs) {
        auto it = present.find(gameDef.idStr);
        if (it != present.end()) {
            gameIds.push_back(*it);
            present.erase(it);
        }
    }
    gameIds.insert(gameIds.end(), present.begin(), present.end());
}

static void BuildStats(const std::vector<PracticeRecord>& records, const std::string& game,
    const std::string& month, PracticeStats& stats)
{
    stats = {};
    std::map<int64_t, DayAgg> days;
    for (auto& record : records) {
        if (!game.empty() && record.game != game)
            continue;
        ++stats.recordCount;
        stats.total += record.amount;

        int64_t dayKey = DateKey(record.year, record.month, record.day);
        auto& day = days[dayKey];
        day.total += record.amount;
        ++day.count;
    }

    stats.dayCount = (int)days.size();

    std::map<int64_t, int64_t> weekTotals;
    std::map<int64_t, std::vector<int64_t>> weekDays;
    for (auto& [dayKey, day] : days) {
        int64_t mondayKey = MondayKey(dayKey);
        weekTotals[mondayKey] += day.total;
        weekDays[mondayKey].push_back(dayKey);
    }

    std::set<int64_t> weekKeys;
    for (int64_t key : MonthWeekKeys(month))
        weekKeys.insert(key);
    for (auto& [mondayKey, total] : weekTotals)
        weekKeys.insert(mondayKey);

    for (int64_t mondayKey : weekKeys) {
        PracticeWeek week;
        week.mondayKey = mondayKey;
        week.label = FormatWeekRange(mondayKey);
        auto weekTotal = weekTotals.find(mondayKey);
        if (weekTotal != weekTotals.end())
            week.total = weekTotal->second;
        auto dayList = weekDays.find(mondayKey);
        if (dayList != weekDays.end()) {
            for (int64_t dayKey : dayList->second) {
                auto day = days.find(dayKey);
                week.days.push_back({ dayKey, day->second.total, day->second.count });
            }
        }
        stats.weeks.push_back(std::move(week));
    }
}

static const char* GameDisplayName(const char* idStr)
{
    for (auto& gameDef : gGameDefs) {
        if (!std::strcmp(gameDef.idStr, idStr))
            return S(gameDef.refStr);
    }
    return idStr;
}

static std::string CurrentMonthKey()
{
    SYSTEMTIME now {};
    GetLocalTime(&now);
    return std::format("{:04}{:02}", (unsigned)now.wYear, (unsigned)now.wMonth);
}

// 日练习量比例条：每 240 为一档，档内只画小数部分；>4 档封顶画满。
static void DrawPracticeRatioBar(ImVec2 pos, float width, float ratio)
{
    constexpr float kBarHeight = 5.0f;
    ImU32 background = 0;
    ImU32 foreground = 0;
    float fill = 0.0f;

    if (ratio <= 1.0f) {
        background = ImGui::GetColorU32(ImGuiCol_FrameBg);
        foreground = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
        fill = std::clamp(ratio, 0.0f, 1.0f);
    } else if (ratio <= 2.0f) {
        background = IM_COL32(196, 168, 40, 255);
        foreground = IM_COL32(90, 200, 90, 255);
        fill = ratio - 1.0f;
    } else if (ratio <= 3.0f) {
        background = IM_COL32(80, 170, 80, 255);
        foreground = IM_COL32(80, 140, 235, 255);
        fill = ratio - 2.0f;
    } else if (ratio <= 4.0f) {
        background = IM_COL32(70, 110, 200, 255);
        foreground = IM_COL32(240, 240, 240, 255);
        fill = ratio - 3.0f;
    } else {
        background = IM_COL32(70, 110, 200, 255);
        foreground = IM_COL32(240, 240, 240, 255);
        fill = 1.0f;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + kBarHeight), background, 2.5f);
    if (fill > 0.0f)
        drawList->AddRectFilled(pos, ImVec2(pos.x + width * fill, pos.y + kBarHeight), foreground, 2.5f);
}

// ---- 页面 ----------------------------------------------------------------
class PracticeTabGui {
public:
    void Gui();

private:
    void EnsureInitialized();
    void Refresh();
    void LoadSelectedMonth();
    void RebuildStats();
    void DrawSummary();
    void DrawFilters();
    void DrawWeeks();
    void DrawDayRow(const PracticeDay& day);

    bool mInitialized = false;
    std::wstring mDataDir;
    std::vector<std::string> mMonths;
    std::string mSelectedMonth;
    std::string mSelectedGame;
    std::vector<std::string> mGameIds;
    std::vector<PracticeRecord> mMonthRecords;
    PracticeStats mStats;
    bool mNoFiles = false;
    bool mReadFailed = false;
    bool mFallback = false;
};

PracticeTabGui gPracticeTab;

void PracticeTabGui::Gui()
{
    EnsureInitialized();

    if (ImGui::Button(S(THPRAC_DATA_REFRESH)))
        Refresh();
    ImGui::SameLine();
    if (!mSelectedMonth.empty())
        ImGui::TextDisabled("%s", FormatMonthLabel(mSelectedMonth).c_str());
    ImGui::Separator();

    if (mNoFiles) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_NO_FILE));
        return;
    }
    if (mReadFailed) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_READ_FAILED));
        return;
    }

    DrawSummary();
    DrawFilters();
    ImGui::Separator();

    if (mMonthRecords.empty()) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_PRACTICE_EMPTY_MONTH));
        return;
    }
    if (mStats.recordCount == 0) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_PRACTICE_EMPTY_FILTER));
        return;
    }

    ImGui::BeginChild("##practice_week_list", ImVec2(0.0f, 0.0f), false);
    DrawWeeks();
    ImGui::EndChild();
}

void PracticeTabGui::EnsureInitialized()
{
    if (mInitialized)
        return;
    mInitialized = true;
    Refresh();
}

void PracticeTabGui::Refresh()
{
    mDataDir = LauncherGetDataDir();
    ListMonthFiles(mDataDir, mMonths);
    mNoFiles = mMonths.empty();
    if (mNoFiles) {
        mSelectedMonth.clear();
        mMonthRecords.clear();
        mGameIds.clear();
        mStats = {};
        mReadFailed = false;
        mFallback = false;
        return;
    }

    std::string currentMonth = CurrentMonthKey();
    bool keepSelection = !mSelectedMonth.empty()
        && std::find(mMonths.begin(), mMonths.end(), mSelectedMonth) != mMonths.end();

    if (!keepSelection) {
        if (std::find(mMonths.begin(), mMonths.end(), currentMonth) != mMonths.end())
            mSelectedMonth = currentMonth;
        else
            mSelectedMonth = mMonths.front();
        mFallback = mSelectedMonth != currentMonth;
    } else {
        mFallback = false;
    }

    LoadSelectedMonth();
}

void PracticeTabGui::LoadSelectedMonth()
{
    ReadMonthRecords(mDataDir, mSelectedMonth, mMonthRecords, mReadFailed);
    BuildGameList(mMonthRecords, mGameIds);
    RebuildStats();
}

void PracticeTabGui::RebuildStats()
{
    BuildStats(mMonthRecords, mSelectedGame, mSelectedMonth, mStats);
}

void PracticeTabGui::DrawSummary()
{
    ImGui::TextDisabled("%s", S(THPRAC_DATA_PRACTICE_TOTAL));
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram), "%s", FormatAmount(mStats.total).c_str());
    std::string monthLabel = FormatMonthLabel(mSelectedMonth);
    ImGui::TextDisabled(S(THPRAC_DATA_PRACTICE_SUMMARY), mStats.recordCount, mStats.dayCount, monthLabel.c_str());
}

void PracticeTabGui::DrawFilters()
{
    ImGui::TextUnformatted(S(THPRAC_GAMES));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    const char* gamePreview = mSelectedGame.empty()
        ? S(THPRAC_GAME_TIME_ALL)
        : GameDisplayName(mSelectedGame.c_str());
    if (ImGui::BeginCombo("##practice_game", gamePreview)) {
        if (ImGui::Selectable(S(THPRAC_GAME_TIME_ALL), mSelectedGame.empty())) {
            mSelectedGame.clear();
            RebuildStats();
        }
        for (auto& gameId : mGameIds) {
            bool selected = gameId == mSelectedGame;
            if (ImGui::Selectable(GameDisplayName(gameId.c_str()), selected)) {
                mSelectedGame = gameId;
                RebuildStats();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(S(THPRAC_DATA_MONTH));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    std::string monthPreview = FormatMonthLabel(mSelectedMonth);
    if (ImGui::BeginCombo("##practice_month", monthPreview.c_str())) {
        for (auto& month : mMonths) {
            bool selected = month == mSelectedMonth;
            std::string label = FormatMonthLabel(month);
            if (ImGui::Selectable(label.c_str(), selected)) {
                mSelectedMonth = month;
                mFallback = false;
                LoadSelectedMonth();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (mFallback) {
        std::string monthLabel = FormatMonthLabel(mSelectedMonth);
        ImGui::TextDisabled(S(THPRAC_DATA_PRACTICE_FALLBACK), monthLabel.c_str());
    }
}

void PracticeTabGui::DrawWeeks()
{
    for (size_t i = 0; i < mStats.weeks.size(); ++i) {
        auto& week = mStats.weeks[i];
        ImGui::PushID((int)i);

        ImGui::TextUnformatted(week.label.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram), "%s %s",
            S(THPRAC_DATA_PRACTICE_WEEK_TOTAL), FormatAmount(week.total).c_str());
        ImGui::Separator();

        if (week.days.empty()) {
            ImGui::TextDisabled("%s", S(THPRAC_DATA_PRACTICE_NO_RECORD));
        } else {
            ImGui::Indent(12.0f);
            for (auto& day : week.days)
                DrawDayRow(day);
            ImGui::Unindent(12.0f);
        }
        ImGui::Spacing();
        ImGui::PopID();
    }
}

void PracticeTabGui::DrawDayRow(const PracticeDay& day)
{
    SysDays date { std::chrono::days { day.dayKey } };
    int isoWeekday = (int)std::chrono::weekday { date }.iso_encoding();
    std::string dateLabel = std::format("{} {}", FormatMonthDay(day.dayKey), WeekdayName(isoWeekday));

    ImGui::TextUnformatted(dateLabel.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(S(THPRAC_DATA_PRACTICE_DAY_COUNT), day.count);

    std::string amount = FormatAmount(day.total);
    float amountWidth = ImGui::CalcTextSize(amount.c_str()).x;
    float right = ImGui::GetContentRegionMax().x - amountWidth;
    if (right > ImGui::GetCursorPosX())
        ImGui::SameLine(right);
    else
        ImGui::SameLine();
    ImGui::TextUnformatted(amount.c_str());

    float ratio = (float)day.total / 240.0f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    DrawPracticeRatioBar(pos, width, ratio);
    ImGui::Dummy(ImVec2(width, 11.0f));
}

// ---- 单过程 --------------------------------------------------------------
// 与「练习量」共用同一批 logger_sessions_*.jsonl，但这里的口径面向「某一段
// 起终点」的练习统计：一条日志 = 一次（或 total 次合并）练习。
struct SessionEvent {
    std::string type;
    std::string checkpoint;
};

struct SessionRecord {
    std::string game;
    std::string start;
    std::string shot;
    std::string startCheckpoint;
    std::string endCheckpoint;
    int checkpointCount = 0;
    int total = 0;
    int got = 0;
    std::vector<SessionEvent> events;
};

static bool IsRunStartText(std::string_view text)
{
    if (text.size() != 19)
        return false;
    if (text[4] != '-' || text[7] != '-' || text[10] != ' ' || text[13] != ':' || text[16] != ':')
        return false;
    for (int i : { 0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18 }) {
        if (text[i] < '0' || text[i] > '9')
            return false;
    }
    int year = 0, month = 0, day = 0;
    return ParseYmdPrefix(text, year, month, day);
}

static bool ParseSessionRecord(const char* text, size_t length, SessionRecord& record)
{
    rapidjson::Document doc;
    doc.Parse(text, length);
    if (doc.HasParseError() || !doc.IsObject())
        return false;

    auto game = doc.FindMember("game");
    auto start = doc.FindMember("start");
    auto startCheckpoint = doc.FindMember("startCheckpoint");
    auto endCheckpoint = doc.FindMember("endCheckpoint");
    auto checkpointCount = doc.FindMember("checkpointCount");
    if (game == doc.MemberEnd() || !game->value.IsString()
        || start == doc.MemberEnd() || !start->value.IsString()
        || startCheckpoint == doc.MemberEnd() || !startCheckpoint->value.IsString()
        || endCheckpoint == doc.MemberEnd() || !endCheckpoint->value.IsString()
        || checkpointCount == doc.MemberEnd() || !checkpointCount->value.IsInt())
        return false;

    std::string_view startText { start->value.GetString(), start->value.GetStringLength() };
    if (!IsRunStartText(startText))
        return false;

    record.game.assign(game->value.GetString(), game->value.GetStringLength());
    record.start.assign(startText);
    record.startCheckpoint.assign(startCheckpoint->value.GetString(), startCheckpoint->value.GetStringLength());
    record.endCheckpoint.assign(endCheckpoint->value.GetString(), endCheckpoint->value.GetStringLength());
    record.checkpointCount = checkpointCount->value.GetInt();

    auto shot = doc.FindMember("shot");
    if (shot != doc.MemberEnd() && shot->value.IsString())
        record.shot.assign(shot->value.GetString(), shot->value.GetStringLength());

    auto total = doc.FindMember("total");
    if (total != doc.MemberEnd() && total->value.IsInt() && total->value.GetInt() > 0) {
        record.total = total->value.GetInt();
        auto got = doc.FindMember("got");
        if (got != doc.MemberEnd() && got->value.IsInt())
            record.got = std::clamp(got->value.GetInt(), 0, record.total);
    }

    auto events = doc.FindMember("events");
    if (events != doc.MemberEnd() && events->value.IsArray()) {
        for (auto& event : events->value.GetArray()) {
            if (!event.IsObject())
                continue;
            auto type = event.FindMember("type");
            auto checkpoint = event.FindMember("checkpoint");
            if (type == event.MemberEnd() || !type->value.IsString()
                || checkpoint == event.MemberEnd() || !checkpoint->value.IsString())
                continue;
            record.events.push_back(SessionEvent {
                std::string(type->value.GetString(), type->value.GetStringLength()),
                std::string(checkpoint->value.GetString(), checkpoint->value.GetStringLength()) });
        }
    }
    return true;
}

static void LoadRecentSessionRecords(const std::wstring& dataDir, const std::vector<std::string>& months,
    std::vector<SessionRecord>& records, bool& anyRead, bool& anyFailed)
{
    records.clear();
    anyRead = false;
    anyFailed = false;

    const size_t monthCount = std::min<size_t>(3, months.size());
    for (size_t i = 0; i < monthCount; ++i) {
        bool read = ReadMonthLines(dataDir, months[i], [&](const char* text, size_t length) {
            SessionRecord record;
            if (ParseSessionRecord(text, length, record))
                records.push_back(std::move(record));
        });
        if (read)
            anyRead = true;
        else
            anyFailed = true;
    }

    // start 是定长本地时间字符串，字典序即时间序；三个文件合并后统一排序。
    std::stable_sort(records.begin(), records.end(), [](const SessionRecord& a, const SessionRecord& b) {
        return a.start > b.start;
    });
}

// 单卡页只读「所选月份」一个文件。与 LoadRecentSessionRecords() 共用同一
// SessionRecord / ParseSessionRecord，避免两页对日志字段的口径漂移。
static void LoadMonthSessionRecords(const std::wstring& dataDir, const std::string& month,
    std::vector<SessionRecord>& records, bool& readFailed)
{
    records.clear();
    readFailed = !ReadMonthLines(dataDir, month, [&](const char* text, size_t length) {
        SessionRecord record;
        if (ParseSessionRecord(text, length, record))
            records.push_back(std::move(record));
    });

    std::stable_sort(records.begin(), records.end(), [](const SessionRecord& a, const SessionRecord& b) {
        return a.start > b.start;
    });
}

// ---- 单过程配置 ----------------------------------------------------------
struct SingleRunConfig {
    std::string name;
    std::string game;
    std::string start;
    std::string end;
};

static constexpr wchar_t kSingleRunConfigFileName[] = L"thprac_single_run_configs.json";

static void LoadSingleRunConfigs(const std::wstring& dataDir, std::vector<SingleRunConfig>& configs)
{
    configs.clear();
    if (dataDir.empty())
        return;

    std::wstring path = JoinDataDir(dataDir) + kSingleRunConfigFileName;
    void* buffer = nullptr;
    size_t size = 0;
    if (!LoadJsonFile(path, buffer, size))
        return;
    std::unique_ptr<void, decltype(&free)> bufferGuard(buffer, &free);

    const char* text = (const char*)buffer;
    if (size >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) {
        text += 3;
        size -= 3;
    }

    rapidjson::Document doc;
    doc.Parse(text, size);
    if (doc.HasParseError() || !doc.IsArray())
        return;

    for (auto& item : doc.GetArray()) {
        if (!item.IsObject())
            continue;
        auto name = item.FindMember("name");
        auto game = item.FindMember("game");
        auto start = item.FindMember("start");
        auto end = item.FindMember("end");
        if (name == item.MemberEnd() || !name->value.IsString() || name->value.GetStringLength() == 0
            || game == item.MemberEnd() || !game->value.IsString() || game->value.GetStringLength() == 0
            || start == item.MemberEnd() || !start->value.IsString() || start->value.GetStringLength() == 0
            || end == item.MemberEnd() || !end->value.IsString() || end->value.GetStringLength() == 0)
            continue;

        SingleRunConfig config;
        config.name.assign(name->value.GetString(), name->value.GetStringLength());
        config.game.assign(game->value.GetString(), game->value.GetStringLength());
        config.start.assign(start->value.GetString(), start->value.GetStringLength());
        config.end.assign(end->value.GetString(), end->value.GetStringLength());
        configs.push_back(std::move(config));
    }
}

static bool WriteSingleRunConfigs(const std::wstring& dataDir, const std::vector<SingleRunConfig>& configs)
{
    if (dataDir.empty())
        return false;

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 4);
    writer.StartArray();
    for (auto& config : configs) {
        writer.StartObject();
        writer.Key("name");
        writer.String(config.name.c_str(), (rapidjson::SizeType)config.name.size());
        writer.Key("game");
        writer.String(config.game.c_str(), (rapidjson::SizeType)config.game.size());
        writer.Key("start");
        writer.String(config.start.c_str(), (rapidjson::SizeType)config.start.size());
        writer.Key("end");
        writer.String(config.end.c_str(), (rapidjson::SizeType)config.end.size());
        writer.EndObject();
    }
    writer.EndArray();

    std::wstring path = JoinDataDir(dataDir) + kSingleRunConfigFileName;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const DWORD length = (DWORD)buffer.GetSize();
    const bool ok = WriteFile(file, buffer.GetString(), length, &written, nullptr) && written == length;
    CloseHandle(file);
    return ok;
}

static std::string TrimText(std::string_view text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r' || text[begin] == '\n'))
        ++begin;
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' || text[end - 1] == '\n'))
        --end;
    return std::string(text.substr(begin, end - begin));
}

template <size_t N>
static void CopyTextBuffer(char (&buffer)[N], const std::string& text)
{
    const size_t length = text.size() < N - 1 ? text.size() : N - 1;
    memcpy(buffer, text.data(), length);
    buffer[length] = '\0';
}

static int FindCheckpointIndex(const std::vector<std::string>& names, const std::string& name, bool last)
{
    if (last) {
        for (int i = (int)names.size() - 1; i >= 0; --i) {
            if (names[i] == name)
                return i;
        }
    } else {
        for (int i = 0; i < (int)names.size(); ++i) {
            if (names[i] == name)
                return i;
        }
    }
    return -1;
}

struct CheckpointSource {
    const char* game;
    const char* csv;
};

static const CheckpointSource kCheckpointSources[] = {
    { "th10", TH10::kTh10CheckpointsCsv },
    { "th11", TH11::kTh11CheckpointsCsv },
    { "th12", TH12::kTh12CheckpointsCsv },
    { "th13", TH13::kTh13CheckpointsCsv },
    { "th14", TH14::kTh14CheckpointsCsv },
    { "th15", TH15::kTh15CheckpointsCsv },
    { "th16", TH16::kTh16CheckpointsCsv },
    { "th17", TH17::kTh17CheckpointsCsv },
    { "th18", TH18::kTh18CheckpointsCsv },
};

static const CheckpointSource* FindCheckpointSource(const std::string& game)
{
    for (auto& source : kCheckpointSources) {
        if (game == source.game)
            return &source;
    }
    return nullptr;
}

static bool LoadCheckpointNames(const char* csv, std::vector<std::string>& names)
{
    names.clear();
    if (!csv)
        return false;
    try {
        std::istringstream stream(csv);
        rapidcsv::Document doc(stream, rapidcsv::LabelParams(0, -1));
        if (doc.GetColumnCount() < 4)
            return false;
        for (size_t row = 0; row < doc.GetRowCount(); ++row) {
            auto values = doc.GetRow<std::string>(row);
            if (values.size() >= 4 && !values[3].empty())
                names.push_back(values[3]);
        }
    } catch (...) {
        names.clear();
        return false;
    }
    return !names.empty();
}

enum class SingleRunRange {
    Last10,
    Last20,
    LastWeek,
    LastMonth,
    All,
};

static const char* SingleRunRangeLabel(SingleRunRange range)
{
    switch (range) {
    case SingleRunRange::Last10:
        return S(THPRAC_DATA_SINGLE_RUN_RANGE_LAST10);
    case SingleRunRange::Last20:
        return S(THPRAC_DATA_SINGLE_RUN_RANGE_LAST20);
    case SingleRunRange::LastWeek:
        return S(THPRAC_DATA_SINGLE_RUN_RANGE_WEEK);
    case SingleRunRange::LastMonth:
        return S(THPRAC_DATA_SINGLE_RUN_RANGE_MONTH);
    case SingleRunRange::All:
    default:
        return S(THPRAC_GAME_TIME_ALL);
    }
}

static std::string LocalTimeMinusDays(int days)
{
    time_t now = time(nullptr);
    now -= (time_t)days * 24 * 60 * 60;
    tm localTime {};
    if (localtime_s(&localTime, &now) != 0)
        return {};
    char text[32] {};
    if (strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &localTime) == 0)
        return {};
    return text;
}

static std::vector<const SessionRecord*> FilterSingleRunRange(
    const std::vector<const SessionRecord*>& hits, SingleRunRange range)
{
    if (range == SingleRunRange::All)
        return hits;
    if (range == SingleRunRange::Last10 || range == SingleRunRange::Last20) {
        const size_t count = std::min<size_t>(range == SingleRunRange::Last10 ? 10 : 20, hits.size());
        return std::vector<const SessionRecord*>(hits.begin(), hits.begin() + count);
    }
    const std::string cutoff = LocalTimeMinusDays(range == SingleRunRange::LastWeek ? 7 : 30);
    if (cutoff.empty())
        return {};
    std::vector<const SessionRecord*> filtered;
    filtered.reserve(hits.size());
    for (auto* record : hits) {
        if (record->start >= cutoff)
            filtered.push_back(record);
    }
    return filtered;
}

static std::vector<std::string> MissPoints(const SessionRecord& record)
{
    std::vector<std::string> points;
    for (auto& event : record.events) {
        if (event.type == "interrupt")
            continue;
        if (std::find(points.begin(), points.end(), event.checkpoint) == points.end())
            points.push_back(event.checkpoint);
    }
    return points;
}

struct SingleRunChapterRate {
    std::string name;
    double rate = 0.0;
};

struct SingleRunStats {
    int attempts = 0;
    int clean = 0;
    int rows = 0;
    int unassignedFailures = 0;
    bool hasChapters = false;
    bool estimatedValid = false;
    double estimatedNnRate = 0.0;
    double actualNnRate = 0.0;
    std::vector<SingleRunChapterRate> rates;
};

static void ComputeSingleRunStats(const std::vector<const SessionRecord*>& filtered,
    const std::vector<std::string>& chapters, int startIndex, int endIndex, SingleRunStats& stats)
{
    stats = {};
    if (startIndex < 0 || endIndex < startIndex || endIndex >= (int)chapters.size())
        return;

    const int chapterCount = endIndex - startIndex + 1;
    std::vector<int> hits((size_t)chapterCount, 0);
    stats.hasChapters = true;

    for (auto* record : filtered) {
        const int attempts = record->total > 0 ? record->total : 1;
        const std::vector<std::string> misses = MissPoints(*record);
        const int clean = record->total > 0
            ? std::clamp(record->got, 0, record->total)
            : (misses.empty() ? 1 : 0);
        stats.attempts += attempts;
        stats.clean += clean;
        ++stats.rows;

        const int failures = attempts - clean;
        if (failures <= 0)
            continue;

        // 合并行（total > 1）的 events 可能被规范化成空数组，也可能残留首条
        // 事件。单章节（起点==终点）时全部失败天然属于该章节；否则只能把
        // 已知事件点各算一次，剩余失败不摊派（只影响实际 NN 率）。
        int singleChapter = -1;
        if (chapterCount == 1) {
            singleChapter = 0;
        } else if (record->startCheckpoint == record->endCheckpoint) {
            for (int i = startIndex; i <= endIndex; ++i) {
                if (chapters[i] == record->startCheckpoint) {
                    singleChapter = i - startIndex;
                    break;
                }
            }
        }
        if (singleChapter >= 0) {
            hits[singleChapter] += failures;
            continue;
        }

        std::vector<int> candidates;
        for (auto& miss : misses) {
            for (int i = startIndex; i <= endIndex; ++i) {
                if (chapters[i] != miss)
                    continue;
                const int local = i - startIndex;
                if (std::find(candidates.begin(), candidates.end(), local) == candidates.end())
                    candidates.push_back(local);
                break;
            }
        }

        if (record->total > 1) {
            const int known = std::min<int>(failures, (int)candidates.size());
            for (int i = 0; i < known; ++i)
                ++hits[candidates[i]];
            stats.unassignedFailures += failures - known;
        } else {
            for (int candidate : candidates)
                ++hits[candidate];
        }
    }

    stats.actualNnRate = stats.attempts == 0
        ? 0.0
        : (double)stats.clean * 100.0 / (double)stats.attempts;

    stats.rates.reserve((size_t)chapterCount);
    double product = 1.0;
    for (int i = 0; i < chapterCount; ++i) {
        const double rate = stats.attempts == 0
            ? 0.0
            : (double)(stats.attempts - hits[i]) * 100.0 / (double)stats.attempts;
        stats.rates.push_back({ chapters[startIndex + i], rate });
        product *= rate / 100.0;
    }
    stats.estimatedValid = true;
    stats.estimatedNnRate = stats.attempts == 0 ? 0.0 : product * 100.0;
}

class SingleRunTabGui {
public:
    void Gui();

private:
    enum class ConfigDialogMode {
        List,
        Edit,
    };

    void EnsureInitialized();
    void Refresh();
    void SelectGame(const std::string& game);
    void LoadGameTable();
    void ResetQueryResult();
    void RunQuery();
    void RecomputeStats();
    void DrawFilters();
    void DrawResults();
    void DrawConfigDialog();
    void OpenConfigList();
    void OpenConfigEditor(int index, bool returnToList);
    void DrawConfigList();
    void DrawConfigEditor();
    void SaveConfigFromEditor();
    void LoadConfig(int index);
    void DeleteConfig(int index);
    bool IsConfigNameTaken(const std::string& name, int ignoreIndex) const;

    bool mInitialized = false;
    bool mGameChosen = false;
    bool mNoFiles = false;
    bool mReadFailed = false;
    bool mGameTableReady = false;
    bool mQueried = false;

    std::wstring mDataDir;
    std::vector<std::string> mMonths;
    std::vector<SessionRecord> mRecords;
    std::vector<SingleRunConfig> mConfigs;
    std::vector<std::string> mCheckpoints;
    std::string mGame = "th14";

    int mStartIndex = -1;
    int mEndIndex = -1;
    SingleRunRange mRange = SingleRunRange::Last20;
    std::vector<const SessionRecord*> mHits;
    std::vector<const SessionRecord*> mFiltered;
    SingleRunStats mStats;

    bool mConfigDialogRequested = false;
    ConfigDialogMode mConfigDialogMode = ConfigDialogMode::List;
    bool mConfigDialogReturnToList = false;
    int mEditingConfigIndex = -1;
    int mPendingDeleteIndex = -1;
    char mConfigName[128] {};
    char mConfigStart[512] {};
    char mConfigEnd[512] {};
    std::string mConfigError;
};

void SingleRunTabGui::Gui()
{
    EnsureInitialized();

    if (ImGui::Button(S(THPRAC_DATA_REFRESH)))
        Refresh();
    ImGui::SameLine();
    ImGui::TextDisabled(S(THPRAC_DATA_SINGLE_RUN_SOURCE), "logger_sessions_*.jsonl");
    ImGui::Separator();

    DrawFilters();
    ImGui::Separator();

    if (mNoFiles) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_NO_FILE));
    } else if (mRecords.empty() && mReadFailed) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_READ_FAILED));
    } else {
        if (mReadFailed)
            ImGui::TextDisabled("%s", S(THPRAC_DATA_READ_FAILED));
        ImGui::BeginChild("##single_run_body", ImVec2(0.0f, 0.0f), false);
        if (!mQueried)
            ImGui::TextWrapped("%s", S(THPRAC_DATA_SINGLE_RUN_HINT));
        else
            DrawResults();
        ImGui::EndChild();
    }

    DrawConfigDialog();
}

void SingleRunTabGui::EnsureInitialized()
{
    if (mInitialized)
        return;
    mInitialized = true;
    mDataDir = LauncherGetDataDir();
    LoadSingleRunConfigs(mDataDir, mConfigs);
    Refresh();
}

void SingleRunTabGui::Refresh()
{
    mDataDir = LauncherGetDataDir();
    ListMonthFiles(mDataDir, mMonths);
    mNoFiles = mMonths.empty();

    bool anyRead = false;
    bool anyFailed = false;
    LoadRecentSessionRecords(mDataDir, mMonths, mRecords, anyRead, anyFailed);
    mReadFailed = anyFailed;

    if (!mGameChosen && !mRecords.empty()) {
        const std::string& newestGame = mRecords.front().game;
        if (FindCheckpointSource(newestGame))
            mGame = newestGame;
        mGameChosen = true;
    }
    if (!FindCheckpointSource(mGame))
        mGame = "th14";

    LoadGameTable();

    if (mGameTableReady) {
        if (mStartIndex >= (int)mCheckpoints.size())
            mStartIndex = -1;
        if (mEndIndex >= (int)mCheckpoints.size())
            mEndIndex = -1;
        if (mStartIndex < 0 || mEndIndex < mStartIndex)
            mEndIndex = -1;
    } else {
        mStartIndex = -1;
        mEndIndex = -1;
    }

    mPendingDeleteIndex = -1;
    mEditingConfigIndex = -1;

    if (mQueried && mStartIndex >= 0 && mEndIndex >= mStartIndex && !mNoFiles)
        RunQuery();
    else
        ResetQueryResult();
}

void SingleRunTabGui::SelectGame(const std::string& game)
{
    if (game == mGame)
        return;
    mGame = game;
    mGameChosen = true;
    mStartIndex = -1;
    mEndIndex = -1;
    mPendingDeleteIndex = -1;
    mEditingConfigIndex = -1;
    mConfigError.clear();
    ResetQueryResult();
    LoadGameTable();
}

void SingleRunTabGui::LoadGameTable()
{
    mCheckpoints.clear();
    mGameTableReady = false;
    const CheckpointSource* source = FindCheckpointSource(mGame);
    if (!source)
        return;
    mGameTableReady = LoadCheckpointNames(source->csv, mCheckpoints);
    if (!mGameTableReady)
        mCheckpoints.clear();
}

void SingleRunTabGui::ResetQueryResult()
{
    mQueried = false;
    mHits.clear();
    mFiltered.clear();
    mStats = {};
}

void SingleRunTabGui::RunQuery()
{
    if (mStartIndex < 0 || mEndIndex < mStartIndex || mEndIndex >= (int)mCheckpoints.size()) {
        ResetQueryResult();
        return;
    }

    mHits.clear();
    for (auto& record : mRecords) {
        if (record.game != mGame)
            continue;
        if (record.startCheckpoint != mCheckpoints[mStartIndex])
            continue;
        if (record.endCheckpoint != mCheckpoints[mEndIndex])
            continue;
        mHits.push_back(&record);
    }
    mQueried = true;
    RecomputeStats();
}

void SingleRunTabGui::RecomputeStats()
{
    if (!mQueried) {
        mFiltered.clear();
        mStats = {};
        return;
    }
    mFiltered = FilterSingleRunRange(mHits, mRange);
    ComputeSingleRunStats(mFiltered, mCheckpoints, mStartIndex, mEndIndex, mStats);
}

void SingleRunTabGui::DrawFilters()
{
    ImGui::TextUnformatted(S(THPRAC_GAMES));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::BeginCombo("##single_run_game", GameDisplayName(mGame.c_str()))) {
        for (auto& source : kCheckpointSources) {
            const bool selected = mGame == source.game;
            if (ImGui::Selectable(GameDisplayName(source.game), selected))
                SelectGame(source.game);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_START));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(420.0f);
    const char* startPreview = (mStartIndex >= 0 && mStartIndex < (int)mCheckpoints.size())
        ? mCheckpoints[mStartIndex].c_str()
        : S(THPRAC_DATA_SINGLE_RUN_NOT_SELECTED);
    if (ImGui::BeginCombo("##single_run_start", startPreview)) {
        for (int i = 0; i < (int)mCheckpoints.size(); ++i) {
            const bool selected = mStartIndex == i;
            if (ImGui::Selectable(mCheckpoints[i].c_str(), selected)) {
                mStartIndex = i;
                if (mEndIndex < mStartIndex)
                    mEndIndex = -1;
                ResetQueryResult();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_END));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(420.0f);
    const char* endPreview = (mEndIndex >= 0 && mEndIndex < (int)mCheckpoints.size())
        ? mCheckpoints[mEndIndex].c_str()
        : S(THPRAC_DATA_SINGLE_RUN_NOT_SELECTED);
    if (ImGui::BeginCombo("##single_run_end", endPreview)) {
        const int first = mStartIndex >= 0 ? mStartIndex : 0;
        for (int i = first; i < (int)mCheckpoints.size(); ++i) {
            const bool selected = mEndIndex == i;
            if (ImGui::Selectable(mCheckpoints[i].c_str(), selected)) {
                mEndIndex = i;
                ResetQueryResult();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_RANGE));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(190.0f);
    if (ImGui::BeginCombo("##single_run_range", SingleRunRangeLabel(mRange))) {
        const SingleRunRange ranges[] = { SingleRunRange::Last10, SingleRunRange::Last20,
            SingleRunRange::LastWeek, SingleRunRange::LastMonth, SingleRunRange::All };
        for (SingleRunRange range : ranges) {
            const bool selected = mRange == range;
            if (ImGui::Selectable(SingleRunRangeLabel(range), selected)) {
                mRange = range;
                RecomputeStats();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    const bool canQuery = mGameTableReady && !mNoFiles && mStartIndex >= 0 && mEndIndex >= mStartIndex;
    if (!canQuery) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_QUERY));
        ImGui::PopStyleVar();
    } else if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_QUERY))) {
        RunQuery();
    }

    ImGui::SameLine();
    if (!mGameTableReady) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_SAVE));
        ImGui::PopStyleVar();
    } else if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_SAVE))) {
        OpenConfigEditor(-1, false);
    }

    ImGui::SameLine();
    if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_LIST)))
        OpenConfigList();

    if (!mGameTableReady && !mNoFiles)
        ImGui::TextWrapped("%s", S(THPRAC_DATA_SINGLE_RUN_TABLE_MISSING));
}

void SingleRunTabGui::DrawResults()
{
    if (ImGui::BeginTable("##single_run_metrics", 2, ImGuiTableFlags_::ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", S(THPRAC_DATA_SINGLE_RUN_RECORD_COUNT));
        ImGui::Text("%d", mStats.attempts);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", S(THPRAC_DATA_SINGLE_RUN_NN_COUNT));
        ImGui::Text("%d", mStats.clean);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", S(THPRAC_DATA_SINGLE_RUN_EST_NN_RATE));
        if (mStats.hasChapters && mStats.estimatedValid)
            ImGui::Text("%.2f%%", mStats.estimatedNnRate);
        else
            ImGui::TextUnformatted("—");
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", S(THPRAC_DATA_SINGLE_RUN_ACTUAL_NN_RATE));
        ImGui::Text("%.2f%%", mStats.actualNnRate);
        ImGui::EndTable();
    }

    if (mFiltered.empty())
        ImGui::TextDisabled("%s", S(THPRAC_DATA_PRACTICE_EMPTY_FILTER));

    ImGui::Spacing();
    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_CHAPTER_RATES));
    if (!mStats.hasChapters) {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "%s", S(THPRAC_DATA_SINGLE_RUN_TABLE_MISSING));
    } else if (ImGui::BeginTable("##single_run_chapters", 3, ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("##bar", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##rate", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 80.0f);
        for (auto& rate : mStats.rates) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(rate.name.c_str());
            ImGui::TableNextColumn();
            ImGui::ProgressBar((float)std::clamp(rate.rate / 100.0, 0.0, 1.0), ImVec2(-1.0f, 6.0f), "");
            ImGui::TableNextColumn();
            ImGui::Text("%.2f%%", rate.rate);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_DETAILS));
    if (mFiltered.empty()) {
        ImGui::TextDisabled("%s", S(THPRAC_DATA_PRACTICE_NO_RECORD));
        return;
    }

    const ImVec4 gold(0.90f, 0.72f, 0.20f, 1.0f);
    const ImVec4 green(0.40f, 0.80f, 0.45f, 1.0f);
    if (ImGui::BeginTable("##single_run_details", 3,
            ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_::ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("##time", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("##shot", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("##miss", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthStretch);
        for (auto* record : mFiltered) {
            const std::vector<std::string> misses = MissPoints(*record);
            std::string text;
            for (auto& miss : misses) {
                if (!text.empty())
                    text += ", ";
                text += miss;
            }
            const bool merged = record->total > 1;
            const bool perfect = record->total > 0 ? record->got >= record->total : misses.empty();
            if (text.empty())
                text = (record->total > 0 && record->got < record->total) ? "—" : "NN";
            if (merged) {
                char suffix[64] {};
                std::snprintf(suffix, sizeof(suffix), S(THPRAC_DATA_SINGLE_RUN_MERGED_COUNT), record->total, record->got);
                text += "  ";
                text += suffix;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(record->start.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(record->shot.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(perfect ? gold : green, "%s", text.c_str());
        }
        ImGui::EndTable();
    }
}

void SingleRunTabGui::OpenConfigList()
{
    mConfigDialogMode = ConfigDialogMode::List;
    mConfigDialogReturnToList = false;
    mEditingConfigIndex = -1;
    mPendingDeleteIndex = -1;
    mConfigError.clear();
    mConfigDialogRequested = true;
}

void SingleRunTabGui::OpenConfigEditor(int index, bool returnToList)
{
    mConfigDialogMode = ConfigDialogMode::Edit;
    mConfigDialogReturnToList = returnToList;
    mEditingConfigIndex = index;
    mPendingDeleteIndex = -1;
    mConfigError.clear();

    if (index >= 0 && index < (int)mConfigs.size() && mConfigs[index].game == mGame) {
        const SingleRunConfig& config = mConfigs[index];
        CopyTextBuffer(mConfigName, config.name);
        CopyTextBuffer(mConfigStart, config.start);
        CopyTextBuffer(mConfigEnd, config.end);
    } else {
        mEditingConfigIndex = -1;
        mConfigName[0] = '\0';
        CopyTextBuffer(mConfigStart, mStartIndex >= 0 && mStartIndex < (int)mCheckpoints.size()
                ? mCheckpoints[mStartIndex]
                : std::string());
        CopyTextBuffer(mConfigEnd, mEndIndex >= 0 && mEndIndex < (int)mCheckpoints.size()
                ? mCheckpoints[mEndIndex]
                : std::string());
    }
    mConfigDialogRequested = true;
}

void SingleRunTabGui::DrawConfigDialog()
{
    constexpr const char* kPopupId = "###single_run_config";
    if (mConfigDialogRequested) {
        if (!ImGui::IsPopupOpen(kPopupId))
            ImGui::OpenPopup(kPopupId);
        mConfigDialogRequested = false;
    }

    const bool editingExisting = mConfigDialogMode == ConfigDialogMode::Edit && mEditingConfigIndex >= 0;
    const char* titleKey = mConfigDialogMode == ConfigDialogMode::List
        ? S(THPRAC_DATA_SINGLE_RUN_CONFIG_LIST)
        : (editingExisting ? S(THPRAC_DATA_SINGLE_RUN_CONFIG_EDIT) : S(THPRAC_DATA_SINGLE_RUN_CONFIG_SAVE));
    std::string title = std::string(titleKey) + kPopupId;
    if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (mConfigDialogMode == ConfigDialogMode::List)
            DrawConfigList();
        else
            DrawConfigEditor();
        ImGui::EndPopup();
    }
}

void SingleRunTabGui::DrawConfigList()
{
    ImGui::TextDisabled("%s: %s", S(THPRAC_GAMES), GameDisplayName(mGame.c_str()));
    ImGui::Separator();

    ImGui::BeginChild("##single_run_config_list", ImVec2(520.0f, 300.0f), true);
    if (mPendingDeleteIndex >= 0 && mPendingDeleteIndex < (int)mConfigs.size()) {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), S(THPRAC_DATA_SINGLE_RUN_CONFIG_DELETE_CONFIRM),
            mConfigs[mPendingDeleteIndex].name.c_str());
        ImGui::Separator();
    }

    int visibleCount = 0;
    if (ImGui::BeginTable("##single_run_config_rows", 3, ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("##range", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 190.0f);
        for (int i = 0; i < (int)mConfigs.size(); ++i) {
            const SingleRunConfig& config = mConfigs[i];
            if (config.game != mGame)
                continue;
            ++visibleCount;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(config.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s → %s", config.start.c_str(), config.end.c_str());
            ImGui::TableNextColumn();
            if (mPendingDeleteIndex == i) {
                if (ImGui::SmallButton(S(THPRAC_GAMES_DELETE))) {
                    DeleteConfig(i);
                    ImGui::PopID();
                    break;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(S(THPRAC_CANCEL)))
                    mPendingDeleteIndex = -1;
            } else {
                if (ImGui::SmallButton(S(THPRAC_DATA_SINGLE_RUN_CONFIG_LOAD))) {
                    LoadConfig(i);
                    ImGui::PopID();
                    break;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(S(THPRAC_DATA_SINGLE_RUN_CONFIG_EDIT_ACTION)))
                    OpenConfigEditor(i, true);
                ImGui::SameLine();
                if (ImGui::SmallButton(S(THPRAC_GAMES_DELETE)))
                    mPendingDeleteIndex = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (visibleCount == 0)
        ImGui::TextDisabled("%s", S(THPRAC_DATA_SINGLE_RUN_CONFIG_EMPTY));
    if (!mConfigError.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "%s", mConfigError.c_str());
    ImGui::EndChild();

    if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_NEW)))
        OpenConfigEditor(-1, true);
    ImGui::SameLine();
    if (ImGui::Button(S(THPRAC_CLOSE)))
        ImGui::CloseCurrentPopup();
}

void SingleRunTabGui::DrawConfigEditor()
{
    ImGui::TextDisabled("%s: %s", S(THPRAC_GAMES), GameDisplayName(mGame.c_str()));
    ImGui::Separator();

    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_CONFIG_NAME));
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(380.0f);
    ImGui::InputText("##single_run_config_name", mConfigName, sizeof(mConfigName));

    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_START));
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(380.0f);
    ImGui::InputText("##single_run_config_start", mConfigStart, sizeof(mConfigStart));

    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_END));
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(380.0f);
    ImGui::InputText("##single_run_config_end", mConfigEnd, sizeof(mConfigEnd));

    if (!mConfigError.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "%s", mConfigError.c_str());

    if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_SAVE_ACTION)))
        SaveConfigFromEditor();
    ImGui::SameLine();
    if (ImGui::Button(S(THPRAC_CANCEL))) {
        if (mConfigDialogReturnToList) {
            mConfigDialogMode = ConfigDialogMode::List;
            mEditingConfigIndex = -1;
            mConfigError.clear();
        } else {
            ImGui::CloseCurrentPopup();
        }
    }
}

void SingleRunTabGui::SaveConfigFromEditor()
{
    const std::string name = TrimText(mConfigName);
    const std::string start = TrimText(mConfigStart);
    const std::string end = TrimText(mConfigEnd);
    if (name.empty()) {
        mConfigError = S(THPRAC_LINKS_EDIT_ERR_NAME);
        return;
    }
    if (IsConfigNameTaken(name, mEditingConfigIndex)) {
        mConfigError = S(THPRAC_DATA_SINGLE_RUN_CONFIG_ERR_DUP);
        return;
    }
    const int startIndex = FindCheckpointIndex(mCheckpoints, start, false);
    const int endIndex = FindCheckpointIndex(mCheckpoints, end, true);
    if (startIndex < 0 || endIndex < 0 || endIndex < startIndex) {
        mConfigError = S(THPRAC_DATA_SINGLE_RUN_CONFIG_ERR_RANGE);
        return;
    }

    std::vector<SingleRunConfig> next = mConfigs;
    SingleRunConfig config { name, mGame, start, end };
    if (mEditingConfigIndex >= 0 && mEditingConfigIndex < (int)next.size()
        && next[mEditingConfigIndex].game == mGame) {
        next[mEditingConfigIndex] = std::move(config);
    } else {
        next.push_back(std::move(config));
    }
    if (!WriteSingleRunConfigs(mDataDir, next)) {
        mConfigError = S(THPRAC_DATA_SINGLE_RUN_CONFIG_WRITE_FAILED);
        return;
    }
    mConfigs = std::move(next);
    mEditingConfigIndex = -1;
    mConfigError.clear();
    if (mConfigDialogReturnToList)
        mConfigDialogMode = ConfigDialogMode::List;
    else
        ImGui::CloseCurrentPopup();
}

void SingleRunTabGui::LoadConfig(int index)
{
    if (index < 0 || index >= (int)mConfigs.size())
        return;
    const SingleRunConfig& config = mConfigs[index];
    if (config.game != mGame)
        return;
    const int startIndex = FindCheckpointIndex(mCheckpoints, config.start, false);
    const int endIndex = FindCheckpointIndex(mCheckpoints, config.end, true);
    if (startIndex < 0 || endIndex < 0 || endIndex < startIndex) {
        mConfigError = S(THPRAC_DATA_SINGLE_RUN_CONFIG_ERR_RANGE);
        return;
    }
    mStartIndex = startIndex;
    mEndIndex = endIndex;
    mPendingDeleteIndex = -1;
    mConfigError.clear();
    RunQuery();
    ImGui::CloseCurrentPopup();
}

void SingleRunTabGui::DeleteConfig(int index)
{
    if (index < 0 || index >= (int)mConfigs.size())
        return;
    std::vector<SingleRunConfig> next = mConfigs;
    next.erase(next.begin() + index);
    if (!WriteSingleRunConfigs(mDataDir, next)) {
        mConfigError = S(THPRAC_DATA_SINGLE_RUN_CONFIG_WRITE_FAILED);
        return;
    }
    mConfigs = std::move(next);
    mPendingDeleteIndex = -1;
    mEditingConfigIndex = -1;
    mConfigError.clear();
}

bool SingleRunTabGui::IsConfigNameTaken(const std::string& name, int ignoreIndex) const
{
    for (int i = 0; i < (int)mConfigs.size(); ++i) {
        if (i == ignoreIndex)
            continue;
        if (mConfigs[i].game == mGame && mConfigs[i].name == name)
            return true;
    }
    return false;
}

SingleRunTabGui gSingleRunTab;

// ---- 单卡 ----------------------------------------------------------------
// 与「单过程」共用同一批 logger_sessions_*.jsonl，但本页只读所选月份的
// 一个文件，并把记录按 startCheckpoint 归到唯一章节，再按日聚合 total/got。
struct SingleCardConfig {
    std::string name;
    std::string game;
    std::string chapter;
};

static constexpr wchar_t kSingleCardConfigFileName[] = L"thprac_single_card_configs.json";

static void LoadSingleCardConfigs(const std::wstring& dataDir, std::vector<SingleCardConfig>& configs)
{
    configs.clear();
    if (dataDir.empty())
        return;

    std::wstring path = JoinDataDir(dataDir) + kSingleCardConfigFileName;
    void* buffer = nullptr;
    size_t size = 0;
    if (!LoadJsonFile(path, buffer, size))
        return;
    std::unique_ptr<void, decltype(&free)> bufferGuard(buffer, &free);

    const char* text = (const char*)buffer;
    if (size >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) {
        text += 3;
        size -= 3;
    }

    rapidjson::Document doc;
    doc.Parse(text, size);
    if (doc.HasParseError() || !doc.IsArray())
        return;

    for (auto& item : doc.GetArray()) {
        if (!item.IsObject())
            continue;
        auto name = item.FindMember("name");
        auto game = item.FindMember("game");
        auto chapter = item.FindMember("chapter");
        if (name == item.MemberEnd() || !name->value.IsString() || name->value.GetStringLength() == 0
            || game == item.MemberEnd() || !game->value.IsString() || game->value.GetStringLength() == 0
            || chapter == item.MemberEnd() || !chapter->value.IsString() || chapter->value.GetStringLength() == 0)
            continue;

        SingleCardConfig config;
        config.name.assign(name->value.GetString(), name->value.GetStringLength());
        config.game.assign(game->value.GetString(), game->value.GetStringLength());
        config.chapter.assign(chapter->value.GetString(), chapter->value.GetStringLength());
        configs.push_back(std::move(config));
    }
}

static bool WriteSingleCardConfigs(const std::wstring& dataDir, const std::vector<SingleCardConfig>& configs)
{
    if (dataDir.empty())
        return false;

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 4);
    writer.StartArray();
    for (auto& config : configs) {
        writer.StartObject();
        writer.Key("name");
        writer.String(config.name.c_str(), (rapidjson::SizeType)config.name.size());
        writer.Key("game");
        writer.String(config.game.c_str(), (rapidjson::SizeType)config.game.size());
        writer.Key("chapter");
        writer.String(config.chapter.c_str(), (rapidjson::SizeType)config.chapter.size());
        writer.EndObject();
    }
    writer.EndArray();

    std::wstring path = JoinDataDir(dataDir) + kSingleCardConfigFileName;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const DWORD length = (DWORD)buffer.GetSize();
    const bool ok = WriteFile(file, buffer.GetString(), length, &written, nullptr) && written == length;
    CloseHandle(file);
    return ok;
}

static bool SessionHasAttemptEvent(const SessionRecord& record)
{
    for (auto& event : record.events) {
        if (event.type != "interrupt")
            return true;
    }
    return false;
}

static bool SessionHasInterruptEvent(const SessionRecord& record)
{
    for (auto& event : record.events) {
        if (event.type == "interrupt")
            return true;
    }
    return false;
}

// 与 thprac_games.cpp 的 GameSessionIsSingleChapter()（口径 13/16）保持一致。
// total > 0 的记录由 logger 直接标记为单章节；旧行没有 total 时按同一判据复判。
static bool IsSingleChapterSession(const SessionRecord& record)
{
    if (record.total > 0)
        return true;
    const bool onlyInterrupt = !record.events.empty() && !SessionHasAttemptEvent(record);
    return (record.checkpointCount == 2 && SessionHasInterruptEvent(record))
        || (record.checkpointCount == 1 && !onlyInterrupt);
}

struct SingleCardDay {
    int64_t dayKey = 0;
    int64_t total = 0;
    int64_t got = 0;
};

static void BuildSingleCardDays(const std::vector<SessionRecord>& records, const std::string& game,
    const std::string& chapter, std::vector<SingleCardDay>& days)
{
    days.clear();
    std::map<int64_t, SingleCardDay> byDay;
    for (auto& record : records) {
        if (record.game != game || record.startCheckpoint != chapter)
            continue;
        if (!IsSingleChapterSession(record))
            continue;

        int year = 0, month = 0, day = 0;
        if (!ParseYmdPrefix(record.start, year, month, day))
            continue;

        const int64_t total = record.total > 0 ? (int64_t)record.total : 1;
        const int64_t got = record.total > 0
            ? (int64_t)std::clamp(record.got, 0, record.total)
            : (MissPoints(record).empty() ? 1 : 0);

        const int64_t dayKey = DateKey(year, month, day);
        auto& aggregate = byDay[dayKey];
        aggregate.dayKey = dayKey;
        aggregate.total += total;
        aggregate.got += got;
    }

    days.reserve(byDay.size());
    for (auto it = byDay.rbegin(); it != byDay.rend(); ++it)
        days.push_back(it->second);
}

class SingleCardTabGui {
public:
    void Gui();

private:
    enum class ConfigDialogMode {
        List,
        Edit,
    };

    void EnsureInitialized();
    void Refresh();
    void SelectGame(const std::string& game);
    void SelectMonth(const std::string& month);
    void LoadSelectedMonth();
    void LoadGameTable();
    void SelectChapter(int index);
    void RecomputeDays();
    void DrawFilters();
    void DrawDays();
    void DrawConfigDialog();
    void OpenConfigList();
    void OpenConfigEditor(int index, bool returnToList);
    void DrawConfigList();
    void DrawConfigEditor();
    void SaveConfigFromEditor();
    void LoadConfig(int index);
    void DeleteConfig(int index);
    bool IsConfigNameTaken(const std::string& name, int ignoreIndex) const;

    bool mInitialized = false;
    bool mGameChosen = false;
    bool mNoFiles = false;
    bool mReadFailed = false;
    bool mGameTableReady = false;
    bool mFallback = false;

    std::wstring mDataDir;
    std::vector<std::string> mMonths;
    std::string mSelectedMonth;
    std::vector<SessionRecord> mRecords;
    std::vector<SingleCardConfig> mConfigs;
    std::vector<std::string> mCheckpoints;
    std::string mGame = "th14";

    int mChapterIndex = -1;
    std::vector<SingleCardDay> mDays;

    bool mConfigDialogRequested = false;
    ConfigDialogMode mConfigDialogMode = ConfigDialogMode::List;
    bool mConfigDialogReturnToList = false;
    int mEditingConfigIndex = -1;
    int mPendingDeleteIndex = -1;
    char mConfigName[128] {};
    char mConfigChapter[512] {};
    std::string mConfigError;
};

void SingleCardTabGui::Gui()
{
    EnsureInitialized();

    if (ImGui::Button(S(THPRAC_DATA_REFRESH)))
        Refresh();
    if (!mSelectedMonth.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", FormatMonthLabel(mSelectedMonth).c_str());
    }
    ImGui::Separator();

    DrawFilters();
    ImGui::Separator();

    if (mNoFiles) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_NO_FILE));
    } else if (mRecords.empty() && mReadFailed) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_READ_FAILED));
    } else {
        if (mReadFailed)
            ImGui::TextDisabled("%s", S(THPRAC_DATA_READ_FAILED));
        ImGui::BeginChild("##single_card_body", ImVec2(0.0f, 0.0f), false);
        if (!mGameTableReady)
            ImGui::TextWrapped("%s", S(THPRAC_DATA_SINGLE_RUN_TABLE_MISSING));
        else if (mChapterIndex < 0)
            ImGui::TextWrapped("%s", S(THPRAC_DATA_SINGLE_CARD_HINT));
        else if (mDays.empty())
            ImGui::TextWrapped("%s", S(THPRAC_DATA_SINGLE_CARD_EMPTY));
        else
            DrawDays();
        ImGui::EndChild();
    }

    DrawConfigDialog();
}

void SingleCardTabGui::EnsureInitialized()
{
    if (mInitialized)
        return;
    mInitialized = true;
    mDataDir = LauncherGetDataDir();
    LoadSingleCardConfigs(mDataDir, mConfigs);
    Refresh();
}

void SingleCardTabGui::Refresh()
{
    mDataDir = LauncherGetDataDir();
    ListMonthFiles(mDataDir, mMonths);
    mNoFiles = mMonths.empty();

    mPendingDeleteIndex = -1;
    mEditingConfigIndex = -1;
    mConfigError.clear();

    if (mNoFiles) {
        mSelectedMonth.clear();
        mFallback = false;
    } else {
        const std::string currentMonth = CurrentMonthKey();
        const bool keepSelection = !mSelectedMonth.empty()
            && std::find(mMonths.begin(), mMonths.end(), mSelectedMonth) != mMonths.end();
        if (keepSelection) {
            mFallback = false;
        } else {
            if (std::find(mMonths.begin(), mMonths.end(), currentMonth) != mMonths.end())
                mSelectedMonth = currentMonth;
            else
                mSelectedMonth = mMonths.front();
            mFallback = mSelectedMonth != currentMonth;
        }
    }

    LoadSelectedMonth();
}

void SingleCardTabGui::SelectGame(const std::string& game)
{
    if (game == mGame)
        return;
    mGame = game;
    mGameChosen = true;
    mChapterIndex = -1;
    mDays.clear();
    mPendingDeleteIndex = -1;
    mEditingConfigIndex = -1;
    mConfigError.clear();
    LoadGameTable();
}

void SingleCardTabGui::SelectMonth(const std::string& month)
{
    if (month == mSelectedMonth)
        return;
    mSelectedMonth = month;
    mFallback = false;
    LoadSelectedMonth();
}

void SingleCardTabGui::LoadSelectedMonth()
{
    mReadFailed = false;
    if (mSelectedMonth.empty())
        mRecords.clear();
    else
        LoadMonthSessionRecords(mDataDir, mSelectedMonth, mRecords, mReadFailed);

    if (!mGameChosen && !mRecords.empty()) {
        const std::string& newestGame = mRecords.front().game;
        if (FindCheckpointSource(newestGame))
            mGame = newestGame;
        mGameChosen = true;
    }
    if (!FindCheckpointSource(mGame))
        mGame = "th14";

    LoadGameTable();
    if (!mGameTableReady || mChapterIndex >= (int)mCheckpoints.size())
        mChapterIndex = -1;
    RecomputeDays();
}

void SingleCardTabGui::LoadGameTable()
{
    mCheckpoints.clear();
    mGameTableReady = false;
    const CheckpointSource* source = FindCheckpointSource(mGame);
    if (!source)
        return;
    mGameTableReady = LoadCheckpointNames(source->csv, mCheckpoints);
    if (!mGameTableReady)
        mCheckpoints.clear();
}

void SingleCardTabGui::SelectChapter(int index)
{
    mChapterIndex = index;
    RecomputeDays();
}

void SingleCardTabGui::RecomputeDays()
{
    mDays.clear();
    if (!mGameTableReady || mChapterIndex < 0 || mChapterIndex >= (int)mCheckpoints.size())
        return;
    BuildSingleCardDays(mRecords, mGame, mCheckpoints[mChapterIndex], mDays);
}

void SingleCardTabGui::DrawFilters()
{
    ImGui::TextUnformatted(S(THPRAC_GAMES));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::BeginCombo("##single_card_game", GameDisplayName(mGame.c_str()))) {
        for (auto& source : kCheckpointSources) {
            const bool selected = mGame == source.game;
            if (ImGui::Selectable(GameDisplayName(source.game), selected))
                SelectGame(source.game);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(S(THPRAC_DATA_MONTH));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    std::string monthPreview = FormatMonthLabel(mSelectedMonth);
    if (ImGui::BeginCombo("##single_card_month", monthPreview.c_str())) {
        for (auto& month : mMonths) {
            const bool selected = month == mSelectedMonth;
            std::string label = FormatMonthLabel(month);
            if (ImGui::Selectable(label.c_str(), selected))
                SelectMonth(month);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted(S(TH_CHAPTER));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(420.0f);
    const char* chapterPreview = (mChapterIndex >= 0 && mChapterIndex < (int)mCheckpoints.size())
        ? mCheckpoints[mChapterIndex].c_str()
        : S(THPRAC_DATA_SINGLE_RUN_NOT_SELECTED);
    if (ImGui::BeginCombo("##single_card_chapter", chapterPreview)) {
        for (int i = 0; i < (int)mCheckpoints.size(); ++i) {
            const bool selected = mChapterIndex == i;
            if (ImGui::Selectable(mCheckpoints[i].c_str(), selected))
                SelectChapter(i);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (!mGameTableReady) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_SAVE));
        ImGui::PopStyleVar();
    } else if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_SAVE))) {
        OpenConfigEditor(-1, false);
    }

    ImGui::SameLine();
    if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_LIST)))
        OpenConfigList();

    if (mFallback) {
        std::string monthLabel = FormatMonthLabel(mSelectedMonth);
        ImGui::TextDisabled(S(THPRAC_DATA_PRACTICE_FALLBACK), monthLabel.c_str());
    }
    if (!mGameTableReady && !mNoFiles)
        ImGui::TextWrapped("%s", S(THPRAC_DATA_SINGLE_RUN_TABLE_MISSING));
}

void SingleCardTabGui::DrawDays()
{
    if (ImGui::BeginTable("##single_card_days", 4,
            ImGuiTableFlags_::ImGuiTableFlags_RowBg | ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn(S(THPRAC_GAME_TIME_DATE), ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(S(THPRAC_DATA_SINGLE_CARD_TOTAL), ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn(S(THPRAC_DATA_SINGLE_CARD_GOT), ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn(S(THPRAC_DATA_SINGLE_CARD_RATE), ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();
        for (auto& day : mDays) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(FormatDate(day.dayKey).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(FormatAmount(day.total).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(FormatAmount(day.got).c_str());
            ImGui::TableNextColumn();
            if (day.total > 0)
                ImGui::Text("%.2f%%", (double)day.got * 100.0 / (double)day.total);
            else
                ImGui::TextUnformatted("—");
        }
        ImGui::EndTable();
    }
}

void SingleCardTabGui::OpenConfigList()
{
    mConfigDialogMode = ConfigDialogMode::List;
    mConfigDialogReturnToList = false;
    mEditingConfigIndex = -1;
    mPendingDeleteIndex = -1;
    mConfigError.clear();
    mConfigDialogRequested = true;
}

void SingleCardTabGui::OpenConfigEditor(int index, bool returnToList)
{
    mConfigDialogMode = ConfigDialogMode::Edit;
    mConfigDialogReturnToList = returnToList;
    mEditingConfigIndex = index;
    mPendingDeleteIndex = -1;
    mConfigError.clear();

    if (index >= 0 && index < (int)mConfigs.size() && mConfigs[index].game == mGame) {
        const SingleCardConfig& config = mConfigs[index];
        CopyTextBuffer(mConfigName, config.name);
        CopyTextBuffer(mConfigChapter, config.chapter);
    } else {
        mEditingConfigIndex = -1;
        mConfigName[0] = '\0';
        CopyTextBuffer(mConfigChapter, mChapterIndex >= 0 && mChapterIndex < (int)mCheckpoints.size()
                ? mCheckpoints[mChapterIndex]
                : std::string());
    }
    mConfigDialogRequested = true;
}

void SingleCardTabGui::DrawConfigDialog()
{
    constexpr const char* kPopupId = "###single_card_config";
    if (mConfigDialogRequested) {
        if (!ImGui::IsPopupOpen(kPopupId))
            ImGui::OpenPopup(kPopupId);
        mConfigDialogRequested = false;
    }

    const bool editingExisting = mConfigDialogMode == ConfigDialogMode::Edit && mEditingConfigIndex >= 0;
    const char* titleKey = mConfigDialogMode == ConfigDialogMode::List
        ? S(THPRAC_DATA_SINGLE_RUN_CONFIG_LIST)
        : (editingExisting ? S(THPRAC_DATA_SINGLE_RUN_CONFIG_EDIT) : S(THPRAC_DATA_SINGLE_RUN_CONFIG_SAVE));
    std::string title = std::string(titleKey) + kPopupId;
    if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (mConfigDialogMode == ConfigDialogMode::List)
            DrawConfigList();
        else
            DrawConfigEditor();
        ImGui::EndPopup();
    }
}

void SingleCardTabGui::DrawConfigList()
{
    ImGui::TextDisabled("%s: %s", S(THPRAC_GAMES), GameDisplayName(mGame.c_str()));
    ImGui::Separator();

    ImGui::BeginChild("##single_card_config_list", ImVec2(520.0f, 300.0f), true);
    if (mPendingDeleteIndex >= 0 && mPendingDeleteIndex < (int)mConfigs.size()) {
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), S(THPRAC_DATA_SINGLE_RUN_CONFIG_DELETE_CONFIRM),
            mConfigs[mPendingDeleteIndex].name.c_str());
        ImGui::Separator();
    }

    int visibleCount = 0;
    if (ImGui::BeginTable("##single_card_config_rows", 3, ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("##chapter", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 190.0f);
        for (int i = 0; i < (int)mConfigs.size(); ++i) {
            const SingleCardConfig& config = mConfigs[i];
            if (config.game != mGame)
                continue;
            ++visibleCount;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(config.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(config.chapter.c_str());
            ImGui::TableNextColumn();
            if (mPendingDeleteIndex == i) {
                if (ImGui::SmallButton(S(THPRAC_GAMES_DELETE))) {
                    DeleteConfig(i);
                    ImGui::PopID();
                    break;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(S(THPRAC_CANCEL)))
                    mPendingDeleteIndex = -1;
            } else {
                if (ImGui::SmallButton(S(THPRAC_DATA_SINGLE_RUN_CONFIG_LOAD))) {
                    LoadConfig(i);
                    ImGui::PopID();
                    break;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(S(THPRAC_DATA_SINGLE_RUN_CONFIG_EDIT_ACTION)))
                    OpenConfigEditor(i, true);
                ImGui::SameLine();
                if (ImGui::SmallButton(S(THPRAC_GAMES_DELETE)))
                    mPendingDeleteIndex = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (visibleCount == 0)
        ImGui::TextDisabled("%s", S(THPRAC_DATA_SINGLE_RUN_CONFIG_EMPTY));
    if (!mConfigError.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "%s", mConfigError.c_str());
    ImGui::EndChild();

    if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_NEW)))
        OpenConfigEditor(-1, true);
    ImGui::SameLine();
    if (ImGui::Button(S(THPRAC_CLOSE)))
        ImGui::CloseCurrentPopup();
}

void SingleCardTabGui::DrawConfigEditor()
{
    ImGui::TextDisabled("%s: %s", S(THPRAC_GAMES), GameDisplayName(mGame.c_str()));
    ImGui::Separator();

    ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_CONFIG_NAME));
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(380.0f);
    ImGui::InputText("##single_card_config_name", mConfigName, sizeof(mConfigName));

    ImGui::TextUnformatted(S(TH_CHAPTER));
    ImGui::SameLine(140.0f);
    ImGui::SetNextItemWidth(380.0f);
    ImGui::InputText("##single_card_config_chapter", mConfigChapter, sizeof(mConfigChapter));

    if (!mConfigError.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "%s", mConfigError.c_str());

    if (ImGui::Button(S(THPRAC_DATA_SINGLE_RUN_CONFIG_SAVE_ACTION)))
        SaveConfigFromEditor();
    ImGui::SameLine();
    if (ImGui::Button(S(THPRAC_CANCEL))) {
        if (mConfigDialogReturnToList) {
            mConfigDialogMode = ConfigDialogMode::List;
            mEditingConfigIndex = -1;
            mConfigError.clear();
        } else {
            ImGui::CloseCurrentPopup();
        }
    }
}

void SingleCardTabGui::SaveConfigFromEditor()
{
    const std::string name = TrimText(mConfigName);
    const std::string chapter = TrimText(mConfigChapter);
    if (name.empty()) {
        mConfigError = S(THPRAC_LINKS_EDIT_ERR_NAME);
        return;
    }
    if (IsConfigNameTaken(name, mEditingConfigIndex)) {
        mConfigError = S(THPRAC_DATA_SINGLE_RUN_CONFIG_ERR_DUP);
        return;
    }
    if (FindCheckpointIndex(mCheckpoints, chapter, false) < 0) {
        mConfigError = S(THPRAC_DATA_SINGLE_CARD_CONFIG_ERR_CHAPTER);
        return;
    }

    std::vector<SingleCardConfig> next = mConfigs;
    SingleCardConfig config { name, mGame, chapter };
    if (mEditingConfigIndex >= 0 && mEditingConfigIndex < (int)next.size()
        && next[mEditingConfigIndex].game == mGame) {
        next[mEditingConfigIndex] = std::move(config);
    } else {
        next.push_back(std::move(config));
    }
    if (!WriteSingleCardConfigs(mDataDir, next)) {
        mConfigError = S(THPRAC_DATA_SINGLE_RUN_CONFIG_WRITE_FAILED);
        return;
    }
    mConfigs = std::move(next);
    mEditingConfigIndex = -1;
    mConfigError.clear();
    if (mConfigDialogReturnToList)
        mConfigDialogMode = ConfigDialogMode::List;
    else
        ImGui::CloseCurrentPopup();
}

void SingleCardTabGui::LoadConfig(int index)
{
    if (index < 0 || index >= (int)mConfigs.size())
        return;
    const SingleCardConfig& config = mConfigs[index];
    if (config.game != mGame)
        return;
    const int chapterIndex = FindCheckpointIndex(mCheckpoints, config.chapter, false);
    if (chapterIndex < 0) {
        mConfigError = S(THPRAC_DATA_SINGLE_CARD_CONFIG_ERR_CHAPTER);
        return;
    }
    mChapterIndex = chapterIndex;
    mPendingDeleteIndex = -1;
    mConfigError.clear();
    RecomputeDays();
    ImGui::CloseCurrentPopup();
}

void SingleCardTabGui::DeleteConfig(int index)
{
    if (index < 0 || index >= (int)mConfigs.size())
        return;
    std::vector<SingleCardConfig> next = mConfigs;
    next.erase(next.begin() + index);
    if (!WriteSingleCardConfigs(mDataDir, next)) {
        mConfigError = S(THPRAC_DATA_SINGLE_RUN_CONFIG_WRITE_FAILED);
        return;
    }
    mConfigs = std::move(next);
    mPendingDeleteIndex = -1;
    mEditingConfigIndex = -1;
    mConfigError.clear();
}

bool SingleCardTabGui::IsConfigNameTaken(const std::string& name, int ignoreIndex) const
{
    for (int i = 0; i < (int)mConfigs.size(); ++i) {
        if (i == ignoreIndex)
            continue;
        if (mConfigs[i].game == mGame && mConfigs[i].name == name)
            return true;
    }
    return false;
}

SingleCardTabGui gSingleCardTab;

// ---- 全关 ----------------------------------------------------------------
// 全关判定与 hmaker 全关页一致：起点 = checkpoint 表第一条记录名，
// 终点 = 表中最后一条以 "ST6" 开头的记录名（行序最后，不是字符串排序）。
struct FullRunTable {
    std::vector<std::string> names;
    std::string first;
    std::vector<std::string> endCheckpoints;
    bool valid = false;
};

// th16 的 ST6 末尾有 SPRING/SUMMER/AUTUMN/WINTER 四个最终关，均可作为全关终点；
// 其余作品仍只取行序最后一条 ST6。
static constexpr int kTh16FullRunEndCount = 4;

static bool LoadFullRunTable(const std::string& game, FullRunTable& table)
{
    table = {};
    const CheckpointSource* source = FindCheckpointSource(game);
    if (!source || !LoadCheckpointNames(source->csv, table.names) || table.names.empty())
        return false;

    table.first = table.names.front();
    const int endCount = game == "th16" ? kTh16FullRunEndCount : 1;
    for (auto it = table.names.rbegin(); it != table.names.rend()
         && (int)table.endCheckpoints.size() < endCount; ++it) {
        if (it->compare(0, 3, "ST6") != 0)
            continue;
        if (std::find(table.endCheckpoints.begin(), table.endCheckpoints.end(), *it) == table.endCheckpoints.end())
            table.endCheckpoints.push_back(*it);
    }
    table.valid = !table.first.empty() && !table.endCheckpoints.empty();
    return table.valid;
}

static bool IsFullRunRecord(const SessionRecord& record, const FullRunTable& table)
{
    return table.valid
        && record.startCheckpoint == table.first
        && std::find(table.endCheckpoints.begin(), table.endCheckpoints.end(), record.endCheckpoint) != table.endCheckpoints.end();
}

static void BuildFullRunGameList(const std::vector<const SessionRecord*>& records,
    std::vector<std::string>& gameIds)
{
    std::set<std::string> present;
    for (auto* record : records)
        present.insert(record->game);

    gameIds.clear();
    for (auto& gameDef : gGameDefs) {
        auto it = present.find(gameDef.idStr);
        if (it != present.end()) {
            gameIds.push_back(*it);
            present.erase(it);
        }
    }
    gameIds.insert(gameIds.end(), present.begin(), present.end());
}

struct FullRunChapterRate {
    std::string name;
    double rate = 0.0;
};

struct FullRunDay {
    int64_t dayKey = 0;
    std::vector<const SessionRecord*> records;
};

struct FullRunStats {
    int recordCount = 0;
    int totalEvents = 0;
    double avgMiss = 0.0;
    double totalRate = 100.0;
    std::vector<FullRunChapterRate> chapterRates;
    std::vector<FullRunDay> days;
};

// 通过率按「该章节出过 event 的记录数」计：同一局同一章节多个 event 只算一次失败。
static void BuildFullRunStats(const std::vector<const SessionRecord*>& records,
    const std::vector<std::string>& chapters, FullRunStats& stats)
{
    stats = {};
    stats.recordCount = (int)records.size();

    std::map<std::string, int> hitCounts;
    for (auto* record : records) {
        stats.totalEvents += (int)record->events.size();
        std::set<std::string> hit;
        for (auto& event : record->events) {
            if (!event.checkpoint.empty())
                hit.insert(event.checkpoint);
        }
        for (auto& name : hit)
            ++hitCounts[name];
    }
    stats.avgMiss = stats.recordCount == 0
        ? 0.0
        : (double)stats.totalEvents / (double)stats.recordCount;

    for (auto& name : chapters) {
        auto it = hitCounts.find(name);
        if (it == hitCounts.end())
            continue;
        const double rate = (double)(stats.recordCount - it->second) * 100.0 / (double)stats.recordCount;
        stats.chapterRates.push_back({ name, rate });
        hitCounts.erase(it);
    }
    for (auto& [name, hitCount] : hitCounts) {
        const double rate = (double)(stats.recordCount - hitCount) * 100.0 / (double)stats.recordCount;
        stats.chapterRates.push_back({ name, rate });
    }

    stats.totalRate = 100.0;
    for (auto& rate : stats.chapterRates)
        stats.totalRate *= rate.rate / 100.0;

    for (auto* record : records) {
        int year = 0, month = 0, day = 0;
        if (!ParseYmdPrefix(record->start, year, month, day))
            continue;
        const int64_t dayKey = DateKey(year, month, day);
        if (stats.days.empty() || stats.days.back().dayKey != dayKey)
            stats.days.push_back({ dayKey, {} });
        stats.days.back().records.push_back(record);
    }
}

static std::string FullRunEventText(const SessionRecord& record)
{
    std::string text;
    for (auto& event : record.events) {
        if (event.type.empty() && event.checkpoint.empty())
            continue;
        if (!text.empty())
            text += "  |  ";
        if (!event.type.empty()) {
            text += event.type;
            if (!event.checkpoint.empty())
                text += ": ";
        }
        text += event.checkpoint;
    }
    return text;
}

class FullRunTabGui {
public:
    void Gui();

private:
    void EnsureInitialized();
    void Refresh();
    void LoadSelectedMonth();
    void BuildFullRuns();
    void SelectGame(const std::string& game);
    void SelectMonth(const std::string& month);
    void RebuildStats();
    void DrawHeader();
    void DrawFilters();
    void DrawDays();
    void DrawRecord(const SessionRecord& record);
    void DrawPassRateDialog();

    bool mInitialized = false;
    bool mNoFiles = false;
    bool mReadFailed = false;
    bool mFallback = false;
    bool mPassRateDialogRequested = false;

    std::wstring mDataDir;
    std::vector<std::string> mMonths;
    std::string mSelectedMonth;
    std::string mSelectedGame;
    std::vector<SessionRecord> mMonthRecords;
    std::vector<const SessionRecord*> mFullRuns;
    std::vector<const SessionRecord*> mFiltered;
    std::vector<std::string> mGameIds;
    std::vector<std::string> mChapterNames;
    FullRunStats mStats;
};

FullRunTabGui gFullRunTab;

void FullRunTabGui::Gui()
{
    EnsureInitialized();

    DrawHeader();
    if (!mNoFiles)
        DrawFilters();
    ImGui::Separator();

    if (mNoFiles) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_NO_FILE));
    } else if (mMonthRecords.empty() && mReadFailed) {
        ImGui::TextWrapped("%s", S(THPRAC_DATA_READ_FAILED));
    } else {
        if (mReadFailed)
            ImGui::TextDisabled("%s", S(THPRAC_DATA_READ_FAILED));
        if (mFullRuns.empty())
            ImGui::TextWrapped("%s", S(THPRAC_DATA_FULL_RUN_EMPTY_MONTH));
        else if (mFiltered.empty())
            ImGui::TextWrapped("%s", S(THPRAC_DATA_FULL_RUN_EMPTY_FILTER));
        else {
            ImGui::BeginChild("##full_run_body", ImVec2(0.0f, 0.0f), false);
            DrawDays();
            ImGui::EndChild();
        }
    }

    DrawPassRateDialog();
}

void FullRunTabGui::EnsureInitialized()
{
    if (mInitialized)
        return;
    mInitialized = true;
    mDataDir = LauncherGetDataDir();
    Refresh();
}

void FullRunTabGui::Refresh()
{
    mDataDir = LauncherGetDataDir();
    ListMonthFiles(mDataDir, mMonths);
    mNoFiles = mMonths.empty();
    if (mNoFiles) {
        mSelectedMonth.clear();
        mSelectedGame.clear();
        mMonthRecords.clear();
        mFullRuns.clear();
        mFiltered.clear();
        mGameIds.clear();
        mChapterNames.clear();
        mStats = {};
        mReadFailed = false;
        mFallback = false;
        return;
    }

    const std::string currentMonth = CurrentMonthKey();
    const bool keepSelection = !mSelectedMonth.empty()
        && std::find(mMonths.begin(), mMonths.end(), mSelectedMonth) != mMonths.end();
    if (keepSelection) {
        mFallback = false;
    } else {
        if (std::find(mMonths.begin(), mMonths.end(), currentMonth) != mMonths.end())
            mSelectedMonth = currentMonth;
        else
            mSelectedMonth = mMonths.front();
        mFallback = mSelectedMonth != currentMonth;
    }

    LoadSelectedMonth();
}

void FullRunTabGui::LoadSelectedMonth()
{
    mFullRuns.clear();
    mFiltered.clear();
    mChapterNames.clear();
    mStats = {};
    mReadFailed = false;
    if (mSelectedMonth.empty())
        mMonthRecords.clear();
    else
        LoadMonthSessionRecords(mDataDir, mSelectedMonth, mMonthRecords, mReadFailed);

    BuildFullRuns();
    if (!mSelectedGame.empty()
        && std::find(mGameIds.begin(), mGameIds.end(), mSelectedGame) == mGameIds.end())
        mSelectedGame.clear();
    RebuildStats();
}

void FullRunTabGui::BuildFullRuns()
{
    mFullRuns.clear();
    mGameIds.clear();

    std::map<std::string, FullRunTable> tables;
    for (auto& record : mMonthRecords) {
        auto it = tables.find(record.game);
        if (it == tables.end()) {
            FullRunTable table;
            LoadFullRunTable(record.game, table);
            it = tables.emplace(record.game, std::move(table)).first;
        }
        if (IsFullRunRecord(record, it->second))
            mFullRuns.push_back(&record);
    }
    BuildFullRunGameList(mFullRuns, mGameIds);
}

void FullRunTabGui::SelectGame(const std::string& game)
{
    if (game == mSelectedGame)
        return;
    mSelectedGame = game;
    RebuildStats();
}

void FullRunTabGui::SelectMonth(const std::string& month)
{
    if (month == mSelectedMonth)
        return;
    mSelectedMonth = month;
    mFallback = false;
    LoadSelectedMonth();
}

void FullRunTabGui::RebuildStats()
{
    mFiltered.clear();
    for (auto* record : mFullRuns) {
        if (mSelectedGame.empty() || record->game == mSelectedGame)
            mFiltered.push_back(record);
    }

    mChapterNames.clear();
    if (!mSelectedGame.empty()) {
        const CheckpointSource* source = FindCheckpointSource(mSelectedGame);
        if (source)
            LoadCheckpointNames(source->csv, mChapterNames);
    }
    BuildFullRunStats(mFiltered, mChapterNames, mStats);
}

void FullRunTabGui::DrawHeader()
{
    if (ImGui::Button(S(THPRAC_DATA_REFRESH)))
        Refresh();
    ImGui::SameLine();
    if (!mSelectedMonth.empty())
        ImGui::TextDisabled("%s", FormatMonthLabel(mSelectedMonth).c_str());

    if (!mSelectedGame.empty()) {
        char recordText[128] {};
        std::snprintf(recordText, sizeof(recordText), "%s: %d",
            S(THPRAC_DATA_FULL_RUN_RECORD_COUNT), mStats.recordCount);
        char avgText[128] {};
        std::snprintf(avgText, sizeof(avgText), "%s: %.2f",
            S(THPRAC_DATA_FULL_RUN_AVG_MISS), mStats.avgMiss);
        const char* buttonText = S(THPRAC_DATA_FULL_RUN_PASS_RATE);
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float statsWidth = ImGui::CalcTextSize(recordText).x
            + ImGui::CalcTextSize(avgText).x
            + ImGui::CalcTextSize(buttonText).x
            + ImGui::GetStyle().FramePadding.x * 2.0f
            + spacing * 4.0f;
        const float right = ImGui::GetContentRegionMax().x - statsWidth;
        if (right > ImGui::GetCursorPosX())
            ImGui::SameLine(right);
        else
            ImGui::SameLine();
        ImGui::TextUnformatted(recordText);
        ImGui::SameLine();
        ImGui::TextUnformatted(avgText);
        ImGui::SameLine();
        if (mStats.recordCount == 0) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            ImGui::Button(buttonText);
            ImGui::PopStyleVar();
        } else if (ImGui::Button(buttonText)) {
            mPassRateDialogRequested = true;
        }
    }
}

void FullRunTabGui::DrawFilters()
{
    ImGui::TextUnformatted(S(THPRAC_GAMES));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    const char* gamePreview = mSelectedGame.empty()
        ? S(THPRAC_GAME_TIME_ALL)
        : GameDisplayName(mSelectedGame.c_str());
    if (ImGui::BeginCombo("##full_run_game", gamePreview)) {
        if (ImGui::Selectable(S(THPRAC_GAME_TIME_ALL), mSelectedGame.empty()))
            SelectGame(std::string());
        for (auto& gameId : mGameIds) {
            const bool selected = gameId == mSelectedGame;
            if (ImGui::Selectable(GameDisplayName(gameId.c_str()), selected))
                SelectGame(gameId);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(S(THPRAC_DATA_MONTH));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    std::string monthPreview = FormatMonthLabel(mSelectedMonth);
    if (ImGui::BeginCombo("##full_run_month", monthPreview.c_str())) {
        for (auto& month : mMonths) {
            const bool selected = month == mSelectedMonth;
            std::string label = FormatMonthLabel(month);
            if (ImGui::Selectable(label.c_str(), selected))
                SelectMonth(month);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (mFallback) {
        std::string monthLabel = FormatMonthLabel(mSelectedMonth);
        ImGui::TextDisabled(S(THPRAC_DATA_PRACTICE_FALLBACK), monthLabel.c_str());
    }
}

void FullRunTabGui::DrawDays()
{
    for (size_t i = 0; i < mStats.days.size(); ++i) {
        auto& day = mStats.days[i];
        SysDays date { std::chrono::days { day.dayKey } };
        int isoWeekday = (int)std::chrono::weekday { date }.iso_encoding();
        std::string dateLabel = std::format("{} {}", FormatDate(day.dayKey), WeekdayName(isoWeekday));

        ImGui::PushID((int)i);
        ImGui::TextUnformatted(dateLabel.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled(S(THPRAC_DATA_PRACTICE_DAY_COUNT), (int)day.records.size());
        ImGui::Separator();

        ImGui::Indent(12.0f);
        for (auto* record : day.records)
            DrawRecord(*record);
        ImGui::Unindent(12.0f);

        ImGui::Spacing();
        ImGui::PopID();
    }
}

void FullRunTabGui::DrawRecord(const SessionRecord& record)
{
    ImGui::TextUnformatted(GameDisplayName(record.game.c_str()));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", record.start.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(S(THPRAC_DATA_FULL_RUN_EVENT_COUNT), (int)record.events.size());

    ImGui::Indent(12.0f);
    const std::string events = FullRunEventText(record);
    if (events.empty())
        ImGui::TextDisabled("%s", S(THPRAC_DATA_FULL_RUN_NO_EVENT));
    else
        ImGui::TextWrapped("%s", events.c_str());
    ImGui::Unindent(12.0f);
    ImGui::Spacing();
}

void FullRunTabGui::DrawPassRateDialog()
{
    constexpr const char* kPopupId = "###full_run_pass_rate";
    if (mPassRateDialogRequested) {
        if (!ImGui::IsPopupOpen(kPopupId))
            ImGui::OpenPopup(kPopupId);
        mPassRateDialogRequested = false;
    }

    std::string title = std::string(S(THPRAC_DATA_FULL_RUN_PASS_RATE)) + kPopupId;
    if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("%s: %s", S(THPRAC_GAMES), GameDisplayName(mSelectedGame.c_str()));
        ImGui::Text("%s: %d", S(THPRAC_DATA_FULL_RUN_RECORD_COUNT), mStats.recordCount);
        ImGui::Text("%s: %.2f", S(THPRAC_DATA_FULL_RUN_AVG_MISS), mStats.avgMiss);
        ImGui::Separator();
        ImGui::TextUnformatted(S(THPRAC_DATA_SINGLE_RUN_CHAPTER_RATES));

        ImGui::BeginChild("##full_run_rate_list", ImVec2(560.0f, 360.0f), true);
        if (mStats.chapterRates.empty()) {
            ImGui::TextDisabled("%s", S(THPRAC_DATA_FULL_RUN_NO_EVENT));
        } else if (ImGui::BeginTable("##full_run_rates", 3,
                       ImGuiTableFlags_::ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_::ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 300.0f);
            ImGui::TableSetupColumn("##bar", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##rate", ImGuiTableColumnFlags_::ImGuiTableColumnFlags_WidthFixed, 80.0f);
            for (auto& rate : mStats.chapterRates) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(rate.name.c_str());
                ImGui::TableNextColumn();
                ImGui::ProgressBar((float)std::clamp(rate.rate / 100.0, 0.0, 1.0), ImVec2(-1.0f, 6.0f), "");
                ImGui::TableNextColumn();
                ImGui::Text("%.2f%%", rate.rate);
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram),
            "%s: %.2f%%", S(THPRAC_DATA_FULL_RUN_TOTAL_PASS_RATE), mStats.totalRate);
        if (ImGui::Button(S(THPRAC_CLOSE)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace

bool LauncherDataGuiUpd()
{
    if (ImGui::BeginTabBar("##data_subtabs")) {
        if (ImGui::BeginTabItem(S(THPRAC_DATA_PRACTICE))) {
            gPracticeTab.Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(S(THPRAC_DATA_FULL_RUN))) {
            gFullRunTab.Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(S(THPRAC_DATA_SINGLE_RUN))) {
            gSingleRunTab.Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(S(THPRAC_DATA_SINGLE_CARD))) {
            gSingleCardTab.Gui();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    return true;
}

}

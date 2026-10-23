#include "thprac_launcher_data.h"
#include "thprac_launcher_cfg.h"
#include "thprac_launcher_games.h"
#include "thprac_launcher_games_def.h"
#include "thprac_gui_locale.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <imgui.h>
#pragma warning(push)
#pragma warning(disable : 26451)
#pragma warning(disable : 26495)
#pragma warning(disable : 33010)
#include <rapidjson/document.h>
#pragma warning(pop)

namespace THPrac {
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

static bool ReadMonthRecords(const std::wstring& dataDir, const std::string& month,
    std::vector<PracticeRecord>& records, bool& readFailed)
{
    records.clear();
    readFailed = false;
    if (dataDir.empty() || month.size() != 6) {
        readFailed = true;
        return false;
    }

    std::wstring fileName = L"logger_sessions_";
    for (char c : month)
        fileName.push_back((wchar_t)c);
    fileName += L".jsonl";

    std::wstring path = JoinDataDir(dataDir) + fileName;
    void* buffer = nullptr;
    size_t size = 0;
    if (!LoadJsonFile(path, buffer, size)) {
        readFailed = true;
        return false;
    }
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

        if (length) {
            PracticeRecord record;
            if (ParsePracticeRecord(cursor, length, record))
                records.push_back(std::move(record));
        }

        if (!newline)
            break;
        cursor = newline + 1;
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
    int64_t maxDaily = 0;
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
    for (auto& [dayKey, day] : days) {
        if (day.total > stats.maxDaily)
            stats.maxDaily = day.total;
    }

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

    float ratio = (mStats.maxDaily > 0) ? (float)day.total / (float)mStats.maxDaily : 0.0f;
    ratio = std::clamp(ratio, 0.0f, 1.0f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    constexpr float kBarHeight = 5.0f;
    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + kBarHeight),
        ImGui::GetColorU32(ImGuiCol_FrameBg), 2.5f);
    if (ratio > 0.0f) {
        drawList->AddRectFilled(pos, ImVec2(pos.x + width * ratio, pos.y + kBarHeight),
            ImGui::GetColorU32(ImGuiCol_PlotHistogram), 2.5f);
    }
    ImGui::Dummy(ImVec2(width, kBarHeight + 6.0f));
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
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(S(THPRAC_DATA_SINGLE_RUN))) {
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(S(THPRAC_DATA_SINGLE_CARD))) {
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    return true;
}

}

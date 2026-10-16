#include "thprac_th16_logger.h"

#include "thprac_games.h"

#include <cstdio>
#include <sstream>

#include "..\3rdParties\rapidcsv\rapidcsv.h"

namespace THPrac {
namespace TH16 {

    namespace {
        // ECL manager + stage number globals.
        constexpr uintptr_t kEclMgrPtr = 0x4A6DC0;
        constexpr uintptr_t kStageNum = 0x4A5790;

        // mgr -> host chain; host -> thread chain; host -> own ECL resource.
        constexpr uintptr_t kMgrHostHeadOff = 0x180;
        constexpr uintptr_t kHostThreadOff = 0x11FC;
        constexpr uintptr_t kHostResOff = 0x11F8;

        // res -> sub count / sub table; thread -> current sub index.
        constexpr uintptr_t kResCountOff = 8;
        constexpr uintptr_t kResTableOff = 0x8C;
        constexpr uintptr_t kThreadSubOff = 4;

        // ECL sub naming convention: MainSub* = mid-stage chapter, MBoss* =
        // mid-boss, Boss* = stage boss. Classified by the whitelist key itself.
        constexpr int kKindChapter = 0;
        constexpr int kKindMidBoss = 1;
        constexpr int kKindBoss = 2;

        int ClassifySub(const char* name)
        {
            if (std::strncmp(name, "MBoss", 5) == 0) {
                return kKindMidBoss;
            }
            if (std::strncmp(name, "Boss", 4) == 0) {
                return kKindBoss;
            }
            return kKindChapter;
        }

        // Sub names that end a mid-boss fight (from reasm/exes/ecl_dump/th16/*mbs*.decl;
        // every mid-boss script has exactly two setBoss(-1) sites, both inside these).
        // Prefixes must stay exact: a loose "contains Dead" test would also match
        // st07mbs' MBossCard1_Dead / MBossCard2_Dead, which are spell-card end
        // animations registered via setDeath() that run while the mid-boss lives.
        bool IsMidBossEndSub(const char* name)
        {
            return std::strncmp(name, "MBossDead", 9) == 0
                || std::strncmp(name, "MBossEscape", 11) == 0
                || std::strncmp(name, "MBossBDead", 10) == 0
                || std::strncmp(name, "MBossBEscape", 12) == 0
                || std::strncmp(name, "BossDeadB", 9) == 0;
        }

        // Hard traversal caps; also act as sanity sentinels for corrupted pointers.
        constexpr int32_t kMaxCount = 8192;
        constexpr int kMaxHosts = 1024;
        constexpr int kMaxThreads = 64;
    }

    TH16Logger::TH16Logger()
    {
        LoadCsv();
    }

    void TH16Logger::LoadCsv()
    {
        try {
            std::istringstream ss(kTh16CheckpointsCsv);
            rapidcsv::Document doc(ss, rapidcsv::LabelParams(0, -1));
            if (doc.GetColumnCount() < 4) {
                return;
            }
            const size_t rows = doc.GetRowCount();
            for (size_t r = 0; r < rows; ++r) {
                auto row = doc.GetRow<std::string>(r);
                if (row.size() < 4) {
                    continue;
                }
                if (row[1] != "th16") {
                    continue;
                }
                mEntries.push_back(Entry { row[0], row[2], row[3] });
            }
        } catch (...) {
            // A malformed table just means we never latch a section.
            mEntries.clear();
        }
    }

    void TH16Logger::BuildStageMap(int32_t stageNum)
    {
        mStageMap.clear();
        mBuiltStage = stageNum;

        char stageKey[16];
        std::snprintf(stageKey, sizeof(stageKey), "ST%d", stageNum);
        for (auto& e : mEntries) {
            if (e.stage == stageKey) {
                mStageMap.emplace(e.name.c_str(), e.record.c_str());
            }
        }
    }

    void TH16Logger::OnStageStart()
    {
        // No allocation: this runs inside a VEH hook callback.
        mSection = "main";
        mPending = nullptr;
        mMidBossActive = false;
        mMidBossDone = false;
    }

    void TH16Logger::Update()
    {
        int32_t stageNum = GetMemContent<int32_t>(kStageNum);
        if (stageNum == 9) {
            // Extra is reported as 9 by some code paths, 7 by others.
            stageNum = 7;
        }
        if (stageNum < 1 || stageNum > 7) {
            // Menu / not in a stage: keep the last value untouched.
            return;
        }
        if (stageNum != mBuiltStage) {
            BuildStageMap(stageNum);
            mSection = "main";
            // mPending aliases a frozen mEntries string, so a stale one is never
            // dangling - but it would name a record from the previous stage.
            mPending = nullptr;
            mMidBossActive = false;
            mMidBossDone = false;
        }
        if (mStageMap.empty()) {
            return;
        }

        uintptr_t mgr = GetMemContent<uintptr_t>(kEclMgrPtr);
        if (!mgr) {
            return;
        }

        // One candidate per kind; the chapter one keeps the old "last hit wins".
        const char* midHit = nullptr;
        const char* bossHit = nullptr;
        const char* chapHit = nullptr;
        bool sawMidBossEnd = false;

        // Nothing is cached across frames: the whole chain is re-walked every tick.
        uintptr_t node = GetMemContent<uintptr_t>(mgr + kMgrHostHeadOff);
        for (int hi = 0; node && hi < kMaxHosts; ++hi) {
            uintptr_t host = GetMemContent<uintptr_t>(node);
            uintptr_t next = GetMemContent<uintptr_t>(node + 4);
            node = next;
            if (!host) {
                continue;
            }
            // Each host snapshots its own ECL resource; the global mgr+0x17C is
            // a different table (boss stages load stNNbs) and must not be used.
            uintptr_t res = GetMemContent<uintptr_t>(host + kHostResOff);
            if (!res) {
                continue;
            }
            int32_t count = GetMemContent<int32_t>(res + kResCountOff);
            uintptr_t table = GetMemContent<uintptr_t>(res + kResTableOff);
            if (!table || count <= 0 || count > kMaxCount) {
                continue;
            }
            uintptr_t tn = host + kHostThreadOff;
            for (int ti = 0; tn && ti < kMaxThreads; ++ti) {
                uintptr_t thread = GetMemContent<uintptr_t>(tn);
                uintptr_t tnext = GetMemContent<uintptr_t>(tn + 4);
                tn = tnext;
                if (!thread) {
                    continue;
                }
                int32_t subIdx = GetMemContent<int32_t>(thread + kThreadSubOff);
                if (subIdx < 0 || subIdx >= count) {
                    continue;
                }
                const char* name = GetMemContent<const char*>(table + (uintptr_t)subIdx * 8);
                if (!name) {
                    continue;
                }
                if (IsMidBossEndSub(name)) {
                    sawMidBossEnd = true;
                }
                auto it = mStageMap.find(name);
                if (it != mStageMap.end()) {
                    // Last hit wins per kind: the host list is tail-inserted, so
                    // walking it head-first ends on the newest host.
                    switch (ClassifySub(name)) {
                    case kKindMidBoss:
                        midHit = it->second;
                        break;
                    case kKindBoss:
                        bossHit = it->second;
                        break;
                    default:
                        chapHit = it->second;
                        break;
                    }
                }
            }
        }

        // A mid-boss fight ends when its death/escape sub runs; whatever chapter
        // checkpoints the main thread crossed meanwhile were reinforcement sections
        // of the latter half and are replayed now. mMidBossDone also keeps a late
        // hit on the entry sub from re-opening the window for the rest of the stage.
        if (sawMidBossEnd) {
            if (mMidBossActive) {
                mMidBossActive = false;
                if (mPending) {
                    mSection = mPending;
                    mPending = nullptr;
                }
            }
            mMidBossDone = true;
        }

        if (bossHit) {
            // The stage boss cannot coexist with a live mid-boss.
            mMidBossActive = false;
            mPending = nullptr;
            mSection = bossHit;
        } else if (midHit && !mMidBossDone) {
            if (!mMidBossActive) {
                mMidBossActive = true;
                mPending = nullptr;
            }
            mSection = midHit;
        } else if (chapHit) {
            if (mMidBossActive) {
                mPending = chapHit;
            } else {
                mSection = chapHit;
            }
        }
    }

}
}

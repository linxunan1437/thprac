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
        }
        if (mStageMap.empty()) {
            return;
        }

        uintptr_t mgr = GetMemContent<uintptr_t>(kEclMgrPtr);
        if (!mgr) {
            return;
        }

        // Nothing is cached across frames: the whole chain is re-walked every tick.
        uintptr_t node = GetMemContent<uintptr_t>(mgr + kMgrHostHeadOff);
        for (int hi = 0; node && hi < kMaxHosts; ++hi) {
            uintptr_t host = GetMemContent<uintptr_t>(node);
            uintptr_t next = GetMemContent<uintptr_t>(node + 4);
            if (host) {
                // Each host snapshots its own ECL resource; the global mgr+0x17C is
                // a different table (boss stages load stNNbs) and must not be used.
                uintptr_t res = GetMemContent<uintptr_t>(host + kHostResOff);
                if (res) {
                    int32_t count = GetMemContent<int32_t>(res + kResCountOff);
                    uintptr_t table = GetMemContent<uintptr_t>(res + kResTableOff);
                    if (table && count > 0 && count <= kMaxCount) {
                        uintptr_t tn = host + kHostThreadOff;
                        for (int ti = 0; tn && ti < kMaxThreads; ++ti) {
                            uintptr_t thread = GetMemContent<uintptr_t>(tn);
                            uintptr_t tnext = GetMemContent<uintptr_t>(tn + 4);
                            if (thread) {
                                int32_t subIdx = GetMemContent<int32_t>(thread + kThreadSubOff);
                                if (subIdx >= 0 && subIdx < count) {
                                    const char* name = GetMemContent<const char*>(table + (uintptr_t)subIdx * 8);
                                    if (name) {
                                        auto it = mStageMap.find(name);
                                        if (it != mStageMap.end()) {
                                            // Last hit wins: the host list is tail-inserted, so
                                            // walking it head-first ends on the newest host.
                                            mSection = it->second;
                                        }
                                    }
                                }
                            }
                            tn = tnext;
                        }
                    }
                }
            }
            node = next;
        }
    }

}
}

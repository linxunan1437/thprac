#include "thprac_games.h"
#include "thprac_utils.h"
#include "..\3rdParties\rapidcsv\rapidcsv.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>
#include <sstream>
#include <string_view>
#include <unordered_map>


namespace THPrac {
namespace TH12 {
    int g_lock_timer = 0;
    bool g_lock_timer_flag = false;

    bool g_show_bullet_hitbox = false;
    struct laser_hitbox_draw {
        ImVec2 posA;
        ImVec2 posB;
        ImVec2 posC;
        ImVec2 posD;
    };
    std::vector<laser_hitbox_draw> g_th12_laser_hit_draw_vec;

    constexpr uint32_t playerDmgSrcCnt = 0x80;

    struct PlayerDamageSource {
        char gap0[0x70];
        uint32_t flags; // 0x70
    };

    struct Player {
        char gap0[35208];
        PlayerDamageSource damage_sources[129];
        char gapC3FC[4];
        Timer iframes;
        char gapC414[388];
        int32_t field_C598;
    };

    static_assert(offsetof(Player, iframes) == 0xc400);

#define player (*(Player**)0x4b4514)
    enum addrs {
        CHARA = 0x4b0c90,
        SUBSHOT = 0x4b0c94,
        MODEFLAGS = 0x4b0ce0,
        STAGE_NUM = 0x4b0cb0,
        REPLAY_MGR_PTR = 0x4b4518,
        ECL_MGR_PTR = 0x4b43dc,
        SPELL_OBJ_PTR = 0x4b43cc,
    };

    using std::pair;
    struct THPracParam {
        int32_t mode;
        int32_t stage;
        int32_t section;
        int32_t phase;
        int64_t score;
        int32_t life;
        int32_t life_fragment;
        int32_t bomb;
        int32_t bomb_fragment;
        int32_t power;
        int32_t value;
        int32_t graze;
        int32_t ufo_side;
        int32_t ventra_1;
        int32_t ventra_2;
        int32_t ventra_3;

        std::vector<PlayerDamageSource> reimuADmgSrcs[5];
        bool dlg;

        bool _playLock = false;
        void Reset()
        {
            for (size_t st = 0; st < elementsof(reimuADmgSrcs); ++st)
                reimuADmgSrcs[st].clear();
            memset(this, 0, sizeof(THPracParam));
        }
        bool ReadJson(std::string& json)
        {
            ParseJson();

            ForceJsonValue(game, "th12");
            GetJsonValue(mode);
            GetJsonValue(stage);
            GetJsonValue(section);
            GetJsonValue(phase);
            GetJsonValueEx(dlg, Bool);

            GetJsonValue(score);
            GetJsonValue(life);
            GetJsonValue(life_fragment);
            GetJsonValue(bomb);
            GetJsonValue(bomb_fragment);
            GetJsonValue(power);
            GetJsonValue(value);
            GetJsonValue(graze);
            GetJsonValue(ufo_side);
            GetJsonValue(ventra_1);
            GetJsonValue(ventra_2);
            GetJsonValue(ventra_3);

           // deserializing damage source data (for ReimuA bomb desync fix)
            GetJsonVectorArray(reimuADmgSrcs, {
                if (!el.IsArray() || el.Size() * sizeof(int32_t) != sizeof(PlayerDamageSource))
                    return std::nullopt;

                PlayerDamageSource dmgSrc {};
                int32_t* p = (int32_t*)(&dmgSrc);
                for (rapidjson::SizeType i = 0; i < el.Size(); i++)
                    p[i] = el[i].GetInt();

                return dmgSrc;
            });


            return true;
        }
        std::string GetJson()
        {
            if (mode == 0) { // vanilla run mode
                CreateJson();

                AddJsonValueEx(version, GetVersionStr(), jalloc);
                AddJsonValueEx(game, "th12", jalloc);
                AddJsonValue(mode);

                // serializing damage source data (for ReimuA bomb desync fix)
                AddJsonVectorArray(reimuADmgSrcs, {
                    rapidjson::Value dmgSrcArray(rapidjson::kArrayType);

                    int32_t* p = (int32_t*)(&el);
                    size_t count = sizeof(PlayerDamageSource) / sizeof(int32_t);
                    for (size_t i = 0; i < count; ++i)
                        dmgSrcArray.PushBack(p[i], jalloc);

                    return dmgSrcArray;
                });

                ReturnJson();
            } else { // thprac mode
                CreateJson();

                AddJsonValueEx(version, GetVersionStr(), jalloc);
                AddJsonValueEx(game, "th12", jalloc);
                AddJsonValue(mode);
                AddJsonValue(stage);
                if (section)
                    AddJsonValue(section);
                if (phase)
                    AddJsonValue(phase);
                if (dlg)
                    AddJsonValue(dlg);

                AddJsonValue(score);
                AddJsonValue(life);
                AddJsonValue(life_fragment);
                AddJsonValue(bomb);
                AddJsonValue(bomb_fragment);
                AddJsonValue(power);
                AddJsonValue(value);
                AddJsonValue(graze);
                AddJsonValue(ufo_side);
                if (ventra_1) {
                    AddJsonValue(ventra_1);
                    if (ventra_2) {
                        AddJsonValue(ventra_2);
                        if (ventra_3) {
                            AddJsonValue(ventra_3);
                        }
                    }
                }

                ReturnJson();
            }
        }
    };
    THPracParam thPracParam {};

    class THGuiPrac : public Gui::GameGuiWnd {
        THGuiPrac() noexcept
        {
            *mMode = 1;
            *mLife = 9;
            *mBomb = 9;
            *mPower = 400;
            *mValue = 10000;

            SetFade(0.8f, 0.1f);
            SetStyle(ImGuiStyleVar_WindowRounding, 0.0f);
            SetStyle(ImGuiStyleVar_WindowBorderSize, 0.0f);
            OnLocaleChange();
        }
        SINGLETON(THGuiPrac)
    public:

        __declspec(noinline) void State(int state)
        {
            switch (state) {
            case 0:
                break;
            case 1:
                mDiffculty = *((int32_t*)0x4aebd0);
                SetFade(0.8f, 0.1f);
                Open();
                thPracParam.Reset();
            case 2:
                break;
            case 3:
                SetFade(0.8f, 0.1f);
                Close();
                *mNavFocus = 0;

                // Fill Param
                thPracParam.mode = *mMode;
                thPracParam.stage = *mStage;
                thPracParam.section = CalcSection();
                thPracParam.phase = SpellPhase() ? *mPhase : 0;
                if (SectionHasDlg(thPracParam.section))
                    thPracParam.dlg = *mDlg;

                thPracParam.score = *mScore;
                thPracParam.life = *mLife;
                thPracParam.life_fragment = *mLifeFragment;
                thPracParam.bomb = *mBomb;
                thPracParam.bomb_fragment = *mBombFragment;
                thPracParam.power = *mPower;
                thPracParam.value = *mValue;
                thPracParam.graze = *mGraze;
                thPracParam.ufo_side = *mUfoSide;

                if (*mVentra1) {
                    thPracParam.ventra_1 = *mVentra1;
                    if (*mVentra2) {
                        thPracParam.ventra_2 = *mVentra2;
                        if (*mVentra3) {
                            thPracParam.ventra_3 = *mVentra3;
                        }
                    }
                }

                break;
            case 4:
                Close();
                *mNavFocus = 0;
                break;
            default:
                break;
            }
        }

    protected:
        virtual void OnLocaleChange() override
        {
            SetTitle(S(TH_MENU));
            switch (Gui::LocaleGet()) {
            case Gui::LOCALE_ZH_CN:
                SetSize(350.f, 410.f);
                SetPos(260.f, 50.f);
                SetItemWidth(-65.0f);
                break;
            case Gui::LOCALE_EN_US:
                SetSize(425.f, 410.f);
                SetPos(185.f, 50.f);
                SetItemWidth(-65.0f);
                break;
            case Gui::LOCALE_JA_JP:
                SetSize(350.f, 410.f);
                SetPos(260.f, 50.f);
                SetItemWidth(-65.0f);
                break;
            default:
                break;
            }
        }
        virtual void OnContentUpdate() override
        {
            ImGui::TextUnformatted(S(TH_MENU));
            ImGui::Separator();

            PracticeMenu();
        }
        const th_glossary_t* SpellPhase()
        {
            auto section = CalcSection();
            if (section == TH12_ST6_BOSS8)
                return TH12_SPELL_PHASE4_LOCK;
            else if(section == TH12_ST6_BOSS10) {
                return TH_SPELL_PHASE1;
            } else if (section == TH12_ST7_END_S10) {
                return TH_SPELL_PHASE3;
            } else if (section == 10000 + 4 * 100 + 8) {
                return TH_PHASE_INF_MODE;
            } else if (section == 10000 + 5 * 100 + 6)
            {
                return TH_PHASE_INF_MODE;
            }else if (section == TH12_ST5_MID2) {
                return TH_TIMEOUT_SETTING;
            }
            return nullptr;
        }
        void PracticeMenu()
        {
            mMode();
            if (mStage())
                *mSection = *mChapter = 0;
            if (*mMode == 1) {
                if (mWarp())
                    *mSection = *mChapter = *mPhase = 0;
                if (*mWarp) {
                    SectionWidget();
                    mPhase(TH_PHASE, SpellPhase());
                }

                mLife();
                mLifeFragment();
                mBomb();
                mBombFragment();
                auto power_str = std::to_string((float)(*mPower) / 100.0f).substr(0, 4);
                mPower(power_str.c_str());
                mUfoSide();
                mVentra1();
                if (*mVentra1) {
                    mVentra2();
                    if (*mVentra2) {
                        mVentra3();
                    }
                }
                mValue();
                mValue.RoundDown(10);
                mGraze();
                mScore();
                mScore.RoundDown(10);
            }

            mNavFocus();
        }
        int CalcSection()
        {
            int chapterId = 0;
            switch (*mWarp) {
            case 1: // Chapter
                // Chapter Id = 10000 + Stage * 100 + Section
                chapterId += (*mStage + 1) * 100;
                chapterId += *mChapter;
                chapterId += 10000; // Base of chapter ID is 1000.
                return chapterId;
                break;
            case 2:
            case 3: // Mid boss & End boss
                return th_sections_cba[*mStage][*mWarp - 2][*mSection];
                break;
            case 4:
            case 5: // Non-spell & Spellcard
                return th_sections_cbt[*mStage][*mWarp - 4][*mSection];
                break;
            default:
                return 0;
                break;
            }
        }
        bool SectionHasDlg(int32_t section)
        {
            switch (section) {
            case TH12_ST1_BOSS1:
            case TH12_ST2_BOSS1:
            case TH12_ST3_BOSS1:
            case TH12_ST4_BOSS1:
            case TH12_ST5_BOSS1:
            case TH12_ST5_MID1:
            case TH12_ST6_BOSS1:
            case TH12_ST7_END_NS1:
            case TH12_ST7_MID1:
                return true;
            default:
                return false;
            }
        }
        void SectionWidget()
        {
            static char chapterStr[256] {};
            auto& chapterCounts = mChapterSetup[*mStage];

            switch (*mWarp) {
            case 1: // Chapter
                mChapter.SetBound(1, chapterCounts[0] + chapterCounts[1]);

                if (chapterCounts[1] == 0 && chapterCounts[0] != 0) {
                    sprintf_s(chapterStr, S(TH_STAGE_PORTION_N), *mChapter);
                } else if (*mChapter <= chapterCounts[0]) {
                    sprintf_s(chapterStr, S(TH_STAGE_PORTION_1), *mChapter);
                } else {
                    sprintf_s(chapterStr, S(TH_STAGE_PORTION_2), *mChapter - chapterCounts[0]);
                };

                mChapter(chapterStr);
                break;
            case 2:
            case 3: // Mid boss & End boss
                if (mSection(TH_WARP_SELECT[*mWarp],
                        th_sections_cba[*mStage][*mWarp - 2],
                        th_sections_str[::THPrac::Gui::LocaleGet()][mDiffculty]))
                    *mPhase = 0;
                if (SectionHasDlg(th_sections_cba[*mStage][*mWarp - 2][*mSection]))
                    mDlg();
                break;
            case 4:
            case 5: // Non-spell & Spellcard
                if (mSection(TH_WARP_SELECT[*mWarp],
                        th_sections_cbt[*mStage][*mWarp - 4],
                        th_sections_str[::THPrac::Gui::LocaleGet()][mDiffculty]))
                    *mPhase = 0;
                if (SectionHasDlg(th_sections_cbt[*mStage][*mWarp - 4][*mSection]))
                    mDlg();
                break;
            default:
                break;
            }
        }

        Gui::GuiCombo mMode { TH_MODE, TH_MODE_SELECT };
        Gui::GuiCombo mStage { TH_STAGE, TH_STAGE_SELECT };
        Gui::GuiCombo mWarp { TH_WARP, TH_WARP_SELECT };
        Gui::GuiCombo mSection { TH_MODE };
        Gui::GuiCombo mPhase { TH_PHASE };
        Gui::GuiCheckBox mDlg { TH_DLG };
        Gui::GuiCombo mUfoSide { TH12_UFO_SIDE, TH_SIDE_SELECT };
        Gui::GuiCombo mVentra1 { TH12_VENTRA_1, TH12_VENTRA_SELECT };
        Gui::GuiCombo mVentra2 { TH12_VENTRA_2, TH12_VENTRA_SELECT };
        Gui::GuiCombo mVentra3 { TH12_VENTRA_3, TH12_VENTRA_SELECT };

        Gui::GuiSlider<int, ImGuiDataType_S32> mChapter { TH_CHAPTER, 0, 0 };
        Gui::GuiDrag<int64_t, ImGuiDataType_S64> mScore { TH_SCORE, 0, 9999999990, 10, 100000000 };
        Gui::GuiSlider<int, ImGuiDataType_S32> mLife { TH_LIFE, 0, 9 };
        Gui::GuiSlider<int, ImGuiDataType_S32> mLifeFragment { TH_LIFE_FRAGMENT, 0, 3 };
        Gui::GuiSlider<int, ImGuiDataType_S32> mBomb { TH_BOMB, 0, 9 };
        Gui::GuiSlider<int, ImGuiDataType_S32> mBombFragment { TH_BOMB_FRAGMENT, 0, 2 };
        Gui::GuiSlider<int, ImGuiDataType_S32> mPower { TH_POWER, 0, 400 };
        Gui::GuiDrag<int, ImGuiDataType_S32> mValue { TH_VALUE, 0, 999990, 10, 100000 };
        Gui::GuiDrag<int, ImGuiDataType_S32> mGraze { TH_GRAZE, 0, 999999, 1, 100000 };

        Gui::GuiNavFocus mNavFocus { TH_STAGE, TH_MODE, TH_WARP,
            TH_MID_STAGE, TH_END_STAGE, TH_NONSPELL, TH_SPELL, TH_PHASE, TH_CHAPTER,
            TH_SCORE, TH_LIFE, TH_LIFE_FRAGMENT, TH_BOMB, TH_BOMB_FRAGMENT,
            TH_POWER, TH_VALUE, TH_GRAZE, TH12_UFO_SIDE, TH12_VENTRA_1, TH12_VENTRA_2, TH12_VENTRA_3 };

        int mChapterSetup[7][2] {
            { 3, 2 },
            { 3, 2 },
            { 4, 3 },
            { 4, 4 },
            { 4, 3 },
            { 3, 0 },
            { 4, 4 },
        };

        int mDiffculty = 0;
    };
    class THGuiRep : public Gui::GameGuiWnd {
        THGuiRep() noexcept
        {
        }
        SINGLETON(THGuiRep)
    public:

        void CheckReplay()
        {
            uint32_t index = GetMemContent(0x4b4530, 0x5a78);
            char* repName = (char*)GetMemAddr(0x4b4530, index * 4 + 0x5a80, 0x1e0);
            std::wstring repDir(L"replay/");
            repDir.append(mb_to_utf16(repName, 932));

            std::string param;
            if (ReplayLoadParam(repDir.c_str(), param) && mRepParam.ReadJson(param))
                mParamStatus = true;
            else
                mRepParam.Reset();
        }

        bool mRepStatus = false;
        void State(int state)
        {
            switch (state) {
            case 1:
                mRepStatus = false;
                mParamStatus = false;
                thPracParam.Reset();
                break;
            case 2:
                CheckReplay();
                break;
            case 3:
                mRepStatus = true;
                if (mParamStatus)
                    memcpy(&thPracParam, &mRepParam, sizeof(THPracParam));
                break;
            default:
                break;
            }
        }

    protected:
        bool mParamStatus = false;
        THPracParam mRepParam;
    };
    class THOverlay : public Gui::GameGuiWnd {
        THOverlay() noexcept
        {
            SetTitle("Mod Menu");
            SetFade(0.5f, 0.5f);
            SetPos(10.0f, 10.0f);
            SetSize(0.0f, 0.0f);
            SetWndFlag(
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | 0);
            OnLocaleChange();
        }
        SINGLETON(THOverlay)
    public:

    protected:
        virtual void OnLocaleChange() override
        {
            float x_offset_1 = 0.0f;
            float x_offset_2 = 0.0f;
            switch (Gui::LocaleGet()) {
            case Gui::LOCALE_ZH_CN:
                x_offset_1 = 0.1f;
                x_offset_2 = 0.14f;
                break;
            case Gui::LOCALE_EN_US:
                x_offset_1 = 0.1f;
                x_offset_2 = 0.14f;
                break;
            case Gui::LOCALE_JA_JP:
                x_offset_1 = 0.1f;
                x_offset_2 = 0.14f;
                break;
            default:
                break;
            }

            mMenu.SetTextOffsetRel(x_offset_1, x_offset_2);
            mMuteki.SetTextOffsetRel(x_offset_1, x_offset_2);
            mInfLives.SetTextOffsetRel(x_offset_1, x_offset_2);
            mInfBombs.SetTextOffsetRel(x_offset_1, x_offset_2);
            mInfPower.SetTextOffsetRel(x_offset_1, x_offset_2);
            mTimeLock.SetTextOffsetRel(x_offset_1, x_offset_2);
            mAutoBomb.SetTextOffsetRel(x_offset_1, x_offset_2);
            mElBgm.SetTextOffsetRel(x_offset_1, x_offset_2);
            mInGameInfo.SetTextOffsetRel(x_offset_1, x_offset_2);
        }
        virtual void OnContentUpdate() override
        {
            mMuteki();
            mInfLives();
            mInfBombs();
            mInfPower();
            mTimeLock();
            mAutoBomb();
            mElBgm();
            mInGameInfo();
        }
        virtual void OnPreUpdate() override
        {
            if (mMenu(false) && !ImGui::IsAnyItemActive()) {
                if (*mMenu) {
                    Open();
                } else {
                    Close();
                }
            }
        }
        Gui::GuiHotKeyChord mMenu { "ModMenuToggle", "BACKSPACE", Gui::GetBackspaceMenuChord() };
        HOTKEY_DEFINE(mMuteki, TH_MUTEKI, "F1", VK_F1)
        PATCH_HK(0x43837F, "01"),
        PATCH_HK(0x436d2f, "eb"),
        PATCH_HK(0x4383cb, "83c4109090")
        HOTKEY_ENDDEF();
        
        
        HOTKEY_DEFINE(mInfBombs, TH_INFBOMBS, "F3", VK_F3)
        PATCH_HK(0x422F27, "00")
        HOTKEY_ENDDEF();
        
        HOTKEY_DEFINE(mInfPower, TH_INFPOWER, "F4", VK_F4)
        PATCH_HK(0x43944B, "48")
        HOTKEY_ENDDEF();
        
        HOTKEY_DEFINE(mAutoBomb, TH_AUTOBOMB, "F6", VK_F6)
        // PATCH_HK(0x436D9B, "c6")
        EHOOK_HK(0x436d9b, 7, {
            if (*(int32_t*)(pCtx->Edi + 0xA34) == 7)
                pCtx->Eip = 0x436da8;
        })
        HOTKEY_ENDDEF();

    public:
        Gui::GuiHotKey mInfLives { TH_INFLIVES2, "F2", VK_F2,};
        HOTKEY_DEFINE(mTimeLock, TH_TIMELOCK, "F5", VK_F5)
        PATCH_HK(0x40DB71, "eb"),
        PATCH_HK(0x414A9B, "90")
        HOTKEY_ENDDEF();
        Gui::GuiHotKey mElBgm { TH_EL_BGM, "F7", VK_F7 };
        Gui::GuiHotKey mInGameInfo { THPRAC_INGAMEINFO, "F8", VK_F8 };
    };

    // Verbatim contents of docs/logger/th12_checkpoints.csv, generated by
    // docs/logger/tools/gen_th12_checkpoints.py into thprac_th12_checkpoints_gen.cpp.
    extern const char kTh12CheckpointsCsv[];

    namespace {
        // TH12 is a B-generation engine: every host points at the same global ECL
        // resource (the host constructor writes H+0x102C = *(u32*)(mgr+0x64) for every
        // host), so the TH16 trap of "res must be taken per host" does not exist here.
        // What TH12 does have is that a thread's +4 is not a sub index but the absolute
        // address of the instruction being executed, so the current sub has to be
        // recovered by mapping that address back through the address-sorted sub table
        // (see BuildIpMap / NameOfIp) - exactly like TH14.
        //
        // Two TH12-specific facts drive the shape of this module:
        //  1. Section subs are entered with `opcode 0x0F` (@MainSubNN() async), i.e.
        //     they run on threads of their own; the main thread's +4 stays inside
        //     `main` for the whole stage. Walking the host's thread chain is therefore
        //     mandatory - reading the main thread alone would always report `main`.
        //  2. Boss / mid-boss containers are separate hosts (enmCreateA, opcode 0x0101,
        //     same host factory), so their subs are not on the main host's thread chain
        //     at all; the outer walk over the global host chain is what reaches them.

        // mgr -> host chain head. Double-linked, with the link node embedded in the
        // host at H+0x12C0 as {+0 = H, +4 = next, +8 = prev}; head = mgr+0x68,
        // tail = mgr+0x6C. The factory links every host unconditionally, whatever its
        // script name, so this is the set of all live ECL hosts (boss included).
        constexpr uintptr_t kMgrHostHeadOff = 0x68;
        // host -> thread chain head (sentinel embedded at H+0x1030, entry layout
        // {+0 = thread, +4 = next, +8 = prev}). The sentinel's own [+0] is the host's
        // main thread (H+8) and the remaining entries are async (spawn) nodes, so
        // walking this list covers the main thread and every async child.
        constexpr uintptr_t kHostThreadListOff = 0x1030;
        // host -> ECL resource (a copy of the same global mgr+0x64 for every host).
        constexpr uintptr_t kHostResOff = 0x102C;
        // thread -> current instruction pointer (absolute VA; 0 once the thread ended).
        constexpr uintptr_t kThreadIpOff = 4;

        // res -> sub count / sub table. Entry is 8B: +0 = name char*, +4 = sub data
        // absolute address; the sub's first instruction sits at +4 + 0x10.
        constexpr uintptr_t kResCountOff = 8;
        constexpr uintptr_t kResTableOff = 0x8C;
        constexpr uintptr_t kSubEntrySize = 8;
        constexpr uintptr_t kSubDataOff = 4;
        constexpr uintptr_t kSubInsnOff = 0x10;

        // Spell-card object (SPELL_OBJ_PTR) flags. bit0 is the "card active" level:
        // set by the spell / spell2 / spell3 / spellEx declaration handler
        // (0x40E0C7 `or eax,3` -> 0x40E0CA `mov [edi+0x7C],eax`), cleared only by
        // spellEnd() (0x40E61D `and dword [edi+0x7C],0xFFFFFFFE`). A 1 -> 0 edge is
        // therefore exactly one card ending, capture or fail alike. The object is a
        // 0xC0-byte singleton (malloc 0xC0 + memset at 0x40D864/0x40DAC0), freed with
        // the global zeroed at 0x40DA8E. TH12 shares TH14's layout (id +0x78,
        // name +0x38, flags +0x7C); only the global address differs.
        constexpr uintptr_t kSpellFlagsOff = 0x7C;

        // ECL sub naming convention: MainSub* = mid-stage chapter, MBoss* = mid-boss,
        // Boss* = stage boss. Classified by the whitelist key itself, same rule as
        // TH14 / TH16. One TH12 quirk: `ST3 后半 #1` is the mid-boss container entry
        // (`MBoss1`), so that single chapter record is classified as a mid-boss.
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

        // Sub names that end a mid-boss fight. Exact enumeration of every
        // setBoss(-1) / setNext / setTimeout site in reasm/exes/ecl_dump/th12/*.decl
        // (9 files) that ends a mid-boss container (line numbers = sub definitions):
        //  - MBossDead   st01:2162  st02:1782  st03:2731  st05:2352  st07:1145
        //  - MBossEscape st01:2206  st02:1820  st03:2778  st05:2402  st07:1180
        // Each is a real phase of the mid-boss container (reached through setNext /
        // setTimeout) and runs for ~120-180 frames, far longer than one 60 Hz sample.
        // Prefixes stay exact: a loose "contains Dead" test would also match names
        // such as MBossCard3AtLoser. The stage-boss ending `BossDead` (present in
        // these very files) is a non-match here - `MBossDead` and `BossDead` differ at
        // the first character - which is what keeps a stage clear from being read as a
        // mid-boss end.
        // ST4 and ST6 have no such sub: their mid-boss container (stage04/06.decl
        // `MBoss`) calls setBoss(0) and setBoss(-1) back to back and runs the whole
        // fight synchronously inside `MBoss1`, ending with delete() - there is nothing
        // to observe. Those two stages are covered by the host-disappearance close in
        // Update() (mMidBossHost) instead.
        bool IsMidBossEndSub(const char* name)
        {
            return std::strncmp(name, "MBossDead", 9) == 0
                || std::strncmp(name, "MBossEscape", 11) == 0;
        }

        // Sub names that end a stage boss fight = the stage is cleared. Exact-match
        // enumeration of every `void Boss*Dead|Escape` in
        // reasm/exes/ecl_dump/th12/*.decl (9 files) yields exactly one name:
        //  - BossDead  stage01:1187 stage02:642 stage03:1915 stage04:1388 stage05:1614
        //              stage06:2597 stage07boss:3470 - each reached by a
        //              setNext(..., "BossDead") from the last attack sub, has a `+60:`
        //              label right after enmKillAll() and keeps running well past it.
        // TH12 has NO stage-boss escape sub: the timeout branch lives inside BossDead
        // itself (`unless ($TIMEOUT == 0) goto BossDead_xxx`), unlike TH14's separate
        // BossEscape. Excluded, and why the exact match matters:
        //  - MBossDead / MBossEscape (every stage with a mid-boss) are mid-boss ends -
        //    already covered by IsMidBossEndSub, and a mid-boss death is not a stage
        //    clear. `BossDead` and `MBossDead` differ at the first character, so the
        //    exact match never confuses the two.
        //  - Girl00Dead / Cir00Dead / Cir01Dead / Cir02Dead / ParasolAt1Dead /
        //    ParasolAt3Dead / BossCard6_atLineDead (setDeath()/setNext() registered
        //    enemy death animations that run while the boss is still alive) and
        //    BossCard2UfoRedDead / BossCard4UfoBlueDead / BossCard6UfoGreenDead /
        //    BossCard8UfoRainbowDead (stage07boss UFO end animations, also mid-fight)
        //    are all non-matches by name.
        bool IsStageClearSub(const char* name)
        {
            return std::strcmp(name, "BossDead") == 0;
        }

        // Hard traversal caps; also act as sanity sentinels for corrupted pointers.
        constexpr int32_t kMaxCount = 8192;
        constexpr int kMaxHosts = 1024;
        constexpr int kMaxThreads = 64;
    }

    // Samples the ECL engine once per frame and latches the current stage section
    // whenever any host is executing a whitelisted script function.
    class TH12Logger {
        SINGLETON(TH12Logger)

    public:
        // Called on game start; the section starts at "main" until a whitelist hit.
        void OnStageStart();

        // Called every frame from th12_update.
        void Update();

        // Never null; points at a string literal or into mEntries.
        const char* CurrentSection() const
        {
            return mSection;
        }

        // 一局日志：miss/bomb 发生时记下当前段落（见 thprac_games.h 的 GameSession）。
        void OnMiss();
        void OnBomb();

    private:
        TH12Logger();

        struct Entry {
            std::string name;
            std::string stage;
            std::string record;
        };

        // One sub of the address-sorted view of the sub table.
        struct SubRange {
            uint32_t start; // first instruction address (table[i].+4 + 0x10)
            uint32_t index; // index back into the sub table
        };

        struct CStrHash {
            size_t operator()(const char* s) const
            {
                return std::hash<std::string_view> {}(std::string_view(s));
            }
        };
        struct CStrEq {
            bool operator()(const char* a, const char* b) const
            {
                return std::strcmp(a, b) == 0;
            }
        };

        void LoadCsv();
        void BuildStageMap(int32_t stageNum);
        void BuildIpMap(int32_t count, uintptr_t table);
        const char* NameOfIp(uintptr_t table, uint32_t ip) const;
        // 每帧采样符卡对象，观测「一张符卡结束」（口径 15，零新 hook）。
        void SampleSpellCard();

        std::vector<Entry> mEntries;
        // Keys/values alias mEntries' strings; mEntries is frozen after LoadCsv().
        std::unordered_map<const char*, const char*, CStrHash, CStrEq> mStageMap;
        // Address-sorted view of the sub table, rebuilt from the live table whenever
        // (count, table) changes; mIpMapCount / mIpMapTable are only a change detector
        // (never dereferenced), so no pointer is held across frames.
        std::vector<SubRange> mIpMap;
        int32_t mIpMapCount = -1;
        uintptr_t mIpMapTable = 0;
        const char* mSection = "main";
        // Mid-boss window: while a mid-boss is alive, chapter checkpoints belong to
        // sections that start running before the mid-boss dies (ST1/ST2/ST4/ST5), so
        // they are held back in mPending until the fight ends.
        const char* mPending = nullptr;
        bool mMidBossActive = false;
        // The host that was running the mid-boss when the window opened. Only ever
        // compared against freshly walked host pointers, never dereferenced - same
        // nature as mIpMapTable. Used to close the window on ST4/ST6, whose mid-boss
        // container has no MBossDead / MBossEscape to observe: it runs the fight
        // synchronously and ends with delete(), so the host dropping off the chain is
        // the only end signal.
        uintptr_t mMidBossHost = 0;
        int32_t mBuiltStage = -1;
        // 符卡结束计数（口径 15）：符卡对象 bit0 的每一帧 1 → 0 下降沿 +1，
        // mSection 换到新 checkpoint 时 -1（下限 0）。退出时仍 > 0 ⇒ 刚结束的
        // 符卡还没等到下一个 checkpoint，不记 interrupt。
        // mSpellObj 用来识别「对象被重建」（换关 / 重试 / 重开）：新对象第一帧
        // 直接清零，避免把旧对象销毁当成一次符卡结束。
        int32_t mSpellFlag = 0;
        uintptr_t mSpellObj = 0;
        bool mSpellActive = false;
        // 本关是否已通关：关底 boss 的收尾 sub（IsStageClearSub）跑过即为真。
        // 段落入口函数切换 / 换关 / 新局时复位。退出游戏时仍为 false ⇒ 记一条
        // interrupt 事件（见 Update() 顶部与 docs/req_logger.md 口径 12）。
        // 与 mSpellFlag 并列：spellFlag 只覆盖「卡结束但 checkpoint 没跟上」，
        // 关底收尾的 +60 帧窗口仍靠这条兜底（TH12 无独立 escape sub）。
        bool mStageClear = false;
        GameSession mSession;
    };

    TH12Logger::TH12Logger()
    {
        LoadCsv();
    }

    void TH12Logger::LoadCsv()
    {
        try {
            std::istringstream ss(kTh12CheckpointsCsv);
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
                if (row[1] != "th12") {
                    continue;
                }
                mEntries.push_back(Entry { row[0], row[2], row[3] });
            }
        } catch (...) {
            // A malformed table just means we never latch a section.
            mEntries.clear();
        }
    }

    void TH12Logger::BuildStageMap(int32_t stageNum)
    {
        mStageMap.clear();
        mBuiltStage = stageNum;

        char stageKey[16];
        std::snprintf(stageKey, sizeof(stageKey), "ST%d", stageNum);
        for (auto& e : mEntries) {
            if (e.stage == stageKey) {
                // First wins on a duplicate entry sub within one stage. TH12 has two:
                //  - ST3 `MBoss1`    = `ST3 后半 #1` (csv line 30) and `ST3 MID1` (line 33)
                //  - ST7 `MainSub03` = `ST7 前半 #4` (csv line 90) and `ST7 后半 #3` (line 93)
                mStageMap.emplace(e.name.c_str(), e.record.c_str());
            }
        }
    }

    void TH12Logger::BuildIpMap(int32_t count, uintptr_t table)
    {
        if (count == mIpMapCount && table == mIpMapTable) {
            return;
        }
        mIpMapCount = count;
        mIpMapTable = table;
        mIpMap.clear();
        mIpMap.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i) {
            uint32_t data = GetMemContent<uint32_t>(table + static_cast<uintptr_t>(i) * kSubEntrySize + kSubDataOff);
            if (data) {
                mIpMap.push_back(SubRange { data + static_cast<uint32_t>(kSubInsnOff), static_cast<uint32_t>(i) });
            }
        }
        // The table itself is ordered by name (it backs a binary search), so code
        // ranges only exist once it is re-sorted by address.
        std::sort(mIpMap.begin(), mIpMap.end(),
            [](const SubRange& a, const SubRange& b) { return a.start < b.start; });
    }

    const char* TH12Logger::NameOfIp(uintptr_t table, uint32_t ip) const
    {
        // Sub i owns [start_i, start_i+1); an address past the last range still lands
        // on the last sub, which is harmless - a wrong name simply never matches the
        // whitelist, so the worst case is a no-op.
        auto it = std::upper_bound(mIpMap.begin(), mIpMap.end(), ip,
            [](uint32_t value, const SubRange& e) { return value < e.start; });
        if (it == mIpMap.begin()) {
            return nullptr;
        }
        --it;
        return GetMemContent<const char*>(table + static_cast<uintptr_t>(it->index) * kSubEntrySize);
    }

    void TH12Logger::OnStageStart()
    {
        // No allocation: this runs inside a VEH hook callback.
        mSession.Begin("th12");
        mSection = "main";
        mPending = nullptr;
        mMidBossActive = false;
        mMidBossHost = 0;
        mSpellFlag = 0;
        mSpellObj = 0;
        mSpellActive = false;
        mStageClear = false;
        // Force the address-sorted sub table to be rebuilt on the next sample.
        mIpMapCount = -1;
        mIpMapTable = 0;
    }

    void TH12Logger::OnMiss()
    {
        // No allocation: this runs inside a VEH hook callback.
        mSession.PushEvent(GameLogEventType::Miss, mSection);
    }

    void TH12Logger::OnBomb()
    {
        // No allocation: this runs inside a VEH hook callback.
        mSession.PushEvent(GameLogEventType::Bomb, mSection);
    }

    void TH12Logger::SampleSpellCard()
    {
        // No allocation: this runs inside a VEH hook callback.
        // The whole game has a single spell-card object (spell / spell2 / spell3 /
        // spellEx all share it). bit0 stays high for the whole card and is cleared by
        // spellEnd(), which runs at the first instruction of the next attack sub, so
        // one sample per frame cannot miss a card end: every card is followed by a
        // non-spell attack sub (>= 60 frames; a static scan of all 9 .decl files shows
        // no card -> card transition). A boss's last card keeps bit0 until BossDead's
        // +60 frame; that window is mStageClear's job, not this counter's.
        uintptr_t obj = GetMemContent<uintptr_t>(SPELL_OBJ_PTR);
        if (!obj) {
            // Menu or stage teardown. Drop the tracking state but keep mSpellFlag:
            // the exit edge in the same Update() still has to consume it.
            mSpellObj = 0;
            mSpellActive = false;
            return;
        }
        bool active = (GetMemContent<uint32_t>(obj + kSpellFlagsOff) & 1) != 0;
        if (obj != mSpellObj) {
            // A rebuilt object (new stage / retry / restart) begins a new window; the
            // teardown of the previous one must not be counted as a card end.
            mSpellFlag = 0;
            mSpellObj = obj;
            mSpellActive = active;
            return;
        }
        if (mSpellActive && !active) {
            ++mSpellFlag;
        }
        mSpellActive = active;
    }

    void TH12Logger::Update()
    {
        // 一局边界与落盘必须在所有 early return 之前：退出一局后 STAGE_NUM 会落到菜单态。
        // TH12 has no PLAYER_PTR enum entry; 0x4B4514 is the same address the IGI panel
        // uses for its show/hide decision (`player` / `*(DWORD*)0x004B4514`, :1303/:1307).
        bool inGame = GetMemContent<uint32_t>(0x4b4514) != 0;
        // 符卡结束采样必须早于 interrupt 判定：同一帧「清卡 + 退出」也要算进去。
        SampleSpellCard();
        // 未通关就退出游戏（PLAYER_PTR 由非 0 变 0）：记一条 interrupt。
        // mSpellFlag > 0 说明有一张卡刚结束、下一个 checkpoint 还没切过来（那是
        // 已知的 checkpoint 滞后 bug），此时退出不记 interrupt。
        // 必须在 Tick **之前** —— Tick 会在同一帧把本局快照转待落盘并写盘。
        if (mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame)) {
            mSession.PushEvent(GameLogEventType::Interrupt, mSection);
        }
        mSession.Tick(inGame, (inGame && mSession.NeedsStartInfo())
                ? S(IGI_PL_12[(*(DWORD*)(0x004B0C90)) * 2 + (*(DWORD*)(0x4B0C94))])
                : nullptr);

        int32_t stageNum = GetMemContent<int32_t>(STAGE_NUM);
        if (stageNum == 9) {
            // Extra is very likely reported as 7 here, not 9: thprac's own
            // th12_patch_main hook (hooked at 0x40E8DF) tests (STAGE_NUM - 1) % 6 == 0
            // with the comment "must not be st1/7", i.e. it puts ST1 and ST7 on the
            // same footing. Map 9 -> 7 defensively anyway, the same way TH14 / TH16 do.
            // 🟡 未真机.
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
            mMidBossHost = 0;
            mSpellFlag = 0;
            mSpellObj = 0;
            mSpellActive = false;
            mStageClear = false;
        }
        if (mStageMap.empty()) {
            return;
        }

        uintptr_t mgr = GetMemContent<uintptr_t>(ECL_MGR_PTR);
        if (!mgr) {
            return;
        }

        // One candidate per kind; the chapter one keeps the old "last hit wins".
        const char* midHit = nullptr;
        const char* bossHit = nullptr;
        const char* chapHit = nullptr;
        bool sawMidBossEnd = false;
        bool midBossHostAlive = false;
        bool sawStageClear = false;

        // Nothing is cached across frames: the whole chain is re-walked every tick.
        uintptr_t nd = GetMemContent<uintptr_t>(mgr + kMgrHostHeadOff);
        for (int hi = 0; nd && hi < kMaxHosts; ++hi) {
            uintptr_t host = GetMemContent<uintptr_t>(nd);
            uintptr_t next = GetMemContent<uintptr_t>(nd + 4);
            nd = next;
            if (!host) {
                continue;
            }
            if (host == mMidBossHost) {
                midBossHostAlive = true;
            }
            uintptr_t res = GetMemContent<uintptr_t>(host + kHostResOff);
            if (!res) {
                continue;
            }
            int32_t count = GetMemContent<int32_t>(res + kResCountOff);
            uintptr_t table = GetMemContent<uintptr_t>(res + kResTableOff);
            if (!table || count <= 0 || count > kMaxCount) {
                continue;
            }
            BuildIpMap(count, table);

            uintptr_t entry = host + kHostThreadListOff;
            for (int ti = 0; entry && ti < kMaxThreads; ++ti) {
                uintptr_t node = GetMemContent<uintptr_t>(entry);
                uintptr_t tnext = GetMemContent<uintptr_t>(entry + 4);
                entry = tnext;
                if (!node) {
                    continue;
                }
                uint32_t ip = GetMemContent<uint32_t>(node + kThreadIpOff);
                if (!ip) {
                    continue;
                }
                const char* name = NameOfIp(table, ip);
                if (!name) {
                    continue;
                }
                if (IsMidBossEndSub(name)) {
                    sawMidBossEnd = true;
                }
                if (IsStageClearSub(name)) {
                    sawStageClear = true;
                }
                auto it = mStageMap.find(name);
                if (it != mStageMap.end()) {
                    // Last hit wins per kind: the host list is tail-inserted, so
                    // walking it head-first ends on the newest host.
                    switch (ClassifySub(name)) {
                    case kKindMidBoss:
                        midHit = it->second;
                        mMidBossHost = host;
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

        // 段落入口函数切换 ⇒ 新的一段开始，本关的「已通关」标记作废。注意不能用
        // 「任一白名单命中即复位」：boss 战期间 boss 宿主自己就停在白名单 sub
        // （Boss1 / BossCard1…）上，那样 BossDead 那一帧会被同帧命中当场抹掉。
        const char* prevSection = mSection;

        // A mid-boss fight ends when its death / escape sub runs; whatever chapter
        // checkpoints the main thread crossed meanwhile belong to sections that start
        // before the fight is over and are replayed now. Like TH14 there is no "done"
        // latch: TH12's mid-boss container sub (`MBoss`) is not whitelisted, so it
        // cannot reopen the window after the fact.
        if (sawMidBossEnd && mMidBossActive) {
            mMidBossActive = false;
            mMidBossHost = 0;
            if (mPending) {
                mSection = mPending;
                mPending = nullptr;
            }
        }
        // ST4 / ST6 have no MBossDead / MBossEscape (their container fights
        // synchronously inside MBoss1 and ends with delete()), so the window is closed
        // by the host itself leaving the chain. Guarded by `!midHit` so that a
        // mid-boss host replaced by a new one in the same frame does not close it.
        if (mMidBossActive && mMidBossHost && !midBossHostAlive && !midHit) {
            mMidBossActive = false;
            mMidBossHost = 0;
            if (mPending) {
                mSection = mPending;
                mPending = nullptr;
            }
        }

        if (bossHit) {
            // The stage boss cannot coexist with a live mid-boss.
            mMidBossActive = false;
            mMidBossHost = 0;
            mPending = nullptr;
            mSection = bossHit;
        } else if (midHit) {
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

        if (mSection != prevSection && std::strcmp(mSection, prevSection) != 0) {
            // 新 checkpoint 接管了：把「已结束、尚未被消费」的符卡还回去。
            // 顺序不能反 —— ++ 在 Update() 顶部，同帧「卡结束 + 新 checkpoint」
            // 必须先 +1 再 -1，净 0，否则残留会吞掉之后整关的 interrupt。
            if (mSpellFlag > 0) {
                --mSpellFlag;
            }
            mStageClear = false;
        }
        // 通关判定放在最后：万一同帧与段落切换撞车，以通关为准。
        if (sawStageClear) {
            mStageClear = true;
        }

        mSession.Latch(GameLogRecordOfSection(mSection));
    }

    class TH12InGameInfo : public Gui::GameGuiWnd {

        TH12InGameInfo() noexcept
        {
            SetTitle("igi");
            SetFade(0.9f, 0.9f);
            SetPosRel(425.0f / 640.0f, 338.0f / 480.0f);
            SetSizeRel(210.0f / 640.0f, 0.0f);
            SetWndFlag(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | 0);
            OnLocaleChange();
        }
        SINGLETON(TH12InGameInfo)

    public:
        int32_t mMissCount;
        int32_t mBombCount;

        int32_t mRUFOCount;
        int32_t mGUFOCount;
        int32_t mBUFOCount;
        int32_t mCUFOCount;

        int32_t mRVentraCount_Get;
        int32_t mGVentraCount_Get;
        int32_t mBVentraCount_Get;

        int32_t mRVentraCount_Drop;
        int32_t mGVentraCount_Drop;
        int32_t mBVentraCount_Drop;
        int32_t mCVentraCount_Drop;

    protected:
        virtual void OnLocaleChange() override
        {
            float x_offset_1 = 0.0f;
            float x_offset_2 = 0.0f;
            switch (Gui::LocaleGet()) {
            case Gui::LOCALE_ZH_CN:
                x_offset_1 = 0.12f;
                x_offset_2 = 0.172f;
                break;
            case Gui::LOCALE_EN_US:
                x_offset_1 = 0.12f;
                x_offset_2 = 0.16f;
                break;
            case Gui::LOCALE_JA_JP:
                x_offset_1 = 0.18f;
                x_offset_2 = 0.235f;
                break;
            default:
                break;
            }
        }

        virtual void OnContentUpdate() override
        {
            int32_t cur_player_type = (*(DWORD*)(0x004B0C90)) * 2 + (*(DWORD*)(0x4B0C94));
            int32_t diff = *((int32_t*)0x4B0CA8);
            auto diff_pl = std::format("{} ({})", S(IGI_DIFF[diff]), S(IGI_PL_12[cur_player_type]));
            auto diff_pl_sz = ImGui::CalcTextSize(diff_pl.c_str());

            ImGui::SetCursorPosX(ImGui::GetWindowSize().x * 0.5f - diff_pl_sz.x * 0.5f);
            ImGui::Text(diff_pl.c_str());

            auto ufo_cnt = std::format("{}/{}/{}/{}", mRUFOCount, mGUFOCount, mBUFOCount, mCUFOCount);
            auto ufo_cnt_sz = ImGui::CalcTextSize(ufo_cnt.c_str()).x;

            ImGui::Columns(2);
            auto width1 = ImGui::GetColumnWidth(1);
            if (width1 < ufo_cnt_sz)
                ImGui::SetColumnWidth(0, ImGui::GetWindowContentRegionWidth() - ufo_cnt_sz);

            ImGui::Text(S(THPRAC_INGAMEINFO_MISS_COUNT));
            ImGui::NextColumn();
            ImGui::Text("%8d", mMissCount);
            ImGui::NextColumn();
            ImGui::Text(S(THPRAC_INGAMEINFO_BOMB_COUNT));
            ImGui::NextColumn();
            ImGui::Text("%8d", mBombCount);

            ImGui::NextColumn();
            ImGui::Text("当前段落");
            ImGui::NextColumn();
            ImGui::Text("%s", TH12Logger::singleton().CurrentSection());
            ImGui::NextColumn();
            ImGui::Text(S(THPRAC_INGAMEINFO_12_UFO_COUNT3));
            ImGui::NextColumn();
            ImGui::Text("%8d", mRVentraCount_Drop + mGVentraCount_Drop + mBVentraCount_Drop + mCVentraCount_Drop);
            ImGui::NextColumn();
            ImGui::Text(S(THPRAC_INGAMEINFO_12_UFO_COUNT2));
            ImGui::NextColumn();
            ImGui::Text("%8d", mRVentraCount_Get + mGVentraCount_Get + mBVentraCount_Get);

            auto last_item_width = ImGui::GetItemRectSize().x;

            ImGui::NextColumn();
            ImGui::Text(S(THPRAC_INGAMEINFO_12_UFO_COUNT1));
            ImGui::NextColumn();
            if (ufo_cnt_sz * 0.5 < last_item_width)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + last_item_width - ufo_cnt_sz * 0.5f);
            ImGui::TextColored({ 1.0f, 0.5f, 0.5f, 1.0f }, "%d", mRUFOCount);
            ImGui::SameLine(0.0f,0.0f);
            ImGui::Text("/");
            ImGui::SameLine(0.0f,0.0f);
            ImGui::TextColored({ 0.5f, 1.0f, 0.5f, 1.0f }, "%d", mGUFOCount);
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::Text("/");
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored({ 0.5f, 0.75f, 1.0f, 1.0f }, "%d", mBUFOCount);
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::Text("/");
            ImGui::SameLine(0.0f, 0.0f);
            if (g_adv_igi_options.th12_chromatic_ufo) {
                float r, g, b;
                static float h = 0.0f;
                h += 0.016f;
                ImGui::ColorConvertHSVtoRGB(h, 0.5f, 1.0f, r, g, b);
                ImGui::TextColored({ r, g, b, 1.0f }, "%d", mCUFOCount);
            }else{
                ImGui::Text("%d", mCUFOCount);
            }
        }

        virtual void OnPreUpdate() override
        {
            if (*(DWORD*)(0x004B4514)) {
                GameUpdateInner(12);
            } else {
            }
            if (*(THOverlay::singleton().mInGameInfo) && player) {
                SetPosRel(425.0f / 640.0f, 338.0f / 480.0f);
                SetSizeRel(210.0f / 640.0f, 0.0f);
                Open();
            } else {
                Close();
            }
        }

    public:
    };

    
    EHOOK_ST(th12_all_clear_bonus_1, 0x420a9b, 7, {
        pCtx->Eip = 0x420acd;
    });
    EHOOK_ST(th12_all_clear_bonus_2, 0x420bc2, 4, {
        if (GetMemContent(MODEFLAGS) & 0x10) {
            pCtx->Eip = 0x420aa4;
        } else if (GetMemContent(0x4b44e8, 0x74) && GetMemContent(REPLAY_MGR_PTR, 0x1c, 0xa) & 1) {
            pCtx->Eip = 0x420ac3;
        }
    });
    EHOOK_ST(th12_all_clear_bonus_3, 0x420c6b, 4, {
        if (GetMemContent(MODEFLAGS) & 0x10) {
            pCtx->Eip = 0x420aa4;
        } else if (GetMemContent(0x4b44e8, 0x74) && GetMemContent(REPLAY_MGR_PTR, 0x1c, 0xa) & 1) {
            pCtx->Eip = 0x420ac3;
        }
    });
    EHOOK_ST(th12_master_disable2, 0x40DF33,6,
    {
        *(DWORD*)(pCtx->Esi + 0x00018F9C) = 0;
    });

    float g_bossMoveDownRange = BOSS_MOVE_DOWN_RANGE_INIT;
    EHOOK_ST(th12_bossmovedown, 0x00417562, 5, {
        float* y_pos = (float*)(pCtx->Ebx + 0x15F8);
        float* y_range = (float*)(pCtx->Ebx + 0x1600);
        float y_max = (*y_pos) + (*y_range) * 0.5f;
        float y_min2 = y_max - (*y_range) * (1.0f - g_bossMoveDownRange);
        *y_pos = (y_max + y_min2) * 0.5f;
        *y_range = (y_max - y_min2);
    });
    EHOOK_ST(th12_laser_hit_test, 0x437a80, 1,
        {
            if (!g_show_bullet_hitbox)
                return;
            bool is_prac = ((*(byte*)(0x4B0CE0) == 0x10));
            bool is_rep = false;
            if (*(DWORD*)(0x4B4518)) {
                is_rep = (*(DWORD*)(*(DWORD*)(0x4B4518) + 0x10) == 1);
            }
            if (is_prac || is_rep) {
                float height, angle, width;
                width = *(float*)(pCtx->Esp + 0xC);
                height = *(float*)(pCtx->Esp + 0x8);
                angle = *(float*)(pCtx->Esp + 0x4);
                ImVec2 pos_tail = *(ImVec2*)(pCtx->Eax);
                auto Multiply = [](ImVec2 a, ImVec2 b) -> ImVec2 {
                    return { a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x };
                };
                auto Add = [](ImVec2 a, ImVec2 b) -> ImVec2 {
                    return { a.x + b.x, a.y + b.y };
                };
                auto GetClientFromStage = [](ImVec2 stage) -> ImVec2 {
                    float x = stage.x;
                    float y = stage.y;

                    y = y * 2.0f + 32.0f;
                    x = x * 2.0f + 448.0f;
                    ImGuiIO& io = ImGui::GetIO();
                    return { x * io.DisplaySize.x / 1280.0f, y * io.DisplaySize.y / 960.0f };
                };
                ImVec2 hit_pl = *(ImVec2*)(*(DWORD*)(0x4B4514) + 0x9E4);
                ImVec2 hit_pl_conved = { hit_pl.x * 1.0f, hit_pl.y * 1.0f };
                ImVec2 posA = { width + hit_pl_conved.x, -(height * 0.5f + hit_pl_conved.y) };
                ImVec2 posB = { width + hit_pl_conved.x, height * 0.5f + hit_pl_conved.y };
                ImVec2 posC = { -hit_pl_conved.x, -(height * 0.5f + hit_pl_conved.y) };
                ImVec2 posD = { -hit_pl_conved.x, height * 0.5f + hit_pl_conved.y };
                ImVec2 rotation = { cosf(angle), sinf(angle) };
                posA = Multiply(posA, rotation);
                posB = Multiply(posB, rotation);
                posC = Multiply(posC, rotation);
                posD = Multiply(posD, rotation);
                posA = GetClientFromStage(Add(posA, pos_tail));
                posB = GetClientFromStage(Add(posB, pos_tail));
                posC = GetClientFromStage(Add(posC, pos_tail));
                posD = GetClientFromStage(Add(posD, pos_tail));
                g_th12_laser_hit_draw_vec.push_back({ posA, posB, posD, posC });
            }
        });


    class THAdvOptWnd : public Gui::PPGuiWnd {
        SINGLETON(THAdvOptWnd)    
    public:
        bool forceBossMoveDown = false;
    private:
        void MasterDisableInit()
        {
            th12_master_disable2.Setup();
            th12_master_disable2.Toggle(g_adv_igi_options.disable_master_autoly);
        }
        void FpsInit()
        {
            if ((mOptCtx.vpatch_base = (uintptr_t)GetModuleHandleW(L"openinputlagpatch.dll")) != NULL) {
                OILPInit(mOptCtx);
            } else if ((mOptCtx.vpatch_base = (uintptr_t)GetModuleHandleW(L"vpatch_th12.dll")) != NULL) {
                uint64_t hash[2];
                CalcFileHash(L"vpatch_th12.dll", hash);
                if (hash[0] != 666604866657820391ll || hash[1] != 18391463919001639953ll)
                    mOptCtx.fps_status = -1;
                else if (*(int32_t*)(mOptCtx.vpatch_base + 0x1b024) == 0) {
                    mOptCtx.fps_status = 2;
                    mOptCtx.fps = *(int32_t*)(mOptCtx.vpatch_base + 0x1b034);
                }
            } else if (*(uint8_t*)0x4cead3 == 3) {
                mOptCtx.fps_status = 1;

                DWORD oldProtect;
                VirtualProtect((void*)0x45044e, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
                *(double**)0x45044e = &mOptCtx.fps_dbl;
                VirtualProtect((void*)0x45044e, 4, oldProtect, &oldProtect);
            } else
                mOptCtx.fps_status = 0;
        }
        void FpsSet()
        {
            if (mOptCtx.fps_status == 3) {
                mOptCtx.oilp_set_game_fps(mOptCtx.fps);
                mOptCtx.oilp_set_replay_skip_fps(mOptCtx.fps_replay_fast);
                mOptCtx.oilp_set_replay_slow_fps(mOptCtx.fps_replay_slow);
            }else if (mOptCtx.fps_status == 1) {
                mOptCtx.fps_dbl = 1.0 / (double)mOptCtx.fps;
            } else if (mOptCtx.fps_status == 2) {
                *(int32_t*)(mOptCtx.vpatch_base + 0x18abc) = mOptCtx.fps;
                *(int32_t*)(mOptCtx.vpatch_base + 0x1b034) = mOptCtx.fps;
            }
        }
        void GameplayInit()
        {
            th12_all_clear_bonus_1.Setup();
            th12_all_clear_bonus_2.Setup();
            th12_all_clear_bonus_3.Setup();
        }
        void GameplaySet()
        {
            th12_all_clear_bonus_1.Toggle(mOptCtx.all_clear_bonus);
            th12_all_clear_bonus_2.Toggle(mOptCtx.all_clear_bonus);
            th12_all_clear_bonus_3.Toggle(mOptCtx.all_clear_bonus);
        }

        THAdvOptWnd() noexcept
        {
            SetWndFlag(ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
            SetFade(0.8f, 0.8f);
            SetStyle(ImGuiStyleVar_WindowRounding, 0.0f);
            SetStyle(ImGuiStyleVar_WindowBorderSize, 0.0f);

            InitUpdFunc([&]() { ContentUpdate(); },
                [&]() { LocaleUpdate(); },
                [&]() {},
                []() {});

            OnLocaleChange();
            FpsInit();
            GameplayInit();
            MasterDisableInit();
            th12_bossmovedown.Setup();
            th12_laser_hit_test.Setup();
        }

    public:
        __declspec(noinline) static bool StaticUpdate()
        {
            auto& advOptWnd = THAdvOptWnd::singleton();

            if (Gui::GetChordPressed(Gui::GetAdvancedMenuChord())) {
                if (advOptWnd.IsOpen())
                    advOptWnd.Close();
                else
                    advOptWnd.Open();
            }
            advOptWnd.Update();

            return advOptWnd.IsOpen();
        }

    protected:
        void LocaleUpdate()
        {
            SetTitle(S(TH_SPELL_PRAC));
            switch (Gui::LocaleGet()) {
            case Gui::LOCALE_ZH_CN:
                SetSizeRel(1.0f, 1.0f);
                SetPosRel(0.0f, 0.0f);
                SetItemWidthRel(-0.0f);
                SetAutoSpacing(true);
                break;
            case Gui::LOCALE_EN_US:
                SetSizeRel(1.0f, 1.0f);
                SetPosRel(0.0f, 0.0f);
                SetItemWidthRel(-0.0f);
                SetAutoSpacing(true);
                break;
            case Gui::LOCALE_JA_JP:
                SetSizeRel(1.0f, 1.0f);
                SetPosRel(0.0f, 0.0f);
                SetItemWidthRel(-0.0f);
                SetAutoSpacing(true);
                break;
            default:
                break;
            }
        }
        void ContentUpdate()
        {
            ImGui::TextUnformatted(S(TH_ADV_OPT));
            ImGui::Separator();
            ImGui::BeginChild("Adv. Options", ImVec2(0.0f, 0.0f));

            if (BeginOptGroup<TH_GAME_SPEED>()) {
                if (GameFPSOpt(mOptCtx))
                    FpsSet();
                EndOptGroup();
            }
            if (BeginOptGroup<TH_GAMEPLAY>()) {
                DisableKeyOpt();
                KeyHUDOpt();
                InfLifeOpt();
                if (ImGui::Checkbox(S(TH_BOSS_FORCE_MOVE_DOWN), &forceBossMoveDown)) {
                    th12_bossmovedown.Toggle(forceBossMoveDown);
                }
                ImGui::SameLine();
                HelpMarker(S(TH_BOSS_FORCE_MOVE_DOWN_DESC));
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::DragFloat(S(TH_BOSS_FORCE_MOVE_DOWN_RANGE), &g_bossMoveDownRange, 0.002f, 0.0f, 1.0f))
                    g_bossMoveDownRange = std::clamp(g_bossMoveDownRange, 0.0f, 1.0f);
                ImGui::Checkbox(S(THPRAC_INGAMEINFO_12_UFO_SUPER_ULTRA_CHROMATIC_INFO_PRO), &g_adv_igi_options.th12_chromatic_ufo);

                if (ImGui::Checkbox(S(TH_DISABLE_MASTER), &g_adv_igi_options.disable_master_autoly)) {
                    th12_master_disable2.Toggle(g_adv_igi_options.disable_master_autoly);
                }
                ImGui::SameLine();
                HelpMarker(S(TH_DISABLE_MASTER_DESC));
                ImGui::Checkbox(S(TH_ENABLE_LOCK_TIMER), &g_adv_igi_options.enable_lock_timer_autoly);

                if (ImGui::Checkbox("show laser hitbox(only practice mode)", &g_show_bullet_hitbox))
                {
                    th12_laser_hit_test.Toggle(g_show_bullet_hitbox);
                }

                if (GameplayOpt(mOptCtx))
                    GameplaySet();
                EndOptGroup();
            }
            SSS::SSS_UI(12);
            InGameReactionTestOpt();
            AboutOpt();
            ImGui::EndChild();
            ImGui::SetWindowFocus();
        }

        adv_opt_ctx mOptCtx;
    };

    void ECLJump(ECLHelper& ecl, unsigned int start, unsigned int ecl_time, int length)
    {
        ecl.SetPos(start);
        ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << length << 0;
    }
    void ECLJumpEx(ECLHelper& ecl, unsigned int start, unsigned int dest, int at_frame, int ecl_time = 0)
    {
        ecl.SetPos(start);
        ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
    }
    void ECLSetHealth(ECLHelper& ecl, unsigned int start, unsigned int ecl_time, int health)
    {
        ecl.SetPos(start);
        ecl << ecl_time << 0x0014019b << 0x01ff0000 << 0x00000000 << health;
    }
    void ECLCallSub(ECLHelper& ecl, unsigned int start, unsigned int ecl_time, unsigned int sub_length, char* sub)
    {
        ecl.SetPos(start);
        ecl << ecl_time << 0x000b << sub_length + 0x14 << 0x01ff0000 << 0x00000000 << sub_length;
        for (unsigned int i = 0; i < sub_length; i++)
            ecl << (int8_t)sub[i];
    }
    __declspec(noinline) void THStageWarp(ECLHelper& ecl, int stage, int portion)
    {
        if (stage == 1) {
            switch (portion) {
            case 1:
                break;
            case 2:
                ecl << pair{0x12f6c, (int16_t)0} << pair{0x12fb8, (int16_t)0};
                ECLJumpEx(ecl, 0x133d4, 0x1346c, 60, 90);
                break;
            case 3:
                ecl << pair{0x12f6c, (int16_t)0} << pair{0x12fb8, (int16_t)0};
                ECLJumpEx(ecl, 0x133d4, 0x13548, 60, 90);
                break;
            case 4:
                ecl << pair{0x12f6c, (int16_t)0} << pair{0x12fb8, (int16_t)0};
                ECLJumpEx(ecl, 0x133d4, 0x1369c, 60, 90);
                break;
            case 5:
                ecl << pair{0x12f6c, (int16_t)0} << pair{0x12fb8, (int16_t)0};
                ECLJumpEx(ecl, 0x133d4, 0x136ec, 60, 90);
                break;
            default:
                break;
            }
        } else if (stage == 2) {
            switch (portion) {
            case 1:
                break;
            case 2:
                ECLJumpEx(ecl, 0x13108, 0x131b4, 60, 90);
                break;
            case 3:
                ECLJumpEx(ecl, 0x13108, 0x1323c, 60, 90);
                break;
            case 4:
                ECLJumpEx(ecl, 0x13108, 0x132e8, 60, 90);
                break;
            case 5:
                ECLJumpEx(ecl, 0x13108, 0x13360, 60, 90);
                break;
            default:
                break;
            }
        } else if (stage == 3) {
            switch (portion) {
            case 1:
                break;
            case 2:
                ECLJumpEx(ecl, 0x16864, 0x168dc, 60, 90);
                break;
            case 3:
                ECLJumpEx(ecl, 0x16864, 0x16910, 60, 90);
                break;
            case 4:
                ECLJumpEx(ecl, 0x16864, 0x16944, 60, 90);
                break;
            case 5:
                ECLJumpEx(ecl, 0x16864, 0x169ac, 60);
                ECLJumpEx(ecl, 0x10548, 0x1226c, 0);
                ECLJumpEx(ecl, 0x1227c, 0x1241c, 0);
                ecl << pair{0x12470, (int16_t)0} << pair{0x124dc, (int16_t)0};
                ecl.SetPos(0x12654);
                ecl << 0 << 0x001001a3 << 0x00ff0000 << 0
                    << 0 << 0x0014019c << 0x01ff0000 << 0 << -1
                    << 0 << 0x00100001 << 0x00ff0000 << 0;
                break;
            case 6:
                ECLJumpEx(ecl, 0x16864, 0x16a30, 60, 90);
                break;
            case 7:
                ECLJumpEx(ecl, 0x16864, 0x16a64, 60, 90);
                break;
            default:
                break;
            }
        } else if (stage == 4) {
            switch (portion) {
            case 1:
                break;
            case 2:
                ECLJumpEx(ecl, 0x13800, 0x13868, 0, 90);
                break;
            case 3:
                ECLJumpEx(ecl, 0x13800, 0x1389c, 0, 90);
                break;
            case 4:
                ECLJumpEx(ecl, 0x13800, 0x138d0, 0, 0);
                break;
            case 5:
                ECLJumpEx(ecl, 0x13800, 0x13948, 0, 90);
                break;
            case 6:
                ECLJumpEx(ecl, 0x13800, 0x1397c, 0, 90);
                break;
            case 7:
                ECLJumpEx(ecl, 0x13800, 0x139b0, 0, 90);
                break;
            case 8:
                if (thPracParam.phase == 1) {
                    ECLJumpEx(ecl, 0x12B10, 0x12110, 0, 0);
                }
                ECLJumpEx(ecl, 0x13800, 0x139e4, 0, 90);
                break;
            default:
                break;
            }
        } else if (stage == 5) {
            switch (portion) {
            case 1:
                break;
            case 2:
                ECLJumpEx(ecl, 0x158c0, 0x15938, 60, 90);
                break;
            case 3:
                ECLJumpEx(ecl, 0x158c0, 0x1596c, 60, 90);
                break;
            case 4:
                ECLJumpEx(ecl, 0x158c0, 0x159a0, 60, 90);
                break;
            case 5:
                ECLJumpEx(ecl, 0x158c0, 0x15a3c, 181, 90);
                break;
            case 6:
                if (thPracParam.phase == 1) {
                    ECLJumpEx(ecl, 0x14214, 0x134F4, 0, 0);
                    ECLJumpEx(ecl, 0x1224C, 0x134F4, 0, 0);
                    ECLJumpEx(ecl, 0x158c0, 0x15a80, 181, 0);
                } else {
                    ECLJumpEx(ecl, 0x158c0, 0x15a80, 181, 90);
                }
                break;
            case 7:
                ECLJumpEx(ecl, 0x158c0, 0x15ab4, 181, 90);
                break;
            default:
                break;
            }
        } else if (stage == 6) {
            switch (portion) {
            case 1:
                break;
            case 2:
                ECLJumpEx(ecl, 0x1a3c0, 0x1a438, 60, 90);
                break;
            case 3:
                ECLJumpEx(ecl, 0x1a3c0, 0x1a46c, 60, 90);
                break;
            default:
                break;
            }
        } else if (stage == 7) {
            switch (portion) {
            case 1:
                break;
            case 2:
                ECLJumpEx(ecl, 0xdb14, 0xdb8c, 60, 90);
                break;
            case 3:
                ECLJumpEx(ecl, 0xdb14, 0xdbc0, 60, 90);
                break;
            case 4:
                ECLJumpEx(ecl, 0xdb14, 0xdbf4, 60, 90);
                break;
            case 5:
                ECLJumpEx(ecl, 0xdb14, 0xdc8c, 60, 90);
                break;
            case 6:
                ECLJumpEx(ecl, 0xdb14, 0xdcc0, 60, 90);
                break;
            case 7:
                ECLJumpEx(ecl, 0xdb14, 0xdcf4, 60, 90);
                break;
            case 8:
                ECLJumpEx(ecl, 0xdb14, 0xdd28, 60, 90);
                break;
            default:
                break;
            }
        }
    }
    __declspec(noinline) void THPatch(ECLHelper& ecl, th_sections_t section)
    {
        auto nue = [&]() {
            ECLJumpEx(ecl, 0xdb14, 0xdd80, 71);
            ecl.SetFile(2);
            ecl << pair{0x774, 60} << pair{0x7e0, 60}; // Invincible Frame & Wait Time
            ecl << pair{0x790, (int16_t)0x0}; // Skip Teleportation
            ECLJumpEx(ecl, 0x7fc, 0x8b8, 2); // Skip Animation
            ecl.SetPos(0x6f0);
            ecl << 0 << 0x00300106 << 0x02ff0000 << 0 << 0 << 0; // Change Sprite
            ecl.SetPos(0x7a4);
            ecl << 0 << 0x002c012d << 0x04ff0000 << 0 << 60 << 4 << 0 << 0x42c00000; // Move 301
        };

        switch (section) {
        case THPrac::TH12::TH12_ST1_MID1:
            ECLJumpEx(ecl, 0x133b0, 0x13658, 59);
            break;
        case THPrac::TH12::TH12_ST1_MID2:
            ECLJumpEx(ecl, 0x133b0, 0x13658, 59);
            ecl << pair{0x0c820, 0x0fa0};
            ecl << pair{0x0c8bc, 0x14b4};
            ECLJumpEx(ecl, 0xcde8, 0xce40, 0);
            ecl << pair{0x0ccfc, (int16_t)0x0};
            ecl << pair{0x0ce74, 60};
            break;
        case THPrac::TH12::TH12_ST1_MID3:
            ECLJumpEx(ecl, 0x133b0, 0x13658, 59);
            ecl << pair{0x0c820, 0x0fa0};
            ecl << pair{0x0c8bc, 0x14b4};
            ECLJumpEx(ecl, 0xcde8, 0xce40, 0);
            ecl << pair{0x0ccfc, (int16_t)0x0};
            ecl << pair{0x0ce74, 60};

            ECLSetHealth(ecl, 0x0ce74, 60, 0x514);
            ecl << pair{0x0ce7a, (int16_t)0x20};
            ecl << pair{0xce94, 9999};
            break;
        case THPrac::TH12::TH12_ST1_BOSS1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0x133b0, 0x13788, 59);
            else
                ECLJumpEx(ecl, 0x133b0, 0x137ac, 59);
            break;
        case THPrac::TH12::TH12_ST1_BOSS2:
            ECLJumpEx(ecl, 0x133b0, 0x137ac, 59);
            ecl << pair{0x0ae0, 0x5dc};
            ecl << pair{0xcec, 60};
            break;
        case THPrac::TH12::TH12_ST1_BOSS3:
            ECLJumpEx(ecl, 0x133b0, 0x137ac, 59);
            ecl << pair{0x0be4, (int8_t)0x32};
            ECLJumpEx(ecl, 0x190c, 0x1978, 0);
            ecl << pair{0x17e4, (int16_t)0x0} << pair{0x197c, (int16_t)0x0};
            ecl.SetPos(0xb4c);
            ecl << 0.0f << 128.0f << 280.0f << 256.0f;
            break;
        case THPrac::TH12::TH12_ST1_BOSS4:
            ECLJumpEx(ecl, 0x133b0, 0x137ac, 59);
            ecl << pair{0x0be4, (int8_t)0x32};
            ECLJumpEx(ecl, 0x190c, 0x1978, 0);
            ecl << pair{0x17e4, (int16_t)0x0};
            ecl << pair{0x162c, 0x4b0};
            ecl << pair{0x1640, 0x3e8};
            ecl.SetPos(0xb4c);
            ecl << 0.0f << 128.0f << 280.0f << 256.0f;
            break;
        case THPrac::TH12::TH12_ST2_MID1:
            ECLJumpEx(ecl, 0x13108, 0x132a4, 59);
            break;
        case THPrac::TH12::TH12_ST2_MID2:
            ECLJumpEx(ecl, 0x13108, 0x132a4, 59);
            ecl.SetPos(0x09ea4);
            ecl << 0 << 0x0014019f << 0x01ff0000 << 0 << 60;
            ECLSetHealth(ecl, 0x09ed8, 60, 1350);
            ecl << pair{0x9eb8, 60};
            break;
        case THPrac::TH12::TH12_ST2_BOSS1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0x13108, 0x134d8, 59);
            else
                ECLJumpEx(ecl, 0x13108, 0x134fc, 59);
            break;
        case THPrac::TH12::TH12_ST2_BOSS2:
            ECLJumpEx(ecl, 0x13108, 0x134fc, 59);
            ecl << pair{0x0d18, 0x44c};
            ecl << pair{0xeac, 60};
            break;
        case THPrac::TH12::TH12_ST2_BOSS3:
            ECLJumpEx(ecl, 0x13108, 0x134fc, 59);
            ecl << pair{0x0df4, (int8_t)0x32};
            ECLJumpEx(ecl, 0x1268, 0x12d4, 0);
            ecl << pair{0x1188, (int16_t)0x0} << pair{0x12d8, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST2_BOSS4:
            ECLJumpEx(ecl, 0x13108, 0x134fc, 59);
            ecl << pair{0x0df4, (int8_t)0x32};
            ECLJumpEx(ecl, 0x1268, 0x12d4, 0);
            ecl << pair{0x1188, (int16_t)0x0};
            ecl << pair{0x0fe4, 0x6a4};
            break;
        case THPrac::TH12::TH12_ST2_BOSS5:
            ECLJumpEx(ecl, 0x13108, 0x134fc, 59);
            ecl << pair{0xdec, 0xc} << pair(0xdf4, 'draC') << pair{0xdf8, 0x00000033};
            ECLJumpEx(ecl, 0x32e8, 0x3340, 0);
            ECLJumpEx(ecl, 0x34dc, 0x3518, 0);
            ecl << pair{0x35b4, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST3_MID1:
            ECLJumpEx(ecl, 0x16864, 0x169ac, 59);
            break;
        case THPrac::TH12::TH12_ST3_MID2:
            ECLJumpEx(ecl, 0x16864, 0x169ac, 59);
            ecl << pair{0x1047c, 0x6a0};
            ECLJumpEx(ecl, 0x10930, 0x10988, 0);
            ECLJumpEx(ecl, 0x1099c, 0x109c0, 0);
            ecl << pair{0x10998, 60};
            ecl << pair{0x108a0, 60};
            ecl << pair{0x108b4, (int16_t)0x0};
            ecl << pair{0x1086c, (int16_t)0x0} << pair{0x10a5c, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST3_BOSS1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0x16864, 0x16a98, 59);
            else
                ECLJumpEx(ecl, 0x16864, 0x16abc, 59);
            break;
        case THPrac::TH12::TH12_ST3_BOSS2:
            ECLJumpEx(ecl, 0x16864, 0x16abc, 59);
            ecl << pair{0x0bec, 0x6a4};
            ecl << pair{0xdac, 60};
            break;
        case THPrac::TH12::TH12_ST3_BOSS3:
            ECLJumpEx(ecl, 0x16864, 0x16abc, 59);
            ecl << pair{0x0cc8, (int8_t)0x32};
            ECLJumpEx(ecl, 0x27f0, 0x285c, 0);
            ecl << pair{0x2710, (int16_t)0x0} << pair{0x28a0, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST3_BOSS4:
            ECLJumpEx(ecl, 0x16864, 0x16abc, 59);
            ecl << pair{0x0cc8, (int8_t)0x32};
            ECLJumpEx(ecl, 0x27f0, 0x285c, 0);
            ecl << pair{0x2710, (int16_t)0x0};
            ecl << pair{0x256c, 0x7d0};
            break;
        case THPrac::TH12::TH12_ST3_BOSS5:
            ECLJumpEx(ecl, 0x16864, 0x16abc, 59);
            ecl << pair{0x0cc8, (int8_t)0x33};
            ECLJumpEx(ecl, 0x40b0, 0x411c, 0);
            ecl << pair{0x3fd0, (int16_t)0x0} << pair{0x4140, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST3_BOSS6:
            ECLJumpEx(ecl, 0x16864, 0x16abc, 59);
            ecl << pair{0x0cc8, (int8_t)0x33};
            ECLJumpEx(ecl, 0x40b0, 0x411c, 0);
            ecl << pair{0x3fd0, (int16_t)0x0};
            ecl << pair{0x3e2c, 0x9c4};
            break;
        case THPrac::TH12::TH12_ST4_MID1:
            ECLJumpEx(ecl, 0x13800, 0x13904, 0);
            break;
        case THPrac::TH12::TH12_ST4_BOSS1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0x13800, 0x13a4c, 119);
            else
                ECLJumpEx(ecl, 0x13800, 0x13a70, 119);
            break;
        case THPrac::TH12::TH12_ST4_BOSS2:
            ECLJumpEx(ecl, 0x13800, 0x13a70, 119);
            ecl << pair{0x34d4, 0x708};
            ecl << pair{0x3694, 60};
            break;
        case THPrac::TH12::TH12_ST4_BOSS3:
            ECLJumpEx(ecl, 0x13800, 0x13a70, 119);
            ecl << pair{0x35b0, (int8_t)0x32};
            ECLJumpEx(ecl, 0x4020, 0x408c, 0);
            ecl << pair{0x3f40, (int16_t)0x0} << pair{0x40d0, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST4_BOSS4:
            ECLJumpEx(ecl, 0x13800, 0x13a70, 119);
            ecl << pair{0x35b0, (int8_t)0x32};
            ECLJumpEx(ecl, 0x4020, 0x408c, 0);
            ecl << pair{0x3f40, (int16_t)0x0};
            ecl << pair{0x3d9c, 0x834};
            break;
        case THPrac::TH12::TH12_ST4_BOSS5:
            ECLJumpEx(ecl, 0x13800, 0x13a70, 119);
            ecl << pair{0x35b0, (int8_t)0x33};
            ECLJumpEx(ecl, 0x5178, 0x51e4, 0);
            ecl << pair{0x507c, (int16_t)0x0} << pair{0x5228, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST4_BOSS6:
            ECLJumpEx(ecl, 0x13800, 0x13a70, 119);
            ecl << pair{0x35b0, (int8_t)0x33};
            ECLJumpEx(ecl, 0x5178, 0x51e4, 0);
            ecl << pair{0x507c, (int16_t)0x0};
            ecl << pair{0x4ec4, 0x835};
            ecl << pair{0x4ed8, 0x835};
            break;
        case THPrac::TH12::TH12_ST4_BOSS7:
            ECLJumpEx(ecl, 0x13800, 0x13a70, 119);
            ecl << pair{0x35a8, 0xc} << pair(0x35b0, 'draC') << pair{0x35b4, 0x00000034};
            ECLJumpEx(ecl, 0x86c4, 0x8730, 0);
            ECLJumpEx(ecl, 0x8798, 0x87d8, 0);
            ecl << pair{0x8760, 20};
            ecl << pair{0x8614, (int16_t)0x0} << pair{0x8774, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST5_MID1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0x158c0, 0x159d4, 179);
            else {
                ECLJumpEx(ecl, 0x158c0, 0x159e8, 179);
                ecl << pair{0xd6ac, (int16_t)0x0} << pair{0xd8b8, (int16_t)0x0};
                ecl << pair{0xd6f0, 60};
            }
            break;
        case THPrac::TH12::TH12_ST5_MID2:
            ECLJumpEx(ecl, 0x158c0, 0x159e8, 179);
            ecl << pair{0xd6ac, (int16_t)0x0};
            ecl << pair{0xd6f0, 60};
            ecl << pair{0x0d7d4, 0x7d0};
            switch (thPracParam.phase) {
            default:
            case 0:
                break;
            case 1:
                ecl << pair { 0xdd54, (int16_t)442 };
                break;
            }
            break;
        case THPrac::TH12::TH12_ST5_BOSS1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0x158c0, 0x15ae8, 190);
            else
                ECLJumpEx(ecl, 0x158c0, 0x15b0c, 190);
            break;
        case THPrac::TH12::TH12_ST5_BOSS2:
            ECLJumpEx(ecl, 0x158c0, 0x15b0c, 190);
            ecl << pair{0x0b00, 0x7d0};
            ecl << pair{0xcec, 60};
            break;
        case THPrac::TH12::TH12_ST5_BOSS3:
            ECLJumpEx(ecl, 0x158c0, 0x15b0c, 190);
            ecl << pair{0x0bf0, (int8_t)0x32};
            ECLJumpEx(ecl, 0x1fbc, 0x2028, 0);
            ecl << pair{0x1edc, (int16_t)0x0} << pair{0x206c, (int16_t)0x0};
            ecl << pair{0xb20, 0x43000000};

            ecl << pair(0x1f04, 2640 - 120);
            break;
        case THPrac::TH12::TH12_ST5_BOSS4:
            ECLJumpEx(ecl, 0x158c0, 0x15b0c, 190);
            ecl << pair{0x0bf0, (int8_t)0x32};
            ECLJumpEx(ecl, 0x1fbc, 0x2028, 0);
            ecl << pair{0x1edc, (int16_t)0x0};
            ecl << pair{0x1d38, 0x7d0};
            break;
        case THPrac::TH12::TH12_ST5_BOSS5:
            ECLJumpEx(ecl, 0x158c0, 0x15b0c, 190);
            ecl << pair{0x0bf0, (int8_t)0x33};
            ECLJumpEx(ecl, 0x35c0, 0x362c, 0);
            ecl << pair{0x3498, (int16_t)0x0} << pair{0x3670, (int16_t)0x0};
            ecl << pair{0xb20, 0x43000000};

            ecl << pair(0x34c0, 2400 - 90) << pair(0x34ec, 2400 - 90);
            break;
        case THPrac::TH12::TH12_ST5_BOSS6:
            ECLJumpEx(ecl, 0x158c0, 0x15b0c, 190);
            ecl << pair{0x0bf0, (int8_t)0x33};
            ECLJumpEx(ecl, 0x35c0, 0x362c, 0);
            ecl << pair{0x3498, (int16_t)0x0};
            ecl << pair{0x32e0, 0x76c};
            ecl << pair{0x32f4, 0x514};
            break;
        case THPrac::TH12::TH12_ST5_BOSS7:
            ECLJumpEx(ecl, 0x158c0, 0x15b0c, 190);
            ecl << pair{0x0bf0, (int8_t)0x34};
            ECLJumpEx(ecl, 0x412c, 0x4198, 0);
            ecl << pair{0x4078, (int16_t)0x0} << pair{0x41bc, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST6_MID1:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a4a0, 60);
            break;
        case THPrac::TH12::TH12_ST6_BOSS1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0x1a3c0, 0x1a4e4, 59);
            else
                ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            break;
        case THPrac::TH12::TH12_ST6_BOSS2:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x968, 0x898};
            ecl << pair{0xb10, 60};
            break;
        case THPrac::TH12::TH12_ST6_BOSS3:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x0a44, (int8_t)0x32};
            ECLJumpEx(ecl, 0x29f0, 0x2a5c, 0);
            ecl << pair{0x2910, (int16_t)0x0};
            ecl << pair{0x298c, 60};
            ECLJumpEx(ecl, 0x2a9c, 0x2af0, 0);
            break;
        case THPrac::TH12::TH12_ST6_BOSS4:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x0a44, (int8_t)0x32};
            ECLJumpEx(ecl, 0x29f0, 0x2a5c, 0);
            ecl << pair{0x2910, (int16_t)0x0};
            ecl << pair{0x298c, 60};
            ECLJumpEx(ecl, 0x2a9c, 0x2b04, 0); // Changed

            ECLSetHealth(ecl, 0x2d04, 0x0, 0x960);
            ecl << pair{0x2d18, 9999};
            break;
        case THPrac::TH12::TH12_ST6_BOSS5:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x0a44, (int8_t)0x33};
            ECLJumpEx(ecl, 0x42e0, 0x4798, 0);
            ecl << pair{0x41e8, (int16_t)0x0} << pair{0x420c, (int16_t)0x0};
            ecl << pair{0x428c, 0};
            break;
        case THPrac::TH12::TH12_ST6_BOSS6:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x0a44, (int8_t)0x33};
            ECLJumpEx(ecl, 0x42e0, 0x47ac, 0); // Changed
            ecl << pair{0x41e8, (int16_t)0x0} << pair{0x420c, (int16_t)0x0};
            ecl << pair{0x428c, 0};

            ecl << pair{0x3f54, 0x9c4};
            break;
        case THPrac::TH12::TH12_ST6_BOSS7:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x0a44, (int8_t)0x34};
            ECLJumpEx(ecl, 0x6378, 0x6830, 0);
            ecl << pair{0x6280, (int16_t)0x0} << pair{0x62a4, (int16_t)0x0};
            ecl << pair{0x6324, 0};
            break;
        case THPrac::TH12::TH12_ST6_BOSS8:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x0a44, (int8_t)0x34};
            ECLJumpEx(ecl, 0x6378, 0x6844, 0); // Changed
            ecl << pair{0x6280, (int16_t)0x0} << pair{0x62a4, (int16_t)0x0};
            ecl << pair{0x6324, 0};

            ecl << pair{0x5fd8, 0x1388};

            switch (thPracParam.phase) {
            default:
            case 0:
                break;
            case 1:
                ECLJumpEx(ecl, 0xd264, 0xd294, 120);
                ecl << pair { 0xd508, (int16_t)0x0 };

                ecl << pair { 0x0ec0c, 0x0 };
                ecl << pair { 0x10188, 0x0 };
                break;
            case 2:
                ECLJumpEx(ecl, 0xd264, 0xd294, 120);
                ecl << pair { 0xd508, (int16_t)0x0 };

                ecl << pair { 0x0ec0c, 0x0 };
                ecl << pair { 0x0f468, 0x0 };
                ecl << pair { 0x10188, 0x0 };
                break;
            case 3:
                ECLJumpEx(ecl, 0xd264, 0xd294, 120);
                ecl << pair { 0xd508, (int16_t)0x0 };

                ecl << pair { 0x0ec0c, 0x0 };
                ecl << pair { 0x0f468, 0x0 };
                ecl << pair { 0x0f650, 0x3d };
                ecl << pair { 0x10188, 0x0 };
                break;
            case 4:
                ECLJumpEx(ecl, 0xd264, 0xd294, 120);
                ecl << pair { 0xd508, (int16_t)0x0 };
                ecl << pair { 0xD620, (int16_t)0x0 };

                ecl << pair { 0x0ec0c, 0x0 };
                ecl << pair { 0x0f468, 0x0 };
                ecl << pair { 0x0f650, 0x3d };
                ecl << pair { 0x0fa90, 0x0 };
                ecl << pair { 0x10188, 0x0 };
                break;
            case 5:
                ECLJumpEx(ecl, 0xd264, 0xd294, 120);
                ecl << pair{0xd508, (int16_t)0x0};

                ecl << pair{0x0ec0c, 0x0};
                ecl << pair{0x0f468, 0x0};
                ecl << pair{0x0f650, 0x3d};
                ecl << pair{0x0fa90, 0x0};
                ecl << pair{0x0ffdc, 99999999};
                ecl << pair{0x10188, 0x0};
                break;
            case 6:
                ECLJumpEx(ecl, 0xd264, 0xd294, 120);
                ecl << pair { 0xd508, (int16_t)0x0 };

                ecl << pair { 0x0ec0c, 0x0 };
                ecl << pair { 0x0f468, 0x0 };
                ecl << pair { 0x0f650, 0x3d };
                ecl << pair { 0x0fa90, 0x0 };
                ecl << pair { 0x0ffdc, 0x0 };
                ecl << pair { 0x10188, 0x0 };
                break;
            }
            break;
        case THPrac::TH12::TH12_ST6_BOSS9:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x0a44, (int8_t)0x35};
            ECLJumpEx(ecl, 0x9030, 0x9508, 0);
            ecl << pair{0x8f44, (int16_t)0x0};
            ecl << pair{0x8fdc, 10};
            break;
        case THPrac::TH12::TH12_ST6_BOSS10:
            ECLJumpEx(ecl, 0x1a3c0, 0x1a508, 59);
            ecl << pair{0x0a44, (int8_t)0x36};
            ECLJumpEx(ecl, 0x98f8, 0x99ac, 0);
            ecl << pair{0x9834, (int16_t)0x0};
            ecl << pair{0x9894, 10};

            if (thPracParam.phase == 1) {
                ecl << pair{0x12a28, 0x118c};
            }
            break;
        case THPrac::TH12::TH12_ST7_MID1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0xdb14, 0xdc28, 59);
            else
                ECLJumpEx(ecl, 0xdb14, 0xdc3c, 59);
            break;
        case THPrac::TH12::TH12_ST7_MID2:
            ECLJumpEx(ecl, 0xdb14, 0xdc3c, 59);
            ecl << pair{0x4099, (int8_t)0x32};
            ECLJumpEx(ecl, 0x52a0, 0x530c, 0);
            ECLJumpEx(ecl, 0x5374, 0x53b4, 0);
            ecl << pair{0x5194, (int16_t)0x0} << pair{0x5350, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST7_MID3:
            ECLJumpEx(ecl, 0xdb14, 0xdc3c, 59);
            ecl << pair{0x4099, (int8_t)0x33};
            ECLJumpEx(ecl, 0x5fd4, 0x6040, 0);
            ECLJumpEx(ecl, 0x60a8, 0x60e8, 0);
            ecl << pair{0x5ec8, (int16_t)0x0} << pair{0x6084, (int16_t)0x0};
            break;
        case THPrac::TH12::TH12_ST7_END_NS1:
            if (thPracParam.dlg)
                ECLJumpEx(ecl, 0xdb14, 0xdd5c, 71);
            else
                nue();
            break;
        case THPrac::TH12::TH12_ST7_END_S1:
            nue();
            ecl << pair{0x788, 0x8fc};
            ecl << pair{0xab0, 9999};
            break;
        case THPrac::TH12::TH12_ST7_END_NS2:
            nue();
            ecl << pair{0x98c, (int8_t)0x32};
            ECLJumpEx(ecl, 0x185c, 0x18dc, 0);
            ecl << pair{0x1768, (int16_t)0x0} << pair{0x1920, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            break;
        case THPrac::TH12::TH12_ST7_END_S2:
            nue();
            ecl << pair{0x98c, (int8_t)0x32};
            ECLJumpEx(ecl, 0x185c, 0x18dc, 0);
            ecl << pair{0x1768, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            ecl << pair{0x15c4, 0x514};
            break;
        case THPrac::TH12::TH12_ST7_END_NS3:
            nue();
            ecl << pair{0x98c, (int8_t)0x33};
            ECLJumpEx(ecl, 0x2240, 0x22c0, 0);
            ecl << pair{0x2160, (int16_t)0x0} << pair{0x2304, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            break;
        case THPrac::TH12::TH12_ST7_END_S3:
            nue();
            ecl << pair{0x98c, (int8_t)0x33};
            ECLJumpEx(ecl, 0x2240, 0x22c0, 0);
            ecl << pair{0x2160, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            ecl << pair{0x1fbc, 0x0a8c};
            break;
        case THPrac::TH12::TH12_ST7_END_NS4:
            nue();
            ecl << pair{0x98c, (int8_t)0x34};
            ECLJumpEx(ecl, 0x2dcc, 0x2e4c, 0);
            ecl << pair{0x2cd8, (int16_t)0x0} << pair{0x2e90, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            break;
        case THPrac::TH12::TH12_ST7_END_S4:
            nue();
            ecl << pair{0x98c, (int8_t)0x34};
            ECLJumpEx(ecl, 0x2dcc, 0x2e4c, 0);
            ecl << pair{0x2cd8, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            ecl << pair{0x2b34, 0x6A4};
            break;
        case THPrac::TH12::TH12_ST7_END_NS5:
            nue();
            ecl << pair{0x98c, (int8_t)0x35};
            ECLJumpEx(ecl, 0x3a2c, 0x3aac, 0);
            ecl << pair{0x3938, (int16_t)0x0} << pair{0x3af0, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            break;
        case THPrac::TH12::TH12_ST7_END_S5:
            nue();
            ecl << pair{0x98c, (int8_t)0x35};
            ECLJumpEx(ecl, 0x3a2c, 0x3aac, 0);
            ecl << pair{0x3938, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            ecl << pair{0x3794, 0x0e10};
            break;
        case THPrac::TH12::TH12_ST7_END_NS6:
            nue();
            ecl << pair{0x98c, (int8_t)0x36};
            ECLJumpEx(ecl, 0x44cc, 0x454c, 0);
            ecl << pair{0x43d8, (int16_t)0x0} << pair{0x4590, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            break;
        case THPrac::TH12::TH12_ST7_END_S6:
            nue();
            ecl << pair{0x98c, (int8_t)0x36};
            ECLJumpEx(ecl, 0x44cc, 0x454c, 0);
            ecl << pair{0x43d8, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            ecl << pair{0x4234, 0x0a8c};
            break;
        case THPrac::TH12::TH12_ST7_END_NS7:
            nue();
            ecl << pair{0x98c, (int8_t)0x37};
            ECLJumpEx(ecl, 0x51e8, 0x5268, 0);
            ecl << pair{0x50f4, (int16_t)0x0} << pair{0x52ac, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            break;
        case THPrac::TH12::TH12_ST7_END_S7:
            nue();
            ecl << pair{0x98c, (int8_t)0x37};
            ECLJumpEx(ecl, 0x51e8, 0x5268, 0);
            ecl << pair{0x50f4, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            ecl << pair{0x4f50, 0x898};
            break;
        case THPrac::TH12::TH12_ST7_END_NS8:
            nue();
            ecl << pair{0x98c, (int8_t)0x38};
            ECLJumpEx(ecl, 0x5f04, 0x5f84, 0);
            ecl << pair{0x5e10, (int16_t)0x0} << pair{0x5fa8, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            break;
        case THPrac::TH12::TH12_ST7_END_S8:
            nue();
            ecl << pair{0x98c, (int8_t)0x38};
            ECLJumpEx(ecl, 0x5f04, 0x5f84, 0);
            ecl << pair{0x5e10, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            ecl << pair{0x5c6c, 0x898};
            break;
        case THPrac::TH12::TH12_ST7_END_S9:
            nue();
            ecl << pair{0x98c, (int8_t)0x39};
            ECLJumpEx(ecl, 0x71dc, 0x725c, 0);
            ecl << pair{0x7114, (int16_t)0x0} << pair{0x72a0, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};
            break;
        case THPrac::TH12::TH12_ST7_END_S10:
            nue();
            ecl << pair{0x98c, (int8_t)0x31} << pair{0x98d, (int8_t)0x30};
            ECLJumpEx(ecl, 0xfc0, 0x106c, 0);
            ecl << pair{0xe6c, (int16_t)0x0} << pair{0xed8, (int16_t)0x0} << pair{0xeec, (int16_t)0x0};
            ecl << pair{0x7bc, 0} << pair{0x7c0, 0x43000000};

            switch (thPracParam.phase) {
                case 1:
                    ecl << pair{0x7a74, 180};
                    ECLSetHealth(ecl, 0x7b00, 0, 4600);
                    ECLJumpEx(ecl, 0x7b14, 0x7e8c, 0);
                    break;
                case 2:
                    ecl << pair{0x7a74, 180};
                    ECLSetHealth(ecl, 0x7b00, 0, 3000);
                    ECLJumpEx(ecl, 0x7b14, 0x823c, 0);
                    break;
                case 3:
                    ecl << pair{0x7a74, 180};
                    ECLSetHealth(ecl, 0x7b00, 0, 1150);
                    ECLJumpEx(ecl, 0x7b14, 0x8610, 0);
                    break;
                case 4:
                    ecl << pair(0x7b48, 0x0e1c); // EXTRA
                    ecl << pair(0x8958, (int16_t)0x15); // EXTRA
                    ecl << pair(0x895e, (int16_t)0x0ff); // EXTRA
            }
            break;
        default:
            break;
        }
    }
    __declspec(noinline) void THSectionPatch()
    {
        ECLHelper ecl;
        ecl.SetBaseAddr((void*)GetMemAddr(0x4b43dc, 0x64, 0xC));

        auto section = thPracParam.section;
        if (section >= 10000 && section < 20000) {
            int stage = (section - 10000) / 100;
            int portionId = (section - 10000) % 100;
            THStageWarp(ecl, stage, portionId);
        } else {
            THPatch(ecl, (th_sections_t)section);
        }
    }

    int THBGMTest()
    {
        if (!thPracParam.mode)
            return 0;
        else if (thPracParam.section >= 10000)
            return 0;
        else if (thPracParam.dlg)
            return 0;
        else
            return th_sections_bgm[thPracParam.section];
    }
    void THSaveReplay(char* repName)
    {
        ReplaySaveParam(mb_to_utf16(repName, 932).c_str(), thPracParam.GetJson());
    }
    __declspec(noinline) void TH12AddVentra(int ventra)
    {
        int32_t item_struct = GetMemAddr(0x4b44f0, 0x171254);
        ventra -= 1;
#ifndef __clang__
        __asm mov eax, ventra;
        __asm mov ecx, item_struct;
        __asm mov edx, 0x4270b0;
        __asm call edx;
#else
        asm volatile(
            "call *%[func]"
            :
            : [func] "r"(0x4270b0), "c"(item_struct), "a"(ventra)
        );
#endif
    }



    static void RenderLockTimer(ImDrawList* p)
    {
        if (g_lock_timer_flag) {
            g_lock_timer++;
            g_lock_timer_flag = false;
        }

        if (g_adv_igi_options.enable_lock_timer_autoly && *THOverlay::singleton().mTimeLock) {
            std::string time_text = std::format("{:.2f}", (float)g_lock_timer / 60.0f);
            auto sz = ImGui::CalcTextSize(time_text.c_str());
            p->AddRectFilled({ 32.0f, 0.0f }, { 110.0f, sz.y }, 0xFFFFFFFF);
            p->AddText({ 110.0f - sz.x, 0.0f }, 0xFF000000, time_text.c_str());
        }
    }

    HOOKSET_DEFINE(THMainHook)
    EHOOK_DY(th12_inf_lives, 0x4381E7,6,{
        if ((*(THOverlay::singleton().mInfLives))) {
            if (!g_adv_igi_options.map_inf_life_to_no_continue) {
                *(DWORD*)(0x004B0C98) = *(DWORD*)(0x004B0C98)+1;
            } else {
                if (*(DWORD*)(0x004B0C98) == 0)
                    *(DWORD*)(0x004B0C98) = 1;
            }
        }
    })
    EHOOK_DY(th12_everlasting_bgm, 0x454960, 10, {
        int32_t retn_addr = ((int32_t*)pCtx->Esp)[0];
        int32_t bgm_cmd = ((int32_t*)pCtx->Esp)[1];
        int32_t bgm_id = ((int32_t*)pCtx->Esp)[2];
        // 4th stack item = i32 call_addr

        bool el_switch;
        bool is_practice;
        bool result;

        el_switch = *(THOverlay::singleton().mElBgm) && !THGuiRep::singleton().mRepStatus && thPracParam.mode && thPracParam.section;
        is_practice = (*((int32_t*)MODEFLAGS) & 0x1);
        result = ElBgmTest<0x430183, 0x4226c7, 0x432820, 0x432982, 0xffffffff>(
            el_switch, is_practice, retn_addr, bgm_cmd, bgm_id, 0xffffffff);

        if (result) {
            pCtx->Eip = 0x4549f4;
        }
    })
    EHOOK_DY(th12_param_reset, 0x43fdfb, 7, {
        thPracParam.Reset();
    })
    EHOOK_DY(th12_prac_menu_1, 0x446059, 7, {
        THGuiPrac::singleton().State(1);
    })
    EHOOK_DY(th12_prac_menu_2, 0x44607b, 3, {
        THGuiPrac::singleton().State(2);
    })
    EHOOK_DY(th12_prac_menu_3, 0x4462d3, 7, {
        THGuiPrac::singleton().State(3);
    })
    EHOOK_DY(th12_prac_menu_4, 0x44636b, 7, {
        THGuiPrac::singleton().State(4);
    })
    PATCH_DY(th12_prac_menu_enter_1, 0x446138, "eb")
    EHOOK_DY(th12_prac_menu_enter_2, 0x446333, 1, {
        pCtx->Eax = thPracParam.stage;
    })
    EHOOK_DY(th12_disable_prac_menu_1, 0x4464e5, 2, {
        pCtx->Eip = 0x44651a;
    })
    PATCH_DY(th12_disable_prac_menu_2, 0x44603e, "83c4106690")
    EHOOK_DY(th12_patch_main, 0x40e8df, 1, {
        uint32_t stageNum = GetMemContent(STAGE_NUM);

        if (stageNum == 1 && !THGuiRep::singleton().mRepStatus && !thPracParam.mode)
            thPracParam.Reset();
        else if (stageNum > 1 && stageNum <= 6 && !(GetMemContent(MODEFLAGS) & 0b10000)) {

            if (THGuiRep::singleton().mRepStatus) { // Playback
                for (int i = 0; i < playerDmgSrcCnt; i++) // if there are already active sources, its a transition - skip
                    if (player->damage_sources[i].flags & 1)
                        return;

                const auto& stageSrcs = thPracParam.reimuADmgSrcs[stageNum - 2];

                if (stageSrcs.size() > 0) {
                    uint32_t curSrcIndex = 0;

                    for (size_t i = 0; i < stageSrcs.size() && i < playerDmgSrcCnt; i++)
                        player->damage_sources[curSrcIndex++] = stageSrcs[i];
                }
            } else { // Recording
                for (int i = 0; i < playerDmgSrcCnt; i++)
                    if (player->damage_sources[i].flags & 1) // (erroneously) active damage source
                        thPracParam.reimuADmgSrcs[stageNum - 2].push_back(player->damage_sources[i]);
            }
        }
        if (thPracParam.mode == 1) {
            *(int32_t*)(0x4b0c44) = (int32_t)(thPracParam.score / 10);
            *(int32_t*)(0x4b0c98) = thPracParam.life;
            *(int32_t*)(0x4b0c9c) = thPracParam.life_fragment ? (thPracParam.life_fragment + 1) : 0;
            *(int32_t*)(0x4b0ca0) = thPracParam.bomb;
            *(int32_t*)(0x4b0ca4) = thPracParam.bomb_fragment * 2;
            *(int32_t*)(0x4b0c48) = thPracParam.power;
            *(int32_t*)(0x4b0c78) = thPracParam.value * 100;
            *(int32_t*)(0x4b0cdc) = thPracParam.graze;
            *(int32_t*)(*(uint32_t*)0x4b44f0 + 0x666fe0) = thPracParam.ufo_side;

            if (thPracParam.ventra_1) {
                TH12AddVentra(thPracParam.ventra_1);
                if (thPracParam.ventra_2) {
                    TH12AddVentra(thPracParam.ventra_2);
                    if (thPracParam.ventra_3) {
                        TH12AddVentra(thPracParam.ventra_3);
                    }
                }
            }

            THSectionPatch();
        }
        thPracParam._playLock = true;

        // fix potential desync w/ iframes being given to player
        // when starting replay on non-st1
        if (!THGuiRep::singleton().mRepStatus)
            return; // must be in replay
        if ((GetMemContent(STAGE_NUM) - 1) % 6 == 0)
            return; // must not be st1/7
        if (!GetMemContent(REPLAY_MGR_PTR, 0xa8 + 0x24 * 1))
            return; // must have st1 in replay

        player->iframes.previous = 1;
        player->iframes.current = 0;
        player->iframes.current_f = 0.0f;
    })
    EHOOK_DY(th12_bgm, 0x42293a, 1, {
        if (THBGMTest()) {
            PushHelper32(pCtx, 1);
            pCtx->Eip = 0x42293b;
        }
    })
    EHOOK_DY(th12_rep_save, 0x43c32c, 7, {
        char* repName = (char*)(pCtx->Esp + 0x28);
        if (thPracParam.mode || !(GetMemContent(CHARA) || GetMemContent(SUBSHOT)))
            THSaveReplay(repName); // thprac mode or ReimuA
    })
    EHOOK_DY(th12_rep_menu_1, 0x4467df, 3, {
        THGuiRep::singleton().State(1);
    })
    EHOOK_DY(th12_rep_menu_2, 0x4468f8, 5, {
        THGuiRep::singleton().State(2);
    })
    EHOOK_DY(th12_rep_menu_3, 0x446ab1, 2, {
        THGuiRep::singleton().State(3);
    })
    EHOOK_DY(th12_update, 0x4625fb, 1, {
        GameGuiBegin(IMPL_WIN32_DX9, !THAdvOptWnd::singleton().IsOpen());

        // Gui components update
        THGuiPrac::singleton().Update();
        THGuiRep::singleton().Update();
        THOverlay::singleton().Update();
        TH12Logger::singleton().Update();
        TH12InGameInfo::singleton().Update();
        
        auto p = ImGui::GetOverlayDrawList();
        // in case boss movedown do not disabled when playing normal games
        {
            if (THAdvOptWnd::singleton().forceBossMoveDown) {
                auto sz = ImGui::CalcTextSize(S(TH_BOSS_FORCE_MOVE_DOWN));
                p->AddRectFilled({ 120.0f, 0.0f }, { sz.x + 120.0f, sz.y }, 0xFFCCCCCC);
                p->AddText({ 120.0f, 0.0f }, 0xFFFF0000, S(TH_BOSS_FORCE_MOVE_DOWN));
            }
        }
        SSS::SSS_Update(12);
        
        if (g_adv_igi_options.show_keyboard_monitor && player) {
            g_adv_igi_options.keyboard_style.size = { 40.0f, 40.0f };
            KeysHUD(12, { 1280.0f, 0.0f }, { 835.0f, 0.0f }, g_adv_igi_options.keyboard_style);
        }


        {
            //0x4B0CE0 prac mode
            if (g_show_bullet_hitbox) 
            {
                bool is_prac = ((*(byte*)(0x4B0CE0) == 0x10));
                bool is_rep = false;
                if (*(DWORD*)(0x4B4518))
                {
                    is_rep = (*(DWORD*)(*(DWORD*)(0x4B4518) + 0x10) == 1);
                }
                if (is_prac || is_rep)
                {
                    ImGuiIO& io = ImGui::GetIO();
                    float x_ratio = io.DisplaySize.x / 1280.0f;
                    float y_ratio = io.DisplaySize.y / 960.0f;
                    p->PushClipRect({ 64.0f * x_ratio, 32.0f * y_ratio }, { 832.0f * x_ratio, 928.0f * y_ratio });
                    for (auto& i : g_th12_laser_hit_draw_vec) {
                        p->AddQuadFilled(i.posA, i.posB, i.posC, i.posD, 0x88CC0000);
                        p->AddQuad(i.posA, i.posB, i.posC, i.posD, 0xCCFFFFFF);
                    }
                    g_th12_laser_hit_draw_vec = {};
                    p->PopClipRect();
                }
            }
        }
        RenderLockTimer(p);
        GameUpdateOuter(p, 12);

        bool drawCursor = THAdvOptWnd::StaticUpdate() || THGuiPrac::singleton().IsOpen();
        GameGuiEnd(drawCursor);
    })
    EHOOK_DY(th12_player_state, 0x436BA0,1,{
        if (g_adv_igi_options.show_keyboard_monitor)
            RecordKey(12, *(DWORD*)(0x4D49D0));
    })
    EHOOK_DY(th12_render, 0x462722, 1, {
        GameGuiRender(IMPL_WIN32_DX9);
    })
    HOOKSET_ENDDEF()

    HOOKSET_DEFINE(THInGameInfo)
    EHOOK_DY(th12_game_start, 0x421DEF,6,// gamestart-bomb set
    {
        TH12InGameInfo::singleton().mBombCount = 0;
        TH12InGameInfo::singleton().mMissCount = 0;
        TH12InGameInfo::singleton().mRUFOCount = 0;
        TH12InGameInfo::singleton().mGUFOCount = 0;
        TH12InGameInfo::singleton().mBUFOCount = 0;
        TH12InGameInfo::singleton().mCUFOCount = 0;

        TH12InGameInfo::singleton().mRVentraCount_Get = 0;
        TH12InGameInfo::singleton().mBVentraCount_Get = 0;
        TH12InGameInfo::singleton().mGVentraCount_Get = 0;

        TH12InGameInfo::singleton().mRVentraCount_Drop = 0;
        TH12InGameInfo::singleton().mBVentraCount_Drop = 0;
        TH12InGameInfo::singleton().mGVentraCount_Drop = 0;
        TH12InGameInfo::singleton().mCVentraCount_Drop = 0;

        TH12Logger::singleton().OnStageStart();
    })
    EHOOK_DY(th12_bomb_dec, 0x422F28,5, // bomb dec
    {
        TH12InGameInfo::singleton().mBombCount++;
        TH12Logger::singleton().OnBomb();
    })
    EHOOK_DY(th12_life_dec, 0x4381E2,5, // life dec
    {
        TH12InGameInfo::singleton().mMissCount++;
        TH12Logger::singleton().OnMiss();
        FastRetry(thPracParam.mode);
    })
    EHOOK_DY(th12_ufo_spawn, 0x44A909, 7, // spawn ufo
    {
        int32_t type = pCtx->Eax;
        if (type == 0)
            TH12InGameInfo::singleton().mRUFOCount++;
        else if (type == 1)
            TH12InGameInfo::singleton().mBUFOCount++;
        else if (type == 2)
            TH12InGameInfo::singleton().mGUFOCount++;
        else if (type == 3)
            TH12InGameInfo::singleton().mCUFOCount++;
    })
    EHOOK_DY(th12_ufo_item, 0x4270b0, 3, { // ufo item eat
        int32_t type = pCtx->Eax;
        switch (type)
        {
            case 0:TH12InGameInfo::singleton().mRVentraCount_Get++;break;
            case 1:TH12InGameInfo::singleton().mBVentraCount_Get++;break;
            case 2:TH12InGameInfo::singleton().mGVentraCount_Get++;break;
            default:
                break;
        }
    })
    EHOOK_DY(th12_ufo_item2, 0x427411, 6, { // ufo item drop
        int32_t type = pCtx->Eax;
        switch (type)
        {
            case 0:TH12InGameInfo::singleton().mRVentraCount_Drop++;break;
            case 1:TH12InGameInfo::singleton().mBVentraCount_Drop++;break;
            case 2:TH12InGameInfo::singleton().mGVentraCount_Drop++;break;

            case 3:TH12InGameInfo::singleton().mCVentraCount_Drop++;break;
            case 4:TH12InGameInfo::singleton().mCVentraCount_Drop++;break;
            case 5:TH12InGameInfo::singleton().mCVentraCount_Drop++;break;
            default:
                break;
        }
    })
    EHOOK_DY(th12_lock_timer1, 0x41D4E5,6, // initialize
    {
        g_lock_timer = 0;
    })
    EHOOK_DY(th12_lock_timer2, 0x418745,5, // SetNextPattern case 414
    {
        g_lock_timer = 0;
    })
    EHOOK_DY(th12_lock_timer3, 0x417AEF,2, // set boss mode case 412
    {
        g_lock_timer = 0;
    })
    EHOOK_DY(th12_lock_timer4, 0x41A821,6, // decrease time (update)
    {
        g_lock_timer_flag = true;
    })
    HOOKSET_ENDDEF()

    static __declspec(noinline) void THGuiCreate()
    {
        // Init
        if (ImGui::GetCurrentContext()) {
            return;
        }
        GameGuiInit(IMPL_WIN32_DX9, 0x4ce8f0, 0x4cf3f0,
            Gui::INGAGME_INPUT_GEN2, 0x4d48c4, 0x4d48c0, 0,
            -1);
        SetDpadHook(0x462CAF, 2);

        // Gui components creation
        THGuiPrac::singleton();
        THGuiRep::singleton();
        THOverlay::singleton();
        TH12InGameInfo::singleton();
        TH12Logger::singleton();

        // Hooks
        EnableAllHooks(THMainHook);
        EnableAllHooks(THInGameInfo);

        // Reset thPracParam
        thPracParam.Reset();
    }
    HOOKSET_DEFINE(THInitHook)
    PATCH_DY(th12_disable_demo, 0x43f78f, "ffffff7f")
    PATCH_DY(th12_disable_mutex, 0x44f603, "eb")
    PATCH_DY(th12_startup_1, 0x43f357, "eb")
    PATCH_DY(th12_startup_2, 0x43fe69, "eb")
    EHOOK_DY(th12_gui_init_1, 0x440808, 3, {
        self->Disable();
        THGuiCreate();
    })
    EHOOK_DY(th12_gui_init_2, 0x4511f9, 1, {
        self->Disable();
        THGuiCreate();
    })
    HOOKSET_ENDDEF()
}

void TH12Init()
{
    EnableAllHooks(TH12::THInitHook);
    InitHook(12,(void*)0x450dcc);
    if (GetModuleHandleA("vpatch_th12.dll")) {
        InitHook(12,(void*)((DWORD)GetModuleHandleA("vpatch_th12.dll") + 0x54ce));
    }
}
}

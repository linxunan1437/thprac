#pragma once

#include <cstdint>

namespace THPrac {
namespace TH15 {

    // ============================================================================
    // TH15 miss/bomb 记录：游戏内存偏移/常量
    // 来源：反编译确认（docs/th15/th15_re_disasm_report2.md / report3.md）。
    //
    // 下文注释里的 boss 指“当前 boss 对象”。TH15 没有保存 boss 对象的全局指针，
    // 需要在 case 512（0x42B938）/ case 514（0x42C738）两个 ECL 解释器钩子里
    // 缓存 pCtx->Edi（EDI 恒为当前 boss 对象，全程不变）。
    // ============================================================================

    // [boss+0x4060]：uint32 标志位，bit23 = 是否处于 boss 战（进入 boss 战置位、
    // 离场清除）。该位为 0 表示道中/非 boss，此时不记录阶段名。
    //   bool inBoss = (*(uint32_t*)(boss + TH15_MB_BOSS_GATE_OFF) & TH15_MB_BOSS_GATE_MASK) != 0;
    #define TH15_MB_BOSS_GATE_OFF  0x4060
    #define TH15_MB_BOSS_GATE_MASK 0x800000u   // bit23

    // 0x4E73F0：int32，当前关卡号（1~7）。绝对地址，使用前需经 RVA() 重定位。
    #define TH15_MB_STAGE_ADDR     0x4E73F0

    // ===== TH15 miss/bomb 记录助手 =====
    // 职责：缓存 0x202 新注册的攻击名 -> 按 <stage>-<name> 查表映射为输出名
    //       -> 一局一行落盘。
    // 仅 thprac 练习模式记录；不含 IGI 显示；记录路径不做任何堆分配。
    class TH15MissBombRecorder {
    public:
        // 新一局边界（th15_game_start）。recordEnabled = 练习模式时为 true。
        void OnGameStart(bool recordEnabled);
        // 每帧调用（th15_update）：PLAYER_PTR 非零表示本局主体已开始，
        // 用于确认“一局”已经真正开始，避免 game_start 与 player 创建之间的空窗被误判为结束。
        void MarkRunActive();
        // 缓存 boss 对象指针（case 514 / case 512 钩子，pCtx->Edi）。
        void CacheBossPtr(uintptr_t bossPtr);
        // boss 进场（case 512，EAX>=0）：清空上一 boss 的攻击名缓存。
        void OnBossEnter();
        // 换攻击钩（0x42C7D6 normal / 0x42C785 BossDead 共用）。
        // newNamePtr = [ESP+8] = 本次 0x202 新写入的名字串指针。直接缓存该新值，
        // 落盘时再经映射表换算成输出名。
        void OnPatternSwitch(uintptr_t newNamePtr);
        // 记录一次 miss/bomb。kind: 'm'/'b'；stage: 当前关卡号(1~7)。
        void Record(char kind, int stage);
        // 一局结束（PLAYER_PTR 归零）落盘。
        void OnRunEnd();

        // ===== 当前章节（ECL sub）读取 =====
        // 主线宿主捕获（0x43BBC8 / 0x43D48E / 0x43D6BE，EAX = 宿主对象）。
        void OnEclHostCaptured(uintptr_t host);
        // 当前主线 sub 名（读 ECL 资源 sub 表）；不可用时返回 nullptr。
        const char* GetEclSubName() const;
        // 上一次 miss/bomb 的位置文本（形如 "m(道中2)" / "b(BossCard1)"）；未记录过返回 nullptr。
        const char* GetLastEvent() const;

    private:
        // 查 <stage>-<name> 映射表；未命中返回 nullptr（调用方回退原始名）。
        const char* LookupMappedName(int stage, const char* name) const;
        // 查 <stage>-<当前sub名> 道中段落映射表；未命中返回 nullptr（回退原始 sub 名）。
        const char* LookupMidStageName(int stage, const char* eclSubName) const;
        // 缓存的原始攻击名；非 boss / 无缓存时返回 nullptr。
        const char* GetBossStageName() const;

        bool mLineOpen = false;
        bool mRunBodySeen = false;   // 本局是否已见过 PLAYER_PTR != 0（确认一局真正开始）
        char mStartTime[32] = {};
        char mLineBuf[4096] = {};
        int  mLineLen = 0;

        uintptr_t mBossPtr = 0;
        char mCurStageName[64] = {};

        uintptr_t mEclHost = 0;      // 关卡主线 ECL 宿主（三个 spawn 点捕获）
        char mLastEvent[64] = {};    // 上一次 miss/bomb 位置（IGI 显示，形如 "m(道中2段)"）
    };

    extern TH15MissBombRecorder g_th15mb;

}
}

# thprac_th15.cpp 钩子（Hook）分析

> 分析对象：`thprac/src/thprac/thprac_th15.cpp`（约 2750 行）
>
> 适用游戏：**东方绀珠传（TH15, Legacy of Lunatic Kingdom）**
>
> 本文档梳理该文件通过代码补丁（PATCH）与执行流钩子（EHOOK）对游戏进程进行的所有修改，以及各类钩子对应的功能。

---

## 一、钩子框架说明

thprac 的钩子均基于 `thprac_hook.h` 定义的一套宏，需要先了解这些宏才能读懂下文的表格：

| 宏 | 含义 |
| --- | --- |
| `EHOOK_ST` / `EHOOK_DY` | 执行流钩子。在原地址覆盖 `instr_size_` 字节并替换为跳转到回调的指令，回调收到寄存器上下文 `PCONTEXT pCtx` 与钩子自身 `HookCtx* self`。回调内可读写寄存器（如 `pCtx->Eip`、`pCtx->Eax`）控制原始执行流。`_ST` 表示独立单个钩子，`_DY` 表示声明在 `HOOKSET` 内的钩子。 |
| `PATCH_ST` / `PATCH_DY` | 字节补丁。直接把十六进制字符串（如 `"eb"`）逐字节写入目标地址，不触发回调。用于改写一条跳转/比较/立即数指令。 |
| `HOOKSET_DEFINE` / `HOOKSET_ENDDEF` | 一组钩子的集合定义，用 `EnableAllHooks(名字)` 批量安装。 |
| `HOTKEY_DEFINE` / `HOTKEY_ENDDEF` | 定义一个热键，内部通过 `PATCH_HK(addr, code)` 在热键开启时安装补丁、关闭时还原。 |
| `self->Enable()` / `self->Disable()` | 运行时启用 / 禁用某个钩子（一次性钩子常 `self->Disable()` 后只执行一次）。 |
| `asm_call<addr, CallType>` | 直接以指定调用约定调用游戏内部地址的函数（不是钩子，是调用原始游戏功能）。 |

代码中的地址均为游戏的静态基地址（thprac 通过 `ingame_image_base` 做重定位）。TH15 被挂载的模块除主 exe 外，还包括 `vpatch_th15.dll`（见文末 `TH15Init`）。

---

## 二、热键作弊 / 便利功能（`THOverlay`）

`THOverlay` 是游戏内“Mod Menu”热键菜单，每个热键通过 `PATCH_HK` 挂一个小字节补丁：

| 热键 | 名称常量 | 功能 | 补丁 |
| --- | --- | --- | --- |
| `BACKSPACE` | ModMenuToggle | 打开 / 关闭 Mod Menu 本体（非补丁，仅 GUI 开关） | — |
| `F1` | `TH_MUTEKI` | **无敌（Muteki）** | `0x4566a5` 处写 `01` |
| `F2` | `TH_INFLIVES2` | **无限残机**（无补丁，由主 hook `th15_inf_lives` 实现，见下文） | — |
| `F3` | `TH_INFBOMBS` | **无限炸弹** | `0x414963` 处写 `90`（NOP 掉扣雷指令） |
| `F4` | `TH_INFPOWER` | **无限火力（Power 恒满）** | `0x4582fa` 处写 `45` |
| `F5` | `TH_TIMELOCK` | **时间锁定（雷锁，Bomb 槽时间不回退）** | `0x41fdf5` 处写 `eb`（改跳转）；`0x428b5d` 处写 `a7` |
| `F6` | `TH_AUTOBOMB` | **自动雷（被弹瞬间自动放 B）** | `0x454cc9` 处写 `c6` |
| `F7` | `TH_EL_BGM` | **永恒 BGM（BGM 不切换/不停止循环）** | 无补丁，由主 hook `th15_everlasting_bgm` 实现 |
| `F8` | `THPRAC_INGAMEINFO` | **游戏内信息显示（InGameInfo 面板）** | 无补丁，控制 `TH15InGameInfo` 窗口显示 |

---

## 三、练习模式系统（`THGuiPrac` + 主流程 hook）

### 3.1 练习参数结构 `THPracParam`

一个 JSON 可序列化的参数结构，记录练习模式的选择：`mode/stage/section/phase`（模式/关卡/段落/阶段）以及开局资源（分数、残机、残机碎片、炸弹、炸弹碎片、火力、信仰值、擦弹、铃仙护盾、道中参数等）。它会写进录象文件用于回放恢复，也会在开局时写入游戏内存。

### 3.2 练习菜单挂钩（进入练习模式选关界面）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th15_prac_menu_1` | `0x4677be`（5 字节） | 练习菜单状态 1：保存当前难度、打开练习 GUI、重置 `thPracParam` |
| `th15_prac_menu_2` | `0x4677e1`（3 字节） | 状态 2：预留分支 |
| `th15_prac_menu_3` | `0x467b2a`（7 字节） | 状态 3（确认）：把 GUI 选项收集进 `thPracParam` |
| `th15_prac_menu_4` | `0x467bcb`（7 字节） | 状态 4（退出）：关闭 GUI |
| `th15_prac_menu_enter_1` | `0x4678a2` | 字节补丁 `eb`：跳过菜单某一分支 |
| `th15_prac_menu_enter_2` | `0x467b8a`（1 字节） | 把 `thPracParam.stage` 写进 `Ecx`，让游戏按所选关卡进入 |
| `th15_disable_prac_menu_1` | `0x467d31` | 字节补丁 `eb3b`：禁用一个菜单分支 |
| `th15_menu_rank_fix` | `0x4527be`（5 字节） | 修复菜单难度显示：强制 `0x4e7424=-1`、`0x4e7410=0x4e0ef4`（恢复真实难度值） |

### 3.3 开局写入参数（核心）`th15_patch_main`

地址 `0x43c68c`（1 字节 EHOOK），练习开局时执行：

- 先禁用一次性钩子 `th15_chapter_set` / `th15_chapter_disable` / `th15_stars_bgm_sync`；
- 若处于练习模式（`thPracParam.mode==1`），把 `thPracParam` 中的分数、残机、残机碎片、炸弹、炸弹碎片、火力、信仰值、擦弹数依次写入对应内存地址（`0x4E740C / 0x4E7450 / 0x4E7454 / 0x4E745C / 0x4E7460 / 0x4E7440 / 0x4E7434 / 0x4E741C`）；
- 调用 `THSectionPatch()` 对当前段落的 ECL 脚本做二进制补丁（见 3.4）；
- 若选择了铃仙护盾，激活炸弹并构造护盾 ANM（见 3.5）。

### 3.4 章节 / ECL 脚本补丁

练习的核心不是钩子而是**直接改写游戏加载的 ECL（弹幕脚本）二进制**。`THSectionPatch()` 通过 `ECLHelper`（VFile）把补丁写入 `0x4e9a80 → +0x17c → +0xC` 处的 ECL 数据区：

- `THStageWarp`：按“关 + 段落”把 ECL 指针跳转到对应段落的入口（跳过前面的段落），大量 `ECLJump` 调用把章节连接起来；
- `THPatch`：按关卡段（`TH15_ST*` 枚举）做细粒度改写——跳过对话、设置道中 Boss 血量和符卡序号、移除移动限制、禁用掉落物与音效、修改待机/无敌/移动时间、隐藏纯狐/赫卡提亚的子 Boss、注入自定义 ANM/精灵指令等。最典型的是把“非符/符卡练习”直接跳转到 Boss 召唤指令处。

服务于这些补丁的一次性执行流钩子：

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th15_chapter_set` | `0x43dd58`（6 字节） | 章节跳转时把 `chapter_set` 写入 `0x4e73f8`（当前章节号），随后自禁用 |
| `th15_chapter_disable` | `0x43d0b5`（2 字节） | 计数递减，为 0 时自禁用并跳到 `0x43d0d5`，用于“跳过章节” |
| `th15_st7boss1_chapter_bonus` | `0x43dece`（3 字节） | 自禁用并写 `0x4e7484=1`，强制六面 Boss1 章节奖励成立 |
| `th15_stars_bgm_sync` | `0x48c294`（2 字节） | 六面“星星（无限符）模式”下拦截 BGM 调用：当调用方为 `0x48B4EB` 时把栈上参数改为 `0x8fc768`（BGM 同步），随后自禁用 |

### 3.5 铃仙护盾 ANM（`MakeReisenShieldANM`）

不是钩子，而是直接调用游戏内部函数：`ALLOCATE_ANM_VM` / `COPY_ANM_VM_FROM_LOADED` / `ANM_VM_RUN` / `APPEND_ANM_VM_TO_WORLD_LIST`（`asm_call`），把玩家炸弹对象挂到世界中，再根据护盾数量手动标记子节点删除，实现练习开局即带 1~3 层铃仙护盾。

### 3.6 练习 BGM

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th15_bgm` | `0x43d5c1`（2 字节） | 根据 `THBGMTest()` 查表得到练习段落对应的 BGM id，若命中则 `PushHelper32(pCtx, 1)` 并跳到 `0x43d5c3` 覆盖原 BGM 调用，使每个练习段落播放正确 BGM |
| `th15_param_reset` | `0x461070`（7 字节） | 游戏初始化路径上重置 `thPracParam` |

---

## 四、录象系统（`THGuiRep`）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th15_rep_save` | `0x45cc49`（5 字节） | 保存录象时把 `thPracParam.GetJson()`（练习参数）附加写入录象文件，便于回放时恢复练习环境 |
| `th15_rep_menu_1` | `0x468197`（3 字节） | 录象菜单状态 1：重置录象 GUI 状态与 `thPracParam` |
| `th15_rep_menu_2` | `0x4682b0`（5 字节） | 状态 2：读取当前选中的录象文件并解析其中的参数 |
| `th15_rep_menu_3` | `0x468474`（2 字节） | 状态 3：若参数有效则 `memcpy` 到 `thPracParam`，实现“看录象 = 复现当时练习配置” |

---

## 五、游戏内信息与统计（`TH15InGameInfo` + `THInGameInfo` hookset）

`TH15InGameInfo` 窗口显示在右上角，数据来自两处：`THInGameInfo` hookset 统计的 Miss / Bomb 次数，以及直接读取游戏内存（难度、玩家、各关重试次数、击破率、擦弹、道中信息等）。进入道中练习段落时还显示道中重试次数与击破率/擦弹乘积。

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th15_game_start` | `0x43E6EE`（7 字节） | 开局（bomb-set）时把 `mBombCount` / `mMissCount` 清零；并作 miss/bomb 记录（`TH15MissBombRecorder`）的“新一局”边界——清空 boss 指针缓存 `g_boss_ptr` 与当前攻击名缓存 `g_cur_stage_name`，若当前行未开则开新行（continue / FastRetry 属同一局，不另起行） |
| `th15_bomb_dec` | `0x41497A`（5 字节） | 炸弹数减少时 `mBombCount++`（统计用雷数）；开启记录时调用 `g_missbomb_rec.Record('b', 关卡号, boss阶段名)` |
| `th15_life_dec` | `0x456398`（5 字节） | 残机数减少时 `mMissCount++`，并调用 `FastRetry(thPracParam.mode)` —— 练习模式下掉命立即重开；开启记录时调用 `g_missbomb_rec.Record('m', 关卡号, boss阶段名)` |
| `th15_inf_lives` | `0x456397`（1 字节） | 与上者相邻。`F2`（无限残机）开启时 `pCtx->Eax++` 抵消扣命；若开启“映射到无续关”选项则仅在 `0x4E7450==0`（无续关）时生效 |
| `th15_lock_timer1` | `0x43404A`（10 字节） | 雷锁计时初始化：`g_lock_timer=0` |
| `th15_lock_timer2` | `0x42C738`（4 字节） | `SetNextPattern` case 514（Boss 切换弹幕）时清零；同时 `g_boss_ptr = pCtx->Edi` 缓存 boss 对象指针（配合 miss/bomb 记录读取当前攻击名） |
| `th15_pattern_switch` | `0x42C7D6`（5 字节） | `SetNextPattern` normal 分支的 `call 0x0042ff10`（写 HP+名字串）。miss/bomb 记录用：命中时读槽**写前旧值**（`scr + EAX*0x88 + 0x528C` = 当前攻击名）缓存到 `g_cur_stage_name`；槽空时兜底缓存新写入名（`[ESP+8]`）。回调只读 pCtx，原 call 由 codecave 自动回放。依据 `th15_re_disasm_report3.md` §2 |
| `th15_pattern_switch_bossdead` | `0x42C785`（5 字节） | `SetNextPattern` BossDead 分支的 `call 0x0042ff10`，逻辑同 `th15_pattern_switch`（两分支互斥，都挂才覆盖全部 0x202 换攻击） |
| `th15_lock_timer3` | `0x42B938`（2 字节） | `set boss mode` case 512（Boss 进入状态）时清零；同时 `g_boss_ptr = pCtx->Edi` 缓存 boss 对象指针；`EAX`(ECL 参数[0])≥0（boss 进场）时清空 `g_cur_stage_name` |
| `th15_lock_timer4` | `0x4301E8`（6 字节） | 时间递减（更新）路径上置 `g_lock_timer_flag=true`，由每帧 `RenderLockTimer` 累加并绘制秒数（配合 `TH_TIMELOCK` 观察雷锁时长） |

> **miss/bomb 记录**（`TH15MissBombRecorder`，高级选项开关默认开启，仅 thprac 练习模式记录）：在 miss/bomb 发生时把“关卡号 + boss 当前阶段名”追加写入游戏目录 `th15_missbomb.txt`，一局一行（行首时间戳）。boss 阶段名 = **换攻击钩缓存的 `g_cur_stage_name`**（ECL 攻击名字串，如 `Boss1`/`BossCard1`/`BossDead`，`BossN`=第 N 个非符、`BossCardN`=第 N 张符卡）：`0x42C7D6`/`0x42C785` 命中（`call 0x0042ff10` 写前）时缓存槽**写前旧值** `scr + 槽索引(EAX)*0x88 + 0x528C`（= 当前攻击名），槽空兜底缓存新写入名；是否在 boss 战 = `[g_boss_ptr+0x4060] & 0x800000`（否则记为 `-`）。依据：`th15_re_disasm_report2.md` §3.2 + `th15_re_disasm_report3.md` §2（TH15 无“当前攻击序号”/“当前攻击名”持久内存字段，0x202 预注册下一攻击，槽内恒为“下一攻击”名；名字串真实写偏移 `+0x528C`/`+0x52CC`，`+0x528B` 是 HP dword 高字节，`[boss+0x4080]` 无写点）。一局结束（残机耗尽/退回主菜单，`PLAYER_PTR` 归零）由 `TH15InGameInfo::OnPreUpdate` 轮询检测并落盘。IGI 显示：miss/bomb 计数下方显示**最近 5 条 miss/bomb 事件**（各 1 行，无事件显示 `NN`）；窗口高度固定为屏幕 38%（`SetSizeRel(360/1280, 0.38f)`）容纳列表。

---

## 六、主循环与渲染挂钩（`THMainHook` 剩余部分）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th15_update` | `0x4015fa`（1 字节） | **每帧主更新**：`GameGuiBegin` 开始 ImGui 帧 → 依次 `Update()` 练习菜单 / 录象菜单 / 热键菜单 / InGameInfo → 绘制“Boss 强制下移”提示、AB Test 结果、SSS、键盘监控（`KeysHUD`）、锁时计 → `GameUpdateOuter` 收尾 → `GameGuiEnd`。相当于把整个 mod 的 UI 与逻辑并入游戏主循环 |
| `th15_render` | `0x40170a`（1 字节） | 渲染 ImGui 帧（`GameGuiRender(IMPL_WIN32_DX9)`），把 UI 画到屏幕 |
| `th15_player_state` | `0x454a70`（1 字节） | 玩家状态更新时调用 `RecordKey(15, 按键内存)`，供键盘监控（KeysHUD）录制按键序列 |
| `th15_everlasting_bgm` | `0x476f10`（1 字节） | **永恒 BGM**：挂 BGM 控制函数入口，用 `ElBgmTest` 判断当前 BGM 是否被锁；练习模式下阻止 BGM 停止/切换（`F7`），命中时跳到 `0x476fa3` 跳过原始逻辑 |

---

## 七、高级选项（`THAdvOptWnd`）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th15_all_clear_bonus_1` | `0x43d99d`（补丁 `eb0b909090`） | 强制“全关奖励”分支成立（练习模式也有全关奖励） |
| `th15_all_clear_bonus_2` | `0x43daac`（7 字节） | 把当前分数写入 `0x4e9a8c+0x160`；若处于实践模式标记（`0x4e7794 & 0x10`）则调用 `0x4512a0` 分数函数并跳回 `0x43d9a2`，保证全关奖励正常累计 |
| `th15_all_clear_bonus_3` | `0x43dcb5`（7 字节） | 与 `th15_all_clear_bonus_2` 同逻辑，另一结算入口 |
| `th15_master_disable`（Hookset，3 个补丁） | `0x420346`=`eb`、`0x42036B`=`eb`、`0x420296`=`00` | 禁用 Boss 的“月面大师”自动雷（`disable_master_autoly` 选项，由 `TH_DISABLE_MASTER` 勾选项控制开关） |
| `th15_bossmovedown` | `0x0042B261`（5 字节） | **Boss 强制下移**：回调内读取 `Edi+0x3F64/0x3F6C`（Boss 的 Y 坐标与活动范围），按 `g_bossMoveDownRange` 重新计算并写入，把 Boss 拉到底部；高级选项里可拖滑条调范围 |
| `mOptCtx` FPS 相关 | — | 非钩子：检测 `openinputlagpatch.dll` / `vpatch_th15.dll` 后直接改写其内部字段（FPS 值），或把 `0x472802` 处的 FPS 指针替换为 mod 的 `fps_dbl`（FPS 解锁） |

---

## 八、AB 测试（ST8 特殊模式）

`TH15_ST8_AB_TEST`（道中 Boss “星灵”练习的隐藏测试）是一套独立的子功能：开局把 `th15_abtest_ecl_file` 注入 ECL、设置测试阶段；运行期间由 `TH15InGameInfo` 显示 5 项原始分数，`ABTestRender()` 用雷达图展示反应/移动/回返/控场/精准/运气 6 维评级结果。这部分由主循环 `th15_update` 驱动，无额外地址钩子。

---

## 九、初始化挂钩（`THInitHook` + `TH15Init`）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th15_disable_demo` | `0x460900`（补丁 `ffffff7f`） | 禁用演示（DEMO）模式，避免演示画面触发崩溃 |
| `th15_disable_mutex` | `0x4713ec`（补丁 `90e9`） | 绕过进程互斥量，支持游戏多开 |
| `th15_startup_1` | `0x46055f`（补丁 `9090`） | 启动流程补丁 1（跳过某次校验/分支） |
| `th15_startup_2` | `0x4610c3`（补丁 `eb`） | 启动流程补丁 2 |
| `th15_gui_init_1` | `0x4617ff`（3 字节） | 自禁用后调用 `THGuiCreate()`：初始化 ImGui + DX9 界面、创建全部 GUI 窗口、`EnableAllHooks(THMainHook/THInGameInfo)`、安装章节辅助钩子 |
| `th15_gui_init_2` | `0x47341b`（1 字节） | 另一条初始化路径上的同样处理 |

`TH15Init()`（文件外调用入口）：

```cpp
EnableAllHooks(TH15::THInitHook);                    // 安装上述初始化钩子
InitHook(15, (void*)0x47356c, (void*)0x47333d);      // 通用初始化钩子（exe 与刷新率相关地址）
if (GetModuleHandleA("vpatch_th15.dll"))             // 若加载了 vpatch，对其代码段也做
    InitHook(15, (void*)((DWORD)GetModuleHandleA("vpatch_th15.dll") + 0x6bd9));
```

另：`THGuiCreate()` 中还会调用 `SetDpadHook(0x401D22, 3)`，对游戏的方向键（十字键）输入函数挂钩，用于实现菜单/界面内的按键导航。

---

## 附录：钩子地址总表

| 地址 | 类型 | 名称 | 归属 |
| --- | --- | --- | --- |
| `0x4566a5` | 补丁 `01` | mMuteki（无敌） | THOverlay 热键 |
| `0x414963` | 补丁 `90` | mInfBombs（无限雷） | THOverlay 热键 |
| `0x4582fa` | 补丁 `45` | mInfPower（无限火力） | THOverlay 热键 |
| `0x454cc9` | 补丁 `c6` | mAutoBomb（自动雷） | THOverlay 热键 |
| `0x41fdf5` | 补丁 `eb` | mTimeLock（雷锁） | THOverlay 热键 |
| `0x428b5d` | 补丁 `a7` | mTimeLock（雷锁） | THOverlay 热键 |
| `0x43d99d` | 补丁 `eb0b909090` | th15_all_clear_bonus_1 | 高级选项 |
| `0x43daac` | EHOOK 7B | th15_all_clear_bonus_2 | 高级选项 |
| `0x43dcb5` | EHOOK 7B | th15_all_clear_bonus_3 | 高级选项 |
| `0x420346` | 补丁 `eb` | th15_master_disable1a | 高级选项 |
| `0x42036B` | 补丁 `eb` | th15_master_disable1b | 高级选项 |
| `0x420296` | 补丁 `00` | th15_master_disable1c | 高级选项 |
| `0x0042B261` | EHOOK 5B | th15_bossmovedown | 高级选项 |
| `0x43dd58` | EHOOK 6B | th15_chapter_set | 章节补丁 |
| `0x43d0b5` | EHOOK 2B | th15_chapter_disable | 章节补丁 |
| `0x43dece` | EHOOK 3B | th15_st7boss1_chapter_bonus | 章节补丁 |
| `0x48c294` | EHOOK 2B | th15_stars_bgm_sync | 章节补丁 |
| `0x456397` | EHOOK 1B | th15_inf_lives | THMainHook |
| `0x476f10` | EHOOK 1B | th15_everlasting_bgm | THMainHook |
| `0x461070` | EHOOK 7B | th15_param_reset | THMainHook |
| `0x4677be` | EHOOK 5B | th15_prac_menu_1 | THMainHook |
| `0x4677e1` | EHOOK 3B | th15_prac_menu_2 | THMainHook |
| `0x467b2a` | EHOOK 7B | th15_prac_menu_3 | THMainHook |
| `0x467bcb` | EHOOK 7B | th15_prac_menu_4 | THMainHook |
| `0x4678a2` | 补丁 `eb` | th15_prac_menu_enter_1 | THMainHook |
| `0x467b8a` | EHOOK 1B | th15_prac_menu_enter_2 | THMainHook |
| `0x467d31` | 补丁 `eb3b` | th15_disable_prac_menu_1 | THMainHook |
| `0x4527be` | EHOOK 5B | th15_menu_rank_fix | THMainHook |
| `0x43c68c` | EHOOK 1B | th15_patch_main | THMainHook |
| `0x43d5c1` | EHOOK 2B | th15_bgm | THMainHook |
| `0x45cc49` | EHOOK 5B | th15_rep_save | THMainHook |
| `0x468197` | EHOOK 3B | th15_rep_menu_1 | THMainHook |
| `0x4682b0` | EHOOK 5B | th15_rep_menu_2 | THMainHook |
| `0x468474` | EHOOK 2B | th15_rep_menu_3 | THMainHook |
| `0x4015fa` | EHOOK 1B | th15_update | THMainHook |
| `0x454a70` | EHOOK 1B | th15_player_state | THMainHook |
| `0x40170a` | EHOOK 1B | th15_render | THMainHook |
| `0x43E6EE` | EHOOK 7B | th15_game_start | THInGameInfo |
| `0x41497A` | EHOOK 5B | th15_bomb_dec | THInGameInfo |
| `0x456398` | EHOOK 5B | th15_life_dec | THInGameInfo |
| `0x43404A` | EHOOK 10B | th15_lock_timer1 | THInGameInfo |
| `0x42C738` | EHOOK 4B | th15_lock_timer2 | THInGameInfo |
| `0x42C7D6` | EHOOK 5B | th15_pattern_switch | THInGameInfo |
| `0x42C785` | EHOOK 5B | th15_pattern_switch_bossdead | THInGameInfo |
| `0x42B938` | EHOOK 2B | th15_lock_timer3 | THInGameInfo |
| `0x4301E8` | EHOOK 6B | th15_lock_timer4 | THInGameInfo |
| `0x460900` | 补丁 `ffffff7f` | th15_disable_demo | THInitHook |
| `0x4713ec` | 补丁 `90e9` | th15_disable_mutex | THInitHook |
| `0x46055f` | 补丁 `9090` | th15_startup_1 | THInitHook |
| `0x4610c3` | 补丁 `eb` | th15_startup_2 | THInitHook |
| `0x4617ff` | EHOOK 3B | th15_gui_init_1 | THInitHook |
| `0x47341b` | EHOOK 1B | th15_gui_init_2 | THInitHook |
| `0x401D22` | D-Pad 输入钩子（`SetDpadHook`） | — | GUI 初始化 |
| exe `0x47356c` / `0x47333d`、vpatch `+0x6bd9` | `InitHook(15, ...)` 通用钩子 | — | TH15Init |

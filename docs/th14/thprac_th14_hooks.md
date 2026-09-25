# thprac_th14.cpp 钩子（Hook）分析

> 分析对象：`thprac/src/thprac/thprac_th14.cpp`（约 3330 行）
>
> 适用游戏：**东方辉针城（TH14, Double Dealing Character）**
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
| `HOTKEY_DEFINE` / `HOTKEY_ENDDEF` | 定义一个热键，内部通过 `PATCH_HK(addr, code)` 在热键开启时安装补丁、关闭时还原；`EHOOK_HK` 同理挂执行流钩子。 |
| `self->Enable()` / `self->Disable()` | 运行时启用 / 禁用某个钩子（一次性钩子常 `self->Disable()` 后只执行一次）。 |
| `asm_call<addr, CallType>` | 直接以指定调用约定调用游戏内部地址的函数（不是钩子，是调用原始游戏功能）。 |

代码中的地址均为游戏的静态基地址（thprac 通过 `ingame_image_base` 做重定位）。TH14 被挂载的模块为主 exe（无外部 vpatch，见文末 `TH14Init`）。

---

## 二、热键作弊 / 便利功能（`THOverlay`）

`THOverlay` 是游戏内“Mod Menu”热键菜单，每个热键通过 `PATCH_HK` 挂一个小字节补丁：

| 热键 | 名称常量 | 功能 | 补丁 |
| --- | --- | --- | --- |
| `BACKSPACE` | ModMenuToggle | 打开 / 关闭 Mod Menu 本体（非补丁，仅 GUI 开关） | — |
| `F1` | `TH_MUTEKI` | **无敌（Muteki）** | `0x44F877` 处写 `01` |
| `F2` | `TH_INFLIVES2` | **无限残机**（无补丁，由主 hook `th14_inf_lives` 实现，见下文） | — |
| `F3` | `TH_INFBOMBS` | **无限炸弹** | `0x412173` 处写 `90`（NOP 掉扣雷指令） |
| `F4` | `TH_INFPOWER` | **无限火力（Power 恒满）** | `0x44DDA5` 处写 `58` |
| `F5` | `TH_TIMELOCK` | **时间锁定（雷锁，Bomb 槽时间不回退）** | `0x41C5DD` 处写 `eb`；`0x424ACA` 处写 `90`；另挂 `EHOOK_HK` `0x483c4f`（4 字节）冻结 ST5 MainLatter 段的 ECL 子时间 |
| `F6` | `TH_AUTOBOMB` | **自动雷（被弹瞬间自动放 B）** | `EHOOK_HK` `0x44DEC4`（7 字节）：有 Bomb 槽时跳 `0x44DFD1`，否则跳 `0x44DED1` |
| `F7` | `TH_EL_BGM` | **永恒 BGM（BGM 不切换/不停止循环）** | 无补丁，由主 hook `th14_everlasting_bgm` 实现 |
| `F8` | `THPRAC_INGAMEINFO` | **游戏内信息显示（InGameInfo 面板）** | 无补丁，控制 `TH14InGameInfo` 窗口显示 |

---

## 三、练习模式系统（`THGuiPrac` + 主流程 hook）

### 3.1 练习参数结构 `THPracParam`

一个 JSON 可序列化的参数结构，记录练习模式的选择：`mode/stage/section/phase`（模式/关卡/段落/阶段）以及开局资源（分数、残机、残机碎片、炸弹、炸弹碎片、cycle、火力、数值、擦弹）与 ST4 **伪机体**（`faketype`，灵梦伪装魔理沙 / 魔理沙伪装灵梦，`thPracParam.stage==3`（0 基）= ST4）。它会写进录象文件用于回放恢复，也会在开局时写入游戏内存。

### 3.2 练习菜单挂钩（进入练习模式选关界面）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th14_prac_menu_1` | `0x45e768`（7 字节） | 练习菜单状态 1：保存当前难度/机体、打开练习 GUI、重置 `thPracParam` |
| `th14_prac_menu_2` | `0x45e787`（3 字节） | 状态 2：预留分支 |
| `th14_prac_menu_3` | `0x45ea9f`（7 字节） | 状态 3（确认）：把 GUI 选项收集进 `thPracParam`（`CalcSection()` 计算段落、`SpellPhase()` 判断阶段） |
| `th14_prac_menu_4` | `0x45eb40`（7 字节） | 状态 4（退出）：关闭 GUI |
| `th14_prac_menu_enter_1` | `0x45e847` | 字节补丁 `eb`：跳过菜单某一分支 |
| `th14_prac_menu_enter_2` | `0x45eafc`（1 字节） | 把 `thPracParam.stage` 写进 `Ecx`，让游戏按所选关卡进入 |
| `th14_disable_prac_menu_1` | `0x45eca7` | 字节补丁 `eb3b`：禁用一个菜单分支 |
| `th14_menu_rank_fix` | `0x449c3d`（5 字节） | 修复菜单难度显示：强制 `0x4f5844=-1`、`0x4f5834=0x4d5984`（恢复真实难度值） |
| `th14_fake_type` | `0x0042ABAF`（3 字节） | ST4 伪机体：练习模式下按 `thPracParam.faketype` 改写 `Eax`（机体号） |

### 3.3 开局写入参数（核心）`th14_patch_main`

地址 `0x4360ce`（1 字节 EHOOK），练习开局时执行：

- 若处于练习模式（`thPracParam.mode==1`），把 `thPracParam` 中的分数、残机、残机碎片、炸弹、炸弹碎片、cycle、火力、数值、擦弹依次写入对应内存地址（`0x4f5830 / 0x4f5864 / 0x4f5868 / 0x4f5870 / 0x4f5874 / 0x4f5894 / 0x4f5858 / 0x4f584c / 0x4f5840`），再调用 `THSectionPatch()` 对当前段落的 ECL 脚本做二进制补丁；
- 若为符卡练习（`mode==2`），通过 `ECLHelper`（基址 `0x4db544 → +0xcc → +0xC`）调用 `THPatchSP(ecl)` 注入符卡练习补丁；
- 置 `thPracParam._playLock = true`。

### 3.4 章节 / ECL 脚本补丁

练习的核心不是钩子而是**直接改写游戏加载的 ECL（弹幕脚本）二进制**。`THSectionPatch()` / `THPatchSP()` 通过 `ECLHelper`（VFile）把补丁写入 `0x4db544 → +0xcc → +0xC` 处的 ECL 数据区：

- `THStageWarp`：按“关 + 段落”把 ECL 指针跳转到对应段落的入口（跳过前面的段落），大量 `ECLJump` 调用把章节连接起来；
- `THPatch`：按关卡段（`TH14_ST*` 枚举）做细粒度改写——跳过对话、设置道中 Boss 血量、移除移动限制、修改待机/无敌时间、注入自定义 ANM/精灵指令等。最典型的是把“非符/符卡练习”直接跳转到 Boss 召唤指令处。

TH14 的练习段落结构（开发组自有，见 `thprac_locale_def.cpp:21816/21961`）：`th_sections_cba[9][2][19]`（道中/关底 boss 段落，`[9]` 维：行 0~5 = ST1~ST6 基础行、行 6 = ST7（EX）、行 7/8 = **ST4 伪机体变体（ST4A/ST4B）**）、`th_sections_cbt[9][2][13]`（非符/符卡段落）、`mChapterSetup[7][2]`（各关道中/关底章节数）。注意 TH14 ST3 道中第 2 boss 按难度分 **EN/HL 两个段落条目**（`TH14_ST3_MID2_EN` / `TH14_ST3_MID2_HL`）。

### 3.5 练习 BGM

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th14_bgm` | `0x43699a`（2 字节） | 根据 `THBGMTest()` 查表得到练习段落对应的 BGM id，若命中则 `PushHelper32(pCtx, 1)` 并跳到 `0x43699c` 覆盖原 BGM 调用，使每个练习段落播放正确 BGM |
| `th14_param_reset` | `0x45a111`（7 字节） | 游戏初始化路径上重置 `thPracParam` |

---

## 四、录象系统（`THGuiRep`）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th14_rep_save` | `0x455bf2`（5 字节） | 保存录象时把 `thPracParam.GetJson()`（练习参数）附加写入录象文件（`%APPDATA%\ShanghaiAlice\th14\replay\`），便于回放时恢复练习环境 |
| `th14_rep_menu_1` | `0x45f10b`（3 字节） | 录象菜单状态 1：重置录象 GUI 状态与 `thPracParam` |
| `th14_rep_menu_2` | `0x45f216`（5 字节） | 状态 2：读取当前选中的录象文件并解析其中的参数（`CheckReplay`） |
| `th14_rep_menu_3` | `0x45f3ed`（2 字节） | 状态 3：若参数有效则 `memcpy` 到 `thPracParam`，实现“看录象 = 复现当时练习配置” |

---

## 五、符卡练习（`THGuiSP`）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th14_sp_menu_1` | `0x464068`（3 字节） | 符卡练习菜单状态 1：读取当前选中符卡 id（`0x4db6a4 → +0x5d70`），打开 GUI；不可练（无 phase）则跳过 |
| `th14_sp_menu_2` | `0x463fd8`（6 字节） | 符卡练习菜单状态 2：确认后填 `thPracParam`（`mode=2`、`phase`），清零帧计数器后跳转练习入口 |

---

## 六、游戏内信息与统计（`TH14InGameInfo` + `THInGameInfo` hookset）

`TH14InGameInfo` 窗口显示在右上角，数据来自两处：`THInGameInfo` hookset 统计的 Miss / Bomb 次数与道具阈值计数，以及直接读取游戏内存（难度、玩家机体、bonus/cycle 等）。窗口位置 `(890/1280, 560/960)`，宽度 `360/1280`、**高度自动**（`0.0f`）。

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th14_game_start` | `0x4375BE`（7 字节） | 开局（gamestart-bomb set）时把 `mBombCount` / `mMissCount` 及道具阈值计数全部清零 |
| `th14_bomb_dec` | `0x41218A`（5 字节） | 炸弹数减少时 `mBombCount++`（统计用雷数） |
| `th14_life_dec` | `0x44F618`（5 字节） | 残机数减少时 `mMissCount++`，并调用 `FastRetry(thPracParam.mode)` —— 练习模式下掉命立即重开 |
| `th14_get_item` | `0x438DF8`（6 字节） | 拾取道具时按 `0x4F5880`（道具计数）跨过 20/30/40/50/60 阈值递增 `m05/08/12/16/20Count` |
| `th14_lock_timer1` | `0x42EC6B`（10 字节） | 雷锁计时初始化：`g_lock_timer=0` |
| `th14_lock_timer2` | `0x4286BA`（4 字节） | `SetNextPattern` case 514（Boss 切换弹幕）时清零雷锁计时 |
| `th14_lock_timer3` | `0x427888`（2 字节） | `set boss mode` case 512（Boss 进入状态）时清零雷锁计时 |
| `th14_lock_timer4` | `0x42C0D4`（6 字节） | 时间递减（更新）路径上置 `g_lock_timer_flag=true`，由每帧 `RenderLockTimer` 累加并绘制秒数（配合 `TH_TIMELOCK` 观察雷锁时长） |

> **miss/bomb 记录**：复刻 TH15 已定案方案（落盘 `th14_missbomb.txt`、IGI 显示最近 5 条，仅 ST1~ST6、仅练习模式）。第一轮反编译已确认全部偏移与站点（`docs/th14/th14_re_disasm_requests.md` §1）：case 512/514 钩子缓存 `g_boss_ptr = pCtx->Edi`；boss 战门控 `[g_boss_ptr+0x4054] & 0x800000`；阶段名 = 槽写前旧值（`scr = [g_boss_ptr+0x40ec]`，`*(char**)(scr + slot*0x10 + 0x5264)`），由 3 个站点缓存——`0x428782`（normal）/ `0x42c5e4`（BossDead）/ `0x42c6c4`（BossEscape）。记录逻辑抽在独立 helper `thprac_th14_missbomb.h`（header-only）。`TH14InGameInfo::OnPreUpdate` 已轮询 `PLAYER_PTR`（`0x4db67c`）作“一局结束”检测。

---

## 七、主循环与渲染挂钩（`THMainHook` 剩余部分）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th14_update` | `0x40138a`（1 字节） | **每帧主更新**：`GameGuiBegin` 开始 ImGui 帧 → 依次 `Update()` 练习菜单 / 录象菜单 / 热键菜单 / 符卡练习 / InGameInfo → 绘制“Boss 强制下移”提示、子弹判定框（`RenderBtHitbox`）、掉雷条（`RenderHitBar`）、键盘监控（`KeysHUD`）、雷锁计时（`RenderLockTimer`）→ `GameUpdateOuter` 收尾 → `GameGuiEnd`。相当于把整个 mod 的 UI 与逻辑并入游戏主循环 |
| `th14_render` | `0x40149a`（1 字节） | 渲染 ImGui 帧（`GameGuiRender(IMPL_WIN32_DX9)`），把 UI 画到屏幕 |
| `th14_player_state` | `0x44DBD0`（1 字节） | 玩家状态更新时调用 `RecordKey(14, 按键内存)`，供键盘监控（KeysHUD）录制按键序列 |
| `th14_everlasting_bgm` | `0x46ef90`（1 字节） | **永恒 BGM**：挂 BGM 控制函数入口，用 `ElBgmTest` 判断当前 BGM 是否被锁；练习模式下阻止 BGM 停止/切换（`F7`），命中时跳到 `0x46f022` 跳过原始逻辑 |
| `th14_inf_lives` | `0x0044F617`（1 字节） | 无限残机（`F2`）：开启时 `pCtx->Eax++` 抵消扣命；若开启“映射到无续关”选项则仅在 `0x4F5864==0`（无续关）时生效 |
| `th14_on_restart` | `0x447810`（5 字节） | 重开（restart）时锁定 `thPracParam` 与高级选项（`ToggleLock(true)`） |
| `th14_normal_game_enter` | `0x45e63e`（8 字节） | 进入正常游戏时锁定 `thPracParam` 与高级选项 |

---

## 八、高级选项（`THAdvOptWnd`）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th14_marisa_laser` | `0x45286f`（6 字节） | **魔理沙激光回放修复**：读/写栈上 `Esp+0x2c` 处的激光浮点值；`th14_fixMarisaBug` 选项开启时先置 0；再经 `THMarisaLaser::Access()` 按回放/正常/恢复三种状态修正激光值（修魔理沙 Sub 激光在回放中的随机性） |
| `th14_all_clear_bonus_1` | `0x43708a`（补丁 `EB0CCCCCCC`） | 强制“全关奖励”分支成立（练习模式也有全关奖励） |
| `th14_all_clear_bonus_2` | `0x43712c`（7 字节） | 把当前分数写入 `0x4db550 → +0x140`；若处于实践模式标记（`0x4f58b8 & 0x10`）则调用 `0x449f20` 分数函数并跳回 `0x43708f`，保证全关奖励正常累计 |
| `th14_all_clear_bonus_3` | `0x43719f`（7 字节） | 与 `th14_all_clear_bonus_2` 同逻辑，另一结算入口 |
| `th14_master_disable`（Hookset，3 个补丁） | `0x41CAD9`=`eb`、`0x41CAB4`=`eb`、`0x41CA0F`=`00` | 禁用 Boss 的“雷（master）自动释放”（`disable_master_autoly` 选项） |
| `th14_bossmovedown` | `0x0042725B`（5 字节） | **Boss 强制下移**：回调内读取 `Edi+0x3F64/0x3F6C`（Boss 的 Y 坐标与活动范围），按 `g_bossMoveDownRange` 重新计算并写入，把 Boss 拉到底部；高级选项里可拖滑条调范围 |

`THAdvOptWnd` 还提供若干非钩子选项开关：`th14_showBonus`（IGI bonus/cycle 显示）、`th14_showItemsCount`（IGI 道具计数）、`th14_showDropBar`（掉雷条）、`th14_fixMarisaBug` / `th14_laserRepRepair`（魔理沙激光修复）、`disable_master_autoly` / `enable_lock_timer_autoly`（禁用 master 自动雷 / 雷锁计时）、`map_inf_life_to_no_continue`（无限残机映射到无续关）、`show_keyboard_monitor` / `keyboard_style`（键盘监控）。

---

## 九、初始化挂钩（`THInitHook` + `TH14Init`）

| 钩子 | 地址 | 说明 |
| --- | --- | --- |
| `th14_disable_demo` | `0x45996c`（补丁 `ffffff7f`） | 禁用演示（DEMO）模式，避免演示画面触发崩溃 |
| `th14_disable_mutex` | `0x469387`（补丁 `90e9`） | 绕过进程互斥量，支持游戏多开 |
| `th14_startup_1` | `0x4595a5`（补丁 `eb`） | 启动流程补丁 1（跳过某次校验/分支） |
| `th14_startup_2` | `0x45a173`（补丁 `eb`） | 启动流程补丁 2 |
| `th14_gui_init_1` | `0x45a591`（3 字节） | 自禁用后调用 `THGuiCreate()`：初始化 ImGui + DX9 界面、创建全部 GUI 窗口（`THGuiPrac/THGuiRep/THOverlay/THGuiSP/TH14InGameInfo`）、`EnableAllHooks(THMainHook/THInGameInfo)`、重置 `thPracParam` |
| `th14_gui_init_2` | `0x46b37b`（1 字节） | 另一条初始化路径上的同样处理 |

`TH14Init()`（文件外调用入口）：

```cpp
EnableAllHooks(TH14::THInitHook);               // 安装上述初始化钩子
InitHook(14, (void*)0x46b4b3, (void*)0x46b2de); // 通用初始化钩子（exe 与刷新率相关地址）
```

另：`THGuiCreate()` 中还会调用 `SetDpadHook(0x401A8E, 3)`，对游戏的方向键（十字键）输入函数挂钩，用于实现菜单/界面内的按键导航。

---

## 附录：钩子地址总表

| 地址 | 类型 | 名称 | 归属 |
| --- | --- | --- | --- |
| `0x44F877` | 补丁 `01` | mMuteki（无敌） | THOverlay 热键 |
| `0x412173` | 补丁 `90` | mInfBombs（无限雷） | THOverlay 热键 |
| `0x44DDA5` | 补丁 `58` | mInfPower（无限火力） | THOverlay 热键 |
| `0x44DEC4` | EHOOK 7B | mAutoBomb（自动雷） | THOverlay 热键 |
| `0x41C5DD` | 补丁 `eb` | mTimeLock（雷锁） | THOverlay 热键 |
| `0x424ACA` | 补丁 `90` | mTimeLock（雷锁） | THOverlay 热键 |
| `0x483c4f` | EHOOK 4B | mTimeLock（雷锁，ST5 MainLatter） | THOverlay 热键 |
| `0x45286f` | EHOOK 6B | th14_marisa_laser | 高级选项 |
| `0x43708a` | 补丁 `EB0CCCCCCC` | th14_all_clear_bonus_1 | 高级选项 |
| `0x43712c` | EHOOK 7B | th14_all_clear_bonus_2 | 高级选项 |
| `0x43719f` | EHOOK 7B | th14_all_clear_bonus_3 | 高级选项 |
| `0x41CAD9` | 补丁 `eb` | th14_master_disable1a | 高级选项 |
| `0x41CAB4` | 补丁 `eb` | th14_master_disable1b | 高级选项 |
| `0x41CA0F` | 补丁 `00` | th14_master_disable1c | 高级选项 |
| `0x0042725B` | EHOOK 5B | th14_bossmovedown | 高级选项 |
| `0x0044F617` | EHOOK 1B | th14_inf_lives | THMainHook |
| `0x46ef90` | EHOOK 1B | th14_everlasting_bgm | THMainHook |
| `0x45a111` | EHOOK 7B | th14_param_reset | THMainHook |
| `0x45e768` | EHOOK 7B | th14_prac_menu_1 | THMainHook |
| `0x45e787` | EHOOK 3B | th14_prac_menu_2 | THMainHook |
| `0x45ea9f` | EHOOK 7B | th14_prac_menu_3 | THMainHook |
| `0x45eb40` | EHOOK 7B | th14_prac_menu_4 | THMainHook |
| `0x45e847` | 补丁 `eb` | th14_prac_menu_enter_1 | THMainHook |
| `0x45eafc` | EHOOK 1B | th14_prac_menu_enter_2 | THMainHook |
| `0x45eca7` | 补丁 `eb3b` | th14_disable_prac_menu_1 | THMainHook |
| `0x449c3d` | EHOOK 5B | th14_menu_rank_fix | THMainHook |
| `0x0042ABAF` | EHOOK 3B | th14_fake_type | THMainHook |
| `0x4360ce` | EHOOK 1B | th14_patch_main | THMainHook |
| `0x43699a` | EHOOK 2B | th14_bgm | THMainHook |
| `0x447810` | EHOOK 5B | th14_on_restart | THMainHook |
| `0x45e63e` | EHOOK 8B | th14_normal_game_enter | THMainHook |
| `0x455bf2` | EHOOK 5B | th14_rep_save | THMainHook |
| `0x45f10b` | EHOOK 3B | th14_rep_menu_1 | THMainHook |
| `0x45f216` | EHOOK 5B | th14_rep_menu_2 | THMainHook |
| `0x45f3ed` | EHOOK 2B | th14_rep_menu_3 | THMainHook |
| `0x464068` | EHOOK 3B | th14_sp_menu_1 | THMainHook |
| `0x463fd8` | EHOOK 6B | th14_sp_menu_2 | THMainHook |
| `0x40138a` | EHOOK 1B | th14_update | THMainHook |
| `0x44DBD0` | EHOOK 1B | th14_player_state | THMainHook |
| `0x40149a` | EHOOK 1B | th14_render | THMainHook |
| `0x4375BE` | EHOOK 7B | th14_game_start | THInGameInfo |
| `0x41218A` | EHOOK 5B | th14_bomb_dec | THInGameInfo |
| `0x44F618` | EHOOK 5B | th14_life_dec | THInGameInfo |
| `0x438DF8` | EHOOK 6B | th14_get_item | THInGameInfo |
| `0x42EC6B` | EHOOK 10B | th14_lock_timer1 | THInGameInfo |
| `0x4286BA` | EHOOK 4B | th14_lock_timer2 | THInGameInfo |
| `0x427888` | EHOOK 2B | th14_lock_timer3 | THInGameInfo |
| `0x42C0D4` | EHOOK 6B | th14_lock_timer4 | THInGameInfo |
| `0x45996c` | 补丁 `ffffff7f` | th14_disable_demo | THInitHook |
| `0x469387` | 补丁 `90e9` | th14_disable_mutex | THInitHook |
| `0x4595a5` | 补丁 `eb` | th14_startup_1 | THInitHook |
| `0x45a173` | 补丁 `eb` | th14_startup_2 | THInitHook |
| `0x45a591` | EHOOK 3B | th14_gui_init_1 | THInitHook |
| `0x46b37b` | EHOOK 1B | th14_gui_init_2 | THInitHook |
| `0x401A8E` | D-Pad 输入钩子（`SetDpadHook`） | — | GUI 初始化 |
| exe `0x46b4b3` / `0x46b2de` | `InitHook(14, ...)` 通用钩子 | — | TH14Init |

# 全作游戏时长统计（按局分条落盘）— 项目开发与逆向工程方案

> 需求来源：`docs/req.md`
> 状态：**P0 已实现**（极简版，改动收敛到 2 个文件；见 §1.7 与文末变更记录）
> 相关文档：`docs/gametime/gametime_thxx_flags.md`（逐作标志位总表）、`docs/th07/th07_re_disasm_requests.md`、`docs/th08/th08_re_disasm_requests.md`

> **2026-09-29 修订**：初版方案把这件事铺到了 8 个改动点。复核后确认「读取 + 校验」本身只有十几行，
> 其余 7 处都在解决「拿不到 `ReadProcessMemory` 的入参」——现有 16ms 循环（`thprac_launcher_others.cpp:369`）
> 的全部输入只是 `CheckIfAnyGame2()` 的一个 `bool`，既不知道是哪一作、也没有 PID / 句柄 / base。
> 按「统一走枚举认游戏 + 就地扩展 `THGameTimeRecorder`」重做后，**改动面收敛到 2 个文件、零新增文件**。
> 被砍掉的 6 处与理由见 §1.7 的对照表。

> 路径约定：本文所有源码路径均相对仓库根（`thprac.sln` 所在目录），例如 `thprac/src/thprac/thprac_launcher_others.cpp`。

---

## 0. 需求与决策

**需求**：把 `thprac_th06.cpp` 里"游戏进行中 且 未暂停 且 非 replay 时累计游戏时长"的机制推广到全作；**计时做在 thprac 进程里**（不是游戏进程），尽量减少对游戏进程的依赖；并且**每一次游戏单独成一条记录**持久化，供后续整体统计。

- "一次游戏" = 从游戏进程启动到游戏进程关闭。
- 现有 TH06 机制每 3 分钟落盘一次；新功能改为**一局一条记录**。

**已定决策**：

| 决策点 | 结论 |
| --- | --- |
| 计时与门控位置 | 纯 launcher 侧 `ReadProcessMemory`，游戏侧零改动 |
| `stash@{0}` 的 WIP | 不采用其代码（走共享内存路线），从零写；仅复用其 th15 反编译结论 |
| 记录字段 | 仅四个：游戏 id / 时长 / 起始墙钟 / 结束墙钟（`gate`、`closed` 留到 P2） |
| 功能开关 | 共用 Others 页现有的 `recordGameTime` 勾选，不新增设置键 |
| 逆向预期 | 各代引擎机制类似，三个标志位必然可找到；RE 按"分代 + 同构搬运"编排 |
| TH06 旧机制 | 保留不动，新旧并行 |
| launcher 不在场 | 接受限制（只有从启动器界面启动且启动器窗口开着才记） |
| 落盘 | 新建追加式记录文件 |
| **认游戏方式** | **统一走枚举**（Toolhelp + exe 名快筛 + PE 指纹），不用启动瞬间拿句柄的快路；顺带免费覆盖 steam / thcrap / 手动启动 |
| **代码归属** | **就地扩展 `THGameTimeRecorder`**，不新建兄弟类；新旧累加器同线程、各自独立 |
| **落盘实现** | **内联 `_snprintf_s` + Win32 文件 API**，不移植 `json_util` |

**现状**：

- TH06 的实现是 `TH06Save::IncreaseGameTime()`（`thprac/src/thprac/thprac_th06.cpp:159-188`），每帧由 `EHOOK_DY(th06_update, 0x41caac, 1)` 调用；三个门控是硬编码绝对地址（见 §2.1）。落盘 `%appdata%\ShanghaiAlice\th06\score06.dat`，3 分钟节流 + 每次符卡事件 + 退出 hook 各写一次。**除 TH06 外没有任何作品做过游戏时长统计。**
- 启动器侧另有 `THGameTimeRecorder`（`thprac/src/thprac/thprac_launcher_others.cpp:350-492`）：独立线程、`Sleep(16)`、每秒用 `CheckIfAnyGame2()` 探测"是否有游戏在跑"，只累加**进程存活时长**，不分条，只落一个 `"Gametime"` 设置项。
- `stash@{0}`（在 `lxn/main` 上）有一版 WIP：游戏侧注入模块每帧算 `in_gameplay` 写命名共享内存 `thprac_gametime_<id>`，launcher 读。**本轮不采用该架构**，但其中 th15 的门控结论已被验证过，直接复用（见 §2.1）。

---

# 一、项目开发方案

## 1.1 关键架构事实

### 进程拓扑

1. **`thprac.exe` 是单文件双身份**：既是启动器（`wWinMain` → `GuiLauncherMain`），也被 `LoadLibraryW` 注入游戏进程当模块。注入后靠 `&__ImageBase == base` 自我识别，`RemoteInit()`（`thprac/src/thprac/thprac_init.cpp:19`）在启动器里是 no-op。**启动器进程与游戏进程共存，前者持有游戏进程句柄完全可行。**

2. **从启动器 GUI 启动游戏时，启动器窗口留活** —— `LaunchGameDirectly`（`thprac/src/thprac/thprac_launcher_games.cpp:1950`，`CreateProcessW(..., CREATE_SUSPENDED, ...)` 在 `:1964`，GUI 按钮调用在 `:2048`）。`THGameTimeRecorder` 的线程随 `LauncherOthersInit/Destroy`（`thprac/src/thprac/thprac_launcher_main.cpp:144` / `:259`）存活。**这是"计时做在 thprac 进程"能成立的前提。**

3. **游戏进程侧没有"退出"回调**：全仓库无 `DllMain`/`DLL_PROCESS_DETACH`，`RemoteInit` 结尾用 `ExitThread` 故意跳过 CRT 静态析构（`thprac_init.cpp:43-52`），游戏侧也没有 `CreateThread`。**→ 会话边界检测必须在 launcher 侧做。**

4. **反例（已知缺口）**：`main.cpp:224-226` 的自动查找并运行路径（`FindAndRunGame`）跑完会 `return 0` 让启动器退出；`:155-157` 的命令行路径同理。这些路径下启动器不在场，无法计时。补缺方案见 §1.8。

### 三处已验证的修正（重要，避免重踩）

5. ⚠️ **不能用 `CheckOngoingGameByPID` 认"已经在跑的 thprac 游戏"。**
   `thprac/src/thprac/thprac_load_exe.cpp:112-124` 有一段 THPrac 签名门：

   ```cpp
   ReadProcessMemory(hProc, (void*)(*base + sigAddr - 4), &sigCheck, 4, &n);
   if (n != 4 || sigCheck) { CloseHandle(hProc); return nullptr; }   // 要求 sigCheck == 0
   ```

   而 `WriteTHPracSig` 恰恰会在该处写入 `'CARP'`（`:162`）。也就是说，该函数**只用于找"尚未被注入的游戏"**（`FindOngoingGame`）。启动器自己启动的游戏在 `ResumeThread` 之前就已被打了签名，因此对它调此函数**恒返回 `nullptr`**。

   → **不需要新增识别函数**：`GetExeInfoEx`（`thprac_launcher_games.cpp:154`，已导出在 `thprac_launcher_games.h:18`）**不做** CARP 校验，注入后的进程内部 `RemoteInit` 本来就在用它（`thprac_init.cpp:27`）。配 `OpenProcess` + `GetGameModuleBase` 十几行即可，`thprac_load_exe.{h,cpp}` **一行不用改**。
   （初版方案曾打算把 `CheckOngoingGameByPID` 拆出 `IdentifyRunningGameByPID`，属于伪必要。）

6. ⚠️ **`thXX_disable_mutex` 补丁不会删掉游戏的命名互斥体。**
   已在 `th07.exe` / `th08.exe` 上反汇编验证：hook 只强制走"没有别的实例"分支，`CreateMutexA` 照常执行。

   ```
   th07.exe  00435be0  push 0x49732c        ; "Touhou YouYouMu App"
             00435be9  call [0x48d084]      ; CreateMutexA     <-- 仍然调用
             00435bf4  call [0x48d07c]      ; GetLastError
             00435bfa  cmp eax, 0xb7        ; ERROR_ALREADY_EXISTS
             00435bff  jne 0x435c1b         <-- th07_disable_mutex 钩在此处
   th08.exe  00443430  push 0x4b64e0        ; "Touhou 08 App"
             00443439  call [0x4b4098]      ; CreateMutexA
             0044344a  cmp eax, 0xb7
             0044344f  jne 0x44346b         <-- th08_disable_mutex 钩在此处
   ```

   → 所以 `CheckIfAnyGame2()` 对注入后的游戏依然有效；**`stash` 里为 th15 加的 `thprac_gametime_live_<id>` 存活事件探测在本方案下不需要**（顺带订正 stash 中"打掉游戏自带互斥量"的错误注释）。

7. ℹ️ **launcher 在启动游戏时就已经知道 PID 与进程句柄，但本方案不用它。**
   `LaunchGameDirectly`（`:1950`）在 `:1964` 拿到 `PROCESS_INFORMATION`，`:2008-2013` 返回 `proc_info.hProcess`；`LaunchThreadFunc`（`:2048-2052`）拿到后**立刻 `CloseHandle` 丢掉**。
   利用它（加 `pOutPid` 出参 + 不关句柄）能省掉一次枚举，但那只覆盖"从启动器启动"这一条路，steam / thcrap / 手动启动仍需枚举兜底。
   既然两条路都要枚举能力，就只保留枚举一条路——**每局一次**（不是每 tick），稳态零成本，代码路径也少一条。

### 其余可用原语

8. `GetGameModuleBase(hProc)`（`thprac/src/thprac/thprac_load_exe.h:38`）；`GetExeInfoEx(hProc, base, sig)`（`thprac/src/thprac/thprac_launcher_games.cpp:154`）从活进程读 PE 指纹；`ReadProcessMemory` 在 launcher 侧先例众多（`thprac_load_exe.cpp:90-170`、`thprac_launcher_games.cpp:99/160-181`）。
9. `CheckIfAnyGame2()`（`thprac_launcher_others.cpp:22-60`）遍历 `gGameDefs` 试 `OpenMutexA/W` —— **廉价的"有没有游戏在跑"探针，保留**。
10. 进程枚举先例：`CheckProcessOmni`（`thprac_launcher_games.cpp:1747-1756`，先用 exe 名做纯字符串过滤）、`FindOngoingGame`（`thprac_main.cpp:275-308`）—— `CheckIfAnyGame()` → `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` → `Process32FirstW/NextW` → 逐项校验。

### 互斥体歧义与消歧

11. **互斥体名有歧义**：th09 / th095 / th08 共用 `"Touhou 08 App"`；th125 / th128 / th13 / th14 / th15 / th165 / th143 共用 `"Touhou 12 App"`（`thprac_launcher_games_def.h:416/430/446/538/554/570/600/616/646/692`）。

    但 **PE 指纹各不相同**：
    | 作 | `timeStamp` | `textSize` |
    | --- | --- | --- |
    | th08 | 1095610188 | 728064 |
    | th09 | 1128708539 | 576512 |
    | th095 | 1137085759 | 603136 |

    → **`CheckIfAnyGame2()` 只能当"有没有游戏"，不能当"是哪一作"**。定游戏必须靠 PE 指纹（`gGameDefs` 里的 `ExeSig`）。

12. **基址**：th07.exe / th08.exe 的 `ImageBase = 0x400000`，且 `FileHeader.Characteristics` 含 `RELOCS_STRIPPED`（不重定位）。`ingame_image_base` 恒为 0，只有 th19/th20 会被赋值（`thprac_th19.*.cpp:801/876`、`thprac_th20.cpp:4595`；`thprac_hook.cpp:82` 初值 0）。→ **地址统一按 `base + RVA` 计算，两代通吃**。

13. **时钟与格式化**：`SetUpClock()` / `ResetClock(int)`（`thprac/src/thprac/thprac_utils.h:35-36`，基于 `QueryPerformanceCounter`；注意 **`ResetClock` 是"读并重置基准"语义**，返回自上次调用起的秒数）、`GetTime_HHMMSS` / `GetTime_YYMMDD_HHMMSS`（`thprac_utils.h:132-133`）。

14. **数据目录**：`LauncherGetDataDir()`（`thprac/src/thprac/thprac_launcher_cfg.cpp:79-84`）返回**绝对路径且以 `\` 结尾**：优先 `<exe目录>\.thprac_data\`，否则 `%APPDATA%\thprac\`。→ **不需要 `PushCurrentDirectory`**（比 `KengRecorder` 依赖 CWD 的写法更稳）；用前判空。

15. ⚠️ **文案约束**：`thprac/src/thprac/thprac_locale_def.h/.cpp` 是**自动生成**的（`th_glossary_str[3][1871]`，硬编码下标），新增词条会移动全部下标，**本地无法重新生成**（需外部 thprac devtools）。→ **一期一律复用既有词条**：`THPRAC_GAME_TIME_TOTAL` / `THPRAC_GAME_TIME_CURRENT` / `THPRAC_GAME_TIME_RESET` / `THPRAC_ENABLE_GAMETIME_RECORD`。

## 1.2 组件设计：就地扩展 `THGameTimeRecorder`，不新建兄弟类

**在 `THGameTimeRecorder::UpdateGameTime()`（`thprac_launcher_others.cpp:369`）的现有 16ms 循环里就地加**，新增成员各自独立：

```
thprac.exe（启动器进程）
┌──────────────────────────────────────────────────────────────────────┐
│ THGameTimeRecorder（同一个类、同一条 16ms 线程）                       │
│                                                                      │
│  旧累加器（一行未改）                 新累加器（本功能）                │
│  判据：CheckIfAnyGame2()             判据：RPM 读三个门控标志位          │
│  语义：进程开着即计时                 语义：进程存活 ⊗ 门控通过           │
│  出口：thpracP.json "Gametime"       出口：gametime_sessions.jsonl 追加 │
└──────────────────────────────────────────────────────────────────────┘
```

**为什么并入而不是新建**：

1. 初版把"语义不同 / 状态机不同 / 生命周期不同 / 并行对照 / 爆炸半径"当作新类的理由，但这些**都靠"新增独立成员"就满足了**——旧 `mGameTime_ns` / `mGameTimeCur_ns` 的累加与落盘时机一行不动，老用户的历史数字不会变味，新旧两套累加器仍可直接对照。
2. 新类会连带引出 3 处纯接线成本：新 `.h/.cpp` + `thprac.vcxproj(.filters)` 两个 Configuration + `thprac_launcher_main.cpp` 的 init/update/destroy，还因为跨 TU 访问 `CheckIfAnyGame2()` 而需要改它的签名。**全是样板，不是收益。**
3. 唯一打折的是"爆炸半径"——新代码出错会牵连整个计时线程。代价可接受：新代码是纯追加，不触碰旧的累加路径。

**`CheckIfAnyGame2()` 一行不改**：它仍是 `bool`、仍只有一个调用点（`:375`）。"是哪一作"由枚举 + PE 指纹回答，不需要它回答。

## 1.3 拿到"是哪一作 + PID + base + 句柄"：统一走枚举

**唯一路径：`CheckIfAnyGame2()` 报"有游戏" 且 当前未绑定 → 枚举一次。**

```
CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)
  → 逐项 ExeNameLooksLikeTouhou() 快筛（照搬 CheckProcessOmni 的字符串逻辑）
  → OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE)
  → GetGameModuleBase() → GetExeInfoEx() → 按 textSize + timeStamp 匹配 gGameDefs
  → 只接受 gametimeGate != nullptr 的条目（"该作是否参与计时"与"是否记录"合成一件事）
```

- **`SYNCHRONIZE`** 用于 `WaitForSingleObject` 精确判定退出；**`PROCESS_VM_READ`** 用于 `ReadProcessMemory`。
- **每局只枚举一次**（不是每 tick）。用一个 `mBindTried` 标志：`is_game_open` 上升沿置位、下降沿复位。
- **绑定是一次性的**：认不出来就整局不记。宁可不记录，也不要一条起点错误的记录（若允许中途重试，就会产出"起点是半局"的静默错数据）。副作用是非计时作品（如 th09）每局只烧一次扫描，不会持续枚举。
- **回退路径被顺带覆盖**：steam 启动 / thcrap / 启动器之外手动开的游戏走的是同一条路，"兜底路 B"不再需要作为独立分支存在。

**为什么不走"启动瞬间拿句柄"的快路**：`LaunchGameDirectly`（`:1950`）确实已经拿到 PID 与 `hProcess`（`LaunchThreadFunc:2050` 立刻 `CloseHandle` 丢掉），改成不关就能省掉这次枚举。但那只覆盖"从启动器启动"一条路，steam / thcrap 仍需枚举能力——两条路都要的东西，就只留一条。**`thprac_launcher_games.cpp` 一行不改。**

## 1.4 门控表

在 `thprac/src/thprac/thprac_launcher_games_def.h` 的 `struct ExeSig`（`:27-33`）之后、`struct THGameSig`（`:35`）之前插入：

```cpp
enum class GateCmp : uint8_t { Eq, Ne, BitClear };
struct THGameTimeFlag {
    uint32_t rva;      // 模块内偏移；0 = 尚未反编译
    uint8_t  bytes;    // 1 / 2 / 4
    uint32_t value;
    GateCmp  cmp = GateCmp::Eq;
};
struct THGameTimeGate {
    THGameTimeFlag gamemode;   // 关卡进行中
    THGameTimeFlag pause;      // 未暂停
    THGameTimeFlag replay;     // 非回放
};
```

`struct THGameSig`（`:35-49`）在 `ExeSig exeSig;`（`:48`）之后加：

```cpp
const THGameTimeGate* gametimeGate = nullptr;  // 非空 = 该作参与按局计时
```

**为什么挂指针而不是按 `idStr` 另建表**：

- 识别函数本就返回 `THGameSig*`，顺着走即可，无需按字符串二次查表；
- 不存在 `idStr` 键的歧义（th09/th095 的 `idStr` 不同但互斥体相同）；
- 同一条目的不同变体（原版 / Steam / 汉化）将来可各指不同 gate；
- 默认 `nullptr`，**其余 60 多个条目一个字都不用改**。

在 `static THGameSig gGameDefs[]`（`:353`）之前放 gate 定义（RVA = 绝对地址 − 0x400000）。**以下为已落地的实际内容**：

```cpp
static const THGameTimeGate gGateTh06 {
    { 0x2C6EA4, 4, 2 },                      // 0x6C6EA4 gameState == 2
    { 0x29D4BF, 1, 0 },                      // 0x69D4BF pauseMenuState == 0
    { 0x29BCBC, 1, 0 },                      // 0x69BCBC is_rep == 0
};
static const THGameTimeGate gGateTh07 {
    { 0x175AA4, 4, 2 },                      // Supervisor(0x575950)+0x154 gamemode == 2
    { 0x22F64C, 1, 0 },                      // GameManager(0x626270)+0x93DC：0=未开 1=刚开 2=打开中，故必须 == 0
    { 0x22F648, 4, 0x8, GateCmp::BitClear }, // GameManager+0x93D8 bit3 = replay
};
static const THGameTimeGate gGateTh08 {
    { 0x13CE8B0, 4, 2 },                     // Supervisor(0x17CE758)+0x158 gamemode == 2
    { 0x124D0BA, 1, 0 },                     // GameManager(0x160F508)+0x3DBB2：0/1/2，故必须 == 0
    { 0x124D0B4, 4, 0x8, GateCmp::BitClear },// GM+0x3DBAC bit3 = replay（该 dword 标题画面基线非 0，只能位测试）
};
static const THGameTimeGate gGateTh15 {
    { 0x0E9BB8, 4, 0, GateCmp::Ne },         // PLAYER_PTR != 0
    { 0x0E9B24, 4, 0x10, GateCmp::BitClear },// StageController+0x90 bit0x10 = 冻结（Esc / GameOver / Continue）
    { 0x0E7ECC, 4, 0xd, GateCmp::Ne },       // 场景场状态 != 0xd（0xd = 回放/演示播放中）
};
```

> th07 / th08 的 pause / replay 已于 2026-09-29 完成反编译 + 三态验证，RVA 直接可用——**P1a 视为已完成**。
> 判据出处、实测状态数与可选强化见 `docs/gametime/gametime_thxx_flags.md` §3。

**阈值本身**：`GateCmp` 三种（`Eq` / `Ne` / `BitClear`）已覆盖四作的全部形态（th15 用了后两种）。
**续关菜单（th07/th08）**：flags 文档提出的 bit-2 强化形态**未经三态验证**，P0 仍用已验证的 `pause == 0`；漏计记为已知项，留 P2 评估。

**TH06 三处地址的推导（可复核）**：`GAME_MANAGER = 0x69BCA0`（`thprac_th06.cpp:29`），`GameManager` 定义在 `thprac/src/thprac/thprac_th06.h:66-121`。

- `difficulty @ +0x10` = `0x69BCB0` ✓（与 `thprac_th06.cpp:171/534/1368` 吻合）
- `character @ +0x181D` = `0x69D4BD` ✓（与 `:172` 吻合）、`shotType @ +0x181E` = `0x69D4BE` ✓
- ⇒ `isInGameMenu @ +0x181F` = `0x69D4BF`（旁证：`:3240` 的 "Close pause menu" 直接把 `0x69d4bf` 置 0）
- `isInReplay @ +0x1C` = `0x69BCBC` ✓（`:3234` 以 `== 0` 为判据）

**求值与降级**：

- 短路顺序固定 `gamemode → pause → replay`；任一项不通过即返回 false，**不再读后续地址**。好处：菜单里每 tick 只花 1 次 RPM；避免在非法上下文读地址（th15 的 `0x4E9B24` 就只在关卡存活期有效，必须在 `PLAYER_PTR != 0` 之后读）。
- 读取：`ReadProcessMemory(hProc, (void*)(base + rva), &v, bytes, &n)`；`bytes` 必须是 1/2/4，且 `n != bytes` 也算失败。任一读失败即本 tick 视为"不计时"（保守，避免游戏崩溃瞬间刷时间）。
- `gametimeGate == nullptr` 的作**不建会话、不落盘**（与"分批推"一致，也避免纯墙钟时长混进统计池）。
- RPM 连续失败 → 本会话时长偏短（不落"纯墙钟"的假数据，比初版方案更保守）。

## 1.5 "一局"边界与采样循环

**开始**：`TryBindSession()` 枚举 → `OpenProcess` → `GetGameModuleBase` → `GetExeInfoEx` 指纹匹配且 `gametimeGate != nullptr` → 持有句柄、记 `start`（本地墙钟）。
认不出来（含"是游戏但不参与计时"）→ `CloseHandle` 放弃，整局不记。

**结束**：`WaitForSingleObject(hProc, 0) == WAIT_OBJECT_0`。

选用理由：

- **精确**：无轮询尾差（对比互斥体轮询最多有 1 秒尾差）；
- **能正确处理强杀 / 崩溃**：进程对象立即 signal；
- **没有 stash 踩过的坑**：launcher 持有**共享内存区**的句柄会让区在游戏退出后仍存活，从而探测不到退出；**进程句柄没有这个问题**（进程对象退出时必然 signal，即便句柄未关）。
- 代价：短暂延长已死进程的内核对象寿命，无害。

**唯一前提**：句柄确实指向那个游戏进程 —— 绑定那一刻已按 PE 指纹（`textSize` + `timeStamp`）校验过，与 `RemoteInit` 同一套判据。

**采样循环（就地扩展现有的 16ms 循环）**：

```
while (mUpdateGameTime) {
    Sleep(16);
    if (mGameTimeTestGameOpen_ns > 1000000000) {       // test every second —— 原有节拍，一行不改
        is_game_open = CheckIfAnyGame2();              // 原有调用，一行不改
        mGameTimeTestGameOpen_ns = 0;
        if (is_game_open) {
            if (!mSessionProc && !mBindTried) { mBindTried = true; TryBindSession(); }
        } else {
            mBindTried = false;                        // 下降沿复位，下一局重新给机会
        }
    }
    passed_time = ResetClock(clock_id) * 1e9;

    if (is_game_open) {                                // ← 旧累加器，原样保留
        mGameTime_ns    += passed_time;
        mGameTimeCur_ns += passed_time;
    }
    if (mSessionProc) {                                // ← 新累加器
        if (WaitForSingleObject(mSessionProc, 0) == WAIT_OBJECT_0) {
            CloseSession();                            // 进程已退出 → 收尾写盘
        } else if (SampleGate(mSessionProc, mSessionBase, mSessionSig->gametimeGate)) {
            mSessionPlayNs += passed_time;
        }
    }
    mGameTimeTestGameOpen_ns += passed_time;           // 原有两行，原样保留
    mGameTimeTooLongSE_ns    += passed_time;
}
CloseSession();                                        // launcher 先被关：仍写一行，end = 关闭时刻
LauncherSetGameTime(mGameTime_ns);                     // 原有，原样保留
```

> `passed_time` 是 `ResetClock` 的"读并重置基准"返回值，天然是"上一轮到现在"的 delta——沿用旧循环语义，未做 clamp。
> **探针节拍保持原有的 1 秒**，不为新功能调整。理由见 §3.3 的"会话起点精度"：受影响的只有 `start` 这个墙钟元数据字段，
> 而 `playNs` 要等门控通过（`gamemode` 成立 = 已经进关卡）才开始累加，那时早过了探测节拍的窗口。
> 单次 `TryBindSession` 会让当轮 tick 变长几毫秒，但 `passed_time` 是按真实经过时间计的，旧累加器的总量不受影响。

**各场景结果**：

| 场景 | 结果 |
| --- | --- |
| 正常退出游戏 | 句柄 signal → 立即写完整记录，`end` = 真实退出时刻 |
| 任务管理器强杀 / 游戏崩溃 | 同上，无损 |
| **launcher 先被关**（游戏仍在跑） | `LauncherGameSessionDestroy()` 停线程并写一行，`end` = launcher 关闭时刻。不丢数据，但时长被截短 |
| 多开 / 多作同开 | 只跟踪单会话（第二个出现时按 PID 顺序取第一个并记日志），不做并发 |
| launcher 从未启动 | 完全没有记录（固有缺口，见 §1.8） |

## 1.6 记录结构与落盘

**一期（P0 / P1）只有四个字段**，直接放在类成员里，不另立结构体：

```cpp
HANDLE     mSessionProc = nullptr;   // 游戏进程句柄，含 SYNCHRONIZE
THGameSig* mSessionSig  = nullptr;   // 指向 gGameDefs 静态条目
uintptr_t  mSessionBase = 0;
DWORD      mSessionPid  = 0;
SYSTEMTIME mSessionStart {};
int64_t    mSessionPlayNs = 0;
bool       mBindTried = false;       // 每个"有游戏"周期只枚举一次
```

**文件**：`<LauncherGetDataDir()>gametime_sessions.jsonl`，**一局一行**，行尾 `\r\n`：

```
{"game":"th06","start":"2026-09-29 21:03:11","end":"2026-09-29 21:41:52","durationNs":2321000000000}
```

**实现（内联，零新增文件）**：`thprac_launcher_others.cpp` 里一个 `static void AppendGameSessionLine(...)`，`_snprintf_s(_TRUNCATE)` 拼一行 + `CreateFileW(FILE_APPEND_DATA)` + `SetFilePointer(FILE_END)` + `WriteFile` + `WriteFile("\r\n")`，**零堆分配**。

- **不移植 `feat/plogth15` 的 `json_util.{h,cpp}`**：那会引入 2 个新文件 + `vcxproj(.filters)` 改动。本功能只有 4 个固定字段，其中 `gameId` 还是 `gGameDefs` 里的固定 ASCII token（无转义需求），一行格式串就够。
- **明确用 Win32 文件 API，不用 `std::fstream`**：仓库既有约定（`thprac_games_dx8.cpp:91`、`thprac_launcher_cfg.cpp:135`、`thprac_log.cpp:57` 等）。
- **`LauncherGetDataDir()` 返回绝对路径且以 `\` 结尾**，不必 `PushCurrentDirectory`；用前判空。

**写入时机**：会话进行中**不写盘**（`playNs` 只在内存）；一局一写，天然低频，无节流需求。

**P2 再评估的字段**：`gate`（本局实际生效的判据数）与 `closed`（是否观察到进程正常退出）。JSON 加字段不破坏既有解析。

**可选崩溃兜底（P2 候选）**：每 3 分钟把当前会话覆写进 sidecar `gametime_open.json`（`CREATE_ALWAYS` 单行小文件），正常收尾时 `DeleteFileW`；下次 `Init` 若发现该文件则合成一条记录后删除。约 25 行，只解决"launcher 崩溃"，不解决"launcher 不存在"。

**与现有 `"Gametime"` 完全不动**：`LauncherGetGameTime` / `LauncherSetGameTime`（`thprac_launcher_cfg.cpp:2874` / `:2886`）、`"Gametime"` 键、`THGameTimeRecorder` 的累加与落盘时机**一行不改**。

⚠️ **不要把记录塞进 `thpracP.json`**：`LauncherCfgWrite()`（`thprac_launcher_cfg.cpp:86-102`）是 `SetFilePointer(0)` + `SetEndOfFile` 的**整文档重写**，且每次 `LauncherSettingSet` 都触发。这也是走独立 JSONL 的硬理由。

**开关**：**共用** Others 页现有 `recordGameTime`（`thprac_launcher_cfg.cpp:2807`），不新增设置键、不加 GUI 文案（受 §1.1 第 15 条约束）。

## 1.7 逐文件改动清单

**新增文件：无。修改 2 个文件。**

| # | 文件 | 内容 |
| --- | --- | --- |
| 1 | `thprac/src/thprac/thprac_launcher_games_def.h` | `GateCmp` / `THGameTimeFlag` / `THGameTimeGate`（`:33` 后）；`THGameSig` 加 `const THGameTimeGate* gametimeGate = nullptr;`（`:48` 后）；`gGameDefs` 前放 `gGateTh06/07/08/15`；th06 / th07 / th08 / th15 四个条目尾部各追加指针 |
| 2 | `thprac/src/thprac/thprac_launcher_others.cpp` | `#include "thprac_load_exe.h"`；文件内 static：`ReadFlagValue` / `EvalRaw` / `EvalFlag` / `SampleGate` / `ExeNameLooksLikeTouhou` / `AppendGameSessionLine`；`THGameTimeRecorder` 加 7 个成员 + `TryBindSession` / `BindProcess` / `CloseSession`；`UpdateGameTime()` 循环里插绑定与采样；`Gui()` 里加调试读数面板 |

**明确不改（初版方案的另外 6 处，全部砍掉）**：

| 初版改动 | 砍掉的依据 |
| --- | --- |
| `thprac_load_exe.{h,cpp}`：拆 `IdentifyRunningGameByPID` | `GetExeInfoEx` 不做 CARP 签名门，本来就能认已注入进程（§1.1 第 5 条） |
| `thprac_launcher_others.h`：导出 `CheckIfAnyGame2()` | 不改它的签名，就不需要跨 TU 声明 |
| `thprac_launcher_others.cpp:22-60`：拆 `GameSigOpenTest` + 改返回 `THGameSig*` | "是哪一作"由枚举 + 指纹回答（§1.2） |
| `thprac_launcher_games.cpp`：`LaunchGameDirectly` 加 `pOutPid`、不关句柄 | 枚举是每局一次，不是每 tick；且 steam/thcrap 仍需枚举（§1.3） |
| `thprac_launcher_main.cpp`：三行 `LauncherGameSessionInit/Update/Destroy` | 沿用旧线程，不需要新接线 |
| `json_util.{h,cpp}` + `vcxproj(.filters)` | 落盘内联在 `others.cpp`，4 个字段用不上 JSON 库（§1.6） |

**也不改**：`CheckIfAnyGame2()`（函数体与唯一调用点 `:375` 全不动）、`thprac_th06/07/08/15.cpp`（游戏侧零改动，旧机制保留）。

**调试读数面板**（已实现，打样期验证门控 RVA 的最快手段）：在 `THGameTimeRecorder::Gui()` 里显示当前绑定 `idStr` / `pid` / `base` / 本局 `playNs`，以及三个标志位的**原始读数与逐项判定**。按采样时的短路顺序逐项读，因此面板输出与真实判定路径一致。只用既有的 `THPRAC_GAME_TIME_*` 词条，**零 locale 成本**（`thprac_locale_def.h/.cpp` 是自动生成的，本地无法重新生成）。

## 1.8 健壮性补缺（可选项，本轮不实现）

- **(a1) 让 CLI / 自动路径的 launcher 无窗口驻留**：`RunGameWithTHPrac`（`thprac_main.cpp:204`）现在在 `:239-240` 关掉句柄就返回；改成把 `hProcess` 交给记录器后 `WaitForSingleObject(hProcess, INFINITE)` 再返回。**窗口 / 托盘 / UX 全不变**（该进程本来就没有 GUI），只是任务管理器里多一个 headless `thprac.exe` 驻留整局；因为不是 Job Object，用户杀掉它不会连坐游戏。**覆盖 `main.cpp:155` 与 `:224` 两条缺口，代价低、风险低，推荐。**
- **(a2) 改成进 GUI / 最小化到托盘**：`main.cpp:224` 不 `return 0` 而继续 `GuiLauncherMain()`（`LauncherWndMinimize` 已存在，`thprac_launcher_main.cpp:239`）。UX 回归风险中高，不推荐。
- **(b) 游戏侧心跳 + launcher 下次启动补全**：**不推荐**。游戏侧没有定时线程且退出无清理 → 心跳文件在游戏退出后必然残留，launcher **无法区分"这份心跳属于已退出的游戏"还是"游戏此刻仍在运行"**，可能造出**虚假记录**（比少一条记录糟糕得多）；而一旦引入进程存活校验来消歧，就已经在做主方案了。

## 1.9 统计详情页（P3，已实现）

**入口**：Others 页 `THGameTimeRecorder::Gui()` 里「重置时间」右侧的「查看时长详情」按钮 → `THOthersGui` 的二级页
`GuiGameTimeDetail()`（仓库标准的 `mGuiUpdFunc` 换页手法，见 `thprac_launcher_games.cpp:2469`）。

**数据流**：进页时 `LoadGameSessionEntries()` 读一次 `<数据目录>gametime_sessions.jsonl`，之后纯内存聚合；
会话只在退出时落盘，所以不做定时刷新。

**筛选**：

| 维度 | 取值 | 默认 |
| --- | --- | --- |
| 游戏 | `全部` + 数据里出现过的作品（按 `gGameDefs` 顺序建列表，标题取 `S(sig->refStr)`） | 全部 |
| 时间范围 | 本年度 1 月..当月（标签 `%04d-%02d`，纯数字免文案） | 当月 |

**展示**：选中月份内逐日累加 `durationNs`，只列有记录的天，末行「合计」；两者都用本地 `FormatGameTimeShort()`（`H:MM:SS`）。
日均/总计在「全部」筛选下就是各游戏之和。当月无记录时不加空状态文案，只有合计行 `0:00:00`。

**解析**：格式是我们自己写的定长格式，**不引入 JSON 库**（与 §1.6 落盘侧对称）——`sscanf_s` 取
`game`/`start`，`durationNs` 用 `strstr` 定位，因此 §1.6 预告的 P2 加字段不会打断解析。

**新文案**：`THPRAC_GAME_TIME_DETAIL` / `_DATE` / `_DURATION` / `_ALL` / `_SUM` 五个词条。
`thprac_locale_def.h/.cpp` 是 devtools 生成的，本次**手工同步**：枚举末尾追加（不动既有下标）+ 三个语言块末尾各补 5 条 +
`th_glossary_str[3][1871]` → `[3][1876]`。核对手段（**必须做**，漏补一个块编译器不报错、只会得到空指针）：

```
grep -c '^        "' thprac/src/thprac/thprac_locale_def.cpp   # 期望 3 × 1876 = 5628
```

**已知边界**：① 记录开关关掉时按钮随 `if (mEnableRecordGameTime)` 一起消失，历史数据也就进不去；
② 时间范围只有本年度，跨年数据不可达；③ 正在游玩的那一局尚未落盘，页面上看不到。

---

# 二、逆向工程方案

## 2.1 目标物

每作需要三个量，合取后表示"正在有效游玩"：

| 量 | th06 参考 | th15 参考 | 语义 |
| --- | --- | --- | --- |
| gamemode | `*(DWORD*)0x6C6EA4 == 2` | `PLAYER_PTR(0x4E9BB8) != 0` | 关卡进行中（排除标题 / 菜单 / 选人） |
| pause | `*(BYTE*)0x69D4BF == 0` | `(*(uint32_t*)0x4E9B24 & 0x10) == 0` | 游戏自身暂停菜单未打开（**不是** thprac 自己的 `THPauseMenu`） |
| replay | `*(byte*)0x69BCBC == 0` | `*(uint32_t*)0x4E7ECC != 0xd` | 未在播放 replay |

⚠️ **注意区分**：多作里已有的 `CheckReplay()`（如 `thprac_th10.cpp:404`）是**读取 replay 文件参数**（`ReplayLoadParam`），与"实时是否在回放"是两件事，**不能复用**。

## 2.2 策略：先分代，再同构搬运

各代引擎机制类似，标志位必然可找到 —— 把这一点变成可执行的编排：

1. **建立引擎分代表**（落在 `docs/gametime/gametime_thxx_flags.md` 第一节）。把 22 作按引擎同构性分组，组内**以证据为准**（代码里同构的判据、相同的结构体形状、相近的 `GameManager` / `Supervisor` 布局），不靠年份猜。
   初版推测（**待验证**）：**A 代** th06–th09 / th095；**B 代** th10–th13 与 th125 / th128；**C 代** th14–th18 与 th143 / th165 / th185；**D 代** th19 / th20（需 base+RVA）。
2. **每代挑一个"参照作"把三个量的结构位置摸透**。已有参照：A 代 th06（三量齐全）、C 代 th15（三量齐全）。
3. **同代其余作按同构搬运**：同代内 gamemode 全局量、暂停菜单对象、回放管理器的**相对位置与判据形态**往往一致，逐作只需确认基址与偏移。
4. **逐作三态验证**（§2.3 第 5 条），不改判据形态。

## 2.3 定位方法（按性价比排序）

1. **从已知指针反查字段偏移（最高性价比）**。多作的 `REPLAY_MGR_PTR` / `PAUSE_MENU_PTR` 已存在但字段偏移未知：

   | 作 | 已知指针 | 备注 |
   | --- | --- | --- |
   | th13 | `REPLAY_MGR_PTR = 0x4c22c8` | `thprac_th13.cpp:16` |
   | th18 | `REPLAY_MANAGER_PTR = 0x4cf418`、`PAUSE_MENU_PTR = 0x4cf40c` | `thprac_th18.cpp:39/61` |
   | th20 | `REPLAY_MGR_PTR = 0x1c60fc` | `thprac_th20.cpp:758` |

   用 Ghidra 从这些指针的使用点展开结构体，找 bool / 位标志字段。**th18 的 `PAUSE_MENU_PTR` 是"暂停菜单对象"的现成模板。**

2. **复用 thprac 自己的 hook 锚点**。thprac 已挂的"暂停 / 恢复"相关 hook 附近就是暂停标志。**th07 / th08 的具体切入点**：
   - th07：`th07_unpause_prevent_desync`（`thprac/src/thprac/thprac_th07.cpp:2091`，`0x403481`）
   - th08：`th08_unpause_prevent_desync`（`thprac/src/thprac/thprac_th08.cpp:3107`，`0x40421a`）
   - 回放：从 `th07_rep_menu_3`（`0x45b2c1`）/ `th08_rep_menu_3`（`0x46ec2e`）反查其写入的全局
   - 其余作同理：`th09_unpause`（`thprac_th09.cpp:342`，`0x434ad8`）、`th18_pause_skip_1/2`（`thprac_th18.cpp:938-939`）

3. **同代同构搬运**（§2.2 第 3 步）。

4. **Ghidra 静态定位**：从暂停菜单 UI 资源 / 字符串交叉引用找暂停菜单构造点；gamemode 类全局量从关卡起止处被写入的常量反查。

5. **真机三态探针验证（不可省）**：读候选地址，在"正常游玩 / 打开暂停菜单 / 播放 replay"三种状态下各打印一次，三者取值必须互不相同且符合判据。**未经此步的地址不得写进 gate。**

## 2.4 工具链

- Ghidra 12.1.2：`D:\tools\ghidra_12.1.2_PUBLIC\support\analyzeHeadless.bat`；JDK 21 走 `JAVA_HOME=D:\tools\AS\jbr`。
- Ghidra 12.x **无内置 Jython**，脚本必须写 **Java**（`-postScript *.java`）；项目目录须先存在；脚本目录里放编译失败的 `.java` 会污染该目录的 OSGi bundle，导致**整目录脚本全挂**（清 `%APPDATA%\ghidra\ghidra_12.1.2_PUBLIC\osgi\compiled-bundles\`）。
- **本机 Python 有 capstone**：对 PE 做"解析节表 → `va2off` → `Cs(CS_ARCH_X86, CS_MODE_32)`"的快速扫描，比开 Ghidra 轻得多，适合先做筛选与交叉引用排查。
- 打样两作的二进制已在手：`D:\workshop\reasm\th07.exe`（650752 B，`ts=1066942337`）、`th08.exe`（840704 B，`ts=1095610188`），指纹与 `gGameDefs` 一致、`ImageBase=0x400000`、不重定位。
- 旧流程模板：`D:\workshop\wind\{th14,th15}decode\`（`ghidra_scripts/`、`docs/thXX_re_disasm_{requests,report}N.md`、`summary.md`）。th15 的既有资料可查，或可省掉部分工作。

## 2.5 交付物

`docs/<作>/<作>_re_disasm_requests.md`（需求）→ `<作>_re_disasm_report.md`（结论：地址 + 判据 + 三态验证记录）。跨作汇总进 `docs/gametime/gametime_thxx_flags.md`。

---

# 三、需求进度表

## 3.1 每作门控现状

图例：✅ 已确认 ｜ ⚠️ 有线索待补 / 待验证 ｜ ❌ 待反编译

| 作品 | 代(推测) | 游戏进行中 | 未暂停 | 非 replay | 状态 |
| --- | --- | --- | --- | --- | --- |
| **th06** | A | ✅ `0x6C6EA4==2` | ✅ `0x69D4BF==0` | ✅ `0x69BCBC==0` | **✅ 已接入** |
| **th07** | A | ✅ `0x575AA4==2` | ✅ `0x62F64C==0` | ✅ `(0x62F648&8)==0` | **✅ 已接入** |
| **th08** | A | ✅ `0x17CE8B0==2` | ✅ `0x164D0BA==0` | ✅ `(0x164D0B4&8)==0` | **✅ 已接入** |
| th09 | A | ❌ | ❌ | ❌ | 同代可搬运 |
| th095 | A | ❌ | ❌ | ❌ | 同代可搬运 |
| th10 | B | ❌ | ❌ | ❌ | — |
| th11 | B | ✅ `0x4a8e88+0x74` | ❌ | ⚠️ `0x4a8eb8→+0xa &1` | 补齐中 |
| th12 | B | ✅ `0x4b44e8+0x74` | ❌ | ⚠️ `0x4b4518→+0xa &1` | 补齐中 |
| th13 | B | ❌ | ❌ | ⚠️ `0x4c22c8` 已知、offset 未知 | — |
| th125 | B′ | ❌ | ❌ | ❌ | — |
| th128 | B′ | ❌ | ❌ | ❌ | — |
| th14 | C | ❌ | ❌ | ❌ | `D:\workshop\wind\th14decode` 有既有资料 |
| th143 | C | ❌ | ❌ | ❌ | — |
| **th15** | C | ✅ `0x4E9BB8 != 0` | ✅ `(0x4E9B24&0x10)==0` | ✅ `0x4E7ECC != 0xd` | **✅ 已接入** |
| th16 | C | ❌ | ❌ | ❌ | — |
| th165 | C | ❌ | ❌ | ❌ | — |
| th17 | C | ❌ | ❌ | ❌ | — |
| th18 | C | ❌ | ⚠️ `0x4cf40c` 已知、offset 未知 | ⚠️ `0x4cf418` 已知、offset 未知 | — |
| th185 | C | ❌ | ❌ | ❌ | — |
| th19 | D | ❌ | ❌ | ❌ | 需 base+RVA；两个版本条目 |
| th20 | D | ❌ | ❌ | ⚠️ `0x1c60fc` 已知、offset 未知 | 需 base+RVA |
| alcostg | — | ❌ | ❌ | ❌ | 优先级最低 |

## 3.2 阶段进度表

| 阶段 | 内容 | 产出 | 状态 |
| --- | --- | --- | --- |
| **文档** | D1 本文档；`docs/th07/th07_re_disasm_requests.md`；`docs/th08/th08_re_disasm_requests.md`；`docs/gametime/gametime_thxx_flags.md` | 四份文档 | 已完成 |
| P0 | 骨架：gate 表 + 枚举认游戏 + 会话边界 + JSONL 落盘；th06 / th07 / th08 / th15 四作门控已填 | 代码（2 个文件） | **已完成，Release\|x86 构建通过；真机三态验证待做** |
| P1a | th07 / th08 反编译取 pause / replay（在 `D:\workshop\reasm`） | th07 / th08 报告 | **已完成**（已定案并三态验证） |
| P1b | th07 / th08 三态探针验证（地址已验证，需验接入后的端到端行为） | 四作打样完成 | 待做 |
| P2 | 分批推其余作品（分代 + 同构搬运）；同时补 `gate` / `closed` 字段与崩溃兜底 | 各作 gate + 标志位总表补齐 | 未开始 |
| P3 | 统计侧（读 `gametime_sessions.jsonl` 做汇总的面板 / 工具） | Others 页「查看时长详情」二级页：按作品 + 月份筛选、逐日汇总（见 §1.9） | **已完成**（真机待验） |

## 3.3 风险与开放问题

- **launcher 必须在场**（已接受）。补缺方案见 §1.8。
- **地址随版本变**：gate 表以 `THGameSig` 条目为单位，但同一 `idStr` 有原版 / Steam / 汉化 / 魔改多个指纹，而 `RemoteInit` 只按 `timeStamp` + `textSize` 匹配。**非原版变体是否共用同一地址需实测**（汉化版改了 `.text` 段大小，反而可能不会误匹配）。
- **th19 有两个条目**（v1.00a / v1.10c），地址不同，必须分别填。
- **多开**：单会话设计。`TryBindSession` 只在未绑定时枚举，因此**只认第一个**（Toolhelp 顺序），后开的自然被忽略——不做并发。
- **th06 数据重复**：TH06 会有两份时长（游戏侧 `score06.dat` 按难度×机体；launcher 侧分条），本轮明确接受。
- **引擎分代表（§2.2 第 1 条）目前是推测**，须先用证据验证后再作为"同构搬运"的依据。
- **会话起点精度**：绑定由探针发现驱动，`start` 最多晚一个探针节拍（**1 s**）。刻意**不**动这个节拍、也**不**改 `LaunchGameDirectly` 去换取"精确到 `ResumeThread` 前"（§1.3）——`start` 只是墙钟元数据，而 `playNs` 要等门控通过才累加，两者都不值得为它改既有代码。
- **exe 名快筛可能漏**：`ExeNameLooksLikeTouhou` 照搬 `CheckProcessOmni` 的白名单 + `thNN` 形态。若某个魔改/汉化版改了 exe 名且不在白名单里，该局不记录（与 launcher 自己的"找游戏"路径同一限制，行为一致）。绑定是一次性的，**不会中途补救**——这是刻意的：宁可不记录，也不要一条起点是半局的静默错数据。
- **续关菜单漏计（th07 / th08）**：`pause == 0` 不覆盖死亡后的 Continue 菜单（`GM+0x93DD` / `GM+0x3DBB3`），那一状态下三个判据全成立会误计。flags 文档给了 bit-2 强化形态但**未经三态验证**，P0 不采用；记入 P2。
- **调试面板的数据竞争**：面板在 GUI 线程读 `mSessionPlayNs` 等，与工作线程写之间无同步——与旧成员 `mGameTime_ns` 的现状完全一致（本类本来就没做同步）。面板是打样期脚手架，接受。

## 3.4 端到端验证（施工后执行）

**构建**（已跑通，exit 0，产出 `Release\thprac.exe`）：

```
"D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe" thprac.sln -p:Configuration=Release -p:Platform=x86 -m
```

**快速语法自检**（不开整包构建）：`docs/gametime/_syntaxcheck.bat`（vcvarsall x86 + `cl /Zs /utf-8` 逐个 TU，几秒出结果）。

**前置**：Others 页确认 `recordGameTime` 打开；调试读数面板已在 `THGameTimeRecorder::Gui()` 里（§1.7）。

1. **认游戏正确性**：启动器启动 th06 → 读数显示 `game=th06`、`base=0x400000`；th07 / th08 / th15 同样核对。边界：分别单开 th09 与 th095 → 都能正确区分（同互斥体，靠 PE 指纹）。
2. **gamemode 门控**：停在标题 / 菜单 / 选人 / Replay 菜单 → `playNs` **不涨**；进第 1 关 → 开始涨。
3. **暂停门控**：th06 游戏中按 Esc → `playNs` 冻结；返回游戏 → 恢复。
4. **回放门控**：th06 播放存档 replay → `gamemode == 2` 但 replay 判据成立，`playNs` **不涨**。**关键用例。**
5. **th15（复合形态）**：同样跑 2~4，验证 `Ne` / `BitClear` 两种比较都生效。
6. **会话边界**：正常退出 → `gametime_sessions.jsonl` 新增一行、`durationNs` 与 UI 相符（±1 tick）；**任务管理器强杀** → 同样新增、延时 <16ms；游戏中挂着不动 30 s 再退出 → `durationNs` 只等于真实游玩段。
7. **不污染旧功能**：全程 Others 页"总飞行时间"行为与改动前一致（进程开着就涨，含暂停 / 回放）；"玩太久"提示阈值不变；`thpracP.json` 的 `"Gametime"` 照常更新；th06 的 IGI 时长面板正常。
8. **非打样作不记录**：开 th09 完整玩一局退出 → 不新增行。
9. **launcher 先关**：游戏在跑时关启动器 → 新增一行，`end` = 关闭时刻。
10. **文件格式**：`python -c "import json;[json.loads(l) for l in open(p,encoding='utf-8')]"` 逐行校验合法 JSON、无 BOM、`\r\n` 行尾。

---

## 4. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-29 | 建档：初版方案（8 个改动点、新建 `THGameSessionRecorder`、启动瞬间拿句柄 + 枚举兜底、移植 `json_util`）。 |
| 2026-09-29 | **极简版落地（P0）**。复核确认初版的 8 处里只有 1 处是功能数据本身（gate 表），其余 7 处都在解决"拿不到 `ReadProcessMemory` 的入参"。按"统一走枚举 + 就地扩展 `THGameTimeRecorder`"重做，**改动收敛到 2 个文件、零新增文件**（§1.7 的对照表列出被砍的 6 处及依据）。同时：th07 / th08 的 pause / replay 已定案并三态验证，`gGateTh07` / `gGateTh08` 填上真值，P1a 结项。实现后 `Release\|x86` 整包构建通过。 |
| 2026-09-29 | **回退一处越界改动**：P0 初版顺手把探测节拍从 1 s 收到 100 ms，属于修改既有行为去换取 `start` 字段的精度——而 `start` 只是墙钟元数据，`playNs` 并不受影响（要等门控通过才开始累加）。**已改回原有的 `> 1000000000` / `// test every second`**。既有代码的节拍不为本功能服务。 |
| 2026-09-29 | **P3 详情页落地**（§1.9）。Others 页新增「查看时长详情」二级页：作品 + 月份筛选、逐日汇总、合计行。改动集中在 `thprac_launcher_others.cpp`（解析 4 个 static + `THOthersGui` 换页）；`THGameTimeRecorder::Gui()` 由 `void` 改 `bool`（唯一调用点）；`thprac_launcher_games.h` 补 1 行 `LoadJsonFile` 声明以复用既有读文件；手工同步 5 个 locale 词条到生成文件。记录/门控/落盘路径**一行未动**。 |

### 实现与初版方案的差异（一句话版）

1. **不新建兄弟类**，就地扩展现有 16ms 循环；新旧累加器各自独立。（§1.2）
2. **不走"启动瞬间拿句柄"**，统一枚举认游戏，steam / thcrap / 手动启动一并覆盖。（§1.3）
3. **不需要 `IdentifyRunningGameByPID`**，`GetExeInfoEx` 不做 CARP 门，本来就能认已注入进程。（§1.1 第 5 条）
4. **不移植 `json_util`**，落盘内联约 20 行。（§1.6）

# logger 日志记录（JSONL 落盘）— 实施记录（beta）

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志；**本迭代做日志记录**，监控已在前一迭代落地）
> 上游文档：`docs/logger/logger_re_disasm_overview.md`（术语口径，以此为准）、`docs/logger/logger_progress.md`（进度，唯一事实来源）、`docs/th16/th16_monitor_impl_report.md`（监控侧参考实现）
> 白名单数据：`docs/logger/th14..th18_checkpoints.csv`（本迭代消费其第 4 列「记录名」）
> 代码：`thprac/src/thprac/thprac_games.{h,cpp}`（共享会话模块，新增 `#pragma region Game Session Log`）、`thprac/src/thprac/thprac_th14/15/16/17/18.cpp`（各作 Logger 接线）
> 分支 / 提交：`lxn/game-logger`；`feat(logger): 共享一局日志落盘模块`、`beta: logger 日志落盘 th14–th18 接线`
> 状态：**已接线 + 编译通过（Release|x86）**；**真机未测**（按用户安排，本功能补齐后统一验证）
> 地址口径：本文不新增任何地址。`PLAYER_PTR` / `STAGE_NUM` / 机体地址一律复用各作 `thprac_thXX.cpp` 顶部的 `enum addrs` 或 IGI 面板既有读法。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 文件格式 | **JSONL**：一行一局，纯追加，不读回改写 |
| 文件位置 | `<数据目录>logger_sessions_YYYYMM.jsonl`（每月一个，五作共享，靠 `game` 字段区分） |
| 一局边界 | **`*_game_start`（开新局，含重开）** + **`PLAYER_PTR` 由非 0 变 0（一局结束）** |
| 结束判据来源 | **直接复用各作 IGI 面板的显隐判据**（`thprac_th16.cpp:922-936` 的 `OnPreUpdate` 用的就是 `GetMemContent(PLAYER_PTR)`），不新增任何逆向 |
| 启动 checkpoint | 本局**首次 latch** 到的记录名（练习 warp 时落点即练习选的段落） |
| checkpoint 字段 | 只有「启动」与「结束」两个（需求 §5 的「最后checkpoint」与「结束checkpoint」合并，用户 2026-10-17 定案） |
| 强退进程 | 主动放弃记录，不处理（用户 2026-10-17 定案） |
| 落盘位置 | `GameSession::Tick()` 里，**一局至多一次** |
| 代码量 | 共享模块 2 文件（新增 region，不加新文件）；五作各 8 处调用，逐字同构；**th17 另有第 9 处**（`break` / 撞咆哮，非逐字同构） |
| 工程文件 | **不需要**改 `thprac.vcxproj` / `.filters`（无新文件） |

---

## 1. 架构与分层

| 层 | 文件 | 职责 | 知道什么 |
| --- | --- | --- | --- |
| 共享容器 + 序列化 + 写盘 | `thprac_games.h`（`#pragma region Game Session Log`，插在 `#pragma region Json` 的 endregion 之后）、`thprac_games.cpp`（同 region，插在 `#pragma region Virtual File System` 之前） | `GameLogEventType` / `GameLogEvent` / `GameSessionData` / `GameSession` / `GameLogRecordOfSection()` / `GameSessionAppendJsonLine()` | 只知道「一局」这个概念，**不知道任何游戏、地址、checkpoint 语义** |
| per-game 拥有者 | `thprac_th14/15/16/17/18.cpp` 里的 `THxxLogger` | 持有 `GameSession mSession;`，在 4 个调用点接线，提供「当前段落」 | 各作的 ECL 采样细节 |

**为什么放在 `thprac_games.*`**：① 既有 JSON 序列化宏（`CreateJson` / `AddJsonValue` / `AddJsonValueEx` / `ReturnJson`，`thprac_games.h:332-446`）就在这个文件里；② `thprac_games.cpp` 已经是「游戏侧共享 + 碰文件」的 TU（`ReplaySaveParam` 在 `:1198` 用 `CreateFileW`/`WriteFile`），且 `LauncherGetDataDir` 的头已在 `:2` include；③ 五个 per-game cpp 首个 include 就是 `thprac_games.h`。

**为什么「一局」归各作 Logger 拥有**：构成一局的输入（首次 latch 的段落、当前段落 `mSection`、miss/bomb 发生时的段落）全是各作 Logger 的私有状态。放进共享单例就要维护两套生命周期，必然漂移。共享模块只当「容器 + 序列化器 + 写盘器」。

### 1.1 零堆分配约束（本迭代唯一的硬性技术约束）

thprac 的 `EHOOK` 是 **INT3 + VEH 同步回调**，跑在触发指令的游戏线程、异常处理上下文里（`thprac_hook.cpp:217-266`）。前车之鉴：`feat/plogth15` 分支的 `thprac_th15_missbomb.{h,cpp}` 因「在热 hook 里用 `std::ofstream`/堆分配」崩溃被弃用（`docs/th15/th15_ecl_re_requests.md:60`）；th16 的 `OnStageStart()` 也特意做成零分配（`thprac_th16.cpp:180` 的注释）。

对应到本方案：

- `Begin()` / `Latch()` / `PushEvent()` 全部**只做指针赋值与 POD 拷贝**（`mPending = mCur` 对只含定长数组和标量的结构体就是一次 `memcpy`），一个 `new`、一个 `std::string`、一次文件 IO 都没有。
- `events` 是定长数组 `GameLogEvent events[256]`（`const char*` + `int8_t`，8 B/项，共 2 KB），不需要增长容器；溢出丢弃并计 `droppedEvents`。
- `GameSessionData` 里的 `const char*` 一律**指向冻结字符串**（各作 `mEntries` 里的 `std::string`，`LoadCsv()` 后永不增删，见 `thprac_th16.cpp:120-127` 注释）或字面量 `"main"`，**永不下垂**；`shot` 用定长 `char[24]` 存副本，避免持有临时对象。
- 序列化 + 写盘全部收在 `Flush()` 里，只从 `Tick()` 调用，**一局至多一次**。

> 「非热路径」在本仓库里的确切含义：注入进游戏进程的模块里**所有代码都在某个 hook 回调里跑**，不存在真正的线程外路径。这里的「非热」= **每帧一次的派发钩子**（`thXX_update`，60 次/秒，且同处已在做 ImGui 绘制与 `std::format`），相对于「每次 miss/bomb 触发」的钩子。写盘频率是一局一次，再差两个数量级。

---

## 2. JSON schema

一行一局，`\n` 分隔，纯追加：

```json
{"game":"th16","start":"2026-10-17 21:03:44","shot":"ReimuA","startCheckpoint":"ST1 前半 #1","endCheckpoint":"ST3 BOSS1","checkpointCount":11,"events":[{"type":"bomb","checkpoint":"ST1 后半 #2"},{"type":"miss","checkpoint":"ST2 BOSS2"}]}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `game` | string | `th14`…`th18`。月度文件五作共享，靠它区分 |
| `start` | string | `YYYY-MM-DD HH:MM:SS` 本地时间，**与兄弟文件 `gametime_sessions.jsonl` 完全同格式**（不引入 ISO-8601 `T`，避免同目录两种时间口径） |
| `shot` | string | 机体类型，取该作 IGI 面板 `diff_pl` 行显示的机体串。**th18 留空**（其 IGI 无 `diff_pl` 行、机体地址未登记，见 §9） |
| `startCheckpoint` | string | 本局**首次** latch 到的记录名；一次都没 latch 则 `"main"` |
| `endCheckpoint` | string | 本局**最后一次** latch 到的记录名；同上兜底 `"main"` |
| `checkpointCount` | int | latch 次数（含启动 checkpoint，即首次 latch 记 1）。等价读法：`checkpointCount - 1` = 启动之后又经过了几段 |
| `events` | array | `{type, checkpoint}`；`type ∈ {"miss","bomb","break"}`（**`break` 仅 th17 产出**，= 撞咆哮）；`checkpoint` = 事件发生时 IGI 面板显示的段落（未 latch 前为 `"main"`） |
| `eventsDropped` | int | **仅在 >0 时出现**；`events` 溢出 256 后被丢弃的条数 |

固定字段顺序（writer 决定）：`game, start, shot, startCheckpoint, endCheckpoint, checkpointCount, events[, eventsDropped]`。读取端按 key 取值，不依赖顺序（吸取 `ParseGameSessionLine` 里 `durationNs` 用 `strstr` 定位的教训，`thprac_launcher_others.cpp:171-173`）。

**序列化用 rapidjson 宏而不是 `_snprintf_s` 手拼**：`AppendGameSessionLine` 用的是 256 字节栈缓冲，字段一长就静默截断；本方案走 rapidjson + `std::string`，天然无截断，也免费拿到转义正确性。注意 `AddJsonValue*` 宏用**变量名**当 key（既有写法，见 `thprac_th16.cpp:399-400`），所以函数里的局部变量名必须与字段名一致；字符串值必须带 `jalloc` 第三参（`AddJsonValueEx(game, gameId, jalloc)`），否则会走到只接受字面量的重载而编译失败。

---

## 3. 写盘路径

```cpp
void GameSessionAppendJsonLine(const char* gameId, const GameSessionData& data)
{
    auto dir = LauncherGetDataDir();          // 带尾 '\'；空串 ⇒ 不可用
    if (dir.empty()) return;

    auto line = GameSessionBuildJsonLine(gameId, data);
    if (line.empty()) return;

    SYSTEMTIME now {};
    GetLocalTime(&now);
    wchar_t suffix[32];
    swprintf_s(suffix, L"logger_sessions_%04u%02u.jsonl", (unsigned)now.wYear, (unsigned)now.wMonth);

    auto hFile = CreateFileW((dir + suffix).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    defer(CloseHandle(hFile));

    SetFilePointer(hFile, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(hFile, line.c_str(), (DWORD)line.size(), &written, nullptr);
    WriteFile(hFile, "\n", 1, &written, nullptr);
}
```

- **写盘骨架照搬** `thprac_launcher_others.cpp:132` 的 `AppendGameSessionLine()`（`FILE_APPEND_DATA` + `OPEN_ALWAYS` + `SetFilePointer(FILE_END)`），不跟着抄它「256 字节栈缓冲」那处隐含截断缺陷。
- **文件名**：`logger_sessions_YYYYMM.jsonl`，与兄弟文件 `gametime_sessions.jsonl` 同族。**五作共享一个文件**，因为需求 §4 说「每一个月一个文件」（单数），且与 gametime 的既有形态一致；要按作分文件只需把前缀换成 `gameId`，一行改动。
- **月份 key** 取**写盘时刻**的 `wYear`/`wMonth`。跨月边界（10-31 开始、11-01 结束）会落进 11 月文件，而 `start` 仍记 10-31 —— 可接受，登记于此。
- **行终止符** `\n`（JSONL 规范）。兄弟文件用 `\r\n`，两者都能被逐行解析。
- **目录不存在**：`%APPDATA%\thprac\` 或 exe 同级 `.thprac_data\` 由 launcher 侧 `LauncherCfgInit(true)`（`main.cpp:169`）创建，注入进程里必然已存在；**不做 `CreateDirectoryW`**（保持「静默失败」的简单语义）。
- **失败全静默**：目录空 / `CreateFileW` 失败 / `WriteFile` 失败一律 `return`，不弹窗、不打日志、不重试。统计功能绝不能干扰游玩。
- `LauncherGetDataDir()` 在注入进程里可用：thprac.exe 是被 `LoadLibraryW` 加载进游戏进程的（`thprac_load_exe.cpp:175-184`），`__ImageBase` 仍指向 thprac 模块，`GetModuleFileNameW(&__ImageBase)` 得到 thprac.exe 所在目录。

---

## 4. 逐作接线

每作 **8 处**改动（逐字同构，只差文件号、`PLAYER_PTR` 值与机体表达式）；**th17 另有第 9 处**（撞咆哮 → `OnBreak()`，见下表末行）：

| # | 位置 | 改动 |
| --- | --- | --- |
| 1 | `THxxLogger` public 区（`CurrentSection()` 之后） | `void OnMiss();` `void OnBomb();` |
| 2 | `THxxLogger` private 成员区末尾 | `GameSession mSession;` |
| 3 | `THxxLogger::OnStageStart()` 体首 | `mSession.Begin("thXX");` |
| 4 | `OnStageStart()` 定义之后 | `OnMiss()` / `OnBomb()` 两个定义（各一行 `PushEvent`） |
| 5 | `THxxLogger::Update()` 体首（**所有 early return 之前**） | `mSession.Tick(inGame, <机体串>);` |
| 6 | `THxxLogger::Update()` 体末 | `mSession.Latch(GameLogRecordOfSection(mSection));` |
| 7 | `thxx_bomb_dec` EHOOK 的**既有 lambda 体内** | `THxxLogger::singleton().OnBomb();` |
| 8 | `thxx_life_dec` EHOOK 的**既有 lambda 体内** | `THxxLogger::singleton().OnMiss();` |
| 9 | `th17_roar_break` EHOOK 的**既有 lambda 体内**（`thprac_th17.cpp:3063`） | `TH17Logger::singleton().OnBreak();`（**仅 th17**；事件 `break` = 撞咆哮） |

### 4.1 逐作差异

| 作 | Logger 类 | `PLAYER_PTR` | 机体表达式（`shot`） | bomb 递增点 | 备注 |
| --- | --- | --- | --- | --- | --- |
| th14 | `TH14Logger` | `0x4DB67C` | `S(IGI_PL_14[static_cast<byte>((*(i32*)0x4f5828))*2 + (*(i32*)0x4f582c)])` | `th14_bomb_dec` `0x41218A` | 采样机制是「指令指针 → sub 名」（`BuildIpMap`/`NameOfIp`），与其余四作不同；`OnStageStart()` 另有 `mIpMapCount/mIpMapTable` 复位 |
| th15 | `TH15Logger` | `0x4E9BB8` | `S(IGI_PL_15[static_cast<byte>(*(i32*)0x4E7404)])` | `th15_bomb_dec` `0x41497A` | mid-boss 收尾状态是 `mMidBossDoneRecord`（记录名）而非 bool |
| th16 | `TH16Logger` | `0x4A6EF8` | `std::format("{}{}", S(IGI_PL_16[*(i32*)0x4A57A4]), S(IGI_PL_16S[*(i32*)0x4A57AC]))` | `th16_bomb_dec` `0x40DB9C` | **唯一双数组拼接**（机体 + 子机），需 `std::format` 临时对象；`Tick` 在同一全表达式内复制完，无悬垂 |
| th17 | `TH17Logger` | `0x4B77D0` | `S(IGI_PL_17[(*(i32*)0x4B59F4))*3 + (*(i32*)0x4B59F8)])` | `th17_bomb_dec` `0x411CAB` | **唯一有第 9 处**：`th17_roar_break` `0x40F880`(10B) 既有 lambda 内 → `OnBreak()`（撞咆哮；同址不新增 EHOOK） |
| th18 | `TH18Logger` | `0x4CF410` | **无**（传 `nullptr`） | `th18_bomb_dec` `0x4574D3` + `th18_cylinder` `0x410F22` | IGI 无 `diff_pl` 行、机体地址未登记；**bomb 有两个递增点，两处都挂了 `OnBomb()`**，否则 JSON 条数会少于 IGI 面板 |

### 4.2 两个必须遵守的接线约束

1. **`Tick` 必须在 `Update()` 函数体的最顶部**，早于所有 `return`。退出一局后关卡号会落到菜单态，而各作 `Update()` 里有 `if (stageNum < 1 || stageNum > 7) return;` —— 落盘与下降沿检测都必须在这类 early return 之前。菜单态下 `thXX_update` EHOOK 照常每帧执行，所以菜单里能安全完成写盘。
2. **第 7/8 项必须写进既有 EHOOK 的 lambda 体内**，绝不能在同地址新增第二个 `EHOOK_DY`：VEH 框架是 `vehHooks[addr] = this`（`thprac_hook.cpp:223`），同址挂第二个会**覆盖**第一个，导致既有的 miss/bomb 计数清零/递增失效。同样不要改既有 EHOOK 的 `instr_size` 参数。（`docs/th16/th16_monitor_impl_report.md:232` 有同样的警告。）

### 4.3 各调用点的上下文安全性

| 调用点 | 上下文 | 是否 VEH | 允许堆分配 | 允许文件 IO |
| --- | --- | --- | --- | --- |
| `OnStageStart()`（`*_game_start` 钩子内） | INT3 + VEH 同步回调，游戏线程 | 是 | **否** | 否 |
| `Update()` 顶部（`Tick`） | 同一个每帧派发 EHOOK 的 VEH 回调 | 是 | 可以（同处已在做 ImGui / `std::format`；先例：`th16_rep_save` 在 VEH 里调 `THSaveReplay` → `ReplaySaveParam` → `CreateFileW`） | **可以**（一局一次） |
| `Update()` 末尾（`Latch`） | 同上 | 是 | 不需要（纯指针比较 + `strcmp`） | 否 |
| `OnMiss()` / `OnBomb()` / `OnBreak()`（`*_life_dec` / `*_bomb_dec` / `th17_roar_break` 钩子内） | INT3 + VEH 回调，事件触发即进 | 是 | **禁止** | **禁止** |

---

## 5. 一局状态机

`GameSession` 内部：`mCur`（当前局）、`mPending` + `mHasPending`（待落盘快照）、`mOpen`、`mWasInGame`、`mGameId`。

| 触发 | 动作 | 分配 |
| --- | --- | --- |
| Logger 首次构造（`THGuiCreate()` 预构造） | 全字段默认 | 堆（既有单例 `new`） |
| **`*_game_start`** → `OnStageStart()` → `Begin("thXX")` | ① 若 `mOpen`：`mPending = mCur; mHasPending = true;`（覆盖「重开 / 直接换局」）② `mCur = {}` ③ `mOpen = true` ④ `mWasInGame = false` | **零** |
| **首次 latch** → `Latch(record)` | `startCheckpoint = endCheckpoint = record`，`checkpointCount = 1` | 零 |
| **后续 latch（记录名变化）** | `endCheckpoint = record`，`++checkpointCount`（同一记录名重复命中不算，用 `strcmp` 比对以容忍 CSV 同名行） | 零 |
| **miss / bomb / break** → `PushEvent(type, mSection)` | `events[eventCount++] = {type, mSection}`；满则 `++droppedEvents` | 零 |
| **`PLAYER_PTR` 由非 0 变 0** → `Tick(false)` | `mPending = mCur; mHasPending = true; mCur = {}; mOpen = false` | 零 |
| **落盘** → `Tick()` 末尾 | 若 `mHasPending`：序列化 `mPending` → 追加写月度文件 → 清空 | **堆 + 文件 IO（一局一次）** |
| **强退进程** | 不落盘（主动放弃） | — |

补充约定：

- **起始时间延迟到本局第一帧取**：`Begin()` 里不调 `GetLocalTime`，保持 `*_game_start` 钩子「零系统调用、零分配」；时间与机体串在 `Tick()` 里 `if (!mCur.hasStart && inGame)` 时取一次，误差 < 1 帧。
- **机体串只在需要时求值**：`Tick` 的 shot 参数由调用方用 `inGame && mSession.NeedsStartInfo()` 守卫，未进入游戏时不读机体地址（菜单里玩家对象不存在，直接读会越界）。
- **`Flush` 跳过没进过游戏的一局**：`if (!data.hasStart) return;`（例如 `game_start` 后立刻回菜单）。
- **`Latch` 只在 `mOpen` 且 `record != nullptr` 时生效**：`GameLogRecordOfSection()` 把 `"main"` 归一成 `nullptr`，所以「回到 main」不会污染 checkpoint 计数。

---

## 6. 构建

- 本机 Windows 11，VS 在 `D:\tools\VS`（不在 PATH）：
  `"D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe" thprac.sln -p:Configuration=Release -p:Platform=x86 -m`
- **Release|x86 编译通过**（2026-10-17）。
- **不需要**改 `thprac/thprac.vcxproj` / `.filters` —— 本迭代没有新增文件（相对 th16 迭代的一处简化）。

---

## 7. 真机验证清单（未做）

1. th16 练习启动一局 → 打几段 → 退出一局回菜单：`<数据目录>logger_sessions_YYYYMM.jsonl` 出现一行，字段与 IGI 面板对得上（`startCheckpoint` = 起始段落，`endCheckpoint` = 退出时段落，`events` 与 miss/bomb 计数一致）。
2. 重开一局：上一局已落盘、新局开始（文件里多一行）。
3. **打完整局从 ST1 到 ST2：确认只产生一行**。若产生两行，说明 `*_game_start` 是「一关」而非「一局」边界（它是「开新局初始化」代码，把 score/graze/power 归零、bomb 置 3；静态上强烈指向「只在新局/重开触发」，但未真机钉死）——此时把 `Begin()` 改成「仅当上一帧不在局内才开新局」一行 guard 即可，其余逻辑不动。
4. 正常退出游戏：最后一局已落盘。
5. th14 / th15 / th17 / th18 各跑一次最小流程（启动 → 打一段 → 退出）：五作行都写进同一文件、`game` 字段正确。
6. 对照 IGI 面板的 miss/bomb 计数与 `events` 条数（**重点 th18 的 cylinder**）。
7. **th17 的 `break`（撞咆哮）**：练习启动一局 → 触发 ≥2 次撞咆哮 + ≥1 miss + ≥1 bomb → 回菜单：`"type":"break"` 条数 == IGI 面板「撞咆哮」计数（`THPRAC_INGAMEINFO_17_ROAR_BREAK_COUNT`）；每条 break 的 `checkpoint` 与触发时刻 IGI「当前段落」一致（允许 §8 #4 的一帧延迟）。顺带确认该局无 `eventsDropped`（撞咆哮与 miss/bomb 共用 256 上限，见 `knownBugs.md` #3）。

---

## 8. 风险与已知边界

| # | 项 | 说明 | 处置 |
| --- | --- | --- | --- |
| 1 | `*_game_start` 是一局还是一关的边界 | 静态证据（score/graze/power 归零）指向「新局」，但未真机钉死。若每关触发，一局会被切成多行 | 验证项 3；修法是一行 guard |
| 2 | 切关/加载时 `PLAYER_PTR` 是否瞬时归零 | 若会，一局会被提前切断 | 各作 IGI 面板用的就是同一判据且长期正常显示，故认为不会；验证项 3 覆盖 |
| 3 | th14 的 latch 可能早一帧 | `mSection` 由「当前指令 IP」反查（IP 指向正在执行的指令），其余四作是线程 sub 索引 | 只影响边界帧归属，不影响 `checkpointCount`；不修 |
| 4 | 事件 checkpoint 有一帧延迟 | miss/bomb 钩子在 ECL tick 中触发，而 `mSection` 是上一帧 `Update()` 的结果；刚好在进入新段落的同一帧 miss 时会记成上一段 | 见 `knownBugs.md` #2（P2） |
| 5 | `events` 上限 256 | 长局 + 关闭 FastRetry 反复 miss 时可超 | 溢出输出 `eventsDropped`；见 `knownBugs.md` #4（P2） |
| 6 | th18 的 `shot` 为空 | 其 IGI 无 `diff_pl` 行，机体地址未登记（总括 §6 Q7 只填了 th14） | 需补逆向；不影响其余字段 |
| 7 | 跨月边界 | 开始与结束跨月时整局落进结束月文件，`start` 仍记开始日 | 可接受，见 §3 |

---

## 9. 明确不做（本迭代）

- **th18 的机体类型**：无现成来源，需新增逆向，不在范围。
- **强退进程的落盘**：用户定案「视为主动放弃记录」。
- **日志文件读回**：本迭代只写不读。
- **th10–th13 / th06–th08**：监控模块尚未落地（th10–th13 白名单未枚举、th06–08 逆向未跑），无从接线。
- **`th16_release` / th18 的 deadbomb**：都不是 bomb 语义（th18 的 deadbomb 已在 IGI 里单独显示在 miss 行），不纳入 `events`。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-17 | 建档。共享模块 `thprac_games.{h,cpp}`（`#pragma region Game Session Log`）+ th14/15/16/17/18 各 8 处接线。口径：JSONL 一行一局、月度文件、一局边界 = `*_game_start` + `PLAYER_PTR` 下降沿、启动 checkpoint = 首次 latch、字段合并为「启动/结束」两个。**Release\|x86 编译通过；真机未测。** |
| 2026-10-17 | **th17 新增 `break` 事件（撞咆哮）**：共享枚举 `GameLogEventType` 增 `Break`（`thprac_games.h`）；序列化由二元三元表达式改为 `GameLogEventTypeName()`（同文件内联；原三元表达式对任何非 `Miss` 值都输出 `"bomb"`，新增枚举值会**静默错标**，改 `switch` 后漏写 case 落到兜底输出显眼的 `"?"`——注意 MSVC 的 C4062 只在 `/Wall` 下报、本工程 `/W4` 不报，**编译器拦不住**，已实测确认；`thprac_games.cpp` 调用点局部变量仍名 `type` 以保 JSON key 不变）；`TH17Logger::OnBreak()` + `th17_roar_break`（`0x40F880`）**既有 lambda 体内**一行 `OnBreak()`（**同址不新增 EHOOK**，`instr_size` 不变）。th14/15/16/18 不受影响（`Miss`/`Bomb` 的输出逐字节不变）。**Release\|x86 编译通过；真机未测。** |

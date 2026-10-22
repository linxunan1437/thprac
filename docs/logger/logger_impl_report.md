# logger 日志记录（JSONL 落盘）— 实施记录（beta）

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志；**本迭代做日志记录**，监控已在前一迭代落地）
> 上游文档：`docs/logger/logger_re_disasm_overview.md`（术语口径，以此为准）、`docs/logger/logger_progress.md`（进度，唯一事实来源）、`docs/th16/th16_monitor_impl_report.md`（监控侧参考实现）
> 白名单数据：`docs/logger/th14..th18_checkpoints.csv`（本迭代消费其第 4 列「记录名」）
> 代码：`thprac/src/thprac/thprac_games.{h,cpp}`（共享会话模块，新增 `#pragma region Game Session Log`）、`thprac/src/thprac/thprac_th14/15/16/17/18.cpp`（各作 Logger 接线）
> 分支 / 提交：`lxn/game-logger`；`feat(logger): 共享一局日志落盘模块`、`beta: logger 日志落盘 th14–th18 接线`
> 状态：**已接线 + 编译通过（Release|x86）**；**五作最小冒烟通过**（2026-10-18，仅确认功能可用；**边界项未逐项验证**）
> 覆盖范围补充（2026-10-22）：**th10–th13 亦已于 2026-10-20 完成监控 + 日志接线**（逐作见 `docs/thXX/thXX_monitor_impl_report.md` §9）；本文正文（§0–§8）仍以 th14–th18 五作为基线，跨作差异表未逐条重写。
> 地址口径：本文不新增任何地址。`PLAYER_PTR` / `STAGE_NUM` / 机体地址一律复用各作 `thprac_thXX.cpp` 顶部的 `enum addrs` 或 IGI 面板既有读法。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 文件格式 | **JSONL**：一行一局，纯追加，不读回改写。**唯一例外**：单章节练习合并会就地改写**最后一行**（口径 11）——仍不读回文件、不跨进程 |
| 文件位置 | `<数据目录>logger_sessions_YYYYMM.jsonl`（每月一个，五作共享，靠 `game` 字段区分） |
| 一局边界 | **`*_game_start`（开新局，含重开）** + **`PLAYER_PTR` 由非 0 变 0（一局结束）** |
| 结束判据来源 | **直接复用各作 IGI 面板的显隐判据**（`thprac_th16.cpp:922-936` 的 `OnPreUpdate` 用的就是 `GetMemContent(PLAYER_PTR)`），不新增任何逆向 |
| 启动 checkpoint | 本局**首次 latch** 到的记录名（练习 warp 时落点即练习选的段落） |
| checkpoint 字段 | 只有「启动」与「结束」两个（需求 §5 的「最后checkpoint」与「结束checkpoint」合并，用户 2026-10-17 定案） |
| 强退进程 | 主动放弃记录，不处理（用户 2026-10-17 定案） |
| 落盘位置 | `GameSession::Tick()` 里，**一局至多一次** |
| 单章节合并 | 同起始 checkpoint 的连续单章节练习局**并入同一行**，就地更新 `total`/`got`（口径 11；**「单章节」判据与 `got` 的定义已由口径 13 修订、子句② 再经口径 16 修订**；**合并行规范化见口径 14**：`checkpointCount` 固定 1、`events` 固定 `[]`）。判定只用共享字段，**五作都生效、默认开启** |
| 代码量 | 共享模块 2 文件（新增 region，不加新文件）；五作各 8 处调用，逐字同构；**th17 另有第 9 处**（`break` / 撞咆哮，非逐字同构）；**五作各另有 9 处**（stageClear / interrupt，口径 12，见 §4.4 的 #1–#9） |
| 工程文件 | **不需要**改 `thprac.vcxproj` / `.filters`（无新文件） |
| 事件类型 | `miss` / `bomb` / `break` / **`interrupt`**（口径 12：未通关就退出游戏；**th14–th18 五作都产出**） |
| stageClear 判据 | 关底 boss 死 / 逃收尾 sub 的**每帧 sub 名采样**（复用 `IsMidBossEndSub` 同一采样点，**零新 hook**）；名字为 `.decl` 全量枚举的精确匹配（含 `BossDeadB` 同名歧义的排除，见 §4.4） |

---

## 1. 架构与分层

| 层 | 文件 | 职责 | 知道什么 |
| --- | --- | --- | --- |
| 共享容器 + 序列化 + 写盘 | `thprac_games.h`（`#pragma region Game Session Log`，插在 `#pragma region Json` 的 endregion 之后）、`thprac_games.cpp`（同 region，插在 `#pragma region Virtual File System` 之前） | `GameLogEventType` / `GameLogEvent` / `GameSessionData` / `GameSession` / `GameLogRecordOfSection()` / `GameSessionWriteResult` / `GameSessionAppendJsonLine()` / `GameSessionRewriteJsonLine()` | 只知道「一局」这个概念，**不知道任何游戏、地址、checkpoint 语义** |
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

单章节练习的合并行（口径 11，判据与 `got` 见口径 13，规范化见口径 14）——同一段连续重开 5 次、其中 3 次无 miss/bomb。合并行**不记 events**（口径 14：`events` 只反映首条、不代表整行，故输出 `[]`）：

```json
{"game":"th16","start":"2026-10-18 14:02:11","shot":"ReimuA","startCheckpoint":"ST3 BOSS2","endCheckpoint":"ST3 BOSS2","checkpointCount":1,"total":5,"got":3,"events":[]}
```

> 合并行的 `endCheckpoint` 只反映**首条**尝试（口径 11 的必然推论）：上例里后 4 次尝试的段落细节不在行里，但计数在 `total`/`got` 里。`events` 与 `checkpointCount` 自口径 14 起**不再反映首条**（`[]` / 固定 1）；`total == 1` 的**首条**记录则仍带真实的 `checkpointCount` 与 `events`。

带 `interrupt` 的一行（th16 练习中途退出，口径 12）：

```json
{"game":"th16","start":"2026-10-18 15:41:02","shot":"ReimuA","startCheckpoint":"ST1 前半 #1","endCheckpoint":"ST1 前半 #2","checkpointCount":2,"events":[{"type":"miss","checkpoint":"ST1 前半 #1"},{"type":"interrupt","checkpoint":"ST1 前半 #2"}]}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `game` | string | `th14`…`th18`。月度文件五作共享，靠它区分 |
| `start` | string | `YYYY-MM-DD HH:MM:SS` 本地时间，**与兄弟文件 `gametime_sessions.jsonl` 完全同格式**（不引入 ISO-8601 `T`，避免同目录两种时间口径） |
| `shot` | string | 机体类型，取该作 IGI 面板 `diff_pl` 行显示的机体串。**th18 留空**（其 IGI 无 `diff_pl` 行、机体地址未登记，见 §9） |
| `startCheckpoint` | string | 本局**首次** latch 到的记录名；一次都没 latch 则 `"main"` |
| `endCheckpoint` | string | 本局**最后一次** latch 到的记录名；同上兜底 `"main"` |
| `checkpointCount` | int | latch 次数（含启动 checkpoint，即首次 latch 记 1）。等价读法：`checkpointCount - 1` = 启动之后又经过了几段。**合并行（`total > 1`）固定为 1**（口径 14） |
| `total` | int | **仅在「单章节」行出现**（`total > 0`）；本行合并进来的尝试次数（口径 11） |
| `got` | int | 与 `total` **成对出现**（判据挂在 `total` 上，`got` 不单独判零）；其中**无 `miss`/`bomb`/`break`** 的次数 —— **`interrupt` 不影响 `got`**（口径 13） |
| `events` | array | `{type, checkpoint}`；`type ∈ {"miss","bomb","break","interrupt"}`（**`break` 仅 th17 产出**，= 撞咆哮；`interrupt` = 未通关就退出游戏，口径 12，**五作都产出**）；`checkpoint` = 事件发生时 IGI 面板显示的段落（未 latch 前为 `"main"`）。**合并行（`total > 1`）固定为 `[]`**（口径 14，键保留） |
| `eventsDropped` | int | **仅在 >0 时出现**；`events` 溢出 256 后被丢弃的条数。**合并行固定不出现**（口径 14 一并归零） |

固定字段顺序（writer 决定）：`game, start, shot, startCheckpoint, endCheckpoint, checkpointCount[, total, got], events[, eventsDropped]`。读取端按 key 取值，不依赖顺序（吸取 `ParseGameSessionLine` 里 `durationNs` 用 `strstr` 定位的教训，`thprac_launcher_others.cpp:171-173`）。

**序列化用 rapidjson 宏而不是 `_snprintf_s` 手拼**：`AppendGameSessionLine` 用的是 256 字节栈缓冲，字段一长就静默截断；本方案走 rapidjson + `std::string`，天然无截断，也免费拿到转义正确性。注意 `AddJsonValue*` 宏用**变量名**当 key（既有写法，见 `thprac_th16.cpp:399-400`），所以函数里的局部变量名必须与字段名一致；字符串值必须带 `jalloc` 第三参（`AddJsonValueEx(game, gameId, jalloc)`），否则会走到只接受字面量的重载而编译失败。

---

## 3. 写盘路径

```cpp
GameSessionWriteResult GameSessionAppendJsonLine(const char* gameId, const GameSessionData& data)
{
    GameSessionWriteResult result;
    auto dir = LauncherGetDataDir();          // 带尾 '\'；空串 ⇒ 不可用
    if (dir.empty()) return result;

    auto line = GameSessionBuildJsonLine(gameId, data);
    if (line.empty()) return result;

    SYSTEMTIME now {};
    GetLocalTime(&now);
    wchar_t suffix[32];
    swprintf_s(suffix, L"logger_sessions_%04u%02u.jsonl", (unsigned)now.wYear, (unsigned)now.wMonth);

    auto hFile = CreateFileW((dir + suffix).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return result;
    defer(CloseHandle(hFile));

    // FILE_APPEND_DATA 恒定写到 EOF ⇒ 写入前的文件长度就是本行行首偏移（口径 11 要用）。
    LARGE_INTEGER size {};
    if (!GetFileSizeEx(hFile, &size)) return result;
    result.offset = (uint64_t)size.QuadPart;

    SetFilePointer(hFile, 0, nullptr, FILE_END);
    DWORD written = 0;
    if (!WriteFile(hFile, line.c_str(), (DWORD)line.size(), &written, nullptr) || written != (DWORD)line.size())
        return result;
    if (!WriteFile(hFile, "\n", 1, &written, nullptr) || written != 1)
        return result;

    result.status = GameSessionWriteResult::Status::Ok;
    result.length = (uint32_t)line.size() + 1; // 含 '\n'
    return result;
}
```

### 3.1 就地改写（口径 11 的唯一例外）

`GameSessionRewriteJsonLine(offset, length, gameId, data)` 只在**单章节练习合并**时调用，把「仍是文件最后一行」的那一段原地重写：

```cpp
auto hFile = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
if (hFile == INVALID_HANDLE_VALUE) return GuardFailed;   // 没动过文件
if (GetFileSizeEx(hFile, &size) != TRUE || size.QuadPart != (LONGLONG)(offset + length))
    return GuardFailed;                                  // 一字节都不写
SetFilePointerEx(hFile, offset, ...);
WriteFile(hFile, line...);  WriteFile(hFile, "\n"...);
SetFilePointerEx(hFile, offset + line.size() + 1, ...);
SetEndOfFile(hFile);                                     // 新行更短时截断；更长时是空操作
```

- **句柄权限必须是 `GENERIC_READ | GENERIC_WRITE`**：`FILE_APPEND_DATA` 的句柄只能写到 EOF，`SetFilePointer` 到中间再写会失败、`SetEndOfFile` 也会 `ACCESS_DENIED`。share mode 与 `SetEndOfFile` 无关。
- **`size == offset + length` 是硬守卫**：并发追加、跨月换文件（月份后缀在改写时刻**重算**）、文件被删/被截断都会让它失败。
- **失败分两类，退化动作不同**（写反了要么丢计数要么重复计数）：

  | 失败点 | 状态 | 文件状态 | 调用方动作 |
  | --- | --- | --- | --- |
  | 目录空 / 行为空 / 打不开 / size 守卫不匹配 | `GuardFailed` | **一字节未动** | 追加**全新**行（`total = 1`），基线改锚 |
  | `SetFilePointerEx` / `WriteFile` / `SetEndOfFile` 失败 | `IoFailed` | 旧行可能已部分覆盖 | 追加**合并后**的行（`total = prev + 1`），把计数补回来 |

- **追加句柄未授予 `FILE_SHARE_WRITE`**，任一进程持有写句柄期间另一进程请求写访问会 `CreateFileW` 失败 ⇒ 句柄本身充当 OS 级互斥，无需额外加锁。

- **写盘骨架照搬** `thprac_launcher_others.cpp:132` 的 `AppendGameSessionLine()`（`FILE_APPEND_DATA` + `OPEN_ALWAYS` + `SetFilePointer(FILE_END)`），不跟着抄它「256 字节栈缓冲」那处隐含截断缺陷。
- **文件名**：`logger_sessions_YYYYMM.jsonl`，与兄弟文件 `gametime_sessions.jsonl` 同族。**五作共享一个文件**，因为需求 §4 说「每一个月一个文件」（单数），且与 gametime 的既有形态一致；要按作分文件只需把前缀换成 `gameId`，一行改动。
- **月份 key** 取**写盘时刻**的 `wYear`/`wMonth`。跨月边界（10-31 开始、11-01 结束）会落进 11 月文件，而 `start` 仍记 10-31 —— 可接受，登记于此。
- **行终止符** `\n`（JSONL 规范）。兄弟文件用 `\r\n`，两者都能被逐行解析。
- **目录不存在**：`%APPDATA%\thprac\` 或 exe 同级 `.thprac_data\` 由 launcher 侧 `LauncherCfgInit(true)`（`main.cpp:169`）创建，注入进程里必然已存在；**不做 `CreateDirectoryW`**（保持「静默失败」的简单语义）。
- **失败全静默**：目录空 / `CreateFileW` 失败 / `WriteFile` 失败一律 `return`，不弹窗、不打日志、不重试。统计功能绝不能干扰游玩。改写路径的退化见 §3.1。
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
| `Update()` 顶部的 `IsGameExitEdge()` + `PushEvent(Interrupt, …)`（五作） | 同一个每帧派发 EHOOK 的 VEH 回调，且早于 `Tick()` | 是 | **禁止**（只做指针比较 + POD 拷贝） | **禁止** |

### 4.4 stageClear / interrupt 接线（口径 12，**th14–th18 五作**）

不动任何 EHOOK。每作 **9 处**，全部落在该作既有的 `THxxLogger::Update()` 内（逐字同构，只差文件号与名字清单）：

| # | 位置 | 改动 |
| --- | --- | --- |
| 1 | 匿名命名空间，`IsMidBossEndSub()` 之后 | `bool IsStageClearSub(const char* name)`（`strcmp` 精确匹配该作自己的名字清单，见 §4.4.1） |
| 2 | `THxxLogger` private 成员区 | `bool mStageClear = false;` |
| 3 | `OnStageStart()` 体末 | `mStageClear = false;`（新局尚未通关） |
| 4 | `Update()` 体首（`inGame` 之后、`Tick()` **之前**） | `if (!mStageClear && mSession.IsGameExitEdge(inGame)) mSession.PushEvent(GameLogEventType::Interrupt, mSection);` |
| 5 | `Update()` 候选变量区 | `bool sawStageClear = false;` |
| 6 | 遍历体内，紧跟 `IsMidBossEndSub` 那两行 | `if (IsStageClearSub(name)) sawStageClear = true;` |
| 7 | `Update()` 关卡切换块（`stageNum != mBuiltStage`） | `mStageClear = false;` |
| 8 | `Update()` 段落裁决之前 | `const char* prevSection = mSection;` |
| 9 | `Update()` 段落裁决之后、`Latch()` 之前 | 段落变化 ⇒ 复位；`sawStageClear` ⇒ 置位（**顺序：复位在前、置位在后**） |

**共享层改动（两处，五作共用）**：`GameLogEventType` 增 `Interrupt`（`thprac_games.h`）+ `GameLogEventTypeName()` 补 `case`；`GameSession` 增 `bool IsGameExitEdge(bool inGame) const`（暴露 Tick 内部那一条下降沿判据）。`thprac_games.cpp` 的合并判据按口径 13/14 处理。

逐作差异只有三处：**名字清单**（§4.4.1）、**th14 的 sub 名来自 `NameOfIp`（IP→名）而非 sub 索引表**（插入点相同）、**th18 的 `Tick()` 无 `shot` 参数**。

#### 4.4.1 逐作名字清单（**必须逐作枚举，不可跨作外推**）

每作的清单都是对该作 `reasm/exes/ecl_dump/thXX/*.decl` **全量** `^void Boss*Dead|Boss*Escape` 枚举后逐名裁决的结果：

| 作 | 收录（`strcmp` 精确匹配） | 关键排除项与理由 |
| --- | --- | --- |
| **th14** | `BossDead`、`BossEscape`、`BossBDead`、`BossBEscape` | `BossBDead`/`BossBEscape` 在 **`st04bs2`**（ST4 B 路线，与 `st04bs` 的 A 路线并列）。**任何 `*mbs*.decl` 里都没有裸 `Boss*Dead\|Escape`** ⇒ th14 **不存在**同名歧义，四个名字精确匹配即可。排除 `TaikoDead*`/`Card01HeadDead`（setDeath/setNext 注册的死亡动画，boss 存活时就跑）。 |
| **th15** | `BossDead`、`BossDead2`、`BossEscape` | **排除 `BossDeadB`（`st07bs2`）**：`st07.decl:968/969` 的两条 `enmCreateA("Boss",…)` / `enmCreateA("BossB",…)` **之间无条件跳转** ⇒ 两个 boss 体**同时在场**，`st07bs2` 是 ST7 战斗的**附属身体**而非独立路线（th15 的 ST7 白名单里也**没有任何 B 路线记录**）；权威收尾是 `st07bs` 的 `BossDead`（已收录）。另 `BossDeadB` 已被 th15 既有的 `IsMidBossEndSub` 认作中boss 收尾。排除 `BossTrase`（无限 `movePos/wait(1)` 死循环，永不 `setBoss(-1)`）。 |
| **th16** | `BossDead`、`BossDead2`、`BossDeadp`、`BossDeadBp`、`BossEscape` | 含 `BossDead2`（`st07bs` Extra）与 `BossDeadp`/`BossDeadBp`（`st05bs` 两条路线；同文件的 `BossDead`/`BossDeadB` 是 **0 帧**转发壳，只有两条 call，60Hz 采样基本抓不到）。**排除 `BossDeadB`**：它在 `st05bs` 是关底收尾、在 **`st07mbs2` 是中boss 收尾**，同名歧义；排除不损失覆盖（`st05bs` 侧由 60 帧的 `BossDeadBp` 兜住）。排除 `BossCard2_Dead`/`BossCard8_Dead`/`BossCard3Et_Dead(Et)`（setDeath 符卡结束动画）。 |
| **th17** | `BossDead`、`BossDead2`、`BossEscape` | 无 B 路线孪生名。排除 `BossPhantom01DeadAt(_at)`/`BossPhantom01_DeadFunc`（`st03bs`）、`BossPhantom09DeadAt(_at,_at2)`/`BossPhantom09_DeadFunc`（`st07bs`）——都是 `setDeath()` 注册的**小怪**死亡动画，最终符卡期间就会跑；排除 `BossCard1_AtEt_Dead`/`BossCard2_AtEt_Dead`（`st05bs` 的掉落分发，0 帧）。 |
| **th18** | `BossDead`、`BossDead2`、`BossEscape` | 无 B 路线孪生名。排除 `BossPhantom01DeadAt(_at)`/`BossPhantom01_DeadFunc`（`st03bs`，同上）。`BossM`/`BossM2`（`st07bs`）是 `st07.decl` 另起的**两个 boss 宿主**，不是收尾，精确匹配天然不命中。 |

**五作共有的两条约定**：

1. **收尾 sub 必须可被 60Hz 采样抓到**：五作的关底收尾 sub 都停在 `+60:` 标签或显式 `wait(60)`/`wait(90)` 上（≥60 帧）。**唯一的 0 帧壳是 th16 `st05bs` 的 `BossDead`/`BossDeadB`**，已由同窗口 60 帧的 `BossDeadp`/`BossDeadBp` 兜底。
2. **`BossEscape`（及 th14–th18 的 `BossDead2`）在 `.decl` 里没有字符串引用**——它们只作为 sub 定义存在（`grep '"BossEscape"' *.decl` 为空），说明触发点在**引擎侧**（超时路径）而非 ECL 注册。**仍然收录**：收录一个不可达的名字是 no-op（一次永不命中的 `strcmp`），漏掉一个可达的名字却会让通关被记成 `interrupt`——风险不对称。

> **不新增任何地址**：全部复用各作既有的宿主遍历（th14 走 `NameOfIp`，th15/16/17 走 sub 索引表，th18 的宿主链头是 `mgr+0x18C`），与 `IsMidBossEndSub` 同一个采样点。**各作的名字清单与复位规则均未真机验证（🟡）**。

#### 4.4.2 spellFlag 补充（口径 15，**th14–th18 五作**）

2026-10-20 落地（先在 th16，随后推广到 th14/15/17/18）：修「某些 `BossCard` checkpoint 在卡结束后不立刻切换、退出时被误记 `interrupt`」。**只关心「符卡结束」，不区分收取/失败**；`mStageClear` **保留为兜底**（不删除）。

下表是 **th16 的落点**，同时作为五作模板；其余四作只是常量与插入行号不同（见 §4.4.2.1），9 处结构逐字同构。

| # | 位置（`thprac_th16.cpp`） | 改动 |
| --- | --- | --- |
| 1 | `enum addrs`（:14） | `SPELL_OBJ_PTR = 0x4a6db0` |
| 2 | 匿名 namespace（:42） | `constexpr uintptr_t kSpellFlagsOff = 0x78;` |
| 3 | `TH16Logger` private 成员（:174） | `int32_t mSpellFlag = 0; uintptr_t mSpellObj = 0; bool mSpellActive = false;` |
| 4 | 私有方法声明（:154） | `void SampleSpellCard();` |
| 5 | `SampleSpellCard()`（`OnBomb()` 之后） | 读 `*(u32*)0x4A6DB0`：空 ⇒ 只清 `mSpellObj/mSpellActive`（**保留 `mSpellFlag`**）；`obj != mSpellObj` ⇒ 三者复位（新对象）；`mSpellActive && !(flags & 1)` ⇒ `++mSpellFlag` |
| 6 | `OnStageStart()`（:238） | 三者复位 |
| 7 | `Update()` 顶部（interrupt 之前，:291） | `SampleSpellCard();` + 条件改 `mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame)` |
| 8 | `Update()` 换关块（:320） | 三者复位（该块在采样**之后**执行，可抹掉换关的假下降沿） |
| 9 | `Update()` 段落变化块（:445） | `if (mSpellFlag > 0) --mSpellFlag;`（在 `mStageClear = false` 之前） |

**顺序约束（三条都不能反）**：

1. `SampleSpellCard()` 在 interrupt 判定**之前** ⇒ 同帧「清卡 + 退出」也能吃到 `++`。
2. `++` 在 `Update()` 顶部、`--` 在段落裁决之后 ⇒ 同帧「卡结束 + 新 checkpoint」净 0；反过来会残留 1，吞掉之后整关的 `interrupt`。
3. 换关复位在采样之后 ⇒ 换关/对象销毁的假下降沿当场抹掉；对象为空时**不清 `mSpellFlag`**，否则退出帧没用上就先被清了。

**静态证据**：

- `obj+0x78` bit0 的置位**只有** `0x417F76`（`or eax,3`，`FUN_00417F00`），清位**只有** `0x41832F`（`and [esi+0x78],~1`，`FUN_004182F0` = `spellEnd`）。
- `spell`/`0x21A`/`spell3`/`spellEx` 四个 opcode 全部走 `0x421757` → `0x42182A mov ecx,[0x4a6db0]` → `0x42183F call FUN_00417F00` ⇒ **一个对象、一个槽**，中boss 与 Extra 同样覆盖。
- **一帧一采样不会漏**：th16 的 `setNext` 链是「非符 → 符卡 → 非符 → 符卡」交替（`st01bs`–`st07bs`、`st03mbs`、`st07mbs`、`st07mbs2` 共 10 份 decl 静态核对，**无 card→card**）⇒ 每张卡结束后 bit0 至少保持 0 到下一个非符结束（1500–5400 帧）。
- **保留 `mStageClear` 的两处兜底**：关底 `BossDead` 是 `+60:` 才 `spellEnd()`（爆炸后 60 帧内无下降沿，ST6 因 `gameSpeed(0.5)` 约 2 s）；`BossEscape` 由引擎在 boss 宿主创建时注册，可在**非符期**触发（`spellEnd` 为 no-op，无下降沿）。

**未做**：本机（macOS）无编译环境，**未编译、未真机**（🟡）；验证项见 `docs/th16/th16_spell_flag_report.md` §8.4 与本文件 §7。

#### 4.4.2.1 逐作常量与证据（**不可跨作外推**）

| 作 | 对象全局 | 存活位 | 大小 | 置位点 | 清位点 | 轮询 / 备注 |
| --- | --- | --- | --- | --- | --- | --- |
| **th14** | `0x4DB534` | **`+0x7C` bit0** | `0xC0` | `0x41CBE7` | `0x41D492`（唯一） | 唯一真实 card→card = `st05bs BossCard3→BossCard4`，但 `BossCard4` 声明前有 `wait(160)` ⇒ 不漏。`+0x78` 是 spell id |
| **th15** | `0x4E9A70` | `+0x78` bit0 | `0xBC` | `0x42047F` | `0x420D62` + **`0x420EDA`** | card→card = 0；第二清位点（重开/载入强制清卡）可能有假下降沿，靠对象指针 / 换关 / 新局复位吸收 |
| **th16** | `0x4A6DB0` | `+0x78` bit0 | `0xBC` | `0x417F76` | `0x41832F`（唯一） | card→card = 0（`BossCard1Hide` 是无 `spell()` 的辅助 sub，不计） |
| **th17** | `0x4B7690`（既有 `SPELLCARD_PTR`） | `+0x78` bit0 | `0xC0` | `0x41AD58` | `0x41B181`（唯一） | card→card = 0；**`+0x1C` 是 ANM 句柄，不能当存活位** |
| **th18** | `0x4CF2C0` | `+0x78` bit0 | `0xC0` | `0x42A388` | `0x42A7C2`（唯一） | card→card = 0；无 eclmap，opcode 用 `ins_NNN` 锚点 + 分发表 `0x4366F4`/`0x436438` 双重验证 |

**五作共同点**：

- 声明（`spell`/`spell3`/`spellEx`）置位、`spellEnd()` 清位；`spellEnd()` 结算**上一张**卡、无卡时 no-op。
- 每张卡后必接一个非符攻击阶段 ⇒ 一帧一次轮询不会漏（th14 的唯一例外也有 160 帧过渡）。
- `mStageClear` 全部保留：关底 `BossDead` 的 `+60` 帧窗口、非符期 `BossEscape` 都没有卡结束事件。
- 覆盖洞（不产生事件、也不在 CSV）：th15 `st02mbs MBossCard1`（无 `spell()`）、th15 `st07bs2 BossCardB10` / th16 `st07mbs2 MBossBCard*` / th16 `st05bs BossCard3Bp`（仅 `spellMode`）。th14 / th17 / th18 的关底末阶段全部是符卡。
- 五作均**未编译、未真机**（🟡），逐作验证项见各作 `docs/thXX/thXX_spell_flag_report.md`。


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
| **关底 boss 死 / 逃收尾 sub**（口径 12，五作） | `mStageClear = true`（`Update()` 段落裁决之后） | 零 |
| **段落入口函数切换 / 换关 / 新局**（口径 12） | `mStageClear = false` | 零 |
| **符卡结束（bit0 1→0）**（口径 15，th14–th18 五作） | `mSpellFlag++`（`Update()` 顶部，interrupt 判定之前） | 零 |
| **新 checkpoint**（口径 15，th14–th18 五作） | `mSpellFlag--`（下限 0，`Update()` 段落裁决之后） | 零 |
| **`PLAYER_PTR` 由非 0 变 0** → 先 `IsGameExitEdge` + `PushEvent(Interrupt, mSection)`（五作），再 `Tick(false)` | `events[]` 追加一条 `interrupt`；然后 `mPending = mCur; mHasPending = true; mCur = {}; mOpen = false` | 零 |
| **落盘** → `Tick()` 末尾 | 若 `mHasPending`：`Flush(mPending)` → `MergeOrAppend()` —— 单章节且可合并则**就地改写上一行**，否则追加新行 → 清空 | **堆 + 文件 IO（一局一次）** |
| **强退进程** | 不落盘（主动放弃） | — |

补充约定：

- **起始时间延迟到本局第一帧取**：`Begin()` 里不调 `GetLocalTime`，保持 `*_game_start` 钩子「零系统调用、零分配」；时间与机体串在 `Tick()` 里 `if (!mCur.hasStart && inGame)` 时取一次，误差 < 1 帧。
- **机体串只在需要时求值**：`Tick` 的 shot 参数由调用方用 `inGame && mSession.NeedsStartInfo()` 守卫，未进入游戏时不读机体地址（菜单里玩家对象不存在，直接读会越界）。
- **`Flush` 跳过没进过游戏的一局**：`if (!data.hasStart) return;`（例如 `game_start` 后立刻回菜单）。
- **`Latch` 只在 `mOpen` 且 `record != nullptr` 时生效**：`GameLogRecordOfSection()` 把 `"main"` 归一成 `nullptr`，所以「回到 main」不会污染 checkpoint 计数。

### 5.1 单章节合并基线的状态迁移（口径 11 / 13）

「单章节」判据与 `got` 的定义见口径 13（2026-10-18 修订，取代口径 11 原判据），**子句② 已由口径 16（2026-10-20）修订**：判据 = `(cpCount==2 && 含interrupt) || (cpCount==1 && 不是「只有interrupt」)`，后者即 `cpCount==1 && (含miss/bomb/break || eventCount == 0)`；`got` 只数「无 miss/bomb/break」的尝试。实现是 `thprac_games.cpp` 匿名命名空间的三个助手 `GameSessionHasAttemptEvent()` / `GameSessionHasInterruptEvent()` / `GameSessionOnlyInterruptEvents()`。

`mMergeBase`（`GameSession` 私有成员）= 上一次写入的那一行。**纯内存态，进程重启即失效**（口径 11）；`data` 保留**首条**记录的全部字段（就地改写时只覆盖 `total`/`got`）。

| 触发 | 动作 |
| --- | --- |
| 写入的是一条**非单章节**记录 | 追加后 `valid = false`（基线作废） |
| 写入的是单章节记录，但基线无效 / 起始 checkpoint 不同 / 不同日 / 跨月 | 追加**新行**（`total = 1`、`got = (无 miss/bomb/break ? 1 : 0)`），基线**改锚**到该行 |
| 写入的是单章节记录，且基线有效 + 同月 + 同日 + 同起始 checkpoint | **就地改写**上一行：`total = prev + 1`、`got = prev + (无 miss/bomb/break ? 1 : 0)`；并做**口径 14 规范化**（`checkpointCount = 1`、`eventCount = 0`、`droppedEvents = 0`）。其余字段（`start`/`shot`/`startCheckpoint`/`endCheckpoint`）不动。基线只更新 `total`/`got`/`length` |
| 改写返回 `IoFailed` | 追加**合并后**的行，基线改锚到该行；追加也失败则 `valid = false` |
| 追加本身失败（目录空 / 打不开） | `valid = false`，绝不留下脏 `offset` |
| 进程重启 | 天然失效（成员在 `GameSession` 里，随进程消失） |

**`Begin()` 不重置基线**：每次重开都会走 `Begin()`，在那里重置等于禁用合并。

判定助手（`thprac_games.cpp` 的匿名命名空间，只看共享字段、与具体游戏无关）：`GameSessionIsSingleChapter()`、`GameSessionSameDay()`（只比 y/m/d）、`GameSessionSameStartCheckpoint()`（nullptr 归一成 `"main"` 后 `strcmp`）、`GameSessionMonthKey()`（`年*12+月`）。

---

## 6. 构建

- 本机 Windows 11，VS 在 `D:\tools\VS`（不在 PATH）：
  `"D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe" thprac.sln -p:Configuration=Release -p:Platform=x86 -m`
- **Release|x86 编译通过**（2026-10-17）。
- **不需要**改 `thprac/thprac.vcxproj` / `.filters` —— 本迭代没有新增文件（相对 th16 迭代的一处简化）。

---

## 7. 真机验证清单（**仅最小冒烟通过**）

> 2026-10-18：th14–th18 五作各做了一次**最小冒烟**（练习启动 → 确认功能可用），结论是功能可用。**下列各项边界未逐项验证**，因此 8–11 条（单章节合并相关）仍按**未逐项验证**处理，不写成已通过。
>
> 2026-10-20：**口径 16 的合并修复真机验证通过** —— 同日、同起始 checkpoint、一条有事件一条零事件的两局，**合并为 1 行**（改前会各占一行）。此项通过；其余各项仍按**未逐项验证**处理。

1. th16 练习启动一局 → 打几段 → 退出一局回菜单：`<数据目录>logger_sessions_YYYYMM.jsonl` 出现一行，字段与 IGI 面板对得上（`startCheckpoint` = 起始段落，`endCheckpoint` = 退出时段落，`events` 与 miss/bomb 计数一致）。
2. 重开一局：上一局已落盘、新局开始（文件里多一行）。
3. **打完整局从 ST1 到 ST2：确认只产生一行**。若产生两行，说明 `*_game_start` 是「一关」而非「一局」边界（它是「开新局初始化」代码，把 score/graze/power 归零、bomb 置 3；静态上强烈指向「只在新局/重开触发」，但未真机钉死）——此时把 `Begin()` 改成「仅当上一帧不在局内才开新局」一行 guard 即可，其余逻辑不动。
4. 正常退出游戏：最后一局已落盘。
5. th14 / th15 / th17 / th18 各跑一次最小流程（启动 → 打一段 → 退出）：五作行都写进同一文件、`game` 字段正确。
6. 对照 IGI 面板的 miss/bomb 计数与 `events` 条数（**重点 th18 的 cylinder**）。
7. **th17 的 `break`（撞咆哮）**：练习启动一局 → 触发 ≥2 次撞咆哮 + ≥1 miss + ≥1 bomb → 回菜单：`"type":"break"` 条数 == IGI 面板「撞咆哮」计数（`THPRAC_INGAMEINFO_17_ROAR_BREAK_COUNT`）；每条 break 的 `checkpoint` 与触发时刻 IGI「当前段落」一致（允许 §8 #4 的一帧延迟）。顺带确认该局无 `eventsDropped`（撞咆哮与 miss/bomb 共用 256 上限，见 `knownBugs.md` #3）。
8. **单章节合并（口径 11 / 13 / 14）**：练习 warp 到单个 section，连续重开 5 次（其中 3 次无 miss/bomb）→ 月度文件里**只有 1 行**，`total == 5`、`got == 3`（`interrupt` 不影响 `got`），且这 5 次之间**没有被追加新行**。注意：① 每次尝试都可能带一条 `interrupt`（口径 12；但口径 15 的 `spellFlag` 会抑制它），判据靠的是 `cpCount==1 && 不是「只有interrupt」`（口径 16：**零事件也算**）或 `cpCount==2 && 含interrupt`；② 合并行**规范化为 `checkpointCount == 1` 且 `"events":[]`**（口径 14）；③ 重开 1 次（`total == 1`）时**不**规范化，仍带真实的 `checkpointCount` 与 `events`。
9. **合并的边界**：接上一条，换一个 section 再练 → **新起一行**；打一次全关（非单章节）→ 追加新行且**不带** `total`/`got`；把系统日期改到次日再练同一 section → 新起一行。
10. **就地改写不破坏文件**：验证期间用 `tail` 确认文件**没有残留半行**（旧行更长时被 `SetEndOfFile` 截断干净），且此前已写入的行**一个字节都没变**。
11. **五作同构**：th14/15/17/18 各做一次单章节练习（连续重开 ≥3 次），确认四作**同样合并**（各留 1 行、`total` 递增）——判定只看共享字段，与具体游戏无关。**注意口径 13 之后四作有一处行为变化**（`cpCount==2` 的干净局不再合并），见验证项 21。

**以下为口径 12（stageClear / interrupt，**th14–th18 五作**）的验证项，2026-10-18 加入，**全部未真机验证**：**

12. **中途退出算 interrupt**：练习 warp 到某关道中段 → 不进 boss → 退到标题。预期 `events` 含 `{"type":"interrupt","checkpoint":"<退出时段落>"}`，且 `checkpoint` 与 IGI 面板「当前段落」一致（允许既有的一帧延迟，`knownBugs.md` #2）。
13. **击破关底不算 interrupt**：warp 到关底 boss → 击破 → 等结算 → 退到标题。预期 `events` 里**没有** `interrupt`。
14. **关底逃逸不算 interrupt**：同上但拖到超时（`BossEscape`）。预期同样**没有** `interrupt`。
15. **中boss 不算通关**：warp 到 `MID1` → 击破中boss → 退出。预期**有** `interrupt`（`MBossDead` 不置位）。
16. **th14 ST4 两条路线都算通关**（`BossBDead` / `BossBEscape`，`st04bs2`）：分别走 A/B 路线击破 → 退出。预期**都无** `interrupt`。
17. **th16 ST5 两条路线都算通关**（`BossDeadp` / `BossDeadBp`）：分别走 A/B 路线击破 → 退出。预期**都无** `interrupt`。**这条最容易做错**（0 帧的 `BossDead`/`BossDeadB` 采样不到）。
18. **th16/th17/th18 ST7 Extra 算通关**（`BossDead2`）：击破 Extra 关底 → 退出。预期**无** `interrupt`。
19. **th16 ST7 中boss B 路线不算通关**（`BossDeadB` 反例）：warp 到 `ST7 MID1` → 走 B 路线击破（`st07mbs2.BossDeadB`）→ 退出。预期**有** `interrupt`。**这条是 th16 `BossDeadB` 排除规则的专用反例**。
20. **th15 ST7 的 `BossDeadB` 判定**（本次唯一未能静态钉死的一处）：`st07.decl:968/969` 的两条 `enmCreateA("Boss","BossB")` 之间无条件跳转 ⇒ 静态判断两个 boss 体**同时在场**、`BossDeadB` 结束的是附属身体，故未收录。真机确认：**清完 ST7 后退出，预期没有 `interrupt`**（若出现，说明 `BossB` 是独立路线，需要把 `BossDeadB` 收进 th15 的清单）。
21. **换关复位**：全关从 ST1 打到 ST2，在 ST2 中途退出。预期**有** `interrupt`（ST1 已清但 ST2 未清 ⇒ 证明换关/段落入口复位生效）。
22. **口径 13 / 16 判据**：① `cpCount==1` 且只有 `interrupt`（warp 进去没 miss 就退）⇒ **另起一行**，不并入（口径 13 原意保留）；② `cpCount==1` 且**零事件**（干净退出、`interrupt` 被 `spellFlag` 抑制）⇒ **并入**（口径 16）；③ `cpCount==2` 且含 `interrupt` ⇒ 并入；④ `got` 只数「无 miss/bomb/break」的尝试 —— 一个干净但被打断的尝试**仍计入 `got`**。
23. **序列化自检**：确认 JSON 里出现的是 `"interrupt"` 而不是 `"?"`（`GameLogEventTypeName()` 漏 case 时 `/W4` 不报 C4062，只会静默输出兜底值）。

> 上面 12–18 需在**每一作**各跑一遍（名字清单逐作不同）；20 只在 th15 跑。

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
| 8 | 合并行的 `endCheckpoint` 只反映**首条**尝试 | 口径 11 的必然推论：后 N−1 次尝试的结束段落不在行里，细节只在 `total`/`got` 里 | 定案，不修（`events`/`checkpointCount` 已由口径 14 改为不再反映首条）；已在 §2 与 `req_logger.md` 口径 11/14 写明 |
| 9 | 改写 `IoFailed` 时的撕裂行 | `SetFilePointerEx`/`WriteFile`/`SetEndOfFile` 失败时旧行可能被部分覆盖，随后追加会产出坏行 | 只在磁盘满 / IO 错误时出现；「统计绝不干扰游玩」优先，静默接受。见 `knownBugs.md` #6 |
| 10 | 并发写者 | 另一游戏进程在同一文件上追加会让 `size != offset + length` | 优雅降级为追加新行（`GuardFailed`）；追加句柄未授予 `FILE_SHARE_WRITE`，句柄本身即 OS 级互斥 |
| 11 | 合并基线不跨进程 | 关掉游戏重新启动后，同日同 section 再练会另起一行（口径 11：不读回文件最后一行） | 定案，不修。见 `knownBugs.md` #7 |
| 12 | 各作 `IsStageClearSub` 的名字清单未真机验证 | 清单来自各作 `.decl` 全量枚举（静态完备），但「每帧采样能否稳定采到」只有真机能证；th16 的 `BossDead`（st05bs）是 0 帧转发壳，靠 60 帧的 `BossDeadp`/`BossDeadBp` 兜底。另 `BossEscape`/`BossDead2` 在 `.decl` 里无字符串引用（触发点在引擎侧），可达性未证 | 验证项 12–18 每作各跑一遍；§4.4.1 的表标 🟡 |
| 13 | 关底收尾后 `mSection` 回退导致误复位 | boss 宿主 `delete()` 之后，若仍有白名单命中的宿主 / 异步线程存活，`mSection` 会从 `"STn BOSSx"` 回退到章节记录 ⇒ 触发一次「段落入口切换」复位 ⇒ 清关后立刻退到标题会误报 `interrupt` | 五作静态均不成立：boss 战期间主线程停在**不在白名单**的 sub（th16/th18 = `MainBoss`；th14/th15/th17 = `main`/`MainBoss` 均不在 CSV），且各作 `.decl` 的 `async` 派生体都不在白名单（th14 `@BossItemPhase`、th15 `@BossN_pos`/`@BossTrase`、th16 `@MainSub05` 约 700 帧即结束、th17 仅 ST3 `@MainSub01` 且在前半段结束）。真机若复现，最小修法是「收尾后 N 帧内忽略复位」。见 `knownBugs.md` |
| 16 | th15 的 `BossDeadB` 是否该收录（**唯一未能静态钉死的一处**） | `st07bs2.BossDeadB` 的 body 与 `st07bs.BossDead` **形状完全一致**（`enmKillAll()` → `+60:` → `setBoss(-1)` → `delete()×2`）。静态判断为「附属身体收尾」（`st07.decl:968/969` 两条 `enmCreateA("Boss","BossB")` 之间无条件跳转 ⇒ 两个 boss 体同时在场；th15 的 ST7 白名单也没有任何 B 路线记录），故未收录；权威收尾 `st07bs.BossDead` 已收录 | 验证项 20（th15 真机跑一次 ST7 通关）。若清 ST7 后出现 `interrupt`，把 `BossDeadB` 加进 th15 的清单即可 |
| 14 | `interrupt` 对单章节判定的影响 | 口径 11 的 `got` 与「单章节」判定原本都拿 `eventCount` 当「有 miss/bomb」的代理；口径 12 引入 `interrupt` 后该代理失真（单章节练习局按定义不推进到关底 boss ⇒ 必带一条 `interrupt` ⇒ `got` 被打成恒 0、`eventCount == 0` 分支变死代码） | **已由口径 13 收口**（2026-10-18），**子句② 再经口径 16 修订**（2026-10-20）：判据 = `(cpCount==2 && 含interrupt) \|\| (cpCount==1 && 不是「只有interrupt」)`，`got` 只数「无 miss/bomb/break」的尝试。**口径 13 遗留的代价**：th14/15/17/18 的 `cpCount==2` 干净局不再合并（定案接受，验证项 21）——口径 16 只动子句②，这条不变。见 `knownBugs.md` #10 |
| 15 | 合并行不记 `events` | 口径 14 定案：合并行的 `events` 原本只反映**首条**尝试、不代表整行，故固定输出 `[]`。代价是合并行看不出任何 miss/bomb/break/interrupt 明细（只有 `total`/`got`） | 定案，不修。要明细请把 `total` 拆开看单条记录 |

---

## 9. 明确不做（本迭代）

- **th18 的机体类型**：无现成来源，需新增逆向，不在范围。
- **强退进程的落盘**：用户定案「视为主动放弃记录」。
- **日志文件读回**：本迭代只写不读（单章节合并的就地改写是「改自己刚写的那一行」，不涉及解析既有行，也不跨进程）。
- **th06–th08**：监控模块尚未落地（E1 已完成：th07/th08 2026-10-19、th06 2026-10-22；E2–E5 未做），无从接线。**th10–th13 已于 2026-10-20 完成监控 + 日志接线**（与 th14–th18 同构 8 处调用，逐作见 `docs/thXX/thXX_monitor_impl_report.md` §9 与 `logger_progress.md` §1），**不属本清单**。
- **`th16_release` / th18 的 deadbomb**：都不是 bomb 语义（th18 的 deadbomb 已在 IGI 里单独显示在 miss 行），不纳入 `events`。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-17 | 建档。共享模块 `thprac_games.{h,cpp}`（`#pragma region Game Session Log`）+ th14/15/16/17/18 各 8 处接线。口径：JSONL 一行一局、月度文件、一局边界 = `*_game_start` + `PLAYER_PTR` 下降沿、启动 checkpoint = 首次 latch、字段合并为「启动/结束」两个。**Release\|x86 编译通过；真机未测。** |
| 2026-10-17 | **th17 新增 `break` 事件（撞咆哮）**：共享枚举 `GameLogEventType` 增 `Break`（`thprac_games.h`）；序列化由二元三元表达式改为 `GameLogEventTypeName()`（同文件内联；原三元表达式对任何非 `Miss` 值都输出 `"bomb"`，新增枚举值会**静默错标**，改 `switch` 后漏写 case 落到兜底输出显眼的 `"?"`——注意 MSVC 的 C4062 只在 `/Wall` 下报、本工程 `/W4` 不报，**编译器拦不住**，已实测确认；`thprac_games.cpp` 调用点局部变量仍名 `type` 以保 JSON key 不变）；`TH17Logger::OnBreak()` + `th17_roar_break`（`0x40F880`）**既有 lambda 体内**一行 `OnBreak()`（**同址不新增 EHOOK**，`instr_size` 不变）。th14/15/16/18 不受影响（`Miss`/`Bomb` 的输出逐字节不变）。**Release\|x86 编译通过；真机未测。** |
| 2026-10-18 | **口径 12 推广到 th14/15/17/18（五作齐）**：每作逐名枚举 `reasm/exes/ecl_dump/thXX/*.decl` 得到自己的关底收尾 sub 清单（**不可跨作外推**），各作 9 处同构接线。清单：th14 = `BossDead`/`BossEscape`/`BossBDead`/`BossBEscape`；th15 = `BossDead`/`BossDead2`/`BossEscape`（排除 `BossDeadB`）；th16 = `BossDead`/`BossDead2`/`BossDeadp`/`BossDeadBp`/`BossEscape`；th17、th18 = `BossDead`/`BossDead2`/`BossEscape`。**共享层零改动**（枚举值 + `IsGameExitEdge()` 上一版已就位）。§0/§4.3/§4.4（新增 §4.4.1 逐作清单表）/§5/§7/§8 同步。**Release\|x86 编译通过；五作真机均未测（🟡）**。 |
| 2026-10-18 | **合并行规范化（口径 14）**：`MergeOrAppend()` 的「就地改写」分支在算出 `total`/`got` 后，把合并行（`total > 1`）的 `checkpointCount` 置 1、`eventCount` 置 0、`droppedEvents` 置 0 ⇒ JSON 里 `checkpointCount` 固定 1、`events` 固定 `[]`（键保留）。**首条（`total == 1`）不规范化**，保留真实的 `checkpointCount` 与 `events`。理由是合并行的 `events` 只反映首条、不代表整行（口径 11），与其误导不如不记。**Release\|x86 编译通过；真机未测。** |
| 2026-10-18 | **单章节判据修订 + `got` 无视 interrupt（口径 13）**：口径 12 的 `interrupt` 让 th16 每个练习局都带一条事件 ⇒ 口径 11 原判据的 `eventCount == 0` 分支变死代码、`got` 被打成恒 0。改为「单章节」= `(cpCount==2 && 含interrupt) \|\| (cpCount==1 && 含miss/bomb/break)`，`got` = 「无 miss/bomb/break」的尝试数（`thprac_games.cpp` 匿名命名空间新增 `GameSessionHasAttemptEvent()` / `GameSessionHasInterruptEvent()`，`GameSessionIsSingleChapter()` 与 `MergeOrAppend()` 的两处 `got` 改用它们）。**代价（定案接受）**：不产出 `interrupt` 的 th14/15/17/18，其 `cpCount==2` 的干净练习局**不再合并**。**Release\|x86 编译通过；真机未测。** |
| 2026-10-18 | **stageClear + interrupt（口径 12，仅 th16）**：共享层 `GameLogEventType` 增 `Interrupt`、`GameLogEventTypeName()` 补 case、`GameSession` 增 `IsGameExitEdge()`（暴露 Tick 内部那条下降沿判据）；`thprac_th16.cpp` 增 `IsStageClearSub()`（5 个名字的 `strcmp` 精确匹配）+ `mStageClear` + 9 处接线（§4.4）。**不新增任何 EHOOK、不新增任何地址**；`thprac_games.cpp` 零改动（合并判据按定案保持原样）。**Release\|x86 编译通过；真机未测（🟡）**。 |
| 2026-10-18 | **单章节练习合并落盘（口径 11）**：`GameSessionData` 增 `total`/`got`（`total == 0` 时整对从 JSON 省略）；`GameSession` 增私有 `MergeBase` 与 `MergeOrAppend()`，`Flush()` 改走它；`GameSessionAppendJsonLine()` 改回传 `GameSessionWriteResult`（含行偏移/长度），新增 `GameSessionRewriteJsonLine()`（`GENERIC_READ\|GENERIC_WRITE` + `size == offset+length` 守卫 + `SetEndOfFile`，`GuardFailed`/`IoFailed` 二分退化）。**合并逻辑全在共享层、默认开启，th14/15/16/17/18 五个 `.cpp` 零改动**（判定只用 `checkpointCount`/`eventCount`/起始 checkpoint/日期，与具体游戏无关）。非单章节记录的输出**逐字节不变**。**Release\|x86 编译通过、`thprac_games.cpp` 零警告；五作最小冒烟通过（仅确认功能可用，边界项未逐项验证）。** |
| 2026-10-20 | **th16 新增 spellFlag（口径 15，仅 th16）**：修「`BossCard` checkpoint 在卡结束后不立刻切换 ⇒ 退出被误记 interrupt」。`thprac_th16.cpp` 增 `SPELL_OBJ_PTR = 0x4a6db0` / `kSpellFlagsOff = 0x78` / `SampleSpellCard()` / `mSpellFlag` / `mSpellObj` / `mSpellActive`，9 处接线见 §4.4.2；`mStageClear` **保留为兜底**（`BossDead+60` 窗口、非符期 `BossEscape`），interrupt 条件改为 `mSpellFlag <= 0 && !mStageClear`。检测复用每帧 `Update()`，**零新 hook、共享层零改动、无新事件类型**。静态核对 `setNext` 链（10 份 decl）无 card→card。**本机无编译环境：未编译、未真机（🟡）**。 |
| 2026-10-20 | **spellFlag 推广到 th14/15/17/18（五作齐）**：逐作新增 `SampleSpellCard()` + `mSpellFlag/mSpellObj/mSpellActive`，9 处接线与 th16 逐字同构；常量逐作独立（§4.4.2.1）：th14 `0x4DB534+0x7C`、th15 `0x4E9A70+0x78`、th17 复用 `SPELLCARD_PTR=0x4B7690` + `+0x78`、th18 `0x4CF2C0+0x78`。`mStageClear` 五作均保留。th15 有第二清位点 `0x420EDA`（重开/载入），靠对象指针 / 换关 / 新局复位吸收。静态核对：th14 唯一 card→card 有 160 帧过渡；th15/17/18 均 0。逐作报告：`docs/th14|th15|th17|th18/thXX_spell_flag_report.md`。**本机无编译环境：未编译、未真机（🟡）**。 |
| 2026-10-20 | **单章节子句② 修订（口径 16）**：口径 13 的子句② 由 `cpCount==1 && 含miss/bomb/break` 改为 `cpCount==1 && 不是「只有interrupt」`（即 `cpCount==1 && (含miss/bomb/break \|\| eventCount == 0)`）。起因：口径 15 的 `spellFlag` 抑制 `interrupt` ⇒ 零事件的 `cpCount==1` 可达，旧判据把它打成「非单章节」并**作废合并基线**（`MergeOrAppend()` 分支 1），同日同起始 checkpoint 的连续练习漏合并。`thprac_games.cpp` 新增 `GameSessionOnlyInterruptEvents()`，`GameSessionIsSingleChapter()` 子句② 改用它；子句① 与「只有 interrupt ⇒ 不算」不变。**共享层改动 ⇒ 五作都生效**。**Release\|x86 编译通过；真机验证通过（2026-10-20）**。 |
| 2026-10-22 | **§9 缺口行订正**：「th10–th13 / th06–th08 监控模块尚未落地（th10–th13 白名单未枚举、th06–08 逆向未跑）」改为「th10–th13 白名单已枚举但 E5 未实现；**th06–th08 E1 逆向已完成**（th07/th08 2026-10-19、th06 2026-10-22）、E2–E5 未做」。**实现与地址一字未改**（本作无实现，仅同步逆向进度）。 |
| 2026-10-22 | **§9 缺口行再订正**：上一条仍把 **th10–th13** 列为「E5 未实现」——与代码不符（四作 2026-10-20 已落地 `THxxLogger` + 8 处日志接线，见各作 `_monitor_impl_report.md` §9）。§9 现只保留 **th06–th08**（E1 已完成、E2–E5 未做），并注明 th10–th13 不属「明确不做」；头部补覆盖范围注记。**未改任何地址与实现描述。** |

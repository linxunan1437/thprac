# TH15 / TH14 miss/bomb 记录功能：实现总结

> **状态：实现代码已弃用。** 原分支 `feat/deadpointLog`（提交 `2a3f4bd` / `63dedb3` / `15a6d85`）的可维护性较差，且存在偶发致命崩溃，故不合并代码，仅保留本文档与相关设计/反编译资料，供后续重做时参考。

## 1. 功能目标

掉命（miss）或放雷（bomb）时，记录**当时所在关卡号 + boss 当前阶段名**，按“一局一行”追加写入游戏目录：

- TH15 → `th15_missbomb.txt`
- TH14 → `th14_missbomb.txt`（仅 ST1~ST6）

同时在 IGI（游戏内信息面板）显示**最近 5 条** miss/bomb 事件。仅 thprac 练习模式记录，高级选项开关 `th15_record_missbomb` / `th14_record_missbomb`（默认开启）。

落盘格式示例：

```
# th15 miss/bomb record (M=miss, B=bomb)
[2026-08-18 20:15:03] ST1 m(Boss1) b(Boss1) ST1 m(BossCard1)
[2026-08-18 20:16:22] ST2 m(BossCard2) ST2 b(-)
```

每条事件为 `ST<关卡> m(<阶段名>)/b(<阶段名>)`；非 boss 段或无法判定时记 `-`。`BossN`=第 N 个非符，`BossCardN`=第 N 张符卡。

## 2. TH15 实现

核心类 `TH15MissBombRecorder`（原 `thprac_th15.cpp`）：

- 状态：`mLineOpen` / `mStartTime` / `mBuffer` / `mRecentMisses`（最近 5 条，`std::deque`）；文件 `th15_missbomb.txt`。
- `OnStageStart()` 开新行；`Record(kind, stage, name)` 追加事件并更新最近 5 条；`OnRunEnd()` 追加落盘（文件不存在时先写表头）。

钩子接入：

| 地址 | 名称 | 作用 |
| --- | --- | --- |
| `0x456398` | `th15_life_dec` | miss 记录点 → `Record('m', ...)` |
| `0x41497A` | `th15_bomb_dec` | bomb 记录点 → `Record('b', ...)` |
| `0x43E6EE` | `th15_game_start` | 新一局边界；清 `g_boss_ptr` / `g_cur_stage_name` / `g_boss_enter_count` |
| `0x42C738` | `th15_lock_timer2` | 缓存 `g_boss_ptr = pCtx->Edi` |
| `0x42B938` | `th15_lock_timer3` | 缓存 `g_boss_ptr`；进场（`EAX>=0`）写死首阶段 `Boss1` + 进场计数 |
| `0x42C7D6` / `0x42C785` | `th15_pattern_switch(_bossdead)` | 读槽写前旧值缓存当前攻击名 |

阶段名来源（逆向结论）：

- 名字串写偏移 `scr + 槽索引*0x88 + 0x528C`，其中 `scr = [g_boss_ptr + 0x44B8]`；槽写前旧值即“当前攻击名”。
- boss 战门控 `[g_boss_ptr + 0x4060] & 0x800000`。
- `BossDead` / `BossEscape` 时查映射表 `g_th15_mb_map[关卡][道中/关底]` 取该段末符卡名。
- 道中/关底判定：练习模式查 `th_sections_cba`；正常游玩按 boss 进场计数（首进场=道中，次进场=关底）。

界面/配置：IGI 窗口高度固定为屏幕 38%，在 miss/bomb 计数下方显示最近 5 条（无事件显示 `NN`）；新增高级选项与 locale 文案。

## 3. TH14 实现

- 逻辑抽为 header-only 的 `thprac_th14_missbomb.h`，由 `thprac_th14.cpp` 接线；仅 ST1~ST6（EX 跳过）。
- 偏移与 TH15 不同：`scr = [boss + 0x40EC]`，名字指针 `scr + 槽索引*0x10 + 0x5264`，门控 `[boss + 0x4054] & 0x800000`。
- 站点：`0x428782`（normal）/ `0x42C5E4`（BossDead）/ `0x42C6C4`（BossEscape）；末符卡名由 `OnPatternSwitchDeath` 捕获（映射表留空待验证）。

## 4. 弃用原因

1. **可维护性差**：大量硬编码的逆向地址、槽步长、boss 阶段映射表，边界条件注释少。
2. **偶发致命崩溃**：
   - 在游戏进程内使用 C++ 文件流 `std::ofstream` / `std::ifstream` 落盘——这是整个 thprac 仓库**唯一**使用 STL 文件流的地方（其它写盘一律用 Win32 `CreateFile` / `WriteFile`，如截图、replay、游戏内日志）。
   - 记录路径上还使用 `std::string` / `std::format` / `std::deque` 在热 hook 中分配内存。
   - 另有一个与写盘无关的隐患：`g_boss_ptr` 仅在开局清空，换关/boss 消失后未清，之后被当野指针读取。

## 5. 重做约定与注意事项

- **文件操作强制使用 Win32 API**：一律用 `CreateFileW` / `WriteFile` / `ReadFile`（配合 `CloseHandle`），**禁止**使用 C++ STL 文件流（`std::ofstream` / `std::ifstream`）。仓库内其它所有写盘（截图 `thprac_games_dx8.cpp`、replay `thprac_games.cpp`、游戏内日志 `thprac_log.cpp`）均如此，是已被验证的方式。
- **游戏进程内避免堆分配**：记录路径（miss/bomb hook 内）不要使用会在运行时分配内存的 C++ 容器/字符串/格式化（如 `std::string` / `std::format` / `std::deque`）；记录数据用定长 `char[]`、定长数组等普通 C 结构。
- **缓存指针必须及时清理**：缓存的游戏对象指针（如 `g_boss_ptr`）应在换关、boss 退场、一局结束时清空，或读取前先校验有效性，避免野指针读取导致的偶发崩溃。
- **（可选）把落盘放到 launcher 进程**：若仍不希望在游戏进程内写盘，可用命名共享内存（`CreateFileMapping` / `MapViewOfFile`）把记录传给 launcher，由 launcher 用 Win32 写文件；这会引入一条跨进程通道，非必需，仅在确有需要时采用。
- **记录范围**：仅在 thprac 练习模式（`mode == 1`）且对应开关开启时记录。

## 6. 相关文档

| 文档 | 说明 |
| --- | --- |
| `docs/th15/th15_missbomb_recording.md` | 功能实现说明（boss 阶段名方案） |
| `docs/th15/th15_re_disasm_report.md` ~ `report4.md` | 反编译调查报告 |
| `docs/th15/th15_re_disasm_requests.md` ~ `requests4.md` | 反编译请求 |
| `docs/th15/thprac_th15_hooks.md` | TH15 全部钩子整理 |
| `docs/th14/th14_re_disasm_requests.md` | TH14 复刻反编译请求 |
| `docs/th14/thprac_th14_hooks.md` | TH14 钩子总表 |

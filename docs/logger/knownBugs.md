# logger 已知问题清单（活文档）

> 上位文档：`docs/logger/logger_progress.md`（进度）、`docs/logger/checkpoint_entry_backfill_workflow.md`（入口回填口径）。
> 用途：登记 checkpoint 白名单 / 监控落地过程中**已知但暂不修**的问题，供后续迭代收口。
> 优先级：P0 阻塞功能 ｜ P1 影响正确性、需尽快修 ｜ **P2 已知、可接受、暂不修**。

---

## 1. 问题清单

| # | 优先级 | 作 | 位置 | 现象 | 影响 | 处置 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | **P2** | th15 | `ST7 前半 #5` / `ST7 后半 #1` | 两行入口同名，均为 `MainSub04` —— `st07` 的 `MainFront()` 末尾与 `MainLatter()` 开头**都** `call @MainSub04()`（偏移 `0x5f30` / `0x5f84`） | 白名单按 sub 名匹配，两个 checkpoint 无法区分 → 二者必有一行**不可达**（实际只会命中同一个记录名） | 暂不修。thprac 章节 warp 数据固有；该关暂不使用 |
| 2 | **P2** | th14–th18 | 日志落盘的 `events[].checkpoint` | miss/bomb 钩子在 ECL tick 中触发，而 `mSection` 是**上一帧** `Logger::Update()` 的结果 | 刚好在「进入新段落的同一帧」miss 时，事件会记成**上一段**（一帧延迟，约 16 ms） | 暂不修。窗口极窄；真机对照 IGI 面板可见 |
| 3 | **P2** | th14–th18 | 日志落盘的 `events` 数组 | `GameSessionData::events` 是定长 256 项（`thprac_games.h`），溢出即丢弃并计 `droppedEvents` | 长局 + 关闭 FastRetry 反复 miss 时可超 256，JSON 里出现 `eventsDropped` 且尾部事件缺失 | 暂不修。练习模式每次 miss 就 FastRetry 重开一局，实际远小于 256 |
| 4 | **P2** | th18 | 日志落盘的 `shot` 字段 | th18 的 IGI 面板无 `diff_pl` 行，机体/子机地址也未登记（总括 §6 Q7 只填了 th14） | th18 每局 `shot` 恒为空字符串 | 暂不修。需补 th18 的机体地址逆向；不影响其余字段 |
| 5 | **P1** | th14–th18 | 日志落盘的「一局」边界 | `*_game_start` 钩子是「开新局初始化」代码（score/graze/power 归零、bomb 置 3），静态上指向「只在新局/重开触发」，但**未真机钉死**是否也在 ST→ST 切关时触发 | 若每关触发，一个全关会产出多行而非一行，与「一局」口径冲突 | **待真机验证**（`logger_impl_report.md` §7 验证项 3）。修法：`Begin()` 加「仅当上一帧不在局内才开新局」一行 guard |

---

## 2. 说明

**#1（th15 ST7）详述**

- 口径：CSV 是**路过式 checkpoint（latch）白名单**——运行期读「当前 ECL sub 名」，命中表内函数即置「当前段落」。
- 问题：`MainSub04` 在 thprac 的 warps 表里对应**两个**不同的记录名（`ST7 前半 #5` 与 `ST7 后半 #1`），但运行期两者是**同一个 sub**，白名单无法二选一。
- 现状：Csv 中两行都填了 `MainSub04`（机械规则所致），实际效果是「命中 `MainSub04` 时只会把段落置为后出现的那一行」。
- 修法（未做）：需 CSV schema 支持「同名入口 → 多记录名」的消歧（如加一列区分条件），或运行期结合额外状态（关卡进度 / 前一 sub）判段。
- 参考：`docs/logger/checkpoint_entry_backfill_workflow.md` §1（latch 口径）、§5（边界）。

**#2–#4（日志落盘）详述**

- 口径与实现见 `docs/logger/logger_impl_report.md`。三条都是**已知可接受**的边界，不影响主链路：
  - **#2**：事件记录的是「钩子触发时刻的当前段落」，而当前段落由每帧 `Update()` 维护，天然有一帧滞后。要在同一帧内消除需把 latch 提前到 tick 内，代价大于收益。
  - **#3**：定长数组是为了满足「热 hook 内零堆分配」（`feat/plogth15` 的崩溃前车之鉴）。溢出时**不静默**：JSON 里会带 `eventsDropped`。
  - **#4**：th18 无现成机体来源；若要补，需按 `docs/gametime/` 的探针流程新增逆向，属独立任务。

**#5（一局边界）详述**

- 现状：`Begin()` 在 `*_game_start` 钩子里**无条件**开新局（若上一局还开着，先把上一局转为待落盘）。
- 静态证据：该钩子所在例程把 score(`0x4A57B0`)、graze(`0x4A57C0`)、power(`0x4A57E4`) 归零、bomb 置 3 —— 若每关都跑，跨关分数会被清空，故语义上是「开新的一局」。但**未真机钉死**。
- 验证：真机 th16 练习从 ST1 打到 ST2，看月度文件里出现 1 行还是 2 行（`logger_impl_report.md` §7 验证项 3）。
- 若确认每关触发，修法（一行）：`Begin()` 里把「无条件开新局」改成「仅当上一帧不在局内（`!mWasInGame`）才开新局」，否则只做段落复位。

---

## 3. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档。登记 th15 `ST7 前半 #5` / `ST7 后半 #1` 入口同名（`MainSub04`）问题（P2，暂不修）。 |
| 2026-10-17 | 追加日志落盘相关的 4 条：#2 事件 checkpoint 一帧延迟（P2）、#3 `events` 上限 256（P2）、#4 th18 `shot` 留空（P2）、#5「一局边界是否等于一关边界」待真机（P1）。 |

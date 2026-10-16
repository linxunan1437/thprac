# logger 已知问题清单（活文档）

> 上位文档：`docs/logger/logger_progress.md`（进度）、`docs/logger/checkpoint_entry_backfill_workflow.md`（入口回填口径）。
> 用途：登记 checkpoint 白名单 / 监控落地过程中**已知但暂不修**的问题，供后续迭代收口。
> 优先级：P0 阻塞功能 ｜ P1 影响正确性、需尽快修 ｜ **P2 已知、可接受、暂不修**。

---

## 1. 问题清单

| # | 优先级 | 作 | 位置 | 现象 | 影响 | 处置 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | **P2** | th15 | `ST7 前半 #5` / `ST7 后半 #1` | 两行入口同名，均为 `MainSub04` —— `st07` 的 `MainFront()` 末尾与 `MainLatter()` 开头**都** `call @MainSub04()`（偏移 `0x5f30` / `0x5f84`） | 白名单按 sub 名匹配，两个 checkpoint 无法区分 → 二者必有一行**不可达**（实际只会命中同一个记录名） | 暂不修。thprac 章节 warp 数据固有；该关暂不使用 |

---

## 2. 说明

**#1（th15 ST7）详述**

- 口径：CSV 是**路过式 checkpoint（latch）白名单**——运行期读「当前 ECL sub 名」，命中表内函数即置「当前段落」。
- 问题：`MainSub04` 在 thprac 的 warps 表里对应**两个**不同的记录名（`ST7 前半 #5` 与 `ST7 后半 #1`），但运行期两者是**同一个 sub**，白名单无法二选一。
- 现状：Csv 中两行都填了 `MainSub04`（机械规则所致），实际效果是「命中 `MainSub04` 时只会把段落置为后出现的那一行」。
- 修法（未做）：需 CSV schema 支持「同名入口 → 多记录名」的消歧（如加一列区分条件），或运行期结合额外状态（关卡进度 / 前一 sub）判段。
- 参考：`docs/logger/checkpoint_entry_backfill_workflow.md` §1（latch 口径）、§5（边界）。

---

## 3. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档。登记 th15 `ST7 前半 #5` / `ST7 后半 #1` 入口同名（`MainSub04`）问题（P2，暂不修）。 |

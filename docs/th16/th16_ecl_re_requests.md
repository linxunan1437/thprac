# TH16 ECL 段落监控（读取 ECL 函数为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th16.exe`，683520 字节，PE `TimeDateStamp = 1501534029`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（`RVA = VA − 0x400000`）。
> 引擎代：**C 代（第四代引擎）**；同代参照：th15（模板）。
> thprac 现有支持：`thprac/src/thprac/thprac_th16.cpp`。
> ⚠️ 本作 **stage 5 存在剧情分支**。

---

## 0. 本轮需要的东西

监控需要“**当前 ECL 函数名**”。本作已有解释器命中点与疑似 sub 索引，主要缺“名字表确认 + 函数栈 + 宿主捕获”。

> **口径**：本迭代最终产物是**实时显示「当前段落」**，读 ECL sub 只是手段。本作读到 sub 后需按白名单映射为“段落入口”再显示（见总括 §1/§2.1）；并需回答 **Q9（段落判定口径）**。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL 解释器命中点 | `0x473995`（EHOOK 4B，注释 “freeze ECL sub time for stage's MainLatter”） | `thprac_th16.cpp:533` |
| 命中时执行对象寄存器 | `pCtx->Edi` | 同上 |
| 执行对象已有字段 | `subID = *(u32*)(pCtx->Edi + 0x4)`（按需求文档/TH15 模型 ⇒ **sub 索引**） | `:534` |
| 现成用法 | MainLatter 判定 `mainLatterIDs[7] = {0,0,0,92,122,0,0}`（stage=STAGE_NUM−1） | `:536-538` |
| ECL 数据区链 | `GetMemAddr(0x4a6dc0, 0x17c, 0xC)` | `thprac_th16.cpp:2347`（`THSectionPatch`） |
| 指针常量 | `ENEMY_MANAGER_PTR=0x4a6dc0`、`PLAYER_PTR=0x4a6ef8`、`STAGE_NUM=0x4a5790` | `thprac_th16.cpp:9-13` |
| 其它对象指针 | PauseInf `0x4a6dd4`、ReplayMgr `0x4a6f08` | `docs/gametime/gametime_thxx_flags.md` §3 th16 |
| miss/bomb/边界钩子 | `th16_life_dec 0x443d3a`(5B)、`th16_bomb_dec 0x40db9c`(5B)、`th16_game_start 0x42e5ae`(7B) | `thprac_th16.cpp:2745-2757` |
| IGI 面板类 | `TH16InGameInfo`（`mMissCount`/`mBombCount`） | `thprac_th16.cpp:551` |

> **统一模型推断**：th16 的 `SetBaseAddr = GetMemAddr(0x4a6dc0, 0x17c, 0xC)` ⇒ `res = [0x4a6dc0]+0x17c`，按 th14/th15 规律 **`res+0x8C` 应为 sub 表（表项 8B，`+0`=名字）**，`res+8`=sub 数。`name = table[subID].name`。**待确认。**

---

## 2. Q1 当前 sub（近似已答，待确认）

- 确认 `*(u32*)(pCtx->Edi + 0x4)` = **sub 索引**（0..count−1），`Edi` = th15 意义的 `thread` 或等价对象。
- 给出该字段的写入点。

## 3. Q2 sub 表：索引 → 名字/起始（待确认）

- 确认 sub 表位置：`table = *(u32*)( *(u32*)(ENEMY_MANAGER_PTR+0x17c) + 0x8C )`（若与 th14/15 同构）；表项 `+0`=名字 `char*`、`+4`=起始指令；`count = *(i32*)(res+8)`。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 ECL 子调用栈？链/遍历规则/深度上限；不存在则给替代方案。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn “main” 时写入主线宿主的 hook 点。参考 TH15 `0x43bbc8/0x43d48e/0x43d6be`。
- 线索：`0x473995` 命中时的 `Edi` 是否恒为主线宿主。

## 6. Q5 解释器命中点与寄存器语义（部分已知）

- 已有 `0x473995`（4B），命中 `Edi`=执行对象。请补：所在函数入口与调用链、覆盖 4B 指令语义、能否作每帧稳定触发点。

## 7. Q6 名字来源（待确认）

- 确认运行期是否保留 sub 名（`table[i].name`）。否则给出索引/偏移字段供离线建表。

## 8. Q7 机体/子机类型（待答）

- 给出 `character` / `shotType` 地址。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：**已定案为「路过式 checkpoint（latch）」**（见总括 §6 Q9）——经过白名单函数即置值、不在表内 no-op（不回退 `main`）。本作只需回答：**当前 sub 如何取**（Q1/Q2 链）+ **每段入口 sub 名单**（= warp 落点所进入的 sub，**含 async**）。

- 练习 `ECLJump`（`thprac_th16.cpp:1078`）是否改变 sub 索引/名字判读。
- ⚠️ **stage 5 分支**：确认运行期读取法在分支路径下仍正确。

---

## 10. 真机验证清单

- [ ] 道中/boss/不同 sub 采样，函数名与实际一致（可用现有 `mainLatterIDs` 交叉印证）。
- [ ] **stage 5 两条分支**各走一遍。
- [ ] `res=[0x4a6dc0]+0x17c` 非空、`res+0x8C` 表可读、索引越界保护生效。

## 11. 交付格式

- Q1–Q6 给出地址/指针链 + 判据 + 反汇编证据。RVA 表述。回填本文件 + 同步总括 §5。

## 12. 工具与复现

- `pefile` + `capstone` 只读反汇编。切入点：反汇编 `0x473995` 所在函数；展开 `[0x4a6dc0]+0x17c` 资源。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（requests）。按 th15 统一模型推断 sub 索引与 sub 表；列出待确认/待答项。未跑逆向。 |

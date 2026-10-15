# TH13 ECL 段落监控（读取 ECL 函数为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th13.exe`，790528 字节，PE `TimeDateStamp = 1313589413`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（`RVA = VA − 0x400000`）。
> 引擎代：**B 代（第二代引擎）**；同代参照：th10 / th11 / th12。
> thprac 现有支持：`thprac/src/thprac/thprac_th13.cpp`。

---

## 0. 本轮需要的东西

监控需要“**当前 ECL 函数名**”的地址/指针链/判据/hook 语义。本作无“freeze ECL sub time”钩子，但有**若干 ECL 解释器内的锁时钩子**（§1），可作为切入点。

> **口径**：本迭代最终产物是**实时显示「当前段落」**，读 ECL sub 只是手段。本作读到 sub 后需按白名单映射为“段落入口”再显示（见总括 §1/§2.1）；并需回答 **Q9（段落判定口径）**。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| “freeze ECL sub time”钩子 | ❌ 无 | — |
| **ECL 解释器锁时钩子（可用切入）** | `th13_lock_timer1 0x4250ea`(6B, 初始化)、`th13_lock_timer2 0x41e26c`(4B, SetNextPattern case 514)、`th13_lock_timer3 0x41d399`(5B, set boss mode case 512)、`th13_lock_timer4 0x421a23`(6B, decrease time update) | `thprac_th13.cpp:2253-2265` |
| ECL 数据区链 | `GetMemAddr(0x4c2188, 0xac, 0xC)` | `thprac_th13.cpp:1808`（`THSectionPatch`） |
| 指针常量 | `PLAYER_PTR=0x4c22c4`、`REPLAY_MGR_PTR=0x4c22c8`、`STAGE_NUM=0x4be81c` | `thprac_th13.cpp:14-16`；`docs/gametime/gametime_thxx_flags.md` §3 th13 |
| 其它对象指针 | PauseInf `0x4c2194` | `docs/gametime/gametime_thxx_flags.md` §3 th13 |
| 练习补丁 | `ECLJump`（`thprac_th13.cpp:936`）、`THStageWarp`（`:948`）、`THPatch`（`:1160`） | 同上 |
| miss/bomb/边界钩子 | `th13_life_dec 0x444a75`(10B)、`th13_bomb_dec 0x40a404`(5B)、`th13_game_start 0x42d3d7`(7B) | `thprac_th13.cpp:2234-2246` |
| IGI 面板类 | `TH13InGameInfo`（`mMissCount`/`mBombCount`） | `thprac_th13.cpp:577` |

> **切入点说明**：`lock_timer2/3`（`0x41e26c`/`0x41d399`）对应 ECL 解释器的 opcode `0x202`/`0x200`（SetNextPattern / set boss mode），与 TH15 的 `0x42c738`/`0x42b938` 同构 —— 命中时很可能能拿到**当前 ECL 执行对象**。优先反汇编这两处及 `0x4250ea`（初始化），定位执行对象寄存器。

---

## 2. Q1 当前 sub：索引还是指令指针？（待答）

- 从 §1 的解释器钩子出发，确定执行对象寄存器与“当前 sub 字段”的偏移与类型。

## 3. Q2 sub 表：索引 → 名字/起始（待答）

- 从执行对象或全局 `[0x4c2188]+0xac` 链，给出到 sub 表（`索引 → 名字/起始`）的完整链；表项大小与字段偏移。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 ECL 子调用栈？链/遍历规则/深度上限；不存在则给替代方案。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn “main” 时写入主线 ECL 宿主对象的 hook 点（地址 + 命中寄存器）。参考 TH15 `0x43bbc8/0x43d48e/0x43d6be`。

## 6. Q5 解释器命中点与寄存器语义（待答，最重要）

- 请给出一个“每帧执行 ECL 指令”的稳定地址 + 覆盖长度 + 命中寄存器（执行对象）。
- 建议从 `0x41d399` / `0x41e26c`（case 512/514）反汇编其所在解释器函数，向函数入口收敛。

## 7. Q6 名字来源（待答）

- 运行期 ECL 资源是否保留 sub 名（`char*`）？不保留则给出索引/偏移字段，便于离线建名表。

## 8. Q7 机体/子机类型（待答）

- 给出 `character` / `shotType` 地址。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：**已定案为「路过式 checkpoint（latch）」**（见总括 §6 Q9）——经过白名单函数即置值、不在表内 no-op（不回退 `main`）。本作只需回答：**当前 sub 如何取**（Q1/Q2 链）+ **每段入口 sub 名单**（= warp 落点所进入的 sub，**含 async**）。

- 练习 `ECLJump` 是否改变 sub 索引/名字判读。

---

## 10. 真机验证清单

- [ ] 道中/boss/不同 sub 采样，函数名与实际一致。
- [ ] 练习模式跳章后名字正确。

## 11. 交付格式

- Q1–Q6 给出地址/指针链 + 判据 + 反汇编证据；Q7/Q8 视需要。RVA 表述。回填本文件 + 同步总括 §5。

## 12. 工具与复现

- `pefile` + `capstone` 只读反汇编。切入点：`0x41d399`/`0x41e26c` → 解释器函数；`THSectionPatch` ECL 数据区链。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（requests）。无 freeze 钩子，但记录 ECL 解释器锁时钩子作为切入；列出 Q1–Q8 待答。未跑逆向。 |

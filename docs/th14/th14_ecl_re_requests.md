# TH14 ECL 段落监控（读取 ECL 函数为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th14.exe`，897536 字节，PE `TimeDateStamp = 1376634201`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（`RVA = VA − 0x400000`）。
> 引擎代：**C 代（第四代引擎）**；同代参照：th15（sub 名读取模板已完整）。
> thprac 现有支持：`thprac/src/thprac/thprac_th14.cpp`。
> ⚠️ 本作 **stage 4 存在剧情分支**。

---

## 0. 本轮需要的东西

监控需要“**当前 ECL 函数名**”。本作已有 ECL 解释器命中点与 **sub 表**，主要缺“sub 索引语义确认 + 名字 + 函数栈 + 宿主捕获”。

> **口径**：本迭代最终产物是**实时显示「当前段落」**，读 ECL sub 只是手段。本作读到 sub 后需按白名单映射为“段落入口”再显示（见总括 §1/§2.1）；并需回答 **Q9（段落判定口径）**。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL 解释器命中点 | `0x483c4f`（EHOOK 4B，注释 “freeze ECL sub time for stage 5's MainLatter”） | `thprac_th14.cpp:590` |
| 命中时执行对象寄存器 | `pCtx->Edi` | 同上 |
| 执行对象已有字段 | `*(u32*)(pCtx->Edi + 0x4)`（代码注释为“当前指令指针”） | `:597` |
| **sub 表（已定位）** | `eclSubArr = GetMemContent(ENEMY_MANAGER_PTR, 0xcc, 0x8c)`；表项 8B，`+4` = 该 sub 起始指令地址 | `:594-596` |
| ECL 数据区链 | `GetMemAddr(0x4db544, 0xcc, 0xC)`（**与 sub 表同源**） | `thprac_th14.cpp:2681`（`THSectionPatch`） |
| 指针常量 | `BOMB_PTR=0x4db52c`、`ENEMY_MANAGER_PTR=0x4db544`、`PLAYER_PTR=0x4db67c`、`CHARA_ADDR=0x4f5828`、`SUBSHOT_ADDR=0x4f582c`、`STAGE_NUM=0x4f58a4`、`WINDOW_PTR=0x4f5a18` | `thprac_th14.cpp:12-18` |
| 其它对象指针 | PauseInf `0x4db558`、ReplayMgr `0x4db688` | `docs/gametime/gametime_thxx_flags.md` §3 th14 |
| miss/bomb/边界钩子 | `th14_life_dec 0x44f618`(5B)、`th14_bomb_dec 0x41218a`(5B)、`th14_game_start 0x4375be`(7B) | `thprac_th14.cpp:3233-3249` |
| IGI 面板类 | `TH14InGameInfo`（`mMissCount`/`mBombCount`） | `thprac_th14.cpp:614` |

> **重要确认（统一模型）**：th14 的 sub 表 = `[[0x4db544]+0xcc]+0x8C`，而 `THSectionPatch` 的 `SetBaseAddr` 正是 `GetMemAddr(0x4db544, 0xcc, 0xC)` —— 说明 **`res = [0x4db544]+0xcc` 就是 ECL 资源对象，`res+0xC` 是文件缓冲、`res+0x8C` 是 sub 表**，与 TH15（`res = host+0x11F8`，`res+0x8C` 表）**完全同构**。因此 Q2 基本已答，只剩“名字字段确认”。

---

## 2. Q1 当前 sub：`[Edi+4]` 到底是什么？（**首要待答**）

- th14 现有注释把 `*(Edi+4)` 当“当前指令指针”，并作 `curInstr ∈ [start(65), start(66))` 的区间比较（`st5MainLatterID = 65`）。
- 但 **TH15 已证实**：`thread+4` = **sub 索引**（非地址）；th16/17/18 亦当索引用。
- 请判定二者之一：
  - **(a)** `Edi` 就是 th15 意义的 `thread`，则 `*(Edi+4)` = **sub 索引**，于是
    `name = *(char**)(table + subIdx*8)`（th14 现有注释有误）；或
  - **(b)** th14 的 `thread+4` 存的是**当前 sub 起始指令**（与 th15 不同），则需另找 sub 索引字段。
- 请给出：命中点处 `Edi` 指向的对象类型、`+0`/`+4`/… 各字段语义、`table` 表项 `+0` 是否 = 名字 `char*`。

## 3. Q2 sub 表：索引 → 名字/起始（基本已答，待确认名字字段）

- 表位置已知：`res+0x8C`，`res = [ENEMY_MANAGER_PTR(0x4db544)+0xcc]`；表项 8B。
- 待确认：`+0` 是否 = 名字 `char*`（同 th15），`+4` 是否 = 起始指令；以及 sub 总数在 `res+8`（同 th15）。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 ECL 子调用栈？链/遍历规则/深度上限；不存在则给替代方案。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn “main” 时写入主线 ECL 宿主对象（TH15 意义）的 hook 点。th14 可先从 `0x483c4f` 命中时的 `Edi` 是否恒为主线宿主入手。

## 6. Q5 解释器命中点与寄存器语义（部分已知）

- 已有 `0x483c4f`（4B），命中 `Edi`=执行对象。请补：所在函数入口与调用链、覆盖 4B 的指令语义、能否作每帧稳定触发点。

## 7. Q6 名字来源（待答）

- 由 §1/§3：sub 表项很可能含名字指针。确认运行期是否保留 sub 名（否则离线建表）。

## 8. Q7 机体/子机类型（**已知**）

- `CHARA_ADDR=0x4f5828`、`SUBSHOT_ADDR=0x4f582c`；th14 IGI 已用 `cur_player_type = *(i32*)0x4f5828 * 2 + *(i32*)0x4f582c`（`thprac_th14.cpp:660`）。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：本作走 **(A) 调用栈祖先法** 还是 **(B) 顺序 latch 法**？（定义见总括 §6 Q9；thprac 倾向 A，需结合 `.ecl` 脚本确认。）

- 练习 `ECLJump`（`thprac_th14.cpp:1658`）是否改变 sub 索引/名字判读。
- ⚠️ **stage 4 分支**：确认运行期读取法在分支路径下仍正确。

---

## 10. 真机验证清单

- [ ] 道中/boss/不同 sub 采样，函数名与实际一致。
- [ ] **stage 4 两条分支**各走一遍，名字均正确。
- [ ] 确认 `res=[0x4db544]+0xcc` 非空、`res+0x8C` 表可读、sub 索引越界保护生效。
- [ ] 练习模式跳章后名字正确。

## 11. 交付格式

- Q1–Q6 给出地址/指针链 + 判据 + 反汇编证据；Q7/Q8 视需要。RVA 表述。回填本文件 + 同步总括 §5。

## 12. 工具与复现

- `pefile` + `capstone` 只读反汇编。切入点：反汇编 `0x483c4f` 所在函数，确认 `Edi` 语义与 sub 表用法；展开 `[0x4db544]+0xcc` 资源。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（requests）。确认 th14 sub 表与 TH15 同构（`res+0x8C`）；将“`[Edi+4]` 是索引还是指令指针”列为 Q1 首要待答；列出其余待答项。未跑逆向。 |

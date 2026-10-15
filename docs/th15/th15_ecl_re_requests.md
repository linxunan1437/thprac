# TH15 ECL 段落监控（读取 ECL 函数为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th15.exe`，951808 字节，PE `TimeDateStamp = 1444028071`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（`RVA = VA − 0x400000`）。
> 引擎代：**C 代（第四代引擎）· 参照作**：本作 sub 名读取链路**已由 `feat/plogth15` 分支验证**，是 th14/16/17/18 的模板。
> thprac 现有支持：`thprac/src/thprac/thprac_th15.cpp`；既有实现参考：`feat/plogth15` 分支的 `thprac_th15_missbomb.{h,cpp}`（`TH15MissBombRecorder::GetEclSubName`）。

---

## 0. 本轮需要的东西

监控需要“**当前 ECL 函数名**”。本作 **Q1/Q2/Q4/Q5/Q6 基本已答**，只剩 **Q3（函数栈）** 与 **Q9（段落判定口径）** 待补。

> **口径**：本迭代最终产物是**实时显示「当前段落」**，读 ECL sub 只是手段。本作读到 sub 后需按白名单映射为“段落入口”再显示（见总括 §1/§2.1）。**Q9 已定案为「路过式 checkpoint（latch）」**（经过白名单函数即置值、不在表内 no-op），本作作参照作最先定案。

---

## 1. 已知锚点与已答案

| 项 | 值 | 出处 |
| --- | --- | --- |
| 主线宿主捕获 hook | `0x43bbc8`(2B, `FUN_0043bbb0` 内)、`0x43d48e`(5B, 关卡进入路径)、`0x43d6be`(5B, 重开/重试路径)；命中 `EAX` = 宿主 | `feat/plogth15` `thprac_th15.cpp`、`th15_missbomb_phase_plan.md` §3.1 |
| 名字读取链（**已答**） | `thread=*(u32*)(host+0xC)`；`subIdx=*(i32*)(thread+4)`；`res=*(u32*)(host+0x11F8)`；`count=*(i32*)(res+8)`；`table=*(u32*)(res+0x8C)`；`name=*(char**)(table+subIdx*8)`；表项 `+0`=名字、`+4`=起始指令 | `thprac_th15_missbomb.cpp::GetEclSubName` |
| 按名查表函数 | `FUN_0048f340`（对 `table[i].name` 二分查找） | `th15_missbomb_phase_plan.md` §3.1 |
| ECL 管理器 / 数据区 | 管理器 `0x4e9a80`：`+0x180` 链表、`+0x18C` 活跃脚本计数、`+0x48+序号*4` 阶段槽；`THSectionPatch` 数据区 = `GetMemAddr(0x4e9a80, 0x17c, 0xC)` | `th15_re_disasm_requests5.md` §0.4；`thprac_th15.cpp:2183` |
| 指针常量 | `PLAYER_PTR=0x4e9bb8`、`BOMB_PTR=0x4e9a68`、`STAGE_NUM=0x4e73f0` | `thprac_th15.cpp:13-14`；`th15_missbomb.h` |
| miss/bomb/边界钩子 | `th15_life_dec 0x456398`(5B)、`th15_bomb_dec 0x41497a`(5B)、`th15_game_start 0x43e6ee`(7B) | `thprac_th15.cpp:2674-2685` |
| 其它 ECL 解释器钩子 | `0x43404a`(lock_timer1)、`0x42c738`(case 514)、`0x42b938`(case 512)、`0x4301e8`(lock_timer4) | `docs/th15/thprac_th15_hooks.md` |
| 解释器函数 | `FUN_004291e0`（`0x4291E0~0x42DBCB`） | `th15_re_disasm_requests5.md` §0.4 |
| IGI 面板类 | `TH15InGameInfo`（`mMissCount`/`mBombCount`） | `thprac_th15.cpp:598` |

**已答案总结**：
- **Q1（已答）** 当前 sub = **索引**：`*(i32*)(thread+4)`，`thread = *(u32*)(host+0xC)`。
- **Q2（已答）** sub 表 = `*(u32*)(res+0x8C)`，`res = *(u32*)(host+0x11F8)`；表项 8B，`+0`=名字。
- **Q4（已答）** 宿主捕获：`0x43bbc8/0x43d48e/0x43d6be`，`EAX`=宿主。
- **Q5（已答/可复用）** 解释器函数 `FUN_004291e0`；`0x42b938/0x42c738` 为 case 512/514。
- **Q6（已答）** 运行期保留 sub 名（`table[i].name` 为 `char*`）。

---

## 2. Q3 函数栈（主要待答）

- ⚠️ **Q9 已定案为路过式 checkpoint（latch）**（见总括 §6 Q9）：不再需要「未命中则沿函数栈上溯」。本项仅作逆向事实记录。
- 请回答（事实性）：
  1. ECL 解释器是否维护显式的**子调用栈**（`call` 入栈、返回出栈）？栈顶是否 = 当前 sub？
  2. 给出**遍历规则/指针链**与**深度上限**（或栈容量）。
  3. 若没有可遍历的调用栈，请明确说明即可（不影响本功能）。

## 3. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：**已定案为「路过式 checkpoint（latch）」**（见总括 §6 Q9）——经过白名单函数即置值、不在表内 no-op（不回退 `main`）。本作只需回答：**当前 sub 如何取**（Q1/Q2 链）+ **每段入口 sub 名单**（= warp 落点所进入的 sub，**含 async**）。作为**参照作**，本作已最先定案（见 `th15_ecl_re_report.md` §3）。

- 练习 `ECLJump`（`thprac_th15.cpp:1018`）直接改写 ECL 字节；确认跳转后 `subIdx`/名字仍正确（`th15_missbomb_phase_plan.md` §7 已初步判断“练习补丁只改 ECL 数据、不改 sub 结构”，待复核）。

---

## 4. 复用注意（来自弃用实现的教训）

> `feat/plogth15`（deadpointLog）因**文件落盘侧**崩溃被弃用（在热 hook 里用 `std::ofstream`/堆分配、缓存指针未清等）。**监控部分本身是纯只读**，可复用；但要注意：
> - 读取函数需含**空指针/越界保护**（`res`/`table`/`subIdx` 越界返回 `nullptr`）；
> - 缓存的宿主指针须在**换关/退场/一局结束**时清理，避免野指针。

---

## 5. 真机验证清单

- [ ] 道中/boss/不同 sub 采样，函数名与实际一致（该实现当年已用于 th15 道中映射，可信度高）。
- [ ] 练习模式跳章后名字正确。
- [ ] 经过白名单函数时「当前段落」被正确置值；段内其余 sub no-op（不需栈上溯）。

## 6. 交付格式

- 仅需补 **Q3**（函数栈）+ **Q8** 确认 + **Q9**（段落判定口径）；其余直接引用本文件。RVA 表述。回填本文件 + 同步总括 §5。

## 7. 工具与复现

- `pefile` + `capstone` 只读反汇编。切入点：解释器 `FUN_004291e0` 内找 `call`/return 的栈操作；`0x42b938/0x42c738`（case 512/514）。

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（requests）。确认 Q1/Q2/Q4/Q5/Q6 已答（引用 `feat/plogth15`）；仅 Q3 函数栈 + Q8 待补。未跑逆向。 |

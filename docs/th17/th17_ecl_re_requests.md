# TH17 ECL 段落监控（读取 ECL 函数为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th17.exe`，747520 字节，PE `TimeDateStamp = 1565429804`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（`RVA = VA − 0x400000`）。
> 引擎代：**C 代（第四代引擎）**；同代参照：th15（模板）。
> thprac 现有支持：`thprac/src/thprac/thprac_th17.cpp`。

---

## 0. 本轮需要的东西

监控需要“**当前 ECL 函数名**”。本作已有解释器命中点与疑似 sub 索引，主要缺“名字表确认 + 函数栈 + 宿主捕获”。

> **口径**：本迭代最终产物是**实时显示「当前段落」**，读 ECL sub 只是手段。本作读到 sub 后需按白名单映射为“段落入口”再显示（见总括 §1/§2.1）；并需回答 **Q9（段落判定口径）**。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL 解释器命中点 | `0x47a712`（EHOOK 4B，注释 “freeze ECL sub time for stage's MainLatter”） | `thprac_th17.cpp:778` |
| 命中时执行对象寄存器 | `pCtx->Edi` | 同上 |
| 执行对象已有字段 | `subID = *(u32*)(pCtx->Edi + 0x4)`（按需求文档/TH15 模型 ⇒ **sub 索引**） | `:779` |
| 现成用法 | MainLatter 判定 `mainLatterIDs[7] = {0,87,91,86,88,92,0}` | `:781-783` |
| ECL 数据区链 | `GetMemAddr(0x4b76a0, 0x17c, 0xC)` | `thprac_th17.cpp:2420`（`THSectionPatch`） |
| 指针常量 | `STAGE_NUM=0x4b59dc`、`ENEMY_MANAGER_PTR=0x4b76a0`、`PLAYER_PTR=0x4b77d0`、`GOAST_MANAGER_PTR=0x4b7684`、`SPELLCARD_PTR=0x4b7690`、`WINDOW_PTR=0x5226c0` | `thprac_th17.cpp:76-82` |
| 其它对象指针 | PauseInf `0x4b76b0`、ReplayMgr `0x4b77d8` | `docs/gametime/gametime_thxx_flags.md` §3 th17 |
| miss/bomb/边界钩子 | `th17_life_dec 0x44921b`(6B)、`th17_bomb_dec 0x411cab`(6B)、`th17_game_start 0x4302e6`(6B) | `thprac_th17.cpp:2716-2767` |
| IGI 面板类 | `TH17InGameInfo`（`mMissCount`/`mBombCount`） | `thprac_th17.cpp:796` |

> **统一模型推断**：th17 的 `SetBaseAddr = GetMemAddr(0x4b76a0, 0x17c, 0xC)` ⇒ `res = [0x4b76a0]+0x17c`，按 th14/th15 规律 **`res+0x8C` 应为 sub 表**（表项 8B，`+0`=名字），`res+8`=sub 数。**待确认。**

---

## 2. Q1 当前 sub（近似已答，待确认）

- 确认 `*(u32*)(pCtx->Edi + 0x4)` = **sub 索引**；`Edi` = th15 意义的 `thread` 或等价对象；给出字段写入点。

## 3. Q2 sub 表：索引 → 名字/起始（待确认）

- 确认 `table = *(u32*)( *(u32*)(ENEMY_MANAGER_PTR+0x17c) + 0x8C )`；表项 `+0`=名字 `char*`、`+4`=起始；`count = *(i32*)(res+8)`。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 ECL 子调用栈？链/遍历规则/深度上限；不存在则给替代方案。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn “main” 时写入主线宿主的 hook 点。参考 TH15 `0x43bbc8/0x43d48e/0x43d6be`。
- 线索：`0x47a712` 命中时的 `Edi` 是否恒为主线宿主。

## 6. Q5 解释器命中点与寄存器语义（部分已知）

- 已有 `0x47a712`（4B），命中 `Edi`=执行对象。请补：所在函数入口与调用链、覆盖 4B 指令语义、能否作每帧稳定触发点。

## 7. Q6 名字来源（待确认）

- 确认运行期是否保留 sub 名（`table[i].name`）。否则给出索引/偏移字段供离线建表。

## 8. Q7 机体/子机类型（待答）

- 给出 `character` / `shotType` 地址。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：本作走 **(A) 调用栈祖先法** 还是 **(B) 顺序 latch 法**？（定义见总括 §6 Q9；thprac 倾向 A，需结合 `.ecl` 脚本确认。）

- 练习 `ECLJump`（`thprac_th17.cpp:1355`）是否改变 sub 索引/名字判读。

---

## 10. 真机验证清单

- [ ] 道中/boss/不同 sub 采样，函数名与实际一致（可用现有 `mainLatterIDs` 交叉印证）。
- [ ] `res=[0x4b76a0]+0x17c` 非空、`res+0x8C` 表可读、索引越界保护生效。

## 11. 交付格式

- Q1–Q6 给出地址/指针链 + 判据 + 反汇编证据。RVA 表述。回填本文件 + 同步总括 §5。

## 12. 工具与复现

- `pefile` + `capstone` 只读反汇编。切入点：反汇编 `0x47a712` 所在函数；展开 `[0x4b76a0]+0x17c` 资源。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（requests）。按 th15 统一模型推断 sub 索引与 sub 表；列出待确认/待答项。未跑逆向。 |

# TH12 ECL 段落监控（读取 ECL 函数为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th12.exe`，737280 字节，PE `TimeDateStamp = 1251344348`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（`RVA = VA − 0x400000`）。
> ECL / decl 素材：`../exes/ecl/th12/`（原始 `.ecl` 9 份）、`../exes/ecl_dump/th12/`（`thecl -d` 反编译 `.decl` 9 份）。清单与来源见 `docs/logger/decompile_materials.md`。
> 引擎代：**B 代（第二代引擎）**；同代参照：th10 / th11。
> thprac 现有支持：`thprac/src/thprac/thprac_th12.cpp`。
> ⚠️ 本作在 thprac 中**没有任何 ECL 解释器 hook**（无“freeze ECL sub time”类钩子），Q1–Q5 **全部待逆向**。

---

## 0. 本轮需要的东西

监控需要“**当前 ECL 函数名**”的地址/指针链/判据/hook 语义。本作需从零建立，切入点见 §9。

> **口径**：本迭代最终产物是**实时显示「当前段落」**，读 ECL sub 只是手段。本作读到 sub 后需按白名单映射为“段落入口”再显示（见总括 §1/§2.1）；并需回答 **Q9（段落判定口径）**。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL 解释器 hook | ❌ 无 | — |
| ECL 数据区链 | `GetMemAddr(0x4b43dc, 0x64, 0xC)` | `thprac_th12.cpp:1805`（`THSectionPatch`） |
| 指针常量 | `PLAYER_PTR=0x4b4514`、`REPLAY_MGR_PTR=0x4b4518`、`STAGE_NUM=0x4b0cb0` | `thprac_th12.cpp:38/43/44` |
| 练习补丁 | `ECLJump`/`ECLJumpEx`/`ECLSetHealth`/`ECLCallSub`（`thprac_th12.cpp:997-1018`） | 同上 |
| miss/bomb/边界钩子 | `th12_life_dec 0x4381e2`(5B)、`th12_bomb_dec 0x422f28`(5B)、`th12_game_start 0x421def`(6B) | `thprac_th12.cpp:2071-2095` |
| IGI 面板类 | `TH12InGameInfo`（`mMissCount`/`mBombCount`） | `thprac_th12.cpp:604` |

> **强线索（Q6）**：`ECLCallSub` 写入的 `call` 指令（opcode `0x000b`）**内嵌 sub 名字符串**：
> `ecl << ecl_time << 0x000b << (sub_length+0x14) << 0x01ff0000 << 0 << sub_length;` 随后逐字节写名字。
> ⇒ th12 的 ECL 子调用**按名字**进行，运行期/脚本内**必然存在 sub 名字**。这决定了 Q6 很可能能用“运行期名字表”或“call 指令内嵌名”解决。
> （`0x000B` 是 call，`0x0018000C` 是 goto；对比 th13/th14 的 goto 指令。）

---

## 2. Q1 当前 sub：索引还是指令指针？（待答）

- 找到 ECL 解释器里“每帧推进/执行指令”的位置，确定执行对象寄存器与“当前 sub 字段”的偏移与类型。

## 3. Q2 sub 表：索引 → 名字/起始（待答）

- 从执行对象或全局 `[0x4b43dc]+0x64` 链，给出到 sub 表（`索引 → 名字/起始`）的完整链；表项大小与字段偏移。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 ECL 子调用栈？链/遍历规则/深度上限；不存在则给替代方案。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn “main” 时写入主线 ECL 宿主对象的 hook 点（地址 + 命中寄存器）。参考 TH15 `0x43bbc8/0x43d48e/0x43d6be`。

## 6. Q5 解释器命中点与寄存器语义（待答，最重要）

- 本作**完全缺失**解释器命中点。请给出一个“每帧执行 ECL 指令”的稳定地址 + 覆盖长度 + 命中寄存器（执行对象）。
- 建议切入点：
  1. 先反汇编 `THSectionPatch` 用的 ECL 数据区加载点，找解释器函数；
  2. 用 th10/th11 的 `0x1014` 链形态作参照，在 th12 里搜同构结构。

## 7. Q6 名字来源（强线索）

- 由 §1 线索：th12 `call` 指令内嵌 sub 名。请确认运行期 sub 表项里是否也保留名字指针（`char*`）。
- 若运行期无名：可用 `call` 指令内嵌名 + 白名单 CSV 做离线映射。

## 8. Q7 机体/子机类型（待答）

- 给出 `character` / `shotType` 地址。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：本作走 **(A) 调用栈祖先法** 还是 **(B) 顺序 latch 法**？（定义见总括 §6 Q9；thprac 倾向 A，需结合 `.ecl` 脚本确认。）

- 练习 `ECLJump`/`ECLCallSub` 是否改变 sub 索引/名字判读。

---

## 10. 真机验证清单

- [ ] 道中/boss/不同 sub 采样，函数名与实际一致。
- [ ] 练习模式跳章后名字正确。
- [ ] 二级指针越界/空指针保护。

## 11. 交付格式

- Q1–Q6 给出地址/指针链 + 判据 + 反汇编证据；Q7/Q8 视需要。RVA 表述。回填本文件 + 同步总括 §5。

## 12. 工具与复现

- `pefile` + `capstone` 只读反汇编。切入点：`THSectionPatch` 的 ECL 数据区链 → 解释器；同代 th10/th11 模板。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（requests）。本作无现成 ECL hook，Q1–Q5 全缺；记录“call 内嵌 sub 名”强线索与切入点。未跑逆向。 |

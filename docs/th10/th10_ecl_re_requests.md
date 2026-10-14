# TH10 ECL 段落监控（读取 ECL 函数为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th10.exe`，487936 字节，PE `TimeDateStamp = 1186086096`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。
> 引擎代：**B 代（第二代引擎）**；同代参照：th11（+ th12/th13）。
> thprac 现有支持：`thprac/src/thprac/thprac_th10.cpp`。

---

## 0. 本轮需要的东西

监控只需要“**可跨进程/在游戏内安全读取**的当前 ECL 函数名”，交付物是**地址 + 指针链 + 判据 + hook 语义**，不要求写实现。硬性要求同总括 §8。

> **口径**：本迭代最终产物是**实时显示「当前段落」**，读 ECL sub 只是手段。本作读到 sub 后需按白名单映射为“段落入口”再显示（见总括 §1/§2.1）；并需回答 **Q9（段落判定口径）**。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL 解释器命中点 | `0x44fb9f`（EHOOK 2B，注释 “freeze ECL sub time for st1/2/4 main during midboss”） | `thprac_th10.cpp:541` |
| 命中时执行对象寄存器 | `pCtx->Esi` | 同上 |
| 执行对象已有字段 | `*(float*)pCtx->Esi` = 当前 ECL 时间 | 同上（`:549`） |
| 主线判据（现成） | `GetMemContent(pCtx->Esi + 0x1014, 0x103c + 0x1444) == 1296` | 同上（`:545`） |
| ECL 数据区链 | `GetMemAddr(0x477704, 0x54, 0xC)` | `thprac_th10.cpp:2162`（`THSectionPatch`） |
| 指针常量 | `CHARA_ADDR=0x474c68`、`DIFF_ADDR=0x474c74`、`ENEMY_MANAGER_PTR=0x477704`、`PLAYER_PTR=0x477834`、`STAGE_NUM=0x474c7c` | `thprac_th10.cpp:16-21` |
| 其它对象指针 | GameManager `0x477810`、ReplayMgr `0x477838` | `docs/gametime/gametime_thxx_flags.md` §3 th10 |
| miss/bomb/边界钩子 | `th10_life_dec 0x426a1c`、`th10_bomb_dec 0x4259cf`、`th10_bomb_dec2 0x425c3e`、`th10_game_start 0x41798c` | `thprac_th10.cpp:3007-3024` |
| IGI 面板类 | `TH10InGameInfo`（`mMissCount`/`mBombCount`，显示在 `thprac_th10.cpp:622/626`） | `thprac_th10.cpp:567` |

---

## 2. Q1 当前 sub：索引还是指令指针？（待答）

- 在 `0x44fb9f` 命中时，`Esi` 是“主线脚本执行对象”。请回答：**该对象里“当前执行到哪个 sub/指令”存在哪个偏移？类型？**
- 若为索引（0..count-1），给出读取表达式；若为指令指针，给出“指针落在哪个 sub 区间”的判定方式（参照 th14 的 sub 表区间做法）。
- 期望：`subIdx = *(i32*)(esi + 0x??)`，或“无索引，仅指令指针 + sub 表区间”。

## 3. Q2 sub 表：索引 → 名字/起始（待答）

- 是否存在“sub 表”（`索引 → 名字(char*) / 起始指令`）？给出从**执行对象**或**全局**出发的完整指针链。
- 线索：`THSectionPatch` 用的 ECL 数据区 `[ENEMY_MANAGER_PTR(0x477704)+0x54]+0xC`；请确认该链上是否有 sub 表（`+0x8C` 一类偏移），或另给链路。
- 表项大小、`+0`/`+4` 字段语义、名字串形态（ASCII 长度前缀？）。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 **ECL 子调用栈**（栈顶=当前 sub）？给出链/遍历规则与深度上限。
- 若不存在，明确说明，并给出替代方案（离线主调关系表）。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn “main” 时，把主线 ECL **宿主对象**写入的 hook 点（地址 + 命中寄存器，如 `EAX`）。
- 可参考 TH15：`0x43bbc8` / `0x43d48e` / `0x43d6be`。

## 6. Q5 解释器命中点与寄存器语义（部分已知）

- 已有 `0x44fb9f`（2B），命中 `Esi`=执行对象。请给出：
  - 该地址所在函数入口与调用链（谁驱动、每帧几次）；
  - 该 hook 覆盖 2B 的指令语义；
  - 是否可直接用作“每帧读当前 sub”的稳定触发点。

## 7. Q6 名字来源（待答）

- 运行期 ECL 资源是否保留 **sub 名**？保留 → 直接读 `char*`；不保留 → 请给出“索引/偏移”字段，以便离线用白名单 CSV / `.ecl` 建名表。

## 8. Q7 机体/子机类型（已知）

- `CHARA_ADDR=0x474c68`、`DIFF_ADDR=0x474c74`（`thprac_th10.cpp:16-17`）。用于 IGI/后续日志。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：本作走 **(A) 调用栈祖先法** 还是 **(B) 顺序 latch 法**？（定义见总括 §6 Q9；thprac 倾向 A，需结合 `.ecl` 脚本确认。）

- 练习模式 `ECLJump`（`thprac_th10.cpp` 内）是否改变 sub 索引/名字判读。

---

## 10. 真机验证清单（本机为 macOS，无法执行）

- [ ] 道中 / boss / 不同 sub 处采样，函数名随进度推进且与实际一致。
- [ ] th10 无剧情分支，主线唯一；确认标题挂机 attract demo 下读值是否异常。
- [ ] 练习模式跳章后名字仍正确。
- [ ] 若依赖二级指针（`host→res→table`），确认各级非空、越界保护生效。

---

## 11. 交付格式

- Q1–Q6 给出**地址/指针链 + 判据 + 反汇编证据片段（地址+指令）**；Q7/Q8 视需要。
- 统一用 RVA 表述（本作基址固定 0x400000）。
- 结论回填本文件，并同步 `docs/logger/logger_re_disasm_overview.md` §5。

## 12. 工具与复现

- `pefile` 取指纹/节表 → `va2off` → `capstone`（`CS_ARCH_X86, CS_MODE_32`）只读反汇编。
- 切入点：从 `0x44fb9f` 反汇编其所在函数；从 `[0x477704]+0x54` 链展开 ECL 资源。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（requests）。汇总已知锚点；列出 Q1–Q8 待答项。未跑逆向。 |

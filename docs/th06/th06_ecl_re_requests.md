# TH06 ECL 段落监控（读取 ECL 当前位置为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th06.exe`（本体名 `東方紅魔郷.exe`），512000 字节，PE `TimeDateStamp = 1038721275`，`SizeOfCode = 430080`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。指纹与 `thprac/src/thprac/thprac_launcher_games_def.h:953` 识别表一致（本机实测）。
> ECL / decl 素材：**无**。A 代**没有 eclmap**（THTK `MAP/` 只有 `ECL_MAP_08.txt` 及以后），th06 的 `.ecl` 也未从 `.dat` 解出 —— 见 §12。
> 引擎代：**A 代（第一代引擎）**；同代参照：**th06 自身**（th07 / th08 按本作模板推进）。
> thprac 现有支持：`thprac/src/thprac/thprac_th06.cpp` —— 已有一个**章节判定案例**（books，见 §1），但它**不读 ECL 函数名**。
> ⚠️ **分支关**：th06 的 **stage4 存在路线分支**（`docs/req_logger.md:29` 点名「三作的 stage4 均存在路线分支」）。

---

## 0. 本轮需要的东西

监控只需要「**可跨进程/在游戏内安全读取**的当前 ECL 位置」，交付物是**地址 + 读取表达式 + 判据 + hook 语义**，不要求写实现。

> **口径差异（A 代）—— 不能照搬 th10–18 的模板**：
> - B/C 代（th10–18）监控的是「当前 sub 名」，链路 `host→thread→subIdx→sub 表→name`。
> - th06 的章节跳转是**往引擎里写一个时间值**（`ECLWarp(time)`，见 §1），不是 `ECLJump` 跳到具名 sub。
> - thprac 现有的 books 章节判定读的是**原始指令偏移**（与硬编码常量比较），不读函数名。
> - th06 的 ECL 格式里**有** `call sub_id`（证据见 §1），但**运行时是否暴露「当前 sub」未知**。
>
> 因此 **Q1 必须先回答「当前位置是 sub 索引 / 时间值 / 指令偏移」三选一**，Q2 起才有意义。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL 脚本基址（`SetBaseAddr` 目标） | `0x487e50`（被当 VFile 指针表用） | `thprac_th06.cpp:2915` |
| ECL 当前指令基址（运行期读） | `pCode = *(u32*)0x487e50` | `thprac_th06.cpp:3757/3788` |
| ECL warp 落点 | `0x5a5fb0`；`ECLWarp(time)` 只写**一个 int32 时间值** | `thprac_th06.cpp:1894-1897` |
| 时间轴解释器 hook（books 起点） | `0x41188A`（EHOOK 3B，`// timeline ins_4`）；命中时从 `pCtx->Ebp-0x14` 取当前指令指针 | `thprac_th06.cpp:3753-3758` |
| 时间轴解释器 hook（books 终点＝mid boss） | `0x411669`（EHOOK 3B，`// timeline ins_0`）；从 `pCtx->Ebp-0xC` 取 | `thprac_th06.cpp:3784-3790` |
| 时间轴冻结 hook | `0x412e31`（EHOOK 5B，`freeze timeline progress during st1/2/4/5 mid`） | `thprac_th06.cpp:483-495` |
| 解释器 opcode 分支 hook（候选命中点） | `0x41B27C`(init) / `0x409A10`(set timeout case 115) / `0x408DDA`(set boss mode case 101) / `0x411F88`(decrease time, update) | `thprac_th06.cpp:3657-3669` |
| 符卡 hook | `0x4096e8`(spellStart) / `0x409978`(spellCapture) / `0x412056`(spellTimeout) | `thprac_th06.cpp:3824/3836/3848` |
| books 章节判定常量 | 起点 `0xCDD4`、终点 `0xCE7C`、book 间距 `0x1C`（相对 `*(u32*)0x487e50` 的**指令偏移**） | `thprac_th06.cpp:3761/3790` |
| 章节名表（IGI 用） | `th06_spells_str[3][65]`，索引 64 = books（硬编码，非运行期读取） | `thprac_res.h:5`、`thprac_res.h:70` |
| 静态宿主对象 | `GameManager* const GAME_MANAGER = (GameManager*)0x69bca0`；结构体定义 | `thprac_th06.cpp:29`；`thprac_th06.h:66-121` |
| 关卡号 | `*(u32*)0x69d6d4`（写入点 `:3226`） | `thprac_th06.cpp:3756` |
| 难度 | `*(i8*)0x69bcb0` | `thprac_th06.cpp:1368` |
| 机体 / 子机 | `*(byte*)0x69D4BD` / `*(byte*)0x69D4BE` | `thprac_th06.cpp:1365-1366` |
| bomb 数（内存直读） | `*(int8_t*)0x69BCC4` | `thprac_th06.cpp:1363` |
| 当前是否符卡中 / 符卡 id | `*(bool*)0x5A5F90` / `*(byte*)0x5A5F98` | `thprac_th06.cpp:1410/1413` |
| miss / bomb / 一局开始 hook | `0x428DD9` / `0x4289CC`（**空 hook**）/ `0x41BDE8` | `thprac_th06.cpp:3650/3655/3641` |
| IGI 面板类 | `TH06InGameInfo`（miss/bomb 显示 `:1383-1389`，章节名行 `:1410-1428`） | `thprac_th06.cpp:1301` |
| 章节数表（前半/后半） | `mChapterSetup[7][2]` = `{{4,2},{2,2},{4,3},{4,5},{3,2},{2,0},{4,3}}` | `thprac_th06.cpp:970-978` |
| **sub 存在的证据（未接线）** | `EclRawInstrCallArgs { int32_t eclSub; … }`；整套 `EclRawInstr*` **只有头文件定义，`.cpp` 从未使用** | `thprac_th06.h:737-743`（全组 `:710-790`） |

> **段落入口候选值已现成**：`THStageWarp(ecl, stage, portion)`（`thprac_th06.cpp:2753-2910`）里逐关逐 portion 硬编码了 `ECLWarp(<time>)`。若 Q1 确认「当前位置＝时间值」，则这张表**直接就是** checkpoint 的「入口」列取值来源。

---

## 2. Q1 当前位置：sub 索引、时间值，还是指令偏移？（待答）

- 在 `0x41188A` / `0x411669` 命中时，`pCtx->Ebp-0x14` / `-0xC` 是「时间轴当前指令指针」。请回答：**引擎里「当前执行到哪」到底存在哪个结构、哪个偏移、什么类型？**
- 三选一（可并存，需说明关系）：
  - **(a) sub 索引** —— `subIdx = *(i32*)(<执行对象> + 0x??)`；
  - **(b) 时间值** —— 即 `ECLWarp` 写入的那类时间量（`0x5a5fb0` 是否是「当前时间」而非「warp 请求」？）；
  - **(c) 指令偏移/指针** —— 相对 `*(u32*)0x487e50` 的偏移（thprac 现在用的就是这个）。
- 期望：给出**完整读取表达式 + 类型 + 取值范围 + 写入点/读点证据**。

## 3. Q2 sub 表：是否存在？与「时间轴」是什么关系？（待答）

- 证据：`thprac_th06.h:737` 的 `EclRawInstrCallArgs.eclSub` 说明 ECL **有带 id 的 call 指令**。请确认：
  - 是否存在「sub 表」（索引 → 起始指令 / 名字）？给出从**全局或执行对象**出发的完整指针链；
  - 表项大小、字段语义；
  - **sub 表与时间轴的关系**：时间轴上的指令是否就是某个 sub 的 body？`call sub_id` 之后「当前 sub」怎么变？
- 若不存在 sub 表，明确说明，并给出替代（离线偏移↔段落映射表）。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 **ECL 子调用栈**（栈顶＝当前 sub）？给出链/遍历规则与深度上限。
- 若不存在，明确说明。**注意**：按总括 §6 Q9 的 latch 口径，函数栈**不参与**判定，本 Q 只作逆向事实保留。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn "main" 时，把主线 ECL **宿主对象**写入的 hook 点（地址 + 命中寄存器）。
- th06 的宿主疑似是**静态对象**（A 代特征：`GAME_MANAGER = 0x69bca0`，单级读，与 B/C 代的「堆对象 + 静态指针」不同），请确认。

## 6. Q5 解释器命中点与寄存器语义（部分已知）

- 已有候选（§1）：时间轴 `0x41188A`/`0x411669`、冻结 `0x412e31`、opcode 分支 `0x41B27C`/`0x409A10`/`0x408DDA`/`0x411F88`。
- 请给出：**每帧读「当前位置」的稳定触发点**是哪一个（或另给）；
  - 该地址所在函数的入口与调用链（谁驱动、每帧几次）；
  - 命中时哪个寄存器/栈帧槽＝执行对象（`Esi`/`Edi`/`pCtx->Ebp-0x??`）；
  - EHOOK 覆盖长度与该处指令语义。

## 7. Q6 名字来源（待答）

- 运行期 ECL 资源是否保留 **sub 名**（ASCII 串）？
- 保留 → 直接读 `char*`；不保留 → 说明「索引/偏移」字段，以便**离线**建「索引 ↔ 名字」表。
- **注意**：A 代**没有 eclmap**（§12），所以离线具名这条路目前不通 —— 若运行期也不保留名字，则「记录名」只能来自 thprac 自己的章节表（`th_sections_str` 等），需要一并说明可行性。

## 8. Q7 机体/子机类型（已知）

- `*(byte*)0x69D4BD`（a）/ `*(byte*)0x69D4BE`（b），合成 `(a<<1)|b`；难度 `*(i8*)0x69bcb0`。用于 IGI/后续日志（`thprac_th06.cpp:1365-1368/3773-3776`）。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：**沿用总括 §6 Q9 的定案「路过式 checkpoint（latch）」** —— 经过白名单条目即置值、不在表内 no-op（不回退 `main`）、首次命中前显示 `main`。**A 代不重新论证**，本作只需回答：**当前位置怎么取**（Q1/Q2）+ **每段入口取值**。

- th06 的练习跳转是 `ECLWarp(<time>)` / `THPatch`（`thprac_th06.cpp:2912-2925`）。请回答：**跳转后读「当前位置」是否会立刻反映新段落**（时间值是否被 warp 直接改写）？
- **入口列口径（已定）**：A 代的 checkpoint「入口」**不用 sub 名，改用时间值 / 指令偏移**（用户 2026-10-16 定）。请给出与 `ECLWarp` 入参对齐的取值。

---

## 10. 真机验证清单（E5 待做）

> 本机为 **Windows 11**，可编译、可真机运行（thprac 的 gametime 迭代已在真机跑过）。**本迭代不要求做** E5，仅列出待验证项。

- [ ] 道中 / boss / 不同章节处采样，读到的位置随进度推进且与实际一致。
- [ ] th06 **stage4 有路线分支**，确认分支下读值仍正确。
- [ ] 练习模式跳章（`ECLWarp`）后读值立即正确。
- [ ] 标题挂机 attract demo / replay 下读值是否异常。
- [ ] 若依赖二级指针，确认各级非空、越界保护生效。

---

## 11. 交付格式

- Q1–Q6 给出**地址/指针链 + 读取表达式 + 判据 + 反汇编证据片段（地址+指令）**；Q7/Q8 视需要。
- 统一用 RVA 表述（本作基址固定 `0x400000`）。
- 结论回填本文件，并同步 `docs/logger/logger_re_disasm_overview.md` §5。
- **入口列口径**：A 代 checkpoint 的「段落入口」记为**时间值 / 指令偏移**（非 sub 名）—— 与 th10–18 的 CSV 语义分叉，回填时需在工作流文档中显式标注。

## 12. 工具与复现

- 静态：`pefile` 取指纹/节表 → `va2off` → `capstone`（`CS_ARCH_X86, CS_MODE_32`）只读反汇编；切入点：从 `0x41188A` 反汇编其所在函数，从 `0x487e50` / `0x5a5fb0` 找写入点与读取点。
- **素材缺口（E1 前置，本迭代不做）**：
  1. `reasm/exes/` **没有 th06.exe** —— 本体在 `D:\C P\C2H2\[th06]\東方紅魔郷.exe`（512000 B，与识别表指纹一致）。
  2. th06 的 ECL 位于哪个 `.dat` 成员未确认（候选 `紅魔郷ST.DAT` 2.96 MB / `紅魔郷TL.DAT` 1.04 MB）；`thecl` 能否处理 A 代格式**未实测**。
  3. **THTK `MAP/` 无 `ECL_MAP_06.txt`** ⇒ 即便解出 `.ecl`，也拿不到具名 sub。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档（requests）。首次为 A 代建档：汇总 thprac 侧全部已知锚点（ECL 基址 / warp 落点 / 时间轴与 opcode hook / books 判定常量 / 静态宿主对象 / sub 证据）；把 Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一；登记素材缺口（无 exe、无 eclmap）。**未跑逆向**。 |

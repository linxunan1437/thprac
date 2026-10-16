# TH08 ECL 段落监控（读取 ECL 当前位置为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th08.exe`，840704 字节，PE `TimeDateStamp = 1095610188`，`SizeOfCode = 728064`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。指纹与 `thprac/src/thprac/thprac_launcher_games_def.h:971` 识别表一致（本机实测）。
> ECL / decl 素材：**无**。A 代**没有 eclmap**（THTK `MAP/` 无 `ECL_MAP_08.txt` 之外的 A 代 map —— 注意 `ECL_MAP_08.txt` 是 **th08**，与本作同号但 A 代格式）；`.ecl` 也未从 `.dat` 解出 —— 见 §12。
> 引擎代：**A 代（第一代引擎）**；同代参照：**th06**（`docs/th06/th06_ecl_re_requests.md`）。
> thprac 现有支持：`thprac/src/thprac/thprac_th08.cpp` —— **A 代三作里 ECL 线索最多的一作**（源码里直接写着 Timeline Struct / ECL Buffer 的注释，见 §1）。
> 本作**另有既有逆向成果**（gametime 主题，非 ECL）：`docs/th08/th08_re_disasm_report.md`、`docs/gametime/gametime_thxx_flags.md` §3。
> ⚠️ **分支关**：th08 的 **stage4 存在路线分支**（需求文档 `docs/req_logger.md:29` 点名「三作的 stage4 均存在路线分支」）。

---

## 0. 本轮需要的东西

监控只需要「**可跨进程/在游戏内安全读取**的当前 ECL 位置」，交付物是**地址 + 读取表达式 + 判据 + hook 语义**，不要求写实现。

> **口径差异（A 代）—— 不能照搬 th10–18 的模板**：
> - B/C 代（th10–18）监控「当前 sub 名」，链路 `host→thread→subIdx→sub 表→name`。
> - th08 的章节跳转是 `ECLWarp(time, offset, …)`，往 **Timeline 结构**里写若干 **(时间, 指令偏移) 对**（见 §1），不是 `ECLJump` 跳到具名 sub。
> - th08 的 ECL 格式里**确定有**带 id 的 sub（`ECLCallSub`，见 §1），但**运行时是否暴露「当前 sub」未知**。
>
> 因此 **Q1 必须先回答「当前位置是 sub 索引 / 时间值 / 指令偏移」三选一**。

---

## 1. 已知锚点

> **本作最大的便利**：thprac 源码里留了一段内存布局注释（`thprac_th08.cpp:3360-3366`）——
> ```
> 0xF54CF0: Timeline Struct
> 0x4ECCB8: ECL Buffer Pointer
> 0x4E4824: STD Buffer
> 0x4E483C & 0x4E4844: STD Time
> ```

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL Buffer 指针 | `pECL = *(i32*)0x4ECCB8` | `thprac_th08.cpp:1413`；注释 `:3363` |
| ECL 脚本基址（`SetBaseAddr` 目标） | `GetMemAddr(0x4ECCB8)` | `thprac_th08.cpp:2773` |
| **Timeline 结构** | `0xF54CF0`（thprac 源码注释直指） | `thprac_th08.cpp:3362` |
| ECL warp 落点 | `0xf54cf8`（＝ Timeline Struct + 8）；`ECLWarp(time1,offset1,time2,offset2,…)` 写 **(时间, 偏移) 对**，偏移为 `pECL + offset` | `thprac_th08.cpp:1407-1439` |
| STD Buffer / STD Time | `0x4E4824` / `0x4E483C` & `0x4E4844` | `thprac_th08.cpp:3364-3365` |
| 指令头结构 | `__th08_ins_header`（含 `time` / `length`）；`ECLTimeFix` 用它逐条遍历指令 | `thprac_th08.cpp:1440-1447` |
| 解释器 opcode 分支 hook（候选命中点） | `0x437AF3`(init) / `0x41CB32`(set timeout case 132) / `0x41C44F`(set boss mode case 127) / `0x42F34D`(decrease time, update) | `thprac_th08.cpp:3277-3287` |
| 符卡捕获 hook | `0x416265` | `thprac_th08.cpp:3250` |
| **sub 存在的证据（已接线）** | `ECLCallSub(ecl, offset, sub_id, …)` 写 opcode **`0x34`**（`0x00100034`）+ `sub_id`；`ECLSetTime(offset, time, sub_id)` 写 `0x00140086` + time + sub_id | `thprac_th08.cpp:1448-1468` |
| 静态宿主对象 | `GameManager` `0x160F508` / `Supervisor` `0x17CE758`（由 `GAMEMODE_ADDR = 0x17CE8B0` 推得） | `docs/gametime/gametime_thxx_flags.md` §3 th08 |
| 章节数表（前半/后半） | `mChapterSetup[10][2]` = `{{1,1},{4,0},{2,1},{4,2},{4,2},{3,2},{2,0},{2,0},{3,4},{0,0}//LW}` | `thprac_th08.cpp:799-810` |
| miss / bomb / 一局开始 hook | `0x43BDD2`(enter_game，清计数) / `0x44CA94`+`0x44CA6A`(bomb / deadbomb，**空 hook**) / `0x44ABE9`(desolve) | `thprac_th08.cpp:3242/3269/3271/3273` |
| 符卡 id（用于捕获判定） | `*(i32*)0x4EA678` | `thprac_th08.cpp:3263` |
| IGI 面板类 | `TH08InGameInfo`（`mMissCount`/`mBombCount`/`mSCCount`/`mLSCCount`/`mDesolveCount`） | `thprac_th08.cpp:984` |
| 既有三态已验证字段（非 ECL） | gamemode `0x17CE8B0`、pause `0x164D0BA`、replay `(0x164D0B4 & 8)` | `docs/gametime/gametime_thxx_flags.md` §3 th08 |

> **段落入口候选值已现成**：`THStageWarp(ecl, stage, portion)`（`thprac_th08.cpp:1513-…`）里逐关逐 portion 硬编码了 `ECLWarp(<time>, <offset>, …)`。若 Q1 确认「当前位置＝时间值 / 指令偏移」，这张表**直接就是** checkpoint「入口」列的取值来源。
> ⚠️ **th08 没有时间轴冻结 hook**（th06 有 `0x412e31`、th07 有 `0x4207a1`），故 Q5 的命中点候选只能从上面的 opcode 分支 hook 里挑。

---

## 2. Q1 当前位置：sub 索引、时间值，还是指令偏移？（待答）

- th08 有**显式 Timeline 结构**（`0xF54CF0`）。请回答：**引擎里「当前执行到哪」存在哪个结构、哪个偏移、什么类型？**
- 三选一（可并存，需说明关系）：
  - **(a) sub 索引** —— `subIdx = *(i32*)(<执行对象> + 0x??)`（th08 有 `ECLCallSub`，故此项很可能存在）；
  - **(b) 时间值** —— Timeline 结构里的当前时间（与 `0x4E483C`/`0x4E4844` 的 STD Time 是什么关系？）；
  - **(c) 指令偏移/指针** —— 相对 `*(u32*)0x4ECCB8` 的偏移。
- 期望：给出**完整读取表达式 + 类型 + 取值范围 + 写入点/读点证据**。

## 3. Q2 sub 表：是否存在？与「时间轴 / Timeline」是什么关系？（待答）

- th08 **确定有** `call sub_id`（opcode `0x34`，`thprac_th08.cpp:1448`）。请确认：
  - 是否存在「sub 表」（索引 → 起始指令 / 名字）？给出完整指针链；
  - 表项大小、字段语义；
  - **sub 表与 Timeline 的关系**：`call sub_id` 后「当前 sub」怎么变？Timeline 的 (时间, 偏移) 对指向的是指令还是 sub？
- 若不存在 sub 表，明确说明，并给出替代（离线偏移↔段落映射表）。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 **ECL 子调用栈**（栈顶＝当前 sub）？给出链/遍历规则与深度上限。
- 若不存在，明确说明。**注意**：按总括 §6 Q9 的 latch 口径，函数栈**不参与**判定，本 Q 只作逆向事实保留。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn "main" 时，把主线 ECL **宿主对象**写入的 hook 点（地址 + 命中寄存器）。
- th08 的宿主疑似是**静态对象**（A 代特征：`GameManager 0x160F508` / `Supervisor 0x17CE758`，单级读），请确认。

## 6. Q5 解释器命中点与寄存器语义（部分已知）

- 已有候选（§1）：opcode 分支 `0x437AF3`/`0x41CB32`/`0x41C44F`/`0x42F34D`。
- 请给出：**每帧读「当前位置」的稳定触发点**是哪一个（或另给）；
  - 该地址所在函数的入口与调用链（谁驱动、每帧几次）；
  - 命中时哪个寄存器/栈帧槽＝执行对象；
  - EHOOK 覆盖长度与该处指令语义。
- 提示：本作**没有**现成的 timeline 冻结 hook，若 opcode 分支 hook 都不合适，请指出从 Timeline 结构（`0xF54CF0`）反推的路径。

## 7. Q6 名字来源（待答）

- 运行期 ECL 资源是否保留 **sub 名**（ASCII 串）？
- 保留 → 直接读 `char*`；不保留 → 说明「索引/偏移」字段，以便**离线**建「索引 ↔ 名字」表。
- **注意**：A 代**没有 eclmap**（§12），离线具名这条路目前不通 —— 若运行期也不保留名字，「记录名」只能来自 thprac 自己的章节表（`th_sections_str` 等）。

## 8. Q7 机体/子机类型（待答）

- `SHOTTYPE_ADDR = 0x164d0b1`、`DIFF_ADDR = 0x160f538`（`thprac_th08.cpp:15-16`）。请确认「机体 × 子机」的组合编码（th08 是 4 种组合 × 2 队，见 `THPRAC_GAMEROLL_TH08_SHOTTYPES`）。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：**沿用总括 §6 Q9 的定案「路过式 checkpoint（latch）」** —— 经过白名单条目即置值、不在表内 no-op（不回退 `main`）、首次命中前显示 `main`。**A 代不重新论证**，本作只需回答：**当前位置怎么取**（Q1/Q2）+ **每段入口取值**。

- th08 的练习跳转是 `ECLWarp` / `THPatch`（`thprac_th08.cpp:2773` 附近）。请回答：**跳转后读「当前位置」是否会立刻反映新段落**？
- **入口列口径（已定）**：A 代的 checkpoint「入口」**不用 sub 名，改用时间值 / 指令偏移**（用户 2026-10-16 定）。请给出与 `ECLWarp` 入参对齐的取值。

---

## 10. 真机验证清单（E5 待做）

> 本机为 **Windows 11**，可编译、可真机运行。**本迭代不要求做** E5，仅列出待验证项。

- [ ] 道中 / boss / 不同章节处采样，读到的位置随进度推进且与实际一致。
- [ ] th08 **stage4 有路线分支**（Final A / Final B），确认各分支下读值仍正确。
- [ ] 练习模式跳章（`ECLWarp`）后读值立即正确。
- [ ] 标题挂机 attract demo / replay 下读值是否异常。
- [ ] 注意 th08 的 pause/replay 在 `GameManager(0x160F508)` 而非 `0x17CE8xx` 一带（`docs/gametime/gametime_thxx_flags.md` §3 已纠正过这个错误假设）。

---

## 11. 交付格式

- Q1–Q6 给出**地址/指针链 + 读取表达式 + 判据 + 反汇编证据片段（地址+指令）**；Q7/Q8 视需要。
- 统一用 RVA 表述（本作基址固定 `0x400000`）。
- 结论回填本文件，并同步 `docs/logger/logger_re_disasm_overview.md` §5。
- **入口列口径**：A 代 checkpoint 的「段落入口」记为**时间值 / 指令偏移**（非 sub 名）—— 与 th10–18 的 CSV 语义分叉，回填时需在工作流文档中显式标注。

## 12. 工具与复现

- 静态：`pefile` 取指纹/节表 → `va2off` → `capstone`（`CS_ARCH_X86, CS_MODE_32`）只读反汇编；切入点：从 `0xF54CF0`（Timeline 结构）与 `0x4ECCB8`（ECL Buffer）的 xref 反推，再看 `0x41CB32`/`0x41C44F` 所在函数。
- **素材缺口（E1 前置，本迭代不做）**：
  1. `reasm/exes/` 只有 `th08.exe`，**没有 th08 的 `.ecl`/`.decl`**。
  2. th08 的 ECL 位于哪个 `.dat` 成员未确认（`th08.dat`）；`thecl` 能否处理 A 代格式**未实测**。
  3. ⚠️ **`ECL_MAP_08.txt` 是 th08 的 eclmap，但它是 th10–18 那批 B/C 代流程留下的** —— 需实测确认它能否套用在 A 代 th08 的 ECL 上（大概率**不能**，A 代格式不同）。
- 既有可复用工具：`reasm/th08/ghidra_scripts/{Th08Dump,Th08Scan}.java`、`reasm/th08/tools/probe08.py`。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档（requests）。为 A 代建档：汇总 thprac 侧全部已知锚点（Timeline 结构 `0xF54CF0` / ECL Buffer `0x4ECCB8` / STD / `ECLCallSub` sub 证据 / 静态宿主对象 / 章节数表）；把 Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一；登记素材缺口（无 `.ecl`、A 代无 eclmap）。**未跑逆向**。 |

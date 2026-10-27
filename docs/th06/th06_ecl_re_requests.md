# TH06 ECL 段落监控（读取 ECL 当前位置为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th06.exe`（本体名 `東方紅魔郷.exe`），512000 字节，PE `TimeDateStamp = 1038721275`，`SizeOfCode = 430080`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。指纹与 `thprac/src/thprac/thprac_launcher_games_def.h:953` 识别表一致（本机实测）。
> ECL / decl 素材：**2026-10-21 已备齐**（`../exes/ecl/th06/ecldata1..7.ecl` + `../exes/ecl_dump/th06/ecldata1..7.decl`，`thecl -d6`、无 eclmap ⇒ `ins_*` / `SubN`）—— 见 §12；本作**仍无 eclmap**（THTK `MAP/` 只有 `ECL_MAP_08.txt` 及以后）。
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

## 2. Q1 当前位置：sub 索引、时间值，还是指令偏移？（**已答** → 报告 §2）

> **结论摘要（✅ 静态定案、🟡 未真机）**：三选一＝ **(b) 时间值 + (c) 指令指针，两者并存于「全局唯一一条时间轴」**；**(a) 没有独立游标**。
> `mgr = ENEMY_MANAGER = 0x4B79C8`（静态）；**`tlInstr = *(u32*)(mgr+0xEE5DC)`（＝`0x5A5FA4`，当前时间轴指令的绝对地址）**、**`tlTime = *(i32*)(mgr+0xEE5E8)`（＝`*(i32*)0x5A5FB0`，当前时间值＝帧）**。
> 时间轴更新 `0x411530`（每帧一次）：`current == instr->time` 执行、`current > time` 跳过（`+= size`）、`current < time` 等待（`0x4115D3`/`0x411CEF`/`0x411D15`）。
> 敌机侧另有 `EnemyEclContext`（`enemy+0x990`）：`+0` = `currentInstr`、`+0x0C` = 局部时间、`+0x48` = `subId`（`enemy+0x9D8`）—— **章节定位不用它；boss 相位相关口径已撤下（2026-10-27）**。

- 在 `0x41188A` / `0x411669` 命中时，`pCtx->Ebp-0x14` / `-0xC` 是「时间轴当前指令指针」。请回答：**引擎里「当前执行到哪」到底存在哪个结构、哪个偏移、什么类型？**
- 三选一（可并存，需说明关系）：
  - **(a) sub 索引** —— `subIdx = *(i32*)(<执行对象> + 0x??)`；
  - **(b) 时间值** —— 即 `ECLWarp` 写入的那类时间量（`0x5a5fb0` 是否是「当前时间」而非「warp 请求」？）；
  - **(c) 指令偏移/指针** —— 相对 `*(u32*)0x487e50` 的偏移（thprac 现在用的就是这个）。
- 期望：给出**完整读取表达式 + 类型 + 取值范围 + 写入点/读点证据**。

## 3. Q2 sub 表：是否存在？与「时间轴」是什么关系？（**已答** → 报告 §3）

> **结论摘要（✅ 静态定案、🟡 未真机）**：**sub 表存在**，与时间轴是**同一文件里的两张表**，都经静态 ECL 管理器 `0x487E50` 取：
> `pCode = *(u32*)0x487E50`（当前关 ECL 缓冲）、`subTable = *(u32*)(0x487E50+4)`（＝`pCode+0x10`）、`timelineStart = *(u32*)(0x487E50+8)`（＝`pCode + *(u32*)(pCode+4)`）。
> **文件头（7 份 `.ecl` 实测）**：`+0` i16 `subCount`、`+2` i16（0）、`+4` u32 **时间轴偏移**、`+8`/`+0xC`（0）、`+0x10` u32[subCount] **sub 偏移表**；加载器 `0x407340` 把 `+4` 与该表**就地绝对化**。
> `subStart(id) = *(u32*)(subTable + id*4)`；**`call sub`＝opcode `0x23`**（`0x407AEC`）、**`ret`＝`0x24`**（`0x407BBD`）、sub 设置器 `0x407440`。
> 关系：**时间轴**是整关「总导演脚本」（按时间推进，op 0–7 生成 sub 实例）；**sub** 是被生成的脚本内容。**两张表都没有名字。**

- 证据：`thprac_th06.h:737` 的 `EclRawInstrCallArgs.eclSub` 说明 ECL **有带 id 的 call 指令**。请确认：
  - 是否存在「sub 表」（索引 → 起始指令 / 名字）？给出从**全局或执行对象**出发的完整指针链；
  - 表项大小、字段语义；
  - **sub 表与时间轴的关系**：时间轴上的指令是否就是某个 sub 的 body？`call sub_id` 之后「当前 sub」怎么变？
- 若不存在 sub 表，明确说明，并给出替代（离线偏移↔段落映射表）。

## 4. Q3 函数栈（**已答** → 报告 §4）

> **结论摘要（✅ 静态定案、🟡 未真机）**：**存在可遍历的显式调用栈**（在**敌机对象**里）：基址 `enemy+0x9DC`、**帧长 `0x4C`**（＝整份 `EnemyEclContext` 快照）、深度 `*(i32*)(enemy+0xC3C)`、**上限 8（0..7）**；`call`(0x23) 压栈、`ret`(0x24) 弹栈；`enemy+0xE52` bit2 置位时**不压栈**（遍历须做保护）。帧 `+0`＝调用者指令、`+0x48`＝调用者 sub id。**仅事实记录，不参与判定。**

- 是否存在可遍历的 **ECL 子调用栈**（栈顶＝当前 sub）？给出链/遍历规则与深度上限。
- 若不存在，明确说明。**注意**：按总括 §6 Q9 的 latch 口径，函数栈**不参与**判定，本 Q 只作逆向事实保留。

## 5. Q4 主线宿主捕获点（**已答** → 报告 §5）

> **结论摘要（✅ 静态定案、🟡 未真机）**：**A 代是静态对象，无需捕获 hook**。`ENEMY_MANAGER = 0x4B79C8`（`0xEE5EC` B）、`ECL_MANAGER = 0x487E50`；管理器由链元素 `0x5A5FB4`（calc，优先级 9，回调 `0x4123E0`）驱动，注册于 `0x412320`；**每次进关由 `0x410E30` 全量清零**（`rep stosd 0x3B97B`＝`0xEE5EC` B）⇒ 时间轴指针/时间一并归零、重新从 `0x487E58` 装入。**不需要「主线宿主」这个概念**（章节进度全在静态管理器的时间轴上）。

- 进关 / spawn "main" 时，把主线 ECL **宿主对象**写入的 hook 点（地址 + 命中寄存器）。
- th06 的宿主疑似是**静态对象**（A 代特征：`GAME_MANAGER = 0x69bca0`，单级读，与 B/C 代的「堆对象 + 静态指针」不同），请确认。

## 6. Q5 解释器命中点与寄存器语义（**已答** → 报告 §6）

> **结论摘要（✅ 静态定案、🟡 未真机）**：**每帧唯一驱动点＝时间轴更新 `0x411530`**（`0x4123E0` 在 `0x4123F0` 调用，`ECX = mgr = 0x4B79C8`）；时钟推进在 `0x412E07`–`0x412E31`（`ZunTimer::Tick` = `0x424285`，`EDX` = 推进前的 `current`）。敌机 VM `0x4074A0`（`ECX=0x487E50`、`[esp+4]=Enemy*`，跳表 `0x40A9A4`，opcode 1..0x87）每活跃敌机每帧一次。**thprac 已占 `0x412E31`(5B)/`0x41188A`/`0x411669` ⇒ 推荐完全不挂 hook（IGI 渲染直接读静态地址）；必须挂时用未被占用的 `0x411530` 入口（9B）。**

- 已有候选（§1）：时间轴 `0x41188A`/`0x411669`、冻结 `0x412e31`、opcode 分支 `0x41B27C`/`0x409A10`/`0x408DDA`/`0x411F88`。
- 请给出：**每帧读「当前位置」的稳定触发点**是哪一个（或另给）；
  - 该地址所在函数的入口与调用链（谁驱动、每帧几次）；
  - 命中时哪个寄存器/栈帧槽＝执行对象（`Esi`/`Edi`/`pCtx->Ebp-0x??`）；
  - EHOOK 覆盖长度与该处指令语义。

## 7. Q6 名字来源（**已答** → 报告 §7）

> **结论摘要（❌ 运行期不保留名字；✅ 离线已可具名）**：加载器 `0x407340` 全文无字符串处理；`call`/生成器只传**索引**；exe 内 ECL 相关串只有文件名（表 `0x4764C4`）。
> **但 2026-10-21 起 `.ecl`/`.decl` 已备齐**：`SubN` ＝ sub 索引 N（`thecl` 无 eclmap 时自动命名），本报告已核对 7/7 文件 `subCount` 与 `decl` 的 `sub` 数全等、逐 sub 指令数差恒为 1（末尾 12B 终止哨兵 `{-1,-1,12}`）⇒ **`pCode` 偏移 ↔ sub 索引 ↔ `SubN` ↔ decl 文本形成闭环**，可离线建「偏移 ↔ 段落」表。**记录名仍取 thprac 章节表；入口列仍记时间值。**

- 运行期 ECL 资源是否保留 **sub 名**（ASCII 串）？
- 保留 → 直接读 `char*`；不保留 → 说明「索引/偏移」字段，以便**离线**建「索引 ↔ 名字」表。
- **注意**：A 代**没有 eclmap**（§12），所以离线具名这条路目前不通 —— 若运行期也不保留名字，则「记录名」只能来自 thprac 自己的章节表（`th_sections_str` 等），需要一并说明可行性。

## 8. Q7 机体/子机类型（**已答** → 报告 §8）

> **结论摘要（✅ 地址全部有效）**：`character = *(u8*)0x69D4BD`（47 处 xref）、`shotType = *(u8*)0x69D4BE`（40）、`difficulty = *(i32*)0x69BCB0`（79，**解释器 `0x4074F6` 直接拿它做 `1<<diff` 难度跳过掩码**）、`currentStage = *(u32*)0x69D6D4`（**1-based**，＝`GameManager+0x1A34`，索引 ECL 文件名表 `0x4764C4`）；`GAME_MANAGER = 0x69BCA0`（`+0x10`=diff、`+0x1C`=isInReplay=`0x69BCBC`、`+0x24`=bombsUsed 低字节=`0x69BCC4`）；`bossPresent = GUI(0x69BC30)+0x20 = 0x69BC50`。

- `*(byte*)0x69D4BD`（a）/ `*(byte*)0x69D4BE`（b），合成 `(a<<1)|b`；难度 `*(i8*)0x69bcb0`。用于 IGI/后续日志（`thprac_th06.cpp:1365-1368/3773-3776`）。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（**已答** → 报告 §9/§10）

> **Q8 结论摘要（✅ 静态定案、🟡 未真机）**：`ECLWarp(t)` 就是 `*(i32*)0x5A5FB0 = t`，而 `0x5A5FB0 = mgr+0xEE5E8 = timelineTime.current` ⇒ **直写时间轴的当前时间**；下一帧 `0x4115D3` 起的比较循环把 `tlInstr` 快进到第一条 `time >= t`（`0x411D15` 的 `+= size`）⇒ **跳转后读时间/落点立刻正确**。⚠️ `ECLWarp` **不重置 `tlInstr`**：同局内把时间往回改不会让指针回退，**必须靠进关重载**（管理器清零）保证起点。`THPatch` 的 `pair{}`/`ECLSetTime`/`ECLSetHealth`/`ECLStall` 改的是 sub 脚本指令，不动时间轴。
> **Q9 结论摘要（✅）**：沿用 latch（不重新论证）。**th06 读取方案＝主键「时间轴指令偏移精确相等」**（`tlOff = tlInstr − pCode`，与附录 C/D 的落点逐条比对）、**辅助校验用时间值** `tlTime`；**boss 相位相关口径已于 2026-10-27 撤下**。**入口列口径＝时间值**（＝`ECLWarp` 入参），与 th07/th08 的「时间轴位置」列同义。**中boss 窗口（st1/st2/st4/st5 缺 `boss_wait`，thprac `TH_TIMELOCK` 会人为冻结）需按 th16 §5.7 处理**。

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
- **素材（2026-10-21 已补齐，E1 已据此完成）**：
  1. `../exes/th06.exe`（512000 B / `ts=1038721275`，与 `thprac_launcher_games_def.h:459` 识别表一致）。
  2. `../exes/ecl/th06/ecldata1..7.ecl`（来自 `紅魔郷ST.DAT`，7/7 解出）。
  3. `../exes/ecl_dump/th06/ecldata1..7.decl`（`thecl -d6`，7/7 成功；**无 eclmap** ⇒ `ins_*`，sub 名自动生成 `Sub0..SubN`）。
  4. **仍缺**：THTK `MAP/` 无 `ECL_MAP_06.txt` ⇒ 指令级具名（`ins_N` → 语义名）不可得；但**sub 级具名已够用**（`SubN` ＝ 索引 N）。
- 复现脚本与步骤见 `docs/th06/th06_ecl_re_report.md` §15（含自写离线 ECL 解码器）。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档（requests）。首次为 A 代建档：汇总 thprac 侧全部已知锚点（ECL 基址 / warp 落点 / 时间轴与 opcode hook / books 判定常量 / 静态宿主对象 / sub 证据）；把 Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一；登记素材缺口（无 exe、无 eclmap）。**未跑逆向**。 |
| 2026-10-22 | **阶段 E1 静态逆向定案（🟡 未真机）**：新建 `docs/th06/th06_ecl_re_report.md`（Q1–Q9 + 统一指针链 + ECL 文件结构 + 时间轴 opcode 全表 + 40 个道中 portion / 70 个 boss section 的 warp→落点→sub 表 + 工具复现 + 真机清单）。本节 §2–§9 各 Q 已回填「已答 + 结论摘要」，§12 素材缺口改为「已补齐（仅缺 eclmap）」。**核心结论**：**Q1 当前位置＝全局唯一时间轴的「时间值 `mgr+0xEE5E8`（=`0x5A5FB0`）+ 指令指针 `mgr+0xEE5DC`（=`0x5A5FA4`）」**，敌机另有 `enemy+0x990` 的 `EnemyEclContext`；**Q2** sub 表 `*(u32*)(0x487E50+4)`、时间轴起点 `*(u32*)(0x487E50+8)`，**文件头 `+0` subCount / `+4` 时间轴偏移 / `+0x10` sub 表**（加载器 `0x407340` 绝对化），`call`=0x23/`ret`=0x24；**Q3** 调用栈 `enemy+0x9DC`/帧 `0x4C`/深度 `+0xC3C`/上限 8；**Q4** 宿主静态（`0x4B79C8`/`0x487E50`，链元素 `0x5A5FB4`→`0x4123E0`，进关由 `0x410E30` 清零）⇒ **无需捕获**；**Q5** 每帧驱动 `0x411530`（`ECX=mgr`），时钟 `0x412E07`–`0x412E31`（`0x424285`），**推荐不挂 hook**（备选 `0x411530` 8B；`0x412E31`/`0x41188A`/`0x411669` 已被 thprac 占用）；**Q6** 运行期无名，**但 `.decl` 已备齐 ⇒ `SubN`＝索引 N，离线可具名**；**Q7** thprac 地址全部有效（另确认 `difficulty` 作 `1<<diff` 难度掩码、`currentStage` 1-based 索引 ECL 文件名表 `0x4764C4`）；**Q8** `ECLWarp` 直写时间、下一帧快进、**指针不回退**；**Q9** latch + **主键「落点偏移精确相等」** + boss 相位副键 `SubN`；入口列仍记**时间值**。**未真机三态；未生成 `docs/logger/th06_checkpoints.csv`。** |

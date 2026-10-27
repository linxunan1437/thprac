# TH07 ECL 段落监控（读取 ECL 当前位置为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th07.exe`，650752 字节，PE `TimeDateStamp = 1066942337`，`SizeOfCode = 572928`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。指纹与 `thprac/src/thprac/thprac_launcher_games_def.h:959` 识别表一致（本机实测）。
> ECL / decl 素材：**无**。A 代**没有 eclmap**（THTK `MAP/` 无 `ECL_MAP_07.txt`），`.ecl` 也未从 `.dat` 解出 —— 见 §12。
> 引擎代：**A 代（第一代引擎）**；同代参照：**th06**（`docs/th06/th06_ecl_re_requests.md`）。
> thprac 现有支持：`thprac/src/thprac/thprac_th07.cpp`（有 ECL 补丁 + 时间轴 hook，但**不读 ECL 函数名**）。
> 本作**另有既有逆向成果**（gametime 主题，非 ECL）：`docs/th07/th07_re_disasm_report.md`、`docs/gametime/gametime_thxx_flags.md` §3。
> ⚠️ **分支关**：th07 的 **stage4 存在路线分支**（＝Prismriver 三姐妹选择；`thprac_th07.cpp:589` 的冻结 hook 注释即 `st4 Lily`）。

---

## 0. 本轮需要的东西

监控只需要「**可跨进程/在游戏内安全读取**的当前 ECL 位置」，交付物是**地址 + 读取表达式 + 判据 + hook 语义**，不要求写实现。

> **口径差异（A 代）—— 不能照搬 th10–18 的模板**：
> - B/C 代（th10–18）监控「当前 sub 名」，链路 `host→thread→subIdx→sub 表→name`。
> - th07 的章节跳转是 `ECLTimeWarp(count, time)`，往一张**时间表**里写 count 个时间值（见 §1），不是 `ECLJump` 跳到具名 sub。
> - th07 的 ECL 格式里**确定有**带 id 的 sub（`ECLCallSub`，见 §1），但**运行时是否暴露「当前 sub」未知**。
>
> 因此 **Q1 必须先回答「当前位置是 sub 索引 / 时间值 / 指令偏移」三选一**。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL 脚本基址（`SetBaseAddr` 目标） | `GetMemAddr(0x1347938)` | `thprac_th07.cpp:1841` |
| ECL warp 落点 | `GetMemAddr(0x9a9af8, 0x9545fc)`；`ECLTimeWarp(count, time)` 写 **count 个 uint32 时间值**，步长 `0x10` | `thprac_th07.cpp:891-898` |
| 时间轴冻结 hook | `0x4207a1`（EHOOK 5B，`freeze timeline progress during st4 Lily`）；命中时 `pCtx->Eip = 0x4207a6` 跳过 | `thprac_th07.cpp:589-598` |
| 解释器 opcode 分支 hook（候选命中点） | `0x42D17E`(init) / `0x414FE6`(set timeout case 114) / `0x413D06`(set boss mode case 99) / `0x41FFD5`(decrease time, update) | `thprac_th07.cpp:2233-2243` |
| **sub 存在的证据（已接线）** | `ECLCallSub(ecl, offset, ecl_time, sub_id)` 写 opcode **`0x29`**（`0x00100029`）+ `sub_id` | `thprac_th07.cpp:918-923` |
| 静态宿主对象 | `GameManager* GAME_MANAGER = (GameManager*)0x00626270`；`Supervisor* SUPERVISOR = (Supervisor*)0x575950` | `thprac_th07.cpp:14-15` |
| 关卡号 | `STAGE_NUM = 0x1347fc8`；冻结 hook 里用 `GetMemContent(STAGE_NUM) != 4` 判 st4 | `thprac_th07.cpp:25`、`:590` |
| 敌方管理器 | `ENEMY_MANAGER = 0x9a9b00` | `thprac_th07.cpp:20` |
| 输入 | `INPUT_ADDR = 0x4B9E4C` | `thprac_th07.cpp:17` |
| 章节数表（前半/后半） | `mChapterSetup[8][2]` = `{{2,1},{1,1},{2,1},{4,4},{3,1},{2,0},{5,3},{5,3}}`（含 Extra/Phantasm 两关） | `thprac_th07.cpp:411-420` |
| miss / bomb / 一局开始 hook | `0x42EB08`(enter_game，清计数) / `0x440B9D`(bomb，**空 hook**) / `0x441DA4`(border break) | `thprac_th07.cpp:2223/2228/2230` |
| IGI 面板类 | `TH07InGameInfo`（`mMissCount` / `mBombCount` / `mBorderBreakCount`） | `thprac_th07.cpp:615` |
| 既有三态已验证字段（非 ECL） | gamemode `0x575AA4`、pause `0x62F64C`、replay `(0x62F648 & 8)` | `docs/gametime/gametime_thxx_flags.md` §3 th07 |

> **段落入口候选值已现成**：`THStageWarp(ecl, stage, portion)`（`thprac_th07.cpp:975-1133`）里逐关逐 portion 硬编码了 `ECLTimeWarp(<count>, <time>)`。若 Q1 确认「当前位置＝时间值」，这张表**直接就是** checkpoint「入口」列的取值来源。

---

## 2. Q1 当前位置：sub 索引、时间值，还是指令偏移？（**已答** → 报告 §2）

> **结论摘要（✅ 静态定案、🟡 未真机）**：三选一＝ **(b) 时间值 + (c) 指令指针，两者并存、按 timeline 各一份**；**(a) sub 索引这个游标不存在**。
> 管理器 `ENEMY_MANAGER = 0x9A9B00`（静态）；第 i 条 timeline 状态对象 `TL_i = 0x9A9B00 + 0x9545F4 + i*0x10`（物理上限 16 槽）：
> `TL_i+0x8 = i32 当前时间`（**章节定位量**，thprac `ECLTimeWarp` 写这里）、`TL_i+0xC = u32 当前指令指针`、`TL_i+0x4 = f32 小数累加器`。
> `count = *(u16*)(*(u32*)0x1347938 + 2)`；`tlStart[i] = *(u32*)(*(u32*)0x1347938 + 4 + 4i)`。
> 读取（可直接用）：`time = *(i32*)(0x12FE0FC)`（= `mgr+0x9545FC` = `TL_0+8`）、`ip = *(u32*)(0x12FE100)`。
> 写点：`TL+0xC` ← `0x420787`（装入 tlStart）/`0x41FCFD`（`+= size`）；`TL+8` ← `0x41EF13`（重置 0）/`0x422FE5` 链（`ECLTimeWarp`）/`0x4395D8`（进位）。解释器每帧比较 `TL+8` 与指令时间（`0x41F6FC`–`0x41F723`）。
> 证据：主循环 `0x420620`（`0x42072B`–`0x4207A6`）、解释器 `0x41F6F0`（`0x41F6FC`–`0x41F74C`）、opcode 跳表 `0x41FD2F`。

- th07 有**显式的时间轴**（`ECLTimeWarp` 写的那张表，步长 `0x10`）。请回答：**引擎里「当前执行到哪」存在哪个结构、哪个偏移、什么类型？**
- 三选一（可并存，需说明关系）：
  - **(a) sub 索引** —— `subIdx = *(i32*)(<执行对象> + 0x??)`（th07 有 `ECLCallSub`，故此项很可能存在）；
  - **(b) 时间值** —— 时间轴表里的当前时间；
  - **(c) 指令偏移/指针** —— 相对 `*(u32*)0x1347938` 的偏移。
- 期望：给出**完整读取表达式 + 类型 + 取值范围 + 写入点/读点证据**。

## 3. Q2 sub 表：是否存在？与「时间轴」是什么关系？（**已答** → 报告 §3）

> **结论摘要（✅ 静态定案、🟡 未真机）**：**sub 表存在**，且与时间轴表是**两张独立的表**，都经同一全局 `0x1347938`：
> ① 时间轴表 = ECL 缓冲本身：`buf = *(u32*)0x1347938`；`count = *(u16*)(buf+2)`；`tlStart[i] = *(u32*)(buf+4+4i)`（主循环据此启动 timeline）。
> ② **sub 表 = `*(u32*)(0x1347938+4)`**；`subStart[id] = *(u32*)(subTable + id*4)`（项 **4B、纯指针**；装载函数 `0x40E530`）。
> `call sub id`(opcode **0x29**)：case `0x411F20` —— 先把返回游标压栈（`E+0x8FC+depth*0x218`），再 `E+0x6E4 = subStart[id]`（跳到 sub 首指令），深度 `E+0x2A7C`+1；`ret`(opcode **0x2A**) case `0x411FE6` 弹栈。
> **「当前 sub」＝ `E+0x6E4`（当前 sub 指令指针）**；sub 表**无名字**（§7 Q6）。
> 章节定位仍**走时间轴**（sub 只在敌方/被调脚本层）。

- th07 **确定有** `call sub_id`（opcode `0x29`，`thprac_th07.cpp:918`）。请确认：
  - 是否存在「sub 表」（索引 → 起始指令 / 名字）？给出完整指针链；
  - 表项大小、字段语义；
  - **sub 表与时间轴的关系**：时间轴上的指令与 sub body 是什么关系？`call sub_id` 后「当前 sub」怎么变？
- 若不存在 sub 表，明确说明，并给出替代（离线偏移↔段落映射表）。

## 4. Q3 函数栈（**已答** → 报告 §4）

> **结论摘要（✅ 静态定案、🟡 未真机）**：**存在可遍历的显式子调用栈**（在**敌方/sub ECL 对象 `E`** 里）：
> 栈基址 `E+0x8FC`；**帧长 `0x218`**；栈顶＝当前 sub 游标 `E+0x6E4`；深度字段 `E+0x2A7C`（压栈 `+1` @ `0x411FD5`、弹栈 `-1` @ `0x412012`），**上限 15（0xF）**。
> 遍历：`for(level=E+0x2A7C; level>=0; --level) frame = E+0x8FC+level*0x218;`。
> ⚠ **timeline 没有调用栈**（只有平铺时间轴）。**仅作逆向事实，不参与判定。**

- 是否存在可遍历的 **ECL 子调用栈**（栈顶＝当前 sub）？给出链/遍历规则与深度上限。
- 若不存在，明确说明。**注意**：按总括 §6 Q9 的 latch 口径，函数栈**不参与**判定，本 Q 只作逆向事实保留。

## 5. Q4 主线宿主捕获点（**已答** → 报告 §5）

> **结论摘要（✅ 静态定案、🟡 未真机）**：**A 代是静态对象，无需捕获 hook**。宿主/管理器 = `ENEMY_MANAGER = 0x9A9B00`（thprac 常量，单级读）。
> `*(u32*)0x9A9AF8 = 0x9A9B00` 写于 **`0x422FE5`**（RVA `0x22FE5`）；管理器初始化 `0x422F40`（`0x422F4C: [ebp-4]=0x9a9b00`）、重置 `0x41EE70`、构造 `0x41EA60`。
> ECL 缓冲/表头 = `*(u32*)0x1347938`（运行期固定地址，可跨进程直接读）。**若坚持要 hook：`0x422FE5`（写 `[0x9A9AF8]`）。**

- 进关 / spawn "main" 时，把主线 ECL **宿主对象**写入的 hook 点（地址 + 命中寄存器）。
- th07 的宿主疑似是**静态对象**（A 代特征：`GameManager 0x626270` / `Supervisor 0x575950`，单级读），请确认。

## 6. Q5 解释器命中点与寄存器语义（**已答** → 报告 §6）

> **结论摘要（✅ 静态定案、🟡 未真机）**：**每帧驱动 = `0x420620`（RVA `0x20620`）**（登记在 `mgr+0x954714` 的每帧回调），其对每条 timeline 调**解释器 `0x41F6F0`（RVA `0x1F6F0`）**，**命中时 `ECX` = `TL_i`**（`TL_i+8` = 当前时间）。每帧调用 `count` 次。
> ⚠ **thprac 已占用 `0x4207A1`（5B，`call 0x41F6F0`）** ⇒ **不要在同址加 hook**。
> **推荐：不挂 hook**（管理器静态，IGI 渲染时直接读 `0x12FE0FC`）；如必须挂，用 **`0x41F6FC`（6B，`8B 45 80`+`8B 48 0C`，`EAX`=TL）**、或 timeline 主循环入口 `0x420620`、或敌方/sub VM 入口 `0x410520`（栈 `[ebp+8]` = 敌机对象）。
> 另：候选 opcode 分支 hook（§1 的 `0x42D17E`/`0x414FE6`/`0x413D06`/`0x41FFD5`）属**敌方/sub VM `0x410520`** 的 opcode case，非 timeline。

- 已有候选（§1）：冻结 `0x4207a1`、opcode 分支 `0x42D17E`/`0x414FE6`/`0x413D06`/`0x41FFD5`。
- 请给出：**每帧读「当前位置」的稳定触发点**是哪一个（或另给）；
  - 该地址所在函数的入口与调用链（谁驱动、每帧几次）；
  - 命中时哪个寄存器/栈帧槽＝执行对象；
  - EHOOK 覆盖长度与该处指令语义。

## 7. Q6 名字来源（**已答** → 报告 §7）

> **结论摘要（❌ 运行期不保留名字）**：A 代 ECL **不保留 sub 名** —— sub 表项只有 4B 指针（`subStart[id]`），`call sub`/`spawn` 的操作数都是 **u16 sub_id**，缓冲内**没有名字串**（与 th12 `"main"`@`0x49FE14` 不同）。
> ⇒ 「读 sub 名」这条路**不通**；本机又**无 `.ecl`/eclmap** ⇒ 离线具名也不行。**「记录名」只能来自 thprac 自己的章节表**，而 checkpoint「入口」必须改用**时间值 / 指令偏移**（见 §9）。可用的离线锚点 = `THStageWarp`/`THPatch` 里 `ECLTimeWarp` 的时间值与 `ECLSetTime`/`ECLCallSub`/`pair{off,…}` 的硬编码偏移。

- 运行期 ECL 资源是否保留 **sub 名**（ASCII 串）？
- 保留 → 直接读 `char*`；不保留 → 说明「索引/偏移」字段，以便**离线**建「索引 ↔ 名字」表。
- **注意**：A 代**没有 eclmap**（§12），离线具名这条路目前不通 —— 若运行期也不保留名字，「记录名」只能来自 thprac 自己的章节表（`th_sections_str` 等）。

## 8. Q7 机体/子机类型（**已答** → 报告 §8）

> **结论摘要（✅ 地址有效、🟡 语义未真机）**：`character = *(u8*)0x62F645`（thprac `:673`，xref 31）、`shotType = *(u8*)0x62F646`（thprac `:674`，xref 17）、`difficulty = *(i32*)0x626280`（thprac `:1863/676`，xref 74）、`STAGE_NUM = *(i32*)0x1347FC8`（thprac `:25`；**运行期固定地址，非映像内，无静态 xref**）。

- th07 有 3 种机体 × 2 种子机（`THPRAC_GAMEROLL_TH07_SHOTTYPES`，`thprac_launcher_games_def.h:959` 附近）。请给出 character / shotType 的读取地址。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（**已答** → 报告 §9/§10）

> **Q8 结论摘要（✅ 静态定案、🟡 未真机）**：`ECLTimeWarp(count,time)` 写 `*(u32*)(0x9A9B00+0x9545FC+i*0x10)`（= `TL_i+8`）；展开复核 `GetMemAddr(0x9a9af8,0x9545fc) = 0x12FE0FC = TL_0+8`（因 `*(u32*)0x9a9af8 = 0x9A9B00`，写于 `0x422FE5`）。下一帧解释器按新时间**快进/对齐**（`0x41F6FC`–`0x41F723`）⇒ **跳转后读「时间」立刻反映新段落**。（`THPatch` 的 `ECLSetTime/ECLCallSub/pair{off,…}` 改的是 sub 脚本，不动 timeline 机制。）
> **Q9 结论摘要（✅）**：沿用 latch（不重新论证）。**A 代读取方案＝读时间值** `time = *(i32*)0x12FE0FC`（`TL_0+8`），按「**本关时间值白名单中最大入口 ≤ time**」取当前段落（单调时间上等价于路过置值、天然不回退）。**入口列口径＝时间值**（＝§11 全表 `ECLTimeWarp` 入参）。
> **⚠ boss 阶段相关口径（副键/分节判定）已于 2026-10-27 全部撤下**，重做前不得引用。
> **候选段落入口值表**（全 8 关道中 + 各 boss/终章，**候选、未真机**）见报告 §11；本节 96 行的口径描述已由上述方案落地。

> **Q9（段落判定口径）**：**沿用总括 §6 Q9 的定案「路过式 checkpoint（latch）」** —— 经过白名单条目即置值、不在表内 no-op（不回退 `main`）、首次命中前显示 `main`。**A 代不重新论证**，本作只需回答：**当前位置怎么取**（Q1/Q2）+ **每段入口取值**。

- th07 的练习跳转是 `ECLTimeWarp` / `THPatch`（`thprac_th07.cpp:1841-1849`）。请回答：**跳转后读「当前位置」是否会立刻反映新段落**？
- **入口列口径（已定）**：A 代的 checkpoint「入口」**不用 sub 名，改用时间值 / 指令偏移**（用户 2026-10-16 定）。请给出与 `ECLTimeWarp` 入参对齐的取值。

---

## 10. 真机验证清单（E5 待做）

> 本机为 **Windows 11**，可编译、可真机运行。**本迭代不要求做** E5，仅列出待验证项。

- [ ] 道中 / boss / 不同章节处采样，读到的位置随进度推进且与实际一致。
- [ ] th07 **stage4 有路线分支**（Prismriver 三姐妹），确认各分支下读值仍正确。
- [ ] 练习模式跳章（`ECLTimeWarp`）后读值立即正确。
- [ ] 标题挂机 attract demo / replay 下读值是否异常。
- [ ] ⚠️ TH07 的 DirectInput 只认 `KEYEVENTF_SCANCODE` 形式的 `SendInput`（普通虚拟键注入游戏读不到）—— 做 E5 时用 `reasm/th07/tools/probe07.py`。

---

## 11. 交付格式

- Q1–Q6 给出**地址/指针链 + 读取表达式 + 判据 + 反汇编证据片段（地址+指令）**；Q7/Q8 视需要。
- 统一用 RVA 表述（本作基址固定 `0x400000`）。
- 结论回填本文件，并同步 `docs/logger/logger_re_disasm_overview.md` §5。
- **入口列口径**：A 代 checkpoint 的「段落入口」记为**时间值 / 指令偏移**（非 sub 名）—— 与 th10–18 的 CSV 语义分叉，回填时需在工作流文档中显式标注。

## 12. 工具与复现

- 静态：`pefile` 取指纹/节表 → `va2off` → `capstone`（`CS_ARCH_X86, CS_MODE_32`）只读反汇编；切入点：从 `0x4207a1` 反汇编其所在函数，从 `0x1347938` / `0x9a9af8+0x9545fc` 找写入点与读取点。
- **素材缺口（E1 前置，本迭代不做）**：
  1. `reasm/exes/` 只有 `th07.exe`，**没有 th07 的 `.ecl`/`.decl`**。
  2. th07 的 ECL 位于哪个 `.dat` 成员未确认（`th07.dat` 已知存在于本体目录）；`thecl` 能否处理 A 代格式**未实测**。
  3. **THTK `MAP/` 无 `ECL_MAP_07.txt`** ⇒ 即便解出 `.ecl`，也拿不到具名 sub。
- 既有可复用工具：`reasm/th07/ghidra_scripts/{Th07Dump,Th07Scan}.java`、`reasm/th07/run.sh`、`reasm/th07/tools/probe07.py`。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档（requests）。为 A 代建档：汇总 thprac 侧全部已知锚点（ECL 基址 / 时间表 warp 落点 / 时间轴与 opcode hook / `ECLCallSub` sub 证据 / 静态宿主对象 / 章节数表）；把 Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一；登记素材缺口（无 `.ecl`、无 eclmap）。**未跑逆向**。 |
| 2026-10-19 | **阶段 E1 静态逆向定案（🟡 未真机）**：新建 `docs/th07/th07_ecl_re_report.md`（Q1–Q9 结论 + 统一指针链 + 候选段落入口值表 + 工具复现 + 真机清单）。本节 §2–§9 各 Q 已回填「已答 + 结论摘要」。**核心结论**：**Q1 当前位置＝时间值（`TL_i+8`）+ 指令指针（`TL_i+0xC`），按 timeline 各一份，无 sub 索引游标**（`TL_i = 0x9A9B00+0x9545F4+i*0x10`，`count = *(u16*)(*(u32*)0x1347938+2)`）；**Q2 sub 表存在**（`*(u32*)(0x1347938+4)`，项 4B 指针，无名字），与时间轴表并列；**Q3 显式调用栈存在**（敌方对象 `E+0x8FC`、帧 `0x218`、深度 `E+0x2A7C` 上限 0xF，仅事实记录）；**Q4 宿主静态**（`0x9A9B00`；`*(u32*)0x9A9AF8=0x9A9B00` 写于 `0x422FE5`，无需捕获 hook）；**Q5 每帧驱动 `0x420620`→解释器 `0x41F6F0`（`ECX=TL_i`）；thprac 已占 `0x4207A1`，推荐不挂 hook（读 `0x12FE0FC`），备用 `0x41F6FC`(6B)**；**Q6 运行期不保留 sub 名**（A 代无 eclmap/.ecl ⇒ 记录名只能取 thprac 章节表）；**Q7** `character=0x62F645`/`shotType=0x62F646`/`difficulty=0x626280`/`STAGE_NUM=0x1347FC8`；**Q8 warp 立即生效**（直写 `TL_i+8`）；**Q9 沿用 latch，读 `*(i32*)0x12FE0FC`（时间值），入口列记时间值**；boss 阶段相关口径已于 2026-10-27 撤下。**未真机三态；`docs/logger/th07_checkpoints.csv` 入口列未回填。** |

# TH06 ECL 段落监控 — 逆向结论（Q1–Q9）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（总括，含术语表 §2、Q1–Q9 §6）。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 本作需求：`docs/th06/th06_ecl_re_requests.md`（Q1–Q9 问法、已知锚点、素材清单、交付格式）。
> 结构模板：`docs/th07/th07_ecl_re_report.md` / `docs/th08/th08_ecl_re_report.md`（**只借结构与 A 代口径，不借结论** —— 三作机制各不相同）。
> 二进制：`../exes/th06.exe`（本体名 `東方紅魔郷.exe`），**512000 B**，PE `TimeDateStamp = 1038721275`，`SizeOfCode = 0x69000`（430080），`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）；
> `sha256 = 9f76483c46256804792399296619c1274363c31cd8f1775fafb55106fb852245`（本机实测）；与 `thprac/src/thprac/thprac_launcher_games_def.h:459` 的 ORIGINAL 条目（`{1038721275, 430080, …}`）一致。
> 节表（本机实测，**`.text` 的 RVA 不是 0**）：`.text` VA `0x401000` / RVA `0x1000` / Raw `0x1000` / RawSize `0x69000` / VSize `0x68A5F`；`.rdata` VA `0x46A000`；`.data` VA `0x476000` / **VirtualSize `0x2719FC`（巨型 BSS，本文的静态全局都在这里）**；`.rsrc` VA `0x6E8000`。
> ECL / decl 素材（**2026-10-21 已备齐，本报告首次使用**）：`../exes/ecl/th06/ecldata1.ecl` … `ecldata7.ecl`（7 份，来自 `紅魔郷ST.DAT`）、`../exes/ecl_dump/th06/ecldata1.decl` … `ecldata7.decl`（`thecl -d6`，**无 eclmap** ⇒ 指令呈 `ins_*` 形态，sub 名为工具自动生成的 `Sub0..SubN`）。汇总见 `../exes/ecl_dump/_dump_report_a_gen.txt`（th06 7/7 成功）。
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` **只读静态反汇编**（脚本见 §15）＋ **自写离线 ECL 解码器**（解析文件头 / sub 偏移表 / 时间轴指令流），用「已知 hook 反汇编 + `disp32`/跳表字节扫描 + 离线 ECL 反解交叉印证」定位。
> 状态：**静态定案（🟡 已定案、未真机三态）**。本机为 macOS，**无法编译、无法真机运行 th06**，未做任何真机验证。
> 地址口径：正文以 **VA** 为主，并标注 **RVA = VA − 0x400000**。
> 置信度图例：✅ 反汇编／素材直接证实 ｜ 🟡 有强证据但缺一环（显式标注缺口）｜ ❌ 未能定案。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前位置** | ✅ **（b）时间值 +（c）指令指针，且是「全局唯一一条时间轴」**；另有**每敌机一份**的 ECL 上下文（含 sub 索引） | 时间轴状态挂在**静态 `EnemyManager`（`0x4B79C8`）尾部**：`tlInstr = *(u32*)(mgr+0xEE5DC)`（当前时间轴指令的**绝对地址**）、`tlTime = *(i32*)(mgr+0xEE5E8)`（当前**时间值**，`= *(i32*)0x5A5FB0`）。**没有** th07 的「每关 count 条 timeline」，也**没有** th08 的「宿主 cursor」——本章节定位就是这**一条**时间轴。敌机侧另有 `enemy+0x990` 的 `EnemyEclContext`（`currentInstr = *(u32*)(enemy+0x990)`、`time.current = *(i32*)(enemy+0x99C)`、`subId = *(u16*)(enemy+0x9D8)`）。 |
| **Q2 sub 表 / 时间轴** | ✅ **两张表，都挂在静态 ECL 管理器 `0x487E50` 上；文件头实测已完全解出** | `pCode = *(u32*)0x487E50`（当前关 ECL 文件缓冲）、`subTable = *(u32*)(0x487E50+4)`（= `pCode + 0x10`）、`timelineStart = *(u32*)(0x487E50+8)`（= `pCode + *(u32*)(pCode+4)`）。文件头：`+0x0` **i16 subCount**、`+0x4` **u32 时间轴偏移**、`+0x10` **u32[subCount] sub 偏移表**（加载器 `0x407340` 把两者就地改成绝对地址）。`call sub` = opcode **0x23**、`ret` = **0x24**。 |
| **Q3 函数栈** | ✅ **存在可遍历的显式调用栈**（在敌机对象里） | 栈基址 `enemy+0x9DC`，**帧长 `0x4C`**，深度 `*(i32*)(enemy+0xC3C)`，**上限 8（`0..7`）**；帧 = **整个 `EnemyEclContext` 的快照**（含 `+0` 指令指针、`+0x48` sub id）。**只作事实记录，不参与判定**（沿用总括 §6 Q9 的 latch 口径）。 |
| **Q4 主线宿主捕获点** | ✅ **A 代是静态对象，无需 hook 捕获** | `ENEMY_MANAGER = 0x4B79C8`（thprac 常量，静态）、`ECL_MANAGER = 0x487E50`（静态）。管理器由**链元素** `0x5A5FB4`（calc，优先级 9，回调 `0x4123E0`）驱动，该元素由 `0x412320` 注册。**整份管理器在每次进关被 `0x410E30` 全量清零**（`rep stosd 0x3B97B`＝`0xEE5EC` 字节）⇒ 时间轴指针/时间也随之归零，重新从 `0x487E58` 装入。 |
| **Q5 解释器命中点** | ✅ **每帧唯一驱动点＝时间轴更新 `0x411530`**（`ECX = mgr`）；**推荐完全不挂 hook** | `0x4123E0`（管理器每帧更新）在 `0x4123F0` 调 `0x411530`；后者逐条时间轴指令「比较/执行/跳过/等待」。时间轴的时钟推进在 `0x412E07`–`0x412E31`（`ZunTimer::Tick` 于 `0x424285`）。thprac 已占用 `0x412E31`（5B，`TH_TIMELOCK`）与 `0x41188A`/`0x411669`（各 3B，`TH06_SavesHook`）⇒ **本报告建议「不挂 hook、在 IGI 渲染时直接读静态地址」**；若必须挂，用**未被占用的 `0x411530` 入口（9B）**。 |
| **Q6 名字来源** | ❌ **运行期不保留 sub 名**；✅ **但离线已可具名（本报告逐文件核对）** | ECL 文件头只有 subCount + 偏移表，**没有名字区**；加载器 `0x407340` 全文无字符串处理。**但 th06 的 `.ecl`/`.decl` 已于 2026-10-21 备齐**：`SubN`（＝ sub 索引 N）与文件偏移一一对应，本报告已把 321 个 sub 全部解出并与 decl 逐条对齐（`subCount` 与 `decl` 的 `sub` 数量 7/7 全等）。⇒ **th06 可以做到「偏移 ↔ sub 索引 ↔ decl 名」的离线映射**；`th07 / th08` 自 2026-10-21 起素材同样备齐、具备同一能力（见各自 `_checkpoint_entry_report.md`），差别只是本报告对 th06 做到了逐文件核验。 |
| **Q7 机体 / 子机 / 难度 / 关卡** | ✅ thprac 既有地址全部有效（有 xref 佐证） | `character = *(u8*)0x69D4BD`（47 处引用）、`shotType = *(u8*)0x69D4BE`（40 处）、`difficulty = *(i32*)0x69BCB0`（79，**解释器直接拿它做 `1<<diff` 的难度跳过掩码**，见 §2.4）、`stage = *(u32*)0x69D6D4`（**1-based**，`= GameManager+0x1A34`，用来索引 ECL 文件名表 `0x4764C4`）。 |
| **Q8 练习跳转影响** | ✅ **立刻反映** | `ECLWarp(t)` = `*(i32*)0x5A5FB0 = t` = **直写 `mgr+0xEE5E8`（时间轴当前时间）**。下一帧 `0x4115D3` 起的时间比较会把 `tlInstr` **快进**到第一条 `time >= t` 的指令（`0x411D15` 的 `+= size` 循环）。练习模式进关会重载 ECL 并把整个管理器清零 ⇒ 指针回到时间轴起点后再快进。**`THPatch` 的另一半（`pair{}`/`ECLSetTime`/`ECLSetHealth`）改的是 sub 脚本指令，与时间轴无关。** |
| **Q9 段落判定口径** | ✅ 沿用**路过式 checkpoint（latch）**；th06 读取方案见 §10 | **主键 = 时间轴指令偏移** `tlOff = tlInstr − pCode`（**精确相等匹配**，因为每个段落的入口就是时间轴上的某一条指令）；**副键 = 时间值** `tlTime`；**boss 相位副键 = boss 敌机的当前 sub**（`bosses[i]` → `ctx.subId`）。**入口列口径沿用 A 代「时间值」**（＝ `ECLWarp` 入参，与 `docs/logger/th07_checkpoints.csv` / `th08_checkpoints.csv` 的 `时间轴位置` 列同义）。 |

> **关键地址（VA / RVA）**
> **静态对象**：`ENEMY_MANAGER = 0x4B79C8` / `0xB79C8`（`EnemyManager`，`0xEE5EC` B）；`ECL_MANAGER = 0x487E50` / `0x87E50`（`EclManager`，`+0`=`pCode`、`+4`=`subTable`、`+8`=`timelineStart`）；`GAME_MANAGER = 0x69BCA0` / `0x29BCA0`；`GUI = 0x69BC30`（`+0x20` = `bossPresent` = `0x69BC50`）；`CHAIN = 0x69D918`。
> **时间轴**：`tlInstr = mgr+0xEE5DC` = `0x5A5FA4`；`timelineTime` = `mgr+0xEE5E0`（`previous`=`0x5A5FA8`、`subFrame`=`0x5A5FAC`、**`current`=`0x5A5FB0`**）。
> **每帧驱动**：链元素 `0x5A5FB4`（calc，优先级 9）→ 回调 **`0x4123E0`**（管理器每帧更新，`0x4123E0`–`0x412E3E`）→ `0x4123F0` 调 **时间轴更新 `0x411530`**（`0x411530`–`0x411D62`）；时钟推进 `0x412E07`–`0x412E31` → `ZunTimer::Tick` = **`0x424285`**。
> **时间轴 opcode 跳表** `0x411D63`（13 项，opcode `0..0xC`）；**sub 生成器 `0x411390`**（`ECX = mgr`、`[esp+4]`=subId、`[esp+8]`=位置三元组指针）。
> **ECL 加载器 `0x407340`**（`ECX = 0x487E50`、`[esp+4]`=文件名）；**sub 设置器 `0x407440`**（`ECX = 0x487E50`、`[esp+4]`=`EnemyEclContext*`、`[esp+8]`=`i16 subId`）。
> **敌机 ECL 解释器 `0x4074A0`**（`0x4074A0`–`0x40A9A1`，`ECX = 0x487E50`、`[esp+4]`=`Enemy*`）；opcode 跳表 **`0x40A9A4`**（opcode `1..0x87`，索引 `opcode−1`）。
> **敌机对象**：`enemies[i] = mgr + 0xED0 + i*0xEC8`（`i = 0..256`）；**`bosses[k] = mgr + 0xEE598 + k*4`**（`k = 0..7`）；`bossPresent = *(u8*)0x69BC50`；`enemyCount = mgr + 0xEE5BC`。
> **ECL 文件名表** `0x4764C4`（`[0]="dummy"`、`[1..7]="data/ecldataN.ecl"`），由 `GameManager+0x1A34`（＝`0x69D6D4`）索引（`0x41C0C1`–`0x41C0D6`）。
> **thprac 现有 hook（均不读当前位置字段）**：`0x41188A`(3B, 时间轴 op4 case)、`0x411669`(3B, op0 case)、`0x412E31`(5B, 时钟推进后)、`0x41B27C`/`0x409A10`/`0x408DDA`/`0x411F88`(VM 内 opcode case)、`0x4096E8`/`0x409978`/`0x412056`(符卡)、`0x428DD9`/`0x4289CC`/`0x41BDE8`(miss/bomb/进关)、`0x42BE49`/`0x42D085`/`0x42C8A0`(存档名)。

---

## 1. 统一指针链（TH06 定案）

```text
// ── ① 静态管理器（A 代：不是堆对象，不需要捕获）───────────────────────────
mgr  = 0x4B79C8                                    // EnemyManager（thprac: ENEMY_MANAGER）
// 布局（由 0x410E30 的 rep stosd 0x3B97B dword 与各处 disp32 交叉定出）：
mgr + 0x0000  : char* stgEnmAnmFilename
mgr + 0x0004  : char* stgEnm2AnmFilename
mgr + 0x0008  : Enemy enemyTemplate（0xEC8 B；0x4113EF 的 rep movsd 0x3B2 拷它）
mgr + 0x0ED0  : Enemy enemies[257]（stride 0xEC8；0x4123E0 / 0x411390 / 0x412F10 的循环）
mgr + 0xEE598 : Enemy* bosses[8]                   // ★ boss 槽（0x408DE5 写入；0x411C64 读）
mgr + 0xEE5B8 : u16 randomItemSpawnIndex
mgr + 0xEE5BA : u16 randomItemTableIndex
mgr + 0xEE5BC : i32 enemyCount                     // 0x412403 自增
mgr + 0xEE5C4 : RunningSpellcardInfo spellcardInfo
mgr + 0xEE5DC : EclTimelineInstr* timelineInstr    // ★ 时间轴当前指令（绝对地址）
mgr + 0xEE5E0 : ZunTimer timelineTime              // ★ {previous,+4 subFrame(float),+8 current}
//   ⇒ timelineInstr = 0x5A5FA4；timelineTime.previous = 0x5A5FA8；
//      timelineTime.subFrame = 0x5A5FAC；timelineTime.current = 0x5A5FB0（= thprac ECLWarp 的写点）

// ── ② 静态 ECL 管理器（当前关 ECL 文件；0x407340 加载时填充）────────────────
ecl      = 0x487E50                                // thprac: ECLHelper::SetBaseAddr 的目标
*(u32*)(ecl + 0x0) = pCode                         // 当前关 ECL 文件缓冲（= ecldataN.ecl 的全部字节）
*(u32*)(ecl + 0x4) = pCode + 0x10                  // sub 偏移表（项 4B，已被加载器改成绝对地址）
*(u32*)(ecl + 0x8) = pCode + *(u32*)(pCode + 0x4)  // 时间轴起点（= ECL 文件头 +4 的那个偏移）

// ── ③ ECL 文件头（7 份 .ecl 实测，见 §11 附录 A）────────────────────────────
pCode + 0x00 : i16  subCount                       // 加载器 0x4073B5: movsx ecx, word [eax]
pCode + 0x02 : i16  （恒 0）
pCode + 0x04 : u32  时间轴偏移（相对文件头；加载器就地改成绝对地址）
pCode + 0x08 : u32  （恒 0）
pCode + 0x0C : u32  （恒 0）
pCode + 0x10 : u32[subCount] sub 起始偏移（相对文件头；加载器就地改成绝对地址）
pCode + 0x10 + subCount*4 : sub 脚本区（首条 sub 的起始）
pCode + 时间轴偏移          : 时间轴指令区（直到文件尾）

// ── ④ 时间轴指令（EclTimelineInstr，8B 头）────────────────────────────────
i = timelineInstr
i + 0x0 : i16  time        // 绝对时间（帧）；< 0 ⇒ 时间轴结束
i + 0x2 : i16  arg0        // 生成类 opcode 的 subId；其它 opcode 的参数
i + 0x4 : i16  opCode      // 0..0xC（跳表 0x411D63，见 §12 附录 B）
i + 0x6 : i16  size        // 本指令字节长度（前进量）
i + 0x8 : ...              // 参数区（位置三元组 / 随机化标志 / 其它）

// ── ⑤ 敌机对象（Enemy，0xEC8 B）与其 ECL 上下文 ────────────────────────────
E   = enemies[i] 或 bosses[k]
E + 0x990 : EnemyEclContext currentContext（0x4C B）
      +0x00 : EclRawInstr* currentInstr（绝对地址；下一条待执行指令）  ★
      +0x04 : ZunTimer time { previous, subFrame(float), current }     ★
      +0x08 :   └ subFrame
      +0x0C :   └ time.current（本 sub 的局部时间，自 0 起）
      +0x10 : funcSetFunc（= enemy+0x9A0，VM 返回前回调）
      +0x14 : var0 … +0x40 var7 / compareRegister
      +0x48 : u16 subId（当前 sub 索引）                              ★
E + 0x9DC : EnemyEclContext savedContextStack[8]（帧长 0x4C）          ★调用栈
E + 0xC3C : i32 stackDepth（0..7）
E + 0xC44 : i32 deathCallbackSub
E + 0xC48 : i32 interrupts[8]（opcode 0x6D 写入的「中断 sub id」表）    ★boss 相位
E + 0xC68 : i32 runInterrupt（-1=无；≥0 ⇒ 下一次 VM 进入时立即跑 interrupts[它]）
E + 0xC6C : Float3 position
E + 0xE40 : u8  本敌机占用的 boss 槽号（opcode 0x65 写入）
E + 0xE50 : u8  flags（bit7 = 活动；0x4123D4 / 0x4113D4 判它）
E + 0xE52 : u8  flags2（bit2 = 「call 不压栈」；VM 0x407B0D 判它）

// ── ⑥ 增益字段（thprac 既有，本报告只做地址确认）───────────────────────────
0x69BCA0 : GameManager（+0x10 difficulty、+0x1C isInReplay、+0x1A34 currentStage）
0x69BCB0 : i32 difficulty（= GameManager+0x10）
0x69BCBC : u8 isInReplay（= GameManager+0x1C）
0x69BCC4 : GameManager+0x24（bombsUsed 低字节）
0x69D4BD / 0x69D4BE : u8 机体 / 子机
0x69D6D4 : i32 currentStage（1-based；= GameManager+0x1A34）
0x69BC30 : ZunGui；0x69BC50 = ZunGui+0x20 = bool bossPresent
```

**与 th07 / th08（同代参照）的差异（重要，不可照搬）**

| 项 | **th06（本作）** | th07 | th08 |
| --- | --- | --- | --- |
| 「当前位置」形态 | **全局唯一一条时间轴**的 `time` + `instr*` | `count` 条 timeline 的 `TL_i+8` / `TL_i+0xC` | 宿主 `cursor` 的 `(ip, time, subId)` |
| 时间轴状态位置 | `EnemyManager+0xEE5DC` / `+0xEE5E8`（**管理器尾部，1 条**） | `mgr+0x9545F4+i*0x10`（16 槽） | `0xF54CF0` 的 slot 数组（**写入侧**，非当前位置） |
| 章节跳转手段 | `ECLWarp(time)` 直写 `mgr+0xEE5E8` | `ECLTimeWarp(count,time)` 直写 `TL_i+8` | `ECLWarp(time,offset)` 写 slot |
| sub 表 | `*(u32*)(0x487E50+4)`（项 4B，文件 `+0x10`） | `*(u32*)(0x1347938+4)`（项 4B） | `*(u32*)(0x4ECCB8+4)`（项 4B，文件 `+0x48`） |
| 宿主 | **静态 `0x4B79C8`**，无需捕获 | 静态 `0x9A9B00` | 静态 manager `0x577F20` 内嵌数组 |
| 解释器 | **两套**：时间轴 `0x411530`（op 0..0xC）+ 敌机 VM `0x4074A0`（op 1..0x87） | timeline `0x41F6F0` + 敌方 VM `0x410520` | 单一 `0x4184B0` |
| 离线具名 | ✅ **`.ecl` + `.decl` 已备齐**（`SubN`；本报告逐文件核对） | ✅ 2026-10-21 已备齐（`SubN`；见 `th07_checkpoint_entry_report.md`） | ✅ 2026-10-21 已备齐（`SubN`；另套用 `ECL_MAP_08.txt` ⇒ 指令具名） |

---

## 2. Q1 —— 「当前位置」＝**全局唯一时间轴的时间值 + 指令指针**，另有每敌机的 ECL 上下文

### 2.1 结论

th06 的「章节进度」是**一条**贯穿整关的时间轴（不是 th07 的 `count` 条，也不是 th08 的宿主 cursor）：

- **`tlTime = *(i32*)(0x4B79C8 + 0xEE5E8) = *(i32*)0x5A5FB0`** —— 时间轴当前时间（帧）；
- **`tlInstr = *(u32*)(0x4B79C8 + 0xEE5DC) = *(u32*)0x5A5FA4`** —— 当前（下一条待执行）时间轴指令的**绝对地址**，`tlOff = tlInstr − pCode` 即**文件偏移**。

敌机（含 boss）另有一份 `EnemyEclContext`：`currentInstr`（`E+0x990`）、`time.current`（`E+0x99C`）、`subId`（`E+0x9D8`）。**章节定位走时间轴，boss 相位才需要敌机上下文。**

### 2.2 证据 A：时间轴更新函数 `0x411530`（VA/RVA `0x411530`/`0x11530`）

```asm
00411530  55                push ebp
00411531  8bec              mov  ebp, esp
00411533  81ec8c000000      sub  esp, 0x8c
00411539  898d78ffffff      mov  [ebp-0x88], ecx            ; ecx = EnemyManager(0x4B79C8)，存到 [ebp-0x88]
00411545  83b8dce50e0000    cmp  dword [eax+0xEE5DC], 0     ; ★ timelineInstr == 0 ?
0041154c  7512              jne  0x411560
00411554  8b15587e4800      mov  edx, [0x487E58]            ; ★ = ECLSRC+8 = pCode + 时间轴偏移
0041155a  8991dce50e00      mov  [ecx+0xEE5DC], edx         ;   ⇒ 首次进入时装入起点
00411560  b930bc6900        mov  ecx, 0x69BC30              ; ZunGui
00411565  e838800000        call 0x4195A2                   ; isPaused?（暂停时不推进时间轴）
...
004115d3  8b8578ffffff      mov  eax, [ebp-0x88]            ; eax = mgr
004115d9  8b88dce50e00      mov  ecx, [eax+0xEE5DC]         ; ★ ecx = timelineInstr
004115df  0fbf11            movsx edx, word [ecx]           ; ★ time = (i16)instr[+0]
004115e2  85d2              test edx, edx
004115e4  0f8c58070000      jl   0x411D42                 ; time < 0 ⇒ 时间轴结束
004115f0  8b88dce50e00      mov  ecx, [eax+0xEE5DC]
004115f6  0fbf11            movsx edx, word [ecx]
00411601  3990e8e50e00      cmp  [eax+0xEE5E8], edx         ; ★ 比较 timelineTime.current 与 instr->time
00411607  0f94c1            sete cl
0041160a  85c9              test ecx, ecx
0041160c  0f84dd060000      je   0x411CEF                 ; 不等 ⇒ 走「等待 / 跳过」
00411612  8b9578ffffff      mov  edx, [ebp-0x88]
00411618  8b82dce50e00      mov  eax, [edx+0xEE5DC]
0041161e  0fbf4804          movsx ecx, word [eax+4]         ; ★ opCode = (i16)instr[+4]
00411628  83bd74ffffff0c    cmp  dword [ebp-0x8c], 0xC
0041162f  0f87b8060000      ja   0x411CED                 ; opCode > 0xC ⇒ 只前进
0041163b  ff2495631d4100    jmp  dword [edx*4+0x411D63]     ; ★ opcode 跳表（0..0xC，见 §12 附录 B）
```

**「等待 / 跳过 / 前进」**（`0x411CEF` / `0x411D15`）：

```asm
00411cef  8b8d78ffffff      mov  ecx, [ebp-0x88]
00411cf5  8b91dce50e00      mov  edx, [ecx+0xEE5DC]
00411cfb  0fbf02            movsx eax, word [edx]           ; instr->time
00411d04  33d2              xor  edx, edx
00411d06  3981e8e50e00      cmp  [ecx+0xEE5E8], eax         ; current < time ?
00411d0c  0f9cc2            setl dl
00411d0f  85d2              test edx, edx
00411d11  7402              je   0x411D15
00411d13  eb2d              jmp  0x411D42                 ; current < time ⇒ 本帧到此为止（等待）
00411d15  8b8578ffffff      mov  eax, [ebp-0x88]
00411d1b  8b88dce50e00      mov  ecx, [eax+0xEE5DC]
00411d21  0fbf5106          movsx edx, word [ecx+6]         ; ★ size = (i16)instr[+6]
00411d25  8b8578ffffff      mov  eax, [ebp-0x88]
00411d2b  0390dce50e00      add  edx, [eax+0xEE5DC]
00411d37  8991dce50e00      mov  [ecx+0xEE5DC], edx         ; ★ timelineInstr += size
00411d3d  e991f8ffff        jmp  0x4115D3                 ; 继续下一条（同一帧可连续跳过多条）
```

⇒ **语义**：`current == instr->time` → 执行 opcode；`current > instr->time` → 跳过（`+= size`，同一帧可连跳）；`current < instr->time` → 本帧结束。**`timelineInstr` 永远指向「下一条待执行」的指令**，这正是「当前位置」的最佳载体。

### 2.3 证据 B：时间轴的时钟推进（`0x412E07`–`0x412E31`，在管理器每帧更新末尾）

```asm
00412e07  8b5508            mov  edx, [ebp+8]               ; edx = mgr
00412e0a  81c2e0e50e00      add  edx, 0xEE5E0               ; &timelineTime
00412e10  8955a8            mov  [ebp-0x58], edx
00412e13  8b45a8            mov  eax, [ebp-0x58]
00412e16  8b4da8            mov  ecx, [ebp-0x58]
00412e19  8b5108            mov  edx, [ecx+8]               ; current
00412e1c  8910              mov  [eax], edx                 ; ★ previous = current
00412e1e  8b45a8            mov  eax, [ebp-0x58]
00412e21  83c004            add  eax, 4                     ; &subFrame
00412e24  50                push eax
00412e25  8b4da8            mov  ecx, [ebp-0x58]
00412e28  83c108            add  ecx, 8                     ; &current
00412e2b  51                push ecx
00412e2c  b9186d6c00        mov  ecx, 0x6C6D18              ; 全局帧步进源
00412e31  e84f140100        call 0x424285                   ; ★ ZunTimer::Tick(&current, &subFrame)
00412e36  b801000000        mov  eax, 1                     ; ★ thprac 的 0x412E31(5B) hook 就压在这
```

`ZunTimer::Tick`（`0x424285`）：

```asm
0042428c  8b45fc            mov  eax, [ebp-4]               ; eax = 0x6C6D18
0042428f  d980ac010000      fld  dword [eax+0x1AC]          ; 帧步进（正常 = 1.0）
00424295  d81d34b74600      fcomp dword [0x46B734]          ; 与 0.0 比较
...
004242a2  8b450c            mov  eax, [ebp+0xC]             ; &subFrame
004242a8  d900              fld  dword [eax]
004242aa  d881a8010000      fadd dword [ecx+0x1A8]          ; subFrame += delta
004242b5  8b450c            mov  eax, [ebp+0xC]
004242b8  d900              fld  dword [eax]
004242ba  d81dc0a24600      fcomp dword [0x46A2C0]          ; 与 1.0 比较
004242c7  8b4508            mov  eax, [ebp+8]               ; &current
004242ca  8b00              mov  eax, [eax]
004242cc  40                inc  eax
004242cd  8b4d08            mov  ecx, [ebp+8]
004242d0  8901              mov  [ecx], eax                 ; ★ current++
004242d2  8b450c            mov  eax, [ebp+0xC]
004242d5  d900              fld  dword [eax]
004242d7  d825c0a24600      fsub dword [0x46A2C0]           ; subFrame -= 1.0
004242e4  ...                                                    ; 步进为 0 的分支：current++（逐帧）
```

⇒ 时间值单位是**帧**（60 帧/秒）。实测各关时间轴取值范围 128–10699（见 §11 附录 A），与关卡时长吻合。`previous` 用于「本帧是否推进过」的判断（`ECLWarp` 只改 `current`）。

### 2.4 证据 C：敌机 ECL 解释器 `0x4074A0`（VA/RVA `0x4074A0`/`0x74A0`）的上下文

```asm
004074b1  8b4508            mov  eax, [ebp+8]               ; eax = Enemy*
004074b4  8b8890090000      mov  ecx, [eax+0x990]           ; ★ currentInstr
004074ba  894de4            mov  [ebp-0x1c], ecx
004074c0  83b8680c000000    cmp  dword [eax+0xC68], 0       ; ★ runInterrupt >= 0 ?
004074c7  7c05              jl   0x4074CE
004074c9  e97f200000        jmp  0x40954D                 ;   ⇒ 立刻转去跑 interrupts[runInterrupt]
004074ce  8b4508            mov  eax, [ebp+8]
004074d1  8b4de4            mov  ecx, [ebp-0x1c]
004074d4  8b909c090000      mov  edx, [eax+0x99C]           ; ★ ctx.time.current
004074da  33c0              xor  eax, eax
004074dc  3b11              cmp  edx, [ecx]                 ; 与 instr->time（i32）比较
004074de  0f94c0            sete al
004074e1  85c0              test eax, eax
004074e3  0f84332b0000      je   0x40A01C                 ; 不等 ⇒ 等待/推进
004074ec  8a5109            mov  dl, [ecx+9]                ; ★ 难度跳过掩码字节
004074f1  b801000000        mov  eax, 1
004074f6  8b0db0bc6900      mov  ecx, [0x69BCB0]            ; ★ difficulty（Q7 证据）
004074fc  d3e0              shl  eax, cl
004074fe  23d0              and  edx, eax
00407500  85d2              test edx, edx
00407502  7505              jne  0x407509                 ; 该难度要跳过 ⇒ 直接前进
...
00407515  0fbf4204          movsx eax, word [edx+4]         ; opCode = (i16)instr[+4]
00407525  83e901            sub  ecx, 1
0040752e  cmp  dword [ebp-0x294], 0x86
00407538  0f87ca2a0000      ja   0x40A008                 ; opCode > 0x87 ⇒ 只前进
00407544  ff2495a4a94000    jmp  dword [edx*4+0x40A9A4]     ; ★ opcode 跳表（1..0x87）
```

**指令头（实测与 thprac `EclRawInstr` 完全一致）**：`+0x00` i32 `time`；`+0x04` i16 `opCode`；`+0x06` i16 `offsetToNext`（**整条指令字节长度**）；`+0x08` u8；`+0x09` u8 **难度跳过掩码**（`1<<difficulty` 置位 ⇒ 跳过）；`+0x0A`/`+0x0B` u8；`+0x0C` 起为参数。**前进**在 `0x40A008`：`currentInstr += offsetToNext`。

**VM 每帧收尾**（`0x40A940`）：把 `currentInstr` 写回 `E+0x990`，然后 `previous = current` 并 `ZunTimer::Tick`（`E+0x994`）⇒ **敌机脚本时间是「自该 sub 启动起的帧数」**，与时间轴的绝对时间**不是同一个量**。

### 2.5 字段表 / 读取表达式

| 字段 | 表达式 | 类型 | 谁写 | 证据 |
| --- | --- | --- | --- | --- |
| **时间轴指令** | `*(u32*)(0x4B79C8+0xEE5DC)` | u32（`pCode` 内绝对地址） | `0x41155A`（首次装入 `0x487E58`）、`0x411D37`（`+= size` 前进） | §2.2 |
| **时间轴时间** | `*(i32*)(0x4B79C8+0xEE5E8)`＝`*(i32*)0x5A5FB0` | i32（帧） | `0x424285`（Tick）、`ECLWarp`（直写） | §2.3 / §9 |
| 时间轴小数累加 | `*(f32*)(0x4B79C8+0xEE5E4)` | f32 | `0x424285` | §2.3 |
| 敌机当前指令 | `*(u32*)(E+0x990)` | u32 | VM `0x407B04`（call）/ `0x409557`（中断）/ `0x40A953`（收尾） | §2.4 |
| 敌机局部时间 | `*(i32*)(E+0x99C)` | i32（帧） | `0x424285`（由 VM 收尾调用） | §2.4 |
| 敌机当前 sub | `*(u16*)(E+0x9D8)` | u16（sub 索引） | `0x407488`（`SetSub`） | §3.2 |

```cpp
// 管理器 / ECL 均为静态，无需捕获；一次读完即可
constexpr uint32_t ENEMY_MANAGER = 0x4B79C8;
constexpr uint32_t ECL_MANAGER   = 0x487E50;

uint32_t pCode   = *(uint32_t*)ECL_MANAGER;                       // 当前关 ECL 文件缓冲
uint32_t tlInstr = *(uint32_t*)(ENEMY_MANAGER + 0xEE5DC);         // 0 表示尚未初始化
int32_t  tlTime  = *(int32_t*)(ENEMY_MANAGER + 0xEE5E8);          // == *(int32_t*)0x5A5FB0
uint32_t tlOff   = (tlInstr && pCode) ? (tlInstr - pCode) : 0;    // 文件偏移（越界/0 需保护）

// boss 相位（仅在需要时读）
for (int k = 0; k < 8; ++k) {
    uint32_t e = *(uint32_t*)(ENEMY_MANAGER + 0xEE598 + k * 4);
    if (!e) continue;
    uint16_t subId = *(uint16_t*)(e + 0x9D8);   // 当前 sub
    uint32_t ip    = *(uint32_t*)(e + 0x990);   // 当前指令（绝对）
    int32_t  rirq  = *(int32_t*)(e + 0xC68);    // runInterrupt（-1 = 无）
}
```

---

## 3. Q2 —— sub 表与时间轴：**两张表，都挂在静态 ECL 管理器 `0x487E50` 上**

### 3.1 ECL 加载器 `0x407340`（VA/RVA `0x407340`/`0x7340`）

```asm
00407340  55                push ebp
00407341  8bec              mov  ebp, esp
00407346  894df8            mov  [ebp-8], ecx              ; ecx = EclManager(0x487E50)
00407349  6a00              push 0
0040734b  8b4508            mov  eax, [ebp+8]               ; 文件名
0040734e  50                push eax
0040734f  e83c6f0100        call 0x41E290                   ; 读文件 → EAX = 缓冲
00407357  8b4df8            mov  ecx, [ebp-8]
0040735a  8901              mov  [ecx], eax                 ; ★ ECLSRC+0 = pCode
0040737b  8b45f8            mov  eax, [ebp-8]
0040737e  8b08              mov  ecx, [eax]
00407380  8b5104            mov  edx, [ecx+4]               ; ★ buf+4 = 时间轴偏移
00407383  8b45f8            mov  eax, [ebp-8]
00407386  0310              add  edx, [eax]                 ;   += pCode
00407388  8b4df8            mov  ecx, [ebp-8]
0040738b  8b01              mov  eax, [ecx]
0040738d  895004            mov  [eax+4], edx               ; ★ 就地改成绝对地址
00407390  8b4df8            mov  ecx, [ebp-8]
00407393  8b11              mov  edx, [ecx]
00407395  83c210            add  edx, 0x10                  ; ★ pCode + 0x10 = sub 表
00407398  8b45f8            mov  eax, [ebp-8]
0040739b  895004            mov  [eax+4], edx               ; ★ ECLSRC+4 = sub 表
004073a7  8b4dfc            mov  ecx, [ebp-4]               ; i
004073aa  83c101            add  ecx, 1
004073b0  8b55f8            mov  edx, [ebp-8]
004073b3  8b02              mov  eax, [edx]
004073b5  0fbf08            movsx ecx, word [eax]           ; ★ subCount = (i16)buf[+0]
004073b8  394dfc            cmp  [ebp-4], ecx
004073bb  7d1f              jge  0x4073DC
004073bd  8b55f8            mov  edx, [ebp-8]
004073c0  8b4204            mov  eax, [edx+4]               ; = pCode+0x10
004073c3  8b4dfc            mov  ecx, [ebp-4]
004073c6  8b1488            mov  edx, [eax+ecx*4]           ; 表项（原为相对偏移）
004073c9  8b45f8            mov  eax, [ebp-8]
004073cc  0310              add  edx, [eax]                 ;   += pCode
004073d1  8b4104            mov  eax, [ecx+4]
004073d4  8b4dfc            mov  ecx, [ebp-4]
004073d7  891488            mov  [eax+ecx*4], edx           ; ★ 就地改成绝对地址
004073dc  8b55f8            mov  edx, [ebp-8]
004073df  8b02              mov  eax, [edx]
004073e1  8b4df8            mov  ecx, [ebp-8]
004073e4  8b5004            mov  edx, [eax+4]               ; buf+4（已是绝对）
004073e7  895108            mov  [ecx+8], edx               ; ★ ECLSRC+8 = 时间轴起点
```

⇒ **文件头**：`+0x00` **i16 subCount**、`+0x04` **u32 时间轴偏移**、`+0x10` **u32[subCount] sub 偏移表**；加载器把 `+0x04` 与该表**就地改成绝对地址**，并让 `ECLSRC+8` 指向时间轴起点。**头里没有名字**（Q6）。

### 3.2 sub 设置器 `0x407440`（VA/RVA `0x407440`/`0x7440`）＝ `call sub` 的落点，也是 `subId` 的写入点

```asm
00407440  55                push ebp
00407441  8bec              mov  ebp, esp
00407446  894df8            mov  [ebp-8], ecx              ; ecx = EclManager(0x487E50)
00407449  0fbf450c          movsx eax, word [ebp+0xC]      ; subId
0040744d  8b4df8            mov  ecx, [ebp-8]
00407450  8b5104            mov  edx, [ecx+4]              ; ★ subTable
00407453  8b4d08            mov  ecx, [ebp+8]              ; ctx = EnemyEclContext*
00407456  8b1482            mov  edx, [edx+eax*4]          ; ★ subStart[subId]
00407459  8911              mov  [ecx], edx                ; ★ ctx->currentInstr = subStart[subId]
0040745b  8b4508            mov  eax, [ebp+8]
0040745e  83c004            add  eax, 4                     ; &ctx->time
00407461  8945fc            mov  [ebp-4], eax
00407467  c7410800000000    mov  dword [ecx+8], 0           ; time.current = 0
0040746e  c7420400000000    mov  dword [edx+4], 0           ; time.subFrame = 0
00407478  c700 19fcffff     mov  dword [eax], 0xFFFFFC19     ; time.previous = -999
00407481  8b4d08            mov  ecx, [ebp+8]
00407484  668b550c          mov  dx, word [ebp+0xC]
00407488  66895148          mov  word [ecx+0x48], dx         ; ★ ctx->subId = subId（ctx+0x48 = enemy+0x9D8）
```

⇒ **`subStart(id) = *(u32*)( *(u32*)(0x487E50+4) + id*4 )`**；同时把 `ctx->subId` 置为该 id、把局部时间清成 `{-999, 0, 0}`。

### 3.3 `call`(0x23) / `ret`(0x24)

`call`（跳表 `0x40A9A4[0x22]` = `0x407AEC`）：

```asm
00407aec  8b4de4            mov  ecx, [ebp-0x1c]           ; ins
00407aef  8b510c            mov  edx, [ecx+0xC]            ; ★ subId = (i32)ins[+0xC]
00407af5  0fbf4806          movsx ecx, word [eax+6]        ; offsetToNext
00407afc  8b55e4            mov  edx, [ebp-0x1c]
00407aff  03d1              add  edx, ecx
00407b01  8b4508            mov  eax, [ebp+8]
00407b04  899090090000      mov  [eax+0x990], edx          ; ★ 返回地址 = ins + size
00407b0a  8b4d08            mov  ecx, [ebp+8]              ; ecx = Enemy*
00407b0d  8a91520e0000      mov  dl, [ecx+0xE52]           ; flags2
00407b13  c0ea02            shr  dl, 2                     ; bit2 = 「call 不压栈」
00407b21  7526              jne  0x407B49
00407b23  8b7508            mov  esi, [ebp+8]
00407b26  81c690090000      add  esi, 0x990                ; src = ctx
00407b2c  8b883c0c0000      mov  ecx, [eax+0xC3C]          ; depth
00407b35  6bc94c            imul ecx, ecx, 0x4C            ; ★ 帧长 0x4C
00407b3b  8dbc0adc090000    lea  edi, [edx+ecx+0x9DC]      ; ★ dst = enemy+0x9DC+depth*0x4C
00407b42  b913000000        mov  ecx, 0x13                  ; 0x13 dword = 0x4C B
00407b47  f3a5              rep movsd                      ; ★ 压栈（整份 ctx 快照）
00407b49  668b45f0          mov  ax, word [ebp-0x10]
00407b4d  50                push eax                       ; subId
00407b4e  8b4d08            mov  ecx, [ebp+8]
00407b51  81c190090000      add  ecx, 0x990
00407b57  51                push ecx                       ; &ctx
00407b58  b9507e4800        mov  ecx, 0x487E50
00407b5d  e8def8ffff        call 0x407440                  ; ★ SetSub
00407b7c  83b93c0c000007    cmp  dword [ecx+0xC3C], 7       ; ★ 深度上限 8
00407b94  89813c0c0000      mov  [ecx+0xC3C], eax          ; depth++
```

`ret`（跳表 `0x40A9A4[0x23]` = `0x407BBD`）：

```asm
00407be4  8b913c0c0000      mov  edx, [ecx+0xC3C]
00407bea  83ea01            sub  edx, 1                     ; depth--
00407bf0  89903c0c0000      mov  [eax+0xC3C], edx
00407bff  6bd24c            imul edx, edx, 0x4C
00407c05  8db410dc090000    lea  esi, [eax+edx+0x9DC]      ; 源帧
00407c0c  8b7d08            mov  edi, [ebp+8]
00407c0f  81c790090000      add  edi, 0x990                ; 目标 ctx
00407c15  b913000000        mov  ecx, 0x13
00407c1a  f3a5              rep movsd                      ; ★ 恢复 ctx（含 currentInstr / subId）
```

### 3.4 两张表的关系

- **sub 表**（`pCode+0x10`）= 「按 id 索引的脚本体」；`call sub <id>` 与 `SetSub` 用它；
- **时间轴表**（`pCode+时间轴偏移` 起）= 「整关的总导演脚本」，按时间推进，`opCode 0..7` 会**生成**一个 sub 脚本对象（`0x411390`）；
- 两者在**同一个文件**、**同一个对象**里，但**语义不同**：时间轴是「何时生成谁」，sub 是「被生成的脚本内容」。**两张表都没有名字**。

---

## 4. Q3 —— 函数栈：**存在、可遍历**（在敌机对象里）

| 项 | 值 | 证据 |
| --- | --- | --- |
| 栈基址 | `enemy + 0x9DC` | `0x407B3B`（`lea edi,[edx+ecx+0x9DC]`）、`0x409593`、`0x407C05` |
| 帧长 | **`0x4C`** | `0x407B35` / `0x40958D` / `0x407BFF`（`imul ...,0x4C`） |
| 深度 | `*(i32*)(enemy+0xC3C)` | `0x407B2F`（压）/ `0x407BE4`（弹） |
| 上限 | **8**（`depth < 7` 才 `++`） | `0x407B7C: cmp dword [ecx+0xC3C], 7` |
| 帧内容 | **整份 `EnemyEclContext` 快照** ⇒ 帧 `+0x00` = 调用者指令指针、帧 `+0x48` = 调用者 sub id、帧 `+0x0C` = 调用者局部时间 | `rep movsd 0x13`（0x4C B） |

- ⚠️ **「call 不压栈」分支**：`enemy+0xE52` bit2 置位时跳过压栈（`0x407B0D`–`0x407B21`）。遍历时须以深度与 `frame+0x48` 合法性做保护。
- **按总括 §6 Q9，函数栈「不参与」段落判定**（本项只作逆向事实记录）。

---

## 5. Q4 —— 主线宿主捕获点：**A 代是静态对象，不需要捕获**

### 5.1 三条硬事实

1. **管理器是静态常量**：`0x4B79C8`。`0x412320` 里 `mov [ebp-4], 0x4B79C8` 后直接 `call 0x410E30`（重置）——**没有任何「分配/创建」的堆对象**。
2. **管理器由链元素驱动**：`0x412320` 把 `0x5A5FB4`（calc 链元素，优先级 9）注册进 `CHAIN = 0x69D918`，回调 = `0x4123E0`：
   ```asm
   00412344  mov dword [0x5A5FB8], 0x4123E0    ; ★ chainElem->callback（元素在 0x5A5FB4）
   0041234e  mov dword [0x5A5FBC], 0            ;   addedCallback
   00412358  mov dword [0x5A5FC0], 0            ;   deletedCallback
   00412362  mov dword [0x5A5FBC], 0x413220
   0041236c  mov dword [0x5A5FC0], 0x413320
   00412379  mov dword [0x5A5FD0], ecx          ;   arg = mgr
   0041237f  push 9                            ;   priority
   00412381  push 0x5A5FB4                     ;   &chainElem
   00412386  mov  ecx, 0x69D918
   0041238b  call 0x41C860                     ;   Chain::AddElement
   ```
   另一个 draw 链元素在 `0x4B79A8`（优先级 6，回调 `0x412F10`）。
   > 注：`0x5A5FB4` ＝ `0x4B79C8 + 0xEE5EC` ＝ **管理器尾部紧邻**（`timelineTime.current` 在 `0x5A5FB0`，正好在元素前 4 字节）。
3. **每次进关整份清零**：`0x410E30` 开头 `rep stosd 0x3B97B`（= `0xEE5EC` B，正好等于 `sizeof(EnemyManager)`）⇒ `timelineInstr`/`timelineTime` 归零，下一次 `0x411530` 再从 `0x487E58` 装入时间轴起点。**这就是「跳关/重开后再 warp 仍准确」的机制。**
   调用链（关卡装载路径）：`0x41C080`–`0x41C0D6`（按 `currentStage` 取文件名、装载 ECL）里 **`0x41C09D call 0x412320`** → `0x41232E call 0x410E30`（清零）→ 之后 `0x41C0D6 call 0x407340`（装载 ECL）⇒ **清零与装载同属一次进关**。另一条重置路径在 `0x41122B`（函数 `0x411190` 尾部，先 `0x424127` 复位时间、再清零管理器）。

### 5.2 「主线宿主」不需要定位

- th06 **没有**「具名 main 宿主」这一层：章节进度全在**静态管理器的时间轴**上；
- 每个被时间轴生成的敌机都只是「脚本实例」，没有「谁是主线」的问题；
- ⇒ **Q4 的答案是「无需 hook 捕获，直接读静态地址」**，比 th08 的 `🟡 主线宿主是哪一槽未定案` 更干净。

### 5.3 若要 hook（不推荐）

| 目的 | 地址（VA/RVA） | 覆盖 | 命中时取值 |
| --- | --- | --- | --- |
| 管理器每帧更新入口 | `0x4123E0`/`0x23E0` | 6B（`55 8B EC 83 EC 64`） | `[ebp+8]` = mgr |
| 时间轴更新入口（**推荐备选**） | **`0x411530`/`0x11530`** | 9B（`55 8B EC 81 EC 8C 00 00 00`） | `ECX` = mgr |
| 时钟推进之后 | `0x412E31`/`0x2E31` | 5B | ⚠️ **已被 thprac `TH_TIMELOCK` 占用** |
| 关卡 ECL 装载 | `0x41C0D6`/`0x1C0D6` | 5B（`call 0x407340`） | `[esp+4]` = 文件名指针、`[esp+8]` = 0 |

---

## 6. Q5 —— 解释器命中点与寄存器语义

### 6.1 每帧驱动链

```
CHAIN(0x69D918) 的 calc 链
  └─ 元素 0x5A5FB4（priority 9）
       └─ callback 0x4123E0  EnemyManager::Update（每帧一次）
            ├─ 0x4123F0  call 0x411530   ★ 时间轴更新（ECX = mgr）
            ├─ 0x412416.. 循环 i = 0..0xFF：活跃敌机 → 0x413380 / 0x412240 / 0x4074A0(VM)
            └─ 0x412E07.. tick timelineTime（0x424285）
```

- **时间轴更新每帧 1 次**；**敌机 VM 每帧每活跃敌机 1 次**（`0x41258A`，`ECX = 0x487E50`、`[esp+4] = Enemy*`）。
- 时间轴解释器**没有函数调用式递归**：它就是 `0x411530` 内的一个 `while`（`0x4115D3` 循环）。

### 6.2 稳定触发点（推荐）

| 用途 | 地址（VA/RVA） | 覆盖 | 命中时寄存器/栈槽 | 说明 |
| --- | --- | --- | --- | --- |
| **每帧一次（推荐备选）** | **`0x411530`/`0x11530`** | 9B | `ECX` = `mgr` | 未被 thprac 占用；进入后 `[ebp-0x88]` 也是 mgr |
| 时间轴指令命中（泛用，但**逐指令**） | `0x4115D3`/`0x115D3` | 6B（`8B 85 78 FF FF FF`） | `[ebp-0x88]` = mgr | 每次循环一次（含跳过） |
| 时钟推进后（**已占用**） | `0x412E31`/`0x2E31` | 5B | `EDX` = 推进前的 `current`（thprac 用它做 `curTime`） | ⚠️ `TH_TIMELOCK` 已占 |
| 现有 `TH06_SavesHook` | `0x41188A` / `0x411669` | 3B | `[ebp-0x14]` / `[ebp-0xC]` = `instr+8`（减 8 得指令起点） | 只在 op4 / op0 执行时命中，**不能当每帧采样** |
| 敌机 VM | `0x4074A0`/`0x74A0` | 9B | `ECX = 0x487E50`、`[esp+4] = Enemy*` | 每敌机每帧一次 |

> **结论：不要挂 hook。** 两个静态对象（`0x4B79C8`、`0x487E50`）都可以在 IGI 渲染（帧间稳定态）直接读，零地址冲突、零 EHOOK 维护成本。

### 6.3 thprac 现有 hook 的定位（本报告逐条核对）

| thprac 名 | 地址 | 实际所在 | 结论 |
| --- | --- | --- | --- |
| `th06_lock_timer1` | `0x41B27C` | 函数 `0x41B1B1` 内（**全局 GUI 重置**：`rep stosd 11` 清 `0x69BC30` 起 11 个 dword） | **只在进关/初始化命中**，不是每帧 |
| `th06_lock_timer2` | `0x409A10` | 敌机 VM **opcode `0x73`**（115）case（handler `0x409A07`）：`enemy+0xEB0 = ins->arg`（set timeout） | 执行到该 opcode 才命中 |
| `th06_lock_timer3` | `0x408DDA` | 敌机 VM **opcode `0x65`**（101）case（handler `0x408DD3`）：`bosses[arg]=enemy`、`bossPresent=1` | 执行到该 opcode 才命中 |
| `th06_lock_timer4` | `0x411F88` | 函数 `0x411F40` 内（**敌机「待处理动作」runner**，由 `0x41257C` 对 `[enemy+0xEB0] >= 0` 的敌机每帧调用） | 与时间轴无关；每帧对**部分**敌机命中 |
| `th06BooksStart` | `0x41188A` | 时间轴 op `4` 的 case（`0x4117CF` 段内） | 逐指令命中；**不是**每帧 |
| `th06BooksEnd` | `0x411669` | 时间轴 op `0` 的 case（`0x411642` 段内） | 同上 |
| `th06_freeze` | `0x412E31` | 时钟推进 `call 0x424285` 之后 | 每帧；**已占用** |

---

## 7. Q6 —— 名字来源：**运行期不保留 sub 名；但离线已可具名**

### 7.1 运行期：❌ 没有名字

1. 加载器 `0x407340`（§3.1）只处理 `+0`（subCount）、`+4`（时间轴偏移）、`+0x10`（sub 偏移表），**没有任何字符串/名字处理**；
2. `SetSub`（§3.2）取的是 `subTable[id]` 的**指针**，`call`（§3.3）的操作数是 **i32 sub 索引**；
3. 时间轴生成（`0x411390`）传的也是 **i16 sub 索引**（`instr+2`）；
4. exe 内的 ECL 相关字符串只有**文件名**（`data/ecldata1.ecl` … `data/ecldata7.ecl`，表 `0x4764C4`）。

### 7.2 离线：✅ `.decl` 已备齐，`SubN` ＝ sub 索引 N

**三作（th06 / th07 / th08）的 `.ecl`/`.decl` 均已于 2026-10-21 备齐**；本报告对 th06 做了逐文件核对（th07 / th08 的对应核对见各自 `_checkpoint_entry_report.md`）：

- `../exes/ecl/th06/ecldataN.ecl` 与 `../exes/ecl_dump/th06/ecldataN.decl` 7 份齐全（`_dump_report_a_gen.txt` 7/7 成功）；
- **`SubN` 就是 sub 索引 N**：`thecl` 无 eclmap 时按索引自动命名；本报告已逐文件核对 —— `subCount` 与 `decl` 里 `sub …()` 的数量 **7/7 全等**，且每个 sub 的**指令条数**与 `decl` 的 `ins_*` 条数**逐 sub 相等（差值恒为 1，即末尾的 12 字节终止哨兵 `{-1,-1,12}`，`thecl` 不打印）**；
- 因此 **`subStart(N)` ↔ `pCode` 偏移 ↔ `SubN` ↔ `decl` 文本** 形成闭环，可离线建「偏移 → 段落」表。

> ⚠️ `SubN` 是**工具生成的序号名，不是游戏内的名字**；但它在本仓库是**稳定标识**（同一份 `.ecl` 永远得到同一组 `SubN`），可直接当 checkpoint 的辅助键使用。

### 7.3 对监控的影响

- 「读运行期 sub 名」不通（本来就没有）；
- **但「读运行期 sub 索引」完全可行**：`*(u16*)(E+0x9D8)`，再用离线表映射到 `SubN`；
- **章节级**仍建议只记「时间值」（A 代口径），**boss 相位级**才用 `SubN` 做副键。

---

## 8. Q7 —— 机体 / 子机 / 难度 / 关卡

| 名称 | VA / RVA | 类型 | 判据（反汇编） | 置信度 |
| --- | --- | --- | --- | --- |
| `character`（机体） | `0x69D4BD` / `0x29D4BD` | u8 | 47 处引用（`0x40995A`、`0x40B2EB`、`0x411BCD`…） | ✅ 地址有效（语义待真机） |
| `shotType`（子机） | `0x69D4BE` / `0x29D4BE` | u8 | 40 处引用（`0x409962`、`0x411BF5`…） | ✅ |
| `difficulty` | `0x69BCB0` / `0x29BCB0` | i32 | **`0x4074F6: mov ecx,[0x69BCB0]; shl eax,cl; and edx,eax`** ⇒ 直接决定难度跳过掩码 | ✅ |
| `currentStage` | `0x69D6D4` / `0x29D6D4` | i32（**1-based**） | `= GameManager+0x1A34`；`0x41C0C4: mov eax,[eax+0x1A34]; push [eax*4+0x4764C4]`（索引 ECL 文件名表） | ✅ |
| `GAME_MANAGER` | `0x69BCA0` / `0x29BCA0` | 静态 struct | `difficulty = +0x10`、`isInReplay = +0x1C`（=`0x69BCBC`，10 处 xref）、`currentStage = +0x1A34` | ✅ |
| `isInReplay` | `0x69BCBC` / `0x29BCBC` | u8 | thprac 既有；10 处 xref | ✅ |
| `bossPresent` | `0x69BC50` / `0x29BC50` | u8（`bool`） | `= ZunGui(0x69BC30)+0x20`；时间轴 op0–7 全部以 `!bossPresent` 为生成前置 | ✅ |
| `ENEMY_MANAGER` | `0x4B79C8` / `0xB79C8` | 静态 struct（`0xEE5EC` B） | §1 / §5 | ✅ |
| `ECL_MANAGER` | `0x487E50` / `0x87E50` | 静态 struct | §3.1 | ✅ |
| ECL 文件名表 | `0x4764C4` / `0x764C4` | `char*[9]` | `0x41C0CD` | ✅（`[0]="dummy"`、`[1..7]=ecldata1..7`） |
| bombs 计数（bomb_dec 用） | `0x69BCC4` / `0x29BCC4` | `GameManager+0x24`（i32，thprac 读低字节） | 4 处 xref（`0x4289CE`…） | ✅ |

> 本迭代 IGI 只需「当前段落」，Q7 非必需；上表供后续日志迭代。

---

## 9. Q8 —— 练习跳转（`ECLWarp` / `THPatch`）对读取的影响：**立刻反映**

1. **`ECLWarp(t)` 的实现**（`thprac_th06.cpp:1894`）就是 `*((int32_t*)(0x5a5fb0)) = t;` ——
   而 `0x5a5fb0 = 0x4B79C8 + 0xEE5E8 = timelineTime.current`。**直写时间轴的当前时间。**
2. **下一帧的效果**：`0x4115D3` 的比较循环会把 `timelineInstr` 逐条 `+= size` 快进，直到第一条 `time >= t`（`0x411D15`）；若某条恰好 `time == t`，当帧就会执行它。
3. ⇒ **跳转后读 `0x5A5FB0` 立刻是新时间**；读 `tlInstr` 立刻是**新落点**（第一条 `time >= t` 的指令）。
4. **瞬态**：`ECLWarp` 只写时间，**不重置 `timelineInstr`**。所以：
   - 练习模式**进关**会重载 ECL 并把管理器清零（§5.1）⇒ 指针回到时间轴起点，再被新时间快进 ⇒ **正确**；
   - 若在**同一局中途**把时间往回改（例如运行中再次 warp 到更早的段落），指针**不会回退**（`current < instr->time` 只会等待）⇒ **读值仍是旧/较后的位置**。⇒ 采样实现必须把「本局开始」作为重置点（与 th14–18 的 `*_game_start` 重置一致）。
5. **`THPatch` 的另一半**（`pair{…}` / `ECLSetTime` / `ECLSetHealth` / `ECLStall`）改的是**sub 脚本的指令**（文件偏移，与时间轴同一坐标系），**不触碰时间轴指针与时间**；但它会改变 boss 的**相位脚本走向**（见 §10.3）。
6. **`ECLNameFix`** 只是 ANM 文件名补丁，与本机制无关。

---

## 10. Q9 —— 段落判定口径与 th06 读取方案

### 10.1 口径沿用（不重新论证）

沿用总括 §6 Q9 的定案：**经过白名单条目即置「当前段落」，不在表内 no-op（不回退 `main`）；本局首次命中前显示 `main`。**

### 10.2 ★ th06 可落地读取方案（本报告新增）

**读什么**

```cpp
uint32_t pCode   = *(uint32_t*)0x487E50;                  // 当前关 ECL 缓冲
uint32_t tlInstr = *(uint32_t*)(0x4B79C8 + 0xEE5DC);      // 时间轴「下一条待执行」指令
int32_t  tlTime  = *(int32_t*)(0x4B79C8 + 0xEE5E8);       // 时间轴时间（= *(int32_t*)0x5A5FB0）
uint32_t tlOff   = tlInstr - pCode;                       // ★ 段落主键（文件偏移）
```

**判定算法（推荐：精确命中 + latch）**

```cpp
// 白名单 = 本关所有段落的 (入口偏移 entryOff, 记录名)，entryOff 见 §13/§14 附录
const Section* cur = mainSection;                 // 初值 "main"
for (const auto& s : whitelistOfStage(stage))     // stage = *(uint32_t*)0x69D6D4（1-based）
    if (s.entryOff == tlOff) { cur = &s; break; } // ★ 精确相等即命中（同一帧只可能停在一条指令上）
```

- **为什么用「偏移精确相等」而不是「时间取最大 ≤ 」**：时间轴上大量指令**共享同一时间值**（同一帧连跳多条），且 thprac 的 warp 值往往比落点时间小 60 帧左右（道中）—— 用时间做「≤ 」会在**同一帧内连中多条**；而 `tlInstr` 是**唯一确定的一条指令**，精确相等天然满足 latch 语义（经过即置、不在表内 no-op）。
- **`tlTime` 的作用**：可读性 / 日志字段 / 消歧（同一 `entryOff` 对应多个段落时，例如 st4 的 `portion 4` 与 `TH06_ST4_BOOKS` 都是 `0xCDD4`，见 §14）。
- **边界保护**：`pCode == 0`（未进关）或 `tlInstr == 0`（管理器刚清零）时直接返回 `main`；`tlOff` 超出文件长度时同样丢弃。

**已知 caveat（🟡 未真机）**

1. **中boss 窗口（st1/st2/st4/st5）**：thprac 的 `TH_TIMELOCK` 注释写明这几关的中boss **缺少 `boss_wait`** —— 即时间轴上没有「等 boss 死」的 op `0xC`，所以**正常游玩**时中boss 战期间时钟**不会**自动冻结，`tlInstr` 会一路掠过后续段落入口（thprac 的对策是在 `0x412E31` 用 `EHOOK_HK` 人为冻结时钟，仅 TIMELOCK/F5 生效）。**建议实现时移植 th16 §5.7 的「中boss 存活期间章节 checkpoint 只缓存、不显示」策略**（本报告只登记缺口，不做方案）。
2. **同一帧连跳**：`bossPresent` 期间生成类 opcode 只前进不生成，若一帧内跨过多条入口，采样（每帧一次）可能只看到最后一条。⇒ 采样必须落在**帧间稳定态**（IGI 渲染），且建议「本帧内若 `tlOff` 跳变超过 N 条，则不 latch」。
3. **`ECLWarp` 不回退指针**（§9 第 4 条）⇒ 必须靠「进关重置」保证起点。

### 10.3 boss 相位的副键（🟡 未真机）

同一 boss 的多个 phase 共用**同一个入口偏移**（见 §14 的 st1/st2/st4/st6/st7），此时用**敌机侧**字段：

```cpp
uint32_t e = *(uint32_t*)(0x4B79C8 + 0xEE598 + k*4);   // bosses[k]，k=0..7
uint16_t sub      = *(uint16_t*)(e + 0x9D8);           // ★ 当前 sub 索引（→ SubN）
uint32_t ip       = *(uint32_t*)(e + 0x990);           // 当前指令（→ SubN+0x偏移）
int32_t  runIrq   = *(int32_t*)(e + 0xC68);            // 最近一次中断序号（-1 = 无）
int32_t  irqSub   = *(int32_t*)(e + 0xC48 + runIrq*4); // 该中断要跑的 sub id（runIrq ∈ 0..7）
```

**机制依据**：boss 的每个阶段是**独立 sub**（例：st1 的 `Sub10` 是 boss 控制器，`Sub11` 是第 1 阶段、`Sub16` 是后续阶段）。控制器用 opcode **0x6D**（`0x409524`，`interrupts[arg1] = arg0`）登记阶段，用 **0x6E**（`0x40953E`，`runInterrupt = arg0` 后压栈并 `SetSub(interrupts[runInterrupt])`）触发；时间轴也能用 **op 0xA**（`0x411C3B`，`bosses[arg0]->runInterrupt = arg1`）从外部触发。VM 每次进入时在 `0x4074C0` 检查 `runInterrupt >= 0`，若成立就跳 `0x40954D` 执行中断并把 `runInterrupt` 置 `-1`。

⇒ **`*(u16*)(boss+0x9D8)` 在阶段切换后就是该阶段的 `SubN`**，可直接做为相位键；`runIrq`/`irqSub` 用于「阶段已登记但尚未起跑」的瞬态消歧。

### 10.4 checkpoint「入口」列记什么（A 代口径不变）

| 段落类型 | 入口列取值 | 来源 |
| --- | --- | --- |
| 道中 portion | **时间值**（＝ `ECLWarp` 入参，例 st1 p1 = `68`） | `THStageWarp` |
| boss / 中boss | **时间值**（例 ST1 BOSS1 = `0x149E`） | `THPatch` |
| 同时间/同落点的多个 boss 相位 | 时间值 **+ 副键（`SubN`）** | §10.3 |
| 实现内部（不入 CSV） | 落点偏移 `tlOff`（§13/§14 的 `落点 offset` 列） | 本报告 §7.2 的离线解码 |

> ⚠ **与 th10–18 的 CSV 语义分叉**：A 代「段落入口」列**不是 sub 名**，而是**时间值**（必要时 + `SubN` 副键）；与 `docs/logger/th07_checkpoints.csv` / `th08_checkpoints.csv` 的 `时间轴位置` 列同义。**本报告不生成 `th06_checkpoints.csv`**（E2 属后续阶段）。

---

## 11. 附录 A —— ECL 文件结构实测（7 份 `.ecl` 全量解码）

### A.1 头布局（由加载器 `0x407340` 与实测双向印证）

```text
+0x00  i16  subCount
+0x02  i16  （恒 0）
+0x04  u32  timelineOffset      ← 加载器就地改成 pCode+offset
+0x08  u32  （恒 0）
+0x0C  u32  （恒 0）
+0x10  u32  subOffset[subCount] ← 加载器就地改成 pCode+offset
+0x10+subCount*4  sub 脚本区（首条 sub 起点）
pCode+timelineOffset  时间轴指令区（连续到文件尾前 4 字节）
文件尾 4 字节        时间轴终止标记 `FF FF 04 00`（首 i16 = -1 ⇒ `0x4115E2` 判 `time < 0` 结束）
```

- **sub 脚本终止哨兵**：每条 sub 末尾有 12 字节 `{time=-1, opCode=-1, size=12}`（`thecl` 不打印）。
- **sub 指令头 12B**：`{i32 time; i16 opCode; i16 size; u8; u8 diffMask; u8; u8}`，参数从 `+0xC` 起，`size` 为整条长度。
- **时间轴指令头 8B**：`{i16 time; i16 arg0; i16 opCode; i16 size}`，参数从 `+8` 起。

### A.2 逐文件统计

| 关卡 | 文件 | 大小 | subCount | timelineOff | 时间轴指令数 | 时间范围 | 末指令末偏移 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| st1 | `ecldata1.ecl` | 24562 | 24 | `0x472E` | 231 | 128–5283 | `0x5FEE` |
| st2 | `ecldata2.ecl` | 26880 | 33 | `0x35C4` | 473 | 330–5988 | `0x68FC` |
| st3 | `ecldata3.ecl` | 33070 | 35 | `0x541E` | 418 | 400–5918 | `0x812A` |
| st4 | `ecldata4.ecl` | 60796 | 61 | `0xC1CC` | 404 | 440–10699 | `0xED78` |
| st5 | `ecldata5.ecl` | 30392 | 52 | `0x5D50` | 238 | 440–7708 | `0x76B4` |
| st6 | `ecldata6.ecl` | 30866 | 46 | `0x6A46` | 139 | 440–3175 | `0x788E` |
| st7 | `ecldata7.ecl` | 59890 | 70 | `0xC456` | 351 | 440–8598 | `0xE9EE` |

> 「末指令末偏移」= 最后一条时间轴指令的 `offset + size`；**7 份全部比文件大小少 4 字节**，差的正是文件尾的终止标记 `FF FF 04 00` ⇒ 解码完整、无遗漏。
> **sub 总数 = 24+33+35+61+52+46+70 = 321**，与 `decl` 的 `sub` 数量逐文件相等。

### A.3 ★ 交叉印证：thprac 的 books 常量正好是时间轴指令边界

`thprac_th06.cpp` 的 `th06BooksStart` / `th06BooksEnd` 用 `pCodeOfs - pCode == 0xCDD4` / `== 0xCE7C` 判 books 起止（st4）。本报告离线解码 `ecldata4.ecl` 得到：

- 时间轴指令 **`0xCDD4`（time=3452, op=4, arg0=14）** —— 与 `TH06_ST4_BOOKS` 的 warp `0xD40` 落点**完全一致**；
- 时间轴指令 **`0xCE7C`（time=4132, op=0, arg0=21）** —— 与 `TH06_ST4_MID1` 的 warp `0x1024` 落点**完全一致**。

⇒ 说明 **(a)** thprac 的硬编码偏移与游戏运行期 `tlInstr − pCode` 是同一坐标系；**(b)** 本报告的离线解码器与真实引擎行为一致。这是本报告最重要的一处「静态互证」。

---

## 12. 附录 B —— 时间轴 opcode 表（跳表 `0x411D63`，13 项）

| op | handler（VA/RVA） | 语义 | 证据 |
| --- | --- | --- | --- |
| `0` | `0x411642`/`0x11642` | **生成** sub `arg0`（位置取 `instr+8` 三元组）；`!bossPresent` 才生成 | `0x411695 call 0x411390` |
| `1` | `0x4116A5`/`0x116A5` | 生成（无位置，三参 `-1`） | `0x4116E7 call 0x411390` |
| `2` | `0x4116F1`/`0x116F1` | 生成后置 `enemy+0xE50` 的 bit6（`or 0x40`） | `0x41174A call`、`0x41175B or` |
| `3` | `0x41176B`/`0x1176B` | 同 2（位置变体） | `0x4117AD call`、`0x4117BE or` |
| `4` | `0x4117CF`/`0x117CF` | 生成 + 位置随机化（`0x69D6F4/0x69D6F8`） | `0x41188A` 起读三元组；`call 0x411390` |
| `5` | `0x4118C6`/`0x118C6` | 同 4（变体） | `0x411994 call` |
| `6` | `0x41199E`/`0x1199E` | 同 4（变体） | `0x411A8B call` |
| `7` | `0x411AAC`/`0x11AAC` | 同 4（变体） | `0x411B80 call` |
| `8` | `0x411BA2`/`0x11BA2` | **非生成类**：`0x418768(arg0 + character*10)`，并写 `[0x6C6EB0]=3`（按位置推断为 BGM/演出切换，🟡） | `0x411BDC`/`0x411C06 call 0x418768` |
| `9` | `0x411C10`/`0x11C10` | **等待**：若「暂停中」则 `ZunTimer::Decrement(&timelineTime, 1)`（冻结时钟） | `0x411C2C call 0x4241E5` |
| `0xA` | `0x411C3B`/`0x11C3B` | `bosses[arg0]->runInterrupt = arg1`（外部触发 boss 中断） | `0x411C66 mov [ecx+0xC68], edx` |
| `0xB` | `0x411C6E`/`0x11C6E` | `*(u16*)0x69D4B0 = arg0`（全局，语义 🟡） | `0x411C7E mov word [0x69D4B0], dx` |
| `0xC` | `0x411C87`/`0x11C87` | **等待 boss 死亡**：`bosses[arg0]` 仍活着 ⇒ `ZunTimer::Decrement(&timelineTime, 1)`（冻结时钟） | `0x411CA7`–`0x411CE6` |

- **op 3 / 5 / 7 / 0xB 在 7 份 `.ecl` 里一次都没出现**（见 A.4 分布表）—— 属引擎支持但原作未用的分支。
- **`0x411390` = 生成器**：在 `enemies[0..255]` 里找 `flags.bit7 == 0` 的空槽 → `rep movsd 0x3B2`（拷 `mgr+8` 的模板 0xEC8 B）→ `SetSub(&slot+0x990, subId)` → 立刻 `0x4074A0` 跑一帧 VM → 写位置/参数。

### A.4 各关 opcode 分布（离线统计）

| 关卡 | op 0 | op 1 | op 2 | op 3 | op 4 | op 5 | op 6 | op 7 | op 8 | op 9 | op 10 | op 11 | op 12 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| st1 | 103 | 0 | 102 | 0 | 10 | 0 | 9 | 0 | 2 | 3 | 1 | 0 | 1 |
| st2 | 92 | 0 | 0 | 0 | 374 | 0 | 0 | 0 | 2 | 3 | 1 | 0 | 1 |
| st3 | 199 | 0 | 180 | 0 | 30 | 0 | 0 | 0 | 3 | 3 | 1 | 0 | 2 |
| st4 | 239 | 0 | 152 | 0 | 6 | 0 | 0 | 0 | 2 | 3 | 1 | 0 | 1 |
| st5 | 133 | 0 | 96 | 0 | 0 | 0 | 0 | 0 | 3 | 3 | 2 | 0 | 1 |
| st6 | 67 | 0 | 59 | 0 | 0 | 0 | 0 | 0 | 3 | 5 | 3 | 0 | 2 |
| st7 | 186 | 0 | 123 | 0 | 31 | 0 | 0 | 0 | 3 | 4 | 2 | 0 | 2 |

---

## 13. 附录 C —— 道中段落入口表（`THStageWarp`，候选、未真机）

> 取值 = `ECLWarp(<time>)` 入参（`thprac_th06.cpp:2753-2910`）；「落点」= 离线解码得到的「第一条 `time >= warp` 的时间轴指令」。
> 落点偏移可与运行期 `tlInstr − pCode` **精确比对**。`SubN` = 自落点起**第一条生成类指令**（op ≤ 7）生成的 sub，仅作参考。

| 关卡 | portion | warp time | 落点 offset | 落点 time | 落点 op | 首个 spawn 的 sub |
| --- | --- | --- | --- | --- | --- | --- |
| st1 | 1 | 68 (`0x44`) | `0x472E` | 128 | 0 | `Sub0` |
| st1 | 2 | 580 (`0x244`) | `0x4ACA` | 640 | 0 | `Sub2` |
| st1 | 3 | 1160 (`0x488`) | `0x4C8A` | 1220 | 6 | `Sub0` |
| st1 | 4 | 1540 (`0x604`) | `0x4E9E` | 1600 | 2 | `Sub0` |
| st1 | 5 | 2348 (`0x92C`) | `0x51CA` | 2408 | 0 | `Sub2` |
| st1 | 6 | 4438 (`0x1156`) | `0x58CA` | 4498 | 0 | `Sub0` |
| st2 | 1 | 270 (`0x10E`) | `0x35C4` | 330 | 4 | `Sub0` |
| st2 | 2 | 924 (`0x39C`) | `0x3D6C` | 984 | 0 | `Sub6` |
| st2 | 3 | 3528 (`0xDC8`) | `0x4798` | 3588 | 4 | `Sub7` |
| st2 | 4 | 4563 (`0x11D3`) | `0x5BB8` | 4623 | 4 | `Sub8` |
| st3 | 1 | 340 (`0x154`) | `0x541E` | 400 | 0 | `Sub0` |
| st3 | 2 | 1050 (`0x41A`) | `0x59CE` | 1110 | 0 | `Sub4` |
| st3 | 3 | 1670 (`0x686`) | `0x5B56` | 1730 | 4 | `Sub5` |
| st3 | 4 | 2762 (`0xACA`) | `0x63A6` | 2822 | 0 | `Sub0` |
| st3 | 5 | 3807 (`0xEDF`) | `0x6C62` | 3816 | 0 | `Sub7` |
| st3 | 6 | 4118 (`0x1016`) | `0x6F1E` | 4178 | 0 | `Sub8` |
| st3 | 7 | 5274 (`0x149A`) | `0x7F86` | 5334 | 0 | `Sub8` |
| st4 | 1 | 380 (`0x17C`) | `0xC1CC` | 440 | 0 | `Sub0` |
| st4 | 2 | 1454 (`0x5AE`) | `0xC584` | 1514 | 0 | `Sub10` |
| st4 | 3 | 2328 (`0x918`) | `0xC93C` | 2388 | 0 | `Sub11` |
| st4 | 4 | 3392 (`0xD40`) | `0xCDD4` | 3452 | 4 | `Sub14` |
| st4 | 5 | 4872 (`0x1308`) | `0xCE98` | 4932 | 0 | `Sub6` |
| st4 | 6 | 5712 (`0x1650`) | `0xD330` | 5772 | 0 | `Sub3` |
| st4 | 7 | 7434 (`0x1D0A`) | `0xD8A8` | 7494 | 0 | `Sub11` |
| st4 | 8 | 8354 (`0x20A2`) | `0xDCD0` | 8414 | 0 | `Sub8` |
| st4 | 9 | 9784 (`0x2638`) | `0xE750` | 9844 | 0 | `Sub11` |
| st5 | 1 | 350 (`0x15E`) | `0x5D50` | 440 | 0 | `Sub0` |
| st5 | 2 | 1352 (`0x548`) | `0x5F80` | 1442 | 0 | `Sub0` |
| st5 | 3 | 2292 (`0x8F4`) | `0x6370` | 2352 | 0 | `Sub3` |
| st5 | 4 | 3814 (`0xEE6`) | `0x64C4` | 3874 | 0 | `Sub1` |
| st5 | 5 | 6774 (`0x1A76`) | `0x74A0` | 6834 | 0 | `Sub2` |
| st6 | 1 | 380 (`0x17C`) | `0x6A46` | 440 | 0 | `Sub1` |
| st6 | 2 | 1484 (`0x5CC`) | `0x7616` | 1544 | 2 | `Sub5` |
| st7 | 1 | 380 (`0x17C`) | `0xC456` | 440 | 0 | `Sub0` |
| st7 | 2 | 1300 (`0x514`) | `0xCC36` | 1360 | 0 | `Sub5` |
| st7 | 3 | 2600 (`0xA28`) | `0xD1E6` | 2660 | 0 | `Sub1` |
| st7 | 4 | 3680 (`0xE60`) | `0xD2AA` | 3740 | 0 | `Sub4` |
| st7 | 5 | 4803 (`0x12C3`) | `0xD30A` | 4863 | 0 | `Sub6` |
| st7 | 6 | 5933 (`0x172D`) | `0xDE4E` | 5993 | 4 | `Sub10` |
| st7 | 7 | 7733 (`0x1E35`) | `0xE1B2` | 7793 | 0 | `Sub9` |

> ⚠️ **st4 有路线分支**：thprac 的 st4 给了 9 个 portion，而 th06 的 st4 只有一份 `.ecl`（`ecldata4.ecl`）⇒ 🟡 **推断**：分支若存在，在 **sub 内部**，不影响时间轴落点（这正是 th06 与 th14「ST4 拆 A/B 两份 `.ecl`」的区别）；**待真机确认**。
> ⚠️ **st3 portion 5**：warp `3807` 与落点 `3816` 之间有 9 帧空档（时间轴上没有指令），属正常。

---

## 14. 附录 D —— boss / 中boss 段入口与相位（`THPatch`，候选、未真机）

> 「warp time」= 该 section 第一个 `ECLWarp` 入参（lambda 内的也算；`s2b_nd`/`s3b_n1`/`s4b_time`/`s7b_n1` 已展开）。
> 「落点 offset」= 第一条 `time >= warp` 的时间轴指令；**多个 section 共享同一落点是常态**（这就是为什么 boss 相位必须用 §10.3 的敌机副键）。
> 「补丁偏移落在的 sub」= 该 section 所有 `pair{0x…}` 偏移（`ECLSetTime`/`ECLSetHealth` 的偏移不计）所属的 sub —— 即 thprac 实际改写的相位脚本。

| section | warp time | 落点 offset | 落点 time | 落点 op | 首个 spawn sub | 补丁偏移落在的 sub |
| --- | --- | --- | --- | --- | --- | --- |
| `TH06_ST1_MID1` | 2008 (`0x7D8`) | `0x51AE` | 2008 | 0 | `Sub8` | `Sub8` |
| `TH06_ST1_MID2` | 2008 (`0x7D8`) | `0x51AE` | 2008 | 0 | `Sub8` | `Sub8` |
| `TH06_ST1_BOSS1` | 5278 (`0x149E`) | `0x5F92` | 5278 | 8 | `Sub10` | `Sub11` |
| `TH06_ST1_BOSS2` | 5279 (`0x149F`) | `0x5F9A` | 5279 | 9 | `Sub10` | `Sub11` |
| `TH06_ST1_BOSS3` | 5279 (`0x149F`) | `0x5F9A` | 5279 | 9 | `Sub10` | `Sub11`, `Sub16` |
| `TH06_ST1_BOSS4` | 5279 (`0x149F`) | `0x5F9A` | 5279 | 9 | `Sub10` | `Sub11`, `Sub16` |
| `TH06_ST2_MID1` | 2588 (`0xA1C`) | `0x477C` | 2588 | 0 | `Sub20` | — |
| `TH06_ST2_BOSS1` | 5983 (`0x175F`) | `0x68A0` | 5983 | 8 | `Sub21` | — |
| `TH06_ST2_BOSS2` | 5984 (`0x1760`) | `0x68A8` | 5984 | 9 | `Sub21` | `Sub22` |
| `TH06_ST2_BOSS3` | 5984 (`0x1760`) | `0x68A8` | 5984 | 9 | `Sub21` | `Sub22`, `Sub25` |
| `TH06_ST2_BOSS4` | 5984 (`0x1760`) | `0x68A8` | 5984 | 9 | `Sub21` | `Sub22`, `Sub25` |
| `TH06_ST2_BOSS5` | 5984 (`0x1760`) | `0x68A8` | 5984 | 9 | `Sub21` | `Sub22`, `Sub25`, `Sub32` |
| `TH06_ST3_MID1` | 3804 (`0xEDC`) | `0x6C2E` | 3804 | 0 | `Sub9` | — |
| `TH06_ST3_MID2` | 3804 (`0xEDC`) | `0x6C2E` | 3804 | 0 | `Sub9` | `Sub9`, `Sub10` |
| `TH06_ST3_BOSS1` | 5844 (`0x16D4`) | `0x80D6` | 5854 | 0 | `Sub17` | `Sub10`, `Sub17`, `Sub18` |
| `TH06_ST3_BOSS2` | 5844 (`0x16D4`) | `0x80D6` | 5854 | 0 | `Sub17` | `Sub10`, `Sub17`, `Sub18` |
| `TH06_ST3_BOSS3` | 5844 (`0x16D4`) | `0x80D6` | 5854 | 0 | `Sub17` | `Sub10`, `Sub17`, `Sub18`, `Sub20` |
| `TH06_ST3_BOSS4` | 5844 (`0x16D4`) | `0x80D6` | 5854 | 0 | `Sub17` | `Sub10`, `Sub17`, `Sub18`, `Sub20` |
| `TH06_ST3_BOSS5` | 5844 (`0x16D4`) | `0x80D6` | 5854 | 0 | `Sub17` | `Sub10`, `Sub17`, `Sub18`, `Sub26` |
| `TH06_ST3_BOSS6` | 5844 (`0x16D4`) | `0x80D6` | 5854 | 0 | `Sub17` | `Sub10`, `Sub17`, `Sub18`, `Sub26` |
| `TH06_ST3_BOSS7` | 5844 (`0x16D4`) | `0x80D6` | 5854 | 0 | `Sub17` | `Sub10`, `Sub17`, `Sub18`, `Sub26`, `Sub33` |
| `TH06_ST4_BOOKS` | 3392 (`0xD40`) | `0xCDD4` | 3452 | 4 | `Sub14` | `Sub14` |
| `TH06_ST4_MID1` | 4132 (`0x1024`) | `0xCE7C` | 4132 | 0 | `Sub21` | — |
| `TH06_ST4_BOSS1` | 10694 (`0x29C6`) | `0xED1C` | 10694 | 8 | `Sub26` | — |
| `TH06_ST4_BOSS2` | 10695 (`0x29C7`) | `0xED24` | 10695 | 9 | `Sub26` | `Sub27` |
| `TH06_ST4_BOSS3` | 10695 (`0x29C7`) | `0xED24` | 10695 | 9 | `Sub26` | `Sub27`, `Sub37` |
| `TH06_ST4_BOSS4` | 10695 (`0x29C7`) | `0xED24` | 10695 | 9 | `Sub26` | `Sub27`, `Sub37` |
| `TH06_ST4_BOSS5` | 10695 (`0x29C7`) | `0xED24` | 10695 | 9 | `Sub26` | `Sub27`, `Sub39` |
| `TH06_ST4_BOSS6` | 10695 (`0x29C7`) | `0xED24` | 10695 | 9 | `Sub26` | `Sub27`, `Sub39`, `Sub40` |
| `TH06_ST4_BOSS7` | 10695 (`0x29C7`) | `0xED24` | 10695 | 9 | `Sub26` | `Sub27`, `Sub39`, `Sub41` |
| `TH06_ST5_MID1` | 3372 (`0xD2C`) | `0x6488` | 3372 | 0 | `Sub12` | — |
| `TH06_ST5_MID2` | 3372 (`0xD2C`) | `0x6488` | 3372 | 0 | `Sub12` | — |
| `TH06_ST5_BOSS1` | 7704 (`0x1E18`) | `0x7660` | 7704 | 0 | `Sub21` | `Sub22` |
| `TH06_ST5_BOSS2` | 7704 (`0x1E18`) | `0x7660` | 7704 | 0 | `Sub21` | `Sub22` |
| `TH06_ST5_BOSS3` | 7704 (`0x1E18`) | `0x7660` | 7704 | 0 | `Sub21` | `Sub22`, `Sub34` |
| `TH06_ST5_BOSS4` | 7704 (`0x1E18`) | `0x7660` | 7704 | 0 | `Sub21` | `Sub22`, `Sub34` |
| `TH06_ST5_BOSS5` | 7704 (`0x1E18`) | `0x7660` | 7704 | 0 | `Sub21` | `Sub22`, `Sub41` |
| `TH06_ST5_BOSS6` | 7704 (`0x1E18`) | `0x7660` | 7704 | 0 | `Sub21` | `Sub22`, `Sub41` |
| `TH06_ST6_MID1` | 2564 (`0xA04`) | `0x77D6` | 2564 | 0 | `Sub8` | `Sub8` |
| `TH06_ST6_MID2` | 2564 (`0xA04`) | `0x77D6` | 2564 | 0 | `Sub8` | `Sub9` |
| `TH06_ST6_BOSS1` | 3167 (`0xC5F`) | `0x781A` | 3167 | 8 | `Sub15` | `Sub17` |
| `TH06_ST6_BOSS2` | 3169 (`0xC61`) | `0x782A` | 3169 | 0 | `Sub15` | `Sub17` |
| `TH06_ST6_BOSS3` | 3169 (`0xC61`) | `0x782A` | 3169 | 0 | `Sub15` | `Sub17`, `Sub19` |
| `TH06_ST6_BOSS4` | 3169 (`0xC61`) | `0x782A` | 3169 | 0 | `Sub15` | `Sub17`, `Sub19` |
| `TH06_ST6_BOSS5` | 3169 (`0xC61`) | `0x782A` | 3169 | 0 | `Sub15` | `Sub17`, `Sub23` |
| `TH06_ST6_BOSS6` | 3169 (`0xC61`) | `0x782A` | 3169 | 0 | `Sub15` | `Sub17`, `Sub23` |
| `TH06_ST6_BOSS7` | 3169 (`0xC61`) | `0x782A` | 3169 | 0 | `Sub15` | `Sub17`, `Sub26` |
| `TH06_ST6_BOSS8` | 3169 (`0xC61`) | `0x782A` | 3169 | 0 | `Sub15` | `Sub17`, `Sub26` |
| `TH06_ST6_BOSS9` | 3169 (`0xC61`) | `0x782A` | 3169 | 0 | `Sub15` | `Sub17`, `Sub43`, `Sub44` |
| `TH06_ST7_MID1` | 4740 (`0x1284`) | `0xD2C6` | 4740 | 0 | `Sub16` | — |
| `TH06_ST7_MID2` | 4740 (`0x1284`) | `0xD2C6` | 4740 | 0 | `Sub16` | `Sub17`, `Sub18` |
| `TH06_ST7_MID3` | 4740 (`0x1284`) | `0xD2C6` | 4740 | 0 | `Sub16` | `Sub17`, `Sub19` |
| `TH06_ST7_END_NS1` | 8593 (`0x2191`) | `0xE992` | 8593 | 8 | `Sub31` | `Sub32` |
| `TH06_ST7_END_S1` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32` |
| `TH06_ST7_END_NS2` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub35` |
| `TH06_ST7_END_S2` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub35` |
| `TH06_ST7_END_NS3` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub38` |
| `TH06_ST7_END_S3` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub38` |
| `TH06_ST7_END_NS4` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub43` |
| `TH06_ST7_END_S4` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub43` |
| `TH06_ST7_END_NS5` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub47` |
| `TH06_ST7_END_S5` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub47` |
| `TH06_ST7_END_NS6` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub49` |
| `TH06_ST7_END_S6` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub49` |
| `TH06_ST7_END_NS7` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub53` |
| `TH06_ST7_END_S7` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub53` |
| `TH06_ST7_END_NS8` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub56` |
| `TH06_ST7_END_S8` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub56` |
| `TH06_ST7_END_S9` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub59` |
| `TH06_ST7_END_S10` | 8594 (`0x2192`) | `0xE99A` | 8594 | 9 | `Sub31` | `Sub32`, `Sub67` |

**读表要点**

1. **BOSS1 与其他 boss 相位的落点差 1 条指令**（`0x5F92` op8 vs `0x5F9A` op9）—— 因为 `0x149E` 与 `0x149F` 只差 1 帧；`TH06_ST1_BOSS2/3/4` **三者落点相同**，必须靠 §10.3 的副键区分。
2. **中boss 的落点唯一**（MID1/MID2 同名同落点）；它们的区别在 `ECLSetHealth`（不在本表统计内）。
3. **st4 `BOOKS` 与 `portion 4` 同落点 `0xCDD4`**：thprac 把「books 关」既当 portion 又当独立 section（`TH06_ST4_BOOKS`）；实现时需按 `th_sections_t` 归一。
4. **`TH06_ST3_BOSS*` 七个 section 全部同落点**（warp 都是 `0x16D4`），相位完全靠 `Sub17`/`Sub18`/`Sub20`/`Sub26`/`Sub33` 区分 —— 这是 th06 里最依赖副键的一组。
5. **`TH06_ST7_END_*` 18 个 section 全部同落点**（`0xE99A`），靠 `Sub32`+`Sub35/38/43/47/49/53/56/59/67` 区分。

---

## 15. 工具与复现

- 环境：macOS + `python3 3.9.6` + `pefile 2024.8.26` + `capstone 5.0.7`（**只读静态反汇编**）；ECL 侧为自写纯 Python 解码（无外部依赖）。
- 反汇编 helper：`P(path)`；`off(va)=pe.get_offset_from_rva(va-0x400000)`（**勿用 `va-0x400000`**）；`dis(va,n)`；`u32(va)`；`xrefs(val)`（在可执行节内搜 4 字节小端立即数/disp32）。
- ECL 解码 helper：`parse(path)` → `(buf, subCount, timelineOff, subOffsets[], timelineInstrs[])`，时间轴按 `{i16 time,i16 arg0,i16 op,i16 size}` 连走，sub 按 `{i32 time,i16 op,i16 size}` 连走，末条 `size<=0` 或越界即止。
- 复现步骤（本次实际路径）：
  1. 从 thprac 已知 hook `0x41188A` 反汇编其所在函数 ⇒ 时间轴更新 `0x411530`、`mgr+0xEE5DC`（instr）、`mgr+0xEE5E8`（time）、opcode 跳表 `0x411D63`。
  2. 由 `0x4123E0`（管理器每帧更新）确认 `0x4123F0 call 0x411530` 与末尾的 `timer tick`（`0x412E07`–`0x412E31`）；反汇编 `0x424285` 得 `ZunTimer::Tick`。
  3. 搜 `mgr+0xEE5DC` 的 `disp32`（33 处）⇒ 确认时间轴所有读点；搜 `0x4B79C8+0xEE598`（boss 槽）与 `+0xEE5BC`（enemyCount）确认结构边界；由 `0x410E30` 的 `rep stosd 0x3B97B` 确认 `sizeof(EnemyManager)=0xEE5EC`。
  4. 由 `0x487E50` 的 xref 反汇编加载器 `0x407340` ⇒ 文件头（`+0` subCount / `+4` 时间轴偏移 / `+0x10` sub 表）与三个字段的绝对化。
  5. 由 `0x407440`（sub 设置器）与 `call 0x4074A0` 的调用点反汇编敌机 VM ⇒ 指令头（`+0` time / `+4` op / `+6` size / `+9` 难度掩码）、opcode 跳表 `0x40A9A4`、`call`(0x23)/`ret`(0x24)、调用栈（`+0x9DC`/`0x4C`/`+0xC3C`）、中断（`+0xC48`/`+0xC68`）。
  6. 离线解码 7 份 `.ecl`：核对 `subCount` 与 `decl` 的 `sub` 数量、逐 sub 指令数与 `decl` 的 `ins_*` 数量（差值恒为 1 = 终止哨兵）；核对时间轴末指令末偏移 = 文件大小 − 4（差的 4 字节是终止标记 `FF FF 04 00`）。
  7. **交叉印证**：`0xCDD4` / `0xCE7C`（thprac books 常量）与 `ecldata4.ecl` 的时间轴指令边界逐字节对齐（§A.3）。
  8. 解析 `thprac_th06.cpp` 的 `THStageWarp` / `THPatch`（含 5 个 lambda）取出全部 `ECLWarp` 入参与 `pair{}` 偏移，与离线时间轴做「落点 = 第一条 `time >= warp`」匹配（§13 / §14）。
- **PE 陷阱**：`.text` 的 RVA 非 0（VA `0x401000`，RVA `0x1000`）⇒ 取文件偏移必须用 `pefile.get_offset_from_rva(va-0x400000)`。
- 素材：`../exes/th06.exe` + `../exes/ecl/th06/*.ecl`（7）+ `../exes/ecl_dump/th06/*.decl`（7）。

---

## 16. 真机验证清单（E6 待做；本轮**未做**）

> 本机为 macOS，**以下全部未验证**；状态一律「静态定案（🟡 已定案、未真机三态）」。

- [ ] **静态性**：进关后 `*(u32*)0x487E50 != 0`；`*(u32*)(0x4B79C8+0xEE5DC)` 落在 `pCode` 与 `pCode+fileSize` 之间。
- [ ] **时间轴时间随进度推进**：道中/boss 各采样一次，`*(i32*)0x5A5FB0` 单调递增（boss 战期间可被 op 0xC 冻结）。
- [ ] **`Q1` 落点**：st1 portion 1 进关后 `tlInstr − pCode == 0x472E`（附录 C 首行）。
- [ ] **`Q8` warp 立即生效**：练习跳章后下一帧 `*(i32*)0x5A5FB0 == 目标时间`，且 `tlInstr − pCode` == 附录 C/D 的落点。
- [ ] **`Q9` latch**：用附录 C/D 的落点表，各段落精确命中、段内 no-op、首次命中前为 `main`。
- [ ] **boss 相位副键**：st3（7 个 section 同落点）与 st7 END（18 个 section 同落点）逐相位验证 `*(u16*)(boss+0x9D8)` 能否区分。
- [ ] **中boss 窗口**：st1/st2/st4/st5 的中boss 战期间 `tlInstr` 是否会掠过后续段落入口（thprac `TH_TIMELOCK` 的注释说会）。
- [ ] **st4 路线分支**：st4 全 9 个 portion 各跑一次（分支在 sub 内部，时间轴落点应一致）。
- [ ] **replay / attract**：`isInReplay`（`0x69BCBC`）为真时读值是否异常（管理器是否照常推进）。
- [ ] **管理器清零**：进退关卡多次后 `tlInstr` 是否每次都从 `0x487E58` 重新起算。
- [ ] **变体**：Steam / 汉化版是否共用同一地址（本报告地址来自 `ts=1038721275`、`sha256=9f76483c…`）。

---

## 17. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-22 | 建档（本报告）。th06 的 ECL 段落监控逆向（阶段 E1，静态定案 🟡），**首次使用 2026-10-21 备齐的 `.ecl`/`.decl` 素材**。**Q1**：当前位置＝**全局唯一时间轴的「时间值 + 指令指针」**（`mgr = 0x4B79C8`；`tlInstr = *(u32*)(mgr+0xEE5DC) = 0x5A5FA4`、`tlTime = *(i32*)(mgr+0xEE5E8) = *(i32*)0x5A5FB0`），**没有** th07 的多 timeline、**没有** th08 的宿主 cursor；敌机另有 `enemy+0x990` 的 `EnemyEclContext`（`+0` ip / `+0x0C` 局部时间 / `+0x48` subId）。**Q2**：sub 表 = `*(u32*)(0x487E50+4)`（= `pCode+0x10`）、时间轴起点 = `*(u32*)(0x487E50+8)`（= `pCode + *(u32*)(pCode+4)`），由加载器 `0x407340` 就地绝对化；**文件头布局实测**：`+0` i16 subCount、`+4` u32 时间轴偏移、`+0x10` u32[subCount] sub 偏移表；`call` = opcode 0x23、`ret` = 0x24。**Q3**：调用栈存在（基址 `enemy+0x9DC`、帧 `0x4C`、深度 `+0xC3C`、上限 8），仅事实记录。**Q4**：宿主静态（`0x4B79C8` / `0x487E50`），由链元素 `0x5A5FB4`（calc，优先级 9）驱动 `0x4123E0`，管理器每次进关被 `0x410E30` 全量清零（`rep stosd 0x3B97B` = `0xEE5EC` B）⇒ **无需捕获**。**Q5**：每帧唯一驱动 = `0x411530`（`ECX = mgr`，`0x4123F0` 调用），时钟推进 `0x412E07`–`0x412E31`（`ZunTimer::Tick` = `0x424285`）；thprac 已占 `0x412E31`/`0x41188A`/`0x411669` ⇒ **推荐不挂 hook**，备选 `0x411530`（9B）。**Q6**：运行期无 sub 名（加载器无字符串处理、`call`/生成器只传索引），**但 `.decl` 已备齐** ⇒ `SubN` = sub 索引 N，7/7 文件 `subCount` 与 `decl` 的 `sub` 数全等、逐 sub 指令数差恒为 1（终止哨兵 `{-1,-1,12}`）⇒ **离线可具名**。**Q7**：thprac 既有地址全部有效，另确认 `difficulty` 被解释器用作 `1<<diff` 难度掩码（`0x4074F6`）、`currentStage`（`0x69D6D4`，1-based）= `GameManager+0x1A34` 索引 ECL 文件名表 `0x4764C4`、`bossPresent = GUI(0x69BC30)+0x20 = 0x69BC50`。**Q8**：`ECLWarp` 直写 `timelineTime.current`，下一帧 `0x4115D3` 快进到第一条 `time >= t`；**指针不回退**（同局内往回 warp 无效），须靠进关清零。**Q9**：沿用 latch；th06 方案 = **主键「时间轴指令偏移精确相等」+ 副键时间值 + boss 相位用 `bosses[k]` 的 `ctx.subId`**；入口列口径沿用 A 代「时间值」。新增附录 A（ECL 文件结构 + 7 文件统计 + books 常量交叉印证）、附录 B（时间轴 opcode 0..0xC 全表 + 各关分布）、附录 C（40 个道中 portion 的 warp/落点/sub）、附录 D（70 个 boss section 的 warp/落点/sub/补丁归属）。**未真机三态**；**未生成 `docs/logger/th06_checkpoints.csv`**（E2 属后续阶段）。 |
| 2026-10-22 | **素材对比口径订正（不改结论 / 地址）**：原文 §1 对比表与 §7.2 写「th07 / th08 无 `.ecl`/`.decl`」「th06 相对两作的最大优势」，与 2026-10-21 已补齐三作素材的事实矛盾（`_dump_report_a_gen.txt` 39/39）。现改为：**三作素材均已备齐，均可做 `SubN`（＝ sub 索引 N）离线映射**；th06 的差异只在于本报告做了 7/7 逐文件核验（th07 8 份 / th08 24 份的核对见各自 `thXX_checkpoint_entry_report.md`）。**未改任何地址、结论与数值。** |

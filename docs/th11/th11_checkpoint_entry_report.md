# TH11 checkpoint 段落入口表 — 白名单枚举小结（入口收口）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（总括：§1.2 latch 口径 / §6 Q9 / **§9 风险 9**——该条点名「th10 st1–st3 段入口落在 `main` 内部、sub 名粒度可能不足，**th11 需同法复核**」；本报告 §0/§4.2 即该复核结论）
> 需求来源：`docs/req_logger.md`
> 逐作逆向：`docs/th11/th11_ecl_re_report.md`（本表即该报告 **§10.4「白名单待枚举」** 的收口产物）
> 回填口径：`docs/logger/checkpoint_entry_backfill_workflow.md`（§3.3 章节行 / §3.4 boss 行 / **§5 分支关**）
> 数据文件：`docs/logger/th11_checkpoints.csv`（第 1 列 **125/125** 已回填：章节 37/37、boss 88/88）
> 二进制指纹：`../exes/th11.exe`，688128 B，PE `TimeDateStamp = 1217734394`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**）。引擎代：**B 代**；同代参照 th10 / th12 / th13。
> 方法：`capstone` 只读反汇编（th11 报告 §1–§9）+ 自写 SCPT 解析器对 `../exes/ecl/th11/*.ecl` 逐指令解码 + `../exes/ecl_dump/th11/*.decl` 对名；与 `thprac_th11.cpp` 的 `THStageWarp`（`:1063`）/ `THPatch`（`:1240`）里的 `ECLJump(start,dest)` / `SetFile(N)` 逐条对账。
> 状态：🟡 **静态枚举完毕（未真机三态）**。本机未运行 th11，全部结论未真机验证。
> 地址口径：正文以 **VA** 为主，RVA = VA − 0x400000。

---

## 0. 结论速览

| 项 | 结论 | 一句话 |
| --- | --- | --- |
| **入口粒度** | ✅ **sub 名粒度够用** | 每个 checkpoint 的 warp 落点**都落在 `call` / `async call`（或 boss 创建 `enmCreate`）上**，被调者/被创建者即入口 sub ⇒ 白名单可直接用 **sub 名**，**不需要** th11 报告 §10.4 里提的「`T+4` 换算成偏移」替代方案。 |
| **唯一例外** | 🟡 `ST3 后半 #1 = MBoss` | 该段落点落在 `main` 内的 **`enmCreate "MBoss"`**（opcode `0x101`）上，不是 `call`；被创建者名 `MBoss` **仍是 sub 名**，白名单照挂。 |
| **st4 分支关** | ✅ 6 机体 × 3 符卡 = **18 条**全部登记 | 走 A/B/C 路线（`stage4c0{0,1}{a,b,c}`）经 `SetFile(2..7)` 选槽；**白名单需同时收录各分支入口**（工作流 §5）。见 §2。 |
| **道中命名** | ✅ 一律 `MainSubNN` | `NN` 两位（`MainSub00`…`MainSub06b`）；段内多个 `MainSubNN` 是 `main` 的**兄弟 `call`/`async call`**（无 `MainFront`/`MainLatter` 容器）。 |
| **boss 命名** | ✅ `BossN` / `BossCardN` / `MBoss*` / `MBossCard*` | 非符 = `BossN`、符卡 = `BossCardN`；中 boss 同理加 `M` 前缀。ST7 末段用 `END_NSk`（非符）/ `END_Sk`（符卡）记录名。 |
| **难度分支** | ✅ 仅 ST1 MID3 一处 | `MID3_EN = MBoss3`（EN 下 `setNext` 目标）、`MID3_HL = MBossCard1`（HL 下目标）—— 由 `MBoss2` 内的 `!EN`/`!HL` 分叉决定。 |
| **实现影响** | 🟡 **须遍历宿主线程链 `H+0x1030`** | 本作大量段入口是 **`async` 落点**（独立线程），只读 `H+4` 会被本帧最后执行的 async 线程覆盖；须对线程链上每个线程的 `T+4` 各判一次。见 §4。 |
| **存疑项** | 🟡 4 处（照实记录） | ST3 后半 #1、ST3/ST5/ST6/ST7 的 MID1「容器 vs 战斗 sub」、ST3 MID2、ST7 前半 #4 / 后半 #3 疑似 thprac off-by-one。见 §4.3 / §5。 |

---

## 1. 入口表（CSV 第 1 列，按关分组）

> 数据源：`docs/logger/th11_checkpoints.csv`。左半「道中」= `THStageWarp`（warp 模式 1，章节行）；右半「boss」= `THPatch`（warp 模式 2/3，boss 行）。
> 记录名 ↔ portion 由 `mChapterSetup[7][2]` 决定（`thprac_th11.cpp:462`）：`{2,2}/{2,2}/{2,1}/{3,4}/{3,3}/{3,3}/{4,3}`。

### 1.1 ST1（道中 4 + boss 8）

| 道中记录名 | 入口 | boss 记录名 | 入口 |
| --- | --- | --- | --- |
| ST1 前半 #1 | `MainSub00` *(async)* | ST1 MID1 | `MBoss` |
| ST1 前半 #2 | `MainSub03` *(async)* | ST1 MID2 | `MBoss2` |
| ST1 后半 #1 | `MainSub06` | ST1 MID3_EN | `MBoss3` |
| ST1 后半 #2 | `MainSub07` *(async)* | ST1 MID3_HL | `MBossCard1` |
| | | ST1 BOSS1 | `Boss1` |
| | | ST1 BOSS2 | `BossCard1` |
| | | ST1 BOSS3 | `Boss2` |
| | | ST1 BOSS4 | `BossCard2` |

### 1.2 ST2（道中 4 + boss 7）

| 道中记录名 | 入口 | boss 记录名 | 入口 |
| --- | --- | --- | --- |
| ST2 前半 #1 | `MainSub00` *(async)* | ST2 MID1 | `MBoss` |
| ST2 前半 #2 | `MainSub03` *(async)* | ST2 MID2 | `MBossCard1` |
| ST2 后半 #1 | `MainSub05` | ST2 BOSS1 | `Boss1` |
| ST2 后半 #2 | `MainSub06` | ST2 BOSS2 | `BossCard1` |
| | | ST2 BOSS3 | `Boss2` |
| | | ST2 BOSS4 | `BossCard2` |
| | | ST2 BOSS5 | `BossCard3` |

### 1.3 ST3（道中 3 + boss 9）

| 道中记录名 | 入口 | boss 记录名 | 入口 |
| --- | --- | --- | --- |
| ST3 前半 #1 | `MainSub00` | ST3 MID1 | `MBoss1` 🟡 |
| ST3 前半 #2 | `MainSub01` | ST3 MID2 | `MBoss2` 🟡 |
| ST3 后半 #1 | `MBoss` 🟡 | ST3 MID3 | `MBossCard1` |
| | | ST3 BOSS1 | `Boss1` |
| | | ST3 BOSS2 | `BossCard1` |
| | | ST3 BOSS3 | `Boss2` |
| | | ST3 BOSS4 | `BossCard2` |
| | | ST3 BOSS5 | `Boss3` |
| | | ST3 BOSS6 | `BossCard3` |

### 1.4 ST4（道中 7 + boss 23）

| 道中记录名 | 入口 | boss 记录名 | 入口 |
| --- | --- | --- | --- |
| ST4 前半 #1 | `MainSub00` | ST4 MID1 | `MBoss` |
| ST4 前半 #2 | `MainSub03` *(async)* | ST4 MID2 | `MBoss2` |
| ST4 前半 #3 | `MainSub04` | ST4 BOSS1 | `Boss1` |
| ST4 后半 #1 | `MainSub05` | ST4 BOSS2 | `BossCard1` |
| ST4 后半 #2 | `MainSub06` | ST4 BOSS3 | `Boss2` |
| ST4 后半 #3 | `MainSub04` ⚠️ | ST4 RA1…MC3（18 行） | 见 §2 |
| ST4 后半 #4 | `MainSub06b` | | |

> ⚠️ **ST4 后半 #3 与前半 #3 同入口 `MainSub04`**：`main` 内两处 `call MainSub04`（`0xf848` / `0xf978`），latch 只读 sub 名 ⇒ **两行不可区分**（见 §5 风险 3）。

### 1.5 ST5（道中 6 + boss 10）

| 道中记录名 | 入口 | boss 记录名 | 入口 |
| --- | --- | --- | --- |
| ST5 前半 #1 | `MainSub00` | ST5 MID1 | `MBoss1` 🟡 |
| ST5 前半 #2 | `MainSub01` | ST5 MID2 | `MBoss2` |
| ST5 前半 #3 | `MainSub02` | ST5 MID3 | `MBossCard1` |
| ST5 后半 #1 | `MainSub04` | ST5 BOSS1 | `Boss1` |
| ST5 后半 #2 | `MainSub05` | ST5 BOSS2 | `BossCard1` |
| ST5 后半 #3 | `MainSub06` | ST5 BOSS3 | `Boss2` |
| | | ST5 BOSS4 | `BossCard2` |
| | | ST5 BOSS5 | `Boss3` |
| | | ST5 BOSS6 | `BossCard3` |
| | | ST5 BOSS7 | `BossCard4` |

### 1.6 ST6（道中 6 + boss 10）

| 道中记录名 | 入口 | boss 记录名 | 入口 |
| --- | --- | --- | --- |
| ST6 前半 #1 | `MainSub00` | ST6 MID1 | `MBossCard1` 🟡 |
| ST6 前半 #2 | `MainSub01` | ST6 BOSS1 | `Boss1` |
| ST6 前半 #3 | `MainSub02` | ST6 BOSS2 | `BossCard1` |
| ST6 后半 #1 | `MainSub03` | ST6 BOSS3 | `Boss2` |
| ST6 后半 #2 | `MainSub04` | ST6 BOSS4 | `BossCard2` |
| ST6 后半 #3 | `MainSub05` | ST6 BOSS5 | `Boss3` |
| | | ST6 BOSS6 | `BossCard3` |
| | | ST6 BOSS7 | `Boss4` |
| | | ST6 BOSS8 | `BossCard4` |
| | | ST6 BOSS9 | `BossCard5` |

### 1.7 ST7（道中 7 + boss 21）

| 道中记录名 | 入口 | boss 记录名 | 入口 |
| --- | --- | --- | --- |
| ST7 前半 #1 | `MainSub00` | ST7 MID1 | `MBossCard1` 🟡 |
| ST7 前半 #2 | `MainSub01` | ST7 MID2 | `MBossCard2` |
| ST7 前半 #3 | `MainSub02` | ST7 MID3 | `MBossCard3` |
| ST7 前半 #4 | `MainSub04` 🟡 | ST7 END_NS1 | `Boss1` |
| ST7 后半 #1 | `MainSub05` | ST7 END_S1 | `BossCard1` |
| ST7 后半 #2 | `MainSub06` | ST7 END_NS2 | `Boss2` |
| ST7 后半 #3 | `MainSub03` 🟡 | ST7 END_S2 | `BossCard2` |
| | | ST7 END_NS3 / END_S3 | `Boss3` / `BossCard3` |
| | | ST7 END_NS4 / END_S4 | `Boss4` / `BossCard4` |
| | | ST7 END_NS5 / END_S5 | `Boss5` / `BossCard5` |
| | | ST7 END_NS6 / END_S6 | `Boss6` / `BossCard6` |
| | | ST7 END_NS7 / END_S7 | `Boss7` / `BossCard7` |
| | | ST7 END_NS8 / END_S8 | `Boss8` / `BossCard8` |
| | | ST7 END_S9 / END_S10 | `BossCard9` / `BossCard10` |

> 计数核对：道中 4+4+3+7+6+6+7 = **37**；boss 8+7+9+23+10+10+21 = **88**；合计 **125** = CSV 行数。

---

## 2. st4 六个机体分支（**本作重点**）

### 2.1 机制

- st4 是 **Satori（こいし/さとり）复制弹幕关**：符卡脚本按「**路线（00 / 01）+ 机体**」分成 **6 份独立 ECL 文件**，由 `ECLHelper::SetFile(N)`（= `res+0xC+N*4`）选槽。
- `stage04.decl` 头：`ecli { "default.ecl"; "stage4c00a.ecl"; "stage4c00b.ecl"; "stage4c00c.ecl"; "stage4c01a.ecl"; "stage4c01b.ecl"; "stage4c01c.ecl"; }`
  ⇒ 按工作流 §3.1「slot 0 = 关卡主文件、slot K = `ecli[K−1]`」：**slot 0 = `stage04.ecl`（道中+容器）、slot 1 = `default.ecl`、slot 2..7 = 上述 6 份分支文件**。
- thprac 侧：`ECLSatoriJump(ecl, shot_type)`（`:1034`，`shot_type` 0..5 = RA/RB/RC/MA/MB/MC）先 `ECLJump(ecl, 0xf764, 0xfa04)`（创建 `Boss`），再 `ECLJump(ecl, 0x5820, 0x5870 + shot_type*0x8c)` —— 落在 **`stage04.ecl::BossCardSelect`** 内的 `call BossCard2_0{0,1}{A,B,C}`（符卡 1）；符卡 2/3 另 `SetFile(2..7)` + `ECLJump` 直接落到分支文件的 `BossCard3_*` / `BossCard4_*` **首指令**。

### 2.2 18 行对照表

| 机体 | 符卡 | CSV 记录名 / `th_sections_cba` 枚举 | `SetFile` 槽 | 分支文件 | 入口 sub | warp 落点 |
| --- | --- | --- | --- | --- | --- | --- |
| ReimuA | 1 | `ST4 RA1` / `TH11_ST4_RA1` | —（slot 0） | `stage4c00a` | `BossCard2_00A` | `stage04.ecl::BossCardSelect` 内 `call`（`0x5870`） |
| ReimuA | 2 | `ST4 RA2` / `TH11_ST4_RA2` | **2** | `stage4c00a` | `BossCard3_00A` | sub 首指令（`0x3f4`） |
| ReimuA | 3 | `ST4 RA3` / `TH11_ST4_RA3` | **2** | `stage4c00a` | `BossCard4_00A` | sub 首指令（`0x1aac`） |
| ReimuB | 1 | `ST4 RB1` / `TH11_ST4_RB1` | —（slot 0） | `stage4c00b` | `BossCard2_00B` | `BossCardSelect` 内 `call`（`0x58fc`） |
| ReimuB | 2 | `ST4 RB2` / `TH11_ST4_RB2` | **3** | `stage4c00b` | `BossCard3_00B` | sub 首指令（`0x1bd0`） |
| ReimuB | 3 | `ST4 RB3` / `TH11_ST4_RB3` | **3** | `stage4c00b` | `BossCard4_00B` | sub 首指令（`0x30e8`） |
| ReimuC | 1 | `ST4 RC1` / `TH11_ST4_RC1` | —（slot 0） | `stage4c00c` | `BossCard2_00C` | `BossCardSelect` 内 `call`（`0x5988`） |
| ReimuC | 2 | `ST4 RC2` / `TH11_ST4_RC2` | **4** | `stage4c00c` | `BossCard3_00C` | sub 首指令（`0x1ac0`） |
| ReimuC | 3 | `ST4 RC3` / `TH11_ST4_RC3` | **4** | `stage4c00c` | `BossCard4_00C` | sub 首指令（`0x2bfc`） |
| MarisaA | 1 | `ST4 MA1` / `TH11_ST4_MA1` | —（slot 0） | `stage4c01a` | `BossCard2_01A` | `BossCardSelect` 内 `call`（`0x5a14`） |
| MarisaA | 2 | `ST4 MA2` / `TH11_ST4_MA2` | **5** | `stage4c01a` | `BossCard3_01A` | sub 首指令（`0x1008`） |
| MarisaA | 3 | `ST4 MA3` / `TH11_ST4_MA3` | **5** | `stage4c01a` | `BossCard4_01A` | sub 首指令（`0x217c`） |
| MarisaB | 1 | `ST4 MB1` / `TH11_ST4_MB1` | —（slot 0） | `stage4c01b` | `BossCard2_01B` | `BossCardSelect` 内 `call`（`0x5aa0`） |
| MarisaB | 2 | `ST4 MB2` / `TH11_ST4_MB2` | **6** | `stage4c01b` | `BossCard3_01B` | sub 首指令（`0x7b8`） |
| MarisaB | 3 | `ST4 MB3` / `TH11_ST4_MB3` | **6** | `stage4c01b` | `BossCard4_01B` | sub 首指令（`0x12ac`） |
| MarisaC | 1 | `ST4 MC1` / `TH11_ST4_MC1` | —（slot 0） | `stage4c01c` | `BossCard2_01C` | `BossCardSelect` 内 `call`（`0x5b2c`） |
| MarisaC | 2 | `ST4 MC2` / `TH11_ST4_MC2` | **7** | `stage4c01c` | `BossCard3_01C` | sub 首指令（`0x1a0c`） |
| MarisaC | 3 | `ST4 MC3` / `TH11_ST4_MC3` | **7** | `stage4c01c` | `BossCard4_01C` | sub 首指令（`0x2a38`） |

**共 18 行**（6 机体 × 3 符卡），全部已登记进 CSV 第 1 列。

### 2.3 与 `th_sections_cba` 的对应

- 记录名与枚举**同名 1:1**：CSV `ST4 RA1` ↔ 枚举 `TH11_ST4_RA1`（`thprac_locale_def.h:2545-2562`），在 `th_sections_cba[3][1]`（ST4 的 **boss 行**）里按 `RA1/RA2/RA3/RB1…MC3` 顺序排列（`thprac_locale_def.cpp:15005` 起，TH11 段）。
- `SetFile` 槽号 ↔ 机体：**RA=2 / RB=3 / RC=4 / MA=5 / MB=6 / MC=7**（`thprac_th11.cpp:1479/1499/1525/1545/1565/1585`）。

### 2.4 白名单口径（工作流 §5「分支关」）

> **分支关天然兼容，但白名单必须同时收录各分支入口**：本口径读的是**运行期状态**（当前 sub 名），走哪条分支就读到哪条；6 份分支文件的 subs 都并入**同一张** `res+0x8C` 表（th11 报告 §3.2：每次加载 `res+8 += n` 并插入同一张有序表），故 18 个 `BossCard*_0XY` 名**必须全部在表内**，缺一即该分支 latch no-op。CSV 已 18/18 收录。

---

## 3. 其余命名规律

### 3.1 道中：`MainSubNN`

- 一律 `MainSub` + 两位编号（`MainSub00`…`MainSub06`），个别带字母后缀（`MainSub01b` / `MainSub06b`）。
- **本作没有 `MainFront` / `MainLatter` 容器**（与 th16 不同）：`main` 是**纯容器**，段内 `MainSubNN` 全是 `main` 的**兄弟 `call`(0x0B) / `async call`(0x0F)**。因此 warp 落点若在 `main` 内，落点本身就是那条 `call`/`async call`（例：st1 `0x10aa0 = async call MainSub00`、`0x10b28 = async call MainSub03`）。
- 部分段落点落在 **`enmCreate`（opcode `0x101`）** 上，被创建者名即入口 sub（见 §3.3）。
- 各关道中脚本所在文件：st1–st7 均在 `stageNN.ecl`（slot 0）；st5–st7 的**道中**亦在 `stageNN.ecl`，boss/midboss 另置 `stageNNmboss.ecl` / `stageNNboss.ecl`（`ecli` 序：**slot 2 = `stageNNmboss`、slot 3 = `stageNNboss`**）；st1–st4 的 boss/midboss **就在 `stageNN.ecl` 内**（`ecli` 仅 `{default}`）。

### 3.2 boss：`BossN` / `BossCardN` / `MBoss*` / `MBossCard*`

- **关底**：非符 = `BossN`（N=1..8），符卡 = `BossCardN`（N=1..10）；入口容器 = `Boss`（`Boss` 内 `call Boss1`）。
- **中 boss**：非符 = `MBoss` / `MBossN`，符卡 = `MBossCardN`；入口容器 = `MBoss`。
- **阶段推进**由 `setNext(slot,hp,time,name)` 串链：容器 `Boss`/`MBoss` 起首，`setNext` 指向下一阶段 sub（`Boss1 → BossCard1 → Boss2 → …`）。
- ST7 末段记录名用 `END_NSk`（非符）/ `END_Sk`（符卡），对应入口仍是 `Bossk` / `BossCardk`（k=1..10）。

### 3.3 `enmCreate` 型落点

- warp 落点若为 **`enmCreate`（opcode `0x101`，名字内嵌指令 `+0x14` 起）**，则**被创建者名**即入口 sub。实测例：
  - `stage01.ecl` `0x10bb0` = `enmCreate "MBoss"`（ST1 MID1）、`0x10d8c` = `enmCreate "Boss"`（ST1 BOSS1）；
  - `stage03.ecl` `0x14ba0` = `enmCreate "MBoss"`（**ST3 后半 #1**，本作唯一「非 `call` 落点」的章节行）；
  - `stage04.ecl` `0xfa04` = `enmCreate "Boss"`、`stage06.ecl` `0x41a4` = `enmCreate "MBoss"`、`stage07.ecl` `0x4d2c` = `enmCreate "MBoss"`。
- 因名字**仍是 sub 名**（`Boss` / `MBoss`），白名单口径不变（th11 报告 §10.4 收口注）。

### 3.4 难度分支：`MID3_EN` / `MID3_HL`

- **全作仅 ST1 一处**：`MBoss2` 内按难度分叉 —— `!EN → setNext(0, 4300, 0, "MBoss3")`、`!HL → setNext(0, 900, 0, "MBossCard1")`（`stage01.decl:1044` 起）。
- ⇒ `ST1 MID3_EN = MBoss3`（非符）、`ST1 MID3_HL = MBossCard1`（符卡）；两条**都**必须进白名单（难度是运行期选择）。

---

## 4. 实现方案影响

### 4.1 段入口多为 `async` 落点 ⇒ 须遍历宿主线程链

- 本作段落子脚本**大量经 `async`(0x0F) 起独立线程**（离线实体：`stage01.decl:2261` `@MainSub00() async;`；`stage01.ecl` `main` `0x10aa0 = async call MainSub00`）。这些线程挂在**同一宿主 `H`** 的线程链上，**不在** `main` 那个线程里。
- ⇒ 若只读 `T = *(u32*)(H+4)`（`H+4` = 本帧最后执行过的线程，每跑一个线程就刷新），读数**不稳定**。
- ⇒ **运行期须遍历宿主线程链 `H+0x1030`**（节点 `{ +0 = 线程 T, +4 = next, +8 = prev }`；**首节点线程恒为 `H+8` = 主线线程**），对每个线程的 `T+4` 各做一次 latch 判定；任一命中白名单即置值（口径要求「含 async 落点」，th11 报告 §5 Q4 / §10.5、总括 §5.1 B 代注记）。
- 读取链（th11 报告 §10.3 / §12）：
  ```text
  H = mEclHost                                  // 0x4204A3 / 0x4205E0 捕获（EAX = 宿主）
  res   = *(u32*)(H + 0x102C)                   // ≡ *(u32*)(*(u32*)0x4A8D7C + 0x64)
  count = *(i32*)(res + 8);  table = *(u32*)(res + 0x8C)   // 项 8B；+0=名字 char*，+4=起始，首指令 +4+0x10
  for (node = *(u32*)(H + 0x1030); node; node = *(u32*)(node + 4)) {
      T  = *(u32*)(node + 0);                   // 首节点 = H+8（主线线程）
      ip = *(u32*)(T + 4);                      // 当前指令绝对地址（Q1：地址语义，非索引）
      cur = NameOfAddress(table, count, ip);    // 区间反查（按 start_i 排序）
      if (section = Whitelist(game, stage, cur)) current = section;   // 未命中 no-op
  }
  ```

### 4.2 `main` 是纯容器、无 `MainFront`/`MainLatter`

- th16 那套「第一条 `ECLJump` 落 `MainFront`/`MainLatter`、第二条落 `MainSubNN`」的**两跳结构在本作不成立**：`main` 直接平铺 `call`/`async call`/`enmCreate` 序列。
- ⇒ 章节行的取值规则退化为**「最后一条 `ECLJump` 的 `dest` 落在哪条 `call`/`async call`/`enmCreate` 上」**；无第二条 `ECLJump` 的段（每个 `p1`）取 `main` 内**第一条** `call`/`async call` 的被调者（各关均为 `MainSub00`）。
- 对账结果：**37 个章节行全部由该规则唯一确定**（thprac `THStageWarp` 偏移 ↔ `.ecl` 指令一一对应，已逐条脚本复核）。

### 4.3 已知存疑项（照实记录，🟡）

1. **ST3 后半 #1 = `MBoss`**：该段落点落在 `main` 内 `enmCreate "MBoss"` 上（非 `call`），是本作**唯一**的「非 `call` 落点」章节行；若实现要求落点必须是 `call`/`async call`，此条需特判（按 `enmCreate` 名字处理）。
2. **ST3 / ST5 / ST6 / ST7 的 MID1「容器 vs 战斗 sub」**：这些 MID1 的 warp 落点是 **`enmCreate "MBoss"`**（或被 `call MainSubNN` 间接创建 `MBoss`），被创建的容器 `MBoss` 再 `call` 战斗 sub：
   - ST3 MID1 → `MBoss` → `call MBoss1`（`stage03.ecl` `0xb458`）⇒ CSV 记 `MBoss1`（**战斗 sub**）；
   - ST5 MID1 → `call MainSub03` → `enmCreate "MBoss"` → `MBoss` → `call MBoss1` ⇒ CSV 记 `MBoss1`；
   - ST6 MID1 → `enmCreate "MBoss"` + `SetFile(2)` 补丁 `stage06mboss.ecl` ⇒ `MBoss` → `call MBossCard1` ⇒ CSV 记 `MBossCard1`；
   - ST7 MID1 → `enmCreate "MBoss"` + `SetFile(2)` 补丁 `stage07mboss.ecl` ⇒ `MBoss` → `call MBossCard1` ⇒ CSV 记 `MBossCard1`。
   - ⚠️ 而 **ST3 后半 #1 记的是容器 `MBoss`**（上一条）。即同一处 `enmCreate` 落点，**章节行取容器、boss 行取战斗 sub** —— 口径不完全统一，**待真机/后续统一**。
3. **ST3 MID2 = `MBoss2`**：thprac 在 `MBoss` 内 `ECLJump(ecl, 0xb458, 0xb6c4)`（注释「Jump From MBoss To MBoss2」）强制跳到 `MBoss2` 代码 ⇒ 入口 `MBoss2`；该结论**依赖 thprac 的二次跳转注释**，静态可核但未真机。
4. **ST7 前半 #4 / 后半 #3 疑似 thprac off-by-one**：`THStageWarp` stage 7 `p4 dest = 0x4ce4 → call MainSub04`、`p7 dest = 0x4de4 → call MainSub03`；`MainSub03` 出现在 `MainSub06` **之后**（顺序反常）。CSV 与 thprac 偏移**逐字一致**，故疑点在 **thprac 自身偏移**（而非 CSV 转录）——需真机确认该两段入口是否确为 `MainSub04` / `MainSub03`。

---

## 5. 风险 / 待真机清单

> 本机未运行 th11；以下**全部未做**，状态一律 🟡。

1. **入口名真机对账**：逐段采样，确认 `NameOfAddress(T+4)` 与该段 CSV 入口一致（37 + 88 行）。
2. **async 线程选择（本作重点）**：含 `@MainSubNN() async` 的段落，比较「只读 `H+0x1030`」与「遍历整条线程链」两种取法的读数；确定 latch 采用哪种（th11 报告 §13 同条目）。
3. **⚠️ ST4 后半 #3 与前半 #3 同入口 `MainSub04`**：两行**不可区分**（latch 只读 sub 名）——需真机确认是否需改登记二级粒度，或接受两段同值。
4. **⚠️ st4 六机体 × 三符卡各走一遍**：分支选择（`SetFile(2..7)`）前后，`T+4` 名字与 18 条白名单映射均正确；分支切换瞬间不误判。
5. **多文件并集表**：确认 6 份 `stage4c0*` 与 `stageNNboss/mboss` 的 subs 确实进入**同一张** `res+0x8C` 表，且 `table[i].+4+0x10` 落在其所属文件缓冲内。
6. **§4.3 四处存疑**：ST3 后半 #1（`enmCreate` 落点）、ST3/ST5/ST6/ST7 MID1 的容器-vs-战斗 sub 口径、ST3 MID2 的二次跳转、ST7 前半 #4 / 后半 #3 的 thprac off-by-one。
7. **`enmCreate` 落点判读**：opcode `0x101` 的名字串取法（`+0x14` 起）在运行期是否稳定可得（本报告只做了离线验证）。
8. **难度分支**：`ST1 MID3_EN` / `MID3_HL` 两条在 E/N/H/L 四难度下各自命中正确的一条。
9. **练习跳章**：跳章后 `T+4` 名字与白名单置值均正确（thprac 报告 §13 同条目）；`ins[0xA]=0xFF` 掩码在四难度下均能执行。
10. **版本变体**：Steam / 汉化版是否共用同一 RVA（本报告地址来自原版 `ts = 1217734394`）。

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-17 | **建档（th11 白名单收口）**。把 `docs/logger/th11_checkpoints.csv` 第 1 列（**125/125**：章节 37/37、boss 88/88）整理为按关分组的**入口表**（§1）；给出 **st4 六机体 × 三符卡 = 18 行**分支对照表（§2：`SetFile` 槽 2..7 ↔ `stage4c00a/b/c` / `stage4c01a/b/c`，入口 `BossCard{2,3,4}_0{0,1}{A,B,C}`，对应 `th_sections_cba` 的 `TH11_ST4_RA1..MC3`；并写明白名单须同时收录各分支入口，工作流 §5）；归纳其余命名规律（§3：道中 `MainSubNN`、boss `BossN`/`BossCardN`/`MBoss*`、`enmCreate` 落点、难度分支 `MID3_EN`/`MID3_HL`）；记实现影响（§4：段入口**多为 async 落点** ⇒ 运行期须遍历宿主线程链 `H+0x1030`；`main` 为纯容器、**无** `MainFront`/`MainLatter`）；登记四处**已知存疑**（ST3 后半 #1、ST3/ST5/ST6/ST7 MID1 容器-vs-战斗 sub、ST3 MID2、ST7 前半 #4/后半 #3 疑似 thprac off-by-one）与待真机清单（§5）。**全部结论 🟡 未真机**。对账方法：`thprac_th11.cpp` 的 `THStageWarp`/`THPatch` 偏移 ↔ 自写 SCPT 解析器逐指令解码 `../exes/ecl/th11/*.ecl`。 |

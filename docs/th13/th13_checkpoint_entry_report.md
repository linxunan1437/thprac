# TH13 checkpoint「段落入口」白名单 — 入口表小结（116/116 已枚举）

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志）
> 总括（术语 / 判定口径以此为准）：`docs/logger/logger_re_disasm_overview.md`（**§1.2 判定口径 / §5.1 注记 / §6 Q9 / §9.9 粒度**）
> 本作逆向报告：`docs/th13/th13_ecl_re_report.md`（**§1 指针链 / §2.1 指令头布局 / §2.3 地址区间反查 / §3.2 sub 表项 `+4` / §3.3 `+0x10` 首指令 / §3.4 `GetMemAddr` 展开 / §4 调用栈 / §5.1 宿主捕获 / §10.4（已收口，指向本文）**）
> 回填口径：`docs/logger/checkpoint_entry_backfill_workflow.md`（**§3.3 章节行 / §3.4 boss 行 / §5 th13 `goto` opcode 注记**）
> 数据：`docs/logger/th13_checkpoints.csv`（**第 1 列 116/116**：章节 **44/44**、boss **72/72**）
> 参考格式：`docs/th10/th10_checkpoint_entry_report.md`（同批建档）、`docs/th14/th14_boss_phase_report.md`（风险 / 地址表写法）
> 二进制：`../exes/th13.exe`，**790528 B**，PE `TimeDateStamp = 1313589413`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）
> 素材：`../exes/ecl/th13/`（原始 `.ecl` **21** 份）、`../exes/ecl_dump/th13/`（`thecl -d` 反编译 `.decl` **21** 份）—— **仓库外只读，不提交**
> 方法：离线 SCPT sub 表解析（`python` + `struct`）+ 逐指令解码 + 对照 `thprac/src/thprac/thprac_th13.cpp` 的 `THStageWarp` / `THPatch` 常量 + `capstone` 复核（脚本在仓库外 `D:\workshop\reasm\th13\`，**不提交**）。
> 状态：**静态定案（🟡 未真机三态）**。本机无法运行 th13，**未做任何真机验证**；本文全部结论均标 🟡。
> 地址口径：正文以 **VA** 为主并标 **RVA = VA − 0x400000**；本文 / CSV 里的「偏移」一律是**相对该 sub 首条指令的字节偏移**（见 §2.1），与「相对文件缓冲的偏移」相差该 sub 的首指令基址。
> 置信度：✅ 离线解码 / 源码 / 反汇编直接证实 ｜ 🟡 有强证据但缺一环（显式标注）｜ ❌ 未能定案。

---

## 0. 结论速览

| # | 问题 | 结论 | 一句话 |
| --- | --- | --- | --- |
| 1 | 白名单是否枚举完毕 | ✅ **116/116** | 章节 **44/44**、boss **72/72**；th13 是 B 代四作（th10–th13）里与 th10 同批「章节 + boss 全表回填」的作。 |
| 2 | 道中行记法 | ✅ **纯 sub 名**为主（`MainSubNN` / `MainSubNNb` / `MainSubNNh`），**全作仅 1 行**用 `sub名+0x偏移` | 唯一带偏移的是 **`ST5 后半 #4` = `MainSub08+0x390`**（§2.1）。 |
| 3 | 偏移基准 | ✅ **相对该 sub 首条指令的字节偏移** | = sub 数据基址 `+0x10`（`table[i].+4 + 0x10`，报告 §3.3）；**与 th10 的 `main+0x28` 同一基准**。**用户已定案**：统一用第 1 列此记法，**不新增第 5 列**。 |
| 4 | boss 行记法 | ✅ **boss 脚本 sub 名**（`BossN` / `BossCardN` / `MBossN` / `MBossCardN`） | 无偏移后缀；与 th14–th18 的 boss 行同形（`stNNbs` = 关底、`stNNmbs` = 中boss）。 |
| 5 | 实现影响 | ✅ **双匹配**：sub 名 + ip 偏移（**偏移命中优先**）；⚠️ **3 行是 async-only** | 解析第 1 列时识别 `+0x` 后缀；**`ST1 后半 #2` / `ST4 前半 #1` / `ST5 前半 #1` 的入口只经 `async call` 进入** ⇒ 须遍历宿主线程链（§3.3）。 |
| 6 | 已知风险 | 🟡 **`ST4 前半 #1` 有 `!HL` 难度门控 + async-only**；离线解析 `goto` 有 3 个 opcode | `MainSub00h` 只在 Hard/Lunatic 派生 ⇒ 低难度下该行永不命中；`goto` 家族 = `0x0C/0x0D/0x0E`（§2.3）。 |

---

## 1. 入口表（CSV 第 1 列，按关分组）

> 左列 = `记录名`，右列 = `段落入口函数`；带 `+0x` 者为偏移记法（§2.1）。每关先列**道中行**、再列 **boss 行**。
> `记录名` 由 `thprac_th13.cpp:405` 的 `mChapterSetup[7][2] = {{3,2},{4,2},{4,4},{4,4},{3,4},{4,0},{3,3}}` 与 `thprac_locale_def.cpp` 的 `th_sections_cba[7][2][19]`（TH13 段）决定。

### ST1（道中 5 行 + boss 7 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST1 前半 #1 | `MainSub00` | | ST1 MID1 | `MBoss1` |
| ST1 前半 #2 | `MainSub01` | | ST1 BOSS1 | `Boss1` |
| ST1 前半 #3 | `MainSub03` | | ST1 BOSS2 | `BossCard1` |
| ST1 后半 #1 | `MainSub04` | | ST1 BOSS3 | `Boss2` |
| ST1 后半 #2 | `MainSub04b` *(async)* | | ST1 BOSS4 | `BossCard2` |
| | | | ST1 BOSS5 | `BossCard3` |
| | | | ST1 BOSS6 | `BossCard4` |

### ST2（道中 6 行 + boss 7 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST2 前半 #1 | `MainSub00` | | ST2 MID1 | `MBoss1` |
| ST2 前半 #2 | `MainSub01` | | ST2 MID2 | `MBossCard1` |
| ST2 前半 #3 | `MainSub02` | | ST2 BOSS1 | `Boss1` |
| ST2 前半 #4 | `MainSub03` | | ST2 BOSS2 | `BossCard1` |
| ST2 后半 #1 | `MainSub05` | | ST2 BOSS3 | `Boss2` |
| ST2 后半 #2 | `MainSub06` | | ST2 BOSS4 | `BossCard2` |
| | | | ST2 BOSS5 | `BossCard3` |

### ST3（道中 8 行 + boss 9 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST3 前半 #1 | `MainSub00` | | ST3 MID1 | `MBoss1` |
| ST3 前半 #2 | `MainSub01` | | ST3 MID2_EN | `MBoss2` |
| ST3 前半 #3 | `MainSub02` | | ST3 MID2_HL | `MBossCard1` |
| ST3 前半 #4 | `MainSub03` | | ST3 BOSS1 | `Boss1` |
| ST3 后半 #1 | `MainSub05` | | ST3 BOSS2 | `BossCard1` |
| ST3 后半 #2 | `MainSub06` | | ST3 BOSS3 | `Boss2` |
| ST3 后半 #3 | `MainSub07` | | ST3 BOSS4 | `BossCard2` |
| ST3 后半 #4 | `MainSub04b` | | ST3 BOSS5 | `Boss3` |
| | | | ST3 BOSS6 | `BossCard3` |

### ST4（道中 8 行 + boss 8 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST4 前半 #1 | `MainSub00h` *(async)* | | ST4 MID1 | `MBoss1` |
| ST4 前半 #2 | `MainSub01` | | ST4 MID2 | `MBossCard1` |
| ST4 前半 #3 | `MainSub00` *(async)* | | ST4 BOSS1 | `Boss1` |
| ST4 前半 #4 | `MainSub04` | | ST4 BOSS2 | `BossCard1` |
| ST4 后半 #1 | `MainSub00` | | ST4 BOSS3 | `Boss2` |
| ST4 后半 #2 | `MainSub07` | | ST4 BOSS4 | `BossCard2` |
| ST4 后半 #3 | `MainSub08` | | ST4 BOSS5 | `Boss3` |
| ST4 后半 #4 | `MainSub09` | | ST4 BOSS6 | `BossCard3` |

### ST5（道中 7 行 + boss 9 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST5 前半 #1 | `MainSub01` *(async)* | | ST5 MID1 | `MBoss1` |
| ST5 前半 #2 | `MainSub03` | | ST5 MID2 | `MBossCard1` |
| ST5 前半 #3 | `MainSub04` | | ST5 BOSS1 | `Boss1` |
| ST5 后半 #1 | `MainSub06` | | ST5 BOSS2 | `BossCard1` |
| ST5 后半 #2 | `MainSub07` | | ST5 BOSS3 | `Boss2` |
| ST5 后半 #3 | `MainSub08` | | ST5 BOSS4 | `BossCard2` |
| ST5 后半 #4 | **`MainSub08+0x390`** | | ST5 BOSS5 | `Boss3` |
| | | | ST5 BOSS6 | `BossCard3` |
| | | | ST5 BOSS7 | `BossCard4` |

### ST6（道中 4 行 + boss 11 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST6 #1 | `MainSub00` | | ST6 BOSS1 | `Boss1` |
| ST6 #2 | `MainSub01` | | ST6 BOSS2 | `BossCard1` |
| ST6 #3 | `MainSub02` | | ST6 BOSS3 | `Boss2` |
| ST6 #4 | `MainSub03` | | ST6 BOSS4 | `BossCard2` |
| | | | ST6 BOSS5 | `Boss3` |
| | | | ST6 BOSS6 | `BossCard3` |
| | | | ST6 BOSS7 | `Boss4` |
| | | | ST6 BOSS8 | `BossCard4` |
| | | | ST6 BOSS9 | `BossCard5` |
| | | | ST6 BOSS10 | `BossCard6` |
| | | | ST6 BOSS11 | `BossCard7` |

### ST7（道中 6 行 + boss 21 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST7 前半 #1 | `MainSub00` | | ST7 MID1 | `MBossCard1` |
| ST7 前半 #2 | `MainSub01` | | ST7 MID2 | `MBossCard2` |
| ST7 前半 #3 | `MainSub02` | | ST7 MID3 | `MBossCard3` |
| ST7 后半 #1 | `MainSub03` | | ST7 END_NS1 | `Boss1` |
| ST7 后半 #2 | `MainSub05` | | ST7 END_S1 | `BossCard1` |
| ST7 后半 #3 | `MainSub06` | | ST7 END_NS2 | `Boss2` |
| | | | ST7 END_S2 | `BossCard2` |
| | | | ST7 END_NS3 | `Boss3` |
| | | | ST7 END_S3 | `BossCard3` |
| | | | ST7 END_NS4 | `Boss4` |
| | | | ST7 END_S4 | `BossCard4` |
| | | | ST7 END_NS5 | `Boss5` |
| | | | ST7 END_S5 | `BossCard5` |
| | | | ST7 END_NS6 | `Boss6` |
| | | | ST7 END_S6 | `BossCard6` |
| | | | ST7 END_NS7 | `Boss7` |
| | | | ST7 END_S7 | `BossCard7` |
| | | | ST7 END_NS8 | `Boss8` |
| | | | ST7 END_S8 | `BossCard8` |
| | | | ST7 END_S9 | `BossCard9` |
| | | | ST7 END_S10 | `BossCard10` |

### 1.1 命名规律

- **道中行**：入口一律是关卡主文件 `stNN.ecl` 的 `MainSubNN` 家族 sub（`NN` = 两位十进制数字，**非连续**——warp 落点跳过「通用波次」sub，例：ST1 落 `00/01/03/04/04b`（跳过 `02`、`05`–`07`）、ST2 落 `00..03/05/06`（跳过 `04`、`07`）、ST5 落 `01/03/04/06/07/08`（跳过 `00/02/05`））。
  - 后缀变体照抄 sub 名：`MainSubNNb`（`ST1 后半 #2`、`ST3 后半 #4`）、`MainSubNNh`（`ST4 前半 #1`，带 `!HL` 难度门控，见 §4）。
  - `记录名`：该关有前 / 后拆分时记 `STn 前半 #k` / `STn 后半 #k`（拆分由 `mChapterSetup[7][2]` 的 前半 / 后半 决定）；**无拆分**（`后半 == 0`，本作仅 **ST6**）记 `STn #k`。`k` 在关内按 warp 顺序从 1 递增。
  - 容器 `main` / `MainFront` / `MainLatter` / `MainMBoss` **不入表**（口径见总括 §1.2）。
- **boss 行**：入口是 **boss 脚本的相位 sub** —— 关底段取 `stNNbs` 的 `BossN`（**非符**）/ `BossCardN`（**符卡**）；中boss 段取 `stNNmbs` 的 `MBossN`（非符）/ `MBossCardN`（符卡）。`记录名` 按 thprac `th_sections_cba[7][2][19]` 顺序记 `STn MIDm` / `STn BOSSj`；**ST7 例外**，关底段按 非/符 成对记 `STn END_NSj` / `STn END_Sj`（thprac `th_sections_cba` 同名）。
  - **ST7 的 3 个 MID 都直接落 `MBossCard1/2/3`**（`st07mbs` 的相位 sub 就是 `MBossCard1/2/3`，无 `MBoss1`）——即 `ST7 MID1 = MBossCard1`，与 ST1–ST6 的 `MID1 = MBoss1` 不同形。
  - ⚠️ **`BOSSj` 的 `j` 与 `BossN`/`BossCardN` 的 `N` 不保证交错**：例 `ST1 BOSS5/BOSS6 = BossCard3/BossCard4`（thprac 的 ST1 只提供 6 个 boss 目标，`Boss3`/`Boss4` 不入表；`THPatch` 里 `ST1_BOSS5` 置 `Spell Ordinal = 0x33`、`ST1_BOSS6` = `0x34` 佐证）。
  - boss 行**可纯离线定出**（不需要 boss 宿主 / 槽表 / 门控位）：`THPatch` 改 boss 容器内 `call Boss1` 的**名字串** + 改 `lifeSet` 操作数逼 `setNext` 立即转移，再与相位链交叉验证（见 `checkpoint_entry_backfill_workflow.md` §3.4）。

---

## 2. 口径与特例（本作重点）

### 2.1 `ST5 后半 #4` = `MainSub08+0x390`（**全作唯一**的两级粒度行）

- **格式**：`sub名+0x偏移`（例 `MainSub08+0x390`）。
- **偏移基准**：**相对该 sub 首条指令的字节偏移** —— `MainSub08` 的 sub 数据基址 `0x5130`、**首条指令 `0x5140`**，warp 落点 **`0x54d0`** ⇒ `0x54d0 − 0x5140 = 0x390`。（与 th10 的 `main+0x28` 同基准；`sub 数据基址 + 0x10` 为偏移 0。）
- **为什么必须用偏移**：`ST5 后半 #3` 与 `#4` **同属 sub `MainSub08`**，纯 sub 名粒度下无法区分（会互相误命中）⇒ 用同一 sub 内的两个不同落点消歧（同 th10 `ST5 后半 #1/#2` 的 `MainSub8+0x14`/`+0x444` 手法）。
- **机制**（可复现，`thprac_th13.cpp:1107`）：

  | 段 | thprac 补丁（`THStageWarp` stage 5） | 落点 → 入口 |
  | --- | --- | --- |
  | ST5 后半 #3（portion 6） | `ECLJump(ecl, 0x60e4, 0x6458, 60, 90)`；`ECLJump(ecl, 0x2850, 0x2898, 0, 0)` | `main` → `call MainLatter`；`MainLatter` 的 2nd 指令（`0x2850`）被改成 `goto 0x2898` = `call MainSub08` ⇒ **`MainSub08`**（正常进入，从 `0x5140` 起跑） |
  | ST5 后半 #4（portion 7） | 同上 + `ECLJump(ecl, 0x5154, 0x54d0, 0, 0)` | 再把 **`MainSub08` 的 2nd 指令（`0x5154`）改成 `goto 0x54d0`** ⇒ 入口 `MainSub08` 立即被重定向到 **`0x54d0` = `MainSub08+0x390`** |

  > 佐证：`st05.ecl` 内 `MainSub08` 的指令流在 `0x54d0` 处正好是**指令边界**（`0x5140`–`0x54d0` 共 0x390 B，逐条按 `size` 步进可整除）；`0x54d0` 仍在 `MainSub08` 的数据区间内（下一 sub `MapleEnemy` 从 `0x5828` 起）。
- **用户已定案**：统一用第 1 列的 `sub名+0x偏移` 记法（与 th10 一致），**不新增第 5 列**。

### 2.2 另两处**同名容忍项**（照实记录）

| 行 | 入口 | 同名情况 |
| --- | --- | --- |
| **`ST4 前半 #3`** | `MainSub00` | 与 **`ST4 后半 #1`** 入口**完全同名**（`MainSub00`），二者**无任何可区分信息**（既不同 sub、也无偏移后缀）⇒ 白名单键不唯一，靠 latch 口径容忍（命中即置值，无法区分是哪一行）。 |
| **`ST5 后半 #3`** | `MainSub08` | 该纯 sub 名同时是 **`ST5 后半 #4`**（`MainSub08+0x390`）的**基名**；`#4` 靠 `+0x390` 消歧，`#3` 只能靠「未命中 `+0x390` 时回落纯名」。（实现上须保证**偏移命中优先**，见 §3.1。） |

> 说明（证据）：`ST4` 的 `MainFront` 依次 `@MainSub00h() async; @MainSub00(); … @MainSub00() async; …`，warp 落点分别为 `0x3f74`（`async MainSub00h`）、`0x4058`（`async MainSub00`）；`ST4 后半 #1` 经 `main → call MainMBoss`（`0x8848`）+ `MainMBoss` 的 `0x42d0` = `call MainSub00` 进入。即**两行确实指向同名 sub**。
> 这两项属**口径外推 / 同名容忍**，不是笔误：th13 未出现 th10 st1–st3 那种「段落入口落在容器内部、必须靠偏移」的普遍情形（**仅 §2.1 一处**）。

### 2.3 离线解析工具坑：th13 的 `goto` 有**三个** opcode

- **文件内**的 `goto Label @ time` 家族**不止一个 opcode**（`.decl` 一律渲染成 `goto <sub>_<off> @ <time>`）：

  | opcode | 语义 | 解释器 handler（VA / RVA） | 本作出现次数（21 份 `.ecl` 合计） |
  | --- | --- | --- | --- |
  | **`0x0C`** | **无条件**跳转（= thprac 注入的那种） | `0x47758E` / `0x7758E` | 817（全部**前跳**） |
  | **`0x0D`** | 条件跳转 = `unless (cond) goto Label` | `0x477559` / `0x77559` | 349（全部**前跳**） |
  | **`0x0E`** | 条件跳转 = `if (cond) goto Label` | `0x477523` / `0x77523` | 608（全部**后跳**，即循环） |

  三者**指令布局一致**：`+0x00` int 时间 ｜ `+0x04` u16 opcode ｜ `+0x06` u16 size(`0x18`) ｜ `+0x08` u32 掩码 ｜ **`+0x10` i32 = 目标相对偏移（`dest − 本指令地址`）** ｜ `+0x14` i32 = `@ time`。⇒ 解析器按 `id ∈ {0x0C,0x0D,0x0E}` 读 `+0x10` 即可拿到目标。
- **thprac 注入的 goto 恒为 `0x0C`**（`thprac_th13.cpp:943` 的 `ECLJump` 写死 `0x0C`/size `0x18`）。
- **坑**：**只认 `0x0C` 会导致离线解析失步** —— 文件内共 **957 条**（349 + 608）条件跳转会被当普通指令，`.decl` ↔ `.ecl` 的对齐随之错位。本仓仓库外工具 `D:\workshop\reasm\th13\boss.py` 已按 `ins['id'] in (0x0C, 0x0D)` 双认（`goto_target()` 的断言处），但仍**未纳入 `0x0E`**（如日后扩用请一并补上）。
- 该注记已同步登记于 `docs/logger/checkpoint_entry_backfill_workflow.md` §5。

---

## 3. 实现方案影响（本次重点）

### 3.1 双匹配：sub 名 + ip 偏移（**偏移命中优先**）

th13 的 logger 实现（**尚未实现**，`logger_progress.md` §1：E5 ❌）须**同时支持两种匹配**：

| 匹配 | 判据 | 适用行 |
| --- | --- | --- |
| **sub 名匹配** | `cur = *(char**)(table + i*8)` ∈ 白名单 | 115 行纯 sub 名的行（th14–th18 同形） |
| **ip 偏移匹配** | `ip` 命中该行的 `(sub 名, 偏移)` | **仅 1 行**：`ST5 后半 #4` = `MainSub08+0x390` |

- **优先级：偏移命中 > 名字命中**（`ST5 后半 #3`（纯名 `MainSub08`）与 `#4`（`MainSub08+0x390`）共存 ⇒ 不这样定序会让 `#4` 恒被 `#3` 抢走）。
- **解析第 1 列时识别 `+0x` 后缀**（**不是**读第 5 列 —— 用户已否决新增列）：无后缀 → 纯 sub 名；有后缀 → 拆成 `(sub 名, 偏移)`。
- th13 **尚无监控实现** ⇒ 「按 ip 偏移匹配」是**新实现的设计选择**，非迁移；th14–th18 按 sub 名匹配、**完全不受影响**。

### 3.2 指针链（运行期取「当前指令 ip」→「偏移」）

```text
H     = 主线 ECL 宿主（捕获点 0x42C76B / 0x42C90A，命中 EAX = H；报告 §5.1）
T     = *(u32*)(H + 0x11D0)                     // 主线内嵌线程（≡ *(u32*)(H + 4)）；报告 §5.3
ip    = *(u32*)(T + 4)                          // 当前（下一条待执行）指令的【绝对地址】；0 = 线程已结束（报告 §2.1）
res   = *(u32*)(H + 0x11CC)                     // ≡ *(u32*)(*(u32*)0x4C2188 + 0xAC)（报告 §3.1/§3.4）
count = *(i32*)(res + 8)
table = *(u32*)(res + 0x8C)                     // 项 8B：+0 = 名字 char*，+4 = sub 数据基址（首指令 = +4+0x10）
base  = *(u32*)(res + 0xC + slot*4)             // ★ slot 的文件缓冲基址（章节行固定 slot 0 = stNN.ecl）
off   = ip - base                               // 相对文件缓冲的字节偏移
```

- **ip → sub 名**（本作**无 sub 索引**，必须区间反查；报告 §2.3）：取 `start_i = *(u32*)(table + i*8 + 4) + 0x10`，**按 `start_i` 升序排序**（表项本身按名字字典序存放，`+4` 不单调），得区间 `[start_i, start_{i+1})`；`name = 满足 start_i <= ip < start_{i+1} 的 table[i].+0`。
- **CSV 第 1 列的偏移 → 运行期判据**：CSV 的 `+0x` 是**相对该 sub 首条指令**的偏移，故运行期要么
  - (a) 直接按地址比：`ip == table[i].+4 + 0x10 + 偏移`（`i` = 该 sub 名在**运行期 sub 表**的下标）；或
  - (b) 预先把该行换算成**文件偏移** `(table[i].+4 + 0x10 + 偏移) − base`，再与 `off = ip − base` 比对。
  - 两者等价（相差该 sub 的首指令基址）。`ST5 后半 #4`：`ip == table[MainSub08].+4 + 0x10 + 0x390`。
- **`ip == 0`**（线程已结束）必须先判、再算偏移（否则 `off` 下溢）。

### 3.3 ⚠️ **订正总括 §5.1 注记**：th13 **并非全同步 `call` 进入**，有 **3 行是 async-only**

总括 §5.1 注记写「th13 为同步 `call` 进入，无此问题（不需遍历线程链）」。**本作离线实测该说法不成立**：

- th13 的 **`async call`（opcode `0x0F`）确实另起线程**：handler `0x477363` → `FUN_004793E0`（`malloc(0x11C4)` 造线程对象，挂进宿主线程链 `H+0x11D0`/`H+0x11D4`，再 `FUN_00477030` 压帧）。⇒ 异步入口 sub 跑在**另一条线程**上，主线 T 的 `T+4` **不会**指向它。
- 逐行核对（`stNN.ecl` 的 `main` / `MainFront` / `MainLatter` / `MainMBoss` 全部 `call`/`async call` 站点）：**入口 sub 只经 `async call` 进入（无任何同步 `call` 站点）的共 3 行**：

  | 行 | 入口 | 唯一站点 | 备注 |
  | --- | --- | --- | --- |
  | `ST1 后半 #2` | `MainSub04b` | `st01.ecl` `MainLatter` `0x5540` `async` | warp 落点即该 async（portion 5：`ECLJump(0x5510,0x5540)`） |
  | `ST4 前半 #1` | `MainSub00h` | `st04.ecl` `MainFront` `0x3f74` `async` | 另有 `!HL` 难度门控（§4） |
  | `ST5 前半 #1` | `MainSub01` | `st05.ecl` `MainFront` `0x270c` `async` | portion 1 无 `ECLJump` ⇒ 取容器首个子，恰是 async |

- 另有 `ST4 前半 #3`（`MainSub00`）的 **warp 落点**（`0x4030` → 之后第一条 `call`/`async` 是 `0x4058` `async MainSub00`）也走 async；但该 sub 另有同步站点（`MainFront 0x3f94`、`MainMBoss 0x42d0/0x42f0/0x4310`），故**不构成 async-only**。
- **实现结论**：th13 须与 th10–th12 同法 —— **遍历宿主线程链**（`T = *(u32*)(H+0x11D0)` 为主线；后继节点自 `*(u32*)(H+0x11D4)` 起，节点 `{+0 = T, +4 = next, +8 = prev}`）**逐线程读 `T+4`**，任一命中白名单即置值。（只读主线 T 会让上表 3 行永不命中 ⇒ 段落停在上一值。）

### 3.4 基址**不可缓存**

- `res`、各 slot 的**文件缓冲基址 `base`** 都在堆上（`res+0xC+slot*4` 由 SCPT 加载器 `FUN_004795B0` 填）⇒ **每帧沿指针链重读**，**不可跨帧 / 跨局缓存地址**（与 th15 正式实现的「不缓存任何指针」一致）。
- `H` 只在进关时由 `0x42C76B`/`0x42C90A` 抓一次（`EAX`）；**清理点**沿用既有 `th13_game_start`（`0x42D3D7`）。
- **slot 号**：本表章节行固定 **slot 0**（`stNN.ecl`）；boss 行在 `stNNbs` / `stNNmbs` 槽（`SetFile(N)` 的含义**逐关不同**，一律按该关 `.decl` 头的 `ecli{}` 读；报告 §3.4）—— 若日后扩到 boss 行须一并登记槽号。

---

## 4. 风险 / 待真机清单

> 本机**未做**任何真机验证；以下**全部未做**，状态一律 🟡（静态定案、未真机三态）。

1. 🟡 **全静态，未真机**：入口表来自离线 SCPT 解析 + 逐指令解码 + `capstone` 复核 + 引用 th13 报告；**未真机三态**。
2. 🟡 **宿主捕获与指针链可读**：`0x42C76B`/`0x42C90A` 抓 `EAX` 非空；进关后 `H+0x11D0`、`H+0x11CC` 均可解引用且 `H+0x11CC == *(u32*)(mgr+0xAC)`（`mgr = *(u32*)0x4C2188`）。
3. 🟡 **地址 → sub 反查（区间表）**：同一段落内 `T+4` 应始终落在**同一个** `[start_i, start_{i+1})` 区间；跨段落时切换。`T+4 == 0` 只应在线程结束（过关 / 退关）出现。
4. 🟡 **偏移消歧（§2.1）**：warp 到 `ST5 后半 #4` 后，帧边界读到 `ip` 恰为 `table[MainSub08].+4 + 0x10 + 0x390`；再 warp 到 `ST5 后半 #3`，确认读到 `MainSub08` 首指令附近而**不**命中 `+0x390`。
5. 🟡 **同名容忍（§2.2）**：`ST4 前半 #3` 与 `ST4 后半 #1` 是否真的无法区分（两行同 `MainSub00`）—— 需真机确认显示行为（latch 会停在哪个 `记录名`）。
6. 🟡 **线程链（§3.3，最需确认）**：warp 到 `ST1 后半 #2` / `ST4 前半 #1` / `ST5 前半 #1`，确认入口 ip 出现在 **`H+0x11D4` 起的 async 线程**上；只读 `H+0x11D0`（主线）会漏。
7. 🟡 **`ST4 前半 #1` 的 `!HL` 门控**：`MainSub00h` 只在 **Hard/Lunatic** 派生（`st04.decl` `MainFront` 首行 `$I0 = 1; !HL @MainSub00h() async;`）⇒ Easy/Normal 下该行**永不命中**；须真机在 E/N 下确认（可能需把该行改判给 `MainSub00` 或容忍）。
8. 🟡 **`goto` opcode（§2.3）**：离线解析须认 `0x0C/0x0D/0x0E` 三个 opcode；`ECLJump` 写入的 `ins[0x0A] = 0xFF`（任意难度）在 E/N/H/L 下均生效（否则 `goto` 会被 `test byte [T+0x101C], cl` 跳过 —— 报告 §13 点名的最需真机确认项）。
9. 🟡 **采样时机**：确认 `Update()` 相对 ECL VM 的执行顺序（帧内 `goto`/`call` 是否一定在采样前执行完）—— 决定「精确匹配」是否够用；`goto` 之后、目标 `call` 之前的那一瞬 `T+4` 仍指向旧 sub 内（报告 §9.2 第 4 条）。
10. ✅ **boss 行（方案二）—— 缺口已消解（2026-10-18，静态）**：本表 boss 行**入口名已枚举**；原先记的「th13 **boss 宿主 / 槽表 / 门控位尚无逆向锚点**」**已由 `docs/th13/th13_ecl_re_report.md` §5.4（Q-A/Q-B/Q-C）定案**：
    - **Q-A**：宿主链 `mgr+0xB0` 是**全局全宿主链**（含 boss）—— 全程序唯一宿主分配点 `FUN_004183F0`（`push 0x52E4` 仅 `0x4183FC`）**无条件**入链 ⇒ **遍历这条链即可拿到 boss 宿主，无需新增捕获点**。
    - **Q-B**：boss / 中boss 宿主由 ECL **`enmCreateA`（opcode `0x12D`）** 另起（`0x41B2EC` → `FUN_0041FCC0` → `call 0x4183F0` @ `0x41FE62`，名字 = 指令 `ins+0x14`）；备选捕获点 **`0x41FE67`/RVA `0x1FE67`**（5B，`EAX` = 新宿主）。
    - **Q-C**：boss 宿主的 `H+0x11CC` = **同一个全局 `res`**（构造器 `FUN_00418F40` @ `0x4191EB` 无条件写 `*(u32*)(mgr+0xAC)`），其 `res+0x8C` 表**含 `stNNbs`/`stNNmbs` 的全部 sub**（`BossN`/`BossCardN`/`MBoss*`/`MBossCardN`）⇒ **「地址 → 名字」区间反查对 boss 行成立**。
    - 本节 §3.4 末条「若日后扩到 boss 行须一并登记槽号」**仅对「按偏移匹配」仍成立**（偏移要用 `res+0xC+slot*4` 的文件基址）；**按 sub 名匹配则不需要槽号**（boss sub 与章节 sub 同在 `res+0x8C` 这一张表里）。
    - 残余 🟡：以上**全部为静态定案**，boss 行仍需真机确认（报告 §13 新增项 / 监控方案 §7 验证 6）。
11. 🟡 **变体**：Steam / 汉化版的 `stNN.ecl` 是否与原版一致（偏移是**数据偏移**，脚本改版即失效）。

---

## 5. 与其它文档的关系

- **口径 / Q9 / 粒度** → `docs/logger/logger_re_disasm_overview.md` §1.2 / §6 Q9 / §9.9。
- **回填流程** → `docs/logger/checkpoint_entry_backfill_workflow.md`（本文是其 §3.3 章节行口径在 th13 的落地；§5 收 `goto` opcode 注记）。
- **本作逆向报告** → `docs/th13/th13_ecl_re_report.md`（**§10.4 已收口、指向本文**；Q1–Q9 与指针链见其 §1–§13）。
- **同批入口表小结** → `docs/th10/th10_checkpoint_entry_report.md`（偏移记法的先行者，本文 §2.1 与其同基准）。
- **参考格式** → `docs/th14/th14_boss_phase_report.md`（风险 / 地址表写法）；**成品数据** → `docs/logger/th13_checkpoints.csv`。
- **仓库外只读工具（不提交）** → `D:\workshop\reasm\th13\`：`chapters.py`（warp 表 ↔ 容器 call 序列）、`boss.py`（boss 相位链 / 名字串补丁）、`write_csv.py`（回填 CSV 第 1 列）、`eclutil.py`（SCPT 解析）。

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-17 | **建档**。汇总 th13 checkpoint 白名单（**116/116**：章节 44/44、boss 72/72）：§1 按关列出全部入口（道中 + boss）并给出命名规律（道中 `MainSubNN`，`NN` 非连续、跳过通用波次 sub；关底 `BossN`/`BossCardN`、中boss `MBossN`/`MBossCardN`；ST7 三个 MID 直接落 `MBossCard1/2/3`）；§2 记录口径与特例（**`ST5 后半 #4` = `MainSub08+0x390` 为全作唯一两级粒度行，偏移基准 = 相对该 sub 首条指令（`MainSub08` 首指令 `0x5140`、落点 `0x54d0`），用户已定案不新增第 5 列**；`ST4 前半 #3`/`ST5 后半 #3` 两处**同名容忍**照实记录；**离线解析工具坑：th13 文件内 `goto` 有 `0x0C`（无条件）/`0x0D`（`unless`）/`0x0E`（`if`）三个 opcode，thprac 注入恒为 `0x0C`，只认 `0x0C` 会失步**）；§3 实现影响（双匹配 sub 名 + ip 偏移、偏移命中优先；指针链 `ip=*(u32*)(T+4)` / `base=*(u32*)(res+0xC+slot*4)` / `off=ip−base`；**订正总括 §5.1「th13 为同步 `call` 进入」——实测 `async call`（`0x0F`→`FUN_004793E0` 另起线程）导致 `ST1 后半 #2`/`ST4 前半 #1`/`ST5 前半 #1` 三行为 async-only，须遍历宿主线程链 `H+0x11D0`→`H+0x11D4`**；基址不可缓存）；§4 风险 / 待真机清单 11 条。**全部 🟡 未真机三态。** |
| 2026-10-18 | **§4 第 10 条改判：boss 缺口消解（静态）**。原「th13 **boss 宿主 / 槽表 / 门控位尚无逆向锚点** ⇒ boss 行匹配待补逆向」改为 **✅ 已由 `docs/th13/th13_ecl_re_report.md` §5.4（Q-A/Q-B/Q-C）定案**：宿主链 `mgr+0xB0` 为**全局全宿主链**（唯一分配点 `FUN_004183F0` 无条件入链）⇒ 遍历即得 boss 宿主、**无需新增捕获点**；boss / 中boss 宿主由 ECL `enmCreateA`（opcode `0x12D`，`0x41B2EC` → `FUN_0041FCC0` → `call 0x4183F0` @ `0x41FE62`）另起，备选捕获点 `0x41FE67`/`0x1FE67`；boss 宿主 `H+0x11CC` = 同一个全局 `res`（构造器 `FUN_00418F40` @ `0x4191EB`），其 `res+0x8C` 表含 `stNNbs`/`stNNmbs` 全部 sub ⇒ 「地址 → 名字」反查对 boss 行成立。同时补注：§3.4 末条「扩到 boss 行须登记槽号」**仅对按偏移匹配仍成立**，按 sub 名匹配不需要槽号。**仍为静态定案，boss 行待真机。** |

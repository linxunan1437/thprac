# TH07 checkpoint「时间轴位置」回填 — 入口表小结（140/140）

> 上位文档：`docs/logger/logger_progress.md`（E3/E4 阶段定义、§1 进度总表）、`docs/logger/logger_re_disasm_overview.md`（术语 / §6 Q9 latch 口径）。
> 需求来源：`docs/req_logger.md`。
> 本作逆向报告（**技术口径唯一来源**）：`docs/th07/th07_ecl_re_report.md`（§10.2 / §10.3 读取方案＝读时间值、入口列记时间值；§11 候选值表）。
> 回填口径：`docs/logger/checkpoint_csv_workflow.md`（**A 代第 1 列表头＝`时间轴位置`**）、`docs/logger/checkpoint_entry_backfill_workflow.md`（A 代只借「自检 / 证据」结构，技术口径以本作报告为准）。
> 数据：`docs/logger/th07_checkpoints.csv`（140 行数据＝**章节 37 + boss 103**；第 1 列 **140/140** 已回填，无留空；`ST6 #2` 于 2026-10-22 按用户裁决删除，见 §3.3）。
> 二进制：`../exes/th07.exe`，**650752 B**，PE `TimeDateStamp = 1066942337`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ `RVA = VA − 0x400000`），`sha256 = f7d03ba1bf6733a60beb3935e0efbbf1436c3d28b4f2155e7e410ba806ce0b9f`（本机复核，与 `decompile_materials.md` §2 一致）。
> 素材：`../exes/ecl/th07/ecldata1..8.ecl`（8 份原始 ECL）、`../exes/ecl_dump/th07/ecldata1..8.decl`（`thecl -d7`；**无 eclmap ⇒ 指令名为 `ins_NNN`**，但 `sub SubN()` / `timeline TimelineN()` 块名与 `+N: //M` 时间注释有效）—— **仓库外只读，不提交**。
> 方法：`thprac_th07.cpp` 源码逐行实读（`mChapterSetup:411-420`、`ECLTimeWarp:891-898`、`THStageWarp:975-1133`、`THPatch:1134-1834`）＋ 离线 ECL 字节解析（`python` + `struct`）＋ `.decl` 逐指令 1:1 对齐交叉验证（**762 个 sub + 17 个 timeline、22413 条实指令** ＋ 16 条 `-1` 终止符）。
> 状态：**静态定案（🟡 未真机三态）**。本轮回填**未编译、未运行游戏、未改任何 exe / ecl / decl**。
> 地址口径：`VA` / `RVA = VA − 0x400000`；本文「偏移」一律是**相对 ECL 文件（运行期 `buf = *(u32*)0x1347938`）的字节偏移**。
> 置信度：`🟡A`＝源码直读 + decl 有**同时间戳指令**精确命中；`🟡B`＝源码直读 + decl **只有落点指令**（`ECLTimeWarp` 语义是「跳到时间点」，不要求 ECL 里存在同时间戳指令，见 §1.4）。**全部 🟡（未真机）**。

---

## 0. 结论速览

| # | 问题 | 结论 | 一句话 |
| --- | --- | --- | --- |
| 1 | 第 1 列填了多少 | ✅ **140/140** | 章节 37/37、boss 103/103；**无留空行**（`ST6 #2` 已按裁决删除）。 |
| 2 | 填的是什么 | ✅ **十进制时间值** | ＝`ECLTimeWarp(<count>, <time>)` 的 `time` 入参；无 `0x`、无单位、无 sub 名。 |
| 3 | 值从哪来 | ✅ **源码直读** | 章节＝`THStageWarp`（37 处；`ST6 #2` 已删）、boss＝`THPatch`（103 处，其中 4 处经 `yuyuko()` 间接调用）。 |
| 4 | dlg 双值怎么取 | ✅ **10 行取非 dlg（较大值）** | 实现期若命中 `thPracParam.dlg` 分支，需要 **−1 容差**（§1.3）。 |
| 5 | ECL 侧能否验证 | ✅ **8/8 文件、22413 条实指令全对齐** | boss **100/103** 精确命中 `ins_8(0)`/`ins_9(0)`；3 行中boss 无精确时间戳、只有落点（§1.4 / §4）。 |
| 6 | 同时间多分节 | 🟡 **15 组** | 副键＝补丁落点所属 sub（`SubN` + decl 行号）＋ `thFakeShot`（§3）；**ST4 三姐妹的 BOSS1 组在 ECL 侧完全没有差异**。 |
| 7 | 跨类型同值 | ✅ **1 处、已裁决** | `ST6 #2`（章节）与 `ST6 CHAOS`（boss 轴）同为 1180 ⇒ **保留 CHAOS、删除 #2**（§3.3）。 |
| 8 | 与 §11 的差异 | ⚠️ **值无错、表述有错** | §11.1 列名 / 列位错（`后半 #5..#8`）、§11.2 是分组汇总而非逐行（§5）。**未改 `th07_ecl_re_report.md`**，留主代理收口。 |
| 9 | 真机 | ❌ **未做** | 全部 🟡；待验清单见 §8 / §7.7。 |

---

## 1. 口径（A 代与 B/C 代的差异，先读这一节）

### 1.1 第 1 列记什么

- **格式**：纯十进制整数字符串，例 `540`、`1341`、`5043`。**不写 `0x`、不写单位、不写 sub 名、不写 `count`**。
- **语义**：`ECLTimeWarp(count, time)` 的 `time` 入参（`thprac_th07.cpp:891-898` 写入 `TL_0+8 … TL_{count-1}+8`，步长 `0x10`）。
- **为什么不是 sub 名**：th07 的 A 代 ECL **运行期不保留 sub 名**（报告 §7 Q6），章节定位走**时间轴** ⇒ 入口列只能记时间值（报告 §10.3）。

### 1.2 行 ↔ section 的对应（**不按 CSV 行序硬套**）

- **章节行**：`portion` ↔ 记录名由 `mChapterSetup[8][2]`（`thprac_th07.cpp:411-420`）决定 —— `portion` 从 1 连续编号，前 `前半` 个 → `STn 前半 #k`；其后 → `STn 后半 #k`；`后半 == 0` 的关 → `STn #k`。
- **boss 行**：记录名 ↔ `TH07` 枚举名（`thprac_locale_def.h:2114-2219`），逐字对应（`ST4 BOSS1_LUNASA` ↔ `TH07_ST4_BOSS1_LUNASA` 等）。**ST4 的 BOSS4/5 三姐妹在 CSV 里交错排列**，本表与 CSV 同序，但行↔section 一律以**记录名**为准。
- 自检：103 个 boss 记录名与 `THPatch` 的 103 个 `case` **一一对应、无缺无多**；37 个章节行与 `mChapterSetup` 之和逐一相等（ST6 少 1 行，见 §3.3）。

| 关卡 | `mChapterSetup`（前半,后半） | `THStageWarp` case 数 | 章节行 | 值（按 portion 顺序） |
| --- | --- | --- | --- | --- |
| ST1 | {2,1} | 3 | 3（前半 #1/前半 #2/后半 #1） | 540, 1341, 3107 |
| ST2 | {1,1} | 2 | 2（前半 #1/后半 #1） | 390, 3366 |
| ST3 | {2,1} | 3 | 3（前半 #1/前半 #2/后半 #1） | 390, 854, 1805 |
| ST4 | {4,4} | 8 | 8（前半 #1/前半 #2/前半 #3/前半 #4/后半 #1/后半 #2/后半 #3/后半 #4） | 80, 1948, 3028, 4288, 7964, 10136, 11396, 13166 |
| ST5 | {3,1} | 4 | 4（前半 #1/前半 #2/前半 #3/后半 #1） | 440, 840, 2550, 4883 |
| ST6 | {2,0} | 2 | 1（#1；#2 已删） | 660 |
| ST7 | {5,3} | 8 | 8（前半 #1/前半 #2/前半 #3/前半 #4/前半 #5/后半 #1/后半 #2/后半 #3） | 485, 1035, 1955, 2615, 3305, 4028, 5808, 7088 |
| ST8 | {5,3} | 8 | 8（前半 #1/前半 #2/前半 #3/前半 #4/前半 #5/后半 #1/后半 #2/后半 #3） | 485, 965, 1595, 2485, 3191, 3928, 5708, 6988 |

> 8 关 = `TH0X_STAGE_SELECT` 的第 1–8 项（ST7=Extra、ST8=Phantasm）；`THStageWarp` 各关 case 数与 `mChapterSetup` 之和**逐关相等**（3,2,3,8,4,2,8,8），无冲突。

### 1.3 dlg / 非 dlg 双值（**一律取非 dlg**）

全表 **10 行**带 `if (thPracParam.dlg)` 双值，CSV 一律取**非 dlg（较大值）**：

| 记录名 | dlg 值 | **非 dlg（已填）** | 源码行 |
| --- | --- | --- | --- |
| ST1 BOSS1 | 5042 | **5043** | `thprac_th07.cpp:1173` |
| ST2 BOSS1 | 7646 | **7647** | `thprac_th07.cpp:1206` |
| ST3 BOSS1 | 3393 | **3394** | `thprac_th07.cpp:1247` |
| ST4 BOSS1_LUNASA | 14826 | **14827** | `thprac_th07.cpp:1294` |
| ST4 BOSS1_MERLIN | 14826 | **14827** | `thprac_th07.cpp:1302` |
| ST4 BOSS1_LYRICA | 14826 | **14827** | `thprac_th07.cpp:1310` |
| ST5 BOSS1 | 6113 | **6114** | `thprac_th07.cpp:1403` |
| ST6 BOSS1 | 2518 | **2519** | `thprac_th07.cpp:1451` |
| ST7 END_NS1 | 8798 | **8799** | `thprac_th07.cpp:1534` |
| ST8 END_NS1 | 9258 | **9259** | `thprac_th07.cpp:1707` |

> **实现期注意**：dlg 值恒为「非 dlg − 1」。ECL 侧已交叉验证该 ±1 的**字形**：dlg 值处是 `ins_8(0)`、非 dlg 值处紧接 `ins_9(0)`，warp 到较大值会**跳过** `ins_8` 而执行 `ins_9`（§1.4 证据 1）。两者的业务语义（推测为「带 / 不带对话的 boss 触发」）🟡 未真机，本报告只登记「成对存在」这一事实。

### 1.4 ECL 侧交叉验证方法（本轮新增，2026-10-21 素材）

**（1）th07 `.ecl` 头格式（本机 8/8 实读，与 th08 不同，勿照搬）**

```text
0x00  u16 subCount
0x02  u16 timelineCount        // ＝运行期 *(u16*)(buf+2)（报告 §2.2 的 count）
0x04  u32 tlStart[timelineCount]
...   u32 endOffset            // 紧跟 tlStart 数组，＝文件长度
0x10..0x43  零填充（本作 8 文件 timelineCount ≤ 3）
0x44  u32 subOffset[subCount]  // 首个子代码紧接表尾
      sub 代码区   [0x44+4*subCount, tlStart[0])
      timeline 区  [tlStart[i], tlStart[i+1])，末条到 endOffset
```

- 指令头 **8 B**：`i16 time@+0`（绝对时间）· `u16 未知@+2` · `u16 opcode@+4` · `u16 size@+6`（**含 8 B 头**，下一条 = `+size`），操作数自 `+8` 起。与报告 §2.3 的反汇编（`movsx [ip]` / `[ip+4]` / `[ip+6]`）**逐字段吻合**。
- `sub` 与 `timeline` 是**两套并列的脚本区**，`call sub id` 的 `id` 即 `subOffset` 下标（`= SubN` 的 N）。

**（2）`.decl` 新事实**：`thecl -d7` 输出里**同时有 `sub SubN()` 与 `timeline TimelineN()` 两类块**（本作 762 + 17 个），`+N: //M` 的 `//M` 是**绝对时间**。报告 §7.2 / §12 写的「本机无 `.ecl`/`.decl`」已过期 —— 素材 2026-10-21 才入本机。

**（3）对齐验证**：把 `.decl` 每条 `ins_N(...)` 与 `.ecl` 每条指令**按序 1:1 比对**，8 个文件的全部 sub / timeline（22413 条实指令）均 **opcode 逐条相等**（含每条 timeline 末尾的 `time=-1` 收尾记录 —— decl 把它渲染成 `65535:` 标签 + 一行 `ins_*`，故仍 1:1；本作 17 条 timeline 中有 16 条带该收尾记录）⇒「文件偏移 ↔ decl 行号」互查可信。

**（4）由此得到的两条结构性证据**

1. **boss 触发序列**：**带 dlg 分支的 10 行**，其 dlg 值 T−1 处是 `ins_8(0)`、非 dlg 值 T 处是 `ins_9(0)` —— **10 对全部命中**（§1.3 表）；无 dlg 的 boss 行同样落在同一条 `ins_9(0)` 上。中boss 则是 `ins_8(2)`（ST5/6/7/8）或 `ins_0(...)`（生成敌机，如 ST2 `ins_0(40,…)`、ST4 `ins_0(35,…)`）。opcode 的业务含义未在此展开（🟡）。
2. **章节行是「提前量」**：37 行里 **36 行**在 Timeline0 有落点（`ST4 后半 #4`=13166 超出 Timeline0 最大值 12870，只落在 Timeline1），其中 **26 行**落点恰为 `T+60`（1 秒），另有 `+0 / +2 / +37 / +44 / +53 / +120` 等。⇒ `ECLTimeWarp` 是「把时间跳到 T」，**不要求 ECL 里存在时间戳恰为 T 的指令**；因此章节行多为 `🟡B`，只有 1 行（`ST3 前半 #2`=854）精确命中。

---

## 2. 逐关入口表（140 行）

> 按 CSV 行序（章节行 → MID → BOSS/END）。`count` 列＝源码 `ECLTimeWarp` 的第 1 参。decl 列 `TimelineN:L###` 中 `L###` 是 `.decl` 行号。

### ST1（章节 3 + boss 6 = 9 行）

| 记录名 | 时间值 | count | 来源（源码行号） | decl 交叉验证（块 + 行号） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST1 前半 #1 | **540** | 2 | `thprac_th07.cpp:980` | Timeline0:L1936（落点 600，+60）；Timeline1:L2448（落点 1401，+861） | 🟡B |
| ST1 前半 #2 | **1341** | 2 | `thprac_th07.cpp:983` | Timeline0:L2032（落点 1608，+267）；Timeline1:L2448（落点 1401，+60） | 🟡B |
| ST1 后半 #1 | **3107** | 2 | `thprac_th07.cpp:986` | Timeline0:L2158（落点 3167，+60）；Timeline1:L2496（落点 3531，+424） | 🟡B |
| ST1 MID1 | **2656** | 2 | `thprac_th07.cpp:1163` | Timeline0:L2156（无精确时间戳；落点 2667，+11，`ins_0(20, 0.0f, 0.0f, 0.0f, 10000, 0, -2, 100000)`） | 🟡B |
| ST1 MID2 | **2656** | 2 | `thprac_th07.cpp:1166` | Timeline0:L2156（无精确时间戳；落点 2667，+11，`ins_0(20, 0.0f, 0.0f, 0.0f, 10000, 0, -2, 100000)`） | 🟡B |
| ST1 BOSS1 | **5043** | 2 | `thprac_th07.cpp:1173` | Timeline1:L2522（t=5043 精确，`ins_9(0)`） | 🟡A |
| ST1 BOSS2 | **5043** | 2 | `thprac_th07.cpp:1179` | Timeline1:L2522（t=5043 精确，`ins_9(0)`） | 🟡A |
| ST1 BOSS3 | **5043** | 2 | `thprac_th07.cpp:1184` | Timeline1:L2522（t=5043 精确，`ins_9(0)`） | 🟡A |
| ST1 BOSS4 | **5043** | 2 | `thprac_th07.cpp:1189` | Timeline1:L2522（t=5043 精确，`ins_9(0)`） | 🟡A |

### ST2（章节 2 + boss 7 = 9 行）

| 记录名 | 时间值 | count | 来源（源码行号） | decl 交叉验证（块 + 行号） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST2 前半 #1 | **390** | 3 | `thprac_th07.cpp:992` | Timeline0:L2926（落点 450，+60）；Timeline1:L3185（落点 600，+210）；Timeline2:L3485（落点 600，+210） | 🟡B |
| ST2 后半 #1 | **3366** | 3 | `thprac_th07.cpp:995` | Timeline0:L3013（落点 3426，+60）；Timeline1:L3283（落点 3820，+454） | 🟡B |
| ST2 MID1 | **2826** | 3 | `thprac_th07.cpp:1194` | Timeline0:L3011（t=2826 精确，`ins_0(40, -32.0f, -32.0f, 0.0f, 17000, 0, 0, 60000)`） | 🟡A |
| ST2 MID2 | **2826** | 3 | `thprac_th07.cpp:1197` | Timeline0:L3011（t=2826 精确，`ins_0(40, -32.0f, -32.0f, 0.0f, 17000, 0, 0, 60000)`） | 🟡A |
| ST2 BOSS1 | **7647** | 3 | `thprac_th07.cpp:1206` | Timeline0:L3167（t=7647 精确，`ins_9(0)`） | 🟡A |
| ST2 BOSS2 | **7647** | 3 | `thprac_th07.cpp:1209` | Timeline0:L3167（t=7647 精确，`ins_9(0)`） | 🟡A |
| ST2 BOSS3 | **7647** | 3 | `thprac_th07.cpp:1213` | Timeline0:L3167（t=7647 精确，`ins_9(0)`） | 🟡A |
| ST2 BOSS4 | **7647** | 3 | `thprac_th07.cpp:1217` | Timeline0:L3167（t=7647 精确，`ins_9(0)`） | 🟡A |
| ST2 BOSS5 | **7647** | 3 | `thprac_th07.cpp:1222` | Timeline0:L3167（t=7647 精确，`ins_9(0)`） | 🟡A |

### ST3（章节 3 + boss 10 = 13 行）

| 记录名 | 时间值 | count | 来源（源码行号） | decl 交叉验证（块 + 行号） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST3 前半 #1 | **390** | 2 | `thprac_th07.cpp:1004` | Timeline0:L2393（落点 450，+60） | 🟡B |
| ST3 前半 #2 | **854** | 2 | `thprac_th07.cpp:1007` | Timeline0:L2419（t=854 精确，`ins_12(0)`） | 🟡A |
| ST3 后半 #1 | **1805** | 2 | `thprac_th07.cpp:1010` | Timeline0:L2577（落点 1858，+53） | 🟡B |
| ST3 MID1 | **821** | 2 | `thprac_th07.cpp:1231` | Timeline0:L2417（无精确时间戳；落点 853，+32，`ins_0(13, 80.0f, -16.0f, 0.0f, 17000, 0, -2, 30000)`） | 🟡B |
| ST3 MID2 | **1858** | 2 | `thprac_th07.cpp:1235` | Timeline0:L2577（t=1858 精确，`ins_0(22, 80.0f, -16.0f, 0.0f, 17000, 0, -2, 30000)`） | 🟡A |
| ST3 MID3 | **1858** | 2 | `thprac_th07.cpp:1239` | Timeline0:L2577（t=1858 精确，`ins_0(22, 80.0f, -16.0f, 0.0f, 17000, 0, -2, 30000)`） | 🟡A |
| ST3 BOSS1 | **3394** | 2 | `thprac_th07.cpp:1247` | Timeline0:L2818（t=3394 精确，`ins_9(0)`） | 🟡A |
| ST3 BOSS2 | **3394** | 2 | `thprac_th07.cpp:1251` | Timeline0:L2818（t=3394 精确，`ins_9(0)`） | 🟡A |
| ST3 BOSS3 | **3394** | 2 | `thprac_th07.cpp:1256` | Timeline0:L2818（t=3394 精确，`ins_9(0)`） | 🟡A |
| ST3 BOSS4 | **3394** | 2 | `thprac_th07.cpp:1264` | Timeline0:L2818（t=3394 精确，`ins_9(0)`） | 🟡A |
| ST3 BOSS5 | **3394** | 2 | `thprac_th07.cpp:1270` | Timeline0:L2818（t=3394 精确，`ins_9(0)`） | 🟡A |
| ST3 BOSS6 | **3394** | 2 | `thprac_th07.cpp:1275` | Timeline0:L2818（t=3394 精确，`ins_9(0)`） | 🟡A |
| ST3 BOSS7 | **3394** | 2 | `thprac_th07.cpp:1281` | Timeline0:L2818（t=3394 精确，`ins_9(0)`） | 🟡A |

### ST4（章节 8 + boss 16 = 24 行）

| 记录名 | 时间值 | count | 来源（源码行号） | decl 交叉验证（块 + 行号） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST4 前半 #1 | **80** | 2 | `thprac_th07.cpp:1020` | Timeline0:L6146（落点 200，+120）；Timeline1:L6993（落点 2200，+2120） | 🟡B |
| ST4 前半 #2 | **1948** | 2 | `thprac_th07.cpp:1023` | Timeline0:L6396（落点 2008，+60）；Timeline1:L6993（落点 2200，+252） | 🟡B |
| ST4 前半 #3 | **3028** | 2 | `thprac_th07.cpp:1026` | Timeline0:L6416（落点 3088，+60）；Timeline1:L7119（落点 8024，+4996） | 🟡B |
| ST4 前半 #4 | **4288** | 2 | `thprac_th07.cpp:1029` | Timeline0:L6443（落点 4348，+60）；Timeline1:L7119（落点 8024，+3736） | 🟡B |
| ST4 后半 #1 | **7964** | 2 | `thprac_th07.cpp:1032` | Timeline0:L6733（落点 11122，+3158）；Timeline1:L7119（落点 8024，+60） | 🟡B |
| ST4 后半 #2 | **10136** | 2 | `thprac_th07.cpp:1035` | Timeline0:L6733（落点 11122，+986）；Timeline1:L7497（落点 10196，+60） | 🟡B |
| ST4 后半 #3 | **11396** | 2 | `thprac_th07.cpp:1038` | Timeline0:L6775（落点 11398，+2）；Timeline1:L7524（落点 11456，+60） | 🟡B |
| ST4 后半 #4 | **13166** | 2 | `thprac_th07.cpp:1041` | Timeline1:L7556（落点 13226，+60） | 🟡B |
| ST4 MID1 | **7122** | 2 | `thprac_th07.cpp:1287` | Timeline0:L6731（t=7122 精确，`ins_0(35, -16.0f, 80.0f, 0.0f, 10, 0, -1, 10000)`） | 🟡A |
| ST4 BOSS1_LUNASA | **14827** | 2 | `thprac_th07.cpp:1294` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS1_MERLIN | **14827** | 2 | `thprac_th07.cpp:1302` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS1_LYRICA | **14827** | 2 | `thprac_th07.cpp:1310` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS2_LUNASA | **14827** | 2 | `thprac_th07.cpp:1315` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS2_MERLIN | **14827** | 2 | `thprac_th07.cpp:1321` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS2_LYRICA | **14827** | 2 | `thprac_th07.cpp:1327` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS3 | **14827** | 2 | `thprac_th07.cpp:1333` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS4_LUNASA | **14827** | 2 | `thprac_th07.cpp:1338` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS5_LUNASA | **14827** | 2 | `thprac_th07.cpp:1344` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS4_MERLIN | **14827** | 2 | `thprac_th07.cpp:1352` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS5_MERLIN | **14827** | 2 | `thprac_th07.cpp:1358` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS4_LYRICA | **14827** | 2 | `thprac_th07.cpp:1366` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS5_LYRICA | **14827** | 2 | `thprac_th07.cpp:1372` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS6 | **14827** | 2 | `thprac_th07.cpp:1380` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |
| ST4 BOSS7 | **14827** | 2 | `thprac_th07.cpp:1385` | Timeline1:L7560（t=14827 精确，`ins_9(0)`） | 🟡A |

### ST5（章节 4 + boss 8 = 12 行）

| 记录名 | 时间值 | count | 来源（源码行号） | decl 交叉验证（块 + 行号） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST5 前半 #1 | **440** | 2 | `thprac_th07.cpp:1049` | Timeline0:L2483（落点 500，+60）；Timeline1:L2699（落点 2701，+2261） | 🟡B |
| ST5 前半 #2 | **840** | 2 | `thprac_th07.cpp:1052` | Timeline0:L2489（落点 900，+60）；Timeline1:L2699（落点 2701，+1861） | 🟡B |
| ST5 前半 #3 | **2550** | 2 | `thprac_th07.cpp:1055` | Timeline0:L2569（落点 2610，+60）；Timeline1:L2699（落点 2701，+151） | 🟡B |
| ST5 后半 #1 | **4883** | 2 | `thprac_th07.cpp:1058` | Timeline0:L2627（落点 4943，+60） | 🟡B |
| ST5 MID1 | **4820** | 2 | `thprac_th07.cpp:1390` | Timeline0:L2618（t=4820 精确，`ins_8(2)`） | 🟡A |
| ST5 MID2 | **4820** | 2 | `thprac_th07.cpp:1395` | Timeline0:L2618（t=4820 精确，`ins_8(2)`） | 🟡A |
| ST5 BOSS1 | **6114** | 2 | `thprac_th07.cpp:1403` | Timeline0:L2679（t=6114 精确，`ins_9(0)`） | 🟡A |
| ST5 BOSS2 | **6114** | 2 | `thprac_th07.cpp:1406` | Timeline0:L2679（t=6114 精确，`ins_9(0)`） | 🟡A |
| ST5 BOSS3 | **6114** | 2 | `thprac_th07.cpp:1410` | Timeline0:L2679（t=6114 精确，`ins_9(0)`） | 🟡A |
| ST5 BOSS4 | **6114** | 2 | `thprac_th07.cpp:1415` | Timeline0:L2679（t=6114 精确，`ins_9(0)`） | 🟡A |
| ST5 BOSS5 | **6114** | 2 | `thprac_th07.cpp:1421` | Timeline0:L2679（t=6114 精确，`ins_9(0)`） | 🟡A |
| ST5 BOSS6 | **6114** | 2 | `thprac_th07.cpp:1430` | Timeline0:L2679（t=6114 精确，`ins_9(0)`） | 🟡A |

### ST6（章节 2 + boss 13 = 15 行）

| 记录名 | 时间值 | count | 来源（源码行号） | decl 交叉验证（块 + 行号） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST6 #1 | **660** | 2 | `thprac_th07.cpp:1066` | Timeline0:L2953（落点 720，+60） | 🟡B |
| ST6 CHAOS | **1180** | 2 | `thprac_th07.cpp:1435` | Timeline0:L2969（t=1180 精确，`ins_0(11, 224.0f, 112.0f, 0.0f, 80, 0, 2, 1500)`） | 🟡A |
| ST6 MID1 | **1914** | 2 | `thprac_th07.cpp:1438` | Timeline0:L3047（t=1914 精确，`ins_8(2)`） | 🟡A |
| ST6 MID2 | **1914** | 2 | `thprac_th07.cpp:1443` | Timeline0:L3047（t=1914 精确，`ins_8(2)`） | 🟡A |
| ST6 BOSS1 | **2519** | 2 | `thprac_th07.cpp:1451` | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS2 | **2519** | 2 | `thprac_th07.cpp:1458` | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS3 | **2519** | 2 | `thprac_th07.cpp:1158`（`yuyuko()` 内） | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS4 | **2519** | 2 | `thprac_th07.cpp:1158`（`yuyuko()` 内） | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS5 | **2519** | 2 | `thprac_th07.cpp:1158`（`yuyuko()` 内） | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS6 | **2519** | 2 | `thprac_th07.cpp:1158`（`yuyuko()` 内） | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS7 | **2519** | 2 | `thprac_th07.cpp:1486` | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS8 | **2519** | 2 | `thprac_th07.cpp:1492` | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS9 | **2519** | 2 | `thprac_th07.cpp:1503` | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |
| ST6 BOSS10 | **2519** | 2 | `thprac_th07.cpp:1508` | Timeline0:L3060（t=2519 精确，`ins_9(0)`） | 🟡A |

### ST7（章节 8 + boss 21 = 29 行）

| 记录名 | 时间值 | count | 来源（源码行号） | decl 交叉验证（块 + 行号） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST7 前半 #1 | **485** | 2 | `thprac_th07.cpp:1077` | Timeline0:L3333（落点 545，+60） | 🟡B |
| ST7 前半 #2 | **1035** | 2 | `thprac_th07.cpp:1080` | Timeline0:L3351（落点 1095，+60） | 🟡B |
| ST7 前半 #3 | **1955** | 2 | `thprac_th07.cpp:1083` | Timeline0:L3443（落点 2015，+60） | 🟡B |
| ST7 前半 #4 | **2615** | 2 | `thprac_th07.cpp:1086` | Timeline0:L3515（落点 2675，+60） | 🟡B |
| ST7 前半 #5 | **3305** | 2 | `thprac_th07.cpp:1089` | Timeline0:L3530（落点 3365，+60） | 🟡B |
| ST7 后半 #1 | **4028** | 2 | `thprac_th07.cpp:1092` | Timeline0:L3559（落点 4065，+37） | 🟡B |
| ST7 后半 #2 | **5808** | 2 | `thprac_th07.cpp:1096` | Timeline0:L3660（落点 5868，+60） | 🟡B |
| ST7 后半 #3 | **7088** | 2 | `thprac_th07.cpp:1099` | Timeline0:L3722（落点 7148，+60） | 🟡B |
| ST7 MID1 | **4065** | 2 | `thprac_th07.cpp:1513` | Timeline0:L3559（t=4065 精确，`ins_8(2)`） | 🟡A |
| ST7 MID2 | **4065** | 2 | `thprac_th07.cpp:1518` | Timeline0:L3559（t=4065 精确，`ins_8(2)`） | 🟡A |
| ST7 MID3 | **4065** | 2 | `thprac_th07.cpp:1523` | Timeline0:L3559（t=4065 精确，`ins_8(2)`） | 🟡A |
| ST7 END_NS1 | **8799** | 2 | `thprac_th07.cpp:1534` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S1 | **8799** | 2 | `thprac_th07.cpp:1539` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_NS2 | **8799** | 2 | `thprac_th07.cpp:1544` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S2 | **8799** | 2 | `thprac_th07.cpp:1550` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_NS3 | **8799** | 2 | `thprac_th07.cpp:1557` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S3 | **8799** | 2 | `thprac_th07.cpp:1563` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_NS4 | **8799** | 2 | `thprac_th07.cpp:1570` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S4 | **8799** | 2 | `thprac_th07.cpp:1576` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_NS5 | **8799** | 2 | `thprac_th07.cpp:1583` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S5 | **8799** | 2 | `thprac_th07.cpp:1589` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_NS6 | **8799** | 2 | `thprac_th07.cpp:1596` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S6 | **8799** | 2 | `thprac_th07.cpp:1602` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_NS7 | **8799** | 2 | `thprac_th07.cpp:1609` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S7 | **8799** | 2 | `thprac_th07.cpp:1615` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_NS8 | **8799** | 2 | `thprac_th07.cpp:1622` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S8 | **8799** | 2 | `thprac_th07.cpp:1628` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S9 | **8799** | 2 | `thprac_th07.cpp:1635` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |
| ST7 END_S10 | **8799** | 2 | `thprac_th07.cpp:1651` | Timeline0:L3826（t=8799 精确，`ins_9(0)`） | 🟡A |

### ST8（章节 8 + boss 22 = 30 行）

| 记录名 | 时间值 | count | 来源（源码行号） | decl 交叉验证（块 + 行号） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST8 前半 #1 | **485** | 2 | `thprac_th07.cpp:1106` | Timeline0:L3996（落点 545，+60） | 🟡B |
| ST8 前半 #2 | **965** | 2 | `thprac_th07.cpp:1109` | Timeline0:L4032（落点 1025，+60） | 🟡B |
| ST8 前半 #3 | **1595** | 2 | `thprac_th07.cpp:1112` | Timeline0:L4108（落点 1655，+60） | 🟡B |
| ST8 前半 #4 | **2485** | 2 | `thprac_th07.cpp:1115` | Timeline0:L4212（落点 2545，+60） | 🟡B |
| ST8 前半 #5 | **3191** | 2 | `thprac_th07.cpp:1118` | Timeline0:L4227（落点 3235，+44） | 🟡B |
| ST8 后半 #1 | **3928** | 2 | `thprac_th07.cpp:1121` | Timeline0:L4261（落点 3965，+37） | 🟡B |
| ST8 后半 #2 | **5708** | 2 | `thprac_th07.cpp:1125` | Timeline0:L4362（落点 5768，+60） | 🟡B |
| ST8 后半 #3 | **6988** | 2 | `thprac_th07.cpp:1128` | Timeline0:L4424（落点 7048，+60） | 🟡B |
| ST8 MID1 | **3965** | 2 | `thprac_th07.cpp:1685` | Timeline0:L4261（t=3965 精确，`ins_8(2)`） | 🟡A |
| ST8 MID2 | **3965** | 2 | `thprac_th07.cpp:1690` | Timeline0:L4261（t=3965 精确，`ins_8(2)`） | 🟡A |
| ST8 MID3 | **3965** | 2 | `thprac_th07.cpp:1695` | Timeline0:L4261（t=3965 精确，`ins_8(2)`） | 🟡A |
| ST8 END_NS1 | **9259** | 2 | `thprac_th07.cpp:1707` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S1 | **9259** | 2 | `thprac_th07.cpp:1712` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_NS2 | **9259** | 2 | `thprac_th07.cpp:1717` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S2 | **9259** | 2 | `thprac_th07.cpp:1723` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_NS3 | **9259** | 2 | `thprac_th07.cpp:1730` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S3 | **9259** | 2 | `thprac_th07.cpp:1736` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_NS4 | **9259** | 2 | `thprac_th07.cpp:1743` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S4 | **9259** | 2 | `thprac_th07.cpp:1749` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_NS5 | **9259** | 2 | `thprac_th07.cpp:1756` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S5 | **9259** | 2 | `thprac_th07.cpp:1762` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_NS6 | **9259** | 2 | `thprac_th07.cpp:1769` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S6 | **9259** | 2 | `thprac_th07.cpp:1775` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_NS7 | **9259** | 2 | `thprac_th07.cpp:1782` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S7 | **9259** | 2 | `thprac_th07.cpp:1788` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_NS8 | **9259** | 2 | `thprac_th07.cpp:1795` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S8 | **9259** | 2 | `thprac_th07.cpp:1801` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S9 | **9259** | 2 | `thprac_th07.cpp:1808` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S10 | **9259** | 2 | `thprac_th07.cpp:1815` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |
| ST8 END_S11 | **9259** | 2 | `thprac_th07.cpp:1826` | Timeline0:L4595（t=9259 精确，`ins_9(0)`） | 🟡A |

---

## 3. 同时间多分节（副键）

> 单靠时间值无法区分同值的多个分节。**副键**＝该 section 在 `THPatch` 里下的补丁落在**哪个 sub 的哪条指令**（decl 可查），必要时再加 `thFakeShot`。全部 🟡（未真机）。

### 3.1 概览（15 组，100 行）

| # | 时间值 | 关 | 行数 | 成员 | 副键可行性 |
| --- | --- | --- | --- | --- | --- |
| 1 | **2656** | ST1 | 2 | ST1 MID1, ST1 MID2 | ✅ 补丁集不同（见 §3.2 逐行） |
| 2 | **5043** | ST1 | 4 | ST1 BOSS1, ST1 BOSS2, ST1 BOSS3, ST1 BOSS4 | ✅ 补丁集不同（见 §3.2 逐行） |
| 3 | **2826** | ST2 | 2 | ST2 MID1, ST2 MID2 | ✅ 补丁集不同（见 §3.2 逐行） |
| 4 | **7647** | ST2 | 5 | ST2 BOSS1, ST2 BOSS2, ST2 BOSS3, ST2 BOSS4, ST2 BOSS5 | ✅ 补丁集不同（见 §3.2 逐行） |
| 5 | **1858** | ST3 | 2 | ST3 MID2, ST3 MID3 | ✅ 补丁集不同（见 §3.2 逐行） |
| 6 | **3394** | ST3 | 7 | ST3 BOSS1, ST3 BOSS2, ST3 BOSS3, ST3 BOSS4, ST3 BOSS5, ST3 BOSS6, ST3 BOSS7 | ✅ 补丁集不同（见 §3.2 逐行） |
| 7 | **14827** | ST4 | 15 | ST4 BOSS1_LUNASA, ST4 BOSS1_MERLIN, ST4 BOSS1_LYRICA, ST4 BOSS2_LUNASA, ST4 BOSS2_MERLIN, ST4 BOSS2_LYRICA, ST4 BOSS3, ST4 BOSS4_LUNASA, ST4 BOSS5_LUNASA, ST4 BOSS4_MERLIN, ST4 BOSS5_MERLIN, ST4 BOSS4_LYRICA, ST4 BOSS5_LYRICA, ST4 BOSS6, ST4 BOSS7 | 🟡 **BOSS1 组无 ECL 差异**（仅 `thFakeShot`）；BOSS4/5 组可靠 `Sub47` 的 4 个 `pair` 操作数 1/2/3 区分 |
| 8 | **4820** | ST5 | 2 | ST5 MID1, ST5 MID2 | ✅ 补丁集不同（见 §3.2 逐行） |
| 9 | **6114** | ST5 | 6 | ST5 BOSS1, ST5 BOSS2, ST5 BOSS3, ST5 BOSS4, ST5 BOSS5, ST5 BOSS6 | ✅ 补丁集不同（见 §3.2 逐行） |
| 10 | **1914** | ST6 | 2 | ST6 MID1, ST6 MID2 | ✅ 补丁集不同（见 §3.2 逐行） |
| 11 | **2519** | ST6 | 10 | ST6 BOSS1, ST6 BOSS2, ST6 BOSS3, ST6 BOSS4, ST6 BOSS5, ST6 BOSS6, ST6 BOSS7, ST6 BOSS8, ST6 BOSS9, ST6 BOSS10 | ✅ 补丁集不同（见 §3.2 逐行） |
| 12 | **4065** | ST7 | 3 | ST7 MID1, ST7 MID2, ST7 MID3 | ✅ 补丁集不同（见 §3.2 逐行） |
| 13 | **8799** | ST7 | 18 | ST7 END_NS1, ST7 END_S1, ST7 END_NS2, ST7 END_S2, ST7 END_NS3, ST7 END_S3, ST7 END_NS4, ST7 END_S4, ST7 END_NS5, ST7 END_S5, ST7 END_NS6, ST7 END_S6, ST7 END_NS7, ST7 END_S7, ST7 END_NS8, ST7 END_S8, ST7 END_S9, ST7 END_S10 | ✅ 补丁集不同（见 §3.2 逐行） |
| 14 | **3965** | ST8 | 3 | ST8 MID1, ST8 MID2, ST8 MID3 | ✅ 补丁集不同（见 §3.2 逐行） |
| 15 | **9259** | ST8 | 19 | ST8 END_NS1, ST8 END_S1, ST8 END_NS2, ST8 END_S2, ST8 END_NS3, ST8 END_S3, ST8 END_NS4, ST8 END_S4, ST8 END_NS5, ST8 END_S5, ST8 END_NS6, ST8 END_S6, ST8 END_NS7, ST8 END_S7, ST8 END_NS8, ST8 END_S8, ST8 END_S9, ST8 END_S10, ST8 END_S11 | ✅ 补丁集不同（见 §3.2 逐行） |

> 100 行 = 103 boss 行 − **3 行独占时间值**（`ST3 MID1`=821、`ST4 MID1`=7122、`ST6 CHAOS`=1180）；其余 100 行全部落在下表 15 组里。

### 3.2 逐组副键

> 「差异化补丁」＝该行相对**本组基准行**（补丁最少者）多出来的补丁；同一 offset 但**值不同**也算差异（已带值）。

#### T = 2656（ST1，2 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST1 MID1 | `:1163` | —（本组基准行，无额外补丁） | — |
| ST1 MID2 | `:1166` | `ECLSetTime@0x2580(0x3c,0x0)`(L1167) | Sub20:L570 |

#### T = 5043（ST1，4 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST1 BOSS1 | `:1173` | —（本组基准行，无额外补丁） | — |
| ST1 BOSS2 | `:1179` | `ECLSetTime@0x4c20(0x3c,0)`(L1180) | Sub38:L1058 |
| ST1 BOSS3 | `:1184` | `pair@0x4c2c←0x27`(L1185)；`pair@0x4d7c←(int16_t)0`(L1185)；`pair@0x4d8c←(int16_t)0`(L1185) | Sub38:L1058；Sub39:L1083；Sub39:L1084 |
| ST1 BOSS4 | `:1189` | `pair@0x4c2c←0x27`(L1190)；`pair@0x4d7c←(int16_t)0`(L1190)；`pair@0x4d8c←(int16_t)0`(L1190)；`ECLSetTime@0x4f44(0,0)`(L1191) | Sub38:L1058；Sub39:L1083；Sub39:L1084；Sub39:L1110 |

#### T = 2826（ST2，2 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST2 MID1 | `:1194` | —（本组基准行，无额外补丁） | — |
| ST2 MID2 | `:1197` | `ECLSetHealth@0x5444(0,0x833)`(L1198)；`pair@0x5388←0`(L1199)；`pair@0x53a8←0`(L1199)；`pair@0x53c8←0`(L1199) 等 7 项 | Sub40:L1150；Sub40:L1139；Sub40:L1141；Sub40:L1143 |

#### T = 7647（ST2，5 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST2 BOSS1 | `:1206` | —（本组基准行，无额外补丁） | — |
| ST2 BOSS2 | `:1209` | `ECLSetTime@0x6ba8(0x3c,0)`(L1210) | Sub49:L1475 |
| ST2 BOSS3 | `:1213` | `pair@0x6bb4←0x35`(L1214)；`pair@0x80d4←(int16_t)0`(L1214)；`pair@0x80e4←(int16_t)0`(L1214) | Sub49:L1475；Sub53:L1665；Sub53:L1666 |
| ST2 BOSS4 | `:1217` | `pair@0x6bb4←0x35`(L1218)；`pair@0x80d4←(int16_t)0`(L1218)；`pair@0x80e4←(int16_t)0`(L1218)；`ECLSetTime@0x81f4(0,0)`(L1219) | Sub49:L1475；Sub53:L1665；Sub53:L1666；Sub53:L1682 |
| ST2 BOSS5 | `:1222` | `pair@0x6bb4←0x35`(L1223)；`pair@0x80d4←(int16_t)0`(L1223)；`pair@0x80e4←(int16_t)0`(L1223)；`pair@0xc698←(int16_t)0`(L1224) 等 10 项 | Sub49:L1475；Sub53:L1665；Sub53:L1666；Sub68:L2589 |

#### T = 1858（ST3，2 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST3 MID2 | `:1235` | —（本组基准行，无额外补丁） | — |
| ST3 MID3 | `:1239` | `ECLSetHealth@0x2e58(0x3c,0x4af)`(L1240) | Sub22:L663 |

#### T = 3394（ST3，7 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST3 BOSS1 | `:1247` | —（本组基准行，无额外补丁） | — |
| ST3 BOSS2 | `:1251` | `ECLSetTime@0x43d0(0x3c,0)`(L1252) | Sub32:L982 |
| ST3 BOSS3 | `:1256` | `pair@0x43dc←36`(L1257)；`pair@0x4d30←(int16_t)0`(L1257)；`pair@0x4d40←(int16_t)0`(L1257)；`pair@0x4248←(int16_t)0`(L1258) 等 9 项 | Sub32:L982；Sub36:L1087；Sub36:L1088；Sub32:L958 |
| ST3 BOSS4 | `:1264` | `pair@0x42ac←0x2e`(L1265)；`pair@0x42bc←0x30`(L1265)；`pair@0x42dc←0x25`(L1265)；`ECLSetTime@0x43bc(0x3c,0)`(L1266) | Sub32:L963；Sub32:L964；Sub32:L966；Sub32:L980 |
| ST3 BOSS5 | `:1270` | `pair@0x43dc←0x25`(L1271)；`pair@0x4e50←(int16_t)0`(L1271)；`pair@0x4e60←(int16_t)0`(L1271) | Sub32:L982；Sub37:L1110；Sub37:L1111 |
| ST3 BOSS6 | `:1275` | `pair@0x43dc←0x25`(L1276)；`pair@0x4e50←(int16_t)0`(L1276)；`pair@0x4e60←(int16_t)0`(L1276)；`ECLSetTime@0x4fb0(0,0)`(L1277) | Sub32:L982；Sub37:L1110；Sub37:L1111；Sub37:L1130 |
| ST3 BOSS7 | `:1281` | `pair@0x43dc←0x25`(L1282)；`pair@0x4e50←(int16_t)0`(L1282)；`pair@0x4e60←(int16_t)0`(L1282)；`pair@0x9c94←(int16_t)0`(L1283) 等 6 项 | Sub32:L982；Sub37:L1110；Sub37:L1111；Sub56:L2160 |

#### T = 14827（ST4，15 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST4 BOSS1_LUNASA | `:1294` | —（本组基准行，无额外补丁） | — |
| ST4 BOSS1_MERLIN | `:1302` | `thFakeShot=4`(L1303) | — |
| ST4 BOSS1_LYRICA | `:1310` | `thFakeShot=2`(L1311) | — |
| ST4 BOSS2_LUNASA | `:1315` | `ECLSetTime@0x4934(0x3c,0)`(L1317) | Sub44:L1003 |
| ST4 BOSS2_MERLIN | `:1321` | `thFakeShot=4`(L1322)；`ECLSetTime@0x4934(0x3c,0)`(L1323) | —；Sub44:L1003 |
| ST4 BOSS2_LYRICA | `:1327` | `thFakeShot=2`(L1328)；`ECLSetTime@0x4934(0x3c,0)`(L1329) | —；Sub44:L1003 |
| ST4 BOSS3 | `:1333` | `pair@0x4880←0`(L1334)；`pair@0x4adc←0`(L1334) | Sub44:L989；Sub45:L1028 |
| ST4 BOSS4_LUNASA | `:1338` | `ECLCallSub@0x4934→sub47`(L1339)；`pair@0x4f08←1`(L1340)；`pair@0x4f30←1`(L1340)；`pair@0x4f74←1`(L1340) 等 5 项 | sub47(L1339)→Sub44:L1003；Sub47:L1095；Sub47:L1098；Sub47:L1103 |
| ST4 BOSS5_LUNASA | `:1344` | `ECLCallSub@0x4934→sub47`(L1345)；`ECLSetTime@0x51a0(0x20,0)`(L1346)；`pair@0x4f08←1`(L1347)；`pair@0x4f30←1`(L1347) 等 7 项 | sub47(L1345)→Sub44:L1003；Sub47:L1137；Sub47:L1095；Sub47:L1098 |
| ST4 BOSS4_MERLIN | `:1352` | `ECLCallSub@0x4934→sub47`(L1353)；`pair@0x4f08←2`(L1354)；`pair@0x4f30←2`(L1354)；`pair@0x4f74←2`(L1354) 等 5 项 | sub47(L1353)→Sub44:L1003；Sub47:L1095；Sub47:L1098；Sub47:L1103 |
| ST4 BOSS5_MERLIN | `:1358` | `ECLCallSub@0x4934→sub47`(L1359)；`ECLSetTime@0x51a0(0x20,0)`(L1360)；`pair@0x4f08←2`(L1361)；`pair@0x4f30←2`(L1361) 等 7 项 | sub47(L1359)→Sub44:L1003；Sub47:L1137；Sub47:L1095；Sub47:L1098 |
| ST4 BOSS4_LYRICA | `:1366` | `ECLCallSub@0x4934→sub47`(L1367)；`pair@0x4f08←3`(L1368)；`pair@0x4f30←3`(L1368)；`pair@0x4f74←3`(L1368) 等 5 项 | sub47(L1367)→Sub44:L1003；Sub47:L1095；Sub47:L1098；Sub47:L1103 |
| ST4 BOSS5_LYRICA | `:1372` | `ECLCallSub@0x4934→sub47`(L1373)；`ECLSetTime@0x51a0(0x20,0)`(L1374)；`pair@0x4f08←3`(L1375)；`pair@0x4f30←3`(L1375) 等 7 项 | sub47(L1373)→Sub44:L1003；Sub47:L1137；Sub47:L1095；Sub47:L1098 |
| ST4 BOSS6 | `:1380` | `ECLCallSub@0x4934→sub49`(L1381) | sub49(L1381)→Sub44:L1003 |
| ST4 BOSS7 | `:1385` | `ECLCallSub@0x4934→sub49`(L1386)；`ECLForceSpell@0x5488`(L1387) | sub49(L1386)→Sub44:L1003；Sub49:L1185 |

> 组内 lambda：`lunasa()`=L1136-1139（4 个 `pair` ← **1**，`thFakeShot=0`）、`merlin()`=L1140-1143（← **2**，`thFakeShot=4`）、`lyrica()`=L1144-1147（← **3**，`thFakeShot=2`）。**这 4 个 `pair@0x4f08/0x4f30/0x4f74/0x4f9c` 全部落在 `Sub47`（decl L1095/1098/1103/1106）** —— 正是 `BOSS4/5` 用 `call→Sub47` 进入的同一个相位脚本 ⇒ **BOSS4/5 三姐妹在 ECL 侧确有差异（同一批 offset 写 1/2/3），可用 `Sub47` 的这些操作数区分**；而 **BOSS1/2 组没有这个 call，只有 `thFakeShot` 差异**。`timesave()`=L1148-1152（9 个 `pair←0`）落在 `Sub69`/`Sub85`/`Sub106`（如 L2844/L3481/L4053），是「存档点归零」，同样不区分三姐妹。

> **结论（分三档）**：① `BOSS1_{…}` / `BOSS2_{…}` 两组三姐妹在 ECL 侧**完全没有差异**（同一 `ins_9(0)`、无 `call`、`BOSS2` 只有共有的 `ECLSetTime@0x4934`），仅有 `thFakeShot` 0/4/2 ⇒ 只能读**游戏侧机体 / 自机**（`character=0x62F645`、`shotType=0x62F646`）或 boss 对象字段；② `BOSS4_{…}` / `BOSS5_{…}` 两组有 `call→Sub47` + `Sub47` 上 4 个 `pair` 写 1/2/3 ⇒ **可用 `Sub47` 的操作数区分**（但仍需先命中 `Sub47`）；③ `BOSS3`/`BOSS6`/`BOSS7` 各一行，无歧义。（🟡 均未真机）

#### T = 4820（ST5，2 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST5 MID1 | `:1390` | —（本组基准行，无额外补丁） | — |
| ST5 MID2 | `:1395` | `pair@0xb748←(int16_t)0`(L1396)；`ECLSetHealth@0x38b4(0x3c,0x5db)`(L1397) | Timeline0:L2618；Sub38:L811 |

#### T = 6114（ST5，6 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST5 BOSS1 | `:1403` | —（本组基准行，无额外补丁） | — |
| ST5 BOSS2 | `:1406` | `ECLSetTime@0x58f0(0x3c,0)`(L1407) | Sub48:L1250 |
| ST5 BOSS3 | `:1410` | `ECLCallSub@0x58f0→sub52`(L1411)；`pair@0x68dc←(int16_t)0`(L1412)；`pair@0x68ec←(int16_t)0`(L1412) | sub52(L1411)→Sub48:L1250；Sub52:L1462；Sub52:L1463 |
| ST5 BOSS4 | `:1415` | `ECLCallSub@0x58f0→sub52`(L1416)；`pair@0x68dc←(int16_t)0`(L1417)；`pair@0x68ec←(int16_t)0`(L1417)；`ECLSetTime@0x68f8(0,0)`(L1418) | sub52(L1416)→Sub48:L1250；Sub52:L1462；Sub52:L1463；Sub52:L1465 |
| ST5 BOSS5 | `:1421` | `ECLCallSub@0x58f0→sub55`(L1422)；`pair@0x7010←(int16_t)0`(L1423)；`pair@0x7020←(int16_t)0`(L1423)；`pair@0x7110←0`(L1424) | sub55(L1422)→Sub48:L1250；Sub55:L1562；Sub55:L1563；Sub55:L1576 |
| ST5 BOSS6 | `:1430` | `pair@0x5820←0`(L1427)；`pair@0x5840←0`(L1427)；`pair@0x5860←0`(L1427)；`pair@0x5880←0`(L1427) 等 13 项 | Sub48:L1237；Sub48:L1239；Sub48:L1241；Sub48:L1243 |

#### T = 1914（ST6，2 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST6 MID1 | `:1438` | —（本组基准行，无额外补丁） | — |
| ST6 MID2 | `:1443` | `ECLSetHealth@0x17cc(0x3c,0x5db)`(L1445) | Sub19:L355 |

#### T = 2519（ST6，10 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST6 BOSS1 | `:1451` | —（本组基准行，无额外补丁） | — |
| ST6 BOSS2 | `:1458` | `ECLSetTime@0x3324(0x3c,0)`(L1460) | Sub29:L748 |
| ST6 BOSS3 | `:1158` | `yuyuko()`(L1464) | — |
| ST6 BOSS4 | `:1158` | `yuyuko()`(L1468)；`ECLSetTime@0x445c(0,0)`(L1469) | —；Sub34:L976 |
| ST6 BOSS5 | `:1158` | `yuyuko()`(L1473)；`ECLCallSub@0x445c→sub36`(L1474)；`pair@0x47a4←(int16_t)0`(L1475)；`pair@0x47b4←(int16_t)0`(L1475) | —；sub36(L1474)→Sub34:L976；Sub36:L1016；Sub36:L1017 |
| ST6 BOSS6 | `:1158` | `yuyuko()`(L1479)；`ECLCallSub@0x445c→sub36`(L1480)；`pair@0x47a4←(int16_t)0`(L1481)；`pair@0x47b4←(int16_t)0`(L1481) 等 5 项 | —；sub36(L1480)→Sub34:L976；Sub36:L1016；Sub36:L1017 |
| ST6 BOSS7 | `:1486` | `pair@0x5574←(int16_t)0`(L1487)；`pair@0x5584←(int16_t)0`(L1487)；`pair@0x3330←0x26`(L1488) | Sub38:L1174；Sub38:L1175；Sub29:L748 |
| ST6 BOSS8 | `:1492` | `pair@0x5574←(int16_t)0`(L1493)；`pair@0x5584←(int16_t)0`(L1493)；`pair@0x3330←0x26`(L1494)；`ECLSetTime@0x5694(0,0)`(L1495) | Sub38:L1174；Sub38:L1175；Sub29:L748；Sub38:L1189 |
| ST6 BOSS9 | `:1503` | `pair@0x3240←0`(L1499)；`pair@0x3260←0`(L1499)；`pair@0x3280←0`(L1499)；`pair@0x32a0←0`(L1499) 等 13 项 | Sub29:L734；Sub29:L736；Sub29:L738；Sub29:L740 |
| ST6 BOSS10 | `:1508` | `th07_rb.Enable()`(L1507)；`pair@0x30dc←0x3e`(L1509)；`pair@0xb9f4←(int16_t)0`(L1510)；`pair@0xbab0←0`(L1510) 等 5 项 | —；Sub28:L708；Sub62:L2406；Sub62:L2418 |

> 组内 lambda：`yuyuko()`=L1153-1159（`pair@0x425c/0x426c/0xe678/0x3330/0x43a8/0x43f0/0x43fc/0x4410/0x4420/0x4434/0x4444` + `ECLTimeWarp(2,0x9d7)`）；其 `pair@0xe678←0x9d7` 落在 `Timeline0:L3056`（即 boss 触发指令自身），其余落在 `Sub34` 以后。

#### T = 4065（ST7，3 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST7 MID1 | `:1513` | —（本组基准行，无额外补丁） | — |
| ST7 MID2 | `:1518` | `ECLSetTime@0x3a70(0x3c,0)`(L1520) | Sub55:L948 |
| ST7 MID3 | `:1523` | `pair@0x3900←(int16_t)0`(L1525)；`pair@0x3a7c←0x3d`(L1525)；`pair@0x39a0←0`(L1526)；`pair@0x39c0←0`(L1526) 等 10 项 | Sub55:L926；Sub55:L948；Sub55:L935；Sub55:L937 |

#### T = 8799（ST7，18 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST7 END_NS1 | `:1534` | —（本组基准行，无额外补丁） | — |
| ST7 END_S1 | `:1539` | `ECLSetTime@0x4ce0(0x3c,0)`(L1540) | Sub68:L1256 |
| ST7 END_NS2 | `:1544` | `pair@0x4df8←(int16_t)0`(L1545)；`pair@0x4e08←(int16_t)0`(L1545)；`pair@0x4e18←(int16_t)0`(L1545)；`ECLCallSub@0x4ce0→sub70`(L1546) | Sub70:L1277；Sub70:L1278；Sub70:L1279；sub70(L1546)→Sub68:L1256 |
| ST7 END_S2 | `:1550` | `pair@0x4df8←(int16_t)0`(L1551)；`pair@0x4e08←(int16_t)0`(L1551)；`pair@0x4e18←(int16_t)0`(L1551)；`ECLCallSub@0x4ce0→sub70`(L1552) 等 5 项 | Sub70:L1277；Sub70:L1278；Sub70:L1279；sub70(L1552)→Sub68:L1256 |
| ST7 END_NS3 | `:1557` | `pair@0x4fd4←(int16_t)0`(L1558)；`pair@0x4fe4←(int16_t)0`(L1558)；`ECLCallSub@0x4ce0→sub71`(L1559) | Sub71:L1309；Sub71:L1310；sub71(L1559)→Sub68:L1256 |
| ST7 END_S3 | `:1563` | `pair@0x4fd4←(int16_t)0`(L1564)；`pair@0x4fe4←(int16_t)0`(L1564)；`ECLCallSub@0x4ce0→sub71`(L1565)；`ECLSetTime@0x510c(0,0)`(L1566) | Sub71:L1309；Sub71:L1310；sub71(L1565)→Sub68:L1256；Sub71:L1327 |
| ST7 END_NS4 | `:1570` | `pair@0x5210←(int16_t)0`(L1571)；`pair@0x5220←(int16_t)0`(L1571)；`pair@0x5230←(int16_t)0`(L1571)；`ECLCallSub@0x4ce0→sub73`(L1572) | Sub73:L1347；Sub73:L1348；Sub73:L1349；sub73(L1572)→Sub68:L1256 |
| ST7 END_S4 | `:1576` | `pair@0x5210←(int16_t)0`(L1577)；`pair@0x5220←(int16_t)0`(L1577)；`pair@0x5230←(int16_t)0`(L1577)；`ECLCallSub@0x4ce0→sub73`(L1578) 等 5 项 | Sub73:L1347；Sub73:L1348；Sub73:L1349；sub73(L1578)→Sub68:L1256 |
| ST7 END_NS5 | `:1583` | `pair@0x5430←(int16_t)0`(L1584)；`pair@0x5440←(int16_t)0`(L1584)；`ECLCallSub@0x4ce0→sub75`(L1585) | Sub75:L1385；Sub75:L1386；sub75(L1585)→Sub68:L1256 |
| ST7 END_S5 | `:1589` | `pair@0x5430←(int16_t)0`(L1590)；`pair@0x5440←(int16_t)0`(L1590)；`ECLCallSub@0x4ce0→sub75`(L1591)；`ECLSetTime@0x5590(0,0)`(L1592) | Sub75:L1385；Sub75:L1386；sub75(L1591)→Sub68:L1256；Sub75:L1405 |
| ST7 END_NS6 | `:1596` | `pair@0x56e4←(int16_t)0`(L1597)；`pair@0x56f4←(int16_t)0`(L1597)；`pair@0x5704←(int16_t)0`(L1597)；`ECLCallSub@0x4ce0→sub77`(L1598) | Sub77:L1429；Sub77:L1430；Sub77:L1431；sub77(L1598)→Sub68:L1256 |
| ST7 END_S6 | `:1602` | `pair@0x56e4←(int16_t)0`(L1603)；`pair@0x56f4←(int16_t)0`(L1603)；`pair@0x5704←(int16_t)0`(L1603)；`ECLCallSub@0x4ce0→sub77`(L1604) 等 5 项 | Sub77:L1429；Sub77:L1430；Sub77:L1431；sub77(L1604)→Sub68:L1256 |
| ST7 END_NS7 | `:1609` | `pair@0x5964←(int16_t)0`(L1610)；`pair@0x5974←(int16_t)0`(L1610)；`ECLCallSub@0x4ce0→sub79`(L1611) | Sub79:L1472；Sub79:L1473；sub79(L1611)→Sub68:L1256 |
| ST7 END_S7 | `:1615` | `pair@0x5964←(int16_t)0`(L1616)；`pair@0x5974←(int16_t)0`(L1616)；`ECLCallSub@0x4ce0→sub79`(L1617)；`ECLSetTime@0x5ac4(0,0)`(L1618) | Sub79:L1472；Sub79:L1473；sub79(L1617)→Sub68:L1256；Sub79:L1492 |
| ST7 END_NS8 | `:1622` | `pair@0x5c28←(int16_t)0`(L1623)；`pair@0x5c38←(int16_t)0`(L1623)；`pair@0x5c48←(int16_t)0`(L1623)；`ECLCallSub@0x4ce0→sub81`(L1624) | Sub81:L1517；Sub81:L1518；Sub81:L1519；sub81(L1624)→Sub68:L1256 |
| ST7 END_S8 | `:1628` | `pair@0x5c28←(int16_t)0`(L1629)；`pair@0x5c38←(int16_t)0`(L1629)；`pair@0x5c48←(int16_t)0`(L1629)；`ECLCallSub@0x4ce0→sub81`(L1630) 等 5 项 | Sub81:L1517；Sub81:L1518；Sub81:L1519；sub81(L1630)→Sub68:L1256 |
| ST7 END_S9 | `:1635` | `pair@0x4b5c←(int16_t)0`(L1636)；`pair@0x5ebc←(int16_t)0`(L1636)；`pair@0x5ecc←(int16_t)0`(L1636)；`pair@0x5fdc←0`(L1637) 等 6 项 | Sub68:L1234；Sub83:L1562；Sub83:L1563；Sub83:L1579 |
| ST7 END_S10 | `:1651` | `pair@0x4b5c←0`(L1652)；`pair@0x6030←0`(L1653)；`pair@0x6040←0`(L1654)；`pair@0x614c←0`(L1655) 等 6 项 | Sub68:L1234；Sub84:L1587；Sub84:L1588；Sub84:L1604 |

#### T = 3965（ST8，3 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST8 MID1 | `:1685` | —（本组基准行，无额外补丁） | — |
| ST8 MID2 | `:1690` | `ECLSetTime@0x4350(0x3c,0)`(L1692) | Sub57:L1068 |
| ST8 MID3 | `:1695` | `ECLCallSub@0x4350→sub63`(L1696)；`pair@0x41e0←(int16_t)0`(L1698)；`pair@0x4280←0`(L1699)；`pair@0x42a0←0`(L1699) 等 9 项 | sub63(L1696)→Sub57:L1068；Sub57:L1046；Sub57:L1055；Sub57:L1057 |

#### T = 9259（ST8，19 行）

| 记录名 | 源码行 | 差异化补丁（源码行） | decl 侧落点 |
| --- | --- | --- | --- |
| ST8 END_NS1 | `:1707` | —（本组基准行，无额外补丁） | — |
| ST8 END_S1 | `:1712` | `ECLSetTime@0x55ec(0x3c,0)`(L1713) | Sub70:L1377 |
| ST8 END_NS2 | `:1717` | `pair@0x5704←(int16_t)0`(L1718)；`pair@0x5714←(int16_t)0`(L1718)；`pair@0x5724←(int16_t)0`(L1718)；`ECLCallSub@0x55ec→sub72`(L1719) | Sub72:L1398；Sub72:L1399；Sub72:L1400；sub72(L1719)→Sub70:L1377 |
| ST8 END_S2 | `:1723` | `pair@0x5704←(int16_t)0`(L1724)；`pair@0x5714←(int16_t)0`(L1724)；`pair@0x5724←(int16_t)0`(L1724)；`ECLCallSub@0x55ec→sub72`(L1725) 等 5 项 | Sub72:L1398；Sub72:L1399；Sub72:L1400；sub72(L1725)→Sub70:L1377 |
| ST8 END_NS3 | `:1730` | `pair@0x58e0←(int16_t)0`(L1731)；`pair@0x58f0←(int16_t)0`(L1731)；`ECLCallSub@0x55ec→sub73`(L1732) | Sub73:L1430；Sub73:L1431；sub73(L1732)→Sub70:L1377 |
| ST8 END_S3 | `:1736` | `pair@0x58e0←(int16_t)0`(L1737)；`pair@0x58f0←(int16_t)0`(L1737)；`ECLCallSub@0x55ec→sub73`(L1738)；`ECLSetTime@0x5a18(0,0)`(L1739) | Sub73:L1430；Sub73:L1431；sub73(L1738)→Sub70:L1377；Sub73:L1448 |
| ST8 END_NS4 | `:1743` | `pair@0x5b1c←(int16_t)0`(L1744)；`pair@0x5b2c←(int16_t)0`(L1744)；`pair@0x5b3c←(int16_t)0`(L1744)；`ECLCallSub@0x55ec→sub75`(L1745) | Sub75:L1468；Sub75:L1469；Sub75:L1470；sub75(L1745)→Sub70:L1377 |
| ST8 END_S4 | `:1749` | `pair@0x5b1c←(int16_t)0`(L1750)；`pair@0x5b2c←(int16_t)0`(L1750)；`pair@0x5b3c←(int16_t)0`(L1750)；`ECLCallSub@0x55ec→sub75`(L1751) 等 5 项 | Sub75:L1468；Sub75:L1469；Sub75:L1470；sub75(L1751)→Sub70:L1377 |
| ST8 END_NS5 | `:1756` | `pair@0x5d3c←(int16_t)0`(L1757)；`pair@0x5d4c←(int16_t)0`(L1757)；`ECLCallSub@0x55ec→sub77`(L1758) | Sub77:L1506；Sub77:L1507；sub77(L1758)→Sub70:L1377 |
| ST8 END_S5 | `:1762` | `pair@0x5d3c←(int16_t)0`(L1763)；`pair@0x5d4c←(int16_t)0`(L1763)；`ECLCallSub@0x55ec→sub77`(L1764)；`ECLSetTime@0x5e9c(0,0)`(L1765) | Sub77:L1506；Sub77:L1507；sub77(L1764)→Sub70:L1377；Sub77:L1526 |
| ST8 END_NS6 | `:1769` | `pair@0x5ff0←(int16_t)0`(L1770)；`pair@0x6000←(int16_t)0`(L1770)；`pair@0x6010←(int16_t)0`(L1770)；`ECLCallSub@0x55ec→sub79`(L1771) | Sub79:L1550；Sub79:L1551；Sub79:L1552；sub79(L1771)→Sub70:L1377 |
| ST8 END_S6 | `:1775` | `pair@0x5ff0←(int16_t)0`(L1776)；`pair@0x6000←(int16_t)0`(L1776)；`pair@0x6010←(int16_t)0`(L1776)；`ECLCallSub@0x55ec→sub79`(L1777) 等 5 项 | Sub79:L1550；Sub79:L1551；Sub79:L1552；sub79(L1777)→Sub70:L1377 |
| ST8 END_NS7 | `:1782` | `pair@0x6270←(int16_t)0`(L1783)；`pair@0x6280←(int16_t)0`(L1783)；`ECLCallSub@0x55ec→sub81`(L1784) | Sub81:L1593；Sub81:L1594；sub81(L1784)→Sub70:L1377 |
| ST8 END_S7 | `:1788` | `pair@0x6270←(int16_t)0`(L1789)；`pair@0x6280←(int16_t)0`(L1789)；`ECLCallSub@0x55ec→sub81`(L1790)；`ECLSetTime@0x63d0(0,0)`(L1791) | Sub81:L1593；Sub81:L1594；sub81(L1790)→Sub70:L1377；Sub81:L1613 |
| ST8 END_NS8 | `:1795` | `pair@0x6534←(int16_t)0`(L1796)；`pair@0x6544←(int16_t)0`(L1796)；`pair@0x6554←(int16_t)0`(L1796)；`ECLCallSub@0x55ec→sub83`(L1797) | Sub83:L1638；Sub83:L1639；Sub83:L1640；sub83(L1797)→Sub70:L1377 |
| ST8 END_S8 | `:1801` | `pair@0x6534←(int16_t)0`(L1802)；`pair@0x6544←(int16_t)0`(L1802)；`pair@0x6554←(int16_t)0`(L1802)；`ECLCallSub@0x55ec→sub83`(L1803) 等 5 项 | Sub83:L1638；Sub83:L1639；Sub83:L1640；sub83(L1803)→Sub70:L1377 |
| ST8 END_S9 | `:1808` | `pair@0x5468←(int16_t)0`(L1809)；`pair@0x67d0←(int16_t)0`(L1809)；`pair@0x67e0←(int16_t)0`(L1809)；`pair@0x68e8←0`(L1810) 等 6 项 | Sub70:L1355；Sub85:L1683；Sub85:L1684；Sub85:L1700 |
| ST8 END_S10 | `:1815` | `pair@0x5468←(int16_t)0`(L1816)；`pair@0x693c←(int16_t)0`(L1816)；`pair@0x694c←(int16_t)0`(L1816)；`pair@0x695c←(int16_t)0`(L1816) 等 8 项 | Sub70:L1355；Sub86:L1708；Sub86:L1709；Sub86:L1710 |
| ST8 END_S11 | `:1826` | `pair@0x5468←(int16_t)0`(L1827)；`pair@0x6abc←(int16_t)0`(L1827)；`pair@0x6acc←(int16_t)0`(L1827)；`pair@0x6bd8←0`(L1828) 等 6 项 | Sub70:L1355；Sub87:L1734；Sub87:L1735；Sub87:L1751 |

### 3.3 跨类型同值：`ST6 #2`（章节）＝ `ST6 CHAOS`（boss 轴）＝ 1180

- `ST6 #2`：`thprac_th07.cpp:1069`（`THStageWarp` stage6 portion2，`ECLTimeWarp(2, 1180)`）。
- `ST6 CHAOS`：`thprac_th07.cpp:1435`（`THPatch` `TH07_ST6_CHAOS`，`ECLTimeWarp(2, 0x49c)`）。
- decl：两者在 `ecldata6.decl` → `Timeline0` 的**同一条指令**（`t=1180`，`ins_0(...)`）上；`ST6 CHAOS` 无任何额外补丁 ⇒ **时间 + ECL 都无法区分**。
- ⇒ **已裁决（2026-10-22，用户）**：二者是同一 ECL 点，**保留 `ST6 CHAOS`、删除 `ST6 #2`**（thprac 侧 CHAOS 本就是「道中小妖精」，与章节 #2 同点）。CSV 因此为 **140 行＝章节 37 + boss 103**；`verify_th07_entries.py` 的期望表已同步 `want.pop('ST6 #2')`。

---

## 4. 留空行与原因

**无。** 140/140 全部有源码级来源，无一行留空。

为免误解，另登记 **3 行「无精确 decl 时间戳」**（**不是留空**，值仍来自源码，只是 ECL 里没有时间戳恰为该值的指令，属 `ECLTimeWarp` 的正常语义）：

| 记录名 | 时间值 | 源码行 | decl 侧落点 |
| --- | --- | --- | --- |
| ST1 MID1 | 2656 | `:1163` | Timeline0:L2156（无精确时间戳；落点 2667，+11，`ins_0(20, 0.0f, 0.0f, 0.0f, 10000, 0, -2, 100000)`） |
| ST1 MID2 | 2656 | `:1166` | Timeline0:L2156（无精确时间戳；落点 2667，+11，`ins_0(20, 0.0f, 0.0f, 0.0f, 10000, 0, -2, 100000)`） |
| ST3 MID1 | 821 | `:1231` | Timeline0:L2417（无精确时间戳；落点 853，+32，`ins_0(13, 80.0f, -16.0f, 0.0f, 17000, 0, -2, 30000)`） |

> 其余 100 个 boss 行均有精确命中（`🟡A`）。

---

## 5. 与 `th07_ecl_re_report.md` §11 的差异

> **本节只登记，不修改 `th07_ecl_re_report.md`**（按任务约定留给主代理收口）。

| # | 位置 | §11 的写法 | 源码 / ECL 事实 | 证据 |
| --- | --- | --- | --- | --- |
| 1 | §11.1 表头 | `后半 #5 \| 后半 #6 \| 后半 #7 \| 后半 #8` | 后半应从 **#1** 重新编号（`ST4` 为 `{4,4}`，后半就是 #1..#4） | `thprac_th07.cpp:411-420` + 本报告 §1.2 |
| 2 | §11.1 列位 | `st1` 的 `3107` 放在「前半 #3」、`st2` 的 `3366` 放在「前半 #2」、`st3` 的 `1805` 放在「前半 #3」、`st5` 的 `4883` 放在「前半 #4」、`st6` 的 `1180` 放在「前半 #2」 | 它们分别是 **`ST1 后半 #1` / `ST2 后半 #1` / `ST3 后半 #1` / `ST5 后半 #1` / `ST6 #2`**（`mChapterSetup` 的 前半/后半 切分） | 同上 |
| 3 | §11.1 列位 | `st7`/`st8` 只有 4 个「前半」列，`3305`/`3191` 被迫落在「后半 #5」列 | 两关都是 `{5,3}`，**前 5 个才是前半**；3305 = `ST7 前半 #5`、3191 = `ST8 前半 #5` | 同上 |
| 4 | §11.2 粒度 | 分组行（`ST4_BOSS1_{…}`、`ST6 BOSS7/8`、`ST7 MID1/2/3`、`ST7 END_S1..S10`、`ST8 …`） | **计数 103 行需逐行登记**；本报告 §2 给出 103 行逐行表，且 100 行有 decl 精确命中 | 本报告 §2 / §3 |
| 5 | §11.2 `ST7_END_S1..S10` 注 | `ECLCallSub(sub 0x46..0x54)` | 实际 `sub id` = `0x46,0x47,0x49,0x4b,0x4d,0x4f,0x51,0x53,0x54`（**跳号**，`0x48/0x4a/…` 未用）；且 `S1` 不是 `ECLCallSub` 而是 `ECLSetTime(0x4ce0)` | `thprac_th07.cpp:1540`、`:1546/1552/1559/1565/1572/1578/1585/1591/1598/1604/1611/1617/1624/1630/1638` |
| 6 | §11.2 `ST8_END_S1..S11` 注 | `ECLCallSub(sub 0x48..0x57)` | 实际 `0x48,0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x56,0x57`（跳号）；`S1` 为 `ECLSetTime` | `thprac_th07.cpp:1713`、`:1719…1829` |
| 7 | §11.2 `ST1_MID1` 注 | 「`ECLST3? 无`」 | 该 case 只有 `ECLTimeWarp(2, 0xa60)`，**无 `ECLST3BG()` 调用**；伪注易误导 | `thprac_th07.cpp:1162-1164` |
| 8 | §0 / §7.2 / §12 素材段 | 「无 `.ecl`/`.decl`」「本机无 eclmap、`.ecl` 未解出」 | **2026-10-21 起 8/8 `.ecl` + `.decl` 已在本机**，本轮据此完成交叉验证 | `docs/logger/decompile_materials.md`（A 代段待更新）+ 本报告 §1.4 |

> **结论：§11 表里的数值与源码逐一相符** —— §11.1 的 38 个道中值、§11.2 覆盖的 103 个 boss 值（含 `{…}` / `A/B` / `S1..Sn` 分组行展开后）**未发现任何数值或 `count` 抄错 / 漏项**；差异集中在 §11.1 的**列名与列位**、§11.2 的**分组粒度**与两条 `sub id` 区间注。

---

## 6. 自检

脚本（仓库根执行；只读 CSV + 源码，不联网、不编译；同一份亦存于 `docs/logger/tools/verify_th07_entries.py`）：

```python
import csv, re, sys
from collections import Counter
SRC = 'thprac/src/thprac/thprac_th07.cpp'
CSV = 'docs/logger/th07_checkpoints.csv'

rows = list(csv.reader(open(CSV, encoding='utf-8')))
hdr, data = rows[0], rows[1:]
print(f'表头            : {hdr}')
print(f'数据行数        : {len(data)}  (期望 140)')
print(f'字段数全为 4    : {all(len(r) == 4 for r in data)}')
nonempty = [r for r in data if r[0].strip()]
print(f'第 1 列非空     : {len(nonempty)}/{len(data)}')
print(f'全为纯十进制数字: {all(re.fullmatch(r"[0-9]+", r[0]) for r in nonempty)}')
print(f'含 0x / 空格    : {any("0x" in r[0] or " " in r[0] for r in nonempty)}')
CHAP = re.compile(r'^ST\d+ (#\d+|前半 #\d+|后半 #\d+)$')
chap = [r for r in data if CHAP.match(r[3])]
boss = [r for r in data if not CHAP.match(r[3])]
print(f'章节行 / boss 行: {len(chap)} / {len(boss)}  (期望 37 / 103)')
print('逐关行数        :', dict(sorted(Counter(r[2] for r in data).items())))

# ---- 从源码重算期望值（章节 = THStageWarp，boss = THPatch；dlg 取非 dlg；yuyuko() 取 0x9d7）
src = open(SRC, encoding='utf-8').read().split('\n')
MCS = {1: (2, 1), 2: (1, 1), 3: (2, 1), 4: (4, 4), 5: (3, 1), 6: (2, 0), 7: (5, 3), 8: (5, 3)}
want, stage, case = {}, None, None
for i, l in enumerate(src, 1):
    if i < 975 or i > 1834:
        continue
    m = re.search(r'if \(stage == (\d+)\)', l)
    if m:
        stage = int(m.group(1))
    m = re.match(r'\s*case (\d+):', l)
    if m:
        case = int(m.group(1))
    m = re.search(r'ECLTimeWarp\((\d+),\s*(0x[0-9a-fA-F]+|\d+)\)', l)
    if not m or i > 1134:          # THPatch 段单独处理
        continue
    a, b = MCS[stage]
    nm = (f'ST{stage} #{case}' if b == 0 else
          f'ST{stage} 前半 #{case}' if case <= a else f'ST{stage} 后半 #{case-a}')
    want[nm] = [int(m.group(2), 0)]
cur = None
for i, l in enumerate(src, 1):
    if i < 1134 or i > 1834:
        continue
    m = re.match(r'\s*case THPrac::TH07::(TH07_\w+):', l)
    if m:
        cur = m.group(1)
        want[cur[5:].replace('_', ' ', 1)] = []
        continue
    m = re.search(r'ECLTimeWarp\((\d+),\s*(0x[0-9a-fA-F]+|\d+)\)', l)
    if m and cur:
        want[cur[5:].replace('_', ' ', 1)].append(int(m.group(2), 0))
for k, v in want.items():          # 双值取第 2 个（非 dlg）；无值 = yuyuko() 行
    want[k] = [v[1] if len(v) == 2 else (v[0] if v else 0x9d7)]
want.pop('ST6 #2', None)   # 2026-10-22 裁决：与 ST6 CHAOS 同一 ECL 点，保留 CHAOS
bad = [(r[3], r[0], want.get(r[3])) for r in data if want.get(r[3]) != [int(r[0])]]
print(f'与源码逐行一致  : {not bad}' + ('' if not bad else f'  差异 {bad}'))
sys.exit(1 if bad else 0)
```

输出：

```text
表头            : ['时间轴位置', '所属游戏', '所属关卡', '记录名']
数据行数        : 140  (期望 140)
字段数全为 4    : True
第 1 列非空     : 140/140
全为纯十进制数字: True
含 0x / 空格    : False
章节行 / boss 行: 37 / 103  (期望 37 / 103)
逐关行数        : {'ST1': 9, 'ST2': 9, 'ST3': 13, 'ST4': 24, 'ST5': 12, 'ST6': 14, 'ST7': 29, 'ST8': 30}
与源码逐行一致  : True
```

**补充自检（人工 / 脚本各一次）**

- [x] 数据行数 140 ＝ 章节 37 + boss 103（`ST6 #2` 已按裁决删除）；行序与骨架完全一致，且**只改第 1 列**（与 `HEAD` 版逐行比对，第 2–4 列完全相同）。
- [x] 其余 3 列**一字未改**（`[r[1:] for r in old] == [r[1:] for r in new]`）、表头 `时间轴位置` 未改、UTF-8 无 BOM、LF 换行、结尾保留换行。
- [x] 每行恰 1 个值；无非空空白、无 `0x`、无单位、无多余字符（`^[0-9]+$` 全通过）。
- [x] 值与 `THStageWarp`/`THPatch` **一一对应**（§6 脚本从源码重算并逐行比对，`True`）。
- [x] 103 个 boss 记录名 ↔ 103 个 `THPatch` case 无缺无多；37 个章节行 ↔ `mChapterSetup` 之和逐关相等（ST6 由 2 变 1）。
- [x] 10 行 dlg 双值全部取非 dlg，且 ECL 侧 `ins_8(0)`（T−1）/`ins_9(0)`（T）**10/10** 命中。
- [x] decl 对齐：762 sub + 17 timeline、**22413 条实指令**，opcode 逐条与 `ins_N` 相等（另 16 条 `-1` 终止符）。

---

## 7. 监控两级判定流程（E5 实现口径，🟡 未真机）

> 本节回答一个实现层必答题：**CSV 第 1 列的时间值在 boss 段大量重复，监控凭什么分辨「现在打到第几个相位」？**
> 结论：**单靠这一列做不到，必须两级判定**。一级＝时间轴（确认进了哪个 boss 容器），二级＝boss 脚本的**指令位置**（确认相位）。
> 本节所有地址/位移沿用 `docs/th07/th07_ecl_re_report.md`；结论仍为 **🟡 静态定案、未真机**。

### 7.1 为什么一级键必然重复（数据事实）

- CSV **140 行只有 53 个不同时间值**；**17 组重复值覆盖 104 行**。
- 最极端：时间 `9259`（`ST8 END_*` 系）**19 行**、`8799`（`ST7 END_*`）**18 行**、`14827`（`ST4` 三姐妹系）**15 行**。
- 根因：`THPatch` 的 `ECLTimeWarp(count,time)` 只把**时间轴**打到「该 boss 容器的入口」一次；区分 BOSS1/2/3/4 的是**就地改写 boss 脚本字节**（`ECLSetTime` / `ECLCallSub` / `pair{off,val}`），**这些补丁不动时间轴**。
- ⇒ 同一 boss 的所有相位共享同一时间值；`latch`（最大入口 ≤ t）只能给出「进入了这个 boss」，给不出「第几相位」。

**示例（ST1，`thprac_th07.cpp:1175-1190`）**：

| 记录名 | time | 与上一行相同，靠什么区分 |
| --- | --- | --- |
| ST1 BOSS1 | 5043 | 基准相位（无额外补丁，仅 `ECLNameFix`） |
| ST1 BOSS2 | 5043 | `ECLSetTime(0x4c20,0x3c,0)` ⇒ 相位边界在 **buf+0x4c20** |
| ST1 BOSS3 | 5043 | `pair{0x4c2c,0x27} …` ⇒ 边界 **buf+0x4c2c** |
| ST1 BOSS4 | 5043 | `pair{…} + ECLSetTime(0x4f44,0,0)` ⇒ 边界 **buf+0x4f44** |

> 这些 `0x4c20/0x4c2c/0x4f44` **就是 boss 脚本里的指令文件偏移**（相对 `buf`），也是相位分界。§3 的逐行「补丁落点」表已给全 15 组。

### 7.2 两级模型

```text
每帧：
  ① 一级：读时间轴 → 定位「当前是哪个 boss 家族 / 哪个章节段」（白名单 = CSV 第 1 列）
  ② 二级：读 boss 对象的当前指令指针 → 落在哪个「相位区间」→ 具体 STn BOSSx
  ③ 合并：一级给出的记录名 + 二级给出的相位 → 最终段落名；任一级无命中 ⇒ no-op（保持上一个值，不回退 main）
```

### 7.3 一级：时间轴（容器判定）

```cpp
const uint32_t mgr      = 0x9A9B00;                        // ENEMY_MANAGER（静态，无需捕获）
const uint32_t eclBuf   = *(uint32_t*)0x1347938;           // buf = ECL 文件缓冲基址
const uint16_t tlCount  = *(uint16_t*)(eclBuf + 2);         // 活动 timeline 数（本关）
int32_t  tlTime = *(int32_t*)(mgr + 0x9545F4 + 0x8);        // = *(int32_t*)0x12FE0FC（TL_0+8）
uint32_t tlIp   = *(uint32_t*)(mgr + 0x9545F4 + 0xC);       // = *(uint32_t*)0x12FE100（TL_0+0xC）
```

- 用 `tlTime` 在本关白名单里做「最大入口 ≤ t」→ 得到**候选记录名**。
- 命中多个同值时（本作 17 组），**不要在这里二选一**，进入二级。
- `tlCount` 可用于 sanity：`0` ⇒ 未进关/已结束，直接 no-op。

### 7.4 二级：boss 脚本位置（相位判定）

**取 boss 对象**（`mgr+0x954598` 起的 8 槽，空槽 = 0）：

```cpp
uint32_t boss = 0;
for (int k = 0; k < 8; ++k) {
    uint32_t e = *(uint32_t*)(mgr + 0x954598 + k * 4);
    if (e) { boss = e; break; }          // ⚠ 中boss + 关底可能同时在槽：需按 ip 落在哪个 boss sub 区间来挑，见 §7.5
}
uint32_t bossIp   = *(uint32_t*)(boss + 0x6E4);   // 当前 sub 指令指针（绝对地址）
int32_t  bossTime = *(int32_t*)(boss + 0x6F0);    // 敌方 VM 当前时间（备用键）
int32_t  depth    = *(int32_t*)(boss + 0x2A7C);   // 调用深度（0..0xF）
```

**相位区间规则（推荐，数据已具备）**：

1. 每个 boss 家族有一组**补丁偏移** `off[]`（§3 表里的 `pair@0x…` / `ECLSetTime@0x…`），按 `off` **升序**排序。
2. `ipOff = bossIp - eclBuf`；取**最后一个 `off[i] <= ipOff`** 的相位；`ipOff` 小于最小边界 ⇒ 该家族的第一个相位。
3. **只用 `ip` 位置，不读补丁内容** —— 正常游玩时 thprac 根本没打补丁，读字节会读到原始值。
4. 同一家族内**相位索引单调不减**（latch）：允许跳级（一键打到后期相位），**不允许回退**；仅在「换 boss 家族 / 换关 / 新局 / `bossIp` 出界」时复位。

**ST1 的具体区间**（`Sub38` 内）：

| `ipOff` 落点 | 相位 |
| --- | --- |
| `< 0x4c20`（含脚本开头） | `ST1 BOSS1` |
| `[0x4c20, 0x4c2c)` | `ST1 BOSS2` |
| `[0x4c2c, 0x4f44)` | `ST1 BOSS3` |
| `≥ 0x4f44` | `ST1 BOSS4` |

> ⚠️ 上表的 4 个边界取自 §3 的补丁偏移，**未经真机确认「相位确实在这些偏移处切换」**。若真机发现边界错位，只需改边界表，不动 CSV。

### 7.5 边界与已知不可解

1. **中boss 与关底同时存活**：`mgr+0x954598` 可能有多个非空槽。做法：对每个非空槽算 `ipOff`，取「落在某个 **boss 家族**区间内」的那个；都落在区间外 ⇒ 视为无 boss、回退到一级结果。
2. **boss 脚本发生 `call sub`（`depth > 0`）**：此时 `bossIp` 在**被调 sub** 里，不在 boss 主脚本区间内 ⇒ 区间法会 miss。缓解：`depth > 0` 时沿调用栈 `boss+0x8FC + level*0x218` 找到**最外层（level 0）**的游标再用；找不到就 no-op。（🟡 栈布局见逆向报告 §4。）
3. **同偏移的相邻相位**：若两个相位的补丁落在**同一偏移**，区间法会合并它们 ⇒ 退化为**按 boss 脚本时间**（`*(int32_t*)(boss+0x6F0)`，敌方 VM 当前时间）切区间；仍不可分则按**先命中**处理并记日志。（不要读「补丁写进去的字节」判相位：正常游玩时 thprac 没打补丁，读到的是原始值。）
4. **ST4 三姐妹 `BOSS1_*` / `BOSS2_*`（6 行）**：ECL 侧完全无差异（无 `call`、补丁相同），**区间法也分不开**。按 2026-10-22 裁决：**属游戏特性、不算 bug**，logger 层可另读游戏侧自机/机体状态（`character=0x62F645`、`shotType=0x62F646`）；在此之前这 6 行会互相误命中。
5. **`ST6 CHAOS`**：CSV 已删 `ST6 #2`，只剩 `ST6 CHAOS`（1180），一级唯一，无需二级。
6. **dlg 分支**：同级时间可能差 1（10 行双值，CSV 取大值）⇒ 一级匹配用 `|t - entry| <= 1` 或「取最大入口 ≤ t + 1」。
7. **关卡/新局/重开**：`tlCount == 0` 或 `STAGE_NUM` 变化 ⇒ 清空当前段落与所有相位状态。

### 7.6 伪代码（可直接翻成 C++）

```cpp
struct Phase { const char* name; uint32_t lo; };   // lo = buf 内偏移下界（升序）
// 由 §3 表机器生成，例：ST1 = {{"ST1 BOSS1",0},{"ST1 BOSS2",0x4c20},{"ST1 BOSS3",0x4c2c},{"ST1 BOSS4",0x4f44}}

const char* ResolveBossPhase(const Phase* ph, int n, uint32_t ipOff, int& lastIdx) {
    if (ipOff == 0) return nullptr;                  // ip 无效
    int idx = (lastIdx >= 0) ? lastIdx : 0;          // 单调：从上次相位起找
    for (int i = n - 1; i >= idx; --i)               // 跳过已过的相位（不允许回退）
        if (ipOff >= ph[i].lo) { lastIdx = i; return ph[i].name; }
    return nullptr;                                   // 未到第一个边界 ⇒ 保持上一值
}

void Tick() {
    const uint32_t mgr = 0x9A9B00, buf = *(uint32_t*)0x1347938;
    if (*(uint16_t*)(buf + 2) == 0) { Reset(); return; }         // 未进关
    int32_t t = *(int32_t*)(mgr + 0x9545F4 + 0x8);
    const char* chap = MatchTimelineLatch(t);                    // 一级：CSV 白名单
    uint32_t boss = PickBossSlot(mgr, buf);                      // §7.5-1
    if (boss) {
        uint32_t bip = *(uint32_t*)(boss + 0x6E4);
        if (bip <= buf) { /* 游标无效：本帧只用一级 */ }
        uint32_t ipOff = bip - buf;                              // 需保证 bip > buf
        if (ipOff > 0x40000) ipOff = 0;                          // 越界保护（ECL 缓冲远小于 256KB）
        int32_t  depth = *(int32_t*)(boss + 0x2A7C);
        if (depth > 0) ipOff = OuterCursorIp(boss) - buf;        // §7.5-2
        if (const char* n = ResolveBossPhase(PhasesOf(chap), N, ipOff, mLastPhase))
            { mSection = n; return; }
    }
    if (chap) mSection = chap;                                   // 只有一级就先用一级
}
```

### 7.7 实现前必须先补的两件事（E5 的第 0 步）

1. **生成相位边界表**：新增/扩展 `docs/logger/tools/gen_th07_checkpoints.py`，从 `thprac_th07.cpp` 的 `THPatch` 里**按 section 解析补丁偏移**（`ECLSetTime(off,…)` / `pair{off,…}` / `ECLCallSub(off,…)`），输出「section → 升序 offset[]」并嵌入生成物 `thprac_th07_checkpoints_gen.cpp`。**人工抄表不可接受**（§3 有 15 组、上百个偏移）。
2. **确认 boss 槽的唯一性**：真机上打印 `mgr+0x954598` 的 8 个槽与各自 `ipOff`，确认「中boss＋关底」的实际占用与挑槽规则（§7.5-1 目前是推演）。

### 7.8 真机验证清单（🟡 全部未做）

- [ ] 进每个 boss，打印 `tlTime` / `tlIp` / 槽号 / `bossIp−buf` / `depth`，确认一级时间值与 CSV 一致。
- [ ] 逐相位打一遍，确认 `ipOff` 的**跳变点**恰好是 §7.4 的边界（每个 boss 家族至少验一组：ST1 四相位、ST4 三姐妹、ST6 BOSS1..10）。
- [ ] `depth > 0` 的相位（有 `call sub` 的）确认 `OuterCursorIp` 能否取到主脚本游标。
- [ ] 中boss 与关底并存、以及「打掉中boss 后」的槽变化。
- [ ] 练习模式直接 warp 到后期相位（补丁已打）与**正常游玩**（无补丁）两种路径，`ipOff` 是否都能落到正确区间。
- [ ] attract / replay / 暂停 / 重开：复位是否正确。

## 8. 实现影响与待真机清单（全部 🟡）

1. 🟡 **采样**：`time = *(int32_t*)0x12FE0FC`（＝`mgr+0x9545FC`＝`TL_0+8`，报告 §10.2）；`count = *(u16*)(*(u32*)0x1347938 + 2)`。
2. 🟡 **latch 的并列项**：同一时间值有多个白名单项时，「最大入口 ≤ t」不唯一 ⇒ 需**显式 tie-break**（建议：同值时按 §3 副键；无副键时保留先命中的那一条，不回退）。
3. 🟡 **副键读取链**：`E = *(u32*)0x12FE098`（`mgr+0x954598` 槽表首项，空＝无 boss）→ `ip = *(u32*)(E+0x6E4)`（当前 sub 指令指针）→ 与 decl 侧 `SubN` 的指令区间比对（本轮已给出每行补丁落在哪个 `SubN:L###`）。
4. 🟡 **dlg 容差**：10 行双值需 `|time - T| ≤ 1` 或按 `ins_8(0)`/`ins_9(0)` 判别；否则 dlg 分支下会漏 latch。
5. 🟡 **ST4 三姐妹**：`BOSS1_*` / `BOSS2_*` 两组在 ECL 侧**完全没有差异**（`thFakeShot` 是 thprac 自己模拟的机体变量，不是 ECL 状态）⇒ 只能靠**游戏侧机体 / 自机类型**（`character=0x62F645`、`shotType=0x62F646`）或 boss 对象字段区分；`BOSS4_*` / `BOSS5_*` 两组可靠 `call→Sub47` 后的 4 个 `pair` 操作数 1/2/3 区分。**已裁决（2026-10-22，用户）：这属游戏特性、不算 bug，logger 层可区分；维持现状，不登记 `knownBugs`。**
6. 🟡 **中boss 窗口**：与 th10–th18 同构 —— 中boss 存活期间章节 checkpoint 会提前开跑（th16 §5.7 的「只缓存」机制需移植）；本作 MID 时间值与章节 后半 #1 的时间关系见 §2 表（如 `ST3 MID2/MID3`=1858 与 `ST3 后半 #1`=1805 仅差 53）。
7. ✅ **ST6 #2 / ST6 CHAOS 同值 1180**（§3.3）：已裁决 —— 保留 CHAOS、删除 #2，CSV 现为 140 行。
8. 🟡 **8 关**：`ST7`=Extra、`ST8`=Phantasm，`mChapterSetup[8][2]`；实现别按 7 关写死。
9. 🟡 **变体**：Steam / 汉化版 ECL 若改版，时间值可能整体平移；本表基于 `ts=1066942337` 的日文版 `.dat` 解包。

---

## 9. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-22 | 建档 + 全表回填。`docs/logger/th07_checkpoints.csv` 第 1 列填 **141/141**（章节 38/38、boss 103/103，无留空）：值＝`ECLTimeWarp` 的 `time` 入参，dlg 双值 **10 行取非 dlg**。用 2026-10-21 新入本机的 `../exes/ecl/th07/ecldata1..8.ecl` + `ecldump/*.decl` 做交叉验证：确认 th07 ECL 头（`subCount`/`timelineCount`/`tlStart[]`/`subOffset[]@0x44`）与 8 B 指令头，`.decl` 含 `timeline TimelineN()` 块，**762 sub + 17 timeline、22413 条实指令**（＋16 条终止符）与 decl 逐条对齐；boss **100/103** 精确命中 `ins_8(0)`/`ins_9(0)`、10 行 dlg 双值 10/10 命中。登记 **15 组同时间多分节**的副键、**1 处跨类型同值**（`ST6 #2`=`ST6 CHAOS`=1180）、**3 行无精确时间戳**（ST1 MID1/MID2、ST3 MID1）与 **§11 的 8 条差异**（值无错，列名/列位/粒度/注有错）。**未改 `th07_ecl_re_report.md` / `th08` / `logger_progress.md`；未编译、未运行、未改任何 exe/ecl/decl。全部 🟡 未真机。** |
| 2026-10-22 | **用户裁决落地，行数 141 → 140**：`ST6 #2`（章节，1180）与 `ST6 CHAOS`（boss 轴，1180）经 ECL 侧交叉验证为**同一条指令**（`ecldata6.decl` Timeline0 t=1180 的 `ins_0`），时间与 ECL 状态均不可区分 ⇒ 按用户决定**删除 `ST6 #2`、保留 `ST6 CHAOS`**，CSV 变为 **章节 37 + boss 103 = 140**，`verify_th07_entries.py` 期望值同步。**ST4 三姐妹（BOSS1/BOSS2 各 3 行）经裁决维持现状**：属游戏特性（姐妹由自机/子机选择决定），不算 bug、不登记 `knownBugs`，logger 层可另行区分。**未改 `th07_ecl_re_report.md`**（§11 的表述订正由主代理统一收口）。 |
| 2026-10-22 | **新增「监控两级判定流程」节（E5 实现口径，🟡 未真机）**：说明 boss 段时间值必然重复（th07 140 行/53 个值、th08 141 行/67 个值），一级键只定「boss 容器」，相位须二级读 boss 脚本位置；给出完整读取链、签名/区间推导规则、伪代码、宿主挑法与边界，以及实现前必须先做的生成脚本与真机采样项。**CSV 未改**。 |

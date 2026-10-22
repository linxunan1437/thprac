# TH08 checkpoint「时间轴位置」回填 — 入口表小结（141/158；LW 17 行按用户决定留空）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（总括；术语表 / §6 Q9 判定口径）。
> 需求来源：`docs/req_logger.md`（ECL 段落监控 + 日志）。
> 本作逆向报告（**技术口径唯一事实来源**）：`docs/th08/th08_ecl_re_report.md`（**§9.2 入口列口径**、**§附录 A 候选表**、§1 ④ Timeline、§3.4 车道命令、§附录 A.2 末尾 LW 空 case）。
> 回填口径：`docs/logger/checkpoint_entry_backfill_workflow.md`（通用判据）；A 代记法差异见 `docs/logger/checkpoint_csv_workflow.md` §2 / §9（第 1 列表头 = **`时间轴位置`**）。
> 数据：`docs/logger/th08_checkpoints.csv` —— 158 行数据（章节 **37** + boss **121**），第 1 列 **141/158**（章节 37/37、boss 104/121）。
> 二进制：`../exes/th08.exe`，**840704 B**，PE `TimeDateStamp = 1095610188`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**），`SizeOfImage = 0x14DC000`，`sha256 = 330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924`（与 `docs/logger/decompile_materials.md` §2 的 `330fbdbf…` 一致）。
> 素材（**仓库外只读，不提交**）：`../exes/ecl/th08/`（24 份 `.ecl`：`ecldata1..8`、`ecldata1sp..8sp`、`ecldata_al/rm/sk/ym/yk/yy`）；`../exes/ecl_dump/th08/`（thecl `-d8 -m ECL_MAP_08.txt` 的 `.decl`，**指令已具名**）；汇总 `../exes/ecl_dump/_dump_report_a_gen.txt`（th08 24/24 成功）。
> 方法：**源码实读**（`thprac_th08.cpp` 的 `THStageWarp` `:1513–1694`、`THPatch` `:1695–2769`、`mChapterSetup` `:799–810`）+ **`.ecl` 头解析与逐命令/逐指令解码**（Python `struct`，只读）+ `.decl` 交叉核对。**未编译、未运行游戏、未改任何 exe/ecl/decl**。
> 状态：**静态定案（🟡 未真机三态）**。所有结论按仓库既有习惯标 🟡。
> 地址口径：本文不新增地址；引用的 VA/RVA 一律取自 `docs/th08/th08_ecl_re_report.md`（**VA 与 RVA = VA − 0x400000**）。CSV 第 1 列的「偏移」是**相对 ECL 文件缓冲 `pECL` 的字节偏移**（与运行期 `cur_ins − pECL` 同一坐标系），不是 VA。
> 置信度图例：🟡 静态定案（源码 + `.ecl` 字节双证，**未真机**）。

---

## 0. 结论速览

| # | 问题 | 结论 | 一句话 |
| --- | --- | --- | --- |
| 1 | 回填率 | **141/158** | 章节 **37/37**、boss **104/121**；其余 **17 行 = `LW 1..17`，按用户 2026-10-22 决定「无监控意义」有意留空**，**不属待办缺口**。 |
| 2 | 第 1 列取值 | **`时间值/0x偏移`**（大写十六进制、无空格） | = 该行 `ECLWarp(...)` 的**第 1 个 pair**；`ECLWarp` 的第 2–4 个 pair 是辅助 lane，**一律不登记**（自检：0 例误登记）。 |
| 3 | dlg / 非 dlg 双值 | **一律取非 dlg（后者）** | 共 **9 行**：`ST4A 后半 #1`、`ST4B 后半 #1` 与 7 个 `*_BOSS1` / `ST7 END_NS1`；明细见 §1.3。 |
| 4 | `.ecl` 复算 | **141/141 命中**（offset 是合法指令/命令起点） | 全部落在**时间轴车道命令链**上（lane，按 `u8 size @+6` 串联）；**没有**一个落在 sub 的主脚本指令链内，二者 offset 集合**无交集**。 |
| 5 | 记录名 ↔ 枚举名 | **逐项相等** | boss 行 = `TH08_` + 记录名（首个空格→下划线）；与 `th_sections_cba[10][2][20]`（`thprac_locale_def.cpp:11197`）**10 组 121 项逐项核对一致**，不是按 CSV 行序硬套。 |
| 6 | 同值分节 | **17 组**（全在 boss 轴） | 章节行**无**重复值；成因 = thprac 对「同一个 boss 场地」只写一次 `ECLWarp`，节间差异全靠 `THPatch` 的补丁。副键与限制见 §3。 |
| 7 | 与报告附录 A 的差异 | **数值无抄错**；1 处标签错、2 类语义偏差 | 报告 A.1 的 `st4…st9` 是 `THStageWarp` 的 **`stage` 实参**（= `mStage+1`）而非「所属关卡」；**`st9` 是 ST7(Extra)，不是 LW**（报告写「st9（LW?）」有误）。详见 §6。 |
| 8 | 读取面（留给主代理/用户裁决） | 🟡 **offset 全在车道命令区 ⇒ 报告 §9.2 的「读宿主 cursor ip 精确匹配」静态上会 0 命中** | 可行替代 = 读 **Timeline 车道**（`slot+8` = time、`slot+0xC` = 命令指针，报告 §1 ④ / §3.4）。CSV 表头 `时间轴位置` 与此一致。**本文未改** `th08_ecl_re_report.md`。 |

---

## 1. 口径与取值来源

### 1.1 第 1 列 = `ECLWarp` 的第 1 个 pair

- **格式**：`时间值/0x偏移`，十进制时间 + `0x` **大写**十六进制偏移，**无空格**（例 `340/0x9C38`）。正则 `^\d+/0x[0-9A-F]+$`。
- **依据**：`docs/th08/th08_ecl_re_report.md` §9.2 第 3 条「checkpoint『入口』列记 `(time, offset)`（= thprac 该段落 `ECLWarp` 的第一个 pair），不记 sub 名」。
- **辅助 lane 不登记**：`ECLWarp(time1, offset1, time2, offset2, …)`（`thprac_th08.cpp:1407–1439`）一次最多写 4 个车道；第 2–4 个 pair 是背景/符卡/演出脚本（报告 §A.2 注），**不登记为 checkpoint**。
- **`offset == -1` 的 pair** 连指针都不写（只置时间），本身也不可能成为入口；本文各行的第 1 个 `offset` 均非 `-1`。

### 1.2 关卡编号的三层映射（**逐关核对源码，不套公式**）

章节 section id = `10000 + (mStage+1)*100 + portion`（`thprac_th08.cpp:663–667`），`THSectionPatch` 反解为 `stage = (section-10000)/100`、`portion = (section-10000)%100`（`:2776–2779`），再交给 `THStageWarp(ecl, stage, portion)`。故 **`mStage`（0 基）↔ `THStageWarp` 的 `stage` 实参（1 基）↔ ECL 文件** 如下：

| `mStage` | `TH08_STAGE_SELECT` token（= CSV `所属关卡`） | `mChapterSetup` | 章节行数 | `THStageWarp` `stage` 实参 | ECL 文件 | `slotCount` |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | `ST1` | `{1,1}` | 2 | `1` | `ecldata1.ecl` | 2 |
| 1 | `ST2` | `{4,0}` | 4 | `2` | `ecldata2.ecl` | 2 |
| 2 | `ST3` | `{2,1}` | 3 | `3` | `ecldata3.ecl` | 4 |
| 3 | `ST4A` | `{4,2}` | 6 | `4` | `ecldata4a.ecl` | 2 |
| 4 | `ST4B` | `{4,2}` | 6 | `5` | `ecldata4b.ecl` | 2 |
| 5 | `ST5` | `{3,2}` | 5 | `6` | `ecldata5.ecl` | 1 |
| 6 | `ST6A` | `{2,0}` | 2 | `7` | `ecldata6.ecl` | 1 |
| 7 | `ST6B` | `{2,0}` | 2 | `8` | `ecldata7.ecl` | 1 |
| 8 | `ST7`（Extra） | `{3,4}` | 7 | `9` | `ecldata8.ecl` | 2 |
| 9 | `ST8`（LW） | `{0,0}` | 0 | —（无 chapter case） | — | — |

> **注意**：任务书里写的「`ecldata4a/4b/6a/6b`」与实际素材不符 —— 目录里**没有** `ecldata6a/6b`，ST6A / ST6B 分别用 **`ecldata6.ecl` / `ecldata7.ecl`**（`ecldata6` 的 `subCount=88`、`ecldata7` 的 `subCount=89`，且 ST6B 的 warp offset（≥`0x10324`）超出 `ecldata6` 的文件长度 `0xC930`，只能在 `ecldata7` 里命中）。各关 `slotCount` 与 `ECLWarp` 的辅助 lane 数**逐关吻合**（如 ST3 = 4 车道 ↔ 3 个辅助 pair），这是文件映射的独立旁证。

**记录名换算**（`mChapterSetup[i] = {前半, 后半}`；`后半 == 0` → `#k`）：ST2/ST6A/ST6B 用 `#k`，其余用 `前半 #k` / `后半 #k`；与 CSV 逐行核对一致（37 行）。

### 1.3 记录名 ↔ `th_sections` 枚举名（boss 轴）

- 规则：`TH08_` + 记录名（**第一个空格换成下划线**）；`ST7 END_NS1` → `TH08_ST7_END_NS1`、`ST6B LS1` → `TH08_ST6B_LS1`；**LW 例外**：`LW n` → `TH08_LW_n`（无 ST token）。
- 与 `th_sections_cba[10][2][20]`（`thprac_locale_def.cpp:11197`）**逐项核对**：10 组、合计 **121 项**，与 CSV 的 121 个 boss 行**同序同值**（`ST4A`/`ST4B` 的 `[0]` 为空 ⇒ 无 MID 行；`ST6B` 关底含 `LS1..LS5`；`ST8` 组 = `LW_1..LW_17`）。
- **dlg / 非 dlg 双值（9 行，一律取非 dlg）**：

| 记录名 | dlg 值（丢弃） | 非 dlg 值（采用） | 源码 |
| --- | --- | --- | --- |
| `ST4A 后半 #1` | `4962/0xB280` | **`4903/0xB2D8`** | `:1572–1581` |
| `ST4B 后半 #1` | `4962/0xD504` | **`4903/0xD55C`** | `:1605–1616` |
| `ST1 BOSS1` | `4175/0xAC70` | **`4175/0xAC84`** | `:1711–1714` |
| `ST4A BOSS1` | `4962/0xB26C` | **`4962/0xB280`** | `:1948–1951` |
| `ST4B BOSS1` | `4962/0xD4F0` | **`4962/0xD504`** | `:2029–2032` |
| `ST5 BOSS1` | `7481/0xB788` | **`7481/0xB79C`** | `:2128–2134` |
| `ST6A BOSS1` | `4022/0xC824` | **`4022/0xC838`** | `:2230–2236` |
| `ST6B BOSS1` | `4112/0x10534` | **`4112/0x10548`** | `:2398–2405` |
| `ST7 END_NS1` | `9497/0x152B4` | **`9497/0x152C8`** | `:2548–2555` |

> 其余 `*_BOSS1`（仅 `ST2 BOSS1`、`ST3 BOSS1`）与各 `MID1`/`MID2` 无 dlg 分支（单值），直接取该 `ECLWarp`；`ST6B MID1` 的 `ECLWarp` 写在 `if (!dlg)` 内，非 dlg 下取值相同。

---

## 2. 逐关入口表（按 CSV 行序；每关先道中行、再 boss 行）

> 「来源」= `thprac_th08.cpp` 的 `ECLWarp(...)` 行号（双值行给**非 dlg** 那条）。
> 「.ecl 复算」= 该 offset 在**该关 ECL 文件**里的落点：`laneN` = 第 N 条时间轴车道的命令链，`t=` 是该命令**自身**的 `time` 字段（`i32 @+0`）。**它不一定等于 CSV 的 time**，见 §6.3。
> 置信度一律 🟡（源码 + 字节双证，未真机）。

### ST1

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST1 前半 #1 | 340 | `0x9C38` | `thprac_th08.cpp:1518` | `ecldata1.ecl` / lane0 / `0x9C38` / t=400 | 🟡 |
| ST1 后半 #1 | 2875 | `0xABA4` | `thprac_th08.cpp:1521` | `ecldata1.ecl` / lane0 / `0xABA4` / t=2935 | 🟡 |
| ST1 MID1 | 2935 | `0xAB78` | `thprac_th08.cpp:1702` | `ecldata1.ecl` / lane0 / `0xAB78` / t=2935 | 🟡 |
| ST1 MID2 | 2935 | `0xAB78` | `thprac_th08.cpp:1705` | `ecldata1.ecl` / lane0 / `0xAB78` / t=2935 | 🟡 |
| ST1 BOSS1 | 4175 | `0xAC84` | `thprac_th08.cpp:1714` | `ecldata1.ecl` / lane0 / `0xAC84` / t=4175 | 🟡 |
| ST1 BOSS2 | 4175 | `0xAC84` | `thprac_th08.cpp:1717` | `ecldata1.ecl` / lane0 / `0xAC84` / t=4175 | 🟡 |
| ST1 BOSS3 | 4175 | `0xAC84` | `thprac_th08.cpp:1721` | `ecldata1.ecl` / lane0 / `0xAC84` / t=4175 | 🟡 |
| ST1 BOSS4 | 4175 | `0xAC84` | `thprac_th08.cpp:1726` | `ecldata1.ecl` / lane0 / `0xAC84` / t=4175 | 🟡 |
| ST1 LS | 4176 | `0xACD0` | `thprac_th08.cpp:1733` | `ecldata1.ecl` / lane0 / `0xACD0` / t=4176 | 🟡 |

### ST2

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST2 #1 | 400 | `0xA194` | `thprac_th08.cpp:1529` | `ecldata2.ecl` / lane0 / `0xA194` / t=460 | 🟡 |
| ST2 #2 | 990 | `0xA8D8` | `thprac_th08.cpp:1532` | `ecldata2.ecl` / lane0 / `0xA8D8` / t=1050 | 🟡 |
| ST2 #3 | 2350 | `0xB418` | `thprac_th08.cpp:1535` | `ecldata2.ecl` / lane0 / `0xB418` / t=2410 | 🟡 |
| ST2 #4 | 3350 | `0xBA78` | `thprac_th08.cpp:1538` | `ecldata2.ecl` / lane0 / `0xBA78` / t=3410 | 🟡 |
| ST2 MID1 | 4870 | `0xC020` | `thprac_th08.cpp:1740` | `ecldata2.ecl` / lane0 / `0xC020` / t=4870 | 🟡 |
| ST2 MID2 | 4870 | `0xC020` | `thprac_th08.cpp:1745` | `ecldata2.ecl` / lane0 / `0xC020` / t=4870 | 🟡 |
| ST2 BOSS1 | 4870 | `0xC020` | `thprac_th08.cpp:1752` | `ecldata2.ecl` / lane0 / `0xC020` / t=4870 | 🟡 |
| ST2 BOSS2 | 4870 | `0xC020` | `thprac_th08.cpp:1770` | `ecldata2.ecl` / lane0 / `0xC020` / t=4870 | 🟡 |
| ST2 BOSS3 | 4870 | `0xC020` | `thprac_th08.cpp:1826` | `ecldata2.ecl` / lane0 / `0xC020` / t=4870 | 🟡 |
| ST2 BOSS4 | 4870 | `0xC020` | `thprac_th08.cpp:1834` | `ecldata2.ecl` / lane0 / `0xC020` / t=4870 | 🟡 |
| ST2 BOSS5 | 4870 | `0xC020` | `thprac_th08.cpp:1847` | `ecldata2.ecl` / lane0 / `0xC020` / t=4870 | 🟡 |
| ST2 LS | 4871 | `0xC07C` | `thprac_th08.cpp:1862` | `ecldata2.ecl` / lane0 / `0xC07C` / t=4871 | 🟡 |

### ST3

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST3 前半 #1 | 340 | `0xB258` | `thprac_th08.cpp:1546` | `ecldata3.ecl` / lane0 / `0xB258` / t=400 | 🟡 |
| ST3 前半 #2 | 1220 | `0xB368` | `thprac_th08.cpp:1549` | `ecldata3.ecl` / lane0 / `0xB368` / t=1280 | 🟡 |
| ST3 后半 #1 | 3103 | `0xB664` | `thprac_th08.cpp:1552` | `ecldata3.ecl` / lane0 / `0xB664` / t=3163 | 🟡 |
| ST3 MID1 | 3080 | `0xB5D8` | `thprac_th08.cpp:1869` | `ecldata3.ecl` / lane0 / `0xB5D8` / t=3080 | 🟡 |
| ST3 MID2 | 3080 | `0xB5D8` | `thprac_th08.cpp:1874` | `ecldata3.ecl` / lane0 / `0xB5D8` / t=3080 | 🟡 |
| ST3 BOSS1 | 4663 | `0xB87C` | `thprac_th08.cpp:1882` | `ecldata3.ecl` / lane0 / `0xB87C` / t=4663 | 🟡 |
| ST3 BOSS2 | 4663 | `0xB87C` | `thprac_th08.cpp:1887` | `ecldata3.ecl` / lane0 / `0xB87C` / t=4663 | 🟡 |
| ST3 BOSS3 | 4663 | `0xB87C` | `thprac_th08.cpp:1895` | `ecldata3.ecl` / lane0 / `0xB87C` / t=4663 | 🟡 |
| ST3 BOSS4 | 4663 | `0xB87C` | `thprac_th08.cpp:1902` | `ecldata3.ecl` / lane0 / `0xB87C` / t=4663 | 🟡 |
| ST3 BOSS5 | 4663 | `0xB87C` | `thprac_th08.cpp:1907` | `ecldata3.ecl` / lane0 / `0xB87C` / t=4663 | 🟡 |
| ST3 BOSS6 | 4663 | `0xB87C` | `thprac_th08.cpp:1931` | `ecldata3.ecl` / lane0 / `0xB87C` / t=4663 | 🟡 |
| ST3 LS | 4663 | `0xB8CC` | `thprac_th08.cpp:1941` | `ecldata3.ecl` / lane0 / `0xB8CC` / t=4663 | 🟡 |

### ST4A

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST4A 前半 #1 | 340 | `0xA2D4` | `thprac_th08.cpp:1560` | `ecldata4a.ecl` / lane0 / `0xA2D4` / t=400 | 🟡 |
| ST4A 前半 #2 | 1180 | `0xA834` | `thprac_th08.cpp:1563` | `ecldata4a.ecl` / lane0 / `0xA834` / t=1240 | 🟡 |
| ST4A 前半 #3 | 2240 | `0xA8AC` | `thprac_th08.cpp:1566` | `ecldata4a.ecl` / lane0 / `0xA8AC` / t=2300 | 🟡 |
| ST4A 前半 #4 | 3180 | `0xABCC` | `thprac_th08.cpp:1569` | `ecldata4a.ecl` / lane0 / `0xABCC` / t=3240 | 🟡 |
| ST4A 后半 #1 | 4903 | `0xB2D8` | `thprac_th08.cpp:1580` | `ecldata4a.ecl` / lane0 / `0xB2D8` / t=4963 | 🟡 |
| ST4A 后半 #2 | 5363 | `0xB598` | `thprac_th08.cpp:1584` | `ecldata4a.ecl` / lane0 / `0xB598` / t=5423 | 🟡 |
| ST4A BOSS1 | 4962 | `0xB280` | `thprac_th08.cpp:1951` | `ecldata4a.ecl` / lane0 / `0xB280` / t=4962 | 🟡 |
| ST4A BOSS2 | 4962 | `0xB280` | `thprac_th08.cpp:1956` | `ecldata4a.ecl` / lane0 / `0xB280` / t=4962 | 🟡 |
| ST4A BOSS3 | 4962 | `0xB280` | `thprac_th08.cpp:1963` | `ecldata4a.ecl` / lane0 / `0xB280` / t=4962 | 🟡 |
| ST4A BOSS4 | 4962 | `0xB280` | `thprac_th08.cpp:1970` | `ecldata4a.ecl` / lane0 / `0xB280` / t=4962 | 🟡 |
| ST4A BOSS5 | 6923 | `0xBA50` | `thprac_th08.cpp:1978` | `ecldata4a.ecl` / lane0 / `0xBA50` / t=6923 | 🟡 |
| ST4A BOSS6 | 6923 | `0xBA50` | `thprac_th08.cpp:1983` | `ecldata4a.ecl` / lane0 / `0xBA50` / t=6923 | 🟡 |
| ST4A BOSS7 | 6923 | `0xBA50` | `thprac_th08.cpp:1990` | `ecldata4a.ecl` / lane0 / `0xBA50` / t=6923 | 🟡 |
| ST4A BOSS8 | 6923 | `0xBA50` | `thprac_th08.cpp:1996` | `ecldata4a.ecl` / lane0 / `0xBA50` / t=6923 | 🟡 |
| ST4A BOSS9 | 6923 | `0xBA50` | `thprac_th08.cpp:2006` | `ecldata4a.ecl` / lane0 / `0xBA50` / t=6923 | 🟡 |
| ST4A LS | 6923 | `0xBAAC` | `thprac_th08.cpp:2019` | `ecldata4a.ecl` / lane0 / `0xBAAC` / t=6924 | 🟡 |

### ST4B

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST4B 前半 #1 | 340 | `0xC538` | `thprac_th08.cpp:1593` | `ecldata4b.ecl` / lane0 / `0xC538` / t=1 | 🟡 |
| ST4B 前半 #2 | 1180 | `0xCAB8` | `thprac_th08.cpp:1596` | `ecldata4b.ecl` / lane0 / `0xCAB8` / t=1240 | 🟡 |
| ST4B 前半 #3 | 2240 | `0xCB30` | `thprac_th08.cpp:1599` | `ecldata4b.ecl` / lane0 / `0xCB30` / t=2300 | 🟡 |
| ST4B 前半 #4 | 3180 | `0xCE50` | `thprac_th08.cpp:1602` | `ecldata4b.ecl` / lane0 / `0xCE50` / t=3240 | 🟡 |
| ST4B 后半 #1 | 4903 | `0xD55C` | `thprac_th08.cpp:1614` | `ecldata4b.ecl` / lane0 / `0xD55C` / t=4963 | 🟡 |
| ST4B 后半 #2 | 5363 | `0xD81C` | `thprac_th08.cpp:1619` | `ecldata4b.ecl` / lane0 / `0xD81C` / t=5423 | 🟡 |
| ST4B BOSS1 | 4962 | `0xD504` | `thprac_th08.cpp:2032` | `ecldata4b.ecl` / lane0 / `0xD504` / t=4962 | 🟡 |
| ST4B BOSS2 | 4962 | `0xD504` | `thprac_th08.cpp:2037` | `ecldata4b.ecl` / lane0 / `0xD504` / t=4962 | 🟡 |
| ST4B BOSS3 | 4962 | `0xD504` | `thprac_th08.cpp:2044` | `ecldata4b.ecl` / lane0 / `0xD504` / t=4962 | 🟡 |
| ST4B BOSS4 | 4962 | `0xD504` | `thprac_th08.cpp:2052` | `ecldata4b.ecl` / lane0 / `0xD504` / t=4962 | 🟡 |
| ST4B BOSS5 | 6923 | `0xDCD4` | `thprac_th08.cpp:2061` | `ecldata4b.ecl` / lane0 / `0xDCD4` / t=6923 | 🟡 |
| ST4B BOSS6 | 6923 | `0xDCD4` | `thprac_th08.cpp:2066` | `ecldata4b.ecl` / lane0 / `0xDCD4` / t=6923 | 🟡 |
| ST4B BOSS7 | 6923 | `0xDCD4` | `thprac_th08.cpp:2074` | `ecldata4b.ecl` / lane0 / `0xDCD4` / t=6923 | 🟡 |
| ST4B BOSS8 | 6923 | `0xDCD4` | `thprac_th08.cpp:2081` | `ecldata4b.ecl` / lane0 / `0xDCD4` / t=6923 | 🟡 |
| ST4B BOSS9 | 6923 | `0xDCD4` | `thprac_th08.cpp:2091` | `ecldata4b.ecl` / lane0 / `0xDCD4` / t=6923 | 🟡 |
| ST4B LS | 6923 | `0xDD30` | `thprac_th08.cpp:2104` | `ecldata4b.ecl` / lane0 / `0xDD30` / t=6924 | 🟡 |

### ST5

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST5 前半 #1 | 200 | `0xA674` | `thprac_th08.cpp:1628` | `ecldata5.ecl` / lane0 / `0xA674` / t=260 | 🟡 |
| ST5 前半 #2 | 1040 | `0xA694` | `thprac_th08.cpp:1631` | `ecldata5.ecl` / lane0 / `0xA694` / t=1100 | 🟡 |
| ST5 前半 #3 | 2990 | `0xAFDC` | `thprac_th08.cpp:1634` | `ecldata5.ecl` / lane0 / `0xAFDC` / t=3050 | 🟡 |
| ST5 后半 #1 | 4651 | `0xB0E8` | `thprac_th08.cpp:1637` | `ecldata5.ecl` / lane0 / `0xB0E8` / t=4711 | 🟡 |
| ST5 后半 #2 | 5971 | `0xB648` | `thprac_th08.cpp:1640` | `ecldata5.ecl` / lane0 / `0xB648` / t=6031 | 🟡 |
| ST5 MID1 | 4530 | `0xB0BC` | `thprac_th08.cpp:2114` | `ecldata5.ecl` / lane0 / `0xB0BC` / t=4530 | 🟡 |
| ST5 MID2 | 4530 | `0xB0BC` | `thprac_th08.cpp:2119` | `ecldata5.ecl` / lane0 / `0xB0BC` / t=4530 | 🟡 |
| ST5 BOSS1 | 7481 | `0xB79C` | `thprac_th08.cpp:2131` | `ecldata5.ecl` / lane0 / `0xB79C` / t=7481 | 🟡 |
| ST5 BOSS2 | 7481 | `0xB79C` | `thprac_th08.cpp:2138` | `ecldata5.ecl` / lane0 / `0xB79C` / t=7481 | 🟡 |
| ST5 BOSS3 | 7481 | `0xB79C` | `thprac_th08.cpp:2146` | `ecldata5.ecl` / lane0 / `0xB79C` / t=7481 | 🟡 |
| ST5 BOSS4 | 7481 | `0xB79C` | `thprac_th08.cpp:2156` | `ecldata5.ecl` / lane0 / `0xB79C` / t=7481 | 🟡 |
| ST5 BOSS5 | 7481 | `0xB79C` | `thprac_th08.cpp:2168` | `ecldata5.ecl` / lane0 / `0xB79C` / t=7481 | 🟡 |
| ST5 BOSS6 | 7481 | `0xB79C` | `thprac_th08.cpp:2178` | `ecldata5.ecl` / lane0 / `0xB79C` / t=7481 | 🟡 |
| ST5 BOSS7 | 7481 | `0xB79C` | `thprac_th08.cpp:2190` | `ecldata5.ecl` / lane0 / `0xB79C` / t=7481 | 🟡 |
| ST5 LS | 7484 | `0xB820` | `thprac_th08.cpp:2203` | `ecldata5.ecl` / lane0 / `0xB820` / t=7484 | 🟡 |

### ST6A

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST6A #1 | 200 | `0xC6F4` | `thprac_th08.cpp:1648` | `ecldata6.ecl` / lane0 / `0xC6F4` / t=260 | 🟡 |
| ST6A #2 | 1140 | `0xC714` | `thprac_th08.cpp:1651` | `ecldata6.ecl` / lane0 / `0xC714` / t=1200 | 🟡 |
| ST6A MID1 | 3400 | `0xC7D4` | `thprac_th08.cpp:2210` | `ecldata6.ecl` / lane0 / `0xC7D4` / t=3400 | 🟡 |
| ST6A MID2 | 3400 | `0xC7D4` | `thprac_th08.cpp:2219` | `ecldata6.ecl` / lane0 / `0xC7D4` / t=3400 | 🟡 |
| ST6A BOSS1 | 4022 | `0xC838` | `thprac_th08.cpp:2233` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A BOSS2 | 4022 | `0xC838` | `thprac_th08.cpp:2241` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A BOSS3 | 4022 | `0xC838` | `thprac_th08.cpp:2250` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A BOSS4 | 4022 | `0xC838` | `thprac_th08.cpp:2260` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A BOSS5 | 4022 | `0xC838` | `thprac_th08.cpp:2272` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A BOSS6 | 4022 | `0xC838` | `thprac_th08.cpp:2282` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A BOSS7 | 4022 | `0xC838` | `thprac_th08.cpp:2294` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A BOSS8 | 4022 | `0xC838` | `thprac_th08.cpp:2304` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A BOSS9 | 4022 | `0xC838` | `thprac_th08.cpp:2315` | `ecldata6.ecl` / lane0 / `0xC838` / t=4022 | 🟡 |
| ST6A LS | 4083 | `0xC89C` | `thprac_th08.cpp:2326` | `ecldata6.ecl` / lane0 / `0xC89C` / t=4083 | 🟡 |

### ST6B

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST6B #1 | 200 | `0x10324` | `thprac_th08.cpp:1659` | `ecldata7.ecl` / lane0 / `0x10324` / t=260 | 🟡 |
| ST6B #2 | 1140 | `0x10364` | `thprac_th08.cpp:1662` | `ecldata7.ecl` / lane0 / `0x10364` / t=1200 | 🟡 |
| ST6B MID1 | 3490 | `0x104E4` | `thprac_th08.cpp:2379` | `ecldata7.ecl` / lane0 / `0x104E4` / t=3490 | 🟡 |
| ST6B MID2 | 3490 | `0x104E4` | `thprac_th08.cpp:2388` | `ecldata7.ecl` / lane0 / `0x104E4` / t=3490 | 🟡 |
| ST6B BOSS1 | 4112 | `0x10548` | `thprac_th08.cpp:2402` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B BOSS2 | 4112 | `0x10548` | `thprac_th08.cpp:2410` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B BOSS3 | 4112 | `0x10548` | `thprac_th08.cpp:2419` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B BOSS4 | 4112 | `0x10548` | `thprac_th08.cpp:2430` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B BOSS5 | 4112 | `0x10548` | `thprac_th08.cpp:2443` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B BOSS6 | 4112 | `0x10548` | `thprac_th08.cpp:2454` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B BOSS7 | 4112 | `0x10548` | `thprac_th08.cpp:2467` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B BOSS8 | 4112 | `0x10548` | `thprac_th08.cpp:2478` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B BOSS9 | 4112 | `0x10548` | `thprac_th08.cpp:2490` | `ecldata7.ecl` / lane0 / `0x10548` / t=4112 | 🟡 |
| ST6B LS1 | 4234 | `0x105AC` | `thprac_th08.cpp:2503` | `ecldata7.ecl` / lane0 / `0x105AC` / t=4234 | 🟡 |
| ST6B LS2 | 4235 | `0x105EC` | `thprac_th08.cpp:2508` | `ecldata7.ecl` / lane0 / `0x105EC` / t=4235 | 🟡 |
| ST6B LS3 | 4236 | `0x1062C` | `thprac_th08.cpp:2513` | `ecldata7.ecl` / lane0 / `0x1062C` / t=4236 | 🟡 |
| ST6B LS4 | 4237 | `0x1066C` | `thprac_th08.cpp:2518` | `ecldata7.ecl` / lane0 / `0x1066C` / t=4237 | 🟡 |
| ST6B LS5 | 4238 | `0x106AC` | `thprac_th08.cpp:2523` | `ecldata7.ecl` / lane0 / `0x106AC` / t=4238 | 🟡 |

### ST7

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST7 前半 #1 | 340 | `0x12544` | `thprac_th08.cpp:1670` | `ecldata8.ecl` / lane0 / `0x12544` / t=400 | 🟡 |
| ST7 前半 #2 | 1010 | `0x127A8` | `thprac_th08.cpp:1673` | `ecldata8.ecl` / lane0 / `0x127A8` / t=1070 | 🟡 |
| ST7 前半 #3 | 2900 | `0x12B6C` | `thprac_th08.cpp:1676` | `ecldata8.ecl` / lane0 / `0x12B6C` / t=2960 | 🟡 |
| ST7 后半 #1 | 5237 | `0x132B8` | `thprac_th08.cpp:1679` | `ecldata8.ecl` / lane0 / `0x132B8` / t=5297 | 🟡 |
| ST7 后半 #2 | 5947 | `0x136B4` | `thprac_th08.cpp:1682` | `ecldata8.ecl` / lane0 / `0x136B4` / t=6007 | 🟡 |
| ST7 后半 #3 | 6757 | `0x13774` | `thprac_th08.cpp:1685` | `ecldata8.ecl` / lane0 / `0x13774` / t=6847 | 🟡 |
| ST7 后半 #4 | 8337 | `0x151F4` | `thprac_th08.cpp:1688` | `ecldata8.ecl` / lane0 / `0x151F4` / t=8397 | 🟡 |
| ST7 MID1 | 5155 | `0x13268` | `thprac_th08.cpp:2526` | `ecldata8.ecl` / lane0 / `0x13268` / t=5155 | 🟡 |
| ST7 MID2 | 5155 | `0x13268` | `thprac_th08.cpp:2534` | `ecldata8.ecl` / lane0 / `0x13268` / t=5155 | 🟡 |
| ST7 MID3 | 5155 | `0x13268` | `thprac_th08.cpp:2541` | `ecldata8.ecl` / lane0 / `0x13268` / t=5155 | 🟡 |
| ST7 END_NS1 | 9497 | `0x152C8` | `thprac_th08.cpp:2551` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S1 | 9497 | `0x152C8` | `thprac_th08.cpp:2559` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_NS2 | 9497 | `0x152C8` | `thprac_th08.cpp:2567` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S2 | 9497 | `0x152C8` | `thprac_th08.cpp:2576` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_NS3 | 9497 | `0x152C8` | `thprac_th08.cpp:2587` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S3 | 9497 | `0x152C8` | `thprac_th08.cpp:2596` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_NS4 | 9497 | `0x152C8` | `thprac_th08.cpp:2607` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S4 | 9497 | `0x152C8` | `thprac_th08.cpp:2617` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_NS5 | 9497 | `0x152C8` | `thprac_th08.cpp:2629` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S5 | 9497 | `0x152C8` | `thprac_th08.cpp:2638` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_NS6 | 9497 | `0x152C8` | `thprac_th08.cpp:2649` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S6 | 9497 | `0x152C8` | `thprac_th08.cpp:2660` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_NS7 | 9497 | `0x152C8` | `thprac_th08.cpp:2673` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S7 | 9497 | `0x152C8` | `thprac_th08.cpp:2684` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_NS8 | 9497 | `0x152C8` | `thprac_th08.cpp:2697` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S8 | 9497 | `0x152C8` | `thprac_th08.cpp:2708` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S9 | 9497 | `0x152C8` | `thprac_th08.cpp:2721` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_S10 | 9497 | `0x152C8` | `thprac_th08.cpp:2732` | `ecldata8.ecl` / lane0 / `0x152C8` / t=9497 | 🟡 |
| ST7 END_LS | 9678 | `0x1530C` | `thprac_th08.cpp:2743` | `ecldata8.ecl` / lane0 / `0x1530C` / t=9678 | 🟡 |

### ST8

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| LW 1 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 2 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 3 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 4 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 5 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 6 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 7 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 8 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 9 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 10 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 11 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 12 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 13 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 14 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 15 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 16 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |
| LW 17 | — | — | `thprac_th08.cpp:2748-2765`（空 case） | — | — |

---

## 3. 同值分节（重复 `time/offset`）

**成因**：thprac 对「同一个 boss 场地」只写**一次** `ECLWarp`；第 n 个 boss 段的差异全靠 `THPatch` 后续补丁（`ECLSetTime` / `ECLCallSub` / `ECLJump` / `ECLTimeFix` 改写主脚本字节）实现。故**同一个 entry 值对应多个记录名是 ECL 事实**，照填重复值。

**可用的副键（`.decl` 侧可得）**：
1. **车道命令 `arg0`** —— 时间轴命令 `opcode = 0`、`arg0 = *(i32*)(ins+8)`，即**该命令派生的 sub_id**，`.decl` 里对应 `Sub<arg0>`（如 `ST1 BOSS1` → `Sub25`、`ST6B LS1..LS5` → `Sub42/46/47/48/49`）。**同组内通常相同**（同一场地同一入口），只在跨组时区分。
2. **`THPatch` 补丁序列** —— 每个记录名在 `ECLWarp` 之后追加的补丁（**补丁偏移 + 目标 sub_id**）。这是真正能逐行区分的副键：`.decl` 里把该偏移落到具体 `SubN`，即可把「同值」拆成不同相位。

**限制（如实记录）**：有若干组**连补丁目标 sub 都相同**（`ST5 BOSS1..7` 全部 `CallSub@0x4C6C→sub51`、`ST6A BOSS1..9` 全部 `CallSub@0x367C→sub33`、`ST6B BOSS1..9` 全部 `CallSub@0x3A28→sub27`、`ST7 END_*` 全部 `CallSub@0x65C4→sub66`），只能靠**补丁偏移**（如 `ST7 END_*` 的 `0x6794/0x67B0/0x67C4` 三分支）或**补丁写入的时间值**（`SetTime` 的 `t`）区分。若监控只按 `(time, offset)` 匹配，这些行**必然无法互相区分** —— 这是 A 代口径的固有粒度限制，需真机 + 副键才能定案。

| # | 值（time/offset） | 行数 | 成员（记录名） | 车道命令 `arg0`（→ `.decl`） | 各成员 `THPatch` 补丁序列（`.decl` 可定位） |
| --- | --- | --- | --- | --- | --- |
| 1 | `2935/0xAB78` | 2 | ST1 MID1<br>ST1 MID2 | `15` → `Sub15` | ST1 MID1: —<br>ST1 MID2: SetTime@0x178C→(t=0,sub22) |
| 2 | `4175/0xAC84` | 4 | ST1 BOSS1<br>ST1 BOSS2<br>ST1 BOSS3<br>ST1 BOSS4 | `25` → `Sub25` | ST1 BOSS1: —<br>ST1 BOSS2: SetTime@0x3D30→(t=0,sub38)<br>ST1 BOSS3: Jump@0x3D08→0x4E0C<br>ST1 BOSS4: CallSub@0x3D08→sub33; SetTime@0x4EF0→(t=0,sub44) |
| 3 | `4870/0xC020` | 7 | ST2 MID1<br>ST2 MID2<br>ST2 BOSS1<br>ST2 BOSS2<br>ST2 BOSS3<br>ST2 BOSS4<br>ST2 BOSS5 | `17` → `Sub17` | ST2 MID1: —<br>ST2 MID2: SetTime@0x22B0→(t=0,sub23)<br>ST2 BOSS1: Jump@0x1E34→0x1EA8; CallSub@0x1F98→sub27; Jump@0x37C0→0x3880<br>ST2 BOSS2: Jump@0x1E34→0x1EA8; CallSub@0x1F98→sub27; Jump@0x37C0→0x3880; SetTime@0x389C→(t=0,sub(diff <= 1) ? 33 : 38)<br>ST2 BOSS3: CallSub@0x3D68→sub51; CallSub@0x1ED8→sub27; CallSub@0x389C→sub29<br>ST2 BOSS4: CallSub@0x1ED8→sub27; CallSub@0x389C→sub29; SetTime@0x3E30→(t=0,sub44)<br>ST2 BOSS5: CallSub@0x1ED8→sub27; CallSub@0x389C→sub29; SetTime@0x3E30→(t=0,sub51) |
| 4 | `3080/0xB5D8` | 2 | ST3 MID1<br>ST3 MID2 | `20` → `Sub20` | ST3 MID1: —<br>ST3 MID2: SetTime@0x245C→(t=0,sub30) |
| 5 | `4663/0xB87C` | 6 | ST3 BOSS1<br>ST3 BOSS2<br>ST3 BOSS3<br>ST3 BOSS4<br>ST3 BOSS5<br>ST3 BOSS6 | `34` → `Sub34` | ST3 BOSS1: —<br>ST3 BOSS2: SetTime@0x3F04→(t=0,sub44)<br>ST3 BOSS3: SetTime@0x3F04→(t=0,sub47)<br>ST3 BOSS4: CallSub@0x3F04→sub38<br>ST3 BOSS5: CallSub@0x3F04→sub38; SetTime@0x4748→(t=0,subsub_id)<br>ST3 BOSS6: CallSub@0x3F04→sub38; SetTime@0x4748→(t=0,sub67); Jump@0x462C→0x46DC; Jump@0x3D80→0x3DF0 |
| 6 | `4962/0xB280` | 4 | ST4A BOSS1<br>ST4A BOSS2<br>ST4A BOSS3<br>ST4A BOSS4 | `16` → `Sub16` | ST4A BOSS1: —<br>ST4A BOSS2: SetTime@0x188C→(t=0,sub28)<br>ST4A BOSS3: CallSub@0x1870→sub19<br>ST4A BOSS4: CallSub@0x1870→sub19; SetTime@0x22F0→(t=0,sub30) |
| 7 | `6923/0xBA50` | 5 | ST4A BOSS5<br>ST4A BOSS6<br>ST4A BOSS7<br>ST4A BOSS8<br>ST4A BOSS9 | `24` → `Sub24` | ST4A BOSS5: —<br>ST4A BOSS6: SetTime@0x2FA8→(t=0,sub33)<br>ST4A BOSS7: —<br>ST4A BOSS8: SetTime@0x3850→(t=0,sub41)<br>ST4A BOSS9: CallSub@0x3850→sub45 |
| 8 | `4962/0xD504` | 4 | ST4B BOSS1<br>ST4B BOSS2<br>ST4B BOSS3<br>ST4B BOSS4 | `16` → `Sub16` | ST4B BOSS1: —<br>ST4B BOSS2: SetTime@0x18D0→(t=0,sub36)<br>ST4B BOSS3: CallSub@0x18B4→sub22<br>ST4B BOSS4: CallSub@0x18B4→sub22; SetTime@0x2570→(t=0,sub41) |
| 9 | `6923/0xDCD4` | 5 | ST4B BOSS5<br>ST4B BOSS6<br>ST4B BOSS7<br>ST4B BOSS8<br>ST4B BOSS9 | `27` → `Sub27` | ST4B BOSS5: —<br>ST4B BOSS6: SetTime@0x304C→(t=0,sub47)<br>ST4B BOSS7: —<br>ST4B BOSS8: SetTime@0x38D4→(t=0,sub55)<br>ST4B BOSS9: CallSub@0x38D4→sub61 |
| 10 | `4530/0xB0BC` | 2 | ST5 MID1<br>ST5 MID2 | `38` → `Sub38` | ST5 MID1: —<br>ST5 MID2: Jump@0x4360→0x437C; SetHealth@0x37E0 |
| 11 | `7481/0xB79C` | 7 | ST5 BOSS1<br>ST5 BOSS2<br>ST5 BOSS3<br>ST5 BOSS4<br>ST5 BOSS5<br>ST5 BOSS6<br>ST5 BOSS7 | `48` → `Sub48` | ST5 BOSS1: CallSub@0x4C6C→sub51<br>ST5 BOSS2: CallSub@0x4C6C→sub51; SetTime@0x4F78→(t=0,sub62)<br>ST5 BOSS3: CallSub@0x4C6C→sub51; CallSub@0x4F5C→sub53<br>ST5 BOSS4: CallSub@0x4C6C→sub51; CallSub@0x4F5C→sub53; SetTime@0x52D8→(t=0,sub66)<br>ST5 BOSS5: CallSub@0x4C6C→sub51; CallSub@0x4F5C→sub56<br>ST5 BOSS6: CallSub@0x4C6C→sub51; CallSub@0x4F5C→sub56; SetTime@0x583C→(t=0,sub63)<br>ST5 BOSS7: CallSub@0x4C6C→sub51; CallSub@0x4F5C→sub56; SetTime@0x583C→(t=0,sub75) |
| 12 | `3400/0xC7D4` | 2 | ST6A MID1<br>ST6A MID2 | `16` → `Sub16` | ST6A MID1: Jump@0x1EC0→0x1EE0<br>ST6A MID2: Jump@0x1EC0→0x1EE0; SetTime@0x1FDC→(t=0,sub21) |
| 13 | `4022/0xC838` | 9 | ST6A BOSS1<br>ST6A BOSS2<br>ST6A BOSS3<br>ST6A BOSS4<br>ST6A BOSS5<br>ST6A BOSS6<br>ST6A BOSS7<br>ST6A BOSS8<br>ST6A BOSS9 | `28` → `Sub28` | ST6A BOSS1: CallSub@0x367C→sub33<br>ST6A BOSS2: CallSub@0x367C→sub33; SetTime@0x3DE4→(t=0,sub52)<br>ST6A BOSS3: CallSub@0x367C→sub33; CallSub@0x3DC8→sub36<br>ST6A BOSS4: CallSub@0x367C→sub33; CallSub@0x3DC8→sub36; SetTime@0x4600→(t=0,sub56)<br>ST6A BOSS5: CallSub@0x367C→sub33; CallSub@0x3DC8→sub40<br>ST6A BOSS6: CallSub@0x367C→sub33; CallSub@0x3DC8→sub40; SetTime@0x4D4C→(t=0,sub63)<br>ST6A BOSS7: CallSub@0x367C→sub33; CallSub@0x3DC8→sub44<br>ST6A BOSS8: CallSub@0x367C→sub33; CallSub@0x3DC8→sub44; SetTime@0x55E8→(t=0,sub68)<br>ST6A BOSS9: CallSub@0x367C→sub33; CallSub@0x3DC8→sub72 |
| 14 | `3490/0x104E4` | 2 | ST6B MID1<br>ST6B MID2 | `14` → `Sub14` | ST6B MID1: CallSub@0x1CD8→sub15<br>ST6B MID2: CallSub@0x1CD8→sub15; SetTime@0x1DF4→(t=0,sub19) |
| 15 | `4112/0x10548` | 9 | ST6B BOSS1<br>ST6B BOSS2<br>ST6B BOSS3<br>ST6B BOSS4<br>ST6B BOSS5<br>ST6B BOSS6<br>ST6B BOSS7<br>ST6B BOSS8<br>ST6B BOSS9 | `26` → `Sub26` | ST6B BOSS1: CallSub@0x3A28→sub27<br>ST6B BOSS2: CallSub@0x3A28→sub27; SetTime@0x3C24→(t=0,sub50)<br>ST6B BOSS3: CallSub@0x3A28→sub27; CallSub@0x3C08→sub31<br>ST6B BOSS4: CallSub@0x3A28→sub27; CallSub@0x3C08→sub31; SetTime@0x4424→(t=0,sub55)<br>ST6B BOSS5: CallSub@0x3A28→sub27; CallSub@0x3C08→sub34<br>ST6B BOSS6: CallSub@0x3A28→sub27; CallSub@0x3C08→sub34; SetTime@0x4CEC→(t=0,sub61)<br>ST6B BOSS7: CallSub@0x3A28→sub27; CallSub@0x3C08→sub38<br>ST6B BOSS8: CallSub@0x3A28→sub27; CallSub@0x3C08→sub38; SetTime@0x5624→(t=0,sub67)<br>ST6B BOSS9: CallSub@0x3A28→sub27; CallSub@0x3C08→sub70 |
| 16 | `5155/0x13268` | 3 | ST7 MID1<br>ST7 MID2<br>ST7 MID3 | `50` → `Sub50` | ST7 MID1: CallSub@0x3E2C→sub51<br>ST7 MID2: CallSub@0x3E10→sub55<br>ST7 MID3: CallSub@0x3E10→sub59 |
| 17 | `9497/0x152C8` | 18 | ST7 END_NS1<br>ST7 END_S1<br>ST7 END_NS2<br>ST7 END_S2<br>ST7 END_NS3<br>ST7 END_S3<br>ST7 END_NS4<br>ST7 END_S4<br>ST7 END_NS5<br>ST7 END_S5<br>ST7 END_NS6<br>ST7 END_S6<br>ST7 END_NS7<br>ST7 END_S7<br>ST7 END_NS8<br>ST7 END_S8<br>ST7 END_S9<br>ST7 END_S10 | `65` → `Sub65` | ST7 END_NS1: CallSub@0x65C4→sub66<br>ST7 END_S1: CallSub@0x65C4→sub66; SetTime@0x67B0→(t=0,sub93)<br>ST7 END_NS2: CallSub@0x65C4→sub66; CallSub@0x67B0→sub68<br>ST7 END_S2: CallSub@0x65C4→sub66; CallSub@0x67B0→sub68; SetTime@0x6B9C→(t=0,sub97)<br>ST7 END_NS3: CallSub@0x65C4→sub66; CallSub@0x67B0→sub70<br>ST7 END_S3: CallSub@0x65C4→sub66; CallSub@0x67B0→sub70; SetTime@0x6F64→(t=0,sub100)<br>ST7 END_NS4: CallSub@0x65C4→sub66; CallSub@0x67B0→sub72<br>ST7 END_S4: CallSub@0x65C4→sub66; CallSub@0x67B0→sub72; SetTime@0x733C→(t=0,sub103)<br>ST7 END_NS5: CallSub@0x65C4→sub66; CallSub@0x67B0→sub74<br>ST7 END_S5: CallSub@0x65C4→sub66; CallSub@0x67B0→sub74; SetTime@0x7638→(t=0,sub108)<br>ST7 END_NS6: CallSub@0x65C4→sub66; CallSub@0x6794→sub76<br>ST7 END_S6: CallSub@0x65C4→sub66; CallSub@0x6794→sub76; SetTime@0x7944→(t=0,sub113)<br>ST7 END_NS7: CallSub@0x65C4→sub66; CallSub@0x6794→sub78<br>ST7 END_S7: CallSub@0x65C4→sub66; CallSub@0x6794→sub78; SetTime@0x7CCC→(t=0,sub118)<br>ST7 END_NS8: CallSub@0x65C4→sub66; CallSub@0x6794→sub80<br>ST7 END_S8: CallSub@0x65C4→sub66; CallSub@0x6794→sub80; SetTime@0x8044→(t=0,sub126)<br>ST7 END_S9: CallSub@0x65C4→sub66; CallSub@0x67B0→sub82; Jump@0x95F4→0x98C4; Jump@0x98DC→0x9910<br>ST7 END_S10: CallSub@0x65C4→sub66; CallSub@0x67C4→sub83 |

> 共 **17 组 / 覆盖 104 个 boss 行中的 91 行**；另 **13 行值唯一**（`ST1 LS`、`ST2 LS`、`ST3 LS`、`ST4A LS`、`ST4B LS`、`ST5 LS`、`ST6A LS`、`ST6B LS1..LS5`（5 行）、`ST7 END_LS`）。章节行 **0 组**（37 行全部唯一）。

---

## 4. `LW 1..17`（`ST8`，17 行）

**结论（用户 2026-10-22 决定）：LW 系列无监控意义，不登记入口，17 行留空。**

事实注脚（仅此一条，不再展开）：
- `THPatch` 里 `TH08_LW_1` … `TH08_LW_17` 是**空 case**（`thprac_th08.cpp:2748–2765`，17 个 `case` + 一个 `break;`），**没有 `ECLWarp`**，因此**没有「入口 pair」可填**。
- `THStageWarp` 的 `stage == 9` 分支是 **ST7（Extra）** 的 7 个 portion，**不是 LW**（LW 的 `mChapterSetup[9] = {0,0}`，无 chapter case）。
- 按本决定，**不再从 `../exes/ecl/` 反推 LW 入口**，17 行保持第 1 列空白，行序 / 记录名 / 其余 3 列均未改动。

---

## 5. 留空行与原因（17 行）

| # | 记录名 | 所属关卡 | 原因 | 是否待办缺口 |
| --- | --- | --- | --- | --- |
| 1–17 | `LW 1` … `LW 17` | `ST8` | **用户 2026-10-22 决定：LW 无监控意义，不登记**；源码为空 case（`:2748–2765`），本无 `ECLWarp` | **否**（有意留空） |

> 除这 17 行外，**没有因证据不足而留空的行**：37 章节行 + 104 boss 行全部有「源码行号 + `.ecl` 字节落点」双证。

---

## 6. 与 `docs/th08/th08_ecl_re_report.md` 附录 A 的差异（**本文不改原报告，留主代理收口**）

### 6.1 D1（标签错）附录 A.1 的 `stN` 是 **`THStageWarp` 的 `stage` 实参**，不是「所属关卡」

- 报告 A.1 用小写 `st1..st9` 标行，正文没说明它就是源码 `stage` 实参。按源码（`:1513–1694`）与 §1.2 的映射：
  `st1=ST1`、`st2=ST2`、`st3=ST3`、**`st4=ST4A`、`st5=ST4B`、`st6=ST5`、`st7=ST6A`、`st8=ST6B`、`st9=ST7(Extra)`**。
- **`st9（LW?）` 有误**：`stage == 9` 的 7 个 portion（`340/0x12544` … `8337/0x151F4`）是 **ST7（Extra）的 `前半 #1..#3` + `后半 #1..#4`**；LW 在 `THStageWarp` 里**完全没有 case**（源码 `:1667–1693` 之后即函数结束）。全部**偏移数值本身正确**，只是标签与「LW」的猜测错。
- 证据：`mChapterSetup[8] = {3,4}`（7 = ST7 的章节行数）、`TH08_STAGE_SELECT[8] = TH_STAGE_EXTRA`、`mChapterSetup[9] = {0,0}`。

### 6.2 D2（读取面，🟡 需裁决）**141 个 offset 全部落在时间轴车道命令区，不在任何 sub 的指令链内**

- `.ecl` 结构（`pECL+0` magic `0x800`、`+4` i16 subCount、`+6` i16 slotCount、`+8` u32[slotCount] **车道初始命令**、`+0x48` u32[subCount] sub 起始偏移表）。本机实测：各关**所有 sub 的指令链**与**车道命令链**的 offset 集合**完全不相交**（自检脚本对 9 个文件逐一求 `车道命令起点 ∩ sub 指令起点` = ∅）。
- 141 个 offset **100% 命中车道命令链**（沿 `slotInit` 起、按 **`u8 size @+6`** 串联 —— 引擎 `0x42AD3D: movzx ecx, byte ptr [eax+6]` 就是这条链的推进方式），**0 命中** sub 指令链。
- ⇒ 报告 §9.2 建议的「`offset = *(u32*)(cursor) − *(u32*)0x4ECCB8`，与 `ECLWarp` 的 `offset` **精确相等**匹配」**静态上会 0 命中**：主解释器（`0x4184B0`–`0x41ECBD`）只跑 sub，**不会把 cursor 停到车道命令区**（报告 §3.4 已说明 lane 命令走另一张 17 项 opcode 表 `0x42AD60`）。
- **可行替代（与 CSV 表头 `时间轴位置` 一致）**：读 **Timeline 车道** —— `TL = 0xF54CF0`（= `0x577F20 + 0x9DCDD0`）、`slot[i] = TL + i*0x10`，`slot+8` = time、`slot+0xC` = 命令指针（报告 §1 ④）。thprac 的 `ECLWarp` 写的正是这两个字段（`:1412–1437`）。**真机需验**：warp 后下一个 stage-update 帧，`slot[i]+8 == 该行 time` 且 `slot[i]+0xC == pECL + 该行 offset`。
- ⚠️ 需注意：车道命令**不是**「当前位置」，而是「待生效的定时跳转」；引擎在 `slot+8` 到达时执行命令并**推进** `slot+0xC`（`0x42AD37–0x42AD4D`）。故 `(time, offset)` 只在 warp 后的窗口内精确相等，之后即失配。**这是 th08 监控实现要正面处理的问题**，本文只登记、不裁决。

**本文引用的地址（VA / RVA = VA − 0x400000；均取自 `docs/th08/th08_ecl_re_report.md`，本文不新增）**

| 用途 | VA | RVA | 出处 |
| --- | --- | --- | --- |
| ECL 资源单例（`+0`=pECL、`+4`=sub 表） | `0x4ECCB8` | `0xECCB8` | 报告 §1 ① |
| Timeline 车道数组（= `0x577F20 + 0x9DCDD0`，步长 `0x10`） | `0xF54CF0` | `0xB54CF0` | 报告 §1 ④ |
| 管理器（车道数组宿主） | `0x577F20` | `0x177F20` | 报告 §1 ③ |
| 主 ECL 解释器（入口 / 末尾 `ret 4`） | `0x4184B0` / `0x41ECBD` | `0x184B0` / `0x1ECBD` | 报告 §6.1 |
| 车道命令处理器（每次扫描对每个 slot 调用） | `0x42A8A0` | `0x2A8A0` | 报告 §1 ④ |
| 车道命令**推进**（`movzx ecx, byte ptr [eax+6]`） | `0x42AD37` / `0x42AD3D` | `0x2AD37` / `0x2AD3D` | 本文 §6.3 |
| 车道命令 `time` 相等比较（`*(slot+8) == ins->time`） | `0x40D3F0` | `0xD3F0` | 报告 §1 ④ / 本文 §6.3 |
| 不相等且 `slot+8 < ins->time` ⇒ 停 | `0x42AD52` | `0x2AD52` | 本文 §6.3 |
| 车道命令 opcode 跳表（17 项） | `0x42AD60` | `0x2AD60` | 报告 §3.4 |
| stage update 里的车道扫描循环 | `0x42C7D5` | `0x2C7D5` | 报告 §1 ④ |

### 6.3 D3（time 语义）**CSV 的 time 是 `ECLWarp` 的参数，不是该 offset 命令自身的 time**

`.ecl` 复算得：**39 行**的命令自身 `time` ≠ CSV 的 time。

| 类别 | 行数 | 偏差 | 明细 |
| --- | --- | --- | --- |
| 章节行 | 35 | **+60** | 除下表两行外的全部章节行 |
| 章节行 | 1 | **+90** | `ST7 后半 #3`（CSV 6757 / `0x13774` / 命令 t=6847） |
| 章节行 | 1 | **−339** | `ST4B 前半 #1`（CSV 340 / `0xC538` / 命令 t=1）—— 该 offset **恰是 `ecldata4b` 的 `slot0` 初始命令**（车道起点），不是「340 对应的那条命令」 |
| boss 行 | 2 | **+1** | `ST4A LS`（6923 / `0xBAAC` / t=6924）、`ST4B LS`（6923 / `0xDD30` / t=6924） |

- 另：**章节 p1 的 offset 常等于（或等于 +0x20）该关 `slot0` 初始命令**（`ST4B 前半 #1`、`ST5 前半 #1`、`ST6A #1`、`ST6B #1`、`ST7 前半 #1` 都**正好是** `slot0` 初值；`ST1/ST2/ST3/ST4A` 的 p1 是初值 `+0x20`）。
- **解释（静态推断，🟡）**：引擎的判定是 `*(u32*)(slot+8) == ins->time`（`0x42A8C5`→`0x40D3F0`），**不相等时**：`slot+8 < ins->time` ⇒ 停（`0x42AD52`）；`slot+8 ≥ ins->time` ⇒ 沿 `u8[+6]` **跳过**该命令（`0x42AD37`）。⇒ `ECLWarp` 的 `offset` 是**扫描起点**、`time` 是**车道时钟目标值**，二者不必相等；首个被执行的命令是「从 offset 起、time ≥ 目标值」的那条。**这可以解释 +60 的系统性偏差**（目标值比该命令早 1 秒）。
- **对监控的含义**：若按 `(time, offset)` **双键精确匹配**，39 行会对不上；若按 **offset 单键**（报告 §9.2 说 offset 为主键），则不受影响。**建议主代理/用户在实现时按 offset 单键，time 仅作日志可读性** —— 但这是口径变更，本文不擅自改。

### 6.4 D4（未发现抄错）

- 附录 A.1 / A.2 的 **141 个 `(time, offset)` 数值与源码逐项相等**（本文用正则重解析 `THStageWarp`/`THPatch` 后逐行比对，0 处不一致）；辅助 lane 的注记（`st3` 的 `0xB91C/0xBB74/0xC0AC`、`st4–5` 的 `0xBED4/0xE158`、`st9` 的 `0x1535C…`）也全部正确。
- 唯一需要改的是 **§6.1 的标签**与 **§6.2/§6.3 的口径注记**，均**留给主代理收口**，本文**未改** `docs/th08/th08_ecl_re_report.md`。

---

## 7. 自检

脚本：`docs/logger/tools/verify_th08_entries.py`（只读；`--git` 额外比对非第 1 列）。实跑结果：

```text
== CSV 形态 ==
表头            : ['时间轴位置', '所属游戏', '所属关卡', '记录名']
数据行          : 158 (期望 158)
章节行 / boss 行 : 37 / 121 (期望 37 / 121)
第 1 列非空      : 141 (期望 141 = 37 章节 + 104 boss)
格式 ^\d+/0x[0-9A-F]+$ : OK

== 源码一致 ==
逐行与 THStageWarp/THPatch 比对 : OK

== .ecl 复算 ==
offset 反查命中   : 141/141
反查落点类别      : {'lane': 141}
命令自身 time != CSV time : 39 行
辅助 lane 误登记  : OK（无）

-- 车道命令链 ∩ sub 指令链（9 关）--
   ST1   ecldata1.ecl   lanes=190  insns=1455 交集=0
   ST2   ecldata2.ecl   lanes=326  insns=1582 交集=0
   ST3   ecldata3.ecl   lanes=158  insns=1805 交集=0
   ST4A  ecldata4a.ecl  lanes=270  insns=1606 交集=0
   ST4B  ecldata4b.ecl  lanes=270  insns=2054 交集=0
   ST5   ecldata5.ecl   lanes=142  insns=1754 交集=0
   ST6A  ecldata6.ecl   lanes=29   insns=2036 交集=0
   ST6B  ecldata7.ecl   lanes=48   insns=2612 交集=0
   ST7   ecldata8.ecl   lanes=508  insns=3268 交集=0

== git HEAD 比对（仅第 1 列可变）==
非第 1 列被改动的行 : 无

== 结论 ==
PASS：158 行 = 章节 37 + boss 121；已填 141（章节 37 + boss 104）；17 行 LW 有意留空（用户 2026-10-22 决定）
```

逐项对照：

- [x] **158 = 37 + 121**（数据行；表头 1 行不计）。
- [x] 每行**恰 4 列**；`所属游戏` 一律 `th08`。
- [x] 第 1 列非空 **141** 行，全部匹配 `^\d+/0x[0-9A-F]+$`（大写十六进制、无空格）。
- [x] **offset 全部能在 `.ecl` 里找到对应指令**：**141/141**（100%）命中「某条命令的起点」；落点类别全是 `lane`（时间轴车道），无一落在 sub 指令链。
- [x] **无辅助 lane 被误登记**：0 例（对 `0xAD18/0xC0CC/0xB91C/0xBB74/0xC0AC/0xBAFC/0xBED4/0xDD80/0xE158/0x1535C/0x155C0/0x15824/0x15D60` 逐一比对）。
- [x] **记录名 ↔ 枚举名**：boss 轴 121 行与 `th_sections_cba` 逐项相等；章节轴 37 行与 `mChapterSetup` + `TH08_STAGE_SELECT` 逐行相等。
- [x] **只改第 1 列**：`git diff` 141 行、`+/-` 两侧第 2–4 列逐字相同；17 行 LW 未改动。

---

## 8. 裁决结果与未做项

> 2026-10-22 用户裁决：**① 改读 Timeline 车道**；**② offset 单键**；**③ 同值分节属实现层限制，接受**；**④ `LW 1..17` 无监控意义，留空、不算缺口**。

1. ✅ **读取面（§6.2）**：141 个 offset 全在时间轴车道命令区、与 sub 指令链交集为空 ⇒ 报告 §9.2 的「读宿主 cursor 的 `cur_ins − pECL` 精确匹配」**作废**，改为读 **Timeline 车道**（`TL=0xF54CF0`，`slot+8`=time、`slot+0xC`=命令指针；`ECLWarp` 写的正是这两个字段）。CSV 表头 `时间轴位置` 与此一致。**待主代理把该结论回填 `th08_ecl_re_report.md` §9.2 / §0。**
2. ✅ **匹配键（§6.3）**：**offset 单键**。CSV 的 `time` 是 `ECLWarp` 入参（warp 目标时间），不是落点命令自身的 time（39 行不同，多为 +60），故 time **只作日志可读性/辅助校验**，不参与相等判定。
3. ✅ **同值分节（§3）**：接受「同一 `(time,offset)` 的多相位不可分」为**已知实现层限制** —— 监控只按 `(offset)` 命中，`ST5 BOSS1..7`、`ST6A/B BOSS1..9`、`ST7 END_*` 等组需在实现层配合 `THPatch` 补丁语义另行消歧；**不在 CSV 里造伪副键**。
4. ✅ **`LW 1..17`**：用户 2026-10-22 决定「LW 无监控意义」⇒ 17 行**有意留空**，不做 ECL 反推，**不计入缺口**。
5. ❌ **真机三态**：未做（本机离线）。需验：warp 后车道 `slot+8/slot+0xC` 是否等于 CSV 的 `(time, offset)`；哪条车道是主线（`slotCount` 1–4，thprac 固定写 `slot0..3`）。
6. ➖ **th07 / `logger_progress.md` / `th08_ecl_re_report.md`**：按分工**未改**（留给主代理收口）。

---

## 9. 与其它文档的关系

- **口径 / Q9** → `docs/logger/logger_re_disasm_overview.md` §6 Q9。
- **回填流程（通用）** → `docs/logger/checkpoint_entry_backfill_workflow.md`；**A 代差异**（第 1 列表头、时间值/偏移记法）→ `docs/logger/checkpoint_csv_workflow.md` §2 / §9。
- **技术口径唯一事实来源** → `docs/th08/th08_ecl_re_report.md`（§9.2 / §附录 A / §1 ④ / §3.4）；本文 §6 的差异**待其收口**。
- **素材来源** → `docs/logger/decompile_materials.md`（A 代段落尚未更新，本文以 `../exes/ecl_dump/_dump_report_a_gen.txt` 的 24/24 为准）。
- **成品数据** → `docs/logger/th08_checkpoints.csv`；**自检** → `docs/logger/tools/verify_th08_entries.py`。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-22 | 建档并回填 `docs/logger/th08_checkpoints.csv` 第 1 列（`时间轴位置`）：**141/158** = 章节 37/37 + boss 104/121；`LW 1..17` 共 17 行**按用户 2026-10-22 决定有意留空**（源码为空 case `:2748–2765`，不反推 ECL）。取值 = `ECLWarp` 第 1 个 pair，dlg/非 dlg 双值取非 dlg（9 行）。新建本文件与 `docs/logger/tools/verify_th08_entries.py`。**`.ecl` 复算 141/141 命中**，全部落在时间轴车道命令区（lane）⇒ 登记 D1（附录 A.1 `st9` 标签错，实为 ST7 Extra）、D2（§9.2 cursor ip 匹配不可行，应读 Timeline 车道）、D3（39 行命令 time ≠ CSV time，多为 +60）、D4（141 个数值无抄错）。**未改** `th07`、`logger_progress.md`、`th08_ecl_re_report.md`。**全部 🟡 静态定案、未真机。** |
| 2026-10-22 | **用户裁决落地**：① 读取面改为 **Timeline 车道**（`slot+8`/`slot+0xC`），原 §9.2 的「宿主 cursor 精确匹配」作废；② 匹配键定为 **offset 单键**（`time` 仅作可读性）；③ 同值分节接受为实现层限制、不造伪副键；④ `LW 1..17` 按用户决定留空、不算缺口。§8 由「仍需裁决」改写为「裁决结果与未做项」。**未改 `th08_ecl_re_report.md`**（§9.2/§0/附录 A.1 的订正由主代理统一收口）。 |

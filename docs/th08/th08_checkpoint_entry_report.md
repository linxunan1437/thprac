# TH08 章节 checkpoint「时间轴位置」回填 — 入口表（章节 37 行）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（总括；术语表 / §6 Q9 判定口径）。
> 需求来源：`docs/req_logger.md`（ECL 段落监控 + 日志）。
> 本作逆向报告（**技术口径唯一事实来源**）：`docs/th08/th08_ecl_re_report.md`（§9.2 入口列口径、§1 ④ Timeline、§3.4 车道命令）。
> 回填口径：`docs/logger/checkpoint_entry_backfill_workflow.md`；A 代记法差异见 `docs/logger/checkpoint_csv_workflow.md` §2 / §9（第 1 列表头 = **`时间轴位置`**）。
> 数据：`docs/logger/th08_checkpoints.csv` —— 158 行数据（章节 **37** + boss **121**），第 1 列 **141/158**（章节 37/37、boss 104/121）。**本文件只保留章节（37 行）部分**；boss 部分已按 2026-10-27 的裁决全部撤下（见 §3）。CSV 未做任何改动。
> 二进制：`../exes/th08.exe`，**840704 B**，PE `TimeDateStamp = 1095610188`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**），`SizeOfImage = 0x14DC000`，`sha256 = 330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924`。
> 素材（**仓库外只读，不提交**）：`../exes/ecl/th08/`（24 份 `.ecl`）、`../exes/ecl_dump/th08/`（thecl `-d8 -m ECL_MAP_08.txt` 的 `.decl`，指令已具名）。
> 方法：源码实读（`thprac_th08.cpp` 的 `THStageWarp:1513–1694`、`mChapterSetup:799–810`）+ `.ecl` 头解析与逐命令解码 + `.decl` 交叉核对。**未编译、未运行游戏、未改任何 exe/ecl/decl。**
> 状态：**静态定案（🟡 未真机三态）**。
> 地址口径：引用的 VA/RVA 一律取自 `docs/th08/th08_ecl_re_report.md`。CSV 第 1 列的「偏移」是**相对 ECL 文件缓冲 `pECL` 的字节偏移**，不是 VA。
> 置信度图例：🟡 静态定案（源码 + `.ecl` 字节双证，**未真机**）。

---

## 0. 结论速览

| # | 问题 | 结论 | 一句话 |
| --- | --- | --- | --- |
| 1 | 回填率 | ✅ **141/158** | 章节 **37/37**、boss 104/121；本文只覆盖章节 37 行。 |
| 2 | 第 1 列取值 | ✅ **`ECLWarp` 的第 1 个 pair** | 道中行＝时间值；同一行可能带 offset。 |
| 3 | 章节行有留空吗 | ❌ **没有** | 17 个空值全部是 `ST8` 的 `LW 1..17`（boss 轴），已在撤下范围内。 |
| 4 | 真机 | ❌ **未做** | 全部 🟡。 |

---

## 1. 口径

### 1.1 第 1 列 = `ECLWarp` 的第 1 个 pair

- 第 1 列取值 = 该 section 的 `ECLWarp(...)` 调用中**第 1 个 pair 的第 1 个分量**（道中＝时间值）；「来源」列给 `thprac_th08.cpp` 行号（双值行给**非 dlg** 那条）。
- 「.ecl 复算」列 = 该 offset 在**该关 ECL 文件**里的落点：`laneN` = 第 N 条时间轴车道的命令链，`t=` 是该命令**自身**的 `time` 字段（`i32 @+0`）。**它不一定等于 CSV 的 time**，见 `docs/th08/th08_ecl_re_report.md` §9.2。
- 置信度一律 🟡（源码 + 字节双证，未真机）。

---

## 2. 章节逐关表（37 行，按 CSV 行序）

| 记录名 | time | offset | 来源（源码行号） | .ecl 复算（文件 / 车道 / 字节偏移 / 命令 time） | 置信度 |
| --- | --- | --- | --- | --- | --- |
| ST1 前半 #1 | 340 | `0x9C38` | `thprac_th08.cpp:1518` | `ecldata1.ecl` / lane0 / `0x9C38` / t=400 | 🟡 |
| ST1 后半 #1 | 2875 | `0xABA4` | `thprac_th08.cpp:1521` | `ecldata1.ecl` / lane0 / `0xABA4` / t=2935 | 🟡 |
| ST2 #1 | 400 | `0xA194` | `thprac_th08.cpp:1529` | `ecldata2.ecl` / lane0 / `0xA194` / t=460 | 🟡 |
| ST2 #2 | 990 | `0xA8D8` | `thprac_th08.cpp:1532` | `ecldata2.ecl` / lane0 / `0xA8D8` / t=1050 | 🟡 |
| ST2 #3 | 2350 | `0xB418` | `thprac_th08.cpp:1535` | `ecldata2.ecl` / lane0 / `0xB418` / t=2410 | 🟡 |
| ST2 #4 | 3350 | `0xBA78` | `thprac_th08.cpp:1538` | `ecldata2.ecl` / lane0 / `0xBA78` / t=3410 | 🟡 |
| ST3 前半 #1 | 340 | `0xB258` | `thprac_th08.cpp:1546` | `ecldata3.ecl` / lane0 / `0xB258` / t=400 | 🟡 |
| ST3 前半 #2 | 1220 | `0xB368` | `thprac_th08.cpp:1549` | `ecldata3.ecl` / lane0 / `0xB368` / t=1280 | 🟡 |
| ST3 后半 #1 | 3103 | `0xB664` | `thprac_th08.cpp:1552` | `ecldata3.ecl` / lane0 / `0xB664` / t=3163 | 🟡 |
| ST4A 前半 #1 | 340 | `0xA2D4` | `thprac_th08.cpp:1560` | `ecldata4a.ecl` / lane0 / `0xA2D4` / t=400 | 🟡 |
| ST4A 前半 #2 | 1180 | `0xA834` | `thprac_th08.cpp:1563` | `ecldata4a.ecl` / lane0 / `0xA834` / t=1240 | 🟡 |
| ST4A 前半 #3 | 2240 | `0xA8AC` | `thprac_th08.cpp:1566` | `ecldata4a.ecl` / lane0 / `0xA8AC` / t=2300 | 🟡 |
| ST4A 前半 #4 | 3180 | `0xABCC` | `thprac_th08.cpp:1569` | `ecldata4a.ecl` / lane0 / `0xABCC` / t=3240 | 🟡 |
| ST4A 后半 #1 | 4903 | `0xB2D8` | `thprac_th08.cpp:1580` | `ecldata4a.ecl` / lane0 / `0xB2D8` / t=4963 | 🟡 |
| ST4A 后半 #2 | 5363 | `0xB598` | `thprac_th08.cpp:1584` | `ecldata4a.ecl` / lane0 / `0xB598` / t=5423 | 🟡 |
| ST4B 前半 #1 | 340 | `0xC538` | `thprac_th08.cpp:1593` | `ecldata4b.ecl` / lane0 / `0xC538` / t=1 | 🟡 |
| ST4B 前半 #2 | 1180 | `0xCAB8` | `thprac_th08.cpp:1596` | `ecldata4b.ecl` / lane0 / `0xCAB8` / t=1240 | 🟡 |
| ST4B 前半 #3 | 2240 | `0xCB30` | `thprac_th08.cpp:1599` | `ecldata4b.ecl` / lane0 / `0xCB30` / t=2300 | 🟡 |
| ST4B 前半 #4 | 3180 | `0xCE50` | `thprac_th08.cpp:1602` | `ecldata4b.ecl` / lane0 / `0xCE50` / t=3240 | 🟡 |
| ST4B 后半 #1 | 4903 | `0xD55C` | `thprac_th08.cpp:1614` | `ecldata4b.ecl` / lane0 / `0xD55C` / t=4963 | 🟡 |
| ST4B 后半 #2 | 5363 | `0xD81C` | `thprac_th08.cpp:1619` | `ecldata4b.ecl` / lane0 / `0xD81C` / t=5423 | 🟡 |
| ST5 前半 #1 | 200 | `0xA674` | `thprac_th08.cpp:1628` | `ecldata5.ecl` / lane0 / `0xA674` / t=260 | 🟡 |
| ST5 前半 #2 | 1040 | `0xA694` | `thprac_th08.cpp:1631` | `ecldata5.ecl` / lane0 / `0xA694` / t=1100 | 🟡 |
| ST5 前半 #3 | 2990 | `0xAFDC` | `thprac_th08.cpp:1634` | `ecldata5.ecl` / lane0 / `0xAFDC` / t=3050 | 🟡 |
| ST5 后半 #1 | 4651 | `0xB0E8` | `thprac_th08.cpp:1637` | `ecldata5.ecl` / lane0 / `0xB0E8` / t=4711 | 🟡 |
| ST5 后半 #2 | 5971 | `0xB648` | `thprac_th08.cpp:1640` | `ecldata5.ecl` / lane0 / `0xB648` / t=6031 | 🟡 |
| ST6A #1 | 200 | `0xC6F4` | `thprac_th08.cpp:1648` | `ecldata6.ecl` / lane0 / `0xC6F4` / t=260 | 🟡 |
| ST6A #2 | 1140 | `0xC714` | `thprac_th08.cpp:1651` | `ecldata6.ecl` / lane0 / `0xC714` / t=1200 | 🟡 |
| ST6B #1 | 200 | `0x10324` | `thprac_th08.cpp:1659` | `ecldata7.ecl` / lane0 / `0x10324` / t=260 | 🟡 |
| ST6B #2 | 1140 | `0x10364` | `thprac_th08.cpp:1662` | `ecldata7.ecl` / lane0 / `0x10364` / t=1200 | 🟡 |
| ST7 前半 #1 | 340 | `0x12544` | `thprac_th08.cpp:1670` | `ecldata8.ecl` / lane0 / `0x12544` / t=400 | 🟡 |
| ST7 前半 #2 | 1010 | `0x127A8` | `thprac_th08.cpp:1673` | `ecldata8.ecl` / lane0 / `0x127A8` / t=1070 | 🟡 |
| ST7 前半 #3 | 2900 | `0x12B6C` | `thprac_th08.cpp:1676` | `ecldata8.ecl` / lane0 / `0x12B6C` / t=2960 | 🟡 |
| ST7 后半 #1 | 5237 | `0x132B8` | `thprac_th08.cpp:1679` | `ecldata8.ecl` / lane0 / `0x132B8` / t=5297 | 🟡 |
| ST7 后半 #2 | 5947 | `0x136B4` | `thprac_th08.cpp:1682` | `ecldata8.ecl` / lane0 / `0x136B4` / t=6007 | 🟡 |
| ST7 后半 #3 | 6757 | `0x13774` | `thprac_th08.cpp:1685` | `ecldata8.ecl` / lane0 / `0x13774` / t=6847 | 🟡 |
| ST7 后半 #4 | 8337 | `0x151F4` | `thprac_th08.cpp:1688` | `ecldata8.ecl` / lane0 / `0x151F4` / t=8397 | 🟡 |

合计 **37 行**，与 CSV 章节行逐行对应。

---

## 3. boss 阶段部分已撤下（2026-10-27）

原文件关于 **boss 阶段**的内容已全部删除，**不作任何保留、也不作结论引用**，包括：

- 记录名 ↔ `th_sections` 枚举名（boss 轴）的映射；
- 「同值分节」（重复 `time/offset`）的分组与相位判定；
- `LW 1..17` 与 17 行留空的登记；
- 与 `th08_ecl_re_report.md` 附录 A 的 D1–D4 差异（boss 入口口径）；
- §9「监控两级判定流程」的全部内容（一级 Timeline 车道 / 二级 boss 脚本位置）。

**撤下原因**：这些结论建立在「boss 阶段可由运行期字段唯一确定」的假设上，该假设未在真机证实，且与 A 代引擎的其它观察冲突，已无法保证正确性。

**未动**：`docs/logger/th08_checkpoints.csv` 的 121 行 boss 记录与 17 个 `LW` 空值原样保留，仅作为**待重做**的输入。

---

## 4. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-22 | 建档：章节 + boss 入口表回填，第 1 列 `时间值/0x偏移`（141/158）；登记 D1–D4 差异、`LW` 留空。全部 🟡 未真机。 |
| 2026-10-27 | **删除全部 boss 阶段分析**（枚举映射、同值分节、`LW`、D1–D4、监控两级判定），只保留章节 37 行。**CSV 未动、代码未动、🟡 未真机。** |

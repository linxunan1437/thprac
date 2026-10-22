# 反编译素材清单与来源

> 上位文档：`docs/req_logger.md`（需求）、`docs/logger/logger_re_disasm_overview.md`（总括）。
> 用途：**登记本机（macOS）现成的反编译素材及其来源**，供后续逆向 / 白名单 CSV 生成引用，避免各篇报告各写各的路径。
> 素材根：**`../exes/`**（相对 `thprac/` 仓库根，即 `/Users/zerozero/pracWork/thprac/../exes`）。该目录在**仓库之外、不进 git**。
> 本机无法编译、无法真机运行；素材仅供**只读静态反汇编 / 离线 ECL 解码**。

---

## 0. 一图看全

| 类别 | 位置 | 内容 | 来源 | 逐作数量 |
| --- | --- | --- | --- | --- |
| 原始可执行 | `../exes/th06.exe` ~ `th18.exe` | 各作原始 PE | 各作官方安装（详见 §2） | 12 个 |
| 拆包 ECL | `../exes/ecl/th06`~`th08/*.ecl`、`th10`~`th18/*.ecl` | 游戏 `*.dat` / `data\*.ecl` 原始脚本 | A 代从 `.dat` 解包、B/C 代为拆包目录（详见 §3.1） | 7/8/24 + 8/20/9/21/24/23/22/22/22 |
| 反编译 decl | `../exes/ecl_dump/th06`~`th08/*.decl`、`th10`~`th18/*.decl` | `thecl -d` 反编译结果 | THTK thtk12（详见 §3.2） | 同左 |
| dump 汇总 | `../exes/ecl_dump/_dump_report.txt`（B/C 代）+ `_dump_report_a_gen.txt`（A 代） | 逐作「期望/实际/失败」 | `dump_ecl_decl.ps1` / thdat+thecl 手工流水（详见 §3.2） | 171/171 + 39/39 成功 |

- **`ecl/` 与 `ecl_dump/` 是同一批脚本的两面**：`ecl/thXX/*.ecl` 是**原始二进制**，`ecl_dump/thXX/*.decl` 是其**反编译文本**（sub 名、`@call` 结构等）。
- **A 代（th06–th08）素材已于 2026-10-21 补齐**：三作 exe 均已入库；ECL 从各作 `.dat` 解出（th06 = `紅魔郷ST.DAT`、th07 = `th07.dat`、th08 = `th08.dat`），共 **7 / 8 / 24** 份，`thecl -d6/7/8` 全部反编译成功（39/39）。**th06 / th07 无 eclmap**（指令保持 `ins_*`、sub 名为 `SubN`）；**th08 有 `ECL_MAP_08.txt` 且实测可套用**（A 代 th08 指令名正常还原，详见 §3.2）。A 代已于 2026-10-16 完成 **E0 建档**：`docs/th06|th07|th08/thXX_ecl_re_requests.md`。

---

## 1. 目录布局

```text
../exes/
├── th06.exe … th18.exe          # 各作原始可执行（§2）
├── ecl/                          # 拆包 ECL 原始脚本（§3.1）
│   ├── th06/  ecldata1.ecl … ecldata7.ecl
│   ├── th07/  ecldata1.ecl … ecldata8.ecl
│   ├── th08/  ecldata{1..8}.ecl / ecldata{1..8}sp.ecl / ecldata_{al,rm,sk,yk,ym,yy}.ecl
│   ├── th10/  default.ecl stage01.ecl … stage07.ecl
│   ├── th11/  default.ecl stageNN(.ecl/.boss.ecl/.mboss.ecl) stage4c0X{a,b,c}.ecl
│   ├── th12/  default.ecl stageNN.ecl stage07boss.ecl
│   └── th13/ … th18/  default.ecl stNN(.ecl/.bs.ecl/.mbs.ecl[2])
└── ecl_dump/                     # thecl -d 反编译结果（§3.2）
    ├── _dump_report.txt          # B/C 代（th10–th18）
    ├── _dump_report_a_gen.txt    # A 代（th06–th08）
    └── th06/ … th18/  <同名>.decl
```

---

## 2. 原始可执行（`../exes/*.exe`）

| 作品 | 文件 | 字节 | PE `TimeDateStamp` | 备注 |
| --- | --- | --- | --- | --- |
| th06 | `th06.exe` | 512000 | 1038721275 | **2026-10-21 入库**；取自 `D:\C P\C2H2\[th06]\th06.exe`（与同目录 `東方紅魔郷.exe` 同指纹），`SizeOfCode = 430080`；`sha256=9f76483c…` |
| th07 | `th07.exe` | 650752 | 1066942337 | 日文版；`sha256=f7d03ba1…` 与 `[th07] 东方妖妖梦 (日文版)` 逐字节相同 |
| th08 | `th08.exe` | 840704 | 1095610188 | 日文版；`sha256=330fbdbf…` 与 `[th08] 东方永夜抄 (日文版)` 逐字节相同 |
| th10 | `th10.exe` | 487936 | 1186086096 | B 代引擎 |
| th11 | `th11.exe` | 688128 | 1217734394 | B 代；安装为**汉化版+日文版** |
| th12 | `th12.exe` | 737280 | 1251344348 | B 代；安装为**汉化版+日文版** |
| th13 | `th13.exe` | 790528 | 1313589413 | C 代 |
| th14 | `th14.exe` | 897536 | 1376634201 | C 代 |
| th15 | `th15.exe` | 951808 | 1444028071 | C 代；**参照作** |
| th16 | `th16.exe` | 683520 | 1501534029 | C 代 |
| th17 | `th17.exe` | 747520 | 1565429804 | C 代 |
| th18 | `th18.exe` | 847360 | 1618619144 | C 代 |

- 全部 `ImageBase = 0x400000`、`RELOCS_STRIPPED`（不重定位）⇒ **`RVA = VA − 0x400000`**。
- **同一性背书**：上表全部 exe 的尺寸 / `TimeDateStamp` 与 `thprac/src/thprac/thprac_launcher_games_def.h` 各作 ORIGINAL 条目一致；th06 / th07 / th08 另有 sha256 与官方安装逐字节比对记录（th06 = `9f76483c…`、th07 = `f7d03ba1…`、th08 = `330fbdbf…`，见各作报告头部）。
- 各篇 `docs/thXX/thXX_re_disasm_report.md` / `thXX_ecl_re_report.md` 头部引用的 `../exes/thXX.exe` 即本目录。
- **A 代 exe 已齐**：`th06.exe` 于 2026-10-21 从 `D:\C P\C2H2\[th06]\th06.exe` 复制入库（512000 B / `ts=1038721275` / `SizeOfCode=430080`，与识别表一致）；`th07.exe` / `th08.exe` 此前已在库且与安装目录逐字节相同。

---

## 3. ECL 素材（`../exes/ecl/` 与 `../exes/ecl_dump/`）

### 3.1 拆包 ECL（原始 `.ecl`）

| 作品 | 目录 | 命名形态 | `.ecl` 数 |
| --- | --- | --- | --- |
| th06 | `ecl/th06/` | `ecldata1.ecl` … `ecldata7.ecl`（1–6 面 + Extra） | 7 |
| th07 | `ecl/th07/` | `ecldata1.ecl` … `ecldata8.ecl`（1–6 面 + Extra + Phantasm） | 8 |
| th08 | `ecl/th08/` | `ecldata1..8.ecl`、`ecldata1..8sp.ecl`（符卡练习）、`ecldata_{al,rm,sk,yk,ym,yy}.ecl` | 24 |
| th10 | `ecl/th10/` | `stageNN.ecl`（+`default.ecl`） | 8 |
| th11 | `ecl/th11/` | `stageNN.ecl` / `stageNNboss.ecl` / `stageNNmboss.ecl` / `stage4c0X{a,b,c}.ecl`（+`default.ecl`） | 20 |
| th12 | `ecl/th12/` | `stageNN.ecl` / `stage07boss.ecl`（+`default.ecl`） | 9 |
| th13 | `ecl/th13/` | `stNN.ecl` / `stNNbs.ecl` / `stNNmbs.ecl`（+`default.ecl`） | 21 |
| th14 | `ecl/th14/` | 同上（+`st04bs2.ecl` / `st04mbs2.ecl`） | 24 |
| th15 | `ecl/th15/` | 同上（+`st01mbs2.ecl` / `st07bs2.ecl`） | 23 |
| th16 | `ecl/th16/` | 同上（+`st07mbs2.ecl`） | 22 |
| th17 | `ecl/th17/` | 同上 | 22 |
| th18 | `ecl/th18/` | 同上 | 22 |

**来源**：B/C 代为各作**安装目录的拆包结果**，取 `<游戏根>\[th<版本>…]\data\*.ecl`（Windows 侧游戏根 `D:\C P\C2H2\`）。切包目录名带方括号前缀（如 `[th10] 东方风神录`、`[th11] 东方地灵殿 (汉化版+日文版)`、`[th18]`），**一关 ≠ 一个 `.ecl`**：`default.ecl` 提供共享 sub，Boss / 道中Boss 段落在 `*bs` / `*mbs` / `*boss` / `*mboss` 里。

**A 代来源（2026-10-21 补齐）**：A 代的 ECL **不在独立 `data\` 目录**，而是打包在 `.dat` 归档内，用 `thdat -x<版本>` 解出：

| 作品 | 归档 | 解包命令 | 归档内 ECL 成员 |
| --- | --- | --- | --- |
| th06 | `[th06]\紅魔郷ST.DAT`（2964458 B；同目录另一份 ST.DAT 与其 sha256 相同） | `thdat -x6 紅魔郷ST.DAT ecldata*.ecl` | `ecldata1..7.ecl`（7 个；其余 TL/IN/MD/ED/CM 归档不含 ECL） |
| th07 | `[th07] 东方妖妖梦 (日文版)\th07.dat`（26315677 B） | `thdat -x7 th07.dat ecldata*.ecl` | `ecldata1..8.ecl`（8 个） |
| th08 | `[th08] 东方永夜抄 (日文版)\th08.dat`（46838025 B） | `thdat -x8 th08.dat ecldata*.ecl` | `ecldata*.ecl`（24 个，含 `*sp` 与角色后缀） |

> th08 解出的 `.ecl` 比 `thdat -l8` 列表中的 Size 少 4 B（THTK 对 TH08/TH09 归档尾部加密填充的处理），`thecl -d8` 可直接解析，不影响内容。

> **2026-10-17 补记（素材的「实际被引用」情况）**：上表是**拆包目录里存在**的 `.ecl`，**不等于**都被该关 `ecli{}` 引用。两例：**th13 的 `st06mbs.ecl` 存在，但 ST6 的 `ecli{}` 不含它**（遗留未用文件）；**th10 无独立 boss 文件**（boss sub 内联在 `stageNN.ecl`）、**th12 只有 `stage07boss.ecl`**（st1–st6 的 boss 内联）。离线枚举一律以各关 `.decl` 头的 `ecli{}` 为准（见 `checkpoint_entry_backfill_workflow.md` §3.1）。

### 3.2 反编译 decl（`thecl -d` 产物）

| 作品 | 目录 | `.decl` 数 | 备注 |
| --- | --- | --- | --- |
| th06 | `ecl_dump/th06/` | 7 | `thecl -d6`；无 eclmap ⇒ 指令 `ins_*`、sub 名 `SubN` |
| th07 | `ecl_dump/th07/` | 8 | `thecl -d7`；无 eclmap ⇒ 指令 `ins_*`、sub 名 `SubN` |
| th08 | `ecl_dump/th08/` | 24 | `thecl -d8 -m ECL_MAP_08.txt`；**实测 map 可套用 A 代 th08**，指令已具名（如 `anmSetEx` / `hitboxSet` / `fset_sub`） |
| th10 | `ecl_dump/th10/` | 8 | |
| th11 | `ecl_dump/th11/` | 20 | |
| th12 | `ecl_dump/th12/` | 9 | |
| th13 | `ecl_dump/th13/` | 21 | |
| th14 | `ecl_dump/th14/` | 24 | |
| th15 | `ecl_dump/th15/` | 23 | |
| th16 | `ecl_dump/th16/` | 22 | |
| th17 | `ecl_dump/th17/` | 22 | |
| th18 | `ecl_dump/th18/` | 22 | **无 eclmap** ⇒ 指令呈 `ins_*` 形态（sub 名、`@call` 结构不受影响） |

**生成工具**：THTK **release 12** 的 `thecl.exe`（Windows 侧 `D:\C P\C2H2\拆包\THTK\thtk\thtk12\thecl.exe`），
命令形如 `thecl -d <版本> [-m ECL_MAP_<版本>.txt] <in.ecl> <out.decl>`；`<版本>` 直接取作号 `6 … 18`。
eclmap（把 `ins_NNN(...)` 翻成具名指令）在 `THTK\MAP\ECL_MAP_<XX>.txt`：**无 `06` / `07`**，故 th06 / th07 不加 `-m`；**th08 的 `ECL_MAP_08.txt` 可用**；**没有 18**，故 th18 不加 `-m`。

**A 代流水（2026-10-21）**：`thdat -x6/7/8 <归档> <ecl...>` → `thecl -d6/7/8 [-m ECL_MAP_08.txt] <in.ecl> <out.decl>`。
th06 从 `紅魔郷ST.DAT`、th07 从 `th07.dat`、th08 从 `th08.dat` 解包；39 份全部成功，汇总落 `_dump_report_a_gen.txt`。

**生成脚本（B/C 代）**：`docs/logger/tools/dump_ecl_decl.ps1`（Windows / PowerShell）。
按 `[th<版本>]` 前缀定位唯一含 `data` 的拆包目录，每作 `glob data\*.ecl` 全量 dump，逐文件按「退出码 / 产物非空 / 含 `^void`」判成败，结尾落 `_dump_report.txt`。
> 该脚本只覆盖 th10–th18 的拆包目录形态；A 代需先 `thdat` 解 `.dat`，目前是手工流水（上一条），未并入脚本。

**来源链（完整）**：

```text
B/C 代：各作安装  D:\C P\C2H2\[thNN…]\data\*.ecl
   │  ① 拆包（thdat，已拆好）
   ▼
拆包目录  <游戏根>\[thNN…]\data\*.ecl
   │  ② thecl -d <ver> [-m ECL_MAP_<ver>.txt]   （THTK thtk12；脚本 dump_ecl_decl.ps1）
   ▼
Windows 产物  D:\workshop\reasm\exes\ecl_dump\thXX\*.decl

A 代：游戏根  [th06]\紅魔郷ST.DAT / [th07…]\th07.dat / [th08…]\th08.dat
   │  ① thdat -x6/7/8 <归档> ecldata*.ecl
   ▼
原始 ECL  D:\workshop\reasm\exes\ecl\th06|th07|th08\*.ecl
   │  ② thecl -d6/7/8 [-m ECL_MAP_08.txt]
   ▼
反编译    D:\workshop\reasm\exes\ecl_dump\th06|th07|th08\*.decl

   │  ③ （B/C 代）连同原始 .ecl 一并复制到本机
   ▼
本机素材  ../exes/ecl/thXX/*.ecl   +   ../exes/ecl_dump/thXX/*.decl
```

**dump 汇总 — B/C 代**（`../exes/ecl_dump/_dump_report.txt`，2026-10-14 20:55:56）：

```text
th10   期望   8  实际   8  失败 0   <- [th10] 东方风神录
th11   期望  20  实际  20  失败 0   <- [th11] 东方地灵殿 (汉化版+日文版)
th12   期望   9  实际   9  失败 0   <- [th12] 东方星莲船 (汉化版+日文版)
th13   期望  21  实际  21  失败 0   <- [th13] 东方神灵庙
th14   期望  24  实际  24  失败 0   <- [th14] 东方辉针城
th15   期望  23  实际  23  失败 0   <- [th15] 东方绀珠传
th16   期望  22  实际  22  失败 0   <- [th16] 东方天空璋
th17   期望  22  实际  22  失败 0   <- [th17] 东方鬼形兽
th18   期望  22  实际  22  失败 0   <- [th18]
合计：期望 171  成功 171  失败 0
```

**dump 汇总 — A 代**（`../exes/ecl_dump/_dump_report_a_gen.txt`，2026-10-21）：

```text
th06   期望   7  实际   7  失败 0   <- [th06]  （紅魔郷ST.DAT；无 eclmap）
th07   期望   8  实际   8  失败 0   <- [th07] 东方妖妖梦 (日文版)（无 eclmap）
th08   期望  24  实际  24  失败 0   <- [th08] 东方永夜抄 (日文版)（ECL_MAP_08.txt）
合计：期望 39  成功 39  失败 0
```

**已知限制**：
- thtk12 的 th18 格式表不完整（`st06bs.ecl` 有一条 `id 94` 解不出），只影响该**一条指令**，sub 名与 `@call` 结构不受影响。
- A 代 th06 / th07 **无 eclmap** ⇒ decl 中指令为 `ins_NNN(...)`、sub 名为合成的 `SubN`；th08 有 map，指令已具名。**三作的 sub 名都不是引擎内真实名字**（A 代 ECL 本身不存名字），离线只能得到「sub 索引 / 偏移」。th08 解出的 `.ecl` 比归档列表 Size 少 4 B（见 §3.1 注），不影响 `thecl` 解析。

---

## 4. th15 的 Ghidra 反编译工程（`../th15decode/`）

th15 作为**参照作**，另有一条**独立素材来源**（相对 `thprac/` 仓库根为 `../th15decode/`）：

| 文件 | 内容 |
| --- | --- |
| `th15.exe_decompiled.c` | th15.exe 的 **Ghidra 全量反编译 C** |
| `th15deco.gpr` / `th15deco.rep` | Ghidra 工程文件 |
| `data/*.ecl`（含 `st01.ecl`） | th15 拆包 ECL（与 `../exes/ecl/th15/` 同源） |
| `ecl-system.md` / `death.md` / `do.md` / `boss-phase-hook.md` | th15 逆向笔记 |

- `docs/th15/th15_ecl_re_report.md` 头部引用的 `../th15decode/th15.exe_decompiled.c`、`../th15decode/data/st01.ecl` 即来自此处。
- `../th15decode/data/st01.ecl` 与 `../exes/ecl/th15/st01.ecl` 为**同一份脚本的两处副本**。

---

## 5. 复核方式

- **exe 同一性**：比对 PE `TimeDateStamp` / `textSize` 与 `thprac/src/thprac/thprac_launcher_games_def.h` 对应条目（th07/th08 另有 sha256 与安装逐字节比对）。
- **decl 完整性**：`_dump_report.txt` 为 171/171 成功、0 失败；单文件判别式 = 存在、非空、含至少一个 `^void`。
- **ecl ↔ decl 对账**：`ecl/thXX` 与 `ecl_dump/thXX` 文件基名一一对应，数量相同（上表）。

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-15 | 建档。登记 `../exes` 下的 exe / ecl / decl 与 `../th15decode/` 的来源、清单、生成链与复核方式；作为各篇 `thXX_ecl_re_report.md` 的素材来源统一出处。 |
| 2026-10-16 | **补记 A 代（th06–th08）素材缺口**：§0/§2 注明 th06 **无 exe**（本体在 `D:\C P\C2H2\[th06]\東方紅魔郷.exe`，指纹已实测一致、尚未入库）、三作**无 eclmap**（THTK `MAP/` 无 `ECL_MAP_06/07.txt`）、`.ecl` 未从 `.dat` 解出 ⇒ **A 代无法离线枚举具名 sub**。A 代已 E0 建档（`docs/th06\|th07\|th08/thXX_ecl_re_requests.md`）。 |
| 2026-10-17 | **§3.1 补记「实际被引用」情况**：拆包目录里的 `.ecl` 不一定被该关 `ecli{}` 引用（th13 `st06mbs.ecl` 未用；th10 无独立 boss 文件、th12 仅 `stage07boss.ecl`）。离线枚举以 `ecli{}` 为准。 |

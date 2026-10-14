# ECL 白名单 CSV 生成指南（离线作业手册）

> 上位文档：`docs/req_logger.md`（需求）、`docs/logger/logger_re_disasm_overview.md`（总括）。
> 本文档面向**执行者（下一个 AI / 人）**：从各作原始 `.ecl` 生成监控用的**白名单 CSV**。
> 本文档只描述做法与工具，**不含 CSV 数据本身**。
> 角色区分：**段落划分与段落名一律以 thprac 为准**（不是从 ECL 推的）；ECL 只负责回答「这个段落运行时落在哪个 sub」。

---

## 0. 目标产物

一个 CSV，UTF-8（建议带 BOM，方便 Excel 直接打开），四列：

| 列 | 内容 | 来源 |
| --- | --- | --- |
| `段落入口函数` | ECL sub 名（如 `MainSub00`、`MainBossSpell`） | ECL `.decl` |
| `所属游戏` | `th10` … `th18` | 目录 |
| `所属关卡` | `1` … `7`（th10–18 均为 1 起） | 文件名 |
| `记录名` | **thprac 练习菜单里显示的段落名**（如 `前半 #1`、`道中一非`、`弹符「射鹰」`） | thprac 源码 |

约定（来自 overview §1.2 / §2.1）：

- 一行 =（游戏，关卡，段落入口函数，记录名）。
- **同一 `记录名` 允许出现多行**：一个 checkpoint 可以对应多个 sub（「监控以 sub 级读取、以段落级呈现」）。
- 本迭代（监控）只用前三列；`记录名` 供后续日志迭代。

---

## 1. 核心认知：这件事要两条腿

**段落划分和段落名不是 ECL 自带的**，是 thprac 在源码里定义的；ECL 只能回答「跑到哪个 sub」。所以必须把两路输入 join 起来：

- **A 路（权威）thprac 侧**：段落有几个、叫什么名。
- **B 路（素材）ECL 侧**：每个段落实际落到哪个 sub。

只做 A 路 → 得到段落清单但没有函数名；只做 B 路 → 得到一堆 sub 名但不知道哪些是段落入口、该怎么分组。**先把 A 路抄清楚，再用 B 路落名。**

---

## 2. 原料与工具（本机实况，已核实）

### 2.1 游戏侧 `.ecl`

每作在 `D:\C P\C2H2\` 下是**直接子目录**，其中**唯一含 `data` 子目录**的那个就是拆包目录，`.ecl` 在 `<作目录>\data\`：

| 作 | 目录（`D:\C P\C2H2\` 下） | ecl 命名形态 | 个数 |
| --- | --- | --- | --- |
| th10 | `[th10] 东方风神录` | `stageNN.ecl` | 8 |
| th11 | `[th11] 东方地灵殿 (汉化版+日文版)` | `stageNN.ecl` / `stageNNboss.ecl` / `stageNNmboss.ecl` / `stage4c0X{a,b,c}.ecl` | 20 |
| th12 | `[th12] 东方星莲船 (汉化版+日文版)` | `stageNN.ecl` / `stage07boss.ecl` | 9 |
| th13 | `[th13] 东方神灵庙` | `stNN.ecl` / `stNNbs.ecl` / `stNNmbs.ecl` | 21 |
| th14 | `[th14] 东方辉针城` | 同上（+ `st04bs2` / `st04mbs2`） | 24 |
| th15 | `[th15] 东方绀珠传` | 同上（+ `st01mbs2` / `st07bs2`） | 23 |
| th16 | `[th16] 东方天空璋` | 同上（+ `st07mbs2`） | 22 |
| th17 | `[th17] 东方鬼形兽` | 同上 | 22 |
| th18 | `[th18]` | 同上 | 22 |

> 一律 `glob *.ecl`，**不要硬编码文件名**。每作都有 `default.ecl`（共享 sub，如各 `Girl*` / `Phantom*`）。

**关键：一关 ≠ 一个 `.ecl`。** decl 头部的 `ecli { … }` 会列出该关引用的所有 ECL：

```
anim { "enemy.anm"; "st01enm.anm"; }
ecli { "default.ecl"; "st01bs.ecl"; "st01mbs.ecl"; "st01mbs2.ecl"; }
```

Boss / 道中Boss 的段落在 `*bs` / `*mbs` 里，**必须一起 dump**，否则 Boss 段落全是空的。

### 2.2 工具

位于 `D:\C P\C2H2\拆包\THTK\thtk\thtk12\`（thtk **release 12**，预编译，无源码）：

| 工具 | 用途 |
| --- | --- |
| `thecl.exe` | `.ecl` ⇄ `.decl`（反编译 / 编译 / 生成头） |
| `thdat.exe` | 解包 `thXX.dat`（本任务一般用不上，`data/` 已拆好） |
| `thanm.exe` / `thmsg.exe` / `thstd.exe` | 无关 |

```
Usage: thecl.exe [-Vrs] [[-c | -h | -d] VERSION] [-m ECLMAP]... [INPUT [OUTPUT]]
   -d  dump ECL file      ← 本任务用这个
   -m  use map file for translating mnemonics
VERSION can be: 6, 7, 8, 9, 95, 10, 103(UB), 11, 12, 125, 128, 13, 14, 143, 15, 16, 165, 17 or 18
```

→ **th10–th18 的 VERSION 全覆盖**（直接用作品号，即 `10 … 18`）。

eclmap（把 `ins_569(...)` 翻成人名）在 `D:\C P\C2H2\拆包\THTK\MAP\ECL_MAP_<XX>.txt`，
**只有 08/10/108/11/12/128/13/14/143/15/16/165/17，没有 18** ⇒ th18 不加 `-m`，dump 出来是 `ins_*` 形态。
**这不影响本任务**：我们要的是 sub 名和 `call` 结构，两者与 eclmap 无关。

### 2.3 已有的 decl（零散、残缺，**必须统一重跑**）

各作 `D:\C P\C2H2\<作>\data\ECL\` 里散着一批历史 `.decl`，但**不完整、部分是坏的、且不统一**：

| 作 | 现存 decl | 状态 |
| --- | --- | --- |
| th11 | `stage03` / `stage07boss` / `stage4c00a` / `stage4c01a` | 仅 4/20；`stage03.decl` 是 **0 字节** |
| th12 | `stage05` / `stage05 - 副本` / `stage06` | 仅 3/9 |
| th14 | `st01`–`st06` + `st03bs`（7 个） | 缺大量 `*bs`/`*mbs` |
| th15 | `st01`–`st06` 等（10 个） | 缺大量 `*bs`/`*mbs`；`st07bs.decl` 是 **0 字节** |
| th18 | `st01*`–`st07bs`（15 个） | 缺 `stNN.decl` 主档；个别档（如 `st03bs.decl`）字符串被**重复编码** |
| th10 / th13 / th16 / th17 | **无** | — |

还有两点让它们不能直接拼用：

- 这批历史产物**大多没加 `-m`**（是 `ins_*` 裸指令），与 §3 的产出形态不一致。
- **一关 ≠ 一个 `.ecl`**，缺 `*bs`/`*mbs` 就等于 Boss 段落全是空的。

结论：一律按 §3 **统一重跑到 `D:\workshop\reasm\exes\ecl_dump\`**，不要拿旧档东拼西凑。

---

## 3. Step 1：批量 dump `.ecl` → `.decl`

产物落在**仓库外** `D:\workshop\reasm\exes\ecl_dump\thXX\`（**不要提交进 git**）。

**直接跑仓库里的脚本**：`docs/logger/tools/dump_ecl_decl.ps1`（Windows / PowerShell 5.1）。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File docs\logger\tools\dump_ecl_decl.ps1
```

脚本在做什么（要手改或排查时才看）：

- 游戏目录**不硬编码**：按 `[th<版本>]` 前缀在 `D:\C P\C2H2` 下找，取其中**唯一含 `data` 子目录**的那个
  （例：有三个 `[th18]...`，只有 `[th18]` 带 `data`）。
- 每作 `glob data\*.ecl` **全量**（含 `default.ecl` 与各 `*bs`/`*mbs`），逐个
  `thecl -d <版本> [-m <map>] <in> <out>`。
- th10–17 带 `-m`；**th18 无 map**，自动省略。
- 逐文件判成败（退出码 / 产物非空 / 含 `^void`），结尾打印每作「期望 / 实际 / 失败」汇总
  并落 `_dump_report.txt`；有失败则退出码 `1`。

注意：

- 路径含空格（`C P`、中文目录名），**必须引号**（脚本内已用 `-LiteralPath` 与引号处理）。
- `-d` 的第三、四个参数是 input/output；不写 output 会打到 stdout。
- **thtk12 的 th18 格式表不完整**：`st06bs.ecl` 里有一条 `id 94` 解不出
  （stderr 报 `id 94 was not found in the format table`）。只影响那**一条指令**，
  sub 名与 `@call` 结构不受影响；历史档亦然，属已知限制，不必修。

**判别式（务必先做）**：dump 一份 th15 `st01.ecl`，`grep '^void '` 应看到 `main` / `MainFront` / `MainBoss` / `MainSub00…` 等（见 §4 样例）。看不到 `main` 就是版本号或文件选错了。

---

## 4. `.decl` 长什么样（th15 st01，节选）

```
anim { "enemy.anm"; "st01enm.anm"; }
ecli { "default.ecl"; "st01bs.ecl"; "st01mbs.ecl"; "st01mbs2.ecl"; }

void MainBossSpell();                    ← 只有声明：定义在 st01bs.ecl
void MainMBossSpell();

void GirlA01()                           ← 当前 .ecl 里定义的 sub
{
    ins_569(1);
    ...
    goto GirlA01_276 @ 0;
GirlA01_256:
    ins_23(1000);
GirlA01_276:
    if (1) goto GirlA01_256 @ 0;
    return;
}

void main()                              ← 主线容器
{
    ...
main_440:
    ins_22(1, "MainMBossDebug");
    ...
+60: //60
    ins_0();
    @MainFront();                        ← call
    ...
    @MainLatter();
    ...
    @MainLatter2();
    ...
    @MainBoss();
    goto main_1264 @ 60;
main_1244:
    ins_23(1000);
main_1264:
    if (1) goto main_1244 @ 60;
    ins_1();
}
```

要点：

1. **sub 名原样保留**（`void MainFront()`），这就是 `段落入口函数` 列的值。
2. `@X()` = 调用 sub；`@X() async` = `spawn`（异步，另起线程，**不进入主线调用栈**）。
3. `main_NNN` / `GirlA01_NNN` 这类 label 的**数字是段内字节偏移**，不是绝对文件偏移（可用 `goto` 恒为 24 字节验证：th15 st01 里 `main_352 → main_376` 差 24）。绝对偏移 = 该 sub 的文件起始偏移 + label 数字。
4. decl 的 **sub 顺序是 sub 表顺序（≈名字序），不是文件顺序**。

---

## 5. Step 2：抄 thprac 的段落划分与段落名（A 路，权威）

### 5.1 机制总览

th10–th18（**以及 th20**）走的是各作 `THGuiPrac` 里的一套控件，**不是** `StageWarpsRender/stage_warps_t` 那套
（后者只有 `th185` 在用，别被 `thprac_games.cpp:1487` 带偏）：

```
mWarp (6 选 1) ─┬─ 1 TH_STAGE_PORTION  道中/章节  → mChapter 滑块
                ├─ 2 TH_MID_STAGE      道中Boss    → mSection 组合框
                ├─ 3 TH_END_STAGE      关底Boss    → mSection 组合框
                ├─ 4 TH_NONSPELL       非符        → mSection 组合框
                └─ 5 TH_SPELL          符卡        → mSection 组合框
```

`mWarp` 的选项名与取值来自 glossary（`thprac_games_def.json:2073`）：

```
"TH_WARP_SELECT": [ "TH_NONE", "TH_STAGE_PORTION", "TH_MID_STAGE", "TH_END_STAGE", "TH_NONSPELL", "TH_SPELL" ]
```

段落 id 只有两种形态（`CalcSection()`，每作一份）：

| `mWarp` | section 计算 | 是否 `th_sections_t` |
| --- | --- | --- |
| 1（道中章节） | `10000 + (stage+1)*100 + chapter` | ❌ 合成整数 |
| 2 / 3 | `th_sections_cba[stage][mWarp-2][mSection]` | ✅ |
| 4 / 5 | `th_sections_cbt[stage][mWarp-4][mSection]` | ✅ |

### 5.2 段落名从哪来（两类，**不要混**）

**(a) 道中章节**：名字是 glossary 词条，不是 `th_sections_str`：

```
thprac_games_def.json:1885-1887
  "TH_STAGE_PORTION_1": [ "前半 #%d", "First Half #%d", "前半 #%d" ],
  "TH_STAGE_PORTION_2": [ "后半 #%d", "Second Half #%d", "後半 #%d" ],
  "TH_STAGE_PORTION_N": ...
```

各作在 UI 里 `sprintf_s` 出来（th15 在 `thprac_th15.cpp:366-380`）。**章节总数**来自每作的
`mChapterSetup[stage][2] = { 前半数, 后半数 }`，例：

```
thprac_th15.cpp:432-440
int mChapterSetup[7][2] {
    { 2, 4 }, { 3, 3 }, { 3, 3 }, { 3, 3 }, { 4, 6 }, { 4, 0 }, { 5, 4 },
};
```

第 1 关 `{2,4}` ⇒ 滑块 1..6 ⇒ `前半 #1, 前半 #2, 后半 #1 … 后半 #4`。
`章节数 = 前半数 + 后半数`；若后半数为 0 则全部用 `TH_STAGE_PORTION_N`。

**(b) Boss / 非符 / 符卡**：名字 = `th_sections_str[语言][难度][th_sections_t 枚举值]`：

- 语言维 3 项：`0=zh-CN, 1=en-US, 2=ja-JP`（`thprac_gui_locale.h` 的 `locale_t`）。
  **本任务取 `0`（zh-CN）**，与练习菜单中文界面对齐。
- 难度维 4 项：`0=Easy, 1=Normal, 2=Hard, 3=Lunatic`。**建议先只取 `1`（Normal）**；同一段落在不同难度名字可能不同（硬/狂难度有独立符卡名，见下例）。
- 段落清单（顺序 = 菜单里组合框的顺序）：

  ```
  th_sections_cba[stage][0] = 道中Boss 的各段落
  th_sections_cba[stage][1] = 关底Boss 的各段落
  th_sections_cbt[stage][0] = 非符   的各段落
  th_sections_cbt[stage][1] = 符卡   的各段落
  ```

  数组定长，**尾随 0（`A0000ERROR`）是终止符**；空名字项在 UI 里被跳过。

**数据表位置**（`thprac_locale_def.cpp`，**autogenerated，只读、别改**）：

| 作 | `th_sections_str[3][4][N]` | `th_sections_cba` | `th_sections_cbt` | `th_sections_t` 枚举（`thprac_locale_def.h`） |
| --- | --- | --- | --- | --- |
| th10 | `:11653` | `:12586` | `:12698` | `:2464` 区 |
| th11 | `:13188` | `:15005` | `:15222` | `:2654` 区 |
| th12 | `:15484` | `:16456` | `:16572` | `:2751` 区 |
| th13 | `:19403` | `:20388` | `:20504` | `:3041` 区 |
| th14 | `:20632` | `:21825` | `:21970` | `:3147` 区 |
| th15 | `:22174` | `:23185` | `:23304` | `:3251` 区（`th_sections_t` 本体 `:3173-3249`） |
| th16 | `:23470` | `:24455` | `:24571` | `:3351` 区 |
| th17 | `:24711` | `:25618` | `:25729` | `:3439` 区 |
| th18 | `:25891` | `:26785` | `:26895` | `:3526` 区 |

权威源是 `thprac/src/thprac/thprac_games_def.json` 里每作的 `"sections"` 块
（th10 `:4527`、th11 `:5180`、th12 `:6608`、th13 `:7610`、th14 `:8141`、th15 `:8793`、
th16 `:9325`、th17 `:9891`、th18 `:10520`）。JSON 里每个条目既有名字也有 `appearance: [stage, 组, 序号]`
坐标，想省事可以直接从 JSON 抽，不必读 `.cpp`：

```json
"TH15_ST1_MID3": {
  "bgm": 0, "spell": 1, "appearance": [ 1, 1, 3 ],
  "!HL": [ "凶弹「高速撞击」", "Evil Bullet \"Speed Strike\"", "凶弾「スピードストライク」" ]
},
"TH15_ST1_BOSS2": {
  "spell": 1, "appearance": [ 1, 2, 2 ],
  "!ENH": [ "弹符「射鹰」", ... ],
  "!L":  [ "弹符「鹰已击中」", ... ]
}
```

`"!XXXX"` 后缀字母 ∈ `{E,N,H,L}` = 该 value 适用的难度；value 为字符串 = 引用 `common` 里的 glossary 键
（如 `TH_MID_NS1` / `TH_END_NS1`），为 3 元数组 = `[zh, en, ja]`。

> 生成器 `thprac_locale_def.{h,cpp}` 在**外部仓库** `touhouworldcup/thprac_utils`，本机没有、也**不能重新生成**（overview §9 风险 6）。读没问题。

### 5.3 每作的关键函数行号（抄 A 路时直接跳过去）

| 作 | `CalcSection()` | `mChapterSetup` | `THStageWarp` | `THPatch` | `THSectionPatch()` |
| --- | --- | --- | --- | --- | --- |
| th10 | `thprac_th10.cpp:282` | `:385` | `:1380` | `:1628` | `:2159` |
| th11 | `thprac_th11.cpp:345` | `:462` | `:1063` | `:1240` | `:2099` |
| th12 | `thprac_th12.cpp:335` | `:444` | `:1019` | `:1200` | `:1802` |
| th13 | `thprac_th13.cpp:275` | `:405` | `:948` | `:1160` | `:1805` |
| th14 | `thprac_th14.cpp:265` | `:427` | `:1664` | `:1855` | `:2678` |
| th15 | `thprac_th15.cpp:314` | `:432` | `:1025` | `:1275` | `:2180` |
| th16 | `thprac_th16.cpp:263` | `:369` | `:1163` | `:1345` | `:2344` |
| th17 | `thprac_th17.cpp:383` | `:604` | `:1369` | `:1547` | `:2417` |
| th18 | `thprac_th18.cpp:584` | `:724` | `:2430` | `:2690` | `:3572` |

---

## 6. Step 3：把段落落到 sub 名（B 路）

入口是 `THSectionPatch()`（th15 `:2180`）：

```cpp
auto section = thPracParam.section;
if (section >= 10000 && section < 20000) {          // ← 道中章节
    int stage     = (section - 10000) / 100;
    int portionId = (section - 10000) % 100;
    THStageWarp(ecl, stage, portionId);
} else {                                            // ← Boss / 非符 / 符卡
    THPatch(ecl, (th_sections_t)section);
}
```

**两条路的落点本质上都是「往 ECL 里写 `goto`(0x0C) + 跳章节」**：

- **道中章节**：`THStageWarp(ecl, stage, portion)` 的每个 `case portion:` 里是一两条
  `ECLJump(ecl, start, dest, at_frame, ecl_time)`，`start`/`dest` 是**该 `.ecl` 的绝对文件偏移**：

  ```cpp
  // thprac_th15.cpp:1018-1053
  void ECLJump(ECLHelper& ecl, unsigned int start, unsigned int dest, int at_frame, int ecl_time = 0) {
      ecl.SetPos(start);
      ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
  }
  ...
  case 2: ECLJump(ecl, 0x7248, 0x7494, 60, 90);   // 前半 #2
          ECLJump(ecl, 0x3f4c, 0x3fa8, 0, 0);
          break;
  ```

  `portionId` **就是 `mChapter` 值**，所以「章节名 ↔ portionId」一一对应，好对。

- **Boss / 非符 / 符卡**：`THPatch` 的 `case TH15_STx_YYY:` 里通常是
  `ECLJump(ecl, …, <BOSS_CREATE_CALL 常量>, 60)` + `ECLSkipChapter(n)`，再对 `stXXbs.ecl` / `stXXmbs.ecl`
  做 `ecl.SetFile(k)` 后的一串定点写入（改 boss 位置/血量/符卡名/相位）：

  ```cpp
  // thprac_th15.cpp:2123-2125
  case THPrac::TH15::TH15_ST7_END_S10:
      ECLJump(ecl, st7Start, st7BossCreateCall, 60);
      ECLSkipChapter(2);
  ```

### 6.1 推荐做法：**结构对齐法**（不碰字节偏移）

th15 已完成的那套（`feat/plogth15` 分支的 `TH15_MB_MID_MAP`，注释：「章节划分由 THStageWarp 各 portion 的
ECLJump 实际落点反推」）可以照搬：

1. 打开该关的 `stXX.decl`，读 `main` 里 **`@MainFront(); / @MainLatter(); / @MainLatter2(); / @MainBoss();`
   的先后顺序**，以及每个容器里 **`call MainSubNN` 的顺序**。
2. 章节（道中）段落按顺序对号。th15 st1 的**实测骨架**（`docs/th15/th15_ecl_re_report.md:189` 的
   离线解码结果，`main` 从 tag 60 起）：

   ```
   main ─ call MainFront ─┬─ call MainSub00
                          ├─ (spawn …)        ← 异步，不驻留主线栈
                          ├─ call MainSub01
                          ├─ call MainSub02
                          └─ call MainSub03
        ─ call MainLatter ─┬─ call MainSub04
                           └─ call MainSub05
        ─ call MainLatter2
        ─ call MainBoss
   ```

   而 `mChapterSetup[0] = {2, 4}` ⇒ **该关正好 6 章**，与 `MainFront` + `MainLatter` 下的 6 个
   `MainSubNN` 数量吻合；`feat/plogth15` 分支的 `TH15_MB_MID_MAP` 已按此建表
   （`"1-MainSub00"` → `前半 #1`、`"1-MainSub01"` → `前半 #2`，其余同理顺推），
   注释写着「章节划分由 `THStageWarp` 各 portion 的 `ECLJump` 实际落点反推」。

   对号时的两个**硬证据**（都在 `THStageWarp` 里，见 `thprac_th15.cpp:1025-1053`）：

   - `ECLJump(ecl, 0x7248, 0x7494, 60, 90)`（case 2）与 `0x7544`（case 3/4）、`0x75f8`（case 5/6）
     是同一个 `main` 内**递增**的落点 ⇒ 章节顺序 = 落点顺序。
   - `ECLSetChapter(2)`（case 3 = `后半 #1`）、`ECLSetChapter(5)`（case 5 = `后半 #3`）直接告诉你
     「这一章之前有几章要跳过」，是与 `MainSubNN` 序号对账的现成锚点。

   > ⚠️ 上面这张「章节 ↔ MainSubNN」的完整对应表**目前只在 th15 st1 上验证过**。每换一关/一作，
   > 都要重新按落点推一遍；不能把 th15 st1 的对应关系套到别处。
3. Boss 段落：`main` 里 `@MainBoss();` / `@MainMBoss();` 是闸口；具体符卡再进 `stXXbs.decl` 的
   `MainBoss()` / `MainBossSpell()` 链，按 `th_sections_cba/cbt` 的条目顺序对号
   （道中Boss 段 → `MainMBoss` 链，关底Boss/非符/符卡 → `MainBoss` 链）。

> 该法的前提假设是「thprac 菜单的段落顺序 == ECL 里 `call MainSubNN` 的顺序」，th15 已用真机核对成立；
> 换作时**每个作都要抽 1–2 关按 §9 验证**，不能默认成立。

### 6.2 交叉校验：把绝对偏移落回 decl（可选）

`ECLJump` 的 `start`/`dest` 是 `.ecl` 的绝对文件偏移，要和 decl 对上，需要「偏移 → sub + 段内偏移」。
本机已有两条成熟路径，**别再自创公式**：

- **Ghidra 原始数据法（th14/th15 用过）**：把 `.ecl` 当 raw binary 导进 Ghidra，直接按文件偏移读那几条指令。
  工程在独立工程 `D:\workshop\wind\`（**不是本仓库**）：th15 为 `th15decode\th15d.gpr`；th14 的 `.gpr` 已不在
  （`th14decode\` 只剩 `ghidra_out\`）。
  `docs/th15/th15_ecl_re_report.md:189` 记录过「文件名偏移与 `ECLJump` 常量逐一吻合」。
- **读 sub 表法**：th14+ 的 sub 表链见 overview §2 模板（`res+0x8C`，项 8 B：`+0` 名字指针 / `+4` 起始）；
  th15 已确认项 `+0` 是 `char*` 名字（`docs/th15/th15_ecl_re_report.md` Q6 已答）。
  拿到每个 sub 的起始偏移后，`dest` 落在哪个 sub 里就一目了然。
  **th10–13 的 sub 表形态尚未确认**（overview §5.1 标 ❌），别照搬偏移。

在 `.ecl` 里肉眼判 opcode：`call` = `0x0B`、`goto` = `0x0C`，且 th10+ 的指令是
`[u16 time][u16 opcode][u16 size][u16 rank_mask][params…]`（`goto` 的 size 恒为 `0x18`）。

---

## 7. Step 4：产出 CSV

1. 按作、按关分组，每关一次输出。
2. 行序建议与菜单一致：道中章节（1..N）→ 道中Boss → 关底Boss → 非符 → 符卡。
3. **对账**：CSV 里每关的行数（去重后的 `记录名` 数）应等于该关菜单里的可选段落数：

   ```
   章节数 = sum(mChapterSetup[stage])
   道中Boss数 = len(非空 th_sections_cba[stage][0])
   关底Boss数 = len(非空 th_sections_cba[stage][1])
   非符数     = len(非空 th_sections_cbt[stage][0])
   符卡数     = len(非空 th_sections_cbt[stage][1])
   ```
   对不上就是漏了 `*bs` / `*mbs` 的 dump，或章节对号错位。
4. 建议同时保留一份「机器可读的中间产物」（每关一个 `(记录名, sub 名列表)` 的 TSV），CSV 由它渲染，
   便于返工。

---

## 8. Step 5：真机抽样验证（不可省）

overview §7.5 与 `docs/th15/th15_ecl_re_report.md` 的做法：真机挂读 sub 名的探针，对 (作/关/段落) 施放
warp，把读到的 sub 与 CSV 对照。

- th15 的分支 `feat/plogth15`（`thprac/src/thprac/thprac_th15_missbomb.{h,cpp}`）里已有 `GetEclSubName()`，
  可直接拿来改造。
- 至少抽：每作 1 关 × （道中第 1 段、道中最后 1 段、关底第 1 符、关底最后 1 符）；分支关（th11 st4、
  th14 st4、th16 st5）必须**每条路线各抽一次**。
- 未验证的作/关，在 CSV 或文档里标注「未验证」。

---

## 9. 已知坑

1. **th18 无 eclmap**：`-m` 省略，dump 出 `ins_*`，不影响 sub 名与结构。
2. **`.decl` 是派生品**：换游戏版本（Steam / 汉化 / 日文）要重 dump。`.ecl` 的**结构**通常跨版本一致，
   所以 sub 名一般可跨版本复用，但**别拿偏移跨版本**。
3. **`thprac_locale_def.{h,cpp}` 不能重生成**（外部 devtools），只读。
4. **分支关**：th11 st4（`mSpellCategory` 把 cba/cbt 的 stage 维偏移，`thprac_th11.cpp:345-371`）、
   th14 st4（`st_offset` / `mFakeType` / `mShotType`，`thprac_th14.cpp:339-394`）、th16 st5。
   这些作的「关卡 ↔ 表行」不是恒等映射，抄表时必须连偏移逻辑一起抄。
5. **`TH_CHAPTER_*` 是死词条**（`thprac_games_def.json:1932-1937`，全仓库无 cpp 引用），别拿它当段落名。
   当前章节名走 `TH_STAGE_PORTION_1/2/N`。
6. **th15 有运行期改名彩蛋**：`thprac_th15.cpp:356-363` 会把 `TH15_ST5_MID1` 的名字改成
   `"it's morbin time"`。以**运行时**为准时要注意这类改写。
7. **`th185`/`th19`/`th20` 不在本任务范围**；th185 走的是另一套（`stage_warps_t`），别混。
8. 本任务**只关心 sub 名**，不关心 RVA，所以 overview §9 风险 8（地区/版本变体 RVA 不同）对本任务影响很小。

---

## 10. 提示词模板（喂给执行 AI）

```
任务：为 <thXX> 生成 ECL 段落监控白名单 CSV 的数据行。

输入（已在手）：
1) 第 <N> 关的 decl：<路径>/stXX.decl、stXXbs.decl、stXXmbs.decl
2) thprac 侧段落定义：
   - mChapterSetup：thprac/src/thprac/thprac_thXX.cpp:<行号>
   - 道中章节名：glossary TH_STAGE_PORTION_1/2（thprac_games_def.json:1885）
   - Boss/非符/符卡段落清单与名字：thprac/src/thprac/thprac_locale_def.cpp
     th_sections_cba / th_sections_cbt / th_sections_str（zh-CN，难度 Normal）
   - 落点：同文件 THStageWarp / THPatch / THSectionPatch

要求：
- 记录名严格照搬上述 thprac 数据，不要自拟、不要翻译、不要改写。
- 段落入口函数取 decl 里的 sub 名，原样（大小写敏感）。
- 段落顺序与练习菜单一致：道中章节 → 道中Boss → 关底Boss → 非符 → 符卡。
- 一个段落对应多个 sub 时，输出多行、记录名相同。
- 输出为 CSV：段落入口函数,所属游戏,所属关卡,记录名
- 不确定的行**不要猜**，单独列在末尾并注明依据缺口。
```

---

## 11. 参考

- `docs/req_logger.md` —— 需求原文（CSV 四列的出处）
- `docs/logger/logger_re_disasm_overview.md` —— 总括；§1.2 判定流程、§2 读取模板、§5.1 各作锚点
- `docs/th15/th15_ecl_re_report.md` —— **th15 是本任务的参照作**：Q1/Q3/Q6/Q8/Q9 已答，
  §3.2 有 st01 的实测调用链，§2.2 有 `ECLJump` 的字节展开
- `docs/th10…th18/<作>_ecl_re_requests.md` / `<作>_ecl_re_report.md` —— 逐作结论与验证清单

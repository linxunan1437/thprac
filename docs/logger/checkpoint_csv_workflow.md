# checkpoint CSV 生成工作流程（thprac「章节」+「boss 段落」）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（ECL 段落监控总括）、`docs/req_logger.md`（需求）。
> 用途：为某作的**监控白名单 CSV** 生成骨架。每行 = 一个 checkpoint（章节 或 boss 段落）。
> 骨架阶段只填 `记录名 / 所属关卡 / 所属游戏`；`段落入口函数` 的回填流程见 **§4.5**（口径：**路过式 checkpoint / latch**）。
> 参考成品：`docs/logger/th15_checkpoints.csv`。
> 地址/术语口径见总括；本文只讲「怎么把 CSV 造出来」。

---

## 0. 一句话

**一条 CSV = 「章节」行（thprac warp 模式 1）+「boss 段落」行（th_sections_cba，warp 模式 2/3），共同构成 checkpoint 白名单。**

---

## 1. 两条轴（都必须有，缺一不可）

| 轴 | 来源 | 粒度 | 记录名样式 |
| --- | --- | --- | --- |
| **章节轴**（道中分段） | `thprac_thXX.cpp` 的 `mChapterSetup` + `THStageWarp` | warp 模式 1（`TH_CHAPTER` 滑块） | `STn 前半 #k` / `STn 后半 #k` / `STn #k` |
| **boss 轴** | `thprac_locale_def.cpp` 的 `th_sections_cba[N][2][19]` | `[0]`=道中Boss、`[1]`=关底Boss（warp 模式 2/3） | `STn MIDm` / `STn BOSSm` / `STn END_NSm` / `STn END_Sm` |

> ⚠️ 两个坑：
> 1. **章节轴天然不含 boss**——thprac 把 boss 放在另一条轴。只做章节 = 没有 boss 记录。
> 2. **别用 `enum th_sections_t` 全量**（那是 cba ∪ cbt 的去重全集，混入了 非符/符卡 再切分）；**boss 行只取 `th_sections_cba`**（道中Boss + 关底Boss）。
> - 游戏内 ECL 的 `setChapter(N)` 又是第三层含义，也不用。

---

## 2. 输入 / 输出

**输入**
- `thprac/src/thprac/thprac_thXX.cpp`（`mChapterSetup`、`THStageWarp`、`SectionWidget`）
- `thprac/src/thprac/thprac_locale_def.cpp` / `.h`（`namespace THxx` 的 `enum th_sections_t` 与 `th_sections_cba`）
- `thprac/src/thprac/thprac_games_def.json`（文案：`TH_STAGE_PORTION_1="前半 #%d"`、`_2="后半 #%d"`、`_N="#%d"`）

**输出**
- `docs/logger/thXX_checkpoints.csv`，UTF-8，`1 行表头 + N 行数据`。
- 列顺序：`段落入口函数,所属游戏,所属关卡,记录名`
- 取值：
  - `段落入口函数` = 空（**骨架阶段**；回填流程见 §4.5）
  - `所属游戏` = `thXX`
  - `所属关卡` = `ST1` … `ST7`（第 7 关 thprac UI 显示为 **Extra**，本 CSV 统一写 `ST7`）。**只为 ST1..ST7 出行**；cba 中超出 ST1..ST7 的组是「路线分支套」，一律不出行（见 §4/§6）
  - `记录名` = 见 §3 / §4

---

## 3. 章节行

1. 读 `mChapterSetup`（`int mChapterSetup[7][2] { {前半,后半}, … }`，7 行 = ST1..ST7）。
2. 交叉验证 `THStageWarp` 里 `if (stage == s)` 块的 `case` 数 == 该关 `前半 + 后半`；不等以 `mChapterSetup` 为准并在报告注明。
3. 逐关生成（`n` = 1..7）：
   - `后半 == 0 且 前半 != 0` → `STn #k`（k = 1..前半）
   - 否则：`k=1..前半` → `STn 前半 #k`；`k=前半+1 .. 前半+后半` → `STn 后半 #(k−前半)`

---

## 4. boss 行

1. 在 `thprac_locale_def.cpp` 里定位 `namespace THxx` 下的 `th_sections_cba[N][2][19]`。
2. 组索引 → 关卡：**只取 `i=0..6` → `ST1..ST7`**。`cba` 里若还有 `i≥7` 的组，那是 warp 用的「路线分支套」（如 th14 `i=7/8`＝ST4A/ST4B，th11 `i=7..12`＝ST4_RA..MC），与 ST1..ST7 指向**同一批原生段落**，只是按路线重排/别名 → **跳过，不出行**（见 §6）。
3. 遍历每组：
   - `cba[i][0]` = 道中Boss 列表 → 记录名 `STn MID…`
   - `cba[i][1]` = 关底Boss 列表 → 记录名 `STn BOSS…` / `STn END_…`
   - 列表为空（`{ }`）→ **不生成行**。
4. **记录名转换规则**：取枚举名 `THxx_STn_XXX` → 去掉 `THxx_` 前缀 → 把第一个 `_` 换成空格。
   - 例：`TH16_ST5_BOSS2A` → `ST5 BOSS2A`；`TH14_ST3_MID2_EN` → `ST3 MID2_EN`；`TH07_…` 不适用。

---

## 4.5 回填「段落入口函数」（warp 落点 → ECL sub）

> 骨架阶段 `段落入口函数` 留空；本步用 thprac 的 warp 偏移 + 各作离线 `.ecl`/`.decl` 回填。
> **完整流程（解析口径、参考脚本、自检、边界）见 `docs/logger/checkpoint_entry_backfill_workflow.md`；本节为速览。**

**判定口径（路过式 checkpoint / latch）**：CSV 是**白名单表**——运行期读「当前 sub」：**∈ 表 → 置当前段落为该记录名**；**不在 → no-op**（保持上一个值，**不回退 `main`**）。因此**每个 checkpoint 只填 1 个「段落入口函数」**，不需要覆盖段内每个 sub。
> 依据：段内多个 `MainSubNN` 是 `MainFront`/`MainLatter` 的**兄弟 `call`**，彼此不互为祖先，「整段驻留栈上」不成立。详见 `logger_re_disasm_overview.md` §6 Q9。

**取值规则**：

1. **文件槽**：`ECLHelper::SetBaseAddr` 默认 **slot 0 = `stNN.ecl`**；`SetFile(N)` 取该关 `ecli{}` 的第 N 项（**各关顺序不同**，如 th16 ST1 `{default,st01bs,st01mbs}`、ST5 `{default,st05mbs,st05bs}`）。
2. **指令布局**：`time(4) + id(u16) + size(u16) + mask(4) + params`；sub 以 `"ECLH"` 开头、头长 `0x10`，首指令在 `substart+0x10`。
3. **章节行**（`THStageWarp`）：该段**最后一条** `ECLJump` 的 `dest` 落在 `call`/`async call MainSubNN`（opcode `0x0B`/`0x0F`）上 → 填**被调者**；该段若无第二条 `ECLJump`（含 portion 1），填对应容器（`MainFront`/`MainLatter`）的**首个子**。
4. **boss 行**（`THPatch`）：主跳转落在 `main` 的 `call MainBoss` / `enmCreateA "Boss"/"MBoss"` 上；真正区分第几个 boss 段的是对 `stNNbs`/`stNNmbs` 的补丁与 `$SPELL_ID` 派发 → 填对应 **boss 脚本 sub**（`Boss`/`Boss1`/`BossCard1`/`MBoss` 等）。（**暂缓**，见 §6。）
5. **async 落点照挂**：`async call`（opcode `0x0F`）的被调者**照填**，不因 async 跳过。
6. 名字直接取离线 `.decl` 的 `void <Name>()`（与 `.ecl` sub 表项 `+0` 一致）。

**自检**：每行恰 1 个函数名；入口 ∈ 该关 `.ecl` 的 sub 名集合；与 `THStageWarp`/`THPatch` 偏移一一对应；异步落点未被跳过。

> 参考成品：`docs/logger/th16_checkpoints.csv`（章节行 39 行已回填）；逐作 §10 入口表见 `docs/thXX/thXX_ecl_re_report.md`。

---

## 5. 排序（可读性，不影响监控）

按关分组，每关内：**章节行 → MID 行 → BOSS/END 行**；关卡顺序固定 `ST1..ST7`。

---

## 6. 边界情况（生成前必读）

- **路线分支套一律删除（th14 / th11）**：thprac 把「按角色/路线分歧的 stage 4」在 `cba` 里另开了几组（th14 `i=7/8`＝ST4A/ST4B；th11 `i=7..12`＝ST4_RA/RB/RC/MA/MB/MC），但它们是**同一批原生段落的按路线重排/别名**：
  - th14：`CalcSection()` 只用 `th_sections_cba[*mStage]`（＝组 3 的朴素 ST4），`ST4A/ST4B` 仅用于下拉显示，patch 里无对应 case；A/B 路线由 `faketype`（`0x42ABAF`）在运行时决定。
  - th11：朴素组 3（`mSpellCategory=0`＝ALL）是并集（公共 `MID1/MID2/BOSS1..3` ＋ 6 条路线符卡 `RA1..MC3`，共 23 项且互不重复），6 个路线组共 48 行却只覆盖同样 23 项。
  - ⇒ 本 CSV **只保留 ST1..ST7（组 0..6）**，路线分支套全部不出行，避免重复的段落入口。
- **th14 ST3**：道中Boss 有两个变体 `MID2_EN` / `MID2_HL`，原样保留。
- **th16 ST5**：关底Boss 有 `BOSS2A/BOSS2B`、`BOSS4A/BOSS4B`（分支），原样保留。
- **th16 ST6**：道中Boss 列表为**空** → 无 `ST6 MID*` 行；关底含 `SPRING/SUMMER/AUTUMN/WINTER_FINAL`。
- **th15 ST3**：关底Boss 列表混入 `TH15_ST8_AB_TEST`（测试段）→ **排除**（th15 boss 73 行）。
- **各作 ST7 关底列表同构**：`END_NS1,S1,NS2,S2,…,NS8,S8,S9,S10` —— 共 **18 项**，**止于 `S9/S10`，没有 `NS9/NS10`**（勿手抄成 `NS1..NS10` 交替；th14 生成时曾误加两项，已按源码修正）。
- 起始哨兵 `A0000ERROR` / 空列表 → **一律不出行**。
- **回填的边界**：章节行入口可由 `THStageWarp` 偏移直接定；**boss 行**（`MID`/`BOSS`/`END_*`/`*_FINAL`）的区分点在 `stNNbs`/`stNNmbs` 的 boss 脚本 sub + `$SPELL_ID` 派发，需额外解码，**暂缓**（见 §4.5 规则 4）。**async 落点照挂**，不跳过。

---

## 7. 自检清单

- [ ] 数据行数 == 章节行数 + boss 行数
- [ ] 章节行数 == Σ(前半 + 后半)
- [ ] boss 行数 == Σ(该关 cba `[0]` 数 + `[1]` 数)，且排除测试段/空列表
- [ ] 每行 4 字段；`记录名` 非空且无 `#0`
- [ ] `记录名` 前缀（章节=`STn`；boss=`STn`，去掉空格后）== `所属关卡`，且 `所属关卡` ∈ {ST1..ST7}
- [ ] 无路线分支套残留（无 `ST4A/ST4B`；th11 无 `ST4 <route>_*`）
- [ ] `所属游戏` 一律 `thXX`；无 `THxx_` 前缀残留
- [ ] **骨架阶段**：`段落入口函数` 全空；**回填后**：每行恰 1 个函数名，且 ∈ 该关 `.ecl` 的 sub 名集合（见 §4.5）

---

## 8. 参考示例（th15，参照作）

- 章节：`mChapterSetup = {2,4},{3,3},{3,3},{3,3},{4,6},{4,0},{5,4}` → 6,6,6,6,10,4,9 = **47 行**
- boss（`th_sections_cba`，排除 `ST8_AB_TEST`）：7,7,9,8,9,12,21 = **73 行**
- 合计 **120 行**；产物 `docs/logger/th15_checkpoints.csv`。

片段：

```csv
段落入口函数,所属游戏,所属关卡,记录名
,th15,ST1,ST1 前半 #1
...
,th15,ST1,ST1 后半 #4
,th15,ST1,ST1 MID1
,th15,ST1,ST1 BOSS1
...
,th15,ST6,ST6 #1
,th15,ST6,ST6 STARS
,th15,ST6,ST6 BOSS1
...
```

---

## 9. 附录：九作行数（已按本文档生成并核验，2026-10-15）

> 下表九作 CSV 均已生成于 `docs/logger/thXX_checkpoints.csv` 并核验通过（每行 4 字段、无空记录名、无 `#0`、无 `THxx_` 残留、记录名前缀 == 所属关卡）。
> boss 行数含**不带 `MID/BOSS` 字样**的关底项（th15 `ST6 STARS`；th16 `*_FINAL`）。
> th10–th13 的核验方式：重新解析 `mChapterSetup` 与各作 `th_sections_cba` 后与 CSV 逐行 diff，全部精确匹配（无缺失/无多余）。
> **回填进度**：**九作均已全表回填**（2026-10-17 完成；th14–th18 于 2026-10-16，th10–th13 于 2026-10-17）。

| 作 | 章节行 | boss 行 | 合计 | 备注 |
| --- | --- | --- | --- | --- |
| th10 | 41 | 68 | 109 | ST6 无 MID；ST1 有道中变体 MID2_EN/HL |
| th11 | 37 | 88 | 125 | cba 为 13 组，组 7–12（ST4_RA..MC 路线分支套）**已删**；ST1 有 MID3_EN/HL |
| th12 | 43 | 71 | 114 | ST6 有 MID1（**2026-10-17 订正**：原备注「ST6 无 MID」有误） |
| th13 | 44 | 72 | 116 | ST6 无 MID；ST3 有 MID2_EN/HL；ST2 见下方注记 |
| th14 | 43 | 72 | 115 | ST4A/ST4B 路线分支套**已删**；ST3 有 MID2_EN/HL |
| th15 | 47 | 73 | 120 | 排除 ST8_AB_TEST |
| th16 | 39 | 72 | 111 | ST6 无 MID；ST5 有 2A/2B、4A/4B；ST6 有 *_FINAL |
| th17 | 39 | 66 | 105 | — |
| th18 | 49 | 65 | 114 | — |

> **骨架期行数 vs 现状**（2026-10-17 订正）：本表是**骨架生成时**的行数；回填过程中有两处**有意的行数变动** —— th14 ST4 拆 A/B 路线 → **43+80=123**；th15 删 `ST6 #3`（与 `ST6 STARS` 同为 `MainSub03`）→ **46+73=119**。th16 现为 **39+71=110**（骨架表写 39+72=111）。th10–th13 行数未变（109 / 125 / 114 / 116）。

> 章节各关行数：th10 5,5,7,8,6,4,6；th11 4,4,3,7,6,6,7；th12 5,5,7,8,7,3,8；th13 5,6,8,8,7,4,6；th14 5,6,6,7,6,4,9；th15 6,6,6,6,10,4,9；th16 4,4,6,6,8,3,8；th17 4,5,6,6,6,4,8；th18 5,5,6,8,9,5,11。
> 后半为 0 的关（用 `#k`）：th10 ST6；th11 无；th12 ST6；th13 ST6；th14 ST6；th15 ST6；th16 ST2、ST6；th17 无；th18 无。
> **th13 ST2 注记**：`THStageWarp` 中 `if (stage == 2)` 有 7 个 `case`，而 `mChapterSetup` 为 `{4,2}`（6）；按 §3.2 以 `mChapterSetup` 为准生成 6 行（case 6/7 指向同一 ECL 目标 `0x7a00`，第 7 个为重复）。
> **路线分支套注记（th11 / th14）**：th11 `th_sections_cba[13][2][22]` 的组 7–12、th14 `[9]` 的组 7/8，都是按自机路线重排的**同一批原生段落**（可经 `th_sections_cba[*mStage + st]` / `faketype` 取用），与组 3 朴素 ST4 重复 → **本 CSV 只保留组 0..6（ST1..ST7）**。故 th11 记录名中保留路线信息的只有 `ST4 RA1..MC3`（6 条路线符卡）；`ST4 RA_MID1` 这类分支套行已删。

**实产各关合计（章节 + boss）**

| 作 | ST1 | ST2 | ST3 | ST4 | ST5 | ST6 | ST7 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| th10 | 12 | 11 | 14 | 16 | 15 | 13 | 28 |
| th11 | 12 | 11 | 12 | 30 | 16 | 16 | 28 |
| th12 | 12 | 12 | 15 | 16 | 16 | 14 | 29 |
| th13 | 12 | 13 | 17 | 16 | 16 | 15 | 27 |
| th14 | 11 | 13 | 15 | 15 | 15 | 16 | 30 |
| th15 | 13 | 13 | 15 | 14 | 19 | 16 | 30 |
| th16 | 9 | 10 | 14 | 14 | 18 | 17 | 29 |
| th17 | 9 | 11 | 13 | 13 | 14 | 16 | 29 |
| th18 | 10 | 11 | 13 | 15 | 17 | 16 | 32 |

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-15 | 建档。定义「按 thprac 章节（warp 模式 1）生成 checkpoint CSV」的流程、输出格式与自检；明确章节轴 ≠ section 轴；th15 为参考成品。 |
| 2026-10-15 | **补 boss 轴**：新增 `th_sections_cba`（warp 模式 2/3）生成规则与命名转换；登记边界情况（th14 ST4A/ST4B、th16 空 MID/A-B/*_FINAL、th15 排除 ST8_AB_TEST）；更新五作预期行数。 |
| 2026-10-15 | **执行并核验**：按本文档生成 th14–th18 五个 CSV（th14 131 / th15 120 / th16 111 / th17 105 / th18 114 行），§9 补「实产各关合计」；§6 补 ST7 关底列表止于 `END_S9/END_S10`（无 NS9/NS10）的说明。 |
| 2026-10-15 | **扩展至 th10–th13**：新增四作 CSV（th10 109 / th11 173 / th12 114 / th13 116 行），§9 扩为「九作行数」并补各关合计；登记新边界 —— th11 cba 为 13 组（组 7–12 为 ST4 自机路线分支，归 `ST4`）、th13 ST2 `THStageWarp` 7 case 与 `mChapterSetup` 6 不一致（以 `mChapterSetup` 为准）。 |
| 2026-10-15 | **删路线分支套（消除重复）**：确认 th14 `ST4A/ST4B`、th11 `ST4_RA..MC` 与朴素 ST4 指向同一批原生段落（th14 的 A/B 由 `faketype` 决定、patch 无对应 case；th11 朴素组 3＝并集 23 项、路线组 48 行仅覆盖同样 23 项）→ **规则改为只取 cba 组 0..6（ST1..ST7）**；重生成 th11（173→**125**）、th14（131→**115**）。§2/§4/§5/§6/§7/§9 同步修订。 |
| 2026-10-15 | **抽出独立流程文档**：新增 `docs/logger/checkpoint_entry_backfill_workflow.md`（ECLJump 落点 → ECL sub 的完整回填流程 + 参考脚本 + 自检；含已废弃写法的登记）。§4.5 加指向。 |
| 2026-10-15 | **新增 §4.5「段落入口函数回填」**：定口径为**路过式 checkpoint（latch）**（经过白名单函数即置值、不在表内 no-op、不回退 `main`）→ **每 checkpoint 只填 1 个入口函数**（= warp 落点所进入的 sub，**含 async**）。给出文件槽/指令布局/章节行/boss 行取值规则与自检；同步 §2、§6、§7、§9。据此回填 **th16 章节行 39 行**（见 `docs/logger/th16_checkpoints.csv`；boss 行暂缓）。 |
| 2026-10-17 | **§9 附录按现状收口**：①「回填进度」行由「th16 章节行已回填、其余为空」改为**九作全表回填**（th14–th18 于 10-16、th10–th13 于 10-17）；②订正 th12 备注 —— 原写「ST6 无 MID」**有误**，实际有 `ST6 MID1`（CSV / `THPatch` / `stage06.ecl` 三处一致）；③新增「骨架期行数 vs 现状」注记（th14 43+80=123、th15 46+73=119、th16 39+71=110 三处与骨架表不同）。 |

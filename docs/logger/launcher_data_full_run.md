# launcher 数据页「全关」— 实施说明

> 需求来源：用户 2026-10-24「实现数据 tab-全关 页面；顶部游戏/月份筛选；按日分组展示每条记录的游戏、时间与全部 event；选中作品后右侧显示总记录数、平均 miss 与通过率统计；弹窗展示有 event 章节的通过率与总通过率」。
> 参考实现：`D:\workshop\hmaker\lib\th_all_clear\`、`D:\workshop\hmaker\docs\dev-th-all-clear.md`、`D:\workshop\hmaker\docs\thlog-data.md`。
> 数据来源：`<thprac 数据目录>/logger_sessions_YYYYMM.jsonl`（`docs/req_logger.md`、`docs/logger/logger_impl_report.md`）。
> 代码：`thprac/src/thprac/thprac_launcher_data.cpp`（`FullRunTabGui`）、`thprac_launcher_main.cpp`（数据 tab 接线）。
> 状态：**Release|x86 编译通过（0 error）；界面由用户真机核对**。
> 口径：只读 logger 数据，不新增任何逆向地址；GUI 不要求还原 hmaker。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 数据源 | `<thprac 数据目录>/logger_sessions_YYYYMM.jsonl`；**只读所选月份**，坏行/半截行静默跳过 |
| 列表范围 | **只显示全关记录**（不是当月全部原始记录） |
| 全关判定 | `startCheckpoint == checkpoint 表第一条记录名` 且 `endCheckpoint == 表中最后一条以 ST6 开头的记录名`（按行序，不按字符串排序）；**th16 例外：最后 4 条 ST6 均为有效终点** |
| 游戏范围 | th10–th18（exe 内已嵌入 checkpoint 表）；无表/未知作品不判为全关 |
| 月份选择 | 默认当前月；当前月无文件回落最新月并提示；切月重读该月文件 |
| 游戏筛选 | `全部` + 当月实际有全关记录的作品；`全部` 时不显示右侧统计 |
| 总记录数 | 当前作品在当前月份的全关记录条数 |
| 平均 miss | `Σ events.size() / 全关记录数`，`%.2f`；所有 event 类型都计入 |
| 章节通过率 | `1 - 该章节出过 ≥1 个 event 的全关记录数 / 全关记录数`，`%.2f%%`；同一局同一章节多个 event 只算一次失败 |
| 总通过率 | 所有“有 event 章节”通过率之积，`%.2f%%`；无 event 章节时按空乘积 `100.00%` |
| 展示 | 按日倒序；每条第一行游戏 + 时间 + `event 数量: N`，第二行全部 event（`type: checkpoint`，`  |  ` 分隔） |
| 文案 | 新增 8 条 zh/en/jp locale 词条 |

---

## 1. 数据读取与月份

- 路径：`LauncherGetDataDir()`（支持便携模式 `.thprac_data\`）+ `logger_sessions_YYYYMM.jsonl`。
- 月份列表：`ListMonthFiles()` 严格校验 `logger_sessions_` + 6 位数字 + `.jsonl`，倒序去重。
- 读取：复用单卡页的 `LoadMonthSessionRecords()`（`LoadJsonFile()` + 按 `\n` 切行 + `ParseSessionRecord()`），读完后按 `start` 倒序。
- 默认月份：
  1. 当前月（`CurrentMonthKey()`）文件存在 ⇒ 选中当前月；
  2. 否则选中最新存在的月份，并显示 `THPRAC_DATA_PRACTICE_FALLBACK`。
- 刷新：当前选中月仍存在则保留；否则按上述规则重选。
- 切月：只读该月文件；切作品：不读盘，只重算内存中的筛选与统计。
- 记录按 `start` 的日期分组；跨月运行仍按写入月份文件读取（与练习量/单卡一致）。

## 2. 全关判定

对每条记录：

1. 用 `game` 找 `FindCheckpointSource()`；找不到（未知作品/th06–th08）⇒ 丢弃。
2. `LoadCheckpointNames()` 取 CSV 第 4 列“记录名”，保留行序。
3. `first = names.front()`；`endCheckpoints` = 反向扫描到的 ST6 记录名（**行序最后**，不是字符串排序最大）。
   - 一般作品取 **1** 条（行序最后一条 ST6）；
   - **th16 取 4 条**：`ST6 SPRING_FINAL` / `ST6 SUMMER_FINAL` / `ST6 AUTUMN_FINAL` / `ST6 WINTER_FINAL` 均可作为全关终点。
4. `startCheckpoint == first && endCheckpoint ∈ endCheckpoints` ⇒ 全关；否则丢弃。

```text
fullRun(record) =
    table.valid
    && record.startCheckpoint == table.first
    && record.endCheckpoint ∈ table.endCheckpoints   // 一般 1 条；th16 为最后 4 条 ST6
```

- 表为空、无 `ST6` 行、CSV 坏行 ⇒ `table.valid = false`，该作品记录一律不判为全关。
- CSV 同名记录只影响行序取首/末，不额外去重。
- 已知边界：logger 的“切关触发 `game_start` 导致全关被拆成多行”（`knownBugs.md` #5）会让首尾不再同时命中，该次全关可能漏判；属已知限制。

## 3. 筛选与统计

### 3.1 筛选

- 游戏列表 `mGameIds` = 当月全关记录里实际出现的作品，按 `gGameDefs` 顺序、未知 id 排后。
- 游戏选择：空串 = `全部`；`全部` 时列表展示所有作品的全关记录，右侧统计与通过率按钮隐藏。
- 选中具体作品时，`mFiltered` = 该作品的全关记录；`mStats` 只基于 `mFiltered` 计算。
- 切换月份后，若原选中作品在新月份没有全关记录，则回退到 `全部`。

### 3.2 指标

设 `N = 全关记录数`、`E = Σ events.size()`：

```text
总记录数     = N
平均 miss    = N == 0 ? 0.00 : E / N
章节通过率(c) = N == 0 ? 0.00%
                : (N - H(c)) * 100 / N
                其中 H(c) = 出过至少 1 个 checkpoint == c 的 event 的记录数
总通过率      = Π(章节通过率(c) / 100) * 100
                （无任何有 event 章节时 = 100.00%）
```

- `H(c)` 按**记录**去重：同一局同一章节多次 miss/bomb 只算一次失败。
- 平均 miss 与列表 event 行按 **event 实例数**统计，不去重。
- `interrupt` 也计入 event（全关记录通常不会有，但数据里出现时照算）。
- 只列出 `H(c) > 0` 的章节；章节顺序优先 CSV 行序，CSV 中不存在的 checkpoint（如 `main`）按名称追加在后。
- 通过率显示 `%.2f%%`；总通过率同样 `%.2f%%`。

## 4. 展示

```text
[刷新]  2026-10                    总记录数: 2   平均miss数: 7.00   [通过率统计]
游戏 [全部 v]    月份 [2026-10 v]
--------------------------------------------------------------------------------
2026-10-21 周二    1 条
    th15 东方绀珠传    2026-10-21 21:58:55    event 数量: 7
        bomb: ST2 BOSS3  |  bomb: ST5 后半 #5  |  bomb: ST6 BOSS11
2026-10-17 周六    2 条
    th14 东方辉针城    2026-10-17 16:08:09    event 数量: 11
        bomb: ST3 MID2_HL  |  bomb: ST3 BOSS3  |  ...
    th15 东方绀珠传    2026-10-17 16:38:02    event 数量: 7
        bomb: ST2 BOSS1  |  bomb: ST2 BOSS3  |  ...
```

- 顶部：刷新、当前月份；选中具体作品时，右侧右对齐显示总记录数、平均 miss、通过率按钮。
- 筛选行：游戏下拉、月份下拉；回落提示复用 `THPRAC_DATA_PRACTICE_FALLBACK`。
- 日头：`YYYY-MM-DD 周X` + `%d 条`；只列有全关记录的日期，日期倒序。
- 单条：第一行 `GameDisplayName` + `start` + `event 数量: N`（`THPRAC_DATA_FULL_RUN_EVENT_COUNT`）；第二行缩进显示全部 event；无 event 显示“无 event”。
- 通过率弹窗：游戏、总记录数、平均 miss；固定大小滚动区内的“章节名 / 进度条 / 通过率”三列表格；底部“总通过率”与关闭按钮。
- 空态：
  | 条件 | 文案 |
  | --- | --- |
  | 无任何月份文件 | `THPRAC_DATA_NO_FILE` |
  | 读取失败且无记录 | `THPRAC_DATA_READ_FAILED` |
  | 该月有记录但无全关 | `THPRAC_DATA_FULL_RUN_EMPTY_MONTH` |
  | 当前筛选无全关 | `THPRAC_DATA_FULL_RUN_EMPTY_FILTER` |

## 5. 与 hmaker 全关页的差异

| 项 | hmaker | 本页 |
| --- | --- | --- |
| 时间筛选 | 近一周/一月/三月/全部（滚动窗口） | **月份**（按日志文件），默认当前月 |
| 分组 | 按日 / 按机体 | **仅按日** |
| 单条内容 | 时间、机体、起止 checkpoint、miss 数/明细 | 游戏、时间、全部 event（含类型） |
| 游戏范围 | th14–th18 | th10–th18（logger 已接线且有 checkpoint 表） |
| 统计 | 表头全关数 / 平均 miss | 顶部总记录数、平均 miss、通过率弹窗 |
| 通过率 | 未实现 | 章节通过率 + 总通过率（按记录去重） |
| 数据目录 | 硬编码 `%APPDATA%\thprac` | `LauncherGetDataDir()`，支持便携模式 |
| 文案 | 硬编码中文 | zh/en/jp locale |

## 6. 边界与已知限制

1. 只读所选月份的单个文件；更早月份需切月查看。
2. 全关判定依赖各作 checkpoint 表；表缺失/无 ST6 行 ⇒ 保守不判全关。
3. 全关被 logger 拆成多行（`knownBugs.md` #5）时可能漏判。
4. 跨月运行按写入月份文件读取，按 `start` 日期分组。
5. `events` 缺失/空数组 ⇒ 0 event、通过率 100%。
6. 同一局同一章节多次 event 只影响平均 miss 与 event 行，不影响章节通过率的分母口径。
7. 读盘同步执行；单月文件数 MB 时可能卡一帧，与其他数据子页一致。
8. 页面不轮询；首次进入与点刷新时读盘。

## 7. 验证

1. 编译：
   ```powershell
   D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe thprac.sln -t:restore,build -p:RestorePackagesConfig=true -p:Configuration=Release
   ```
   预期 0 error；本页无新增警告。
2. 用本机 `%APPDATA%\thprac\logger_sessions_202610.jsonl` 核对：
   - 全关记录共 **4** 条：th14 **1** 条（`2026-10-17 16:08:09`，`ST1 前半 #1 → ST6 BOSS10`，11 event）、th15 **2** 条（`2026-10-17 16:38:02`、`2026-10-21 21:58:55`，`ST1 前半 #1 → ST6 BOSS11`，各 7 event）、th16 **1** 条（`2026-10-21 22:35:32`，`ST1 前半 #1 → ST6 AUTUMN_FINAL`，5 event）。
   - 日期分组：`2026-10-17` 2 条、`2026-10-21` 2 条。
   - 选 th14：总记录数 **1**、平均 miss **11.00**、11 个有 event 章节均 **0.00%**、总通过率 **0.00%**。
   - 选 th15：总记录数 **2**、平均 miss **7.00**、13 个有 event 章节（`ST2 BOSS3` 0.00%，其余 12 个 50.00%）、总通过率 **0.00%**。
   - 选 th16：总记录数 **1**、平均 miss **5.00**、4 个有 event 章节均 **0.00%**（`ST5 BOSS4B` 有 2 个 event，但按记录去重只算 1 次）、总通过率 **0.00%**；`ST6 AUTUMN_FINAL` 作为末条 4 个终点之一应被计入。
   - 带 `total` 的练习/单章节合并记录不应出现。
3. 界面人工核对：全部/具体作品切换时统计显隐；月份切换；刷新；通过率弹窗滚动/关闭；窄窗口布局。
4. 切换 English / 日本語 重启 launcher，确认 7 条新文案。
5. 回归：练习量 / 单过程 / 单卡 三个子页仍可正常打开，locale 数组下标未错位。

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-24 | 建档：实现 launcher「数据 → 全关」；只读所选月份 `logger_sessions_YYYYMM.jsonl`，按 CSV 首条/ST6 末条判定全关；顶部游戏（全部 + 当月有全关记录的作品）/月份筛选；按日倒序展示游戏、时间与全部 event；选中具体作品时显示总记录数、平均 miss（2 位小数）与通过率统计；弹窗按“出过该章节 event 的记录数 / 总记录数”计算各章节通过率并求乘积总通过率，均 2 位小数；新增 7 条 zh/en/jp locale。**Release\|x86 编译通过（0 error）；界面由用户核对。** |
| 2026-10-24 | **th16 全关终点特例**：th16 的 ST6 末尾有 `SPRING_FINAL` / `SUMMER_FINAL` / `AUTUMN_FINAL` / `WINTER_FINAL` 四个最终关，现把行序最后 **4** 条 ST6 都视为有效终点（其余作品仍只取最后 1 条）。验收数字更新为 4 条全关（th14 1、th15 2、th16 1）；th16 的 `ST1 前半 #1 → ST6 AUTUMN_FINAL` 现应计入。**Release\|x86 编译通过（0 error）。** |
| 2026-10-24 | **每条记录显示 event 数量量**：第一行在游戏/时间后追加 `event 数量: N`（新增 `THPRAC_DATA_FULL_RUN_EVENT_COUNT`，zh/en/jp），第二行仍显示全部 event。**Release\|x86 编译通过（0 error）。** |

# launcher 数据页「单卡」— 实施说明

> 需求来源：用户 2026-10-24「实现数据 tab-单卡 页面；顶部选择作品/章节；像单过程一样支持新建/保存配置；配置有且仅有一个章节；按日聚合所有仅含本章节的记录，每条只展示 total 总和、got 总和、收率」。
> 参考实现：`docs/logger/launcher_data_single_run.md`（检查点表 / 配置框架）、`docs/logger/launcher_data_practice.md`（月份选择 / 回落提示）、`docs/req_logger.md` 口径 11/13/14/16。
> 数据来源：`<thprac 数据目录>/logger_sessions_YYYYMM.jsonl`，本页只读**所选月份**一个文件。
> 代码：`thprac/src/thprac/thprac_launcher_data.cpp`（`SingleCardTabGui`）、`thprac_launcher_main.cpp`（数据 tab 接线）。
> 状态：**Release|x86 编译通过；启动冒烟通过；界面由用户真机核对**。
> 口径：GUI 不要求还原 hmaker；筛选、聚合与展示内容一致即可。不新增任何逆向地址。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 数据源 | `<数据目录>logger_sessions_YYYYMM.jsonl`；**只读所选月份**，坏行/半截行静默跳过 |
| 月份选择 | 与「练习量」一致：默认当前月；当前月无文件时回落最新存在的月份并提示；切月重读该月文件 |
| 查询条件 | `game` + `startCheckpoint == 所选章节`，且记录为 logger 单章节记录 |
| 游戏范围 | th10–th18（复用 exe 内嵌 checkpoint CSV） |
| 单章节判据 | `total > 0` 直接视为单章节；旧行无 `total` 时按口径 16 复判（`cpCount==2 && 含interrupt` 或 `cpCount==1 && 不是只有interrupt`） |
| 章节锚定 | `startCheckpoint`（练习启动段落）；收卡后 `endCheckpoint` 推进到下一段的记录仍归本章节 |
| 每条记录口径 | `total = total > 0 ? total : 1`；`got = total > 0 ? clamp(got,0,total) : (无 miss/bomb/break ? 1 : 0)` |
| 聚合 | 按 `start` 日期分组，`Σ total` / `Σ got` / `got*100/total`（`%.2f%%`） |
| 展示 | 每日一行，只显示日期、total 总和、got 总和、收率；日期倒序 |
| 配置 | `保存配置 / 配置列表 / 新建 / 编辑 / 删除 / 加载`；落盘 `<数据目录>thprac_single_card_configs.json`；**一条配置只有一个章节**，列表按当前作品过滤 |
| 文案 | 新增 6 条 zh/en/jp locale 词条；复用月份、回落提示、单过程配置文案 |

---

## 1. 数据读取与月份

- 路径：`LauncherGetDataDir()`（支持便携模式 `.thprac_data\`）+ `logger_sessions_YYYYMM.jsonl`。
- 月份列表：`ListMonthFiles()` 严格校验 `logger_sessions_` + 6 位数字 + `.jsonl`，倒序去重。
- 默认月份：
  1. 当前月（`CurrentMonthKey()`）文件存在 ⇒ 选中当前月；
  2. 否则选中最新存在的月份，并显示 `THPRAC_DATA_PRACTICE_FALLBACK`。
- 刷新：当前选中月仍存在则保留；否则按上述规则重选。
- 切月：`LoadMonthSessionRecords()` 只读该月文件，读完后按 `start` 倒序排序。
- 切作品/章节：不读盘，只重算内存中的日聚合。
- 读取失败：`ReadMonthLines()` 失败时 `mReadFailed = true`；没有可用记录时页面显示「读取记录失败」，有其他记录时仅显示灰色提示。

## 2. 单卡记录口径

### 2.1 筛选

```text
include(record) =
    record.game == game
    && record.startCheckpoint == chapter
    && IsSingleChapterSession(record)
```

`IsSingleChapterSession()` 与 `thprac_games.cpp` 的 `GameSessionIsSingleChapter()`（口径 13/16）一致：

```text
total > 0                                      -> true
否则：
  onlyInterrupt = events 非空 且 没有 miss/bomb/break
  true = (checkpointCount == 2 && 含 interrupt)
      || (checkpointCount == 1 && !onlyInterrupt)
```

- `total > 0` 是 logger 写入的单章节行标记；这类行即使 `endCheckpoint` 已推进到下一段（收卡成功后进入下一非符再退出），仍是本章节的练习记录，**必须计入**，否则收率会因漏掉成功记录而严重偏低。
- 旧日志中 `total` 缺失的行按口径 16 复判，兼容合并功能上线前的数据。
- `checkpointCount == 1 && 只有 interrupt`（warp 进去直接退出）不算单章节练习，避免虚增 `got`。
- `checkpointCount == 2` 且无 `interrupt` 的旧行不纳入（当前口径下不是单章节）。

### 2.2 total / got

```text
total(record) = record.total > 0 ? record.total : 1
got(record)   = record.total > 0
                ? clamp(record.got, 0, record.total)
                : (无 miss/bomb/break 事件 ? 1 : 0)   // interrupt 不影响 got
```

- 合并行（`total > 1`）按 `total`/`got` 展开，不按行数计。
- `got` 缺失按 0；`got > total` 截断；`interrupt` 不计入失败。

## 3. 按日聚合与展示

以记录 `start` 的日期（`YYYY-MM-DD`）为 key，仅限所选月份：

```text
day.total = Σ total(record)
day.got   = Σ got(record)
day.rate  = day.total == 0 ? "—" : day.got * 100.0 / day.total
```

展示（ImGui）：

```text
[刷新]  2026-10
游戏 [th14 ... v]    月份 [2026-10 v]
章节 [ST4B BOSS6 ... v]  [保存配置]  [配置列表]
------------------------------------------------
日期          total 总和   got 总和   收率
2026-10-22    33           8          24.24%
...
```

- 只列有匹配记录的日期；日期倒序（最新在上）。
- 不加总行、不加记录条数、不加明细，严格遵循“每条只展示 total 总和 / got 总和 / 收率”。
- 收率保留两位小数；`total == 0` 防御性显示 `—`（正常不会出现）。

空态：

| 条件 | 文案 |
| --- | --- |
| 无任何月份文件 | `THPRAC_DATA_NO_FILE` |
| 读取失败且无记录 | `THPRAC_DATA_READ_FAILED` |
| checkpoint 表缺失 | `THPRAC_DATA_SINGLE_RUN_TABLE_MISSING` |
| 未选章节 | `THPRAC_DATA_SINGLE_CARD_HINT` |
| 本月该章节无记录 | `THPRAC_DATA_SINGLE_CARD_EMPTY` |

## 4. 配置（保存 / 列表 / 新建 / 编辑 / 删除）

### 4.1 存储

- 文件：`<数据目录>thprac_single_card_configs.json`（UTF-8，顶层 JSON 数组，保持保存顺序）。
- 单条：`{ "name": string, "game": string, "chapter": string }`。
- 读失败 / 解析失败 / 顶层不是数组 ⇒ 空列表；单条坏项跳过，不影响其余。
- 增/删/改全量重写；写失败不提交内存态，并在弹窗内提示。

### 4.2 作品限制

- **配置列表只显示 `config.game == 当前选中作品` 的条目**；切换游戏后列表随之变化。
- 保存 / 编辑弹窗的「游戏」固定为当前作品（只读展示）。
- 同名校验只在**同一作品内**进行。
- 配置与月份无关：同一配置可在任意月份加载。

### 4.3 交互

| 操作 | 行为 |
| --- | --- |
| 保存配置（主页面） | 打开编辑器，名称必填；章节预填当前选择，也可手输；保存前按当前作品 checkpoint 表校验 |
| 新建配置（列表内） | 同上；保存后回到列表 |
| 配置列表 | 只列当前作品；每行显示名称、章节，操作：加载 / 编辑 / 删除 |
| 加载 | 校验章节在表中，设置章节并立即重算，关闭弹窗 |
| 编辑 | 名称 / 章节可改，游戏固定；保存后回列表 |
| 删除 | 行内二次确认；写盘成功才从内存移除 |

## 5. 与「单过程」的差异

| 项 | 单过程 | 单卡 |
| --- | --- | --- |
| 数据范围 | 最近 3 个存在的月份文件 | **所选月份的 1 个文件**（默认当前月） |
| 选择条件 | 游戏 + 起点 + 终点 | 游戏 + 月份 + 章节 |
| 统计范围 | 最近 10/20 条、一周、一月、全部 | 所选月份全部记录 |
| 展示 | 记录条数、NN 数、预计/实际 NN 率、章节通过率、明细 | **每日 total 总和 / got 总和 / 收率** |
| 单章节口径 | 按起终点精确匹配；`total>0` 优先，否则按事件推导 | 复用 logger 单章节判据；章节锚定 `startCheckpoint` |
| 配置 | `{name,game,start,end}` | `{name,game,chapter}`（有且仅有一个章节） |
| 配置文件 | `thprac_single_run_configs.json` | `thprac_single_card_configs.json` |
| 查询按钮 | 有 | 无；选章节即重算 |

## 6. 边界与已知限制

1. 只统计**所选月份**；跨月不会合并。
2. 合并行 `endCheckpoint` 只反映首条尝试；本页按 `startCheckpoint` 归章，不受影响。
3. 旧行没有 `total` 时按口径 16 复判；`cpCount==2` 且无 `interrupt` 的旧行不纳入。
4. 旧行 `checkpointCount == 1 && 只有 interrupt` 不纳入，避免 warp 进/出被当成干净通过。
5. 读盘同步执行；单月文件数 MB 时可能卡一帧，与练习量页一致。
6. 配置的章节是记录名字符串；若 CSV 改名导致名字不在表中，加载/保存会提示章节无效。
7. 配置列表按作品过滤；其他作品的配置不会显示，但仍保存在文件中。
8. 页面不轮询；刷新 / 切月 / 加载配置时才更新。

## 7. 验证

1. 编译：
   ```powershell
   D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe thprac.sln -t:restore,build -p:RestorePackagesConfig=true -p:Configuration=Release
   ```
   预期 0 error。
2. 启动 `Release\thprac.exe`，确认 launcher 正常启动（启动冒烟已通过）。
3. 打开 `数据 → 单卡`，选择月份 `2026-10`，用本机 `logger_sessions_202610.jsonl` 核对：

   | 作品 / 章节 | 日期 | total 总和 | got 总和 | 收率 |
   | --- | --- | ---: | ---: | ---: |
   | th17 / `ST6 BOSS10` | 2026-10-20 | 20 | 7 | 35.00% |
   | th17 / `ST6 BOSS10` | 2026-10-19 | 8 | 0 | 0.00% |
   | th14 / `ST4B BOSS6` | 2026-10-22 | 33 | 8 | 24.24% |
   | th16 / `ST6 BOSS8` | 2026-10-18 | 5 | 3 | 60.00% |
   | th17 / `ST6 BOSS2` | 2026-10-20 | 14 | 6 | 42.86% |

   - `th16 ST6 BOSS8 -> ST6 BOSS9` 的 `total>0` 行归到 `ST6 BOSS8`，用于验证“收卡后推进下一段仍计入本章节”。
   - 上述数值已用独立 Python 按 §2/§3 公式复算一致。
4. 配置：
   - 当前作品保存一条配置 → 切到其他作品，列表不可见；切回可见；
   - 编辑名称 / 章节后列表即时更新；删除有二次确认；
   - 重启 launcher 后配置仍在；文件中其他作品的条目不被覆盖；
   - 章节不在表中时保存/加载提示错误。
5. 月份：
   - 当前月存在时默认选中当前月；
   - 切月只显示该月记录；作品/章节选择保留；
   - 当前月文件缺失时回落最新月并提示。
6. 切换 English / 日本語 重启 launcher，确认标签、月份、表头、空态、配置弹窗文案。

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-24 | 建档：实现 launcher「数据 → 单卡」；顶部游戏/月份/章节，默认当前月、无当前月文件回落最新月；按 `startCheckpoint` 锚定章节，复用 logger 单章节判据；按日聚合 `total`/`got`/收率；新增保存配置 / 配置列表 / 新建 / 编辑 / 删除，配置落盘 `thprac_single_card_configs.json`（`{name,game,chapter}`），列表按当前作品过滤；新增 6 条 locale。**Release\|x86 编译通过；启动冒烟通过；界面待用户核对。** |

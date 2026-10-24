# launcher 数据页「单过程」— 实施说明

> 需求来源：用户 2026-10-24「参考 `../hmaker` 的 th 练习记录页，实现『单过程』页面；保存配置 / 配置列表，配置列表受已选择的作品限制」。
> 参考实现：`../hmaker/lib/th_practice_record/`（Flutter）、`../hmaker/docs/dev-th-practice-record.md`。
> 数据来源：`docs/req_logger.md`、`docs/logger/logger_impl_report.md`（`logger_sessions_YYYYMM.jsonl`）。
> 代码：`thprac/src/thprac/thprac_launcher_data.cpp`（`SingleRunTabGui`）、`thprac_launcher_main.cpp`（数据 tab 接线）。
> 状态：**Release|x86 编译通过；真机效果由用户核对**。
> 口径：GUI 不要求还原 hmaker；筛选、统计与展示内容一致即可。不新增任何逆向地址。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 数据源 | `<thprac 数据目录>/logger_sessions_YYYYMM.jsonl`；取**最近 3 个存在的月份文件**，坏行/半截行静默跳过 |
| 查询条件 | `game` + `startCheckpoint` + `endCheckpoint` 三者**精确字符串相等** |
| 游戏范围 | th10–th18（logger 已接线且 exe 内已有 checkpoint 表；th06–th08 / th09 不在日志范围） |
| checkpoint 表 | 复用各作已编译进 exe 的 `kThXXCheckpointsCsv`（与 logger 同一份 CSV），不复制 `docs/` 数据 |
| 统计范围 | 最近 10 条 / 最近 20 条（默认）/ 最近一周 / 最近一个月 / 全部 |
| 练习次数口径 | `attempts = total > 0 ? total : 1`（合并行按次数展开） |
| NN 口径 | `clean = total > 0 ? clamp(got,0,total) : (无 miss/bomb/break 事件 ? 1 : 0)`；`interrupt` **不计** miss，`break` **计入** |
| 展示指标 | 记录条数（= 次数）、NN 数、预计 NN 率、实际 NN 率、各章节通过率、记录明细 |
| 配置 | `保存配置 / 配置列表 / 编辑 / 删除`；落盘 `<数据目录>thprac_single_run_configs.json`；**列表只显示当前选中作品的配置** |
| 文案 | 新增 32 条 zh/en/jp locale 词条（19 条单过程 + 13 条配置） |

---

## 1. 数据读取

- 路径：`LauncherGetDataDir()`（支持便携模式 `.thprac_data\`）+ `logger_sessions_YYYYMM.jsonl`。
- 月份：`ListMonthFiles()` 严格校验 `logger_sessions_` + 6 位数字 + `.jsonl`，倒序去重；取前 3 个。
- 读取：复用练习量页的 `LoadJsonFile()` + 按 `\n` 切行；抽出 `ReadMonthLines()` 模板，练习量页行为不变。
- 必需字段：`game` / `start` / `startCheckpoint` / `endCheckpoint` / `checkpointCount`。
- 可选字段：`shot`（th18 为空串）、`total` / `got`（成对）、`events[]`。
- `start` 必须是 `YYYY-MM-DD HH:MM:SS`；非法则丢行，保证排序与时间筛选可靠。
- 三个文件合并后按 `start` 倒序（定长字符串，字典序 = 时间序）。

## 2. 单条记录口径

```text
attempts(row)   = total > 0 ? total : 1
clean(row)      = total > 0 ? clamp(got, 0, total)
                            : (没有 miss/bomb/break 事件 ? 1 : 0)
missPoints(row) = 去重(events[].checkpoint，排除 type == "interrupt")
```

- **`interrupt` 不计入 miss**：练习段打完退出几乎必带 interrupt；照 hmaker 旧口径会把 NN 率打成 0。
- **`break` 计入 miss**（th17 撞咆哮）。
- **合并行必须按 `total`/`got` 展开**：否则 th17 `ST6 BOSS10` 的 32 次练习会被当成 12 条且全部 NN。
- `total == 1` 的首条记录不规范化，保留真实 `events`；`total > 1` 的合并行可能 `events=[]`，也可能残留首条事件，统计以 `total`/`got` 为准。

## 3. 查询与统计

### 3.1 筛选

- 匹配：`game`、`startCheckpoint`、`endCheckpoint` 三者全等。
- 范围过滤（对命中日志行）：
  - 最近 10 / 20 条：命中结果已倒序，直接取前 N 行；
  - 最近一周 / 一个月：本地时间算 `now - 7d/30d` 的字符串阈值，`start >= cutoff`；
  - 全部：不过滤。

### 3.2 指标

设 `attempts = Σ attempts(row)`、`clean = Σ clean(row)`：

```text
记录条数   = attempts
NN数       = clean
实际NN率   = attempts == 0 ? 0.00% : clean * 100 / attempts
各章节通过率(name) = attempts == 0 ? 0.00%
                     : (attempts - 该章节失败次数) * 100 / attempts
预计NN率   = 章节列表为空 ? “—”
             : attempts == 0 ? 0.00%
             : Π(各章节通过率 / 100) * 100
```

失败次数归属（章节列表 = CSV 起点→终点切片，行序含两端）：

1. 区间章节数 == 1，或 `startCheckpoint == endCheckpoint` 且该名字在区间内 ⇒ 该行全部 `attempts - clean` 次失败归到这一章节；
2. 否则（合并行 / 跨多章节）⇒ 已知事件点各记 1 次命中，最多 `min(失败次数, 不同 miss 点个数)`；剩余失败不摊派，只影响实际 NN 率；
3. 章节列表为空（表缺失 / 区间无效）⇒ 预计 NN 率显示 `—`，不照搬 hmaker 的 `fold(1.0)=100%` 陷阱。

### 3.3 明细

- 每条匹配日志一行，倒序：时间、机体、去重后的 miss 点；
- 无 miss 显示 `NN`（金色），有 miss 绿色；
- `total > 1` 的合并行末尾追加 `×N（NN G）`；`got < total` 且无事件时显示 `—`，不误报 NN。

## 4. 配置（保存 / 列表 / 编辑 / 删除）

### 4.1 存储

- 文件：`<数据目录>thprac_single_run_configs.json`（UTF-8，顶层 JSON 数组，保持保存顺序）。
- 单条：`{ "name": string, "game": string, "start": string, "end": string }`。
- 读失败 / 解析失败 / 顶层不是数组 ⇒ 空列表；单条坏项跳过，不影响其余。
- 增/删/改全量重写；写失败不提交内存态，并在弹窗内提示。

### 4.2 作品限制

- **配置列表只显示 `config.game == 当前选中作品` 的条目**；切换游戏后列表随之变化。
- 保存 / 编辑弹窗的「游戏」固定为当前作品（只读展示），因此不能在本页给其他作品建配置；其他作品已有的配置保留在文件中，切到该作品才可见。
- 同名校验只在**同一作品内**进行。

### 4.3 交互

| 操作 | 行为 |
| --- | --- |
| 保存配置（主页面） | 打开编辑器，名称必填；起点/终点预填当前条件，也可手输，保存前按当前作品 checkpoint 表校验 |
| 保存配置（列表内「新建」） | 同上；保存后回到列表 |
| 配置列表 | 只列当前作品；每行显示名称、`起点 → 终点`，操作：加载 / 编辑 / 删除 |
| 加载 | 校验起终点在表中且顺序正确，设条件并立即查询，关闭弹窗 |
| 编辑 | 名称 / 起点 / 终点可改，游戏固定；保存后回列表 |
| 删除 | 行内二次确认；写盘成功才从内存移除 |

## 5. UI 结构

```
[刷新]  数据来源：logger_sessions_*.jsonl（最近 3 个文件）
游戏 [th14 ... v]
起点 [ST4 BOSS1 ... v]
终点 [ST4 BOSS6 ... v]
统计范围 [最近20条 v]  [查询]  [保存配置]  [配置列表]

（未查询）请选择游戏、起点与终点后点击「查询」
（已查询）
  记录条数 / NN数
  预计NN率 / 实际NN率
  各章节通过率（名称 + 进度条 + 百分比）
  记录明细（时间 / 机体 / miss 或 NN）
```

- 游戏下拉 th10–th18；起点全量、终点只列起点及其之后。
- 换游戏重置起终点与结果；改起点/终点清空结果；改范围只重算；刷新重读文件并保留当前条件。
- 首次进入默认取“最新一条记录的作品”，无记录时回退 th14。
- 配置弹窗：列表模式 / 编辑模式共用同一个模态窗口，避免嵌套弹窗。

## 6. 与 hmaker 的差异

| 项 | hmaker | 本页 |
| --- | --- | --- |
| 数据目录 | 硬编码 `%APPDATA%\thprac` | `LauncherGetDataDir()`，支持便携模式 |
| 游戏范围 | th14–th18 | th10–th18（覆盖本机 th11 数据） |
| miss 口径 | 所有 `events` 一律计入（当时无 `interrupt`） | 排除 `interrupt`，`break` 计入 |
| 合并行 | 不支持 `total`/`got` | 按 `total`/`got` 展开 |
| 起终点输入 | Autocomplete（自由输入 + 候选） | 下拉选择 |
| 配置 | 可管理多套，列表不按作品过滤 | **列表按当前作品过滤**；同名只在本作品内校验 |
| 配置位置 | `%APPDATA%\hmaker\th_practice_configs.json` | `<thprac 数据目录>thprac_single_run_configs.json` |
| 文案 | 硬编码中文 | zh/en/jp locale |

## 7. 边界与已知限制

1. 只读最近 3 个存在的月份文件；更早的历史不在统计范围（与 hmaker 一致）。
2. 合并行 `events` 可能为空或只含首条；多章节合并行的剩余失败只影响实际 NN 率，不摊派到章节。
3. 时间范围按日志行 `start` 归属；合并行跨阈值时整行计入。
4. 配置的起终点是记录名字符串；若 CSV 改名导致名字不在表中，加载/保存会提示区间无效。
5. 配置列表按作品过滤；其他作品的配置不会显示，但仍保存在文件中。
6. 读盘同步执行；单月文件数 MB 时可能卡一帧，与练习量页一致。
7. 页面不轮询；刷新/加载配置时才更新。

## 8. 验证

1. 编译：
   ```powershell
   D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe thprac.sln -t:restore,build -p:RestorePackagesConfig=true -p:Configuration=Release
   ```
   预期 0 error。
2. 打开 `数据 → 单过程`，用本机现有 `logger_sessions_202610.jsonl` 核对（默认范围最近 20 条）：
   - `th17` / `ST6 BOSS10 → ST6 BOSS10`：记录条数 **32**、NN 数 **11**、实际 NN 率 **34.38%**、明细 **12 行**；
   - `th14` / `ST4B BOSS6 → ST4B BOSS6`：记录条数 **33**、NN 数 **8**、实际 NN 率 **24.24%**、明细 **14 行**；
   - `th18` / `ST4 BOSS2 → ST4 BOSS2`：记录条数 **6**、NN 数 **3**、实际 NN 率 **50.00%**、明细 **1 行**。
3. 配置：
   - 当前作品 th14 保存一条配置 → 切到 th17，列表应看不到它；切回 th14 可见；
   - 编辑名称 / 起终点后列表即时更新；删除有二次确认；
   - 重启 launcher 后配置仍在；配置文件中其他作品的条目不被覆盖。
4. 切换 English / 日本語 重启 launcher，确认新增标签、指标名、配置弹窗文案。

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-24 | 建档：实现 launcher「数据 → 单过程」；按 game + 起终点精确匹配最近 3 个月日志，五档范围统计与明细；`interrupt` 不计 miss、`total`/`got` 展开；新增保存配置 / 配置列表 / 编辑 / 删除，配置落盘 `thprac_single_run_configs.json` 且**列表按当前作品过滤**；新增 32 条 locale。**Release\|x86 编译通过。** |

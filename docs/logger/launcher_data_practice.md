# launcher 数据页「练习量」— 实施说明

> 需求来源：用户 2026-10-23「把 hmaker 的 th 练习量记录搬到 launcher 数据页」。
> 参考实现：`D:\workshop\hmaker\lib\th_practice_amount\`（Flutter）、`D:\workshop\hmaker\docs\dev-th-practice-amount.md`。
> 数据来源：`docs/req_logger.md`、`docs/logger/logger_impl_report.md`（`logger_sessions_YYYYMM.jsonl`，本页是该文件的首个读取端）。
> 代码：`thprac/src/thprac/thprac_launcher_data.cpp`（`PracticeTabGui`）、`thprac_launcher_main.cpp`（数据 tab 接线）。
> 状态：**Release|x86 编译通过；真机验证通过（2026-10-23，用户人工核对）**。
> 口径：GUI 不要求还原 hmaker，只要求筛选、聚合口径与展示内容一致；不新增任何逆向地址。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 数据源 | `<thprac 数据目录>/logger_sessions_YYYYMM.jsonl`；一局一行，坏行/半截行静默跳过 |
| 数据目录 | `LauncherGetDataDir()`（支持便携模式 `.thprac_data\`，不硬编码 `%APPDATA%`） |
| 消费字段 | `game` / `start`（只取前 10 位日期）/ `checkpointCount` / `total`（可选）/ `events[].type`（只找 `interrupt`） |
| 练习量口径 | `total > 0` 取 `total`；否则 `checkpointCount`，含 `interrupt` 时减 1，下限 0（见 §2） |
| 筛选 | 游戏（当月数据中实际出现的作品 + 全部）、月份（目录中实际存在的 yyyyMM，倒序） |
| 分组 | 自然周（周一为首日），所选月覆盖的所有周都列出（含空周）；只列有记录的天 |
| 默认 | 当月；当月无文件回落最新月并提示；游戏=全部 |
| 刷新 | 仅手动；换月重读文件，换游戏只重算内存；游戏选择跨月保留 |
| 文案 | 新增 19 条 zh/en/jp locale 词条（含 7 个星期名）；复用 `THPRAC_GAMES`、`THPRAC_GAME_TIME_ALL` |

---

## 1. 数据源

- 文件：`LauncherGetDataDir() + "logger_sessions_" + YYYYMM + ".jsonl"`，每月一个，全作品共享。
- 月份列表：`FindFirstFileW("logger_sessions_*.jsonl")`，严格校验前缀 + 6 位数字 + `.jsonl`，倒序去重。
- 读取：`LoadJsonFile()` 一次读入内存，按 `\n` 切行、去尾部 `\r`/空白；每行直接以「指针 + 长度」交给 rapidjson（logger 行可能超过 256 字节，不能用定长缓冲）。
- 必需字段：`game`(string) / `start`(string) / `checkpointCount`(int)。`start` 前 10 位必须是合法 `YYYY-MM-DD`；不合法则丢行。
- `total` 仅在其为 `>0` 的整数时生效；`events` 仅在 `total <= 0` 时扫描，找 `type == "interrupt"`。

## 2. 练习量口径

hmaker 写于 `total`/`got` 字段之前，原口径只累加 `checkpointCount`；thprac 现在会把连续的单章节练习合并成一行（`total=N`、`checkpointCount=1`），照搬会明显低估。故本页口径为：

```text
amount(record) =
    total                      , total > 0
    max(0, checkpointCount - 1), total <= 0 且含 interrupt
    checkpointCount            , 其余
```

- 多个 `interrupt` 也只减 1（一份记录只对应一次退出）。
- `total > 0` 优先，即使 `events` 里也有 `interrupt`（合并行本身 `events` 为 `[]`；单章节首条 `total=1` 时与 `checkpointCount-1` 等价）。

| 例子 | amount |
| --- | --- |
| `total=6, got=3, checkpointCount=1, events=[]` | 6 |
| `total=1, checkpointCount=2`（单章节首条） | 1 |
| 无 total，`checkpointCount=6`，含 interrupt | 5 |
| 无 total，`checkpointCount=6`，无 interrupt | 6 |
| 无 total，`checkpointCount=1`，含 interrupt | 0 |
| 无 total，`checkpointCount=0` | 0 |

## 3. 聚合与展示

| 量 | 定义 |
| --- | --- |
| 本月总和 | 所选月文件 ∩ 所选游戏 的 `amount` 之和 |
| 日总和 | 按记录 `start` 的日期分组求和；只列有记录的天 |
| 周总和 | 自然周（周一为首日）；周列表 = 所选月 1 日到月末覆盖的所有周一 ∪ 记录实际所在周，升序；空周显示「无记录」；不跨月文件补天数 |
| 记录条数 | 过滤后的记录数 |
| 天数 | 有记录的不同日期数 |
| maxDaily | 过滤后日总和最大值，比例条分母 |

展示顺序（ImGui）：刷新按钮 + 当前月份 → 「本月总和」+ 千分位数字 + `N 条记录 · M 天有记录 · YYYY-MM` → 游戏/月份下拉（当月无文件时附回落提示）→ 周分组（周头 `YYYY-MM-DD~MM-DD` + `周总和 N`；空周「无记录」；日行 `MM-DD 周X`、`N 条`、日总和、比例条）。

空态：无记录文件 / 读取失败 / 该月暂无记录 / 当前筛选无记录。

## 4. 与 hmaker 的差异

| 项 | hmaker | 本页 |
| --- | --- | --- |
| 数据目录 | 硬编码 `%APPDATA%\thprac` | `LauncherGetDataDir()`，支持便携模式 |
| 游戏列表 | 固定 th14–th18 | 当月数据实际出现的作品，按 `gGameDefs` 顺序，未知 id 排后 |
| 练习量口径 | `Σ checkpointCount` | `total` 优先 + `interrupt` 减 1（§2） |
| 文案 | 硬编码中文 | zh/en/jp locale |
| 星期名 | 中文 | zh/en/jp locale |

## 5. 已知限制

1. `eventsDropped > 0` 且被丢弃的事件里含 `interrupt` 时，无法察觉，该行会少减 1。
2. 非法 `start` 的行直接丢弃（hmaker 仍计入月总和）；属损坏数据场景，换来「月总和 == Σ日 == Σ周」恒等式。
3. 读盘同步执行；单月文件到数 MB 时可能卡一帧，暂不引入后台线程。
4. 游戏选择跨月保留：切到不含该作品的月份会显示「当前筛选无记录」，不会自动重置为「全部」。

## 6. 验证

1. `MSBuild`：`msbuild thprac.sln -t:restore,build -p:RestorePackagesConfig=true,Configuration=Release`。
2. 运行 `Release\thprac.exe` → 数据 → 练习量。
3. 用独立 Python 按 §2/§3 的公式读同一文件，逐项核对月总和、记录条数、天数、maxDaily、每个周总和与日行。
4. 逐项点击游戏/月份/刷新，确认换游戏不读盘、换月重读、游戏名称与顺序正确。
5. 切换 English / 日本語 重启 launcher，确认标签、星期名、空态文案。

**结果（2026-10-23）**：`Release|x86` 编译通过；用户人工打开「数据 → 练习量」核对，月总和、记录条数、天数、周分组与日明细均符合预期。

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-23 | 建档：从 hmaker `th_practice_amount` 移植数据内容到 launcher「数据 → 练习量」；确定练习量口径为 `total` 优先 + `interrupt` 减 1；游戏列表数据驱动；新增 19 条 locale 词条。 |
| 2026-10-23 | `Release\|x86` 编译通过；真机人工核对通过（月总和、记录条数、天数、周分组、日明细）。状态改 ✅。 |

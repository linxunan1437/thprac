
## 需求内容
我需要在exe内部的ecl引擎执行时, 获取当前正在执行的ecl函数名称, 并做一些纪录. 具体需求如下:
### 功能范围
限定th06-th18的整数作. 其中th09不需要支持.
### 日志记录功能
1. 每一次启动游戏(包括正常启动/thprac内部菜单启动), 增加一条json日志, 记录其起始时间, 启动位置(thp启动可直接获取其启动位置), 选择的机体类型(thprac目前已有显示, 可以找一下).
2. 启动后, 当miss/bomb时, 记录当前关卡位置(具体位置见监控功能)
3. 退出游戏时, 记录其最后一个checkpoint的位置到json, 完成该日志记录并落盘保存(追加写入文件/新建文件, 见4)
4. 日志文件每一个月一个文件, 保存在thprac配置文件那里. 本月若已有文件, 则在这个文件后面追加写入.
5. 每一次游玩本身为一个json对象, 记录起始时间, 启动checkpoint, 最后checkpoint, 经过的checkpoint数量, 结束checkpoint, miss/bomb的列表.
6. 所有的checkpoint均为字符串.

### 监控功能
thprac支持的所有功能范围内游戏都有一个ecl引擎, 其引擎会顺序执行脚本(部分地方存在分支, 但函数模块层面依然是单向顺序执行, 不存在向后跳转). 需要监控其当前正在执行的ecl脚本函数, 并获取其名称
1. 我有一个ecl脚本函数白名单csv, 名单内包括了需要监控的函数名称, 所属游戏, 所属关卡, 以及其在日志记录功能中需要记录的名称.
2. 启动游戏后, 先加载csv, 获取到当前游戏和关卡需要监控的函数名称.
3. 监控过程中, 先检测当前函数是否在当前白名单函数内. 若在, 则把「当前段落」置为该函数对应的记录名(覆盖上一个值); 若不在, 则 no-op(保持当前值不变, 不回退 main). 本局首次命中任何白名单函数之前, 当前段落显示/记录为 `main`.
4. 这个函数在imgui里显示, 显示位置是当前的miss/bomb显示位下一行. 仅当miss/bomb数显示的功能开启时, 顺带加上它即可.
5. 所有白名单函数, 都被视为日志记录功能中的checkpoint, csv中会有用于日志记录的名称.

## 逆向工程相关
### 逆向思路
有一个大致的逆向工程思路:
1. 拿到当前的引擎执行指令指针
2. 走指针获取到地址段, 进而追踪其所属的ecl函数
th14.cpp中可能已经有了类似的hook, 可以检查一下作为复用.
### 引擎相关
1. th06-08是第一代引擎, ecl格式与其他作不太一致, 可能不适用于同一类监控功能. 且三作的stage4均存在路线分支.
2. th10-11是第二代引擎, 其中th10不存在剧情分支, th11的stage4存在分支
3. th12是第三代引擎, 无剧情分支
4. th13-18是第四代引擎, th14的stage4, th16的stage5存在分支

## 注意事项
1. 需要检查本机类型, 若本机为macos, 无法进行编译-验证工作, ide也无法识别windows.h, 就不做实际编码工作, 只出反编译结论, 和开发方案.
2. 我只比较熟悉纯C开发和kotlin/dart这种语言的开发, 不怎么熟悉cpp, 有些cpp特有特性需要向我解释一下.



## 落地口径（2026-10-17 定案）

> 本节的目的是把上面需求里留了自由度的地方**钉死**，供实现与后续迭代引用。**不改动上方原有需求文字。**
> 实现记录：`docs/logger/logger_impl_report.md`；进度：`docs/logger/logger_progress.md`。

| # | 项 | 定案 |
| --- | --- | --- |
| 1 | 文件格式 | **JSONL**：一行一局，纯追加，不读回改写（与既有 `gametime_sessions.jsonl` 同族）。**唯一例外**：单章节练习合并会就地改写**最后一行**（见口径 11）——仍不读回文件、不跨进程 |
| 2 | 文件位置与命名 | `<thprac 数据目录>logger_sessions_YYYYMM.jsonl`，**每月一个**，五作共享、靠 `game` 字段区分 |
| 3 | 一局边界 | 开始 = `*_game_start` 钩子（含重开）；结束 = `PLAYER_PTR` 由非 0 变 0。**该判据直接复用各作 IGI 面板的显隐判据**，不新增逆向 |
| 4 | 强退进程 | 视为**主动放弃记录**，不处理 |
| 5 | 「启动位置」的定义 | = **启动 checkpoint**：练习模式选择的起始关卡与章节/boss 阶段。取值为**本局首次 latch 到的记录名**（练习 warp 时 ECL 直接跳到落点，故首次 latch 即练习选的段落）；非练习启动则为首个命中白名单的段落 |
| 6 | checkpoint 字段 | 只有「启动 checkpoint」与「结束 checkpoint」两个（§5 的「最后checkpoint」与「结束checkpoint」合并；正常情况两者相同） |
| 7 | 记录名口径 | 全部取 CSV 第 4 列「记录名」；本局首次命中白名单函数之前记 `main` |
| 8 | 覆盖范围 | th14 / th15 / th16 / th17 / th18（其余作监控未落地） |
| 9 | 机体类型 | 取该作 IGI 面板 `diff_pl` 行显示的机体串；**th18 留空**（其 IGI 无该行、地址未登记） |
| 10 | 事件类型 | `miss` / `bomb` / `break` / `interrupt`；`break` = **撞咆哮**，**仅 th17** 产出（`th17_roar_break` `0x40F880`，与 IGI 面板 `THPRAC_INGAMEINFO_17_ROAR_BREAK_COUNT` 同源）；`interrupt` = **未通关就退出游戏**（口径 12），**th14–th18 五作都产出**（五作均有 `spellFlag` 抑制，见口径 15） |
| 11 | **单章节练习合并**（2026-10-18 定案） | 「单章节」的判据**已于 2026-10-18 由口径 13 取代**（原文为 `(checkpointCount == 1 && eventCount > 0) \|\| (eventCount == 0 && (checkpointCount == 1 \|\| checkpointCount == 2))`，其 `eventCount` 语义见口径 13）。判定只用共享字段、**与具体游戏无关 ⇒ th14–th18 五作都生效、默认开启**。<br>**首条**：单章节记录首次写入时 `total = 1`、`got = (无 miss/bomb/break ? 1 : 0)`（`got` 的定义见口径 13）。<br>**合并**：下一条若**同日**（比 `start` 的年月日）**且**起始 checkpoint 相同**且**也是单章节 ⇒ **不新增行**，就地改写上一行：`total + 1`，且该条无 miss/bomb/break 时 `got + 1`。**其余字段一律保留首条的值**（`start`/`shot`/`endCheckpoint`/`checkpointCount`/`events`）。<br>**另起一行**：跨日 / 跨月 / 起始 checkpoint 变化 / 出现非单章节记录。<br>**跨进程不合并**：基线是纯内存态，进程重启即失效（不读回文件最后一行）。<br>**字段可见性**：`total`/`got` **仅在单章节行出现**（`total == 0` 时整对省略）——所有游戏的**非单章节**记录都不带这两个字段。 |

| 12 | **stageClear / interrupt**（2026-10-18 定案，**th14–th18 五作**） | **置位**：关底 boss 的「死 / 逃」收尾 sub 当前正在执行 ⇒ `stageClear = true`。判据复用**既有的每帧 sub 名采样**（与中boss 收尾 `IsMidBossEndSub` 同位置、同机制），**不新增任何 hook、不新增任何地址**；名字**逐作**从该作 `reasm/exes/ecl_dump/thXX/*.decl` 全量枚举后**精确匹配**（各作清单与排除理由见 `logger_impl_report.md` §4.4，**不可跨作外推**）。<br>**复位**：① **段落入口函数切换**（`mSection` 变成另一个记录名）的那一帧；② 换关（`STAGE_NUM` 变化）；③ 新局（`OnStageStart`）。**不能用「任一白名单命中即复位」**——boss 战期间 boss 宿主自己就停在白名单 sub（`Boss1`/`BossCard2`…）上，那样收尾 sub 那一帧会被同帧命中当场抹掉。<br>**同帧冲突**（收尾 sub 与段落切换同帧）：**以通关为准**（置位在复位之后执行）。<br>**事件触发**：`stageClear == false` 且**退出游戏**（沿用口径 3 的 `PLAYER_PTR` 由非 0 变 0 下降沿）⇒ 记一条 `interrupt`，`checkpoint` = 退出时刻的「当前段落」（与 miss/bomb 同口径）。必须在该帧 `GameSession::Tick()` **之前**入队——Tick 会在同一帧把本局快照转待落盘并写盘。<br>**与口径 11 的关系**：`interrupt` **计入** `eventCount`，故单章节判据与 `got` 的定义由**口径 13** 一并修订。 |

| 13 | **单章节判据修订 + `got` 无视 interrupt**（2026-10-18 定案，**取代口径 11 的原判据**） | **起因**：口径 12 的 `interrupt` 让 th16 的每个练习局都至少带一条事件 ⇒ 口径 11 原判据的 `eventCount == 0` 分支变成死代码，且 `got` 会被打成恒 0。<br>**「单章节」新判据**（两个子句，`hasAttempt` = 含 `miss`/`bomb`/`break` 中任一，`hasInterrupt` = 含 `interrupt`）：<br>① `checkpointCount == 2 && hasInterrupt` —— 只 latch 了 2 段且被打断；<br>② `checkpointCount == 1 && hasAttempt` —— 只 latch 了 1 段且在首段内就出事。<br>**不算单章节**：`checkpointCount == 1 && 只有 interrupt`（warp 进去、没 miss 就退到菜单）⇒ 每次单独占一行；以及 `checkpointCount ≥ 3`。<br>**`got` 无视 interrupt**：`got` = 本行里**无 `miss`/`bomb`/`break`** 的尝试数（interrupt 不影响 `got`）——恢复口径 11 对 `got` 的原始定义。<br>**跨作影响（2026-10-18 定案接受）**：不产出 `interrupt` 的 th14/15/17/18，子句 ① 永不成立 ⇒ 它们「干净走完两段（`checkpointCount == 2` 且无事件）」的练习局**不再合并**，会另起一行。子句 ② 对四作等价于原 `checkpointCount == 1 && eventCount > 0`，行为不变；`got` 对四作等价于原 `eventCount == 0`，行为不变。 |

| 14 | **合并行规范化**（2026-10-18 定案） | 对**满足口径 13 单章节判定、且已被合并的行（`total > 1`）**：JSON 里 `checkpointCount` **固定输出 1**、`events` **固定输出空数组 `[]`**（**键保留**，只是不记事件）；`eventsDropped` 一并归零（events 都不记了，丢弃数无从谈起）。<br>**首条（`total == 1`）不规范化**：保留真实的 `checkpointCount`（1 或 2）与 `events` —— 那时 events 就是该次尝试的真实明细，不存在「只反映首条、不代表整行」的误导问题。<br>**理由**：合并行的 `events` 只反映**首条**尝试、不代表整行（口径 11 的必然推论），与其误导不如不记；`checkpointCount` 同理（首条是 1 还是 2 对汇总行没有意义）。<br>**其余字段不变**：`start`/`shot`/`startCheckpoint`/`endCheckpoint`/`total`/`got` 照旧。 |

| 15 | **spellFlag 计数器**（2026-10-20 定案，**th14–th18 五作**） | **目的**：修「某些 `BossCard` checkpoint 在卡结束后不立刻切换、退出时被误记 `interrupt`」；`BossDead`/`BossEscape` 覆盖不到卡内结束。**只关心「符卡结束」，不区分收取/失败**。<br>**检测（零新 hook）**：每帧读符卡对象（单例）flags 的 **bit0**（符卡存活位）的 **1→0 下降沿** ⇒ `spellFlag++`。`spell`/`spell3`/`spellEx` 共用该对象；声明函数置位、`spellEnd()` 清位。逐作地址/偏移**不可跨作外推**：th14 `0x4DB534+0x7C`、th15 `0x4E9A70+0x78`、th16 `0x4A6DB0+0x78`、th17 `0x4B7690+0x78`（复用既有 `SPELLCARD_PTR`）、th18 `0x4CF2C0+0x78`；置/清位点与逐作证据见 `logger_impl_report.md` §4.4.2.1。<br>**采样不会漏**：各作 `setNext` 链中每张符卡后都接一个非符攻击阶段（th14 唯一的 card→card `st05bs BC3→BC4` 也夹着 160 帧非符过渡）⇒ 一帧一次采样能稳定看到下降沿。<br>**消费**：`mSection` 换到新 checkpoint（沿用 `prevSection` + `strcmp` 判据）⇒ `spellFlag--`（下限 0）。`++` 在 `Update()` 顶部、`--` 在段落裁决之后 ⇒ 同帧「卡结束 + 新 checkpoint」净 0。<br>**interrupt**：退出下降沿到来时 `spellFlag > 0` ⇒ 不记 `interrupt`。`stageClear` **保留为兜底**——关底 `BossDead` 的 `+60` 帧窗口与非符期 `BossEscape` 没有卡结束事件，五作的最终条件均为 `spellFlag == 0 && !stageClear`。<br>**复位**：新局 / 换关 / 符卡对象重建（对象指针变化）⇒ `spellFlag = 0`；对象为空（菜单 / 销毁中）只清采样状态、**不清计数**（退出帧还要用）。<br>**已知副作用**：CSV 剔除的过渡非符会让计数滞留（各作窗口不同），期间 `interrupt` 为**假阴性**；th15 另有第二处强制清位点（`0x420EDA`，靠三处复位吸收假下降沿）。登记 `knownBugs.md` #12。 |

**一行 JSON 示例**（字段说明见实现报告 §2）：

```json
{"game":"th16","start":"2026-10-17 21:03:44","shot":"ReimuA","startCheckpoint":"ST1 前半 #1","endCheckpoint":"ST3 BOSS1","checkpointCount":11,"events":[{"type":"bomb","checkpoint":"ST1 后半 #2"},{"type":"miss","checkpoint":"ST2 BOSS2"}]}
```

> 本迭代**不含**：th18 的机体类型、强退进程落盘、日志文件读回、th10–th13 与 th06–th08 的接线。

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-18 | 建档本表（此前改动未逐条记录）。口径 1 加「就地改写最后一行」例外；新增**口径 11「单章节练习合并」**：判定式、`total`/`got` 语义、合并/改锚规则、跨日跨月与跨进程边界、字段可见性（仅单章节行输出）。 |
| 2026-10-18 | 口径 10 事件类型补 `interrupt`（本迭代仅 th16）；新增**口径 12「stageClear / interrupt」**：置位判据（每帧 sub 名采样 + 枚举精确匹配，含 `BossDeadB` 同名歧义的处理）、三条复位判据、同帧冲突取向、事件触发时序（Tick 之前入队）、与口径 11 的相互作用（计入 `eventCount`）。 |
| 2026-10-18 | **口径 12 推广到 th14/15/17/18**（原为 th16 单作）：逐作枚举 `reasm/exes/ecl_dump/thXX/*.decl` 得到各自的关底收尾 sub 清单（**不可跨作外推**），各作 9 处同构接线；共享层零改动。口径 10 的「仅 th16 产出 `interrupt`」改为**五作都产出**。逐作清单与排除理由见 `logger_impl_report.md` §4.4。**Release\|x86 编译通过；真机未测（🟡）**。 |
| 2026-10-18 | **新增口径 14「合并行规范化」**：`total > 1` 的合并行在 JSON 里 `checkpointCount` 固定 1、`events` 固定 `[]`（键保留）、`eventsDropped` 归零；`total == 1` 的首条不规范化。 |
| 2026-10-18 | **新增口径 13「单章节判据修订 + `got` 无视 interrupt」**，取代口径 11 的原判据：单章节 = `(cpCount==2 && 含interrupt) \|\| (cpCount==1 && 含miss/bomb/break)`；`got` 只数「无 miss/bomb/break」的尝试。起因是口径 12 的 `interrupt` 让 `eventCount == 0` 分支变成死代码、`got` 被打成恒 0。**定案接受跨作影响**：th14/15/17/18 的「干净走完两段」练习局不再合并。口径 11 的判据与 `got` 描述同步标注取代关系。 |
| 2026-10-20 | **新增口径 15「spellFlag 计数器」（仅 th16）**：符卡对象 `*(u32*)0x4A6DB0` 的 `+0x78` bit0 下降沿 ⇒ `spellFlag++`；`mSection` 换新 checkpoint ⇒ `--`（下限 0）；退出时 `spellFlag > 0` ⇒ 不记 `interrupt`。`stageClear` 保留为 `BossDead+60` / 非符期 `BossEscape` 的兜底，最终条件 `spellFlag == 0 && !stageClear`。**零新 hook、共享层零改动**。口径 10 同步补注。实现见 `docs/logger/logger_impl_report.md` §4.4.2，副作用见 `knownBugs.md` #12。**本机无编译环境，未编译、未真机（🟡）**。 |
| 2026-10-20 | **口径 15 推广到 th14/15/17/18，五作齐**：逐作定位符卡单例与存活位（th14 `0x4DB534+0x7C`、th15 `0x4E9A70+0x78`、th16 `0x4A6DB0+0x78`、th17 `0x4B7690+0x78`、th18 `0x4CF2C0+0x78`），9 处接线逐字同构（`SampleSpellCard()` + `mSpellFlag`），`mStageClear` 五作均保留兜底。th15 另有一处重开/载入强制清位（`0x420EDA`），靠对象指针变化 / 换关 / 新局复位吸收。口径正文改为五作口径，逐作证据见 `docs/logger/logger_impl_report.md` §4.4.2 / §4.4.2.1 与各作 `docs/thXX/thXX_spell_flag_report.md`。**零新 hook、共享层零改动**。**本机无编译环境：未编译、未真机（🟡）**。 |




# logger 已知问题清单（活文档）

> 上位文档：`docs/logger/logger_progress.md`（进度）、`docs/logger/checkpoint_entry_backfill_workflow.md`（入口回填口径）。
> 用途：登记 checkpoint 白名单 / 监控落地过程中**已知但暂不修**的问题，供后续迭代收口。
> 优先级：P0 阻塞功能 ｜ P1 影响正确性、需尽快修 ｜ **P2 已知、可接受、暂不修**。

---

## 1. 问题清单

| # | 优先级 | 作 | 位置 | 现象 | 影响 | 处置 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | **P2** | th15 | `ST7 前半 #5` / `ST7 后半 #1` | 两行入口同名，均为 `MainSub04` —— `st07` 的 `MainFront()` 末尾与 `MainLatter()` 开头**都** `call @MainSub04()`（偏移 `0x5f30` / `0x5f84`） | 白名单按 sub 名匹配，两个 checkpoint 无法区分 → 二者必有一行**不可达**（实际只会命中同一个记录名） | 暂不修。thprac 章节 warp 数据固有；该关暂不使用 |
| 2 | **P2** | th14–th18 | 日志落盘的 `events[].checkpoint` | miss/bomb 钩子在 ECL tick 中触发，而 `mSection` 是**上一帧** `Logger::Update()` 的结果 | 刚好在「进入新段落的同一帧」miss 时，事件会记成**上一段**（一帧延迟，约 16 ms） | 暂不修。窗口极窄；真机对照 IGI 面板可见 |
| 3 | **P2** | th14–th18 | 日志落盘的 `events` 数组 | `GameSessionData::events` 是定长 256 项（`thprac_games.h`），溢出即丢弃并计 `droppedEvents` | 长局 + 关闭 FastRetry 反复 miss 时可超 256，JSON 里出现 `eventsDropped` 且尾部事件缺失 | 暂不修。练习模式每次 miss 就 FastRetry 重开一局，实际远小于 256 |
| 4 | **P2** | th18 | 日志落盘的 `shot` 字段 | th18 的 IGI 面板无 `diff_pl` 行，机体/子机地址也未登记（总括 §6 Q7 只填了 th14） | th18 每局 `shot` 恒为空字符串 | 暂不修。需补 th18 的机体地址逆向；不影响其余字段 |
| 5 | **P1** | th14–th18 | 日志落盘的「一局」边界 | `*_game_start` 钩子是「开新局初始化」代码（score/graze/power 归零、bomb 置 3），静态上指向「只在新局/重开触发」，但**未真机钉死**是否也在 ST→ST 切关时触发 | 若每关触发，一个全关会产出多行而非一行，与「一局」口径冲突 | **待真机验证**（`logger_impl_report.md` §7 验证项 3）。修法：`Begin()` 加「仅当上一帧不在局内才开新局」一行 guard |
| 6 | **P2** | th14–th18 | 日志落盘的**就地改写**（口径 11） | `SetFilePointerEx`/`WriteFile`/`SetEndOfFile` 失败（磁盘满 / IO 错误）时，上一行可能被**部分覆盖**，随后追加会产出「撕裂行」 | 该行 JSON 解析失败、计数错乱 | 暂不修。只在磁盘满/IO 错误时出现；「统计绝不干扰游玩」优先，静默接受 |
| 7 | **P2** | th14–th18 | 日志落盘的**单章节合并基线**（口径 11） | 基线是**纯内存态**，进程重启即失效（口径 11 明确定为不读回文件最后一行） | 关掉游戏重新启动后，同日同 section 再练会**另起一行**，`total` 从 1 重新计 | 暂不修。要跨进程合并需读回并解析文件最后一行，代价大于收益 |
| 8 | **P2** | th16 | `IsStageClearSub` 的 `BossDeadB` | 该名字在 `st05bs` 是**关底 boss** 收尾、在 `st07mbs2` 是**中boss** 收尾（`enmCall($GI0,"MBossDeadp")` + `@MBossBDeadp()`），名字无法区分；且 `IsMidBossEndSub` 恰好用前缀 `BossDeadB` 认中boss，两条谓词同名冲突 | 若收录它，ST7 中boss B 路线击破会被误判成「本关已清」⇒ 该局退出时**漏记** interrupt | **已处理**：`IsStageClearSub` 排除它，`strcmp` 精确匹配其余 5 个名字。排除不损失覆盖 —— `st05bs` 侧由 60 帧的 `BossDeadBp` 兜住（同文件的 `BossDead`/`BossDeadB` 是 0 帧转发壳） |
| 9 | **P2** | th16 | stageClear 的复位判据 | 复位挂在「`mSection` 变成另一个记录名」上；boss 宿主 `delete()` 之后，若仍有白名单命中的宿主 / 异步线程存活，`mSection` 会从 `"STn BOSSx"` **回退**到章节记录 ⇒ 触发一次复位 | 清关后立刻退到标题会**误报**一次 interrupt | 暂不修。静态看 th16 不成立（`stNN.decl` 的 `main()` 在 boss 战期间阻塞在**不在白名单**的 `MainBoss`；`MainLatter` 的 `@MainSub05() async` 约 700 帧即结束，远早于 `@MainBoss()`）。真机若复现，最小修法是「收尾后 N 帧内忽略复位」。**口径 15 后同样适用于 `spellFlag`**：段落回退会多消耗一次 `--` ⇒ 仍可能误报 interrupt |
| 11 | **P2** | th15 | `IsStageClearSub` 的 `BossDeadB`（口径 12） | `st07bs2.BossDeadB` 的 body 与 `st07bs.BossDead` **形状完全一致**（`enmKillAll()` → `+60:` → `setBoss(-1)` → `delete()×2`），静态**无法**从 body 区分「关底收尾」与「附属身体收尾」 | 若 `BossB` 其实是 ST7 的**独立路线**（而非附属身体），则清 B 路时权威收尾 `BossDead` 不跑 ⇒ 每次清 B 路都**误记**一条 `interrupt` | **暂不收录**（静态判为附属身体：`st07.decl:968/969` 两条 `enmCreateA("Boss","BossB")` 之间无条件跳转 ⇒ 同时在场；th15 的 ST7 白名单也无任何 B 路线记录）。**真机确认：清完 ST7 后退出应无 `interrupt`**（`logger_impl_report.md` §7 验证项 20）。若复现，把 `BossDeadB` 加进 th15 清单即可 |
| 10 | **P2** | th14–th18 | 单章节判据跨作不对称（口径 13 的代价） | 口径 13 的判据 `(cpCount==2 && 含interrupt) \|\| (cpCount==1 && 不是「只有interrupt」)`（子句② 已由口径 16 修订）里，第一个子句依赖 `interrupt`；口径 15 之前 th14/15/17/18 不产出 `interrupt`，口径 15 之后五作都产出、但都能被 `spellFlag` 抑制 ⇒ 本条触发面已缩小 | 四作「干净走完两段（`cpCount==2` 且无任何事件）」的练习局**不再合并**，改前会合并 ⇒ 四作的日志行数变多 | **用户 2026-10-18 定案接受**（判据由共享层统一，不按作分叉）。若日后要恢复，最小改法是给第一个子句加一条「无任何事件」的旁路：`cpCount==2 && (含interrupt \|\| eventCount == 0)`（**口径 16 未采纳，只动了子句②**） |
| 12 | **P2** | th14–th18 | `spellFlag` 的滞留窗口（口径 15） | `--` 只在 `mSection` 换到新记录名时发生，而各作 CSV 都刻意剔除了部分过渡非符（th16：ST2 `Boss3`、ST6 `Boss6`、ST7 `Boss9`/`Boss10`、ST5 `Boss4`；th18：ST2 `Boss3`、ST6 `Boss5`/`Boss6`、ST7 `Boss9`、ST7mbs `MBoss2`/`MBoss3`；th17：ST2 `Boss3`、ST5 `Boss4`、ST6 `Boss5`–`7`、ST7 `Boss9`/`Boss10`；th14：`Boss3`/`5`/`6`/`9`/`10` 等）⇒ 卡结束到下一个 checkpoint 命中之间 `spellFlag` 一直 > 0 | 这些窗口内退出**不记** `interrupt`（假阴性）；窗口从数秒到 90 s+；中boss 清卡后退出也不再记。th15 另有两处「只有 `spellMode`、无符卡对象」的末阶段（ST2 `MBossCard1`、ST7 `st07bs2 BossCardB10`），不产生事件（也不在 CSV），靠 `mStageClear` 兜底 | 暂不修。「卡刚结束、checkpoint 未跟上」本就不该记 interrupt。若要缩短窗口，最小改法是「新卡开始（bit0 0→1）时也 `--`」，会偏离用户给定的计数器语义，需先确认。th15 另有重开/载入强制清位点 `0x420EDA`，可能产生假下降沿，靠对象指针 / 换关 / 新局复位吸收 |

---

## 2. 说明

**#1（th15 ST7）详述**

- 口径：CSV 是**路过式 checkpoint（latch）白名单**——运行期读「当前 ECL sub 名」，命中表内函数即置「当前段落」。
- 问题：`MainSub04` 在 thprac 的 warps 表里对应**两个**不同的记录名（`ST7 前半 #5` 与 `ST7 后半 #1`），但运行期两者是**同一个 sub**，白名单无法二选一。
- 现状：Csv 中两行都填了 `MainSub04`（机械规则所致），实际效果是「命中 `MainSub04` 时只会把段落置为后出现的那一行」。
- 修法（未做）：需 CSV schema 支持「同名入口 → 多记录名」的消歧（如加一列区分条件），或运行期结合额外状态（关卡进度 / 前一 sub）判段。
- 参考：`docs/logger/checkpoint_entry_backfill_workflow.md` §1（latch 口径）、§5（边界）。

**#2–#4（日志落盘）详述**

- 口径与实现见 `docs/logger/logger_impl_report.md`。三条都是**已知可接受**的边界，不影响主链路：
  - **#2**：事件记录的是「钩子触发时刻的当前段落」，而当前段落由每帧 `Update()` 维护，天然有一帧滞后。要在同一帧内消除需把 latch 提前到 tick 内，代价大于收益。
  - **#3**：定长数组是为了满足「热 hook 内零堆分配」（`feat/plogth15` 的崩溃前车之鉴）。溢出时**不静默**：JSON 里会带 `eventsDropped`。
  - **#4**：th18 无现成机体来源；若要补，需按 `docs/gametime/` 的探针流程新增逆向，属独立任务。

**#5（一局边界）详述**

- 现状：`Begin()` 在 `*_game_start` 钩子里**无条件**开新局（若上一局还开着，先把上一局转为待落盘）。
- 静态证据：该钩子所在例程把 score(`0x4A57B0`)、graze(`0x4A57C0`)、power(`0x4A57E4`) 归零、bomb 置 3 —— 若每关都跑，跨关分数会被清空，故语义上是「开新的一局」。但**未真机钉死**。
- 验证：真机 th16 练习从 ST1 打到 ST2，看月度文件里出现 1 行还是 2 行（`logger_impl_report.md` §7 验证项 3）。
- 若确认每关触发，修法（一行）：`Begin()` 里把「无条件开新局」改成「仅当上一帧不在局内（`!mWasInGame`）才开新局」，否则只做段落复位。

**#6–#7（单章节合并）详述**

- 口径与实现见 `docs/req_logger.md` 口径 11、`logger_impl_report.md` §3.1 / §5.1。两条都是**已知可接受**的边界：
  - **#6**：改写失败分两类 —— `GuardFailed`（打不开 / size 守卫不匹配）**一字节未动**，优雅退化成追加新行；`IoFailed`（`SetFilePointerEx`/`WriteFile`/`SetEndOfFile` 失败）则可能留下「旧行前半 + 新行后半」的撕裂行。只有磁盘满 / IO 错误能触发，此时用户本来就会先撞上别的问题。要根治需「写临时文件再原子替换」，对每 10–20s 一次的热路径不划算。
  - **#7**：基线放在 `GameSession` 的私有成员里，进程一退就没了。要跨进程必须**读回文件最后一行并解析**（`game` / `start` 日期 / `startCheckpoint` / `total` / `got` / 字节偏移），而 `req_logger.md` 口径 1 明确「不读回改写」——本次只开了「改自己刚写的那一行」这一个口子，不扩大到解析既有行。

---

## 3. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档。登记 th15 `ST7 前半 #5` / `ST7 后半 #1` 入口同名（`MainSub04`）问题（P2，暂不修）。 |
| 2026-10-17 | 追加日志落盘相关的 4 条：#2 事件 checkpoint 一帧延迟（P2）、#3 `events` 上限 256（P2）、#4 th18 `shot` 留空（P2）、#5「一局边界是否等于一关边界」待真机（P1）。 |
| 2026-10-18 | 追加单章节练习合并（口径 11）相关的 2 条：#6 就地改写 `IoFailed` 时的撕裂行残留（P2）、#7 合并基线不跨进程（P2）。 |
| 2026-10-18 | 追加 stageClear / `interrupt`（口径 12，仅 th16）相关的 3 条：#8 `BossDeadB` 的同名歧义（已按排除处理）、#9 关底收尾后 `mSection` 回退导致误复位（静态不成立，未真机）、#10 `interrupt` 计入 `eventCount` 导致单章节练习的 `got` 语义变化。 |
| 2026-10-18 | #10 随**口径 13** 改写：判据已修订（`got` 不再受 `interrupt` 影响），剩余项改为**跨作不对称**——th14/15/17/18 的 `cpCount==2` 干净练习局不再合并（定案接受，附旁路改法）。 |
| 2026-10-18 | 口径 12 推广到 th14/15/17/18 后追加 **#11**：th15 的 `BossDeadB` 判定是五作里**唯一未能静态钉死**的一处（body 与关底收尾形状一致，但静态判为附属身体），登记待真机确认。 |
| 2026-10-20 | 追加 **#12**（th16 `spellFlag` 的滞留窗口与 ST5 B 兜底，P2）；#9 补注：段落回退会多消耗一次 `--`，对 `spellFlag` 同样成立。 |
| 2026-10-20 | **#12 扩到 th14–th18 五作**：补 th14/15/17/18 的滞留窗口与非符缺口（th15 ST2 `MBossCard1`、ST7 `st07bs2 BossCardB10`；th18 ST2/ST6/ST7mbs 非符未登记等），并补记 th15 的第二清位点 `0x420EDA`。 |
| 2026-10-20 | #10 的判据字符串随**口径 16** 同步（子句② 改为「不是『只有 interrupt』」）；并补记口径 15 之后五作都产出 `interrupt` ⇒ 该跨作不对称的触发面缩小。**#10 的结论不变**（`cpCount==2` 的干净局仍不合并，定案接受）。 |

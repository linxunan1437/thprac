# TH10 ECL 段落监控 — 实施记录（监控 E5）

> 上位文档（术语 / 判定口径 / 粒度，一律以此为准）：`docs/logger/logger_re_disasm_overview.md`（**§1.2 粒度 / §2 术语表 / §5.1 锚点总表 / §6 Q9 / §9.9**）
> 需求来源：`docs/req_logger.md`（**监控** + **日志落盘**）。监控见 §1–§8；**日志接线（JSONL / `GameSession` / `OnMiss` / `OnBomb` / `interrupt` / `stageClear` / `spellFlag`）见 §9**（2026-10-20 追加；此前 §1–§8 中「本迭代只做监控、不做落盘」的表述是**监控迭代当时**的范围声明，已被 §9 取代）。
> 本作逆向结论：`docs/th10/th10_ecl_re_report.md`（Q1–Q9；**§1 指针链 / §2 `[T+4]` = 指令绝对地址 / §2.3 地址区间反查 / §3 sub 表 / §4.5 async 线程 / §5A boss 宿主可达性 / §10.4–§10.5 粒度**）
> 本作入口表：`docs/th10/th10_checkpoint_entry_report.md`（**109/109**；§2 口径与特例 / §3 实现影响）
> 偏移表与可行性：`docs/th10/th10_checkpoint_ip_offset.md`（**§3.1 18 行偏移表 / §5.1 boss 撞点 5 处 / §7 回填结论**）
> 白名单数据：`docs/logger/th10_checkpoints.csv`（**109 数据行** = 章节 41 + boss 68；**18 行**带 `+0x` 偏移后缀）
> 直接模板（**同形**：节点 `+4` = 指令绝对地址）：`thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（`:621-1130`）
> 参考实现（权威，真机验证过）：`docs/th16/th16_monitor_impl_report.md`（**§5.7 中boss 窗口** / §7 验证清单）
> 代码：`thprac/src/thprac/thprac_th10.cpp`（`TH10Logger` 模块，`:574-1021`）、`thprac/src/thprac/thprac_th10_checkpoints_gen.cpp`（生成物）
> 脚本：`docs/logger/tools/gen_th10_checkpoints.py`
> 二进制指纹：`../exes/th10.exe`，**487936 B**，PE `TimeDateStamp = 1186086096`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）
> 方法：引用已定案的逆向结论 + 逐处对照 `thprac_th14.cpp` / `thprac_th16.cpp` / `thprac_th10.cpp` + 离线枚举 `../exes/ecl_dump/th10/*.decl` 的中boss 收尾 sub。**未编译、未真机。**
> 状态：**静态定案（🟡 未编译、未真机三态）**。逐条标注 ✅ 有静态证据 ｜ 🟡 待真机。
> 地址口径：正文 `VA` 与 `RVA = VA − 0x400000` 并列；「偏移」一律指**相对该 sub 首条指令**的字节偏移（与 `th10_checkpoint_ip_offset.md` §7 同基准）。

---

## 0. 结论速览

| # | 项 | 结论 | 状态 |
| --- | --- | --- | --- |
| 1 | 产物 | `gen_th10_checkpoints.py`（新建）+ `thprac_th10_checkpoints_gen.cpp`（生成物，入库）+ `thprac_th10.cpp` 内 `TH10Logger` 模块 + 4 处调用；**工程登记由主代理统一做**（本迭代不碰 `.vcxproj`） | ✅ |
| 2 | 接口 | 仅 `OnStageStart()` / `Update()` / `CurrentSection()` 三个；`CurrentSection()` **永不为 null** | ✅ |
| 3 | 取值 | 遍历 **全局宿主链 `mgr+0x58` × 每宿主线程链 `H+0x1030`**，逐线程 `ip = *(u32*)(T+4)`，经**地址升序区间表**（`table[i].+4 + 0x10`）反查 sub 名 | ✅（静态） |
| 4 | 判定 | **路过式 checkpoint（latch）**：命中白名单 ⇒ 置值；未命中 ⇒ **no-op（绝不回退 `main`）**；首命中前显示 `main` | ✅ |
| 5 | 双匹配 | **sub 名 + ip 偏移**，**偏移命中优先于名字命中**；`+0x` 后缀在**解析期**拆成 `(sub 名, 偏移)`，运行期不再解析字符串 | ✅ |
| 6 | 偏移行数 | CSV **18 行**（`main` ×15 = st1–st3、`MainSub1` ×1 = `ST4 前半 #2`、`MainSub8` ×2 = `ST5 后半 #1/#2`）；`mOffsetRows` 是 **vector**（同一 sub 可多行） | ✅ |
| 7 | 中boss 窗口 | **必需**（`th10_checkpoint_entry_report.md` §3.4 记有 5 处 boss warp 撞章节锚点）。照 th14 机制：`MBoss*` 命中开窗、当前 sub 名命中收尾 sub 名关窗并补显示 `mPending` | ✅（静态）／🟡（真机） |
| 8 | 中boss 收尾 sub 名 | **精确匹配 `MBossDead` / `MBossEscape`**（离线枚举全部 7 份 `stageNN.decl` 的 `setBoss(-1)` 站点；**stage06 无中boss、stage05 无 `MBossDead`**） | ✅（离线） |
| 9 | `STAGE_NUM` 换算 | 内置 **`9 → ST7`**（与 th14 同法）；**th10 的 Extra 是否报 9 在现有文档里查不到** ⇒ **🟡 未证实** | 🟡 |
| 10 | 安全校验 | 逐级判空、`count ∈ (0, 8192]`、遍历硬上限（宿主 ≤ 1024、线程 ≤ 64）、**不缓存任何跨帧指针**（`mIpMapCount`/`mIpMapTable` 只作变化检测、不解引用） | ✅ |
| 11 | 边界 | **不做日志落盘**；不新增 hook；不动 `thprac_locale_def.*`；不加 `AdvancedGameOptions` 开关；不碰 `.vcxproj` / 其它作 / `wind` | ✅ |

**一句话**：th10 的监控 = **th14 的骨架**（宿主链 × 线程链 + `ip → sub` 区间反查）+ **th16 的中boss 窗口** + **`sub名+0x偏移` 偏移匹配**（偏移优先）；`res` 每宿主读 `H+0x102C`（≡ 全局 `*(u32*)(mgr+0x54)`，th10 所有宿主共用一份）。

---

## 1. 指针链（运行期，逐条复核自本作报告）

```text
mgr    = *(u32*)0x477704                     // 脚本/ECL 管理器（thprac 名 ENEMY_MANAGER_PTR）
node   = *(u32*)(mgr + 0x58)                 // 宿主链头；节点嵌在宿主内 H+0x116C = {+0 = H, +4 = next, +8 = prev}
host   = *(u32*)node
res    = *(u32*)(host + 0x102C)              // ★ 每宿主的 ECL 资源（构造时无条件写 = *(u32*)(mgr+0x54) ⇒ 与全局链恒等）
count  = *(i32*)(res + 8)                    // sub 总数（越界保护）
table  = *(u32*)(res + 0x8C)                 // sub 表；项 8B：+0 = 名字 char*、+4 = sub 数据基址
firstInsn(i) = *(u32*)(table + i*8 + 4) + 0x10   // ★ 该 sub 的首条指令（地址升序区间表的键）
entry  = host + 0x1030                       // 线程链头（哨兵嵌在宿主内）；{+0 = T, +4 = next, +8 = prev}
thread = *(u32*)entry                        // [+0] = 主线线程（≡ host+8），其余为 async 子线程
ip     = *(u32*)(thread + 4)                 // ★ 当前（下一条待执行）指令的【绝对地址】；0 = 线程已结束
name   = SubOfIp(table, ip, &index)          // upper_bound 反查：name = *(char**)(table + index*8)
```

- **`res` 一律按宿主读 `H+0x102C`**：报告 §5A.3 证实宿主构造器 `FUN_0040D830` @ `0x40DA11` **无条件**写 `H+0x102C = *(u32*)(mgr+0x54)`，故「按宿主读」与「读全局链」**恒等**；而 **async 线程对象只 `malloc(0x1024)`、没有 `res` 槽**（报告 §4.5）⇒ 必须按**宿主**（不是按线程）取 `res`。本实现按宿主读，与 th13/th14 同形。
- **必须遍历线程链**：th10 段入口常以 `@MainSubNN() async` 派生为**独立线程**（报告 §4.5）。本作 18 个偏移行里 **st1–st3 的 15 行在主线程**，**`ST4 前半 #2`（`MainSub1`）、`ST5 后半 #1/#2`（`MainSub8`）在 async 线程**（入口报告 §3.3）⇒ 只读 `H+0x1030` 会漏 3 行。
- **必须遍历宿主链**：中boss / 关底 boss 的 ECL 跑在**各自的宿主**里（报告 §5A.1：`mgr+0x58` 是**全局全宿主链**，含 boss；boss 宿主捕获点 `0x40ED16` 仅为可选加速）。只走链头宿主看不到 boss 行。
- **与 C 代（th15–th18）不可互搬**：C 代 `thread+4` = **sub 索引**；th10 是**指令绝对地址**（报告 §2 三处独立证据），必须做地址→sub 反查。与 **th14 同形**（`host+0x11CC` = res、`host+0x11D0` = 线程链头、节点 `+4` = ip），仅**链偏移不同**。

---

## 2. 改动清单

### 2.1 新增文件（2）

| 文件 | 内容 |
| --- | --- |
| `docs/logger/tools/gen_th10_checkpoints.py` | 由 `gen_th14_checkpoints.py` 复制，只改 5 处：`CSV_PATH` → `docs/logger/th10_checkpoints.csv`、`OUT_PATH` → `thprac/src/thprac/thprac_th10_checkpoints_gen.cpp`、`RAW_DELIM = "TH10CSV"`、命名空间 `THPrac::TH10`、符号 `kTh10CheckpointsCsv`。幂等；断言正文不含终止符 `)TH10CSV"`；**自校验「嵌入正文与 CSV 逐字节一致」通过**（`csv bytes: 3384  embedded bytes: 3384`；二次运行输出 `unchanged`） |
| `thprac/src/thprac/thprac_th10_checkpoints_gen.cpp` | **生成物（入库）**：`namespace THPrac { namespace TH10 { extern const char kTh10CheckpointsCsv[] = R"TH10CSV(…)TH10CSV"; } }` |

### 2.2 改动既有文件（1）

| 文件 | 行号区间 | 改动 |
| --- | --- | --- |
| `thprac/src/thprac/thprac_th10.cpp` | `:1-10` | 文件头补 include：`"..\3rdParties\rapidcsv\rapidcsv.h"` + `<algorithm>` `<cstdio>` `<cstring>` `<sstream>` `<string_view>` `<unordered_map>`（`<format>` 原有） |
| 同上 | `:574-576` | `extern const char kTh10CheckpointsCsv[];` 声明 |
| 同上 | `:578-651` | 匿名命名空间：链偏移常量、sub 表项常量、`ClassifySub()`、`IsMidBossEndSub()`、遍历硬上限 |
| 同上 | `:656-740` | `class TH10Logger`（`SINGLETON`）；接口 `OnStageStart()` / `Update()` / `CurrentSection()`；私有 `Entry` / `SubRange` / `OffsetRow` / 哈希器 / `LoadCsv()` / `BuildStageMap()` / `BuildIpMap()` / `SubOfIp()` |
| 同上 | `:742-1021` | 上述成员实现（含 `Update()` 的链遍历 + 双匹配 + 中boss 窗口裁决） |
| 同上 | `:1083-1086` | IGI 显示：bomb 计数行之后插 `ImGui::NextColumn(); ImGui::Text("当前段落"); ImGui::NextColumn(); ImGui::Text("%s", TH10Logger::singleton().CurrentSection());`（保持既有 `Columns(2)` 的「标签 / 值」成对结构；标签**硬编码中文**，不动 `thprac_locale_def.*`） |
| 同上 | `:3284` | `EHOOK_DY(th10_update, 0x449d0e, 3, {…})` 内，`TH10InGameInfo::singleton().Update();` **之前**加 `TH10Logger::singleton().Update();` |
| 同上 | `:3474` | `EHOOK_DY(th10_game_start, 0x41798C, 6, {…})` 的**既有 lambda 体内**加 `TH10Logger::singleton().OnStageStart();`（同址挂第二个钩子会覆盖第一个，见 `thprac_hook.cpp:223`） |
| 同上 | `:3540` | `THGuiCreate()` 内 `TH10InGameInfo::singleton();` 旁加 `TH10Logger::singleton();`（预构造，跑一次 `LoadCsv`） |

> **`enum addrs` 未改动**：所需地址 `ENEMY_MANAGER_PTR = 0x477704` 与 `STAGE_NUM = 0x474c7c` **均已在表内**；链偏移（`0x58` / `0x1030` / `0x102C` / `+4`）与 sub 表偏移照 th14 惯例放**匿名命名空间的 `constexpr`**（`kMgrHostHeadOff` / `kHostThreadListOff` / `kHostResOff` / `kThreadIpOff` / `kResCountOff` / `kResTableOff` / `kSubEntrySize` / `kSubDataOff` / `kSubInsnOff`），不新增枚举项。

### 2.3 明确未做

不碰 `thprac/thprac.vcxproj` / `.filters`（主代理统一登记）；**不编译**；不做日志落盘（不碰 `thprac_games.{h,cpp}`，不加 `GameSession` / `OnMiss` / `OnBomb` / `interrupt` / `stageClear` / `spellFlag`）；**不新增 hook**；不加 `AdvancedGameOptions` 开关；不动 `thprac_locale_def.h/.cpp`；不改其它作任何文件；不碰 `wind`。

---

## 3. 中boss 窗口口径

**为什么必需（不是可选）**：`th10_checkpoint_entry_report.md` §3.4 / `th10_checkpoint_ip_offset.md` §5.1 离线实测 **5 处** boss warp 把 ip 落到章节锚点上——`ST1 MID1` / `MID2_EN` / `MID2_HL` 的可观测 ip `0xafd8` 正是 `ST1 后半 #1` 的锚点；`ST4 MID1` 的 `0x14aa4` / `0xf930` 分别是 `ST4 前半 #1` / `ST4 后半 #1`。这与 th16 真机反馈的「增援章节」误报**同源**（th16 §5.7），且 **sub 名口径下同样会误判**。

**机制（照 th14，不改 CSV、零新 hook）**：

| 项 | 口径 |
| --- | --- |
| 命中分类 | 按**白名单键的 sub 名部分**（`+0x` 后缀已剥离）前缀：`MBoss*` = 中boss、`Boss*` = 关底 boss、其余（`main` / `MainSub*`）= 章节 |
| 中boss 战结束 | 当前 sub 名命中 **`MBossDead` / `MBossEscape`**（**`strcmp` 精确匹配**，见 §3.1） |
| 裁决 | `bossHit` ⇒ 关窗并清 `mPending`；否则 `midHit` ⇒ 开窗（若未开）、显示中boss 记录；否则 `chapHit` ⇒ 窗口内**只存 `mPending` 不显示**、窗口外正常显示 |
| 关窗 | 收尾 sub 名一出现即关窗并把 `mPending` **补显示**出来（取最后一个，与「值只单调前进」一致）；`mPending` 为空则维持中boss 记录 |
| 采样 | `th10_update`（`0x449d0e`）每帧；同帧内「主线程进 `MainSubN`」与「boss 宿主跑 `MBoss*`」都可见，靠裁决优先级处理 |
| 与 th16 的差异 | **不设 `mMidBossDone`**（同 th14）：th10 的中boss 容器 sub 是**交棒**给 `MBossDead` / `MBossEscape`（`setNext` / `setTimeout` 转移）而不是「返回白名单入口 sub」，故窗口不会自行重开。th16 的 `mMidBossDone` 是为 `st01mbs` 的 `MBoss()` 在 `@MBoss1()`（call）返回后 `goto` 自循环、当前 sub **变回白名单 `MBoss`** 这一形态准备的 |

**「中boss 存活」的备用判据（未采用，登记备查）**：`*(u32*)(mgr + 0x10)`（= thprac 既有 `bossExists`，`thprac_th10.cpp` 的 `mTimeLock` 钩子内使用；报告 §5A.2 / §5A.4）。本实现走 th14 的「收尾 sub 名关窗」，**不读**该字段——`bossExists` 是全局「当前有 boss」而非「中boss 阶段」，与关底 boss 无法区分，作为窗口判据不如 sub 名精确。

### 3.1 中boss 收尾 sub 名的离线枚举（**逐名精确匹配**）

枚举源：`D:\workshop\reasm\exes\ecl_dump\th10\*.decl`（7 份 `stageNN.decl` + `default.decl`；**本作无独立 boss 文件 / boss 槽**，中boss 脚本内联在 `stageNN.decl` 里）。

| sub 名 | 出现关 | 依据 |
| --- | --- | --- |
| **`MBossDead`** | st01 / st02 / st03 / st04 / st07 | `setNext(…, "MBossDead")` 的落点（st01:1249/1329、st02:1311、st03:1338/1398、st04:1877、st07:2927/2953），体内含 **`setBoss(-1)`** |
| **`MBossEscape`** | st01 / st02 / st03 / st04 / st05 | `setTimeout(…, "MBossEscape")` 的落点（st01:1250/1330、st02:1312、st03:1339/1399/1572、st04:1878、st05:1816/1934），体内含 **`setBoss(-1)`** |

- **排除 `MBossDead_anim`**（st03:1550 / st04:1984）：它由 `MBossDead` 自己用 **`@MBossDead_anim() async`**（st03:1532 / st04:1969）拉起，是一段 `+60` 帧的动画，**不是**战斗结束点。⇒ 这正是**必须精确匹配**的理由：th14 用的 `strncmp(name,"MBossDead",9)` 前缀法在 th10 会把 `MBossDead_anim` 也算成一次结束。
- **stage06 无中boss**（无任何 `MBoss*` sub，CSV 亦无 `ST6 MID*` 行）；**stage05 无 `MBossDead`**（只有 `MBossEscape`）。
- 全 7 关枚举结果中 `^void .*Dead|Escape` 仅有 **`BossDead` / `MBossDead` / `MBossDead_anim` / `MBossEscape`** 四个名字 ⇒ 本表**无同名歧义**（不同于 th16 的 `st07mbs2/BossDeadB`）。
- **与 `bossExists` 判据不冲突**：本实现根本不用 `bossExists`，两者独立。
- ⚠️ **待真机复核**：若真机上发现某关中boss 走的是**未被枚举到的收尾路径**（例如 `killAsync` / `delete` 直接结束而不经这两个 sub），窗口会不关 ⇒ 本章后半 checkpoint 被吞。兜底是 `bossHit`（关底命中）**强制关窗**，最坏只吞「中boss → 关底」一段。

---

## 4. 偏移匹配

### 4.1 判据

`i` = **包含 `ip` 的那个 sub** 在运行期 sub 表的下标（由 §1 的区间反查直接给出）：

```text
目标地址 = *(u32*)(table + i*8 + 4) + 0x10 + 偏移          // 偏移 = CSV 第 1 列 "+0x…" 的值
命中 ⟺ ip == 目标地址
```

- 成立前提：偏移 < 该 sub 长度 ⇒ 目标地址仍落在该 sub 区间内 ⇒ 「偏移行的 sub 名」与「包含 `ip` 的 sub」**必然同一个**（`th13_monitor_plan.md` §3.3 同法）。故**不需要**任何跨 sub 比较或复合键结构。
- 与 §1 的 `firstInsn(i)` 复用同一表达式（`table[i].+4 + 0x10`）——该值即报告 §3.3 的 `FindSubByName` 返回值（`add eax, 0x10`），运行期可得 ✅。

### 4.2 匹配顺序（**偏移命中优先**）

```text
(idx, name) = SubOfIp(table, ip)                       // §1 的 upper_bound
for row in mOffsetRows where row.sub == name:          // 本关的偏移行；同一 sub 可多行
    if ip == firstInsn(idx) + row.offset:  → 取 row.record（优先）
若未命中 → 查 mNameMap[name]                            // 纯名命中
```

### 4.3 解析期拆分（运行期不解析字符串）

`LoadCsv()` 把第 1 列的 `+0x` 后缀**手工按十六进制解析**（不用 `<cstdlib>` / `std::stoul`，避免多引头文件与异常）并 `erase` 掉，得到 `Entry{ sub, stage, record, offset, hasOffset, kind }`；`BuildStageMap(stageNum)` 按 `ST%d` 过滤本关，把 `hasOffset` 的行装进 `mOffsetRows`（`vector`，**同一 sub 可多行**）、其余装进 `mNameMap`。**格式非法的后缀 ⇒ 整行丢弃**（不当作纯 sub 名匹配，避免静默错配）。

### 4.4 本作偏移行分布（复算自 CSV，`python` 校验）

| 关 | 偏移行 |
| --- | --- |
| ST1 | `main+0x28` / `main+0x2b0` / `main+0x928` / `main+0x1348` / `main+0x1560` |
| ST2 | `main+0x5c` / `main+0x374` / `main+0x8dc` / `main+0x920` / `main+0x144c` |
| ST3 | `main+0x5c` / `main+0x144` / `main+0x964` / `main+0xc74` / `main+0x17d4` |
| ST4 | `MainSub1+0x154` |
| ST5 | `MainSub8+0x14` / `MainSub8+0x444` |
| ST6 / ST7 | —（无） |

- 合计 **18 行**（`main` ×15、`MainSub8` ×2、`MainSub1` ×1）；**91 行**为纯 sub 名；**同关内无重复键**（已复算）。
- **`MainSub8` 两行** ⇒ `mOffsetRows` 必须是 `vector` 且同一 sub 名要能查多行（`th13_monitor_plan.md` §10 第 1 条点名项）——本实现按插入顺序线性扫描，天然支持。
- **`ST4 前半 #2`（`MainSub1+0x154`）已知缺陷（照实登记，不特判）**：该 warp 落点 `t_land = 110 ≤ at_frame = 409` ⇒ 落点被**快进跑穿**，帧边界上观测不到 `0xe048`；`th10_checkpoint_ip_offset.md` §3.2 / §7 记「**用户已决定无视该 1 帧窗口**」，本行**照落点登记**，不引入区间匹配。

---

## 5. 安全校验与生命周期

| 项 | 处理 |
| --- | --- |
| 逐级判空 | `mgr` / `host` / `res` / `table` / `thread` 全部判 0 跳过 |
| `count` 哨兵 | `count ∈ (0, 8192]`（挡 `res` 被破坏）——本实现里这是唯一哨兵 |
| 遍历硬上限 | 宿主 ≤ 1024、线程 ≤ 64（防链成环）；`ip == 0`（线程已结束）先判、再反查 |
| 指针生命周期 | **不缓存任何宿主 / 线程 / 表指针**，每帧从 `0x477704` 重走链；`mIpMap` 是**值拷贝**（`SubRange{start,index}`），`mIpMapCount` / `mIpMapTable` 只作**变化检测**、从不解引用 ⇒ 结构性消除野指针（th15 分支的崩溃根因） |
| 一局/一关开始 | `th10_game_start`（`0x41798C`）钩子内 `OnStageStart()` ⇒ `mSection = "main"` + 清中boss 窗口 + 失效 `mIpMap` 变化检测（**零堆分配**，VEH 回调内安全） |
| 关卡切换 / 练习 warp | `Update()` 内 `STAGE_NUM` 变化 ⇒ `BuildStageMap(new)` + `mSection = "main"` + 清中boss 窗口 |
| 退出一局（`PLAYER_PTR == 0`） | **故意不重置**（需求要求保留最后 checkpoint；IGI 此时本就不绘制） |
| 菜单 / 非关卡态 | `STAGE_NUM ∉ {1..7, 9}` 直接 `return`，不改状态 |
| 按关过滤 | `BuildStageMap` 按 `ST%d` 过滤（`main` / `MainSub*` / `Boss1` / `BossCard1` 等名字在各关大量重复，只按名字匹配会串关） |
| 解析失败 | `LoadCsv()` 抛异常 ⇒ 清空 `mEntries`（恒显示 `main`，不抛到游戏线程外） |

---

## 6. 未真机声明（🟡）

> 按 `CLAUDE.md` §2.5 / AGENTS.md：**未真机验证的地址不得写成结论**。本迭代**未编译、未真机**，下列全部为 **🟡 静态定案**：

1. 🟡 **宿主链 / 线程链可达**：进关后 `*(u32*)(mgr+0x58)` 非空、节点 `+4` 可遍历；`*(u32*)(H+0x1030)` 的 `[+0]` = 主线线程、`[+4]` 起为 async 子线程。
2. 🟡 **`res` 恒等**：`*(u32*)(H+0x102C) == *(u32*)(*(u32*)0x477704 + 0x54)`（主线宿主与 boss 宿主**同值**）。
3. 🟡 **偏移坐标系**：warp 到 `ST1 前半 #2` 后，`*(u32*)(T+4) − *(u32*)(res+0xC)` 恰为 `0x9f40`（= `main+0x2b0` + `main` 首指令偏移 `0x9c90`）；st1–st3 的 12 个稳定锚点逐条采样（入口报告 §4 第 3 条）。
4. 🟡 **帧边界精确匹配**：帧边界上 `ip` 是否**恰好等于** `table[i].+4 + 0x10 + 偏移`（取决于 warp 落点是否等待）。若观测不到 ⇒ 改用「下一等待点」或**区间匹配**（`th10_checkpoint_ip_offset.md` §4.3）。**`ST4 前半 #2` 已知会被跑穿**（§4.4）。
5. 🟡 **多线程 3 行**：warp 到 `ST4 前半 #2`（`MainSub1`）与 `ST5 后半 #1/#2`（`MainSub8`），确认锚点 ip 出现在 **async 线程**上（`H+0x1034` 起），只读 `H+0x1030` 会漏。
6. 🟡 **中boss 窗口**：warp 到 `ST1 MID1` / `ST4 MID1`，确认主线程 ip 确实落到 `0xafd8` / `0x14aa4` / `0xf930`（章节行会被误 latch），再确认窗口把它挡住；中boss 存活期间显示保持 `STn MID1`，收尾 sub 一出现才补显示后半第一节。
7. 🟡 **中boss 收尾 sub 名**：§3.1 的 `MBossDead` / `MBossEscape` 是**离线枚举**，真机需观察是否所有关的中boss 都经这两个 sub 收尾（否则窗口不关，兜底是 `bossHit` 强制关窗）。
8. 🟡 **boss 行读数**：进 boss 后 `mgr+0x58` 链上存在**第 2 个**宿主，其线程链的 `ip` 落在 `Boss*` / `MBoss*` 区间内（入口报告 §4 第 12 条）。
9. 🟡 **`STAGE_NUM` 换算**：内置 `9 → ST7`；**th10 的 Extra 是否报 9 在现有文档里查不到**（`th10_ecl_re_report.md` §8 Q7 只登记了地址，未记取值），需真机加临时诊断行实测（如 `ImGui::Text("DBG stage=%d", *(int32_t*)0x474c7c);`，验完**删除**）。
10. 🟡 **采样时机**：`th10_update`（`0x449d0e`）相对 ECL VM 的执行顺序（帧内 `goto` / `call` 是否一定在采样前执行完）——决定「精确匹配」是否够用；备选触发点 = 既有 `0x44fb9f` 钩子（报告 §5.4 / §12 方案 B）。
11. 🟡 **四难度**：`ECLJump` 写入的 `ins[0x0A] = 0xFF`（任意难度）在 E/N/H/L 下均生效（报告 §13 点名的「最需真机确认项」）。
12. 🟡 **变体**：Steam / 汉化版的 `stageNN.ecl` 是否与原版一致（偏移是**数据偏移**，脚本改版即失效）。
13. 🟡 **未编译**：本迭代按主代理统一构建的安排**未跑 MSBuild**，故「能否通过编译」亦未验证。

---

## 7. 与其它文档的关系

- **口径 / Q9 / 粒度** → `docs/logger/logger_re_disasm_overview.md` §1.2 / §6 Q9 / §9.9。
- **本作逆向** → `docs/th10/th10_ecl_re_report.md`（§1 指针链 / §5A boss 宿主可达性 / §10.4–§10.5 粒度）。
- **本作入口表** → `docs/th10/th10_checkpoint_entry_report.md`（§2 口径特例 / §3 实现影响 / §4 待真机清单）。
- **偏移表与 boss 撞点** → `docs/th10/th10_checkpoint_ip_offset.md`（§3.1 / §5.1 / §7）。
- **同形参考实现** → `thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`；**权威参考** → `docs/th16/th16_monitor_impl_report.md`（§5.7 中boss 窗口）。
- **偏移匹配通用口径** → `docs/th13/th13_monitor_plan.md` §3.3 / §4（th10 是其「一个 sub 多偏移行」的推广，见该文 §10 第 1 条）。
- **进度** → `docs/logger/logger_progress.md` §1（E5 列，由主代理回填）。

---

## 8. 真机反馈与修复（2026-10-20）

> 真机反馈（原文）：**① th10 的 ST1 前半正常，但 MID2_HL 后，后半没有展示后半 #1 和 #2，一直展示 MID2_HL；② ST5 后半 #1/#2 正确（从 5 前半开始，经过 5 道中boss，整体完全正确），但 ST4 前半 #2 无法显示。**
> 两个 bug 均已用**离线解码钉死根因**（不是猜测），修复只动 `thprac/src/thprac/thprac_th10.cpp`。

### 8.1 BUG-A（ST1 中boss 窗口**不关**）

**结论先行**：`MBossCard1` 的线程在卡结束后**不存活**；窗口是**没关**，不是被重开。

**根因链（三条，逐条有地址/字节证据）**

1. **`setNext` 的相位转移不新建线程，而是「改道 + 同帧重跑」**。`enmCreate*` 之外的宿主相位推进由 `FUN_0040DC80` 内两处 `call 0x4127A0`（`0x40E1C2` / `0x40E27B`，`ECX = *(u32*)(H+0x2514) = H`）驱动；返回非空即转移：

   ```asm
   0040e2a2  mov eax, dword ptr [esi + 0x102c]   ; esi = H ⇒ eax = res
   0040e2a8  push edi                            ; edi = 目标 sub 名（0x4127A0 返回）
   0040e2a9  call 0x450470                       ; FindSubByName(res, name)
   0040e2ae  mov edx, dword ptr [esi + 4]        ; edx = *(u32*)(H+4) = 主线线程 T（驱动器 0x44FD90 把它保持为 H+8）
   0040e2b1  mov dword ptr [edx + 4], eax        ; ★ T+4 = 目标 sub 首指令
   0040e2b7  mov dword ptr [eax], 0              ; ★ T+0 = 0
   ```

   （另一条同型路径 `0x450700` 逐字同义。）`0x40E205` 随后**同帧**再 `call 0x44FD10` 重跑该线程 ⇒ 转移与「跑目标 sub」发生在**同一帧内**。⇒ **相位 sub 跑在 boss 宿主的同一条主线线程上**，`MBoss`/`MBossCard1` 的 `delete()` 不可达只是「永远走不到」，不是「线程活着」。
2. **`MBossDead` 全身没有等待指令**：`stage01.ecl` 的 `MBossDead`（sub 基址 `0x8334`）**35 条指令 `time` 全为 0**，末尾两条是 `delete()`（opcode `0x001` → `0x44FBB0`：`[T+4] = 0` 并返回 −1）。⇒ 它从入口跑到线程结束**全部落在两次采样之间**，帧边界上 `ip` 永远不在它体内。
3. **逐关复核 `MBossDead` / `MBossEscape` 的 `time` 上界**（离线解码 7 份 `stageNN.ecl`）：

   | 关 | `MBossDead` 指令数 / `maxTime` | `MBossEscape` 指令数 / `maxTime` |
   | --- | --- | --- |
   | ST1 | 35 / **0（无等待）** | 22 / 60 |
   | ST2 | 27 / 60 | 17 / 60 |
   | ST3 | 33 / 120 | 17 / 60 |
   | ST4 | 30 / 60 | 8 / 60 |
   | ST5 | **无 `MBossDead`** | 37 / 60 |
   | ST7 | 29 / **0（无等待）** | — |

   ⇒ **ST5 只有 `MBossEscape`（有 `+60` 等待，可观测）⇒ 窗口必然关闭 ⇒ 与「ST5 完全正确」一致**；**ST1 击破符卡（hp→0）走 `MBossDead`（无等待）⇒ `sawMidBossEnd` 恒假 ⇒ 窗口永不关**，`mMidBossActive` 永久为真、之后所有章节 checkpoint 只进 `mPending`、`mSection` 钉在 `ST1 MID2_HL`。ST7 同理（本轮未反馈，同一机制）。

**修复（`thprac_th10.cpp`）**：增补一条**跨帧可观测的存活判据** —— 引擎自己的中boss 槽位 `*(u32*)(mgr+0x10)`：

- `setBoss`（opcode `0x14C`）正值分支 `0x40FFFE`：`[mgr + idx*4 + 0x10] = *(u32*)(H+0x2514)`（= 宿主自身）；负值分支 `0x40FFB7`：`[mgr + idx*4 + 0x10] = 0`。
- `MBossDead` / `MBossEscape` 体内都调 `setBoss(-1)` ⇒ **该值跨帧保留**（不像 `ip` 一闪而过）。
- 同址即 thprac 既有 `bossExists`（`thprac_th10.cpp:555` 的 `mTimeLock` 钩子用它判「中boss 存活」，注释 `// freeze ECL sub time for st1/2/4 main during midboss`）；`enmCreate*` 也用它门控「同时至多一个 boss 宿主」（`0x40E7D6`）。
- 关窗条件改为 `sawMidBossEnd || (!midBossHost && !midHit)`；`!midHit` 守卫同 th12 §4.2，防「窗口内仍有白名单中boss sub 在执行」时误关。新增常量 `kMgrMidBossOff = 0x10`。

**为什么不是 `mMidBossDone`**：容器 sub 被相位转移**永久改道**（`MBoss_764: if (1) goto MBoss_476 @ 0;` 自旋、`delete()` 不可达），**不会**像 th16 `st01mbs` / th11 那样「返回白名单容器 sub」⇒ 不存在迟到命中重开；加 done 位反而会挡住「同一场中boss 的下一个相位」（`MID1 → MID2`）。§3 表末「不设 `mMidBossDone`」的结论**不变**，只是理由订正为「改道而非交棒」。

**为什么不用 th12 的「宿主从全局宿主链消失即关窗」**：本作相位转移**不销毁宿主**（`delete()` 只结束线程），且「宿主何时摘链」未证实 ⇒ 该方案在 th10 **不成立**（已排除）。

### 8.2 BUG-B（`ST4 前半 #2` 采不到）

**根因（离线解码证实）**：`MainSub1+0x154`（= 文件偏移 `0xe048`）的**落点指令 `time = 110`**，而 warp 的 `goto` 把 `T+0` 置 `409` ⇒ 落点同帧被跑穿；帧边界上 `ip` 实际停在 `0xe0c0`（= `MainSub1+0x1cc`，`time = 410`）。⇒ 等式 `ip == firstInsn + off` **物理上不可满足**，偏移支从不命中、永远回落纯名（`ST4 前半 #2` 因此永不显示）。与 th13 `ST5 后半 #4 = MainSub08+0x390` 是同一机制（th13 侧已先定案）。

**修复：偏移行改为「半开区间」匹配**（与 th13 定案**同语义**，不引入第三套）：

1. `BuildStageMap()` 里 `mOffsetRows` 按 `offset` **降序** `std::sort`（每关一次，运行期零分配）；
2. `Update()` 偏移支：`const uint32_t delta = ip - firstInsn;`，遍历（已降序）取**第一个** `strcmp(row.sub, name) == 0 && delta >= row.offset` 的行 ⇒ **命中即 break**（= 包含 `ip` 的**最内层**段）；`row.sub == name` 守卫保留，`delta` 不会下溢（`index` 由区间反查给出 ⇒ `ip >= firstInsn` 恒成立）。

**为何不破坏现有正常项（18 行逐行独立复算，脚本复现）**

- **18 行锚点全部不变**：每行自己的落点 `ip`（`delta == off`）仍取本行记录（降序 + `delta >= off` 下「最大的 `off <= delta`」即本行）。
- **`ST5 后半 #1/#2`（`MainSub8+0x14` / `+0x444`）不变**：两锚点仍各命中本行；区间只把 `[0x14,0x444)` 归 #1、`[0x444,∞)` 归 #2，与 warp 落点一致（**用户确认正常的两行，语义等价**）。
- **ST1–ST3 的 15 行不变**：段界就是行偏移本身，而 `ip` 永远落在**指令边界**上 ⇒ 落在段内的 `ip` 本就属于该段、原「锚点即段首」处仍取同一记录。唯一新增覆盖是 `ST2 MID1` 的撞点 `0xc018`（原等式匹配为 no-op），它落进 `前半 #3` 的段 —— 但该 `ip` 对应的 `time=2974` 的 `nop` 正是主线程 `t=974`（`enmCreateA("MBossDummy")`，= `前半 #3` 锚点）之后的**同一段**，记录名与锚点处已 latch 的值**相同**；且同帧有 `midHit`（`MBossCard1` = `ST2 MID1`）优先 ⇒ 无可见变化。
- **`ST4 前半 #1`（纯名 `MainSub1`）与 `前半 #2`（`MainSub1+0x154`）优先级不变**：`delta < 0x154` 时无偏移行命中 ⇒ 回落纯名 ⇒ `前半 #1`；`delta >= 0x154` ⇒ `前半 #2`（**本次修复目标**，`0xe0c0` 现在命中）。
- 其它 boss 撞点（`ST1` `0xafd8`、`ST4` `0x14aa4` / `0xf930`）在新旧语义下**取值相同**（`ST1 0xafd8` 仍 = `后半 #1`，仍靠中boss 窗口挡住；`ST4` 两处新语义下仍不命中任何行）。

**未决 / 需真机复核**：§6 第 4 条的「帧边界精确匹配」现改为**区间匹配**（本次已落地），故该条待验项改为「区间匹配下 `ST4 前半 #2` 能否稳定显示」；`ST2 MID1` 练习 warp 的 1–2 帧瞬态（`前半 #3` → `MID1`）需真机确认是否可见（正常游玩路径无变化）。

---

## 9. 日志接线（JSONL 落盘，2026-10-20）

> 需求来源：`docs/req_logger.md`（口径 11/12/13/14/15/16）；实现记录与共享层 API：`docs/logger/logger_impl_report.md`（§4 逐作接线 / §4.4 stageClear / §4.4.2 spellFlag）。
> 唯一模板：`thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（**逐字同构移植，只改常量与本作判据**）。
> 符卡对象逆向方法论：`docs/th14/th14_spell_flag_report.md`（§0 证据表粒度）。
> 范围：**只加日志接线，不改监控逻辑**；共享层 `thprac_games.{h,cpp}` 的 `#pragma region Game Session Log` **零改动**（口径 11/13/14/16 的合并逻辑、口径 12 的 `IsGameExitEdge` 均已就绪）。
> 代码：`thprac/src/thprac/thprac_th10.cpp`（`TH10Logger`）。
> 状态：**静态定案（🟡 未编译、未真机）**；本机按主代理统一构建，未跑 MSBuild。

### 9.1 结论速览

| # | 项 | 结论 | 状态 |
| --- | --- | --- | --- |
| 1 | 改动面 | `thprac_th10.cpp` 内 **9 处**（含 `enum addrs` + 匿名 namespace 常量）；调用点 **3 处**（`bomb_dec` / `bomb_dec2` / `life_dec` 既有 lambda 体内各一行）。`git diff --numstat` = **纯插入、0 删除** | ✅ |
| 2 | `enum addrs` | 新增 `SPELL_OBJ_PTR = 0x4776f4`（`:29`） | ✅ |
| 3 | 匿名 namespace | 新增 `kSpellFlagsOff = 0x378C`（`:626`）+ `IsStageClearSub()`（`:680`） | ✅ |
| 4 | (a) in-game 判据 | `GetMemContent<uint32_t>(PLAYER_PTR) != 0`（`PLAYER_PTR = 0x477834`，既有） | ✅ |
| 5 | (b) `IsStageClearSub` 清单 | **仅 `BossDead`**（`strcmp` 精确匹配）；**本作无 `BossEscape`**（超时折进 `BossDead` 自身） | ✅（离线）／🟡（真机） |
| 6 | (c) spellFlag | 对象 `*(u32*)0x4776f4`、存活位 **`+0x378C` bit0**；置位 `0x40930C`、清位 `0x409C4F`（均唯一） | ✅（反汇编）／🟡（真机） |
| 7 | bomb 双钩子 | **两处都接 `OnBomb()`**（不同 shot 类型的 bomb 分支，互斥；与 IGI 面板一致） | ✅ |
| 8 | shot 字段 | 接（`S(IGI_PL_10[chara*3 + subshot])`，与 IGI 面板同源） | ✅ |
| 9 | 不新增 hook | `bomb_dec` / `bomb_dec2` / `life_dec` 均写进**既有** `EHOOK_DY` 的 lambda 体内（同址二次 hook 会覆盖，`thprac_hook.cpp:223`） | ✅ |

### 9.2 逐处接线（`thprac_th10.cpp`）

| # | 位置 | 落地行 | 改动 |
| --- | --- | --- | --- |
| 1 | `enum addrs` | `:29` | `SPELL_OBJ_PTR = 0x4776f4,` |
| 2 | 匿名 namespace（`kThreadIpOff` 之后） | `:615-626` | `constexpr uintptr_t kSpellFlagsOff = 0x378C;`（含证据注释） |
| 3 | 匿名 namespace（`IsMidBossEndSub` 之后） | `:667-680` | `bool IsStageClearSub(const char* name)`（`strcmp(name,"BossDead")`，见 §9.3） |
| 4 | `TH10Logger` public（`CurrentSection()` 之后） | `:711-712` | `void OnMiss();` `void OnBomb();` |
| 5 | `TH10Logger` private 方法声明（`SubOfIp` 之后） | `:760` | `void SampleSpellCard();` |
| 6 | `TH10Logger` private 成员（`mBuiltStage` 之后） | `:784-793` | `mSpellFlag` / `mSpellObj` / `mSpellActive` / `mStageClear` / `GameSession mSession;` |
| 7 | `OnStageStart()` 体首 | `:926` | `mSession.Begin("th10");` |
| 8 | `OnStageStart()` 体末 | `:930-933` | 复位 `mSpellFlag` / `mSpellObj` / `mSpellActive` / `mStageClear` |
| 9 | `OnStageStart()` 之后 | `:939-980` | `OnMiss()` / `OnBomb()` 定义（各一行 `PushEvent`）+ `SampleSpellCard()` 定义（照 th14 `:931-960`，含「对象为空 ⇒ 只清采样状态、保留计数」「对象变化 ⇒ 三者复位」两分支） |
| 10 | `Update()` 体首（**早于所有 early return**） | `:984-997` | `inGame` 判据 + `SampleSpellCard()` + `if (mSpellFlag <= 0 && !mStageClear && IsGameExitEdge) PushEvent(Interrupt)` + `mSession.Tick(inGame, <shot 串>)` |
| 11 | `Update()` 换关块（`stageNum != mBuiltStage`） | `:1015-1018` | 复位 `mSpellFlag` / `mSpellObj` / `mSpellActive` / `mStageClear`（该块在采样之后执行，可抹掉换关的假下降沿） |
| 12 | `Update()` 候选变量区 | `:1035` | `bool sawStageClear = false;` |
| 13 | `Update()` 遍历体内（`IsMidBossEndSub` 之后） | `:1079-1081` | `if (IsStageClearSub(name)) sawStageClear = true;` |
| 14 | `Update()` 中boss 窗口裁决之前 | `:1169` | `const char* prevSection = mSection;` |
| 15 | `Update()` 段落裁决之后、`Latch()` 之前 | `:1198-1212` | 段落变化 ⇒ `if (mSpellFlag > 0) --mSpellFlag; mStageClear = false;`；最后 `if (sawStageClear) mStageClear = true;`；`mSession.Latch(GameLogRecordOfSection(mSection));` |

**调用点（3 处，全部落在既有 `EHOOK_DY` 的 lambda 体内）**：

| 位置 | 落地行 | 改动 |
| --- | --- | --- |
| `th10_bomb_dec`（`0x4259CF`） | `:3671` | `TH10Logger::singleton().OnBomb();` |
| `th10_bomb_dec2`（`0x425C3E`） | `:3676` | `TH10Logger::singleton().OnBomb();` |
| `th10_life_dec`（`0x426A1C`） | `:3681` | `TH10Logger::singleton().OnMiss();` |

`th10_update`（`:3476`）的 `Update()`、`th10_game_start`（`:3666`）的 `OnStageStart()`、`THGuiCreate`（`:3735`）的预构造 **监控迭代已就位**，本次未动。

### 9.3 (a)(b)(c) 三件事的定案与证据

#### (a) in-game 判据
`PLAYER_PTR = 0x477834`（`enum addrs` 既有，`:28`）；`bool inGame = GetMemContent<uint32_t>(PLAYER_PTR) != 0;`。与 thprac 既有 IGI 面板的显隐判据同源（口径 3），**不新增逆向**。

#### (b) `IsStageClearSub` 清单 = `{ "BossDead" }`（`strcmp` 精确匹配）
枚举源：`reasm/exes/ecl_dump/th10/*.decl`（7 份 `stageNN.decl`）。对 `^void Boss*Dead|Boss*Escape` **全量**枚举，**本作只有 `BossDead`**：

| sub 名 | 出现关 | 依据 / 处置 |
| --- | --- | --- |
| **`BossDead`** | st01:771 / st02:940 / st03:1019 / st04:1074 / st05:1284 / st06:1842 / st07:1978 | **收录**。每份体内含 **`setBoss(-1)`**（st01:793 / st02:963 / st03:1038 / st04:1093 / st05:1303 / st06:1862 / st07:1997）与 `delete()`；**每份都停在 `+60:` 标签上**（`+60: //60`，st06 另有 `+1:`）⇒ 60Hz 采样**稳定可观测** |
| `MBossDead` / `MBossEscape` | st01/02/03/04/07 与 st01/02/03/04/05 | **排除**：中boss 收尾（已被 `IsMidBossEndSub` 收录），**非通关** |
| `MBossDead_anim` | st03:1550 / st04:1984 | **排除**：`MBossDead` 用 `@MBossDead_anim() async` 拉起的 `+60` 帧动画，非战斗结束点 |

- **本作无 `BossEscape`**：`grep -i escape *.decl` 只命中 `MBossEscape`；**关底超时折进 `BossDead` 自身**——st01:779 `unless ($TIMEOUT == 0) goto BossDead_308 @ 0;`，两条分支在 `+60:` 汇合。故**不需要**（也不应）收录一个不存在的 `BossEscape`。
- **本作无 B 路线**：无 `BossBDead` / `BossBEscape`。
- **必须精确匹配**（与中boss 收尾同法）：`MBossDead` 是独立名字，`strcmp` 天然不命中；若用 `strncmp(name,"BossDead",8)` 则会**同时**命中 `MBossDead` 与 `MBossDead_anim`，把中boss 收尾误判成通关。
- **与 `MBossDead` 不可观测（§8.1）无关**：`BossDead` 有 `+60` 等待，本作不存在「关底收尾 0 帧跑穿」的缺口。

#### (c) spellFlag：对象 `*(u32*)0x4776f4`、存活位 `+0x378C` bit0（**反汇编定案，非邻接推断**）

> ⚠️ 说明：`0x4776f4` 恰等于 `ENEMY_MANAGER_PTR(0x477704) − 0x10`，但**该地址是从符卡 opcode 处理器反汇编出来的，不是从邻接关系推的**（邻接不作为证据）。证据链如下：

| 项 | 值 | 证据（VA / RVA = VA − 0x400000） |
| --- | --- | --- |
| **对象全局指针** | **`0x4776f4`**（RVA `0x776f4`） | 符卡 opcode 处理器内 `0x410f2b mov eax,[0x4776f4]` → 作为 **`this`** 入栈（`0x410f39 push eax`）→ `0x410f3a call 0x409280`（声明函数）；`spellEnd` 处理器 `0x410f56 mov eax,[0x4776f4]` → `0x410f5b call 0x409c00`（结算函数）。全 `.text` 4 字节小端立即数扫描共 **15 处 xref**：读 13 处（`0x405793` `0x4058d9` `0x40597a` `0x405a2c` `0x405a68` `0x405ac7` `0x405b18` `0x40e16f` `0x40e4a2` `0x410f2b` `0x410f56` `0x410f77` `0x410f9a`）、写 2 处（`0x4089aa` 构造器存指针、`0x408c1f` 析构器清 0） |
| **存活位（flags bit0）** | **`obj + 0x378C` bit0** | 见下「置位 / 清位」 |
| **置位点（唯一）** | **`0x409309 or eax,3` → `0x40930c mov [ebp+0x378c],eax`** | 在声明函数 `FUN_00409280`（被 `0x410f3a` 唯一调用）；`eax = (old & ~0x18) \| 3` ⇒ 置 bit0（存活）+ bit1（可收取） |
| **清位点（唯一）** | **`0x409c4c and edx,0xfffffffe` → `0x409c4f mov [esi+0x378c],edx`** | 在结算函数 `FUN_00409c00`（被 `0x410f5b` 唯一调用）；`edx = old & ~1` ⇒ 清 bit0。函数开头 `0x409c04 test byte[esi+0x378c],1; je 0x409d8c`（无活动卡 ⇒ no-op，幂等） |
| **opcode 分发** | `spell`/`spell3`/`spellEx` = **`0x165` / `0x166` / `0x167`** | 扩展 opcode 处理器 `FUN_0040E770` 入口：`0x40e79c mov ax,word[esi+4]`（opcode）→ `0x40e7a3 add ecx,0xFFFFFF00`（−`0x100`）→ `0x40e7c2 movzx ecx,byte[ecx+0x411ee0]` → `0x40e7c9 jmp [ecx*4+0x411d30]`；**三者 idx 同为 51、同指 handler `0x410e7b`**，其内 `0x410ed8 movsx eax,word[edx+4]` → `sub eax,0x165` → `je/dec/dec` 三路（`+diff` / `+diff−1` / `+diff−2`，`diff = *(u32*)0x474c74`） |
| **对象生命周期** | 构造器 `FUN_004088c0`（`0x4089aa mov [0x4776f4],esi`；调用点 `0x431f60` / `0x4321c1`），析构器 `0x408c1f mov [0x4776f4],0` | 单一全局对象；换关 / 重试 / 重开若重建对象则指针变化 ⇒ `SampleSpellCard()` 的 `obj != mSpellObj` 分支吸收 |
| **与 th14 的差异** | th14 对象 `0x4DB534`、flags **`+0x7C`**；**th10 flags 在 `+0x378C`**（本作对象是 ~`0x37B0` 字节的大结构，符卡子结构位于 `+0x3734`–`+0x3790`） | **不可跨作套用**（与 th16 `+0x78`、th15/17/18 `+0x78` 均不同） |

**唯一性核对**：对 `+0x378c` 的全 `.text` 写点扫描，**只有** `0x40930c`（置 bit0）、`0x409c4f`（清 bit0）两处动 bit0；其余写点分别清 bit1（`0x405520 and ~2`）、清 bit2（`0x408f76 and ~4`）、置 bit3/bit4（`0x40ce90 or 8` / `0x40cea0 or 0x10` / `0x410f7c or 8`）——**均不触碰 bit0**。⇒ 与 th14 同形（一个置位点、一个清位点）。

**已知覆盖缺口（照实登记）**：th10 的 `setNext` 链存在 **card→card 直转**（离线枚举 `*Card*` → `*Card*` 共 **8 处**：`st02 BossCard2→BossCard3`、`st04 BossCard3→BossCard4`、`st05 BossCard3→BossCard4`、`st06 BossCard4→BossCard5`、`st07 BossCard8→BossCard9`、`st07 BossCard9→BossCard10`、`st07 MBossCard1→MBossCard2`、`st07 MBossCard2→MBossCard3`）。下一张卡 sub 的首指令即 `spellEnd()` 紧随 `spell(...)`（**无 `wait` 间隔**）⇒ 同帧内 bit0 **1→0→1**，一帧一次采样**看不到这个下降沿**。**影响可控**：这些点**恰好也是 checkpoint 切换点**（`mSection` 从上一张卡的记录切到下一张），`spellFlag` 本就不需要在此处抑制 `interrupt`（净效果退化为无 spellFlag 的旧行为，方向是**多记**而非漏记）。**真正的兜底目标**（关底最后一张卡 → `BossDead`，`BossDead` 在 `+60` 才 `spellEnd()`、下降沿可观测；且 `mStageClear` 亦置位）**不受影响**。此缺口登记备查，不特判。

#### bomb 双钩子裁决：**两处都接**
`th10_bomb_dec`（`0x4259CF`）与 `th10_bomb_dec2`（`0x425C3E`）**都**执行 `TH10InGameInfo::singleton().mBombCount++`（thprac 既有 IGI 面板）。反汇编裁决：两者位于**同一个 per-shot-type 派发函数** `FUN_00425730`（入口 `0x425730 sub esp,0x1c` → `0x425739 mov eax,[ebp+0x458]`（shot 类型 0..4）→ `0x42574a jmp [eax*4+0x426344]` 跳表）的**不同 case**：

| 跳表（`0x426344`） | case 0 | case 1 | case 2 | case 3 | case 4 |
| --- | --- | --- | --- | --- | --- |
| 目标 | `0x425751` | `0x425964` | `0x425aa9` | `0x425d25` | `0x425a96` |

- `th10_bomb_dec`（`0x4259cf`）落在 **case 1**（`0x425964`–`0x425aa9`）；`th10_bomb_dec2`（`0x425c3e`）落在 **case 2**（`0x425aa9`–`0x425d25`）。
- 两条路径**互斥**（不同 shot 类型），且各自 `call 0x405860` + `add word[0x474c48],-0x14`（**同一 bomb 槽** `0x474c48` 各减 20）——若同一次 bomb 两条都走，bomb 槽会被扣 40，与游戏内表现不符 ⇒ 实际只会命中一条。
- ⇒ **两处都接 `OnBomb()`**，使 JSON 的 `bomb` 条数与 IGI 面板**逐条对齐**（同 **th18** 的双 bomb 点先例：`th18_bomb_dec` + `th18_cylinder` 两处都挂）。

#### shot 字段
有现成来源，**接**（照 th14 `:976` 的守卫 `inGame && mSession.NeedsStartInfo()`）：`S(IGI_PL_10[static_cast<byte>((*(byte*)(CHARA_ADDR)) * 3 + (*(byte*)(SUBSHOT_ADDR)))])`，与 IGI 面板 `OnContentUpdate()`（`thprac_th10.cpp:1114` 的 `cur_player_type = (*(byte*)0x474C68)*3 + (*(byte*)0x474C6C)`）**同源**。`CHARA_ADDR`/`SUBSHOT_ADDR` 为 `enum addrs` 既有项。`IGI_PL_10[7]` = `{ReimuA,ReimuB,ReimuC,MarisaA,MarisaB,MarisaC}`。

### 9.4 未真机声明（🟡）

按 `CLAUDE.md` §2.5 / AGENTS.md，本迭代**未编译、未真机**，下列均为 **🟡 静态定案**：

1. 🟡 **未编译**：按主代理统一构建的安排，未跑 MSBuild。
2. 🟡 **`0x4776f4` 真机可达**：进关后 `*(u32*)0x4776f4` 非 0、符卡声明后 `+0x378C` bit0 = 1、卡结束（下一 sub 首帧 `spellEnd`）后 bit0 = 0。
3. 🟡 **`BossDead` 可观测**：关底 boss 收尾时 `sawStageClear` 能稳定置位（各关 `+60` 窗口）；退出后**无** `interrupt`。
4. 🟡 **card→card 缺口**：§9.3 末登记的 8 处直转下 `spellFlag` 不计数（预期：退化为旧行为，不误报）。
5. 🟡 **bomb 双钩子**：确认两次 `mBombCount++` 不会在同一次 bomb 同时命中（JSON 条数 == IGI 面板）。
6. 🟡 **shot 串**：与 IGI 面板 `diff_pl` 行的机体串一致。
7. 🟡 **口径 12/15 时序**：`mSession.Tick()` 在 `Update()` 顶部、`interrupt` 判定在 `Tick()` **之前**（同帧落盘一致性）。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **新增 §9「日志接线」（JSONL 落盘，只加接线不改监控）**。照 `thprac_th14.cpp` 的 `TH14Logger` 逐字同构移植，`thprac/src/thprac/thprac_th10.cpp` 内 9 处（`enum addrs` 加 `SPELL_OBJ_PTR = 0x4776f4`；匿名 namespace 加 `kSpellFlagsOff = 0x378C` + `IsStageClearSub()`；`OnMiss`/`OnBomb`/`SampleSpellCard` 声明与定义；`mSpellFlag`/`mSpellObj`/`mSpellActive`/`mStageClear`/`GameSession mSession`；`OnStageStart` 复位；`Update()` 顶部 `Tick`+`interrupt`、换关复位、`sawStageClear`、段落裁决后的 `--mSpellFlag`/`mStageClear`/`Latch`）+ 调用点 3 处（`th10_bomb_dec`/`th10_bomb_dec2` 既有 lambda 内 `OnBomb()`、`th10_life_dec` 内 `OnMiss()`；**不新增 EHOOK**）。**`git diff --numstat` = 纯插入、0 删除**；共享层 `thprac_games.{h,cpp}` 零改动。**(a)** in-game 判据 = `PLAYER_PTR(0x477834) != 0`。**(b)** `IsStageClearSub` = **仅 `BossDead`**（7 份 `stageNN.decl` 全量枚举；**本作无 `BossEscape`**——超时折进 `BossDead` 自身 st01:779；精确匹配以排除 `MBossDead`/`MBossDead_anim`；各关 `BossDead` 均停在 `+60:` ⇒ 可观测）。**(c)** spellFlag：**反汇编定案（非邻接推断）** —— 对象 `*(u32*)0x4776f4`（符卡 opcode 处理器 `0x410f2b` 把它作 `this` 传给声明函数 `0x409280`；`0x410f56` 传给结算函数 `0x409c00`；全 `.text` 4 字节立即数扫描 15 处 xref），存活位 **`+0x378C` bit0**（**置位唯一 `0x409309 or eax,3` → `0x40930c`；清位唯一 `0x409c4c and edx,~1` → `0x409c4f`**），opcode `spell/spell3/spellEx` = `0x165/0x166/0x167`（跳表 `0x411d30`/字节表 `0x411ee0` idx 51 → handler `0x410e7b`）；**flags 偏移与 th14 的 `+0x7C` 不同**。**bomb 双钩子裁决 = 两处都接**（`FUN_00425730` 的 per-shot 跳表 `0x426344` 中 `0x4259cf` 属 case 1、`0x425c3e` 属 case 2，互斥；两处都 `mBombCount++` ⇒ 对齐 IGI，同 th18 先例）。**shot 接**（`S(IGI_PL_10[chara*3+subshot])`，与 IGI 面板同源）。**已知缺口**：6 处 card→card 直转（无 `wait` 间隔）采样看不到下降沿，但那些点本就是 checkpoint 切换点、影响退化为旧行为（多记方向），登记备查。**未碰** `.vcxproj` / `.filters` / `thprac_games.{h,cpp}` / 其它作 / `wind`；**未编译**（主代理统一构建）。**🟡 未真机三态**（§9.4 列 7 条待验项）。 |
| 2026-10-20 | **真机反馈修复（两个 bug，均离线钉死根因，只动 `thprac_th10.cpp`）**。**BUG-A（ST1 中boss 窗口不关）**：根因 = `sawMidBossEnd` 在 ST1 击破路线下**不可观测** —— ① `setNext` 相位转移**不新建线程**，而是把 boss 宿主**主线线程**重写为目标 sub 并把 `T+0` 归零（`0x450700` / `0x40E2A2–0x40E2B7`），随后**同帧** `call 0x44FD10` 重跑（`0x40E205`）；② `MBossDead` **全身无等待指令**（`stage01` 35 条 `time` 全 0，末尾 `delete()` opcode `0x001` → `0x44FBB0`）⇒ 两次采样之间跑完、帧边界永不可见；③ 逐关复核 `maxTime`：`MBossDead` = **st1 0 / st7 0**、st2 60 / st3 120 / st4 60，`MBossEscape` 五关 60，**ST5 无 `MBossDead`** ⇒ 与「ST5 完全正确、ST1 卡死」的反馈一致。**结论：`MBossCard1` 线程不存活，窗口是「没关」不是「被重开」**。**修复** = 增补跨帧判据 `*(u32*)(mgr+0x10)`（`setBoss` opcode `0x14C` 正/负分支 `0x40FFFE` / `0x40FFB7` 写/清；= thprac 既有 `bossExists`，`thprac_th10.cpp:555`；`enmCreate*` 门控 `0x40E7D6`），关窗条件改 `sawMidBossEnd \|\| (!midBossHost && !midHit)`，新增 `kMgrMidBossOff = 0x10`。**不采用 `mMidBossDone`**（容器被**永久改道**而非交棒，无迟到命中；加 done 位会挡住 MID1→MID2）；**不采用 th12「宿主消失」**（本作转移不销毁宿主，且摘链未证实）。**BUG-B（`ST4 前半 #2`）**：根因 = 落点 `MainSub1+0x154`（`0xe048`）指令 `time=110` 而 warp 把 `T+0` 置 409 ⇒ 跑穿、可观测 `ip` 为 `0xe0c0`（`+0x1cc`，`t=410`），等式匹配物理上不可满足；**修复** = 偏移行改**半开区间**匹配（与 **th13 定案同语义**：`mOffsetRows` 按 `offset` 降序 `std::sort` + `delta = ip − firstInsn` + 取第一个 `sub 名相同 && delta >= off` 的行、命中即 break），`delta` 不下溢、`sub` 守卫保留。**18 行逐行独立复算**：锚点全部不变；`ST5 后半 #1/#2` 不变；ST1–ST3 的 15 行不变（新增覆盖 `ST2 MID1` 撞点 `0xc018` 落进 `前半 #3` 段，值与锚点同、且同帧 `midHit` 优先）；`ST4 前半 #2` 修好。**未碰** `.vcxproj` / `.filters` / 其它作 / `wind`；**未编译**（主代理统一构建）。**🟡 未真机三态**：BUG-A 与 BUG-B 的修复均待真机复核（§8.2 末）。 |
| 2026-10-20 | **建档：th10 监控（E5）实现落地（只做监控，不做落盘）**。① 新增 `docs/logger/tools/gen_th10_checkpoints.py`（复制 th14 版改 5 处）并运行，**自校验「嵌入正文与 CSV 逐字节一致」通过**（3384 B）；生成物 `thprac/src/thprac/thprac_th10_checkpoints_gen.cpp` 入库（`THPrac::TH10::kTh10CheckpointsCsv`）。② `thprac_th10.cpp` 加 `TH10Logger` 模块（`:574-1021`）：文件头补 7 个 include；`extern` 声明；匿名命名空间（链偏移 `kMgrHostHeadOff=0x58` / `kHostThreadListOff=0x1030` / `kHostResOff=0x102C` / `kThreadIpOff=4`、sub 表项 `+8` / `+0x8C` / 8B / `+4` / `+0x10`、`ClassifySub()`、`IsMidBossEndSub()`、上限 8192/1024/64）；`class TH10Logger` + 实现（`LoadCsv` 解析期拆 `+0x` 后缀 / `BuildStageMap` 分 `mNameMap` + `mOffsetRows` / `BuildIpMap` 地址升序区间表 / `SubOfIp` / `Update` 遍历**全局宿主链 × 每宿主线程链**、双匹配**偏移优先**、中boss 窗口三类裁决）。③ 4 处调用：IGI 显示行（`:1083-1086`，bomb 行之后、标签硬编码 `"当前段落"`）、`th10_update`（`:3284`）、`th10_game_start` 既有 lambda 体内（`:3474`）、`THGuiCreate`（`:3540`）。**关键常量**：宿主链 `mgr+0x58`（节点 `H+0x116C`）、线程链 `H+0x1030`、`res = *(u32*)(H+0x102C)`（≡ 全局 `*(u32*)(mgr+0x54)`）、`ip = *(u32*)(T+4)`（**绝对地址**，非索引）、sub 表 `res+0x8C` 项 8B（`+0` 名 / `+4` 数据基址 / 首指令 `+4+0x10`）。**中boss 收尾 sub 名 = `MBossDead` / `MBossEscape`**（离线枚举 7 份 `stageNN.decl` 的 `setBoss(-1)` 站点；**精确匹配**，排除 `MBossDead_anim`；stage06 无中boss、stage05 无 `MBossDead`）。**偏移行 18 行**（`main` ×15、`MainSub8` ×2、`MainSub1` ×1），`mOffsetRows` 为 `vector` 以支持同一 sub 多行。**`enum addrs` 无需新增**（`ENEMY_MANAGER_PTR` / `STAGE_NUM` 均已在表内）。**未做**：日志落盘、新 hook、`.vcxproj` 登记（主代理统一）、编译。**状态：🟡 静态定案，未编译、未真机**（§6 列 13 条待验项；其中 `STAGE_NUM` 的 Extra 取值在本作文档里**查不到**，内置 `9 → ST7` 并标 🟡）。 |

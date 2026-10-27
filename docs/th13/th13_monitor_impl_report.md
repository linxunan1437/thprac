# TH13 ECL 段落监控 — 实施记录（E5）

> 需求来源：`docs/req_logger.md`（ECL 段落监控 + 日志）。监控见 §0–§8；**日志接线见 §9**（2026-10-20 追加）。
> 上位文档（术语 / 判定口径 / 粒度，一律以此为准）：`docs/logger/logger_re_disasm_overview.md`（**§1.2 粒度 / §2 术语表 / §5.1 锚点总表 / §6 Q9 / §9.9**）。
> 实现方案（本作，权威）：`docs/th13/th13_monitor_plan.md`（§3.1 / §3.2 / §3.3 / §4 / §5 / §6 / §7.1–§7.4）。
> 本作逆向结论：`docs/th13/th13_ecl_re_report.md`（**§1 指针链 / §2.1 指令头布局 / §2.3 地址区间反查 / §3.2 sub 表项 `+4` / §3.3 `+0x10` 首指令 / §3.4 `GetMemAddr` 展开 / §5 宿主捕获 / §5.4 宿主可达性 / §10.4**）、`docs/th13/th13_checkpoint_entry_report.md`（**116/116**；§2.1 偏移行 / §2.2 同名容忍 / §3.1 双匹配 / §3.3 3 行 async-only）。
> 参考实现（权威，真机验证过）：`docs/th16/th16_monitor_impl_report.md`（**§5.7 中boss 窗口**）。
> 直接模板（同形）：`thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（宿主链 × 线程链 + `ip → sub` 区间反查）。
> 二进制指纹：`../exes/th13.exe`，**790528 B**，PE `TimeDateStamp = 1313589413`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）。
> 方法：照方案 §7.1–§7.4 逐条落地；中boss 死亡 sub 名**离线枚举** `../exes/ecl_dump/th13/*.decl`（只读）。**未运行 MSBuild、未编译、未真机**（主代理统一构建）。
> 状态：**已实现（静态核对通过）+ 真机反馈已修（2026-10-20，见 §8）**；🟡 修复后未再编译、未再真机。
> 地址口径：正文 `VA` 与 `RVA = VA − 0x400000` 并列；术语（宿主 / 线程 / sub / checkpoint / 当前段落）以总括 §2 术语表为准。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 采用方案 | 方案 §3.1 / §3.2：**th14 骨架**（全局宿主链 × 每宿主线程链 → `ip = *(u32*)(T+4)` → 地址升序区间反查 sub 名 → 白名单 latch） |
| 采样点 | **`th13_update`（`0x470C04`）每帧轮询**，**不新增 hook**（沿用既有 EHOOK_DY 的 lambda 体） |
| 取值范围 | 外层遍历 `mgr+0xB0` **全局宿主链**（主线 + boss + 中boss + 敌方波次），内层遍历每宿主 `H+0x11D0` **线程链**（主线 + async） |
| ECL 资源取法 | **按宿主读 `H+0x11CC`**（≡ `*(u32*)(mgr+0xAC)`；th13 所有宿主共用一份全局 `res`，与 th14 同法，**无** th16 的「res 必须按宿主取」陷阱） |
| 当前 sub 取法 | 本作**无 subIdx 层**：`ip` 是**指令绝对地址** ⇒ 用 `table[i].+4 + 0x10` 建**地址升序区间表** `upper_bound` 反查 |
| 偏移匹配 | **`sub名+0x偏移`**：判据 `ip − (table[idx].+4 + 0x10) >= 偏移`（`idx` = 区间反查返回的下标；**区间/位置比较，非等式** —— 真机修复，理由与证据见 **§8**）；**偏移命中优先于名字命中**。本作**仅 1 行**：`ST5 后半 #4 = MainSub08+0x390` |
| 中boss 窗口 | **移植 th16 §5.7 三态**（`mPending` / `mMidBossActive` / `mMidBossDone` + 三类命中裁决）；死亡 sub 名 = **`MBossDead` / `MBossEscape`**（离线枚举，与方案 §6 一致） |
| 指针生命周期 | **不缓存任何宿主/线程/表指针**，每帧从 `ECL_MGR_PTR` 重走链；`mIpMapCount`/`mIpMapTable` 只作**变化检测**、不解引用 |
| 白名单进二进制 | `docs/logger/tools/gen_th13_checkpoints.py` → raw string 嵌入生成物 `.cpp`（生成物入库，**不加** MSBuild 预构建步骤），运行期 `rapidcsv` 解析一次 |
| 显示 | IGI 面板 bomb 计数行下方新增一对「标签 / 值」；标签硬编码中文 `"当前段落"`（**未动** `thprac_locale_def.*`） |
| 开关 | 不新增 `AdvancedGameOptions`，跟着 IGI（F9）走 |
| 范围边界 | **监控迭代**只做监控（`TH13Logger` 当时只有 `OnStageStart()` / `Update()` / `CurrentSection()`）；**2026-10-20 已补日志接线**（§9）：新增 `OnMiss()` / `OnBomb()` / `SampleSpellCard()` + `GameSession` / `mStageClear` / `mSpellFlag`（口径 12 / 15），监控逻辑未动 |

---

## 1. 指针链（th13，已落地）

```text
mgr    = *(u32*)0x4C2188                  // ECL_MGR_PTR（本次新增进 enum addrs）
nd     = *(u32*)(mgr + 0xB0)              // 宿主链头；节点 = H+0x14CC = {+0 = H, +4 = next, +8 = prev}
host   = *(u32*)nd
entry  = host + 0x11D0                    // 线程链头（嵌在宿主内）；[+0] = 主线 T（≡ host+8），+4 = next
thread = *(u32*)entry                     // 后继异步线程自 *(u32*)(host+0x11D4) 起
ip     = *(u32*)(thread + 4)              // 当前（下一条待执行）指令【绝对地址】；0 = 线程结束
res    = *(u32*)(host + 0x11CC)           // ≡ *(u32*)(mgr + 0xAC)（所有宿主共用一个全局 res）
count  = *(i32*)(res + 8)
table  = *(u32*)(res + 0x8C)              // 项 8B：+0 = 名字 char*，+4 = sub 数据基址
firstInsn(i) = table[i].+4 + 0x10         // 该 sub 的首条指令地址
```

- **地址 → sub**：`SubRange{ start = table[i].+4 + 0x10, index = i }` 按 `start` **显式 `std::sort` 升序**（表项本身按**名字字典序**存放、地址不单调）⇒ `upper_bound` 前一个即 `idx`；名字 = `*(char**)(table + idx*8)`。
- **与 th15–th18（C 代）不可互搬**：C 代 `host+0xC` → thread、`thread+4` = **sub 索引**；th13 是 `H+0x11D0` → thread、`T+4` = **指令绝对地址**，与 th14 同形。

---

## 2. 改动清单

### 2.1 功能本身

| 文件 | 改动（行号为改动后） |
| --- | --- |
| `thprac/src/thprac/thprac_th13.cpp` | ① `:3-10` 补 include（`..\3rdParties\rapidcsv\rapidcsv.h` + `<algorithm> <cstdio> <cstring> <sstream> <string_view> <unordered_map>`；`<format>` 保留）；② `:26` `enum addrs` 加 `ECL_MGR_PTR = 0x4c2188`；③ `:29-31` `extern const char kTh13CheckpointsCsv[];` 声明；④ `:33-131` 匿名命名空间常量 + `ClassifySub` / `IsMidBossEndSub` / `SplitOffsetName`；⑤ `:133-476` `class TH13Logger`（`:135` 起）与全部方法定义，落点 = `enum addrs` 之后、`struct THPracParam`（`:478`）之前；⑥ `:1098-1101` IGI 显示行；⑦ `:2657` 每帧采样；⑧ `:2698` 一局重置；⑨ `:2748` 预构造 |
| `docs/logger/tools/gen_th13_checkpoints.py`（新建） | 由 `gen_th14_checkpoints.py` 复制，只改 5 处：`CSV_PATH` → `th13_checkpoints.csv`、`OUT_PATH` → `thprac_th13_checkpoints_gen.cpp`、`RAW_DELIM = "TH13CSV"`、命名空间 `THPrac::TH13`、符号 `kTh13CheckpointsCsv`。幂等；断言正文不含终止符 `)TH13CSV"`；自校验逐字节一致 |
| `thprac/src/thprac/thprac_th13_checkpoints_gen.cpp`（生成物入库） | `THPrac::TH13::kTh13CheckpointsCsv[]`，raw string 内嵌 CSV **3488 B 逐字节一致** |

> **生成脚本已运行**：`python docs/logger/tools/gen_th13_checkpoints.py` ⇒ `wrote …`，`csv bytes: 3488  embedded bytes: 3488`，`self-check: embedded body is byte-identical to the csv`；二次运行输出 `unchanged`（幂等）。

### 2.2 工程登记（**不在本次范围**）

`thprac/thprac.vcxproj` / `.filters` 需登记 `thprac_th13_checkpoints_gen.cpp`（各 +1 条），由**主代理统一登记**（本子代理未触碰这两个文件）。

### 2.3 四个调用点（方案 §7.3 逐条）

| # | 位置 | 内容 | 行号 |
| --- | --- | --- | --- |
| a | `TH13InGameInfo::OnContentUpdate()` | 在 **bomb 计数行之后、trance 行之前**插「标签 / 值」两格（`Text("当前段落")` + `NextColumn()` + `Text("%s", …CurrentSection())` + `NextColumn()`），保持既有 `Columns(2)` 成对结构 | `:1098-1101` |
| b | `EHOOK_DY(th13_update, 0x470c04, 1, {…})` | 在 `TH13InGameInfo::singleton().Update();` **之前**加 `TH13Logger::singleton().Update();` | `:2657` |
| c | `EHOOK_DY(th13_game_start, 0x42D3D7, 7, {…})` | **写在既有 lambda 体内**（同址挂第二个钩子会覆盖第一个，`thprac_hook.cpp:223`）：`TH13Logger::singleton().OnStageStart();` | `:2698` |
| d | `THGuiCreate()` | 在 `TH13InGameInfo::singleton();` 旁加 `TH13Logger::singleton();`（预构造，跑一次 `LoadCsv`） | `:2748` |

> **未新增任何 hook**；未碰 `thprac_games.{h,cpp}`、`thprac_locale_def.{h,cpp}`、`wind`、其它作的任何文件。

---

## 3. 中boss 窗口（移植 th16 §5.7 三态）

**静态根因（th13 同构）**：`st01.decl` 的 `main()` 在 `@MainFront()` 之后 `enmCreateA("MBoss", …)` → `wait(160)` → **`@MainLatter()`**，而 `MainLatter` 的首个子 = `MainSub04` = `ST1 后半 #1` ⇒ 中boss 出场后主线程就开始 latch `后半 #1`（增援章节误报）。

| 项 | 口径（与 th16 逐字同构） |
| --- | --- |
| 命中分类 | 按**白名单键前缀**：`MBoss*` = 中boss、`Boss*` = 关底 boss、其余（`MainSub*`）= 章节。**不改 CSV**。偏移行按其**基名**分类（`MainSub08+0x390` → 章节） |
| 中boss 战结束 | 当前 sub 名命中**死亡 / 逃逸 sub 前缀**：`MBossDead` / `MBossEscape`（**精确前缀**，`strncmp`） |
| 裁决 | `bossHit` → 关窗并清 pending、显示 boss；否则 `midHit && !mMidBossDone` → 开窗、显示中boss 记录；否则 `chapHit` → 窗口内**只存 `mPending` 不显示**，窗口外正常显示 |
| 关窗 | 死亡 sub 名一出现即关窗并**把 `mPending` 补显示出来**（取最后一个）；`mPending` 为空则维持中boss 记录 |
| `mMidBossDone` | 保留（方案 §6 要求「先照搬三态」）：`MBoss()` 在 `@MBoss1()` 返回后 `goto` 自循环，当前 sub 可能变回白名单名 ⇒ 防「迟到命中重开窗口且永不关闭」 |
| 采样点 | `th13_update` 每帧；同帧内「主线程进 `MainSub04`」与「`MBoss1` 命中」都可见，靠裁决优先级处理 |
| 新增钩子 | **无**。只复用 walk 已读到的 `name` 指针；`IsMidBossEndSub()` 只做 `strncmp` |

### 3.1 死亡 sub 名枚举（离线，2026-10-20 复核）

对 `../exes/ecl_dump/th13/*.decl` 全 21 份做 `^void .*(Dead|Escape)` 与 `setBoss(-1)` 双向枚举：

- **中boss 侧**（`st01.decl` + `st02mbs`…`st07mbs`）：每个文件**恰有** `MBossDead` + `MBossEscape` 各一个，均由 `setNext(…, "MBossDead")` / `setTimeout(…, "MBossEscape")` 注册（如 `st01.decl:406-407`）。**无** `MBossBDead` / `MBossBEscape` 变体（th13 无 B 路线），**无** `MBossCard*_Dead` 同名陷阱。
- **ST1 的中boss 内联在 `st01.decl`**（`../exes/ecl_dump/th13/` **无 `st01mbs.decl`**）——`MBoss` / `MBoss1` / `MBoss2` / `MBossCard1` / `MBossDead` / `MBossEscape` 全在 `st01.decl`；逐关枚举**未漏 ST1**（与方案 §6 的告警一致）。
- **关底侧**另有 `BossDead` / `BossEscape`（`stNNbs`）与 `st04bs` 的 `BossMiyako*Dead/Escape` 一族 —— 后者是**关底 boss** 的名字，**不**匹配 `MBoss*` 前缀，与本迭代的中boss 判定无冲突（`mStageClear` 不在本迭代范围）。
- ⇒ 判定函数取 `strncmp(name, "MBossDead", 9) == 0 || strncmp(name, "MBossEscape", 11) == 0`，与方案 §6 的 `MBossDead`/`MBossEscape` **一致**。
- **ST7 复核**：`st07mbs` 的三个相位 sub `MBossCard1/2/3`（= `ST7 MID1/2/3`）内部各自 `setNext(…, "MBoss2"/"MBoss3"/"MBossDead")` + `setTimeout(0, "MBossEscape")` ⇒ `MBossEscape` 是**超时逃逸**（章节计时耗尽）、不是相位中继，与 th14/th16 的语义一致，窗口不会在 MID1→MID2 之间被误关。

---

## 4. 偏移匹配（本作唯一 1 行）

- 行：`ST5 后半 #4 = MainSub08+0x390`（与 `ST5 后半 #3 = MainSub08` **同 sub**，靠**偏移命中优先**消歧）。
- 解析：`SplitOffsetName()` 在 **`LoadCsv` 期**把 `+0x` 后缀拆成 `(基名, 偏移)`（运行期不再解析字符串）；`mEntries` 冻结后 `mNameMap` / `mOffsetRows` 的键值指针终身有效。
- 判据（**2026-10-20 真机修复后**）：`ip − (table[idx].+4 + 0x10) >= row.off`，其中 `idx` = **`SubIndexOfIp(ip)` 已返回的区间反查下标**（偏移 < 该 sub 长度 ⇒ 目标地址仍落在同一区间内，复用同一 `idx`，无跨 sub 歧义）。建档时为**等式** `ip == table[idx].+4 + 0x10 + row.off`；真机证明等式对该行**永不成立**，故改为**区间/位置比较** —— **根因、证据与「为什么不能保持等式」见 §8**。
- 优先级：**先试偏移行（并要求 `row.sub == name`）→ 未命中再查 `mNameMap`**。名字比对不能省：否则「另一 sub 内恰好 +0x390 处的指令」会被误判为命中。
- 同一 sub 多偏移行：`mOffsetRows` 按 `off` **降序**排序 ⇒ 第一个 `delta >= off` 的行胜 = **包含 ip 的最内层段**（推广性见 §8）。
- 等价换算（方案 §3.3 备选，**未采用**）：也可折算成文件偏移 `(firstInsn + 偏移) − base` 与 `off = ip − base` 比对；本迭代按**绝对地址区间**匹配。

---

## 5. 安全校验（照 th14/th16）

- `mgr` / `res` / `table` **逐级判空**（菜单态、ECL 未加载时为 0）；
- `count ∈ (0, 8192]` —— 挡「`res` 被破坏」的唯一哨兵；
- 遍历硬上限：**宿主 ≤ 1024、线程 ≤ 64**（防链成环）；
- `ip == 0`（线程已结束）**先判**，再进区间反查；
- **不缓存任何跨帧指针** ⇒ 重置退化为「换关时 `mSection ← "main"`」；
- `STAGE_NUM ∉ {1..7}`（含 Extra 报 9 → 内置 `9 → ST7`）直接 return，**不改状态**；
- 退出一局（`PLAYER_PTR == 0`）**故意不重置**（保留最后 checkpoint；IGI 此时本就不绘制）。

---

## 6. 静态核对（已做）

| # | 核对项 | 结果 |
| --- | --- | --- |
| 1 | 方案 §7.1 两产物（脚本 + 生成物） | ✅ 脚本 5 处改动齐；生成物 3488 B 逐字节一致；幂等 |
| 2 | 方案 §7.2 的 `thprac_th13.cpp` 五类改动（include / `enum addrs` / extern / 模块 / 4 调用） | ✅ 全部落地（§2.1、§2.3） |
| 3 | 方案 §7.3 四调用点逐条 | ✅ a `:1098-1101`、b `:2657`、c `:2698`（**在既有 lambda 体内**）、d `:2748` |
| 4 | 方案 §3.2「与 th14 的 `BuildIpMap` / `NameOfIp` 逐字同形」 | ✅ `BuildIpMap` 逐字照搬（仅常量名 `kThreadIpOff` 等本作私有）；`NameOfIp` 扩展为 `SubIndexOfIp`（返回 `index` 而非名字，方案 §3.3 的「复用 `SubOfIp` 已返回的 index」要求） |
| 5 | 方案 §3.1 遍历「宿主链 × 线程链」 | ✅ 外层 `mgr+0xB0`、内层 `host+0x11D0`→`+0x11D4`；**3 行 async-only** 因此可达 |
| 6 | 方案 §3.3 偏移匹配判据 + 偏移优先 | ✅ `ip == table[idx].+4 + 0x10 + off`，偏移行先于 `mNameMap` |
| 7 | 方案 §6 中boss 窗口（含 `mMidBossDone`） | ✅ 三态 + 三类裁决逐字同构；死亡 sub 名枚举 = `MBossDead`/`MBossEscape`（含 ST1 内联复核） |
| 8 | 方案 §7.4 安全校验 / 不缓存指针 / 退出一局不重置 | ✅ 见 §5 |
| 9 | 方案 §7.4「接口只有三个」 | ✅ `TH13Logger` 只暴露 `OnStageStart()` / `Update()` / `CurrentSection()`；无 `GameSession`/`OnMiss`/`OnBomb`/`interrupt`/`stageClear`/`spellFlag` |
| 10 | 硬约束（§2） | ✅ 未碰 `thprac_games.{h,cpp}` / `thprac.vcxproj(.filters)` / `thprac_locale_def.*` / `wind` / 其它作；**未新增 hook**；未加 `AdvancedGameOptions` 开关；未加 MSBuild 预构建步骤；**未编译** |
| 11 | 语法静态检查 | ✅ 括号/花括号配平（`{}` 607/607、`()` 1654/1654，注释与字符串已剔除）；C++20（`stdcpp20`）下 `Entry` 的 NSDMI + 聚合初始化合法 |
| 12 | 标识符冲突 | ✅ `ClassifySub` / `SubRange` / `OffsetRow` / `SubIndexOfIp` / `SplitOffsetName` / `mEntries` / `kMax*` 在本 TU 内**仅模块内**出现 |

### 6.1 一处口径取舍（记录，不阻塞）

`ST4 前半 #3` 与 `ST4 后半 #1` 入口**同为 `MainSub00`**（方案 §5.2 的同名容忍项）。`mNameMap` 用 `emplace`（= th14 逐字同形）⇒ **先出现者胜**，即命中 `MainSub00` 时显示 `ST4 前半 #3`。

- 方案 §5.2 的「处置」列写「置为**后出现**的那一行」（即 `ST4 后半 #1`）；本实现按 th14 的 `emplace` 取**先出现**的那一行。
- 理由：先出现者胜可保持方案 §8.2 验证 2 明令的**单调性不变式**（「整段值只单调前进或不动，任何回退都是 bug」）。若取后出现者，玩家在 `前半 #3` 时会立刻显示 `后半 #1`，随后到 `前半 #4` 便是**回退**。两行本身「不可区分」（入口报告 §2.2），属口径内已知容忍，**不新增偏移**。
- 真机验证项 8 仍按方案执行（观察实际停在哪个记录名）。

---

## 7. 未真机声明（🟡）

**本机为 Windows 11，可编译可真机；但本子代理按任务边界「不运行 MSBuild、不编译」，且未做任何真机验证。** 因此：

- 本文全部结论均为**静态定案（🟡）**；方案 §8.2 的 13 项真机用例**一项未跑**。
- 以下关键地址/语义**沿用既有逆向报告（同为 🟡 未真机）**，实现阶段**未再复核二进制**：`ECL_MGR_PTR = 0x4C2188`、`mgr+0xB0`（宿主链头）、`H+0x11CC`（res）、`H+0x11D0`/`+0x11D4`（线程链）、`T+4`（指令绝对地址）、`res+8`/`res+0x8C`（count / sub 表）、`res+0x8C` 项 `+4 + 0x10`（首指令）。
- **编译状态**：未编译（由主代理统一构建；若报错请回给本子代理）。

### 7.1 真机验证清单（照方案 §8.2，未跑）

1. 宿主捕获与指针链可解引用（`H+0x11CC == *(u32*)(mgr+0xAC)`）。
2. latch 正确性（道中，ST1 从头打）：`main → 前半 #1 → 前半 #2 → 前半 #3 → 后半 #1 → 后半 #2`，**单调不回退**。
3. **async 三行（最需确认）**：warp 到 `ST1 后半 #2` / `ST4 前半 #1` / `ST5 前半 #1`，入口 ip 应出现在 `H+0x11D4` 起的异步线程上。
4. **偏移消歧（真机已跑 → 见 §8）**：warp 到 `ST5 后半 #4`，帧边界 `ip` 应恰为 `table[MainSub08].+4 + 0x10 + 0x390`；再 warp `ST5 后半 #3` 确认不命中 `+0x390`。**若 `0x54d0` 观测不到（落点被跑穿）⇒ 改用下一等待点或区间匹配（方案 §3.3）。** —— **真机结论：`0x54d0` 观测不到（`goto` 把线程时间归零 ⇒ 同帧跑穿到 `0x5788` 的 `wait(60)`，帧边界 ip = `0x579c`）；已按「区间匹配」修复，见 §8。**
5. 中boss 窗口（ST1）：中boss 存活期间显示**保持 `ST1 MID1`**；`MBossDead` 一出现才变 `ST1 后半 #1`；**慢杀 flush** 与**快杀 flush** 两个分支都要跑。
6. boss 行读数与画面一致（锚点缺口已由报告 §5.4 消解）。
7. 关卡过滤（打 ST2 不得出现 ST1 的记录名）。
8. 同名容忍（`ST4 前半 #3` / `ST4 后半 #1` 实际停在哪个记录名 —— 见 §6.1）。
9. `!HL` 门控（E/N 下 `ST4 前半 #1 = MainSub00h` 不命中，预期容忍）。
10. 生命周期回归（ST1→ST2 连打 / Restart / 自杀 Continue / 回标题再进 / 直接退出）。
11. `STAGE_NUM` 换算（Extra 是否报 9；临时诊断行，验完删除）。
12. 采样时机（`th13_update` 相对 ECL VM 的执行顺序；决定精确匹配是否够用）。
13. 变体（Steam / 汉化版 `stNN.ecl` 是否与原版一致 —— 偏移是**数据偏移**）。

---

## 8. 真机反馈与修复（2026-10-20）

**现象（用户真机，唯一一个 bug）**：`ST5 后半 #4` 读不出来，一直显示 `ST5 后半 #3`；其余 115 行正常。

**根因（离线定案；证据 = `../exes/ecl/th13/st05.ecl` 逐指令解码 + `FUN_004772A0` 反汇编 + `st05.decl` 对照）**

1. **偏移基准与目标地址无误**（排除「首指令不是 `0x5140`」）：`MainSub08` sub 数据基址 `0x5130`（以 `ECLH` 魔数 + `0x10` 开头），首指令 `0x5140`（`id 0x28` = 局部槽分配，decl/thprac 都不渲染它，故 `st05.decl` 的首行是 `wait(20)` = `0x5154`）；`0x54d0 − 0x5140 = 0x390` ✓ —— **CSV 值正确**。
2. **`0x54d0` 是指令边界** ✓（从 `0x5140` 逐条按 `size` 步进可整除），但它**不是一条会阻塞帧的指令**：`0x54d0` = `id 0x2a`（栈 push，对应 `st05.decl:845` 的 `$I0 = 12`），`time` 字段 = 0，**不是 `wait`**。
3. warp `ECLJump(ecl, 0x5154, 0x54d0, 0, 0)` 就地覆写 `0x5154`（`MainSub08` 第 2 条指令，原 `wait(20)`）为 `goto`，`+0x14`（`@ time` / `at_frame`）= **0** ⇒ `goto` 把 `T+0`（线程时间）**赋成 0**、`T+4` = `0x54d0`。
4. **解释器 `FUN_004772A0` 的帧边界判据** = 「下一条指令的 `time` 字段 > `T+0`」（`0x477383`–`0x477397`：`mov edx,[ebx+4]` / `fild dword [edx]` / `fld dword [ebx]` / `fcompp` / `test ah,1` / `je 0x4772d2` 继续循环；不满足则 `fadd [ebp+8]`（`T+0 += dt`）后 `ret 4`）。`0x54d0` 的 `time` = 0 ≤ `T+0` = 0 ⇒ **同帧执行、ip 前进** —— 即方案 §3.3 / §9 预判的「**落点被跑穿**」，与 `th10_checkpoint_ip_offset.md` §2.3/§3.2 的 `ST4 前半 #2` **同型**。
5. 从 `0x54d0` 起，`MainSub08` 内**其后 26 条指令的 `time` 全为 0**（`push` / `$I0=` / `%F0=` / `enmCreate` / 条件 `goto` 都不阻塞），一路跑到 **`0x5788` 的 `wait(60)`**（`id 0x17`，`MainSub08+0x648`）才停下 ⇒ **帧边界 ip = `0x579c`（`MainSub08+0x65C`）**，**永远不是 `0x54d0`**。
   - 附注：`0x54d0` 在**自然流程**里确实是帧边界（`0x54bc` 的 `wait(120)` 之后）—— 只有 **warp 这条路径**观测不到（`goto` 把 `T+0` 归零 ⇒ 落点不再被时间门挡住）。
6. ⇒ 等式 `ip == firstInsn + 0x390` **永不成立** ⇒ 偏移支从不命中 ⇒ 回落 `mNameMap["MainSub08"]` = `ST5 后半 #3`。**与用户现象逐字吻合。**

**排除的其它候选**：**(b)** 首指令 / `+0x10` 算法 —— 已由 `ECLH` 魔数与首指令 `0x5140` 证实（方案 §3.3 的判据正确）；**(c)** 落点非指令边界 / warp 实际落点不是 `0x54d0` —— `0x54d0` 是边界，且 `ECLJump` 写的就是 `dest = 0x54d0`；**(d)** `SubIndexOfIp(0x54d0)` 返回错 sub —— 区间表按 `table[i].+4+0x10` 升序，`0x54d0 ∈ [0x5140, 0x5828)`，返回的正是 `MainSub08`（下一 sub `MapleEnemy` 从 `0x5828` 起），`row.sub == name` 守卫**不会**挡。

**修复（只改 `thprac_th13.cpp`；语义：等式 → 区间 / 位置比较）**

| 位置（改后行号） | 改动 |
| --- | --- |
| `BuildStageMap()`（`:256`–`:279`） | 建完 `mOffsetRows` 后按 `off` **降序** `std::sort`（新增 6 行） |
| `Update()` 偏移支（`:413`–`:449`） | 判据 `ip == firstInsn + row.off` → `ip − firstInsn >= row.off`（新增 `delta` 局部量；`ip >= firstInsn` 由 `SubIndexOfIp` 的返回保证，无下溢） |
| 注释（`SplitOffsetName` `:97-101`、`OffsetRow` `:168-172`、`Update` 偏移支） | 同步改写为「区间 / 位置」口径 + 根因说明 |
| 未动 | CSV / 生成物 / 生成脚本 / 4 处调用 / 中boss 窗口 / `enum addrs` / 安全校验 —— **全部零改动** |

**为什么不能保持等式**：该行的「偏移」是 **warp 的 `goto` 落点**，而落点指令**不阻塞帧**（证据见上 2–5）⇒ 等式**在物理上不可满足**，不是采样时机问题。改挂 `0x4772D2`（方案 §9 备选）也不解决：`goto` 之后 ip 立即是 `0x54d0`，但**同一帧内**就继续前进，除非在解释器里逐指令比对 —— 那是**新增 hook**，本迭代不做（任务硬约束）。th10 doc §3.2 对同类行的处理是「登记**可观测点**」（`0xe0c0`），但那条 (a) 只有 **1 帧窗口**、(b) 需改 CSV（本任务硬约束不改 CSV），且 th13 的可观测点会随采样时机在 `0x5788` / `0x579c` / `0x57c8` 间漂移 ⇒ **区间匹配是唯一稳健解**。

**语义正确性（推广到「同一 sub 多个偏移行」）**：把偏移当**段边界**，取「`off` 降序中第一个 `delta >= off` 的行」= **包含 ip 的最内层段**。对该 sub 的相邻锚点 `off_a < off_b`，判定天然是 `[firstInsn+off_a, firstInsn+off_b)` → A、`[firstInsn+off_b, …)` → B，与「`ip < 该 sub 的下一锚点`」**等价**（无需 sub 末尾，也无跨 sub 比较 —— `row.sub == name` 守卫仍在）。
- **th10 逐条复算（不在本迭代范围，仅验证语义可推广）**：5 个 `main` 锚点（`+0x28` / `+0x2b0` / `+0x928` / `+0x1348` / `+0x1560`）⇒ 5 段；`MainSub8` 两行（`+0x14` / `+0x444`）⇒ 2 段。th10 doc §3.1 的 18 个「可观测 ip」在降序规则下**全部落到原记录名**（17 行锚点即 ip ⇒ `delta == off` ⇒ 命中该行；`ST4 前半 #2` 的 `0xe0c0` = `MainSub1+0x1dc`，CSV 记 `+0x154` ⇒ `delta = 0x1dc ≥ 0x154` 且小于下一锚点 ⇒ 仍是该行）⇒ **与 th10 现登记值一致**。
- **代价（th10 doc §4.3 已记）**：区间把「撞点面」变大（th10 的 `ST2 MID1` → `0xc018` 会落进 `ST2 前半 #3` 的区间）。**th13 侧已核无此风险**：ST5 的 boss / 中boss warp 只改 `main` 的闸门（`0x60e4 → 0x6438` / `0x648c` / `0x64b4`，落点在 `main` / `MainMBoss` / `MainBoss`）⇒ **没有任何 boss warp 会把 ip 落进 `MainSub08`**；且 `MainSub08` 只有 1 行偏移 ⇒ 区间 `[0x54d0, 0x5828)` 内的 ip 只可能来自「#4 warp」或「自然流程走过后半」。

**对 `ST5 后半 #3` 的影响（不破坏）**：`#3` 是**纯名行**（`MainSub08`，走 `mNameMap`）。`#3`（portion 6）warp 后帧边界 ip = **`0x5168`**（`MainSub08+0x28`，紧随被保留的 `wait(20)`）⇒ `delta = 0x28 < 0x390` ⇒ 偏移支不命中 ⇒ 回落纯名 ⇒ 显示 **`ST5 后半 #3`** ✓。只有玩家把 `MainSub08` 玩过 `0x54d0`（自然流程 `wait(120)` 之后）时才前进到 `#4` —— 这正是「段落入口 = 段落起点、走过即前进」的口径（方案 §1），且**只单调前进**（方案 §8.2 验证 2 的不变式）。

**仍待真机（🟡）**：① warp 到 `ST5 后半 #4` 应显示 `ST5 后半 #4`；② warp 到 `ST5 后半 #3` 应先显示 `#3`，并确认「玩到后半时前进到 `#4`」是期望行为；③ 其余 115 行回归不变（尤其 ST4 的 `MainSub08`（`ST4 后半 #3`，无偏移行）不受影响 —— 偏移行按关过滤）。

---

## 9. 日志接线（JSONL 落盘，2026-10-20）

> 需求与口径：`docs/req_logger.md`（口径 11–16）；实现记录：`docs/logger/logger_impl_report.md`（§4.4.1 逐作清单 / §4.4.2 spellFlag）；模板：`thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（逐字同构）。
> 本次**只加日志接线，不改监控逻辑**：`mSection` 裁决、中boss 窗口三态、偏移行语义（`mOffsetRows` 按 `off` 降序 + `delta >= off`）**一行未动**。
> 共享层 `thprac_games.{h,cpp}` 的 `#pragma region Game Session Log` **零改动**（口径 11/12/13/14/16 均已就绪）。

### 9.1 改动清单（`thprac_th13.cpp`，**只增不删**）

| # | 位置（改后行号） | 改动 |
| --- | --- | --- |
| 1 | `:27` `enum addrs` | `SPELL_OBJ_PTR = 0x4c2178,` |
| 2 | `:58–65` 匿名命名空间（`kThreadIpOff` 之后） | `constexpr uintptr_t kSpellFlagsOff = 0x7C;`（含证据注释） |
| 3 | `:129–135`（`IsMidBossEndSub` 之后） | `bool IsStageClearSub(const char* name)`（4 名字精确匹配，见 §9.3） |
| 4 | `:192–194` public 区 | `void OnMiss();` `void OnBomb();` |
| 5 | `:241` private 方法声明 | `void SampleSpellCard();` |
| 6 | `:265–279` private 成员区 | `mSpellFlag` / `mSpellObj` / `mSpellActive` / `mStageClear` / `GameSession mSession` |
| 7 | `:384` `OnStageStart()` 体首 | `mSession.Begin("th13");` |
| 8 | `:389–392` `OnStageStart()` 体末 | 四态复位（`mSpellFlag` / `mSpellObj` / `mSpellActive` / `mStageClear`） |
| 9 | `:398–408`（`OnStageStart()` 之后） | `OnMiss()` / `OnBomb()` 定义（各一行 `PushEvent`） |
| 10 | `:410–440`（`OnBomb()` 之后） | `SampleSpellCard()` 定义 |
| 11 | `:444–458` `Update()` 体首（**所有 early return 之前**） | `inGame` + `SampleSpellCard();` + interrupt 判定 + `mSession.Tick(...)` |
| 12 | `:477–480` `Update()` 换关块（`stageNum != mBuiltStage`） | 四态复位 |
| 13 | `:496` `Update()` 候选变量区 | `bool sawStageClear = false;` |
| 14 | `:543–545` 遍历体内（`IsMidBossEndSub` 之后） | `if (IsStageClearSub(name)) sawStageClear = true;` |
| 15 | `:608`（中boss 窗口之前） | `const char* prevSection = mSection;` |
| 16 | `:644–658`（三类裁决之后、`Latch()` 之前） | 段落变化 ⇒ `--mSpellFlag` + `mStageClear = false`；`sawStageClear` ⇒ `mStageClear = true`；`mSession.Latch(GameLogRecordOfSection(mSection));` |
| 17 | `:2889` `th13_bomb_dec` **既有 lambda 体内** | `TH13Logger::singleton().OnBomb();` |
| 18 | `:2894` `th13_life_dec` **既有 lambda 体内** | `TH13Logger::singleton().OnMiss();` |

**零新增 hook**：`th13_update`（`:2840`）/ `th13_game_start`（`:2881`）/ `THGuiCreate`（`:2933`）三处调用**监控迭代已存在**，本次未动；`OnBomb()` / `OnMiss()` 写进既有 `EHOOK_DY` 的 lambda 体内（同址挂第二个钩子会覆盖第一个，`thprac_hook.cpp:223`），`instr_size` 不变。**`git diff` 纯插入、0 删除**。

### 9.2 (a) in-game 判据

`bool inGame = GetMemContent<uint32_t>(PLAYER_PTR) != 0;`，`PLAYER_PTR = 0x4c22c4`（本作 `enum addrs:21`，既有）。与 IGI 面板显隐判据同源（`thprac_th13.cpp:2846` 的 `GetMemContent(PLAYER_PTR)`），不新增逆向。

### 9.3 (b) `IsStageClearSub` 清单（**本作逐名枚举，不外推**）

对 `../exes/ecl_dump/th13/*.decl` 全 21 份做 `^void .*(Dead|Escape)` 与 `setBoss(-1)` 双向枚举（`setBoss(-1)` 全表：`stNNbs` 的 `BossDead`/`BossEscape`、`st01.decl` + `stNNmbs` 的 `MBossDead`/`MBossEscape`、`st04bs` 的 `BossMiyakoAllDead`/`BossMiyakoEscape`）。

**收录**（`strcmp` 精确匹配）：

| 名字 | 出处 | 证据 |
| --- | --- | --- |
| `BossDead` | `st01bs..st07bs`（各 1） | 关底 `BossCardN` 的 `setNext` 目标（如 `st01bs.decl:648` 的 `BossCard3 → "BossDead"`），`+60:` 后 `setBoss(-1)` |
| `BossEscape` | `st01bs..st07bs`（各 1） | 引擎超时逃逸收尾，`setBoss(-1)` |
| `BossMiyakoAllDead` | **仅 `st04bs`**（`:1070`，`setBoss(-1)` 在 `:1087`） | ST4 是**双 boss 体**战斗（`st04.decl:680/683` `MainBoss()` 里 `enmCreateA("Boss")` + `enmCreateA("BossMiyako")`）；本名字**只**被 `BossDead` 的 `enmCall($GI0,"BossMiyakoAllDead")`（`st04bs.decl:740`）调用 |
| `BossMiyakoEscape` | **仅 `st04bs`**（`:1548`，`setBoss(-1)` 在 `:1560`） | **只**被 `BossEscape` 的 `enmCall($GI0,"BossMiyakoEscape")`（`st04bs.decl:766`）调用 |

**排除**（附理由）：

- `MBossDead` / `MBossEscape`（ST1 内联于 `st01.decl:696/736`，ST2–ST7 用 `stNNmbs.decl`）：**中boss 收尾**，已由 `IsMidBossEndSub` 覆盖，中boss 死亡不是通关。**任一 `*mbs*.decl` 都没有裸 `Boss*Dead|Escape`** ⇒ **无** th16 `st07mbs2/BossDeadB` 式的同名歧义（本作**无 B 路线**、无 `MBossB*` 变体）。
- `BossMiyakoDead`（`st04bs:1520`）/ `BossMiyako2Dead`（`:912`）/ `BossMiyako3Dead`（`:1016`）/ `BossMiyakoCard1Dead`（`:1119`）/ `BossMiyakoCard2Dead`（`:1253`）/ `BossMiyakoCard3Dead`（`:1393`）：都是 ST4 从体（`BossMiyako`）的**相位 / 符卡收尾**，战斗进行中就会跑（`BossMiyakoDead` 甚至 `wait(1000)` 死循环、**不** `setBoss(-1)`）⇒ 收录会在战斗中误置 `mStageClear`。`st04bs` 的 `BossMiyako*Dead` 一族里**只有** `BossMiyakoAllDead` 带 `setBoss(-1)`。

**同名容忍复核**：本作既有的 `ST4 前半 #3` / `ST4 后半 #1` 同 `MainSub00` 是**段落白名单**的同名容忍（取先出现者胜，见 §6.1），与收尾清单无关；收尾清单的 4 个名字**互不重名、也不与任何中boss 收尾重名**（精确 `strcmp` 天然排除前缀匹配）。

### 9.4 (c) 符卡对象指针 + flags 偏移（口径 15）

**本作 `thprac_th13.cpp` 无任何既有符卡状态读取**（`SpellPhase()` `:859` 只是练习菜单的段落 ID 判断，不读引擎符卡对象）⇒ 做了一次**有界的只读逆向**（`pefile` + `capstone`；素材 `../exes/th13.exe`，790528 B，`TimeDateStamp = 1313589413`，`ImageBase = 0x400000`，`RELOCS_STRIPPED` ⇒ RVA = VA − 0x400000）。

| 项 | 值 | 证据（VA） |
| --- | --- | --- |
| 符卡对象全局指针 | **`0x4C2178`** | `0x41E593`（扩展 opcode `0x21E` 处理器）`mov eax,[0x4c2178]; or [eax+0x7c],8`；`0x41E53F` `mov ecx,[0x4c2178]` → `call 0x413270`（声明） |
| 存活位 | **`*(u32*)(obj + 0x7C) & 1`**（bit0） | **置位唯一**：`0x4132DF`（`or ecx,3`）→ `0x4132E2 mov [edi+0x7c],ecx`（声明函数 `0x413270`，入参 `ecx = [0x4c2178]`）；**清位唯一**：`0x413A9D and dword [edi+0x7c],~1`（结算函数 `0x413A60`，`0x413A63 mov edi,[0x4c2178]`） |
| 对象大小 / 构造 | **`0xC0`**，`memset` 归零 | `0x412990`：`push 0xC0` → `call 0x483330`（memset）→ `or [esi],2` → `0x4129A7 mov [0x4c2178],esi`；另一构造点 `0x412C60`（同形）；析构 `0x412C4C mov [0x4c2178],0` |
| opcode 共用 | `spell`/`0x219`、`0x21A`、`spell3`/`0x21B`、`spellEx`/`0x20A` | 扩展 opcode 分发：`0x41B2B5 add edx,-0x12C` → `0x41B2CF movzx edx,byte[0x41F9D8+edx]` → `0x41B2D6 jmp [0x41F760+edx*4]`；`0x219/0x21a/0x21b/0x20a` **同 idx 60 → `0x41E473`**（一个对象、一个槽）；`spellEnd`/`0x20B` idx 61 → `0x41E582` → `call 0x413A60` |
| 一帧一采样不漏 | ✅ | 逐份 `.decl` 枚举 `setNext(...,"*Card*")`：**全部来自非符 sub**（`BossN` / `MBossN`），**0 处 card→card** ⇒ bit0 在每张卡之间至少保持 0 一个非符段 |

**全 `.text` 写点扫描**（对 46 处 `0x4c2178` xref 所在函数 + 全部 `[reg+0x7c]` 写点）：`+0x7C` 的写点里，`or` 的立即数只有 `4 / 0x40 / 8 / 0x10 / 0x80`（bit2/6/3/4/7），`mov [reg+0x7c],eax` 全是失败路径的「读-改-写回」（`and eax,~0x22` / `or eax,0x20`；见 `0x40a610` / `0x40b4e9` / `0x40c94d` / `0x444c3d` 等，形状与 th14 §2.5 一致），**没有一个能置 bit0** ⇒ **bit0 的 0→1 只有 `0x4132DF`、1→0 只有 `0x413A9D`**。

**落地**：`SPELL_OBJ_PTR = 0x4c2178`（`enum addrs`）、`kSpellFlagsOff = 0x7C`（匿名命名空间）、`SampleSpellCard()`（`Update()` 顶部每帧调用，读 `obj+0x7C` bit0 的 1→0 下降沿 ⇒ `++mSpellFlag`）。**对象为空（菜单 / 销毁中）只清采样状态、不清计数**（退出帧还要用）；**对象指针变化（重建）⇒ 三者复位**（`mSpellFlag = 0`）。`mStageClear` **保留为兜底**（关底 `BossDead` 的 `+60` 帧窗口、非符期 `BossEscape` 没有卡结束事件）。

> 本作 `+0x7C` 与 **th14 同**、与 th16 的 `+0x78` **不同** ⇒ 口径 15 的「逐作地址/偏移不可跨作外推」在此再次成立。

### 9.5 shot 字段

取本作 IGI 面板 `diff_pl` 行的**现成来源**：`S(IGI_PL_13[*(byte*)(0x4be7b8)])`，与 `TH13InGameInfo::OnContentUpdate()`（`:1261` 的 `cur_player_type = *(byte*)(0x4BE7B8)`、`:1265` 的 `S(IGI_PL_13[cur_player_type])`）同源。th13 只有机体（Reimu / Marisa / Sanae / Youmu），**无 A/B 子机**（`IGI_PL_13[5]` 只有 4 项）⇒ 串即机体名，与 th14 取 `IGI_PL_14[...]` 的做法一致。**未新增逆向**；求值由 `inGame && mSession.NeedsStartInfo()` 守卫（菜单里不读机体地址）。

### 9.6 顺序约束（三条都不能反，与 th14 逐字同构）

1. `SampleSpellCard()` 在 interrupt 判定**之前** ⇒ 同帧「清卡 + 退出」也能吃到 `++`。
2. `++` 在 `Update()` 顶部、`--` 在段落裁决之后 ⇒ 同帧「卡结束 + 新 checkpoint」净 0；反过来会残留 1，吞掉之后整关的 `interrupt`。
3. 换关复位在采样之后 ⇒ 换关 / 对象销毁的假下降沿当场抹掉；**对象为空时不清 `mSpellFlag`**，否则退出帧没用上就先被清了。

> `--mSpellFlag` 只放在**最终段落比较块**（`prevSection` + `strcmp`）；`prevSection` 快照取在中boss `mPending` 提升**之前**（提升也会改 `mSection`，属同一次段落切换，统一在比较块里消费一次）。

### 9.7 未真机声明（🟡）

- **本子代理未运行 MSBuild、未编译、未真机**（主代理统一构建）。以下均为静态结论：
  - `SPELL_OBJ_PTR = 0x4c2178` / `+0x7C` bit0 / `0x4132DF` 置位 / `0x413A9D` 清位 —— **本次逆向定案**（🟡，未真机读值）。
  - `IsStageClearSub` 的 4 个名字（含 ST4 的 `BossMiyako*` 两个）—— 静态枚举完备，但「每帧采样能否稳定采到」只有真机能证。
- **真机验证项**（照 `logger_impl_report.md` §7）：① 练习启动一局 → 打一段 → 退出，`logger_sessions_YYYYMM.jsonl` 出现一行、字段与 IGI 对得上；② 中途退出（未通关）⇒ `events` 含 `interrupt`；③ 击破关底 / 超时逃逸 ⇒ **无** `interrupt`；④ **ST4 双 boss 体**：击破（`BossDead` + `BossMiyakoAllDead`）与超时（`BossEscape` + `BossMiyakoEscape`）都应**无** `interrupt`；⑤ 符卡结束（bit0 1→0）后立刻退出、checkpoint 尚未切换 ⇒ **无** `interrupt`（`mSpellFlag` 抑制生效）；⑥ 单章节练习连续重开 ⇒ 合并为 1 行（口径 11/13/16）。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **日志接线（JSONL 落盘）**：在 `TH13Logger` 内新增 `GameSession mSession` / `mSpellFlag` / `mSpellObj` / `mSpellActive` / `mStageClear` + `OnMiss()` / `OnBomb()` / `SampleSpellCard()`，18 处接线（见 §9.1，**只增不删**；`git diff` 0 删除）。**(a)** in-game 判据 = 既有 `PLAYER_PTR = 0x4c22c4`。**(b)** `IsStageClearSub` = `BossDead` / `BossEscape` / `BossMiyakoAllDead` / `BossMiyakoEscape`（逐名枚举 `../exes/ecl_dump/th13/*.decl` 21 份 + `setBoss(-1)` 全表；ST4 是双 boss 体，两个 `BossMiyako*` 只被 `BossDead`/`BossEscape` 的 `enmCall` 调用 ⇒ 冗余但安全；排除中boss `MBoss*` 与 ST4 从体的相位收尾 `BossMiyako{2,3}Dead` / `BossMiyakoCard{1,2,3}Dead`）。**(c)** 符卡对象全局 **`0x4C2178`**、存活位 **`+0x7C` bit0**（置位唯一 `0x4132DF`、清位唯一 `0x413A9D`；对象 `0xC0`、构造 memset 归零；`spell`/`0x219`/`0x21A`/`spell3`/`0x21B`/`spellEx`/`0x20A` 共用；逐份 decl 枚举**0 处 card→card**）—— 本作**无既有符卡信号可复用**，故做了一次有界只读逆向（`pefile` + `capstone`）。**shot** 取 IGI 现成来源 `S(IGI_PL_13[*(byte*)(0x4BE7B8)])`。**共享层 `thprac_games.{h,cpp}` 零改动**；**零新增 hook**；监控逻辑（中boss 窗口 / 偏移行语义）一行未动。**未运行 MSBuild、未编译、未真机（🟡）**。 |
| 2026-10-20 | **真机反馈修复：`ST5 后半 #4` 偏移匹配从等式改为区间/位置比较**（新增 §8；§0 速览「偏移匹配」行、§4 判据行同步改写）。**根因**：`MainSub08+0x390` 的锚点 `0x54d0` 是**非阻塞指令**（`id 0x2a` 栈 push，`time`=0），而 warp 的 `goto`（`ECLJump(ecl, 0x5154, 0x54d0, 0, 0)`，`at_frame`=0）把线程时间归零 ⇒ 解释器（`FUN_004772A0`，帧边界判据 = `下一指令 time > T+0`，`0x477383`–`0x477397`）**同帧跑穿**到 `0x5788` 的 `wait(60)` ⇒ 帧边界 ip = `0x579c`（`MainSub08+0x65C`），**永不等于 `0x54d0`** ⇒ 偏移支从不命中 ⇒ 回落 `mNameMap["MainSub08"]` = `ST5 后半 #3`（与现象逐字吻合）。**修复**：`BuildStageMap()` 把 `mOffsetRows` 按 `off` **降序**排序；`Update()` 偏移支判据 `ip == firstInsn + off` → `ip − firstInsn >= off`（= 取包含 ip 的最内层段）。**为何不能保持等式**：落点指令不阻塞帧 ⇒ 等式物理上不可满足（改挂 `0x4772D2` 亦无效，除非逐指令比对 = 新增 hook，本迭代不做；th10 §3.2 的「登记可观测点」只有 1 帧窗口且需改 CSV，本任务硬约束不改 CSV）。**推广性**：降序规则 = 「`ip < 下一锚点`」等价形式，逐条复算 **th10 的 18 行全部落到原记录名**（含 `MainSub8` 两行、`ST4 前半 #2` 的跑穿行）；th13 侧已核**无 boss warp 落进 `MainSub08`**。**不破坏 `#3`**：`#3` 走纯名 `mNameMap`，其 warp 后 ip = `0x5168`（`delta = 0x28 < 0x390`）⇒ 仍显示 `ST5 后半 #3`。改动**仅限** `thprac_th13.cpp` 的 2 处逻辑 + 3 处注释；CSV / 生成物 / 生成脚本 / 4 处调用 / 中boss 窗口 / 工程登记**零改动**；**未编译、未真机（🟡）**。 |
| 2026-10-20 | **建档（th13 监控 E5 实现）**。按 `docs/th13/th13_monitor_plan.md` 逐条落地：① 新增 `docs/logger/tools/gen_th13_checkpoints.py`（复制 th14 版改 5 处；已运行，自校验「逐字节一致」通过、幂等）；② 生成物 `thprac/src/thprac/thprac_th13_checkpoints_gen.cpp` 入库（`THPrac::TH13::kTh13CheckpointsCsv`，3488 B）；③ `thprac_th13.cpp` 补 include + `ECL_MGR_PTR = 0x4c2188` + `extern` 声明 + `TH13Logger` 模块（`enum addrs` 之后、`THPracParam` 之前）+ 4 处调用；④ 中boss 死亡 sub 名**离线枚举** `../exes/ecl_dump/th13/*.decl` 全 21 份，得 `MBossDead` / `MBossEscape`（**与方案 §6 一致**，无 `MBossB*` 变体；ST1 内联于 `st01.decl`、已复核不漏）。核心机制：`th13_update`（`0x470C04`）每帧遍历 `mgr+0xB0` 全局宿主链 × 每宿主 `H+0x11D0` 线程链 → `ip = *(u32*)(T+4)` → 地址升序区间反查 → **偏移命中优先的双匹配** → 白名单 latch（未命中 no-op、首命中前 `main`）+ th16 §5.7 的中boss 窗口三态；不缓存任何指针。**未运行 MSBuild、未编译、未真机（🟡）**；`thprac.vcxproj` / `.filters` 的登记由主代理统一处理。 |

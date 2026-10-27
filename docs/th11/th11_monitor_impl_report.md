# TH11 ECL 段落监控 — 实施记录（E5）

> 上位文档（术语 / 判定口径 / 粒度，一律以此为准）：`docs/logger/logger_re_disasm_overview.md`（**§1.2 粒度 / §2 术语表 / §5.1 锚点总表 / §6 Q9 / §9.9**）。
> 需求来源：`docs/req_logger.md`。**监控**（显示行 + 每帧采样 + 重置 + 预构造）见 §1–§8；**日志落盘**（JSONL / `GameSession` / `OnMiss` / `OnBomb` / `interrupt` / `stageClear` / `spellFlag`）见 **§9**（2026-10-20 追加；此前 §1–§8 中「本迭代只做监控、不做落盘」的表述是**监控迭代当时**的范围声明，已被 §9 取代）。
> 本作逆向结论：`docs/th11/th11_ecl_re_report.md`（Q1–Q9 静态定案；**§1 指针链 / §2.2 地址语义 / §2.3 区间反查 / §3 sub 表 / §5.4 Q4 / §5A boss 宿主可达性 / §10.4–10.5 白名单与 async**）。
> 本作入口表小结：`docs/th11/th11_checkpoint_entry_report.md`（CSV 第 1 列 **125/125**：章节 37 + boss 88；§2 st4 六机体 18 行 / §4.1 async 落点 / §4.3 四处存疑）。
> 白名单数据：`docs/logger/th11_checkpoints.csv`（**125 数据行**）。
> 参考实现（权威，真机验证过）：`docs/th16/th16_monitor_impl_report.md`（**§5.7 中boss 窗口** / §5.3 安全校验 / §6 工程登记）。
> 直接模板（与本作同形：节点 `+4` = 指令绝对地址）：`thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（`:620`–`:1130`）。
> 二进制指纹：`../exes/th11.exe`，**688128 B**，PE `TimeDateStamp = 1217734394`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）。
> 方法：**静态实现**——引用已定案的逆向结论 + 离线枚举 `reasm/exes/ecl_dump/th11/*.decl`（20 份）+ 只读核对 `thprac_th11.cpp`。**未编译、未真机、未改任何二进制**。
> 状态：**已实现（🟡 静态定案，未真机三态）**。按 AGENTS.md §2.5，未真机验证的地址不得写成结论。
> 地址口径：正文 `VA` 与 `RVA = VA − 0x400000` 并列。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 采用方案 | **方案二**（读 ECL 引擎当前 sub 名 + 白名单 latch），与 th14/th16 同一机制，**不需要**名字→阶段映射表 |
| 采样点 | **`th11_update`（`0x456deb`）每帧轮询**（复用既有 EHOOK，不新增钩子），位置与 th14 一致：插在 `TH11InGameInfo::singleton().Update();` 之前 |
| 取值范围 | 遍历 `mgr+0x68` **全局宿主链** × 每宿主 `H+0x1030` **线程链**，一次覆盖主线 + 中boss + 关底 boss + async |
| ECL 资源取法 | **按每个宿主读 `H+0x102C`**（≡ `*(u32*)(mgr+0x64)`；本作所有宿主共用同一份 `res` ⇒ 与 th16 的「必须按宿主取」不同） |
| 「当前 sub」取法 | `ip = *(u32*)(T+4)` = **当前指令绝对地址**（B 代语义，**不是** sub 索引）⇒ 用 `table[i].+4+0x10` 建**地址升序区间表**反查（`BuildIpMap` / `SubOfIp`，与 th14 逐字同形） |
| 指针生命周期 | **不缓存任何宿主/线程/表指针**，每帧从 `0x4A8D7C` 重走链；`mIpMapCount`/`mIpMapTable` 只作变化检测、不解引用 |
| 白名单进二进制 | 生成脚本 → raw string 嵌入 `.cpp`（生成物入库，不加 MSBuild 预构建步骤），运行期 `rapidcsv` 解析 |
| 偏移行（`sub名+0x偏移`） | **本作 CSV 无偏移行**（125 行全是纯 sub 名）⇒ `mOffsetRows` 恒空；仍按 th10–th13 双匹配骨架实现（解析期拆 `+0x` 后缀、**偏移命中优先**），以保持四作结构一致 |
| 中boss 窗口 | **移植 th16 §5.7**（`mPending` / `mMidBossActive` / **`mMidBossDone`** / 三类命中裁决）；死亡 / 逃逸 sub 名**逐名精确匹配**（离线枚举，见 §3） |
| 显示 | IGI 面板 bomb 计数行下方新增一行；标签硬编码中文 `"当前段落"`（**未动 `thprac_locale_def.*`**） |
| 开关 | 不新增 `AdvancedGameOptions`，跟着 IGI（F7）走 |
| 代码量 | `thprac_th11.cpp` **+469 行、0 删除**（头文件 `+7` / 模块 `:643-1096` 共 454 行 / 4 处调用 `+7` / 分隔空行 `+1`）；新增 2 文件（生成脚本 + 生成物） |

---

## 1. 指针链（TH11 定案，全部引自 `th11_ecl_re_report.md`）

```text
mgr    = *(u32*)0x4A8D7C                       // 全局 ECL 管理器（enum addrs: ENEMY_MGR_PTR）
node   = *(u32*)(mgr + 0x68)                   // 宿主链头；节点 = H+0x11A4 = {+0 = H, +4 = next, +8 = prev}
host   = *(u32*)node
res    = *(u32*)(host + 0x102C)                // ≡ *(u32*)(mgr + 0x64)；所有宿主共用同一份 res
count  = *(i32*)(res + 8)                      // sub 总数（越界保护）
table  = *(u32*)(res + 0x8C)                   // sub 表；项 8B：+0 = 名字 char*，+4 = sub 数据基址
first_i= *(u32*)(table + i*8 + 4) + 0x10       // 第 i 个 sub 的首条指令（FindSubByName @0x45DB8B）
entry  = host + 0x1030                         // 线程链头（哨兵内嵌于宿主）
thread = *(u32*)entry                          // [+0] = 主线线程（= host+8）；其余为 async 线程
ip     = *(u32*)(thread + 4)                   // 当前指令绝对地址；0 = 线程结束
STAGE_NUM = *(i32*)0x4A5728                    // = globals->stage（enum addrs: STAGE_NUM）
```

- **`host+4` 不用**：它是「本帧最后执行过的线程」，会被 async 覆盖（报告 §5.3 / §10.5）；本作段入口**大量是 `async` 落点**，只读主线会漏 latch。
- **宿主链必须遍历**：`mgr+0x68` 是**全局全宿主链**（唯一宿主工厂 `FUN_004108F0` 尾插、无旁路；boss / 中boss 宿主也在链上，报告 §5A）⇒ boss 行与中boss 行都靠它。
- **`enum addrs` 无需新增常量**：`STAGE_NUM`（`0x4a5728`）与 `ENEMY_MGR_PTR`（`0x4a8d7c`）本作早已登记，模块直接复用（同 th16 §10 的教训：不重复定义地址常量）。

**关键常量取值（本作）**

| 常量 | 值 | 含义 |
| --- | --- | --- |
| `kMgrHostHeadOff` | `0x68` | `mgr` → 宿主链头 |
| `kHostThreadListOff` | `0x1030` | `host` → 线程链头（首节点线程 = `host+8`） |
| `kHostResOff` | `0x102C` | `host` → `res` |
| `kThreadIpOff` | `4` | `thread` → 当前指令绝对地址 |
| `kResCountOff` / `kResTableOff` | `8` / `0x8C` | `res` → sub 数 / sub 表 |
| `kSubEntrySize` / `kSubDataOff` / `kSubInsnOff` | `8` / `4` / `0x10` | 表项大小 / 数据基址 / 首指令偏移 |
| `kMaxCount` / `kMaxHosts` / `kMaxThreads` | `8192` / `1024` / `64` | 安全上限（同 th14/th16） |

---

## 2. 改动清单

### 2.1 新增文件（2）

| 文件 | 内容 |
| --- | --- |
| `docs/logger/tools/gen_th11_checkpoints.py` | 由 `gen_th14_checkpoints.py` 复制，只改 5 处：`CSV_PATH` → `docs/logger/th11_checkpoints.csv`、`OUT_PATH` → `thprac/src/thprac/thprac_th11_checkpoints_gen.cpp`、`RAW_DELIM = "TH11CSV"`、命名空间 `THPrac::TH11`、符号 `kTh11CheckpointsCsv`。幂等；断言正文不含终止符 `)TH11CSV"`；**自校验逐字节一致** |
| `thprac/src/thprac/thprac_th11_checkpoints_gen.cpp` | **生成物（入库）**：`namespace THPrac { namespace TH11 { extern const char kTh11CheckpointsCsv[] = R"TH11CSV(…)TH11CSV"; } }`（CSV 3882 B 逐字节嵌入） |

生成脚本运行结果（本机已跑）：

```text
wrote thprac\src\thprac\thprac_th11_checkpoints_gen.cpp
  csv bytes: 3882  embedded bytes: 3882
  self-check: embedded body is byte-identical to the csv
```

### 2.2 改动既有文件（1）

`thprac/src/thprac/thprac_th11.cpp`（**+469 行 / −0 行**，无任何既有行为改动）：

| # | 行号区间 | 内容 |
| --- | --- | --- |
| ① | `:3-10` | 头文件补 include：`"..\3rdParties\rapidcsv\rapidcsv.h"`、`<algorithm>`、`<cstdio>`、`<cstring>`、`<sstream>`、`<string_view>`、`<unordered_map>`（`<format>` / `"thprac_res.h"` 原位保留） |
| ② | `:645` | `extern const char kTh11CheckpointsCsv[];` 声明 |
| ③ | `:647-760` | 匿名命名空间：链偏移 / sub 表布局常量、`ClassifySub`、`IsMidBossEndSub`、`SplitOffsetKey`、遍历上限 |
| ④ | `:764-845` | `class TH11Logger`（`SINGLETON`）声明：接口仅 `OnStageStart()` / `Update()` / `CurrentSection()` |
| ⑤ | `:847-850` | 构造函数（跑一次 `LoadCsv`） |
| ⑥ | `:852-878` | `LoadCsv()`：`rapidcsv` 解析嵌入 CSV，跳过列数 < 4 与 `第2列 != "th11"`，解析期拆 `+0x` 后缀 |
| ⑦ | `:880-899` | `BuildStageMap()`：按 `ST%d` 过滤本关 → `mStageMap`（纯名）+ `mOffsetRows`（偏移行，本作恒空） |
| ⑧ | `:901-920` | `BuildIpMap()`：地址升序 `SubRange{start,index}`（`(count, table)` 变化时重建） |
| ⑨ | `:922-936` | `SubOfIp()`：`upper_bound` 反查 `(index, name)` |
| ⑩ | `:938-948` | `OnStageStart()`：`mSection = "main"`、清中boss 三态、失效区间表（**零堆分配**，VEH 回调内安全） |
| ⑪ | `:950-1096` | `Update()`：`STAGE_NUM` 过滤 → 宿主链 × 线程链 → 双匹配 → 三类裁决 |
| ⑫ | `:1157-1160` | `TH11InGameInfo::OnContentUpdate()`：bomb 计数行之后插「当前段落」标签 / 值两格（保持既有 `Columns(2)` 成对结构） |
| ⑬ | `:2793` | `EHOOK_DY(th11_update, 0x456deb, 1, …)`：在 `TH11InGameInfo::singleton().Update();` 之前加 `TH11Logger::singleton().Update();` |
| ⑭ | `:2830` | `EHOOK_DY(th11_game_start, 0x41FA6D, 3, …)`：**写在既有 lambda 体内**加 `TH11Logger::singleton().OnStageStart();` |
| ⑮ | `:2879` | `THGuiCreate()`：在 `TH11InGameInfo::singleton();` 旁加 `TH11Logger::singleton();`（预构造，跑一次 `LoadCsv`） |

> ⚠ `th11_game_start` 的接线**必须写在既有 lambda 体内**：hook 框架对带回调的钩子用 INT3 + `vehHooks[addr] = this`（`thprac_hook.cpp:223`），**同址挂第二个钩子会覆盖第一个**，会让既有的 miss/bomb 计数清零失效。
> **未碰**：`thprac/thprac.vcxproj` / `.filters`（由主代理统一登记）、`thprac_locale_def.*`、`thprac_games.{h,cpp}`、其它作的任何文件、`wind`。

### 2.3 生命周期

| 事件 | 处理 |
| --- | --- |
| 一局开始（含重开） | `th11_game_start`（`0x41FA6D`）→ `OnStageStart()`：`mSection = "main"` + 清中boss 三态 + 失效区间表 |
| 换关 / 练习 warp | `Update()` 内 `STAGE_NUM` 变化 → `BuildStageMap(new)` + `mSection = "main"` + 清中boss 三态 |
| 退出一局（`PLAYER_PTR == 0`） | **故意不重置**（需求要求保留最后 checkpoint；IGI 此时本就不绘制） |
| 菜单 / 非关卡态 | `STAGE_NUM ∉ {1..7}` 直接 return，不改状态 |
| 中boss 战结束 | 见 §3 |

---

## 3. 中boss 窗口口径（移植 th16 §5.7）

**为什么必需**：`stage01.decl:2270-2272` 的 `main()` 在 `enmCreateA("MBoss", …)` 后 `wait(600)` 就 `@MainSub06()`（= `ST1 后半 #1`）⇒ 中boss 存活期间主线程就会 latch `后半 #1`（th16 的「增援章节」误报同一模式）。

| 项 | 口径（沿用 th16，**不改 CSV**） |
| --- | --- |
| 命中分类 | 按**白名单键前缀**：`MBoss*` = 中boss、`Boss*` = 关底 boss、其余（`MainSub*`）= 章节 |
| 中boss 战结束 | 当前 sub 名命中**死亡 / 逃逸 sub 名**（**逐名精确匹配**，见下） |
| 裁决 | `bossHit` → 关窗并清 pending、显示关底记录；否则 `midHit && !mMidBossDone` → 开窗、显示中boss 记录；否则 `chapHit` → 窗口内**只存 `mPending` 不显示**，窗口外正常显示 |
| 关窗 | 死亡 / 逃逸 sub 名一出现即关窗并把 `mPending` **补显示出来**（取最后一个）；`mPending` 为空则维持中boss 记录 |
| 采样点 | `th11_update`（`0x456deb`）每帧；同帧内「主线程进 `MainSub06`」与「`MBoss` 命中」都可见，靠裁决优先级处理 |
| 新增钩子 | **无**。只复用 walk 已读到的 `name` 指针；`IsMidBossEndSub()` 只做 `strcmp` |

**中boss 死亡 / 逃逸 sub 名（离线枚举 `reasm/exes/ecl_dump/th11/*.decl` 全 20 份，逐名精确匹配）**

| sub 名 | 出现位置 | 备注 |
| --- | --- | --- |
| `MBossDead` | `stage01:1281`、`stage02:1527`、`stage03:2023`、`stage06mboss:113`、`stage07mboss:476` | 各文件内**唯一**的 `setBoss(-1)` 收尾之一 |
| `MBossEscape` | `stage01:1319`、`stage02:1565`、`stage03:2057`、`stage04:1403`、`stage05mboss:546` | 超时 / 逃逸收尾 |
| `MBossEscape2` | `stage04:1430` | **ST4 独有**：ST4 的中boss 有两条逃逸路径（`stage04.decl:1282` 直接 `@MBossEscape()`、`:1304` `setNext(0, 0, 1590, "MBossEscape2")`），且**没有** `MBossDead` |

- **必须精确匹配**：松匹配（「含 `Dead`」）会误命中 `stage04:1224 Girl3Dead`、`stage03:1449 Cir01Boss2Dead`、`stage01:2111 RockBDead` / `:2173 RockMDead`、`stage05boss:299 Boss1GirlDead` / `:1262 BossCard4GirlDead` 等**敌人 / 符卡结束动画**（中boss 存活时就会出现）。
- **`mMidBossDone` 在 th11 是必需的**（与 th14 的「不必要」相反）：th11 的白名单**收录了中boss 容器 sub 本身**（`MBoss` = ST1/ST2/ST4 MID1 **与 ST3 后半 #1**；`MBoss2` = ST1/ST4 MID2；`MBoss1` = ST3/ST5 MID1…）。没有这个标志，战后一次迟到的 `MBoss` 命中就会重开窗口且永不关闭，吞掉本关之后所有章节 checkpoint。
- 残余：`ST3` 的中boss 逃逸 sub（`stage03.decl:2110`）**自循环不 delete** ⇒ `sawMidBossEnd` 每帧为真；因 `mMidBossActive` 已为假，该条件恒为 no-op（无副作用，如实记录）。

---

## 4. 未真机声明（🟡）与已知存疑

> 本机**未运行 th11**；下列**全部未做**，状态一律 🟡（AGENTS.md §2.5：未真机验证的地址不得写成结论）。**本迭代亦未编译**（由主代理统一构建）。

### 4.1 未真机项（照 `th11_ecl_re_report.md` §13 / 入口报告 §5）

1. **宿主链 × 线程链读数**：进关后 `H+0x1030` / `H+0x102C` 可解引用，且 `H+0x102C == *(u32*)(*(u32*)0x4A8D7C + 0x64)`。
2. **`ip → sub` 反查**：道中 / boss / 不同段落各采样一次，`SubOfIp` 结果与画面进度一致。
3. **async 落点（本作重点）**：本作段入口**大量是 `async` 落点**（入口报告 §4.1）⇒ 须确认遍历线程链后这些行能 latch（只读 `H+0x1030` 首节点会漏）。
4. **boss 行**：`mgr+0x68` 为全局全宿主链（报告 §5A 静态定案）⇒ boss / 中boss 宿主可达；只余「读数与画面一致」待真机。
5. **ST4 六机体分支 18 行**：`SetFile(2..7)` 切换后 `BossCard{2,3,4}_0{0,1}{A,B,C}` 仍能从同一张 `res+0x8C` 表反查到（报告 §5A.3 Q-C 静态定案：所有宿主共用一份 `res`、表跨文件合并；`stage4c0*.decl` 中 18 个名字均在）。**若真机发现取不到这些名字，须回报主代理**（不自行改架构）。
6. **`STAGE_NUM` 换算**：本作 Extra 在 thprac 全部表里都是 **7**（`mChapterSetup` / `th_sections_cba` 按 1..7 索引，7 = Extra；`thprac_th11.cpp:620` 的既有 time-lock 钩子（`EHOOK_HK 0x45d2a3`）即用 `globals->stage == 7` 判 Extra）⇒ **预期报 7、不需要 9→7 折叠**；代码仍内置与 th14 相同的 `9 → 7`（**防御性，🟡 未真机**）。
7. **练习跳章 / `ECLSatoriJump`**：warp 后 `T+4` 名字与白名单置值正确；`ins[0xA]=0xFF` 掩码在 E/N/H/L 四难度下均能执行。
8. **变体**：Steam / 汉化版是否共用同一 RVA。

### 4.2 入口报告 §4.3 的四处已知存疑（**照实记录，本实现不试图消除**）

1. **`ST3 后半 #1 = MBoss`**：本作唯一的「非 `call` 落点」章节行（落在 `main` 内 `enmCreate "MBoss"` 上）。**本实现的后果**：该行的键以 `MBoss` 开头 ⇒ 被 `ClassifySub` 判为**中boss 类**，即它会**开中boss 窗口**并显示 `ST3 后半 #1`（与「中boss 出现 = 后半开始」的语义自洽），随后由 `ST3 MID1..MID3` 接管、`MBossDead` / `MBossEscape` 关窗。
2. **ST3 / ST5 / ST6 / ST7 的 MID1「容器 vs 战斗 sub」**：同一处 `enmCreate` 落点，章节行取容器、boss 行取战斗 sub，口径不统一。CSV 原样消费，未做特判。
3. **`ST3 MID2 = MBoss2`**：依赖 thprac 的二次跳转注释（`ECLJump(ecl, 0xb458, 0xb6c4)`），静态可核、未真机。
4. **`ST7 前半 #4 / 后半 #3` 疑似 thprac off-by-one**：`stage07.decl` 的 `main()` 里 `MainSub03`（CSV = `ST7 后半 #3`）被调用**两次**——第一次在 `MainSub02` 之后、**早于** `MainSub04`（即 `前半 #4` 的入口）与 `MainSub05/06`，第二次在 `MainSub06` 之后（调用序 `MainSub00 01 02 03 04 [MBoss] 05 06 03 07`）⇒ `后半 #3` 可能**提前 latch**；CSV 与 thprac 偏移逐字一致，疑点在 thprac 自身偏移。

### 4.3 ⚠ 静态发现：章节 sub **被复用** ⇒ 面板可能回退（**非实现 bug，口径固有**）

离线核对 7 关 `main()` 的 `MainSubNN` 调用序（`reasm/exes/ecl_dump/th11/stage0*.decl`）发现**同名章节 sub 在关内被调用多次**，而 latch 只认 sub 名 ⇒ 显示会**回退**：

| 关 | `main()` 调用序（仅 `MainSub*`） | 后果 |
| --- | --- | --- |
| ST1 | `00 01 02 03 04 05 06 07 **03** **07** 09 10 09 10` | 中boss 后先 `后半 #2`(`MainSub07`) → **回退 `前半 #2`**(`MainSub03`) → 再 `后半 #2` |
| ST4 | `00 01 03 02 04 00b 01b 05 06 03b 02b 04 06b 02 03 04 05 06` | `MainSub03`(`前半 #2`)、`MainSub04`(`前半 #3`/`后半 #3` 同名)、`MainSub06`(`后半 #2`) 均多次调用 ⇒ 多次回退 |
| ST5 | `00 01 02 03 04 05 06 **00**` | 关底 boss 前再调 `MainSub00` ⇒ 回退 `前半 #1` |
| ST7 | `00 01 02 03 04 05 06 **03** 07` | 第二次 `MainSub03` 仍是 `后半 #3`（**同值，无可见变化**） |

- 起因：warp 落点是「**该关某条 `call`/`async call`**」，而 th11 的 `main` 是**纯容器**、章节 sub 被复用（增援波 / 二次演出）⇒ 同名不同段。
- 处置：**本迭代不修**（修 = 改白名单 / 改粒度口径，属 E3/E4 范围，且 `thprac` 的 warp 偏移本就按第一次调用登记）。th16 的验证清单把「回退」列为 bug，但此处**不是实现缺陷**——真机若确认影响体验，须由主代理决定是否改口径。

---

## 9. 日志接线（JSONL 落盘，口径 11/12/13/15/16）

> 本迭代只加**日志接线**，不改监控逻辑。模板 = `thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（`docs/logger/logger_impl_report.md` §4 / §4.4 / §4.4.2）；口径 = `docs/req_logger.md` 口径 11–16；共享层 `thprac_games.{h,cpp}` 的 `#pragma region Game Session Log` **零改动**。
> 状态：**已接线（🟡 未编译、未真机）**。主代理统一构建，本子代理**不编译**。

### 9.1 改动清单（`thprac/src/thprac/thprac_th11.cpp`，本迭代 **+161 行 / −0 行**，纯插入）

> 行数口径：本迭代日志接线 **+161 / −0**；监控迭代（未提交，本报告 §2）**+469 / −0** ⇒ `git diff HEAD` 对本文件合计 **+630 / −0**。

| # | 位置（落地后行号） | 内容 |
| --- | --- | --- |
| ① | `:59` | `enum addrs` 增 `SPELL_OBJ_PTR = 0x4a8d6c,` |
| ② | `:673-683` | 匿名命名空间增 `kSpellFlagsOff = 0x8E0`（含逆向证据注释） |
| ③ | `:730-753` | 匿名命名空间增 `IsStageClearSub()`（名单见 §9.3） |
| ④ | `:818-819` | `TH11Logger` public 增 `void OnMiss(); void OnBomb();` |
| ⑤ | `:863` | private 增 `void SampleSpellCard();` 声明 |
| ⑥ | `:893-902` | private 成员增 `mSpellFlag` / `mSpellObj` / `mSpellActive` / `mStageClear` / `mSession` |
| ⑦ | `:996-1008` | `OnStageStart()`：体首 `mSession.Begin("th11");` + 五者复位 |
| ⑧ | `:1013-1023` | `OnMiss()` / `OnBomb()` 定义（各一行 `PushEvent`） |
| ⑨ | `:1025-1060` | `SampleSpellCard()` 定义（逐字同构 th14 `:931-960`） |
| ⑩ | `:1064-1079` | `Update()` 体首（**早于所有 early return**）：`inGame` + `SampleSpellCard()` + `interrupt` 入队 + `Tick()` |
| ⑪ | `:1102-1105` | `Update()` 换关块（`stageNum != mBuiltStage`）四者复位 |
| ⑫ | `:1121` / `:1163-1165` | `bool sawStageClear = false;` / `if (IsStageClearSub(name)) sawStageClear = true;` |
| ⑬ | `:1206` | `const char* prevSection = mSection;`（在中boss 窗口裁决**之前**） |
| ⑭ | `:1238-1253` | `Update()` 体末：段落切换 ⇒ `--mSpellFlag` + `mStageClear = false`；`sawStageClear` ⇒ 置位；`mSession.Latch()` |
| ⑮ | `:2988` | `th11_game_start`（`0x41FA6D`）既有 lambda 体内 `OnStageStart()`（**监控迭代已有**） |
| ⑯ | `:2993` / `:2998` | `th11_bomb_dec`（`0x4311E6`）/ `th11_bomb_dec2`（`0x431293`）**既有 lambda 体内**各加 `OnBomb()` |
| ⑰ | `:3003` | `th11_life_dec`（`0x4327F0`）**既有 lambda 体内**加 `OnMiss()` |
| ⑱ | `:2951` | `th11_update`（`0x456deb`）既有 lambda 内 `TH11Logger::singleton().Update();`（**监控迭代已有**） |
| ⑲ | `:3040` | `THGuiCreate()` 增 `TH11Logger::singleton();` 预构造 |

- **未新增任何 EHOOK**：⑯⑰ 全部写在**既有 EHOOK 的 lambda 体内**（同址二次 hook 会覆盖前一个，`thprac_hook.cpp:223`）。
- **未碰**：`thprac.vcxproj` / `.filters`、`thprac_games.{h,cpp}`、`thprac_locale_def.*`、`docs/logger/logger_progress.md`、`docs/logger/knownBugs.md`、其它作的任何文件、`reasm/wind`。
- `git diff --check` 通过；剥离注释/字符串后的 `()[]{}` 括号平衡检查通过。

### 9.2 (a) in-game 判据（本作自定，**不外推**）

| 项 | 结论 |
| --- | --- |
| 判据 | `bool inGame = GetMemContent<uint32_t>(0x4a8eb4) != 0;`（与 th14 的 `GetMemContent<uint32_t>(PLAYER_PTR)` 同形） |
| 依据 | `0x4A8EB4` 就是本文件 `:51` 的生产宏 `#define player (*(Player**)0x4a8eb4)` —— **玩家对象指针**。本作 IGI 面板的**显隐判据**正是它：`thprac_th11.cpp:1323`（`OnPreUpdate` 里 `if (*(DWORD*)(0x4A8EB4)) GameUpdateInner(11);`）与 `:1327`（`if (*(THOverlay::singleton().mInGameInfo) && *(DWORD*)(0x4A8EB4)) { Open(); } else { Close(); }`）；键盘监视器 `:2964` 也用同一判据 |
| 「退出回菜单归零」核实 | **成立**（生产信号）：IGI 面板只在 `0x4A8EB4 != 0` 时 `Open()`、否则 `Close()`，若它在菜单里不归零，IGI 面板就会在标题画面常驻显示 —— 与长期使用的实际行为矛盾。⇒ 与口径 3「一局边界直接复用各作 IGI 面板的显隐判据，不新增逆向」一致 |
| 额外旁证 | `th11.exe` 的符卡失败路径函数 `0x406C37` 之后紧接着 `mov eax,[0x4a8eb4]`（`0x406C8A`），即游戏自身把「玩家对象」当作战状态根对象使用 |

### 9.3 (b) `IsStageClearSub` 清单（逐名枚举 `reasm/exes/ecl_dump/th11/*.decl` 全 20 份）

**收录（`strcmp` 精确匹配）：`BossDead`** —— 唯一一个。

全量 `^void *Dead|*Escape` 枚举结果（12 个名字）与逐名裁决：

| 名字 | 出现位置 | 裁决 | 理由 |
| --- | --- | --- | --- |
| **`BossDead`** | `stage01:590` `stage02:874` `stage03:1251` `stage04:809` `stage05boss:1463` `stage06boss:1667` `stage07boss:2365`（ST7 = Extra） | ✅ **收录** | 7 关关底 boss 的**唯一**收尾 sub，与 `setBoss(-1)` 站点一一对应（`stage01:621`、`stage02:905`、`stage03:1283`、`stage04:841`、`stage05boss:1496`、`stage06boss:1687`、`stage07boss:2385`） |
| `MBossDead` | `stage01:1281` `stage02:1527` `stage03:2023` `stage06mboss:113` `stage07mboss:476` | ❌ 排除 | 中boss 收尾，已由 `IsMidBossEndSub` 覆盖；**中boss 击破 ≠ 通关**（同 th14/th16 口径）。无任何 `*mboss*.decl` 含裸 `BossDead` ⇒ **无同名歧义** |
| `MBossEscape` | `stage01:1319` `stage02:1565` `stage03:2057` `stage04:1403` `stage05mboss:546` | ❌ 排除 | 同上 |
| `MBossEscape2` | `stage04:1430`（**ST4 独有**，且 ST4 **无** `MBossDead`：`stage04:1282` 直接 `@MBossEscape()`、`:1304` `setNext(0,0,1590,"MBossEscape2")`） | ❌ 排除 | 同上（中boss 的第二条逃逸路径，**不是**关底收尾） |
| `RockBDead` / `RockMDead` | `stage01:2111` / `:2173` | ❌ 排除 | `setNext(0,0,36000,…)` 注册的**岩石杂兵**死亡动画（`:2105` / `:2167`），boss 存活期间就会跑 |
| `Girl3Dead` / `Cir01Boss2Dead` | `stage04:1224` / `stage03:1449` | ❌ 排除 | 敌人死亡动画 |
| `Boss1GirlDead` / `Boss1GirlDead2` / `BossCard4GirlDead` / `BossCard4GirlDead2` | `stage05boss:299` `:329` `:1262` `:1290` | ❌ 排除 | `setDeath()` 注册的**符卡结束 / 部件**死亡动画，boss 存活时就会跑 |

**本作与 th14–th18 的关键差异（逐名枚举的产物，不得跨作外推）**：

1. **TH11 没有独立的 `BossEscape`**。全 20 份 `.decl` 里 `Escape` 只出现在 `MBossEscape` / `MBossEscape2`（中boss）。`setTimeout` 也**只被中boss 使用**（`stage01:1016/1020/1054/1130`、`stage01:1205`、`stage02:1271`、`stage05mboss:16/180`，全部指向 `MBossEscape`）。关底 boss **没有** `setTimeout` ⇒ 死 / 逃**共用一个 `BossDead`**：其 body 首部就 `unless ($TIMEOUT == 0) goto BossDead_308`（`stage01:598`、`stage04:811` 等），两条路径都汇合到 `+60:` 标签后的 `etClearAll(); spellEnd(); … setBoss(-1); delete(); delete();`。
2. **`BossDead` 可被 60 Hz 采样抓到**：所有 7 份都停在 `+60:` 标签（≥60 帧），**不存在 th16 `st05bs` 那种 0 帧转发壳** ⇒ 不需要同窗口兜底名字。
3. **中boss 的 `MBossEscape2` 是 ST4 独有**，且 ST4 的中boss **没有** `MBossDead`（两条逃逸路径二选一）。这是**中boss 清单**（监控迭代已落地）的事实，与关底清单无关 —— 不要把它误当成关底收尾。
4. `BossDead` **不在** `docs/logger/th11_checkpoints.csv`（125 行全是 `MainSub*` / `MBoss*` / `Boss*` 战斗 sub），所以「段落入口切换复位」与「`BossDead` 同帧置位」不会互相打架（置位在复位之后执行，见 §9.5 顺序约束）。

### 9.4 (c) 符卡对象指针 + flags 偏移（口径 15 的 `spellFlag`）

**结论：本作可做，不需跳过。**

| 项 | 值 | 证据（VA / RVA = VA − 0x400000） |
| --- | --- | --- |
| 符卡对象全局指针 | **`0x4A8D6C` / `0xA8D6C`** | 1) `0x40C655 mov ebp,[0x4a8d6c]`（`spell` 声明函数 `0x40C650` 的开头）；2) 游戏自身对它做**空指针检查**：`0x40C481 cmp dword [0x4a8d6c],0 / je 0x40C48F`；3) 3 处赋值：`0x40BF64 mov [0x4a8d6c],ebp`、`0x40BCF4`、`0x40C006 mov [0x4a8d6c],esi` |
| 对象分配 | **`0x40BF90`**：`push 0x924` → `call 0x45FCE4`（operator new）→ `push 0x924; push 0; push esi; call 0x46A720`（`memset` 归零）→ `or dword [esi],2` → `mov [0x4a8d6c],esi`。**大小 `0x924`** | 由**关卡载入链**调用：`0x41FCEF call 0x40BF90`（`0x41FC69`/`0x41FC76`/`0x41FC7F`/`0x41FC88`/`0x41FCDD`/`0x41FCE6` 一串 init 调用，任一返回 0 即 `je 0x41FCF8` 中止）⇒ **每次进关重建** |
| **flags 偏移** | **`+0x8E0`** | 与 th14 同族但**逐作不同**：th14 `+0x7C`、th16 `+0x78`、th11 **`+0x8E0`** |
| **bit0 = 符卡存活** | `*(u32*)(obj + 0x8E0) & 1` | 见下 |
| 置位点（唯一） | **`0x40C6DE`** | `0x40C6CC mov eax,[ebp+0x8e0]` → `0x40C6D8 and eax,0xFFFFFFE7`（清 bit3/4）→ **`0x40C6DB or eax,3`**（置 **bit0 + bit1**）→ `0x40C6DE mov [ebp+0x8e0],eax`。位于 `0x40C650`（声明例程），由 opcode 处理器 **`0x415B35`** 在 `0x415BF1 call 0x40C650` 调用 |
| 清位点（唯一） | **`0x40CDA5`** | `0x40CD60`（`spellEnd`）：`0x40CD62 mov esi,[0x4a8d6c]` → `0x40CD6D test byte [esi+0x8e0],1` → `je 0x40CEAC`（**无卡 ⇒ no-op，幂等**）→ … → **`0x40CDA5 and dword [esi+0x8e0],0xFFFFFFDE`**（清 **bit0 + bit5**）。唯一调用点 `0x415C24`（opcode `0x157` = `spellEnd()`） |
| 其它字段（未使用，仅作指纹） | `+0x888` = 魔数 **`0xFFF0BDC1`**（与 th14 的 `+0x24` 同值）；`+0x89C` = **已解密的 CP932 符卡名**；`+0x8DC` = 卡时长；`+0x8E4` = 奖励值；`+0x898` bit0 = 「已初始化」 | `0x40C67A` / `0x40C6B8` / `0x40C6A2` |

**`+0x8E0` 全 `.text` 写点扫描（含所有 `mov/and/or/xor`，`dword ptr` 形式，无 byte/word 形式）**：

| 写点 | 指令 | 作用 | 是否动 bit0 |
| --- | --- | --- | --- |
| **`0x40C6DE`** | `mov [ebp+0x8e0],eax`（前 `and ~0x18; or 3`） | 声明：置 bit0+bit1 | **唯一的 0→1** |
| **`0x40CDA5`** | `and dword [esi+0x8e0],0xFFFFFFDE` | `spellEnd`：清 bit0 | **唯一的 1→0** |
| `0x4068E6` / `0x406914` / `0x406C84` | `mov [ecx+0x8e0],eax` | **失败路径**：`test al,1` 通过后 `and ~0x22`（清 bit1+bit5）或 `or 0x20`（置 bit5） | **否**（写回值来自读出的 flags） |
| `0x40C1DD` / `0x40C25F` | `or 4` / `and ~4` | 奖励倒计时位（bit2） | 否 |
| `0x40C338` / `0x40C407` / `0x40C7CC` | `or 0x40` / `and ~0x40` | 奖励计时位（bit6） | 否 |
| `0x40C790` | `and ~0x20` | 宽限位（bit5） | 否 |
| `0x4107A5` / `0x415C3A` / `0x4107B7` | `or 8` / `or 0x10` | 脚本置位（bit3 / bit4） | 否 |
| `0x40C2FF` / `0x40C7C1` | `mov [x+0x8e0],eax` | 上述 `and`/`or` 的写回 | 否 |
| `0x409E58` | `mov [ebp+0x8e0],edx` | 另一对象的状态**快照拷贝**（源为静态缓冲 `0x4C326C..0x4C3274`，`ebp != [0x4a8d6c]`） | 否（不在正常游玩路径上） |

⇒ **bit0 的置位只有声明例程、清位只有 `spellEnd`**，与 th14 的形状逐点同构（th14：置 `0x41CBE7` / 清 `0x41D492`）。

**opcode 链（定位路径，供复核）**：基本 opcode（≤ `0x58`）在 `0x45B890` 分发；**扩展 opcode 走虚拟调用** `0x45D269 mov ecx,[edi+0x1014](=H) → mov edx,[ecx](=[H]=0x494074) → mov eax,[edx](=0x412E20) → call eax`。`0x412E20 add ecx,0x103C` → `0x412E30` 内 `movzx eax,word[edi+4]`、`add ecx,0xFFFFFF00`、`cmp ecx,0xC2`、字节表 `0x416DF0`、跳表 `0x416BD4` ⇒ `0x165`(=`spell`)、`0x166`、`0x167`(=`spell3`)、`0x15C` 全部落到 **`0x415B35`**（与 th14 的「四个 opcode 共用一个对象」同形）。`spellEnd` = `0x157` → `0x415C24 call 0x40CD60`。
（离线交叉印证：`stage01.ecl` 的 `BossCard1` 里 `op=0x0165 size=0x3C args=02000000 34080000 20a10700 …` ↔ `stage01.decl:270 spell(2, 2100, 500000, "…")`；`op=0x0167` ↔ `:272 spell3(4, 2100, 500000, "…")`。）

**「一帧一采样不漏」判据**：`bit0` 的 1→0 必须至少保持到下一次 `Update()` 采样。最危险的是 **card → card 直接转移**。对全 20 份 `.decl` 枚举每个 `card` sub 的 `setNext` / `@` 目标，真实 card→card 共 **11 处**，逐一核对「目标 sub 的 `spellEnd()` 与 `spell()` 之间的帧数」：

| 文件 | 转移 | `spellEnd()` → `spell()` 之间 |
| --- | --- | --- |
| `stage02` | `BossCard2 → BossCard3` | `+90:` 标签（90 帧） |
| `stage05boss` | `BossCard3 → BossCard4` | `+60:` 标签（60 帧） |
| `stage06boss` | `BossCard4 → BossCard5` | `wait(60)` |
| `stage07boss` | `BossCard8 → BossCard9` | `wait(120)` |
| `stage07boss` | `BossCard9 → BossCard10` | `wait(180)` |
| `stage07mboss` | `MBossCard1 → MBossCard2` | `+60:` 标签（60 帧） |
| `stage07mboss` | `MBossCard2 → MBossCard3` | `+60:` 标签（60 帧） |
| `stage4c0{0a,0b,0c,1a,1b,1c}` | `BossCard2_0XY → BossCard3_0XY` | 各 `wait(90)` |
| `stage4c0{0a,0b,0c,1a,1b,1c}` | `BossCard3_0XY → BossCard4_0XY` | 各 `wait(90)` |

⇒ **最短 60 帧**（≥1 个 60 Hz 采样周期，实际 60 倍余量），**不会漏采样**。其余「非符 → 符卡」的转移（`BossCard1 → Boss2 → BossCard2`、`BossCard3 → Boss4`…）里，**下一个攻击 sub 的首条指令就是 `spellEnd()`**（例如 `stage01.decl:170` 的 `Boss2` 首部），bit0 在整个非符期（1500–8400 帧）保持 0。

**已知缺口 / 兜底（与 th14/th16 同类，登记于此）**：

| # | 洞 | 影响 | 兜底 |
| --- | --- | --- | --- |
| 1 | 关底最后一张卡死亡 → `BossDead` 的 `+60:` 才 `spellEnd()` | 60 帧内无下降沿 | ✅ `mStageClear` 在 `BossDead` **第一帧**就置位 |
| 2 | 过渡非符（`Boss2`/`Boss3`/…/`Boss10`）不在 CSV | 卡结束后到下一个白名单 checkpoint 之间计数滞留 ⇒ 该窗口退出**假阴性**（少记 `interrupt`） | ❌ 不够；与 `knownBugs.md` #12 同类副作用，接受 |
| 3 | ST4 中boss（`MBossEscape` / `MBossEscape2`）与关底共用同一对象 | 中boss 卡结束也会 `++` | 中boss 不是 stage clear ⇒ `mStageClear` 不覆盖；`--` 由下一个 checkpoint 消费 |
| 4 | 对象**每次进关重建**（`0x40BF90`） | 若 `malloc` 复用同一地址，轮询可能看不到「指针变空」的中间态 | `obj != mSpellObj` 分支 + 换关复位 + `OnStageStart()` 复位三重吸收 |

**未做**：本机无编译环境，**未编译、未真机**（🟡）；逐项验证清单见 §9.6。

### 9.5 顺序约束（三条都不能反，与 th14 §5.4 同）

1. `SampleSpellCard()` 在 `interrupt` 判定**之前**（`Update()` 顶部）⇒ 同帧「清卡 + 退出」也能吃到 `++`。
2. `++` 在 `Update()` 顶部、`--` 在**最终**段落裁决之后 ⇒ 同帧「卡结束 + 新 checkpoint」净 0；反过来会残留 1，吞掉之后整关的 `interrupt`。
3. 换关复位在采样**之后** ⇒ 换关 / 对象销毁的假下降沿当场抹掉；**对象为空（菜单 / 销毁中）只清 `mSpellObj`/`mSpellActive`、不清 `mSpellFlag`**，否则退出帧还没用上就被清了。

`prevSection` 必须取在**中boss 窗口裁决之前**（`:1206`），因为「中boss 结束 ⇒ `mSection = mPending`」的晋升也会改 `mSection`，那次也应算一次段落切换（与 th14 `:1080` 同位置）。

### 9.6 bomb 双钩子裁决与 shot 字段

**bomb 双钩子 ⇒ 两处都接 `OnBomb()`。**

| 项 | 结论 / 证据 |
| --- | --- |
| 两个钩子的地址 | `th11_bomb_dec` `0x4311E6`（注释 "bomb dec"）与 `th11_bomb_dec2` `0x431293`（注释 "dead bomb"）**都 hook 同一个被调函数 `call 0x406510`**（`E8 25 53 FD FF` / `E8 78 52 FD FF`，rel32 均指向 `0x406510`），只是**两个不同的调用点** |
| 两个调用点的语义 | `0x4311E6` 在「正常 bomb」分支（`0x4311DD test byte [0x4C93C0],2` = 输入位 bit2 = **B 键**，且玩家对象存活）；`0x431293` 在**死亡路径**分支（前驱是 `0x431236 call 0x4327D0`（与 `life_dec 0x4327F0` 同族）、`0x431286 call 0x406100` 起 60 帧计时器 `[esi+0x944]`、调用后 `0x4312AA mov dword [esi+0x928],1` 置「已死」标记）⇒ 即 **death bomb（死bomb）** |
| **不会双记** | 两个调用点**共享同一个 latch**：`0x406510` 开头 `mov esi,[0x4a8d64]; cmp [esi+0x3c],0; je 0x406527; or eax,-1; ret` —— 首次调用即 `0x40652E mov [esi+0x3c],1`，第二次调用（同一次 bomb 事件内）直接返回 `-1`。两个调用点自身也各自预检 `cmp [eax+0x3c],0; jne <skip>`（`0x4311C7` / `0x43125C`）⇒ **同一次 bomb 至多一处生效** |
| **裁决：两处都接** | 理由 = **与 IGI 面板对齐**：`th11_bomb_dec` 与 `th11_bomb_dec2` 两处**都在递增 `TH11InGameInfo::mBombCount`**，即 IGI 面板把 death bomb 也计为 bomb；日志的 `events` 若只接主钩子，条数会**少于** IGI 面板（`logger_impl_report.md` §7 验证项 6 要求「对照 IGI 面板的 miss/bomb 计数与 `events` 条数」）。此裁决与 th18 的先例一致（`logger_impl_report.md` §4.1：「th18 bomb 有两个递增点，两处都挂了 `OnBomb()`，否则 JSON 条数会少于 IGI 面板」）。**latch 证据排除了「同一次 bomb 双记」这一唯一风险** |

**shot 字段 ⇒ 接本作 IGI 的机体串来源。**

```cpp
mSession.Tick(inGame, (inGame && mSession.NeedsStartInfo())
        ? S(IGI_PL_11[globals->chara * 3 + globals->subshot])
        : nullptr);
```

- 表达式逐字取自本作 IGI 的 `OnContentUpdate()`（落地后 `:1299`-`:1301`：`byte cur_player_type = globals->chara * 3 + globals->subshot;` … `S(IGI_PL_11[cur_player_type])`），只是去掉 `diff_pl` 的难度前缀（th14 的 `shot` 也只取机体串）。
- `globals` 是**本文件既有的**全局指针 `Globals* globals = (Globals*)0x4a56e0;`（`:52`），照既有取法直接用。
- 读取被 `inGame && mSession.NeedsStartInfo()` 守卫（与 th14 同），菜单里不读机体地址。
- `IGI_PL_11` 有 7 项，`chara ∈ {0,1} × subshot ∈ {0,1,2}` ⇒ 索引 0–5，不越界。
- 与 th18 不同（th18 无来源、传 `nullptr`），本作**有**现成来源，故接上。

### 9.7 未真机声明（🟡）与验证项

> 本机**未运行 th11**、**未编译**（由主代理统一构建）。下列全部未做，状态一律 🟡（AGENTS.md §2.5）。

1. **一局边界**：练习启动一局 → 打几段 → 退到标题：`<数据目录>logger_sessions_YYYYMM.jsonl` 出现一行，`game = "th11"`，`shot` 与 IGI 面板 `diff_pl` 行的机体串一致。
2. **in-game 判据**：确认 `*(u32*)0x4A8EB4` 在标题 / 菜单为 0、进关后非 0（IGI 面板显隐已长期证明，此处只作复核）。
3. **`interrupt`**：练习 warp 到道中段 → 不进 boss → 退到标题 ⇒ `events` 含 `{"type":"interrupt","checkpoint":"<退出时段落>"}`。
4. **击破关底不算 interrupt**：warp 到关底 → 击破 → 等结算 → 退到标题 ⇒ **无** `interrupt`（验 `BossDead` + `mStageClear`）。
5. **关底超时不算 interrupt**：同上但拖到超时 ⇒ 同样**无** `interrupt`（TH11 的死 / 逃共用 `BossDead`，此条同时验「没有独立 `BossEscape`」）。
6. **中boss 不算通关**：warp 到 `ST1 MID1` → 击破中boss → 退出 ⇒ **有** `interrupt`（`MBossDead` 不置位）。
7. **ST4 中boss 的两条逃逸路径**：`ST4 MID1/MID2` 拖到超时（`MBossEscape` / `MBossEscape2`）→ 退出 ⇒ **有** `interrupt`（都不是 stage clear）。
8. **`spellFlag` 抑制**：warp 到关底某张符卡 → 击破该卡 → **在下一个 checkpoint 切过来之前**立刻退到标题 ⇒ **不记** `interrupt`（验 `mSpellFlag > 0` 分支）；等 checkpoint 切过来后再退 ⇒ 记 `interrupt`。
9. **`spellFlag` 计数不漏**：`ST7` 的 `BossCard8 → BossCard9 → BossCard10` 连打三张卡，逐卡击破并观察 `mSection` 是否逐段推进（若有卡结束被漏计，会出现「卡结束后退到标题却记了 `interrupt`」）。
10. **bomb 双钩子**：练习局触发 ≥1 次普通 bomb + ≥1 次 death bomb ⇒ `"type":"bomb"` 条数 == IGI 面板 bomb 计数（验「不双记、不漏记」）。
11. **序列化自检**：确认 JSON 里是 `"interrupt"` 而非 `"?"`。
12. **单章节合并**：同起始 checkpoint 连续重开 5 次 ⇒ 月度文件只有 1 行、`total == 5`（口径 11/13/16；共享层逻辑，本作只提供字段）。

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **日志接线（JSONL 落盘）**。`thprac_th11.cpp` 本迭代 **+161 / −0 行**（纯插入；含监控迭代的未提交改动则 `git diff HEAD` = +630 / −0），19 处接线见 §9.1：`SPELL_OBJ_PTR` / `kSpellFlagsOff` / `IsStageClearSub` / `mSpellFlag`+`mSpellObj`+`mSpellActive`+`mStageClear`+`mSession` / `OnStageStart`+`OnMiss`+`OnBomb`+`SampleSpellCard` / `Update()` 顶（`inGame`+采样+`interrupt`+`Tick`）与尾（`--mSpellFlag`+`mStageClear`+`Latch`）/ 换关复位 / 两处 bomb 钩子 + 一处 life 钩子（**均在既有 lambda 体内，零新 hook**）。**三项本作自定结论**：(a) in-game 判据 = `*(u32*)0x4A8EB4`（本文件 `player` 宏，即 IGI 显隐判据，§9.2）；(b) `IsStageClearSub` = **仅 `BossDead`**（逐名枚举 20 份 `.decl`；**TH11 无独立 `BossEscape`**，死 / 逃共用 `BossDead` 的 `$TIMEOUT` 分支；`MBossEscape2` 是 ST4 中boss 的第二逃逸路径，不收录，§9.3）；(c) 符卡对象 **`0x4A8D6C`**、flags **`+0x8E0`** bit0（置 `0x40C6DE`、清 `0x40CDA5`、对象 `0x924` 字节由 `0x40BF90` 每次进关重建，§9.4）。**bomb 双钩子两处都接**（共享 `[0x4A8D64]+0x3C` latch ⇒ 同一次 bomb 至多一处生效；与 IGI 面板计数对齐，同 th18 先例，§9.6）；**shot** 取 `S(IGI_PL_11[globals->chara*3+globals->subshot])`（§9.6）。**未碰** `.vcxproj` / `.filters` / `thprac_games.{h,cpp}` / `thprac_locale_def.*` / 其它作 / `wind`；共享层零改动。**未编译、未真机（🟡）**，验证项见 §9.7。 |
| 2026-10-20 | **建档（TH11 监控 E5 实现）**。产出：① `docs/logger/tools/gen_th11_checkpoints.py`（复制 th14 版改 5 处，**已运行并通过逐字节自校验**）；② 生成物 `thprac/src/thprac/thprac_th11_checkpoints_gen.cpp`（`THPrac::TH11::kTh11CheckpointsCsv`，3882 B）；③ `thprac_th11.cpp` **+469 / −0 行**：头文件 include、`extern` 声明、`TH11Logger` 模块（`:643-1096`）+ 4 处调用（`:1157-1160` 显示行 / `:2793` 每帧 `Update` / `:2830` `OnStageStart` / `:2879` 预构造）；④ 本报告。**取法**：遍历 `mgr+0x68` 宿主链 × `H+0x1030` 线程链 → `ip = *(u32*)(T+4)` → `table[i].+4+0x10` 地址升序区间反查 → 白名单 latch（未命中 no-op、不回退 `main`）。**双匹配骨架**（`mOffsetRows` 本作恒空，为 th10–th13 结构一致而保留）。**中boss 窗口**移植 th16 §5.7（含 `mMidBossDone`——th11 白名单收录了 `MBoss` 容器，故必需）；死亡 / 逃逸 sub 名**逐名精确枚举** = `MBossDead` / `MBossEscape` / `MBossEscape2`（`stage04` 独有第三个，且 ST4 无 `MBossDead`）。**未做**日志落盘 / `GameSession` / miss / bomb / `interrupt` / `stageClear` / `spellFlag`（下一迭代）；**未碰** `.vcxproj` / `.filters` / `thprac_locale_def.*` / 其它作。登记四处已知存疑（§4.2）与**新发现**的章节 sub 复用回退（§4.3）。**未编译、未真机（🟡）**。 |

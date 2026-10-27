# TH16 ECL 段落监控 — 实施记录（beta）

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志；本迭代只做**监控**，不做落盘）
> 上游逆向结论：`docs/th16/th16_ecl_re_report.md`（Q1–Q9，指针链 / hook 点）、`docs/th16/th16_boss_phase_report.md`（boss 段方案一/方案二）
> 术语口径（以此为准）：`docs/logger/logger_re_disasm_overview.md`
> 白名单数据：`docs/logger/th16_checkpoints.csv`
> 二进制指纹：`../exes/th16.exe`，**683520 B**，PE `TimeDateStamp = 1501534029`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）
> 代码：`thprac/src/thprac/thprac_th16.cpp`（`TH16Logger` 模块；2026-10-16 由独立的 `thprac_th16_logger.{h,cpp}` 并入，见 §10）、`thprac/src/thprac/thprac_th16_checkpoints_gen.cpp`（生成物）
> 脚本：`docs/logger/tools/gen_th16_checkpoints.py`（嵌入生成）、`docs/logger/tools/fill_th16_boss_rows.py`（boss 行回填）
> 分支 / 提交：`lxn/game-logger`；`4dd7362`（boss 行回填）、`58745fe`（**beta** 功能）
> 状态：**真机验证通过（除完整全关外）**。§7 / §7.1 清单已逐项跑过，仅剩全关测试；功能提交带 `beta:` 前缀。
> 地址口径：正文以 **VA** 为主。**本机为 Windows**（VS 在 `D:\tools\VS`），与本文其他 `docs/th16/` 文档里「本机 macOS 不可编译」的旧描述不符——以本文为准。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 采用方案 | **方案二**（读 ECL 引擎当前执行 sub 名 + 白名单 latch），与道中同一机制，**不需要**名字→阶段映射表 |
| 采样点 | **`th16_update`（`0x40156f`）每帧轮询**，不挂 `0x473995`（后者非无条件每帧，且看不到 async/boss 宿主） |
| 取值范围 | 遍历 `mgr+0x180` **宿主链** × 每宿主的 `host+0x11FC` **线程链**，一次覆盖主线 + boss + async |
| ECL 资源取法 | **按每个宿主读 `host+0x11F8`**（⚠ 不是全局 `mgr+0x17C`，见 §4） |
| 指针生命周期 | **不缓存任何宿主/线程/表指针**，每帧从 `0x4A6DC0` 重走链——结构性消除 th15 的野指针崩溃根因 |
| 白名单进二进制 | 生成脚本 → raw string 嵌入 `.cpp`（生成物入库，不加 MSBuild 预构建步骤），运行期 rapidcsv 解析 |
| 显示 | IGI 面板 miss/bomb 行下方新增一行；标签硬编码中文 `"当前段落"`（未动 `thprac_locale_def.*`） |
| 开关 | 不新增高级选项，跟着 IGI（F9）走 |
| 代码量 | 功能本身：`thprac_th16.cpp` 内的 `TH16Logger` 模块 + 4 处调用；独立新文件只有生成物 1 个；其余为管道/脚手架 |

---

## 1. 开发流程总览

```text
需求 (docs/req_logger.md)
  │
  ├─ ① 逆向调研 ──────────────► th16_ecl_re_report.md（指针链 Q1–Q9 定案）
  │                              th16_boss_phase_report.md（boss 段两方案）
  │
  ├─ ② 方案选型 ──────────────► 选方案二（读 sub 名 + latch），否决方案一（名字槽 + 映射表）
  │
  ├─ ③ 数据准备 ──────────────► 回填 th16_checkpoints.csv 的 boss 行（72 → 71 行）
  │                              脚本：fill_th16_boss_rows.py   提交：4dd7362
  │
  ├─ ④ 方案设计 ──────────────► 计划文件（宿主链遍历 / 白名单嵌入 / 生命周期）
  │                              过程中反汇编复核，订正「res 取法」一处（§4）
  │
  ├─ ⑤ 编码 ─────────────────► `TH16Logger` 模块并入 thprac_th16.cpp + 生成物 + 工程登记
  │                              脚本：gen_th16_checkpoints.py → 生成物
  │
  ├─ ⑥ 构建 ─────────────────► MSBuild Release|x86 通过
  │                              途中修：生成物缺命名空间（LNK2001）、输出 exe 被占用（LNK1104）
  │
  └─ ⑦ 提交（beta）──────────► 58745fe，标 `beta:` 表示未真机验证
       │
       └─ ⑧ 真机三态验证（§7 / §7.1）──► ✅ 除完整全关外全部通过
```

### 1.1 每一步的产出与判定

| 步 | 输入 | 产出 | 关键判定 |
| --- | --- | --- | --- |
| ① 逆向 | `../exes/th16.exe` + 离线 `.decl`/`.ecl` | 两份报告 | 指针链静态定案；boss 段结论标 🟡 |
| ② 选型 | 两份报告 | 决策：方案二 | 与道中同机制、不需映射表、与 CSV 直接对齐 |
| ③ 数据 | `../exes/ecl_dump/th16/*.decl` + `thprac_th16.cpp` 的 `THPatch` | CSV 第 1 列 71 行 | 三重证据交叉验证；定不了的留空不猜 |
| ④ 设计 | ①②③ | 实施计划 | 反汇编复核推翻了一处取法（§4） |
| ⑤ 编码 | ④ | 代码 + 生成物 | 逐处对照计划，无计划外改动 |
| ⑥ 构建 | ⑤ | `Release\thprac.exe` | 见 §6 的两个途中问题 |
| ⑦ 提交 | ⑥ | 2 个 commit | 数据与功能分开提交，功能带 `beta:` |

---

## 2. 方案选型：为什么是方案二

`docs/th16/th16_boss_phase_report.md` 给出两条路线，本质是同一个 boss 对象 `B` 的两个字段入口：

| 维度 | 方案一：读 boss「下一阶段名」+ 映射 | **方案二：读当前 sub 名 + latch（采用）** |
| --- | --- | --- |
| 数据来源 | `B+0x4080+s*0x88` 等名字槽副本 | `host → thread → res → sub 表` 的当前 sub 名 |
| 语义 | 「**下一**阶段名」，需换算成当前 | 「**当前** sub 名」，直接对白名单 |
| 是否需要映射表 | 需要（且 th16 未填） | **不需要**，CSV 即白名单 |
| 与道中一致性 | 两套机制 | **同一机制** |
| 主要风险 | `B` 野指针（生命周期）；映射表完备性 | boss 宿主取法（🟡） |

**选方案二的决定性理由**：与道中同一套读取 + latch 逻辑 ⇒ 一份代码覆盖章节段与 boss 段，且直接消费已回填的 `th16_checkpoints.csv`；方案一还要额外维护一张未填的映射表，并重新引入 `B` 的生命周期问题（th15 分支正是因此崩溃）。

---

## 3. 数据准备：回填 boss 行

`docs/logger/th16_checkpoints.csv` 共 **111 行**（1 表头 + **110 数据行** = 章节 39 + boss 71）。列：`段落入口函数,所属游戏,所属关卡,记录名`。

- **boss 行第 1 列由脚本回填**（`docs/logger/tools/fill_th16_boss_rows.py`），三重证据交叉验证：
  1. `.decl` 的**阶段链**（`setNext(0,…)` / `@call`），并用 `<Name>_at` 同伴判定「过渡非符」；
  2. `thprac_th16.cpp` 的 **`THPatch`**（`SetFile(N)` + `ECLJump` 目标偏移）落点；
  3. `.ecl` 原始二进制的 sub 字节区间。
- **过渡非符剔除**：名字规范、非符卡、且无 `<Name>_at` 同伴者（ST2 `Boss3`、ST6 `Boss6`、ST7 `Boss9`/`Boss10`、ST5 `Boss4` 等）——它们只是「进入符卡前的过场」，不该占一行 checkpoint。
- **已知例外**（脚本头部有记）：
  - `ST5 BOSS7 = BossCard4`：其补丁落点全在过渡非符 `Boss4`/`Boss4B` 上，改按 `Set Spell Ordinal = 0x34` + `Set Health = 3000` 定位（与 `Boss()` 派发的 `$SPELL_ID 66..69 → lifeSet(3000); @BossCard4()` 对上）。
  - `ST7 END_S9 = BossCard9`：同理，补丁落点是 `Boss9`（过渡非符）。
  - `ST5 BOSS1 = Boss1`：该 case 只改入口站位，无更硬判据，按「关底第一个非符」兜底。
- **`ST4 MID2` 已删**（72 → 71）：该行对应的 `THPatch` 只改章节脚本 `st04.ecl` 的 `MainLatter`，道中 Boss 脚本 `st04mbs` 只有 1 个阶段，游戏内也无此段。

> ⚠ **本迭代只用 CSV 的前 3 列**做匹配（`段落入口函数` + `所属游戏` + `所属关卡`）；第 4 列 `记录名` 用于显示，日志迭代才会进一步消费。

---

## 4. 关键订正：`res` 必须按宿主取

**设计阶段反汇编复核推翻了一处取法**，这是本实现里最容易踩的坑：

```text
✗ 错：res 统一从全局 *(u32*)0x4A6DC0 + 0x17C 取一次，所有宿主共用一张 sub 表
✓ 对：每个宿主各自读 *(u32*)(host + 0x11F8)
```

证据（th16.exe 反汇编）：

| 事实 | 地址 |
| --- | --- |
| 宿主构造时把 `[ecx+0x17C]` 存进 `host+0x11F8` | `0x41B897: mov ecx,[ecx+0x17C]` → `0x41B89D: mov [edi+0x11F8],ecx` |
| `mgr+0x17C` 另有 3 个写点（装载新 ECL / 清零重置） | `0x41AED0`、`0x41B2D1`、`0x429F61` |

⇒ 每宿主的 `res` 只是**构造那一刻**对 `mgr+0x17C` 的快照，两者**不恒等**。boss 段加载的是另一份 ECL（`stNNbs`），若读全局 `mgr+0x17C`，boss 线程的 `subIdx` 会索引到**主线那张 sub 表** → 读出垃圾名字。

按 `host+0x11F8` 取 res 与 `th16_ecl_re_report.md` §1 / `th16_boss_phase_report.md` §5.5 以及 th15 参照实现一致。代码见 `TH16Logger::Update()`（`thprac_th16.cpp:188-314`）。

---

## 5. 代码结构

### 5.1 指针链（th16）

```text
mgr    = *(u32*)0x4A6DC0                 // ECL 管理器
node   = *(u32*)(mgr + 0x180)            // 宿主链头；node[0] = host，node+4 = next
host   = *(u32*)node
tn     = host + 0x11FC                   // 线程链头节点（嵌在宿主内）
thread = *(u32*)tn                       // tn[0] = thread，tn+4 = next
subIdx = *(i32*)(thread + 4)             // -1 = 线程结束
res    = *(u32*)(host + 0x11F8)          // ★ 每宿主自己的 ECL 资源
count  = *(i32*)(res + 8)                // sub 总数（越界保护）
table  = *(u32*)(res + 0x8C)             // sub 表，项 8B
name   = *(char**)(table + subIdx*8)     // 项 +0 = 名字 char*
```

- **厂商同构佐证**：`FUN_00417580`（`0x417580`）的宿主链遍历与上表逐指令一致（`mov esi,[0x4a6dc0]` → `[esi+0x180]` → `mov ecx,[eax]` / `cmp [ecx+0x5740],edx` / `mov eax,[eax+4]`）；线程链遍历见 `FUN_004744E0`（`0x4744F0`）。
- **线程链头节点的 `[0]` 就是主线线程**：`0x41A5A7 lea eax,[ecx+0x10]` → `0x41A63F mov [ecx+0x11fc],eax`。遍历线程链 = 主线 + async 全覆盖，且不依赖 `host+0xC`（后者在更新循环结束时被重置，帧边界语义不稳定）。

### 5.2 文件落点

| 文件 | 内容 |
| --- | --- |
| `thprac/src/thprac/thprac_th16.cpp` | 单例 `TH16Logger`：`OnStageStart()` / `Update()` / `CurrentSection()`；`mEntries`（110 行）+ `mStageMap`（本关 `sub名 → 记录名`）；常量、`LoadCsv()`、`BuildStageMap()`、`Update()` 的链遍历。落在 `enum addrs` 之后、`class TH16InGameInfo` 之前（`:24-314`）。<br>2026-10-16 前这里是独立文件 `thprac_th16_logger.{h,cpp}`；拆分无封装收益（唯一使用方就是本文件），故并入，并把重复的 `kStageNum` / `kEclMgrPtr` 改为直接复用 `enum addrs` 的 `STAGE_NUM` / `ENEMY_MANAGER_PTR`。 |
| `thprac/src/thprac/thprac_th16_checkpoints_gen.cpp` | **生成物**：`THPrac::TH16::kTh16CheckpointsCsv[]`，raw string 内嵌 CSV |

### 5.3 采样与安全校验（`TH16Logger::Update()`，`thprac_th16.cpp:188-314`）

- `mgr` / `res` / `table` 逐级判空（菜单态、ECL 未加载时为 0）；
- `count ∈ (0, 8192]` —— 这是挡住「`res` 被破坏」的**唯一哨兵**；
- `subIdx ∈ [0, count)` —— `-1`（线程结束）被负数挡掉；
- 遍历有硬上限（宿主 ≤ 1024、线程 ≤ 64）防链成环；
- **不缓存任何指针** ⇒ 不存在「指针失效清零」问题，所谓重置退化为「关卡切换时 `mSection ← "main"`」。

### 5.4 白名单加载

- `LoadCsv()`：`std::istringstream(kTh16CheckpointsCsv)` + `rapidcsv::Document(ss, LabelParams(0, -1))`；跳过列数 < 4 的行、`第2列 != "th16"` 的行；解析失败 → 空表（恒显示 `main`，不抛到游戏线程外）。只在单例首次构造时跑一次。
- `BuildStageMap()`：按 `ST%d` 过滤本关；键值指向 `mEntries` 内 `std::string::c_str()`（`mEntries` 冻结 ⇒ 指针终身有效，查表零分配）。
- **过滤必须做**：`MainSub00` / `Boss1` / `BossCard1` 等名字在 ST1..ST7 中大量重复，只按名字匹配会串关。
- **过滤时机 = 每关一次**（`STAGE_NUM` 变化时重建），不是每帧。

### 5.5 显示

`TH16InGameInfo::OnContentUpdate()`（`thprac_th16.cpp:892` 起）在 bomb 行之后插入：

```cpp
ImGui::Text("当前段落");                                          // :913
ImGui::NextColumn();
ImGui::Text("%s", TH16Logger::singleton().CurrentSection());      // :915
ImGui::NextColumn();
```

保持既有 `Columns(2)` 的「标签 / 值」成对结构，未新增列、未改 `Columns` 参数。

### 5.6 生命周期

| 事件 | 处理 |
| --- | --- |
| 一局/一关开始 | `th16_game_start`（`0x42E5AE`）钩子内调 `OnStageStart()` → `mSection = "main"`，并清中boss 窗口状态（**零堆分配**，VEH 回调内安全） |
| 关卡切换 / 练习 warp | `Update()` 内 `STAGE_NUM` 变化 → `BuildStageMap(new)` + `mSection = "main"` + 清中boss 窗口状态 |
| 退出一局（`PLAYER_PTR == 0`） | **故意不重置**（需求要求保留最后 checkpoint；IGI 此时本就不绘制） |
| 菜单 / 非关卡态 | `STAGE_NUM ∉ {1..7, 9}` 直接 return，不改状态 |

> `STAGE_NUM`（`0x4A5790`）换算：`STn → n`（1..7，7 = Extra）；`9` 防御性地当 7。此换算**已真机确认**（§7.7 的临时诊断行）。

### 5.7 中boss 窗口（beta2，修「增援章节」误报）

**业务 bug**：很多关卡「后半第一节」的 checkpoint 实际是**增援章节**，它在**中boss 出场后立刻就开始跑**，于是 IGI 面板在中boss 还活着时就从 `STn MID1` 跳到 `STn 后半 #1`。

静态根因（`reasm/exes/ecl_dump/th16/`）：

- `st01.decl:1503-1543` `main()`：`@MainFront()` → `enmCreateA("MBoss", …)` → `wait(160)` → **`@MainLatter()`** → … → `@MainBoss()`。ST3 `:1554`、ST4 `:1903`、ST5 `:1597`、ST7 `:1580` 同构。
- `st01.decl:1218-1257` `MainSub03()`（= `ST1 后半 #1`）通体是 `enmCreateF("Phantom01", …)` 增援波（4 轮 ×10）。
- `st01mbs.decl`：`MBoss()` 第 8 行 `setBoss(0)`，第 33 行 `@MBoss1()`（**call** ⇒ `thread+4` 变成非白名单的 `MBoss1`），第 38 行死循环 ⇒ 中boss 战期间**不再有 `MBoss` 白名单命中**，主线程的 `MainSub03` 就赢了。7 份 `stNNmbs` 全部同构。

**机制**：新增 `mPending` / `mMidBossActive` / `mMidBossDone` 三个状态，`Update()` 由「最后一个命中者胜」改为「先遍历收集、再统一裁决」：

| 项 | 口径 |
| --- | --- |
| 命中分类 | 按**白名单键前缀**：`MBoss*` = 中boss、`Boss*` = 关底boss、其余（`MainSub*`）= 章节。不改 CSV |
| 中boss 战结束 | 当前 sub 名命中**死亡 sub 名前缀**：`MBossDead` / `MBossEscape` / `MBossBDead` / `MBossBEscape` / `BossDeadB`（来源 `.decl`，每份中boss 脚本恰有 2 处 `setBoss(-1)`）。**必须精确前缀**——`st07mbs:195/332` 的 `MBossCard1_Dead` / `MBossCard2_Dead` 是 `setDeath()` 注册的符卡结束动画，中boss 活着时就会出现 |
| 裁决 | `bossHit` → 关窗并清 pending；否则 `midHit && !mMidBossDone` → 开窗、显示中boss 记录；否则 `chapHit` → 窗口内**只存 `mPending` 不显示**，窗口外正常显示 |
| 关窗 | 死亡 sub 名一出现即关窗并**把 `mPending` 补显示出来**（取最后一个，与「值只单调前进」一致）；`mPending` 为空则维持中boss 记录 |
| `mMidBossDone` 的作用 | `st01mbs.decl:34-38` 的 `MBoss()` 在 `@MBoss1()` 返回后会 `goto MBoss_780: wait(1000)` 自循环，当前 sub 可能**变回白名单里的 `MBoss`**；没有这个标志，窗口会被迟到命中重开且永不关闭，吞掉本关之后所有章节 checkpoint |
| 采样点 | `th16_update`(`0x40156F`) 是每帧派发器 `0x401460` 的 `ret`（**在 ECL tick 之后**），同帧内「主线程进 `MainSub03`」与「`MBoss` 命中」都可见，靠裁决优先级处理，不需要额外的时序守卫 |
| 新增钩子 | **无**。只复用现有 walk 已读到的 `name` 指针；`IsMidBossEndSub()` 只做 `strncmp` |

已知残余：ST7 是双中boss（`st07mbs` + `st07mbs2`，后者 `setBoss(1)`），一个 twin 死、另一个还在场时会提前关窗——`st07mbs2`（B 路线）本就不在白名单，属 §9 已记的既有覆盖缺口。

---

## 6. 改动清单与构建

### 6.1 功能本身

| 文件 | 改动 |
| --- | --- |
| `thprac/src/thprac/thprac_th16.cpp` | `TH16Logger` 模块全部实现（`:24-314`）；`:913-915` 显示行；`:3017` 每帧采样；`:3055` 重置；`:3104` 预初始化 |

> ⚠ `th16_game_start` 的重置**必须写在既有 lambda 体内**：hook 框架对带回调的钩子用 INT3 + `vehHooks[addr] = this`（`thprac_hook.cpp:223`），**同址挂第二个钩子会覆盖第一个**，导致原有计数清零失效。

### 6.2 管道 / 脚手架

| 文件 | 改动 |
| --- | --- |
| `docs/logger/tools/gen_th16_checkpoints.py`（新建） | 从 CSV 生成嵌入 `.cpp`；幂等；断言正文不含分隔符 `)TH16CSV"`；自校验逐字节一致 |
| `thprac/src/thprac/thprac_th16_checkpoints_gen.cpp`（生成物入库） | 见 §5.2 |
| `thprac/thprac.vcxproj` / `.filters` | 登记 1 个新文件（`thprac_th16_checkpoints_gen.cpp`，+1 / +3 行） |

### 6.3 构建

```bash
"D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe" thprac.sln -p:Configuration=Release -p:Platform=x86 -m
```

产物：`Release\thprac.exe`（+ `thprac_compatible.exe`）。**结果：通过**（仅有既有的 `zlibstat` PDB `LNK4099` 警告，与本次无关）。

**途中修的两个问题**（供后人避坑）：

1. **`LNK1104: 无法打开文件 Release\thprac.exe`** —— 输出文件被一个正在运行的 `thprac.exe` 占用。构建前先关掉它。
2. **`LNK2001: 无法解析的外部符号 kTh16CheckpointsCsv`** —— 生成物最初把符号定义在**全局作用域**，而头文件声明在 `THPrac::TH16`。生成脚本已修为输出 `namespace THPrac { namespace TH16 { … } }`。

> **CRLF 说明**：仓库 `core.autocrlf=true`，生成物的包装行是 LF、raw string 正文是 CRLF（= CSV 原字节）。本机 checkout 后内嵌内容与 CSV 逐字节一致；在 `autocrlf=false` 的机器上正文会变 LF——**功能无影响**（rapidcsv 的 `mHasCR` 会剥 `\r`，无 `\r` 也无所谓），仅「逐字节相等」这一性质不成立。

---

## 7. 真机验证清单（已通过，除完整全关）

**已真机验证通过（除完整全关外）**，下列各项均已实测通过（本机 Windows 可编译可跑）：

1. **latch 正确性（道中）**：进 ST1 从头打，F9 开 IGI。title 卡时应为 `main`；随进度依次 `ST1 前半 #1 → 前半 #2 → 后半 #1 → 后半 #2`。
2. **未命中 no-op（关键反例）**：`ST1 前半 #1` 置值后，段内非白名单 sub（`MainSub01`/`MainSub02`）运行期间值**必须不变**，不得回退 `main`；`MainLatter` 期间停在 `ST1 后半 #2`。判定：整段值只单调前进或不动，**任何回退都是 bug**。
3. **boss 段（最大 🟡）**：打 ST1 中 boss → `ST1 MID1`；关底 boss → `ST1 BOSS1..BOSS4`，与符卡 / non-spell 切换同步。**若中 boss 出场后值不动** ⇒ boss 宿主取法失败，切备选（§8）。再打 ST2 复核。
4. **关卡过滤**：打 ST2 不能出现 ST1 的记录名；打 ST5 验 A/B 分支（`BOSS2A` = `BossCard1` / `BOSS2B` = `BossCard1B`）。
5. **练习 warp**：warp 到 `ST1 BOSS3` 应变 `ST1 BOSS3`；warp 到段中途应保持 `main` 或上一段。
6. **生命周期回归**（对 th15 的坑）：依次跑「ST1 打完进 ST2 / Restart / 自杀 Continue / 退回标题再进 / 直接退出」，观察无崩溃且值正确重置。
7. **`STAGE_NUM` 换算**：在 `OnContentUpdate` 里**临时**加 `ImGui::Text("DBG stage=%d", *(int32_t*)0x4A5790);`，把 ST1..ST6 / Extra / 练习 warp / 菜单各记一次值，确认后**删除**。
8. **CSV 嵌入未漂移**：临时显示 `mEntries.size()`，应为 110。（注意 `log_print` 是空实现、`log_init` 全仓库无调用，不能靠它排障。）

### 7.1 中boss 窗口（beta2）追加清单

**已真机验证通过**（含两个 flush 分支与卡死反例）。临时诊断行加在 `thprac_th16.cpp:913` 之前，验完**删除**：

```cpp
uintptr_t dbgMgr = GetMemContent(0x4A6DC0);
ImGui::Text("DBG mgr=%X slot0=%X stage=%d",
    dbgMgr, dbgMgr ? *(int32_t*)(dbgMgr + 0x48) : 0, *(int32_t*)0x4A5790);
```

1. **ST1 正例（目标场景）**：从头打 ST1，F9 开 IGI。前半 `#1 → #2`；中boss 出场显示 `ST1 MID1`；**关键**：中boss 存活期间即便主线程已进 `MainSub03`，显示**必须保持 `ST1 MID1`**；中boss 战结束的 sub 一出现才变 `ST1 后半 #1`。
2. **慢杀 flush**：中boss 存活 >20s（主线程已越过 `MainSub05`）后击破 → 显示应从 `ST1 MID1` 直接变 `ST1 后半 #2`（缓存取最后一个）。
3. **快杀 flush**：中boss 5s 内击破（`MainSub03` 尚未命中任何 checkpoint）→ 关窗时无值可补，显示维持 `ST1 MID1`，直到 `MainSub03` 命中后变 `ST1 后半 #1`。**2 与 3 是两个分支，都要跑。**
4. **ST4 / ST5 关键反例**（`mainLatterIDs = {0,0,0,92,122,0,0}` 证明 MainLatter 与 boss 重叠的正是这两关）：同样验证 MID1 期间不被后半覆盖；ST5 顺带验 `BOSS2A/BOSS2B` 分支。
5. **负例 A（warp 不得被吞）**：练习 warp 到 `ST1 后半 #1` → 应显示 `ST1 后半 #1`（窗口不得打开）；warp 到 `ST1 BOSS3` → `ST1 BOSS3`。
6. **负例 B（卡死反例，最关键）**：打 ST1 中boss，战斗中暂停 → 练习菜单重开到 `ST1 前半 #1` → 之后 `前半 #1 → 后半 #1 → 后半 #2` 必须继续推进，**不得永远停在 `ST1 MID1`**。若出现一帧闪名（残留 `mPending` 在 warp 后首帧被 flush），兜底是 1 行：在 `thprac_th16.cpp:2866` 的 `th16_patch_main` lambda 顶部加 `TH16Logger::singleton().OnStageStart();`。
7. **单调性回归**：整关值**只前进或不动，任何回退都是 bug**；段内非白名单 sub（`MainSub01`/`MainSub02`）运行期间值不得变。
8. **关底回归**：中boss 打完后 `ST1 BOSS1..BOSS4` 与符卡 / 非符切换仍同步，整段不卡在 `MID1`。
9. **ST2 复核**：`st02.decl` 的块序可疑（`@MainLatter()` 出现在死循环之后），对「中boss 命中先于章节命中」的依赖需真机确认。
10. **ST7 复核**：双中boss（`st07mbs` + `st07mbs2`）。观察 `MID1/2/3` 与击杀后推进；若一个 twin 还活着就跳到后半，记下当时 sub 名（见 §5.7 残余）。
11. **ST6 负例**：无中boss ⇒ 章节 `#1..#3`、关底 `BOSS1..` 均正常，窗口全程不得打开。
12. **生命周期回归**：ST1→ST2 连打 / Restart / 自杀 Continue / 回标题再进 / 直接退出 → 无崩溃，每关从 `main` 起算。

---

## 8. 风险与回退

| 风险 | 判定 | 回退 / 缓解 |
| --- | --- | --- |
| boss 宿主不在 `mgr+0x180` 链上，或 `host+0x11F8` 不指向 boss 自己的 ECL | 检查 3：中 boss 出场后值不动 | 切备选：在 `0x4215B4` / `0x42160B` 抓 `EDI = B`，`host = *(B+0x44B8)`；**须每帧校验 `[B+0x4060] & 0x800000` 并在 game_start 清空**（即 th15 崩溃的老路，谨慎） |
| 链遍历崩溃（链被破坏 / 成环） | 关卡切换或大量敌机时崩 | 已有三重上限 + `subIdx` 范围检查；仍崩则把 `Update()` 挪进 `TH16InGameInfo::OnPreUpdate()` 缩小暴露面 |
| `STAGE_NUM` 换算错 | Extra 关显示 `main` 或 ST6 的名字 | 已内置 `9 → ST7`；用 §7.7 的临时诊断行实测 |
| 无关宿主撞名导致误 latch | 值跳到不相干段落 | 名字较专用，概率低；若出现，改为同时校验 `host+0x5740` 或宿主链位置 |
| 英文 locale 中文渲染为空 | 切 EN 后空白 | **已知固有局限**：`thprac_gui_locale.cpp:444-467` 的 EN glyph range 不含 CJK，而 CSV 记录名本身就是中文；非本改动引入 |

**回退粒度**：功能改动集中在「`thprac_th16.cpp` 内的 `TH16Logger` 模块 + 生成物 + 脚本 + 4 处调用 + 工程登记」，`git revert 58745fe` 即可干净回退，不影响后续 JSON 落盘迭代。**2026-10-16 模块并入 `thprac_th16.cpp` 后**，中boss 窗口（§5.7）不再能只 revert 两个独立文件——它与监控主体同处一个 TU，回退需按提交（`58745fe` / 中boss 修复提交）整笔回退。

**中boss 窗口的残余风险**：死亡 sub 名若漏了变体（如 `MMBossCard3_at2` 这类笔误），窗口不关 ⇒ 本章后半 checkpoint 被吞；`bossHit`（关底boss 命中）会强制关窗，最坏只吞「中boss→关底」这一段。§7.1.10 的诊断行能直接看到真实 sub 名，据此补 `IsMidBossEndSub` 前缀。

---

## 9. 明确不做（本迭代）

- **不做 JSON 落盘 / 日志记录**（`docs/req_logger.md` 的「日志记录功能」是下一迭代）。
- 不碰 `wind`（独立工程）；不动 `thprac_locale_def.h/.cpp`（外部 devtools 生成，本地无法重生成）。
- 不新增 `AdvancedGameOptions` 开关；不加 MSBuild 预构建步骤。
- 不顺手「优化」既有 `0x473995` 的 `mTimeLock` 钩子；不动 th15 分支的任何文件。
- 未覆盖：**A/B 分支的 B 路线变体**（ST5 的 `Boss1B` / `Boss2Bp` / `Boss3Bp` / `BossCard3B` / `BossCard4B`、ST7 的 `MBossBCard1/2/3`）——CSV 是单值列，装不下「一段 → 多入口」，需后续给白名单加多入口能力（对应 `th16_boss_phase_report.md` §8 风险 8）。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | **结构整理：`TH16Logger` 并入 `thprac_th16.cpp`**（3 文件 → 2）。动机：`.h`/`.cpp` 拆分无封装收益（唯一使用方就是 `thprac_th16.cpp` 那 4 个调用点），且与仓库「一作一 cpp、无 per-game 头」的惯例不符；拆分还逼出与 `enum addrs` 重复的 `kStageNum`/`kEclMgrPtr`。做法：模块代码落在 `enum addrs` 之后、`class TH16InGameInfo` 之前（`:24-314`），地址常量改为直接复用 `enum addrs` 的 `STAGE_NUM`/`ENEMY_MANAGER_PTR`；删除 `thprac_th16_logger.{h,cpp}`，`.vcxproj` / `.filters` 各删 2 条登记。生成物 `thprac_th16_checkpoints_gen.cpp` 保持独立 TU（AUTO-GENERATED，故是 3→2 而非 3→1）。4 个调用点与 `th16_game_start` 的「重置必须写在既有 lambda 内」约束均未动。代价：`thprac_th16.cpp` 2843 → 3142 行；本文所有 `thprac_th16.cpp:NNN` 引用整体 +299。**状态：编译通过（Release\|x86），真机回归待跑。** |
| 2026-10-16 | **beta2：修「中boss 增援章节」误报**（§5.7）。真机反馈：很多关卡「后半第一节」的 checkpoint 实为增援章节，中boss 出场后立刻开跑，面板在中boss 还活着时就从 `STn MID1` 跳到 `STn 后半 #1`。静态根因：`st01.decl:1525-1531` 的 `main()` 在 `enmCreateA("MBoss",…)` 后 `wait(160)` 就 `@MainLatter()`；`st01mbs.decl` 的 `MBoss()` 在 `@MBoss1()`（call）后整场战不再有白名单命中，于是主线程的 `MainSub03` 胜出。修法：新增 `mPending`/`mMidBossActive`/`mMidBossDone`，`Update()` 改为「先遍历收集、再统一裁决」——按白名单键前缀分「中boss/关底boss/章节」，中boss 存活期间章节 checkpoint 只缓存，当前 sub 名命中死亡 sub 前缀（`MBossDead` 等 5 个，精确前缀，避开 `MBossCard1_Dead`）时关窗并补显示。仅动 `thprac_th16_logger.{h,cpp}`，无新钩子、无新反编译、不改数据。**状态：真机验证通过（除完整全关外）**，仅剩全关测试。 |
| 2026-10-20 | **新增 `spellFlag`（口径 15，与 interrupt/stageClear 并列）**：修「`BossCard` checkpoint 在卡结束后不立刻切换 ⇒ 退出被误记 `interrupt`」。在既有 `TH16Logger::Update()` 里每帧采样符卡对象 `*(u32*)0x4A6DB0` 的 `+0x78` bit0（符卡存活）1→0 下降沿 ⇒ `++mSpellFlag`；`mSection` 换新 checkpoint ⇒ `--`（下限 0）；interrupt 条件改为 `mSpellFlag <= 0 && !mStageClear`。`mStageClear` **保留兜底**（`BossDead+60` 窗口 / 非符期 `BossEscape`）。**零新 hook、共享层零改动**；静态核对 `setNext` 链无 card→card。**本机无编译环境：未编译、未真机（🟡）**。详见 `docs/logger/logger_impl_report.md` §4.4.2、`docs/th16/th16_spell_flag_report.md` §8、`knownBugs.md` #12。 |
| 2026-10-16 | **建档**。落盘 TH16 ECL 段落监控的实施记录（beta）。开发流程：需求 → 逆向调研（`th16_ecl_re_report.md` / `th16_boss_phase_report.md`）→ 选型（采用方案二）→ 数据准备（回填 CSV 的 71 行 boss 行，`ST4 MID2` 删除，72→71）→ 设计（反汇编复核**订正 `res` 取法**：按宿主读 `host+0x11F8`，不用全局 `mgr+0x17C`）→ 编码（新增 `thprac_th16_logger.{h,cpp}` + 生成物 `thprac_th16_checkpoints_gen.cpp` + 脚本 `gen_th16_checkpoints.py`，`thprac_th16.cpp` 5 处共 +8 行，工程登记）→ 构建（Release\|x86 通过；途中修 `LNK1104` 输出占用、`LNK2001` 生成物缺命名空间）→ 提交（`4dd7362` 数据、`58745fe` 功能带 `beta:`）。核心机制：`th16_update` 每帧遍历 `mgr+0x180` 宿主链 × `host+0x11FC` 线程链，按每宿主 `host+0x11F8` 取 res，命中白名单即 latch、未命中 no-op、首命中前 `main`；不缓存任何指针。**状态：编译通过，未真机验证**（§7 清单待跑）。 |

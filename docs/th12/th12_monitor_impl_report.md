# TH12 ECL 段落监控 — 实施记录（E5）

> 上位文档（术语 / 判定口径 / 粒度，一律以此为准）：`docs/logger/logger_re_disasm_overview.md`（**§1.2 粒度 / §2 术语表 / §5.1 锚点总表 / §6 Q9 / §9.9**）
> 需求来源：`docs/req_logger.md`（ECL 段落监控 + 日志）。监控见 §1–§8；**日志接线见 §9**（2026-10-20 追加；此前 §1–§8 中「本迭代只做监控、不做落盘」的表述是**监控迭代当时**的范围声明，已被 §9 取代）。
> 本作逆向结论：`docs/th12/th12_ecl_re_report.md`（Q1–Q9；**§1 指针链 / §5.4 boss 宿主可达性 Q-A/Q-B/Q-C / §10.3 判定流程 / §10.4 白名单**）
> 本作入口表小结：`docs/th12/th12_checkpoint_entry_report.md`（**114/114**；§2.1 两个 warp API / §2.2 boss 行口径 / §2.3 `ST6 BOSS9/10` 修正 / §3.1 async 派生线程 / §3.2 全局宿主链 / §4 风险清单）
> 白名单数据：`docs/logger/th12_checkpoints.csv`（**114 数据行** = 章节 43 + boss 71）
> 参考实现（权威，真机验证过）：`docs/th16/th16_monitor_impl_report.md`（C 代；**§5.7 中boss 窗口**）
> 参考实现（**同形直接模板**）：`docs/th13/th13_monitor_plan.md`（方案）、`thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（th12/th13/th14 同为「节点 `+4` = **指令绝对地址**」语义，须做「地址 → sub」反查）
> 二进制指纹：`../exes/th12.exe`，**737280 B**，PE `TimeDateStamp = 1251344348`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）
> 代码：`thprac/src/thprac/thprac_th12.cpp`（`TH12Logger` 模块）、`thprac/src/thprac/thprac_th12_checkpoints_gen.cpp`（生成物）
> 脚本：`docs/logger/tools/gen_th12_checkpoints.py`（CSV 逐字节嵌入）
> 方法：**静态实现** —— 引用已定案的逆向结论，只读对读 `thprac_th14.cpp` / `thprac_th16.cpp` / `../exes/ecl_dump/th12/*.decl`。**未编译、未真机**（主代理统一构建）。
> 状态：**已实现（🟡 未编译、未真机）**。按 AGENTS.md §2.5，本文全部结论在真机验证前不得当作结论。
> 地址口径：正文以 **VA** 为主，`RVA = VA − 0x400000`。术语（宿主 / 线程 / sub / checkpoint / 当前段落）以总括 §2 术语表为准。
> 置信度：✅ 源码 / 离线解码直接证实 ｜ 🟡 有强证据但缺一环（显式标注）｜ ❌ 未能定案。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 采用方案 | **方案二**（读 ECL 引擎当前 sub 名 + 白名单 latch），与道中 / boss 同一机制，**不需要**名字→阶段映射表 |
| 采样点 | **`th12_update`（`0x4625FB`）既有钩子内每帧轮询**（挂在 `TH12InGameInfo::Update()` 之前）；**不新增任何 hook** |
| 取值范围 | 外层遍历 `mgr+0x68` **全局宿主链**（节点 `H+0x12C0`）× 内层遍历每宿主 `H+0x1030` **线程链** —— 一次覆盖主线 + 全部 async 线程 + boss / 中boss 宿主 |
| 「当前 sub」取法 | 线程节点 `+4` = **当前指令绝对地址**（B 代口径，**不是** sub 索引）⇒ 用 `table[i].+4 + 0x10` 建**地址升序区间表**反查 sub 名（与 th14 同法） |
| ECL 资源取法 | **按宿主读 `H+0x102C`**（Q-C：每个宿主构造时写 `= *(u32*)(mgr+0x64)`，故与全局 `mgr+0x64` 恒等） |
| 指针生命周期 | **不缓存任何宿主 / 线程 / 表指针**，每帧从 `ECL_MGR_PTR(0x4B43DC)` 重走链；`mIpMapCount`/`mIpMapTable`/`mMidBossHost` **只作变化检测 / 比较，绝不解引用** |
| 白名单进二进制 | 生成脚本 → raw string 嵌入 `.cpp`（生成物入库，不加 MSBuild 预构建步骤），运行期 `rapidcsv` 解析 |
| 中boss 窗口 | **移植 th16 §5.7 / th14 形态**（`mPending` / `mMidBossActive` / 三类命中裁决）+ **一条 th12 专有的关窗条件**（宿主消失，见 §4） |
| 显示 | IGI 面板 miss/bomb 行下方新增一行；标签硬编码中文 `"当前段落"`（未动 `thprac_locale_def.*`） |
| 开关 | 不新增高级选项，跟着 IGI（F8）走 |
| 接口 | 仅三个：`OnStageStart()` / `Update()` / `CurrentSection()`（`CurrentSection()` 永不为 null） |

---

## 1. 指针链（TH12，逐条引自本作逆向报告）

```text
mgr    = *(u32*)0x4B43DC                              // ECL 全局管理器（本迭代新增常量 ECL_MGR_PTR）
node   = *(u32*)(mgr + 0x68)                          // 全局宿主链头；节点内嵌于宿主 H+0x12C0，
                                                      //   {+0 = H, +4 = next, +8 = prev}；尾 = mgr+0x6C
host   = *(u32*)node                                  // 宿主（boss / 中boss 宿主同在这条链上）
tn     = host + 0x1030                                // 线程链头节点（嵌在宿主体内）
thread = *(u32*)tn                                    // {+0 = T, +4 = next, +8 = prev}；[+0] = 主线线程
ip     = *(u32*)(thread + 4)                          // 当前（下一条待执行）指令绝对地址；0 = 线程已结束
res    = *(u32*)(host + 0x102C)                       // ★ 每宿主的 ECL 资源（≡ *(u32*)(mgr+0x64)）
count  = *(i32*)(res + 8)                             // sub 总数（越界保护）
table  = *(u32*)(res + 0x8C)                          // sub 表，项 8B
data   = *(u32*)(table + i*8 + 4)                     // 项 +4 = sub 数据基址
start  = data + 0x10                                  // ★ sub 首指令地址（区间表用）
name   = *(char**)(table + i*8)                       // 项 +0 = 名字 char*（运行期保留）
```

**与 th14 / th16 的差异（不可互搬）**

| 项 | th14（模板） | **th12（本作）** |
| --- | --- | --- |
| 宿主链头 | `mgr+0xD0`，节点 `H+0x14D8` | **`mgr+0x68`**，节点 **`H+0x12C0`** |
| 宿主 → 线程链 | `H+0x11D0` | **`H+0x1030`** |
| 宿主 → res | `H+0x11CC` | **`H+0x102C`** |
| 节点 `+4` 语义 | 指令**绝对地址** | **完全一致** ✅（与 th15–18 的「sub 索引」**不同**） |
| sub 表 | `res+0x8C`，项 8B，首指令 `+4+0x10` | **完全一致** ✅ |
| 管理器全局 | `0x4DB544`（ENEMY_MANAGER_PTR） | **`0x4B43DC`**（本迭代新增 `ECL_MGR_PTR`） |
| 段落 sub 进入方式 | 同步 `call` | **`opcode 0x0F`（`@MainSubNN() async`）派生独立线程** ⇒ 主线 `T+4` 全程停在 `main` |
| boss 宿主 | 同一条链 | **同一条链**（`enmCreateA`/`0x0101` 经同一宿主工厂，无条件入链） |

> **两条本作硬约束**（报告 §Q9 / §10.2 / §5.4）：① **必须遍历宿主线程链** —— 只读主线线程会永远读到 `main`；② **必须外层遍历全局宿主链** —— boss / 中boss 宿主跑在自己的线程链上，主线宿主的线程链**读不到** boss sub。
> **等价写法**：从链头 `mgr+0x68` 走（本实现）与从 `H_main+0x12C0` 沿 `next` 走等价（boss 宿主必排在 `main` 之后）；从链头走覆盖面更大。

---

## 2. 改动清单（文件 + 行号区间）

> 行号为改动后 `thprac/src/thprac/thprac_th12.cpp`（2630 行）的当前值。

| 文件 | 行号区间 | 改动 |
| --- | --- | --- |
| `docs/logger/tools/gen_th12_checkpoints.py` | 新建（87 行） | 由 `gen_th14_checkpoints.py` 复制，**只改 5 处**：`CSV_PATH` → `th12_checkpoints.csv`、`OUT_PATH` → `thprac_th12_checkpoints_gen.cpp`、`RAW_DELIM = "TH12CSV"`、命名空间 `THPrac::TH12`、符号 `kTh12CheckpointsCsv`。幂等；断言正文不含终止符 `)TH12CSV"`；**自校验逐字节一致（已运行通过）** |
| `thprac/src/thprac/thprac_th12_checkpoints_gen.cpp` | 新建（126 行，生成物入库） | `namespace THPrac { namespace TH12 { extern const char kTh12CheckpointsCsv[] = R"TH12CSV(…)TH12CSV"; } }`，CSV **3515 B 逐字节**嵌入 |
| `thprac/src/thprac/thprac_th12.cpp` | `3`–`10` | 补 include：`"..\3rdParties\rapidcsv\rapidcsv.h"` + `<algorithm> <cstdio> <cstring> <sstream> <string_view> <unordered_map>`（`<format>` 为既有） |
| 同上 | `52` | `enum addrs` 新增 **`ECL_MGR_PTR = 0x4b43dc`**（全局 ECL 管理器；**原有 enum 里没有**） |
| 同上 | `612`–`1022` | `extern const char kTh12CheckpointsCsv[];`（`612`–`614`）+ 匿名命名空间常量/谓词（`616`–`703`）+ `class TH12Logger`（`705`–`779`）+ 全部实现（`781`–`1022`）。**落点：`enum addrs` 之后、`class TH12InGameInfo`（`:1024`）之前** |
| 同上 | `1105`–`1108` | ① 显示：`TH12InGameInfo::OnContentUpdate()` 内、`ImGui::Text("%8d", mBombCount);` 之后插入 `NextColumn` / `Text("当前段落")` / `NextColumn` / `Text("%s", …CurrentSection())`，保持既有 `Columns(2)` 的「标签/值」成对结构 |
| 同上 | `2435` | ② 每帧：`EHOOK_DY(th12_update, 0x4625fb, 1, …)` 内、`TH12InGameInfo::singleton().Update();` 之前加 `TH12Logger::singleton().Update();` |
| 同上 | `2514` | ③ 一局开始：`EHOOK_DY(th12_game_start, 0x421DEF, 6, …)` 的**既有 lambda 体内**（末尾）加 `TH12Logger::singleton().OnStageStart();` |
| 同上 | `2597` | ④ 预构造：`THGuiCreate()` 内、`TH12InGameInfo::singleton();` 旁加 `TH12Logger::singleton();`（跑一次 `LoadCsv`） |

> ⚠ ③ **必须写在既有 lambda 体内**：hook 框架对带回调的钩子用 INT3 + `vehHooks[addr] = this`（`thprac_hook.cpp:223`），**同址挂第二个钩子会覆盖第一个**，会让既有计数清零失效（th16 §6.1 同坑）。
> **未做（按本迭代边界）**：不碰 `thprac/thprac.vcxproj` / `.filters`（生成物登记由主代理统一处理）；不做日志落盘（不加 `GameSession` / `OnMiss` / `OnBomb` / `interrupt` / `stageClear` / `spellFlag`）；不新增 hook；不动 `thprac_locale_def.*`；不碰其它作。

---

## 3. 采样与安全校验

`TH12Logger::Update()`（`thprac_th12.cpp:875`–`1022`）：

1. `STAGE_NUM` 过滤：`∉ {1..7}` 直接 return（**不改状态**，保留上一值；菜单 / 退出一局时不重置，与需求「保留最后 checkpoint」一致）。
2. `mgr` / `res` / `table` **逐级判空**；`count ∈ (0, 8192]`（挡住 `res` 被破坏的唯一哨兵）。
3. 遍历硬上限：宿主 ≤ **1024**、线程 ≤ **64**（防链成环）；每步先取 `next` 再解引用当前节点。
4. `ip == 0` ⇒ 该线程已结束，跳过。
5. `NameOfIp`：`upper_bound` 命中 `mIpMap` 的**地址升序**区间；落在最后一个区间之后仍返回最后一个 sub（最坏是「名字不在白名单 ⇒ no-op」）。`mIpMap` 仅在 `(count, table)` 变化时重建（表按**名字字典序**存放、地址不单调 ⇒ 必须显式排序）。
6. **不缓存任何跨帧指针**；`mIpMapCount` / `mIpMapTable` / `mMidBossHost` 只作变化检测 / 比较，**从不解引用**。
7. 白名单：`LoadCsv()` 用 `std::istringstream(kTh12CheckpointsCsv)` + `rapidcsv::Document`；跳过列数 < 4 的行、第 2 列 `!= "th12"` 的行；解析失败 ⇒ 空表（恒显示 `main`，不抛到游戏线程外）。`BuildStageMap()` 按 `ST%d` 过滤本关（`MainSub00` / `Boss1` / `MBoss1` 等名字在 ST1–ST7 中大量重复，**必须按关过滤**），**每关一次**（`STAGE_NUM` 变化时重建）。

### 3.1 `STAGE_NUM` 换算（🟡）

`STn → n`（1..7，7 = Extra）；内置 `9 → 7` 的防御性映射（与 th14 / th16 同法）。
**本作线索**：`thprac_th12.cpp` 自己的 `th12_patch_main` 钩子（挂在游戏 VA `0x40E8DF`）里 `(GetMemContent(STAGE_NUM) - 1) % 6 == 0` 并注释「must not be st1/7」，把 ST1 与 ST7 同等对待 ⇒ **Extra 很可能直接报 7**（不是 9）。两条路径都安全：报 7 时原样通过，报 9 时被映射为 7。**🟡 待真机确认**（临时诊断行 `ImGui::Text("DBG stage=%d", *(int32_t*)0x4b0cb0);`，验完删除）。

### 3.2 白名单同名容忍项（照实记录，不新增偏移）

| 关 | 入口 sub | 冲突记录名 | 处置 |
| --- | --- | --- | --- |
| ST3 | `MBoss1` | `ST3 后半 #1`（csv 第 30 行）/ `ST3 MID1`（第 33 行） | `emplace` **先到者胜** ⇒ 显示 `ST3 后半 #1`；且按前缀分类为**中boss 命中**（见 §4） |
| ST7 | `MainSub03` | `ST7 前半 #4`（第 90 行）/ `ST7 后半 #3`（第 93 行） | 先到者胜 ⇒ 显示 `ST7 前半 #4`；**不可区分**，latch 容忍（th13 §5.2 同类） |

> 本作**无 `+0x` 偏移行**（114 行全是纯 sub 名），故**不实现** th13 方案里的「`mOffsetRows` 偏移匹配」骨架 —— 见 §7 待决项。

---

## 4. 中boss 窗口口径

**业务 bug（同 th16 §5.7）**：若干关卡的「后半第一节」在中boss 出场后**立刻开跑**，于是 IGI 面板在中boss 还活着时就跳到 `STn 后半 #1`。

静态根因（`reasm/exes/ecl_dump/th12/*.decl` 的 `main()` 逐关核对）：

| 关 | `main()` 序 | 中boss 存活期间是否已进后半 | 需要窗口 |
| --- | --- | --- | --- |
| ST1 | `enmCreateA("MBoss")` → `wait(400)` → `@MainSub10() async`（后半 #1） | **是**（中boss 约 1440 帧） | ✅ |
| ST2 | `enmCreateA("MBoss")` → `wait(300)` → `@MainSub06()`（后半 #1） | **是** | ✅ |
| ST3 | `enmCreateA("MBoss")` → **`deathWait()`** → … → `@MainSub04()` | 否（先等中boss 死） | ➖（无害） |
| ST4 | `enmCreateA("MBoss")` → `wait(1620)` → `@MainSub04() async`（后半 #1） | **是**（`MBoss1` 全程 ~2000+ 帧，按 `wait`/循环离线估算，> 1620） | ✅ |
| ST5 | `enmCreateA("MBoss")` → `wait(1000)` → `@MainSub05()`（后半 #1） | **是** | ✅ |
| ST6 | `enmCreateA("MBoss")` → `wait(1860)` → `enmCreateA("Boss")` | 否（中boss 之后无章节行） | ➖（无害） |
| ST7 | `enmCreateA("MBoss")` → **`deathWait()`** → `@MainSub00b()` | 否 | ➖（无害） |

**机制**（沿用 th16 §5.7 / th14 形态，`mPending` / `mMidBossActive` / 三类命中裁决）：

| 项 | 口径 |
| --- | --- |
| 命中分类 | 按**白名单键前缀**（`ClassifySub`）：`MBoss*` = 中boss、`Boss*` = 关底boss、其余（`MainSub*`）= 章节。不改 CSV |
| 中boss 战结束（主判据） | 当前 sub 名命中**死亡 / 逃逸 sub 名前缀**：**`MBossDead` / `MBossEscape`**（**逐名精确枚举**，见下） |
| 裁决 | `bossHit` → 关窗并清 pending；否则 `midHit && !mMidBossActive` → 开窗、显示中boss 记录；否则 `chapHit` → 窗口内**只存 `mPending` 不显示**，窗口外正常显示 |
| 关窗 | 死亡 sub 名一出现即关窗并**把 `mPending` 补显示出来**（取最后一个，与「值只单调前进」一致）；`mPending` 为空则维持中boss 记录 |
| `mMidBossDone` | **不需要**（沿用 th14 的做法）：th12 的中boss 容器 sub `MBoss` **不在白名单**，容器自循环时命中不会重开窗口 |
| 采样点 | `th12_update`（`0x4625FB`）每帧；同帧内「主线程进 `MainSubNN`」与「`MBoss1` 命中」都可见，靠裁决优先级处理，不需要额外时序守卫 |
| 新增钩子 | **无**。只复用现有 walk 已读到的 `name` 指针；`IsMidBossEndSub()` 只做 `strncmp` |

### 4.1 中boss 死亡 / 逃逸 sub 名（**离线逐名精确枚举**）

枚举口径：`reasm/exes/ecl_dump/th12/*.decl`（9 份）全量扫描 **`setBoss(-1)` 站点**（脚本 Python，逐条打印所在 sub 名），并用 `setNext` / `setTimeout` 的转移目标交叉核对：

| sub 名 | sub 定义行（`*.decl`） | 所在关 |
| --- | --- | --- |
| **`MBossDead`** | `stage01:2162`、`stage02:1782`、`stage03:2731`、`stage05:2352`、`stage07:1145` | ST1 / ST2 / ST3 / ST5 / ST7 |
| **`MBossEscape`** | `stage01:2206`、`stage02:1820`、`stage03:2778`、`stage05:2402`、`stage07:1180` | ST1 / ST2 / ST3 / ST5 / ST7 |

- 两者都是中boss 容器的**真实相位**，由容器自己用 `setNext` / `setTimeout` 转入（例：ST1 `MBoss()` 的 `setNext(1, 0, 1440, "MBossDead")` + `setTimeout(1, "MBossEscape")`；ST7 `MBossCard3` 的 `setNext(0, 0, 3600, "MBossDead")`），各持续 ~120–180 帧，**远长于一帧采样**。
- **前缀必须精确**：松匹配（「含 Dead」）会命中 `MBossCard3AtLoser` 这类名字；而 `BossDead`（关底收尾，同样在这些文件里）与 `MBossDead` **首字符即不同**，`strncmp(name,"MBossDead",9)` 不会误中 —— 这正是「中boss 收尾」不被读成「关底收尾」的保证。
- 与入口报告 §1.2 的相位链**一致**（ST1/2/3/5/7 的 midboss 链均以 `MBossDead` 收尾；ST4 / ST6 的 midboss 列只有 `MBoss1`）。

### 4.2 ★ th12 专有：`ST4` / `ST6` **没有** `MBossDead` / `MBossEscape`

**离线事实**：`stage04.decl` 与 `stage06.decl` 的中boss 容器 `MBoss()` **连续调用 `setBoss(0)` 与 `setBoss(-1)`**（`stage04:1839`/`1846`、`stage06:2996`/`3003`），随后 **同步 `@MBoss1()`** 跑完整场战斗；`MBoss1` 结尾是 `wait(50); delete();` ⇒ **宿主被直接销毁**（容器 `MBoss()` 尾部的 `goto`/`wait` 循环根本不会执行）。**没有任何死亡 / 逃逸 sub 可观测**（`stage04/06.decl` 全文件 `setBoss(-1)` 只有上述两处，都在容器体内）。

⇒ 只按 `MBossDead`/`MBossEscape` 关窗的话，**ST4 的窗口永不关闭**：中boss 存活期间 latch 的后半 checkpoint 全进 `mPending`，之后被 `bossHit` 强制关窗时一并丢弃 ⇒ `ST4 后半 #1..#4` 永远显示不出来。ST6 因中boss 之后没有章节行，**无害**（停在 `MID1` 直到 `BOSS1`，本就正确）。

**处置（本实现）**：新增一条**关窗条件** —— **打开窗口的那个中boss 宿主从全局宿主链上消失**即关窗并补显示 `mPending`。

```cpp
// mSection / mPending / mMidBossActive 与 th14 同；
// mMidBossHost：产生中boss 命中的宿主指针，**只比较、不解引用**（与 mIpMapTable 同性质）
if (host == mMidBossHost) midBossHostAlive = true;              // 宿主链遍历中顺带标记
...
if (mMidBossActive && mMidBossHost && !midBossHostAlive && !midHit) {
    mMidBossActive = false; mMidBossHost = 0;
    if (mPending) { mSection = mPending; mPending = nullptr; }
}
```

- **对 ST1/2/3/5/7 零行为变化**：这些关的 `MBossDead`/`MBossEscape` 先触发（宿主在其 `delete()` 之前还活着）⇒ 主判据先关窗。
- **对 ST4**：中boss 被销毁那一帧关窗 ⇒ 补显示 `mPending`（最后一个已 latch 的后半 checkpoint），语义与 th16 §7.1.2 的「慢杀 flush」一致。
- `!midHit` 守卫：同帧出现**另一个**中boss 宿主时（宿主指针被复用 / 双中boss）不误关。
- **这是本实现相对 th14 模板的唯一扩展**，共 ~6 行 + 1 个成员；如需回到严格模板形态，删掉该分支与 `mMidBossHost` 即可（代价：ST4 后半 #1..#4 不再显示）。**已列入 §7 待主代理确认项。**

---

## 5. 与 th14 / th16 模板的逐项对照

| 项 | th14（模板） | th16（权威参考） | **th12（本实现）** |
| --- | --- | --- | --- |
| 宿主链 | `mgr+0xD0` × `H+0x11D0` | `mgr+0x180` × `H+0x11FC` | **`mgr+0x68` × `H+0x1030`** |
| res | 按宿主 `H+0x11CC` | 按宿主 `H+0x11F8` | **按宿主 `H+0x102C`** |
| 当前 sub | `ip → 区间反查`（`+4+0x10`） | `subIdx`（`thread+4`） | **`ip → 区间反查`（与 th14 同）** |
| 中boss 收尾名 | `MBossDead`/`MBossEscape`/`MBossBDead`/`MBossBEscape` | `MBossDead` 等 5 个前缀 | **`MBossDead`/`MBossEscape`**（逐名枚举） |
| `mMidBossDone` | 无（不需要） | 有 | **无**（同 th14） |
| 关窗补充条件 | 无 | 无 | **宿主消失**（ST4/ST6 专有） |
| `mSpellFlag` / `mStageClear` / `GameSession` | 有 | 有 | **无**（属日志落盘迭代，本迭代明确不做） |
| 接口数 | 7（含日志） | 7 | **3**（仅监控） |

---

## 6. 未真机声明 / 已知存疑（🟡）

> 按 AGENTS.md §2.5：**未真机验证的地址不得写成结论**。以下全部为**静态实现**，未编译、未真机；本机未跑 `MSBuild`（主代理统一构建）。

1. **全部地址 / 偏移均引自本作逆向报告的静态定案**（`th12_ecl_re_report.md` §1 / §5.4），本身即 🟡 未真机三态。本实现未新增任何逆向结论。
2. **R5（入口报告 §4）「`ST6 BOSS9/BOSS10` 过渡 sub 可能多采一帧」**：`BOSS9`/`BOSS10` 的补丁落点是容器 `Boss5`/`Boss6`（非符），当帧才 `call BossCard5/6`。**未在实现里消除**（按任务要求如实记录）。同类机制在 ST4 / ST6 的**中boss** 上：`MBoss1` 之前容器会先跑一段非白名单代码（`wait(60)` 等），不影响 latch（那时窗口未开）。
3. **`ST7 END_S9/END_S10` 口径与 ST6 不一致**（入口报告 §4 R4）：ST7 登记**容器** sub（`Boss9`/`Boss10`），ST6 登记**符卡** sub（`BossCard5`/`BossCard6`）。本实现按 CSV 原样消费，**未改数据**；真机若发现 ST7 `END_S9` 长期停在容器上，属数据口径问题而非实现问题。
4. **`ST3 后半 #1 = MBoss1`**：该行既是章节记录又是中boss 入口，按前缀被分类为**中boss 命中** ⇒ 它会**打开**中boss 窗口。ST3 的章节都在 `deathWait()` 之后开跑，窗口内不会积压 pending，故**预期无害**；真机需确认 `ST3 后半 #2/#3` 能正常推进（`MBossDead` 关窗）。
5. **`STAGE_NUM` 换算**（§3.1）：内置 `9 → 7`，本作 Extra 很可能直接报 7。**未真机**。
6. **R2（本作最关键的一条）**：进关后主线线程（`*(u32*)(H+0x1030)`）的 `T+4` 应**长期落在 `main`**，而线程链上应能观察到 `MainSubNN` 线程。**若观察到主线线程自己进入 `MainSubNN`，则 async 结论需订正**（本实现的「遍历线程链」写法仍然成立，只是不再必需）。
7. **R3 短命 worker**：`MainSubNN` 多为短命 worker（`stage01` 的 `MainSub02` 约 200 帧）；latch 只在「看见」入口 sub 的那一帧置值 ⇒ 采样频率须 ≥ 1 次/帧。若偶发漏采，备选是在 `spawn`（`0x46724C`）处捕获（报告 §10.5，**未实现**）。
8. **R7 练习跳章**：`ins[0xA] = 0xFF` 的难度掩码在 E/N/H/L 四难度下是否都能执行（否则 `goto` 会被 `test byte [edi+0x101C], al` 跳过）。
9. **R6 区间表重建**：跳章 / 换关后运行期缓冲切换（`res+0xC+slot*4`）时 `table` 是否被**重建**（加载器 `FUN_004692E0` 累加 `res+8`）⇒ 本实现靠 `(count, table)` 变化检测重建；若「count 与 table 均不变但内容变了」会读到脏名字（与 th14 同一风险面）。
10. **`ST4` 的中boss 窗口**（§4.2）：关窗靠**宿主指针消失**，依赖宿主析构路径 `FUN_00412BE0` 及时摘链；若指针被新宿主复用，窗口可能延后关闭（最坏到 `bossHit`）。**未真机**。
11. **st4 路线分支**（报告 §13）：th12 st4 是否有路线分支**未定论**，本实现未做分支特化。
12. **变体**：Steam / 汉化版是否共用同一 RVA（本文地址来自原版 `ts=1251344348`）。
13. **英文 locale 下中文记录名渲染为空**：已知固有局限（`thprac_gui_locale.cpp` 的 EN glyph range 不含 CJK，且 CSV 记录名本身就是中文），非本改动引入。

---

## 7. 待决 / 需主代理决策项

1. **`ST4`/`ST6` 无 `MBossDead`/`MBossEscape`**（§4.2）：本实现用「宿主消失」关窗。**替代方案**：① 严格 th14 形态（不关窗 ⇒ ST4 后半 #1..#4 显示不出来）；② 按关禁用窗口（需硬编码 `{1,2,3,5,7}` 名单）；③ 在 `MBoss1` 的收尾处另找可观测锚点。**请主代理确认采用哪一种**（本实现选的是「宿主消失」，代价最小且对其它关零影响）。
2. **是否补 th13 方案的「`+0x` 偏移匹配」骨架**：th12 CSV **114 行全是纯 sub 名**，无偏移行 ⇒ 本实现**未**引入 `mOffsetRows` / 偏移解析（约 30 行死代码）。任务书 §5.3 说「若判断会引入计划外复杂度，先向主代理确认」—— 本次判断为**不引入**（保持 th12 与 th14 同形；th10–th13 的偏移骨架一致性由 th13 方案承载）。
3. **工程登记**：`thprac_th12_checkpoints_gen.cpp` 需登记进 `thprac/thprac.vcxproj` / `.filters`（按边界**由主代理统一处理**，本实现未碰）。
4. **`logger_progress.md` §1 总表的 th12「E5 监控实现」列**：本实现未改（避免与并行作子代理写冲突），待主代理统一回填。

---

## 8. 真机验证用例（供 E6，未跑）

对齐 `th16_monitor_impl_report.md` §7 / §7.1 与入口报告 §4：

1. **latch 正确性（道中）**：进 ST1 从头打，F8 开 IGI。title 卡应为 `main`；随进度 `ST1 前半 #1 → 前半 #2 → 前半 #3 → 后半 #1 → 后半 #2`。整段值**只单调前进或不动，任何回退都是 bug**。
2. **★ async 线程（本作最关键）**：`ST1 后半 #1`（`MainSub10`）等段落入口必须能在**宿主线程链**上看到；若只有主线线程可读，本实现会恒显示 `main`。
3. **未命中 no-op**：段内非白名单 sub（`MainSub01`/`MainSub03`…）运行期间值**必须不变**，不得回退 `main`。
4. **★ boss 宿主可达（全局宿主链）**：打 ST1 中boss → `ST1 MID1`；关底 boss → `ST1 BOSS1..BOSS4` 与符卡 / 非符切换同步。**若中boss / boss 出场后值不动** ⇒ 宿主链或 `H+0x102C` 取法失败。
5. **★ 中boss 窗口（ST1/ST2/ST5）**：中boss 存活期间即便主线程已进 `MainSub10`，显示**必须保持 `ST1 MID1`**；`MBossDead` 一出现才变 `ST1 后半 #1`。**慢杀 flush**（中boss 存活 >20s → 关窗后直接跳后一个 checkpoint）与**快杀 flush**（无值可补 ⇒ 维持 `MID1`）**两个分支都要跑**。
6. **★ ST4 专有（宿主消失关窗）**：打 ST4，中boss 出场显示 `ST4 MID1`；中boss 被击破后**必须**推进到 `ST4 后半 #1`（或直接跳过后几个），**不得永远停在 `ST4 MID1`**。ST6 同理：中boss 期间停 `ST6 MID1`，之后 `ST6 BOSS1`。
7. **`ST3 后半 #1`（= `MBoss1`）**：打 ST3，确认 `后半 #2`（`MainSub02b`）/`后半 #3`（`MainSub05`）在 `MBossDead` 之后正常推进。
8. **同名容忍**：`ST7 前半 #4` 与 `ST7 后半 #3`（同 `MainSub03`）实际停在哪个记录名（预期 `ST7 前半 #4`）。
9. **关卡过滤**：打 ST2 不得出现 ST1 的记录名。
10. **练习 warp**：warp 到 `ST1 BOSS3` 应变 `ST1 BOSS3`；warp 到段中途应保持 `main` 或上一段；**warp 到 `ST1 后半 #1` 时中boss 窗口不得被打开**（负例 A）。
11. **卡死反例（最关键）**：打 ST1 中boss，战斗中暂停 → 练习菜单重开到 `ST1 前半 #1` → 之后 `前半 #1 → 后半 #1 → 后半 #2` 必须继续推进，**不得永远停在 `ST1 MID1`**。
12. **生命周期回归**：ST1→ST2 连打 / Restart / 自杀 Continue / 回标题再进 / 直接退出 ⇒ 无崩溃，每关从 `main` 起算。
13. **`STAGE_NUM` 换算**：临时诊断行实测 ST1..ST6 / Extra / 练习 warp / 菜单，确认 Extra 报 7 还是 9，验完**删除**。
14. **CSV 嵌入未漂移**：临时显示 `mEntries.size()`，应为 **114**。
15. **变体**：Steam / 汉化版 RVA 一致性。

---

## 9. 日志接线（JSONL 落盘，口径 11/12/13/15/16）

> 需求：`docs/req_logger.md`（口径 11–16）。模板：`thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（**逐字同构移植，只改常量与本作判据**）。共享层 `thprac_games.{h,cpp}` 的 `#pragma region Game Session Log` **零改动**。
> 本迭代**只加日志接线，不改监控逻辑**：`TH12Logger` 的采样 / 中boss 窗口 / 段落裁决代码**一字未动**，新代码全部是**插入**。
> 状态：**已接线（🟡 未编译、未真机）**。按 AGENTS.md §2.5，本文全部结论在真机验证前不得当作结论。

### 9.1 改动清单（`thprac/src/thprac/thprac_th12.cpp`，全部纯插入）

| # | 位置（改动后行号） | 改动 |
| --- | --- | --- |
| 1 | `enum addrs`（`:53`） | `SPELL_OBJ_PTR = 0x4b43cc,`（符卡对象全局指针） |
| 2 | 匿名 namespace（`:657`–`:666`） | `constexpr uintptr_t kSpellFlagsOff = 0x7C;`（含证据注释） |
| 3 | 匿名 namespace，`IsMidBossEndSub()` 之后（`:708`–`:731`） | `bool IsStageClearSub(const char* name)`（`strcmp` 精确匹配 **`"BossDead"`**，见 §9.3） |
| 4 | `TH12Logger` public（`:760`–`:761`） | `void OnMiss(); void OnBomb();` |
| 5 | `TH12Logger` private 方法声明（`:796`） | `void SampleSpellCard();` |
| 6 | `TH12Logger` private 成员（`:821`–`:835`） | `int32_t mSpellFlag = 0; uintptr_t mSpellObj = 0; bool mSpellActive = false; bool mStageClear = false; GameSession mSession;` |
| 7 | `OnStageStart()`（`:923`、`:928`–`:931`） | `mSession.Begin("th12");` + 复位 `mSpellFlag/mSpellObj/mSpellActive/mStageClear` |
| 8 | `OnBomb()` 之前/之后（`:937`–`:980`） | `OnMiss()` / `OnBomb()` / `SampleSpellCard()` 三个定义（各一行 `PushEvent`；`SampleSpellCard()` 照 th14 `:931-960`） |
| 9 | `Update()` 顶部（`:984`–`:999`，**所有 early return 之前**） | `bool inGame = GetMemContent<uint32_t>(0x4b4514) != 0;` → `SampleSpellCard();` → `if (mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame)) PushEvent(Interrupt, mSection);` → `mSession.Tick(inGame, <shot>);` |
| 10 | `Update()` 换关块（`:1022`–`:1025`） | `mSpellFlag/mSpellObj/mSpellActive/mStageClear` 复位 |
| 11 | `Update()` 候选变量区（`:1042`） | `bool sawStageClear = false;` |
| 12 | `Update()` 遍历体内（`:1086`–`:1088`，紧跟 `IsMidBossEndSub` 两行） | `if (IsStageClearSub(name)) sawStageClear = true;` |
| 13 | `Update()` 段落裁决之前（`:1109`–`:1112`） | `const char* prevSection = mSection;` |
| 14 | `Update()` 段落裁决之后、`Latch()` 之前（`:1160`–`:1174`） | 段落变化 ⇒ `if (mSpellFlag > 0) --mSpellFlag;` + `mStageClear = false;`（**复位在前**）；`if (sawStageClear) mStageClear = true;`（**置位在后**）；`mSession.Latch(GameLogRecordOfSection(mSection));` |
| 15 | `th12_bomb_dec`（`:2672`）**既有 lambda 体内** | `TH12Logger::singleton().OnBomb();` |
| 16 | `th12_life_dec`（`:2677`）**既有 lambda 体内** | `TH12Logger::singleton().OnMiss();` |

**已有、本次未动**：`th12_update`（`:2588`）的 `TH12Logger::singleton().Update();`、`th12_game_start`（`:2667`）的 `TH12Logger::singleton().OnStageStart();`、`THGuiCreate`（`:2752`）的 `TH12Logger::singleton();`（均属 E5 监控迭代，本次无需新增；行号为**接线后**值，E5 时的 `:2435`/`:2514`/`:2597` 见 §2）。

**约束遵守**：**未新增任何 hook**（`OnBomb`/`OnMiss` 写进既有 `th12_bomb_dec` / `th12_life_dec` 的 EHOOK_DY lambda 体内 —— 同址二次 hook 会覆盖前一个，见 `thprac_hook.cpp:223`）；**未引入** `AdvancedGameOptions`；**未碰** `thprac.vcxproj` / `.filters`、`thprac_games.{h,cpp}`、`thprac_locale_def.*`、其它作、`wind`。`git diff` 对 `thprac_th12.cpp` **纯插入、0 删除**。

### 9.2 (a) in-game 判据

**`GetMemContent<uint32_t>(0x4b4514) != 0`**。

- th12 **没有** `PLAYER_PTR` 枚举项，但 `:45` 的生产宏 `#define player (*(Player**)0x4b4514)` 就是它。
- **核实「退出游戏回菜单时归零」**：本作 IGI 面板 `TH12InGameInfo::OnPreUpdate()`（`:1301`）的**显隐判据**正是这一处 —— `if (*(DWORD*)(0x004B4514)) { GameUpdateInner(12); }`（`:1303`）与 `if (*(THOverlay::singleton().mInGameInfo) && player) { Open(); } else { Close(); }`（`:1307`）。面板靠它 `Open()`/`Close()` 且长期正常显示 ⇒ 退出游戏回菜单时该地址归零（**与口径 3「复用各作 IGI 面板的显隐判据」同源**，不新增逆向）。
- 另有 `:1381` 读 `*(DWORD*)(0x4B4514)+0x9E4` 取玩家坐标、`:2506`+ 用 `player->` 读伤害源 ⇒ 该地址在局内确为有效玩家对象指针。

### 9.3 (b) `IsStageClearSub` 清单（逐名枚举，**不可跨作外推**）

**收录（`strcmp` 精确匹配）**：**`BossDead`** —— th12 关底 boss 收尾 sub **只有这一个名字**。

**枚举口径**：对 `reasm/exes/ecl_dump/th12/*.decl`（9 份）全量 `^void .*(Dead|Escape)` 检索 + 全量 `setBoss(` 站点交叉核对。

| 名字 | 站点（`.decl`） | 裁决 |
| --- | --- | --- |
| **`BossDead`** | `stage01:1187`、`stage02:642`、`stage03:1915`、`stage04:1388`、`stage05:1614`、`stage06:2597`、`stage07boss:3470` | ✅ **收录**。各关由 `setNext(0, 0, N, "BossDead")` 从最后一个攻击 sub 转入；body 里 `enmKillAll()` 后紧跟 **`+60:` 标签**、结尾 `setBoss(-1); delete(); delete();`，运行**远长于一帧采样**（ST6/Extra 还有 `gameSpeed(0.5)`）。 |
| `MBossDead` / `MBossEscape` | `stage01:2162/2206`、`stage02:1782/1820`、`stage03:2731/2778`、`stage05:2352/2402`、`stage07:1145/1180` | ❌ **排除**：中boss 收尾（已由 `IsMidBossEndSub` 覆盖），中boss 死 ≠ 通关。`BossDead` 与 `MBossDead` **首字符即不同**，精确匹配天然不混淆。 |
| `Girl00Dead`、`Cir00Dead`、`Cir01Dead`、`Cir02Dead`、`ParasolAt1Dead`、`ParasolAt3Dead`、`BossCard6_atLineDead` | 各 `stageNN.decl` | ❌ **排除**：`setDeath()` / `setNext()` 注册的**小怪 / 部件死亡动画**，boss 存活时就会跑。 |
| `BossCard2UfoRedDead`、`BossCard4UfoBlueDead`、`BossCard6UfoGreenDead`、`BossCard8UfoRainbowDead` | `stage07boss.decl:1852/2131/2536/2854` | ❌ **排除**：Extra 关 UFO 的收尾动画，符卡战期间运行。 |

**★ 本作关键差异 —— th12 没有独立的关底 escape sub**：th14 有单独的 `BossEscape`（超时路径），th12 把超时**合进 `BossDead` 体内**（`unless ($TIMEOUT == 0) goto BossDead_xxx`），因此**不需要**（也**不存在**）`BossEscape` 之类的名字。`.decl` 全量 `setTimeout(...)` 检索：**只有 `MBossEscape`**（中boss 侧），关底**零处**。

**与中boss 侧的区分**（任务书提到的 ST4/ST6）：ST4/ST6 的**中boss**容器无 `MBossDead`/`MBossEscape`（`MBoss1` 结尾 `wait(50); delete()` 自毁宿主，监控层用「宿主消失即关窗」处理）；**关底**收尾是 `BossDead`，与中boss 无关，两者不混淆。

### 9.4 (c) 符卡对象指针 + flags 偏移（口径 15 的 `spellFlag`）

**结论**：**符卡对象全局指针 = `0x4B43CC`（RVA `0xB43CC`）**；**flags 偏移 = `+0x7C`**，**bit0 = 符卡存活位**。**与 th14 结构完全相同**（id `+0x78`、名 `+0x38`、flags `+0x7C`），**只有全局地址不同**。本作**无**既有生产信号读符卡状态（`thprac_th12.cpp` 全域 `spell` 仅练习菜单 `SpellPhase()` 的 glossary，不读对象），故做了**一次有界的只读逆向**（`pefile` + `capstone`，素材 `../exes/th12.exe`，737280 B，`TimeDateStamp = 1251344348`，`RELOCS_STRIPPED` ⇒ `RVA = VA − 0x400000`）。

**证据（VA）**：

| 项 | 证据 |
| --- | --- |
| 全局指针 | `0x40E069 mov edi,[0x4b43cc]`（声明）、`0x40E5E3 mov edi,[0x4b43cc]`（结算） |
| 构造（对象大小 `0xC0`） | `0x40D860`：`and [esi+0x34],~1` → `push 0xC0; push 0; push esi; call 0x477420`（`memset`）→ `or [esi],2` → **`0x40D877 mov [0x4b43cc],esi`**；另一构造路径 `0x40DAC0`（`malloc(0xC0)` + `memset`）→ `0x40DACB mov [0x4b43cc],esi` |
| 析构（全局归零） | **`0x40DA8E mov dword [0x4b43cc], ebp`**（`ebp = 0`） |
| **声明（置位，唯一）** | `0x40E060`（spell 声明函数）：`0x40E0AA mov [edi+0x78],esi`（spell id）→ 拷名字到 `[edi+0x38]` → **`0x40E0C7 or eax,3` / `0x40E0CA mov [edi+0x7c],eax`**（bit0 存活 + bit1 可收取） |
| **结算（清位，唯一）** | `0x40E5E0`（spellEnd）：`0x40E5EE test byte [edi+0x7c],1`（不存活 ⇒ no-op）→ **`0x40E61D and dword [edi+0x7c],0xFFFFFFFE`** → `0x40E62F and [edi+0x7c],~0x20` → `0x40E63F test byte [edi+0x7c],2`（收取判定） |
| ECL 侧：`spell` 声明 opcode | `0x1B5`（`spell`）/ `0x1B6`（`spell2`）/ `0x1B7`（`spell3`）/ `0x1B8`（`spellEx`）—— 处理器在 `0x4188E1` 起：`movsx eax,word[ins+4]; sub eax,0x1b5` ⇒ id = `base + diff` / `base + diff − 1` / `base + diff − 2` / `base`（**与 th14 同一 id 公式**），随后 `0x418942 call 0x40E060`（声明）。**四个 opcode 共用一个对象**。 |
| 唯一性 | 全 `.text` 扫描：作用于符卡对象的 `and [reg+0x7c],imm` 只有 `~1`(bit0, `0x40E61D`)、`~0x20`(bit5)、`~0x40`(bit6)、`~0x04`(bit2)；`or [reg+0x7c],imm` 只有 `04`/`40`/`08`/`10`（均非 bit0）。**bit0 的置位只有 `0x40E0CA`，清位只有 `0x40E61D`**。 |

**采样不会漏**：静态扫描 9 份 `.decl` 的全部 `setNext` / `@...()` 直接调用 —— **card → card 转移 0 处**（每张符卡后必接一个**非符攻击 sub**，其**首条指令即 `spellEnd()`** 清 bit0，且该非符 sub 都带 `+60:` 标签、运行 ≥60 帧）。⇒ 一帧一次轮询能稳定看到 1→0 下降沿。

**`mStageClear` 保留为兜底**：关底 `BossDead` 在 **`+60:` 之后**才 `spellEnd()`（最后一张卡死亡后 60 帧内 bit0 仍为 1）；且 `BossDead` 也是**超时**路径（`$TIMEOUT != 0`），超时可能落在**非符期**（无下降沿）。最终条件 `mSpellFlag == 0 && !mStageClear`。

**已知副作用**（与 th14/th16 同族，登记 `knownBugs.md` #12）：CSV 剔除的**过渡非符**（th12 各关的 `BossN` 非符、`MBossDead` 之后等）会让计数滞留（120–160 帧以上）⇒ 该窗口退出**假阴性**（少记 `interrupt`），可接受。

### 9.5 shot 字段

`Tick()` 第 2 参接本作 IGI 既有表达式（`:1234`/`:1236`，E5 时的 `:1081-1083`）：

```cpp
mSession.Tick(inGame, (inGame && mSession.NeedsStartInfo())
        ? S(IGI_PL_12[(*(DWORD*)(0x004B0C90)) * 2 + (*(DWORD*)(0x4B0C94))])
        : nullptr);
```

- 取 `CHARA(0x4B0C90) * 2 + SUBSHOT(0x4B0C94)` 作为 `IGI_PL_12` 下标，与 IGI 面板 `diff_pl` 行的机体串**同一来源**（口径 9）。
- `inGame && NeedsStartInfo()` 守卫：菜单里玩家对象不存在，直接读会越界（与 th14 同法）。

### 9.6 未完成 / 待确认（🟡）

1. **未编译、未真机**（本机无 MSVC；主代理统一构建）。全部地址 / 偏移均为**静态结论**，按 AGENTS.md §2.5 在真机验证前不得当作结论。
2. **`0x4B43CC` / `+0x7C` 的真机验证项**：① 标题 / 菜单读 `*(u32*)0x4B43CC` 期望 **0**，进关后非 0；② 打 ST1 `BossCard1`：卡开始后 `obj+0x7C & 1` = 1，击破 / 进入下一非符 sub 后变 0；③ 连续 `BossCard1→Boss2→BossCard2`：每张卡结束后 `bit0` 至少 60 帧为 0。
3. **`BossDead` 采样可达性**：`+60:` 标签前有 `enmKillAll()` / `gameSpeed(0.5)`（ST6/Extra），需真机确认 60Hz 采样稳定命中（与 th16 的 0 帧壳不同，本作有实体的 `+60` 窗口）。
4. **`STAGE_NUM` 换算**（E5 遗留 🟡）：Extra 报 7 还是 9 未真机（本实现内置 `9 → 7`）。
5. **变体 exe**（Steam / 汉化版）是否共用同一 RVA：未核。

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **建档（E5 实现）**。th12 监控代码落地：新增 `docs/logger/tools/gen_th12_checkpoints.py`（复制 th14 版改 5 处，**已运行且自校验逐字节一致**）与生成物 `thprac/src/thprac/thprac_th12_checkpoints_gen.cpp`（CSV 3515 B 嵌入）；`thprac_th12.cpp` 补 include（`:3-10`）、`enum addrs` 新增 **`ECL_MGR_PTR = 0x4b43dc`**（`:52`）、新增 **`TH12Logger`** 模块（`:612-1022`，落点 `enum addrs` 之后、`class TH12InGameInfo`（`:1024`）之前）+ **4 处调用**（显示 `:1105-1108` / 每帧 `:2435` / 一局开始 `:2514` / 预构造 `:2597`）。**只做监控**：接口仅 `OnStageStart()` / `Update()` / `CurrentSection()`，**不含**日志落盘（`GameSession` / `OnMiss` / `OnBomb` / `interrupt` / `stageClear` / `spellFlag` 均不做）。读取面 = **`mgr+0x68` 全局宿主链 × `H+0x1030` 线程链**，逐线程 `ip = *(u32*)(T+4)` 经**地址升序区间表**（`table[i].+4 + 0x10`）反查 sub 名，命中白名单即 latch、未命中 no-op、首命中前 `main`；res 按宿主 `H+0x102C` 取；**不缓存任何跨帧指针**。**中boss 窗口**移植 th16 §5.7 / th14 形态（`mPending` / `mMidBossActive` / 三类命中裁决），死亡 / 逃逸 sub 名**离线逐名精确枚举** = **`MBossDead` / `MBossEscape`**（ST1/2/3/5/7，站点行号见表）；**th12 专有扩展**：`ST4`/`ST6` 的中boss 容器（`stage04/06.decl` `MBoss`）连续 `setBoss(0)`+`setBoss(-1)` 后同步跑完 `MBoss1` 并 `delete()`，**无死亡 sub 可观测** ⇒ 增补「**产生中boss 命中的宿主从全局宿主链消失**即关窗」一条（`mMidBossHost` 只比较不解引用，~6 行，可整段删除回退）。**未碰** `thprac.vcxproj` / `.filters`（主代理统一登记）、`thprac_locale_def.*`、其它作、`wind`；**未新增 hook**（`OnStageStart()` 写在既有 `th12_game_start` lambda 体内）。**未编译、未真机（🟡）**；静态核对：`STAGE_NUM` 换算内置 `9→7`（🟡，本作线索指向 Extra 直接报 7）、CSV 同名容忍 2 处（ST3 `MBoss1`、ST7 `MainSub03`）。 |
| 2026-10-20 | **日志接线（JSONL 落盘，口径 11/12/13/15/16）**。在 E5 监控模块**之上**追加 §9：`thprac_th12.cpp` **纯插入** 16 处（`enum addrs` `SPELL_OBJ_PTR = 0x4b43cc`；匿名 namespace `kSpellFlagsOff = 0x7C` + `IsStageClearSub()`；`TH12Logger` 增 `OnMiss`/`OnBomb`/`SampleSpellCard`/`mSpellFlag`/`mSpellObj`/`mSpellActive`/`mStageClear`/`mSession`；`OnStageStart` 加 `Begin`+复位；`Update()` 顶部 `SampleSpellCard`+`interrupt`+`Tick`、换关复位、`sawStageClear`/`prevSection`、段落变化 `--`/复位/置位/`Latch`；`th12_bomb_dec`/`th12_life_dec` **既有 lambda 体内**加 `OnBomb`/`OnMiss`）。**零新 hook、共享层零改动**。三件本作自定项：**(a)** in-game 判据 = `GetMemContent<uint32_t>(0x4b4514) != 0`（复用本作 IGI 显隐判据，`:1218`）；**(b)** `IsStageClearSub` = **`{"BossDead"}`**（9 份 `.decl` 全量枚举；**th12 无关底 escape sub**，超时合进 `BossDead` 的 `$TIMEOUT` 分支）；**(c)** 符卡对象 **`0x4B43CC`** + flags **`+0x7C`** bit0（置位唯一 `0x40E0CA`、清位唯一 `0x40E61D`；结构与 th14 相同，仅全局地址不同），**有界只读逆向**（`pefile`+`capstone`）。`shot` 接本作 IGI 既有 `S(IGI_PL_12[CHARA*2+SUBSHOT])`。**未编译、未真机（🟡）**。 |

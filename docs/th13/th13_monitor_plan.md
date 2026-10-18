# TH13 ECL 段落监控 — 实现方案（监控 E5）

> 需求来源：`docs/req_logger.md`（ECL 段落监控 + 日志落盘）。**本方案只做「监控」（进度表 E5）**，日志落盘（JSONL / `GameSession` / miss / bomb / `interrupt`）**不在范围**，另行设计。
> 上位文档（术语 / 判定口径 / 粒度，一律以此为准）：`docs/logger/logger_re_disasm_overview.md`（**§1.2 粒度 / §2 术语表 / §5.1 锚点总表 / §6 Q9 / §9.9**）。
> 本作逆向结论：`docs/th13/th13_ecl_re_report.md`（Q1–Q9 静态定案；**§1 指针链 / §2.1 指令头布局 / §2.3 地址区间反查 / §3 sub 表 / §5 宿主捕获 / §10.4 白名单**）。
> 本作入口表小结：`docs/th13/th13_checkpoint_entry_report.md`（**116/116**；§2.1 `MainSub08+0x390` / §2.2 同名容忍 / §3.1 双匹配 / §3.2 指针链 / §3.3 3 行 async-only）。
> 偏移记法口径：`docs/th10/th10_checkpoint_ip_offset.md`（`sub名+0x偏移`，偏移基准 = **相对该 sub 首条指令**；§4.2 / §7 已定案「不新增第 5 列」）。
> 回填口径：`docs/logger/checkpoint_entry_backfill_workflow.md`（§3.3 规则 5 / §5 `goto` opcode 注记）。
> 数据：`docs/logger/th13_checkpoints.csv`（**116 数据行** = 章节 44 + boss 72；**仅 1 行**用偏移记法）。
> 参考实现（权威，真机验证过）：`docs/th16/th16_monitor_impl_report.md`（C 代；**§5.7 中boss 窗口** / §6 工程登记 / §7 验证清单）。
> 参考实现（**同形，直接模板**）：`thprac/src/thprac/thprac_th14.cpp` 的 `TH14Logger`（th14 与 th13 同为「节点 `+4` = **指令绝对地址**」语义，须做「地址 → sub」反查）。
> 生成脚本形态：`docs/logger/tools/gen_th14_checkpoints.py`（87 行；CSV **逐字节**嵌成 raw string）。
> 二进制指纹：`../exes/th13.exe`，**790528 B**，PE `TimeDateStamp = 1313589413`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）。
> 方法：**静态方案设计**——引用已定案的逆向结论，并只读核对 `thprac_th13.cpp` / `thprac_th14.cpp` / `thprac_th16.cpp` / `../exes/ecl_dump/th13/*.decl`。**本方案不写代码、不跑编译。**
> 状态：**方案定案（未实现、未真机）**。逐条标注 **✅ 已定案（有静态证据）** ｜ **🟡 待真机**。
> 地址口径：正文 `VA` 与 `RVA = VA − 0x400000` 并列；术语（宿主 / 线程 / sub / checkpoint / 当前段落）以总括 §2 术语表为准。
> 置信度：✅ 反汇编 / 源码 / 离线解码直接证实 ｜ 🟡 有强证据但缺一环（显式标注）｜ ❌ 未能定案。

---

## 0. 结论速览

| # | 问题 | 结论 | 定案? |
| --- | --- | --- | --- |
| 1 | **运行期锚点** | `mgr = *(u32*)0x4C2188` → 宿主链 `mgr+0xB0`（节点 `H+0x14CC`）；宿主捕获点 `0x42C76B`/`0x42C90A`（命中 `EAX` = H）；`H+0x11D0` = 线程链头（`[+0]` = 主线 T ≡ `H+8`，`H+0x11D4` = 后继）；`H+0x11CC` = res；`T+4` = **当前指令绝对地址**；`res+8` = count、`res+0x8C` = sub 表（项 8B，`+0` = 名字 `char*`、`+4` = 数据基址，首指令 `+4+0x10`）。 | ✅（静态定案） |
| 2 | **「当前 sub」怎么取** | **照搬 th14 形态**：遍历 `mgr+0xB0` **宿主链** × 每宿主 `H+0x11D0` **线程链**，逐线程取 `ip = *(u32*)(T+4)`，经**地址升序区间表**反查得 sub 名。**th13 必须遍历线程链**（3 行 async-only，`H+0x11D4` 起的异步线程）——与 th10–th12 同向，**不可**照搬 th15–th18 的 `thread+4`=索引写法。 | ✅（静态定案） |
| 3 | **偏移匹配怎么算** | ✅ **运行期能拿到该 sub 的首指令地址**：`firstInsn(i) = table[i].+4 + 0x10`（`FindSubByName` @ `0x47984B` 的 `add eax,0x10` 直接证实）。判据 `ip == table[i].+4 + 0x10 + 偏移`（`i` = 该 sub 在运行期 sub 表的下标）。**偏移命中优先于名字命中。** | ✅ 首指令地址可得；**🟡 帧边界 ip 是否恰为该地址**（取决于落点是否等待，见 §3.3/§5） |
| 4 | **数据表示与匹配结构** | **照搬 th14–th18**：CSV 逐字节嵌入生成物 + 运行期 `rapidcsv` 解析；`+0x` 后缀在**解析期**拆成 `(sub 名, 偏移)`；`mNameMap`（纯名行）+ `mOffsetRows`（偏移行）+ `mIpMap`（地址升序 `SubRange{start,index}`）。**不需要单独的第 5 列 / 复合键列**——`sub名+0x偏移` 本身即复合键。 | ✅ |
| 5 | **th13 特有风险** | ① `ST5 后半 #4`(`MainSub08+0x390`) 与 `#3`(`MainSub08`) 同 sub ⇒ **偏移优先**消歧；② `ST4 前半 #3`/`后半 #1` 同为 `MainSub00` ⇒ **不可区分**，latch 容忍；③ `ST4 前半 #1`(`MainSub00h`) 带 `!HL` 门控 ⇒ E/N 下永不命中，latch 容忍。 | ✅（①）／🟡（②③真机确认显示行为） |
| 6 | **复用 th16 的坑** | **中boss 窗口必须移植**：th13 同构（`st01.decl:1292-1295`：`@MainFront()` → `enmCreateA("MBoss",…)` → `wait(160)` → `@MainLatter()`；`MainLatter` 首个子 = `MainSub04` = `ST1 后半 #1`）⇒ 中boss 存活期间主线程就会 latch `后半 #1`。中boss 死亡 sub 名（离线枚举）= **`MBossDead` / `MBossEscape`**。 | ✅（结构同构，静态）／🟡（逐关名单复核） |
| 7 | **工程登记与产物** | 新增 `docs/logger/tools/gen_th13_checkpoints.py` + `thprac/src/thprac/thprac_th13_checkpoints_gen.cpp`；改 `thprac_th13.cpp`（内加 `TH13Logger`，**per-game 模块不单列 cpp**）+ `thprac.vcxproj`/`.filters`。 | ✅ |
| 8 | **实施步骤** | 生成脚本 → 生成物 → 模块（含偏移解析 / 双匹配 / 中boss 窗口）→ 4 处调用 → 工程登记 → `Release\|x86` 构建 → 真机 13 项用例。 | ✅ |

**一句话**：th13 的监控 = **th14 的骨架**（宿主链 × 线程链 + `ip → sub` 区间反查）+ **th16 的中boss 窗口** + **`sub名+0x偏移` 的偏移匹配**（偏移命中优先）。th13 侧偏移行**只有 1 个实例**（`MainSub08+0x390`），故偏移匹配在 th13 上是**单点验证**；其通用性面向 **th10（15 行）** 与 **A 代（时间 / 指令偏移口径）**，见 §8。

---

## 1. 需求与边界

- **目标**：实时显示「当前段落」——CSV 每个 checkpoint 指向一个**段落开头**；运行期读当前 ECL sub（手段），命中白名单即置值（**路过式 checkpoint / latch**：未命中 **no-op**、不回退 `main`；首命中前显示 `main`）。口径见总括 §1.2 / §6 Q9，本方案不自造口径。
- **粒度**：纯 sub 名为主；入口落在容器 `main` 内部、拿不到唯一 sub 名时用 **`sub名+0x偏移`**（偏移 = **相对该 sub 首条指令**的字节偏移）。th13 全作**仅 1 行**用偏移（`ST5 后半 #4` = `MainSub08+0x390`）。
- **本方案范围**：只做**监控**（E5）——显示行 + 每帧采样 + 重置 + 预构造。**不做**：JSONL 落盘、`GameSession`、`OnMiss`/`OnBomb`、`interrupt`/`stageClear`（口径 12）——均属日志落盘迭代，见 `docs/logger/logger_impl_report.md`，另行设计。
- **不做**（沿用 th16 §9 的边界）：不碰 `wind`；不动 `thprac_locale_def.h/.cpp`（外部 devtools 生成，本地无法重生成）；不新增 `AdvancedGameOptions` 开关（跟着 IGI/F9 走）；不加 MSBuild 预构建步骤；不顺手改既有钩子。

---

## 2. Q1 —— th13 的运行期锚点（VA / RVA 并列）

> 全部引自 `docs/th13/th13_ecl_re_report.md`（**静态定案 🟡 未真机三态**；其内部 ✅ = 反汇编直接证实）。地址一律 `VA` / `RVA = VA − 0x400000`。

| 项 | 表达式 / 地址 | VA / RVA | 来源 | 状态 |
| --- | --- | --- | --- | --- |
| ECL 管理器全局 | `mgr = *(u32*)0x4C2188` | `0x4C2188` / `0xC2188` | 报告 §1/§3.1（`GetRes`@`0x414170`） | ✅ |
| ECL 资源对象 | `res = *(u32*)(mgr + 0xAC)` | — | 报告 §1/§3.1/§3.4 | ✅ |
| 宿主链头 / 尾 | `*(u32*)(mgr + 0xB0)` / `*(u32*)(mgr + 0xB4)`；节点 `H+0x14CC = {+0=H, +4=next, +8=prev}` | — | 报告 §5.2（`0x418640`） | ✅（静态兜底） |
| **主线宿主捕获点** | `call 0x4183F0` 的下一跳 `0x42C76B` / `0x42C90A`，命中 **`EAX` = H**；`"main"` @ `0x4AADE0` | `0x42C76B`/`0x2C76B`、`0x42C90A`/`0x2C90A` | 报告 §5.1 | ✅（EHOOK 5B） |
| 宿主 → 线程链 | `H+0x11D0` = 线程链头（`[+0]` = 主线 T ≡ `H+8`；`H+0x11D4` = 后继，0 = 无） | — | 报告 §5.3（`0x417797`/`0x4177A9`/`0x478DE0`） | ✅ |
| 宿主 → res | `H+0x11CC` = res（**≡ `*(u32*)(mgr+0xAC)`**） | — | 报告 §1/§3.4 | ✅ |
| **`[T+4]` 语义** | `ip = *(u32*)(T+4)` = **当前（下一条待执行）指令的绝对地址**；`0` = 线程结束。**不是 sub 索引**（本作无 subIdx 层） | — | 报告 §2 | ✅ |
| 当前时间 | `*(f32*)(T+0)` | — | 报告 §1 | ✅ |
| sub 表 | `count = *(i32*)(res+8)`；`table = *(u32*)(res+0x8C)`；项 **8B**：`+0` = 名字 `char*`、`+4` = 数据基址；**首指令 = `+4 + 0x10`** | — | 报告 §3.1/§3.2/§3.3 | ✅ |
| 解释器命中点（备选触发） | `0x4772D2`（6B EHOOK；`EBX` = 执行对象 T）；函数 `FUN_004772A0` @ `0x4772A0` | `0x4772D2`/`0x772D2` | 报告 §6 | ✅ |
| 既有 per-frame 钩子（本方案采样点） | `EHOOK_DY(th13_update, 0x470c04, 1, …)` | `0x470C04`/`0x70C04` | `thprac_th13.cpp:2189` | ✅（存在）；🟡（是否在 ECL tick **之后**，见 §7） |
| 一局边界钩子 | `EHOOK_DY(th13_game_start, 0x42D3D7, 7, …)` | `0x42D3D7`/`0x2D3D7` | `thprac_th13.cpp:2234` | ✅ |
| `STAGE_NUM` | `0x4BE81C` | `0x4BE81C`/`0xBE81C` | `thprac_th13.cpp:15` | ✅（既有）；🟡（Extra 是否报 9，见 §7） |
| `PLAYER_PTR` | `0x4C22C4` | `0x4C22C4`/`0xC22C4` | `thprac_th13.cpp:14` | ✅ |
| IGI 面板类 | `TH13InGameInfo`（`mMissCount`/`mBombCount`/`mTranceCount`） | `thprac_th13.cpp:577` | — | ✅ |

> **与 th15/th16（C 代）不可互搬**：C 代 `host+0xC` → thread、`thread+4` = **索引**；th13 是 `H+0x11D0` → thread、`T+4` = **指令绝对地址**。与 **th14 同形**（`host+0x11CC` = res、`host+0x11D0` = 线程链头、节点 `+4` = ip）。
> **`H+0x11CC` 是否每宿主独立 —— 已定案（2026-10-18，静态）**：**th13 与 th14 同法，所有宿主共用一个全局 res**。报告 §5.4.3（Q-C）：宿主构造器 `FUN_00418F40` 在 `0x4191E4`/`0x4191EB` **无条件**写 `H+0x11CC = *(u32*)(mgr+0xAC)`（对**每个**宿主成立，含 boss / 中boss）；`res+0x8C` 表由加载器 `FUN_004795B0` **跨文件累积**，关卡头 `ecli{}` 含 `stNNbs`（ST2–5/7 还含 `stNNmbs`）⇒ **boss sub 全在表内**。本方案按宿主读 `H+0x11CC` 的写法**正确**（与 `*(u32*)(mgr+0xAC)` 恒等）。⇒ **原「真机验证项 6」的前半（boss 宿主是否挂链 / res 是否含 boss sub）已降级为「按宿主链取 res」**；只余「boss 行读数是否与画面一致」待真机（§7 验证 6）。详见报告 §5.4。

---

## 3. Q2/Q3 —— 「当前 sub」与「偏移匹配」

### 3.1 当前 sub 的取法（必须遍历宿主线程链）

```text
mgr   = *(u32*)0x4C2188
node  = *(u32*)(mgr + 0xB0)                     // 宿主链头；节点 H+0x14CC = {+0 = H, +4 = next}
H     = *(u32*)node
ent   = H + 0x11D0                              // 线程链头（哨兵内嵌于宿主）
T     = *(u32*)ent                              // [+0] = 主线线程（= H+8）
next  = *(u32*)(ent + 4)                        // H+0x11D4 = 后继异步线程
res   = *(u32*)(H + 0x11CC)                     // 每宿主自己的 ECL 资源
count = *(i32*)(res + 8)
table = *(u32*)(res + 0x8C)
ip    = *(u32*)(T + 4)                          // 当前指令绝对地址；0 = 线程结束
```

- **必须逐线程读 `ip`**：th13 段入口常以 `async call`（opcode `0x0F`）派生为**独立线程**；**3 行的入口只经 `async call` 进入**（`ST1 后半 #2 = MainSub04b`、`ST4 前半 #1 = MainSub00h`、`ST5 前半 #1 = MainSub01`，入口报告 §3.3）。只读主线 T 会让这 3 行永不 latch。
- **必须遍历宿主链**：中boss / 关底 boss 的 ECL 跑在**各自的宿主**里（与主线宿主不同），只走主线宿主看不到 boss 行。`mgr+0xB0` 链覆盖全部宿主（同 th14 `mgr+0xD0` / th16 `mgr+0x180` 的角色）。

### 3.2 地址 → sub（区间反查）

本作**无 subIdx**，`ip` 是地址 ⇒ 必须用 `table[i].+4 + 0x10` 建**地址升序区间表**反查（报告 §2.3；表项按**名字字典序**存放，**地址不单调** ⇒ 必须显式 `std::sort`）：

```cpp
// SubRange{ start = table[i].+4 + 0x10, index = i }，按 start 升序
// ip 所属 sub = upper_bound 前一个 ⇒ (index, name = table[index].+0)
```

- 与 **th14 的 `BuildIpMap` / `NameOfIp` 逐字同形**（`thprac_th14.cpp:847-880`）⇒ 直接照搬，仅换链偏移。
- 越界保护：`ip == 0`（线程已结束）先判；`count ∈ (0, 8192]`；`ip` 落在最后一个区间之后时返回**最后一个 sub**（th14 同法；错名不会命中白名单 ⇒ 最坏 no-op）。
- 区间表**在 `(count, table)` 变化时重建**（`mIpMapCount`/`mIpMapTable` 只作**变化检测**，不解引用 ⇒ 不跨帧持指针，与 th14 的「不缓存任何指针」一致）。

### 3.3 偏移匹配：运行期如何判定「当前指令 == 该 sub 首指令 + 偏移」

**核心结论（✅ 已定案，有反汇编证据）**：**运行期能拿到该 sub 的首指令地址**——

| 事实 | 证据 |
| --- | --- |
| `table[i].+4` = **sub 数据基址**（= `buffer + offset[i]`） | 加载器 `FUN_004795B0` @ `0x479652`：`mov [eax + ecx*8 + 4], edx`（`edx = 缓冲 + offset[i]`）——报告 §3.2 |
| **首指令 = `table[i].+4 + 0x10`** | `FindSubByName` @ `0x47984B`：`mov eax, [edx + esi*8 + 4]` → **`add eax, 0x10`** → `ret`——报告 §3.3 |
| 指令头布局 `+0x10` 起 = 参数 ⇒ sub 前导 16 字节非指令 | 报告 §2.1（指令头 `+0x10` 起） |

⇒ **判据**（`i` = 该 sub 在**运行期 sub 表**的下标）：

```text
目标地址 = table[i].+4 + 0x10 + 偏移          // 偏移 = CSV 第 1 列 "+0x…" 的值
命中 ⟺ ip == 目标地址
```

**如何拿到 `i`（推荐做法，零额外结构）**：偏移行的 sub 名与「包含 `ip` 的那个 sub」**必然是同一个**（偏移 < 该 sub 长度 ⇒ 目标地址仍落在该 sub 区间内）。而 §3.2 的区间反查**已经返回了 `index`** ⇒ 直接复用：

```text
(idx, name) = SubOfIp(ip)                       // §3.2 的 upper_bound
for row in 本关偏移行 where row.sub == name:    // th13：仅 MainSub08 一行
    if ip == table[idx].+4 + 0x10 + row.off:  → 偏移命中，取 row.record（优先）
否则查 mNameMap[name]                           // 纯名命中
```

- **等价备选**：预解析成「目标地址 → 记录名」小表（在 `(stage, table)` 变化时重建），运行期只做 `ip == addr` 比较；th10 的 15 行更适合这一形态（见 §8）。
- **等价换算**：也可把偏移行折算成**文件偏移** `目标地址 − base`（`base = *(u32*)(res + 0xC + slot*4)`，章节行固定 slot 0 = `stNN.ecl`），再与 `off = ip − base` 比对——两者相差该 sub 的首指令基址，**判定等价**（报告 §3.2 入口小结 / th10 doc §3.2）。
- **🟡 待真机（本方案的核心不确定项）**：`ip` 在**帧边界**上是否**恰好等于**目标地址。这取决于 warp 落点指令的 `time > T+0`（「落点会等待」判据，th10 doc §2.3）：`t_land > at_frame` ⇒ ip 停在落点（可观测）；`t_land ≤ at_frame` ⇒ 落点被**跑穿**、帧边界观测不到。th13 的 `ST5 后半 #4` 的 warp 是 `ECLJump(ecl, 0x5154, 0x54d0, 0, 0)`（`at_frame = 0`）⇒ 需真机确认 `0x54d0` 处的指令 `time > 0`（否则要改用下一等待点或区间匹配）。**这一条与 th10 的 `ST4 前半 #2` 同类**（th10 doc §3.2）。

---

## 4. Q4 —— 数据表示与匹配结构（推荐）

**推荐：沿用 th14–th18 的形态（CSV 逐字节嵌入 + 运行期 `rapidcsv` 解析），不新增列、不产出结构化 C++ 表。**

| 决策 | 内容 | 理由 |
| --- | --- | --- |
| 数据来源 | `docs/logger/th13_checkpoints.csv` **原样**嵌入生成物（raw string） | CSV 是**唯一事实来源**；人可读、可 diff；与 th10–th13 其它作、与 th14–th18 五作完全一致 |
| 生成脚本 | 复制 `gen_th14_checkpoints.py`，只改 5 处：CSV 路径 / 输出路径 / `RAW_DELIM`（`TH13CSV`）/ 命名空间（`THPrac::TH13`）/ 符号名（`kTh13CheckpointsCsv`） | 生成物形态**零设计**；thprac 不加 MSBuild 预构建步骤 |
| 偏移行的解析时机 | **解析期**（`LoadCsv` / `BuildStageMap`）把第 1 列的 `+0x` 后缀拆成 `(sub 名, 偏移)`；运行期**不再解析字符串** | 避免每帧 `strtol`；CSV 文本本身保持既有记法（与 th10 一致） |
| 匹配结构 | ① `mNameMap`：`unordered_map<const char*, const char*>`（**纯名行**；键值别名 `mEntries` 的字符串，`mEntries` 冻结 ⇒ 指针终身有效、查表零分配）；② `mOffsetRows`：`vector<{ const char* sub; uint32_t off; const char* record; }>`（**偏移行**，按关过滤）；③ `mIpMap`：地址升序 `SubRange{start,index}`（§3.2） | 与 th14 同构，只多 ② 一个容器 |
| 是否需要 (subId, offset) 复合键 | **不需要单独的数据结构**：CSV 第 1 列的 `sub名+0x偏移` **天然就是复合键**（th10 doc §4.3 已定案），运行期以「包含 ip 的 sub 的 `index`」为 sub 分量、以 `ip − (table[i].+4+0x10)` 为偏移分量比对 | 免去新列 / 新表；消费方只多识别一个 `+0x` 后缀 |
| 为什么不产出结构化 C++ 表 | 会与**五作已落地**的实现分叉，需第二套生成路径；收益（省一次解析）在 116 行规模下可忽略 | 一致性 > 微优化 |

- **按关过滤必须做**：`MainSub00` / `Boss1` / `BossCard1` 等名字在 ST1..ST7 中大量重复，只按名字匹配会串关（th16 §5.4）。过滤时机 = **每关一次**（`STAGE_NUM` 变化时重建），不是每帧。
- **行解析**：跳过列数 < 4 的行、`第 2 列 != "th13"` 的行；解析失败 → 空表（恒显示 `main`，不抛到游戏线程外）。

---

## 5. Q5 —— th13 特有风险

### 5.1 `ST5 后半 #4`(`MainSub08+0x390`) 与 `#3`(`MainSub08`) 同 sub

- **现象**：`#3` 与 `#4` 同属 sub `MainSub08`（入口报告 §2.1）；`#4` 的 warp 把 `MainSub08` 的**第 2 条指令**（`0x5154`）改成 `goto 0x54d0`，落点 `0x54d0 = 0x5140 + 0x390`（`0x5140` = `MainSub08` 首指令）。
- **消歧**：**偏移命中优先于名字命中**（入口报告 §3.1）。判定顺序（§3.3）：先试偏移行，命中 `ip == firstInsn + 0x390` ⇒ 取 `#4`；未命中 ⇒ 回落纯名 `MainSub08` ⇒ 取 `#3`。
- **为何天然成立**：`ip` 落在 `MainSub08` 区间内 ⇒ 用**同一个 `index`** 计算目标地址，不存在「跨 sub 比较」的歧义；两行的 sub 分量相同、只有偏移分量不同。
- 🟡 真机：`#3` 的 `ip` 是否落在 `MainSub08` 首指令附近、且**不**等于 `+0x390`（验证项 4）。

### 5.2 另两处**同名容忍项**（照实记录，不新增偏移）

| 行 | 入口 | 情况 | 处置 |
| --- | --- | --- | --- |
| `ST4 前半 #3` / `ST4 后半 #1` | **同为 `MainSub00`** | 二者**无任何可区分信息**（既不同 sub、也无偏移后缀，入口报告 §2.2） | **不新增偏移**（无依据）；latch 容忍：命中 `MainSub00` 时置为后出现的那一行；真机确认显示行为（验证项 8） |
| `ST4 前半 #1` | `MainSub00h` | `!HL` 难度门控（`st04.decl` `MainFront` 首行 `!HL @MainSub00h() async;`）⇒ **Easy/Normal 下永不命中**（入口报告 §4 项 7） | latch 容忍（no-op，保持上一值）；真机在 E/N 下确认（验证项 9） |

### 5.3 async-only 3 行

`ST1 后半 #2`(`MainSub04b`) / `ST4 前半 #1`(`MainSub00h`) / `ST5 前半 #1`(`MainSub01`) 的入口**只经 `async call`（`0x0F` → `FUN_004793E0`）进入**，跑在 `H+0x11D4` 起的异步线程上 ⇒ 必须遍历线程链（§3.1）。**这是总括 §5.1 注记「th13 为同步 `call` 进入、无此问题」的订正**（入口报告 §3.3 已订正）。

---

## 6. Q6 —— 复用 th16 的已知坑（中boss 窗口）

**th13 同构（静态证据）**：`st01.decl:1270-1300` 的 `main()` 在 `@MainFront()` 之后 `enmCreateA("MBoss", 192.0f, -32.0f, 40, 1000, 1)` → `wait(160)` → **`@MainLatter()`**；`MainLatter`（`st01.decl:807`）首个子 = `MainSub04` = **`ST1 后半 #1`**。⇒ 中boss 出场后 ~160 帧，主线程就 latch `ST1 后半 #1`——与 th16 §5.7 的「增援章节」误报**同一模式**（th16 §5.7 静态根因 `st01.decl:1503-1543` 同形）。

**方案：移植 th16 §5.7 的中boss 窗口**（`thprac_th16.cpp:103-391` 的三个状态 + 分类 + 裁决）：

| 项 | 口径（沿用 th16，不改 CSV） |
| --- | --- |
| 命中分类 | 按**白名单键前缀**：`MBoss*` = 中boss、`Boss*` = 关底 boss、其余（`MainSub*`）= 章节 |
| 中boss 战结束 | 当前 sub 名命中**死亡 / 逃逸 sub 名前缀**（th13 离线枚举 = **`MBossDead` / `MBossEscape`**，见下） |
| 裁决 | `bossHit` → 关窗并清 pending；否则 `midHit && !mMidBossDone` → 开窗、显示中boss 记录；否则 `chapHit` → 窗口内**只存 `mPending` 不显示**，窗口外正常显示 |
| 关窗 | 死亡 sub 名一出现即关窗并**把 `mPending` 补显示出来**（取最后一个）；`mPending` 为空则维持中boss 记录 |
| 采样点 | `th13_update`(`0x470C04`) 每帧；同帧内「主线程进 `MainSub04`」与「`MBoss1` 命中」都可见，靠裁决优先级处理 |
| 新增钩子 | **无**。只复用现有 walk 已读到的 `name` 指针；`IsMidBossEndSub()` 只做 `strncmp` |

**th13 的中boss 死亡 sub 名（离线枚举，🟡 待实现期逐关复核）**：

- 依据 `../exes/ecl_dump/th13/*.decl` 的 `setBoss(-1)` 站点与 `setNext(1, 0, 1800, "MBossDead")` / `setTimeout(1, "MBossEscape")`（`st01.decl:411-412` 形态）⇒ 前缀 **`MBossDead` / `MBossEscape`**。
- **th13 无 `MBossB*` 变体**；`setDeath(...)` 的目标是 `Phantom01_at2` / `Phantom03_at2` 等（**无** th16 那种 `MBossCard*_Dead` 同名陷阱）——但**仍须精确前缀**并在实现期逐关复核（与 th16/14 同纪律）。
- ⚠️ **ST1 的中boss 脚本内联在 `st01.decl`**（**无 `st01mbs.decl`**）：`MBoss` / `MBoss1` / `MBossDead` / `MBossEscape` 都在 `st01.decl`。逐关枚举时**不要漏 ST1**（ST2–ST7 在 `stNNmbs.decl`）。

**与 th16 的差异 / 残余**：

- th13 白名单的中boss 行全是 `MBossN` / `MBossCardN`（容器 `MBoss` **未入表**）⇒ `MBoss()` 在 `@MBoss1()` 返回后 `goto MBoss_788: wait(1000)` 自循环时的当前 sub = `MBoss`（**非白名单**）⇒「迟到命中重开窗口」的风险**低于** th16。`mMidBossDone` 在 th13 是否**必需**待真机确认（无它也不会重开时，可省）；建议**先照搬三态**（结构一致、代价为零），真机若确认多余再删。
- 关底 boss 的窗口裁决顺序与 th16 相同（`bossHit` 强制关窗，兜底最坏只吞「中boss → 关底」一段）。

---

## 7. Q7 —— 工程登记与产物清单

> **仓库惯例**：一作一 cpp、**per-game 模块不单列 cpp**（见 `MEMORY` / th16 §10 的并入先例）⇒ **logger 模块写进 `thprac_th13.cpp` 内部**；只有**生成物**独立成 TU（AUTO-GENERATED）。

### 7.1 新增文件（2）

| 文件 | 内容 |
| --- | --- |
| `docs/logger/tools/gen_th13_checkpoints.py` | 由 `gen_th14_checkpoints.py` 复制；改 5 处：`CSV_PATH` → `docs/logger/th13_checkpoints.csv`、`OUT_PATH` → `thprac/src/thprac/thprac_th13_checkpoints_gen.cpp`、`RAW_DELIM = "TH13CSV"`、命名空间 `THPrac::TH13`、符号 `kTh13CheckpointsCsv`。幂等；断言正文不含终止符 `)TH13CSV"`；自校验逐字节一致 |
| `thprac/src/thprac/thprac_th13_checkpoints_gen.cpp` | **生成物（入库）**：`namespace THPrac { namespace TH13 { extern const char kTh13CheckpointsCsv[] = R"TH13CSV(…)TH13CSV"; } }` |

### 7.2 改动既有文件（2 类）

| 文件 | 改动 |
| --- | --- |
| `thprac/src/thprac/thprac_th13.cpp` | ① `enum addrs` 加 `ECL_MGR_PTR = 0x4c2188`；② 文件头补 `#include "..\3rdParties\rapidcsv\rapidcsv.h"` 与 `<algorithm>` / `<cstdio>` / `<cstring>` / `<sstream>` / `<string_view>` / `<unordered_map>`（当前只 include 了 `thprac_games.h` / `thprac_utils.h` / `<format>`）；③ 加 `extern const char kTh13CheckpointsCsv[];` 声明；④ 加 `TH13Logger` 模块（落点：`enum addrs` 之后、`class TH13InGameInfo`（`:577`）之前，与 th16 `:24-314` / th14 `:621-…` 同）；⑤ 4 处调用（见 7.3） |
| `thprac/thprac.vcxproj` / `thprac.vcxproj.filters` | 登记 `thprac_th13_checkpoints_gen.cpp`（各 +1 条 `ClCompile`；th13 无既有 `_gen` 文件，见 `.vcxproj:140` 一带） |

### 7.3 四个调用点（对齐 th16 §6.1 / th14）

| # | 位置 | 内容 |
| --- | --- | --- |
| a | `TH13InGameInfo::OnContentUpdate()`（`thprac_th13.cpp:619` 起） | 在**计数块内**插入「当前段落」标签 / 值两格：`ImGui::Text("当前段落");` + `ImGui::NextColumn();` + `ImGui::Text("%s", TH13Logger::singleton().CurrentSection());` + `ImGui::NextColumn();`，保持既有 `Columns(2)` 成对结构。**位置**：总括 §1.3 的口径是「miss/bomb 计数显示位的**下一行**」⇒ th13 建议**紧跟 bomb 计数行**（`mBombCount` 之后、trance 行之前）；若希望与其余计数行连续，也可放 trance 行之后（1 行位置的取舍，不影响功能）。**标签硬编码中文 `"当前段落"`，不动 `thprac_locale_def.*`** |
| b | `EHOOK_DY(th13_update, 0x470c04, 1, {…})`（`:2189`） | 在 `TH13InGameInfo::singleton().Update();`（`:2196`）之前加 `TH13Logger::singleton().Update();`（每帧采样） |
| c | `EHOOK_DY(th13_game_start, 0x42D3D7, 7, {…})`（`:2234`） | **必须写在既有 lambda 体内**（同址挂第二个钩子会覆盖第一个，见 th16 §6.1 引 `thprac_hook.cpp:223`）：加 `TH13Logger::singleton().OnStageStart();` |
| d | `THGuiCreate()`（`:2270` 起） | 在 `TH13InGameInfo::singleton();`（`:2284`）旁加 `TH13Logger::singleton();`（预构造，跑一次 `LoadCsv`） |

> **日志落盘的 4 个额外调用点**（`th13_life_dec` / `th13_bomb_dec` 既有 lambda 内的 `OnMiss`/`OnBomb`、`Update` 顶/末的 `Tick`/`Latch`）**不在本方案范围**（属日志落盘迭代）。本方案的 `TH13Logger` **只含** `OnStageStart()` / `Update()` / `CurrentSection()` 三个接口。

### 7.4 模块内部结构（本方案要做什么）

```text
class TH13Logger {                       // SINGLETON(TH13Logger)
    OnStageStart();                      // mSection = "main"；清中boss 三态；失效 mIpMap 变化检测
    Update();                            // 每帧：STAGE_NUM 过滤 → 遍历宿主链 × 线程链 → 双匹配 → 裁决 → latch
    CurrentSection();                    // 永不为 null（字面量或 mEntries 内）
private:
    LoadCsv();                           // rapidcsv 解析嵌入的 CSV；拆 `+0x` 后缀
    BuildStageMap(stageNum);             // 本关：mNameMap（纯名）+ mOffsetRows（偏移行）
    BuildIpMap(count, table);            // 地址升序 SubRange{start,index}（th14 同法）
    SubOfIp(table, ip) → (index, name);  // upper_bound（th14 的 NameOfIp 扩展为返回 index）
    mEntries / mNameMap / mOffsetRows / mIpMap / mSection / mPending / mMidBossActive / mMidBossDone / mBuiltStage / mIpMapCount / mIpMapTable
};
```

- **安全校验**（照 th14/th16）：`mgr` / `res` / `table` 逐级判空；`count ∈ (0, 8192]`（挡 `res` 被破坏）；遍历硬上限（宿主 ≤ 1024、线程 ≤ 64）防链成环；**不缓存任何指针**（每帧重走链）⇒ 无野指针，重置退化为「关卡切换时 `mSection ← "main"`」。
- **菜单 / 非关卡态**：`STAGE_NUM ∉ {1..7}` 直接 return，不改状态（注意 Extra 是否报 9，见验证项 11）。
- **退出一局**（`PLAYER_PTR == 0`）：**故意不重置**（需求要求保留最后 checkpoint；IGI 此时本就不绘制）。

---

## 8. Q8 —— 实施步骤与验证计划

### 8.1 任务清单（可执行）

| 步 | 任务 | 产出 / 判定 |
| --- | --- | --- |
| 1 | 写 `docs/logger/tools/gen_th13_checkpoints.py`（复制 th14 版改 5 处）并运行 | 生成物写出；脚本自校验「逐字节一致」通过 |
| 2 | 生成物入库 `thprac_th13_checkpoints_gen.cpp` | 含 `THPrac::TH13::kTh13CheckpointsCsv` |
| 3 | `thprac_th13.cpp` 加 `TH13Logger`（§7.4）+ 4 处调用（§7.3）+ 头文件 + `enum addrs` | 逐处对照本方案，无计划外改动 |
| 4 | 工程登记 `thprac.vcxproj` / `.filters` | 各 +1 条 |
| 5 | 构建 `Release\|x86` | `"D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe" thprac.sln -p:Configuration=Release -p:Platform=x86 -m`。**构建前先关掉正在运行的 `thprac.exe`**（否则 `LNK1104`）；注意生成物**必须**在 `THPrac::TH13` 命名空间内（否则 `LNK2001`，th16 §6.3 同坑） |
| 6 | 真机验证 | §8.2 的 13 项；未验证地址不得写成结论（AGENTS.md §2.5） |

### 8.2 真机验证用例（对齐 th16 §7/§7.1 + th13 入口报告 §4）

1. **宿主捕获**：`0x42C76B`/`0x42C90A` 命中时 `EAX` 非空；进关后 `H+0x11D0`、`H+0x11CC` 均可解引用，且 `H+0x11CC == *(u32*)(mgr+0xAC)`。
2. **latch 正确性（道中）**：ST1 从头打，F9 开 IGI。title 卡应为 `main`；随进度 `ST1 前半 #1 → 前半 #2 → 前半 #3 → 后半 #1 → 后半 #2`。**整段值只单调前进或不动，任何回退都是 bug**；段内非白名单 sub 运行期间值**必须不变**。
3. **async 三行（最需确认）**：warp 到 `ST1 后半 #2` / `ST4 前半 #1` / `ST5 前半 #1`，确认入口 `ip` 出现在 **`H+0x11D4` 起的异步线程**上（只读主线 T 会漏）。
4. **偏移消歧（本作唯一偏移行）**：warp 到 `ST5 后半 #4`，帧边界读到 `ip` 恰为 `table[MainSub08].+4 + 0x10 + 0x390`（=`0x54d0`）⇒ 显示 `ST5 后半 #4`；再 warp 到 `ST5 后半 #3`，确认读到 `MainSub08` 首指令附近而**不**命中 `+0x390`（显示 `ST5 后半 #3`）。**若 `0x54d0` 观测不到（落点被跑穿）⇒ 改用下一等待点或区间匹配（§3.3）。**
5. **中boss 窗口**：ST1 从头打。前半 `#1 → #2 → #3`；中boss 出场显示 `ST1 MID1`；**关键**：中boss 存活期间即便主线程已进 `MainSub04`，显示**必须保持 `ST1 MID1`**；`MBossDead` 一出现才变 `ST1 后半 #1`。再验**慢杀 flush**（中boss 存活 >20s → 关窗后直接跳 `后半 #2`）与**快杀 flush**（5s 内击破 → 关窗时无值可补，维持 `MID1`，待 `MainSub04` 命中后变 `后半 #1`）**两个分支都要跑**（同 th16 §7.1.2/3）。
6. **boss 行（🟡 已降级：锚点缺口已消解，只余读数核对）**：打 ST1 关底 boss → `ST1 BOSS1..BOSS6` 与符卡 / 非符切换同步；中boss 段 `ST1 MID1` 同步。
    - **前置静态结论（报告 §5.4，2026-10-18）**：宿主链 `mgr+0xB0` 是**全局全宿主链**（含 boss / 中boss），boss 宿主由 ECL `enmCreateA`（opcode `0x12D`）经同一宿主工厂 `FUN_004183F0` 另起，其 `H+0x11CC` = **同一个全局 `res`**，`res+0x8C` 表**含 `stNNbs`/`stNNmbs` 的 boss sub** ⇒ **本方案「遍历宿主链 × 线程链 + 地址反查」的取法对 boss 行成立，无需新增捕获点**。
    - **本条只验「读数与画面一致」**：boss 存活期间沿链能枚举到一个新宿主（`mgr+0xB8` 计数增加）且其 `T+4` 反查得 `Boss*`/`BossCard*`；`H+0x11CC` 与主线宿主**同值**。
    - **若仍不动** ⇒ 才回到「补 boss 宿主/槽表逆向」；此时可用备选捕获点 `0x41FE67`/RVA `0x1FE67`（5B，`EAX` = 新宿主；注意每次 `enmCreate*` 都命中且宿主数超限早退分支跳同址，须校验 `EAX != 0 && EAX != mgr`）作诊断锚点。
7. **关卡过滤**：打 ST2 不得出现 ST1 的记录名（同名 sub 大量重复，§4）。
8. **同名容忍**：`ST4 前半 #3` 与 `ST4 后半 #1`（同 `MainSub00`）实际停在哪个记录名（预期**不可区分**，§5.2）。
9. **`!HL` 门控**：E/N 难度下 `ST4 前半 #1`（`MainSub00h`）不命中（预期，容忍，§5.2）。
10. **生命周期回归**：ST1→ST2 连打 / Restart / 自杀 Continue / 回标题再进 / 直接退出，观察无崩溃、每关从 `main` 起算。
11. **`STAGE_NUM` 换算**：在 `OnContentUpdate` 里**临时**加诊断行（如 `ImGui::Text("DBG stage=%d", *(int32_t*)0x4BE81C);`），把 ST1..ST6 / Extra / 练习 warp / 菜单各记一次，确认 Extra 是否报 9；验完**删除**。
12. **采样时机**：确认 `th13_update`(`0x470C04`) 相对 ECL VM 的执行顺序（帧内 `goto`/`call` 是否一定在采样前执行完）——决定「精确匹配」是否够用（§3.3）。
13. **变体**：Steam / 汉化版的 `stNN.ecl` 是否与原版一致（偏移是**数据偏移**，脚本改版即失效）。

---

## 9. 风险与回退

| 风险 | 判定 | 缓解 / 回退 |
| --- | --- | --- |
| **boss 宿主不在 `mgr+0xB0` 链上**，或其 `H+0x11CC` 不含 boss sub ⇒ **boss 行永不 latch** | **已消解（2026-10-18 静态定案，报告 §5.4）** | 两条**均不成立**：① 宿主链是**全局全宿主链**——全程序唯一宿主分配点 `FUN_004183F0`（`push 0x52E4` 仅 `0x4183FC`）**无条件**入链（`0x418632`–`0x418652` / `0x41865D`–`0x41869B`），boss 宿主由 ECL `enmCreateA`（opcode `0x12D`）经**同一工厂**另起（`0x41B2EC` → `FUN_0041FCC0` → `call 0x4183F0` @ `0x41FE62`）⇒ **遍历链即得**；② boss 宿主 `H+0x11CC` = **同一个全局 `res`**（构造器 `FUN_00418F40` @ `0x4191EB` 无条件写 `*(u32*)(mgr+0xAC)`），`res+0x8C` 跨文件累积、含 `stNNbs`/`stNNmbs` 全部 sub（CSV 116 行与加载集比对 116/116 命中）⇒ **地址反查成立**。**⇒ 无需补 boss 宿主/槽表逆向、无需新增捕获点**（`logger_progress.md` §4 #11 的缺口在 th13 侧关闭）。残余 🟡 仅「读数与画面一致」，见验证 6 |
| 帧边界 `ip` ≠ `首指令 + 偏移`（落点被跑穿） | 验证 4 | 备用：**区间匹配**（`[目标, 下一锚点)`）；th13 仅 1 行偏移，先按**精确匹配** |
| `th13_update`(`0x470C04`) **不在 ECL tick 之后** ⇒ 同帧漏命中 | 验证 12 | 改挂 **`0x4772D2`**（6B EHOOK，`EBX` = T）作无条件每帧触发（报告 §6.3 的备选；但它会对所有脚本对象命中，须用捕获的 H 过滤） |
| 中boss 死亡 sub 名漏变体 ⇒ 窗口不关、吞掉本关后半 | 验证 5 | `bossHit`（关底命中）**强制关窗**兜底（最坏只吞「中boss → 关底」一段）；真机诊断行可直接看到真实 sub 名，据此补前缀 |
| `ST4 前半 #1` 的 `!HL` 门控 / `ST4` 两行同名 | 验证 8/9 | **口径内已知容忍**，不新增偏移（§5.2）；如真机确认影响体验，另行决策（不在本方案） |
| 英文 locale 下中文记录名渲染为空 | — | **已知固有局限**（`thprac_gui_locale.cpp` 的 EN glyph range 不含 CJK；非本改动引入，th16 §8 同） |
| `STAGE_NUM` 换算错（Extra） | 验证 11 | 内置「`9 → ST7`」（th16/th14 同法）；用临时诊断行实测 |

**回退粒度**：改动集中在「`thprac_th13.cpp` 内的 `TH13Logger` 模块 + 生成物 + 脚本 + 4 处调用 + 工程登记」⇒ 按提交整笔回退即可，不影响其它作（th14–th18 零改动）。

---

## 10. 对 th10–th13 通用性的判断（本方案为何把偏移当一等公民）

**th13 侧偏移匹配只有 1 个实例**（`MainSub08+0x390`）⇒ **单点验证**；通用性面向 **th10（15 行）** 与 **A 代（时间 / 指令偏移口径）**。th13 单点验证通过后，推广到 th10 还需补：

1. **一个 sub 多偏移行**：th10 `main` 在 st1 有 5 行偏移（`main+0x28`/`+0x2b0`/`+0x928`/`+0x1348`/`+0x1560`）⇒ 匹配结构须支持「同一 sub 名 → 多偏移」（本方案 §4 的 `mOffsetRows` 是 `vector`，**天然支持**）；优先级仍是**偏移命中 > 名字命中**。
2. **目标地址预解析**：15 行逐帧线性比较虽可接受，但建议把偏移行预解析成「目标地址 → 记录名」小表（§3.3 备选），运行期只做 `ip == addr`。
3. **落点被跑穿**（th13 **没有**的形态）：th10 `ST4 前半 #2`（`MainSub1+0x154`）的 warp 落点 `t_land ≤ at_frame` ⇒ 帧边界**观测不到**该地址（th10 doc §3.2）。需真机决定「精确匹配 vs 区间匹配」，或按 th10 doc §7 的**用户已定夺**（无视 1 帧窗口、按落点登记）。
4. **p1 锚点会被每次 warp 路过**：th10 的 `ST1/ST2/ST3 前半 #1` 锚点 = 该关**补丁位**（`main+0x28` 等），warp 到 p2..pN 时会被路过、但此处已被换成 `ecl_time=0` 的 `goto`（立即执行、不停留）⇒ 采不到。真机须确认「同帧内 `goto` 一定在采样前执行完」——**即本方案验证项 12**。
5. **boss 跳转撞章节锚点**：th10 有 **5 处** boss warp 落到章节锚点上（th10 doc §5.1）⇒ 中boss 窗口在 th10 是**必需**（不是可选），且 th10 的「中boss 存活」状态须从 **boss 宿主侧**采集（与 th13/th16 同构；**th13 侧的宿主可达性已于 2026-10-18 定案，见 `docs/th13/th13_ecl_re_report.md` §5.4**，th10 侧仍 🟡）。
6. **逐作差异须参数化**（本方案已把这几项做成常量）：主线宿主捕获点（th10 `0x41829E`/`0x41849E`、th13 `0x42C76B`/`0x42C90A`）、宿主链头（th10 `mgr+0x58`、th13 `mgr+0xB0`）、宿主→线程（th10 `H+0x1030`、th13 `H+0x11D0`）、**res 取法**（th10 doc §5.4：异步线程对象**无 res 槽** ⇒ 一律走**全局链** `*(u32*)(*(u32*)0x477704+0x54)`；**th13 已定案（报告 §5.4.3）**：按宿主 `H+0x11CC` 取即**恒等于** `*(u32*)(mgr+0xAC)`——**th13 是「所有宿主共用一个全局 res」**，与 th14 同法；th10 的「异步线程对象无 res 槽」差异**不适用于 th13**，但**仍须逐作确认**）。
7. **A 代（th06–th08）**：其 checkpoint「入口」是**时间值 / 指令偏移**而非 sub 名（各作 `_ecl_re_requests.md` §11）⇒ 本方案的「偏移匹配」骨架（`ip − 基址` 与「首指令 + 偏移」）可复用，但「**时间值**」匹配需要 A 代引擎的**时间轴锚点**（当前 ❌ 未逆向）⇒ **不在本方案覆盖范围**，需 A 代独立逆向后再设计。

---

## 11. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-18 | **boss 宿主可达性收口（静态定案）**。依 `docs/th13/th13_ecl_re_report.md` **§5.4（Q-A/Q-B/Q-C）** 改三处既有「boss 缺口」表述：① §9 风险 1 由「**boss 宿主不在 `mgr+0xB0` 链上** / 其 `H+0x11CC` 不含 boss sub ⇒ boss 行永不 latch ⇒ 需补 boss 宿主/槽表逆向」改为 **「已消解」**（宿主链为**全局全宿主链**：唯一分配点 `FUN_004183F0` 无条件入链，boss 宿主由 ECL `enmCreateA`(opcode `0x12D`) 经同一工厂另起；boss 宿主 `H+0x11CC` = 同一个全局 `res`，其表含 `stNNbs`/`stNNmbs` 全部 sub）⇒ **无需新增捕获点**；② §7 验证 6 由「最大 🟡 / 若不动则需补逆向」**降级**为「锚点缺口已消解，只验读数与画面一致」，并登记备选诊断锚点 `0x41FE67`/`0x1FE67`（5B，`EAX` = 新宿主；附两条告警：每次 `enmCreate*` 都命中、宿主数超限早退分支跳同址且 `EAX = mgr`）；③ §2 的「真机待确认：`H+0x11CC` 是否每宿主独立」改为 **「已定案：th13 与 th14 同法，所有宿主共用一个全局 res」**。另同步 §10 项 5（th13 侧已定案）与项 6（**res 取法：th13 按宿主 `H+0x11CC` 取即恒等于全局 res**）。**本方案其余内容未动**（未写代码、未跑编译、未真机）。 |
| 2026-10-18 | **建档**。给出 th13 监控（E5）实现方案（**只写方案，不写代码**）：① §2 汇总 th13 运行期锚点（VA/RVA 并列，全部引自 `th13_ecl_re_report.md` 的静态定案，标 🟡 未真机）；② §3 定「当前 sub」取法 = **th14 同形**（`mgr+0xB0` 宿主链 × `H+0x11D0` 线程链 → `ip = *(u32*)(T+4)` → 地址升序区间反查），并**订正**总括 §5.1「th13 为同步 `call` 进入」——3 行 async-only 必须遍历线程链；③ §3.3 定**偏移匹配判据** `ip == table[i].+4 + 0x10 + 偏移`（**✅ 首指令地址可得**，证据 = `FindSubByName`@`0x47984B` 的 `add eax,0x10`；**🟡 帧边界 ip 是否恰为该地址**待真机），并定**偏移命中优先于名字命中**；④ §4 定数据表示 = 沿用「CSV 逐字节嵌入 + 运行期 rapidcsv 解析」、`+0x` 后缀在解析期拆成 `(sub, 偏移)`、匹配结构 = `mNameMap` + `mOffsetRows` + `mIpMap`（**不新增列**，`sub名+0x偏移` 本身即复合键）；⑤ §5 记录 th13 特有风险（`MainSub08+0x390` vs `MainSub08` 消歧、`ST4 两行同名`、`MainSub00h` 的 `!HL` 门控）；⑥ §6 判定 th13 与 th16 §5.7 的「增援章节」**同构**（`st01.decl:1292-1295`），**须移植中boss 窗口**，中boss 死亡 sub 名 = `MBossDead`/`MBossEscape`（ST1 内联于 `st01.decl`，无 `st01mbs.decl`）；⑦ §7 产物清单（新增 2 文件 / 改 2 类，per-game 模块**写进 `thprac_th13.cpp`**）；⑧ §8 实施步骤 + 13 项真机用例；⑨ §9 风险表；⑩ §10 对 th10–th13 通用性的判断（th13 偏移匹配为**单点验证**，推广到 th10 的 7 项补充）。**未写任何代码、未跑编译、未真机。** |

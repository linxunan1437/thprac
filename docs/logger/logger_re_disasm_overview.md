# ECL 段落监控（读取 ECL 函数为手段）— 总括反编译需求文档

> 需求来源：`docs/req_logger.md`
> 本迭代范围：**仅监控**（**实时显示“当前段落”**；读 ECL sub 只是判定手段）；**不含日志记录**（JSON 落盘、checkpoint 统计、miss/bomb 列表）。
> 逐作需求：`docs/<作>/<作>_ecl_re_requests.md`（th10 ~ th18）。
> 反编译素材清单与来源：`docs/logger/decompile_materials.md`。
> 文档类型：**逆向需求文档（requests）**。§5 的“结论/状态”列由后续逆向迭代回填。
> 本机为 **Windows 11**（可编译、可真机运行；`docs/gametime/` 那套已在真机跑过）。`../exes/th10.exe ~ th18.exe`、`../exes/ecl/`（原始 `.ecl`）、`../exes/ecl_dump/`（`thecl -d` 反编译 `.decl`）已在本机，`capstone 5.0.7` + `pefile` 可用（供只读静态反汇编 / 离线 ECL 解码）。
> ⚠️ **A 代（th06–th08）没有 `.ecl`/`.decl` 素材，也没有 eclmap**（THTK `MAP/` 只有 `ECL_MAP_08.txt` 及以后）—— 见 §4、`docs/logger/decompile_materials.md`。

---

## 0. 范围与边界

| 项 | 本迭代 | 后续迭代 |
| --- | --- | --- |
| **实时显示当前段落**（读 ECL sub 为手段） | ✅ | |
| 白名单匹配（路过即置值，未命中 no-op） | ✅ | |
| IGI 显示（miss/bomb 计数下一行） | ✅ | |
| 读白名单 CSV 的“记录名”列 | 不消费（仅用“段落入口函数/游戏/关卡”做匹配） | ✅ |
| 每次启动/退出的 JSON 日志 | ❌ | ✅ |
| miss/bomb 事件列表、checkpoint 统计 | ❌ | ✅ |

> **最终目的**：实时显示**当前段落**。CSV 里每个 checkpoint 都**指向一个段落的开头**；“读取当前 ECL 函数(sub)”只是判定“现在到了哪个段落”的手段。日志迭代里的“启动/最后/经过的 checkpoint”全部是**段落级**。

- 覆盖作品：**th10 ~ th18**（9 作整数作，逆向进行中）。**th06–th08（A 代）已于 2026-10-16 完成 E0 建档**（`docs/th06|th07|th08/thXX_ecl_re_requests.md`），逆向待做；th09 需求明确不支持、th095 不在范围。
- 本机不能编译验证，故本文档只定义“需要逆向到什么 + 期望的产出格式”，**不写实现代码**。
- 所有地址一律按 **RVA = 绝对 VA − 0x400000** 表述；th10–18 均 `ImageBase = 0x400000` 且 `RELOCS_STRIPPED`（不重定位），故 RVA 与 VA 可直接换算。

---

## 1. 需求（监控部分）

### 1.1 目标

thprac 支持的整数作都有 ECL（ZUN 的弹幕脚本）引擎，它**顺序执行**脚本；函数模块层面不存在向后跳转（部分关卡存在路线分支，见 §4）。

**本迭代的最终产物是「当前段落」的实时显示**：

- CSV 里每个 checkpoint 都**指向一个段落的开头**（即“段落入口函数”）。
- 运行时读当前 ECL 函数(sub)，据此判定“现在处于哪个段落”。
- **读 sub 是为显示段落服务的手段**，不是目的。

### 1.2 由当前 sub 判定「当前段落」（路过式 checkpoint / latch）

1. 启动时加载白名单 CSV，按“当前游戏 + 当前关卡”过滤出本关的**段落入口函数**集合。
2. 运行时取“当前 ECL 函数名(sub)”。
3. **当前 sub ∈ 段落入口集合** → 把「当前段落」置为该 checkpoint（覆盖上一个值）。
4. **不在** → **no-op**：保持上一个 checkpoint 不变（**不回退 `main`**）。
5. 本局尚未命中任何 checkpoint 时，「当前段落」初始显示为 `main`（兜底值）。

> CSV 是**路过式 checkpoint 表**：「经过表中函数就置值，不在表内就 no-op」。因此**每个 checkpoint 只需登记 1 个「段落入口函数」**（= warp 跳转落点所进入的那个 sub，**含 `async` 子**）；**不需要**为段落内的每个 sub 都建条目。
> 原因：段内多个 `MainSubNN` 是 `MainFront`/`MainLatter` 的**兄弟 `call`**（前一个 `ret` 之后才调下一个），彼此**不构成祖先关系**；故“沿调用栈上溯”既非必需、也不可靠（详见 §6 Q9）。
> 白名单 CSV 的列：`段落入口函数`、`所属游戏`、`所属关卡`、`记录名`。本迭代只用前三列；`记录名` 供后续日志迭代。
> 判定口径已定案见 §6 Q9；`函数栈`（Q3）仍作逆向事实保留，但**不参与**本判定。

### 1.3 IGI 显示

- 位置：`THxxInGameInfo`（游戏内信息面板）里 **miss/bomb 计数显示位的下一行**。
- 时机：**仅当 miss/bomb 计数的显示功能开启时**，顺带显示**当前段落**。
- 形式：面板新增一行（占位符先用 ASCII，避免依赖自动生成的 locale 表；详见 §9 风险）。

### 1.4 与日志迭代的边界

本迭代不落盘、不做 JSON、不做 checkpoint 统计。CSV 的“记录名”列、以及“每次游玩一个 JSON 对象”等，全部推迟。

---

## 2. 术语与统一数据模型

| 术语 | 含义 |
| --- | --- |
| ECL 宿主（host） | 承载一段 ECL 脚本执行状态的对象（关卡主线脚本一个宿主）。 |
| ECL 线程（thread） | 宿主内的当前执行游标；保存“当前执行到哪个 sub / 哪条指令”。 |
| sub（函数） | ECL 里的子脚本（`call` 的目标）；运行期由**索引**标识，名字来自 sub 表。**读它是手段**。 |
| sub 表 | `索引 → 名字(char*) / 起始指令` 的映射表。 |
| 函数栈 | ECL 子调用（`call`）形成的调用链；**逆向事实，不参与**当前段落判定（见 §1.2 / §6 Q9）。 |
| **段落入口函数** | CSV 里每个 checkpoint 对应的那个 sub（= 段落开头的函数）。 |
| **checkpoint** | 一条“段落入口”记录：`段落入口函数 + 游戏 + 关卡 + 记录名`。日志迭代里“经过的 checkpoint”即经过的段落。 |
| **当前段落** | 最近经过的 checkpoint（按 §1.2：经过白名单函数时置值，未命中 no-op）。本迭代的**最终显示物**。 |

**参考实现模板（TH15，结构已由 `feat/plogth15` 分支验证）**：

```text
host    = 主线 ECL 宿主（spawn "main" 时捕获，见 §5）
thread  = *(u32*)(host + 0x0C)
subIdx  = *(i32*)(thread + 4)              // 当前 sub 索引
res     = *(u32*)(host + 0x11F8)           // ECL 资源对象
count   = *(i32*)(res + 8)                 // sub 总数（越界保护）
table   = *(u32*)(res + 0x8C)              // sub 表；每项 8 字节
name    = *(char**)(table + subIdx * 8)    // 项 +0 = 名字；项 +4 = 起始指令
```

> 推论（待各作验证，见 §6 Q2）：`res` 很可能就是各作 `THSectionPatch` 里 `ECLHelper::SetBaseAddr` 的那一级对象 —— `res + 0xC` 是 ECL 文件缓冲、`res + 0x8C` 是 sub 表。若成立，各代都能从 `SetBaseAddr` 的指针链拿到 sub 表。

### 2.1 与关卡跳转练习（Warp）的关系（重要）

练习模式的“关卡跳转”**本身就是靠 ECL 跳转实现的**，它和本监控是 ECL 的**写/读两面**，互为佐证：

- **Warp = 写**：`ECLHelper`（`SetBaseAddr(res)` + `SetFile(ordinal)`）定位运行期 ECL 缓冲，再在**硬编码的绝对偏移**处写入一条 `goto`（opcode `0x0C`）把执行重定向到目标段落入口，例如 `thprac_th14.cpp:1658` 的 `ECLJump`。这些偏移是**离线手工解码 `.ecl`** 得到的「段落入口/出口」地址。少数作用到“按名字调用 sub”的形态：`ThModern_ECLGetSub`（th185/th20）。
- **监测 = 读**：查询运行期“当前执行到哪个 sub”，映射为当前段落。
- 结论：Warp 的存在**证明运行期 ECL 缓冲（`res` 对象）可达**，正是 §6 Q2 要读的同一对象；而 Warp 用的那批段落入口偏移，可作为 §6 Q6 的**离线兜底资产**（运行期无名表时用于“偏移 ↔ 名字/段落”映射）。
- **粒度约定**：Warp 的粒度是“关卡 + 段落(portion)”，本监控的 checkpoint 同义；**监控以 sub 级读取、以段落级呈现**。每个 checkpoint 只登记**其 warp 落点那一个「段落入口函数」**（含 `async` 落点）；段内其余 sub 不在表内 → no-op（见 §1.2 / §6 Q9）。

---

## 3. 运行期读取流程（游戏侧，伪代码）

```cpp
// 每帧（或挂在 ECL 解释器上）：
host = GetMainEclHost();
const char* cur = nullptr;               // 当前 sub 名（手段）
if (host) {
    thread = *(u32*)(host + THREAD_OFF);
    subIdx = *(i32*)(thread + SUBIDX_OFF);
    res    = *(u32*)(host + RES_OFF);
    table  = *(u32*)(res + TABLE_OFF);
    cur    = *(char**)(table + subIdx * 8);
}
// 由当前 sub 判定「当前段落」（目的，路过式 checkpoint / latch）：
const char* section = LookupCheckpoint(game, stage, cur);   // 当前 sub ∈ 白名单？
if (section) cache = section;                               // 命中 → 置值
                                                            // 未命中 → no-op（保持 cache，不回退 main）
                                                            // cache 初值为 "main"
```

---

## 4. 引擎分代与同构策略

| 代 | 作品 | 参照作 | sub 表/索引形态 |
| --- | --- | --- | --- |
| A | th06–th09, th095 | th06 | 第一代引擎；**th06–th08 已 E0 建档**（2026-10-16），逆向待做。**章节跳转＝时间轴**（非 sub 跳转），见下注 |
| **B** | th10, th11, th12, th13, th125, th128 | **th10 / th11**（已有 ECL 解释器命中点） | 执行对象 + 时间字段 + 主线判据已知；sub 表/名字待定 |
| **C** | th14, th15, th16, th17, th18, th143, th165, th185 | **th15**（sub 名读取已完整） | “宿主→线程→subIdx→sub 表”模板成立 |
| D | th19, th20 | — | 会重定位，需 base+RVA，本轮不做 |

> **A 代（th06–th08）的特殊性**：与 B/C 代不同，A 代的章节跳转是往引擎里写**时间值**，不是 `ECLJump` 跳到具名 sub —— th06 `ECLWarp(time)`→`0x5a5fb0`、th07 `ECLTimeWarp(count,time)`→`[0x9a9af8+0x9545fc]`、th08 `ECLWarp(time,offset,…)`→`0xf54cf8`（Timeline 结构 `0xF54CF0`）。因此 A 代各作 requests 的 **Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一**，checkpoint「入口」列改用**时间值 / 指令偏移**。另：A 代 ECL 格式里**确实有带 id 的 sub**（th07 `ECLCallSub` opcode `0x29`、th08 opcode `0x34`、th06 有未接线的 `EclRawInstrCallArgs.eclSub`），运行时是否暴露「当前 sub」是待逆向项。

> **B 代细分建议（2026-10-16，由 th12 报告提出；待补证，本文暂不改上表）**：th12 与 th13 的 ECL VM 骨架**逐字段同源**（帧长 `0x0C`、`T+0x1008`/`+0x1014`/`+0x101C`、`0x0A/0x0B/0x0C` = ret/call/goto、sub 表布局一致），仅**宿主对象层偏移分叉**（th12 `H+0x1030`/`H+0x102C`/宿主 `0x27B8` ↔ th13 `H+0x11D0`/`H+0x11CC`/`0x52E4`）；且 th12 全域**不存在** th10/th11 主线判据所用的 `+0x103C+0x14XX` 形态。故建议把 B 代细分为 **B1 = th12/th13**、**B2 = th10/th11**。**不足**：th10 帧长 `0x08`、th12/th13 `0x0C`，而 **th11 的帧长/遍历步长本轮未定案**，该细分线被 th11 悬空，尚不足以定案。

**同构搬运**：先在参照作把 Q1–Q4 摸透，再在同代内按“相同对象形态 + 相对偏移”搬运，逐作验证。

**分支关**（需求文档点名）：th11 st4、th14 st4、th16 st5 存在剧情/路线分支；**A 代三作（th06 / th07 / th08）的 st4 也均有路线分支**。因为监控读的是**运行期状态**（当前执行到哪个 sub/位置 → 当前段落），而非静态时间轴，理论上天然兼容分支；但需在真机验证（§6 Q8/Q9 / 各作验证清单）。

---

## 5. 逐作锚点总表（活文档）

> 状态图例：✅ 已确认（可直接用）｜🟡 有线索待定｜❌ 未知/待逆向。
> 本表由后续逆向迭代回填。

### 5.1 引擎 / 逆向锚点

| 作 | 代 | 二进制 / ECL 与 decl（../exes） | 已有 ECL 解释器命中点（寄存器） | 命中点已知字段 | ECL 数据区链（THSectionPatch `SetBaseAddr` 来源） | sub 名/索引线索 |
| --- | --- | --- | --- | --- | --- | --- |
| th06 | A | th06.exe 512000B ts=1038721275（**无 ecl/decl**） | 时间轴 `0x41188A`/`0x411669`(各 3B)；冻结 `0x412e31`(5B) | 命中时从 `pCtx->Ebp-0x14`/`-0xC` 取**当前指令指针** | `SetBaseAddr(0x487e50)` | ❌ 无 eclmap；`EclRawInstrCallArgs.eclSub`（未接线） |
| th07 | A | th07.exe 650752B ts=1066942337（**无 ecl/decl**） | 冻结 `0x4207a1`(5B)；opcode 分支 `0x42D17E`/`0x414FE6`/`0x413D06`/`0x41FFD5` | — | `SetBaseAddr(GetMemAddr(0x1347938))` | ❌ 无 eclmap；`ECLCallSub` opcode `0x29` |
| th08 | A | th08.exe 840704B ts=1095610188（**无 ecl/decl**） | ❌ 无冻结 hook；opcode 分支 `0x437AF3`/`0x41CB32`/`0x41C44F`/`0x42F34D` | Timeline 结构 `0xF54CF0`；ECL Buffer `0x4ECCB8` | `SetBaseAddr(GetMemAddr(0x4ECCB8))` | ❌ 无 eclmap；`ECLCallSub` opcode `0x34` |
| th10 | B | th10.exe 487936B ts=1186086096 | `0x44fb9f`(2B) `Esi`〔**更正 2026-10-16**：是「帧时间累加并返回」的**收尾句**，非 sub 写入点〕 | `*(f32)Esi`=当前时间；**`*(u32*)(T+4)`=当前指令绝对地址（指针语义，同 th13）**；主线 `*(u32*)(H+0x1480)==1296`（= 旧写法 `[Esi+0x1014]+0x103c+0x1444` 的展开；`T+0x1014`=**宿主 H 自指针**，非“敌人 flags”） | `GetMemAddr(0x477704, 0x54, 0xC)`（= `res+0xC`，**原值正确**） | ✅ `res+0x8C` 表（项 8B）〔旧值 ❌〕 |
| th11 | B | th11.exe 688128B ts=1217734394 | `0x45d2a3`(2B) `Edi`〔**更正**：`Edi`=**线程 T**（非宿主）；分发头 `0x45B890`、解释器入口 `0x45B860`〕 | `*(f32)Edi`=当前时间；**`*(u32*)(T+4)`=当前指令绝对地址**；主线 `*(u32*)(H+0x25BC)==20512`（同上展开） | `GetMemAddr(0x4a8d7c, 0x64, 0xC)`（**原值正确**） | ✅ `res+0x8C` 表〔旧值 ❌〕 |
| th12 | B | th12.exe 737280B ts=1251344348 | **`0x4671A0`(6B) `EDI`**（2026-10-16 **自找**；主解释器 `FUN_00467170`，跳表 `0x468C38`／映射 `0x468D30`／上界 `0x59`）〔旧值 ❌〕 | **`*(u32*)(T+4)`=当前指令绝对地址**；`T+0`=f32 时间 | `GetMemAddr(0x4b43dc, 0x64, 0xC)`（= `res+0xC`） | ✅ `res+0x8C` 表〔旧值 ❌〕 |
| th13 | B | th13.exe 790528B ts=1313589413 | **`0x4772D2`(6B) `EBX`**〔**同步 th13 报告**（其 §6）；旧值 ❌〕 | `*(u32*)(T+4)`=当前指令绝对地址；跳表 `0x478C38`／映射 `0x478D44` | `GetMemAddr(0x4c2188, 0xac, 0xC)` | ✅ `res+0x8C` 表〔旧值 ❌〕 |
| th14 | C | th14.exe 897536B ts=1376634201 | `0x483c4f`(4B) `Edi` | `[Edi+4]`（语义待定）；sub 表 `[ENEMY_MGR(0x4db544)+0xcc]+0x8c`，项 8B，`+4`=起始 | `GetMemAddr(0x4db544, 0xcc, 0xC)`（与 sub 表同源） | 🟡 有 sub 表，有名字待确认 |
| **th15** | C | th15.exe 951808B ts=1444028071 | 宿主捕获 `0x43bbc8`/`0x43d48e`/`0x43d6be`（`EAX`=宿主） | **完整**：`host+0xC→thread+4=subIdx`；`host+0x11F8=res`；`res+0x8C`=sub 表 | `GetMemAddr(0x4e9a80, 0x17c, 0xC)` | ✅ 模板 |
| th16 | C | th16.exe 683520B ts=1501534029 | `0x473995`(4B) `Edi` | `[Edi+4]`=subID | `GetMemAddr(0x4a6dc0, 0x17c, 0xC)` | 🟡 有 subID，名字表待定 |
| th17 | C | th17.exe 747520B ts=1565429804 | `0x47a712`(4B) `Edi` | `[Edi+4]`=subID | `GetMemAddr(0x4b76a0, 0x17c, 0xC)` | 🟡 有 subID，名字表待定 |
| th18 | C | th18.exe 847360B ts=1618619144 | `0x48c822`(4B) `Edi` | `[Edi+4]`=subID | `GetMemAddr(0x4cf2d0, 0x188, 0xc)` | 🟡 有 subID，名字表待定 |

> 二进制同一性：以上 ts 与 `thprac/src/thprac/thprac_launcher_games_def.h` 的识别表一致；均 `ImageBase=0x400000`、`RELOCS_STRIPPED`。
> **ECL / decl 素材**：th10–th18 的原始 `.ecl`（`../exes/ecl/thXX/`）与 `thecl -d` 反编译 `.decl`（`../exes/ecl_dump/thXX/`）均已在机（各作 8/20/9/21/24/23/22/22/22，共 171 份，dump 171/171 成功），可离线枚举各关段落入口 sub；清单与来源见 `docs/logger/decompile_materials.md`。
> **A 代（th06–th08）例外**：**无 eclmap**（THTK `MAP/` 只有 `ECL_MAP_08.txt` 及以后），`.ecl` 未从 `.dat` 解出，`reasm/exes/` 缺 `th06.exe` ⇒ **离线具名枚举这条路对 A 代不通**；checkpoint「入口」列口径改用**时间值 / 指令偏移**。详见 `docs/th06|th07|th08/thXX_ecl_re_requests.md` §12。
> **「`[执行对象+4]` 语义」已裁定（2026-10-16，按代分叉）**：**B 代（th10 / th11 / th12 / th13）= 当前指令的绝对地址（指针）**，`*(u32*)(T+4)`，值 `0` = 线程结束；**C 代（th15–th18）= sub 索引**（th15 实证 `thread+4`）。两者**不可互搬**：B 代由地址取“当前 sub 名”须先用 `table[i].+4+0x10` 建**区间表**反查（见各作报告 §Q1/Q6）。th14 仍按“ip 区间比较”写在 C 代内，与 th15–th18 口径不一致 —— **该残留项未在本轮范围，保留待查**（见 §9.4）。
> **B 代取「当前 sub」须遍历宿主线程链**（th10 / th11 / th12 三作独立得出同向结论，2026-10-16）：段落入口常以 `@MainSubNN() async` 派生为**独立线程**，主线线程 `T+4` 可能**长期停在 `main`**（th12 尤甚），且 `H+4`（当前线程）会被本帧最后执行过的 async 线程覆盖 ⇒ 应遍历 `H+0x1030` 线程链（节点 `{+0=线程,+4=next,+8=prev}`）**逐线程**读 `T+4`，任一命中白名单即置值。th13 为同步 `call` 进入，无此问题。
> **B 代主线宿主捕获点**（本轮新增，命中寄存器均为 `EAX` = 宿主 H）：th10 `0x41829E` / `0x41849E`(6B)、th11 `0x4204A3` / `0x4205E0`(5B)、th12 `0x4228BE` / `0x4229D2`(5B)、th13 `0x42C76B` / `0x42C90A`。静态兜底：`H = *(u32*)(*(u32*)(mgr+<链头偏移>))`（th10 `+0x58`、th11/th12 `+0x68`、th13 `+0xB0`）。
> **读到的 sub → 当前段落**：本表各作的 sub/索引结论，需再经白名单映射为“段落入口”，最终显示**当前段落**（见 §1.2、§2.1、§6 Q9）。

### 5.2 接线锚点（miss/bomb / 一局边界 / IGI）

> 供“IGI 显示下一行”与后续日志迭代复用。所有地址为 `.text` 静态地址（RVA=VA−0x400000）。

| 作 | IGI 类名 | miss 记录点 | bomb 记录点 | 一局边界（game_start） |
| --- | --- | --- | --- | --- |
| th10 | `TH10InGameInfo`（`thprac_th10.cpp:567`） | `th10_life_dec` `0x426a1c`(6B) | `th10_bomb_dec` `0x4259cf`(5B) / `th10_bomb_dec2` `0x425c3e`(5B) | `th10_game_start` `0x41798c`(6B) |
| th11 | `TH11InGameInfo`（`:636`） | `th11_life_dec` `0x4327f0`(6B) | `th11_bomb_dec` `0x4311e6`(5B) / `th11_bomb_dec2`(dead bomb) `0x431293`(5B) | `th11_game_start` `0x41fa6d`(3B) |
| th12 | `TH12InGameInfo`（`:604`） | `th12_life_dec` `0x4381e2`(5B) | `th12_bomb_dec` `0x422f28`(5B) | `th12_game_start` `0x421def`(6B) |
| th13 | `TH13InGameInfo`（`:577`） | `th13_life_dec` `0x444a75`(10B) | `th13_bomb_dec` `0x40a404`(5B) | `th13_game_start` `0x42d3d7`(7B) |
| th14 | `TH14InGameInfo`（`:614`） | `th14_life_dec` `0x44f618`(5B) | `th14_bomb_dec` `0x41218a`(5B) | `th14_game_start` `0x4375be`(7B) |
| th15 | `TH15InGameInfo`（`:598`） | `th15_life_dec` `0x456398`(5B) | `th15_bomb_dec` `0x41497a`(5B) | `th15_game_start` `0x43e6ee`(7B) |
| th16 | `TH16InGameInfo`（`:551`） | `th16_life_dec` `0x443d3a`(5B) | `th16_bomb_dec` `0x40db9c`(5B) | `th16_game_start` `0x42e5ae`(7B) |
| th17 | `TH17InGameInfo`（`:796`） | `th17_life_dec` `0x44921b`(6B) | `th17_bomb_dec` `0x411cab`(6B) | `th17_game_start` `0x4302e6`(6B) |
| th18 | `TH18InGameInfo`（`:1221`） | `th18_life_dec` `0x45d1a3`(5B) | `th18_bomb_dec` `0x4574d3`(4B) | `th18_game_start` `0x44278f`(5B) |

其它可复用指针（各作 `thprac_thXX.cpp` 顶部 enum）见各作文档 §1。

### 5.3 boss 段（方案二）逐作锚点

> **方案二**（与道中同机制）：读 **boss 宿主**里 ECL 引擎的**当前 sub 名** → 白名单 latch。boss ECL 跑在 **boss 敌人自己的宿主**里、**不在主线宿主**，故须**先取 boss 宿主**。逐作报告：`docs/th14/th14_boss_phase_report.md`、`docs/th15/th15_boss_phase_report.md`、`docs/th16/th16_boss_phase_report.md`、`docs/th17/th17_boss_phase_report.md`、`docs/th18/th18_boss_phase_report.md`。
> 统一读取链（th15–18）：`thread=*(host+0xC)` → `subIdx=*(thread+4)` → `res=*(host+0x11F8)` → `table=*(res+0x8C)` → `name=*(char**)(table+subIdx*8)`；**th14 无 sub 索引**（`node+4`=绝对 ip，需 ip→sub 区间映射）。

| 作 | 管理器全局（VA） | 槽表（存 id / 存宿主指针） | 宿主/对象链表头（`mgr+`） | 按槽取 host | 按 id 取 host | host id 字段 | boss 门控位 | 状态 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| th14 | `0x4DB544` | `+0x5C+slot*4`（**直接存宿主指针**） | `+0xD0`（双向链） | `FUN_0041C050` | `FUN_00421AA0` | 对象 `+0x11E4` | `host+0x5244` bit23 | 🟡 |
| th15 | `0x4E9A80` | `+0x48+slot*4`（存 **id**） | `+0x180` | `FUN_0041F7F0` | `FUN_004255E0` | `+0x5740` | `host+0x526C` bit23 | 🟡 |
| th16 | `0x4A6DC0` | `+0x48+slot*4`（存 id） | `+0x180` | `FUN_00417580` | `FUN_0041A9C0` | `+0x5740` | `host+0x526C` bit23 | 🟡 |
| th17 | `0x4B76A0` | `+0x48+slot*4`（存 id） | `+0x180` | `FUN_0041A5C0` | `FUN_0041DDD0` | `+0x5760` | `host+0x528C` bit23 | 🟡 |
| th18 | `0x4CF2D0` | `+0x48+slot*4`（存 id） | `+0x18C` | `FUN_004237F0` | `FUN_0042D500` | `+0x6830` | `host+0x635C` bit23 | 🟡 |

> 同代差异：th17/th18 宿主对象较 th15/th16 长大（th17 各字段 +0x20；th18 更大幅度换布局），故 id / 门控 / `B` 自指针偏移各异；**th14 槽表直接存宿主指针**（无需 id 匹配），且**无 sub 索引**（读 ip 再映射）。boss 对象 `B = host + …`（th15/16 `+0x120C`、th17 `+0x120C`、th18 `+0x122C`），`*(B+…)` = host 自指针（th15/16 `B+0x44B8`、th17 `B+0x44D8`、th18 `B+0x5588`）。
> 已同步回填各作 `docs/logger/thXX_checkpoints.csv` 的 boss 行（第 1 列）：th14 64/72、th15 10、th17 63、th18 65（🟡 静态推导、未真机；细节见各作报告 §5）。

---

## 6. 通用逆向问题 Q1–Q9

> 每作文档只标注“已答/待答/不适用”，答案格式统一如下。
> 注意：**Q1–Q6 是“读 subs”这一手段**；**Q9 才是决定“显示当前段落”能否成立的判定口径**。

### Q1 当前 sub：索引还是指令指针？
- 期望产出：完整表达式（如 `*(i32*)(*(u32*)(host+0x0C)+4)`）+ 类型 + 取值范围（0..count-1？）。
- 证据：写入点 + 解释器内被谁读。

### Q2 sub 表：索引 → 名字 / 起始指令
- 期望产出：完整指针链到 sub 表 + 表项大小与字段偏移（`+0`=名字 `char*`？`+4`=起始？）+ 名字串形态。
- 证据：构造/加载 ECL 时填表的代码 + 按名查表的代码（如 TH15 的 `FUN_0048f340`）。

### Q3 函数栈（子调用链）
- 期望产出：是否存在显式调用栈（栈顶=当前 sub）？表达式/遍历规则 + 深度上限。
- 说明：**Q9 定案为路过式 checkpoint（latch）后，本项不再是判定前端**；仅作逆向事实记录（各作已确认存在可遍历子调用栈）。

### Q4 主线宿主捕获点
- 期望产出：进关/spawn“main”时把宿主对象写入的 hook 点（地址 + 命中寄存器）。
- TH15 参考：`0x43bbc8` / `0x43d48e` / `0x43d6be`，命中时 `EAX`=宿主。

### Q5 解释器命中点与寄存器语义
- 期望产出：地址 / EHOOK 覆盖长度 / 命中时哪个寄存器=执行对象（`Esi`/`Edi`）及其字段。
- 用途：作为每帧读“当前 sub”的稳定触发点（避免自己找每帧主循环）。
- 已有：见 §5.1；th12/th13 **完全没有**，需从零找。

### Q6 名字来源
- 期望产出：运行期，sub 表项里是否保留**名字字符串**？
  - 若“保留”→ 直接读 `char*`。
  - 若“不保留”（只有索引/偏移）→ 需要**离线**“名字↔索引/偏移”表（依赖白名单 CSV 与各作 `.ecl` 脚本）。

### Q7 机体 / 子机类型地址（可选，供 IGI/后续日志）
- 期望产出：`character` / `shotType` 地址（每关创建、可选）。th14 已知 `CHARA_ADDR=0x4f5828`、`SUBSHOT_ADDR=0x4f582c`。

### Q8 练习跳转补丁（`ECLJump`）对读取的影响
- 说明：thprac 练习模式用 `ECLJump` 直接改 ECL 字节跳转到段落入口。
- 期望产出：该补丁是否改变 sub 索引/名字判读（例如跳转后的 sub 归属是否仍正确）。

### Q9 「当前段落」的判定口径（**决定显示能否成立**）
- **已定案：路过式 checkpoint（latch）**。CSV 即白名单表：当前 sub ∈ 表 → 置「当前段落」为该 checkpoint；**不在 → no-op**（保持上一个值，**不回退 `main`**）；本局首次命中前显示 `main`。
- 口径早期曾在两条路之间摇摆，**保留备查**：
  - **(A) 调用栈祖先法**：假设段落入口 sub 通过 `call` 进入并**驻留栈上**，整段期间都可沿函数栈上溯。
  - **(B) 顺序 latch 法**：段落入口只是“经过”的点，需**记住最近通过的 checkpoint**。
- **为何不采用 (A)**：离线实测（th15 `st01.ecl`，th16/17/18 同构）表明，段内多个 `MainSubNN` 是 `MainFront`/`MainLatter` 的**兄弟 `call`**——前一个 `ret` 后才调下一个，彼此**不互为祖先**。「驻留栈上」只对**单个** `MainSubNN` 成立，对**整个段落**不成立，故 (A) 既非必需也不可靠。（注：Q3 的“显式调用栈存在”是**事实**，只是不用于本判定。）
- 期望产出：每个 checkpoint 登记 **1 个「段落入口函数」** = warp（`ECLJump`）跳转落点所进入的那个 sub（**含 `async` 落点**）；名字直接取自 Q2 的 sub 表（落点偏移 ↔ sub 由离线 `.ecl`/`.decl` 对应）。

---

## 7. 逆向方法论与工具

1. **PE 解析 + 只读反汇编**（与既有 `docs/thXX/thXX_re_disasm_report.md` 同路子）：`pefile` 取指纹/节表 → `va2off` → `capstone`（`CS_ARCH_X86`, `CS_MODE_32`）。
2. **字符串锚点**：先用已知 `"Pause"`/`"UnPause"`/`"replay/%s"` 等 trace 串定位代码区；ECL 侧可用已知 hook 地址反查解释器函数入口。
3. **从已知 hook 反查**：§5.1 的 ECL 命中点已在 thprac 里，直接反汇编其所在函数，确定执行对象寄存器与字段语义。
4. **同构搬运**：同代参照作先摸透，再逐作确认基址与偏移。
5. **真机三态/多态验证（不可省，但本机无法做）**：在真机上对“标题/道中/boss/不同章节/分支关”分别采样，确认读到的名字与实际一致；未验证的地址不得写进结论。
6. **坑**：`.text` 线性反汇编会因内嵌数据/跳表失步；应在 `int3` 边界后的函数入口处重解码，或做“原始字节搜索 + 局部对齐”。

---

## 8. 交付物与验收

- 逐作结论回填到 `docs/<作>/<作>_ecl_re_requests.md`，并同步 §5 总表。
- 每作需给出：Q1–Q6 的**地址/指针链 + 判据 + 证据（反汇编片段）**，并回答 **Q9（段落判定口径）**，Q7/Q8 视需要。
- 验收：任一作的“**当前段落**”判定，在真机上对同一关卡的不同段落采样，显示出正确段落名并随进度推进（或与已知段落序一致），分支关亦成立。

---

## 9. 风险与开放问题

1. **段落判定口径**：已定案为「路过式 checkpoint（latch）」——经过白名单函数即置值，不在表内 no-op。~~需求原文默认 A~~：(A) 假设“入口 sub 整段驻留栈上”，与段内兄弟 `call` 结构不符（见 §6 Q9）。
2. **函数栈**：Q3 已确认各作存在可遍历的子调用栈，但**不参与**本判定；仅作逆向事实与后续参考保留。
3. **运行期是否保留名字 —— 已定案（2026-10-16）**：**th10 / th11 / th12 / th13 均保留**（`table[i].+0` 指向加载缓冲内的名字串）。但因 B 代 Q1 是**地址语义**，由地址取“当前 sub 名”仍需用 `table[i].+4+0x10` 建**区间表**反查（Q6）。各作 `.ecl`/`.decl` 素材已在机，该映射可离线建立（`docs/logger/decompile_materials.md`）。
4. **`[执行对象+4]` 语义 —— 已裁定（2026-10-16）**：**B 代（th10–13）= 当前指令绝对地址**、**C 代（th15–18）= sub 索引**（详见 §5.1 注记）。**残留**：th14 属 C 代却按“ip 区间比较”，与 th15–18 口径不一致，未在本轮范围，保留待查（Q1）。
5. **~~th12 / th13 无现成 ECL 命中点~~ 已消解（2026-10-16）**：th12 命中点**自找定案 `0x4671A0`**(6B, `EDI` = 执行对象 T)，th13 报告已有 `0x4772D2`(6B, `EBX`)。两作均沿「SCPT 魔数 `0x54504353` → 加载器 → `res`/sub 表」+「字串 `"main"` → 宿主工厂 → 捕获点」+「主解释器跳表」三线交叉定位，路径已写入各作报告。
6. **locale 文案**：`thprac_locale_def.h/.cpp` 是由外部 devtools 生成的，本地无法重新生成；IGI 新增行优先复用既有词条或使用 ASCII 占位，避免动 locale 表。
7. **分支关**：th11 st4 / th14 st4 / th16 st5，须真机确认（Q8/Q9）。
8. **地区/版本变体**：Steam / 汉化版是否共用同一 RVA，需按变体实测。
9. **B 代 th10 的「段落入口」粒度可能不够（2026-10-16，待 E3/E4 决策）**：把 `thprac_th10.cpp` `THStageWarp` 的硬编码偏移离线映射回 sub 后，**st1–st3 的段落入口全部落在 `main` 内部**（`.decl` 佐证：`stage01` 无 `MainSubNN`，`main` 是覆盖全关的 ~0x1EB4 B 超长脚本，道中为内联区段），仅 st4–st7 多为独立 `MainSubNN`。若成立，`§1.2`「每个 checkpoint 只登记 1 个段落入口函数」对 th10 st1–st3 **不适用**，或需改登记 **`(sub 名, 指令偏移)` 二级粒度**（`THStageWarp` 的偏移即现成资产）。参照：th12 已抽样 8/8 命中且入口是 `main` 内 `0x0F` 指令的**内嵌名** ⇒ 仍属 sub 名粒度；**th11 需同法复核**。**本文 §1.2 暂不改，待决策。**

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档。范围＝监控（第一迭代）；确定总括 + 逐作（th10–18）的 requests 结构；汇总既有 ECL 解释器命中点、ECL 数据区链、miss/bomb 钩子、IGI 类名；定义通用问题 Q1–Q8。未跑逆向。 |
| 2026-10-14 | **口径修正为目标＝实时显示「当前段落」**：checkpoint 定义为“段落开头对应的白名单函数（段落入口函数）”，读 ECL sub 降为手段。§0 表格、§1、§2 术语、§3 伪代码、§5.1 注记同步改写；新增 §2.1“与 Warp 的关系与粒度约定”；新增 **Q9（段落判定口径：A 调用栈祖先 / B 顺序 latch）**；§8/§9 相应调整。 |
| 2026-10-15 | **登记反编译素材来源**：新增 `docs/logger/decompile_materials.md`（`../exes` 的 exe / ecl / decl 与 `../th15decode/` 的来源、清单、生成链、复核方式）；文档头与 §5.1 表头、注记同步指向该清单，明确 th10–18 各作 `.ecl`/`.decl` 已在机、可离线枚举段落入口。 |
| 2026-10-15 | **订正 Q9 判定口径**：原「(A) 调用栈祖先法」与段内脚本结构不符——段内多个 `MainSubNN` 是 `MainFront`/`MainLatter` 的**兄弟 `call`**，彼此不互为祖先，「整段驻留栈上」不成立。改为**路过式 checkpoint（latch）**：经过表内函数即置值、不在表内 no-op（**不回退 `main`**）。据此**每个 checkpoint 只登记 1 个段落入口函数**（= warp 落点所进入的 sub，含 `async`）。“函数栈存在”仍是事实（Q3），但不参与判定。§0/§1.2/§2/§2.1/§6 Q9/§9 同步改写。 |
| 2026-10-16 | **回填 boss 段（方案二）总表**：新增 §5.3「boss 段（方案二）逐作锚点」（th14–th18 的管理器全局 / 槽表 / 宿主链 / 按槽·按 id 取 host 函数 / host id / boss 门控位）；新建逐作报告 `docs/th{14,15,17,18}/thXX_boss_phase_report.md`（th16 已有），并回填各作 `docs/logger/thXX_checkpoints.csv` 的 boss 行（th14 64、th15 10、th17 63、th18 65）。**关键订正**：th15 `../th15decode/boss-phase-hook.md` 的 `host=*(E+0x44B8)` 有误（`mgr+0x180` 链上对象 `E` **本身即 host**；游戏自身「当前 boss」查询应为 `FUN_0041F7F0` 而非 `0x48F7F0`）；th17 报告 §4.1 的 `+0x48` 属管理器 `mgr`（`mgr+0x48` = 槽→id 表）、非 host 字段。状态：静态调研（🟡 未真机三态）。 |
| 2026-10-16 | **A 代（th06–th08）E0 建档**：§0 范围改写；§4 分代表 A 代行 + A 代特殊性注记（**章节跳转＝时间轴**，非 sub 跳转；Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一）；**§5.1 新增 th06–th08 三行** + A 代「无 eclmap / 无 ecl 素材」例外注记。**纠错**：文档头与 §5 原写「本机为 macOS、无法编译/真机」为**误**，改为 Windows 11（`docs/gametime/` 已在真机跑过）。逐作文档：`docs/th06|th07|th08/thXX_ecl_re_requests.md`。 |
| 2026-10-16 | **B 代 th10/th11/th12 逆向收口（E1）**：新建 `docs/th10|th11|th12/thXX_ecl_re_report.md`（Q1–Q9 静态定案），并回填各自 `_requests.md`。据此同步本文：**§5.1** 填 th10/th11/th12 三行（命中点 / 字段 / 链 / sub 名线索 ❌→✅），并**同步 th13 那行早已定案却仍写 ❌ 的偏差**（`0x4772D2`/6B/`EBX`）；**§5.1 注记**把「`[执行对象+4]` 是索引还是指针」由「首要矛盾」改判为**分代结论**（B 代 = 指令绝对地址、C 代 = sub 索引），并新增「B 代取当前 sub 须**遍历宿主线程链**」与「B 代主线宿主捕获点」两条；**§4** 增记 th12 报告提出的 **B1 = th12/th13 / B2 = th10/th11** 细分建议（含 th11 帧长未定案这一反证，暂不改分代表）；**§9** 风险 3（保留 sub 名）、4（`[+4]` 语义）、5（th12/13 无命中点）由「待厘清」改判为已定案/已消解，并新增风险 9（**th10 st1–st3 段入口落在 `main` 内部，sub 名粒度可能不足**，待 E3/E4 决策）。**全部为 🟡 静态定案（未真机三态）**；段落入口白名单仍未枚举。 |

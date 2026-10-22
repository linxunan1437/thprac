# TH10 checkpoint「段落入口」白名单 — 入口表小结（109/109 已枚举）

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志）
> 总括（术语 / 判定口径以此为准）：`docs/logger/logger_re_disasm_overview.md`（**§1.2 判定口径 / §6 Q9 / §9.9 粒度**）
> 本作逆向报告：`docs/th10/th10_ecl_re_report.md`（**§2.3 地址区间反查 / §3.3 `+0x10` 首指令 / §3.5 `GetMemAddr` 展开 / §5A boss 宿主可达性 / §10.4 / §10.5**）
> ip 偏移可行性与逐行数据来源：`docs/th10/th10_checkpoint_ip_offset.md`（**§3.1 18 行偏移表 / §3.2 特例行 / §5.1 boss 撞点**）
> 回填口径：`docs/logger/checkpoint_entry_backfill_workflow.md`
> 数据：`docs/logger/th10_checkpoints.csv`（**第 1 列 109/109**：章节 41/41、boss 68/68）
> 二进制：`../exes/th10.exe`，**487936 B**，PE `TimeDateStamp = 1186086096`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）
> 素材：`../exes/ecl/th10/`（原始 `.ecl` 8 份）、`../exes/ecl_dump/th10/`（`thecl -d` 反编译 `.decl` 8 份）—— **仓库外只读，不提交**
> 方法：离线 SCPT sub 表解析（`python` + `struct`）+ 逐指令解码 + 对照 `thprac_th10.cpp` 的 `THStageWarp`/`THPatch` 常量 + 引用 th10 报告的只读反汇编结论。
> 状态：**静态定案（🟡 未真机三态）**。本文只汇总**入口表 / 口径特例 / 实现影响**；逐行偏移的推导证据见 `th10_checkpoint_ip_offset.md`。
> 地址口径：正文以 **VA** 为主并标 **RVA = VA − 0x400000**；本文 / CSV 里的「偏移」一律是**相对该 sub 首条指令的字节偏移**（见 §2.1），与「相对文件缓冲的偏移」相差该 sub 的首指令基址。
> 置信度：✅ 离线解码 / 源码直接证实 ｜ 🟡 有强证据但缺一环（显式标注）｜ ❌ 未能定案。

---

## 0. 结论速览

| # | 问题 | 结论 | 一句话 |
| --- | --- | --- | --- |
| 1 | 白名单是否枚举完毕 | ✅ **109/109** | 章节 **41/41**、boss **68/68**；th10 是 B 代里首个「章节 + boss 全表回填」的作。 |
| 2 | 道中行记法 | ✅ **sub 名** 为主，st1–st3 的 15 行用 **`sub名+0x偏移`** | 全部章节行中 **18 行**带 `+0x` 后缀（st1–st3 共 15 + `ST4 前半 #2` + `ST5 后半 #1/#2`），其余为纯 sub 名。 |
| 3 | 偏移基准 | ✅ **相对该 sub 首条指令的字节偏移** | = sub 数据基址 `+0x10`；与 th13 的 `MainSub08+0x390` **同一基准**。**用户已定案**：统一用此第 1 列记法，**不新增第 5 列**。 |
| 4 | boss 行记法 | ✅ **boss 脚本 sub 名**（`MBoss*` / `Boss*` / `BossCard*`） | 无偏移后缀；与 th14–th18 的 boss 行同形。✅ **boss 侧读取链已收口（2026-10-18）**：宿主链表 `mgr+0x58` 是**全宿主链（含 boss）**，boss 宿主另有捕获点 `0x40ED16`，boss sub 与主线**同表** ⇒ 无需新锚点（报告 **§5A**；§3.5 已改写）。 |
| 5 | 实现影响 | ✅ **双匹配**：sub 名 + ip 偏移（**偏移命中优先**） | 解析第 1 列时识别 `+0x` 后缀；`off = ip − base`，`base` 在堆上、**不可缓存**（§3）。 |
| 6 | 已知风险 | 🟡 **boss 跳转撞章节锚点 5 处** | `ST1 MID1/MID2_EN/MID2_HL`、`ST4 MID1`；复用 th16「中boss 窗口只缓存」（§3.4）。 |

---

## 1. 入口表（CSV 第 1 列，按关分组）

> 左列 = `记录名`，右列 = `段落入口函数`；带 `+0x` 者为偏移记法（§2.1）。每关先列**道中行**、再列 **boss 行**。

### ST1（道中 5 行 + boss 7 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST1 前半 #1 | `main+0x28` | | ST1 MID1 | `MBoss` |
| ST1 前半 #2 | `main+0x2b0` | | ST1 MID2_EN | `MBoss2` |
| ST1 前半 #3 | `main+0x928` | | ST1 MID2_HL | `MBossCard1` |
| ST1 后半 #1 | `main+0x1348` | | ST1 BOSS1 | `Boss1` |
| ST1 后半 #2 | `main+0x1560` | | ST1 BOSS2 | `BossCard1` |
| | | | ST1 BOSS3 | `Boss2` |
| | | | ST1 BOSS4 | `BossCard2` |

### ST2（道中 5 行 + boss 6 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST2 前半 #1 | `main+0x5c` | | ST2 MID1 | `MBossCard1` |
| ST2 前半 #2 | `main+0x374` | | ST2 BOSS1 | `Boss1` |
| ST2 前半 #3 | `main+0x8dc` | | ST2 BOSS2 | `BossCard1` |
| ST2 后半 #1 | `main+0x920` | | ST2 BOSS3 | `Boss2` |
| ST2 后半 #2 | `main+0x144c` | | ST2 BOSS4 | `BossCard2` |
| | | | ST2 BOSS5 | `BossCard3` |

### ST3（道中 7 行 + boss 7 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST3 前半 #1 | `main+0x5c` | | ST3 MID1 | `MBossCard1` |
| ST3 前半 #2 | `main+0x144` | | ST3 BOSS1 | `Boss1` |
| ST3 前半 #3 | `MainSub03` | | ST3 BOSS2 | `BossCard1` |
| ST3 前半 #4 | `MainSub04` | | ST3 BOSS3 | `Boss2` |
| ST3 后半 #1 | `main+0x964` | | ST3 BOSS4 | `BossCard2` |
| ST3 后半 #2 | `main+0xc74` | | ST3 BOSS5 | `Boss3` |
| ST3 后半 #3 | `main+0x17d4` | | ST3 BOSS6 | `BossCard3` |

### ST4（道中 8 行 + boss 8 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST4 前半 #1 | `MainSub1` | | ST4 MID1 | `MBoss` |
| ST4 前半 #2 | `MainSub1+0x154` | | ST4 BOSS1 | `Boss1` |
| ST4 前半 #3 | `MainSub2` | | ST4 BOSS2 | `BossCard1` |
| ST4 前半 #4 | `MainSub3` | | ST4 BOSS3 | `Boss2` |
| ST4 后半 #1 | `MainSub5` | | ST4 BOSS4 | `BossCard2` |
| ST4 后半 #2 | `MainSub6L` | | ST4 BOSS5 | `Boss3` |
| ST4 后半 #3 | `MainSub7` | | ST4 BOSS6 | `BossCard3` |
| ST4 后半 #4 | `MainSub8L` | | ST4 BOSS7 | `BossCard4` |

### ST5（道中 6 行 + boss 9 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST5 前半 #1 | `MainSub1` | | ST5 MID1 | `MBoss` |
| ST5 前半 #2 | `MainSub2` | | ST5 MID2 | `MBossCard1` |
| ST5 前半 #3 | `MainSub3` | | ST5 BOSS1 | `Boss1` |
| ST5 前半 #4 | `MainSub7` | | ST5 BOSS2 | `BossCard1` |
| ST5 后半 #1 | `MainSub8+0x14` | | ST5 BOSS3 | `Boss2` |
| ST5 后半 #2 | `MainSub8+0x444` | | ST5 BOSS4 | `BossCard2` |
| | | | ST5 BOSS5 | `Boss3` |
| | | | ST5 BOSS6 | `BossCard3` |
| | | | ST5 BOSS7 | `BossCard4` |

### ST6（道中 4 行 + boss 9 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST6 #1 | `MainSub1` | | ST6 BOSS1 | `Boss1` |
| ST6 #2 | `MainSub2` | | ST6 BOSS2 | `BossCard1` |
| ST6 #3 | `MainSub3` | | ST6 BOSS3 | `Boss2` |
| ST6 #4 | `MainSub5` | | ST6 BOSS4 | `BossCard2` |
| | | | ST6 BOSS5 | `Boss3` |
| | | | ST6 BOSS6 | `BossCard3` |
| | | | ST6 BOSS7 | `Boss4` |
| | | | ST6 BOSS8 | `BossCard4` |
| | | | ST6 BOSS9 | `BossCard5` |

### ST7（道中 6 行 + boss 22 行）

| 记录名 | 段落入口函数 | | 记录名 | 段落入口函数 |
| --- | --- | --- | --- | --- |
| ST7 前半 #1 | `MainSub1` | | ST7 MID1 | `MBoss1` |
| ST7 前半 #2 | `MainSub2` | | ST7 MID2 | `MBossCard1` |
| ST7 前半 #3 | `MainSub4` | | ST7 MID3 | `MBossCard2` |
| ST7 前半 #4 | `MainSub6` | | ST7 MID4 | `MBossCard3` |
| ST7 后半 #1 | `MainSub8` | | ST7 END_NS1 | `Boss1` |
| ST7 后半 #2 | `MainSub9` | | ST7 END_S1 | `BossCard1` |
| | | | ST7 END_NS2 | `Boss2` |
| | | | ST7 END_S2 | `BossCard2` |
| | | | ST7 END_NS3 | `Boss3` |
| | | | ST7 END_S3 | `BossCard3` |
| | | | ST7 END_NS4 | `Boss4` |
| | | | ST7 END_S4 | `BossCard4` |
| | | | ST7 END_NS5 | `Boss5` |
| | | | ST7 END_S5 | `BossCard5` |
| | | | ST7 END_NS6 | `Boss6` |
| | | | ST7 END_S6 | `BossCard6` |
| | | | ST7 END_NS7 | `Boss7` |
| | | | ST7 END_S7 | `BossCard7` |
| | | | ST7 END_NS8 | `Boss8` |
| | | | ST7 END_S8 | `BossCard8` |
| | | | ST7 END_S9 | `BossCard9` |
| | | | ST7 END_S10 | `BossCard10` |

### 1.1 命名规律

- **道中行**：该关有前 / 后拆分时记 `STn 前半 #k` / `STn 后半 #k`（拆分由 `mChapterSetup[7][2]` 的 前半 / 后半 决定）；**无拆分**（`后半 == 0`，本作仅 **ST6**）记 `STn #k`。`k` 在关内按 warp 顺序从 1 递增，**与 sub 名无关**（同一 `main` 的内联区段也会各占一个 `k`）。
- **boss 行**：**中boss** 段记 `STn MIDm`（本作 `MID1..MID4`，ST6 无 MID 行）、**关底 boss** 段记 `STn BOSSj`；**ST7 例外**，关底段不叫 `BOSSj` 而按「非符 / 符卡」成对命名 `STn END_NSj` / `STn END_Sj`。`m` / `j` 在关内按 `THPatch` 顺序递增。

---

## 2. 口径与特例（本作重点）

### 2.1 st1–st3 的 15 行用 **ip 偏移记法**

- **格式**：`sub名+0x偏移`（例 `main+0x28`）。
- **偏移基准**：**相对该 sub 首条指令的字节偏移** —— 即 `sub 数据基址 + 0x10`（`table[i].+4 + 0x10`，见报告 §3.3 的 `add eax, 0x10`）为偏移 0。**与 th13 的 `MainSub08+0x390` 同一基准**。
- **为什么用偏移**：st1–st3 的**道中段落全部落在 `main` 内部**（`main` 是覆盖全关的 ~0x1EB4 B 超长脚本，道中为**内联区段**，`.decl` 佐证无 `MainSubNN`）⇒ **sub 名粒度不够**，必须靠 ip 偏移区分（报告 §10.5）。15 行 = ST1 5 + ST2 5 + ST3 5。
- **与 `th10_checkpoint_ip_offset.md` §3.1 的换算**：该文「落点」列以 **sub 数据基址**为基准（如 `main +0x2c0`），**本表 / CSV 以首条指令**为基准，故 **CSV 值 = §3.1 落点值 − 0x10**（`main +0x2c0` ↔ `main+0x2b0`）。
- **用户已定案**：统一用第 1 列的 `sub名+0x偏移` 记法（与 th13 一致），**不新增第 5 列**。

### 2.2 `ST4 前半 #2` = `MainSub1+0x154`（取 **warp 落点**）

- 该值 = `THPatch`/`THStageWarp` 的 **warp 落点 `dest`**（文件偏移 `0xe048`，= `MainSub1` 数据基址 `+0x164`）。
- **已知缺陷（如实记录）**：该落点会被「**快进跑穿**」——`t_land = 110 ≤ at_frame = 409`，`goto` 把 `T+0` 置 409 后，落点及其后 6 条指令立即执行，ip 一路跑到 `0xe0c0`（t=410）才停 ⇒ **`0xe048` 在帧边界上观测不到**；`0xe0c0` 又只有 **1 帧窗口**。
- **~~用户决定无视该 1 帧窗口问题~~ → 2026-10-20 推翻**：真机反馈「`ST4 前半 #2` 无法显示」，离线解码证实等式匹配**物理上不可满足**（落点指令 `time=110` 而 warp 把 `T+0` 置 409 ⇒ 跑穿）。⇒ **CSV 值不变**（仍登记落点 `MainSub1+0x154`），但**运行期匹配语义改为「半开区间」**（`delta = ip − firstInsn` + 取第一个 `delta >= offset` 的同行行，与 th13 定案同语义）。详见 `th10_checkpoint_ip_offset.md` §3.2 / §4.3 与变更记录（2026-10-20）、`th10_monitor_impl_report.md` §8.2。

### 2.3 `ST5 后半 #1/#2` = `MainSub8+0x14` / `MainSub8+0x444`（同 sub、不同偏移）

- 两行**同属 sub `MainSub8`**，纯 sub 名粒度下无法区分（会互相误命中）⇒ 用**同一 sub 内两个不同偏移**消歧（`0x132b8` / `0x136e8`，`th10_checkpoint_ip_offset.md` §3.1 行 17/18）。

### 2.4 3 行 p1（`ST1/ST2/ST3 前半 #1`）用 `main` 的第一个 `t=160` 等待点

- 值 = `main+0x28` / `main+0x5c` / `main+0x5c`：**该关章节补丁位**（`THStageWarp` 的 `start`）。
- **理由**：p1 段**没有 `ECLJump`**（warp 模式 1 的 `case 1: break;`），段落入口 = 关卡正常开始 ⇒ 语义入口 = `main` **首指令**（`t=0`，会立即执行、**不可观测**）；**可观测锚点 = `main` 的第一条 `time > T+0`（`T+0=0`）的等待点**，即该 `t=160` 处 —— thprac 正是把 `goto` 插在「关卡开始后第一个等待点」上，故此处**恰是章节补丁位**。
- **好处**：warp 到 p2..pN 时该处已被换成 `ecl_time = 0` 的 `goto`（立即执行、不在帧边界停留）⇒ 不会误 latch 为 p1（同帧内自纠，见 `th10_checkpoint_ip_offset.md` §5.2）。

---

## 3. 实现方案影响（本次重点）

### 3.1 双匹配：sub 名 + ip 偏移（**偏移命中优先**）

th10 的 logger 实现（**尚未实现**，`logger_progress.md` §1：E5 ❌）将**同时支持两种匹配**：

| 匹配 | 判据 | 适用行 |
| --- | --- | --- |
| **sub 名匹配** | `cur = *(char**)(table + i*8)` ∈ 白名单 | 纯 sub 名的行（th14–th18 同形） |
| **ip 偏移匹配** | `ip` 命中该行的 `(sub 名, 偏移)` | 18 行带 `+0x` 后缀者 |

- **优先级：偏移命中 > 名字命中**（同一行既有 sub 名又有偏移时以偏移为准）。
- **解析第 1 列时识别 `+0x` 后缀**（**不是**读第 5 列 —— 用户已否决新增列）：无后缀 → 纯 sub 名；有后缀 → 拆成 `(sub 名, 偏移)`。
- th10 **尚无监控实现** ⇒ 「按 ip 偏移匹配」是**新实现的设计选择**，非迁移；th14–th18 按 sub 名匹配、**完全不受影响**（其 CSV 只取前 4 列）。

### 3.2 指针链（运行期取「当前指令 ip」→「偏移」）

```text
H     = 主线 ECL 宿主（捕获点 0x41829E / 0x41849E，命中 EAX = H）
T     = *(u32*)(H + 0x1030)                    // 主线内嵌线程（≡ *(u32*)(H+4)）
ip    = *(u32*)(T + 4)                          // 当前（下一条待执行）指令的【绝对地址】；0 = 线程已结束
res   = *(u32*)(*(u32*)0x477704 + 0x54)         // ECL 资源对象（全局链；异步线程对象无 res 槽）
base  = *(u32*)(res + 0xC + slot*4)             // ★ slot 的文件缓冲基址（章节行固定 slot 0 = stNN.ecl）
off   = ip - base                               // ★ 相对文件缓冲的字节偏移
```

- **与 CSV 第 1 列的换算**：CSV 的 `+0x` 是**相对该 sub 首条指令**的偏移，故运行期要么
  - (a) 直接按地址比：`ip == table[i].+4 + 0x10 + 偏移`（`i` = 该 sub 名在**运行期 sub 表**的下标）；或
  - (b) 预先把该行换算成**文件偏移** `(table[i].+4 + 0x10 + 偏移) − base`，再与 `off = ip − base` 比对。
  - 两者等价（相差该 sub 的首指令基址）。**`table[i].+4 + 0x10` = 该 sub 首条指令**（报告 §3.3）。
- **`ip == 0`**（线程已结束）必须先判、再算偏移（否则 `off` 下溢）。

### 3.3 基址**不可缓存**

- `res`、各 slot 的**文件缓冲基址 `base`** 都在堆上（`FUN_0040D260` `malloc(0x1098)` 造 `res`；加载器 `FUN_00450220` 另分配文件缓冲）⇒ **每帧沿指针链重读**，**不可跨帧 / 跨局缓存地址**（与 th15 正式实现的「不缓存任何指针」一致，`logger_progress.md` §4 第 8 条）。
- **`res` 一律走全局链** `*(u32*)(*(u32*)0x477704 + 0x54)`：异步线程对象只 `malloc(0x1024)`、**无 `res` 槽**（报告 §4.5）。
- **多线程**：B 代段入口常以 `@MainSubNN() async` 派生为**独立线程**，主线 `T+4` 可能长期停在 `main`，且 `H+4` 会被本帧最后执行过的 async 线程覆盖 ⇒ **须遍历宿主线程链 `H+0x1030`**（节点 `{+0=线程,+4=next,+8=prev}`）逐线程取 ip（报告 §4.5 / 总括 §5.1 注记）。本表 18 行里 **st1–st3 的 15 行在主线程**，**`ST4 前半 #2`（`MainSub1`）、`ST5 后半 #1/#2`（`MainSub8`）在 async 线程**。
- **slot 号**：本表章节行固定 **slot 0**；若日后扩到 boss 行须一并登记槽号（boss 脚本在 `stNNbs` / `stNNmbs` 等槽）。

### 3.4 已知风险：**boss 跳转撞章节锚点 5 处**（🟡 已离线实测）

- thprac 的章节跳转与 boss 跳转**共用同一个「章节闸门」偏移**（st1 `0x9cb8`、st4 `0x14a64`），boss 跳转只改 `at_frame`；`t_land ≤ at_frame` 时 ip 继续跑到下一个等待点，**正好是「后半第一节」的锚点**：

| boss 行 | 撞上的章节行 | 撞点（可观测 ip） |
| --- | --- | --- |
| `ST1 MID1` / `MID2_EN` / `MID2_HL` | **`ST1 后半 #1`** | `0xafd8` |
| `ST4 MID1` | **`ST4 前半 #1`** | `0x14aa4` |
| `ST4 MID1` | **`ST4 后半 #1`** | `0xf930` |

- **性质**：**不是 ip 偏移方案特有** —— 既有 sub 名口径同源（`ST4 MID1 → MainSub5` = `ST4 后半 #1` 的 sub 名，同样误判）。
- **对策**：复用 th16 的「**中boss 窗口只缓存**」机制（中boss 存活期间章节 checkpoint 只缓存 `mPending`，boss 死亡 sub 出现时补显示）。th10 的 boss ECL 跑在 **boss 自己的宿主**里，误判发生在**主线宿主**；「中boss 存活」**不必从 boss 宿主侧采集** —— 可直接读 `*(u32*)(mgr+0x10)`（thprac 既有 `bossExists`，`thprac_th10.cpp:548`；报告 **§5A.2/§5A.4**）。
- 逐处明细与成因见 `docs/th10/th10_checkpoint_ip_offset.md` §5.1。

### 3.5 boss 侧运行期读取链（**2026-10-18 已收口：缺口消解**）

- **原缺口（`logger_progress.md` §4 第 11 条）**：boss 是**另一个 ECL 宿主**（由 `enmCreate` 另起），而 Q4 只给了**主线宿主**捕获点 ⇒ 曾判「boss 宿主 / 线程链尚无逆向锚点」。
- **收口结论（见报告 §5A）**：
  1. **宿主链表 `mgr+0x58` 是「全宿主链」（含 boss）** —— 全程序只有一个宿主工厂 `FUN_0040CFB0`，每个宿主（main / 中boss / 关底boss / 杂兵）都被**无条件**挂上这条链。⇒ **可经宿主链表读到 boss 宿主，无需补锚点**；只是链头是最先建的宿主（通常 `main`），须**沿 `node+4` 遍历**。
  2. **boss 宿主捕获点（可选加速）** = `0x40ED16` / RVA `0xED16`（`enmCreateA("Boss"/"MBoss", …)` 走的工厂调用点 `0x40ED11` 的下一条；`EAX` = 新宿主 H）；全 `enmCreate*` 变体另见 `0x40EAEF` / `0x40EC0D`。
  3. **「地址 → 名字」区间反查对 boss 行成立** —— th10 **无独立 boss 文件 / boss 槽**（7 关 `ecli` 只有 `default.ecl`），boss sub 与 `main` 同在 `stageNN.ecl`、同属**一份全局 `res`**（`H+0x102C = *(u32*)(mgr+0x54)`）⇒ 同表。
- ⇒ **入口表（本表）已齐，boss 侧的读取链也已齐**：boss 行**不需要**新的捕获点即可实现（把读取链改成「遍历全宿主链 × 各宿主线程链 `H+0x1030`」即可，见报告 §5A.4）。

---

## 4. 风险 / 待真机清单

> 本机**未做**任何真机验证；以下**全部未做**，状态一律 🟡（静态定案、未真机三态）。

1. 🟡 **全静态，未真机**：入口表来自离线 SCPT 解析 + 逐指令解码 + `capstone` 复核 + 引用 th10 报告；**未真机三态**。
2. 🟡 **偏移可读**：进关后 `res = *(u32*)(*(u32*)0x477704+0x54)`、`base = *(u32*)(res+0xC)` 均非空，且 `base` 处 4 字节 = `"SCPT"`（`0x54504353`）。
3. 🟡 **坐标系（st1–st3 的 12 个稳定锚点 = 15 行 − 3 行 p1）**：warp 到 `ST1 前半 #2` 后，`*(u32*)(T+4) − base` 恰为 `0x9f40`（= `main+0x2b0` + `main` 首指令偏移 `0x9c90`）；逐条采样。（注：`th10_checkpoint_ip_offset.md` §6 写「13 个」应为笔误，实为 12。）
4. 🟡 **p1 锚点自纠（§2.4）**：warp 到 `ST1 前半 #1` 后帧边界读到 `0x9cb8`；再 warp 到 `ST1 前半 #2`，确认**从未**在帧边界读到 `0x9cb8`（补丁位已被换成 `goto`）。
5. 🟡 **`ST4 前半 #2` 跑穿（§2.2）**：warp 到 c2，确认帧边界读到 `0xe0c0` 或 `0xe148`（落点 `0xe048` 不可观测）。**用户已决定无视 1 帧窗口**，此处仅记录实测现象、不据此改表。
6. 🟡 **同名冲突消解（§2.3）**：warp 到 `ST5 后半 #1` / `#2`，确认分别读到 `0x132b8` / `0x136e8`（同一 sub `MainSub8` 内两个不同偏移）。
7. 🟡 **多线程**：warp 到 `ST4 前半 #2`（`MainSub1`）与 `ST5 后半 #1/#2`（`MainSub8`），确认锚点 ip 出现在 **async 线程**上（`H+0x1034` 起的链表）；只读 `H+0x1030` 会漏。
8. 🟡 **boss 撞点（§3.4，5 处）**：warp 到 `ST1 MID1` / `ST4 MID1`，确认主线程 ip 确实落到 `0xafd8` / `0x14aa4` / `0xf930`（即章节行会被误 latch），再验证「中boss 窗口只缓存」能否挡住。
9. 🟡 **四难度**：`ECLJump` 写入的 `ins[0x0A] = 0xFF`（任意难度）在 E/N/H/L 下均生效（否则 `goto` 会被 `test byte [T+0x101C], cl` 跳过 —— 报告 §13 点名的最需真机确认项）。
10. 🟡 **采样时机**：确认 `Update()` 相对 ECL VM 的执行顺序（帧内 `goto` 是否一定在采样前执行完）—— 决定「精确匹配」是否够用。
11. 🟡 **变体**：Steam / 汉化版的 `stageNN.ecl` 是否与原版一致（偏移是**数据偏移**，脚本改版即失效）。
12. 🟡 **boss 侧读取链（§3.5，2026-10-18 缺口消解）**：宿主链表 `mgr+0x58` 是**全宿主链（含 boss）**、boss sub 与主线**同表** ⇒ boss 行**无需补锚点**即可实现。**真机需验**：进 boss 后 `*(u32*)(mgr+0x58)` 链上存在**第 2 个**宿主（节点 `+4` 链），其线程链 `H+0x1030` 的 `*(u32*)(T+4)` 落在 `Boss*` / `MBoss*` 区间内；可选捕获点 `0x40ED16`（`EAX` = 新宿主）命中。

---

## 5. 与其它文档的关系

- **口径 / Q9** → `docs/logger/logger_re_disasm_overview.md` §1.2 / §6 Q9 / §9.9。
- **回填流程** → `docs/logger/checkpoint_entry_backfill_workflow.md`（本文是其 §3.3 章节行口径在 th10 的落地）。
- **本作逆向报告** → `docs/th10/th10_ecl_re_report.md`（§10.4 已收口、指向本文；§10.5 为粒度分析）。
- **逐行偏移数据与推导** → `docs/th10/th10_checkpoint_ip_offset.md`（§3.1 偏移表 / §5.1 boss 撞点）。
- **参考格式** → `docs/th14/th14_boss_phase_report.md`（风险 / 地址表写法）；**成品数据** → `docs/logger/th10_checkpoints.csv`。

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **§2.2 口径订正（真机反馈驱动）**：原「`ST4 前半 #2` 的 1 帧窗口问题**用户决定无视**、照落点登记、**不引入区间匹配**」**已推翻** —— 真机反馈该行无法显示，离线解码证实 `MainSub1+0x154` 的落点指令 `time=110`、warp 把 `T+0` 置 409 ⇒ 同帧跑穿（可观测 `ip` 为 `0xe0c0`）⇒ 等式匹配不可满足。**CSV 值不变**（仍 `MainSub1+0x154`），**运行期匹配语义改为半开区间**（与 th13 同语义）；18 行锚点复算不变。详见 `th10_monitor_impl_report.md` §8.2 / `th10_checkpoint_ip_offset.md` 变更记录（2026-10-20）。**🟡 未真机。** |
| 2026-10-17 | 建档。汇总 th10 checkpoint 白名单（**109/109**：章节 41/41、boss 68/68）：§1 按关列出全部入口（道中 + boss）并给出命名规律；§2 记录口径与特例（**st1–st3 的 15 行用 `sub名+0x偏移` 记法，偏移基准 = 相对该 sub 首条指令的字节偏移**、与 th13 `MainSub08+0x390` 同基准，**用户已定案不新增第 5 列**；`ST4 前半 #2` = `MainSub1+0x154` 取 warp 落点、已知跑穿但**用户决定无视 1 帧窗口**；`ST5 后半 #1/#2` = `MainSub8+0x14`/`MainSub8+0x444` 消歧；3 行 p1 = `main` 第一个 `t=160` 等待点 = 章节补丁位）；§3 实现影响（**双匹配 sub 名 + ip 偏移、偏移命中优先**、解析第 1 列 `+0x` 后缀、指针链 `ip=*(u32*)(T+4)`/`base=*(u32*)(res+0xC+slot*4)`/`off=ip−base`、**基址不可缓存**、**boss 跳转撞章节锚点 5 处**、boss 侧读取链前置缺口）；§4 风险 / 待真机清单 12 条。**全部 🟡 未真机三态。** |
| 2026-10-18 | **boss 侧读取链收口（缺口消解）**：把 §3.5 从「boss 侧运行期读取链**尚未逆向**（前置缺口）」改写为「**已收口：缺口消解**」—— 依报告新增 §5A：① `mgr+0x58` 是**全宿主链（含 boss）**，全程序只有 1 个宿主工厂 `FUN_0040CFB0` 且无条件挂链 ⇒ **可经宿主链表读到 boss 宿主，无需补锚点**（只须沿 `node+4` 遍历）；② boss 宿主捕获点（可选加速）= **`0x40ED16` / RVA `0xED16`**（`EAX` = 新宿主 H），全变体另见 `0x40EAEF` / `0x40EC0D`；③ **「地址 → 名字」区间反查对 boss 行成立**（th10 无独立 boss 文件 / boss 槽，boss sub 与 `main` 同在 `stageNN.ecl`、同属一份全局 `res`）。连带订正：§0 速览第 4 行（原「boss 侧运行期读取链尚未逆向」→ 已收口）、§3.4 末条（「中boss 存活」**不必**从 boss 宿主侧采集，可直接读 `*(u32*)(mgr+0x10)` = thprac 既有 `bossExists`）、待真机第 12 条（改为真机需验「链上第 2 个宿主 + 其线程 ip 落在 `Boss*`/`MBoss*` 区间」）、文档头引用块（补 §5A）。**🟡 未真机。** |

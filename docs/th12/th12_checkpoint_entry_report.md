# TH12 checkpoint「段落入口」白名单 — 入口表小结（114/114 已枚举）

> 上位文档（术语 / 判定口径以此为准）：`docs/logger/logger_re_disasm_overview.md`（**§1.2 latch 口径 / §5.1 注记 / §6 Q9 判定 / §9.9 粒度风险**）
> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志）
> 本作逆向报告：`docs/th12/th12_ecl_re_report.md`（Q1–Q9；**§10.4 = 白名单枚举那一节，已收口并指向本文**）
> 回填口径：`docs/logger/checkpoint_entry_backfill_workflow.md`（**§3.3 章节行 / §3.4 boss 行**）
> 数据：`docs/logger/th12_checkpoints.csv`（**第 1 列 114/114**：章节 **43/43**、boss **71/71**）
> 参考格式：`docs/th10/th10_checkpoint_entry_report.md`、`docs/th13/th13_checkpoint_entry_report.md`（同批建档）、`docs/th14/th14_boss_phase_report.md`（风险 / 地址表写法）
> 二进制：`../exes/th12.exe`，**737280 B**，PE `TimeDateStamp = 1251344348`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）
> 素材：`../exes/ecl/th12/`（原始 `.ecl` **9** 份）、`../exes/ecl_dump/th12/`（`thecl -d` 反编译 `.decl` **9** 份）——**仓库外只读，不提交**；来源见 `docs/logger/decompile_materials.md`
> 方法：离线 SCPT sub 表解析（`python` + `struct`）+ 逐指令解码（偏移 → sub 名 / `call`·`spawn` 内嵌名）+ 与 `thprac/src/thprac/thprac_th12.cpp` 的 `THStageWarp` / `THPatch` **逐条对读**。
> 状态：**静态收口（🟡 已枚举、未真机三态）**。本机无法运行 th12，**未做任何真机验证**；本文全部结论均标 🟡。
> 地址口径：本文的「偏移」一律是**该关 `.ecl`（或当前文件槽）内的字节偏移**——与 thprac `ECLJump*` 的 `start` / `dest` **同口径**（运行期缓冲内偏移 = 文件内偏移）；正文另标 VA 时同时给 **RVA = VA − 0x400000**。
> 置信度：✅ 离线解码 / 源码直接证实 ｜ 🟡 有强证据但缺一环（显式标注）｜ ❌ 未能定案。

---

## 0. 结论速览

| # | 问题 | 结论 | 一句话 |
| --- | --- | --- | --- |
| 1 | 白名单是否枚举完毕 | ✅ **114/114（2026-10-17）** | 章节 **43/43**、boss **71/71**；`docs/th12/th12_ecl_re_report.md` §10.4 由「未定论 / 待枚举」**收口**。 |
| 2 | 道中（章节）行记法 | ✅ **纯 sub 名**（`MainSubNN` / `MainSubNNb`），**无偏移后缀** | 落点 = `main` 内一条 **`@MainSubNN() async`（opcode `0x0F`）** 指令的**首字节** ⇒ 该指令内嵌名即入口（§2.1）。 |
| 3 | `portion 1` 入口 | ✅ **定案 = `MainSub00`（各关一致）** | 报告 §10.4 第 4 条原「待定」已消解（ST1–ST7 的「前半 #1」全部 = `MainSub00`）。 |
| 4 | boss 行记法 | ⚠️ **阶段 sub 名**（`BossK`/`BossCardK`/`MBossK`/`MBossCardK`） | **不适用** th14–th18 的「`stNNbs`/`stNNmbs` + `$SPELL_ID`」口径：th12 **无独立 boss 脚本文件**、**无 `$SPELL_ID`**（§2.2）。 |
| 5 | 相位选择机制 | ✅ 两条（本作特有） | ① 改容器 `call` 的**名字串**（`pair{off,'K'}` / 多字节改写）；② 改 `lifeSet` 操作数（`ECLSetHealth`）逼 `setNext` 立即转移（§2.2）。 |
| 6 | warp API | ✅ 一律 `ECLJumpEx` | `ECLJump`（**无 `dest`**，只做原地跳过）在本作 warp 代码里 **0 次调用**（定义在 `thprac_th12.cpp:997`，§2.1）。 |
| 7 | `ST6 BOSS9/BOSS10` | ✅ **= `BossCard5` / `BossCard6`（2026-10-17 修正）** | 前值 `Boss5`/`Boss6`；证据见 §2.3。 |
| 8 | 实现影响 | ⚠️ 读取面必须**遍历宿主线程链 `H+0x1030`** | 段落 sub 由 `opcode 0x0F` **派生为独立线程**，主线 `T+4` 全程停在 `main`（报告 §Q9 / §10.3，§3.1）。 |
| 9 | 已知风险 | 🟡 boss 容器是**独立宿主**（`enmCreateA`）⇒ 读取面必须改走**全局宿主链**；可能先采到**过渡 sub**；`ST7 END_S9/S10` 口径待确认 | 见 §3.2 / §4（R4、R5）。**2026-10-18 补**：宿主链 `mgr+0x68` 是**全局全宿主链**（含 boss 宿主）⇒ **不需要新捕获点**；报告 `th12_ecl_re_report.md` **§5.4（Q-A/Q-B/Q-C）** 已定案。 |

---

## 1. 入口表（`docs/logger/th12_checkpoints.csv` 第 1 列，按关分组）

> 左列 = `记录名`（前置所属关卡），右列 = `段落入口函数`。每关先列**道中（章节）行**、再列 **boss 行**。
> 记录名 ↔ portion 的换算由 `thprac_th12.cpp` 的 `THStageWarp` 与 `thprac_locale_def.cpp` 的 `th_sections_cba[7][2][…]`（TH12 段）决定。
> 容器（`main` / `Boss` / `MBoss`）**不入表**。

### ST1（道中 5 行 + boss 7 行）

| 记录名 | 入口 | | 记录名 | 入口 |
| --- | --- | --- | --- | --- |
| ST1 前半 #1 | `MainSub00` | | ST1 MID1 | `MBoss1` |
| ST1 前半 #2 | `MainSub02` | | ST1 MID2 | `MBoss2` |
| ST1 前半 #3 | `MainSub06` | | ST1 MID3 | `MBossCard1` |
| ST1 后半 #1 | `MainSub10` | | ST1 BOSS1 | `Boss1` |
| ST1 后半 #2 | `MainSub11` | | ST1 BOSS2 | `BossCard1` |
| | | | ST1 BOSS3 | `Boss2` |
| | | | ST1 BOSS4 | `BossCard2` |

### ST2（道中 5 行 + boss 7 行）

| 记录名 | 入口 | | 记录名 | 入口 |
| --- | --- | --- | --- | --- |
| ST2 前半 #1 | `MainSub00` | | ST2 MID1 | `MBoss1` |
| ST2 前半 #2 | `MainSub02` | | ST2 MID2 | `MBossCard1` |
| ST2 前半 #3 | `MainSub04` | | ST2 BOSS1 | `Boss1` |
| ST2 后半 #1 | `MainSub06` | | ST2 BOSS2 | `BossCard1` |
| ST2 后半 #2 | `MainSub07` | | ST2 BOSS3 | `Boss2` |
| | | | ST2 BOSS4 | `BossCard2` |
| | | | ST2 BOSS5 | `BossCard3` |

### ST3（道中 7 行 + boss 8 行）

| 记录名 | 入口 | | 记录名 | 入口 |
| --- | --- | --- | --- | --- |
| ST3 前半 #1 | `MainSub00` | | ST3 MID1 | `MBoss1` |
| ST3 前半 #2 | `MainSub01` | | ST3 MID2 | `MBossCard1` |
| ST3 前半 #3 | `MainSub02` | | ST3 BOSS1 | `Boss1` |
| ST3 前半 #4 | `MainSub03` | | ST3 BOSS2 | `BossCard1` |
| ST3 后半 #1 | **`MBoss1`** | | ST3 BOSS3 | `Boss2` |
| ST3 后半 #2 | `MainSub02b` | | ST3 BOSS4 | `BossCard2` |
| ST3 后半 #3 | `MainSub05` | | ST3 BOSS5 | `Boss3` |
| | | | ST3 BOSS6 | `BossCard3` |

### ST4（道中 8 行 + boss 8 行）

| 记录名 | 入口 | | 记录名 | 入口 |
| --- | --- | --- | --- | --- |
| ST4 前半 #1 | `MainSub00` | | ST4 MID1 | `MBoss1` |
| ST4 前半 #2 | `MainSub01` | | ST4 BOSS1 | `Boss1` |
| ST4 前半 #3 | `MainSub02` | | ST4 BOSS2 | `BossCard1` |
| ST4 前半 #4 | `MainSub03` | | ST4 BOSS3 | `Boss2` |
| ST4 后半 #1 | `MainSub04` | | ST4 BOSS4 | `BossCard2` |
| ST4 后半 #2 | `MainSub05` | | ST4 BOSS5 | `Boss3` |
| ST4 后半 #3 | `MainSub06` | | ST4 BOSS6 | `BossCard3` |
| ST4 后半 #4 | `MainSub07` | | ST4 BOSS7 | `BossCard4` |

### ST5（道中 7 行 + boss 9 行）

| 记录名 | 入口 | | 记录名 | 入口 |
| --- | --- | --- | --- | --- |
| ST5 前半 #1 | `MainSub00` | | ST5 MID1 | `MBoss1` |
| ST5 前半 #2 | `MainSub01` | | ST5 MID2 | `MBossCard1` |
| ST5 前半 #3 | `MainSub02` | | ST5 BOSS1 | `Boss1` |
| ST5 前半 #4 | `MainSub03` | | ST5 BOSS2 | `BossCard1` |
| ST5 后半 #1 | `MainSub05` | | ST5 BOSS3 | `Boss2` |
| ST5 后半 #2 | `MainSub04` | | ST5 BOSS4 | `BossCard2` |
| ST5 后半 #3 | `MainSub06` | | ST5 BOSS5 | `Boss3` |
| | | | ST5 BOSS6 | `BossCard3` |
| | | | ST5 BOSS7 | `Boss4` |

### ST6（道中 3 行 + boss 11 行）

| 记录名 | 入口 | | 记录名 | 入口 |
| --- | --- | --- | --- | --- |
| ST6 #1 | `MainSub00` | | ST6 MID1 | `MBoss1` |
| ST6 #2 | `MainSub01` | | ST6 BOSS1 | `Boss1` |
| ST6 #3 | `MainSub02` | | ST6 BOSS2 | `BossCard1` |
| | | | ST6 BOSS3 | `Boss2` |
| | | | ST6 BOSS4 | `BossCard2` |
| | | | ST6 BOSS5 | `Boss3` |
| | | | ST6 BOSS6 | `BossCard3` |
| | | | ST6 BOSS7 | `Boss4` |
| | | | ST6 BOSS8 | `BossCard4` |
| | | | ST6 BOSS9 | **`BossCard5`** |
| | | | ST6 BOSS10 | **`BossCard6`** |

### ST7（道中 8 行 + boss 21 行）

| 记录名 | 入口 | | 记录名 | 入口 |
| --- | --- | --- | --- | --- |
| ST7 前半 #1 | `MainSub00` | | ST7 MID1 | `MBossCard1` |
| ST7 前半 #2 | `MainSub01` | | ST7 MID2 | `MBossCard2` |
| ST7 前半 #3 | `MainSub02` | | ST7 MID3 | `MBossCard3` |
| ST7 前半 #4 | `MainSub03` | | ST7 END_NS1 | `Boss1` |
| ST7 后半 #1 | `MainSub00b` | | ST7 END_S1 | `BossCard1` |
| ST7 后半 #2 | `MainSub04` | | ST7 END_NS2 | `Boss2` |
| ST7 后半 #3 | `MainSub03` | | ST7 END_S2 | `BossCard2` |
| ST7 后半 #4 | `MainSub06` | | ST7 END_NS3 | `Boss3` |
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
| | | | ST7 END_S9 | `Boss9` |
| | | | ST7 END_S10 | `Boss10` |

### 1.1 注记（命名 / 特例）

- **`portion 1` = `MainSub00`（各关一致）**：报告 §10.4 第 4 条「portion 1 入口待定」**已定案**。CSV 中 ST1–ST7 的「前半 #1」全部为 `MainSub00`（ST6 无「前半/后半」划分，其 `#1` 亦为 `MainSub00`）。
- **`ST3 后半 #1` = `MBoss1`**：该段入口不是 `MainSubNN` 而是 midboss 容器入口 sub（`MBoss` 容器 → `MBoss1`），照实登记。
- **`ST5` 后半序号非单调**（`#1`=`MainSub05`、`#2`=`MainSub04`）——按 thprac `THStageWarp` 的 portion 顺序照实登记，**非笔误**。
- **`ST7 END_S9/END_S10` 登记为容器 sub**（`Boss9`/`Boss10`），与 `ST6 BOSS9/BOSS10` 登记符卡 sub 的口径**不同** —— **见 §4 风险 R4**（🟡 待人工确认）。
- 段落入口一律是 `main` 内 `@MainSubNN() async`（opcode `0x0F`）指令的**内嵌名**；`main` 主线线程本身不入表（§3.1）。

### 1.2 相位链（`setNext` 链，逐关）

> 来源：各关 `.decl` 的 `setNext(slot, hp, time, name)`（`BossK → BossCardK → Boss(K+1)` 交替推进）；midboss 走 `MBoss` 容器。

| 关 | boss（st1–st6 内联于 `stageNN.ecl`；st7 在 `stage07boss.ecl`） | midboss |
| --- | --- | --- |
| ST1 | `Boss1` → `BossCard1` → `Boss2` → `BossCard2` → `BossDead` | `MBoss1` / `MBoss2` → `MBossCard1` → `MBossDead` |
| ST2 | `Boss1` → `BossCard1` → `Boss2` → `BossCard2` → `Boss3` → `BossCard3` → `BossDead` | `MBoss1` → `MBossCard1` → `MBossDead` |
| ST3 | `Boss1` → `BossCard1` → `Boss2` → `BossCard2` → `Boss3` → `BossCard3` → `BossDead` | `MBoss1` → `MBossCard1` → `MBossDead` |
| ST4 | `Boss1` → `BossCard1` → `Boss2` → `BossCard2` → `Boss3` → `BossCard3` → (`BossCard4`) → `BossDead` | `MBoss1` |
| ST5 | `Boss1` → `BossCard1` → `Boss2` → `BossCard2` → `Boss3` → `BossCard3` → `Boss4` → `BossDead` | `MBoss1` → `MBossCard1` → `MBossDead` |
| ST6 | `Boss1` → `BossCard1` → `Boss2` → `BossCard2` → `Boss3` → `BossCard3` → `Boss4` → `BossCard4` → **`Boss5` → `BossCard5` → `Boss6` → `BossCard6`** → `BossDead` | `MBoss1` |
| ST7 | `Boss1` → `BossCard1` → `Boss2` → `BossCard2` → … → `Boss8` → `BossCard8` → `Boss9` → `BossCard9` → `Boss10` → `BossCard10` → `BossDead` | `MBossCard1` → `MBossCard2` → `MBossCard3` → `MBossDead` |

- 相位链 ↔ CSV 的对应：**「非」行 ↔ `BossK`、符卡行 ↔ `BossCardK`**（ST6 的 `BossCard5`/`BossCard6` 与 ST7 的 `BossCard1..8` 均落在 locale 的符卡栏，见 §2.3）。
- ST6 的 `BossCard4` 有**分支** `setNext`：`"Boss5"`（继续全链）或 `"Boss6"`（跳过 `Boss5`/`BossCard5`）——两条分支各自的入口都已在 CSV 收录（`BOSS9` = `BossCard5`、`BOSS10` = `BossCard6`）。

---

## 2. 口径与特例（本作重点）

### 2.1 两个 warp API：`ECLJump`（**无 `dest`**）与 `ECLJumpEx`（**`dest` 才是落点**）

`thprac_th12.cpp:997`–`1006`：

```cpp
void ECLJump  (ECLHelper& ecl, unsigned int start, unsigned int ecl_time, int length)
{
    ecl.SetPos(start);
    ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << length << 0;
}
void ECLJumpEx(ECLHelper& ecl, unsigned int start, unsigned int dest, int at_frame, int ecl_time = 0)
{
    ecl.SetPos(start);
    ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
}
```

两者都在 `start` 处写同一条 **`goto`（opcode `0x0C`）** 指令（6 个 dword，`size = 0x18`）；差别只在后两个操作数：

| API | `ins[+0x10]` | `ins[+0x14]` | 语义 |
| --- | --- | --- | --- |
| `ECLJump` | `length` | `0` | **相对**跳转：`goto` 语义为 `T+4 = ins + ins[+0x10]` ⇒ **原地向前跳过 `length` 字节**；**没有绝对落点**。 |
| `ECLJumpEx` | `dest − start` | `at_frame` | **`dest` 才是落点**（`T+4 = ins + (dest − start)` = `dest`），并顺带把 `T+0`（时间）设为 `at_frame`。 |

- **本作 warp 一律用 `Ex` 版**：全文件 `ECLJumpEx(` 共 **171** 处调用；`ECLJump(`（非 `Ex`）**0 处调用**（仅 997 行的定义本身）⇒ 本作**不存在**「无落点」的 warp 段，回填工作流 §3.3 规则 2（「该段没有第二条 `ECLJump`」）在本作**不适用**。
- ⚠️ **与回填工作流的口径差异**：`checkpoint_entry_backfill_workflow.md` §3.1 把「`ECLJump(start, dest, at_frame, ecl_time)`，`dest` 才是落点」写作通用形态——**那其实是本作的 `ECLJumpEx`**；th12 里名为 `ECLJump` 的函数**没有 `dest`**。本文按 th12 源码的实际签名叙述。
- 相关死代码：`ECLCallSub`（`thprac_th12.cpp:1012`）在本作**从未被调用**，且写出的指令头与真实 `call` 不符（报告 §9.3）——**不可用于 th12**。

### 2.2 boss 行**不适用**「`stNNbs`/`stNNmbs` + `$SPELL_ID`」口径

回填工作流 §3.4（及 th14/th16 的 boss 报告）默认的 boss 行口径是：**boss 有独立脚本文件**（`stNNbs` / `stNNmbs`，由 `SetFile(N)` 切槽），相位由 **`$SPELL_ID`**（或 ordinal）派发。**这两条在 th12 都不成立**：

1. **没有独立 boss 脚本文件**：各关 `.decl` 头 `ecli{}` 实测——
   - ST1–ST6：`ecli { "default.ecl"; }` ⇒ **只有槽 0 = `stageNN.ecl`**，`Boss` / `Boss1` / `BossCard1` / … / `MBoss1` 等 boss sub **全部内联在 `stageNN.ecl` 里**（例：`stage06.ecl` 含 `Boss`、`Boss1`…`Boss6`、`BossCard1`…`BossCard6`、`MBoss`、`MBoss1`）。
   - ST7：`ecli { "default.ecl"; "stage07boss.ecl"; }` ⇒ 槽 2 = `stage07boss.ecl`（thprac 的 `SetFile(2)`，见 `THPatch` 的 `nue` lambda @ `thprac_th12.cpp:1204`），**只有这一关有独立 boss 文件**。
2. **没有 `$SPELL_ID`**：9 份 `.decl` 全量检索 **0 处** `SPELL_ID`（th12 的相位派发不靠该变量）。

⇒ **th12 的 boss 相位选择靠两条 thprac 侧机制**：

**① 改容器 `call` 的名字串。** 每关都有一个 `Boss` 容器 sub（st1–st6 在 `stageNN.ecl`，st7 在 `stage07boss.ecl`），其末段是一条 **`call Boss1`（opcode `0x0B`）**；`THPatch` 用 `ecl << pair{off, val}` 直接覆写该 `call` 指令 `+0x14` 处的**名字串**，把目标改写成 `BossK`（或整串改写成 `BossCardK`）：

| 关 | 容器所在文件（槽） | `call "Boss1"` 指令 | 名字串偏移 | 数字字符偏移 |
| --- | --- | --- | --- | --- |
| ST1 | `stage01.ecl`（槽 0） | `0x0BCC` | `0x0BE0` | **`0x0BE4`** |
| ST2 | `stage02.ecl`（槽 0） | `0x0DDC` | `0x0DF0` | **`0x0DF4`** |
| ST3 | `stage03.ecl`（槽 0） | `0x0CB0` | `0x0CC4` | **`0x0CC8`** |
| ST4 | `stage04.ecl`（槽 0） | `0x3598` | `0x35AC` | **`0x35B0`** |
| ST5 | `stage05.ecl`（槽 0） | `0x0BD8` | `0x0BEC` | **`0x0BF0`** |
| ST6 | `stage06.ecl`（槽 0） | `0x0A2C` | `0x0A40` | **`0x0A44`** |
| ST7 | `stage07boss.ecl`（槽 2） | `0x0974` | `0x0988` | **`0x098C`** |

- 例（ST6）：`pair{0x0A44, (int8_t)0x32}` = 把 `"Boss1"` 的 `'1'` 改成 `'2'` ⇒ `"Boss2"`（`TH12_ST6_BOSS3`/`BOSS4`）；`0x35`/`0x36` = `'5'`/`'6'`（`BOSS9`/`BOSS10`）。
- 例（ST2 BOSS5）：`pair{0x0DF4, 'draC'} << pair{0x0DF8, 0x33} << pair{0x0DEC, 0xC}` = 把 `"Boss1"` **整串改写成 `"BossCard3"`**（`'draC'` 小端 = `C a r d`）——即**也可以直接选中符卡 sub**。
- 例（ST7 END_S10）：`pair{0x098C,'1'} << pair{0x098D,'0'}` = 两字节拼出 `"Boss10"`。
- 证据（离线解码）：`stage06.ecl:0x0A40` = `"Boss1\0"`、`stage07boss.ecl:0x0988` = `"Boss1\0"`（名字串紧邻 `call` 指令 `+0x14`）。

**② 改 `lifeSet` / HP 阈值，逼 `setNext` 立即转移。** 相位推进本由容器内的 **`setNext(slot, hp, time, name)`（opcode `0x0202`）按 HP 阈值触发**；thprac 用 **`ECLSetHealth(ecl, off, t, hp)`（opcode `0x019B`）** 或 `pair{off, hp}` 把 HP 直接压到阈值以下，使 `setNext` **当帧生效** ⇒ 相位当场推进到下一段（`BossCardK`）。例：`TH12_ST6_BOSS4` 的 `ECLSetHealth(ecl, 0x2D04, 0x0, 0x960)`；`TH12_ST7_END_S10` 的 `ECLSetHealth(ecl, 0x7B00, 0, 4600/3000/1150)`（按 `thPracParam.phase` 选 HP）。

> 两条机制**常组合使用**：① 把容器 `call` 改到目标相位（`BossK`），② 再把该相位内的 HP 压到阈值以下，使其立刻 `setNext` 到目标符卡 —— 这正是 §2.3 机制注记 / §3.2 里「过渡 sub」风险的来源。

### 2.3 `ST6 BOSS9/BOSS10 = BossCard5/BossCard6`（**2026-10-17 修正**）

CSV 中 ST6 的 `BOSS9`/`BOSS10` 入口**由前值 `Boss5`/`Boss6` 修正为 `BossCard5`/`BossCard6`**。三条独立证据：

1. **thprac 自己的分栏**：`thprac_locale_def.cpp` 的 `TH12` 命名空间里，`th_sections_cbt[7][2][14]`（`thprac_locale_def.cpp:16572` 起；`[5]` 即 **ST6**）把 `TH12_ST6_BOSS9` / `TH12_ST6_BOSS10` 归在**第 2 栏（符卡栏）**，与 `TH12_ST6_BOSS2/4/6/8` 同栏：
   ```text
   th_sections_cbt[5][0]（非符）= MID1, BOSS1, BOSS3, BOSS5, BOSS7
   th_sections_cbt[5][1]（符卡）= BOSS2, BOSS4, BOSS6, BOSS8, BOSS9, BOSS10
   ```
2. **相位链的第 5/6 个 `BossCardK`**：ST6 相位链为 `Boss1→BC1→Boss2→BC2→Boss3→BC3→Boss4→BC4→Boss5→BC5→Boss6→BC6`（§1.2）⇒ 第 5/6 个符卡 sub 正是 **`BossCard5` / `BossCard6`**（`Boss5`/`Boss6` 是第 5/6 个**非符**容器）。
3. **`.decl` 的 `spell()` 名逐条对上圣白莲符卡**：`stage06.decl` 中 `BossCard5()` 用 `spell2(93,…)`/`spell3(94,…)`/`spellEx(95,…)`（符卡名 =「圣尼公之大空卷轴」/「超人「圣白莲」」），`BossCard6()` 用 `spell(96, 7200, 500000, …)`（=「飞钵「飞行幻想」」）；与 `thprac_games_def.json` 的 `TH12_ST6_BOSS9`（`"spell": 1`；`!NH`=「圣尼公之大空卷轴」/`!L`=「超人「圣白莲」」）、`TH12_ST6_BOSS10`（`"spell": 1`；`!EN`=「飞钵「飞行幻想」」/`!HL`=「传说的飞空圆盘」）**逐条对应**。

> 🟡 机制注记：`TH12_ST6_BOSS9` 的补丁是 `pair{0x0A44,'5'}`（容器 `call` → `"Boss5"`，**非符**）+ 在 `Boss5` 内的 `ECLJumpEx(ecl, 0x9030, 0x9508, 0)`（跳到 `Boss5` 尾部、**当帧** `call BossCard5`）；`BOSS10` 同理（`'6'` + `0x98F8→0x99AC`）。即**当帧落点是符卡 sub**，与 CSV 登记一致；`Boss5`/`Boss6` 仅是过渡（见 §3.2 风险）。

---

## 3. 实现方案影响（本次重点）

### 3.1 段落 sub 是 **async 派生线程** ⇒ 必须遍历宿主线程链 `H+0x1030`

（引报告 §Q9 / §10.3。）

- th12 的 `main` **不用同步 `call` 进入段落**，而是用 **opcode `0x0F`（`@MainSubNN() async`）** 把每个段落 sub **派生为独立线程**：
  ```text
  // stage01.decl 的 main（截选）
  @MainSub00() async;   // opcode 0x0F
  wait(300);
  @MainSub01() async;
  wait(590);
  @MainSub02() async;   // ← warp 落点之一
  ...
  @MainSub10b();        // ← 无 async ⇒ opcode 0x0B（同步 call）
  ```
- 每关 `Boss` / `MBoss` 容器亦由 **`enmCreateA("Boss"/"MBoss", …)` 另起独立宿主**（见 §3.2），故**主线线程的 `T+4` 全程停在 `main`**。
- ⇒ **只读主线线程无效**：必须**遍历宿主线程链 `H+0x1030`**（头节点内嵌于 `H+0x1030`，节点 `{+0 = T, +4 = next, +8 = prev}`），对**每个线程**取 `T+4` → 经 `table[i].+4+0x10` 的**地址区间表**反查 sub 名 → 命中白名单即置「当前段落」（latch，未命中 no-op）。
- 关键链（与报告 §1 / §10.3 一致）：`mgr = *(u32*)0x4B43DC` → `res = *(mgr+0x64)`（≡ `*(u32*)(H+0x102C)`）→ `count = res+8` → `table = res+0x8C`（项 8B）；宿主捕获点 `0x4228BE` / `0x4229D2`（`EAX` = 宿主 `H`，各 5B EHOOK）。
- 采样点：**帧间稳定态**（IGI 渲染）即可；`T+4 == 0` ⇒ 该线程已结束（跳过）。
- ⚠️ **2026-10-18 修订（重要）**：上面「遍历宿主线程链 `H+0x1030`」只覆盖**主线宿主**那一份线程链 —— **boss 行读不到**（boss sub 跑在 boss 宿主自己的线程链上）。正确读取面 = **外层遍历全局宿主链 `mgr+0x68`**（节点 `H+0x12C0`，`+0` = H、`+4` = next）→ 对每个宿主再走其 `H+0x1030` 线程链。定案见 `th12_ecl_re_report.md` **§5.4**（Q-A 全局宿主链 / Q-B 捕获点 `0x4157AF` / Q-C 同一张 `res` 表），伪码见该节 §5.4.4。

### 3.2 boss 容器是独立宿主（`enmCreateA`）⇒ **必须改走全局宿主链**（2026-10-18 定案）

> **结论（先读）**：boss 宿主**在** `mgr+0x68` 这条宿主链上（**全局全宿主链**），所以**不需要**新的捕获点 / 备选钩子；但读取面**不能只遍历主线宿主的线程链** —— 必须**外层遍历全局宿主链、里层再遍历每个宿主的 `H+0x1030` 线程链**。定案与证据见 `docs/th12/th12_ecl_re_report.md` **§5.4**（Q-A / Q-B / Q-C）。

- 每关 `main` 里 boss / midboss 由 **`enmCreateA("Boss"/"MBoss", …)`** 创建（例：`stage06.decl` 的 `enmCreateA("MBoss", …)` / `enmCreateA("Boss", …)`；对应 `.ecl` 偏移 `0x1A4A0` / `0x1A508`，opcode `0x0101`）——**boss 是另一个 ECL 宿主**；`THPatch` 的 boss 行正是把 `main` 跳到该 `enmCreateA` 指令上。
- **✅ Q-A（缺口「坐实」的一半）**：boss sub 跑在**boss 宿主**自己的线程链 `H_boss+0x1030` 上，**主线宿主的线程链上没有它们** ⇒ §3.1 原先的「只遍历主线宿主的 `H+0x1030`」**读不到 boss 行**。
- **✅ Q-A（缺口「消解」的一半）**：`mgr+0x68` 那条链是**全局全宿主链** —— 工厂 `FUN_00412990` 在构造之后**无条件**把节点 `H+0x12C0` 挂上（`0x412B3E`–`0x412B6D`，**与名字串无关**），而 boss / midboss 宿主由 `enmCreateA` 经**同一个工厂**创建 ⇒ **boss 宿主就在链上**。⇒ **不需要**新捕获点；§3.2 旧版「必要时在 `spawn` / `enmCreateA` 处加**备选钩子**」**不再必要**。若确实要事件驱动，可用 `enmCreateA` 的工厂调用点 `0x4157AA` ⇒ EHOOK **`0x4157AF`**（5B，`EAX` = 新宿主），但它只是可选辅助。
- **✅ Q-C**：boss 宿主的 `res` 与主线**是同一个对象、同一张表**（构造器对每个宿主写 `H+0x102C = *(mgr+0x64)`；`res+0x8C` 由 `ECLI` 头处理器 `FUN_00412DD0` 把同一次进关的多个 `.ecl` **合并**进一张表）⇒ **不需要**为 boss 另建地址区间表（§3.3 的「随缓冲切换重建」仍整体适用）。
- **仍存在的 🟡 过渡 sub 风险**：符卡行靠「① 改容器 `call` 名 → ② 设 HP = `setNext` 阈值」逼转移；**转移前会先经过容器 sub（`BossK` 之类）**。若采样恰好落在转移前的那一帧，latch 会先读到**过渡 sub**（可能不在白名单 ⇒ no-op，也可能命中相邻行）。⇒ **建议**：采样放帧间稳定态（同一帧内 `t=0` 的指令会连跑，最终停在下一个 `t>0` 的指令上），并对「同一帧内多个候选」取**最靠近白名单**/最后一次命中。
- 该风险同时解释了 §2.3 的机制注记：`BOSS9`/`BOSS10` 的**当帧**落点是 `BossCard5`/`BossCard6`，而容器 `Boss5`/`Boss6` 是它的过渡。

### 3.3 基址 / 区间表**不可跨关缓存**

- `table[i].+4+0x10` 升序建区间（表按**名字字典序**存放，地址**不单调**，**必须显式排序**）。
- 跳章 / 换关后运行期缓冲切换（`res+0xC+slot*4`）时表可能被**重建**（加载器 `FUN_004692E0` 累加 `res+8`）⇒ **地址区间表须随之重建**，否则读到脏名字。

---

## 4. 风险 / 待真机清单

> 本机无法运行 th12；以下**全部未做**，状态一律 🟡。

- **R1 全静态**：本文所有入口 / 偏移均来自 `.decl` + `.ecl` 离线解码与 `thprac_th12.cpp` 对读，**未真机三态**。
- **R2 段落 sub 走异步线程（本作最关键的一条）**：进关后主线线程（`*(u32*)(H+0x1030)`）的 `T+4` 应**长期落在 `main`**；线程链上应能观察到各 `MainSubNN` 线程。**若观察到主线线程自己进入 `MainSubNN`，则 §3.1 的 async 结论需订正。**
- **R3 短命 worker**：`MainSubNN` 多为短命 worker（例 `stage01` 的 `MainSub02` 约 200 帧）；latch 只在「**看见**」入口 sub 的那一帧置值 ⇒ 采样频率须 ≥ 1 次/帧；若偶发漏采，启用 `spawn` 钩子备选（报告 §10.5）。
- **R4 `ST7 END_S9/END_S10` 登记为容器 sub**（`Boss9`/`Boss10`）：与 ST6 `BOSS9/BOSS10` 登记**符卡** sub 的口径**不同**。离线证据：`END_S9` 的 `ECLJumpEx(ecl,0x71DC,0x725C,0)` 落在 `Boss9` 尾部，其后依次是 `moveLimit` → `movePosTime` → **`wait(120)`** → `call BossCard9`（即 `Boss9` 是一个**持续约 120 帧的真实相位**）；`END_S10` 的 `0x106C` 则**直接**是 `call BossCard10`。⇒ 两行取值口径需人工确认（统一为符卡 sub，或维持容器 sub）。**本轮未改 CSV。**
- **R5 `ST6 BOSS9/BOSS10` 过渡 sub**：见 §3.2；真机确认「是否会在 `Boss5`/`Boss6` 上多采一帧」。**2026-10-18 更新**：boss 宿主可达性已定案（报告 `th12_ecl_re_report.md` §5.4）—— **缺口坐实**（主线宿主的线程链读不到 boss sub）**但可经全局宿主链 `mgr+0x68` 读到**，**不需要新捕获点 / 备选钩子**；读取面改为「全局宿主链 → 每宿主 `H+0x1030`」后，本条的「过渡 sub」风险仍然存在，仍需真机确认多采帧。<br>**新增待真机项**：全局宿主链能否稳定枚举（`mgr+0x70` 计数 vs 实走节点数一致；boss 战期间链上能观察到入口 sub 为 `Boss`/`MBoss` 的宿主），以及退关/重开时旧宿主是否已摘链（`FUN_00412BE0`）。
- **R6 地址区间表**：见 §3.3（须随缓冲切换重建）。
- **R7 练习跳章**：`ins[0xA] = 0xFF` 的难度掩码在 E/N/H/L 四难度下是否都能执行（否则 `goto` 会被 `test byte [edi+0x101C], al` 跳过）。
- **R8 `ST6` 分支 `setNext`**：`BossCard4` 有 `"Boss5"`/`"Boss6"` 两条 `setNext` 分支；两分支入口均已收录，真机各走一遍确认。
- **R9 变体**：Steam / 汉化版是否共用同一 RVA（本文偏移来自原版 `ts=1251344348`）。

---

## 5. 与其它文档的关系

- **口径 / Q9 / 粒度** → `docs/logger/logger_re_disasm_overview.md` §1.2 / §6 Q9 / §9.9。
- **回填流程** → `docs/logger/checkpoint_entry_backfill_workflow.md`（§3.3 章节行 / §3.4 boss 行；**注意本文 §2.1 的口径差异**）。
- **本作指针链 / 命中点 / 判定口径** → `docs/th12/th12_ecl_re_report.md`（Q1–Q9；§10.4 已收口并指向本文）。
- **数据** → `docs/logger/th12_checkpoints.csv`（本文 §1 即其第 1 列）。
- **同批建档** → `docs/th10/th10_checkpoint_entry_report.md`、`docs/th11/th11_checkpoint_entry_report.md`、`docs/th13/th13_checkpoint_entry_report.md`。

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-17 | **建档（收口）**。把 `docs/logger/th12_checkpoints.csv` 第 1 列的 **114 行**（章节 43 + boss 71）整理为入口表小结：**入口表按关分组**（§1，每关先道中行、再 boss 行；含 `portion 1 = MainSub00` 各关一致的定案、`ST3 后半 #1 = MBoss1`、`ST5` 后半序号非单调等注记）；**相位链**（§1.2，逐关 `setNext` 链）。新增**本作特有的两条机制**：**§2.1 两个 warp API**（`ECLJump` **无 `dest`** = 原地跳过、`ECLJumpEx` 的 **`dest` 才是落点**；本作 warp **一律 `Ex`**，`ECLJump` 0 次调用；并注明与回填工作流 §3.1 的口径差异）；**§2.2 boss 行不适用「`stNNbs`/`stNNmbs` + `$SPELL_ID`」**（th12 无独立 boss 脚本文件——st1–st6 内联于 `stageNN.ecl`、仅 ST7 有 `stage07boss.ecl`；无 `$SPELL_ID`；相位选择靠 ① 改容器 `call` 名字串 ② 改 `lifeSet` 操作数逼 `setNext` 立即转移，附逐关名字串偏移表）。**§2.3 记入 `ST6 BOSS9/BOSS10 = BossCard5/BossCard6`（2026-10-17 修正，前值 `Boss5`/`Boss6`）**，证据 = `th_sections_cbt[5]` 符卡栏 + 相位链第 5/6 个 `BossCardK` + `.decl` `spell()` 名对上圣白莲符卡。**§3 实现影响**（async 派生线程 ⇒ 必须遍历宿主线程链 `H+0x1030`；boss 容器独立宿主 `enmCreateA` ⇒ 过渡 sub 风险；区间表不可跨关缓存）。**§4 风险清单**（含 R4：ST7 `END_S9/END_S10` 登记容器 sub 与 ST6 口径不同，待确认）。状态：**静态收口（🟡 已枚举、未真机三态）**。 |
| 2026-10-18 | **boss 宿主可达性定案回填**（依据 `docs/th12/th12_ecl_re_report.md` **§5.4** 新增节，Q-A/Q-B/Q-C）。**§0 第 9 行**补注「宿主链 = 全局全宿主链 ⇒ 不需要新捕获点」。**§3.1** 追加修订：只遍历**主线宿主**的 `H+0x1030` **读不到 boss sub** ⇒ 读取面改为「外层全局宿主链 `mgr+0x68`（节点 `H+0x12C0`，`+0`=H / `+4`=next）→ 内层每宿主 `H+0x1030`」。**§3.2 重写**：原「boss 容器是独立宿主 ⇒ 可能先采到过渡 sub；必要时加备选钩子（未验证）」改为 —— **缺口坐实**（boss sub 在 boss 宿主线程链上，主线线程链读不到）**但缺口消解**（工厂 `FUN_00412990` 无条件把所有宿主挂上 `mgr+0x68`，boss 宿主经同一个工厂创建 ⇒ 遍历宿主链即可读到，**无需新捕获点 / 无需备选钩子**）；同时给出可选的 `enmCreateA` 捕获点 **`0x4157AF`**（RVA `0x157AF`，5B，`EAX` = 新宿主；`call 0x412990` @ `0x4157AA` 的下一条）。补 Q-C 结论：boss 宿主的 `res` 与主线**同一对象、同一合并表**（`H+0x102C = *(mgr+0x64)`；`ECLI` 头处理器 `FUN_00412DD0` → 加载器 `FUN_004692E0` 合并）⇒ 不需要为 boss 另建地址区间表。**§4 R5** 同步：保留「过渡 sub 多采帧」待真机项，并新增「全局宿主链枚举稳定性 / 退关摘链」待真机项。**未真机（🟡）**。 |

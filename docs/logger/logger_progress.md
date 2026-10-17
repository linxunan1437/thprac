# logger 需求进度监控（活文档）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（总括，含术语表 §2、Q1–Q9 §6、锚点总表 §5）。
> 需求来源：`docs/req_logger.md`。
> 范围：**监控**（实时显示「当前段落」）已落地 th14–th18；**日志记录**（JSONL 落盘）已于 2026-10-17 接入 th14–th18（见 `docs/logger/logger_impl_report.md`），**真机未测**；**th06–th08 已 E0 建档**（2026-10-16，A 代口径见 `docs/thXX/thXX_ecl_re_requests.md`），th09 需求明确不支持、th095 不在范围。
> 工作流：`docs/logger/checkpoint_csv_workflow.md`（CSV 骨架）、`docs/logger/checkpoint_entry_backfill_workflow.md`（入口回填）。
> 素材来源：`docs/logger/decompile_materials.md`。
> 本机为 **Windows**（VS 在 `D:\tools\VS`），**可编译、可真机运行**——旧描述「macOS 不可编译」已作废，以 `docs/th16/th16_monitor_impl_report.md` 为准。**th16 已完成真机验证**（除完整全关外全部通过）；其余作仍为**静态定案（🟡 未真机三态）**，方法为只读反汇编（`pefile` + `capstone`）+ 离线 ECL 解码。
> 地址口径：`VA` 与 `RVA = VA − 0x400000`。本文不新增地址，地址以总括 / 逐作报告为准。
> **维护约定**：本文是进度**唯一事实来源**；每次「建档 / 逆向 / CSV 回填 / 真机验证」后，更新 §1 总表与本文 §6 变更记录。逐作细节见各作文档。

---

## 0. 口径与图例

**阶段定义**（每个作品按序推进）：

| 代号 | 阶段 | 产出 |
| --- | --- | --- |
| E0 | requests 建档 | `docs/thXX/thXX_ecl_re_requests.md` |
| E1 | ECL 逆向报告（道中/章节，Q1–Q9） | `docs/thXX/thXX_ecl_re_report.md` |
| E2 | checkpoint CSV 骨架（章节行 + boss 行） | `docs/logger/thXX_checkpoints.csv` |
| E3 | 章节行「段落入口函数」回填 | CSV 第 1 列（章节行） |
| E4 | boss 行「段落入口函数」回填 | CSV 第 1 列（boss 行）+ `docs/thXX/thXX_boss_phase_report.md` |
| E5 | 真机三态验证 | 真机采样记录（本机无法做） |

**状态图例**：✅ 完成 ｜ 🟡 部分完成 / 静态定案未真机 ｜ ❌ 未开始 ｜ ➖ 不适用。

**「日志落盘」列**（2026-10-17 新增）：指 `docs/req_logger.md` 的**日志记录**迭代，与上面 E0–E5 的「监控」阶段并列 —— `➖ 不适用`（监控未落地，无从接线）｜`❌ 未接线`｜`🟡 已接线未真机`｜`✅ 真机通过`。
**「测试情况」列**：指该作**日志落盘**的真机测试情况 —— `—`（不适用）｜`未测`｜`部分通过`｜`全通过`。与「E5 真机」列（监控侧）相互独立。

> 「完成」仅指**静态定案**——按 AGENTS.md §2.5，未真机验证的地址不得当作结论；E5 未做前，E1/E4 的 ✅ 均为 🟡 语义（**th16 例外**：E5 已做，故其 E1/E4 记真 ✅）。

---

## 1. 进度总表（结论速览）

> CSV 填/空由 §5 脚本实算（2026-10-16）。`章节行` 记录名形态 = `STn #k` / `STn 前半 #k` / `STn 后半 #k`，其余为 `boss 行`。
> **th06–th08（A 代）尚未生成 CSV**，不在 §5 统计范围内；其「段落入口」列口径已改为**时间值 / 指令偏移**（非 sub 名，见各作 requests §11）。

| 作 | E0 建档 | E1 逆向报告 | E2 CSV 骨架 | E3 章节行回填 | E4 boss 行回填 | E5 真机 | 日志落盘 | 测试情况 | 综合档位 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **th06** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ | ➖ | — | A 代建档，**逆向未跑** |
| **th07** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ | ➖ | — | 同上 |
| **th08** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ | ➖ | — | 同上（A 代线索最多） |
| **th10** | ✅ | ✅🟡 | ✅ 41+68=109 | ❌ 0/41 | ❌ 0/68 ➖无 boss 报告 | ❌ | ➖ | — | 逆向完成，**白名单未枚举**；st1–st3 段入口在 `main` 内（粒度待决 §4.10） |
| **th11** | ✅ | ✅🟡 | ✅ 37+88=125 | ❌ 0/37 | ❌ 0/88 ➖ | ❌ | ➖ | — | 逆向完成，**白名单未枚举**；段入口多为 `async` 派生线程；st4 分支关已论证兼容 |
| **th12** | ✅ | ✅🟡 | ✅ 43+71=114 | ❌ 0/43 | ❌ 0/71 ➖ | ❌ | ➖ | — | 逆向完成，**白名单未枚举**；**Q5 命中点自找** `0x4671A0`；主线 `T+4` 长期停在 `main` |
| **th13** | ✅ | ✅🟡 | ✅ 44+72=116 | ❌ 0/44 | ❌ 0/72 ➖ | ❌ | ➖ | — | 逆向完成，**白名单未枚举** |
| **th14** | ✅ | ✅🟡 | ✅ 43+80=123 | ✅ 43/43 | ✅ 80/80 | ❌ | 🟡 已接线 | 未测 | **CSV 全表回填**；ST4 拆 A/B 路线（boss 8→16 行） |
| **th15** | ✅ | ✅🟡 | ✅ 46+73=119 | ✅ 46/46 | ✅ 73/73 | ❌ | 🟡 已接线 | 未测 | **CSV 全表回填**；删 `ST6 #3`（与 `ST6 STARS` 同一段） |
| **th16** | ✅ | ✅ | ✅ 39+71=110 | ✅ 39/39 | ✅ 71/71 | 🟡 除全关外全通过 | 🟡 已接线 | 未测 | **监控功能完成**；仅剩全关测试 |
| **th17** | ✅ | ✅🟡 | ✅ 39+66=105 | ✅ 39/39 | ✅ 66/66 | ❌ | 🟡 已接线 | 未测 | **CSV 全表回填**；双分支统一按 `th_sections_cbt` 符卡口径 |
| **th18** | ✅ | ✅🟡 | ✅ 49+65=114 | ✅ 49/49 | ✅ 65/65 | ❌ | 🟡 已接线 | 未测 | **CSV 全表回填** |

**整体回填率（第 1 列非空 / 总行数）**：

| 作 | th10 | th11 | th12 | th13 | th14 | th15 | th16 | th17 | th18 | 合计 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 已填/总 | 0/109 | 0/125 | 0/114 | 0/116 | 123/123 | 119/119 | 110/110 | 105/105 | 114/114 | **571/1035** |
| 空值率 | 100% | 100% | 100% | 100% | 0% | 0% | 0% | 0% | 0% | **45%** |

> 九作 CSV 均已生成（合计 **1035 行 = 章节 381 + boss 654**）；其中 **th14 / th15 / th16 / th17 / th18 已全表回填**。
> 相对骨架的两处**有意的行数变动**：th14 ST4 拆 A/B 路线 → boss 行 **+8**（115→123）；th15 删 `ST6 #3`（与 `ST6 STARS` 同为 `MainSub03`，thprac 章节划分问题）→ 章节行 **−1**（120→119）。

---

## 2. 分档小结

- **档 A′：A 代建档（th06 / th07 / th08）** —— 2026-10-16 新建 `_ecl_re_requests.md`（三作齐全）。与 B/C 代模板的**主要差异**：章节跳转是「**时间轴**」而非 sub 跳转（th06 `ECLWarp(time)` / th07 `ECLTimeWarp(count,time)` / th08 `ECLWarp(time,offset,…)`，落点分别是 `0x5a5fb0` / `[0x9a9af8+0x9545fc]` / `0xf54cf8`），故 **Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一**；checkpoint「入口」列改用**时间值 / 指令偏移**。**无 CSV、无逆向**；A 代还**没有 eclmap**、`.ecl` 未从 `.dat` 解出。
  - A 代 ECL 格式里**确实有带 id 的 sub**：th07 `ECLCallSub` opcode `0x29`、th08 opcode `0x34`、th06 有 `EclRawInstrCallArgs.eclSub` 脚手架（未接线）—— 但**运行时是否暴露「当前 sub」未知**，正是 Q1/Q2 待答项。
- **~~档 A：仅建档（th10 / th11 / th12）~~ 已消解（2026-10-16）** —— 三作均已产出 `_ecl_re_report.md` 并回填 requests，**升入档 B**；原「无报告、CSV 全空」描述作废。
- **档 B：逆向完成、白名单未枚举（th10 / th11 / th12 / th13）** —— 四作 `_ecl_re_report.md` 均已静态定案 Q1–Q9（命中点、`res+0x8C` sub 表、宿主捕获点等），但**报告自陈「段落入口白名单尚未枚举」**，四作 CSV（109 / 125 / 114 / 116 行）**全空**，且**均未新建 boss 报告**。
  - **B 代三作共性（th10 / th11 / th12，2026-10-16 收口）**：`[T+4]` = **当前指令绝对地址**（非 sub 索引，与 th13 同类；C 代 th15–18 才是索引）；取「当前 sub」须**遍历宿主线程链 `H+0x1030`** —— 段入口常以 `@MainSubNN() async` 派生为**独立线程**，主线 `T+4` 可能长期停在 `main`（th12 尤甚），且 `H+4` 会被本帧最后执行过的 async 线程覆盖。
  - **th12 额外**：原「在 thprac 中**没有任何 ECL 解释器 hook**」→ **已自找定案** `0x4671A0`(6B, `EDI` = 执行对象)；th13 亦有 `0x4772D2`(6B, `EBX`)。
  - **th10 额外（待决）**：st1–st3 的段落入口**全部落在 `main` 内部**（`.decl` 佐证无 `MainSubNN`，道中为内联区段），若成立则需 `(sub 名, 指令偏移)` 二级粒度 ⇒ 影响总括 §1.2 口径，见 §4.10。
  - **B 代细分建议（待决，由 th12 报告提出）**：把 B 代细分为 **B1 = th12/th13**、**B2 = th10/th11**；但 th11 帧长/遍历步长未定案，**该细分待补证**（总括 §4 注记）。
- **档 C：逆向 + boss 报告完成、CSV 全表回填（th14 / th15 / th17 / th18）** —— 2026-10-16 四作**章节行 + boss 行全部回填**（th14 123/123、th15 119/119、th17 105/105、th18 114/114），口径＝`checkpoint_entry_backfill_workflow.md`（warp 落点 → ECL sub，async 照挂；boss 行＝`THPatch` 的 `SetFile`/`ECLJump` + `pair{}` 名字串补丁）。四作均**无真机**（E5 ❌）。
  - **遗留**：th14 ST4 的 A/B 双路线已拆为 16 行（`ST4A`/`ST4B`，B 路取 `st04mbs2`/`st04bs2` 的 `MBossB*`/`BossB*`）；th15 `ST7 前半 #5` 与 `ST7 后半 #1` 入口同名（`MainSub04`）→ 登记于 `knownBugs.md` #1（P2）。
- **档 D：功能落地（th16 监控；th14–th18 日志落盘）** —— th16 是唯一跑通**监控**全链路的一作：CSV 全表回填（110/110）、源码实现（`thprac_th16.cpp` 内的 `TH16Logger` 模块 + 生成物）、**真机验证除完整全关外全部通过**；剩余：全关测试（E5 收尾）。**日志落盘**（2026-10-17）已接入 **th14 / th15 / th16 / th17 / th18** 五作（共享模块 `thprac_games.{h,cpp}` + 各作 8 处调用，JSONL 月度文件），Release|x86 编译通过，**真机未测**。详见 `docs/logger/logger_impl_report.md`。

---

## 3. 逐作明细与缺口

| 作 | 报告状态标记 | 已回填范围 | 留空原因 |
| --- | --- | --- | --- |
| th06 | E0 建档（无报告） | — | 逆向未跑；A 代无 eclmap、`.ecl` 未从 `.dat` 解出 |
| th07 | E0 建档（无报告） | — | 同上 |
| th08 | E0 建档（无报告） | — | 同上 |
| th10 | 🟡 静态定案 | — | 白名单未枚举；**st1–st3 段入口落在 `main` 内**（粒度待决，见 §4.10） |
| th11 | 🟡 静态定案 | — | 白名单未枚举；段入口多为 `async` 派生线程（须遍历宿主线程链 `H+0x1030`） |
| th12 | 🟡 静态定案 | — | 白名单未枚举；Q5 命中点自找 `0x4671A0`；主线 `T+4` 长期停在 `main` |
| th13 | 🟡 静态定案 | — | 白名单未枚举（`.decl` 素材已在机，可离线枚举） |
| th14 | 🟡 静态定案 / 静态调研 | 章节 43/43、boss 80/80（全表 123/123） | —（ST4 拆 A/B 路线为 16 行：A＝朴素 `st04mbs/st04bs`，B＝`st04mbs2/st04bs2`） |
| th15 | 🟡 静态定案 / 静态调研 | 章节 46/46、boss 73/73（全表 119/119） | —（删 `ST6 #3`：与 `ST6 STARS` 同为 `MainSub03`；`ST7` 同名入口见 `knownBugs.md` #1） |
| th16 | ✅ 真机验证（除全关） | 章节 39/39、boss 71/71（全表 110/110） | — |
| th17 | 🟡 静态定案 / 静态调研 | 章节 39/39、boss 66/66（全表 105/105） | —（`keepSpellDrops` 双分支类行统一按 `th_sections_cbt` 符卡口径） |
| th18 | 🟡 静态定案 / 静态调研 | 章节 49/49、boss 65/65（全表 114/114） | — |

> 各作报告细节：`docs/thXX/thXX_ecl_re_report.md`、boss 报告 `docs/thXX/thXX_boss_phase_report.md`（th14–th18）。
> 报告普遍声明「未修改任何既有文件，回填留待收口任务」——见 §4 不一致清单第 1 条。

---

## 4. 缺口 / 不一致清单

1. **总括 §5.1 与逐作报告未同步 —— 部分收口（2026-10-16）**：**th10–th13 已同步**（含 th13 那行命中点由 ❌ 改为 `0x4772D2`/6B/`EBX`、三行 sub 名线索 ❌→✅）；**th14/16/17/18 仍未同步**（报告已确认名字表，总括仍写「待确认 / 待定」）。
2. **工作流文档口径过期（仍未收口）**：`checkpoint_csv_workflow.md:159`、`checkpoint_entry_backfill_workflow.md`（§3.4 / §6）仍写「其余作及 boss 行仍为空 / boss 行流程暂缓」（2026-10-15 口径），与 2026-10-16 已全表回填 th14/15/17/18 的实际冲突。两处**本次未改**（避免夹带）。
3. **th10–th13 报告完成但 CSV 未回填**：四作报告全套 Q1–Q9 定案，CSV（109 / 125 / 114 / 116 行）**全空**（自陈未枚举）。
4. ~~**th14 章节行 0、th17/th18 章节行 0**~~ → **已消解（2026-10-16）**：th14/15/17/18 章节行与 boss 行**全部回填**，整体回填率 **312/1028（70% 空）→ 571/1035（45% 空）**。剩余空白**全在 th10–th13**（逆向完成、白名单未枚举）。
5. ~~**th15 回填最少**~~ → **已消解（2026-10-16）**：th15 全表 119/119（参照作已补齐）。
6. ~~**th06–th08 完全空白**~~ → **2026-10-16 部分收口：三作已 E0 建档**。`docs/th06|th07|th08/thXX_ecl_re_requests.md` 已建（A 代口径，Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一）；但 **E1 逆向未跑、E2 CSV 未生成**，且 A 代**无 eclmap**、`.ecl` 未从 `.dat` 解出、`reasm/exes/` 缺 `th06.exe`（见各作 requests §12）。总括 §0/§4/§5.1 已同步。
7. ~~**日志（JSON 落盘）功能整体未起步**~~ → **已部分落地（2026-10-17）**：共享模块 `thprac_games.{h,cpp}`（`#pragma region Game Session Log`）+ **th14 / th15 / th16 / th17 / th18** 各 8 处接线；JSONL 一行一局、月度文件 `logger_sessions_YYYYMM.jsonl`；口径见 `docs/req_logger.md` §落地口径、实现见 `docs/logger/logger_impl_report.md`。**Release|x86 编译通过；五作真机均未测**（按用户安排，功能补齐后统一验证）。**未接线**：th10–th13（白名单未枚举）、th06–th08（逆向未跑）。th18 的机体类型字段留空（无现成来源，见 `docs/logger/logger_impl_report.md` §8 第 6 条）。
8. ~~**源码侧无实现**~~ **部分消解：th16 已实现**（`thprac/src/thprac/thprac_th16.cpp` 内的 `TH16Logger` 模块 + 生成物 `thprac_th16_checkpoints_gen.cpp`，见 `docs/th16/th16_monitor_impl_report.md`）；**其余八作仍无实现**。既有 warp（`ECLHelper::SetBaseAddr`/`ECLJump`）与「freeze ECL sub time」hook（`thprac_th10.cpp:541`、`thprac_th14.cpp:590`、`thprac_th18.cpp:1202`）均不读函数名。原型 `thprac_th15_missbomb.{h,cpp}`（含 `GetEclSubName()`）仅在 **`feat/plogth15`** 分支、**未合并**，且因落盘崩溃被弃用。
9. **E5 真机验证：th16 已做（除完整全关），其余八作全部未做**。th16 的 `docs/th16/th16_monitor_impl_report.md` §7 / §7.1 清单已逐项通过，仅剩全关测试；其余作结论仍为 🟡 静态定案，AGENTS.md §2.5 要求真机验证后方可作结论。
10. **B 代「段落入口」粒度待决（2026-10-16 新增，阻塞 B 代 E3/E4）**：th10 的 st1–st3 **段落入口全部落在 `main` 内部**（`.decl` 佐证：无 `MainSubNN`，`main` 是覆盖全关的 ~0x1EB4 B 超长脚本，道中为内联区段），仅 st4–st7 多为独立 `MainSubNN`；若成立，总括 §1.2「每个 checkpoint 只登记 1 个段落入口函数」对 th10 st1–st3 **不适用**，或需改登记 **`(sub 名, 指令偏移)` 二级粒度**。参照：th12 已抽样 8/8 命中且入口是 `main` 内 `0x0F` 指令的**内嵌名** ⇒ 仍属 sub 名粒度；**th11 需同法复核**。详见总括 §9.9。

---

## 5. 复现统计（只读）

在仓库根执行（统计第 1 列非空的章节行 / boss 行）：

```bash
cd docs/logger
python3 - <<'PY'
import csv, re
CHAP = re.compile(r'^ST\d+ (#\d+|前半 #\d+|后半 #\d+)$')   # 章节行记录名形态；其余为 boss 行
print(f"{'work':5}{'chap_f':>7}{'chap_t':>7}{'boss_f':>7}{'boss_t':>7}{'all_f':>7}{'all_t':>7}")
for n in range(10, 19):
    cf = ct = bf = bt = 0
    for row in list(csv.reader(open(f"th{n}_checkpoints.csv", encoding='utf-8')))[1:]:
        if not row or not any(row):
            continue
        filled = row[0].strip() != ''
        if CHAP.match(row[3]):
            ct += 1; cf += filled
        else:
            bt += 1; bf += filled
    print(f"th{n:<3}{cf:>7}{ct:>7}{bf:>7}{bt:>7}{cf+bf:>7}{ct+bt:>7}")
PY
```

预期输出（2026-10-16 复算）：

```text
work  chap_f chap_t boss_f boss_t  all_f  all_t
th10       0     41      0     68      0    109
th11       0     37      0     88      0    125
th12       0     43      0     71      0    114
th13       0     44      0     72      0    116
th14      43     43     80     80    123    123
th15      46     46     73     73    119    119
th16      39     39     71     71    110    110
th17      39     39     66     66    105    105
th18      49     49     65     65    114    114
```

> 合计 **571/1035**（空值率 45%）。th14 的 `ST4A MID1` 等 A/B 行不匹配章节行正则，正确计入 boss 行。

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档。按 §1 的阶段定义（E0–E5）汇总 th10–th18 进度：E1 报告 th10–th12 缺、th13 完成但白名单未枚举、th14–th18 完成；CSV 第 1 列合计 241/1029（77% 空），th16 仅章节行、th14/17/18 仅 boss 行、th15 最少、th10–th13 全空。登记 §4 缺口（总括 §5.1 与逐作报告未同步、工作流文档口径过期、th06–08 空白、日志迭代未起步、源码无实现）。 |
| 2026-10-16 | **th06–th08 建档（E0）**：新建 `docs/th06\|th07\|th08/thXX_ecl_re_requests.md`（A 代口径）。核心差异：A 代章节跳转是「**时间轴**」（th06 `ECLWarp(time)`→`0x5a5fb0` / th07 `ECLTimeWarp(count,time)`→`[0x9a9af8+0x9545fc]` / th08 `ECLWarp(time,offset,…)`→`0xf54cf8`）而非 sub 跳转；**Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一**；checkpoint「入口」列改用**时间值 / 指令偏移**（与 th10–18 语义分叉）。登记 A 代素材缺口（无 eclmap、`.ecl` 未解出、`reasm/exes/` 缺 `th06.exe`）。§1/§2/§3/§4 同步；§5 统计脚本不变（这三作无 CSV）。 |
| 2026-10-16 | th16 真机反馈修 bug（**跨作共性**）：很多关卡「后半第一节」的 checkpoint 实为**增援章节**，主线程在**中boss 存活期间**就开跑（`.decl` 实证：`st01.decl` `main()` 在 `enmCreateA("MBoss",…)` 后 `wait(160)` 就 `@MainLatter()`），导致面板提前跳到 `STn 后半 #1`。th16 已修（`thprac_th16_logger.{h,cpp}`：按白名单键前缀分「中boss/关底boss/章节」，中boss 存活期间章节 checkpoint 只缓存，死亡 sub 名出现时补显示）。**同一模式在 th10/th11/th14/th17/th18/th20 都存在**（各作 `TH_TIMELOCK` 的 `bossExists` 钩子即佐证），移植到其他作时需按各作 `.decl` 归纳死亡 sub 名。细节见 `docs/th16/th16_monitor_impl_report.md` §5.7 / §7.1。 |
| 2026-10-16 | **th16 监控功能收口**：真机验证通过（§7 / §7.1 清单除完整全关外全部通过，含 §7.1 的中boss 窗口两个 flush 分支与卡死反例）。据此同步 §1 总表 / 回填率 / §2 分档 / §3 明细 / §4 缺口 / §5 复算：th16 改为 **E1 ✅、E4 ✅ 71/71、E5 🟡（仅剩全关）**，CSV 全表 110/110（原文档写的 39/111、boss 0/72 是回填前的旧数）；整体回填率 **241/1029（77% 空）→ 312/1028（70% 空）**；新增**档 D「功能落地」**；§4 第 8、9 条的「源码侧无实现 / E5 全部未做」按 th16 已落地改写。头部「本机 macOS 不可编译」的旧描述一并作废（本机为 Windows）。**th16 剩余：完整全关测试；其余八作仍为静态定案。** |
| 2026-10-16 | **B 代 th10 / th11 / th12 逆向收口（E1）**：三作新建 `docs/thXX/thXX_ecl_re_report.md`（Q1–Q9 静态定案）并回填各自 `_requests.md`。**§1 总表**：th10/11/12 的 **E1 ❌ → ✅🟡**、综合档位改「逆向完成，白名单未枚举」。**§2**：「档 A：仅建档」**消解**（三作升入**档 B**，档 B 扩为 th10–th13），并记 B 代三作共性（`[T+4]`=指令绝对地址；取当前 sub 须**遍历宿主线程链 `H+0x1030`**）与 th12 命中点自找 `0x4671A0`、B 代细分建议（待补证）。**§3**：三作明细改写（原「逆向未跑」作废）。**§4**：第 1 条记为**部分收口**（th10–th13 已同步，th14/16/17/18 未同步）、第 3 条扩为 th10–th13、**新增第 10 条**（th10 st1–st3 段入口落在 `main` 内 ⇒ 段落入口粒度待决，阻塞 B 代 E3/E4）。**回填率不变**（三作 CSV 未动：0/109、0/125、0/114；合计仍 **312/1028**）。同步总括 §4 / §5.1 / §9 / §10。**全部 🟡 静态定案，未真机。** |
| 2026-10-16 | **C 代 th14 / th15 / th17 / th18 CSV 全表回填（E3 + E4）**：四作按 `checkpoint_entry_backfill_workflow.md` 口径（warp 落点 → ECL sub；boss 行＝`THPatch` 的 `SetFile`/`ECLJump` + `pair{}` 名字串补丁）回填第 1 列，**章节 382→381、boss 646→654**，合计 **312/1028（70% 空）→ 571/1035（45% 空）**。**§1**：th14 记 **43+80=123 / E3 43/43 / E4 80/80**、th15 记 **46+73=119 / 46/46 / 73/73**、th17 记 **39/39 + 66/66**、th18 记 **49/49 + 65/65**，综合档位均改「CSV 全表回填」。**§2**：档 C 由「部分回填」改「全表回填」。**§3**：四作明细改写（原留空原因作废）。**§4**：第 2 条标注「仍未收口」、第 4/5 条**消解**。**§5**：预期输出更新。**行数两处有意变动**：th14 ST4 拆 A/B 路线（A＝朴素 `st04mbs/st04bs`，B＝`st04mbs2/st04bs2`）boss **+8**、记录名取 thprac 规范名 `ST4A`/`ST4B`；th15 删 `ST6 #3`（与 `ST6 STARS` 同为 `MainSub03`）章节 **−1**。新建 `docs/logger/knownBugs.md`（登记 th15 `ST7 前半 #5`/`后半 #1` 入口同名 `MainSub04`，P2）。th17 的 `keepSpellDrops` 双分支类行统一按 `th_sections_cbt` 符卡口径（ST2 BOSS5 `Boss3`→`BossCard3`、ST5 BOSS7 `Boss4`→`BossCard4`）。**四作仍 🟡 静态定案，未真机。** |
| 2026-10-16 | **th16 监控模块并入 `thprac_th16.cpp`（结构整理，3 文件 → 2）**：`TH16Logger` 原为独立 `thprac_th16_logger.{h,cpp}`，但唯一使用方就是 `thprac_th16.cpp`（显示 / 每帧采样 / 重置 / 预构造 4 个调用点），拆分无封装收益、且与「一作一 cpp、无 per-game 头」的仓库惯例不符；拆分还逼出与 `enum addrs` 重复的 `kStageNum`/`kEclMgrPtr`。已把模块并入 `thprac_th16.cpp`（`:24-314`），地址常量改为直接复用 `enum addrs` 的 `STAGE_NUM`/`ENEMY_MANAGER_PTR`，删除这两个文件并从 `.vcxproj`/`.filters` 摘掉登记；生成物 `thprac_th16_checkpoints_gen.cpp` 保持独立 TU。4 个调用点与 `th16_game_start` 的重置约束未动。细节与行号变动（`thprac_th16.cpp:NNN` 整体 +299）见 `docs/th16/th16_monitor_impl_report.md` §5.2 / §10。**编译通过，真机回归待跑。** |
| 2026-10-17 | **日志记录（JSONL 落盘）落地（th14–th18）**：新增共享模块 `thprac_games.{h,cpp}` 的 `#pragma region Game Session Log`（`GameSession` / `GameSessionData` / `GameLogEvent` / `GameLogRecordOfSection()` / `GameSessionAppendJsonLine()`），五作各 8 处接线（`OnStageStart` → `Begin`、`Update` 顶 → `Tick`、`Update` 末 → `Latch`、`life_dec`/`bomb_dec` 既有 lambda 内 → `OnMiss`/`OnBomb`）。口径：JSONL 一行一局、月度文件 `<数据目录>logger_sessions_YYYYMM.jsonl`、一局边界 = `*_game_start` + `PLAYER_PTR` 下降沿（复用各作 IGI 的显隐判据）、启动 checkpoint = 本局首次 latch、字段合并为「启动/结束」两个、强退视为放弃。**Release\|x86 编译通过，真机未测。** §0 新增「日志落盘 / 测试情况」两列并回填；§1 总表、§2 档 D、§4 第 7 条同步。新建 `docs/logger/logger_impl_report.md`；`docs/req_logger.md` 追加 §落地口径。 |

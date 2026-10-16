# logger 需求进度监控（活文档）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（总括，含术语表 §2、Q1–Q9 §6、锚点总表 §5）。
> 需求来源：`docs/req_logger.md`。
> 本迭代范围：**仅监控**（实时显示「当前段落」）；**不含日志记录**（JSON 落盘 / miss·bomb 列表 / checkpoint 统计）；**th06–th08 已 E0 建档**（2026-10-16，A 代口径见 `docs/thXX/thXX_ecl_re_requests.md`），th09 需求明确不支持、th095 不在范围。
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

> 「完成」仅指**静态定案**——按 AGENTS.md §2.5，未真机验证的地址不得当作结论；E5 未做前，E1/E4 的 ✅ 均为 🟡 语义（**th16 例外**：E5 已做，故其 E1/E4 记真 ✅）。

---

## 1. 进度总表（结论速览）

> CSV 填/空由 §5 脚本实算（2026-10-16）。`章节行` 记录名形态 = `STn #k` / `STn 前半 #k` / `STn 后半 #k`，其余为 `boss 行`。
> **th06–th08（A 代）尚未生成 CSV**，不在 §5 统计范围内；其「段落入口」列口径已改为**时间值 / 指令偏移**（非 sub 名，见各作 requests §11）。

| 作 | E0 建档 | E1 逆向报告 | E2 CSV 骨架 | E3 章节行回填 | E4 boss 行回填 | E5 真机 | 综合档位 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **th06** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ | A 代建档，**逆向未跑** |
| **th07** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ | 同上 |
| **th08** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ | 同上（A 代线索最多） |
| **th10** | ✅ | ❌ | ✅ 41+68=109 | ❌ 0/41 | ❌ 0/68 ➖无 boss 报告 | ❌ | 仅建档，**逆向未跑** |
| **th11** | ✅ | ❌ | ✅ 37+88=125 | ❌ 0/37 | ❌ 0/88 ➖ | ❌ | 同上（st4 分支关） |
| **th12** | ✅ | ❌ | ✅ 43+71=114 | ❌ 0/43 | ❌ 0/71 ➖ | ❌ | 同上；**无任何现成 ECL hook** |
| **th13** | ✅ | ✅🟡 | ✅ 44+72=116 | ❌ 0/44 | ❌ 0/72 ➖ | ❌ | 逆向完成，**白名单未枚举** |
| **th14** | ✅ | ✅🟡 | ✅ 43+72=115 | ❌ 0/43 | 🟡 64/72 | ❌ | 章节行全空；ST4 8 行留空 |
| **th15** | ✅ | ✅🟡 | ✅ 47+73=120 | ❌ 0/47 | 🟡 10/73 | ❌ | 参照作，**回填最少** |
| **th16** | ✅ | ✅ | ✅ 39+71=110 | ✅ 39/39 | ✅ 71/71 | 🟡 除全关外全通过 | **监控功能完成**；仅剩全关测试 |
| **th17** | ✅ | ✅🟡 | ✅ 39+66=105 | ❌ 0/39 | 🟡 63/66 | ❌ | 章节行全空；ST6 3 行留空 |
| **th18** | ✅ | ✅🟡 | ✅ 49+65=114 | ❌ 0/49 | 🟡 65/65 | ❌ | 章节行全空；boss 行已全填 |

**整体回填率（第 1 列非空 / 总行数）**：

| 作 | th10 | th11 | th12 | th13 | th14 | th15 | th16 | th17 | th18 | 合计 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 已填/总 | 0/109 | 0/125 | 0/114 | 0/116 | 64/115 | 10/120 | 110/110 | 63/105 | 65/114 | **312/1028** |
| 空值率 | 100% | 100% | 100% | 100% | 44% | 92% | 0% | 40% | 43% | **70%** |

> 九作 CSV 骨架均已生成（合计 1028 行 = 章节 382 + boss 646）；其中 **th16 已全表回填**（110/110）。

---

## 2. 分档小结

- **档 A′：A 代建档（th06 / th07 / th08）** —— 2026-10-16 新建 `_ecl_re_requests.md`（三作齐全）。与 B/C 代模板的**主要差异**：章节跳转是「**时间轴**」而非 sub 跳转（th06 `ECLWarp(time)` / th07 `ECLTimeWarp(count,time)` / th08 `ECLWarp(time,offset,…)`，落点分别是 `0x5a5fb0` / `[0x9a9af8+0x9545fc]` / `0xf54cf8`），故 **Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一**；checkpoint「入口」列改用**时间值 / 指令偏移**。**无 CSV、无逆向**；A 代还**没有 eclmap**、`.ecl` 未从 `.dat` 解出。
  - A 代 ECL 格式里**确实有带 id 的 sub**：th07 `ECLCallSub` opcode `0x29`、th08 opcode `0x34`、th06 有 `EclRawInstrCallArgs.eclSub` 脚手架（未接线）—— 但**运行时是否暴露「当前 sub」未知**，正是 Q1/Q2 待答项。
- **档 A：仅建档（th10 / th11 / th12）** —— 有 `_ecl_re_requests.md`（2026-10-14 建档、注明「未跑逆向」），**无报告、CSV 全空**。B 代四作只剩 th13 有报告。
  - th12 额外：requests 头部注明本作在 thprac 中**没有任何 ECL 解释器 hook**，Q1–Q5 全部待逆向；总括 §5.1 命中点为 ❌。
- **档 B：逆向完成、白名单未枚举（th13）** —— `th13_ecl_re_report.md` 已静态定案 Q1–Q9（命中点 `0x4772D2`、`res+0x8C` sub 表等），但报告自陈「**⚠️ 段落入口白名单尚未枚举**」，`th13_checkpoints.csv` 116 行全空，且**未新建 boss 报告**。
- **档 C：逆向 + boss 报告完成、CSV 部分回填（th14 / th15 / th17 / th18）** —— 均无真机（E5 ❌）。回填分布**互补但不完整**：
  - **只填 boss、章节全空**：th14（64/72）、th17（63/66）、th18（65/65）。
  - **两边都缺**：th15（章节 0/47，boss 10/73）——作为参照作反而回填最少。
- **档 D：功能落地（th16）** —— 唯一跑通全链路的一作：CSV 全表回填（110/110）、源码实现（`thprac_th16_logger.{h,cpp}` + 生成物）、**真机验证除完整全关外全部通过**。剩余：全关测试（E5 收尾）+ 后续迭代的 JSON 落盘。

---

## 3. 逐作明细与缺口

| 作 | 报告状态标记 | 已回填范围 | 留空原因 |
| --- | --- | --- | --- |
| th06 | E0 建档（无报告） | — | 逆向未跑；A 代无 eclmap、`.ecl` 未从 `.dat` 解出 |
| th07 | E0 建档（无报告） | — | 同上 |
| th08 | E0 建档（无报告） | — | 同上 |
| th10 | V–Z 段未做 | — | 逆向未跑 |
| th11 | 同上 | — | 逆向未跑（st4 分支关） |
| th12 | 同上 | — | 逆向未跑；无现成 hook |
| th13 | 🟡 静态定案 | — | 白名单未枚举（`.decl` 素材已在机，可离线枚举） |
| th14 | 🟡 静态定案 / 静态调研 | boss 64/72 | ST4 8 行：A/B 双 boss 名不同，CSV 单列不足；章节行未做 |
| th15 | 🟡 静态定案 / 静态调研 | boss 10/73（ST1 BOSS1–4、ST2 BOSS1–6） | ST3–ST7 与各关 MID 暂缺；章节行未做 |
| th16 | ✅ 真机验证（除全关） | 章节 39/39、boss 71/71（全表 110/110） | — |
| th17 | 🟡 静态定案 / 静态调研 | boss 63/66 | ST6 BOSS9–11：`keepSpellDrops` 双分支留空；章节行未做 |
| th18 | 🟡 静态定案 / 静态调研 | boss 65/65 | 章节行未做（报告 §Q9 自陈章节白名单未枚举） |

> 各作报告细节：`docs/thXX/thXX_ecl_re_report.md`、boss 报告 `docs/thXX/thXX_boss_phase_report.md`（th14–th18）。
> 报告普遍声明「未修改任何既有文件，回填留待收口任务」——见 §4 不一致清单第 1 条。

---

## 4. 缺口 / 不一致清单

1. **总括 §5.1 与逐作报告未同步**：th13 报告已把命中点 / sub 表定为 ✅，总括 `§5.1` 仍写 ❌；th14/16/17/18 报告已确认名字表，总括仍写「待确认 / 待定」。多份报告显式声明「未修改任何既有文件」，回填留待「收口任务」（如 `th13_ecl_re_report.md:589`）。
2. **工作流文档口径过期**：`checkpoint_csv_workflow.md:159`、`checkpoint_entry_backfill_workflow.md`（§3.4 / §6）仍写「其余作及 boss 行仍为空 / boss 行流程暂缓」（2026-10-15 口径），与 2026-10-16 已回填 th14/15/17/18 boss 行的实际冲突。
3. **th13 报告完成但 CSV 未回填**：报告全套 Q1–Q9 定案，CSV 116 行全空（自陈未枚举）。
4. **th14 章节行 0、th17/th18 章节行 0**：整体回填率 312/1028（70% 空）。~~th16 boss 行 0~~ 已消解——th16 全表 110/110 回填完毕。
5. **th15 回填最少**：参照作却只有 10/120。
6. ~~**th06–th08 完全空白**~~ → **2026-10-16 部分收口：三作已 E0 建档**。`docs/th06|th07|th08/thXX_ecl_re_requests.md` 已建（A 代口径，Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一）；但 **E1 逆向未跑、E2 CSV 未生成**，且 A 代**无 eclmap**、`.ecl` 未从 `.dat` 解出、`reasm/exes/` 缺 `th06.exe`（见各作 requests §12）。总括 §0/§4/§5.1 已同步。
7. **日志（JSON 落盘）功能整体未起步**：需求 `docs/req_logger.md` §日志记录功能（启动/退出 JSON、miss·bomb 列表、每月一文件）在总括 `§0` 全标 ❌ 延后，`docs/` 与 `src/` 均无产出。
8. ~~**源码侧无实现**~~ **部分消解：th16 已实现**（`thprac/src/thprac/thprac_th16_logger.{h,cpp}` + 生成物 `thprac_th16_checkpoints_gen.cpp`，见 `docs/th16/th16_monitor_impl_report.md`）；**其余八作仍无实现**。既有 warp（`ECLHelper::SetBaseAddr`/`ECLJump`）与「freeze ECL sub time」hook（`thprac_th10.cpp:541`、`thprac_th14.cpp:590`、`thprac_th18.cpp:1202`）均不读函数名。原型 `thprac_th15_missbomb.{h,cpp}`（含 `GetEclSubName()`）仅在 **`feat/plogth15`** 分支、**未合并**，且因落盘崩溃被弃用。
9. **E5 真机验证：th16 已做（除完整全关），其余八作全部未做**。th16 的 `docs/th16/th16_monitor_impl_report.md` §7 / §7.1 清单已逐项通过，仅剩全关测试；其余作结论仍为 🟡 静态定案，AGENTS.md §2.5 要求真机验证后方可作结论。

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
th14       0     43     64     72     64    115
th15       0     47     10     73     10    120
th16      39     39     71     71    110    110
th17       0     39     63     66     63    105
th18       0     49     65     65     65    114
```

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档。按 §1 的阶段定义（E0–E5）汇总 th10–th18 进度：E1 报告 th10–th12 缺、th13 完成但白名单未枚举、th14–th18 完成；CSV 第 1 列合计 241/1029（77% 空），th16 仅章节行、th14/17/18 仅 boss 行、th15 最少、th10–th13 全空。登记 §4 缺口（总括 §5.1 与逐作报告未同步、工作流文档口径过期、th06–08 空白、日志迭代未起步、源码无实现）。 |
| 2026-10-16 | **th06–th08 建档（E0）**：新建 `docs/th06\|th07\|th08/thXX_ecl_re_requests.md`（A 代口径）。核心差异：A 代章节跳转是「**时间轴**」（th06 `ECLWarp(time)`→`0x5a5fb0` / th07 `ECLTimeWarp(count,time)`→`[0x9a9af8+0x9545fc]` / th08 `ECLWarp(time,offset,…)`→`0xf54cf8`）而非 sub 跳转；**Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一**；checkpoint「入口」列改用**时间值 / 指令偏移**（与 th10–18 语义分叉）。登记 A 代素材缺口（无 eclmap、`.ecl` 未解出、`reasm/exes/` 缺 `th06.exe`）。§1/§2/§3/§4 同步；§5 统计脚本不变（这三作无 CSV）。 |
| 2026-10-16 | th16 真机反馈修 bug（**跨作共性**）：很多关卡「后半第一节」的 checkpoint 实为**增援章节**，主线程在**中boss 存活期间**就开跑（`.decl` 实证：`st01.decl` `main()` 在 `enmCreateA("MBoss",…)` 后 `wait(160)` 就 `@MainLatter()`），导致面板提前跳到 `STn 后半 #1`。th16 已修（`thprac_th16_logger.{h,cpp}`：按白名单键前缀分「中boss/关底boss/章节」，中boss 存活期间章节 checkpoint 只缓存，死亡 sub 名出现时补显示）。**同一模式在 th10/th11/th14/th17/th18/th20 都存在**（各作 `TH_TIMELOCK` 的 `bossExists` 钩子即佐证），移植到其他作时需按各作 `.decl` 归纳死亡 sub 名。细节见 `docs/th16/th16_monitor_impl_report.md` §5.7 / §7.1。 |
| 2026-10-16 | **th16 监控功能收口**：真机验证通过（§7 / §7.1 清单除完整全关外全部通过，含 §7.1 的中boss 窗口两个 flush 分支与卡死反例）。据此同步 §1 总表 / 回填率 / §2 分档 / §3 明细 / §4 缺口 / §5 复算：th16 改为 **E1 ✅、E4 ✅ 71/71、E5 🟡（仅剩全关）**，CSV 全表 110/110（原文档写的 39/111、boss 0/72 是回填前的旧数）；整体回填率 **241/1029（77% 空）→ 312/1028（70% 空）**；新增**档 D「功能落地」**；§4 第 8、9 条的「源码侧无实现 / E5 全部未做」按 th16 已落地改写。头部「本机 macOS 不可编译」的旧描述一并作废（本机为 Windows）。**th16 剩余：完整全关测试；其余八作仍为静态定案。** |

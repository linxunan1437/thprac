# logger 需求进度监控（活文档）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（总括，含术语表 §2、Q1–Q9 §6、锚点总表 §5）。
> 需求来源：`docs/req_logger.md`。
> 范围：**监控**（实时显示「当前段落」）与**日志记录**（JSONL 落盘）均已落地 th14–th18（日志见 `docs/logger/logger_impl_report.md`），**五作全部走完、进入待真机测试阶段**（th16 监控已真机验证除完整全关，其余未测）；th10–th13 逆向完成、**白名单已枚举并全表回填**（2026-10-17）；**th06–th08 已 E0 建档**（2026-10-16，A 代口径见 `docs/thXX/thXX_ecl_re_requests.md`），th09 需求明确不支持、th095 不在范围。
> 工作流：`docs/logger/checkpoint_csv_workflow.md`（CSV 骨架）、`docs/logger/checkpoint_entry_backfill_workflow.md`（入口回填）。
> 素材来源：`docs/logger/decompile_materials.md`。
> 本机为 **Windows**（VS 在 `D:\tools\VS`），**可编译、可真机运行**——旧描述「macOS 不可编译」已作废，以 `docs/th16/th16_monitor_impl_report.md` 为准。**th16 已完成真机验证**（除完整全关外全部通过）；其余作仍为**静态定案（🟡 未真机三态）**，方法为只读反汇编（`pefile` + `capstone`）+ 离线 ECL 解码。
> 地址口径：`VA` 与 `RVA = VA − 0x400000`。本文不新增地址，地址以总括 / 逐作报告为准。
> **维护约定**：本文是进度**唯一事实来源**；每次「建档 / 逆向 / CSV 回填 / 监控实现 / 真机验证」后，更新 §1 总表与本文 §6 变更记录。逐作细节见各作文档。

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
| E5 | 监控代码实现 | 各作 `thprac_thXX.cpp` 内的 `THxxLogger` 模块 + 生成物 `thprac_thXX_checkpoints_gen.cpp`（消费 E2–E4 的 CSV）+ 工程登记 |
| E6 | 真机三态验证 | 真机采样记录（本机可做；th16 监控已做） |

**状态图例**：✅ 完成 ｜ 🟡 部分完成 / 静态定案未真机 ｜ ❌ 未开始 ｜ ➖ 不适用。

**「日志落盘」列**（2026-10-17 新增）：指 `docs/req_logger.md` 的**日志记录**迭代，与上面 E0–E6 的「监控」阶段并列 —— `➖ 不适用`（监控未落地，无从接线）｜`❌ 未接线`｜`🟡 已接线未真机`｜`✅ 真机通过`。
**「测试情况」列**：指该作**日志落盘**的真机测试情况 —— `—`（不适用）｜`未测`｜`部分通过`｜`全通过`。与「E6 真机」列（监控侧）相互独立。

> 「完成」仅指**静态定案**——按 AGENTS.md §2.5，未真机验证的地址不得当作结论；**E6** 未做前，E1/E4/**E5** 的 ✅ 均为 🟡 语义（**th16 例外**：E6 已做，故其 E1/E4/E5 记真 ✅）。

---

## 1. 进度总表（结论速览）

> CSV 填/空由 §5 脚本实算（2026-10-16）。`章节行` 记录名形态 = `STn #k` / `STn 前半 #k` / `STn 后半 #k`，其余为 `boss 行`。
> **th06–th08（A 代）尚未生成 CSV**，不在 §5 统计范围内；其「段落入口」列口径已改为**时间值 / 指令偏移**（非 sub 名，见各作 requests §11）。

| 作 | E0 建档 | E1 逆向报告 | E2 CSV 骨架 | E3 章节行回填 | E4 boss 行回填 | E5 监控实现 | E6 真机 | 日志落盘 | 测试情况 | 综合档位 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **th06** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ 未实现 | ❌ | ➖ | — | A 代建档，**逆向未跑** |
| **th07** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ 未实现 | ❌ | ➖ | — | 同上 |
| **th08** | ✅ | ❌ | ❌ 未生成 | ❌ | ❌ | ❌ 未实现 | ❌ | ➖ | — | 同上（A 代线索最多） |
| **th10** | ✅ | ✅🟡 | ✅ 41+68=109 | ✅ 41/41 | ✅ 68/68 | ❌ 未实现 | ❌ | ➖ | — | **CSV 全表回填（2026-10-17）**；st1–st3 用 **`sub名+0x偏移`** 二级粒度（`main+0x…`）；boss 行纯离线定出；监控未实现 |
| **th11** | ✅ | ✅🟡 | ✅ 37+88=125 | ✅ 37/37 | ✅ 88/88 | ❌ 未实现 | ❌ | ➖ | — | **CSV 全表回填（2026-10-17）**；st4 六机体分支 18 行全收录；段入口多为 `async` 派生线程；监控未实现 |
| **th12** | ✅ | ✅🟡 | ✅ 43+71=114 | ✅ 43/43 | ✅ 71/71 | ❌ 未实现 | ❌ | ➖ | — | **CSV 全表回填（2026-10-17）**；warp 一律走 `ECLJumpEx`；`ST6 BOSS9/10` 已修正为 `BossCard5/6`；监控未实现 |
| **th13** | ✅ | ✅🟡 | ✅ 44+72=116 | ✅ 44/44 | ✅ 72/72 | ❌ 未实现 | ❌ | ➖ | — | **CSV 全表回填（2026-10-17）**；`ST5 后半 #4` 用 `MainSub08+0x390` 消歧；监控未实现 |
| **th14** | ✅ | ✅🟡 | ✅ 43+80=123 | ✅ 43/43 | ✅ 80/80 | ✅ 已实现 | ❌ | 🟡 已接线（含单章节合并 + interrupt/stageClear） | 最小冒烟通过 | **CSV 全表回填 + 监控实现 + 日志接线**；ST4 拆 A/B 路线（boss 8→16 行）；监控未真机 |
| **th15** | ✅ | ✅🟡 | ✅ 46+73=119 | ✅ 46/46 | ✅ 73/73 | ✅ 已实现 | ❌ | 🟡 已接线（含单章节合并 + interrupt/stageClear） | 最小冒烟通过 | **CSV 全表回填 + 监控实现 + 日志接线**；删 `ST6 #3`（与 `ST6 STARS` 同一段）；监控未真机 |
| **th16** | ✅ | ✅ | ✅ 39+71=110 | ✅ 39/39 | ✅ 71/71 | ✅ 已实现 | 🟡 除全关外全通过 | 🟡 已接线（含单章节合并 + interrupt/stageClear） | 最小冒烟通过 | **监控功能完成**（真机验证除全关）；仅剩全关测试；`interrupt` 未真机 |
| **th17** | ✅ | ✅🟡 | ✅ 39+66=105 | ✅ 39/39 | ✅ 66/66 | ✅ 已实现 | ❌ | 🟡 已接线（含单章节合并 + interrupt/stageClear） | 最小冒烟通过 | **CSV 全表回填 + 监控实现 + 日志接线**；双分支统一按 `th_sections_cbt` 符卡口径；监控未真机 |
| **th18** | ✅ | ✅🟡 | ✅ 49+65=114 | ✅ 49/49 | ✅ 65/65 | ✅ 已实现 | ❌ | 🟡 已接线（含单章节合并 + interrupt/stageClear） | 最小冒烟通过 | **CSV 全表回填 + 监控实现 + 日志接线**；监控未真机 |

**整体回填率（第 1 列非空 / 总行数）**：

| 作 | th10 | th11 | th12 | th13 | th14 | th15 | th16 | th17 | th18 | 合计 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 已填/总 | 109/109 | 125/125 | 114/114 | 116/116 | 123/123 | 119/119 | 110/110 | 105/105 | 114/114 | **1035/1035** |
| 空值率 | 0% | 0% | 0% | 0% | 0% | 0% | 0% | 0% | 0% | **0%** |

> 九作 CSV 均已生成（合计 **1035 行 = 章节 381 + boss 654**）；**九作已全表回填**（th10–th13 于 2026-10-17 补齐，其中 th10 的 st1–st3 用 `sub名+0x偏移` 二级粒度）。
> 相对骨架的两处**有意的行数变动**：th14 ST4 拆 A/B 路线 → boss 行 **+8**（115→123）；th15 删 `ST6 #3`（与 `ST6 STARS` 同为 `MainSub03`，thprac 章节划分问题）→ 章节行 **−1**（120→119）。
> **boss 宿主可达性（2026-10-18）**：th10–th13 四作已独立定案 —— 宿主链为**全局全宿主链**，boss 宿主**可达**，**无需补捕获点**；四作 E5 的 boss 行读取面改为「**遍历全局宿主链 × 每宿主线程链**」。详见 §4 第 11 条。**四作 E5（监控实现）仍为 ❌ 未实现**，故综合档位不变。

---

## 2. 分档小结

- **档 A′：A 代建档（th06 / th07 / th08）** —— 2026-10-16 新建 `_ecl_re_requests.md`（三作齐全）。与 B/C 代模板的**主要差异**：章节跳转是「**时间轴**」而非 sub 跳转（th06 `ECLWarp(time)` / th07 `ECLTimeWarp(count,time)` / th08 `ECLWarp(time,offset,…)`，落点分别是 `0x5a5fb0` / `[0x9a9af8+0x9545fc]` / `0xf54cf8`），故 **Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一**；checkpoint「入口」列改用**时间值 / 指令偏移**。**无 CSV、无逆向**；A 代还**没有 eclmap**、`.ecl` 未从 `.dat` 解出。
  - A 代 ECL 格式里**确实有带 id 的 sub**：th07 `ECLCallSub` opcode `0x29`、th08 opcode `0x34`、th06 有 `EclRawInstrCallArgs.eclSub` 脚手架（未接线）—— 但**运行时是否暴露「当前 sub」未知**，正是 Q1/Q2 待答项。
- **~~档 A：仅建档（th10 / th11 / th12）~~ 已消解（2026-10-16）** —— 三作均已产出 `_ecl_re_report.md` 并回填 requests，**升入档 B**；原「无报告、CSV 全空」描述作废。
- **~~档 B：逆向完成、白名单未枚举（th10 / th11 / th12 / th13）~~ 已消解（2026-10-17）** —— 四作白名单**已全部枚举并全表回填**（109 / 125 / 114 / 116 = 464 行，全部非空），各作入口表小结见 `docs/thXX/thXX_checkpoint_entry_report.md`。原「白名单未枚举 / CSV 全空 / 未建 boss 报告」描述作废。
  - **B 代 boss 行可纯离线定出（2026-10-17 订正）**：`THPatch` 的字节补丁（改 boss 容器内 `call Boss1` 的**名字串** + 改 `lifeSet` 操作数逼 `setNext` 立即转移）＋ 相位链交叉验证即可逐行定案，**不需要** boss ECL 宿主 / 槽表 / 门控位 —— 那些只在**运行期读当前 sub**（实现监控）时才需要。（此前 §4 第 11 条的「boss 侧前置逆向缺失」判断有误，已撤。）
  - **B 代三作共性（th10 / th11 / th12，2026-10-16 收口）**：`[T+4]` = **当前指令绝对地址**（非 sub 索引，与 th13 同类；C 代 th15–18 才是索引）；取「当前 sub」须**遍历宿主线程链 `H+0x1030`** —— 段入口常以 `@MainSubNN() async` 派生为**独立线程**，主线 `T+4` 可能长期停在 `main`（th12 尤甚），且 `H+4` 会被本帧最后执行过的 async 线程覆盖。
  - **th12 额外**：原「在 thprac 中**没有任何 ECL 解释器 hook**」→ **已自找定案** `0x4671A0`(6B, `EDI` = 执行对象)；th13 亦有 `0x4772D2`(6B, `EBX`)。
  - **th10 额外（已定案 2026-10-17）**：st1–st3 的段落入口确实**全部落在 `main` 内部**（`.decl` 佐证无 `MainSubNN`，道中为内联区段）⇒ 这 15 行改用 **`sub名+0x偏移`** 二级粒度（`main+0x…`），偏移 = 相对该 sub **首条指令**的文件偏移；方案与偏移表见 `docs/th10/th10_checkpoint_ip_offset.md`。**th11 不适用**（其 `main` 是纯容器，入口全是独立 `MainSubNN`）。总括 §1.2 口径已同步。
  - **B 代细分建议（待决，由 th12 报告提出）**：把 B 代细分为 **B1 = th12/th13**、**B2 = th10/th11**；但 th11 帧长/遍历步长未定案，**该细分待补证**（总括 §4 注记）。
- **~~档 C：逆向 + boss 报告完成、CSV 全表回填（th14 / th15 / th17 / th18）~~ 已消解（2026-10-17）** —— 四作均已推进到**档 D**（监控实现 + 日志接线）。原档 C 内容（2026-10-16 章节行 + boss 行全表回填：th14 123/123、th15 119/119、th17 105/105、th18 114/114，口径＝`checkpoint_entry_backfill_workflow.md`）保留备查。
  - **遗留**：th14 ST4 的 A/B 双路线已拆为 16 行（`ST4A`/`ST4B`，B 路取 `st04mbs2`/`st04bs2` 的 `MBossB*`/`BossB*`）；th15 `ST7 前半 #5` 与 `ST7 后半 #1` 入口同名（`MainSub04`）→ 登记于 `knownBugs.md` #1（P2）。
- **档 D：功能落地（th14–th18 监控实现 + 日志落盘）** —— **监控（E5）**：th14 / th15 / th16 / th17 / th18 五作**均已实现** —— 各作 `thprac_thXX.cpp` 内的 `THxxLogger` 模块 + 生成物 `thprac_thXX_checkpoints_gen.cpp` + 脚本 `docs/logger/tools/gen_thXX_checkpoints.py`，工程登记于 `thprac.vcxproj`/`.filters`；Release|x86 编译通过。其中 **th16 已真机验证**（除完整全关外全部通过，E6 🟡）；**th14 / th15 / th17 / th18 真机未测**（E6 ❌）。参考实现见 `docs/th16/th16_monitor_impl_report.md`。**日志落盘**（2026-10-17）已接入同一五作（共享模块 `thprac_games.{h,cpp}` + 各作 8 处调用，JSONL 月度文件），Release|x86 编译通过，**真机未测**。**至此五作的监控实现与日志接线全部走完，进入待真机测试阶段。** 详见 `docs/logger/logger_impl_report.md`。

---

## 3. 逐作明细与缺口

| 作 | 报告状态标记 | 已回填范围 | 留空原因 |
| --- | --- | --- | --- |
| th06 | E0 建档（无报告） | — | 逆向未跑；A 代无 eclmap、`.ecl` 未从 `.dat` 解出 |
| th07 | E0 建档（无报告） | — | 同上 |
| th08 | E0 建档（无报告） | — | 同上 |
| th10 | 🟡 静态定案 | 章节 41/41、boss 68/68（全表 109/109） | —（st1–st3 的 15 行用 `main+0x…` 二级粒度；`ST4 前半 #2` 取 warp 落点 `MainSub1+0x154`） |
| th11 | 🟡 静态定案 | 章节 37/37、boss 88/88（全表 125/125） | —（st4 六机体分支 18 行全收录） |
| th12 | 🟡 静态定案 | 章节 43/43、boss 71/71（全表 114/114） | —（`ST6 BOSS9/10` 已修正为 `BossCard5/6`） |
| th13 | 🟡 静态定案 | 章节 44/44、boss 72/72（全表 116/116） | —（`ST5 后半 #4` 用 `MainSub08+0x390` 消歧） |
| th14 | 🟡 静态定案 / 静态调研 | 章节 43/43、boss 80/80（全表 123/123） | —（ST4 拆 A/B 路线为 16 行：A＝朴素 `st04mbs/st04bs`，B＝`st04mbs2/st04bs2`） |
| th15 | 🟡 静态定案 / 静态调研 | 章节 46/46、boss 73/73（全表 119/119） | —（删 `ST6 #3`：与 `ST6 STARS` 同为 `MainSub03`；`ST7` 同名入口见 `knownBugs.md` #1） |
| th16 | ✅ 真机验证（除全关） | 章节 39/39、boss 71/71（全表 110/110） | — |
| th17 | 🟡 静态定案 / 静态调研 | 章节 39/39、boss 66/66（全表 105/105） | —（`keepSpellDrops` 双分支类行统一按 `th_sections_cbt` 符卡口径） |
| th18 | 🟡 静态定案 / 静态调研 | 章节 49/49、boss 65/65（全表 114/114） | — |

> 各作报告细节：`docs/thXX/thXX_ecl_re_report.md`、boss 报告 `docs/thXX/thXX_boss_phase_report.md`（th14–th18）、**入口表小结 `docs/thXX/thXX_checkpoint_entry_report.md`（th10–th13，2026-10-17 新增）**。
> 报告普遍声明「未修改任何既有文件，回填留待收口任务」——见 §4 不一致清单第 1 条。

---

## 4. 缺口 / 不一致清单

1. ~~**总括 §5.1 与逐作报告未同步**~~ → **已收口（2026-10-17）**：总括 §5.1 的 th14/16/17/18「名字待确认 / 待定」已改 ✅；并修三处**未登记**的过期残留 —— 总括 §0「本机不能编译验证」（与本机 Windows 可编译矛盾）、§5.3 注的 boss 行旧数（th14 64/72、th15 10、th17 63 → 全表 80/80、73/73、66/66）、§6 Q5「th12/th13 完全没有」（早已自找定案 `0x4671A0`/`0x4772D2`）。
2. ~~**工作流文档口径过期**~~ → **已收口（2026-10-17）**：`checkpoint_csv_workflow.md` §9 附录（回填进度行 / th12「ST6 无 MID」错误备注 / 骨架期行数）与 `checkpoint_entry_backfill_workflow.md`（§3.3 判据、§3.4 boss 行、§6 自检）均已按现状改写；另补 §3.3 两条口径（`dest` 非 call ⇒ 取**包含 dest 的 sub**；只跟随 `start` 落在当前链 sub 内的 jump）与 th13 的 `goto` opcode `0x0D` 注记。
3. ~~**th10–th13 报告完成但 CSV 未回填**~~ → **已消解（2026-10-17）**：四作 CSV 全部回填（109 / 125 / 114 / 116 = 464 行，**100%**）；th10 的 st1–st3 走 `sub名+0x偏移` 二级粒度。
4. ~~**th14 章节行 0、th17/th18 章节行 0**~~ → **已消解（2026-10-16）**：th14/15/17/18 章节行与 boss 行**全部回填**，整体回填率 **312/1028（70% 空）→ 571/1035（45% 空）**。剩余空白**全在 th10–th13**（逆向完成、白名单未枚举）。
5. ~~**th15 回填最少**~~ → **已消解（2026-10-16）**：th15 全表 119/119（参照作已补齐）。
6. ~~**th06–th08 完全空白**~~ → **2026-10-16 部分收口：三作已 E0 建档**。`docs/th06|th07|th08/thXX_ecl_re_requests.md` 已建（A 代口径，Q1 改为「sub 索引 / 时间值 / 指令偏移」三选一）；但 **E1 逆向未跑、E2 CSV 未生成**，且 A 代**无 eclmap**、`.ecl` 未从 `.dat` 解出、`reasm/exes/` 缺 `th06.exe`（见各作 requests §12）。总括 §0/§4/§5.1 已同步。
7. ~~**日志（JSON 落盘）功能整体未起步**~~ → **已部分落地（2026-10-17）**：共享模块 `thprac_games.{h,cpp}`（`#pragma region Game Session Log`）+ **th14 / th15 / th16 / th17 / th18** 各 8 处接线；JSONL 一行一局、月度文件 `logger_sessions_YYYYMM.jsonl`；口径见 `docs/req_logger.md` §落地口径、实现见 `docs/logger/logger_impl_report.md`。**Release|x86 编译通过；五作真机均未测**（按用户安排，功能补齐后统一验证）。**未接线**：th10–th13（白名单**已枚举**，但监控 E5 未实现 ⇒ 无生成物可接线）、th06–th08（逆向未跑）。th18 的机体类型字段留空（无现成来源，见 `docs/logger/logger_impl_report.md` §8 第 6 条）。
8. ~~**源码侧无实现**~~ **已收口（2026-10-17）**：**th14 / th15 / th16 / th17 / th18 五作均已实现** —— 各作 `thprac_thXX.cpp` 内的 `THxxLogger` 模块 + 生成物 `thprac_thXX_checkpoints_gen.cpp` + 脚本 `docs/logger/tools/gen_thXX_checkpoints.py`，工程登记于 `thprac.vcxproj` / `.filters`（th16 见 `docs/th16/th16_monitor_impl_report.md`）。**th10–th13 仍无实现**（白名单未枚举 ⇒ 无 CSV 可嵌入）、**th06–th08 无实现**（逆向未跑）。既有 warp（`ECLHelper::SetBaseAddr`/`ECLJump`）与「freeze ECL sub time」hook（`thprac_th10.cpp:541`、`thprac_th14.cpp:590`、`thprac_th18.cpp:1202`）均不读函数名。原型 `thprac_th15_missbomb.{h,cpp}`（含 `GetEclSubName()`）仅在 **`feat/plogth15`** 分支、**未合并**，且因落盘崩溃被弃用 —— th15 正式实现已改为「不缓存任何指针」。
9. **E6 真机验证（监控侧）：th16 已做（除完整全关），其余八作全部未做**。th16 的 `docs/th16/th16_monitor_impl_report.md` §7 / §7.1 清单已逐项通过，仅剩全关测试；**th14 / th15 / th17 / th18 的监控实现已编译通过但真机未测**（建议照 th16 §7 / §7.1 清单逐项跑）；th10–th13 / th06–th08 无实现，无从验证。AGENTS.md §2.5 要求真机验证后方可作结论。
10. ~~**B 代「段落入口」粒度待决**~~ → **已定案（2026-10-17）**：**仅 th10 st1–st3** 需要二级粒度，记法 **`sub名+0x偏移`**（偏移 = 相对该 sub **首条指令**，例 `main+0x28`）；**th11 / th12 / th13 不需要**（其入口全是独立 sub 名）。方案与偏移表见 `docs/th10/th10_checkpoint_ip_offset.md`；总括 §9.9 同步。
11. ~~**th10–th13 的 boss 侧（方案二）连逆向锚点都没有**~~ → **已撤销（2026-10-17）**：该判断**有误**。四作 boss 行**均可纯离线定出**（`THPatch` 改 boss 容器内 `call Boss1` 的**名字串** + 改 `lifeSet` 操作数逼 `setNext` 立即转移 + 相位链交叉验证），**不依赖** boss ECL 宿主 / 槽表 / 门控位 —— 那些只在**运行期读当前 sub**（实现监控）时才需要，**不阻塞白名单枚举**。**~~但仍阻塞「运行期匹配」：需先补 boss 宿主/线程链的逆向锚点（道中侧主线宿主捕获点已知）~~ → 已收口（2026-10-18）**：四作**各派一个子代理独立只读反汇编**（`pefile` + `capstone`，素材 `reasm/exes/`），**同向定案 —— 宿主链（th10 `mgr+0x58` / th11 `mgr+0x68` / th12 `mgr+0x68` / th13 `mgr+0xB0`）是「全局全宿主链」，boss 宿主必在其上**（依据：全程序**只有唯一宿主工厂**、且入链**无条件、与名字无关**；引擎自身的宿主迭代也走这条链）⇒ **不需要补任何捕获点**。四作 Q-C 亦一致：**所有宿主共用一份全局 `res`**（`H+0x102C`/`H+0x11CC` 均指向 `*(mgr+0xXX)`：th10 `0x54` / th11、th12 `0x64` / th13 `0xAC`），`res+0x8C` 表**跨文件合并** ⇒ boss sub 与主线同表，「地址 → 名字」区间反查对 boss 行成立（th13 另有**全表交叉验证 116/116 命中**）。**真正要改的是读取面写法**：由「只读主线宿主」改为「**外层遍历全局宿主链 → 内层每宿主线程链**」（th12 据此**撤销**原 §3.2 的「备选钩子」方案）。boss 宿主捕获点（th10 `0x40ED16` / th11 `0x413044` / th12 `0x4157AF` / th13 `0x41FE67`）仅登记为**可选诊断锚点**。逐作结论见各作 `_ecl_re_report.md`（th10 §5A / th11 §5A / th12 §5.4 / th13 §5.4）。**全部 🟡 静态定案、未真机**；四作共同的真机断言 = 「进 boss 后，**主线宿主**线程链上**看不到** boss sub，而**全局宿主链**上能找到入口 sub 为 `Boss`/`MBoss` 的宿主」。

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
th10      41     41     68     68    109    109
th11      37     37     88     88    125    125
th12      43     43     71     71    114    114
th13      44     44     72     72    116    116
th14      43     43     80     80    123    123
th15      46     46     73     73    119    119
th16      39     39     71     71    110    110
th17      39     39     66     66    105    105
th18      49     49     65     65    114    114
```

> 合计 **1035/1035**（空值率 **0%**，2026-10-17 复算）。th14 的 `ST4A MID1` 等 A/B 行不匹配章节行正则，正确计入 boss 行。
> th10 的 18 行 st1–st3 走 `sub名+0x偏移` 记法（如 `main+0x28`），仍计入「非空」。

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
| 2026-10-17 | **补「E5 监控实现」阶段与总表列（结构补齐）**：§0 阶段定义原先 **E4（boss 行回填）直接跳到 E5（真机验证）**，**缺「监控代码实现」这一步**；现插入 **E5 = 监控代码实现**（产出＝各作 `THxxLogger` 模块 + 生成物 `thprac_thXX_checkpoints_gen.cpp` + 工程登记），**原 E5 真机三态验证顺延为 E6**（实现本就应先于验证）。**§1 总表**：新增「E5 监控实现」列并回填 —— **th14 / th15 / th16 / th17 / th18 ✅ 已实现**（五作均已落地：`003817e th14-18监控`，th16 另有 `docs/th16/th16_monitor_impl_report.md`）、**th10–th13 与 th06–th08 ❌ 未实现**；原「E5 真机」列改名「E6 真机」。**综合档位**同步：th14/15/17/18 由「CSV 全表回填」改为「**CSV 全表回填 + 监控实现 + 日志接线**；监控未真机」、th16 补「（真机验证除全关）」、th10–13 补「（⇒ 监控未实现）」的因果。**§0 注记**（E5 未做前 → E6 未做前，并纳入 E5）、**§2**（档 C **消解**、四作并入档 D）、**§4 第 8 条**（源码侧无实现 → 已收口：五作已实现 / 四作仍无）、**§4 第 9 条**（E5 → E6）同步。回填率与 §5 统计不变。**th14/15/17/18 的监控实现仍为 🟡 静态定案，未真机。** |
| 2026-10-17 | **文档同步收口（th14–th18 全部走完、进入待测试）**：修两处**未登记**残留 —— §0 阶段表 E6 行的「本机无法做」（与本机可真机运行、th16 已真机矛盾）、§1 总表 th10–th13 的 E4 列把「无 boss 报告」误标 `➖`（= 不适用）改为 ❌ 口径。**§4**：第 1 条改为**已收口**（总括 §5.1 名字列 ✅，并修总括 §0/§5.3/§6 Q5 三处过期残留）；**新增第 11 条**（th10–th13 boss 侧连锚点都没有，比「白名单未枚举」更深一层）。**§2 档 D** 补「五作监控 + 日志接线全部走完、进入待真机测试阶段」。回填率与 §5 统计不变。 |
| 2026-10-17 | **th10–th13 CSV 全表回填（E3 + E4）**：四作各开一个子代理离线枚举，按 `checkpoint_entry_backfill_workflow.md` 口径（warp 落点 → `call`/`async call` 被调者；boss 行＝`THPatch` 补丁 + 相位链）回填第 1 列，**合计 464/464**，九作回填率 **571/1035（45% 空）→ 1035/1035（0% 空）**。**§1 总表**：th10 记 41/41 + 68/68、th11 记 37/37 + 88/88、th12 记 43/43 + 71/71、th13 记 44/44 + 72/72，综合档位均改「CSV 全表回填；监控未实现」。**§2**：档 B **消解**（四作升入「CSV 全表回填」）；记录 **B 代 boss 行可纯离线定出**（撤销 §4 第 11 条）。**§3**：四作明细改写（原留空原因作废）。**§4**：第 2 条（工作流文档口径）**已收口**、第 3 条**消解**、第 10 条**已定案**（仅 th10 st1–st3 用 `sub名+0x偏移`）、**第 11 条撤销**（判断有误）。**§5** 预期输出更新。**关键订正**：① th10 的 18 行（st1–st3 内联区段 15 行 + 同名冲突 3 行）改用 **`sub名+0x偏移`** 二级粒度（偏移基准＝相对该 sub **首条指令**）；② th12 `ST6 BOSS9/BOSS10` 由 `Boss5`/`Boss6` 修正为 **`BossCard5`/`BossCard6`**（thprac `th_sections_cbt[5]` 归在符卡栏）；③ th13 `ST5 后半 #4` 由 `MainSub08` 改为 **`MainSub08+0x390`**（与 #3 同 sub 消歧）。新建 `docs/th10/th10_checkpoint_ip_offset.md`（th10 ip 偏移可行性）与四作 `docs/thXX/thXX_checkpoint_entry_report.md`（入口表小结）。**四作仍 🟡 静态定案，未真机；监控（E5）未实现。** |
| 2026-10-17 | **th17 日志新增 `break`（撞咆哮）事件**：共享枚举 `GameLogEventType` 增 `Break`；序列化由二元三元表达式改为 `GameLogEventTypeName()`（内联 `switch` 无 `default`，防将来漏写 case 时静默错标）；`th17_roar_break`（`0x40F880`）**既有 lambda 内**加 `TH17Logger::OnBreak()`（同址不新增 EHOOK）。**仅 th17 产出 `break`**；th14/15/16/18 的 JSONL 输出不变。口径见 `docs/req_logger.md` §落地口径 第 10 行，实现见 `docs/logger/logger_impl_report.md` §4。**Release\|x86 编译通过，真机未测**（§1 总表状态不变：仍 🟡 已接线未真机）。 |
| 2026-10-18 | **口径 12（stageClear / interrupt）推广到 th14/15/17/18，五作齐**：每作逐名枚举 `reasm/exes/ecl_dump/thXX/*.decl` 得到自己的关底收尾 sub 清单（**不可跨作外推**），各作 9 处同构接线，**共享层零改动**。清单：th14 = `BossDead`/`BossEscape`/`BossBDead`/`BossBEscape`；th15 = `BossDead`/`BossDead2`/`BossEscape`（排除 `st07bs2` 的 `BossDeadB`）；th16 = `BossDead`/`BossDead2`/`BossDeadp`/`BossDeadBp`/`BossEscape`；th17、th18 = `BossDead`/`BossDead2`/`BossEscape`。**§1 总表**：th14/15/17/18 四行的「日志落盘」列改为 `🟡 已接线（含单章节合并 + interrupt/stageClear）`。逐作清单与排除理由见 `docs/logger/logger_impl_report.md` §4.4.1。**Release\|x86 编译通过；五作真机均未测（🟡）**。 |
| 2026-10-18 | **合并行规范化（口径 14）**：`total > 1` 的合并行在 JSON 里 `checkpointCount` 固定输出 1、`events` 固定输出 `[]`（键保留）、`eventsDropped` 归零；`total == 1` 的首条不规范化（保留真实明细）。改在共享层 `thprac_games.cpp` 的 `MergeOrAppend()` 就地改写分支 ⇒ **五作都生效**。口径见 `docs/req_logger.md` 口径 14，实现见 `docs/logger/logger_impl_report.md` §2 / §5.1 / §8。**Release\|x86 编译通过；真机未测。** |
| 2026-10-18 | **单章节判据修订 + `got` 无视 interrupt（口径 13）**：口径 12 的 `interrupt` 让 th16 每个练习局都带一条事件 ⇒ 口径 11 原判据的 `eventCount == 0` 分支变死代码、`got` 被打成恒 0。新判据 = `(cpCount==2 && 含interrupt) \|\| (cpCount==1 && 含miss/bomb/break)`；`got` = 「无 miss/bomb/break」的尝试数。实现在共享层 `thprac_games.cpp`（`GameSessionHasAttemptEvent()` / `GameSessionHasInterruptEvent()`）。**跨作代价（定案接受）**：不产出 `interrupt` 的 th14/15/17/18，其 `cpCount==2` 的干净练习局**不再合并** ⇒ **四作的日志行数会变多**（`logger_progress.md` §1 的「日志落盘」列口径同步）。口径见 `docs/req_logger.md` 口径 13，实现见 `docs/logger/logger_impl_report.md` §5.1 / §7 / §8，代价见 `knownBugs.md` #10。**Release\|x86 编译通过；真机未测。** |
| 2026-10-18 | **th16 新增 stageClear + `interrupt`（口径 12，仅 th16）**：关底 boss 的「死/逃」收尾 sub（`BossDead`/`BossDead2`/`BossDeadp`/`BossDeadBp`/`BossEscape`，名字为 `reasm/exes/ecl_dump/th16/*.decl` 全 22 份的枚举精确匹配）被每帧 sub 名采样命中 ⇒ `stageClear = true`；段落入口切换 / 换关 / 新局 ⇒ 复位；`stageClear == false` 且 `PLAYER_PTR` 下降沿 ⇒ 记一条 `interrupt` 事件。**不新增任何 hook / 任何地址**（复用 `IsMidBossEndSub` 同一采样点）。共享层只加枚举值 `Interrupt` + `GameSession::IsGameExitEdge()`；**`thprac_games.cpp` 零改动**（合并判据按定案保持原样）。**§1 总表**：th16 的「日志落盘」列改 `🟡 已接线（含单章节合并 + interrupt/stageClear，仅本作）`，其余四作不动。口径见 `docs/req_logger.md` 口径 10 / 12，实现见 `docs/logger/logger_impl_report.md` §4.4 / §5 / §7 / §8，边界见 `knownBugs.md`。**Release\|x86 编译通过；真机未测（🟡）**。 |
| 2026-10-18 | **单章节练习合并落盘（口径 11）**：单章节练习每 10–20s 重开一次、每次都产出一行 1–2 checkpoint 的记录，量级失控。现按「同起始 checkpoint + 同日 + 单章节」把连续练习局**并入同一行**（就地改写上一行，`total`/`got` 累加），其余情况另起一行；跨进程不合并。判定只用共享字段（`checkpointCount`/`eventCount`/起始 checkpoint/日期）⇒ **逻辑全在共享 `GameSession`、默认开启，th14/15/16/17/18 五个 `.cpp` 零改动**。`GameSessionData` 增 `total`/`got`（`total == 0` 时整对从 JSON 省略 ⇒ 非单章节记录输出逐字节不变）。**§1 总表**：th14–th18 五行的「日志落盘」列改为 `🟡 已接线（含单章节合并 + interrupt/stageClear）`，其余行不动。口径见 `docs/req_logger.md` 口径 11，实现见 `docs/logger/logger_impl_report.md` §2 / §3.1 / §5.1，边界见 `knownBugs.md` #6/#7。**Release\|x86 编译通过、`thprac_games.cpp` 零警告；五作最小冒烟通过**（§1 总表 th14–th18 的「测试情况」列同步为「最小冒烟通过」；**仅确认功能可用，边界项未逐项验证**）。 |
| 2026-10-18 | **th13 监控方案建档**：新建 `docs/th13/th13_monitor_plan.md`（th13 监控 E5 实现方案，**只写方案不写代码**；§1 总表 th13 状态不变，仍为「监控未实现」）。核心：取「当前 sub」= **th14 同形**（`mgr+0xB0` 宿主链 × `H+0x11D0` 线程链 → `ip = *(u32*)(T+4)` → 地址升序区间反查；3 行 async-only 须遍历线程链）；**偏移匹配判据** `ip == table[i].+4 + 0x10 + 偏移`（首指令地址可得 ✅，帧边界 ip 是否恰为该地址 🟡 待真机），**偏移命中优先**；数据表示沿用「CSV 逐字节嵌入 + 运行期 rapidcsv 解析」、`+0x` 后缀解析期拆分（不新增列）；中boss 窗口按 th16 §5.7 **须移植**（th13 同构）；产物 = 新增 `gen_th13_checkpoints.py` + `thprac_th13_checkpoints_gen.cpp`，改 `thprac_th13.cpp`（`TH13Logger` 模块内联）+ 工程登记。**未实现、未真机。** |
| 2026-10-18 | **th10–th13「boss 宿主可达性」定案 —— §4 第 11 条的「运行期匹配仍阻塞」收口**：四作**各派一个子代理独立只读反汇编**（`pefile` + `capstone`，素材 `reasm/exes/`；**未跑游戏、未跑编译、未改任何二进制**），**同向定案：宿主链（th10 `mgr+0x58` / th11 `mgr+0x68` / th12 `mgr+0x68` / th13 `mgr+0xB0`）是「全局全宿主链」，boss 宿主必在其上 ⇒ 不需要补任何捕获点**。判据统一为「全程序是否只有唯一宿主工厂 + 入链是否无条件、与名字无关」；th10/th13 的「唯一宿主工厂」论证最干净，th11 是「宿主构造的唯一调用点就在工厂体内」，th12 是「工厂内无条件插链、无『只有 main 才入链』分支」。四作 Q-C 一致：**所有宿主共用一份全局 `res`**，`res+0x8C` **跨文件合并** ⇒ boss sub 与主线同表（th13 另做**全表交叉验证 116/116 命中**）。**结论的可执行部分 = 读取面写法**：由「只读主线宿主」改为「**外层遍历全局宿主链 → 内层每宿主线程链**」；th12 据此**撤销**其入口表小结 §3.2 的「备选钩子」方案。boss 宿主捕获点（th10 `0x40ED16` / th11 `0x413044` / th12 `0x4157AF` / th13 `0x41FE67`）登记为**可选诊断锚点**。**文档**：四作 `_ecl_re_report.md` 新增章节（th10 §5A +172 / th11 §5A +146 / th12 §5.4 +198 / th13 §5.4 +251），并同步各作入口表小结（th10 §3.5 与第 12 条 → 「缺口消解」、th12 §3.2/R5 重写、th13 第 10 条 → ✅；**th11 结论为「缺口不存在」故未改其入口表小结**，但遗留「其 §4.1 读取链仍以主线宿主起，对 boss 行写法不足」待补）与 `docs/th13/th13_monitor_plan.md`（§9 风险 1 与 §7 验证 6 降级）。**§4 第 11 条**改「已收口」、**§1 总表**补注一行。**全部 🟡 静态定案、未真机**；四作共同真机断言见 §4 第 11 条。**未改任何源码。** |

# TH08 Boss 阶段监控方案重查

> 上位文档：`docs/logger/logger_re_disasm_overview.md`；需求：`docs/req_logger.md`、`docs/th08/th08_ecl_re_requests.md`。
> 方法参照：`docs/th06/th06_boss_phase_re_report.md`（时间轴负责开战，Boss 自身 sub 链负责阶段；阶段白名单与调用栈由内向外）。本篇独立核对 th08 字节，不搬用 th06 字段偏移。
> 输入：`docs/logger/th08_checkpoints.csv`，37 章节 + 104 非 LW Boss + 17 LW；2026-10-27 撤下的是 Boss 分析，CSV 未删除。
> 素材：`D:/workshop/reasm/exes/th08.exe`，840704 B，PE 时间戳 1095610188，SHA256 `330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924`；`ecl/th08/`、`ecl_dump/th08/` 同根，只读、不提交。
> 方法：pefile / capstone x86-32，只读反汇编；ECL 原始指令链解码与 decl 逐指令数量、关键 opcode 核对；THPatch 练习补丁交叉比对。
> 状态：**🟡 静态候选方案；104 行均有候选 sub 及原始字节证据，尚未形成真机验证白名单。2026-11-01已接入E5实现（见 `th08_monitor_impl_report.md`），E6未做。**先按 th06 的 L 范围调查普通关，Extra 使用独立难度掩码；E/N/H 未展开。17 LW 仍按既有用户决定排除。
> 地址口径：PE 地址给 VA / RVA（ImageBase `0x400000`）；ECL 文件偏移仅记 `0x...`，不伪装为 PE 地址。字段 `H+...` 是宿主相对偏移。

## 0. 结论速览

| 问题 | 本轮结果 | 状态 |
| --- | --- | --- |
| CSV 是否丢失 Boss 行 | 121 行仍在；104 行非空、17 LW 有意空。非空仅证明 warp 输入已登记 | 已复算 |
| 时间轴能否分阶段 | 同组阶段共用开战入口，不能。必须另读 Boss ECL 状态 | 🟡 静态证据 |
| Boss 宿主可达性 | `setBoss(id)` 把宿主写进 `0xF54CC0 / 0xB54CC0 + id*4`；脚本中有 0、1 两槽，ST6A LS 使用 1 | 🟡 未真机 |
| 阶段读取 | 主 cursor `H+0x7F8`，subId 在 `+0x224`，配合已提交主调用栈；由当前 sub → 栈顶 → 栈底找阶段白名单 | 🟡 待验证 |
| 转阶段 | HP 阈值、计时阈值、HP 归零回调三路均最终进入 `SetSub`；目标字段会复用，不能事后把 next 字段当当前阶段 | 🟡 静态证据 |
| 映射产出 | `docs/logger/th08_boss_phase_candidates.csv`：104 个候选，各带 sub 文件偏移、L/Extra spell id、指令偏移/时间/opcode/run mask、decl 行号、ECL SHA256 | 🟡 候选，不能计作 E4 全完成 |
| 剩余缺口 | 104 行均未真机确认；包装 sub 的进出边界、同帧多阶段、LS 条件、跨宿主裁决未验证；E/N/H 和 LW 无本轮映射 | 明确保留 |

## 1. 从时间轴切到 Boss 宿主

`THPatch` 的 `ECLWarp` 首 pair 是开战位置。例如 ST1 BOSS1–4 同为 `4175/0xAC84`。练习跳到 BOSS2 时另用 `ECLSetTime(...,38,...)`，BOSS3 用 `ECLJump(...,0x4E0C,...)`，BOSS4 用 `ECLCallSub(...,33,...)` 后改计时器到 44。时间轴值相同与阶段不同可同时成立。

Boss 注册的直接证据（opcode `0x7F`）：

| 指令 VA / RVA | 读写 | 含义 |
| --- | --- | --- |
| `0x41C4B9 / 0x1C4B9` | `mov [ecx*4+0xF54CC0],edx`，edx=`[ebp+8]` 宿主 | `bosses[id]=H` |
| `0x41C562 / 0x1C562` | `mov [H+0x3313],dl` | 保存本宿主 Boss 槽号 |
| `0x41C5B6 / 0x1C5B6` | `mov [eax*4+0xF54CC0],0` | `setBoss(-1)` 反注册 |
| `0x41C523 / 0x1C523` 与 `0x41C5D0 / 0x1C5D0` | 设置/清除 `H+0x3324` bit1 | Boss 身份位 |

因此无需为了找 Boss 遍历 480 个宿主。优先读脚本使用的槽 0、1；验证 `H` 是宿主数组中合法活动对象，且 subId 在本文件范围内。宿主数组首址 `0x57D2F0 / 0x17D2F0`、步长 `0x53D0`、480 槽（既有 E1；本轮计时转阶段函数中 `0x42BB01 / 0x2BB01`、`0x42BB1D / 0x2BB1D`、`0x42BB26 / 0x2BB26` 再次交叉印证）。

不能只读槽 0：`ecldata6.decl` Sub30 先反注册，再 `setBoss(1)`；ST6A LS 的练习也特意禁止两处 `setBoss(-1)`，说明槽生命周期会影响倒计时与采样窗口。槽消失时沿用 latch，不把“无宿主”当新阶段。

## 2. 调用栈与采样时机

资源单例 `0x4ECCB8 / 0xECCB8` 的 `+0` 为 pECL、`+4` 为已绝对化 sub 表。`SetSub` 在 `0x418470 / 0x18470` 取 `subTable[id]`，`0x418473 / 0x18473` 写 cursor ip，`0x418499 / 0x18499` 写 cursor `+0x224` 的 i16 subId。文件头 `pECL+4` 为 subCount，离线 `SubN` 即索引 N，无运行期名字。

`call` 的 `0x421C31 / 0x21C31` 把整个 `0x228` 字节 cursor 保存到调用栈，再在 `0x421C47 / 0x21C47` 切子 sub。所以仅当前 subId 会变成攻击子脚本，必须恢复阶段父层；练习又可能嵌套 call 新阶段，故白名单搜索由内向外，不能优先匹配栈底旧阶段。

**订正旧报告对工作 cursor 的建议**：解释器内必须跟 `*(H+0x2CA0)`，但在一帧末统一采样主脚本时应配对主 cursor 和主深度：

```text
主 cursor = H+0x7F8
主栈     = H+0xA20，帧长 0x228
主深度   = i16(H+0x2CE8)       // 已提交值
当前sub  = i16(H+0x7F8+0x224)
栈帧sub  = i16(H+0xA20+i*0x228+0x224)
```

证据：`0x41EB6A / 0x1EB6A`–`0x41EB71 / 0x1EB71` 主上下文结束把工作深度 `+0x2CEA` 提交到 `+0x2CE8`；子上下文则在 `0x41EB91 / 0x1EB91` 提交到子对象 `+6`。解释器会在 `0x41EC2E / 0x1EC2E` 切工作 cursor 到子对象 `+8`，最后 `0x41EC9E / 0x1EC9E` 恢复为主 cursor，**但没有同样恢复工作深度**。因此帧末不得把主 cursor 与残留 `H+0x2CEA` 盲目配对。同步攻击 call 已在主栈；若未来须查异步上下文，必须将其 cursor、栈、已提交深度分别成组读取。计时转阶段清理异步对象的循环上界 4，证据 `0x42BBB1 / 0x2BBB1`。

建议采样放在一帧宿主更新完成后，读取主状态，限制深度 0–16，逐层检查 subId。若挂解释器内部则以工作指针/深度为准，两种时机不能混用。2 帧去抖仅作为参照 th06 的实现候选，需验证是否漏掉短阶段。

## 3. 三条转阶段路径

| 路径 | 字段/直接证据（VA / RVA） | 风险 |
| --- | --- | --- |
| HP 阈值 | `lifeThreshold` opcode `0x85` 写 `H+0x3358[k]` 和 `H+0x3368[k]`；检查函数 `0x42B490 / 0x2B490`，`0x42B504 / 0x2B504` 比较当前 HP 与阈值，`0x42B53F / 0x2B53F` 取目标，`0x42B557 / 0x2B557` 调 SetSub | 本作有 4 个阈值槽，不能照抄 th06 的单阈值字段 |
| 计时阈值 | `timerThreshold` opcode `0x86` 写 `H+0x3378`(time)、`H+0x337C`(sub)；`0x42BA53 / 0x2BA53` 取目标，`0x42BA69 / 0x2BA69` 调 SetSub | 紧接 `0x42BA88 / 0x2BA88` 把 next 字段改成 `H+0x2CEE`，事后读 next 会读错 |
| HP 归零回调 | `0x42D516 / 0x2D516` 检查 HP ≤0，随后检查 `H+0x2CEE`；`0x42DAE4 / 0x2DAE4` 取该 sub，`0x42DAFA / 0x2DAFA` 调 SetSub | 清工作栈和阈值、回调清为 -1；仅观察 HP 变化不足以认定新阶段 |

`setNext` 是 opcode `0x82`，阶段候选 CSV 逐行列出实际目标；阶段结束时 next 指向的可能是包装 sub，之后才声明符卡。若改用钩子，三条 SetSub 调用点可作为采样候选，必须经过 Boss 身份/槽及本关白名单过滤；不要把普通 call/异步脚本的所有 SetSub 都算转阶段。

## 4. L / Extra 候选映射

以下数字为 SubN 的 N，按记录名数字顺序列举，**不是直接可上线的白名单**。每个候选的具体指令偏移、run mask、spell id、decl 行号及文件指纹见 CSV；脚本逐条复算。文件映射 ST6A→ecldata6、ST6B→ecldata7、ST7→ecldata8，不能把游戏 stage 直接拼文件名。

| 关卡 | MID | BOSS / END_NS | LS / END_S / END_LS | 行数 |
| --- | --- | --- | --- | --- |
| ST1 | 15,22 | 26,38,33,44 | LS=48 | 7 |
| ST2 | 17,23 | 27,38,29,44,52 | LS=58 | 8 |
| ST3 | 20,30 | 35,44,47,38,62,67 | LS=71 | 9 |
| ST4A | — | 17,28,19,30,25,33,26,41,46 | LS=54 | 10 |
| ST4B | — | 17,36,22,41,28,47,31,55,62 | LS=67 | 10 |
| ST5 | 38,45 | 51,62,53,66,56,63,75 | LS=78 | 10 |
| ST6A | 17,21 | 33,52,36,56,40,63,44,68,73 | LS=78 | 12 |
| ST6B | 15,19 | 27,50,31,55,34,61,38,67,71 | LS1–5=77,80,82,84,87 | 16 |
| ST7 Extra | 51,56,60 | NS1–8=66,68,70,72,74,76,78,80 | S1–10=93,97,100,103,108,113,118,126,131,143；END_LS=149 | 22 |
| 合计 | | | | **104** |

难度过滤直接按指令 `+9` 的 run mask：普通关 bit3(L)，Extra bit4；不是“带 !L 则跳过 L”。各文件可复用相同 SubN，因此查表键必须包含实际文件/关卡和难度。A/B 路线分别有独立文件及不同 sub，不能共表。

以 ST1 为可复核样例：MID1 Sub15 的 `0x1614` 阈值目标22；Sub22 在 `0x32B4` 声明 L 符卡1。关底 Sub26 的 `0x3C14` 阈值目标38、`0x3BF0` next33；Sub38 声明符卡5、next33；Sub33 的 `0x4EA0` 阈值目标44；Sub44 声明符卡9。故 BOSS1–4 是26→38→33→44，绝不是时间轴入口的四个不同点。

### 4.1 包装 sub 与未闭合的验证

本表优先选实际战斗/符卡 sub，包装 sub 可由 latch 跨过，但必须真机验证入口时机：ST2 BOSS5 的51→52、ST4A BOSS9 的45→46、ST4B BOSS9 的61→62、ST6A BOSS9 的72→73、ST6B BOSS9 的70→71；这些包装 sub 自身也会设置 HP/计时，若有效战斗发生在包装窗口，需补别名或入口偏移。

LS 有进入条件，且部分 LS 在槽0反注册后才切槽1；不应按 CSV 下标自动推进。ST6B 五张 LS 各有独立符卡 sub。Extra 的第9/10张符卡前有准备 sub82/83，但 CSV 无 END_NS9/10，不能把这两段另算不存在的 checkpoint。

**104/104 仅是输入覆盖与字节证据覆盖，不是验证完成率**。尚未实现 CFG 符号执行来证明每条 HP/timeout/击破分支均到达同一候选，也未穷举机体变量、LS 条件和实际子上下文寿命。当前无候选的范围为 E/N/H、17 LW；当前有候选但待真机的范围为全部104行，包装窗口尤其须先核查。

## 5. 复现与后续实施

```powershell
& C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe docs/logger/tools/investigate_th08_boss.py
& C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe docs/logger/tools/investigate_th08_boss.py --disasm
```

默认从 `../reasm/exes` 读素材；`--materials` 可覆盖。默认只核对已生成 CSV；`--write` 重生成该候选 CSV（不修改 checkpoints）。`--disasm` 校验 exe SHA256 后输出所引 PE 锚点，需 pefile/capstone。

本轮检查通过：104 个记录名与非 LW Boss 输入集合完全一致，所有候选 sub 有效，decl 与原始 sub 指令数逐一相等，关键指令 opcode 一致，run mask 下均有阶段设置证据；9 个 ECL 文件各自保存 SHA256。没有运行游戏、没有编译或修改游戏代码。

后续先做槽0/1、主 sub/栈、工作与提交深度的只读采样，对上述包装窗口与 LS 路径逐一核对；再将候选收敛为正式阶段白名单。监控应在 Boss 窗口内优先阶段匹配，无命中 no-op；道中继续读时间轴。重开、换关、文件切换复位；Boss离场后恢复章节裁决，避免时间轴开战入口覆盖阶段 latch。

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-11-01 | 按 th06 方法独立重查 th08；确认 Boss 槽与三条 SetSub 路径，订正帧末主 cursor 应配已提交深度；新增104行 L/Extra候选映射及字节复算工具，保留17 LW空值，明确候选不等于E4完成；未真机。 |
| 2026-11-01 | 按用户授权接入监控与日志实现；104候选加9个经直接call核对的包装别名，帧末主栈读取与L/Extra门控；新增实现报告，候选证据等级不变，未真机。 |

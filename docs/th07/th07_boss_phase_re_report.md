# TH07 Boss 阶段数据方案重查（2026-11-01）

> **同日后续实施更新（当前状态）**：用户授权实现后，已继续调查并生成 `docs/logger/th07_boss_phases.csv`：103条sub规则覆盖102个Boss记录，另`ST6 CHAOS`走时间轴，覆盖原103个非章节记录；道中37行与Boss均已接入TH07Logger。ST4使用controller＋实际body栈联合判定，Ex/Ph按difficulty4/5；详细映射、源码/字节交叉证据及实现边界见 `docs/th07/th07_monitor_impl_report.md`。**以下“4行候选、99行未定”是本次首轮调查快照，已由上述后续静态字典取代；仍未真机。** 普通关Boss仅L。旧4行候选CSV只保留为调查证据，不供运行期消费。

> 上位文档：`docs/logger/logger_re_disasm_overview.md`；需求：`docs/req_logger.md`。
> 方法参照：`docs/th06/th06_boss_phase_re_report.md`（Boss 宿主 → 当前执行上下文 / 调用栈 → 阶段白名单；只借方法，字段逐条重新反汇编）。
> 输入：`docs/logger/th07_checkpoints.csv` 的 103 个非章节记录，原 CSV 不改。
> 素材：`D:/workshop/reasm/exes/th07.exe`、`ecl/th07/ecldata1..8.ecl`、`ecl_dump/th07/*.decl`，仓库外只读。
> 二进制指纹：650752 B，TimeDateStamp=1066942337，ImageBase=0x400000，SHA256=`f7d03ba1bf6733a60beb3935e0efbbf1436c3d28b4f2155e7e410ba806ce0b9f`。
> 方法：Python + pefile + capstone x86-32；原始 ECL 指令边界解析、与 decl sub 数和源码练习补丁交叉检查。复现见 §6。
> 状态：**🟡 静态调查；未真机，未实现监控**。已获得读取方案和 ST1 关底四阶段静态链；**不是 103 行 E4 完成**。
> 地址口径：指令 / 静态地址均列 VA / RVA（RVA=VA−0x400000）；`E+…` 是敌机内字段，ECL `0x…` 均是文件偏移，不能当作 VA。

## 0. 结论速览

| 问题 | 本轮结果 | 状态 |
| --- | --- | --- |
| 数据是否删光 | 撤的是旧 Boss 分析，103 行 CSV 仍在；同一 Boss 共用 warp 时间，非空不代表阶段可识别 | 🟡 |
| 是否真的没有 subId | **有**。当前上下文 `C=E+0x6E4`，`u16(C+0x214)=u16(E+0x8F8)`；旧 E1 的“无 subIdx”只适用于 timeline，不能推广到敌机 VM | 🟡 |
| 能否照 th06 读栈 | 可借方法：当前 sub → 栈顶至栈底。TH07 帧长 0x218，帧 subId 在 +0x214；并保存返回 ip，用于共享 sub 的调用点消歧 | 🟡 |
| 引擎是否没有 HP 阈值分叉 | **存在四组 HP 阈值及其目标 sub**，不能沿用撤稿说明中的笼统否定 | 🟡 |
| 推荐方案 | Boss 槽的上下文 / 调用栈 + 逐阶段白名单；对共享控制 sub 再看调用点和已确认的转移，不按 CSV 顺序盲推 | 🟡 |
| 已有具体实例 | L 的 ST1 关底：`Sub38 → Sub42 → Sub39 → Sub55`，四条 CSV 记录有静态候选 | 🟡 未真机 |
| 覆盖 | 103 行输入全量审计；72 行存在 helper 补丁点；阶段候选仅 **4/103**，其余 **99 行未定** | 🟡 |

## 1. 读取链：实际存在的 subId 与调用栈

| 字段 / 操作 | 值 | 证据 VA / RVA |
| --- | --- | --- |
| Boss 槽数组 | `S=0x12FE098` / `0xEFE098`，`E=*(u32*)(S+4*k)`，k=0..7 | 注册 opcode99 分支从 `0x413CFA / 0x13CFA` 开始；`0x413D70 / 0x13D70` 写槽，`0x413DD5 / 0x13DD5` 写 E+0x2E17 槽号；注销 `0x413E22 / 0x13E22` |
| 当前上下文 | `C=E+0x6E4`；C+0=ip、C+0xC=脚本时间 | call 装载 `0x411F82–0x411F91 / 0x11F82–0x11F91`；VM 时间比较 `0x4106DD / 0x106DD` |
| 当前 subId | `u16(E+0x8F8)` | `SetSub=0x40E530 / 0xE530`，在 `0x40E5A1 / 0xE5A1` 执行 `mov word [edx+0x214],ax`；edx 是 C，ax 是目标 subId |
| sub 表 | `*(u32*)0x134793C` / `0xF4793C`，每项 u32 指针 | `0x40E540–0x40E549 / 0xE540–0xE549`：按 subId 取指针写 C+0 |
| 栈 | E+0x8FC，stride=0x218，depth=i32(E+0x2A7C) | call 在 `0x411F54–0x411F7B / 0x11F54–0x11F7B` 将 C 的 0x86 dword 整份压栈，`0x411FDB / 0x11FDB` 更新深度 |
| 帧 subId / 返回点 | `frame+0x214` / `frame+0` | 同一份 0x218 上下文的复制；call 先将当前 ip 更新为下一条指令，见 `0x411F29–0x411F38 / 0x11F29–0x11F38` |
| 无压栈模式 | `E+0x2E2A` bit5=1 时 call 不保存、不增长深度 | `0x411F41–0x411F52 / 0x11F41–0x11F52` 与 `0x411FAE–0x411FBE / 0x11FAE–0x11FBE` |

TH06 的“由内向外找白名单”可以迁移，数值偏移不能迁移。当前 subId 可能是共享攻击脚本，不能直接当阶段号；同一攻击子脚本可从多个阶段调用。TH07 的栈同时保留调用者 sub 和返回 ip，后者允许区分**同一容器内不同 call**，但仅在已有逐条控制流证据时使用。

Boss 槽是直接可读的静态数组，无需新增捕获 hook。需要遍历并验证槽号、当前 subId 范围及有效指针，ST4 多敌机时禁止默认只读 slot0。Boss 注册/注销与真实战斗覆盖仍待真机。

## 2. 阶段迁移机制：撤稿后的重新核验

| 路径 / 指令 | 字段与作用 | 证据 VA / RVA |
| --- | --- | --- |
| HP 阈值 | `E+0x2EBC+4*i`（i=0..3）与当前 HP `E+0x2BB8` 比较；低于阈值才转移，目标 `u16(E+0x2ECC+4*i)`；钳 HP、清阈值/时限并清调用深度 | 函数 `0x41FD70 / 0x1FD70`；比较 `0x41FDB9–0x41FDC6 / 0x1FDB9–0x1FDC6`，目标装载/SetSub `0x41FDE8–0x41FDFF / 0x1FDE8–0x1FDFF`，depth=0 `0x41FE7C / 0x1FE7C` |
| 超时 | 当前阶段计时 `E+0x2BCC >= E+0x2EDC`；目标 `E+0x2EE0`；SetSub 后目标被回填为死亡回调 | `0x41FFE1–0x41FFF4 / 0x1FFE1–0x1FFF4`；`0x420085–0x42009C / 0x20085–0x2009C`；`0x4200B4–0x4200BA / 0x200B4–0x200BA` |
| HP0 回调 | `E+0x2A84`，设置于 opcode107；死亡路径把此 sub 装入 C，并清回调 | opcode107 `0x414A85–0x414A95 / 0x14A85–0x14A95`；更新死亡路径的 SetSub `0x421D7F–0x421D95 / 0x21D7F–0x21D95` |
| opcode110 | 同时设置当前与总 HP：E+0x2BB8 / E+0x2BBC | `0x414C3B / 0x14C3B`、`0x414C4A / 0x14C4A` |
| opcode112 / 113 | 设置第0组 HP 阈值 / 目标 sub | `0x414E9A / 0x14E9A`、`0x414EDE / 0x14EDE` |
| opcode148 | `(i,hp,sub)` 设置第 i 组阈值与目标，i 应有边界检查 | 跳表 `0x417838 / 0x17838` 的第148项目标 `0x414EE9 / 0x14EE9`；写入 `0x414F58 / 0x14F58`、`0x414FCE / 0x14FCE` |
| opcode114 / 115 | 设置时限并归零计时器 / 设置超时目标 | `0x415013 / 0x15013`、`0x41508C / 0x1508C` |
| opcode133 | 将死亡回调复制成超时目标并重置计时器；这是读阶段链不能漏的一环 | `0x415EED–0x415EF3 / 0x15EED–0x15EF3` |
| opcode108 / 109 | 中断目标表 E+0x2A88+4*i / 待执行中断 E+0x2B08；并非“阶段号” | 写表 `0x414B09 / 0x14B09`；写中断号 `0x414B4E / 0x14B4E`；VM 经 `0x414BB3–0x414BCA / 0x14BB3–0x14BCA` 装载目标并压栈 |

运行掩码取指令 +9 字节，与全局难度掩码相与：`0x4106F5–0x410703 / 0x106F5–0x10703`。含 bit3 才是 L 执行。opcode148 等不能只按行首常量推链，还要处理变量参数、条件跳转与调用副作用。

2026-10-27 撤稿所说“阶段=sub 且 ip 唯一”的否定仍需尊重：**有 subId 和 HP 分叉，不等于每条 CSV 都有唯一 sub**。但“引擎不按 HP 阈值分叉”这一笼统说明，已被本轮机器码反证，应由本节取代。

## 3. ST1 关底完整四阶段实例（L，静态候选）

ECL 表头的 sub 偏移表从 **0x44** 开始（不是 TH08 的 0x48），SubN 与 decl 的索引逐文件一致。下面全是 `ecldata1.ecl` 文件偏移，不是 VA。

开战宿主 Sub31 在 0x3CE0 用 opcode99 注册 slot0，在 0x3D58 用 opcode108 `(38,0)` 设置中断0的目标为38；中断执行经 §2 的压栈 / SetSub 路径进入首阶段。

| CSV 记录 | 阶段 sub | 关键原始指令及转移 |
| --- | --- | --- |
| ST1 BOSS1 | Sub38 | 0x4A70 op110 HP=15000；0x4A80 op148 `(0,1700,42)`；0x4AC4 op115 超时目标42；0x4AE4 op107 死亡回调39；普通 HP 阈值/超时均入 Sub42 |
| ST1 BOSS2 | Sub42 | L 分支在 0x59B0 op90 声明 spell id5；阶段执行 opcode133，将从 Sub38 继承的死亡回调39设为超时目标；击破/超时均到 Sub39 |
| ST1 BOSS3 | Sub39 | 0x4DC4 op110 HP=15000；0x4E00 阈值2000；0x4E30 L 目标55，0x4E70 L 超时目标55；0x4E90 死亡回调51 |
| ST1 BOSS4 | Sub55 | 0x832C 声明 spell id9；opcode133 将继承的死亡回调51作为超时目标；击破/超时到 Sub51（收尾，不加 checkpoint） |

源码练习路径交叉验证：BOSS2 的 `ECLSetTime(ecl,0x4C20,60,0)` 把 Sub38 内的普通 call32 覆盖为“时限归零”以立刻切 Sub42；BOSS3 把同点的 call 目标改成39，从 Sub38 直接嵌套进入 Sub39；BOSS4 再把 Sub39 的 0x4F44 普通 call40 覆盖成归零时限，以进入55。这解释了为什么练习时栈可能同时含 Sub38 和 Sub39：必须优先内层阶段，不能拿最外层阶段覆盖。

数据见 `docs/logger/th07_boss_phase_candidates.csv`。**4 行仅静态候选，不能算真机已验证或完整 E4。** 不能据此推 ST1 MID 或其它关。

## 4. 补丁落点不等于阶段入口：可重复反例

`ST2 MID2` 的 `ECLSetHealth(ecl,0x5444,0,0x833)` 覆盖的原始指令是 **Sub40 的 t=60 opcode41（call41）**，不是原生 HP 阈值指令。审计 CSV 给出原始 opcode/参数，不能把此 helper 所在 sub 自动登记成 MID2。

`ST1 BOSS2` 的 0x4C20 原本也是 call32，32 是攻击子脚本而非下一阶段。helper 把它改成 opcode114(time=0) 才触发由另处设置好的超时目标42。因此“补丁所在 sub / call 的原目标 / 修改后的阶段”是三件事，不能直接互换。

## 5. 推荐实现方案与未解范围

1. 道中保持既有 timeline 方案。Boss 侧独立读槽与 C / 调用栈；字段和阶段字典分开，保留 CSV 原始 warp 值。
2. 阶段字典最少包含 stage、difficulty/route、phase_sub、记录名；遇到同 sub 多阶段时增加调用者返回点或已证实的阶段事件。先按当前上下文、再由内向外查调用栈。找不到则 no-op，不回退、不按观察到的任意 sub 自增。
3. 对当前无法判别的共享控制 sub，备用在 **HP 阈值 SetSub、超时 SetSub、HP0 SetSub** 入口捕获 `(敌机、转移原因、目标sub、源上下文)`；中断与普通 call 单独记录。TH07 引擎还存在定时子任务，不能把每次 SetSub 都当阶段切换。
4. 首轮以 ST1 BOSS1–4 在自然通关与四个练习入口采样，验证 phase_sub、调用深度、返回 ip、击破/超时双方及同帧覆盖。随后再扩字典，不能按总 CSV 行序推进 ST4 三姐妹和 ST6 分支。

**99 个待定记录**精确清单＝`th07_boss_patch_audit.csv` 全部记录排除上述4条。包括 ST1 MID1/2、ST2–ST8 全部 Boss 区（ST4 三姐妹、ST6 多阶段、Extra/Phantasm 都未宣告覆盖）。本轮覆盖的是读取链和可复查输入，不是完整阶段字典。72 个 helper-site 行仅代表抽取到了证据落点，不代表72个阶段可识别；另外31行仍包含直接 pair、lambda、自然首阶段等，审计工具未把这些补丁伪装成已解映射。

## 6. 工具与验证

仓库根运行（实际 Python 路径可替换）：

```powershell
& 'C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe' docs/logger/tools/audit_th07_boss.py
& 'C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe' docs/logger/tools/audit_th07_boss.py --disasm
& 'C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe' docs/logger/tools/audit_th07_boss.py --write
```

默认只读，重新计算并逐单元格校验三份落盘 CSV；`--write` 仅重建仓库内 `th07_boss_patch_audit.csv`、`th07_phase_instructions.csv` 与 `th07_boss_phase_candidates.csv`。指令表仅保留阶段相关 opcode 和 call/ret，提供8关的相关原始指令证据，**不是阶段映射表**。所有参数以原始 i32 输出，opcode90 的混合字符串/短整数字段不能把整列按 i32 解释为符卡名/id；本报告 spell id 来自对应 decl 语法交叉核对，并用原始混合字段中的高16位另行断言。

实跑：8/8 文件 sub 数与 decl 一致；58/77/60/153/78/74/126/136 个 sub；19919 条有效 sub 指令；103/103 行输入均有源码 case，72行有 helper site；所有 helper 点均是原始指令边界；PE SHA256 与 subId 写入机器码断言通过。未编译、未运行游戏。

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-11-01 | 用户授权落地实现后继续扩查：原99个待定记录均补入静态方案（含CHAOS时间轴），运行期字典103条sub规则覆盖102个Boss记录；ST4控制器＋body栈区分主体，普通L/Ex/Ph齐。新增实现报告说明完整映射、63条源码helper/字节断言和12route中断表检查；保留本报告首轮快照，当前状态以头部实施更新与实现报告为准。未真机。 |
| 2026-11-01 | 按 th06 方法重新独立反汇编：补出敌机 subId、栈、Boss 槽与 HP/超时/死亡/中断读取面；纠正“无 subId/无 HP 阈值分叉”泛化说法；给出 ST1 四阶段静态链；新增103行原始补丁审计和阶段相关指令数据。仅4行静态候选、99行待定，原 checkpoint CSV不改；全部🟡未真机。 |

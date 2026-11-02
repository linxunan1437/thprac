# TH07 段落监控与日志实现

> 上位文档：`docs/logger/logger_re_disasm_overview.md`；需求：`docs/req_logger.md`。
> 技术基线：`docs/th07/th07_boss_phase_re_report.md`；实现模板：`thprac_th06.cpp` 的 TH06Logger。
> 二进制：`../reasm/exes/th07.exe`，650752 B、PE timestamp 1066942337、SHA256=`f7d03ba1bf6733a60beb3935e0efbbf1436c3d28b4f2155e7e410ba806ce0b9f`。
> 方法：既有源码与本地 ECL/decl 只读交叉核对；静态映射生成、独立补丁目标校验；统一 Windows Release|x86 编译/链接已由主代理执行通过（0错误，既有C4715/LNK4099警告）。
> 状态：**🟡 已实现，Release|x86 编译与链接通过，静态字典覆盖原 CSV 140 个记录；用户确认本次中Boss→后半章节交接回归通过；其它项目按实际测试范围继续验证**。普通关 Boss 限 Lunatic，Extra/Phantasm 分别使用实际 difficulty=4/5；道中不限制难度。源码/字典覆盖不等于实际游玩验证通过。
> 地址口径：VA / RVA=VA−0x400000；E+… 为对象相对字段；数据表中 sub_file_start 是 ECL 文件偏移。

## 0. 结论速览

| 项 | 实现 |
| --- | --- |
| 道中 | CSV 原37行 + `ST6 CHAOS`，读取时间轴0时间，取最大已到边界；Boss入场时消费此前旧边界；战斗期间缓存最新章节，退场后交接 |
| Boss | 103条 sub 规则覆盖102个Boss记录（Extra最后一符有两个root别名）；另CHAOS走时间轴，合计覆盖原103个非章节记录 |
| 普通关范围 | ST1–6 Boss仅L；ST4路线取真实控制宿主与三姐妹当前执行栈，不读thFakeShot/机体猜路线 |
| Extra / Ph | ST7 difficulty4、ST8 difficulty5；MID与NS/S字典均登记，不按CSV顺序计阶段 |
| 显示 | IGI miss/bomb区下新增“当前段落”；Logger每帧独立更新，不依赖IGI是否打开 |
| 日志 | GameSession Begin/Tick/Latch、miss/bomb/break、interrupt；复用共享月JSONL与单章节合并 |
| 重开与换关 | 既有stage-init钩子通知；同关/练习新局Begin，正常向后换关保留Session，仅清当前段落/基线 |
| 验证 | 原始ECL sub边界、63条独立helper/原字节目标断言、12路线中断表检查、CSV集合与生成物roundtrip通过；真机待测 |

## 1. 数据产物与生成

- `docs/logger/th07_checkpoints.csv` 原样保留；第一列Boss重复warp时间继续只作练习输入，不当作阶段键。
- `docs/logger/th07_boss_phases.csv` 为运行期实际消费的静态阶段字典，列：stage、difficulty、sub、controller_sub、slot、record_name、sub_file_start、status。
- `docs/logger/tools/build_th07_phase_map.py` 默认只读重算并比较字典；`--write` 更新该CSV。原始sub偏移表从0x44读取，逐sub核decl索引，route标记必须确实是指定slot注册宿主的中断目标；63条另外通过源码helper参数与原始ECL配置目标独立互证。
- `docs/logger/tools/gen_th07_checkpoints.py` 把两份CSV逐字嵌入 `thprac_th07_checkpoints_gen.cpp`，供 `kTh07CheckpointsCsv` / `kTh07BossPhasesCsv` 使用；幂等并校验字节roundtrip。

旧 `th07_boss_phase_candidates.csv` 4行是本次调查第一阶段ST1证据快照，**不是运行期字典**。`audit_th07_boss.py` 仍保留对此历史快照与原始补丁清单的复算（审计表status仅表示补丁点证据，阶段映射请看 `th07_boss_phases.csv`），不把该文件误当完整覆盖。

## 2. 静态阶段字典（普通关L）

以下都是SubN，列表顺序对应记录编号，仅用于列示；运行期以实际sub命中字典，不按行号递增。

| 关卡 | MID | BOSS |
| --- | --- | --- |
| ST1 | 20,29 | 38,42,39,55 |
| ST2 | 40,45 | 49,58,53,66,75 |
| ST3 | 13,22,27 | 32,42,36,48,37,54,56 |
| ST4 | 35 | 路线组合见§3；公共 BOSS3=46、BOSS6=49、BOSS7=51 |
| ST5 | 38,42 | 48,57,52,62,55,75 |
| ST6 | 19,23 | 29,43,34,50,36,52,38,55,58,62 |

明确区分三类证据：

1. `ECLSetTime` 覆盖普通call/设变量等指令以后，归零的是时限，目标取当时有效opcode115配置；不是原call目标。`ECLSetHealth` 把HP设为阈值−1，取opcode113/148的实际阈值目标。
2. `ECLCallSub` / pair覆写call参数直接选择新的阶段上下文；若后面还有归零时限/HP补丁，则继续追到其目标。`ECLForceSpell` 以L低字节设置阈值/超时目标。
3. 自然入口/承接阶段按对应归零补丁所在sub、原中断表/死亡回调与实际decl交叉核对。ST3 MID1时间轴0xB054（文件偏移）t=853 spawn13；ST4 MID1时间轴0x1CEBC t=7122 spawn35，两者均在sub内注册Boss槽0。

特殊链不能漏：ST3 BOSS3=Sub36在t=340依L分支call48，内层48应显示BOSS4，返回36不应被当作阶段后退；ST3 BOSS7=56来自54内HP阈值2000目标56。ST5 BOSS6=75来自55阈值2000目标75，**不是55超时目标70**。ST6 BOSS10从28的中断表目标改62进入，62本身含最终spell声明，攻击子脚本64/65等仅作为段内执行。

## 3. ST4三姐妹：控制阶段与真实body联合判定

Controller slot0的Sub44发中断2，45发3，46发8，47按累计伤害选主体后发4/5/6，48发9，49发10。三姐妹在slot1/2/3分别由Sub53/71/88注册。中断表来自各宿主的opcode108，发送由opcode145触发，不能把某个角色的存在本身当成正在主攻。

| CSV阶段 | controller | Lunasa：slot1活动root | Merlin：slot2活动root | Lyrica：slot3活动root |
| --- | --- | --- | --- | --- |
| BOSS1_* | 44 | 58 | 76 | 95 |
| BOSS2_* | 45 | 64 | 80 | 98 |
| BOSS4_* | 47 | 68 | 83 | 103 |
| BOSS5_* | 48 | 68（其后中断/调用spell128） | 83（spell138） | 103（spell145） |

支援root分别包括63/79/96/97、67/82/101/102、69/85/106，不能据此选择主体。路线另由thprac练习的lunasa/merlin/lyrica lambda把Sub47的选择常量设1/2/3交叉印证；**运行期不读取这些patch值或thFakeShot**。

返回栈保留旧阶段，所以算法先从slot0的“当前→栈顶→栈底”选**最近的控制sub**；再从各body栈由内向外找最近的活动/支援root，支援root会屏蔽更外层旧活动root。如果同一控制阶段真的同时匹配两个主体，保持当前段落（no-op），不猜一个角色。公共controller46/49/51也优先于更外层旧route控制sub。

证据地址：opcode108写中断表 **VA0x414B09 / RVA0x14B09**；中断执行SetSub **0x414BCA / 0x14BCA**，先复制上下文压栈；opcode145向目标槽写待处理中断 **0x416337 / 0x16337**。角色中断表的具体文件偏移可由 `th07_phase_instructions.csv` stage4/opcode108筛选复查。

## 4. Extra / Phantasm

| 关卡 | MID1–3 | END_NS1–8 | END_S1起 |
| --- | --- | --- | --- |
| ST7 Ex | 55,58,61 | 68,70,71,73,75,77,79,81 | 85,88,94,97,99,101,106,110,83,84（最后一符body123亦识别） |
| ST8 Ph | 57,60,63 | 70,72,73,75,77,79,81,83 | 88,93,97,100,102,104,109,114,85,86,87 |

末尾S9/S10/S11包含符前控制root，沿其调用栈保持该记录。ST7原源码注释已明确Sub84为符前控制、Sub123为最后符body。各NS/S入口由原练习helper与原sub中的opcode148/115配置相互验证，不把sub数字跳号改成连续序号。

## 5. 运行期接线与边界

读取地址沿新逆向报告：Boss槽 **0x12FE098 / 0xEFE098**，enemy固定pool **0x9AEA50 / 0x5AEA50**、480槽×0x4F48；校验范围/对齐/槽号。当前subId E+0x8F8；depth E+0x2A7C，0..15；栈E+0x8FC、stride0x218、帧subId+0x214。无保存模式（E+0x2E2A bit5）不读取旧栈；sub越界直接忽略。

道中取 **timeline0时间0x12FE0FC / 0xEFE0FC**，本作既有warp也写该量。Boss刚入场（`hasBoss && !mHadBoss`）时消费当时已到的旧章节边界，避免“直接warp进Boss → 打完Boss → 倒退显示开战前道中”。战斗期间只缓存最新经过的章节（`mDeferredChapter`），不消费边界；Boss退场后将缓存与当时新到边界比较取最新，连续2帧后latch并清缓存。新局/换关清缓存。候选连续2帧才latch；找不到白名单时保持原值，首次命中前为main。

既有patch-main钩子 **0x42F2E3 / 0x2F2E3** 位于关卡初始化收尾，补丁应用后调用OnStageStart；它也覆盖同关fast retry。Logger从既有每帧hook **0x42FDF8 / 0x2FDF8** 更新，早于IGI显示。游戏态使用已有IGI同源 `SUPERVISOR->gamemode==2`（**0x575AA4 / 0x175AA4**）。正常换关事件在 **0x4380F1 / 0x380F1** 临时写gamemode_next=3，随后释放/初始化关卡并恢复next=2；stage-init **0x42E894 / 0x2E894** 按next=3保留局内资源。实现按正常stage增加保留Session，同关/练习stage-init重新Begin。

miss/bomb读取与IGI同源：globals指针 **0x626278 / 0x226278**，miss_count=+0x50、bombs_used=+0x6C；stage-init捕获基线，后续只记正增量。计数下降建立新基线，异常大跳不伪造事件；暂停不增长自然无事件。机体取 **0x62F645 / 0x22F645** 与 **0x62F646 / 0x22F646** 拼IGI_PL_07下标。

撞结界（`break`）同样与IGI同源：既有`th07_border_break` EHOOK（**0x441DA4 / 0x41DA4**，5B）原先只递增`mBorderBreakCount`；该处指令为`mov [0x62F890], eax`（`cherry_plus = globals->cherry_base`），位于结界结束例程**0x441BD0 / 0x41BD0**内，由撞弹（player state `[+0x2408]==4`，调用点`0x43E366` / `0x43EB4E`）、bomb输入（`0x440A26`）及带结界死亡等路径共用。现于同一lambda内追加`TH07Logger::OnBreak()`，一条`break`事件与IGI「撞结界」计数一一对应；不新增hook、不在VEH回调内分配堆。事件checkpoint取当时`mSection`（与miss/bomb同口径，可能滞后一帧）。

退出前先入interrupt，再Tick结束/落盘；最终记录已latch且Boss槽从有到无时将stageClear置位。该收尾判据、窗口2帧、正常跨关不拆日志、最后一符自然结束/练习提前退出、极短阶段与同帧miss等均需真机回归。**没有宣称采用th14–18的spellFlag语义；本轮使用th06式stageClear兜底，符卡完成后提前退出的细粒度interrupt抑制待专项验证。**

## 5.1 真机反馈：ST1/ST2 中 Boss 结束后停留 MID2（2026-11-01）

用户确认：ST1、ST2 自然流程均出现，非练习跳转。**根因在章节消费逻辑**：旧实现每帧 `if (hasBoss)` 都推进 `mLastChapter`，把中 Boss 战斗期间经过的后半入口当成已处理；Boss消失后候选要求 `r.time > mLastChapter`，同一入口再也不能命中。`!candidate` 时按no-op保留原值，因而一直显示MID2，直至其它新白名单命中。战斗期间经过后半入口是常见正常路径。

静态证据：`th07_checkpoints.csv` ST1后半#1=3107、ST2后半#1=3366；时间轴时间 VA/RVA `0x12FE0FC/0xEFE0FC`，每帧计时调用 `0x41FD26/0x1FD26`。ST1 ECL Sub27 在文件偏移 `0x3400` 执行 `opcode99(-1)`（同sub时间全0）；引擎注销路径在 VA/RVA `0x413E22/0x13E22` 将 `0x12FE098/0xEFE098` 对应Boss槽清零。因此不应依赖收尾sub恰好被帧采样命中，继续以Boss槽从有到无作为交接读取面。以上地址仍为🟡静态复核，用户未提供运行期采样。

**修复**：只在Boss入场沿消费此前章节；在场期间缓存新经过的最大章节时间及记录，多个入口只保留最后一个；槽清空后优先交接缓存，并与此时到达的章节取最新，沿用2帧去抖。缓存仅在章节成功latch、新Boss入场或新局/换关时清除。若提前击杀而时间轴尚未到后半入口，则继续等待真实边界。

`verify_th07_chapter_handoff.py` 抽取生产C++的入场沿、章节选择与去抖代码编译运行：覆盖晚杀、早杀、直接Boss warp、经过多个入口、缓存跨较低时间样本、重复采样不回退及下一Boss窗口。旧每帧消费作为负对照，稳定在晚杀交接断言失败；修复版通过。**静态检查之外，用户于2026-11-01确认本次交接修复回归通过。** 该确认只收口本次MID2交接问题，不扩写为全部关卡、日志及所有边界均已验证。

## 6. 检查与真机清单

```powershell
& 'C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe' docs/logger/tools/build_th07_phase_map.py
& 'C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe' docs/logger/tools/gen_th07_checkpoints.py
& 'C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe' docs/logger/tools/audit_th07_boss.py
& 'C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe' docs/logger/tools/verify_th07_route.py
```

静态检查：103条规则唯一、102个Boss记录与CSV求差为空（仅CHAOS按时间轴）；63条helper/原字节目标断言；12route标记核指定body中断表；8/8 ECL sub起点/decl索引；生成物两CSV逐字一致且重复生成不变。`verify_th07_route.py` 直接抽取生产C++的ActiveSister和controller选择代码，以独立小程序验证12个嵌套控制器、支援root遮蔽旧active、符卡body祖先等回归，全部通过。Release|x86编译/链接通过（主代理执行），未真机。

待真机：L自然各关与所有练习Boss入口；ST4三姐妹分别走12路线记录；Ex/Ph自然与符前warp；同关重开、正常跨关、菜单退出、回放；miss/bomb与JSONL起终点/计数、关闭IGI后的独立采样；**结界触发后撞弹 ⇒ IGI「撞结界」+1 且 JSONL 出现 `"type":"break"`，break 条数 == IGI 计数，checkpoint 为当时段落（允许1帧滞后）**；长记录名在IGI中的宽度。

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-11-01 | 用户授权实现后续调查并接线TH07Logger。阶段字典从ST1四候选扩到103规则/102 Boss记录＋CHAOS时间轴，含ST4真实body路线及Ex/Ph；增加生成器与独立源码helper/字节验证，接IGI、GameSession及stage-init基线。普通Boss限L，全部🟡未真机，统一Release\|x86编译通过。 |
| 2026-11-01 | 主代理集成检查：Release\|x86 编译/链接与Win7兼容构建检查通过，产物 `Release/thprac.exe`；MSVC PDB辅助进程在沙箱内报LNK1101，在已授权沙箱外VS开发环境构建成功。仅既有C4715/LNK4099警告；对应生成物/映射静态检查通过，未运行游戏，E6仍未测。 |
| 2026-11-01 | 用户实测ST1/ST2自然流程中Boss结束后滞留MID2；修复每帧吞掉后半入口，改入场沿消费旧边界＋战斗期间缓存最新章节＋退场交接。生产C++帧序列回归/旧逻辑负对照通过；实机待复测，见§5.1。 |
| 2026-11-01 | 本次缓存交接修复的Release Win32（x86）编译/链接及Win7兼容检查通过，测试版 `Release/thprac_th07_handoff.exe`。默认输出thprac.exe无法写入，故另存；沙箱内链接持续等待，改经授权沙箱外MSBuild完成（既有LNK4099）。修复尚未真机复测。 |
| 2026-11-01 | 用户确认本次回归测试通过，中Boss后半入口缓存与退场交接问题收口；当前状态与§5.1同步。未扩写用户未提供的全关/日志测试明细。 |
| 2026-11-02 | 日志新增 `break`（撞结界）：复用既有 `th07_border_break` EHOOK `0x441DA4`（与 IGI `THPRAC_INGAMEINFO_07_BORDER_COUNT` 同源），在同一 lambda 内追加 `TH07Logger::OnBreak()`；共享层与 CSV 无改动。`break` 与 miss/bomb 同为尝试级事件（计入 `got`/miss 统计）。**Release\|x86 编译/链接通过（2026-11-02）；真机待验证（🟡）。** |

# TH08 段落监控与日志实现

> 上位文档：`docs/logger/logger_re_disasm_overview.md`；需求：`docs/req_logger.md`；进度：`docs/logger/logger_progress.md`。
> 技术基线：`th08_boss_phase_re_report.md`、`th08_ecl_re_report.md`；参照实现：`thprac_th06.cpp` 的 `TH06Logger`。
> 二进制：`D:/workshop/reasm/exes/th08.exe`，840704B，PE时间戳1095610188，SHA256 `330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924`。仓库外素材只读，不提交。
> 方法：C++ 每帧只读状态，既有/新增整指令钩子记录事件，离线 ECL/decl 与 PE 字节复算。地址同时给 VA / RVA=VA−0x400000。
> 状态：**🟡 E5代码已实现，统一 Release|x86 编译与链接通过（主代理核验）；未运行游戏，E6未测。**普通关 Boss 仅L；Extra用独立难度。候选映射接入实现不代表真机验证通过。

## 0. 结论速览

| 项目 | 实现 |
| --- | --- |
| 章节 | 37行 CSV 时间轴文件偏移边界，lane0 指针 `0xF54CFC / 0xB54CFC`，越过入口仍能命中；Boss期间不消费，退场取最新未latch章节（已静态回归） |
| Boss | 104非LW记录 + 9个同步包装 sub 别名；槽0/1，主cursor+已提交深度，白名单由内向外 |
| 难度/路线 | 9个实际 ECL 文件索引独立路由，普通Boss diff3(L)、Extra diff4；E/N/H只监控道中 |
| 排除 | Native spell practice（flags bit14）整体排除，避免 `*sp` 文件误用正常 sub 表；其中含17 LW，原空值不变 |
| latch | 2个有效采样帧稳定后置值；未命中no-op；初始化/换关/退出清main |
| 日志 | `GameSession("th08")`，启动/结束checkpoint、miss/bomb/interrupt、机体；复用共享JSONL与练习合并 |
| 新局/重开 | 既有43BDD2初始化钩子明确Begin；正常局内文件索引前进保留同一局；同文件重开Begin |
| 通关 | 独立完成分发钩子置stageClear，覆盖练习与最终关；gamemode3本身不作为通关证据 |
| IGI | 既有miss/bomb面板新增Section行，跟随原IGI开关显示 |

## 1. 代码与生成数据

`thprac/src/thprac/thprac_th08.cpp` 新增 `TH08Logger`，在既有每帧更新钩子中、IGI更新前执行。`THGuiCreate` 提前构造单例解析两份CSV，事件钩子中不做CSV解析或动态容器增长。

`docs/logger/tools/gen_th08_checkpoints.py` 生成独立翻译单元 `thprac/src/thprac/thprac_th08_checkpoints_gen.cpp`，导出：

- `kTh08CheckpointsCsv`：原158行checkpoint CSV按UTF-8嵌入，去除可选BOM，保留内容；
- `kTh08BossPhasesCsv`：104阶段sub加9个包装别名，共113行；运行时不读取仓库外素材。

9个别名均在decl中直接同步call其正式候选，生成时断言：ST2 Sub51→52、ST4A45→46、ST4B61→62、ST6A72→73、ST6B70→71、Extra55→56、59→60、82→131、83→143。包装期和攻击期归同一记录，不新增checkpoint；同帧父阶段和子阶段同时在栈中时优先取子层。不存在按CSV下一行自动推进。

## 2. 运行时读取与边界

实际文件索引 `0x164D2CC / 0x124D2CC` 为0–8，对应ST1、ST2、ST3、ST4A、ST4B、ST5、ST6A、ST6B、Extra。PE 文件名表 `0x4C73CC / 0xC73CC` 的9个条目已逐一核对，不能用索引+1拼ST名。

时间轴lane0为 `*(u32*)0xF54CFC`，减 `pECL=*(u32*)0x4ECCB8`（RVA `0xECCB8`）获得文件偏移；文件魔数0x800、subCount 1–1024、laneCount 1–16、偏移0–0x20000范围检查。9个正常文件均在此范围内。章节按入口偏移升序，取最后一个≤当前偏移的条目。只允许越过已latch的章节入口，Boss期间不采章节。

**不能把Boss期间的时间轴指针抬为章节floor**：ip是下一条待执行命令，MID期间可能提前指向后半入口；若把它记为floor，会永久漏掉该入口。实现floor仅记录已实际latch的章节；Boss消失后可命中尚未latch的后半章节。

Boss槽 `0xF54CC0 / 0xB54CC0` 和 `+4`；先1后0以覆盖ST6A LS交接。宿主必须位于数组 `0x57D2F0 / 0x17D2F0` 起、480槽、步长0x53D0、对齐正确，且flags `H+0x3324 &3 ==3`。不跟随任意Boss指针。

读主cursor `H+0x7F8`，当前sub `H+0xA1C`；栈 `H+0xA20`、帧0x228、sub在帧+0x224；深度用 `H+0x2CE8` 已提交值且0–16。工作深度2CEA可残留异步上下文，不用于该帧末主栈采样。每个sub须小于文件头subCount。当前sub→最近保存帧→更外层依序查本关白名单。

2帧去抖只在非暂停的有效采样上累加；事件使用最近latch值，因此换阶段的最初帧仍可能记录上一阶段，需E6实际核对。换文件、初始化、新局、退出复位；同文件时间轴明显回退作为重开兜底，同时Begin日志，主要新局判据仍是明确的初始化钩子。

## 2.1 对照 th07 MID2 滞留问题的核查（2026-11-01）

**🟡 静态结论：未发现 th07 同样的“Boss在场每帧消费后半入口”缺陷，无需修改运行期实现。** th08 的 `mChapterFloor` 只在章节成功latch时更新；Boss期间 `if (!bossPresent)` 的章节选择块不执行，也不推进floor。Boss槽清空后，从本文件全部 `chapter.offset <= offset && chapter.offset > mChapterFloor` 的章节中取最后一个（构造时已按offset升序）。因此战斗期间经过一个或多个入口均未被消费，退场后能交接到最新章节，连续两个有效采样帧确认；没有命中时保持原段落。

两作读取面的差异必须保留：th07按时间值缓存最新章节；th08按单调时间轴偏移重新查询未消费入口，达到保留最新入口的效果。th08 lane0当前指针 VA/RVA `0xF54CFC/0xB54CFC` 是下一条待执行命令，可能在Boss入场时已指向后半入口；故不能机械移植th07“入场沿消费旧入口”逻辑，否则会把该后半入口抬为floor。Boss读取面为 `0xF54CC0/0xB54CC0` 的槽0/1和宿主有效位，具体校验见§2。

新增 `docs/logger/tools/verify_th08_chapter_handoff.py`，直接抽取生产C++章节选择及两帧去抖代码，使用原CSV的37个真实章节偏移、覆盖9个正常文件路线（ST1/2/3/4A/4B/5/6A/6B/7），模拟Boss期间推进到最新入口、退场后交接、早退等待边界、重复采样不回退。当前实现全部通过；另注入“Boss在场更新floor”的负对照，能稳定检出错误。模拟帧序列不等同于实际Boss时间分布，也不提升E6；**未运行游戏，仍待实机测试**。本次仅新增回归脚本与文档，没有修改th08运行期代码或生成物，无需重新构建。

## 3. 日志生命周期及事件证据

局内沿用th06：练习仅gamemode2，普通游玩gamemode2/3以跨关保留日志；真正采样只在2且非暂停进行。43BDD2初始化时，同文件/回退或离开游戏后的新局Begin；普通局内文件索引前进则只清章节缓存，不拆整局。退出下降沿先入interrupt，再Tick落盘。机体取既有 `IGI_PL_08[shottype]`，与IGI同源；只接0–11有效机体编号。

| 钩子 VA / RVA | 原字节、整指令长度 | 用途与判据 |
| --- | --- | --- |
| `0x43BDD2 / 0x3BDD2` | `BF 34 CF 64 01`，5B | 既有enter_game，清本次统计块前通知Begin/复位 |
| `0x44CA94 / 0x4CA94` | `8B 15 A8 CF 64 01`，6B | 普通bomb读取计数前记一次；后面给0x164CFA8加1 |
| `0x44CA83 / 0x4CA83` | `8B 0D AC CF 64 01`，6B | 所有成功deathbomb共同汇合点，读取计数前记一次 |
| `0x44CD29 / 0x4CD29` | `E8 32 15 00 00`，5B | 唯一call0x44E260，在游戏给miss计数加1前记Miss |
| `0x439C1F / 0x39C1F` | `C7 05 40 F5 60 01 01 00 00 00`，10B | 完成状态分发，置stageClear，位于练习/正常/最终关分支之前 |

Miss计数证明：被调函数 `0x44E2B0 / 0x4E2B0` 读 `GM+0x3DA9C`，+1后写回；GM=`0x160F508 / 0x120F508`，故地址=`0x164CFA4 / 0x124CFA4`，与IGI同源。不是碰撞进入决死窗口就算miss，成功deathbomb不会调用该生命损失路径。

Deathbomb覆盖证明：已有 `0x44CA6A / 0x4CA6A` 只覆盖资源≥2的一条分支；资源不足或另一决死路径分别在 `0x44CA38 / 0x4CA38`、`0x44CA68 / 0x4CA68` 跳到共同 `0x44CA83`。该路径计数后 `0x44CA92 / 0x4CA92` 无条件跳 `0x44CAAF / 0x4CAAF`，越过普通bomb点，两钩子互斥。原CA6A空钩子保留但不接日志，避免双记。

通关证明：`0x439BFE / 0x39BFE` 取flags第5–6位，`0x439C01 / 0x39C01` 要求值2，随后 `0x439C13 / 0x39C13` OR0x60提交为3，进入完成记录/换关/结束分发。新增钩子在该一次性分发内，早于 `0x439D29 / 0x39D29` 的练习分支和最终关判断。不能直接把gamemode3或Boss槽消失当通关。stageClear仅新局/换关/退出复位，不因同帧latch覆盖掉。

既有desolve钩子 `0x44ABE9 / 0x4ABE9` 保持原行为，没有当成真正Miss。未新增spellFlag计数器，卡结束后立即退出仍可能记interrupt，沿用th06阶段日志基线而非th14–18扩展口径；待真机后评估。

## 4. 检查与待测

```powershell
& C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe docs/logger/tools/investigate_th08_boss.py
& C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe docs/logger/tools/gen_th08_checkpoints.py --check
& C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe docs/logger/tools/verify_th08_logger_hooks.py
& C:/Users/lxn/AppData/Local/Python/pythoncore-3.14-64/python.exe docs/logger/tools/verify_th08_chapter_handoff.py
```

已通过104个候选sub的decl/原始指令数量、关键opcode和数值参数、难度run mask复算；9包装别名直接call断言；生成物字节一致性；5处钩子指纹、原字节与整指令长度；miss唯一调用、deathbomb汇合/互斥和文件名索引验证；新增C++范围按根.clang-format格式化，git diff --check通过。

尚未真机：L各关道中/MID/关底、4A/B与6A/B切换、Extra全阶段、正常/死亡bomb、同关fast retry及跨关局不拆分、ST6A槽1 LS、ST6B五张LS及条件不满足、最终关/练习通关与中退、native spellpractice/LW不误标。2帧去抖和包装sub的实际采样窗口仍需实测，代码接入不提升候选映射的验证等级。

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-11-01 | 参照th06实现TH08Logger、37章节+104非LW阶段/9包装别名、IGI与GameSession；独立补证真正miss、完整deathbomb汇合和通关分发钩子，新增生成/验证工具；Release|x86 编译与链接通过；未真机。 |
| 2026-11-01 | 主代理集成检查：Release\|x86 编译/链接与Win7兼容构建检查通过，产物 `Release/thprac.exe`；MSVC PDB辅助进程在沙箱内报LNK1101，在已授权沙箱外VS开发环境构建成功。仅既有C4715/LNK4099警告；对应生成物/映射静态检查通过，未运行游戏，E6仍未测。 |
| 2026-11-01 | 对照th07中Boss结束MID2滞留问题：th08仅章节latch时更新floor，Boss期间不消费入口，未发现同类缺陷；9文件路线生产C++交接回归及错误floor负对照通过。保留原实现，新增验证脚本，E6仍未测，见§2.1。 |

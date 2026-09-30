# 游戏时长统计 — 逐作标志位总表

> 本文档是"全作游戏时长统计"功能的**活文档**：跨作汇总每作的三个门控标志位（地址 + 判据 + 验证状态），随 P2 分批推进不断补齐。
> 总体方案见 `docs/gametime/gametime_plan.md`。逐作的反编译需求见 `docs/<作>/<作>_re_disasm_requests.md`，结论回填到 `docs/<作>/<作>_re_disasm_report.md`。
> **需要真机验证的清单见 `docs/gametime/gametime_runtime_verification.md`**（本文档只记静态结论，标 🟡 的都以该清单为准）。

---

## 0. 约定

### 判据格式
"正在有效游玩" = 下列三个量**合取**：

| 量 | 语义 |
| --- | --- |
| `gamemode` | 关卡进行中（排除标题 / 菜单 / 选人 / 结算 / 剧情对话） |
| `pause` | 游戏自身暂停菜单**未**打开（不是 thprac 的 `THPauseMenu`） |
| `replay` | 未在播放 replay |

代码里的表达形式（见 `docs/gametime/gametime_plan.md` §1.4）：

```cpp
enum class GateCmp : uint8_t { Eq, Ne, BitClear };
struct THGameTimeFlag { uint32_t rva; uint8_t bytes; uint32_t value; GateCmp cmp; };
struct THGameTimeGate { THGameTimeFlag gamemode, pause, replay; };
```

本文档表格里的写法：`Eq` / `Ne` / `BitClear` 分别记作 `== v` / `!= v` / `& m == 0`。

### 地址格式
- **统一用 RVA（模块内偏移）**，即 `RVA = 绝对 VA − ImageBase`。
- **A/B/C 代（th06–th18 等）**：`ImageBase = 0x400000` 且 `RELOCS_STRIPPED` 不重定位，故 `RVA = VA − 0x400000`。本文档为便于查阅，同时列出绝对 VA。
- **D 代（th19 / th20）**：会重定位，**必须**用 `base + RVA`，`base` 由 `GetGameModuleBase(hProc)` 取得。

### 指针链格式（B/C 代普遍形态）
A 代（th06–th08）的宿主对象（`Supervisor` / `GameManager`）**是静态对象**，字段 = `地址 + 偏移`，单级静态读即可。
B 代起（th10+）宿主对象改为**堆对象、挂在静态指针变量后面**，字段 = `*(u32*)指针 + 偏移`，需要**两级读**。本文档写法：

- **单级（静态对象字段）**：`0xAAAAAA + 0xNN` —— 读 `*(类型*)(base + 0xAAAAAA + 0xNN)`。
- **两级（指针链）**：`[0xAAAAAA] + 0xNN` —— 先 `p = *(u32*)(base + 0xAAAAAA)`，再读 `p + 0xNN`；`p == 0` 视为本次读失败（保守不计时）。

> ⚠️ 现有 `THGameTimeFlag` **只支持单级**（`ReadProcessMemory(base+rva)`，`thprac/src/thprac/thprac_launcher_others.cpp:66`）。要用 th10 / th11 / th15 的 pause，必须给结构补第二级偏移（`rva2`）。依据见 §1"第二组实证"。

### 状态图例
| 标记 | 含义 |
| --- | --- |
| ✅ | 已确认，且通过三态验证 |
| 🟡 | 静态反编译已定案（地址 / 判据齐全），**尚未**真机三态验证 |
| ⚠️ | 有线索（已知指针 / hook 锚点），但字段偏移或判据待定 |
| ❌ | 待反编译 |

末列"状态"：**已接入** = 三个判据都已写进 `thprac_launcher_games_def.h` 的 gate 表，代码侧可计时；**完成** = 反编译定案但尚未接入代码。

---

## 1. 引擎分代表（**待验证**）

同代内引擎同构，三个量的"结构位置与判据形态"往往一致，因此 RE 策略是"每代挑一个参照作摸透 → 同代按同构搬运 → 逐作三态验证"。

⚠️ **本表目前是基于代码形态的推测，尚未用证据验证。在验证前不要把它当作"同构搬运"的依据。**

**✅ 第一组实证（2026-09-29，th07 ↔ th08）**：th07 与 th08 的暂停/回放实现**逐条同构**，A 代"同构搬运"在 th07↔th08 上成立。对照表：

| 概念 | th07 | th08 | 形态 |
| --- | --- | --- | --- |
| `Supervisor` 静态基址 | `0x575950` | `0x17CE758` | — |
| `gamemode` | `+0x154` (`0x575AA4`) | `+0x158` (`0x17CE8B0`) | `== 2` |
| `GameManager` 静态基址 | `0x626270` | `0x160F508` | — |
| `flags` dword | `+0x93D8` (`0x62F648`) | `+0x3DBAC` (`0x164D0B4`) | **bit3 = replay，bit2 = 关卡中∧未暂停∧不在续关菜单** |
| 暂停字节 | `+0x93DC` (`0x62F64C`) | `+0x3DBB2` (`0x164D0BA`) | **0/1/2 三值** |
| 暂停菜单状态机 | `0x402780` | `0x4037B0` | 同构（case 4=恢复 / 9=回标题 / 10=退出） |
| 关卡内每帧更新 | `0x42D8D5` | `0x439BC7` | 同构 |
| 暂停置 1 指令 | `0x42D958` | `0x43A119` | `input & 8` 新按下 |
| replay 置位指令 | `0x45B395` | `0x46ED45` | `flags & ~8 \| 8`，随后 `gamemode_next = 2` |
| thprac unpause 钩子 | `0x403481` | `0x40421A` | 均在暂停菜单机 case 4 |
| 输入字 | `0x4B9E4C` | `0x164D528` | 暂停键位都是 `0x8` |

⇒ **th09 / th095 可以直接按这个模板找**（两个静态结构 + 两个 bit），不必从头摸。但**仍需逐作三态验证**（尤其要确认 `flags` 的基线值非 0 时只做位测试）。

**✅ 第二组实证（2026-09-30，th10 ↔ th11 ↔ th15 的"堆对象 + 静态指针"形态）**：B/C 代的宿主对象**不是静态对象**，而是**堆分配、由静态指针变量引用**。三作的暂停位都是"宿主对象 `+偏移` 的 bit `0x10`"，但对象必须经一级指针取：

| 概念 | th10（MoF） | th11（SA） | th15（LoLK） |
| --- | --- | --- | --- |
| 宿主对象指针（静态变量） | `0x477810`（GameManager） | `0x4a8e88`（GameThread） | `0x4e9a94`（StageController） |
| 暂停位 | `[0x477810]+0x58` bit4 | `[0x4a8e88]+0x60` bit4 | `[0x4e9a94]+0x90` bit4 |
| 指针被写入处（堆对象） | `0x418136: mov [0x477810],esi` | `0x42031F: mov [0x4a8e88],edi` | `0x43CBE3: mov [0x4e9a94],esi` |
| Pause / UnPause 置清位 | `0x422AB0` / `0x422C30` | `0x42C620`,`0x42C760` / `0x42C880` | `0x451192` / `0x452942` |

⇒ **A 代"静态对象、单级读"的结论不能平移到 B/C 代**；B/C 代的门控需要**二级读**（§0 的指针链写法）。这也是必须给 `THGameTimeFlag` 增加第二级偏移的直接依据。th10 ↔ th11 的三量逐条同构（见 §3）；th15 的暂停位与二者同构，仅偏移不同（这也顺带暴露了 th15 原 pause 地址的建模错误，见 §3）。

| 代 | 作品 | 参照作（三量齐全） | 验证状态 |
| --- | --- | --- | --- |
| **A** | th06, th07, th08, th09, th095 | **th06** | **部分验证**（th07↔th08 同构已证；th06↔th07 形态不同，th06 用的是分散的布尔量）|
| **B** | th10, th11, th12, th13, th125, th128 | **th10 / th11**（§3 已定案） | **部分验证**（th10↔th11 三量同构；宿主对象为堆指针，需二级读） |
| **C** | th14, th15, th16, th17, th18, th143, th165, th185 | **th15**（pause 已更正） | **部分验证**（th15 的宿主对象同为堆指针，与 B 代同构） |
| **D** | th19, th20 | 无 | 待验证（另需注意重定位） |
| — | alcostg | 无 | 优先级最低 |

**待办**：用 1~2 组证据验证分代（例如同代内 `GameManager` / `Supervisor` 结构体形状是否同构、判据写法是否一致），验证结果回填本节，并把结论升级为"可作为依据"。

---

## 2. 逐作标志位总表

| 作品 | 代 | gamemode | pause | replay | 状态 |
| --- | --- | --- | --- | --- | --- |
| th06 | A | ✅ `0x6C6EA4 == 2` | ✅ `0x69D4BF == 0` | ✅ `0x69BCBC == 0` | **已接入** |
| th07 | A | ✅ `0x575AA4 == 2` | ✅ `0x62F64C == 0` | ✅ `(0x62F648 & 8) == 0` | **已接入** |
| th08 | A | ✅ `0x17CE8B0 == 2` | ✅ `0x164D0BA == 0` | ✅ `(0x164D0B4 & 8) == 0` | **已接入** |
| th09 | A | ❌ | ❌ | ❌ | 未开始 |
| th095 | A | ❌ | ❌ | ❌ | 未开始 |
| th10 | B | 🟡 `*(u32*)0x477834 != 0`（PLAYER_PTR） | 🟡 `[0x477810]+0x58 & 0x10 == 0` | 🟡 `[0x477838]+0x10 != 1` | 静态完成，待三态 |
| th11 | B | 🟡 `*(u32*)0x4a8eb4 != 0`（PLAYER_PTR） | 🟡 `[0x4a8e88]+0x60 & 0x10 == 0` | 🟡 `[0x4a8eb8]+0x10 != 1` | 静态完成，待三态 |
| th12 | B | 🟡 `*(u32*)0x4b4514 != 0`（PLAYER_PTR） | 🟡 `[0x4b44e8]+0x60 & 0x10 == 0` | 🟡 `[0x4b4518]+0x10 != 1` | 已接入，待三态 |
| th13 | B | 🟡 `*(u32*)0x4c22c4 != 0`（PLAYER_PTR） | 🟡 `[0x4c2194]+0x60 & 0x10 == 0` | 🟡 `[0x4c22c8]+0x10 != 1` | 已接入，待三态 |
| th125 | B | ❌ | ❌ | ❌ | 未开始 |
| th128 | B | ❌ | ❌ | ❌ | 未开始 |
| th14 | C | 🟡 `*(u32*)0x4db67c != 0`（PLAYER_PTR） | 🟡 `[0x4db558]+0x80 & 0x10 == 0` | 🟡 `[0x4db688]+0x10 != 1` | 已接入，待三态 |
| th143 | C | ❌ | ❌ | ❌ | 未开始 |
| th15 | C | 🟡 `*(u32*)0x4E9BB8 != 0`（PLAYER_PTR） | 🟡 `[0x4E9A94]+0x90 & 0x10 == 0`（**原 `0x4E9B24` 有误，见 §3**） | 🟡 `[0x4E9BC4]+0xc != 1`（**原 `0x4E7ECC != 0xd` 已改，见 §3**） | 已接入，待三态 |
| th16 | C | ❌ | ❌ | ❌ | 未开始 |
| th165 | C | ❌ | ❌ | ❌ | 未开始 |
| th17 | C | ❌ | ❌ | ❌ | 未开始 |
| th18 | C | ❌ | ⚠️ `0x4cf40c` 已知、offset 待定 | ⚠️ `0x4cf418` 已知、offset 待定 | 未开始 |
| th185 | C | ❌ | ❌ | ❌ | 未开始 |
| th19 | D | ❌ | ❌ | ❌ | 未开始（需 base+RVA；两个版本条目） |
| th20 | D | ❌ | ❌ | ⚠️ `0x1c60fc` 已知、offset 待定 | 未开始（需 base+RVA） |
| alcostg | — | ❌ | ❌ | ❌ | 未开始 |

> **2026-09-30 更正**：th11 原记录 `gamemode = 0x4a8e88 + 0x74` 有误——`GameThread+0x74` 是关卡启动时传给 `ReplayManager::Start` 的 mode（`0x41FC27`），不是"关卡进行中"。th12 的类比条目（`0x4b44e8 + 0x74`）**同日复核确认同因**：th12 宿主对象构造器 `0x422755: mov [edi+0x74],eax` 同样存的是传给 `ReplayManager::Start` 的 mode，故已推翻；th12 改用 `PLAYER_PTR != 0`（详见 §3 th12）。
> th15 原 pause 记录 `0x4E9B24` 也有误（那是"指针变量 `0x4E9A94` + 0x90"算出来的静态地址，实际字段在堆对象里，`0x4E9B24` 全程序 0 引用），详见 §3 th15。
> th15 replay 原记录 `0x4E7ECC != 0xd`（场景状态）同日改为 `[0x4E9BC4]+0xc != 1`（ReplayInf mode，与 th13/th14 同构），详见 §3 th15。

---

## 3. 已确认条目明细

### th06（A 代 · 参照作）

| 量 | VA | RVA | 判据 | 出处 |
| --- | --- | --- | --- | --- |
| gamemode | `0x6C6EA4` | `0x2C6EA4` | `== 2` | `thprac/src/thprac/thprac_th06.cpp:163` |
| pause | `0x69D4BF` | `0x29D4BF` | `== 0` | `thprac_th06.cpp:164` |
| replay | `0x69BCBC` | `0x29BCBC` | `== 0` | `thprac_th06.cpp:165` |

**推导已复核**：`GAME_MANAGER = 0x69BCA0`（`thprac_th06.cpp:29`），`GameManager` 定义在 `thprac/src/thprac/thprac_th06.h:66-121`：

- `difficulty @ +0x10` = `0x69BCB0` ✓（与 `thprac_th06.cpp:171/534/1368` 吻合）
- `character @ +0x181D` = `0x69D4BD` ✓（与 `:172` 吻合）、`shotType @ +0x181E` = `0x69D4BE` ✓
- ⇒ `isInGameMenu @ +0x181F` = `0x69D4BF`（旁证：`:3240` 的 "Close pause menu" 直接把 `0x69d4bf` 置 0）
- `isInReplay @ +0x1C` = `0x69BCBC` ✓（`:3234` 以 `== 0` 为判据）

### th07（A 代 · 打样）—— **完成**

| 量 | VA | RVA | 判据 | 出处 |
| --- | --- | --- | --- | --- |
| gamemode | `0x575AA4` | `0x175AA4` | `== 2` | `Supervisor+0x154` |
| pause | `0x62F64C` | `0x22F64C` | `== 0` | 反编译结论（见下） |
| replay | `0x62F648` | `0x22F648` | `& 8 == 0`（bit 3） | 反编译结论（见下） |

**gamemode 推导**：`Supervisor` 基址 `0x575950`（`thprac_th07.cpp:15`），`gamemode` 字段在 `+0x154` ⇒ `0x575950 + 0x154 = 0x575AA4`。`thprac_th07.cpp:710` 用 `SUPERVISOR->gamemode == 2`；`thprac/src/thprac/thprac_th07.h:212/243` 有对应 `static_assert`。

**pause 推导**：`0x62F64C` = `GAME_MANAGER(0x626270) + 0x93DC` = thprac 的 `GameManager.__byte_93DC`。取值 **0 = 暂停菜单未开，1 = 刚打开（瞬态），2 = 打开中（稳态）** ⇒ 判据必须写 `== 0`（写 `== 1` 会漏）。置位在关卡每帧更新 `FUN_0042d8d5`（`0x42D958`，条件是 Esc/暂停键 `input & 8` 新按下），清零在暂停菜单状态机 `FUN_00402780`（case 4 = `"UnPause"`，即 thprac `th07_unpause_prevent_desync` @`0x403481` 所在分支）。

**replay 推导**：`0x62F648` = `GAME_MANAGER + 0x93D8` = thprac 的 `GameManager.flags_93D8`，**bit 3**。置位在 replay 菜单 `FUN_0045a924`（`0x45B395`）与标题挂机 attract demo `FUN_004555dd`（`0x455B50`）；清零在回标题 `FUN_004555dd`（`0x4558BE`）与开新游戏 `FUN_00459518`（`0x459EF2`）。thprac 自己在 `th07_save_data1/2` 里就写着 `(0x62F648 & 8) == 0 // not in rep`。

**同 dword 的其它位**：bit 0 = 练习模式本局，bit 1 = attract demo，bit 2 = `关卡中 ∧ 未暂停 ∧ 不在续关菜单`（游戏每帧重算；thprac 的 `RenderBtHitbox` 已在用 `game_state & 4`），bit 4 = ED/Staff Roll。

**⚠️ 可选的门控强化**：`0x62F64C == 0` **不覆盖"死亡后的续关(Continue)菜单"**（`0x62F64D == 1`；`FUN_00403b60`），那一状态下 `gamemode` 仍为 2、三个判据全成立，会误计。若要一并排除，把 pause 一项换成 **`{ 0x22F648, 4, 0x4, BitClear }`**（bit 2 天然同时排除暂停与续关菜单），无需改 `THGameTimeGate` 结构。

**已排除**：`0x575a87` 是 **BGM 类型 == 2**（`thprac_th07.cpp:2041`），不是暂停标志。

**三态实测与完整证据链**：见 `docs/th07/th07_re_disasm_report.md`（含 11 组状态实测值 + 截图存证 + 复现脚本）。

### th08（A 代 · 打样）—— **完成**

| 量 | VA | RVA | 判据 | 出处 |
| --- | --- | --- | --- | --- |
| gamemode | `0x17CE8B0` | `0x13CE8B0` | `== 2` | `Supervisor(0x17CE758)+0x158` |
| pause | `0x164D0BA` | `0x124D0BA` | `== 0` | 反编译结论（见下） |
| replay | `0x164D0B4` | `0x124D0B4` | `& 8 == 0`（bit 3） | 反编译结论（见下） |

**gamemode 出处**：`GAMEMODE_ADDR = 0x17CE8B0`（`thprac_th08.cpp:27`），用于 `:1066`、`:2842`。相邻 `GAMEMODE_NEXT_ADDR = 0x17CE8B4`、`GAMEMODE_PREV_ADDR = 0x17CE8B8` ⇒ `Supervisor` 静态基址 **`0x17CE758`**。

**pause 推导**：`0x164D0BA` = `GameManager(0x160F508) + 0x3DBB2`。取值 **0 = 未开 / 1 = 刚开（瞬态）/ 2 = 打开中（稳态）** ⇒ 判据写 `== 0`。置位在关卡内每帧更新 `FUN_00439bc7`（`0x43A119`，条件是 Esc/暂停键 `input & 8` 新按下 + 若干标志位为 0），清零在暂停菜单机 `FUN_004037b0`（case 4 = 恢复，即 thprac `th08_unpause_prevent_desync` @`0x40421A` 所在分支）。

**replay 推导**：`0x164D0B4` = `GameManager + 0x3DBAC`，**bit 3**。置位在 replay 菜单 `FUN_0046e136`（`0x46ED45`，随后 `gamemode_next = 2`）；清零在开新游戏 `FUN_0046bbc0`（`0x46CAAB`）与回标题 `FUN_00470a6c`（`0x470B39`）。

**⚠️ 注意**：本作 `0x164D0B4` 在**标题画面的基线值是 `0x00038000` 而不是 0**（bit 15/16/17 常驻）⇒ 这个 dword **只能做位测试**，不能拿整值跟常数比。

**同 dword 的其它位**：bit 0 = 练习/符卡练习，bit 1 = attract demo，bit 2 = `关卡中 ∧ 未暂停 ∧ 不在另一个菜单`（游戏每帧重算），bit 4 = ED/Staff Roll，bit 14 = replay 头的一个符号位。

**⚠️ 可选的门控强化**：`0x164D0BA == 0` **不覆盖 `0x164D0BB`（`GM+0x3DBB3`）那个"另一个全屏菜单"**——它出现时三个判据全成立，会误计。若要一并排除，把 pause 一项换成 **`{ 0x124D0B4, 4, 0x4, BitClear }`**（bit 2 天然同时排除两个菜单），无需改 `THGameTimeGate` 结构。

**已排除**：`0x17CE88F` 是 **BGM 类型 == 2**（`thprac_th08.cpp:3004`），不是暂停标志；变量名 `th08_pause_test`（`:2974`）是栈参数、用于区分 BGM 命令调用点（`:2991`），**不是地址也不是暂停标志**。**另需纠正**：需求文档猜"暂停标志很可能在 `0x17CE8xx` 一带"是**死路**——暂停/回放都在另一个结构 `GameManager(0x160F508)` 里。

**三态实测与完整证据链**：见 `docs/th08/th08_re_disasm_report.md`（含 9 组状态实测值 + 截图存证 + TH07↔TH08 同构对照表）。

### th10（B 代 · 风神录）—— 静态定位完成，待真机三态

| 量 | 对象 / 指针链（VA） | 判据 | 出处（反汇编，基址 `0x400000`） |
| --- | --- | --- | --- |
| gamemode | `*(u32*)0x477834`（PLAYER_PTR） | `!= 0` | 现成用法 `thprac/src/thprac/thprac_th10.cpp:641` |
| pause | `[0x477810]+0x58`（GameManager，bit4） | `& 0x10 == 0` | `0x422AB0`（PauseInf 初始化：`or [eax+0x58],0x10` + `"Pause"`）/ `0x422C30`（关闭：`and [eax+0x58],0xffffffef` + `"UnPause"`） |
| replay | `[0x477838]+0x10`（ReplayManager） | `!= 1` | `0x428F60` 存 mode（0=录制 `"t10r"` / 1=回放）；游戏 All-Clear 判据 `0x416D6C: cmp [edx+0x10],1` |

**宿主对象都是堆对象**：
- GameManager 指针 `0x477810`：`0x418136: mov [0x477810],esi`（`esi` 为构造出的对象）、`0x41807A: mov [0x477810],0`。
- ReplayManager 指针 `0x477838`：`0x428F77` / `0x429251: mov [0x477838],ebx`、`0x4295D3: mov [0x477838],0`。
- 调试串（ZUN trace，可作定位锚点）：`.\src\game\pause.cpp:192 PauseInf`（`0x46E1CC`）、`"Pause"`（`0x46E0C0`）、`"UnPause"`（`0x46E0B8`）。

**模式标志静态备选**：`*(u32*)0x474CA0` 的 **bit3(0x8) = replay**（置位处 `0x417D9C: or [0x474ca0],8`，位于 `GS_MODE(0x491FB8) == 0xd` 分支；与 th07/th08 bit3=replay 同构）。该 dword 是静态的，可作为"不想走指针链"时的 replay 备选，但**语义尚未三态验证**。

**⚠️ 挂机演示**：`0x474CA0` 的 bit1(0x2) 用于"跳过 replay 录制"（`0x417A74: test [0x474ca0],2`），疑似 attract demo。用 `PLAYER_PTR != 0` 当 gamemode 时，标题挂机演示是否会被误计，需在三态阶段确认。

### th11（B 代 · 地灵殿）—— 静态定位完成，待真机三态

| 量 | 对象 / 指针链（VA） | 判据 | 出处（反汇编，基址 `0x400000`） |
| --- | --- | --- | --- |
| gamemode | `*(u32*)0x4A8EB4`（PLAYER_PTR） | `!= 0` | 现成用法 `thprac/src/thprac/thprac_th11.cpp:699` |
| pause | `[0x4A8E88]+0x60`（GameThread，bit4） | `& 0x10 == 0` | `0x42C620` / `0x42C760`（`or [eax+0x60],0x10` + `"Pause"`）/ `0x42C880`（`and [eax+0x60],0xffffffef` + `"UnPause"`） |
| replay | `[0x4A8EB8]+0x10`（ReplayManager） | `!= 1` | `0x4356E0` 存 mode（0=录制 `"t11r"` / 1=回放）；游戏 All-Clear 判据 `0x41ECA6: cmp [ecx+0x10],1` |

**宿主对象都是堆对象**：GameThread 指针 `0x4A8E88`（`0x42031F: mov [0x4a8e88],edi`、`0x420263` 置 0）；ReplayManager 指针 `0x4A8EB8`（`0x4356FC` / `0x435A07` 赋值、`0x435DC0` 置 0）。

**⚠️ 更正原 `gamemode` 线索**：`GAME_THREAD_PTR(0x4a8e88) + 0x74` **不是** gamemode。关卡启动函数 `0x41F8F0`（开头 `mov ebp,[0x4a8e88]`）在 `0x41FC27: mov eax,[ebp+0x74]; push eax; call 0x435E00` 把 `+0x74` 当作 `ReplayManager::Start(mode)` 的 mode 传入（th10 的对应字段是 `GameThread+0x5c`，`0x417A7D`）。所以 `+0x74 != 0` 是"回放 / 非普通局"，不是"关卡进行中"。

**replay 注释**：thprac 现用的 `REPLAY_MGR_PTR → +0x18 → +0xa & 1`（`thprac_th11.cpp:721`）来自 All-Clear 分支 `0x41EBBC`，读的是回放**数据头**里的一个位；它与本表的 `[ReplayMgr]+0x10 == 1`（游戏自己判"是否回放"用的）**不是同一个量**，不要混用。优先用 `+0x10 == 1`。

**模式标志静态备选**：`*(u32*)0x4A5758` 的 **bit3(0x8) = replay**（置位处 `0x41FEFD: or [0x4a5758],8`，位于 `GS_MODE(0x4C37D8) == 0xe` 分支）；同为静态备选，语义未三态验证。

**调试串**：`.\src\game\pause.cpp:203 PauseInf`（`0x49527C`）、`"Pause"`（`0x494260`）、`"UnPause"`（`0x494268`）。

### th12（B 代 · 星莲船）—— 静态定位完成并接入，待真机三态

| 量 | 对象 / 指针链（VA） | RVA | 判据 | 出处（反汇编，基址 `0x400000`） |
| --- | --- | --- | --- | --- |
| gamemode | `*(u32*)0x4B4514`（PLAYER_PTR） | `0x0B4514` | `!= 0` | 现成用法 `thprac/src/thprac/thprac_th12.cpp:38`（`player` 宏） |
| pause | `[0x4B44E8]+0x60`（宿主对象，bit4） | 一级 `0x0B44E8`，二级 `+0x60` | `& 0x10 == 0` | `0x432850`（`mov eax,[0x4b44e8]` → `or [eax+0x60],0x10` + `"Pause"`）/ `0x432960`（`and [eax+0x60],0xffffffef` + `"UnPause"`） |
| replay | `[0x4B4518]+0x10`（ReplayManager） | 一级 `0x0B4518`，二级 `+0x10` | `!= 1` | `0x43AE80` 存 mode（`mov [ebp+0x10],eax`、`"t12r"`）；thprac 已用 `[0x4b4518]+0x10 == 1` 判回放（`thprac_th12.cpp:781/2038`） |

**宿主对象是堆对象**（必须二级读）：

- Pause 置位函数 `0x432850`：`0x4328AE: mov eax,[0x4b44e8]` → `0x4328BC: or dword ptr [eax+0x60],0x10`，随后 `0x432919: mov edi,0x4a1044`（`"Pause"`）日志。
- UnPause 清位函数 `0x432960`：`0x432961: mov eax,[0x4b44e8]` → `0x432965: and dword ptr [eax+0x60],0xffffffef`，随后 `0x432973: mov edi,0x4a104c`（`"UnPause"`）日志。
- 指针 `0x4b44e8` 由构造器 `0x422700` 写入堆对象：`alloc 0x78` → `0x42274B: or [edi+0x60],4` → `0x42274F: mov [0x4b44e8],edi` → `0x422755: mov [edi+0x74],eax`（存 mode）；清零点 `0x42269B: mov [0x4b44e8],0`。
- 调试串锚点：`.\src\game\pause.cpp:203 PauseInf`、`initialize PauseInf`、`shutdown PauseInf`（`0x4a0ee0` 一带），与 th10 / th11 同源。

**⚠️ 更正原 `gamemode` 线索**：`0x4b44e8 + 0x74` **不是** gamemode。宿主对象构造器 `0x422755: mov [edi+0x74],eax` 存的是传给 `ReplayManager::Start` 的 mode（与 th11 的 `0x41FC27` 同型），故 `+0x74 != 0` 是"回放 / 非普通局"。gamemode 改用 `*(u32*)0x4b4514 != 0`（PLAYER_PTR 在 `0x435A8C` 赋值、`0x43634B` 清零）。

**replay 注释**：thprac 现用的 `REPLAY_MGR_PTR → +0x1c → +0xa & 1`（`thprac_th12.cpp:749`）来自 All-Clear 分支 `0x420ABA`，读的是回放**数据头**里的一个位；它与本表的 `[ReplayMgr]+0x10 == 1`（游戏自己判"是否回放"用的）**不是同一个量**，不要混用。优先用 `+0x10 == 1`。

**⚠️ 挂机演示**：与 th10 / th11 同代，`PLAYER_PTR != 0` 在标题挂机 attract demo 下是否被误计，需在三态阶段确认。

### th13（B 代 · 神灵庙）—— 静态定位完成并接入，待真机三态

| 量 | 对象 / 指针链（VA） | RVA | 判据 | 出处（反汇编，基址 `0x400000`） |
| --- | --- | --- | --- | --- |
| gamemode | `*(u32*)0x4c22c4`（PLAYER_PTR） | `0x0C22C4` | `!= 0` | 现成用法 `thprac_th13.cpp:663/667`；赋值 `0x441d82`、清零 `0x4427d0` |
| pause | `[0x4c2194]+0x60`（PauseInf，bit4） | 一级 `0x0C2194`，二级 `+0x60` | `& 0x10 == 0` | `0x43e46a` / `0x43e61d` / `0x440aba`（`mov eax,[0x4c2194]` → `or [eax+0x60],0x10` + `"Pause"`）/ `0x43e71b` / `0x440769`（`and [eax+0x60],0xffffffef` + `"UnPause"`） |
| replay | `[0x4c22c8]+0x10`（ReplayManager，mode） | 一级 `0x0C22C8`，二级 `+0x10` | `!= 1` | 构造器 `0x447900`（`0x447909: mov [ebx+0x10],eax`）；游戏自带访问器 `0x413c60: cmp [ecx+0x10],1; sete al`；`0x447c0c` mode==1 走回放读取分支 |

**与 th12 同构（B 代模板成立）**：宿主对象是**堆对象、挂在静态指针变量后**，必须二级读：

- PauseInf 指针 `0x4c2194` 由构造器 `0x42c490` 写入：`alloc 0x7c` → `0x42c4de: or [esi+0x60],4` → `0x42c4e7: mov [0x4c2194],esi` → `0x42c4ed: mov [esi+0x74],eax`（存 mode）；清零点 `0x42c40d: mov [0x4c2194],0`。
- 全程序对 `+0x60` 的 `or/and` 只有 5 处，且**全部**以 `[0x4c2194]` 为基址 ⇒ 该位语义单一（暂停/冻结）。全程序无 `lea reg,[0x4c2194]`，确认它是指针变量而非静态对象。
- ReplayManager 指针 `0x4c22c8`：写 `0x44791b`（mode 0）/ `0x447c15`（mode 1），清 `0x447ea1`。

**replay 注释**：thprac 现用的 `REPLAY_MGR_PTR → +0x1c → +0xc` 读的是回放**数据头**字段，与本表的 `[ReplayMgr]+0x10 == 1`（游戏自己判"是否回放"用的）**不是同一个量**，不要混用。

**⚠️ 挂机演示**：`PLAYER_PTR != 0` 在标题挂机 attract demo 下是否被误计，需在三态阶段确认。

### th14（东方辉针城）—— 静态定位完成并接入，待真机三态

| 量 | 对象 / 指针链（VA） | RVA | 判据 | 出处（反汇编，基址 `0x400000`） |
| --- | --- | --- | --- | --- |
| gamemode | `*(u32*)0x4db67c`（PLAYER_PTR） | `0x0DB67C` | `!= 0` | 现成定义 `thprac_th14.cpp:16`；赋值 `0x44c44a`（`alloc 0x184c0`）、清零 `0x44d17a` |
| pause | `[0x4db558]+0x80`（PauseInf，bit4） | 一级 `0x0DB558`，二级 `+0x80` | `& 0x10 == 0` | 置位 `0x448e24`/`0x448f8d`/`0x449e0a`/`0x449f8a`/`0x44b22a`（`mov reg,[0x4db558]` → `or [reg+0x80],0x10` + `"Pause"`）；清位 `0x449043`/`0x44aee9`（`and [eax+0x80],0xffffffef` + `"UnPause"`） |
| replay | `[0x4db688]+0x10`（ReplayManager，mode） | 一级 `0x0DB688`，二级 `+0x10` | `!= 1` | 游戏自带访问器 `0x41d640: mov ecx,[0x4db688]; xor eax,eax; cmp [ecx+0x10],1; sete al`；构造器 `0x454630` 存 mode（mode0→`0x45464c`、mode1→`0x4549b6` 各存指针），清零 `0x454e2b` |

**与 th13 同构（B 代模板成立）**：宿主对象是**堆对象、挂在静态指针变量后**，必须二级读。差异仅在 PauseInf 字段偏移：th13 是 `+0x60`，th14 是 `+0x80`；replay 仍为 `+0x10 == 1`，gamemode 仍为 `PLAYER_PTR != 0`：

- PauseInf 指针 `0x4db558` 由构造器 `0x436560` 写入：`alloc 0x9c` → `0x4365ac: or [esi+0x80],4` → `0x4365b9: mov [0x4db558],esi`；清零点 `0x4364ee: mov [0x4db558],0`。全程序与暂停语义相关（imm `0x10`/`0xffffffef`）的 7 处 `or/and [reg+0x80]` 基址全部来自 `[0x4db558]` ⇒ 语义单一。
- ReplayManager 指针 `0x4db688` 由构造器 `0x454630` 写入（`0x45463b: mov [ebx+0x10],eax` 存 mode）。
- trace 串锚点：`"Pause"`（`0x4bedbc`）/`"UnPause"`（`0x4bedb4`）/`"src\game\pause.cpp:216 PauseInf"`（`0x4bec38`）；replay 侧 `"initialize ReplayInf"`（`0x4bef5c`）/`"replay/%s"`（`0x4bf058`）/`"th14_%.2d.rpy"`（`0x4bed98`）。

**⚠️ 只做位测试**：`+0x80` 同偏移另有 bit2（构造器常量 `4`）与 bit11（`0x4367f1: or [edi+0x80],0x800`）等其它语义位，pause 判据必须写 `& 0x10`，不能拿整值比较。

**⚠️ 挂机演示**：`PLAYER_PTR != 0` 在标题挂机 attract demo 下是否被误计，需在三态阶段确认。

报告：`docs/th14/th14_re_disasm_report.md`。

### th15（C 代）—— **静态定位完成并接入，待真机三态（2026-09-30 复核）**

| 量 | 地址 / 指针链 | RVA | 判据 | 出处 |
| --- | --- | --- | --- | --- |
| gamemode | `*(u32*)0x4E9BB8`（PLAYER_PTR，静态指针变量） | `0x0E9BB8` | `!= 0` | `thprac_th15.cpp:14`；创建 `0x453365`/`0x45404A`、清零 `0x453F04` |
| pause | `[0x4E9A94]+0x90`（PauseInf，bit4） | 一级 `0x0E9A94`，二级 `+0x90` | `& 0x10 == 0` | 构造 `0x43CB80`；置位 `0x450E2A`/`0x451006`/`0x451192`/`0x4512E0`（+ `"Pause"`）、清位 `0x452942`（+ `"UnPause"`） |
| replay | `[0x4E9BC4]+0xc`（ReplayInf，mode） | 一级 `0x0E9BC4`，二级 `+0xc` | `!= 1` | 构造 `0x45B630`（mode 0 录制写 `"t15r"` / mode 1 回放读文件）；每关起始 `0x43C46B` 调 `ReplayMgr::Start(0x45BE60)` |

**⚠️ pause 更正（2026-09-30）**：原 gate 把 pause 写成静态 `0x0E9B24`（VA `0x4E9B24`），注释"StageController + 0x90"——这是把**指针变量**当成了**静态对象**：

- `0x4E9A94` 是**指向堆对象（PauseInf）的静态指针变量**：构造/赋值在 `0x43CBE3: mov [0x4E9A94],esi`（`esi` 为刚构造出的对象，紧邻 `0x43CBD6: or [esi+0x90],4`），清零点 `0x43CB08`。
- Pause：`0x451113: mov edi,[0x4E9A94]` → `0x451192: or dword ptr [edi+0x90],0x10`（+ `"Pause"`）；UnPause：`0x45293D: mov eax,[0x4E9A94]` → `0x452942: and dword ptr [eax+0x90],0xffffffef`（+ `"UnPause"`）。
- **`0x4E9B24` 在 th15.exe 里被引用 0 次**（无读无写）⇒ 原 gate 读的是无关 `.data`，pause 判据实际**恒通过**（暂停也会被计时）。必须改成二级指针。
- 全程序没有任何"取 `0x4E9A94` 作对象基址"的指令（无 `lea reg,[0x4E9A94]`），可确认它不是静态对象。

**⚠️ replay 更正（2026-09-30）**：replay 由场景状态 `0x4E7ECC != 0xd` 改为 **ReplayInf mode `[0x4E9BC4]+0xc != 1`**，与 th13 / th14 的 `[ReplayMgr]+0x10 != 1` 同构：

- `0x4E9BC4` 是 ReplayInf 指针变量：构造 `0x45B630` 存 `[obj+0xc]=mode`；mode 0（录制）在 `0x45B64C` 发布并写 `"t15r"`，mode 1（回放）在 `0x45B9B6` 发布 + `0x45CC80` 读回放文件。
- `ReplayMgr::Start(0x45BE60)` 在关卡起始 `0x43C46B` 调用一次，mode 取自 `[PauseInf+0xB4]`；回放期该 mode 为 1 时暂停键失效（`0x451119`/`0x4512B8`），与判据互证。
- 弃用候选：`0x4E7ECC` 场景状态（`0xd` 在 `0x4609A0`/`0x468489` 写入，与 th10 的 `GS_MODE==0xd` 同构）。弃用理由：`0x4E7ECC == 0xd` 无消费点、无法证明回放期稳定保持 `0xd`；留作真机对照采样。
- ⚠️ `0x4E9BC4` **无清零点**（回放结束只 `free`@`0x43C7CC`，指针不置 0），靠 `gamemode` 短路兜底；练习模式是否创建 ReplayInf 需实测。

更正后的门控式：

`PLAYER_PTR(0x4E9BB8) != 0 && (*(u32*)(*(u32*)0x4E9A94 + 0x90) & 0x10) == 0 && *(u32*)(*(u32*)0x4E9BC4 + 0xc) != 1`

> ⚠️ 该条目的三个量均为不同来源的判据（PLAYER_PTR 单级 / PauseInf 二级位 / ReplayInf 二级 mode）。对应 `THGameTimeGate`：`gamemode` = `Ne 0`，`pause` = 链上 `BitClear 0x10`，`replay` = 链上 `Ne 1`（二级偏移 `+0xc`）。

报告：`docs/th15/th15_re_disasm_report.md`。

---

## 4. 待补清单与定位切入点索引

下表汇总**每作已经掌握的指针 / hook 锚点**，是后续分批推进时的起点。**TH10 / TH11 的判据已定案**（见 §3），只差真机三态验证；TH12 / TH13 与 TH10 / TH11 同代（B 代），按 §1 的同构结论就近搬运；其余作品同理。

| 作品 | 已有指针 / 地址常量 | 可用的 hook 锚点 |
| --- | --- | --- |
| th09 | `P1_CPU_PTR = 0x4a7db8`、`P2_CPU_PTR = 0x4a7df0`（`thprac_th09.cpp:9-10`） | `th09_unpause` `0x434ad8`（`thprac_th09.cpp:342`） |
| th095 | — | — |
| th10 | `CHARA_ADDR = 0x474c68`、`DIFF_ADDR = 0x474c74`、`ENEMY_MANAGER_PTR = 0x477704`、`PLAYER_PTR = 0x477834`（`thprac_th10.cpp:16-21`）；GameManager 指针 `0x477810`、ReplayMgr 指针 `0x477838` | 判据已定案（见 §3） |
| th11 | `STAGE_PTR = 0x4a8d60`、`ENEMY_MGR_PTR = 0x4a8d7c`、`GAME_THREAD_PTR = 0x4a8e88`、`REPLAY_MGR_PTR = 0x4a8eb8`（`thprac_th11.cpp:51-54`） | 判据已定案（见 §3） |
| th12 | `REPLAY_MGR_PTR = 0x4b4518`（`thprac_th12.cpp:44`）、宿主对象指针 `0x4b44e8`、`PLAYER_PTR = 0x4b4514`（`player` 宏 `:38`） | 判据已定案（见 §3），已接入 |
| th13 | `PLAYER_PTR = 0x4c22c4`、`REPLAY_MGR_PTR = 0x4c22c8`（`thprac_th13.cpp:14-16`）、PauseInf 指针 `0x4c2194` | 判据已定案（见 §3） |
| th125 | — | — |
| th128 | `PLAYER_PTR = 0x4B8A80`（`thprac_th128.cpp:8`） | — |
| th14 | `PLAYER_PTR = 0x4db67c`、`BOMB_PTR = 0x4DB52C`、`ENEMY_MANAGER_PTR = 0x4db544`、`CHARA_ADDR = 0x4f5828`（`thprac_th14.cpp:12-16`）；PauseInf 指针 `0x4db558`、ReplayMgr 指针 `0x4db688` | 判据已定案（见 §3） |
| th143 | — | — |
| th15 | `PLAYER_PTR = 0x4e9bb8`、`BOMB_PTR = 0x4e9a68`（`thprac_th15.cpp:13-14`）；PauseInf 指针 `0x4e9a94`、ReplayInf 指针 `0x4e9bc4` | 判据已定案（见 §3） |
| th16 | `ENEMY_MANAGER_PTR = 0x4a6dc0`、`PLAYER_PTR = 0x4a6ef8`（`thprac_th16.cpp:12-13`） | — |
| th165 | — | — |
| th17 | `PLAYER_PTR = 0x4b77d0`、`ENEMY_MANAGER_PTR = 0x4b76a0`、`GOAST_MANAGER_PTR = 0x4b7684`、`SPELLCARD_PTR = 0x4b7690`、`WINDOW_PTR = 0x5226c0`（`thprac_th17.cpp:78-82`） | — |
| th18 | `PAUSE_MENU_PTR = 0x4cf40c`、`REPLAY_MANAGER_PTR = 0x4cf418`、`ABILITY_MANAGER_PTR = 0x4cf298`（`thprac_th18.cpp:39-54`） | `th18_pause_skip_1/2` `0x458692` / `0x4588e3`（`thprac_th18.cpp:938-939`） |
| th185 | `GAME_THREAD_PTR = 0x4d7b0c`、`BLACK_MARKET_PTR = 0x4d7ac4`（`thprac_th185.cpp:22-25`） | `th185_change_gamemode` `0x45b77a`（`thprac_th185.cpp:1468`）、`th185_replaymanager_on_tick_cutoff` `0x468764`（`:1007`） |
| th19 | `GUI_PTR = 0x1AE460`(v1.00a) / `0x1D1A50`(v1.10c)、`P1_PTR`、`P2_PTR`、`ASCII_MANAGER_PTR`（`thprac_th19.v1.00a.cpp:19-25`、`thprac_th19.v1.00c.cpp:59-65`） | — |
| th20 | `REPLAY_MGR_PTR = 0x1c60fc`、`WINDOW_PTR = 0x1b6758`、`GAME_SIDE0 + 0x4/0x8/0x28`（`thprac_th20.cpp:749-753`） | — |
| alcostg | — | — |

**高性价比优先项**（"已知指针、只缺 offset"）：

| 优先 | 作品 | 目标 | 做法 |
| --- | --- | --- | --- |
| 1 | th18 | `PAUSE_MENU_PTR = 0x4cf40c` → 暂停标志字段 | 从指针使用点展开结构体；同作已有 `th18_pause_skip_1/2` 钩子可交叉定位 |
| 2 | th18 | `REPLAY_MANAGER_PTR = 0x4cf418` → 回放标志字段 | 同上 |
| 3 | th20 | `REPLAY_MGR_PTR = 0x1c60fc` → 回放标志字段 | 需 base+RVA |
| 4 | ~~th13~~ | ~~`REPLAY_MGR_PTR = 0x4c22c8` → 回放标志字段~~ | **已完成（见 §3 th13）**：`[0x4c22c8]+0x10 != 1` |
| 5 | th10 / th11 / th12 / th13 | 已定案判据的真机三态验证 | 见 §3，直接上探针验证，无需新反编译 |
| 6 | ~~th13~~ | ~~按 B 代同构搬运（th10 / th11 模板）~~ | **已完成（见 §3 th13）**：pause = `[0x4c2194]+0x60` bit4 |

> **实现前置（已完成）**：th10 / th11 / th12 / th13 / th15 的 pause 都在**堆对象**里（§0 指针链），`THGameTimeFlag` 已加第二级偏移 `rva2` 并接入；th15 原静态 `0x0E9B24` 恒通过的坑已按 §3 th15 更正。

---

## 5. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-29 | 建档。th06 / th15 完整（✅）；th07 / th08 的 gamemode 确认、pause / replay 待 RE；其余列为待补。引擎分代表初版（待验证）。 |
| 2026-09-29 | **th07 完成**：pause = `0x62F64C == 0`、replay = `(0x62F648 & 8) == 0`，均已运行时三态验证（11 组实测状态）。附 `flags_93D8` 完整位表与"续关菜单是否纳入门控"的可选强化。报告：`docs/th07/th07_re_disasm_report.md`。 |
| 2026-09-29 | 补记 th07 反汇编期的可复用经验：TH07 的 DirectInput 键盘**只**接受 `KEYEVENTF_SCANCODE` 形式的 `SendInput`（普通虚拟键注入游戏读不到）；标题画面挂机 900 帧会自动播放 attract demo。th08 那轮直接复用。 |
| 2026-09-29 | **th08 完成**：pause = `0x164D0BA == 0`、replay = `(0x164D0B4 & 8) == 0`，均已运行时三态验证（9 组实测状态）。纠正了"暂停标志在 `0x17CE8xx` 一带"的错误假设（实际在 `GameManager 0x160F508` 里）。报告：`docs/th08/th08_re_disasm_report.md`。 |
| 2026-09-29 | **§1 分代假设拿到第一组实证**：th07 ↔ th08 的暂停/回放实现逐条同构（见 §1 对照表）⇒ A 代同构搬运成立，th09/th095 可按模板推进。 |
| 2026-09-29 | **th06 / th07 / th08 / th15 四作已接入代码**：四个 gate 写进 `thprac_launcher_games_def.h`（`gGateTh06/07/08/15`），launcher 侧按 RVA + `GateCmp`（`Eq` / `Ne` / `BitClear`）读内存判定。方案见 `docs/gametime/gametime_plan.md`（极简版，改动 2 个文件）。**真机三态验证待做**。 |
| 2026-09-30 | **th10 / th11 静态反编译完成**（capstone 只读分析，**未**做真机三态）：gamemode = `PLAYER_PTR != 0`；pause = `[0x477810]+0x58` / `[0x4a8e88]+0x60` 的 bit4；replay = `[0x477838]+0x10` / `[0x4a8eb8]+0x10` 的 `== 1` 取反。pause 由引擎自带 `pause.cpp` trace 字符串交叉定位，replay 用游戏 All-Clear 自身的 `cmp …+0x10,1` 判据。见 §3。 |
| 2026-09-30 | **确认 B/C 代"堆对象 + 静态指针"形态**，A 代单级静态读的结论不可平移（§1"第二组实证"）；因此 `THGameTimeFlag` 需加二级偏移 `rva2`（§0、§4）。 |
| 2026-09-30 | **更正 th15 pause**：原 `0x0E9B24` 是把指针变量 `0x4E9A94` 当静态对象算出来的地址（该地址全程序 0 引用），实际字段为 `[0x4E9A94]+0x90`，需二级读；原 gate 导致 pause 判据恒通过。见 §3 th15。 |
| 2026-09-30 | **更正 th11 的 gamemode 线索**：`0x4a8e88 + 0x74` 实为传给 `ReplayManager::Start` 的 mode，不是"关卡进行中"；th12 的类比条目需重查。见 §2 注与 §3 th11。 |
| 2026-09-30 | **th12 静态反编译完成并接入代码**（capstone 只读分析，**未**做真机三态）：gamemode = `PLAYER_PTR(0x4b4514) != 0`；pause = `[0x4b44e8]+0x60` 的 bit4（`0x432850` OR / `0x432960` AND + `"Pause"`/`"UnPause"`）；replay = `[0x4b4518]+0x10 != 1`（`0x43AE80` 存 mode + `"t12r"`）。新增 `gGateTh12` 并挂到 th12 条目。原 `0x4b44e8 + 0x74` gamemode 线索与 th11 同因，已推翻；`+0xa & 1` 是回放数据头位，不可与 `+0x10 == 1` 混用。见 §3 th12。 |
| 2026-09-30 | **th13 静态反编译完成并接入代码**（capstone 只读分析，**未**做真机三态）：gamemode = `PLAYER_PTR(0x4c22c4) != 0`；pause = `[PauseInf(0x4c2194)]+0x60` 的 bit4（`0x43e46a`/`0x43e61d`/`0x440aba` OR、`0x43e71b`/`0x440769` AND + `"Pause"`/`"UnPause"`）；replay = `[ReplayMgr(0x4c22c8)]+0x10 != 1`（游戏自带访问器 `0x413c60: cmp [ecx+0x10],1; sete al`）。新增 `gGateTh13` 并挂到 th13 条目。与 th12 同构。报告：`docs/th13/th13_re_disasm_report.md`。 |
| 2026-09-30 | **th14 静态反编译完成并接入代码**（capstone 只读分析，**未**做真机三态）：gamemode = `PLAYER_PTR(0x4db67c) != 0`；pause = `[PauseInf(0x4db558)]+0x80` 的 bit4（`0x448e24`/`0x448f8d`/`0x449e0a`/`0x449f8a`/`0x44b22a` OR、`0x449043`/`0x44aee9` AND + `"Pause"`/`"UnPause"`）；replay = `[ReplayMgr(0x4db688)]+0x10 != 1`（游戏自带访问器 `0x41d640: cmp [ecx+0x10],1; sete al`）。新增 `gGateTh14` 并挂到 th14 条目。与 th13 同构（仅 pause 偏移由 `+0x60` 变 `+0x80`）。报告：`docs/th14/th14_re_disasm_report.md`。 |
| 2026-09-30 | **th15 独立复核并更正接入代码**（capstone 只读分析，**未**做真机三态）：gamemode = `PLAYER_PTR(0x4e9bb8) != 0`（创建 `0x453365`/`0x45404a`、清零 `0x453f04`）；pause = `[PauseInf(0x4e9a94)]+0x90` 的 bit4（更正后的二级读；`0x4E9B24` 全程序 0 引用，原判据恒通过）；replay = `[ReplayInf(0x4e9bc4)]+0xc != 1`（**由原场景状态 `0x4E7ECC != 0xd` 改判**，与 th13/th14 的 ReplayMgr mode 同构）。更正 `gGateTh15`。报告：`docs/th15/th15_re_disasm_report.md`。 |

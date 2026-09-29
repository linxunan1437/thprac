# 游戏时长统计 — 逐作标志位总表

> 本文档是"全作游戏时长统计"功能的**活文档**：跨作汇总每作的三个门控标志位（地址 + 判据 + 验证状态），随 P2 分批推进不断补齐。
> 总体方案见 `docs/gametime/gametime_plan.md`。逐作的反编译需求见 `docs/<作>/<作>_re_disasm_requests.md`，结论回填到 `docs/<作>/<作>_re_disasm_report.md`。

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

### 状态图例
| 标记 | 含义 |
| --- | --- |
| ✅ | 已确认，且通过三态验证 |
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

| 代 | 作品 | 参照作（三量齐全） | 验证状态 |
| --- | --- | --- | --- |
| **A** | th06, th07, th08, th09, th095 | **th06** | **部分验证**（th07↔th08 同构已证；th06↔th07 形态不同，th06 用的是分散的布尔量）|
| **B** | th10, th11, th12, th13, th125, th128 | 无（th11/th12 部分已知） | 待验证 |
| **C** | th14, th15, th16, th17, th18, th143, th165, th185 | **th15** | 待验证 |
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
| th10 | B | ❌ | ❌ | ❌ | 未开始 |
| th11 | B | ⚠️ `0x4a8e88 + 0x74` | ❌ | ⚠️ `0x4a8eb8 → +0xa & 1` | 待验证 |
| th12 | B | ⚠️ `0x4b44e8 + 0x74` | ❌ | ⚠️ `0x4b4518 → +0xa & 1` | 待验证 |
| th13 | B | ❌ | ❌ | ⚠️ `0x4c22c8` 已知、offset 待定 | 未开始 |
| th125 | B | ❌ | ❌ | ❌ | 未开始 |
| th128 | B | ❌ | ❌ | ❌ | 未开始 |
| th14 | C | ❌ | ❌ | ❌ | 未开始（有既有反编译资料） |
| th143 | C | ❌ | ❌ | ❌ | 未开始 |
| th15 | C | ✅ `0x4E9BB8 != 0` | ✅ `(0x4E9B24 & 0x10) == 0` | ✅ `0x4E7ECC != 0xd` | **已接入** |
| th16 | C | ❌ | ❌ | ❌ | 未开始 |
| th165 | C | ❌ | ❌ | ❌ | 未开始 |
| th17 | C | ❌ | ❌ | ❌ | 未开始 |
| th18 | C | ❌ | ⚠️ `0x4cf40c` 已知、offset 待定 | ⚠️ `0x4cf418` 已知、offset 待定 | 未开始 |
| th185 | C | ❌ | ❌ | ❌ | 未开始 |
| th19 | D | ❌ | ❌ | ❌ | 未开始（需 base+RVA；两个版本条目） |
| th20 | D | ❌ | ❌ | ⚠️ `0x1c60fc` 已知、offset 待定 | 未开始（需 base+RVA） |
| alcostg | — | ❌ | ❌ | ❌ | 未开始 |

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

### th15（C 代 · 参照作）

| 量 | VA | RVA | 判据 | 出处 |
| --- | --- | --- | --- | --- |
| gamemode | `0x4E9BB8` | `0x0E9BB8` | `!= 0`（PLAYER_PTR） | `thprac_th15.cpp:14` |
| pause | `0x4E9B24` | `0x0E9B24` | `& 0x10 == 0` | 反编译结论（见下） |
| replay | `0x4E7ECC` | `0x0E7ECC` | `!= 0xd` | 反编译结论（见下） |

**来源**：`stash@{0}`（`lxn/main` 上）那版 WIP 的反编译结论，已被实践验证。门控式：
`PLAYER_PTR != 0 && *(uint32_t*)0x4e7ecc != 0xd && (*(uint32_t*)0x4e9b24 & 0x10) == 0`

语义：`0x4E7ECC` 是场景管理器的场状态，`0xd` = 回放/演示播放中（回放期间 `PLAYER_PTR` 非 0，**必须**靠它排除）；`0x4E9B24`（StageController `+0x90`）的 bit `0x10` = 游戏冻结（Esc 暂停 / GameOver / Continue）。

> ⚠️ 该条目的 `pause` 与 `replay` 的语义映射与 th06 不同（th15 的三个量并非"三个独立布尔"，而是三种不同来源的判据）。写进 `THGameTimeGate` 时按 §0 的格式映射即可（`gamemode` = `Ne 0`，`pause` = `BitClear 0x10`，`replay` = `Ne 0xd`）。

---

## 4. 待补清单与定位切入点索引

下表汇总**每作已经掌握的指针 / hook 锚点**，是后续分批推进时的起点。TH11/TH12 的判据已基本成立（只需三态验证）；其余作品按 §1 的分代就近搬运。

| 作品 | 已有指针 / 地址常量 | 可用的 hook 锚点 |
| --- | --- | --- |
| th09 | `P1_CPU_PTR = 0x4a7db8`、`P2_CPU_PTR = 0x4a7df0`（`thprac_th09.cpp:9-10`） | `th09_unpause` `0x434ad8`（`thprac_th09.cpp:342`） |
| th095 | — | — |
| th10 | `CHARA_ADDR = 0x474c68`、`DIFF_ADDR = 0x474c74`、`ENEMY_MANAGER_PTR = 0x477704`、`PLAYER_PTR = 0x477834`（`thprac_th10.cpp:16-21`） | — |
| th11 | `STAGE_PTR = 0x4a8d60`、`ENEMY_MGR_PTR = 0x4a8d7c`、`GAME_THREAD_PTR = 0x4a8e88`、`REPLAY_MGR_PTR = 0x4a8eb8`（`thprac_th11.cpp:51-54`） | 判据已现成（`:721`） |
| th12 | `REPLAY_MGR_PTR = 0x4b4518`（`thprac_th12.cpp:44`）、`0x4b44e8` | 判据已现成（`:749`） |
| th13 | `PLAYER_PTR = 0x4c22c4`、`REPLAY_MGR_PTR = 0x4c22c8`（`thprac_th13.cpp:14-16`） | — |
| th125 | — | — |
| th128 | `PLAYER_PTR = 0x4B8A80`（`thprac_th128.cpp:8`） | — |
| th14 | `PLAYER_PTR = 0x4db67c`、`BOMB_PTR = 0x4DB52C`、`ENEMY_MANAGER_PTR = 0x4db544`、`CHARA_ADDR = 0x4f5828`（`thprac_th14.cpp:12-16`） | 另有既有资料 `D:\workshop\wind\th14decode` |
| th143 | — | — |
| th15 | `PLAYER_PTR = 0x4e9bb8`、`BOMB_PTR = 0x4e9a68`（`thprac_th15.cpp:13-14`） | 已完成 |
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
| 4 | th13 | `REPLAY_MGR_PTR = 0x4c22c8` → 回放标志字段 | 可参考 th11/th12 的 `→+0xa & 1` 形态 |
| 5 | th11 / th12 | 现有判据的三态验证 | 直接上探针验证，无需新反编译 |

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

# TH07 反编译报告 — 游戏时长统计门控标志位

> 对应需求：`docs/th07/th07_re_disasm_requests.md`
> 二进制：`D:\workshop\reasm\th07.exe`，650752 B，PE `TimeDateStamp = 1066942337`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。
> **二进制同一性已核对**：`sha256 = f7d03ba1bf6733a60beb3935e0efbbf1436c3d28b4f2155e7e410ba806ce0b9f`，与 `D:\C P\C2H2\[th07] 东方妖妖梦 (日文版)\th07.exe` **逐字节相同**（§7 的三态验证就是在该安装的沙箱副本上做的）。
> 作业场地：`D:\workshop\reasm\th07\`（Ghidra 工程 `ghidra\th07d`、脚本 `ghidra_scripts\`、原始 dump `out\`、运行时探针 `tools\`、运行沙箱 `run\`）。

---

## 0. 结论速览

**"正在有效游玩" = 下列三条合取**（三者互相独立，任一不成立即不计时）：

| 量 | 绝对 VA | RVA | 宽度 | 判据 | 含义 |
| --- | --- | --- | --- | --- | --- |
| `gamemode` | `0x00575AA4` | `0x175AA4` | 4 | `== 2` | 关卡进行中 |
| `pause` | `0x0062F64C` | `0x22F64C` | 1 | `== 0` | **游戏自身暂停菜单未打开** |
| `replay` | `0x0062F648` | `0x22F648` | 4 | `& 0x8 == 0` | **未在播放 replay**（bit 3） |

写进 `THGameTimeFlag`（见 `docs/gametime/gametime_thxx_flags.md` §0 格式）：

```cpp
{ 0x175AA4, 4, 0x2, GateCmp::Eq       }   // gamemode
{ 0x22F64C, 1, 0x0, GateCmp::Eq       }   // pause    —— 字面答案
{ 0x22F648, 4, 0x8, GateCmp::BitClear }   // replay
```

**三个地址都是 `.data` 里的静态全局量，无指针链、进程生命周期内恒可读**，读法就是 `ReadProcessMemory(base + RVA, …)`。

> ⚠️ **一条推荐改动**：`pause` 建议改用 **`{ 0x22F648, 4, 0x4, GateCmp::BitClear }`** 而不是 `0x22F64C == 0`。
> `0x62F648` 的 **bit 2** 是游戏**每帧重算**的"关卡进行中 ∧ 未暂停 ∧ 不在 Continue 菜单"，恰好就是这三者；而 `0x62F64C` 只覆盖**暂停菜单**，**不覆盖"死亡后的续关(Continue)菜单"**——那个状态下 `gamemode` 仍是 2、`0x62F64C` 仍是 0，会被误计。详见 §5.2。

---

## 1. 「未暂停」标志（需求 §1）

### 1.1 结论

**`0x62F64C`，1 字节，绝对地址固定，无指针链。**

- 它就是 `GAME_MANAGER`（`0x626270`）的 **`+0x93DC`** 字段，即 thprac `thprac_th07.h` 里 `GameManager.__byte_93DC`。
  `0x626270 + 0x93DC = 0x0062F64C` ✓（与 `static_assert(offsetof(GameManager, __byte_93DC) == 0x93dc)` 吻合）。

### 1.2 取值语义

| 取值 | 含义 |
| --- | --- |
| **0** | 暂停菜单**未打开**（正常游玩 / 标题 / 各菜单 / replay 播放中） |
| **1** | 暂停菜单**刚被打开**（瞬态，只存在极少数帧） |
| **2** | 暂停菜单**打开中**（稳态；也是"暂停中且本局已不能继续"的标记） |

⇒ **判据：`!= 0` 表示暂停菜单打开**；写进 `GateCmp` 就是 `{ 0x22F64C, 1, 0x0, Eq }`。

> ⚠️ 注意稳态值是 **2 不是 1**。只判 `== 1` 会漏。实测：按下 Esc 后 1.5 s 采样到的是 `2`（见 §3）。

### 1.3 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| **置 1（开暂停）** | `FUN_0042d8d5` @ `0x0042D958` | `MOV byte ptr [EAX+0x93DC], 1`，且紧接 `FUN_0044d2f0(6,0,"Pause")`（播放暂停音效） |
| 置 1 的触发条件 | `FUN_0042d8d5` @ `0x0042D927` 起 | `*(uint*)(GM+0x93D8) & 2 == 0`（非 demo）∧ `GM+0xD == 0` ∧ **`input & 0x8` 本帧新按下**（`0x8` 就是 Esc/暂停键，见 §7.3） |
| 这个函数是什么 | `FUN_0042f3c5` 注册的处理表 | `_DAT_0062f8b8 = FUN_0042d8d5`，即 **gamemode==2（关卡内）每帧更新**；同表 `FUN_0042e83e`=关卡初始化、`FUN_0042f2e4`=关卡退出 |
| 1 → 2 | `FUN_0042e1d4` @ `0x0042E1EC` | `if (GM+0x93DC != 0) GM+0x93DC = 2;` |
| **清 0（恢复）** | `FUN_00402780` @ `0x0040342C` | case 4「恢复游戏」分支，紧接 `FUN_0044d2f0(7,0,"**UnPause**")`（字符串 `0x00498A38`） |
| 清 0（回标题） | `FUN_00402780` @ `0x0040387A` | case 9（`gamemode_next = 1`） |
| 清 0（退出） | `FUN_00402780` @ `0x00403901` | case 10（`gamemode_next = 10`） |
| 清 0（关卡初始化） | `FUN_0042e83e` @ `0x0042EF46` | `AND byte ptr [EAX+0x93DC], 0` |

**`FUN_00402780` 就是暂停菜单的状态机**：入口判据来自 `FUN_004017e0`：

```c
if (DAT_0062f64c == 0 && DAT_0062f64d == 0) { /* 正常：更新特效 */ }
else if (DAT_0062f64c != 0) FUN_00402780();   // ← 暂停菜单
if (DAT_0062f64d != 0) FUN_00403b60();        // ← Continue 菜单
```

菜单项状态（`*param_1`）：`4`=恢复游戏(UnPause)、`5..8`=四个菜单项、`9`=回标题（`gamemode_next=1`）、`10`=退出（`gamemode_next=10`）。
**thprac 已有的 `th07_unpause_prevent_desync`（`0x403481`）正落在这个函数的 case 4「恢复游戏」里**——反汇编确认 `0x403481` 紧跟在 `0x40342C`（清 `0x62F64C`）之后，随后即 `CMP [0x62F85C],6 / PUSH 0x498A38("UnPause")`。这与需求文档 §1「建议切入点」的猜测完全对上。

### 1.4 逐条回答需求 §1 的问题

1. **存在哪个地址？** 全局字节 `0x0062F64C`（= `GAME_MANAGER + 0x93DC`）。**不是指针链**，直接读。
2. **取值语义？** 见 §1.2。`!= 0` = 暂停菜单打开；`0` = 未打开。
3. **非游玩状态下是什么值？** **0**。实测标题画面 = 0；标题/菜单/选人/结算下 `0x62F64C` 不参与，恒 0。（`0x62F64C != 0` 只在 gamemode==2 且按了 Esc 之后出现。）
4. **是否全生命周期可读？** **是**。静态 `.data` 字节，镜像不重定位，地址全程有效，不存在"某阶段才有效"的问题。

### 1.5 与 TH06 的对应

TH06 的 `isInGameMenu`（`0x69D4BF`）→ TH07 的对应物就是 `0x62F64C`。
（需求文档 §1.4 提到 `GameManager.__byte_93DC / __byte_93DD` 是"疑似候选项、未经证实、不要直接采用"——**现在证实了：`0x93DC` 就是暂停菜单，`0x93DD` 是续关菜单**。）

---

## 2. 「非 replay」标志（需求 §2）

### 2.1 结论

**`0x0062F648`（4 字节 = `GAME_MANAGER + 0x93D8`，即 thprac 已命名的 `flags_93D8`）的 bit 3（掩码 `0x8`）。**

### 2.2 取值语义

- **`& 0x8 != 0`** ⇒ 正在播放 replay（含标题画面挂机自动播放的 attract demo）
- **`& 0x8 == 0`** ⇒ 正常游玩 / 其它一切状态

⇒ 判据：`{ 0x22F648, 4, 0x8, GateCmp::BitClear }`。

> 旁证：**thprac 自己早就这么用了** —— `thprac_th07.cpp:2249 / :2266` 的 `th07_save_data1/2` 里写着 `if ((*(DWORD*)(0x62F648) & 8) == 0) // not in rep`。

### 2.3 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| **置位：菜单选 replay 开始播放** | `FUN_0045a924`（replay 菜单）@ `0x0045B38A`/`0x0045B395` | `flags = (flags & 0xFFFFFFF7) \| (sel & 1) << 3`，`sel = 1`；紧接着写入 replay 里的难度/机体（`0x626280`、`0x62F645..47`）、`gamemode_next = 2` |
| **置位：标题挂机 attract demo** | `FUN_004555dd`（标题画面机）@ `0x00455B50`/`0x00455B5E` | `flags = (flags & ~8) \| 1<<3;` 然后 `flags \|= 2;`（**同时置 bit 3 + bit 1**）。触发点是标题上 `0xd100` 帧计数 > 900（≈15 s 无输入）后载入 `data/demo/demorpy?.rpy` |
| **清零：从 replay 回到标题** | `FUN_004555dd` @ `0x004558BE` | `flags = (flags & ~8) \| (0 & 1) << 3;`（局部变量硬编码 0） |
| **清零：开新游戏** | `FUN_00459518` @ `0x00459EF2` | 同上形式，`sel = 0`（"Start" 且非 practice 分支） |
| **被当作"in replay"测试** | `FUN_0042e83e`（关卡初始化）@ `0x42F020/0x42F255…` | replay 时跳过 `globals->life_count` 初始化、樱桃/分数初始化、最高分写入；并把 `level_state+0x25` 清零 |
| | `FUN_0042d8d5` @ 早退分支 | `bit3 && 0x62F64F == 1` ⇒ 快进跳过（`return 3` 冻结逻辑） |
| | `FUN_0042f2e4`（关卡退出）@ `0x42F397` 附近 | `if ((flags >> 3 & 1) == 0) FUN_0043a3f4();` |
| | `FUN_00402780`（暂停菜单）case 0 / case 1 | 按 bit 3 走不同分支 |

### 2.4 逐条回答需求 §2 的问题

1. **存在哪个地址？** `0x0062F648` 的 bit 3（= `GAME_MANAGER + 0x93D8`）。全局 dword，无指针链。
2. **取值语义？** 见 §2.2。TH06 的 `isInReplay == 0` 对应这里 `(flags & 8) == 0`。
3. **何时置位 / 何时清除？**
   - 置位：① replay 菜单里按下确定、开始播放的那一刻（`FUN_0045a924`）；② 标题挂机 900 帧后 attract demo 自动开播（`FUN_004555dd`）。
   - 清除：① 从 replay 回到标题（`FUN_004555dd`）；② 开新游戏（`FUN_00459518`）。
   - 也就是说它覆盖的范围是"**本次 replay 播放会话**"，不是"某一帧"。
4. **replay 期间 `gamemode == 2` 是否也为真？** **为真，实测 = 2。** 两个判据**互相独立**，`gamemode` 单独**无法**排除回放。见 §3 表。
5. **任意时刻可读？** **是**，静态 dword，全程有效。

### 2.5 replay 管理器指针（需求文档顺带要的）

TH07 的 replay **不是**用一个全局 `REPLAY_MGR_PTR` 暴露的；replay 列表/状态挂在**标题画面对象**里（`FUN_0045a924` 的 `param_1`，数组偏移如 `param_1[0x2c2d]`=条目数、`param_1 + n*0x1e93`=条目、`param_1[0x2c2b]`=当前 replay 的解码结果）。目前**没有**一个像 th11/th12 `REPLAY_MGR_PTR` 那样的单一全局指针值得记录；本功能也不需要它。
相关全局量（供参考，非门控用）：

| 地址 | 含义 |
| --- | --- |
| `0x0062F64E` | attract demo 序号（`(x+1)%3`，对应 `data/demo/demorpy{0,1,2}.rpy`） |
| `0x0062F64F` | replay 快进/速读选择值（`1` 时配合 bit 3 触发跳帧分支） |
| `0x0062F650` | replay 播放计时/跳帧计数器（每次开播清零） |

---

## 3. 三态验证（需求 §3）

**方法**：把 `th07.exe` 连同数据在 `D:\workshop\reasm\th07\run\` 建沙箱（exe 拷贝、`th07.dat`/`thbgm.dat` 硬链接、`data`/`replay` 拷贝），用 `tools\probe07.py` **从外部进程用 `ReadProcessMemory`** 读上述地址——**与将来 launcher 的读取方式完全一致**；按键用 `SendInput` 注入；每个状态同时抓窗口截图存证（`out\ss_*.png`）。

### 3.1 实测值

| # | 状态 | `gamemode` `0x575AA4` | `pause` `0x62F64C` | `flags` `0x62F648` | 截图 |
| --- | --- | --- | --- | --- | --- |
| 1 | 标题画面 / 菜单 | `1` | `0` | `0x00` | `ss_1_title.png` |
| 2 | 难度选择画面 | `1` | `0` | `0x00` | `ss_2_diff.png` |
| 3 | 机体/符卡选择 | `1` | `0` | `0x00` | `ss_4_spell.png` |
| 4 | **关卡内正常游玩** | `2` | **`0`** | **`0x04`** (bit2) | `ss_5_stage.png` |
| 5 | **关卡内按 Esc（暂停菜单打开）** | `2` | **`2`** | **`0x00`** | `ss_7_paused.png` |
| 6 | 暂停后仍保持 | `2` | `2` | `0x00` | `ss_8_paused2.png` |
| 7 | 再按 Esc（恢复） | `2` | **`0`** | **`0x04`** | `ss_9_resumed.png` |
| 8 | **播放 replay（菜单选入）** | `2` | **`0`** | **`0x0C`** (bit2+**bit3**) | `ss_r2_play.png` |
| 9 | replay 中按 Esc | `2` | **`2`** | **`0x08`** (只剩 bit3) | `ss_r3_pause.png` |
| 10 | replay 恢复播放 | `2` | `0` | `0x0C` | `ss_r4_resume.png` |
| 11 | attract demo 回放（标题挂机） | `2` | `0` | **`0x0E`** (bit1+bit2+bit3) | — |

三态取值**互不相同且各自符合判据**：
- 「正常游玩」= `gm 2 / pause 0 / replay(bit3) 0` → 三条**全部成立** ⇒ 计时 ✓
- 「暂停」= `gm 2 / pause 2` → pause 不成立 ⇒ 不计时 ✓
- 「replay」= `gm 2 / bit3 1` → replay 不成立 ⇒ 不计时 ✓
- 「标题/选人」= `gm 1` → gamemode 不成立 ⇒ 不计时 ✓

### 3.2 三态之间的独立性

- **pause 与 replay 独立**：第 9 行——replay 播放中按 Esc，`bit3` 仍为 1、`pause` 变成 2。两者互不干扰。
- **replay 与 gamemode 不独立**：replay 播放时 `gamemode == 2`（第 8 行），**必须**靠 bit 3 排除。
- **暂停时 gamemode 仍为 2**（第 5 行），**必须**靠 pause 排除。

### 3.3 还没实测到的两处（如实标注）

1. **续关(Continue)菜单**（`0x62F64D == 1`）**未做运行时实测**（需要让角色死 3 次再等界面，未纳入本轮）。静态证据充分（§5.2），但请按"⚠️ 待验证"对待。
2. **replay 播完后 bit 3 的回落**未直接观测到（TH07 的 replay 不可中途回标题，只能等播完）。静态证据：`FUN_004555dd` @ `0x004558BE` 在回到标题时清 bit 3。旁证：每一次全新启动进程、停在标题时 `flags` 都读到 `0x00`（§3.1 第 1 行）。

---

## 4. 对需求文档假设的修正

| 需求文档的说法 | 修正 |
| --- | --- |
| §1.4 "`GameManager.__byte_93DC / __byte_93DD`（`0x62F64C` / `0x62F64D`）……**未经证实，不要直接采用**" | **证实**：`0x62F64C` = 暂停菜单（= TH06 `isInGameMenu`），`0x62F64D` = 续关菜单（= TH06 `isInRetryMenu`）。可以直接采用。 |
| §1.2 隐含"暂停 = 某个布尔" | 实际是 **三值**（0/1/2）。判据要写成 `!= 0`（`Eq 0` 取反），不能写 `== 1`。 |
| §1 "游戏是通过 `Supervisor` 指针 + 偏移访问的，所以搜绝对地址这条路对 TH07 无效" | 对 **gamemode** 成立，但对暂停/回放**不成立**：`0x62F64C` / `0x62F648` 都有直接的绝对地址引用（`FUN_0042d8d5`、`FUN_0045a924` 等），capstone 扫 `0x62F64D` / `0x62F648` 就能命中。真正的坑是 `+0x93DC` 这类**结构体偏移**访问（要单独扫立即数 `0x93DC`，见 §7）。 |
| §2 "TH07 的 replay 管理器在 thprac 里尚未定出指针" | 确实没有单一全局指针；replay 状态挂在标题画面对象上，且**门控根本不需要它**——用 `flags_93D8` bit 3 即可。 |
| §0.3 "若某地址只在特定阶段有效，请特别标注" | 三个地址都是静态全局量，**全程有效**，无阶段限制。 |

---

## 5. 附带发现

### 5.1 `flags_93D8`（`0x62F648`）完整位表

反编译 + 运行时双向确认：

| bit | 掩码 | 含义 | 置位处 | 清零处 |
| --- | --- | --- | --- | --- |
| 0 | `0x1` | **练习模式本局**（practice）。旁证：`FUN_0042e83e` 里 `if (flags & 1) level_state->lives = 8;`（练习给 8 残机）；`FUN_00403b60` 里 `if (flags & 1)` ⇒ 不给续关直接回标题 | `FUN_004555dd` 标题菜单选 "Practice Start" | `FUN_00459518` / `FUN_0045bf15` @`0x45C376` / `FUN_004555dd` @`0x455CEC` 等 |
| 1 | `0x2` | **attract demo 播放中**（标题挂机自动演示）。`FUN_0042d8d5` 里拿它禁止暂停、并做帧预算自动退出 | `FUN_004555dd` @`0x455B5E`（与 bit3 同时） | `FUN_0045bf15` @`0x45Cxxxx` |
| 2 | `0x4` | **关卡进行中 ∧ 未暂停 ∧ 不在续关菜单**（每帧重算）。**thprac 已在用**：`RenderBtHitbox` 的 `game_state & 0x4` | `FUN_0042e83e` @`0x42F255`（关卡初始化 `\|= 4`）；`FUN_0042d8d5` @`0x42DD8F/0x42DD9D` 每帧重算 | 同上重算；`FUN_0042f2e4` @`0x42F397`（关卡退出 `&= ~4`） |
| 3 | `0x8` | **replay 播放中**（本报告 §2） | `FUN_0045a924` @`0x45B395`；`FUN_004555dd` @`0x455B50` | `FUN_004555dd` @`0x4558BE`；`FUN_00459518` @`0x459EF2` |
| 4 | `0x10` | **ED / Staff Roll 播放中** | `FUN_0041e590`（入口 `flags \|= 0x10; … "data/staff01.anm"`）、`FUN_00429c42` | `FUN_0042e83e` @`0x42F294`（`&= ~0x10`） |

### 5.2 续关(Continue)菜单 —— 建议纳入门控

**`0x0062F64D`**（= `GAME_MANAGER + 0x93DD`，1 字节）：`1` = 续关菜单打开，`0` = 未打开。

- **置位**：`FUN_00440cf0` @ `0x00441161` `MOV byte ptr [0x62F64D], 1`（玩家死亡 / 残机耗尽时）。
  （thprac 的无限残机补丁 `PATCH_HK(0x44115F, "eb")` 就在它前一条指令上——跳过这句就永不弹续关菜单，逻辑自洽。）
- **清零**：`FUN_00403b60`（续关菜单状态机自身）多处；`FUN_0042e83e` @ `0x0042F24B`（关卡初始化）。
- **处理函数**：`FUN_00403b60`。入口判据：`if (globals->continues_used >= max_continues || difficulty > 3) { 关菜单; gamemode_next = 6; }`，即残机用尽直接回标题。

**为什么建议把它纳入**：死亡后弹出"是否续关"提示时，`gamemode` 仍是 `2`、`0x62F64C` 仍是 `0`、bit 3 也是 `0` —— 三个判据**全部成立**，玩家盯着提示发呆的时间会被算成游玩时长。

**推荐做法（不需要改 `THGameTimeGate` 结构）**：把 `pause` 一项从 `0x22F64C == 0` 换成 **`0x62F648` 的 bit 2（`BitClear 0x4`）**。该位是游戏自己每帧算出来的 `!(暂停菜单 ∨ 续关菜单)`，语义恰好等于"这一帧游戏真的在跑"：

```cpp
{ 0x175AA4, 4, 0x2, GateCmp::Eq       }   // gamemode == 2
{ 0x22F648, 4, 0x4, GateCmp::BitClear }   // pause  ← 同时覆盖暂停菜单与续关菜单
{ 0x22F648, 4, 0x8, GateCmp::BitClear }   // replay
```

**代价**：pause 与 replay 变成同一个 dword 的两位（两次读同一地址，接口上仍是两个独立 `THGameTimeFlag`，无结构改动）；另外 bit 2 与 gamemode 有语义重叠（bit 2 只在 gamemode==2 的更新里重算），但这只是冗余、不会误判。

**若想严格只答"暂停菜单"**：保留 `{ 0x22F64C, 1, 0x0, Eq }`，接受续关菜单被计入。**这是我的推荐之外的一个可选项，请定夺。**

### 5.3 `Supervisor` / `GameManager` 偏移复核（全部对上）

- `0x575950` + `0x154` = `0x575AA4` = `gamemode`；`+0x158` = `0x575AA8` = `gamemode_next`；`+0x15C` = `0x575AAC` = `gamemode_prev`。
  场景调度器 `FUN_00437c70` 里直接读这三个字段并打印 `"scene %d -> %d"`，可交叉验证。
- `0x626270` + `0x8` = `0x626278` = `globals` 指针；`+0x10` = `0x626280` = `difficulty`；`+0x93D5..0x93D8` = 机体/机体类型/全类型/`flags_93D8`。全部与 `thprac_th07.h` 的 `static_assert` 一致。

### 5.4 输入位表（顺带修正/补充）

`0x4B9E4C` = 本帧输入、`0x4B9E54` = 上帧输入、`0x4B9E5C` = 长按重复标志（与 thprac `GameGuiInit(..., 0x4b9e4c, 0x4b9e54, 0x4b9e5c, -1)` 一致）。从各处使用点归纳：

| 位 | 含义 |
| --- | --- |
| `0x1` | Shoot（Z） |
| `0x2` | Bomb（X） |
| `0x4` | Slow（Shift） |
| **`0x8`** | **Pause（Esc）**——`FUN_0042d8d5` 用它开暂停菜单，`FUN_00402780` 用它从暂停菜单恢复 |
| `0x10` | 上 |
| `0x20` | 下 |
| `0x40` | 左 |
| `0x80` | 右 |
| `0x1000` | Enter |
| `0x1001` | 决定（Z 或 Enter） |
| `0x8\|0x2` = `0xA` | 取消（Esc 或 X） |

---

## 6. 复现方法

### 6.1 静态（Ghidra）

```bash
# 已导入的工程
D:\workshop\reasm\th07\ghidra\th07d           # 项目
D:\workshop\reasm\th07\ghidra_scripts\        # Th07Dump.java / Th07Scan.java

# 通用查询（打反编译/反汇编/xref/调用图到文本）
cd D:/workshop/reasm/th07 && ./run.sh Th07Dump.java <out.txt> \
    xref:0x62F64C func:0x42d8d5 range:0x4558a0:0x4558d0

# 扫"结构体偏移"型访问（绝对地址扫不到的那些，例如 +0x93DC）
./run.sh Th07Scan.java <out.txt> 0x93dc:4 0x93dd:4 0x93d8:4
```
`Th07Dump.java` 支持的 spec：`xref:` `xrefr:` `xrefname:` `func:` `raw:` `range:A:B` `callers:` `callees:` `name:`。
**坑**：Ghidra 的 `postScript` 参数会**按逗号切分**，所以 `range:` 的两端用 **`:`** 分隔而不是逗号。

### 6.2 运行时（探针）

```bash
cd D:/workshop/reasm/th07/tools
python inp_test.py      # 输入链路自检
python s_play.py        # 标题→游玩→Esc暂停→恢复
python s_replay.py      # 标题→Replay 菜单→播放
python s_replay2.py     # replay 中 Esc / 恢复
```
- `probe07.py`：进程启动 / 窗口查找 / `ReadProcessMemory` / 扫描码注入 / 状态采样。
- `grab.py`：窗口截图（纯 stdlib PNG 编码，无 Pillow 依赖），落 `out\ss_*.png`。

### 6.3 ⚠️ 踩到的坑（**th08 那轮会直接复用到**）

1. **DirectInput 看不见普通 `SendInput`。** TH07 用 `DirectInput8Create` 建键盘设备（`FUN_00430b50` 走 `IDirectInputDevice8::GetDeviceState`），**只带虚拟键的 `SendInput` 一律无效**——`GetAsyncKeyState` 已报告按键按下，但游戏输入字 `0x4B9E4C` 恒为 0。
   **必须**用 `KEYEVENTF_SCANCODE` + `wVk=0` + 扫描码（方向键再加 `KEYEVENTF_EXTENDEDKEY`）。实测对照：

   | 方法 | 游戏是否读到 |
   | --- | --- |
   | `SendInput` VK | ❌ |
   | **`SendInput` SCANCODE** | ✅（`0x4B9E4C = 0x1`） |
   | `keybd_event` VK | ❌ |
   | `PostMessage(WM_KEYDOWN)` | ❌ |

2. **标题画面挂机 900 帧（≈15 s）会自动播放 attract demo**，状态在探针脚下自己就变了（`flags` 从 `0x00` 跳到 `0x0E`）。探针要么保持 ≤10 s 间隔送键，要么显式识别 `bit1`。
3. 建立运行沙箱时**不要**在游戏原目录里跑（会写 `score.dat` / `th07.cfg` / `snapshot/`）。`th07.dat` 26 MB / `thbgm.dat` 444 MB 用**硬链接**（`os.link`），只有 `data/`(63 MB) 需要真拷贝。
4. Windows 上 `cmd //c mklink` 在 git-bash 里参数会被吞（`/H` 被当路径转换），改用 Python `os.link()`。
5. 本机 Python 3.14 + `capstone 5.0.7`；`pefile` 需 `pip install`。

---

## 7. 原始产物清单（`D:\workshop\reasm\th07\`）

| 路径 | 内容 |
| --- | --- |
| `ghidra\th07d` | Ghidra 工程（th07.exe 已分析入库） |
| `ghidra_scripts\Th07Dump.java` | 通用查询脚本（xref / 反编译 / 反汇编区间 / 调用图） |
| `ghidra_scripts\Th07Scan.java` | 立即数扫描（找 `+0x93DC` 这类结构体偏移访问） |
| `out\core1.txt` | **暂停菜单状态机 `FUN_00402780` 全量反编译**（含 `"UnPause"`） |
| `out\menu1.txt` | replay 菜单 `FUN_0045a924` + 续关菜单 `FUN_00403b60` 反编译 |
| `out\gm1.txt` / `gm2.txt` | 关卡每帧更新 `FUN_0042d8d5` / 关卡初始化 `FUN_0042e83e` / 退出 `FUN_0042f2e4` |
| `out\writers.txt` | `flags_93D8` 全部写入点所在函数的反编译 |
| `out\scan93.txt` | `+0x93D8 / +0x93DC / +0x93DD / +0x93DE` 偏移扫描结果 |
| `out\input*.txt` | 输入链路（`FUN_00437c70` 场景调度 / `FUN_00430b50` DInput 读取） |
| `out\clear.txt` | `flags_93D8` 各写入点的精确指令 |
| `out\recon1.txt` `graph.txt` `graph2.txt` | 交叉引用、调用图、处理表注册 |
| `out\ss_*.png` | 三态截图存证（标题 / 难度 / 机体 / 关卡内 / 暂停菜单 / replay 播放 …） |
| `tools\probe07.py` `grab.py` `inp_test.py` `s_*.py` | 运行时探针与驱动脚本 |
| `run\` | 运行沙箱（exe 拷贝 + 数据硬链接） |

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-29 | 建档。TH07 三条判据全部定出：`gamemode 0x575AA4==2`、`pause 0x62F64C==0`、`replay 0x62F648&8==0`。暂停/回放两态**运行时三态验证通过**（§3.1）。附带给出 `flags_93D8` 完整位表与续关菜单（`0x62F64D`）的门控建议。续关菜单与 replay 结束回落标注为"待实测"。 |

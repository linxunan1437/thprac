# TH08 反编译报告 — 游戏时长统计门控标志位

> 对应需求：`docs/th08/th08_re_disasm_requests.md`
> 二进制：`D:\workshop\reasm\th08.exe`，840704 B，PE `TimeDateStamp = 1095610188`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。
> **二进制同一性已核对**：`sha256 = 330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924`，与 `D:\C P\C2H2\[th08] 东方永夜抄 (日文版)\th08.exe` **逐字节相同**（§3 的三态验证在该安装的沙箱副本上完成）。
> 作业场地：`D:\workshop\reasm\th08\`（Ghidra 工程 `ghidra\th08d`、脚本 `ghidra_scripts\`、原始 dump `out\`、运行时探针 `tools\`、运行沙箱 `run\`）。

---

## 0. 结论速览

**"正在有效游玩" = 下列三条合取**（三者互相独立）：

| 量 | 绝对 VA | RVA | 宽度 | 判据 | 含义 |
| --- | --- | --- | --- | --- | --- |
| `gamemode` | `0x17CE8B0` | `0x13CE8B0` | 4 | `== 2` | 关卡进行中 |
| `pause` | `0x164D0BA` | `0x124D0BA` | 1 | `== 0` | **游戏自身暂停菜单未打开** |
| `replay` | `0x164D0B4` | `0x124D0B4` | 4 | `& 0x8 == 0` | **未在播放 replay**（bit 3） |

```cpp
{ 0x13CE8B0, 4, 0x2, GateCmp::Eq       }   // gamemode
{ 0x124D0BA, 1, 0x0, GateCmp::Eq       }   // pause    —— 字面答案
{ 0x124D0B4, 4, 0x8, GateCmp::BitClear }   // replay
```

⚠️ **`0x164D0B4` 的"标题画面基线值"不是 0 而是 `0x00038000`**（bit 15/16/17 常驻），所以这个 dword **只能用位测试**，不能拿整值跟某个常数比。

> **推荐强化**（同 TH07）：`pause` 改用 **`{ 0x124D0B4, 4, 0x4, GateCmp::BitClear }`**，即 flags 的 **bit 2**。该位是游戏每帧重算的"关卡进行中 ∧ 未暂停 ∧ 不在另一个菜单里"，能一并覆盖暂停菜单与另一个全屏菜单（`0x164D0BB`）。

---

## 1. 「未暂停」标志（需求 §1）

### 1.1 结论

**`0x164D0BA`，1 字节 = `GameManager(0x160F508) + 0x3DBB2`。无指针链，静态地址直接读。**

### 1.2 取值语义

| 取值 | 含义 |
| --- | --- |
| **0** | 暂停菜单**未打开** |
| **1** | 暂停菜单**刚被打开**（瞬态） |
| **2** | 暂停菜单**打开中**（稳态） |

⇒ 判据 `!= 0`（写进 `GateCmp` 即 `{ 0x124D0BA, 1, 0x0, Eq }`）。**不能写 `== 1`**——稳态实测是 2。

### 1.3 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| **置 1（开暂停）** | `FUN_00439bc7` @ `0x0043A119` | `MOV byte ptr [EAX+0x3DBB2], 1` |
| 置 1 的完整触发条件 | `FUN_00439bc7` @ `0x0043A0A2`–`0x0043A110` | `GM[+0x3DBB3]==0` ∧ `GM[+0x3DBB2]==0` ∧ `(flags>>1 & 1)==0` ∧ `(char)GM[+0x2D]==0` ∧ **`input & 0x8` 本帧新按下**（`0x8` = Esc/暂停键） |
| 这个函数是什么 | 关卡内每帧更新（TH07 `FUN_0042d8d5` 的对应物） | 处理输入、暂停、结算、樱桃/得点等 |
| 1 → 2 | `FUN_0043aa03` @ `0x0043AA1B` | `if (GM[+0x3DBB2] != 0) GM[+0x3DBB2] = 2;` |
| **清 0（恢复）** | `FUN_004037b0` @ `0x004041E4` | 暂停菜单状态机 case 4「恢复游戏」 |
| 清 0（回标题） | `FUN_004037b0` @ `0x00404595` | case 9（`gamemode_next = 1`） |
| 清 0（退出 / 重开） | `FUN_004037b0` @ `0x004045FB`、`0x00404666` | case 10（`gamemode_next = 0xA/0xB`） |
| 清 0（关卡初始化） | `FUN_0043abd7` @ `0x0043B1CF` | `AND byte ptr [EAX+0x3DBB2], 0` |

**`FUN_004037b0` 就是暂停菜单状态机**（`*param_1`：`4`=恢复、`5..8`=菜单项、`9`=回标题、`10`=退出）。
**thprac 已有的 `th08_unpause_prevent_desync`（`0x40421a`）正落在这个函数的 case 4「恢复游戏」分支里**：
`0x004041E4` 先写 `pause = 0`，随后是一段清理循环（`0x004041F4`–`0x00404218`），紧接着 `0x0040421A: MOV ECX,0x18B8A68; CALL 0x00406C50` —— **thprac 就是钩在这一串上**。与需求文档 §1「建议切入点」的猜测完全吻合。

### 1.4 逐条回答需求 §1 的问题

1. **存在哪个地址？** 全局字节 `0x164D0BA`。**不是指针链**。
2. **取值语义？** 见 §1.2。`!= 0` = 暂停菜单打开。
3. **是否有多个子状态？** 有，但都被同一个字节覆盖：`1`（刚打开）与 `2`（稳定暂停）都是"非正常游玩"。暂停菜单里选"重开/回标题"时，是**先清 0、再由 `gamemode_next` 切场景**（`FUN_004037b0` case 9/10/…），不存在"菜单还开着但字节是 0"的窗口。**另有第二个全屏菜单**由 `0x164D0BB` 表示（见 §5.2），它**不在**本判据覆盖范围内。
4. **非游玩状态下的值？** **0**。实测标题画面 = 0；选人/菜单/结算下 gamemode 不是 2，门控自然不成立。
5. **全生命周期可读？** **是**，静态字节。

---

## 2. 「非 replay」标志（需求 §2）

### 2.1 结论

**`0x164D0B4`（4 字节 = `GameManager + 0x3DBAC`）的 bit 3（掩码 `0x8`）。**

- `& 0x8 != 0` ⇒ 正在播放 replay（含标题挂机自动播放的 attract demo）
- `& 0x8 == 0` ⇒ 正常游玩 / 其它一切状态

### 2.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| **置位：replay 菜单开始播放** | `FUN_0046e136` @ `0x0046ED45` | `flags = (flags & 0xFFFFFFF7) \| 1<<3`；紧随其后把 replay 的难度写入 `0x160F538`、机体写入 `0x164D0B1`、速读值写入 `0x164D0BD`，并在 `0x0046EE2E` 写 `gamemode_next = 2` |
| **清零：开新游戏** | `FUN_0046bbc0` @ `0x0046CAAB` | `flags = (flags & 0xFFFFFFF7) \| (0\&1)<<3` |
| **清零：回到标题** | `FUN_00470a6c` @ `0x00470B39` | `if ((flags >> 1 & 1) != 0) flags &= 0xFFFFFFF7;`（bit 1 = demo 置位时清 bit 3） |
| **被当作"in replay"测试** | `FUN_00439bc7`（关卡内每帧）多处 | 例：`if (((flags >> 3 & 1) == 0) \|\| (0x164D0BD != 1) \|\| …)` —— replay 快进跳帧；另有大量"replay 时跳过存档/初始化"的分支 |
| 同 dword 的 bit 4 被置位 | `FUN_00439bc7` @ `0x439EE0`/`0x439F0F` | `flags \|= 0x10`（与 TH07 的 bit 4 = ED/Staff Roll 对应） |

### 2.3 逐条回答需求 §2 的问题

1. **存在哪个地址？** `0x164D0B4` 的 bit 3。全局 dword，无指针链。
2. **取值语义？** 见 §2.1。TH06 的 `isInReplay == 0` 对应这里 `(flags & 8) == 0`。
3. **何时置位 / 清除？** 置位：replay 菜单确认开始播放的那一刻（`0x46ED45`）；清除：开新游戏（`0x46CAAB`）或回到标题（`0x470B39`）。覆盖范围是"**本次 replay 播放会话**"。
4. **replay 期间 `gamemode == 2` 是否也为真？** **为真，实测 = 2**（§3）。两判据独立，`gamemode` 单独**无法**排除回放。
5. **任意时刻可读？** **是**。

### 2.4 replay 管理器指针（需求文档顺带要的）

TH08 与 TH07 一样，**没有一个像 th11/th12 `REPLAY_MGR_PTR` 那样的单一全局 replay 管理器指针**：replay 列表与播放状态都挂在**标题画面的 replay 菜单对象**上（`FUN_0046e136` 的 `param_1`，数组偏移形如 `param_1 + n*0x80`、`param_1[0x30a0]` 等）。本功能不需要它。

相关全局量（参考，非门控用）：

| 地址 | 含义 |
| --- | --- |
| `0x164D0BD` | replay 速读/跳帧选择值（`1` 时配合 bit 3 触发跳帧分支） |
| `0x164D0B8` | 16 位量，replay 开始时由 replay 头 `+0x7C` 写入；也参与菜单判断 |
| `0x164D2CC` | 当前场景/关卡标识（replay 开始时被写入，供 `FUN_00439bc7` 比较） |
| `0x160F538` | 难度（thprac `DIFF_ADDR`，已被本报告交叉验证） |
| `0x164D0B1` | 机体/自机类型（thprac `SHOTTYPE_ADDR`） |

---

## 3. 三态验证（需求 §3）

**方法**：`th08.exe` 连同数据在 `D:\workshop\reasm\th08\run\` 建沙箱（exe 拷贝、`th08.dat`/`thbgm.dat` 硬链接、`data`/`replay` 拷贝），用 `tools\probe08.py` **从外部进程 `ReadProcessMemory`** 读取——与将来 launcher 的读法一致；按键用 `SendInput`（**必须带 `KEYEVENTF_SCANCODE`**，见 §6.3）；每个状态抓窗口截图存证。

### 3.1 实测值

| # | 状态 | `gamemode` `0x17CE8B0` | `pause` `0x164D0BA` | `flags` `0x164D0B4` | 截图 |
| --- | --- | --- | --- | --- | --- |
| 1 | 标题画面 / 菜单 | `1` | `0` | `0x00038000` | `ss_1_title.png` |
| 2 | **关卡内正常游玩** | `2` | **`0`** | **`0x00038004`** (bit2) | `ss_2_stage.png` |
| 3 | **按 Esc（暂停菜单打开）** | `2` | **`2`** | **`0x00038000`** | `ss_3_paused.png` |
| 4 | 暂停保持 | `2` | `2` | `0x00038000` | `ss_4_paused2.png` |
| 5 | 再按 Esc（恢复） | `2` | **`0`** | **`0x00038004`** | `ss_5_resumed.png` |
| 6 | **播放 replay（菜单选入）** | `2` | **`0`** | **`0x0003800C`** (bit2+**bit3**) | `ss_r5_play2.png` |
| 7 | replay 中按 Esc | `2` | **`2`** | **`0x00038008`** (只剩 bit3) | `ss_r6_pause.png` |
| 8 | attract demo 回放（标题挂机 ~21 s） | `2` | `0` | **`0x0003800E`** (bit1+bit2+bit3) | `ss_f1_demo.png` |
| 9 | demo 中按键 → 回标题 | **`1`** | `0` | **`0x00038000`**（bit1/bit3 **已清**） | `ss_f2_after.png` |

三态取值互不相同且各自符合判据：
- 「正常游玩」= `gm 2 / pause 0 / bit3 0` → 三条**全部成立** ⇒ 计时 ✓
- 「暂停」= `gm 2 / pause 2` → 不计时 ✓
- 「replay」= `gm 2 / bit3 1` → 不计时 ✓
- 「标题/菜单」= `gm 1` → 不计时 ✓

### 3.2 独立性

- **pause 与 replay 独立**：第 7 行——replay 中按 Esc，`bit3` 仍为 1、`pause` 变成 2。
- **replay 与 gamemode 不独立**：replay 播放时 `gamemode == 2`（第 6 行），**必须**靠 bit 3 排除。
- **暂停时 gamemode 仍为 2**（第 3 行），**必须**靠 pause 排除。
- **TH08 与 TH07 的差异**：TH08 的 `flags` 基线非 0（`0x38000`），**只能做位测试**。

### 3.3 第 9 行是怎么拿到的（值得记一下）

TH08 的 attract demo 有"**任意输入即回标题**"的逻辑（`FUN_00439bc7` 里，bit 1 置位时 `if (input != 0 && input != prev_input) gamemode_next = 1;`）。所以只要等 demo 真正跑起来（bit2 也置位后）再按一次键，就能观察到 `bit3` 被清、`gamemode` 回到 1。

⚠️ 首次尝试在 **fade-in 期间**按键无效（那几帧 bit2 还是 0，demo 分支还没生效），要等 `bit2 == 1` 再按。

### 3.4 还没实测到的

- **replay 播完后 bit 3 的回落**：TH08 的 replay 同样不能中途回标题（暂停菜单里"回标题"在 replay 时不可用），所以没直接观测到。静态证据见 §2.2 的两处清零；运行时旁证是第 9 行（demo 会话结束时 bit3 确实被清）。
- **`0x164D0BB` 那个"另一个菜单"**（§5.2）：未做运行时实测，静态证据充分。
- **`0x164D0B4` 的"标题画面基线值"不是 0 而是 `0x00038000`**（bit 15/16/17 常驻），所以这个 dword **只能用位测试**，不能拿整值跟某个常数比。

---

## 4. 对需求文档假设的修正

| 需求文档的说法 | 修正 |
| --- | --- |
| §1「暂停状态**很可能也在附近或同属一个更大的状态结构**——建议优先扫 `0x17CE8xx` 一带」 | ❌ **这条路是死路**。`gamemode` 在 `Supervisor`（`0x17CE758`）里，而暂停/replay 在**另一个结构** `GameManager`（`0x160F508`）里，两者相距约 0x1B8 万字节。真正邻近的锚点是 **thprac 自己的 `DIFF_ADDR = 0x160F538`（= GameManager + 0x30）和 `SHOTTYPE_ADDR = 0x164D0B1`（= GameManager + 0x3DBA9）**——暂停/回放就在同一结构里。 |
| §1「直接搜绝对地址 `0x17CE8B0` 这条路可能无效，请从指针/结构体入手」 | gamemode 本体的直接引用确实极少（只找到一条经 `[EAX+0x158]` 的），**但 `0x17CE8B4`（gamemode_next）有 40+ 条直接绝对引用**，是最好的定位锚点。而暂停/回放两个量在 `GameManager` 里，要扫**结构体偏移 `+0x3DBAC` / `+0x3DBB2`**（绝对地址扫不到 `MOV [EAX+0x3DBB2],1` 这种写法）。 |
| §1「`0x17CE88F` 不是暂停标志，是 BGM 类型 == 2」 | ✅ 复核确认。`0x17CE88F` 落在 `Supervisor` 的 config 区，与暂停无关。 |
| §2「建议切入点：`th08_rep_menu_3`（`0x46ec2e`）」 | ✅ **有效**。它在 `FUN_0046e136`（replay 菜单）里，正是"确认开始播放"的状态；置 replay bit 的指令在 `0x46ED45`（同一函数内）。 |
| §1「TH08 暂停菜单还有'暂停时间/继续'等选项，逻辑可能比 TH06/TH07 略复杂」 | 实测暂停菜单是 `一时停止 Pause / 閉じる / 再開 / タイトルに戻る / 最初からやり直す`，与 TH07 同构；状态字节仍是 0/1/2 三值，没有额外的子状态需要单独覆盖。 |
| §0.3「若某地址只在特定阶段有效，请特别标注」 | 三个地址都是静态全局量，**全程有效**。 |

---

## 5. 附带发现

### 5.1 `flags`（`0x164D0B4` = `GameManager + 0x3DBAC`）位表

| bit | 掩码 | 含义 | 证据 |
| --- | --- | --- | --- |
| 0 | `0x1` | 练习/符卡练习相关（标题菜单选 Spell Practice / Practice 时置位） | `0x46BC59`、`0x46CB51` 附近 `flags \|= 1` |
| 1 | `0x2` | **attract demo 播放中**（标题挂机自动演示）。置位时禁止暂停、启用"任意输入回标题"、并做帧预算自动退出 | 运行时实测（§3.1 第 8 行）+ `FUN_00439bc7` 的 demo 分支 |
| 2 | `0x4` | **关卡进行中 ∧ 未暂停 ∧ 不在另一个菜单**（每帧重算） | `FUN_00439bc7` @ `0x0043A5BC`：`flags = flags & 0xFFFFFFFB \| local<<2`，`local = (GM[+0x3DBB3]==0 && GM[+0x3DBB2]==0)` |
| 3 | `0x8` | **replay 播放中**（本报告 §2） | `0x46ED45` 置位；§3 实测 |
| 4 | `0x10` | ED / Staff Roll 播放中 | `FUN_00439bc7` @ `0x439EE0`/`0x439F0F` 附近 `flags \|= 0x10` |
| 14 | `0x4000` | replay 头 `+0x7C` 那个 word 是否非负（replay 开始时由 `0x46EDC5` 计算写入） | `0x46EDC8`–`0x46EDD7` |
| 15/16/17 | `0x38000` | **常驻位**，标题画面就有。含义未深究（疑似图像/音频/窗口模式等配置镜像），**门控不受影响** | §3.1 第 1 行 |

### 5.2 另一个全屏菜单 —— `0x164D0BB`（= `GM + 0x3DBB3`）

与 TH07 的 `0x62F64D`（续关菜单）对应的量。它是暂停触发条件里的**第一个守卫项**（见 §1.3：`GM[+0x3DBB3] != 0` 时不允许开暂停菜单），其状态机在 **`FUN_00404890`**（把 `gamemode_next` 设为 6/7/9/0xB，并在 `0x4048B6`/`0x4048F0` 清 `0x164D0BB`）。

**为什么建议把它也纳入**：这个菜单出现时 `gamemode` 仍是 2、`pause` 仍是 0、bit 3 也是 0——三个判据**全部成立**，会被误计。

**推荐做法（不改 `THGameTimeGate` 结构）**：把 `pause` 一项从 `0x164D0BA == 0` 换成 **`0x164D0B4` 的 bit 2（`BitClear 0x4`）**。该位是游戏自己每帧算的 `!(暂停菜单 ∨ 另一个菜单)`，语义恰好是"这一帧游戏真的在跑"：

```cpp
{ 0x13CE8B0, 4, 0x2, GateCmp::Eq       }   // gamemode == 2
{ 0x124D0B4, 4, 0x4, GateCmp::BitClear }   // pause  ← 同时覆盖两个菜单
{ 0x124D0B4, 4, 0x8, GateCmp::BitClear }   // replay
```

⚠️ 注意上位门控新增的 `0x164D0BB` 尚未运行时实测（§3.4），采纳前建议补一次探针。

### 5.3 结构体基址（复核）

| 对象 | 静态基址 | 字段 | 出处 |
| --- | --- | --- | --- |
| `Supervisor` | **`0x17CE758`** | `gamemode +0x158` = `0x17CE8B0`；`gamemode_next +0x15C` = `0x17CE8B4`；`gamemode_prev +0x160` = `0x17CE8B8` | `0x46CAB1`/`0x46B0DC` 等处的 `MOV ECX,0x17CE758` |
| `GameManager` | **`0x160F508`** | `difficulty +0x30` = `0x160F538`（= thprac `DIFF_ADDR` ✓）；`shottype +0x3DBA9` = `0x164D0B1`（= thprac `SHOTTYPE_ADDR` ✓）；`flags +0x3DBAC` = `0x164D0B4`；`word +0x3DBB0` = `0x164D0B8`；`pause +0x3DBB2` = `0x164D0BA`；`menu2 +0x3DBB3` = `0x164D0BB`；`replay_speed +0x3DBB5` = `0x164D0BD` | `0x470B3E: MOV ECX,0x160F508` 等 |

### 5.4 输入位（与 TH07 一致）

`0x164D528` = 本帧输入（word）、`0x164D530` = 上帧输入（= `INPUT_ADDR + 8`）。

| 位 | 含义 |
| --- | --- |
| `0x1` | Shoot（Z） |
| `0x2` | Bomb（X） |
| `0x4` | Slow（Shift） |
| **`0x8`** | **Pause（Esc）** —— 关卡内 `FUN_00439bc7` 用它开暂停菜单 |
| `0x10` / `0x20` | 上 / 下 |
| `0x40` / `0x80` | 左 / 右 |
| `0x1001` | 决定 |
| `0xA`（`0x8\|0x2`） | 取消 |

### 5.5 A 代同构证据（**给出 `gametime_thxx_flags.md` §1 分代假设的第一组实证**）

TH07 与 TH08 的暂停/回放实现**逐条同构**（同一作者、同一套代码模板）：

| 概念 | TH07 | TH08 | 形态 |
| --- | --- | --- | --- |
| `Supervisor` | `0x575950` | `0x17CE758` | 静态 |
| `gamemode` | `+0x154` (`0x575AA4`) | `+0x158` (`0x17CE8B0`) | `== 2` |
| `GameManager` | `0x626270` | `0x160F508` | 静态 |
| `flags` dword | `+0x93D8` (`0x62F648`) | `+0x3DBAC` (`0x164D0B4`) | **bit3 = replay，bit2 = 关卡中∧未暂停** |
| 暂停字节 | `+0x93DC` (`0x62F64C`) | `+0x3DBB2` (`0x164D0BA`) | **0/1/2 三值** |
| 暂停菜单状态机 | `0x402780` | `0x4037B0` | 同构（case 4=恢复 / 9=回标题 / 10=退出） |
| 关卡内每帧更新 | `0x42D8D5` | `0x439BC7` | 同构 |
| 暂停置 1 指令 | `0x42D958` | `0x43A119` | `input & 8` 新按下 |
| replay 置位指令 | `0x45B395` | `0x46ED45` | `flags & ~8 \| 8`，随后 `gamemode_next = 2` |
| thprac unpause 钩子 | `0x403481` | `0x40421A` | 都在暂停菜单机 case 4 |
| 输入字 / 暂停键位 | `0x4B9E4C` / `0x8` | `0x164D528` / `0x8` | 一致 |

**结论**：A 代（th06/th07/th08）"同构搬运"的假设**在 th07↔th08 上成立**。th09/th095 大概率可沿用同一套（`Supervisor` → `GameManager` 两个静态结构 + 两个 bit）。建议把本条回填进 `gametime_thxx_flags.md` §1。

---

## 6. 复现方法

### 6.1 静态（Ghidra）

```bash
cd D:/workshop/reasm/th08 && ./run.sh Th08Dump.java <out.txt> \
    xref:0x164D0B4 func:0x439BC7 range:0x46ED20:0x46EDE0
./run.sh Th08Scan.java <out.txt> 0x3DBAC:4 0x3DBB2:4 0x3DBB0:4   # 结构体偏移型访问
```
`Th08Dump.java` 支持：`xref:` `xrefr:` `xrefname:` `func:` `raw:` `range:A:B` `callers:` `callees:` `name:`。
**坑**：`postScript` 参数按**逗号**切分，`range:` 两端用 `:`。

### 6.2 运行时（探针）

```bash
cd D:/workshop/reasm/th08/tools
python s1_play.py       # 标题→游玩→Esc暂停→恢复
python s2_replay.py     # 标题→Replay 菜单→播放→Esc
python s6_clean.py      # 挂机等 attract demo → 按键回标题
```
`probe08.py` 由 `th07/tools/probe07.py` 改地址而来（结构完全复用），`grab.py` 同。

### 6.3 ⚠️ 坑（与 TH07 相同，**th09/th095 直接复用**）

1. **DirectInput 只认 `KEYEVENTF_SCANCODE` 的 `SendInput`**（`wVk=0` + 扫描码，方向键加 `KEYEVENTF_EXTENDEDKEY`）。普通虚拟键注入 / `keybd_event` / `PostMessage` 游戏全读不到。
2. **标题画面挂机约 21 s 会自动播放 attract demo**，状态在探针脚下就变了。
3. 沙箱：**别在游戏原目录里跑**（会写 `score.dat`/`th08.cfg`/`snapshot/`）；`th08.dat`(47 MB)/`thbgm.dat`(449 MB) 用 `os.link()` 硬链接，`data/` 真拷贝。`cmd //c mklink` 在 git-bash 里会被路径转换吞掉。
4. **demo 要在 fade-in 结束后（`bit2 == 1`）再按键**才能触发"回标题"。

---

## 7. 原始产物清单（`D:\workshop\reasm\th08\`）

| 路径 | 内容 |
| --- | --- |
| `ghidra\th08d` | Ghidra 工程 |
| `ghidra_scripts\Th08Dump.java` / `Th08Scan.java` | 通用查询 / 立即数扫描 |
| `out\recon1.txt` | gamemode、gamemode_next、输入字的交叉引用 |
| `out\core.txt` | **关卡内每帧更新 `FUN_00439bc7`** + `1→2` 的 `FUN_0043aa03` + replay 菜单 `FUN_0046e136` |
| `out\pause.txt` | 暂停字节 `0x164D0BA` 的全部引用 |
| `out\scan1.txt` / `scan2.txt` | `+0x3DBB2` / `+0x3DBAC` 等结构体偏移扫描 |
| `out\flags.txt` | flags dword `0x164D0B4` 的全部写入点 |
| `out\flagops.txt` / `or2.txt` / `bit2.txt` | 各写入点的精确指令（置/清 bit3、bit2 重算） |
| `out\pause2.txt` | 暂停触发条件、bit2 重算、`0x40421A` 钩子点 |
| `out\ss_*.png` | 三态截图存证（标题 / 关卡内 / 暂停菜单 / replay / demo） |
| `tools\probe08.py` `grab.py` `s*.py` | 运行时探针与驱动脚本 |
| `run\` | 运行沙箱（exe 拷贝 + 数据硬链接） |

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-29 | 建档。TH08 三条判据全部定出：`gamemode 0x17CE8B0==2`、`pause 0x164D0BA==0`、`replay 0x164D0B4&8==0`，**运行时三态验证通过**（9 组实测状态）。修正了需求文档"暂停标志在 `0x17CE8xx` 一带"的假设。附带给出 TH07↔TH08 同构对照表（分代假设的第一组实证）、`flags` 完整位表，以及"另一个菜单 `0x164D0BB` 是否纳入门控"的可选强化。 |

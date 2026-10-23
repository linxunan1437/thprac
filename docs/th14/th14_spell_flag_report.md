# TH14 符卡结束（spellFlag）— 静态逆向报告

> 上位文档：`docs/req_logger.md`（口径 12 / 13 / **15**）、`docs/logger/logger_re_disasm_overview.md`（术语与地址口径）、`docs/logger/logger_impl_report.md`（§4.4.2，th16 的 9 处接线）
> 需求来源：用户 2026-10-20 需求——把 th14 的 `stageClear` 升级为 **`spellFlag` 计数器**：只关心「一张符卡结束」（不区分收取/失败）；`spellFlag++` 由卡结束触发，`mSection` 换新 checkpoint 时 `--`（下限 0），退出时 `spellFlag > 0` 则不记 `interrupt`；`mStageClear` **保留为兜底**。
> 模板报告：`docs/th16/th16_spell_flag_report.md`（本作与 th16 需求完全同构，仅对象地址/偏移/结算函数不同）。
> 同作既有逆向：`docs/th14/th14_ecl_re_report.md`（Q1–Q9，含 `NameOfIp` 口径）、`docs/th14/th14_boss_phase_report.md`、`docs/th14/th14_re_disasm_report.md`。
> 二进制指纹：`../exes/th14.exe`，**897536 B**，PE `TimeDateStamp = 1376634201`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`；SHA-256 = `f08e885e28d2e0fc8c579e88034dd7de8f2a648d8f08ce781cf08eadc9ffe353`（见 `th14_ecl_re_report.md`）。
> ⇒ **RVA = VA − 0x400000**；`.text` = VA `0x401000`–`0x48F000` 量级（本报告只引用 `.text`/`.data` 内的确定地址）。
> 方法：`python3` + `pefile` + `capstone 5.0.7` **只读反汇编**；按「4 字节小端 VA 立即数」定位 xref；按 `E8 rel32` 扫描定位直接调用；离线解析 `.ecl` 的 SCPT 头 / sub 表 / 指令流，并按 CP932 解码 `.decl` 做脚本级对照。
> 素材：`../exes/ecl/th14/*.ecl`（24 份）、`../exes/ecl_dump/th14/*.decl`（24 份）。
> 状态：**静态定案（🟡 未真机）**。本机 macOS，无 MSVC、不可编译、不可真机；未真机验证的推断一律标 🟡。
> 地址口径：正文以 **VA** 为主，同时标注 **RVA = VA − 0x400000**。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 现有符卡 hook | **无**。`thprac_th14.cpp` 全部 EHOOK/PATCH 中**没有任何一个**读写 `0x4DB534`（符卡对象全局指针）；`th14_bomb_dec`（`0x41218A`）与 `th14_life_dec`（`0x44F618`）所在**函数后段**会读/清符卡状态，但 hook 覆盖的指令本身不碰它（详见 §1） |
| 符卡对象全局指针 | **`0x4DB534` / RVA `0xDB534`**；对象大小 **`0xC0`**（`0x41C511 push 0xC0` + `malloc`，`0x41C528`–`0x41C530` `memset(0,0xC0)`） |
| 存活位 | **`*(u32*)(obj + 0x7C) & 1`**（bit0）。**置位点唯一**：`0x41CBE4 or eax,3` → `0x41CBE7 mov [edi+0x7C],eax`（`spell` 声明函数 `0x41CB70`）；**清位点唯一**：`0x41D492 and dword [esi+0x7C],~1`（`spellEnd` 结算函数 `0x41D450`） |
| 其它字段 | `+0x78` = spell id；`+0x38` = 已解密的符卡名（CP932）；`+0x28` = 卡内帧数（60 帧宽限判据）；`+0x80` = 奖励值。**与 th16 不同**（th16 为 id `+0x74`、名 `+0x34`、flags `+0x78`） |
| ECL 声明符卡 | opcode **`0x0219`（`spell`）/ `0x021A` / `0x021B`（`spell3`）/ `0x020A`（`spellEx`）**，四个 opcode **共用处理器 `0x4288B3`、共用一个对象**；最终 id = `idBase + difficulty − (opcode − 0x219)`（`0x20A` 不调整） |
| ECL 结算符卡 | opcode **`0x020B`（`spellEnd`）** → 处理器 **`0x4289EA`** → **`0x41D450`**（`FUN_0041D450`）。`spellEnd()` **结算的是“上一张”卡**；每个攻击 sub 的开头都会调一次（首个非符/首个中boss 非符除外，此时无上一张） |
| 轮询可行性 | **可行**。每帧读 `obj+0x7C` bit0 的 **1→0 下降沿**即可。逐份 `.decl` 枚举 `setNext`：card→card 只有 **`st05bs BossCard3 → BossCard4`** 一处，但 `BossCard4` 开头有 **160 帧非符过渡（`wait(160)`）**，bit0 会保持 0 至少 160 帧 ⇒ **不会漏采样** |
| 推荐方案 | **纯轮询（零新 hook）**：`SampleSpellCard()` + `mSpellFlag` 计数器，`++` 在 `Update()` 顶部、`--` 在段落变化块；`mStageClear` 保留兜底 |
| 落地状态 | **已落地（2026-10-20，方案 A）**：`thprac/src/thprac/thprac_th14.cpp` 九处接线完成，见 §5；**未编译、未真机**（本机无 MSVC） |
| 语义 | **只做「符卡结束」事件**，不区分收取/失败；`spellFlag` 不是「当前是否在符卡中」，而是「已结束但尚未被新 checkpoint 消费的卡数」 |
| 最大覆盖缺口 | 没有“关底最后一张攻击阶段不 `spell()`”的关；主要缺口是**过渡非符/中boss 的计数滞留**（短则 120–160 帧、长则整段中boss）⇒ 该窗口退出会**假阴性**（少记 `interrupt`），`mStageClear` 只兜住 `BossDead`/`BossEscape` 之后的退出 |

---

## 1. 现有 hook 盘点（th14）

### 1.1 `thprac_th14.cpp` 全表

> 文件：`thprac/src/thprac/thprac_th14.cpp`（3788 行）。`EHOOK_*` 第二参为**覆盖长度**，不是“至少 5 字节”的 detour 长度。

**`HOOKSET_DEFINE(THMainHook)`（:3515）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th14_inf_lives` | `0x44F617` | 1 | 无限命 | 否（同函数后段 `0x44F746` 会清符卡收取位，见 §2.5） |
| `th14_everlasting_bgm` | `0x46EF90` | 1 | BGM | 否 |
| `th14_param_reset` | `0x45A111` | 7 | 参数复位 | 否 |
| `th14_prac_menu_1..4` | `0x45E768`/`0x45E787`/`0x45EA9F`/`0x45EB40` | 7/3/7/7 | 练习菜单 | 否 |
| `th14_prac_menu_enter_1/2` | `0x45E847`(patch)/`0x45EAFC` | -/1 | 进入练习 | 否 |
| `th14_menu_rank_fix` | `0x449C3D` | 5 | 菜单 rank | 否 |
| `th14_patch_main` | `0x4360CE` | 1 | 练习参数写入 / `THPatchSP` | 否 |
| `th14_bgm` / `th14_on_restart` / `th14_normal_game_enter` | `0x43699A` / `0x447810` / `0x45E63E` | 2/5/8 | BGM / 重开 / 入关 | 否（`th14_on_restart` 是可选的重置挂点，见 §5.5） |
| `th14_rep_save` / `th14_rep_menu_1..3` | `0x455BF2` / `0x45F10B`/`0x45F216`/`0x45F3ED` | 5/3/5/2 | rep | 否 |
| `th14_sp_menu_1/2` | `0x464068` / `0x463FD8` | 3/6 | **符卡练习菜单** | **间接**：`THGuiSP::State()` 读 `0x4DB6A4+0x28` 的练习 id 表；不读符卡对象 |
| `th14_update` | `0x40138A` | 1 | 每帧派发（含 `TH14Logger::Update()`，:3648） | **采样点** |
| `th14_player_state` | `0x44DBD0` | 1 | 键盘监视 | 否 |
| `th14_render` | `0x40149A` | 1 | 渲染 | 否 |

**`HOOKSET_DEFINE(THInGameInfo)`（:3687）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th14_game_start` | `0x4375BE` | 7 | 一局/一关开始的计数器复位；调 `OnStageStart()`（:3697） | 否 |
| `th14_bomb_dec` | `0x41218A` | 5 | bomb 计数 + `OnBomb()` | **函数**后段 `0x41219F` 读 `[0x4DB534]+0x7C` bit0 / `+0x28`（bomb 的符卡判定），**hook 指令本身不读** |
| `th14_life_dec` | `0x44F618` | 5 | miss 计数 + `OnMiss()` | **函数**后段 `0x44F746`/`0x44F7E4` 会 `and ~0x22` 清收取位，**hook 指令本身不读** |
| `th14_get_item` | `0x438DF8` | 6 | 20/16/12/8/5 道具计数 | 否 |
| `th14_lock_timer1..4` | `0x42EC6B`/`0x4286BA`/`0x427888`/`0x42C0D4` | 10/4/2/6 | 计时器锁 | 否 |

**静态 `EHOOK_ST` / `PATCH_ST`（namespace 作用域）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th14_marisa_laser` | `0x45286F` | 6 | 魔理沙激光 rep 修复 | 否 |
| `th14_all_clear_bonus_1/2/3` | `0x43708A`(patch)/`0x43712C`/`0x43719F` | -/7/7 | 全关奖励 | 否 |
| `th14_master_disable1a..1c` | `0x41CAD9`/`0x41CAB4`/`0x41CA0F` | patch | master 自动 ly 关闭 | 否 |
| `th14_bossmovedown` | `0x42725B` | 5 | boss 下移 | 否 |

**HOTKEY / `PATCH_HK` / `EHOOK_HK`（`THOverlay`）**

| 名称 | 地址 | 说明 | 与符卡有关？ |
| --- | --- | --- | --- |
| `mMuteki` | patch `0x44F877` | 无敌 | 否 |
| `mInfBombs` | patch `0x412173` | 无限 bomb | 否（bomb 结算在 `0x4121xx` 等，见 §2.5） |
| `mInfPower` | patch `0x44DDA5` | 无限 power | 否 |
| `mAutoBomb` | EHOOK `0x44DEC4`(7) | 自动 bomb | 否 |
| `mTimeLock` | patch `0x41C5DD`/`0x424ACA`；EHOOK `0x483C4F`(4) | 冻结 ECL 时间（`Edi`=节点） | 否 |
| `THInitHook` | patch `0x45996C`/`0x469387`/`0x4595A5`/`0x45A173`；EHOOK `0x45A591`/`0x46B37B` | GUI 初始化 | 否 |

**小结：没有任何 hook 读取 `0x4DB534`，也没有“符卡是否结束/是否收取”的判定。** `grep -n "4db534\|spellFlag" thprac_th14.cpp` 零命中。

### 1.2 符卡练习相关既有逻辑（只服务菜单）

| 项 | 位置 | 语义 |
| --- | --- | --- |
| `0x4DB6A4` | `:1262`（`THGuiSP::State`） | 指向练习菜单/记录对象；`+0x28` 是当前练习游标，`+0x5D70` 起是 spell id 表 |
| `THGuiSP::mSpellId` | `:1358` | 练习菜单当前 spell id；用于 `SpellPhase()`（`:1341`）与 `TH_SPELL_PHASE*` |
| `0x4F58B8` | `:3538` 等 | 游戏 flags（bit0 练习/重开、bit4/5 模式位） |
| `0x4F5828`/`0x4F582C`/`0x4F5834` | `:917`/`:1110` | 机体 / 子机 / 难度 |

> 这些地址都**不是**“符卡是否结束”。不要拿 `0x4F58B8` 或练习 id 当 spellFlag 来源。

---

## 2. 引擎侧：符卡对象

### 2.1 全局指针、大小、构造/销毁

| 项 | 证据（VA / RVA） |
| --- | --- |
| 全局指针 | **`0x4DB534` / `0xDB534`**：`0x428982 mov ecx,[0x4db534]`、`0x4289FC`、`0x41D459 mov esi,[0x4db534]` 等 |
| 构造函数 | **`0x41C510` / `0x1C510`**：`0x41C511 push 0xC0` → `0x41C516 call 0x4854AF`（`operator new`）→ `0x41C524 and [esi+0x34],~1` → `0x41C528`–`0x41C530 memset(esi,0,0xC0)` → `0x41C538 or [esi],2` → **`0x41C53B mov [0x4DB534],esi`** |
| 初始化 | `0x41C2B0`（由构造函数 `0x41C547` 调用）：创建 `+0x08`/`+0x0C` 两个对象、清 `+0x24..+0x30` |
| 唯一构造调用点 | `0x435FC3 call 0x41C510`（游戏/关卡启动路径；`0x435FBA` 先 `call 0x411BC0` 判定，失败则不建对象） |
| 析构函数 | **`0x41C390` / `0x1C390`**：释放音效对象，**`0x41C4F4 mov dword [0x4DB534],0`** |
| 常态销毁调用点 | `0x43642F`–`0x436446`：`mov esi,[0x4DB534]` → `call 0x41C390` → `free`（析构内部已把全局置 0） |
| 大小 | **`0xC0`** |
| 是否 memset | **是**，`0x41C530` 清零 `0xC0` 字节 ⇒ 新对象 bit0 = 0 |

**生命周期结论（静态）**：进关时构造（`0x435FC3`），回菜单/关卡销毁时析构并把全局置 0（`0x41C4F4`），重新构造时 memset 归零。**因此换关、重试、回菜单都会让 bit0 归 0**；但若“销毁 + 重建”发生在同一帧且 `malloc` 复用同一地址，轮询可能看不到“指针变空”的中间态 ⇒ 需靠换关/新局复位兜住（见 §5.5、§7）。

### 2.2 字段表（静态实测）

| 偏移 | 类型 | 语义 | 写入/读取点 |
| --- | --- | --- | --- |
| `+0x00` | u32 | 对象分配标志（`or 2`） | `0x41C538` |
| `+0x08` | ptr | 音效/对象 A | `0x41C2CF` |
| `+0x0C` | ptr | 音效/对象 B | `0x41C2ED` |
| `+0x10` | — | 另一对象（`0x47F6E0` 操作） | `0x41D496`/`0x41D4AA` |
| `+0x14`/`+0x18`/`+0x1C` | u32 | 音效句柄（`spellEnd` 逐个停） | `0x41D476`–`0x41D48D` |
| `+0x20` | ptr | 特效对象（`0x421D25` 释放） | `0x421D25` |
| `+0x24` | i32 | 初值 `0xFFF0BDC1` | `0x41CBA0`/`0x41C2FA` |
| `+0x28` | i32 | **卡内帧数**（与 `0x3C` 比较做 60 帧宽限） | `0x4124FD`/`0x412EE2`/`0x414C20`/`0x42C191`/`0x44F75A` |
| `+0x2C` | i32 | 0 | `0x41CBA0` |
| `+0x30` | u32 | `0x4D8F58` | `0x41CBA0` |
| `+0x34` | u32 | 初始化标志 bit0 | `0x41C524`/`0x41C2F0` |
| `+0x38` | char[0x40] | **已解密的符卡名（CP932）** | `0x41CBB0` 起拷贝；`0x41CBC8` 后紧邻 id |
| `+0x78` | i32 | **spell id（含难度，`spellEx` 不加）** | **`0x41CBC8 mov [edi+0x78],esi`** |
| `+0x7C` | u32 | **flags（见 §2.3）** | 多处，见 §2.4 |
| `+0x80` | i32 | 当前符卡奖励值/基数 | `0x41D4C4` 读、`0x41D4F2` 用；失败路径清 0 |
| `+0x90` | i32 | 1 | `0x41CCE3` |
| `+0x94` | i32 | HUD/显示用 | `0x42E970` 读 |
| `+0x9C` | i32 | 另一显示位 | `0x42E960` 读 |

> **与 th16 的字段差异（务必按作重新定位）**：th16 为 id `+0x74`、名 `+0x34`、flags `+0x78`、age `+0x24`、奖励 `+0x7C`；th14 为 id `+0x78`、名 `+0x38`、**flags `+0x7C`**、age `+0x28`、奖励 `+0x80`。**不可跨作套用。**

### 2.3 `+0x7C` flags 位语义

| bit | 值 | 语义 | 置位 | 清位 |
| --- | --- | --- | --- | --- |
| 0 | `0x1` | **符卡存活** | **`0x41CBE4 or 3` → `0x41CBE7`**（唯一） | **`0x41D492 and ~1`**（唯一） |
| 1 | `0x2` | **仍可收取（capture）** | 同 `0x41CBE4 or 3` | 失败路径 `and ~0x22`（见 §2.5） |
| 2 | `0x4` | 奖励倒计时已启动 | `0x41C6D3 or 4` | `0x41C71F and ~4` |
| 3 | `0x8` | 脚本置位（脚本类函数 `0x421CF0`，opcode 未定） | `0x421CF9` / `0x428A00` | — |
| 4 | `0x10` | 脚本置位（脚本类函数 `0x421D10`，opcode 未定） | `0x421D21` | — |
| 5 | `0x20` | “60 帧内失败”宽限标记 | 失败路径 `or 0x20` | `0x41D49E`（`spellEnd`）、失败路径 `and ~0x22` |
| 6 | `0x40` | 奖励计时中 | `0x41C823 or 0x40` | `0x41C8FC` / `0x41CCD3 and ~0x40` |
| 7 | `0x80` | 逃逸/超时处理过 | `0x42C183` / `0x421D89` | `0x41C010 and ~0x80` |
| 8 | `0x100` | 难度/模式派生 | `0x421CA8`（xor） | 同左 |

> **核心：`bit0` = 在符卡中，`bit1` = 还没失败（仍可收取）。** 我们的 spellFlag **只读 bit0**。

### 2.4 置位点 / 清位点唯一性（全 `.text` 扫描）

对 `0x4DB534` 的全部 60 处 xref 所在函数做写点扫描（`mov/and/or/xor [reg+0x7C]`），**作用于符卡对象的写点**如下：

| 写点 | 指令 | 作用 | 是否动 bit0 |
| --- | --- | --- | --- |
| **`0x41CBE7`** | `mov [edi+0x7C],eax`（前 `0x41CBE4 or eax,3`） | `spell` 声明：置 bit0+bit1 | **唯一的 0→1** |
| **`0x41D492`** | `and dword [esi+0x7C],0xFFFFFFFE` | `spellEnd`：清 bit0 | **唯一的 1→0** |
| `0x41D49E` | `and dword [esi+0x7C],0xFFFFFFDF` | `spellEnd`：清 bit5 | 否 |
| `0x41CCC0`/`0x41CCCA`/`0x41CCD3` | `and ~0x20` / `or 0x20` / `and ~0x40` | 声明期按 bomb 状态置/清宽限 | 否 |
| `0x412465`/`0x41247A`/`0x412520` | `mov [edx+0x7C],ecx` | bomb/miss 路径写回 | 否（写回值来自读出的 flags） |
| `0x412F06`/`0x4132A4`/`0x414C43` | `mov [edx+0x7C],ecx` | 同上（不同机体/子机） | 否 |
| `0x41566B`/`0x415C7B` | `mov [esi+0x7C],edx` | 同上（另两种 bomb 实现） | 否 |
| `0x42C189`/`0x42C1B4` | `mov [esi+0x7C],ecx` | 超时路径（先 `or 0x80` 再 `and ~0x22`） | 否 |
| `0x44F779`/`0x44F815` | `mov [edx+0x7C],ecx` | miss 路径 | 否 |
| `0x421CA8` | `xor [ecx+0x7C],eax`（`and 0x100`） | opcode 0x21E 类标志 | 否 |
| `0x421CF9`/`0x421D21`/`0x421D89`/`0x428A00` | `or 8` / `or 0x10` / `or 0x80` / `or 8` | 脚本位置位 | 否 |
| `0x41C010` | `and [ecx+0x7C],~0x80` | 清 bit7 | 否 |

> 结论：**bit0 的置位只有 `0x41CBE7`，清位只有 `0x41D492`**。对象构造时的 `memset` 不算“置位”；析构不写 `+0x7C`（直接 free）。
> `0x413604`/`0x4138FD`/`0x41621D` 等虽然同函数引用了 `0x4DB534`，但写的是**别的对象**的 `+0x7C`（内存初始化/链表），已排除。

**静态片段（声明，`0x41CBDC`–`0x41CBE7`）**

```asm
0041cbdc  8b 47 7c              mov  eax,[edi+0x7c]      ; edi = [0x4DB534]
0041cbdf  25 67 ff ff ff        and  eax,0xffffff67      ; 清 bit3/4/7
0041cbe4  83 c8 03              or   eax,3               ; ★ bit0=存活, bit1=可收取
0041cbe7  89 47 7c              mov  [edi+0x7c],eax
```

**静态片段（结算，`0x41D45E` / `0x41D492`）**

```asm
0041d457  8b 35 34 b5 4d 00     mov  esi,[0x4db534]
0041d45e  f6 46 7c 01           test byte [esi+0x7c],1   ; 不存活 ⇒ 空调用返回
0041d462  0f 84 39 01 00 00     je   0x41d5a1
...
0041d492  83 66 7c fe           and  dword [esi+0x7c],0xfffffffe ; ★ 清存活位
0041d49e  83 66 7c df           and  dword [esi+0x7c],0xffffffdf ; 清宽限位
0041d4af  f6 46 7c 02           test byte [esi+0x7c],2   ; bit1 ⇒ 收取成功
```

### 2.5 失败路径（清 bit1，不改 bit0）

所有失败路径形状一致：

```asm
mov  ecx,[obj+0x7c]
test cl,1                 ; 在符卡中？
je   skip
cmp  dword [obj+0x28],0x3C ; 卡内 ≥60 帧？
jl   early
mov  dword [obj+0x80],0   ; 奖励清零
and  ecx,0xffffffdd       ; ★ 清 bit1 + bit5
jmp  store
early:
  ; [0x4DB52C]+0x40 == 1 时 or ecx,0x20（只置宽限位）
store:
mov  [obj+0x7c],ecx
```

| 触发 | 证据（VA） | 同处还会写 |
| --- | --- | --- |
| **bomb / miss 的不同实现** | `0x412465`、`0x41247A`、`0x412520`、`0x412F06`、`0x4132A4`、`0x414C43`、`0x41566B`、`0x415C7B` | `[0x4DB544]+0x14` 自增（bomb 计数）等 |
| **miss（life_dec 同函数）** | `0x44F779`、`0x44F815`；`0x44F782 inc [mgr+0x10]`、`0x44F786 mov [mgr+0x18],0` | `$MISS_COUNT = mgr+0x10`、`$CAPTURE = mgr+0x18` |
| **超时** | `0x42C183 or 0x80`、`0x42C1A1 and ~0x22`、`0x42C1B4`；`0x42C171 or [host+0x5244],0x1000000` | `$TIMEOUT` = host `+0x5244` bit24（🟡 本作偏移，静态推定），`$CAPTURE` 清 0 |

> **注意**：`th14_life_dec` 在 `0x44F618`，清位在同函数更后面的 `0x44F746`；`th14_bomb_dec` 在 `0x41218A`，清位在 `0x4124xx` 等。**现有钩子拿不到收取结果**，这也是我们只做纯轮询、不依赖现有钩子的原因。

### 2.6 四个 opcode 共用一个对象

| opcode | decl 写法 | 处理器 | 是否同一对象 |
| --- | --- | --- | --- |
| `0x0219` | `spell` | `0x4288B3` | ✅ |
| `0x021A` | 未在本作 decl 出现 | `0x4288B3` | ✅ |
| `0x021B` | `spell3`（中boss） | `0x4288B3` | ✅ |
| `0x020A` | `spellEx`（Extra/中boss） | `0x4288B3` | ✅ |

四个 opcode 在 `0x4288B3` 内汇合，最终都是：

```asm
00428980  8b 0d 34 b5 4d 00   mov  ecx,[0x4db534]   ; this = 唯一符卡对象
...
00428994  e8 d7 41 ff ff      call 0x41cb70         ; 声明：写 +0x78/+0x38/+0x7C
```

⇒ **一张卡一个槽、一个全局对象**；中boss 与 Extra 的符卡同样覆盖。

---

## 3. ECL 侧：`spell` / `spellEnd`

### 3.1 opcode 与分发

th14 的 ECL 指令格式（与 th15/th16 同代）：

```text
[time:u32][opcode:u16][size:u16][? u16][mask:u8][? u8][stack:u32?][args...]
                                         ^ instr+0x0A = 可见性掩码
size 含头；args 从 instr+0x10 开始
```

- 基础 opcode：`0x4823D2` `movsx eax,word[ip+4]`；`cmp eax,0x5d`；`jbe` 走 `0x483E0C` 字节映射 + `0x483D00` 跳表。
- **扩展 opcode**：`0x4252C9` `movzx ecx,word[esi+4]` → `0x4252D0 add eax,0xFFFFFED4`（**减 `0x12C`**）→ `cmp eax,0x2BF` → `0x4252E0 movzx eax,byte[0x429EF4+eax]` → `0x4252E7 jmp [0x429C50+eax*4]`。
- 实测：`0x219 − 0x12C = 0xED`，`byte[0x429EF4+0xED] = 65`，`[0x429C50+65*4] = 0x4288B3`。

| opcode | decl | 处理器 VA / RVA | 语义 |
| --- | --- | --- | --- |
| `0x0219` | `spell(idBase,time,bgm,name)` | **`0x4288B3` / `0x288B3`** | 关底 boss 符卡 |
| `0x021A` | （未出现） | 同上 | id = `idBase + diff − 1` |
| `0x021B` | `spell3(...)` | 同上 | 中boss；id = `idBase + diff − 2` |
| `0x020A` | `spellEx(...)` | 同上 | Extra / ST7；id = `idBase`（不调整） |
| **`0x020B`** | `spellEnd()` | **`0x4289EA` / `0x289EA`** | 结算上一张 |
| `0x0238` | `spellMode(v)` | `0x4289CB` / `0x289CB` | 只翻转 boss `+0x3F8C` bit0；**不创建对象** |
| `0x021E` | （本作 decl 未出现，opcode 未定） | `0x421CF0` | `or [obj+0x7C],8` |
| `0x021F` | （本作 decl 未出现，opcode 未定） | `0x421D10` | `or [obj+0x7C],0x10` + 停音效 |

**id 公式证据（`0x42891C`–`0x428945`）**

```asm
0042891c  2d 19 02 00 00      sub  eax,0x219
00428921  74 1c               je   0x42893f          ; 0x219 → +diff
00428923  48                  dec  eax
00428924  74 0f               je   0x428935          ; 0x21A → +diff−1
00428926  48                  dec  eax
00428927  75 20               jne  0x428949          ; 其它（含 0x20A）→ 不调整
00428929  a1 34 58 4f 00      mov  eax,[0x4F5834]  ; difficulty
0042892e  83 c0 fe            add  eax,-2          ; 0x21B → +diff−2
```

### 3.2 `spellEnd()` 的结算对象与时机

`0x4289EA`：

```asm
004289ea  e8 61 4a ff ff      call 0x41d450        ; ★ 真正的结算
004289ef  83 a7 8c 3f 00 00 fe  and [edi+0x3f8c],~1 ; 清 boss 的“符卡中”标记
```

`0x41D450` 开头：

```asm
0041d457  8b 35 34 b5 4d 00   mov  esi,[0x4db534]
0041d45e  f6 46 7c 01         test byte [esi+0x7c],1
0041d462  0f 84 39 01 00 00   je   0x41d5a1        ; 无活动卡 ⇒ no-op（幂等）
```

⇒ `spellEnd()` 可以出现在任何 sub 里；没有活动符卡时是 **no-op**。

### 3.3 每张卡的 sub 结构（`st01bs.BossCard1` / `Boss2`，实测）

```text
BossCard1:                                  Boss2（下一张非符）:
  @BossItemCard();                            ... timerReset/killAllAsync ...
  timerReset();                               spellEnd();          ← ★ 结算 BossCard1
  killAllAsync();                             ...
  enmKillAll();                               $MISS_COUNT=0; $BOMB_COUNT=0; $CAPTURE=1;
  enmCreate("Ecl_EtBreak",...);               setNext(0,0,1500,"BossCard2");
  spellEnd();           ← 结算上一张        ...
  ...
  setNext(0,1900,2100,"BossCard2");
  spell(2,2400,500000,"水符「テイルフィンスラップ」");
  ... 攻击本体 ...
```

⇒ 两条硬结论（与 th16 相同）：

1. **`spell()` 在本张卡的 sub 里声明本张卡**；`spellEnd()` 在同一 sub 的更前面，**结算的是上一张卡**。
2. **“上一张卡的结算时刻” = 下一张攻击 sub 的第一帧**（无论下一张是符卡还是非符）。

### 3.4 `_at` / `_Dead` 的角色（简要）

| 名字 | 角色 | 对 spellFlag 的影响 |
| --- | --- | --- |
| `BossCardN` / `BossBCardN` / `MBossCardN` | **符卡本体**（声明 + 攻击主循环） | 记录名来源（CSV 精确匹配这些名字） |
| `BossCardN_at` / `_at2` / `BossCard2Move` / `_shadow` / `_eye` | 由本体 `@... async` 或同步 `@...` 派生的弹幕/演出子 | 同步 `_at` 会让宿主当前 sub 变成 `_at`，但 **bit0 是引擎状态，不受 sub 名影响** |
| `BossCardN_Dead` / `Card01HeadDead` / `TaikoDead` | `setDeath()` / `setNext()` 注册的死亡动画，boss 活着时就会跑 | 不在 CSV 白名单；精确匹配天然排除；**不产生符卡对象** |
| `BossN` / `MBossN` / `BossBN` | 非符攻击 | 不产生符卡事件 |
| `BossDead` / `BossEscape` / `BossBDead` / `BossBEscape` | 关卡收尾 | `BossDead` 在 `+60:` 才 `spellEnd()`；`BossEscape` 一开始就 `spellEnd()` |

---

## 4. 每帧采样可行性

### 4.1 最小读取集

```text
obj = *(u32*)0x4DB534;                 // 符卡对象；菜单/销毁后为 0
if (obj) {
    alive = *(u32*)(obj + 0x7C) & 1;   // 是否在符卡中（本需求唯一需要的位）
    id    = *(i32*)(obj + 0x78);       // 可选：诊断/记录
    name  = (const char*)(obj + 0x38); // 可选：CP932，NUL 结尾
    age   = *(i32*)(obj + 0x28);       // 可选：60 帧宽限
}
```

- 同一对象内的定长字段，无指针链、无野指针（`obj != 0` 时）。
- 名字长度 ≤ `0x40`（`+0x78` id 紧邻），读取时建议 `strnlen(name, 0x40)`（本迭代不需要）。

### 4.2 一帧一次采样会不会漏？

**判据**：`bit0` 的 1→0 必须至少保持到下一次 `Update()` 采样。最危险的情况是 **card → card 直接转移**：上一张的 `spellEnd()` 和下一张的 `spell()` 在同一帧完成，bit0 在该帧内 1→0→1，采样看不到。

逐份 `.decl` 枚举每个 sub 内的 `setNext(...,"名字")`，并额外检查 `@...Card...()` **直接调用**（正常流程里非符过渡 sub 会直接调下一张卡，例如 `Boss3→BossCard3`、`Boss5→BossCard5`、`Boss6→BossCard6`、`Boss9→BossCard9`、`Boss10→BossCard10`）；检查 source/target 是否都含 `Card`：

| 文件 | source sub | target | 是否真实 card→card | 结论 |
| --- | --- | --- | --- | --- |
| `st05bs` | `BossCard3` | `BossCard4` | **是**（`setNext`，唯一一处） | **安全**：`BossCard4` 在 `spell(80,...)` 之前有 `@BossItemPhase(25,18,64)` + `wait(160)`，bit0 至少保持 0 160 帧 |
| `st02bs` | `Card02Head` | `Card01HeadDead` | 否（头部件动画） | 不影响 |
| `st03bs` | `BossCard2Wolf` | `Card01HeadDead` | 否（死亡动画） | 不影响 |
| `st03mbs` | `MBossCard1Wolf` | `Card01HeadDead` | 否（死亡动画） | 不影响 |
| 其余 20 份 `.decl` | — | — | **0 处 card→card**；直接调用全部是「非符过渡 sub → 下一张卡」 | 非符↔符卡交替，bit0 在非符期长时间为 0 |

**证据（`st05bs.decl:819-913`）**

```text
BossCard3:
    setNext(0,2700,2100,"BossCard4");
    spell(76,2100,...);
BossCard4:
    ... spellEnd(); ...          ← 结算 BossCard3（第 1 帧）
    @BossItemPhase(25,18,64);
    wait(160);                   ← ★ 160 帧非符过渡，bit0 = 0
    ... spellEnd(); ...          ← no-op
    spell(80,3000,...);          ← 第二张卡声明
```

⇒ **th14 的 card→card 不会造成漏采样**（与 th16 的“0 处 card→card”结论不同，但结果同样安全）。

### 4.3 采样时机

`TH14Logger::Update()` 由 `th14_update`（`0x40138A`，每帧主循环）调用。ECL tick 可能在本帧稍后执行，因此采样最多有 **≤1 帧延迟**（与“当前段落”同源）。由于 bit0 在任何非符期都保持 0 数十到数千帧，1 帧延迟不影响判定。

---

## 5. 接入方案（`thprac_th14.cpp`，精确插入点）

> **已落地（2026-10-20，方案 A）**，代码在 `thprac/src/thprac/thprac_th14.cpp`；下表行号为落地前的参考值，落地后实际行号见 §5.6。**未编译、未真机**。
> 落地的成员命名以 th16 为准（`mSpellActive`），本表草图中的 `mSpellAlive` 未采用。

### 5.1 常量

| # | 位置 | 改动 |
| --- | --- | --- |
| 1 | `enum addrs`（`:18`–`:26`，`WINDOW_PTR` 之后） | `SPELL_OBJ_PTR = 0x4DB534,` |
| 2 | 匿名 namespace 常量（`:625`–`:723`，`kNodeIpOff`（`:644`）附近） | `constexpr uintptr_t kSpellFlagsOff = 0x7C;`<br>`constexpr uint32_t kSpellAliveBit = 1;` |

### 5.2 成员与方法

| # | 位置 | 改动 |
| --- | --- | --- |
| 3 | `TH14Logger` private 成员（`:799` `mStageClear` 之后） | `int32_t mSpellFlag = 0;`<br>`uintptr_t mSpellObj = 0;`<br>`bool mSpellAlive = false;` |
| 4 | private 方法声明（`:778` `NameOfIp` 之后） | `void SampleSpellCard();` |
| 5 | 定义（`:905` `OnBomb()` 之后、`:907` `Update()` 之前） | 见下方草图 |

```cpp
// 只读、零分配：每帧由 Update() 顶部调用。
void TH14Logger::SampleSpellCard()
{
    uintptr_t obj = GetMemContent<uintptr_t>(SPELL_OBJ_PTR);
    if (!obj) {
        // 菜单 / 销毁中：只清采样状态，保留计数（退出帧还要用）。
        mSpellObj = 0;
        mSpellAlive = false;
        return;
    }

    bool alive = (GetMemContent<uint32_t>(obj + kSpellFlagsOff) & kSpellAliveBit) != 0;

    if (obj != mSpellObj) {
        // 新对象：只同步状态，不产生事件（避免重建/换关的假下降沿）。
        mSpellObj = obj;
        mSpellAlive = alive;
        return;
    }

    if (mSpellAlive && !alive) {
        ++mSpellFlag; // 一张符卡结束（不区分收取/失败）
    }
    mSpellAlive = alive;
}
```

### 5.3 九处接线（对应 th16 §4.4.2 的 9 点）

| # | 位置（当前行号 + 语句） | 改动 |
| --- | --- | --- |
| 1 | `:18`–`:26` `enum addrs { ... WINDOW_PTR = 0x4f5a18, };` | 加 `SPELL_OBJ_PTR = 0x4DB534` |
| 2 | `:644` `constexpr uintptr_t kNodeIpOff = 4;` 之后 | 加 `kSpellFlagsOff = 0x7C` / `kSpellAliveBit = 1` |
| 3 | `:799` `bool mStageClear = false;` 之后 | 加 `mSpellFlag` / `mSpellObj` / `mSpellAlive` |
| 4 | `:778` `const char* NameOfIp(...) const;` 之后 | 加 `void SampleSpellCard();` |
| 5 | `:905` `OnBomb()` 结束 `}` 之后 | 加 `SampleSpellCard()` 定义 |
| 6 | `:882`–`:893` `OnStageStart()` 内 `:889 mStageClear = false;` 之后 | `mSpellFlag = 0; mSpellObj = 0; mSpellAlive = false;` |
| 7 | `:907` `void TH14Logger::Update()` 顶部、`:913` interrupt 判定之前 | 先调 `SampleSpellCard();`；再把 `:913` 条件改为 `if (mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame))` |
| 8 | `:929`–`:937` 换关块（`if (stageNum != mBuiltStage) { ... mStageClear = false; }`） | 在 `:936 mStageClear = false;` 之后加三者的复位 |
| 9 | `:1053`–`:1055` 段落变化块 | 在 `mStageClear = false;` **之前**加 `if (mSpellFlag > 0) { --mSpellFlag; }` |

**第 9 点的准确位置（th14 的段落判据）**

```cpp
// :1018
const char* prevSection = mSection;
...
// :1053-1055
if (mSection != prevSection && std::strcmp(mSection, prevSection) != 0) {
    if (mSpellFlag > 0) {
        --mSpellFlag;          // ★ 新 checkpoint 消费一张已结束的卡
    }
    mStageClear = false;
}
// :1061
mSession.Latch(GameLogRecordOfSection(mSection));
```

- **th14 与 th16 的 `mSection` 判据形式相同**：都是 `prevSection` + `mSection != prevSection && strcmp`；但 th14 在 `:1026`–`:1032` 还有 **中boss 结束后的 `mPending` 提升**，它也会改 `mSection`。**`--` 必须只放在 `:1053` 的最终比较块里**，不能放在 `:1038`/`:1044`/`:1049` 的每次赋值处，否则中boss 提升会被重复消费。
- `:920`–`:928` 的 `stageNum` 归一（Extra=9→7）与 `:925` 的菜单 early return 都在采样之后，不影响 `++`；采样放在 `Update()` 最顶部即可。
- `:938`–`:945` 的 `mStageMap.empty()` / `mgr == 0` early return 在采样之后，所以这些帧仍能吃到 `++`，但不会 `--`/Latch（与现有行为一致）。

### 5.4 顺序约束（三条都不能反）

1. `SampleSpellCard()` 在 interrupt 判定**之前** ⇒ 同帧「清卡 + 退出」也能吃到 `++`。
2. `++` 在 `Update()` 顶部、`--` 在段落裁决之后 ⇒ 同帧「卡结束 + 新 checkpoint」净 0；反过来会残留 1，吞掉之后整关的 `interrupt`。
3. 换关复位在采样之后 ⇒ 换关/对象销毁的假下降沿当场抹掉；**对象为空时只清 `mSpellObj/mSpellAlive`、不清 `mSpellFlag`**，否则退出帧还没用上就被清了。

### 5.5 与 th16 的差异 / 改法说明

| 项 | th16 | th14 | 改法 |
| --- | --- | --- | --- |
| 对象全局 | `0x4A6DB0` | **`0x4DB534`** | 换常量 |
| flags 偏移 | `+0x78` | **`+0x7C`** | 换常量 |
| id / 名字 | `+0x74` / `+0x34` | `+0x78` / `+0x38` | 本迭代不读，若诊断需改 |
| 对象大小 | `0xBC` | **`0xC0`** | 仅影响越界校验 |
| 声明函数 | `0x417F00` | **`0x41CB70`** | 只作证据 |
| 结算函数 | `0x4182F0` | **`0x41D450`** | 只作证据 |
| `Update()` 结构 | 无中boss pending | **有 `mPending` / `mMidBossActive`（`:1026`–`:1051`）** | `--` 只放最终段落比较块 |
| 段落采样 | sub 索引表 | **`NameOfIp`（IP→名）** | 与 spell 轮询无关，互不影响 |
| 重试 | `th16_game_start` | `th14_on_restart`（`:3604`）/ `th14_game_start`（`:3688`） | 可选：在 `th14_on_restart` 里加一个只复位 spell 采样状态的 `ResetSpellSample()`（**不属于 9 处接线**，用于防“同地址重建”的假下降沿） |

**结论：状态机与 9 处接线骨架可以照搬 th16，但常量必须按本作替换（对象 `0x4DB534`、flags `+0x7C`），且 `--` 必须落在 th14 自己的最终段落比较块（`:1053`）里。**

### 5.6 落地结果（2026-10-20，方案 A）

代码：`thprac/src/thprac/thprac_th14.cpp`（落地后 3856 行）。九处实际行号：

| # | 位置 | 落地行 |
| --- | --- | --- |
| 1 | `enum addrs`：`SPELL_OBJ_PTR = 0x4db534,` | `:26` |
| 2 | 匿名 namespace：`constexpr uintptr_t kSpellFlagsOff = 0x7C;`（含注释） | `:647`–`:653` |
| 3 | `TH14Logger` 成员：`mSpellFlag` / `mSpellObj` / `mSpellActive` | `:812`–`:814` |
| 4 | 方法声明：`void SampleSpellCard();` | `:789` |
| 5 | 定义 `SampleSpellCard()`（`OnBomb()` 之后、`Update()` 之前） | `:931`–`:961` |
| 6 | `OnStageStart()` 三者复位 | `:910`–`:912` |
| 7 | `Update()` 顶部 `SampleSpellCard();` + `if (mSpellFlag <= 0 && !mStageClear && ...)` | `:967` / `:972` |
| 8 | 换关块三者复位 | `:995`–`:997` |
| 9 | 段落变化块 `if (mSpellFlag > 0) { --mSpellFlag; }`（`mStageClear = false;` 之前） | `:1119`–`:1120` |

`IsStageClearSub()` / `mStageClear` / `sawStageClear` 全部保留（方案 A 兜底），未新增 EHOOK，未改共享层，未引入新事件类型。

静态检查（本机无 MSVC，未编译）：`git diff --check -- thprac/src/thprac/thprac_th14.cpp` 通过；剥离注释/字符串后的 `()[]{}` 括号平衡检查通过（无未配对/未闭合）；符号声明与定义各一处。

---

## 6. 覆盖性（逐关，含 ST4 A/B）

### 6.1 关底（stage boss）

| 关卡 | 正常流程（setNext 链） | 最后一张攻击阶段 | 是否 `spell()` | 最后一张卡 → 收尾 |
| --- | --- | --- | --- | --- |
| ST1 | `Boss1 → BossCard1 → Boss2 → BossCard2 → BossDead` | `BossCard2` | ✅ | `BossCard2 → BossDead` |
| ST2 | `Boss1 → BC1 → Boss2 → BC2 → Boss3 → BC3 → BossDead` | `BossCard3` | ✅ | `BC3 → BossDead` |
| ST3 | `Boss1 → BC1 → Boss2 → BC2 → Boss3 → BC3 → BossDead` | `BossCard3` | ✅ | `BC3 → BossDead` |
| ST4 A | `Boss1 → BC1 → Boss2 → BC2 → Boss3 → BC3 → BossDead` | `BossCard3` | ✅ | `BC3 → BossDead` |
| ST4 B | `BossB1 → BBC1 → BossB2 → BBC2 → BossB3 → BBC3 → BossBDead` | `BossBCard3` | ✅ | `BBC3 → BossBDead` |
| ST5 | `Boss1 → BC1 → Boss2 → BC2 → Boss3 → BC3 → BossCard4[160f 非符 + 第二张卡] → BossDead` | `BossCard4`（第二段，`spell(80)`） | ✅ | `BC4 → BossDead` |
| ST6 | `Boss1 → BC1 → Boss2 → BC2 → Boss3 → BC3 → Boss4 → BC4 → Boss5 → BC5 → Boss6 → BC6 → BossDead`（`BC5`/`BC6` 由 `Boss5`/`Boss6` 直接 `@` 调用） | `BossCard6` | ✅ | `BC6 → BossDead` |
| ST7 正常 | `Boss1 → BC1 → … → BC8 → Boss9 → BC9 → Boss10 → BC10 → BossDead`（`BC9`/`BC10` 由 `Boss9`/`Boss10` 直接 `@` 调用） | `BossCard10` | ✅（`spellEx 119`） | `BC10 → BossDead` |
| ST7 练习 | `BossCard10`（`SPELL_ID==119` 分支） | `BossCard10` | ✅ | 同正常流程 |

**关键结论**：

- **所有关底的“最后一张攻击阶段”都确实 `spell()`**（ST7 最后也是 `BossCard10`，不是 `Boss10` 非符；`Boss10` 只是 `wait(120)` 后直接 `@BossCard10()` 的过渡非符）。
- `BossDead` 的 `spellEnd()` 在 **`+60:` 之后**（`st01bs.decl:587` 起；各关同形），所以最后一张卡死亡后 **60 帧内 bit0 仍为 1**；这 60 帧内退出，`spellFlag` 还是 0，**必须靠 `mStageClear` 兜底**。
- `BossEscape` 一开始就 `spellEnd()`，且可在**任意非符期**由超时触发；同样靠 `mStageClear` 兜底。
- **过渡非符**（`Boss3`/`Boss5`/`Boss6`/`Boss9`/`Boss10`、ST5 `BossCard4` 内部 160 帧等）不是 CSV checkpoint：卡结束后到下一张卡/下一个白名单 sub 之间，`spellFlag` 会滞留 120–160 帧以上 ⇒ 该窗口退出会**假阴性**（§6.4）。

### 6.2 中boss

| 关卡 | 正常流程 | 最后阶段 | 是否有符卡对象 | 备注 |
| --- | --- | --- | --- | --- |
| ST1 | E/N：`MBoss → MBossDead`；H/L：`MBoss1 → MBossCard1 → MBossDead` | E/N 非符；H/L 符卡 | **仅 H/L** | E/N 无卡 |
| ST2 | `MBoss1 → MBossCard1 → MBossDead` | 符卡 | ✅ | |
| ST3 | `MBoss1 → MBoss2 → MBossDead`；MBoss2 在 **H/L** 时 `wait(120)` 后直接 `@MBossCard1()`（`st03mbs.decl:172`），E/N 不进该分支 | E/N 非符；H/L 符卡 | **仅 H/L** | E/N 无卡；`MBossCard1` 的 `$SPELL_ID` 分支只是练习起点 |
| ST4 A | `MBoss1 → MBoss2 → MBossDead` | 非符 | ❌ | `MBossCard1` 仅练习 |
| ST4 B | `MBossB1 → MBossB2 → MBossBDead` | 非符 | ❌ | `MBossBCard1` 仅练习（`st04mbs2.decl:56`） |
| ST5 | `MBoss1 → MBossCard1 → MBossDead` | 符卡 | ✅ | |
| ST6 | `MBoss1 → MBoss2 → MBossDead` | 非符 | ❌ | 整场无卡 |
| ST7 A | `MBoss1 → MBossSuspend → MBoss2 → MBossCard2 → MBossSuspend2 → MBoss3 → MBossCard3 → MBossSuspend3 → MBossDead` | 符卡 | ✅（`spellEx 108/109`） | A 路线 |
| ST7 B | `MBoss1 → MBossSuspend → MBoss2B → … → MBoss3B → … → MBossDead` | 非符 | ❌ | B 路线只用 `spellMode(1)`（`st07mbs.decl:500/609`），**不建对象** |

### 6.3 `BossEscape` 是否可能在非符期触发？

**是**（静态）：`BossEscape` 是独立的收尾 sub，`st01bs.decl:612` / `st07bs.decl:2517` 等每关都有一份；它不依赖当前是否在符卡中，开头就 `spellEnd()`（无卡时 no-op）。stage boss 的 `setTimeout` 不在 ECL 里显式出现（与中boss 的 `setTimeout(1,"MBossEscape")` 不同），推定由引擎在关底计时结束时启动（🟡 触发条件未真机）。因此**超时可能发生在任意非符阶段**，此时 bit0 不会产生下降沿 ⇒ **`mStageClear` 必须保留**。

### 6.4 有洞 / 兜底是否足够

| # | 洞 | 影响 | `mStageClear` 是否足够 |
| --- | --- | --- | --- |
| 1 | **过渡非符**（`Boss3`/`Boss5`/`Boss6`/`Boss9`/`Boss10`、ST5 `BossCard4` 内部 160 帧等）不在 CSV | 卡结束后到下一个白名单 checkpoint 之间，计数滞留 120–160 帧以上；该窗口退出**漏记** `interrupt`（假阴性） | ❌ 不够；这些过渡非符不是 stage clear。与 th16 `knownBugs.md` #12 同类副作用 |
| 2 | 关底最后一张卡死亡 → `BossDead+60` 才 `spellEnd()` | 60 帧内无下降沿 | ✅ `mStageClear` 在 `BossDead` 第一帧就置位 |
| 3 | 超时走 `BossEscape`（任意非符期） | 非符期没有下降沿 | ✅ `mStageClear` 在 `BossEscape` 第一帧置位 |
| 4 | ST4 A/B、ST6 中boss、ST7 B 路线无符卡对象 | 不产生事件；退出按原逻辑记 `interrupt` | 不需要兜底（本来就没有卡） |
| 5 | ST1 E/N、ST3 E/N 中boss 无卡 | 同上 | 不需要 |
| 6 | ST5 `BC3 → BC4` 的 160 帧非符过渡 | bit0 有 160 帧为 0 | ✅ 采样可见 |
| 7 | 中boss 最后一张卡之后（`MBossDead`/`MBossEscape`/`MBossSuspend` 不在 CSV） | 计数可能滞留到下一个关卡 checkpoint | 中boss 不是 stage clear，`mStageClear` 不覆盖；假阴性可接受 |
| 8 | 符卡练习（`$SPELL_ID` 分支直接 `@BossCardN()`） | 练习结束路径可能不经过 `spellEnd`（首个 `Boss1` 无 `spellEnd`） | 🟡 需真机确认；若 bit0 不落，退出可能误记 `interrupt` |

> **总体判断**：对“关底最后一张卡结束后退出被误记 `interrupt`”这一主目标，`spellFlag + mStageClear` **足够**；对“非符尾巴/中boss/练习”只保证不漏卡，代价是可能**假阴性**（少记 `interrupt`），这与 th16 的取舍一致。

---

## 7. 风险与真机验证项（🟡）

| # | 项 | 现状 / 已排除 | 下一步最小验证（真机读哪里、期望值） |
| --- | --- | --- | --- |
| 1 | `0x4DB534` 在标题/菜单为 0 | 静态：构造只在 `0x435FC3` 的启动路径；析构 `0x41C4F4` 置 0 | 标题读 `*(u32*)0x4DB534`，期望 0；进关后非 0 |
| 2 | bit0 就是“在符卡中” | 静态：声明 `0x41CBE4 or 3`；`spellEnd` `0x41D45E test` / `0x41D492 and ~1`；失败路径只清 bit1 | 打 ST1 `BossCard1`：卡开始后 `obj+0x7C & 1` = 1；击破下一帧变 0 |
| 3 | `spellEnd` 结算上一张 | 静态：`0x4289EA` → `0x41D450`，不依赖当前 sub | 连续 `BC1→Boss2`：`Boss2` 第一帧 `bit0` 变 0、`+0x78` 仍为上一张 id |
| 4 | ST5 `BC3→BC4` 不漏采样 | 静态：`BossCard4` 先 `wait(160)` 再 `spell(80)` | 打 ST5：`BC3` 结束到 `BC4` 声明之间，连续 160 帧 `bit0` = 0 |
| 5 | 对象重建假下降沿 | 静态：构造 memset 0；析构置全局 0；但同地址重建可能跳过“空指针”帧 | 重试同一关：在重试前后打印 `obj` 与 `bit0`，确认没有额外 `++`（若有，需在 `th14_on_restart` 复位采样状态） |
| 6 | 练习模式 | 🟡 直接 `@BossCardN()`，首个 `Boss1` 无 `spellEnd` | 单符卡练习：收卡 → 重试 → 退出，确认计数不跨局泄漏、不误记 `interrupt` |
| 7 | `BossEscape` 触发条件 | 🟡 ECL 内无 stage boss `setTimeout`，推定引擎注册 | 关底不击破拖到超时：确认 `BossEscape` 运行、`mStageClear` 置位、退出无 `interrupt` |
| 8 | `BossDead +60` 窗口 | 静态：`+60:` 后才 `spellEnd` | boss 爆炸瞬间按 Esc：期望无 `interrupt`（验证 `mStageClear`） |
| 9 | ST7 过渡非符 `Boss10`（120 帧） | 静态：`BC9 → Boss10 → BC10`；`Boss10` 不是 CSV checkpoint | 在 `Boss10` 的 120 帧内退出：预计**不记** `interrupt`（假阴性）；确认可接受 |
| 10 | 采样时序 | 静态：`th14_update` 在 ECL tick 之前/之后未定 | 在 `SampleSpellCard()` 打印 `obj/bit0/帧号`，与 IGI 当前段落对照，确认 ≤1 帧 |
| 11 | 变体 exe（Steam/汉化） | 未核 | 按变体重跑 `0x4DB534`/`0x41CBE7`/`0x41D492` 的字节比对 |
| 12 | 本机无法编译 | 无 MSVC | 本报告只做静态设计；改动需在 Windows Release\|x86 编译并真机验证 |

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **建档**。th14 `spellFlag` 静态调研：① 盘点 `thprac_th14.cpp` 全部 EHOOK/PATCH，确认**无任何 hook 读写符卡对象 `0x4DB534`**（`th14_bomb_dec`/`th14_life_dec` 的**函数后段**才碰，hook 指令本身不碰）；② 定案符卡对象 **`0x4DB534`**、大小 **`0xC0`**（构造 `0x41C510`/`malloc 0xC0`/`memset`、析构 `0x41C390`/`0x41C4F4` 置 0）、flags 在 **`+0x7C`**（bit0 存活、bit1 可收取），**置位唯一 `0x41CBE7`、清位唯一 `0x41D492`**；③ 定案 ECL：`spell/0x219`、`0x21A`、`spell3/0x21B`、`spellEx/0x20A` 共用 **`0x4288B3`** 与同一对象，`spellEnd/0x20B` → **`0x4289EA` → `0x41D450`**；扩展 opcode 分发基址 `0x12C`、字节表 `0x429EF4`、跳表 `0x429C50`；④ 逐份 `.decl` 枚举 `setNext`：card→card 仅 `st05bs BossCard3→BossCard4`，且有 160 帧非符过渡 ⇒ 一帧一采样不漏；⑤ 给出 `thprac_th14.cpp` 的 9 处精确接线（`:18`/`:644`/`:799`/`:778`/`:905`/`:889`/`:909-913`/`:936`/`:1053`）与 th14 独有注意点（`mPending` 提升、`--` 只放最终比较块）；⑥ 逐关覆盖表（含 ST4 A/B、ST7 A/B、练习分支）与洞/兜底分析。**全部为静态结论；本机无编译环境，未编译、未真机（🟡）**。 |

| 2026-10-20 | **代码落地（方案 A，未编译/未真机）**。在 `thprac/src/thprac/thprac_th14.cpp` 完成九处接线：`SPELL_OBJ_PTR = 0x4db534`（`:26`）、`kSpellFlagsOff = 0x7C`（`:653`）、成员 `mSpellFlag`/`mSpellObj`/`mSpellActive`（`:812`–`:814`）、`SampleSpellCard()` 声明/定义（`:789`/`:931`）、`OnStageStart()` 复位（`:910`）、`Update()` 顶部采样 + `mSpellFlag <= 0` 条件（`:967`/`:972`）、换关复位（`:995`）、段落变化块 `--`（`:1119`）。保留 `IsStageClearSub`/`mStageClear` 兜底；未新增 EHOOK。静态检查：`git diff --check`（本文件）通过、括号平衡通过。**未编译、未真机（🟡）**。 |

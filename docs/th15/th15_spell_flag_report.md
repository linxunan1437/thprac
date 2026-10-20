# TH15 符卡结束（spellFlag）— 静态逆向报告

> 上位文档：`docs/req_logger.md`（**口径 12**「stageClear / interrupt」、**口径 15**「spellFlag 计数器」）、`docs/logger/logger_re_disasm_overview.md`（术语与地址口径）、`docs/logger/logger_impl_report.md`（§4.4.2 th16 的 spellFlag 落地）
> 模板报告：`docs/th16/th16_spell_flag_report.md`（本次需求与 th16 完全同构，只是换一作）
> 需求来源：用户 2026-10-20 —— 把 th15 的 `stageClear` 升级为 **`spellFlag`**，只关心「**一张符卡结束**」这一事件（**不区分收取 / 失败**）；`mStageClear` **保留为兜底**
> 同作既有逆向：`docs/th15/th15_ecl_re_report.md`（道中 Q1–Q9）、`docs/th15/th15_boss_phase_report.md`（boss 宿主与 sub 名链）
> 实现对照：`thprac/src/thprac/thprac_th15.cpp`（`TH15Logger`，落地前 **3179 行** / 落地后 **3250 行**）、`thprac_th16.cpp`（已落地的 9 处接线）
> 额外素材：`../th15decode/th15.exe_decompiled.c`（Ghidra 全量反编译，146950 行）、`../th15decode/ecl-system.md`、`../th15decode/boss-phase-hook.md`
> 二进制指纹：`../exes/th15.exe`，**951808 B**，PE `TimeDateStamp = 1444028071`（`0x56121EA7`），`ImageBase = 0x400000`，**RELOCS_STRIPPED**
> （⇒ **RVA = VA − 0x400000**；`.text` = VA `0x401000`–`0x4BD92B`，RVA `0x1000`–`0xBC92B`；`.rdata` 自 `0x4BE000`，`.data` 自 `0x4DE000`）
> 方法：`pefile 2024.8.26` + `capstone 5.0.7`（x86-32）只读反汇编；Ghidra 反编译 C 交叉核对；`RELOCS_STRIPPED` ⇒ 用 4 字节小端 VA 立即数做字节搜索定位 xref；离线 `.decl` 枚举（23 份）
> ECL 素材：`../exes/ecl/th15/*.ecl`（23 份）、`../exes/ecl_dump/th15/*.decl`（23 份）
> 状态：**静态定案（🟡 未真机）**。本机 macOS，不可编译、不可真机；文中未真机验证的读数一律标 🟡
> 地址口径：正文以 **VA** 为主，同时标注 **RVA = VA − 0x400000**

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| **落地状态** | ✅ **已落地（2026-10-20，方案 A）**。`thprac/src/thprac/thprac_th15.cpp` 已按 §5 接线（`SPELL_OBJ_PTR = 0x4e9a70`、`kSpellFlagsOff = 0x78`、`SampleSpellCard()`、`mSpellFlag` 的 `++`/`--`/三处复位，`IsStageClearSub` / `mStageClear` 兜底**保留**）。**未编译、未真机（🟡）**，验证项见 §7。 |
| **现有符卡 hook** | **无**。`thprac_th15.cpp` 的 50 处 EHOOK/PATCH 里没有任何一个读写符卡对象；`0x4E9A70` 在 `thprac/src/**` 与全仓库文档中 **0 引用**（详见 §1） |
| **符卡对象全局指针** | **`0x4E9A70`** / RVA `0xE9A70`。构造 `FUN_0041FD00`（`0x41FD00`，`malloc(0xBC)`）；析构 `FUN_0041FB70`（`0x41FB70`，末尾 `0x41FCED mov [0x4e9a70],0`） |
| **对象大小** | **`0xBC`**（`0x41FD01 push 0xbc`；`0x41FD43 call 0x49b080` 即 `memset(obj,0,0xBC)`） |
| **「符卡存活」位** | **`*(u32*)(obj + 0x78) & 1`** —— **偏移与 th16 相同（`+0x78` bit0），但本作已独立验证**（不是照搬） |
| **置位点** | **唯一**：`0x420477 or eax,3` + `0x42047A and eax,0xffffff67` + **`0x42047F mov [edi+0x78],eax`**（`FUN_00420400` = 符卡声明初始化，`EDI = *(u32*)0x4E9A70`） |
| **清位点** | **两处**（th16 只有一处）：① **`0x420D62 and dword [esi+0x78],0xfffffffe`**（`FUN_00420D20` = `spellEnd`，`ESI = *(u32*)0x4E9A70`）；② **`0x420EDA and dword [ebx+0x78],0xfffffffe`**（`FUN_00420E90` = 强制清卡，仅重开/载入路径可达） |
| **四个 opcode 是否共用对象** | **是**。`spell`(`0x219`) / `0x21A` / `spell3`(`0x21B`) / `spellEx`(`0x20A`) 全部映射到同一 dispatch case `0x41` → 同一个 `FUN_00420400(*(u32*)0x4E9A70, …)` |
| **ECL 结算** | `spellEnd()` = opcode **`0x20B`** → dispatch case `0x42` → **`0x42CA5A call 0x420D20`**。**结算的是「上一张」卡**，每个攻击 sub 开头都会调 |
| **一帧一采样是否会漏** | **不会**（静态）：23 份 `.decl` 全量枚举，`setNext` + 同步 `@Sub()` 两类转移中 **card→card = 0**，每张卡后必接一个非符攻击阶段（详见 §4） |
| **轮询可行性** | ✅ **可行且推荐**（方案 B）。零新 hook、零新地址写入，最坏只会漏一张卡，不影响游戏 |
| **语义** | 只做「**卡结束**」：`+0x78` bit0 的 **1→0 下降沿 ⇒ `mSpellFlag++`**；`mSection` 换新 checkpoint ⇒ `--`（下限 0）；退出时 `mSpellFlag > 0` ⇒ 不记 `interrupt`；`mStageClear` 保留兜底 |
| **已知覆盖洞 1** | **ST2 中boss 末阶段 `MBossCard1` 没有 `spell()`**（`st02mbs.decl:180`，只有 `spellEnd()` + `setNext(…,"MBossDead")`）⇒ 该阶段不产生符卡对象、不产生下降沿；该名也不在 CSV ⇒ 无记录名，**本期不覆盖** |
| **已知覆盖洞 2** | **ST7 附属身体 `st07bs2` 的 `BossCardB10` 只有 `spellMode(1)`**（`st07bs2.decl:137`），无 `spell()` ⇒ 不产生下降沿；`st07bs` 自己的 `BossCard10`（`spellEx(118)`）才是关底最后一张卡，**已覆盖** |
| **与 th16 的结构差异** | th15 **没有 ST4/ST5 的 A/B 路线**（每关只有一条 boss 链）；th15 多一个清零点；th15 的中boss 窗口用「记录名」记忆（`mMidBossDoneRecord`）而非布尔 |
| **是否照搬 th16 的 9 处接线** | **可以照搬结构，但常量必须换**：`SPELL_OBJ_PTR` 改 `0x4e9a70`；`kSpellFlagsOff` 仍为 `0x78`（已独立验证）；其余各处的插入位置与顺序完全同构（详见 §5.4） |

---

## 1. 现有 hook 盘点（th15）

### 1.1 `thprac_th15.cpp` 全表

> `EHOOK_*` 第二参是**覆盖长度**（`instr_len`，必须覆盖完整指令），不是「至少 5 字节」的 detour 长度。
> 文件当前 3179 行；全表共 **50 处**钩子（`THMainHook` 20 + `THInGameInfo` 7 + 静态 11 + `THOverlay` 的 `PATCH_HK` 6 + `THInitHook` 6）；下表地址均来自源码字面量，未做二次改写。

**`HOOKSET_DEFINE(THMainHook)`（:2878）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th15_inf_lives` | `0x456397` | 1 | 无限命（读 `0x4E7450`） | 否 |
| `th15_everlasting_bgm` | `0x476F10` | 1 | BGM | 否 |
| `th15_param_reset` | `0x461070` | 7 | `thPracParam.Reset()` | 否 |
| `th15_prac_menu_1..4` | `0x4677BE`/`0x4677E1`/`0x467B2A`/`0x467BCB` | 5/3/7/7 | 关卡练习菜单 `State(1..4)` | 否 |
| `th15_prac_menu_enter_1`（PATCH） | `0x4678A2` | `"eb"` | 进入练习 | 否 |
| `th15_prac_menu_enter_2` | `0x467B8A` | 1 | 写 `thPracParam.stage` | 否 |
| `th15_disable_prac_menu_1`（PATCH） | `0x467D31` | `"eb3b"` | 关闭原版菜单 | 否 |
| `th15_menu_rank_fix` | `0x4527BE` | 5 | 写 `0x4E7424`/`0x4E7410` | 否（不是 th16 的符卡练习 id） |
| `th15_patch_main` | `0x43C68C` | 1 | 练习参数写入 + `THSectionPatch()` | **间接**（ECL 补丁入口，不读符卡对象） |
| `th15_bgm` | `0x43D5C1` | 2 | BGM | 否 |
| `th15_rep_save` | `0x45CC49` | 5 | 存 rep | 否 |
| `th15_rep_menu_1..3` | `0x468197`/`0x4682B0`/`0x468474` | 3/5/2 | rep 菜单 | 否 |
| `th15_update` | `0x4015FA` | 1 | **每帧派发**（含 `TH15Logger::Update()`，:3035） | **采样点** |
| `th15_player_state` | `0x454A70` | 1 | 键盘监视 | 否 |
| `th15_render` | `0x40170A` | 1 | 渲染 | 否 |

**`HOOKSET_DEFINE(THInGameInfo)`（:3092）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th15_game_start` | `0x43E6EE` | 7 | 一局开始（清 miss/bomb、`OnStageStart()`） | 否 |
| `th15_bomb_dec` | `0x41497A` | 5 | bomb 计数 + `OnBomb()` | **否**：真正的失败清位在 `0x414C20` 等函数里，**不在钩子指令上** |
| `th15_life_dec` | `0x456398` | 5 | miss 计数 + `OnMiss()` + `FastRetry` | 否 |
| `th15_lock_timer1..4` | `0x43404A`/`0x42C738`/`0x42B938`/`0x4301E8` | 10/4/2/6 | 计时器锁 | 否 |

**静态 `EHOOK_ST` / `PATCH_ST`（namespace 作用域）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th15_all_clear_bonus_1`（PATCH `"eb0b909090"`） | `0x43D99D` | — | 全关奖励 | 否 |
| `th15_all_clear_bonus_2` | `0x43DAAC` | 7 | 读 `0x4E7794 & 0x10` | 否 |
| `th15_all_clear_bonus_3` | `0x43DCB5` | 7 | 读 `0x4E7794 & 0x10` | 否 |
| `th15_master_disable1a/1b/1c`（PATCH） | `0x420346`/`0x42036B`/`0x420296` | — | master 自动 ly 关闭 | 否 |
| `th15_bossmovedown` | `0x42B261` | 5 | boss 下移（`EDI+0x3F64`） | 否（只碰 `B` 的坐标，不碰符卡） |
| `th15_chapter_set` | `0x43DD58` | 6 | 写 `0x4E73F8` | 否 |
| `th15_chapter_disable` | `0x43D0B5` | 2 | 跳章 | 否 |
| `th15_st7boss1_chapter_bonus` | `0x43DECE` | 3 | 写 `0x4E7484` | 否 |
| `th15_stars_bgm_sync` | `0x48C294` | 2 | ST6 BGM | 否 |

**`THOverlay` 的 `PATCH_HK`**

| 名称 | 地址 | 用途 | 与符卡有关？ |
| --- | --- | --- | --- |
| `mMuteki` | `0x4566A5` | 无敌 | 否 |
| `mInfBombs` | `0x414963` | 无限 bomb | 否 |
| `mInfPower` | `0x4582FA` | 无限 power | 否 |
| `mAutoBomb` | `0x454CC9` | 自动 bomb | 否 |
| `mTimeLock` | `0x41FDF5`/`0x428B5D` | 冻结 ECL 时间 | 否 |

**`HOOKSET_DEFINE(THInitHook)`（:3155）**：`0x460900`、`0x4713EC`、`0x46055F`、`0x4610C3`、`0x4617FF`、`0x47341B` —— 全部是启动/GUI 初始化，**与符卡无关**。

### 1.2 小结

- **没有任何 hook 读取 `0x4E9A70`**，也没有任何 hook 读 `obj+0x78`。
- **不存在现成的符卡对象全局地址**：`enum addrs`（:17–23）里只有 `STAGE_NUM`/`CHARACTER`/`BOMB_PTR`/`ENEMY_MANAGER_PTR`/`PLAYER_PTR`；`0x4E9A70` 在 `thprac/src/**`、`docs/**` 中 **0 命中**。
- `th15_bomb_dec`（`0x41497A`）与 `th15_life_dec`（`0x456398`）**拿不到收取/失败结果**：清 `+0x78` bit1 的指令在别的函数里（例如 miss 路径 `FUN_00414C20`：`0x414C38 and ecx,0xffffffdd` → `0x414C42 mov [edx+0x78],ecx`），钩子指令上只有计数。

---

## 2. 引擎侧：符卡对象

### 2.1 全局指针、分配与销毁

| 项 | 证据（VA / RVA） |
| --- | --- |
| **全局指针** | `0x4E9A70` / `0xE9A70`；`0x41FD4B mov dword [0x4e9a70],esi`（构造） |
| **构造** | `FUN_0041FD00`（`0x41FD00`）：`0x41FD01 push 0xbc` → `0x41FD06 call 0x49039f`（malloc）→ **`0x41FD43 call 0x49b080`（memset 0, `0xBC`）** → `0x41FD4B` 写全局 → `0x41FD51 or dword [esi],2`（对象**类型/引用字段** `+0x00`，**不是** `+0x78`） |
| 另一条构造路径 | `FUN_0041FA40`（`0x41FA40`）同样 `memset(0xBC)` 并 `0x41FA7D mov [0x4e9a70],esi` |
| **析构** | `FUN_0041FB70`（`0x41FB70`）：释放 4 个 ANM 句柄后 **`0x41FCED mov dword [0x4e9a70],0`** |
| 释放外层 | `FUN_0041FD80`（`0x41FD80`）= `FUN_0041FB70` + `free` |
| **构造调用点** | `0x43C557 call 0x41fd00`（`FUN_0043BFF0` 内的关卡初始化路径，与 th16 的 `0x42D0B7` 同型；🟡「每关一次 / 每局一次」未真机确认） |
| 对象大小 | **`0xBC`** |

> **memset 结论**：对象每次新建都是全 0，`+0x78` 从 0 开始 ⇒ **新对象第一帧 bit0 必为 0**，不会把「上一关残留的 1」当成一次结束。这一点与 th16 相同。

### 2.2 字段表（静态实测，均为 `obj + off`）

| 偏移 | 类型 | 语义 | 关键写入点 |
| --- | --- | --- | --- |
| `+0x00` | u32 | 对象类型/引用（`or 2`） | `0x41FD51` |
| `+0x0C`,`+0x10`,`+0x14`,`+0x18`,`+0x1C` | u32 | ANM / 音效句柄 | 声明期创建；`spellEnd` 释放并清 0（`0x420D46`–`0x420D7A`） |
| `+0x24` | i32 | **符卡内已过帧数**（age） | `0x42044D` 置 0；每帧 +1（`FUN_0041FDC0`） |
| `+0x28` | f32 | 帧数小数累加器 | `0x420456` |
| `+0x34` | char[0x40] | **已解密的符卡名（CP932，NUL 结尾）** | `0x420445 lea edx,[edi+0x34]` + `0x42046C` 拷贝循环 |
| `+0x74` | i32 | **spell id（含难度）** | **`0x420464 mov [edi+0x74],eax`** |
| `+0x78` | u32 | **flags（见 §2.3）** | 见 §2.4 |
| `+0x7C` | i32 | 当前符卡奖励值 | `0x420A77`；每帧递减；失败清 0 |
| `+0x80` | i32 | 奖励基数 | `0x420A7D` |
| `+0x84` | i32 | 符卡时限（帧，`spell()` 第 2 参） | **`0x420A56 mov [edi+0x84],ecx`** |
| `+0x88` | i32 | 本关符卡序号（写入历史数组用） | `FUN_00420060` |
| `+0x8C` | i32 | 帧计数器（**声明时置 1**，此后每帧 +1） | `FUN_00420400`（置 1）/ `FUN_0041FDC0`（每帧 +1） |
| `+0x90`,`+0x94`,`+0x9C`,`+0xA4` | — | 计时/得分辅助 | `FUN_00420060` |
| `+0xA8`–`+0xB0` | f32×3 | 符卡背景/位置插值 | `FUN_0041FDC0` |

### 2.3 `+0x78` 位语义（本作实测）

| bit | 值 | 语义 | 置位 | 清位 |
| --- | --- | --- | --- | --- |
| **0** | `0x1` | **符卡存活（本迭代唯一使用）** | **`0x420477/0x42047A/0x42047F`（唯一）** | **`0x420D62`（spellEnd）**、`0x420EDA`（强制清卡） |
| 1 | `0x2` | 仍可收取（capture） | 同上 `or 3` | `and ~0x22`：`0x414C38` 等（miss/bomb/超时路径，见 §2.5） |
| 2 | `0x4` | 奖励倒计时已启动 | `0x41FF03 or 4` | `0x41FF4F and ~4` |
| 3 | `0x8` | 脚本置位（`0x21E`） | `0x425E29` / `0x42CA70` | `0x420EF7 and ~0x18` |
| 4 | `0x10` | 脚本置位（`0x21F`，停音效） | `0x425E54` | `0x420EF7 and ~0x18` |
| 5 | `0x20` | 「60 帧内失败」宽限标记 | `0x4205AF` / `0x414C54` / `FUN_00414C20` | `0x420D6E` / `0x420EE6` |
| 6 | `0x40` | 奖励计时中 | `0x420083` | `0x42004A` / `0x420153` / `0x4205BA` / `0x420Dxx` |
| 7 | `0x80` | 逃逸/超时处理过 | `0x425EB9` | `0x41F7B0` / 声明期 `and 0xffffff67` |
| 8 | `0x100` | `spellMode(v)`（opcode `0x238`） | `0x425DD8`（xor 写） | 同左（可清） |
| 9 | `0x200` | 难度/模式派生位 | `0x420C37` / `0x41F7A1`（xor 写） | 同左 |

### 2.4 置位 / 清位的唯一性验证

**方法**：capstone 线性反汇编整个 `.text`（`0x401000`–`0x4BD92B`），筛出所有**内存操作数 `disp == 0x78`** 的引用（读 + 写共 **129** 条），其中真正的**写指令 47 条**（`mov/and/or/xor/add/inc/dec/movss/movdqu/…` 且 `[reg+0x78]` 为目的操作数）；这 47 条里**有 22 条作用于符卡对象**（`EDI`/`ESI`/`ECX` 来自 `0x4E9A70` 或 `FUN_00420400` 的 `this`），逐条判定如下。

| 类别 | 地址 | 指令 | 结论 |
| --- | --- | --- | --- |
| **置 bit0（唯一）** | `0x420474` | `mov eax,[edi+0x78]` | `FUN_00420400`，`EDI = *(u32*)0x4E9A70` |
| | `0x420477` | `or eax,3` | 同时置 bit1 |
| | `0x42047A` | `and eax,0xffffff67` | 清 bit3/4/7 |
| | **`0x42047F`** | **`mov [edi+0x78],eax`** | **唯一的 bit0 置位落点** |
| **清 bit0（spellEnd）** | `0x420D27` | `mov esi,[0x4e9a70]` | |
| | `0x420D2E` | `test byte [esi+0x78],1` | **幂等守卫**（无卡时空调用） |
| | **`0x420D62`** | **`and dword [esi+0x78],0xfffffffe`** | **唯一的正常清位** |
| **清 bit0（强制清卡）** | **`0x420EDA`** | **`and dword [ebx+0x78],0xfffffffe`** | `FUN_00420E90`（`EBX = ESI = 参数 = *(u32*)0x4E9A70`，见 §2.6） |
| 其它位（不影响 bit0） | `0x41F7A1`,`0x41F7B0`,`0x41FF03`,`0x41FF4F`,`0x42004A`,`0x420083`,`0x420153`,`0x42059D`,`0x4205B2`,`0x4205BA`,`0x420C37`,`0x420D6E`,`0x420EE6`,`0x420EF7`,`0x425DD8`,`0x425E29`,`0x425E54`,`0x425EB9`,`0x42CA70` | — | 只碰 bit2–bit9 / bit8 / bit9 |

> **结论**：`+0x78` bit0 的写点 **只有 3 处**，其中「置 1」**唯一**（`0x42047F`），「清 0」有 **2 处**（`0x420D62` 正常结算 + `0x420EDA` 强制清理）。
> 其余写点只碰 bit2–bit9；129 条引用中的读点是 `test/cmp` 守卫（`0x420D2E`、`0x420D7F`、`0x420E74`、`0x420E99`、`0x425DFC`…）。另有 `esp+0x78` 是栈槽、`movdqu [reg+0x78]` 是向量拷贝、`0x423C83`/`0x423CE9` 是**别的对象**（计数达到 5 后归零），与本对象无关。

### 2.5 `spell` / `0x21A` / `spell3` / `spellEx` 共用同一对象

分发链（`FUN_004291E0` = ECL 指令分发器）：

```text
0x429268  add  eax, 0xfffffed4     ; eax = opcode - 300
0x429271  cmp  eax, 0x2be
0x429276  ja   0x42dba7            ; 越界 ⇒ 无操作
0x42927c  movzx eax, byte [eax + 0x42de78]   ; ★ 字节映射表（base 0x42DE78）
0x429283  jmp  dword [eax*4 + 0x42dbcc]      ; ★ case 跳表（base 0x42DBCC）
```

实测映射（`data[0x42DE78 + (op-300)]`）：

| opcode | decl 写法 | case | 处理 |
| --- | --- | --- | --- |
| `0x20A` | `spellEx(...)` | `0x41` | → `FUN_00420400`（**不加难度**） |
| `0x219` | `spell(...)` | `0x41` | → `FUN_00420400`（`+difficulty`） |
| `0x21A` | （本作 decl 未出现） | `0x41` | `+difficulty-1` |
| `0x21B` | `spell3(...)` | `0x41` | `+difficulty-2` |
| `0x20B` | `spellEnd()` | `0x42` | → `FUN_00420D20` |
| `0x21E` | — | `0x51` | `or [obj+0x78],8`（`0x425E29`） |
| `0x21F` | — | `0x52` | `or [obj+0x78],0x10` + 停音效（`0x425E54`） |
| `0x238` | `spellMode(v)` | `0x6B` | `xor` 写 `+0x78` bit8（`0x425DD8`），**不创建符卡对象** |
| `0x202` | `setNext(...)` | `0x39` | 见 §3.5（兼注册 `BossDead` / `BossEscape`） |

⇒ **四个「声明符卡」的 opcode 全部走同一条 `0x42CA04 call 0x420400`，`ECX` 恒为 `*(u32*)0x4E9A70`：一个对象、一个槽。**

### 2.6 生命周期与「假下降沿」风险

| 场景 | 静态事实 | 对 spellFlag 的影响 |
| --- | --- | --- |
| 新建对象 | `memset(0,0xBC)` ⇒ bit0 = 0 | 新对象不会产生假下降沿（首帧 `mSpellActive=false`） |
| 对象销毁 | `0x41FCED` 把全局置 0 | `SampleSpellCard` 见到 `obj==0` 只清采样状态、**保留计数**（与 th16 同策略） |
| 换关 | 全局指针变化（若按关重建）或沿用 | 指针变化 ⇒ 三者复位；沿用 ⇒ 换关块复位 |
| **重开 / 载入回放** | **`FUN_0043DEE0`（`0x43DEE0`）在 `0x43DFAC` 调 `FUN_00420E90`（强制清 bit0）**；该函数被 `0x413AAB`（回放载入）、`0x43D612`、`0x45276C`、`0x45292E` 调用 | **会产生一次假下降沿（bit0 1→0，但并不是「一张卡打完」）**；由 `OnStageStart()` / 换关 / 指针变化复位吃掉（🟡 重试路径是否走这条链未真机确认） |
| `BossDead` 爆炸 | **7 关的 `BossDead` 全部在 `+60:` 之后才 `spellEnd()`**（`st01bs.decl:830-832`、`st02bs`…`st07bs:2774`，`BossDeadB` 亦然）；`BossEscape` 则在**第一条语句**就 `spellEnd()` | 关底 boss 死亡后有 **60 帧（约 1 s）没有下降沿**的窗口 ⇒ 与 th16 完全同构，**`mStageClear` 是必须的兜底**，不能只靠 `spellFlag` |
| 回菜单 | 对象销毁 ⇒ `obj==0` | 只清采样状态（不清计数），退出帧仍能消费 |

> `FUN_00420E90` 的调用点共 2 个：`0x43BE50`（`int3` 填充后被 Ghidra 并入 `FUN_0043BD40` 的一段独立小包装：`push ecx; mov ecx,[0x4e9a70]; call 0x420e90; pop ecx; ret`；全 `.text` 无任何 `call/jmp` 指向它，静态上不可达，🟡）与 `0x43DFAC`（`FUN_0043DEE0`）。

---

## 3. ECL 侧

### 3.1 指令格式

与 th16 完全一致（`th15decode/ecl-system.md` §1.4）：

```text
[time:u32][opcode:u16][size:u16][mask:u16][diff:u8][0xFF][pad:u32][args...]
```

### 3.2 声明处理器（case `0x41`，`0x42C978`–`0x42CA36`，内联在 `FUN_004291E0`）

```asm
0042c950  ...                       ; 名字解密循环（xor al,ah / add ah,dl / add dl,0x10）
0042c978  mov  ecx,[edx+0xc]
0042c97d  call 0x48e750             ; arg?（第 1 参）
0042c984  movsx eax, word [esi+4]   ; eax = opcode
0042c98c  sub  eax, 0x219
0042c991  je   0x42c9af             ; 0x219 → +difficulty
0042c993  dec  eax
0042c994  je   0x42c9a5             ; 0x21A → +difficulty-1
0042c996  dec  eax
0042c997  jne  0x42c9b9             ; 其它（含 0x20A）→ 不调整
0042c999  mov  eax,[0x4e7410]
0042c99e  add  eax,-2               ; 0x21B → +difficulty-2
...
0042c9f0  mov  ecx,[0x4e9a70]       ; ★ this = 符卡对象
0042ca04  call 0x420400             ; ★ FUN_00420400(id, name, time, isEx?)
0042ca0d  or   dword [edi+0x3f8c],1 ; B 的符卡标记（B = host+0x120C ⇒ host+0x5198）
0042ca14  mov  eax,[edi+0x3f74]
0042ca23  mov  [edi+0x3f80],ecx     ; = ordinal*7
```

### 3.3 结算处理器（case `0x42`，`0x42CA5A`）

```asm
0042ca5a  call 0x420d20                        ; ★ FUN_00420D20 = spellEnd
0042ca5f  and  dword [edi+0x3f8c],0xfffffffe   ; 清 B 的符卡标记
```

`FUN_00420D20`（`0x420D20`）逐段：

```asm
00420d27  mov  esi,[0x4e9a70]
00420d2e  test byte [esi+0x78],1
00420d32  je   0x420e82                  ; ★ 无卡 ⇒ 直接返回（幂等，可无脑调用）
...                                       ; 释放 ANM/音效句柄
00420d62  and  dword [esi+0x78],0xfffffffe ; ★ 清存活位
00420d6e  and  dword [esi+0x78],0xffffffdf ; 清 bit5
00420d7f  test byte [esi+0x78],2
00420d83  je   0x420e65                  ; 失败分支
...                                       ; 成功：加分 0x439d60、历史计数、SE 0x2E
00420e65  ...                             ; 失败：0x439d60(0,1)
00420e74  test byte [esi+0x78],0x80
00420e7d  call 0x476360                  ; SE 0x45
```

⇒ **`spellEnd()` 结算的是「上一张」卡**：它清理的是当前对象里的卡，而对象是在**同一个 sub** 中更靠后的位置才被 `spell()` 重新填充。每张卡的 sub 都是「先 `spellEnd()`（结算上一张）→ … → `spell()`（声明本张）」的固定形状。

### 3.4 每个攻击 sub 都调 `spellEnd()`

对 15 份 `*bs*.decl`（7 关关底 + 8 份中boss/附属，含 `st07bs2`）全量核对（脚本按 `void name()` 切分）：

- **每一个非符攻击 sub**（`Boss2..Boss10`、`MBoss2/3`）都含 `spellEnd()`，且位于该 sub 的第一屏；
- **每一张符卡 sub** 都含 `spellEnd()`，且 **`spellEnd` 出现在同一 sub 里第一个 `spell*()` 之前**；
- 唯一的例外是**整场第一个攻击 sub**（`Boss1`/`MBoss1`），它不需要结算——此时 bit0 本来就是 0，`FUN_00420D20` 的幂等守卫直接返回。

### 3.5 `setNext`（`0x202`）兼注册 `BossDead` / `BossEscape`

`case 0x39`（opcode `0x202`，`0x42C747`–`0x42C78F`）：

```asm
0042c747  test dword [edi+0x4060],0x800000   ; boss 门控位
0042c751  je   0x42c791
0042c773  push 0x4cc1c0                      ; "BossDead"
0042c778  push esi
0042c779  push eax
0042c785  call 0x42ff10                      ; 注册死亡回调
0042c78a  push 0x4cc1cc                      ; "BossEscape"
0042c78f  jmp  0x42c7e4                      ; → FUN_00430000(host, arg0, "BossEscape")
```

（`"BossDead"` 字符串在 `.rdata 0x4CC1C0`，`"BossEscape"` 在 `0x4CC1CC`；`grep '"BossEscape"' *.decl` 为空 ⇒ 它**只由引擎注册**，与 th16 一致。）

⇒ **`BossEscape` 是「当前攻击阶段的超时回调」，每个调用过 `setNext` 的攻击阶段都会注册它**，因此它在**非符期同样可能触发**（th15 的非符阶段同样有 `setNext(…,timeout,…)`）。

### 3.6 `_at` / `_Dead` 的角色

| 名字 | 角色 | 对 spellFlag 的影响 |
| --- | --- | --- |
| `BossCardN` / `MBossCardN` / `BossCardNB` | **符卡本体**（`spell*()` + 攻击主循环） | 记录名来源（CSV 精确匹配这些名字） |
| `BossCardN_at` / `_at2` / `_at3` / `_tri*` / `_Moon` | 弹幕发射子；**大量为同步 `@…()` 调用**（见 §4.2） | 会让「当前 sub 名」在卡内瞬变 ⇒ **不能用 sub 名判卡是否结束**；`+0x78` bit0 不受影响 |
| `BossCardN_Dead` / `*DeadAt` | `setDeath()` 注册的死亡动画（boss 活着时就跑） | 不在 CSV，精确匹配天然排除 |
| `Boss1..Boss10` / `MBoss1..3` | 非符攻击 | 不产生符卡事件 |
| `BossDead` / `BossDead2` / `BossEscape` / `MBossDead` / `MBossEscape` / `MBossBDead` / `MBossBEscape` / `BossDeadB` | 收尾 | 已由既有 `IsStageClearSub` / `IsMidBossEndSub` 覆盖；关底 `BossDead` 在 **`+60:` 之后**才 `spellEnd()`（`spellFlag` 在那 60 帧里无效，靠 `mStageClear`） |

---

## 4. 轮询可行性：逐份 `.decl` 核对 card→card

### 4.1 方法

对 `../exes/ecl_dump/th15/*.decl`（23 份）按 `^void name(...)` 切分 sub，提取两类转移：

1. **`setNext(a, b, c, "target")`**（opcode `0x202`，跨帧阶段切换）；
2. **同步 `@Target();`**（无 `async`，ECL 同步调用 ⇒ **当前 sub 名会变成 Target**，th16 报告 §2.4 的同一条结论）——
   这一步是**必须**的：`st06bs` 的 `Boss5→BossCard5`、`st07bs` 的 `Boss9→BossCard9` / `Boss10→BossCard10` 都**不是** `setNext`，而是同步 `@`。

判定「card」：sub 名匹配 `^M?BossCard[0-9A-Za-z]*$`（`BossCard1`、`MBossCard2`、`BossCardB10`…；`_at`/`_Dead` 自动排除）。

### 4.2 结果：**card → card 转移 = 0**

| 文件 | 符卡 sub | 符卡之后的去向 | card→card? |
| --- | --- | --- | --- |
| `st01bs` | BossCard1/2 | Boss2 / BossDead | 无 |
| `st01mbs` | MBossCard1 | MBossDead | 无 |
| `st01mbs2` | MBossCardB1 | MBossBDead | 无 |
| `st02bs` | BossCard1/2/3 | Boss2 / Boss3 / BossDead | 无 |
| `st02mbs` | MBossCard1（**无 `spell()`**） | MBossDead | 无 |
| `st03bs` | BossCard1/2/3/4 | Boss2 / Boss3 / Boss4 / BossDead | 无 |
| `st03mbs` | MBossCard1 | MBossDead | 无 |
| `st04bs` | BossCard1/2/3/4 | Boss2 / Boss3 / Boss4 / BossDead | 无 |
| `st04mbs` | （无符卡） | MBossDead | — |
| `st05bs` | BossCard1..5 | Boss2 / Boss3 / Boss4 / Boss5 / BossDead | 无 |
| `st05mbs` | （无符卡，纯对话） | 宿主 `delete()` | — |
| `st06bs` | BossCard1..7 | Boss2…Boss7 / BossDead（`Boss5/6/7→BossCard5/6/7` 为同步 `@`） | 无 |
| `st07bs` | BossCard1..10 | Boss2…Boss10 / BossDead（`Boss9→BossCard9`、`Boss10→BossCard10` 为同步 `@`） | 无 |
| `st07bs2` | BossCardB10（**无 `spell()`**） | BossDeadB | 无 |
| `st07mbs` | MBossCard1/2/3 | MBoss2 / MBoss3 / MBossDead | 无 |
| `st01..st07.decl` / `default.decl` | （无符卡，道中） | — | — |

**完整链（示例）**

```text
ST1  Boss1 → BossCard1 → Boss2 → BossCard2 → BossDead
ST2  Boss1 → BossCard1 → Boss2 → BossCard2 → Boss3 → BossCard3 → BossDead
ST6  Boss1 → BossCard1 → Boss2 → BossCard2 → Boss3 → BossCard3 → Boss4 → BossCard4
     → Boss5 →(同步 @)BossCard5 → Boss6 →(@)BossCard6 → Boss7 →(@)BossCard7 → BossDead
ST7  Boss1 → BossCard1 → … → Boss8 → BossCard8 → Boss9 →(@)BossCard9
     → Boss10 →(@)BossCard10 → BossDead
```

### 4.3 结论与对策

- 每张卡结束后，**bit0 至少保持 0 到下一个非符阶段结束**（非符阶段的计时在千帧量级）⇒ **一帧一次采样不可能漏掉下降沿**。
- 同帧「结算 + 下一张声明」在 th15 也**不可能**发生（中间必隔一个非符 sub），因此 th16 §5.2 里用于切段的 `id 变化` / `记录名变化` 兜底**不需要**；只保留最朴素的「bit0 1→0」即可。
- 唯一需要注意的是 §2.6 的**强制清卡**（重开/载入）会制造一次假下降沿 —— 用既有的三处复位吃掉。

---

## 5. 接入方案（`thprac_th15.cpp`，与 th16 的 9 处同构）—— ✅ 已落地（2026-10-20）

### 5.1 精确插入点（表中行号以**落地前** 3179 行版本为准；落地后实际行号见本节末注）

| # | 位置 | 当前行 | 改动 |
| --- | --- | --- | --- |
| 1 | `enum addrs` | **:21**（`ENEMY_MANAGER_PTR = 0x4e9a80,` 之后） | 增 `SPELL_OBJ_PTR = 0x4e9a70,` |
| 2 | 匿名 namespace 常量 | **:55**（`kHostBossGateMask` 之后） | 增 `constexpr uintptr_t kSpellFlagsOff = 0x78;` |
| 3 | `TH15Logger` private 方法声明 | **:170**（`void BuildStageMap(int32_t stageNum);` 之后） | 增 `void SampleSpellCard();` |
| 4 | `TH15Logger` 成员 | **:186**（`int32_t mBuiltStage = -1;` 之后、`bool mStageClear` 注释之前） | 增 `int32_t mSpellFlag = 0; uintptr_t mSpellObj = 0; bool mSpellActive = false;` |
| 5 | `SampleSpellCard()` 定义 | **:259**（`OnBomb()` 的 `}` 之后） | 见 §5.2 |
| 6 | `OnStageStart()` | **:246**（`mStageClear = false;` 之后、`}` 之前） | 三者复位 |
| 7 | `Update()` 顶部 | **:264**（`bool inGame = …` 之后） | 插 `SampleSpellCard();` |
| 8 | `Update()` 的 interrupt 条件 | **:267** | `if (!mStageClear && …)` → `if (mSpellFlag <= 0 && !mStageClear && …)` |
| 9 | `Update()` 段落变化块 | **:420–422** | 在 `mStageClear = false;` **之前**插 `if (mSpellFlag > 0) { --mSpellFlag; }` |
| 10 | `Update()` 换关块 | **:284–293**（`if (stageNum != mBuiltStage)`） | 在 `mStageClear = false;` 之前加三者复位 |

> 上表 **10 行 = th16 的 9 处逻辑改动**：只是把 th16 的第 7 项（「`Update()` 顶部插采样 + 改 interrupt 条件」）拆成两行（#7 / #8），**没有多出新的逻辑**。第 10 行（换关复位）对应 th16 的第 8 项。
>
> **落地后实际落点**（文件 3250 行，行号为落地时的 `thprac_th15.cpp`）：#1 `:21`、#2 `:58–65`、#3 `:181–182`、#4 `:199–206`、#5 `:286–317`、#6 `:268–270`、#7 `:323–324`、#8 `:329`、#9 `:486–491`、#10 `:354–356`。上表「当前行」保留为改动前的参考口径。

### 5.2 `SampleSpellCard()`（与 th16 逐行同构，仅常量不同）

```cpp
void TH15Logger::SampleSpellCard()
{
    // No allocation: this runs inside a VEH hook callback.
    // th15 与 th16 同引擎族：全局只有一个符卡对象，spell / 0x21A / spell3 / spellEx
    // 全部落到 FUN_00420400(*(u32*)0x4E9A70, …)；bit0 由 0x42047F 置位、
    // 由 0x420D62（spellEnd）清除。spellEnd 在每个攻击 sub 的开头执行
    // （关底 BossDead 例外，它在 +60: 之后才清），而每张卡后必接一个非符阶段（§4）
    // ⇒ 一帧一次采样不会漏。
    uintptr_t obj = GetMemContent<uintptr_t>(SPELL_OBJ_PTR);
    if (!obj) {
        // 菜单 / 关卡销毁中。只清采样状态，保留 mSpellFlag：退出帧还要用它。
        mSpellObj = 0;
        mSpellActive = false;
        return;
    }
    bool active = (GetMemContent<uint32_t>(obj + kSpellFlagsOff) & 1) != 0;
    if (obj != mSpellObj) {
        // 对象被重建（换关 / 重试 / 重开）：新对象 memset 过，bit0 从 0 开始，
        // 不能把旧对象的销毁当成一次「卡结束」。
        mSpellFlag = 0;
        mSpellObj = obj;
        mSpellActive = active;
        return;
    }
    if (mSpellActive && !active) {
        ++mSpellFlag;
    }
    mSpellActive = active;
}
```

### 5.3 「段落变化」判据与 th16 是否一致

**完全一致**。th15 的 `Update()` 里已经是：

```cpp
const char* prevSection = mSection;          // :378
...
if (mSection != prevSection && std::strcmp(mSection, prevSection) != 0) {  // :420
    mStageClear = false;                     // :421
}
```

与 th16（`:440`）逐字同形：`prevSection` 快照 + `strcmp` 判据；`"main"` 由 `GameLogRecordOfSection()` 归一成 `nullptr`。

**th15 的差异点（不影响 `--` 的判据，但影响时机）**：

- th15 有 **中boss 挂起窗口** `mMidBossActive` / `mPending` / `mMidBossDoneRecord`：中boss 存活期间，道中 checkpoint 命中会被**暂存**到 `mPending`，要等中boss 收尾 sub 才回放（:390–418）。⇒ 中boss 期间的段落变化可能**延后若干帧**，`--` 也随之延后 ⇒ `mSpellFlag` 会多滞留一会儿，只会造成 `interrupt` 的**假阴性**（可接受，与 th16 的 knownBugs #12 同类）。
- th15 的 `mMidBossDoneRecord` 是「记录名」而不是 th16 的布尔 `mMidBossDone`；这与 spellFlag 无关，**不要顺手改**。

### 5.4 是否照搬 th16 的 9 处接线

| 项 | 能否照搬 | 说明 |
| --- | --- | --- |
| 结构（9 处插入 + 顺序约束） | ✅ **照搬** | 语义与 th16 完全一致 |
| `SPELL_OBJ_PTR` | ❌ 必须换 | th16 `0x4A6DB0` → th15 **`0x4E9A70`** |
| `kSpellFlagsOff` | ⚠️ 可照搬但要写清「已独立验证」 | th15 实测同为 `+0x78`，**不是假设** |
| 复位规则 | ✅ 照搬 | 新局 / 换关 / 指针变化 |
| `mStageClear` 兜底 | ✅ 保留 | 见 §6 |

---

## 6. 覆盖性（逐关，静态）

### 6.1 关底

| 关卡 | 脚本 | 阶段链（→ 表示 `setNext`，⇒ 表示同步 `@`） | **关底最后一张攻击阶段** | 有 `spell()`？ |
| --- | --- | --- | --- | --- |
| ST1 | `st01bs` | Boss1→BossCard1→Boss2→BossCard2→BossDead | **BossCard2** | ✅ `spell(6, 2400, …)` |
| ST2 | `st02bs` | Boss1→BossCard1→Boss2→BossCard2→Boss3→BossCard3→BossDead | **BossCard3** | ✅ |
| ST3 | `st03bs` | Boss1→BossCard1→Boss2→BossCard2→Boss3→BossCard3→Boss4⇒BossCard4→BossDead | **BossCard4** | ✅ |
| ST4 | `st04bs` | Boss1→BossCard1→Boss2→BossCard2→Boss3→BossCard3→Boss4⇒BossCard4→BossDead | **BossCard4** | ✅ |
| ST5 | `st05bs` | Boss1→BossCard1→Boss2→BossCard2→Boss3→BossCard3→Boss4⇒BossCard4→Boss5⇒BossCard5→BossDead | **BossCard5** | ✅ |
| ST6 | `st06bs` | Boss1→BossCard1→…→Boss4→BossCard4→Boss5⇒BossCard5→Boss6⇒BossCard6→Boss7⇒BossCard7→BossDead | **BossCard7** | ✅ |
| ST7 | `st07bs` | Boss1→BossCard1→…→Boss8→BossCard8→Boss9⇒BossCard9→Boss10⇒BossCard10→BossDead | **BossCard10** | ✅ `spellEx(118)` |

**结论：7 关的关底最后一张攻击阶段全部是符卡** ⇒ 一张卡结束后必然有一个下降沿（只不过这个下降沿发生在 `BossDead` 的 `+60:` 之后，那 60 帧由 `mStageClear` 兜）。

> **th15 没有 ST4 / ST5 的 A/B 路线**（`st04bs` / `st05bs` 各只有一条 `BossCard1..4/5` 链，CSV 也只有一套 BOSS 行）。这一点与 th16 不同，th16 的 `BossCard1B/2B`、`BossCard3Bp`、`BossCard4B` 等在这里**不存在**。

### 6.2 中boss

| 关卡 | 脚本 | 末阶段 | 有 `spell()`？ | 备注 |
| --- | --- | --- | --- | --- |
| ST1 | `st01mbs` | MBossCard1 → MBossDead | ✅ `spell3(0, 1500, …)` | |
| ST1 | `st01mbs2` | MBossCardB1 → MBossBDead | ✅ `spell3(0, 1500, …)` | ST1 中boss 的第二条线（`MBossB1`） |
| **ST2** | `st02mbs` | MBossCard1 → MBossDead | **❌ 无 `spell()`，只有 `spellEnd()`** | **覆盖洞**：该阶段无符卡对象；也不在 CSV（ST2 只有 `MBoss1` 一行） |
| ST3 | `st03mbs` | MBossCard1 → MBossDead | ✅ `spell(22, 1320, …)`（**注意这里是 `spell` 不是 `spell3`**） | |
| ST4 | `st04mbs` | MBoss → MBossDead | — 无符卡 | CSV 无 ST4 中boss 行 |
| ST5 | `st05mbs` | `MBoss()` 读完对话后 `delete()` 自毁 | — 无符卡 | 与既有注释「dialogue-only scene」一致；靠 boss 门控位收尾 |
| ST7 | `st07mbs` | MBossCard1→MBoss2→MBossCard2→MBoss3→MBossCard3→MBossDead | ✅ `spellEx(106/107/108)` | |
| **ST7** | `st07bs2` | BossCardB10 → BossDeadB | **❌ 只有 `spellMode(1)`** | **覆盖洞**：`st07bs` 的附属身体（`BossB10` / `BossCardB10`），不产生符卡对象 |

### 6.3 `BossEscape` 是否可能在非符期触发

**会。** 证据链：

1. `BossEscape` / `BossDead` 字符串只在引擎里被引用（`0x4CC1C0` / `0x4CC1CC`），`.decl` 中 **没有** `setNext(…,"BossEscape")`。
2. 注册点在 **`setNext`（opcode `0x202`）的处理器**：`0x42C747` 判 `B[+0x4060] & 0x800000`（boss 门控），成立则 `0x42C785 call 0x42ff10(host,arg0,arg2,"BossDead")` + `0x42C78A push "BossEscape"` → `FUN_00430000`。
3. 因此**每一个调用过 `setNext` 的攻击阶段**（无论符卡还是非符）都注册了 `BossEscape` 作为该阶段的超时收尾。

⇒ 在**非符期**拖到超时 → 直接进 `BossEscape` → `spellEnd()` 是 no-op（没有活动卡）→ **没有下降沿**。这正是 `mStageClear` 必须保留的原因（与 th16 §8.2 的兜底理由完全一致）。

### 6.4 覆盖洞汇总与兜底是否足够

| # | 洞 | 影响 | `mStageClear` 是否够 |
| --- | --- | --- | --- |
| 1 | ST2 中boss 末阶段 `MBossCard1` 无 `spell()` | 该阶段结束**不产生** `spellFlag++`；该段本来也不是「打完了」 | ✅ 够（中boss 收尾后仍有 `MBossDead`，且这不是关底） |
| 2 | ST7 `st07bs2` 的 `BossCardB10` 只有 `spellMode(1)` | 不产生下降沿 | ✅ 够（该身体只是附属；`st07bs` 自己的 `BossCard10` 才是关底末卡，且 `BossDead` 在 `IsStageClearSub` 内） |
| 3 | 强制清卡（`0x420EDA`）造成**假**下降沿 | 可能吞掉一次本该记录的 `interrupt` | ✅ 概率极低且只出现在重开/载入路径；由 `OnStageStart()` / 换关 / 指针变化三种复位吃掉 |
| 4 | 中boss 段落的 `mPending` 延后 | 计数滞留，`interrupt` 假阴性 | ⚠️ 语义上可接受（与 th16 的 knownBugs #12 同类） |
| 5 | 关底 `BossDead` 的 **`+60` 帧窗口**（7 关全部如此，`spellEnd` 在 `+60:` 之后） | 那 60 帧内**没有**下降沿，但 sub 已是 `BossDead` ⇒ `mStageClear` 立刻为真 | ✅ 必须靠 `mStageClear`（`spellFlag` 在此窗口内无效） |

---

## 7. 风险与真机验证项（🟡）

> 本机 macOS 无 MSVC：**未编译、未真机**。以下 1–6 是落地后必须先过的最小集。

| # | 项 | 静态现状 | 最小验证（真机读哪里 / 期望值） |
| --- | --- | --- | --- |
| 1 | `0x4E9A70` 在标题/菜单为 0 | 静态：构造 `0x41FD4B`、析构 `0x41FCED` | 标题界面读 `*(u32*)0x4E9A70` 期望 `0`；进关后期望非 0 |
| 2 | bit0 = 符卡存活 | 静态：`0x42047F` 唯一置位、`0x420D62` 结算清位 | 打 ST1 BOSS2（`BossCard1`）：卡内 `+0x78 & 1 == 1`；进 `Boss2` 那一帧变 0 |
| 3 | **主目标**：卡结束后立刻退出不再记 `interrupt` | 静态：`mSpellFlag` 在 `Update()` 顶部 +1 | 打完 ST1 `BOSS4`（`BossCard2`）立刻 Esc → 日志该局**无** `interrupt` |
| 4 | 同帧「卡结束 + 新 checkpoint」净 0 | 静态：`++` 在顶部、`--` 在段落裁决后 | 非符期 `mSpellFlag` 应为 0（可在 IGI 临时打印） |
| 5 | 关底爆炸瞬间 Esc | 静态：`BossDead` 的 `spellEnd` 在 **`+60:` 之后**；`mStageClear` 在 `BossDead` sub 被采样到的第一帧就置真 | 爆炸瞬间 Esc → 无 `interrupt`（验证 `mStageClear` 兜底；此时 `spellFlag` 还没 +1） |
| 6 | 非符期超时 → `BossEscape` | 静态：`0x42C78A` 注册 / `0x42C747` 门控 | ST1 在 `Boss1` 拖到超时 → 无 `interrupt`（`mStageClear` 兜底） |
| 7 | 重试（`FastRetry`）是否走 `FUN_0043DEE0` ⇒ 假下降沿 | 静态：只知 `FUN_00420E90` 可达点有 2 个；`0x43BE50` 无调用者（🟡） | 在卡内重试 → 观察 `mSpellFlag` 是否短暂变 1；若变 1，确认下一次段落变化能吃掉 |
| 8 | 对象是否**每关**重建 | 静态：构造调用点 `0x43C557` 在 `FUN_0043BFF0`（疑似关卡初始化），未钉死粒度 | 换关时读 `*(u32*)0x4E9A70` 是否变化 |
| 9 | ST2 中boss 末阶段 | 静态：`MBossCard1` 无 `spell()` | 打 ST2 中boss：确认 `+0x78 & 1` 全程为 0（即无符卡事件） |
| 10 | ST7 附属身体 | 静态：`BossCardB10` 只有 `spellMode(1)` | 打 ST7 后半：确认 `BossCardB10` 期间 bit0 为 0，且不影响 `BossCard10` 的下降沿 |
| 11 | Rep / 练习一致性 | 未验证 | 存 rep 回放 ST1，读 bit0 与 `+0x74`，期望与实战一致 |
| 12 | 变体 exe（Steam / 汉化） | 未核 | 按变体重跑 `0x4E9A70` / `0x42047F` / `0x420D62` 的字节比对 |
| 13 | 60 帧宽限（仅记录，不影响本方案） | 静态：`0x414C32 cmp [edx+0x24],0x3c`，`<60` 只置 bit5 不清 bit1 | 本迭代只读 bit0，**不需要**验证；留作将来做「收取/失败」时用 |

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **建档**。th15 `spellFlag` 静态调研：① 盘点 `thprac_th15.cpp` 全部 50 处 EHOOK/PATCH —— **无任何符卡状态读写，`0x4E9A70` 全仓库 0 引用**；② 定案符卡对象 **`0x4E9A70` / `0xBC` B**（构造 `0x41FD00`、`memset 0xBC` @`0x41FD43`、析构 `0x41FB70`→`0x41FCED`），字段表与 th16 同构（`+0x24` age、`+0x34` 名字、`+0x74` id、**`+0x78` flags**、`+0x7C/+0x80/+0x84` 奖励与时限）；③ **`+0x78` bit0 置位唯一（`0x42047F`）**、清位 **2 处**（`0x420D62` = `spellEnd`，`0x420EDA` = 强制清卡），全 `.text` 129 条 `+0x78` 引用（其中写指令 47 条、符卡对象 22 条）逐条核对；④ ECL 侧定案 `spell=0x219`/`0x21A`/`spell3=0x21B`/`spellEx=0x20A` 共用同一对象（字节映射表 `0x42DE78`、跳表 `0x42DBCC`、case `0x41`），`spellEnd=0x20B` → `0x42CA5A call 0x420D20`；⑤ 23 份 `.decl` 全量枚举（`setNext` + 同步 `@Sub()`）：**card→card = 0** ⇒ 一帧一采样不会漏；⑥ 逐关覆盖性：7 关关底末阶段全为符卡，且 7 关的 `BossDead` 均在 `+60:` 后才 `spellEnd`（与 th16 同构）；中boss 两处洞（ST2 `MBossCard1` 无 `spell()`、ST7 `st07bs2` `BossCardB10` 仅 `spellMode`）；⑦ 给出 `thprac_th15.cpp` 的 10 处精确插入点（行号）与 `SampleSpellCard()` 草图，确认「段落变化」判据与 th16 逐字同构；⑧ 保留 `mStageClear` 兜底（`BossDead` 的 `+60` 帧窗口 + 非符期 `BossEscape`，后者由 `setNext`(`0x202`) 的 `0x42C78A` 注册）。**全部为静态结论，未编译、未真机（🟡）**。 |
| 2026-10-20 | **代码落地（方案 A）**。按 §5.1 在 `thprac/src/thprac/thprac_th15.cpp` 完成 9 处逻辑改动 / 10 个落点：① `enum addrs` 增 `SPELL_OBJ_PTR = 0x4e9a70`（`:21`）；② 匿名 namespace 增 `kSpellFlagsOff = 0x78`（`:58–65`，注明本作独立验证 + 第二清位点 `0x420EDA`）；③ private 增 `SampleSpellCard()` 声明（`:181–182`）与 `mSpellFlag` / `mSpellObj` / `mSpellActive` 成员（`:199–206`）；④ `SampleSpellCard()` 定义（`:286–317`，与 th16 逐行同构，对象为空时**不清**计数）；⑤ `OnStageStart()` 三者复位（`:268–270`）；⑥ `Update()` 顶部在 `inGame` 之后、interrupt 判定**之前**调用采样（`:323–324`）并把条件改为 `mSpellFlag <= 0 && !mStageClear && …`（`:329`）；⑦ 换关块三者复位（`:354–356`）；⑧ 段落变化块在 `mStageClear = false` **之前** `--mSpellFlag`（`:486–491`）。`IsStageClearSub()` / `mStageClear` / `sawStageClear` **保留**为兜底；未新增 EHOOK、未改共享层。静态扫描：`git -c core.whitespace=cr-at-eol diff --check` 通过（本文件为 CRLF，默认配置下 CR 会被报成 trailing whitespace）；括号平衡检查（剥离注释/字符串）通过；新标识符声明/定义各一处。**未编译、未真机（🟡）**，真机验证项见 §7。 |

# TH17 符卡结束（spellFlag）— 静态逆向报告

> 上位文档：`docs/req_logger.md`（口径 12 / 15）、`docs/logger/logger_re_disasm_overview.md`（术语与地址口径）、`docs/logger/logger_impl_report.md` §4.4.2（th16 的 9 处接线）
> 模板（必读）：`docs/th16/th16_spell_flag_report.md`（§2 ECL 侧、§3 符卡对象与 bit0、§4 轮询可行性、§8 最终落地）
> 需求来源：用户 2026-10-20 需求——把 th16 的 `spellFlag` 推广到 **th17**：只关心「一张符卡结束」事件（**不区分收取/失败**）；`spellFlag++` 后由 `mSection` 换新 checkpoint 时 `--`（下限 0）；退出时 `spellFlag > 0` 不记 `interrupt`；`mStageClear` **保留为兜底**（方案 A，不删除）
> 同作既有逆向：`docs/th17/th17_ecl_re_report.md`（Q1–Q9）、`docs/th17/th17_boss_phase_report.md`、`docs/logger/th17_checkpoints.csv`
> 二进制指纹：`../exes/th17.exe`，**747520 B**，PE `TimeDateStamp = 1565429804`（`0x5D4E902C`），`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；`.text` = VA `0x401000`–`0x499EBA`，RVA `0x1000`–`0x98EBA`，VirtualSize `0x98EBA`）
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` 只读反汇编；按“函数起始（`CC CC` 填充）+ 线性解码”落表，全 `.text` 按**指令字节模式**穷举对 `+0x78` 的写点；离线 ECL 解码（SCPT 头 → 子表 `0x34` → 名称区 → 指令流，指令 = `[time:u32][op:u16][size:u16]…args@+0x10`，名字 XOR 解密 `ah=0x77 / dl=+0x10`）并与 `../exes/ecl_dump/th17/*.decl`（22 份）逐行对照
> 素材：`../exes/ecl/th17/*.ecl`（22 份）、`../exes/ecl_dump/th17/*.decl`（22 份）
> 状态：**静态定案（🟡 未真机）** + **代码已落地（方案 A，2026-10-20，未编译 / 未真机）**。本机 macOS，不可编译、不可真机；文中未真机验证的读数一律标 🟡
> 地址口径：正文以 **VA** 为主，同时标注 **RVA = VA − 0x400000**
> 改动范围：2026-10-20 仅把 §5 的 9 处接线落到 `thprac/src/thprac/thprac_th17.cpp`（第 1 处无需改动：复用既有 `SPELLCARD_PTR`），并回填本文；未改动其它文件。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 现有符卡 hook | **无**。`thprac_th17.cpp` 的全部 EHOOK/PATCH 里没有一处读写符卡状态；唯一相关的是 `IsInKeikiFinal()`（`:2833`）**只读** `SPELLCARD_PTR`（`0x4B7690`）的 `+0x1C` 与 `+0x74`（详见 §1.2） |
| `+0x1C` 能不能当 bit0 | **不能**。`+0x1C` 是**符卡 ANM 句柄**（声明期 `0x41AF68` 创建、结算期 `0x41B1A5` 停用并置 0），声明过程中还会因查表失败被写 0（`0x41AFEC` / `0x41AFFF`）⇒ 卡内也可能为 0。`IsInKeikiFinal()` 只是“通常非 0”的近似 |
| `+0x74` 是不是 spell id | **是**。声明函数 `0x41AD3D mov [edi+0x74], ebx`，`ebx` = 处理器算出的 `idBase + difficulty` |
| 符卡对象 | 全局指针 **`0x4B7690`**（RVA `0xB7690`），`malloc(0xC0)`，对象大小 **`0xC0`**（th16 是 `0xBC`） |
| 「符卡存活」标志 | **`*(u32*)(obj + 0x78) & 1`**（bit0）。**置位唯一**：`0x41AD58`（声明函数 `or eax,3` 后写回）；**清位唯一**：`0x41B181`（结算函数 `and [esi+0x78],~1`） |
| 收取位 | 同一 flags 的 **bit1（`0x2`）**：声明时置 1；**7 个失败点**用 `and eax/ecx,0xFFFFFFDD` 清掉（miss / bomb / 超时）⇒ 与 th16 语义一致，但本迭代**不使用** |
| ECL 声明符卡 | opcode **`0x0219`**（`spell`）/ **`0x020A`**（`spellEx`）→ 同一处理器 **`0x42513E`** → 声明函数 **`0x41ACF0`**；`0x021A` / `0x021B` / `0x0210` 也映射到同一处理器，但 th17 脚本未使用 |
| ECL 结算符卡 | opcode **`0x020B`**（`spellEnd()`）→ 处理器 **`0x425256`** → 结算函数 **`0x41B140`**。**结算的是“上一张”卡**：每个**后续**攻击 sub 的**开头**都会调一次（首段 `Boss1` / `MBoss` 除外）；无活动卡时是 no-op（`0x41B14D` 判 bit0） |
| `spell` / `spellEx` 是否共用对象 | **是**。四个家族 opcode 的跳表项全部指向 `0x42513E`，最终都走 `mov ecx,[0x4B7690]` + `call 0x41ACF0` ⇒ 单例对象、单一槽位 |
| 轮询可行性 | **可行**。22 份 `.decl` 全量核对：**card → card 直接转移 = 0**；每张卡结束后必接一个非符（1800–7200 帧）或 `BossDead`/`MBossDead` ⇒ 一帧一次采样不会漏 |
| 接入方案 | **照搬 th16 的 9 处接线**（常量可复用既有 `SPELLCARD_PTR`，只需新增 1 个偏移常量），零新 hook、共享层零改动（§5） |
| 落地状态 | **已落地**（方案 A，2026-10-20）：`thprac/src/thprac/thprac_th17.cpp` 按 §5 完成 8 处实质改动（第 1 处复用既有 `SPELLCARD_PTR`，未新增常量）；`IsStageClearSub()` / `mStageClear` / `sawStageClear` 全部保留。**未编译、未真机（🟡）** |
| 覆盖性 | 7 关关底**最后一张攻击阶段全部是 `spell()`**；ST1–ST6 中boss **没有任何符卡**（单相位），ST7 中boss 3 张 `spellEx`（末张 `MBossCard3`）⇒ 全部覆盖 |
| 已知洞 | ST2 `Boss3`、ST5 `Boss4`、ST6 `Boss5/6/7`、ST7 `Boss9/10` **不在 CSV 白名单** ⇒ 卡结束后 `mSection` 不切换，计数滞留，期间 `interrupt` 为**假阴性**（与 th16 同类副作用，登记建议见 §6.3） |
| 保底 | `mStageClear` **必须保留**：关底 `BossDead` 的 `spellEnd()` 在 `+60:` 帧才执行（死亡后 60 帧内 bit0 仍为 1、无下降沿），且 `BossEscape` 可能由引擎在非符期触发（`spellEnd` 为 no-op，无下降沿） |

---

## 1. 现有 hook 盘点（th17）

### 1.1 `thprac_th17.cpp` 全表

> 文件 `thprac/src/thprac/thprac_th17.cpp`（3250 行）。`EHOOK_*` 第二参是**覆盖长度**（`instr_len`），不是“至少 5 字节”的 detour 长度。

**静态 `EHOOK_ST` / `PATCH_ST`（namespace 作用域）**

| 名称 | VA | len | 用途 | 读写符卡状态？ |
| --- | --- | --- | --- | --- |
| `th17_force_goast_angle`（:1429） | `0x4105C9` | 7 | 强制灵体角度（一次性 `Disable()`） | 否 |
| `th17_all_clear_bonus_1`（:1433） | `0x43229F` | patch | 全关奖励 | 否 |
| `th17_all_clear_bonus_2`（:1434） | `0x4323D5` | 7 | 全关奖励（读 `0x4B59C8 & 0x10`） | 否 |
| `th17_all_clear_bonus_3`（:1443） | `0x43252E` | 7 | 同上 | 否 |
| `th17_goast_disable`（:1470） | `0x4347AF` | patch | 禁止灵体 | 否 |
| `th17_bossmovedown`（:1473） | `0x423606` | 5 | boss 下移 | 否 |
| `th17_master_disable1a..1c`（:1465-1467） | `0x41AC42` / `0x41AC87` / `0x41AB96` | patch | master 自动 ly | 否 |

**`HOOKSET_DEFINE(THMainHook)`（:2873）**

| 名称 | VA | len | 用途 | 读写符卡状态？ |
| --- | --- | --- | --- | --- |
| `th17_inf_lives` | `0x44921A` | 1 | 无限命 | 否（同函数后面的 `0x449390` 才是清 bit1 点，hook 本身不读） |
| `th17_keiki_phasetrack` | `0x47AD70` | 1 | Keiki 终符 4 相位计时 | **只读**：`IsInKeikiFinal()` 读 `+0x1C` / `+0x74` |
| `th17_window_mousedown` | `0x46198B` | 1 | 鼠标生成灵体 | 否 |
| `th17_goast_bugfix` / `_repfix` | `0x430080` / `0x430C97` | 1 / 2 | 灵体 bug | 否 |
| `th17_everlasting_bgm` | `0x4662E0` | 1 | BGM | 否 |
| `th17_param_reset` | `0x451ED0` | 7 | 参数复位 | 否 |
| `th17_prac_menu_1..4` | `0x456970` / `0x456993` / `0x456BC7` / `0x456C76` | 5/3/6/7 | 练习菜单 | 否 |
| `th17_prac_menu_enter_1/2` | `0x456A59`(patch) / `0x456C2B` | -/1 | 进入练习 | 否 |
| `th17_disable_prac_menu_1` | `0x456DE4` | patch | 禁用原版练习菜单 | 否 |
| `th17_menu_rank_fix` | `0x445ED1` | 5 | 难度回写 | 否 |
| `th17_patch_main` | `0x430CB3` | 1 | 练习参数写入 / `THPatchSP` | **间接**（写 ECL 缓冲，不碰符卡对象） |
| `th17_bgm` / `th17_rep_save` | `0x431D97` / `0x44EEAA` | 2 / 5 | BGM / rep | 否 |
| `th17_restart` | `0x4451DC` | 2 | 快速重开 | 否 |
| `th17_rep_menu_1..3` | `0x4570F6` / `0x457216` / `0x4573DA` | 3/5/2 | rep 菜单 | 否 |
| `th17_sp_menu_1/2` | `0x45C554` / `0x45C4C4` | 3 / 6 | 符卡练习菜单 `THGuiSP::State()` | **间接**：读 `0x4B77F0` 的练习列表/游标（不是符卡状态） |
| `th17_update` | `0x4013B5` | 1 | 每帧派发（`TH17Logger::Update()`，:3086） | **采样点** |
| `th17_player_state` / `th17_render` | `0x447970` / `0x4014D0` | 1 / 1 | 键盘监视 / 渲染 | 否 |

**`HOOKSET_DEFINE(THInGameInfo)`（:3118）**

| 名称 | VA | len | 用途 | 读写符卡状态？ |
| --- | --- | --- | --- | --- |
| `th17_game_start` | `0x4302E6` | 6 | 开局（清 IGI 计数 + `OnStageStart()`） | 否（但它是 `mSpellFlag` 的复位点） |
| `th17_roar_break` / `th17_roar` | `0x40F880` / `0x40FC8A` | 10 / 7 | 咆哮计数 / `OnBreak()` | 否 |
| `th17_life_dec` | `0x44921B` | 6 | miss 计数 + `OnMiss()` | **否**（清 bit1 在**同函数更后面** `0x449390`，hook 覆盖不到） |
| `th17_bomb_dec` | `0x411CAB` | 6 | bomb 计数 + `OnBomb()` | **否**（真正的清位在 `0x412F2B` / `0x413DBE` / `0x40F8E4` 等） |
| `th17_lock_timer1..4` | `0x42A3CA` / `0x428C10` / `0x423E16` / `0x41F7C4` | 10/1/6/6 | 计时器锁 | 否 |

**`HOOKSET_DEFINE(THInitHook)`（:3229）**：`0x45167C` / `0x46055C` / `0x4511AF` / `0x451F32` / `0x452502` / `0x462E17` —— 启动与 GUI 初始化，全部与符卡无关。

**`THOverlay` 热键（:1133-1191）**：`0x44956A`（无敌）、`0x411C96`（无限 bomb）、`0x447B84`（无限 power）、`0x447C20`（自动 bomb）、`0x40EF6A`/`0x40EF37`（无限咆哮）、`0x40FDD4`（无 hyper）、`0x41A8CF`/`0x420A1E`/`0x47A712`（时停）—— 全部与符卡无关。

> **小结：表内没有任何一处写符卡对象 `0x4B7690` 的 `+0x78`；只有 `th17_keiki_phasetrack` 通过 `IsInKeikiFinal()` 只读 `+0x1C` / `+0x74`。**

### 1.2 `SPELLCARD_PTR` / `IsInKeikiFinal()` 复核（**关键**）

现状（`thprac_th17.cpp:85`、`:2833-2841`、`:2885-2899`）：

```cpp
SPELLCARD_PTR = 0x4b7690,                      // :85
bool IsInKeikiFinal()                          // :2833
{
    const uintptr_t spellCard = GetMemContent(SPELLCARD_PTR);
    if (!spellCard || !GetMemContent(spellCard + 0x1c))
        return false; // active spell          // ← 注释写的是 “active”，但 +0x1C 不是
    const uint32_t spellID = GetMemContent(spellCard + 0x74);
    return spellID >= 84 && spellID <= 87;
}
```

**`+0x1C` 的真实语义 = 符卡 ANM 句柄**（不是布尔存活位）：

| 证据 | 地址 | 指令 |
| --- | --- | --- |
| 声明期创建（从 ANM 资源 `[0x4B7698]` 取实例，存 `+0x1C`） | `0x41AF4D`–`0x41AF68` | `mov eax,[0x4b7698]` … `call 0x405A70` / `mov [edi+0x1c],eax` |
| 声明期用它取 ANM、失败则**把 `+0x1C` 写 0** | `0x41AFDE`–`0x41AFF1` / `0x41AFFF`、`0x41B018`–`0x41B037` | `push [edi+0x1c]` / `call 0x476470` / `mov [edi+0x1c],eax` |
| 每帧用 `+0x1C` 同步符卡贴图位置 | `0x41AADB`–`0x41AAFD` | `push [edi+0x1c]` / `call 0x476470` / `movq [eax+0x5e4],xmm0` |
| 结算期停用并置 0 | `0x41B19D`–`0x41B1A5` | `push [esi+0x1c]` / `call 0x476620` / `mov dword [esi+0x1c],0` |
| 对象析构期也停用 | `0x41A78E`–`0x41A7B9` | `push [esi+0x10/0x14/0x18]` / `call 0x476620` |

⇒ **`+0x1C != 0` 只是“通常与卡同生命周期”的近似**：ANM 实例创建失败时卡内也会是 0；反过来 `+0x1C` 在 `+0x78` bit0 清零后被立刻清 0，两者大体同步但**不是同一个判据**。

`+0x74` 复核：**确实是 spell id**（`0x41AD3D mov [edi+0x74],ebx`，`ebx` = 处理器算出的 id），与 `IsInKeikiFinal` 的 `84..87` 区间一致（ST6 `BossCard7` 的 `idBase = 84`，E/N/H/L → 84/85/86/87，见 §3.5）。

**结论：**
1. `IsInKeikiFinal()` 的 `+0x1C` 判断**与 bit0 不是同一判据**（ANM 句柄 vs 存活位）；本迭代**不修改它**（只读任务），但**不要**把它当成 bit0 的替代。
2. `spellFlag` 必须读 **`+0x78` bit0**。
3. `SPELLCARD_PTR = 0x4B7690` **可直接复用**（它就是符卡对象全局；见 §2.1 的构造点 `0x430AD0`），无需新增地址常量。

### 1.3 为什么“挂现有钩子”拿不到符卡状态

| 需求 | 现有钩子 | 结论 |
| --- | --- | --- |
| 符卡存活 | 无 | 只能新增每帧采样（零新 hook，复用 `th17_update`） |
| 收取/失败 | `0x44921B`（miss）/ `0x411CAB`（bomb） | **拿不到**：清 bit1 在 `0x449390` / `0x449513` / `0x412F2B` / `0x413DBE` / `0x40F8E4` / `0x411E30` / `0x41F87F`，都不在钩子覆盖的指令上 |

> 本迭代只要「卡结束」，因此**不需要**这些钩子：bit0 的 1→0 下降沿即事件。

---

## 2. 引擎侧：符卡对象

### 2.1 全局指针、对象大小、生命周期

| 项 | 证据（VA / RVA） |
| --- | --- |
| 全局指针 | **`0x4B7690`** / `0xB7690`（位于 `.data` 的 BSS 尾，加载时 = 0） |
| 构造（唯一） | 关卡初始化函数内联：`0x430A84 push 0xC0` → `0x430A89 call 0x47B250`（分配）→ `0x430A8E mov edi,eax` → 逐字段清零 + `0x430AC5 call 0x47CE60`（`memset(edi,0,0xC0)`）→ **`0x430AD0 mov [0x4B7690],edi`** → `0x430AD6 push 0x41AB30`（vtable）→ `0x430ADB call 0x401570` |
| 对象大小 | **`0xC0`**（`0x430A84` 与析构 `0x41A856` 的 `push 0xC0` 一致） |
| vtable | `0x41AB30`（`jmp 0x41A890` = 每帧 update；见 §2.2） |
| 析构（唯一） | **`0x41A760`**：停 `+0x10/+0x14/+0x18` → 销毁 `+4` / `+8` 两个子对象 → **`0x41A85C mov dword [0x4B7690],0`** → `0x41A866 call 0x47B280`（free，`0xC0`） |
| 析构调用点（唯一） | `0x43110A`（在关卡拆除路径 `0x430D70` 内，`call 0x411B10` 之后） |
| 换关 / 重试 | 析构 → 重新 `malloc` ⇒ **全局指针变化**（极端情况下 malloc 复用同一地址，见 §2.5） |
| 菜单 / 标题 | 指针为 0（BSS 初值 / 析构后） |

> **与 th16 的差别：对象大小 `0xBC` → `0xC0`，全局 `0x4A6DB0` → `0x4B7690`；`+0x78` flags 偏移与位语义完全一致。**

### 2.2 字段表（静态实测）

| 偏移 | 类型 | 语义 | 写入 / 读取点 |
| --- | --- | --- | --- |
| `+0x0C` | u32 | ANM/子对象句柄 | 声明期 `0x41B0D6`；结算期 `0x41B185`–`0x41B18D` 停用并置 0 |
| `+0x10` `+0x14` `+0x18` | u32 | 特效/音效句柄（3 个） | 声明期 `0x41AEE3` / `0x41AF00` / `0x41AF1E`–`0x41AF2D`；结算期 `0x41B165`–`0x41B17C` 停用 |
| `+0x1C` | u32 | **符卡 ANM 句柄** | 声明期 `0x41AF68`（见 §1.2）；结算期 `0x41B1A5` 置 0 |
| `+0x20` | i32 | 上一帧年龄快照 | `0x41A936 mov [edi+0x20],[edi+0x24]`；声明期置 `-1`（`0x41AD36`） |
| `+0x24` | i32 | **符卡内已过帧数（age）** | 声明期 `0x41AD26` 清 0；每帧 `0x41A979 mov [edi+0x24],ecx` |
| `+0x28` | f32 | 帧数小数累加器 | 声明期 `0x41AD2F`；每帧 `0x41A974` |
| `+0x2C` / `+0x30` | u32 | 初始化门（`+0x30` bit0） | `0x41AD08`–`0x41AD19` |
| `+0x34` | char[0x40] | **已解密的符卡名（CP932，NUL 结尾）** | 声明期 `0x41AD40`–`0x41AD4B` 的拷贝循环（目标 `edi+0x34`） |
| `+0x74` | i32 | **spell id（含难度）** | 声明期 `0x41AD3D mov [edi+0x74],ebx` |
| `+0x78` | u32 | **flags（见 §2.3）** | 声明期 `0x41AD58`；结算期 `0x41B181` / `0x41B194` |
| `+0x7C` | i32 | 当前符卡奖励值 | 声明期 `0x41B083`；每帧递减 `0x41A918`；失败清 0 |
| `+0x80` | i32 | 奖励基数 | 声明期 `0x41B08F` |
| `+0x84` | i32 | 符卡时限（帧，`spell()` 第 2 参） | 声明期 `0x41B057 mov [edi+0x84],esi` |
| `+0x8C` | i32 | 帧计数器 | 声明期 `0x41AEB4` 置 1；每帧 `0x41A89F inc [edi+0x8c]` |
| `+0xAC` `+0xB0` `+0xB4` | f32×3 | 最近一次 boss 坐标（ANM 跟随用） | `0x41AF92`–`0x41AFB3`；每帧插值 `0x41AA69`–`0x41AAD3` |

### 2.3 `+0x78` flags 位语义（**核心**）

| bit | 值 | 语义 | 置位（VA） | 清位（VA） |
| --- | --- | --- | --- | --- |
| 0 | `0x1` | **符卡存活** | **`0x41AD50 or eax,3` → `0x41AD58 mov [edi+0x78],eax`（唯一）** | **`0x41B181 and dword [esi+0x78],0xFFFFFFFE`（唯一）** |
| 1 | `0x2` | 仍可收取（capture） | 同 `or 3`（`0x41AD50`） | 7 个失败点 `and 0xFFFFFFDD`（见下） |
| 2 | `0x4` | 奖励倒计时已启动 | `0x41A9E1 or [edi+0x78],4` | `0x41AA31 and [edi+0x78],~4` |
| 3 | `0x8` | 脚本 `0x21E` 置位 | `0x41DFD9 or [eax+0x78],8` | 声明期 `and ~0x98`（`0x41AD53`） |
| 4 | `0x10` | 脚本 `0x21F` 置位（并停 `+0x1C` 音效） | `0x41DFFB or [esi+0x78],0x10` | 声明期 `and ~0x98` |
| 5 | `0x20` | “60 帧内失败”宽限标记 | 7 个失败点的 `or 0x20` 分支 | `0x41B194 and ~0x20`；`0x41AB1E and ~0x20` |
| 6 | `0x40` | 保留/未用 | — | 声明期 `0x41AEBE and ~0x40` |
| 7 | `0x80` | 超时/逃逸已处理 | `0x41F867 or [edx+0x78],0x80` | 声明期 `and ~0x98` |
| 8 | `0x100` | 由脚本 opcode `0x237` 切换（`0x41DFA0`：`flags ^= ((arg<<8) & 0x100)`；th17 脚本未使用） | `0x41DFB8` | 同左（arg=0 时清） |
| 9 | `0x200` | 由 BGM/难度表派生 | `0x41B0E3`–`0x41B0EB xor [edi+0x78],(table<<9&0x200)` | — |

**清 bit1 的 7 个失败点**（全部是同一段被复制到多处的形状：`if (!(flags&1)) skip; if (age < 0x3C) → 只置 bit5; else { flags &= ~0x22; [obj+0x7C]=0 }`）：

| # | VA / RVA | 所在函数 | 同处还会写 |
| --- | --- | --- | --- |
| 1 | `0x40F8E4` / `0xF8E4` | `0x40F6D0` | 玩家状态 `+0x18E88` |
| 2 | `0x411E30` / `0x11E30` | `0x411D90` | 玩家死亡状态 |
| 3 | `0x412F2B` / `0x12F2B` | `0x412EE0` | **`0x412F46 inc [mgr+0x40]`（`$BOMB_COUNT++`）** |
| 4 | `0x413DBE` / `0x13DBE` | `0x413D60` | **`0x413E1E inc [mgr+0x40]`（`$BOMB_COUNT++`）** |
| 5 | `0x449390` / `0x49390` | `0x4491B0` | **`0x4493AB inc [mgr+0x3C]`（`$MISS_COUNT++`）** |
| 6 | `0x449513` / `0x49513` | `0x449420` | 玩家死亡状态 |
| 7 | `0x41F87F` / `0x1F87F` | `0x41F5E0`（超时） | `0x41F84C or [host+0x528C],0x1000000`（`$TIMEOUT=1`）、`[mgr+0x44]=0`（`$CAPTURE=0`） |

> `mgr = *(u32*)0x4B76A0`；`+0x3C` = `$MISS_COUNT`、`+0x40` = `$BOMB_COUNT`、`+0x44` = `$CAPTURE`（偏移与 th16 相同）。
> 注意 **`$TIMEOUT` 是 per-host `+0x528C` 的 bit24**（th16 是 `+0x526C`），宿主结构整体偏移 +0x20。

### 2.4 bit0 置位 / 清位唯一性 —— 全 `.text` 扫描

**方法**：① 枚举 `.text` 内所有 `0x4B7690` 的立即数引用（**35 处：33 读 + 2 写**，形态为 `A1` / `8B 0D|15|35|3D` / `89 3D` / `C7 05`，全部落在指令边界）；② 再对**所有** `[reg+0x78]` 形态的写指令做字节模式穷举（`83 4?/6? 78 ib`、`81 …`、`80 …`、`C7 4? 78 id`、`89/88/31/21/09 4? 78`、`FF 4? 78` 等），并逐条 capstone 校验；③ 按“是否作用于符卡对象”过滤。

结论：

| 判据 | 结论 | 证据 |
| --- | --- | --- |
| **bit0 置位** | **唯一** `0x41AD58` | `0x41AD4D mov eax,[edi+0x78]` / `0x41AD50 or eax,3` / `0x41AD53 and eax,0xFFFFFF67` / `0x41AD58 mov [edi+0x78],eax`；该函数 `0x41ACF0` 的**唯一调用点**是 `0x425201`（符卡声明处理器） |
| **bit0 清位** | **唯一** `0x41B181` | `0x41B181 and dword [esi+0x78],0xFFFFFFFE`；该函数 `0x41B140` 的**唯一调用点**是 `0x42525C`（`spellEnd` 处理器） |
| 其余 `+0x78` 写点 | 全部**保留 bit0** | 只做 `or 4/8/0x10/0x20/0x80`、`and ~4/~0x20/~0x40`、`xor 0x100/0x200`，或整值 `and 0xFFFFFFDD`（只清 bit1/bit5） |

> th17 的声明函数 `0x41ACF0` 与结算函数 `0x41B140` **各自只有一个调用点**（`0x425201` / `0x42525C`），这是 bit0 “只由 `spell()` 置、只由 `spellEnd()` 清”的最强静态证据。
> 逐帧的 vtable update `0x41A890` **不写 bit0**（只 `inc +0x8C`、算奖励、清 bit5），因此 bit0 是纯“声明—结算”电平。

### 2.5 生命周期与“假下降沿”风险

| 场景 | 对象行为 | 对采样的影响 |
| --- | --- | --- |
| 标题 / 菜单 | `*(u32*)0x4B7690 == 0` | 采样器判空 ⇒ 只清 `mSpellObj/mSpellActive`，**保留 `mSpellFlag`**（退出帧还要用） |
| 进关 / 换关 / 重试 | 析构（`0x41A760` 置全局 0）→ 重新 `malloc(0xC0)` + `memset` ⇒ **指针变化** | 指针变化 ⇒ 三者复位（不计 `++`） |
| 对象被 `memset` 后**复用同一地址** | 理论上 `malloc` 可能返回同一地址：bit0 由 1 变 0，而 `obj == mSpellObj` | **假下降沿风险**。已由 `th17_game_start`（`0x4302E6`，在 `0x430A89` 分配**之前**执行）里的 `OnStageStart()` 复位 `mSpellObj = 0` 兜住；换关块也会复位（在采样之后执行） |
| 卡进行中被析构（理论上：卡内退出/强退） | 全局先置 0 | 判空 ⇒ 不清计数（与设计一致） |
| 重开时的 `th17_restart`（`0x4451DC`） | 走同一套关卡重入路径 | 🟡 待真机确认会触发 `0x430080` 的初始化（从而 `OnStageStart`） |

---

## 3. ECL 侧：`spell` / `spellEnd`

### 3.1 opcode 与处理器（**含分发表验证**）

扩展 opcode 分发（`0x421183`–`0x4211C7`）：

```asm
0042118b  0fb74204        movzx eax, word [edx+4]        ; edx = 当前指令 → opcode
0042119b  05d4feffff      add   eax, 0xFFFFFED4          ; eax = opcode - 0x12C
004211a6  3dbd020000      cmp   eax, 0x2BD
004211ab  0f8741 540000   ja    0x4265F2                ; 越界 → 默认
004211b1  0fb680d0684200  movzx eax, byte [eax+0x4268D0] ; 字节映射表
004211c7  ff24851c664200  jmp   dword [eax*4+0x42661C]  ; 跳表
```

按该映射逐项求值（静态计算）：

| opcode | decl 写法 | 映射索引 | 处理器 VA / RVA | 语义 |
| --- | --- | --- | --- | --- |
| `0x0219` | `spell(idBase, time, bgm, name)` | `0x43` | **`0x42513E` / `0x2513E`** | 关底/中boss 符卡；`id = idBase + difficulty` |
| `0x021A` | 未使用 | `0x43` | 同上 | `id = idBase − 1 + difficulty`（处理器内 `sub eax,0x219` 分支） |
| `0x021B` | 未使用 | `0x43` | 同上 | `id = idBase − 2 + difficulty` |
| `0x020A` | `spellEx(idBase, time, bgm, name)` | `0x43` | 同上 | Extra / ST7 中boss；**不加难度** |
| `0x0210` | 未使用 | `0x43` | 同上 | 同 `spellEx` 语义（无难度调整） |
| `0x020B` | `spellEnd()` | `0x44` | **`0x425256` / `0x25256`** | **结算上一张卡**（见 §3.3） |
| `0x021E` | `—`（th17 仅 `st07bs` 的 `BossCard9` 用 1 次） | `0x53` | `0x425273` / `0x25273` | `or [obj+0x78],8`（`0x41DFD0`），只置 bit3，**不影响 bit0** |
| `0x021F` | 未使用 | `0x54` | `0x425283` / `0x25283` | `or [obj+0x78],0x10` + 停 `+0x1C` 音效（`0x41DFF0`） |
| `0x0237` | 未使用 | `—` | `0x425293` / `0x25293` | `call 0x41DFA0`：按参数置/清 **bit8（`0x100`）** |
| `0x0238` | 未使用（th17 脚本无 `spellMode`） | `0x6D` | `0x42523C` / `0x2523C` | 只写 `B+0x3F74`，**不创建符卡对象** |

**id 公式证据**（处理器体 `0x425191`–`0x4251DF`）：

```asm
004251a2  0fbf4004        movsx eax, word [eax+4]     ; opcode
004251a6  2d19020000      sub   eax, 0x219
004251ab  7426            je    0x4251D3            ; 0x219 → 直接叫 0x41B3B0
004251ad  83e801          sub   eax, 1
004251b0  7414            je    0x4251C6            ; 0x21A → 先 dec esi
004251b2  83e801          sub   eax, 1
004251b5  7528            jne   0x4251DF            ; 其它（0x20A/0x210）→ 不调整
004251c6  b9c0594b00      mov   ecx, 0x4B59C0
004251cb  e8e061ffff      call  0x41B3B0            ; return *(u32*)(0x4B59C0+0x40) = difficulty
004251d0  4e              dec   esi
004251dd  03f0            add   esi, eax            ; esi = idBase(±偏移) + difficulty
```

`0x41B3B0`：`mov eax,[ecx+0x40]; ret`，而 `0x4B59C0 + 0x40 = 0x4B5A00` = `DIFFICULTY`（thprac 的 `enum addrs` 同一地址）⇒ **难度直接相加**，与 th16 一致。

**声明调用**（`0x4251F2`–`0x425201`）：

```asm
004251f2  8b0d90764b00    mov  ecx, [0x4B7690]        ; this = 符卡对象
004251f8  50              push eax                    ; bgm
004251f9  8d8568ffffff    lea  eax, [ebp-0x98]        ; 解密后的名字缓冲
004251ff  50              push eax
00425200  56              push esi                    ; id
00425201  e8ea5affff      call 0x41ACF0               ; ★ 声明函数
```

**名字解密**（`0x42513E`–`0x425189`）：`ah = 0x77`、`dl = 0x07`、每字节 `ah += dl`、`dl += 0x10` —— 与 th16 **完全相同**。实测解 `st01bs.ecl 0x1cb0` 的 28 B → `石符「ストーンコニファー」`，与 `.decl` 逐字一致。

**实测样例**（离线解码，全部与 `.decl` 对齐）：

| 文件 | 指令偏移 | opcode | size | args | 名字 |
| --- | --- | --- | --- | --- | --- |
| `st01bs.ecl` | `0x1C78` | `0x0219` | `0x38` | `(0, 1800, 0, 24)` | `石符「ストーンウッズ」` |
| `st01bs.ecl` | `0x1CB0` | `0x0219` | `0x3C` | `(0, 1800, 0, 28)` | `石符「ストーンコニファー」` |
| `st07bs.ecl` | `0x65A0` | `0x020A` | `0x40` | `(91, 4200, 0, 32)` | `勁疾技「スリリングショット」` |
| `st07mbs.ecl` | `0x13EC` | `0x020A` | `0x34` | `(88, 2400, 1, 20)` | `血戦「血の分水嶺」` |
| `st01bs.ecl` | `0x10A8` | `0x020B` | `0x10` | — | `spellEnd()` |

### 3.2 每张卡的 sub 结构（**与 th16 完全同形**）

以 `st01bs.decl` 的 `BossCard1` / `Boss2` 为例：

```text
void BossCard1() {
    unless ($TIMEOUT == 0) goto ...; etCancel/etClear;   // 收尾上一段特效
    @BossItem(...); timerReset(); killAllAsync(); enmKillAll();
    spellEnd();                        // ← 结算“上一张”卡（本例为 no-op）
    funcSet(0); reset(); playSound(27); ...
    $MISS_COUNT = 0; $BOMB_COUNT = 0; $CAPTURE = 1;
    setNext(0, 0, 1800, "Boss2");      // ← 本卡结束后的去向
    spell(0, 1800, 0, "...");          // ← 声明“本张”卡（bit0 = 1）
    ... 攻击本体（循环 wait / goto）...
}
void Boss2() {
    ...
    spellEnd();                        // ← 结算 BossCard1（bit0 1 → 0）
    ...
    setNext(0, 2400, 1800, "BossCard2");
    ... 非符攻击本体 ... 最后 @BossCard2(); return;
}
```

三条硬结论：

1. **`spell()` 声明本张卡、`spellEnd()` 结算上一张卡**，两者都在 sub 的**开头**（同一帧内，中间隔着 `funcSet/reset/playSound/$MISS_COUNT…/setNext` 等即时指令）。
2. **卡结束的下降沿发生在“下一个攻击 sub 的第一帧”**，不是卡自己 sub 的尾帧。
3. 每个攻击 sub（`Boss2`…`Boss10`、`BossDead`、`BossEscape`、`MBoss2/3`、`MBossDead`、`MBossEscape`）**开头都有 `spellEnd()`**；只有 `Boss1` / `MBoss` 没有（首段无前一张卡）。⇒ `spellEnd` 是幂等的（`0x41B14D` 判 bit0 后直接返回）。

### 3.3 `spellEnd()` 的结算对象与时机（`0x41B140`）

```asm
0041b147  8b3590764b00    mov  esi,[0x4B7690]
0041b14d  f6467801        test byte [esi+0x78],1
0041b151  0f8495010000    je   0x41B2EC              ; 无活动卡 → 直接返回（幂等）
0041b157  a180764b00      mov  eax,[0x4B7680]
0041b15e  83888c67000001  or   dword [eax+0x678C],1  ; boss 侧“符卡中”标记
0041b165 ... ff7610/14/18 push +0x10/+0x14/+0x18 ; call 0x476510（停 3 个句柄）
0041b181  836678fe        and  dword [esi+0x78],0xFFFFFFFE   ; ★ bit0 清 0（唯一）
0041b185  ff760c          push [esi+0x0C] ; call 0x476620 ; [esi+0x0C]=0
0041b194  836678df        and  dword [esi+0x78],0xFFFFFFDF   ; 清 bit5
0041b198  e873f4ffff      call 0x41A610
0041b19d  ff761c          push [esi+0x1C] ; call 0x476620 ; [esi+0x1C]=0
0041b1ac  f6467802        test byte [esi+0x78],2
0041b1b0  0f8419010000    je   0x41B2CF              ; 失败分支
   ; —— 成功：加分（bonus/10，封顶 1e9）、call 0x42F8A0(mgr,bonus,0)、
   ;          若 [0x4B77D8]+0xC != 1 则累加收取计数、playSe(0x2E)
0041b2cf  8b0dac764b00    mov  ecx,[0x4B76AC] ; 6a01/6a00 ; call 0x42F8A0   ; 失败
0041b2de  f6467880        test byte [esi+0x78],0x80 ; je ret ; playSe(0x45)
```

⇒ **`bit1` 是引擎自己认定的“收取成功”判据**（与 th16 逐句同构：`test 2` → 加分 + 计数 + `playSe(0x2E)`；否则走 `0x42F8A0(mgr,0,1)`）。本迭代不使用它。

### 3.4 `_at` / `_Dead` 的角色

| 名字 | 角色 | 对 spellFlag 的影响 |
| --- | --- | --- |
| `BossCardN` / `MBossCardN` | **符卡本体**（声明 + 攻击主循环） | 记录名来源（CSV 精确匹配） |
| `BossCardN_at` / `_at2` / `_at3` | 弹幕发射子脚本。**既有 `async` 派生也有同步 `@` 调用**（如 `st01bs` 的 `@BossCard1_at(_f(100), $A);` 无 `async`） | 同步调用时主线程当前 sub 会短暂变成 `_at` ⇒ **不能用“当前 sub 名”判卡是否结束**；用 bit0 则不受影响 |
| `BossCard1_AtEt_Dead` / `BossCard2_AtEt_Dead`（st05bs） | `setDeath()` 注册的 add 敌人死亡动画，boss 活着就会跑 | 不在 CSV，精确匹配天然排除 |
| `BossPhantom01DeadAt(_at)` 等 | add 敌人死亡动画（`st03bs` / `st07bs`） | 同上 |
| `BossN` / `MBossN` | 非符攻击 | 不产生符卡事件；**但它们是卡结束后 `mSection` 切换的目标** |
| `BossDead` / `MBossDead` / `BossEscape` / `MBossEscape` / `BossDead2` | 关卡收尾 | **最后一张卡由它们结算**：`BossDead` 的 `spellEnd()` 在 `+60:` 帧、`BossEscape`/`MBossEscape` 在 t=0（见 §6.4 / §6.5） |

### 3.5 逐卡 id（离线解出，**按 sub 名**，不是 CSV 记录名）

| 文件 | sub（opcode） | idBase | 实际 id（E/N/H/L） | 时限（帧） |
| --- | --- | --- | --- | --- |
| `st01bs` | `BossCard1` (0x219) / `BossCard2` (0x219) | 0 / 4 | 0–3 / 4–7 | 1800 / 1800 |
| `st02bs` | `BossCard1/2/3` (0x219) | 8 / 12 / 16 | 8–19 | 1800 / 2400 / 2400 |
| `st03bs` | `BossCard1/2/3` (0x219) | 20 / 24 / 28 | 20–31 | 2100 / 2100 / 2400 |
| `st04bs` | `BossCard1/2/3` (0x219) | 32 / 36 / 40 | 32–43 | 2400 / 2400 / 3300 |
| `st05bs` | `BossCard1/2/3/4` (0x219) | 44 / 48 / 52 / 56 | 44–59 | 2700 ×3 / 3600 |
| `st06bs` | `BossCard1..7` (0x219) | 60,64,68,72,76,80,84 | 60–87 | 3300 ×3 / 3600 ×2 / 4800 / 7200 |
| `st07bs` | `BossCard1..10` (0x20A `spellEx`) | 91..100 | 91–100（**不加难度**） | 4200/4200/4200/4800×4/5940/5880/7200 |
| `st07mbs` | `MBossCard1/2/3` (0x20A) | 88 / 89 / 90 | 88–90 | 2400 ×3 |
| `st01mbs`..`st06mbs` | **无符卡声明** | — | — | 中boss 单相位（`MBoss` 直接 `setNext(...,"MBossDead")`） |

> `st06bs` 的 `BossCard7` = **id 84**，E/N/H/L → 84/85/86/87，正是 `IsInKeikiFinal()` 的区间（`th17_keiki_phasetrack` 的用途）。

---

## 4. 轮询可行性

### 4.1 方法

对 `../exes/ecl_dump/th17/*.decl`（22 份）逐份做**语句级**解析（去注释 → 按 `;` 切分、跨行拼接 → 按 `void Name(){…}` 切函数体），提取每个 sub 的 `spell/spellEx/spellEnd/setNext`，并同时用离线 `.ecl` 解码（按 `size` 线性走指令流）交叉核对，避免 `.decl` 文本与二进制不一致。

### 4.2 结果：**无 card → card**

| 项 | 结果 |
| --- | --- |
| 声明符卡的 sub | 8 份含符卡的脚本（`st01bs`–`st07bs`、`st07mbs`）合计 **35 个**符卡 sub（唯一名 13 个：`BossCard1..10` + `MBossCard1..3`） |
| `setNext(..., "…Card…")` 且源 sub 也是符卡 sub 的情况 | **0** |
| 每个卡 sub 的 `setNext` 目标 | `Boss2`/`Boss3`/…/`Boss10`、`BossDead`、`MBoss2`/`MBoss3`、`MBossDead` —— **全部是非符或收尾 sub** |
| 卡与非符的进入方式 | 两种：① 非符 sub 里 `setNext(…, "BossCardN")`；② 非符 sub 末尾 `@BossCardN(); return;`（`st06bs` 的 `Boss5/6/7`、`st07bs` 的 `Boss10` 等）——**都在非符跑完之后** |
| 最小间隔 | 非符攻击时限 1800–4200 帧（`setNext` 第 3 参）⇒ 卡结束后要过 **30–70 s** 才有下一张卡（60 Hz） |

**结论：一帧一次采样不会漏。** 每张卡结束（bit0 1→0）后，bit0 至少保持 0 到下一个非符结束，远大于 1 帧。

**对策（本期不需要，留作变体备注）**：若将来出现 card→card（改脚本 / 新变体 / 汉化版差异），一帧一次的下降沿会漏计；届时应改用 th16 §5.2 的做法——在采样里额外比对 `+0x74`（spell id）或「当前命中的记录名」，id / 记录名变化即视为“上一张已结算”，不依赖 bit0 的下降沿。

### 4.3 th17 的“双分支”核查

- th17 **没有** th16 那种 `BossCard3B` / `BossCard4B` 的 A/B 路线孪生卡（全 22 份 `.decl` 的 `*Card*` 名枚举：仅 `BossCard1..10`、`MBossCard1..3` 及 `_at/_at2/_at3/_AtEt/_Dead/Pos/beast/wait/move` 辅助 sub）。
- 唯一的分支是**符卡练习**：`st01bs` 的 `Boss()` 里 `unless (($SPELL_ID >= 0) && ($SPELL_ID <= 3)) goto …` → `@BossCard1()`；进入的是**同一个卡 sub**，id 体系不变。
- thprac 的 `keepSpellDrops` 只是**改写 ECL 缓冲里的“掉落开关”字节**（`THPatch` 的 `pair{0x1a44,0}` 等），不改变 sub 结构，也不产生新的卡。
- ⇒ 用户提到的「`BossCard*` 变体」在 th17 **不存在**；`card → card` 检测结果为 0 与 th16 同结论。

### 4.4 同帧风险

| 风险 | 结论 |
| --- | --- |
| 卡 sub 开头「`spellEnd()` + `spell()`」同帧 | 该 `spellEnd()` 结算的是**上一张**，而上一张在**上一个 sub 开头**就已结算 ⇒ 此时是 no-op；真正“结算 + 声明”同帧只可能发生在**卡紧跟卡**时，而 card→card = 0 ⇒ **不会漏** |
| 卡结束与新 checkpoint 同帧 | 预期行为：`++`（顶部）与 `--`（段落变化）同帧净 0（§5 顺序约束） |
| 卡结束时当前 sub 是 `_at` | bit0 是引擎状态，不受当前 sub 名影响 ⇒ 比 sub 名稳 |

---

## 5. 接入方案（`thprac_th17.cpp` 精确插入点）

> **已落地（2026-10-20，方案 A）**：以下 9 处在 `thprac/src/thprac/thprac_th17.cpp` 中已实现；本节的「当前行」是**落地前**的参考值，落地后的实际行号见 §5.6。未编译、未真机（🟡）。
>
> 方案：**照搬 th16 方案 B（纯轮询 bit0 下降沿）**，零新 hook、共享层零改动、无新事件类型。下表行号为**落地前**的 `thprac/src/thprac/thprac_th17.cpp`（3250 行）参考值。

### 5.1 九处改动（逐条）

| # | 位置（当前行） | 现语句 | 改动 |
| --- | --- | --- | --- |
| 1 | `enum addrs`，**`:85`** | `SPELLCARD_PTR = 0x4b7690,` | **不改值**（它已经是符卡对象全局）。仅建议把注释改成「符卡对象全局指针（`+0x78` bit0 = 存活）」 |
| 2 | 匿名 namespace，**`:111`** 之后 | `constexpr uintptr_t kThreadSubOff = 4;` | 新增 `constexpr uintptr_t kSpellFlagsOff = 0x78;`（附 bit0/bit1 语义注释） |
| 3 | `TH17Logger` private 声明，**`:228`** 之后 | `void BuildStageMap(int32_t stageNum);` | 新增 `void SampleSpellCard();` |
| 4 | 成员，**`:241`**（`int32_t mBuiltStage = -1;`）之后、`:242` 的注释块之前 | `int32_t mBuiltStage = -1;` | 新增 `int32_t mSpellFlag = 0;` / `uintptr_t mSpellObj = 0;` / `bool mSpellActive = false;`（附口径 15 语义注释） |
| 5 | 定义，**`:320`**（`OnBreak()` 之后、`Update()` 之前） | `}` | 新增 `void TH17Logger::SampleSpellCard()`（见 §5.2） |
| 6 | `OnStageStart()`，**`:301`**（`mStageClear = false;`）之后 | `mStageClear = false;` | 新增 `mSpellFlag = 0; mSpellObj = 0; mSpellActive = false;` |
| 7 | `Update()` 顶部，**`:325`** 之后 / **`:328`** 之前 | `bool inGame = …;` | 插入 `SampleSpellCard();`；并把 `:328` 改为 `if (mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame))` |
| 8 | 换关块，**`:356`**（`mStageClear = false;`）之后 | `mStageClear = false;` | 新增 `mSpellFlag = 0; mSpellObj = 0; mSpellActive = false;` |
| 9 | 段落变化块，**`:476`–`:478`**（块内 `mStageClear = false;` 在 `:477`） | `if (mSection != prevSection && std::strcmp(mSection, prevSection) != 0) {` … `mStageClear = false;` | 块内**首句**新增 `if (mSpellFlag > 0) { --mSpellFlag; }`（在 `mStageClear = false;` 之前） |

### 5.2 `SampleSpellCard()`（与 th16 逐字同构，只换常量）

```cpp
void TH17Logger::SampleSpellCard()
{
    // No allocation: this runs inside a VEH hook callback.
    // 本作 spell / spellEx 共用同一个对象（0x42513E → 0x41ACF0），
    // bit0 只在声明 0x41AD58 置 1、只在 spellEnd 0x41B181 清 0。
    uintptr_t obj = GetMemContent<uintptr_t>(SPELLCARD_PTR);
    if (!obj) {
        // Menu / stage teardown. 只清采样状态，保留 mSpellFlag：退出帧还要用。
        mSpellObj = 0;
        mSpellActive = false;
        return;
    }
    bool active = (GetMemContent<uint32_t>(obj + kSpellFlagsOff) & 1) != 0;
    if (obj != mSpellObj) {
        // 对象被重建（换关 / 重试 / 重开）：新窗口，旧对象的销毁不算一次卡结束。
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

### 5.3 顺序约束（三条都不能反，与 th16 相同）

1. `SampleSpellCard()` 必须在 `interrupt` 判定**之前**（`:328`）⇒ 同帧「清卡 + 退出」也能吃到 `++`。
2. `++` 在 `Update()` 顶部、`--` 在段落裁决之后（`:476`）⇒ 同帧「卡结束 + 新 checkpoint」净 0；反过来会残留 1，吞掉之后整关的 `interrupt`。
3. 换关复位（`:356`）在采样**之后** ⇒ 换关/对象销毁的假下降沿当场抹掉；对象为空时**不清 `mSpellFlag`**，否则退出帧没用上就先被清了。

### 5.4 「段落变化」判据：**与 th16 一致**

```cpp
const char* prevSection = mSection;      // :440
...
if (mSection != prevSection && std::strcmp(mSection, prevSection) != 0) {   // :476
    // ← 在此加 --mSpellFlag
    mStageClear = false;                 // :477
}
```

差别只在**上游赋值**：th17 多了一段中boss「迟到的章节 checkpoint」回放（`:446`–`:474`，`sawMidBossEnd` + `mPending`），它同样在 `:476` 之前改 `mSection`，因此 `--` 的位置无需调整。**判据本身（指针不等 + `strcmp`）与 th16 完全相同。**

### 5.5 与 `mStageClear` 的关系（方案 A 保留）

- `mStageClear` 的置位/复位逻辑**不动**（`:480` 置位、`:477`/`:356`/`:301` 复位）。
- `interrupt` 条件从 `!mStageClear && …` 变为 **`mSpellFlag <= 0 && !mStageClear && …`**（两个条件相互独立，不互相复位）。
- 二者覆盖不同窗口：`spellFlag` 管「卡已结束但 checkpoint 还没切」，`mStageClear` 管「关底收尾的 `+60` 帧窗口」与「非符期 `BossEscape`」。

### 5.6 落地后实际行号（2026-10-20，`thprac_th17.cpp` 现 3314 行）

| # | 落地位置（现行号） | 内容 |
| --- | --- | --- |
| 1 | `:85` | `SPELLCARD_PTR = 0x4b7690,` —— **未改动**（复用，全文仅此一处 `0x4b7690`） |
| 2 | `:113`–`:118` | `constexpr uintptr_t kSpellFlagsOff = 0x78;`（附 bit0/bit1 语义与「别用 `+0x1C`」注释） |
| 3 | `:236`–`:237` | 声明 `void SampleSpellCard();` |
| 4 | `:251`–`:258` | 成员 `mSpellFlag` / `mSpellObj` / `mSpellActive`（在 `mBuiltStage` 之后、`mStageClear` 之前） |
| 5 | `:344`–`:372` | `void TH17Logger::SampleSpellCard()` 定义（在 `OnBreak()` 之后、`Update()` 之前） |
| 6 | `:320`–`:322` | `OnStageStart()` 体末复位三者（在 `mStageClear = false;` 之前） |
| 7 | `:378`–`:379` / `:384` | `Update()` 顶部 `SampleSpellCard();`；条件改为 `if (mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame))` |
| 8 | `:412`–`:414` | 换关块（`stageNum != mBuiltStage`）复位三者 |
| 9 | `:535`–`:542` | 段落变化块首句 `if (mSpellFlag > 0) { --mSpellFlag; }`（在 `mStageClear = false;` 之前） |

> 顺序核对（与 §5.3 一致）：`SampleSpellCard()` 在 `interrupt` 判定之前；`++` 在 `Update()` 顶部、`--` 在段落裁决块；换关复位在采样之后。`IsStageClearSub()` / `mStageClear` / `sawStageClear` 均未删除或改写。
> 静态自检（本机无 MSVC，**未编译**）：`git diff --check`（本文件）干净；剥离注释/字符串后 `()[]{}` 平衡；`SPELLCARD_PTR` 仅 `:85` 一处定义、`kSpellFlagsOff` 1 定义 + 1 引用、`SampleSpellCard` 1 声明 + 1 定义。

---

## 6. 覆盖性（逐关）

### 6.1 关底：最后一张攻击阶段是否 `spell()`

| 关 | 非符 sub（是否在 CSV） | 符卡 sub（idBase） | 最后一张卡 → 下一 sub | 结论 |
| --- | --- | --- | --- | --- |
| ST1 | `Boss1`(✓) `Boss2`(✓) | `BossCard1(0)` `BossCard2(4)` | `BossCard2` → `BossDead` | ✅ 末段是符卡 |
| ST2 | `Boss1`(✓) `Boss2`(✓) `Boss3`(**✗**) | `BossCard1(8)` `BossCard2(12)` `BossCard3(16)` | `BossCard3` → `BossDead` | ✅（`Boss3` 处有滞留洞，§6.3） |
| ST3 | `Boss1/2/3`(✓) | `BossCard1(20)` `BossCard2(24)` `BossCard3(28)` | `BossCard3` → `BossDead` | ✅ |
| ST4 | `Boss1/2/3`(✓) | `BossCard1(32)` `BossCard2(36)` `BossCard3(40)` | `BossCard3` → `BossDead` | ✅ |
| ST5 | `Boss1/2/3`(✓) `Boss4`(**✗**) | `BossCard1(44)` … `BossCard4(56)` | `BossCard4` → `BossDead` | ✅（`Boss4` 滞留洞） |
| ST6 | `Boss1/2/3/4`(✓) `Boss5/6/7`(**✗**) | `BossCard1(60)` … `BossCard7(84)` | `BossCard7` → `BossDead` | ✅（`Boss5/6/7` 滞留洞） |
| ST7 | `Boss1..8`(✓) `Boss9/10`(**✗**) | `BossCard1(91)` … `BossCard10(100)`（`spellEx`） | `BossCard10` → `BossDead`（Extra 另有 `BossDead2`） | ✅（`Boss9/10` 滞留洞） |

> **7 关关底最后一张攻击阶段全部是 `spell()` / `spellEx()`**，没有“末段是非符”的关。

### 6.2 中boss 最后阶段

| 关 | 中boss 脚本 | 符卡 | 结论 |
| --- | --- | --- | --- |
| ST1–ST6 | `st01mbs`…`st06mbs` | **0 张**（`MBoss` 单相位，`setNext(1, 0, 1440/2160/1500, "MBossDead")`） | 无卡可结束 ⇒ 不产生 `spellFlag`，也不会漏（`MBossDead` 的 `spellEnd` 是 no-op） |
| ST7 | `st07mbs` | 3 张 `spellEx`（88/89/90） | 末张 `MBossCard3` → `MBossDead` ✅ |

> 与 th16 的差异：th16 的中boss（`st03mbs` / `st07mbs`）有 `spell3` 卡；th17 的 ST1–ST6 中boss **完全没有符卡**（这也是 `docs/logger/th17_checkpoints.csv` 每关只有一行 `MBoss` 的原因）。

### 6.3 已知洞 / 假阴性（`interrupt`）

「卡结束后 `mSection` 不切换 ⇒ 计数滞留」的情形（下一 sub 不在 CSV 白名单）：

| 关 | 卡结束后进入的非符 | 该非符是否在 CSV | 滞留窗口 |
| --- | --- | --- | --- |
| ST2 | `Boss3`（`BossCard2` 之后） | ✗ | 整个 `Boss3` 阶段（`lifeSet(3000)`，到 `@BossCard3()` 为止） |
| ST5 | `Boss4`（`BossCard3` 之后） | ✗ | 整个 `Boss4` 阶段（`lifeSet(3000)`） |
| ST6 | `Boss5` / `Boss6` / `Boss7` | ✗ | 整三段（`lifeSet(3500 / 5000 / 10000)`） |
| ST7 | `Boss9` / `Boss10` | ✗ | 整两段（`lifeSet(3000 / 10000)`） |

**影响**：这些窗口内 `mSpellFlag > 0` ⇒ 玩家在此退出**不会**记 `interrupt`（假阴性）。这是 th16 已知副作用（`docs/knownBugs.md` #12）的同类现象，本迭代接受；若将来要收敛，可在 `BuildStageMap` 时把「上一段是卡」与「当前 sub 名」做一次显式消费（不建议本期做）。

### 6.4 `BossEscape` 是否可能在非符期触发

| 证据 | 内容 |
| --- | --- |
| 脚本侧 | **只有中boss** 注册超时：`st01mbs`…`st07mbs` 的 `MBoss` 里 `setTimeout(1, "MBossEscape")`；**7 个 `*bs` 都没有 `setTimeout`** |
| 引擎侧 | `.rdata` 保留 `"MBossDead"` / `"BossDead"` / `"BossEscape"` / `"MBossBDead"`（VA `0x4A15F4` / `0x4A1600` / `0x4A160C` / `0x4A161A`），且 `0x424F18 mov ecx,0x4A1600`、`0x424F4F push 0x4A160C` 在扩展 opcode 的“按名切换 sub”路径里被使用 ⇒ **引擎自己可以把关底 boss 切到 `BossEscape`**（🟡 触发条件未静态定死） |
| 结论 | **可能**在非符期触发（那时 bit0 已经是 0，`spellEnd` 是 no-op ⇒ **无下降沿**）⇒ **必须靠 `mStageClear` 兜底**（`IsStageClearSub` 已含 `BossEscape`） |

### 6.5 关底 `BossDead` 的 60 帧窗口

`st01bs` / `st06bs` / `st07bs` 的 `BossDead` 都是（7 关同形，此处摘三例）：

```text
void BossDead() {
    flagSet(156); playSound(5); moveVel(...);
    unless ($TIMEOUT == 0) goto …; enmCreate("Ecl_EtBreak2…");
    enmKillAll();
    +60:                       // ← 60 帧之后
    etClearAll();
    spellEnd();                // ← 最后一张卡在这里才清 bit0
    ...
}
```

⇒ 关底 boss 死亡后的 **60 帧内 bit0 仍为 1、没有下降沿**；此时退出游戏只能靠 `mStageClear`（`BossDead` 成为当前 sub 的那一刻就置位）。**这正是保留方案 A 的理由之一。**

---

## 7. 风险与真机验证项（🟡）

> 本机（macOS）无 MSVC：**未编译、未真机**。以下为落地前的最小验证集。

| # | 项 | 静态结论 | 真机最小验证 |
| --- | --- | --- | --- |
| 1 | `0x4B7690` 空/非空 | 构造 `0x430AD0`、析构 `0x41A85C` | 标题读 `*(u32*)0x4B7690` 期望 0；进关后非 0 |
| 2 | bit0 = 存活 | 置位唯一 `0x41AD58`、清位唯一 `0x41B181` | ST1 `BOSS2`（`BossCard1`）：卡内 `obj+0x78 & 1` = 1；击破后变 0，同时 `spellFlag` +1 |
| 3 | 卡结束 = 下一 sub 首帧 | 每个攻击 sub 开头 `spellEnd()`（§3.2） | 击破 `BossCard1` 的那一帧单步：`bit0` 变 0 与 sub 切到 `Boss2` 同帧 |
| 4 | 主目标（修 `interrupt`） | `++/--` 净 0、`mStageClear` 兜底 | 打完 `ST1 BOSS4` 立即退出 ⇒ 无 `interrupt`；`BossDead+60` 窗口内退出 ⇒ 仍无 `interrupt` |
| 5 | 卡→非符的 checkpoint 切换 | `Boss2` 在 CSV | 卡结束当帧 `mSection` 应变 `ST1 BOSS3`，`mSpellFlag` 应为 0（净 0） |
| 6 | 滞留洞 | ST2 `Boss3` / ST5 `Boss4` / ST6 `Boss5-7` / ST7 `Boss9-10` 不在 CSV | 在这些非符期退出：预期**无** `interrupt`（假阴性，接受）；若不可接受则需二期方案 |
| 7 | 中boss | ST1–6 无符卡；ST7 三张 `spellEx` | ST1 中boss 全程 `bit0` 恒 0；ST7 `MID1/2/3` 各产生一次 `++` |
| 8 | `BossEscape` | 引擎字符串存在、脚本无 `setTimeout` | 让关底 boss 超时/逃逸：确认 `BossEscape` 是否真的出现，以及出现时 `mStageClear` 是否置位 |
| 9 | 对象重建（同地址） | `malloc/free` 可能复用地址 | 重试同一关，确认 `spellFlag` 不会因对象重建而多计（`OnStageStart` 先复位） |
| 10 | `spellEx` 的 id | `0x20A` 不加难度（§3.1） | ST7 各卡读 `+0x74`：期望 91…100 不随难度变化 |
| 11 | thprac 的 ECL 补丁 | `THPatch` 只改掉落开关字节 | 带 `keepSpellDrops` 打一张卡，确认 `bit0` 行为不变 |
| 12 | 变体 exe（Steam/汉化） | 未核 | 按变体重跑 `0x4B7690` / `0x41AD58` / `0x41B181` 的字节比对 |

### 7.1 与 th16 的差异汇总（**回答“能否直接照搬 9 处接线”**）

| 项 | th16 | th17 | 影响 |
| --- | --- | --- | --- |
| 符卡对象全局 | `0x4A6DB0` | **`0x4B7690`** | 只换常量；th17 已存在 `SPELLCARD_PTR`，**无需新增** |
| 对象大小 | `0xBC` | **`0xC0`** | 无（只读 `+0x78`） |
| flags 偏移/位 | `+0x78` bit0/bit1 | **`+0x78` bit0/bit1（完全相同）** | ✅ 直接复用 |
| 置位/清位 | `0x417F76` / `0x41832F` | **`0x41AD58` / `0x41B181`**（均唯一） | 只影响文档记录 |
| `IsInKeikiFinal` 的 `+0x1C` | th16 的 `+0x1C` 是音效句柄 | **th17 的 `+0x1C` 是 ANM 句柄**（可在卡内被写 0） | **不可用作 bit0**；也不要改 `IsInKeikiFinal`（本迭代只读） |
| ECL 声明 | `0x219/0x21A/0x21B/0x20A` | `0x219`/`0x20A`（另 `0x21A/0x21B/0x210` 映射同处理器但未用） | 无 |
| ECL 结算 | `0x20B → 0x421891 → 0x4182F0` | **`0x20B → 0x425256 → 0x41B140`** | 无 |
| 中boss 符卡 | th16 有 `spell3` | **ST1–6 无符卡**；ST7 三张 `spellEx` | 无（无卡可漏） |
| card→card | 0 | **0** | ✅ 轮询可行 |
| 60 帧宽限 | `[0x4A6DA8]+0x30 == 1` | `[0x4B7688]+0x30 == 1` | 不使用（只看 bit0） |
| `BossDead` 的 `+60` | 有 | **有**（7 关一致） | 保留 `mStageClear` |

**结论：9 处接线可以逐条照搬**（第 1 处不用新增常量，只是复用 `SPELLCARD_PTR`），差异仅在注释与常量取值；**唯一必须注意的是 `+0x1C` 不能当存活位**——这正是既有 `IsInKeikiFinal` 的近似用法，不要把它当成 bit0 的替代。

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **建档**。th17 `spellFlag` 静态调研：① 盘点 `thprac_th17.cpp` 全部 EHOOK/PATCH（无一处读写符卡状态；仅 `IsInKeikiFinal` 只读 `+0x1C`/`+0x74`）；② 复核 `+0x1C` = 符卡 **ANM 句柄**（`0x41AF68` 创建 / `0x41B1A5` 清 0，卡内可被写 0）⇒ **不可当 bit0**；`+0x74` = spell id（`0x41AD3D`）；③ 定案符卡对象 **`0x4B7690`**、大小 **`0xC0`**（构造 `0x430A84`–`0x430AD0`、析构 `0x41A760` / 唯一调用 `0x43110A`）；**`+0x78` bit0 = 存活**，置位唯一 **`0x41AD58`**、清位唯一 **`0x41B181`**（全 `.text` 写点穷举）；bit1 = 收取，7 个 `and 0xFFFFFFDD` 失败点；④ ECL 侧用分发表（`0x4268D0` 字节映射 + `0x42661C` 跳表）证 **`0x0219`/`0x020A`/`0x021A`/`0x021B`/`0x0210` → `0x42513E` → `0x41ACF0`**、**`0x020B` → `0x425256` → `0x41B140`**，`id = idBase + difficulty`（`0x41B3B0` 读 `0x4B5A00`）；⑤ 22 份 `.decl` + 22 份 `.ecl` 全量核对 **card→card = 0**（35 个符卡 sub / 唯一名 13 个），th17 无 A/B 路线孪生卡；⑥ 给出 `thprac_th17.cpp` 的 9 处精确插入点（行号 + 语句）与「段落变化」判据（与 th16 相同）；⑦ 逐关覆盖：7 关关底末段均为符卡，ST1–6 中boss 无符卡、ST7 三张 `spellEx`；⑧ 列出滞留洞（ST2 `Boss3`、ST5 `Boss4`、ST6 `Boss5-7`、ST7 `Boss9-10` 不在 CSV）与 `BossDead +60` / `BossEscape` 的 `mStageClear` 兜底理由。**全部为静态结论，未编译、未真机（🟡）**。 |
| 2026-10-20 | **代码落地（方案 A）**。在 `thprac/src/thprac/thprac_th17.cpp` 按 §5 完成 9 处接线中的 8 处实质改动：① `enum addrs` **未改**（复用既有 `SPELLCARD_PTR = 0x4b7690`，全文仅一处）；② 匿名 namespace 新增 `kSpellFlagsOff = 0x78`；③ 新增私有方法 `SampleSpellCard()`（1 声明 :237 + 1 定义 :344）；④ 新增成员 `mSpellFlag` / `mSpellObj` / `mSpellActive`（:256–258）；⑤ `OnStageStart()` 复位三者（:320）；⑥ `Update()` 顶部 `inGame` 之后、interrupt 判定之前采样，条件改为 `mSpellFlag <= 0 && !mStageClear && …`（:379 / :384）；⑦ 换关块复位三者（:412）；⑧ 段落变化块首句 `--mSpellFlag`（下限 0，:538）。`IsStageClearSub()` / `mStageClear` / `sawStageClear` 保留（兜底）；零新 EHOOK、共享层零改动、无新事件类型。静态自检：本文件 `git diff --check` 干净、剥离注释/字符串后括号平衡、无重复常量定义。**未编译 / 未真机（🟡）**；§7 的 12 项真机验证仍待做。 |

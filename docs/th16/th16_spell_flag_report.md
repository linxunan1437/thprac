# TH16 符卡收取 / 失败（spellFlag）— 静态逆向报告

> 上位文档：`docs/req_logger.md`（口径 10 / 12 / 13）、`docs/logger/logger_re_disasm_overview.md`（术语与地址口径）、`docs/logger/logger_impl_report.md`（§4.4 stageClear 接线）
> 需求来源：用户 2026-10-20 需求——把 th16 的 `stageClear` 升级为 **`spellFlag`**：在「符卡收取成功 / 失败」时触发；**本迭代只做 th16**
> 同作既有逆向：`docs/th16/th16_ecl_re_report.md`（Q1–Q9）、`docs/th16/th16_boss_phase_report.md`、`docs/th16/th16_monitor_impl_report.md`
> 二进制指纹：`../exes/th16.exe`，**683520 B**，PE `TimeDateStamp = 1501534029`（`0x597F974D`），`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；`.text` = VA `0x401000`–`0x48ACDA`，RVA `0x1000`–`0x89CDA`）
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` 只读反汇编；`objdump -d --arch=i386` 交叉核对；离线 ECL 解码（SCPT 头 → sub 表 → 指令流 → 名字 XOR 解密），并与 `../exes/ecl_dump/th16/*.decl` 逐行对照
> 素材：`../exes/ecl/th16/*.ecl`（22 份）、`../exes/ecl_dump/th16/*.decl`（22 份）
> 状态：**静态定案（🟡 未真机）**。本机 macOS，不可编译、不可真机；文中未真机验证的读数一律标 🟡
> 地址口径：正文以 **VA** 为主，同时标注 **RVA = VA − 0x400000**

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 现有符卡 hook | **无**。全仓库对 `0x4A6DB0`（符卡对象全局指针）**0 引用**；th16 的 EHOOK 里没有任何一个读写符卡状态（详见 §1） |
| ECL 声明符卡 | opcode **`0x0219`**（`spell`）/ **`0x021A`** / **`0x021B`**（`spell3`，中boss）/ **`0x020A`**（`spellEx`，Extra）；参数 `(idBase, time, bgmIdx, name)`，最终 id = `idBase + difficulty`（`spellEx` 不加） |
| ECL 结算符卡 | opcode **`0x020B`**（`spellEnd()`）→ 处理器 `0x421891` → `FUN_004182F0`。**`spellEnd()` 结算的是“上一张”卡**，每个攻击 sub 的开头都会调一次 |
| 引擎侧“是否在符卡中” | **`*(u32*)(*(u32*)0x4A6DB0 + 0x78) & 1`**（bit0）——符卡对象存活位，声明时置 1、结算时清 0 |
| 引擎侧“是否收取成功” | **同一个 flags 的 bit1（`0x2`）**：声明时置 1；**被弹 / 放 bomb / 超时**三条路径都会 `and ~0x22` 清掉它。结算时 bit1 还在 ⇒ 收取成功 |
| 失败原因可分辨 | ✅ 能：超时 = host `+0x526F` bit0（`$TIMEOUT`）；被弹 = `mgr+0x3C`（`$MISS_COUNT`）> 0；bomb = `mgr+0x40`（`$BOMB_COUNT`）> 0 |
| 失败原因读取的坑 | `$MISS_COUNT`/`$BOMB_COUNT`/`$CAPTURE` 是**每次攻击开头由 ECL 脚本清零/置 1** 的，结算瞬间就会被下一张卡重置 ⇒ **必须边打边累积**，不能等结算再读 |
| 方案 A（新增 hook） | 可行。最佳点 `0x41835A`（4 B，`testb $2,0x78(%esi)`，`ESI`=符卡对象）；也可挂 `0x421891`（5 B，`call FUN_004182F0`）。EHOOK 是 INT3+codecave，**不要求 ≥5 B**，只要求不切开指令 |
| 方案 B（纯轮询） | **可行且推荐**。`0x4A6DB0` + bit0/bit1 每帧可读；现有 `TH16Logger::Update()` 已经在遍历宿主/线程，边界与记录名映射可直接复用；**零新 hook、零新 ECL 地址** |
| 推荐 | **方案 B（纯轮询 bit0 下降沿）**，2026-10-20 已按此落地（见 §8）：`mSpellFlag` 计数器 + `mStageClear` 兜底；方案 A 仅作二期备选 |
| 语义 | **只做「符卡结束」事件**（2026-10-20 需求收敛，**不区分收取/失败**）：`spellFlag++` 由「一张卡结束」触发，`mSection` 换新 checkpoint 时 `--`；不是 `spellGet`/`spellFail` |
| 最大数据缺口 | ST7 中boss B 路线的 `MBossBCard1..3` **既不在 CSV、也没有 `spell()` 声明**（只有 `spellMode(1)`），本迭代无法产出记录名（详见 §4.3） |

---

## 1. 现有 hook 盘点（th16）

### 1.1 `thprac_th16.cpp` 全表

> 文件：`thprac/src/thprac/thprac_th16.cpp`（3221 行）。`EHOOK_*` 第二参为**覆盖长度**（`instr_len`），不是“至少 5 字节”的 detour 长度。

**`HOOKSET_DEFINE(THMainHook)`（:2852）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th16_inf_lives` | `0x443D39` | 1 | 无限命 | 否（同函数后段有 `$MISS_COUNT++`/`$CAPTURE=0`，但 hook 本身不读） |
| `th16_spbugfix` | `0x4214FA` | 6 | ST5 符卡 bug 修复（读 `0x4A5BEC & 0x30`、`STAGE_NUM==5`） | **否**（只判模式/关卡，不判收取） |
| `th16_everlasting_bgm` | `0x45ED00` | 1 | BGM | 否 |
| `th16_param_reset` | `0x44B610` | 7 | 参数复位 | 否 |
| `th16_prac_menu_1..4` | `0x450F60`/`0x450F83`/`0x4512CC`/`0x45136D` | 5/3/7/7 | 关卡练习菜单 | 否 |
| `th16_prac_menu_enter_1/2` | `0x451044`(patch)/`0x451327` | -/5 | 进入练习；Extra 改 sub-season | 否 |
| `th16_menu_rank_fix` | `0x44097F` | 5 | 把 `0x4A57C8`（符卡练习 id）置 −1 | **间接**（符卡练习 id 的重置点） |
| `th16_patch_main` | `0x42D1EC` | 1 | 练习参数写入；`mode==2` 调 `THPatchSP()` | **间接**（符卡练习补丁入口） |
| `th16_bgm` | `0x42DE8C` | 2 | BGM | 否 |
| `th16_rep_save` | `0x448BE4` | 5 | 存 rep | 否 |
| `th16_rep_menu_1..3` | `0x4518A6`/`0x4519C6`/`0x451B86` | 3/5/2 | rep 菜单 | 否 |
| `th16_spmenu_alt_1..4` | `0x455DA6`/`0x455E75`/`0x456B8B`/`0x456BB8` | patch | 符卡练习菜单布局修补 | 否 |
| `th16_sp_menu_1..5` | `0x455DBC`/`0x455DFF`/`0x455EEB`/`0x455FEE`/`0x455FE3` | 7/3/7/7/5 | 符卡练习菜单 `THGuiSP::State(1..5)` | **间接**（读 `0x4A6F20` 的练习列表/游标） |
| `th16_update` | `0x40156F` | 1 | 每帧派发（含 `TH16Logger::Update()`） | 采样点 |
| `th16_player_state` | `0x442560` | 1 | 键盘监视 | 否 |
| `th16_render` | `0x40168A` | 1 | 渲染 | 否 |

**`HOOKSET_DEFINE(THInGameInfo)`（:3126）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th16_game_start` | `0x42E5AE` | 7 | 一局开始（清 miss/bomb/release、`OnStageStart()`） | 否 |
| `th16_bomb_dec` | `0x40DB9C` | 5 | bomb 计数 + `OnBomb()` | 否（真正的 bomb 结算在 `0x40E8xx` 等 4 处） |
| `th16_life_dec` | `0x443D3A` | 5 | miss 计数 + `OnMiss()` | 否（但同函数 `0x443EAB` 之后清 capture，见 §3.4） |
| `th16_release` | `0x40DC8A` | 10 | 灵梦 release 计数 | 否 |
| `th16_lock_timer1..4` | `0x426D4B`/`0x424E40`/`0x420665`/`0x42509A` | 10/1/2/6 | 计时器锁 | **`0x424E40` 是 `setNext` 的写名字函数**（每次阶段注册都会进），可当“阶段切换”信号，但拿不到收取结果 |

**静态 `EHOOK_ST` / `PATCH_ST`（namespace 作用域）**

| 名称 | VA | len | 用途 | 与符卡有关？ |
| --- | --- | --- | --- | --- |
| `th16_all_clear_bonus_1` | `0x42E240` | patch | 全关奖励 | 否 |
| `th16_all_clear_bonus_2` | `0x42E2C0` | 7 | 读 `0x4A5BEC & 0x10` | 否 |
| `th16_all_clear_bonus_3` | `0x42E39B` | 7 | 读 `0x4A5BEC & 0x10` | 否 |
| `th16_crash_fix` | `0x46E6E8` | 10 | ANM 链表崩溃修复 | 否 |
| `th16_bossmovedown` | `0x41FF49` | 5 | boss 下移 | 否 |
| `th16_master_disable1a..1c` | `0x417E5C`/`0x417E81`/`0x417DC6` | patch | master 自动 ly 关闭（`disable_master_autoly`） | 否 |

**HOTKEY / `PATCH_HK` / `EHOOK_HK`（`THOverlay`）**

| 名称 | 地址 | 说明 | 与符卡有关？ |
| --- | --- | --- | --- |
| `mMuteki` | patch `0x443FE1` | 无敌 | 否 |
| `mInfBombs` | patch `0x40DB83` | 无限 bomb | 否 |
| `mInfPower` | patch `0x442749` | 无限 power | 否 |
| `mInfSubPower` | patch `0x40DCA8`/`0x40DCCC` | 无限 sub power | 否 |
| `mAutoBomb` | patch `0x4427A1` | 自动 bomb | 否 |
| `mTimeLock` | patch `0x417965`/`0x41D4EF`；EHOOK `0x473995`(4 B) | 冻结 ECL 时间（`Edi`=thread） | 否 |
| `THInitHook` | `0x44BDC1`/`0x45B788` | GUI 初始化 | 否 |

**小结：表内没有任何一个 hook 读取符卡对象 `0x4A6DB0`，也没有任何“收取/失败”判定。**

### 1.2 符卡练习（spell practice）相关的既有逻辑

这些逻辑**只服务练习菜单**，与“一局内的符卡结算”无关，但地址语义要记清楚（后续容易混）：

| 项 | 位置 | 语义 |
| --- | --- | --- |
| `0x4A57C8` | `thprac_th16.cpp:2561`（`THPatchSP` 读）、`:2935`（置 −1） | **符卡练习选中的 spell id**（`$SPELL_ID`，系统变量 id −9907）。普通游玩为 −1 |
| `0x4A6F20` | `thprac_th16.cpp:771`/`:772`（rep 名）、`:1039`（练习列表） | 指向 game manager 的**指针**；`+0x5B48` rep index、`+0x5B50` rep 名数组、`+0x5DCC` 练习列表游标、`+0x5DD0` 练习 spell id 数组 |
| `THPatchSP()` | `thprac_th16.cpp:2559` | `switch (*(i32*)0x4A57C8)` 改 HP / 速度 / 关闭 ANM；`ecl.SetFile(2/4)` |
| `th16_patch_main` | `0x42D1EC`，`thprac_th16.cpp:2943` | `mode==1` 写关卡练习参数；`mode==2` 调 `THPatchSP` |
| `THGuiSP::State(1)` | `thprac_th16.cpp:1039` | `spell_id = *(u32*)(*(u32*)0x4A6F20 + *(u32*)(*(u32*)0x4A6F20+0x5DCC)*4 + 0x5DD0)` |
| `THGuiSP::State(5)` | `thprac_th16.cpp:1075` | 按 `mSpellId < 106` 写 `SUBSHOT`（副季节） |
| `0x4A5BEC` | `:1149`/`:1168`（`&0x10`）、`:2870`（`&0x30==0x20`）、`:2899`（`&0x1`） | 游戏 flags：bit0 = 练习（`is_practice`）；bit4 = `0x10`（全关奖励判定）；bit5 = `0x20`（与符卡练习/历史桶相关，🟡）；bit6 = `0x40`（初始化分支，🟡） |

> `0x4A5BEC` 的三个已知用法都**不是**“符卡是否收取”：bit0 是“是否练习”，bit4/5 是模式位。**不要把 `0x4A5BEC` 当作 spellFlag 来源。**

### 1.3 其它作品有没有可借鉴的“符卡收取/失败”实现

| 作品 | 现有代码 | 结论 |
| --- | --- | --- |
| th17 | `SPELLCARD_PTR = 0x4B7690`（`thprac_th17.cpp:85`），`IsInKeikiFinal()`（`:2835`）读 `spellCard+0x1C`（active）与 `+0x74`（spellID） | **只用于判定“是否在 Keiki 终符”**，不做收取/失败；且**布局与 th16 不同**（th16 的 `+0x1C` 是音效句柄，存活位在 `+0x78` bit0），**不可跨作外推** |
| th14 / th15 / th18 | 只有符卡练习菜单（`mSpellId`）与 `spellMode` / ECL 补丁 | 无收取/失败实现 |
| `thprac_games.{h,cpp}` / `thprac_games_def.json` / `thprac_locale_def.*` | 只有 `GameSession` 事件容器、JSON 序列化与 locale 词条（`TH_SPELL`、`TH07_SPELLBONUS`） | **无 spell history 读取**；共享层没有任何“符卡”概念 |
| 全部 | 全仓库 `grep -i "capture"` 只命中 ImGui / locale 文案 | **没有任何作品已有 spellFlag 可借鉴** |

**结论：无。**

---

## 2. ECL 侧：符卡是怎么声明、怎么结束的

### 2.1 opcode 与参数

对 `st01bs.ecl`（SCPT，15 个 sub，sub 表在 `0x34`、名字区 `0x70`）离线解码，实测指令格式为：

```text
[time:u32][opcode:u16][size:u16][pad:u16][argc:u8][0xFF][pad:u32][args...]
                                              ^ instr+0x0B
args 从 instr+0x10 开始，每个 4 B；字符串参数 = [长度:u32][bytes...]
```

符卡相关 opcode（均走 `0x473995` 之外的“扩展 opcode”路径：`FUN_0041DCB0` 的跳表 `0x422AB8` / 字节映射 `0x422D70`）：

| opcode | decl 写法 | 处理器 VA / RVA | 语义 |
| --- | --- | --- | --- |
| `0x0219` | `spell(idBase, time, bgm, name)` | `0x421757` / `0x21757` | 关底 boss 符卡；最终 id = `idBase + difficulty` |
| `0x021A` | 未具名（本作 22 份 decl 均未出现） | 同上 | 按处理器推导 id = `idBase + difficulty − 1` |
| `0x021B` | `spell3(idBase, time, bgm, name)` | 同上 | **中boss 符卡**；id = `idBase + difficulty − 2` |
| `0x020A` | `spellEx(idBase, time, bgm, name)` | 同上 | **Extra**；id = `idBase`（不加难度，Extra 无难度） |
| `0x020B` | `spellEnd()` | `0x421891` / `0x21891` | **结算上一张卡**（见 §2.3） |
| `0x0238` | `spellMode(v)` | `0x421872` / `0x21872` | 只置 `B+0x3F8C` bit0（HUD 的“符卡模式”），**不创建符卡对象** |
| `0x021E` | —（脚本未显式使用） | `0x4218A2` / `0x218A2` | `or [obj+0x78], 8` |
| `0x021F` | — | `0x4218B0` / `0x218B0` | `or [obj+0x78], 0x10` + 停音效 |

**证据（st01bs.decl / st01bs.ecl）**

- `st01bs.decl:348` `spell(0, 1500, 0, "蝶符「ミニットスケールス」")` ↔ `.ecl` 偏移 `0x1DC8`：`op=0x0219, sz=0x3C`，args = `[0][1500][0][len=0x1C][encrypted name]`。
- `st01bs.decl:350` 是同一 id 的 `!HL`（Hard/Lunatic）变体，紧邻的下一条 `0x0219` 在 `.ecl 0x1E04`。
- `st01bs.decl:501` `spell(4, 1500, 0, ...)` ↔ `.ecl 0x299C`（BossCard2 内）。
- `st03mbs.decl:195` `spell3(20, 1320, 1, "春符「サプライズスプリング」")`（解码实测）：opcode `0x021B`，中boss 专用。
- `st07bs.decl:930` `spellEx(109, 4200, 0, ...)`：opcode `0x020A`。

### 2.2 每张卡的 sub 结构（**关键**）

以 `st01bs.BossCard1`（sub 偏移 `0x1AF0`，指令从头 `0x1B00`）与 decl `st01bs.decl:319-380` 对照：

```text
void BossCard1() {
    unless ($TIMEOUT == 0) goto +0x44;   // 0x0028/0x002A/0x003B/0x000D
    etCancel(640.0f); goto +0x2C;        // 0x0267 / 0x000C
    /* +0x44: */ etClear(640.0f);        // 0x0268
    @BossItemCard();                     // 0x000B call "BossItemCard"
    timerReset();                        // 0x0201
    killAllAsync();                      // 0x0015
    enmKillAll();                        // 0x020D
    /* 第二次 TIMEOUT 分支：同样 etCancel / etClear */
    spellEnd();                          // 0x020B  ← 结算“上一张”卡
    funcSet(0); reset(); playSound(27);  // 0x0278 / 0x0221 / 0x0204
    moveVel(...); moveVelTime(...); movePosTime(...);
    $MISS_COUNT = 0; $BOMB_COUNT = 0; $CAPTURE = 1;   // 0x002A/0x002B × 3
    setNext(0, 0, 1500, "Boss2");        // 0x0202
    spell(0, 1500, 0, "...");            // 0x0219  ← 声明“本张”卡
    ... 攻击本体 ...
}
```

由此得到三条硬结论：

1. **`spell()` 在本张卡的 sub 里声明本张卡**；`spellEnd()` 在同一 sub 的更前面，**结算的是上一张**。
2. **非符攻击（`Boss1`/`Boss2`）同样会调 `spellEnd()` 并重置 `$MISS_COUNT/$BOMB_COUNT/$CAPTURE`**（`st01bs.decl:197-206`），所以这三项是**每次攻击**的计数器，不是整关计数器。
3. 因此“上一张卡的结算时刻”= **下一张攻击 sub 的第一帧**（无论下一张是符卡还是非符）。

### 2.3 `spellEnd()` 的结算对象与时机

`FUN_004182F0`（`0x4182F0` / `0x182F0`）开头：

```asm
004182f5  8b 35 b0 6d 4a 00   mov esi,[0x4a6db0]      ; esi = 符卡对象
004182fb  f6 46 78 01         test byte [esi+0x78],1  ; bit0 = 存活
004182ff  0f 84 16 01 00 00   je   0x41841b           ; 不存活 → 直接返回（**幂等/空调用安全**）
```

⇒ `spellEnd()` 可以出现在任何 sub 里：没有活动符卡时是 no-op。这也解释了为什么每个攻击 sub 都敢无脑调它。

### 2.4 `_at` / `_at2` / `_Dead` 的角色

| 名字 | 角色 | 对 spellFlag 的影响 |
| --- | --- | --- |
| `BossCardN` / `BossCardNB` / `MBossCardN` | **符卡本体**（声明 + 攻击主循环） | 记录名的来源（CSV 精确匹配这些名字） |
| `BossCardN_at` / `_at2` | 由本体 `@... async` **或同步 `@...`** 派生的弹幕发射子 | **同步调用时主线程的当前 sub 会变成 `_at`** ⇒ 不能用“当前 sub 名”判卡是否结束（见 §5.2） |
| `BossCardN_Dead` | `setDeath()` 注册的符卡结束动画，**boss 活着时就会跑** | 不在 CSV 白名单；精确匹配天然排除 |
| `BossN` / `BossNB` / `MBossN` | 非符攻击 | 不产生符卡事件 |
| `BossDead` / `BossEscape` / `MBossDead` / `MBossEscape` | 关卡收尾；`BossDead` 在 `+60:` 才 `spellEnd()`，`BossEscape` 一开始就 `spellEnd()` | **最后一张卡由它们结算**（`st01bs.decl:662-700`） |

### 2.5 spell id 公式与逐卡 idBase

处理器 `0x421757` 里：

```asm
004217d5  2d 19 02 00 00      sub eax,0x219      ; 0x219 → 0
004217dc  83 e8 01            sub eax,1          ; 0x21A → 1
004217df  74 11               je  ...
004217e1  83 e8 01            sub eax,1          ; 0x21B → 2
004217e4  75 22               jne 0x421808       ; 其它（含 0x20A）→ 不调整
...
004217fc  03 0d b4 57 4a 00   add ecx,[0x4a57b4] ; difficulty
```

即 `id = idBase + difficulty − (opcode − 0x219)`，`0x20A` 不调整。逐卡 `idBase`（离线从 `.ecl` 解出，**按 sub 名**，不是 CSV 记录名）：

| 文件 | sub | opcode | idBase | 实际 id |
| --- | --- | --- | --- | --- |
| st01bs | BossCard1 | 0x219 | 0 | 0–3 |
| st01bs | BossCard2 | 0x219 | 4 | 4–7 |
| st02bs | BossCard1/2/3 | 0x219 | 8 / 12 / 16 | 8–19 |
| st03mbs | MBossCard1 | 0x21B | 20 | 18–21 |
| st03bs | BossCard1/2/3 | 0x219 | 22 / 26 / 30 | 22–33 |
| st04bs | BossCard1/2/3 | 0x219 | 34 / 38 / 42 | 34–45 |
| st05bs | BossCard1/1B/2/2B/3/3B/4 | 0x219 | 46/50/54/58/62/62/66 | 46–69（`3B` 与 `3` 同 id） |
| st06bs | BossCard1..9 | 0x219 | 70..102（步长 4） | 70–105 |
| st07mbs | MBossCard1/2/3 | 0x20A | 106/107/108 | 106–108 |
| st07bs | BossCard1..10 | 0x20A | 109..118 | 109–118 |

> **id ≠ 序号**：`BossCard3B` 与 `BossCard3` 共用 62–65；中boss 的 `spell3` 有 −2 偏移。**记录名映射必须用 sub 名，不要用 id 反推“第几张”。**

### 2.6 符卡名字是加密的（但运行期已解密）

`0x421757` 内的解密循环（`0x421790`–`0x4217AB`）：

```asm
xor al, ah          ; ah 初值 0x77
add ah, dl          ; dl 初值 0x07
add dl, 0x10        ; 每字节 +0x10
```

实测解 `st01bs.ecl 0x1DC8` 的 32 B → `蝶符「ミニットスケールス」`（CP932），与 decl 完全一致。`FUN_00417F00` 会把**解密后的名字**拷贝到 `obj+0x34`，所以运行期直接读 `obj+0x34` 即可（无需自己解密）。

---

## 3. 引擎侧：结算函数与可观测字段

### 3.1 符卡对象：全局 `0x4A6DB0`

| 项 | 证据 |
| --- | --- |
| 全局指针 | `0x4A6DB0` / RVA `0xA6DB0`；构造函数 `0x4178A0`，`malloc(0xBC)`（`0x4178A5`），`0x4178F0 mov [0x4a6db0],esi` |
| 分配时机 | 唯一调用点 `0x42D0B7`（关卡初始化路径，紧邻 `th16_patch_main` 的 `0x42D1EC`）⇒ **进关后非空、标题/菜单可能为 0** |
| 释放 | `0x417886 mov dword [0x4a6db0],0`（销毁路径）；正常游玩期常驻 |
| 大小 | `0xBC` |

**字段表（静态实测）**

| 偏移 | 类型 | 语义 | 写入点 |
| --- | --- | --- | --- |
| `+0x1C` | u32 | 音效句柄（spellEnd 时停） | 声明期 `FUN_00417F00`；结算期 `0x4218C2` 清 0 |
| `+0x24` | i32 | 符卡内已过帧数 | `0x417A0A mov [edi+0x24],eax`（每帧 +1） |
| `+0x28` | f32 | 帧数小数累加器 | `0x417A0D` |
| `+0x34` | char[0x40] | **已解密的符卡名（CP932）** | `0x417F66` 起的拷贝循环 |
| `+0x74` | i32 | **spell id（含难度）** | `0x417F63 mov [edi+0x74],ebx` |
| `+0x78` | u32 | **flags（见下）** | 多处 |
| `+0x7C` | i32 | 当前符卡奖励值 | 起始 `0x418232`；每帧递减 `0x4179AF`；失败清 0 |
| `+0x80` | i32 | 奖励基数（起始值，封顶 1e9） | `0x41823E` |
| `+0x84` | i32 | 符卡时限（帧，`spell()` 第 2 参） | `0x418206 mov [edi+0x84],esi` |
| `+0x8C` | i32 | 另一个帧计数器（每帧 +1） | `0x41793E incl [edi+0x8c]` |

**`+0x78` flags 位语义**

| bit | 值 | 语义 | 置位 | 清位 |
| --- | --- | --- | --- | --- |
| 0 | `0x1` | **符卡存活** | `0x417F76 or 3` | `0x41832F and ~1`（spellEnd） |
| 1 | `0x2` | **仍可收取（capture）** | `0x417F76 or 3` | **`and ~0x22`**：`0x40E896`（bomb）、`0x443ED0`/`0x443F6C`（miss）、`0x42516E`（timeout） |
| 2 | `0x4` | 奖励倒计时已启动 | `0x417A73` | `0x417AC1` |
| 3 | `0x8` | 脚本置位（`0x21E`） | `0x4218A7` | — |
| 4 | `0x10` | 脚本置位（`0x21F`，停音效） | `0x4218B6` | — |
| 5 | `0x20` | “60 帧内失败”标记（宽限） | `0x40E8A6`/`0x443EE0`/`0x42518F` | `0x418342`/`0x417BB3`/`0x42516E`（同 `and ~0x22`） |
| 6 | `0x40` | 奖励计时中 | `0x417BE3` | `0x417CB3` |
| 7 | `0x80` | 逃逸/超时处理过 | `0x425154` | — |
| 9 | `0x200` | 由 BGM/难度表派生 | `0x4182A4` | — |

> **核心：`bit0` = 在符卡中，`bit1` = 还没失败（仍可收取）。**

### 3.2 声明链（`spell()`）

```text
ECL 0x0219/0x021A/0x021B/0x020A
  → FUN_0041DCB0 (扩展 opcode 分发, 跳表 0x422AB8)
  → 0x421757  (解密名字, 算 id, 读 arg1/arg2)
  → FUN_00417F00 (0x417F00): 初始化对象、拷名字、写 id、写 +0x84 时限、
                             算 +0x7C/+0x80 奖励基数、置 flags |= 3、
                             清 bit3/4/7、累加“遭遇”计数
  → 0x421844 or [B+0x3F8C],1  (boss 侧符卡标记)
```

### 3.3 结算链（`spellEnd()`）

```text
ECL 0x020B
  → 0x421891  call FUN_004182F0 ; and [B+0x3F8C],~1
  → FUN_004182F0 (0x4182F0):
       if (!(obj+0x78 & 1)) return;      // 幂等
       ... 停音效/清 +0x0C/+0x1C ...
       obj+0x78 &= ~1;                   // 存活位清 0
       obj+0x78 &= ~0x20;
       if (obj+0x78 & 2) {               // ★ 仍可收取 ⇒ 收取成功
           call 0x43E080(obj+0x7C);      // 奖励分
           call 0x42BCF0(...);
           if ([0x4A6F08]+0x0C != 1) {
               累加“收取”计数：block+id*0x9C+0x958+bucket*4
               累加第二组：    block+id*0x9C+0x155C0+bucket*4
           }
           playSe(0x2E);
       } else {
           call 0x42BCF0([0x4A6DCC], 0, 1);   // 失败路径（无奖励分/无计数）
       }
       if (obj+0x78 & 0x80) playSe(0x45);
```

**这条链是“引擎自己认定收取成功/失败”的唯一判据**：`bit1` 在 ⇒ 加奖励分 + 加收取计数 + 播成功音；`bit1` 不在 ⇒ 走失败分支。

关键片段（`FUN_004182F0`，`0x41835A`–`0x4183CE`）：

```asm
0041835a  f6 46 78 02          test byte [esi+0x78],2      ; ★ bit1 = 仍可收取
0041835e  0f 84 9a 00 00 00    je   0x4183fe              ; 失败分支
00418364  ff 76 7c             push dword [esi+0x7c]       ; 当前奖励值
00418367  e8 14 5d 02 00       call 0x43e080               ; 加奖励分
0041836c  8b 0d cc 6d 4a 00    mov  ecx,[0x4a6dcc]
00418372  6a 00                push 0
00418374  ff 76 7c             push dword [esi+0x7c]
00418377  e8 74 39 01 00       call 0x42bcf0
0041837c  a1 08 6f 4a 00       mov  eax,[0x4a6f08]
00418381  83 78 0c 01          cmp  dword [eax+0xc],1
00418385  74 6d                je   0x4183f4               ; 某模式跳过历史计数
004183bd  8d 0c 90             lea  ecx,[eax+edx*4]
004183c0  8b 81 80 00 00 00    mov  eax,[ecx+0x80]         ; = 0x958（收取次数）
004183c6  3d 9f 86 01 00       cmp  eax,0x1869f
004183cb  7d 07                jge  0x4183d4
004183cd  40                   inc  eax
004183ce  89 81 80 00 00 00    mov  [ecx+0x80],eax
...
004183fe  8b 0d cc 6d 4a 00    mov  ecx,[0x4a6dcc]         ; 失败分支
00418404  6a 01                push 1
00418406  6a 00                push 0
00418408  e8 e3 38 01 00       call 0x42bcf0
```

### 3.4 三条失败路径（都清 `bit1`）

三条路径的代码形状**完全一致**（同一段被复制到多处）：

```asm
mov ecx,[obj+0x78]
test cl,1                 ; 符卡存活？
je  skip
cmp dword [obj+0x24],0x3C ; 已过 ≥60 帧？
jl  early
mov dword [obj+0x7C],0    ; 奖励清零
and ecx,~0x22             ; ★ 清 bit1(收取) + bit5
jmp store
early:
  ... 见 §3.7 ...
store:
mov [obj+0x78],ecx
```

| 失败原因 | 触发点（VA / RVA） | 同处还会写 |
| --- | --- | --- |
| **被弹（miss）** | `0x443EAB`–`0x443EE3` / `0x43EAB`（在 `th16_life_dec` 同函数内）；另一条 `0x443F51`–`0x443F7F` / `0x43F51` | `incl [mgr+0x3C]`（`0x443EE6`，`$MISS_COUNT++`）、`[mgr+0x44]=0`（`0x443EEE`，`$CAPTURE=0`） |
| **放 bomb** | 4 处：`0x40E881`–`0x40E8AC`、`0x40F0F1`–`0x40F11C`、`0x40F98B`–`0x40F9B6`、`0x410D43`–`0x410D6E`（对应 4 种机体/子机的 bomb 实现） | 各自紧接着 `incl [mgr+0x40]`（`0x40E912`/`0x40F182`/`0x40FA1C`/`0x410D73`，`$BOMB_COUNT++`） |
| **超时 / 攻击计时到** | `FUN_00425010`（`0x425010`）：`0x425139 or [host+0x526C],0x1000000`（`$TIMEOUT=1`）、`0x42516E and ~0x22`、`0x425171 [obj+0x7C]=0`、`0x42517B`/`0x42519A [mgr+0x44]=0` | — |

> 注意 **miss / bomb 的捕获清位在这些函数里，不在 thprac 已挂的 `th16_life_dec`/`th16_bomb_dec` 钩子指令上**：`th16_life_dec` 在 `0x443D3A`，而清位在同函数更后面的 `0x443EAB`；`th16_bomb_dec` 在 `0x40DB9C`，清位在 `0x40E896` 等 4 处。**所以“挂现有钩子”拿不到收取结果**，这也是方案 A 必须新增 hook 的原因。

### 3.5 计数器语义（**不要用错**）

`[0x4A6F0C]` 是“按机体+子机分块”的记录基址，块大小 `0x5318`（= `0x14C6 * 4`）；每张符卡槽 `0x9C`（= `0x27 * 4`）：

| 地址 | 语义 | 写入点 |
| --- | --- | --- |
| `base + (char+shot)*0x5318 + id*0x9C + 0x8D8` | 符卡名（CP932） | `0x417FC0`–`0x417FCC`（拷贝循环） |
| `base + (char+shot)*0x5318 + id*0x9C + 0x958 + bucket*4` | **收取次数**（`bucket` = `(flags&0x30)==0x20` ? 1 : 0） | `0x4183CE`（**仅 `bit1` 置位时**） |
| `base + (char+shot)*0x5318 + id*0x9C + 0x960 + bucket*4` | **遭遇次数** | `0x417FFA`/`0x418009`（声明时） |
| `base + (char+shot)*0x5318 + id*0x9C + 0x15538` | 第二组名字（另一套记录） | `0x418020` |
| `base + (char+shot)*0x5318 + id*0x9C + 0x155C0 + bucket*4` | 第二组收取次数 | `0x4183F2` |

结果画面 `0x417E52`/`0x417E77` 用 `"%02d/%02d"`（字符串 `0x492054`）显示 `0x958/0x960`，即 **收取/遭遇**；`0x493680` 是 HUD 的 `GAME MODE %4d/%4d`。

> ⚠️ **这两组计数器是“事后统计”，不能当实时标志**：它们只在 `spellEnd` 时 +1，且需要 `char+shot` 与 `id` 才能算地址。**实时判定请用 `obj+0x78 bit1`。**

### 3.6 引擎的 ECL 系统变量（顺带定案）

`FUN_00423810`（`0x423810`）是系统变量 getter：`index = arg + 0x2710`，跳表 `0x423DF4`。实测映射：

| ECL 名 | 变量 id | 存储位置 | getter | 写入方 |
| --- | --- | --- | --- | --- |
| `$MISS_COUNT` | −9949 | `*(i32*)(*(u32*)0x4A6DC0 + 0x3C)` | `0x423A2E` | miss 路径 `0x443EE6 incl`；ECL 每次攻击开头清零 |
| `$BOMB_COUNT` | −9948 | `mgr + 0x40` | `0x423A3C` | bomb 路径 4 处 `incl`；ECL 清零 |
| `$CAPTURE` | −9947 | `mgr + 0x44` | `0x423A4A` | miss `0x443EEE` / timeout `0x42517B`/`0x42519A` 清 0；ECL 置 1 |
| `$TIMEOUT` | −9986 | `*(u8*)(host + 0x526F) & 1`（= `host+0x526C` bit24） | `0x423900` | `0x425139 or [host+0x526C],0x1000000`；另在 `0x425001`（下一张攻击的计时重置路径）清 bit24 |
| `$SPELL_ID` | −9907 | `*(i32*)0x4A57C8` | `0x423DAD` | 符卡练习进入时写（`0x451C36`），否则 −1 |

> 这也是判断“收取失败原因”的最短路径：`$TIMEOUT` → 超时；`$MISS_COUNT>0` → 被弹；`$BOMB_COUNT>0` → bomb。三者可同时成立（同帧 bomb+miss），**建议按 miss > bomb > timeout 记优先级并保留多标记**。

### 3.7 60 帧宽限（🟡 语义待真机确认）

三条失败路径都有一致的 `if (age < 60)` 分支：

```asm
cmp [obj+0x24],0x3C
jl  early
...清 bit1...
early:
  mov eax,[0x4a6da8]
  cmp dword [eax+0x30],1
  jne skip
  or ecx,0x20          ; 只置 bit5，不清 bit1
```

⇒ **符卡开始后 60 帧内被弹/放 bomb，引擎不清 `bit1`，即仍算收取成功**（同时置 bit5 供 HUD/奖励逻辑使用）。`[0x4A6DA8]` 是玩家相关对象（`0x40D93A` 写入），`+0x30 == 1` 的确切含义未静态定死（🟡）。

> 实现上**不要自己判 60 帧**，直接读 `bit1` 即可：引擎已经把这个规则包含进去了。

---

## 4. 每帧可观测点与稳定性

### 4.1 最小读取集（全部是全局/静态地址，无需缓存宿主指针）

```text
obj = *(u32*)0x4A6DB0;                      // 符卡对象；进关后非空
if (obj) {
    active   = *(u32*)(obj+0x78) & 1;       // 是否在符卡中
    captured = *(u32*)(obj+0x78) & 2;       // 是否仍可收取（实时）
    spellId  = *(i32*)(obj+0x74);           // 含难度
    name     = (const char*)(obj+0x34);     // CP932，NUL 结尾
    bonus    = *(i32*)(obj+0x7C);
    age      = *(i32*)(obj+0x24);
}

mgr  = *(u32*)0x4A6DC0;
miss = *(i32*)(mgr+0x3C);                   // $MISS_COUNT
bomb = *(i32*)(mgr+0x40);                   // $BOMB_COUNT
cap  = *(i32*)(mgr+0x44);                   // $CAPTURE

// 超时：在现有宿主遍历里对每个 host 读
timeout = (*(u8*)(host+0x526F) & 1);        // $TIMEOUT（per-host）
```

- `obj` 的 `bit0`/`bit1`/`+0x74`/`+0x34` 都是**同一对象内的定长字段**，无指针链、无野指针风险（只要 `obj != 0`）。
- `+0x34` 名字长度 ≤ `0x40`（`+0x74` 是 id，紧邻），读到 NUL 为止；建议 `strnlen(name, 0x40)` 防御。
- `mgr+0x3C/0x40/0x44` 是全局游戏状态，生命周期 = 进程。

### 4.2 稳定性评估

| 风险 | 评估 |
| --- | --- |
| `0x4A6DB0` 为空 | 标题/菜单/关卡初始化前可能为 0 ⇒ **必须判空**；进关后非空（构造点 `0x42D0B7`） |
| 对象被重建 | 每次进关重建；**不缓存 obj 指针**，每帧重读全局即可（与现有 logger “每帧重走链”一致） |
| 字段越界 | `obj` 大小 `0xBC`，`+0x8C` 是最大偏移，安全 |
| 异步/同步 `_at` | `bit0` 是引擎状态，不受“当前 sub 名变成 `_at`”影响 ⇒ **比 sub 名稳** |
| 一帧延迟 | 采样点在 `th16_update`，与 ECL tick 的先后关系会带来 ≤1 帧偏差（与现有“当前段落”同源） |
| 同帧“结算 + 下一张声明” | 可能在同一帧完成 ⇒ 采样看不到 `bit0` 的下降沿。**用“每帧 AND 累积 bit1 + id/记录名变化”兜住**（见 §5.2） |

### 4.3 与 CSV 白名单的对齐

`docs/logger/th16_checkpoints.csv` 中 `BossCard*` / `MBossCard*` 共 **40 行**，逐关如下（第 4 列即事件 `checkpoint`）：

| 关卡 | 符卡记录名 ← sub |
| --- | --- |
| ST1 | `BossCard1→ST1 BOSS2`、`BossCard2→ST1 BOSS4` |
| ST2 | `BossCard1/2/3 → BOSS2/BOSS4/BOSS5` |
| ST3 | `MBossCard1→MID2`、`BossCard1/2/3 → BOSS2/BOSS4/BOSS6` |
| ST4 | `BossCard1/2/3 → BOSS2/BOSS4/BOSS6` |
| ST5 | `BossCard1→BOSS2A`、`BossCard1B→BOSS2B`、`BossCard2→BOSS4A`、`BossCard2B→BOSS4B`、`BossCard3→BOSS6`、`BossCard4→BOSS7` |
| ST6 | `BossCard1..5 → BOSS2/4/6/8/10`、`BossCard6..9 → SPRING/SUMMER/AUTUMN/WINTER_FINAL` |
| ST7 | `MBossCard1/2/3 → MID1/2/3`、`BossCard1..10 → END_S1..S10` |

**实现建议：用“现有 `mStageMap` 精确匹配到的 sub → 记录名”作为 `mSpellSection`，不要用 id 反推。** 原因：

- ST5 的 A/B 路线是**两个不同 sub、两个不同 id**（46–49 vs 50–53），但 `BossCard3B` 与 `BossCard3` 共用 62–65，id 无法区分；
- ST7 的 `spellEx` id 与难度无关，和前面关卡的 id 体系不同；
- `BossCard2_Dead` 等“结束动画”会被精确匹配天然排除。

**已知缺口（沿用 `th16_monitor_impl_report.md` §8 风险 8）**

1. **ST7 中boss B 路线 `MBossBCard1..3`（`st07mbs2`）不在 CSV**。它们只有 `spellMode(1)`（`st07mbs2.decl:147/256/292`），**不会创建符卡对象**（`spellMode` 只置 `B+0x3F8C` bit0）⇒ 本迭代**既不产生事件，也没有记录名**。
2. ST5 的 `BossCard3Bp` 等变体同样用 `spellMode(1)` 而不 `spell()`（`st05bs.decl:2242/2386/2451/2554`），不在 CSV，也不产生事件。
3. 这两类都建议**本期不覆盖**，在文档与代码注释里写明；若将来要覆盖，需先给 CSV 加 B 路线行 + 用 `$CAPTURE/$TIMEOUT/$MISS/$BOMB` 的累积值代替 `bit1`（因为没有符卡对象）。

### 4.4 四季 / 副季节的影响

- th16 的“季节/副季节”体现在 `SUBSHOT`（`0x4A57AC`，副季节）上；**符卡对象 `+0x74` 只由 `idBase + difficulty` 决定，与季节无关**（`spell()` 的处理器里没有读 `0x4A57AC`）。
- 副季节只影响 `THGuiSP::State(5)` 写 `SUBSHOT`（符卡练习入场）与 `THPatchSP` 的补丁偏移，**不影响收取判定**。
- 唯一与模式相关的是 `0x4A5BEC & 0x30`（历史桶 `bucket`）与 `[0x4A6DA8]+0x30`（60 帧宽限分支）；**两者都不改变 `bit1` 的“失败即清位”语义**（🟡 仅确认到静态）。

---

## 5. 可行性方案对比

### 5.1 方案 A：新增 hook

**候选 1（推荐给 A）：`0x41835A` / RVA `0x1835A`**

```asm
0041835a  f6 46 78 02   test byte [esi+0x78],2   ; 4 B，整条指令
0041835e  0f 84 9a 00 00 00  je 0x4183FE
```

- `ESI` = 符卡对象（来自 `0x4182F5 mov esi,[0x4a6db0]`）。
- 命中时：`bit1` 尚未被消费，`+0x74`/`+0x34`/`+0x7C` 全部有效 ⇒ **一次回调即可拿到 id、名字、收取位**。
- `instr_len = 4`（完整指令）。**thprac 的 EHOOK 是 INT3 + codecave，不是 5 B detour**，只要求 `instr_len` 覆盖完整指令、不切开指令。
- 每张卡只触发一次；VEH 同步回调；**零分配**（只读）。
- `EHOOK_ST`（常开）即可；若要跟 IGI 开关联动就用 `EHOOK_DY`（但本功能是日志的一部分，建议常开）。

**候选 2：`0x421891` / RVA `0x21891`**

```asm
00421891  e8 5a 6a ff ff   call 0x4182f0   ; 5 B
```

- `EDI` = boss 对象 `B`；符卡对象仍可从 `0x4A6DB0` 读。
- 缺点：此时 `ESI` 是 ECL 指令指针，不是符卡对象；必须自己读全局，且“哪张卡”要靠 logger 上一帧的记录名。
- 同样 5 B、每卡一次。

**A 的代价**：需要新增一个 EHOOK（动 `.text` 一处），要重新做真机稳定性验证；但收益是**结算时刻精确**，不受帧采样影响。

### 5.2 方案 B：纯轮询（不新增 hook）

在 `TH16Logger::Update()` 里（现有遍历之后、`mSession.Latch()` 之前）加一段状态机：

```text
obj = *(u32*)0x4A6DB0
if (obj && (*(u32*)(obj+0x78) & 1)) {
    id = *(i32*)(obj+0x74)
    if (!mSpellOpen || id != mSpellId || (spellHit && mSpellSection && strcmp(spellHit,mSpellSection))) {
        if (mSpellOpen) settle();          // 同一帧“结算+下一张”时先结上一张
        mSpellOpen = true; mSpellId = id; mSpellSection = spellHit; // 可能为 null，后续帧补
        mCaptured = true; mTimeout = mMiss = mBomb = false;
    }
    if (!mSpellSection && spellHit) mSpellSection = spellHit;   // 同步 _at 期间延后识别
    mCaptured &= (*(u32*)(obj+0x78) & 2) != 0;                  // 失败是永久清位 ⇒ AND 累积
    mTimeout |= sawHostTimeout;
    mMiss    |= mgr && *(i32*)(mgr+0x3C) > 0;
    mBomb    |= mgr && *(i32*)(mgr+0x40) > 0;
} else if (mSpellOpen) {
    settle(); mSpellOpen = false;
}
```

`settle()` 里：`mSession.PushEvent(mCaptured ? SpellGet : SpellFail, mSpellSection)`（`mSpellSection` 为空则跳过，并计一次“未映射”诊断）。

**B 的关键点**

| 点 | 处理 |
| --- | --- |
| 同步 `_at` 让当前 sub 变成 `_at` | **不用 sub 名判结束**，用 `bit0`；`spellHit` 只在看到白名单卡 sub 的帧更新，`_at` 帧保持上一个值 |
| 同帧“结算 + 下一张声明” | `bit0` 可能一直为 1；用 **`id` 变化**或 **`spellHit` 记录名变化**触发 `settle()`，并用**每帧 AND 累积的 `mCaptured`** 给出上一张的结果 |
| `$MISS_COUNT`/`$BOMB_COUNT` 被下一张卡清零 | 在卡进行期间**每帧累积**，不依赖结算瞬间的值 |
| `$TIMEOUT` 是 per-host | 在现有宿主遍历里对每个 host 读 `+0x526F` bit0，OR 进 `mTimeout`（不缓存 host 指针） |
| 符卡练习 | `spell()` 同样创建对象 ⇒ 天然支持；`$SPELL_ID` 不参与判定 |

### 5.3 对比与推荐

| 维度 | 方案 A（hook `0x41835A`） | 方案 B（轮询） |
| --- | --- | --- |
| 新增 hook | 1 个（4 B） | **0** |
| 改动面 | 新 EHOOK + logger 状态机 | 只在 `TH16Logger` 内加状态机 |
| 结算时刻 | **精确**（引擎消费 `bit1` 前） | ≤1 帧；同帧多卡靠 id/记录名切段 |
| 真机验证成本 | 需验证 hook 不破坏 `FUN_004182F0`（它被 `spellEnd` 频繁调用） | 只验证读数 |
| 与现有 stageClear 兼容 | 完全独立 | 完全独立 |
| 失败风险 | VEH 回调、指令覆盖、`instr_len` 选错会破坏结算 | 读数错只会误报，不会影响游戏 |
| 结论 | 二期备选 | **✅ 推荐一期** |

**推荐理由**：引擎已经把“是否仍可收取”**锁存**在 `bit1`（失败即永久清位），轮询不会丢信息；现有 `TH16Logger::Update()` 已经在每帧遍历宿主/线程、已经有 `mStageMap` 记录名映射；零新 hook 与口径 12 的“零新地址”哲学一致，且**最坏情况只是漏一张卡，不会影响游戏运行**。方案 A 留作真机发现“同帧丢事件”时的补丁。

---

## 6. 语义迁移与最小改动草图（**只写设计，不落代码**）

### 6.1 `spellFlag` 的语义

建议做**事件语义**：

- 每张符卡**结算时产出一次** `spellGet` 或 `spellFail`，`checkpoint` = 该卡自己的记录名（如 `ST1 BOSS2`）。
- **不是**“当前处于符卡收集中”。后者另有用途（IGI 显示），用独立的 `mInSpell` 表达，二者不要混。

理由：日志的一行一局结构里，“当前是否在符卡中”无法表达“这一张到底收没收到”；而 `stageClear` 之所以是状态，是因为它只在“退出游戏”那一刻被消费（口径 12）。`spellFlag` 的消费点是“结算”，天然是事件。

### 6.2 共享层最小改动

| 位置 | 改动 | 说明 |
| --- | --- | --- |
| `thprac_games.h` `GameLogEventType` | 增 `SpellGet`、`SpellFail` | 用两个枚举值，**不改 `GameLogEvent` 结构**（仍是 8 B：`type + checkpoint`） |
| `GameLogEventTypeName()` | 补 `"spellGet"` / `"spellFail"` | 漏 case 会落 `"?"`（`/W4` 不报 C4062） |
| `GameSessionHasAttemptEvent()`（`thprac_games.cpp:1458`） | **必须收窄**为 `Miss`/`Bomb`/`Break` | 否则新事件会被当成“尝试级事件”，**口径 13 的 `got` 会被打成恒 0、单章节合并判据失真** |
| 可选：`GameSessionData` | 增 `spellCount` / `spellGetCount` | 仅在做“符卡收取率”汇总时需要；本期可不动 |
| 可选：`GameSessionHasInterruptEvent()` | 不变 | 与 spell 无关 |

> 若不想动共享枚举，也可只在 th16 内用 `PushEvent(Miss/Bomb)` 变体——**不推荐**，会把“符卡失败”混进 miss/bomb 统计，破坏 IGI 对照与口径 13。

### 6.3 `TH16Logger` 的最小成员与流程

```cpp
// 新增私有成员
bool mSpellOpen = false;
const char* mSpellSection = nullptr;   // 指向 mEntries 的冻结串
int32_t mSpellId = -1;
bool mSpellCaptured = true;            // 每帧 AND (obj+0x78 & 2)
bool mSpellTimeout = false;            // OR host+0x526F bit0
bool mSpellMiss = false;               // OR mgr+0x3C > 0
bool mSpellBomb = false;               // OR mgr+0x40 > 0
```

- `OnStageStart()`：全部复位（与 `mStageClear = false` 同处）。
- `Update()` 换关块（`stageNum != mBuiltStage`）：全部复位。
- `Update()` 的宿主遍历：新增 `const char* spellHit = nullptr;`，在 `it != mStageMap.end()` 分支里按名字前缀 `Boss`/`MBoss` + 是否含 `Card` 归类（或用 `IsSpellCardSub()` 精确匹配 `^M?BossCard`），命中即 `spellHit = it->second`；同时 `sawHostTimeout |= (hostByte[0x526F] & 1)`。
- `Update()` 段落裁决之后、`Latch()` 之前：跑 §5.2 的状态机。
- **不要**把 spell 状态与 `mStageClear` 互相复位；二者独立。

### 6.4 与现有口径的兼容

| 口径 | 影响 |
| --- | --- |
| 口径 12（stageClear / interrupt） | 不受影响；`mStageClear` 的复位条件不引用 spell 状态 |
| 口径 13（单章节判据 / `got`） | **必须**把 `SpellGet/SpellFail` 从 `hasAttempt` 里排除，否则练习局每次符卡都会把 `got` 打 0 |
| 口径 14（合并行规范化） | 合并行 `events` 固定 `[]`，spell 事件同样被丢弃（与 miss/bomb 一致），无需额外处理 |
| 事件上限 256 | 一局最多 40 张卡，远小于上限；`eventsDropped` 逻辑不变 |

---

## 7. 风险与未验证项（🟡）

| # | 项 | 现状 / 已排除的可能 | 下一步最小验证（真机读哪里、期望值） |
| --- | --- | --- | --- |
| 1 | `0x4A6DB0` 在标题/菜单为 0 | 静态：唯一构造点 `0x42D0B7`（关卡初始化），销毁 `0x417886` | 标题界面读 `*(u32*)0x4A6DB0`，期望 `0`；进关后期望非 0 |
| 2 | `bit1` 就是“收取成功” | 静态：三条失败路径都 `and ~0x22`；`FUN_004182F0` 用 bit1 决定加奖励分/加收取计数/播成功音 | 打 ST1 BOSS2（`BossCard1`）：不 miss 不 bomb 击破 → 结算帧 `obj+0x78 & 2` 仍为 1；`...&1` 变 0 |
| 3 | bomb 是否清 `bit1` | 静态：4 处 bomb 路径都清，并 `mgr+0x40++` | 在卡内放 bomb：期望 `bit1` 立即变 0、`*(i32*)(mgr+0x40)` +1 |
| 4 | miss 是否清 `bit1` | 静态：两处 miss 路径都清，并 `mgr+0x3C++`、`mgr+0x44=0` | 卡内被弹：期望 `bit1` 变 0、`mgr+0x3C` +1、`mgr+0x44` = 0 |
| 5 | 超时是否清 `bit1` | 静态：`0x42516E` 清，`0x425139` 置 `$TIMEOUT` | 卡内拖到计时结束：期望 `host+0x526F & 1` = 1、`bit1` = 0、`+0x7C` = 0 |
| 6 | 60 帧宽限 | 静态：`age < 60` 时不清 `bit1`（只置 bit5） | 卡开始后 <1 s 内被弹：期望 `bit1` **仍为 1**；>1 s 再被弹：期望 0。若与预期相反，说明 `[0x4A6DA8]+0x30` 条件在真机的取值不同，需改读 `bit5` 做辅助 |
| 7 | 同帧“结算 + 下一张” | 静态：`spellEnd` 在下一张 sub 开头，与 `spell()` 同帧完成是可能的 | 连续符卡（ST1 BOSS2→BOSS4）：在结算帧单步，确认 `+0x74` 是否已变成下一张的 id；据此决定是否必须用“累积 + id 切段” |
| 8 | `$TIMEOUT` 的 per-host 归属 | 静态：`host+0x526F` bit0，仅 boss host 使用 | 在 ST1 关底卡超时：分别读主线 host 与 boss host 的 `+0x526F`，期望只有 boss host 为 1 |
| 9 | ST7 B 路线中boss | 静态：`MBossBCard1..3` 只有 `spellMode(1)`，无符卡对象、无 CSV 行 | 打 ST7 中boss B 路线：确认 `0x4A6DB0 bit0` 始终为 0（即确实不产生符卡事件） |
| 10 | ST5 变体 `spellMode` | 静态：`BossCard3Bp` 等 4 处 `spellMode(1)` 不声明 `spell()` | 走对应路线，确认是否真的走到该变体；若是，则本期不记录该卡 |
| 11 | Rep / 练习模式一致性 | 未验证；脚本相同，理论上一致 | 存 rep 回放 ST1，读 `bit1` 与 `+0x74`，期望与实战一致 |
| 12 | 难度对 id 的影响 | 静态：`spell` id = `idBase + difficulty` | 同一张卡在 E/N/H/L 下读 `+0x74`，期望差 0/1/2/3 |
| 13 | 变体 exe（Steam/汉化） | 未核 | 按变体重跑 `0x4A6DB0`/`0x41835A` 的字节比对 |
| 14 | `$CAPTURE` 与 bomb | 静态：bomb **不**清 `mgr+0x44`，但清 `bit1`；`BossItemCard` 只看 `$CAPTURE` | 卡内放 bomb 后看掉落：若仍掉道具，说明“收取”与“掉落”在 th16 是两套规则；**我们的 spellFlag 以 `bit1` 为准** |

> 上面 1–8 是**落地前必须过**的最小集；9–14 是边界。

---

## 8. 最终落地（2026-10-20，口径 15）

### 8.1 需求收敛

用户 2026-10-20 明确：**不需要区分收取/失败，只要「一张符卡结束了」这个事件**。用途是把 `stageClear` 升级成 `spellFlag`，修「`BossCard` checkpoint 在卡结束后不立刻切换 ⇒ 退出被误记 `interrupt`」。

⇒ 本文 §0/§6 原先的 `SpellGet` / `SpellFail` 事件方案**不采用**：不新增 `GameLogEventType`、不改 `GameSessionHasAttemptEvent()`、共享层零改动。

### 8.2 实现（`thprac/src/thprac/thprac_th16.cpp`）

| 项 | 做法 |
| --- | --- |
| 检测 | `SampleSpellCard()` 每帧读 `*(u32*)0x4A6DB0`；`+0x78 & 1` 的 **1 → 0** 下降沿 ⇒ `++mSpellFlag`（零新 hook） |
| 消费 | `Update()` 段落裁决之后，`mSection` 变化（沿用 `prevSection` + `strcmp`）⇒ `--mSpellFlag`（下限 0） |
| interrupt | `mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame)` |
| 复位 | `OnStageStart()` / 换关块 / 符卡对象指针变化 ⇒ 三者归零；对象为空只清采样状态，**保留计数**（退出帧还要用） |
| 顺序 | 采样在 interrupt **之前**；`++` 在 `Update()` 顶部、`--` 在末尾（同帧净 0）；换关复位在采样之后 |
| 兜底 | **保留** `mStageClear` / `IsStageClearSub`（方案 A）：覆盖 `BossDead` 的 `+60` 帧窗口与非符期 `BossEscape` |

### 8.3 覆盖性（静态）

- th16 的 `setNext` 链是「非符 → 符卡 → 非符 → 符卡」交替；对 `st01bs`–`st07bs`、`st03mbs`、`st07mbs`、`st07mbs2` 共 10 份 `.decl` 做 card→card 检测，**结果为 0** ⇒ 每张卡结束后 bit0 至少保持 0 到下一个非符结束，一帧一次采样不会漏。
- `spell`/`spell3`/`spellEx` 共用 `0x4A6DB0`，中boss 与 Extra 符卡同样覆盖。
- ST5 B 路线终符 `BossCard4B` 只有 `spellMode(1)`（无 `spell()`、无对象），本身不产生事件；但前面的 `BossCard3B` 有 `spell(62)`，且 B 路线此后没有 CSV checkpoint ⇒ 计数滞留到退出，能兜住。登记 `knownBugs.md` #12。

### 8.4 真机验证项（🟡，本机无编译环境，未编译/未真机）

1. 标题读 `*(u32*)0x4A6DB0` = 0；进关后非 0。
2. ST1 `BOSS2` 卡结束帧：`bit0` 变 0、`spellFlag` +1。
3. ST1 连续符卡：非符期 `spellFlag` 应为 0（同帧「卡结束 + 新 checkpoint」净 0）。
4. **主目标**：ST1 打完 `BOSS4` 立即退出 ⇒ 无 `interrupt`。
5. boss 爆炸瞬间（`BossDead+60` 窗口）按 Esc ⇒ 仍无 `interrupt`（验证 `mStageClear` 兜底）。
6. 关底拖到非符期超时 ⇒ 无 `interrupt`（`BossEscape` 兜底）。
7. 单符卡练习：收卡 → 重试 → 退出，确认计数不跨局泄漏。
8. ST5 B 路线 / ST7 中boss B 路线：确认既无异常 `interrupt`、也无明显漏记。

### 8.5 未做

- 本机（macOS）无 MSVC：**未编译、未真机**。
- 推广到 th14/15/17/18 未做（符卡对象地址各作不同，需逐作重新定位）。

---

## 9. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **建档**。th16 `spellFlag` 静态调研：① 盘点 th16 全部 EHOOK/PATCH（无任何符卡收取/失败 hook，`0x4A6DB0` 全仓 0 引用）；② ECL 侧定案 `spell/spell3/spellEx`（`0x0219/0x021B/0x020A`）与 `spellEnd`（`0x020B`→`0x421891`→`FUN_004182F0`），并厘清“`spellEnd()` 结算上一张、每张卡开头都会调”；③ 引擎侧定案符卡对象 `0x4A6DB0`（`0xBC` B）与字段表，**`+0x78` bit0 = 存活、bit1 = 仍可收取**；miss/bomb/timeout 三条路径均 `and ~0x22` 清 bit1；④ 定案系统变量 `$MISS_COUNT=mgr+0x3C`、`$BOMB_COUNT=mgr+0x40`、`$CAPTURE=mgr+0x44`、`$TIMEOUT=host+0x526F bit0`、`$SPELL_ID=0x4A57C8`；⑤ 方案 A（hook `0x41835A` 4 B / `0x421891` 5 B）与方案 B（纯轮询）对比，**推荐 B**；⑥ 给出 `GameLogEventType` 增 `SpellGet/SpellFail` + **必须收窄 `GameSessionHasAttemptEvent`** 的最小改动草图。**全部为静态结论，未真机（🟡）**。 |
| 2026-10-20 | **需求收敛 + 落地（§8，口径 15）**：用户明确只要「符卡结束」、不区分收取/失败 ⇒ §6 的 `SpellGet/SpellFail` 事件方案**不采用**（共享层零改动）。实现改为 `TH16Logger::SampleSpellCard()` 轮询 `*(u32*)0x4A6DB0 + 0x78` bit0 的 1→0 下降沿 ⇒ `mSpellFlag++`；`mSection` 换新 checkpoint ⇒ `--`；interrupt 条件 `mSpellFlag <= 0 && !mStageClear`，**保留 `mStageClear` 兜底**。静态补证：10 份 boss/mbs `.decl` 无 card→card，轮询不会漏。§0 的「推荐 / 语义」两行同步。**本机无编译环境：未编译、未真机（🟡）**。 |

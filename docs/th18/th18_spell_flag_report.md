# TH18 符卡结束（spellFlag）— 静态逆向报告

> 上位文档：`docs/req_logger.md`（口径 10 / 12 / 13 / **15**）、`docs/logger/logger_re_disasm_overview.md`（术语与地址口径）、`docs/logger/logger_impl_report.md`（§4.4.2 th16 spellFlag 的 9 处接线）
> 需求来源：用户 2026-10-20 需求——把 th18 的 `stageClear` 升级为 **`spellFlag`**：只在「一张符卡结束」时触发（**不区分收取 / 失败**）；`spellFlag++` → `mSection` 换新 checkpoint 时 `--` → 退出时 `spellFlag > 0` 不记 `interrupt`；`mStageClear` **保留为兜底**。
> 模板报告（结构 / 结论口径完全对齐）：`docs/th16/th16_spell_flag_report.md`（§2 ECL 侧、§3 符卡对象、§4 轮询可行性、§8 最终落地）
> 同作既有逆向：`docs/th18/th18_ecl_re_report.md`（Q1–Q9：`res=*(u32*)(host+0x1218)`、`thread+4`=当前 sub、`thread+0x1018`=host、命中点 `0x48C822`）、`docs/th18/th18_boss_phase_report.md`
> 二进制指纹：`../exes/th18.exe`，**847360 B**，PE `TimeDateStamp = 1618619144`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；`.text` = VA `0x401000`–`0x4AD000`，vsize `0xAC000`、raw `0xAB800`）
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` **只读静态反汇编**；按 `RELOCS_STRIPPED` 用「4 B 小端 VA 立即数」做字节搜索定位 xref；opcode→case 用**字节映射表 `0x4366F4` + 跳表 `0x436438`** 手工解码；ECL 侧离线解析 `../exes/ecl_dump/th18/*.decl`（22 份）+ `../exes/ecl/th18/*.ecl`
> 状态：**静态定案（🟡 未真机）**；**2026-10-20 已按 §5 落地代码（方案 A，未编译 / 未真机）**。本机 macOS，无 MSVC、不可编译、不可真机；凡未经真机读数的取值一律标 🟡。
> 地址口径：正文以 **VA** 为主，同时标注 **RVA = VA − 0x400000**。
> 改动范围：建档时为只读；落地时只改 `thprac/src/thprac/thprac_th18.cpp` 与本文件的「变更记录 / 落地标注」，未改共享层与其它文件。

---

## 0. 结论速览

| 项 | 结论 |
| --- | --- |
| 现有符卡 hook | **无**。`thprac_th18.cpp` 共 94 个 EHOOK/PATCH（5 个 `HOOKSET_DEFINE`）；全仓库 `grep 4cf2c0` **0 命中** ⇒ 没有任何 hook 读写符卡对象。既有名为 “card” 的 hook 全部是**能力卡（ability card）**，与本需求无关（§1） |
| 符卡对象全局 | **`0x4CF2C0` / RVA `0xCF2C0`** —— 持有**指针**（不是对象本体）。`operator new(0xC0)` @ `0x443040`，写回全局 @ `0x44308C`；销毁路径先把全局置 0 @ `0x429E7C` 再 `operator delete` @ `0x429D80` |
| 对象大小 / 关键字段 | `0xC0`；`+0x34` 已解密卡名（CP932，NUL 结尾）、`+0x74` spell id、**`+0x78` flags**、`+0x7C` 当前奖励、`+0x84` 时限（帧） |
| 「符卡存活」标志 | **`*(u32*)(*(u32*)0x4CF2C0 + 0x78) & 1`（bit0）** —— 与 th16 的 `+0x78` bit0 **同偏移同语义** |
| 置位 / 清位唯一性 | ✅ 置位**唯一** `0x42A380 or eax,3` + `0x42A388 mov [edi+0x78],eax`（`FUN_0042A320`）；清位**唯一** `0x42A7C2 and dword [edi+0x78],0xFFFFFFFE`（`FUN_0042A780` = spellEnd 核心）。全 `.text` 对 `[reg+0x78]` 的 30 处写点逐一比对（18 处属符卡对象），无第二处触碰 bit0（§2.3） |
| `spell` / `spell3` / `spellEx` 是否共用对象 | ✅ **共用**。opcode `0x219`/`0x21A`/`0x21B`/`0x20A` 在字节映射 `0x4366F4` 上映射到**同一个索引 `0x43`** ⇒ 同一 case `0x434EBA` ⇒ 同一 `0x42A320`、同一 `[0x4CF2C0]` |
| ECL 侧 opcode | `spell` = **`0x219`**（decl `ins_537`）、`spellEx` = **`0x20A`**（`ins_522`）、`spellEnd` = **`0x20B`**（`ins_523`）；本作 22 份 decl **未使用** `0x21A`/`0x21B`（但处理器支持，id 分别 −1 / −2） |
| 处理器地址 | `0x20A/0x219/0x21A/0x21B` → case `0x434EBA` / RVA `0x34EBA` → `call 0x42A320` @ `0x434F97`；`0x20B` → case `0x434FEC` / RVA `0x34FEC` → `call 0x42A780` @ `0x435050` |
| `spellEnd` 结算对象 | ✅ **结算「上一张」**。`0x42A780` 开头 `test byte [edi+0x78],1; je 0x42A96B`（幂等/空调用安全）；每个攻击 sub **除本关第一个非符外**都在开头调 `ins_523()` |
| 轮询可行性 | ✅ **可行**。22 份 decl、52 个 card sub 逐一枚举 `ins_514(setNext)` 与同步 `@...()` 调用：**card→card = 0**（§4）⇒ bit0 下降沿在每个真实的卡结束后至少保持 0 到下一个非符阶段结束，60 Hz 采样不会漏卡 |
| 方案选择 | **纯轮询**（零新 hook、共享层零改动、无新事件类型），与 th16 §8 完全同构；hook 方案（挂 `0x42A7C2` 前的 `test`）本迭代不需要 |
| 与 th16 的接线差异 | **9 处接线逐点同构**，只有两处取值不同：`SPELL_OBJ_PTR = 0x4cf2c0`（th16 `0x4a6db0`）、`kSpellFlagsOff = 0x78`（**巧合相同**）。**可以照搬**（§5） |
| 覆盖缺口（计数滞留 ⇒ `interrupt` 假阴性） | ST2 `Boss3`、ST6 `Boss5`/`Boss6`、ST7 `Boss9`、ST7 中boss `MBoss2`/`MBoss3` **不在 CSV**；这些非符段（`setNext` 第 3 参 `2400`–`5940` 帧）里 `spellFlag` 会滞留 >0 ⇒ 期间退出不记 `interrupt`。与 th16 的 `knownBugs.md` #12 同类（§6.2） |
| 兜底 | `mStageClear` 必须保留：关底 `BossDead` 的 `spellEnd()` 在 **`+60:`** 才执行（th18 同 th16），非符期 `BossEscape` 也走 `ins_523()`（空调用） |
| 落地状态 | ✅ **已落地（方案 A）**：2026-10-20 按 §5 的 9 处接线写入 `thprac/src/thprac/thprac_th18.cpp`（本文件 §5 表格里的行号为**建档时**行号，落地后的实际行号见 §5 开头）。**未编译 / 未真机**（本机 macOS 无 MSVC），§7 的真机项全部仍为 🟡 |

---

## 1. 现有 hook 盘点（`thprac/src/thprac/thprac_th18.cpp`）

### 1.1 总量与分组

文件内 `EHOOK_*` / `PATCH_*` / `HOOKSET_DEFINE` 命中 **99 行**，其中 `HOOKSET_DEFINE` 5 处（`:1942` `th18_master_disable`、`:1948` `th18_bossmovement`、`:4004` `THMainHook`、`:4429` `THInGameInfo`、`:4539` `THInitHook`），实际 hook/patch **94 个**。

| 分组 | 代表项（VA） | 与符卡状态有关？ |
| --- | --- | --- |
| 静态 `EHOOK_ST` / `PATCH_ST` | `th18_free_blank` `0x411F4B`、`th18_pause_skip_1/2`、`th18_shop_disable` `0x4181FF`、`th18_shop_escape_1/2` `0x4181F9`/`0x418402`、`th18_all_clear_bonus_1..3` `0x4448AB`/`0x444AFA`/`0x444C49`、`th18_score_uncap_replay_disp` `0x468405`、`th18_bossmovedown` `0x433347`、`th18_rep_card_fix` `0x462E4B`、`th18_*mallet*` `0x429222`/`0x42921D`、`th18_restart_reset_koishi` `0x42ABA9` | **否** |
| `THMainHook`（`:4004`） | `th18_inf_lives` `0x45D1A0`、`th18_param_reset` `0x465ABD`、`th18_prac_menu_1..4` / `enter_1/2`、`th18_bgm` `0x444370`、`th18_restart` `0x4594B7`、`th18_replay_restart` `0x417A70`、`th18_exit` `0x459562`、`th18_stage_transition` `0x443E60`、`th18_scroll_fix` `0x407E05`、`th18_active_card_fix` `0x462F33`、**`th18_update` `0x4013F5`**、`th18_render` `0x401510` | **否**（`*_card_*` 全是能力卡；`th18_update` 只是每帧派发 `TH18Logger::Update()` 的采样点） |
| `THInGameInfo`（`:4429`） | `th18_force_card` `0x417310`、**`th18_game_start` `0x44278F`**（→ `OnStageStart()`）、`th18_bomb_dec` `0x4574D3`、`th18_cylinder` `0x410F22`、`th18_life_dec` `0x45D1A3`、`th18_deadbomb1/2`、`th18_lock_timer1..4` `0x43A836`/`0x4389F0`/`0x433BA6`/`0x42EF1D` | **否**（miss/bomb 的**清收取位**在同函数的更后面：`0x45D4A6`/`0x40A3CD`/`0x4574xx`…，hook 指令本身不读 `+0x78`） |
| `THInitHook`（`:4539`） | `th18_disable_demo`、`th18_gui_init_1/2` | 否 |
| HOTKEY（`PATCH_HK` / `EHOOK_HK`） | `0x45D4EA`、`0x4574D3`、`0x40A3ED`、`0x40A42C`、`0x45C244`、`0x40D96F`、`0x418496`、`0x418465`、`0x418225`、`0x41842A`、`0x45C2BD`、`0x45C0E3`、`0x45748E`、`0x418283`、`0x410E7A`、`0x418427`、`0x429EEF`、`0x43021B`、**`0x48C822`**（冻结 ECL 时间，`Edi` = thread） | **否**（`0x48C822` 只写 `thread+0`，见 `th18_ecl_re_report.md` §6） |

**判据**：`grep -n "4cf2c0\|4CF2C0" thprac/src/**` → **0 命中**；`thprac_th18.cpp` 里唯一的 “Spell” 标识符是符卡练习菜单的 `mSpellId`（`:1774`–`:1874`，读 `0x4B77F0` 的练习列表 / 游标），与「一局内的卡结算」无关。

### 1.2 容易混淆的三样东西（必须分清）

| 名字 | 地址 | 是什么 | 与 spellFlag |
| --- | --- | --- | --- |
| 符卡对象 | **`0x4CF2C0`** | 本需求的符卡（spell card）单例指针 | ✅ 唯一数据源 |
| 能力卡管理器 | `ABILITY_MANAGER_PTR = 0x4CF298`、`CARD_DESC_LIST = 0x4C53C0`、`CARD_PRICE_TABLE = 0x4B35C4` | th18 的「18 张能力卡」商店/装备系统 | ❌ 无关 |
| 符卡练习 | `0x4B77F0`（`spell_id = GetMemContent(0x4b77f0, GetMemContent(0x4b77f0,0x24)*4+0x5ddc)`，`:1778`） | 符卡练习菜单的选中项 | ❌ 无关 |

> 本作 hook 名里的 `card`（`th18_force_card` / `th18_add_card` / `th18_active_card_fix` / `th18_rep_card_fix` / `th18_scroll_fix`）**全部指能力卡**；不要按名字把它们当成符卡 hook。

---

## 2. 符卡对象（引擎侧）

### 2.1 全局指针与生命周期

| 项 | VA / RVA | 证据（指令） |
| --- | --- | --- |
| 全局指针 | **`0x4CF2C0` / `0xCF2C0`** | `0x434F88 mov ecx,[0x4cf2c0]`（声明）、`0x42A787 mov edi,[0x4cf2c0]`（结算）等 40+ 处读 |
| 分配 | `0x443040` / `0x43040` | `push 0xc0; call 0x48dc71`（`0x48DC71` = operator new，内含 `new_handler` 循环） |
| 清零 | `0x443059` / `0x43059` | `push 0xc0; push 0; push edi; call 0x48f8f0`（memset） |
| 写回全局 | **`0x44308C` / `0x4308C`** | `89 3d c0 f2 4c 00` = `mov [0x4cf2c0],edi` —— **全 `.text` 唯一写点** |
| 销毁 | `0x429D80` / `0x29D80`（调用者 `0x443670` / `0x43670`） | 先 `0x429E7C c7 05 c0 f2 4c 00 00…` = `mov [0x4cf2c0],0`，再 `0x429E86 call 0x48dca1`（operator delete）；调用侧 `0x44365B mov ecx,[0x4cf2c0]; test ecx,ecx; je …; push ecx; call 0x429d80` |
| 对象大小 | `0xC0` | 分配 `0xC0`；最高写入偏移 `+0xB4`（`0x42A5D5 mov [edi+0xb4],eax`） |
| 创建 / 销毁的时机 | 🟡 **进关初始化路径** | 两者都在函数 `0x4424E0`（SEH 序言 `push ebx; mov ebx,esp; …`）内，且该函数同时重置 boss 宿主链（`0x44301F mov [mgr+0x18c],0`、`0x443029 mov [mgr+0x190],0`）；`th18_game_start` 钩子点 `0x44278F` 也在此函数内 ⇒ 换关 / 重试重建。**「每关重建」为静态强推断，🟡 待真机确认指针是否每关变化** |

**关键推论（决定了 §5 的复位逻辑）**：销毁路径**先把全局置 0、再释放对象**。因此「对象被销毁」在我们每帧一次的采样里表现为 **`obj == 0`**，而不是「`obj` 非空但 bit0 变 0」⇒ **不会产生假下降沿**（与 th16 的处置一致：为空只清采样状态、保留计数）。

### 2.2 字段表（静态实测）

| 偏移 | 类型 | 语义 | 写入点 |
| --- | --- | --- | --- |
| `+0x24` | i32 | 与 `0x3C`(60) 比较的计数器（60 帧宽限，同 th16 形状） | 声明时清 0（`0x42A356`）；失败路径 `cmp [obj+0x24],0x3c`（`0x409AE2`/`0x40A3AD`/`0x40D951`/`0x42145E`/`0x423196`/`0x45D346`/`0x45D486`）；**递增点未定位 🟡** |
| `+0x34` | char[0x40] | **已解密卡名（CP932）** | `0x42A370`–`0x42A37B` 拷贝循环（首字节落在 `+0x34`） |
| `+0x74` | i32 | **spell id（含难度，`spellEx` 不含）** | `0x42A36D mov [edi+0x74],ebx` |
| `+0x78` | u32 | **flags** | 见 §2.3 |
| `+0x7C` | i32 | 当前符卡奖励 | `0x42A64F`；失败路径清 0 |
| `+0x80` | i32 | 奖励基数（封顶 `0x3B9AC9FF`） | `0x42A652` / `0x42A65F` |
| `+0x84` | i32 | 符卡时限（帧，`spell()` 第 2 参） | `0x42A639 mov [edi+0x84],esi` |
| `+0x8C` | i32 | 置 1（HUD/符卡模式） | `0x42A4DD` |

**`+0x78` flags 位语义（与 th16 同构）**

| bit | 值 | 语义 | 置位 | 清位 |
| --- | --- | --- | --- | --- |
| 0 | `0x1` | **符卡存活** | **`0x42A380 or eax,3` → `0x42A388`**（唯一） | **`0x42A7C2 and …,0xFFFFFFFE`**（唯一） |
| 1 | `0x2` | 仍可收取（capture） | 同上 `or 3`（与 bit0 同时） | `and …,0xFFFFFFDD`（**清 bit1+bit5**）：`0x409AE8`、`0x40A3BA`、`0x40D95E`、`0x40FB..`、`0x42064C`、`0x42146B`、`0x42266C`、`0x4231A3`、`0x45D34F`、`0x45D493` |
| 2 | `0x4` | 奖励倒计时已启动 | `0x42A001 or [edi+0x78],4` | `0x42A051 and …,0xFFFFFFFB` |
| 3 | `0x8` | 脚本置位（opcode `0x21E`） | `0x42D659 or [eax+0x78],8` | — |
| 4 | `0x10` | 脚本置位（opcode `0x21F`） | `0x42D67B or [esi+0x78],0x10` | — |
| 5 | `0x20` | 60 帧内失败宽限标记 | 失败路径 `or ecx,0x20` | `0x42A7D5 and [edi+0x78],0xFFFFFFDF`；`0x42A4D5 and eax,0xFFFFFFDF`；`0x42A14B`（`…,0xFFFFFFDF` 后 `0x42A14E` store） |
| 6 | `0x40` | 声明尾部清 | — | `0x42A4DA and eax,0xFFFFFFBF`（store 于 `0x42A4E9`） |
| 7 | `0x80` | 声明时清 | — | `0x42A383 and eax,0xFFFFFF67`（清 bit3/4/7） |

> 本迭代**只用 bit0**。bit1 的语义只作旁证：`0x42A7ED test byte [edi+0x78],2` 决定走「收取成功」分支（加分 + 记历史）还是失败分支（`je 0x42A8FD`）——与 th16 `FUN_004182F0` 完全同形。

### 2.3 置位点 / 清位点唯一性验证

**方法**：对全 `.text` 做两轮字节级扫描，再逐点反汇编确认对象来源。

1. 形如 `83 6x/4x 78 imm8`、`89 4x/7x 78`、`c7 4x 78 imm32` 的 `[reg+0x78]` 写点，共 **30 处**；逐点反汇编确认对象来源后，其中 **18 处**属于符卡对象（函数体内有 `[0x4CF2C0]` 取指）：`0x409B07`、`0x40A3CD`、`0x40D96C`、`0x40FBBF`、`0x42064C`、`0x42147E`、`0x42266C`、`0x4231B6`（以上为失败路径，清 bit1+bit5）、`0x42A001`/`0x42A051`（bit2）、`0x42A14E`（bit5）、**`0x42A388`（置 bit0+bit1）**、`0x42A4E9`（清 bit5/6 后回写）、**`0x42A7C2`（清 bit0）**、`0x42D659`/`0x42D67B`（opcode `0x21E`/`0x21F` 置 bit3/4）、`0x45D362`/`0x45D4A6`（失败路径）。其余 12 处属其它类对象（`0x4201BA` 的 `+0x78` 是 float 字段；`0x4417A7`、`0x4729BF`、`0x43A710`/`0x43FED8`/`0x4416ED`/`0x441814`/`0x4418C2`/`0x4419B2`/`0x45EFA3`/`0x48ABAA` 等均无 `[0x4CF2C0]` 取指）。
2. 形如 `and r32, 0xFFFFFFFE` 的位清除共 **100+ 处**，其中「清 `[reg+0x78]` 的 bit0」全 `.text` 只有 **两处**：
   - `0x4123DD`（`and dword [esi+0x78],0xFFFFFFFE`）—— **不是符卡对象**：它所在构造函数 `0x412380` 分配 `0xB4`、vtable `0x4B5460`，并同时清 `+0x64`/`+0x78`/`+0x8c`/`+0xa0` 四个 dword 的 bit0（不同类，勿混淆）；
   - **`0x42A7C2`**（`83 66 78 FE`）—— `FUN_0042A780` = spellEnd 核心，`edi = *(u32*)0x4CF2C0`（`0x42A787`）。

**结论**：

- **置位唯一**：`0x42A388`（声明函数 `FUN_0042A320` 尾部，`or eax,3` 后立即 store）。
- **清位唯一**：`0x42A7C2`（`FUN_0042A780`，`test bit0` 幂等门之后）。该函数**唯一调用者**是 ECL `ins_523` 的 case：`0x435050 call 0x42A780`（对 `0x42A780` 做「`e8` 相对调用」全 `.text` 字节扫描 + 4 B 立即数全节扫描，均只有这一处 / 无跳表项）。
- ⇒ **bit0 是一个干净的「1 = 卡进行中」电平**，其 1→0 边沿 = 一次卡结束。

### 2.4 声明链 / 结算链

```text
ECL 0x219 / 0x21A / 0x21B / 0x20A
  → 扩展 opcode 分发：op = word[instr+4]；idx = byte[0x4366F4 + (op-0x12C)]；
    jmp dword [0x436438 + idx*4]
  → case 0x434EBA（四个 opcode 共用，idx=0x43）
      · 名字解密循环（0x434EC2 起，`mov ah,0x77` / 每字节 +0x10）
      · 计算 id：0x434F1E sub eax,0x219 → je +0 / −1 / −2；0x20A 不调整
        （难度由 `0x42AA20`（ecx=0x4CCCC0）返回，仅 0x219/0x21A/0x21B 累加）
      · 0x434F97 call 0x42A320（ecx = [0x4CF2C0]，参数 id / name / time / bgm）
  → FUN_0042A320：写 +0x74 id、+0x34 名字、+0x84 时限、+0x7C/+0x80 奖励
                  flags: 0x42A37D 读 → 0x42A380 or 3 → 0x42A383 and ~0x98 → 0x42A388 写
                  尾部 0x42A4D5 再清 bit5、0x42A4DA 清 bit6（store 于 0x42A4E9）

ECL 0x20B（spellEnd）
  → case 0x434FEC（ability 掉落判定）→ 落到 0x43504A/0x435050 call 0x42A780
  → FUN_0042A780：
       edi = *(u32*)0x4CF2C0
       0x42A78D test byte [edi+0x78],1 ; 0x42A791 je 0x42A96B   ← 幂等：无卡则直接返回
       ... 停音效（+0x10/+0x14/+0x18）、清 +0x0C/+0x1C ...
       0x42A7C2 and dword [edi+0x78],0xFFFFFFFE                ← ★ 清 bit0
       0x42A7D5 and dword [edi+0x78],0xFFFFFFDF                ← 清 bit5
       0x42A7ED test byte [edi+0x78],2 → 成功分支（加分/记收取）／ 0x42A8FD 失败分支
```

**id 公式**（同 th16）：`id = idBase + difficulty − (opcode − 0x219)`，`0x20A` 不调整。逐卡 idBase（离线从 decl 读出，**按 sub 名**）：

| 文件 | sub → idBase | opcode |
| --- | --- | --- |
| st01bs | BossCard1 → 0、BossCard2 → 4 | `0x219` |
| st02bs | BossCard1/2/3 → 8 / 12 / 16 | `0x219` |
| st03bs | BossCard1/2/3 → 20 / 24 / 28 | `0x219` |
| st04bs | BossCard1/2/3 → 32 / 36 / 40 | `0x219` |
| st05bs | BossCard1/2/3/4 → 44 / 48 / 52 / 56 | `0x219` |
| st06bs | BossCard1..6 → 60 / 64 / 68 / 72 / 76 / 80 | `0x219` |
| st07mbs | MBossCard1/2/3 → 84 / 85 / 86 | **`0x20A`** |
| st07bs | BossCard1..10 → 87 … 96 | **`0x20A`** |

> 同一 sub 内出现 2–3 条 `ins_537/522` 是 `!EN` / `!HL` 难度分支（例：`st01bs.decl:329/331` 的 `ins_537(0, 2100, 0, …)`；`st01bs.decl:534/536` 的 `ins_537(4, …)`），**idBase 相同、运行期只有一条执行** ⇒ 一张卡只声明一次。

---

## 3. ECL 侧：opcode 定位方法与语义

> **本作没有 eclmap**（thtk12 的 th18 表不全），`.decl` 里大量指令呈 `ins_<十进制 opcode>` 形态。本节说明**如何在无 eclmap 的情况下把 opcode 号钉死**。

### 3.1 两条互相独立的取证路线

**路线 A（decl 侧）**：thtk 对未知指令按 `ins_<opcode 十进制>` 命名。

- 锚点：`thprac_th18.cpp:4491` 的既有注释 **`// SetNextPattern case 514`** 直接把 `ins_514` 钉成 `setNext`（th16 的 `setNext` 也是 `0x202` = 514）。同处 `:4495`「set boss mode case 512」= `0x200`。
- 由此得：`ins_537` = `537` = **`0x219`**、`ins_522` = `522` = **`0x20A`**、`ins_523` = `523` = **`0x20B`**。
- 行为交叉验证（与 th16 §2.2 逐点对齐，见 §3.3）：
  - `ins_523()` 出现在**每个攻击 sub 开头**、`BossEscape` **第一行**、`BossDead` 的 `+60:` 之后 —— 与 th16 的 `spellEnd()` 位置分布完全一致；
  - `ins_537/522(...)` 带 `(idBase, 时限, bgm, "卡名")` 四参、位于卡 sub 的开头 —— 与 th16 的 `spell()` 同形；
  - `ins_514(...)` 的参数形如 `(0, 0, 2100, "Boss2")` —— 与 th16 的 `setNext` 同形。

**路线 B（二进制侧）**：不依赖 decl 命名。

```asm
00430DA9  movzx edx, word [eax+4]        ; opcode
00430DB5  movsx eax, dx
00430DB8  add   eax, 0xfffffed4          ; eax = opcode - 300
00430DBD  cmp   eax, 0x2bd               ; 上界
00430DC2  ja    0x436417                 ; default
00430DC8  movzx eax, byte [eax+0x4366f4] ; ★ 字节映射（702 B）
00430DD4  jmp   dword [eax*4+0x436438]   ; ★ 跳表（113 项）
```

对目标 opcode 查表（实测）：

| opcode | `byte[0x4366F4+(op−300)]` | 跳表目标 | 语义 |
| --- | --- | --- | --- |
| `0x20A` | `0x43` | **`0x434EBA`** | `spellEx` |
| `0x219` | `0x43` | **`0x434EBA`** | `spell` |
| `0x21A` | `0x43` | **`0x434EBA`** | （id −1，本作未用） |
| `0x21B` | `0x43` | **`0x434EBA`** | （id −2，本作未用） |
| `0x20B` | `0x44` | **`0x434FEC`** | `spellEnd` |
| `0x238` | `0x6D` | `0x434FD2` | `spellMode`（只写 `B+0x4FF4`，**不建卡**） |
| `0x21E` | `0x53` | `0x435067` → `0x42D650` | `or [obj+0x78],8` |
| `0x21F` | `0x54` | `0x435077` → `0x42D670` | `or [obj+0x78],0x10` |

两条路线**互相印证**：decl 里 `ins_522/537` 的调用点在二进制里必然落到 `0x434EBA`，`ins_523` 落到 `0x434FEC`；且 `0x434EBA` 的 `sub eax,0x219` 恰好把 `0x219/0x21A/0x21B` 三分支、`0x20A` 落到默认（不加难度）。✅ 静态已证。

### 3.2 `spellEnd` 结算的是「上一张」

- 对象是**单例**：声明 `0x42A320` 无脑覆盖 `+0x34/+0x74/+0x78/+0x84`；结算 `0x42A780` 读同一地址。
- 序：`BossCardN` 的**开头** `ins_523()` → 再 `ins_514(...)` → 再 `ins_537(...)`。例 `st01bs.decl` `BossCard2`：`522: ins_523()` → `532: ins_514(…"BossDead")` → `534: ins_537(4, …)` ⇒ 该 `ins_523()` 结算的是上一段 `BossCard1` 的卡。
- **是否每个攻击 sub 开头都调**：本作**除本关第一个攻击 sub 外**全部调用（第一个 sub 此前无卡，所以「不调」也无害）：

| 关 | 第一个攻击 sub（无 `ins_523`） | 之后每个攻击 sub（有 `ins_523`） |
| --- | --- | --- |
| ST1–ST6 | `Boss1` | `Boss2..N`、`BossCard1..M` |
| ST7 | `Boss1` | `Boss2..10`、`BossCard1..10` |
| ST7 中boss | `MBoss1` | `MBoss2`、`MBoss3`、`MBossCard1..3` |
| 中boss ST1–ST6 | `MBoss1`（**且全程没有卡**） | `MBossDead` / `MBossEscape`（空调用） |

- **幂等**：`0x42A78D test…,1; je 0x42A96B` ⇒ 无卡时 `spellEnd()` 是 no-op，所以 `BossEscape` 敢在第一行就调。

### 3.3 `_at` / `_Et` / `_Dead` 的角色（简要）

| 名字模式 | 角色 | 对 spellFlag 的影响 |
| --- | --- | --- |
| `BossCardN` / `MBossCardN` | **符卡本体**（声明 + 攻击主循环） | 记录名的来源（CSV 精确匹配这些名字） |
| `BossCardN_at` / `_at2` / `_at2a` / `_at2b` | 弹幕发射子脚本；既有 `@…() async`（另起线程）也有**同步 `@…()`**（例 `st03bs BossCard2` 同步调 `@BossCard2_at2a()`） | **不能用「当前 sub 名」判卡是否结束**（同步调用期间 `thread+4` 会变成 `_at*`）；但 bit0 轮询不受影响 |
| `BossCardN_Et` / `_et` / `_et2` | 由 `ins_300("BossCardN_Et", …)` 拉起的效果/演出**独立敌脚本**（不是 boss 线程的 sub）；`_Et` 不在 CSV | 不产生卡事件 |
| `_move` / `_wait` / `_circle`（仅 ST7） | 计时/位移辅助 sub（`@…() async`） | 不产生卡事件 |
| `_Dead` / `setDeath` 动画 | 本作**没有** `BossCardN_Dead` 这种卡结束动画；`ins_556("…")` 只用于小怪死亡演出（`st01.decl DeadAttack1`、`st03bs BossPhantom01_DeadFunc`） | 与 `interrupt` 判据无关；`IsStageClearSub` 的精确匹配天然排除 |

---

## 4. 轮询可行性：`setNext` 链逐份核对

### 4.1 方法

对 22 份 `.decl` 逐份解析：

1. 按 `^void <name>(` 切 sub；
2. 抽每 sub 内的 `ins_514(...)`（`setNext`）末位字符串参数；
3. 抽每 sub 内的同步 `@Name(...)` 调用（排除 `async` 之后的语义差别，只取名字）；
4. 判定「source sub 名含 `Card` 且 target 名含 `Card` 且不是同一张卡的 `_at*/_Et/_move/_wait/_circle` 辅助」⇒ card→card 边。

### 4.2 结果：**card→card = 0**

| 文件 | card sub 数 | card→card（`setNext`） | card→card（同步 `@card()`） |
| --- | --- | --- | --- |
| st01bs | 2 | 0 | 0 |
| st02bs | 3 | 0 | 0 |
| st03bs | 3 | 0 | 0（`@BossCard2_at2a` 是同卡 helper） |
| st04bs | 3 | 0 | 0 |
| st05bs | 4 | 0 | 0 |
| st06bs | 6 | 0 | 0（`@BossCard6_circle/at*` 全是同卡 helper） |
| st07bs | 10 | 0 | 0（`@BossCard10_move/wait/at*` 同上） |
| st07mbs | 3 | 0 | 0 |
| **合计** | **52** | **0** | **0** |

典型链（每关都是「非符 → 卡 → 非符 → 卡 … → `BossDead`」，**最后一张一定是卡**）：

```text
ST1: Boss1 → BossCard1 → Boss2 → BossCard2 → BossDead
ST2: Boss1 → BossCard1 → Boss2 → BossCard2 → Boss3 → @BossCard3() → BossDead
ST6: Boss1 → BossCard1 → Boss2 → BossCard2 → Boss3 → BossCard3 → Boss4 → BossCard4
     → Boss5 → BossCard5 → Boss6 → BossCard6 → BossDead
ST7: Boss1 → BossCard1 → … → Boss9 → BossCard9 → Boss10 → BossCard10 → BossDead
ST7mbs: MBoss1 → MBossCard1 → MBoss2 → MBossCard2 → MBoss3 → MBossCard3 → MBossDead
```

> 注意 ST2 的最后一跳是**同步 `@BossCard3()`**（`st02bs Boss3` 尾部），不是 `setNext`；这类「同步调用」在本报告里也逐份检查过，**没有**一张卡直接同步调另一张卡。

### 4.3 为什么「0 条 card→card」就足够

- bit0 只在 `spellEnd` 清 0，而 `spellEnd` 位于**下一个攻击 sub 的开头**。
- 若下一个 sub 是非符（本作 100% 是非符），则 bit0 至少保持 0 到该非符阶段结束（文件里 `ins_514` 的第 3 参数为 `2100`–`5940` 帧，≈35–99 s）⇒ 60 Hz 采样必然看得到 0。
- 若存在 card→card，则「上一张结算」与「下一张声明」可能同帧完成，bit0 全程为 1，会丢一次 `++`。本作**静态不存在**这种情况。

### 4.4 采样风险（th18 特有）

| 风险 | 评估 |
| --- | --- |
| 同步 `@…_at()` 让当前 sub 离开白名单 | **不影响**：我们只用 bit0，不看 sub 名 |
| 对象在进关/重试重建 | 指针变化 ⇒ 复位（§5.2 的 `obj != mSpellObj` 分支，及 §5.1 #6/#8）；销毁路径先置 0 再释放 ⇒ 不产生假边沿 |
| 一帧延迟 | 采样在 `th18_update`，与 ECL tick 的先后可能差 ≤1 帧（与现有「当前段落」同源） |
| 同帧「结算 + 下一张」 | 静态不存在（§4.2）；仍保留 th16 的「对象变化 ⇒ 复位」与「`mSection` 变化 ⇒ `--`」两道保险 |
| 练习模式 / rep 回放 | 走同一 opcode / 同一对象 ⇒ 天然支持（🟡 未真机） |

---

## 5. 接入方案（方案 A：纯轮询；**已落地** 2026-10-20，未编译 / 未真机）

> **落地完成（2026-10-20）**：下表 9 处已全部写入 `thprac/src/thprac/thprac_th18.cpp`，语义与 `thprac_th16.cpp` 逐点同构。表内行号是**建档时**的行号；落地后的实际位置为：
> `SPELL_OBJ_PTR` `:76`、`kSpellFlagsOff` `:102`、`SampleSpellCard()` 声明 `:214`、成员 `:233`–`:235`、`OnStageStart()` 复位 `:297`–`:299`、`SampleSpellCard()` 定义 `:315`–`:343`、`Update()` 采样 / 条件 `:350` / `:355`、换关复位 `:380`–`:382`、段落变化块 `--` `:507`–`:509`。
> 未新增 EHOOK / PATCH，未改共享层，未引入新事件类型；`IsStageClearSub()` / `mStageClear` / `sawStageClear` 原样保留。

### 5.1 精确插入点（对照 `thprac/src/thprac/thprac_th18.cpp` 建档时行号）

| # | 位置（建档时行号 + 语句） | 改动 |
| --- | --- | --- |
| 1 | `enum addrs`（`:44`–`:73`），末项 `WINDOW_PTR = 0x568c30,`（`:72`） | 追加 `SPELL_OBJ_PTR = 0x4cf2c0,`（并加一行注释说明「单例指针、进关重建」） |
| 2 | 匿名 namespace（`:84`–`:92`），`constexpr uintptr_t kThreadSubOff = 4;`（`:92`） | 追加 `constexpr uintptr_t kSpellFlagsOff = 0x78;`（注释：bit0 = 卡进行中，置位唯一 `0x42A388`、清位唯一 `0x42A7C2`） |
| 3 | `TH18Logger` 私有方法声明（`:201`–`:202`），`void BuildStageMap(int32_t stageNum);`（`:202`） | 追加 `void SampleSpellCard();` |
| 4 | 成员（`:215`–`:220`），`int32_t mBuiltStage = -1;`（`:215`）与 `bool mStageClear = false;`（`:219`）之间 | 追加 `int32_t mSpellFlag = 0;` `uintptr_t mSpellObj = 0;` `bool mSpellActive = false;` + 三行注释（照抄 th16 `:168`–`:175` 的中文注释，把地址换成 `0x4CF2C0`） |
| 5 | `TH18Logger::OnBomb()` 结束（`:288`）与 `TH18Logger::Update()`（`:290`）之间 | 新增 `void TH18Logger::SampleSpellCard()`（逻辑见 §5.2） |
| 6 | `OnStageStart()`（`:267`–`:276`），`mStageClear = false;`（`:275`）之前 | `mSpellFlag = 0; mSpellObj = 0; mSpellActive = false;` |
| 7 | `Update()` 顶部：`bool inGame = GetMemContent<uint32_t>(PLAYER_PTR) != 0;`（`:293`）之后、interrupt 判定 `if (!mStageClear && mSession.IsGameExitEdge(inGame))`（`:296`）之前 | 插 `SampleSpellCard();`；条件改为 **`if (mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame))`**（并补注释：`mSpellFlag > 0` = 有卡刚结束、下一个 checkpoint 还没切过来） |
| 8 | 换关块 `if (stageNum != mBuiltStage) { … }`（`:313`–`:322`），`mStageClear = false;`（`:321`）之前 | 三行复位（与 #6 同） |
| 9 | 段落变化块（`:441`–`:443`），`mStageClear = false;`（`:442`）之前 | `if (mSpellFlag > 0) { --mSpellFlag; }`（下限 0；**必须在 `mStageClear = false` 之前**） |

> 九处与 `docs/logger/logger_impl_report.md` §4.4.2 的 th16 表格**一一对应**；不需要动任何 EHOOK / PATCH，不需要新事件类型，共享层零改动。✅ **2026-10-20 已按此表落地**（见本节开头）。

### 5.2 `SampleSpellCard()` 设计（与 th16 逐行同构，只换常量）

```text
obj = *(u32*)0x4CF2C0                       // GetMemContent<uintptr_t>(SPELL_OBJ_PTR)
if (!obj) {                                 // 菜单 / 销毁中
    mSpellObj = 0; mSpellActive = false;    // 只清采样状态，★保留 mSpellFlag
    return;
}
active = (*(u32*)(obj + 0x78) & 1) != 0;
if (obj != mSpellObj) {                     // 对象重建（换关 / 重试 / 重开）
    mSpellFlag = 0; mSpellObj = obj; mSpellActive = active;
    return;
}
if (mSpellActive && !active) ++mSpellFlag;  // ★ 1→0 下降沿 = 一张卡结束
mSpellActive = active;
```

### 5.3 「段落变化」判据：与 th16 **完全一致**

| 项 | th16 | th18（本作） |
| --- | --- | --- |
| 判据 | `mSection != prevSection && strcmp(mSection, prevSection) != 0`（`thprac_th16.cpp:440`） | **同一判据**（`thprac_th18.cpp:441`） |
| `prevSection` 取值点 | 段落裁决之前 | 同（`:405`） |
| `--` 位置 | 段落变化块内、`mStageClear = false` 之前 | 同（`:441`–`:443` 之间） |
| 日志记录名 | `GameLogRecordOfSection(mSection)` | 同（`:449`） |

⇒ **不需要为本作新增判据**；`mSection` 的三种命中（`midHit`/`bossHit`/`chapHit`）逻辑、`mPending` 中boss 窗口逻辑都保持原样。

### 5.4 顺序约束（三条，缺一不可）

1. `SampleSpellCard()` 必须早于 interrupt 判定 ⇒ 同帧「清卡 + 退出」也能吃到 `++`。
2. `++` 在 `Update()` 顶部、`--` 在段落裁决之后 ⇒ 同帧「卡结束 + 新 checkpoint」净 0；反过来会残留 1，吞掉之后整关的 `interrupt`。
3. 换关复位（#8）在采样之后 ⇒ 换关时新旧对象交替产生的「假边沿」当场抹掉；对象为空时**不清 `mSpellFlag`**，否则退出帧还没用上就被清了。

---

## 6. 覆盖性（逐关）

### 6.1 「关底最后一张攻击阶段」是否 `spell()`

| 关 | 最后一个攻击 sub | 有 `spell()`？ | 之后 | 关底收尾 sub | `spellEnd` 位置 |
| --- | --- | --- | --- | --- | --- |
| ST1 | `BossCard2`（id 4） | ✅ `0x219` | `setNext "BossDead"` | `BossDead` | **`+60:` 之后**（`+60:` 在 `st01bs.decl:709`，`ins_523()` 在 `:711`） |
| ST2 | `BossCard3`（id 16） | ✅ `0x219` | `setNext "BossDead"` | `BossDead` | `+60:` 之后 |
| ST3 | `BossCard3`（id 28） | ✅ `0x219` | `setNext "BossDead"` | `BossDead` | `+60:` 之后 |
| ST4 | `BossCard3`（id 40） | ✅ `0x219` | `setNext "BossDead"` | `BossDead` | `+60:` 之后 |
| ST5 | `BossCard4`（id 56） | ✅ `0x219` | `setNext "BossDead"` | `BossDead` | `+60:` 之后 |
| ST6 | `BossCard6`（id 80） | ✅ `0x219` | `setNext "BossDead"` | `BossDead` | `+60:` 之后 |
| ST7（Extra） | `BossCard10`（id 96） | ✅ `0x20A` | `setNext "BossDead"` | `BossDead` / `BossDead2` | `+60:` 之后 |
| ST7 中boss | `MBossCard3`（id 86） | ✅ `0x20A` | `setNext "MBossDead"` | `MBossDead` | 开头附近（`:80` 偏移） |

**⇒ 每一关的最后一个攻击阶段都是符卡**，因此「关底最后一击 → 卡结束」一定有 `spell()` / `spellEnd()` ⇒ 一定有 bit0 下降沿。**没有「最后一击是非符」的洞。**

### 6.2 中boss 最后阶段

- **ST1–ST6 的中boss 全程没有符卡**（`st01mbs`–`st06mbs` 的 6 份 decl 里 `ins_537/522` 计数 = 0）。中boss 结束（`MBossDead`/`MBossEscape` 的 `ins_523()`）是**空调用**，不产生边沿。
- **ST7 中boss 有 3 张卡**（`MBossCard1..3`，`0x20A`），最后一张 → `MBossDead`；CSV 有 `MBossCard1/2/3 → MID1/MID2/MID3`（无缺口）。

### 6.3 `BossEscape` 能否在非符期触发

- `BossEscape` 在**每一份** `stNNbs.decl` 里都有一个 sub（ST1 `st01bs.decl:722`、ST7 `st07bs.decl:2574` 等），第一行即 `ins_523()`；中boss 侧对应 `MBossEscape`（`ins_521(1, "MBossEscape")` 在 7 份 `*mbs.decl` 里注册，字符串 `"BossEscape"` @ `0x4B64BC`，见 `th18_boss_phase_report.md`）。**本报告未在二进制里定位 boss 侧 `BossEscape` 的注册/触发点（🟡）**，但从 sub 存在与 `ins_523()` 的位置看，它与「当前打的是符卡还是非符」无关 ⇒ 可以在非符期触发。
- 触发时第一行就是 `ins_523()`：有卡 ⇒ 正常结算（边沿正常）；无卡 ⇒ `0x42A78D` 幂等返回（无假边沿）。**两种情况都不会误报**。
- 但「非符期 escape → 关底收尾 → 退出」这一窗口**没有卡结束事件**，只能靠 `mStageClear`（`IsStageClearSub` 命中 `BossEscape`）兜底 ⇒ **`mStageClear` 必须保留**。

### 6.4 CSV 白名单缺口（计数滞留 ⇒ `interrupt` 假阴性）

逐关比对「ECL 攻击链」与 `docs/logger/th18_checkpoints.csv`：

| 关 | 缺口（**在 ECL 链里但不在 CSV**） | 滞留区间（`setNext` 第 3 参） | 后果 |
| --- | --- | --- | --- |
| ST1 | 无 | — | 完整 |
| ST2 | `Boss3` | `2400`（`st02bs.decl:568`） | `BossCard2` 结束后 `spellFlag` 滞留到 `BossCard3` 命中 |
| ST3 / ST4 / ST5 | 无 | — | 完整 |
| ST6 | `Boss5`、`Boss6` | `3000` / `3600`（`st06bs.decl:1113` / `:1250`） | `BossCard4`、`BossCard5` 结束后各滞留一段 |
| ST7 | `Boss9` | `5940`（`st07bs.decl:2255`） | `BossCard8` 结束后滞留 ≈99 s |
| ST7 中boss | `MBoss2`、`MBoss3` | `3600` / `3600`（`st07mbs.decl:207` / `:251`） | `MBossCard1`/`MBossCard2` 结束后各滞留 ≈60 s |

**影响**：期间退出 ⇒ `mSpellFlag > 0` ⇒ **不记 `interrupt`**（假阴性）。这与 th16 `knownBugs.md` #12 的已知副作用同类；**不会**造成「多记 interrupt」（漏卡方向的错误我们没有：§4.2 证明不会漏卡）。

**CSV 侧的洞（与 th16 不同）**：本作 **22 份 decl 的 52 个 card sub 全部在 CSV 里**（ST1–ST6 的 `BossCard*`、ST7 的 `BossCard1..10`、`MBossCard1..3`），**没有 th16 那种「有卡但无 CSV 行」的记录名缺口**。

### 6.5 `mStageClear` 兜底是否足够

| 场景 | 有卡结束边沿？ | 靠什么兜住 |
| --- | --- | --- |
| 最后一击收卡 → 立刻退出（`BossDead` 前 60 帧） | ❌ **无边沿**（`spellEnd` 在 `+60:` 之后才跑） | 需 `mStageClear`；但 `BossDead` 的 `IsStageClearSub` 命中也在同一帧 ⇒ 覆盖 |
| 最后一击收卡 → `BossDead+60` 已 `spellEnd` → 退出 | ✅ 有边沿 | `spellFlag > 0` |
| 非符期 `BossEscape` → 退出 | ❌ 无 | `mStageClear`（`BossEscape` 在 `IsStageClearSub` 内） |
| 非符期（CSV 有该非符）超时 → 退出 | ❌ 无 | 无兜底 ⇒ 记 `interrupt`（这是**语义正确**的：确实没打完） |
| 非符期（CSV **缺**该非符，§6.4）→ 退出 | 计数已滞留 >0 | 假阴性（不记 `interrupt`） |

⇒ 兜底**足够**（不会漏掉「刚收完卡就退」的主诉场景），代价是 §6.4 的假阴性。

---

## 7. 风险与真机验证项（🟡）

| # | 项 | 现状 / 已排除的可能 | 真机最小验证 |
| --- | --- | --- | --- |
| 1 | `0x4CF2C0` 在标题/菜单为 0 | 静态：销毁路径 `0x429E7C` 先置 0 再 free；创建在进关路径 | 标题读 `*(u32*)0x4CF2C0`，期望 `0`；进关后期望非 0 |
| 2 | bit0 = 「卡进行中」 | 静态：置位唯一 `0x42A388`、清位唯一 `0x42A7C2`、清位函数唯一调用者 `0x435050` | 打 ST1 BOSS2：卡内 `obj+0x78 & 1` = 1，结算帧变 0 |
| 3 | `spellFlag` 计数正确 | 静态：`++` 在 `Update()` 顶部、`--` 在段落裁决后 | 连打 ST1（2 张卡）：非符期 `mSpellFlag` 应回 0；若长期 >0 说明 `--` 没生效 |
| 4 | **主目标**：收完卡立刻退出不记 `interrupt` | 设计目标 | ST1 打完 BOSS4（`BossCard2`）立即退到菜单 ⇒ 日志无 `interrupt` |
| 5 | `BossDead+60` 窗口 | 静态：`spellEnd` 在 `+60:` 之后 | boss 爆炸瞬间（60 帧内）按 Esc ⇒ 仍无 `interrupt`（验 `mStageClear`） |
| 6 | 非符期 `BossEscape` | 静态：第一行 `ins_523()`，无卡时幂等 | 关底非符期拖到逃逸 ⇒ 无 `interrupt` |
| 7 | 对象是否每关重建 | 🟡 静态推断（创建/销毁同在 `0x4424E0`） | 换关前后各读一次 `0x4CF2C0`，期望**指针变化**；若不变，`mSpellObj` 复位分支退化为「只靠 `mSection`」，仍可用 |
| 8 | 死亡/重试时是否强清 bit0 | 静态：只有 `0x42A7C2` 一处清位，重试不经过 ECL ⇒ **大概率保持不变** | 卡内自杀 → 重试 → 读 bit0：若仍为 1，说明不会产生假边沿（当前设计可接受）；若变 0，会多记一次 `++`（需靠插入点 #8 的换关复位抹掉） |
| 9 | 符卡练习 / rep 回放一致性 | 静态：同一 opcode、同一对象 | 单卡练习：收卡 → 重试 → 退出，确认计数不跨局泄漏；存 rep 回放 ST1 比对 |
| 10 | §6.4 的滞留假阴性 | 静态：CSV 缺 `Boss3`/`Boss5`/`Boss6`/`Boss9`/`MBoss2`/`MBoss3` | ST7 `Boss9` 段（≈99 s）中途退出 ⇒ 预期**不记** `interrupt`（属已知副作用，非回归） |
| 11 | 60 帧宽限（`+0x24` vs `0x3C`） | 静态：失败路径同 th16 形状；**递增点未定位** 🟡 | 与本需求无关（只看 bit0），仅记录 |
| 12 | 版本变体（Steam / 汉化） | 未核 | 按变体重跑 `0x4CF2C0` / `0x42A388` / `0x42A7C2` 的字节比对 |

> 1–6 是落地前必须过的最小集；7–12 是边界。
> 本机（macOS）**无 MSVC**：本次**未编译、未真机**。

---

## 8. 与 th16 的差异清单（供评审快速比对）

| 项 | th16 | th18（本作） | 是否影响接线 |
| --- | --- | --- | --- |
| 符卡对象全局 | `0x4A6DB0` | **`0x4CF2C0`** | 只改常量 |
| flags 偏移 / 位 | `+0x78` bit0 | **`+0x78` bit0**（巧合相同） | **常量可照抄** |
| 对象大小 | `0xBC` | `0xC0` | 否 |
| 对象生命周期 | 每关分配（`0x42D0B7`） | 进关路径里 new/free（`0x443040` / `0x429D80`），🟡 是否每关待真机 | 否（指针变化即复位） |
| 声明函数 | `FUN_00417F00`，置位 `0x417F76` | `FUN_0042A320`，置位 `0x42A380/0x42A388` | 只影响注释 |
| 结算函数 | `FUN_004182F0`，清位 `0x41832F` | `FUN_0042A780`，清位 `0x42A7C2` | 只影响注释 |
| opcode 分发 | 扩展表 `0x422AB8` / 字节表 `0x422D70` | 字节表 **`0x4366F4`**（op−300）+ 跳表 **`0x436438`** | 否 |
| `spell` / `spellEx` | `0x219` / `0x20A` | **同**（`0x21A/0x21B` 存在但本作未用） | 否 |
| 中boss 符卡 | `spell3`（`0x21B`，id −2）用于 `st03mbs`/`st07mbs` | **ST1–ST6 中boss 无卡**；ST7 中boss 用 `spellEx`（`0x20A`） | 否 |
| 「最后一张卡 → 关底」 | 各关最后一击都是卡 | **同**（§6.1） | 否 |
| card→card | 0（10 份 decl） | **0（22 份 decl / 52 card sub）** | 否 |
| CSV 有无「有卡无记录名」缺口 | 有（ST7 `MBossBCard1..3`、ST5 `BossCard3Bp` 等） | **无**（52 张卡全部有 CSV 行） | 否 |
| CSV 有无「非符漏登记」 | 有（ST5/ST7 等） | **有**：`Boss3`(ST2)、`Boss5`/`Boss6`(ST6)、`Boss9`(ST7)、`MBoss2`/`MBoss3`(ST7mbs) | 否（只是假阴性） |
| 取证难度 | 有 eclmap，decl 直接给 `spell()/spellEnd()` | **无 eclmap**：decl 呈 `ins_NNN`，需靠 ① `ins_514 = setNext` 锚点（`thprac_th18.cpp:4491`）反推十进制 opcode、② 二进制字节映射表 + 跳表交叉验证 | 否（结论等价） |

---

## 9. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-20 | **建档**。th18 `spellFlag` 静态调研（只读，未改任何代码/既有文档）：① 盘点 `thprac_th18.cpp` 94 个 EHOOK/PATCH（5 个 hookset），**无一处读写符卡状态**，`4cf2c0` 全仓 0 引用；② 定案符卡对象 **`0x4CF2C0`**（`operator new(0xC0)` @ `0x443040`、写回 @ `0x44308C`、销毁先置 0 @ `0x429E7C`）、**`+0x78` bit0 = 卡存活**，置位唯一 `0x42A388`（`FUN_0042A320`）、清位唯一 `0x42A7C2`（`FUN_0042A780`），并对全 `.text` 30 处 `[reg+0x78]` 写点逐一排除（其中 `0x4123DD` 属另一类对象，勿混淆）；③ ECL 侧在**无 eclmap** 条件下用「`ins_514`=setNext 锚点 + 字节映射 `0x4366F4`/跳表 `0x436438`」双路定案 `spell=0x219`/`spellEx=0x20A`/`spellEnd=0x20B`（`0x21A`/`0x21B` 处理器支持但本作未用），case `0x434EBA` → `0x42A320`、case `0x434FEC` → `0x435050 call 0x42A780`；确认 `spellEnd` 结算上一张、除本关第一个攻击 sub 外每个攻击 sub 开头都调、幂等；④ 逐份核对 22 份 decl 的 `setNext` 与同步 `@…()` 调用：**52 个 card sub，card→card = 0** ⇒ 一帧一采样不会漏卡；⑤ 给出 `thprac_th18.cpp` 的 **9 处精确插入点（含当前行号与语句）**，并确认「段落变化」判据与 th16 **完全一致**；⑥ 覆盖性逐关核对：每关最后一击都是符卡、ST1–ST6 中boss 无卡、ST7 中boss 有 3 张卡、`BossEscape` 可在非符期触发（`spellEnd` 幂等）；CSV 缺口为非符段（`Boss3`/`Boss5`/`Boss6`/`Boss9`/`MBoss2`/`MBoss3`）⇒ 计数滞留、`interrupt` **假阴性**（同 `knownBugs.md` #12）；⑦ 风险 12 条，全部标 🟡。**本机无编译环境：未编译、未真机。** |
| 2026-10-20 | **代码落地（方案 A）**。按 §5 的 9 处接线修改 `thprac/src/thprac/thprac_th18.cpp`（**本需求唯一改动的代码文件**）：① `enum addrs` 加 `SPELL_OBJ_PTR = 0x4cf2c0`（附分配 / 写回 / 销毁地址注释）；② 匿名 namespace 加 `constexpr uintptr_t kSpellFlagsOff = 0x78`（注释：置位唯一 `0x42A388`、清位唯一 `0x42A7C2`）；③ `TH18Logger` private 加 `mSpellFlag` / `mSpellObj` / `mSpellActive` 三成员（照抄 th16 中文注释，地址换成 `0x4CF2C0`）；④ 加 `SampleSpellCard()` 声明；⑤ 在 `OnBomb()` 之后实现 `TH18Logger::SampleSpellCard()`（空对象只清采样状态、**不清** `mSpellFlag`；对象变化清零重来；`mSpellActive && !active` ⇒ `++mSpellFlag`）；⑥ `OnStageStart()` 体末三行复位；⑦ `Update()` 顶部在 `inGame` 之后、interrupt 判定之前调 `SampleSpellCard()`，条件改为 `mSpellFlag <= 0 && !mStageClear && mSession.IsGameExitEdge(inGame)`；⑧ 换关块加三行复位（在 `mStageClear = false` 之前）；⑨ 段落变化块加 `if (mSpellFlag > 0) { --mSpellFlag; }`（在 `mStageClear = false` 之前）。**保留** `IsStageClearSub()` / `mStageClear` / `sawStageClear` 作兜底；**未新增 EHOOK / PATCH，未改共享层，未引入新事件类型**。静态扫描：`git diff --check` 对本文件无告警；剥离注释/字符串后 `()[]{}` 平衡（深度 0）；新标识符声明/定义各一处。**本机 macOS 无 MSVC：未编译、未真机**，§7 的真机项仍全部 🟡 |

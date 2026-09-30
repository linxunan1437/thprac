# 游戏时长统计 — 真机待验证清单（Runtime Verification Checklist）

> 本文档**只列"需要真机（运行中的游戏 + 启动器）验证"的部分**，是活文档：每完成一项，把实测值回填到 §4，并同步更新 `docs/gametime/gametime_thxx_flags.md` 的状态标记。
>
> - 判据 / 地址的反汇编出处与背景：`docs/gametime/gametime_thxx_flags.md`
> - 总体方案与端到端用例：`docs/gametime/gametime_plan.md`（§2.3 第 5 条、§3.4）
> - 逐作需求 / 报告：`docs/<作>/<作>_re_disasm_{requests,report}.md`（th07 / th08 已有）

---

## 0. 前置与判定标准

**为什么要真机验证**：静态反编译只能定出"地址 + 判据"，不能排除
①指针/字段在特定时刻为 0、②同一位被别的语义复用、③标题挂机 attract demo 误计 等。
`gametime_plan.md` §2.3 第 5 条明确规定：**未经三态验证的地址不得写进 gate。** 本清单就是把目前"已写进 gate（或已定案待写）但尚未验证"的条目集中列出。

**判定标准**：在"**正常游玩 / 打开暂停菜单 / 播放 replay**"三种状态下各采样一次，三个 flag 取值必须**互不相同且各自符合判据**，且 `playNs` 只在"正常游玩"时增长。

**工具 / 做法**：

1. 构建：`"D:\tools\VS\MSBuild\Current\Bin\MSBuild.exe" thprac.sln -p:Configuration=Release -p:Platform=x86 -m`；
   快速语法自检：`docs/gametime/_syntaxcheck.bat`。
2. Others 页确认 `recordGameTime` 已开。
3. 用**已实现的调试读数面板**（`THGameTimeRecorder::Gui()`，`thprac/src/thprac/thprac_launcher_others.cpp:754` 起）读数：面板显示当前绑定 `game` / `pid` / `base` / 本局 `playNs`，并按真实判定顺序逐项打印三个 flag 的**原始值与 ok/blocked**，与真实门槛路径一致。
4. 每个待验条目按 §2 的三态脚本跑，原始值记进 §4。

**2026-09-30 实际采用的方法（与上面等价，但自动化）**：沿用 th07/th08 先例，用外部 Python 探针
`D:\workshop\reasm\gametime\tools\{probe,gates,drive}.py` 采集 —— 外部 `OpenProcess` +
`ReadProcessMemory`（**与启动器读取方式完全一致**），扫描码注入驱动状态序列，每个状态截图存证，
签名日志落在 `out\<thXX>\`。探针**独立实现**了一份判定逻辑（`Eq` / `Ne` / `BitClear` 与
`ReadFlagValue` / `EvalRaw` 逐条对齐，含二级读），因此验的是**地址语义**而非生产代码路径；
且它**逐个 flag 无条件打印**，不受调试面板「遇到第一个 blocked 就停止打印」的限制 ——
暂停态下 replay 的原始值正是靠这一点才采到的。

探针**在原目录就地运行游戏**，全程不复制、不链接、不修改任何文件（尤其不触碰 replay）：
th13+ 的 replay 由游戏从 `%APPDATA%\ShanghaiAlice\thNN\replay\` 自己读取，沙箱反而会让列表变空。

**工具侧两处必要修正**：模块基址改走 `EnumProcessModules`（PEB 路径在本机返回 `0xFFFFFFFF`）；
窗口按 `BASE` 类筛选（ZUN 游戏启动时会弹一个 `#32770` 的 `log` 对话框，按「第一个可见窗口」抓会把按键全发给它）。

---

## 1. 总览

| 作品 | 代 | 判据来源 | 代码状态 | 真机状态 | 待验证内容 |
| --- | --- | --- | --- | --- | --- |
| th06 | A | 既有代码推导（无报告） | 已接入 | ⚠️ **无三态实测记录** | 三条 flag + 端到端（P0 遗留）；**本轮确认不在验证范围**（成熟代码） |
| th07 | A | 反编译报告 + 三态 | 已接入 | ✅ 2026-09-30 三态复验通过 | 续关菜单漏计（未测）；启动器侧端到端（未做） |
| th08 | A | 反编译报告 + 三态 | 已接入 | ✅ 2026-09-30 三态复验通过 | 同上 |
| th10 | B | 静态反编译 | 已接入 | ✅ 2026-09-30 三态验证通过 | 续关菜单漏计（未测） |
| th11 | B | 静态反编译 | 已接入 | ✅ 2026-09-30 三态验证通过 | 同上 |
| th12 | B | 静态反编译（本轮新增） | 已接入 | ✅ 2026-09-30 三态验证通过 | 同上 |
| th13 | B | 静态反编译（本轮新增） | 已接入 | ✅ 2026-09-30 三态验证通过 | 同上 |
| th14 | C | 静态反编译（本轮新增） | 已接入 | ✅ 2026-09-30 三态验证通过 | 同上 |
| th16 | C | 静态反编译（本轮新增） | 已接入 | ✅ 2026-09-30 三态验证通过 | 同上 |
| th17 | C | 静态反编译（本轮新增） | 已接入 | ✅ 2026-09-30 三态验证通过 | 同上 |
| th18 | C | 静态反编译（本轮新增） | 已接入 | ✅ 2026-09-30 三态验证通过 | 同上 |
| th15 | C | 静态反编译 + 复核更正 | 已接入（pause/replay 已更正） | ✅ 2026-09-30 三态验证通过 | 练习模式漏计（未测） |

> 图例：✅ 已验证 ｜ 🟡 已定案未验证 ｜ ⚠️ 有已知问题需先处理

---

## 2. 逐作待验证明细

### th06（P0 遗留，无三态记录）

代码里已填（`gGateTh06`），但**从未做过真机三态验证**（`gametime_plan.md` §3.2 P0 备注"真机三态验证待做"，且没有对应报告）。

| 量 | 判据（RVA，base `0x400000`） |
| --- | --- |
| gamemode | `0x2C6EA4 == 2`（`0x6C6EA4` gameState） |
| pause | `0x29D4BF == 0`（`0x69D4BF` isInGameMenu） |
| replay | `0x29BCBC == 0`（`0x69BCBC` isInRep） |

- [ ] 标题 / 菜单 / 选人 → `playNs` 不涨
- [ ] 进关 → 涨；按 Esc → 冻结；返回 → 恢复
- [ ] 播放 replay → `gamemode == 2` 但 `playNs` 不涨（**关键用例**）

### th07 / th08（地址已三态验证；本轮复验通过，剩启动器侧端到端）

地址本身已在 `docs/th07` / `docs/th08` 报告中三态验证；剩 P1b：验证**接入 gate 表后**的端到端行为与"续关菜单"边界。

| 作 | gamemode | pause | replay |
| --- | --- | --- | --- |
| th07 | `0x175AA4 == 2` | `0x22F64C == 0` | `(0x22F648 & 8) == 0`（bit3） |
| th08 | `0x13CE8B0 == 2` | `0x124D0BA == 0` | `(0x124D0B4 & 8) == 0`（bit3） |

- [x] th07 / th08 三态（§3 用例 2~4）2026-09-30 用探针复验通过 —— th07：进关 `gm=2 pause=0 flags=0x04` PASS / Esc `pause=2` blocked / 回放 `gm=2 pause=0 flags=0x0c`（bit3=1）blocked；th08 同构（`flags=0x38004` / `pause=2` / `flags=0x3800c`）
- [ ] §3 用例 6（会话边界）—— **未做**，属启动器侧，本轮未跑
- [ ] **续关菜单漏计确认**：死亡后进 Continue 菜单时 `pause == 0`、`gamemode == 2`、`replay` 不成立 → 会误计。实测确认后决定是否改用 bit2 强化形态（`{ 0x22F648, 4, 0x4, BitClear }` / `{ 0x124D0B4, 4, 0x4, BitClear }`）。**该强化形态本身也未经三态验证。**（本轮**未测**：需要故意死亡，未纳入自动化序列）

### th10（B 代 · 风神录）—— ✅ 三条已三态验证（2026-09-30）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x477834 != 0`（PLAYER_PTR） | 单级读 |
| pause | `[0x477810]+0x58 & 0x10 == 0`（GameManager，bit4） | 二级读 |
| replay | `[0x477838]+0x10 != 1`（ReplayManager） | 二级读 |

- [x] 三态各采一次，确认 `[0x477810]` / `[0x477838]` 在游玩期间**非空**（空 → 读失败 → 保守不计时）—— 2026-09-30：游玩期间两指针全程非空
- [x] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）—— **成立**（`PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不会误计**
- [ ] 备选：静态 replay 量 `*(u32*)0x474CA0` bit3（`0x417D9C` 置位），未验证，可顺带采一次

### th11（B 代 · 地灵殿）—— ✅ 三条已三态验证（2026-09-30）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4A8EB4 != 0`（PLAYER_PTR） | 单级读 |
| pause | `[0x4A8E88]+0x60 & 0x10 == 0`（GameThread，bit4） | 二级读 |
| replay | `[0x4A8EB8]+0x10 != 1`（ReplayManager） | 二级读 |

- [x] 三态各采一次，确认两个指针非空 —— 2026-09-30 探针实测：游玩期间一级指针全程非空
- [x] 确认 attract demo 误计风险 —— 2026-09-30 实测：**成立**（attract demo 期间 `PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不误计**
- [ ] 注意：`+0x74` 是 `ReplayManager::Start` 的 mode，**不是** gamemode（已更正），勿再拿它当门控
- [ ] 备选：静态 replay 量 `*(u32*)0x4A5758` bit3（`0x41FEFD` 置位），未验证

### th12（B 代 · 星莲船）—— ✅ 三条已三态验证（2026-09-30）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4B4514 != 0`（PLAYER_PTR） | 单级读 |
| pause | `[0x4B44E8]+0x60 & 0x10 == 0`（宿主对象，bit4） | 二级读；置位 `0x432850`、清位 `0x432960` |
| replay | `[0x4B4518]+0x10 != 1`（ReplayManager） | 二级读；`0x43AE80` 存 mode |

- [x] 三态各采一次，确认两个指针非空 —— 2026-09-30 探针实测：游玩期间一级指针全程非空
- [x] 确认 attract demo 误计风险 —— 2026-09-30 实测：**成立**（attract demo 期间 `PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不误计**
- [ ] 注意 `0x4b44e8 + 0x74` 是 `ReplayManager::Start` 的 mode（已更正），勿当 gamemode

### th13（B 代 · 东方神灵庙）—— ✅ 三条已三态验证（2026-09-30）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4C22C4 != 0`（PLAYER_PTR） | 单级读；赋值 `0x441d82`、清零 `0x4427d0` |
| pause | `[0x4C2194]+0x60 & 0x10 == 0`（PauseInf，bit4） | 二级读；OR `0x43e46a`/`0x43e61d`/`0x440aba`、AND `0x43e71b`/`0x440769` |
| replay | `[0x4C22C8]+0x10 != 1`（ReplayManager，mode） | 二级读；游戏自带访问器 `0x413c60` |

- [x] 三态各采一次，确认 `[0x4C2194]` / `[0x4C22C8]` 在游玩期间**非空**（空 → 读失败 → 保守不计时） —— 2026-09-30 探针实测：游玩期间一级指针全程非空
- [x] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**） —— 2026-09-30 实测：**成立**（attract demo 期间 `PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不误计**
- [ ] 注意 `0x4c2194` 是**指针变量**（PauseInf），必须二级读；`+0x74` 是 `ReplayManager::Start` 的 mode，勿当 gamemode
- ~~STEAM / 汉化变体是否共用同一 RVA~~ —— 需求只覆盖**原版**，**不在验证范围**，不再验证
- [ ] 详细证据见 `docs/th13/th13_re_disasm_report.md`

### th14（C 代 · 东方辉针城）—— ✅ 三条已三态验证（2026-09-30）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4DB67C != 0`（PLAYER_PTR） | 单级读；赋值 `0x44c44a`（`alloc 0x184c0`）、清零 `0x44d17a` |
| pause | `[0x4DB558]+0x80 & 0x10 == 0`（PauseInf，bit4） | 二级读；OR `0x448e24`/`0x448f8d`/`0x449e0a`/`0x449f8a`/`0x44b22a`、AND `0x449043`/`0x44aee9` |
| replay | `[0x4DB688]+0x10 != 1`（ReplayManager，mode） | 二级读；游戏自带访问器 `0x41d640` |

- [x] 三态各采一次，确认 `[0x4DB558]` / `[0x4DB688]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时） —— 2026-09-30 探针实测：游玩期间一级指针全程非空
- [x] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**） —— 2026-09-30 实测：**成立**（attract demo 期间 `PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不误计**
- [ ] 注意 `0x4DB558` 是**指针变量**（PauseInf），必须二级读；`+0x80` 同偏移另有 bit2/bit11 等其它语义位，pause 只做 `& 0x10` 位测试
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- ~~STEAM / 汉化变体是否共用同一 RVA~~ —— 需求只覆盖**原版**，**不在验证范围**，不再验证
- [ ] 详细证据见 `docs/th14/th14_re_disasm_report.md`

### th16（C 代 · 东方天空璋）—— ✅ 三条已三态验证（2026-09-30）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4A6EF8 != 0`（PLAYER_PTR） | 单级读；创建 `0x441cac`（构造器 `0x441c60`，`alloc 0x2c828`）、清零 `0x441b18` |
| pause | `[0x4A6DD4]+0x88 & 0x10 == 0`（PauseInf，bit4） | 二级读；构造 `0x42d700`；OR `0x43f111`/`0x43f2a1`/`0x43f3de`/`0x43f552`、AND `0x43f6c5`/`0x440add` |
| replay | `[0x4A6F08]+0xc != 1`（ReplayManager，mode） | 二级读；构造 `0x447760`；游戏自带判据 `0x417f86`/`0x418381`/`0x4428b7` |

- [x] 三态各采一次，确认 `[0x4A6DD4]` / `[0x4A6F08]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时） —— 2026-09-30 探针实测：游玩期间一级指针全程非空
- [x] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**） —— 2026-09-30 实测：**成立**（attract demo 期间 `PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不误计**
- [ ] 注意 `0x4A6DD4` 是**指针变量**（PauseInf），必须二级读；`+0x88` 同偏移另有 bit2/bit14 等其它语义位，pause 只做 `& 0x10` 位测试
- [ ] 注意本作 replay mode 偏移是 **`+0xc`**（与 th13/th14 的 `+0x10` 不同，与 th15 一致），勿按 `+0x10` 读
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- ~~STEAM / 汉化变体是否共用同一 RVA~~ —— 需求只覆盖**原版**，**不在验证范围**，不再验证
- [ ] 详细证据见 `docs/th16/th16_re_disasm_report.md`

### th17（C 代 · 东方鬼形兽）—— ✅ 三条已三态验证（2026-09-30）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4B77D0 != 0`（PLAYER_PTR） | 单级读；注册 `0x446e6e`（函数 `0x446af0` 内 `memset(obj,0,0x190fc)`）、清零 `0x446f76` |
| pause | `[0x4B76B0]+0x8c & 0x10 == 0`（PauseInf，bit4） | 二级读；构造 `0x431240`；OR `0x443b32`/`0x44472c`/`0x44485e`/`0x4449db`、AND `0x445d96`/`0x44602b` |
| replay | `[0x4B77D8]+0xc != 1`（ReplayManager，mode） | 二级读；构造 `0x44dbb0`；游戏自带判据 `0x41ad60`/`0x41b1f9`/`0x447dd7` |

- [x] 三态各采一次，确认 `[0x4B76B0]` / `[0x4B77D8]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时） —— 2026-09-30 探针实测：游玩期间一级指针全程非空
- [x] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**） —— 2026-09-30 实测：**成立**（attract demo 期间 `PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不误计**
- [ ] 注意 `0x4B76B0` 是**指针变量**（PauseInf），必须二级读；相邻的 `0x4B76AC` 是另一对象（StageController 类），勿混；`+0x8c` 同偏移另有 bit2/bit16 等其它语义位，pause 只做 `& 0x10` 位测试
- [ ] 注意本作 replay mode 偏移是 **`+0xc`**（与 th15/th16 一致，th13/th14 为 `+0x10`），勿按 `+0x10` 读
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- ~~STEAM / 汉化变体是否共用同一 RVA~~ —— 需求只覆盖**原版**，**不在验证范围**，不再验证
- [ ] 详细证据见 `docs/th17/th17_re_disasm_report.md`

### th18（C 代 · 东方虹龙洞）—— ✅ 三条已三态验证（2026-09-30）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4CF410 != 0`（PLAYER_PTR） | 单级读；创建 `0x45afed`（构造器 `0x45af20`，`alloc 0x479d0`）、清零 `0x45b0f6` |
| pause | `[0x4CF2E4]+0xb0 & 0x10 == 0`（GameThread，bit4） | 二级读；构造 `0x4437b0`；OR `0x4586a5`/`0x45894e`/`0x458aae`/`0x458c2b`、AND `0x45a0ce`/`0x45a39a` |
| replay | `[0x4CF418]+0xc != 1`（ReplayManager，mode） | 二级读；构造 `0x4615e0`；游戏自带判据 `0x42a390`/`0x42a82a`/`0x45c28d` |

- [x] 三态各采一次，确认 `[0x4CF2E4]` / `[0x4CF418]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时） —— 2026-09-30 探针实测：游玩期间一级指针全程非空
- [x] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**） —— 2026-09-30 实测：**成立**（attract demo 期间 `PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不误计**
- [ ] 注意 pause 位在 **GameThread（`0x4CF2E4`）**，**不是** `PAUSE_MENU_PTR(0x4CF40C)`；`+0xb0` 同偏移另有 bit2/bit14 等其它语义位，pause 只做 `& 0x10` 位测试。可对照 thprac 既有 hook `th18_pause_skip_1`（`0x458692`）复核
- [ ] 注意本作 replay mode 偏移是 **`+0xc`**（与 th15/th16/th17 一致）
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- ~~STEAM / 汉化变体是否共用同一 RVA~~ —— 需求只覆盖**原版**，**不在验证范围**，不再验证
- [ ] 详细证据见 `docs/th18/th18_re_disasm_report.md`

### th15（C 代 · 东方绀珠传）—— 代码已更正，✅ 三条已三态验证（2026-09-30）

`gGateTh15` 已按 `docs/th15/th15_re_disasm_report.md` 更正（原 pause `0x0E9B24` 是 0 引用的伪地址、原 replay 用场景状态）：

```cpp
static const THGameTimeGate gGateTh15 {
    { 0x0E9BB8, 4, 0,    GateCmp::Ne       },        // gamemode: PLAYER_PTR != 0
    { 0x0E9A94, 4, 0x10, GateCmp::BitClear, 0x90 },   // pause: [PauseInf]+0x90 bit4
    { 0x0E9BC4, 4, 1,    GateCmp::Ne,       0x0C },   // replay: [ReplayInf]+0xc != 1
};
```

- [x] 三态各采一次，确认 `[0x4E9A94]` / `[0x4E9BC4]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时） —— 2026-09-30 探针实测：游玩期间一级指针全程非空
- [x] pause：游玩中按 Esc → 冻结；恢复 → 涨（**验证原恒通过 bug 已修**） —— 2026-09-30 实测：`+0x90` bit4 正常翻转（`0x10000` → `0x0` → `0x10` → `0x0`），原恒通过 bug 确已修复
- [x] 回放：播放存档 replay → replay 判据成立 → `playNs` **不涨** —— 2026-09-30 实测：回放态（含 attract demo）`replay` 原始值 = `0x1` → 判据成立 → GATE blocked，`playNs` 不涨
- [ ] **练习模式**：若练习不录制回放导致 `[0x4E9BC4] == 0`，会漏计练习时长，需实测确认
- [ ] **回放指针悬空**：退出关卡/回放后 `[0x4E9BC4]` 未被清零，确认 `gamemode` 短路后不会读到无效内存
- [x] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）；注意原 gate 曾用场景状态 `0x4E7ECC == 0xd` 判回放，可顺带对照采样 —— 2026-09-30 实测：**成立**（attract demo 期间 `PLAYER_PTR` 非空），但 `replay` 原始值 = `0x1` 判据不成立 → GATE blocked，**不误计**
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- ~~STEAM / 汉化变体是否共用同一 RVA~~ —— 需求只覆盖**原版**，**不在验证范围**，不再验证
- [ ] 详细证据见 `docs/th15/th15_re_disasm_report.md`

---

## 3. 通用端到端用例（每作跑一遍）

> 摘自 `gametime_plan.md` §3.4，作为逐作验证的固定脚本。
>
> **本节 2026-09-30 未执行**：这些是**启动器侧**用例（要看真 `playNs` 与会话文件），与 §2 的地址级三态验证是两层。
> §2 已全部完成；本节需真人操作一次新构建的 `Release\thprac.exe`，待补。

- [ ] **认游戏正确性**：读数显示正确的 `game` / `base=0x400000`（th06~th15）
- [ ] **gamemode**：标题 / 菜单 / 选人 / Replay 菜单 → `playNs` 不涨；进第 1 关 → 涨
- [ ] **暂停**：游戏中按 Esc → 冻结；返回 → 恢复
- [ ] **回放**（关键）：播放存档 replay → `gamemode` 仍成立但 replay 判据成立 → `playNs` **不涨**
- [ ] **会话边界**：正常退出 → `gametime_sessions.jsonl` 新增一行且 `durationNs` 与 UI 相符；任务管理器强杀 → 同样新增、延时 < 16 ms；挂机 30 s → `durationNs` 只含真实游玩段
- [ ] **不污染旧功能**：Others 页"总飞行时间"行为不变；`thpracP.json` 的 `"Gametime"` 照常更新
- [ ] **未接入作不记录**：如 th09 / th185 玩一局退出 → 不新增行
- [ ] **launcher 先关**：游戏在跑时关启动器 → 新增一行、`end` = 关闭时刻
- [ ] **文件格式**：`python -c "import json;[json.loads(l) for l in open(p,encoding='utf-8')]"` 逐行合法 JSON、无 BOM

---

## 4. 实测记录表（回填用）

> 2026-09-30 探针实测。B/C 代的 `gamemode` 是 `PLAYER_PTR`（堆地址每次启动不同），故只记「非 0」；
> `pause` / `replay` 记**二级读出来的字段原始值**。A 代的 `replay` 记 `FLAGS` 原始值（判据为 bit3）。
> 「结论」列 PASS = 三判据全成立（该 tick 计 `playNs`），blocked = 被列出的那一项挡住。

| 日期 | 作品 | 状态（正常 / 暂停 / replay） | gamemode 原始值 | pause 原始值 | replay 原始值 | playNs 是否增长 | 结论 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-09-30 | th07 | 正常游玩 | `2` | `0` | `0x04`（bit3=0） | 是 | PASS |
| 2026-09-30 | th07 | 暂停（Esc） | `2` | `2` | `0x00` | 否 | blocked（pause） |
| 2026-09-30 | th07 | 回放 | `2` | `0` | `0x0c`（bit3=1） | 否 | blocked（replay） |
| 2026-09-30 | th08 | 正常游玩 | `2` | `0` | `0x38004`（bit3=0） | 是 | PASS |
| 2026-09-30 | th08 | 暂停（Esc） | `2` | `2` | `0x38000` | 否 | blocked（pause） |
| 2026-09-30 | th08 | 回放 | `2` | `0` | `0x3800c`（bit3=1） | 否 | blocked（replay） |
| 2026-09-30 | th10 | 正常游玩 | 非 0 | `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th10 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th10 | attract demo（=回放播放） | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |
| 2026-09-30 | th11 | 正常游玩 | 非 0 | `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th11 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th11 | attract demo | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |
| 2026-09-30 | th12 | 正常游玩 | 非 0 | `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th12 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th12 | attract demo | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |
| 2026-09-30 | th13 | 正常游玩 | 非 0 | `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th13 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th13 | attract demo | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |
| 2026-09-30 | th14 | 正常游玩 | 非 0 | `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th14 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th14 | attract demo | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |
| 2026-09-30 | th15 | 正常游玩 | 非 0 | `0x10000` → `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th15 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th15 | attract demo | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |
| 2026-09-30 | th16 | 正常游玩 | 非 0 | `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th16 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th16 | attract demo | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |
| 2026-09-30 | th17 | 正常游玩 | 非 0 | `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th17 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th17 | attract demo | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |
| 2026-09-30 | th18 | 正常游玩 | 非 0 | `0x0` | `0x0` | 是 | PASS |
| 2026-09-30 | th18 | 暂停（Esc） | 非 0 | `0x10` | `0x0` | 否 | blocked（pause） |
| 2026-09-30 | th18 | attract demo | 非 0 | `0x0` | `0x1` | 否 | blocked（replay） |

**原始证据**：`D:\workshop\reasm\gametime\out\<thXX>\`（签名日志 + 各状态截图）。

---

## 5. 影响验证的已知风险 / 开放问题

> 2026-09-30 实测后更新；已收口的条目保留结论，未做的明确标注为**未测**。

1. **attract demo 误计 —— 已收口（风险成立但被 replay 判据挡住）**：th10–th18 的 attract demo 期间 `PLAYER_PTR` **确实非 0**（原担心成立），但 `replay` 字段原始值同时为 `0x1`，判据不成立 → GATE blocked。**回放判据正是防误计的那道闸，无需补"非 demo"判据。** A 代同理（th06 的 demo 也是 `replay=1`）。
2. **续关菜单漏计（th07 / th08，th10–th18）—— 仍未测**：`pause == 0` 不覆盖死亡后的 Continue 菜单，那一状态三门控可能全成立。需要在三个状态之外再故意死亡一次；本轮自动化序列未纳入，**该项保持开放**。
3. **指针为空的瞬时 —— 已收口**：th10–th18 正常游玩期间一级指针**全程非空**；仅在 attract demo 起止的切换瞬间读到空指针 → 该 tick 保守不计时（符合设计）。不会静默少计。
4. ~~非原版变体（Steam / 汉化 / 魔改）~~ —— **需求只覆盖原版，本条作废**，不再验证。
5. **th15 ReplayInf 指针 —— 部分收口**：回放判据 `[0x4E9BC4]+0xc != 1` 已由 attract demo 实测验证（demo 期间 = `0x1` → blocked）。**练习模式是否创建 ReplayInf 仍未测**（练习模式若 `[0x4E9BC4] == 0` 会漏计）；旧判据场景状态 `0x4E7ECC == 0xd` 可作对照。见 `docs/th15/th15_re_disasm_report.md` §2、§6。
6. **多开**：单会话设计，只认第一个被枚举到的游戏进程，不做并发。
7. **launcher 必须驻留**：CLI / 自动查找路径下启动器会退出，无法计时（已知缺口，见 `gametime_plan.md` §1.8）。

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-30 | 建档：汇总 th06（P0 遗留）、th07/th08（端到端 + 续关边界）、th10/th11/th12（三条静态判据待三态）、th15（pause 需先改代码再验）等待验证项，附通用端到端脚本与回填模板。 |
| 2026-09-30 | 新增 th13（B 代）：三条静态判据待三态 —— gamemode `0x4C22C4 != 0`、pause `[0x4C2194]+0x60 & 0x10 == 0`、replay `[0x4C22C8]+0x10 != 1`。详见 `docs/th13/th13_re_disasm_report.md`。 |
| 2026-09-30 | 新增 th14（C 代，与 th13 同构）：三条静态判据待三态 —— gamemode `0x4DB67C != 0`、pause `[0x4DB558]+0x80 & 0x10 == 0`、replay `[0x4DB688]+0x10 != 1`。详见 `docs/th14/th14_re_disasm_report.md`。 |
| 2026-09-30 | **th15 更正并接入**：pause 由错误单级静态 `0x0E9B24` 改为二级读 `[0x4E9A94]+0x90 & 0x10 == 0`；replay 由场景状态 `0x4E7ECC != 0xd` 改为 `[0x4E9BC4]+0xc != 1`（ReplayInf mode，与 th13/th14 同构）。新增练习模式/悬空指针验证项。详见 `docs/th15/th15_re_disasm_report.md`。 |
| 2026-09-30 | 新增 th16（C 代，与 th13/th14 同构）：三条静态判据待三态 —— gamemode `0x4A6EF8 != 0`、pause `[0x4A6DD4]+0x88 & 0x10 == 0`、replay `[0x4A6F08]+0xc != 1`（replay mode 偏移为 `+0xc`，与 th15 一致）。详见 `docs/th16/th16_re_disasm_report.md`。 |
| 2026-09-30 | 新增 th17（C 代，与 th13/th14/th16 同构）：三条静态判据待三态 —— gamemode `0x4B77D0 != 0`、pause `[0x4B76B0]+0x8c & 0x10 == 0`、replay `[0x4B77D8]+0xc != 1`（replay mode 偏移 `+0xc`）。详见 `docs/th17/th17_re_disasm_report.md`。 |
| 2026-09-30 | 新增 th18（C 代，与 th13~th17 同构）：三条静态判据待三态 —— gamemode `0x4CF410 != 0`、pause `[0x4CF2E4]+0xb0 & 0x10 == 0`（在 GameThread 而非 PauseMenu）、replay `[0x4CF418]+0xc != 1`。详见 `docs/th18/th18_re_disasm_report.md`。 |
| 2026-09-30 | **三态验证完成（11 作）**：th07/th08 复验通过；th10–th18 首次验证通过。每作三态取值互不相同且各自符合判据，`playNs` 只在正常游玩时累计 —— §2.3 第 5 条满足。**attract demo 误计风险实测成立**（demo 期间 `PLAYER_PTR` 非空）**但被 replay 判据挡住**，风险 1 收口；二级指针游玩期间全程非空，风险 3 收口；变体条目按需求收口为不验证，风险 4 作废；th15 pause 实测正常翻转，原恒通过 bug 确认已修。原始证据：`D:\workshop\reasm\gametime\out\<thXX>\`。**th06 经确认不在验证范围**（成熟代码，本轮未做）。未做项：续关菜单漏计（风险 2）、th15 练习模式（风险 5）、启动器侧端到端（§3）、th10–th13 的菜单回放（本机无 replay 文件，改用 attract demo 采样）。 |

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

---

## 1. 总览

| 作品 | 代 | 判据来源 | 代码状态 | 真机状态 | 待验证内容 |
| --- | --- | --- | --- | --- | --- |
| th06 | A | 既有代码推导（无报告） | 已接入 | ⚠️ **无三态实测记录** | 三条 flag + 端到端（P0 遗留） |
| th07 | A | 反编译报告 + 三态 | 已接入 | ✅ 地址已三态验证 | 接入后端到端行为（P1b）；续关菜单漏计 |
| th08 | A | 反编译报告 + 三态 | 已接入 | ✅ 地址已三态验证 | 同上 |
| th10 | B | 静态反编译 | 已接入 | 🟡 **未验证** | 三条 flag（含二级读） |
| th11 | B | 静态反编译 | 已接入 | 🟡 **未验证** | 三条 flag（含二级读） |
| th12 | B | 静态反编译（本轮新增） | 已接入 | 🟡 **未验证** | 三条 flag（含二级读） |
| th13 | B | 静态反编译（本轮新增） | 已接入 | 🟡 **未验证** | 三条 flag（含二级读） |
| th14 | C | 静态反编译（本轮新增） | 已接入 | 🟡 **未验证** | 三条 flag（含二级读） |
| th16 | C | 静态反编译（本轮新增） | 已接入 | 🟡 **未验证** | 三条 flag（含二级读） |
| th17 | C | 静态反编译（本轮新增） | 已接入 | 🟡 **未验证** | 三条 flag（含二级读） |
| th18 | C | 静态反编译（本轮新增） | 已接入 | 🟡 **未验证** | 三条 flag（含二级读） |
| th15 | C | 静态反编译 + 复核更正 | 已接入（pause/replay 已更正） | 🟡 **未验证** | 三条 flag（含二级读）+ 练习模式/悬空指针 |

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

### th07 / th08（地址已验证，剩"接入后端到端行为"）

地址本身已在 `docs/th07` / `docs/th08` 报告中三态验证；剩 P1b：验证**接入 gate 表后**的端到端行为与"续关菜单"边界。

| 作 | gamemode | pause | replay |
| --- | --- | --- | --- |
| th07 | `0x175AA4 == 2` | `0x22F64C == 0` | `(0x22F648 & 8) == 0`（bit3） |
| th08 | `0x13CE8B0 == 2` | `0x124D0BA == 0` | `(0x124D0B4 & 8) == 0`（bit3） |

- [ ] th07 / th08 各跑一遍 §3 的用例 2~4、6
- [ ] **续关菜单漏计确认**：死亡后进 Continue 菜单时 `pause == 0`、`gamemode == 2`、`replay` 不成立 → 会误计。实测确认后决定是否改用 bit2 强化形态（`{ 0x22F648, 4, 0x4, BitClear }` / `{ 0x124D0B4, 4, 0x4, BitClear }`）。**该强化形态本身也未经三态验证。**

### th10（B 代 · 风神录）—— 三条全未验证

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x477834 != 0`（PLAYER_PTR） | 单级读 |
| pause | `[0x477810]+0x58 & 0x10 == 0`（GameManager，bit4） | 二级读 |
| replay | `[0x477838]+0x10 != 1`（ReplayManager） | 二级读 |

- [ ] 三态各采一次，确认 `[0x477810]` / `[0x477838]` 在游玩期间**非空**（空 → 读失败 → 保守不计时）
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）
- [ ] 备选：静态 replay 量 `*(u32*)0x474CA0` bit3（`0x417D9C` 置位），未验证，可顺带采一次

### th11（B 代 · 地灵殿）—— 三条全未验证

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4A8EB4 != 0`（PLAYER_PTR） | 单级读 |
| pause | `[0x4A8E88]+0x60 & 0x10 == 0`（GameThread，bit4） | 二级读 |
| replay | `[0x4A8EB8]+0x10 != 1`（ReplayManager） | 二级读 |

- [ ] 三态各采一次，确认两个指针非空
- [ ] 确认 attract demo 误计风险
- [ ] 注意：`+0x74` 是 `ReplayManager::Start` 的 mode，**不是** gamemode（已更正），勿再拿它当门控
- [ ] 备选：静态 replay 量 `*(u32*)0x4A5758` bit3（`0x41FEFD` 置位），未验证

### th12（B 代 · 星莲船）—— 三条全未验证（本轮新增）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4B4514 != 0`（PLAYER_PTR） | 单级读 |
| pause | `[0x4B44E8]+0x60 & 0x10 == 0`（宿主对象，bit4） | 二级读；置位 `0x432850`、清位 `0x432960` |
| replay | `[0x4B4518]+0x10 != 1`（ReplayManager） | 二级读；`0x43AE80` 存 mode |

- [ ] 三态各采一次，确认两个指针非空
- [ ] 确认 attract demo 误计风险
- [ ] 注意 `0x4b44e8 + 0x74` 是 `ReplayManager::Start` 的 mode（已更正），勿当 gamemode

### th13（B 代 · 东方神灵庙）—— 三条全未验证（本轮新增）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4C22C4 != 0`（PLAYER_PTR） | 单级读；赋值 `0x441d82`、清零 `0x4427d0` |
| pause | `[0x4C2194]+0x60 & 0x10 == 0`（PauseInf，bit4） | 二级读；OR `0x43e46a`/`0x43e61d`/`0x440aba`、AND `0x43e71b`/`0x440769` |
| replay | `[0x4C22C8]+0x10 != 1`（ReplayManager，mode） | 二级读；游戏自带访问器 `0x413c60` |

- [ ] 三态各采一次，确认 `[0x4C2194]` / `[0x4C22C8]` 在游玩期间**非空**（空 → 读失败 → 保守不计时）
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）
- [ ] 注意 `0x4c2194` 是**指针变量**（PauseInf），必须二级读；`+0x74` 是 `ReplayManager::Start` 的 mode，勿当 gamemode
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹）
- [ ] 详细证据见 `docs/th13/th13_re_disasm_report.md`

### th14（C 代 · 东方辉针城）—— 三条全未验证（本轮新增）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4DB67C != 0`（PLAYER_PTR） | 单级读；赋值 `0x44c44a`（`alloc 0x184c0`）、清零 `0x44d17a` |
| pause | `[0x4DB558]+0x80 & 0x10 == 0`（PauseInf，bit4） | 二级读；OR `0x448e24`/`0x448f8d`/`0x449e0a`/`0x449f8a`/`0x44b22a`、AND `0x449043`/`0x44aee9` |
| replay | `[0x4DB688]+0x10 != 1`（ReplayManager，mode） | 二级读；游戏自带访问器 `0x41d640` |

- [ ] 三态各采一次，确认 `[0x4DB558]` / `[0x4DB688]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时）
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）
- [ ] 注意 `0x4DB558` 是**指针变量**（PauseInf），必须二级读；`+0x80` 同偏移另有 bit2/bit11 等其它语义位，pause 只做 `& 0x10` 位测试
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹）
- [ ] 详细证据见 `docs/th14/th14_re_disasm_report.md`

### th16（C 代 · 东方天空璋）—— 三条全未验证（本轮新增）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4A6EF8 != 0`（PLAYER_PTR） | 单级读；创建 `0x441cac`（构造器 `0x441c60`，`alloc 0x2c828`）、清零 `0x441b18` |
| pause | `[0x4A6DD4]+0x88 & 0x10 == 0`（PauseInf，bit4） | 二级读；构造 `0x42d700`；OR `0x43f111`/`0x43f2a1`/`0x43f3de`/`0x43f552`、AND `0x43f6c5`/`0x440add` |
| replay | `[0x4A6F08]+0xc != 1`（ReplayManager，mode） | 二级读；构造 `0x447760`；游戏自带判据 `0x417f86`/`0x418381`/`0x4428b7` |

- [ ] 三态各采一次，确认 `[0x4A6DD4]` / `[0x4A6F08]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时）
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）
- [ ] 注意 `0x4A6DD4` 是**指针变量**（PauseInf），必须二级读；`+0x88` 同偏移另有 bit2/bit14 等其它语义位，pause 只做 `& 0x10` 位测试
- [ ] 注意本作 replay mode 偏移是 **`+0xc`**（与 th13/th14 的 `+0x10` 不同，与 th15 一致），勿按 `+0x10` 读
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹）
- [ ] 详细证据见 `docs/th16/th16_re_disasm_report.md`

### th17（C 代 · 东方鬼形兽）—— 三条全未验证（本轮新增）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4B77D0 != 0`（PLAYER_PTR） | 单级读；注册 `0x446e6e`（函数 `0x446af0` 内 `memset(obj,0,0x190fc)`）、清零 `0x446f76` |
| pause | `[0x4B76B0]+0x8c & 0x10 == 0`（PauseInf，bit4） | 二级读；构造 `0x431240`；OR `0x443b32`/`0x44472c`/`0x44485e`/`0x4449db`、AND `0x445d96`/`0x44602b` |
| replay | `[0x4B77D8]+0xc != 1`（ReplayManager，mode） | 二级读；构造 `0x44dbb0`；游戏自带判据 `0x41ad60`/`0x41b1f9`/`0x447dd7` |

- [ ] 三态各采一次，确认 `[0x4B76B0]` / `[0x4B77D8]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时）
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）
- [ ] 注意 `0x4B76B0` 是**指针变量**（PauseInf），必须二级读；相邻的 `0x4B76AC` 是另一对象（StageController 类），勿混；`+0x8c` 同偏移另有 bit2/bit16 等其它语义位，pause 只做 `& 0x10` 位测试
- [ ] 注意本作 replay mode 偏移是 **`+0xc`**（与 th15/th16 一致，th13/th14 为 `+0x10`），勿按 `+0x10` 读
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹）
- [ ] 详细证据见 `docs/th17/th17_re_disasm_report.md`

### th18（C 代 · 东方虹龙洞）—— 三条全未验证（本轮新增）

| 量 | 判据 | 备注 |
| --- | --- | --- |
| gamemode | `*(u32*)0x4CF410 != 0`（PLAYER_PTR） | 单级读；创建 `0x45afed`（构造器 `0x45af20`，`alloc 0x479d0`）、清零 `0x45b0f6` |
| pause | `[0x4CF2E4]+0xb0 & 0x10 == 0`（GameThread，bit4） | 二级读；构造 `0x4437b0`；OR `0x4586a5`/`0x45894e`/`0x458aae`/`0x458c2b`、AND `0x45a0ce`/`0x45a39a` |
| replay | `[0x4CF418]+0xc != 1`（ReplayManager，mode） | 二级读；构造 `0x4615e0`；游戏自带判据 `0x42a390`/`0x42a82a`/`0x45c28d` |

- [ ] 三态各采一次，确认 `[0x4CF2E4]` / `[0x4CF418]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时）
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）
- [ ] 注意 pause 位在 **GameThread（`0x4CF2E4`）**，**不是** `PAUSE_MENU_PTR(0x4CF40C)`；`+0xb0` 同偏移另有 bit2/bit14 等其它语义位，pause 只做 `& 0x10` 位测试。可对照 thprac 既有 hook `th18_pause_skip_1`（`0x458692`）复核
- [ ] 注意本作 replay mode 偏移是 **`+0xc`**（与 th15/th16/th17 一致）
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹）
- [ ] 详细证据见 `docs/th18/th18_re_disasm_report.md`

### th15（C 代 · 东方绀珠传）—— 代码已更正，三条待验证（2026-09-30 复核）

`gGateTh15` 已按 `docs/th15/th15_re_disasm_report.md` 更正（原 pause `0x0E9B24` 是 0 引用的伪地址、原 replay 用场景状态）：

```cpp
static const THGameTimeGate gGateTh15 {
    { 0x0E9BB8, 4, 0,    GateCmp::Ne       },        // gamemode: PLAYER_PTR != 0
    { 0x0E9A94, 4, 0x10, GateCmp::BitClear, 0x90 },   // pause: [PauseInf]+0x90 bit4
    { 0x0E9BC4, 4, 1,    GateCmp::Ne,       0x0C },   // replay: [ReplayInf]+0xc != 1
};
```

- [ ] 三态各采一次，确认 `[0x4E9A94]` / `[0x4E9BC4]` 在游玩期间**非空**（空 → 二级读失败 → 保守不计时）
- [ ] pause：游玩中按 Esc → 冻结；恢复 → 涨（**验证原恒通过 bug 已修**）
- [ ] 回放：播放存档 replay → replay 判据成立 → `playNs` **不涨**
- [ ] **练习模式**：若练习不录制回放导致 `[0x4E9BC4] == 0`，会漏计练习时长，需实测确认
- [ ] **回放指针悬空**：退出关卡/回放后 `[0x4E9BC4]` 未被清零，确认 `gamemode` 短路后不会读到无效内存
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（**误计风险**）；注意原 gate 曾用场景状态 `0x4E7ECC == 0xd` 判回放，可顺带对照采样
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹）
- [ ] 详细证据见 `docs/th15/th15_re_disasm_report.md`

---

## 3. 通用端到端用例（每作跑一遍）

> 摘自 `gametime_plan.md` §3.4，作为逐作验证的固定脚本。

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

| 日期 | 作品 | 状态（正常 / 暂停 / replay） | gamemode 原始值 | pause 原始值 | replay 原始值 | playNs 是否增长 | 结论 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| | | | | | | | |

---

## 5. 影响验证的已知风险 / 开放问题

1. **attract demo 误计（th10 / th11 / th12 / th13 / th14 / th15 / th16 / th17 / th18）**：`PLAYER_PTR != 0` 在标题挂机自动演示时是否成立，需实测；若成立需补一个"非 demo"判据（th08 有现成 bit2 形态可参考）。
2. **续关菜单漏计（th07 / th08，th10~th12 需一并确认）**：`pause == 0` 不覆盖死亡后的 Continue 菜单，那一状态三门控可能全成立。
3. **指针为空的瞬时**：二级读中一级指针为 0 时 `ReadFlagValue` 返回 false → 该 tick 不计时（保守）。验证时需确认正常游玩期间指针稳定非空，否则会静默少计。
4. **非原版变体（Steam / 汉化 / 魔改）**：gate 按 `timeStamp + textSize` 匹配，非原版是否共用同一地址**需实测**；汉化版改了 `.text` 段大小可能反而不匹配。
5. **th15 ReplayInf 指针（已改代码，剩实测）**：`[0x4E9BC4]` 无清零点、练习模式是否创建 ReplayInf 未知。若练习/某些模式下 `[0x4E9BC4] == 0` 会漏计；旧判据场景状态 `0x4E7ECC == 0xd` 可作对照。见 `docs/th15/th15_re_disasm_report.md` §2、§6。
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

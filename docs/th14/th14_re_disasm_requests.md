# TH14 miss/bomb 记录：boss 阶段名来源确认（第一轮反编译请求）

> 状态：**第一轮回复已完成（2026-08-22，Ghidra 12.1.2 headless 反编译 th14.exe）**，结论见 §1 各问题下「结论」小节。
> 背景与开发组侧实现参考：`docs/th15/th15_missbomb_recording.md`（TH15 方案，TH14 拟复刻）。
> TH14 钩子总表：`docs/th14/thprac_th14_hooks.md`。

---

## 0. 背景与现状

thprac 已有 **TH15** miss/bomb 记录功能并已定案：掉命/放雷时记录「关卡号 + boss 阶段名」，一局一行落盘 `th15_missbomb.txt`，IGI 面板显示最近 5 条事件。现需在 **TH14（东方辉针城）** 复刻同一功能。

TH14 与 TH15 共享 ECL 解释器架构（case 512/514 解释器、`SetNextPattern` 写 HP+名字串、`FUN_0042ff10` 等价函数），TH15 已探明语义大概率可平移。但**所有偏移与 hook 站点必须按 TH14 反汇编确认**——这正是本请求的目的。

**范围声明：本功能仅覆盖 ST1~ST6，EX（第 7 关）不在范围内，无需反馈 EX 相关内容。**

### 开发组已探明信息（无需重复劳动，以下直接引用）

1. **记录点（miss/bomb 钩子，已存在，TH14 源码 `thprac_th14.cpp`）**：
   - `0x44F618`（EHOOK 5B，残机-1）→ 拟 `Record('m', 关卡号, boss阶段名)`；
   - `0x41218A`（EHOOK 5B，炸弹-1）→ 拟 `Record('b', ...)`。
   - 回调内可直接读游戏内存；"关卡号"候选 = `0x4f58a4`（语义待确认，见问题 4）。
2. **boss 相关钩子（已存在，当前仅用于"雷锁"计时）**：
   - `0x4286BA`（EHOOK 4B，case 514 = SetNextPattern）；
   - `0x427888`（EHOOK 2B，case 512 = set boss mode）。
   - 另：`th14_bossmovedown` @ `0x42725B`（EHOOK 5B）已读 `pCtx->Edi + 0x3F64/+0x3F6C`（boss Y 坐标/活动范围），**证明该处 `pCtx->Edi` = boss 对象**（TH15 bossmovedown 偏移同为 `+0x3F64/+0x3F6C`）。
3. **运行边界（已具备）**：
   - `PLAYER_PTR = 0x4db67c`（IGI `TH14InGameInfo::OnPreUpdate` 已轮询，残机耗尽/退回菜单即归零，作"一局结束"检测）；
   - `th14_game_start` @ `0x4375BE`（EHOOK 7B）已做开局计数清零，拟扩展为"新一局"边界（清 boss 指针/阶段名缓存、开新记录行）。
4. **TH15 定案方案摘要（供参考，详见 `docs/th15/th15_re_disasm_report.md` §1.1/1.2、`report2.md` §3.2、`report3.md` §2）**：
   - boss 对象指针**无全局**，由 case 512/514 钩子缓存 `pCtx->Edi`；
   - boss 战门控 `[g_boss_ptr + 0x4060] & 0x800000`（非 boss 时段此位为 0 → 记 `-`）；
   - 阶段名 = **攻击名字符串**（TH15 无持久"当前攻击编号/名字"字段）：在两个 `call FUN_0042ff10` 站点（normal `0x42C7D6` + BossDead `0x42C785`）命中时缓存槽**写前旧值** `scr + EAX*0x88 + 0x528C`（`scr = [g_boss_ptr + 0x44B8]`），槽空时兜底缓存新写入名（`[ESP+8]`）；
   - BossDead/BossEscape 时阶段名会被写成 `BossDead`/`BossEscape`，开发组用硬编码表映射回末符卡名（该表由开发组按游戏内符卡名自建，**仅 ST1~ST6**）。

### 缺口（本请求目标）

TH14 的对应偏移与 hook 站点未知，需按 TH14 反汇编确认（大概率与 TH15 相近但不保证一致）。

---

## 1. 反编译需求

### 问题 1：boss 指针缓存与 case 512/514 寄存器上下文

- `0x4286BA`（case 514）与 `0x427888`（case 512）命中时，`pCtx->Edi` 是否为 boss 对象指针？（TH15 对应 `0x42C738`/`0x42B938`，命中时 Edi = boss 对象，见 TH15 report1 §1.2）
- TH14 是否有全局 boss 指针？若无，是否只能由现有钩子缓存 EDI？
- case 512 命中时 `EAX` 语义：TH15 中 `EAX`（ECL 参数[0]）≥0 表示 boss 进场、<0 表示离场/击破（开发组据此区分道中/关底 boss 进入次数）。TH14 是否一致？

#### 结论（问题 1）

**两个站点都在 ECL 解释器 `FUN_00425270`（entry `0x425270`，body 至 `0x429c4c`）内；命中时 `pCtx->Edi` = boss 对象（ECL 实体上下文）。**

- `0x427888`（case 512，指令 `MOV ESI,EAX`，**2 字节** `8B F0`，与 hooks 文档标注一致）：
  - 命中前一条 `0x42787d PUSH 0x0; MOV ECX,EDI; CALL 0x0042c1d0` → `EAX = ECL param[0]`（`FUN_0042c1d0` 即 `FUN_00483f30([this+0x40ec]->[4], idx)` 的参数数组取值）。
  - **`EAX`（=ESI）≥ 0 → boss 进场**：`0x4278cc OR [EDI+0x4054],0x800000`（置门控）、`0x4278d6 MOV [0x4db544+EAX*4+0x5c],ECX`（scr 指针入全局模式数组）、`0x4278da MOV [EDI+0x4064],EAX`（记模式索引）。
  - **`EAX` < 0 → 离场/击破**：`0x427893 TEST [EDI+0x4054],0x800000` → `0x4278b2 AND [EDI+0x4054],0xff7fffff`（清门控），并清零 `[0x4db544 + [EDI+0x4064]*4 + 0x5c]`。
  - 与 TH15 口径**一致**。
- `0x4286ba`（case 514，指令 `MOV [ESP+0x2c],EAX`，**4 字节** `89 44 24 2C`，与 hooks 文档标注一致）：
  - `pCtx->Edi` = boss 对象；`[ESP+0x2c]` = 名字缓冲区指针（= `ESI+0x20`，见问题 2）。
- **无全局 boss 指针**：全局仅有 ECL 上下文 `[0x4db544]`（其 `+0x5c` 处为 8 个模式对象指针数组）；boss 对象本体只经 EDI 传递 → 只能由现有钩子缓存 EDI（同 TH15）。

### 问题 2：SetNextPattern（opcode 0x202）两个 call 站点定位（本请求核心）

- 定位 TH14 中「写攻击 HP + 名字串」的函数（TH15 为 `FUN_0042ff10`，`__thiscall this=scr, param_1=slot index, param_2=HP, param_3=name`）的**全部 call 站点**，特别标出：
  - **normal 分支**（TH15 对应 `0x42C7D6`，EHOOK 5B）；
  - **BossDead 分支**（TH15 对应 `0x42C785`，EHOOK 5B）。
  - 两分支互斥，都要挂才能覆盖全部 0x202 换攻击。
- 给出每个站点的**地址 + 指令 + 建议 EHOOK 长度**（须覆盖完整指令，VEH + codecave 需按该长度复制原指令并回跳）。
- 命中时 `EAX` 是否 = slot index？`name` 参数是否在栈上 `[ESP+8]`？（TH15 命中时 ESP 指向栈顶实参，非返回地址，见 TH15 report2 §1.4）

#### 结论（问题 2）

**TH14 的"写攻击 HP + 名字"函数 = `FUN_0042bed0`（`0x42bed0`，`__thiscall(this=scr, slot, HP, name)`）**：

```c
scr[slot*0x10 + 0x5260] = HP;    // 0x5260
scr[slot*0x10 + 0x5264] = name;  // 0x5264（名字拷贝 1）
scr[slot*0x10 + 0x5268] = name;  // 0x5268（名字拷贝 2）
```

- **normal 分支（唯一 call 站点）`0x00428782`**（`CALL 0x0042bed0`，**5 字节** `E8 49 37 00 00`）：
  - 命中时 `ECX` = scr = `[EDI+0x40ec]`；`EAX` = **slot index**（= ECL param[0]）；
  - 栈顶实参（hook 回调见 `pCtx->Esp` 指向最后压入实参）：`[ESP]`=slot、`[ESP+4]`=HP（param[2]）、`[ESP+8]`=**name**（指针，指向 `ESI+0x20` 缓冲区）——与 TH15 `[ESP+8]` 口径一致；
  - 建议 **EHOOK 5B @ `0x428782`**。
- **BossDead / BossEscape 分支**（TH14 结构 ≠ TH15：不是同一函数的第二个 call，而是**两个独立函数**）：
  - `FUN_0042c540`（`0x42c540`）@ **`0x42c5e4`**（`PUSH 0x4bde2c` = `"BossDead"`，5 字节）：遍历 `[0x4db544]+0x5c` 的 8 个模式对象，对每个经 `FUN_004848b0(obj+0x11cc, "BossDead")` 取内部字符串，写入 `obj[4][4]` 并 `obj[4][0]=0`。命中时 `[EBX]` = 模式对象，**写前旧名 = `[[EBX]+4]+4`**。⚠ **无静态调用者（间接调用）**，触发时机需真机 hook 验证。
  - `FUN_0042c620`（`0x42c620`）@ **`0x42c6c4`**（`PUSH 0x4bde38` = `"BossEscape"`，5 字节）：同结构。
  - 另：case 514 内符卡练习分支（`0x4286c9` 起，仅当 `[0x4f58b8] & 0x30 == 0x20` 时走）直接写 `scr+slot*0x10+0x5264/0x5268 = 0x4bde2c/0x4bde38`（站点 `0x428705`/`0x428710`/`0x42872f`）；正常游玩不走此分支。
- **推荐 name 缓存方案**（对齐 TH15「缓存槽写前旧值」）：
  - normal 站点 hook `0x428782`：写前缓存 `scr + EAX*0x10 + 0x5264` 的旧值；旧值空时兜底缓存新名 `[ESP+8]`；
  - BossDead/Escape 站点 hook `0x42c5e4` / `0x42c6c4`：写前缓存 `[[EBX]+4]+4` 旧值（= 末符卡名）。

### 问题 3：boss 对象布局（偏移确认；TH15 值仅作候选，须按 TH14 反汇编核实）

- boss 战门控标志位 `[boss + X] & 0x800000`：TH15 = `+0x4060` → TH14 是多少？
- 脚本资源指针 `[boss + 0x44B8]`（TH15：boss 对象 → ECL 脚本资源对象）→ TH14 偏移？
- 攻击名槽布局：`scr + slot*0x88 + 0x528C`（名字拷贝1）/ `+0x52CC`（拷贝2），槽数 8、步长 0x88（TH15）→ TH14 是否一致？
  - 注意 TH15 的坑：名字串真实偏移在 `+0x528C` 而非 `+0x528B`（后者是 HP dword 高字节）；`[boss+0x4080]` 无写点（TH15 report3 §2）。TH14 是否同样？
- TH14 是否也无持久化「当前攻击编号/名字」字段（唯一稳定标识 = 名字字符串）？
- BossDead/BossEscape 阶段名行为：请确认 TH14 与 TH15 相同——攻击结束/被击破时阶段名被写成 `BossDead`/`BossEscape`，不会留下末符卡名。**末符卡名映射表由开发组按游戏内符卡名自建（仅 ST1~ST6），无需反编译组提供。**

#### 结论（问题 3）

| TH14（已确认） | TH15（参考） | 语义 |
| --- | --- | --- |
| `[boss + 0x4054] & 0x800000` | `[boss + 0x4060] & 0x800000` | **boss 战门控位**（case 512 进场置 / 离场清） |
| `[boss + 0x40ec]` = scr | `[boss + 0x44B8]` = scr | 脚本/参数块指针 |
| `scr + slot*0x10 + 0x525c` | — | 符卡序号（ordinal）dword |
| `scr + slot*0x10 + 0x5260` | （`+0x528C` 一带 HP） | 攻击 HP dword |
| `scr + slot*0x10 + 0x5264` / `+0x5268` | `+0x528C` / `+0x52CC` | 攻击名 **char\***（拷贝 1/2） |
| `[0x4db544] + slot*4 + 0x5c` | — | 全局 8 模式对象指针数组 |

- **槽步长 0x10、共 8 槽**（TH15 为 0x88）。名字以**指针**存、非内联字符串；`"BossDead"` = `0x4bde2c`、`"BossEscape"` = `0x4bde38`。
- **门控偏移 = `+0x4054`**（≠ TH15 `+0x4060`）；**脚本资源指针 = `+0x40ec`**（≠ TH15 `+0x44B8`）。其余结构（stride/槽数/名字拷贝 x2）与 TH15 同构但数值不同。
- **TH14 也无持久「当前攻击编号/名字」字段**：`[boss+0x4064]` 仅在 case 512 进场时写一次（`0x4278da`），SetNextPattern（case 514）**不更新** → 唯一稳定标识 = 名字字符串本身（同 TH15）。
- **BossDead/BossEscape 行为确认**：Boss 被击破/逃离时 `FUN_0042c540`/`FUN_0042c620` 把每个模式对象 `[4][4]` 写成 `BossDead`/`BossEscape`，不会留下末符卡名 → 需在死亡站点缓存写前旧值。末符卡名映射表由开发组自建（仅 ST1~ST6）。

### 问题 4：关卡号语义（仅需 ST1~ST6）

- `0x4f58a4` 取值与语义（是否为 1~6，7=EX 可忽略），与 TH15 `0x4E73F0`（1~7，7=EX）是否同类。
- miss/bomb 钩子（`0x44F618`/`0x41218A`）命中时读取该地址是否可靠/恒定？

#### 结论（问题 4）

- **`0x4f58a4` = 当前关卡号**（全局 dword）。
  - 正常游玩取值 **1~6**；EX = 7；另有特殊值（如符卡练习 9：`0x44a1ea` 处 `CMP EAX,0x7` → `CMOVNZ EAX,0x9`；`0x44a22c` 处 9→7 回射）。建议记录时判 `1 <= stage <= 6`，其余（7/8/9 等）跳过。
  - `0x4f58a8` = 配对寄存器（选中/新关卡）。restart 路径 `0x447815`：`MOV [0x4f58a4],EAX`（EAX=`[0x4f58a8]`）→ `IMUL EAX,EAX,0xcc; ADD EAX,0x4d5988; MOV [0x4db69c],EAX`（per-stage 数据块指针 = `0x4d5988 + stage*0xcc`）。
  - 写入点仅见于选关菜单/开局/restart/回放载入（`FUN_0044bdc0`、`FUN_0045e1c0`、`FUN_0045ef40`、`FUN_00460a00`、`FUN_00463f30`、`FUN_0045e710`、`FUN_00459940`、`FUN_0044b0f0`、`FUN_00447810` 等）。
- **miss/bomb 钩子命中时读取可靠**：关卡号整局不变，仅在开局/restart 写入；`th14_life_dec`（`0x44f618`）/`th14_bomb_dec`（`0x41218a`）回调内读 `0x4f58a4` 恒定。
- 附带确认：`0x4f58b8` = 模式标记字节（含练习位，与 hooks 文档 `0x4f58b8 & 0x10` 口径一致）；`0x4f5838` = 关卡切换计数（上限 9）。

---

## 2. 交付物要求

- 每个问题给出**结论 + 支撑反汇编片段（地址 + 指令）**。
- 最终推荐：miss/bomb 时读取「boss 阶段名」的**精确表达式**（含全部偏移）+ 两个 pattern_switch hook 站点地址（normal + BossDead）。
- 若部分偏移与 TH15 完全一致，请逐项确认「一致」而非省略。

---

## 3. 附：开发组侧落地方向（供反编译组判断需求合理性）

```cpp
// TH14 拟复刻 TH15MissBombRecorder → TH14MissBombRecorder（仅 ST1~ST6）
// 已确认偏移（第一轮）：
//   boss 战门控 = [g_boss_ptr + 0x4054] & 0x800000   （case 512/514 钩子缓存 pCtx->Edi）
//   scr = *(void**)(g_boss_ptr + 0x40ec)
//   阶段名 = *(char**)(scr + slot*0x10 + 0x5264)     （写前旧值缓存；BossDead 站点兜底）
//   阶段名缓存站点：0x428782（normal）/ 0x42c5e4·0x42c6c4（BossDead/BossEscape）
// th14_life_dec (0x44F618) / th14_bomb_dec (0x41218A) 回调内：
if (thPracParam.mode == 1) {                       // 仅 thprac 练习模式记录
    int stage = *(int*)RVA(0x4f58a4);
    if (stage >= 1 && stage <= 6) {                // 仅 ST1~ST6，EX=7 跳过
        g_missbomb_rec.Record(kind, stage, GetBossStageName());
    }
}
```

---

## 4. 相关文档索引

| 文件 | 说明 |
| --- | --- |
| `thprac/src/thprac/thprac_th14.cpp` | 开发组 TH14 实现（现有钩子与地址） |
| `docs/th14/thprac_th14_hooks.md` | TH14 钩子总表 |
| `docs/th15/th15_missbomb_recording.md` | TH15 miss/bomb 记录功能实现说明（boss 阶段名方案已定案） |
| `docs/th15/th15_re_disasm_report.md` | TH15 第一轮报告（§1.1 门控标志、§1.2 boss 指针缓存、§4 miss/bomb 钩子上下文） |
| `docs/th15/th15_re_disasm_report2.md` | TH15 第二轮报告（§3.2 名字字符串为唯一稳定标识） |
| `docs/th15/th15_re_disasm_report3.md` | TH15 第三轮报告（§2 名字串真实偏移与写前旧值方案） |

---

*创建：2026-08-22 20:44*

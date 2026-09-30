# TH15 反编译报告 — 游戏时长统计门控标志位

> 对应总方案：`docs/gametime/gametime_plan.md`；逐作标志位总表：`docs/gametime/gametime_thxx_flags.md`。
> 二进制：`../exes/th15.exe`，951808 B，PE `TimeDateStamp = 1444028071`，`textSize = 0xBCA00`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。
> **二进制同一性已核对**：`TimeDateStamp = 1444028071`、`textSize = 772608 (0xBCA00)`，与 `thprac/src/thprac/thprac_launcher_games_def.h` 的 th15 ORIGINAL 条目（`1444028071, 772608`）一致。
> 方法：沿用 th13 / th14 的 **capstone 只读反汇编**（解析 PE 节表 → 按 trace 字符串绝对地址反查引用 → 在函数入口处反汇编定位位操作）。**未**做真机三态验证（本机为 macOS，验证项见 `docs/gametime/gametime_runtime_verification.md`）。

---

## 0. 结论速览

**"正在有效游玩" = 下列三条合取**（三者互相独立）：

| 量 | 指针链（VA） | RVA | 宽度 | 判据 | 含义 |
| --- | --- | --- | --- | --- | --- |
| `gamemode` | `*(u32*)0x4e9bb8`（PLAYER_PTR） | `0x0E9BB8` | 4 | `!= 0`（单级） | 关卡进行中（玩家对象已创建） |
| `pause` | `[0x4e9a94] + 0x90`（PauseInf，bit4） | 一级 `0x0E9A94`，二级 `+0x90` | 4 | `& 0x10 == 0`（二级） | 游戏自身暂停菜单未打开 |
| `replay` | `[0x4e9bc4] + 0xc`（ReplayInf，mode） | 一级 `0x0E9BC4`，二级 `+0xc` | 4 | `!= 1`（二级） | 未在播放 replay |

```cpp
static const THGameTimeGate gGateTh15 {
    { 0x0E9BB8, 4, 0,    GateCmp::Ne       },        // gamemode：PLAYER_PTR != 0
    { 0x0E9A94, 4, 0x10, GateCmp::BitClear, 0x90 },   // pause：[PauseInf]+0x90 bit4
    { 0x0E9BC4, 4, 1,    GateCmp::Ne,       0x0C },   // replay：[ReplayInf]+0xc != 1
};
```

> th15 与 **th13 / th14 同为"堆对象 + 静态指针、二级读"形态**：暂停位是宿主对象 `+偏移` 的 bit4，回放是 ReplayInf 的 mode（`== 1`）。差异仅在偏移：th13 的 pause 字段是 `+0x60`、th14 是 `+0x80`、th15 是 `+0x90`；回放 mode 字段 th13/th14 在 `+0x10`、th15 在 `+0xc`（对象布局更靠前）。
>
> ⚠️ **本报告推翻了 `gGateTh15` 的两条旧值**：pause 旧值 `0x0E9B24` 是全程序 0 引用的伪地址（§1），replay 旧值 `0x4E7ECC != 0xd` 是场景状态（§2.4 记为弃用候选）。两者均已按本报告更正。

---

## 1. 「未暂停」标志（`0x4e9a94 + 0x90` bit4）

### 1.1 结论

**`[0x4e9a94] + 0x90` 的 bit4（掩码 `0x10`），4 字节。`0x4e9a94` 是堆对象（PauseInf）的静态指针变量，必须二级读。**

- `& 0x10 != 0` ⇒ 暂停菜单打开（冻结）
- `& 0x10 == 0` ⇒ 未暂停

### 1.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| trace 字符串锚点 | VA `0x4cd24c` / `0x4cd244` | `"Pause"` / `"UnPause"`（`"src\game\pause.cpp:207 PauseInf"` @ `0x4cd120`、`"initialize PauseInf"` @ `0x4cd0f4`） |
| 构造器 | `0x43cb80` | `push 0xbc; call alloc` → `0x43cbd6: or dword ptr [esi+0x90],4`（bit2 常量）→ `0x43cbe3: mov [0x4e9a94],esi`（存指针）→ `0x43cbe9: mov [esi+0xb4],edi`（存 mode） |
| 清零点 | `0x43cb08` | `mov dword ptr [0x4e9a94],0` |
| **置位（开暂停）** | `0x450e2a`（函数 `0x450e00`） | `0x450e17: mov ebx,[0x4e9a94]` → `0x450e2a: or [ebx+0x90],0x10`，随后 trace `"Pause"`（`0x450eea`） |
| **置位（开暂停）** | `0x451006`（函数 `0x450fa0`） | `0x450fea: mov eax,[0x4e9a94]` → `0x451006: or [eax+0x90],0x10` |
| **置位（开暂停）** | `0x451192`（函数 `0x451100`） | `0x451113: mov edi,[0x4e9a94]` → `0x451192: or [edi+0x90],0x10`，随后 trace `"Pause"`（`0x4511c5`） |
| **置位（开暂停）** | `0x4512e0`（函数 `0x4512a0`） | `0x4512b3: mov eax,[0x4e9a94]` → `0x4512e0: or [eax+0x90],0x10` |
| **清位（恢复）** | `0x452942`（函数 `0x452900`） | `0x45293d: mov eax,[0x4e9a94]` → `0x452942: and [eax+0x90],0xffffffef`，随后 trace `"UnPause"`（`0x452a50`） |

- 全程序对 `+0x90` 的 `or imm 0x10` 只有 **4 处**、`and imm 0xffffffef` 只有 **1 处**，且**全部**以 `mov reg,[0x4e9a94]` 为基址 ⇒ 该位语义单一，就是暂停/冻结。
- 宿主对象 `[esi+0xb4]` 另存一个 mode（0/1），供 `ReplayMgr::Start` 选择录制/回放、并让暂停键在回放期失效（见 §2.2）。**该 mode ≠ 本判据**，本判据只读 `+0x90` 的 bit4。

### 1.3 旧值 `0x0E9B24` 的止损说明（重要）

原 `gGateTh15` 把 pause 写成**单级静态** `0x0E9B24`（VA），注释"StageController + 0x90"——这是把**指针变量** `0x4e9a94` 当成了静态对象基址、再加 `0x90` 算出来的地址。事实上：

- `0x4e9a94` 是**指针变量**（全程序无 `lea reg,[0x4e9a94]`，读法一律 `mov reg,[0x4e9a94]`）。
- `0x4e9b24` 在 th15.exe 中**被引用 0 次**（无读无写）⇒ 原门控读到的是无关 `.data`，**pause 判据恒通过**（暂停期间也会计时）。
- 修正后必须二级读 `[0x4e9a94]+0x90`。

---

## 2. 「非 replay」标志（`0x4e9bc4 + 0xc`）

### 2.1 结论

**`[0x4e9bc4] + 0xc`（ReplayInf 的 mode 字段），4 字节。`0x4e9bc4` 是静态指针变量，必须二级读。**

- `== 1` ⇒ 正在播放 replay
- `== 0` ⇒ 正常游玩（录制中）
- 其它值 ⇒ 清理/关闭路径，非回放

### 2.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| ReplayInf 构造器 | `0x45b630` | 入参 `eax = [ebp+8]`；`0x45b63b: mov [ebx+0xc],eax`（存 mode）；`0x45b640: test eax,eax; jne 0x45b9aa` |
| `mode == 0`（录制） | `0x45b64c` | `mov [0x4e9bc4],ebx`；`0x45b666: mov [ebx+0xbc],eax`（分配头）；`0x45b690: mov dword ptr [eax],0x72353174`（小端 = `"t15r"`） |
| `mode == 1`（回放） | `0x45b9aa` / `0x45b9b6` | `cmp eax,1; jne 0x45bb3e` → `0x45b9b6: mov [0x4e9bc4],ebx`，随后 `call 0x45cc80`（读回放文件）、`rep movsd` 复制回放头到 `[0x4e9a94]+0x24` |
| **发布时机** | `0x43c46b`（函数 `0x43bff0`，关卡起始） | `0x43c465: mov ecx,[esi+0xb4]`（esi = PauseInf）→ `call 0x45be60`（`ReplayMgr::Start`）。即**每关起始调用一次**，mode 取自 `[PauseInf+0xb4]` |
| `ReplayMgr::Start` | `0x45be60` | `alloc 0x31c` → `push 0x4e9aa0(路径) / push edi(mode)` → `call 0x45b630`（构造 ReplayInf） |
| 回放期禁用暂停 | `0x451119` / `0x4512b8` | `cmp dword ptr [reg+0xb4],1; jne <暂停分支>` —— `[PauseInf+0xb4] == 1`（回放）时按 Esc 不进入暂停 ⇒ 与 mode 语义互证 |
| replay 资源串 | `0x4cd3f4` `"initialize ReplayInf"`、`0x4cd494` `"shutdown ReplayInf"`、`0x4cd4f0` `"replay/%s"`、`0x4cd234` `"th15_%.2d.rpy"`、`0x4cdd30` `"demo0.rpy"`（表 `0x4ce1ec`） | 定位 replay 代码区域的锚点 |

- `[0x4e9bc4]` 全程序只在**构造器**的 mode 0 / mode 1 两个分支里被写入（`0x45b64c` / `0x45b9b6`），由 `ReplayMgr::Start` 每关发布一次。
- 与 th13 的 `[0x4c22c8]+0x10 == 1`、th14 的 `[0x4db688]+0x10 == 1` **同构**，只是 mode 字段偏移不同（`+0xc`）。

### 2.3 语义

与 th12 / th13 / th14 的 ReplayManager mode 完全一致：mode 0 = 录制（正常游玩），mode 1 = 回放。回放播放时玩家对象同样存在 ⇒ `gamemode` 为真，**必须**靠本判据排除。

> ⚠️ thprac 现用的 replay 读取走的是 replay **数据头**里的字段，与本表的 `+0xc`（游戏自己的 mode）**不是同一个量**，勿混用。
>
> ⚠️ **指针生命周期**：`[0x4e9bc4]` 只有构造器两处写入，**没有清零点**（退出/回放结束时对象被 `free`@`0x43c7cc`，但指针未置 0）。因此理论上退出关卡后可能悬空。门控中 `gamemode` 排在最前，只有玩家对象存在（关卡中）时才会读 replay；而 ReplayInf 在同一关卡起始发布 ⇒ 正常游玩期间指针有效。**仍需真机确认**（见验证清单）。

### 2.4 弃用候选：场景状态 `*(u32*)0x4e7ecc != 0xd`

旧结论把 replay 定为场景状态 `0x4E7ECC != 0xD`（`0xd` 在 `0x4609a0`（attract/replay 启动：从 `0x4ce1ec` 名表取回放名、置 `0x4e7794` bit6、设 `0x4e9bd8`）与 `0x468489` 写入；与 th10 的 `GS_MODE==0xd` 回放分支同构）。本报告改为 §2.1 的 ReplayInf mode，理由：

- ReplayInf mode 是游戏自己判定"是否回放"的量（`ReplayMgr::Start` 的 mode 直接区分录制/回放），语义单一、可直接引用；
- 场景状态 `0x4e7ecc` 在回放播放中是否稳定保持 `0xd` **未证实**（全程序对 `0x4e7ecc == 0xd` 没有消费点，只见到写入），若中途改变会导致回放被误计。

> 该候选**留作真机三态阶段的对照采样**（见验证清单），不作为当前 gate。

---

## 3. 「关卡进行中」标志（`*(u32*)0x4e9bb8`）

### 3.1 结论

**`*(u32*)0x4e9bb8 != 0`（单级读）。`0x4e9bb8` 即 thprac 既有 `PLAYER_PTR`。**

- `!= 0` ⇒ 玩家对象存在 ⇒ 关卡进行中
- `== 0` ⇒ 标题 / 菜单 / 关卡销毁后

### 3.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| thprac 既有定义 | `thprac_th15.cpp:14` | `PLAYER_PTR = 0x4e9bb8`；`:736/:740` 等用 `GetMemContent(PLAYER_PTR)` 判在局内 |
| **赋值（创建玩家，原地构造）** | `0x453365`（函数 `0x453300`） | `push 0x2c0a0; ...; push esi; call 0x49b080`（构造 0x2c0a0 字节对象）→ `mov [0x4e9bb8],esi` |
| **赋值（创建玩家，堆分配）** | `0x45404a`（函数 `0x453fd0`） | `push 0x2c0a0; call alloc` → 构造 → `mov [0x4e9bb8],esi` |
| **清零点（销毁）** | `0x453f04` | `mov dword ptr [0x4e9bb8],0` |

### 3.3 说明

- 玩家对象（0x2c0a0 字节）每关创建/销毁，是"关卡进行中"的可靠判据（与 th12 / th13 / th14 用 `PLAYER_PTR != 0` 同构）。全程序 150+ 处直接读该指针。
- replay 播放时玩家对象同样存在 ⇒ `gamemode` 为真，**必须**靠 §2 的 replay 判据排除。
- ⚠️ **挂机 attract demo 误计风险**：标题挂机自动演示会加载关卡/玩家对象，`PLAYER_PTR != 0` 可能成立，需真机确认（与 th10~th14 同类风险）。

---

## 4. 与 th13 / th14 的同构对照

th15 与 th13 / th14 是同一套引擎形态（宿主对象为堆对象、挂在静态指针变量后，需二级读），三个量逐条同构：

| 概念 | th13 | th14 | th15 | 形态 |
| --- | --- | --- | --- | --- |
| pause 宿主对象（静态指针变量） | `0x4c2194` | `0x4db558` | `0x4e9a94` | `[ptr]+偏移` bit4 |
| pause 字段偏移 | `+0x60` | `+0x80` | `+0x90` | 位掩码 `0x10` |
| Pause / UnPause trace | `"Pause"`/`"UnPause"` | `"Pause"`/`"UnPause"` | `"Pause"`/`"UnPause"` | 同源 `src\game\pause.cpp` |
| ReplayInf 指针 | `0x4c22c8` | `0x4db688` | `0x4e9bc4` | `[ptr]+偏移 == 1` = 回放 |
| ReplayInf mode 字段 | `+0x10` | `+0x10` | `+0xc` | 0=录制 / 1=回放 |
| 玩家指针 | `0x4c22c4` | `0x4db67c` | `0x4e9bb8` | `!= 0` |

⇒ **同代同构模板成立**，th15 可直接套用（结果已更正进 `gGateTh15`）。

---

## 5. 复现方法

静态（capstone，Python 3，只读）：

1. 解析 PE：`ImageBase = 0x400000`；`.text` RVA `0x1000` / raw `0x400` / size `0xBCA00`；`.rdata` RVA `0xBE000` / raw `0xBCE00`。实现 `va2off` / `off2va`。
2. 在 `.rdata` 找 trace 字符串：`"Pause" = 0x4cd24c`、`"UnPause" = 0x4cd244`。
3. 在整个文件里搜这两个绝对地址的 LE 字节，得到 `push 0x4cd24c` / `push 0x4cd244` 位置 → 向前找函数入口（`int3` 填充后的首字节）再反汇编。
4. 在这些函数内定位 `or dword ptr [reg+0x90],0x10`（`0x450e2a`/`0x451006`/`0x451192`/`0x4512e0`）与 `and dword ptr [reg+0x90],0xffffffef`（`0x452942`），回看 `reg` 来源为 `[0x4e9a94]`。
5. 搜 `mov [0x4e9a94],reg` 找 PauseInf 构造器 `0x43cb80`（`alloc 0xbc` → `or [esi+0x90],4`）；搜 `mov [0x4e9a94],0` 找清零点 `0x43cb08`。
6. ReplayInf：搜 `mov [0x4e9bc4],reg` 找构造器 `0x45b630`（`mov [ebx+0xc],eax` 存 mode、mode 0 写 `"t15r"` / mode 1 读文件）；搜 `call 0x45be60` 找每关起始的 `ReplayMgr::Start` 调用点 `0x43c46b`。
7. 玩家：搜 `mov [0x4e9bb8],reg` / `mov [0x4e9bb8],0` 找创建 `0x453365`/`0x45404a`（`alloc 0x2c0a0`）/ 清零 `0x453f04`。
8. **反证旧 pause 值**：搜 `0x4e9b24` 的 LE 字节，全程序 0 命中 ⇒ 原 gate 恒通过。

> 坑：`.text` 线性反汇编会因内嵌数据/跳表**早期失步**（capstone 从函数中间起扫会解码出伪指令）。可行做法是先用"原始字节搜索 + 在 `int3` 边界后的函数入口处重解码"，或对候选地址做局部对齐。

---

## 6. 待验证（真机三态）

本报告为**静态结论**，状态 🟡（已定案、未三态）。接入后的验证清单见 `docs/gametime/gametime_runtime_verification.md`：

- [ ] 正常游玩 / Esc 暂停 / 播放 replay 三态各采一次，三 flag 取值互不相同且符合判据。
- [ ] 游玩期间确认 `[0x4e9a94]`、`[0x4e9bc4]` 非空（空 → 二级读失败 → 保守不计时，会少计）。
- [ ] **练习模式是否创建 ReplayInf**：若练习不录制回放导致 `[0x4e9bc4] == 0`，会漏计练习时长，需实测确认。
- [ ] **回放指针悬空**：退出关卡/回放后 `[0x4e9bc4]` 未被清零，确认 `gamemode` 短路后不会读到无效内存。
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（误计风险）。
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）。
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹，若地址不同需按变体另配）。
- [ ] 对照采样场景状态 `0x4e7ecc`（候选 §2.4），验证其是否在回放期稳定为 `0xd`（作为备选判据的兜底）。

---

## 7. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-30 | 建档。三条判据全部定出并复核（capstone 只读分析，未三态）：`gamemode = PLAYER_PTR(0x4e9bb8) != 0`、`pause = [PauseInf(0x4e9a94)]+0x90 bit4`、`replay = [ReplayInf(0x4e9bc4)]+0xc != 1`。更正 `gGateTh15`：pause 由伪地址单级 `0x0E9B24`（全程序 0 引用）改为二级读；replay 由场景状态 `0x4E7ECC != 0xd` 改为 ReplayInf mode。 |

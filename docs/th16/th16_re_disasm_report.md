# TH16 反编译报告 — 游戏时长统计门控标志位

> 对应总方案：`docs/gametime/gametime_plan.md`；逐作标志位总表：`docs/gametime/gametime_thxx_flags.md`；需求：`docs/th16/th16_re_disasm_requests.md`。
> 二进制：`../exes/th16.exe`，683520 B，PE `TimeDateStamp = 1501534029`，`textSize = 0x89E00`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。
> **二进制同一性已核对**：`TimeDateStamp = 1501534029`、`textSize = 564736 (0x89E00)`，与 `thprac/src/thprac/thprac_launcher_games_def.h` 的 th16 条目（`1501534029, 564736`）一致。
> 方法：沿用 th13 / th14 的 **capstone 只读反汇编**（解析 PE 节表 → 按 trace 字符串 / replay 魔数绝对地址反查引用 → 在函数入口处反汇编定位位操作）。**未**做真机三态验证（本机为 macOS）。

---

## 0. 结论速览

**"正在有效游玩" = 下列三条合取**（三者互相独立）：

| 量 | 指针链（VA） | RVA | 宽度 | 判据 | 含义 |
| --- | --- | --- | --- | --- | --- |
| `gamemode` | `*(u32*)0x4a6ef8`（PLAYER_PTR） | `0x0A6EF8` | 4 | `!= 0`（单级） | 关卡进行中（玩家对象已创建） |
| `pause` | `[0x4a6dd4] + 0x88`（PauseInf，bit4） | 一级 `0x0A6DD4`，二级 `+0x88` | 4 | `& 0x10 == 0`（二级） | 游戏自身暂停菜单未打开 |
| `replay` | `[0x4a6f08] + 0xc`（ReplayManager，mode） | 一级 `0x0A6F08`，二级 `+0xc` | 4 | `!= 1`（二级） | 未在播放 replay |

```cpp
static const THGameTimeGate gGateTh16 {
    { 0x0A6EF8, 4, 0,    GateCmp::Ne       },        // gamemode：PLAYER_PTR != 0
    { 0x0A6DD4, 4, 0x10, GateCmp::BitClear, 0x88 },   // pause：[PauseInf]+0x88 bit4
    { 0x0A6F08, 4, 1,    GateCmp::Ne,       0x0C },   // replay：[ReplayMgr]+0xc != 1
};
```

> th16 与 **th13 / th14 同构**（同族引擎，宿主对象 `+偏移` 的 bit4 = 暂停；ReplayManager 的 mode = 回放；PLAYER_PTR = 关卡进行中）。形态一致，仅偏移不同：pause 偏移 `+0x88`（th13 `+0x60`、th14 `+0x80`），replay mode 偏移 `+0xc`（th13 / th14 为 `+0x10`）。"堆对象 + 静态指针、二级读"的模板可直接套用。

---

## 1. 「未暂停」标志（`0x4a6dd4 + 0x88` bit4）

### 1.1 结论

**`[0x4a6dd4] + 0x88` 的 bit4（掩码 `0x10`），4 字节。`0x4a6dd4` 是堆对象（PauseInf）的静态指针变量，必须二级读。**

- `& 0x10 != 0` ⇒ 暂停菜单打开（冻结）
- `& 0x10 == 0` ⇒ 未暂停

### 1.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| trace 字符串锚点 | VA `0x492ac4` / `0x492abc` | `"Pause"` / `"UnPause"`（本作无 `pause.cpp` 路径串） |
| 构造器 | `0x42d700`（函数） | `push 0xb4; call alloc` → `0x42d755: or dword ptr [edi+0x88],4` → `0x42d762: mov [0x4a6dd4],edi`（存指针） |
| 析构/清理 | `0x42d780`；清零点 `0x42d681: mov [0x4a6dd4],0` | `mov esi,[0x4a6dd4]` → `call 0x42d200` → `free 0xb4` |
| **置位（开暂停）** | `0x43f111`（函数 `0x43f0f0`） | `0x43f106: mov esi,[0x4a6dd4]` → `0x43f111: or [esi+0x88],0x10`，随后 trace `"Pause"`（`0x43f186`） |
| **置位（开暂停）** | `0x43f2a1`（函数 `0x43f240`） | `0x43f287: mov eax,[0x4a6dd4]` → `0x43f2a1: or [eax+0x88],0x10`，随后 trace `"Pause"`（`0x43f2fd`） |
| **置位（开暂停）** | `0x43f3de`（函数 `0x43f350`） | `0x43f360: mov edx,[0x4a6dd4]` → `0x43f3de: or [edx+0x88],0x10`，随后 trace `"Pause"`（`0x43f3fd`） |
| **置位（开暂停）** | `0x43f552`（函数 `0x43f500`） | `0x43f523: mov eax,[0x4a6dd4]` → `0x43f552: or [eax+0x88],0x10` |
| **清位（恢复）** | `0x43f6c5`（函数 `0x43f6a0`） | `0x43f6a1: mov eax,[0x4a6dd4]` → `0x43f6c5: and [eax+0x88],0xffffffef` |
| **清位（恢复）** | `0x440add`（函数 `0x440a60`） | `0x440ad8: mov eax,[0x4a6dd4]` → `0x440add: and [eax+0x88],0xffffffef`，附近 trace `"UnPause"`（`0x4409ae`） |

- 全程序对 `+0x88` 的 `or/and` 里，**与暂停语义相关（imm = `0x10` / `0xffffffef`）的全部 6 处**，其基址都来自 `mov reg,[0x4a6dd4]` ⇒ 该位语义单一，就是暂停/冻结。
- 同偏移 `+0x88` 另有 `or ...,4`（构造器，bit2 常量）与 `test eax,0x4000`（bit14，另一语义位）⇒ **只做位测试 `& 0x10`，不与整值比较**。

### 1.3 语义

与 th13 / th14 的 PauseInf 同构：bit4 置位 = 暂停/冻结，清位 = 恢复正常游玩。暂停期间 `gamemode` 仍成立，必须靠本判据排除。

---

## 2. 「非 replay」标志（`0x4a6f08 + 0xc`）

### 2.1 结论

**`[0x4a6f08] + 0xc`（ReplayManager 的 mode 字段），4 字节。`0x4a6f08` 是静态指针变量，必须二级读。**

- `== 1` ⇒ 正在播放 replay
- `== 0` ⇒ 正常游玩（录制中）
- 其它值 ⇒ 清理/关闭路径，非回放

### 2.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| ReplayManager 构造器 | `0x447760` | `0x44776b: mov [ebx+0xc],eax`（存 mode，入参 `[ebp+8]`）；`0x44776e: test eax,eax; jne 0x447ac2` |
| `mode == 0`（录制） | `0x44777c` | `mov [0x4a6f08],ebx`；随后 `call 0x449270`（写 replay 头，魔数 `"t16r"` = `0x72363174` @ `0x4477bf`） |
| `mode == 1`（回放） | `0x447ace` | `0x447ac2: cmp eax,1; jne …` → `0x447ace: mov [0x4a6f08],ebx`；随后 `call 0x448c10`（回放加载函数） |
| 回放加载函数校验魔数 | `0x448c10` | `0x448ca5: cmp dword ptr [eax],0x72363174`（`"t16r"`）→ 确认 `mode == 1` 路径就是"播放 replay" |
| **游戏自带判据** | `0x417f86` / `0x418381` / `0x4428b7` | 均为 `mov eax,[0x4a6f08]; cmp dword ptr [eax+0xc],1`（`je` 走回放分支） |
| replay 资源串 | `0x492cf0` `"replay/%s"`、`0x492aac` `"th16_%.2d.rpy"` | 定位 replay 代码区域的锚点 |

### 2.3 语义

与 th13 / th14 的 `[ReplayMgr]+mode == 1` 同构，差异仅在 mode 字段偏移：th16 是 **`+0xc`**（th13 / th14 为 `+0x10`）。构造函数的 mode 0/1 两分支与回放加载函数的魔数校验共同给出可信度背书。

> ⚠️ thprac 现用的 replay 读取走的是 replay **数据头**里的字段，与本表的 `+0xc`（游戏自己的 mode）**不是同一个量**，勿混用。

---

## 3. 「关卡进行中」标志（`*(u32*)0x4a6ef8`）

### 3.1 结论

**`*(u32*)0x4a6ef8 != 0`（单级读）。`0x4a6ef8` 即 thprac 既有 `PLAYER_PTR`。**

- `!= 0` ⇒ 玩家对象存在 ⇒ 关卡进行中
- `== 0` ⇒ 标题 / 菜单 / 关卡销毁后

### 3.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| thprac 既有定义 | `thprac_th16.cpp:13` | `PLAYER_PTR = 0x4a6ef8` |
| **赋值（创建玩家）** | `0x441cac`（构造器 `0x441c60`） | `push 0x2c828; call alloc` → 初始化 → `mov dword ptr [0x4a6ef8],esi` |
| **清零点（销毁）** | `0x441b18`（函数） | `mov dword ptr [0x4a6ef8],0` |

### 3.3 说明

- 玩家对象每个关卡创建/销毁，是"关卡进行中"的可靠判据（与 th13 / th14 用 `PLAYER_PTR != 0` 同构）。
- replay 播放时玩家对象同样存在 ⇒ `gamemode` 为真，**必须**靠 §2 的 replay 判据排除。
- ⚠️ **挂机 attract demo 误计风险**：标题挂机自动演示会加载关卡/玩家对象，`PLAYER_PTR != 0` 可能成立，需真机确认（与 th10~th14 同类风险）。

---

## 4. 与 th13 / th14 的同构对照

th16 与 th13 / th14 是同一套引擎形态（宿主对象为堆对象、挂在静态指针变量后，需二级读），三个量逐条同构，仅偏移不同：

| 概念 | th13 | th14 | th16 | 形态 |
| --- | --- | --- | --- | --- |
| pause 宿主对象（静态指针变量） | `0x4c2194` | `0x4db558` | `0x4a6dd4` | `[ptr]+偏移` bit4 |
| pause 字段偏移 | `+0x60` | `+0x80` | `+0x88` | 位掩码 `0x10` |
| Pause / UnPause trace | `"Pause"`/`"UnPause"` | `"Pause"`/`"UnPause"` | `"Pause"`/`"UnPause"` | 同源 |
| ReplayManager 指针 | `0x4c22c8` | `0x4db688` | `0x4a6f08` | mode 字段 == 1 = 回放 |
| ReplayManager mode 偏移 | `+0x10` | `+0x10` | **`+0xc`** | 同族、本作偏移不同 |
| replay 魔数 | `"t13r"` | `"t13r"`（沿用） | `"t16r"` | 本作区分 |
| 玩家指针 | `0x4c22c4` | `0x4db67c` | `0x4a6ef8` | `!= 0` |

⇒ **同族同构模板成立**，th16 可直接套用（结果已接入 `gGateTh16`）。

---

## 5. 复现方法

静态（capstone，Python 3，只读）：

1. 解析 PE：`ImageBase = 0x400000`；`.text` RVA `0x1000`；`.rdata` RVA `0x8B000`。实现 `va2off` / `off2va`。
2. 在 `.rdata` 找 trace 字符串：`"Pause" = 0x492ac4`、`"UnPause" = 0x492abc`。
3. 在整个文件里搜这两个绝对地址的 LE 字节，得到 `push 0x492ac4` / `push 0x492abc` 位置 → 向前找函数入口（`int3` 填充后的首字节）再反汇编。
4. 在这些函数内定位 `or dword ptr [reg+0x88],0x10`（`0x43f111`/`0x43f2a1`/`0x43f3de`/`0x43f552`）与 `and dword ptr [reg+0x88],0xffffffef`（`0x43f6c5`/`0x440add`），回看 `reg` 来源为 `[0x4a6dd4]`。
5. 搜 `mov [0x4a6dd4], reg` 找 PauseInf 构造器 `0x42d700`（`alloc 0xb4` → `or [edi+0x88],4`），搜 `mov [0x4a6dd4],0` 找清零点 `0x42d681`。
6. ReplayManager：搜 replay 魔数 `"t16r"`（`0x72363174`）的立即数，定位构造器 `0x447760`（`mov [ebx+0xc],eax` 存 mode、`mov [0x4a6f08],ebx` 存指针）与回放加载函数 `0x448c10`（`cmp [eax],0x72363174`）；再搜 `mov reg,[0x4a6f08]; cmp dword ptr [reg+0xc],1` 交叉验证。
7. 玩家：搜 `mov [0x4a6ef8],reg` / `mov [0x4a6ef8],0` 找创建 `0x441cac`（构造器 `0x441c60`，`alloc 0x2c828`）/ 清零 `0x441b18`。

> 坑：`.text` 线性反汇编会因内嵌数据/跳表**早期失步**（capstone 从函数中间起扫会解码出伪指令）。可行做法是先用"原始字节搜索 + 在 `int3` 边界后的函数入口处重解码"，或对候选地址做局部对齐。

---

## 6. 待验证（真机三态）

本报告为**静态结论**，状态 🟡（已定案、未三态）。接入后的验证清单见 `docs/gametime/gametime_runtime_verification.md`：

- [ ] 正常游玩 / Esc 暂停 / 播放 replay 三态各采一次，三 flag 取值互不相同且符合判据。
- [ ] 游玩期间确认 `[0x4a6dd4]`、`[0x4a6f08]` 非空（空 → 二级读失败 → 保守不计时，会少计）。
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（误计风险）。
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）。
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹，若地址不同需按变体另配）。
- [ ] `+0x88` 的其它位（bit2 常量、bit14 `0x4000`）在游玩期间是否可能被置位——本 gate 只做 `& 0x10` 位测试，理论不受影响，实测顺带确认。

---

## 7. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-30 | 建档。三条判据全部定出（capstone 只读分析，未三态）：`gamemode = PLAYER_PTR(0x4a6ef8) != 0`、`pause = [PauseInf(0x4a6dd4)]+0x88 bit4`、`replay = [ReplayMgr(0x4a6f08)]+0xc != 1`。新增 `gGateTh16` 并接入 launcher。 |

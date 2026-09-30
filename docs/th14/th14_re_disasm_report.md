# TH14 反编译报告 — 游戏时长统计门控标志位

> 对应总方案：`docs/gametime/gametime_plan.md`；逐作标志位总表：`docs/gametime/gametime_thxx_flags.md`；需求：`docs/th14/th14_re_disasm_requests.md`。
> 二进制：`../exes/th14.exe`，897536 B，PE `TimeDateStamp = 1376634201`，`textSize = 0xB0000`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。
> **二进制同一性已核对**：`TimeDateStamp = 1376634201`、`textSize = 720896 (0xB0000)`，与 `thprac/src/thprac/thprac_launcher_games_def.h` 的 th14 ORIGINAL 条目（`1376634201, 720896`）一致。
> 方法：沿用 th13 的 **capstone 只读反汇编**（解析 PE 节表 → 按 trace 字符串绝对地址反查引用 → 在函数入口处反汇编定位位操作）。**未**做真机三态验证（本机为 macOS）。

---

## 0. 结论速览

**"正在有效游玩" = 下列三条合取**（三者互相独立）：

| 量 | 指针链（VA） | RVA | 宽度 | 判据 | 含义 |
| --- | --- | --- | --- | --- | --- |
| `gamemode` | `*(u32*)0x4db67c`（PLAYER_PTR） | `0x0DB67C` | 4 | `!= 0`（单级） | 关卡进行中（玩家对象已创建） |
| `pause` | `[0x4db558] + 0x80`（PauseInf，bit4） | 一级 `0x0DB558`，二级 `+0x80` | 4 | `& 0x10 == 0`（二级） | 游戏自身暂停菜单未打开 |
| `replay` | `[0x4db688] + 0x10`（ReplayManager，mode） | 一级 `0x0DB688`，二级 `+0x10` | 4 | `!= 1`（二级） | 未在播放 replay |

```cpp
static const THGameTimeGate gGateTh14 {
    { 0x0DB67C, 4, 0,    GateCmp::Ne       },        // gamemode：PLAYER_PTR != 0
    { 0x0DB558, 4, 0x10, GateCmp::BitClear, 0x80 },   // pause：[PauseInf]+0x80 bit4
    { 0x0DB688, 4, 1,    GateCmp::Ne,       0x10 },   // replay：[ReplayMgr]+0x10 != 1
};
```

> th14 与 **th13 几乎完全同构**（同代引擎，宿主对象 `+偏移` 的 bit4 = 暂停；ReplayManager `+0x10` = mode；PLAYER_PTR = 关卡进行中）。差异仅在 PauseInf 的字段偏移：th13 是 `+0x60`，th14 是 `+0x80`。B 代"堆对象 + 静态指针、二级读"的模板可直接套用。

---

## 1. 「未暂停」标志（`0x4db558 + 0x80` bit4）

### 1.1 结论

**`[0x4db558] + 0x80` 的 bit4（掩码 `0x10`），4 字节。`0x4db558` 是堆对象（PauseInf）的静态指针变量，必须二级读。**

- `& 0x10 != 0` ⇒ 暂停菜单打开（冻结）
- `& 0x10 == 0` ⇒ 未暂停

### 1.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| trace 字符串锚点 | VA `0x4bedbc` / `0x4bedb4` | `"Pause"` / `"UnPause"`（同源 `src\game\pause.cpp`；`"src\game\pause.cpp:216 PauseInf"` @ `0x4bec38`） |
| 构造器 | `0x436560`（函数） | `push 0x9c; call alloc` → `0x4365ac: or dword ptr [esi+0x80],4` → `0x4365b9: mov [0x4db558],esi`（存指针） |
| 析构器 | `0x4365d0` | `mov esi,[0x4db558]` → `call 0x4360e0` → `free`；清零点 `0x4364ee: mov [0x4db558],0` |
| **置位（开暂停）** | `0x448e24`（函数 `0x448bf0`） | `0x448dfa: mov edx,[0x4db558]` → `0x448e24: or [edx+0x80],0x10` |
| **置位（开暂停）** | `0x448f8d`（函数 `0x448ef0`） | `0x448f69: mov eax,[0x4db558]` → `0x448f8d: or [eax+0x80],0x10` |
| **置位（开暂停）** | `0x449e0a`（函数 `0x449d30`） | `0x449d40: mov ecx,[0x4db558]` → `0x449e0a: or [ecx+0x80],0x10` |
| **置位（开暂停）** | `0x449f8a`（函数 `0x449f20`） | `0x449f30: mov ecx,[0x4db558]` → `0x449f8a: or [ecx+0x80],0x10` |
| **置位（开暂停）** | `0x44b22a`（函数 `0x44a320`） | `0x44b225: mov eax,[0x4db558]` → `0x44b22a: or [eax+0x80],0x10` |
| **清位（恢复）** | `0x449043`（函数 `0x449020`） | `0x449020: mov eax,[0x4db558]` → `0x449043: and [eax+0x80],0xffffffef`，随后 trace `"UnPause"`（`0x449077`） |
| **清位（恢复）** | `0x44aee9`（函数 `0x44a320`） | `0x44aeda: mov eax,[0x4db558]` → `0x44aee9: and [eax+0x80],0xffffffef` |

- 全程序对 `+0x80` 的 `or/and` 里，**与暂停语义相关（imm = `0x10` / `0xffffffef`）的全部 7 处**，其基址都来自 `mov reg,[0x4db558]` ⇒ 该位语义单一，就是暂停/冻结。
- 同偏移 `+0x80` 另有 `or ...,4`（构造器，bit2 常量）与 `or ...,0x800`（`0x4367f1`，另一语义位）⇒ **只做位测试 `& 0x10`，不与整值比较**。

### 1.3 语义

与 th13 的 PauseInf（`+0x60` bit4）完全同构：bit4 置位 = 暂停/冻结，清位 = 恢复正常游玩。暂停期间 `gamemode` 仍成立，必须靠本判据排除。

---

## 2. 「非 replay」标志（`0x4db688 + 0x10`）

### 2.1 结论

**`[0x4db688] + 0x10`（ReplayManager 的 mode 字段），4 字节。`0x4db688` 是静态指针变量，必须二级读。**

- `== 1` ⇒ 正在播放 replay
- `== 0` ⇒ 正常游玩（录制中）
- 其它值 ⇒ 清理/关闭路径，非回放

### 2.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| ReplayManager 构造器 | `0x454630` | `0x45463b: mov [ebx+0x10],eax`（存 mode，入参 `[ebp+8]`）；`0x454640: test eax,eax; jne 0x4549aa` |
| `mode == 0`（录制） | `0x45464c` | `mov [0x4db688],ebx`；随后写 replay 魔数 `0x45469a: mov dword ptr [eax],0x72333174`（小端 = `"t13r"`） |
| `mode == 1`（回放） | `0x4549b6` | `0x4549aa: cmp eax,1; jne 0x454b38` → `0x4549b6: mov [0x4db688],ebx`，随后加载 replay 头到全局 |
| 指针清零 | `0x454e2b` | `mov eax,[0x4db688]; xor ecx,ecx; cmp ebx,eax; cmove eax,ecx; mov [0x4db688],eax`（`if (replayMgr == this) replayMgr = 0`） |
| **游戏自带判据** | `0x41d640` | `mov ecx,[0x4db688]; xor eax,eax; cmp dword ptr [ecx+0x10],1; sete al; ret` —— 即 `IsReplay() { return replayMgr->mode == 1; }` |
| 另一处同判据 | `0x41cbef` / `0x41d502` / `0x436ca3` / `0x44deb4` | 均为 `mov reg,[0x4db688]; cmp dword ptr [reg+0x10],1`，与访问器一致 |
| 回放分支用 mode | `0x455349` / `0x455e72` | 在 replay 相关函数里以 `[reg+0x10] == 1` 判定回放 |
| replay 资源串 | `0x4bef5c` `"initialize ReplayInf"`、`0x4beffc` `"shutdown ReplayInf"`、`0x4bf058` `"replay/%s"`、`0x4bed98` `"th14_%.2d.rpy"` | 定位 replay 代码区域的锚点 |

### 2.3 语义

与 th13 的 `[ReplayMgr]+0x10 == 1` 完全一致（连构造函数形状、mode 0/1 两分支都逐条同构）。`0x41d640` 是游戏自己算出来的"是否回放"访问器，可直接作为判据来源的可信度背书。

> ⚠️ thprac 现用的 replay 读取走的是 replay **数据头**里的字段，与本表的 `+0x10`（游戏自己的 mode）**不是同一个量**，勿混用。
> ℹ️ 本作 replay 魔数为 `"t13r"`（`0x72333174`），疑为 ZUN 沿用上一代常量；与门控无关，仅记以备查。

---

## 3. 「关卡进行中」标志（`*(u32*)0x4db67c`）

### 3.1 结论

**`*(u32*)0x4db67c != 0`（单级读）。`0x4db67c` 即 thprac 既有 `PLAYER_PTR`。**

- `!= 0` ⇒ 玩家对象存在 ⇒ 关卡进行中
- `== 0` ⇒ 标题 / 菜单 / 关卡销毁后

### 3.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| thprac 既有定义 | `thprac_th14.cpp:16` | `PLAYER_PTR = 0x4db67c` |
| **赋值（创建玩家）** | `0x44c44a`（函数） | `push 0x184c0; call alloc` → `mov dword ptr [0x4db67c],edi` |
| **清零点（销毁）** | `0x44d17a`（函数） | `mov dword ptr [0x4db67c],0` |

### 3.3 说明

- 玩家对象每个关卡创建/销毁，是"关卡进行中"的可靠判据（与 th13 用 `PLAYER_PTR != 0` 同构）。
- replay 播放时玩家对象同样存在 ⇒ `gamemode` 为真，**必须**靠 §2 的 replay 判据排除。
- ⚠️ **挂机 attract demo 误计风险**：标题挂机自动演示会加载关卡/玩家对象，`PLAYER_PTR != 0` 可能成立，需真机确认（与 th10/11/12/13 同类风险）。

---

## 4. 与 th13 的同构对照

th14 与 th13 是同一套引擎形态（宿主对象为堆对象、挂在静态指针变量后，需二级读），三个量逐条同构：

| 概念 | th13 | th14 | 形态 |
| --- | --- | --- | --- |
| pause 宿主对象（静态指针变量） | `0x4c2194` | `0x4db558` | `[ptr]+偏移` bit4 |
| pause 字段偏移 | `+0x60` | `+0x80` | 位掩码 `0x10` |
| Pause / UnPause trace | `"Pause"`/`"UnPause"` | `"Pause"`/`"UnPause"` | 同源 `src\game\pause.cpp` |
| ReplayManager 指针 | `0x4c22c8` | `0x4db688` | `[ptr]+0x10 == 1` = 回放 |
| ReplayManager mode 字段 | `+0x10` | `+0x10` | 同 |
| 玩家指针 | `0x4c22c4` | `0x4db67c` | `!= 0` |

⇒ **同代同构模板成立**，th14 可直接套用（结果已接入 `gGateTh14`）。

---

## 5. 复现方法

静态（capstone，Python 3，只读）：

1. 解析 PE：`ImageBase = 0x400000`；`.text` RVA `0x1000` / raw `0x400` / size `0xB0000`；`.rdata` RVA `0xB1000` / raw `0xB0400`。实现 `va2off` / `off2va`。
2. 在 `.rdata` 找 trace 字符串：`"Pause" = 0x4bedbc`、`"UnPause" = 0x4bedb4`。
3. 在整个文件里搜这两个绝对地址的 LE 字节，得到 `push 0x4bedbc` / `push 0x4bedb4` 位置 → 向前找函数入口（`int3` 填充后的首字节）再反汇编。
4. 在这些函数内定位 `or dword ptr [reg+0x80],0x10`（`0x448e24`/`0x448f8d`/`0x449e0a`/`0x449f8a`/`0x44b22a`）与 `and dword ptr [reg+0x80],0xffffffef`（`0x449043`/`0x44aee9`），回看 `reg` 来源为 `[0x4db558]`。
5. 搜 `mov [0x4db558], reg` 找 PauseInf 构造器 `0x436560`（`alloc 0x9c` → `or [esi+0x80],4`），搜 `mov [0x4db558],0` 找清零点 `0x4364ee`。
6. ReplayManager：搜 `mov [0x4db688],reg` 找构造器 `0x454630`（`mov [ebx+0x10],eax` 存 mode）；并搜"访问器形态" `xor eax,eax; cmp dword ptr [reg+0x10],1; sete al` 得到 `0x41d640`。
7. 玩家：搜 `mov [0x4db67c],reg` / `mov [0x4db67c],0` 找创建 `0x44c44a`（`alloc 0x184c0`）/ 清零 `0x44d17a`。

> 坑：`.text` 线性反汇编会因内嵌数据/跳表**早期失步**（capstone 从函数中间起扫会解码出伪指令）。可行做法是先用"原始字节搜索 + 在 `int3` 边界后的函数入口处重解码"，或对候选地址做局部对齐。

---

## 6. 待验证（真机三态）

本报告为**静态结论**，状态 🟡（已定案、未三态）。接入后的验证清单见 `docs/gametime/gametime_runtime_verification.md`：

- [ ] 正常游玩 / Esc 暂停 / 播放 replay 三态各采一次，三 flag 取值互不相同且符合判据。
- [ ] 游玩期间确认 `[0x4db558]`、`[0x4db688]` 非空（空 → 二级读失败 → 保守不计时，会少计）。
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（误计风险）。
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）。
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹，若地址不同需按变体另配）。
- [ ] `+0x80` 的其它位（bit2 常量、bit11 `0x800`）在游玩期间是否可能被置位——本 gate 只做 `& 0x10` 位测试，理论不受影响，实测顺带确认。

---

## 7. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-30 | 建档。三条判据全部定出（capstone 只读分析，未三态）：`gamemode = PLAYER_PTR(0x4db67c) != 0`、`pause = [PauseInf(0x4db558)]+0x80 bit4`、`replay = [ReplayMgr(0x4db688)]+0x10 != 1`。新增 `gGateTh14` 并接入 launcher。 |

# TH13 反编译报告 — 游戏时长统计门控标志位

> 对应总方案：`docs/gametime/gametime_plan.md`；逐作标志位总表：`docs/gametime/gametime_thxx_flags.md`。
> 二进制：`../exes/th13.exe`，790528 B，PE `TimeDateStamp = 1313589413`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（不重定位 ⇒ `RVA = VA − 0x400000`）。
> **二进制同一性已核对**：`TimeDateStamp = 1313589413`、`textSize = 656384 (0xA0400)`，与 `thprac/src/thprac/thprac_launcher_games_def.h` 的 th13 ORIGINAL 条目（`1313589413, 656384`）一致。
> 方法：与 th10 / th11 / th12 同轮的 **capstone 只读反汇编**（解析 PE 节表 → 按地址反查交叉引用 → 定位 trace 字符串锚点）。**未**做真机三态验证。

---

## 0. 结论速览

**"正在有效游玩" = 下列三条合取**（三者互相独立）：

| 量 | 指针链（VA） | RVA | 宽度 | 判据 | 含义 |
| --- | --- | --- | --- | --- | --- |
| `gamemode` | `*(u32*)0x4c22c4`（PLAYER_PTR） | `0x0C22C4` | 4 | `!= 0`（单级） | 关卡进行中（玩家对象已创建） |
| `pause` | `[0x4c2194] + 0x60`（PauseInf，bit4） | 一级 `0x0C2194`，二级 `+0x60` | 4 | `& 0x10 == 0`（二级） | 游戏自身暂停菜单未打开 |
| `replay` | `[0x4c22c8] + 0x10`（ReplayManager，mode） | 一级 `0x0C22C8`，二级 `+0x10` | 4 | `!= 1`（二级） | 未在播放 replay |

```cpp
static const THGameTimeGate gGateTh13 {
    { 0x0C22C4, 4, 0,    GateCmp::Ne       },        // gamemode：PLAYER_PTR != 0
    { 0x0C2194, 4, 0x10, GateCmp::BitClear, 0x60 },   // pause：[PauseInf]+0x60 bit4
    { 0x0C22C8, 4, 1,    GateCmp::Ne,       0x10 },   // replay：[ReplayMgr]+0x10 != 1
};
```

> th13 虽被称作"新一代引擎"，但门控形态与 **th12 几乎完全同构**（宿主对象 `+0x60` bit4 = 暂停；ReplayManager `+0x10` = mode；宿主对象 `+0x74` = 传给 `ReplayManager::Start` 的 mode）。B 代"堆对象 + 静态指针、二级读"的模板可直接套用。

---

## 1. 「未暂停」标志（`0x4c2194 + 0x60` bit4）

### 1.1 结论

**`[0x4c2194] + 0x60` 的 bit4（掩码 `0x10`），4 字节。`0x4c2194` 是堆对象（PauseInf）的静态指针变量，必须二级读。**

- `& 0x10 != 0` ⇒ 暂停菜单打开（冻结）
- `& 0x10 == 0` ⇒ 未暂停

### 1.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| trace 字符串锚点 | VA `0x4abb38` / `0x4abb40` | `"Pause"` / `"UnPause"`（与 th10/11/12 同源，`src\game\pause.cpp:214 PauseInf`） |
| **置位（开暂停）** | `0x43e46a`（函数 `0x43e3f0`） | `mov eax,[0x4c2194]` → `or dword ptr [eax+0x60],0x10`，同函数随后 trace `"Pause"`（`0x43e546`） |
| **置位（开暂停）** | `0x43e61d`（函数 `0x43e5a0`） | `mov eax,[0x4c2194]` → `or dword ptr [eax+0x60],0x10`，随后 trace `"Pause"`（`0x43e6bd`） |
| **置位（暂停菜单分支）** | `0x440aba`（函数 `0x440960`） | `mov eax,[0x4c2194]` → `or dword ptr [eax+0x60],0x10` |
| **清位（恢复）** | `0x43e71b` | `mov eax,[0x4c2194]` → `and dword ptr [eax+0x60],0xffffffef`，随后 trace `"UnPause"`（`0x43e729`） |
| **清位（退出/回标题）** | `0x440769`（函数 `0x43fa90`） | `mov eax,[0x4c2194]` → `and dword ptr [eax+0x60],0xffffffef` |

- **全程序对 `+0x60` 的 `or/and` 只有这 5 处**，且**全部**以 `[0x4c2194]` 为基址 ⇒ 该位语义单一，就是暂停/冻结。
- 宿主对象构造器 `0x42c490`：`push 0x7c; call alloc` → 分配 `0x7c` 字节 → `0x42c4de: or [esi+0x60],4` → `0x42c4e7: mov [0x4c2194],esi` → `0x42c4ed: mov [esi+0x74],eax`（存 mode）。销毁清零点 `0x42c40d: mov [0x4c2194],0`。
- **是"指针变量"不是"静态对象"**：全程序没有把 `0x4c2194` 当对象基址使用（无 `lea reg,[0x4c2194]`），读法一律 `mov reg,[0x4c2194]` / 写指针 `mov [0x4c2194],…`。⇒ 必须二级读（与 th15 那个"把指针当静态对象"的坑不同）。

### 1.3 语义

与 th10/11/12 的 PauseInf 一致：bit4 置位 = 暂停/冻结，清位 = 恢复正常游玩。暂停期间 `gamemode` 仍成立，必须靠本判据排除。

---

## 2. 「非 replay」标志（`0x4c22c8 + 0x10`）

### 2.1 结论

**`[0x4c22c8] + 0x10`（ReplayManager 的 mode 字段），4 字节。`0x4c22c8` 是静态指针变量，必须二级读。**

- `== 1` ⇒ 正在播放 replay
- `== 0` ⇒ 正常游玩（录制中）
- 其它值（如 `2`）⇒ 清理/关闭路径，非回放

### 2.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| ReplayManager 构造器 | `0x447900` | `0x447909: mov [ebx+0x10],eax`（存 mode）；`0x44790e: jne 0x447c0c`（mode != 0 时走非录制分支） |
| `mode == 0`（录制） | `0x44791b` | `mov [0x4c22c8],ebx`；写 replay 魔数 `0x447961: mov dword ptr [eax],0x72333174`（小端 = `"t13r"`） |
| `mode == 1`（回放） | `0x447c0c` | `cmp eax,1; jne 0x447d3c` → 走 replay 读取分支（`0x447c15: mov [0x4c22c8],ebx`，随后加载 replay 头到全局） |
| `mode == 2` | `0x447d3c` | `cmp eax,2; jne …` → 打印/清理路径，非回放 |
| **游戏自带判据** | `0x413c60` | `mov ecx,[0x4c22c8]; xor eax,eax; cmp dword ptr [ecx+0x10],1; sete al; ret` —— 即 `IsReplay() { return replayMgr->mode == 1; }` |
| 另一处同判据 | `0x443515` | `mov edx,[0x4c22c8]; cmp dword ptr [edx+0x10],1; je …`（replay 分支） |
| 录制 tick 用 mode | `0x449072` | `mov edi,[0x4c22c8]; mov eax,[edi+0x10]; test eax,eax; jne …`（mode != 0 跳过录制） |
| 指针生命周期 | 写 `0x44791b` / `0x447c15`，清 `0x447ea1: mov [0x4c22c8],0` | 指针变量，非空时才可解引用 |

### 2.3 语义

与 th11 / th12 的 `[ReplayMgr]+0x10 == 1` 完全一致。`0x413c60` 是游戏自己算出来的 "是否回放" 访问器，可直接作为判据来源的可信度背书。

> ⚠️ thprac 现用的 `REPLAY_MGR_PTR → +0x1c → +0xc`（如 `thprac_th13.cpp:429` 一带读 replay 名）读的是回放**数据头**里的字段，与本表的 `+0x10`（游戏自己的 mode）**不是同一个量**，勿混用。

---

## 3. 「关卡进行中」标志（`*(u32*)0x4c22c4`）

### 3.1 结论

**`*(u32*)0x4c22c4 != 0`（单级读）。`0x4c22c4` 即 thprac 既有 `PLAYER_PTR`。**

- `!= 0` ⇒ 玩家对象存在 ⇒ 关卡进行中
- `== 0` ⇒ 标题 / 菜单 / 关卡销毁后

### 3.2 证据链（静态）

| 环节 | 位置 | 内容 |
| --- | --- | --- |
| thprac 既有定义 | `thprac_th13.cpp:14` | `PLAYER_PTR = 0x4c22c4`；`:663/:667` 用 `GetMemContent(PLAYER_PTR)` 判 in-game |
| **赋值（创建玩家）** | `0x441d82`（函数 `0x441c90`） | `push 0x14844; call alloc` → `mov dword ptr [0x4c22c4],edi` |
| **清零点（销毁）** | `0x4427d0`（函数 `0x442700`） | `mov dword ptr [0x4c22c4],0` |

### 3.3 说明

- 玩家对象每个关卡创建/销毁，是"关卡进行中"的可靠判据（与 th12 用 `PLAYER_PTR != 0` 同构）。
- replay 播放时玩家对象同样存在 ⇒ `gamemode` 为真，**必须**靠 §2 的 replay 判据排除。
- ⚠️ **挂机 attract demo 误计风险**：标题挂机自动演示会加载关卡/玩家对象，`PLAYER_PTR != 0` 可能成立，需真机确认（与 th10/11/12 同类风险）。

---

## 4. 对"新一代引擎"的说明

th13 常被视为引擎换代（GameManager / PauseInf / ReplayManager 的对象划分更清晰、菜单状态机更大），但就本功能需要的三个量而言：

| 概念 | th12 | th13 | 形态 |
| --- | --- | --- | --- |
| pause 宿主对象（静态指针变量） | `0x4b44e8` | `0x4c2194` | `[ptr]+0x60` bit4 |
| 暂停 trace 字符串 | `"Pause"`/`"UnPause"` | `"Pause"`/`"UnPause"` | 同源 `src\game\pause.cpp` |
| 宿主对象 `+0x74` | `ReplayManager::Start` 的 mode | 同 | 非 gamemode |
| ReplayManager 指针 | `0x4b4518` | `0x4c22c8` | `[ptr]+0x10 == 1` = 回放 |
| 玩家指针 | `0x4b4514` | `0x4c22c4` | `!= 0` |

⇒ **B 代同构模板成立**，th13 可直接套用（结果已接入 `gGateTh13`）。

---

## 5. 复现方法

静态（capstone，Python 3）：

1. 解析 PE：`.text` RVA `0x1000` / raw `0x400` / size `0xA0275`；`.rdata` RVA `0xA2000` / raw `0xA0800`；`ImageBase = 0x400000`。
2. 在 `.rdata` 找 trace 字符串：`"Pause" = 0x4abb38`、`"UnPause" = 0x4abb40`。
3. 在整个文件里搜这两个绝对地址的 LE 字节，得到操作数位置 → 向前对齐一位得到 `mov edi, 0x4abb38` / `mov edi, 0x4abb40`，即暂停/恢复函数。
4. 在暂停函数内定位 `or dword ptr [eax+0x60],0x10`（`0x43e46a`/`0x43e61d`），回看 `eax` 来源为 `[0x4c2194]`；恢复函数定位 `and dword ptr [eax+0x60],0xffffffef`。
5. ReplayManager：搜 `mov [0x4c22c8],…` 找构造器 `0x447900`，读 `[ebx+0x10]` 的赋值与分支；并核对游戏自带访问器 `0x413c60`。
6. 玩家：搜 `mov dword ptr [0x4c22c4],…` 找赋值 `0x441d82` / 清零 `0x4427d0`。

> 坑：th13 的 `.text` 线性反汇编会因内嵌数据/跳表**早期失步**（capstone `disasm(text, 0x401000)` 直接扫会漏大量指令）。可行做法是先用「原始字节搜索 + 局部对齐重解码」定位候选指令，再在函数入口处反汇编整个函数。

---

## 6. 待验证（真机三态）

本报告为**静态结论**，状态 🟡（已定案、未三态）。接入后的验证清单见 `docs/gametime/gametime_runtime_verification.md`：

- [ ] 正常游玩 / Esc 暂停 / 播放 replay 三态各采一次，三 flag 取值互不相同且符合判据。
- [ ] 游玩期间确认 `[0x4c2194]`、`[0x4c22c8]` 非空（空 → 二级读失败 → 保守不计时，会少计）。
- [ ] 确认标题挂机 attract demo 下 `PLAYER_PTR != 0` 是否成立（误计风险）。
- [ ] Continue 菜单是否被 pause 判据覆盖（th07/th08 曾有漏计）。
- [ ] STEAM / 汉化变体是否共用同一 RVA（该 gate 条目同时覆盖原版与 Steam 指纹，若地址不同需按变体另配）。

---

## 7. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-09-30 | 建档。三条判据全部定出（capstone 只读分析，未三态）：`gamemode = PLAYER_PTR(0x4c22c4) != 0`、`pause = [0x4c2194]+0x60 bit4`、`replay = [0x4c22c8]+0x10 != 1`。新增 `gGateTh13` 并接入 launcher。 |

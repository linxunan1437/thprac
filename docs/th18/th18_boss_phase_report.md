# TH18 Boss 段监控 — 方案二（读 boss 宿主里 ECL 当前 sub 名 + 白名单 latch）

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志）
> 总括（术语口径以此为准）：`docs/logger/logger_re_disasm_overview.md`
> 模板 / 上位：`docs/th16/th16_boss_phase_report.md`（TH16 方案二定案；本报告采用其**方案二**）
> 同作（道中已定案，本报告是其「boss 段」续篇）：`docs/th18/th18_ecl_re_report.md`（**尤其 §1 / §1.1 偏移差异表 / §4 / §5**）
> 回填工作流：`docs/logger/checkpoint_entry_backfill_workflow.md`；素材清单与来源：`docs/logger/decompile_materials.md`
> 二进制指纹：`../exes/th18.exe`，**847360 B**，PE `TimeDateStamp = 1618619144`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；本次核对 `.text` = VA `0x401000`–`0x4AD000`，RVA `0x1000`–`0xAC000`）
> 引擎代：**C 代（第四代引擎）**；同代参照作 th15/16/17（本作偏移**逐项不同**，见 §2.5 换算表）。
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` **只读静态反汇编**（用「4 字节小端 VA 立即数」做字节搜索定位 xref；线性反汇编会因内嵌数据/跳表失步，故关键结论均以**字节模式搜索 + 定点反汇编**复核）；`RELOCS_STRIPPED` ⇒ `RVA = VA − 0x400000`。
> ECL / decl 素材：`../exes/ecl/th18/`（原始 `.ecl` 22 份）、`../exes/ecl_dump/th18/`（`thecl -d` 反编译 `.decl` 22 份；**本作无 eclmap**，指令呈 `ins_*` 形态，sub 名与 `@call` 结构正常）。
> 状态：**静态调研（🟡 未真机三态）**。本机 macOS 不可编译、不可真机；未真机验证的地址/结论文中均标 🟡。
> 地址口径：正文以 **VA** 为主，并同时标注 **RVA = VA − 0x400000**。
> 只读：本报告为新建文件；对既有文件的改动仅为：回填 `docs/logger/th18_checkpoints.csv` 的 boss 行第 1 列、在 `docs/th18/th18_ecl_re_report.md` 末尾追加交叉引用与变更记录（均见 §5 / §8）。

---

## 0. 结论速览

| 维度 | 结论 | 一句话 |
| --- | --- | --- |
| **boss 宿主是否成立（方案二）** | ✅ **成立（静态）** | boss ECL 跑在 **boss 敌人自己的宿主**。「当前 boss 宿主」可由 **`mgr+0x48`（槽→id 表，槽 0 = 主 boss 🟡）** 或 **遍历 `mgr+0x18C` 对象链 + 门控位 `host+0x635C & 0x800000`** 取得；二者都落到同一 `mgr`（`*(u32*)0x4CF2D0`）。 |
| **取 host 的两个函数** | ✅ | 按**槽**：`FUN_004237F0(slot)`（= th15 `FUN_0048F7F0` / th16 `FUN_00417580` 的本作同构）；按 **id**：`FUN_0042D500(id)`（= th16 `FUN_0041A9C0` 同构）；id 是否存在：`FUN_00409990(id)`；注册槽：`FUN_0042D490(slot,host)`。 |
| **boss 门控位（本作偏移）** | ✅ | **`host+0x635C` bit23（`0x800000`）**（= boss 子对象 `B'+0x5130`，`B' = host+0x122C`）。th16 的 `boss+0x526C` **不能照搬**。 |
| **当前 sub 名读取链** | ✅ 复核通过 | `thread=*(host+0xC)`；`subIdx=*(thread+4)`；`res=*(host+0x1218)`（= `*(*(0x4CF2D0)+0x188)`）；`table=*(res+0x8C)`；`name=*(char**)(table+subIdx*8)`。与道中同链。 |
| **boss 对象 `B'`** | ✅ | `B' = host + 0x122C`（th16 为 `host+0x120C`，**本作差 +0x20**）；`*(B'+0x5588) == host`（自指针落在 `host+0x67B4`，唯一写点 `0x42E208`）。 |
| **boss 脚本 / 阶段名** | ✅ | `stNNbs`（主 boss）/ `stNNmbs`（道中 boss）子脚本名为 `Boss`/`Boss1..N`/`BossCard1..N`/`BossDead`、`MBoss`/`MBoss1..N`/`MBossCardN`/`MBossDead`。**`setNext` = `ins_514`（opcode `0x202`，4 参 `(slot, hp, timeout, "下一阶段名")`）**；`ins_22(id,"名")` = 章节名/调试名。 |
| **boss 行回填** | 🟡 已回填（静态推导） | 以 **`THPatch` 的 `ins_514` 槽号 / `Set Spell Ordinal` / `Change Nonspell` 常量** + `.decl` 阶段链推出每行入口 sub（见 §4.3 / §5），已写入 `docs/logger/th18_checkpoints.csv` 的 **65 行** boss 行。 |
| **真机验证** | ❌ 未做 | 本机无法运行 th18；全部为静态结论。 |

> **一句话**：TH18 的「方案二」= **TH16 方案二的同构版**，只是把 `mgr` / `res` / 对象链 / boss 门控 / boss 子对象等偏移整体换成本作值（`mgr+0x18C` 链、`host+0x6830` id、`host+0x635C` 门控、`B'=host+0x122C`）。

---

## 1. 方案二机制（为什么读 boss 宿主，不读主线宿主）

- boss ECL（`Boss`/`BossCardN`/`MBoss`…）由关卡脚本用 `ins_301("Boss"/"MBoss", …)`（opcode `0x12D` = `enmCreateA`）**异步生成 boss 敌人**，其 ECL **跑在 boss 敌人自己的宿主体内**，**不在主线宿主**（`stNN.ecl` 的 `main` 所跑的宿主）。
  - 佐证：`../exes/ecl_dump/th18/st01.decl:1306-1320` `void MainBoss(){ … ins_301("Boss", 144.0f, -16.0f, 40, 1000, 0); … }`；`st01.decl:1397-1407` `MainMBossDebug(){ … ins_301("MBoss", …) }`。（`ins_301` 的字符串即新宿主起始 sub 名。）
- ⇒ **照搬道中「读主线宿主」（`host+0x10` / `host+0xC`）做 boss，会恒读到 `main`/主线 sub**；做 boss 段**必须先取得 boss 宿主**。
- 取得 boss 宿主后，读「当前 sub 名」的指针链与道中**完全一致**（同一 `res`/`table`/`thread+4`），再走**路过式 checkpoint（latch）**白名单（命中即置「当前段落」，未命中 no-op）。

---

## 2. boss 宿主取法（本作版）

### 2.1 管理器与对象链（本作偏移）

`M = *(u32*)0x4CF2D0`（ENEMY_MANAGER，thprac 常量 `ENEMY_MANAGER_PTR = 0x4cf2d0`，`thprac_th18.cpp:57`）。

| 字段 | 偏移（相对 `M`） | 含义 | 证据（VA / RVA） |
| --- | --- | --- | --- |
| `M+0x48` | `+0x48` | **槽→id 表**（`M + 0x48 + slot*4` = 该槽 host 的 **id**；**槽 0 = 主 boss**（🟡，见 §6）） | 写侧 `FUN_0042D490` @ `0x42D4AA`（`mov [edx+eax*4+0x48],ecx`，`ecx=[host+0x6830]`）；读侧 `FUN_004237F0` @ `0x4237FF`（`mov edx,[esi+eax*4+0x48]`） |
| `M+0x18C` | `+0x18C` | **对象链头**（双向链表；节点 `[0]=host`、`[4]=next`、`[8]=prev`） | `FUN_004237F0` @ `0x42380E`（`mov eax,[esi+0x18c]`）+ `0x423820`（`mov ecx,[eax]`）；宿主析构 `FUN_0042D220` 摘链 `0x42D264` 起 |
| `M+0x190` / `M+0x194` | `+0x190/+0x194` | **另两条对象链头**（同类节点，析构 `0x42D278`/`0x42D28C` 一并摘除；本报告主用 `+0x18C`） | `0x42D278` / `0x42D28C` |
| `M+0x90` / `M+0x94` | `+0x90/+0x94` | 宿主序号计数 / 上一序号 | 宿主构造 `FUN_0042E100` @ `0x42E3FC`-`0x42E40B` |
| `M+0x188` | `+0x188` | ECL 资源对象 `res` | `FUN_0042E100` @ `0x42E431`（`mov eax,[ecx+0x188]`）→ `0x42E439`（`mov [esi+0x1218],eax`） |

**host 对象内关键字段**（host 尺寸 `0x683C`）：

| 字段 | 偏移（相对 host） | 含义 | 证据（VA / RVA） |
| --- | --- | --- | --- |
| `+0x000C` | `+0xC` | 根线程 `thread`（= `host+0x10`，`FUN_0042E100` @ `0x42E444`） | `0x42E444`（`mov eax,[esi+0xc]`） |
| `+0x1218` | `+0x1218` | ECL 资源对象 `res` | `0x42E439` |
| `+0x6830` | `+0x6830` | **宿主 id**（由 `M+0x90` 计数器分配） | `FUN_0042E100` @ `0x42E3F6`（`mov [esi+0x6830],eax`） |
| `+0x1514` | `+0x1514` | **`M+0x18C` 链节点**（`node[0]=host`） | 构造 `0x42E2A2`（`mov [esi+0x1514],esi`）；析构摘链 `0x42D252` 起 |
| `+0x67B4` | `+0x67B4` | **host 自指针**（`*(host+0x67B4)==host`） | 构造 `0x42E208`（`mov [esi+0x67b4],esi`） |
| `+0x122C` | `+0x122C` | **boss 子对象 `B'` 基址**（th16 为 `host+0x120C`） | 构造 `0x42E1CC`（`lea eax,[esi+0x122c]`）；访问器 `FUN_0042D210` @ `0x42D210`（`lea eax,[ecx+0x122c]`） |
| `+0x635C` | `+0x635C` | **boss 门控标志**（= `B'+0x5130`）；**bit23 `0x800000` = 当前 boss** | 置位 `0x433BF5`（`or [edi+0x5130],0x800000`）；清除 `0x433BE2`（`and ecx,0xff7fffff`）；`test [esi+0x635c],0x800000` @ `0x42D2FC` / `0x42EED2` |
| `+0x636C` | `+0x636C` | **boss 槽位**（= `B'+0x5140`；`-1` = 未注册） | 构造初始化 `0x42E298`（`mov [esi+0x636c],0xffffffff`）；写 `0x433C13`（`mov [edi+0x5140],eax`）；析构清槽 `0x42D308`（`mov eax,[esi+0x636c]`） |
| `+0x5588`（B' 相对） | = `host+0x67B4` | `*(B'+0x5588) == host`（自指针，th16 为 `B+0x44B8`） | 访问助手 `FUN_00438AA0` @ `0x438AA3`（`mov ecx,[ecx+0x5588]; mov ecx,[ecx+0xc]`） |

### 2.2 「按槽 / 按 id 取 host」的函数（本作版）

**按槽取 host —— `FUN_004237F0(slot)`**（= th15 `FUN_0048F7F0` / th16 `FUN_00417580` 的同构）：

```asm
004237f0  55                push ebp
004237f1  8bec              mov  ebp, esp
004237f3  8b4508            mov  eax, [ebp+8]              ; slot
004237f6  33c9              xor  ecx, ecx
004237f8  56                push esi
004237f9  8b35d0f24c00      mov  esi, [0x4cf2d0]           ; esi = M
004237ff  8b548648          mov  edx, [esi+eax*4+0x48]     ; ★ id = M+0x48+slot*4
00423803  85d2              test edx, edx
00423805  7507              jne  0x42380e                  ; id==0 → 返回 0
00423807  33c0              xor  eax, eax
00423809  5e                pop  esi
0042380a  5d                pop  ebp
0042380b  c20400            ret  4
0042380e  8b868c010000      mov  eax, [esi+0x18c]          ; ★ eax = M+0x18C 链头
00423814  85c0              test eax, eax
00423816  7419              je   0x423831
00423818  0f1f840000000000  nop  [eax+eax]
00423820  8b08              mov  ecx, [eax]                ; ★ node[0] = host
00423822  399130680000      cmp  [ecx+0x6830], edx         ; ★ [host+0x6830] == id ?
00423828  7407              je   0x423831
0042382a  8b4004            mov  eax, [eax+4]              ; node = node[4]
0042382d  85c0              test eax, eax
0042382f  75ef              jne  0x423820
00423831  8bc1              mov  eax, ecx                   ; ★ 返回 host（未命中则 0）
00423833  5e                pop  esi
00423834  5d                pop  ebp
00423835  c20400            ret  4
```

**按 id 取 host —— `FUN_0042D500(id)`**（同一循环，直接给 id）：

```asm
0042d500  55                push ebp
0042d501  8bec              mov  ebp, esp
0042d503  8b5508            mov  edx, [ebp+8]              ; id
0042d506  33c9              xor  ecx, ecx
0042d508  85d2              test edx, edx
0042d50a  7506              jne  0x42d512
0042d50c  33c0              xor  eax, eax                   ; id==0 → 0
0042d50e  5d                pop  ebp
0042d50f  c20400            ret  4
0042d512  a1d0f24c00        mov  eax, [0x4cf2d0]
0042d517  8b808c010000      mov  eax, [eax+0x18c]          ; M+0x18C 链头
0042d51d  85c0              test eax, eax
0042d51f  7411              je   0x42d532
0042d521  8b08              mov  ecx, [eax]
0042d523  399130680000      cmp  [ecx+0x6830], edx
0042d529  7407              je   0x42d532
0042d52b  8b4004            mov  eax, [eax+4]
0042d52e  85c0              test eax, eax
0042d530  75ef              jne  0x42d521
0042d532  8bc1              mov  eax, ecx                   ; ★ 返回 host
0042d534  5d                pop  ebp
0042d535  c20400            ret  4
```

**id 是否存在 —— `FUN_00409990(id)`**（同链，返回 `0/1`）：

```asm
00409990  55                push ebp
00409991  8bec              mov  ebp, esp
00409993  8b5508            mov  edx, [ebp+8]
00409996  85d2              test edx, edx
00409998  7427              je   0x4099c1
0040999a  a1d0f24c00        mov  eax, [0x4cf2d0]
0040999f  8b808c010000      mov  eax, [eax+0x18c]
004099a5  85c0              test eax, eax
004099a7  7418              je   0x4099c1
004099b0  8b08              mov  ecx, [eax]
004099b2  399130680000      cmp  [ecx+0x6830], edx
004099b8  740d              je   0x4099c7
004099ba  8b4004            mov  eax, [eax+4]
004099bd  85c0              test eax, eax
004099bf  75ef              jne  0x4099b0
004099c1  33c0              xor  eax, eax
...
004099c7  b801000000        mov  eax, 1
```

**注册槽 —— `FUN_0042D490(slot, host)`**（把 host 的 id 写入 `M+0x48+slot*4`；`host==0` 则清槽）：

```asm
0042d490  55                push ebp
0042d491  8bec              mov  ebp, esp
0042d493  51                push ecx
0042d494  8b4d0c            mov  ecx, [ebp+0xc]             ; arg2 = host
0042d497  8b15d0f24c00      mov  edx, [0x4cf2d0]           ; M
0042d49d  8b4508            mov  eax, [ebp+8]              ; arg1 = slot
0042d4a0  85c9              test ecx, ecx
0042d4a2  7410              je   0x42d4b4
0042d4a4  8b8930680000      mov  ecx, [ecx+0x6830]         ; host id
0042d4aa  894c8248          mov  [edx+eax*4+0x48], ecx     ; ★ M+0x48+slot*4 = id
0042d4ae  8be5              mov  esp, ebp
0042d4b0  5d                pop  ebp
0042d4b1  c20800            ret  8
0042d4b4  c744824800000000  mov  [edx+eax*4+0x48], 0       ; 清槽
```

### 2.3 boss 门控位与「当前 boss」的两种取法

**门控位**：boss 行为派发器（`edi = B'`）里置/清：

```asm
; 置位（boss 激活）—— @ 0x433BF3
00433bf3  ff36              push [esi]
00433bf5  81c900008000      or   ecx, 0x800000               ; ecx = B'+0x5130
00433bfb  898f30510000      mov  [edi+0x5130], ecx          ; ★ B'+0x5130 |= 0x800000
00433c01  8b0dd0f24c00      mov  ecx, [0x4cf2d0]           ; M
00433c07  50                push eax
00433c08  e88398ffff        call 0x42d490                  ; ★ 注册槽（slot=eax, host=[esi]）
00433c0d  8b8594faffff      mov  eax, [ebp-0x56c]
00433c13  898740510000      mov  [edi+0x5140], eax         ; ★ B'+0x5140 = 槽号
```
```asm
; 清除（boss 退场）—— @ 0x433BE2
00433be2  81e1ffff7fff      and  ecx, 0xff7fffff            ; 清 bit23
00433be8  898f30510000      mov  [edi+0x5130], ecx
```
宿主析构 `FUN_0042D220` 中也由门控位决定清槽：

```asm
0042d2fc  f7865c63000000008000  test dword [esi+0x635c], 0x800000   ; ★ host+0x635C 门控
0042d306  740e                  je   0x42d316
0042d308  8b866c630000          mov  eax, [esi+0x636c]              ; ★ boss 槽位
0042d30e  c744814800000000      mov  dword [ecx+eax*4+0x48], 0      ; 清 M+0x48+slot*4
```

**取「当前 boss 宿主」两种口径**（都自 `M=*(u32*)0x4CF2D0` 出发）：

> **方法 A（按槽，最贴近 thprac 既有 `bossExists`）**：`bossId = *(u32*)(M+0x48)`（**槽 0 = 主 boss**；`thprac_th18.cpp:1208` 即 `GetMemContent(ENEMY_MANAGER_PTR, 0x48)` 作 `bossExists`）；`host = FUN_0042D500(bossId)`（或 `FUN_004237F0(0)`）。
>
> **方法 B（按门控位，无需知道槽号）**：遍历 `node = *(u32*)(M+0x18C)` 链，取 `h = *(u32*)(node)`，命中 `*(u32*)(h+0x635C) & 0x800000` 的 `h` = 当前 boss 宿主。

```cpp
// —— 方法 A ——
uint32_t M      = *(uint32_t*)0x4CF2D0;
uint32_t bossId = *(uint32_t*)(M + 0x48);            // 槽 0 = 主 boss（🟡）
uint32_t host   = FUN_0042D500(bossId);              // 按 id 取 host（0 = 无 boss）

// —— 方法 B（等价，且不依赖槽号）——
uint32_t host = 0;
for (uint32_t node = *(uint32_t*)(M + 0x18C); node; node = *(uint32_t*)(node + 4)) {
    uint32_t h = *(uint32_t*)node;                   // node[0] = host
    if (*(uint32_t*)(h + 0x635C) & 0x800000) { host = h; break; }
}

// —— 两者取得 host 之后，读当前 sub 名（与道中同链）——
uint32_t thread = *(uint32_t*)(host + 0x000C);
int32_t  subIdx = *(int32_t*)(thread + 4);
uint32_t res    = *(uint32_t*)(host + 0x1218);       // = *(u32*)(*(u32*)0x4CF2D0 + 0x188)
uint32_t table  = *(uint32_t*)(res + 0x8C);
const char* name = *(const char**)(table + (uintptr_t)subIdx * 8);
```

### 2.4 boss 子对象 `B'`（与宿主的分工）

- **`B' = host + 0x122C`**（`FUN_0042D210` 返回它；构造时 `0x42E1CC` `lea eax,[esi+0x122c]`）。boss 行为派发器里 `edi = B'`：门控 `B'+0x5130`、槽号 `B'+0x5140`、自指针 `B'+0x5588`。
- **`*(B'+0x5588) == host`**：自指针落在 `host+0x67B4`（唯一写点 `0x42E208`）。访问助手 `FUN_00438AA0` 即 `ecx=[B'+0x5588] → [ecx+0xC] → jmp 0x48D4F0`（取该宿主 ECL 线程的表达式/变量），**自证 `B'` 与 host 的从属关系**。
  - ⚠️ th16 的 `*(B+0x44B8)==host` 在本作变为 **`*(B'+0x5588)==host`**（`B'` 相对），**不能照搬**。

### 2.5 th15 / th16 / th17 → th18 偏移换算表（本作核对项）

| 项 | th15 | th16 | th17 | **th18（本作）** |
| --- | --- | --- | --- | --- |
| 管理器全局 | `0x4E9A80` | `0x4A6DC0` | `0x4B76A0` | **`0x4CF2D0`** ✅ |
| `M → res` | `+0x17C` | `+0x17C` | `+0x17C` | **`+0x188`** ✅ |
| `host → res` | `+0x11F8` | `+0x11F8` | `+0x11F8` | **`+0x1218`** |
| 槽→id 表 | `M+0x48+slot*4` | `M+0x48+slot*4` | — | **`M+0x48+slot*4`**（同） |
| 对象链头 | `M+0x180` | `M+0x180` | — | **`M+0x18C`** |
| host id 字段 | — | `+0x5740` | — | **`+0x6830`** |
| 链节点 | — | `host+0x14F4` | — | **`host+0x1514`** |
| boss 子对象 `B` | — | `host+0x120C` | — | **`host+0x122C`** |
| boss 门控位 | `boss+0x4060` | `boss+0x4060`(=`host+0x526C`) | — | **`B'+0x5130`(= `host+0x635C`)** |
| `B → host` 自指针 | — | `B+0x44B8`(=`host+0x56C4`) | — | **`B'+0x5588`(=`host+0x67B4`)** |
| boss 槽位 | — | `B+0x4070`(=`host+0x527C`) | — | **`B'+0x5140`(=`host+0x636C`)** |
| host 对象尺寸 | `0x574C` | `0x574C` | `0x576C` | **`0x683C`** |
| 按槽取 host | `FUN_0048F7F0` | `FUN_00417580` | — | **`FUN_004237F0`** |
| 按 id 取 host | — | `FUN_0041A9C0` | — | **`FUN_0042D500`** |
| 注册槽 | — | `FUN_0041A910` | — | **`FUN_0042D490`** |

> **结论**：需求文档「本作偏移与 th16/17 不同」的提示成立；`M+0x48`（槽表）与 th16 一致，但 **对象链头 `+0x18C`、host id `+0x6830`、boss 门控 `+0x635C`、`B'=host+0x122C` 均为本作值**。

---

## 3. 当前 sub 名读取链（复核，与道中同链）

在取得 boss `host` 后，读名字的链与道中**逐字段一致**（解释器 `FUN_0048B3A0` 内实测）：

```asm
0048b3cb  8b8718100000      mov  eax, [edi+0x1018]        ; host = thread+0x1018
0048b3d4  f30f100f          movss xmm1, [edi]             ; thread+0 = 时间
0048b3d8  8b8018120000      mov  eax, [eax+0x1218]        ; ★ res  = host+0x1218
0048b3de  8b808c000000      mov  eax, [eax+0x8c]          ; ★ table= res+0x8C
0048b3e4  8b44c804          mov  eax, [eax+ecx*8+4]       ;     table[subIdx].+4 = 本 sub 起始
0048b3e8  83c010            add  eax, 0x10
0048b3eb  03c2              add  eax, edx                 ;     + ip = 当前指令
```

- `thread = *(u32*)(host+0xC)`（根线程；构造/复位时 `host+0xC = host+0x10`，`0x42E444` / `0x42CF08`）。
- `subIdx = *(i32*)(thread+4)`（当前 sub 索引，`-1` = 线程结束/按名解析失败）。
- `res = *(u32*)(host+0x1218) = *(u32*)(*(u32*)0x4CF2D0 + 0x188)`。
- `table = *(u32*)(res+0x8C)`（项 8 B：`+0` = 名字 `char*`、`+4` = sub 起始绝对地址）。
- `name = *(char**)(table + subIdx*8)`（运行期保留，可直接读）。
- 命中点仍为 `0x48C822`（`f3 0f 11 0f` = `movss [edi],xmm1`），`Edi = thread`（**注意 `Edi` 不必然主线**：`spawn` 子线程 / 敌机线程都会命中，`th18_ecl_re_report.md` §5.4）。
- 判定口径：**路过式 checkpoint（latch）**——`name ∈ 白名单 → 置当前段落；不在 → no-op`（不回退 `main`）。

> **boss 宿主取法对读取的影响**：只需把道中的 `host`（主线宿主）替换为 §2 求得的 **boss 宿主**即可；`thread`/`res`/`table` 三级链和 `subIdx +4` 语义**完全复用**。

---

## 4. boss 脚本 / 阶段名

### 4.1 文件槽位与子脚本名

关卡声明 `ecli { … }`（见 `.decl` 头，例 `st01.decl:2`）：`ecli { "default.ecl"; "st01bs.ecl"; "st01mbs.ecl"; }` ⇒ **slot0 = `st01.ecl`（关卡主文件）、slot1 = `default.ecl`、slot2 = `stNNbs.ecl`（主 boss）、slot3 = `stNNmbs.ecl`（道中 boss）**。与 `THPatch` 的 `SetFile(2)`（主 boss）/`SetFile(3)`（道中 boss）一致。

逐作 boss 脚本 sub 名单（`../exes/ecl_dump/th18/`）：

| 关卡 | `stNNbs`（主 boss）sub | `stNNmbs`（道中 boss）sub |
| --- | --- | --- |
| ST1 | `Boss`,`Boss1`,`Boss2`,`BossCard1`,`BossCard2`,`BossDead`,`BossEscape` | `MBoss`,`MBoss1`,`MBossDead`,`MBossEscape` |
| ST2 | +`Boss3`,`BossCard3` | 同形 |
| ST3 | +`Boss3`,`BossCard3`,`BossPhantom01*` | 同形 |
| ST4 | 同 ST3 | 同形 |
| ST5 | `Boss`,`Boss1..Boss4`,`BossCard1..BossCard4`,`BossDead`,`BossEscape`,`BossM` | 同形 |
| ST6 | `Boss`,`Boss1..Boss6`,`BossCard1..BossCard6`,`BossBGMFrame2..7`,`BossDead`,`BossEscape`,`BossM` | 同形 |
| ST7 | `Boss`,`Boss1..Boss10`,`BossCard1..BossCard10`,`BossDead`,`BossDead2`,`BossEscape`,`BossM`,`BossM2` | `MBoss`,`MBoss1..MBoss3`,`MBossCard1..MBossCard3`,`MBossDead`,`MBossEscape` |

### 4.2 `setNext`（opcode `0x202`）= `ins_514`；`ins_22` = 章节名/调试名

- **无 eclmap ⇒ `setNext` 呈现为 `ins_514`**（opcode `0x202` = 514）。参数 4 个：`(slot, hp, timeout, "下一阶段名")`。
  - 对照 th16（有 eclmap）：`setNext(0, 1700, 1500, "BossCard1")`；本作 th18 呈现为 `ins_514(0, 2100, 2100, "BossCard1")`——**参数形态完全同构**。
- **阶段链（`setNext` 链 + `Boss()` 的 `@BossN()` 派发）**，以 ST1 为例（`st01bs.decl`）：

  | 行号 | 片段 | 语义 |
  | --- | --- | --- |
  | `st01bs.decl:36` | `@BossCard1();`（`Boss()` 内，`$SPELL_ID` 分支） | 派发到 spell1 |
  | `st01bs.decl:41` | `@BossCard2();` | 派发到 spell2 |
  | `st01bs.decl:44` | `@Boss1();` | 派发到 nonspell1 |
  | `st01bs.decl:53` | `ins_514(0, 2100, 2100, "BossCard1");`（`Boss1` 内） | `Boss1` 打完 → 下一阶段 `BossCard1` |
  | `st01bs.decl:196` | `ins_514(0, 2200, 2100, "BossCard2");`（`Boss2` 内） | `Boss2` → `BossCard2` |
  | `st01bs.decl:327` | `ins_514(0, 0, 2100, "Boss2");`（`BossCard1` 内） | `BossCard1` → `Boss2` |
  | `st01bs.decl:532` | `ins_514(0, 0, 2100, "BossDead");`（`BossCard2` 内） | `BossCard2` → `BossDead` |
  | `st01mbs.decl:25` | `ins_514(1, 0, 1200, "MBossDead");`（`MBoss` 内） | **槽号 = 1**（道中 boss 用槽 1） |
  | `st01mbs.decl:26` | `ins_521(1, "MBossEscape");` | 逃逸名注册 |

  ⇒ **槽号**：主 boss 用 **slot 0**（`ins_514(0,…)`，与 `M+0x48` 槽 0 = 主 boss 呼应），道中 boss 用 **slot 1**（`ins_514(1,…)`）。
- `ins_22(id, "名")`（opcode `0x16` = th16 的 `debug22`）：在 `Boss()` 内出现（如 `st01bs.decl:43` `ins_22(11, "Boss2")`、`st02bs.decl:47-48` `ins_22(11,"Boss2")/ins_22(12,"Boss3")`），为**章节名/调试名**，非控制流。

### 4.3 阶段链（推出「下一阶段」顺序，供 §5 回填）

| 关卡 | 阶段顺序（`Boss()` 派发 + `setNext` 链） | boss 行数 |
| --- | --- | --- |
| ST1 | `Boss1 → BossCard1 → Boss2 → BossCard2` | 4 |
| ST2 | `Boss1 → BossCard1 → Boss2 → BossCard2 → (Boss3 →) BossCard3` | 5 |
| ST3/ST4 | `Boss1 → BossCard1 → Boss2 → BossCard2 → Boss3 → BossCard3` | 6 |
| ST5 | 同 ST3/4 + `BossCard4` | 7 |
| ST6 | `… Boss4 → BossCard4 → (Boss5/Boss6) → BossCard5 → BossCard6` | 10 |
| ST7 | `Boss1 → BossCard1 → Boss2 → … → Boss8 → BossCard8 → BossCard9 → BossCard10`（`END_NSk`/`END_Sk`） | 18 |

> 注：`BossN` 与 `BossCardN` 是**兄弟 `call`**（`Boss()` 按 `$SPELL_ID`/槽派发，`setNext` 注册「下一阶段」），不是父子；`Boss`/`MBoss` 是**容器**，不入白名单。

---

## 5. boss 行回填进展（`docs/logger/th18_checkpoints.csv`）

### 5.1 状态

- 本报告**已回填** `docs/logger/th18_checkpoints.csv` 的 **65 行 boss 行**（`MID*` / `BOSS*` / `END_*`，仅第 1 列）——**章节行保持原样（未动）**。
- 回填依据：`THPatch`（`thprac_th18.cpp:2690-3560`）各 `case` 的 **`SetFile` 槽 + `Set Spell Ordinal`（`0x3k`）/ `Change Nonspell`（`0x3k`）常量 + `ECLJump` 落点**，对应到 `.decl` 的阶段链（§4.3）；`MID` 行另由 `ECLJump` 落点所进入的 sub 直接确定。
- 置信度：**🟡 静态推导、未真机**。推导链见 §5.2 / §5.3。

### 5.2 `MID` 行（道中 boss）

| 记录名 | 入口 sub | 依据 |
| --- | --- | --- |
| ST1–ST6 `MID1` | `MBoss` | `THPatch` 落点在 `stNN.ecl` 的 boss/道中 boss 生成路径；道中 boss 宿主起始 sub = `MBoss`（`st01.decl:1397` `ins_301("MBoss",…)`） |
| ST7 `MID1` | `MBossCard1` | `THPatch` ST7_MID1 `ECLJump(…,0x428,…)`，`SetFile(3)=st07mbs`；`st07mbs.ecl` 0x428 = `ins_1ff` 紧邻 `call MBossCard1`（0x43c） |
| ST7 `MID2` | `MBossCard2` | `0x4c4` 紧邻 `call MBossCard2`（0x4d8） |
| ST7 `MID3` | `MBossCard3` | `0x560` 紧邻 `call MBossCard3`（0x574） |

### 5.3 `BOSS*` / `END_*` 行（主 boss）

规则：**`BOSS1` = `Boss1`**（boss 宿主起始 sub = `Boss`，经 `Boss()` 派发进入 `Boss1`）；`BOSS_k (k≥2)` 由 `THPatch` 常量给出——**`Set Spell Ordinal 0x3m` ⇒ `BossCardm`；`Change Nonspell 0x3m` ⇒ `Bossm`**。ST7 用 `END_NSk ⇒ Bossk`、`END_Sk ⇒ BossCardk`。

已回填（节选；全部见 CSV）：

| 关卡 | 回填 |
| --- | --- |
| ST1 | `Boss1`,`BossCard1`,`Boss2`,`BossCard2` |
| ST2 | `Boss1`,`BossCard1`,`Boss2`,`BossCard2`,`BossCard3` |
| ST3 | `Boss1`,`BossCard1`,`Boss2`,`BossCard2`,`Boss3`,`BossCard3` |
| ST4 | 同 ST3 |
| ST5 | ST3 + `BossCard4` |
| ST6 | `Boss1`,`BossCard1`,`Boss2`,`BossCard2`,`Boss3`,`BossCard3`,`Boss4`,`BossCard4`,`BossCard5`,`BossCard6` |
| ST7 | `END_NSk=Bossk`(k=1..8)、`END_Sk=BossCardk`(k=1..10) |

### 5.4 遗留 / 不确定

- **`BOSS` 行与游戏实际「攻击阶段」的严格 1:1 未真机核对**：部分关卡（ST2/ST5/ST6）按 `THPatch` 常量推导会**跳过**某些 `BossN`（如 ST2 的 `Boss3`、ST6 的 `Boss5`/`Boss6`）——这是**忠实地**按 thprac 自身 warp 设计得出，但 thprac 行语义与游戏阶段名是否严格对应需真机确认（🟡）。
- ST7 的 `END_NS1` 无显式 `Change Nonspell`（默认首阶段），按 `Boss1` 回填（🟡）。
- 章节行（`STn 前半/后半 #k`）**未在本轮处理**（属道中范畴）。

---

## 6. 风险与未验证点

1. **未真机三态**（🟡）：本机无法运行 th18；`M+0x48` 槽 0 = 主 boss、门控位 `host+0x635C & 0x800000` 的运行时行为均需真机确认。
2. **boss 宿主生命周期**：boss 敌人宿主随退场/换关销毁（析构 `FUN_0042D220` 清槽/摘链）；读名前须做 `host`/`thread`/`res`/`table`/`subIdx` 全链空指针 + 越界保护（`subIdx ∈ [0, *(res+8))`）。
3. **`Edi` 不必然主线**（`0x48C822` 对任意线程命中，`th18_ecl_re_report.md` §5.4）：若在命中点取读，须按 `*(Edi+0x1018)==bossHost` 过滤，或直接在 boss 宿主上读 `*(host+0xC)`。
4. **`host+0x1218` 是否全程指向 boss 自己的 ECL（`stNNbs`/`stNNmbs`）**：静态推定为是（构造时 `host+0x1218 = M+0x188`），需真机核对（🟡）。
5. **boss 行回填的 `BOSS`/`END` 语义**（见 §5.4）：基于 `THPatch` 常量与 `.decl` 阶段链的静态推导，未真机比对游戏阶段顺序。
6. **`MID` 槽号假设**：主 boss 槽 0 / 道中 boss 槽 1 由 `ins_514` 首参与 thprac `bossExists`（`M+0x48`）推得，未真机核对（🟡）。
7. **地区/版本变体**：Steam / 汉化版是否共用同一 RVA，需按变体实测。

---

## 7. 地址表（VA / RVA = VA − 0x400000）

### 7.1 管理器与对象链

| 项 | VA | RVA |
| --- | --- | --- |
| ENEMY_MANAGER 全局 `M` | `0x4CF2D0` | `0xCF2D0` |
| 宿主工厂 `FUN_0042D7D0`（`alloc 0x683C`） | `0x42D7D0` | `0x2D7D0` |
| 宿主构造 `FUN_0042E100` | `0x42E100` | `0x2E100` |
| 宿主析构 `FUN_0042D220` | `0x42D220` | `0x2D220` |
| `B'` 访问器 `FUN_0042D210`（`lea [ecx+0x122c]`） | `0x42D210` | `0x2D210` |
| 按槽取 host `FUN_004237F0` | `0x4237F0` | `0x237F0` |
| 按 id 取 host `FUN_0042D500` | `0x42D500` | `0x2D500` |
| id 存在 `FUN_00409990` | `0x409990` | `0x9990` |
| 注册槽 `FUN_0042D490` | `0x42D490` | `0x2D490` |
| boss ECL 线程访问助手 `FUN_00438AA0` | `0x438AA0` | `0x38AA0` |
| 宿主 self-ptr 写点（`mov [esi+0x67b4],esi`） | `0x42E208` | `0x2E208` |
| `host+0x1218 = res` 写点 | `0x42E439` | `0x2E439` |
| `res = *(M+0x188)` 读点 | `0x42E431` | `0x2E431` |
| host id 写点（`mov [esi+0x6830],eax`） | `0x42E3F6` | `0x2E3F6` |
| boss 槽位初始化（`mov [esi+0x636c],-1`） | `0x42E298` | `0x2E298` |
| boss 子对象 memset（`lea [esi+0x122c]; memset 0x5600`） | `0x42E1CC`/`0x42E1EE` | `0x2E1CC`/`0x2E1EE` |
| 析构：门控测试 + 清槽 | `0x42D2FC`/`0x42D308` | `0x2D2FC`/`0x2D308` |
| boss 门控置位 / 清除（派发器 `B'`） | `0x433BF5`/`0x433BE2` | `0x33BF5`/`0x33BE2` |
| boss 槽号写入（`mov [edi+0x5140],eax`） | `0x433C13` | `0x33C13` |
| 注册槽调用点（派发器） | `0x433C08` | `0x33C08` |
| 门控测试（闩）`test [reg+0x635c],0x800000` | `0x42D2FC` / `0x42EED2` | `0x2D2FC` / `0x2EED2` |

### 7.2 读取链 / 解释器 / 命中点

| 项 | VA | RVA |
| --- | --- | --- |
| 解释器 `FUN_0048B3A0` | `0x48B3A0` | `0x8B3A0` |
| `res/table` 读取链 | `0x48B3CB`–`0x48B3EB` | `0x8B3CB`–`0x8B3EB` |
| 命中点（`movss [edi],xmm1`，4 B） | `0x48C822` | `0x8C822` |
| 线程步进器 `FUN_0048D420` | `0x48D420` | `0x8D420` |
| `spawn` 新线程 `FUN_0048D850`（`push 0x1208`） | `0x48D850` | `0x8D850` |
| 宿主复位（根线程 `host+0x10`） | `0x42CE80` | `0x2CE80` |
| 宿主基构造 | `0x42CE10` | `0x2CE10` |
| 线程基构造 | `0x42CCC0` | `0x2CCC0` |

### 7.3 字面量 / 素材

- 字面量：`"main"` = `0x4B6634`；`"MBossDead"` = `0x4B64A4`；`"BossDead"` = `0x4B64B0`；`"BossEscape"` = `0x4B64BC`；`"MBossBDead"` = `0x4B64C8`；`"BossCard6_et"`/`"BossCard6_et2"` = `0x4B651C`/`0x4B652C`。
- `M` 字段：`+0x48` 槽→id、`+0x18C`/`+0x190`/`+0x194` 对象链头、`+0x188` res、`+0x90`/`+0x94` 计数。
- host 字段：`+0xC` 根线程、`+0x10` 根线程内嵌、`+0x121C` 线程链头、`+0x1218` res、`+0x6830` id、`+0x1514` 链节点、`+0x67B4` 自指针、`+0x122C` `B'`、`+0x635C` 门控、`+0x636C` boss 槽位。
- 离线素材：`../exes/ecl_dump/th18/st01bs.decl:53`（`ins_514(0,2100,2100,"BossCard1")`）、`st01mbs.decl:25`（`ins_514(1,0,1200,"MBossDead")`）；`../exes/ecl_dump/th18/st07mbs.decl` / `st07bs.decl`（ST7 阶段）；`../exes/ecl_dump/th18/st01.decl:1306/1397`（`ins_301("Boss"/"MBoss",…)`）。
- 反汇编：本次用 `pefile` + `capstone 5.0.7` 临时只读脚本（**未入库**；口径同 `docs/th18/th18_ecl_re_report.md` 所述 `reth.py`：PE 解析 + 定点反汇编 + VA 立即数 xref 搜索）；`.ecl` 偏移↔sub 名用临时解析脚本（SCPT 头 `0x24+hlen` → 偏移数组 → 名字表），与 `checkpoint_entry_backfill_workflow.md` §3.2 口径一致。

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | **建档**。为 TH18 产出「方案二（读 boss 宿主里 ECL 当前 sub 名 + 白名单 latch）」逆向结论。**§2 关键结论**：管理器 `M=*(u32*)0x4CF2D0`；**槽→id 表 `M+0x48+slot*4`（槽 0 = 主 boss，thprac `bossExists` 同源）**；**对象链头 `M+0x18C`**（节点 `host+0x1514`，`node[0]=host`）；**按槽取 host `FUN_004237F0`**（= th15 `FUN_0048F7F0` 同构）/ **按 id 取 host `FUN_0042D500`** / id 存在 `FUN_00409990` / 注册槽 `FUN_0042D490`；**host id `+0x6830`**；**boss 门控 `host+0x635C & 0x800000`**；**boss 子对象 `B' = host+0x122C`**（`*(B'+0x5588)==host`，自指针 `host+0x67B4`，写点 `0x42E208`）；boss 槽位 `host+0x636C`。**§3** 复核当前 sub 链 `thread=*(host+0xC)`→`subIdx=*(thread+4)`→`res=*(host+0x1218)`→`table=*(res+0x8C)`→`name`（解释器 `0x48B3CB`–`0x48B3EB`）。**§4** `setNext` = `ins_514`（opcode `0x202`，4 参 `(slot,hp,timeout,"下一阶段名")`；主 boss 槽 0 / 道中 boss 槽 1）；`ins_22` = 章节名；逐关阶段链。**§5** 已回填 `docs/logger/th18_checkpoints.csv` 的 **65 行** boss 行（`MID*`/`BOSS*`/`END_*`，仅第 1 列）：`MID` 由 `ECLJump` 落点 sub 定，`BOSS_k`/`END_Sk`/`END_NSk` 由 `THPatch` 的 `Set Spell Ordinal`/`Change Nonspell` 常量 + `.decl` 阶段链定（🟡 静态推导）。含 th15/16/17→th18 偏移换算表、风险、地址表。状态：静态调研（🟡 未真机三态）。 |

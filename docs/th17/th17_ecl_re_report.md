# TH17 ECL 段落监控 — 逆向结论（Q1–Q9）

> 对应需求：`docs/th17/th17_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`；同代模板：`docs/th15/th15_ecl_re_report.md`。
> 二进制：`../exes/th17.exe`，**747520 B**，PE `TimeDateStamp = 1565429804`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**，绝对 VA 以立即数内嵌，可直接字节搜索 xref）。
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` 只读静态反汇编（**本机无 IDA/Ghidra/radare2**）；
> 与 th15 的 **Ghidra 全量反编译 C**（`../th15decode/th15.exe_decompiled.c`）与 th15 结论逐函数对照；
> 同代同源，th17 的 ECL 解释器与 th15 **结构一致**（指令级基本重合）。
> 状态：**静态定案（🟡 已定案、未真机三态）**。全部结论均有“地址 + 反汇编片段”支撑；无 `.ecl` 可供脚本级实测。
> 地址口径：正文以 **VA** 为主，关键处附 **RVA = VA − 0x400000**；本机 thprac 源码同 VA 口径。
> 只读：本文为新建文件，未改动任何既有文件。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前 sub** | ✅ **`*(i32*)(Edi+4)` = 当前 sub 索引**；`Edi` = th15 意义的 **thread** | 与 th15 完全一致；写入点为 `call`/`ret`/初始化。 |
| **Q2 sub 表** | ✅ **成立**：`res=*(u32*)(host+0x11F8)`，`table=*(u32*)(res+0x8C)`，`count=*(i32*)(res+8)`；表项 8B，`+0`=名字 `char*`、`+4`=起始 | 建表器 `0x47AEF0` + 按名查表 `0x47AE40` 双重自证。 |
| **Q3 函数栈** | ✅ **存在可遍历的显式子调用栈**（与 th15 同形） | `SP=*(u32*)(thread+0x100C)`、栈底 `thread+0xC`、帧长 `0x10`、帧 `+0xC`=调用者 sub；上限 `0x1000`。 |
| **Q4 主线宿主捕获** | ✅ **进关/重开 = `0x431C58` / `0x431E83`（`call 0x41E140`，`ecx=ENEMY_MANAGER`，arg=`"main"`），EAX=宿主** | 捕获点建议 `0x431C5D`/`0x431E88`（call 后，EAX=宿主）；`0x47A712` 命中时 `Edi` 是**当前被步进的 thread**，非必然主线。 |
| **Q5 命中点/调用链** | ✅ 函数入口 `FUN_00478A70=0x478A70`；4B=`movss [edi],xmm1` | 线程步进 `0x47A940`（遍历 `host+0x11FC` 线程链）→ `0x478A70`；可用作稳定钩子（非严格“每帧一次”）。 |
| **Q6 名字来源** | ✅ 运行期保留 sub 名（`table[i].name` 为 `char*`） | 按名查表 `0x47AE40` 逐字节 strcmp，证明名字在内存中。 |
| **Q7 机体/子机** | ✅ `character = 0x4B59F4`、`shotType = 0x4B59F8` | 与 thprac IGI `*0x4B59F4*3 + *0x4B59F8` 一致；读写点均在。 |
| **Q8 练习跳转补丁** | ✅ **不影响 subIdx/名字判读** | `ECLJump` 只写 `goto`(`0x0C`)；`goto` 只改 `thread+0`(time) 与 `thread+8`(ip)，不动 `thread+4`、不动 sub 表。 |
| **Q9 段落判定口径** | ✅ **走 (A) 调用栈祖先法** | 与 th15 同源同形：显式 `call 0x0B` 压帧 / `ret 0x0A` 弹，段落入口 `call` 后驻留栈上。 |

> **关键地址（VA / RVA）**
> 主分发（ECL VM step）`FUN_00478A70` = `0x478A70` / `0x78A70`；压帧 `FUN_00478730` = `0x478730` / `0x8730`；
> `ret`(case `0x0A`) `0x478B0A` / `0x8B0A`；`call`(case `0x0B`) `0x478D22` / `0x8D22`；`goto`(case `0x0C`) `0x478DB0` / `0x8DB0`；
> 越界护栏(case `0x28`) `0x478E1C` / `0x8E1C`；线程步进 `0x47A940` / `0x7A940`；spawn `0x47AD70` / `0x7AD70`；
> 按名查表 `0x47AE40` / `0x7AE40`；**ECL 建表器 `0x47AEF0` / `0x7AEF0`**；当前指令重解码 `0x47B100` / `0x7B100`；
> 宿主/敌人构造 `0x41EA20`（其中 `res` 赋值 `0x41ED6F`）；根线程宿主写入 `0x41D6D8`；新宿主创建返回 `0x41E140`；
> **`"main"` spawn `0x431C58` / `0x431E83`**。命中点 `0x47A712` / `0x7A712`。

---

## 1. Q1 — 当前 sub：`*(i32*)(Edi+4)` 是**索引**，`Edi` = thread

### 1.1 结论

- `Edi` 即 th15 意义的 **ECL 线程对象（thread）**，其字段与 th15 逐一对齐：
  | 偏移 | 类型 | 语义 |
  | --- | --- | --- |
  | `+0x00` | float | 当前 ECL 时间 |
  | `+0x04` | i32 | **当前 sub 索引** |
  | `+0x08` | i32 | 当前 sub 内的 ip offset |
  | `+0x0C` | — | 调用栈数组起点 |
  | `+0x100C` | i32 | 调用栈指针 SP |
  | `+0x1010` | i32 | 表达式/临时栈配对指针 |
  | `+0x1018` | u32 | 所属宿主 `host` |
  | `+0x1020` | byte | 标志位（与指令 `[instr+0xA]` 做 `test`） |
- `thread+4` 的**唯一语义就是“当前 sub 索引”**：它被直接用作下表下标 `table[idx].+4`。

### 1.2 证据（读取点）

主分发 `FUN_00478A70`（`0x478A70`）开头即把 `+4` 当 sub 索引：

```asm
00478a7b  8bf9            mov  edi, ecx              ; edi = thread（this）
00478a87  8b5708          mov  edx,[edi+8]           ; thread+8  = ip
00478a93  8b4f04          mov  ecx,[edi+4]           ; thread+4  = sub 索引   ← Q1
00478a9f  8b8718100000    mov  eax,[edi+0x1018]      ; thread+0x1018 = host
00478aaf  8b80f8110000    mov  eax,[eax+0x11f8]      ;   host+0x11F8 = res
00478ab5  8b808c000000    mov  eax,[eax+0x8c]        ;     res+0x8C   = sub 表
00478abb  8b44c804        mov  eax,[eax+ecx*8+4]     ;       表项[+4]   = sub 起始
00478abf  03f0            add  esi, eax              ; esi = 0x10 + ip + 起始 = 当前指令
```

（RVA：`0x478A70`→`0x78A70`，`0x478A9F`→`0x78A9F`，`0x478ABB`→`0x78ABB`。）
`ret` 后重解码函数 `0x47B100`（RVA `0x7B100`）亦证：

```asm
0047b109  8b5104          mov  edx,[ecx+4]           ; sub 索引
0047b11d  8b808c000000    mov  eax,[eax+0x8c]        ; res+0x8C = sub 表
0047b123  8b44d004        mov  eax,[eax+edx*8+4]     ; 表项[+4] = 起始
0047b127  83c010          add  eax, 0x10
0047b12a  03c6            add  eax, esi              ; = 起始 + 0x10 + ip = 当前指令
```

### 1.3 写入点（`thread+4`）

| 场景 | 地址 | 指令 |
| --- | --- | --- |
| **初始化**（根线程内联于 `host+0x10`，`thread+4`=`host+0x14`） | `0x41D6C7` | `mov dword [ecx+0x14], 0xFFFFFFFF`（先置 -1） |
| 随后按名解析 `"main"` 设为索引 | `0x41ED75` → `0x47AEDB` | `mov [eax+4], esi`（eax=thread） |
| **spawn 新线程**（置 -1） | `0x47ADB9` | `mov dword [esi+4], 0xFFFFFFFF` |
| **`call`(0x0B)**：`thread+4` = 被调 sub 索引（按名解析成功） | `0x47AEDB` | `mov [eax+4], esi` |
| **`call`**：名字解析失败 → -1 | `0x47AEC7` | `mov dword [eax+4], 0xFFFFFFFF` |
| **`ret`(0x0A)**：`thread+4` = 帧[+0xC]（调用者 sub） | `0x478B40` | `mov [edi+4], eax` |
| **`goto`(0x0C)** | — | **不写** `thread+4`（见 §8） |

`call` 的解析路径（`0x47AE40` = `host::SetSubByName(host, name)`，`this`=host）：

```asm
0047aed4  8b45fc          mov  eax,[ebp-4]           ; host
0047aed7  8b400c          mov  eax,[eax+0xc]         ; host+0xC = 当前 thread
0047aedb  897004          mov  [eax+4], esi          ; thread+4 = 解析得到的 sub 索引
```

> **范围**：合法值 `0 .. count-1`（`count=*(i32*)(res+8)`），或 `-1`（无 caller / 解析失败 / 空线程）。
> 需求 `mainLatterIDs[7]={0,87,91,86,88,92,0}` 均为小正整数索引，**索引语义自洽**（若是“指令指针”绝不可能是这些值）。

---

## 2. Q2 — sub 表：`res+0x8C`、表项 8B、`+0`=名字、`+4`=起始、`count=res+8`

### 2.1 完整指针链

```
M       = *(u32*)0x4b76a0                      ; ENEMY_MANAGER 对象        (RVA 0xB76A0)
res     = *(u32*)(M + 0x17C)                   ; ECL 资源对象
        = *(u32*)(host + 0x11F8)               ; 运行期等价链（dispatcher 用此）
host    = *(u32*)(thread + 0x1018)
count   = *(i32*)(res + 8)
table   = *(u32*)(res + 0x8C)
name    = *(char**)(table + subIdx*8)          ; 表项 +0 = 名字 char*
start   = *(u32*)(table + subIdx*8 + 4)        ; 表项 +4 = 起始（= 起始指令绝对地址基准）
```

> ⚠️ 需求文档把链写作 `*(u32*)(ENEMY_MANAGER_PTR+0x17c)`，**更精确应为** `*(u32*)( *(u32*)0x4b76a0 + 0x17C )`：
> `0x4b76a0` 处存放的是 **M（manager 指针）**，`GetMemAddr/GetMemContent` 会先解引用一次。两条链（manager 侧与 host 侧）指向**同一 `res`**，见 §2.3。

### 2.2 建表器（`FUN_0047AEF0`，`0x47AEF0` / RVA `0x7AEF0`）

进入解析 `"SCPT"` 段，`edi`=res：

```asm
0047af10  813953435054      cmp  dword [ecx], 0x54504353   ; "SCPT" 魔数
0047af1c  6683790401        cmp  word  [ecx+4], 1          ; 版本 1
0047af2d  0fb74910          movzx ecx, word [ecx+0x10]     ; 该段 sub 数
0047af31  014f08            add  dword [edi+8], ecx        ; ★ res+8 += count（累计 sub 总数）
0047af36  8bb78c000000      mov  esi, [edi+0x8c]           ; res+0x8C = 现有 sub 表
0047af4b  c1e003            shl  eax, 3                    ; count*8
0047af52  e83d400000        call 0x47ef94                  ; malloc(count*8)
0047af5d  89878c000000      mov  [edi+0x8c], eax           ; ★ res+0x8C = 新 sub 表
...
0047af70  8b4704            mov  eax,[edi+4]
0047af73  8b4c870c          mov  ecx,[edi+eax*4+0xc]       ; res+0xC+idx*4 = SCPT 块指针
0047af7a  0308              add  ecx, dword [eax]          ; + 段内偏移
0047af7c  8b878c000000      mov  eax,[edi+0x8c]
0047af82  894cd004          mov  [eax+edx*8+4], ecx        ; ★ 表项[+4] = sub 起始
0047af88  8b878c000000      mov  eax,[edi+0x8c]
0047af91  891cd0            mov  [eax+edx*8], ebx          ; ★ 表项[+0] = 名字 char*（指向 NUL 结尾串）
0047af94  8a01              mov  al,[ecx]                  ; 扫到 '\0'，量名字长度
0047af96  41                inc  ecx
0047af97  84c0              test al, al
0047af99  75f9              jne  0x47af94
```

⇒ **建表 + 字段语义（名字/起始/计数）全部自证**。名字在运行期内存中保留（NUL 结尾 `char*`）。

### 2.3 两链合一（`res` 唯一）

在**宿主/敌人构造** `FUN_0041EA20` 内：

```asm
0041ed67  8b817c010000      mov  eax,[ecx+0x17c]           ; eax = *(src+0x17C) = manager.res
0041ed6f  8987f8110000      mov  [edi+0x11f8], eax         ; ★ host+0x11F8 = res（唯一写点）
```

且 `res = *(M+0x17C)` 的**赋值点**在资源创建处：

```asm
0041e5fc  c70670164a00      mov  dword [esi], 0x4a1670     ; res 的 vtable
0041e605  89b77c010000      mov  [edi+0x17c], esi          ; ★ *(M+0x17C) = res
```

⇒ manager 侧 `+0x17C` 与 host 侧 `+0x11F8` **指向同一个 `res`**；两链等价。

### 2.4 按名查表（`FUN_0047AE40`，`0x47AE40` / RVA `0x7AE40`）

`host::SetSubByName(host, name)`：`this`=host(`ecx`)，`name` 为栈参。

```asm
0047ae4d  8b80f8110000      mov  eax,[eax+0x11f8]          ; res
0047ae56  8b5808            mov  ebx,[eax+8]               ; ★ count = *(res+8)
0047ae59  83eb01            sub  ebx, 1                    ; hi = count-1
0047ae5e  8b888c000000      mov  ecx,[eax+0x8c]            ; ★ table = *(res+0x8C)
0047ae67  8bc3              mov  eax, ebx                  ; —— 二分查找 ——
0047ae6b  99                cdq
0047ae71  8bf0              mov  esi, eax
0047ae73  d1fe              sar  esi, 1
0047ae75  03f7              add  esi, edi                  ; mid
0047ae77  8b0cf1            mov  ecx,[ecx+esi*8]           ; ★ 表项[+0] = name char*
0047ae80  8a02              mov  al,[edx]                  ; —— strcmp(name, table[mid].name) ——
0047ae82  3a01              cmp  al,[ecx]
```

⇒ **按名查表 = 对 `table[i].+0`（名字串）二分 + strcmp**，与 th15 的 `FUN_0048F340` 一一对应。

---

## 3. Q3 — 函数栈（子调用链）：**存在，与 th15 同形**

### 3.1 结论

1. **有显式子调用栈**，内嵌在 **thread** 对象中（`SP`=字节偏移，`0`=空栈/顶层）：
   - **栈指针**：`SP = *(u32*)(thread + 0x100C)`；
   - **栈存储**：`thread + 0xC + SP`（dword 数组）；
   - **帧长**：`0x10` 字节；**帧内 `+0xC` = 调用者 sub 索引**；
   - **深度上限**：栈区 `0x1000` 字节 ⇒ 物理上限 256 帧，护栏在 `0x478E1C`。
2. **栈顶 ≠ 当前 sub**：当前 sub 是 `*(i32*)(thread+4)`（§1）；每帧保存的是**调用者**上下文。
3. `call`(`0x0B`) 压帧切到被调 sub；`ret`(`0x0A`) 弹帧恢复调用者；**同步** `call` 走此链，**异步** `spawn`(`0x0F`) 另起一个 thread（见 §3.5）。

### 3.2 证据 (a) — 帧内布局（压帧 `FUN_00478730`，由 `call` case 调用）

`call` case @ `0x478D22` → `call 0x478730`：

```asm
00478d22  51                push ecx
00478d23  6a00              push 0
00478d25  57                push edi
00478d26  8bcf              mov  ecx, edi
00478d28  c6460c00          mov  byte [esi+0xc], 0
00478d2c  e8fff9ffff        call 0x478730          ; ★ 压帧/执行 call
00478d31  85c0              test eax, eax
00478d33  0f8559feffff      jne  0x478b92         ; 失败 → 线程结束
00478d39  8bcf              mov  ecx, edi
00478d3b  e8c0230000        call 0x47b100          ; 重解码当前指令
```

压帧序列（`esi`=thread）：

```asm
00478902  8b55ec            mov  edx,[ebp-0x14]    ; edx = SP0（压帧前的 SP）
00478905  8b8e0c100000      mov  ecx,[esi+0x100c]; ecx = SP0
...
00478936  8b860c100000      mov  eax,[esi+0x100c]; eax = SP0
0047893c  894c300c          mov  [eax+esi+0xc], ecx  ; 帧[+0x00] = SP0   （旧 SP / 遍历链）
00478949  898e0c100000      mov  [esi+0x100c], ecx  ; SP += 4
00478953  8b07              mov  eax,[edi]         ; thread+0（时间）
00478955  8944310c          mov  [ecx+esi+0xc], eax  ; 帧[+0x04] = time
00478968  8b4708            mov  eax,[edi+8]       ; thread+8（ip）
0047896b  8944310c          mov  [ecx+esi+0xc], eax  ; 帧[+0x08] = 旧 ip
0047897e  8b4704            mov  eax,[edi+4]       ; thread+4（调用者 sub）
00478981  8944310c          mov  [ecx+esi+0xc], eax  ; 帧[+0x0C] = 调用者 sub  ← 祖先
004789bd  83860c10000004    add  [esi+0x100c], 4    ; SP = SP0 + 0x10
```

> 顶层（`SP0==0`）走 `0x478987` 分支，把 `-1,-1,-1` 写入帧（根哨兵）。**帧长 = 0x10**，与 th15 完全一致。

### 3.3 证据 (b) — `ret`(case `0x0A`) @ `0x478B0A`

```asm
00478b0a  83870c100000fc    add  [edi+0x100c], -4
00478b17  8b8f10100000      mov  ecx,[edi+0x1010]    ; 表达式栈配对指针（见下注）
00478b27  898f0c100000      mov  [edi+0x100c], ecx
00478b2f  7461              je   0x478b92          ; 空 → subIdx=-1（结束）
00478b31  8d41fc            lea  eax,[ecx-4]
00478b36  89870c100000      mov  [edi+0x100c], eax
00478b3c  8b44070c          mov  eax,[edi+eax+0xc]   ; 帧[+0xC] = 调用者 sub
00478b40  894704            mov  [edi+4], eax        ; ★ thread+4 = 调用者 sub
00478b54  894708            mov  [edi+8], eax        ; thread+8 = 旧 ip
00478b68  8907              mov  [edi], eax          ; thread+0 = 旧 time
00478b7b  89870c100000      mov  [edi+0x100c], eax   ; thread+0x100C = 旧 SP
```

> 注：th17 的 `ret` 首段（`0x478B0A`~`0x478B2F`）比 th15 多一次“表达式/临时栈指针（`thread+0x1010`）的换回”，这是本作 `ret` 与 th15 的**唯一结构差异**；其效果与 th15 的 `FUN_0048F690` 清理等价。**弹出次序仍为 `subIdx / ip / time / SP`**，与压帧严格对称。

### 3.4 证据 (c) — 越界护栏 @ `0x478E1C`（case `0x28` 栈局部分配）

```asm
00478e0a  6a00              push 0
00478e0c  8bcf              mov  ecx, edi
00478e0e  e8fd1b0000        call 0x47aa10          ; 计算 size
00478e13  8b970c100000      mov  edx,[edi+0x100c]  ; 当前 SP
00478e19  8d0c02            lea  ecx,[edx+eax]     ; SP + size
00478e1c  81f900100000      cmp  ecx, 0x1000       ; ★ 栈上限 0x1000
00478e22  0f8dac180000      jge  0x47a6d4          ; 溢出 → 本步不执行
```

⇒ 栈区 `thread+0xC .. thread+0x100C`，上限 **`0x1000`**（与 th15 相同）。

### 3.5 遍历规则、深度上限与异步例外

```cpp
// thread = *(u32*)(host + 0x0C);            // 既有链路已取到
// 在“指令边界”遍历（此时表达式/临时栈已清空，SP 即调用栈顶）：
uint32_t sp = *(uint32_t*)(thread + 0x100C);   // 0 = 无 caller
int n = 0;
while (sp >= 0x10 && sp < 0x1000 && n < MAX_DEPTH) {
    int32_t caller = *(int32_t*)(thread + 0x0C + sp - 4);   // 帧[+0x0C] = 调用者 sub
    if (caller < 0 || caller >= count) break;               // -1 = 根哨兵 / 越界保护
    ancestors[n++] = *(char**)(table + (uintptr_t)caller * 8);  // 表项 +0 = 名字
    sp = *(uint32_t*)(thread + 0x0C + sp - 0x10);           // 帧[+0x00]（通常即 sp-0x10）
}
```

- **遍历链**：`sp` 每次以上一帧 `帧[+0x00]` 回退；正常 `帧[+0x00] == sp − 0x10`；本帧调用者 = `帧[+0x0C]`。
- **深度上限**：`0x1000 / 0x10 = 256`（实际更浅）。
- **共用同一 `SP`**：表达式/栈局部（case `0x28/0x29/0x2A/...`）也走 `thread+0x100C`；**只应在指令边界（如 IGI 渲染帧）遍历**。
- **异步 `spawn`（`FUN_0047AD70`，`0x47AD70`）**：新 `malloc 0x11E8` 一个**独立 thread**，挂到父 **host 的线程链 `host+0x11FC`**（`0x47ADCF` `lea ecx,[edi+0x11FC]` + 节点插入），其 `+0x1018 = 父 host`（唯一写点 `0x47ADA6`）。故 **读主线 thread 只会看到主线同步调用链**，异步子脚本不混入。

---

## 4. Q4 — 主线宿主捕获点

### 4.1 对象模型（先厘清）

- **host**（≈ 0x576C 大小的“敌人/脚本宿主”对象，vtable `0x4A1680`）字段：
  `+0x0C`=当前 thread；`+0x11F8`=res；`+0x11FC`=**线程链头**；`+0x18C`=活跃脚本计数；`+0x48`=boss 存在标志。
- **thread**（0x11E8）字段见 §1.1；`thread+0x1018`=host。
- **根线程内联在 host 内**：`host+0xC` 指向 `host+0x10` 处的内联 thread（构造时 `0x41D738 mov [ecx+0xc], eax`，`eax=ecx+0x10`）；此时 `thread+0x1018 = host+0x1028 = host`（写点 `0x41D6D8`）。

### 4.2 进关 / 重开 写“main”宿主

`"main"` 字符串 @ `0x4A1770`，**全库仅 2 处引用**，均在关卡进入/重开路径：

```asm
; 进关（函数 FUN_004318B0）
00431c48  8d4590            lea  eax,[ebp-0x70]      ; 0x54 字节参数块
00431c4c  8b0da0764b00      mov  ecx,[0x4b76a0]      ; ecx = ENEMY_MANAGER = *(u32*)0x4b76a0
00431c52  50                push eax
00431c53  6870174a00        push 0x4a1770            ; "main"
00431c58  e8e3c4feff        call 0x41e140            ; ★ CreateScriptHost("main") → EAX = 宿主
00431c5d  e89e87ffff        call 0x42a400            ; ← 建议 EHOOK 点（EAX = 宿主）

; 重开/重试（函数 FUN_00431DF0）
00431e7d  50                push eax
00431e7e  6870174a00        push 0x4a1770            ; "main"
00431e83  e8b8c2feff        call 0x41e140            ; ★ 同上
00431e88  e87385ffff        call 0x42a400            ; ← 建议 EHOOK 点（EAX = 宿主）
```

`0x41E140` = `ENEMY_MANAGER::CreateScriptHost(name, params)`：`malloc 0x576C`（`0x41e146 push 0x576c`），
构造后 **`0x41E35D mov eax, esi` → `ret 0xc`**，即**返回宿主对象于 EAX**；随后 `0x41E35F inc [edi+0x18C]`。

### 4.3 捕获建议与寄存器

| 项 | 值 |
| --- | --- |
| 进关捕获 | EA `0x431C5D`（RVA `0x31C5D`）— 命中 `EAX` = 主线宿主 |
| 重开捕获 | EA `0x431E88`（RVA `0x31E88`）— 命中 `EAX` = 主线宿主 |
| 备选（更直接） | `0x41E36A`（`0x41E140` 的 `ret`）`EAX` = 宿主，但**对所有敌人生效**，需结合“main”上下文筛选 |
| 宿主→res | `*(u32*)(host+0x11F8)`（写点 `0x41ED6F`） |
| 主线**根** thread | `host + 0x10`（内联） |

> 与 th15 `0x43BBC8/0x43D48E/0x43D6BE` 的差异：th15 的宿主来自 `FUN_426050` 的**返回值**，而 th17 的宿主创建函数（`0x41E140`）**没有独立 wrapper 返回点**，故钩子应放在**创建调用之后的指令**（`0x431C5D`/`0x431E88`，此时 `EAX` 仍为 `0x41E140` 的返回值）。

### 4.4 「`0x47A712` 命中时 `Edi` 是否恒为主线 thread/宿主」→ **否**

- `0x47A712` 位于**主分发 `FUN_00478A70`**，它对**任意被步进的 thread**执行（主线根线程、主线内 spawn 的子线程、敌方脚本线程……）。
- 步进器 `FUN_0047A940`（`0x47A940`）先 `mov [ebx+0xc], ecx`（host+0xC=当前 thread），再对**该 host 的整条线程链 `host+0x11FC`** 逐个 `call 0x478A70`；`FUN_0047A940` 又被多个宿主更新路径（`0x41F981`/`0x41FD97`/`0x42084B`/`0x420BC0`）调用。
- ⇒ **`Edi` = 当前正在被步进的 thread**，**不保证**是主线。
- **实现建议**：捕获主线宿主后，用 `*(u32*)(Edi+0x1018) == mainHost`（根线程亦可 `Edi == mainHost+0x10`）过滤；或直接把 `Edi+4`（sub 索引）经 `res→table` 映射后按白名单判定（现有 `mainLatterIDs` 用法即依赖此）。

---

## 5. Q5 — 命中点、函数入口与调用链

### 5.1 所在函数

- **入口**：`FUN_00478A70` = `0x478A70` / RVA `0x78A70`（`push ebp; mov ebp,esp; and esp,-8; sub esp,0x38; push esi/edi; mov edi,ecx`）。
- 这是**单线程的 ECL VM step**（对 th15 `FUN_0048CA80` 的同代对应）。

### 5.2 调用链

```
宿主更新路径 (0x41F981 / 0x41FD97 / 0x42084B / 0x420BC0)
        └─ FUN_0047A940  (host::StepThreads; 遍历 host+0x11FC 线程链)
                ├─ mov [host+0xc], thread
                └─ call FUN_00478A70 (ecx=thread)   ← 0x47A97F / 0x47A98E
                        └─ … 命中点 0x47A712 ←—— EHOOK 4B
```

### 5.3 4B 覆盖指令语义

```asm
00478ac5  660f6e06          movd  xmm0,[esi]           ; 指令自带时间
00478ac9  0f5bc0            cvtdq2ps xmm0, xmm0
00478acc  0f2fc8            comiss xmm1, xmm0
00478acf  0f82371c0000      jb    0x47a70c           ; 未到执行时间 → 累加时间分支
...
0047a70c  f30f584c2424      addss xmm1,[esp+0x24]      ; time += 帧增量
0047a712  f30f110f          movss dword [edi], xmm1    ; ★ 4B：thread+0（时间）= 新值
0047a716  8d8724100000      lea   eax,[edi+0x1024]     ; 命中后下一条（EHOOK 跳此）
```

- **4B = `f3 0f 11 0f` = `movss [edi],xmm1`**，语义 = **把 thread 时间推进（写回 `thread+0`）**。
- thprac 现有 `EHOOK(0x47A712,4,…)` 命中时或令 `pCtx->Eip = 0x47A716`（跳过写回）以**冻结 MainLatter 的 sub 时间**（`thprac_th17.cpp:778-789`）。
- **可作为稳定触发点**：该分支在“**当前指令的时间尚未到达**”（含 `wait`/定时指令）时稳定进入，是确定性的；但注意它**并非“每帧恰好一次”**——它是“每 step、每个时间等待指令”命中，且对**每个 thread** 都命中。若需“每帧一次”，建议改为钩 `FUN_00478A70` 入口或宿主步进 `0x47A940`，或直接由 IGI 帧回调读取。

> 命中点附近另有 `lea eax,[edi+0x1024]; mov ecx,8; lea edx,[edi+0x11a8]` 的 **8 元循环**，其 `[edi+0x11a4]` 也作 sub 表下标、`[edi+0x11a8]` 作偏移。**这是与 `thread+4` 无关的另一结构**（线程内 8 槽），**切勿与 §1 的当前 sub 索引混淆**。

---

## 6. Q6 — 名字来源：运行期保留

- 建表器把**名字串指针**写入 `table[i].+0`（`0x47AF91`），名字为文件内 NUL 结尾串；运行期内存中**保留**。
- 按名查表 `0x47AE40` 直接对 `table[mid].+0` 与目标串**逐字节 strcmp**（`0x47AE80`~`0x47AE9A`），证明名字在内存中可读。

⇒ **无需离线名字表**，`name = *(char**)(table + subIdx*8)` 可直接读（既有 thprac 链路）。离线兜底（`ECLJump` 偏移）仅在运行时列表不可达时备用。

---

## 7. Q7 — `character` / `shotType`

| 全局 | 值 | 判据 |
| --- | --- | --- |
| `character` | **`0x4B59F4`**（RVA `0xB59F4`） | 与 `shotType` 相邻；`mov ecx,[0x4b59f4]; mov eax,[0x4b59f8]; lea eax,[eax+ecx*2]`（`0x411B10`）用于机体/子机组合 |
| `shotType` | **`0x4B59F8`**（RVA `0xB59F8`） | `mov eax,[0x4b59f8]; dec; neg; sbb; add 0x17`（`0x412CA8`）用于子机类型判定 |

```asm
004115b4  8b0df4594b00      mov  ecx,[0x4b59f4]      ; character（读）
004115ba  a1f8594b00        mov  eax,[0x4b59f8]      ; shotType（读）
```

写入点（绝对寻址 `mov [0x4b59f4],eax`）：`0x44DFDD`、`0x451724`、`0x45612C`、`0x456212`、`0x457451`、`0x45B40A`、`0x45B4A4`（player 初始化处）。
与 thprac IGI（`thprac_th17.cpp:845`：`(*(int32_t*)0x4B59F4)*3 + (*(int32_t*)0x4B59F8)`）一致。

---

## 8. Q8 — 练习跳转补丁（`ECLJump`）不影响 sub 判读

### 8.1 写入内容（`thprac_th17.cpp:1361-1368`）

```cpp
ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << (dest - start) << at_frame;
//      [time]       [op=0x000C size=0x18]  [..]        [0(=+0x10)]   [dest-start(=+0x10?)] [at_frame(=+0x14)]
```

⇒ 写一条 **`goto`（opcode `0x000C`，size `0x18`=24B）** 到运行期 ECL 缓冲的硬编码偏移；与 th15 的 `ECLJump` **完全同形**。

### 8.2 `goto` 语义（case `0x0C` @ `0x478DB0`）

```asm
00478db0  660f6e4614        movd  xmm0,[esi+0x14]     ; at_frame
00478db5  8d4e10            lea   ecx,[esi+0x10]
00478db8  0f5bc0            cvtdq2ps xmm0, xmm0
00478dbb  f30f1107          movss [edi], xmm0         ; thread+0 = at_frame（时间）
00478dbf  8b01              mov   eax,[ecx]          ; dest - start
00478dc1  014708            add   [edi+8], eax       ; thread+8 += (dest-start)   （ip）
00478dc4  0331              add   esi,[ecx]
```

⇒ `goto` **只改 `thread+0`（时间）与 `thread+8`（ip）**，**从不改 `thread+4`（sub 索引）**，也不触碰 sub 表。

### 8.3 结论

✅ 补丁跳转后 `subIdx` 与名字仍正确。`ECLJump` 经 `ECLHelper::SetBaseAddr(res+0xC)` 只写 ECL 数据缓冲字节，不改 SCPT 头/sub 表/索引。

- **唯一瞬态**：`goto` 与随后目标 `call` 之间（同一指令流、极短），`thread+4` 仍为跳转前所在 sub（通常是 `main`）；`call` 执行后即正确。采样在帧间稳定态不会命中。

---

## 9. Q9 — 段落判定口径：**(A) 调用栈祖先法**（决定成败）

### 9.1 结论

✅ **走 (A) 调用栈祖先法。** th17 与 th15 **同代同源、VM 逐指令同形**：段落入口 sub 由 `call`(`0x0B`) 进入并**在整个段落期间驻留栈上**，`ret`(`0x0A`) 时才弹出；沿 §3.5 遍历即可上溯到最近的白名单段落入口。

### 9.2 依据

1. **显式调用栈确实存在**（§3）：`call` 压帧、`ret` 弹帧、帧 `+0xC` 即调用者 sub。这是一条**真实、可遍历、有驻留语义**的调用链。
2. **段入口以 `call` 组织**：主线宿主由 `host::SetSubByName("main")` 起步（§4.2），其后 `@MainFront`/`@MainLatter`/`@MainSubNN` 等子脚本以 `call` 进入；运行到段落内任意子 sub 时，其祖先链必含段落入口 `MainSubNN`。
3. **现成用法佐证**：thprac 现有 `0x47A712` 钩子用 `subID == mainLatterIDs[stage]`（`{0,87,91,86,88,92,0}`）判定 “MainLatter”——`mainLatterIDs` 是**主线段的 sub 索引**，说明段落的稳定标识就是**这些被 `call` 的 sub 自身**（进入即命中当前 sub，无需“顺序经过”）。
4. **对照定案模板**：th15 已由离线 `.ecl` 实测 `main → call MainFront → call MainSubNN`，段落入口驻留栈上，定案为 (A)；th17 的 `call/ret/帧/表` 与其逐条对应。

⇒ **不需要 (B) 的顺序 latch**；(B) 会引入“何时算经过”的歧义，本作无必要。

### 9.3 两个必须写进实现的细节（同 th15）

1. **根哨兵 `-1`**：`main` 首次 `call`（栈空 `SP==0`）压入的帧 `帧[+0x0C]=-1`（`0x478987` 分支写 `-1`）；遍历遇 `caller==-1` 即“到底”，兜底为 **`main`**。
2. **当前 sub 先行判定**：先查 `*(i32*)(thread+4)`（可能已是 `MainSubNN`），再遍历祖先；两步共用同一 `res/table/名字` 链与白名单。

```cpp
host   = mEclHost;                            // 0x431C5D / 0x431E88 捕获（EAX）
thread = (host == 0) ? 0 : *(u32*)(host + 0x0C);
idx    = *(i32*)(thread + 4);                 // 当前 sub
name   = NameOf(host, idx);                   // res=*(host+0x11F8); table=*(res+0x8C)
section = Whitelist(game, stage, name);       // ① 当前 sub 即段入口？
if (!section) {
    for (uint32_t sp = *(u32*)(thread+0x100C);
         sp >= 0x10 && sp < 0x1000;
         sp = *(u32*)(thread + 0x0C + sp - 0x10)) {
        int32_t caller = *(i32*)(thread + 0x0C + sp - 4);
        if (caller < 0) break;                          // 根哨兵 → main
        section = Whitelist(game, stage, NameOf(host, caller));
        if (section) break;                             // ② 最近的白名单祖先
    }
}
if (!section) section = "main";                         // ③ 兜底
```

> 白名单粒度 = `MainSubNN`；容器 `main/MainFront/MainLatter` 命不中即回退。

---

## 10. 真机验证清单（补充 requests §10）

- [ ] **当前 sub**：道中/boss/不同 sub 采样，`*(Edi+4)` 名字与实际一致（用现有 `mainLatterIDs` 的 86–92 交叉印证）。
- [ ] **sub 表链**：`res=[*(u32*)0x4b76a0]+0x17C` 非空、`res+0x8C` 可读、`count=*(res+8)` 合理、索引越界保护生效。
- [ ] **宿主捕获**：在 `0x431C5D`/`0x431E88` 命中，`EAX` 指向的对象 `+0x11F8` 即 `res`、`+0x11FC` 线程链非空。
- [ ] **是否主线**：确认 `0x47A712` 命中时 `Edi` 可能非主线（如 spawn 子线程）；用 `*(Edi+0x1018)==mainHost` 过滤后显示稳定。
- [ ] **函数栈上溯**：段内取一个非白名单子 sub，确认上溯命中最近 `MainSubNN`；根哨兵处按 `main` 兜底。
- [ ] **练习跳章后**：`thread+4` 名字、上溯结果均正确（Q8 复核）。
- [ ] **异步关**：含 `spawn` 的段落，主线 `thread` 的当前 sub 稳定为 `MainSubNN`（不被异步子脚本污染）。
- [ ] 采样时机取**帧间稳定态**（IGI 渲染），避免表达式临时栈占用 `SP` 时误读。
- [ ] **`ECLJump` 落点**：抽验 stage1 portion2/3（`0x7C3C`/`0x7CD8`）等跳转落点是否命中 `call MainSubNN`（无 `.ecl`，本机只能真机验）。

---

## 11. 未定论 / 风险项

1. **Q9 未经脚本级实测**：th17 **无离线 `.ecl`**，且本机不能运行；(A) 结论基于“与 th15 同源同形 + VM 逐指令比对 + 现成 `mainLatterIDs` 用法”，属**静态强推断**，建议真机复核一次上溯行为。
2. **`Edi` 是否主线**：`0x47A712` 对任意 thread 命中（§4.4）。若直接沿用现有钩子而不做宿主过滤，理论上可能读到异步/敌方子脚本的 sub。当前 thprac 用法靠 `subID==mainLatterIDs[stage]` + `bossExists` 间接规避。
3. **`res+0xC` 的双重身份**：建表器把 `res+0xC+idx*4` 当“SCPT 块指针数组”写读，而 `GetMemAddr(0x4b76a0,0x17C,0xC)`（= `res+0xC`）又是 `ECLJump` 的写入基址。二者可能因 ECL 数据在 `res+0xC` 起**连续布局**而自洽（本机无 `.ecl`/无法运行，未实证）。**不影响 Q1–Q9 其余结论**（表的地址链已由 `0x41ED6F`/`0x47AF5D`/`0x47AE40` 独立确证）。
4. **地区/版本变体**：Steam/汉化版是否共用同一 RVA 需按变体实测。

---

## 12. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档。静态反汇编 th17 ECL VM，回答 Q1–Q9：**Q1** `*(Edi+4)`=sub 索引（写点 `0x47AEDB/0x478B40/0x41D6C7/0x47ADB9`）；**Q2** `res=*(host+0x11F8)=*(*(u32*)0x4b76a0+0x17C)`、`table=*(res+0x8C)`、`count=*(res+8)`、项 8B（`+0`名/`+4`始），建表器 `0x47AEF0`、查表 `0x47AE40`；**Q3** 显式调用栈（`SP=thread+0x100C`、底 `thread+0xC`、帧 `0x10`、帧 `+0xC`=调用者、上限 `0x1000`、护栏 `0x478E1C`；`call 0x478D22`/`ret 0x478B0A`；spawn `0x47AD70` 另起线程）；**Q4** 主线宿主 spawn `0x431C58/0x431E83`（`call 0x41E140`，EAX=宿主），命中点 `Edi` 非必然主线；**Q5** 入口 `FUN_00478A70`、4B=`movss [edi],xmm1`、调用链 `0x47A940→0x478A70`；**Q6** 运行期保留名字；**Q7** `character=0x4B59F4`、`shotType=0x4B59F8`；**Q8** `ECLJump` 只写 `goto(0x0C)`，不改 sub；**Q9** 定案 **(A) 调用栈祖先法**。证据：`capstone` 反汇编 + th15 Ghidra 全量反编译对照。未真机三态。 |

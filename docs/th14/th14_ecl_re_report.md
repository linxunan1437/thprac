# TH14 ECL 段落监控 — 逆向结论（Q1 当前 sub 语义 / Q2 sub 表 / Q3 函数栈 / Q4 宿主捕获 / Q5 命中点 / Q6 名字 / Q8 练习跳转 / Q9 段落判定口径）

> 对应需求：`docs/th14/th14_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`。
> 二进制：`../exes/th14.exe`，**897536 B**，PE `TimeDateStamp = 1376634201`，`ImageBase = 0x400000`，
> **`RELOCS_STRIPPED`**（`RVA = VA − 0x400000`）。SHA-256 = `f08e885e28d2e0fc8c579e88034dd7de8f2a648d8f08ce781cf08eadc9ffe353`。
> 方法：`python3 (3.9.6)` + `pefile 2024.8.26` + `capstone 5.0.7` **只读反汇编**（无 IDA/Ghidra/radare2）；
> 借助 `RELOCS_STRIPPED` 以「4 字节小端 VA 立即数」做字节搜索定位 xref；并与同代模板 th15
> （`docs/th15/th15_ecl_re_report.md` + 本机 `../th15decode/th15.exe_decompiled.c`）逐函数对照。
> ECL / decl 素材：`../exes/ecl/th14/`（原始 `.ecl` 24 份）、`../exes/ecl_dump/th14/`（`thecl -d` 反编译 `.decl` 24 份），清单与来源见 `docs/logger/decompile_materials.md`。
> 本报告「脚本结构」结论均由二进制代码 + 已有 thprac 用法反推（**无脚本级实证**）；`.decl` 素材已在机，可离线补脚本级证据。
> 状态：**静态定案（🟡 已定案、未真机）**。凡未经真机的结论均标注「静态、待真机」。
> 地址口径：正文以 **VA** 为主，并在关键处标注 **RVA = VA − 0x400000**。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前 sub 语义** | ✅ **(b) `[Edi+4]` = 当前指令指针（绝对 VA）**，**不是** sub 索引 | `Edi` 是「脚本节点/子线程」；`+0`=时间、`+4`=ip、`+8`=调用栈底、`+0x1008`=SP。thprac 现有注释（“当前指令指针”）**正确**；编排者怀疑的 (a) 被证伪。 |
| **Q2 sub 表** | ✅ 确认（th15 同构） | `res=[[0x4db544]+0xcc]`；`res+8`=sub 总数；`res+0x8c`=sub 表；表项 8B：`+0`=名字 `char*`、`+4`=**sub 数据绝对地址**（首指令 = `+4`+0x10）。 |
| **Q3 函数栈** | ✅ **存在可遍历的子调用栈**，但**祖先字段是 ip 而非 sub 索引** | `SP=*(u32*)(node+0x1008)`，栈底 `node+8`，帧长 `0x10`；`call`(`0x0B`) 压 `{旧SP,time,ip}`、`ret`(`0x0A`) 弹回。**帧内没有 caller sub 索引**（th14 全程不保存 sub 索引）⇒ 祖先需用「返回 ip → 名字」映射。 |
| **Q4 主线宿主捕获点** | 🟡 候选已定位，待真机 | 命中点处 `host = *(u32*)(node+0x1014)`；「进关建主线宿主」写入点 = **`0x45883e`（`mov [esi+0x2dc],eax`，`EAX`=宿主）**，其宿主由 **`0x484940`** 创建（`EAX`=新宿主）。 |
| **Q5 解释器命中点** | ✅ 对 | `0x483c4f` 位于 **`0x482380`（每节点 ECL 步进器）**；命中 `Edi`=节点；覆盖 `movss [edi],xmm0`（时间+dt）；由宿主迭代器 `0x483e70` 每帧调用（对**每个**节点都触发，不保证是主线）。 |
| **Q6 名字来源** | ✅ 运行期保留 | 表项 `+0` 直指名字 blob；按名查表 `0x4848b0`（二分）与按索引显示（`0x41e1c9`/`0x458b8f`）都在运行期读 `char*`。 |
| **Q7 机体/子机类型** | ✅（已知，从略） | `CHARA_ADDR=0x4f5828`、`SUBSHOT_ADDR=0x4f582c`（`thprac_th14.cpp:660` 已用）。 |
| **Q8 练习跳转补丁** | ✅ **不影响判读**（同 th15） | `ECLJump` 只写 `goto`(`opcode 0x0C`)；`goto` 只改 `node+0`(time) 与 `node+4`(ip)，不改 sub 归属。th14 **根本没有 sub 索引**，更不可能被改。 |
| **Q9 段落判定口径** | ✅ **路过式 checkpoint（latch）** | CSV 即白名单：当前 sub（th14 需用「按地址排序的 sub 表」把 `node+4`=ip 映射为名字）∈ 表 → 置当前段落；**不在 → no-op**（不回退 `main`）。**每 checkpoint 只登记 1 个入口函数**（含 async 落点）。stage 4 分支在运行期天然正确。 |

> 关键地址（VA / RVA）：节点步进器 `0x482380`/`0x82380`；宿主迭代器 `0x483e70`/`0x83e70`；
> `call` 压帧 `0x482080`/`0x82080`（opcode `0x0B`=`0x482552`/`0x82552`）；
> `ret`（opcode `0x0A`）`0x4823ED`/`0x823ED`；`goto`（opcode `0x0C`）`0x4825D7`/`0x825D7`；
> sub 名→地址解析 `0x4848B0`/`0x848B0`；ECL 加载器 `0x4846A0`/`0x846A0`；新建宿主+起 sub `0x484940`/`0x84940`。
> 命中点 `0x483c4f`/`0x83c4f` 的原始字节 = `f3 0f 11 07`（`movss [edi],xmm0`，4B）。

---

## 1. 统一数据模型（th14 版指针链）

命中点 `0x483c4f` 处 `pCtx->Edi` = **脚本节点（sub-thread）**，其字段与 th15 的 `thread` **不同构**（th14 无 sub 索引）：

```cpp
// —— 命中点 0x483c4f，pCtx->Edi = node（脚本节点/子线程）——
node   = pCtx->Edi;                    // 0x482380 的 this
time   = *(float*)(node + 0x00);       // 当前 ECL 时间（被命中点指令冻结）
ip     = *(uint32_t*)(node + 0x04);    // ★当前指令指针（绝对 VA，指向 ECL 缓冲内）
stack  = node + 0x08;                  // 子调用栈基址
sp     = *(uint32_t*)(node + 0x1008);  // 子调用栈指针（字节偏移，0 = 空）
host   = *(uint32_t*)(node + 0x1014);  // 宿主（ECL 宿主对象）
mask   = *(uint8_t*)(node + 0x101C);   // 难度/等级掩码（指令可见性过滤）

// —— 宿主 / 资源 ——
res    = *(uint32_t*)(host + 0x11CC);      // ECL 资源对象（亦 = *(u32*)(*(u32*)0x4db544 + 0xCC)）
count  = *(int32_t*)(res  + 0x08);         // sub 总数（越界保护）
table  = *(uint32_t*)(res + 0x8C);         // sub 表；每项 8 字节
name_i = *(const char**)(table + i*8);     // 表项 +0 = 名字 char*
addr_i = *(uint32_t*)(table + i*8 + 0x04); // 表项 +4 = sub 数据绝对地址；首指令 = addr_i + 0x10
```

> 宿主 ↔ 段落场景：`host = *(u32*)(scene + 0x2DC)`、`res = *(u32*)(scene + 0x2D8)`（`scene`=关卡 ECL 场景对象，见 §4）。
> 主线节点：宿主复位（`0x4211C0`）置 `host+4 = host+8`，即**主线节点内嵌在 `host+8`**；故 `host+8` 的 `+0x1014` 回指 `host` 自身（`= host+0x101C`）。

---

## 2. Q1 — `[Edi+4]` 是 sub 索引还是指令指针？（**首要待答**，存在矛盾）

### 2.1 结论

✅ **(b) `[Edi+4]` = 当前指令指针（绝对 VA），不是 sub 索引。**

- `Edi` 指向的对象是 **ECL 脚本节点（子线程）**，不是 th15 意义上的 `thread`。
- th14 现有注释（`thprac_th14.cpp:597` “当前指令指针”）**正确**；编排者「(a) `+4` 是 sub 索引、现有注释有误」的假设**被证伪**。
- th14 与 th15 的**关键差异**：th15 用 `thread+4`=sub 索引 + `thread+8`=ip 偏移（双字段）；th14 用**单个绝对 ip 指针**（`node+4`），**全程不保存 sub 索引**。

### 2.2 命中点所在对象（`node`）字段语义

| 偏移 | 类型 | 语义 | 证据（VA） |
| --- | --- | --- | --- |
| `+0x00` | float | 当前 ECL 时间 | `0x483c2e` `movss xmm0,[edi]`；`0x483c4f` `movss [edi],xmm0` |
| `+0x04` | u32 | **当前指令指针（绝对 VA）** | 见 §2.3 |
| `+0x08` | — | 子调用栈底（`call` 压帧 / `ret` 弹帧的数组基址） | `0x4823ed` `lea ecx,[edi+8]`；`0x4823f5` `cmp [edi+0x1008],0` |
| `+0x1008` | i32 | 子调用栈指针 SP（字节，`0`=空） | `0x483c8f` `mov esi,[edi+0x100c]`… 及 `0x4820c2`/`0x4820a3` |
| `+0x100C` | i32 | 第二栈指针（表达式/局部，与 `+0x1008` 配对） | `0x484b50`（`[ecx+0x1004]`⇄`[ecx+0x1000]` 交换） |
| `+0x1010` | u32 | 节点 id（`spawn` 参数） | `0x4844ff`/`0x484505`；`0x484580` 按它查节点 |
| `+0x1014` | u32 | 宿主 `host` 指针 | `0x48230d` `mov esi,[edi+0x1014]`；`0x422fe`→`0x482325` 等 |
| `+0x101C` | u8 | 难度/等级掩码（指令可见性过滤） | `0x4823c6` `test [edi+0x101c],al` |
| `+0x1018` | u32 | 可被 `opcode 0x14` 写的通用值 | `0x482547` `mov [esi+0x1018],eax` |

### 2.3 `[Edi+4]` 的**写入点**（决定性证据）

**(a) 推进：`ip += 指令长度`（4 处主写入之一）** — `0x483C0C`（opcode `0x00/0x16/0x1E/0x1F` 的“空步进”）：

```asm
00483c0c  8b4704            mov  eax,[edi+4]              ; eax = ip
00483c0f  8a400c            mov  al,[eax+0xc]             ; ip+0xC 字节（栈释放量）
00483c12  84c0              test al,al
00483c14  7409              je   0x483c1f
00483c16  0fb6c0            movzx eax,al
00483c19  298708100000      sub  [edi+0x1008],eax         ; SP -= al
00483c1f  8b4704            mov  eax,[edi+4]
00483c22  0fb74806          movzx ecx,word ptr [eax+6]    ; ecx = word[ip+6] = 指令长度
00483c26  03c8              add  ecx,eax
00483c28  894f04            mov  [edi+4],ecx              ; ★ip += 指令长度
```

**(b) 进入时被解引用为“指令”** — 函数 `0x482380` 入口：

```asm
00482399  8b4704            mov  eax,[edi+4]              ; eax = ip
004823a0  85c0              test eax,eax
004823a2  0f84fd180000      je   0x483ca5                  ; ip==0 → 结束
004823a8  660f6e08          movd xmm1,[eax]               ; ★解引用 ip（指令 +0 = 帧时间 int）
004823ac  f30f1007          movss xmm0,[edi]              ; node+0 = 时间
004823b3  0f2fc1            comiss xmm0,xmm1
004823b6  0f8286180000      jb   0x483c42
004823c0  8b4f04            mov  ecx,[edi+4]
004823c3  8a410a            mov  al,[ecx+0xa]             ; 指令 +0xA = 可见性掩码
004823c6  84871c100000      test [edi+0x101c],al
004823cc  0f844d180000      je   0x483c1f
004823d2  0fbf4104          movsx eax,word ptr [ecx+4]    ; ★指令 +4 = opcode
004823d9  0f8713180000      ja   0x483bf2                  ; 越界 → default
004823df  0fb6800c3e4800    movzx eax,byte ptr [eax+0x483e0c]
004823e6  ff2485003d4800    jmp  dword ptr [eax*4 + 0x483d00]; 双表分发（见 §5）
```

**(c) 其余写入点**（均为“改写 ip”，可与 th15 逐一对上）：

| 写入点 | 语义 | 值来源 |
| --- | --- | --- |
| `0x483C28` | 空步进 | `ip += word[ip+6]`（指令长度） |
| `0x482412` | `ret`(`0x0A`) | 从栈弹出**返回 ip** |
| `0x482325` | `call`(`0x0B`) | 被调 sub 起始地址（`0x4848B0` 名→地址） |
| `0x4825EB` | `goto`(`0x0C`) | `ip += dword[ip+0x10]` |
| `0x483C9E` | `0x01`（终止） | `ip = 0` |

### 2.4 与「sub 索引」的辨析（为何不是 (a)）

- 全流程未见任何「用 `node+4` 当索引进 `table[…*8]`」的代码；相反，凡是需要名字的地方，**索引进表的写法都出现在别的对象字段 `+0x1EC` 上**（`0x41E194`：`mov ecx,[edi+0x1ec]; push [table+ecx*8]`；`0x458B89`：`mov ecx,[esi+0x1ec]`）。
- `+0x1EC` 并非节点字段，而是**某些“场景/实体”对象的“脚本入口/状态索引”**（构造函数 `0x41DB10`、复位 `0x4581D0` 都把它清零；`0x45882A` 用它 `push table[[scene+0x1ec]*8]` 去**按名启动**关卡脚本）。**它是“入口 sub 索引”，未被证实会随执行推进而更新**（静态下只见到初始化写入 `0x458216`、`0x457294`）。⇒ **不能**据此判定“当前 sub”。
- 结论：th14 的「当前执行到哪个 sub」**只能由 ip 反推**（见 §9）。

> 附：thprac 现有区间比较（`thprac_th14.cpp:595-601`）用 `table[65].+4`、`table[66].+4` 与 `[Edi+4]` 比较——
> 由于表项 `+4` 是 sub **数据地址**，而节点 ip 是 **`+4`+0x10**（跳过 0x10 头），二者**相差 0x10**；作者按名字序取 `[65]/[66]`
> 也未必地址相邻，故注释“not strictly contiguous”成立。**这不是把 `[Edi+4]` 当“指令指针”的错，而是表项语义/排序带来的近似**。

---

## 3. Q2 — sub 表：索引 → 名字 / 起始（确认）

### 3.1 结论

✅ 与 th15 同构，且**名字字段确认为 `char*`**：

```
res   = *(u32*)(*(u32*)ENEMY_MANAGER_PTR + 0xCC)      // = *(u32*)(host + 0x11CC)
count = *(i32*)(res + 0x08)
table = *(u32*)(res + 0x8C)
// 表项 8B： +0 = 名字 char* ； +4 = sub 数据绝对地址（首指令 = +4 + 0x10）
```

### 3.2 证据

**(a) ECL 加载器 `0x4846A0`（= th15 `FUN_0048f130` 的对应物）**——校验 “SCPT”、累计计数、生成表：

```asm
004846c1  813953435054        cmp  dword ptr [ecx],0x54504353   ; "SCPT"
004846db  6683790401          cmp  word ptr [ecx+4],1
004846e2  0fb74106            movzx eax,word ptr [ecx+6]
004846e6  8d5124              lea  edx,[ecx+0x24]
004846e9  03d0                add  edx,eax
004846eb  0fb74110            movzx eax,word ptr [ecx+0x10]     ; 本文件 sub 数（word）
004846ef  014708              add  dword ptr [edi+8],eax        ; ★res+8 += 子数（= 总数）
004846f9  8b9f8c000000        mov  ebx,[edi+0x8c]               ; 旧表
00484702  50                  push eax
00484709  e8320e0000          call 0x485540                     ; 重分配 count*8
0048470e  89878c000000        mov  [edi+0x8c],eax               ; ★res+0x8C = 新表
...（逐 sub 填表）...
00484730  8b4704              mov  eax,[edi+4]                  ; 文件槽号
00484733  8b4c870c            mov  ecx,[edi+eax*4+0xc]          ; 文件缓冲基址
0048473a  0308                add  ecx,[eax]                    ; ★filebuf + 偏移 = 绝对地址
0048473c  8b878c000000        mov  eax,[edi+0x8c]
00484742  894cd004            mov  [eax+edx*8+4],ecx            ; ★表项[+4] = 绝对数据地址
00484746  8b878c000000        mov  eax,[edi+0x8c]
0048474c  8bcb                mov  ecx,ebx
0048474e  891cd0              mov  [eax+edx*8],ebx              ; ★表项[+0] = 名字 char*
```

**(b) 运行期“按索引取名字”**（两处，证明 `+0` 是 `char*`）：

```asm
; —— 0x41E194 区域：name = table[idx].+0 ；res = [0x4db544]+0xcc
0041e1a0  a144b54d00          mov  eax,[0x4db544]               ; ENEMY_MANAGER_PTR
0041e1ad  8b80cc000000        mov  eax,[eax+0xcc]               ; res
0041e1bb  8b808c000000        mov  eax,[eax+0x8c]               ; table
0041e192  8b8fec010000        mov  ecx,[edi+0x1ec]              ; idx
0041e1c9  ff34c8              push dword ptr [eax+ecx*8]        ; ★push table[idx].+0（名字）
0041e1d4  e8173d0000          call 0x421ef0                     ; 格式化
```

```asm
; —— 0x458B89 区域：res = [scene+0x2d8]，idx = [scene+0x1ec]
00458b72  8b86d8020000        mov  eax,[esi+0x2d8]              ; res（scene 持有）
00458b83  8b808c000000        mov  eax,[eax+0x8c]               ; table
00458b89  8b8eec010000        mov  ecx,[esi+0x1ec]              ; idx
00458b8f  ff34c8              push dword ptr [eax+ecx*8]        ; ★名字
```

**(c) 按名查表（“名→地址”，= th15 `FUN_0048f340` 的对应物）** — `0x4848B0`（二分查找，**返回 `table[idx].+4 + 0x10`**）：

```asm
004848b7  8b7908              mov  edi,[ecx+8]                  ; count
004848ba  33db                xor  ebx,ebx
004848bc  4f                  dec  edi
004848bf  8b898c000000        mov  ecx,[ecx+0x8c]               ; table
004848d8  8b0cf1              mov  ecx,[ecx+esi*8]              ; table[mid].name
004848e0  8a02                mov  al,[edx]                     ; 与目标名逐字节比较
...
00484925  8b45fc              mov  eax,[ebp-4]
00484929  8b44f004            mov  eax,[eax+esi*8+4]            ; ★table[mid].+4
0048492e  83c010              add  eax,0x10                      ; ★+0x10 = 首指令
00484935  c20400              ret  4
```
> 说明：该二分要求**表按名字有序** ⇒ **表项 `+4`（地址）不随索引单调**。这正是「按名字序做区间比较会错」的根因（见 §2.4）。

---

## 4. Q3 — 函数栈（子调用链）

### 4.1 结论

1. **存在显式子调用栈**，内嵌在**节点对象**里，随解释器自动维护：
   - **栈指针**：`SP = *(u32*)(node + 0x1008)`（int，字节偏移，**`0` = 空栈/顶层**）。
   - **栈存储**：`node + 0x08 + SP`（dword 数组）。
   - **帧长**：`0x10` 字节（与 th15 相同）。
2. **帧内没有 caller sub 索引**（**与 th15 的关键差异**）：th14 帧保存的是 `{旧SP, time, 返回 ip}`；th15 帧保存 `{旧SP, time, ip, caller subIdx}`。
   ⇒ th15 可直接读帧内 `+0xC` 得祖先 sub；**th14 只能读帧内“返回 ip”，再映射为 sub**。
3. 同步 `call`(`0x0B`) 压帧并切换 `node+4`；`ret`(`0x0A`) 弹帧；**异步 `spawn`(`0x0F`) 另起节点**（`0x4844D0`，挂到 `host+0x11D0` 链表）。

### 4.2 帧布局与遍历

`ret`（opcode `0x0A`）@ `0x4823ED`（从栈顶依次弹回 `ip / time / SP`）：

```asm
004823ed  8d4f08              lea  ecx,[edi+8]              ; ecx = node+8（栈基址）
004823f0  e85b270000          call 0x484b50                 ; 弹出第 4 槽（见下）
004823f5  83bf0810000000      cmp  dword ptr [edi+0x1008],0 ; SP==0 ?
004823fc  0f849c180000        je   0x483c9e                 ;   空 → 终止（ip=0, -1）
00482402  838100100000fc      add  dword ptr [ecx+0x1000],-4; SP -= 4
00482409  8b8100100000        mov  eax,[ecx+0x1000]
0048240f  8b0408              mov  eax,[eax+ecx]            ; pop
00482412  894704              mov  [edi+4],eax              ; ★ip = 返回 ip
00482415  838100100000fc      add  dword ptr [ecx+0x1000],-4; SP -= 4
0048241c  8b8100100000        mov  eax,[ecx+0x1000]
00482422  8b0408              mov  eax,[eax+ecx]            ; pop
00482425  8907                mov  [edi],eax                ; time = 弹出值
00482427  838100100000fc      add  dword ptr [ecx+0x1000],-4; SP -= 4
0048242e  837f0400            cmp  dword ptr [edi+4],0
00482432  8b8100100000        mov  eax,[ecx+0x1000]
00482438  8b0408              mov  eax,[eax+ecx]            ; pop
0048243b  898708100000        mov  [edi+0x1008],eax         ; ★SP = 旧 SP
```

`call`（opcode `0x0B`）@ `0x482552` → 压帧体 `0x482080`（**压帧 + 参数求值 + 名→地址**）：

```asm
00482552  51                  push ecx                      ; 参数：ip
00482553  6a00                push 0                        ; 参数：0（起始参数号）
00482555  c6410c00            mov  byte ptr [ecx+0xc],0     ; [ip+0xC]=0（与 th15 一致）
00482559  57                  push edi                      ; 参数：node
0048255a  8bcf                mov  ecx,edi
0048255c  e81ffbffff          call 0x482080
...
; —— 0x482080 压帧部分（node=esi，旧SP=ebx=[ebp-0x10]） ——
00482278  8b8e08100000        mov  ecx,[esi+0x1008]         ; 当前 SP
0048227e  85db                test ebx,ebx
00482280  741d                je   0x48229f                 ; 根：SP0==0 特判
004822a9  8b8608100000        mov  eax,[esi+0x1008]
004822af  894c3008            mov  [eax+esi+8],ecx          ; ★帧[+0] = 旧 SP
004822b3  83860810000004      add  dword ptr [esi+0x1008],4
004822c4  8b07                mov  eax,[edi]
004822c6  89440e08            mov  [esi+ecx+8],eax          ; ★帧[+4] = time
004822ca  83860810000004      add  dword ptr [esi+0x1008],4
004822d7  8b4704              mov  eax,[edi+4]
004822da  89440e08            mov  [esi+ecx+8],eax          ; ★帧[+8] = 返回 ip
...
; —— 名 → 地址，并改写 node+4（= 被调 sub 起始） ——
0048230a  8b4704              mov  eax,[edi+4]
0048230d  8bb714100000        mov  esi,[edi+0x1014]         ; host
00482313  83c014              add  eax,0x14                  ; ★ip+0x14 = 被调名（名字操作数）
00482316  8b8ecc110000        mov  ecx,[esi+0x11cc]         ; res
0048231c  50                  push eax
0048231d  e88e250000          call 0x4848b0                 ; 名→地址
00482325  894104              mov  [ecx+4],eax              ; ★node+4 = 被调起始地址
00482328  8b4604              mov  eax,[esi+4]
0048232b  c70000000000        mov  dword ptr [eax],0        ; node+0 = 0（时间清零）
```

> **帧布局**（`base = node + 8 + SP0`）：
> | 帧内 | 类型 | 语义 |
> | --- | --- | --- |
> | `+0x00` | u32 | 旧 SP（退栈恢复） |
> | `+0x04` | float | 调用时 `node+0`（时间） |
> | `+0x08` | u32 | **返回 ip**（th15 此处存 `ip 偏移`；th14 存绝对 ip） |
> | `+0x0C` | u32 | **参数区 / 保留**（th15 此处是 caller `subIdx`，th14 **无 sub 索引**） |

### 4.3 遍历规则与深度上限

```cpp
// node = 命中点 pCtx->Edi（或 host+8 主线节点）
uint32_t sp = *(uint32_t*)(node + 0x1008);   // 0 = 无 caller
int n = 0;
while (sp >= 0x10 && sp < 0x1000 && n < MAX_DEPTH) {
    uint32_t ret_ip = *(uint32_t*)(node + 0x08 + sp - 0x08);  // 帧[+8] = 返回 ip
    // ★ 用 ip→sub 映射（§9）把 ret_ip 映射为祖先 sub 名
    ancestors[n++] = NameOfIp(res, ret_ip);
    sp = *(uint32_t*)(node + 0x08 + sp - 0x10);               // 帧[+0] = 旧 SP
}
```

- **遍历链**：`sp` 每次以帧 `+0`（= 旧 SP）回退，逐帧读帧 `+8`（返回 ip）。
- **深度上限**：栈区 `0x1000` 字节 ÷ 帧长 `0x10` ⇒ **物理上限 256 帧**（与 th15 相同；实际远浅）。
- **越界护栏**：`0x484B00`（栈局部分配）用 `cmp edx,0x1000 ; jl … ; or eax,-1`，即 **栈区上限 `0x1000`**。
- **共用 `SP`**：表达式/局部栈（`0x1008`/`0x100C` 配对）也走同一区域，**只应在指令边界（帧间稳定态，如 IGI 渲染时）遍历**。

### 4.4 异步 `spawn` 的例外

`spawn`(`0x0F`) @ `0x48244C` → `0x4844D0`：`new(0x11C4)` 建新节点，`node+0x1014 = 父 host`，并挂到 `host+0x11D0` 链表（`lea ecx,[edi+0x11d0]`，`0x484529`）。新节点**自带空栈**。⇒ 读主线宿主链（`host+8` 或迭代器当前节点）只会看到该节点的同步调用链；异步子脚本是**另一个节点**，不会混入。若 `0x483c4f` 命中的 `Edi` 不是主线节点，需以 `node+0x1014` 核对宿主、或直接锚定 `host+8`。

---

## 5. Q4 — 主线宿主捕获点

### 5.1 结论（🟡 候选，待真机）

- **命中点处即可拿到宿主**：`host = *(u32*)(pCtx->Edi + 0x1014)`，`res = *(u32*)(host + 0x11CC)`。**无需另设全局**即可完成读到 sub（§1 链）。
- **“进关建主线宿主”写入点（推荐 hook）**：
  - **`0x45883E`**：`mov dword ptr [esi+0x2DC],eax` —— **`EAX` = 新建的主线宿主**，`esi` = 关卡 ECL 场景对象（同函数还会 `mov [esi+0x30],4` 置状态）。见 `0x4587F9`~`0x458844`：
    ```asm
    00458824  8b8ed8020000        mov  ecx,[esi+0x2d8]          ; res
    0045882a  8b96ec010000        mov  edx,[esi+0x1ec]          ; 入口 sub 索引
    00458830  8b818c000000        mov  eax,[ecx+0x8c]           ; table
    00458836  ff34d0              push dword ptr [eax+edx*8]    ; 名字 = table[entry].+0
    00458839  e802c10200          call 0x484940                 ; ★新建宿主 + 起该 sub
    0045883e  8986dc020000        mov  [esi+0x2dc],eax          ; ★host = EAX
    00458844  c7463004000000      mov  dword ptr [esi+0x30],4
    ```
  - **`0x484940`**：宿主工厂（`__thiscall`，`ecx`=res，返回宿主 `EAX`）：
    ```asm
    00484945  68e0110000          push 0x11e0
    0048494c  e85e0b0000          call 0x4854af                 ; operator new(0x11e0)
    0048495a  e8f1c7f9ff          call 0x421150                 ; 宿主构造
    0048496a  89becc110000        mov  [esi+0x11cc],edi         ; host+0x11cc = res
    00484970  e83bffffff          call 0x4848b0                 ; 名→地址
    00484979  894104              mov  [ecx+4],eax              ; node+4 = 起始地址
    00484985  8bc6                mov  eax,esi                  ; ★返回宿主
    ```
- 另一处宿主初始化：`0x422DFC`（`mov [edi+0x11cc],ecx`，`ecx=[[…]+0xcc]`）——`edi` 即宿主，用于敌人/重玩路径。

### 5.2 命中点 `Edi` 是否恒为主线宿主？

❌ **不恒等**。`0x483c4f` 位于**每节点步进器**，由宿主迭代器 `0x483E70` 对**链上每个节点**调用：主线节点（`host+8`）与 `spawn` 出的异步节点都会命中。实现上应：
1. 用 `node+0x1014` 取宿主；或
2. 直接取主线节点 `host+8`（`*(u32*)(host+4)` 即当前节点，复位后指向 `host+8`）。
> 相关：`0x483E70` 的调用者 `0x423C42 / 0x42485D / 0x424C7C / 0x4586E5`（后者为关卡场景更新，见 `0x4586DF` `mov ecx,[esi+0x2dc]; call 0x483e70`）。

---

## 6. Q5 — 解释器命中点与寄存器语义

- **所在函数入口**：**`0x482380`**（`push ebp; mov ebp,esp; and esp,-8; sub esp,0xC0; push esi; push edi; mov edi,ecx`）——**每节点 ECL 步进器**（th15 `FUN_0048ca80` 的同位体，但结构不同：以**绝对 ip 指针**为核心，而非 `thread+4` 索引）。
  - 参数：`ecx`=节点；`xmm1`=本帧 `dt`（存于 `[esp+0x84]`，命中点用它做 `time += dt`）。
- **命中点 `0x483c4f`**：`f3 0f 11 07` = `movss dword ptr [edi],xmm0`（`0x483c42` 先 `movss xmm0,[edi]`，`0x483c46` `addss xmm0,[esp+0x84]`，故此处 = **`time += dt`**）。EHOOK 覆盖 4B（`0x483c4f..0x483c52`），跳过即“冻结时间” ⇒ `pCtx->Eip = 0x483c53`（thprac 现有做法正确）。
- **命中寄存器**：`Edi` = 节点对象（执行对象）。
- **调度方式**：双表分发（`opcode = word[ip+4]`；`byte[0x483E0C + opcode]` → `jmp [0x483D00 + idx*4]`）。
- **能否作每帧稳定触发点**：✅ 触发稳定（宿主迭代器每帧跑链），但**对每个节点都会触发**（非“每帧一次”）；作为“读当前 sub”的采样点可行，但需自行判定是否是主线节点（§5.2）。

**opcode 对照（与 th15 一致）**：`0x00/0x16/0x1E/0x1F`=空步进(`0x483c0c`)；`0x01`=终止(`0x483c9e`)；`0x0A`=ret；`0x0B`=call；`0x0C`=goto；`0x0F`=spawn；`0x10`=spawn+值；`0x14`=设节点值（`+0x1018`）。

---

## 7. Q6 — 名字来源

✅ **运行期保留 sub 名**。表项 `+0` 直指名字 blob（`char*`），三处代码均在**运行期**按 `char*` 使用：
- 加载器 `0x48474E`（`mov [eax+edx*8],ebx`，`ebx`=名字串指针）；
- 按索引显示/格式化 `0x41E1C9`、`0x458B8F`；
- 按名二分查表 `0x4848B0`。

⇒ **无需离线建表**（与 th15 相同）。

---

## 8. Q7 — 机体 / 子机类型（已知，从略）

`CHARA_ADDR = 0x4f5828`、`SUBSHOT_ADDR = 0x4f582c`；`thprac_th14.cpp:660` 已用 `cur_player_type = *(i32*)0x4f5828 * 2 + *(i32*)0x4f582c`。本轮无新增。

---

## 9. Q8 — 练习跳转补丁（`ECLJump`）对读取的影响

### 9.1 结论

✅ **不影响判读**（与 th15 同结论）。

### 9.2 依据

1. **补丁写入内容**：`thprac_th14.cpp:1658`
   ```cpp
   ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
   ```
   展开后与 th15 完全一致：`opcode = 0x000C`（**`goto`**）、`size = 0x0018`、`dword4 = dest-start`、`dword5 = at_frame`。只把一条 `goto` 覆盖到硬编码文件偏移处。
2. **`goto` 语义**（opcode `0x0C`）@ `0x4825D7`：
   ```asm
   004825d7  8b4f04              mov  ecx,[edi+4]              ; ip
   004825da  660f6e4114          movd xmm0,[ecx+0x14]          ; operand5 = at_frame
   004825df  0f5bc0              cvtdq2ps xmm0,xmm0
   004825e2  f30f1107            movss [edi],xmm0              ; node+0 = at_frame（绝对赋值）
   004825e6  8b4110              mov  eax,[ecx+0x10]           ; operand4 = dest-start
   004825e9  03c1                add  eax,ecx
   004825eb  894704              mov  [edi+4],eax              ; node+4 += (dest-start)
   ```
   **只改 `node+0`（时间）与 `node+4`（ip）；th14 根本没有 sub 索引字段，谈不上被改。**
3. **sub 结构未动**：`ECLJump` 经 `ECLHelper::SetBaseAddr/SetFile`（`thprac_th14.cpp:2681`，`GetMemAddr(0x4db544,0xcc,0xC)`）定位运行期 ECL 缓冲并按文件偏移写字节；不触碰 SCPT 头、偏移数组、名字 blob、sub 表。
4. **瞬态注意**：`goto` 刚发生、目标 `call` 尚未执行时，节点 ip 仍在外层容器（通常是 `main`）；`call` 执行后 ip 才指向目标段落入口。采样落在帧间稳定态即可避开。

> 说明：th14 用 **ip** 而非 sub 索引判读，故 `ECLJump` 后只要重新做 ip→sub 映射即可，结论更强。

---

## 10. Q9 — 「当前段落」判定口径：**路过式 checkpoint（latch）**

### 10.1 结论

✅ **走「路过式 checkpoint（latch）」**：读当前 sub（th14 需先补「ip → sub」映射层，见 §10.3）；**∈ 白名单则置当前段落；不在 → no-op**（保持上一个值，不回退 `main`）；本局首次命中前显示 `main`。

> ⚠️ **本处订正**：早期草案写「(A) 调用栈祖先法」，与段内脚本结构不符（原因见 10.2）。

### 10.2 依据与订正原因

1. **段内多个段落 sub 是兄弟 `call`，不互为祖先**：`MainSubNN` 由 `main/MainFront/MainLatter` 用 `call`(`0x0B`) 进入、末 `ret`(`0x0A`) 返回；**一个段落可能跨多个兄弟子脚本**，前一个 `ret` 后才 `call` 下一个 ⇒ 「段落入口 sub **整段**驻留栈上、可沿栈上溯」的 (A) 前提对**整个段落**不成立（只对单个 sub 成立）。故**每段只登记入口那一个 sub**，段内其余 sub 不在表内 → no-op。
2. **`call` 按名进入**：`0x482080` 解析 `ip+0x14` 的名字（`0x4848B0`）并把 `node+4` 置为该 sub 首指令 ⇒ 拿到 `node+4`（经名映射）即可知道“当前 sub”。
3. **stage 4 分支天然正确**：判定读的是**运行态**（当前 ip），而非静态时间轴；分支路径执行的 sub 不同 ⇒ 映射出的段落即该分支实际段落。**无需为分支特判**（但需真机各走一遍确认）。

### 10.3 关键难点：th14 无 sub 索引 ⇒ 需 ip→sub 映射

- th14 **不保存** sub 索引（`node`/`host` 均无；`+0x1EC` 是别的“场景/实体”对象的入口索引，见 §2.4）。
- 因此「当前 sub」需由 **ip → sub** 反推。两种可行实现（**均需真机验证**）：

**方案 ①（推荐，纯读、无侵入）— 按地址排序的 sub 表 + ip 区间映射**
```
// 每关（res 变化时）构建一次：按 table[i].+4 升序排序得 addr_sorted[]
// sub i 的指令区间 ≈ [table[i].+4, table[next].+4)（首指令在 +4+0x10）
NameOfIp(ip):
    lo = 最大的 i 使 table[i].+4 <= ip
    return table[lo].+0                     // 名字
```
- 说明：表本身**按名字有序**（供 `0x4848B0` 二分），故 `+4` 不单调；**先按 `+4` 重排**即得代码序区间。
- 风险：若某些 sub 的代码不严格连续（thprac 旧注释即指此），区间可能偏移；需真机抽样核对（尤其 `MainLatter` 段、boss 段）。

**方案 ②（最稳，稍侵入）— 影子 sub 名栈**
-在 `call` 落点（`0x482325`，`EAX`=被调首指令地址；或 `0x482080` 尾部）与 `ret`（`0x482412`）处，按 `node` 维护一份“当前 sub 名”影子（进入压名、返回弹名）。
- 逐节点维护；命中 `0x483c4f` 时直接读该节点影子栈栈顶即为“当前 sub”。
- 优点：不依赖 ip 区间连续性；缺点：需 hook 多处、维护 per-node 状态（注意热路径只读、勿分配/落盘）。

### 10.4 判定流程

```cpp
node = pCtx->Edi;                              // 命中点 0x483c4f
host = *(u32*)(node + 0x1014);
res  = *(u32*)(host + 0x11CC);
ip   = *(u32*)(node + 4);

name = NameOfIp(res, ip);                      // ip → sub（§10.3）
section = Whitelist(game, stage, name);        // 命中 → 置当前段落
if (section) current = section;                // 未命中 → no-op（不回退 main）
```

> checkpoint 粒度 = 每行 1 个入口 sub（= warp 落点所进入的 sub，**含 async 落点**）；容器 `main`/`MainFront`/`MainLatter` 不入表。
> 采样时机放在**帧间稳定态**（IGI 渲染），避免表达式临时栈占用 `SP` 时误读。

---

## 11. 真机验证清单（对应 requests §10）

- [ ] 道中/boss/不同 sub 采样：`NameOfIp` 结果与实际一致（**先验证 §10.3 方案 ① 的 ip 区间是否连续**）。
- [ ] **stage 4 两条分支**各走一遍，段落名均正确。
- [ ] `res = [[0x4db544]+0xcc]` 非空、`res+0x8C` 表可读、`res+8` 计数与 `subIdx` 越界保护生效。
- [ ] 练习跳章后 ip→sub 映射结果正确（Q8 复核）。
- [ ] 确认 `host+0x11CC == [[0x4db544]+0xcc]`（两处 res 是否同一对象）。
- [ ] 确认 `0x483c4f` 命中时 `Edi` 是否为主线节点（`Edi == *(u32*)(host+4)` 且 `host==*(u32*)(Edi+0x1014)`）；若不是，改锚定 `host+8`。
- [ ] 路过置值：经过各段入口 sub 时当前段落被正确置值；段内其余 sub 不改变当前段落（no-op）。
- [ ] 复用 §4.3 深度/越界校验；确认异步 `spawn` 段不污染主线节点栈。
- [ ] 若采用方案 ②：确认 hook 只在热路径**只读**，无分配/落盘。

---

## 12. 未定论 / 风险

1. **ip→sub 区间连续性**（决定方案 ① 成败）：`../exes/ecl_dump/th14/` 的 `.decl` 素材已在机，可**离线**尝试实证「sub 代码在内存中按 `+4` 连续铺开」；**仍建议真机抽样复核**，若发现断裂，退回方案 ②。
2. **主线节点判定**：命中点是“每节点”步进器，需真机确认 `Edi` 与主线节点（`host+8`）的关系。
3. **两个 res 是否同一对象**：`[scene+0x2D8]` vs `[[0x4db544]+0xCC]` 静态看都具备 `+8`/`+0x8C` 结构，但未证实二者恒等（thprac 现有 stage5 用法指向后者可用）。
4. **`+0x1EC` 语义**：是某些“场景/实体”对象的“入口/状态索引”，未证实会随执行更新；**不作为当前 sub 依据**。
5. **分支关 stage 4**：静态推定运行期天然正确，需真机两分支验证。
6. **地区/版本变体**：Steam/汉化版是否共用同一 RVA，需按变体实测。

---

> **boss 段续篇（方案二）**：TH14 **boss 段监控**（读 boss 宿主 ECL 当前 sub 名 + 白名单 latch）**已单独立档** → `docs/th14/th14_boss_phase_report.md`。要点：boss 宿主 = `*(u32*)(*(u32*)0x4DB544 + 0x5C + slot*4)`（槽表直接存宿主指针，`FUN_0041C050` 按槽取；`FUN_00421AA0` 按 id）、boss 门控 = `[host+0x5244] & 0x800000`（ECL `setBoss` opcode `0x200` 置/清）、当前 sub 沿用本文 §1/§10.3 的「`host→node→ip→res→sub 表` + ip→sub 区间映射」。本文 Q1–Q9 结论不变。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档。静态定案 **Q1 = (b) `[Edi+4]` 为当前指令指针（绝对 VA），非 sub 索引**（`Edi`=脚本节点；`+4` 由 `0x483C28` 指令长度推进、被解引用为指令；thprac 注释正确、编排者 (a) 假设证伪）；**Q2 确认**（加载器 `0x4846A0`：`+0`=名字 `char*`、`+4`=sub 数据绝对地址、`res+8`=计数；按名解析 `0x4848B0` 返回 `+4`+0x10）；**Q3 确认有显式调用栈**（`SP=node+0x1008`、栈底 `node+8`、帧 `0x10`、`call 0x0B` 压 / `ret 0x0A` 弹；**帧内无 sub 索引，祖先为返回 ip**，与 th15 的关键差异）；**Q4** 宿主=`node+0x1014`/`host+0x11CC`=res，写入点候选 `0x45883E`(+`0x484940`)；**Q5** 命中点在 `0x482380` 每节点步进器（由 `0x483E70` 每帧迭代）；**Q6** 运行期保留名字；**Q8** `ECLJump` 仅写 `goto 0x0C`，`0x4825D7` 只改 `node+0`/`node+4`；**Q9 定案为 (A) 调用栈祖先法**〔**后于 2026-10-15 订正为「路过式 checkpoint（latch）」**〕（须补 ip→sub 映射，给出「按地址排序表」与「影子名栈」两方案）。证据：`capstone` 反汇编 + th15 模板对照。**未真机。** |
| 2026-10-15 | 素材来源登记：`../exes/ecl/th14/`（`.ecl` 24 份）+ `../exes/ecl_dump/th14/`（`.decl` 24 份）已在机；据此把「th14 无离线 `.ecl`」订正为「`.decl` 素材已在机，可离线补脚本级证据」。清单见 `docs/logger/decompile_materials.md`。 |
| 2026-10-15 | **订正 Q9 口径**：由「(A) 调用栈祖先法」改为「**路过式 checkpoint（latch）**」——经过白名单函数即置值、不在表内 no-op（不回退 `main`）。原因：一个段落可能跨多个兄弟 `call`，不互为祖先，「整段驻留栈上」不成立 → 每 checkpoint 只需登记 1 个入口 sub（含 async）；仍需保留「ip→sub」映射层。§0 Q9 行、§10、§11 同步改写。 |
| 2026-10-16 | **新增 boss 段监控（方案二）交叉引用**：本文末尾追加指向 `docs/th14/th14_boss_phase_report.md`（TH14 boss 宿主取法 `mgr+0x5C` / boss 门控 `host+0x5244` bit23 / 当前 sub 链 / boss 行回填 64/72 行）。本次仅追加交叉引用与变更记录，正文 Q1–Q9 结论不变。 |

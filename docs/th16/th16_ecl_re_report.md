# TH16 ECL 段落监控 — 逆向结论（Q1–Q9）

> 对应需求：`docs/th16/th16_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`；同代模板：`docs/th15/th15_ecl_re_report.md`。
> 二进制：`../exes/th16.exe`，683520 B，PE `TimeDateStamp = 1501534029`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；本次核对 `.text` = VA `0x401000`–`0x48ACDA`，RVA `0x1000`–`0x89CDA`）。
> 引擎代：**C 代（第四代引擎）**；同代已定案参照：th15。
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` 只读反汇编（`reth.py`）；并与 th15 的 Ghidra 全量反编译 C（`../th15decode/th15.exe_decompiled.c`）逐函数对照。
> ECL / decl 素材：`../exes/ecl/th16/`（原始 `.ecl` 22 份）、`../exes/ecl_dump/th16/`（`thecl -d` 反编译 `.decl` 22 份），清单与来源见 `docs/logger/decompile_materials.md`。
> 素材已在机，故“名字↔段落入口”**可离线枚举**（枚举本身仍待做）；“运行期读取链路 / 函数栈 / 宿主捕获 / 跳转补丁语义”均已静态确证。
> 状态：**静态定案（🟡 已定案、未真机三态）**。地址口径：正文以 **VA** 为主，并标注 **RVA = VA − 0x400000**。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前 sub=索引** | ✅ **确认** | `subIdx = *(i32*)(Edi+4)`，`Edi = thread`（= th15 意义的线程对象）；`[thread+4]` 与 `[thread+8]` 同为 `-1` 表示线程结束；写入点 `0x41B8B2`(初始化 `main`)、`0x4720EB`(ret 恢复)、`0x471FA3`(call 切被调)、`0x472143`(置 −1)。 |
| **Q2 sub 表** | ✅ **确认** | `res = *(u32*)(*(u32*)(0x4A6DC0)+0x17C)`；`table = *(u32*)(res+0x8C)`；表项 8B，`+0`=名字 `char*`、`+4`=起始；`count = *(i32*)(res+8)`。加载器 `FUN_00474530`（SCPT 魔数 `0x54504353`），按名查表 `FUN_00474740`。 |
| **Q3 函数栈** | ✅ **存在可遍历显式子调用栈（与 th15 完全同构）** | `SP = *(u32*)(thread+0x100C)`，栈底 `thread+0xC`，帧长 `0x10`；帧 `+0x00`=旧SP / `+0x04`=time / `+0x08`=旧ip / `+0x0C`=**调用者 sub**；`call`(`0x0B`) 压帧、`ret`(`0x0A`) 弹帧；栈区上限 `0x1000`（≈256 帧）。 |
| **Q4 主线宿主捕获** | ✅ **给出捕获点** | 宿主 = `FUN_0041AA70` 的返回值（`EAX`）；「spawn main」两站点 `0x42DD52`/`0x42DF69`（其后 `0x42DD57`/`0x42DF6E` 可挂 EHOOK，命名参数 `"main"` @ `0x492270`）。**命中 `0x473995` 时 `Edi=thread`，`host=*(Edi+0x1018)`**，可直接在既有命中点取宿主。 |
| **Q5 解释器命中点** | ✅ **确认** | `0x473995` = `movss [edi],xmm0`（`F3 0F 11 07`，4B，写 `thread+0`=时间）；所在函数 `FUN_00472030`（`0x472030`/RVA `0x72030`）= ECL 线程步进/解释器；由宿主更新 `FUN_00473BC0` 每帧逐线程调用。 |
| **Q6 名字来源** | ✅ **运行期保留** | 表项 `+0` 为 `char*`，指向**已加载 ECL 缓冲内的名字串**（加载器 `FUN_00474530` 填入）；`name = *(char**)(table+idx*8)` 直接可读。 |
| **Q7 机体/子机** | ✅ **给出地址** | `CHARACTER = 0x4A57A4`、`SUBSHOT = 0x4A57AC`（均为全局 dword，全程序大量引用）。 |
| **Q8 ECLJump** | ✅ **不影响 sub 索引/名字** | `ECLJump` 只写一条 `goto`(`opcode 0x0C`)；`goto` 只改 `thread+0`(时间) 与 `thread+8`(ip)，**从不改 `thread+4`，不动 sub 表**。 |
| **Q9 段落判定口径** | ✅ **走 (A) 调用栈祖先法** | 段落入口 `MainSubNN` 由 `call` 进入并**驻留栈上**至 `ret`；沿 §Q3 遍历即得所属段落。**stage 5 分支天然兼容**（读的是运行期状态，非静态时间轴）。**⚠️ 段落入口白名单尚未枚举（`../exes/ecl_dump/th16/` 素材已在机，可离线枚举）。** |

> 关键地址（VA / RVA）：解释器 `FUN_00472030` = `0x472030`/`0x72030`；命中点 `0x473995`/`0x73995`；
> 压帧 `FUN_00471DB0` = `0x471DB0`/`0x71DB0`；`ret`(case `0x0A`) `0x4720CA`；`call`(case `0x0B`) `0x472281`；
> `goto`(case `0x0C`) `0x47230F`；opcode 跳表 `0x473A4C`（表）/ `0x473B58`（字节映射）；
> 加载器 `FUN_00474530`/`0x74530`；按名查表 `FUN_00474740`/`0x74740`；取当前 ip `FUN_004747D0`/`0x747D0`；
> 栈局部分配护栏 `FUN_00474810`/`0x74810`（上限 `0x1000`）；宿主 spawner `FUN_0041AA70`/`0x1AA70`；宿主构造 `FUN_0041B580`/`0x1B580`。

---

## 1. 统一指针链（TH16 定案）

```text
EDi (命中点/解释器内)  = thread                    // th15 意义的“线程/脚本上下文”
thread+0x000  float   当前 ECL 时间（命中点 0x473995 正在更新它）
thread+0x004  i32     当前 sub 索引 subIdx（−1 = 线程结束）
thread+0x008  i32     当前 sub 内 ip 偏移（−1 = 线程结束）
thread+0x00C  —       调用栈数组起点（帧存储基址）
thread+0x100C i32     调用栈指针 SP（0 = 空栈）
thread+0x1010 i32     表达式/局部值配对指针（ret 助手 FUN_00474860 使用）
thread+0x1018 u32     所属宿主 host

host    = *(u32*)(thread+0x1018)
res     = *(u32*)(host+0x11F8)                     // = *(u32*)(*(u32*)(0x4A6DC0)+0x17C)
count   = *(i32*)(res+0x008)                       // sub 总数（越界保护用）
table   = *(u32*)(res+0x08C)                       // sub 表，项 8B
name    = *(char**)(table + subIdx*8)              // 项 +0 = 名字 char*；项 +4 = 起始
```

与 th15 模板（`docs/th15/th15_ecl_re_report.md` §1.2）**逐字段一致**。

---

## 2. Q1 —— `[Edi+4]` = sub 索引，`Edi` = thread

### 2.1 判据

解释器入口 `FUN_00472030`（`0x472030`/RVA `0x72030`）prologue 立即把 `Edi` 当线程对象，取 `+4`/`+8` 并与 `−1` 判等（线程结束），再用 `[Edi+4]` **索引 sub 表**：

```asm
0047203b  8bf9              mov   edi, ecx                 ; edi = this = thread
00472047  8b5708            mov   edx, [edi+8]             ; thread+8 = ip 偏移
0047204a  83faff            cmp   edx, -1
0047204d  0f84f7000000      je    0x47214a                 ; ip == -1 → 线程结束
00472053  8b4f04            mov   ecx, [edi+4]             ; thread+4 = sub 索引
00472056  83f9ff            cmp   ecx, -1
00472059  0f84eb000000      je    0x47214a                 ; subIdx == -1 → 线程结束
0047205f  8b8718100000      mov   eax, [edi+0x1018]        ; host = thread+0x1018
00472065  8d7210            lea   esi, [edx+0x10]          ; esi = ip + 0x10
00472068  f30f1007          movss xmm0, [edi]              ; time
0047206c  8b80f8110000      mov   eax, [eax+0x11f8]        ; res = host+0x11F8
00472072  8b808c000000      mov   eax, [eax+0x8c]          ; table = res+0x8C
00472078  8b44c804          mov   eax, [eax+ecx*8+4]       ; table[subIdx].+4 = 本 sub 起始
0047207c  03f0              add   esi, eax                 ; esi = ip+0x10+起始 = 当前指令指针
```

`[Edi+4]` 被直接用作 `[eax+ecx*8+4]` 的下标 ⇒ **索引语义**（取值域 `0..count−1`，`−1` 为结束哨兵）。索引是 `edx`(`ip`)+`eax`(`起始`) 之和再作为绝对地址使用 ⇒ `[Edi+8]` 是 **sub 内偏移**，不是绝对指针。

**交叉印证**：`thprac_th16.cpp:536-538` 的 `mainLatterIDs[7] = {0,0,0,92,122,0,0}`（`stage = STAGE_NUM−1`）以 `subID == 92/122` 判 MainLatter。92/122 只可能是**索引**（若 `[Edi+4]` 是地址则不可能是这种小值）。且这两作（stage 4/5）的 sub 数必 > 122，与“索引”一致。

### 2.2 字段写入点（谁写 `thread+4`）

| VA / RVA | 语义 | 片段 |
| --- | --- | --- |
| `0x41B8B2` / `0x1B8B2` | 进关初始化，写入主线 `main` 的索引（按名查表 `FUN_00474740` 结果） | `call 0x474740` → `mov ecx,[edi+0xc]` → `mov [ecx+4],eax` |
| `0x471FA3` / `0x71FA3` | `call`(`0x0B`) 切到被调 sub：`thread+4 = 被调 sub 索引` | `mov ecx,[esi+0xc]` → `mov [ecx+4],eax` |
| `0x4720EB` / `0x720EB` | `ret`(`0x0A`) 恢复调用者：`thread+4 = 帧[+0x0C]` | `mov eax,[eax+ecx]` → `mov [edi+4],eax` |
| `0x472143` / `0x72143` | 线程结束：`thread+4 = −1`（与 `thread+8 = −1` 配对，见 `0x47213C`） | `mov dword ptr [edi+8],0xffffffff` / `mov dword ptr [edi+4],0xffffffff` |

> ⚠️ **勿混淆**：另有 `res+4`（ECL 文件槽计数器，加载器 `FUN_00474530` 内 `0x474717 mov [edi+4],eax`，`edi=res`）与 `thread+0x11A4`（8 元“活跃脚本槽”数组下标，见 §Q5 说明）。二者**都不是** thread 的 sub 索引。

---

## 3. Q2 —— sub 表：`res+0x8C`，项 8B

### 3.1 表解析（读侧）

见 §Q1 的 `0x47206c`–`0x472078` 片段：`res=*(host+0x11F8)`；`table=*(res+0x8C)`；`entry = table[idx*8]`，`+4`=起始。TH15 模板调用点 `0x4739C5`–`0x4739D1` 亦用同一链（该处下标为活跃槽下标，见 §Q5.3）。

### 3.2 建表 / 加载（写侧）—— `FUN_00474530`

ECL 加载器 `FUN_00474530`（`0x474530`/RVA `0x74530`）＝ th15 `FUN_0048F130` 的同源实现：

```asm
0047453f  8b5704            mov   edx, [edi+4]              ; 文件槽序号
00474542  8944970c          mov   [edi+edx*4+0xc], eax     ; 槽[edx] = ECL 文件缓冲
00474549  8b4c870c          mov   ecx, [edi+eax*4+0xc]     ; 缓冲首址
00474550  813953435054      cmp   dword ptr [ecx], 0x54504353  ; 魔数 "SCPT"
00474556  7411              je    0x474569
...
00474569  6683790401        cmp   word ptr [ecx+4], 1      ; 版本 1
00474570  0fb74106          movzx eax, [ecx+6]           ; 头尺寸
00474574  8d5124            lea   edx, [ecx+0x24]
00474577  03d0              add   edx, eax                 ; 名字区
00474579  0fb74110          movzx eax, [ecx+0x10]        ; 本文件 sub 数
0047457d  8b4f08            mov   ecx, [edi+8]             ; count
00474580  03c8              add   ecx, eax
00474589  894f08            mov   [edi+8], ecx             ; res+8 += sub 数   ← count
0047458d  8bb78c000000      mov   esi, [edi+0x8c]          ; 旧表
00474593  8d04cd00000000    lea   eax, [ecx*8]
0047459e  e8043a0000        call  0x477fa7                 ; malloc(count*8)
004745a3  89878c000000      mov   [edi+0x8c], eax          ; res+0x8C = 新表   ← table
...
004745c0  8b4704            mov   eax, [edi+4]
004745c3  8b4c870c          mov   ecx, [edi+eax*4+0xc]     ; 缓冲
004745c7  8b4508            mov   eax, [ebp+8]
004745ca  0308              add   ecx, [eax]               ; 缓冲 + 偏移
004745d2  894cd004          mov   [eax+edx*8+4], ecx       ; table[i].+4 = 起始
004745d6  8bcb              mov   ecx, ebx
004745de  8d7101            lea   esi, [ecx+1]
004745e1  891cd0            mov   [eax+edx*8], ebx         ; table[i].+0 = 名字 char*
```

⇒ **`table = *(u32*)(res+0x8C)`；项 8B；`+0`=名字、`+4`=起始；`count = *(i32*)(res+8)`**。`this`(ecx)=`res`，`res+0xC`=首个 ECL 文件缓冲（对应 `GetMemAddr(0x4A6DC0,0x17C,0xC)`，见 §Q2.4）。

### 3.3 按名查表 —— `FUN_00474740`

`FUN_00474740`（`0x474740`/RVA `0x74740`）＝ th15 `FUN_0048F340` 的同源二分查表：

```asm
00474747  8b7908            mov   edi, [ecx+8]             ; count
0047474c  83ef01            sub   edi, 1
00474751  8b898c000000      mov   ecx, [ecx+0x8c]          ; table
0047476d  8b04f1            mov   eax, [ecx+esi*8]         ; table[mid].+0 = 名字
00474770  8b4d08            mov   ecx, [ebp+8]             ; 目标名
00474773  8a11              mov   dl, [ecx]                ; 逐字节比较
00474775  3a10              cmp   dl, [eax]
```

⇒ `table[i].+0` 确为 `char*` 名字。`FUN_004747D0`（`0x4747D0`，被 `call`/`ret` 后的重定位分支调用）则返回 `table[[ecx+4]].+4 + 0x10 + [ecx+8]`（当前指令绝对地址），再次印证字段语义。

### 3.4 `GetMemAddr(0x4A6DC0, 0x17C, 0xC)` 的展开

`thprac_games.h:570-578` 的 `GetMemAddr(addr,off,...)=*(u32*)(GETMemAddr(...))`，展开：

```text
GetMemAddr(0x4A6DC0, 0x17C, 0xC) = *(u32*)( *(u32*)(0x4A6DC0) + 0x17C ) + 0xC = res + 0xC
```

⇒ 与加载器「首文件缓冲 = `res+0xC`」一致（`THSectionPatch` 的 `SetBaseAddr` 正是写 ECL 字节的那一级对象）。

---

## 4. Q3 —— ECL 子调用栈（**与 th15 完全同构**）

### 4.1 结论

1. **有显式可遍历子调用栈**，内嵌在 `thread` 内，随解释器自动维护：
   - **栈指针**：`SP = *(u32*)(thread+0x100C)`（`0` = 空/顶层）。
   - **栈存储**：`thread+0xC + SP`（dword 数组）。
   - **帧长**：`0x10`。
2. **栈顶帧存的是“调用者”**，当前 sub 仍是 `thread+4`。
3. `call`(`0x0B`) 压帧后切到被调 sub；`ret`(`0x0A`) 弹帧恢复调用者。
4. **栈区上限 `0x1000` 字节 ⇒ ≈256 帧**（护栏 `FUN_00474810`）。

### 4.2 `ret`（opcode `0x0A`）—— case @ `0x4720CA`/RVA `0x720CA`

```asm
004720ca  8d4f0c            lea   ecx, [edi+0xc]           ; ecx = 栈基址 thread+0xC
004720cd  e88e270000        call  0x474860                 ;（弹一值到 thread+0x1010）
004720d2  83bf0c10000000    cmp   dword ptr [edi+0x100c], 0; SP == 0 ?
004720d9  7461              je    0x47213c                 ;   空 → thread+4/8 = -1（结束）
004720db  838100100000fc    add   dword ptr [ecx+0x1000], -4; SP -= 4 （ecx+0x1000 = thread+0x100C）
004720e2  8b8100100000      mov   eax, [ecx+0x1000]
004720e8  8b0408            mov   eax, [eax+ecx]           ; 帧[+0x0C] = 调用者 sub
004720eb  894704            mov   [edi+4], eax             ; thread+4 = 调用者 sub
004720ee  838100100000fc    add   dword ptr [ecx+0x1000], -4
004720f5  8b8100100000      mov   eax, [ecx+0x1000]
004720fb  8b0408            mov   eax, [eax+ecx]           ; 帧[+0x08] = 旧 ip
004720fe  894708            mov   [edi+8], eax             ; thread+8 = 旧 ip
00472101  838100100000fc    add   dword ptr [ecx+0x1000], -4
00472108  ...                                          ; 帧[+0x04] = 旧 time → thread+0
00472113  838100100000fc    add   dword ptr [ecx+0x1000], -4
0047211a  8b8100100000      mov   eax, [ecx+0x1000]
00472120  8b0401            mov   eax, [ecx+eax]           ; 帧[+0x00] = 旧 SP
00472125  89870c100000      mov   [edi+0x100c], eax        ; thread+0x100C = 旧 SP
```

弹栈顺序 `帧[+0x0C]→thread+4`、`帧[+0x08]→thread+8`、`帧[+0x04]→thread+0`、`帧[+0x00]→thread+0x100C`；与压帧对称。**帧 `+0x0C` 即祖先 sub 索引**。

### 4.3 `call`（opcode `0x0B`）—— case @ `0x472281`/RVA `0x72281`

```asm
00472281  51                push  ecx
00472282  6a00              push  0
00472284  57                push  edi
00472285  8bcf              mov   ecx, edi
00472287  c6460c00          mov   byte ptr [esi+0xc], 0
0047228b  e820fbffff        call  0x471db0                 ; ← 压帧助手
00472298  8bcf              mov   ecx, edi
0047229a  e831250000        call  0x4747d0                 ; ← 取被调起始/写 thread+4
```

压帧助手 `FUN_00471DB0`（`0x471DB0`/RVA `0x71DB0`）＝ th15 `FUN_0048C800`（Ghidra C `line 95839` 逐句同构）。关键片段：

```asm
00471e02  8d4110            lea   eax, [ecx+0x10]          ; 新 SP = 旧 SP + 0x10 （帧长 0x10）
...
00471eb5  8b7df0            mov   edi, [ebp-0x10]          ; SP0
00471ec2  83860c100000fc    add   dword ptr [esi+0x100c], -4
...
00471ef3  83860c10000004    add   dword ptr [esi+0x100c], 4
00471f09  ...                                       ; 帧[ ] = thread+0  （time）
00471f1d  ...                                       ; 帧[ ] = thread+8  （旧 ip）
00471f31  ...                                       ; 帧[ ] = thread+4  （调用者 sub）
; SP0 == 0 的根哨兵分支：
00471f3d  c744300cffffffff  mov   [eax+esi+0xc], 0xffffffff ; = -1
00471f52  c744300cffffffff  mov   [eax+esi+0xc], 0xffffffff ; = -1
00471f67  c744300cffffffff  mov   [eax+esi+0xc], 0xffffffff ; = -1
; 切到被调 sub：
00471fa3  894104            mov   [ecx+4], eax             ; thread+4 = 被调 sub（FUN_00474740 查找结果）
00471fa9  c7400800000000    mov   [eax+8], 0               ; thread+8 = 0
00471fb3  c70000000000      mov   [eax], 0                 ; thread+0 = 0
```

⇒ 压帧顺序 `{旧SP, time, 旧ip, 旧subIdx}`；**根哨兵 `−1`**（`main` 首次 `call` 时 `SP0==0`，压入 `−1`）。与 th15 §1.2/§3.3 完全一致。

### 4.4 栈上限护栏 —— `FUN_00474810`/RVA `0x74810`（＝ th15 `FUN_0048F640`）

```asm
00474817  8bb100100000      mov   esi, [ecx+0x1000]        ; 当前 SP（ecx = thread+0xC）
0047481d  03d6              add   edx, esi
0047481f  81fa00100000      cmp   edx, 0x1000
00474825  7c08              jl    0x47482f                 ; SP+size < 0x1000 才继续
00474827  83c8ff            or    eax, 0xffffffff           ; 否则失败（栈溢出保护）
```

⇒ **栈区 `thread+0xC .. thread+0x100C`，字节上限 `0x1000`**（÷ 帧长 `0x10` ⇒ 物理上限 256 帧）。

### 4.5 遍历规则与实现注意

```cpp
// thread 见 §1（在 0x473995 处 = Edi）
uint32_t sp = *(uint32_t*)(thread + 0x100C);        // 0 = 无 caller
int n = 0;
while (sp >= 0x10 && sp < 0x1000 && n < MAX_DEPTH) {
    int32_t caller = *(int32_t*)(thread + 0x0C + sp - 4);   // 帧[+0x0C] = 调用者 sub
    if (caller < 0 || caller >= count) break;               // -1 = 根哨兵 → main
    ancestors[n++] = *(char**)(table + (uintptr_t)caller * 8);
    sp = *(uint32_t*)(thread + 0x0C + sp - 0x10);           // 帧[+0x00] = 旧 SP
}
// 祖先序: ancestors[0] = 直接调用者 … 最后 = 最外层；当前 sub 由 *(thread+4) 另行判定
```

- **遍历链**：以帧 `+0x00`（旧 SP）逐帧回退，逐帧读 `+0x0C`。
- **只应在指令边界（帧间稳定态，如 IGI 渲染）遍历**：表达式临时值 / 栈局部共用同一 `SP`（`FUN_00474860`/`FUN_00474810` 亦操作 `thread+0x100C`/`thread+0x1010`）。
- 空指针/越界保护：`res`/`table`/`count`/`sp`/`caller` 全部校验。

### 4.6 异步 `spawn` 的例外

TH15 的异步 `spawn`（opcode `0x0F`/`0x15`）另起线程对象；th16 同为把新线程挂到宿主自身的线程链表（见 §Q4.3 的 `host+0x11FC` 链），**自带空栈**，不混入主线的同步调用链。读主线宿主对应的 thread 即可只看到同步调用链。

---

## 5. Q4 —— 主线宿主捕获点

### 5.1 宿主 spawner：`FUN_0041AA70`（＝ th15 `FUN_00426050`）

`FUN_0041AA70`（`0x41AA70`/RVA `0x1AA70`）分配 `0x574c` 字节并构造宿主对象，**返回值 `EAX` = 宿主**：

```asm
0041aa70  55                push  ebp
...
0041aa76  684c570000        push  0x574c                   ; malloc(0x574c)  ← 与 th15 FUN_00426050 同尺寸
0041aa7d  e82a9f0500        call  0x4749ac
...
0041aa8d  e8ee0a0000        call  0x41b580                 ; ← 宿主构造/初始化（见 §5.2）
```

th15 对应：`FUN_00426050`（`0x426050`）同样 `malloc(0x574c)` 后调 `FUN_00426c60`；th15 的宿主捕获 hook（`0x43BBC8`/`0x43D48E`/`0x43D6BE`）正是抓取该调用后的 `EAX`。

### 5.2 「spawn main」站点（`name = "main"`）

字符串 `"main"` 位于 `0x492270`（RVA `0x92270`）。两个主线宿主 spawn 站点：

```asm
; 站点 1 @ 0x42DD46（进关/重置路径之一）
0042dd39  ...                             ; 初始化参数结构 [ebp-0x58]
0042dd45  51                push  ecx
0042dd46  8b0dc06d4a00      mov   ecx, [0x4a6dc0]          ; ECX = ECL 管理器 *(0x4A6DC0)
0042dd4c  50                push  eax
0042dd4d  6870224900        push  0x492270                 ; "main"
0042dd52  e819cdfeff        call  0x41aa70                 ; EAX = 宿主  ← 捕获点（下一条 0x42DD57 可挂 EHOOK 5B）
0042dd57  e81490ffff        call  0x426d70

; 站点 2 @ 0x42DF64（另一重置/重开路径）
0042df57  8b0dc06d4a00      mov   ecx, [0x4a6dc0]
0042df5d  8d45a8            lea   eax, [ebp-0x58]
0042df63  50                push  eax
0042df64  6870224900        push  0x492270                 ; "main"
0042df69  e802cbfeff        call  0x41aa70                 ; EAX = 宿主  ← 捕获点（下一条 0x42DF6E 可挂 EHOOK 5B）
0042df6e  e8fd8dffff        call  0x426d70
```

另有两处调用 `FUN_0041AA70`：`0x41480D`（其下一条 `0x414812` 是 5B `jmp`）、`0x42323E`（其下一条 `0x423243` 是 `pop edi`）。若采用「调用后抓 EAX」的方式，需处理重新补齐被覆盖指令。

### 5.3 宿主构造 `FUN_0041B580`（＝ th15 `FUN_00426C60`）：设 `res` 并写入 `main`

```asm
0041b580  55                push  ebp
0041b586  8bf9              mov   edi, ecx                 ; edi = host
0041b596  c707a8214900      mov   dword ptr [edi], 0x4921a8; vtable
...
0041b897  8b897c010000      mov   ecx, [ecx+0x17c]         ; res = *(mgr+0x17C)
0041b89d  898ff8110000      mov   [edi+0x11f8], ecx        ; host+0x11F8 = res
0041b8a3  e8988e0500        call  0x474740                 ; 按名查表（名 = "main"）
0041b8a8  8b4f0c            mov   ecx, [edi+0xc]           ; thread = host+0xC
0041b8b2  894104            mov   [ecx+4], eax             ; thread+4 = main 的 sub 索引 ★Q1 写入点
0041b8b8  c7400800000000    mov   [eax+8], 0
0041b8c2  c70000000000      mov   [eax], 0
```

⇒ `host+0x11F8 = *(u32*)(*(u32*)(0x4A6DC0)+0x17C) = res`；`host+0xC = thread`；`thread+4` 初始化为 `main`。**与 th15 `FUN_00430880`（Ghidra C `line 32474`：`*(param_1+0x11f8)=*(iVar1+0x17c)`）逐句同构。**

### 5.4 命中点 `0x473995` 时的 `Edi`：直接可得宿主

解释器内 `host = *(u32*)(Edi+0x1018)`，而宿主更新循环在调用解释器**前**刚把 `host+0xC = Edi`：

```asm
00473bdc  8db3fc110000      lea   esi, [ebx+0x11fc]        ; ebx = host，宿主线程链表头
00473bf0  8b7e04            mov   edi, [esi+4]             ; 下一节点
00473bf6  8b0e              mov   ecx, [esi]               ; 线程
00473bf8  894b0c            mov   [ebx+0xc], ecx           ; host+0xC = 当前线程
00473bff  e82ce4ffff        call  0x472030                 ; 运行该线程（解释器）
```

⇒ 在 `0x473995`（`Edi=thread`）处，`*(Edi+0x1018)` 即宿主，`*(host+0xC)==Edi`。**故 thprac 无需新增独立宿主捕获钩子：在既有命中点用 `Edi+0x1018` 取宿主即可。** 若要与 th15 完全对齐（缓存主线宿主），可在 §5.2 两站点抓 `EAX`。

> ⚠️ **Edi 是否恒为主线宿主**：`FUN_00472030` 是**逐线程**步进；`0x473BC0` 会遍历宿主链表 `host+0x11FC` 的每个线程依次调用。通常主线仅一个活跃线程（spawn 线程瞬态），但仍建议：**捕获主线宿主后，用 `*(host+0xC)==Edi` 过滤**，或直接在主线宿主上读 `*(host+0xC)`。（th15 既有实现也依赖同一前提，实机已验证。）

---

## 6. Q5 —— 解释器命中点 `0x473995` 与寄存器语义

### 6.1 命中点

```asm
0047398b  f30f1007          movss xmm0, [edi]              ; thread+0 = 当前时间
0047398f  f30f58442424      addss xmm0, [esp+0x24]         ; += 帧增量
00473995  f30f1107          movss [edi], xmm0              ; ★EHOOK(4B) 命中：写回 thread+0
00473999  8d8724100000      lea   eax, [edi+0x1024]        ; Eip 恢复目标（跳过整条 4B 指令）
```

- **地址**：`0x473995` / RVA `0x73995`；**覆盖 4 字节**（`F3 0F 11 07`），正好是一条完整指令。
- **命中寄存器**：`Edi = thread`（执行对象）；`thread+0` = 时间；`thread+4` = sub 索引；`thread+0x1018` = host。
- **语义**：当前指令的 `time` 尚未到达（上一轮比较 `jb` 分流到此处），此指令把帧增量累加到线程时间上 —— 即 `thprac` 注释的 “freeze ECL sub time”；`pCtx->Eip = 0x473999` 即跳过本次时间推进。

### 6.2 所在函数与调用链

- **函数**：`FUN_00472030`，入口 `0x472030`（RVA `0x72030`），函数体 `0x472030`–`0x473A4A`（`ret` 后紧邻 opcode 跳表 `0x473A4C`）。prologue：

```asm
00472030  55                push  ebp
00472031  8bec              mov   ebp, esp
00472033  83e4f8            and   esp, 0xfffffff8
00472036  83ec38            sub   esp, 0x38
00472039  56                push  esi
0047203a  57                push  edi
0047203b  8bf9              mov   edi, ecx                 ; edi = thread
```

- **opcode 分发**：`0x4720A0` 起，取指令 `[esi+4]` 的 opcode（signed word），经字节映射表 `0x473B58` 转索引，再经跳表 `0x473A4C` 分发：

```asm
004720af  0fbf4604          movsx eax, word ptr [esi+4]      ; opcode
004720b3  83f85d            cmp   eax, 0x5d
004720b6  0f877b180000      ja    0x473937                 ; 越界 → 走宿主虚调用分支
004720bc  0fb680583b4700    movzx eax, byte ptr [eax+0x473b58]
004720c3  ff24854c3a4700    jmp   dword ptr [eax*4+0x473a4c]
```

  经映射核实：**opcode `0x0A`(ret) → `0x4720CA`；`0x0B`(call) → `0x472281`；`0x0C`(goto) → `0x47230F`**（逐条见下）。这与 th15 的 `0x0A`/`0x0B`/`0x0C` 编号一致。

- **调用链（每帧逐线程）**：主更新函数 `FUN_00473BC0`（`0x473BC0`，遍历 `host+0x11FC` 线程链）→ 对每个线程 `call FUN_00472030`；`FUN_00473BC0` 被 `0x41C535`/`0x41C80A`/`0x41D331`/`0x41D6D1`（关卡更新路径）调用。

### 6.3 作为每帧稳定触发点？

- `FUN_00472030` 由宿主更新**每帧**对每个活跃线程调用一次；`0x473995` 位于「指令未到期、推进线程时间」的分支，**在等待定时指令期间每帧命中**（道中/boss 等大量 `ins_23/wait` 场景即如此）。
- **局限**：处于「连续立即指令」时线程走 `0x4720A0` 分发分支而不经过 `0x473995`，故它**不是无条件每帧**。
- **结论**：`0x473995` 可作稳定的“当前 sub 读取点”（读 `Edi+4`/`Edi+0x1018`），与既有实现一致；若要**无条件每帧**触发，建议改挂宿主更新 `0x473BC0`（每次 `ecx`/`ebx`=宿主）或在 `0x473BC0` 的循环内取线程。

### 6.4 ⚠️ 与编排者初步观察的更正

编排者注意到命中点附近 `0x4739BC` 的 `host/res/table/[ecx*8+4]` 链与 th15「逐指令相同」，**指针链确实相同**，但**该处的下标寄存器不是 `[Edi+4]`**，而是活跃脚本槽数组 `[edx-4] = [Edi+0x11A4]`：

```asm
00473999  8d8724100000      lea   eax, [edi+0x1024]        ; 槽数组 A（8 元，跨距 0x30）
0047399f  b908000000        mov   ecx, 8
004739a4  8d97a8110000      lea   edx, [edi+0x11a8]        ; 槽数组 B（8 元，跨距 8）
...
004739bc  8bb718100000      mov   esi, [edi+0x1018]        ; host
004739c2  8b4afc            mov   ecx, [edx-4]             ; ★= [edi+0x11A4]（活跃槽下标，非 thread+4）
004739c5  8b86f8110000      mov   eax, [esi+0x11f8]        ; res
004739cb  8b808c000000      mov   eax, [eax+0x8c]          ; table
004739d1  8b44c804          mov   eax, [eax+ecx*8+4]       ; table[槽下标].+4
```

⇒ **`thread+4`（当前 sub 索引）与 `thread+0x11A4`（活跃脚本槽下标）是两回事**；当前 sub 索引的正确落点是 §2.1 的 `0x472053`–`0x472078`（以及 §Q1 的写入点）。取“当前 ECL 函数名”应读 **`*(Edi+4)`**，不要读 `[Edi+0x11A4]`。

---

## 7. Q6 —— 运行期保留 sub 名

✅ **保留**。加载器 `FUN_00474530` 在 `0x4745E1` 把**名字区指针**（`ebx`，指向已加载 ECL 缓冲内的名字串）直接填入 `table[i].+0`（见 §3.2）。故运行期 `name = *(char**)(table+idx*8)` 指向常驻缓冲内的 `char*`，**无需离线名表**即可读名字。

---

## 8. Q7 —— `character` / `shotType` 地址

| 名称 | VA / RVA | 说明 |
| --- | --- | --- |
| `CHARACTER` | `0x4A57A4` / `0xA57A4` | 机体 ID（全局 dword） |
| `SUBSHOT` | `0x4A57AC` / `0xA57AC` | 子机类型（全局 dword） |

出处：`thprac_th16.cpp:10-11` 的 `addrs` 枚举；静态核实两地址被全程序大量引用（`CHARACTER` 73 处、`SUBSHOT` 23 处），例如自机得分/统计路径：

```asm
0042dc1b  8b0da4574a00      mov   ecx, [0x4a57a4]          ; CHARACTER
0042dc21  030da8574a00      add   ecx, [0x4a57a8]          ; （+0x4A57A8）
0042dc27  69c918530000      imul  ecx, ecx, 0x5318
```

（`STAGE_NUM = 0x4A5790`、`PLAYER_PTR = 0x4A6EF8` 亦同步核实：xref 密集。）状态：**静态确认地址有效**，具体取值语义待真机核对。

---

## 9. Q8 —— 练习 `ECLJump` 对读取的影响

### 9.1 结论

✅ **不影响 sub 索引/名字判读**（与 th15 §2 一致）。

### 9.2 依据

1. **补丁写入**（`thprac_th16.cpp:1078-1082`）：
   ```cpp
   ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
   ```
   展开为 6 个 dword（24B，与 `size`=0x18 一致）：`[ecl_time, 0x0018000C, 0x02ff0000, 0, dest-start, at_frame]`。其中 `dword1 = 0x0018000C`：低 16 位 `0x000C` = **`goto`**，高 16 位 `0x0018` = `size`。

2. **`goto` 语义**（case `0x0C` @ `0x47230F`/RVA `0x7230F`）：
   ```asm
   0047230f  660f6e4614        movd  xmm0, dword ptr [esi+0x14]  ; [esi+0x14] = at_frame
   00472314  8d4e10            lea   ecx, [esi+0x10]
   00472317  0f5bc0            cvtdq2ps xmm0, xmm0
   0047231a  f30f1107          movss [edi], xmm0                ; thread+0 = at_frame（绝对赋值）
   0047231e  8b01              mov   eax, [ecx]                ; [esi+0x10] = dest-start
   00472320  014708            add   [edi+8], eax              ; thread+8 += (dest-start)
   00472323  0331              add   esi, [ecx]
   00472325  8974240c          mov   [esp+0xc], esi
   00472329  e949160000        jmp   0x473977
   ```
   **只改 `thread+0`（时间）与 `thread+8`（ip），从不触碰 `thread+4`。**

3. **sub 结构未动**：`ECLHelper::SetBaseAddr(res+0xC)` 定位的是运行期 ECL 文件缓冲；`ECLJump` 只在硬编码偏移处覆写字节，不触碰 SCPT 头、名字区、`res+0x8C` 表（表由 `FUN_00474530` 一次解析生成）。

4. **瞬态注意**：`goto` 之后、目标 `call` 尚未执行的那一瞬，`thread+4` 仍是跳转前所在 sub（通常 `main`）；`call` 后即正确。采样落在帧间稳定态即可规避。

---

## 10. Q9 —— 段落判定口径：**(A) 调用栈祖先法**

### 10.1 结论

✅ **走 (A)**。th16 的同步调用栈与 th15 完全同构（§Q3），段落入口 `MainSubNN` 由 `call`(`0x0B`) 进入、**整个段落期间驻留栈上**，`ret` 才弹出；沿栈上溯（帧 `+0x0C`）即得最近的白名单祖先。

### 10.2 依据

- **栈存在且可遍历**（§Q3）：满足 (A) 的前提（(B) 仅在“无栈”时才必需）。
- **入口为驻留调用帧**：`main` 初始化（§5.3）与 `call` 压帧机制（§4.3）表明段落入口是**驻留的调用帧**，不是“只经过的点”，故无需顺序 latch。
- **同代定案**：th15（模板）已定 (A)；th16 的 `call`/`ret`/栈布局逐指令同构 ⇒ 结论可直接搬运。
- **stage 5 分支天然兼容**：本判定读的是**运行期状态**（当前 sub + 其调用栈祖先），分支走下去哪条路径，读到的 sub 就是哪条 —— 分支不影响正确性。仅需白名单**同时收录两条分支各自的段落入口**。

### 10.3 推荐判定流程

```cpp
host   = *(u32*)(Edi + 0x1018);          // 0x473995 处；或捕获自 0x42DD57/0x42DF6E
thread = *(u32*)(host + 0x0C);           // 通常情况下 == Edi
res    = *(u32*)(host + 0x11F8);
count  = *(i32*)(res + 8);
table  = *(u32*)(res + 0x8C);

idx    = *(i32*)(thread + 4);            // ① 当前 sub
section = Whitelist(game, stage, NameOf(table, idx, count));
if (!section) {                          // ② 沿调用栈上溯（§4.5）
    for (sp = *(u32*)(thread + 0x100C);
         sp >= 0x10 && sp < 0x1000;
         sp = *(u32*)(thread + 0x0C + sp - 0x10)) {
        int caller = *(i32*)(thread + 0x0C + sp - 4);   // 帧[+0x0C]
        if (caller < 0 || caller >= count) break;        // 根哨兵 -1 → main
        section = Whitelist(game, stage, NameOf(table, caller, count));
        if (section) break;                              // ③ 最近白名单祖先
    }
}
if (!section) section = "main";                          // ④ 兜底
```

> 白名单粒度 = `MainSubNN`（与 th15 一致）；容器 `main`/`MainFront`/`MainLatter` 与异步子脚本不建条目，命不中即回退。

### 10.4 ⚠️ 未定论项（段落入口白名单待枚举）

- th16 的段落入口 `MainSubNN` 名单与 `THStageWarp` 偏移一一对应的入口表**尚未枚举**（`../exes/ecl_dump/th16/` 的 `.decl` 素材已在机，可离线枚举）（可用现有 `mainLatterIDs` 的 92/122 作为首批已知点交叉验证）。
- “段落入口 sub 在整段期间驻留栈上”是**由 th15 同构 + th16 机制确认推断**，th16 自身脚本结构**未逐脚本实测**（静态、待真机）。

---

## 11. 真机验证清单

- [ ] 道中/boss/不同 sub 采样，`NameOf(*(thread+4))` 与实际一致（用 `mainLatterIDs` 的 92/122 交叉印证）。
- [ ] **函数栈上溯**：在段落内取一个非白名单子 sub，确认上溯命中最近 `MainSubNN`；`caller==-1` 处按 `main` 兜底。
- [ ] **stage 5 两条分支**各走一遍，运行期读取与白名单映射均正确。
- [ ] `res = *(u32*)(*(u32*)(0x4A6DC0)+0x17C)` 非空、`res+0x8C` 表可读、`subIdx`/`caller` 越界保护生效。
- [ ] 练习跳章后：`thread+4` 名字与上溯结果均正确（Q8 复核）。
- [ ] 命中点 `0x473995` 的 `Edi` 是否恒为主线线程（若出现 `host+0x11FC` 异步线程干扰，改用宿主捕获 + `*(host+0xC)==Edi` 过滤）。
- [ ] 采样时机置于帧间稳定态（IGI 渲染），避免表达式临时栈占用 `thread+0x100C` 时误读。
- [ ] Q7 的 `CHARACTER`(0x4A57A4)/`SUBSHOT`(0x4A57AC) 取值与所选机体/子机一致。

---

## 12. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（本报告）。静态定案 Q1–Q9：Q1 `[Edi+4]`=sub 索引（写入点 `0x41B8B2`/`0x4720EB`/`0x471FA3`/`0x472143`）；Q2 `res=*(*(0x4A6DC0)+0x17C)`、`table=res+0x8C`、项 8B（加载器 `FUN_00474530`，查表 `FUN_00474740`）；Q3 显式调用栈（`SP=thread+0x100C`、栈底 `thread+0xC`、帧 `0x10`、帧 `+0x0C`=祖先、上限 `0x1000`，`call 0x0B`/`ret 0x0A`，与 th15 逐指令同构）；Q4 宿主 spawner `FUN_0041AA70`、站点 `0x42DD52`/`0x42DF69`（`"main"`@`0x492270`），命中点可直接 `*(Edi+0x1018)` 取宿主；Q5 命中点 `0x473995`=`movss [edi],xmm0`(4B)、函数 `FUN_00472030`、opcode 表 `0x473A4C`/`0x473B58`；Q6 运行期保留名字；Q7 `CHARACTER=0x4A57A4`/`SUBSHOT=0x4A57AC`；Q8 `ECLJump` 只写 `goto`(0x0C)，不改 `thread+4`；Q9 定案 **(A) 调用栈祖先法**。更正编排者初判：`0x4739BC` 处下标为 `[Edi+0x11A4]`（活跃槽），非 `thread+4`。证据：`capstone` 反汇编 + th15 Ghidra 全量反编译 C 逐函数对照。**未真机三态；段落入口白名单待枚举。** |
| 2026-10-15 | 素材来源登记：`../exes/ecl/th16/`（`.ecl` 22 份）+ `../exes/ecl_dump/th16/`（`.decl` 22 份）已在机；据此把「本机无 th16 `.ecl` / 离线枚举无法完成」订正为「素材已在机、段落入口白名单待枚举」。清单见 `docs/logger/decompile_materials.md`。 |

# TH18 ECL 段落监控 — 逆向结论（Q1–Q9）

> 对应需求：`docs/th18/th18_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`；
> 同代模板与结构范本：`docs/th16/th16_ecl_re_report.md`、`docs/th17/th17_ecl_re_report.md`、`docs/th15/th15_ecl_re_report.md`。
> 二进制：`../exes/th18.exe`，**847360 B**，PE `TimeDateStamp = 1618619144`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**，绝对 VA 以立即数内嵌，可直接按 4 字节小端做字节搜索定位 xref）。
> 本次核对 `.text` = VA `0x401000`–`0x4AD000`（vsize `0xAC000`，raw size `0xAB800`）。
> 引擎代：**C 代（第四代引擎）**；与 th15/16/17 **同源同形**（指令级基本重合，仅偏移/尺寸不同）。
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` **只读静态反汇编**（脚本 `../../reasm/th18/reth.py`，本机无 IDA/Ghidra/radare2）；
> 按 `RELOCS_STRIPPED` 用「4 字节小端 VA 立即数」做字节搜索定位 xref；并与 th15/16/17 逐字段对照。
> ECL / decl 素材：`../exes/ecl/th18/`（原始 `.ecl` 22 份）、`../exes/ecl_dump/th18/`（`thecl -d` 反编译 `.decl` 22 份；本作无 eclmap，指令呈 `ins_*` 形态），清单与来源见 `docs/logger/decompile_materials.md`。
> **本机无法运行 th18**；`.decl` 素材已在机，可离线枚举段落入口 ⇒ 状态：**静态定案（🟡 已定案、未真机三态）**，全部结论均有「地址 + 反汇编片段」支撑。
> 地址口径：正文以 **VA** 为主，关键处附 **RVA = VA − 0x400000**。
> 只读：本文为新建文件，**未改动任何既有文件**（对既有文档的回填写成 §12「回填建议」）。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前 sub = 索引** | ✅ **确认** | `subIdx = *(i32*)(Edi+4)`，`Edi = thread`（th15 意义的线程对象）；`[thread+4]` **不是**指令指针，而是**当前 sub 索引**（`−1` = 线程结束 / 按名解析失败）；写入点 `0x42E43F`(初始化 `main`)、`0x48D9BB`(`call` 按名解析成功)、`0x48D9A7`(解析失败→−1)、`0x48B474`(`ret` 恢复调用者)、`0x48B4D1`(线程结束→−1)、`0x48D899`(`spawn` 新线程→−1)。 |
| **Q2 sub 表** | ✅ **确认（本作资源偏移 = +0x188，实测成立）** | `res = *(u32*)(*(u32*)0x4CF2D0 + 0x188)`（= `host+0x1218`）；`table = *(u32*)(res+0x8C)`；`count = *(i32*)(res+8)`；表项 **8 B**，`+0` = 名字 `char*`、`+4` = sub 起始绝对地址（首指令 = `+4 + 0x10`）。加载器 `FUN_0048D9D0`（SCPT 魔数 `0x54504353`），按名查表 `FUN_0048D920`。 |
| **Q3 函数栈** | ✅ **存在可遍历显式子调用栈（与 th15/16/17 逐字段同构）** | `SP = *(u32*)(thread+0x100C)`，栈底 `thread+0xC`，帧长 `0x10`；帧 `+0x00`=旧SP / `+0x04`=time / `+0x08`=旧ip / `+0x0C`=**调用者 sub**；`call`(`0x0B`) 压帧、`ret`(`0x0A`) 弹帧；栈区上限 `0x1000`（≈256 帧，护栏 `0x48B778`）。 |
| **Q4 主线宿主捕获** | ✅ **给出捕获点** | 宿主 = `FUN_0042D7D0` 的返回值（`EAX`）；「spawn `main`」两站点 `0x444228` / `0x444471`（`call 0x42D7D0`，`ecx = *(u32*)0x4CF2D0`，参 = `"main"`@`0x4B6634`），其后 **`0x44422D` / `0x444476`** 可挂 EHOOK（`EAX` = 宿主）。命中 `0x48C822` 时 `Edi = thread`，`host = *(u32*)(Edi+0x1018)`；**`Edi` 不必然为主线 thread**（见 Q4.4）。 |
| **Q5 解释器命中点** | ✅ **确认** | `0x48C822` = `f3 0f 11 0f` = `movss [edi],xmm1`（4 B，写 `thread+0` = 时间）；所在函数 `FUN_0048B3A0`（`0x48B3A0` / RVA `0x8B3A0`）= ECL 线程步进器/解释器；由宿主步进 `FUN_0048D420`（`0x48D420`）逐线程调用（遍历 `host+0x121C` 线程链）。 |
| **Q6 名字来源** | ✅ **运行期保留** | 表项 `+0` 为 `char*`，指向**已加载 ECL 缓冲内的名字串**（加载器 `0x48DA71` 填入）；`name = *(char**)(table+idx*8)` 直接可读；按名查表 `0x48D920` 对 `table[mid].+0` 逐字节 `strcmp` 自证。 |
| **Q7 机体/子机** | 🟡 **部分** | 定位到**角色名指针表 `0x4B41E0`**（`Reimu / Marisa / Sakuya / Sanae`，共 4 角色）与其**每角色函数表** `0x4B41F4`/`0x4B41F8`/`0x4B41FC`（`0x45F9D0` / `0x460320` / `0x4609C0`）；**未定位**“关卡内当前 character / shotType 全局 dword”（thprac_th18.cpp 无既有常量，需真机或进一步逆向，见 Q7）。 |
| **Q8 ECLJump** | ✅ **不影响 sub 索引/名字** | `ECLJump` 只写一条 `goto`(`opcode 0x0C`)；`goto`(`0x48B700`) 只改 `thread+0`(time) 与 `thread+8`(ip)，**从不改 `thread+4`**，不动 sub 表。 |
| **Q9 段落判定口径** | ✅ **路过式 checkpoint（latch）** | CSV 即白名单：当前 sub ∈ 表 → 置当前段落；**不在 → no-op**（不回退 `main`）。**每 checkpoint 只登记 1 个入口函数**（含 async 落点）。**⚠️ 段落入口白名单尚未枚举（`../exes/ecl_dump/th18/` 素材已在机，可离线枚举）。** |

> **关键地址（VA / RVA）**
> 解释器 `FUN_0048B3A0` = `0x48B3A0` / `0x8B3A0`；命中点 `0x48C822` / `0x8C822`（原始字节 `f3 0f 11 0f`）；
> 时间推进分支 `0x48C81C`（`addss xmm1,[esp+0x20]`）；
> opcode 分发：`0x48B423`(`movsx eax,word [eax+4]`) / `cmp eax,0x5E`@`0x48B427` / 字节映射表 `0x48D3B8` / 跳表 `0x48D2A8`；
> `ret`(case `0x0A`) `0x48B43E`；`call`(case `0x0B`) `0x48B66C`；`goto`(case `0x0C`) `0x48B700`；
> 终止(case `0x01`) `0x48B4CA`；`spawn`(case `0x0F`) `0x48B4E1`；`spawn+val`(case `0x10`) `0x48B530`；`spawn`(case `0x15`) `0x48B4F5`；栈局部分配护栏(case `0x28`) `0x48B762`；
> 压帧助手 `FUN_0048B030` = `0x48B030` / `0x8B030`；栈上限检查 `0x48B778`；
> 加载器 `FUN_0048D9D0` / `0x8D9D0`；按名查表/设 sub `FUN_0048D920` / `0x8D920`；当前指令重解码 `FUN_0048DBE0` / `0x8DBE0`；按名起 sub `FUN_0048DC20` / `0x8DC20`；
> 线程步进器 `FUN_0048D420` / `0x8D420`；spawn 新线程 `FUN_0048D850` / `0x8D850`；
> 宿主工厂 `FUN_0042D7D0` / `0x2D7D0`；宿主构造 `FUN_0042E100` / `0x2E100`；宿主基构造 `0x42CE10` / `0x2CE10`；宿主复位 `0x42CE80` / `0x2CE80`；线程基构造 `0x42CCC0` / `0x2CCC0`；
> `"main"` @ `0x4B6634` / `0xB6634`；`main` spawn 站点 `0x444228`/`0x444471`（捕获 `0x44422D`/`0x444476`）。

---

## 1. 统一指针链（TH18 定案）

```text
EDi (命中点/解释器内)  = thread                        // th15 意义的“线程/脚本上下文”
thread+0x000  float   当前 ECL 时间（命中点 0x48C822 正在更新它）
thread+0x004  i32     当前 sub 索引 subIdx（−1 = 线程结束 / 解析失败）
thread+0x008  i32     当前 sub 内 ip 偏移（−1 = 线程结束）
thread+0x00C  —       调用栈数组起点（帧存储基址；线程基构造 memset(thread+0xC,0,0x1008)）
thread+0x100C i32     调用栈指针 SP（0 = 空栈）
thread+0x1010 i32     表达式/临时栈配对指针（ret/栈局部使用）
thread+0x1018 u32     所属宿主 host
thread+0x1020 u8      指令可见性/难度掩码（test byte [edi+0x1020], al）
thread+0x11A4 —       8 元“活跃脚本槽”起点（**非** sub 索引，勿混，见 §Q5.3）
thread+0x11C8 —       与上配对的第二槽数组（步进循环用 [edx-4] 作 table 下标）

host    = *(u32*)(thread+0x1018)
res     = *(u32*)(host+0x1218)                     // = *(u32*)(*(u32*)0x4CF2D0 + 0x188)
count   = *(i32*)(res+0x008)                       // sub 总数（越界保护用）
table   = *(u32*)(res+0x08C)                       // sub 表，项 8 B
name    = *(char**)(table + subIdx*8)              // 项 +0 = 名字 char*；项 +4 = sub 起始绝对地址
curInstr= *(u32*)(table+subIdx*8+4) + 0x10 + *(u32*)(thread+8)

rootThread = host + 0x10                           // 主线“根线程”内嵌在宿主内（构造/复位时 host+0xC = host+0x10）
threadChain= host + 0x121C                         // 宿主线程链头（节点: [0]=thread, [4]=next, [8]=prev）
```

### 1.1 与 th16 / th17 / th15 的**偏移差异表**（本作重点核对项）

| 项 | th15 | th16 | th17 | **th18（本作）** |
| --- | --- | --- | --- | --- |
| 管理器 → `res` | `0x4E9A80 + 0x17C` | `0x4A6DC0 + 0x17C` | `0x4B76A0 + 0x17C` | **`0x4CF2D0 + 0x188`** ✅（实测） |
| `host → res` | `+0x11F8` | `+0x11F8` | `+0x11F8` | **`+0x1218`** |
| 宿主线程链头 | `+0x1200` | `+0x11FC` | `+0x11FC` | **`+0x121C`** |
| 根线程 | `host+0xC` | `host+0xC` | `host+0x10` | **`host+0x10`** |
| `thread` 对象尺寸 | `0x11E8` | — | `0x11E8` | **`0x1208`**（`spawn` `push 0x1208`） |
| `res` 对象尺寸 | — | — | — | **`0x1098`**（vtable `0x4B6540`） |
| 宿主对象尺寸 | `0x574C` | `0x574C` | `0x576C` | **`0x683C`**（vtable `0x4B654C`） |
| 命中点指令 | `movss [edi],xmm0` | `movss [edi],xmm0` | `movss [edi],xmm1` | **`movss [edi],xmm1`** |
| 解释器序言 | `sub esp,0x38` | `sub esp,0x38` | `sub esp,0x38` | **`sub esp,0x40`** |
| opcode 上界 | — | `0x5D` | — | **`0x5E`** |
| 活跃槽数组 | — | `0x11A4` / `0x11A8` | `0x11A4` / `0x11A8` | **`0x11A4` / `0x11C8`** |

> **结论**：**`+0x188` 与 thprac `thprac_th18.cpp:3575` 的 `GetMemAddr(0x4cf2d0, 0x188, 0xc)` 完全一致，需求文档的“本作非 +0x17c”提示成立**（静态证据：宿主构造 `0x42E431` 直接 `mov eax,[ecx+0x188]`，`ecx = *(u32*)0x4CF2D0`）。

---

## 2. Q1 —— `[Edi+4]` = 当前 sub **索引**，`Edi` = thread

### 2.1 判据（读侧）

解释器入口 `FUN_0048B3A0`（`0x48B3A0` / RVA `0x8B3A0`）prologue 立即把 `Edi` 当线程对象，取 `+4`/`+8` 与 `−1` 判等（线程结束），再用 `[Edi+4]` **索引 sub 表**：

```asm
0048b3a0  55                push  ebp
0048b3a1  8bec              mov   ebp, esp
0048b3a3  83e4f8            and   esp, 0xfffffff8
0048b3a6  83ec40            sub   esp, 0x40
0048b3a9  56                push  esi
0048b3aa  57                push  edi
0048b3ab  8bf9              mov   edi, ecx                 ; edi = this = thread
0048b3ad  f30f114c2420      movss [esp+0x20], xmm1          ; 本帧 dt
0048b3b3  8b5708            mov   edx, [edi+8]             ; thread+8 = ip 偏移
0048b3b6  83faff            cmp   edx, -1
0048b3b9  0f8419010000      je    0x48b4d8                 ; ip == -1 → 线程结束
0048b3bf  8b4f04            mov   ecx, [edi+4]             ; ★ thread+4 = sub 索引
0048b3c2  83f9ff            cmp   ecx, -1
0048b3c5  0f840d010000      je    0x48b4d8                 ; subIdx == -1 → 线程结束
0048b3cb  8b8718100000      mov   eax, [edi+0x1018]        ; host = thread+0x1018
0048b3d4  f30f100f          movss xmm1, [edi]                ; time
0048b3d8  8b8018120000      mov   eax, [eax+0x1218]        ;   host+0x1218 = res
0048b3de  8b808c000000      mov   eax, [eax+0x8c]          ;     res+0x8C   = sub 表
0048b3e4  8b44c804          mov   eax, [eax+ecx*8+4]       ;       表项[+4]   = sub 起始
0048b3e8  83c010            add   eax, 0x10
0048b3eb  03c2              add   eax, edx                 ; = 起始 + 0x10 + ip = 当前指令
```

`[Edi+4]` 被直接用作 `[eax+ecx*8+4]` 的下标 ⇒ **索引语义**（取值域 `0..count−1`，`−1` 为结束/失败哨兵）。索引与 `edx`(`ip`) 相加后作绝对地址 ⇒ `[Edi+8]` 是 **sub 内偏移**，不是指针。`+0x10` 跳过的正是 sub 数据头（0x10 字节），与 th16/17 一致。

**交叉印证**：`thprac_th18.cpp:1205-1207` 的 `mainLatterIDs[7] = {0,99,97,0,91,113,0}`（`stage = STAGE_NUM−1`）以 `subID == mainLatterIDs[stage]` 判 MainLatter。这些是小正整数，**只可能是索引**（若 `[Edi+4]` 是地址绝不可能是这些值）；且本作各关 sub 数必 > 113。

### 2.2 字段写入点（谁写 `thread+4`）

| # | VA / RVA | 语义 | 片段 |
| --- | --- | --- | --- |
| 1 | `0x42E43F` / `0x2E43F` | 进关 spawn `"main"`：宿主构造内经 `SetSubByName` 写入 `main` 的索引 | `mov ecx,esi`(host) → `call 0x48D920`（名 = `[ebp+8]` = `"main"`）→ 内部 `0x48D9BB mov [eax+4],esi` |
| 2 | `0x48D9BB` / `0x8D9BB` | `call`(`0x0B`) 按名解析**成功**：`thread+4 = 被调 sub 索引` | `mov eax,[ebp-4]`(host) → `mov eax,[eax+0xc]`(thread) → `mov [eax+4],esi` |
| 3 | `0x48D9A7` / `0x8D9A7` | 按名解析**失败**：`thread+4 = −1` | `mov eax,[eax+0xc]` → `mov dword [eax+4],0xffffffff` |
| 4 | `0x48B474` / `0x8B474` | `ret`(`0x0A`) 弹帧恢复调用者：`thread+4 = 帧[+0x0C]` | `mov eax,[edi+eax+0xc]` → `mov [edi+4],eax` |
| 5 | `0x48B4D1` / `0x8B4D1` | 线程结束：`thread+4 = −1`（与 `0x48B4CA` 的 `thread+8=−1` 配对） | `mov dword [edi+8],0xffffffff` / `mov dword [edi+4],0xffffffff` |
| 6 | `0x48D899` / `0x8D899` | `spawn` 新线程：`thread+4 = −1`（随后由 `call`/`StartSubByName` 填） | `mov dword [esi+8],0xffffffff` / `mov dword [esi+4],0xffffffff` |

> ⚠️ **勿混淆**：`thread+0x11A4`/`thread+0x11C4`（活跃脚本槽数组）在命中点后的 8 元循环里也被当作 **sub 表下标**使用（`0x48C860 mov ecx,[edx-4]`，`edx=thread+0x11C8`），但它**不是**当前 sub 索引。见 §Q5.3。

---

## 3. Q2 —— sub 表：`res+0x8C`，项 8 B，`+0` = 名字、`+4` = 起始、`count = res+8`

### 3.1 完整指针链（本作）

```text
M       = *(u32*)0x4CF2D0                      ; ENEMY_MANAGER 对象        (RVA 0xCF2D0)
res     = *(u32*)(M + 0x188)                   ; ECL 资源对象（0x1098 B, vtable 0x4B6540）
        = *(u32*)(host + 0x1218)               ; 运行期等价链（解释器/宿主构造均用此）
host    = *(u32*)(thread + 0x1018)
count   = *(i32*)(res + 8)
table   = *(u32*)(res + 0x8C)
name    = *(char**)(table + subIdx*8)          ; 表项 +0 = 名字 char*
start   = *(u32*)(table + subIdx*8 + 4)        ; 表项 +4 = sub 起始（首指令 = +4 + 0x10）
```

### 3.2 宿主构造：`mgr+0x188 → res → host+0x1218`（**+0x188 实测确认**）

`FUN_0042E100`（`0x42E100` / RVA `0x2E100`）尾部（`ecx` 已由 `0x42E3B1 mov ecx,[0x4cf2d0]` 置为管理器）：

```asm
0042e3b1  8b0dd0f24c00      mov   ecx, [0x4cf2d0]          ; ecx = ENEMY_MANAGER
...
0042e3fc  8b8190000000      mov   eax, [ecx+0x90]
0042e402  898194000000      mov   [ecx+0x94], eax
0042e408  83c001            add   eax, 1
0042e40b  898190000000      mov   [ecx+0x90], eax          ; 宿主序号自增
0042e411  7507              jne   0x42e41a
0042e413  40                inc   eax
0042e414  898190000000      mov   [ecx+0x90], eax
0042e41a  ff7508            push  dword [ebp+8]              ; 参 = 名（"main"）
0042e431  8b8188010000      mov   eax, [ecx+0x188]         ; ★ res = *(mgr+0x188)   ← 本作偏移
0042e437  8bce              mov   ecx, esi                 ; this = host
0042e439  898618120000      mov   [esi+0x1218], eax        ; ★ host+0x1218 = res
0042e43f  e8dcf40500        call  0x48d920                 ; SetSubByName(host, name) → 写 thread+4
0042e444  8b460c            mov   eax, [esi+0xc]           ; thread = host+0xC
0042e44f  c7400800000000    mov   dword [eax+8], 0         ; thread+8 = 0（ip）
0042e456  8b460c            mov   eax, [esi+0xc]
0042e459  c70000000000      mov   dword [eax], 0           ; thread+0 = 0（time）
0042e45f  8bc6              mov   eax, esi                 ; ★ 返回宿主于 EAX
```

`res` 对象的创建与写回管理器（`FUN_0042DC20`（`0x42DC20` / RVA `0x2DC20`）内）：

```asm
0042dca6  6898100000        push  0x1098                   ; res 尺寸 0x1098
0042dcac  c70640654b00      mov   dword [esi], 0x4b6540    ; res vtable
0042dcb5  89b788010000      mov   [edi+0x188], esi         ; ★ *(mgr+0x188) = res
```

> 展开 `thprac_games.h:575-578` 的 `GetMemAddr(addr,off,...) = *(T*)( *(u32*)addr + off ) …`：
> `GetMemAddr(0x4CF2D0, 0x188, 0xC) = *(u32*)( *(u32*)0x4CF2D0 + 0x188 ) + 0xC = res + 0xC`。
> 与加载器「首个 ECL 文件缓冲 = `res+0xC`」一致（§3.4）——即 `THSectionPatch` 的 `SetBaseAddr` 正是写 ECL 字节的那一级。

### 3.3 建表 / 加载（写侧）—— `FUN_0048D9D0`（SCPT 魔数 `0x54504353`）

```asm
0048d9d0  55                push  ebp
0048d9d1  8bec              mov   ebp, esp
0048d9d3  83ec10            sub   esp, 0x10
0048d9d6  8b4508            mov   eax, [ebp+8]             ; 参 = ECL 文件缓冲
0048d9d9  53                push  ebx
0048d9da  8bd9              mov   ebx, ecx                 ; ebx = res（this）
0048d9df  8b5304            mov   edx, [ebx+4]             ; 文件槽序号
0048d9e2  8944930c          mov   [ebx+edx*4+0xc], eax     ; res+0xC+槽*4 = ECL 文件缓冲
0048d9e6  8b4304            mov   eax, [ebx+4]
0048d9e9  8b4c830c          mov   ecx, [ebx+eax*4+0xc]     ; 缓冲首址
0048d9f0  813953435054      cmp   dword [ecx], 0x54504353  ; ★ 魔数 "SCPT"
0048d9f6  0f85cd010000      jne   0x48dbc9
0048d9fc  6683790401        cmp   word [ecx+4], 1          ; 版本 1
0048da07  0fb74106          movzx eax, word [ecx+6]
0048da0b  03c1              add   eax, ecx                 ; 头尺寸 → 名字区基准
0048da0d  0fb74910          movzx ecx, word [ecx+0x10]     ; 本文件 sub 数
0048da11  014b08            add   [ebx+8], ecx             ; ★ res+8 += sub 数（累计 count）
0048da15  8bb38c000000      mov   esi, [ebx+0x8c]          ; 旧表
0048da1c  8d5024            lea   edx, [eax+0x24]          ; 名字区
0048da28  8b4308            mov   eax, [ebx+8]
0048da2b  c1e003            shl   eax, 3                   ; count*8
0048da32  e8fd3f0000        call  0x491a34                 ; malloc/realloc
0048da3d  89838c000000      mov   [ebx+0x8c], eax          ; ★ res+0x8C = 新表
...
0048da50  8b4304            mov   eax, [ebx+4]
0048da53  8b4c830c          mov   ecx, [ebx+eax*4+0xc]     ; 文件缓冲
0048da5a  0308              add   ecx, [eax]               ; + 段内偏移 = 绝对地址
0048da5c  8b838c000000      mov   eax, [ebx+0x8c]
0048da62  894cd004          mov   [eax+edx*8+4], ecx       ; ★ 表项[+4] = sub 起始（绝对）
0048da71  893cd0            mov   [eax+edx*8], edi         ; ★ 表项[+0] = 名字 char*
0048da74  8a01              mov   al, [ecx]
0048da76  41                inc   ecx
0048da77  84c0              test  al, al
0048da79  75f9              jne   0x48da74                 ; 量名字长度（NUL 结尾串）
```

⇒ **`table = *(res+0x8C)`；项 8 B；`+0`=名字、`+4`=起始；`count = *(res+8)`**；`res+0xC` = 首个 ECL 文件缓冲。
后续 `0x48DA92` 起为「追加到既有表」的合并分支（`esi != 0`，对应多文件共用一个 `res`），与 th16 §3.2 / th17 §2.2 同形。

### 3.4 按名查表 —— `FUN_0048D920`（`SetSubByName(host, name)`，`ecx` = host）

```asm
0048d920  55                push  ebp
0048d92d  8b8018120000      mov   eax, [eax+0x1218]        ; res = host+0x1218
0048d936  8b5808            mov   ebx, [eax+8]             ; ★ count = *(res+8)
0048d939  83eb01            sub   ebx, 1                   ; hi = count-1
0048d93e  8b888c000000      mov   ecx, [eax+0x8c]          ; ★ table = *(res+0x8C)
0048d947  8bc3              mov   eax, ebx                  ; —— 二分查找 ——
0048d949  2bc7              sub   eax, edi
0048d94b  99                cdq
0048d94c  2bc2              sub   eax, edx
0048d951  8bf0              mov   esi, eax
0048d953  d1fe              sar   esi, 1
0048d955  03f7              add   esi, edi                 ; mid
0048d957  8b0cf1            mov   ecx, [ecx+esi*8]         ; ★ 表项[+0] = name char*
0048d960  8a02              mov   al, [edx]
0048d962  3a01              cmp   al, [ecx]                ; —— strcmp(name, table[mid].name) ——
...
0048d9a7  c74004ffffffff    mov   dword [eax+4], 0xffffffff ; 未命中 → thread+4 = -1
0048d9bb  897004            mov   [eax+4], esi             ; ★ 命中 → thread+4 = mid（索引）
0048d9c3  c20400            ret   4
```

⇒ `table[i].+0` 确为 `char*` 名字；`SetSubByName` 的返回值路径即为 **Q1 的索引写入**。

### 3.5 当前指令重解码 —— `FUN_0048DBE0`（再证表项 `+4` 语义）

```asm
0048dbf1  8b8118100000      mov   eax, [ecx+0x1018]        ; host
0048dbf7  8b8018120000      mov   eax, [eax+0x1218]        ; res
0048dbfd  8b808c000000      mov   eax, [eax+0x8c]          ; table
0048dc03  8b44d004          mov   eax, [eax+edx*8+4]       ; 表项[+4]
0048dc07  83c010            add   eax, 0x10
0048dc0a  03c6              add   eax, esi                 ; + ip = 当前指令绝对地址
```

---

## 4. Q3 —— ECL 子调用栈（**与 th15/16/17 逐字段同构**）

### 4.1 结论

1. **有显式可遍历子调用栈**，内嵌在 `thread` 内，随解释器自动维护：
   - **栈指针**：`SP = *(u32*)(thread+0x100C)`（`0` = 空/顶层）；
   - **栈存储**：`thread + 0xC + SP`（dword 数组）；
   - **帧长**：`0x10`；
   - 线程基构造 `0x42CCC0` 直接 `memset(thread+0xC, 0, 0x1008)` 清零整个栈区。
2. **栈顶帧存的是“调用者”**，当前 sub 仍是 `thread+4`。
3. `call`(`0x0B`) 压帧后切到被调 sub；`ret`(`0x0A`) 弹帧恢复调用者。
4. **栈区上限 `0x1000` 字节 ⇒ ≈256 帧**（护栏 `0x48B778`）。

### 4.2 线程基构造 —— `0x42CCC0`（`0x2CCC0`）

```asm
0042ccc0  56                push  esi
0042ccc1  8bf1              mov   esi, ecx
0042ccc3  6808100000        push  0x1008
0042ccc8  6a00              push  0
0042ccca  8d460c            lea   eax, [esi+0xc]           ; 栈区 = thread+0xC
0042cccd  50                push  eax
0042ccce  e81d2c0600        call  0x48f8f0                 ; memset(thread+0xC, 0, 0x1008)
```

### 4.3 `ret`（opcode `0x0A`）—— case @ `0x48B43E` / RVA `0x8B43E`

```asm
0048b43e  83870c100000fc    add   dword [edi+0x100c], -4   ; 与表达式栈配对指针交换（thread+0x1010）
0048b445  8b870c100000      mov   eax, [edi+0x100c]
0048b44b  8b8f10100000      mov   ecx, [edi+0x1010]
0048b451  8b44380c          mov   eax, [eax+edi+0xc]
0048b455  898710100000      mov   [edi+0x1010], eax
0048b45b  898f0c100000      mov   [edi+0x100c], ecx
0048b461  85c9              test  ecx, ecx
0048b463  7465              je    0x48b4ca                 ; 空 → thread+4/8 = -1（结束）
0048b465  8d41fc            lea   eax, [ecx-4]
0048b46a  89870c100000      mov   [edi+0x100c], eax
0048b470  8b44070c          mov   eax, [edi+eax+0xc]       ; 帧[+0x0C] = 调用者 sub
0048b474  894704            mov   [edi+4], eax             ; ★ thread+4 = 调用者 sub
0048b477  83870c100000fc    add   dword [edi+0x100c], -4
0048b47e  8b870c100000      mov   eax, [edi+0x100c]
0048b484  8b44070c          mov   eax, [edi+eax+0xc]
0048b488  894708            mov   [edi+8], eax             ; thread+8 = 旧 ip
0048b48b  83870c100000fc    add   dword [edi+0x100c], -4
0048b492  8b870c100000      mov   eax, [edi+0x100c]
0048b498  8b44070c          mov   eax, [edi+eax+0xc]
0048b49c  8907              mov   [edi], eax               ; thread+0 = 旧 time
0048b49e  83870c100000fc    add   dword [edi+0x100c], -4
0048b4a5  8b870c100000      mov   eax, [edi+0x100c]
0048b4ab  8b44070c          mov   eax, [edi+eax+0xc]
0048b4af  89870c100000      mov   [edi+0x100c], eax        ; thread+0x100C = 旧 SP
```

弹栈顺序 `帧[+0x0C]→thread+4`、`帧[+0x08]→thread+8`、`帧[+0x04]→thread+0`、`帧[+0x00]→thread+0x100C`；**帧 `+0x0C` 即祖先 sub 索引**。
> 注：首段「表达式栈配对指针（`thread+0x1010`）的换回」与 th17 的 `ret` 唯一结构差异同源（th17 §3.3 已注记），不影响语义；弹出次序与压帧严格对称。

### 4.4 `call`（opcode `0x0B`）—— case @ `0x48B66C` / RVA `0x8B66C`

```asm
0048b66c  8b442408          mov   eax, [esp+8]             ; 当前指令
0048b670  51                push  ecx
0048b671  6a00              push  0
0048b673  57                push  edi
0048b674  8bcf              mov   ecx, edi
0048b676  c6400c00          mov   byte [eax+0xc], 0
0048b67a  e8b1f9ffff        call  0x48b030                 ; ★ 压帧助手
0048b67f  85c0              test  eax, eax
0048b681  0f8543feffff      jne   0x48b4ca                 ; 失败 → 线程结束
0048b687  8bcf              mov   ecx, edi
0048b689  e852250000        call  0x48dbe0                 ; 重解码当前指令
0048b68e  89442408          mov   [esp+8], eax
0048b692  e96e110000        jmp   0x48c805
```

压帧助手 `FUN_0048B030`（`0x48B030` / RVA `0x8B030`）关键片段（`ebx` = thread，`edi` = 当前指令）：

```asm
0048b0e0  8a46ff            mov   al, [esi-1]              ; 逐参求值（0x66/0x67 宽/窄）…
...
0048b224  8b8b0c100000      mov   ecx, [ebx+0x100c]        ; 压帧前 SP
0048b22a  85d2              test  edx, edx                  ; edx = 旧 SP
0048b22c  7468              je    0x48b296                 ; SP0 == 0 → 根哨兵分支
0048b22e  83830c100000fc    add   dword [ebx+0x100c], -4
0048b235  8b830c100000      mov   eax, [ebx+0x100c]
0048b23b  8b44180c          mov   eax, [eax+ebx+0xc]
0048b23f  89930c100000      mov   [ebx+0x100c], edx        ; SP = 旧 SP
0048b245  89441a08          mov   [edx+ebx+8], eax
0048b24f  894c030c          mov   [ebx+eax+0xc], ecx       ; 帧[+0x00] = 旧 SP
0048b262  8b07              mov   eax, [edi]               ; thread+0（time）
0048b264  8944190c          mov   [ecx+ebx+0xc], eax       ; 帧[+0x04] = time
0048b277  8b4708            mov   eax, [edi+8]             ; thread+8（ip）
0048b27a  8944190c          mov   [ecx+ebx+0xc], eax       ; 帧[+0x08] = 旧 ip
0048b28d  8b4704            mov   eax, [edi+4]             ; thread+4（调用者 sub）
0048b290  8944190c          mov   [ecx+ebx+0xc], eax       ; ★ 帧[+0x0C] = 调用者 sub
0048b294  eb59              jmp   0x48b2ef
; —— 根哨兵分支（SP0 == 0）——
0048b296  c7830c10000004000000  mov dword [ebx+0x100c], 4
0048b2a6  894c030c          mov   [ebx+eax+0xc], ecx       ; 帧[+0x00] = 0（旧 SP）
0048b2b9  c744180cffffffff  mov   dword [eax+ebx+0xc], 0xffffffff   ; 帧[+0x04] = -1
0048b2d0  c744180cffffffff  mov   dword [eax+ebx+0xc], 0xffffffff   ; 帧[+0x08] = -1
0048b2e7  c744180cffffffff  mov   dword [eax+ebx+0xc], 0xffffffff   ; ★ 帧[+0x0C] = -1（根哨兵）
0048b2ef  83830c10000004    add   dword [ebx+0x100c], 4            ; SP = SP0 + 0x10（帧长 0x10）
; —— 切到被调 sub ——
0048b2f6  8b8718100000      mov   eax, [edi+0x1018]        ; host
0048b2ff  89580c            mov   [eax+0xc], ebx           ; host+0xC = 本 thread
0048b30b  83c314            add   ebx, 0x14                ; 指令+0x14 = 被调名操作数
0048b311  8bce              mov   ecx, esi                 ; this = host
0048b313  53                push  ebx
0048b314  e807260000        call  0x48d920                 ; SetSubByName → thread+4 = 被调索引
0048b319  8b460c            mov   eax, [esi+0xc]
0048b31c  c7400800000000    mov   dword [eax+8], 0         ; thread+8 = 0
0048b326  c70000000000      mov   dword [eax], 0           ; thread+0 = 0
```

⇒ 压帧顺序 `{旧SP, time, 旧ip, 旧subIdx}`；**根哨兵 `−1`**（`main` 首次 `call` 时 `SP0==0`）。与 th15/16/17 完全一致。

### 4.5 栈上限护栏 —— case `0x28` @ `0x48B762` / RVA `0x8B762`

```asm
0048b76b  8b970c100000      mov   edx, [edi+0x100c]        ; 当前 SP
0048b771  8d0c10            lea   ecx, [eax+edx]           ; SP + size
0048b778  81f900100000      cmp   ecx, 0x1000
0048b77e  0f8dae1a0000      jge   0x48d232                 ; 溢出 → 本步不执行
```

⇒ **栈区 `thread+0xC .. thread+0x100C`，字节上限 `0x1000`**（÷ 帧长 `0x10` ⇒ 物理上限 256 帧）。

### 4.6 遍历规则与实现注意

```cpp
// thread 见 §1（在 0x48C822 处 = Edi）
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

- **只应在指令边界（帧间稳定态，如 IGI 渲染）遍历**：表达式临时值/栈局部共用同一 `SP`。
- 空指针/越界保护：`res`/`table`/`count`/`sp`/`caller` 全部校验。

### 4.7 异步 `spawn` 的例外 —— `FUN_0048D850`（`0x48D850` / RVA `0x8D850`）

```asm
0048d856  6808120000        push  0x1208                   ; thread 尺寸 0x1208
0048d85d  e80f040000        call  0x48dc71                 ; operator new
0048d86a  e851f4f9ff        call  0x42ccc0                 ; 线程基构造（清栈）
0048d86f  6a10              push  0x10
0048d873  e8f9030000        call  0x48dc71                 ; 链表节点
0048d886  89be18100000      mov   [esi+0x1018], edi        ; ★ 新线程+0x1018 = 父 host
0048d88c  c70600000000      mov   dword [esi], 0           ; time = 0
0048d892  c74608ffffffff    mov   dword [esi+8], 0xffffffff; ip = -1
0048d899  c74604ffffffff    mov   dword [esi+4], 0xffffffff; ★ subIdx = -1
0048d8a3  8a8920100000      mov   cl, [ecx+0x1020]         ; 继承可见性掩码
0048d8af  8d8f1c120000      lea   ecx, [edi+0x121c]        ; ★ 挂到 host+0x121C 线程链
```

⇒ spawn 出的子线程**自带空栈**，挂到**父 host 的线程链 `host+0x121C`**，**不混入主线的同步调用链**（但与主线同宿主 ⇒ Q4.4 的“`Edi` 是否主线”问题）。

---

## 5. Q4 —— 主线宿主捕获点

### 5.1 宿主工厂 `FUN_0042D7D0`（`0x42D7D0` / RVA `0x2D7D0`）

```asm
0042d7d0  55                push  ebp                        ; __thiscall(this = ENEMY_MANAGER)
0042d7d6  683c680000        push  0x683c                   ; malloc(0x683c) ← 本作宿主尺寸
0042d7dd  e88f040600        call  0x48dc71
0042d7e8  8bc8              mov   ecx, eax                 ; this = 新宿主
0042d7ea  ff7508            push  dword [ebp+8]              ; 参 = 名（"main"）
0042d7ed  e80e090000        call  0x42e100                 ; ★ 宿主构造（§3.2，返回 EAX = 宿主）
```

### 5.2 「spawn `main`」站点（`name = "main"` @ `0x4B6634`）

全库仅 2 处引用字符串 `"main"`，均在关卡进入 / 重开路径：

```asm
; 站点 1 @ 0x444228（进关）
00444208  6a54              push  0x54                     ; 参数块（0x54 B，memset）
0044420a  8d4590            lea   eax, [ebp-0x70]
00444210  e8dbb60400        call  0x48f8f0
00444218  8d4590            lea   eax, [ebp-0x70]
0044421b  51                push  ecx
0044421c  8b0dd0f24c00      mov   ecx, [0x4cf2d0]          ; ecx = ENEMY_MANAGER
00444222  50                push  eax
00444223  6834664b00        push  0x4b6634                 ; "main"
00444228  e8a395feff        call  0x42d7d0                 ; ★ EAX = 宿主
0044422d  e87e66ffff        call  0x43a8b0                 ; ← 建议 EHOOK 点（EAX = 宿主）

; 站点 2 @ 0x444471（重开/重试）
0044445f  8b0dd0f24c00      mov   ecx, [0x4cf2d0]          ; ecx = ENEMY_MANAGER
0044446b  50                push  eax
0044446c  6834664b00        push  0x4b6634                 ; "main"
00444471  e85a93feff        call  0x42d7d0                 ; ★ EAX = 宿主
00444476  e83564ffff        call  0x43a8b0                 ; ← 建议 EHOOK 点（EAX = 宿主）
```

| 项 | 值 |
| --- | --- |
| 进关捕获 | VA `0x44422D`（RVA `0x4422D`）— 命中 `EAX` = 主线宿主 |
| 重开捕获 | VA `0x444476`（RVA `0x44476`）— 命中 `EAX` = 主线宿主 |
| 备选（更直接） | `0x42E45F`（宿主构造尾 `mov eax,esi`）或 `0x42D7D0` 的 `ret`，`EAX` = 宿主，但**对所有敌人脚本宿主生效**，需按 `"main"` 上下文筛选 |
| 宿主 → res | `*(u32*)(host+0x1218)`（写点 `0x42E439`） |
| **主线根 thread** | `host + 0x10`（内嵌，见 §5.3） |
| 宿主线程链头 | `host + 0x121C` |

### 5.3 根线程内嵌于宿主 —— `0x42CE10` / `0x42CE80`

宿主基构造 `0x42CE10`：`lea ecx,[esi+0x10]; call 0x42ccc0`（根线程 = `host+0x10`）。
宿主复位 `0x42CE80` 明确写回：

```asm
0042ce87  8d4110            lea   eax, [ecx+0x10]          ; eax = 根线程 = host+0x10
0042ce8a  c70000000000      mov   dword [eax], 0           ; 根线程+0 = 0（time）
0042ce90  c74118ffffffff    mov   dword [ecx+0x18], 0xffffffff ; 根线程+8 = -1（ip）
0042ce97  c74114ffffffff    mov   dword [ecx+0x14], 0xffffffff ; ★ 根线程+4 = -1（subIdx）
0042cea8  898928100000      mov   [ecx+0x1028], ecx        ; ★ 根线程+0x1018 = host（回指）
0042cf08  89410c            mov   [ecx+0xc], eax           ; ★ host+0xC = 根线程
0042cf1f  89811c120000      mov   [ecx+0x121c], eax        ; ★ host+0x121C = 线程链头 → 根线程
```

### 5.4 命中 `0x48C822` 时 `Edi` 是否恒为主线 thread？→ **否**（与 th16/17 同结论）

宿主步进器 `FUN_0048D420`（`0x48D420` / RVA `0x8D420`）**遍历 `host+0x121C` 的整条线程链**，对每个线程各调一次解释器：

```asm
0048d43c  8db31c120000      lea   esi, [ebx+0x121c]        ; ebx = host，线程链头
0048d450  8b7e04            mov   edi, [esi+4]             ; 下一节点
0048d456  8b0e              mov   ecx, [esi]               ; 线程
0048d458  894b0c            mov   [ebx+0xc], ecx           ; ★ host+0xC = 当前线程
0048d45f  e83cdfffff        call  0x48b3a0                 ; 运行该线程（解释器）
```

`FUN_0048D420` 被宿主更新路径 `0x42ED40` / `0x42FF80` / `0x430240` 调用（关卡内每帧）。

⇒ **`Edi` = 当前正在被步进的 thread**（主线根线程、主线内 `spawn` 的子线程、敌方脚本线程……）；**`Edi` 不保证是主线**。
**实现建议**（二选一）：
1. 捕获主线宿主后 `*(u32*)(Edi+0x1018) == mainHost` 过滤；
2. 直接读 **`mainHost+0xC`**（步进器已写为当前线程）或 **`mainHost+0x10`**（根线程）取当前 sub（最稳，等价于 th14 的 `host+8` 策略）。

> thprac 现有 `0x48C822` 钩子靠 `subID == mainLatterIDs[stage]` + `bossExists` 间接规避该问题，**未做宿主过滤**。

---

## 6. Q5 —— 解释器命中点 `0x48C822` 与寄存器语义

### 6.1 命中点

```asm
0048c80c  f30f100f          movss xmm1, [edi]                ; thread+0 = 当前时间
0048c810  0f5bc0            cvtdq2ps xmm0, xmm0
0048c813  0f2fc8            comiss xmm1, xmm0                ; 与指令自带时间比较
0048c816  0f83f4ebffff      jae   0x48b410                 ; 已到 → 执行指令
0048c81c  f30f584c2420      addss xmm1, [esp+0x20]           ; ★ time += 帧增量 dt
0048c822  f30f110f          movss dword [edi], xmm1          ; ★ EHOOK(4B) 命中：写回 thread+0
0048c826  8d87a4110000      lea   eax, [edi+0x11a4]        ; Eip 恢复目标（跳过整条 4B 指令）
```

- **地址**：`0x48C822` / RVA `0x8C822`；**覆盖 4 字节**（`F3 0F 11 0F`），正好是一条完整指令。
- **命中寄存器**：`Edi = thread`（执行对象）；`thread+0` = 时间；`thread+4` = sub 索引；`thread+0x1018` = host。
- **语义**：当前指令的 `time` 尚未到达（上一轮 `comiss/jae` 分流到此处），把帧增量累加到线程时间上 —— 即 thprac 注释的 “freeze ECL sub time”；`pCtx->Eip = 0x48C826` 即跳过本次时间推进（**与 `thprac_th18.cpp:1202` 的 `pCtx->Eip = 0x48c826` 一致**）。

### 6.2 所在函数与调用链

- **函数**：`FUN_0048B3A0`，入口 `0x48B3A0`（RVA `0x8B3A0`），序言 `push ebp; mov ebp,esp; and esp,-8; sub esp,0x40; push esi; push edi; mov edi,ecx`。
- **opcode 分发**：`0x48B423` 起，取 `word [instr+4]` 的 opcode，越界阈值 `0x5E`，经字节映射表 `0x48D3B8` 转索引，再经跳表 `0x48D2A8` 分发：

```asm
0048b41f  8b442408          mov   eax, [esp+8]             ; 当前指令
0048b423  0fbf4004          movsx eax, word [eax+4]          ; opcode
0048b427  83f85e            cmp   eax, 0x5e
0048b42a  0f87dc1d0000      ja    0x48d20c                 ; 越界 → default
0048b430  0fb680b8d34800    movzx eax, byte [eax+0x48d3b8]   ; 字节映射表
0048b437  ff2485a8d24800    jmp   dword [eax*4+0x48d2a8]   ; 跳表
```

  实测解码（`idx = byte[0x48D3B8+op]`，`target = dword[0x48D2A8+idx*4]`）：
  **opcode `0x0A`(ret) → `0x48B43E`；`0x0B`(call) → `0x48B66C`；`0x0C`(goto) → `0x48B700`；`0x01`(终止) → `0x48B4CA`；`0x0F`(spawn) → `0x48B4E1`；`0x10` → `0x48B530`；`0x15` → `0x48B4F5`；`0x28`(栈局部分配) → `0x48B762`**。
  编号与 th15/16/17 一致。

- **调用链（每帧逐线程）**：
```
宿主更新 (0x42ED40 / 0x42FF80 / 0x430240)
      └─ FUN_0048D420 (host::StepThreads；遍历 host+0x121C 线程链)
              ├─ mov [host+0xC], thread
              └─ call FUN_0048B3A0 (ecx = thread)   ← 0x48D45F / 0x48D46E
                      └─ … 命中点 0x48C822 ←—— EHOOK 4B
```

### 6.3 作为每帧稳定触发点？

- `FUN_0048B3A0` 由宿主步进 `FUN_0048D420` **每帧**对每个活跃线程调用一次；`0x48C822` 位于「指令未到期、推进线程时间」的分支，**在等待定时指令期间每帧命中**（道中/boss 等大量 `ins_23/wait` 场景即如此）。
- **局限**：处于「连续立即指令」时线程走 `0x48B410` 分发分支而不经过 `0x48C822`，故它**不是无条件每帧**；且对**每个 thread** 都命中。
- **结论**：`0x48C822` 可作稳定的“当前 sub 读取点”（读 `Edi+4` / `Edi+0x1018`），与既有实现一致；若要**无条件每帧**触发，建议另挂宿主步进 `0x48D420`（`ebx` = host）或直接由 IGI 帧回调读取 `mainHost+0x10`。

### 6.4 ⚠️ 与 `[Edi+4]` 相邻的“活跃槽”结构 —— 勿混淆

命中点之后 `0x48C826` 起是一个 **8 元活跃脚本槽循环**，**其下标不是 `thread+4`**：

```asm
0048c826  8d87a4110000      lea   eax, [edi+0x11a4]        ; 槽数组 A（8 元）
0048c82c  b908000000        mov   ecx, 8
0048c831  8d97c8110000      lea   edx, [edi+0x11c8]        ; 槽数组 B（8 元）
0048c850  837e2800          cmp   dword [esi+0x28], 0      ; esi = thread+0x1024 + i*0x30
0048c85a  8b8718100000      mov   eax, [edi+0x1018]        ; host
0048c860  8b4afc            mov   ecx, [edx-4]             ; ★ = [edi+0x11C4]（活跃槽下标，非 thread+4）
0048c863  8b8018120000      mov   eax, [eax+0x1218]        ; res
0048c869  8b808c000000      mov   eax, [eax+0x8c]          ; table
0048c86f  8b44c804          mov   eax, [eax+ecx*8+4]       ; table[槽下标].+4
```

⇒ **`thread+4`（当前 sub 索引）与 `thread+0x11C4`（活跃脚本槽下标）是两回事**；取“当前 ECL 函数名”应读 **`*(Edi+4)`**。（同 th16 §6.4 / th17 §5.3。）

---

## 7. Q6 —— 运行期保留 sub 名

✅ **保留**。加载器 `FUN_0048D9D0` 在 `0x48DA71` 把**名字区指针**（`edi`，指向已加载 ECL 缓冲内的名字串）直接填入 `table[i].+0`（见 §3.3）；`0x48DA74`–`0x48DA79` 的 `mov al,[ecx]/test al,al/jne` 即**量名字 NUL 长度**，证明其为运行期常驻的 `char*`。
按名查表 `FUN_0048D920` 亦对 `table[mid].+0` 与目标串**逐字节 `strcmp`**（`0x48D960`–`0x48D982`）。

⇒ **无需离线名表**，`name = *(char**)(table + subIdx*8)` 可直接读。离线兜底（`ECLJump` 偏移）仅在运行时列表不可达时备用。

---

## 8. Q7 —— 机体 / 子机类型（🟡 部分定案）

**已定位**：角色名指针表 + 每角色函数表（静态数据区，`.rdata`）：

| VA / RVA | 内容 |
| --- | --- |
| `0x4B41E0` / `0xB41E0` | 角色名指针表（8 B/项）：`0x4B6DAC "Reimu "`、`0x4B6DB4 "Marisa"`、`0x4B6DBC "Sakuya"`、`0x4B6DC4 "Sanae "`（共 **4 角色**） |
| `0x4B41F4` / `0xB41F4` | 每角色函数表：`0x45F9D0`、`0x460320`、`0x4609C0`（第 4 项未列出） |
| `0x4B41C0` / `0xB41C0` | 难度名指针表：`Easy/Normal/Hard/Lunatic/Extra/O.D.` |

引用 `0x4B41E0` 的 **13 处**站点（`0x457DCE`、`0x457E16`、`0x457EFB`、`0x457F45`、`0x46242B`、`0x468228`、`0x468271`、`0x46830D`、`0x46854C`、`0x468590`、`0x46861F`、`0x46ACCD`、`0x46AF52`）**全部为同一惯用法** `ff 34 85 e0 41 4b 00` = `push dword [eax*4 + 0x4B41E0]`（按索引取角色名），均出现在**名字格式化路径**（回放/截图/记录名等，索引用的是菜单/场景对象字段，非全局 dword）。

**未定案**：**关卡内“当前 character / shotType”全局 dword 地址**。
- `thprac_th18.cpp` **无** `CHARA_ADDR`/`SUBSHOT` 常数（th10/12/14/16 有）；
- 玩家对象构造 `0x45AF20`（`alloc 0x479D0`，写 `[0x4CF410]`）未显式存角色/子机到全局；
- 已确认的既有 `PLAYER_PTR = 0x4CF410`（`docs/th18/th18_re_disasm_report.md` §3）可作“关卡进行中”判据，但**其对象内 character/shotType 字段偏移未定**。

**下一步（真机或进一步逆向）**：
1. 真机在角色选择画面/关卡内分别切 4 角色 × 2 子机，对 `.data`（`0x4C3000`–`0x570000`）做快照差分，定位 `0..3` / `0..1` 的全局；
2. 或反查 `0x45AF20` 的调用者（关卡初始化），看角色/子机从哪个全局/回放头读出。

> **本项不影响 ECL 段落监控（Q1–Q6/Q9）**；仅在 IGI 需要显示机体/子机时补。

---

## 9. Q8 —— 练习跳转补丁（`ECLJump`）不影响 sub 判读

### 9.1 写入内容（`thprac_th18.cpp:2413-2419`）

```cpp
void ECLJump(ECLHelper& ecl, unsigned int start, unsigned int dest, int at_frame, int ecl_time = 0)
{
    if (start) ecl.SetPos(start); else start = ecl.GetPos();
    ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
}
```

展开为 6 个 dword（24 B）：`dword1 = 0x0018000C`：低 16 位 `0x000C` = **`goto`**、高 16 位 `0x0018` = `size`。即只把一条 **`goto`(`opcode 0x0C`)** 覆盖到运行期 ECL 缓冲的**硬编码指令边界**（`start`/`dest` 为离线解码所得偏移）。与 th15/16/17 **完全同形**。

### 9.2 `goto` 语义（case `0x0C` @ `0x48B700` / RVA `0x8B700`）

```asm
0048b700  8b442408          mov   eax, [esp+8]             ; 当前指令
0048b704  660f6e4014        movd  xmm0, [eax+0x14]           ; operand5 = at_frame
0048b709  8d4810            lea   ecx, [eax+0x10]
0048b70c  0f5bc0            cvtdq2ps xmm0, xmm0
0048b70f  f30f1107          movss [edi], xmm0                ; thread+0 = at_frame（绝对赋值）
0048b713  8b01              mov   eax, [ecx]               ; operand4 = dest-start
0048b715  014708            add   [edi+8], eax             ; thread+8 += (dest-start)（ip）
0048b718  8b442408          mov   eax, [esp+8]
0048b71c  0301              add   eax, [ecx]
0048b71e  89442408          mov   [esp+8], eax
0048b722  e9e1100000        jmp   0x48c808
```

**只改 `thread+0`（时间）与 `thread+8`（ip），从不触碰 `thread+4`**，也不触碰 SCPT 头 / 名字区 / `res+0x8C` 表。

### 9.3 sub 结构未动

`ECLHelper::SetBaseAddr((void*)GetMemAddr(0x4CF2D0, 0x188, 0xC))` 定位的是运行期 **ECL 文件缓冲**（`res+0xC`，§3.3）；`ECLJump` 只在硬编码文件偏移处覆写字节，表由 `FUN_0048D9D0` 一次性解析生成（索引 = 名字排序位次）。故 `subIdx ↔ name` 映射稳定。

### 9.4 结论与唯一瞬态

✅ 补丁跳转后 `subIdx` 与名字仍正确。
- **唯一瞬态**：`goto` 之后、目标 `call` 尚未执行的那一瞬（同一指令流、极短），`thread+4` 仍是跳转前所在 sub（通常 `main`）；`call` 执行后即正确。采样落在帧间稳定态即可规避。

> **本作 `THStageWarp` 的离线锚点资产**（`thprac_th18.cpp:2419-2460`，供 Q9 白名单兜底）：
> `st1PostMaple=0xA750`、`st2PostMaple=0x90B4`、`st3PostMaple=0x83A0`、`st4PostMaple=0x5A04`、`st5PostMaple=0x96EC`、`st6PostMaple=0x7E70`、`st7PostMaple=0xA8FC`；ST1 落点示例 `0x6FC8→0x7088 / 0x70BC`、`0xAB90`、`0xAC2C`、`0x7184→0x71F4`。**本机无法把这些偏移运行期解码为 sub 名**（`../exes/ecl_dump/th18/` 素材已在机，可**离线**按文件偏移解码为 sub 名，见 `docs/logger/decompile_materials.md`；运行期仍待真机采集）。

---

## 10. Q9 —— 段落判定口径：**路过式 checkpoint（latch）**

### 10.1 结论

✅ **走「路过式 checkpoint（latch）」**：读当前 sub（th18 的同步调用栈与 th15/16/17 **逐字段同构**，`thread+4` = 当前 sub）；**∈ 白名单则置当前段落；不在 → no-op**（保持上一个值，不回退 `main`）；本局首次命中前显示 `main`。CSV 即白名单表。

> ⚠️ **本处订正**：早期草案写「(A) 调用栈祖先法」，与段内脚本结构不符（原因见 10.2）。

### 10.2 依据与订正原因

1. **栈存在且可遍历**（§Q3）：`call` 压帧、`ret` 弹帧、帧 `+0x0C` = 调用者 sub —— 这是**事实**，但**不用于**本判定。
2. **段入口以 `call` 组织**：主线宿主由 `SetSubByName(host,"main")` 起步（§3.2/§5.2），其后 `@MainFront`/`@MainLatter`/`@MainSubNN` 等子脚本以 `call` 进入。**但一个段落可能跨多个兄弟子脚本**（前一个 `ret` 后才 `call` 下一个），它们**不互为祖先** ⇒ 「段落入口 sub **整段**驻留栈上、可沿栈上溯」的 (A) 前提对**整个段落**不成立（只对单个 sub 成立）。
3. **现成用法佐证**：thprac 现有 `0x48C822` 钩子用 `subID == mainLatterIDs[stage]`（`{0,99,97,0,91,113,0}`）即按**当前 sub** 判定 —— 印证「进入即命中当前 sub」的路过式（latch）口径。
4. ⇒ **每段只登记 1 个入口 sub**（段内其余 sub 不在表内 → no-op）；不需要为段内每个 sub 建条目。

### 10.3 判定流程

```cpp
host   = mEclHost;                            // 0x44422D / 0x444476 捕获（EAX）
thread = (host == 0) ? 0 : *(u32*)(host + 0x10);   // 根线程（最稳）；或 *(u32*)(host+0xC)
res    = *(u32*)(host + 0x1218);              // = *(u32*)(*(u32*)0x4CF2D0 + 0x188)
count  = *(i32*)(res + 8);
table  = *(u32*)(res + 0x8C);

idx  = *(i32*)(thread + 4);                   // 当前 sub
name = NameOf(table, idx, count);             // 越界保护
section = Whitelist(game, stage, name);       // 命中 → 置当前段落
if (section) current = section;               // 未命中 → no-op（不回退 main）
```

> checkpoint 粒度 = 每行 1 个入口 sub（= warp 落点所进入的 sub，**含 async 落点**）；容器 `main`/`MainFront`/`MainLatter` 不入表。

### 10.4 ⚠️ 未定论项（段落入口白名单待枚举）

- th18 的段落入口 `MainSubNN` 名单与 `THStageWarp` 偏移（§9.4）一一对应的入口表**尚未枚举**（`../exes/ecl_dump/th18/` 的 `.decl` 素材已在机，可离线枚举；本作无 eclmap，指令为 `ins_*`，不影响 sub 名）（可用现有 `mainLatterIDs` 的 99/97/91/113 作为首批已知点交叉验证）。
- 本作**非分支关**（总括 §4 点名的分支关为 th11 st4 / th14 st4 / th16 st5），但读的是**运行期状态**，分支关亦天然兼容，无需特判。

---

## 11. 真机验证清单（补充 requests §10）

- [ ] **当前 sub**：道中/boss/不同 sub 采样，`NameOf(*(thread+4))` 与实际一致（用现有 `mainLatterIDs` 的 99/97/91/113 交叉印证）。
- [ ] **sub 表链（本作重点）**：`res = *(u32*)(*(u32*)0x4CF2D0 + 0x188)` 非空、`res+0x8C` 表可读、`count = *(res+8)` 合理、索引越界保护生效。**确认 `+0x188`（非 `+0x17C`）**。
- [ ] **宿主捕获**：在 `0x44422D` / `0x444476` 命中，`EAX` 指向的对象 `+0x1218` 即 `res`、`+0x11FC`…（本作 `+0x121C`）线程链非空、`+0xC` = `+0x10`（根线程）。
- [ ] **是否主线**：确认 `0x48C822` 命中时 `Edi` 可能非主线（如 `spawn` 子线程/敌方线程）；用 `*(Edi+0x1018)==mainHost` 过滤，或改读 `mainHost+0x10`。
- [ ] **路过置值**：经过各段入口 sub 时当前段落被正确置值；段内其余 sub 不改变当前段落（no-op）。
- [ ] **异步落点**：含 `spawn`/async 落点的段落，async 子能否被观测到并置值。
- [ ] 采样时机置于**帧间稳定态**（IGI 渲染），避免表达式临时栈占用 `SP` 时误读。
- [ ] **练习跳章后**：`thread+4` 名字与白名单置值均正确（Q8 复核）；抽验 §9.4 的 `ECLJump` 落点是否命中 `call MainSubNN`。
- [ ] **角色/子机（Q7，可选）**：真机切 4 角色 × 2 子机，对 `.data` 做差分定位全局。
- [ ] **地区/版本变体**：Steam / 汉化版是否共用同一 RVA，按变体实测。

---

## 12. 对总括 §5 与 `th18_ecl_re_requests` 的回填建议

> 本节仅为建议文本，**未改动**任何既有文件（避免与并行任务写冲突）。

1. **总括 `logger_re_disasm_overview.md` §5.1 th18 行**：
   - 「已有 ECL 解释器命中点」备注补：命中点 `0x48C822`(4B) = `movss [edi],xmm1`，所在函数 `FUN_0048B3A0`。
   - 「命中点已知字段」由 `[Edi+4]`=subID 升级为：**`[Edi+4]` = 当前 sub 索引（✅ 已定案）**；`[Edi+0x1018]` = host。
   - 「ECL 数据区链」补确认：`GetMemAddr(0x4cf2d0, 0x188, 0xc)` = `res+0xC`，**`res = *(u32*)(*(u32*)0x4cf2d0+0x188)` = `*(u32*)(host+0x1218)`**（✅ 实测，本作非 `+0x17c`）。
   - 「sub 名/索引线索」由 🟡 升级为 **✅ 有 sub 表 + 有名字**（`table = *(res+0x8C)`，项 8 B，`+0`=名字 `char*`）。
2. **§5.1 注记「需静态厘清的首要矛盾（`[执行对象+4]` 语义）」**：th18 归入 **“索引”** 阵营（与 th15/16/17 同），th18 的 `Edi` = th15 意义的 `thread`；**th14 是唯一异类**（`node+4` = 绝对 ip，其注释正确）——该矛盾在 th14 报告中已定案，**无需再列为“待厘清”**。
3. **总括 §5.2 th18 行**（接线锚点）：无需改动（miss/bomb/game_start 与本次一致）；可加注「Q1–Q9 见 `docs/th18/th18_ecl_re_report.md`」。
4. **`th18_ecl_re_requests.md`**：
   - §1 表「ECL 数据区链」补 ✅ 实测结论（`+0x188` 成立）。
   - §2 由「近似已答，待确认」→ **✅ 已确认**（附 §2 写入点表）。
   - §3 由「待确认」→ **✅ 已确认**（附 §3 加载器/查表证据）。
   - §4/§5/§6/§7/§9 分别替换为本文 §4/§5/§6/§7/§10 结论。
   - §10 真机验证清单替换为本文 §11。
   - §13 变更记录追加一行：「2026-10-14 逆向回填：参见 `th18_ecl_re_report.md`；Q1–Q9 静态定案（Q7 🟡 部分）。」

---

## 13. 未定论 / 风险

1. **段落入口白名单**（决定显示内容，非决定口径）：th18 `MainSubNN` 名单与 `THStageWarp` 偏移↔名字映射**尚未建立**；`../exes/ecl_dump/th18/` 的 `.decl` 素材已在机，可**离线**枚举（本机仍不能运行，运行期采集待真机）。
2. **`Edi` 是否主线**（§5.4）：`0x48C822` 对任意 thread 命中。现有实现靠 `subID==mainLatterIDs[stage]` + `bossExists` 间接规避；若要更稳，建议改读 `mainHost+0x10`（根线程）。
3. **`res+0xC` 的双重身份**：加载器把 `res+0xC+idx*4` 当“ECL 文件缓冲指针数组”，而 `GetMemAddr(0x4cf2d0,0x188,0xc)`（= `res+0xC`）又是 `ECLJump` 的写入基址。二者因 ECL 数据自 `res+0xC` 起连续布局而自洽（本机不能运行；`.decl` 素材已在机，可离线核对，未实证）。**不影响 Q1–Q9 其余结论**（表地址链已由 `0x42E439` / `0x48DA3D` / `0x48D920` 独立确证）。
4. **Q7 机体/子机全局地址未定**（§8）。
5. **真机三态未做**：本机无法运行 th18，全部结论为**静态定案**；`spawn` 子线程是否会短暂占据 `host+0xC` 等运行时行为需真机确认。
6. **地区/版本变体**：Steam / 汉化版是否共用同一 RVA，需按变体实测。

---

## 14. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档。静态反汇编 th18 ECL VM，回答 Q1–Q9：**Q1 ✅** `*(Edi+4)` = sub **索引**（`Edi`=thread；写点 `0x42E43F` 初始化 `main`、`0x48D9BB` call 成功、`0x48D9A7` 失败→−1、`0x48B474` ret 恢复、`0x48B4D1` 结束→−1、`0x48D899` spawn→−1）；**Q2 ✅** `res = *(u32*)(*(u32*)0x4CF2D0+0x188) = *(u32*)(host+0x1218)`（**本作 `+0x188` 实测成立**）、`table = *(res+0x8C)`、`count = *(res+8)`、项 8 B（`+0` 名 / `+4` 始），加载器 `FUN_0048D9D0`（SCPT `0x54504353`）、按名查表 `FUN_0048D920`、重解码 `FUN_0048DBE0`；**Q3 ✅** 显式调用栈（`SP=thread+0x100C`、底 `thread+0xC`、帧 `0x10`、帧 `+0x0C`=调用者、上限 `0x1000` 护栏 `0x48B778`；`call` `0x48B66C`→压帧 `FUN_0048B030`、`ret` `0x48B43E`；根哨兵 −1；`spawn` `FUN_0048D850` 另起 0x1208 线程挂 `host+0x121C`）；**Q4 ✅** 宿主工厂 `FUN_0042D7D0`（`alloc 0x683c`）、宿主构造 `FUN_0042E100`、`"main"`@`0x4B6634` spawn 站点 `0x444228`/`0x444471`、捕获 `0x44422D`/`0x444476`（EAX=宿主）；根线程 = `host+0x10`（`0x42CE87`/`0x42CF08`）；**Q5 ✅** 命中点 `0x48C822`=`movss [edi],xmm1`(4B `f3 0f 11 0f`)、函数 `FUN_0048B3A0`(0x48B3A0)、opcode 表 `0x48D2A8`/`0x48D3B8`（`0x0A`/`0x0B`/`0x0C` 映射与 th15–17 一致）、步进器 `FUN_0048D420`；**Q6 ✅** 运行期保留名字；**Q7 🟡** 角色名表 `0x4B41E0`（Reimu/Marisa/Sakuya/Sanae）+ 角色函数表 `0x4B41F4/…`，**关卡内 character/shotType 全局未定位**；**Q8 ✅** `ECLJump` 仅写 `goto`(`0x0C`)，`0x48B700` 只改 `thread+0`/`thread+8`；**Q9 ✅ 定案 (A) 调用栈祖先法**〔**后于 2026-10-15 订正为「路过式 checkpoint（latch）」**〕。证据：`capstone` 只读反汇编（`../../reasm/th18/reth.py`）+ th15/16/17 逐字段对照。**未真机三态；段落入口白名单待枚举。** |
| 2026-10-15 | 素材来源登记：`../exes/ecl/th18/`（`.ecl` 22 份）+ `../exes/ecl_dump/th18/`（`.decl` 22 份；本作无 eclmap，指令 `ins_*`）已在机；据此把「本机无 th18 `.ecl` / 无法离线解码」订正为「`.decl` 素材已在机、段落入口白名单待枚举」。清单见 `docs/logger/decompile_materials.md`。 |
| 2026-10-15 | **订正 Q9 口径**：由「(A) 调用栈祖先法」改为「**路过式 checkpoint（latch）**」——经过白名单函数即置值、不在表内 no-op（不回退 `main`）。原因：一个段落可能跨多个兄弟 `call`，不互为祖先，「整段驻留栈上」不成立 → 每 checkpoint 只需登记 1 个入口 sub（含 async）。§0 Q9 行、§10、§11 同步改写。 |

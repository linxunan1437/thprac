# TH11 ECL 段落监控 — 逆向结论（Q1–Q9）

> 对应需求：`docs/th11/th11_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`；结构模板（同代 B 代参照）：`docs/th13/th13_ecl_re_report.md`。
> 二进制：`../exes/th11.exe`，688128 B，PE `TimeDateStamp = 1217734394`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**）。`.text` = VA `0x401000`–`0x48AE15`，RVA `0x1000`–`0x8AE15`，`SizeOfRawData = 0x8A000`。
> 引擎代：**B 代**；同代参照：th10 / th12 / th13（th13 已有报告，其余仅 requests）。
> 方法：`pefile` + `capstone 5.0.7` 只读反汇编（自写脚本 `D:\workshop\reasm\th11\dl.py`，由 `D:\workshop\reasm\th13\dl.py` 改造），用「原始字节搜索 + 局部对齐 + 函数入口整段反汇编」定位（避开 `.text` 线性反汇编失步）。
> ECL / decl 素材：`../exes/ecl/th11/`（原始 `.ecl` 20 份）、`../exes/ecl_dump/th11/`（`thecl -d` 反编译 `.decl` 20 份），清单与来源见 `docs/logger/decompile_materials.md`。
> 素材已在机，故「名字 ↔ 段落入口」**可离线枚举**（枚举本身仍待做，见 §10.4）。
> 状态：**静态定案（🟡 已定案、未真机三态）**。本机未运行 th11，**未做任何真机验证**。
> 地址口径：正文以 **VA** 为主，并标注 **RVA = VA − 0x400000**。
> 置信度图例：✅ 反汇编直接证实 ｜ 🟡 有强证据但缺一环（显式标注缺口）｜ ❌ 未能定案。
> ⚠️ 本作 **stage 4 存在路线分支**（`stage4c00{a,b,c}.ecl` / `stage4c01{a,b,c}.ecl`），见 §9.4 / §10.5 / §13。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前 sub = 索引？指令指针？** | ✅ **是「指令地址（指针）」**，**不是 sub 索引** | `ip = *(u32*)(T+0x04)`（T = 线程/脚本上下文）；指向运行期 ECL 缓冲内的**当前（下一条待执行）指令绝对地址**；`0` = 线程结束。`subIdx` 这一层**在本作不存在**。**与 th13 同构**，与 th15–18 的 `thread+4 = 索引` **不同**。 |
| **Q2 sub 表** | ✅ **确认（与 th13 逐字段一致）** | `mgr = *(u32*)0x4A8D7C`；`res = *(u32*)(mgr+0x64)`；`count = *(i32*)(res+0x08)`；`table = *(u32*)(res+0x8C)`；项 **8B**，`+0` = 名字 `char*`、`+4` = sub 数据基址（**首指令 = `+4 + 0x10`**）。加载器 `FUN_0045D900`（SCPT 魔数 `0x54504353`，版本 1）；查名 `FUN_0045DB10`（按名二分）。 |
| **Q3 函数栈** | 🟡 **存在显式栈（骨架确认，遍历步长未完全定案）** | 栈基址 `T+0x08`，栈指针 `SP = *(u32*)(T+0x1008)`（字节偏移，`0` = 顶层）；`ret`(0x0A) 以固定 **8B** 回退并恢复 `{调用者 ip, 时间}`；压帧 = `FUN_0045B610`。⚠️ **实参与帧同处一段区间**，逐帧上溯的精确步长未定案（见 §4.5）。 |
| **Q4 主线宿主捕获点** | ✅ **给出捕获点** | `FUN_004108F0(name)` 构造并返回宿主；**返回 `EAX` = 宿主 H**（`0x410AD8: mov eax,ebp`）。「spawn main」调用点 = **`0x4204A3`** 与 **`0x4205E0`**（其前一条 `call 0x4108F0` 分别在 `0x42049E` / `0x4205DB`，`"main"` @ `0x494258`）。静态兜底：`H = *(u32*)( *(u32*)(mgr+0x68) )`（宿主链表头节点，节点 `H+0x11A4`，`+0` = H）。 |
| **Q5 解释器命中点** | ✅ **确认（并在 requests 的基础上更正语义）** | 解释器 = **`FUN_0045B860`**（入口 `0x45B860`/RVA `0x5B860`，`EAX` = 线程 T，栈参 = 帧增量 float）；**分发头 = `0x45B890`**（`mov esi,[edi+4]` / `mov al,[esi+0xa]`），opcode 跳表 `0x45D2C8` / 字节映射 `0x45D3BC`（阈值 `0x58`）。既有 hook **`0x45D2A3`**（2B `D9 1F`）位于「时间未到 → 累加并返回」尾部，命中寄存器 **`Edi` = 线程 T**。 |
| **Q6 名字来源** | ✅ **运行期保留名字** | `table[i].+0` = `char*`，指向**已加载 ECL 缓冲内的名字串**（加载器 `FUN_0045D900` 在 `0x45D99E`/`0x45DA97` 填入）。但因 Q1 是地址语义，取「当前 sub 名」需**地址 → sub 反查**（用 `table[i].+4+0x10` 建区间表）。 |
| **Q7 机体 / 子机** | 🟡 **给出现有地址（沿用 thprac，未真机核对）** | `mainShot = *(u32*)(*(u32*)0x4A8ECC + 0x38)`、`subShot = *(u32*)(*(u32*)0x4A8ECC + 0x3C)`（`thprac_th11.cpp:175-176` 既有用法）。难度 `*(i32*)0x4A5720`。 |
| **Q8 ECLJump** | ✅ **不影响判读（分支关亦成立）** | `ECLJump` 只把 `goto`(`opcode 0x0C`) 写进**选定槽位**的运行期缓冲；`goto`(case `0x0C` @ `0x45BAC2`) **只改 `T+0`(时间) 与 `T+4`(ip)**。⚠️ 分支关下偏移是**相对该槽位文件**的，故 st4 boss 段 thprac 会先 `ecl.SetFile(2/3/…)`；对**监控**无影响。 |
| **Q9 段落判定口径** | ✅ **路过式 checkpoint（latch）** | CSV 即白名单：当前 sub（经 Q2 地址区间表把 `T+4` 映射回名字）∈ 表 → 置当前段落；**不在 → no-op**（不回退 `main`）。**每 checkpoint 只登记 1 个入口函数**（含 async 落点）。⚠️ **段落入口白名单尚未枚举**（`../exes/ecl_dump/th11/` 素材已在机，可离线枚举）。⚠️ **本作大量入口是 `async` 落点**，实现需考虑线程选择（见 §10.5）。 |

> **关键地址（VA / RVA）**：
> ECL 主解释器 `FUN_0045B860` = `0x45B860`/`0x5B860`（体 `0x45B860`–`0x45D2C4`）；分发头 `0x45B890`/`0x5B890`；
> 逐指令推进 `0x45D281`/`0x5D281`；时间判定/累加 `0x45D294`–`0x45D2A3`；**既有 hook `0x45D2A3`/`0x5D2A3`**（2B `D9 1F`）；
> `ret`(case `0x0A`) `0x45B8BD`/`0x5B8BD`；`call`(case `0x0B`) `0x45BA10`/`0x5BA10`；`goto`(case `0x0C`) `0x45BAC2`/`0x5BAC2`；`async`(case `0x0F`) `0x45B912`/`0x5B912`；
> opcode 跳表 `0x45D2C8`；字节映射 `0x45D3BC`；`>0x58` 委派点 `0x45D269`；线程结束 `0x45D2B4`；
> 压帧 `FUN_0045B610`/`0x5B610`；按名查表 `FUN_0045DB10`/`0x5DB10`；async 起线程 `FUN_0045D7A0`/`0x5D7A0`；
> SCPT 加载器 `FUN_0045D900`/`0x5D900`；线程驱动器 `FUN_0045D420`/`0x5D420`（调用点 `0x411ABB`/`0x411D43`/`0x438B06`）；
> 线程/宿主「复位」`FUN_0040FED0`/`0xFED0`；宿主构造 `FUN_004111C0`/`0x111C0`（`0x411392` 起读 res、`0x4113BB` 存 `H+0x102C`、`0x4113C9` 设 `T+4`）；
> 宿主工厂（spawn by name）`FUN_004108F0`/`0x108F0`；`"main"` @ `0x494258`/`0x94258`；spawn 站点 `0x4204A3`/`0x204A3`、`0x4205E0`/`0x205E0`；
> 访问器 `GetSubName 0x40D070`、`GetResCount 0x40D0A0`、`GetRes 0x40D0B0`、`GetRes2 0x40D090`、`GetTable 0x4106D0`、`GetHostListHead 0x410540`。

---

## 1. 统一指针链（TH11 定案）

```text
mgr   = *(u32*)0x4A8D7C                              // 全局 ECL 管理器（= thprac 的 ENEMY_MGR_PTR）
res   = *(u32*)(mgr + 0x64)                          // ECL 资源对象（GetRes @0x40D0B0）
count = *(i32*)(res + 0x08)                          // sub 总数（GetResCount @0x40D0A0）
table = *(u32*)(res + 0x8C)                          // sub 表（GetTable @0x4106D0；项 8B）
name_i  = *(char**)(table + i*8)                     // 项 +0 = 名字 char*
start_i = *(u32*)(table + i*8 + 4) + 0x10            // 项 +4 = sub 数据基址 ⇒ 首指令 = +4+0x10
buf_slot = *(u32*)(res + 0xC + slot*4)               // 已加载 ECL 文件缓冲数组（槽号 = res+4）

// ── 主线宿主（进关时由 0x4204A3 / 0x4205E0 抓到 EAX）────────────────────
H     = 主线 ECL 宿主对象（malloc 0x2678；vtable 0x494074）
H+0x000  = vtable（主线宿主 = 0x494074）
H+0x004  = T（当前线程；= H+8，见 0x4111FD / 0x45D495）
H+0x008  = T 本体（内嵌线程；H+8..H+0x103C 区域）
H+0x102C = res                                       // ★ 宿主持有的 ECL 资源（0x4113BB 写入）
H+0x1030 = 线程链表头节点 { +0 = 线程指针, +4 = next, +8 = prev }（首个节点的线程 = H+8 ★主线线程）
H+0x11A4 = 宿主链表节点 { +0 = H, +4 = next, +8 = prev }（链表头/尾 = mgr+0x68 / mgr+0x6C）
H+0x25BC = u32 flags（主线标记值 = 20512 = 0x5020）★既有的「主线判据」就读这里

// ── 线程 T（T = H+8；亦可为 async 新线程对象，malloc 0x103C / vtable 0x494058）──
T+0x000 = float 当前 ECL 时间（跨帧累积）
T+0x004 = u32  当前（下一条待执行）ECL 指令的绝对地址；0 = 线程结束  ★Q1
T+0x008 = u32  调用栈基址（帧区起点）
T+0x1008 = i32 调用栈指针 SP（字节偏移，0 = 空栈）
T+0x100C = i32 局部/表达式栈指针（与 0x1008 共用基址 T+8）
T+0x1014 = u32 宿主 H（自指针；宿主/线程复位时写 `[T+0x1014] = H`）★既有主线判据的取址基
T+0x101C = u8  指令选择掩码（难易度位掩码；供 `test byte [T+0x101C], ins[0xA]`）
```

**指令头布局**（由 §2.1 指令流 + `.ecl` 实体推出）：`+0x00` = int32 时间 ｜ `+0x04` = u16 opcode ｜ `+0x06` = u16 size ｜ `+0x08` = u16 浮点参数掩码 ｜ `+0x0A` = u8 选择掩码（难易度） ｜ `+0x0B` = u8 **实参个数** ｜ `+0x0C` = u8 局部槽数 ｜ `+0x0D..0x0F` 填充 ｜ **`+0x10` 起 = 参数（dword）**。
> 与 `thprac_th11.cpp:992` 的 `ECLJump` 写出的 6 个 dword（`ecl_time`、`0x0018000C`、`0x02FF0000`、`0`、`dest-start`、`delay`）**完全吻合**（`size = 0x18`，`ins[0xA] = 0xFF` 任意难度）。
> 另：**`call`/`async` 的目标以「名字串」内嵌在指令 `+0x14` 起**（见 §2.4 / §5.2），这是本作与 th13 的一处**结构差异**（th13 报告未记录该形态）。

**THSectionPatch 的展开（与 thprac `ECLHelper` 对齐）**

`thprac_games.h:653-661`：`SetBaseAddr(addr){ mPtrToBuffer = addr; VFile::SetFile((uint8_t*)(*(uint32_t*)addr), …); }`、`SetFile(ord){ VFile::SetFile((uint8_t*)(*(uint32_t*)(mPtrToBuffer + ord*4)), …); }`。
`GetMemAddr(0x4A8D7C, 0x64, 0xC) = *(u32*)(*(u32*)(0x4A8D7C)+0x64) + 0xC = res + 0xC` ⇒ **`res+0xC` 正是加载器用的 `res+0xC+slot*4` 数组首址**（§3.2），与 th13 §3.4 **完全一致**。`THSectionPatch` 的 `SetBaseAddr` 一级对象就是 `res`。

**与 th13 的差异对照（同代同构、偏移不同）**

| 项 | th13（B 代） | **th11（本作）** |
| --- | --- | --- |
| 管理器全局 | `0x4C2188` | **`0x4A8D7C`** |
| `res` | `*(mgr+0xAC)` | **`*(mgr+0x64)`** |
| `count` / `table` | `res+8` / `res+0x8C` | **完全一致** ✅ |
| 表项 | 8B，`+0` 名字 `+4` 起始 | **完全一致** ✅ |
| 首指令 | `+4+0x10` | **完全一致** ✅ |
| 宿主 → 线程 | `H+0x11D0`（≡ `H+4`）= T | **`H+0x1030`（节点 `+0`）= T；`H+4` = T** |
| 宿主 → res | `H+0x11CC` | **`H+0x102C`** |
| 当前 sub | `T+4` = 指令地址 | **一致** ✅ |
| 调用栈基址 / SP | `T+8` / `T+0x1008` | **一致** ✅ |
| 帧长 | `0x0C`（含旧 SP） | **≈`0x08`**（`ret` 固定回退 8B，**不含旧 SP**） |
| opcode 阈值 | `0x5D` | **`0x58`** |
| 跳表 / 映射 | `0x478C38` / `0x478D44` | **`0x45D2C8` / `0x45D3BC`** |
| 子调用寻址 | `ins[+0x10]`（偏移） | **指令内嵌「名字串」+ 按名二分表** |
| 解释器入口 | `FUN_004772A0` | **`FUN_0045B860`** |

---

## 2. Q1 —— `[T+4]` 是**指令地址（指针）**，不是 sub 索引

### 2.1 判据（分发头把 `[T+4]` 当指针用）

```asm
0045b860  81ec20010000      sub   esp, 0x120                 ; ★ 解释器入口（EAX = 线程 T，栈参 = 帧增量）
0045b866  53                push  ebx
0045b867  55                push  ebp
0045b868  56                push  esi
0045b869  57                push  edi
0045b86a  8bf8              mov   edi, eax                   ; edi = T（脚本上下文）
0045b86c  8b4704            mov   eax, dword ptr [edi + 4]   ; eax = T+4 = 当前指令地址
0045b86f  33db              xor   ebx, ebx
0045b871  3bc3              cmp   eax, ebx
0045b873  0f843e1a0000      je    0x45d2b7                   ; ip == 0 → 线程结束
0045b879  db00              fild  dword ptr [eax]              ; 指令的「时间」= int32
0045b87b  d907              fld   dword ptr [edi]            ; T+0 = 当前时间(float)
0045b87d  ded9              fcompp
0045b87f  dfe0              fnstsw ax
0045b881  f6c401            test  ah, 1
0045b884  0f85101a0000      jne   0x45d29a                   ; 时间未到 → 累加时间后返回
0045b88a  8d6b01            lea   ebp, [ebx + 1]
0045b890  8b7704            mov   esi, dword ptr [edi + 4]   ; ★ 分发头：重新取当前指令
0045b893  8a460a            mov   al, byte ptr [esi + 0xa]   ; 指令 +0xA = 选择掩码字节
0045b896  84871c100000      test  byte ptr [edi + 0x101c], al
0045b89c  0f84d8190000      je    0x45d27a                   ; 掩码不匹配 → 跳过该指令
0045b8a2  0fbf4604          movsx eax, word ptr [esi + 4]      ; opcode（16 位）
0045b8a6  83f858            cmp   eax, 0x58
0045b8a9  0f87ba190000      ja    0x45d269                   ; > 0x58 → 委派给 [T+0x1014] 的虚方法
0045b8af  0fb688bcd34500    movzx ecx, byte ptr [eax + 0x45d3bc]  ; 字节映射表
0045b8b6  ff248dc8d24500    jmp   dword ptr [ecx*4 + 0x45d2c8]  ; 跳表分发
```

`[EDI+4]` 被当作**指针**：读 `[ins+0]`(int 时间)、`[ins+4]`(opcode)、`[ins+6]`(size)、`[ins+0xA]`(掩码) —— 它**绝不可能是索引或偏移**。且**本作不存在「当前 sub 索引」这一层**：解释器对 `[T+4]` 的所有写入都是「绝对地址」形态。

### 2.2 循环骨架（逐指令推进 → 时间判定 → 执行/累加）

```asm
0045d27a  8b4704            mov   eax, dword ptr [edi + 4]   ; 当前指令
0045d27d  0fb74806          movzx ecx, word ptr [eax + 6]      ; ins.size
0045d281  03c8              add   ecx, eax
0045d283  894f04            mov   dword ptr [edi + 4], ecx   ; ★ T+4 = 下一条指令（推进）
0045d286  8b5704            mov   edx, dword ptr [edi + 4]
0045d289  db02              fild  dword ptr [edx]              ; 下一条指令的时间
0045d28b  d907              fld   dword ptr [edi]
0045d28d  ded9              fcompp
0045d28f  dfe0              fnstsw ax
0045d291  f6c401            test  ah, 1
0045d294  0f84f6e5ffff      je    0x45b890                   ; 时间已到 → 执行
0045d29a  d907              fld   dword ptr [edi]            ; 时间未到 → 累加
0045d29c  d8842434010000    fadd  dword ptr [esp + 0x134]      ; + 帧增量（本函数唯一栈参）
0045d2a3  d91f              fstp  dword ptr [edi]              ; ★★ 既有 EHOOK(2B) 命中点：回写 T+0
0045d2a5  33c0              xor   eax, eax                   ; 返回 0 = 本帧到此为止
0045d2a7  5f                pop   edi
0045d2a8  5e                pop   esi
0045d2a9  5d                pop   ebp
0045d2aa  5b                pop   ebx
0045d2ab  81c420010000      add   esp, 0x120
0045d2b1  c20400            ret   4
0045d2b4  895f04            mov   dword ptr [edi + 4], ebx   ; ★ 线程结束：T+4 = 0
0045d2b7  5f                pop   edi
0045d2b8  5e                pop   esi
0045d2b9  5d                pop   ebp
0045d2ba  83c8ff            or    eax, 0xffffffff            ; 返回 −1 = 脚本结束
0045d2bd  5b                pop   ebx
0045d2be  81c420010000      add   esp, 0x120
0045d2c4  c20400            ret   4
```

⇒ **T+4 = 「下一条待执行指令」的绝对地址**；执行时由分发头读取 → 语义上即「当前指令地址」。执行完一条后回到 `0x45D27A` 再推进。

### 2.3 `T+4` 的写入点一览（全部是「绝对地址」）

| VA / RVA | 语义 | 片段 |
| --- | --- | --- |
| **`0x4113C9`** / `0x113C9` | **进关/构造宿主时设主线 sub**：`T+4 = FindSubByName(res, name)`（= 该 sub 首指令） | `mov edx,[edi+4]` → `mov [edx+4],eax`（eax 来自 `call 0x45DB10`，name 来自 ctor 入参） |
| `0x40FEE1` / `0xFEE1` | **宿主/线程复位**：`H+0xC = 0`（即 `T+4 = 0`，T = H+8）；同时 `H+4 = H+8`、`H+0x101C = H` | `mov [eax+0xc],edx(0)` |
| `0x45B814` / `0x5B814` | **`call`(0x0B) 换 sub**：`T+4 = FindSubByName(res, ins+0x14)` | `mov ecx,[esi+4]` → `mov [ecx+4],eax` |
| `0x45DB??`（`FUN_0045DB90`/`FUN_0045DBE0` 内） | 独立线程对象的「设初 sub」同型写入 | `mov [esi+0x102c],edi` → `call 0x45DB10` → `mov [ecx+4],eax` |
| **`0x45D283`** / `0x5D283` | **逐指令推进**：`T+4 = T+4 + ins.size` | `movzx ecx,word[eax+6]` → `add ecx,eax` → `mov [edi+4],ecx` |
| `0x45B8E9` / `0x5B8E9` | **`ret` 恢复调用者 ip** | `mov edx,[esi+eax]` → `mov [edi+4],edx` |
| `0x45BAC2`+`0x45D283` / `0x5BAC2` | **`goto` 重定位**：`T+4 = ins + ins[+0x10]` | `mov ecx,[eax+0x10]` → `jmp 0x45d281` |
| **`0x45D2B4`** / `0x5D2B4` | **线程结束**：`T+4 = 0` | `mov dword ptr [edi+4], ebx`（ebx = 0） |

### 2.4 子调用**按名字**寻址（本作特点，与 th13 不同）

`call`(0x0B) 与 `async`(0x0F) 的**目标不是偏移，而是内嵌在指令里的名字串**（起始 = `ins+0x14`，即指令头 0x14 字节之后）：

```asm
0045ba10  53                push  ebx                        ; call(0x0B) case
0045ba11  8bc7              mov   eax, edi                     ; eax = T
0045ba13  e8f8fbffff        call  0x45b610                    ; 压帧 + 按名解析目标
0045ba18  85c0              test  eax, eax
0045ba1a  0f8597180000      jne   0x45d2b7                     ; 解析失败 → 线程结束
0045ba20  e961180000        jmp   0x45d286
...
0045b7f7  8b5704            mov   edx, dword ptr [edi + 4]     ; edx = call 指令（T+4）
0045b7fa  8bb714100000      mov   esi, dword ptr [edi + 0x1014]  ; esi = H
0045b800  8b862c100000      mov   eax, dword ptr [esi + 0x102c]  ; eax = res（H+0x102C）
0045b806  83c214            add   edx, 0x14                    ; ★ edx = 指令 + 0x14 = 名字串
0045b809  52                push  edx
0045b80a  e801230000        call  0x45db10                     ; ★ FindSubByName(res, name)
0045b80f  d9ee              fldz
0045b811  8b4e04            mov   ecx, dword ptr [esi + 4]     ; ecx = H+4 = 当前线程
0045b814  894104            mov   dword ptr [ecx + 4], eax     ; ★ T+4 = 该 sub 的首指令
0045b817  8b7604            mov   esi, dword ptr [esi + 4]
0045b81a  d91e              fstp  dword ptr [esi]                ; T+0 = 0.0f
```

交叉印证（离线 `.ecl` 实体，`../exes/ecl/th11/stage01.ecl` 的 `main` sub，起始 `0x10A58`，首指令 `0x10A68`）：

```text
00010a90 time=60 op=0x0000 size=0x10
00010aa0 time=60 op=0x000f size=0x20  rest=b'MainSub00\x00\x00\x00'   ← async
00010bf4 time=60 op=0x000b size=0x20  rest=b'MainSub06\x00\x00\x00'   ← call
```
与 `../exes/ecl_dump/th11/stage01.decl:2254` 的 `@MainSub00() async;` / `@MainSub06();` 一一对应 ⇒ **`0x0B` = 同步 `call`，`0x0F` = `async`（另起线程）**，两者都以名字解析。
⇒ 副作用：**`main` 段内大量段落子脚本是 `async`**（`MainSub00`…），运行在**额外线程**里，见 §5.3 / §10.5 的实现注意。

### 2.5 与总括 §5.1 矛盾的裁定

总括 §5.1 的遗留矛盾「`[执行对象+4]` 到底是 sub 索引还是指令指针」在本作有明确答案：**th11 是「指令指针」**，与 **th13 / th14 同类**；**th15/16/17/18 是「索引」**，两者不可互相搬运。

---

## 3. Q2 —— sub 表：`res+0x8C`，项 8B（与 th13 逐字段一致）

### 3.1 四个现成访问器（游戏自己写的，直接坐实字段语义）

```asm
0040d070  8b888c000000      mov   ecx, dword ptr [eax + 0x8c]  ; eax = res → table
0040d076  8b04d1            mov   eax, dword ptr [ecx + edx*8] ; table[idx].+0 → 名字 char*
0040d079  c3                ret                                 ; GetSubName(res, idx)

0040d090  a17c8d4a00        mov   eax, dword ptr [0x4a8d7c]
0040d095  8b4070            mov   eax, dword ptr [eax + 0x70]  ; 第二个资源对象（同族）
0040d098  c3                ret                                 ; GetRes2()

0040d0a0  a17c8d4a00        mov   eax, dword ptr [0x4a8d7c]    ; mgr
0040d0a5  8b4864            mov   ecx, dword ptr [eax + 0x64]  ; res = mgr+0x64
0040d0a8  8b4108            mov   eax, dword ptr [ecx + 8]     ; res+8 → count
0040d0ab  c3                ret                                 ; GetResCount()

0040d0b0  a17c8d4a00        mov   eax, dword ptr [0x4a8d7c]    ; mgr
0040d0b5  8b4064            mov   eax, dword ptr [eax + 0x64]  ; res = mgr+0x64
0040d0b8  c3                ret                                 ; GetRes()

004106d0  8b808c000000      mov   eax, dword ptr [eax + 0x8c]  ; res → table
004106d6  c3                ret                                 ; GetTable(res)
```

⇒ 四条链一次坐实：`res = *(*(0x4A8D7C)+0x64)`、`count = res+8`、`table = res+0x8C`、`table[i].+0 = char* 名字`。
（对照 th13：`0x414150/0x414160/0x414170` 为同型访问器，仅基址与偏移不同。）

### 3.2 按名查表 —— `FUN_0045DB10`@`0x45DB10`（**name-sorted 二分**）

```asm
0045db10  53                push  ebx
0045db11  8b5808            mov   ebx, dword ptr [eax + 8]     ; eax = res → count
0045db14  55                push  ebp
0045db15  56                push  esi
0045db16  57                push  edi
0045db17  33ff              xor   edi, edi                     ; lo = 0
0045db19  83eb01            sub   ebx, 1                       ; hi = count-1
0045db1c  7859              js    0x45db77                     ; 空表 → 返回 0
0045db1e  8ba88c000000      mov   ebp, dword ptr [eax + 0x8c]  ; ebp = table
0045db24  8b4c2414          mov   ecx, dword ptr [esp + 0x14]  ; ecx = name（入参）
0045db28  8bc3              mov   eax, ebx
0045db2a  2bc7              sub   eax, edi
0045db2c  99                cdq
0045db2d  2bc2              sub   eax, edx
0045db2f  8bf0              mov   esi, eax
0045db31  d1fe              sar   esi, 1
0045db33  03f7              add   esi, edi                     ; mid = lo + (hi-lo)/2
0045db35  8b54f500          mov   edx, dword ptr [ebp + esi*8] ; table[mid].+0 = 名字
0045db40  8a01              mov   al, byte ptr [ecx]           ; 逐字节 strcmp(name, table[mid].name)
0045db42  3a02              cmp   al, byte ptr [edx]
...
0045db80  8b44f504          mov   eax, dword ptr [ebp + esi*8 + 4] ; ★ table[mid].+4 = 数据基址
0045db87  83c010            add   eax, 0x10                    ; ★★ +0x10 ⇒ 该 sub 的首条指令
0045db8b  c20400            ret   4
```

⇒ **`FindSubByName(res, name) = table[idx].+4 + 0x10`** —— 与 th13 的 `FUN_004797D0`（`0x47984B: add eax,0x10`）**完全同构**。

### 3.3 建表 / 加载（写侧）—— `FUN_0045D900`@`0x45D900`

```asm
0045d900  83ec08            sub   esp, 8
0045d903  56                push  esi
0045d904  8bf1              mov   esi, ecx                     ; esi = res（__thiscall）
0045d906  8b4604            mov   eax, dword ptr [esi + 4]     ; 文件槽号
0045d909  8b4c2410          mov   ecx, dword ptr [esp + 0x10]  ; 入参 = ECL 文件缓冲
0045d90d  894c860c          mov   dword ptr [esi + eax*4 + 0xc], ecx  ; res[+0xC+slot*4] = 缓冲
0045d918  813953435054      cmp   dword ptr [ecx], 0x54504353  ; 魔数 "SCPT"
0045d934  6683790401        cmp   word ptr [ecx + 4], 1        ; 版本 1
0045d93d  0fb74806          movzx ecx, word ptr [eax + 6]        ; 头尺寸
0045d949  8d6c0124          lea   ebp, [ecx + eax + 0x24]      ; 名字区 = buf+0x24+hdr
0045d94d  0fb74010          movzx eax, word ptr [eax + 0x10]     ; 本文件 sub 数
0045d951  014608            add   dword ptr [esi + 8], eax     ; ★ res+8 += sub 数  ← count
0045d955  8d7c8500          lea   edi, [ebp + eax*4]           ; 名字串区 = 名字区 + n*4
0045d967  e826280000        call  0x460192                     ; 重新分配（count*8）
0045d96c  89868c000000      mov   dword ptr [esi + 0x8c], eax  ; ★ res+0x8C = 新表
0045d984  8b5604            mov   edx, dword ptr [esi + 4]
0045d987  8b44960c          mov   eax, dword ptr [esi + edx*4 + 0xc]  ; 缓冲
0045d98b  034500            add   eax, dword ptr [ebp]                ; 缓冲 + offset[i]
0045d994  8944ca04          mov   dword ptr [edx + ecx*8 + 4], eax    ; table[i].+4 = 起始
0045d99e  893cc8            mov   dword ptr [eax + ecx*8], edi        ; table[i].+0 = 名字 char*
...
; ebx != 0（表已存在）→ 走「strcmp 找插入点 + 后移 + 插入」，即 ★ 表按名字字典序保持有序
0045da20  8b4d00            mov   ecx, dword ptr [ebp]                 ; table[j].+0
0045da25  8a10              mov   dl, byte ptr [eax]                   ; strcmp(name, table[j].name)
...
0045da8d  894cda04          mov   dword ptr [edx + ebx*8 + 4], ecx     ; 插入项 +4
0045da97  893cd8            mov   dword ptr [eax + ebx*8], edi         ; 插入项 +0
```

⇒ `res+0x8C` = 表；项 **8 字节**；`+0` = 名字 `char*`（指向已加载缓冲内的名字串，**运行期保留**）；`+4` = `buffer + offset[i]`（sub 数据基址）；`res+0xC+slot*4` = 文件缓冲数组；**表按名字字典序有序**（故 `FUN_0045DB10` 可二分）。
⇒ **多文件合并**：每加载一个 ECL 文件都会 `res+8 += n` 并把该文件的 sub 一并插入同一张表 ⇒ **表是「当前已加载全部 ECL 文件」的并集**（这是分支关天然兼容的关键，见 §9.4）。

### 3.4 首指令 = `+4 + 0x10`（离线实体交叉印证）

`../exes/ecl/th11/stage01.ecl` 的 sub 数据以 **16 字节前导** 开头：

```text
sub 0 BGirl00  off=0x498  bytes[0:0x20] = 45 43 4c 48 | 10 00 00 00 | 00 00 00 00 00 00 00 00 | 00 00 00 00 28 00 14 00 00 00 ff 01 ...
                                                   ↑ "ECLH"    ↑ 大小            ↑ 前导末尾    ↑ +0x10 = 首指令
```
- `45 43 4c 48` = `"ECLH"` = `0x484C4345`，正与 `thprac_th11.cpp:1005` 的 `ECLTimeFix` 判停条件 `ecl_time == 0x484c4345` 吻合；
- `+0x10` 起 = `time(int32)=0`、`opcode(u16)=0x0028`、`size(u16)=0x0014`、掩码、`0xFF`（任意难度）—— 是一条合法指令头。
⇒ **`table[i].+4 + 0x10` = 该 sub 的首条指令** ✅（与 th13 完全一致）。

### 3.5 `.ecl` 文件头（供离线枚举与兜底用）

```text
+0x00 "SCPT"｜+0x04 u16 版本(1)｜+0x06 u16 头尺寸 hdr｜+0x10 u16 sub 数 n｜+0x24 hdr 字节附加头
+0x24+hdr       : n 个 u32「文件内绝对偏移」（每个 sub 的数据基址）
+0x24+hdr+4n    : n 个 NUL 结尾的名字串
之后            : sub 数据（每个以 "ECLH"+0x10 前导开头）
```
实测（`stage01.ecl`：hdr=`0x34`、n=81、offs[0:6]=`0x498,0x538,0x5d8,0x670,0x710,0x8f4`、名字区首串 `BGirl00`）——与 §3.2 加载器逐条对上。

---

## 4. Q3 —— ECL 子调用栈（**存在**；遍历步长未完全定案）

### 4.1 结论

1. **有显式可遍历的子调用栈**，随解释器自动维护，与线程 T 内嵌：
   - **帧存储基址**：`base = T + 0x08`
   - **栈指针**：`SP = *(u32*)(T + 0x1008)`（int，**字节**偏移；`0` = 空栈/顶层）
   - 帧内容：`{ 调用时 T+0（时间, float）；调用者当时的指令地址（= 调用者 T+4） }`
2. `ret`(opcode `0x0A`) 以**固定 8B** 回退（先 `SP -= 4` 取 ip，再 `SP -= 4` 取时间）。
3. ⚠️ **实参与帧同处一段区间**（实参位于帧**之上**），因此逐帧上溯需要知道每帧的实参个数，**精确步长规则未完整定案**（见 §4.5）——本项**不参与** Q9 判定，故不影响实现。

### 4.2 `ret`（opcode `0x0A`）—— case @ `0x45B8BD`/RVA `0x5B8BD`

```asm
0045b8bd  8d7708            lea   esi, [edi + 8]               ; esi = T+8 = 栈基址
0045b8c0  8bc6              mov   eax, esi
0045b8c2  e859240000        call  0x45dd20                     ;（清表达式临时值）
0045b8c7  399f08100000      cmp   dword ptr [edi + 0x1008], ebx ; SP == 0 ?
0045b8cd  0f84e1190000      je    0x45d2b4                     ;   空 → T+4 = 0（线程结束）
0045b8d3  8b8600100000      mov   eax, dword ptr [esi + 0x1000] ; eax = *(T+0x1008) = SP
0045b8d9  83c0fc            add   eax, -4
0045b8dc  3bc3              cmp   eax, ebx
0045b8de  7c0c              jl    0x45b8ec
0045b8e0  898600100000      mov   dword ptr [esi + 0x1000], eax ; SP -= 4
0045b8e6  8b1406            mov   edx, dword ptr [esi + eax]    ; 读 [T+8+SP] = 调用者 ip
0045b8e9  895704            mov   dword ptr [edi + 4], edx      ; ★ T+4 = 调用者 ip
0045b8ec  8b8600100000      mov   eax, dword ptr [esi + 0x1000]
0045b8f2  83c0fc            add   eax, -4
0045b8f5  3bc3              cmp   eax, ebx
0045b8f7  7c0b              jl    0x45b904
0045b8f9  898600100000      mov   dword ptr [esi + 0x1000], eax ; SP -= 4
0045b8ff  8b0406            mov   eax, dword ptr [esi + eax]
0045b902  8907              mov   dword ptr [edi], eax          ; ★ T+0 = 调用者时间
0045b904  395f04            cmp   dword ptr [edi + 4], ebx
0045b907  0f84aa190000      je    0x45d2b7
0045b90d  e968190000        jmp   0x45d27a
```

### 4.3 `call`（opcode `0x0B`）—— case @ `0x45BA10` → 压帧 `FUN_0045B610`@`0x45B610`

```asm
0045b610  83ec10            sub   esp, 0x10
0045b61a  8bf0              mov   esi, eax                     ; esi = T
0045b61c  8b4704            mov   eax, dword ptr [edi + 4]     ; 当前 call 指令
0045b61f  8b4810            mov   ecx, dword ptr [eax + 0x10]  ; ins[+0x10] = 实参区偏移
0045b622  8b8608100000      mov   eax, dword ptr [esi + 0x1008] ; eax = SP
0045b628  8d5c9104          lea   ebx, [ecx + edx*4 + 4]
0045b636  8d4808            lea   ecx, [eax + 8]               ; ★ 实参首槽 = SP+8（SP==0 时为 0xC）
...
0045b65a  8b5704            mov   edx, dword ptr [edi + 4]
0045b661  0fb6520b          movzx edx, byte ptr [edx + 0xb]    ; ★ ins[+0xB] = 实参个数
0045b665  45                inc   ebp
0045b666  3bea              cmp   ebp, edx
0045b668  0f8de0000000      jge   0x45b74e                     ; 拷贝实参 → 压帧
...
0045b74e  ba04000000        mov   edx, 4
0045b757  8b8e08100000      mov   ecx, dword ptr [esi + 0x1008]
0045b75d  83c1fc            add   ecx, -4
0045b762  898e08100000      mov   dword ptr [esi + 0x1008], ecx
0045b768  8b4c3108          mov   ecx, dword ptr [ecx + esi + 8]
0045b76c  894c2414          mov   dword ptr [esp + 0x14], ecx
0045b774  898608100000      mov   dword ptr [esi + 0x1008], eax ; SP 复位
0045b77a  894c3004          mov   dword ptr [eax + esi + 4], ecx ; 首帧「旧值」规范化
0045b78f  8b0f              mov   ecx, dword ptr [edi]         ; T+0 = 时间
0045b791  894c3008          mov   dword ptr [eax + esi + 8], ecx ; ★ 压帧[+0] = 时间
0045b795  019608100000      add   dword ptr [esi + 0x1008], edx
0045b7ac  8b4f04            mov   ecx, dword ptr [edi + 4]      ; T+4 = 当前指令
0045b7af  …                 （→ 0x45B7E1: mov [eax+esi+8],ecx）  ; ★ 压帧[+4] = 调用者 ip
```
⇒ 压栈顺序 `{时间, 调用者 ip}`，与 §4.2 弹栈次序严格对称；随后 `T+4 = FindSubByName(res, ins+0x14)`（§2.4）、`T+0 = 0`。

### 4.4 异步 `async`（opcode `0x0F`）—— case @ `0x45B912` → `FUN_0045D7A0`@`0x45D7A0`

```asm
0045b912  8b8714100000      mov   eax, dword ptr [edi + 0x1014] ; eax = H
0045b918  53                push  ebx                        ; 0
0045b919  6aff              push  -1
0045b91b  e8801e0000        call  0x45d7a0                     ; 另起线程

0045d7a3  6824100000        push  0x1024
0045d7aa  e835250000        call  0x45fce4                     ; malloc 线程对象
0045d7b8  899808100000      mov   dword ptr [eax + 0x1008], ebx ; SP = 0
0045d7d7  d91e              fstp  dword ptr [esi]              ; T'+0 = 0.0f
0045d7df  89be14100000      mov   dword ptr [esi + 0x1014], edi ; ★ T'+0x1014 = H
0045d7f7  8d8f30100000      lea   ecx, [edi + 0x1030]         ; ★ 线程链表头
0045d7fd  8930              mov   dword ptr [eax], esi         ; 节点+0 = 线程
0045d802  895808            mov   dword ptr [eax + 8], ebx     ; 节点+8 = prev
0045d818  894104            mov   dword ptr [ecx + 4], eax     ; 头节点.next = 新节点
0045d81b  894808            mov   dword ptr [eax + 8], ecx
0045d828  e8e3ddffff        call  0x45b610                     ; 压帧 + 按名解析该 async sub
```
⇒ **线程链表节点 = `{ +0 线程指针, +4 next, +8 prev }`，链表头 = `H+0x1030`**；**首节点的线程 = `H+8` = 主线线程**。

### 4.5 ⚠️ 未定案与遍历建议

- **帧长**：`ret` 固定回退 8B（`{时间, ip}`），**不含「旧 SP」**（与 th13 的 0x0C 三槽帧**不同**）；
- **实参位置**：实参写在帧**上方**（首槽 = `T+8+SP+8`），故相邻两帧之间夹着 `4×实参数` 字节；
- ⇒ 逐帧上溯需按 `帧 8B + 4B×实参数` 跳步，而某个帧的实参数只能从**它自己的 `call` 指令**（`ins[+0xB]`）得知，从栈上无法直接读出 ⇒ **完整遍历规则未定案（🟡）**。
- **推荐**：本迭代**不使用**函数栈（Q9 已定案为路过式 latch）；如需保留该事实，只记录「栈存在 + 基址/SP/压弹点」，不实现遍历。
- 另注：`SP` 有效域上限 `0x1000`（源码多处 `cmp …, 0x1000; jge`），故物理帧数上限约 `0x1000/8 = 512`。

---

## 5. Q4 —— 主线宿主捕获点

### 5.1 两条「spawn main」站点（`EAX` = 宿主）

字符串 `"main"` @ **`0x494258`**（RVA `0x94258`，全程序仅此一处被引用；另有未引用的 `0x49465A`）。

```asm
; 站点 1（进关）
00420485  6a50              push  0x50
00420487  8d442414          lea   eax, [esp + 0x14]
0042048b  53                push  ebx
0042048c  50                push  eax
0042048d  e88ea20400        call  0x46a720                     ; memset(参数结构, 0, 0x50)
00420495  6858424900        push  0x494258                     ; "main"
0042049a  8d5c2414          lea   ebx, [esp + 0x14]            ; ebx = 参数结构（ctor 的隐式入参）
0042049e  e84d04ffff        call  0x4108f0                     ; ★ 返回 EAX = 主线宿主
004204a3  e828a2ffff        call  0x41a6d0                     ; ★★ 可挂 5B EHOOK 的站点（EAX = H）

; 站点 2（另一重开/复位路径，同型）
004205d2  6858424900        push  0x494258                     ; "main"
004205d7  8d5c2414          lea   ebx, [esp + 0x14]
004205db  e81003ffff        call  0x4108f0                     ; ★ 返回 EAX = 主线宿主
004205e0  e8eba0ffff        call  0x41a6d0                     ; ★★ 可挂 5B EHOOK 的站点（EAX = H）
```

⇒ **Q4 答案**：捕获点 = **`0x4204A3` / `0x4205E0`**，命中时 **`EAX` = 主线 ECL 宿主 H**（对应总括里 TH15 的 `0x43BBC8/0x43D48E/0x43D6BE` 角色；下一条均为 `call 0x41a6d0`，5B，**5B EHOOK 合适**）。

### 5.2 宿主工厂与构造（`FUN_004108F0` / `FUN_004111C0`）

```asm
004108f0  51                push  ecx
004108f1  a17c8d4a00        mov   eax, dword ptr [0x4a8d7c]    ; mgr
004108f9  6878260000        push  0x2678
00410902  e8ddf30400        call  0x45fce4                     ; malloc 0x2678
0041090e  8b4c2414          mov   ecx, dword ptr [esp + 0x14]  ; 入参 = 脚本名
00410912  51                push  ecx
00410913  8bf8              mov   edi, eax
00410915  e8a6080000        call  0x4111c0                     ; ★ 宿主构造：EAX = H
0041091a  8be8              mov   ebp, eax
...
00410ad8  8bc5              mov   eax, ebp                     ; ★ 返回宿主
00410ada  5f                pop   edi
00410adb  5e                pop   esi
00410adc  5d                pop   ebp
00410add  59                pop   ecx
00410ade  c20400            ret   4
```

宿主构造函数 `FUN_004111C0`（`edi` = 对象，栈参 = 脚本名，`ret 4`，**返回 `EAX` = H**）关键片段：

```asm
004111d7  c70774404900      mov   dword ptr [edi], 0x494074    ; ★ 主线宿主 vtable
004111f0  83a728100000fe    and   dword ptr [edi + 0x1028], 0xfffffffe
004111f7  d95f08            fstp  dword ptr [edi + 8]           ; T+0 = 0.0f
004111fd  894704            mov   dword ptr [edi + 4], eax     ; ★ H+4 = H+8（当前线程 = 内嵌线程）
00411203  89bf1c100000      mov   dword ptr [edi + 0x101c], edi ; ★ T+0x1014 = H（自指针）
00411218  898730100000      mov   dword ptr [edi + 0x1030], eax ; ★ H+0x1030 = H+8（线程链表头节点）
0041121e  899f34100000      mov   dword ptr [edi + 0x1034], ebx ; next = 0
...
00411392  a17c8d4a00        mov   eax, dword ptr [0x4a8d7c]    ; ★ mgr
0041139d  8b4c2410          mov   ecx, dword ptr [esp + 0x10]  ; ★ 脚本名（ctor 入参）
004113b7  8b4064            mov   eax, dword ptr [eax + 0x64]  ; ★ res = *(mgr+0x64)
004113ba  51                push  ecx
004113bb  89872c100000      mov   dword ptr [edi + 0x102c], eax ; ★★ H+0x102C = res
004113c1  e84ac70400        call  0x45db10                     ; ★ FindSubByName(res, name)
004113c6  8b5704            mov   edx, dword ptr [edi + 4]     ; T = H+4
004113c9  894204            mov   dword ptr [edx + 4], eax     ; ★★ T+4 = 该 sub 的首指令
004113cf  d918              fstp  dword ptr [eax]              ; T+0 = 0.0f
```

⇒ 一次坐实三件事：**(a)** `H+0x102C = res`；**(b)** 主线 sub 的落点 = `FindSubByName(res, name)` = `table[i].+4+0x10`；**(c)** 「设主线 sub」的写入点 = **`0x4113C9`**（th11 版的 th13 `0x4191F9`）。

### 5.3 宿主→线程（`H+4` 与 `H+0x1030`）与静态兜底

```asm
0040fed0  fldz
0040fed2  83a028100000fe    and   dword ptr [eax + 0x1028], 0xfffffffe
0040fedb  d95808            fstp  dword ptr [eax + 8]           ; H+8 = 0.0f    ← T+0
0040fede  8d4808            lea   ecx, [eax + 8]
0040fee1  89500c            mov   dword ptr [eax + 0xc], edx   ; H+0xC = 0     ← T+4
0040fee4  89801c100000      mov   dword ptr [eax + 0x101c], eax ; ★ T+0x1014 = H
0040feea  c78018100000ffffffff  mov dword ptr [eax + 0x1018], 0xffffffff
0040fefa  894804            mov   dword ptr [eax + 4], ecx     ; ★ H+4 = H+8
0040fefd  898830100000      mov   dword ptr [eax + 0x1030], ecx ; ★ H+0x1030 = H+8（链表头节点）
0040ff03  899034100000      mov   dword ptr [eax + 0x1034], edx ; next = 0
0040ff0f  c3                ret
```
（`FUN_0040FED0` = th11 版的 th13 `FUN_00417740`）

```asm
0045d420  51                push  ecx                        ; 线程驱动器
0045d421  53                push  ebx
0045d422  55                push  ebp
0045d423  56                push  esi
0045d424  8db730100000      lea   esi, [edi + 0x1030]          ; ★ 线程链表头
0045d433  8b06              mov   eax, dword ptr [esi]         ; eax = 线程
0045d439  8b5e04            mov   ebx, dword ptr [esi + 4]     ; ebx = next
0045d440  894704            mov   dword ptr [edi + 4], eax     ; ★ H+4 = 当前线程
0045d443  e818e4ffff        call  0x45b860                     ; ★ 跑一帧 ECL VM
0045d48f  8bf3              mov   esi, ebx
0045d491  85db              test  ebx, ebx
0045d493  759e              jne   0x45d433
0045d495  8d4708            lea   eax, [edi + 8]
0045d498  894704            mov   dword ptr [edi + 4], eax     ; 无其他线程 → H+4 = H+8
0045d49b  33c0              xor   eax, eax
0045d4a1  c20400            ret   4
```
⇒ **`T = *(u32*)(H+0x1030)`（首节点线程）恒为 `H+8` = 主线线程**；`H+4` 是「本帧最后执行过的线程」（可能被 async 覆盖）。

宿主链表（供静态兜底）：
```asm
00410540  a17c8d4a00        mov   eax, dword ptr [0x4a8d7c]
00410545  8b4068            mov   eax, dword ptr [eax + 0x68]  ; GetHostListHead() = *(mgr+0x68)
00410548  c3                ret
00410ac1  8d8da4110000      lea   ecx, [ebp + 0x11a4]          ; ★ 节点 = H+0x11A4
00410acc  017070            add   dword ptr [eax + 0x70], esi  ; mgr+0x70 = 计数
00410ad2  894868            mov   dword ptr [eax + 0x68], ecx  ; mgr+0x68 = 头
00410ad8  8bc5              mov   eax, ebp
00410ae1  8b506c            mov   edx, dword ptr [eax + 0x6c]  ; mgr+0x6C = 尾
00410af4  894a04            mov   dword ptr [edx + 4], ecx     ; 尾.next = 新节点
00410afc  895108            mov   dword ptr [ecx + 8], edx     ; 新节点.prev = 尾
```
⇒ 节点 = `{ +0 = H, +4 = next, +8 = prev }`（遍历代码 `0x4129AD: mov ecx,[eax]` / `0x4129B9: mov ebx,[eax+4]` 印证）。
⇒ **静态兜底（无需钩子）**：`H = *(u32*)( *(u32*)( *(u32*)0x4A8D7C + 0x68 ) )`（链表头节点）。⚠️ `FUN_004108F0` 共有 **7** 个调用点（`0x40D873`/`0x41303F`/`0x4131CD`/`0x4132BB`/`0x418503`/`0x42049E`/`0x4205DB`），只有后两处传 `"main"` ⇒「链头 = main」只在 main 最先创建时成立，**推荐仍用捕获点**。

---

## 6. Q5 —— 解释器命中点 `0x45D2A3` 与寄存器语义

### 6.1 命中点（覆盖 2B 的指令语义）

```asm
0045d29a  d907              fld   dword ptr [edi]              ; T+0（当前 ECL 时间）
0045d29c  d8842434010000    fadd  dword ptr [esp + 0x134]      ; + 本帧增量（唯一栈参）
0045d2a3  d91f              fstp  dword ptr [edi]              ; ★★ EHOOK(2B) 命中：写回 T+0
```
- **地址**：`0x45D2A3` / RVA `0x5D2A3`；**覆盖 2 字节**（`D9 1F` = `fstp dword ptr [edi]`，一条完整指令）⇒ 2B EHOOK，与 thprac `EHOOK_HK(0x45d2a3, 2, …)` **一致**。
- **命中寄存器**：**`Edi` = 执行对象 = 线程 T**（脚本上下文）；`T+0` = 当前 ECL 时间；`T+4` = 当前指令地址。
- **语义**：**「下一条指令时间未到 → 累加本帧时间并返回」路径**（不是分发头）。因此该 hook **只在某线程处于等待态时命中**，且**每个线程各命中一次** —— **不是无条件每帧触发点**。

### 6.2 所在函数与调用链

- **函数**：`FUN_0045B860`，入口 `0x45B860`（RVA `0x5B860`），体 `0x45B860`–`0x45D2C4`。
- **入参**：`EAX` = 线程 T，栈参（`[esp+4]`，进函数后 `[esp+0x134]`）= 帧增量 `float`；返回 `0` = 本帧到此为止、`−1` = 脚本结束。
- **opcode 分发**：`(i16)[ins+4]`；`> 0x58` → `0x45D269`（对 `[T+0x1014]` = H 做 **虚调用** vtable slot 0，`eax == -1` 则返回 0）；`≤ 0x58` → 经字节映射 `0x45D3BC` → 跳表 `0x45D2C8`。
- 映射核实：**`0x0A`(ret) → `0x45B8BD`；`0x0B`(call) → `0x45BA10`；`0x0C`(goto) → `0x45BAC2`；`0x0F`(async) → `0x45B912`**（与 th13 的 `0x0A/0x0B/0x0C` 编号一致；`0x0F` 为本作新增关注点）。
- **调用链**：`FUN_0045D420`（`0x45D420`，遍历线程链 `H+0x1030`，`mov [edi+4],eax` 后 `call FUN_0045B860`）→ 被 **`0x411ABB` / `0x411D43`**（宿主每帧更新主路径）与 `0x438B06` 调用。

### 6.3 作为「每帧稳定触发点」的评价

- `FUN_0045B860` 每帧对**每个活跃线程**调用一次；`0x45D2A3` 只在「时间未到」时命中 ⇒ **不是无条件每帧**；且会**对所有线程（含 async、含 boss 宿主）命中**。
- 既有 hook 用 `*(u32*)(Edi+0x1014)+0x25BC == 20512` 过滤主线宿主 —— 这一判据**成立且可复用**：`*(u32*)(T+0x1014) = H`（宿主自指针，ctor/复位/async 三处都写；见 §5.2 / §4.4 / §5.3），`H+0x25BC` = 宿主 flags。
- **推荐做法（与 th13 同风格）**：**不新增 VM 钩子**。只在 `0x4204A3`/`0x4205E0` 抓一次 H，然后在 IGI 渲染（帧间稳定态）读 `T = *(u32*)(H+0x1030)`、`ip = *(u32*)(T+4)`。`0x45D2A3` 仅作「需要无条件每帧触发」时的备选（它同时已承载 thprac 的「锁时」功能，勿与监控混用）。

### 6.4 ⚠️ 对 requests §1 / 总括 §5.1 旧值的更正

| 旧值（requests §1 / 总括 §5.1） | 更正 |
| --- | --- |
| 「ECL 解释器命中点 `0x45d2a3`（2B）」 | **地址与长度正确**，但它是**「时间未到→累加」尾部**，**不是**解释器分发头；分发头 = `0x45B890`，解释器入口 = `0x45B860`。 |
| 「命中时执行对象寄存器 `Edi`」 | **正确**；但语义应明确为 **`Edi` = 线程/脚本上下文 T（不是宿主 H）**。既有 hook 的 `pCtx->Edi + 0x1014` 正是借此拿到 H。 |
| 「主线判据 `GetMemContent(Edi+0x1014, 0x103c+0x1580) == 20512`」 | **判据正确、无需改**（= `*(u32*)(H+0x25BC) == 20512`）。但注记里「`[Edi+0x1014]` 是某个管理器」应改为 **「= 宿主 H 自身」**；「`+0x103c+0x1580` 是敌人的 flags」应改为 **「宿主 `H+0x103C` 内嵌子对象 `+0x1580`，即 `H+0x25BC` 的宿主 flags」**。 |
| 「ECL 数据区链 `GetMemAddr(0x4a8d7c, 0x64, 0xC)`」 | **正确、可直接用**（= `res+0xC`，`res = *(*(0x4A8D7C)+0x64)`）。 |
| 总括 §5.1 th11 行「sub 名/索引线索 ❌」 | **更正为 ✅**：`res+0x8C` 表（项 8B，`+0`=名字 `char*`，`+4`=起始，首指令 `+4+0x10`）；`count = res+8`。 |
| requests §1 注记「这条链很可能也通向 ECL 资源/sub 表」 | **已被证实**：`H+0x102C = res`（`0x4113BB`），`res+0x8C` = sub 表。 |
| requests §1「指针常量 …」 | 与本轮结论无冲突；新增 `0x4A8D7C` = 全局 ECL 管理器、`0x4A8ECC` = 机体/子机对象、`0x494258` = `"main"`、`0x494074` = 主线宿主 vtable。 |

**新增（requests §1 未记录）**：主线宿主捕获 `0x4204A3`/`0x4205E0`（`EAX` = 宿主）；线程驱动器 `0x45D420`；`FindSubByName 0x45DB10`；SCPT 加载器 `0x45D900`；宿主工厂 `0x4108F0`。

---

## 7. Q6 —— 运行期保留 sub 名

✅ **保留**。加载器 `FUN_0045D900` 把**名字区指针**（已加载 ECL 缓冲内的名字串首址）直接写入 `table[i].+0`：

```asm
0045d998  8b868c000000      mov   eax, dword ptr [esi + 0x8c]   ; table
0045d99e  893cc8            mov   dword ptr [eax + ecx*8], edi   ; ★ table[i].+0 = 名字 char*
0045d9a1  8bc7              mov   eax, edi
0045d9a3  8d5801            lea   ebx, [eax + 1]
0045d9a6  8a10              mov   dl, byte ptr [eax]             ; 扫到 NUL 推进到下一个名字
0045d9a8  40                inc   eax
0045d9a9  84d2              test  dl, dl
0045d9ab  75f9              jne   0x45d9a6
```
（排序插入路径同型：`0x45DA97: mov [eax+ebx*8], edi`）
⇒ 运行期 `name = *(char**)(table + i*8)` 直接可读，**无需离线名表**。
> ⚠️ **但本作需要一个「地址 → i」的反查**（因 Q1 是地址而非索引），见 §2.3/§8.3。反查只需 `table[i].+4 + 0x10`，**不需要离线 `.ecl`**；离线 `.ecl` 只用于「段落入口白名单」本身。

---

## 8. Q7 —— 机体 / 子机 / 难度地址

| 名称 | VA / RVA | 说明 | 置信度 |
| --- | --- | --- | --- |
| `mainShot`（机体） | `*(u32*)(*(u32*)0x4A8ECC + 0x38)` | `thprac_th11.cpp:175` 既有用法（用于识别 MarisaA） | 🟡 地址来自 thprac，**未真机核对** |
| `subShot`（子机） | `*(u32*)(*(u32*)0x4A8ECC + 0x3C)` | `thprac_th11.cpp:176` | 🟡 同上 |
| 难度 / rank | `0x4A5720` / `0xA5720` | `*(i32*)0x4A5720`；`thprac_th11.cpp:174` 既有；全程序多处（`0x410977` 附近由它生成选择掩码） | 🟡 |
| `0x4A8ECC` | `0xA8ECC` | 一个**堆对象指针**（`0x429856: mov esi,[0x4a8ecc]`，其在 `0x429861` 被 `call 0x4396c0` 处理后再 `free`），疑为机体/子机/自机管理对象 | 🟡 语义待真机 |
| `PLAYER_PTR` | `0x4A8EB4` / `0xA8EB4` | `thprac_th11.cpp:44` 既有（`player`） | ✅ |
| `STAGE_NUM` | `0x4A5728` / `0xA5728` | `thprac_th11.cpp:52` 既有 | ✅ |
| `REPLAY_MGR_PTR` | `0x4A8EB8` / `0xA8EB8` | `thprac_th11.cpp:53` 既有 | ✅ |
| `GAME_THREAD_PTR` | `0x4A8E88` / `0xA8E88` | `thprac_th11.cpp:54` 既有（GameThread，**与 ECL 宿主 H 是两个对象**） | ✅ |

> 说明：本迭代 IGI 只需显示「当前段落」，Q7 非必需；上表供后续日志迭代。`0x4A8ECC` 系列**未经真机取值核对，勿直接用于判据**。

---

## 9. Q8 —— 练习 `ECLJump` 对读取的影响

### 9.1 结论

✅ **不影响 sub 判读**（与 th13 §9 / th15 §2 / th16 §Q8 一致）；**分支关亦成立**（§9.4）。

### 9.2 依据

1. **补丁写入**（`thprac_th11.cpp:992-996`）：
   ```cpp
   ecl.SetPos(start);
   ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << delay;
   ```
   6 个 dword = 24B（`size = 0x18`）：`+0` = `ecl_time`(int32)、`+4` = `0x0018000C`（低 16 位 `0x000C` = **`goto`**，高 16 位 `0x0018` = size）、`+8` = `0x02FF0000`（`ins[0xA] = 0xFF` = 任意难度；`ins[8..9] = 0` = 无浮点参数）、`+0xC` = 0（局部槽数 0）、`+0x10` = `dest-start`、`+0x14` = `delay`。
   ⇒ 与本作指令头布局（§1）**逐字段吻合**。
2. **`goto` 语义**（case `0x0C` @ `0x45BAC2`/RVA `0x5BAC2`）：
   ```asm
   0045bac2  8b4704            mov   eax, dword ptr [edi + 4]       ; 当前指令
   0045bac5  db4014            fild  dword ptr [eax + 0x14]         ; ins[+0x14] = delay（int）
   0045bac8  d91f              fstp  dword ptr [edi]                ; ★ T+0 = delay（绝对赋值）
   0045baca  8b4810            mov   ecx, dword ptr [eax + 0x10]    ; ins[+0x10] = dest-start
   0045bacd  e9af170000        jmp   0x45d281                        ; ★ → add ecx,eax; mov [edi+4],ecx
   ```
   **只改 `T+0`（时间）与 `T+4`（ip），从不触碰 sub 表，也不触碰任何「当前 sub」的旁路字段。**
3. **sub 结构未动**：`ECLHelper::SetBaseAddr` 定位的是 `res+0xC` 起的**运行期 ECL 文件缓冲**（§1）；`ECLJump` 只在硬编码偏移处覆写指令体，不触碰 SCPT 头、名字区、`res+0x8C` 表（表由 `FUN_0045D900` 一次解析生成）。
4. **瞬态注意**：`goto` 之后、目标 `call`/`async` 尚未执行的那一瞬，`T+4` 指向「跳转后位置」（仍是**旧 sub 内**的地址）；执行后即变到目标段落入口。采样落在帧间稳定态即可规避。

### 9.3 `ECLSatoriJump`（th11 特有，用于 st4 Satori 段）

`thprac_th11.cpp:1034` 的 `ECLSatoriJump` 是 `ECLJump` + 一批**按机体的分支 `ECLJump`**（`case shot_type 0..5` → `0x5820 → 0x5870/0x58fc/…`）；它同样是纯 `goto` 补丁，**不改变 sub 判读**。

### 9.4 ⚠️ 分支关（st4）的天然兼容性

- **st4 的分支形态**：`../exes/ecl/th11/` 里有 `stage4c00{a,b,c}.ecl` 与 `stage4c01{a,b,c}.ecl` 共 **6 份变体脚本**（sub 名为 `BossCard2_00A`/`BossCard3_00Aat`… 与 `…_01A` 系列，见 `../exes/ecl_dump/th11/stage4c0*.decl`）——即 Satori 按**路线（A/B）+ 机体/子机**选择不同的「复制弹幕」脚本。**这 6 份是各自的独立 ECL 文件**，通过 `ECLHelper::SetFile(ord)`（= `res+0xC+ord*4`）选择（thprac 在 st4 boss 段确实用了 `ecl.SetFile(2)`/`SetFile(3)`，见 `thprac_th11.cpp:1481/1496/1506`）。
- **为什么监控天然兼容**：本技法是「读**运行期状态**」——`T+4` 是**绝对地址**，而 `res+0x8C` 表是**所有已加载 ECL 文件的并集**（§3.2：每个文件加载时 `res+8 += n` 并把它的 sub 一并插入排序表）。因此**无论走哪条分支、哪个槽位**，`T+4` 都能映射回正确的 sub 名；不需要为分支做任何特判，也不需要在 warp 前后重新建表。
- **⚠️ 与 Q8 的交互**：`ECLJump` 的 `start`/`dest` 偏移是**相对「当前选中槽位的文件」**的（thprac 对 st4 boss 段先 `SetFile(2/3/…)` 再 `ECLJump`）。这只是**写侧**的槽位纪律问题；对**读侧**（监控）无影响。
- **仍需真机确认**：分支脚本的 subs 是否都进了同一张表、以及在分支切换的瞬间 `T+4` 的映射是否稳定（见 §13）。

---

## 10. Q9 —— 段落判定口径：**路过式 checkpoint（latch）**

### 10.1 结论

✅ **走「路过式 checkpoint（latch）」**：读当前 sub（本作经 `NameOfAddress(T+4)` 得到）→ **∈ 白名单则置当前段落；不在 → no-op**（保持上一个值，不回退 `main`）；本局首次命中前显示 `main`。CSV 即白名单表。

### 10.2 依据与口径确认

- 总括 §6 Q9 已定案；本作只需确认**「当前 sub 怎么取」**（= §2 / §3 的链）与**「每 checkpoint 登记 1 个入口 sub（含 async）」**。
- **本作的取法**：`T+4` 是**绝对地址** ⇒ 不能直接当索引用；须用 `table[i].+4+0x10` 建**区间表**（见 §10.3），这与 th13 完全相同。
- 段内多个 `MainSubNN` 在本作是 `main` 的**兄弟 `call` 或兄弟 `async`**（离线实体，`stage01.decl:2254`：`@MainSub00() async; … @MainSub06();`），彼此**不构成祖先关系** ⇒ 再次印证「(A) 调用栈祖先法」不成立、必须用 latch。
- **分支关天然兼容**：读的是运行期「当前指令 → sub」，走哪条分支就读到哪条（§9.4）。

### 10.3 判定流程

```cpp
H = mEclHost;                                        // 0x4204A3 / 0x4205E0 捕获
if (!H) return "main";
res = *(u32*)(H + 0x102C);                           // ≡ *(u32*)(*(u32*)0x4A8D7C + 0x64)
if (!res) return "main";
count = *(i32*)(res + 8);
table = *(u32*)(res + 0x8C);
if (!table || count <= 0) return "main";
T = *(u32*)(H + 0x1030);                             // ★ 主线线程（= H+8），不用 H+4（会被 async 覆盖）
if (!T) return "main";
uint32_t ip = *(u32*)(T + 4);                        // 当前指令绝对地址（Q1）；0 = 线程已结束
const char* cur = NameOfAddress(table, count, ip);   // 区间表反查：start_i <= ip < start_{i+1}
const char* section = Whitelist(game, stage, cur);   // 命中 → 置当前段落
if (section) current = section;                      // 未命中 → no-op（不回退 main）
```

**区间表构造**（一次性 / 每次 res 变化时重建）：
```cpp
// start_i = *(u32*)(table + i*8 + 4) + 0x10
// ⚠️ 表项按「名字字典序」（FUN_0045DB10 二分）存放，地址并不单调 ⇒ 必须先按 start_i 排序再区间判定
// 越界/脏指针保护：ip 必须落在某 [start_i, start_{i+1}) 内，否则视为「不在已知 sub」；ip == 0 ⇒ 线程已结束
```

### 10.4 ⚠️ 未定论项（段落入口白名单待枚举）

- th11 的段落入口名单**尚未枚举**（`../exes/ecl_dump/th11/` 的 20 份 `.decl` 素材已在机，可离线枚举），与 `THStageWarp` 偏移一一对应的入口表亦待建。
- **替代方案（可立即使用）**：`thprac_th11.cpp:1063-1239`（`THStageWarp`）与 `1239-2116`（`THPatch`）里已有一批**硬编码的段落入口文件偏移**（如 st1 `0xf764→0xfa04`、st4 `0xf78c→0xf7f4`…），它们是离线手工解码得到的**指令偏移**；运行期把 `T+4` 换算成「相对某个文件缓冲的偏移」即可直接比对 ⇒ 可作为**白名单的初始实现**（需一并解决「槽位 ↔ 文件」映射与「算哪个槽位的基址」，见 §1/§9.4）。
- 「段落入口 sub 在整段期间驻留栈上」**待真机确认**（本作因 `async` 的存在更不成立）。

### 10.5 ⚠️ 本作特有实现注意：`async` 与线程选择

- 本作段落子脚本**大量经 `async`(0x0F) 起新线程**（§2.4 的离线实体证据：`@MainSub00() async;`）。这些线程挂在**同一宿主 H** 的线程链 `H+0x1030` 上，**不在** `main` 那个线程里。
- ⇒ 若只读 `T = *(u32*)(H+4)`，读到的可能是**本帧最后执行过的 async 线程**（`H+4` 每跑一个线程就刷新一次，见 §5.3），语义不稳定。
- ⇒ **推荐读 `T = *(u32*)(H+0x1030)`**（链表首节点 = 内嵌主线线程 `H+8`），语义稳定；若希望覆盖 async 落点（口径要求「含 async」），可**遍历 `H+0x1030` 起的线程链**（节点 `{+0 线程, +4 next}`），对每个线程的 `T+4` 都做一次 latch 判定。**两种取法都需真机确认**（§13）。

---

## 11. 对总括 §5 与 `th11_ecl_re_requests` 的回填建议

> 本报告**未修改**任何既有文件（避免与并行任务写冲突）。建议由本迭代的收口任务按下列内容回填。

### 11.1 总括 `docs/logger/logger_re_disasm_overview.md` §5.1 的 th11 行

| 列 | 建议填入 |
| --- | --- |
| 已有 ECL 解释器命中点（寄存器） | `0x45D2A3`（2B，`Edi` = 线程 T）；解释器入口 `FUN_0045B860`、分发头 `0x45B890` |
| 命中点已知字段 | `*(u32*)(T+4)` = **当前指令地址**（**不是索引**）；`*(f32*)(T+0)` = 当前 ECL 时间；`*(u32*)(T+0x1014)` = **宿主 H** |
| ECL 数据区链 | `GetMemAddr(0x4A8D7C, 0x64, 0xC)` = `res + 0xC`（`res = *(*(0x4A8D7C)+0x64)`）——**原值正确，可直接用** |
| sub 名/索引线索 | ✅ `res+0x8C` 表（项 8B，`+0`=名字 `char*`，`+4`=起始，首指令 `+4+0x10`）；`count = res+8`；按名查表 `FUN_0045DB10` |

并新增两条注释：
- **`[执行对象+4]` 语义矛盾**（总括 §5.1 注记 / §9 风险 4）应改为：**th11 / th13 / th14 = 指令指针**；**th15/16/17/18 = 索引**。两者不可互搬。
- th11 的**宿主→线程→ip** 链与 th15 不同（`H+0x1030` / `H+0x102C` / `T+4`），**不得套用 C 代偏移**。

### 11.2 总括 §5.2 接线锚点表 th11 行

保持现值即可（`th11_life_dec 0x4327F0`(6B) / `th11_bomb_dec 0x4311E6`(5B) / `th11_bomb_dec2 0x431293`(5B) / `th11_game_start 0x41FA6D`(3B) 均未复核，本轮未触及）。**新增建议**：把「主线宿主捕获」补进 §5.2 或新增一列 —— `th11` 捕获点 `0x4204A3` / `0x4205E0`（`EAX` = 宿主）。

### 11.3 `docs/th11/th11_ecl_re_requests.md`

| 章节 | 建议 |
| --- | --- |
| §1 表格「ECL 解释器命中点」 | 保留地址，补注「位于『时间未到→累加』尾部，非分发头」；新增行「解释器入口 `FUN_0045B860`」「分发头 `0x45B890`」「主线宿主捕获 `0x4204A3`/`0x4205E0`」 |
| §1 表格「sub 名/索引线索」 | 由 ❌ 改为 ✅（`res+0x8C`，项 8B，首指令 `+4+0x10`） |
| §2 Q1 | 答：**指令地址**（非索引），`ip = *(u32*)(T+4)`；写入点见本报告 §2.3。 |
| §3 Q2 | 答：`res = *(*(0x4A8D7C)+0x64)`；`table = res+0x8C`；项 8B；`+0`=名字、`+4`=起始、首指令 `+4+0x10`；加载器 `0x45D900`，查表 `0x45DB10`，访问器 `0x40D070`/`0x40D0A0`/`0x40D0B0`/`0x4106D0`。 |
| §4 Q3 | 答：**有**显式栈；基址 `T+8`，SP `T+0x1008`；⚠️ 帧/实参布局与遍历步长未完全定案（本判定不使用）。 |
| §5 Q4 | 答：`FUN_004108F0` 返回宿主；站点 `0x4204A3`/`0x4205E0`（`"main"`@`0x494258`）；静态兜底 `*(u32*)(*(u32*)(mgr+0x68))`。 |
| §6 Q5 | 答：`0x45D2A3`（2B EHOOK，`D9 1F`，`Edi` = 线程）；函数 `FUN_0045B860`；跳表 `0x45D2C8`/映射 `0x45D3BC`；驱动器 `0x45D420`。 |
| §7 Q6 | 答：**运行期保留名字**；但因是地址语义，需 `table[i].+4+0x10` 建区间表反查。 |
| §8 Q7 | 答：`0x4A8ECC + 0x38/+0x3C`（机体/子机，thprac 既有）；难度 `0x4A5720`。 |
| §9 Q8/Q9 | 答：Q8 不影响、分支关亦成立；**Q9 = 路过式 checkpoint（latch）**；⚠️ 注意 `async` 线程选择。 |
| §10 真机验证清单 | 追加本报告 §13 的条目。 |

---

## 12. 推荐实现摘要（供接线）

```cpp
// ── 初始化：捕获主线宿主 ─────────────────────────────────────────────
EHOOK_DY(th11_ecl_host, 0x4204A3, 5, { mEclHost = pCtx->Eax; self->Disable(); });
EHOOK_DY(th11_ecl_host2, 0x4205E0, 5, { mEclHost = pCtx->Eax; self->Disable(); });
// 清理：th11_game_start(0x41FA6D) 里 mEclHost = nullptr;（沿用既有 EHOOK）

// ── 每帧（IGI 渲染时，帧间稳定态）读取「当前段落」────────────────────
if (!mEclHost) { section = "main"; }
else {
    auto mgr   = *(uint32_t*)0x4A8D7C;                 // 兜底/完整性校验用
    auto res   = *(uint32_t*)(mEclHost + 0x102C);      // ≡ *(uint32_t*)(mgr + 0x64)
    if (!res) { section = "main"; }
    else {
        auto count = *(int32_t*)(res + 8);
        auto table = *(uint32_t*)(res + 0x8C);
        auto T     = *(uint32_t*)(mEclHost + 0x1030);  // ★ 主线线程（= H+8）；勿用 H+4
        if (!table || count <= 0 || !T) { section = "main"; }
        else {
            uint32_t ip = *(uint32_t*)(T + 4);         // 当前指令绝对地址；0 = 线程已结束
            // ip → sub（§10.3 区间表，需按 start_i 排序）→ 白名单；
            // 命中则置当前段落，未命中 no-op（不回退 main）
            // ⚠️ 如需覆盖 async 落点：遍历 (H+0x1030) 线程链，对每个线程各判一次
        }
    }
}
```

**EHOOK 覆盖长度**：`0x4204A3` / `0x4205E0` 处下一条指令均为 `call 0x41A6D0`（5B，`E8 xx xx xx xx`）⇒ **5B EHOOK** 合适。
（`0x45D2A3` 的 2B EHOOK 已由既有 `mTimeLock` 占用，本迭代**不要复用它**。）

---

## 13. 真机验证清单

> 本机未运行 th11，以下**全部未做**；状态一律为「静态定案（🟡 已定案、未真机三态）」。

- [ ] **宿主捕获**：在 `0x4204A3`/`0x4205E0` 抓 `EAX` 非空；进关后 `H+0x1030`、`H+0x102C` 均可解引用，且 `H+0x102C == *(u32*)(*(u32*)0x4A8D7C + 0x64)`。
- [ ] **Q1 地址语义**：道中 / boss / 不同段落各采样一次，`NameOfAddress(T+4)` 与画面进度一致；`T+4 == 0` 只在线程结束（过关/退关）时出现。
- [ ] **地址→sub 反查**：同一段落内 `T+4` 应始终落在**同一个** `[start_i, start_{i+1})` 区间内；跨段落时切换。
- [ ] **路过置值**：经过各段入口 sub 时当前段落被正确置值；段内其余 sub 不改变当前段落（no-op）。
- [ ] **⚠️ `async` 线程选择（本作重点）**：含 `@MainSubNN() async` 的段落里，比较「读 `H+0x1030`」与「读 `H+4`」两种取法的读数稳定性；确认前者恒为 `H+8`、后者会被 async 覆盖。确定 latch 应采用「只读主线线程」还是「遍历线程链」。
- [ ] **⚠️ stage 4 两条路线（A / B）各走一遍**：分支选择（`stage4c00{a,b,c}` / `stage4c01{a,b,c}`）前后，`T+4` 的名字与白名单映射均正确；分支切换瞬间不出现误判。
- [ ] **多文件并集表**：确认分支/boss 脚本的 subs（`BossCard*`）确实进入同一张 `res+0x8C` 表，且 `table[i].+4+0x10` 落在其所属文件缓冲内。
- [ ] **Q7 待实测**：`*(u32*)(*(u32*)0x4A8ECC + 0x38/0x3C)`（机体/子机）取值与所选机体/子机一致；难度 `*(i32*)0x4A5720` 与所选难度一致。
- [ ] **练习跳章**：跳章后 `T+4` 名字与白名单置值均正确（Q8 复核）；`ins[0xA]=0xFF` 的难度掩码在 E/N/H/L 四难度下均能执行（否则 `goto` 会被 `test byte [T+0x101C], cl` 跳过 —— **这是最需要真机确认的一条**）。
- [ ] **st4 boss 段（`ECLSatoriJump`）**：`ecl.SetFile(2/3/…)` 选槽后 `ECLJump` 生效，且监控读数不受影响。
- [ ] **采样时机**：置于帧间稳定态（IGI 渲染），避免局部/表达式栈占用 `T+0x100C` 时误读栈。
- [ ] **attract demo / 重开**：demo 与菜单态下 `mEclHost` 是否为空、是否出现误读（`H+0x25BC == 20512` 判据是否足以排除 demo）。⚠️ 注：本报告**未**对 `H+0x25BC == 20512` 做新的独立验证，沿用 thprac 既有判据。
- [ ] **变体**：Steam / 汉化版是否共用同一 RVA（本报告地址来自原版 `ts=1217734394`）。

---

## 14. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档（本报告）。静态定案 Q1–Q9。**Q1** `[T+4]` = **当前指令绝对地址（不是 sub 索引）**，与 th13 同构；写入点 `0x4113C9`/`0x45B814`/`0x45D283`/`0x45B8E9`/`0x45BAC2`/`0x45D2B4`/`0x40FEE1`。**Q2** `mgr=*(u32*)0x4A8D7C` → `res=*(mgr+0x64)` → `count=res+8` → `table=res+0x8C`（项 8B，`+0`=名字 `char*`，`+4`=数据基址，首指令 `+4+0x10`），访问器 `0x40D070`/`0x40D0A0`/`0x40D0B0`/`0x4106D0`，加载器 `FUN_0045D900`（SCPT `0x54504353`，`.ecl` 头布局实测吻合），按名二分 `FUN_0045DB10`，与 th13 逐字段一致（仅 mgr 全局 `0x4C2188→0x4A8D7C`、`res` 偏移 `0xAC→0x64`）。**Q3** 显式栈存在（基址 `T+8`、SP `T+0x1008`、`ret` 固定回退 8B、压帧 `FUN_0045B610`），⚠️ 帧/实参布局与遍历步长未完全定案。**Q4** 宿主工厂 `FUN_004108F0` 返回 `EAX`=H，站点 `0x4204A3`/`0x4205E0`（`"main"`@`0x494258`），宿主 vtable `0x494074`、`H+0x102C=res`（`0x4113BB`）、`H+0x1030`=线程链头、`H+4`=当前线程（复位 `FUN_0040FED0`）、静态兜底 `*(u32*)(*(u32*)(mgr+0x68))`（节点 `H+0x11A4`）。**Q5** 解释器 `FUN_0045B860`（`EAX`=T）、分发头 `0x45B890`、跳表 `0x45D2C8`/映射 `0x45D3BC`（阈值 `0x58`）、驱动器 `FUN_0045D420`；既有 hook `0x45D2A3`（2B `D9 1F`，`Edi`=线程T）位于「时间未到→累加」尾部。**Q6** 运行期保留名字（需地址区间反查）。**Q7** `0x4A8ECC+0x38/+0x3C`（机体/子机，thprac 既有，🟡）、难度 `0x4A5720`。**Q8** `ECLJump` 只写 `goto`(`0x0C`)（case `0x45BAC2`，只改 `T+0`/`T+4`）；⚠️ 分支关（st4 的 6 份 `stage4c0*`）天然兼容，记入验证清单。**Q9** 路过式 checkpoint（latch）——经过白名单函数即置值、不在表内 no-op（不回退 `main`）；每 checkpoint 登记 1 个入口 sub（含 async）；⚠️ 本作 `async` 落点需注意线程选择（推荐读 `H+0x1030`）。**订正 requests §1 / 总括 §5.1**：`0x45d2a3` 非分发头；`[Edi+0x1014]` = 宿主 H 自身（不是"某个管理器"）、`+0x103c+0x1580` = `H+0x25BC` 宿主 flags；数据区链旧值 `GetMemAddr(0x4a8d7c,0x64,0xC)` **正确可直接用**；th11「sub 名/索引线索」由 ❌ 更正为 ✅。证据：`capstone` 只读反汇编（自写 `D:\workshop\reasm\th11\dl.py`）+ 全程序字节/交叉引用扫描 + `../exes/ecl/th11/*.ecl` 离线实体对账（`stage01.ecl` 的 `main`/`MainSubNN`/`@…async` 与 `stage4c0*` 分支脚本）。**未真机三态；段落入口白名单待枚举。** |

# TH12 ECL 段落监控 — 逆向结论（Q1–Q9）

> 对应需求：`docs/th12/th12_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`；结构模板与方法论参照：`docs/th13/th13_ecl_re_report.md`（同代、同样「无现成 hook、从零找」）。
> 二进制：`../exes/th12.exe`，737280 B，PE `TimeDateStamp = 1251344348`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**）。`.text` = VA `0x401000`–`0x496AD5`，RVA `0x1000`–`0x96AD5`，`SizeOfRawData = 0x96C00`，与 `thprac_launcher_games_def.h` 的 th12 ORIGINAL 条目一致。
> 引擎代：**B 代**；同代参照：th13（已定案报告）、th10/th11（仅 requests）。
> 方法：`pefile` + `capstone 5.0.7` 只读反汇编（自写脚本 `D:\workshop\reasm\th12\dl.py`，由 th13 的 `dl.py` 复制改造），用「原始字节搜索 + 函数入口整段反汇编 + 跳表/字段偏移交叉印证」定位（避开 `.text` 线性反汇编失步，见总括 §7.6）。
> ECL / decl 素材：`../exes/ecl/th12/`（原始 `.ecl` 9 份）、`../exes/ecl_dump/th12/`（`thecl -d` 反编译 `.decl` 9 份），清单与来源见 `docs/logger/decompile_materials.md`。
> 状态：**静态定案（🟡 已定案、未真机三态）**。本机无法运行 th12，**未做任何真机验证**。
> 地址口径：正文以 **VA** 为主，并标注 **RVA = VA − 0x400000**。
> 置信度图例：✅ 反汇编直接证实 ｜ 🟡 有强证据但缺一环（显式标注缺口）｜ ❌ 未能定案。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前 sub = 索引？指令指针？** | ✅ **是「指令地址（指针）」**，**不是 sub 索引** | `ip = *(u32*)(T+0x004)`（T = 执行对象/线程），指向 ECL 文件缓冲内的**当前（下一条待执行）指令绝对地址**；`0` = 线程结束。**本作不存在 subIdx 这一层**（与 th15–th18 的 `thread+4 = 索引` **不同**；与 **th13 / th14 同类**）。 |
| **Q2 sub 表** | ✅ **确认（与 th13 逐字段同构）** | `mgr = *(u32*)0x4B43DC`；`res = *(u32*)(mgr+0x64)`；`count = *(i32*)(res+0x08)`；`table = *(u32*)(res+0x8C)`；项 **8B**，`+0` = 名字 `char*`、`+4` = sub 数据基址（**首指令 = `+4 + 0x10`**）。加载器 `FUN_004692E0`（SCPT 魔数 `0x54504353`）；按名查表 `FUN_004694F0`（二分）。 |
| **Q3 函数栈** | ✅ **存在可遍历显式子调用栈** | 栈基址 `T+0x08`，栈指针 `SP = *(u32*)(T+0x1008)`（`0` = 空栈）；**帧长 `0x0C`**；帧内 `+0x00` = 旧 SP / `+0x04` = 时间 / **`+0x08` = 调用者指令地址**。压帧 = `call`(opcode `0x0B`) 处理器 `FUN_00466F00`；弹帧 = `ret`(opcode `0x0A`) @ `0x4671FF`。 |
| **Q4 主线宿主捕获点** | ✅ **给出捕获点** | 宿主工厂 `FUN_00412990`（`malloc(0x27B8)` + `FUN_00413230` 构造），**返回 `EAX` = 宿主 H**；「spawn main」两站点：`call 0x412990` @ **`0x4228B9`** 与 @ **`0x4229CD`**（均在 `FUN_004227C0`）⇒ 下一条指令 **`0x4228BE`** / **`0x4229D2`**（各 5B）可挂 EHOOK；`"main"` @ `0x49FE14`（全程序唯一引用点）。宿主工厂内 `T+4 = FindSubByName(res,"main")` @ `0x413448`。① 静态兜底：宿主链头 = `mgr+0x68`，节点 = `H+0x12C0`（**节点 `+0` = H**）⇒ **`H = *(u32*)(*(u32*)(mgr+0x68))`**。 |
| **Q5 解释器命中点** | ✅ **确认（全新自找）** | **`0x4671A0`**（`8B 77 04` + `8A 46 0A`，可挂 **6B** EHOOK）：`mov esi,[edi+4]` / `mov al,[esi+0xa]`；**命中寄存器 `EDI` = 执行对象 T**；所在函数 = **`FUN_00467170`**（`0x467170`/RVA `0x67170`）= **ECL 主解释器**（每帧逐指令步进），体 `0x467170`–`0x468C37`；opcode 跳表 `0x468C38`（62×4B）、字节映射 `0x468D30`（`0x5A` B）。 |
| **Q6 名字来源** | ✅ **运行期保留名字** | `table[i].+0` 为 `char*`，指向**已加载 ECL 缓冲内的名字串**（加载器 `FUN_004692E0` 在 `0x46937E` 填入）；访问器 `FUN_0040E8F0`。但因 Q1 是地址语义，取「当前 sub 名」需**地址 → sub 反查**（用 `table[i].+4+0x10` 建区间表排序后二分/线性扫）。 |
| **Q7 机体 / 子机** | ✅ 地址有效（thprac 既有） | `CHARA = 0x4B0C90`（xref 72）、`SUBSHOT = 0x4B0C94`（xref 51）、`STAGE_NUM = 0x4B0CB0`（73）、`MODEFLAGS = 0x4B0CE0`（57）、`PLAYER_PTR = 0x4B4514`（116）、`REPLAY_MGR_PTR = 0x4B4518`（31）。语义仍待真机核对。 |
| **Q8 ECLJump** | ✅ **不影响判读** | 练习补丁只把 `goto`(`opcode 0x0C`) 写进运行期缓冲；`goto`(case @ `0x4673FC`) **只改 `T+0`(时间) 与 `T+4`(ip)**，不触碰 sub 表、不触碰「当前 sub」的任何旁路字段。⚠️ **更正**：requests 里的 `ECLCallSub` 在 th12 源码中**从未被调用（死代码）**，且其写出的 6 个 dword 与真实 `call` 指令头不符（`size` 会落到 `+6` = 0）⇒ **不可用于 th12**。 |
| **Q9 段落判定口径** | ✅ **路过式 checkpoint（latch）**，但**读取面扩大** | CSV 即白名单：**宿主线程链上任一线程**的当前 sub（经 Q2 地址区间表把 `T+4` 映射回名字）∈ 表 → 置当前段落；**不在 → no-op**（不回退 `main`）。**⚠️ 与 th13 的实质差异**：th12 的段落 sub 由 `@MainSubNN() async`（**opcode `0x0F`**）**派生为独立线程**，主线线程的 `T+4` 全程停留在 `main` ⇒ **只读主线线程无效，必须遍历宿主线程链 `H+0x1030`**。**每 checkpoint 只登记 1 个入口函数**（含 async 落点）。**✅ 段落入口白名单已枚举完毕（2026-10-17）**：`docs/logger/th12_checkpoints.csv` 第 1 列 **114/114**（章节 43/43、boss 71/71）；入口表小结见 `docs/th12/th12_checkpoint_entry_report.md`（§10.4 收口）。 |

> **关键地址（VA / RVA）**：
> ECL 主解释器 `FUN_00467170` = `0x467170`/`0x67170`（体 `0x467170`–`0x468C37`）；
> **命中点 `0x4671A0`/`0x671A0`（6B，`EDI`=T）**；循环回跳 `0x468C04`/`0x68C04`；逐指令推进 `0x468BEA`–`0x468BF3`/`0x68BEA`；
> opcode 跳表 `0x468C38`；字节映射 `0x468D30`；默认/委派 `0x468BD9`（对 `[T+0x1014]` 的虚调用）；线程结束 `0x468C24`/`0x468C2A`；
> `ret`(0x0A) `0x4671CD`/`0x671CD`；`call`(0x0B) `0x46734A`/`0x6734A` → `FUN_00466F00`；`goto`(0x0C) `0x4673FC`/`0x673FC`；**`spawn`(0x0F) `0x46724C`/`0x6724C` → `FUN_00469110`**；
> 线程操作 opcode `0x10`–`0x15` = `0x46726F` / `0x46729C` / `0x4672C3` / `0x4672ED` / `0x467318` / `0x46725F`；
> 压帧 + 按名解析 `FUN_00466F00` = `0x466F00`/`0x66F00`（按名解析在 `0x4670F9`–`0x467122`）；
> 异步建线程 `FUN_00469110` = `0x469110`/`0x69110`（`malloc(0x1024)` 新 T + `malloc(0xC)` 节点，插入 `H+0x1030` 链）；按 id 找线程 `FUN_004691B0` = `0x4691B0`；
> 宿主/线程 VM 驱动器 `FUN_00468D90` = `0x468D90`/`0x68D90`（`0x468DB0: mov [H+4], T` → `call 0x467170`）；
> 宿主工厂 `FUN_00412990` = `0x412990`/`0x12990`；宿主构造 `FUN_00413230` = `0x413230`/`0x13230`（this = `EDI`）；宿主/线程复位 `FUN_00411E40` = `0x411E40`/`0x11E40`；
> `"main"` @ `0x49FE14`/`0x9FE14`；spawn 站点 `0x4228B9`（下一指令 `0x4228BE`）/ `0x4229CD`（下一指令 `0x4229D2`）；
> SCPT 加载器 `FUN_004692E0`/`0x692E0`；按名查表 `FUN_004694F0`/`0x694F0`；
> 访问器 `GetSubName` `FUN_0040E8F0`/`0xE8F0`、`GetResCount` `FUN_0040E920`/`0xE920`、`GetRes` `FUN_0040E930`/`0xE930`。

---

## 1. 统一指针链（TH12 定案）

```text
mgr   = *(u32*)0x4B43DC                              // 全局管理器（重引用 168 处；GetRes/GetResCount 均经它）
res   = *(u32*)(mgr + 0x64)                          // ECL 资源对象（GetRes @0x40E930）
count = *(i32*)(res + 0x08)                          // sub 总数（GetResCount @0x40E920）
table = *(u32*)(res + 0x8C)                          // sub 表（项 8B）
name_i  = *(char**)(table + i*8)                     // 项 +0 = 名字 char*
start_i = *(u32*)(table + i*8 + 4) + 0x10            // 项 +4 = sub 数据基址 ⇒ 首指令 = +4+0x10

// ── 主线宿主（进关时由 0x4228BE / 0x4229D2 抓到 EAX）────────────────────
H     = 主线 ECL 宿主对象（`malloc(0x27B8)`）
H+0x000  = vtable = 0x49FC50（宿主构造器 `FUN_00413230` @ `0x413247: mov [edi], 0x49FC50`；
                             另：`FUN_00469570`/`0x411E87` 处的线程类对象置 vtable 0x49FC34）
H+0x004  = T（当前线程；由 FUN_00411E40 置为 H+8，并每帧被驱动器刷新）
H+0x008  = T 本体（内嵌线程；`T = H + 0x08`）
H+0x102C = res                                       // 宿主持有的 ECL 资源（= *(mgr+0x64)）
H+0x1030 = T（线程链表头节点；+0x1034 = next，+0x1038 = prev）
H+0x12C0 = 宿主链表节点 { +0 = H, +4 = next, +8 = prev }；链表头 = `mgr+0x68`、尾 = `mgr+0x6C`、计数 = `mgr+0x70`、id 计数 = `mgr+0x74`
H+0x278C = H（自指针；供扩展指令处理器读回宿主）
H+0x27B4 = 宿主 id（工厂从 `mgr+0x74` 递增分配）

// ── 线程 T（主线线程 = H+8；异步派生线程 = FUN_00469110 单独 malloc(0x1024)）──
T+0x000  = f32  当前 ECL 时间（跨帧累积）
T+0x004  = u32  当前（下一条待执行）ECL 指令的**绝对地址**；0 = 线程结束   ★Q1
T+0x008  = u32  调用栈基址（帧数组起点）
T+0x1008 = i32  调用栈指针 SP（字节偏移，0 = 空栈）
T+0x100C = i32  第二指针（局部/表达式栈，推测；与 0x1008 共用基址 T+8）
T+0x1010 = i32  线程 id / handle（主线线程 = −1；异步线程由 spawn 传入）
T+0x1014 = u32  指令处理器对象（主线线程 = H 自身；异步线程 = 派它的宿主 H）
T+0x1018 = i32  0
T+0x101C = u8   指令选择掩码（难易度位掩码；供 `test byte [T+0x101C], ins[0xA]`）
T+0x1020 = u32  线程标志位（opcode `0x12`/`0x13` 置/清 bit0；推测「暂停/锁定」）
```

**与 th13 模板的差异（重要，不可照搬）**

| 项 | **th13** | **th12（本作）** |
| --- | --- | --- |
| 宿主大小 | `0x52E4` | **`0x27B8`** |
| 宿主 → 线程 T | `H+0x11D0`（≡ `H+4`） | **`H+0x1030`**（≡ `H+4`），线程链头节点内嵌于此 |
| 宿主 → res | `H+0x11CC` = res | **`H+0x102C` = res**（两者均 ≡ `*(u32*)(mgr+… )`） |
| 全局管理器字段 | `mgr = *(0x4C2188)`，`res = *(mgr+0xAC)`，宿主链头 `mgr+0xB0` | **`mgr = *(0x4B43DC)`，`res = *(mgr+0x64)`，宿主链头 `mgr+0x68`** |
| 宿主链节点 | `H+0x14CC` | **`H+0x12C0`** |
| 当前 sub | `T+4` = **指令绝对地址**（无 subIdx） | **完全一致** ✅ |
| 调用栈基址 / SP / 帧长 | `T+0x08` / `T+0x1008` / `0x0C`（帧 `+0x08` = 调用者 ip） | **完全一致** ✅ |
| sub 表 | `res+0x8C`，项 8B，`+0`=名字 `+4`=起始，首指令 `+4+0x10` | **完全一致** ✅ |
| opcode 域 | `≤ 0x5D` 走跳表 | **`≤ 0x59`** 走跳表 |
| 跳表 / 字节映射 | `0x478C38` / `0x478D44` | **`0x468C38` / `0x468D30`** |
| 段落 sub 的进入方式 | `main` → **同步 `call`** `MainFront`/`MainLatter` → 同步 `call` `MainSubNN` | `main` → **`opcode 0x0F` 异步派生线程** `@MainSubNN() async`（见 §10） |

> ⇒ **th12 与 th13 是同一 ECL VM 骨架的两份实例**：线程/栈/表/opcode 语义逐字段一致，**只有「宿主对象层」的字段偏移分叉**。故总括 §5.1 的偏移不可跨作搬运，但**语义结论可搬运**。

---

## 2. Q1 —— `[T+4]` 是**指令地址（指针）**，不是 sub 索引

### 2.1 判据（解释器把 `[EDI+4]` 当指针用）

主解释器 `FUN_00467170` 的 prologue 与分发头把 `EDI` 当脚本上下文，取 `[EDI+4]` 作为**当前指令**再读其 opcode：

```asm
00467170  81ec20010000      sub   esp, 0x120
00467176  53                push  ebx
00467177  55                push  ebp
00467178  56                push  esi
00467179  57                push  edi
0046717a  8bf8              mov   edi, eax                  ; ★ EDI = T（执行对象，入参 EAX）
0046717c  8b4704            mov   eax, dword ptr [edi + 4]  ; eax = 当前指令地址
0046717f  33db              xor   ebx, ebx
00467181  3bc3              cmp   eax, ebx
00467183  0f849e1a0000      je    0x468c27                  ; ip == 0 → 线程结束（返回 −1）
00467189  db00              fild  dword ptr [eax]             ; 指令的「时间」是 int
0046718b  d907              fld   dword ptr [edi]             ; T+0 = 当前时间(float)
0046718d  ded9              fcompp
0046718f  dfe0              fnstsw ax
00467191  f6c401            test  ah, 1
00467194  0f85701a0000      jne   0x468c0a                  ; 时间未到 → T+0 += 帧增量后返回 0
0046719a  8d6b01            lea   ebp, [ebx + 1]
0046719d  8d4900            lea   ecx, [ecx]
004671a0  8b7704            mov   esi, dword ptr [edi + 4]  ; ★ 分发循环头：重新取当前指令
004671a3  8a460a            mov   al, byte ptr [esi + 0xa]  ; 指令 +0xA = 难度选择掩码
004671a6  84871c100000      test  byte ptr [edi + 0x101c], al
004671ac  0f84381a0000      je    0x468bea                  ; 掩码不匹配 → 跳过该指令
004671b2  0fbf4604          movsx eax, word ptr [esi + 4]     ; opcode（16 位）
004671b6  83f859            cmp   eax, 0x59
004671b9  0f871a1a0000      ja    0x468bd9                  ; > 0x59 → 委派给 [T+0x1014] 的虚方法
004671bf  0fb688308d4600    movzx ecx, byte ptr [eax + 0x468d30]  ; 字节映射表
004671c6  ff248d388c4600    jmp   dword ptr [ecx*4 + 0x468c38]  ; 跳表分发
```

`[EDI+4]` 被当作**指针**：读 `[ins+0]`（int 时间）、`[ins+4]`（opcode）、`[ins+6]`（size）、`[ins+0xA]`（掩码）—— 它**绝不可能是索引或偏移**，且**本作不存在「当前 sub 索引」这一层**（全程序对 `[T+4]` 的写入只有「绝对地址」一种形态，见 §2.2）。

> **指令头布局（由真实 `.ecl` 字节 + 上述指令流共同推出）**：
> `+0x00` = int 时间 ｜ `+0x04` = u16 opcode ｜ `+0x06` = u16 size ｜ `+0x08` = u16 浮点参数掩码 ｜ `+0x0A` = u8 难度掩码 ｜ **`+0x0B` = u8 参数个数** ｜ `+0x0C` = u32 局部槽数 ｜ **`+0x10` = u32 名字长度**（仅 `call`/`spawn`）｜ **`+0x14` = 名字串 / 参数区**。
> 实测样本（`stage01.ecl` 文件偏移 `0x1346C`，`@MainSub02() async`）：
> `3C 00 00 00 | 0F 00 | 20 00 | 00 00 | FF | 01 | 00 00 00 00 | 0C 00 00 00 | "MainSub02\0\0\0"`
> ⇒ 时间 60、opcode `0x000F`、size `0x20`、掩码 `0xFF`、参数个数 1、名字长 12。
> `goto`（`ECLJump` 写出 6 个 dword）与之吻合：`ins[+0x10]` = `dest−start`、`ins[+0x14]` = `at_frame`（见 §9.2）。

### 2.2 字段写入点（谁写 `T+4`）

| VA / RVA | 语义 | 片段 |
| --- | --- | --- |
| `0x411E51` / `0x11E51` | **宿主/线程复位**：`H+0xC` = 0（即 `T+4` = 0） | `xor edx,edx` → `mov [eax+0xc], edx`（`FUN_00411E40`） |
| **`0x413448` / `0x13448`** | **进关设主线 sub**：`T+4 = FindSubByName(res,"main")` | `mov edx,[edi+4]` → `mov [edx+4], eax` |
| `0x413EEA` / `0x13EEA`、`0x413FC0` / `0x13FC0` | **按名切 sub**（对 `[ebx+0x174C]` 的宿主线程） | `mov ecx,[esi+4]` → `mov [ecx+4], eax` |
| **`0x467122` / `0x67122`** | **`call`(0x0B) / `spawn`(0x0F) 按名解析**：`T+4 = FindSubByName(res, (char*)(ins+0x14))` | `mov ecx,[esi+4]` → `mov [ecx+4], eax` |
| `0x467409` / `0x67409` | **`goto`(0x0C) 重定位**：`T+4 = ins + ins[+0x10]` | `mov edx,[eax+0x10]` → `add edx,eax` → `mov [edi+4], edx` |
| `0x4671F9` / `0x671F9` | **`ret`(0x0A) 恢复调用者 ip** | `mov edx,[esi+eax]` → `mov [edi+4], edx` |
| **`0x468BF3` / `0x68BF3`** | **逐指令推进**：`T+4 += ins.size` | `movzx ecx, word[eax+6]` → `add ecx,eax` → `mov [edi+4], ecx` |
| `0x468C24` / `0x68C24` | **线程结束**：`T+4 = 0`（ebx 恒 0） | `mov dword ptr [edi+4], ebx` |
| `0x468DB0` / `0x68DB0` | **驱动器刷新宿主当前线程**：`H+4 = T` | `mov [edi+4], eax` |

### 2.3 「地址 → 当前 sub」的取法（因无 subIdx）

```cpp
// 用 sub 表建「起始地址 → 索引」区间表（一次性 / 每次 res 变化时重建）
//   start_i = *(u32*)(table + i*8 + 4) + 0x10
// 按 start_i 升序排序；当前 sub = 满足 start_i <= ip < start_{i+1} 的 i
// name = *(char**)(table + i*8)
```
> ⚠️ 表项按**名字字典序**（`FUN_004694F0` 二分）存放，**地址并不单调** ⇒ 必须显式排序。
> ⚠️ 越界/脏指针保护：`ip` 必须落在某 `[start_i, start_{i+1})` 内，否则视为「不在已知 sub」。`ip == 0` → 线程已结束。
> ✅ 优待条件：th12 的 `call`/`spawn` 都是**按名**解析（`FindSubByName` 返回 `table[i].+4+0x10`），故 `T+4` 与某个 `start_i` **精确相等**的情形占绝大多数；区间表只是兜底。

### 2.4 与总括 §5.1 遗留矛盾的裁定

总括 §5.1 的遗留矛盾「`[执行对象+4]` 到底是 sub 索引还是指令指针」在本作有明确答案：**th12 是「指令指针」**（**与 th13、th14 同类**）；**th15/th16/th17/th18 是「索引」**，两者不可互相搬运。

---

## 3. Q2 —— sub 表：`res+0x8C`，项 8B（与 th13 逐字段一致）

### 3.1 三个现成访问器（游戏自己写的，直接证明字段语义）

```asm
0040e8f0  8b888c000000   mov   ecx, dword ptr [eax + 0x8c]   ; eax = res → table
0040e8f6  8b04d1         mov   eax, dword ptr [ecx + edx*8]  ; table[idx].+0  → 名字 char*
0040e8f9  c3             ret                                  ; GetSubName(res, idx)
0040e920  a1dc434b00     mov   eax, dword ptr [0x4b43dc]      ; mgr
0040e925  8b4864         mov   ecx, dword ptr [eax + 0x64]    ; res = *(mgr+0x64)
0040e928  8b4108         mov   eax, dword ptr [ecx + 8]       ; res+8 → count
0040e92b  c3             ret                                  ; GetResCount()
0040e930  a1dc434b00     mov   eax, dword ptr [0x4b43dc]      ; mgr
0040e935  8b4064         mov   eax, dword ptr [eax + 0x64]    ; res = *(mgr+0x64)
0040e938  c3             ret                                  ; GetRes()
```

⇒ 三条链一次坐实：**`res = *(*(0x4B43DC)+0x64)`、`count = res+8`、`table = res+0x8C`、`table[i].+0 = char*` 名字**。

### 3.2 建表 / 加载（写侧）—— `FUN_004692E0`@`0x4692E0`

```asm
004692e4  8bf1              mov   esi, ecx                          ; esi = res（__thiscall）
004692e6  8b4604            mov   eax, dword ptr [esi + 4]          ; 文件槽号
004692ed  894c860c          mov   dword ptr [esi + eax*4 + 0xc], ecx ; res[+0xC+slot*4] = ECL 文件缓冲
004692f8  813953435054      cmp   dword ptr [ecx], 0x54504353        ; 魔数 "SCPT"
00469314  6683790401        cmp   word ptr [ecx + 4], 1              ; 版本 1
0046931d  0fb74806          movzx ecx, word ptr [eax + 6]            ; 头尺寸
00469329  8d6c0124          lea   ebp, [ecx + eax + 0x24]            ; 名字区 = buf+0x24+hdr
0046932d  0fb74010          movzx eax, word ptr [eax + 0x10]         ; 本文件 sub 数
00469331  014608            add   dword ptr [esi + 8], eax            ; res+8 += sub 数  ← count
00469335  8d7c8500          lea   edi, [ebp + eax*4]                 ; 名字串区 = 名字区 + n*4
00469347  e8fe3c0000        call  0x46d04a                           ; malloc(count*8)
0046934c  89868c000000      mov   dword ptr [esi + 0x8c], eax         ; res+0x8C = 新表  ← table
...
00469364  8b5604            mov   edx, dword ptr [esi + 4]
00469367  8b44960c          mov   eax, dword ptr [esi + edx*4 + 0xc]  ; 缓冲
0046936b  034500            add   eax, dword ptr [ebp]                ; 缓冲 + offset[i]
00469374  8944ca04          mov   dword ptr [edx + ecx*8 + 4], eax    ; table[i].+4 = 起始
...
00469378  8b868c000000      mov   eax, dword ptr [esi + 0x8c]
0046937e  893cc8            mov   dword ptr [eax + ecx*8], edi        ; table[i].+0 = 名字 char*（指向缓冲内名字串）
00469386  8a10              mov   dl, byte ptr [eax]                  ; 逐字节扫 NUL 推进到下一个名字
```
⇒ `res+0x8C` = 表；项 **8 字节**；`+0` = 名字 `char*`（指向**已加载缓冲内的名字串，运行期保留**）；`+4` = `buffer + offset[i]`（sub 数据基址）；`res+0xC+slot*4` = 文件缓冲数组。

### 3.3 按名查表 —— `FUN_004694F0`@`0x4694F0`（二分）

```asm
004694f0  53                push  ebx
004694f1  8b5808            mov   ebx, dword ptr [eax + 8]       ; count = res+8
004694fe  8ba88c000000      mov   ebp, dword ptr [eax + 0x8c]    ; table = res+0x8C
00469504  8b4c2414          mov   ecx, dword ptr [esp + 0x14]    ; name
00469515  8b54f500          mov   edx, dword ptr [ebp + esi*8]   ; table[mid].+0 = 名字
00469520  8a01              mov   al, byte ptr [ecx]             ; 逐字节 strcmp(name, table[mid].+0)
...
0046955d  c20400            ret   4
00469560  8b44f504          mov   eax, dword ptr [ebp + esi*8 + 4]  ; table[idx].+4
00469567  83c010            add   eax, 0x10                        ; ★ +0x10 ⇒ 该 sub 的首条指令
0046956b  c20400            ret   4
```
⇒ `FindSubByName(res, name) = table[idx].+4 + 0x10`（**与 th13 的 `FUN_004797D0` 逐指令同构**）。**`+0x10` 正是 sub 的前导 16 字节**（与 §2.1 指令头 `0x10` 一致）。

**调用点**：`0x413440`（进关绑 `"main"`）、`0x413EE0`/`0x413FB6`（按名切 sub）、**`0x467118`（VM 内：`call`/`spawn` 的按名解析，读 `ins+0x14`）**、`0x4695A5`/`0x4695C7`/`0x4695E5`/`0x469737`。

### 3.4 `GetMemAddr(0x4b43dc, 0x64, 0xC)` 的展开（与 thprac `ECLHelper` 对齐）

`thprac_games.h:574-578`：`GetMemAddr(addr, off, …) = GetMemAddr<R>(*(R*)addr + off, …)`（逐级**解引用后再加偏移**）。
⇒ `GetMemAddr(0x4b43dc, 0x64, 0xC) = *(u32*)(*(u32*)0x4b43dc + 0x64) + 0xC = res + 0xC`。
`thprac_games.h:653-661` 的 `SetBaseAddr` / `SetFile` 用 `*(u32*)(P + ord*4)` 取第 ord 个文件缓冲 ⇒ **`res+0xC` 正是加载器用的 `res+0xC+ord*4` 数组首址**（§3.2）。**完全吻合**：`THSectionPatch` 的 `SetBaseAddr` 一级对象就是 `res`。**requests §1 的原值正确、可直接用**。

---

## 4. Q3 —— ECL 子调用栈（**存在、可遍历**）

### 4.1 结论

1. **有显式可遍历的子调用栈**，随解释器自动维护，内嵌于线程 T：
   - **帧存储基址**：`base = T + 0x08`
   - **栈指针**：`SP = *(u32*)(T + 0x1008)`（int，**字节**偏移；`0` = 空栈/顶层）
   - **帧长**：**`0x0C`**（3 个 dword）
2. **帧内布局**（压帧顺序 = 地址升序）：`+0x00` = 旧 `SP`；`+0x04` = 调用时 `T+0`（时间，float）；**`+0x08` = 调用者当时的指令地址（= 调用者 `T+4`）**。
3. 「栈顶帧存的是**调用者 ip**」（不是调用者 sub 索引 —— 本作没有 subIdx）。当前指令仍是 `T+4`。
4. 栈区上限 **`0x1000` 字节**（每次压帧都校验 `SP+4 < 0x1000`）⇒ 物理上限 ≈ 341 帧。
5. `SP == 0` 分支会把首帧的「旧 SP」槽规范化（写哨兵，见 `0x466F2D`–`0x466F45`、`0x467072`），故遍历终点判据取 **`SP < 0x0C` 或旧 SP 为哨兵/0**。

### 4.2 `ret`（opcode `0x0A`）—— case @ `0x4671CD` / RVA `0x671CD`

```asm
004671cd  8d7708            lea   esi, [edi + 8]                 ; 栈基址 = T+8
004671d0  8bc6              mov   eax, esi
004671d2  e829250000        call  0x469700                       ; 清局部/表达式临时（FUN_00469700）
004671d7  399f08100000      cmp   dword ptr [edi + 0x1008], ebx  ; SP == 0 ?
004671dd  0f84411a0000      je    0x468c24                       ;   空 → T+4 = 0（线程结束）
004671e3  8b8600100000      mov   eax, dword ptr [esi + 0x1000]  ; SP
004671e9  83c0fc            add   eax, -4
004671ec  3bc3              cmp   eax, ebx
004671ee  7c0c              jl    0x4671fc
004671f0  898600100000      mov   dword ptr [esi + 0x1000], eax  ; SP -= 4
004671f6  8b1406            mov   edx, dword ptr [esi + eax]     ; 读 frame[+0x08]
004671f9  895704            mov   dword ptr [edi + 4], edx       ; ★ T+4 = 调用者 ip
004671fc  ... 弹时间 → T+0（帧 +0x04） ...
00467214  ... 弹旧 SP → T+0x1008（帧 +0x00） ...
```
⇒ 弹栈顺序 `ip → time → 旧SP`，**帧长 `0x0C`**，与 §4.3 的压栈严格对称。

### 4.3 压帧（`call` opcode `0x0B` / `spawn` 共用）—— `FUN_00466F00`@`0x466F00`

帧压栈体（`esi` = 目标线程 T，`edi` = 执行中的线程 T）：

```asm
00466f12  8b8608100000      mov   eax, dword ptr [esi + 0x1008]  ; 当前 SP
00466f22  89442418          mov   dword ptr [esp + 0x18], eax    ; 保存 SP0
00466f26  8d480c            lea   ecx, [eax + 0xc]
00466f29  85c0              test  eax, eax
00466f2b  751d              jne   0x466f4a
00466f2d  8bc8              mov   ecx, eax                       ; SP0 == 0 分支
00466f2f  8d6904            lea   ebp, [ecx + 4]
00466f32  81fd00100000      cmp   ebp, 0x1000
00466f38  7d0b              jge   0x466f45
00466f3a  89543108          mov   dword ptr [ecx + esi + 8], edx ; 压哨兵（旧 SP 槽）
00466f3e  83860810000004    add   dword ptr [esi + 0x1008], 4
...（参数拷贝循环，见下）...
0046703e  8b9608100000      mov   edx, dword ptr [esi + 0x1008]
00467044  bb04000000        mov   ebx, 4
00467078  8b8e08100000      mov   ecx, dword ptr [esi + 0x1008]
00467081  81fd00100000      cmp   ebp, 0x1000
00467089  89543108          mov   dword ptr [ecx + esi + 8], edx ; ★ 压 frame[+0x00] = 旧 SP
0046708d  019e08100000      add   dword ptr [esi + 0x1008], ebx
004670a8  8b0f              mov   ecx, dword ptr [edi]           ; T+0 = 调用者时间
004670aa  894c3008          mov   dword ptr [eax + esi + 8], ecx ; ★ 压 frame[+0x04] = 时间
004670c5  8b4f04            mov   ecx, dword ptr [edi + 4]       ; T+4 = 调用者 ip
004670ef  894c3008          mov   dword ptr [eax + esi + 8], ecx ; ★ 压 frame[+0x08] = 调用者 ip
```
⇒ 压栈 `{旧SP, 时间, 调用者ip}`，**帧长 `0x0C`**。

**参数拷贝循环**（`0x466F74`–`0x467034`）：以 `ins[+0xB]` 为**参数个数**，逐条从 `ins` 读参数（`+0` = 类型字节，`'f'`/`'g'` = 浮点走 `FUN_00468FE0`，否则走 `FUN_00468F70`），写入被调线程的局部/表达式栈 ⇒ 印证 `ins[+0xB]` 是参数个数（§2.1 头布局）。

### 4.4 遍历规则（推荐实现）

```cpp
uint32_t sp = *(uint32_t*)(T + 0x1008);          // 0 = 无 caller
int n = 0;
while (sp >= 0x0C && sp < 0x1000 && n < 256) {
    uint32_t callerIp = *(uint32_t*)(T + 0x08 + sp - 4);   // 帧 +0x08 = 调用者 ip
    if (!callerIp) break;
    int callerSub = SubOfAddress(callerIp);               // §2.3 的地址区间表
    if (callerSub < 0) break;
    ancestors[n++] = NameOf(callerSub);                   // table[callerSub].+0
    sp = *(uint32_t*)(T + 0x08 + sp - 0x0C);              // 帧 +0x00 = 旧 SP
}
```
- 只在指令边界（帧间稳定态，如 IGI 渲染）遍历。
- **⚠️ 本作注意**：段落 sub 多为 **async 派生线程**（§10.1），它们的「调用者」是派生它的那条指令，而它们各自有**独立的栈**；跨线程不构成调用链（见 §4.5 / §10.5）。

### 4.5 异步 `spawn`（opcode `0x0F`）—— 新线程，不是新栈帧

`FUN_00469110`@`0x469110` 是 opcode `0x0F` 的实现：**为新 sub 单独 `malloc(0x1024)` 出一个 T**，再 `malloc(0xC)` 一个链表节点插入宿主线程链：

```asm
00469113  6824100000        push  0x1024                        ; 新 T 对象
0046911a  e8cb380000        call  0x46c9ea                      ; malloc
00469128  899808100000      mov   dword ptr [eax + 0x1008], ebx ; T+0x1008 = SP = 0
0046912e  89980c100000      mov   dword ptr [eax + 0x100c], ebx
0046913a  6a0c              push  0xc                          ; 链表节点
0046913c  e8a9380000        call  0x46c9ea
00469147  d91e              fstp  dword ptr [esi]                 ; T+0 = 0.0f
00469149  898e10100000      mov   dword ptr [esi + 0x1010], ecx  ; T+0x1010 = 线程 id/handle
0046914f  89be14100000      mov   dword ptr [esi + 0x1014], edi  ; ★ T+0x1014 = 派它的宿主 H
00469155  895e04            mov   dword ptr [esi + 4], ebx       ; T+4 = 0
0046915b  8a8a1c100000      mov   cl, byte ptr [edx + 0x101c]    ; 宿主线程的选择掩码
00469161  888e1c100000      mov   byte ptr [esi + 0x101c], cl    ; ★ 复制掩码到新 T
00469167  8d8f30100000      lea   ecx, [edi + 0x1030]            ; ★ 宿主 H 的线程链头
0046916d  8930              mov   dword ptr [eax], esi           ; 节点+0 = 新 T
00469188  894104            mov   dword ptr [ecx + 4], eax       ; 头节点.next = 新节点
0046918b  894808            mov   dword ptr [eax + 8], ecx       ; 新节点.prev = 头节点
00469192  8b7f04            mov   edi, dword ptr [edi + 4]        ; edi = 派它的那条线程 T
00469196  8bc6              mov   eax, esi
00469198  e863ddffff        call  0x466f00                      ; ★ 压帧 + 按名解析（与 call 同款）
```
⇒ **`spawn` = 新线程对象 + 压帧 + 按名设 `T+4`，并把新线程挂到 `H+0x1030` 链上**。**没有任何「调用者帧」记录在线程链里**，「谁派生了它」只隐含在 `T+0x1014`（宿主）层面。**⇒ 跨线程不存在可遍历的调用链**（这也再次支持 Q9 的 latch 口径）。

---

## 5. Q4 —— 主线宿主捕获点

### 5.1 「spawn main」站点

字符串 `"main"` @ **`0x49FE14`**（RVA `0x9FE14`，全程序仅此一处且仅被下面两处引用）。
宿主工厂 `FUN_00412990`：`malloc(0x27B8)` → `push "main"` → `FUN_00413230(this=EDI=新对象)` 构造 → 末尾 **`mov eax, ebp; ret 8`（`0x412B79`）⇒ 返回 `EAX` = 宿主 H**。

```asm
; 站点 1（进关）
0042289f  6a50              push  0x50
004228a1  8d4c240c          lea   ecx, [esp + 0xc]
004228a5  53                push  ebx
004228a6  51                push  ecx
004228a7  e8744b0500        call  0x477420                      ; memset(参数结构, 0, 0x50)
004228ac  83c40c            add   esp, 0xc
004228af  8d542408          lea   edx, [esp + 8]
004228b3  52                push  edx
004228b4  6814fe4900        push  0x49fe14                      ; "main"
004228b9  e8d200ffff        call  0x412990                      ; ★ EAX = 主线宿主（下一条 0x4228BE 可挂 EHOOK）
004228be  e89dacffff        call  0x41d560                      ; （5B，EHOOK 覆盖对象）

; 站点 2（另一重开/复位路径，同型；同一函数 FUN_004227C0）
004229c3  8d442408          lea   eax, [esp + 8]
004229c7  50                push  eax
004229c8  6814fe4900        push  0x49fe14                      ; "main"
004229cd  e8befffeff        call  0x412990                      ; ★ EAX = 主线宿主（下一条 0x4229D2 可挂 EHOOK）
004229d2  e889abffff        call  0x41d560                      ; （5B，EHOOK 覆盖对象）
```

⇒ **Q4 答案**：捕获点 = **`0x4228BE`** / **`0x4229D2`**（各 **5B** EHOOK），命中时 **`EAX` = 主线 ECL 宿主 H**。（对应总括里 TH15 的 `0x43BBC8/0x43D48E/0x43D6BE` 角色。）

### 5.2 宿主工厂如何入链表（可作静态兜底）

宿主构造器末尾把自身登记为宿主链表节点：

```asm
00413323  8daf60260000      lea   ebp, [edi + 0x2660]
00413331  89bfc0120000      mov   dword ptr [edi + 0x12c0], edi   ; ★ 节点 H+0x12C0 的 +0 = H（自指针）
00413337  899fc4120000      mov   dword ptr [edi + 0x12c4], ebx   ; +4 = next = 0
0041333d  899fc8120000      mov   dword ptr [edi + 0x12c8], ebx   ; +8 = prev = 0
```
工厂的入链代码：
```asm
00412b44  397368            cmp   dword ptr [ebx + 0x68], esi     ; ebx = mgr, esi = 0
00412b49  894368            mov   dword ptr [ebx + 0x68], eax     ; mgr+0x68 = 头（空链时）
00412b4e  8b4b6c            mov   ecx, dword ptr [ebx + 0x6c]     ; mgr+0x6C = 尾
00412b58  895004            mov   dword ptr [eax + 4], edx        ; 节点.next
00412b5e  894208            mov   dword ptr [edx + 8], eax        ; 前驱.prev
00412b61  894104            mov   dword ptr [ecx + 4], eax
00412b67  017b70            add   dword ptr [ebx + 0x70], edi     ; mgr+0x70 = 宿主计数
00412b6a  89436c            mov   dword ptr [ebx + 0x6c], eax     ; mgr+0x6C = 新尾
00412b73  40                inc   eax
00412b74  894374            mov   dword ptr [ebx + 0x74], eax     ; mgr+0x74 = id 计数
```
取头访问器 `FUN_004124C0`：`mov eax,[0x4b43dc]; mov eax,[eax+0x68]; ret`；按 id 查宿主 `FUN_00412500`：`mov eax,[mgr+0x68]` → `mov edx,[eax]`（**节点 +0 = H**）→ `cmp [H+0x27B4], id`。
⇒ **静态兜底（无需钩子）**：`H = *(u32*)(*(u32*)(mgr + 0x68))`（链头，最早创建）或 `*(u32*)(*(u32*)(mgr + 0x6C))`（链尾，最新创建）。⚠️ `FUN_00412990` 共有 **10 个调用点**（`0x40F100`/`0x4156DC`/`0x4157AA`/`0x4158A3`/`0x415A56`/`0x41B08F`/`0x4228B9`/`0x4229CD`/`0x44A916`/`0x44AF56`），只有 `0x4228B9`/`0x4229CD` 传 `"main"`（其余传 `table[i].+0` 里的脚本名，如 `0x40F0F4` 处的按索引 spawn）——**推荐仍用捕获点**。

### 5.3 宿主 → 线程（`H+4` 与 `H+0x1030`）

宿主/线程复位 `FUN_00411E40`@`0x411E40`（由宿主构造链上的 `0x413ECE`/`0x413FA4` 调用）：

```asm
00411e40  d9ee              fldz
00411e42  83a028100000fe    and   dword ptr [eax + 0x1028], 0xfffffffe
00411e4b  d95808            fstp  dword ptr [eax + 8]           ; H+8 = 0.0f  ← T+0（时间）
00411e4e  8d4808            lea   ecx, [eax + 8]               ; ecx = H+8 = T
00411e51  89500c            mov   dword ptr [eax + 0xc], edx   ; H+0xC = 0   ← T+4（ip）
00411e54  89801c100000      mov   dword ptr [eax + 0x101c], eax  ; T+0x1014 = H（指令处理器 = 自身）
00411e5a  c78018100000ffffffff  mov dword ptr [eax + 0x1018], -1 ; T+0x1010 = −1（主线线程 id）
00411e64  899020100000      mov   dword ptr [eax + 0x1020], edx  ; T+0x1018 = 0
00411e6a  894804            mov   dword ptr [eax + 4], ecx     ; ★ H+4 = H+8（T）
00411e6d  898830100000      mov   dword ptr [eax + 0x1030], ecx  ; ★ H+0x1030 = T（线程链头节点 +0）
00411e73  899034100000      mov   dword ptr [eax + 0x1034], edx  ; H+0x1034 = 0（无后继）
00411e79  899038100000      mov   dword ptr [eax + 0x1038], edx  ; H+0x1038 = 0（无前驱）
00411e7f  c3                ret
```
⇒ **`T = H + 0x08`（内嵌）；`H+4 = T`；`H+0x1030 = T`（线程链头节点）**。宿主构造器 `FUN_00413230` 的等价片段在 `0x413260`–`0x413294`（并额外置 `H+0x102C = res` @ `0x41343A`、`H+0x278C = H` @ `0x4132A4`）。

每帧驱动器 `FUN_00468D90`@`0x468D90` 遍历线程链并刷 `H+4`：

```asm
00468d90  51                push  ecx
00468d91  53                push  ebx
00468d92  55                push  ebp
00468d93  56                push  esi
00468d94  8db730100000      lea   esi, [edi + 0x1030]          ; ★ 线程链头（edi = 宿主 H）
00468d9f  85f6              test  esi, esi
00468da3  8b06              mov   eax, dword ptr [esi]         ; eax = 节点+0 = T
00468da9  8b5e04            mov   ebx, dword ptr [esi + 4]     ; ebx = 节点+4 = next
00468db0  894704            mov   dword ptr [edi + 4], eax     ; ★ 刷新 H+4 = T
00468db3  e8b8e3ffff        call  0x467170                     ; ★ 跑一帧 ECL VM（EAX = T）
00468dbe  7554              jne   0x468e14
00468dff  8bf3              mov   esi, ebx
00468e01  85db              test  ebx, ebx
00468e03  759e              jne   0x468da3                     ; 下一个线程节点
00468e05  8d4708            lea   eax, [edi + 8]
00468e08  894704            mov   dword ptr [edi + 4], eax     ; H+4 复位为主线 T
```
`FUN_00468D90` 的调用点：`0x413C63`、`0x413F0C`（均在 `FUN_00413840` 内；`edi = *(u32*)(ebx+0x174C)`，`0x413EC8`/`0x413EFA` 实测为 `8B 83 4C170000` / `8B BB 4C170000`）、`0x43E916`（`edi = *(u32*)(esi+0x2DC)`）。⇒ 主线 ECL 宿主同时可从这些上层对象缓存读到，但**推荐仍用 `0x4228BE`/`0x4229D2` 捕获点**（上层对象布局未定论）。

---

## 6. Q5 —— 解释器命中点 `0x4671A0` 与寄存器语义 ★本次硬骨头

### 6.1 命中点

```asm
004671a0  8b7704            mov   esi, dword ptr [edi + 4]     ; ★ EHOOK(6B) 命中：esi = T+4（当前指令）
004671a3  8a460a            mov   al, byte ptr [esi + 0xa]      ; 指令难度选择掩码
004671a6  84871c100000      test  byte ptr [edi + 0x101c], al
004671ac  0f84381a0000      je    0x468bea
```
- **地址**：**`0x4671A0` / RVA `0x671A0`**；**覆盖 6 字节**（`8B 77 04` + `8A 46 0A`，正好两条完整指令）⇒ 可挂 **6B EHOOK**。
- **命中寄存器**：**`EDI` = 执行对象 T**（脚本上下文）；`T+4` = 当前（下一条待执行）指令地址；`T+0` = 当前时间。
- **语义**：ECL 主解释器的**逐指令分发循环头**（入口 `0x467170` 顺延至此；`0x468C04: je 0x4671a0` 为循环回跳）。

### 6.2 所在函数与调用链

- **函数**：`FUN_00467170`，入口 `0x467170`（RVA `0x67170`），函数体 **`0x467170`–`0x468C37`**（前后均有 `0xCC` 填充）。
- **入参**：**`EAX` = T**（`0x46717A: mov edi, eax`）；返回 `EAX`：`0` = 本帧到此为止（时间未到，已把帧增量加到 `T+0`）、`−1` = 脚本结束（`T+4` 置 0）。
- **opcode 分发**：`0x4671B2` 取 `(i16)[ins+4]`；`> 0x59` → `0x468BD9`（对 `[T+0x1014]` 的**虚调用**，等价于「扩展指令处理器」）；`≤ 0x59` → 经字节映射 `0x468D30` → 跳表 `0x468C38`。
- **映射核实（字节映射 → 跳表 → case）**：`0x0A`(ret) → idx 2 → **`0x4671CD`**；`0x0B`(call) → idx 3 → **`0x46734A`**；`0x0C`(goto) → idx 4 → **`0x4673FC`**；**`0x0F`(spawn) → idx 7 → `0x46724C`**；`0x10`–`0x15` → idx 8–13 → `0x46726F` / `0x46729C` / `0x4672C3` / `0x4672ED` / `0x467318` / `0x46725F`（后六个分别是「带参 spawn」「按 id 杀线程」「置线程位」「清线程位」等线程管理指令）。
- **调用链**：`FUN_00468D90`（遍历宿主线程链，`eax = T` 后 `call FUN_00467170`）→ 被 `0x413C63`/`0x413F0C`/`0x43E916` 调用（关卡/宿主更新路径）。

### 6.3 作为「每帧稳定触发点」的评价

- `FUN_00467170` 由 `FUN_00468D90` **每帧对宿主线程链上的每个活跃线程调用一次**；`0x4671A0` 在**每次执行/尝试执行一条指令**时命中。
- ⚠️ **局限（与 th13 完全相同）**：处于「时间未到、纯等待」时，函数会在 `0x467194` 就跳去 `0x468C0A` 累加时间并返回，**不经过 `0x4671A0`**。因此它**不是无条件每帧**。
- ⚠️ 它会**对所有脚本线程**命中 ⇒ 必须用 §5.1 捕获的 H 做过滤（`T` 是否在 `H+0x1030` 链上），否则会把敌方/异步线程也算进来。
- **推荐做法（与 th13/th15 同风格）**：**不挂 VM 钩子**。只在 `0x4228BE`/`0x4229D2` 抓一次 H，然后在 IGI 渲染（帧间稳定态）读线程链与 `T+4`。`0x4671A0` 作为「需要无条件每帧触发」时的备选。

### 6.4 ⚠️ 对 requests §6「建议切入点」的更正（重要）

requests §6 建议「先反汇编 `THSectionPatch` 用的 ECL 数据区加载点，找解释器函数」／「用 th10/th11 的 `0x1014` 链形态作参照，在 th12 里搜同构结构」。实测更短、更可靠的路径是：

1. **`THSectionPatch` 链先落地（10 分钟级）**：搜 SCPT 魔数 `0x54504353`（全程序 **1 处**，`0x4692FA`）⇒ 立刻得到加载器 `FUN_004692E0` 与 `res` 的字段语义（`res+8`/`res+0x8C`/`res+0xC+slot*4`）；再用 `mov eax,[0x4b43dc]; mov eax,[eax+0x64]; ret` 形状搜到 `GetRes`@`0x40E930` ⇒ `res = *(mgr+0x64)` 定案。
2. **宿主与解释器由「字符串 + 字段偏移」定位，而不是由数据区链定位**：`"main"` 是唯一引用 ⇒ 宿主工厂 `FUN_00412990` ⇒ 捕获点；再搜 `0x1008` 这个 disp32（79 处，**密集聚在 `0x466F00`–`0x469200`**）⇒ VM 与 spawn/驱动一族函数一次成型。
3. **解释器入口由「跳表分发」确认**：`0x4671B2` 的 `movsx eax,word[esi+4]` + `cmp eax,0x59` + `movzx ecx,byte[eax+0x468D30]` + `jmp [ecx*4+0x468C38]` 是唯一的 ECL opcode 跳表（th12 全域 `jmp [reg*4+disp]` 扫描中，只有这一处的表落在 `0x468C38` 且紧随 62 项之后正好接字节映射表 `0x468D30`）。
4. **th10/th11 的 `0x1014` 链不可直接搬**：th12 的 `[T+0x1014]` = **宿主自指针 H**（th13 同），而总括 §5.1 给 th10/th11 的判据是 `[Esi+0x1014]+0x103c+0x1444==1296` 形态（两级偏移）。本作 `H+0x103C` 是**普通计数字段**（宿主构造器 `0x413430: mov [edi+0x103c], 0`），th12 全域**不存在** `disp 0x1444` ⇒ 该「主线判据」在 th12 里另有形态，**未定论（本轮未追）**，见 §13。

---

## 7. Q6 —— 运行期保留 sub 名

✅ **保留**。加载器 `FUN_004692E0` 在 `0x46937E` 把**名字区指针**（已加载 ECL 缓冲内的名字串首址）直接写入 `table[i].+0`（见 §3.2）；`0x469386`–`0x469396` 逐字节扫 NUL 推进到下一个名字。故运行期 `name = *(char**)(table + i*8)` 直接可读，**无需离线名表**。

**旁证**：`main` 与各 `MainSubNN` 的名字串**就存在于 `.ecl` 缓冲里**（§2.1 实测字节），且 `spawn`/`call` 指令内嵌名字、运行期还要拿它做 `FindSubByName` ⇒ 名字**必须**保留在缓冲中。

> ⚠️ **但本作需要一个「地址 → i」的反查**（因为 Q1 是地址而非索引），见 §2.3。反查只需 `table[i].+4 + 0x10`，**不需要离线 `.ecl`**；离线 `.ecl` 只用于「段落入口白名单」本身。

---

## 8. Q7 —— 机体 / 子机 / 其它全局

| 名称 | VA / RVA | xref 数 | 说明 | 置信度 |
| --- | --- | --- | --- | --- |
| `CHARA` | `0x4B0C90` / `0xB0C90` | 72 | thprac 既有（`thprac_th12.cpp:39`） | ✅ 地址有效（语义待真机） |
| `SUBSHOT` | `0x4B0C94` / `0xB0C94` | 51 | thprac 既有（`:40`），**紧邻 `CHARA`** | ✅ 地址有效（语义待真机） |
| `STAGE_NUM` | `0x4B0CB0` / `0xB0CB0` | 73 | thprac 既有（`:42`） | ✅ |
| `MODEFLAGS` | `0x4B0CE0` / `0xB0CE0` | 57 | thprac 既有（`:41`） | ✅ |
| `PLAYER_PTR` | `0x4B4514` / `0xB4514` | 116 | thprac 既有（`:38`） | ✅ |
| `REPLAY_MGR_PTR` | `0x4B4518` / `0xB4518` | 31 | thprac 既有（`:43`） | ✅ |
| `ECL 管理器` | `0x4B43DC` / `0xB43DC` | 168 | **本报告新增**（Q2 入口） | ✅ |

> 说明：本迭代 IGI 只需显示「当前段落」，Q7 非必需；上表供后续日志迭代。**未真机取值核对，勿直接用于判据**。

---

## 9. Q8 —— 练习 `ECLJump` 对读取的影响

### 9.1 结论

✅ **不影响 sub 判读**（与 th13 §9 / th15 / th16 一致）。但 **th12 的段落入口结构特殊**，见 §10。

### 9.2 依据

1. **补丁写入**（`thprac_th12.cpp:997-1010`）：
   ```cpp
   void ECLJump (ECLHelper& ecl, unsigned start, unsigned ecl_time, int length)         { ecl.SetPos(start); ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << length << 0; }
   void ECLJumpEx(ECLHelper& ecl, unsigned start, unsigned dest, int at_frame, int t=0) { ecl.SetPos(start); ecl << t << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame; }
   ```
   6 个 dword = 24B（`size = 0x18`）：`+0` = `ecl_time`(int)、`+4` = `0x0018000C`（低 16 位 `0x000C` = **`goto`**，高 16 位 `0x0018` = size）、`+8` = `0x02FF0000`（`ins[0xA] = 0xFF` = 任意难度；`ins[8..9] = 0` = 无浮点参数）、`+0xB` = `0x02`（参数个数 2）、`+0xC` = 0（局部槽数 0）、`+0x10` = `dest−start`、`+0x14` = `at_frame`。
2. **`goto` 语义**（case `0x0C` @ `0x4673FC`/RVA `0x673FC`）：
   ```asm
   004673fc  8b4704            mov   eax, dword ptr [edi + 4]       ; 当前指令
   004673ff  db4014            fild  dword ptr [eax + 0x14]         ; ins[+0x14] = at_frame（int）
   00467402  d91f              fstp  dword ptr [edi]                ; ★ T+0 = at_frame（时间，绝对赋值）
   00467404  8b5010            mov   edx, dword ptr [eax + 0x10]    ; ins[+0x10] = dest−start
   00467407  03d0              add   edx, eax
   00467409  895704            mov   dword ptr [edi + 4], edx       ; ★ T+4 = ins + (dest−start)（绝对地址）
   ```
   **只改 `T+0`（时间）与 `T+4`（ip），从不触碰 sub 表，也不触碰任何「当前 sub」的旁路字段。**
3. **sub 结构未动**：`ECLHelper::SetBaseAddr` 定位的是 `res+0xC` 起的**运行期 ECL 文件缓冲**（§3.4）；`ECLJump` 只在硬编码偏移处覆写指令体，不触碰 SCPT 头、名字区、`res+0x8C` 表（表由 `FUN_004692E0` 一次解析生成）。
4. **瞬态注意**：`goto` 之后、目标 `spawn`/`call` 尚未执行的那一瞬，`T+4` 指向「跳转后位置」（仍是**旧 sub 内**的地址）；`spawn` 执行后目标段落进入**另一条线程**。采样落在帧间稳定态即可规避。

### 9.3 ⚠️ 对 requests §1「强线索」的更正：`ECLCallSub` 是死代码

requests §1 以 `ECLCallSub` 写出的 `0x000b` 指令为「call 内嵌 sub 名」的强线索。**结论：线索的结论正确，但载体不对**：

- `ECLCallSub`（`thprac_th12.cpp:1012`）在 th12 里**从未被调用**（全文件仅此定义，无调用点）⇒ **死代码**。
- 其布局也与真实 `call` 指令头**不符**：真实指令头是 `+4` = u16 opcode、`+6` = u16 size；而 `ECLCallSub` 把 `0x000B` 整个写在 `+4`（⇒ `size` 落到 `+6` = **0**），把 `sub_length+0x14` 写在 `+8`。照此写出的指令会被 VM 判为 size 0 ⇒ **不可用于 th12**（th13 的 `ECLCallSub` 类用法若有同样问题，需另行核对，超出本作范围）。
- **真实的「call 内嵌 sub 名」证据来自游戏自身脚本**：`.ecl` 里 `opcode 0x0B`（同步 call）与 `opcode 0x0F`（async spawn）都在 `+0x10` 写名字长度、`+0x14` 写名字串；VM 在 `0x4670F9`–`0x467122` 用 `FindSubByName(res, ins+0x14)` 解析。见 §10.1。

---

## 10. Q9 —— 段落判定口径：**路过式 checkpoint（latch）**（读取面＝宿主线程链）

### 10.1 结论

✅ **走「路过式 checkpoint（latch）」**：读**宿主线程链上任一线程**的当前 sub（本作经 `SubOfAddress(T+4)` 得到）→ **∈ 白名单则置当前段落；不在 → no-op**（保持上一个值，不回退 `main`）；本局首次命中前显示 `main`。CSV 即白名单表；**每 checkpoint 只登记 1 个入口函数**（含 async 落点）。

### 10.2 ★ 本作与 th13 的实质差异：段落 sub 是 **async 派生线程**

th12 的 `main` **不用同步 `call` 进入段落，而是用 opcode `0x0F` 把每个段落 sub 派生为独立线程**：

```text
// stage01.decl 的 main（截选）
void main()
{
    flagSet(32);
    enmMapleEnemy("MapleEnemy", 0, 0, 100, 1000, 0);
    @ShipShadowEnemy() async;        // opcode 0x0F
    @LogoEnemy()       async;        // opcode 0x0F
+60:
    nop();
    @MainSub00() async;              // opcode 0x0F
    wait(300);
    @MainSub01() async;
    wait(590);
    @MainSub02() async;              // ← warp 落点之一
    @MainSub03() async;
    ...
    @MainSub10b();                   // ← 无 async ⇒ opcode 0x0B（同步 call）
    ...
}
```
`.ecl` 实测（`stage01.ecl`，文件偏移 → opcode → 内嵌名）：

| 文件偏移 | opcode | 内嵌名 | 语义 |
| --- | --- | --- | --- |
| `0x133B0` | `0x000F` | `ShipShadowEnemy` | async 派生 |
| `0x133D4` | `0x000F` | `LogoEnemy` | async 派生（**stage1 各段的 `ECLJump` 写入点**） |
| `0x1346C` | `0x000F` | `MainSub02` | async 派生（**stage1 portion 2 的落点**） |
| `0x13548` | `0x000F` | `MainSub06` | （portion 3 落点） |
| `0x1369C` | `0x000F` | `MainSub10` | （portion 4 落点） |
| `0x136EC` | `0x000F` | `MainSub11` | （portion 5 落点） |
| `0x136BC` | `0x000B` | `MainSub10b` | **同步 call** |

⇒ **主线线程的 `T+4` 在整个关卡里几乎一直停在 `main`**（它只 spawn + `wait`）。**断言：th12 若照 th13 的读法（只读主线线程 `*(u32*)(H+0x1030)`），会永远读到 `main`** ⇒ 必须**遍历宿主线程链 `H+0x1030`**，对每个线程取 `T+4` 再映射为名字，任一命中白名单即置值。

### 10.3 判定流程（推荐实现）

```cpp
H = mEclHost;                                     // 0x4228BE / 0x4229D2 捕获
if (!H) return "main";
mgr   = *(u32*)0x4B43DC;
res   = *(u32*)(mgr + 0x64);                      // ≡ *(u32*)(H + 0x102C)
count = *(i32*)(res + 8);
table = *(u32*)(res + 0x8C);
if (!res || !table || count <= 0) return "main";
// 遍历宿主线程链：头节点内嵌于 H+0x1030，节点 {+0 = T, +4 = next, +8 = prev}
for (u32 node = H + 0x1030; node; node = *(u32*)(node + 4)) {
    u32 T  = *(u32*)(node);
    if (!T) continue;
    u32 ip = *(u32*)(T + 4);                      // 该线程当前指令绝对地址（Q1）；0 = 已结束
    if (!ip) continue;
    int idx = SubOfAddress(table, count, ip);     // §2.3 区间表
    if (idx < 0) continue;
    const char* cur = *(char**)(table + idx * 8);
    const char* section = Whitelist(game, stage, cur);   // 命中 → 置当前段落
    if (section) current = section;               // 未命中 → no-op（不回退 main）
}
```
> checkpoint 粒度 = 每行 1 个入口 sub（th12 即 `MainSubNN` 级）；**含 async 落点**；容器（`main`）不入表。

### 10.4 ✅ 段落入口白名单：**已枚举完毕（2026-10-17）**

> **收口**：本节原为「未定论 / 待枚举」。现已按下列方法**机械化枚举完毕**，成品 = `docs/logger/th12_checkpoints.csv` 第 1 列 **114/114**（章节 43/43、boss 71/71）；**入口表小结 + 本作特有机制（两个 warp API、boss 行口径、`ST6 BOSS9/BOSS10` 修正、实现影响、风险）= `docs/th12/th12_checkpoint_entry_report.md`**。以下「方法与抽验」保留为**可复现过程**。

**方法与已有的强证据（本次已抽验；E3/E4 已据此枚举完毕）**：

`THStageWarp`（`thprac_th12.cpp:1018`–`1804`）的每次调用形如：
```cpp
ECLJumpEx(ecl, <start>, <dest>, at_frame, ecl_time);   // 在 start 写 goto，跳到 dest
```
**实测规律（stage1 / stage2 抽验，全部命中）**：
- `<start>` 是 `main` 内一条**与本段无关的 early async-spawn 指令**（stage1 恒为 `0x133D4` = `@LogoEnemy() async`；stage2 恒为 `0x13108` = `@LogoEnemy() async`）——被 thprac **原地覆盖成 `goto`**。
- **`<dest>` 精确落在 `main` 内一条 `opcode 0x0F`（`@MainSubNN() async`）指令的首字节上** ⇒ **该 `MainSubNN` 即「段落入口函数」**。

抽验记录（文件偏移 → 内嵌名）：

| 关卡 / 段 | `<start>` | `<dest>` | `dest` 处指令内嵌名 |
| --- | --- | --- | --- |
| st1 p2 | `0x133D4` | `0x1346C` | `MainSub02` |
| st1 p3 | `0x133D4` | `0x13548` | `MainSub06` |
| st1 p4 | `0x133D4` | `0x1369C` | `MainSub10` |
| st1 p5 | `0x133D4` | `0x136EC` | `MainSub11` |
| st2 p2 | `0x13108` | `0x131B4` | `MainSub02` |
| st2 p3 | `0x13108` | `0x1323C` | `MainSub04` |
| st2 p4 | `0x13108` | `0x132E8` | `MainSub06` |
| st2 p5 | `0x13108` | `0x13360` | `MainSub07` |

⇒ **E3/E4 的枚举算法（可机械化）**：
1. 从 `thprac_th12.cpp` 的 `THStageWarp`/`THPatch` 抽出全部 `ECLJumpEx/Jump` 的 `(stage, portion, dest)`；
2. 按 stage → `.ecl` 文件（st1–6 → `stage0N.ecl`；st7 → `stage07.ecl` + `stage07boss.ecl`；各关共享 `default.ecl`）解析 SCPT 头（`+6` = hdr、`+0x10` = sub 数、偏移数组在 `buf+0x24+hdr`、名字区紧随其后）；
3. 在 `dest` 处解码指令头：`op == 0x000F / 0x000B` ⇒ 读 `+0x10`（名字长度）→ `+0x14` 起的名字串 = **段落入口函数**；
4. `portion 1` = 无补丁（关卡开头），入口 = **`MainSub00`**（**已定案，各关一致**，2026-10-17）。
> ✅ **收口（2026-10-17）**：第 4 条已定案 —— `portion 1` 入口 = **`MainSub00`**（ST1–ST7 一致）。「`ECLJump`（非 `Ex`）形态的段」（write-back 型、无 dest）在 th12 **不存在**：本作 warp **一律 `ECLJumpEx`**（`dest` 才是落点），名为 `ECLJump` 的函数（`thprac_th12.cpp:997`）**0 次调用**。stage3 p5 等处的**原始字节写入**（`SetPos(...)` + 多条 `ecl << opcode ...` / `pair{off,…}`）属同一「覆写名字串 / 覆写操作数」机制，已随本次枚举逐条核对，见 `docs/th12/th12_checkpoint_entry_report.md` §2.1 / §2.2。**CSV 已回填**（第 1 列 114/114）；**boss 行口径见该文档 §2.2–§2.3**（th12 无独立 boss 脚本文件、无 `$SPELL_ID`）。

### 10.5 风险：短命段落 sub

`MainSubNN` 多为**短命 worker**（例：`stage01.decl` 的 `MainSub02()` 仅 `$A = 10; while($A--) { enmCreate(...); wait(20); } return;`，寿命约 200 帧后线程结束、`T+4` 归 0）。latch 只在「**看见**」入口 sub 的那一帧置值，故：
- 采样频率必须 ≥1 次/帧（IGI 渲染即可）；
- 若某段入口 sub 寿命 < 1 帧（**未定论，需真机确认**），可退化为**在 `spawn` 处捕获**（EHOOK `0x46724C`，读 `[edi+0x1014]`=H 与当前指令 `ins+0x14` 的名字）——**备选方案，未验证**。

---

## 11. 对总括 §5 / §4 与 `th12_ecl_re_requests` 的回填建议

> 本报告**未修改**任何既有文件（避免与并行任务写冲突）。建议由本迭代的收口任务按下列内容回填。

### 11.1 总括 `docs/logger/logger_re_disasm_overview.md` §5.1 的 th12 行

| 列 | 建议填入 |
| --- | --- |
| 已有 ECL 解释器命中点（寄存器） | **`0x4671A0`（6B，`EDI` = 执行对象 T）**；函数 `FUN_00467170`（体 `0x467170`–`0x468C37`） |
| 命中点已知字段 | `*(u32*)(T+4)` = **当前指令地址**（**不是索引**）；`*(f32*)(T+0)` = 当前 ECL 时间 |
| ECL 数据区链 | `GetMemAddr(0x4B43DC, 0x64, 0xC)` = `res + 0xC`（`res = *(*(0x4B43DC)+0x64)`）——**原值正确，可直接用** |
| sub 名/索引线索 | ✅ `res+0x8C` 表（项 8B，`+0` = 名字 `char*`，`+4` = 起始，首指令 `+4+0x10`）；`count = res+8`；按名查表 `FUN_004694F0` |

并新增以下注记：
- **`[执行对象+4]` 语义矛盾的裁定补齐**（总括 §5.1 注记 / §9 风险 4）：**th12 = 指令指针（与 th13、th14 同类）；th15/16/17/18 = 索引**。两者不可互搬。
- **th12 的宿主层偏移与 th13 不同**（`H+0x1030` 线程链 / `H+0x102C` res / `H` = `0x27B8`；th13 为 `H+0x11D0` / `H+0x11CC` / `0x52E4`），**但线程 T 的字段偏移与 th13 逐字段一致**（`T+0x1008`/`T+0x1014`/`T+0x101C`，帧长 `0x0C`）。⇒ 可搬运「语义」，不可搬运「宿主偏移」。
- **新增一行 Q9 注记**：th12 的段落 sub 由 `opcode 0x0F`（`@X() async`）**派生为独立线程** ⇒ 读「当前段落」**必须遍历宿主线程链 `H+0x1030`**，不能只读主线线程。

### 11.2 总括 §4 分代表：**建议细分 B 代**（重要）

总括 §4 把 th10–th13 同列 B 代、参照作写作「th10 / th11」。本作实测：

- **th12 与 th13 的 ECL VM 骨架同源且逐字段一致**（线程字段、调用栈帧、sub 表、`0x0A/0x0B/0x0C` = ret/call/goto），**只有宿主对象层分叉**；
- 但**段落判定口径不同**：th13 的 `main` 用**同步 `call`** 进入 `MainFront`/`MainLatter`/`MainSubNN`（故读主线线程 `H+0x11D0` 有效），th12 用 **async 派生**（必须遍历线程链）；
- 而总括 §5.1 给 th10/th11 的判据是 `[Esi+0x1014]+0x103c+0x1444==1296` 这类**两级偏移**形态，th12 全域**不存在** `disp 0x1444`、且 `H+0x103C` 在 th12 是**普通计数**（宿主构造器 `0x413430`）⇒ **th10/th11 与 th12/th13 很可能并非同一份宿主布局**。

⇒ **建议**：把 §4 的 B 代拆为 **B1 = th12 / th13（ECL VM 骨架同源；宿主层分叉；参照作写 th13）** 与 **B2 = th10 / th11（待核；参照作写 th10/th11）**；或至少在表下注记里写明「B 代内部宿主布局至少两型，th12/th13 为一型，th10/th11 待核」。**th12 与 th13 近、与 th10/th11 远**。

### 11.3 总括 §5.2 接线锚点表 th12 行

保持现值即可（`th12_life_dec 0x4381E2`(5B，实测为 `mov ebx,1`，函数入口 `0x4381E0`) / `th12_bomb_dec 0x422F28`(5B，`mov [0x4b0ca0], eax`，函数入口 `0x422F20`) / `th12_game_start 0x421DEF`(6B，`mov [0x4b0ca0], ebx`，函数入口 `0x421CB0`) 均**未复核语义**，本轮未触及）。**新增建议**：把「主线宿主捕获」补进 §5.2 或新增一列 —— `th12` 捕获点 **`0x4228BE` / `0x4229D2`**（`EAX` = 宿主 H，各 5B）。

### 11.4 `docs/th12/th12_ecl_re_requests.md`

| 章节 | 建议 |
| --- | --- |
| §1 表格「ECL 解释器 hook ❌ 无」 | **改为**「✅ `0x4671A0`（6B，`EDI` = 执行对象 T）；函数 `FUN_00467170`」。 |
| §1 表格「ECL 数据区链」 | **保持原值**（正确）；补注展开式 `res+0xC`、`res = *(*(0x4b43dc)+0x64)`。 |
| §1 表格「指针常量」 | **保持**（地址有效）。 |
| §1 强线索（`call` 内嵌 sub 名） | **结论保留、载体更正**：真实载体是游戏自身 `.ecl` 的 `opcode 0x0B`（同步 call）/`opcode 0x0F`（async spawn）；**`ECLCallSub` 是死代码且布局有误**（见本报告 §9.3）。 |
| §2 Q1 | 答：**指令指针**（非索引），`ip = *(u32*)(T+4)`；写入点见 §2.2。 |
| §3 Q2 | 答：`mgr = *(0x4B43DC)`；`res = *(mgr+0x64)`；`count = res+8`；`table = res+0x8C`；项 8B；`+0` = 名字、`+4` = 起始、首指令 `+4+0x10`；加载器 `0x4692E0`，查表 `0x4694F0`，访问器 `0x40E8F0`/`0x40E920`/`0x40E930`。 |
| §4 Q3 | 答：**有**显式栈；基址 `T+8`，SP `T+0x1008`，帧长 `0x0C`，帧 `+0x08` = 调用者 ip；压帧 `FUN_00466F00`；**async 派生线程各有独立栈，跨线程不成链**。 |
| §5 Q4 | 答：`FUN_00412990` 返回宿主；站点 `0x4228BE`/`0x4229D2`（`"main"`@`0x49FE14`）；`T+4 = FindSubByName(res,"main")` @ `0x413448`；静态兜底 `*(u32*)(*(u32*)(mgr+0x68))`。 |
| §6 Q5 | 答：`0x4671A0`（6B EHOOK），`EDI` = 执行对象 T；函数 `FUN_00467170`；跳表 `0x468C38` / 映射 `0x468D30`；opcode 上界 `0x59`。 |
| §7 Q6 | 答：**运行期保留名字**；因是地址语义，需 `table[i].+4+0x10` 建区间表反查。 |
| §8 Q7 | 答：见本报告 §8 表（`CHARA=0x4B0C90`、`SUBSHOT=0x4B0C94` 等，均 thprac 既有）。 |
| §9 Q8/Q9 | 答：Q8 不影响；**Q9 = 路过式 checkpoint（latch）**，但**读取面 = 宿主线程链 `H+0x1030` 的全体线程**（原因是段落 sub 为 async 派生）。 |
| §10 真机验证清单 | 追加本报告 §13 的条目。 |
| §11 交付格式 | 保持；本报告即交付物。 |
| §12 工具与复现 | 补：脚手架 `D:\workshop\reasm\th12\dl.py`；推荐切入点更正为「SCPT 魔数 → 加载器 → res/表；`"main"` → 宿主工厂 → 捕获点；`disp 0x1008` 密集区 → VM/spawn/驱动器」。 |

---

## 12. 推荐实现摘要（供接线）

```cpp
// ── 初始化：捕获主线宿主 ─────────────────────────────────────────────
EHOOK_DY(th12_ecl_host,  0x4228BE, 5, { mEclHost = (uint32_t)pCtx->Eax; self->Disable(); });
EHOOK_DY(th12_ecl_host2, 0x4229D2, 5, { mEclHost = (uint32_t)pCtx->Eax; self->Disable(); });
// 清理：th12_game_start(0x421DEF) 里 mEclHost = 0;（沿用既有 EHOOK）

// ── 每帧（IGI 渲染时，帧间稳定态）读取「当前段落」────────────────────
const char* section = nullptr;
if (!mEclHost) { section = "main"; }
else {
    auto mgr   = *(uint32_t*)0x4B43DC;               // ★ th12：全局管理器
    auto res   = *(uint32_t*)(mgr + 0x64);           // ECL 资源（≡ *(uint32_t*)(mEclHost + 0x102C)）
    auto count = *(int32_t*)(res + 8);
    auto table = *(uint32_t*)(res + 0x8C);
    if (!res || !table || count <= 0) { section = "main"; }
    else {
        // ★ 遍历宿主线程链：头节点内嵌于 H+0x1030，节点 {+0 = T, +4 = next, +8 = prev}
        for (uint32_t node = mEclHost + 0x1030; node; node = *(uint32_t*)(node + 4)) {
            uint32_t T = *(uint32_t*)(node);          // 线程对象
            if (!T) continue;
            uint32_t ip = *(uint32_t*)(T + 4);        // 该线程当前指令绝对地址；0 = 已结束
            if (!ip) continue;
            int idx = SubOfAddress(table, count, ip); // §2.3 区间表（按 table[i].+4+0x10 排序）
            if (idx < 0) continue;
            const char* cur = *(char**)(table + idx * 8);
            const char* s = Whitelist(game, stage, cur);
            if (s) { section = s; break; }            // 命中 → 置值；未命中 → no-op
        }
    }
}
// 地址区间表：一次性/每次 res 变化时重建
//   start_i = *(u32*)(table + i*8 + 4) + 0x10，按 start_i 升序排序
```

**EHOOK 覆盖长度**：`0x4228BE` / `0x4229D2` 处下一条指令 = `call 0x41D560`（5B，`E8 xx xx xx xx`）⇒ **5B EHOOK** 合适。

**备选（需要无条件每帧触发时的 VM 钩子）**：
```cpp
EHOOK_DY(th12_ecl_vm, 0x4671A0, 6, { if (IsMainHostThread(pCtx->Edi)) { /* 读 [Edi+4] */ } });
// 命中时 EDI = 执行对象 T；注意「纯等待」时不经过此点，且会对所有线程命中，须过滤。
```

---

## 13. 真机验证清单

> 本机无法运行 th12，以下**全部未做**；状态一律为「静态定案（🟡 已定案、未真机三态）」。

- [ ] **宿主捕获**：在 `0x4228BE`/`0x4229D2` 抓 `EAX` 非空；进关后 `H+0x1030`、`H+0x102C` 均可解引用，且 **`H+0x102C == *(u32*)(mgr+0x64)`**（`mgr = *(u32*)0x4B43DC`）。
- [ ] **线程链可遍历**：`H+0x1030` 的 `{+0 = T, +4 = next}` 链长度 ≥1；`*(u32*)(H+0x1030) == H+8`（主线线程）。
- [ ] **Q1 地址语义**：道中/boss/不同段落各采样一次，`SubOfAddress(T+4)` 与画面进度一致；`T+4 == 0` 只在线程结束（worker 跑完/过关/退关）时出现。
- [ ] **地址→sub 反查**：同一 subs 内 `ip` 始终落在**同一个** `[start_i, start_{i+1})` 区间内；跨 sub 时切换。
- [ ] **★ 段落 sub 走异步线程（本作最关键的一条）**：进 stage1 后，主线线程（`*(u32*)(H+0x1030)`）的 `T+4` 应**长期落在 `main` 内**；而线程链上应能观察到 `MainSubNN` 的线程（`T+4` 落在对应 `start_i`）。**若观察到主线线程自己进入 `MainSubNN`，则本报告 §10.1 的 async 结论需订正。**
- [ ] **路过置值**：经过各段入口 sub 时当前段落被正确置值；段内其余 sub 不改变当前段落（no-op）；`main` 本身不入表（否则会立刻覆盖）。
- [ ] **短命 worker**：`MainSub02` 这类 ~200 帧的 worker，在 IGI 每帧采样下是否**必然**被捕获一次（若偶发漏采 ⇒ 启用 §10.5 的 `spawn` 钩子备选方案）。
- [ ] **练习跳章**：跳章后 `T+4`/线程链读数与白名单置值均正确（Q8 复核）；`ins[0xA] = 0xFF` 的难度掩码在 E/N/H/L 四难度下均能执行（否则 `goto` 会被 `test byte [edi+0x101C], al` 跳过 —— **这是最需要真机确认的一条**）。
- [ ] **`ord`/文件槽位**：跳章后运行期缓冲切换（`res+0xC+slot*4`）时，`table` 是否被**重建**（`FUN_004692E0` 累加 `res+8`）⇒ 地址区间表须随之重建，否则读到脏名字。
- [ ] **分支关**：th12 st4 是否有路线分支（**未定论**），各分支各走一遍，读取与白名单映射均正确。
- [ ] **Q7 待实测**：`0x4B0C90`（机体）、`0x4B0C94`（子机）取值与所选机体/子机一致。
- [ ] **变体**：Steam / 汉化版是否共用同一 RVA（本报告地址来自原版 `ts=1251344348`）。
- [ ] **未追项（见 §6.4）**：th12 的「主线判据」（th10/th11 的 `…+0x103c+0x1444==1296` 形态）在 th12 里的对应形态 —— **未定论**，若后续需要「只取主线宿主」的判据须补做。

---

## 14. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档（本报告）。静态定案 th12 的 Q1–Q9。**Q1** `[T+4]` = **当前指令绝对地址（不是 sub 索引）**，写入点 `0x411E51`/`0x413448`/`0x413EEA`/`0x413FC0`/`0x467122`/`0x467409`/`0x4671F9`/`0x468BF3`/`0x468C24`；**Q2** `mgr=*(u32*)0x4B43DC` → `res=*(mgr+0x64)` → `count=res+8` → `table=res+0x8C`（项 8B，`+0`=名字 `char*`，`+4`=数据基址，首指令 `+4+0x10`），访问器 `0x40E8F0`/`0x40E920`/`0x40E930`，加载器 `FUN_004692E0`（SCPT `0x54504353`），查表 `FUN_004694F0`；**Q3** 显式调用栈（基址 `T+8`、SP `T+0x1008`、帧长 `0x0C`、帧 `+0x08`=调用者 ip，压帧 `FUN_00466F00`，弹帧 `ret`@`0x4671CD`），**async 派生线程各有独立栈**；**Q4** 宿主工厂 `FUN_00412990`（`mem 0x27B8`，返回 `EAX`=宿主），站点 `0x4228BE`/`0x4229D2`（`"main"`@`0x49FE14`），宿主线程链 `H+0x1030`↔`H+4`（`FUN_00411E40`/`FUN_00468D90`），静态兜底 `*(u32*)(*(u32*)(mgr+0x68))`；**Q5（全新自找）** 命中点 **`0x4671A0`**（6B，`EDI`=执行对象 T），函数 `FUN_00467170`（`0x467170`–`0x468C37`），跳表 `0x468C38`/映射 `0x468D30`，opcode 上界 `0x59`，`>0x59` 委派 `0x468BD9`；**Q6** 运行期保留名字（需地址区间反查）；**Q7** `CHARA=0x4B0C90`、`SUBSHOT=0x4B0C94`、`STAGE_NUM=0x4B0CB0`、`MODEFLAGS=0x4B0CE0`、`PLAYER_PTR=0x4B4514`、`REPLAY_MGR_PTR=0x4B4518`；**Q8** `ECLJump` 只写 `goto`(`0x0C`)，只改 `T+0`/`T+4`；**Q9** 路过式 checkpoint（latch），**读取面 = 宿主线程链 `H+0x1030` 全体线程**（因段落 sub 由 opcode `0x0F` 异步派生）。**关键订正**：(a) requests §1 的 `ECLCallSub` 在 th12 是**死代码**且布局与真实 `call` 指令头不符（见 §9.3）；(b) requests §6 的切入点建议改为「SCPT 魔数 → 加载器 → res/表；`"main"` → 宿主工厂；`disp 0x1008` 密集区 → VM/spawn/驱动器」（见 §6.4）；(c) 总括 §4 建议把 B 代细分 B1(th12/th13)/B2(th10/th11)（见 §11.2）。证据：`capstone` 只读反汇编（自写 `D:\workshop\reasm\th12\dl.py`）+ 全程序字节/交叉引用扫描 + `.ecl` 容器离线解码（`stage01`/`stage02`）。**未真机三态；段落入口白名单待枚举（阶段 E3/E4）。** |
| 2026-10-17 | **§10.4 收口（+ §0 Q9 行状态同步）**：段落入口白名单由「未定论 / 待枚举」改为 **✅ 已枚举完毕（2026-10-17）** —— `docs/logger/th12_checkpoints.csv` 第 1 列 **114/114**（章节 43/43、boss 71/71）；§10.4 第 4 条「`portion 1` 入口待定」**定案 = `MainSub00`（各关一致）**；「`ECLJump`（非 `Ex`）形态的段」确认在本作**不存在**（本作 warp 一律 `ECLJumpEx`，`ECLJump` 0 次调用）。**新建 `docs/th12/th12_checkpoint_entry_report.md`**（入口表小结）：入口表（章节/boss 分组）+ 相位链 + 本作特有两条机制（① 两个 warp API：`ECLJump` 无 `dest` / `ECLJumpEx` 的 `dest` 才是落点；② boss 行不适用「`stNNbs`/`stNNmbs` + `$SPELL_ID`」——无独立 boss 脚本文件、无 `$SPELL_ID`，靠改容器 `call` 名字串 + 改 `lifeSet` 操作数）+ `ST6 BOSS9/BOSS10 = BossCard5/BossCard6`（修正）+ 实现影响（async 派生线程 ⇒ 必须遍历宿主线程链 `H+0x1030`；boss 容器独立宿主 `enmCreateA` ⇒ 过渡 sub 风险）+ 风险清单。**未真机（🟡）**。 |

# TH13 ECL 段落监控 — 逆向结论（Q1–Q9）

> 对应需求：`docs/th13/th13_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`；方法论范例（结构参照）：`docs/th16/th16_ecl_re_report.md`、`docs/th15/th15_ecl_re_report.md`。
> 二进制：`../exes/th13.exe`，790528 B，PE `TimeDateStamp = 1313589413`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**）。`.text` = VA `0x401000`–`0x4A2275`，RVA `0x1000`–`0xA2275`，`SizeOfRawData = 0xA0400`，与 `thprac_launcher_games_def.h` 的 th13 ORIGINAL 条目一致。
> 引擎代：**B 代**；同代参照：th10/th11/th12（均**无**报告，仅 requests）。
> 方法：`pefile` + `capstone 5.0.7` 只读反汇编（自写脚本 `D:\workshop\reasm\th13\dl.py`），用「原始字节搜索 + 局部对齐 + 函数入口整段反汇编」定位（避开 `.text` 线性反汇编失步，见 `th13_re_disasm_report.md` §5 的坑）。
> ECL / decl 素材：`../exes/ecl/th13/`（原始 `.ecl` 21 份）、`../exes/ecl_dump/th13/`（`thecl -d` 反编译 `.decl` 21 份），清单与来源见 `docs/logger/decompile_materials.md`。
> 素材已在机，故「名字 ↔ 段落入口」**可离线枚举**（枚举本身仍待做）。
> 状态：**静态定案（🟡 已定案、未真机三态）**。本机无法运行 th13，**未做任何真机验证**。
> 地址口径：正文以 **VA** 为主，并标注 **RVA = VA − 0x400000**。
> 置信度图例：✅ 反汇编直接证实 ｜ 🟡 有强证据但缺一环（显式标注缺口）｜ ❌ 未能定案。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前 sub = 索引？指令指针？** | ✅ **是「指令地址（指针）」**，**不是 sub 索引** | `ip = *(u32*)(T+0x04)`（T = 执行对象），指向 ECL 文件缓冲内的**当前/下一条待执行指令绝对地址**；`0` = 线程结束。`subIdx` 这一层**在本作不存在**（与 th15/th16 的 `thread+4 = 索引` **不同**）。 |
| **Q2 sub 表** | ✅ **确认（与 th15/th16 同构）** | `mgr = *(u32*)0x4C2188`；`res = *(u32*)(mgr+0xAC)`；`count = *(i32*)(res+0x08)`；`table = *(u32*)(res+0x8C)`；项 **8B**，`+0` = 名字 `char*`、`+4` = sub 数据基址（**首指令 = `+4 + 0x10`**）。加载器 `FUN_004795B0`（SCPT 魔数 `0x54504353`）。 |
| **Q3 函数栈** | ✅ **存在可遍历显式子调用栈** | 栈基址 `T+0x08`，栈指针 `SP = *(u32*)(T+0x1008)`（`0` = 空栈）；**帧长 `0x0C`**；帧内 `+0x00`=旧 SP / `+0x04`=时间 / **`+0x08`=调用者指令地址**。压帧 `FUN_00477030`，弹帧 = `ret`(case `0x0A`) @ `0x4772FF`。**栈顶帧存「调用者 ip」而非索引**（因本作无 subIdx）。 |
| **Q4 主线宿主捕获点** | ✅ **给出捕获点** | `FUN_004183F0` 分配 `0x52E4` 并构造宿主，**返回 `EAX` = 宿主 H**；「spawn main」两站点：`call 0x4183F0` @ **`0x42C766`** 与 @ **`0x42C905`**（下一条 `0x42C76B` / `0x42C90A` 可挂 5B EHOOK），命名参数 `"main"` @ `0x4AADE0`。① 静态兜底：宿主链表 `*(u32*)(mgr+0xB0)` → 节点 `H+0x14CC`，**`H = *(u32*)(*(u32*)(mgr+0xB0))`**。 |
| **Q5 解释器命中点** | ✅ **确认** | **`0x4772D2`**（`8B 43 04` + `8A 48 0A`，可挂 6B EHOOK）：`mov eax,[ebx+4]` / `mov cl,[eax+0xa]`；**命中寄存器 `EBX` = 执行对象 T**（脚本上下文）；所在函数 = `FUN_004772A0`（`0x4772A0`/RVA `0x772A0`）= **ECL 主解释器**（每帧逐指令步进），opcode 跳表 `0x478C38` / 字节映射 `0x478D44`。 |
| **Q-A 宿主链覆盖 boss 宿主？** | ✅ **覆盖（全局全宿主链）** | 全程序**唯一**宿主分配点 = `FUN_004183F0`（`push 0x52E4` 仅 `0x4183FC` 一处），且该工厂**无条件**把新宿主挂进 `mgr+0xB0`（`0x41863A`–`0x418652` 空表头/尾；`0x41865D`–`0x41869B` 追加）。⇒ 链 = **全宿主链（主线 + boss + 中boss + 敌方波次宿主）**。引擎自己的宿主迭代就是这条链（`FUN_00414180` @ `0x4141EE`、`FUN_0041AC70` @ `0x41AC7C`、`FUN_00417E90` @ `0x417E96`）。**⇒ 遍历这条链即可拿到 boss 宿主，不需要新捕获点**（详见 §5.4.1）。 |
| **Q-B boss 宿主捕获点** | ✅ **给出（但非必需）** | boss / 中boss 宿主由 **ECL 的 `enmCreateA`（扩展 opcode `0x12D`；`0x12C`/`0x130`/`0x131`/`0x141` 同分支）** 另起：`0x41B2EC: push edi; call 0x41FCC0` → `FUN_0041FCC0` → `call 0x4183F0` @ `0x41FE62`，**名字 = 该 ECL 指令的 `ins+0x14`**（`enmCreateA("Boss",…)` / `("MBoss",…)`）。可挂 5B EHOOK 的下一条指令 = **`0x41FE67` / RVA `0x1FE67`（`EAX` = 新宿主）**。⚠️ 该点对**每次** `enmCreate*` 都命中、且「宿主数上限」早退分支也跳到同址 ⇒ 须校验 `EAX`。工厂的其余 4 个调用点（`0x42C766`/`0x42C905` = `"main"`、`0x422F1A` = 字面量 `"BossCard2_EtAt"`、`0x414925` = sub 表按索引取名、`0x40FF0F` = `[edi+0x28]`）均**不是** boss 宿主（详见 §5.4.2）。 |
| **Q-C boss 宿主的 `res` 含 boss sub？** | ✅ **含（同一个全局 res）** | 宿主构造器 `FUN_00418F40` 在 `0x4191E4`/`0x4191EB` **无条件**写 `H+0x11CC = *(u32*)(mgr+0xAC)`（= 全局 `res`）——对**每个**宿主（含 boss / 中boss）成立；`res+0x8C` 表由加载器 `FUN_004795B0` **跨文件累积**（`0x479606` 累加 `res+8`；`0x479680` 起 memcpy 旧表 + `0x4796E0` 起二分插入）。关卡头 `ecli{}` 含 `stNNbs.ecl`（ST2–5/7 还含 `stNNmbs.ecl`）⇒ **`BossN`/`BossCardN`/`MBoss*`/`MBossCardN` 全在表内**，「地址 → 名字」区间反查与 `H+0x11CC` 取法对 boss 宿主**均成立**（详见 §5.4.3）。 |
| **Q6 名字来源** | ✅ **运行期保留名字** | `table[i].+0` 为 `char*`，指向**已加载 ECL 缓冲内的名字串**（加载器 `FUN_004795B0` 在 `0x47965E` 填入）。但因 Q1 是地址而非索引，取「当前 sub 名」需**地址 → sub 反查**（用 `table[i].+4+0x10` 建区间表二分）。 |
| **Q7 机体 / 子机** | 🟡 **给出现有地址 + 推测** | `CHARA = 0x4BE7B8`（thprac 既有，xref 109 处）；`SHOT/SHOTTYPE ≈ 0x4BE7BC`（**紧邻、xref 48 处，与 th12 的 `CHARA=0x4b0c90 / SUBSHOT=0x4b0c94` 同构 ⇒ 推测，待实测**）；`0x4BE7C4`（xref 91 处）疑为**难度/rank**（`shl dl,cl` 生成 rank 掩码）。 |
| **Q8 ECLJump** | ✅ **不影响判读** | `ECLJump` 只把 `goto`(`opcode 0x0C`) 写进运行期缓冲；`goto`(case `0x0C` @ `0x47758E`) **只改 `T+0`(时间) 与 `T+4`(ip)**，不触碰 sub 表、不触碰「当前 sub」的任何其他字段。 |
| **Q9 段落判定口径** | ✅ **路过式 checkpoint（latch）** | CSV 即白名单：当前 sub（经 Q2 地址区间表把 `T+4` 映射回名字）∈ 表 → 置当前段落；**不在 → no-op**（不回退 `main`）。**每 checkpoint 只登记 1 个入口函数**（含 async 落点）。**段落入口白名单已于 2026-10-17 枚举完毕**（CSV 第 1 列 **116/116**：章节 44/44、boss 72/72；逐行入口见 `docs/th13/th13_checkpoint_entry_report.md`）。⚠️ **本作有 3 行的入口只经 `async call` 进入** ⇒ 实现须遍历宿主线程链（见 §10.4 与总括 §5.1 注记）。 |

> **关键地址（VA / RVA）**：
> ECL 主解释器 `FUN_004772A0` = `0x4772A0`/`0x772A0`（体 `0x4772A0`–`0x478BEA`）；
> 命中点 `0x4772D2`/`0x772D2`；ip 推进 `0x477377`/`0x77377`；
> `ret`(case `0x0A`) `0x4772FF`/`0x772FF`；`call`(case `0x0B`) `0x477502`/`0x77502`；压帧 `FUN_00477030`/`0x77030`；`goto`(case `0x0C`) `0x47758E`/`0x7758E`；
> opcode 跳表 `0x478C38`；字节映射 `0x478D44`；>0x5D 委派点 `0x478B9D`；推进/等时 `0x478BBF`；线程结束 `0x478BDB`；
> 线程驱动器 `FUN_00478DB0`/`0x78DB0`（`0x478DE0: mov [H+4],[H+0x11D0]`）；
> 宿主构造 `FUN_00418F40`/`0x18F40`；宿主「复位」`FUN_00417740`/`0x17740`（`0x417797: H+4 = H+8`、`0x4177A9: H+0x11D0 = H+8`）；
> 宿主工厂 `FUN_004183F0`/`0x183F0`；`"main"` @ `0x4AADE0`/`0xAADE0`；spawn 站点 `0x42C766`/`0x2C766`、`0x42C905`/`0x2C905`；
> **（§5.4 boss 宿主可达性）** ECL `enmCreateA`(opcode `0x12D`) 分支 `0x41B2EC`/`0x1B2EC` → `FUN_0041FCC0`/`0x1FCC0` → `call 0x4183F0` @ `0x41FE62`/`0x1FE62` ⇒ **boss 宿主捕获点 `0x41FE67`/`0x1FE67`（5B，`EAX` = 新宿主）**；
> `"BossCard2_EtAt"` @ `0x4AABD8`/`0xAABD8`；工厂其余调用点 `0x414925`/`0x14925`、`0x422F1A`/`0x22F1A`、`0x40FF0F`/`0xFF0F`；宿主链遍历器 `FUN_00414180`/`0x14180`（`0x4141EE`）、`FUN_0041AC70`/`0x1AC70`（`0x41AC7C`）、`FUN_00417E90`/`0x17E90`（`0x417E96`）；单文件 ECL 加载 `FUN_00417E20`/`0x17E20`（vtable 槽 `0x4AAC38`）；
> SCPT 加载器 `FUN_004795B0`/`0x795B0`；按名查表 `FUN_004797D0`/`0x797D0`；
> 访问器 `GetSubName` `0x414150`/`0x14150`、`GetResCount` `0x414160`/`0x14160`、`GetRes` `0x414170`/`0x14170`；
> 实体（线程）构造 `FUN_00479860`/`0x79860`（`malloc(0x11E0)`）；
> 扩展 opcode（≥0x12C）处理器 `FUN_0041B260`/`0x1B260`，入口蹦床 `0x41B250`/`0x1B250`（`add ecx,0x11EC; jmp`）。

---

## 1. 统一指针链（TH13 定案）

```text
mgr   = *(u32*)0x4C2188                              // 全局管理器（ECL/脚本管理器）
res   = *(u32*)(mgr + 0xAC)                          // ECL 资源对象（GetRes @0x414170）
count = *(i32*)(res + 0x08)                          // sub 总数（GetResCount @0x414160）
table = *(u32*)(res + 0x8C)                          // sub 表（项 8B）
name_i  = *(char**)(table + i*8)                     // 项 +0 = 名字 char*
start_i = *(u32*)(table + i*8 + 4) + 0x10            // 项 +4 = sub 数据基址 ⇒ 首指令 = +4+0x10

// ── 宿主（主线 / boss / 中boss / 敌方波次 **同构**；§5.4）────────────────
//   枚举：node = *(u32*)(mgr+0xB0); while (node) { H = *(u32*)node; node = *(u32*)(node+4); }
//   主线宿主也可由 0x42C76B / 0x42C90A 抓到 EAX
H     = ECL 宿主对象（`0x52E4` 字节）
H+0x11CC = res                                       // 宿主持有的 ECL 资源（= 上面那个 res）
T     = *(u32*)(H + 0x11D0)                          // 线程/脚本上下文（≡ *(u32*)(H+4)）
T+0x000 = float 当前 ECL 时间（跨帧累积）
T+0x004 = u32  当前（下一条待执行）ECL 指令的绝对地址；0 = 线程结束 ★Q1
T+0x008 = u32  调用栈基址（帧数组起点）
T+0x1008 = i32 调用栈指针 SP（字节偏移，0 = 空栈）
T+0x100C = i32 局部/表达式栈指针（与 0x1008 共用基址 T+8）
T+0x1014 = u32 ins 处理器对象（= H 自身；opcode > 0x5D 时对它做虚调用）
T+0x101C = u8  指令选择掩码（难易度位掩码；供 `test byte [T+0x101C], ins[0xA]`）

// ── 宿主自身的其它字段 ────────────────────────────────────────────────
H+0x000  = vtable（主线宿主 = 0x4AAC14；更早设过一次 0x4AABF8）
H+0x004  = T（= H+8，见 0x417797 / 0x478DE0）
H+0x008  = T 本体（内嵌线程）
H+0x11D0 = T（线程链表头；H+0x11D4 = 后继，0 = 无）
H+0x11EC = 扩展 opcode 处理器子对象（vtable slot 0/0x1C → `0x41B250: add ecx,0x11EC; jmp 0x41B260`）
H+0x14CC = 宿主链表节点 { +0 = H, +4 = next, +8 = prev }；链表头/尾 = `*(u32*)(mgr+0xB0)` / `*(u32*)(mgr+0xB4)`
H+0x52B4 = H（自指针；供 `H+0x11EC` 处读 `[this+0x40C8]` 拿回宿主）
```

**与 th15/th16 模板的差异（重要，不可照搬）**

| 项 | th15 / th16（C 代） | **th13（本作）** |
| --- | --- | --- |
| 宿主 → 线程 | `host+0xC` = thread | **`H+0x11D0`（≡ `H+4`）= T** |
| 宿主 → res | `host+0x11F8` = res | **`H+0x11CC` = res**（更直接：`res = *(*(0x4C2188)+0xAC)`，**不需要宿主**） |
| 当前 sub | `thread+4` = **sub 索引**，`thread+8` = ip 偏移 | **`T+4` = 指令绝对地址**（无 subIdx 层） |
| 调用栈基址 / SP | `thread+0xC` / `thread+0x100C` | **`T+0x08` / `T+0x1008`** |
| 帧长 / 帧内 | `0x10`，帧内 `+0xC` = 调用者 sub 索引 | **`0x0C`**，帧内 `+0x08` = 调用者**指令地址** |
| sub 表 | `res+0x8C`，项 8B，`+0`=名字 `+4`=起始 | **完全一致** ✅ |

---

## 2. Q1 —— `[T+4]` 是**指令地址（指针）**，不是 sub 索引

### 2.1 判据

ECL 主解释器 `FUN_004772A0` 的 prologue 与分发头直接把 `EBX` 当脚本上下文，取 `[EBX+4]` 作为**当前指令**再读其 opcode：

```asm
004772a0  55                push  ebp
004772a1  8bec              mov   ebp, esp
004772a3  83e4f8            and   esp, 0xfffffff8
004772a6  8b4304            mov   eax, dword ptr [ebx + 4]     ; eax = 当前指令地址
004772a9  81ecb0000000      sub   esp, 0xb0
004772af  56                push  esi
004772b0  57                push  edi
004772b1  85c0              test  eax, eax
004772b3  0f8429190000      je    0x478be2                      ; ip == 0 → 线程结束
004772b9  db00              fild  dword ptr [eax]                 ; 指令的「时间」是 int
004772bb  d903              fld   dword ptr [ebx]               ; T+0 = 当前时间(float)
004772bd  ded9              fcompp
004772bf  dfe0              fnstsw ax
004772c1  f6c401            test  ah, 1
004772c4  0f85cd000000      jne   0x477397                      ; 时间未到 → 累加时间后返回
004772ca  befcffffff        mov   esi, 0xfffffffc
004772cf  8d7e08            lea   edi, [esi + 8]
004772d2  8b4304            mov   eax, dword ptr [ebx + 4]      ; ★ 分发循环头：重新取当前指令
004772d5  8a480a            mov   cl, byte ptr [eax + 0xa]      ; 指令 +0xA = 选择掩码字节
004772d8  848b1c100000      test  byte ptr [ebx + 0x101c], cl
004772de  0f8493000000      je    0x477377                      ; 掩码不匹配 → 跳过该指令
004772e4  0fbf4804          movsx ecx, word ptr [eax + 4]       ; opcode（16 位）
004772e8  83f95d            cmp   ecx, 0x5d
004772eb  0f87ac180000      ja    0x478b9d                      ; > 0x5D → 委派给 T+0x1014 的虚方法
004772f1  0fb691448d4700    movzx edx, byte ptr [ecx + 0x478d44]  ; 字节映射表
004772f8  ff2495388c4700    jmp   dword ptr [edx*4 + 0x478c38]  ; 跳表分发
```

`[EBX+4]` 被当作**指针**：读 `[ins+0]`（int 时间）、`[ins+4]`（opcode）、`[ins+6]`（size）、`[ins+0xA]`（掩码）——它**绝不可能是索引或偏移**。且**本作不存在「当前 sub 索引」这一层**：全函数对 `[EBX+4]` 的写入只有「绝对地址」一种形态。

> **指令头布局（由上述指令流推出）**：`+0x00` = int 时间 ｜ `+0x04` = u16 opcode ｜ `+0x06` = u16 size ｜ `+0x08` = u16 浮点参数掩码 ｜ `+0x0A` = u8 选择掩码（难易度） ｜ `+0x0C` = u8 局部槽数 ｜ `+0x0D..0x0F` 填充 ｜ **`+0x10` 起 = 参数（dword）**。与 `thprac_th13.cpp:943` 的 `ECLJump` 写出的 6 个 dword 完全吻合（`opcode=0x0C`/`size=0x18`/掩码 `0x02FF0000` ⇒ `ins[0xA]=0xFF` 任意难度 | `ins[0x10]`=dest−start | `ins[0x14]`=at_frame）。

### 2.2 字段写入点（谁写 `T+4`）

| VA / RVA | 语义 | 片段 |
| --- | --- | --- |
| `0x41779A` / `0x1779A` | **线程复位**：`T+4` 与 `T+0x11D0` 同指向内嵌线程首地址 | `lea edx,[eax+8]` → `mov [eax+4],edx`（`FUN_00417740`，由宿主构造器 `0x418FA7` 调用） |
| `0x4191F9` / `0x191F9` | **进关/复位设主线 sub**：`T+4 = FindSubByName("main")` 结果（= 该 sub 首指令） | `mov ecx,[edi+4]` → `mov [ecx+4],eax` |
| `0x41AC2F` / `0x1AC2F` | **按名切 sub**：`T+4 = FindSubByName(res, name) + 0x10` | `mov edx,[ebx+4]` → `mov [edx+4],eax` |
| `0x4798D2` / `0x798D2`、`0x479901` / `0x79901`、`0x479921` / `0x79921` | 线程构造函数族里的同型写入 | `mov ecx,[esi+4]` → `mov [ecx+4],eax` |
| **`0x477380` / `0x77380`** | **逐指令推进**：`T+4 = T+4 + ins.size` | `movzx ecx,word[eax+6]` → `add ecx,eax` → `mov [ebx+4],ecx` |
| `0x477324` / `0x77324` | **`ret` 恢复调用者 ip** | `mov eax,[eax+ebx+8]` → `mov [ebx+4],eax` |
| `0x47758E`+`0x47737E` / `0x7758E` | **`goto` 重定位**：`T+4 = ins + ins[+0x10]` | `mov ecx,[eax+0x10]` → `add ecx,eax` → `mov [ebx+4],ecx` |
| `0x478BDB` / `0x78BDB` | **线程结束**：`T+4 = 0` | `mov dword ptr [ebx+4], 0` |

### 2.3 「地址 → 当前 sub」的取法（因无 subIdx）

```cpp
// 用 sub 表建「起始地址 → 索引」区间表（一次性 / 每次 res 变化时重建）
//   start_i = *(u32*)(table + i*8 + 4) + 0x10
// 按 start_i 升序排序；当前 sub = 满足 start_i <= ip < start_{i+1} 的 i
// name = *(char**)(table + i*8)
```
> ⚠️ 表项按**名字字典序**（`FUN_004797D0` 二分）存放，**地址并不单调** ⇒ 必须显式排序，不能直接对索引二分。
> ⚠️ 越界/脏指针保护：`ip` 必须落在某 `[start_i, start_{i+1})` 内，否则视为「不在已知 sub」。`ip == 0` → 线程已结束。

### 2.4 与总括 §5.1 矛盾的裁定

总括 §5.1 的遗留矛盾「`[执行对象+4]` 到底是 sub 索引还是指令指针」在本作有明确答案：**th13 是「指令指针」**（该行原先标 th14 为「指令指针」；**th13 应与 th14 同类**）。**th15/th16/17/18 是「索引」**，两者不可互相搬运。

---

## 3. Q2 —— sub 表：`res+0x8C`，项 8B（与 th15/th16 逐字段一致）

### 3.1 三个现成访问器（游戏自己写的，直接证明字段语义）

```asm
00414150  8b888c000000   mov   ecx, dword ptr [eax + 0x8c]   ; eax = res → table
00414156  8b04d1         mov   eax, dword ptr [ecx + edx*8]  ; table[idx].+0  → 名字 char*
00414159  c3             ret                                  ; GetSubName(res, idx)
00414160  a188214c00     mov   eax, dword ptr [0x4c2188]      ; mgr
00414165  8b88ac000000   mov   ecx, dword ptr [eax + 0xac]    ; res = mgr+0xAC
0041416b  8b4108         mov   eax, dword ptr [ecx + 8]       ; res+8 → count
0041416e  c3             ret                                  ; GetResCount()
00414170  a188214c00     mov   eax, dword ptr [0x4c2188]      ; mgr
00414175  8b80ac000000   mov   eax, dword ptr [eax + 0xac]    ; res = mgr+0xAC
0041417b  c3             ret                                  ; GetRes()
```

⇒ 三条链一次坐实：`res = *(*(0x4C2188)+0xAC)`、`count = res+8`、`table = res+0x8C`、`table[i].+0 = char* 名字`。

### 3.2 建表 / 加载（写侧）—— `FUN_004795B0`@`0x4795B0`

```asm
004795b7  8bf1              mov   esi, ecx                       ; esi = res
004795b9  8b4604            mov   eax, dword ptr [esi + 4]        ; 文件槽号
004795bf  894c860c          mov   dword ptr [esi + eax*4 + 0xc], ecx  ; res[+0xC+slot*4] = ECL 文件缓冲
004795ca  813953435054      cmp   dword ptr [ecx], 0x54504353    ; 魔数 "SCPT"
004795e9  6683790401        cmp   word ptr [ecx + 4], 1          ; 版本 1
004795f2  0fb74806          movzx ecx, word ptr [eax + 6]        ; 头尺寸
004795fe  8d7c0124          lea   edi, [ecx + eax + 0x24]        ; 名字区 = buf+0x24+hdr
00479602  0fb74010          movzx eax, word ptr [eax + 0x10]     ; 本文件 sub 数
00479606  014608            add   dword ptr [esi + 8], eax        ; res+8 += sub 数  ← count
0047960c  8d3c87            lea   edi, [edi + eax*4]             ; 名字串区 = 名字区 + n*4
00479616  8d14c500000000    lea   edx, [eax*8]
0047961e  e8ca0a0000        call  0x47a0ed                       ; malloc(count*8)
00479628  89868c000000      mov   dword ptr [esi + 0x8c], eax    ; res+0x8C = 新表  ← table
...
00479640  8b4604            mov   eax, dword ptr [esi + 4]
00479643  8b54860c          mov   edx, dword ptr [esi + eax*4 + 0xc]  ; 缓冲
00479647  8b4508            mov   eax, dword ptr [ebp + 8]            ; 偏移数组游标
0047964a  0310              add   edx, dword ptr [eax]                ; 缓冲 + offset[i]
0047964c  8b868c000000      mov   eax, dword ptr [esi + 0x8c]
00479652  8954c804          mov   dword ptr [eax + ecx*8 + 4], edx    ; table[i].+4 = 起始
00479656  8b968c000000      mov   edx, dword ptr [esi + 0x8c]
0047965c  8bc7              mov   eax, edi
0047965e  893cca            mov   dword ptr [edx + ecx*8], edi        ; table[i].+0 = 名字 char*（指向缓冲内名字串）
00479661  8d5801            lea   ebx, [eax + 1]                     ; 扫描到 NUL，推进到下一个名字
00479664  8a10              mov   dl, byte ptr [eax]
00479666  40                inc   eax
00479667  84d2              test  dl, dl
00479669  75f9              jne   0x479664
0047966b  83450804          add   dword ptr [ebp + 8], 4             ; 偏移数组 +4
0047966f  2bc3              sub   eax, ebx
00479671  41                inc   ecx
00479672  8d7c0701          lea   edi, [edi + eax + 1]
00479676  3b4e08            cmp   ecx, dword ptr [esi + 8]
00479679  7cc5              jl    0x479640
```
⇒ `res+0x8C` = 表；项 **8 字节**；`+0` = 名字 `char*`（指向已加载缓冲内的名字串，**运行期保留**）；`+4` = `buffer + offset[i]`（sub 数据基址）；`res+0xC+slot*4` = 文件缓冲数组。

### 3.3 按名查表 —— `FUN_004797D0`@`0x4797D0`（二分）

```asm
004797d5  8b5808            mov   ebx, dword ptr [eax + 8]       ; count
004797df  8b808c000000      mov   eax, dword ptr [eax + 0x8c]    ; table
004797f8  8b14f1            mov   edx, dword ptr [ecx + esi*8]   ; table[mid].+0 = 名字
00479800  8a01              mov   al, byte ptr [ecx]             ; 逐字节 strcmp(callerName, table[mid].name)
...
00479842  8b55fc            mov   edx, dword ptr [ebp - 4]       ; table
00479845  8b44f204          mov   eax, dword ptr [edx + esi*8 + 4] ; table[idx].+4
0047984b  83c010            add   eax, 0x10                      ; ★ +0x10 ⇒ 该 sub 的首条指令
00479852  c20400            ret   4
```
⇒ `FindSubByName(res, name) = table[idx].+4 + 0x10`。**`+0x10` 正是 sub 的前导 16 字节**（与 §2.1 指令头 `0x10` 一致）——即每个 sub 的数据以 16 字节前导开头，真正指令从 `+0x10` 起。

### 3.4 `GetMemAddr(0x4c2188, 0xac, 0xC)` 的展开（与 thprac `ECLHelper` 对齐）

`thprac_games.h:653-661`：
```cpp
void SetBaseAddr(void* addr) { mPtrToBuffer = addr; VFile::SetFile((uint8_t*)(*(uint32_t*)addr), 0x99999); }
void SetFile(unsigned int ord) { VFile::SetFile((uint8_t*)(*(uint32_t*)(mPtrToBuffer + ord*4)), 0x99999); }
```
⇒ `SetBaseAddr` 收到的指针 `P` 满足 `*(u32*)(P + ord*4)` = 第 ord 个 ECL 文件缓冲。展开 `GetMemAddr(0x4c2188, 0xac, 0xC) = *(u32*)(*(u32*)(0x4c2188)+0xac) + 0xC = res + 0xC` ⇒ **`res+0xC` 正是加载器用的 `res+0xC+ord*4` 数组首址**（§3.2）。**完全吻合**，`THSectionPatch` 的 `SetBaseAddr` 一级对象就是 `res`。

---

## 4. Q3 —— ECL 子调用栈（**存在、可遍历**）

### 4.1 结论

1. **有显式可遍历的子调用栈**，随解释器自动维护，与线程 T 内嵌：
   - **帧存储基址**：`base = T + 0x08`
   - **栈指针**：`SP = *(u32*)(T + 0x1008)`（int，**字节**偏移；`0` = 空栈/顶层）
   - **帧长**：**`0x0C`**（3 个 dword）
2. **帧内布局**（压帧顺序 = 地址升序）：`+0x00` = 旧 `SP`；`+0x04` = 调用时 `T+0`（时间，float）；**`+0x08` = 调用者当时的指令地址（= 调用者 `T+4`）**。
3. 「栈顶帧存的是**调用者 ip**」（不是调用者 sub 索引 —— 本作没有 subIdx）。当前指令仍是 `T+4`。
4. 栈区上限 **`0x1000` 字节**（`SP` 的有效域 `0 .. 0x1000`）⇒ 物理上限 ≈ 341 帧（`0x1000 / 0xC`）。
5. `SP == 0` 分支会把首帧的「旧 SP」槽规范化（写一个非 0 哨兵，见 `0x47705A`/`0x4771CA`），故遍历终点判据取 **`SP < 0x0C` 或旧 SP 为哨兵/0**。

### 4.2 `ret`（opcode `0x0A`）—— case @ `0x4772FF`/RVA `0x772FF`

```asm
004772ff  8d4308            lea   eax, [ebx + 8]              ; 栈基址
00477302  e859270000        call  0x479a60                    ;（清表达式临时值）
00477307  83bb0810000000    cmp   dword ptr [ebx + 0x1008], 0 ; SP == 0 ?
0047730e  0f84c7180000      je    0x478bdb                    ;   空 → T+4 = 0（线程结束）
00477314  01b308100000      add   dword ptr [ebx + 0x1008], esi  ; esi = -4
0047731a  8b8308100000      mov   eax, dword ptr [ebx + 0x1008]
00477320  8b441808          mov   eax, dword ptr [eax + ebx + 8] ; 读 frame[+0x08]
00477324  894304            mov   dword ptr [ebx + 4], eax      ; ★ T+4 = 调用者 ip
00477327  01b308100000      add   dword ptr [ebx + 0x1008], esi
0047732d  8b8308100000      mov   eax, dword ptr [ebx + 0x1008]
00477333  8b4c1808          mov   ecx, dword ptr [eax + ebx + 8] ; 读 frame[+0x04]
00477337  890b              mov   dword ptr [ebx], ecx          ; ★ T+0 = 调用者时间
00477339  01b308100000      add   dword ptr [ebx + 0x1008], esi
0047733f  837b0400          cmp   dword ptr [ebx + 4], 0
00477343  8b8308100000      mov   eax, dword ptr [ebx + 0x1008]
00477349  8b7c1808          mov   edi, dword ptr [eax + ebx + 8] ; 读 frame[+0x00]
0047734d  89bb08100000      mov   dword ptr [ebx + 0x1008], edi  ; ★ SP = 旧 SP
```
⇒ 弹栈顺序 `ip → time → 旧SP`，**帧长 0x0C**，与 §4.3 的压栈严格对称。

### 4.3 `call`（opcode `0x0B`）—— case @ `0x477502` → 压帧 `FUN_00477030`@`0x477030`

调用点：
```asm
00477502  6a00              push  0
00477504  53                push  ebx
00477505  8bf3              mov   esi, ebx
00477507  c6400c00          mov   byte ptr [eax + 0xc], 0
0047750b  e820fbffff        call  0x477030                  ; ← 压帧（含参数拷贝）
00477510  85c0              test  eax, eax
00477512  0f85ca160000      jne   0x478be2
00477518  8d70fc            lea   esi, [eax - 4]
0047751b  8d7804            lea   edi, [eax + 4]
0047751e  e960feffff        jmp   0x477383                  ; 回到「时间判定」
```
压帧体（节选，`edx` = T）：
```asm
004771a0  8b4df4            mov   ecx, dword ptr [ebp - 0xc]    ; SP0（进入前的 SP）
004771a3  8bba08100000      mov   edi, dword ptr [edx + 0x1008]
004771a9  85c9              test  ecx, ecx
004771ab  741d              je    0x4771ca                      ; SP0 == 0 → 走规范化分支
004771ad  838208100000fc    add   dword ptr [edx + 0x1008], -4
004771b4  8b8208100000      mov   eax, dword ptr [edx + 0x1008]
004771ba  8b441008          mov   eax, dword ptr [eax + edx + 8]
004771be  898a08100000      mov   dword ptr [edx + 0x1008], ecx   ; 复位 SP = SP0
004771c4  89441104          mov   dword ptr [ecx + edx + 4], eax  ; ★ 压 frame[+0x00] = 旧 SP
004771c8  eb0a              jmp   0x4771d4
004771ca  c7820810000004000000  mov dword ptr [edx + 0x1008], 4 ; SP0 == 0 的规范化
004771d4  8b9a08100000      mov   ebx, dword ptr [edx + 0x1008]
004771da  897c1308          mov   dword ptr [ebx + edx + 8], edi
004771e3  01ba08100000      add   dword ptr [edx + 0x1008], edi   ; edi = 4
004771e9  85c9              test  ecx, ecx
004771eb  7421              je    0x47720e
004771ed  8b8a08100000      mov   ecx, dword ptr [edx + 0x1008]
004771f3  8b1e              mov   ebx, dword ptr [esi]           ; T+0 = 时间
004771f5  895c1108          mov   dword ptr [ecx + edx + 8], ebx  ; ★ 压 frame[+0x04] = 时间
004771f9  01ba08100000      add   dword ptr [edx + 0x1008], edi
004771ff  8b8a08100000      mov   ecx, dword ptr [edx + 0x1008]
00477205  8b5e04            mov   ebx, dword ptr [esi + 4]        ; T+4 = 当前指令
00477208  895c1108          mov   dword ptr [ecx + edx + 8], ebx  ; ★ 压 frame[+0x08] = 调用者 ip
0047720c  eb1c              jmp   0x47722a
```
⇒ 压栈 `{旧SP, 时间, 调用者ip}`，**帧长 `0x0C`**，与 §4.2 弹栈次序完全对应。

### 4.4 遍历规则（推荐实现）

```cpp
// T = *(u32*)(H + 0x11D0)（或 *(u32*)(H+4)，两者恒等）
uint32_t sp = *(uint32_t*)(T + 0x1008);          // 0 = 无 caller
int n = 0;
while (sp >= 0x0C && sp < 0x1000 && n < 256) {
    uint32_t callerIp = *(uint32_t*)(T + 0x08 + sp - 4);   // 帧 +0x08 = 调用者 ip
    if (!callerIp) break;
    int callerSub = SubOfAddress(callerIp);               // §2.3 的地址区间表
    if (callerSub < 0) break;                             // 越界 → 停止
    ancestors[n++] = NameOf(callerSub);                   // table[callerSub].+0
    sp = *(uint32_t*)(T + 0x08 + sp - 0x0C);              // 帧 +0x00 = 旧 SP
}
// 祖先序：ancestors[0] = 直接调用者 … 最后 = 最外层；当前 sub 由 T+4 另行判定
```

- **只在指令边界（帧间稳定态，如 IGI 渲染）遍历**：局部/表达式栈共用基址 `T+8`、指针 `T+0x100C`（见 `0x4770BF`、`0x478F14`、`0x478FBA` 的 `[esi+0x100C]`），执行中遍历有误读风险。
- 保护：`T`、`sp`、`callerIp`、`callerSub` 全部校验。

### 4.5 异步 `spawn` 的例外

`FUN_00478DB0`（§5.3）对宿主按**线程链表 `H+0x11D0 → H+0x11D4`** 逐个跑 VM；`FUN_00479860`（`malloc(0x11E0)`）就是「另起一个线程对象」的构造器。主线的同步 `call` 链只在**主线宿主那一个 T** 上，读 `*(u32*)(H+0x11D0)` 不会混入异步线程。

---

## 5. Q4 —— 主线宿主捕获点

### 5.1 「spawn main」站点

字符串 `"main"` @ **`0x4AADE0`**（RVA `0xAADE0`，全程序仅此一处且仅被下面两处引用）。宿主工厂 `FUN_004183F0`：`malloc(0x52E4)` → `call 0x418F40` 构造 → **两条路径都 `mov eax, ebx` 返回宿主**。

```asm
; 站点 1（进关）
0042c74c  6a54              push  0x54
0042c74e  8d4c2414          lea   ecx, [esp + 0x14]
0042c752  53                push  ebx
0042c753  51                push  ecx
0042c754  e8d76b0500        call  0x483330                  ; memset(参数结构, 0, 0x54)
0042c75c  8d542410          lea   edx, [esp + 0x10]
0042c760  52                push  edx
0042c761  68e0ad4a00        push  0x4aade0                  ; "main"
0042c766  e885bcfeff        call  0x4183f0                  ; ★ EAX = 主线宿主（下一条 0x42C76B 可挂 EHOOK）

; 站点 2（另一重开/复位路径，同型）
0042c8fb  8d442410          lea   eax, [esp + 0x10]
0042c8ff  50                push  eax
0042c900  68e0ad4a00        push  0x4aade0                  ; "main"
0042c905  e8e6bafeff        call  0x4183f0                  ; ★ EAX = 主线宿主（下一条 0x42C90A 可挂 EHOOK）
```

⇒ **Q4 答案**：捕获点 = `0x42C76B` / `0x42C90A`，命中时 **`EAX` = 主线 ECL 宿主 H**。（对应总括里 TH15 的 `0x43BBC8/0x43D48E/0x43D6BE` 角色。）

### 5.2 宿主工厂如何入链表（可作静态兜底）

```asm
00418632  39bab0000000      cmp   dword ptr [edx + 0xb0], edi    ; edx = mgr, edi = 0
00418638  7523              jne   0x41865d                      ; 链表非空 → 追加
0041863a  ff82b8000000      inc   dword ptr [edx + 0xb8]        ; mgr+0xB8 = 计数
00418640  8d83cc140000      lea   eax, [ebx + 0x14cc]           ; ★ 节点 = H+0x14CC
00418646  8982b4000000      mov   dword ptr [edx + 0xb4], eax   ; mgr+0xB4 = 尾
0041864c  8982b0000000      mov   dword ptr [edx + 0xb0], eax   ; mgr+0xB0 = 头
00418652  8bc3              mov   eax, ebx                      ; ★ 返回宿主
0041865a  c20800            ret   8
```
宿主构造器里 `0x41904C: mov dword ptr [edi + 0x14cc], edi` ⇒ **节点 `+0` = 宿主自身**。
⇒ **静态兜底（无需钩子）**：`H = *(u32*)( *(u32*)(mgr + 0xB0) )`（链头）或 `*(u32*)( *(u32*)(mgr + 0xB4) )`（链尾）。⚠️ `0x4183F0` 共有 6 个调用点，只有 `0x42C766`/`0x42C905` 传 `"main"`（其余传其它脚本名），故「链头=main」只在 main 最先创建时成立 —— **推荐仍用捕获点**。

### 5.3 宿主 → 线程（`H+4` 与 `H+0x11D0`）

宿主构造器 `FUN_00418F40` 在 `0x418FA7` 调 `FUN_00417740(H)`，该函数把宿主与内嵌线程绑定：

```asm
00417740  83a0c8110000fe    and   dword ptr [eax + 0x11c8], 0xfffffffe
00417747  d9ee              fldz
00417749  33c9              xor   ecx, ecx
0041774b  d95808            fstp  dword ptr [eax + 8]        ; H+0x08 = 0.0f    ← T+0（时间）
0041774e  89480c            mov   dword ptr [eax + 0xc], ecx  ; H+0x0C = 0       ← T+4（ip）
00417751  89801c100000      mov   dword ptr [eax + 0x101c], eax ; T+0x1014 = H（ins 处理器 = 自身）
00417757  c78018100000ffffffff  mov dword ptr [eax + 0x1018], -1
...
00417797  8d5008            lea   edx, [eax + 8]
0041779a  895004            mov   dword ptr [eax + 4], edx  ; ★ H+4 = H+8
...
004177a9  8990d0110000      mov   dword ptr [eax + 0x11d0], edx ; ★ H+0x11D0 = H+8
004177af  8988d4110000      mov   dword ptr [eax + 0x11d4], ecx ; H+0x11D4 = 0（无后继线程）
```
⇒ **`T = H + 0x08`（内嵌）；`H+4 = T`；`H+0x11D0 = T`**（三者指向同一处）。`FUN_00478DB0` 每帧再刷新一次：

```asm
00478db0  55                push  ebp
00478db1  8bec              mov   ebp, esp
00478db3  83e4f8            and   esp, 0xfffffff8
00478db6  83ec08            sub   esp, 8
00478db9  53                push  ebx
00478dba  56                push  esi
00478dbb  8db7d0110000      lea   esi, [edi + 0x11d0]        ; 线程链表头
00478dc1  c744240801000000  mov   dword ptr [esp + 8], 1
00478dc9  85f6              test  esi, esi
00478dcb  746f              je    0x478e3c
00478dd0  8b4604            mov   eax, dword ptr [esi + 4]
00478dd3  d94508            fld   dword ptr [ebp + 8]
00478dd6  8b1e              mov   ebx, dword ptr [esi]       ; ebx = T
00478dd8  51                push  ecx
00478dd9  d91c24            fstp  dword ptr [esp]
00478ddc  89442410          mov   dword ptr [esp + 0x10], eax
00478de0  895f04            mov   dword ptr [edi + 4], ebx  ; ★ 刷新 H+4 = T
00478de3  e8b8e4ffff        call  0x4772a0                   ; ★ 跑一帧 ECL VM
00478de8  837c240800        cmp   dword ptr [esp + 8], 0
```

> ⚠️ **实现注意**：宿主构造后到第一帧 `FUN_00478DB0` 之前，`H+4`/`H+0x11D0` 由 `FUN_00417740`（`0x418FA7`）在**构造期间**就已设好；但**建议读取仍放在帧间稳定态**（IGI 渲染），并做非空校验。

---

### 5.4 boss 宿主可达性 —— Q-A / Q-B / Q-C（2026-10-18 静态定案）

> **起因**：`docs/logger/logger_progress.md` §4 第 11 条、本作入口表小结 `th13_checkpoint_entry_report.md` 第 10 条、监控方案 `th13_monitor_plan.md` §9 风险 1 / §7 验证 6 把「th13 boss 宿主 / 线程链 / 槽表」列为**最大 🟡 缺口**——因为 §5（Q4）给的只是**主线宿主**的捕获点（`"main"` 的 spawn 站点），而 boss 是**另一个 ECL 宿主**。
> **本节结论**：缺口**消解**。宿主链是全局链、boss 宿主由 ECL `enmCreateA` 经同一工厂另起、其 `res` 就是那个全局 `res` ⇒ **boss 行用现有锚点即可实现，不需要新增捕获点**。
> 方法：`python` + `pefile` + `capstone` 只读反汇编（`D:\workshop\reasm\th13\dl.py`）+ `thecl -d` 反编译文本（`../exes/ecl_dump/th13/*.decl`）+ 离线 SCPT 解析（`eclutil.py`）。**未跑游戏、未跑编译、未改任何字节。**
> 状态：**全部 🟡（静态定案、未真机）**。

#### 5.4.1 Q-A —— 宿主链表覆盖 boss 宿主：✅ **是（全局全宿主链）**

**判据 1：全程序只有一个宿主分配点。**
字节搜索 `68 E4 52 00 00`（`push 0x52E4`）**全程序仅 1 处** = `0x4183FC`（在 `FUN_004183F0` 内）。⇒ **所有** ECL 宿主对象都出自 `FUN_004183F0`，没有第二个分配路径。

**判据 2：该工厂无条件把新宿主挂进 `mgr+0xB0`。**

```asm
; —— 空表分支：头 = 尾 = 新节点（FUN_004183F0 内）——
0041862f  8b55fc              mov   edx, dword ptr [ebp - 4]      ; edx = mgr（0x418401 存下）
00418632  39bab0000000        cmp   dword ptr [edx + 0xb0], edi   ; edi = 0 ⇒ 链表空？
00418638  7523                jne   0x41865d                      ;   非空 → 追加分支
0041863a  ff82b8000000        inc   dword ptr [edx + 0xb8]        ; mgr+0xB8 = 宿主计数 ++
00418640  8d83cc140000        lea   eax, [ebx + 0x14cc]           ; ★ 节点 = H+0x14CC
00418646  8982b4000000        mov   dword ptr [edx + 0xb4], eax   ; mgr+0xB4 = 尾 = 节点
0041864c  8982b0000000        mov   dword ptr [edx + 0xb0], eax   ; mgr+0xB0 = 头 = 节点
00418652  8bc3                mov   eax, ebx                      ; ★ 返回宿主
0041865a  c20800              ret   8

; —— 非空分支：追加到尾部 ——
0041865d  8b82b4000000        mov   eax, dword ptr [edx + 0xb4]   ; eax = 尾节点
00418663  8b7004              mov   esi, dword ptr [eax + 4]      ; 尾->next
00418666  8d8bcc140000        lea   ecx, [ebx + 0x14cc]           ; ecx = 新节点
...
00418681  894804              mov   dword ptr [eax + 4], ecx      ; 尾->next = 新节点
00418685  894108              mov   dword ptr [ecx + 8], eax      ; 新->prev = 尾
00418688  ff82b8000000        inc   dword ptr [edx + 0xb8]        ; 计数 ++
00418691  898ab4000000        mov   dword ptr [edx + 0xb4], ecx   ; mgr+0xB4 = 新尾
0041868f  8bc3                mov   eax, ebx                      ; ★ 返回宿主
0041869b  c20800              ret   8
```
节点 `+0` = 宿主自身（构造器 `0x41904C: mov dword ptr [edi + 0x14cc], edi`）⇒ 节点 = `{+0 = H, +4 = next, +8 = prev}`（与 §5.2 一致）。

**判据 3：引擎自己迭代宿主用的就是这条链。**

```asm
; FUN_00414180(mgr) —— 对链上每个宿主调 vtable+0x14（关卡起点的「复位全部宿主」）
004141ee  8b87b0000000        mov   eax, dword ptr [edi + 0xb0]   ; eax = mgr+0xB0 = 头节点（edi = mgr）
004141f4  85c0                test  eax, eax
004141f6  741a                je    0x414212
004141f8  8b7004              mov   esi, dword ptr [eax + 4]      ; ★ esi = 节点->next
004141fb  8b00                mov   eax, dword ptr [eax]          ; ★ eax = 节点->H
004141fd  85c0                test  eax, eax
004141ff  740b                je    0x41420c
00414201  8b10                mov   edx, dword ptr [eax]          ; H->vtable
00414203  8bc8                mov   ecx, eax
00414205  8b4214              mov   eax, dword ptr [edx + 0x14]   ; vtable slot 0x14
00414208  6a01                push  1
0041420a  ffd0                call  eax
0041420c  8bc6                mov   eax, esi
0041420e  85f6                test  esi, esi
00414210  75e6                jne   0x4141f8                      ; ★ 沿 +4 走完整条链
```
同型遍历另有两处：`FUN_0041AC70` @ `0x41AC7C`（`mov eax,[mgr+0xB0]` → `ebx = [eax]` / `edi = [eax+4]` 循环，读 `[H+0x521C]` 标志位）、`FUN_00417E90` @ `0x417E96`（`ecx = [mgr+0xB0]` → `edx = [ecx+4]` / `ecx = [ecx]` 循环，按 `[H+0x521C]` / `[H+0x51BC]` 计数）。⇒ **`mgr+0xB0` 就是「全部宿主」的注册表**。

**判据 4：boss / 中boss 宿主确实经由该工厂创建** —— 见 §5.4.2。

> **⇒ Q-A 答案：含 boss。** 遍历 `mgr+0xB0`（`node = *(u32*)(mgr+0xB0)`；`H = *(u32*)node`；`node = *(u32*)(node+4)`）即可枚举**全部**宿主，其中就有 boss / 中boss 宿主。
> **⇒ 监控方案 §7 验证 6 可降级**：不必再问「boss 宿主是否挂 `mgr+0xB0` 链」；改为「按宿主链取 `res` 后，boss 行的 `T+4` → 名字是否与画面一致」。
> ⚠️ **对 §5.2「静态兜底」的补充**：`H = *(u32*)(*(u32*)(mgr+0xB0))` 取的是**链头**；链头 = **最先创建**的宿主。进关时 `"main"` 宿主（`0x42C766`/`0x42C905`）先建、boss 宿主（`MainBoss` 里 `enmCreateA("Boss",…)`）后建并追加到尾部 ⇒ 链头通常仍是主线宿主。**但正确做法是遍历整条链**（§5.4.1 判据 3 的写法），不要只取链头。

#### 5.4.2 Q-B —— boss 宿主的捕获点：`0x41FE67` / RVA `0x1FE67`（5B，`EAX` = 新宿主）

**宿主工厂 `FUN_004183F0` 的全部 6 个调用点**（`e8 rel32` 目标 = `0x4183F0` 的全量扫描）：

| # | 调用点 VA / RVA | 名字参数（`[ebp+8]`） | 语义 | boss？ |
| --- | --- | --- | --- | --- |
| 1 | `0x42C766` / `0x2C766` | `"main"` @ `0x4AADE0`（`push` 于 `0x42C761`） | **主线宿主**（进关） | — |
| 2 | `0x42C905` / `0x2C905` | `"main"` @ `0x4AADE0`（`push` 于 `0x42C900`） | **主线宿主**（重开/复位路径） | — |
| 3 | **`0x41FE62` / `0x1FE62`** | **ECL 指令的 `ins+0x14`**（`0x41FE5B: add ebx,0x14; push ebx`） | **`enmCreate*` 另起脚本宿主** | ✅ **是** |
| 4 | `0x414925` / `0x14925` | `table[[this+0x1EC]].+0`（`0x414913`/`0x414919`，从全局 sub 表按索引取名字） | `FUN_004146D0` 的状态机路径（`[this+0x30]` 状态 3 分支；**用途未追**） | 否（非 boss 入口） |
| 5 | `0x422F1A` / `0x22F1A` | 字面量 **`"BossCard2_EtAt"`** @ `0x4AABD8`（`push` 于 `0x422F05`；该名字是 `st07bs.decl:1281` 的子脚本） | `FUN_00422E40`（读 `[0x4C2174]` / `[0x4C22C4]`(PLAYER_PTR) 与宿主 `+0xB74`/`+0xB78`；**触发条件未追**）——**引擎侧硬编码路径，不是 ECL `enmCreate*`** | 疑似 boss 侧宿主（非本作 boss 行的入口） |
| 6 | `0x40FF0F` / `0xFF0F` | `[edi+0x28]`（运行期指针） | `FUN_0040F030`（`[this+0xBD0] < 0x12`，0x2C 步长表；**用途未追**） | 否（非 boss 入口） |

**boss / 中boss 宿主走的是 #3**：`enmCreateA` 的扩展 opcode 是 **`0x12D`**，其分派链：

```asm
; —— 扩展 opcode 分派（FUN_0041B260，opcode 域 ≥0x12C；`edi` = H+0x11EC）——
0041b2c3  81fabf020000        cmp   edx, 0x2bf                    ; edx = opcode - 0x12C
0041b2c9  0f8769440000        ja    0x41f738
0041b2cf  0fb692d8f94100      movzx edx, byte ptr [edx + 0x41f9d8]  ; ★ 字节映射表
0041b2d6  ff249560f74100      jmp   dword ptr [edx*4 + 0x41f760]  ; ★ 跳表
; —— 字节映射 idx0（opcode 0x12C / 0x12D / 0x130 / 0x131 / 0x141）→ 0x41B2EC ——
0041b2ec  57                  push  edi                           ; edi = H+0x11EC（thiscall this）
0041b2ed  e8ce490000          call  0x41fcc0                      ; ★ 另起宿主
0041b2f2  e941440000          jmp   0x41f738
```
> `0x41F9D8` = 字节映射（与主解释器的 `0x478D44` 同构）；idx0 覆盖 `0x12C/0x12D/0x130/0x131/0x141`。**离线核对**（`eclutil.py` 解码 + `thecl -d` 文本对照）：`0x12C = enmCreate`（`MainSub00@0x5868: enmCreate("GirlRedA01",…)`）、**`0x12D = enmCreateA`**（`MainBoss@0x5220: enmCreateA("Boss",…)`、`main@0x8C50: enmCreateA("MBoss",…)`）、`0x130 = enmCreateM`、`0x141 = enmMapleEnemy`。

`FUN_0041FCC0` 内（`edi` = 调用方的 `H+0x11EC`；`[edi+0x40C8]` = `H+0x52B4` = 宿主自身，构造器 `0x418FB9` 写入）：

```asm
0041fcc3  a188214c00          mov   eax, dword ptr [0x4c2188]     ; mgr
0041fcc8  8b88b8000000        mov   ecx, dword ptr [eax + 0xb8]   ; 当前宿主数
0041fcd4  3b88c4000000        cmp   ecx, dword ptr [eax + 0xc4]   ; vs 宿主数上限
0041fcda  0f8d87010000        jge   0x41fe67                      ; ⚠️ 超限 → 直接跳到「返回」处
...
0041fe38  8b4508              mov   eax, dword ptr [ebp + 8]      ; eax = edi = H+0x11EC
0041fe3b  8db788020000        lea   esi, [edi + 0x288]            ; 父宿主的 0xC dword 配置块
0041fe41  b90c000000          mov   ecx, 0xc
0041fe46  8d7dc0              lea   edi, [ebp - 0x40]
0041fe49  f3a5                rep movsd                           ; 拷贝到栈上 cfg 结构（+0x20 起）
0041fe4b  8b88c8400000        mov   ecx, dword ptr [eax + 0x40c8]  ; ecx = H
0041fe51  8b91e4110000        mov   edx, dword ptr [ecx + 0x11e4]  ; 父宿主 id
0041fe57  8d45a0              lea   eax, [ebp - 0x60]             ; cfg 结构首址
0041fe5a  50                  push  eax                           ; arg2 = cfg
0041fe5b  83c314              add   ebx, 0x14                     ; ★ 名字 = 当前 ECL 指令的 +0x14
0041fe5e  53                  push  ebx                           ; arg1 = name
0041fe5f  8955f0              mov   dword ptr [ebp - 0x10], edx
0041fe62  e88985ffff          call  0x4183f0                      ; ★ 宿主工厂 ⇒ EAX = 新宿主
0041fe67  5f                  pop   edi                           ; ← ★ 可挂 5B EHOOK（EAX = 新宿主）
0041fe68  5e                  pop   esi
0041fe69  33c0                xor   eax, eax
0041fe6b  5b                  pop   ebx
0041fe6c  8be5                mov   esp, ebp
0041fe6e  5d                  pop   ebp
0041fe6f  c20400              ret   4
```
字节核对：`0x41FE62` = `E8 89 85 FF FF`（`call 0x4183F0`，5B）；`0x41FE67` = `5F 5E 33 C0 5B`（`pop edi; pop esi; xor eax,eax; pop ebx`，正好 **5B**）。

**⇒ Q-B 答案**：**boss / 中boss 宿主的捕获点 = `0x41FE67` / RVA `0x1FE67`（5B EHOOK，命中 `EAX` = 新宿主）**。
- ⚠️ 该点对**每一次** `enmCreate*`（含 `GirlRedA01` 等敌方波次宿主、`MapleEnemy`）都命中 ⇒ 若要用它，必须按**名字过滤**（宿主创建后 `T+4 = FindSubByName(res, name)+0x10`，可经 `T+4` 的地址区间反查回名字）。
- ⚠️ `0x41FCDA: jge 0x41FE67` 的**宿主数超限早退**分支会**跳到同一地址**，此时 `EAX` = `mgr`（`0x41FCD4` 处 `eax` 尚是 `[0x4C2188]`），**不是**新宿主 ⇒ 用作捕获点时须校验 `EAX != 0 && EAX != mgr`。
- ✅ **但按 Q-A 结论，本点并非必需**：宿主链 `mgr+0xB0` 已覆盖 boss 宿主，监控方案按链遍历即可，**不必新增捕获点**。此点仅作为「想要 boss 宿主诞生瞬间」时的备选锚点登记。

#### 5.4.3 Q-C —— boss 宿主的 `res` 里**有** boss sub：✅ **同一个全局 res**

**判据 1：宿主构造器无条件把 `H+0x11CC` 指向全局 `res`。**（`FUN_00418F40` = 宿主构造器，被 `FUN_004183F0` 在 `0x418416` 调用）

```asm
0041917e  a188214c00          mov   eax, dword ptr [0x4c2188]     ; eax = mgr
00419195  899764140000        mov   dword ptr [edi + 0x1464], edx
0041919b  899fe0110000        mov   dword ptr [edi + 0x11e0], ebx
004191a7  8b88bc000000        mov   ecx, dword ptr [eax + 0xbc]   ; mgr+0xBC = 宿主 id 计数器
004191ad  898fe4110000        mov   dword ptr [edi + 0x11e4], ecx ; H+0x11E4 = 本宿主 id
...
004191e4  8b80ac000000        mov   eax, dword ptr [eax + 0xac]   ; ★ res = *(u32*)(mgr+0xAC)
004191ea  52                  push  edx                           ; edx = [ebp+8] = 名字
004191eb  8987cc110000        mov   dword ptr [edi + 0x11cc], eax ; ★★ H+0x11CC = res（无条件，每个宿主）
004191f1  e8da050600          call  0x4797d0                      ; FindSubByName(res, name)
004191f6  8b4f04              mov   ecx, dword ptr [edi + 4]      ; ecx = T（= H+8）
004191f9  894104              mov   dword ptr [ecx + 4], eax      ; ★ T+4 = 该 sub 首指令（= 起始 ip）
```
⇒ **每个**宿主（主线 / boss / 中boss / 敌方波次）在构造期就把 `H+0x11CC` 设为 `*(u32*)(mgr+0xAC)`，并把内嵌线程的起始 `ip` 设为 `FindSubByName(res, name)+0x10`。**th13 是「所有宿主共用一个全局 res」**（与 th14 同法；**不是** th16 那种「boss 段另加载一份 ECL」）。
> 旁证：`FUN_00418F40` 在 `0x418FA7` 调 `FUN_00417740`，该函数在 `0x4177A9` 写 `H+0x11D0 = H+8` ⇒ **boss 脚本跑在宿主的「内嵌线程」上**（`T = *(u32*)(H+0x11D0)`），与主线宿主同构 ⇒ 监控方案 §3.1 的线程链取法对 boss 宿主成立。

**判据 2：`res+0x8C` 表是跨文件累积的全局表。**（加载器 `FUN_004795B0`，`esi` = `res`）

```asm
004795b9  8b4604              mov   eax, dword ptr [esi + 4]      ; res+4 = 当前槽号
004795bf  894c860c            mov   dword ptr [esi + eax*4 + 0xc], ecx ; res[+0xC+slot*4] = 文件缓冲
004795ca  813953435054        cmp   dword ptr [ecx], 0x54504353  ; 魔数 "SCPT"
004795f7  8b9e8c000000        mov   ebx, dword ptr [esi + 0x8c]   ; ebx = 旧表（0 = 首次）
00479602  0fb74010            movzx eax, word ptr [eax + 0x10]     ; 本文件 sub 数
00479606  014608              add   dword ptr [esi + 8], eax      ; ★ res+8（总数）+= 本文件 sub 数
00479612  85db                test  ebx, ebx
00479614  756a                jne   0x479680                      ; ★ 表已存在 → 走「保留旧表」分支
...
; —— 0x479680：旧表非空 ⇒ malloc(新总数*8) + memcpy(旧表) + free(旧表) ——
00479680  03c0                add   eax, eax                      ; eax = res+8 = 新总数
00479682  03c0                add   eax, eax
00479684  03c0                add   eax, eax
00479686  50                  push  eax                           ; malloc(新总数 * 8)
00479687  e8610a0000          call  0x47a0ed
0047968c  8b4df4              mov   ecx, dword ptr [ebp - 0xc]
0047968f  89868c000000        mov   dword ptr [esi + 0x8c], eax   ; res+0x8C = 新表
00479695  8b11                mov   edx, dword ptr [ecx]
00479697  0fb75210            movzx edx, word ptr [edx + 0x10]     ; 本文件 sub 数
0047969b  8b4e08              mov   ecx, dword ptr [esi + 8]
0047969e  2bca                sub   ecx, edx                      ; 旧总数 = 新总数 − 本文件 sub 数
004796a0  894dfc              mov   dword ptr [ebp - 4], ecx      ; [ebp-4] = 旧总数（下面据此定位）
004796a3  03c9                add   ecx, ecx
004796a5  03c9                add   ecx, ecx
004796a7  03c9                add   ecx, ecx
004796a9  51                  push  ecx                           ; 旧表字节数
004796aa  53                  push  ebx
004796ab  50                  push  eax
004796ac  e87f520100          call  0x48e930                      ; ★ memcpy(新表, 旧表, 旧表字节数)
004796b8  53                  push  ebx
004796b9  e8c30a0000          call  0x47a181                      ; free(旧表)
; —— 0x4796E0 起：对本文件每个名字做「二分定位 + 整体上移一格 + 写入」——
0047972a  8b4e08              mov   ecx, dword ptr [esi + 8]      ; 总数
0047972d  49                  dec   ecx
00479732  8b968c000000        mov   edx, dword ptr [esi + 0x8c]
00479738  8d04ca              lea   eax, [edx + ecx*8]
0047973b  8b50f8              mov   edx, dword ptr [eax - 8]      ; ★ 逐项上移一格（插入排序）
0047973e  8910                mov   dword ptr [eax], edx
00479740  8b50fc              mov   edx, dword ptr [eax - 4]
00479744  895004              mov   dword ptr [eax + 4], edx
...
0047975d  894cd804            mov   dword ptr [eax + ebx*8 + 4], ecx ; table[i].+4 = 本文件缓冲 + offset[i]
00479769  893cd9              mov   dword ptr [ecx + ebx*8], edi     ; table[i].+0 = 名字 char*
...
0047979e  8b7e04              mov   edi, dword ptr [esi + 4]
004797a1  8d4701              lea   eax, [edi + 1]
004797a4  894604              mov   dword ptr [esi + 4], eax      ; ★ 槽号自增
```
⇒ **同一张表、同一个 `res+8` 计数**，按**名字字典序**插入；每加载一个文件就把它全部 sub 并进这张表。
> 单文件加载入口 `FUN_00417E20`（`0x417E20`，把文件名拼进 `0x4E5370` 缓冲 → `call 0x4724C0` 读盘 → `call 0x4795B0`），是 **vtable 槽 `0x4AAC38`** 上的方法（`0x417E20` 作为数据出现在 `0x4AAC38`）。

**判据 3：关卡头 `ecli{}` 就声明了 boss 侧文件。**（`../exes/ecl_dump/th13/*.decl` 头 2 行）

| 关 | `ecli{}` | boss 侧文件 |
| --- | --- | --- |
| ST1 | `default.ecl` ; `st01bs.ecl` | `st01bs`（中boss 内联在 `st01.ecl`） |
| ST2 | `default.ecl` ; `st02mbs.ecl` ; `st02bs.ecl` | `st02mbs` + `st02bs` |
| ST3 | `default.ecl` ; `st03mbs.ecl` ; `st03bs.ecl` | `st03mbs` + `st03bs` |
| ST4 | `default.ecl` ; `st04mbs.ecl` ; `st04bs.ecl` | `st04mbs` + `st04bs` |
| ST5 | `default.ecl` ; `st05mbs.ecl` ; `st05bs.ecl` | `st05mbs` + `st05bs` |
| ST6 | `default.ecl` ; `st06bs.ecl` | 仅 `st06bs`（**未加载 `st06mbs`**） |
| ST7 | `default.ecl` ; `st07mbs.ecl` ; `st07bs.ecl` | `st07mbs` + `st07bs` |

**判据 4：白名单与「加载集」逐行比对 116/116 命中。** 用 `eclutil.py` 解析各关 `ecli{}` 声明的全部 `.ecl`，取其 sub 名并集，再与 `docs/logger/th13_checkpoints.csv` 第 1 列（去掉 `+0x…` 偏移后缀）逐行比对：

```
rows: 116   missing: 0
```
⇒ 44 个章节行 + **72 个 boss 行**的入口 sub 名**全部**落在该关运行期 `res+0x8C` 表的名字集合内。（ST6 无 MID 行，与其 `ecli{}` 未加载 `st06mbs` 自洽。）

**⇒ Q-C 答案：有。** boss 宿主的 `res` = 全局 `res`，其 `res+0x8C` 表含 `stNNbs.ecl` / `stNNmbs.ecl` 的全部 sub 名（`Boss`/`Boss1..8`/`BossCard1..10`/`MBoss*`/`MBossCardN`）。⇒ **「地址 → 名字」区间反查（§2.3）对 boss 行成立**，监控方案里 `H+0x11CC`（宿主 → res）的取法**对 boss 宿主成立**。
> 另一处交叉印证：`call` opcode（case `0x0B`）就是用**宿主的 res** 解析被调名字的——
> `0x47723F: mov edi,[esi+0x1014]`（`T+0x1014` = 宿主自身 H）→ `0x477245: mov eax,[edi+0x11cc]`（= H 的 res）→ `0x47724F: call 0x4797D0`（`FindSubByName(res, [T+4]+0x14)`）⇒ boss 宿主里 `Boss` 能 `call BossCard1`，正是因为 `Boss`/`BossCard1` 同在 `res` 的那张表里。

#### 5.4.4 对监控方案的影响（一句话）

**boss 行可用现有锚点实现**：`mgr+0xB0` 宿主链（Q-A 全局）→ 每宿主 `H+0x11D0` 线程链（Q-C 判据 1 旁证：boss 跑在内嵌线程）→ `ip = *(u32*)(T+4)` → `H+0x11CC` 的 res 表（Q-C：含 boss sub）→ 地址区间反查 → 白名单。**不需要新增任何捕获点**；`0x41FE67`（Q-B）仅在需要「boss 宿主诞生瞬间」时备用。

---

## 6. Q5 —— 解释器命中点 `0x4772D2` 与寄存器语义

### 6.1 命中点

```asm
004772d2  8b4304            mov   eax, dword ptr [ebx + 4]     ; ★ EHOOK(6B) 命中：eax = T+4（当前指令）
004772d5  8a480a            mov   cl, byte ptr [eax + 0xa]      ; 指令选择掩码
004772d8  848b1c100000      test  byte ptr [ebx + 0x101c], cl
004772de  0f8493000000      je    0x477377
```
- **地址**：`0x4772D2` / RVA `0x772D2`；**覆盖 6 字节**（`8B 43 04` + `8A 48 0A`，正好两条完整指令）⇒ 可挂 6B EHOOK。
- **命中寄存器**：**`EBX` = 执行对象 T**（脚本上下文）；`T+4` = 当前（下一条待执行）指令地址；`T+0` = 当前时间。
- **语义**：ECL 主解释器的**逐指令分发循环头**。

### 6.2 所在函数与调用链

- **函数**：`FUN_004772A0`，入口 `0x4772A0`（RVA `0x772A0`），函数体 `0x4772A0`–`0x478BEA`。
- **opcode 分发**：`0x4772E4` 取 `(i16)[ins+4]`；`> 0x5D` → 走 `0x478B9D`（对 `[T+0x1014]` 的**虚调用**，即扩展指令处理器 `FUN_0041B260`）；`≤ 0x5D` → 经字节映射 `0x478D44` → 跳表 `0x478C38`。
- 映射核实：**`0x0A`(ret) → `0x4772FF`；`0x0B`(call) → `0x477502`；`0x0C`(goto) → `0x47758E`**（与 th15/th16 的 `0x0A/0x0B/0x0C` 编号一致）。
- **调用链**：`FUN_00478DB0`（`0x478DB0`，遍历宿主线程链表 `H+0x11D0`，把 `ebx = T` 后 `call FUN_004772A0`）→ 被 `0x419F07`/`0x41A8F8`/`0x41AC3D`/`0x44B454` 调用（关卡/实体更新路径）。
- **返回值**：`0` = 本帧到此为止（下一条指令时间未到，已把 `[ebp+8]` 的帧增量加到 `T+0`）；`−1` = 脚本结束（`T+4` 置 0）。

### 6.3 作为「每帧稳定触发点」的评价

- `FUN_004772A0` 由 `FUN_00478DB0` **每帧对每个活跃脚本线程调用一次**；`0x4772D2` 在**每次执行/尝试执行一条指令**时命中（包含 `ins_23`/`wait` 的等待重试 —— 等待期每帧都会走到「时间未到」分支，但**不经过** `0x4772D2`）。
- ⚠️ **局限**：处于「时间未到、纯等待」时，`0x4772A0` 会在 `0x4772C4` 就跳去 `0x477397` 累加时间并返回，**不经过 `0x4772D2`**。因此它**不是无条件每帧**。
- ⚠️ 它会**对所有脚本对象**命中 ⇒ 必须用 §5.1 捕获的 H 做过滤（`T == *(u32*)(H+0x11D0)`），否则会把敌方/异步线程也算进来。
- **推荐做法（与 th15 同风格）**：**不挂 VM 钩子**。只在 `0x42C76B`/`0x42C90A` 抓一次 H，然后在 IGI 渲染（帧间稳定态）读 `T = *(u32*)(H+0x11D0)`、`ip = *(u32*)(T+4)`。`0x4772D2` 作为「需要无条件每帧触发」时的备选。

### 6.4 ⚠️ 对 requests §1「切入点」的更正（重要）

requests 建议从 `th13_lock_timer3 0x41D399`（"set boss mode case 512"）/ `lock_timer2 0x41E26C`（"SetNextPattern case 514"）/ `lock_timer1 0x4250EA` 反汇编「解释器」。实测：

- 这三处（以及 `0x4250EA`，其实是**全局计时器对象** `[0x4C2190]` 的字段写）**不在 ECL 主解释器里**，而在 **`FUN_0041B260`@`0x41B260`** 里；
- `FUN_0041B260` 是**扩展 opcode 处理器**，入口经蹦床 `0x41B250: add ecx,0x11EC; jmp 0x41B260`（vtable slot 0 / 0x1C），`this` = `H+0x11EC`；它用 `opcode - 0x12C` 索引自己的跳表（`cmp edx,0x2BF`，即 opcode 域 `0x12C..0x3EB`，**512/514 落在此域**）：
  ```asm
  0041b250  81c1ec110000      add   ecx, 0x11ec
  0041b256  e905000000        jmp   0x41b260
  0041b260  55                push  ebp
  ...
  0041b2a0  8bf9              mov   edi, ecx                    ; edi = H+0x11EC
  0041b2a2  8b87c8400000      mov   eax, dword ptr [edi + 0x40c8]  ; = [H+0x52B4] = H（自指针）
  0041b2a8  8b4804            mov   ecx, dword ptr [eax + 4]    ; T = [H+4]
  0041b2ab  8b5904            mov   ebx, dword ptr [ecx + 4]    ; 当前指令 = [T+4]   ← 与 §1 完全一致
  0041b2ae  0fb74b04          movzx ecx, word ptr [ebx + 4]       ; opcode
  0041b2b5  81c2d4feffff      add   edx, 0xfffffed4             ; opcode - 0x12C
  0041b2c3  81fabf020000      cmp   edx, 0x2bf
  0041b2d6  ff249560f74100    jmp   dword ptr [edx*4 + 0x41f760]
  ```
  ⇒ 该函数与主解释器读的是**同一个 `T+4`**，**互为交叉印证**。
- **结论**：requests §1 的「锁时钩子」是**旁证/交叉印证**，不是读「当前 sub」的正确入口；正确入口是 `0x42C76B`/`0x42C90A`（宿主）+ `T+4`（ip）。

---

## 7. Q6 —— 运行期保留 sub 名

✅ **保留**。加载器 `FUN_004795B0` 在 `0x47965E` 把**名字区指针**（已加载 ECL 缓冲内的名字串首址）直接写入 `table[i].+0`（见 §3.2）；`0x479661`–`0x479672` 逐字节扫 NUL 推进到下一个名字。故运行期 `name = *(char**)(table + i*8)` 直接可读，**无需离线名表**。

> ⚠️ **但本作需要一个「地址 → i」的反查**（因为 Q1 是地址而非索引），见 §2.3。反查只需 `table[i].+4 + 0x10`，**不需要离线 `.ecl`**；离线 `.ecl` 只用于「段落入口白名单」本身。

---

## 8. Q7 —— 机体 / 子机 / 难度地址

| 名称 | VA / RVA | 说明 | 置信度 |
| --- | --- | --- | --- |
| `CHARA` | `0x4BE7B8` / `0xBE7B8` | thprac 既有（`thprac_th13.cpp:17`），全程序 109 处引用，例如 `thprac_th13.cpp:2149` 用它区分角色 | ✅ 地址有效（语义待真机） |
| `SHOT/SHOTTYPE` | `0x4BE7BC` / `0xBE7BBC` | **推测**：紧邻 `CHARA` 的下一个全局 dword，xref 48 处（集中出现在 `0x40A080`/`0x40A2C0`/`0x41315C`/`0x4132F0`/`0x413B2C`/`0x4144FE` 等**与 `CHARA` 相邻引用**的位置），形态与 th12 的 `CHARA=0x4b0c90 / SUBSHOT=0x4b0c94` 同构 | 🟡 待实测 |
| 难度/rank | `0x4BE7C4` / `0xBE7C4` | 91 处引用；`0x41847E` 处 `mov ecx,[0x4be7c4]` 后 `shl dl,cl` 生成位掩码 ⇒ 疑为难度/rank（也用于 `T+0x101C` 的选择掩码） | 🟡 待实测 |
| `MODEFLAGS` | `0x4BE830` / `0xBE830` | thprac 既有（90 处引用） | ✅ |
| `STAGE_NUM` | `0x4BE81C` / `0xBE81C` | thprac 既有（63 处引用） | ✅ |
| `PLAYER_PTR` | `0x4C22C4` / `0xC22C4` | thprac 既有 | ✅ |
| `REPLAY_MGR_PTR` | `0x4C22C8` / `0xC22C8` | thprac 既有 | ✅ |
| `PauseInf` | `0x4C2194` / `0xC2194` | 见 `th13_re_disasm_report.md` §1 | ✅ |

> 说明：本迭代 IGI 只需显示「当前段落」，Q7 非必需；上表供后续日志迭代。`0x4BE7BC` / `0x4BE7C4` **未经真机取值核对，勿直接用于判据**。

---

## 9. Q8 —— 练习 `ECLJump` 对读取的影响

### 9.1 结论

✅ **不影响 sub 判读**（与 th15 §2 / th16 §Q8 一致）。

### 9.2 依据

1. **补丁写入**（`thprac_th13.cpp:936-944`）：
   ```cpp
   ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
   ```
   6 个 dword = 24B（`size = 0x18`）：`+0`=`ecl_time`(int)、`+4`=`0x0018000C`（低 16 位 `0x000C` = **`goto`**，高 16 位 `0x0018` = size）、`+8`=`0x02FF0000`（`ins[0xA]=0xFF` = 任意难度；`ins[8..9]=0` = 无浮点参数）、`+0xC`=0（局部槽数 0）、`+0x10`=`dest-start`、`+0x14`=`at_frame`。
2. **`goto` 语义**（case `0x0C` @ `0x47758E`/RVA `0x7758E`）：
   ```asm
   0047758e  8b4304            mov   eax, dword ptr [ebx + 4]       ; 当前指令
   00477591  db4014            fild  dword ptr [eax + 0x14]         ; ins[+0x14] = at_frame（int）
   00477594  d91b              fstp  dword ptr [ebx]                ; ★ T+0 = at_frame（时间，绝对赋值）
   00477596  8b4810            mov   ecx, dword ptr [eax + 0x10]    ; ins[+0x10] = dest-start
   00477599  e9e0fdffff        jmp   0x47737e                        ; ★ → add ecx,eax; mov [ebx+4],ecx
   ```
   **只改 `T+0`（时间）与 `T+4`（ip），从不触碰 sub 表，也不触碰任何「当前 sub」的旁路字段。**
3. **sub 结构未动**：`ECLHelper::SetBaseAddr` 定位的是 `res+0xC` 起的**运行期 ECL 文件缓冲**（§3.4）；`ECLJump` 只在硬编码偏移处覆写指令体，不触碰 SCPT 头、名字区、`res+0x8C` 表（表由 `FUN_004795B0` 一次解析生成）。
4. **瞬态注意**：`goto` 之后、目标 `call` 尚未执行的那一瞬，`T+4` 指向「跳转后位置」（仍是**旧 sub 内**的地址）；`call` 执行后即变到目标段落入口。采样落在帧间稳定态即可规避。

---

## 10. Q9 —— 段落判定口径：**路过式 checkpoint（latch）**

### 10.1 结论

✅ **走「路过式 checkpoint（latch）」**：读当前 sub（本作经 `NameOfAddress(T+4)` 得到）→ **∈ 白名单则置当前段落；不在 → no-op**（保持上一个值，不回退 `main`）；本局首次命中前显示 `main`。CSV 即白名单表。

> ⚠️ **本处订正**：早期草案写「(A) 调用栈祖先法」，与段内脚本结构不符（原因见 10.2）。

### 10.2 依据与订正原因

- **段内多个段落 sub 是兄弟 `call`，不互为祖先**：`THStageWarp` 各段落到 `ECLJump` 目标用的是同一套硬编码偏移（如 st1 的 `0x8a70 → 0x8c30/0x8c94`、st7 的 `0x4b98 → 0x4ef0`），与 th15 的「`dest` 精确命中一条 `call`」同型；一个段落可能跨多个兄弟子脚本，前一个 `ret` 后才 `call` 下一个。
  ⇒ 「段落入口 sub **整段**驻留栈上、可沿栈上溯」的 (A) 前提**对整个段落不成立**（只对单个 sub 成立）；故**每段只登记入口那一个 sub**，段内其余 sub 不在表内 → no-op。
- **调用栈本身仍是事实**（§4，`call`/`ret`/帧骨架齐全），只是**不用于**本判定。
- **分支关天然兼容**：读的是运行期「当前指令 → sub」，分支走哪条就读到哪条。
- ⚠️ 本作尚无 th15 `main → MainFront → MainSubNN` 的实测命名证据；入口表可离线枚举（§10.4）。

### 10.3 判定流程

```cpp
H = mEclHost;                                    // 0x42C76B / 0x42C90A 捕获
if (!H) return "main";
T = *(u32*)(H + 0x11D0);
if (!T) return "main";
res = *(u32*)(H + 0x11CC);                       // ≡ *(u32*)(*(u32*)0x4C2188 + 0xAC)
count = *(i32*)(res + 8);
table = *(u32*)(res + 0x8C);
uint32_t ip = *(u32*)(T + 4);                    // 当前指令绝对地址（Q1）
const char* cur = NameOfAddress(table, count, ip);
const char* section = Whitelist(game, stage, cur); // 命中 → 置当前段落
if (section) current = section;                    // 未命中 → no-op（不回退 main）
```

> checkpoint 粒度 = 每行 1 个入口 sub（与 th15 的 `MainSubNN` 同级）；**含 async 落点**；容器不入表。

### 10.4 ✅ 段落入口白名单 —— **已枚举完毕（2026-10-17）**

- th13 的段落入口名单**已枚举完毕（2026-10-17）**：`docs/logger/th13_checkpoints.csv` 第 1 列 **116/116**（章节 **44/44**、boss **72/72**），与 `THStageWarp` / `THPatch` 偏移一一对应的**入口表小结见 `docs/th13/th13_checkpoint_entry_report.md`**（入口表 / 命名规律 / 口径特例 / 实现影响 / 待真机清单）。
- **本节原提的「用硬编码偏移作初始实现」的替代方案已被取代**：白名单以 **sub 名表**为准（运行期按 `NameOfAddress(T+4)`（§2.3）得当前 sub 名，再查白名单表），**全作仅 `ST5 后半 #4` 一处**用 **`MainSub08+0x390`** 的 **ip 偏移**消歧（偏移基准 = 相对该 sub 首条指令的字节偏移，`MainSub08` 首指令 `0x5140`、落点 `0x54d0`；见入口表小结 §2.1）。`THStageWarp` / `THPatch` 的硬编码偏移仍作**离线资产**保留（离线推导与自检用），**不再是运行期判据**。
- 「段落入口 sub 在整段期间驻留栈上」**待真机确认**。

---

## 11. 对总括 §5 与 `th13_ecl_re_requests` 的回填建议

> 本报告**未修改**任何既有文件（避免与并行任务写冲突）。建议由本迭代的收口任务按下列内容回填。

### 11.1 总括 `docs/logger/logger_re_disasm_overview.md` §5.1 的 th13 行

| 列 | 建议填入 |
| --- | --- |
| 已有 ECL 解释器命中点（寄存器） | `0x4772D2`（6B，`EBX` = 执行对象 T）；函数 `FUN_004772A0` |
| 命中点已知字段 | `*(u32*)(T+4)` = **当前指令地址**（**不是索引**）；`*(f32*)(T+0)` = 当前 ECL 时间 |
| ECL 数据区链 | `GetMemAddr(0x4C2188, 0xAC, 0xC)` = `res + 0xC`（`res = *(*(0x4C2188)+0xAC)`）——**原值正确，可直接用** |
| sub 名/索引线索 | ✅ `res+0x8C` 表（项 8B，`+0`=名字 `char*`，`+4`=起始，首指令 `+4+0x10`）；`count = res+8` |

并新增两行注释：
- **`[执行对象+4]` 语义矛盾**（总括 §5.1 注记 / §9 风险 4）应改为：**th13 = 指令指针（与 th14 同类）；th15/16/17/18 = 索引**。两者不可互搬。
- th13 的**宿主→线程→ip** 链与 th15 不同（`H+0x11D0` / `T+4`），**不得套用 C 代偏移**。

### 11.2 总括 §5.2 接线锚点表 th13 行

保持现值即可（`th13_life_dec 0x444A75`(10B) / `th13_bomb_dec 0x40A404`(5B) / `th13_game_start 0x42D3D7`(7B) 均未复核，本轮未触及）。**新增建议**：把「主线宿主捕获」补进 §5.2 或新增一列 —— `th13` 捕获点 `0x42C76B` / `0x42C90A`（`EAX` = 宿主）。

### 11.3 `docs/th13/th13_ecl_re_requests.md`

| 章节 | 建议 |
| --- | --- |
| §1 表格「ECL 解释器锁时钩子（可用切入）」 | **改为「旁证」**：这三处位于扩展 opcode 处理器 `FUN_0041B260`（opcode 域 `0x12C..0x3EB`），**不是**读「当前 sub」的入口；但它们读的 `[T+4]` 与主解释器同源，可作交叉印证。 |
| §1 表格新增行 | 「ECL 主解释器命中点 = `0x4772D2`(6B) `EBX`」；「主线宿主捕获 = `0x42C76B`/`0x42C90A` `EAX`」 |
| §2 Q1 | 答：**指令指针**（非索引），`ip = *(u32*)(T+4)`；写入点见本报告 §2.2。 |
| §3 Q2 | 答：`res = *(*(0x4C2188)+0xAC)`；`table = res+0x8C`；项 8B；`+0`=名字、`+4`=起始、首指令 `+4+0x10`；加载器 `0x4795B0`，查表 `0x4797D0`，访问器 `0x414150`/`0x414160`/`0x414170`。 |
| §4 Q3 | 答：**有**显式栈；基址 `T+8`，SP `T+0x1008`，帧长 `0x0C`，帧 `+0x08` = 调用者 ip。 |
| §5 Q4 | 答：`FUN_004183F0` 返回宿主；站点 `0x42C76B`/`0x42C90A`（`"main"`@`0x4AADE0`）；静态兜底 `*(u32*)(*(u32*)(mgr+0xB0))`。**新增（§5.4）**：`mgr+0xB0` 是**全局全宿主链**（含 boss）；boss / 中boss 宿主由 ECL `enmCreateA`(opcode `0x12D`) 经同一工厂另起（`0x41B2EC` → `FUN_0041FCC0` → `call 0x4183F0` @ `0x41FE62`；备选捕获点 `0x41FE67`/`0x1FE67`）；boss 宿主的 `H+0x11CC` = 同一个全局 `res`。 |
| §6 Q5 | 答：`0x4772D2`（6B EHOOK），`EBX` = 执行对象；函数 `FUN_004772A0`；跳表 `0x478C38`/映射 `0x478D44`。 |
| §7 Q6 | 答：**运行期保留名字**；但因是地址语义，需 `table[i].+4+0x10` 建区间表反查。 |
| §8 Q7 | 答：`CHARA=0x4BE7B8`；`SHOT≈0x4BE7BC`（推测，待实测）；难度 `0x4BE7C4`（推测）。 |
| §9 Q8/Q9 | 答：Q8 不影响；**Q9 = 路过式 checkpoint（latch）**——经过白名单函数即置值、不在表内 no-op。 |
| §10 真机验证清单 | 追加本报告 §13 的条目。 |

---

## 12. 推荐实现摘要（供接线）

```cpp
// ── 初始化：捕获主线宿主 ─────────────────────────────────────────────
EHOOK_DY(th13_ecl_host, 0x42C76B, 5, { mEclHost = pCtx->Eax; self->Disable(); });
EHOOK_DY(th13_ecl_host2, 0x42C90A, 5, { mEclHost = pCtx->Eax; self->Disable(); });
// 清理：th13_game_start(0x42D3D7) 里 mEclHost = nullptr;（沿用既有 EHOOK）

// ── 每帧（IGI 渲染时，帧间稳定态）读取「当前段落」────────────────────
if (!mEclHost) { section = "main"; }
else {
    auto mgr   = *(uint32_t*)0x4C2188;              // 兜底/完整性校验用
    auto res   = *(uint32_t*)(mEclHost + 0x11CC);   // ≡ *(uint32_t*)(mgr + 0xAC)
    auto count = *(int32_t*)(res + 8);
    auto table = *(uint32_t*)(res + 0x8C);
    auto T     = *(uint32_t*)(mEclHost + 0x11D0);   // ≡ *(uint32_t*)(mEclHost + 4)
    if (!res || !table || count <= 0 || !T) { section = "main"; }
    else {
        uint32_t ip = *(uint32_t*)(T + 4);          // 当前指令绝对地址；0 = 线程已结束
        // ip → sub（§2.3 区间表）→ 白名单；命中则置当前段落，未命中 no-op（不回退 main）
    }
}
```

**EHOOK 覆盖长度**：`0x42C76B` / `0x42C90A` 处下一条指令 = `call 0x425170`（5B，`E8 xx xx xx xx`）⇒ **5B EHOOK** 合适。

---

## 13. 真机验证清单

> 本机无法运行 th13，以下**全部未做**；状态一律为「静态定案（🟡 已定案、未真机三态）」。

- [ ] **宿主捕获**：在 `0x42C76B`/`0x42C90A` 抓 `EAX` 非空；进关后 `H+0x11D0`、`H+0x11CC` 均可解引用且 `H+0x11CC == *(u32*)(mgr+0xAC)`。
- [ ] **Q1 地址语义**：道中/boss/不同段落各采样一次，`NameOfAddress(T+4)` 与画面进度一致；`T+4 == 0` 只在线程结束（过关/退关）时出现。
- [ ] **地址→sub 反查**：同一段落内 `T+4` 应始终落在**同一个** `[start_i, start_{i+1})` 区间内；跨段落时切换。
- [ ] **路过置值**：经过各段入口 sub 时当前段落被正确置值；段内其余 sub 不改变当前段落（no-op）。
- [ ] **Q7 待实测**：`0x4BE7B8`（机体）、`0x4BE7BC`（子机，**推测**）、`0x4BE7C4`（难度，**推测**）取值与所选机体/子机/难度一致。
- [ ] **练习跳章**：跳章后 `T+4` 名字与白名单置值均正确（Q8 复核）；`ins[0xA]=0xFF` 的难度掩码在 E/N/H/L 四难度下均能执行（否则 `goto` 会被 `test byte [T+0x101C], cl` 跳过 —— **这是最需要真机确认的一条**）。
- [ ] **异步关**：含 `spawn` 的段落，主线宿主 T 的读数是否稳定（不被 `H+0x11D4` 链上的异步线程污染）。
- [ ] **boss 宿主可达性（§5.4 的三条静态结论）**：打 ST1 关底 boss 时，沿 `mgr+0xB0` 链能枚举到一个**新宿主**（`mgr+0xB8` 计数增加），其 `H+0x11CC == *(u32*)(mgr+0xAC)`（与主线宿主**同一** `res`），且其 `T = *(u32*)(H+0x11D0)` 的 `T+4` 经地址反查得 `Boss*`/`BossCard*`（中boss 同理得 `MBoss*`）。**若此处不成立**，则 §5.4 的静态结论需重审。
- [ ] **采样时机**：置于帧间稳定态（IGI 渲染），避免局部/表达式栈占用 `T+0x100C` 时误读栈。
- [ ] **分支关**：th13 若有路线分支关卡，各分支各走一遍，读取与白名单映射均正确。
- [ ] **变体**：Steam / 汉化版是否共用同一 RVA（本报告地址来自原版 `ts=1313589413`）。

---

## 14. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（本报告）。静态定案 Q1–Q9：**Q1** `[T+4]` = **当前指令绝对地址（不是 sub 索引）**，写入点 `0x41779A`/`0x4191F9`/`0x41AC2F`/`0x477380`/`0x477324`/`0x47758E`/`0x478BDB`；**Q2** `mgr=*(u32*)0x4C2188` → `res=*(mgr+0xAC)` → `count=res+8` → `table=res+0x8C`（项 8B，`+0`=名字 `char*`，`+4`=数据基址，首指令 `+4+0x10`），访问器 `0x414150`/`0x414160`/`0x414170`，加载器 `FUN_004795B0`（SCPT `0x54504353`），查表 `FUN_004797D0`；**Q3** 显式调用栈（基址 `T+8`、SP `T+0x1008`、帧长 `0x0C`、帧 `+0x08`=调用者 ip，压帧 `FUN_00477030`，弹帧 `ret`@`0x4772FF`）；**Q4** 宿主工厂 `FUN_004183F0`（`mem 0x52E4`，返回 `EAX`=宿主），站点 `0x42C76B`/`0x42C90A`（`"main"`@`0x4AADE0`），宿主线程链 `H+0x11D0`↔`H+4`（`FUN_00417740`/`FUN_00478DB0`），静态兜底 `*(u32*)(*(u32*)(mgr+0xB0))`；**Q5** 命中点 `0x4772D2`（6B，`EBX`=执行对象 T），函数 `FUN_004772A0`（`0x4772A0`–`0x478BEA`），跳表 `0x478C38`/映射 `0x478D44`，>0x5D 委派 `0x478B9D`；**Q6** 运行期保留名字（需地址区间反查）；**Q7** `CHARA=0x4BE7B8`，`SHOT≈0x4BE7BC`（推测），难度 `0x4BE7C4`（推测）；**Q8** `ECLJump` 只写 `goto`(`0x0C`)，只改 `T+0`/`T+4`；**Q9** 定案 **(A) 调用栈祖先法**〔**后于 2026-10-15 订正为「路过式 checkpoint（latch）」**〕。**更正 requests §1**：`0x41D399`/`0x41E26C`/`0x4250EA` 位于扩展 opcode 处理器 `FUN_0041B260`（经蹦床 `0x41B250`，`opcode-0x12C` 域），**不是**主解释器，仅作交叉印证。证据：`capstone` 只读反汇编（自写 `D:\workshop\reasm\th13\dl.py`）+ 全程序字节/交叉引用扫描。**未真机三态；段落入口白名单待枚举。** |
| 2026-10-15 | 素材来源登记：`../exes/ecl/th13/`（`.ecl` 21 份）+ `../exes/ecl_dump/th13/`（`.decl` 21 份）已在机；据此把「本机无 th13 `.ecl` / 离线枚举无法完成」订正为「素材已在机、段落入口白名单待枚举」。清单见 `docs/logger/decompile_materials.md`。 |
| 2026-10-15 | **订正 Q9 口径**：由「(A) 调用栈祖先法」改为「**路过式 checkpoint（latch）**」——经过白名单函数即置值、不在表内 no-op（不回退 `main`）。原因：段内 sub 多为兄弟 `call`，不互为祖先，「整段驻留栈上」不成立 → 每 checkpoint 只需登记 1 个入口 sub（含 async）。§0 Q9 行、§10、§11、§12、§13 同步改写。 |
| 2026-10-17 | **§10.4 收口**：由「⚠️ 段落入口白名单**尚未枚举**」改为「✅ **已枚举完毕（2026-10-17）**」——`docs/logger/th13_checkpoints.csv` 第 1 列 **116/116**（章节 44/44、boss 72/72），并指向新建的入口表小结 `docs/th13/th13_checkpoint_entry_report.md`；同时注明本节原提的「用硬编码偏移作初始实现」的替代方案**已被 sub 名表取代**（运行期按 `NameOfAddress(T+4)` 查 sub 名白名单），**全作仅 `ST5 后半 #4` 一处**用 `MainSub08+0x390` 的 ip 偏移消歧。**本节其余文字未动**（§0 Q9 行的「尚未枚举」字样与 §11/§13 未在本轮同步）。 |
| 2026-10-18 | **新增 §5.4「boss 宿主可达性 —— Q-A / Q-B / Q-C」**（静态定案，全部 🟡 未真机）。**Q-A** ✅：宿主链 `mgr+0xB0` 是**全局全宿主链**——全程序唯一宿主分配点 = `FUN_004183F0`（`push 0x52E4` 仅 `0x4183FC`），且该工厂**无条件**入链（`0x418632`–`0x418652` 空表头/尾、`0x41865D`–`0x41869B` 追加）；引擎自身遍历这条链的证据 = `FUN_00414180`@`0x4141EE`、`FUN_0041AC70`@`0x41AC7C`、`FUN_00417E90`@`0x417E96` ⇒ **含 boss，无需新捕获点**。**Q-B** ✅：boss / 中boss 宿主由 **ECL `enmCreateA`（扩展 opcode `0x12D`；`0x12C`/`0x130`/`0x131`/`0x141` 同分支，经字节映射 `0x41F9D8` + 跳表 `0x41F760`）** 另起：`0x41B2EC: push edi; call 0x41FCC0` → `FUN_0041FCC0` → `call 0x4183F0` @ `0x41FE62`，名字 = 该指令的 `ins+0x14`；**备选捕获点 `0x41FE67` / RVA `0x1FE67`（5B，`EAX` = 新宿主）**，附两条告警（每次 `enmCreate*` 都命中须按名过滤；宿主数超限早退分支 `0x41FCDA: jge 0x41FE67` 跳到同址且 `EAX = mgr`）。工厂其余 4 个调用点 `0x42C766`/`0x42C905`（`"main"`）、`0x422F1A`（字面量 `"BossCard2_EtAt"` @ `0x4AABD8`）、`0x414925`（sub 表按索引取名）、`0x40FF0F`（`[edi+0x28]`）均非 boss 宿主。**Q-C** ✅：宿主构造器 `FUN_00418F40` 在 `0x4191E4`/`0x4191EB` **无条件**写 `H+0x11CC = *(u32*)(mgr+0xAC)`（**所有宿主共用一个全局 res**，与 th14 同法、非 th16 的「boss 段另加载」），并在 `0x4191F9` 设 `T+4 = FindSubByName(res, name)+0x10`；`res+0x8C` 表由加载器 `FUN_004795B0` **跨文件累积**（`0x479606` 累加 `res+8`；`0x479680` memcpy 旧表；`0x4796E0` 二分插入；`0x4797A4` 槽号自增）；关卡头 `ecli{}` 含 `stNNbs`（ST2–5/7 还含 `stNNmbs`）⇒ **boss sub 全在表内**；**交叉验证 = CSV 全部 116 行（含 72 boss 行）与该关加载集逐行比对 116/116 命中**；旁证 = `call` opcode 用宿主 res 解析名字（`0x477245: mov eax,[edi+0x11cc]`）。同步更新 §0 速览（+3 行）、§0 关键地址块、§11.3 的 `§5 Q4` 行、§13 真机清单（+1 条）。**未跑游戏、未跑编译、未改任何字节。** |

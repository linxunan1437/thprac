# TH07 ECL 段落监控 — 逆向结论（Q1–Q9）

> 上位文档：`docs/logger/logger_re_disasm_overview.md`（总括）。需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 本作需求：`docs/th07/th07_ecl_re_requests.md`（Q1–Q9 问法、已知锚点、素材缺口、交付格式）。
> 结构模板与方法论参照：`docs/th12/th12_ecl_re_report.md`（**只借结构，不借技术结论** —— th12 是 B 代，本作是 A 代，机制不同）。
> 二进制：`../exes/th07.exe`，650752 B，PE `TimeDateStamp = 1066942337`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**）；`sha256 = f7d03ba1bf6733a60beb3935e0efbbf1436c3d28b4f2155e7e410ba806ce0b9f`（本机实测；与 `docs/logger/decompile_materials.md` §2 的 `f7d03ba1…` 一致）。
> 引擎代：**A 代（第一代引擎）**；同代参照：th06 / th08（另篇）。
> ECL / decl 素材：**本报告撰写时无**；**2026-10-21 已补齐**（`../exes/ecl/th07/ecldata1..8.ecl` + `../exes/ecl_dump/th07/*.decl`，`thecl -d7`、无 eclmap ⇒ `SubN`）—— **本报告结论未据此复核**（仍是「运行期无 sub 名」的结论；补齐的是**离线具名**能力）。THTK `MAP/` 仍无 `ECL_MAP_07.txt`。
> 方法：`pefile` + `capstone 5.0.7` 只读反汇编（自写脚本，见 §12），用「已知 hook 地址反汇编 + 原始字节搜索 + `disp32` 交叉印证」定位（避开 `.text` 线性反汇编失步）。
> 状态：**静态定案（🟡 已定案、未真机三态）**。本机为 macOS，**无法编译、无法运行 th07**，未做任何真机验证。
> 地址口径：正文以 **VA** 为主，并标注 **RVA = VA − 0x400000**。
> 置信度图例：✅ 反汇编直接证实 ｜ 🟡 有强证据但缺一环（显式标注缺口）｜ ❌ 未能定案。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前位置** | ✅ **没有 sub 索引，只有「时间值 + 指令指针」两件，且按 timeline 各一份** | 道中有**一条贯穿全局的时间轴**：引擎维护 `count` 条 timeline，每条的状态对象 `TL_i = ENEMY_MANAGER + 0x9545F4 + i*0x10`，**`TL_i+8` = 当前时间（i32）**、**`TL_i+0xC` = 当前指令指针（u32，指向 ECL 缓冲内的绝对地址）**。章节/段落由**时间值**定位（thprac 的 `ECLTimeWarp` 就是写这个字段）。**不存在** th10–18 那种 `subIdx` 游标。 |
| **Q2 sub 表 / 时间轴** | ✅ **两者都在，是两张独立的表，都经同一个全局 `0x1347938` 取** | ① 时间轴表 = ECL 缓冲本身（`buf = *(u32*)0x1347938`；`count = *(u16*)(buf+2)`，`tlStart[i] = *(u32*)(buf+4+4i)`）。② **sub 表 = `*(u32*)(0x1347938+4)`**，项 4B = sub 起始指针，按 `sub_id` 索引（`call sub` 用）。**两张表都不含名字。** |
| **Q3 函数栈** | ✅ **存在可遍历的显式子调用栈（在敌方对象里）** | 栈基址 `E+0x8FC`，帧长 **`0x218`**，栈顶＝当前 sub 游标；深度字段 `E+0x2A7C`（压栈 `[+1]` @ `0x411FD5`、弹栈 `[-1]` @ `0x412012`），**上限 15（0xF）**。**只作事实记录，不参与判定**（沿用总括 §6 Q9 的 latch 口径）。 |
| **Q4 主线宿主捕获点** | ✅ **A 代是静态对象，无需 hook 捕获** | 宿主/管理器 = `ENEMY_MANAGER = 0x9A9B00`（静态，thprac 常量）；`*(u32*)0x9A9AF8 = 0x9A9B00` 写于 **`0x422FE5`**（RVA `0x22FE5`）；管理器初始化 `0x422F40` / 重置 `0x41EE70`、`0x41EA60`。 |
| **Q5 解释器命中点** | ✅ **给了新点（避开 thprac 已占用的 `0x4207A1`）** | 每帧稳定触发点＝**timeline 主循环 `0x420620`（RVA `0x20620`）每帧一次**，其中对每条 timeline 调 **解释器 `0x41F6F0`（RVA `0x1F6F0`）**，命中时 **`ECX` = `TL_i`**。thprac 的冻结 hook 占用了 `0x4207A1`（`call 0x41F6F0`，5B）⇒ **推荐不挂 hook，直接静态读**；若要挂，用 `0x41F6FC`(6B，取 `pCtx->Ebp-0x80`=TL_i) 或敌方 VM 入口 `0x410520`。 |
| **Q6 名字来源** | ❌ **运行期不保留 sub 名** | A 代 ECL 缓冲里**没有名字串**：sub 表项只存指针；`call sub` 的操作数是 **u16 sub_id**。⇒ 序号/偏移↔段落映射**只能离线自建**，而本作**没有 `.ecl`/eclmap** ⇒ 具名不可得，**「记录名」只能来自 thprac 自己的章节表**（`th_sections_str`）。 |
| **Q7 机体 / 子机 / 难度 / 关卡** | ✅ thprac 既有地址有效 | `character = *(u8*)0x62F645`、`shotType = *(u8*)0x62F646`、`difficulty = *(i32*)0x626280`、`STAGE_NUM = *(i32*)0x1347FC8`（均 thprac 既有，反汇编有引用点）。 |
| **Q8 练习跳转影响** | ✅ **立刻反映** | `ECLTimeWarp` 直接写 `TL_i+8`；下一帧解释器按新时间**跳过/对齐**到对应指令（`0x41F6FC` 起的时间比较）。故跳转后读「时间」**立刻**是新段落的值。 |
| **Q9 段落判定口径** | ✅ **沿用路过式 checkpoint（latch）**；A 代读取方案＝**读 `TL_0+8`（时间值）**，入口列记**时间值** | 建议：每帧读 `time = *(i32*)(mgr+0x9545F4+8)`（＝`*(i32*)0x12FE0FC`），与「本关时间值白名单」按 `最大入口 ≤ time` 取当前段落（等价于单调时间上的 latch）。**同一时间值对应多个 boss 分节时**，用副键＝**boss 敌机的当前 sub（`[0x12FE098]→E+0x6E4`）或 `TL_i+0xC`** 消歧（🟡 见 §10）。 |

> **关键地址（VA / RVA）**
> 管理器 `ENEMY_MANAGER` = `0x9A9B00`（静态）；
> `*(u32*)0x9A9AF8` = 管理器基址（warp 落点链的根，写于 `0x422FE5`/`0x22FE5`）；
> timeline 数组 = `mgr + 0x9545F4`（VA `0x12FE0F4`，16 槽 × `0x10`）；`TL_0+8`（时间）= VA `0x12FE0FC`；`TL_0+0xC`（ip）= VA `0x12FE100`；
> ECL 全局 `0x1347938`（RVA 不在映像内：运行期固定地址）：`buf = *(u32*)0x1347938`（时间轴表/ECL 缓冲）、`subTable = *(u32*)(0x1347938+4)`；
> timeline 主循环函数 `0x420620`/`0x20620`（每帧一次）；**timeline 解释器 `0x41F6F0`/`0x1F6F0`**（体 `0x41F6F0`–`0x41FD2E`）；opcode 跳表 `0x41FD2F`/`0x1FD2F`（opcode 0..0xC）；时间比较 `0x41F6FC`/`0x1F6FC`；收尾 `0x41FD08`/`0x1FD08`；
> **敌方/sub ECL 解释器 `0x410520`/`0x10520`**（体 `0x410520`–`0x417837`）；opcode 跳表 `0x417838`/`0x17838`（**opcode 1..0xA1**）；`call sub`(0x29) case `0x411F20`/`0x11F20`；`ret`(0x2A) case `0x411FE6`/`0x11FE6`；sub 装载 `0x40E530`/`0xE530`；
> 管理器初始化 `0x422F40`/`0x22F40`、重置 `0x41EE70`/`0x1EE70`、构造 `0x41EA60`/`0x1EA60`；bossExists `0x4232A0`/`0x232A0`；SUPERVISOR 时间推进 `0x43958D`/`0x3958D`；
> boss 槽表 = `mgr+0x954598`（VA `0x12FE098`，8 槽，存**敌机对象指针**）；
> thprac th07 冻结 hook `0x4207A1`/`0x207A1`（5B，**已占用**）。

---

## 1. 统一指针链（TH07 定案）

```text
// ── A 代「全局管理器 / 宿主」（静态；ENEMY_MANAGER）────────────────────────
mgr   = 0x9A9B00                                      // 静态常量（thprac: ENEMY_MANAGER）
// mgr[0 .. 0x37A164) 为管理器本体（构造 0x4232E0: rep stosd 0xde859 dword）
//   mgr + 0x0008        : 敌机模板（0x4F48 B，构造 0x41EF70）
//   mgr + 0x004F50      : 敌机实例池（最多 0x1E0=480 个 × 0x4F48 B）
//   mgr + 0x954598      : boss/敌机槽表 8×4B（存敌机对象指针，空=0）★
//   mgr + 0x9545E0      : 单个小型时间游标（重置 0x41EEC2）
//   mgr + 0x9545F4      : timeline 数组 16×0x10（重置 0x41EEF5，stride 0x10）★
//   mgr + 0x9546F4      : 单个额外时间游标（重置 0x41EF38）
//   mgr + 0x954714      : 每帧回调函数指针 = 0x420620（注册于 0x422F6E）
*(u32*)0x9A9AF8 = mgr                                 // 写于 0x422FE5（ECLTimeWarp 链的根）
E_TIME_WARP_TARGET = *(u32*)0x9A9AF8 + 0x9545FC = mgr + 0x9545FC = TL_0 + 8

// ── timeline 数组（道中/boss 的时间轴；每条各一份「时间 + ip」）──────────────
TL_i   = mgr + 0x9545F4 + i*0x10                       // i = 0 .. count-1（物理上限 16）
TL_i+0x0 = i32   保存时间（收尾时 [TL+0]=[TL+8]，0x41FD11）
TL_i+0x4 = f32   秒内小数累加器（SUPERVISOR 每帧 += delta，0x4395A2/0x4395B2）
TL_i+0x8 = i32   ★ 当前「时间」（= 章节定位量；ECLTimeWarp 写这里）
TL_i+0xC = u32   ★ 当前「指令指针」（指向 ECL 缓冲内的绝对地址；0 = 未启动/结束）

// ── timeline 表的来源（全局 0x1347938；thprac: ECLHelper::SetBaseAddr）──────
buf        = *(u32*)0x1347938                          // ECL 缓冲 / 时间轴表头（= thprac 打补丁的基址）
count      = *(u16*)(buf + 2)                          // 本关「活动 timeline 数」（主循环 0x420743）
tlStart[i] = *(u32*)(buf + 4 + i*4)                    // timeline i 的起始 ip（主循环 0x42076B）
// ⚠ thprac 的 SetBaseAddr / SetPos 用的就是 buf（写入偏移 0x2580/0x4934/0x4c20 … 都相对 buf）

// ── sub 表（`call sub <id>` 的目标表；与 timeline 表并列，同一全局）──────────
subTable     = *(u32*)(0x1347938 + 4)                  // 项 4B（0x40E530 的 `[ecx+4]`，ecx=0x1347938）
subStart[id] = *(u32*)(subTable + id*4)                // id = u16；被 0x40E530 装入敌机游标 E+0x6E4
// ⚠ 表项只有指针，**没有名字**（§7 Q6）

// ── 敌方 / sub ECL 的「当前执行对象」（boss 位置、函数栈）────────────────────
slot[k]     = *(u32*)(mgr + 0x954598 + k*4)            // k = 0..7；存敌机对象指针，空=0（bossExists 0x4232A0）
E           = slot[k]                                  // 敌机对象
E+0x6E4     = u32  当前 sub 指令指针（「当前 sub 游标」；装载于 0x40E530）
E+0x6E8     = i32  保存时间（初值 -999）  E+0x6EC = f32 小数累加器
E+0x6F0     = i32  当前时间（敌方 VM 比较用，0x4106DD）
E+0x2A7C    = i32  调用深度（0..0xF）    E+0x8FC = u8[]  调用栈（帧 0x218 B）
E+0x2E17    = u8   槽号（清槽时用，0x42036F）
```

**与 B/C 代模板的差异（重要，不可照搬）**

| 项 | B/C 代（th10–18） | **th07（A 代，本作）** |
| --- | --- | --- |
| 「当前位置」形态 | `thread+4` = sub 索引 / 指令绝对地址 | **timeline 的 `+8` 时间 + `+0xC` ip**；**没有 sub 索引游标** |
| 章节定位手段 | `ECLJump` 跳具名 sub | **`ECLTimeWarp` 写 timeline 时间**（时间轴定位） |
| sub 表（名字） | `res+0x8C`，项 8B，`+0` = `char*` 名字 | `*(u32*)(0x1347938+4)`，项 4B，**无名字** |
| 宿主 | 进关 spawn 的宿主对象（需 hook 捕获） | **静态**（`mgr=0x9A9B00`，单级读，**无需捕获**） |
| 解释器 | 单一主解释器 | **两套**：timeline VM `0x41F6F0`（opcode 0..0xC）+ 敌方/sub VM `0x410520`（opcode 1..0xA1） |

---

## 2. Q1 —— 「当前位置」＝**时间值 + 指令指针**，按 timeline 各一份（**没有 sub 索引**）

### 2.1 结论

th07 的「当前执行到哪」**不是 sub 索引**，而是：引擎维护 `count` 条**时间轴（timeline）**，每条 timeline 有自己的状态对象 `TL_i`，其中：

- **`TL_i + 0x8` = 当前时间（i32）** —— 章节/段落靠它定位（thprac `ECLTimeWarp` 写的就是它）；
- **`TL_i + 0xC` = 当前指令指针（u32，指向 ECL 缓冲内的绝对地址）** —— 等价于「时间轴上的位置」的另一种表述（随执行推进）。

`count` 与每条 timeline 的初始 ip 都从 **ECL 缓冲表头**（`buf = *(u32*)0x1347938`）取。

### 2.2 证据 A：主循环（`0x420620`，VA/RVA `0x420620`/`0x20620`）

```asm
0042072b  mov  dword ptr [ebp - 0x1c], 0          ; i = 0
0042073d  mov  edx, dword ptr [0x1347938]         ; edx = buf = *(u32*)0x1347938
00420743  movsx eax, word ptr [edx + 2]           ; ★ count = *(u16*)(buf+2)
00420747  cmp  dword ptr [ebp - 0x1c], eax
0042074a  jge  0x4207a8                            ; i >= count → 本帧 timeline 跑完
0042074c  mov  ecx, dword ptr [ebp - 0x1c]
0042074f  shl  ecx, 4                             ; i*0x10
00420752  mov  edx, dword ptr [ebp - 0x1e8]        ; edx = mgr (= 0x9A9B00)
00420758  cmp  dword ptr [edx + ecx + 0x954600], 0 ; ★ TL_i+0xC == 0 ?（未启动/已结束）
00420760  jne  0x42078e
00420762  mov  eax, dword ptr [ebp - 0x1c]
00420765  mov  ecx, dword ptr [0x1347938]
0042076b  mov  edx, dword ptr [ecx + eax*4 + 4]    ; ★ tlStart[i] = *(u32*)(buf+4+4i)
0042076f  mov  dword ptr [ebp - 0xe0], edx
00420775  mov  eax, dword ptr [ebp - 0x1c]
00420778  shl  eax, 4
0042077b  mov  ecx, dword ptr [ebp - 0x1e8]
00420781  mov  edx, dword ptr [ebp - 0xe0]
00420787  mov  dword ptr [ecx + eax + 0x954600], edx ; ★ TL_i+0xC = tlStart[i]
0042078e  mov  eax, dword ptr [ebp - 0x1c]
00420791  shl  eax, 4
00420794  mov  ecx, dword ptr [ebp - 0x1e8]
0042079a  lea  ecx, [ecx + eax + 0x9545f4]        ; ★ ecx = TL_i = mgr + i*0x10 + 0x9545F4
004207a1  call 0x41f6f0                            ; ★ 解释器（thprac 冻结 hook 就挂这条）
004207a6  jmp  0x420734                            ; i++
```
- `0x954600 − 0x9545F4 = 0xC` ⇒ `[...]` 读的正是 **`TL_i+0xC`**（ip）：`0` 表示该 timeline 尚未启动或已结束⇒用表头初值重新装入。
- `[ebp−0x1e8]` 由函数序（`0x420620: mov [ebp-0x1e8], ecx`）带入 = 管理器基址；由 §5 Q4 证实 = `0x9A9B00`。
- 该函数入口 `0x420620`（`push ebp; mov ebp,esp; sub esp,0x210`），它是登记在 `mgr+0x954714` 的**每帧回调**（注册于 `0x422F6E`；见 §5）。

### 2.3 证据 B：解释器 `0x41F6F0`（VA/RVA `0x41F6F0`/`0x1F6F0`）

```asm
0041f6f0  push ebp
0041f6f1  mov  ebp, esp
0041f6f3  sub  esp, 0x84
0041f6f9  mov  dword ptr [ebp - 0x80], ecx        ; ★ this = TL_i（ECX 入参）
0041f6fc  mov  eax, dword ptr [ebp - 0x80]
0041f6ff  mov  ecx, dword ptr [eax + 0xc]         ; ★ ip = TL+0xC
0041f702  movsx edx, word ptr [ecx]               ; ins.time = (i16)[ip]  (指令 +0)
0041f705  test edx, edx
0041f707  jl   0x41fd08                            ; time < 0 ⇒ timeline 收尾
0041f70d  mov  eax, dword ptr [ebp - 0x80]
0041f710  mov  ecx, dword ptr [eax + 0xc]
0041f713  movsx edx, word ptr [ecx]
0041f716  mov  eax, dword ptr [ebp - 0x80]
0041f719  xor  ecx, ecx
0041f71b  cmp  dword ptr [eax + 8], edx           ; ★ 比较 TL+8（当前时间）与 ins.time
0041f71e  sete cl
0041f721  test ecx, ecx
0041f723  je   0x41fcd3                            ; 不等 ⇒ 跳过/等待
0041f729  mov  edx, dword ptr [ebp - 0x80]
0041f72c  mov  eax, dword ptr [edx + 0xc]
0041f72f  movsx ecx, word ptr [eax + 4]           ; ★ opcode = (i16)[ip+4]
0041f733  mov  dword ptr [ebp - 0x84], ecx
0041f739  cmp  dword ptr [ebp - 0x84], 0xc
0041f740  ja   0x41fcd1                            ; opcode > 0xC ⇒ 直接前进
0041f746  mov  edx, dword ptr [ebp - 0x84]
0041f74c  jmp  dword ptr [edx*4 + 0x41fd2f]        ; ★ opcode 跳表（0..0xC）
```
- **指令头布局**：`+0x00` = i16 时间；`+0x04` = u16 opcode；`+0x06` = u16 size；`+0x08` 起为操作数。
- **前进逻辑** `0x41FCED`：`[TL+0xC] += (i16)[ip+6]`（按 size 走到下一条）。
- **等待/跳过** `0x41FCD3`：`TL+8 < ins.time` ⇒ 收尾（本帧到此）；`TL+8 > ins.time` ⇒ 跳过该指令。
- ⇒ 语义：**`TL+8` 是驱动量**；它被外部（`ECLTimeWarp` / SUPERVISOR 累加）改动后，解释器会**快进/对齐**到时间匹配的指令。**这就是章节定位的全部机制。**

### 2.4 证据 C：`TL_i` 的字段被谁写

| 字段 | 写入点 | 语义 |
| --- | --- | --- |
| `TL_i+0x0` | `0x41EF1D`（重置写 `-999`）；`0x41FD11`（收尾 `[TL+0]=[TL+8]`） | 保存/哨兵时间 |
| `TL_i+0x4` | `0x41EF26`（重置 0）；`0x4395BB`（`*TL+4 += delta`，float） | 秒内小数累加器 |
| **`TL_i+0x8`** | `0x41EF13`（重置 0）；**`0x422FE5` 链 + thprac `ECLTimeWarp`（写 int）**；`0x4395D8`（`++` 整秒进位） | **当前时间（章节量）** |
| **`TL_i+0xC`** | `0x420787`（装入 `tlStart[i]`）；`0x41FCFD`（`+= size` 前进） | **当前指令指针** |

> `0x43958D`（SUPERVISOR 时间推进；入口 `0x43958D`，`[0x575950]+0x178` 为每帧 delta）实测：**参数 `[ebp+8] = &TL+8`（int，`inc`）、`[ebp+0xc] = &TL+4`（float，`+= delta`）** —— 即 `TL+8` 是**整数时间**、`TL+4` 是小数累加器（**修正**总括里「`TL+8` 可能就是浮点」的猜测；两者协同：小数到 1.0 时 `TL+8++`、小数 `−1.0`）。

### 2.5 取值范围 / 读取表达式

```cpp
// 管理器为静态对象，无需捕获；
int32_t count = *(uint16_t*)(*(uint32_t*)0x1347938 + 2);   // 活动 timeline 数（本关）
for (int i = 0; i < count; ++i) {                          // 物理上限 16
    uint32_t TL = 0x9A9B00 + 0x9545F4 + i*0x10;
    int32_t  time = *(int32_t*)(TL + 0x8);                 // ★ 当前时间（章节量）
    uint32_t ip   = *(uint32_t*)(TL + 0xC);                //    当前指令指针（0 = 未启动/已结束）
}
```
- 值为 **i32 时间**（实测样本量级：st1 段落入口 540 / 1341 / 3107；boss 5042/5043；st4 峰值 13166；st7 峰值 9259 …，见 §11）。
- **⚠ 可能是「每条 timeline 各一份」**：`ECLTimeWarp(count, t)` 会**把同一个时间写进前 `count` 条 timeline**（见 §9），故读 **`TL_0+8`** 即可；若需更稳，可读所有 `TL_i (ip != 0)` 并按白名单匹配。

---

## 3. Q2 —— sub 表：**存在**，与「时间轴表」是**两张独立的表**

### 3.1 两张表（同一全局 `0x1347938` 的两个字段）

| 表 | 链 | 项 | 用途 |
| --- | --- | --- | --- |
| **时间轴表** | `buf = *(u32*)0x1347938`；`count = *(u16*)(buf+2)`；`tlStart[i] = *(u32*)(buf+4+4i)` | 4B（指针） | 主循环据此初始化/运行 `count` 条 timeline |
| **sub 表** | `subTable = *(u32*)(0x1347938+4)`；`subStart[id] = *(u32*)(subTable+id*4)` | **4B（指针）** | `call sub <id>`（opcode 0x29）的目标表 |

**sub 表证据**（装载函数 `0x40E530`，VA/RVA `0x40E530`/`0xE530`）：
```asm
0040e530  push ebp
0040e531  mov  ebp, esp
0040e536  mov  dword ptr [ebp - 0xc], ecx         ; ecx = this = 0x1347938
0040e539  movsx eax, word ptr [ebp + 0xc]         ; arg2 = sub_id (u16)
0040e53d  mov  ecx, dword ptr [ebp - 0xc]
0040e540  mov  edx, dword ptr [ecx + 4]           ; ★ edx = *(u32*)(0x1347938+4) = subTable
0040e543  mov  ecx, dword ptr [ebp + 8]           ; arg1 = dest（敌机游标，如 E+0x6E4）
0040e546  mov  edx, dword ptr [edx + eax*4]       ; ★ subStart[id] = *(u32*)(subTable + id*4)
0040e549  mov  dword ptr [ecx], edx               ; *dest = subStart[id]
0040e54b  mov  eax, dword ptr [ebp + 8]
0040e54e  add  eax, 4                             ; 游标其余字段初始化 {+0=-999,+4=0,+8=0}
...
```

### 3.2 `call sub`（opcode 0x29）之后「当前 sub」怎么变

case `0x411F20`（敌方/sub ECL 解释器，VA/RVA `0x411F20`/`0x11F20`）：
```asm
00411f20  mov  eax, dword ptr [ebp - 8]           ; [ebp-8] = 当前指令 ip
00411f23  mov  ecx, dword ptr [eax + 0xc]         ; 操作数1 = (u32)[ip+0xC]  ← sub_id
00411f26  mov  dword ptr [ebp - 4], ecx
00411f29  mov  edx, dword ptr [ebp - 8]
00411f2c  movsx eax, word ptr [edx + 6]           ; size
00411f30  mov  ecx, dword ptr [ebp - 8]
00411f33  add  ecx, eax                           ; 下一条指令（返回地址）
00411f35  mov  edx, dword ptr [ebp + 8]           ; E = 敌机对象
00411f38  mov  dword ptr [edx + 0x6e4], ecx       ; ★ 先推进 E+0x6E4 = 返回地址
...
00411f54  mov  esi, dword ptr [ebp + 8]
00411f57  add  esi, 0x6e4                         ; 保存当前游标
00411f5d  mov  eax, dword ptr [ebp + 8]
00411f60  mov  ecx, dword ptr [eax + 0x2a7c]      ; depth
00411f66  imul ecx, ecx, 0x218                    ; 帧长 0x218
00411f6c  mov  edx, dword ptr [ebp + 8]
00411f6f  lea  edi, [edx + ecx + 0x8fc]           ; ★ 压入调用栈 E+0x8FC+depth*0x218
00411f76  mov  ecx, 0x86
00411f7b  rep  movsd                              ; 0x86 dword = 0x218 B
00411f7d  mov  ax, word ptr [ebp - 4]
00411f81  push eax                                ; arg2 = sub_id
00411f82  mov  ecx, dword ptr [ebp + 8]
00411f85  add  ecx, 0x6e4
00411f8b  push ecx                                ; arg1 = &E+0x6E4
00411f8c  mov  ecx, 0x1347938
00411f91  call 0x40e530                           ; ★ E+0x6E4 = subStart[sub_id]（跳到 sub）
00411f96  ... 复制 8 dword 到 E+0x744 ...
00411fc0  cmp  dword ptr [edx + 0x2a7c], 0xf      ; ★ 深度上限 15
00411fc3  ...
00411fd5  add  ecx, 1
00411fdb  mov  dword ptr [edx + 0x2a7c], ecx       ; ★ depth++
00411fe1  jmp  0x410531                            ; 继续跑新 sub
```
⇒ **`call sub id`：压栈保存返回游标 → `E+0x6E4 = subStart[id]`（跳到 sub 首指令）→ 深度 +1。** `ret`(opcode **0x2A**) case `0x411FE6`：`depth--`（`0x412012`）后从 `E+0x8FC + depth*0x218` 恢复 `E+0x6E4`。**「当前 sub」的判定依据是 `E+0x6E4` 落在哪个 `subStart[id]` 区间，表里没有名字。**

### 3.3 关系总结

- **时间轴（timeline）= 关卡级并发脚本**（`count` 条；本关 st1=2、st2=3 … 由表头决定），**章节定位走时间轴**；
- **sub = 敌方/被调脚本**（由 `call sub` 进入，编号 `id` 存在操作数里）；
- **两张表都不含名字** ⇒ 章节「入口」只能记**时间值**（对时间轴）或**指令偏移**（对 sub）。

---

## 4. Q3 —— 函数栈：**存在、可遍历**（在敌方对象里）

```asm
// 压栈（call sub，0x411F66/0x411F7B）：帧长 0x218，栈基 E+0x8FC，索引 depth=[E+0x2A7C]
00411f66  imul ecx, ecx, 0x218
00411f6f  lea  edi, [edx + ecx + 0x8fc]
00411f76  mov  ecx, 0x86           ; 0x86 dwords = 0x218 B
00411f7b  rep  movsd
// 弹栈（ret 0x2A，0x412050/0x412077）
00412012  sub  ecx, 1
00412018  mov  dword ptr [edx + 0x2a7c], ecx   ; depth--
00412059  imul eax, eax, 0x218
00412062  lea  esi, [ecx + eax + 0x8fc]
00412072  mov  ecx, 0x86
00412077  rep  movsd                            ; 恢复到 E+0x6E4
```
- 栈顶 = 当前 sub 游标（`E+0x6E4`）；**深度字段 `E+0x2A7C`**，**上限 15（0xF，`0x411FC0`/`0x410663`）**。
- 遍历：`sp = *(i32*)(E+0x2A7C)`；`frame(level)` 在 `E+0x8FC + level*0x218`；每帧 0x218 B，**帧内容 = 该层被 call 时的完整游标**（含 ip）。
- **⚠ 注意**：这是**敌方/sub ECL**的栈。**timeline 没有调用栈**（timeline 只有一条平铺的时间轴）。
- 按总括 §6 Q9 的 latch 口径，**本项只作逆向事实记录，不参与判定**。

---

## 5. Q4 —— 主线宿主捕获点：**A 代是静态对象，无需捕获**

### 5.1 管理器初始化（写 `[0x9A9AF8]`）

```asm
00422f40  push ebp
00422f41  mov  ebp, esp
00422f4c  mov  dword ptr [ebp - 4], 0x9a9b00      ; ★ 管理器基址常量
00422f53  mov  ecx, dword ptr [ebp - 4]
00422f56  call 0x41ea60                            ; 初始化（敌机池/timeline 游标…）
00422f5b  mov  eax, dword ptr [ebp - 4]
00422f5e  mov  ecx, dword ptr [ebp - 8]
00422f61  mov  dword ptr [eax], ecx                ; mgr[0] = arg
00422f63  mov  edx, dword ptr [ebp - 4]
00422f66  mov  eax, dword ptr [ebp - 0xc]
00422f69  mov  dword ptr [edx + 4], eax            ; mgr[4] = arg
00422f6c  mov  dword ptr [0x12fe214], 0x420620     ; ★ mgr+0x954714 = 每帧回调 = 0x420620
00422f8a  mov  dword ptr [0x12fe218], 0x422ce0
00422f94  mov  dword ptr [0x12fe21c], 0x422e70
...
00422fce  ...
00422fe2  mov  edx, dword ptr [ebp - 4]            ; edx = 0x9a9b00
00422fe5  mov  dword ptr [0x9a9af8], edx           ; ★ *(u32*)0x9A9AF8 = 0x9A9B00（warp 链根）
```

- ⇒ **宿主 = `ENEMY_MANAGER = 0x9A9B00`，静态常量，单级读**（700000 级地址，不与 RELOCS 冲突因为它是运行期固定分配区）。
- 重置/构造：`0x41EE70`（重置 timeline 与敌机池；`0x41EEF5` 起 16 槽 × 0x10）、`0x41EA60`（构造）、`0x4232E0`（`rep stosd 0xde859`）。
- **「捕获点」**（若实现方坚持要 hook）：`0x422FE5`（RVA `0x22FE5`，写 `[0x9A9AF8]`）或 `0x422F4C`（RVA `0x22F4C`，`[ebp-4]=0x9A9B00`）。**但本作完全不需要 hook —— 直接读静态地址即可。**

### 5.2 与 requests 的「疑似静态对象」一致

requests §5 猜测「A 代宿主疑似静态对象（`0x9a9b00` 等），请确认」⇒ **确认**：`0x9A9B00` 是管理器本体；`*(u32*)0x9A9AF8`（= 管理器）与 `*(u32*)0x1347938`（= ECL 缓冲/表头）都是**运行期固定地址**，可跨进程直接读。

---

## 6. Q5 —— 解释器命中点与寄存器语义

### 6.1 每帧驱动器与调用次数

- **管理器的每帧更新函数 `0x420620`**（VA/RVA `0x420620`/`0x20620`）：入口 `push ebp; mov ebp,esp; sub esp,0x210`，`ECX` = 管理器基址（存入 `[ebp-0x1e8]`）。它登记在 `mgr+0x954714`（`0x12FE214`），是**每帧回调**。
- 在其中对 `i = 0 .. count-1` 逐条 timeline，**`call 0x41F6F0`（ECX = `TL_i`）⇒ 解释器每帧被调用 `count` 次**（每条 timeline 一次）。
- **全程序 rel32 扫描：`0x41F6F0` 的调用点只有 `0x4207A1` 一处**（`call` 目标唯一）⇒ 可确定「timeline 解释器」每帧只由该循环驱动。
- 另有**敌方/sub ECL 解释器 `0x410520`**（VA/RVA `0x410520`/`0x10520`，体到 `0x417837`）：每个活跃敌机每帧一次（读 `[E+0x6F0]` 时间、按 opcode 表 `0x417838` 分发）。

### 6.2 稳定触发点（推荐）

| 用途 | 地址（VA/RVA） | 覆盖 | 命中时寄存器/栈槽 | 说明 |
| --- | --- | --- | --- | --- |
| **timeline 主循环** | `0x420620`/`0x20620` | 函数入口（需 6B 才含 `sub esp`） | `ECX` = `mgr`（0x9A9B00） | 每帧一次；可在此整体读所有 `TL_i` |
| **timeline 解释器** | `0x41F6F0`/`0x1F6F0` | 入口 `55 8B EC 81 EC 84 00 00 00` | `ECX` = `TL_i` | 每帧 `count` 次 |
| **解释器内（干净 6B）** | **`0x41F6FC`/`0x1F6FC`** | `8B 45 80` + `8B 48 0C`（6B，两条完整指令） | 命中时 `[ebp-0x80]` = `TL_i`（已于 `0x41F6F9` 存入）；覆盖的 6B 执行后 `EAX`=`TL_i`、`ECX`=`[TL_i+0xC]` | 逐指令命中；须用 `pCtx->Ebp-0x80` 取 `TL_i` |
| 敌方/sub VM | `0x410520`/`0x10520` | 入口 | `ECX` = 0x1347938；栈 `[ebp+8]` = 敌机对象 `E` | boss 位置/函数栈用 |

### 6.3 与既有 thprac hook 的冲突

- ⚠ **thprac 已占用 `0x4207A1`/`0x207A1`（5B）** —— 正是 `call 0x41F6F0` 这条指令（st4 Lily 冻结）。**不要在同一地址再加 hook。**
- 若要「每帧读当前位置」，**首选完全不挂 hook**（管理器静态，IGI 渲染时读 `0x12FE0FC` 即可，零冲突）。
- 若必须挂，选 **`0x41F6FC`（6B）**（同函数内、与 `0x4207A1` 不重叠）或 **`0x410520`**（敌方 VM 入口）。

---

## 7. Q6 —— 名字来源：**运行期不保留 sub 名**

### 7.1 结论

❌ **A 代 ECL 不保留 sub 名（ASCII 串）**：
- sub 表 `subTable` 项只有 **4 字节指针**（`subStart[id]`），**没有名字指针**（§3.1 的 `0x40E530` 全流程）。
- `call sub`、`spawn` 的操作数都是 **u16 sub_id**（如 `0x411F23: mov ecx,[ip+0xC]` = id），**不是名字**。
- 全程序**没有**任何 "main"/"MainSub" 之类的 ECL sub 名串（与 th12 的 `"main"`@`0x49FE14` 形成对比；本作 ECL 格式无名字区）。

### 7.2 对监控的影响

- 「读当前 sub 名」这条路**不通**（运行期无名）。**2026-10-21 起** `.ecl`/`.decl` 已在本机 ⇒ **离线只剩 `SubN` 级具名**（sub 索引 N），仍无 eclmap 的**指令级语义名**；对监控不构成「读名字」的可能（运行期没有名字指针）。
- ⇒ **A 代的「记录名」只能来自 thprac 自己的章节表**（`th_sections_str` / `m_i18nStr` 等），而**「段落入口」必须改用「时间值 / 指令偏移」**（见 §10 Q9），对齐用户 2026-10-16 的定案。
- 若将来要「偏移↔段落」的离线映射：可用的**唯一锚点**是 `thprac_th07.cpp` 里 `THStageWarp`/`THPatch` 的 `ECLTimeWarp` 入参（时间值）与 `ECLSetTime/ECLCallSub/ecl << pair{…}` 的**硬编码偏移**（相对 `buf = *(u32*)0x1347938`，与运行期 sub_start 同坐标系）。

---

## 8. Q7 —— 机体 / 子机 / 难度 / 关卡号

| 名称 | VA / RVA | thprac 出处 | xref 数 | 说明 | 置信度 |
| --- | --- | --- | --- | --- | --- |
| `character`（机体） | `0x62F645` | `thprac_th07.cpp:673` | 31 | `byte`；反汇编有直接引用（如 timeline VM case8 `0x41FC0A: movzx eax,byte [0x62f645]`，用于 `id + char*10` 查表） | ✅ 地址有效（语义待真机） |
| `shotType`（子机） | `0x62F646` | `thprac_th07.cpp:674` | 17 | `byte`，紧邻 character | ✅ 地址有效（语义待真机） |
| `difficulty` | `0x626280` | `thprac_th07.cpp:1863/676` | 74 | `i32`（0=E/1=N/2=H/3=L/4=Extra/5=Phantasm） | ✅ 地址有效 |
| `STAGE_NUM` | `0x1347FC8` | `thprac_th07.cpp:25`（`GetMemContent(STAGE_NUM) != 4`） | 0（**运行期固定地址，非映像内，无静态 xref**） | 与 `0x1348020/0x1348024`（`ECLNameFix` 用）同区 | 🟡 沿用 thprac（未真机） |
| `GAME_MANAGER` | `0x626270` | `thprac_th07.cpp:14` | 100 | 静态 | ✅ |
| `ENEMY_MANAGER` | `0x9A9B00` | `thprac_th07.cpp:20` | 18（+1 于 `0x422F4C` 直接常量） | 本作管理器/宿主 | ✅ |
| `game flags`（replay 位） | `0x62F648`（`& 8` = replay） | `thprac_th07.cpp:2249` | — | 三态已验证字段见 `docs/gametime/gametime_thxx_flags.md` §3 | 🟡 沿用 thprac/gametime |

> 说明：本迭代 IGI 只需显示「当前段落」，Q7 非必需；上表供后续日志迭代。

---

## 9. Q8 —— 练习跳转（`ECLTimeWarp` / `THPatch`）对读取的影响：**立刻反映**

1. **补丁写入**（`thprac_th07.cpp:891-898`）：
   ```cpp
   void ECLTimeWarp(int count, uint32_t time) {
       uint32_t* addr = (uint32_t*)GetMemAddr(0x9a9af8, 0x9545fc);  // = 0x9A9B00 + 0x9545FC
       for (auto i = 0; i < count; i++) { *addr = time; addr = (uint32_t*)((uint32_t)addr + 0x10); }
   }
   ```
   写的是 `TL_0+8, TL_1+8, …`（步长 `0x10`）⇒ **直接改「当前时间」**。
   （`GetMemAddr(0x9a9af8, 0x9545fc)` 展开 = `*(u32*)0x9a9af8 + 0x9545fc`；而 `*(u32*)0x9a9af8 = 0x9A9B00`（`0x422FE5`）⇒ 目标 = `0x12FE0FC` = `TL_0+8`。**复核通过。**）
2. **下一帧的效果**：主循环 `0x41F6FC` 用 `TL+8` 与 `ins.time` 比较 —— `TL+8 > ins.time` 则 `0x41FCED` 前进（`+= size`）、`==` 则执行、`<` 则收尾。故写入新时间后，解释器**立即把 ip 快进到时间匹配的指令**。
3. ⇒ **跳转后读 `TL_0+8` 立刻是新段落的时间值**；（`TL+0xC` 的 ip 也在同一帧内被对齐，但读「时间」是最直接的判据。）
4. **瞬态**：`goto`/`call sub` 等被补丁改写的**敌方/sub 脚本**位置（`E+0x6E4`）要等敌方 VM 跑到那一帧才更新；而在**帧间稳定态（IGI 渲染）**采样可规避。
5. **`THPatch` 的另一半**（`ECLSetTime`/`ECLCallSub`/`pair{off,…}`）改的是 **sub 脚本指令**（相对 `buf` 的硬编码偏移，如 `0x4934/0x4c20/0x55ec`），**不改 timeline 机制**、**不触碰 sub 表**。

---

## 10. Q9 —— 段落判定口径：**路过式 checkpoint（latch）**（A 代读取方案）

### 10.1 口径沿用（不重新论证）

沿用总括 §6 Q9 的定案：**经过白名单条目即置「当前段落」，不在表内 no-op（不回退 `main`）；本局首次命中前显示 `main`。**

### 10.2 ★ A 代可落地读取方案（本作新增）

**读哪个字段**：**时间值** —— `time = *(i32*)(0x9A9B00 + 0x9545F4 + 0x8) = *(i32*)0x12FE0FC`。

**入口列口径**：**时间值**（＝ `ECLTimeWarp(<count>, <time>)` 的 `time` 入参，见 §11 表）。**不需要 sub 名**（本作也没有）。

**判定算法（推荐，单调时间上的 latch）**：

```cpp
// 白名单 = 本关所有段落的 (时间值 = entryTime, 记录名)，按 entryTime 升序（§11 表）
int32_t t = *(int32_t*)0x12FE0FC;                 // TL_0+8（当前时间）
const Section* cur = nullptr;
for (const auto& s : whitelistOfStage(stageNum))   // stageNum = *(i32*)0x1347FC8
    if (s.entryTime <= t) cur = &s;                // 取「最大入口 ≤ t」
// cur 即当前段落；未命中任何（t < 最小入口）⇒ 保持初值 "main"
```
> 因为 `t` 单调递增（SUPERVISOR 每帧推进），「最大入口 ≤ t」**等价于**「经过该入口时 latch」，且天然不回退。

**⚠️ 一级键不够用（2026-10-22 补）**：本作 boss 段的时间值大量重复（CSV **140 行只有 53 个不同值**，17 组重复覆盖 104 行；如 `ST1 BOSS1..4` 全为 5043）。原因是 `THPatch` 的 `ECLTimeWarp` 只把时间轴打到 boss 容器入口，相位由**就地改写 boss 脚本字节**（`ECLSetTime`/`ECLCallSub`/`pair`）选择，不动时间轴。因此监控必须**两级判定**：

- **一级**：本节的时间值 latch → 定位「哪个 boss 容器」；
- **二级**：boss 对象（`E=*(u32*)(0x9A9B00+0x954598+k*4)`）的当前指令指针 `*(u32*)(E+0x6E4)`，相对 `buf=*(u32*)0x1347938` 的偏移，按 §11.2 各 section 的补丁偏移切区间 → 定位相位。

完整流程（读取链、区间表、伪代码、边界、E2E 验证清单）见 **`docs/th07/th07_checkpoint_entry_report.md` §7**。

**当同一时间值对应多个 boss 分节时的消歧（🟡 未真机）**：如 ST1 `BOSS2/3/4` 一律 `ECLTimeWarp(2, 0x13b3)`，仅靠时间无法区分。此时用**副键**：
- 读 **boss 敌机当前 sub**：`E = *(u32*)(0x12FE098)`（`mgr+0x954598`，空则无 boss）；`subIp = *(u32*)(E+0x6E4)`；用离线「sub 起始指针 ↔ 段落」表（`subStart[id] = *(u32*)(*(u32*)(0x1347938+4)+id*4)` 现场反查区间）映射到段落。**该映射需离线建立**（可用 §11 里 `ECLCallSub`/`ECLSetTime` 的硬编码偏移作锚点）。
- 或读 `TL_0+0xC`（timeline ip）—— 与时间同坐标系、单调，一般不带来额外区分度。

### 10.3 checkpoint「入口」列记什么

| 段落类型 | 入口列取值 | 来源 |
| --- | --- | --- |
| 道中（portion） | **时间值**（如 st1 前半 #1 = 540） | `THStageWarp` 的 `ECLTimeWarp(count, time)` |
| boss / 中boss | **时间值**（如 ST1 BOSS1 = 5042/5043） | `THPatch` 的 `ECLTimeWarp(count, time)` |
| boss 内同时间的多个分节 | 时间值 **+ 副键（sub 偏移/boss ip）** | `THPatch` 里额外的 `ECLSetTime/ECLCallSub/pair{off,…}` 偏移 |

> ⚠ **与 th10–18 的 CSV 语义分叉**：A 代「段落入口」列**不是 sub 名**，而是**时间值（必要时 + 偏移）**；回填产物（`docs/logger/th07_checkpoints.csv`）第 1 列须按其字形记录并在工作流文档显式标注（本报告不改这些文件）。

---

## 11. 附录 —— 候选段落入口值表（从 `thprac_th07.cpp` 抽出；**候选、未真机**）

> 来源：`THStageWarp`（`thprac_th07.cpp:975-1133`）与 `THPatch`（`:1134-1834`）。值 = `ECLTimeWarp(<count>, <time>)` 的 `time`。**「dlg」分支时间比非 dlg 小 1**（如 `0x13b2` vs `0x13b3`），实现时可只在非 dlg 值上建表或同时收录。十进制/十六进制并列。

### 11.1 道中（`THStageWarp`；`section = 10000 + stage*100 + portion`）

> **列名 / 列位订正（2026-10-22）**：原表列头把后半写成 `后半 #5..#8`，且把跨前后半的 portion 全塞进「前半」列（如 st1 的 3107 实为 `ST1 后半 #1`、st2 的 3366 实为 `ST2 后半 #1`、st5 的 4883 实为 `ST5 后半 #1`、st6 的 1180 实为 `ST6 #2`，st7/st8 的 3305/3191 实为**前半 #5**）。下表改为**按记录名**列出；`portion` ↔ 记录名由 `mChapterSetup` 决定（前 `前半` 个 → `前半 #k`，其余 → `后半 #k`；`后半 == 0` 的 ST6 → `STn #k`）。

| 关卡 | `mChapterSetup` | 记录名 ← portion（time） | count |
| --- | --- | --- | --- |
| st1 | {2,1} | `ST1 前半 #1`=540、`ST1 前半 #2`=1341、`ST1 后半 #1`=3107 | 2 |
| st2 | {1,1} | `ST2 前半 #1`=390、`ST2 后半 #1`=3366 | 3 |
| st3 | {2,1} | `ST3 前半 #1`=390、`ST3 前半 #2`=854、`ST3 后半 #1`=1805 | 2 |
| st4 | {4,4} | `ST4 前半 #1..4`=80/1948/3028/4288、`ST4 后半 #1..4`=7964/10136/11396/13166 | 2 |
| st5 | {3,1} | `ST5 前半 #1..3`=440/840/2550、`ST5 后半 #1`=4883 | 2 |
| st6 | {2,0} | `ST6 #1`=660、`ST6 #2`=1180（**该行已于 2026-10-22 从 CSV 删除**，见 §11.3） | 2 |
| st7 | {5,3} | `ST7 前半 #1..5`=485/1035/1955/2615/3305、`ST7 后半 #1..3`=4028/5808/7088 | 2 |
| st8 | {5,3} | `ST8 前半 #1..5`=485/965/1595/2485/3191、`ST8 后半 #1..3`=3928/5708/6988 | 2 |

> 注：st3 portion3 另写 `pair{0xbb3c,1559}`、st7 portion6 另写 `pair{0xee3e,0x68}`、st8 portion6 另写 `pair{0x1265a,0x68}`。**逐行权威表**（含 decl 交叉验证）见 `docs/th07/th07_checkpoint_entry_report.md` §2。

### 11.2 boss / 中boss / 终章（`THPatch`）

> **本表是「按 section 分组」的汇总，不是逐行表**（2026-10-22 订正）。以下分组在 CSV 里会展开成多行，逐行权威表见 `docs/th07/th07_checkpoint_entry_report.md` §2：
> `ST4_BOSS1/BOSS2/BOSS4/BOSS5` 各展开 **3 行**（`_LUNASA`/`_MERLIN`/`_LYRICA`，而 BOSS3/BOSS6/BOSS7 各 1 行）；`ST6_BOSS7/8` 与 `BOSS9/10` 各 2 行、`ST6_BOSS1..6` 各 1 行；`ST7_END_S1..S10`、`ST8_END_S1..S11` 逐行（另有 `ST7_END_NS1`、`ST8_END_NS1`）；`ST7_MID1/2/3`、`ST8_MID1/2/3` 各 1 行。合计 **103 行**。

| section | 时间值（非 dlg / dlg） | count | 附加补丁（时间外） |
| --- | --- | --- | --- |
| ST1_MID1 | 0xa60 = 2656 | 2 | —（仅 `ECLTimeWarp`，无 `ECLST3BG()`；原「ECLST3? 无」为臆测，2026-10-22 删除） |
| ST1_MID2 | 0xa60 = 2656 | 2 | `ECLSetTime(0x2580,0x3c,0)` |
| ST1_BOSS1 | 0x13b2=5042 / 0x13b3=5043 | 2 | `ECLNameFix` |
| ST1_BOSS2 | 0x13b3=5043 | 2 | `ECLSetTime(0x4c20,0x3c,0)` |
| ST1_BOSS3 | 0x13b3=5043 | 2 | `pair{0x4c2c,0x27} …` |
| ST1_BOSS4 | 0x13b3=5043 | 2 | `pair{…} + ECLSetTime(0x4f44,0,0)` |
| ST2_MID1 | 0xb0a = 2826 | 3 | — |
| ST2_MID2 | 0xb0a = 2826 | 3 | `ECLSetHealth(0x5444,…)` + pairs |
| ST2_BOSS1 | 0x1dde=7646 / 0x1ddf=7647 | 3 | — |
| ST2_BOSS2 | 0x1ddf=7647 | 3 | `ECLSetTime(0x6ba8,0x3c,0)` |
| ST2_BOSS3 | 0x1ddf=7647 | 3 | `pair{0x6bb4,0x35} …` |
| ST2_BOSS4 | 0x1ddf=7647 | 3 | `pair{…} + ECLSetTime(0x81f4,0,0)` |
| ST2_BOSS5 | 0x1ddf=7647 | 3 | `ECLForceSpell(0x81f4,…)` |
| ST3_MID1 | 0x335 = 821 | 2 | `ECLST3BG` |
| ST3_MID2 | 0x742 = 1858 | 2 | `ECLST3BG` |
| ST3_MID3 | 0x742 = 1858 | 2 | `ECLST3BG + ECLSetHealth(0x2e58,…)` |
| ST3_BOSS1 | 0xd41=3393 / 0xd42=3394 | 2 | `ECLST3BG` |
| ST3_BOSS2 | 0xd42 = 3394 | 2 | `ECLST3BG + ECLSetTime(0x43d0,0x3c,0)` |
| ST3_BOSS3 | 0xd42 = 3394 | 2 | `ECLST3BG + pair{0x43dc,36} …` |
| ST3_BOSS4 | 0xd42 = 3394 | 2 | `ECLST3BG + pairs + ECLSetTime(0x43bc,0x3c,0)` |
| ST3_BOSS5 | 0xd42 = 3394 | 2 | `ECLST3BG + pair{0x43dc,0x25} …` |
| ST3_BOSS6 | 0xd42 = 3394 | 2 | 同上 + `ECLSetTime(0x4fb0,0,0)` |
| ST3_BOSS7 | 0xd42 = 3394 | 2 | 同上 + `ECLForceSpell(0x4fb0,…)` |
| ST4_MID1 | 0x1bd2 = 7122 | 2 | —（注：thprac 冻结注释 `lilyStart=7122` 与此**同值**） |
| ST4_BOSS1_{LUNASA,MERLIN,LYRICA} | 0x39ea=14826 / 0x39eb=14827 | 2 | `ECLNameFix + thFakeShot` |
| ST4_BOSS2_{…} | 0x39eb = 14827 | 2 | `ECLNameFix + ECLSetTime(0x4934,0x3c,0)` |
| ST4_BOSS3 | 0x39eb = 14827 | 2 | `ECLNameFix + pair{0x4880,0} …` |
| ST4_BOSS4_{…} | 0x39eb = 14827 | 2 | `ECLCallSub(0x4934,0,0x2f) + lunasa/merlin/lyrica()` |
| ST4_BOSS5_{…} | 0x39eb = 14827 | 2 | 同上 + `ECLSetTime(0x51a0,0x20,0) + timesave()` |
| ST4_BOSS6 | 0x39eb = 14827 | 2 | `ECLCallSub(0x4934,0,0x31)` |
| ST4_BOSS7 | 0x39eb = 14827 | 2 | 同上 + `ECLForceSpell(0x5488,…)` |
| ST5_MID1 | 0x12d4 = 4820 | 2 | `pair{0xb74c,13}` |
| ST5_MID2 | 0x12d4 = 4820 | 2 | `pair{0xb748,0} + ECLSetHealth(0x38b4,0x3c,0x5db)` |
| ST5_BOSS1 | 0x17e1=6113 / 0x17e2=6114 | 2 | — |
| ST5_BOSS2 | 0x17e2 = 6114 | 2 | `ECLSetTime(0x58f0,0x3c,0)` |
| ST5_BOSS3 | 0x17e2 = 6114 | 2 | `ECLCallSub(0x58f0,0x3c,0x34) + pairs` |
| ST5_BOSS4 | 0x17e2 = 6114 | 2 | 同上 + `ECLSetTime(0x68f8,0,0)` |
| ST5_BOSS5 | 0x17e2 = 6114 | 2 | `ECLCallSub(0x58f0,0x3c,0x37) + pairs` |
| ST5_BOSS6 | 0x17e2 = 6114 | 2 | `ECLCallSub(0x58f0,0,0x37) + ECLSetHealth(0x7110,0,0x7cf)` |
| ST6_CHAOS | 0x49c = 1180 | 2 | — |
| ST6_MID1 | 0x77a = 1914 | 2 | `pair{0xe630,0}` |
| ST6_MID2 | 0x77a = 1914 | 2 | `pair{0xe630,0} + ECLSetHealth(0x17cc,0x3c,0x5db)` |
| ST6_BOSS1 | 2518 / 0x9d7=2519 | 2 | `ECLNameFix + pair{0xe678,0x9d7}` |
| ST6_BOSS2 | 0x9d7 = 2519 | 2 | `ECLSetTime(0x3324,0x3c,0)` |
| ST6_BOSS3 | 0x9d7 = 2519 | 2 | `yuyuko()`（含 `ECLTimeWarp(2,0x9d7)`） |
| ST6_BOSS4 | 0x9d7 = 2519 | 2 | `yuyuko() + ECLSetTime(0x445c,0,0)` |
| ST6_BOSS5 | 0x9d7 = 2519 | 2 | `yuyuko() + ECLCallSub(0x445c,0,0x24) + pairs` |
| ST6_BOSS6 | 0x9d7 = 2519 | 2 | 同上 + `ECLSetTime(0x48d4,0,0)` |
| ST6_BOSS7/8 | 0x9d7 = 2519 | 2 | `pairs{0x5574,0x5584,0xe678,0x3330} (+ECLSetTime(0x5694,0,0))` |
| ST6_BOSS9/10 | 0x9d7 = 2519 | 2 | 大串 pairs（BOSS10 另 `th07_rb.Enable()`） |
| ST7_MID1/2/3 | 0xfe1 = 4065 | 2 | `pair{0xee58,0}`（MID2/3 追加 `ECLSetTime`/pairs） |
| ST7_END_NS1 | 8798 / 0x225f=8799 | 2 | — |
| ST7_END_S1..S10 | 0x225f = 8799 | 2 | 各自 `ECLSetTime`/`ECLCallSub`；实际 sub id 为 `0x46,0x47,0x49,0x4b,0x4d,0x4f,0x51,0x53,0x54`（**跳号**，且 `S1` 走 `ECLSetTime` 而非 `CallSub`）/pairs（S10 另有 phase 1/2 分支） |
| ST8_MID1/2/3 | 0xf7d = 3965 | 2 | `pair{0x12674,0}`（MID3 另 `ECLCallSub(0x4350,0,0x3f)`+pairs） |
| ST8_END_NS1 | 9258 / 0x242b=9259 | 2 | — |
| ST8_END_S1..S11 | 0x242b = 9259 | 2 | 各自 `ECLSetTime`/`ECLCallSub`；实际 sub id 为 `0x48,0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x56,0x57`（**跳号**）/pairs |

> **候选、未真机**：上表数值直接取自 thprac 源码的 `ECLTimeWarp` 入参，**未经实机核对**（🟡）。子机/难度分支不影响时间值。
> **⚠ st4 分支**：`TH07_ST4_BOSS1/2/4/5` 各拆 LUNASA/MERLIN/LYRICA 三节，但**时间值相同**（0x39eb）⇒ 仅靠时间无法区分三姐妹，须用副键（§10.2）或 thprac 的 `thFakeShot`。

---

### 11.3 `ST6 #2` 的删除（2026-10-22 裁决）

- `ST6 #2`（章节轴，`thprac_th07.cpp:1069`，`ECLTimeWarp(2, 1180)`）与 `ST6 CHAOS`（boss 轴，`:1435`，`ECLTimeWarp(2, 0x49c)`）**时间值同为 1180**，且在 `ecldata6.decl` 的 `Timeline0` 上落到**同一条指令**（`t=1180` 的 `ins_0(...)`），`ST6 CHAOS` 无任何额外补丁 ⇒ **时间与 ECL 状态都无法区分**。
- **裁决（用户，2026-10-22）**：**保留 `ST6 CHAOS`、删除 `ST6 #2`**。CSV 因此为 **140 行＝章节 37 + boss 103**（原 141/38）。
- 影响：§11.1 的 st6 仍列两个 portion（源码事实），但 CSV 只保留 `ST6 #1` 与 `ST6 CHAOS`。


## 12. 工具与复现

- 环境：macOS + `python3 3.9.6` + `pefile 2024.8.26` + `capstone 5.0.7`（只读静态反汇编；**本机无法编译/运行**）。
- helper（本机 `reasm` 脚手架）：`P(path)`；`p.off(va)`（`pefile.get_offset_from_rva(va-base)`，**勿用 `va-0x400000`**）；`p.dis(va,n)`；`p.data_of(va,n)`；`p.xrefs(va)`（`.text` 内 4 字节小端立即数搜索）。
- 复现步骤（本次实际路径）：
  1. 从已知 hook `0x4207A1` 反汇编其所在函数 ⇒ 函数入口 `0x420620`、timeline 循环、`TL_i = mgr+i*0x10+0x9545F4`。
  2. 反汇编被调解释器 `0x41F6F0` ⇒ `TL+8` 时间、`TL+0xC` ip、指令头 `{+0 time,+4 opcode,+6 size}`、opcode 跳表 `0x41FD2F`。
  3. 搜 `disp32 = 0x9545FC`（0 处）/`0x9545F4`（2 处）/`0x954600`（2 处）⇒ 印证字段偏移与访问点。
  4. 由 `0x9A9AF8` 的唯一 xref（`0x422FE5`）反汇编 ⇒ 管理器初始化，`*(u32*)0x9A9AF8 = 0x9A9B00`。
  5. 搜「绝对地址 `0x12FE098`（= mgr+0x954598）」⇒ boss 槽表用途；搜 `ff 24 xx disp32` 跳表 ⇒ 敌方解释器 `0x410520`、opcode 表 `0x417838`（opcode−1，上界 `0xA0`）。
  6. 定位 `call sub`(0x29)/`ret`(0x2A) case，读 `0x40E530` ⇒ sub 表 `*(u32*)(0x1347938+4)`、调用栈 `E+0x8FC/depth=0x2A7C/帧 0x218`。
- **PE 陷阱**（务必）：`.text` 的 RVA 非 0（VA `0x401000`，RVA `0x1000`）⇒ 取文件偏移必须用 `pefile.get_offset_from_rva(va-0x400000)`。
- 素材：`../exes/th07.exe`（本报告反汇编所用）；**`.ecl`/`.decl` 于 2026-10-21 补齐**（`../exes/ecl/th07/ecldata1..8.ecl`、`../exes/ecl_dump/th07/*.decl`，`thecl -d7`、无 eclmap）。

---

## 13. 真机验证清单（E5 待做；本轮**未做**）

> 本机为 macOS，**以下全部未验证**；状态一律「静态定案（🟡 已定案、未真机三态）」。

- [ ] **管理器静态性**：进关后 `0x9A9B00` 可解引用；`*(u32*)0x9A9AF8 == 0x9A9B00`。
- [ ] **timeline 数与字段**：`count = *(u16*)(*(u32*)0x1347938+2)` 与本关实际并发脚本数一致（st1 期望 2、st2 期望 3…）；`TL_0+0xC` 非 0 且指向 `buf` 内。
- [ ] **Q1 时间随进度推进**：道中/boss 各采样一次，`*(i32*)0x12FE0FC` 随进度单调递增，且与画面段落一致。
- [ ] **Q8 warp 立即生效**：练习跳章（`ECLTimeWarp`）后下一帧 `TL_0+8` == 目标时间（±1 因 dlg 分支）。
- [ ] **Q9 latch**：用 §11 时间值白名单，各段落能正确置值、段内 no-op、首次命中前为 `main`。
- [ ] **同时间多分节**：ST1 `BOSS2/3/4`、ST4 三姐妹等，验证 §10.2 副键（boss `E+0x6E4` 或 `TL_0+0xC`）能否区分；若不能，需补「离线 sub 偏移↔段落」表。
- [ ] **st4 分支**：Prismriver 三姐妹各走一遍，读值/白名单映射正确。
- [ ] **title 挂机 attract / replay**：读值是否异常（`count`/`TL` 是否被重置）。
- [ ] **敌方 VM 命中点**（若采用 `0x410520`）：命中时 `[ebp+8]` 为敌机对象、`E+0x6E4` 落在某 `subStart[id]` 区间。
- [ ] **变体**：Steam / 汉化版是否共用同一地址（本报告地址来自 `ts=1066942337`、sha256 `f7d03ba1…`）。

---

## 14. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-19 | 建档（本报告）。A 代 th07 的 ECL 段落监控逆向（阶段 E1，静态定案 🟡）。**Q1**：当前位置＝**时间值（`TL_i+8`）+ 指令指针（`TL_i+0xC`）**，`TL_i = ENEMY_MANAGER(0x9A9B00)+0x9545F4+i*0x10`，`count = *(u16*)(*(u32*)0x1347938+2)`、`tlStart[i] = *(u32*)(buf+4+4i)`；**无 sub 索引游标**；thprac 冻结 hook `0x4207A1`（`call 0x41F6F0`）命中时 `ECX = TL_i`、`TL_i+8` = 当前时间；解释器 `0x41F6F0` 指令头 `{+0 i16 time, +4 u16 opcode, +6 u16 size}`、跳表 `0x41FD2F`（opcode 0..0xC）、>0xC 直接前进；`TL_i+4` 为 float 小数累加器（修正总括「`TL+8` 可能为浮点」的猜测）。**Q2**：sub 表存在 = `*(u32*)(0x1347938+4)`（项 4B＝sub 起始指针，`0x40E530` 装载）；与**时间轴表**（`buf = *(u32*)0x1347938` 的表头）是两张独立表；`call sub`(opcode 0x29) case `0x411F20` 压栈后 `E+0x6E4 = subStart[id]`、深度 `E+0x2A7C`+1。**Q3**：显式调用栈存在（基址 `E+0x8FC`、帧 `0x218`、深度 `E+0x2A7C`、上限 `0xF`；`ret`(0x2A) 弹栈）；仅事实记录。**Q4**：宿主静态（`0x9A9B00`），`*(u32*)0x9A9AF8 = 0x9A9B00` 写于 `0x422FE5`；初始化 `0x422F40`/重置 `0x41EE70`/构造 `0x41EA60`；**无需 hook 捕获**。**Q5**：每帧驱动 `0x420620`（登记于 `mgr+0x954714`），逐 timeline 调 `0x41F6F0`；**thprac 已占用 `0x4207A1`**，推荐不挂 hook（静态读 `0x12FE0FC`），备用点 `0x41F6FC`(6B)/敌方 VM `0x410520`。**Q6**：**运行期不保留 sub 名**（sub 表仅指针），A 代无 eclmap/`.ecl` ⇒ 具名不可得，记录名只能取 thprac 章节表；入口改用时间值/偏移。**Q7**：`character=0x62F645`、`shotType=0x62F646`、`difficulty=0x626280`、`STAGE_NUM=0x1347FC8`（均 thprac 既有）。**Q8**：`ECLTimeWarp` 直写 `TL_i+8`，下一帧即生效（附展开式复核 `GetMemAddr(0x9a9af8,0x9545fc)=0x12FE0FC`）。**Q9**：沿用 latch；A 代读取方案＝读 `*(i32*)0x12FE0FC`（时间值），入口列记时间值，同时间 boss 分节用副键（boss 敌机 `[0x12FE098]→E+0x6E4` 或 `TL_0+0xC`）消歧。新增 §11「候选段落入口值表」（全 8 关道中 + 各 boss/终章，均为 `ECLTimeWarp` 入参，**候选、未真机**）。**证据**：`capstone` 只读反汇编 + 全程序 `disp32`/跳表字节扫描（脚本见 §12）。**未真机三态**；`docs/logger/th07_checkpoints.csv` 第 1 列（入口列，时间值口径）**未回填**。 |
| 2026-10-22 | **素材状态订正（不改结论）**：头部「ECL / decl 素材：无」改为「本报告撰写时无；2026-10-21 已补齐 `../exes/ecl/th07/ecldata1..8.ecl` + `../exes/ecl_dump/th07/*.decl`（`thecl -d7`、无 eclmap ⇒ `SubN`），**本报告结论未据此复核**」。同代 th06 已于 2026-10-22 完成 E1（`docs/th06/th06_ecl_re_report.md`）。**地址与结论一字未改。** |
| 2026-10-22 | **E3/E4 回填 + §11 表述订正**（本报告结论/地址未改）。CSV 第 1 列回填 **140/140**（章节 37 + boss 103，`ST6 #2` 按裁决删除、保留 `ST6 CHAOS`）。**§11.1**：列头 `后半 #5..#8` → 按记录名重列（st1 3107＝`ST1 后半 #1`、st2 3366＝`ST2 后半 #1`、st3 1805、st5 4883、st6 1180＝`ST6 #2`、st7/st8 3305/3191＝`前半 #5`）。**§11.2**：标明是**分组汇总**、补齐展开规则（ST4 三姐妹 3 行、ST6 BOSS7..10 各 2 行、ST7/ST8 END 逐行、合计 103 行）；`ST1_MID1` 的「ECLST3? 无」伪注删除；`ST7_END_S1..S10` 与 `ST8_END_S1..S11` 的 sub id 改为实测**跳号**值（`0x46,47,49,4b,4d,4f,51,53,54` / `0x48,49,4b,4d,4f,51,53,55,56,57`，且 `S1` 走 `ECLSetTime`）。**§11.3 新增**：`ST6 #2` 删除缘由。**§7.2/§12** 的「本机无 `.ecl`/无 eclmap」过期描述改写为「离线仅 `SubN` 级具名」。逐行权威表见 `docs/th07/th07_checkpoint_entry_report.md`。**未真机（🟡）。** |
| 2026-10-22 | **补「两级判定」交叉引用**：§10.2 追加说明 —— boss 段时间值大量重复（140 行 53 个不同值），一级 latch 只能定「boss 容器」，相位须用 boss 对象 `E+0x6E4` 的指令偏移按 §11.2 补丁偏移切区间（二级）。完整实现口径见 `docs/th07/th07_checkpoint_entry_report.md` §7。**结论/地址未改；🟡 未真机。** |

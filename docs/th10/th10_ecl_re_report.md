# TH10 ECL 段落监控 — 逆向结论（Q1–Q9）

> 对应需求：`docs/th10/th10_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`；方法论范例（结构参照）：`docs/th13/th13_ecl_re_report.md`（**同代 B 代，结论可直接对照**）。
> 二进制：`../exes/th10.exe`，487936 B，PE `TimeDateStamp = 1186086096`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**）。`.text` = VA `0x401000`–`0x465A81`，RVA `0x1000`–`0x64A81`，`SizeOfRawData = 0x64C00`，与 `thprac_launcher_games_def.h` 的 th10 ORIGINAL 条目一致。
> 引擎代：**B 代**；同代参照：th11/th12/th13（**th13 已有报告**，本轮 th10 与 th13 逐字段对照）。
> 方法：`pefile` + `capstone 5.0.7` 只读反汇编（自写脚本 `D:\workshop\reasm\th10\dl.py`），用「函数入口整段反汇编 + `int3` 边界扫函数头 + 原始字节搜索 + 交叉引用」定位（避开 `.text` 线性反汇编失步）。
> ECL / decl 素材：`../exes/ecl/th10/`（原始 `.ecl` 8 份）、`../exes/ecl_dump/th10/`（`thecl -d` 反编译 `.decl` 8 份），清单与来源见 `docs/logger/decompile_materials.md`。本报告**实际用到了**这批素材（离线解析 SCPT sub 表，见 §10.5）。
> 状态：**静态定案（🟡 已定案、未真机三态）**。**未做任何真机验证**。
> 地址口径：正文以 **VA** 为主，并标注 **RVA = VA − 0x400000**。
> 置信度图例：✅ 反汇编直接证实 ｜ 🟡 有强证据但缺一环（显式标注缺口）｜ ❌ 未能定案。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前 sub = 索引？指令指针？** | ✅ **是「指令地址（指针）」，不是 sub 索引 —— 与 th13 同构** | `ip = *(u32*)(T+0x04)`（T = 执行对象），指向运行期 ECL 文件缓冲内的**当前/下一条待执行指令的绝对地址**；`0` = 线程结束。本作**不存在**「当前 sub 索引」这一层。三处独立证据：解释器把 `[T+4]` 当指针读 `[ins+0x04]/[+0x06]/[+0x0A]`；`FindSubByName` 的返回值直接写进 `T+4`；`goto` 把 `ins+ins[+0x10]` 写进 `T+4`。 |
| **Q2 sub 表** | ✅ **确认，与 th13/th15 逐字段同构** | `mgr = *(u32*)0x477704`；`res = *(u32*)(mgr+0x54)`；`count = *(i32*)(res+0x08)`；`table = *(u32*)(res+0x8C)`；项 **8B**，`+0` = 名字 `char*`、`+4` = sub 数据基址（**首指令 = `+4 + 0x10`**）。加载器 `FUN_00450220`（SCPT 魔数 `0x54504353`）；按名查表 `FUN_00450470`。 |
| **Q3 函数栈** | ✅ **存在可遍历显式子调用栈** | 栈基址 `T+0x08`，栈指针 `SP = *(u32*)(T+0x1008)`（`0` = 空栈）；**帧长 `0x08`**；帧内 `+0x00`=调用者时间、**`+0x04`=调用者指令地址（= 调用者 `T+4`）**。压帧 `FUN_0044DF70`（`call` opcode `0x0B`），弹帧 `ret`(opcode `0x0A`) @ `0x44E20C`。⚠️ 帧长比 th13 少一个 dword（th13 为 `0x0C`，多存一个「旧 SP」）。 |
| **Q4 主线宿主捕获点** | ✅ **给出捕获点** | `"main"` @ `0x46D278`；宿主工厂 `FUN_0040CFB0`（`malloc(0x2518)` → 构造 → **`EAX` 返回宿主 H**）只有两处传 `"main"`：`call 0x40CFB0` @ **`0x418299`** 与 @ **`0x418499`**（下一条 `0x41829E` / `0x41849E` 可挂 **6B EHOOK**，命中时 `EAX` = H）。① 静态兜底：宿主链表 `mgr+0x58`（头）→ 节点 `H+0x116C`，**`H = *(u32*)(*(u32*)(mgr+0x58))`**。② 另一个「近零成本」捕获：`H = *(u32*)(Esi + 0x1014)`（见 Q5）。 |
| **Q5 解释器命中点** | ✅ **确认（`0x44FB9F` 即为主解释器「累加时间」那一句）** | `0x44FB9F`（`D9 1E` = `fstp dword ptr [esi]`，**2B EHOOK**）：`T+0 += 帧时间增量`；**命中时 `Esi` = 执行对象 T**（`*(f32*)Esi` = 当前 ECL 时间，`*(u32*)(Esi+0x1014)` = 宿主 H）。所在函数 `FUN_0044E1A0`（`0x44E1A0`/RVA `0x4E1A0`）= **ECL 主解释器**；opcode 映射 `0x44FCB4` / 跳表 `0x44FBC4`（`>0x57` 委派 `0x44FB68`）。⚠️ **它不是无条件每帧**：只在「下一条指令时间未到 → 累加时间后返回 0」这条路径命中，故**每帧每活跃线程至多 1 次**，且线程结束时**不**命中。 |
| **Q6 名字来源** | ✅ **运行期保留名字** | `table[i].+0` 为 `char*`，指向**已加载 ECL 缓冲内的名字串**（加载器 `FUN_00450220` 在 `0x4502CD` 填入）；游戏侧亦按 `table[i].+0` 按名 spawn（`0x40A6BD`）。 |
| **Q7 机体 / 子机 / 难度 / 关卡** | ✅ **沿用 thprac 既有地址** | `CHARA = 0x474C68`、`SUBSHOT = 0x474C6C`、`DIFF = 0x474C74`、`STAGE_NUM = 0x474C7C`、`PLAYER_PTR = 0x477834`、`REPLAY_MGR = 0x477838`、`GAME_MANAGER = 0x477810`（均 thprac 既有，本轮未复核语义）。 |
| **Q8 ECLJump** | ✅ **不影响判读** | `ECLJump` 只把 `goto`(`opcode 0x0C`) 写进运行期缓冲；`goto`(case `0x0C` @ `0x44E40F`) **只改 `T+0`(时间) 与 `T+4`(ip)**，不触碰 sub 表、不触碰任何「当前 sub」旁路字段。 |
| **Q9 段落判定口径** | ✅ **路过式 checkpoint（latch）**（与总括 §6 Q9 一致） | 读 `(sub, ip)` → 命中白名单则置当前段落，**未命中 no-op**（不回退 `main`）；本局首次命中前显示 `main`。**两处原 🟡 未定论已于 2026-10-17 定案**：(1) 段落入口白名单**已枚举完毕**（CSV 第 1 列 **109/109**）；(2) st1–st3 的道中段落确实落在 `main` 内部 ⇒ 这 15 行采用 **`sub名+0x偏移`** 二级粒度（另 3 行同名冲突同法消歧）。详见 §10.4/§10.5 与 `docs/th10/th10_checkpoint_entry_report.md`。 |
| **Q-A boss 宿主可达性（宿主链表）** | ✅🟡 **缺口消解：`mgr+0x58` 是「全宿主链」（含 boss）** | 全程序只有**一个**宿主工厂 `FUN_0040CFB0`（`push 0x2518` 仅 @ `0x40CFB4`；`0x40CFB0` 无间接调用；宿主构造 `FUN_0040D830` 仅 1 个调用点），它把**每一个**宿主（main / 中boss / 关底boss / 每个杂兵）**无条件**挂上 `mgr+0x58` ⇒ **不需要新捕获点**；但链头只是最先建的宿主（通常 `main`）⇒ **要拿 boss 宿主须沿 `node+4` 遍历**。详见 §5A.1。 |
| **Q-B boss 宿主捕获点** | ✅🟡 **给出捕获点** | boss / 中boss 由 `enmCreateA("Boss"/"MBoss", …)`（opcode **`0x101`**）创建 ⇒ 工厂调用点 `0x40ED11`，**EHOOK 落点 `0x40ED16`（RVA `0xED16`，命中时 `EAX` = 新宿主 H）**；全 `enmCreate*` 变体另见 `0x40EAEF` / `0x40EC0D`（练习菜单按索引 spawn 在 `0x40A6D3`）。名字是 `ins+0x14` 的**运行期内联串**（非静态）。详见 §5A.2。 |
| **Q-C boss 宿主的 `res` 有 boss sub？** | ✅🟡 **有（所有宿主共用同一份 `res`）** | `FUN_0040D830` 里 `H+0x102C = *(u32*)(mgr+0x54)`（**全局** res）⇒ 主线与 boss 宿主**共用一份 `res`**；th10 **无独立 boss 文件**（7 关 `ecli` 只有 `default.ecl`），boss sub 与 `main` **同在 `stageNN.ecl`** ⇒ 同表 ⇒ §2.3 的「地址 → 名字」区间反查**对 boss 行同样成立**。详见 §5A.3。 |

> **关键地址（VA / RVA）**：
> ECL 主解释器 `FUN_0044E1A0` = `0x44E1A0`/`0x4E1A0`（体 `0x44E1A0`–`0x44FBAD`）；分发循环头 `0x44E1E0`/`0x4E1E0`；
> **解释器命中点** `0x44FB9F`/`0x4FB9F`（2B）；ip 推进 `0x44FB77`/`0x4FB77`；累加时间并返回 0 `0x44FB96`–`0x44FBAD`；线程结束 `0x44FBB0`/`0x4FBB0`；
> `ret`(case `0x0A`) `0x44E20C`/`0x4E20C`；`call`(case `0x0B`) `0x44E35C`/`0x4E35C` → 压帧 `FUN_0044DF70`/`0x4DF70`；`goto`(case `0x0C`) `0x44E40F`/`0x4E40F`；
> opcode 字节映射 `0x44FCB4`/`0x4FCB4`；opcode 跳表 `0x44FBC4`/`0x4FBC4`；扩展 opcode 委派 `0x44FB68`/`0x4FB68`；
> 线程驱动器 `FUN_0044FD10`/`0x4FD10`（`EDI` = 宿主 H）；宿主「跑一帧」`FUN_0040DC80`/`0xDC80`（参数 = `H+0x103C`）；全宿主循环 `FUN_0040D750`/`0xD750`（`EDI` = mgr，遍历 `mgr+0x58`）；
> 宿主工厂 `FUN_0040CFB0`/`0xCFB0`（`EAX` = 参数结构，栈 = 名字）；宿主构造 `FUN_0040D830`/`0xD830`（`ESI` = H，`EAX` 返回 H）；`"main"` @ `0x46D278`/`0x6D278`；spawn 站点 `0x418299`/`0x18299`、`0x418499`/`0x18499`（**EHOOK 落点** `0x41829E`/`0x1829E`、`0x41849E`/`0x1849E`）；
> **boss / 杂兵宿主 spawn（§5A）**：扩展 opcode 处理器 `FUN_0040E770`/`0xE770`（蹦床入口 `0x40E760`/`0xE760`；opcode 映射表 `0x411EE0`/`0x11EE0`、跳表 `0x411D30`/`0x11D30`）内的 **3 处**工厂调用点 `0x40EAEA`/`0xEAEA`、`0x40EC08`/`0xEC08`、`0x40ED11`/`0xED11`（**EHOOK 落点** `0x40EAEF`/`0xEAEF`、`0x40EC0D`/`0xEC0D`、**`0x40ED16`/`0xED16`**，命中时 `EAX` = 新宿主 H）；练习菜单按 sub 索引 spawn `0x40A6CE`/`0xA6CE`（落点 `0x40A6D3`/`0xA6D3`，宿主 `FUN_0040A450`/`0xA450`）；`mgr+0x10` = **`bossExists`**（thprac `thprac_th10.cpp:548`）；
> SCPT 加载器 `FUN_00450220`/`0x50220`（`ECX` = res）；按名查表 `FUN_00450470`/`0x50470`；异步线程派生 `FUN_004500D0`/`0x500D0`；
> 管理器全局 `0x477704`/`0x37704`（thprac 名 `ENEMY_MANAGER_PTR`，实为**脚本/ECL 管理器**）；mgr 构造 `FUN_0040D6B0`/`0xD6B0`（`malloc(0x68)`）；res 构造 `FUN_0040D260`/`0xD260`（`malloc(0x1098)`，vtable `0x46D0B4`）；H vtable `0x46D0C0`（slot 0 = 扩展 opcode 处理器蹦床 `0x40E760`）。

---

## 1. 统一指针链（TH10 定案）

```text
// ── ECL 资源 / sub 表（与 th13 逐字段同构）─────────────────────────────
mgr   = *(u32*)0x477704                              // 脚本管理器（0x68 B；thprac 名 ENEMY_MANAGER_PTR）
res   = *(u32*)(mgr + 0x54)                          // ECL 资源对象（0x1098 B，vtable 0x46D0B4）
count = *(i32*)(res + 0x08)                          // sub 总数
table = *(u32*)(res + 0x8C)                          // sub 表（项 8 B）
name_i  = *(char**)(table + i*8)                     // 项 +0 = 名字 char*（指向已加载缓冲内的名字串）
start_i = *(u32*)(table + i*8 + 4) + 0x10            // 项 +4 = sub 数据基址 ⇒ 首指令 = +4 + 0x10

// ── 主线宿主（进关时由 0x41829E / 0x41849E 抓到 EAX）──────────────────
H     = 主线 ECL 宿主对象（0x2518 B；vtable 0x46D0C0）
H + 0x004   = T                                      // ≡ H+8（内嵌线程）
H + 0x008   = T 本体（0x1000 B 栈区 + 字段，见下）
H + 0x101C  = H                                      // 自指针 ⇒ T+0x1014 = H
H + 0x102C  = res                                    // 宿主持有的 ECL 资源（= 上面那个 res）
H + 0x1030  = T                                      // 线程链表头（异步线程挂在 +0x1034 起）
H + 0x116C  = H                                      // 宿主链表节点 { +0 = H, +4 = next, +8 = prev }
H + 0x1480  = 脚本信息标志位（thprac 的「主线判据」字段；见 §6.3）
H + 0x2514  = H                                      // 自指针（供扩展 opcode 处理器反查宿主）

// ── 线程/脚本上下文 T（= H+8；异步线程为独立 malloc(0x1024)）─────────────
T + 0x000   = f32  当前 ECL 时间（跨帧累积）
T + 0x004   = u32  当前（下一条待执行）ECL 指令的绝对地址；0 = 线程结束   ★Q1
T + 0x008   = u32  调用栈区基址
T + 0x1008  = i32  调用栈指针 SP（字节偏移，0 = 空栈）
T + 0x100C  = i32  局部/表达式栈指针（与 0x1008 共用基址 T+8）
T + 0x1014  = u32  宿主 H（自指针；opcode > 0x57 时对它做虚调用）
T + 0x101C  = u8   指令选择掩码（难度位掩码；供 `test byte [T+0x101C], ins[0xA]`）
(T + 0x1024 = res)  ⚠️ **仅主线内嵌线程有**（= H+0x102C）；异步线程对象只 malloc(0x1024)，无此槽

// ── 宿主链表（在 mgr 里）───────────────────────────────────────────────
mgr + 0x54 = res
mgr + 0x58 = 宿主链表头（指向 H+0x116C）    mgr + 0x5C = 尾    mgr + 0x60 = 计数    mgr + 0x64 = 另一计数
```

**与 th13（同代 B 代）的差异（重要，不可照搬偏移）**

| 项 | **th13** | **th10（本作）** |
| --- | --- | --- |
| 管理器全局 | `0x4C2188` | **`0x477704`** |
| 宿主 → res | `H+0x11CC` | **`H+0x102C`** |
| 宿主 → 线程 | `H+0x11D0`（≡ `H+4`） | **`H+0x1030`（≡ `H+4`）** |
| 当前 sub | `T+4` = **指令绝对地址** | **完全一致** ✅ |
| 调用栈基址 / SP | `T+0x08` / `T+0x1008` | **完全一致** ✅ |
| 帧长 / 帧内 | `0x0C`，帧 `+0x08` = 调用者 ip | **`0x08`**，帧 `+0x04` = 调用者 ip（**无「旧 SP」槽**） |
| sub 表 | `res+0x8C`，项 8B，`+0`=名字 `+4`=起始，首指令 `+4+0x10` | **完全一致** ✅ |
| `res` 对象 / 加载器 | `res = *(mgr+0xAC)`；`FUN_004795B0`（SCPT） | **`res = *(mgr+0x54)`**；`FUN_00450220`（SCPT） |
| 宿主链节点 | `H+0x14CC`，挂 `mgr+0xB0/0xB4` | **`H+0x116C`，挂 `mgr+0x58/0x5C`** |

> **同构结论**：**Q1 / Q2 / Q3（骨架）/ Q6 与 th13 同构**（`[T+4]` 都是「指令地址」而不是 sub 索引；sub 表逐字段一致）。**宿主/管理器/线程的偏移各作不同**，只能按「形状」搬运，不能按数值搬运。

---

## 2. Q1 —— `[T+4]` 是**指令地址（指针）**，不是 sub 索引

### 2.1 判据（主解释器把 `[T+4]` 当**指针**用）

```asm
0044e1a0  81ec08010000      sub   esp, 0x108
0044e1a9  8bf0              mov   esi, eax                    ; esi = T（EAX 传入的执行对象）
0044e1ab  8b4604            mov   eax, dword ptr [esi + 4]    ; eax = T+4
0044e1ae  8d5e04            lea   ebx, [esi + 4]              ; ebx = &T+4
0044e1b3  3bc5              cmp   eax, ebp (0)
0044e1b5  750f              jne   0x44e1c6
0044e1b9  83c8ff            or    eax, 0xffffffff             ; ip == 0 → 返回 −1（线程已结束）
0044e1c6  db00              fild  dword ptr [eax]               ; ★ 读 [ip+0] = 指令的「时间」
0044e1c9  d81e              fcomp dword ptr [esi]               ;   与 T+0 比较
0044e1cd  f6c441            test  ah, 0x41
0044e1d0  0f8ac0190000      jp    0x44fb96                    ; ins.time >= T+0 → 累加时间后返回 0
0044e1e0  8b3b              mov   edi, dword ptr [ebx]        ; ★ 分发循环头：edi = T+4（当前指令）
0044e1e2  8a470a            mov   al, byte ptr [edi + 0xa]    ; ★ 指令 +0xA = 选择掩码字节
0044e1e5  84861c100000      test  byte ptr [esi + 0x101c], al
0044e1eb  0f8486190000      je    0x44fb77                    ; 掩码不匹配 → 跳过该指令
0044e1f1  0fbf4704          movsx eax, word ptr [edi + 4]       ; ★ 指令 +4 = opcode（16 位）
0044e1f5  83f857            cmp   eax, 0x57
0044e1f8  0f876a190000      ja    0x44fb68                    ; > 0x57 → 委派 T+0x1014 的虚方法
0044e1fe  0fb688b4fc4400    movzx ecx, byte ptr [eax + 0x44fcb4] ; 字节映射表
0044e205  ff248dc4fb4400    jmp   dword ptr [ecx*4 + 0x44fbc4] ; 跳表分发
```

`[T+4]` 被当作**指针**：读 `[ip+0]`（int 时间）、`[ip+4]`（opcode）、`[ip+6]`（size）、`[ip+0xA]`（选择掩码）—— 它**绝不可能是索引或偏移**。此外整个解释器对 `[T+4]` 的写入**只有「绝对地址」一种形态**（见 §2.2），**本作不存在「当前 sub 索引」这一层**。

> **指令头布局（由上述指令流 + `ECLJump` 写入格式推出）**：`+0x00` = int 时间 ｜ `+0x04` = u16 opcode ｜ `+0x06` = u16 size ｜ `+0x08` = u16 浮点参数掩码 ｜ `+0x0A` = u8 选择掩码（难易度） ｜ `+0x0C` = u8 局部槽数 ｜ `+0x0D..0x0F` 填充 ｜ **`+0x10` 起 = 参数（dword）**。与 `thprac_th10.cpp:1297` 的 `ECLJump` 写出的 6 个 dword 完全吻合（`0x0018000C` ⇒ `opcode=0x0C`、`size=0x18`；`0x02FF0000` ⇒ `ins[0x0A]=0xFF` 任意难度；`ins[0x10]`=dest−start、`ins[0x14]`=at_frame）。

### 2.2 字段写入点（谁写 `T+4`）

| VA / RVA | 语义 | 片段 |
| --- | --- | --- |
| `0x40D87E`–`0x40D881` / `0xD87E` | **宿主构造**：`T+4 = 0`、`H+4 = T` | `lea eax,[esi+8]` → `mov [esi+4],eax`（另见 `0x40D863: mov [esi+8],0`、`0x40D866: mov [esi+0xc],0`） |
| **`0x40DA1F`** / `0xDA1F` | **进关/spawn：`T+4 = FindSubByName(res, "main")` 的返回值** | `call 0x450470` → `mov edx,[esi+4]` → `mov [edx+4],eax` |
| `0x44FB83` / `0x4FB83` | **逐指令推进**：`T+4 = T+4 + ins.size` | `movzx ecx,word[eax+6]` → `add ecx,eax` → `mov [ebx],ecx` |
| `0x44E238` / `0x4E238` | **`ret`（0x0A）恢复调用者 ip** | `mov edx,[eax+edi]` → `mov [ebx],edx` |
| `0x44E40F`–`0x44E419` / `0x4E40F` | **`goto`（0x0C）重定位**：`T+4 = ins + ins[+0x10]` | `mov ecx,[eax+0x10]` → `jmp 0x44fb81`（→ `add ecx,eax; mov [ebx],ecx`） |
| `0x44FBB0` / `0x4FBB0` | **线程结束**：`T+4 = 0` | `mov dword ptr [ebx], ebp` |
| `0x450108` / `0x50108` | **异步线程派生**：新线程 `T+4 = 0` 后由压帧函数写入该 sub 首指令 | `mov [esi+4], ebx`（=0）→ `call 0x44df70` |

### 2.3 「地址 → 当前 sub」的取法（因无 subIdx）

```cpp
// 用 sub 表建「起始地址 → 索引」区间表（一次性 / 每次 res 变化时重建）
//   start_i = *(u32*)(table + i*8 + 4) + 0x10
// 按 start_i 升序排序；当前 sub = 满足 start_i <= ip < start_{i+1} 的 i
// name = *(char**)(table + i*8)
```
> ⚠️ 表项按**名字字典序**存放（`FUN_00450470` 二分）⇒ **地址并不单调**，必须显式排序后二分，不能直接按索引二分。
> ⚠️ 越界/脏指针保护：`ip` 必须落在某 `[start_i, start_{i+1})` 内，否则视为「不在已知 sub」；`ip == 0` → 线程已结束。

### 2.4 与总括 §5.1 矛盾的裁定（**首要矛盾**）

总括 §5.1 的遗留矛盾「`[执行对象+4]` 到底是 sub 索引还是指令指针」在本作有明确答案：

- **th10 = 「指令指针」**（与 **th13 同构**；两者同属 B 代「执行对象 + ip」形态）。
- **th15/th16/th17/th18 = 「索引」**（C 代），**不可互相搬运**。
- 本作另有一层独立佐证：**扩展 opcode 处理器**（`T+0x1014` = H 的 vtable slot 0 = 蹦床 `0x40E760`）读的是**同一个 `T+4`**：
  ```asm
  0040e760  81c13c100000      add   ecx, 0x103c        ; ecx = H → H+0x103C
  0040e766  e905000000        jmp   0x40e770
  0040e786  8b93d8140000      mov   edx, dword ptr [ebx + 0x14d8]  ; [H+0x103C+0x14D8] = [H+0x2514] = H
  0040e793  8b4204            mov   eax, dword ptr [edx + 4]       ; H+4 = T
  0040e797  8b7004            mov   esi, dword ptr [eax + 4]       ; ★ T+4 = 当前指令（与主解释器同源）
  0040e79c  668b4604          mov   ax, word ptr [esi + 4]         ; opcode
  ```
  ⇒ 主解释器与扩展处理器读的是**同一个 `T+4`**，互为交叉印证（对应 th13 报告 §6.4 的同类旁证）。

---

## 3. Q2 —— sub 表：`res+0x8C`，项 8B（与 th13/th15 逐字段一致）

### 3.1 管理器 / 资源对象构造

```asm
; res 的创建（ECX/EBX = mgr；EDX = 新对象）
0040d28d  6898100000        push  0x1098                    ; malloc(0x1098)
0040d295  e8f9510400        call  0x452493
0040d2a5  89aa90100000      mov   [edx+0x1090], ebp
0040d2b1  b926040000        mov   ecx, 0x426                ; 0x426*4 = 0x1098（清零）
0040d2ba  f3ab              rep   stosd
0040d2bc  c702b4d04600      mov   dword ptr [edx], 0x46d0b4 ; res vtable
0040d2cd  894b54            mov   dword ptr [ebx + 0x54], ecx   ; ★ mgr+0x54 = res
0040d2d3  ff5208            call  dword ptr [edx + 8]         ; res->LoadECL(name)（vtable+8 = 0x40CD20）
; mgr 的创建
0040d6b1  6a68              push  0x68                      ; malloc(0x68)
0040d6d4  891504774700      mov   dword ptr [0x477704], edx ; ★ mgr 全局
```

### 3.2 加载器（写侧）—— `FUN_00450220`@`0x450220`（`ECX` = res）

```asm
0045022d  894c830c          mov   dword ptr [ebx + eax*4 + 0xc], ecx  ; res[+0xC+slot*4] = ECL 文件缓冲
0045023c  813953435054      cmp   dword ptr [ecx], 0x54504353    ; 魔数 "SCPT"
00450244  6683790401        cmp   word ptr [ecx + 4], 1          ; 版本 1
0045025d  0fb74806          movzx ecx, word ptr [eax + 6]        ; 头尺寸 hdr
00450261  8b5308            mov   edx, dword ptr [ebx + 8]       ; count（跨文件累加）
00450265  8bab8c000000      mov   ebp, dword ptr [ebx + 0x8c]    ; 旧 table
0045026d  8d7c0124          lea   edi, [ecx + eax + 0x24]        ; 名字区 = buf+0x24+hdr
00450271  0fb74010          movzx eax, word ptr [eax + 0x10]     ; 本文件 sub 数 n
00450275  03d0              add   edx, eax                       ; count += n
0045027b  8d3c87            lea   edi, [edi + eax*4]             ; 名字串区 = 名字区 + n*4
00450280  c1e003            shl   eax, 3
0045028b  e876240000        call  0x452706                       ; malloc(count*8)
00450295  89838c000000      mov   dword ptr [ebx + 0x8c], eax    ; res+0x8C = 新表  ← table
004502b0  8b5304            mov   edx, dword ptr [ebx + 4]
004502b3  8b44930c          mov   eax, dword ptr [ebx + edx*4 + 0xc]  ; 该槽的文件缓冲
004502bb  0301              add   eax, dword ptr [ecx]           ; 缓冲 + offset[i]
004502bd  8b938c000000      mov   edx, dword ptr [ebx + 0x8c]
004502c3  8944f204          mov   dword ptr [edx + esi*8 + 4], eax   ; table[i].+4 = sub 数据基址
004502c7  8b838c000000      mov   eax, dword ptr [ebx + 0x8c]
004502cd  893cf0            mov   dword ptr [eax + esi*8], edi        ; table[i].+0 = 名字 char*
004502d2  8d5001            lea   edx, [eax + 1]                     ; 扫 NUL 推进到下一个名字
004502d5  8a08              mov   cl, byte ptr [eax]
004502d8  84c9              test  cl, cl
004502da  75f9              jne   0x4502d5
004502ed  46                inc   esi
004502f1  3bf0              cmp   esi, eax                           ; 直到 count
```
⇒ `res+0x8C` = 表；项 **8 字节**；`+0` = 名字 `char*`（指向**已加载缓冲内**的名字串，运行期保留）；`+4` = `buffer + offset[i]`（sub 数据基址）；`res+0xC+slot*4` = 文件缓冲数组。**与 th13 §3.2 完全一致。**

### 3.3 按名查表 —— `FUN_00450470`@`0x450470`（`EAX` = res，`[esp+4]` = 名字；二分）

```asm
00450472  8b5808            mov   ebx, dword ptr [eax + 8]       ; count
0045047d  8b808c000000      mov   eax, dword ptr [eax + 0x8c]    ; table
00450498  8b34f9            mov   esi, dword ptr [ecx + edi*8]   ; table[mid].+0 = 名字
004504a0  8a11              mov   dl, byte ptr [ecx]             ; 逐字节 strcmp(name, table[mid].name)
...
004504e5  8b442410          mov   eax, dword ptr [esp + 0x10]    ; table
004504e9  8b44f804          mov   eax, dword ptr [eax + edi*8 + 4] ; table[idx].+4
004504f0  83c010            add   eax, 0x10                     ; ★ +0x10 ⇒ 该 sub 的首条指令
004504f5  c20400            ret   4
```
⇒ `FindSubByName(res, name) = table[idx].+4 + 0x10`（未命中返回 `0`）。**`+0x10` 正是 sub 前导 16 字节**（与 §2.1 的指令头 `0x10` 一致）。**与 th13 `FUN_004797D0` 逐指令同型。**

### 3.4 游戏侧交叉印证（非加载器，直接证明字段语义）

```asm
; ① 按 sub 索引 spawn（读 table[i].+0 = 名字）
0040a6af  a104774700        mov   eax, dword ptr [0x477704]      ; mgr
0040a6b4  8b5054            mov   edx, dword ptr [eax + 0x54]    ; res = *(mgr+0x54)
0040a6b7  8b928c000000      mov   edx, dword ptr [edx + 0x8c]    ; table = res+0x8C
0040a6bd  8b0cca            mov   ecx, dword ptr [edx + ecx*8]   ; table[i].+0 = 名字 char*
0040a6ce  e8dd280000        call  0x40cfb0                       ; 按名 spawn 宿主
; ② 读 sub 总数
0040a381  a104774700        mov   eax, dword ptr [0x477704]      ; mgr
0040a386  8b4854            mov   ecx, dword ptr [eax + 0x54]    ; res
0040a389  8b5108            mov   edx, dword ptr [ecx + 8]       ; count = res+8
0040a396  8996f4010000      mov   dword ptr [esi + 0x1f4], edx
```
⇒ 三条链一次坐实：`res = *(*(0x477704)+0x54)`、`count = res+8`、`table = res+0x8C`、`table[i].+0 = char* 名字`。

### 3.5 `GetMemAddr(0x477704, 0x54, 0xC)` 的展开（与 thprac `ECLHelper` 对齐）

`thprac_games.h:569-578`：`GetMemAddr(addr, offset, rest...)` 递归为 `GetMemAddr(*(R*)addr + offset, rest...)`，末层直接返回**地址**。
⇒ `GetMemAddr(0x477704, 0x54, 0xC) = *(u32*)(*(u32*)0x477704 + 0x54) + 0xC = res + 0xC`。
而 `res+0xC` 正是加载器用的 `res+0xC+slot*4` 数组首址（§3.2）⇒ **`THSectionPatch` 的 `SetBaseAddr` 一级对象就是 `res`**（`thprac_th10.cpp:2162`）。**与 th13 §3.4 结论相同**（只是 th13 是 `res = *(mgr+0xAC)`）。

---

## 4. Q3 —— ECL 子调用栈（**存在、可遍历**）

### 4.1 结论

1. **有显式可遍历的子调用栈**，随解释器自动维护，与线程 T 内嵌：
   - **帧存储基址**：`base = T + 0x08`
   - **栈指针**：`SP = *(u32*)(T + 0x1008)`（int，**字节**偏移；`0` = 空栈）
   - **帧长**：**`0x08`**（2 个 dword）
2. **帧内布局**（压帧顺序 = 地址升序）：`+0x00` = 调用时的 `T+0`（时间，float）；**`+0x04` = 调用者的指令地址（= 调用者当时的 `T+4`）**。
3. 「栈顶帧存的是**调用者 ip**」（不是调用者 sub 索引 —— 本作没有 subIdx）。
4. 栈区上限 **`0x1000` 字节**（`SP` 有效域 `0 < SP ≤ 0x1000`）⇒ 物理上限 512 帧（`0x1000 / 0x08`）。
5. ⚠️ **与 th13 的差异**：th13 帧长 `0x0C`（多一个「旧 SP」槽）；th10 **帧长 `0x08`**，压帧时对 `base+SP0-4` 做了一次「读回原值再写回」的冗余操作（编译器产物，不构成第 3 槽）。遍历公式仍与 th13 相同（调用者 ip 在 `base+SP-4`）。

### 4.2 `ret`（opcode `0x0A`）—— case @ `0x44E20C`/RVA `0x4E20C`

```asm
0044e20c  8d7e08            lea   edi, [esi + 8]              ; 栈区基址 = T+8
0044e20f  8bc7              mov   eax, edi
0044e211  e8ba240000        call  0x4506d0                    ;（清表达式临时值）
0044e216  39ae08100000      cmp   dword ptr [esi + 0x1008], ebp ; SP == 0 ?
0044e21c  0f848e190000      je    0x44fbb0                    ;   空 → T+4 = 0（线程结束）
0044e222  8b8700100000      mov   eax, dword ptr [edi + 0x1000] ; eax = SP
0044e228  83c0fc            add   eax, 0xfffffffc
0044e22f  898700100000      mov   dword ptr [edi + 0x1000], eax ; SP -= 4
0044e235  8b1438            mov   edx, dword ptr [eax + edi]    ; 帧顶 = base + SP − 4
0044e238  8913              mov   dword ptr [ebx], edx          ; ★ T+4 = 调用者 ip
0044e23a  8b8700100000      mov   eax, dword ptr [edi + 0x1000]
0044e240  83c0fc            add   eax, 0xfffffffc
0044e247  898700100000      mov   dword ptr [edi + 0x1000], eax ; SP -= 4
0044e24d  8b0407            mov   eax, dword ptr [edi + eax]
0044e250  8906              mov   dword ptr [esi], eax          ; ★ T+0 = 调用者时间
0044e252  392b              cmp   dword ptr [ebx], ebp
0044e254  0f8458190000      je    0x44fbb2
0044e25a  e918190000        jmp   0x44fb77
```
⇒ 弹栈顺序 `ip → 时间`，**帧长 0x08**，`base = T+8`。（`[edi+0x1000]` = `[T+0x1008]` = SP。）

### 4.3 `call`（opcode `0x0B`）—— case @ `0x44E35C` → 压帧 `FUN_0044DF70`@`0x44DF70`

调用点：
```asm
0044e35c  55                push  ebp
0044e35d  56                push  esi
0044e35e  8bc6              mov   eax, esi
0044e360  e80bfcffff        call  0x44df70                  ; ← 压帧（含参数拷贝）
0044e365  85c0              test  eax, eax
0044e367  0f8545180000      jne   0x44fbb2                 ; 失败 → 线程结束
0044e36d  e913180000        jmp   0x44fb85                 ; 回到「时间判定/推进」
```
压帧体（`esi` = T；`edi` = `&T+4`；`edx` = 目标 sub 首指令）：
```asm
0044df8f  8d6c9104          lea   ebp, [ecx + edx*4 + 4]     ; 参数区游标（ecx = ins[+0x10]）
0044df89  8b8608100000      mov   eax, dword ptr [esi + 0x1008] ; SP0
0044df9b  8944241c          mov   dword ptr [esp + 0x1c], eax
0044df9f  8d4808            lea   ecx, [eax + 8]             ; SP0 != 0 → 参数区从 SP0+8 起
0044dfbc  b90c000000        mov   ecx, 0xc                   ; SP0 == 0 → 规范化 SP=4 后参数区 = 0x0C
...
0044e08e  85c0              test  eax, eax                 ; eax = SP0
0044e099  7459              je    0x44e0f4                   ; SP0 == 0 → 走「底帧」分支
0044e0d3  8b0a              mov   ecx, dword ptr [edx]       ; edx = T → T+0（时间）
0044e0d5  894c3008          mov   dword ptr [eax + esi + 8], ecx  ; ★ 帧[+0x00] = 调用者时间
0044e0d9  019e08100000      add   dword ptr [esi + 0x1008], ebx   ; SP += 4
0044e0f0  8b0f              mov   ecx, dword ptr [edi]       ; edi = &T+4 → 调用者 ip
0044e124  894c3008          mov   dword ptr [eax + esi + 8], ecx  ; ★ 帧[+0x04] = 调用者 ip
0044e128  019e08100000      add   dword ptr [esi + 0x1008], ebx   ; SP += 4
```
⇒ 压栈 `{调用者时间, 调用者 ip}`，**帧长 `0x08`**，与 §4.2 弹栈次序严格对称。
（`eax` 为「帧相对偏移」：`T+8+相对偏移` 即绝对地址；`T+0x1008` 存该相对偏移。）

### 4.4 遍历规则（推荐实现）

```cpp
// T = *(u32*)(H + 0x1030)（主线内嵌线程；≡ *(u32*)(H+4)）
uint32_t sp = *(uint32_t*)(T + 0x1008);          // 0 = 无 caller
int n = 0;
while (sp >= 0x08 && sp <= 0x1000 && n < 256) {
    uint32_t callerIp = *(uint32_t*)(T + 0x08 + sp - 4);   // 帧 +0x04 = 调用者 ip
    if (!callerIp) break;
    int callerSub = SubOfAddress(callerIp);               // §2.3 的地址区间表
    if (callerSub < 0) break;                             // 越界 → 停止
    ancestors[n++] = NameOf(callerSub);                   // table[callerSub].+0
    sp -= 0x08;                                           // 帧长 0x08
}
// 祖先序：ancestors[0] = 直接调用者 … 最后 = 最外层；当前 sub 由 T+4 另行判定
```
- **只在指令边界（帧间稳定态，如 IGI 渲染）遍历**：局部/表达式栈共用基址 `T+8`、指针 `T+0x100C`，执行中遍历有误读风险。
- 保护：`T`、`sp`、`callerIp`、`callerSub` 全部校验。
- ⚠️ 与 th13 的 `sp -= 0x0C` 不同，本作是 **`sp -= 0x08`**。

### 4.5 异步 `spawn` 的例外（**本作重要**）

th10 的 `@xxx() async` 会**新建一个独立线程对象**并挂到**同一个宿主的线程链表**上（`FUN_004500D0`@`0x4500D0`）：

```asm
004500d2  6824100000        push  0x1024                   ; malloc(0x1024) = 异步线程 T'
004500d7  e8b7230000        call  0x452493
004500e5  899808100000      mov   dword ptr [eax + 0x1008], ebx ; T'+0x1008 = 0（SP）
004500eb  89980c100000      mov   dword ptr [eax + 0x100c], ebx ; T'+0x100C = 0（表达式栈）
004500f7  6a0c              push  0xc                      ; malloc(0x0C) = 链表节点
00450102  898e10100000      mov   dword ptr [esi + 0x1010], ecx ; T'+0x1010
00450108  89be14100000      mov   dword ptr [esi + 0x1014], edi ; ★ T'+0x1014 = H
0045010e  891e              mov   dword ptr [esi], ebx          ; T'+0x000 = 0
00450110  895e04            mov   dword ptr [esi + 4], ebx      ; T'+0x004 = 0
00450116  8a8a1c100000      mov   cl, byte ptr [edx + 0x101c]   ; 复制难度掩码
0045011c  888e1c100000      mov   byte ptr [esi + 0x101c], cl
00450122  8d8f30100000      lea   ecx, [edi + 0x1030]           ; ★ 宿主线程链表头
00450128  8930              mov   dword ptr [eax], esi          ; 节点 +0 = T'
0045013a  895004            mov   dword ptr [eax + 4], edx      ; 节点 +4 = next
00450143  894104            mov   dword ptr [ecx + 4], eax      ; 头->next = 新节点
00450146  894808            mov   dword ptr [eax + 8], ecx      ; 新节点 +8 = 头（prev）
00450154  e817deffff        call  0x44df70                     ; 用新线程压帧 → 执行该 sub
```

⇒ **同一宿主内可以有多个线程**（主线 `main` + 若干 `async` 子）。`FUN_0044FD10` 每帧**遍历整个线程链表**逐个跑（§6.2）。故：
- 读「当前 sub」时，若只读 `*(u32*)(H+0x1030)`，得到的是**主线线程**；道中某些段落跑在 **async 线程**里（见 §10.4）。
- `res` 指针：主线内嵌线程有 `T+0x1024 = res`；**异步线程对象只有 0x1024 字节，没有该槽** ⇒ **推荐统一走全局链 `res = *(u32*)(*(u32*)0x477704 + 0x54)`**。

---

## 5. Q4 —— 主线宿主捕获点

### 5.1 「spawn main」站点

字符串 `"main"` @ **`0x46D278`**（RVA `0x6D278`；相邻字符串为 `"initialize FrontInf\n"`，附近为 `.\src\game\game.cpp:366 GameInf`），全程序仅被 `0x41828F` / `0x41848F` 两处引用。宿主工厂 `FUN_0040CFB0`@`0x40CFB0`：

```asm
0040cfb4  6818250000        push  0x2518                   ; malloc(0x2518) = 宿主对象
0040cfbb  e8d3540400        call  0x452493
0040cfc7  8b4c2418          mov   ecx, dword ptr [esp + 0x18]  ; 名字（"main"）
0040cfcb  51                push  ecx
0040cfcc  8bf0              mov   esi, eax                 ; esi = 新宿主
0040cfce  e85d080000        call  0x40d830                 ; ★ 宿主构造 FUN_0040D830(ESI = H)
0040d19e  8bc5              mov   eax, ebp                 ; ⤴ 构造器内：mov eax,esi
0040d1a2  c20800            ret   8                        ; ★ 返回 EAX = H
```
两处「spawn main」站点（同型代码，分属同一大函数 `FUN_00418190`@`0x418190`/RVA `0x18190` 的两条分支）：

```asm
; 站点 1
0041828a  a104774700        mov   eax, dword ptr [0x477704]   ; mgr
0041828f  6878d24600        push  0x46d278                    ; "main"
00418294  50                push  eax                          ; mgr
00418295  8d442418          lea   eax, [esp + 0x18]            ; 参数结构
00418299  e8124dffff        call  0x40cfb0                    ; ★ EAX = 主线宿主 H（下一条 0x41829E 可挂 EHOOK）
0041829e  8b3d0c774700      mov   edi, dword ptr [0x47770c]    ; ← 6B，EHOOK 覆盖长度充足

; 站点 2（另一重开/复位路径，同型）
00418489  8b0d04774700      mov   ecx, dword ptr [0x477704]   ; mgr
0041848f  6878d24600        push  0x46d278                    ; "main"
00418494  51                push  ecx
00418499  e8124bffff        call  0x40cfb0                    ; ★ EAX = 主线宿主 H（下一条 0x41849E 可挂 EHOOK）
0041849e  8b3d0c774700      mov   edi, dword ptr [0x47770c]    ; ← 6B
```

⇒ **Q4 答案（推荐）**：捕获点 = **`0x41829E` / `0x41849E`**（`call` 之后的下一条指令），命中时 **`EAX` = 主线 ECL 宿主 H**。（对应总括里 TH15 的 `0x43BBC8/0x43D48E/0x43D6BE` 角色。）

### 5.2 宿主构造器关键字段（`FUN_0040D830`@`0x40D830`，`ESI` = H）

```asm
0040d849  c706c0d04600      mov   dword ptr [esi], 0x46d0c0        ; Host vtable = 0x46D0C0
0040d83b  899e10100000      mov   dword ptr [esi + 0x1010], ebx    ; T+0x1008 = 0（SP）
0040d841  899e14100000      mov   dword ptr [esi + 0x1014], ebx    ; T+0x100C = 0（表达式栈）
0040d863  895e08            mov   dword ptr [esi + 8], ebx         ; T+0x000 = 0（时间）
0040d866  895e0c            mov   dword ptr [esi + 0xc], ebx       ; T+0x004 = 0（ip）
0040d869  89b61c100000      mov   dword ptr [esi + 0x101c], esi    ; ★ T+0x1014 = H（自指针）
0040d87e  8d4608            lea   eax, [esi + 8]
0040d881  894604            mov   dword ptr [esi + 4], eax         ; ★ H+4 = H+8 = T
0040d88d  898630100000      mov   dword ptr [esi + 0x1030], eax    ; ★ H+0x1030 = T（线程链表头）
0040d91d  89b66c110000      mov   dword ptr [esi + 0x116c], esi    ; ★ 宿主链表节点 +0 = H
0040d9f2  a104774700        mov   eax, dword ptr [0x477704]
0040da0d  8b4054            mov   eax, dword ptr [eax + 0x54]      ; res
0040da11  89862c100000      mov   dword ptr [esi + 0x102c], eax    ; ★ H+0x102C = res
0040da17  e8542a0400        call  0x450470                         ; FindSubByName(res, 名字)
0040da1c  8b5604            mov   edx, dword ptr [esi + 4]         ; T
0040da1f  894204            mov   dword ptr [edx + 4], eax         ; ★ T+4 = 该 sub 首指令（"main" 时即 main 首指令）
0040dacb  8bc6              mov   eax, esi                         ; 返回 H
```

### 5.3 宿主入 mgr 链表（静态兜底依据）

```asm
; FUN_0040CFB0 内（eax = mgr，ecx = H+0x116C）
0040d162  397858            cmp   dword ptr [eax + 0x58], edi      ; edi = 0
0040d16d  894858            mov   dword ptr [eax + 0x58], ecx      ; mgr+0x58 = 头
0040d170  eb19              jmp   0x40d18b
0040d172  8b505c            mov   edx, dword ptr [eax + 0x5c]      ; 尾
0040d185  894a04            mov   dword ptr [edx + 4], ecx
0040d18e  89485c            mov   dword ptr [eax + 0x5c], ecx      ; mgr+0x5C = 尾
0040d198  895060            mov   dword ptr [eax + 0x60], edx      ; mgr+0x60 = 计数（先 inc）
```
⇒ **静态兜底（无需钩子）**：`H = *(u32*)( *(u32*)(mgr + 0x58) )`（链头，节点 = `H+0x116C`，节点 `+0` = H）。⚠️ `FUN_0040CFB0` 有多个调用点（其余传其它脚本名，见 §3.4 的按名 spawn），故「链头 = main」只在 main 最先创建时成立 —— **推荐仍用捕获点**。

### 5.4 「近零成本」的第二条捕获路径

在解释器命中点 `0x44FB9F`（Q5）处，**`H = *(u32*)(Esi + 0x1014)`**，再用 `*(u32*)(H + 0x1480) == 1296` 过滤主线（见 §6.3）即可 latch 住 H —— **与 thprac 既有 `mTimeLock` 钩子同一处**，不需要新增钩子点。两条路径可互为备份。

---

## 5A. boss 宿主可达性（Q-A / Q-B / Q-C）—— 对「boss 侧前置缺口」的收口

> 触发：`docs/logger/logger_progress.md` §4 第 11 条 —— th10–th13 的 boss 行要跑起来，需先补「boss 宿主 / 线程链」的逆向锚点（四作 Q4 给的都只是**主线宿主**捕获点，而 boss 是**另一个 ECL 宿主**，由 `enmCreate` 另起）。
> 三问：**Q-A** 宿主链表 `mgr+0x58` 是否覆盖 boss 宿主？**Q-B** boss 宿主的捕获点在哪？**Q-C** boss 宿主的 `res` 里有没有 boss sub？
> 结论：**Q-A 缺口消解（该链是全宿主链、含 boss）；Q-B 给出捕获点；Q-C 确认 boss 与主线共用同一份 `res` / 同一张 sub 表。**
> 状态：**🟡 静态定案（未真机）**。方法：`pefile` + `capstone` 只读反汇编 + 全程序 `E8`/imm32 扫描 + 离线 SCPT sub 表解析 + 对照 `.decl`；**未跑游戏、未跑编译、未改任何字节**。
> 地址口径：`VA` 与 `RVA = VA − 0x400000` 并列。

### 5A.0 三问速览

| 问 | 结论 | 一句话 |
| --- | --- | --- |
| **Q-A 宿主链表覆盖 boss 宿主？** | ✅🟡 **覆盖（缺口消解）** | `mgr+0x58` 是**全宿主链**：全程序只有**一个**宿主工厂 `FUN_0040CFB0`，它把**每一个**宿主（main / 中boss / 关底boss / 每个杂兵）**无条件**挂上这条链 ⇒ **不需要新捕获点也能拿到 boss 宿主**，但要**遍历**（链头只是最先建的那个宿主）。 |
| **Q-B boss 宿主捕获点在哪？** | ✅🟡 **`enmCreate*` 扩展 opcode 内的工厂调用点** | boss / 中boss 宿主由 `enmCreateA("Boss"/"MBoss", …)`（opcode **`0x101`**）创建 ⇒ 工厂调用点 **`0x40ED11`**，**EHOOK 落点 `0x40ED16`（RVA `0xED16`）**，命中时 **`EAX` = 新宿主 H**。全变体另见 `0x40EAEF` / `0x40EC0D`（练习菜单按索引 spawn 在 `0x40A6D3`）。 |
| **Q-C boss 宿主的 `res` 有 boss sub？** | ✅🟡 **有** | `FUN_0040D830` 里 `H+0x102C = *(u32*)(mgr+0x54)`（**全局** res）⇒ 所有宿主**共用一份 `res`**；th10 **无独立 boss 文件**（7 关 `ecli` 只有 `default.ecl`），boss sub（`Boss`/`Boss1`/`MBoss`/`BossCard1`…）与 `main` **同在 `stageNN.ecl`** ⇒ 同表 ⇒ **「地址 → 名字」区间反查对 boss 行同样成立**。 |

### 5A.1 Q-A —— `mgr+0x58` 是**全宿主链**（含 boss）：缺口消解

**判据：全程序只有一个宿主工厂，且它无条件挂链。**

1. **宿主对象尺寸 `0x2518` 全程序只被 push 一次** —— `push 0x2518` @ **`0x40CFB4` / RVA `0xCFB4`**（在 `FUN_0040CFB0` 内）⇒ 不存在第二个宿主分配点。
2. **宿主构造 `FUN_0040D830` 全程序只有一个调用点** —— `0x40CFCE`（在工厂内）⇒ 每个宿主都经工厂构造。
3. **`0x40CFB0` 作为 imm32 在数据里零出现** ⇒ 无函数指针 / 跳表间接调用。

4. 工厂尾部的**无条件**链表插入（`FUN_0040CFB0`：`EAX` = mgr，`ECX` = 新宿主节点 `H+0x116C`，`EDI` = 0）：

```asm
0040d162  397858            cmp   dword ptr [eax + 0x58], edi   ; 链头 == 0 ?
0040d165  8d8d6c110000      lea   ecx, [ebp + 0x116c]           ; ★ 新节点 = H+0x116C
0040d16b  7505              jne   0x40d172
0040d16d  894858            mov   dword ptr [eax + 0x58], ecx   ; ★ 空链 → mgr+0x58 = 新节点（头）
0040d170  eb19              jmp   0x40d18b
0040d172  8b505c            mov   edx, dword ptr [eax + 0x5c]   ; 尾
0040d175  8b7204            mov   esi, dword ptr [edx + 4]
0040d178  3bf7              cmp   esi, edi
0040d17a  7409              je    0x40d185
0040d17c  897104            mov   dword ptr [ecx + 4], esi      ; 新节点 +4 = next
0040d17f  8b7204            mov   esi, dword ptr [edx + 4]
0040d182  894e08            mov   dword ptr [esi + 8], ecx      ; next +8 = 新节点（prev 回链）
0040d185  894a04            mov   dword ptr [edx + 4], ecx      ; 旧尾 +4 = 新节点
0040d188  895108            mov   dword ptr [ecx + 8], edx      ; 新节点 +8 = 旧尾
0040d18b  8b5060            mov   edx, dword ptr [eax + 0x60]
0040d18e  89485c            mov   dword ptr [eax + 0x5c], ecx   ; ★ mgr+0x5C = 尾 = 新节点
0040d191  8b4864            mov   ecx, dword ptr [eax + 0x64]
0040d194  42                inc   edx
0040d196  41                inc   ecx
0040d198  895060            mov   dword ptr [eax + 0x60], edx   ; 计数
0040d19b  894864            mov   dword ptr [eax + 0x64], ecx
0040d19e  8bc5              mov   eax, ebp                      ; ★ 返回 EAX = 新宿主 H
0040d1a2  c20800            ret   8
```

5. **节点自身初始化**（`FUN_0040D830` 内 @ `0x40D91D`）：

```asm
0040d91d  89b66c110000      mov   dword ptr [esi + 0x116c], esi   ; ★ 节点 +0 = H（自指针）
0040d923  899e70110000      mov   dword ptr [esi + 0x1170], ebx   ; 节点 +4 = next = 0
0040d929  899e74110000      mov   dword ptr [esi + 0x1174], ebx   ; 节点 +8 = prev = 0
```
⇒ 节点 = `{+0 = H, +4 = next, +8 = prev}`；头 = `mgr+0x58`、尾 = `mgr+0x5C`、计数 = `mgr+0x60` / `mgr+0x64`。

6. **`mgr+0x58` 的写入点全程序只有工厂内的两处**（`0x40D16D` 头、`0x40D18E` 尾）—— 其余 `mov [reg+0x58], …` 经逐处核实属于**别的对象**：`0x4187B6` / `0x4187E6` / `0x4189EB` 的 `eax = [0x491c10]`、`0x422C3D` 的 `eax = [0x477810]`（`GAME_MANAGER`）、`0x44CB74` / `0x44CE41` 的 `eax = [edx]`。⇒ **这条链不会被别处清空 / 改写**。
7. **boss 宿主走同一工厂**：boss / 中boss 由 `main` 里的 `enmCreateA("Boss"/"MBoss", …)` 创建（§5A.2），该指令由**主线宿主的线程**执行 ⇒ 命中 `FUN_0040CFB0` ⇒ 挂同一条链。
8. **mgr 每次进关 / 进练习菜单重建**（`FUN_0040D6B0` 的调用点 `0x417B33` 进关、`0x40A377` 练习菜单）⇒ 链随 mgr 重置，不会跨关残留脏节点。

**⇒ 结论（Q-A）**：`mgr+0x58` = **全宿主链（含 boss）**。但要注意：

- `H = *(u32*)(*(u32*)(mgr+0x58))`（§5.3 的静态兜底）取的是**链头 = 最先创建的宿主** —— 通常就是 `main`（`main` 在进关 `0x418299` / `0x418499` 先建，杂兵 / boss 都晚于它）⇒ **静态兜底只能拿主线**；
- 要拿 **boss 宿主** 必须**沿 `node+4` 遍历**：

```cpp
uint32_t node = *(u32*)(mgr + 0x58);            // 链头
while (node) {
    uint32_t H = *(u32*)node;                   // 节点 +0 = 宿主
    // ……对每个 H：遍历其线程链 H+0x1030，逐线程取 ip = *(u32*)(T+4)（§4.5）
    node = *(u32*)(node + 4);                   // 下一个宿主
}
```

### 5A.2 Q-B —— boss 宿主的捕获点

**工厂 `FUN_0040CFB0` 的 6 个调用点（全程序 `E8` 扫描，已确认无间接调用）**：

| # | 调用点 VA / RVA | 调用者 | 传入的名字 | **下一条指令（EHOOK 落点）** |
| --- | --- | --- | --- | --- |
| 1 | `0x418299` / `0x18299` | `FUN_00418190` 进关分支 1 | 静态 `"main"` @ `0x46D278` | `0x41829E` / `0x1829E`（§5.1） |
| 2 | `0x418499` / `0x18499` | `FUN_00418190` 进关分支 2 | 同上 | `0x41849E` / `0x1849E`（§5.1） |
| 3 | **`0x40EAEA` / `0xEAEA`** | `FUN_0040E770` 扩展 opcode case `0x10E` / `0x10F` | **`ins+0x14`**（ECL 指令内联串） | **`0x40EAEF` / `0xEAEF`** |
| 4 | **`0x40EC08` / `0xEC08`** | 同上 case `0x104` / `0x10B` | 同上 | **`0x40EC0D` / `0xEC0D`** |
| 5 | **`0x40ED11` / `0xED11`** | 同上 case `0x100` / `0x101` / `0x105` / `0x109` / `0x10A` / `0x10C` | 同上 | **`0x40ED16` / `0xED16`** |
| 6 | `0x40A6CE` / `0xA6CE` | `FUN_0040A450` 练习菜单 | `table[i].+0`（**按 sub 索引**取名） | `0x40A6D3` / `0xA6D3` |

**除 `"main"` 外没有任何静态名字串** —— 3/4/5 号的名字是**运行期指针**：`ins+0x14`（当前 ECL 指令的内联字符串参数，`ins = *(u32*)(T+4)`）；6 号是 `table[i].+0`（练习菜单选中的 sub 名）。

**opcode → `enmCreate*` 名**（离线统计 7 关全部 `.ecl` 的指令流，与 `.decl` 逐名计数**完全吻合**，合计 1774 条）：

| opcode | `.decl` 名 | 计数 | 工厂调用点 |
| --- | --- | --- | --- |
| `0x100` | `enmCreate`（短形） | 762 | `0x40ED11` |
| `0x101` | **`enmCreateA`** | 293 | `0x40ED11` |
| `0x104` | `enmCreateM` | 318 | `0x40EC08` |
| `0x105` | `enmCreateAM` | 141 | `0x40ED11` |
| `0x109` | `enmCreateF` | 29 | `0x40ED11` |
| `0x10A` | `enmCreateAF` | 60 | `0x40ED11` |
| `0x10B` | `enmCreateMF` | 29 | `0x40EC08` |
| `0x10C` | `enmCreateAMF` | 53 | `0x40ED11` |
| `0x10E` | `enmCreate`（长形） | 89 | `0x40EAEA` |

（`0x10F` 与 `0x10E` 同体（fall-through），`.ecl` 中未出现；`0x102` / `0x103` / `0x106` / `0x107` / `0x108` / `0x10D` / `0x110` / `0x111` **不建宿主** —— CFG 遍历确认到不了任何工厂调用点。）

**boss / 中boss 宿主就是这些 opcode 建的**（`.decl` 实证 + 原始 `.ecl` 字节）：

```text
stage01.decl:1948   enmCreateA("MBoss", -64.0f, -16.0f, 6000, 10000, 1);   // 中boss
stage01.decl:2036   enmCreateA("Boss",  -144.0f, -16.0f, 40, 10000, 1);    // 关底 boss
stage02.decl:2289 / stage07.decl:3747 / stage07.decl:3929  同形
```
对应原始指令（`stage01.ecl` @ **`0xBAB0`**，在 `sub main` 内）：
```text
0xBAB0: 12 11 00 00 | 01 01 | 30 00 | 00 00 | ff 06 | 00 00 00 00 | 08 00 00 00 | "Boss\0"+pad | 00 00 10 c3 | 00 00 80 c1 | 28 00 00 00 | 10 27 00 00 | 01 00 00 00
        ↑time=4370   ↑opcode 0x0101(=enmCreateA) ↑size=0x30            ↑ins[+0x10]=8(内联串区长度)  ↑名字串在 ins+0x14    -144.0f      -16.0f        40           10000         1
```
⇒ 这正是工厂调用点 5 的 `ins+0x14` 名字来源。（`ins[+0x10]` = **内联串区长度**（4 字节对齐）；**参数区起点 = `ins + 0x14 + ins[+0x10]`** —— 与 `FUN_0040E770` 内 `ins + 0x10 + ((ins[+0x10]+4)>>2)*4` 在本例（`ins[+0x10]=8`）下同为 `ins+0x1C`。）

**⇒ 结论（Q-B）**：

- **boss / 中boss 宿主捕获点 = `0x40ED16`（RVA `0xED16`）**（`enmCreateA` 走的落点；命中时 **`EAX` = 新宿主 H**）。
- 若要**覆盖全部 `enmCreate*` 变体**（`enmCreateM` / `enmCreateMF` 走 `0x40EC0D`，长形 `enmCreate` 走 `0x40EAEF`），需同时挂 **`0x40EAEF` / `0x40EC0D` / `0x40ED16`** 三处；练习菜单的按索引 spawn 另在 **`0x40A6D3`**。
- **EHOOK 覆盖长度**：`0x40EAEF` / `0x40EC0D` / `0x40ED16` 处下一条指令都是 `xor eax, eax`（`33 C0`，**2B**），紧随 `5F 5E 5B`（3 条 `pop`）⇒ 连续 **5B** 可用 ⇒ **2B 或 5B EHOOK** 均可；`0x40A6D3` 是 `mov dword ptr [ebp+0x30], 4`（`C7 45 30 04 00 00 00`，**7B**）。
- ⚠️ **取值时机**：落点处 `EAX` **随即被 `xor eax, eax` 清零** ⇒ EHOOK 必须在**执行该指令之前**取 `EAX`（thprac 的 `EHOOK` 语义即如此）。
- ⚠️ **闸门**：每个 `enmCreate*` case 开头都有 `if (*(u32*)(mgr+0x10) != 0) 直接返回`（`0x40E7DB` / `0x40E9E1` / `0x40EB10` / `0x40EC2F` 等）。`mgr+0x10` 正是 thprac 既有的 **`bossExists`** 判据（`thprac_th10.cpp:548`：`GetMemContent(ENEMY_MANAGER_PTR, 0x10)`）⇒ **同一时刻至多一个 boss 宿主**（已有 boss 时不再 spawn）。

### 5A.3 Q-C —— boss 宿主的 `res` 里**有** boss sub

**判据 1：所有宿主共用同一份全局 `res`。** `FUN_0040D830`（宿主构造）里：

```asm
0040d9f2  a104774700        mov   eax, dword ptr [0x477704]     ; mgr（全局）
0040d9f7  8b4c2410          mov   ecx, dword ptr [esp + 0x10]   ; 名字（栈参）
0040da0d  8b4054            mov   eax, dword ptr [eax + 0x54]   ; ★ res = *(u32*)(mgr+0x54)（全局）
0040da10  51                push  ecx
0040da11  89862c100000      mov   dword ptr [esi + 0x102c], eax ; ★ H+0x102C = res（同一份）
0040da17  e8542a0400        call  0x450470                      ; FindSubByName(res, 名字)
0040da1c  8b5604            mov   edx, dword ptr [esi + 4]      ; T
0040da1f  894204            mov   dword ptr [edx + 4], eax      ; T+4 = 该 sub 首指令
```
⇒ **main 宿主与 boss 宿主读的是同一个 `*(u32*)(mgr+0x54)`**（`mgr+0x54` 只在 `res` 构造时写一次，`0x40D2CD`）。若 boss 名不在该表内，`FindSubByName` 返回 0 ⇒ `T+4 = 0` ⇒ 宿主立刻空转；`enmCreateA("Boss")` 能正常跑，**反证** boss 名就在这张表里。

**判据 2：th10 无独立 boss 文件，boss sub 与 `main` 同文件。**

- 7 关 `.decl` 头部 `ecli` **一律只有** `{ "default.ecl"; }`（slot 1）⇒ **slot 0 = `stageNN.ecl`，slot 1 = `default.ecl`，没有 `stNNbs` / `stNNmbs` 之类的 boss 槽**。
- 离线解析 `stage01.ecl` 的 SCPT sub 表：**68 个名字**，其中 `Boss` @ `0x6D8`、`Boss1` @ `0x864`、`BossCard1` @ `0x218C`、`MBoss` @ `0x6D6C`、`MBossCard1` @ `0x7584`、`main` @ `0x9C80` —— **boss sub 与 `main` 同表**。
- 加载器 `FUN_00450220` **跨文件累加**（`count += n`，§3.2）⇒ slot 0 + slot 1 的 sub 都在同一张 `res+0x8C` 表里。

**⇒ 结论（Q-C）**：boss 宿主的 `res` = 主线宿主的 `res` = 全局 `*(u32*)(mgr+0x54)`；`res+0x8C` 表**同时含 boss sub 名** ⇒ §2.3 的「地址 → 名字」区间反查（`table[i].+4 + 0x10`）**对 boss 行同样成立**（boss 宿主的 `T+4` 落在同一张表的区间内）。

### 5A.4 对实现的影响（**一句话**）

**th10 的 boss 行不需要「新的 boss 宿主捕获点」也能实现**：把 §10.2 的读取链从「只读链头宿主」改成「**遍历 `mgr+0x58` 全宿主链**（§5A.1），逐宿主遍历其线程链 `H+0x1030`，逐线程取 `ip = *(u32*)(T+4)`」，再按 §2.3 反查名字、走**同一张白名单**即可 —— boss sub 名（`Boss1` / `MBossCard1` / …）与章节 sub 名（`main` / `MainSubNN`）**不冲突**，杂兵 sub 名（`Girl00` …）不在白名单内 ⇒ 自动 no-op（latch 语义，§10.1）。

- **捕获点 `0x40ED16`**（及 `0x40EAEF` / `0x40EC0D`）是**可选加速**（「boss 宿主 latch」，与 §5.4 的 H latch 同风格），**非必需**。
- **过渡 sub 风险**：boss 宿主刚建立时其 `T+4` 停在**容器 sub**（`Boss` / `MBoss`）的首指令上 —— 容器名 `MBoss` **本身就在白名单里**（= 中boss 相位，正是 `THPatch` 的 warp 目标），故**不是**误判；未在白名单的容器名（如裸 `Boss`）则 latch no-op。**两种情形都无需特判**。
- **中boss 窗口**（§3.4 的 5 处撞点）仍需 th16 那套「只缓存」机制，但「中boss 存活」可**直接读 `*(u32*)(mgr+0x10)`**（thprac 既有 `bossExists`，§5A.2）—— **不必**从 boss 宿主侧采集（这是对 `th10_checkpoint_ip_offset.md` §5.1 原表述的**订正**）。

---

## 6. Q5 —— 解释器命中点 `0x44FB9F` 与寄存器语义

### 6.1 命中点与指令语义

```asm
0044fb77  8b4604            mov   eax, dword ptr [esi + 4]     ; eax = T+4 = 当前指令
0044fb7a  0fb74806          movzx ecx, word ptr [eax + 6]       ; ins[+0x06] = size
0044fb7e  8d5e04            lea   ebx, [esi + 4]
0044fb81  03c8              add   ecx, eax
0044fb83  890b              mov   dword ptr [ebx], ecx         ; T+4 += ins.size（推进到下一条）
0044fb85  8b13              mov   edx, dword ptr [ebx]
0044fb87  db02              fild  dword ptr [edx]               ; 下一条指令的「时间」
0044fb89  d81e              fcomp dword ptr [esi]               ; 与 T+0 比较
0044fb8b  dfe0              fnstsw ax
0044fb8d  f6c441            test  ah, 0x41
0044fb90  0f8b4ae6ffff      jnp   0x44e1e0                    ; 时间已到 → 继续分发
0044fb96  d984241c010000    fld   dword ptr [esp + 0x11c]     ; 本帧时间增量
0044fb9d  d806              fadd  dword ptr [esi]
0044fb9f  d91e              fstp  dword ptr [esi]               ; ★★ EHOOK 命中点：T+0 += 增量
0044fba1  5f                pop   edi
0044fba4  33c0              xor   eax, eax                    ; 返回 0 = 本帧到此为止
0044fbad  c20400            ret   4
```
- **地址**：`0x44FB9F` / RVA `0x4FB9F`；**覆盖 2 字节**（`D9 1E` = `fstp dword ptr [esi]`）⇒ 与 thprac 的 `EHOOK_HK(0x44fb9f, 2, ...)` 一致（hook 内 `pCtx->Eip = 0x44fba1` 正是这条指令之后）。
- **命中寄存器**：**`Esi` = 执行对象 T**（脚本上下文，= `H+8`）；`*(f32*)Esi` = `T+0` = **当前 ECL 时间**；`*(u32*)(Esi+0x1014)` = **宿主 H**。
- **语义**：主解释器的**「等待」收尾** —— 下一条指令时间未到 → 把本帧时间增量累加进 `T+0` 并返回 `0`。

### 6.2 所在函数与调用链（谁驱动、每帧几次）

- **函数**：`FUN_0044E1A0`，入口 `0x44E1A0`（RVA `0x4E1A0`），函数体 `0x44E1A0`–`0x44FBAD`（`0x44FBB0` 起是「线程结束」出口）。
- **寄存器约定**：EAX = 执行对象 T（非标准 `__fastcall`；th10 大量使用 EAX/ESI 传 `this`）。
- **opcode 分发**：`0x44E1F1` 取 `(i16)[ins+4]`；`> 0x57` → `0x44FB68`（对 `[T+0x1014]` 的**虚调用**，即宿主 vtable slot 0 = 扩展 opcode 处理器蹦床 `0x40E760`）；`≤ 0x57` → 经字节映射 `0x44FCB4` → 跳表 `0x44FBC4`。映射核实：**`0x0A`(ret) → `0x44E20C`；`0x0B`(call) → `0x44E35C`；`0x0C`(goto) → `0x44E40F`**（与 th13 的 `0x0A/0x0B/0x0C` 编号一致）。
- **驱动链**：
  ```asm
  0040d750  56                push  esi
  0040d751  8b7758            mov   esi, dword ptr [edi + 0x58]   ; mgr+0x58 = 宿主链表头
  0040d760  8b06              mov   eax, dword ptr [esi]          ; 节点 +0 = H
  0040d762  f7808024000000000200  test dword ptr [eax + 0x2480], 0x20000
  0040d76c  8b5e04            mov   ebx, dword ptr [esi + 4]      ; 下一个节点
  0040d771  053c100000        add   eax, 0x103c                   ; H+0x103C
  0040d776  50                push  eax
  0040d777  e804050000        call  0x40dc80                      ; → FUN_0040DC80（跑该宿主）
  ```
  ```asm
  0040dc80  ...                ; FUN_0040DC80（参数 = H+0x103C）
  0040e0f7  8bc2              mov   eax, edx
  0040e0fe  e80d1c0400        call  0x44fd10                     ; → 线程驱动器（EDI = H）
  0040e205  e8061b0400        call  0x44fd10                     ; （第二条路径）
  ```
  ```asm
  0044fd10  53                push  ebx
  0044fd13  8db730100000      lea   esi, [edi + 0x1030]           ; ★ 宿主线程链表头（edi = H）
  0044fd22  8b06              mov   eax, dword ptr [esi]          ; eax = T
  0044fd26  8b5e04            mov   ebx, dword ptr [esi + 4]      ; 下一个节点
  0044fd29  894704            mov   dword ptr [edi + 4], eax      ; ★ H+4 = T
  0044fd33  e868e4ffff        call  0x44e1a0                      ; ★★ 跑一帧 ECL VM（EAX = T）
  0044fd8c  8d4708            lea   eax, [edi + 8]
  0044fd90  894704            mov   dword ptr [edi + 4], eax      ; H+4 = H+8（复位）
  ```
  ⇒ 完整链：**（帧循环）→ `FUN_0040D750(H=mgr)` → 逐宿主 `FUN_0040DC80` → `FUN_0044FD10(H)` → 逐线程 `FUN_0044E1A0(T)`**。
- **每帧几次**：
  - `FUN_0044E1A0` **每帧每活跃线程调用 1 次**（`FUN_0044FD10` 遍历线程链表，链表含主线 + 所有 `async` 线程）。
  - 单次调用内部会**连续执行多条指令**（`0x44E1E0` 分发循环 → 每条执行后回到 `0x44FB77`，若下一条时间已到则 `jnp 0x44E1E0` 继续），直到「时间未到」或「线程结束」。
  - **`0x44FB9F` 每次调用至多命中 1 次**（只在「时间未到 → 累加时间」这条收尾路径上）；**线程结束时走 `0x44FBB0`，不命中**。
- **返回值**：`0` = 本帧到此为止（已把 `[esp+0x11C]` 的帧增量加到 `T+0`）；`−1` = 脚本结束（`T+4` 置 0）。

### 6.3 「主线判据」的准确含义（**订正 requests §1 的表述**）

requests §1 写的 `GetMemContent(pCtx->Esi + 0x1014, 0x103c + 0x1444) == 1296` 展开为：

```text
*(u32*)( *(u32*)(Esi + 0x1014) + 0x1480 )        // Esi = T ⇒ *(u32*)(T+0x1014) = H
= *(u32*)( H + 0x1480 )                          // 0x103c + 0x1444 = 0x1480
= *(u32*)( (H + 0x103C) + 0x1444 )
```

`H+0x103C` 是宿主内的**脚本信息子对象**（`FUN_0040DC80` 的参数就是 `H+0x103C` 本身），`+0x1444`（= `H+0x1480`）是它的**标志位字段**：

```asm
0040dc80  83ec18            sub   esp, 0x18
0040dc84  8b6c2420          mov   ebp, dword ptr [esp + 0x20]   ; = H+0x103C
0040dc88  8b8544140000      mov   eax, dword ptr [ebp + 0x1444] ; ★ = [H+0x1480]
0040dc8e  f6c404            test  ah, 4                          ; bit 0x400
0040dc91  7409              je    0x40dc9c
0040dc9c  0d00040000        or    eax, 0x400
0040dca3  898544140000      mov   dword ptr [ebp + 0x1444], eax
```
⇒ 该字段是**脚本角色/状态位掩码**，`1296 (0x510)` 是「主线（main）」这一角色的取值（thprac 既有经验值）。**thprac 的「只认 main」判据成立且可用**，只是它比较的是**宿主 H** 的字段，不是「执行对象 T」的字段。

### 6.4 作为「每帧稳定触发点」的评价（**重要**）

- ✅ 优点：**与 thprac 既有 `mTimeLock` 钩子同址**，无需新增钩子；命中时 `Esi` 就是执行对象，可直接 `H = *(u32*)(Esi+0x1014)`。
- ⚠️ 局限 1：**不是无条件每帧**。处于「纯等待、时间未到」时命中；**线程结束那一次不命中**。
- ⚠️ 局限 2：**对所有脚本对象命中**（主线 / 敌方 / boss / async 线程都会走到这里）⇒ 必须用 §6.3 的 H 过滤（`*(u32*)(H+0x1480) == 1296`）或 §5.1 捕获的 H 做身份比对。
- **推荐做法（与 th13 同风格）**：
  - **首选（零钩子）**：只在 `0x41829E`/`0x41849E` 抓一次 H，然后在 IGI 渲染（帧间稳定态）读 `T = *(u32*)(H+0x1030)`、`ip = *(u32*)(T+4)`。
  - **备选**：复用 `0x44FB9F` 钩子做「H latch」（`H = *(u32*)(Esi+0x1014)`，`*(u32*)(H+0x1480)==1296` 时置位），它**无需**找每帧主循环，但有「线程结束不命中」的时序缺口（H 一旦 latch 住即长期有效，缺口无影响）。

---

## 7. Q6 —— 运行期保留 sub 名

✅ **保留**。加载器 `FUN_00450220` 在 `0x4502CD` 把**名字区指针**（已加载 ECL 缓冲内的名字串首址）直接写入 `table[i].+0`，随后 `0x4502D5`–`0x4502DC` 逐字节扫 NUL 推进到下一个名字（见 §3.2）。游戏侧亦按 `table[i].+0` 做「按索引用名字 spawn」（§3.4 的 `0x40A6BD`）。故运行期 `name = *(char**)(table + i*8)` 直接可读，**无需离线名表**。

> ⚠️ **但本作需要一个「地址 → i」的反查**（因为 Q1 是地址而非索引），见 §2.3。反查只需 `table[i].+4 + 0x10`，**不需要离线 `.ecl`**；离线 `.ecl` 只用于「段落入口白名单」本身（本报告的离线解析见 §10.5）。
> ⚠️ 名字串位于**堆上的运行期 ECL 缓冲**内（`GetMemAddr(0x477704,0x54,0xC)` = `res+0xC` 那一级数组的缓冲），**不是** exe 内静态字符串 ⇒ 读 `char*` 时必须做「缓冲内/长度上限」保护。

---

## 8. Q7 —— 机体 / 子机 / 难度 / 关卡地址

| 名称 | VA / RVA | 出处 | 置信度 |
| --- | --- | --- | --- |
| `CHARA` | `0x474C68` / `0x74C68` | `thprac_th10.cpp:16`（IGI `OnContentUpdate` 里读 byte） | ✅ 地址（语义未复核） |
| `SUBSHOT` | `0x474C6C` / `0x74C6C` | `thprac_th10.cpp:17`（同上，`cur_player_type = CHARA*3 + SUBSHOT`） | ✅ 地址 |
| `DIFF` | `0x474C74` / `0x74C74` | `thprac_th10.cpp:18` | ✅ 地址 |
| `STAGE_NUM` | `0x474C7C` / `0x74C7C` | `thprac_th10.cpp:19`（既有 `mTimeLock` 钩子用它） | ✅ 地址 |
| `PLAYER_PTR` | `0x477834` / `0x37834` | `thprac_th10.cpp:21` | ✅ 地址 |
| `REPLAY_MGR` | `0x477838` / `0x37838` | requests §1（`docs/gametime/gametime_thxx_flags.md`） | 🟡 |
| `GAME_MANAGER` | `0x477810` / `0x37810` | requests §1 | 🟡 |
| `SOUND_MANAGER` | `0x492590` / `0x92590` | `thprac_th10.cpp:22` | 🟡 |

> 说明：本迭代 IGI 只需显示「当前段落」，Q7 非必需；上表**未经真机取值核对，勿直接用于判据**。

---

## 9. Q8 —— 练习 `ECLJump` 对读取的影响

### 9.1 结论

✅ **不影响 sub / ip 判读**（与 th13 §9 一致）。

### 9.2 依据

1. **补丁写入**（`thprac_th10.cpp:1294-1298`）：
   ```cpp
   void ECLJump(ECLHelper& ecl, unsigned int start, unsigned int dest, int at_frame, int ecl_time = 0)
   {
       ecl.SetPos(start);
       ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
   }
   ```
   6 个 dword = 24B（`size = 0x18`）：`+0`=`ecl_time`(int)、`+4`=`0x0018000C`（低 16 位 `0x000C` = **`goto`**，高 16 位 `0x0018` = size）、`+8`=`0x02FF0000`（`ins[0x0A]=0xFF` = 任意难度；`ins[8..9]=0` = 无浮点参数）、`+0xC`=0（局部槽数 0）、`+0x10`=`dest-start`、`+0x14`=`at_frame`。
2. **`goto` 语义**（case `0x0C` @ `0x44E40F`/RVA `0x4E40F`）：
   ```asm
   0044e40f  8b03              mov   eax, dword ptr [ebx]            ; 当前指令
   0044e411  db4014            fild  dword ptr [eax + 0x14]          ; ins[+0x14] = at_frame（int）
   0044e414  d91e              fstp  dword ptr [esi]                 ; ★ T+0 = at_frame（绝对赋值）
   0044e416  8b4810            mov   ecx, dword ptr [eax + 0x10]     ; ins[+0x10] = dest − start
   0044e419  e963170000        jmp   0x44fb81                        ; ★ → add ecx,eax; mov [ebx],ecx
   ```
   **只改 `T+0`（时间）与 `T+4`（ip），从不触碰 sub 表，也不触碰任何「当前 sub」旁路字段。**
3. **sub 结构未动**：`ECLHelper::SetBaseAddr` 定位的是 `res+0xC` 起的**运行期 ECL 文件缓冲**（§3.5）；`ECLJump` 只在硬编码偏移处覆写指令体，不触碰 SCPT 头、名字区、`res+0x8C` 表（表由 `FUN_00450220` 一次解析生成）。
4. **瞬态注意**：`goto` 之后、目标 sub 尚未执行的那一瞬，`T+4` 指向「跳转后位置」（仍是**旧 sub 内**的地址）；采样落在帧间稳定态即可规避。
5. **⚠️ 本作特有提醒**：th10 的 warp 目标偏移多数落在 `main` **内部**（见 §10.4/§10.5），所以「跳转后 sub 归属」在 st1–st3 上**仍是 `main`** —— 这不是 Q8 的问题，而是 Q9 的粒度问题。

---

## 10. Q9 —— 段落判定口径：**路过式 checkpoint（latch）**

### 10.1 结论

✅ **走「路过式 checkpoint（latch）」**（与总括 §6 Q9 一致）：读当前 sub（本作经 `NameOfAddress(T+4)` 得到）→ **∈ 白名单则置当前段落；不在 → no-op**（保持上一个值，不回退 `main`）；本局首次命中前显示 `main`。CSV 即白名单表。**每 checkpoint 登记 1 个「段落入口函数」**（含 `async` 落点）。

### 10.2 判定流程

```cpp
H = mEclHost;                                     // 0x41829E / 0x41849E 捕获
if (!H) return "main";
T = *(u32*)(H + 0x1030);                          // 主线内嵌线程（≡ *(u32*)(H+4)）
if (!T) return "main";
res   = *(u32*)(*(u32*)0x477704 + 0x54);          // 全局链（异步线程对象无 res 槽，见 §4.5）
count = *(i32*)(res + 8);
table = *(u32*)(res + 0x8C);
uint32_t ip = *(u32*)(T + 4);                     // 当前指令绝对地址（Q1）；0 = 线程已结束
const char* cur = NameOfAddress(table, count, ip);
const char* section = Whitelist(game, stage, cur); // 命中 → 置当前段落
if (section) current = section;                    // 未命中 → no-op（不回退 main）
```

### 10.3 「当前 sub 怎么取」小结

| 步 | 取法 | 证据 |
| --- | --- | --- |
| 1 | `H` = 主线宿主（捕获点 / 静态兜底 / 解释器钩子三选一） | §5.1 / §5.3 / §5.4 |
| 2 | `T = *(u32*)(H+0x1030)`（≡ `*(u32*)(H+4)`） | §5.2 / §6.2 |
| 3 | `ip = *(u32*)(T+4)`（**指令绝对地址**，非索引） | §2 |
| 4 | `res = *(u32*)(*(u32*)0x477704 + 0x54)` | §3.1 / §3.4 |
| 5 | `table = *(u32*)(res+0x8C)`、`count = *(i32*)(res+8)` | §3.2 |
| 6 | `ip` → `i`（区间表）→ `name = *(char**)(table+i*8)` | §2.3 |
| 7 | 白名单 latch | §10.1 |

### 10.4 ✅ 已收口项 A —— 段落入口白名单**已枚举完毕（2026-10-17）**

th10 的段落入口白名单**已枚举完毕**：`docs/logger/th10_checkpoints.csv` 第 1 列 **109/109**（章节 41/41、boss 68/68）。**入口表、口径与特例、实现影响**见新文档 **`docs/th10/th10_checkpoint_entry_report.md`**（逐行偏移的推导证据见 `docs/th10/th10_checkpoint_ip_offset.md`）。

其中 **st1–st3 的 15 行采用 ip 偏移记法**：格式 **`sub名+0x偏移`**（例 `main+0x28`），**偏移基准 = 相对该 sub 首条指令的字节偏移**（= sub 数据基址 `+0x10`），**与 th13 的 `MainSub08+0x390` 同一基准**。起因即 §10.5 所述：st1–st3 的道中段落是 `main` 的**内联区段**，sub 名粒度不够，须靠 ip 偏移区分。**用户已定案**：统一用此第 1 列记法，**不新增第 5 列**。

> 运行期读取链（`ip = *(u32*)(T+4)`、`base = *(u32*)(res+0xC+slot*4)`、`off = ip − base`）与实现影响（**双匹配、偏移命中优先**）、已知风险（**boss 跳转撞章节锚点 5 处**）见新文档 §3。

### 10.5 ✅ 已定案项 B —— **th10 的「入口」不止是 sub 名**（2026-10-17 定案）

本轮顺带把 `THStageWarp` 的全部补丁偏移**离线映射回 sub**（自写脚本离线解析各关 `.ecl` 的 SCPT 头 —— 与 §3.2 的加载器同一格式：魔数 `0x54504353`、`hdr = u16@[6]`、`n = u16@[0x10]`、偏移数组 = `buf+0x24+hdr`（`n` 个 u32）、名字区紧随其后），结果：

| 关 | warp 补丁 `start` 落在哪个 sub | 目标 `dest` 落在哪个 sub |
| --- | --- | --- |
| **st1** | **`main`**（+0x38） | **`main`**（+0x2C0 / +0x938 / +0x12F4 / +0x1358 / +0x1570） |
| **st2** | **`main`**（+0x6C） | **`main`**（+0x384 / +0x8EC / +0x930 / +0x145C） |
| **st3** | **`main`**（+0x6C） | **`main`**（+0x154 … +0x17E4） |
| st4 | `MainSub1` / `MainSub2` | `MainSub1` / `MainSub2` / `MainSub4` / `MainSub5` / `MainSub7` |
| st5 | `MainSub1` / `MainSubMBoss` | `MainSub2` / `MainSub3` / `MainSub7` / `MainSub8` |
| st6 | `MainSub1` / `MainSubBoss` **与 `main`** | `MainSub2/3/5`、`MainSubBoss`、**`main`** |
| st7 | `MainSub7` / `MainSubBoss` | 同左 |

配合 `.decl` 的结构（`../exes/ecl_dump/th10/stage01.decl:1776` 起）：

- **st1 的 `main` 是一个覆盖全关的超长脚本**（约 0x1EB4 B），道中各段是它的**内联区段**（section01 中没有 `MainSub01…`，只有一个 `MainSub` 由 `@MainSub() async` 派生）；`THStageWarp` 在 `main` 内部写 `goto` 跳过区段 ⇒ **st1 的道中段落全部属于同一个 sub `main`**。
- **st2+** 有一批独立的 `MainSub01/MainSub02/…/MainSubNN` 子脚本；`main` 通常只做 `@MainSubNN() async` 派生 + 等 boss（`stage04.decl:2875-2897`、`stage05.decl` 的 `main` 可见）；这些 `MainSubNN` 跑在**同宿主的不同（async）线程**里（§4.5）。
- **st6 兼有两类**：有 `MainSubN` 型入口，也有补丁直接落在 `main` 内部的（`0x1366C → 各种 main 内偏移`）。

⇒ **结论（✅ 已定案，2026-10-17）**：
1. th10 的「段落入口」**不能一律用「一个 sub 名」表示** —— st1/st2/st3 的道中段落是 `main` 的**内联区段**，其区分靠 **ip 偏移**（正好对应 `THStageWarp` 的硬编码偏移）；
2. st4–st7 的多数段落是**独立 sub**（且跑在 async 线程上）⇒ 这些关走「1 checkpoint = 1 入口 sub」；
3. **定案**：采用 **(sub 名, ip 偏移)** 二级粒度，**仅 st1–st3 的 15 行**使用，记法 **`sub名+0x偏移`**（偏移 = 相对该 sub **首条指令**的字节偏移，例 `main+0x28`）；另 3 行同名冲突（`ST4 前半 #2`、`ST5 后半 #1/#2`）同法消歧。其余各作与其余段落均为纯 sub 名。**未采用**「统一改为文件偏移映射」的替代方案（保留 sub 名可读性）。
4. 「当前 sub 是哪一个线程的」在本作也需明确（主线线程 vs async 线程，§4.5）——**仍未定案，属实现（E5）范畴**。

> 白名单已于 2026-10-17 枚举完毕（CSV 第 1 列 **109/109**），逐行入口见 `docs/th10/th10_checkpoint_entry_report.md`，偏移方案见 `docs/th10/th10_checkpoint_ip_offset.md`。**🟡 未真机。**

---

## 11. 对总括 §5 与 `th10_ecl_re_requests` 的回填建议

> 本报告**未修改** `docs/logger/**` 下任何文件（避免与并行任务写冲突）。建议由本迭代的收口任务按下列内容回填。

### 11.1 总括 `docs/logger/logger_re_disasm_overview.md` §5.1 的 th10 行

| 列 | 建议填入 |
| --- | --- |
| 已有 ECL 解释器命中点（寄存器） | `0x44FB9F`（2B，`Esi` = 执行对象 T）；函数 `FUN_0044E1A0`；**旁证**：扩展 opcode 处理器 `0x40E760` 读同一个 `T+4` |
| 命中点已知字段 | `*(f32*)(T+0)` = 当前 ECL 时间；**`*(u32*)(T+4)` = 当前指令地址（不是索引）**；`*(u32*)(T+0x1014)` = 宿主 H |
| ECL 数据区链 | `GetMemAddr(0x477704, 0x54, 0xC)` = `res + 0xC`（`res = *(*(0x477704)+0x54)`）——**原值正确，可直接用** |
| sub 名/索引线索 | ✅ `res+0x8C` 表（项 8B，`+0`=名字 `char*`，`+4`=起始，首指令 `+4+0x10`）；`count = res+8` |
| （新增列或注释）主线宿主捕获 | `0x41829E` / `0x41849E`（`EAX` = H）；静态兜底 `H = *(u32*)(*(u32*)(mgr+0x58))` |

并更新注记：**`[执行对象+4]` 语义矛盾**（总括 §5.1 注记 / §9 风险 4）应改为：**th10 / th13 = 指令指针（B 代，与 th14 同类）；th15/16/17/18 = 索引（C 代）**。两者不可互搬 —— th10 与 th13 已两份报告互相印证。

### 11.2 总括 §5.2 接线锚点表 th10 行

保持现值即可（`th10_life_dec 0x426A1C`(6B) / `th10_bomb_dec 0x4259CF`(5B) / `th10_bomb_dec2 0x425C3E`(5B) / `th10_game_start 0x41798C`(6B) 本轮**未触及、未复核**）。**新增建议**：把「主线宿主捕获」补进 §5.2 或新增一列 —— `th10` 捕获点 `0x41829E` / `0x41849E`（`EAX` = 宿主）。

### 11.3 `docs/th10/th10_ecl_re_requests.md`

| 章节 | 建议 |
| --- | --- |
| §1 表格「ECL 解释器命中点」 | 补齐：所在函数 `FUN_0044E1A0`；`Esi` = 执行对象 T；命中指令语义 = `T+0 += 帧增量`（**不是** sub 索引写入点）。 |
| §1 表格「主线判据」 | 展开为 `*(u32*)(H + 0x1480)`（`H = *(u32*)(Esi+0x1014)`；`0x103c+0x1444` 是「宿主脚本信息子对象 + 标志位」），并注明 1296 = `0x510` = main 的角色掩码。 |
| §1 表格新增行 | 「ECL 主解释器 = `FUN_0044E1A0` `0x44E1A0`」；「主线宿主捕获 = `0x41829E`/`0x41849E` `EAX`」；「sub 表 = `res+0x8C`（`res = *(*(0x477704)+0x54)`）」；「`"main"` @ `0x46D278`」。 |
| §2 Q1 | 答：**指令指针**（非索引），`ip = *(u32*)(T+4)`；写入点见报告 §2.2；**与 th13 同构**。 |
| §3 Q2 | 答：`res = *(*(0x477704)+0x54)`；`table = res+0x8C`；项 8B；`+0`=名字、`+4`=起始、首指令 `+4+0x10`；加载器 `FUN_00450220`，查表 `FUN_00450470`；`THSectionPatch` 的 `SetBaseAddr` 一级对象 = res。 |
| §4 Q3 | 答：**有**显式栈；基址 `T+8`，SP `T+0x1008`，**帧长 `0x08`**，帧 `+0x04` = 调用者 ip（⚠️ 与 th13 的 `0x0C` 不同）。 |
| §5 Q4 | 答：`FUN_0040CFB0` 返回宿主 H；站点 `0x41829E`/`0x41849E`（`"main"`@`0x46D278`）；静态兜底 `*(u32*)(*(u32*)(mgr+0x58))`。 |
| §6 Q5 | 答：`0x44FB9F`（2B EHOOK），`Esi` = 执行对象；函数 `FUN_0044E1A0`；跳表 `0x44FBC4`/映射 `0x44FCB4`；**不是无条件每帧**。 |
| §7 Q6 | 答：**运行期保留名字**；因是地址语义，需 `table[i].+4+0x10` 建区间表反查。 |
| §8 Q7 | 答：沿用既有 `CHARA=0x474C68` / `SUBSHOT=0x474C6C` / `DIFF=0x474C74` / `STAGE_NUM=0x474C7C`。 |
| §9 Q8/Q9 | 答：Q8 不影响；**Q9 = 路过式 checkpoint（latch）**；⚠️ 追加 §10.4/§10.5 的两条未定论（白名单待枚举；st1–st3 的入口是 `main` 内联区段，粒度可能需 `(sub, ip 偏移)`）。 |
| §10 真机验证清单 | 追加本报告 §13 的条目。 |
| §13 变更记录 | 追加一行（见本报告 §14）。 |

---

## 12. 推荐实现摘要（供接线）

```cpp
// ── 方案 A（首选，零新增钩子）：捕获主线宿主 ──────────────────────────
EHOOK_DY(th10_ecl_host,  0x41829E, 6, { mEclHost = pCtx->Eax; self->Disable(); });  // 6B 覆盖
EHOOK_DY(th10_ecl_host2, 0x41849E, 6, { mEclHost = pCtx->Eax; self->Disable(); });
// 清理：th10_game_start(0x41798C) 里 mEclHost = nullptr;（沿用既有 EHOOK）

// ── 方案 B（复用既有 mTimeLock 钩子做 H latch，零新增钩子点）──────────
// 在既有 EHOOK_HK(0x44fb9f, 2, ...) 内追加：
//   uint32_t H = GetMemContent(pCtx->Esi + 0x1014);
//   if (H && GetMemContent(H + 0x1480) == 1296) mEclHost = H;      // 主线角色掩码

// ── 每帧（IGI 渲染时，帧间稳定态）读取「当前段落」────────────────────
if (!mEclHost) { section = "main"; }
else {
    auto mgr   = *(uint32_t*)0x477704;              // 脚本管理器
    auto res   = *(uint32_t*)(mgr + 0x54);           // ECL 资源（全局链，异步线程亦适用）
    auto count = *(int32_t*)(res + 8);
    auto table = *(uint32_t*)(res + 0x8C);
    auto T     = *(uint32_t*)(mEclHost + 0x1030);    // ≡ *(uint32_t*)(mEclHost + 4)
    if (!res || !table || count <= 0 || !T) { section = "main"; }
    else {
        uint32_t ip = *(uint32_t*)(T + 4);           // 当前指令绝对地址；0 = 线程已结束
        // ip → sub（§2.3 区间表）→ 白名单；命中则置当前段落，未命中 no-op（不回退 main）
        // ⚠️ st1–st3 的道中段落在 main 内部，若白名单只列 sub 名将全部落到 "main"（见 §10.5）
    }
}
```

**EHOOK 覆盖长度**：`0x41829E` / `0x41849E` 处下一条指令 = `mov edi, dword ptr [0x47770c]`（`8B 3D ...`，**6B**）⇒ **6B EHOOK** 合适。

---

## 13. 真机验证清单

> 本机**未做**任何真机验证；以下**全部未做**；状态一律为「静态定案（🟡 已定案、未真机三态）」。

- [ ] **宿主捕获**：在 `0x41829E`/`0x41849E` 抓 `EAX` 非空；进关后 `*(u32*)(H+0x1030)`、`*(u32*)(H+0x102C)` 均可解引用，且 `*(u32*)(H+0x102C) == *(u32*)(*(u32*)0x477704 + 0x54)`。
- [ ] **Q1 地址语义**：道中/boss/不同段落各采样一次，`NameOfAddress(T+4)` 与画面进度一致；`T+4 == 0` 只在线程结束（过关/退关）时出现。
- [ ] **地址→sub 反查**：同一段落内 `T+4` 应始终落在**同一个** `[start_i, start_{i+1})` 区间内；跨段落时切换。
- [ ] **Q3 栈**：`SP = *(u32*)(T+0x1008)` 在指令边界处为 `0x08` 的整数倍；`*(u32*)(T+8+SP-4)` 落在某个 sub 区间内；帧长确为 **`0x08`**（对照 th13 的 `0x0C`）。
- [ ] **Q4 判据**：`*(u32*)(H+0x1480) == 1296` 仅在主线宿主成立；打断点核对敌方/异步宿主的取值不同。
- [ ] **Q5 时序**：确认 `0x44FB9F` 每帧每活跃线程至多命中 1 次；线程结束那一帧不命中。
- [ ] **多线程（重要）**：含 `@MainSubNN() async` 的关卡（st4/st5/st6/st7），确认 `*(u32*)(H+0x1030)` 只给出主线线程，async 线程挂在 `*(u32*)(H+0x1034)` 起的链表上；确认 async 线程对象的 `T+0x1024` 越界（不可读 res）。
- [ ] **Q9 粒度（本作最关键）**：st1/st2/st3 确认道中段落确实全部落在 sub `main` 内（即 `T+4` 始终在 `main` 的 `[start, end)` 区间内），从而验证「必须用 ip 偏移区分段落」这一 🟡 推论；st4–st7 确认段落落在 `MainSubNN` 上。
- [ ] **练习跳章**：跳章后 `T+4` 与其 sub 归属正确（Q8 复核）；`ins[0x0A]=0xFF` 的难度掩码在 E/N/H/L 四难度下均能执行（否则 `goto` 会被 `test byte [T+0x101C], cl` 跳过 —— **这也是最需要真机确认的一条**）。
- [ ] **采样时机**：置于帧间稳定态（IGI 渲染），避免局部/表达式栈占用 `T+0x100C` 时误读栈。
- [ ] **attract demo / 标题挂机**：th10 无剧情分支（requests §10），确认标题挂机时 `H` 为空或 `*(u32*)(H+0x1480) != 1296`，不会误显示段落。
- [ ] **变体**：Steam / 汉化版是否共用同一 RVA（本报告地址来自原版 `ts=1186086096`）。
- [ ] **§5A boss 宿主可达性（新增）**：① 进 boss 后沿 `mgr+0x58` → `node+4` 遍历，确认链上**不止一个**宿主，且存在一个宿主的 `*(u32*)(T+4)` 落在 `Boss*` / `MBoss*` 区间内（用 `NameOfAddress` 反查）；② 确认该 boss 宿主的 `H+0x102C == *(u32*)(*(u32*)0x477704 + 0x54)`（与主线宿主**同一份 `res`**）；③ 可选：在 `0x40ED16`（及 `0x40EAEF`/`0x40EC0D`）挂 2B EHOOK，确认 `EAX` = 新宿主、且该宿主随后出现在 `mgr+0x58` 链上；④ 确认 `*(u32*)(mgr+0x10)`（`bossExists`）在中boss / 关底 boss 存活期间非 0。

---

## 14. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | 建档（本报告）。静态定案 Q1–Q9（🟡 未真机三态，本机 Windows 11）：**Q1** `[T+4]` = **当前指令绝对地址（不是 sub 索引）**，与 th13 同构；写入点 `0x40DA1F`（FindSubByName 返回值）/`0x44FB83`（逐指令推进）/`0x44E238`（ret）/`0x44E40F`（goto）/`0x44FBB0`（线程结束）。**Q2** `mgr=*(u32*)0x477704` → `res=*(mgr+0x54)` → `count=res+8` → `table=res+0x8C`（项 8B，`+0`=名字 `char*`，`+4`=数据基址，首指令 `+4+0x10`），加载器 `FUN_00450220`（SCPT `0x54504353`），按名查表 `FUN_00450470`，游戏侧交叉印证 `0x40A6AF`/`0x40A381`；`GetMemAddr(0x477704,0x54,0xC)` = `res+0xC`。**Q3** 显式调用栈（基址 `T+8`、SP `T+0x1008`、**帧长 `0x08`**、帧 `+0x04`=调用者 ip，压帧 `FUN_0044DF70`，弹帧 `ret`@`0x44E20C`）；⚠️ 帧长与 th13（`0x0C`）不同。**Q4** 宿主工厂 `FUN_0040CFB0`（`malloc 0x2518`，`EAX` 返回宿主），站点 `0x418299`/`0x418499`（EHOOK 落点 `0x41829E`/`0x41849E`，`"main"`@`0x46D278`），宿主构造 `FUN_0040D830`，宿主链节点 `H+0x116C` 挂 `mgr+0x58`，静态兜底 `*(u32*)(*(u32*)(mgr+0x58))`。**Q5** 命中点 `0x44FB9F`（2B，`Esi`=执行对象 T，语义 `T+0 += 帧增量`），函数 `FUN_0044E1A0`（`0x44E1A0`–`0x44FBAD`），跳表 `0x44FBC4`/映射 `0x44FCB4`/`>0x57` 委派 `0x44FB68`，驱动链 `FUN_0040D750 → FUN_0040DC80 → FUN_0044FD10 → FUN_0044E1A0`（每帧每线程 1 次，命中非无条件）；旁证：扩展 opcode 处理器 `0x40E760` 读同一 `T+4`。**Q6** 运行期保留名字（需地址区间反查）。**Q7** 沿用既有 `CHARA=0x474C68`/`SUBSHOT=0x474C6C`/`DIFF=0x474C74`/`STAGE_NUM=0x474C7C`。**Q8** `ECLJump` 只写 `goto`(`0x0C`)，只改 `T+0`/`T+4`。**Q9** 路过式 checkpoint（latch）；**新增两条 🟡 未定论**：(A) 段落入口白名单待枚举；(B) **离线映射 `THStageWarp` 偏移发现 st1/st2/st3 的段落入口落在 `main` 内部（道中是 `main` 的内联区段），st4–st7 多为独立 `MainSubNN` 且跑在 async 线程上 ⇒ th10 的「入口」可能需 `(sub, ip 偏移)` 二级粒度**。**订正 requests §1**：主线判据展开为 `*(u32*)(H+0x1480)`（`H = *(u32*)(T+0x1014)`，是**宿主**的脚本信息标志位，不是执行对象的字段）。证据：`capstone` 只读反汇编（自写 `D:\workshop\reasm\th10\dl.py`）+ 全程序字节/交叉引用扫描 + 离线 SCPT sub 表解析（§10.5）。**未真机三态。** |
| 2026-10-17 | **§10.4 收口（白名单已枚举）**：「段落入口白名单尚未枚举」→「**已枚举完毕（2026-10-17）**」（`docs/logger/th10_checkpoints.csv` 第 1 列 **109/109**：章节 41/41、boss 68/68），指向新文档 `docs/th10/th10_checkpoint_entry_report.md`，并注明 **st1–st3 的 15 行采用 ip 偏移记法**（`sub名+0x偏移`，偏移基准 = 相对该 sub 首条指令的字节偏移，与 th13 的 `MainSub08+0x390` 同基准；**用户已定案不新增第 5 列**）。**只改本节**（§10.5 粒度分析与其余章节未动）。**🟡 未真机。** |
| 2026-10-18 | **新增 §5A「boss 宿主可达性（Q-A / Q-B / Q-C）」**（接在 §5/Q4 之后），收口 `logger_progress.md` §4 第 11 条记的「boss 侧前置缺口」。**Q-A（缺口消解，🟡）**：`mgr+0x58` 是**全宿主链（含 boss）** —— 全程序只有 1 个宿主工厂 `FUN_0040CFB0`（`push 0x2518` 仅 @ `0x40CFB4`；宿主构造 `FUN_0040D830` 仅 1 个调用点 `0x40CFCE`；`0x40CFB0` 无间接调用），工厂尾部 `0x40D162`–`0x40D1A2` **无条件**挂链（头 `mgr+0x58`/尾 `mgr+0x5C`/计数 `mgr+0x60`、`0x64`，节点 `H+0x116C` = `{+0=H,+4=next,+8=prev}`，`0x40D91D` 初始化），且 `mgr+0x58` 全程序只被这两处写（其余同形写入经核实属 `0x491c10`/`GAME_MANAGER`/其它对象）⇒ **不需要新捕获点**；但链头只是**最先建的宿主**（通常 `main`）⇒ 要拿 boss 宿主**须沿 `node+4` 遍历**；mgr 每关重建（`FUN_0040D6B0` @ `0x417B33`/`0x40A377`）⇒ 链随关重置。**Q-B（给出捕获点，🟡）**：枚举工厂全部 **6 个**调用点（`0x418299`/`0x418499` = `"main"`；`0x40EAEA`/`0x40EC08`/`0x40ED11` = 扩展 opcode 处理器 `FUN_0040E770` 内的 `enmCreate*`；`0x40A6CE` = 练习菜单按索引），名字除 `"main"` @ `0x46D278` 外均为**运行期串**（`ins+0x14` 内联 / `table[i].+0`）；opcode→名 逐名计数吻合（`0x101`=`enmCreateA`、`0x104`=`enmCreateM`、`0x105`=`enmCreateAM`、`0x109`/`0x10A`/`0x10B`/`0x10C`=`F`/`AF`/`MF`/`AMF`、`0x100`/`0x10E`=`enmCreate` 短/长形），**boss/中boss 捕获点 = `0x40ED16`（RVA `0xED16`，`EAX` = 新宿主 H）**，全变体另见 `0x40EAEF`/`0x40EC0D`（落点均为 2B `xor eax,eax`，连续 5B 可用；`0x40A6D3` 7B）；每 case 开头有 `mgr+0x10 != 0` 闸门 = thprac 既有 **`bossExists`**（`thprac_th10.cpp:548`）。**Q-C（有 boss sub，🟡）**：`FUN_0040D830` 里 `H+0x102C = *(u32*)(mgr+0x54)`（**全局** res）⇒ 所有宿主共用一份 `res`；7 关 `ecli` 只有 `default.ecl`（slot 1）⇒ **无独立 boss 文件 / boss 槽**，boss sub（`Boss`@`0x6D8`/`Boss1`@`0x864`/`MBoss`@`0x6D6C`/`MBossCard1`@`0x7584`…）与 `main`@`0x9C80` 同在 `stageNN.ecl` ⇒ 同表 ⇒ §2.3 的地址→名字反查**对 boss 行成立**。**对实现的影响**：boss 行无需新捕获点 —— 改成「遍历 `mgr+0x58` 全宿主链 × 各宿主线程链 `H+0x1030`」即可，捕获点 `0x40ED16` 仅为可选加速。同步订正 `docs/th10/th10_checkpoint_entry_report.md` §3.5 / 待真机清单第 12 条与 `docs/th10/th10_checkpoint_ip_offset.md` §5.1 / §1.3 / §7.3（原文「boss 侧连逆向锚点都没有 / boss 脚本在其它槽」）。**🟡 全部静态定案、未真机。** |

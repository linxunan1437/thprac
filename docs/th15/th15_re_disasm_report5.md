# TH15 miss/bomb 记录：非 boss 阶段的 ECL 执行时间（第五轮反编译报告）

> 状态：**已回复开发组**。对应需求：`th15_re_disasm_requests5.md`。前四轮：`th15_re_disasm_report.md` / `report2.md` / `report3.md` / `report4.md`。
> 依据：`th15.exe.c`（Ghidra 反编译 C）+ 对 `th15.exe` 的直接反汇编交叉核对（capstone）。
> 地址约定：全文为**静态基址**（= 运行时地址 − `ingame_image_base`，ImageBase `0x400000`），与 thprac `RVA(a)` 口径一致。
> 范围声明：本机制**仅用于非 boss 的正式游戏阶段**（道中）；boss 战 / 对话 / GameOver 等不依赖本时间。仅 ST1~ST6。

---

## 0. 结论速览（TL;DR）

| 需求问题 | 结论 |
| --- | --- |
| **问题 1**：时间字段与每帧推进点 | ECL 时间存于**线程结构体 `thread + 0x0`**（float，单位≈帧，可为负）；线程指针 = `*(宿主 + 0xC)`。每帧推进点 **`0x48E463`**（`addss xmm0,[esp+0x84]`，9B），位于通用执行器 `FUN_0048CA80` 尾部。推进量 = 全局时间缩放 `DAT_004e73e8`（正常 1.0）。**每个宿主每帧一次**。 |
| **问题 2**：主线脚本对象定位 | 主线宿主是**普通敌人对象**，由 `FUN_0043BBB0`（`0x43BBB0`）调用 `FUN_00426050(manager,"main")` 生成。**捕获点 `0x43BBC8`（命中时 `EAX` = 宿主对象）**。`@MainFront()/@MainLatter()` 为**同线程 call**（opcode `0x0B`），**共用单一连续时钟**。与 TH14「sub 表 + 指令指针」结构一致。 |
| **问题 3**：ECLJump 对时间的影响 | ECL 跳转 = opcode `0x0C`（case 0xc，`0x48CD36`）：**`thread.time = 指令 dword5`（= thprac 的 `at_frame`，绝对赋值）**，同时 `ip += dword4`（= `dest-start`）。`ecl_time`（dword0）= goto 自身的执行时间。故补丁跳转后 **`t0 = at_frame`**。 |
| **问题 4**：冻结/暂停 | 时间按 `DAT_004e73e8` 推进；该值为 0 时冻结（例 `0x45175`）。Esc 暂停时整个更新循环不运行，时间不推进。仅非 boss 阶段，无需穷举其余情况。 |
| **问题 5**：推荐挂钩/读取 | **无钩读取优先**：捕获主线宿主（EHOOK `0x43BBC8`，2B）后 `time = *(float*)(*(int*)(host+0xC))`。`t0`：由 ECLJump 进入取 `at_frame`，自然进入取 `0`。 |

---

## 1. 核心结构：ECL 线程（执行上下文）与时间字段

### 1.1 两套解释器分工

TH15 的 ECL 执行由**两个函数**协作：

| 函数 | 地址 | 职责 |
| --- | --- | --- |
| `FUN_0048CA80` | `0x48CA80` | **主执行循环**：读线程时间、按指令时间驱动、处理控制类 opcode（`0x00~0x5D`：goto/wait/call/ret/if 等），并在返回前推进时间 |
| `FUN_004291E0` | `0x4291E0` | 动作类 opcode（`opcode ≥ 0x12C`，即 case 0x00~0xA9）；由主循环通过宿主虚表槽 0 间接调用 |

调用关系（`FUN_0048CA80` 的 default 分支）：

```asm
0048cb18  ...                       ; opcode 落在动作区间
; (C 反编译 line 95618)
iVar9 = (*(code *)**(undefined4 **)param_1[0x406])();   ; 调宿主虚表槽 0 → 0x4291D0 → FUN_004291E0
```

即 `FUN_004291E0` 只处理单条动作指令，**时间推进与时间比较都在 `FUN_0048CA80`**。

### 1.2 宿主对象布局（SptInf/EnemyInf 基类）

宿主 = 敌人对象（`EnemyInf`，虚表 `0x4CC2E8`，构造 `FUN_00426C60`）。相关字段：

| 偏移 | 语义 |
| --- | --- |
| `+0x0C` | **当前线程指针**（`FUN_0048E690` 每处理一个线程就写入） |
| `+0x10` | 内嵌首个线程结构体 |
| `+0x11F8` | ECL 程序资源（`EclResourceInf`） |
| `+0x11FC` | 线程链表头 |
| `+0x2C8` | 时间缩放索引（见 §2.3；敌人/玩家均为 0） |
| `+0x56C4` | 自指针（`= 宿主`，构造时写入；即既有报告的 `scr`） |

> 既有 `g_boss_ptr` 缓存的是 `EDI = enemy + 0x120C`；`*(g_boss_ptr + 0x44B8)`（=`*(enemy+0x56C4)`）**就是宿主对象本身**。

### 1.3 线程结构体（时间字段所在）

线程结构体（`thread`）布局，由解释器入口反汇编坐实：

```asm
00429225  8b97b8440000    mov  edx,[edi+0x44b8]   ; edx = 宿主(enemy)
0042922f  8b420c          mov  eax,[edx+0xc]      ; eax = thread
00429232  8b7008          mov  esi,[eax+8]        ; thread+8  = 执行偏移
0042923a  8b4804          mov  ecx,[eax+4]        ; thread+4  = 程序索引(sub index)
00429261  0fb74e04        movzx ecx,word [esi+4]  ; 当前指令 opcode = [当前指令+4]
```

| 偏移 | 类型 | 语义 |
| --- | --- | --- |
| `thread + 0x00` | **float** | **当前 ECL 时间**（单位≈帧，见 §2.4） |
| `thread + 0x04` | int | 当前 sub 索引（`-1` = 无） |
| `thread + 0x08` | int | 当前 sub 内的执行偏移 |

构造/切脚本时初始化（`th15.exe.c:28684-28686`、`29867-29869`）：

```c
*(int *)(*(int *)(this + 0xc) + 4) = iVar2;   // thread+4 = sub 索引
*(undefined4 *)(*(int *)(this + 0xc) + 8) = 0;// thread+8 = 0
**(undefined4 **)(this + 0xc) = 0;            // thread+0 = 0  (时间清零)
```

### 1.4 精确读取表达式

```cpp
// 宿主指针 host = 敌人对象本身
uintptr_t host   = *(uintptr_t*)(g_boss_ptr + 0x44B8);   // g_boss_ptr = EDI = enemy+0x120C
uintptr_t thread = *(uintptr_t*)(host + 0x0C);
float     t      = *(float*)(thread + 0x00);             // ← 当前 ECL 时间
```

对主线宿主（`g_main_host` = `0x43BBC8` 处捕获的 `EAX`，已是宿主本身）：

```cpp
float t = *(float*)(*(uintptr_t*)(g_main_host + 0x0C));
```

---

## 2. 问题 1：时间字段与每帧推进点

### 2.1 类型 / 单位 / 取值

- **类型**：float。指令内的时间标签是 int，比较前 `cvtdq2ps` 转 float（见 §2.2）。
- **单位**：≈帧。每帧推进量 = `DAT_004e73e8`（正常 `1.0f`，60fps 下 1 单位 = 1/60 秒）。
- **可否为负**：**可以**。wait 类指令 `time -= param`（§3.3），goto 也可直接写入任意值（thprac 的 `at_frame` 可为 0/1/5/60/90 等）。

### 2.2 每帧推进点（问题 1 核心）

执行器 `FUN_0048CA80` 入口先比较时间与当前指令时间；无待执行指令时直接落到推进段：

```asm
0048ca9d  8b5708          mov  edx,[edi+8]           ; thread+8
0048caa9  8b4f04          mov  ecx,[edi+4]           ; thread+4
0048cabe  f30f1007        movss xmm0,[edi]           ; xmm0 = thread.time
0048cace  8b44c804        mov  eax,[eax+ecx*8+4]     ; subTable[idx].start
0048cad2  03f0            add  esi,eax
0048cad8  660f6e0e        movd xmm1,[esi]            ; 指令 dword0 = 指令时间(int)
0048cadc  0f5bc9          cvtdq2ps xmm1,xmm1
0048cadf  0f2fc1          comiss xmm0,xmm1
0048cae2  0f8277190000    jb   0x48e45f              ; time < instr.time → 本帧不执行，去推进
...
0048e45f  f30f1007        movss xmm0,[edi]           ; xmm0 = thread.time
0048e463  f30f58842484000000  addss xmm0,[esp+0x84] ; <<< 每帧推进：time += delta
0048e46c  f30f1107        movss [edi],xmm0           ; thread.time = xmm0
```

- **推进点地址：`0x48E463`**，指令 `addss xmm0,dword ptr [esp+0x84]`，**长度 9 字节**。
- 命中时 `EDI` = **线程指针**（不是宿主）；`[ESP+0x84]` = 本帧增量 `delta`（float）。
- 读到的是**推进前**的值；执行后 `[EDI]` 为新值。
- 一次调用只推进一次；一个宿主的所有线程在 `FUN_0048E690` 中逐个调用 `FUN_0048CA80`。

### 2.3 delta（推进量）来源

`FUN_0048E690` 的调用方通过 `XMM1` 传入 delta。敌人更新路径（`FUN_00427A10`，`0x427D8E`）：

```asm
00427d8e  8b87c8020000    mov  eax,[edi+0x2c8]        ; 时间缩放索引（敌人恒 0）
00427d94  8b8fb8440000    mov  ecx,[edi+0x44b8]       ; ecx = 宿主
00427d9a  8b048520a64c00  mov  eax,[eax*4+0x4ca620]   ; PTR_DAT_004ca620[idx]
00427da1  f30f1008        movss xmm1,[eax]            ; delta
00427da5  e8e6680600      call 0x48e690
```

`PTR_DAT_004ca620[0] = 0x4E73E8`（= `DAT_004e73e8`，全局时间缩放，正常 `1.0f`；进关 `0x43BFF0` 重置为 1.0）。
另有脚本切换路径（`0x428DE8`）显式 `xorps xmm1,xmm1`（delta=0），用于切脚本当帧不推进。

> 结论：**主线时间的每帧增量 = `DAT_004e73e8`**（正常 1.0）。`enemy+0x2C8` 作为索引恒为 0（`0x427A10` / `FUN_00428930` 内均置 0）。

### 2.4 组织方式与调用链（谁驱动）

```
FUN_00426AD0  (管理器每帧任务, 0x426AD0)
  └─ 遍历 [0x4E9A80+0x180] 敌人链表
       └─ FUN_00428830(enemy)   (0x428830)
            └─ FUN_00428930(enemy+0x120C)
                 └─ FUN_0048E690(enemy)   (0x428984)
                      └─ 遍历 [enemy+0x11FC] 线程链
                           ├─ [enemy+0x0C] = thread
                           └─ FUN_0048CA80(thread, delta)   ; 每线程每帧一次
```

- **组织方式 = "每个活跃宿主（对象）每帧、对其线程链逐个各一次"**。主线宿主只有一个线程，故**每帧一次**。
- 主线宿主也被同一管理器链驱动（它是普通敌人对象），无需额外驱动代码。

---

## 3. 问题 2：如何定位"关卡主线脚本"的执行对象

### 3.1 主线宿主 = 用 sub 名 `"main"` 生成的普通敌人对象

关首由 `FUN_0043BBB0`（`0x43BBB0`）生成主线宿主：

```asm
0043bbb0  55              push ebp
0043bbb1  8bec            mov  ebp,esp
0043bbb5  ff750c          push [ebp+0xc]          ; param_2
0043bbb8  8b0d809a4e00    mov  ecx,[0x4e9a80]    ; this = ECL 管理器
0043bbbe  68f4c44c00      push 0x4cc4f4          ; "main"
0043bbc3  e888a4feff      call 0x426050          ; EAX = FUN_00426050(manager,"main",param2) = 宿主
0043bbc8  8be5            mov  esp,ebp            ; <<< 此处 EAX 仍为宿主指针
0043bbca  5d              pop  ebp
0043bbcb  c20c00          ret  0xc
```

- 字符串 `"main"` 位于 `0x4CC4F4`（`FUN_0048F340` 按 sub 名查表）。
- `FUN_00426050` 返回新生成的 `EnemyInf` 对象（`th15.exe.c:27986`），即主线宿主。
- 主线脚本内的 `@MainFront()`/`@MainLatter()` 均为该宿主的 sub，由 opcode `0x0B`（call）进入。

### 3.2 运行期定位（推荐）

**捕获主线宿主指针**（与既有 `g_boss_ptr` 同思路）：

```cpp
// EHOOK @ 0x43BBC8, 覆盖 2B（"8b e5" = mov esp,ebp），命中时 EAX = 宿主
g_main_ecl_host = pCtx->Eax;
```

- 覆盖长度 2B 即可（`0x43BBC8` 为 `mov esp,ebp`，2 字节；下一字节 `0x43BBCA` 为 `pop ebp`）。
- 也可挂在 `0x43BBC3`（5B 的 `call`），但需 `asm_call` 回放才能拿到返回值；**推荐直接挂 `0x43BBC8`**。
- 清空时机：沿用既有 `th15_game_start`（`0x43E6EE`）钩子，与 `g_boss_ptr`/`g_cur_stage_name` 一并清 `g_main_ecl_host = 0`。

**读取表达式**：

```cpp
float t = g_main_ecl_host ? *(float*)(*(uintptr_t*)(g_main_ecl_host + 0x0C)) : NAN;
```

**无钩备选**：遍历管理器链表 `[0x4E9A80+0x180]`，取 `[host+0x11F8]` 为当前关卡 ECL 资源、且当前 sub 名为 `"main"` 的对象。不推荐（需反查 sub 名，且 boss ECL 会替换 `+0x11F8`）。

### 3.3 主线时间是否单一连续时钟？——是

- `@MainFront()`/`@MainLatter()` 是 **opcode `0x0B`（call）**，在同线程上执行：
  - call（case 0x0B，`0x48CCA8`）：保存 `thread.time/+4/+8` 后切 sub（`FUN_0048C800`），**不重置时间**；
  - ret（case 0x0A，`0x48CB1A`）：恢复保存的 `thread+4`/`+8`/`time`。
- 因此 **主线时间在 sub 调用/返回间连续**，`MainFront → MainLatter` 不另起时钟。
- 唯一改变时间的是 **goto（opcode `0x0C`）**（§4）与 wait 类指令（§4.3）。

### 3.4 与 TH14 / TH11 的对应

| 项 | TH14 | TH11 | **TH15** |
| --- | --- | --- | --- |
| 执行对象 | `pCtx->Edi` | `pCtx->Edi` | 宿主 = `*(g_boss_ptr+0x44B8)`（主线另缓存） |
| 时间字段 | 无（用 sub 表 + 指令指针判定归属） | `*(float*)Edi` | **`*(float*)(*(int*)(host+0xC))`** |
| 主线判定 | sub 表区间（MainLatter） | enemy flags == 20512 | **宿主用 `"main"` 生成，捕获于 `0x43BBC8`** |
| 推进点 | `0x483C4F`（4B） | `0x45D2A3`（2B） | **`0x48E463`（9B，通用执行器内）** |
| 结构接近度 | — | — | **与 TH14 的「sub 表 + 指令指针」同构**（TH15 用同一 `[[[host+0x1018]+0x11F8]+0x8C]` 子表） |

> **一致性结论**：TH15 的时间字段更接近 **TH11 的 `*(float*)Edi`**（对象自带时间）；执行对象/主线判定更接近 **TH14 的子表结构**。两者在 TH15 合流：时间挂在线程结构上，主线是 `"main"` 宿主。

---

## 4. 问题 3：thp 补丁跳转（ECLJump）对时间的影响

### 4.1 thprac ECLJump 指令格式

`thprac_th15.cpp`：

```cpp
void ECLJump(ECLHelper& ecl, unsigned int start, unsigned int dest, int at_frame, int ecl_time = 0)
{
    ecl.SetPos(start);
    ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0x00000000 << dest - start << at_frame;
}
```

按 dword 展开（指令 24 字节，opcode 位于 `+4`、size 位于 `+6`）：

| dword | 值 | 语义 |
| --- | --- | --- |
| `[0]` | `ecl_time` | **goto 指令自身的时间标签**（`thread.time ≥ 它` 时执行该 goto） |
| `[1]` | `0x0018000C` | opcode `0x000C`（=12，goto）+ size `0x0018`（24） |
| `[2]` | `0x02ff0000` | 参数掩码 |
| `[3]` | `0` | — |
| `[4]` | `dest - start` | **跳转偏移** |
| `[5]` | `at_frame` | **跳转后写入的时间（绝对值）** |

### 4.2 goto 处理（坐实"直接改写时间"）

case 0xc @ `0x48CD36`：

```asm
0048cd36  660f6e4614      movd   xmm0,[esi+0x14]  ; dword5 = at_frame
0048cd3b  8d4e10          lea    ecx,[esi+0x10]   ; &dword4 = dest-start
0048cd3e  0f5bc0          cvtdq2ps xmm0,xmm0
0048cd41  f30f1107        movss  [edi],xmm0       ; thread.time = at_frame  (绝对赋值)
0048cd45  8b01            mov    eax,[ecx]        ; dest-start
0048cd47  014708          add    [edi+8],eax      ; thread+8 += dest-start
0048cd4a  0331            add    esi,[ecx]        ; ip += dest-start
```

- **对时间字段的操作 = 直接改写为 `at_frame`（保持原值 → 否；重置/改写 → 是，绝对赋值）。**
- `at_frame` 语义 = 跳转后主线脚本的**当前时间**（即该练习段落的起始时间）。
- `ecl_time` 语义 = goto 指令自身的执行时间（控制"何时跳"）。

### 4.3 游戏自身的时间标签交互

| opcode | 地址 | 行为 |
| --- | --- | --- |
| `0x0C` goto | `0x48CD36` | `time = dword5`；`ip += dword4` |
| `0x17` wait | `0x48CD51` | `time -= param[0]`（`movss xmm1,[edi]; subss xmm1,xmm0; movss [edi],xmm1`） |
| `0x18` | `0x48CD72` | 由 `FUN_0048E810` 取值后同样做减法 |
| `0x0B` call | `0x48CCA8` | 保存 `time` 后切 sub，**不改时间** |
| `0x0A` ret | `0x48CB1A` | 恢复保存的 `time` |

即：脚本里的 `goto ... @ T` 通过 goto 的 `at_frame` 写入时间；`+N:` 一类等待通过 `time -= N` 消耗时间。**goto 是唯一的"时间改写"通道**。

### 4.4 `t0` 计算规则（对开发组）

- **由 thp ECLJump 进入某段**：goto 执行后 `thread.time = at_frame`，故 **`t0 = 该次 ECLJump 的 `at_frame` 参数**（`THStageWarp` 调用里已直接给出，无需运行期捕获）。
- **从关首自然进入（无跳转）**：主线宿主创建时 `thread.time = 0`，故 **`t0 = 0`**。
- 于是：`elapsed = ReadMainEclTime() - t0`。
- 也可不用 `t0`，直接把边界表建在**绝对时间**上（因为跳转写入的是绝对值，段内时间 = `at_frame + 帧数`）。

---

## 5. 问题 4：时间推进的冻结/暂停（仅非 boss 阶段）

- 推进量 = `DAT_004e73e8`（float）。**该值为 0 时主线时间冻结**；实例：`FUN_00441920`（`0x45175`）在某舞台控制器状态下临时 `DAT_004e73e8 = 0` 后恢复。
- **Esc 暂停 / 菜单**：整个更新循环（含管理器 `FUN_00426AD0`）不运行，时间不推进。
- 本机制**只用于非 boss 的正式游戏阶段**；boss / 对话 / GameOver/Continue 的具体冻结细节开发组不依赖，未穷举。

> 读取连续性：主线时间为单一 float 计数器，正常推进下逐帧 +1.0，连续；被跳转改写时按 `at_frame` 重置。

---

## 6. 问题 5：推荐挂钩点与读取方案

### 6.1 捕获 `t0` / 主线宿主

| 用途 | 地址 | 覆盖 | 命中语义 |
| --- | --- | --- | --- |
| **捕获主线宿主**（推荐） | `0x43BBC8` | **2B** | `EAX` = 主线宿主对象（`FUN_0043BBB0` 内 spawn `"main"` 之后） |
| 清空宿主（新局/进关） | `0x43E6EE`（既有 `th15_game_start`） | 7B | 与 `g_boss_ptr` 一并清 `g_main_ecl_host` |
| 备选捕获点 | `0x43BBC3` | 5B | `call 0x426050`；需 `asm_call` 回放取返回值，不如上者 |

### 6.2 运行期读取（无钩优先）

```cpp
// 缓存一次（0x43BBC8），之后纯读内存，无每帧钩子
float ReadMainEclTime()
{
    if (!g_main_ecl_host) return NAN;
    uintptr_t thread = *(uintptr_t*)(g_main_ecl_host + 0x0C);
    if (!thread) return NAN;
    return *(float*)(thread + 0x00);
}
```

### 6.3 若必须挂钩（不推荐）

- 每帧推进点 `0x48E463`（`addss xmm0,[esp+0x84]`，**9B**）：`EDI` = 线程指针，`[ESP+0x84]` = delta，命中值为**推进前**。
- 该点是**全宿主共用**（所有 ECL 线程都经此），需自行按 `EDI` 过滤主线线程，开销大于无钩读取。

### 6.4 落地接线（对开发组）

```cpp
// miss/bomb 回调内
uint32_t t = ReadMainEclTime();
if (t != INVALID) {
    int elapsed = (int)t - g_eclTimeOrigin;   // g_eclTimeOrigin = ECLJump 的 at_frame（或 0）
    const char* phase = LookupPhaseByTime(stage, elapsed);
    // phase 只需唯一标识；查不到保底记原始 elapsed
}
```

---

## 7. TH14 / TH11 结论逐项对照（一致 / 不一致）

| 对照项 | 结论 |
| --- | --- |
| 时间是否存于执行对象 | **一致**（TH11 式）：TH15 存在宿主的线程结构 `*(host+0xC)+0`（float），而非宿主本体 `+0` |
| `Edi` 是否为执行对象 | **不一致**：TH15 既有钩子的 `EDI` = `enemy+0x120C`；真正的"脚本宿主"是 `*(EDI+0x44B8)` |
| sub 表 + 指令指针区间判定 | **一致**（TH14 式）：TH15 子表 = `[[[host+0x1018]+0x11F8]+0x8C]`，每项 8B、`+4` = sub 起始指令地址 |
| "跳过指令即冻结时间" | **一致**：TH15 跳过 `0x48E463` 的 `addss` 即可冻结该线程时间（但该点全局共用） |
| enemy flags 判定主线 | **不一致**：TH15 无 `20512` 式标志；主线用 **sub 名 `"main"`** 生成，捕获 spawn 点最稳 |
| 时间单位 | **一致**：≈帧（TH15 增量 = `DAT_004e73e8`，正常 1.0） |

---

## 8. 证据索引

| 证据 | 位置 |
| --- | --- |
| 线程布局（`[edx+0xc]`→`+8`/`+4`，opcode @ `[esi+4]`） | 反汇编 `0x429225-0x429261` |
| 时间清零初始化 | `th15.exe.c:28684-28686`、`29867-29869` |
| 解释器 case 0x57（opcode `0x224`，`time -= param`） | case 地址 `0x42D7CD`；`th15.exe.c:31759` |
| 主执行循环入口/时间比较 | 反汇编 `0x48CA9D-0x48CAE2` |
| **每帧推进点** | 反汇编 `0x48E45F-0x48E46C`（`addss xmm0,[esp+0x84]`） |
| 线程链遍历 + delta 传入 | `FUN_0048E690` `0x48E690`（`lea esi,[ebx+0x11fc]`、`call 0x48ca80`） |
| delta 来源（`PTR_DAT_004ca620` → `DAT_004e73e8`） | 反汇编 `0x427D8E-0x427DA5`、`0x428979-0x428984`；表 `0x4CA620[0]=0x4E73E8` |
| 管理器每帧驱动链 | `FUN_00426AD0` `0x426AD0` → `FUN_00428830` → `FUN_00428930` → `FUN_0048E690` |
| 主线宿主 spawn（`"main"`） | 反汇编 `0x43BBB0-0x43BBC3`；字符串 `0x4CC4F4`；`FUN_00426050` `th15.exe.c:27986` |
| goto 改写时间 | 反汇编 `0x48CD36-0x48CD4C` |
| wait 减时间 | 反汇编 `0x48CD51-0x48CD6D` |
| call/ret 保时间 | 反汇编 `0x48CCA8`、`0x48CB1A`；`FUN_0048C800` `th15.exe.c:95441` |
| 时间缩放置 0 实例 | `FUN_00441920` `0x45175` |
| thprac ECLJump 格式 | `thprac/src/thprac/thprac_th15.cpp:1019-1023` |

---

## 9. 遗留 / 建议

- **建议开发组采用无钩读取**：`0x43BBC8` 捕获 `g_main_ecl_host` 后纯内存读取；`0x43E6EE` 清缓存。
- `t0` 建议直接用 `THStageWarp` 的 `at_frame` 常量（跳转即写入绝对值），无需运行期捕获；自然进入段用 `0`。
- 若实测发现主线宿主存在多线程（理论上 `call` 同线程，不会），再改用遍历 `[host+0x11FC]` 并取当前 sub 名为 `main` 的线程。

---

*创建：2026-09-26*

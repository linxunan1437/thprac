# TH15 Boss 段监控 — 方案二（boss 宿主 ECL 当前 sub 名）逆向结论

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志）
> 上位 / 模板：`docs/th16/th16_boss_phase_report.md`（boss 段监控「方案一 / 方案二」；本次采用其 **方案二**）
> 总括（术语口径以此为准）：`docs/logger/logger_re_disasm_overview.md`
> 同作：`docs/th15/th15_ecl_re_report.md`（TH15 道中 ECL 段落监控 Q1–Q9；本报告是其「boss 段」续篇）
> 素材清单与来源：`docs/logger/decompile_materials.md`；回填工作流：`docs/logger/checkpoint_entry_backfill_workflow.md`
> 重点验证对象：`../th15decode/boss-phase-hook.md`（旧「方案 A」式取法，本报告对其 **[3] `host = *(E+0x44B8)`** 一处口径冲突作判定）
> 二进制指纹：`../exes/th15.exe`，**951808 B**，PE `TimeDateStamp = 1444028071`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；`.text` = VA `0x401000`–`0x4BD92B`，RVA `0x1000`–`0xBC92B`）
> 方法：`pefile 2024.8.26` + `capstone 5.0.7`（x86-32）只读反汇编；`RELOCS_STRIPPED` ⇒ 用 4 字节小端 VA 立即数做字节搜索定位 xref；并与本机 `../th15decode/th15.exe_decompiled.c`（Ghidra 全量反编译）交叉核对。
> ECL / decl 素材：`../exes/ecl/th15/`（原始 `.ecl` 23 份）、`../exes/ecl_dump/th15/`（`thecl -d` 反编译 `.decl` 23 份）。
> 状态：**静态调研（🟡 未真机三态）**。本机 macOS 不可编译、不可真机；未真机验证的地址/结论文中均标 🟡。
> 地址口径：正文以 **VA** 为主，并同时标注 **RVA = VA − 0x400000**。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **口径冲突（核心）** | ✅ **`mgr+0x180` 链上对象 `E` 本身就是 host** | `boss-phase-hook.md` 的 `host = *(E+0x44B8)` **错**：`E` 已是要找的 ECL 宿主；`*(E+0x44B8)` 读到的是 `host+0x44B8`（= B 内部一个无关字段），不是 host |
| **boss 宿主取法** | ✅ **给出（TA 版）** | `node=*(mgr+0x180)`；`E=*(node)`；`*(E+0x5740)==boss_id` 命中；**`host = E`**（**不再取 `[E+0x44B8]`**）。等价于直接调游戏自身的「当前 boss」查询 `FUN_0041F7F0(slot)` |
| **`FUN_0048F7F0`** | ⚠️ **地址有误，应为 `FUN_0041F7F0`（VA `0x41F7F0`）** | 游戏自身「按槽取当前 boss host」查询；与 TH16 `FUN_00417580` **逐指令同形**（仅管理器全局地址不同） |
| **B 与 host 的关系** | ✅ **确定** | `B = host + 0x120C`（`FUN_00426C60` 的 `lea esi,[edi+0x120c]`）；**`*(B+0x44B8) == host`**（= `host+0x56C4` host 自指针）；`B+0x4060`（= `host+0x526C`）为 boss 门控位 |
| **当前 sub 名链** | ✅ **确认（与道中一致）** | `host=…；thread=*(host+0xC)；subIdx=*(thread+4)；res=*(host+0x11F8)；table=*(res+0x8C)；name=*(char**)(table+subIdx*8)`；`thread+4` 确为**索引** |
| **boss 脚本 / 阶段名** | ✅ **枚举** | `setNext`=`opcode 0x202`、`setTimeout`=`0x203`；boss 脚本 sub（`Boss`/`Boss1`…/`BossCard1`…/`MBoss…`）；`st01mbs2`=`MBossB…`、`st07bs2`=`BossB…` |
| **boss 行回填** | 🟡 **部分回填** | 已回填 `docs/logger/th15_checkpoints.csv` 的 **ST1 BOSS1–4、ST2 BOSS1–6（共 10 行）**（高置信）；ST3–ST7 与各关 MID 行**暂缺**（原因见 §5） |
| **与 TH16 同源** | ✅ | `FUN_0041F7F0`㊤ ≡ `FUN_00417580`；`FUN_004255E0` ≡ `FUN_0041A9C0`；均「按 id 遍历 `mgr+0x180` 链、`*node` 即 host」 |

> **一句话**：TH15 与 TH16 完全同形 —— **`mgr+0x180` 敌机链里的对象就是 ECL 宿主（host）本身**；`B`（`=host+0x120C`）是宿主的「action / 指令处理子对象」，它用 `+0x44B8` **回指** host。`boss-phase-hook.md` 把「子对象回指」误用到「链上对象」上，多走了一步，导致读错。

---

## 1. 方案二机制（TH16 定案，TH15 复核通过）

boss ECL（`Boss` / `BossCardN` / `MBoss`…）**跑在 boss 敌人自己的宿主里**（由 `enmCreateA("Boss"/"MBoss"…)` 异步生成），**不在关卡主线宿主**。故做 boss 段必须先取得 **boss 宿主**，再按与道中相同的指针链读「当前 sub 名」，经白名单 latch 映射为当前阶段。

统一的「当前 sub 名」指针链（TH15 = TH16 = 道中）：

```text
host   = boss 宿主（见 §2）
thread = *(u32*)(host + 0x0C)          // ECL 线程/执行游标
subIdx = *(i32*)(thread + 4)           // 当前 sub 索引（−1 = 脚本未运行/已结束）
res    = *(u32*)(host + 0x11F8)        // = *(u32*)(*(u32*)(0x4E9A80) + 0x17C)
count  = *(i32*)(res + 8)              // sub 总数（越界保护）
table  = *(u32*)(res + 0x8C)           // sub 表，项 8B：+0=名字 char*、+4=起始
name   = *(char**)(table + subIdx*8)   // 项 +0 = 名字
```

TH15 侧逐字段证据（VA / RVA）：

| 字段 | 证据函数（VA） | 反汇编片段 |
| --- | --- | --- |
| `host+0xC = thread` | `FUN_0048E690`（`0x48E690`） | `mov [param_1+0xc], pfVar2`（每帧把当前线程写入宿主 +0xC） |
| `host+0x11F8 = res` | `FUN_00430880`（`0x430880`） | `mov eax,[ecx+0x17c]; mov [esi+0x11f8],eax` |
| `thread+4 = subIdx`（索引） | `FUN_0048F410`（`0x48F410`） | `mov ecx,[esi+0xc]; mov [ecx+4],eax`（`eax` = 按名查表结果） |
| `thread+4` 作 `table[...]` 下标 | `FUN_0048CA80`（`0x48CA80`） | `0x48CAB5 mov eax,[edi+0x1018];` `0x48CAC2 mov eax,[eax+0x11f8];` `0x48CAC8 mov eax,[eax+0x8c];` `0x48CACE mov eax,[eax+ecx*8+4]`（`ecx=[edi+4]`） |
| 取当前运行地址（同链） | `FUN_0048F3D0`（`0x48F3D0`） | `[ecx+0x1018]→+0x11f8→+0x8c→[. +edx*8+4]+0x10+[ecx+8]` |
| 操作数求值（同链、下标=thread+4） | `FUN_0048E750`（`0x48E750`） | 同上链，`*(param_1+4)` 作 `*8` 下标 |

> `host+0x11F8` 亦可经线程取得：`res = *(u32*)(*(u32*)(thread+0x1018) + 0x11F8)`（因 `thread+0x1018 == host`，见 `FUN_0048F700` 的 `mov [edi+0x1018],eax`）。

---

## 2. boss 宿主取法（含口径冲突的判定）——**本报告核心**

### 2.1 游戏自身「当前 boss」查询：`FUN_0041F7F0`（**不是** `0x48F7F0`）

`boss-phase-hook.md` 记「`FUN_0041f7f0`(0x48F7F0)」——**函数名对、地址错**：该函数实际 VA = **`0x41F7F0`**（RVA `0x1F7F0`）；`0x48F7F0` 处并非函数入口（反汇编为错位字节）。其反汇编：

```asm
0041f7f0  55                push ebp
0041f7f1  8bec              mov  ebp, esp
0041f7f3  8b4508            mov  eax, [ebp+8]              ; eax = slot
0041f7f6  33c9              xor  ecx, ecx
0041f7f8  56                push esi
0041f7f9  8b35809a4e00      mov  esi, [0x4e9a80]          ; ★ mgr（ECL/敌机管理器）
0041f7ff  8b548648          mov  edx, [esi+eax*4+0x48]    ; boss_id = *(mgr+0x48+slot*4)
0041f803  85d2              test edx, edx
0041f805  7507              jne  0x41f80e
0041f807  33c0              xor  eax, eax
0041f809  5e                pop  esi
0041f80a  5d                pop  ebp
0041f80b  c20400            ret  4
0041f80e  8b8680010000      mov  eax, [esi+0x180]         ; ★ 链头 = *(mgr+0x180)
0041f814  85c0              test eax, eax
0041f816  7419              je   0x41f831
0041f818  eb06              jmp  0x41f820
0041f81a  8d9b00000000      lea  ebx, [ebx]
0041f820  8b08              mov  ecx, [eax]               ; ★ E = node[0]
0041f822  399140570000      cmp  [ecx+0x5740], edx        ; ★ *(E+0x5740) == boss_id ?
0041f828  7407              je   0x41f831
0041f82a  8b4004            mov  eax, [eax+4]             ; node = node[1]（下一个节点）
0041f82d  85c0              test eax, eax
0041f82f  75ef              jne  0x41f820
0041f831  8bc1              mov  eax, ecx                  ; 返回 E
0041f833  5e                pop  esi
0041f834  5d                pop  ebp
0041f835  c20400            ret  4
```

⇒ 返回的对象 `E` **就是**要用的 **host**。**此处没有任何 `[E+0x44B8]` 步骤。**

### 2.2 链表节点 = host 本身（而非 B）

`FUN_00430610`（`0x430610`，宿主生成+挂链）与 `FUN_00430880`（`0x430880`）显示节点构造：

```asm
; FUN_00430610（宿主生成后挂到 mgr+0x180）
00430725  8d8e0c120000      lea  ecx, [esi+0x120c]        ; ★ B = host+0x120C
00430777  e8e44bffff        call 0x425360                 ;   构造 B（FUN_00425360）
0043077c  89b6f4140000      mov  [esi+0x14f4], esi        ; ★ host+0x14F4 = host（链节点自指）
...
004307be  8d9ef4140000      lea  ebx, [esi+0x14f4]        ; node = host+0x14F4
004307c4  8933              mov  [ebx], esi               ; ★ node[0] = host
...
; 挂链
00430828  83b88001000000    cmp  [eax+0x180], 0           ; mgr+0x180 为空？
0043082f  7508              jne  0x430839
00430831  899880010000      mov  [eax+0x180], ebx         ; ★ mgr+0x180 = node（= host+0x14F4）
```

另一个佐证：`FUN_004254D0`（`0x4254D0`）遍历同一链时直接读**宿主体**字段：

```asm
004254d0  a1809a4e00        mov  eax, [0x4e9a80]
004254d7  8b8080010000      mov  eax, [eax+0x180]
...
004254e4  8b00              mov  eax, [eax]               ; E = node[0]
004254e6  f6806c52000031    test byte [eax+0x526c], 0x31   ; ★ 读 E+0x526C（host 门控字段）
004254ef  83b80852000000    cmp  dword [eax+0x5208], 0
```

`E+0x526C` 与 `E+0x5208` 都在 `host` 的 `0x574C` 分配区间内（见 §2.4），且 `0x526C == 0x120C + 0x4060`（即 `B+0x4060`）——**若 `E` 是 B，则 `E+0x526C` 落在宿主分配之外**。故 `E` 只能是 **host**。

### 2.3 `B`、`host`、`+0x44B8` 三者的确切关系

`FUN_00426C60`（`0x426C60`，宿主构造器）：

```asm
00426c60  55                push ebp
00426c61  8bec              mov  ebp, esp
00426c65  8bf9              mov  edi, ecx                 ; edi = host
00426c6c  8db70c120000      lea  esi, [edi+0x120c]        ; ★ esi = B = host+0x120C
00426c72  c707e8c24c00      mov  dword [edi], 0x4cc2e8    ; host vtable = EnemyInf
00426c78  8bce              mov  ecx, esi
00426c7a  e8e1e6ffff        call 0x425360                 ; 构造 B（FUN_00425360）
00426c96  6a44              push 0x44
00426c9e  89bfc4560000      mov  [edi+0x56c4], edi        ; ★ host+0x56C4 = host（host 自指针）
...
00426d8b  89bff4140000      mov  [edi+0x14f4], edi        ; ★ host+0x14F4 = host（链节点自指）
00426d7e  c7877c520000ffffffff mov [edi+0x527c], 0xffffffff ; 槽位初值 −1
```

偏移算术（**关键**）：

```text
B       = host + 0x120C
B+0x44B8 = host + 0x120C + 0x44B8 = host + 0x56C4 = host      （host 自指针写入点）
```

⇒ **`*(B+0x44B8) == host` 成立，且仅当持有 `B` 时才有意义。** 反过来，若持有 `host`（链上对象 `E`）再取 `[E+0x44B8]`，读到的是：

```text
E + 0x44B8 = host + 0x44B8 = B + (0x44B8 − 0x120C) = B + 0x32AC   （B 内部的一个无关字段）
```

**故 `boss-phase-hook.md` 第 [3] 步 `host = *(u32*)(E+0x44B8)` 是错的**：它把「B 回指 host」当成了「链上对象 E 回指 host」。由于 `E` 已经是 host，正确做法是 **`host = E`**，不再取一次 `[E+0x44B8]`。

> **`E+0x5740` 与 `E+0x44B8` 为何并存于同一对象？**
> 二者并不「同属 E 一下一上」：`E+0x5740` 是 **host 级字段**（`E` 即 host，`0x5740 < 0x574C`）；而 `E+0x44B8` 若按 `E=B` 解读则应是 **B 级字段**（`B+0x44B8`）。`E` 只能是 host，故 `E+0x44B8` 不是「host 指针」，而是 host 落在 B 区间内的普通字节偏移。**同一对象（E=host）上既不能读 `+0x5740`（id）又读 `+0x44B8`（→host）地自洽**——这正是冲突的根源：
> - 若 `E` = **host**：`E+0x5740`=id ✓，`E+0x44B8` 无 host 语义 ✗；
> - 若 `E` = **B**：`E+0x44B8`=host ✓，但 `E+0x5740 = host+0x694C` 已**超出宿主 0x574C 分配**（id 字段根本不存在）✗。
> 由 §2.2（`E+0x526c` 在分配内、链节点自指 `host+0x14F4`）判定 **`E` = host**。

### 2.4 host 尺寸与字段归属（判定依据）

`FUN_00426050`（`0x426050`，宿主 spawner）分配 **`0x574C`** 字节：

```asm
00426050  55                push ebp
...
00426069  8bf9              mov  edi, ecx
0042606b  684c570000        push 0x574c                   ; ★ malloc(0x574C)
00426070  e8??              call 0x49039f
```

宿主（`host`，VA 0x574C 区间）字段：`+0xC`=thread、`+0x11F8`=res、`+0x14F4`=链节点（node[0]=host）、`+0x5208`/`+0x526C`（门控/状态）、`+0x56C4`=host 自指针、`+0x5740`=敌机 id。
子对象 `B = host+0x120C`（区间 `0x120C…0x573C`，约 `0x4530` 字节，见 `FUN_00426C60` 的 `FUN_0049b080(host+0x120c,0,0x4530)`）字段：`+0x44`…（位置/速度数组）、`+0x4060`=门控位（= `host+0x526C`）、`+0x44B8`=host 回指（= `host+0x56C4`）。

### 2.5 为什么 `+0x44B8` 会「看起来」像 host 指针

ECL 内建指令分发器 `FUN_004291D0`（`0x4291D0`，host vtable[0]）**入口把 host 直接转成 B**：

```asm
004291d0  81c10c120000      add  ecx, 0x120c             ; ★ ecx: host → B
004291d6  e905000000        jmp  0x4291e0
004291e0  55                push ebp
...
0042921f  8bf9              mov  edi, ecx                 ; edi = B（本次 this）
00429225  8b97b8440000      mov  edx, [edi+0x44b8]        ; ★ host = *(B+0x44B8)（回指）
0042922f  8b420c            mov  eax, [edx+0xc]           ; thread = *(host+0xC)
00429235  8b7008            mov  esi, [eax+8]             ; ip
0042923a  8b4804            mov  ecx, [eax+4]             ; sub 索引
...
```

内建 handler thunk（如 `0x430320`）同样以 **B 为 `this`**：

```asm
00430320  55                push ebp
00430321  8bec              mov  ebp, esp
00430323  8b89b8440000      mov  ecx, [ecx+0x44b8]        ; ★ host = *(B+0x44B8)
00430329  8b490c            mov  ecx, [ecx+0xc]           ; thread = *(host+0xC)
0043032c  5d                pop  ebp
0043032d  e91ee40500        jmp  0x48e750               ; 求值
```

⇒ `+0x44B8` 是 **B → host 的回指**，只用于「以 B 为 this」的指令处理路径。`boss-phase-hook.md` 很可能就是看到了这类 thunk 而误以为链上对象也是 B。**链上对象是 host，指令处理器才拿 B。**

### 2.6 boss 宿主注册：`mgr+0x48+slot*4 = host+0x5740`

boss 出场/退场的内建处理（TH16 报告称之为 th15 的 `case512`，VA `0x42B92D`）：

```asm
0042b92d  6a00              push 0
0042b92f  8bcf              mov  ecx, edi                 ; edi = B
0042b931  e8ea490000        call 0x430320                 ; 求值 operand0
0042b938  8bf0              mov  esi, eax
0042b93a  e8319cffff        call 0x425570
0042b93f  85f6              test esi, esi
0042b941  792e              jns  0x42b971                 ; ≥0 → 出场路径
; 退场路径：清门控 + 清槽
0042b943  f7876040000000008000 test dword [edi+0x4060], 0x800000   ; ★ 门控 bit23
0042b94d  7413              je   0x42b962
0042b94f  8b8f70400000      mov  ecx, [edi+0x4070]        ; boss 槽
0042b955  a1809a4e00        mov  eax, [0x4e9a80]
0042b95a  c744884800000000  mov  [eax+ecx*4+0x48], 0      ; ★ 清 mgr+0x48+slot*4
0042b962  81a760400000ffff7fff and dword [edi+0x4060], 0xff7fffff ; 清 bit23
; 出场路径
0042b971  ffb7b8440000      push [edi+0x44b8]             ; ★ host = *(B+0x44B8)
0042b977  818f6040000000008000 or dword [edi+0x4060], 0x800000    ; ★ 置门控 bit23
0042b981  56                push esi                      ; slot
0042b982  e8a99bffff        call 0x425530                 ; ★ FUN_00425530(slot, host)
0042b987  89b770400000      mov  [edi+0x4070], esi        ; B+0x4070 = slot
```

`FUN_00425530`（`0x425530`）——槽表写入：

```asm
00425530  55                push ebp
...
00425534  8b4d0c            mov  ecx, [ebp+0xc]           ; ecx = host
00425537  8b15809a4e00      mov  edx, [0x4e9a80]
0042553d  8b4508            mov  eax, [ebp+8]             ; eax = slot
00425540  85c9              test ecx, ecx
00425542  7410              je   0x425554
00425544  8b8940570000      mov  ecx, [ecx+0x5740]        ; ★ host+0x5740 = 敌机 id
0042554a  894c8248          mov  [edx+eax*4+0x48], ecx    ; ★ mgr+0x48+slot*4 = id
```

⇒ 与 `FUN_0041F7F0` 完全对称：**写入端**把 `host` 的 `+0x5740`（id）登记到 `mgr+0x48+slot*4`；**查询端**用该 id 在 `mgr+0x180` 链上匹配 `*(E+0x5740)` 找回 **host**。门控位 `B+0x4060` 的 bit23（`0x800000`）在出场置位、退场清除。

### 2.7 正确的 TH15 boss 宿主取法

```cpp
// 方案二：先取 boss 宿主（host）；链上对象 E 本身即 host
uint32_t mgr    = *(uint32_t*)0x4E9A80;
uint32_t slot   = 0;                                 // 通常 0；为 0 时回退 1、2（HUD 同序）
uint32_t bossId = *(uint32_t*)(mgr + 0x48 + slot*4);
if (!bossId) return nullptr;                         // 无 boss
uint32_t node   = *(uint32_t*)(mgr + 0x180);
uint32_t host   = 0;
while (node) {
    uint32_t E = *(uint32_t*)node;                   // node[0]
    if (*(uint32_t*)(E + 0x5740) == bossId) { host = E; break; }   // ★ host = E
    node = *(uint32_t*)(node + 4);                   // node[1]
}
// 等价：host = (uint32_t)FUN_0041F7F0(slot);          // 0x41F7F0(RVA 0x1F7F0)
```

> 若某钩点处寄存器持有的是 **B**（解释器 this / 指令 handler this，如 `±0x4060` 门控相关函数、`FUN_004291D0` 内 `edi=B`），则应先 `host = *(u32*)(B+0x44B8)`，再走 §1 的链。**两条口径的差别仅在「当前持有的是 host 还是 B」。**

### 2.8 与 `boss-phase-hook.md` 的逐条核对

| 步 | `boss-phase-hook.md` | 判定 |
| --- | --- | --- |
| [1] | `boss_id = *(u32*)(0x4E9A80 + 0x48 + slot*4)` | ✅ 正确（= `FUN_0041F7F0` / `FUN_00425530` 口径） |
| [2] | `node=*(0x4E9A80+0x180)`；`E=*(node)`；`*(E+0x5740)==id` | ✅ 正确，**且 `E` 已是 host** |
| [3] | `host = *(u32*)(E+0x44B8)` | ❌ **错**；`host = E`。`[+0x44B8]` 是 B 的回指，不适用于 E |
| [4] | `state = *(u32*)(host+0x0C)` | ✅ 正确（= thread） |
| [5] | `funcIdx = *(u32*)(state+4)`（−1=未运行） | ✅ 正确（= sub 索引） |
| [6] | `owner = *(u32*)(state+0x1018)` | ✅ 正确（`thread+0x1018 == host`，见 `FUN_0048F700`） |
| [7] | `res = *(u32*)(owner+0x11F8)` | ✅ 正确（= host+0x11F8，即 `*(mgr+0x17c)`） |
| [8] | `table = *(u32*)(res+0x8C)` | ✅ 正确 |
| [9] | `阶段名 = *(u32*)(table+funcIdx*8)` | ✅ 正确（项 +0 = 名字 `char*`） |

> 结论：**仅 [3] 一处错**（多了一次 `[E+0x44B8]`）。其余一致。该处修正后，`boss-phase-hook.md` 的方案即可直接落地（其触发点 `0x58DE0` 等另论，不在本报告范围）。

---

## 3. 当前 sub 名读取链（复核 `thread+4` = 索引）

### 3.1 直接证据

`FUN_0048CA80`（`0x48CA80`，字节码分发）：`0x48CAA9 mov ecx,[edi+4]`（sub 索引）→ `0x48CAB5 mov eax,[edi+0x1018]`（host）→ `0x48CAC2 mov eax,[eax+0x11f8]`（res）→ `0x48CAC8 mov eax,[eax+0x8c]`（table）→ `0x48CACE mov eax,[eax+ecx*8+4]`（**以 `ecx=[edi+4]` 为 `*8` 下标**）⇒ `thread+4` 是 **索引**（域 `0..count−1`，`−1` = 未运行）。

`FUN_0048F410`（`0x48F410`，按名发脚本 / 建 0x120C 线程）确认写入端：

```asm
0048f415  680c120000        push 0x120c                 ; 分配 0x120C 线程对象
0048f41c  e87e0f0000        call 0x49039f
...
0048f43a  89bef8110000      mov  [esi+0x11f8], edi      ; 线程对象 +0x11F8 = res
0048f440  e8fbfeffff        call 0x48f340               ; 按名查 sub 索引（FUN_0048F340）
0048f445  8b4e0c            mov  ecx, [esi+0xc]         ; ecx = thread
0048f449  894104            mov  [ecx+4], eax           ; ★ thread+4 = sub 索引
0048f44f  c7400800000000    mov  dword [eax+8], 0       ; thread+8 = ip = 0
0048f459  c70000000000      mov  dword [esi], 0
```

`FUN_0048F700`（`0x48F700`，spawn / 克隆线程）：

```asm
0048f70e  8983f8110000      mov  [ebx+0x11f8], eax      ; 子线程 +0x11F8 = res
0048f72c  899b28100000      mov  [ebx+0x1028], ebx
0048f732  8930              mov  [eax], esi             ; host+0x11FC 链
...
0048f79a  898718100000      mov  [edi+0x1018], eax      ; ★ 子线程 +0x1018 = 父 host
```

⇒ `thread+0x1018 == host`（`FUN_0048E750` / `FUN_0048CA80` 都用它反查 res）。

### 3.2 名字表：加载器 `FUN_0048F130` / 按名查表 `FUN_0048F340`

`FUN_0048F340`（`0x48F340`，二分查表）：`mov edi,[ecx+8]`（count）、`mov ecx,[ecx+0x8c]`（table）、`mov eax,[ecx+esi*8]`（`table[i].+0` = 名字 `char*`），逐字节比较 ⇒ **项 +0 = 名字 `char*`**。（与 th15_ecl_re_report §2 一致；加载器 `FUN_0048F130` 一次解析生成 `res+0x8C` 表。）

### 3.3 boss 段取用（latch）

在宿主 `host` 上取 `name=NameOf(host, subIdx)`：

```cpp
uint32_t thread = *(uint32_t*)(host + 0x0C);
int32_t  idx    = *(int32_t*)(thread + 4);
if (idx < 0) return;                                  // 脚本未运行
uint32_t res    = *(uint32_t*)(host + 0x11F8);
int32_t  count  = *(int32_t*)(res + 8);
uint32_t table  = *(uint32_t*)(res + 0x8C);
if (idx >= count) return;                             // 越界保护
const char* name = *(const char**)(table + idx*8);
// 命中白名单 → 置当前段落；否则 no-op（latch）
```

> 与道中同机制（`docs/logger/logger_re_disasm_overview.md` §1.2 / §6 Q9）；白名单即 `docs/logger/th15_checkpoints.csv`。

---

## 4. boss 脚本 / 阶段名

### 4.1 `setNext` / `setTimeout` 结构（opcode）

对 `../exes/ecl_dump/th15/st01bs.decl` 的 `setNext(0,1700,2280,"BossCard1")` 逐字节定位：指令 `id = 0x202`、`size = 0x2c`；`setTimeout(1,"MBossEscape")` 为 `id = 0x203`、`size = 0x24`。

| 指令 | opcode | 说明 |
| --- | --- | --- |
| `setNext(slot, hp, time, name)` | **`0x202`** | 预注册「下一阶段」：`slot` = 槽索引（0..7）、`hp` = 触发血量、`time` = 触发时间、`name` = **下一**阶段 sub 名 |
| `setTimeout(slot, name)` | `0x203` | 超时注册（如 `BossEscape`） |

- 「下一阶段名」是**下一**个而非当前；对方案二是只读参考（本报告**不用**它，方案二用当前 sub 名）。
- 各关 boss 的 `setNext` 链（阶段推进顺序）已枚举，例：
  - ST1 `st01bs`：`Boss1→BossCard1`、`BossCard1→Boss2`、`Boss2→BossCard2`、`BossCard2→BossDead`。
  - ST2 `st02bs`：`Boss1→BossCard1→Boss2→BossCard2→Boss3→BossCard3→BossDead`。
  - ST7 `st07bs`：`Boss1→…→Boss10→BossCard10`（`END_NS/END_S` 交替）。

### 4.2 boss / 道中 boss 脚本 sub 名单（逐关）

> 均为脚本内「战斗 sub」（略去 `_at` / `_pos` / `_tri` / `_Moon` 等辅助 sub）。文件槽顺序按各关 `.decl` 头部 `ecli{}`（**逐关不同**）。

| 关 | 主 `.ecl` 的 `ecli`（slot1 起） | 道中 boss 脚本 / 战斗 sub | 关底 boss 脚本 / 战斗 sub |
| --- | --- | --- | --- |
| ST1 | `default, st01bs, st01mbs, st01mbs2` | `st01mbs`: `MBoss, MBoss1, MBossCard1, MBossDead, MBossEscape`；`st01mbs2`: `MBossB, MBossB1, MBossCardB1, MBossBDead, MBossBEscape` | `st01bs`: `Boss, Boss1, Boss2, BossCard1, BossCard2, BossDead, BossEscape` |
| ST2 | `default, st02bs, st02mbs` | `MBoss, MBoss1, MBossCard1, MBossDead, MBossEscape` | `Boss, Boss1, Boss2, Boss3, BossCard1, BossCard2, BossCard3, BossDead, BossEscape` |
| ST3 | `default, st03bs, st03mbs` | `MBoss, MBoss1, MBossCard1, MBossDead, MBossEscape` | `Boss, Boss1…Boss4, BossCard1…BossCard4, BossDead, BossEscape` |
| ST4 | `default, st04bs(2), st04mbs(3)` | `MBoss, MBoss1, MBossDead, MBossEscape` | `Boss, Boss1…Boss4, BossCard1…BossCard4, BossDead, BossEscape` |
| ST5 | `default, st05mbs, st05bs`（顺序不同！） | `MBoss, MBossDead, MBossEscape` | `Boss, Boss1…Boss5, BossCard1…BossCard5, BossDead, BossEscape, BossTrase` |
| ST6 | 无 `st06mbs` | （无道中 boss 文件） | `Boss, Boss1…Boss7, BossCard1…BossCard7, BossDead, BossEscape` |
| ST7 | `default, st07mbs, st07bs, st07bs2` | `st07mbs`: `MBoss, MBoss1…MBoss3, MBossCard1…MBossCard3, MBossDead, MBossEscape` | `st07bs`: `Boss, Boss1…Boss10, BossCard1…BossCard10, BossDead, BossDead2, BossEscape, BossTrase`；`st07bs2`: `BossB, BossB1…BossB5, BossB10, BossCardB10, BossDeadB` |

> ⚠️ **`st01mbs2` / `st07bs2` 是「第二条 boss 线」而非 st01mbs 的重复**：ST1 `main` 里先后 `enmCreateA("MBoss",…)` 与 `enmCreateA("MBossB",…)`；ST7 亦分 `st07bs`（`SetFile(3)`）与 `st07bs2`（`SetFile(4)`，`BossB…`）。thprac 各关 `ecli` 顺序逐关不同，`SetFile(N)` 落点须按各关 `.decl` 头部 `ecli{}` 读（ST5 `SetFile(3)`=st05bs、ST7 `SetFile(3)`=st07bs/`SetFile(4)`=st07bs2）。
> `st01mbs.decl:26` 可见 `setNext(1,0,1500,"MBossDead")`；`st01bs.decl:54` 可见 `setNext(0,1700,2280,"BossCard1")`（与 TH16 `st01bs` 同形）。

---

## 5. boss 行回填进展（`docs/logger/th15_checkpoints.csv`）

### 5.1 判定口径（本报告采用）

- CSV = **路过式 checkpoint（latch）白名单**：读 boss 宿主当前 sub 名，∈ 表 → 置当前段落；否则 no-op。
- boss 行的「段落入口函数」= **该阶段由 boss 脚本进入并驻留的那个战斗 sub**（顶层的 `Boss()`/`MBoss()` 只是按 `$SPELL_ID` 分派的**容器**，不作为入口）。
- 依据：`thprac_th15.cpp` 的 `THPatch`（`ecl.SetFile(N)` + `ECLJump`）+ 对应 `.decl` 的 `$SPELL_ID` 分派与 `setNext` 链。

### 5.2 已回填（高置信）

对 `thprac_th15.cpp` `THPatch`（`0x1275` 起）逐段解码 + `st01bs`/`st02bs` 的 `$SPELL_ID` 分派（`Boss()` 内 `unless($SPELL_ID…) goto …; @BossCard1()/@BossCard2()/@Boss1()`）：

| 记录名 | 入口 sub | 判据 |
| --- | --- | --- |
| ST1 BOSS1 | `Boss1` | BOSS1 无 spell 跳转，`Boss()` 在 `$SPELL_ID≈−1` 分支 `@Boss1()` |
| ST1 BOSS2 | `BossCard1` | BOSS2 `ECLJump(0x3b8→0x4a0)` 命中 `Boss()` 内 `@BossCard1()`；`setNext…"BossCard1"` |
| ST1 BOSS3 | `Boss2` | BOSS3 `pair{0x5d0,0x32}`「Change Nonspell」→ 走 `Boss2` 路径 |
| ST1 BOSS4 | `BossCard2` | BOSS4 `ECLJump(0x3b8→0x4a0)`+ `pair{0x4b0,1900}`/`{0x4d0,0x32}` → `@BossCard2()` |
| ST2 BOSS1..6 | `Boss1, BossCard1, Boss2, BossCard2, Boss3, BossCard3` | `setNext` 链 `Boss1→BossCard1→Boss2→BossCard2→Boss3→BossCard3`；BOSS2/4/6 = spell 跳转，BOSS3/5 = 「Change Nonspell」 |

已写入 CSV（**仅第 1 列**，共 10 行）：`ST1 BOSS1–4`、`ST2 BOSS1–6`。

### 5.3 暂缺（未回填，说明）

- **ST3–ST7 的 BOSS 行**：CSV 行数与脚本战斗 sub 数**不吻合**（ST3/ST4：CSV 7 行 vs `Boss1..4` + `BossCard1..4` 共 8 个；ST5：8 行 vs 10 个；ST6：11 行 vs 14 个），说明 thprac 的「实践阶段」与 boss 脚本 sub 并非简单 1:1（涉及 `$SPELL_ID`/ordinal 归并、`*_FINAL`/`BOSS2A/B` 变体），**须逐关解码 `$SPELL_ID`/ordinal 分派后才能定**，本轮不做。
- **各关 MID 行**：ST1 有三个 MID 行，而 ST1 有两条道中 boss 线（`MBoss`=st01mbs、`MBossB`=st01mbs2），且 `THPatch` MID1/MID2/MID3 的落点（`st01` `0x74d8`/`0x758c`/`0x75b4`）与 `SetFile(4)` 补丁（st01mbs2）交错，MID↔sub 归属**未定**，故不回填。
- **ST6 `STARS`、ST7 `END_NS*/END_S*`**：ST7 的 `END_NS1,END_S1,…,END_NS8,END_S8,END_S9,END_S10` 呈「无符卡/符卡交替」形态，与 `st07bs` 的 `Boss1..Boss10`/`BossCard1..BossCard10`（20 个）数量亦不符，需逐条核对（🟡）。

> ⚠️ 回填值仍属**静态推断（🟡）**：未真机采样。真机须验证「经过对应 sub 时 latch 置值正确」。

---

## 6. 风险与未验证点

1. **`host+0x11F8` 是否全程指向 boss 自己的 ECL（`stNNbs`）** 需真机核对（🟡）——本报告只证到 boss 宿主取法与指针链。
2. **`FUN_0041F7F0(slot)` 的 slot 取值**：通常 0；`0` 为 0 时应按 HUD 同序回退 1、2（`boss-phase-hook.md` §2 亦如此）。真机确认槽序。
3. **`B` 野指针不与本方案相关**：方案二每帧从 `mgr+0x180` 链现取 host，**不缓存 `B`**；但须在宿主退场（门控 `B+0x4060` bit23 清除、槽表清零）后自然失效——每帧重取即可，无需手动清理。
4. **采样时机**：异步 sub（`*_at` 等）会短暂把 `host+0xC` 指向异步线程；应在帧间稳定态采样，且这些辅助 sub 不在白名单 → no-op（与道中一致）。
5. **ST1 两条道中 boss 线（`MBoss` / `MBossB`）、ST7 双 boss 文件（`st07bs`/`st07bs2`）**：白名单须同时收录两线入口（本报告未定，见 §5.3）。
6. **变体（Steam / 汉化）RVA 未核**：`ImageBase`/`RELOCS_STRIPPED` 同，但 RVA 需按变体实测。
7. **`boss-phase-hook.md` 的 [3] 已订正**：链上对象 `E` = host，勿再取 `[E+0x44B8]`（详见 §2.8）。

---

## 7. 地址表（TH15，VA / RVA）

### 7.1 ECL 管理器与宿主

| 项 | VA | RVA |
| --- | --- | --- |
| ECL/敌机管理器 `mgr` 全局 | `0x4E9A80` | `0xE9A80` |
| `mgr+0x17C` = res（`ECLResourceInf`） | — | — |
| `mgr+0x48+slot*4` = boss 槽 → 敌机 id | — | — |
| `mgr+0x180` = 敌机（宿主）链头 | — | — |
| 宿主 spawner（`malloc(0x574C)`）`FUN_00426050` | `0x426050` | `0x26050` |
| 宿主构造器（`lea [edi+0x120c]`；`[host+0x56c4]=host`）`FUN_00426C60` | `0x426C60` | `0x26C60` |
| 宿主生成+挂链（`mgr+0x180=host+0x14f4`）`FUN_00430610` | `0x430610` | `0x30610` |
| 宿主初始化（`[host+0x11f8]=res`）`FUN_00430880` | `0x430880` | `0x30880` |
| 宿主初始化（变体）`FUN_00430910` | `0x430910` | `0x30910` |
| 宿主数量统计（读 `E+0x526c`）`FUN_004254D0` | `0x4254D0` | `0x254D0` |

### 7.2 boss 查询 / 注册（与 TH16 同源）

| 功能 | TH15 函数 | VA / RVA | TH16 对应 |
| --- | --- | --- | --- |
| **按槽取当前 boss host**（=「当前 boss」查询） | `FUN_0041F7F0` | `0x41F7F0` / `0x1F7F0` | `FUN_00417580`（`0x417580`） |
| 按 id 取 host | `FUN_004255E0` | `0x4255E0` / `0x255E0` | `FUN_0041A9C0`（`0x41A9C0`） |
| 按 id 判 host 是否存在 | `FUN_004255A0` | `0x4255A0` / `0x255A0` | （同族） |
| 写槽表 `mgr+0x48+slot*4 = host+0x5740` | `FUN_00425530` | `0x425530` / `0x25530` | — |
| 读 `mgr+0x164+slot*4` | `FUN_0041F7D0` | `0x41F7D0` / `0x1F7D0` | — |
| boss 出场/退场（门控/槽）| 内建 case | `0x42B92D` / `0x2B92D` | th16 case512（`0x420665`） |
| ↳ 门控测试（B 侧） | — | `0x42B943` / `0x2B943` | — |
| ↳ 门控置位 | — | `0x42B977` / `0x2B977` | — |
| ↳ 门控读（host 侧） | — | `0x427167` / `0x27167` | `[boss+0x4060]&0x800000` |

### 7.3 ECL 解释器（读取链）

| 项 | VA | RVA |
| --- | --- | --- |
| 字节码分发 `FUN_0048CA80` | `0x48CA80` | `0x8CA80` |
| ↳ `thread+4`（sub 索引）读取 | `0x48CAA9` | `0x8CAA9` |
| ↳ `host/res/table` 链 | `0x48CAB5` / `0x48CAC2` / `0x48CAC8` | `0x8CAB5` / `0x8CAC2` / `0x8CAC8` |
| 压帧/启子 `FUN_0048C800` | `0x48C800` | `0x8C800` |
| 每帧 tick `FUN_0048E690`（`host+0xc`=thread、`host+0x11fc` 链） | `0x48E690` | `0x8E690` |
| 操作数求值 `FUN_0048E750`（`thread+0x1018`→res） | `0x48E750` | `0x8E750` |
| SCPT 加载器 `FUN_0048F130` | `0x48F130` | `0x8F130` |
| 按名二分查表 `FUN_0048F340` | `0x48F340` | `0x8F340` |
| 当前运行地址 `FUN_0048F3D0` | `0x48F3D0` | `0x8F3D0` |
| 建 0x120C 线程 + 按名发脚本 `FUN_0048F410` | `0x48F410` | `0x8F410` |
| spawn / 克隆线程（`+0x1018`=父 host）`FUN_0048F700` | `0x48F700` | `0x8F700` |
| ECL 内建分发（host→B `add ecx,0x120c`）`FUN_004291D0` | `0x4291D0` | `0x291D0` |
| 内建 handler thunk（`[B+0x44b8]`） | `0x430320` | `0x30320` |

### 7.4 关键偏移（TH15 定案）

```text
异常口径（boss-phase-hook 用错）：host = *(E+0x44B8)
正确口径：host = E （链上对象即宿主）
          B = host + 0x120C ; *(B+0x44B8) == host  (== host+0x56C4)
门控：    B+0x4060 == host+0x526C  bit23 (0x800000) = boss 门控
id：      host+0x5740 = 敌机 id（写在 0x574C 分配内）
链节点：  host+0x14F4 (node[0]=host, node[1]=next)；mgr+0x180 = 链头
```

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | **建档**。落盘 TH15 boss 段监控「**方案二**（读 boss 宿主 ECL 当前 sub 名 + 白名单 latch）」反编译结论：§1 指针链逐字段取证；§2 **口径冲突判定** —— `mgr+0x180` 链上对象 `E` **即 host**（`FUN_00430610` 挂链 `mgr+0x180=host+0x14F4`、`FUN_004254D0` 读 `E+0x526c`、`E+0x5740` id 均在 `0x574C` 宿主区间内），`B=host+0x120C`（`FUN_00426C60` 的 `lea [edi+0x120c]`），`*(B+0x44B8)==host`（= `host+0x56C4` 自指针），故 `boss-phase-hook.md` 的 `host=*(E+0x44B8)` **错、应 `host=E`**（且 `E+0x5740` 若按 `E=B` 会越界至 `host+0x694C`）；`+0x44B8` 仅为「以 B 为 this」的指令 handler 回指（`FUN_004291D0` `add ecx,0x120c`、thunk `0x430320`）；boss 注册 `mgr+0x48+slot*4=host+0x5740`（`FUN_00425530`）。§3 当前 sub 名链复核（`thread+4`=索引，`FUN_0048F410`/`FUN_0048F700`/`FUN_0048E750` 佐证）。§4 `setNext`=`0x202`/`setTimeout`=`0x203` + 各关 boss/道中 boss sub 名单 + `st01mbs2`/`st07bs2` 说明。§5 回填 `th15_checkpoints.csv` 的 **10 行**（ST1/ST2 BOSS 行，高置信；ST3–ST7 与 MID 行暂缺并说明）。§6 风险、§7 地址表。**订正 `boss-phase-hook.md`「FUN_0041f7f0(0x48F7F0)」地址笔误为 `0x41F7F0`**。与 TH16 `FUN_00417580`/`FUN_0041A9C0` 同源同形。状态：静态调研（🟡 未真机三态）。 |

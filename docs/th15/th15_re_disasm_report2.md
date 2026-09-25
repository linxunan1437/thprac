# TH15（東方紺珠伝）miss/bomb 记录 · 第二轮反编译调查报告

> 依据：`th15_re_disasm_requests2.md`（第二轮请求）
> 对象：`th15.exe`（PE32 x86，ImageBase `0x400000`），Ghidra 12.1.2 已分析工程 `th15d`
> 配套：第一轮报告 `th15_re_disasm_report.md`；实测日志 `th15_missbomb_debug.txt`；ECL 反编译 `data/ECL/*.decl`
> 地址约定：全文为**静态基址**（= 运行时地址 − `ingame_image_base`）。

---

## 0. 结论摘要（TL;DR）

| 需求点 | 结论 |
| --- | --- |
| 问题 1：`FUN_0042ff10` 真实签名 | `void __thiscall FUN_0042ff10(void *this, int param_1, int param_2, char *param_3)`。`this = [EDI+0x44B8]`（ECL 脚本资源对象）；**param_1 = ECL 参数[0] = 常量 0（个别脚本为 1）**；param_2 = ECL 参数[2] = **血量 HP**；param_3 = 阶段名字符串指针（`"BossDead"`/`"BossEscape"` 或 `[ESI+0x20]` 名字缓冲）。**报告 §2.4「param_1=当前阶段、param_2=下一阶段」完全错误，`FUN_0042ff10` 不存在"下一攻击序号"参数** |
| 问题 2：case 0x39 全链路 | 已补全（正常分支 + BossDead 分支），见 §2。ECL 参数读取后：`param[1]` 存入 `[EDI+0x44B8 + param0*0x88 + 0x5284]`，`param[2]`(HP) 与名字经 `FUN_0042ff10` 写入 `[EDI+0x44B8 + 0x5288]` / `+0x528B`。**`0x42C7D6` 命中时 EAX = param[0] = 0/1 常量**（非序号） |
| 问题 3：稳定内存字段 | **TH15 不存在"当前攻击序号"内存字段**。`[EDI+0x4070]` 只是"boss 战斗标志"（进场 `ins_512(0)` 写 0，退场 `ins_512(-1)` 写 -1），全程恒 0（第一轮 §1.1 判定**作废**）。**唯一稳定、随攻击不变化的标识 = 攻击名字符串** `[*(g_boss_ptr+0x44B8)+0x528B]`（见 §3.2，**推荐软件开发组直接以此为主记录源**，见 §6 落地代码） |
| 问题 4：ESI 结构 | ESI = 当前攻击数据区指针，`[ESI+0x20]` = 名字/显示子缓冲（`FUN_0042ff10` 的 param_3 来源）。分支内 ESI 被改写（normal→param[1]、BossDead→param[2]），**不能作为缓存** |
| 问题 5：`0x42C7E4` | 命中时 EAX = ECL 参数[0]（=0，脚本切换目标索引），`FUN_00430000(script, param[0], pcVar37)` 切换脚本；**非攻击序号**，仅覆盖 BossDead/BossEscape 分支 |

> **核心修正**：第二轮请求 §1 的实测数据与报告 §2.4 冲突，根因有二 ——
> ① 报告 §2.4 对 `FUN_0042ff10` 参数语义标注错误（param_1 不是阶段/下一阶段）；
> ② 请求方对 hook 命中时 ESP 指向的判定偏移（EHOOK 命中时 **ESP 指向栈顶参数 param_1**，不是返回地址），导致 `[esp+4]` 读到的是 param_2(=HP) 而非 param_1。
> 因此"真实下一攻击序号在 `[esp+0x10]` 及以上或某寄存器"的推断**不成立** —— TH15 的 `0x202` 路径**根本没有序号参数**，它按名字+HP 描述每次攻击。

---

## 1. 问题 1：`FUN_0042ff10` 完整签名

### 1.1 反编译（`th15_report_raw.txt:2245`）

```c
void __thiscall FUN_0042ff10(void *this, int param_1, undefined4 param_2, char *param_3)
{
  char cVar1;
  int  iVar2;
  char *pcVar3;

  *(undefined4 *)((int)this + param_1 * 0x88 + 0x5288) = param_2;   // 写 HP
  if (param_3 != (char *)0x0) {
    pcVar3 = param_3;
    do { cVar1 = *pcVar3; pcVar3++;
         pcVar3[(int)this + (param_1*0x88 - (int)param_3) + 0x528b] = cVar1; }
    while (cVar1 != '\0');                                          // 名字串 → this+0x528B
    iVar2 = param_1 * 0x88 - (int)param_3;
    do { cVar1 = *param_3; param_3++;
         param_3[(int)this + iVar2 + 0x52cb] = cVar1; }             // 名字串 → this+0x52CB
    while (cVar1 != '\0');
    return;
  }
  *(undefined1 *)((int)this + param_1*0x88 + 0x528c) = 0;
  *(undefined1 *)((int)this + param_1*0x88 + 0x52cc) = 0;
  return;
}
```

### 1.2 调用点（case 0x39 两分支）参数映射

**调用约定**：`__thiscall`，`ECX = this = [EDI+0x44B8]`（ECL 脚本资源对象，`param_1[0x112e]`）。

**normal 分支**（0x42C7CD~0x42C7D6，见 §2）：
| 参数 | 栈位@call | 来源 | 语义 |
| --- | --- | --- | --- |
| `this` (ECX) | — | `[EDI+0x44B8]` | ECL 脚本资源对象 |
| `param_1` | `[ESP+0]`(=最后压栈 EAX) | ECL 参数[0] | **常量 0/1**（脚本标志，非序号） |
| `param_2` | `[ESP+4]` | ECL 参数[2]（局部 `[ESP+0x50]`） | **该攻击血量 HP** |
| `param_3` | `[ESP+8]` | `[ESP+0x2C]` = `&orig_ESI[0x20]` | 阶段名字符串缓冲 |

**BossDead 分支**（0x42C773~0x42C785）：
| 参数 | 栈位@call | 来源 | 语义 |
| --- | --- | --- | --- |
| `this` (ECX) | — | `[EDI+0x44B8]` | 同 |
| `param_1` | `[ESP+0]`(EAX) | ECL 参数[0] | 常量 0/1 |
| `param_2` | `[ESP+4]` | ECL 参数[2]（=ESI） | HP |
| `param_3` | `[ESP+8]` | 硬编码 `0x4CC1C0` = `"BossDead"` | 名字串 |

### 1.3 报告 §2.4 勘误

- 报告称 `FUN_0042ff10(this=脚本资源、param_1=当前阶段、param_2=下一阶段)` —— **错误**。
- 实际：`param_1` = ECL 参数[0]（常量 0/1），`param_2` = ECL 参数[2]（HP），`param_3` = 名字串。
- 请求方实测 `[esp+8]=[ESI+0x20]`、`[esp+0xC]=const`、`[esp+4]=HP 量级`，与上表完全吻合（见 §1.4 的 ESP 说明）。**`FUN_0042ff10` 中不存在任何"阶段/序号"参数。**

### 1.4 关键：EHOOK 命中时 ESP 指向栈顶参数（不是返回地址）

thprac 的 EHOOK 用 `jmp` 覆盖原 `call 0x0042ff10`（5 字节）。`call` 未执行 → **返回地址未被压栈**，命中时 `ESP` 指向**最后压栈的实参 = param_1**（不是请求方假设的返回地址）。

因此请求方 `th15_missbomb_debug.txt` 的转储应按下表解读（每行 = 相对栈顶参数）：

| 请求方标签 | 实际含义 | 实测值 | 对应 |
| --- | --- | --- | --- |
| `[esp+0]`（未读） | **param_1 = ECL 参数[0]** | = EAX = 0/1 | 常量 |
| `[esp+4]` | param_2 = ECL 参数[2] | 2400/2100/2280/1500/3000 | **HP** |
| `[esp+8]` | param_3 = `&orig_ESI[0x20]` | ~0x10CABD9C 堆指针 | 名字缓冲 |
| `[esp+0xC]` | 栈下局部（本函数栈） | 0xE48C0BF1 常量 | 无关 |

> 结论：请求方把 `[esp+4]` 当"阶段"是**读错位** —— 它其实是 param_2（HP）。真正的 param_1（参数[0]，常量 0/1）在 `[esp+0]`，从未被读。

---

## 2. 问题 2：case 0x39（opcode 0x202 = `ins_514`）全链路

`FUN_004291e0`（ECL 解释器）case 0x39，入口 `0x42C735`，区间 `0x42C735~0x42C7F9`（`th15_report_raw.txt:474-527`）：

```
0042c735  lea eax,[esi+0x20]
0042c738  mov [esp+0x2c],eax          ; HOOK(lock_timer2,4B)  ; [esp+0x2c]=&orig_ESI[0x20]
0042c73c  mov eax,[0x4e7794]
0042c741  and al,0x30
0042c743  cmp al,0x20                 ; 游戏态==0x20？
0042c745  jnz 0x42c791                ; 否 → normal 分支
0042c747  test dword [edi+0x4060],0x800000   ; boss 攻击中？
0042c751  jz  0x42c791                ; 否 → normal 分支
;------------------- BossDead 分支（boss 死亡/换阶段收尾）-------------------
0042c753  push 0x2
0042c755  mov ecx,edi
0042c757  call 0x00430320             ; EAX = ECL 参数[2] = HP
0042c75c  push 0x0
0042c75e  mov ecx,edi
0042c760  mov esi,eax                 ; ESI = 参数[2] = HP
0042c762  call 0x00430320             ; EAX = ECL 参数[0] = 0/1
0042c767  mov ecx,[edi+0x44b8]
0042c76d  imul edx,eax,0x88
0042c773  push 0x4cc1c0               ; "BossDead"
0042c778  push esi                    ; param_2 = HP
0042c779  push eax                    ; param_1 = 参数[0]
0042c77a  mov [edx+ecx+0x5284],0x0    ; 清当前攻击槽
0042c785  call 0x0042ff10             ; FUN_0042ff10(this,param[0],HP,"BossDead")
0042c78a  push 0x4cc1cc               ; "BossEscape"
0042c78f  jmp 0x42c7e4                ; → 切脚本
;------------------- normal 分支（普通换攻击：非符→非符/符卡）-------------------
0042c791  push 0x2
0042c793  mov ecx,edi
0042c795  call 0x00430320             ; EAX = 参数[2] = HP
0042c79a  push 0x1
0042c79c  mov ecx,edi
0042c79e  mov [esp+0x50],eax          ; 局部保存 参数[2]=HP
0042c7a2  call 0x00430320             ; EAX = 参数[1]
0042c7a7  push 0x0
0042c7a9  mov ecx,edi
0042c7ab  mov esi,eax                 ; ESI = 参数[1]（0/HP）
0042c7ad  call 0x00430320             ; EAX = 参数[0] = 0/1
0042c7b2  mov ecx,[edi+0x44b8]
0042c7b8  imul edx,eax,0x88
0042c7be  mov [edx+ecx+0x5284],esi    ; 槽[参数0] = 参数[1]
0042c7c5  test esi,esi
0042c7c7  js  0x42dba7                ; 参数[1]<0 → 结束（不进 FUN_0042ff10）
0042c7cd  push dword [esp+0x2c]       ; param_3 = &orig_ESI[0x20]（名字缓冲）
0042c7d1  push dword [esp+0x50]       ; param_2 = 参数[2] = HP
0042c7d5  push eax                    ; param_1 = 参数[0]（EAX 未被改写）
0042c7d6  call 0x0042ff10             ; <<<<< 既有钩子 boss_setnext
0042c7db  jmp 0x42dba7
;------------------- 共享脚本切换点（case 0x40 / BossDead 共用）-------------------
0042c7e0  lea eax,[esi+0x18]
0042c7e4  push 0x0                    ; (BossDead 分支经 0x42c78f 跳到此处)
0042c7e6  mov ecx,edi
0042c7e8  call 0x00430320             ; EAX = 参数[0]
0042c7ed  mov ecx,[edi+0x44b8]
0042c7f3  push eax                    ; arg = 参数[0]
0042c7f4  call 0x00430000             ; FUN_00430000(script, param[0], pcVar37) 切脚本
0042c7f9  jmp 0x42dba7
```

### 2.1 ECL 参数读取函数

```c
// FUN_00430320(this, i) = 读 ECL 参数[i]（this = EDI = boss 对象）
void __thiscall FUN_00430320(void *this, int param_1) {
  FUN_0048e750(*(int *)(*(int *)((int)this + 0x44b8) + 0xc), param_1);  // EAX = 参数[param_1]
}
```

### 2.2 "下一攻击序号"的真相（重要）

- **0x202 参数[0] 恒为 0（个别 1）**，不是序号。`0x42C738`（lock_timer2）命中时 EAX 尚未读参数；`0x42C7D6`（boss_setnext）命中时 **EAX = 参数[0] = 0/1 常量**。请求方实测 `EAX=0 或 1` 正是这个常量。
- 请求方 `th15_missbomb_debug.txt` 中 normal 分支 `ESI=0`（=参数[1]，0/HP）、BossDead 分支 `ESI=0x898...`（=参数[2]=HP），均与上表一致。
- **结论：0x202 路径不存在"下一攻击序号"**。TH15 用「名字串 + HP」描述每次攻击，无单调序号。

### 2.3 可挂接点（供 EHOOK）

| 地址 | 指令长度 | 命中时可用值 | 覆盖分支 | 说明 |
| --- | --- | --- | --- | --- |
| `0x42C7D6` | 5B（`call 0x0042ff10`） | EAX=参数[0]；`[ESP+8]`=名字缓冲；`[ESP+4]`=HP | **仅 normal** | 既有钩子，可读名字/HP，但无序号 |
| `0x42C785` | 5B（`call 0x0042ff10`） | EAX=参数[0]；`[ESP+8]`="BossDead"；`[ESP+4]`=HP | **仅 BossDead** | 补充 BossDead 覆盖 |
| `0x42C7F4` | 5B（`call 0x00430000`） | EAX=参数[0]=0；参数[2] 在 `[ESP+0x2C]` | BossDead + case 0x40 | 脚本切换点，无序号 |

> 因无序号参数，**任何单点都拿不到"攻击序号"**；如需序号必须自维护（见 §3.3）。

---

## 3. 问题 3：稳定保存"当前攻击"的内存字段

### 3.1 结论：TH15 无"当前攻击序号"字段

- `[EDI+0x4070]`（第一轮 §1.1 判定为"当前攻击序号"）**作废**。经 ECL 脚本确认，其唯一写入点是 case 0x37（opcode 0x200 = `ins_512`）：
  - boss 进场 `ins_512(0)` → `[EDI+0x4070]=0`
  - boss 退场 `ins_512(-1)` → `[EDI+0x4070]=-1`，清战斗标志
  - **boss 战内的每次换攻击走 opcode 0x202（`ins_514`），不写 `[EDI+0x4070]`** → 全程恒 0。请求方实测「恒为 0」正确。
- 全 exe 对 `[EDI+0x4070]` 的写仅 `0x42B987` 一处；读在 `0x42B94F`、`0x42C905`、`0x42CAA3`、`0x42D70C`（多为战斗标志判断）。**不存在"随攻击递增的序号写入"**。
- 候选逐一否定：
  - `[EDI+0x4070]`：恒 0，已否。
  - `[EDI+0x3F80]`（=参数[0]*7，case 0x36 写）：HP 相关，攻击中随血量变化，已否（请求方实测正确）。
  - `0x4E9A80` 槽数组：`DAT_004e9a80+0x48+序号*4` 按"序号"索引，但该"序号"正是上面不存在的攻击序号，**需先有序号**，不成立。
  - ESI 指向的当前攻击数据区：分支内被改写，不能缓存（见 §4）。

### 3.2 唯一稳定的攻击标识：名字符串（内存字段）【推荐给软件开发组的主记录源】

> **结论：TH15 唯一稳定、可直接从 `g_boss_ptr` 读到的"当前攻击"标识就是名字符串。建议软件开发组直接用它做记录，不必自维护计数器。**

每次 `0x202` 都会把**本次攻击的名字串**经 `FUN_0042ff10` 写入（`param_3` → `this+0x528B` 与 `+0x52CB`，`this = [EDI+0x44B8]`）：

```
写入点：0x42C7D6 / 0x42C785 → FUN_0042ff10（每次换攻击写一次）
[ *(g_boss_ptr + 0x44B8) + 0x528B ]  = 当前攻击名字 C 串（如 "Boss1"、"BossCard1"…，含结尾 '\0'）
[ *(g_boss_ptr + 0x44B8) + 0x52CB ]  = 同上（第二份副本）
[ *(g_boss_ptr + 0x44B8) + 0x5288 ]  = 当前攻击 HP（dword）
```

- **稳定性**：名字在攻击进行中**不变化、不随血量变化**（`FUN_0042ff10` 只在换攻击时写一次，攻击中无其它写入点），满足"miss/bomb 读到的就是当时正在进行的攻击"要求。
- **可达性**：`[EDI+0x44B8]`（ECL 脚本资源指针）在整个 boss 战期间保持分配，仅凭缓存的 `g_boss_ptr`(=EDI) 即可到达，无需知道 ESI/序号。
- **有效性判定**：攻击中名字非空且以 `'\0'` 结尾；boss 战外（进场前/退场后）该缓冲可能为空串或残留上一场名字，**记录时须配合 §1.4 的 boss 战标志判定**（`[g_boss_ptr+0x4060] & 0x800000`，或 §3.3 进场/退场复位）。
- **完整读取表达式（miss/bomb 时）**：
  ```cpp
  // g_boss_ptr 由 ECL 钩子（0x42B938/0x42C738/0x42B261 任一）缓存 EDI
  char stage_name[64];
  if (g_boss_ptr && (*(uint*)(g_boss_ptr + 0x4060) & 0x800000)) {
      int script = *(int*)(g_boss_ptr + 0x44B8);
      strncpy_s(stage_name, sizeof(stage_name),
                (const char*)(script + 0x528B), sizeof(stage_name)-1);  // 当前攻击名
  } else {
      stage_name[0] = '\0';   // 不在 boss 战中
  }
  ```
- **名字取值（各关 boss 阶段名）**：来自 ECL 脚本 `ins_514` 的第 4 参，见 `data/ECL/st03bs.decl`、`st05bs.decl`、`st06bs.decl`（及 `st07bs.decl`）。典型序列（ST3）：
  ```
  "Boss1"→"BossCard1"→"Boss2"→"BossCard2"→"Boss3"→"BossCard3"→"Boss4"（→"BossDead"收尾）
  ```
  名字 = 阶段序号的自然标识：`"BossN"` = 第 N 个非符，`"BossCardN"` = 第 N 张符卡。
- **局限**：名字是**字符串**且跨关不唯一（不同关都有 `"Boss2"`）。作为记录字段可**直接落盘名字**，或由 thprac 在启动时从 `data/ECL/*.decl` 建表把 `关卡+名字 → 统一序号` 映射。若仅需"当时第几非符/第几符卡"，`BossN/BossCardN` 后缀数字即可区分，无需映射。

### 3.3 推荐：自维护计数（满足"序号"需求）

因无内存序号字段，**最稳妥做法是 thprac 自维护一个计数器**：

1. 缓存 `g_boss_ptr = EDI`（沿用第一轮方案，在 `0x42B938`/`0x42C738`/`0x42B261` 任一回调缓存）。
2. 每次换攻击 +1：挂 **`0x42C7D6`（normal）** 与 **`0x42C785`（BossDead）** 两处（互斥，覆盖全部 `0x202` 换攻击），命中即 `g_attack_idx++`。也可直接挂 `FUN_0042ff10` 入口。
3. boss 进场（`0x42B938` 命中、参数[0]=0）时 `g_attack_idx = 0`；退场（参数[0]<0）时置 -1。
4. miss/bomb 时读 `g_boss_ptr ? g_attack_idx : -1`。

---

## 4. 问题 4：ESI（case514 命中时）指向的结构体

- **语义**：case 0x39 入口 `ESI` = "当前攻击数据区"指针（`FUN_004291e0` 内由外层设置，指向本 boss 当前 ECL 指令/攻击上下文）。
- **`[ESI+0x20]`**：指向一个名字/显示子缓冲（`LEA [ESI+0x20]` 后作为 `FUN_0042ff10` 的 param_3 名字来源）。请求方实测其为堆地址 `~0x10CABD9C`，即该子结构指针。
- **是否含"模式序号"字段**：否。分支内 `ESI` 被立即改写（normal → 参数[1]=0/HP，BossDead → 参数[2]=HP），且 0x202 本就无序号参数。**`ESI` 不能作为缓存**。
- **与 boss 对象 EDI 的关系**：`ESI` 由解释器局部维护，无固定全局；无法仅凭 EDI 或全局可靠到达 ESI 所指对象。**弃用 ESI，改用 §3.2 的名字字段或 §3.3 自维护计数。**

---

## 5. 问题 5（备选挂接点）：`0x42C7E4`

- `0x42C7F4 call 0x00430000` 命中时（BossDead 分支跳来 + case 0x40 共用）：
  - `EAX = ECL 参数[0]`（=0，`FUN_00430320` 于 `0x42C7E8` 返回）
  - `FUN_00430000(ECX=[EDI+0x44B8], arg=参数[0], pcVar37)` 真正切换脚本；`pcVar37 = "BossEscape"`（BossDead 分支）或脚本名（case 0x40）。
- **不是攻击序号**（参数[0]=0）。此点仅覆盖 BossDead / case 0x40 分支，**不覆盖 normal 分支**（normal 走 `0x42C7DB → 0x42DBA7`，不经 `0x42C7E4`）。作为"换脚本/收尾"事件有用，作"换攻击序号来源"**不可用**。

---

## 6. 交付汇总（挂接点）

| 用途 | 地址 | 长度 | 读取表达式 | 局限 |
| --- | --- | --- | --- | --- |
| **攻击名字（稳定内存，主记录源）** | — | — | `(char*)(*(int*)(g_boss_ptr+0x44B8)+0x528B)` | 字符串，跨关不唯一；需 g_boss_ptr |
| 换攻击事件 + 自维护序号 | `0x42C7D6` | 5B | 命中 `g_attack_idx++`；HP=`[ESP+4]`，名字=`[ESP+8]` | 仅 normal 分支 |
| 同上（BossDead） | `0x42C785` | 5B | 命中 `g_attack_idx++` | 仅 BossDead 分支（两处互斥，需都挂才全覆盖） |
| boss 进场/退场复位 | `0x42B938` | 2B | 参数[0]>=0 → 计数归零；<0 → -1 | 既有钩子，可直接用 |
| （备选）脚本切换 | `0x42C7F4` | 5B | 仅收尾事件，无序号 | 不覆盖 normal |

**miss/bomb 时（软件开发组落地，推荐方案）**：直接读内存名字字段，无需任何计数器。
```cpp
int   stage   = *(int*)RVA(0x4E73F0);                    // 关卡号
int   inBoss  = g_boss_ptr && (*(uint*)(g_boss_ptr+0x4060) & 0x800000);
char  name[64] = {0};
if (inBoss) strncpy_s(name, sizeof(name), (char*)(*(int*)(g_boss_ptr+0x44B8)+0x528B), sizeof(name)-1);
// 落盘：stage, name（如 "BossCard2"），可另存 HP=*(int*)(*(int*)(g_boss_ptr+0x44B8)+0x5288)
```
自维护计数（§3.3）仅在需要**跨关统一的数字序号**时才用；默认不必要。

---

## 7. 与请求方实测数据的核对

| 请求方观测 | 本报告解释 | 一致？ |
| --- | --- | --- |
| `[esp+4]` = 2400/2100/... 跨 boss 重复、疑似 HP | 是 **param_2 = ECL 参数[2] = 该攻击 HP**（EHOOK 命中时 ESP 指向栈顶参数，`[esp+4]`=param_2） | ✓ |
| `[esp+8]` = `[ESI+0x20]` 堆指针 | 是 **param_3 = &orig_ESI[0x20]**（名字缓冲），与 `FUN_0042ff10` param_3 对应 | ✓ |
| `[esp+0xC]` = 0xE48C0BF1 恒定 | 本函数栈下局部（更早的局部变量），与参数无关 | ✓ |
| `EAX` = 0 或 1 | = **param_1 = ECL 参数[0]**（常量 0/1，非阶段） | ✓ |
| normal 分支 `ESI`=0、BossDead `ESI`=0x898... | normal→参数[1]=0；BossDead→参数[2]=HP（各自分支内的 ESI 被改写） | ✓ |
| `[edi+0x4070]` 恒 0 | case 0x37 只在进场写 0、退场写 -1；换攻击走 0x202 不写 | ✓ |
| `[edi+0x3F80]` 攻击中易变 | case 0x36 写 `参数[0]*7`，参数[0] 为 HP 相关 | ✓ |
| "下一攻击序号应在 `[esp+0x10]` 及以上" | **不成立** —— 0x202 无序号参数 | ✗（修正） |

---

## 附录：证据文件

- `th15_report_raw.txt`：`FUN_0042ff10`(L2245)、`FUN_00430320`(L2219)、case 0x39 反汇编(L474~527)、case 0x37 反汇编(L303~334)
- `th15_ghidra_dump.txt`：case 0x39 C 反编译(L1454~1475)、case 0x37(L1435~1451)、`[EDI+0x4070]` 用途 case 0xa7(L2860~2877)
- `data/ECL/*.decl`：`ins_512`（0x200）仅进场/退场调用、`ins_514`（0x202）每攻击调用且 `参数[0]` 恒 0/1

*创建：2026-08-19*

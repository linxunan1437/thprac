# TH14 Boss 段监控 — 方案二（读 boss 宿主 ECL 当前 sub 名 + 白名单 latch）逆向报告

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志）
> 总括（术语口径以此为准）：`docs/logger/logger_re_disasm_overview.md`
> **模板 / 上位文档**：`docs/th16/th16_boss_phase_report.md`（TH16 boss 段监控「方案一 / 方案二」；本报告采用其 **方案二**，并把它搬到 TH14）
> 同代主线参照：`docs/th16/th16_ecl_re_report.md`（TH16 指针链）、`docs/th15/th15_ecl_re_report.md`、`../th15decode/boss-phase-hook.md`
> 本作已定案：`docs/th14/th14_ecl_re_report.md`（TH14 道中 / Q1–Q9；**boss 段未覆盖**，本报告是其「boss 段」续篇）
> 素材清单与来源：`docs/logger/decompile_materials.md`
> 回填工作流：`docs/logger/checkpoint_entry_backfill_workflow.md`（§3.4 boss 行）
> 二进制指纹：`../exes/th14.exe`，**897536 B**，PE `TimeDateStamp = 1376634201`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；`.text` = VA `0x401000`–`0x4B0F3C`，RVA `0x1000`–`0xB0F3C`）
> 引擎代：**C 代（第四代引擎）**；同代参照作 th15 / th16。
> 方法：`python3 (3.9.6)` + `pefile 2024.8.26` + `capstone 5.0.7` 只读反汇编；`RELOCS_STRIPPED` ⇒ 用「4 字节小端 VA 立即数」字节搜索定位 xref；
> 离线素材 `../exes/ecl_dump/th14/`（`thecl -d` 反编译 `.decl` 24 份）、`../exes/ecl/th14/`（原始 `.ecl` 24 份），并自写 SCPT 解析器对 `.ecl` 逐指令解码。
> 状态：**静态调研（🟡 未真机三态）**。本机 macOS 不可编译、不可真机；未真机验证的地址 / 结论文中均标 🟡。
> 地址口径：正文以 **VA** 为主，并同时标注 **RVA = VA − 0x400000**。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **boss ECL 跑在哪** | ✅（静态）**boss 敌人自己的宿主**里，与道中主线宿主**不是同一个对象** | 与 TH16 同结论；TH14 的宿主分两类（见 §2.2） |
| **boss 宿主取法** | ✅ **`mgr=*(u32*)0x4DB544`；`host = *(u32*)(mgr + 0x5C + slot*4)`**（槽 0..7，boss 用槽号记在 `host+0x5254`） | **本作「槽表直接存宿主指针」**，不像 TH16 存 id 再遍历链表，**无需 id 匹配** |
| **是否存在 TH16 的 `mgr+0x48` / `mgr+0x180` / `E+0x5740` / `E+0x44B8`** | ❌ / 变换 | TH14：槽表在 **`mgr+0x5C`**（不是 +0x48）；敌人链在 **`mgr+0xD0`**（双向链，不是 +0x180 单链）；**全 `.text` 无 `0x5740`、无 `0x44B8`** |
| **按槽 / 按 id 查对象函数** | ✅ | 按槽：`FUN_0041C050`（`0x41C050`，直接返回 `[mgr+0x5c+slot*4]`）；按 id：`FUN_00421AA0`（`0x421AA0`，链上比 `[obj+0x11E4]`） |
| **boss 门控位** | ✅ **`[host + 0x5244] & 0x800000`**（bit23） | 与 TH16 `[boss+0x4060]&0x800000` 同义；由 ECL `setBoss`（opcode **`0x200`**）置 / 清 |
| **游戏自身「取当前 boss」** | ✅（近似）**槽 0** | 游戏内部大量 ECL 指令直接读 `*(u32*)(mgr+0x5c)`（= 槽 0）当「当前 boss」；`setBoss` 负责登记。TH15 的 `FUN_0041F7F0`（按槽 + id 匹配）在 TH14 对应 `FUN_0041C050`（按槽直取） |
| **当前 sub 名读取链** | ✅（沿用 th14 主线结论） | **TH14 无 sub 索引**：`host → node=*(u32*)(host+4) → ip=*(u32*)(node+4) → res=*(u32*)(host+0x11CC) → sub 表 → 按 ip 落区间取名` |
| **ip→sub 映射在 boss 场景是否成立** | ✅（静态）成立 | boss 脚本（`stNNbs` / `stNNmbs`）与道中脚本**共用同一个全局 `res`**（`mgr+0xCC`），sub 表含全部文件的 sub；按 `table[i].+4` 升序即得代码序区间 |
| **boss 行回填** | 🟡 部分完成 | `docs/logger/th14_checkpoints.csv` **72 行 boss 行中回填 64 行**；ST4（8 行，A/B 双 boss 名不同，CSV 单列不足）留空 |

> **一句话**：TH14 取 boss 宿主比 TH16 **更直接**——`mgr+0x5C` 槽表里存的就是 boss 的 ECL 宿主对象本身（由 ECL 指令 `setBoss(slot)` 登记，同时置 `host+0x5244` bit23 = boss 门控）。取到 `host` 后，**当前 sub 名仍按 th14 主线的「ip→sub 区间映射」读**（th14 无 sub 索引，这一点与 th16 不同）。

---

## 1. 方案二机制（TH16 → TH14 对照）

### 1.1 方案二回顾（见模板 §3）

```text
# TH16 版
mgr   = *(u32*)0x4A6DC0
host  = 按槽位 FUN_00417580 / 按 id FUN_0041A9C0 取得（mgr+0x48+slot*4 存 id；遍历 mgr+0x180 链比 E+0x5740）
thread= *(u32*)(host + 0x0C)
subIdx= *(i32*)(thread + 4)
res   = *(u32*)(host + 0x11F8)
table = *(u32*)(res + 0x8C)
name  = *(char**)(table + subIdx*8)
```

### 1.2 TH14 对照表（本报告结论）

| 环节 | TH16 | TH14（本作实际） | 备注 |
| --- | --- | --- | --- |
| 管理器全局 | `0x4A6DC0` | **`0x4DB544`** | 管理器对象由 `FUN_004222F0`（`0x4222F0`）构造，构造末尾 `mov [0x4DB544],esi`（`0x42231A`）；析构置 0（`0x422821`） |
| 槽→对象表 | `mgr+0x48+slot*4`（存 **id**） | **`mgr+0x5C+slot*4`（存 **宿主对象指针**）** | 8 槽（`0x5C..0x7B`；循环计数 `mov ecx,8` @ `0x42C549`） |
| 敌人 / 宿主链头 | `mgr+0x180`（单链，node{+0,+4}） | **`mgr+0xD0`**（**双向**链，node 内嵌于对象 `obj+0x14D8`；`+0xD4`=尾、`+0xD8`=计数） | 见 §2.3 |
| 按槽查对象 | `FUN_00417580` | **`FUN_0041C050`**（`0x41C050`） | 直接返回 `[mgr+0x5c+slot*4]` |
| 按 id 查对象 | `FUN_0041A9C0` / `FUN_0041F7F0` | **`FUN_00421AA0`**（`0x421AA0`，返回对象）/ `FUN_00421A60`（`0x421A60`，存在性） | id 在 `obj+0x11E4` |
| 宿主自指针 | `*(B+0x44B8)==host`（`B=host+0x120C`） | **`*(obj+0x52DC)==obj`** | 写点 `0x422AF1`；`obj+0x120C` 处**无**自指针（TH14 布局不同） |
| boss 门控 | `[boss+0x4060] & 0x800000` | **`[obj+0x5244] & 0x800000`** | 相对 `obj+0x11F0` 子对象即 `+0x4054`（与 TH16 的 `+0x4060` 仅差 0xC） |
| 当前 sub | `thread+4`（**sub 索引**） | **无 sub 索引**：`node+4` = **当前指令指针（绝对 VA）**，需 **ip→sub 映射** | 见 §3 |
| res | `*(host+0x11F8)` | **`*(host+0x11CC)` = `*(u32*)(mgr+0xCC)`** | 全局唯一 res（含全部 ecl 文件） |
| sub 表 | `res+0x8C`，项 8B | 同（`res+0x8C`，项 8B：`+0`=名字 `char*`、`+4`=sub 数据绝对地址，首指令 `+4+0x10`） | 与 `docs/th14/th14_ecl_re_report.md` §3 一致 |
| 命中点 | `0x473995`（`Edi`=thread） | **`0x483C4F`**（`Edi`=node） | 4B `movss [edi],xmm0` |

> 关键差异（务必注意）：**TH14 没有 sub 索引**。TH16 的「读当前 sub 索引」在 TH14 **不存在**，取而代之的是 **`node+4` 的绝对指令指针 + 区间映射**（详见 `docs/th14/th14_ecl_re_report.md` §2/§10.3）。

---

## 2. boss 宿主取法

### 2.1 管理器与「槽表」（**核心**）

管理器对象指针存于全局 **`0x4DB544`**（`RVA 0xDB544`）：

- 构造 `FUN_004222F0`（`0x4222F0`）：`0x42231A: mov [0x4DB544],esi`（对象 0xE8 级头部）；析构 `0x422821: mov dword [0x4DB544],0`。
- **槽表 = `mgr + 0x5C`，8 项 × 4B**（`0x5C..0x7B`）。

直接证据（`0x42C540` 开头，8 槽循环）:

```asm
0042c540  55                push ebp
0042c541  8bec              mov ebp, esp
0042c545  8d595c            lea ebx, [ecx + 0x5c]     ; ebx = mgr + 0x5C（槽表基址）
0042c549  b908000000        mov ecx, 8                ; ★ 8 槽
0042c552  8b03              mov eax, [ebx]            ; eax = slot[i]
...
0042c606  83c304            add ebx, 4                ; 下一槽
0042c609  49                dec ecx
0042c60d  0f853fffffff      jne 0x42c552
```

- **按槽取对象** `FUN_0041C050`（`0x41C050`，`__cdecl`，`ret 4`）：

```asm
0041c050  55                push ebp
0041c051  8bec              mov ebp, esp
0041c053  8b4d08            mov ecx, [ebp + 8]        ; ecx = slot
0041c056  a144b54d00        mov eax, [0x4db544]       ; eax = mgr
0041c05b  8b44885c          mov eax, [eax + ecx*4 + 0x5c]  ; ★ host = slot[slot]
0041c05f  5d                pop ebp
0041c060  c20400            ret 4
```

- **按槽写对象**（`0x421A10`，`ret 8`）：`0x421A1E: mov [eax+ecx*4+0x5c], edx`。

> ⚠️ 与 TH16 的**关键区别**：TH16 的 `mgr+0x48` 存的是 **host 的 id**，需再遍历 `mgr+0x180` 链比对 `E+0x5740`；**TH14 的 `mgr+0x5C` 直接存宿主对象指针**，故取 boss 宿主**一步到位**。

### 2.2 宿主分两类（确认 boss 用自己的宿主）

TH14 有两类带 ECL 宿主头的对象（都以 `res` 在 `+0x11CC`、node 在 `+0x04/+0x08` 为特征）：

| 类别 | 尺寸 / vtable | 创建点 | 用途（🟡 部分未定） |
| --- | --- | --- | --- |
| **A：ECL 宿主基类**（瘦） | `0x11E0`，vtable `0x4BDEA4` | 工厂 `FUN_00484940`（`0x484940`），唯一调用点 `0x458839` | 由「关卡 ECL 场景对象」持有（`scene+0x2DC`，写点 `0x45883E`）；其 vtable method0 = `0x420F40`（`xor eax,eax; ret`）⇒ **不能执行 opcode ≥ 0x12C 的指令** |
| **B：敌人 / boss 宿主类**（胖） | **`0x530C`**，vtable **`0x4BDE7C`** | 工厂 `FUN_00421EF0`（`0x421EF0`，`0x421EFE: push 0x530C`），ctor `FUN_00422AB0`（`0x422AB0`） | 道中敌机、midboss、boss **全部**；vtable method0 = 转发到 **ECL 指令大 switch `0x425270`** |

- `0x530C` 类 ctor 关键片段（`FUN_00422AB0`）：

```asm
00422ab5  8bf9              mov edi, ecx              ; edi = obj
00422ab7  e894e6ffff        call 0x421150             ; ★ 内嵌 ECL 宿主 ctor（vtable 0x4BDEA4 → 随后覆盖）
00422abc  8db7f0110000      lea esi, [edi + 0x11f0]   ; 子对象（承载 0x12C+ 指令 switch 的状态）
00422ac4  c7077cde4b00      mov dword ptr [edi], 0x4bde7c  ; ★ vtable = 0x4BDE7C
...
00422af1  89bfdc520000      mov [edi + 0x52dc], edi   ; ★ 自指针 obj+0x52DC = obj
...
00422df6  8b89cc000000      mov ecx, [ecx + 0xcc]     ; ecx = mgr → res
00422dfc  898fcc110000      mov [edi + 0x11cc], ecx   ; ★ obj+0x11CC = res
00422e02  e8a91a0600        call 0x4848b0             ; 按名解析（name = ctor 的 [ebp+8]）
00422e07  8b4f04            mov ecx, [edi + 4]        ; ecx = obj+4 = node
00422e0a  894104            mov dword ptr [ecx + 4], eax   ; ★ node+4 = 首指令地址
```

- ⇒ **敌人 / boss 就 = B 类对象**，即「`obj+0x11CC` = 全局 `res`、`obj+0x04 → node → node+4 = ip`」的宿主。这正是 th14 主线报告 §1 的 `host`（`node+0x1014 = host`，`host+8` 内嵌主线 node；此处 `node = obj+8`）。

> 结论：**读 boss 的当前 sub，必须取 B 类对象**（A 类不是 boss）；boss 恰好被 `setBoss` 登记进 `mgr+0x5C`。

### 2.3 敌人链 `mgr+0xD0`（备用取法）与 id

- 新敌人对象创建后挂入 **`mgr+0xD0`**（链表节点内嵌于 `obj+0x14D8`，双向：`+4`=next、`+8`=prev；`mgr+0xD4`=尾、`mgr+0xD8`=计数）：

```asm
00422181  83bfd000000000    cmp dword ptr [edi + 0xd0], 0   ; edi = mgr
00422190  8d86d8140000      lea eax, [esi + 0x14d8]         ; node = obj + 0x14D8
00422196  8987d4000000      mov [edi + 0xd4], eax
0042219c  8987d0000000      mov [edi + 0xd0], eax           ; ★ 链头 = obj+0x14D8
```

- 遍历 / 更新循环 `0x424CB0`：`edi=[mgr+0xd0]; ebx=[edi+4]; edi=[edi]` ⇒ `node[+0]=obj`、`node[+4]=next`。
- **按 id 查对象** `FUN_00421AA0`（`0x421AA0`）：

```asm
00421ab2  a144b54d00        mov eax, [0x4db544]       ; mgr
00421ab7  8b80d0000000      mov eax, [eax + 0xd0]     ; 链头
00421ac1  8b08              mov ecx, [eax]            ; ecx = obj
00421ac3  3991e4110000      cmp [ecx + 0x11e4], edx   ; ★ obj+0x11E4 == id ?
00421acb  8b4004            mov eax, [eax + 4]        ; next
```

- **敌机 id** 在 `obj+0x11E4`，取自管理器计数器 `mgr+0xDC`（分配点 `0x422DAB: mov eax,[ecx+0xdc]; mov [edi+0x11e4],eax`）；id getter `FUN_00421800`（`0x421800`）。

> 🟡 TH14 **无**「槽→id 表 + 按 id 遍历」的组合（TH16 的 `mgr+0x48`+`mgr+0x180`+`E+0x5740`）；本作是「**槽→宿主指针**」（`mgr+0x5C`）与「**实体双向链**」（`mgr+0xD0`，含全部敌机而非仅宿主）两套。

### 2.4 boss 门控与登记（ECL 指令 `setBoss`）

- **ECL 指令 `setBoss(slot)` = opcode `0x200`**（dispatch case `0x42787D`；由 `0x425270` 大 switch 的 opcode 映射表确认：opcode `0x200 → 0x42787D`）。

```asm
; setBoss(slot >= 0)：登记 + 置门控
004278c1  a144b54d00        mov eax, [0x4db544]            ; mgr
004278c6  8b8fec400000      mov ecx, [edi + 0x40ec]        ; edi = obj+0x11F0 ⇒ [obj+0x52DC] = obj（自指针）
004278cc  818f5440000000008000 or dword ptr [edi + 0x4054], 0x800000  ; ★ obj+0x5244 |= 0x800000（boss 门控）
004278d6  894cb05c          mov [eax + esi*4 + 0x5c], ecx  ; ★ mgr+0x5C+slot*4 = obj
004278da  89b764400000      mov [edi + 0x4064], esi        ; ★ obj+0x5254 = slot（记录槽号）
```
```asm
; setBoss(slot < 0)：退场 + 清门控
00427893  f7875440000000008000 test dword ptr [edi + 0x4054], 0x800000
0042789f  8b8f64400000      mov ecx, [edi + 0x4064]        ; obj+0x5254 = slot
004278a5  a144b54d00        mov eax, [0x4db544]
004278aa  c744885c00000000  mov dword ptr [eax + ecx*4 + 0x5c], 0  ; 清槽
004278b2  81a754400000ffff7fff and dword ptr [edi + 0x4054], 0xff7fffff  ; 清 bit23
```

- **门控位确认**（`0x4286C9`，`edi=obj+0x11F0`）：

```asm
004286c9  f7875440000000008000 test dword ptr [edi + 0x4054], 0x800000   ; ([obj+0x5244] & 0x800000)
```
- 旁证：`BossDead()` / `BossEscape()` 末尾调 `setBoss(-1)`（`st01bs.ecl` sub `BossDead` 内 ins @ `0x3E88`、`BossEscape` 内 @ `0x3FC8`，均 `id=0x200, p=[0,-1]`）。
- 另有 `0x42C540`（把某宿主 node 置为 `"BossDead"`）/ `0x42C620`（置 `"BossEscape"`）——游戏自身「boss 死 / 逃」的脚本切换函数（字面量 `"BossDead"` = `0x4BDE2C`、`"BossEscape"` = `0x4BDE38`）。

### 2.5 boss 宿主取法结论（可直接用）

```cpp
// 取「当前 boss 宿主」（slot = 0..7；boss 自身记录在 host+0x5254）
uint32_t mgr  = *(uint32_t*)0x4DB544;                 // ECL 管理器
if (!mgr) return nullptr;
uint32_t host = *(uint32_t*)(mgr + 0x5C + slot*4);    // ★ boss 宿主（0x530C 对象）
if (!host) return nullptr;
bool isBoss   = (*(uint32_t*)(host + 0x5244) & 0x800000) != 0;   // boss 门控
int  mySlot   = *(uint32_t*)(host + 0x5254);          // 该宿主登记的槽号（校验用）
```
- 槽 0 是游戏内部最常引用的「当前 boss」（大量 ECL 指令直接读 `*(u32*)(mgr+0x5C)`：如 `0x427199` / `0x4271C2` / `0x42AAB1` / `0x42AAC7` / `0x43E12F`）。
- 若需覆盖多 boss（ST4 双 boss），**遍历 8 槽 + 判 `host+0x5244` bit23`** 即可。

---

## 3. 当前 sub 名读取链（TH14 版）

### 3.1 指针链

```text
mgr   = *(u32*)0x4DB544
host  = *(u32*)(mgr + 0x5C + slot*4)     // boss 宿主（0x530C 对象，内嵌 ECL 宿主头于 +0）
node  = *(u32*)(host + 0x04)             // 当前脚本节点（reset 时 = host+8；迭代器每步更新）
ip    = *(u32*)(node + 0x04)             // ★当前指令指针（绝对 VA）
res   = *(u32*)(host + 0x11CC)           // = *(u32*)(mgr+0xCC)，全局 ECL 资源
count = *(i32*)(res + 0x08)
table = *(u32*)(res + 0x8C)              // 项 8B：+0=名字 char*，+4=sub 数据绝对地址
name  = NameOfIp(ip)                     // 见 §3.3（th14 无 sub 索引，必须 ip→sub）
```

- 现成的 getter：`FUN_004217F0`（`0x4217F0`）直接返回 **ip**：`mov eax,[ecx+4]; mov eax,[eax+4]`。
- `host+0x04` 的写点：宿主复位 `FUN_004211C0`（`0x4211C0`）`0x421242: mov [ecx+4],eax`（`eax = ecx+8`）；迭代器 `FUN_00483E70`（`0x483E70`）`0x483EA5: mov [ebx+4],ecx`（`ebx`=host，`ecx`=当前 node）。
- `node+0x04` 的写点：`call`（opcode `0x0B`）处理 `0x482322: mov ecx,[esi+4]; mov [ecx+4],eax`（`esi`=host，`eax`=`0x4848B0` 返回的被调 sub 首指令）；空步进 `0x483C28: mov [edi+4],ecx`。

### 3.2 为什么 boss 与道中同链

- `res` 由管理器持有（`mgr+0xCC`），**加载器 `FUN_004846A0`（`0x4846A0`）把 `default` / `stNN` / `stNNbs` / `stNNmbs` 等全部 ecl 文件追加进同一个 `res`**（`0x4846EB: add [edi+8],eax` 累计计数；`0x48470E: mov [edi+0x8c],eax` 重建表；`0x4846C1` 校验 `"SCPT"`）。
- `0x530C` 类 ctor 也把 `res` 写进 `obj+0x11CC`（`0x422DFC`）⇒ **boss 宿主与道中宿主指向同一 `res` / 同一 sub 表**。故 `stNNbs`/`stNNmbs` 的 sub 天然在表中。

### 3.3 ip → sub（**TH14 特有，boss 亦适用**）

th14 无 sub 索引（`docs/th14/th14_ecl_re_report.md` §2.4/§10.3）：

- `table[i].+4` = sub **数据绝对地址**；`table[i].+0` = 名字 `char*`；**sub 表按名字有序**（供按名二分 `FUN_004848B0`），**`+4` 不随 i 单调**。
- 故：**把 `table[*].+4` 升序排序**，得「代码序区间」`[table[i].+4, table[next].+4)`；`name = NameOfIp(ip) = 名字(最大的 i 使 table[i].+4 <= ip)`。
- 首指令在 `table[i].+4 + 0x10`（sub 头 `"ECLH"`+0x10）；`node+4` 初始为某 sub 首指令，随后按指令长度推进 ⇒ **只要 ip 落在该 sub 的数据范围内即命中该 sub**。
- **boss 场景成立**（🟡 静态）：boss sub（如 `Boss1`/`BossCard2`）是全局表的一员，其数据地址区间同样参与排序与映射；`Boss` 通过同步 `@Boss1()` / `@BossCard1()` 进入 → 主线 node 的 ip 落在相应 sub ⇒ 名字正确。异步子脚本走 `spawn`（`FUN_004844D0`，`0x4844D0`）另起 node，不污染主线 node。

> 采样注意：`node+0x04`（ip）在**帧间稳定态**读（IGI 渲染时）；表达式/局部栈与 `sp=node+0x1008` 共用区域时不宜遍历。

### 3.4 与 TH16 链的差异（汇总）

| 项 | TH16 | TH14 |
| --- | --- | --- |
| 宿主里的「当前 sub」 | `thread+4` = **sub 索引** | ❌ 无；`node+4` = **绝对 ip** |
| node 归属 | `thread = *(host+0xC)`（独立对象） | `node = *(host+4)`（宿主**内嵌** node） |
| res | `host+0x11F8` | `host+0x11CC` |
| 名字 | `*(char**)(table+subIdx*8)` | `NameOfIp(table, ip)`（**区间映射**） |
| 命中点 | `0x473995` | `0x483C4F`（4B，`movss [edi],xmm0`） |

---

## 4. boss 脚本与阶段名

### 4.1 脚本文件与槽位（`ecli`）

`SetFile(N)` 的 N 与文件对应**逐关不同**，以各关 `.decl` 头 `ecli{}` 为准（thprac `THPatch` 用法一致）：

| 关 | `ecli()` | slot 2 | slot 3 | slot 4 | slot 5 |
| --- | --- | --- | --- | --- | --- |
| st01 | `default, st01bs, st01mbs` | st01bs | st01mbs | — | — |
| st02 | `default, st02mbs, st02bs` | st02mbs | st02bs | — | — |
| st03 | `default, st03mbs, st03bs` | st03mbs | st03bs | — | — |
| st04 | `default, st04mbs, st04mbs2, st04bs, st04bs2` | st04mbs | st04mbs2 | st04bs | st04bs2 |
| st05/06/07 | `default, stNNmbs, stNNbs` | stNNmbs | stNNbs | — | — |

### 4.2 `setNext`（「下一阶段」注册）= opcode `0x202`

`.decl` 例（`st01bs.decl`：`:54`、`:193`、`:300`、`:440`）：

```
Boss1()     : setNext(0, 1500, 2100, "BossCard1");   // st01bs.decl:54
Boss2()     : setNext(0, 1900, 2100, "BossCard2");   // st01bs.decl:193
BossCard1() : setNext(0, 0,    2400, "Boss2");       // st01bs.decl:300
BossCard2() : setNext(0, 0,    2400, "BossDead");    // st01bs.decl:440
```

`.ecl` 原始布局（`st01bs.ecl` @ `0x6A4`，`setNext(0,1500,2100,"BossCard1")`）：

```
+0x00 i32  时间(0)
+0x04 u16  opcode = 0x0202   ← setNext
+0x06 u16  size   = 0x002C
+0x08 u32  mask   = 0x04FF0000
+0x0C i32  param0 = 0        ← 槽号（decl 的第 1 个参数）
+0x10 i32  param1 = 0        （未显示）
+0x14 i32  param2 = 1500     （decl 第 2 参数）
+0x18 i32  param3 = 2100     （decl 第 3 参数）
+0x1C i32  param4 = 12       （未显示）
+0x20 cstr "BossCard1\0"     ← ★「下一阶段」名字
```

- ⇒ **`setNext(slot, hp, time, name)`：`name` 在指令 `+0x20`（NUL 结尾），槽号在 `+0xC`，`hp`/`time` 在 `+0x14`/`+0x18`。**
- `setTimeout(slot, name)`（opcode `0x209`，decl 中的 `setTimeout(1,"MBossEscape")`）为超时切换，结构类似。

### 4.3 boss 阶段链（`setNext` 链）与 `setBoss`

各关 boss 的阶段按 `setNext` 串成链，`Boss()` / `MBoss()`（入口）用 SPELL_ID 分支选择进入哪一段；`setBoss(slot)` 在入口处登记宿主。示例（ST1）：

```
st01bs: Boss → @Boss1()  ──setNext──▶ BossCard1 ──▶ Boss2 ──▶ BossCard2 ──▶ BossDead / BossEscape
st01mbs: MBoss → @MBoss1() ──setNext──▶ MBossCard1 ──▶ MBossDead        （入口另有 setTimeout→MBossEscape）
```

各关子名单（自 `.decl`/`.ecl` 解码）：

| 关 | boss（`stNNbs`）阶段 sub | midboss（`stNNmbs`）阶段 sub |
| --- | --- | --- |
| ST1 | `Boss`(入口) `Boss1` `BossCard1` `Boss2` `BossCard2` | `MBoss`(入口) `MBoss1` `MBossCard1` |
| ST2 | `Boss` `Boss1` `BossCard1` `Boss2` `BossCard2` `Boss3` `BossCard3` | `MBoss1` `MBossCard1` |
| ST3 | `Boss` `Boss1` `BossCard1` `Boss2` `BossCard2` `Boss3` `BossCard3` | `MBoss1` `MBoss2` `MBossCard1` |
| ST4 | `Boss` `Boss1..Boss3` `BossCard1..BossCard3`（**A**）／ `BossB` `BossB1..BossB3` `BossBCard1..3`（**B**） | `MBoss1/MBoss2`、`MBossB1/MBossB2` |
| ST5 | `Boss` `Boss1..Boss3` `BossCard1..BossCard4` | `MBoss1` `MBossCard1` |
| ST6 | `Boss` `Boss1..Boss4` `BossCard1..BossCard6`（另 `BossCup`/`BossCupEnd`） | `MBoss1` `MBoss2` |
| ST7 | `Boss` `Boss1..Boss10` `BossCard1..BossCard10`（大 `Taiko*` 辅助） | `MBoss1` `MBoss2` `MBoss3` `MBossCard2/3` `MBossB`… |

> ST4 为**双 boss 并行**（`st04bs` =  A，`st04bs2` = B），同名阶段在 B 侧带 `B` 后缀（`Boss1` vs `BossB1`）⇒ 单列白名单容纳不下，见 §5/§6。
> ST7 的 `setNext` 用 **slot 8**（非常规），且 midboss 走 `MBossSuspend*` 中转，结构特殊。

---

## 5. boss 行回填进展

### 5.1 方法（可复现，不靠猜）

1. 解析 `thprac_th14.cpp` 的 `THPatch`（`thprac_th14.cpp:1855` 起）：对每个 boss 段抄出 `SetFile(N)` + `ECLJump(start,dest)` + `ecl << pair{off,val}` 补丁。
2. 对 `../exes/ecl/th14/stNNbs.ecl` / `stNNmbs.ecl` 用自写 SCPT 解析器解码，得「偏移 → sub / 指令」。
3. 对每行，把补丁落点换算成**被强制进入的 boss sub**：
   - `ECLJump(dest)` 落在 `lifeSet(...)` / `call <Phase>` 之前 → 取随后的 `call` 目标；
   - `pair{off, byte}` 若落在 `call <Name>` 的**名字串**上 → 直接得到改写后的名字（TH14 特有：thprac 用**改名字符**选阶段，如把 `"Boss1"` 改成 `"Boss2"`）。
4. 交叉核对 `thprac_locale_def.cpp` 的 `TH14::th_sections_str`（段落显示名：`关底k非` / 符卡名），确认「第 k 非 = `Bossk`、第 k 符卡 = `BossCardk`」。

### 5.2 回填结果（`docs/logger/th14_checkpoints.csv`，第 1 列）

| 关 | MID1 | MID2 | BOSS1..N |
| --- | --- | --- | --- |
| ST1 | `MBoss1` | `MBossCard1` | `Boss1` `BossCard1` `Boss2` `BossCard2` |
| ST2 | `MBoss1` | `MBossCard1` | `Boss1` `BossCard1` `Boss2` `BossCard2` `BossCard3` |
| ST3 | `MBoss1` | `MID2_EN`=`MBoss2` / `MID2_HL`=`MBossCard1` | `Boss1` `BossCard1` `Boss2` `BossCard2` `Boss3` `BossCard3` |
| ST4 | *（留空）* | *（留空）* | *（留空）* |
| ST5 | `MBoss1` | `MBossCard1` | `Boss1` `BossCard1` `Boss2` `BossCard2` `Boss3` `BossCard3` `BossCard4` |
| ST6 | `MBoss1` | `MBoss2` | `Boss1` `BossCard1` `Boss2` `BossCard2` `Boss3` `BossCard3` `Boss4` `BossCard4` `BossCard5` `BossCard6` |
| ST7 | `MBoss1` | `MBoss2` | `END_NSk`=`Bossk`(k=1..8)、`END_Sk`=`BossCardk`(k=1..10) |

- **共回填 64 / 72 行**；ST4（8 行）留空（原因见下），ST7 的 MID 3 行已按 `setNext`/名字补丁解出（`MID2`→`MBoss2`、`MID3`→`MBoss3`）。

### 5.3 逐关关键证据（节选）

| 行 | thprac 补丁 | 解码结论 |
| --- | --- | --- |
| ST1 MID2 | `st01mbs` jump `0x478→0x560` | `0x560` = `wait(50); lifeSet(1300); @MBossCard1()` ⇒ **`MBossCard1`** |
| ST1 BOSS2 | `st01bs` jump `0x400→0x4E8`；`pair{0x518,0x31}` | `0x4E8` = `lifeSet(1500); @BossCard1()`；`0x518` 是名字串 `"BossCard1"` 的 `'1'` ⇒ **`BossCard1`** |
| ST1 BOSS3 | `pair{0x638,0x32}` | `0x638` = `"Boss1"` 的 `'1'` → 改 `"Boss2"` ⇒ **`Boss2`** |
| ST1 BOSS4 | jump `0x400→0x4E8`；`pair{0x518,0x32}` | `"BossCard1"` → `"BossCard2"` ⇒ **`BossCard2`** |
| ST2 BOSS3 | `pair{0x800,0x32}` | `0x800` = `"Boss1"` 的 `'1'` → `"Boss2"` ⇒ **`Boss2`** |
| ST2 BOSS5 | jump `0x4E4→0x5CC`；`pair{0x5FC,0x33}` | `"BossCard1"` → `"BossCard3"` ⇒ **`BossCard3`**（ST2 仅 2 个「非」，故 `Boss3` 不入表） |
| ST3 MID2_EN | `st03mbs` jump `0x520→0x614`、`0x664→0xC88` | 强制进 `MBoss2` ⇒ **`MBoss2`**（另 `MID2_HL` jump `0x404→0x4EC` ⇒ `MBossCard1`） |
| ST5 BOSS7 | jump `0x43C→0x524`；`pair{0x554,0x34}` | `"BossCard1"`→`"BossCard4"` ⇒ **`BossCard4`** |
| ST6 BOSS9 | `pair{0x8CDC,...}`（落在 `BossCard5` 内） | ⇒ **`BossCard5`**（ST6 BOSS10 落在 `BossCard6`） |
| ST7 END_NS2 | `pair{0xF08,0x32}` | `0xF08` = `"Boss1"` 的 `'1'` → `"Boss2"` ⇒ **`Boss2`** |
| ST7 END_S1 | `pair{0x860,0x31}`（`st7bsSpellSubCallOrd`） | `0x860` = `"BossCard1"` 的 `'1'` ⇒ **`BossCard1`** |

### 5.4 ST4 留空原因（🟡）

- ST4 为**双 boss 并行**：`THPatch` 的**每一行都同时补丁 `st04bs`（A）与 `st04bs2`（B）**，两文件阶段 sub **名字不同**（A：`Boss1`/`BossCard1`…；B：`BossB1`/`BossBCard1`…）。
- CSV 单列只能放 1 个名字；只填 A 名会让 **B 侧 boss 恒不命中白名单**（latch no-op）。⇒ **留空**，待 CSV schema 支持一行多入口（或运行期同时比对 A/B 两名）后再填。
- 已解出的 A/B 对照（供后续使用）：`MID1`= `MBoss1`/`MBossB1`；`MID2`= `MBoss2`/`MBossB2`；`BOSS1..6`（依次 = 非1/符1/非2/符2/非3/符3）= `Boss1`/`BossB1`、`BossCard1`/`BossBCard1`、`Boss2`/`BossB2`、`BossCard2`/`BossBCard2`、`Boss3`/`BossB3`、`BossCard3`/`BossBCard3`。

---

## 6. 风险与未验证点

1. 🟡 **全静态，未真机**：所有地址 / 偏移均来自 `capstone` 反汇编 + `.decl`/`.ecl` 解码，**未真机三态**。
2. 🟡 **A 类宿主（`0x11E0`，`0x484940`）确切用途未定**：其 vtable method0 = `0x420F40`（no-op），不能执行 opcode ≥ `0x12C`，与 `main`（用到 `0x12D`/`0x141`）不符；另存在 `FUN_00421EF0("main")` 调用（`0x436870`，疑似标题 / demo 或另一路径）。**结论不受影响**（boss = `mgr+0x5C` 中的 B 类对象），但 A 类归属仍需真机/进一步逆向确认。
3. 🟡 **`mgr+0x5C` 是否恒为 boss**：由 `setBoss`（opcode `0x200`）登记、`setBoss(-1)` 清除，且游戏内部多处把槽 0 当「当前 boss」；但**槽号由 ECL 传入**（`setNext` 用槽 0，`st01mbs` 的 `MBoss` 用槽 1），需真机确认「哪个槽 = 显示中的 boss」。
4. 🟡 **boss 的 `node` 选取**：`host+0x04` 是「迭代器最后步进的 node」（`0x483EA5`）；boss 有 `async` 子脚本时会切到子 node。**建议取主线 node**：`*(u32*)(host + 0x11D0)`（链头首个 node，reset 时为 `host+8`），或对 `*(u32*)(host+0x04)` 前先判其 ip 是否落在 `Boss*` 区间。
5. 🟡 **ip→sub 区间连续性**：`table[i].+4` 升序区间假定各 sub 代码连续铺开（与 `docs/th14/th14_ecl_re_report.md` §10.3/§12 同一风险）。boss sub 密集、无断裂的静态观察；**须真机抽样复核**（尤其 `BossCard2At` 之类被 `enmCreate` 拉起的辅助 sub）。
6. 🟡 **ST4 双 boss 单列不足**（§5.4）；**ST6/ST7 的 `setNext` slot 8 / `MBossSuspend*` 中转**结构非常规，midboss 阶段的「入口」判定（`MID2`=`MBoss2` vs `MBossCard2`）依赖名字字节补丁，建议真机复核。
7. 🟡 **版本变体**：Steam / 汉化版是否共用同一 RVA 未核（`ImageBase`/`RELOCS_STRIPPED` 同，但 RVA 需按变体实测）。
8. **latch 过场**：段切换瞬间（`goto` 后、目标 `call` 前，ip 仍在外层容器）的采样时机，与 th14 主线同风险；采样放帧间稳定态。

---

## 7. 地址表（VA / RVA）

### 7.1 管理器 / 槽表 / 敌人链

| 项 | VA | RVA |
| --- | --- | --- |
| 管理器全局（指向对象） | `0x4DB544` | `0xDB544` |
| 管理器构造（末尾写 `[0x4DB544]`） | `0x42231A`（函数 `0x4222F0`） | `0x2231A` |
| 管理器析构（置 0） | `0x422821` | `0x22821` |
| 槽表基址 | `mgr+0x5C`（8×4B → `0x5C..0x7B`） | — |
| 按槽取对象 `FUN_0041C050` | `0x41C050` | `0x1C050` |
| 按槽写对象 | `0x421A10` | `0x21A10` |
| `mgr+0xA0` 数组 getter / setter | `0x41C030` / `0x4219F0` | `0x1C030` / `0x219F0` |
| 敌人链头 / 尾 / 计数 | `mgr+0xD0` / `+0xD4` / `+0xD8` | — |
| 对象内嵌链表节点 | `obj+0x14D8`（插入点 `0x422190`…`0x4221DD`） | — |
| 按 id 查对象 `FUN_00421AA0` | `0x421AA0` | `0x1AA0` |
| 按 id 存在性 `FUN_00421A60` | `0x421A60` | `0x1A60` |
| 敌机 id 字段 / getter / 分配 | `obj+0x11E4` / `0x421800` / `0x422DAB` | — / `0x21800` / `0x22DAB` |
| id 计数器 getter（`mgr+0xDC`） | `0x421AF0` | `0x21AF0` |
| 实体更新循环 `FUN_00424CB0` | `0x424CB0` | `0x24CB0` |
| 实体计数 `FUN_004219B0` | `0x4219B0` | `0x219B0` |

### 7.2 boss 门控 / 宿主对象

| 项 | VA | RVA |
| --- | --- | --- |
| ECL 指令 `setBoss`（opcode `0x200`）case | `0x42787D` | `0x2787D` |
| 置 boss 门控 `or [obj+0x5244],0x800000` | `0x4278CC`（`edi=obj+0x11F0`） | `0x278CC` |
| 写槽 `[mgr+slot*4+0x5C]=obj` | `0x4278D6` | `0x278D6` |
| 记槽号 `[obj+0x5254]=slot` | `0x4278DA` | `0x278DA` |
| 门控判读 `test [edi+0x4054],0x800000` | `0x4286C9` | `0x286C9` |
| 敌人 / boss 工厂 `FUN_00421EF0`（`alloc 0x530C`） | `0x421EF0`（`0x421EFE`） | `0x21EF0` |
| 敌人 / boss ctor `FUN_00422AB0`（vtable `0x4BDE7C`） | `0x422AB0` | `0x22AB0` |
| `obj+0x52DC` 自指针写点 | `0x422AF1` | `0x22AF1` |
| ECL 宿主 ctor `FUN_00421150`（vtable `0x4BDEA4`） | `0x421150` | `0x21150` |
| ECL 宿主复位 `FUN_004211C0` | `0x4211C0` | `0x211C0` |
| `obj+0x11CC` = res 写点 | `0x422DFC` | `0x22DFC` |
| `"BossDead"` / `"BossEscape"` 字面量 | `0x4BDE2C` / `0x4BDE38` | `0xBDE2C` / `0xBDE38` |
| boss 死 / 逃脚本切换 | `0x42C540` / `0x42C620` | `0x2C540` / `0x2C620` |
| `"main"` 字面量 | `0x4B773A` / `0x4BE064` | `0xB773A` / `0xBE064` |
| `FUN_00421EF0("main")` 调用点 | `0x436870`（函数 `0x436620`） | `0x36870` |

### 7.3 当前 sub 读取链

| 项 | VA | RVA |
| --- | --- | --- |
| 命中点（`Edi`=node，4B `movss [edi],xmm0`） | `0x483C4F` | `0x83C4F` |
| node 步进器 `FUN_00482380` | `0x482380` | `0x82380` |
| 宿主迭代器 `FUN_00483E70`（写 `host+4`） | `0x483E70`（`0x483EA5`） | `0x83E70` |
| ip getter `FUN_004217F0` | `0x4217F0` | `0x217F0` |
| 子对象 getter（`obj+0x11F0`） | `0x421810` | `0x21810` |
| ip 写点：空步进 / `call` / `goto` | `0x483C28` / `0x482322` / `0x4825EB` | `0x83C28` / `0x82322` / `0x825EB` |
| spawn `FUN_004844D0`（挂 `host+0x11D0`） | `0x4844D0` | `0x844D0` |
| ECL 加载器 `FUN_004846A0`（SCPT） | `0x4846A0`（`0x4846C1`） | `0x846A0` |
| 按名解析 `FUN_004848B0`（返回 `+4+0x10`） | `0x4848B0`（`0x48492E`） | `0x848B0` |
| res getter `FUN_0041DA10` / count getter `FUN_0041DA00` | `0x41DA10` / `0x41DA00` | `0x1DA10` / `0x1DA00` |
| 名字 by-index `FUN_0041D9E0` | `0x41D9E0` | `0x1D9E0` |
| ECL 指令大 switch（thunk → 实体） | `0x425260` → `0x425270` | `0x25260` → `0x25270` |
| 主线（关卡）宿主工厂 `FUN_00484940`（`0x11E0`） | `0x484940` | `0x84940` |
| 主线宿主写 `scene+0x2DC` | `0x45883E` | `0x5883E` |

### 7.4 ECL 指令（`.decl` ↔ opcode）

| 指令 | opcode | 结构要点 |
| --- | --- | --- |
| `setBoss(slot)` | `0x200` | 登记 / 清除 `mgr+0x5C+slot*4`；置 / 清 `obj+0x5244` bit23；槽号存 `obj+0x5254` |
| `setNext(slot,hp,time,name)` | `0x202` | 参数 `+0xC`=slot、`+0x14`=hp、`+0x18`=time、`+0x20`=name |
| `setTimeout(slot,name)` | `0x209` | 超时切换（结构类似） |
| `call` / `async call` | `0x0B` / `0x0F` | 名字在 `+0x14`；`call` 把 `node+4` 置为被调首指令 |

### 7.5 离线素材

- `../exes/ecl_dump/th14/st01bs.decl`（`setNext` 例 `:54/:193/:300/:440`）、`st01mbs.decl`（`:28/:35/:239`）、`stNNbs.decl` / `stNNmbs.decl`；
- `../exes/ecl/th14/stNNbs.ecl` / `stNNmbs.ecl`（原始，自写 SCPT 解析器逐指令解码）。

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | **建档**。落盘 TH14 boss 段监控（**方案二**：读 boss 宿主 ECL 当前 sub 名 + 白名单 latch）：**boss 宿主取法**（`mgr=*(u32*)0x4DB544`；槽表 `mgr+0x5C+slot*4` **直接存宿主指针**；按槽 getter `FUN_0041C050`；按 id `FUN_00421AA0`（id=obj+0x11E4）；敌人双向链 `mgr+0xD0`；**boss 门控 `[obj+0x5244]&0x800000`**，由 ECL `setBoss`（opcode `0x200`）置/清；宿主自指针 `obj+0x52DC`；确认 TH14 **无** `0x5740`/`0x44B8`/`mgr+0x48`/`mgr+0x180` 同形结构）；**当前 sub 链**（沿用 th14 主线：`host→node=*(u32*)(host+4)→ip=*(u32*)(node+4)→res=*(u32*)(host+0x11CC)→ip→sub 区间映射`；boss 与道中共用全局 `res`）；**boss 脚本/阶段名**（`SetFile` ↔ `ecli`；`setNext`=opcode `0x202`、name 在 `+0x20`；各关阶段链与子名单）；**boss 行回填**（`docs/logger/th14_checkpoints.csv` 72 行中回填 64 行，ST4 双 boss 名 A/B 单列不足留空 8 行，ST7 MID 3 行已解）。含风险与未验证点、VA/RVA 地址表、`.decl`/`.ecl` 证据。状态：静态调研（🟡 未真机三态）。 |

# TH08 ECL 段落监控 — 逆向结论（Q1–Q9）

> 对应需求：`docs/th08/th08_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`；结构模板：`docs/th12/th12_ecl_re_report.md`（**只借结构，不借技术结论** —— th08 是 A 代，机制不同）。
> 二进制：`../exes/th08.exe`，**840704 B**，PE `TimeDateStamp = 1095610188`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**），`SizeOfImage = 0x14DC000`，`SizeOfCode = 0xB1C00`。
> `sha256 = 330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924`（与 `docs/logger/decompile_materials.md` §2 的 `330fbdbf…` 一致；对应 `[th08] 东方永夜抄 (日文版)`）。
> 节表（本机实测，**注意 `.text` 的 RVA 不是 0**）：`.text` VA `0x402000` / RVA `0x2000` / RawPtr `0x1E00` / RawSize `0xB1C00`；`.rdata` VA `0x4B4000` / RawSize `0x11400`；`.data` VA `0x4C6000` / **VirtualSize `0x1413840`（巨型 BSS，本文的静态全局都在这里面）**；`.data1` VA `0x18DA000`；`.rsrc` VA `0x18DB000`。**VA→文件偏移必须用 `pefile.get_offset_from_rva(VA−0x400000)`**。
> 引擎代：**A 代（第一代引擎）**；同代参照：th06（无 exe）/ th07。
> ECL / decl 素材：**无**（无 `.ecl`、无 `.decl`、无 A 代 eclmap）。`../exes/th08.dat` 内的 ECL 成员名可从 exe 字符串读出（见 §Q2），但**未解包**。
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` **只读静态反汇编**（脚本见「工具与复现」），用「原始字节搜索 + 函数入口整段反汇编 + 跳表/字段位移交叉印证」定位。
> 状态：**静态定案（🟡 已定案、未真机三态）**。本机为 macOS，**无法编译、无法真机运行，未做任何真机验证**。
> 地址口径：正文以 **VA** 为主，并标注 **RVA = VA − 0x400000**。
> 置信度图例：✅ 反汇编直接证实 ｜ 🟡 有强证据但缺一环（显式标注缺口）｜ ❌ 未能定案。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q1 当前位置** | ✅ **（c）指令指针为主**，且**同时存在（a）sub 索引与（b）时间值**（三者并存于同一 cursor 帧） | `cursor = *(u32*)(host+0x2CA0)`（通常 = `host+0x7F8`）；`cur_ins = *(u32*)(cursor)`（当前指令**绝对地址**，指向 ECL 文件缓冲内）；`cur_time = *(i32*)(cursor+0xC)`；`cur_sub = *(i16*)(cursor+0x224)`；`offset = cur_ins − *(u32*)0x4ECCB8`。**另一套「Timeline」**（thprac `ECLWarp` 写的那个）在 `0xF54CF0`，每 slot 0x10 B，`+8`=time、`+0xC`=指令指针。 |
| **Q2 sub 表 / 时间轴的关系** | ✅ sub 表存在但**无名字**；Timeline 是**另一套机制**，二者不同 | `pECL = *(u32*)0x4ECCB8`；`subTable = *(u32*)(0x4ECCB8+4)`；`count = *(i16*)(pECL+4)`；`sub_start(id) = *(u32*)(subTable+id*4)`。`call sub`＝opcode **0x34** → `0x421BD0`。Timeline 的 (time, ins) 指向「timeline 命令」（17 项 opcode 表 `0x42AD60`），**不是 sub**。 |
| **Q3 函数栈** | ✅ **有显式可遍历子调用栈** | 栈基址 `host+0xA20`，**帧长 `0x228`**，深度 `*(i16*)(host+0x2CEA)`，上限 16（`0xF`）；帧 = **调用者 cursor 的快照**（帧 `+0`＝调用者指令地址、`+0x224`＝调用者 sub_id）；当前层是活动 cursor `host+0x7F8`。 |
| **Q4 主线宿主捕获点** | 🟡 **A 代没有「具名 main 宿主」概念**；ECL 资源是静态单例 `0x4ECCB8`，宿主是静态 manager（`0x577F20`）内嵌的 480 槽对象数组（步长 `0x53D0`） | 捕获点建议：**`0x418450`**（sub setter，命中时 `[ebp+8]`=cursor ⇒ `host = [ebp+8] − 0x7F8`，`[ebp+0xC]`=sub_id，3B 可 EHOOK）或解释器入口 **`0x4184C8`**（`mov eax,[ebp+8]`，3B，`[ebp+8]`=host）。⚠️ **主线宿主究竟是哪一槽，未静态定案**（缺口见 §Q4）。 |
| **Q5 解释器命中点** | ✅ ECL 主解释器 = **`0x4184B0`–`0x41ECBD`**；opcode 跳表 `0x41ECBE`（`0xB8` 项，opcode `1..0xB8`） | thprac 现用的 `0x41CB32`（op 0x84）/`0x41C44F`（op 0x7F）**可复用**（命中时 `[ebp+8]`=host、`[ebp−0xC]`=当前指令），但**只在执行到该 opcode 时命中**；`0x42F34D` 只是一个 3B setter（**与当前位置无关，不可复用**）；`0x437AF3` 在初始化/复位路径，非每帧。**推荐：不挂 VM 钩子**，在 IGI 渲染时直接读宿主 `+0x7F8`。 |
| **Q6 名字来源** | ✅ **运行期不保留 sub 名** | ECL 文件头只有**偏移表**（`buf+0x48`，`count = *(i16*)(buf+4)`）；加载器 `0x418330` 全文无名字解析；exe 内除文件名字符串外**无任何 sub 名串**。⇒ 只能离线做「偏移 ↔ 段落」映射（用 thprac 自己的 warp 表，见 §附录）。 |
| **Q7 机体 / 子机** | ✅ 地址有效（thprac 既有） | `SHOTTYPE_ADDR = 0x164D0B1`（**byte**，作表索引）；`DIFF_ADDR = 0x160F538`（int，`GameManager+0x30`，`imul …,0x44` 作行距）；**新登记**：`0x160F53C` = **rankMask**（`GameManager+0x34`，解释器与 slot 处理器都用它判难度）、`0x164D0B8`（**i16**，关卡/END 选择子，`≥0xCD` = Extra）。组合编码语义待真机。 |
| **Q8 练习跳转影响** | 🟡 **等价物是「写哪就读哪」**，但两条路时间尺度不同 | `ECLJump/ECLCallSub/ECLSetTime/ECLTimeFix` **直接改写运行期 ECL 缓冲**（`ECLHelper` 基址 = `*(u32*)0x4ECCB8`）⇒ 下一帧该宿主执行即读到；`ECLWarp` 写 **Timeline slot**（time/ins），由 stage update 每帧扫描、`slot+8 == ins->time` 时执行 slot 命令 ⇒ **下一个 stage-update 帧生效**。 |
| **Q9 段落判定口径** | ✅ 沿用**路过式 checkpoint（latch）**；A 代读取方案见下 | **读 `(cur_sub, offset)`**（`offset` 优先，因为 warp 落点是精确偏移）；**checkpoint「入口」列记 `(time, offset)`**（不用 sub 名）。数组匹配 = 偏移**精确相等 / 区间**。`main` 兜底值。 |

> **关键地址（VA / RVA）**
> **ECL 资源单例** `0x4ECCB8` / `0xECCB8`（`+0`=ECL 文件缓冲、`+4`=sub 偏移表＝`buf+0x48`）；
> **ECL 加载器** `0x418330` / `0x18330`；**sub 设置器** `0x418450` / `0x18450`；
> **ECL 主解释器 `FUN_004184B0`** = `0x4184B0`–`0x41ECBD` / `0x184B0`–`0x1ECBD`；分发头 `0x4185F1` / `0x185F1`；跳表 `0x41ECBE` / `0x1ECBE`；难度位检测 `0x4185BF` / `0x185BF`；
> **当前指令/时间重取** `0x4184FF` / `0x184FF`；**逐指令推进** `0x41E7E4` / `0x1E7E4`（`[ebp−0xC] += [ins+6]`）；
> **call(0x34)** `0x4197E2` / `0x197E2` → handler **`0x421BD0`** / `0x21BD0`；**ret(0x35，推定)** `0x4197F9` / `0x197F9` → handler `0x421CB0` / `0x21CB0`（`0x421D95` 把帧拷回 cursor）；
> **goto(0x04)** `0x4186F1` / `0x186F1`；**setTime(0x86)** `0x41CCFC` / `0x1CCFC`；
> **时间比较器** `0x40D3F0` / `0xD3F0`（`*(u32*)(this+8) == arg`）；**取字段** `0x40D3B0` / `0xD3B0`；
> **ECL 管理器（模板）** `0x577F20` / `0x177F20`（ctor `0x42A210` / `0x2A210`，由 .rdata 桩 `0x4B3710` / `0xB3710` 以 `ecx=0x577F20` 调用）；**敌/ECL 上下文数组** `0x57D2F0` / `0x17D2F0`（＝`0x577F20+0x53D0`，步长 `0x53D0`，活动 480 槽）；
> **Timeline slot 数组** `0xF54CF0` / `0xB54CF0`（＝`0x577F20+0x9DCDD0`，步长 `0x10`）；**slot 处理器** `0x42A8A0` / `0x2A8A0`；**slot 命令跳表** `0x42AD60` / `0x2AD60`（`0x11` 项）；**slot 计数 getter** `0x42DFB0` / `0x2DFB0`；**slot 初始 ins getter** `0x42DFD0` / `0x2DFD0`；**stage update 中的 slot 扫描循环** `0x42C7D5`–`0x42C833` / `0x2C7D5`；
> **thprac 现有 hook**：`0x437AF3` / `0x37AF3`、`0x41CB32` / `0x1CB32`、`0x41C44F` / `0x1C44F`、`0x42F34D` / `0x2F34D`；
> **全局**：`GameManager 0x160F508` / `0x120F508`（`+0x30`=diff、`+0x34`=rankMask；⚠️ `0x160F510`(＝GameManager+8) 是**指针**，thprac `ECLCheckTime` 的「时间」在 `*(u32*)0x160F510 + 0x3C` 与 `+0x44`）、`0x164D0B1`(shottype) / `0x124D0B1`、`0x164D0B8`(stage, i16) / `0x124D0B8`、`0x164D0BA`(pause) / `0x124D0BA`、`0x164D0BB` / `0x124D0BB`、`0x164D2CC`(ECL 文件索引) / `0x124D2CC`、`0x164CFB4` / `0x124CFB4`、`0x4E4824`(STD) / `0xE4824`、`0x4EA290` / `0xEA290`、`0x4EA678`(spell id) / `0xEA678`。

---

## 1. 统一指针链（TH08 定案）

```text
// ── ① ECL 资源单例（静态对象，A 代特征：不是堆对象）─────────────────────
ECLSRC = 0x4ECCB8                     // 静态 struct；ctor/加载见 0x418330
  ECLSRC + 0x00 = u32  ECL 文件缓冲基址（= pECL = *(u32*)0x4ECCB8，thprac 的 GetMemAddr 一级）
  ECLSRC + 0x04 = u32  sub 偏移表基址（= pECL + 0x48，加载器把它内部偏移改成绝对地址）
  ECLSRC + 0x08 = u32[16] 文件头 +8 起的 16 个 dword（加载器同样改成绝对地址）
  ECLSRC + 0x168 .. 0x188：被 0x421BD0 copy 到 cursor+0x70 的 8 个 dword（sub 局部寄存器组）

// ── ② ECL 文件（pECL 指向的缓冲）头布局（由 0x418330 加载器与 0x42DFB0/0x42DFD0 推出）─
pECL + 0x00 = u32  魔数 0x00000800
pECL + 0x04 = i16  subCount（sub 表项数）
pECL + 0x06 = i16  slotCount（Timeline slot 数，见 0x42DFB0）
pECL + 0x08 = u32[slotCount]  slot 的「初始 timeline 命令」指针（加载时 += pECL 变成绝对）
pECL + 0x48 = u32[subCount]   sub 起始偏移表（加载时 += pECL 变成绝对）★Q2

sub_start(id) = *(u32*)( *(u32*)(0x4ECCB8+4) + id*4 )
subCount      = *(i16*)( pECL + 4 )

// ── ③ ECL 宿主对象（敌/脚本对象；静态 manager 内嵌数组，步长 0x53D0）──────────
MGR    = 0x577F20                     // 静态对象：0..0x53D0 = 「模板 ECL 对象」，其后是 480 槽数组
OBJ(i) = 0x577F20 + 0x53D0 + i*0x53D0  // i = 0..479（更新循环上界恒为 0x1E0=480）
                                       // OBJ(0) = 0x57D2F0（=数组首址），OBJ(480)=0xF4F8F0（保留/哨兵对象）

OBJ + 0x7F8 = cursor 帧（★「当前执行到哪」就在这里；**主 cursor**）：
      cursor + 0x000 = u32  当前（下一条待执行）指令的【绝对地址】（指向 pECL 缓冲内）  ★Q1(c)
      cursor + 0x004 = ?（被 0x40D3F0 以 this=cursor+4 用于 `*(this+8)==arg` 比较的基址）
      cursor + 0x00C = i32  当前脚本时间（= OBJ+0x804，0x40D3F0 实际比较的字段）        ★Q1(b)
      cursor + 0x070 = u32[8] sub 局部寄存器组（call 时从 ECLSRC+0x168 拷入）
      cursor + 0x220 = i32  本 cursor 的序号（ret 时用 `序-1` 索引 OBJ+0x3384，见 0x421D08）
      cursor + 0x224 = i16  当前 sub_id                                                 ★Q1(a)
OBJ + 0xA20 = 子调用栈基址（帧数组）；帧长 0x228；深度 = *(i16*)(OBJ+0x2CEA)（上限 16）
OBJ + 0x2CA0 = u32  「当前 cursor」指针（解释器入口 0x4184E5 置为 OBJ+0x7F8；
                    0x41EC2E 一类路径会改为 OBJ+0x3384[i]+8 —— 见下）
OBJ + 0x2CA4 = u32  「栈基址」指针（解释器入口 0x4184D3 置为 OBJ+0xA20；同上会被改为 +0x230）
OBJ + 0x3384 = u32[N] **子上下文对象指针数组**（🟡 元素数未定案）
               子上下文对象：cursor@+8（含 +0/+0xC/+0x220/+0x224 同上）、栈@+0x230
               （`0x41EC34`–`0x41EC54`：把 OBJ+0x2CA0/0x2CA4 切到该子上下文，并写 `cursor+0x220`）
OBJ + 0x2CE8 = i16 初始/已提交深度；+0x2CEA = i16 工作深度；+0x2CEE = i16 待切换 sub_id
OBJ + 0x2D00 面：+0x2D30 = i16（由操作数写入，0x42AC10）、+0x2D34 = vec3 位置、+0x2D40 = 目标位置、
                +0x2D88 = 渲染位置、+0x2DFC = i32 计数/寿命、+0x2E0C = i32 槽序号
OBJ + 0x3324 = u32 flags（bit0 = 活动、bit7 = 触发更新、bit0x18 = 子上下文标志…）
OBJ + 0x3328 = u32 flags2；+0x3330 = u8 难度掩码；+0x3311/+0x3313 = u8 索引；+0x3314 = u8 计数

// ── ④ Timeline（thprac ECLWarp 写的那个；与 ③ 的 cursor 无关）─────────────
TL  = 0x577F20 + 0x9DCDD0 = 0xF54CF0        // slot 数组基址（0x42C7E4 / 0x42C7FD / 0x42C82A 用它）
slotCount = *(i16*)( pECL + 6 )             // 0x42DFB0
slot[i] = TL + i*0x10：
      slot + 0x08 = i32  time               ★ thprac ECLWarp 的 target[0]（写 0xF54CF8+i*0x10）
      slot + 0x0C = u32  指令指针（指向 pECL 缓冲内的一条「timeline 命令」）
                                            ★ thprac ECLWarp 的 target[1]（写 0xF54CFC+i*0x10）
      （slot+0 与 slot+4 未见消费方；未定案）
slot 命令执行器：0x42A8A0（ecx = slot）；命令 opcode 表 0x42AD60（0x11 项，opcode 0..0x10）
stage update 每帧扫描：0x42C7D5 循环 `i < slotCount` → `0x42A8A0( slot[i] )`
```

> ⚠️ **`0xF54CF0` 在本机全文件搜索不到任何立即数引用**——因为它位于**静态 manager `0x577F20` 的尾部**，代码一律以 `[manager + 0x9DCDD0]` 形式访问（disp32 = `0x9DCDD0`，见 `0x42C7F3`/`0x42C817`/`0x42C82A`）。**这是本作最容易误判的一点**：thprac 源码注释把它叫 “Timeline Struct”，但它其实是管理器尾部的一个 slot 数组，不是宿主对象。

---

## 2. Q1 —— 「当前位置」＝ 指令指针（并存 sub 索引与时间值）

### 2.1 判据：解释器把 cursor[0] 当指令指针用

解释器入口 `0x4184B0` 先建立两个内嵌指针，然后**重新从 cursor 取当前指令**：

```asm
004184b0  55                push  ebp
004184b1  8bec              mov   ebp, esp
004184b3  81ecc4050000      sub   esp, 0x5c4
004184b9  56                push  esi
004184ba  57                push  edi
004184bb  898dbcfeffff      mov   [ebp-0x144], ecx        ; ecx（调用方给的第二参/this）
004184c1  c745f8ffffffff    mov   dword ptr [ebp-8], -1
004184c8  8b4508            mov   eax, [ebp+8]            ; ★ host（宿主对象）
004184cb  05200a0000        add   eax, 0xa20
004184d0  8b4d08            mov   ecx, [ebp+8]
004184d3  8981a42c0000      mov   [ecx+0x2ca4], eax      ; host+0x2CA4 = host+0xA20（栈基址）
004184d9  8b5508            mov   edx, [ebp+8]
004184dc  81c2f8070000      add   edx, 0x7f8
004184e2  8b4508            mov   eax, [ebp+8]
004184e5  8990a02c0000      mov   [eax+0x2ca0], edx      ; host+0x2CA0 = host+0x7F8（cursor 指针）
004184eb  8b4d08            mov   ecx, [ebp+8]
004184ee  8b5508            mov   edx, [ebp+8]
004184f1  668b82e82c0000    mov   ax, word ptr [edx+0x2ce8]
004184f8  668981ea2c0000    mov   word ptr [ecx+0x2cea], ax ; 工作深度 = 已提交深度
004184ff  8b4d08            mov   ecx, [ebp+8]            ; ★ 0x4184FF：重取当前指令
00418502  8b91a02c0000      mov   edx, [ecx+0x2ca0]      ; edx = host+0x7F8
00418508  8b02              mov   eax, [edx]             ; eax = *(host+0x7F8) = 当前指令地址
0041850a  8945f4            mov   [ebp-0xc], eax         ; ★ [ebp-0xc] = 当前指令
0041850d  8b4d08            mov   ecx, [ebp+8]
00418510  0fbf91302d0000    movsx edx, word ptr [ecx+0x2d30]
00418517  85d2              test  edx, edx
00418519  7c05              jl    0x418520
0041851b  e96ac00000        jmp   0x41c88a
```

⇒ **`cur_ins = *(u32*)(host + 0x7F8)`**（等价于解释器内的 `[ebp−0xC]`）。

### 2.2 分发：指令头布局 + opcode 跳表

```asm
004185f1  8b55f4            mov   edx, [ebp-0xc]          ; 当前指令
004185f4  0fbf4204          movsx eax, word ptr [edx+4]     ; ★ opcode = (i16)ins[+4]
004185f8  8985b8feffff      mov   [ebp-0x148], eax
004185fe  8b8db8feffff      mov   ecx, [ebp-0x148]
00418604  83e901            sub   ecx, 1
00418607  898db8feffff      mov   [ebp-0x148], ecx
0041860d  81bdb8feffffb7000000  cmp dword ptr [ebp-0x148], 0xb7
00418617  0f87 c7610000     ja    0x41e7e4                ; opcode > 0xB8 → 直接推进
0041861d  8b95b8feffff      mov   edx, [ebp-0x148]
00418623  ff2495beec4100    jmp   dword ptr [edx*4+0x41ecbe]  ; ★ opcode 跳表（0xB8 项）
```

指令推进：

```asm
0041e7e4  8b4df4            mov   ecx, [ebp-0xc]
0041e7e7  0fbf5106          movsx edx, word ptr [ecx+6]     ; ★ length = (i16)ins[+6]
0041e7eb  8b45f4            mov   eax, [ebp-0xc]
0041e7ee  03c2              add   eax, edx
0041e7f0  8945f4            mov   [ebp-0xc], eax
0041e7f3  e9 289dffff       jmp   0x418520
```

⇒ **指令头（与 thprac `__th08_ins_header` 吻合）**：`+0x00` = i32 time；`+0x04` = i16 opcode；`+0x06` = i16 **length（= 整条指令的字节长度）**；`+0x08` = 难度/参数域；`+0x09` = u8 难度位（`0x4185BF: movzx edx, byte ptr [ecx+9]` 再与 `[0x160F53C]`(rankMask) 和 `host+0x3330` 做位与）；`+0x0A` = i16 参数掩码（逐位表示操作数是立即数/寄存器，决定是否走 `0x41F420`（变量取值））；`+0x0C` 起 = 操作数。
> 与 thprac 的 `time/opcode/length/rankMask/paramMask` 顺序一致，但 **rankMask 实为 `+9` 的 1 字节**（thprac 记成 `+8` 的 i16，只是近似；`ECLTimeFix` 只用 `length`，不受影响）。
> **opcode 上限 = `0xB8`**，跳表 `0x41ECBE`（0xB8 项）；解释器函数边界 **`0x4184B0`–`0x41ECBD`**（末尾 `0x41ECBB: c2 04 00 ret 4`）。

### 2.3 三个字段的关系与写入点

| 字段 | 表达式 | 类型/范围 | 谁写 | 证据 |
| --- | --- | --- | --- | --- |
| **当前指令** `cur_ins` | `*(u32*)(host+0x7F8)` | u32（pECL 内绝对地址） | `0x418450`（切 sub）；op 0x04 `0x4186F1`：`mov [host+0x2ca0]→[+0xC]… [ebp−0xC] += ins[+0x10]`；op 0x34 handler `0x421BD0`；`0x421CB0`（ret） | 上两段反汇编 |
| **当前时间** `cur_time` | `*(i32*)(host+0x804)` | i32（脚本帧时间） | op 0x04 `0x418700: mov [eax+0xc], edx`（eax = `*(host+0x2CA0)` = host+0x7F8）；`0x40D3F0` 用它做门控 | `0x4186F1`–`0x41870C` |
| **当前 sub** `cur_sub` | `*(i16*)(host+0xA1C)` | i16（sub_id，`≥0`；`0x418450` 里 `<0` 会被拒绝） | `0x418495: mov word ptr [ecx+0x224], dx`（ecx = cursor，dx = sub_id） | `0x418450` 尾部 |

**取值建议（运行期）**：

```cpp
uint32_t pECL   = *(uint32_t*)0x4ECCB8;              // ECL 文件缓冲基址
uint32_t cursor = *(uint32_t*)(host + 0x2CA0);       // ★「当前 cursor」；通常 == host+0x7F8
uint32_t ip     = *(uint32_t*)(cursor);              // 当前指令（绝对）＝ ip - pECL 为偏移
int32_t  eol_off= (int32_t)(ip - pECL);              // 相对偏移 ← 与 thprac warp 表同一坐标系
int32_t  etime  = *(int32_t*)(cursor + 0x0C);        // 当前脚本时间
int16_t  esub   = *(int16_t*)(cursor + 0x224);       // 当前 sub_id
// 若不想跟 cursor 指针，直接用主 cursor 亦可（绝大多数时刻成立）：
//   ip = *(u32*)(host+0x7F8);  etime = *(i32*)(host+0x804);  esub = *(i16*)(host+0xA1C);
```

`(a)/(b)/(c)` 关系：**三者并存于同一 cursor 帧**。引擎推进以 `cur_ins`（(c)）为准；`cur_time`（(b)）是脚本时间（也用于 warp 落点的 `time`）；`cur_sub`（(a)）只在切 sub / `call` / `ret` 时更新（**不是**「当前指令所属 sub」的实时反查结果）；另外还有一套**全局 Timeline**（§1 ④），其 `(time, ip)` 对正是 `ECLWarp` 写进去的东西 —— 它**不是**「当前位置」，而是「若干待生效的定时跳转」。

---

## 3. Q2 —— sub 表与「时间轴」的关系

### 3.1 ECL 加载器 `0x418330` / RVA `0x18330`（sub 表的写入点）

```asm
00418330  55                push  ebp
00418331  8bec              mov   ebp, esp
00418333  83ec08            sub   esp, 8
00418336  894df8            mov   [ebp-8], ecx              ; ecx = ECLSRC（0x4ECCB8）
00418339  6a00              push  0
0041833b  33d2              xor   edx, edx
0041833d  8b4d08            mov   ecx, [ebp+8]              ; 文件名/序号
00418340  e81b630200        call  0x43e660                  ; 打开并读入文件 → EAX = 缓冲
00418345  8b4df8            mov   ecx, [ebp-8]
00418348  8901              mov   [ecx], eax               ; ★ ECLSRC+0 = pECL（ECL 文件缓冲）
0041834a  8b55f8            mov   edx, [ebp-8]
0041834d  833a00            cmp   dword ptr [edx], 0
00418350  751a              jne   0x41836c
00418352  6830484b00        push  0x4b4830                 ; 错误串
00418357  6840d56401        push  0x164d540
0041835c  e89f660200        call  0x43ea00                 ; log
00418361  83c408            add   esp, 8
00418364  83c8ff            or    eax, 0xffffffff
00418367  e9ad000000        jmp   0x418419
0041836c  8b45f8            mov   eax, [ebp-8]
0041836f  8b08              mov   ecx, [eax]
00418371  813900080000      cmp   dword ptr [ecx], 0x800   ; ★ 魔数 = 0x00000800
00418377  741a              je    0x418393
00418379  6808484b00        push  0x4b4808                 ; 错误串
0041837e  6840d56401        push  0x164d540
00418383  e878660200        call  0x43ea00                 ; log
00418388  83c408            add   esp, 8
0041838b  83c8ff            or    eax, 0xffffffff
0041838e  e986000000        jmp   0x418419
00418393  c745fc00000000    mov   dword ptr [ebp-4], 0
0041839a  eb09              jmp   0x4183a5
0041839c  8b55fc            mov   edx, [ebp-4]              ; （循环自增，回到 0x41839c）
0041839f  83c201            add   edx, 1
004183a2  8955fc            mov   [ebp-4], edx
004183a5  837dfc10          cmp   dword ptr [ebp-4], 0x10  ; ★ 固定 16 项
004183a9  7d1f              jge   0x4183ca
004183ab  8b45f8            mov   eax, [ebp-8]
004183ae  8b08              mov   ecx, [eax]
004183b0  8b55fc            mov   edx, [ebp-4]
004183b3  8b449108          mov   eax, [ecx+edx*4+8]       ; 头 +8 起的 16 个 dword
004183b7  8b4df8            mov   ecx, [ebp-8]
004183ba  0301              add   eax, [ecx]               ; ★ += pECL（改成绝对地址）
004183bc  8b55f8            mov   edx, [ebp-8]
004183bf  8b0a              mov   ecx, [edx]
004183c1  8b55fc            mov   edx, [ebp-4]
004183c4  89449108          mov   [ecx+edx*4+8], eax
004183c8  ebd2              jmp   0x41839c
004183ca  8b45f8            mov   eax, [ebp-8]
004183cd  8b08              mov   ecx, [eax]
004183cf  83c148            add   ecx, 0x48
004183d2  8b55f8            mov   edx, [ebp-8]
004183d5  894a04            mov   [edx+4], ecx             ; ★ ECLSRC+4 = pECL + 0x48（sub 偏移表）
004183d8  c745fc00000000    mov   dword ptr [ebp-4], 0
004183ea  8b4df8            mov   ecx, [ebp-8]
004183ed  8b11              mov   edx, [ecx]
004183ef  0fbf4204          movsx eax, word ptr [edx+4]     ; ★ count = (i16)buf[+4]
004183f3  3945fc            cmp   [ebp-4], eax
004183f6  7d1f              jge   0x418417
004183f8  8b4df8            mov   ecx, [ebp-8]
004183fb  8b5104            mov   edx, [ecx+4]            ; = pECL+0x48
004183fe  8b45fc            mov   eax, [ebp-4]
00418401  8b0c82            mov   ecx, [edx+eax*4]        ; 表项（原为偏移）
00418404  8b55f8            mov   edx, [ebp-8]
00418407  030a              add   ecx, [edx]              ; ★ += pECL → 绝对
00418409  8b45f8            mov   eax, [ebp-8]
0041840c  8b5004            mov   edx, [eax+4]
0041840f  8b45fc            mov   eax, [ebp-4]
00418412  890c82            mov   [edx+eax*4], ecx
00418415  ebca              jmp   0x4183e1
00418417  33c0              xor   eax, eax
00418419  8be5              mov   esp, ebp
0041841b  5d                pop   ebp
0041841c  c20400            ret   4
```

⇒ **ECL 文件头（`pECL`）**：`+0` u32 魔数 `0x800`；`+4` **i16 subCount**；`+6` i16（slot 数，`0x42DFB0` 读它）；`+8` 起 16 个 dword（被改成绝对地址）；`+0x48` 起 subCount 个 dword（sub 起始偏移表，被改成绝对地址）。
**没有任何名字表**（加载器全文无名字/字符串处理）⇒ Q6 见 §7。

### 3.2 sub 设置点 `0x418450` / RVA `0x18450`（`call sub` 的落点，也是 sub_id 的写入点）

```asm
00418450  55                push  ebp
00418451  8bec              mov   ebp, esp
00418453  51                push  ecx
00418454  894dfc            mov   [ebp-4], ecx              ; ecx = ECLSRC
00418457  0fbf450c          movsx eax, word ptr [ebp+0xc]    ; sub_id
0041845b  85c0              test  eax, eax
0041845d  7d04              jge   0x418463
0041845f  33c0              xor   eax, eax                  ; sub_id < 0 → 直接返回 0
00418461  eb3f              jmp   0x4184a2
00418463  0fbf4d0c          movsx ecx, word ptr [ebp+0xc]
00418467  8b55fc            mov   edx, [ebp-4]
0041846a  8b4204            mov   eax, [edx+4]              ; pECL+0x48（sub 表）
0041846d  8b5508            mov   edx, [ebp+8]              ; cursor
00418470  8b0488            mov   eax, [eax+ecx*4]          ; ★ sub 表[sub_id]（绝对）
00418473  8902              mov   [edx], eax                ; ★ cursor[0] = sub 起始指令
00418475  6a00              push  0
00418477  8b4d08            mov   ecx, [ebp+8]
0041847a  83c104            add   ecx, 4
0041847d  e86ee1feff        call  0x4065f0
00418482  6a00              push  0
00418484  8b4d08            mov   ecx, [ebp+8]
00418487  81c190000000      add   ecx, 0x90
0041848d  e85ee1feff        call  0x4065f0
00418492  8b4d08            mov   ecx, [ebp+8]
00418495  668b550c          mov   dx, word ptr [ebp+0xc]
00418499  66899124020000    mov   word ptr [ecx+0x224], dx   ; ★ cursor[+0x224] = sub_id
004184a0  33c0              xor   eax, eax
004184a2  8be5              mov   esp, ebp
004184a4  5d                pop   ebp
004184a5  c20800            ret   8
```

⇒ 签名：`void setSub(ECLSRC, u32* cursor, i16 sub_id)`（`ecx` = `0x4ECCB8`，`[esp+4]` = cursor，`[esp+8]` = sub_id）。
⇒ **`sub_start(id) = *(u32*)( *(u32*)(0x4ECCB8+4) + id*4 )`**；同时把 `cursor+0x224` 置为该 sub_id。

### 3.3 `call`（0x34）与 `ret`（0x35，推定）

```asm
; opcode 0x34 case @ 0x4197E2
004197e2  8b4df4            mov   ecx, [ebp-0xc]          ; 当前指令
004197e5  8b510c            mov   edx, [ecx+0xc]          ; ins[+0xC] = sub_id（操作数）
004197e8  52                push  edx
004197e9  8b55f4            mov   edx, [ebp-0xc]
004197ec  8b4d08            mov   ecx, [ebp+8]
004197ef  e8dc830000        call  0x421bd0                ; → sub 调用
004197f4  e906edffff        jmp   0x4184ff                ; 重新取 cursor[0]
; opcode 0x35 case @ 0x4197F9  → call 0x421cb0（返回）
```

`0x421BD0`（`ecx` = host，`edx` = ins，`[esp+8]` = sub_id）：

```asm
00421bd0  55                push  ebp
00421bd1  8bec              mov   ebp, esp
00421bd3  83ec08            sub   esp, 8
00421bd6  56                push  esi
00421bd7  57                push  edi
00421bd8  8975f8            mov   [ebp-8], edx            ; ins
00421bdb  894dfc            mov   [ebp-4], ecx            ; host
00421bde  8b45f8            mov   eax, [ebp-8]
00421be1  0fbf4806          movsx ecx, word ptr [eax+6]     ; ins 长度
00421be5  8b55f8            mov   edx, [ebp-8]
00421be8  03d1              add   edx, ecx
00421bea  8b45fc            mov   eax, [ebp-4]
00421bed  8b88a02c0000      mov   ecx, [eax+0x2ca0]       ; cursor
00421bf3  8911              mov   [ecx], edx              ; ★ cursor[0] = 返回地址（下一条指令）
00421bf5  8b55fc            mov   edx, [ebp-4]
00421bf8  8b8224330000      mov   eax, [edx+0x3324]       ; flags
00421bfe  c1e81a            shr   eax, 0x1a               ; bit0x1A
00421c01  83e001            and   eax, 1
00421c04  85c0              test
00421c06  751b              jne   0x421c33                ; 该标志置位时「不压栈」
00421c08  8b4dfc            mov   ecx, [ebp-4]
00421c0b  8bb1a02c0000      mov   esi, [ecx+0x2ca0]       ; esi = cursor（host+0x7F8）
00421c11  8b55fc            mov   edx, [ebp-4]
00421c14  0fbf82ea2c0000    movsx eax, word ptr [edx+0x2cea] ; 深度
00421c1b  69c028020000      imul  eax, eax, 0x228         ; ★ 帧长 0x228
00421c21  8b4dfc            mov   ecx, [ebp-4]
00421c24  8bb9a42c0000      mov   ecx, [ecx+0x2ca4]       ; = host+0xA20（栈基址）
00421c2a  03f8              add   edi, eax
00421c2c  b9 8a000000       mov   ecx, 0x8a
00421c31  f3a5              rep movsd                     ; ★ 把 cursor（0x228 B）压入栈帧
00421c33  668b5508          mov   dx, word ptr [ebp+8]
00421c37  52                push  edx
00421c38  8b45fc            mov   eax, [ebp-4]
00421c3b  8b88a02c0000      mov   ecx, [eax+0x2ca0]
00421c41  51                push  ecx
00421c42  b9 b8cc4e00       mov   ecx, 0x4eccb8
00421c47  e8 0408ffff       call  0x418450                ; ★ 切 cursor 到 sub 起始
00421c4c  8b55fc            mov   edx, [ebp-4]
00421c4f  8ba aa02c0000      mov   edi, [edx+0x2ca0]
00421c55  83c770            add   edi, 0x70
00421c58  b9 08000000       mov   ecx, 8
00421c5d  be 20ce4e00       mov   esi, 0x4ece20            ; ECLSRC+0x168 = sub 局部寄存器组
00421c62  f3a5              rep movsd                     ; 拷 8 个 dword 到 cursor+0x70
00421c64  8b45fc            mov   eax, [ebp-4]
00421c67  8b88 24330000     mov   ecx, [eax+0x3324]
00421c6d  c1e91a            shr   ecx, 0x1a
00421c70  83e101            and   ecx, 1
00421c73  85c9              test
00421c75  750d              jne   0x421c9e
00421c77  8b55fc            mov   edx, [ebp-4]
00421c7a  0fb f82 ea2c0000   movsx eax, word ptr [edx+0x2cea]
00421c81  83f80f            cmp   eax, 0xf                 ; 深度上限 16
00421c84  7d18              jge   0x421c9e
00421c86  8b4dfc            mov   ecx, [ebp-4]
00421c89  668b91ea2c0000    mov   dx, word ptr [ecx+0x2cea]
00421c90  6683c201          add   dx, 1
00421c94  8b45fc            mov   eax, [ebp-4]
00421c97  668990ea2c0000    mov   word ptr [eax+0x2cea], dx ; ★ 深度 +1
00421c9e  5f                pop   edi
00421c9f  5e                pop   esi
```

⇒ **`call sub_id` = ① `cursor[0] = 下一条指令`（返回地址）② 把整个 cursor（0x228 B）压入 `host+0xA20 + depth*0x228` ③ `0x418450` 把 cursor 切到 `sub_start(sub_id)` ④ 拷 8 个 dword 的 sub 局部寄存器 ⑤ depth++（≤16）。**

### 3.4 Timeline 与 sub 的关系（回答「Timeline 的 (time,offset) 对指向的是指令还是 sub」）

- **Timeline slot 的 `+0xC` 是「指令指针」**（`0x42A8A0` 直接当指令头用：`cmp dword ptr [ecx],0`（time）、`byte[ecx+7]`（难度位）、`movsx …,word[ecx+4]`（opcode））。**不是 sub**。
- **它走的是另一套 opcode 表 `0x42AD60`（0x11 项，opcode `0..0x10`）**，与主解释器的 0xB8 项表完全不同 ⇒ 这些「timeline 命令」位于 ECL 文件里一段**不被主解释器正常执行**的命令区。
- （**至少 op `1` / op `0xF` 两个分支**）命令执行时会走到 `0x42A4E0`（`ecx` = 管理器 `0x577F20`）：**在 480 槽里找一个空闲槽、把管理器模板（0x53D0 B）整个拷进去、再 `0x418450` 设 sub 并跑解释器** —— 即「按时间派生一个脚本对象」。（另一些分支只是置状态字节，例 op `0x10` → `0x42A919: mov byte ptr [0x164D0BB], 1`。）
- ⇒ **章节/段落定位可以走时间轴（Timeline）**：thprac 的 `ECLWarp(time, offset)` 就是把「在 time 时刻执行 offset 处的 timeline 命令」写进 slot。**但要注意：`offset` 指向的是 timeline 命令，不是 sub 名/索引。** boss 段同理（`THPatch` 也用 `ECLWarp`），**没有 th14–18 那种「读 sub 名」的路径**。

---

## 4. Q3 —— ECL 子调用栈（存在、可遍历）

- **栈基址**：`host + 0xA20`（解释器入口 `0x4184CB` 把 `host+0x2CA4` 指向它）。
- **帧长**：**`0x228`**（`0x421C1B: imul eax, eax, 0x228`）。
- **深度**：`*(i16*)(host + 0x2CEA)`；**上限 16（`0xF`）**（`0x421C81: cmp eax, 0xf`）。
- **帧内容**＝**整份 cursor 的快照**（`rep movsd` 拷贝 `0x228` 字节，`0x421C31`），因此：
  - 帧 `+0x000` = **调用者当时的指令指针**（即「上一层」的 `cursor[0]`；返回时被 `0x421D95` 的 `rep movsd` 原样拷回 `host+0x7F8`）
  - 帧 `+0x00C` = 调用者当时的时间
  - 帧 `+0x224` = **调用者当时的 `sub_id`** ★ —— 所以「谁调了当前 sub」可直接读
- **当前层不在帧数组里**，它是**活动 cursor**（`host+0x7F8`）；帧数组里的第 d 项是「第 d+1 层（被调方）所在层的上一层」。
  `[host+0x2CEA]`（i16）= **待返回层数**：`call`（`0x421C97`）`+1`、`ret`（`0x421CE1–0x421CEF`）`-1`（上限 16）。
  证据（`ret` = opcode `0x35` → `0x421CB0`；恢复在 `0x421D95`）：
  ```asm
  00421ce1  668b82ea2c0000    mov   ax, word ptr [edx+0x2cea]     ; 深度
  00421ce8  662d0100          sub   ax, 1
  00421cef  668981ea2c0000    mov   word ptr [ecx+0x2cea], ax     ; ★ 深度 −1
  ...
  00421d95  8b45f8            mov   eax, [ebp-8]                  ; host
  00421d98  0fbf88ea2c0000    movsx ecx, word ptr [eax+0x2cea]     ; 深度
  00421d9f  69c928020000      imul  ecx, ecx, 0x228                 ; ★ 帧长 0x228
  00421da8  8bb2a42c0000      mov   esi, [edx+0x2ca4]             ; = host+0xA20
  00421dae  03f1              add   esi, ecx                      ; esi = 源帧
  00421db3  8bb8a02c0000      mov   edi, [eax+0x2ca0]             ; = host+0x7F8
  00421db9  b98a000000        mov   ecx, 0x8a
  00421dbe  f3a5              rep movsd                           ; ★ 帧 → cursor（恢复）
  ```
- **遍历规则**：
  ```cpp
  int depth = *(int16_t*)(host + 0x2CEA);              // ≥0；上限 16
  for (int d = 0; d < depth && d < 16; ++d) {
      uint32_t frame = host + 0xA20 + d * 0x228;       // 第 d 层「被调方的上一层」
      int16_t  sub   = *(int16_t*)(frame + 0x224);     // 调用者 sub_id
      uint32_t ins   = *(uint32_t*)(frame);            // 调用者指令地址 = pECL + offset
  }
  // 当前层 = *(u32*)(host+0x2CA0) 指向的 cursor（通常 = host+0x7F8；活动 cursor，不在帧数组里）
  ```
- ⚠️ **按总括 §6 Q9，函数栈「不参与」段落判定**（本项只作逆向事实记录）。且 `0x421C06` 存在「flags bit0x1A 置位时不压栈」的分支（推测为「叶/内联调用」形态），遍历时应以 depth 与 `frame+0x224` 合法性做保护。

---

## 5. Q4 —— 宿主捕获点与「主线宿主」

### 5.1 三条硬事实

1. **ECL 资源是静态单例**：`0x4ECCB8` 是**静态 struct**（不是堆对象）。`thprac` 的 `ECLHelper::SetBaseAddr` 目标 `GetMemAddr(0x4ECCB8)` = `*(u32*)(0x4ECCB8) + …` ⇒ 一级对象就是它。**同一时刻整个游戏只有一份「当前 ECL 文件」**（文件由 `ECLSRC+0`/`+4` 决定），所有脚本对象共用它。
2. **宿主对象是静态 manager 内嵌数组的槽**：`MGR = 0x577F20`（ctor `0x42A210`，由 `.rdata` 桩 `0x4B3710` 以 `ecx=0x577F20` 调用 ⇒ 静态）。
   - `MGR + 0x53D0` = **敌/脚本对象数组首址 = `0x57D2F0`**（`0x42B64E`/`0x42BB01`/`0x42C835` 的循环都是 `ptr = 0x57D2F0`、步长 `0x53D0`、上界 `0x1E0`）。
   - `MGR` 自身前 `0x53D0` 字节是**模板对象**（`0x42A4E0` 用 `rep movsd 0x14F4`（= `0x53D0` 字节）把管理器拷进新槽）。
   - `MGR + 0x9DCDD0 = 0xF54CF0` = Timeline slot 数组（§1 ④）。
   - 保留槽：`OBJ(480) = 0xF4F8F0`（解释器若干 handler 的**默认返回对象**，`0x41F11C`/`0x41F28C: mov [ebp-4], 0xf4f8f0`）。
3. **stage 模块是静态 slot**：`0xF54E30`（`MGR+0x9DCF10`）注册于 `0x42C5BB`：
   `[0xF54E38] = 0x42EBF0`（handler：加载 stage ECL）、`[0xF54E3C] = 0x42EE80`（handler：每帧更新）、`[0xF54E4C] = <manager>`。
   加载点 `0x42ED48: push <名字>; mov ecx, 0x4ECCB8; call 0x418330`（名字来自表 `0x4C73CC`）。

### 5.2 捕获点建议（地址 + 命中寄存器）

| 目的 | 地址（VA/RVA） | 覆盖 | 命中时取值 |
| --- | --- | --- | --- |
| **抓「任意脚本切 sub」**（最通用） | **`0x418450`** / `0x18450` | 3B（`55 8b ec`，需 EHOOK 重建 prologue） | 入口处 `ecx` = `0x4ECCB8`、`[esp+4]` = **cursor**、`[esp+8]` = `sub_id`（`ebp` 未建）。⚠️ `cursor` 常为 `host+0x7F8`（此时 `host = cursor − 0x7F8`），但在 `0x3384` 子上下文路径下为 `subctx+8`（此时 `host` 需另行取得，建议以 `0x4184C8` 取 host） |
| **抓解释器宿主**（每帧多次） | **`0x4184C8`** / `0x184C8` | **3B**（`8b 45 08` = `mov eax,[ebp+8]`） | **`[ebp+8]` = host**（`ebp` 已建立） |
| **抓主线 stage ECL 装载** | `0x42ED48` / `0x2ED48` | 5B（`e8 …` call） | `ecx` = `0x4ECCB8`；`[esp+8]` = 文件名字符串指针（`ecldataN.ecl` / `ecldataNsp.ecl`，见附录 §A.4） |
| **抓 stage 模块注册** | `0x42C5D2` / `0x2C5D2` | 3B | `[0xF54E4C]` 被写为 manager ⇒ 之后即可用 `[0xF54E4C]` 作静态兜底 |

> **静态兜底**（无需钩子）：`MGR = 0x577F20`（**它是常量的静态地址，直接可解引用**）；敌/脚本对象 = `0x577F20 + 0x53D0 + i*0x53D0`。

### 5.3 ⚠️ 未定案缺口（必须真机补）

**「主线宿主」是哪一槽，静态未定案**。理由与现状：

- A 代没有 `host+0x11F8 = res` 那类「具名 main 宿主」结构；stage ECL 的运行对象是**通过 timeline 命令 `0x42A4E0` 动态从 480 槽里分配的**（§3.4），因此**主线脚本可能随段落变化而换对象**。
- 因此本报告**不宣称**「读某一个固定槽就是主线」。
- **可落地的判定建议（供真机验证）**：
  1. 用 `0x4184C8` 钩子对所有宿主采样 `(host, cur_sub, offset, time)`；
  2. 主线宿主的判据候选 = **`host` 的 `cursor` 落在 `*(u32*)0x4ECCB8` 所指缓冲内**（即 `pECL ≤ cur_ins < pECL + 文件大小`）**且 `cur_sub` 随关卡推进单调前进**；
  3. 真机三态（标题 / 道中 / boss）采样确认哪些槽属于主线。
- 若真机发现「同一时刻只有一个槽在跑 stage ECL」，则实现可直接用「唯一处于活动且 `cur_sub` 前进的槽」。
- ⚠️ **单个宿主内部还可能有多个 cursor/子上下文**：`host+0x3384` 是**子上下文对象指针数组**（子上下文 cursor@+8、栈@+0x230），`0x41EC2E` 一类路径会把 `host+0x2CA0` 从 `host+0x7F8` 切过去。⇒ 采样时**一律走 `cursor = *(u32*)(host+0x2CA0)`**，不要硬编码 `host+0x7F8`；并考虑「同一帧内遍历该宿主的 0x3384 数组」以捕获并行的子上下文（🟡 数组元素数未定案）。

---

## 6. Q5 —— 解释器命中点与寄存器语义

### 6.1 主解释器（自找定案）

- **函数**：`FUN_004184B0`，**入口 `0x4184B0` / RVA `0x184B0`，函数体 `0x4184B0`–`0x41ECBD`**（末尾 `0x41ECBB: c2 04 00 ret 4`，后接 opcode 跳表）。
- **入参**：`[ebp+8]` = **host（宿主对象）**；`ecx` 被存到 `[ebp-0x144]`（未在正文使用到的地方另查）。
- **返回值**：`-1` = 脚本结束（`0x41862A: or eax,-1`）；`0` = 正常。
- **当前状态**：`[ebp−0xC]` = 当前指令指针（≡ `*(u32*)(*(u32*)(host+0x2CA0))`，通常即 `*(u32*)(host+0x7F8)`）。
- **分发**：`0x4185F1`（取指令）→ `0x4185F4`（取 opcode）→ `0x418623`（跳表 `0x41ECBE`）。
- **调用点**（`call rel32` 扫描）：`0x42A5DC`、`0x42A76E`、`0x42C998`；其中 `0x42A76E` 所在函数 `0x42A680` 又被解释器 handler `0x41D893`/`0x41DA6D`/`0x41F262`/`0x41F3E4`/`0x42517F` 调用。

### 6.2 thprac 现有 4 个 hook 的评价

| thprac 名 | 地址 | 所在 | 可否复用为「每帧读当前位置」 |
| --- | --- | --- | --- |
| `th08_lock_timer1` | `0x437AF3` | 解释器外的初始化/复位路径（该函数另处引用敌表 `0xF54CC0`，见 `0x4379A3`）；字节 `8b 7d f0` | ❌ **非每帧**（只在初始化/复位命中） |
| `th08_lock_timer2` | `0x41CB32` | **解释器内 opcode `0x84` case**（`0x41CB26` 起），字节 `74 16`（`je`） | 🟡 **可用但非无条件**：只在执行到 opcode 0x84 时命中。命中时 `[ebp+8]`=host、`[ebp−0xC]`=当前指令 |
| `th08_lock_timer3` | `0x41C44F` | **解释器内 opcode `0x7F` case**（`0x41C443` 起），字节 `74 16`（`je`） | 🟡 同上（opcode 0x7F），且该 case 前一条 `0x41C432–0x41C43A` 写 `[0xF54CEC]`（管理器尾部字段） |
| `th08_lock_timer4` | `0x42F34D` | `0x42F340` 里的**单字段 setter**：`mov dword ptr [eax+0x24], ecx`（字节 `89 48 24`） | ❌ **与「当前位置」无关**（thprac 注释的 “decrease time (update)” **属误读**，该地址不是时间递减） |

> ⇒ **推荐做法（与 th12/th13/th15 同风格）**：**不挂 VM 钩子**。在 IGI 渲染（帧间稳定态）直接读宿主 cursor（§2.3 的表达式）。
> 若实现上确实需要一个「无条件每帧」的触发点：用 **`0x4184C8`（3B，`mov eax,[ebp+8]`）** 或分发头 **`0x4185F1`**；两者都在解释器内、`[ebp+8]`=host 恒成立，但**会对所有脚本对象命中**，必须按 §5.3 的判据做主线过滤。

### 6.3 「谁驱动、每帧几次」

- stage 模块每帧 handler `0x42EE80` → 对 480 槽逐个 `call 0x42BC90`（槽更新）；`0x42C3B0`/`0x42C660` 一族按帧驱动；`0x42C998`（在 `0x42C660` 族内）调解释器。
- **每次 `call 0x4184B0` = 跑一个脚本对象的一帧**（内部 while 直到时间未到 / 脚本结束）；`0x4185F1` 的命中次数与该帧内可执行指令条数同阶（**不是**「每帧固定几次」）。

---

## 7. Q6 —— 名字来源：**运行期不保留 sub 名**

**证据**

1. 加载器 `0x418330`（§3.1）全文只处理 `+8` 的 16 个 dword与 `+0x48` 的 sub 偏移表，**没有任何名字/字符串解析**。
2. 头字段只有 `subCount`（i16）与 `slotCount`（i16），**没有名字区指针**。
3. 全 exe 字符串扫描：与 ECL 相关的只有 **文件名字符串**（`ecldata1.ecl` … `ecldata8sp.ecl`，以及角色专用 `ecldata_al/rm/sk/ym/yk/yy`，见附录 §A.4），**没有任何 sub 名串**（对照 C 代 th15 有大量 `Boss*`/`MainSub*` 名串）。
4. exe 内 **`0x4C73CC` / `0x4C73F0` 表**把「ECL 文件索引」映射到文件名；`0x4C7054` 映射到 `.end` 文件 —— 均为**文件级**，非 sub 级。

**结论**：`❌ 运行期没有 sub 名`。因此

- 需要 **离线「偏移 ↔ 段落」表**；
- 本作**无 `.ecl`/`.decl`**，离线**也无法**从脚本枚举 sub 名；
- **可行资产 = thprac 自己的章节 warp 表**：`THStageWarp` / `THPatch` 里每个段落显式给出了 `ECLWarp(<time>, <offset>)`，其 `(time, offset)` **就是**「段落入口」在该引擎里的坐标（`offset` 是 `pECL` 内的字节偏移，与运行期 `cur_ins − pECL` **同一坐标系**）。这就是 §附录 表的来源。
- 记录名（「记录名」列）只能沿用 thprac 既有的章节文案表（`th_sections_str[3][4][122]`）。

> 🟡 缺口：**无 `.ecl` 素材**，无法核对「`offset` 处的 timeline 命令」与「该 offset 是否正是段落第一条可执行指令」。故附录的 `(time, offset)` 一律标注 **候选、未真机**。

---

## 8. Q7 —— 机体 / 子机 / 难度 / 关卡

| 名称 | VA / RVA | 类型 | 判据（反汇编） | 置信度 |
| --- | --- | --- | --- | --- |
| `SHOTTYPE_ADDR` | `0x164D0B1` / `0x124D0B1` | **u8** | `0x405163: movzx edx, byte ptr [0x164d0b1]` → 作表索引（`lea ecx,[ecx+edx*4+0x164cd3c]`） | ✅ 地址/类型确证，组合编码待真机 |
| `DIFF_ADDR` | `0x160F538` / `0x120F538` | i32（`GameManager+0x30`，`GameManager = 0x160F508`） | `0x40515A: mov ecx,[0x160f538]; imul ecx, ecx, 0x44`（作难度行距）；`0x403A88` 作字符串表选择子 | ✅ |
| **rankMask**（新登记） | `0x160F53C` / `0x120F53C` | u32（`GameManager+0x34`） | `0x4185C6: movzx ecx, byte[host+0x3330]; mov eax,[0x160f53c]; or eax,ecx; and edx,eax`（难度位检测）；slot 处理器 `0x42A8E9` 同用 | ✅ |
| 关卡选择子（新登记） | `0x164D0B8` / `0x124D0B8` | **i16** | `0x403A5E: movsx eax, word[0x164d0b8]; cmp eax, 0xcd; jge …`（`≥0xCD` 走 Extra 分支）；`0x42EC75` 同用 | 🟡 语义（关卡 vs END 选择）待真机 |
| `pause` | `0x164D0BA` / `0x124D0BA` | u8 | thprac 既有；`0x42A919: mov byte ptr [0x164d0bb], 1` 说明 `0x164D0BB` 是紧邻的另一状态字节 | ✅（既有三态已验证） |
| ECL 文件索引 | `0x164D2CC` / `0x124D2CC` | i32 | `0x42ED36: mov eax,[0x164d2cc]; mov ecx,[eax*4+0x4c73cc]`（选 `ecldataN.ecl`） | ✅ |
| `SPELLCARD_ID_ADDR` | `0x4EA678` / `0xEA678` | i32 | thprac 既有（`0x3263`） | 🟡 |
| `STD Buffer` / `STD Time` | `0x4E4824` / `0x4E483C` / `0x4E4844` | — | 仅 6 处 xref（`0x434DBD`、`0x43B719/30/41/58/69`），thprac `STDJump` 用 `SetFile(*(void**)0x4E4824)` | 🟡 = **另一条独立时间轴**（`stageN.std`/`stageN_s.std`），管关底/剧情；与 ECL 的 Timeline **不是同一个** |

**th08 的「机体 × 子机」**：thprac 用 `THPRAC_GAMEROLL_TH08_SHOTTYPES` 显示 4 组合 × 2 队。运行期本机能确认到的只有 `0x164D0B1` 这个**字节**（作表索引）。「队 / 角色」的第二个维度是否也编码在同一字节（或是别处的另一字节），**未在本轮反汇编中定案** —— 标 🟡。

---

## 9. Q8 / Q9 —— 练习跳转的影响 与 段落判定口径

### 9.1 Q8：warp 之后读「当前位置」是否立刻反映新段落

**两条路径，尺度不同**：

| 手段 | 作用于 | 生效时机 | 依据 |
| --- | --- | --- | --- |
| `ECLJump` / `ECLCallSub` / `ECLSetTime` / `ECLTimeFix` / `ECLSetHealth`（`thprac_th08.cpp:1440–1477`） | **运行期 ECL 缓冲字节**（`ECLHelper` 基址 = `*(u32*)0x4ECCB8`） | **下一次该宿主执行到该处即读到**（基本＝同帧/下一帧） | op 0x04 `0x4186F1` 读 `ins[+0xC]`/`ins[+0x10]`；op 0x86 `0x41CCFC` 读 time/sub_id |
| `ECLWarp`（`thprac_th08.cpp:1407`） | **Timeline slot**（`0xF54CF8 + i*0x10`：`+8`=time、`+0xC`=ins） | **下一个 stage-update 帧**：`0x42C7D5` 循环扫 slot → `0x42A8A0` 判 `slot+8 == ins->time` → 执行 slot 命令（→`0x42A4E0` 派生脚本） | `0x42C7E4/0x42C817/0x42C82A`、`0x42A8B3/0x42A8CB` |
| `STDJump` / `STDStage4Fix` | **STD 缓冲**（`*(u32*)0x4E4824`） | 独立时间轴，下一帧由 STD 驱动 | `thprac_th08.cpp:1489–1508` |

⇒ **结论**：warp 之后读「当前位置」**会立刻（或被 warp 的脚本对象一执行就）反映新段落**，但要注意：

- ③ 的 cursor 读法与 ② 的 slot 写法是**两个不同对象**；`ECLWarp` 先把 slot 写进去，再由 slot 命令**派生新脚本对象**（`0x42A4E0`）→ 因此「读哪个宿主」必须先确定（§5.3）。
- 采样应落在**帧间稳定态**（IGI 渲染），避开「slot 已写、脚本尚未执行」的一帧。

### 9.2 Q9：判定口径（**A 代可落地建议**）

**口径不变**：**路过式 checkpoint（latch）** —— 经过白名单条目即置「当前段落」，不在表内 **no-op**（不回退 `main`），本局首次命中前显示 `main`。

**A 代读取方案（本作建议，🟡 未真机）**：

1. **读哪个字段**（先 `cursor = *(u32*)(host+0x2CA0)`，通常 = `host+0x7F8`）：
   - **主键 = 指令偏移** `offset = *(u32*)(cursor) − *(u32*)0x4ECCB8`。理由：`ECLWarp` 的落点就是「`pECL + offset`」的**精确字节偏移**，与运行期 `cur_ins − pECL` 同坐标系、可**精确相等**匹配（不需要区间表）。
   - **辅键 = 时间值** `*(i32*)(cursor+0x0C)`（常用于分支/对话差异导致的同级不同落点消歧，以及日志可读性）。
   - **辅键 = sub_id** `*(i16*)(cursor+0x224)`（若真机发现「段落入口恰好是一个独立 sub 起点」，可用它做廉价预筛）。
2. **哪个 slot**：**不看 Timeline slot**。Timeline 是「写入侧的定时跳转队列」，它的 (time, ins) 是**段落入口的坐标**，不是「当前位置」。当前位置一律从**宿主的 cursor 帧**读。
3. **checkpoint「入口」列记什么**：**记 `(time, offset)`**（= thprac 该段落 `ECLWarp` 的第一个 pair），**不记 sub 名**（本作无 sub 名）。建议 CSV「入口」列写成 `时间值/偏移`（例 `340/0x9C38`），与 th10–18 的「sub 名」口径显式分叉。
4. **匹配规则**：`offset` 精确相等优先；若某段落入口落在**容器内部**（`main` 型超长脚本）导致 `offset` 会先经过若干无关指令，再退化到「区间 + 时间上限」二级匹配（参考 th10 的 `sub名+0x偏移` 思路）。
5. **多 lane**：`ECLWarp` 一次最多写 4 个 slot（`_requests` §1）。**第 1 个 pair = 该段落主线入口**；第 2–4 个 pair 是辅助 lane（背景/符卡/演出脚本），**不要登记为 checkpoint**（候选，见附录 §A.2 注）。

---

## 附录 A —— 候选段落入口值表（候选、**未真机**）

> 来源：`thprac/src/thprac/thprac_th08.cpp` 的 `THStageWarp`（道中 portion，`:1513–1694`）与 `THPatch`（boss section，`:1695–2769`）。
> 取值 = 每次 `ECLWarp(...)` 的**第一个 `(time, offset)` 对**（= `ECLWarp` 的 `time1/offset1`）；括号内为其余 pair。
> ⚠️ 全部为**候选**：`offset` 指向的是「timeline 命令」（§3.4），**未真机确认它就是该段落的第一条可执行指令**。

### A.1 `THStageWarp`（道中，section 10000+stage*100+portion）

| 关卡 | portion | time | offset | 其余 pair（辅助 lane） |
| --- | --- | --- | --- | --- |
| st1 | 1 | 340 | `0x9C38` | (20, `0xAD18`) |
| st1 | 2 | 2875 | `0xABA4` | (20, `0xAD18`) |
| st2 | 1..4 | 400 / 990 / 2350 / 3350 | `0xA194` / `0xA8D8` / `0xB418` / `0xBA78` | 各 (20, `0xC0CC`) |
| st3 | 1 | 340 | `0xB258` | (20,`0xB91C`) (20,`0xBB74`) (20,`0xC0AC`) |
| st3 | 2 | 1220 | `0xB368` | (200,`0xBA14`) (20,`0xBB74`) (20,`0xC0AC`) |
| st3 | 3 | 3103 | `0xB664` | (380,`0xBACC`) (20,`0xBB74`) (160,`0xC304`) |
| st4 | 1..4 | 340 / 1180 / 2240 / 3180 | `0xA2D4` / `0xA834` / `0xA8AC` / `0xABCC` | p1/p2 (20,`0xBAFC`)；p3/p4 (360,`0xBED4`) |
| st4 | 5 | 4962（dlg）/ 4903 | `0xB280` / `0xB2D8` | (360, `0xBED4`) |
| st4 | 6 | 5363 | `0xB598` | (360, `0xBED4`) |
| st5 | 1..4 | 340 / 1180 / 2240 / 3180 | `0xC538` / `0xCAB8` / `0xCB30` / `0xCE50` | p1/p2 (20,`0xDD80`)；p3/p4 (360,`0xE158`) |
| st5 | 5 | 4962（dlg）/ 4903 | `0xD504` / `0xD55C` | (360, `0xE158`) |
| st5 | 6 | 5363 | `0xD81C` | (360, `0xE158`) |
| st6 | 1..5 | 200 / 1040 / 2990 / 4651 / 5971 | `0xA674` / `0xA694` / `0xAFDC` / `0xB0E8` / `0xB648` | — |
| st7 | 1,2 | 200 / 1140 | `0xC6F4` / `0xC714` | — |
| st8 | 1,2 | 200 / 1140 | `0x10324` / `0x10364` | — |
| st9（LW?） | 1..7 | 340 / 1010 / 2900 / 5237 / 5947 / 6757 / 8337 | `0x12544` / `0x127A8` / `0x12B6C` / `0x132B8` / `0x136B4` / `0x13774` / `0x151F4` | p1 (20,`0x1535C`)；p2 (210,`0x155C0`)；p3 (390,`0x15824`)；p4 (810,`0x15D60`)；p5–p7 (20,`0x1535C`) |

### A.2 `THPatch`（boss / 中boss section，按枚举顺序 `THPrac::TH08::TH08_ST*`）

| section | time | offset | 备注 |
| --- | --- | --- | --- |
| ST1_MID1 / MID2 | 2935 | `0xAB78` | + (20, `0xAD18`)；MID2 另有 `ECLSetTime` |
| ST1_BOSS1 | 4175 | `0xAC70`(dlg) / `0xAC84` | 单 pair |
| ST1_BOSS2 / BOSS3 / BOSS4 | 4175 | `0xAC84` | |
| ST1_LS | 4176 | `0xACD0` | + (0, −1) |
| ST2_MID1 / MID2 / BOSS1..BOSS5 | 4870 | `0xC020` | + (0, −1) |
| ST2_LS | 4871 | `0xC07C` | + (0, −1) |
| ST3_MID1 / MID2 | 3080 | `0xB5D8` | + (380,`0xBACC`) (0,`0xBB74`) (160,`0xC304`) |
| ST3_BOSS1..BOSS6 | 4663 | `0xB87C` | + (0,−1)×3 |
| ST3_LS | 4663 | `0xB8CC` | + (0,−1)×3 |
| ST4A_BOSS1 | 4962 | `0xB26C`(dlg) / `0xB280` | + (360, `0xBED4`)；另 `STDJump/STDStage4Fix` |
| ST4A_BOSS2..BOSS4 | 4962 | `0xB280` | 同上 |
| ST4A_BOSS5..BOSS9 | 6923 | `0xBA50` | + (0,−1) |
| ST4A_LS | 6923 | `0xBAAC` | + (0,−1) |
| ST4B_BOSS1 | 4962 | `0xD4F0`(dlg) / `0xD504` | + (360, `0xE158`) |
| ST4B_BOSS2..BOSS4 | 4962 | `0xD504` | |
| ST4B_BOSS5..BOSS9 | 6923 | `0xDCD4` | + (0,−1) |
| ST4B_LS | 6923 | `0xDD30` | + (0,−1) |
| ST5_MID1 / MID2 | 4530 | `0xB0BC` | |
| ST5_BOSS1 | 7481 | `0xB788`(dlg) / `0xB79C` | |
| ST5_BOSS2..BOSS7 | 7481 | `0xB79C` | |
| ST5_LS | 7484 | `0xB820` | |
| ST6A_MID1 / MID2 | 3400 | `0xC7D4` | |
| ST6A_BOSS1 | 4022 | `0xC824`(dlg) / `0xC838` | |
| ST6A_BOSS2..BOSS9 | 4022 | `0xC838` | |
| ST6A_LS | 4083 | `0xC89C` | |
| ST6B_MID1 / MID2 | 3490 | `0x104E4` | |
| ST6B_BOSS1 | 4112 | `0x10534`(dlg) / `0x10548` | |
| ST6B_BOSS2..BOSS9 | 4112 | `0x10548` | |
| ST6B_LS1..LS5 | 4234 / 4235 / 4236 / 4237 / 4238 | `0x105AC` / `0x105EC` / `0x1062C` / `0x1066C` / `0x106AC` | |
| ST7_MID1..MID3 | 5155 | `0x13268` | + (810, `0x15D60`) |
| ST7_END_NS1 | 9497 | `0x152B4`(dlg) / `0x152C8` | |
| ST7_END_S1..S10 | 9497 | `0x152C8` | |
| ST7_END_LS | 9678 | `0x1530C` | |
| TH08_LW_1..17 | — | — | **源码里是空 case（`break`）**，无 `ECLWarp` ⇒ 无候选入口 |

> **A.2 注（辅助 lane）**：`ECLWarp` 的第 2–4 个 pair（如 st3 的 `0xB91C`/`0xBB74`/`0xC0AC`、st4–5 的 `0xBED4`/`0xE158`、st9 的 `0x1535C`…）**不是**该段落的入口，而是同帧要一并压进其它 slot 的辅助脚本（推测：背景/符卡/演出）。做 checkpoint 时**只取第 1 个 pair**；`offset == -1` 的 pair（如 `(0, -1)`）连指针都没写，只是「把时间置 0」。

### A.3 `STDStage4Fix` / `STDJump`（独立 STD 时间轴，**不入 checkpoint 表**）

```text
STDJump(0xF48, 38, 4562) / STDJump(0xF48, 56, 6558)   // 写 STD 缓冲 0xF48 处的 (ord, time)
STDStage4Fix(): std[0xED8]=1, std[0xF14]=1（+ 末符卡时 0x1394/0xF48 各写一组）
```
⇒ STD（`stageN.std` / `stageN_s.std`，见 §A.4 表）是**关底/剧情阶段**的独立时间轴，**与 ECL 段落入口无关**；本轮不作 checkpoint 依据。

### A.4 exe 内 ECL / STD 文件表（供离线核对）

| 表 | 位置（VA / RVA） | 索引变量 | 内容 |
| --- | --- | --- | --- |
| ECL（正文） | `0x4C73CC` / `0xC73CC` | `0x164D2CC` | `[0..8]` = `ecldata1..8.ecl`（`4a`/`4b` 分列）；`[9..]` = `ecldataNsp.ecl` |
| ECL（sp） | `0x4C73F0` / `0xC73F0` | `0x164D2CC` | `ecldata1sp..5sp.ecl` |
| STD | `0x4C731C` / `0xC731C`（`stage1.std`…`stage8.std`）与 `0x4C7340` / `0xC7340`（`stageN_s.std`） | — | 每区块 8 项，按关卡索引；相邻还有 `0x4C72F8` 的 `stgNbg.anm` |
| `.end` | `0x4C7054` / `0xC7054` | `0x164D0B8` | `end00a..end03c.end` |
| 敌方 ANM | `0x4C7364` / `0xC7364` | `0x164D2CC` | `stg1enm.anm` … `stg8enm.anm` |

> 另有角色专用 ECL：`ecldata_al/ec/law/…`（实测字符串：`ecldata_al`、`ecldata_rm`、`ecldata_sk`、`ecldata_ym`、`ecldata_yk`、`ecldata_yy`）。

---

## 工具与复现

- 脚本目录：`/private/var/folders/cn/1_b1wvxj3wqc3dtzw9lz8xy00000gn/T/opencode/th08work/`（`x1.py`–`x58.py`，均由 `pe.py` 驱动；`pe.py` 为随任务提供的 helper：`P.off(va)`＝`get_offset_from_rva(va−ImageBase)`、`P.dis(va,n)`、`P.data_of`、`P.xrefs`）。
- 关键手法：
  1. **原始字节全文搜索 4 字节小端立即数**（找 `0xF54CC0`/`0x9DCDxx`/`0x57D2F0`/`0x577F20` 等静态全局与「`[reg+disp32]` 的 disp」）；
  2. **`E8 rel32` 全 `.text` 扫描**判 `call` 目标（`xrefs` 只找绝对立即数，**找不到直接 call**，这是本作最容易踩的坑）；
  3. **从 `int3` 填充反查函数入口**（`d.rfind(b'\xcc\xcc\xcc\xcc')`）；
  4. **跳表定位**：`0x418623: jmp dword ptr [edx*4+0x41ecbe]` → 读 0xB8 项得到全部 opcode case 地址；`0x42A912: jmp dword ptr [edx*4+0x42ad60]` → slot 命令表。
- 复核要点（供后续 E2/E3 用）：
  - 「当前位置」三字段：`host+0x7F8` / `host+0x804` / `host+0xA1C`；
  - 「sub 表」：`*(u32*)(0x4ECCB8+4)`（= `pECL+0x48`）；
  - 「Timeline」：`0xF54CF0 + i*0x10`（`+8` = time、`+0xC` = ins），`i < *(i16*)(pECL+6)`；
  - 「宿主数组」：`0x57D2F0 + i*0x53D0`（`i < 0x1E0`）。
- ⚠️ 未跑：任何真机；任何 `.ecl` 解包（本机无 th08 素材）。

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-19 | 建档（本报告，E1）。静态定案 th08 的 Q1–Q9。**Q1**：th08「当前执行到哪」= **指令指针为主**、并存 sub_id 与时间值 —— `cur_ins = *(u32*)(host+0x7F8)`（cursor[0]）、`cur_time = *(i32*)(host+0x804)`（cursor+0xC）、`cur_sub = *(i16*)(host+0xA1C)`（cursor+0x224）；`offset = cur_ins − *(u32*)0x4ECCB8`。**Q2**：`ECLSRC = 0x4ECCB8`（静态单例，`+0`=pECL、`+4`=sub 表=pECL+0x48）；`sub_start(id) = *(u32*)(*(u32*)(0x4ECCB8+4)+id*4)`；ECL 文件头 = `{u32 magic 0x800; i16 subCount@+4; i16 slotCount@+6; u32[16]@+8; u32[subCount]@+0x48}`（加载器 `0x418330` 把偏移改成绝对）；`call sub`=opcode 0x34→`0x421BD0`→`0x418450`；**Timeline 与 sub 无关**（slot `+0xC` 是指令指针，走 17 项表 `0x42AD60`）。**Q3**：显式调用栈（基址 `host+0xA20`、帧长 `0x228`、深度 `*(i16*)(host+0x2CEA)` ≤16、帧 `+0x224`=该层 sub_id）。**Q4**：**A 代无「具名 main 宿主」**；ECL 资源静态单例、宿主为静态 manager `0x577F20` 内嵌数组（`0x57D2F0`、步长 `0x53D0`、480 槽）；捕获点 `0x418450`（3B，`host=[ebp+8]−0x7F8`）或 `0x4184C8`（3B，`[ebp+8]`=host）；**「主线是哪一槽」未定案（🟡 缺口）**。**Q5**：ECL 主解释器 `FUN_004184B0`（`0x4184B0`–`0x41ECBD`），opcode 跳表 `0x41ECBE`（0xB8 项），`[ebp+8]`=host、`[ebp−0xC]`=当前指令；thprac 现有 4 hook 的裁定：`0x41CB32`/`0x41C44F` 可用但非无条件每帧、**`0x42F34D` 与当前位置无关（thprac 注释误读）**、`0x437AF3` 非每帧。**Q6**：**运行期不保留 sub 名**（加载器无名字解析、exe 无 sub 名串）⇒ 只能离线「偏移↔段落」映射，且本作无 `.ecl` ⇒ **可行资产 = thprac 自己的 `ECLWarp` 表**。**Q7**：`0x164D0B1`(u8 shottype)、`0x160F538`(i32 diff=GameManager+0x30)、新登记 `0x160F53C`(rankMask=GameManager+0x34)、`0x164D0B8`(i16 关卡/END 选择)、`0x164D2CC`(ECL 文件索引)；「机体×子机（4 组合×2 队）」第二维编码**未定案（🟡）**。**Q8**：`ECLJump/ECLCallSub/ECLSetTime/ECLTimeFix` 直接改写运行期缓冲（下一帧即读到）；`ECLWarp` 写 Timeline slot（下一个 stage-update 帧由 `0x42C7D5` 扫描 → `0x42A8A0` 执行）。**Q9**：沿用 latch；**A 代读取方案 = 以 `offset` 为主键、`time` 为辅键**，checkpoint「入口」列记 **`(time, offset)`（非 sub 名）**，`ECLWarp` 只取第 1 个 pair。**附录 A** 抽出 `THStageWarp`/`THPatch` 全部候选 `(time, offset)` 表（`LW_1..17` 为空 case）。**补记**：① 「当前位置」的权威写法 = 先 `cursor = *(u32*)(host+0x2CA0)`，再读 `*(u32*)(cursor)`/`cursor+0xC`/`cursor+0x224`（`host+0x7F8` 只是主 cursor 的默认值）；② 新发现 **`host+0x3384` = 子上下文对象指针数组**（子上下文 cursor@+8、栈@+0x230，`0x41EC2E` 类路径会切换过去）⇒ 单宿主内可并行多个 cursor（🟡 元素数未定案）。证据：`capstone` 只读反汇编（脚本 `th08work/x*.py`）+ 全文立即数/`E8 rel32`/跳表扫描。**未真机三态；无 `.ecl` 素材，未做离线脚本核对（🟡）**。 |

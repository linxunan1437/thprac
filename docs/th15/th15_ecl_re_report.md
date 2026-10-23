# TH15 ECL 段落监控 — 逆向结论（Q3 函数栈 / Q8 练习跳转补丁 / Q9 段落判定口径）

> 对应需求：`docs/th15/th15_ecl_re_requests.md`；总括：`docs/logger/logger_re_disasm_overview.md`。
> 二进制：`../exes/th15.exe`，951808 B，PE `TimeDateStamp = 1444028071`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**）。`textSize = 0xBCA00`，与 `thprac_launcher_games_def.h` th15 ORIGINAL 条目一致。
> 方法：`pefile` + `capstone 5.0.7` 只读反汇编，并与本机 `../th15decode/th15.exe_decompiled.c`（Ghidra 全量反编译）交叉核对；
> 另用离线 `.ecl` 解码（`../th15decode/data/st01.ecl`）核实 `main`→`MainSubNN` 结构。
> ECL / decl 素材：`../exes/ecl/th15/`（原始 `.ecl` 23 份）、`../exes/ecl_dump/th15/`（`thecl -d` 反编译 `.decl` 23 份）；`st01.ecl` 与上文 `../th15decode/data/st01.ecl` 为同一脚本的两处副本。清单与来源见 `docs/logger/decompile_materials.md`。
> 状态：**静态定案（🟡 已定案、未真机三态）**。Q1/Q2/Q4/Q5/Q6 引用需求文件，本文只补 **Q3 / Q8 / Q9**。
> 地址口径：正文以 **VA** 为主（本机源码/反编译器同口径），并在关键处标注 **RVA = VA − 0x400000**。

---

## 0. 结论速览

| 问题 | 结论 | 一句话 |
| --- | --- | --- |
| **Q3 函数栈** | ✅ **存在可遍历的显式子调用栈** | 栈指针 `SP = *(u32*)(thread+0x100C)`，栈底 `thread+0xC`，帧长 `0x10`；`call`(`0x0B`) 压帧 / `ret`(`0x0A`) 退帧。栈顶帧存的是**调用者**，当前 sub 是 `thread+4`。 |
| **Q8 练习跳转补丁** | ✅ **不影响 subIdx/名字判读** | `ECLJump` 只把 `goto`(`opcode 0x0C`) 写进运行期 ECL 缓冲的指令体；`goto` 只改 `thread+8`(ip) 与 `thread+0`(time)，不改 `thread+4`，也不动 sub 表。 |
| **Q9 段落判定口径** | ✅ **路过式 checkpoint（latch）** | CSV 即白名单：当前 sub ∈ 表 → 置当前段落；**不在 → no-op**（不回退 `main`）。**每个 checkpoint 只登记 1 个入口函数** = warp 落点所进入的 sub（**含 async 落点**）。 |

> 关键地址（VA / RVA）：解释器分发 `FUN_0048CA80` = `0x48CA80` / `0x8CA80`；压帧 `FUN_0048C800` = `0x48C800` / `0x8C800`；
> `ret`(case 0xA) `0x48CB1A` / `0x8CB1A`；`call`(case 0xB) `0x48CCA8` / `0x8CCA8`；越界护栏 `FUN_0048F640` = `0x48F640` / `0x8F640`。

---

## 1. Q3 函数栈（子调用链）

### 1.1 结论

1. **有显式子调用栈。** 它内嵌在“线程/脚本上下文”对象（`thread = *(u32*)(host+0xC)`，即既有 `GetEclSubName` 里的 `thread`）内，随解释器自动维护：
   - **栈指针**：`SP = *(u32*)(thread + 0x100C)`（int，字节偏移，**`0` = 空栈/顶层**）。
   - **栈存储**：`thread + 0xC + SP`（dword 数组，向下为更早的帧）。
   - **帧长**：`0x10` 字节。
2. **栈顶 ≠ 当前 sub。** 当前 sub 是 `*(i32*)(thread+4)`（Q1 已答）。每帧保存的是**调用者**的返回上下文；当前 sub 不在栈上（故“栈顶=当前 sub”为**否**）。
3. `call`(opcode `0x0B`) 压入一帧后切到被调 sub；`ret`(opcode `0x0A`) 弹出该帧并恢复调用者。**同步**子调用走此链；**异步** `spawn`(opcode `0x0F`/`0x15`) 走**另一个 thread 对象**（见 §1.5）。

### 1.2 线程对象与栈布局

线程对象字段（VA 偏移；`thread` 即 `*(host+0xC)`）：

| 偏移 | 类型 | 语义 |
| --- | --- | --- |
| `+0x00` | float | 当前 ECL 时间（Q1/report5 已答） |
| `+0x04` | i32 | **当前 sub 索引**（当前段落判定的入口） |
| `+0x08` | i32 | 当前 sub 内的执行偏移（ip offset） |
| `+0x0C` | — | **调用栈数组起点**（帧存储基址） |
| `+0x100C` | i32 | **调用栈指针 SP**（`0` = 空） |
| `+0x1010` | i32 | 栈局部变量/表达式临时栈的配对指针（`0x28`/`0x29` 使用） |
| `+0x1018` | u32 | 所属宿主对象（`host`）指针 |

单个栈帧（`base` 处，`base = thread + 0xC + SP − 0x10`）：

| 帧内偏移 | 类型 | 语义 |
| --- | --- | --- |
| `+0x00` | u32 | 上一帧的 `SP`（= 本帧基址，退栈后恢复为此值） |
| `+0x04` | float | 调用时的 `thread+0`（时间） |
| `+0x08` | i32 | 调用时的 `thread+8`（ip offset） |
| `+0x0C` | i32 | **调用者的 sub 索引** |

> 因此“帧内 `+0x0C`”才是我们想要的**祖先 sub**；“帧内 `+0x00`”是遍历到上一帧的链指针。

### 1.3 证据链（静态）

**(a) 主分发自证线程布局** — `FUN_0048CA80`（`0x48CA80`，反编译 line 95957 起）：

```asm
0048ca8e  8bf9            mov  edi, ecx                 ; edi = thread（C: param_1）
...
0048ca9d  8b5708          mov  edx,[edi+8]              ; thread+8  = ip offset
0048caa9  8b4f04          mov  ecx,[edi+4]              ; thread+4  = sub 索引
0048cab5  8b8718100000    mov  eax,[edi+0x1018]         ; thread+0x1018 = host
0048cac2  8b80f8110000    mov  eax,[eax+0x11f8]         ;   host+0x11F8 = res
0048cac8  8b808c000000    mov  eax,[eax+0x8c]           ;     res+0x8C   = sub 表
0048cace  8b44c804        mov  eax,[eax+ecx*8+4]        ;       表项[+4]    = sub 起始
```

**(b) `ret`（opcode `0x0A`）** — case @ `0x48CB1A`（反编译 line 96026 起）：

```asm
0048cb1a  8d4f0c          lea  ecx,[edi+0xc]            ; ecx = thread+0xc（栈基址）
0048cb22  83bf0c10000000  cmp  dword ptr [edi+0x100c],0 ; SP == 0 ?
0048cb29  7461            je   0x48cb8c                 ;   空 → subIdx = -1（结束）
0048cb2b  838100100000fc  add  dword ptr [ecx+0x1000],-4; SP -= 4
0048cb38  8b0408          mov  eax,[eax+ecx]            ;   pop
0048cb3b  894704          mov  [edi+4],eax              ; thread+4  = 帧[+0x0C]（调用者 sub）
0048cb3e  ...                                            ; SP -= 4 → thread+8  = 帧[+0x08]（ip）
0048cb51  ...                                            ; SP -= 4 → thread+0  = 帧[+0x04]（time）
0048cb63  ...                                            ; SP -= 4 → thread+0x100C = 帧[+0x00]（旧 SP）
```

**(c) `call`（opcode `0x0B`）** — case @ `0x48CCA8` → `FUN_0048C800`（`0x48C800`，反编译 line 95839 起）：

```c
/* FUN_0048c800(this=thread, param_1=thread, param_2=0) —— 只列压帧部分 */
iVar3 = *(int *)(param_1 + 0x100c);        // SP0（本帧基址）
...
uVar4 = *(undefined4 *)(param_1 + 0x100c); // = SP0（上一帧的 SP）
*(undefined4 *)(*(int *)(param_1 + 0x100c) + 0xc + param_1) = uVar4;   // 帧[+0x00] = SP0
*(int *)(param_1 + 0x100c) += 4;
*(...(SP)+0xc+param_1) = *(undefined4 *)this;          // 帧[+0x04] = time
*(int *)(param_1 + 0x100c) += 4;
*(...(SP)+0xc+param_1) = *(undefined4 *)((int)this + 8);// 帧[+0x08] = 旧 ip offset
*(int *)(param_1 + 0x100c) += 4;
*(...(SP)+0xc+param_1) = *(undefined4 *)((int)this + 4);// 帧[+0x0C] = 旧 sub 索引（调用者）
*(int *)(param_1 + 0x100c) += 4;                        // SP = SP0 + 0x10
/* 随后： thread+4 = 被调 sub 索引（FUN_0048f340 按名解析）；thread+8 = 0；thread+0 = 0 */
```

> 即：压栈顺序 `{旧SP, time, 旧ip, 旧subIdx}`；`ret` 从栈顶向下依次恢复 `subIdx / ip / time / SP`，与 (b) 完全对称。**帧长 = 0x10**。

**(d) 越界护栏** — `FUN_0048F640`（`0x48F640`，case `0x28` 栈局部分配）：

```asm
0048f647  8bb100100000    mov  esi,[ecx+0x1000]     ; 当前 SP
0048f64d  03d6            add  edx,esi              ; SP + size
0048f64f  81fa00100000    cmp  edx,0x1000
0048f655  7c08            jl   0x48f65f            ; SP+size < 0x1000 才继续
0048f657  83c8ff          or   eax,0xffffffff       ; 否则失败（栈溢出保护）
```

⇒ **栈区上限 `0x1000` 字节**（`thread+0xC .. thread+0x100C`）。

### 1.4 遍历规则与深度上限

```cpp
// thread = *(u32*)(host + 0x0C);            // 既有 GetEclSubName 已取到
uint32_t sp = *(uint32_t*)(thread + 0x100C);  // 0 = 无 caller
int n = 0;
while (sp >= 0x10 && sp < 0x1000 && n < MAX_DEPTH) {
    int32_t caller = *(int32_t*)(thread + 0x0C + sp - 4);   // 帧[+0x0C] = 调用者 sub 索引
    if (caller < 0 || caller >= count) break;               // -1 = 根哨兵（见 §3.3）/ 越界保护
    ancestors[n++] = *(char**)(table + (uintptr_t)caller * 8);  // 表项 +0 = 名字
    sp = *(uint32_t*)(thread + 0x0C + sp - 0x10);           // 帧[+0x00]（通常即 sp-0x10）
}
// 祖先序：ancestors[0]=直接调用者，…，最后=最外层；当前 sub 本身由 thread+4 给出，需先行判定。
```

- **遍历链**：`sp` 每次以上一帧的 `帧[+0x00]`（= 上一帧的 SP）回退，逐帧读 `帧[+0x0C]`。正常情况下 `帧[+0x00] == sp − 0x10`。
- **深度上限**：栈区 `0x1000` 字节 ÷ 帧长 `0x10` ⇒ **物理上限 256 帧**（实际因表达式/栈局部共用同一 `SP` 而更浅；ZUN 脚本调用链一般仅数层）。`FUN_0048F640` 的 `0x1000` 护栏只作用于栈局部分配，`call` 压帧本身未见独立深度检查。
- **共用同一 `SP`**：表达式临时值/栈局部（`0x2a/0x2b/0x2c/0x28/0x29` 等）也走 `thread+0x100C`。**只应在“指令边界”（帧间稳定态，例如 IGI 渲染时）遍历**；此时临时栈已清空，`SP` 即调用栈顶。
- **空指针/越界保护**：既有的 `res`/`table`/`count` 保护保持不变；遍历时对 `sp`、`caller` 再加范围校验（如上）。

### 1.5 异步调用（`spawn`）的例外

- 同步调用 `call`(`0x0B`)：在本 `thread` 上压帧（本文 §1.2~1.4）。
- 异步 `spawn`（`FUN_0048EF50`，`0x48EF50`，被 case `0x0F`/`0x15` 调用）：`alloc 0x11e8` 新建一个**独立 SptInf/线程对象**（`new+0x1018 = 父 host`），并把它挂到父对象的脚本节点链表（`host+0x1200`）上；新线程**自带空栈**（其首帧 `帧[+0x0C] = -1`）。
- 因此：**读主线宿主 `host` 的 `thread`** 只会看到主线那条同步调用链——正是监控要的；异步子脚本不会混入该链。若日后要监控异步子脚本，需另取节点对象的 `thread`（其祖先链为空，只能按名字判）。

---

## 2. Q8 练习跳转补丁（`ECLJump`）对读取的影响

### 2.1 结论

✅ **补丁跳转后 `subIdx` 与名字仍正确。** 复核需求 §3 的初判成立。

### 2.2 依据

1. **补丁写入内容**：`thprac_th15.cpp:1018` 的
   ```cpp
   ecl << ecl_time << 0x0018000C << 0x02ff0000 << 0 << (dest-start) << at_frame;
   ```
   展开后 `opcode = 0x000C`（**`goto`**）、`size = 0x18`(24)、`dword4 = dest-start`、`dword5 = at_frame`。即只把一条 **`goto`** 覆盖到 ECL 指令流的硬编码偏移处（`start`/`dest` 均为离线解码得到的**指令边界**）。
2. **`goto` 语义**（`FUN_0048CA80` case `0xC`，`0x48CD36`；反编译 line 96047）：
   ```c
   *param_1 = (float)piVar12[5];                    // thread+0 = at_frame（时间，绝对赋值）
   param_1[2] = (float)((int)param_1[2] + piVar12[4]); // thread+8 += dest-start
   ```
   **只改 `thread+0`（时间）与 `thread+8`（ip offset），从不改 `thread+4`（sub 索引）。**
3. **sub 结构未动**：`ECLJump` 经 `ECLHelper::SetBaseAddr/SetFile` 定位到运行期 **ECL 文件缓冲**（`thprac_th15.cpp:2183`，`GetMemAddr(0x4e9a80,0x17c,0xC)`）并按文件偏移写字节；它不触碰 SCPT 头、`ECLH` 块头、函数偏移数组、名字 blob，也不重建/重排 sub 表（`res+0x8C`，由加载器 `FUN_0048F130` 一次解析生成，索引=名字排序位次）。故 `subIdx ↔ name` 映射稳定。
4. **落点设计**：`THStageWarp` 各 portion 的 `dest` 精确命中一条 `call MainSubNN`（离线反推，见 `th15_missbomb_phase_plan.md` §6）。
   - 例：ST1 `ECLJump(ecl, 0x7248, 0x7494, 60, 90)`，`0x7494` 正好是 `st01.ecl` 中 `main` 的 `call MainFront`（本文 §3.1 实测）。
   - 执行到该 `call` 后，`thread+4` 即变为目标段落入口 `MainSubNN`，名字正确。

### 2.3 唯一注意点（瞬态）

`goto` 与随后目标 `call` 之间（同一指令流、极短）`thread+4` 仍是**跳转前所在的 sub**（通常是 `main`）。也就是说：**跳转刚发生、`call` 尚未执行**的那一瞬，读到的名字是外层容器而非目标段落。`call` 执行后即正确。采样若发生在 IGI/帧间稳定态，不会命中该瞬态。

---

## 3. Q9 段落判定口径：**路过式 checkpoint（latch）**

### 3.1 结论

✅ **走「路过式 checkpoint（latch）」**：监控读主线当前 sub；**∈ 白名单 → 置「当前段落」为该 checkpoint**；**不在 → no-op**（保持上一个值，**不回退 `main`**）；本局首次命中前显示 `main`。CSV 即白名单表。

> ⚠️ **本处订正**：早期草案写「(A) 调用栈祖先法」，与段内脚本结构不符（原因见 3.2）。

### 3.2 结构证据（实测 `st01.ecl`）与订正原因

从 `main` 到段落入口的同步调用链（离线解码 `../th15decode/data/st01.ecl`，文件名偏移与 `ECLJump` 常量逐一吻合）：

```
main（tag60 起）
 ├─ call MainFront
 │    ├─ call MainSub00
 │    ├─ (spawn …)            ← 异步，另起线程
 │    ├─ call MainSub01
 │    ├─ call MainSub02
 │    └─ call MainSub03      → ret
 ├─ call MainLatter
 │    ├─ call MainSub04
 │    └─ call MainSub05      → ret
 ├─ call MainLatter2
 ├─ call MainBoss
 └─ …
```

- 每个 `MainSubNN` 由 `call`(opcode `0x0B`) 进入、执行本段内容、最后 `ret`(opcode `0x0A`) 返回。
- **但它们是 `MainFront`/`MainLatter` 的兄弟 `call`**：`MainSub00` `ret` 后才 `call MainSub01`……彼此**不互为祖先**。
  ⇒ 「段落入口 sub **整段**驻留栈上、可沿栈上溯」的 (A) 前提**对整个段落不成立**（只对**单个** `MainSubNN` 成立）。
- ⇒ **不需要**为段内每个 sub 建条目：只需登记**每段入口那一个 sub**；段内其余 sub 不在表内 → no-op。调用栈（§1.4 / Q3）仍是事实，但**不用于**本判定。

### 3.3 两个仍要保留的实现细节

1. **根哨兵 `-1`**（**仅供理解，不参与判定**）：`main` 的**首次** `call`（栈空，`SP==0`）压入的帧 `帧[+0x0C] = -1`（见 `FUN_0048C800` 的 `iVar3==0` 分支，反编译 line 95918-95923）。
2. **当前 sub 即判定源**：`thread+4` = 当前 sub；直接用它查白名单。

### 3.4 判定流程

```cpp
host   = mEclHost;                            // 0x43BBC8 捕获
thread = *(u32*)(host + 0x0C);
idx    = *(i32*)(thread + 4);                 // 当前 sub
name   = NameOf(host, idx);                   // res=*(host+0x11F8); table=*(res+0x8C)
section = Whitelist(game, stage, name);       // 命中 → 置当前段落
if (section) current = section;               // 未命中 → no-op（保持 current，不回退 main）
```

> checkpoint 粒度 = 每行 1 个入口 sub（= warp 落点所进入的 sub）；**含 async 落点**（async 子同样登记）。容器 `main/MainFront/MainLatter` 不入表。
> 备注：`thprac_th15_missbomb.cpp::TH15_MB_MID_MAP` 曾按「当前 sub 名 → 章节名」列举多个 `MainSubNN`（那里是**名字查表**用途）；本 CSV 口径下**每段只登记入口那一个 sub 即可**，无需照抄该表的多 sub 形态。

---

## 4. 对总括 §5 / 监控接线的回填建议

1. **th15 行**（总括 §5.1）补：**Q3 = 有显式调用栈**（`SP=[thread+0x100C]`，帧 `0x10`，`call 0x0B` 压 / `ret 0x0A` 弹，帧 `+0x0C`=祖先 sub，仅作事实）；**Q9 = 路过式 checkpoint（latch）**，th15 定案。
2. **读取面**：在既有 `TH15MissBombRecorder::GetEclSubName` 的 `thread` 上直接取当前 sub（`thread+4`）查白名单即可，无需栈遍历；宿主指针清理沿用 `th15_game_start(0x43E6EE)`。
3. **同代搬运**：th16/17/18 的 `GetMemAddr(...,0x17c,0xC)` 形态与 th15 同源，Q3 的栈偏移/帧长可优先按本作模板验证后搬运。

---

## 5. 真机验证清单（补充 requests §5）

- [ ] 道中/boss 采样：当前 sub 名与实际一致（沿用既有可信链路）。
- [ ] **路过置值**：经过各段入口 sub 时「当前段落」被正确置值；段内其余 sub 不改变当前段落（no-op）。
- [ ] **练习跳章**后：`thread+4` 名字与白名单置值均正确（Q8 复核）。
- [ ] **异步落点**：async 落点（如 ST1 前半 #2 = `MainSub01b`）能否被观测到并置值。
- [ ] 采样时机放在帧间稳定态（IGI 渲染），避免表达式临时栈占用 `SP` 时误读。

---

## 5.1 boss 段监控（方案二）交叉引用

- boss 段（读 **boss 宿主** ECL 当前 sub 名 + 白名单 latch，含「boss 宿主取法 / 口径冲突判定」）已**单独立档** → **`docs/th15/th15_boss_phase_report.md`**。
- 本文 §1（`host→thread→subIdx(res)→sub 表` 链）、§3（latch 口径）与 boss 段**同机制**；boss 侧仅**多一步「先取 boss 宿主」**（`mgr+0x180` 链上对象 `E` **即 host**，`docs/th15/th15_boss_phase_report.md` §2）。
- 已同步回填 `docs/logger/th15_checkpoints.csv` 的 **10 行** boss 行（ST1/ST2 BOSS；其余暂缺，见该报告 §5）。

---

## 6. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档。补 **Q3**（显式调用栈：`thread+0x100C`=SP、`thread+0xC` 栈底、帧 `0x10`、帧 `+0x0C`=祖先 sub；`call 0x0B` 压 / `ret 0x0A` 弹；深度上限 ≈256，护栏 `0x1000`；异步另起线程）；**Q8 确认**（`ECLJump` 只写 `goto`，不改 `thread+4`/sub 表，跳转后名字正确）；**Q9 定案为 (A) 调用栈祖先法**〔**后于 2026-10-15 订正为「路过式 checkpoint（latch）」**〕（`st01.ecl` 实测 `main→MainFront→MainSubNN`，入口 sub 驻留栈上；根哨兵 `-1` 兜底 `main`）。证据：`capstone` 反汇编 + 本机 `th15.exe_decompiled.c`（Ghidra）+ 离线 `.ecl` 解码。未真机三态。 |
| 2026-10-15 | 素材来源登记：`../exes/ecl/th15/`（`.ecl` 23 份）+ `../exes/ecl_dump/th15/`（`.decl` 23 份）已在机；`st01.ecl` 与 `../th15decode/data/st01.ecl` 为同一脚本两处副本。清单见 `docs/logger/decompile_materials.md`。 |
| 2026-10-15 | **订正 Q9 口径**：由「(A) 调用栈祖先法」改为「**路过式 checkpoint（latch）**」——经过白名单函数即置值、不在表内 no-op（不回退 `main`）。原因：段内 `MainSubNN` 是 `MainFront`/`MainLatter` 的**兄弟 `call`**，不互为祖先，「整段驻留栈上」不成立 → 每 checkpoint 只需登记 1 个入口 sub（含 async）。§0 Q9 行、§3、§5 同步改写。 |
| 2026-10-16 | **追加 boss 段（方案二）交叉引用**：新增 §5.1 指向 `docs/th15/th15_boss_phase_report.md`（boss 段读 boss 宿主当前 sub 名 + latch；含 boss 宿主取法、与 `../th15decode/boss-phase-hook.md` 的口径冲突判定、10 行 boss 行回填）。本文 Q1–Q9 结论不变。 |

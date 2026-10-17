# checkpoint CSV「段落入口函数」回填工作流（ECLJump 落点 → ECL sub）

> 上位文档：`docs/logger/checkpoint_csv_workflow.md`（CSV 骨架生成）、`docs/logger/logger_re_disasm_overview.md`（总括，§6 Q9）、`docs/logger/decompile_materials.md`（`.ecl`/`.decl` 素材来源）。
> 用途：为某作的 checkpoint CSV 回填第 1 列 `段落入口函数`——**由 thprac 的 warp 跳转偏移（`ECLJump`）+ 离线 `.ecl`/`.decl` 直接推出**，不靠猜。
> 本文只保留**已验证的正确流程**；早期“调用栈祖先 / 一段多 sub”的写法已废弃（见 §7）。
> 参考成品：`docs/logger/th16_checkpoints.csv`（章节行 39 行已回填）。

---

## 0. 一句话

**对每个 checkpoint，取它在 thprac 里那条 warp 跳转的落点，落点上是一条 `call MainSubNN`（或 `async call`），被调者就是该 checkpoint 的「段落入口函数」。** 每行只填 1 个。

---

## 1. 判定口径（必须先明确）

CSV 是**路过式 checkpoint（latch）白名单表**：

1. 运行期读「当前 ECL sub 名」。
2. **当前 sub ∈ 表** → 把「当前段落」置为该行 `记录名`（覆盖上一个值）。
3. **不在表** → **no-op**（保持上一个值，**不回退 `main`**）。
4. 本局首次命中任何表内函数之前，「当前段落」显示/记录为 `main`。

**推论**：每个 checkpoint **只需登记 1 个入口函数**——即“进入此函数即代表此段开始”的那一个。
**不需要**为段内每个 sub 都建条目；容器 `main`/`MainFront`/`MainLatter` 不入表。

> ⚠️ 依据：段内多个 `MainSubNN` 是 `MainFront`/`MainLatter` 的**兄弟 `call`**（前一个 `ret` 后才调下一个），彼此**不互为祖先**。「入口 sub 整段驻留栈上、可沿调用栈上溯」的旧口径**不成立**（详见 `logger_re_disasm_overview.md` §6 Q9）。

---

## 2. 输入 / 输出

**输入**
- `thprac/src/thprac/thprac_thXX.cpp`：`THStageWarp`（章节，`ECLJump` 偏移）、`THPatch`（boss，`ECLJump` 偏移）。
- `../exes/ecl/thXX/*.ecl`（原始脚本，**运行时偏移即文件内字节偏移**）。
- `../exes/ecl_dump/thXX/*.decl`（`thecl -d` 反编译，用于对名字/结构；`sym` 与 `.ecl` sub 表一致）。
- 骨架 CSV：`docs/logger/thXX_checkpoints.csv`。

**输出**
- 同 CSV，第 1 列填好（本章节行）。boss 行流程见 §4 规则 4。

---

## 3. 关键机制（离线解码口径）

### 3.1 ECL 文件与“槽”

- `ECLHelper` 在 `THSectionPatch` 里 `SetBaseAddr(GetMemAddr(...))`，默认定位 **slot 0**；`SetFile(N)` 切换到第 N 槽。
- 槽位映射：**slot 0 = `stNN.ecl`（关卡主文件）**；`slot K (K≥1) = ecli[K−1]`，其中 `ecli{}` 见该关 `.decl` 头。
  - 例：th16 ST1 `ecli = {default, st01bs, st01mbs}` → slot1=default、slot2=st01bs、slot3=st01mbs。
  - 例：th16 ST5 `ecli = {default, st05mbs, st05bs}` → slot2=st05mbs、slot3=st05bs（**顺序逐关不同，务必按 `ecli{}` 读**）。
- thprac 的 `ECLJump(start, dest, at_frame, ecl_time)`：在 `start` 处写一条 `goto(0x0C)`，**`dest` 才是落点**；`start`/`dest` 都是**当前槽文件内的字节偏移**。
  > ⚠️ **函数名逐作不同**（2026-10-17 补）：带 `dest` 的那个在 **th12** 叫 **`ECLJumpEx(ecl, start, dest, at_frame, ecl_time)`**；th12 里名为 `ECLJump(ecl, start, ecl_time, length)` 的**没有 `dest`**（原地向前跳过 `length`，是段内补丁、不产生入口）。**认函数看签名，不要看名字**。

### 3.2 `.ecl` 结构（解析用）

```
0x00  "SCPT"                 magic
0x06  u16 header_len         头部附加区长度
0x10  u32 sub_count          sub 数
0x24  …                      头部附加区（ANIM/ECLI 名字串等），长 header_len
0x24+header_len               sub 起始偏移数组：sub_count × u32
其后                          名字表：sub_count 个 NUL 结尾字符串（顺序与偏移数组对齐）
其后                          各 sub 代码，每个以 "ECLH" 开头
```

- **sub 代码头**：`"ECLH"` + `0x10` 字节，首条指令在 `substart + 0x10`。
- **指令布局**：`time(i32)@+0 · id(u16)@+4 · size(u16)@+6 · mask(u32)@+8 · params@+0xC`；下一条指令 = 当前 `+ size`。
- **`call` / `async call`**：`id = 0x0B`（同步 call）/ `0x0F`（async call）；名字参数在指令内 `+0x14` 起的 NUL 结尾字符串。

### 3.3 章节行取值规则（`THStageWarp`，warp 模式 1）

一段（portion）通常有**两条** `ECLJump`：

- **第一条**落点在 `main` 内，通常是一条 `call MainFront` 或 `call MainLatter`（容器）——只起“把 `main` 拨到该容器”的作用。
- **第二条**（若有）落点在 `MainFront`/`MainLatter` 内，是一条 `call`/`async call MainSubNN`——**被调者即该段入口**。

规则：

1. 取该段**「落点会等待」的那一条** `ECLJump` 的 `dest` → 解析成指令（**2026-10-17 修订**；原写「最后一条」，会误取段内时间跳跃）：
   - 是 `call`/`async call` → 填**被调者名字**；
   - **不是 `call`** → 填**包含 `dest` 的那个 sub**；若该 sub 是容器 `main` ⇒ 见规则 5；
   - 否则按 §3.4 处理。
   > **判据（2026-10-17 新增）**：设 `t_land` = 落点指令的 `time`、`at_frame` = `ECLJump` 的第 4 参。
   > `t_land > at_frame` ⇒ 解释器在落点**等待** ⇒ 该 jump 是**段落入口**；`t_land ≤ at_frame` ⇒ 落点被**快进跑穿**、帧边界观测不到 ⇒ 那是**段内时间跳跃**，不是入口。
2. **只跟随「`start` 落在当前链那个 sub 内」的 jump**（**2026-10-17 新增**）：同段常有多条 `ECLJump`，其中有些打在**别的 sub / 别的宿主**的派生器上（辅助补丁），照字面取会得到重复或错误的入口。
3. 该段**没有**上述 `ECLJump`（含 portion 1）→ 落在第一跳指向的容器（`MainFront`/`MainLatter`）里，填**该容器里第一个 `call MainSubNN` 的被调者**。
4. **async 落点照挂**：`async call`（`0x0F`）的被调者**照样填**，不因为它跑在异步线程就跳过。
5. **入口落在容器 `main` 内部、拿不到唯一 sub 名时**（**2026-10-17 新增**）：改用 **`sub名+0x偏移`** 二级粒度登记 —— 偏移 = **相对该 sub 首条指令**的文件偏移（小写十六进制，无空格），例 `main+0x28`、`MainSub08+0x390`。运行期按 `(sub 名, 偏移)` 匹配，偏移命中优先。详见 `docs/th10/th10_checkpoint_ip_offset.md`。

> 记录名 ↔ portion 的换算由 `mChapterSetup[7][2] {前半,后半}` 决定：`后半==0` → `STn #k`；否则 `k≤前半 → STn 前半 #k`，`k>前半 → STn 后半 #(k−前半)`。

### 3.4 boss 行取值规则（`THPatch`，warp 模式 2/3）——**已落地（2026-10-17）**

- `THPatch` 的主跳转（slot 0）落在 `main` 的 `call MainBoss` / `enmCreateA "Boss"/"MBoss"` 上；**这不能唯一区分第几个 boss 段**。
- 入口登记 **boss 脚本的战斗 sub**（`Boss1`/`BossCard1`/`MBoss`/`MBossCard1`…）；**顶层容器 `Boss()`/`MBoss()` 不作为入口**。
- **逐行定法（纯离线，th10–th18 通用）**：把 `THPatch` 对该 case 的补丁应用在 `.ecl` 字节副本上，看它落到 boss 脚本的哪一条指令：
  1. **改名字串**：`Boss()`/`MBoss()` 内那条 `call Boss1`/`call MBoss1` 的**名字串**被 `pair{off,'K'}` 原地改成 `BossK`/`BossCardK` ⇒ 入口 = 改后的被调者；
  2. **改 `lifeSet` 操作数 / `setNext` 的 hp 阈值** ⇒ 该相位**立刻**转移到 `setNext` 的目标 sub ⇒ 入口 = 那个目标（通常是符卡）；
  3. 两者都没有 ⇒ `$SPELL_ID = -1` 默认分派 ⇒ `Boss1` / `MBoss1`。
- **交叉验证**：boss 脚本自身的 `setNext` 相位链（`Boss1 → BossCard1 → Boss2 → …`）+ `.decl` 的 `spell()` 名 + thprac 自己的 `th_sections_cbt`（非符/符卡分栏）三者对齐。
- **不需要** boss ECL 宿主 / 槽表 / 门控位 —— 那些只在**运行期读当前 sub**（实现监控）时才需要，**不阻塞白名单枚举**（2026-10-17 订正）。
- **B 代例外（th12 型）**：无独立 boss 脚本文件、无 `$SPELL_ID`，相位全靠上述 ①②；`SetFile(2..7)` 的分支脚本（如 th11 st4 六机体）**每支各自登记入口**（见 §5「分支关」）。

---

## 4. 步骤（可复现）

1. **列 warp 表**：读 `thprac_thXX.cpp` 的 `THStageWarp`，逐关逐 portion 抄出所有 `ECLJump(start, dest, …)`；同时记下是否 `SetFile(N)`（章节 warp 一般不动文件槽）。
2. **解析主文件**：对每关 `stNN.ecl` 用 §3.2 解析出 `names[]`/`offs[]`，得到「偏移 → sub 名」区间表。
3. **抽 call 序列**：解析 `main` / `MainFront` / `MainLatter`（及个别作的 `MainLatterB`）内所有 `call`/`async call`，得到按代码顺序的被调者序列（**同时标注 async**）。
4. **定每段入口**：按 §3.3 规则 1/2，对每个 portion 求出唯一的入口 sub。
5. **换算记录名**：按 §3.3 的 `mChapterSetup` 公式，把 portion 映射到 CSV 的 `记录名`。
6. **回填 CSV**：只改第 1 列；boss 行保持空（本轮）。
7. **自检**：见 §6。

### 参考实现（Python，只读；th16 用的就是这套）

```python
import struct

def parse_ecl(path):
    """→ (data, names[], offsets[])；偏移为文件内字节偏移。"""
    d = open(path, 'rb').read()
    assert d[:4] == b'SCPT'
    hlen  = struct.unpack_from('<H', d, 6)[0]
    count = struct.unpack_from('<I', d, 0x10)[0]
    off0  = 0x24 + hlen
    offs  = list(struct.unpack_from('<%dI' % count, d, off0))
    p = off0 + count * 4
    names = []
    for _ in range(count):
        e = d.index(b'\x00', p); names.append(d[p:e].decode('latin1')); p = e + 1
    return d, names, offs

def _sub_range(d, names, offs, name):
    i = names.index(name); s = offs[i]
    nxt = [o for o in offs if o > s]
    return s, (min(nxt) if nxt else len(d))

def call_at(d, off):
    """若 off 处是 call/async call，返回 (被调名, 是否async)，否则 (None, None)。"""
    _, idv, _, _ = struct.unpack_from('<IHHI', d, off)
    if idv in (0x0B, 0x0F):
        z = d.index(b'\x00', off + 0x14)
        return d[off + 0x14:z].decode('latin1'), (idv == 0x0F)
    return None, None

def calls_in_sub(d, names, offs, name):
    """按代码顺序返回该 sub 内的 call 序列 [(被调名, is_async), …]。"""
    s, e = _sub_range(d, names, offs, name); out = []; o = s + 0x10
    while o < e:
        _, idv, size, _ = struct.unpack_from('<IHHI', d, o)
        if idv in (0x0B, 0x0F):
            z = d.index(b'\x00', o + 0x14)
            out.append((d[o + 0x14:z].decode('latin1'), idv == 0x0F))
        o += size
    return out

# 章节行：对每个 portion，SECOND = 第二条 ECLJump 的 dest（无则取容器首个子）
#   entry, _ = call_at(d, SECOND)                    # 有第二条
#   entry = calls_in_sub(d, names, offs, 'MainFront')[0][0]   # 无第二条、落 MainFront
#   entry = calls_in_sub(d, names, offs, 'MainLatter')[0][0]  # 无第二条、落 MainLatter
```

---

## 5. 边界情况（必读）

- **async 落点**：照挂（例：th16 ST1 前半#2=`MainSub01b`、ST1 后半#2=`MainSub05`、ST2 #1=`MainSub00b`、ST2 #4=`MainSub05`）。
- **无第二条 `ECLJump` 的段**：含每个 `p1`，以及“第一条直接进 `MainLatter`”的段（th16 的 ST1 p3、ST3 p5、ST4 p4、ST5 p6、ST7 p5）→ 取容器首个子，**不要**填容器名。
- **同名 sub 二次出现**：个别作 `MainLatter` 会再次 `call MainSubNN`（如 th16 ST2）；本流程只取“入口那一个”，不涉及“区间划界”，故无歧义；若日后要枚举区间，请**按 call 目标索引**而非名字来划界。
- **`dest` 落在非 `call` 指令**：**2026-10-17 起处理** —— 章节行取**包含 dest 的 sub**（§3.3 规则 1）；boss 行按 §3.4（如落点是 `enmCreateA "Boss"`，则跟到 boss 脚本的战斗 sub）。
- **`goto` 有多个 opcode**（2026-10-17 新增，th13 全 21 份 `.ecl` 实测）：**`0x0C` = 无条件 goto**（817 条，全前跳）、**`0x0D` = `unless (cond) goto`**（349 条，全前跳）、**`0x0E` = `if (cond) goto`**（608 条，全后跳），三者布局一致（`+0x10` = 相对目标）。**thprac 注入的恒为 `0x0C`** —— 离线解析只认 `0x0C` 会**失步**（须至少认 `0x0C`/`0x0D`/`0x0E`）。
- **文件槽顺序**：`SetFile(N)` 的含义**逐关不同**，一律以该关 `.decl` 的 `ecli{}` 为准。
- **分支关**（th11 st4 / th14 st4 / th16 st5）：本口径读的是**运行期状态**，分支天然兼容；只需白名单**同时收录两条分支各自的入口**。

---

## 6. 自检清单

- [ ] 章节行数 == Σ(各关 `mChapterSetup` 的 前半+后半)；与 `checkpoint_csv_workflow.md` §3 一致。
- [ ] 每行恰 1 个入口：**纯 sub 名**，或 **`sub名+0x偏移`** 二级粒度（2026-10-17 起允许，见 §3.3 规则 5）；无空、无多值、无容器名（`main`/`MainFront`/`MainLatter`）单独出现。
- [ ] 每个入口 ∈ 该关主 `.ecl` 的 sub 名集合。
- [ ] 入口与该段 `THStageWarp` 偏移**一一对应**（可写脚本重跑比对）。
- [ ] async 落点未被跳过（对照 §5 名单）。
- [ ] CSV 仅第 1 列被改动，`所属游戏/关卡/记录名` 不变。

---

## 7. 已废弃的写法（勿再采用）

- ❌ **「(A) 调用栈祖先法」**：假设入口 sub 在整段期间驻留调用栈、命中失败就沿栈上溯。→ 前提不成立（段内 sub 是兄弟 `call`）。
- ❌ **「一个 checkpoint 列多个 sub / 一段覆盖所有制」**：与 latch 口径不符，CSV 会无谓膨胀。
- ✅ 正确口径：**路过式 checkpoint（latch）**，每行 1 个入口函数。
- （注：各作“存在可遍历调用栈”仍是**事实**，但不参与判定；见 `logger_re_disasm_overview.md` §6 Q3/Q9。）

---

## 8. 示例：th16 章节行（已回填）

| 记录名 | 入口 | 记录名 | 入口 |
| --- | --- | --- | --- |
| ST1 前半 #1 | `MainSub00` | ST5 前半 #1 | `MainSub00` |
| ST1 前半 #2 | `MainSub01b` *(async)* | ST5 前半 #2 | `MainSub01` |
| ST1 后半 #1 | `MainSub03` | ST5 前半 #3 | `MainSub02` |
| ST1 后半 #2 | `MainSub05` *(async)* | ST5 前半 #4 | `MainSub03` |
| ST2 #1 | `MainSub00b` *(async)* | ST5 前半 #5 | `MainSub04` |
| ST2 #2 | `MainSub02` | ST5 后半 #1 | `MainSub05` |
| ST2 #3 | `MainSub03` | ST5 后半 #2 | `MainSub06` |
| ST2 #4 | `MainSub05` *(async)* | ST5 后半 #3 | `MainSub07` |
| ST3 前半 #1 | `MainSub00` | ST6 #1 | `MainSub00` |
| ST3 前半 #2 | `MainSub01` | ST6 #2 | `MainSub01` |
| ST3 前半 #3 | `MainSub02` | ST6 #3 | `MainSub02` |
| ST3 前半 #4 | `MainSub04` | ST7 前半 #1 | `MainSub00` |
| ST3 后半 #1 | `MainSub05` | ST7 前半 #2 | `MainSub01` |
| ST3 后半 #2 | `MainSub06` | ST7 前半 #3 | `MainSub02` |
| ST4 前半 #1 | `MainSub00` | ST7 前半 #4 | `MainSub03` |
| ST4 前半 #2 | `MainSub01` | ST7 后半 #1 | `MainSub04` |
| ST4 前半 #3 | `MainSub02` | ST7 后半 #2 | `MainSub05` |
| ST4 后半 #1 | `MainSub03` | ST7 后半 #3 | `MainSub06` |
| ST4 后半 #2 | `MainSub04` | ST7 后半 #4 | `MainSub07` |
| ST4 后半 #3 | `MainSub05` | | |

---

## 9. 与其它文档的关系

- CSV **骨架**（哪些行、记录名怎么来）→ `docs/logger/checkpoint_csv_workflow.md`（本文是其 §4.5 的展开）。
- **口径 / Q9 / Q3** → `docs/logger/logger_re_disasm_overview.md` §6。
- **素材来源**（`.ecl`/`.decl` 从哪来）→ `docs/logger/decompile_materials.md`。
- **逐作结论与入口表** → `docs/thXX/thXX_ecl_re_report.md`（th16 见其 §10.4）。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-15 | 建档。整合本轮（th16）**正确**回填流程：口径＝路过式 checkpoint（latch）；取值＝warp 落点所进入的 `call/async call MainSubNN`（async 照挂）；给出 `.ecl` 解析、步骤、参考脚本、自检与边界；§7 登记已废弃的「调用栈祖先 / 一段多 sub」写法。示例＝th16 章节行 39 行（已回填 `docs/logger/th16_checkpoints.csv`）。boss 行流程暂缓（§3.4）。 |
| 2026-10-17 | **按 th10–th13 全表回填的实际口径收口**：**§3.3** 规则 1 由「取最后一条 `ECLJump`」改为「取**落点会等待**（`t_land > at_frame`）的那一条」，并新增「`dest` 非 `call` ⇒ 取包含 dest 的 sub」「只跟随 `start` 落在当前链 sub 内的 jump」「入口落在 `main` 内部时用 `sub名+0x偏移` 二级粒度」三条（§3.3 规则 1/2/5）；**§3.4** boss 行由「暂缓」改为**已落地**，写明纯离线定法（改名字串 / 改 `lifeSet` 逼 `setNext` / 默认分派）与「不需要 boss 宿主锚点」；**§5** 补 th13 `goto` opcode 为 `0x0D`（thprac 注入才是 `0x0C`）的注记；**§6** 自检放宽为「纯 sub 名 或 `sub名+0x偏移`」。 |

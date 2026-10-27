# TH17 Boss 段监控 — 方案二（读 boss 宿主 ECL 当前 sub 名 + 白名单 latch）逆向报告

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志）
> 总括（术语口径以此为准）：`docs/logger/logger_re_disasm_overview.md`
> 模板 / 上位：`docs/th16/th16_boss_phase_report.md`（TH16 boss 段两方案调研；**本报告采用其「方案二」**）
> 同作：`docs/th17/th17_ecl_re_report.md`（TH17 ECL 段落监控 Q1–Q9，含 §4 宿主对象模型 / 捕获；本报告是其「boss 段」的续篇）
> 素材清单与来源：`docs/logger/decompile_materials.md`
> 二进制指纹：`../exes/th17.exe`，**747520 B**，PE `TimeDateStamp = 1565429804`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；绝对 VA 以立即数内嵌，可直接 4 字节小端字节搜索定位 xref）
> 引擎代：**C 代（第四代引擎）**；同代参照作 th15 / th16。
> 方法：`pefile 2024.8.26` + `capstone 5.0.7` 只读反汇编（x86-32）；与 th15 的 Ghidra 全量反编译 C（`../th15decode/th15.exe_decompiled.c`）逐函数对照；离线素材 `../exes/ecl/th17/*.ecl`（22 份）与 `../exes/ecl_dump/th17/*.decl`（22 份）。
> 状态：**静态调研（🟡 未真机三态）**。本机 macOS 不可编译、不可真机；未真机验证的地址/结论文中均标 🟡。
> 地址口径：正文以 **VA** 为主，并同时标注 **RVA = VA − 0x400000**。只读：本次未改任何 C++ 源码、未 git commit。

---

## 0. 结论速览

| 维度 | 结论 | 置信 |
| --- | --- | --- |
| **boss ECL 跑在哪** | boss ECL（`Boss`/`BossCardN`/`MBoss`）跑在 **boss 敌人自己的宿主**里；照搬道中「读主线宿主」做 boss 会恒定读到 `main`（与 th16 §3.2 同）。**必须先取 boss 宿主**。 | 🟡 静态 |
| **取 boss 宿主（本作版）** | ✅ 成立。管理器全局 `0x4b76a0`（`ENEMY_MANAGER_PTR`）→ `mgr = *(u32*)0x4b76a0`；**槽→id 表 = `mgr+0x48`**；**宿主链表头 = `mgr+0x180`**（节点在 `host+0x14F4`，`node[0]=host`、`node[+4]=next`）；**敌机 id = `host+0x5760`**。 | 🟡 静态 |
| **游戏自身「取当前 boss」函数** | ✅ `FUN_0041A5C0`（`0x41A5C0`）＝ th15 `FUN_0041F7F0` / th16 `FUN_00417580`（按槽位取 host）的同源实现：读 `mgr+0x48+slot*4` 作 id，再遍历 `mgr+0x180` 链比对 `[E+0x5760]`。**被全程序大量调用（选 boss 槽 0/1/2）**。 | 🟡 静态 |
| **同族辅助** | 按 id 取对象 `FUN_0041DDD0`（`0x41DDD0`）/`FUN_0041E9E0`；按 id 判存在 `FUN_0041DD90`；写槽 `FUN_0041DD20`（`mgr+0x48+slot*4 = host+0x5760`）。 | 🟡 静态 |
| **boss 门控位** | `[host+0x528C] & 0x800000`（bit23）；置位时（如宿主析构）清 `mgr` 槽表项；B 侧口径 `[B+0x4080]`（`B = host+0x120C`）同一位。 | 🟡 静态 |
| **boss 槽位字段** | `host+0x529C`（构造时置 `-1`）—— 候选（仅见「初值 + 单处读」，见 §6）。 | 🟡 静态 |
| **boss 对象 `B`** | `B = host+0x120C`（getter `0x41DAB0`：`lea eax,[ecx+0x120c]; ret`）；**`*(B+0x44D8) == host`**（th16 的 `B+0x44B8` 在 th17 为 `B+0x44D8`，因宿主/子对象整体长大 0x20）。 | 🟡 静态 |
| **当前 sub 名读取链** | ✅ 与道中完全同一链：`host=…`；`thread=*(u32*)(host+0xC)`；`subIdx=*(i32*)(thread+4)`；`res=*(u32*)(host+0x11F8)`；`count=*(i32*)(res+8)`；`table=*(u32*)(res+0x8C)`；`name=*(char**)(table+subIdx*8)`。 | 🟡 静态 |
| **boss 脚本/阶段名** | `stNNmbs`＝道中 boss（`MBoss`…）；`stNNbs`＝boss（`Boss`→`Boss1`/`BossCardN`…）。阶段推进由 `setNext(0x202)` 注册「下一攻击」的名字。 | ✅ 离线 `.decl` |
| **boss 行回填** | ST1–ST5 **全部回填**（含 MID1）；ST6 填 MID1+BOSS1–8；ST7 填 MID1+MID2+MID3+END_NS1–8+END_S1–S10。ST6 BOSS9–11 因 `keepSpellDrops` 双分支留空（🟡）。 | 🟡 见 §5 |

> **一句话**：TH17 的 boss 宿主取法与 th15/th16 **同族**（`mgr+0x48` 槽→id、`mgr+0x180` 宿主链），但因本作**宿主对象长大 0x20 字节**，若干字段偏移需 +0x20（id `0x5760`、门控 `0x528C`、`B` 自指针 `0x56E4`）；取到宿主后 ECL sub 名读取链与 th17 道中**逐字段相同**。

---

## 1. 方案二机制（与道中同一机制）

```cpp
// 1) 取 boss 宿主（本作版；见 §2）
mgr  = *(u32*)0x4b76a0;                       // ENEMY_MANAGER_PTR
id   = *(u32*)(mgr + 0x48 + slot*4);          // slot=0 → 主 boss；0 = 无 boss
node = *(u32*)(mgr + 0x180);                  // 宿主链表头
host = 0;
while (node) {                                // node[0]=host, node[+4]=next
    u32 E = *(u32*)node;
    if (*(u32*)(E + 0x5760) == id) { host = E; break; }
    node = *(u32*)(node + 4);
}
// 或直接调用游戏自身：host = FUN_0041A5C0(slot);

// 2) 读当前 sub 名（与道中同链；见 §3）
thread = *(u32*)(host + 0x0C);
subIdx = *(i32*)(thread + 4);
res    = *(u32*)(host + 0x11F8);              // = *(u32*)(mgr + 0x17C)
count  = *(i32*)(res + 8);
table  = *(u32*)(res + 0x8C);
name   = *(char**)(table + subIdx*8);         // sub 名（NUL 结尾 char*）

// 3) 路过式 checkpoint（latch）：name ∈ 白名单 → 置当前段落；不在 → no-op
```

- 与 th16 §3.1 方案二**同构**；差别仅在「取 boss 宿主」的偏移随宿主尺寸整体 +0x20。
- 白名单（`docs/logger/th17_checkpoints.csv`）每行 1 个「段落入口 sub」（= warp 落点进入的 sub）；容器 `main` 不入表。

---

## 2. Q-boss-1 —— boss 宿主取法（本作版）

### 2.1 关键订正：`mgr+0x48` 才是「槽→id 表」，不是 `host+0x48`

- th17 ECL 报告 §4.1 把 `+0x48` 记在 host 字段表里（写「boss 存在标志」）。**实测：该字段属于管理器 `mgr`，非 host** —— thprac 现有代码即 `GetMemContent(ENEMY_MANAGER_PTR, 0x48)`（`thprac_th17.cpp:784`）读 `mgr+0x48` 判 boss 存在；th16 亦 `GetMemContent(ENEMY_MANAGER_PTR, 0xc+0x3c)`（同为 `mgr+0x48`）。
- 结论：**`mgr+0x48` = 槽→id 表（`mgr+0x48+slot*4` = 该槽 boss 的敌机 id，0 = 无）**，与 th15/th16 一致；§4.1 的 host 字段行应删除/更正。

### 2.2 游戏自身的「取当前 boss」函数 `FUN_0041A5C0`

VA `0x41A5C0` / RVA `0x1A5C0`（`__cdecl`/`ret 4`，参数 = 槽号）：

```asm
0041a5c0  55                push ebp
0041a5c1  8bec              mov  ebp, esp
0041a5c3  8b4508            mov  eax, [ebp+8]          ; eax = slot
0041a5c9  8b35a0764b00      mov  esi, [0x4b76a0]       ; esi = mgr
0041a5cf  8b548648          mov  edx, [esi+eax*4+0x48] ; ★ id = mgr[0x48 + slot*4]
0041a5d3  85d2              test edx, edx
0041a5d5  7507              jne  0x41a5de
0041a5d7  33c0              xor  eax, eax               ; id==0 → 返回 0
0041a5d9  5e                pop  esi
0041a5da  5d                pop  ebp
0041a5db  c20400            ret  4
0041a5de  8b8680010000      mov  eax, [esi+0x180]       ; ★ 链表头 = mgr+0x180
0041a5e4  85c0              test eax, eax
0041a5e6  7419              je   0x41a601
0041a5f0  8b08              mov  ecx, [eax]             ; E = node[0]
0041a5f2  399160570000      cmp  [ecx+0x5760], edx      ; ★ host id = [E+0x5760]
0041a5f8  7407              je   0x41a601
0041a5fa  8b4004            mov  eax, [eax+4]           ; node = node[+4]
0041a5fd  85c0              test eax, eax
0041a5ff  75ef              jne  0x41a5f0
0041a601  8bc1              mov  eax, ecx                ; 返回 E（= host）
0041a603  5e                pop  esi
0041a604  5d                pop  ebp
0041a605  c20400            ret  4
```

- 与 th15 `FUN_0041F7F0`（Ghidra `line 20795`：`iVar1 = *(int *)(DAT_004e9a80 + 0x48 + param_1 * 4); … for (piVar2 = *(int **)(DAT_004e9a80 + 0x180); … *(int *)(iVar3 + 0x5740) != iVar1; …)`）**逐句同构**，仅 id 偏移 `0x5740 → 0x5760`。
- 调用点密集（`0x423532`/`0x4250E5`/`0x4250F6`/`0x425107`/`0x427685`…共 41 处），其中 `0x4250DD` 一次尝试槽 0→1→2（正是「取当前 boss」的语义）。

### 2.3 宿主链表与 id（写侧证据）

- 宿主链节点**内嵌在 host** 里：`host+0x14F4`（`node[0] = host`、`node[+4] = next`、`node[+8] = prev`）。初始化写点 `0x41EBBF`（`mov [edi+0x14f4], edi`，`edi` = host，属宿主构造 `FUN_0041EA20`）。
- **挂链 / 计数**（在 `FUN_0041E140` = `CreateScriptHost` 内，宿主构造后）：
  ```asm
  0041e31b  cmp  dword ptr [edi + 0x180], 0    ; edi = mgr
  0041e322  lea  ecx, [esi + 0x14f4]           ; ecx = &node（esi = host）
  0041e32a  mov  dword ptr [edi + 0x180], ecx  ; ★ 空链 → mgr+0x180 = node
  …（非空 → 追加到 mgr+0x184 尾，再写 [edi+0x184] = node）
  0041e35f  inc  dword ptr [edi + 0x18c]       ; mgr+0x18C = 宿主计数
  ```
  ⇒ **`mgr+0x180` = 宿主链头，`mgr+0x184` = 尾**（另有 `mgr+0x188` 第三头，见卸载点 `0x41DB18`）。
- 宿主链**卸载**（析构，vtable 写 `[esi] = 0x4A1680`）：`0x41DB04`/`0x41DB12` 比较并前移 `mgr+0x180` 头、`+0x184`/`+0x188` 同处理。
- **敌机 id 写点**：`0x41ED2C`（`mov [edi+0x5760], eax`，`eax = [ecx+0x90]`，`edi` = host）。
- 宿主工厂 `FUN_0041E140`：`malloc 0x576c`（`0x41E146 push 0x576c`）→ `call FUN_0041EA20`（构造）→ `ret 0xc`（`0x41E36A`）。

### 2.4 boss 门控位 / boss 槽位

- **门控位**：`[host+0x528C] & 0x800000`（bit23）。读取/判据点：
  ```asm
  0041db98  f7868c52000000008000  test dword ptr [esi+0x528c], 0x800000  ; esi = host
  0041dba2  740e                  je   0x41dbb2
  0041dba4  8b869c520000          mov  eax, [esi+0x529c]                ; 该 boss 占用槽
  0041dbaa  c744814800000000      mov  dword ptr [ecx+eax*4+0x48], 0    ; ecx = mgr，清槽表项
  ```
  B 侧同一位：`0x423DA8`（`mov eax,[edi+0x4080]; test eax,0x800000; or eax,0x40000000; mov [edi+0x4080],eax`，`edi = B`，`B+0x4080 = host+0x528C`）。
- **boss 槽位字段**：`host+0x529C`（构造初值 `-1`，写点 `0x41EBB5`；读点 `0x41DBA6`）。⚠️ 全 `.text` 仅见「初值 + 单处读」，未见明确的「赋真实槽号」写点（🟡，见 §6）。

### 2.5 boss 对象 `B`（供方案一 / 交叉校验）

- `B = host + 0x120C`：getter `FUN`@`0x41DAB0`（`lea eax,[ecx+0x120c]; ret`）；构造点 `0x41EA33`/`0x41EAEE` 等。
- **`*(B+0x44D8) == host`**（自指针）：写点 `0x41EB21`（`mov [edi+0x56e4], edi`；`B+0x44D8 = host+0x56E4`）；读点 `0x41F6C0`（`mov eax,[ebx+0x44d8]; … mov eax,[eax+0x5760]`，`ebx = B`）。
  - ⚠️ th16 的 `B+0x44B8` 在 th17 应改为 **`B+0x44D8`**（宿主/子对象整体长大 `0x20`；`0x44B8` 在 th17 `.text` **0 命中**）。
- 与方案二的关系：方案二**不需要** `B` —— 直接从 `mgr` 槽表 + 链取 host 即可；`B`/自指针仅供方案一或交叉校验。

---

## 3. Q-boss-2 —— 当前 sub 名读取链（复核）

与 `docs/th17/th17_ecl_re_report.md` §1/Q2 **一致**（逐字段复核通过）：

```text
thread = *(u32*)(host + 0x0C)        // host+0xC=当前 thread；根线程内联于 host+0x10
subIdx = *(i32*)(thread + 4)         // 当前 sub 索引（-1=线程结束）
res    = *(u32*)(host + 0x11F8)      // ECL 资源对象 = *(u32*)(mgr + 0x17C)
count  = *(i32*)(res + 8)            // sub 总数
table  = *(u32*)(res + 0x8C)         // sub 表，项 8B
name   = *(char**)(table + subIdx*8) // 项 +0 = 名字 char*；项 +4 = 起始
```

反汇编佐证（解释器 `FUN_00478A70` 入口）：

```asm
00478a93  8b4f04            mov  ecx, [edi+4]        ; ★ thread+4 = sub 索引 (edi=thread)
00478a9f  8b8718100000      mov  eax, [edi+0x1018]   ; thread+0x1018 = host
00478aaf  8b80f8110000      mov  eax, [eax+0x11f8]   ; ★ host+0x11F8 = res
00478ab5  8b808c000000      mov  eax, [eax+0x8c]     ; ★ res+0x8C = sub 表
00478abb  8b44c804          mov  eax, [eax+ecx*8+4]  ; 表项[+4] = 起始
```

写点：
- `host+0xC = thread`：`0x41D738`（`mov [ecx+0xc], eax`，构造时 `eax = ecx+0x10`，根线程内联）。
- `thread+0x1018 = host`：`0x41D6D8`（`mov [ecx+0x1028], ecx`；`thread = host+0x10` ⇒ `thread+0x1018 = host+0x1028`）。
- `host+0x11F8 = res`：`0x41ED6F`（`mov [edi+0x11f8], eax`，`eax = [ecx+0x17C] = *(res 源+0x17C)`）。
- 建表器 `FUN_0047AEF0`、按名查表 `FUN_0047AE40`（名字运行期保留）见 ECL 报告 §2。

⇒ **boss 侧读法与道中完全相同，只是 `host` 换成 boss 宿主。**

---

## 4. Q-boss-3 —— boss 脚本 / 阶段名

### 4.1 文件槽与脚本组织（本作全关一致）

`.decl` 头 `ecli{}`（th17 ST1–ST7 相同）：

```text
ecli { "default.ecl"; "stNNbs.ecl"; "stNNmbs.ecl"; }
```

⇒ `slot 0 = stNN.ecl`（关卡主线）｜`slot 1 = default.ecl`｜`slot 2 = stNNbs.ecl`（boss）｜`slot 3 = stNNmbs.ecl`（道中 boss）。

- 进 boss：主线 `main` 里 `call MainBoss`（如 st01 `0x7d84`）；`MainBoss` 内 `enmCreateA("Boss", …)`（`0x448C`，指令 id `0x012D`）→ 生成 boss 敌人及其宿主、运行 `stNNbs` 的 `Boss` sub。
- 进道中 boss：主线 `main` 里 `enmCreateA("MBoss", …)`（如 st01 `0x7C94`）→ `stNNmbs` 的 `MBoss` sub。

各关 `main` 内的 `enmCreateA` 名称（`id=0x012D`，名在指令 `+0x14`）：

| 关卡 | `enmCreateA("MBoss")` | `call MainBoss` | 备注 |
| --- | --- | --- | --- |
| st01 | `0x7C94` | `0x7D84` | |
| st02 | `0x9E44` | `0x9EF4` | |
| st03 | `0x907C` | `0x9140` | |
| st04 | `0xC704` | `0xC7C8` | |
| st05 | `0xB1AC` | `0xB270` | |
| st06 | `0x7570` | `0x7634`（`call` 名即 `MainBoss`） | |
| st07 | `0xA714` | `0xA7D8` | |

### 4.2 阶段推进 = `setNext`（opcode `0x202`）

- `setNext(slot, hp, time, "下一攻击名")` ＝ opcode **`0x202`**，instruction size `0x2C`（44B），名字串内嵌于指令尾。
  例（离线 `.decl` 行号 + 原始指令地址）：

  | `.decl` | 行号 | 所在 sub | 原始指令地址 | 内容 |
  | --- | --- | --- | --- | --- |
  | `st01mbs.decl` | 25 | `MBoss` | — | `setNext(1, 0, 1440, "MBossDead")` |
  | `st01bs.decl` | 53 | `Boss1` | `0x640` | `setNext(0, 2400, 1800, "BossCard1")` |
  | `st01bs.decl` | 204 | `BossCard1` | — | `setNext(0, 2400, 1800, "BossCard2")` |
  | `st01bs.decl` | 321 | `BossCard2` | — | `setNext(0, 0, 1800, "Boss2")` |
  | `st01bs.decl` | 526 | `Boss2` | — | `setNext(0, 0, 1800, "BossDead")` |

  指令原始字节（st01bs `0x640`，44B）：

  ```text
  0x640: t=0 · id=0x0202 · size=0x2C · mask · params (0, 0, 2400, 1800, 12, "BossCard1"\0)
  ```

- **`setNext` 注册的是「下一攻击」**（`0x202` 预注册），据此可重建阶段序：`Boss1 → BossCard1 → BossCard2 → Boss2 → BossDead`（st01）。
- `Boss()`（boss 脚本入口）按 `$SPELL_ID` 分派 `BossCard1/2`，否则落到第一个 nonspell（`call Boss1`）；thprac 的 `THPatch` 用「改名」与「spell practice 跳转」强制到指定 sub（见 §5.2）。

### 4.3 各关 boss 脚本 sub 名单（离线 `.ecl` 解析）

- **`stNNbs`（boss）**：`Boss`、`Boss1…BossK`、`BossCard1…BossM`、`BossDead`、`BossEscape`（+ 各 `_at`/`PhantomNN` 辅助 sub）。
- **`stNNmbs`（道中 boss）**：`MBoss`、`MBoss1`、`MBoss2`…、`MBossCardN`、`MBossDead`、`MBossEscape`。

各关数量（供回填核对）：

| 关 | `stNNbs` 主阶段数 | `stNNmbs` 主阶段数 |
| --- | --- | --- |
| ST1 | `Boss1,Boss2,BossCard1,BossCard2` | `MBoss`（→`MBossDead`） |
| ST2 | `Boss1..Boss3,BossCard1..BossCard3` | `MBoss` |
| ST3 | `Boss1..Boss3,BossCard1..BossCard3` | `MBoss` |
| ST4 | `Boss1..Boss3,BossCard1..BossCard3` | `MBoss` |
| ST5 | `Boss1..Boss4,BossCard1..BossCard4` | `MBoss` |
| ST6 | `Boss1..Boss7,BossCard1..BossCard7` | `MBoss` |
| ST7 | `Boss1..Boss10,BossCard1..BossCard10` | `MBoss,MBoss1..MBoss3` |

---

## 5. Q-boss-4 —— boss 行回填进展

### 5.1 判定规则（用 `THPatch` 的落点 + `.decl`）

对 CSV 每个 boss 行，取 `THPatch`（`thprac_th17.cpp:1547+`）该 section 的**最后一次落点**：

1. `ECLJump(ecl, 0, dest, 60)`（不 `SetFile`）→ 落在主线 `enmCreateA("MBoss")` / `call MainBoss`：
   - `enmCreateA("MBoss")` ⇒ 入口 sub = **`MBoss`**（道中 boss）；
   - `call MainBoss`（其内 `enmCreateA("Boss")`）⇒ 入口 = boss 脚本起点；但 `Boss` 被所有 boss section 共用，**不能用 `Boss` 作某段入口**，故取首个阶段 sub（`Boss1`），与 th16 §3.4 一致。
2. `SetFile(2)`（切到 `stNNbs`）后的**改名补丁** `ecl << pair{addr, (int8_t)0xNN}`：`addr` 指向某 `call "BossN"`/`"BossCardN"` 名字的**末位数字字符**；把该字节改成 `NN` 即改写被调 sub 名。
   - 例：ST1 BOSS3 `pair{0x5D4, 0x32}` ⇒ `call "Boss1"` 的 `'1'`(`0x5D4`)→`'2'` ⇒ **`Boss2`**。
   - 例：ST1 BOSS4 `pair{0x4B4, 0x32}` ⇒ `call "BossCard1"` 的 `'1'`→`'2'` ⇒ **`BossCard2`**。
   - 例：ST7 END_NS10 `pair{0x1180,0x31} << 0x30` ⇒ `"Boss1"`→`"Boss10"` ⇒ **`Boss10`**。
3. `SetFile(2)` 后的 spell-practice 跳转 `ECLJump(ecl, A, B, 1)`：`B` 落在某 `call BossCardN` 之前 ⇒ 入口 = **`BossCardN`**（再配合改名补丁确定 N）。

### 5.2 按上述规则解出的映射（已写入 CSV）

> `*` = 由 `setNext`/`Boss()` 分派 + `THPatch` 落点交叉确认；`†` = 仅由 `THPatch` 落点确认。

| 关卡 | 记录名 | 入口 sub | 依据（`THPatch` / `.decl`） |
| --- | --- | --- | --- |
| ST1 | MID1 | `MBoss` | `ECLJump(0,0x7C94)`=st01 `enmCreateA("MBoss")` |
| ST1 | BOSS1 | `Boss1` | `ECLJump(0,0x7D84)`=`call MainBoss`→`enmCreateA("Boss")`；`Boss()` 默认落 `Boss1` * |
| ST1 | BOSS2 | `BossCard1` | `SetFile2;ECLJump(0x39C,0x484,1); pair{0x4B4,0x31}` † |
| ST1 | BOSS3 | `Boss2` | `pair{0x5D4,0x32}`（`"Boss1"→"Boss2"`）* |
| ST1 | BOSS4 | `BossCard2` | `ECLJump(0x39C,0x484,1); pair{0x4B4,0x32}` † |
| ST2 | MID1 / BOSS1 | `MBoss` / `Boss1` | 同上 |
| ST2 | BOSS2/3/4/5 | `BossCard1`/`Boss2`/`BossCard2`/`Boss3` | `pair{0x514,…}`/`pair{0x738,0x32}`/`pair{0x514,0x32}`/`pair{0x738,0x33}` |
| ST3 | MID1 / BOSS1 | `MBoss` / `Boss1` | 同上 |
| ST3 | BOSS2..BOSS6 | `BossCard1`/`Boss2`/`BossCard2`/`Boss3`/`BossCard3` | `pair{0x588,…}`/`pair{0x7CC,…}` 交替 |
| ST4 | MID1 / BOSS1 | `MBoss` / `Boss1` | 同上 |
| ST4 | BOSS2..BOSS6 | `BossCard1`/`Boss2`/`BossCard2`/`Boss3`/`BossCard3` | `pair{0x4EC,…}`/`pair{0x730,…}` 交替 |
| ST5 | MID1 / BOSS1 | `MBoss` / `Boss1` | 同上 |
| ST5 | BOSS2..BOSS7 | `BossCard1`/`Boss2`/`BossCard2`/`Boss3`/`BossCard3`/`Boss4` | `pair{0x59C,…}`/`pair{0x8E4,…}`（末位 `0x34`） |
| ST6 | MID1 / BOSS1 | `MBoss` / `Boss1` | 同上（`0x7570`/`0x7634`） |
| ST6 | BOSS2..BOSS8 | `BossCard1`/`Boss2`/`BossCard2`/`Boss3`/`BossCard3`/`Boss4`/`BossCard4` | `pair{0x660,…}`（`0x644+0x1c`）/`pair{0xDA4,…}`（`0xd8c+0x18`）交替 |
| ST6 | BOSS9/10/11 | **留空** | `keepSpellDrops` 双分支：`true`→`Boss5/6/7`，`else`→`BossCard5/6/7`（🟡） |
| ST7 | MID1 | `MBoss` | `SetFile3; ECLJump(0,0xA714)`=st07 `enmCreateA("MBoss")` |
| ST7 | MID2 / MID3 | `MBoss2` / `MBoss3` | `pair{0x7FD,0x32}` / `0x33`（`"MBoss1"→"MBoss2/3"`）† |
| ST7 | END_NS1..NS8 | `Boss1`..`Boss8` | `END_NS1` 落 `MainBoss`（默认 `Boss1`）；`END_NSk` `pair{0x1180,0x3k}` * |
| ST7 | END_S1..S8 | `BossCard1`..`BossCard8` | spell jump `0x800 → 0x8A0/0x93C/0x9D8/0xA74/0xB10/0xBAC/0xC48/0xCE4` † |
| ST7 | END_S9 / END_S10 | `BossCard9` / `BossCard10` | spell jump `0x800 → 0xD80` / `0xE1C`（🟡 `keepSpellDrops` 另有 `Boss9/10` 分支） |

> 说明：END_NS1 落 `call MainBoss`（`0xA7D8`），`Boss()` 默认落 `Boss1`；END_NS2..NS8 由 `pair{0x1180,0x3k}` 显式改名。END_Sk 由 `ECLJump(0x800, dest, 3)` 的 `dest` 定位到对应 `call BossCardk`。

### 5.3 已回填统计

- 回填：**ST1 全 5 行；ST2 全 6 行；ST3 全 7 行；ST4 全 7 行；ST5 全 8 行；ST6 9 行（MID1+BOSS1–8）；ST7 21 行（MID1–3 + END_NS1–8 + END_S1–10）**。
- 留空：**ST6 BOSS9/10/11**（3 行，`keepSpellDrops` 双分支，见 §6）。
- 仅改 CSV 第 1 列；`所属游戏/所属关卡/记录名` 未动（见 §8 变更记录）。

---

## 6. 风险与未验证点

1. **boss 宿主的识别（🟡 静态推得）**：boss ECL 跑在 boss 敌人自己的宿主里；本作取法（`mgr+0x48` 槽表 + `mgr+0x180` 链 + `host+0x5760` id）为**静态新增**，未真机三态。
2. **`mgr+0x180` 链含全部宿主**：`mgr+0x180` 链同时含 `main` 宿主与全部敌人/boss 宿主（`CreateScriptHost` 统一挂链）；故**必须靠 id 精确匹配**（`FUN_0041A5C0` 已如此），不可只取链首。
3. **`host+0x529C`（boss 槽位）**：全 `.text` 仅见「初值 `-1`（`0x41EBB5`）+ 单处读（`0x41DBA6`）」，未见明确赋真实槽号的写点 ⇒ 仅作候选（🟡）。
4. **`host+0x11F8` 是否全程指向 boss 自己的 ECL**（`stNNbs`/`stNNmbs`）需真机核对（🟡）；理论上 `host` 与 `res` 一一对应（构造时 `host+0x11F8 = *(mgr+0x17C)`）。
5. **ST6 BOSS9/10/11 双分支**：`keepSpellDrops=true` 走「Change Nonspell」（`Boss5/6/7`），`else` 走 spell practice（`BossCard5/6/7`）。当前 thprac 默认 `keepSpellDrops=false` ⇒ 默认落在 `BossCard5/6/7`；但 section 命名未指明 NS/S，故**留空**待确认。
6. **ST7 END_S9/END_S10 同类双分支**：本文按「spell 段 ⇒ `BossCard9/10`」回填，标注 🟡。
7. **`B` 生命周期**：方案一若用 `B`（`host+0x120C`）需在换关/退场清空（th15 分支正因 `B` 野指针崩溃）；**方案二不持有 `B`**，只按需从 `mgr` 取宿主，规避该风险。
8. **槽索引 `slot` 不固定**：本作 boss 占槽 0（`0x4250DD` 回退 0→1→2）；建议按 `FUN_0041A5C0(0/1/2)` 依次尝试取第一个非空 boss。
9. **变体（Steam / 汉化）RVA 未核**：`ImageBase`/`RELOCS_STRIPPED` 同，但 RVA 需按变体实测。
10. **latch 过场推进时机**：段落切换瞬间采样；建议在 IGI 渲染帧读「当前 sub 名」（与道中同一读取点）。

---

## 7. 地址表（th17，VA / RVA）

### 7.1 管理器 / 宿主链 / id

| 项 | VA | RVA | 说明 |
| --- | --- | --- | --- |
| 管理器全局 `ENEMY_MANAGER_PTR` | `0x4B76A0` | `0xB76A0` | `mgr = *(u32*)0x4B76A0` |
| 槽→id 表 | `mgr+0x48` | — | `id = *(u32*)(mgr+0x48+slot*4)` |
| 宿主链头 / 尾 / 第三头 | `mgr+0x180` / `+0x184` / `+0x188` | — | 挂链 `0x41E32A`；卸载 `0x41DB04/12` |
| 宿主计数 | `mgr+0x18C` | — | `inc` @ `0x41E35F` |
| 宿主链节点（内嵌） | `host+0x14F4` | — | `node[0]=host`、`node[+4]=next`、`node[+8]=prev`；初始化 `0x41EBBF` |
| 敌机 id | `host+0x5760` | — | 写点 `0x41ED2C`；比较点 `0x41A5F2` |
| `res`（manager 侧） | `mgr+0x17C` | — | `res = *(u32*)(mgr+0x17C)` |

### 7.2 关键函数 / 指令

| 项 | VA | RVA | 说明 |
| --- | --- | --- | --- |
| **取 boss（按槽）** `FUN_0041A5C0` | `0x41A5C0` | `0x1A5C0` | th15 `FUN_0041F7F0` 同源 |
| 按 id 取对象 `FUN_0041DDD0` | `0x41DDD0` | `0x1DDD0` | `ret 4` |
| 按 id 取对象 `FUN_0041E9E0` | `0x41E9E0` | `0x1E9E0` | `ecx`=含 id 的结构体指针 |
| 按 id 判存在 `FUN_0041DD90` | `0x41DD90` | `0x1DD90` | 返回 0/1 |
| 写槽 `FUN_0041DD20` | `0x41DD20` | `0x1DD20` | `mgr+0x48+slot*4 = host+0x5760` |
| 宿主工厂 `FUN_0041E140` | `0x41E140` | `0x1E140` | `malloc 0x576C`；链注册 `0x41E32A`；`ret 0xc`@`0x41E36A` |
| 宿主构造 `FUN_0041EA20` | `0x41EA20` | `0x1EA20` | node `0x41EBBF`；id `0x41ED2C`；res `0x41ED6F` |
| 宿主析构 / 卸载 | `0x41DAE0` | `0x1DAE0` | vtable `[esi]=0x4A1680`；卸 `mgr+0x180/184/188` |
| boss 门控判据 | `0x41DB98` | `0x1DB98` | `test [host+0x528C], 0x800000` |
| boss 门控（B 侧） | `0x423DA8` | `0x23DA8` | `[B+0x4080]` bit23 / bit30 |
| `B` getter | `0x41DAB0` | `0x1DAB0` | `lea eax,[ecx+0x120c]; ret` |
| `*(B+0x44D8)` 自指针 | 写 `0x41EB21` / 读 `0x41F6C0` | — | `= host` |
| 主线 "main" spawn | `0x431C58` / `0x431E83` | `0x31C58` / `0x31E83` | `call FUN_0041E140`（EAX=宿主） |

### 7.3 当前 sub 名链（复核）

| 项 | VA | RVA |
| --- | --- | --- |
| 解释器 `FUN_00478A70` | `0x478A70` | `0x78A70` |
| `subIdx = [edi+4]` 读点 | `0x478A93` | `0x78A93` |
| `host = [edi+0x1018]` 读点 | `0x478A9F` | `0x78A9F` |
| `res = [host+0x11F8]` 读点 | `0x478AAF` | `0x78AAF` |
| `table = [res+0x8C]` 读点 | `0x478AB5` | `0x78AB5` |
| `host+0xC = thread` 写点 | `0x41D738` | `0x1D738` |
| `thread+0x1018 = host` 写点 | `0x41D6D8` | `0x1D6D8` |
| `host+0x11F8 = res` 写点 | `0x41ED6F` | `0x1ED6F` |
| 建表器 `FUN_0047AEF0` | `0x47AEF0` | `0x7AEF0` |
| 按名查表 `FUN_0047AE40` | `0x47AE40` | `0x7AE40` |
| 主线命中点（EHOOK 4B） | `0x47A712` | `0x7A712` |

### 7.4 字面量 / 数据

| 项 | VA | 说明 |
| --- | --- | --- |
| `"main"` | `0x4A1770` | 全库仅 2 处引用（进关/重开） |
| `"MBossDead"` | `0x4A15F4` | |
| `"BossDead"` | `0x4A1600` | |
| `"BossEscape"` | `0x4A160C` | |
| opcode `setNext` | `0x0202` | `size=0x2C`；名字内嵌指令尾 |
| opcode `enmCreateA` | `0x012D` | `size=0x30`；名在指令 `+0x14` |

### 7.5 离线素材

`../exes/ecl/th17/st01…st07.ecl`（原始）、`stNNbs.ecl`/`stNNmbs.ecl` 的 `thecl -d` 反编译 `../exes/ecl_dump/th17/*.decl`。
示例：`st01bs.decl:53`（`Boss1` 内 `setNext(0,2400,1800,"BossCard1")`）、`st01mbs.decl:25`（`MBoss` 内 `setNext(1,0,1440,"MBossDead")`）。

---

## 8. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-16 | **建档**。落盘 TH17 boss 段监控「方案二」逆向：**boss 宿主取法（本作版）**——管理器 `mgr = *(u32*)0x4B76A0`；**槽→id 表 = `mgr+0x48`**、**宿主链头 = `mgr+0x180`**（尾 `+0x184`）、链节点 `host+0x14F4`、**敌机 id = `host+0x5760`**；游戏自身「取当前 boss」函数 **`FUN_0041A5C0`（`0x41A5C0`）**（读 `mgr+0x48+slot*4` + 遍历 `mgr+0x180`，th15 `FUN_0041F7F0` 同源）；辅助 `FUN_0041DDD0`/`FUN_0041DD90`/`FUN_0041DD20`；宿主工厂 `FUN_0041E140`（`malloc 0x576C`）/构造 `FUN_0041EA20`。**订正**：`mgr+0x48`（非 `host+0x48`）才是槽→id 表；th17 ECL 报告 §4.1 的 `+0x48` 属 `mgr`。**门控位** `[host+0x528C]&0x800000`（B 侧 `[B+0x4080]`）。**`B = host+0x120C`，`*(B+0x44D8)=host`**（th16 的 `B+0x44B8` 在 th17 应改 `B+0x44D8`，宿主整体 +0x20）。当前 sub 名链复核通过（`thread+4`/`host+0x0C`/`host+0x11F8`/`res+0x8C`）。回填 `docs/logger/th17_checkpoints.csv` boss 行：ST1–ST5 全部、ST6 9 行、ST7 21 行；ST6 BOSS9–11 留空（`keepSpellDrops` 双分支）。状态：静态调研（🟡 未真机三态）。 |

# TH16 Boss 段监控 — 方案一 / 方案二 逆向调研报告

> 需求：`docs/req_logger.md`（ECL 段落监控 + 日志）
> 总括（术语口径以此为准）：`docs/logger/logger_re_disasm_overview.md`
> 同作：`docs/th16/th16_ecl_re_report.md`（TH16 ECL 段落监控 Q1–Q9；本报告是其「boss 段」的续篇）
> 旧分支参照：`feat/plogth15`（th15 miss/bomb 记录 + ECL 段落监控参照实现；`thprac/src/thprac/thprac_th15_missbomb.{h,cpp}`）
> 素材清单与来源：`docs/logger/decompile_materials.md`
> 二进制指纹：`../exes/th16.exe`，**683520 B**，PE `TimeDateStamp = 1501534029`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`
> （⇒ **RVA = VA − 0x400000**；`.text` = VA `0x401000`–`0x48ACDA`，RVA `0x1000`–`0x89CDA`）
> 引擎代：**C 代（第四代引擎）**；同代参照作 th15。
> 方法：`pefile` + `capstone 5.0.7` 只读反汇编；离线素材 `../exes/ecl_dump/th16/`（`thecl -d` 反编译 `.decl`）、`../exes/ecl/th16/`（原始 `.ecl`）。
> 状态：**静态调研（🟡 未真机三态）**。本机 macOS 不可编译、不可真机；未真机验证的地址/结论文中均标 🟡。
> 地址口径：正文以 **VA** 为主，并同时标注 **RVA = VA − 0x400000**。

---

## 0. 结论速览

| 维度 | 方案一：boss「下一阶段名」+ 映射 | 方案二：ECL 执行指针取当前 sub 名 |
| --- | --- | --- |
| 机制 | 读 **boss 对象 `B`** 内固定偏移处的「下一阶段名」（`setNext`/opcode `0x202` 写入的名字），经 `<关卡号,名字>` 映射换算成当前阶段名 | 读 **boss 宿主**里 ECL 引擎当前执行 **sub 索引** → sub 名，白名单 latch 映射成段落名（与道中同机制） |
| 是否共用 boss 对象取法 | 都落到同一个 **boss 对象 `B` = `host + 0x120C`**；但**取得 B/host 的路径不同**（方案一用钩点寄存器 `EDI`；方案二用 `mgr+0x48+slot*4`（host 的 id）+ 遍历 `mgr+0x180` 链） | 同左 |
| boss 对象能否固定偏移取 ECL 对象 | **能**：`B → host=*(B+0x44B8) → thread=*(host+0xC) → subIdx=*(thread+4) → res=*(host+0x11F8) → table=*(res+0x8C) → name`（三处独立同构代码佐证） | 同左 |
| 语义 | 「**下一**阶段名」，需换算成当前阶段名 | 「**当前** sub 名」，直接对白名单 latch |
| 是否需要映射表 | 需要「名字→当前阶段名」映射表（**尚未填**） | 需要白名单（段落入口函数→记录名），**不需**名字换算 |
| 与道中一致性 | 与道中**两套机制** | 与道中**同一机制** |
| 真机验证状况 | th15 分支上**已真机验证可行**（th15 成品）；th16 未真机 | th16 主线同机制已静态定案；boss 宿主取法为**新增**、未真机 |
| 主要待办 | 补 th16 boss 阶段名映射表；新增 `thprac_th16_missbomb.{h,cpp}` 并接钩 | 新增 th16 logger 模块；回填 `docs/logger/th16_checkpoints.csv` 的 **72 行** boss 行 |
| 主要风险 | `B` 生命周期（野指针，须在换关/退场清空）；槽索引 `s` 不固定；映射表完备性 | boss 宿主取法（boss 不在主线宿主）；`host+0x11F8` 是否全程指向 boss 自己的 ECL（🟡） |

> **一句话**：两方案**本质是同一对象（boss 对象 `B`）的两个不同字段入口**，**非互斥**——方案一读 `B` 的「名字槽」，方案二读 `B → host → thread → res → sub 表` 的 sub 名。

---

## 1. 背景与目标

logger 需求（`docs/req_logger.md`）要求监控 ECL 引擎**当前正在执行的脚本函数名**，并在白名单命中时把「当前段落」置为该 checkpoint。TH16 的章节（道中）段落已在 `docs/th16/th16_ecl_re_report.md` 定案并回填 `docs/logger/th16_checkpoints.csv`（39 行章节行）。

本报告处理剩余的 **boss 段**：

- 需求：判断 th16 boss 战当前处于哪个「**阶段**」，**阶段名与 ECL boss 阶段函数名一致**（如 `Boss1`/`BossCard1`/`MBoss`…）。
- 素材：`../exes/ecl_dump/th16/stNNbs.decl`（boss）、`stNNmbs.decl`（道中 boss）。`st01bs.decl:56` 可见 `setNext(0,1700,1500,"BossCard1")` 形式的「下一阶段」注册。
- 两套候选方案：**方案一**（boss「下一阶段名」+映射，旧分支 `feat/plogth15` 路线）、**方案二**（ECL 执行指针取当前 sub 名，与道中同机制）。
- 本报告的结论：两方案**均可行且共享同一个 boss 对象 `B`**，差别只在「取哪个字段」与「如何取得 `B`」。

---

## 2. 方案一：boss「下一阶段名」+ 映射（旧分支 `feat/plogth15`）

### 2.1 原理

读取 boss 对象内固定偏移处的「**下一阶段名**」（= `setNext` / opcode `0x202` 写入的名字），再经 `<关卡号,名字>` 映射换算成**当前**阶段名。

- 该路线在 **th15** 分支上**已真机验证可行**（th15 成品 `feat/plogth15:thprac/src/thprac/thprac_th15_missbomb.{h,cpp}`，提交 `a3d7c1c` / `5c6e8b3` / `0e4c02a`）；该分支废弃另有原因（见 §2.5），**非因方案不可行**。

### 2.2 「名字的持久位置」

名字串的**持久副本**落在 host 对象的槽队列里：

| 口径 | 副本1 | 副本2 |
| --- | --- | --- |
| host 对象 `H` | `H + 0x528C + s*0x88` | `H + 0x52CC + s*0x88` |
| boss 对象 `B`（= `H + 0x120C`） | **`B + 0x4080 + s*0x88`** | **`B + 0x40C0 + s*0x88`** |

- 由**写名字函数 `FUN_00424E40`**（VA `0x424E40` / RVA `0x24E40`）用 `strcpy` 落盘（实为逐字节拷贝循环；`ret 0xc`，3 个参数：`slot, hp, name`）。反汇编证实：`ECX = host`，`EDI = host + slot*0x88`（**槽项指针**），副本1 起点 `[edi+0x528C]`、副本2 起点 `[edi+0x52CC]`。
- 槽队列基址 = `H + 0x5284`（= `B + 0x4078`），**8 槽 × 0x88**。

### 2.3 槽索引 `s` 不固定（关键）

- `s` = 该 `0x202` 指令**参数[0]**（**槽索引**，0..7），**不固定**：
  - th15 曾为 1；
  - th16 `st01bs.decl:56` 为 `setNext(0, 1700, 1500, "BossCard1")` ⇒ **`s = 0`**。
- ⇒ 读取**必须用命中时的槽索引动态算偏移**，**不能硬编码**：
  - th15「换阶段钩」处 = **命中时的 `EAX`**（`= 参数[0]`）；
  - th16 `call 0x424E40` 命中点处 = **栈上 `[ESP+0]`**（见 §2.4）。

### 2.4 临时 vs 持久

`call 0x424E40` 的命中点 `0x4215B4`（normal，RVA `0x215B4`）/ `0x42160B`（BossDead，RVA `0x2160B`）：

- 命中时栈上 `[ESP+0] = slot`、`[ESP+4] = hp`、`[ESP+8] = name 指针`（指令负载 / `.rdata` 临时指针）；
- 命中时 `EDI = B`（boss 对象）、`ECX = host`（`0x42159B: mov ecx,[edi+0x44b8]`）——**方案一用钩点寄存器 `EDI` 缓存 `B`**；
- 与之相对，对象内 `+0x4080 + s*0x88` 才是**可随时固定偏移读取的持久串**（不必落在 `call` 命中那一瞬）。

### 2.5 ⚠️ 口径订正（重要）

写名字函数 `FUN_00424E40` 内：**`EDI` 不是 boss 对象**（= `host + slot*0x88` **槽项指针**），**`ECX` 才是 host**。要缓存 boss 对象须在**调用点**取 `EDI`（调用点处 `EDI = B`），不要在 `FUN_00424E40` 函数体内把它当 boss。

### 2.6 th15 参照地址（VA）

| 项 | th15 地址 |
| --- | --- |
| 写名字函数 | `FUN_0042FF10` = `0x42FF10` |
| 换阶段钩 | `0x42C7D6`（normal）/ `0x42C785`（BossDead） |
| case512 | `0x42B92D`；钩点 `0x42B938` |
| boss 门控 | `[boss + 0x4060] & 0x800000` |
| 当前关卡号 | `0x4E73F0` |
| ECL 管理器 | `0x4E9A80` |
| 字面量 | `"BossDead"` = `0x4CC1C0`、`"BossEscape"` = `0x4CC1CC` |

### 2.7 th15 分支现状 / 缺陷

- 映射表 `TH15_MB_PHASE_MAP` 实为**占位未填**（源码中仅两行「示例，待手动填写」）。
- `g_boss_ptr` **生命周期缺失**，导致野指针崩溃（分支废弃原因之一）。
- th15 另有「**读槽写前旧值 = 当前名、无需映射**」的改进口径：因 `0x202` 预注册的是「下一攻击」，槽内**旧值**恰是「当前攻击」，在写前缓存旧值即可稳定得到当前名（见 `docs/th15/th15_re_disasm_report3.md` §2.6、`docs/th15/th15_missbomb_recording.md` §2.2）。

---

## 3. 方案二：ECL 执行指针取当前 sub 名（与道中同机制）

### 3.1 原理

读 ECL 引擎**当前执行 sub 索引**取 sub 名，经白名单 latch 映射成段落名。统一指针链（与 `docs/th16/th16_ecl_re_report.md` §1 同构）：

```text
host   = 宿主
thread = *(u32*)(host + 0x0C)
subIdx = *(i32*)(thread + 4)
res    = *(u32*)(host + 0x11F8)      // = *(u32*)(*(u32*)(0x4A6DC0)+0x17C)
count  = *(i32*)(res + 8)            // sub 总数（越界保护）
table  = *(u32*)(res + 0x8C)         // sub 表，项 8B
name   = *(char**)(table + subIdx*8) // 项 +0 = 名字 char*
```

### 3.2 关键结论：boss ECL 跑在 boss 敌人自己的宿主里

- boss ECL（`Boss` / `BossCardN` / `MBoss`）跑在 **boss 敌人自己的宿主**里（`enmCreateA` 异步生成），**不在主线宿主**。
- ⇒ **照搬道中「读主线宿主」做 boss，会恒定读到 `main` / `MainBoss`**。做 boss 段**必须先取得 boss 宿主**。

### 3.3 th16 取证关键点

| 项 | VA / RVA | 说明 |
| --- | --- | --- |
| 主线命中点 | `0x473995` / RVA `0x73995` | `Edi = thread`；`host = *(Edi + 0x1018)` |
| 主线宿主 spawn 站点 | `0x42DD52` / `0x42DF69` | `call FUN_0041AA70`（`EAX` = 宿主）；命名参数 `"main"` @ `0x492270` |
| `res` | `*(u32*)(*(u32*)(0x4A6DC0)+0x17C)` | 与 ECL 监控报告一致 |
| boss 门控 | `[boss+0x4060] & 0x800000` | bit23 |

### 3.4 数据前置：回填 72 行 boss 行

需回填 `docs/logger/th16_checkpoints.csv` 的 **72 行** boss 行（`MID` / `BOSS` / `END_*` / `*_FINAL`，目前**第 1 列全空**）：

- **记录名顺序**取自 `thprac_locale_def.h` 的 `TH16::th_sections_t`（约 3275–3349）与 `th_sections_cba[7][2][19]`；
- **sub ↔ 记录名**由 `thprac_th16.cpp` 的 `THPatch`（`SetFile(N)` + `ECLJump` 目标偏移）解出。
- **ST1 已解示例**：

  | 记录名 | boss 脚本 sub |
  | --- | --- |
  | ST1 MID1 | `MBoss` |
  | ST1 BOSS1 | `Boss1` |
  | ST1 BOSS2 | `BossCard1` |
  | ST1 BOSS3 | `Boss2` |
  | ST1 BOSS4 | `BossCard2` |

  （`st01bs.decl:56` `Boss1` 内 `setNext(0,1700,1500,"BossCard1")` 佐证 BOSS1→BOSS2 的「下一阶段」链。）

- **72 行**分关计数：ST1=5、ST2=6、ST3=8、ST4=8、ST5=10、ST6=14、ST7=21。

---

## 4. 两方案核心区别

| 维度 | 方案一 | 方案二 |
| --- | --- | --- |
| **数据来源** | boss 对象 `B` 内固定偏移的名字槽副本（`B+0x4080+s*0x88` / `B+0x40C0+s*0x88`） | `B → host → thread → res → sub 表` 的当前 sub 名 |
| **语义** | 「**下一**阶段名」（`0x202` 预注册） | 「**当前** sub 名」 |
| **是否需要映射表** | 需要「名字→当前阶段名」映射 **`<关卡号,名字>`** | 需要白名单（段落入口函数→记录名）；**不需**名字换算 |
| **与道中一致性** | 与道中**两套机制**（道中读 sub，boss 读名字槽） | 与道中**同一机制**（都是读 sub 名 + latch） |
| **th15 验证状况** | th15 分支**已真机验证可行**（成品已落盘） | th15 主线同机制已验证；**boss 宿主取法**为新增、未验证 |
| **覆盖 boss 的难点** | 槽索引 `s` 不固定；需在钩点取 `B`；映射表未填 | **必须先取得 boss 宿主**（boss 不在主线宿主） |
| **数据前置工作** | 补 th16 boss 阶段名映射表 | 回填 72 行 boss 行 |
| **主要风险** | `B` 野指针（生命周期）；映射表完备性 | boss 宿主取法；`host+0x11F8` 是否指向 boss 自己 ECL（🟡） |

> **结论**：两方案**本质是同一对象（boss 对象 `B`）的两个不同字段入口**，**非互斥**。
> 方案一 = 读 `B` 的「名字槽」（`B+0x4080/0x40C0`）；方案二 = 读 `B → host → thread → res → sub 表` 的 sub 名。二者所需的前提（取得 `B` / `host`）相同，先取得 `B` 即可两路取用。

---

## 5. 核心共性结论：boss 对象与 ECL 对象

### 5.1 boss 对象 `B` 的定义与自指针

- `B`（**boss 对象 / action 解释器 `this`**）= `host + 0x120C`。
  - 佐证：`0x41B590: lea esi,[edi+0x120c]`（`edi = host` 构造时取子对象基址）。
- `*(B + 0x44B8) == host`：`B + 0x44B8 = host + 0x56C4`。
  - 该字段全 `.text` **只有读、无写**（除唯一写点 `0x41B5C5`：`mov [edi+0x56c4],edi`，写 host 自身）。

### 5.2 能用固定偏移从 `B` 拿到 ECL 对象

```text
B = boss 对象（action 解释器 this）
host   = *(u32*)(B + 0x44B8)
thread = *(u32*)(host + 0x0C)
subIdx = *(i32*)(thread + 4)
res    = *(u32*)(host + 0x11F8)
table  = *(u32*)(res + 0x8C)
name   = *(char**)(table + subIdx*8)
```

**三处独立同构代码佐证**（`edd = B`，取 `[B+0x44B8] → host → +0xC → thread → +4/+8 → +0x1018 → +0x11f8`）：

| 函数 | VA / RVA | 关键片段 |
| --- | --- | --- |
| `FUN_0041DCB0` | `0x41DCB0` / `0x1DCB0` | `0x41DCE5…0x41DD19` |
| `FUN_00423050` | `0x423050` / `0x23050` | `0x42307C…0x4230AB` |
| `FUN_004251D0` 系列 | `0x4251D0` / `0x251D0` | `mov ecx,[ecx+0x44b8]; mov ecx,[ecx+0xc]; jmp 0x473c90` |

### 5.3 差别仅在「取得 `B` / `host`」这一步

- `B` **无全局指针**（th15 也如此）。
- **方案一**用**钩点寄存器 `EDI`**（th16 调用点 `0x4215B4`/`0x42160B` 处 `EDI = B`）。
- **方案二**用 `mgr + 0x48 + slot*4`（存的是 **host 的 id**）+ 遍历 `mgr + 0x180` 链表：
  - 按槽位查 host：`FUN_00417580`（`0x417580`，`mov esi,[0x4a6dc0]; mov edx,[esi+eax*4+0x48]`）；
  - 按 id 查 host：`FUN_0041A9C0`（`0x41A9C0`，`cmp [ecx+0x5740],edx`）。

### 5.4 ⚠️ 订正：链表里的对象本身就是 host

- `mgr + 0x180` 链表里的对象**本身就是 host（敌机本体）**，**不是 `B`**；
- `*(E + 0x44B8)` **仅在 `E = B`**（解释器 `this`，如 `0x4231C5` 处 `esi = B`）时才等于 host。
- 厂商做法：方案二从链表拿到 `E` 后**即 host**，**无需**再取 `[E+0x44B8]`。

### 5.5 `[boss+0x44B8]` 的含义

- `[boss + 0x44B8]` = **host（宿主 / 敌机对象）本身**，**不是**一个独立的「脚本资源对象」。
- `res = *(host + 0x11F8)` 才是 **ECL 资源对象**。
- th15 文档中 `scr` 有时指 host，**本报告统一称 host**。

---

## 6. 关键字段释义表

> 双列口径：**相对 `B`**（`B = host + 0x120C`）/ **相对 `H`**（`H = host`）。
> 槽项 `item = H + 0x5284 + s*0x88 = B + 0x4078 + s*0x88`（`s` = 槽索引，0..7）。

| 相对 B | 相对 H | 含义 | 写入点（VA） | 与 host/thread/res 关系 |
| --- | --- | --- | --- | --- |
| `+0x0000` | `+0x120C` | boss 子对象基址（`B = host + 0x120C`） | `0x41B590`（`lea esi,[edi+0x120c]`） | `B` = action 解释器 `this` |
| `+0x4060` | `+0x526C` | 标志位；**bit23 `0x800000` = boss 门控** | 置位 `0x4206A4`（`or [edi+0x4060],0x800000`）；清除 `0x42068F`（`and [edi+0x4060],0xff7fffff`） | — |
| `+0x4070` | `+0x527C` | boss 槽位 | 写 `0x4206B4`（`mov [edi+0x4070],esi`）；读 `0x42067C`（`mov ecx,[edi+0x4070]`） | — |
| `+0x4078` | `+0x5284` | 槽队列基址（**8 槽 × 0x88**） | 初始化 `0x41B8F9`（`mov [edi+0x5284],-1`） | `item = base + s*0x88` |
| `+0x407C` | `+0x5288` | 槽 +4：攻击 HP / 计时 | `0x424E50`（`mov [edi+0x5288],eax`） | — |
| `+0x4080` | `+0x528C` | 槽 +8：**名字副本1**（方案一读取点） | `0x424E70`（strcpy 落盘） | 持久可读 |
| `+0x40C0` | `+0x52CC` | 槽 +48：**名字副本2** | `0x424E86` | 持久可读 |
| `+0x44B8` | `+0x56C4` | **host 自指针**（`*(B+0x44B8) == host`） | `0x41B5C5`（`mov [edi+0x56c4],edi`） | = host；该字段只有读、无写（唯一写点即此） |
| —（H 独有） | `+0x000C` | ECL 线程 **thread** | `0x473BF8`（`mov [ebx+0xc],ecx`） | `thread = *(host+0xC)` |
| —（H 独有） | `+0x11F8` | ECL 资源对象 **res** | `0x41B89D`（`mov [edi+0x11f8],ecx`，`ecx = *(mgr+0x17C)`） | `res = *(host+0x11F8) = *(*(0x4A6DC0)+0x17C)` |
| —（H 独有） | `+0x14F4` | mgr 链表节点（`node[0] = host`） | `0x41B692`（`mov [edi+0x14f4],edi`）；挂链 `0x41AD00` / `0x41AD0C` | `mgr+0x180` 链 |
| —（H 独有） | `+0x5740` | 敌机 id | `0x41B865`（`mov [edi+0x5740],eax`） | 注册 `FUN_0041A910`（调用点 `0x4206AF`） |

**写名字调用点**：`0x4215B4` / `0x42160B`（栈上 `[ESP+0]=slot`、`[ESP+4]=hp`、`[ESP+8]=name`）。

**相关辅助函数**：

| 功能 | 函数 | VA / RVA |
| --- | --- | --- |
| 写名字（slot, hp, name），`ECX=host`、`EDI=host+slot*0x88` | `FUN_00424E40` | `0x424E40` / `0x24E40` |
| 按**槽位**查 host | `FUN_00417580` | `0x417580` / `0x17580` |
| 按 **id** 查 host | `FUN_0041A9C0` | `0x41A9C0` / `0x1A9C0` |
| 注册 id（调用点 `0x4206AF`） | `FUN_0041A910` | `0x41A910` / `0x1A910` |
| 槽消费，返回名字**副本1** | `FUN_00424F00` | `0x424F00` / `0x24F00` |

**字面量**：`"MBossDead"` = `0x492164`、`"BossDead"` = `0x492170`、`"BossEscape"` = `0x49217C`、`"MBossBDead"` = `0x492188`。

**ECL 管理器**：`0x4A6DC0`（`+0x48` = 槽→id 表、`+0x180` = host 链头、`+0x17C` = res）。

---

## 7. 若要落地（两方案的待办清单）

### 7.1 方案一

- 新增 `thprac_th16_missbomb.{h,cpp}`（镜像 th15 的 `thprac_th15_missbomb.{h,cpp}`）。
- `thprac_th16.cpp` 加钩子：
  - `0x4215B4` / `0x42160B`：缓存 boss 对象 `B`（钩点 `EDI`）；
  - `0x420665`：case512 进 / 退场；
  - `0x42E5AE`：`th16_game_start`（一局边界）；
  - `0x443D3A`：miss（`th16_life_dec`）；
  - `0x40DB9C`：bomb（`th16_bomb_dec`）。
- `.vcxproj` / `.filters` 登记新文件。
- 补 **th16 boss 阶段名映射表**。

### 7.2 方案二

- 新增 th16 logger 模块：`GetEclSubName()`（主线）+ `GetBossEclSubName()`（boss 宿主）。
- 同样登记 `.vcxproj` / `.filters`。
- 回填 `docs/logger/th16_checkpoints.csv` 的 **72 行** boss 行。

### 7.3 现有 th16 相关钩子

- `thprac_th16.cpp:2764-2778`：`th16_lock_timer1..4` = `0x426D4B` / `0x424E40` / `0x420665` / `0x42509A`。

---

## 8. 风险与未验证点

1. **boss 不在主线宿主**（🟡 静态推得）：boss ECL 跑在 boss 敌人自己的宿主（`enmCreateA` 异步生成）里；须先取得 boss 宿主，否则恒定读到 `main`/`MainBoss`。
2. **逐关 boss 映射表未解完**：ST2–ST7、ST6 `*_FINAL`、ST7 `END_*` 的 sub↔记录名尚未逐条解出（仅 ST1 已解）。
3. **`host+0x11F8` 是否全程指向 boss 自己的 ECL**（`stNNbs`）需真机核对（🟡）。
4. **槽索引 `s` 不固定**（th15 曾为 1、th16 st01 为 0）：读取必须用命中时槽索引动态算偏移。
5. **`B` 野指针**：须在换关 / 退场时清空；th15 分支正因 `g_boss_ptr` 生命周期缺失而崩溃。
6. **latch 过场推进**：段落切换瞬间的采样时机；「命中即置值、未命中 no-op」是否覆盖所有过场。
7. **`0x473995` 非无条件每帧**（连续立即指令时不经过）：可作读取点但非无条件每帧触发。
8. **ST5 A/B 分支**、**ST7 EX 变体**：白名单须同时收录两条分支各自入口。
9. **变体（Steam / 汉化）RVA 未核**：`ImageBase`/`RELOCS_STRIPPED` 同，但 RVA 需按变体实测。

---

## 9. 关键证据与出处

### 9.1 文档出处

| 文档 | 用途 |
| --- | --- |
| `docs/th16/th16_ecl_re_report.md`（Q1–Q9） | TH16 主线 ECL 指针链 / 宿主捕获 / 判定口径 |
| `docs/logger/logger_re_disasm_overview.md` | 术语口径、统一数据模型、Q1–Q9、逐作锚点 |
| `docs/logger/th16_checkpoints.csv` | 段落白名单（39 行章节行已回填；**72 行 boss 行待回填**） |
| `docs/logger/decompile_materials.md` | 反编译素材来源 |
| `feat/plogth15` 的 `docs/th15/th15_re_disasm_report3.md`（§2.6）、`docs/th15/th15_missbomb_recording.md`（§2.2） | 方案一「读槽写前旧值」改进口径 |
| `../exes/ecl_dump/th16/st01bs.decl`（等 `stNNbs.decl`/`stNNmbs.decl`） | boss 脚本 sub / `setNext` 结构 |

### 9.2 本次反汇编地址表（th16，VA / RVA）

| 项 | VA | RVA |
| --- | --- | --- |
| 写名字函数 `FUN_00424E40` | `0x424E40` | `0x24E40` |
| 写名字调用点（normal） | `0x4215B4` | `0x215B4` |
| 写名字调用点（BossDead） | `0x42160B` | `0x2160B` |
| boss 子对象基址（`lea [edi+0x120c]`） | `0x41B590` | `0x1B590` |
| host 自指针写点 | `0x41B5C5` | `0x1B5C5` |
| 门控置位 / 清除 | `0x4206A4` / `0x42068F` | `0x206A4` / `0x2068F` |
| boss 槽位写 / 读 | `0x4206B4` / `0x42067C` | `0x206B4` / `0x2067C` |
| case512（进/退场） | `0x420665` | `0x20665` |
| `host+0x11F8`（res）写点 | `0x41B89D` | `0x1B89D` |
| `host+0x14F4`（链表节点） | `0x41B692` | `0x1B692` |
| `host+0x5740`（敌机 id）写点 | `0x41B865` | `0x1B865` |
| 同构取 ECL 链 `FUN_0041DCB0` | `0x41DCB0`（链 `0x41DCE5…0x41DD19`） | `0x1DCB0` |
| 同构取 ECL 链 `FUN_00423050` | `0x423050`（链 `0x42307C…0x4230AB`） | `0x23050` |
| 同构取 ECL 链 `FUN_004251D0` | `0x4251D0` | `0x251D0` |
| 按槽位查 host `FUN_00417580` | `0x417580` | `0x17580` |
| 按 id 查 host `FUN_0041A9C0` | `0x41A9C0` | `0x1A9C0` |
| 注册 id `FUN_0041A910`（调用点 `0x4206AF`） | `0x41A910` | `0x1A910` |
| 槽消费 `FUN_00424F00` | `0x424F00` | `0x24F00` |
| 主线命中点 | `0x473995` | `0x73995` |
| 主线宿主 spawn 站点 | `0x42DD52` / `0x42DF69` | `0x2DD52` / `0x2DF69` |
| 宿主 spawner `FUN_0041AA70` | `0x41AA70` | `0x1AA70` |
| 宿主更新（逐线程）`FUN_00473BC0` | `0x473BC0` | `0x73BC0` |

**字面量**：`"main"` = `0x492270`；`"MBossDead"` = `0x492164`；`"BossDead"` = `0x492170`；`"BossEscape"` = `0x49217C`；`"MBossBDead"` = `0x492188`。

**管理器**：`0x4A6DC0`（`+0x48` 槽→id、`+0x180` host 链头、`+0x17C` res）。

**离线素材**：`../exes/ecl_dump/th16/st01bs.decl`（`Boss1` 内 `setNext(0,1700,1500,"BossCard1")`）。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-15 | **建档**。落盘 TH16 boss 段监控两方案调研：方案一（`feat/plogth15` 路线的 boss「下一阶段名」+映射；名字持久位置 `B+0x4080+s*0x88`/`B+0x40C0+s*0x88`；写名字函数 `FUN_00424E40`；调用点 `0x4215B4`/`0x42160B`；口径订正 `FUN_00424E40` 内 `EDI`=槽项指针、`ECX`=host）、方案二（boss 宿主内 ECL sub 名；boss 跑在 boss 敌人自己宿主、不在主线宿主；72 行 boss 行待回填；ST1 已解）。核心共性结论：boss 对象 `B = host+0x120C`，`*(B+0x44B8)=host`（唯一写点 `0x41B5C5`），可由固定偏移取 ECL 对象（同构代码 `FUN_0041DCB0`/`FUN_00423050`/`FUN_004251D0`）；`mgr+0x180` 链对象本身即 host；`[boss+0x44B8]` = host 本身。含字段释义表、两方案对比表、落地待办、风险与未验证点、证据地址表。状态：静态调研（🟡 未真机三态）。 |

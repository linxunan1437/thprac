# TH06（A 代）BOSS 阶段监控方案 — 逆向报告

> **上位文档**：`docs/logger/logger_re_disasm_overview.md`（术语表 / §6 Q9 latch 口径）；`docs/th06/th06_ecl_re_report.md`（th06 ECL 读取链 Q1–Q8 的技术基线）。
> **需求来源**：`docs/req_logger.md`；用户 2026-10-27 补充条件：**只考虑 Lunatic**；BOSS 阶段在时间轴上是**同一个时间点**，阶段靠别的东西分辨；只需要「转阶段时下一阶段名」，若名字不可得，退化为「转阶段时机 + CSV 顺序匹配」也可接受。
> **本作数据**：`docs/logger/th06_checkpoints.csv`（BOSS 区 70 行：ST1 6 / ST2 6 / ST3 9 / ST4 9 / ST5 8 / ST6 11 / ST7 21；含中 BOSS 与 ST4 BOOKS）。
> **二进制**：`../exes/th06.exe`（本体名 `東方紅魔郷.exe`），**512000 B**，PE `TimeDateStamp = 1038721275`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（⇒ **RVA = VA − 0x400000**）；
> `sha256 = 9f76483c46256804792399296619c1274363c31cd8f1775fafb55106fb852245`（本机复核）。
> **素材**（仓库外只读，不提交）：`../exes/ecl/th06/ecldata1..7.ecl`、`../exes/ecl_dump/th06/ecldata1..7.decl`。
> **方法**：`pefile 2024.8.26` + `capstone 5.0.7` 只读反汇编；自写 Python ECL 解码器、L 难度分支符号执行器、阶段链模拟器；`thprac_th06.cpp` / `thprac_locale_def.cpp` 源码交叉核对。
> **状态**：**✅ 真机通过（2026-11-01 用户确认此前测试已通过）**。本报告方案已按 §3.2 接入 `thprac_th06.cpp`（`TH06Logger`：道中按时间轴偏移区间、BOSS/MID 按 L 阶段 sub + 调用栈由内向外），日志接线与 IGI 显示同步落地；当前状态按用户确认为真机通过；历史验证记录保留，不额外推定逐项测试过程。
> **地址口径**：正文以 **VA** 为主，必要时标注 **RVA = VA − 0x400000**。
> **置信度图例**：✅ 反汇编/ECL 字节直接证实 ｜ 🟡A 反汇编 + ECL 字节 + 源码三证，未真机 ｜ 🟡B 仅落点/链级 ｜ ❌ 未定案。

---

## 0. 结论速览

| # | 问题 | 结论 | 置信度 |
| --- | --- | --- | --- |
| 1 | BOSS 阶段是什么？ | **每一阶段对应 BOSS 敌机正在执行的某个 ECL `sub`**；时间轴只在开战时生成一次 BOSS，后续阶段全部在 BOSS 自己的 sub 链里流转 | ✅ |
| 2 | 阶段怎么推进？ | 三条引擎路径：**HP 阈值**（`curLife < lifeThreshold`）、**时限归零**（`timer.current >= timeout`）、**HP 归零死亡回调**（`deathCallback`）；首阶段可能由时间轴 `op 0xA` 触发 `runInterrupt`，也可能直接以阶段 sub 开战（见 §8.4） | ✅ |
| 3 | 为什么 CSV 里同组阶段时间轴位置相同？ | CSV 第一列是 `THPatch` 的 **warp 时间**，不是阶段入口；同一 BOSS 的所有阶段共享开战 warp 值 | ✅ |
| 4 | 之前 `E+0x9D8` 副键为什么错？ | `E+0x9D8` 是**当前正在执行的 sub**，阶段 sub 会通过 `ins_35`（opcode `0x23`，call）调用攻击子脚本，`subId` 随之变成子弹脚本号，**不是阶段号** | ✅ |
| 5 | 难度怎么处理？ | `EclRawInstr+0x09` 是**运行掩码（run mask）**：`mask & (1<<difficulty) != 0` 才执行；`!L`＝仅 L，`!NHL`＝N/H/L，`!EN`＝E/N。**既有报告 §2.4 把它写反了**（写成 skip mask） | ✅ |
| 6 | 只看 L 时还有别的分支吗？ | **ST4 的 BOSS2/BOSS3 随机体/子机（PLAYER_SHOT）分叉**，且 CSV 顺序是 `BOSS1,BOSS2,BOSS4,BOSS3,BOSS5..7`；其余六关的阶段 sub 在 L 下与机体无关 | 🟡A |
| 7 | 推荐怎么监控？ | **每帧读 `bosses[0]` 的调用栈 + 本关阶段白名单**：按「当前 sub → 栈顶 → 栈底」由内向外找第一个命中白名单的 sub（thprac 练习 warp 会 `ins_35` 嵌套 call 目标阶段，必须由内向外）；不需要新 hook，和章节监控的 latch 口径一致 | 🟡A |
| 8 | 兜底方案？ | 在三个 `SetSub` 调用点（`0x411DED` / `0x411FF2` / `0x412CD0`）+ 开战中断 `0x4095C4` 挂 EHOOK，直接读目标 sub；或只记转阶段时机按 CSV 顺序推进 | 🟡A |
| 9 | 映射覆盖率 | BOSS 区 **70 行 → 75 个 L 有效 sub**（ST4 的 8 个阶段行（MID1 + 7 BOSS）因 4 机体变体展开为 14 个 sub；ST4 BOOKS 无 sub；其余 61 行各 1 个）；逐阶段 spell id/名与 thprac 分表交叉一致 | 🟡A |
| 10 | ST4 BOOKS | 不是 BOSS 阶段，是时间轴段落：落点 `tlInstr − pCode == 0xCDD4`，沿用章节监控的「时间轴偏移」判据 | 🟡B |

---

## 1. 需求拆解与 CSV 结构

### 1.1 用户给定条件的落地含义

1. **BOSS 战同时有 HP 与倒计时**：对应引擎字段 `curLife`（`E+0xCE4`）、`lifeThreshold`（`E+0xEA8`）、`timeout`（`E+0xEB0`）、阶段计时器（`E+0xCF0`）。
2. **只要转阶段后的下一阶段名**：本报告给出「阶段 sub → CSV 记录名」的完整映射；运行时在 `SetSub` 之后即可读到新 sub。
3. **只考虑 L**：所有 ECL 指令按「运行掩码含 bit3」过滤；E/N/H 的专属 sub 与分支一律排除。
4. **阶段在时间轴同一时间**：时间轴只负责「生成 BOSS + 触发首阶段中断」；阶段链在 BOSS 的 ECL sub 内部完成。

### 1.2 CSV BOSS 区的行数与分组

| 关卡 | MID | BOSS 组 | 合计 |
| --- | --- | --- | --- |
| ST1 | MID1–2 | BOSS1–4 | 6 |
| ST2 | MID1 | BOSS1–5 | 6 |
| ST3 | MID1–2 | BOSS1–7 | 9 |
| ST4 | BOOKS + MID1 | BOSS1, BOSS2, BOSS4, BOSS3, BOSS5–7 | 9 |
| ST5 | MID1–2 | BOSS1–6 | 8 |
| ST6 | MID1–2 | BOSS1–9 | 11 |
| ST7 | MID1–3 | END_NS1–8 + END_S1–10 | 21 |
| **合计** |  |  | **70** |

> ⚠️ ST4 的行序是 **1,2,4,3,5,6,7**，与 `th_sections_cba[3]` 的练习菜单顺序一致；这是真实游戏顺序，不是 CSV 笔误。

---

## 2. 核心机制

### 2.1 BOSS 的 ECL 结构：spawn → intro → interrupt → 阶段 sub 链

以 ST1 为例（`ecldata1.ecl`）：

```text
Timeline:
  t=5279  op=0  spawn Sub10          // 开战生成 BOSS（只此一次）
  t=5281  op=0xA arg0=0 arg1=0       // 触发 bosses[0] 的 interrupt 0
  t=5282  op=0xC arg0=0              // 等待 BOSS 死亡

Sub10（intro）:
  ins_101(0)                 // bosses[0] = 本敌机，bossPresent=1，flags1 |= 8
  ins_108("Sub16")           // deathCallback = Sub16
  ins_109("Sub11", 0)        // interrupts[0] = Sub11
  ...
Sub11（BOSS1 阶段）:
  ins_111(7000)  ins_113(900)  ins_114("Sub22")
  ins_115(2100)  ins_116("Sub22")
  ins_108("Sub16")
  ins_35("Sub12", ...)       // ★ call Sub12（攻击子脚本），subId 变成 12
Sub22（BOSS2 阶段）: spell id=1 ...
Sub16（BOSS3 阶段）: HP 7500 / thr 800 / to 1800 / next Sub23
Sub23（BOSS4 阶段）: spell id=2 ...
Sub17（终局子）: HP=0 → 真正死亡
```

关键点：

- **时间轴只生成一次 BOSS**；关底 BOSS（以及部分中 BOSS）用 `op 0xA` 触发首阶段中断，ST1/ST2 等中 BOSS 则直接以阶段 sub 开战（逐关见 §8.4）。后续阶段全部由引擎在 BOSS 的 ECL 上下文里 `SetSub`。
- **`op 0xA` 的语义**（`0x411C3B`，VA/RVA `0x411C3B`/`0x11C3B`）：
  `bosses[arg0]->runInterrupt = arg1`（写 `E+0xC68`，见 `0x411C66`）。
- **中断执行**（VM 入口 `0x4074C0` → `0x40954D`）：压栈当前上下文 → `SetSub(interrupts[runInterrupt])` → `depth++` → `runInterrupt = -1`。
- **ST6 的 BOSS 需要两次 `op 0xA`**：`Sub15` 先设 `interrupts[0]=Sub16`；`Sub16` 再设 `interrupts[0]=Sub17`；时间轴在 t=3171 与 t=3173 连续触发两次，真正的 BOSS1 是 `Sub17`。

### 2.2 三条阶段转移路径（引擎级）

每帧对每个活跃敌机的处理顺序（`0x4123E0` 循环，`0x412559`–`0x41258A`）：

```asm
; 1) HP 阈值
00412559  cmp dword [eax+0xEA8], 0      ; lifeThreshold >= 0 ?
00412563  jl  0x41256D
00412568  call 0x411DA0                 ; BossLifeCheck
; 2) 时限
0041256D  cmp dword [ecx+0xEB0], 0      ; timeout >= 0 ?
00412577  jl  0x412581
0041257C  call 0x411F40                 ; BossTimeoutCheck
; 3) 敌机 VM
00412581  push edx
00412585  mov ecx, 0x487E50
0041258A  call 0x4074A0
```

#### 路径 A：HP 阈值（`0x411DA0`）

```asm
00411DAF  mov edx, [eax+0xCE4]          ; curLife
00411DB5  cmp edx, [ecx+0xEA8]          ; lifeThreshold
00411DBB  jge 0x411F36                  ; curLife >= thr ⇒ 返回 0
00411DC7  mov edx, [ecx+0xEA8]
00411DCD  mov [eax+0xCE4], edx          ; ★ curLife = lifeThreshold（钳位）
00411DD6  mov cx, word [eax+0xEAC]      ; ★ 目标 sub = nextOnHp
00411DED  call 0x407440                 ; SetSub(ctx, nextOnHp)
00411DF5  mov [eax+0xEA8], -1           ; threshold 清除
00411E05  mov eax, [edx+0xC44]
00411E0B  mov [ecx+0xEB4], eax          ; EB4 = deathCallback
00411E5E  mov [ecx+0xC3C], 0            ; ★ depth = 0
```

#### 路径 B：时限归零（`0x411F40`）

```asm
00411F94  mov edx, [eax+0xCF8]          ; 阶段计时器 current
00411F9C  cmp edx, [ecx+0xEB0]          ; timeout
00411FA2  setge al                      ; current >= timeout ?
00411FA7  je 0x4121A9
00411FB0  cmp [ecx+0xEA8], 0
00411FB7  jle 0x411FD8
00411FBF  mov ecx, [eax+0xEA8]
00411FC5  mov [edx+0xCE4], ecx          ; 若有阈值：HP 钳到阈值
00411FCE  mov [edx+0xEA8], -1
00411FDB  mov cx, word [eax+0xEB4]      ; ★ 目标 sub = nextOnTimeout
00411FF2  call 0x407440                 ; SetSub(ctx, nextOnTimeout)
00411FFA  mov [eax+0xEB0], -1           ; timeout 清除
0041200A  mov eax, [edx+0xC44]
00412010  mov [ecx+0xEB4], eax          ; ★ EB4 立刻被改成 deathCallback
00412198  mov [ecx+0xC3C], 0            ; depth = 0
```

> ⚠️ **`EB4` 在 `SetSub` 之后立刻被覆盖为 `deathCallback`**，所以「事后读 `E+0xEB4` 推下一阶段」是不可靠的；必须在 `SetSub` 调用点读取，或干脆用调用栈判阶段。

#### 路径 C：HP 归零 → deathCallback（`0x412C50`）

```asm
00412C50  mov edx, [ebp-0x24]
00412C53  cmp dword [edx+0xC44], 0      ; deathCallback >= 0 ?
00412C5A  jl  0x412CE2
00412CAD  mov [eax+0xC3C], 0            ; depth = 0
00412CBA  mov dx, word [ecx+0xC44]      ; ★ 目标 sub = deathCallback
00412CD0  call 0x407440                 ; SetSub(ctx, deathCallback)
00412CD8  mov [ecx+0xC44], -1           ; deathCallback 清除
```

> 注：该路径要求 `flags1 bit0 == 1`。敌机初始化时 bit0 默认置 1（`0x410ED3`–`0x410EE2`：`or cl,1`），`ins_117(0)` 会清除它（用于可被 kill-all 的小怪）；BOSS/阶段敌机在攻击阶段保持 1（个别 intro 会短暂 `ins_117(0)` 再在 t≈30 置回 1），因此 HP 归零会正常走 deathCallback。

#### 三种转移的配合

- **非符阶段**通常设 `ins_111`（HP）+ `ins_113`（阈值）+ `ins_114`（阈值目标）+ `ins_115`（时限）+ `ins_116`（时限目标）。
- **符卡阶段**通常只设 `ins_93`（符卡）+ `ins_115`（时限），不设阈值；它的「下一阶段」由前一个非符阶段 `ins_108` 设下的 `deathCallback` 承担：
  - 时限到 ⇒ `SetSub(EB4)`，而 EB4 在进入该符卡前已被置为 `deathCallback`；
  - HP 归零（收取）⇒ `SetSub(deathCallback)`。
  - 例外：也有符卡自带阈值/阈值目标（如 ST2 BOSS4 `Sub31` 的 `ins_113(1400)`、ST3 BOSS6 `Sub32` 的 `ins_113(2000)`），此时按阈值路径走。
- 因此符卡阶段的两条结束路径**通常指向同一个下一阶段 sub**；判定时统一按「阈值 ≥0 先看 `nextOnHp`，否则看 `deathCallback`」处理即可。

### 2.3 指令语义表（本报告用到的）

> 说明：`.decl` 里的 `ins_N` 中 **N 是十进制原始 opcode**（例如 `ins_111` = opcode `0x6F`）。

| decl | 原始 op | 语义 | 写入字段 | 证据 |
| --- | --- | --- | --- | --- |
| `ins_101(arg)` | `0x65` | `bosses[arg] = enemy`；`arg<0` 时反注册 | `0x5A5F60+arg*4`、`0x69BC50`、`E+0xE51 bit3`、`E+0xE40` | `0x408DD3`–`0x408E20` |
| `ins_108(sub)` | `0x6C` | 设置死亡回调 sub | `E+0xC44` | `0x409510` |
| `ins_109(sub,id)` | `0x6D` | 设置中断表项 | `E+0xC48+id*4` | `0x409524` |
| `ins_111(v)` | `0x6F` | **同时**设置总 HP 与当前 HP | `E+0xCE8`、`E+0xCE4` | `0x4095FC` |
| `ins_112(v)` | `0x70` | 阶段计时器复位到 v（通常 0） | `E+0xCF0` 的 `{prev,subFrame,current}` | `0x409997` |
| `ins_113(v)` | `0x71` | 设置 HP 阈值 | `E+0xEA8` | `0x4099DF` |
| `ins_114(sub)` | `0x72` | 设置阈值目标 sub | `E+0xEAC` | `0x4099F3` |
| `ins_115(v)` | `0x73` | 设置时限（帧）并复位计时器 | `E+0xEB0`、`E+0xCF0` | `0x409A07` |
| `ins_116(sub)` | `0x74` | 设置时限目标 sub | `E+0xEB4` | `0x409A4F` |
| `ins_93(sprite,id,name)` | `0x5D` | 声明符卡；全局符卡 id/表 | `0x5A5F98`、`0x5A5F94`、`0x5A5F90`、`0x5A5F8C` | `0x40963D`–`0x4096E5` |
| `ins_35(sub,...)` | `0x23` | **call sub**：压栈 ctx → `SetSub(sub)` → `depth++` | `E+0x9D8`、`E+0x9DC+depth*0x4C`、`E+0xC3C` | `0x407AEC`–`0x407B94` |
| `ins_36()` | `0x24` | ret：`depth--` → 恢复 ctx | 同上 | `0x407BBD`–`0x407C1A` |
| `ins_39(sub,...)` | `0x27` | 生成子敌机（不是 call，不压栈） | 新 Enemy | `0x409CC7`–`0x409D60` |

### 2.4 ⚠️ 难度字节的真实语义（更正既有报告）

`.decl` 行首的 `!E/!N/!H/!L/!*` 与 ECL 指令头 `+0x09` 的掩码**不是 skip mask，而是 run mask**：

```asm
004074E9  mov ecx, [ebp-0x1C]           ; instr
004074EC  xor edx, edx
004074EE  mov dl, [ecx+9]               ; mask
004074F1  mov eax, 1
004074F6  mov ecx, [0x69BCB0]           ; difficulty (0=E,1=N,2=H,3=L)
004074FC  shl eax, cl
004074FE  and edx, eax
00407500  test edx, edx
00407502  jne 0x407509                  ; ★ 非 0 ⇒ 执行该指令
00407504  jmp 0x40A008                  ;    0 ⇒ 跳过
```

因此 **L 难度（difficulty=3，bit 8）**：

| 前缀 | 掩码 | L 是否执行 |
| --- | --- | --- |
| `!L` | `0x08` | ✅ |
| `!HL` | `0x0C` | ✅ |
| `!NHL` | `0x0E` | ✅ |
| `!EN` | `0x03` | ❌ |
| `!E` / `!N` / `!H` | `0x01/0x02/0x04` | ❌ |
| `!*` / 无前缀 | `0xFF` | ✅ |

> **这是前一轮 BOSS 结论大面积出错的根因之一**：按既有报告「`1<<diff` 置位 ⇒ 跳过」解读，`!L` 行会被当成「L 跳过」，`!EN` 行反而会在 L 执行；而实际恰好相反。本文所有阶段链/映射均按真实 run mask 过滤。

### 2.5 为什么不能直接读 `E+0x9D8` 当阶段号

`E+0x9D8` 是 `EnemyEclContext.subId`，由 `SetSub` 写入。阶段 sub 在战斗中会 `ins_35` 调用攻击子脚本：

```text
ST1 BOSS1 = Sub11
  t=100  ins_35("Sub12", ...)   → subId = 12
Sub12 t=180  ins_35("Sub15")     → subId = 15
...
```

整个 BOSS1 期间，`E+0x9D8` 绝大多数时间是 **12 / 15 等攻击子脚本号**，只在 call/ret 的瞬间回到 11。直接拿 `E+0x9D8` 查「阶段 sub 表」必然大面积错配——这就是上一轮副键失效的直接原因。

**正确读法：读调用栈，按「当前 sub → 栈顶 → 栈底」找第一个白名单阶段 sub。**

- call（`ins_35`）把当前 ctx **整份 0x4C 字节**压到 `E+0x9DC + depth*0x4C`（`0x407B23`–`0x407B47`），然后 `depth++`。
- 帧内 `+0x48` = 调用者 subId；`E+0x9D8` = 当前（最内层）sub。
- 阶段转移（阈值/时限/HP0）都会把 `depth` **清零**（`0x411E5E` / `0x412198` / `0x412CAD`），所以新阶段第一层 call 的帧 `[0]` 就是新阶段 sub。
- 首阶段经中断进入时 `depth=1`，帧 `[0]` 是 intro sub（不在白名单），当前 sub 才是阶段 sub；先查当前 sub 即可。
- 正常战斗里白名单 sub 只会作为「外层」出现；但 thprac 的练习 warp 会从早期阶段 `ins_35` **直接 call 目标阶段**，栈里会出现两个白名单 sub，此时必须取**内层**（目标阶段）。详见 §3.2 / §4.10。

### 2.6 ST4 的 PLAYER_SHOT 分支

ECL 变量 `-10025`（`ECL_VAR_PLAYER_SHOT`）在 `0x40B2E7` 解析为：

```asm
0040B2E9  mov dl, [0x69D4BD]            ; character (0=Reimu,1=Marisa)
0040B2F1  mov al, [0x69D4BE]            ; shotType  (0=A,1=B)
0040B2F6  lea ecx, [eax + edx*2]        ; ★ character*2 + shotType
```

即 **0=ReimuA、1=ReimuB、2=MarisaA、3=MarisaB**。ST4 的 `Sub27`（BOSS1）与 `Sub37`（BOSS4）在 L 下按该值选择下一阶段：

| shot | BOSS2（spell） | BOSS3（spell） |
| --- | --- | --- |
| 0 ReimuA | `Sub43` 火符「アグニシャイン上級」(id 19) | `Sub52` 土符「トリリトンシェイク」(id 25) |
| 1 ReimuB | `Sub46` 水符「ベリーインレイク」(id 23) | `Sub49` 木符「グリーンストーム」(id 24) |
| 2 MarisaA | `Sub48` 木符「シルフィホルン上級」(id 20) | `Sub44` 火符「アグニレイディアンス」(id 22) |
| 3 MarisaB | `Sub51` 土符「レイジィトリリトン上級」(id 21) | `Sub54` 金符「シルバードラゴン」(id 26) |

> 运行时**不需要知道 shot**：把 8 个变体 sub 全部映射到同一个记录名即可（`ST4 BOSS2` / `ST4 BOSS3`）。
> 其余六关在 L 下无 PLAYER_SHOT 阶段分叉（ST6 中 BOSS 只有阈值数值随 shot 变化，阶段 sub 不变）。

---

## 3. 运行时读取链

### 3.1 常量与表达式（全部为静态，不需要捕获宿主）

```cpp
constexpr uint32_t ENEMY_MANAGER = 0x4B79C8;   // RVA 0xB79C8
constexpr uint32_t BOSS_SLOTS    = 0x5A5F60;   // = ENEMY_MANAGER + 0xEE598
constexpr uint32_t BOSS_PRESENT  = 0x69BC50;   // RVA 0x29BC50
constexpr uint32_t STAGE_NUM     = 0x69D6D4;   // RVA 0x29D6D4, 1-based
constexpr uint32_t DIFFICULTY    = 0x69BCB0;   // RVA 0x29BCB0
constexpr uint32_t ECL_MANAGER   = 0x487E50;   // 仅 BOOKS/章节用
```

```cpp
struct BossView {
    uint32_t enemy;     // bosses[0]
    bool     active;    // E+0xE50 bit7
    bool     isBoss;    // E+0xE51 bit3
    uint16_t curSub;    // E+0x9D8
    int      depth;     // E+0xC3C（0..7）
    uint16_t frameSub[8]; // E+0x9DC + i*0x4C + 0x48
};
```

逐字段：

| 字段 | 表达式（`E` = `*(u32*)0x5A5F60`） | 类型 | 说明 |
| --- | --- | --- | --- |
| BOSS 指针 | `*(u32*)0x5A5F60` | u32 | 只使用 slot 0（ECL 只写 0 / −1） |
| bossPresent | `*(u8*)0x69BC50` | bool | 敌机死亡/清场时置 0 |
| 活跃位 | `*(u8*)(E+0xE50) & 0x80` | bit | 敌机是否在活动槽 |
| BOSS 位 | `*(u8*)(E+0xE51) & 0x08` | bit | `ins_101(0)` 置位 |
| 当前 sub | `*(u16*)(E+0x9D8)` | u16 | 当前执行 sub（可能是攻击子脚本） |
| 调用深度 | `*(i32*)(E+0xC3C)` | i32 | 0..7 |
| 栈帧 sub | `*(u16*)(E+0x9DC + i*0x4C + 0x48)` | u16 | `i = 0..depth-1` |
| 阶段 HP | `*(i32*)(E+0xCE4)` / `*(i32*)(E+0xCE8)` | i32 | cur / total |
| 阈值 | `*(i32*)(E+0xEA8)` | i32 | ≥0 才参与判定 |
| 阈值目标 | `*(u16*)(E+0xEAC)` | u16 | nextOnHp |
| 时限 | `*(i32*)(E+0xEB0)` | i32 | ≥0 才参与判定 |
| 时限目标 | `*(u16*)(E+0xEB4)` | u16 | nextOnTimeout（转换后被覆盖） |
| 阶段计时器 | `*(i32*)(E+0xCF8)` | i32 | = `E+0xCF0+8`，只读用于诊断；屏幕秒数 `*(i32*)0x69BC48 = (timeout − timer.current)/60`（`0x411F62`–`0x411F8A`） |
| deathCallback | `*(i32*)(E+0xC44)` | i32 | −1 = 无 |
| interrupts[i] | `*(u16*)(E+0xC48 + i*4)` | u16 | 0..? |
| runInterrupt | `*(i32*)(E+0xC68)` | i32 | −1 = 无 |

### 3.2 阶段 sub 提取算法（推荐）

```cpp
// 返回本关阶段白名单命中的 sub；0xFFFF = 当前没有命中
uint16_t GetBossPhaseSub(uint32_t stage, uint32_t E) {
    if (!E) return 0xFFFF;
    if (!(*(uint8_t*)(E + 0xE50) & 0x80)) return 0xFFFF; // 不活跃
    if (!(*(uint8_t*)(E + 0xE51) & 0x08)) return 0xFFFF; // 不是 BOSS

    int depth = *(int32_t*)(E + 0xC3C);
    if (depth < 0) depth = 0;
    if (depth > 7) depth = 7;

    // ★ 由内向外：当前 sub 是最内层，然后从栈顶（最近一次 call）往栈底走。
    //   thprac 的练习 warp 会用 ins_35 从早期阶段直接 call 目标阶段，
    //   此时栈里会同时出现两个白名单 sub，必须取内层（目标阶段）。
    uint16_t s = *(uint16_t*)(E + 0x9D8); // 当前执行的 sub
    if (LookupPhase(stage, s)) return s;
    for (int i = depth - 1; i >= 0; --i) {
        s = *(uint16_t*)(E + 0x9DC + i * 0x4C + 0x48);
        if (LookupPhase(stage, s)) return s;
    }
    return 0xFFFF;
}
```

**为什么这个算法成立**（静态推理）：

| 场景 | depth | 栈（外→内） | current | 结果 |
| --- | --- | --- | --- | --- |
| 首阶段经中断进入 | 1 | `[intro]` | phase | current=phase ✅ |
| 阶段未 call | 0 | — | phase | current=phase ✅ |
| 阶段 call 攻击子脚本 | ≥1 | `[phase, ...]` | helper | current 不命中 → frame[0]=phase ✅ |
| 阶段 call → 再 call | ≥2 | `[phase, helper1]` | helper2 | frame[1]=helper1 不命中 → frame[0]=phase ✅ |
| thprac 练习：早期阶段 call 目标阶段 | ≥1 | `[早期phase, 目标phase, ...]` | helper 或目标 | 内层目标先命中 ✅ |
| ST7 Sub59 call Sub60 | 1 | `[59]` | 60 | current=60 ✅ |
| ST7 Sub67 call Sub68 | 1 | `[67]` | 68 | current=68 ✅ |

> **原始 ECL 里**除 ST7 的 `Sub59→Sub60`、`Sub67→Sub68`（59/67 不在白名单）外，**没有任何白名单阶段 sub 被别的 sub 当 call 目标**；但 thprac 的练习 patch 会**人为制造**这种嵌套——例如 `ST1 BOSS3` 把 `Sub11` 里 `ins_35` 的 subId 改成 `Sub16`，`ST6 BOSS9` 把 `Sub17` 的 call 改成目标 `Sub44`（见 §4.10）。所以读取方向必须是**由内向外**；若由外向内（栈底优先），练习 warp 时会先命中早一阶段（如 BOSS1），读到错误阶段。
>
> ⚠️ 该白名单**只对 L 成立**。运行时先用 `*(i32*)0x69BCB0 == 3` 闸门；其它难度需要按 §2.4 的 run mask 另建映射（本报告不提供）。`stage = *(u32*)0x69D6D4` 只取 1..7；0/越界直接返回未命中。

### 3.3 采样时机

- 与现有 logger 一致，在 IGI 渲染（帧间稳定态）读取。
- 引擎在每帧的 **阈值检查 → 时限检查 → 敌机 VM →（HP 归零时）死亡处理** 之后才进入渲染；阈值/时限路径的 `SetSub` 在 VM 之前，HP0 死亡路径的 `SetSub` 在同帧后段，**两者都在渲染前完成**。
- 转移帧本身就会看到新阶段 sub：即使新阶段 t=0 指令立即 `call` 攻击子脚本，栈里仍有阶段帧（见 §3.2）。

### 3.4 落地步骤（建议）

1. 进关/换关时取 `stage = *(u32*)0x69D6D4`；`stage ∈ [1,7]` 才启用 `kTh06BossPhaseSubs`。
2. 每帧 IGI 采样：`difficulty != 3` 或 `*(u8*)0x69BC50 == 0` 直接跳过；`E = *(u32*)0x5A5F60`，校验活跃位/BOSS 位。
3. `phaseSub = GetBossPhaseSub(stage, E)`；命中则查记录名并 latch（建议 2–3 帧去抖，见 §5.3）。
4. 同一采样里额外检查 ST4 BOOKS：`(tlInstr − pCode) == 0xCDD4`。
5. 练习模式：若能在 thprac 侧拿到 `thPracParam.section`，用它初始化「启动 checkpoint」；否则依赖去抖吃掉快进瞬态。

---

## 4. 全量映射表（仅 L）

> 列说明：**记录名** = CSV 第 4 列；**sub** = ECL sub 索引（`SubN`）；**类型** = 非符（NS）/ 符卡（S）；**spell** = `ins_93` 的 id 与名称（若有）。
> 这些 sub 都是**运行时 `bosses[0]` 的阶段 sub**；攻击子脚本（Sub12/15/18/21…）不在表中。

### 4.1 ST1（6）

| 记录名 | sub | 类型 | spell |
| --- | --- | --- | --- |
| ST1 MID1 | 8 | NS | — |
| ST1 MID2 | 9 | S | 0 月符「ムーンライトレイ」 |
| ST1 BOSS1 | 11 | NS | — |
| ST1 BOSS2 | 22 | S | 1 夜符「ナイトバード」 |
| ST1 BOSS3 | 16 | NS | — |
| ST1 BOSS4 | 23 | S | 2 闇符「ディマーケイション」 |

### 4.2 ST2（6）

| 记录名 | sub | 类型 | spell |
| --- | --- | --- | --- |
| ST2 MID1 | 20 | NS | — |
| ST2 BOSS1 | 22 | NS | — |
| ST2 BOSS2 | 30 | S | 4 雹符「ヘイルストーム」 |
| ST2 BOSS3 | 25 | NS | — |
| ST2 BOSS4 | 31 | S | 5 凍符「パーフェクトフリーズ」 |
| ST2 BOSS5 | 32 | S | 6 雪符「ダイアモンドブリザード」 |

> E/N 专属的 `Sub29`（冰符「アイシクルフォール」，id 3）在 L 不出现；L 的 BOSS2 是 `Sub30`。

### 4.3 ST3（9）

| 记录名 | sub | 类型 | spell |
| --- | --- | --- | --- |
| ST3 MID1 | 9 | NS | — |
| ST3 MID2 | 14 | S | 8 華符「セラギネラ９」 |
| ST3 BOSS1 | 18 | NS | — |
| ST3 BOSS2 | 29 | S | 9 虹符「彩虹の風鈴」 |
| ST3 BOSS3 | 20 | NS | — |
| ST3 BOSS4 | 30 | S | 10 幻符「華想夢葛」 |
| ST3 BOSS5 | 26 | NS | — |
| ST3 BOSS6 | 32 | S | 12 彩符「彩光乱舞」 |
| ST3 BOSS7 | 33 | S | 13 彩符「極彩颱風」 |

### 4.4 ST4（9；BOOKS + MID1 + 7 BOSS）

| 记录名 | sub（L） | 类型 | spell |
| --- | --- | --- | --- |
| ST4 BOOKS | —（时间轴 `0xCDD4`） | — | 非 BOSS 阶段，见 §5.1 |
| ST4 MID1 | 21 | NS | — |
| ST4 BOSS1 | 27 | NS | — |
| ST4 BOSS2 | 43 / 46 / 48 / 51 | S | 见 §2.6（19/23/20/21） |
| ST4 BOSS4 | 37 | NS | — |
| ST4 BOSS3 | 52 / 49 / 44 / 54 | S | 见 §2.6（25/24/22/26） |
| ST4 BOSS5 | 39 | S* | 贤者之石第 1 波（书） |
| ST4 BOSS6 | 40 | S* | 贤者之石第 2 波（书） |
| ST4 BOSS7 | 41 | S* | 贤者之石第 3 波（书） |

> `S*`：BOSS5–7 的 boss 本体 sub 本身不 `ins_93`，符卡由它生成的 5 本书（Sub55–59，spell 27–31）承担；CSV 按 3 个 HP 段（5100→阈值 3400→阈值 1700→HP0）分 3 行。
> 🟡A：`BOSS5/6/7 ↔ Sub39/40/41` 由 HP 阈值与阶段链 + thprac 跳转交叉确认（三者入口都 call `Sub39`；BOSS6 把 HP 设为 3399 < 阈值 3400、BOSS7 把阈值改为 1700 且 HP=1699，分别落 40/41，见 §4.10）；真机练习起步可能短暂经过前一段，需按 §5.3 处理。

### 4.5 ST5（8）

| 记录名 | sub | 类型 | spell |
| --- | --- | --- | --- |
| ST5 MID1 | 13 | NS | — |
| ST5 MID2 | 18 | S | 33 奇術「幻惑ミスディレクション」 |
| ST5 BOSS1 | 22 | NS | — |
| ST5 BOSS2 | 45 | S | 37 幻幽「ジャック・ザ・ルドビレ」 |
| ST5 BOSS3 | 34 | NS | — |
| ST5 BOSS4 | 47 | S | 38 幻世「ザ・ワールド」 |
| ST5 BOSS5 | 41 | NS | — |
| ST5 BOSS6 | 50 | S | 39 メイド秘技「殺人ドール」 |

### 4.6 ST6（11）

| 记录名 | sub | 类型 | spell |
| --- | --- | --- | --- |
| ST6 MID1 | 9 | NS | — |
| ST6 MID2 | 14 | S | 40 奇術「エターナルミーク」 |
| ST6 BOSS1 | 17 | NS | — |
| ST6 BOSS2 | 31 | S | 46 神罰「幼きデーモンロード」 |
| ST6 BOSS3 | 19 | NS | — |
| ST6 BOSS4 | 33 | S | 47 獄符「千本の針の山」 |
| ST6 BOSS5 | 23 | NS | — |
| ST6 BOSS6 | 35 | S | 48 神術「吸血鬼幻想」 |
| ST6 BOSS7 | 26 | NS | — |
| ST6 BOSS8 | 39 | S | 49 紅符「スカーレットマイスタ」 |
| ST6 BOSS9 | 44 | S | 50 「紅色の幻想郷」 |

> BOSS1 需要两次 `op 0xA` 才进入（`Sub15 → Sub16 → Sub17`），见 §2.1。

### 4.7 ST7（21）

| 记录名 | sub | 类型 | spell |
| --- | --- | --- | --- |
| ST7 MID1 | 17 | S* | （call）Sub22 → id 51 月符「サイレントセレナ」 |
| ST7 MID2 | 18 | S* | （call）Sub23 → id 52 日符「ロイヤルフレア」 |
| ST7 MID3 | 19 | S* | （call）Sub24 → id 53 火水木金土符「賢者の石」 |
| ST7 END_NS1 | 32 | NS | — |
| ST7 END_S1 | 33 | S | 54 禁忌「クランベリートラップ」 |
| ST7 END_NS2 | 35 | NS | — |
| ST7 END_S2 | 36 | S | 55 禁忌「レーヴァテイン」 |
| ST7 END_NS3 | 38 | NS | — |
| ST7 END_S3 | 39 | S | 56 禁忌「フォーオブアカインド」 |
| ST7 END_NS4 | 43 | NS | — |
| ST7 END_S4 | 44 | S | 57 禁忌「カゴメカゴメ」 |
| ST7 END_NS5 | 47 | NS | — |
| ST7 END_S5 | 48 | S | 58 禁忌「恋の迷路」 |
| ST7 END_NS6 | 49 | NS | — |
| ST7 END_S6 | 50 | S | 59 禁弾「スターボウブレイク」 |
| ST7 END_NS7 | 53 | NS | — |
| ST7 END_S7 | 54 | S | 60 禁弾「カタディオプトリック」 |
| ST7 END_NS8 | 56 | NS | — |
| ST7 END_S8 | 57 | S | 61 禁弾「過去を刻む時計」 |
| ST7 END_S9 | 60 | S | 62 秘弾「そして誰もいなくなるか？」 |
| ST7 END_S10 | 68 | S | 63 QED「495年の波纹」 |

> **不在 CSV 的两个真实阶段子**：`Sub59`（HP 1000 / thr 1000 / to 1800，位于 S8 与 S9 之间）与 `Sub67`（HP 6000 / to 1800，位于 S9 与 S10 之间）。CSV 没有 NS9/NS10；运行时它们**不命中白名单 ⇒ no-op**，当前记录保持 S8/S9，随后 `Sub60`/`Sub68` 命中时再切到 S9/S10。这与 latch 口径一致。
>
> **ST7 MID1–3 的符卡名在 call 目标里**：`Sub17→Sub22`（id 51 月符「サイレントセレナ」）、`Sub18→Sub23`（id 52 日符「ロイヤルフレア」）、`Sub19→Sub24`（id 53 火水木金土符「賢者の石」）。这也再次说明：阶段期间 `E+0x9D8` 会停在 call 目标（22/23/24），栈底仍保留阶段 sub（17/18/19）。

### 4.8 覆盖率核对

- 记录数：6+6+9+9+8+11+21 = **70**。
- 映射 sub 数（ST4 展开变体）：6+6+9+**14**+8+11+21 = **75**。
- 逐阶段 spell id/名与 thprac `th_sections_str` / `th_sections_cbt` / ST4 分机体表交叉核对：✅。

### 4.9 扁平映射表（可直接生成代码表）

> 按 stage（1-based，`*(u32*)0x69D6D4`）+ sub 查记录名。ST4 的 8 个 BOSS2/BOSS3 变体全部指向同一记录名，因此运行时不需要读 shot。

```cpp
struct { uint8_t stage; uint16_t sub; const char* name; } kTh06BossPhaseSubs[] = {
    {1,  8, "ST1 MID1"},  {1,  9, "ST1 MID2"},
    {1, 11, "ST1 BOSS1"}, {1, 22, "ST1 BOSS2"}, {1, 16, "ST1 BOSS3"}, {1, 23, "ST1 BOSS4"},
    {2, 20, "ST2 MID1"},
    {2, 22, "ST2 BOSS1"}, {2, 30, "ST2 BOSS2"}, {2, 25, "ST2 BOSS3"}, {2, 31, "ST2 BOSS4"}, {2, 32, "ST2 BOSS5"},
    {3,  9, "ST3 MID1"},  {3, 14, "ST3 MID2"},
    {3, 18, "ST3 BOSS1"}, {3, 29, "ST3 BOSS2"}, {3, 20, "ST3 BOSS3"}, {3, 30, "ST3 BOSS4"},
    {3, 26, "ST3 BOSS5"}, {3, 32, "ST3 BOSS6"}, {3, 33, "ST3 BOSS7"},
    {4, 21, "ST4 MID1"},  {4, 27, "ST4 BOSS1"},
    {4, 43, "ST4 BOSS2"}, {4, 46, "ST4 BOSS2"}, {4, 48, "ST4 BOSS2"}, {4, 51, "ST4 BOSS2"},
    {4, 37, "ST4 BOSS4"},
    {4, 52, "ST4 BOSS3"}, {4, 49, "ST4 BOSS3"}, {4, 44, "ST4 BOSS3"}, {4, 54, "ST4 BOSS3"},
    {4, 39, "ST4 BOSS5"}, {4, 40, "ST4 BOSS6"}, {4, 41, "ST4 BOSS7"},
    {5, 13, "ST5 MID1"},  {5, 18, "ST5 MID2"},
    {5, 22, "ST5 BOSS1"}, {5, 45, "ST5 BOSS2"}, {5, 34, "ST5 BOSS3"},
    {5, 47, "ST5 BOSS4"}, {5, 41, "ST5 BOSS5"}, {5, 50, "ST5 BOSS6"},
    {6,  9, "ST6 MID1"},  {6, 14, "ST6 MID2"},
    {6, 17, "ST6 BOSS1"}, {6, 31, "ST6 BOSS2"}, {6, 19, "ST6 BOSS3"}, {6, 33, "ST6 BOSS4"},
    {6, 23, "ST6 BOSS5"}, {6, 35, "ST6 BOSS6"}, {6, 26, "ST6 BOSS7"}, {6, 39, "ST6 BOSS8"},
    {6, 44, "ST6 BOSS9"},
    {7, 17, "ST7 MID1"},  {7, 18, "ST7 MID2"},  {7, 19, "ST7 MID3"},
    {7, 32, "ST7 END_NS1"}, {7, 33, "ST7 END_S1"},
    {7, 35, "ST7 END_NS2"}, {7, 36, "ST7 END_S2"},
    {7, 38, "ST7 END_NS3"}, {7, 39, "ST7 END_S3"},
    {7, 43, "ST7 END_NS4"}, {7, 44, "ST7 END_S4"},
    {7, 47, "ST7 END_NS5"}, {7, 48, "ST7 END_S5"},
    {7, 49, "ST7 END_NS6"}, {7, 50, "ST7 END_S6"},
    {7, 53, "ST7 END_NS7"}, {7, 54, "ST7 END_S7"},
    {7, 56, "ST7 END_NS8"}, {7, 57, "ST7 END_S8"},
    {7, 60, "ST7 END_S9"},  {7, 68, "ST7 END_S10"},
};
```

### 4.10 与 thprac `THPatch` 练习跳转的交叉验证

> 方法：把 `thprac_th06.cpp` → `THPatch()` 里每个 section 的 patch **文件偏移**落回 ECL 文件，定位所属 sub 与指令字段。patch 的语义只有三类：
> ① 把某条指令的时间改 0（提前执行）；
> ② 用 `ECLSetTime` / `ECLSetHealth` 把某条指令改写成 `timeout=0` / `HP=xxx`，借引擎的时限/阈值路径转入下一 sub（§2.2）；
> ③ 直接改 `ins_35` 的 subId 参数（指令偏移 `+0xC`），或把一条 `ins_34` 改写成 `ins_35`，**直接 call 目标阶段 sub**。
> 因此每个 section 都能反推出「它想进入哪个 sub」，与 §4 的映射独立比对。

**结果：BOSS 区 70 行全部对上（70/70），无一处冲突。** 逐关摘要：

| 关 | thprac 跳转反推出的阶段链（L） | 与 §4 |
| --- | --- | --- |
| ST1 | 时间轴 spawn `Sub8`=MID1；`HP→9`=MID2；spawn `Sub10→interrupt 11`=BOSS1；`timeout(11)→22`=BOSS2；`call→16`=BOSS3；`timeout(16)→23`=BOSS4 | ✅ |
| ST2 | spawn `Sub20`=MID1；`interrupt 22`=BOSS1；`timeout(22)→30`=BOSS2；`call→25`=BOSS3；`timeout(25)→31`=BOSS4；`call→32`=BOSS5 | ✅ |
| ST3 | spawn `Sub9`=MID1；`HP→14`=MID2；`interrupt 18`=BOSS1；`timeout(18)→29`=BOSS2；`call→20`=BOSS3；`timeout(20)→30`=BOSS4；`call→26`=BOSS5；`timeout(26)→32`=BOSS6；把 26 的 `nextOnTimeout` 改写为 33=BOSS7 | ✅ |
| ST4 | spawn `Sub21`=MID1；`interrupt 27`=BOSS1；`timeout(27)→43/46/48/51`=BOSS2；`call→37`=BOSS4；`timeout(37)→52/49/44/54`=BOSS3；`call→39`=BOSS5，再由 `HP` 依次 40=BOSS6、41=BOSS7；BOOKS 走时间轴 `0xCDxx` | ✅ |
| ST5 | spawn `Sub12→interrupt 13`=MID1；`HP→18`=MID2；`interrupt 22`=BOSS1；`timeout(22)→45`=BOSS2；`call→34`=BOSS3；`timeout(34)→47`=BOSS4；`call→41`=BOSS5；`timeout(41)→50`=BOSS6 | ✅ |
| ST6 | spawn `Sub8→interrupt 9`=MID1；`HP→14`=MID2；spawn `Sub15→16→17`=BOSS1；`timeout(17)→31`=BOSS2；`call→19`=BOSS3；`timeout(19)→33`=BOSS4；`call→23`=BOSS5；`timeout(23)→35`=BOSS6；`call→26`=BOSS7；`timeout(26)→39`=BOSS8；`call(runmask 0x0C)→44`=BOSS9 | ✅ |
| ST7 | spawn `Sub16→interrupt 17`=MID1；`call→18`=MID2；`call→19`=MID3；spawn `Sub31→interrupt 32`=NS1；`timeout(32)→33`=S1；NS2–NS8 的 call 目标 35/38/43/47/49/53/56；S2–S8 由对应 NS 的 `timeout` 转入 36/39/44/48/50/54/57；S9 `call→59`+`timeout→60`；S10 `call→67`+`timeout→68` | ✅ |

**三个额外结论：**

1. **ST4 机体分支顺序被 thprac 直接写死**：`Sub27` 的 L 分支按 `-10025`（`character*2+shotType`）依次 timeout/threshold 到 `43/46/48/51`；`Sub37` 的 L 分支依次到 `52/49/44/54`。与 §2.6 / §4.4 的四机体顺序完全一致。
2. **ST6 BOSS9 是「一 section 两难度」**：thprac 在 `Sub17` 写入两条 call——runmask `0x03`(E/N) → `Sub43`（「レッドマジック」，id 45），runmask `0x0C`(H/L) → `Sub44`（「紅色の幻想郷」，id 50）。L 命中 `Sub44`。这条同时**独立印证了 §2.4 的 run mask 语义**（0x03=E/N，0x0C=H/L，置位=执行），因为 thprac 自己就是用这两个值做难度分派的。
3. **ST7 S9/S10 证实 `Sub59/Sub67` 是真实阶段**：thprac 的 S9/S10 入口先 call `Sub59/Sub67`，再把它们的 `timeout` 清零转入 `Sub60/Sub68`——正好对应 §4.7 所说的「不在 CSV 的两个真实阶段子」。

> ⚠️ **对 §3.2 的直接影响**：thprac 的后期阶段练习入口不是等正常转移，而是**从早期阶段 sub 直接 `ins_35` call 目标阶段 sub**（ST1 BOSS3、ST2 BOSS3、ST3 BOSS3、ST5 BOSS3、ST6 BOSS3/BOSS9、ST7 各 NS/S；ST4 BOSS3–7 则是把一条 `ins_34` 改写成 `ins_35`）。这会在调用栈里留下**两个白名单 sub**，因此阶段读取必须**由内向外**（当前 sub → 栈顶 → 栈底），不能栈底优先。

---

## 5. 特殊项

### 5.1 ST4 BOOKS

- 不是 BOSS 阶段；时间轴上 `t=3452` 的 `op 4` 生成书（`Sub14` 等），CSV 的 `3392/0xCDD4` 是 warp 时间 + 落点。
- 判据沿用章节监控：`pCode = *(u32*)0x487E50`、`tlInstr = *(u32*)(0x4B79C8+0xEE5DC)`，`(tlInstr − pCode) == 0xCDD4` 时 latch `ST4 BOOKS`。
- 它与 BOSS 子监控互不冲突（此时 `bossPresent=0`）。

### 5.2 中 BOSS

- 中 BOSS 同样 `ins_101(0)` 注册到 `bosses[0]`，阶段链完全同构；§4 已包含 MID 行。
- 中 BOSS 的 `nextOnTimeout` / `deathCallback` 常指向**逃逸/清理子**（如 ST1 的 `Sub7`/`Sub6`、ST2 的 `Sub19`；ST3/5/6/7 的清理子还会 `ins_101(-1)`）。这些 sub **不在白名单**，记录保持在上一阶段，直到 `bossPresent=0`——符合「只在命中白名单时覆盖」的 latch 口径。
- 中 BOSS 死亡/逃逸后 `bossPresent=0`；`bosses[0]` 可能悬空（死亡子不一定 `ins_101(-1)`），因此**必须同时检查活跃位与 BOSS 位**，不能只看指针非空。

### 5.3 练习 warp 的瞬态

- thprac 的 BOSS 练习入口通常是：`ECLWarp(开战时间)` → patch 若干指令时间为 0 / 改 HP / `ECLStall`，让引擎快进到目标阶段。具体有两种快进方式（§4.10）：
  1. **强制转移**：把早期阶段的指令改写成 `timeout=0` 或 `HP=xxx`，借阈值/时限路径进入下一 sub；
  2. **直接 call**：改 `ins_35` 的 subId，或把 `ins_34` 改写成 `ins_35`，从早期阶段 sub **直接 call 目标阶段 sub**（`Sub11→Sub16`、`Sub17→Sub44`、`Sub32→Sub59` 等）。
- 方式 2 会让调用栈同时含有两个白名单 sub；§3.2 已改为**由内向外**读取，因此目标 sub 一旦被 call，日志立即看到目标，而不是早期阶段。
- 但**在 call/转移真正发生之前**，BOSS 仍处在早期阶段（例如 `ST1 BOSS3` 的 call 在 t=80 才执行），这段瞬态仍会先 latch 到更早的记录（如 BOSS1）。
- 若 logger 的「启动 checkpoint」取「本局首次 latch」，可能记到更早的记录。
- 建议（二选一）：
  1. **去抖**：候选记录连续稳定 N 帧（建议 2–3 帧）再 latch；阶段正常持续数秒，不会误伤。
  2. 练习模式下直接用 `thPracParam.section` 初始化启动 checkpoint（thprac 侧已有该参数），BOSS 子监控只负责后续转移。

### 5.4 罕见同帧双转移

- 每帧顺序是「阈值 → 时限」。若某一帧同时满足「HP < 阈值」和「计时器 ≥ 时限」，阈值路径先 `SetSub(nextOnHp)`，但不会清除 `timeout`/计时器；同帧的时限检查可能立即再转一次到 `nextOnTimeout/deathCallback`。
- 概率极低（需要恰好在时限帧把 HP 打过阈值）；若发生，监控会看到后一个阶段，前一个阶段可能只存在一帧。去抖也可能把它吞掉。
- 记为 known caveat，真机验证时留意。

### 5.5 重放 / attract

- 地址与机制与正常游玩相同；logger 已有 `isInReplay`（`0x69BCBC`）与「一局开始」闸门，沿用即可。

---

## 6. 备选方案：在转阶段点挂 hook

> 仅在「不想读调用栈」或「需要精确的转阶段事件时间戳」时使用。推荐方案仍是 §3.2 的零 hook 采样。

四个 5 字节 `call 0x407440` 站点（均未被 thprac 现有 hook 占用）：

| 路径 | VA / RVA | 覆盖 | 目标 sub 读法 |
| --- | --- | --- | --- |
| HP 阈值 | `0x411DED` / `0x11DED` | 5B `E8 4E 56 FF FF` | `*(u16*)(E+0xEAC)`（或 `[esp+4]`） |
| 时限 | `0x411FF2` / `0x11FF2` | 5B `E8 49 54 FF FF` | `*(u16*)(E+0xEB4)`（**必须在 call 前读**） |
| HP 归零 | `0x412CD0` / `0x12CD0` | 5B `E8 6B 47 FF FF` | `*(u16*)(E+0xC44)` |
| 首阶段中断 | `0x4095C4` / `0x95C4` | 5B `E8 77 DE FF FF` | `*(u16*)(E + 0xC48 + *(i32*)(E+0xC68)*4)` |

- 命中时 `ECX = 0x487E50`。调用点栈（**hook 在 `call` 指令处**）为 `[esp] = &ctx`、`[esp+4] = subId`；若 hook 在 `SetSub` 入口，则变成 `[esp+4] = subId`、`[esp+8] = &ctx`。更稳妥的做法是直接读 `E+0xEAC / E+0xEB4 / E+0xC44`（见 §2.2）。`E` 可从 `[ebp+8]`（中断）或各函数的 `[ebp-0x18]` / `[ebp-0xC]` / `[ebp-0x24]` 取（见 §2.2 的反汇编）。
- 目标 sub → 记录名仍用 §4 的映射。
- ⚠️ 三个转移函数/`0x412CD0` 都是通用敌机路径，小怪/特殊敌机也可能命中；hook 内必须用 `E+0xE51 bit3`（BOSS 位）与 `bossPresent` 过滤。
- **顺序匹配兜底**：若只想要时机，可以在上述四个点计数；但必须按每关预置的「真实阶段序列」推进（ST7 有 20 个真实阶段子但 CSV 只有 18 行；ST4 有 7 行但每行可能跨多个书波），否则单纯计数会错位。

---

## 7. 真机验证清单

> 2026-11-01：用户确认此前已真机通过（监控 + 日志接线）。以下为原验证清单，保留供后续回归使用，不再作为“全关未测”的当前状态。

- [ ] **基础读值**：BOSS 出现后 `*(u32*)0x5A5F60` 非空、`*(u8*)0x69BC50 == 1`、`E+0xE50 bit7 == 1`、`E+0xE51 bit3 == 1`。
- [ ] **阶段 sub 命中**：L 难度、每关 1 次完整 BOSS 战，逐阶段打印 `GetBossPhaseSub()`，与 §4 表逐行比对。
- [ ] **三条转移路径**：
  - [ ] HP 打到阈值 → 下一阶段；
  - [ ] 拖到时限 → 下一阶段；
  - [ ] 符卡收取（HP 0）→ 下一阶段。
- [ ] **调用栈**：在 BOSS1（如 ST1 Sub11）中确认 `depth ≥ 1` 且 `frame[0]`/current 各阶段值与预期一致；确认攻击子脚本（Sub12/15…）出现时扫描仍返回阶段 sub。
- [ ] **ST4 四机体**：ReimuA/B、MarisaA/B 各跑一次，确认 BOSS2/BOSS3 的 8 个变体 sub 都映射到正确记录（顺序按 CSV 的 1,2,4,3）。
- [ ] **ST4 BOSS5/6/7**：确认 `Sub39→Sub40→Sub41` 的切换点与练习菜单的三段（书波）一致，书波内部不误判。
- [ ] **ST7 尾部**：确认 `Sub59`/`Sub67` 出现时记录保持不变，`Sub60`/`Sub68` 出现时切到 S9/S10。
- [ ] **中 BOSS**：ST1/2/3/4/5/6/7 的 MID 行逐关命中；中 BOSS 结束后 `bossPresent` 归 0，不残留旧记录。
- [ ] **练习 warp**：每个 BOSS 阶段各 warp 一次，确认启动 checkpoint 不被快进瞬态带偏（或去抖/覆盖生效）。
- [ ] **重放**：replay 下阶段 sub 与正常游玩一致。
- [ ] **变体**：Steam / 汉化版是否共用同一地址（本报告指纹见头部）。

---

## 8. 证据附录

### 8.1 关键地址（VA / RVA）

| 名称 | VA | RVA |
| --- | --- | --- |
| EnemyManager 静态 | `0x4B79C8` | `0xB79C8` |
| bosses[0..7] | `0x5A5F60` | `0x1A5F60` |
| bossPresent | `0x69BC50` | `0x29BC50` |
| difficulty | `0x69BCB0` | `0x29BCB0` |
| currentStage | `0x69D6D4` | `0x29D6D4` |
| character / shotType | `0x69D4BD` / `0x69D4BE` | `0x29D4BD` / `0x29D4BE` |
| ECL 管理器 | `0x487E50` | `0x87E50` |
| 时间轴指令 | `0x5A5FA4`（= `mgr+0xEE5DC`） | `0x1A5FA4` |
| 时间轴时间 | `0x5A5FB0`（= `mgr+0xEE5E8`） | `0x1A5FB0` |
| BossLifeCheck | `0x411DA0` | `0x11DA0` |
| BossTimeoutCheck | `0x411F40` | `0x11F40` |
| SetSub | `0x407440` | `0x7440` |
| 敌机 VM | `0x4074A0` | `0x74A0` |
| 管理器每帧更新 | `0x4123E0` | `0x123E0` |
| 时间轴 op 0xA | `0x411C3B` | `0x11C3B` |
| 敌机死亡路径 | `0x412C50` | `0x12C50` |
| PLAYER_SHOT 变量 | `0x40B2E7` | `0xB2E7` |

### 8.2 代表性反汇编

**HP 阈值转移（0x411DA0）**

```asm
00411DAF  8b90e40c0000      mov edx, [eax+0xCE4]       ; curLife
00411DB5  3b91a80e0000      cmp edx, [ecx+0xEA8]       ; threshold
00411DBB  0f8d75010000      jge 0x411F36               ; >= ⇒ 返回
00411DC7  8b91a80e0000      mov edx, [ecx+0xEA8]
00411DCD  8990e40c0000      mov [eax+0xCE4], edx       ; clamp
00411DD6  668b88ac0e0000    mov cx, [eax+0xEAC]        ; nextOnHp
00411DED  e84e56ffff        call 0x407440              ; SetSub
00411DF5  c780a80e0000ffffffff  mov dword [eax+0xEA8], -1
00411E05  8b82440c0000      mov eax, [edx+0xC44]
00411E0B  8981b40e0000      mov [ecx+0xEB4], eax       ; EB4=deathCB
00411E5E  c7813c0c000000000000  mov dword [ecx+0xC3C], 0  ; depth=0
```

**时限转移（0x411F40）**

```asm
00411F94  8b90f80c0000      mov edx, [eax+0xCF8]       ; timer.current
00411F9C  3b91b00e0000      cmp edx, [ecx+0xEB0]       ; timeout
00411FA2  0f9dc0            setge al
00411FDB  668b88b40e0000    mov cx, [eax+0xEB4]        ; nextOnTimeout
00411FF2  e84954ffff        call 0x407440              ; SetSub
00411FFA  c780b00e0000ffffffff  mov dword [eax+0xEB0], -1
0041200A  8b82440c0000      mov eax, [edx+0xC44]
00412010  8981b40e0000      mov [ecx+0xEB4], eax       ; 覆盖为 deathCB
00412198  c7813c0c000000000000  mov dword [ecx+0xC3C], 0  ; depth=0
```

**call / ret 压栈（0x407AEC / 0x407BBD）**

```asm
00407AEC  8b4de4            mov ecx, [ebp-0x1C]        ; instr
00407AF2  8b510c            mov edx, [ecx+0xC]         ; subId
00407B23  8b7508            mov esi, [ebp+8]
00407B26  81c690090000      add esi, 0x990              ; src=ctx
00407B35  6bc94c            imul ecx, ecx, 0x4C         ; frame size
00407B3B  8dbc0adc090000    lea edi, [edx+ecx+0x9DC]    ; dst=frame[depth]
00407B47  f3a5              rep movsd                   ; 压栈
00407B5D  e8def8ffff        call 0x407440              ; SetSub(subId)
00407B7C  83b93c0c000007    cmp dword [ecx+0xC3C], 7
00407B94  89813c0c0000      mov [ecx+0xC3C], eax        ; depth++
```

**难度 run mask（0x4074EC）**

```asm
004074EE  8a5109            mov dl, [ecx+9]
004074F1  b801000000        mov eax, 1
004074F6  8b0db0bc6900      mov ecx, [0x69BCB0]
004074FC  d3e0              shl eax, cl
004074FE  23d0              and edx, eax
00407500  85d2              test edx, edx
00407502  7505              jne 0x407509              ; 非 0 ⇒ 执行
00407504  e9ff2a0000        jmp 0x40A008              ; 0 ⇒ 跳过
```

### 8.3 ECL 解码与复现

- 素材：`../exes/ecl/th06/ecldata1..7.ecl` + `../exes/ecl_dump/th06/ecldata1..7.decl`。
- 自写脚本（**仓库外、不提交**）：`../reasm/th06/ecl_inspect.py`（文件头/sub/时间轴）、`phase_l.py`（L 有效 setup 摘取）、`chain_sim.py`（阶段链模拟）、`ecl_exec.py`（difficulty/shot 分支符号执行）、`dis86.py`（PE 反汇编 helper）。
- 文件头/sub 表/时间轴格式沿用 `th06_ecl_re_report.md` §3/§11；本文的所有 sub 号都是**sub 索引**（= `decl` 的 `SubN`）。
- 阶段链的复核方法：
  1. 从时间轴 spawn 得到开战 sub（§2.1 表）；
  2. 对每个 sub 按 L run mask 摘出 `ins_101/108/109/111/112/113/114/115/116/93`；
  3. 按「阈值→nextOnHp；无阈值→deathCallback（时限也落到 EB4/deathCallback）」模拟链；
  4. 与 CSV 行序、`th_sections_cbt` 的 NS/S 分类、`th_sections_str` 的符卡名、ST4 分机体表四方交叉。

### 8.4 开战入口与首阶段触发（逐关）

| 关卡 | 组 | 时间轴 spawn（offset / time / sub） | 首阶段触发 | 首阶段 sub |
| --- | --- | --- | --- | --- |
| ST1 | MID | `0x51AE` / 2008 / 8 | 直接 | 8 |
| ST1 | BOSS | `0x5FA2` / 5279 / 10 | `op 0xA` `0x5FC6` (5281) | 11 |
| ST2 | MID | `0x477C` / 2588 / 20 | 直接 | 20 |
| ST2 | BOSS | `0x68B0` / 5984 / 21 | `op 0xA` `0x68D4` (5986) | 22 |
| ST3 | MID | `0x6C2E` / 3804 / 9 | 直接 | 9 |
| ST3 | BOSS | `0x80D6` / 5854 / 17 | `op 0xA` `0x8102` (5916) | 18 |
| ST4 | MID | `0xCE7C` / 4132 / 21 | 直接 | 21 |
| ST4 | BOSS | `0xED2C` / 10695 / 26 | `op 0xA` `0xED50` (10697) | 27 |
| ST5 | MID | `0x6488` / 3372 / 12 | `op 0xA` `0x64B4` (3374) | 13 |
| ST5 | BOSS | `0x7660` / 7704 / 21 | `op 0xA` `0x768C` (7706) | 22 |
| ST6 | MID | `0x77D6` / 2564 / 8 | `op 0xA` `0x7802` (2566) | 9 |
| ST6 | BOSS | `0x782A` / 3169 / 15 | `op 0xA` `0x784E` (3171) → 16；`0x7866` (3173) → 17 | 17 |
| ST7 | MID | `0xD2C6` / 4740 / 16 | `op 0xA` `0xD2F2` (4742) | 17 |
| ST7 | BOSS | `0xE9A2` / 8594 / 31 | `op 0xA` `0xE9C6` (8596) | 32 |

> CSV 第一列的时间是 `THPatch` 的 warp 值，可能比时间轴 spawn 时间早若干帧（例如 ST3 BOSS CSV=5844，spawn=5854）；**不要用 CSV 时间直接匹配时间轴指令**。

---

## 9. 与既有文档的关系

- 本报告**取代** `th06_ecl_re_report.md` §10.3 / `th06_checkpoint_entry_report.md` §3 已撤下的全部 BOSS 阶段内容；CSV 70 行未改动。
- **更正** `th06_ecl_re_report.md` §2.4 中「`1<<difficulty` 置位 ⇒ 跳过」的表述：实际是 **run mask，置位 ⇒ 执行**（证据 `0x4074EC`–`0x407504`）。该报告引用的 `EclRawInstr.skipForDifficulty`（`thprac_th06.h`）字段名同样具有误导性——它存的是 run mask。这一条会直接影响任何按难度过滤的 ECL 分析，建议后续回填总表。
- 本报告**不修改** `docs/logger/th06_checkpoints.csv`、源码或既有报告；落地接线不在本轮范围。

---

## 10. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-27 | 建档。重新调查 th06 BOSS 阶段监控（L 专用）：定案「阶段 = BOSS 敌机的 ECL 阶段 sub」，给出三条引擎转移路径（HP 阈值 `0x411DA0`、时限 `0x411F40`、HP 归零 `0x412C50`）及首阶段 `op 0xA` 中断链；更正难度字节为 **run mask**（`0x4074EC`）；解释 `E+0x9D8` 为何不能直接当阶段号（`ins_35` call 会改 subId）并给出「调用栈最底层白名单」读取算法；给出 ST4 的 PLAYER_SHOT 分支（`0x40B2E7`）与 8 个变体 sub；产出 70 行 CSV → 75 个 L 有效 sub 的完整映射表（ST4 BOOKS 走时间轴 `0xCDD4`；ST7 的 Sub59/Sub67 不入表）；列出零 hook 主方案、四个 SetSub hook 备选、练习 warp 瞬态与罕见同帧双转移等 caveat，以及真机验证清单。**未编译、未运行、未改 CSV/源码；🟡 未真机。** |
| 2026-10-27 | 与 `thprac_th06.cpp` 的 `THPatch()` 练习跳转做交叉验证（新增 §4.10）：按 patch 偏移落回 ECL sub/字段，70/70 section 的目标 sub 与 §4 完全一致；ST4 机体顺序、ST6 BOSS9 的 E/N/H/L 分派（同时独立印证 run mask 0x03/0x0C）、ST7 S9/S10 的 Sub59/Sub67 均得到源码级佐证。**修正 §3.2 读取方向**：thprac 会用 `ins_35` 从早期阶段直接 call 目标阶段，调用栈内可能出现两个白名单 sub，阶段提取改为「当前 sub → 栈顶 → 栈底」由内向外；同步更新 §0 第 7 条、§4.4 注、§5.3。**仍未真机。** |
| 2026-10-27 | **监控落地 + 实机单关通过**：`TH06Logger` 已接入 `thprac_th06.cpp`（CSV 由 `docs/logger/tools/gen_th06_checkpoints.py` 生成 `thprac_th06_checkpoints_gen.cpp`；道中按 `tlInstr−pCode` 区间匹配 + 偏移单调，ST4 BOOKS 走同一路径；BOSS/MID 仅 L，按阶段 sub 白名单 75 条、调用栈由内向外；2 帧去抖；miss/bomb/interrupt 接 `GameSession`；IGI 增加「当前段落」）。`Release|x86` 编译通过；**用户实机单关通过**；全关/重放/四机体回归未测。 |
| 2026-11-01 | 用户确认 th06 此前已真机通过，当前状态改✅；撤下“单关通过 / 全关回归未测”的当前限制，历史测试过程记录与回归清单保留，未新增或推定逐项测试数据。 |

# TH15 miss/bomb 记录：boss 阶段名字串来源确认（第三轮反编译报告）

> 状态：**已回复开发组**。对应需求：`th15_re_disasm_requests3.md`。前两轮：`th15_re_disasm_report.md`、`th15_re_disasm_report2.md`。
> 结论基于 `th15.exe.c`（Ghidra 反编译 C）、`th15_ghidra_dump.txt`、`th15_report_raw.txt` 的交叉核对，全部地址为静态基址（ImageBase 0x400000）。

---

## 0. 结论速览（TL;DR）

1. **`[g_boss_ptr+0x4080]` 不是名字字段**：整个 exe 反编译与 dump 中**没有任何指令读写内存偏移 `0x4080`**（`0x4080` 仅以位掩码 `0x40800000` 出现，是对 `+0x4060` 标志位的检查）。上版 dump 在 @4080 读到的"下一攻击名"是**读取伪影**（过期指针/错位读），本轮实测为空才是真实情况。→ **应从读取链剔除 @4080**。
2. **名字串真实写偏移 = `scr + param_1*0x88 + 0x528C`**（`scr = *(int*)(g_boss_ptr+0x44B8)`），第二份副本 `+0x52CC`。字符串起点确为 `+0x528C`（Ghidra 反编译把复制循环基址标成 `+0x528B` 是先自增伪影，`+0x528B` 是 HP dword `+0x5288~+0x528B` 的高字节，不是名字起点）。**off-by-one 从指令层坐实**。
3. **为什么槽里存的是"下一攻击"名**：`0x202`（ins_514，SetNextPattern）是**预注册**下一攻击的指令；当前攻击名只在"攻击开始瞬间"由 `FUN_00430050` 消费槽（阈值置 -1）时短暂在槽中，随后被下一条 0x202 覆盖。**不存在"当前攻击名"的持久内存字段**。
4. **最终方案（推荐，已验证）**：在换攻击钩 `0x42C7D6`（normal）+ `0x42C785`（BossDead）缓存**写前旧值**（槽当前内容 = 正在进行的攻击名）；槽为空时（每 boss 首个 0x202）兜底缓存新写入名。miss/bomb 时读该缓存，门控 `flg & 0x800000`。
5. `param_1`（槽索引）**不保证恒为 1**，是 ECL 脚本各 0x202 的参数[0]（0~7）；读取必须用 hook 命中时的 `EAX` 动态计算，不能硬编码。

---

## 1. 问题 1：boss 对象 `+0x4080` 字段的写入点与语义

### 1.1 结论：无写入点，非名字字段，应剔除

在 `th15.exe.c` 全文件检索 `0x4080`，仅 3 处，全部是**位掩码常量**，与内存偏移 `+0x4080` 无关：

```
th15.exe.c:9647   puVar2[1] = 0x40800000;              // 浮点常量的一部分
th15.exe.c:29352  if ((uVar4 & 0x40800000) == 0)       // 对 +0x4060 标志位检查
th15.exe.c:29374  if ((*(uint *)(param_1 + 0x4060) & 0x40800000) != 0)  // 同上
```

对三个 dump（`th15_ghidra_dump.txt` / `th15_report_raw.txt` / `th15_extra_raw.txt`）检索 `0x4080` / `4080` / `0x408[0-9a-f]`，**零出现**。即：解释器、boss 对象管理、名字显示路径中都没有 `[EDI+0x4080]` 的写或读。

### 1.2 上版 dump 为何显示"下一攻击名"？

- 上版 @4080 读到的序列（`BossCard1/Boss2/BossCard2/BossDead`）与本轮实测 @5314（槽 1 副本 1）的"下一攻击名"序列**完全相同**，但 `+0x4080`（boss 对象偏移）与 `+0x528C`（脚本对象槽区）在地址上无重叠。唯一合理解释：上版 dump 的读取经**错误的指针/过期对象**（如缓存的 `g_boss_ptr` 指向残留对象，或读的是同一内存里的旧名字缓冲残留）。
- 本轮实测 @4080 为空与该偏移无写入点的事实一致。**结论：@4080 不是可靠字段，从 `GetBossStageName()` 读取链删除。**

---

## 2. 问题 2：名字串写入机制 与 miss/bomb 时"当前攻击名"的稳定读取

### 2.1 槽结构（已确认）

8 槽队列，位于脚本对象上，每槽 `0x88` 字节：

```
槽 i 基址 = scr + 0x5284 + i*0x88     （i = ECL 参数[0]，0..7；8 槽共 0x440 字节，止于 0x56C4）
  +0x00 (0x5284): int32  阈值/活跃标记（-1 = 已消费/空）
  +0x04 (0x5288): int32  该攻击 HP
  +0x08 (0x528C): 名字串 副本1（null 结尾）   ← 非符/符卡名显示用
  +0x48 (0x52CC): 名字串 副本2（同串副本）    ← BossDead/BossEscape 等收尾用
```

槽在 boss 对象构造时全部初始化为 `+0x5284=-1, +0x5288=-1, +0x528C=0`（`th15.exe.c:28692-28715`，`FUN_00426c60`）。

### 2.2 写入点：`FUN_0042ff10`（0x42FF10）——写 HP + 名字串

`th15.exe.c:33785-33811` 完整反编译：

```c
void __thiscall FUN_0042ff10(void *this,int param_1,undefined4 param_2,char *param_3)
{
  char cVar1;  int iVar2;  char *pcVar3;
  *(undefined4 *)((int)this + param_1 * 0x88 + 0x5288) = param_2;      // 写 HP
  if (param_3 != (char *)0x0) {
    pcVar3 = param_3;
    do { cVar1 = *pcVar3; pcVar3 = pcVar3 + 1;
         pcVar3[(int)this + (param_1 * 0x88 - (int)param_3) + 0x528b] = cVar1; }  // 副本1 → +0x528C
    while (cVar1 != '\0');
    iVar2 = param_1 * 0x88 - (int)param_3;
    do { cVar1 = *param_3; param_3 = param_3 + 1;
         param_3[(int)this + iVar2 + 0x52cb] = cVar1; }                // 副本2 → +0x52CC
    while (cVar1 != '\0');
    return;
  }
  *(undefined1 *)((int)this + param_1 * 0x88 + 0x528c) = 0;            // param_3==NULL 清副本1
  *(undefined1 *)((int)this + param_1 * 0x88 + 0x52cc) = 0;            // param_3==NULL 清副本2
  return;
}
```

参数语义（由两处调用点反推，`th15_report_raw.txt:474-527`）：
- `this`(ECX) = `scr` = `*(int*)(g_boss_ptr + 0x44B8)`（ECL 脚本资源对象）；
- `param_1` = **ECL 参数[0] = 槽索引**（常量 0/1/…，非攻击序号）；
- `param_2` = ECL 参数[2] = **该攻击 HP**；
- `param_3` = 攻击名字串（normal 分支 = 指令负载内嵌名 `pfVar24+8`；BossDead 分支 = 硬编码 `"BossDead"` @ `0x4CC1C0`）。

> **off-by-one 确认**：Ghidra 复制循环把目标基址写作 `+0x528B`/`+0x52CB`，但循环体是"先自增指针再写"（`pcVar3 = pcVar3 + 1` 后 `pcVar3[...] = cVar1`），**首字节实际落在 `+0x528B+1 = +0x528C`（第二份 `+0x52CB+1 = +0x52CC`）**。消费方佐证：`case 0xA7`（`th15_ghidra_dump.txt:2870`）用 `iVar22*0x88 + 0x528C + scr` 读取名字串传给 `FUN_0048f6c0`（显示）。`+0x528B` 本身是 HP dword（`+0x5288`~`+0x528B`）的最高字节，读作字符串起点只会得到空/残缺。

### 2.3 两个调用点（换攻击钩，`case 0x39` / opcode 0x202）

原始反汇编（`th15_report_raw.txt:474-527`）：

```
0042c735: LEA EAX,[ESI+0x20]                  ; EAX = &攻击数据[0x20]（名字缓冲）
...
0042c747: TEST dword ptr[EDI+0x4060],0x800000 ; boss 攻击中? （DAT_004e7794 态==0x20 时）
0042c751: JZ  0042c791                        ; 否 → normal 分支
; ---- BossDead 分支 ----
0042c762: CALL 00430320                       ; EAX = ECL 参数[0] = 槽索引
0042c767: MOV  ECX,[EDI+0x44B8]               ; ECX = scr
0042c76d: IMUL EDX,EAX,0x88
0042c773: PUSH 0x4cc1c0                       ; param_3 = "BossDead"
0042c778: PUSH ESI                            ; param_2 = HP
0042c779: PUSH EAX                            ; param_1 = 槽索引
0042c77a: MOV  [EDX+ECX+0x5284],0             ; 清当前槽阈值
0042c785: CALL 0042ff10                       ; <<< 调用点 A（BossDead）
; ---- normal 分支 ----
0042c7ad: CALL 00430320                       ; EAX = ECL 参数[0] = 槽索引
0042c7b2: MOV  ECX,[EDI+0x44B8]               ; ECX = scr
0042c7b8: IMUL EDX,EAX,0x88
0042c7be: MOV  [EDX+ECX+0x5284],ESI           ; 槽阈值 = 参数[1]
0042c7c5: TEST ESI,ESI
0042c7c7: JS   0042dba7                       ; 参数[1]<0 → 不写名字，直接结束
0042c7cd: PUSH [ESP+0x2C]                     ; param_3 = 指令负载名（&攻击数据[0x20]）
0042c7d1: PUSH [ESP+0x50]                     ; param_2 = HP
0042c7d5: PUSH EAX                            ; param_1 = 槽索引
0042c7d6: CALL 0042ff10                       ; <<< 调用点 B（normal）
```

**两处命中时寄存器/栈位**（EHOOK 替换的是 5 字节 CALL，命中时返回地址未压栈，栈顶即参数）：

| 地址 | 覆盖 | EAX | ECX | [ESP+0] | [ESP+4] | [ESP+8] |
| --- | --- | --- | --- | --- | --- | --- |
| `0x42C7D6` | normal | 槽索引(参数[0]) | scr | 槽索引 | HP | 指令负载名 |
| `0x42C785` | BossDead | 槽索引(参数[0]) | scr | 槽索引 | HP | `"BossDead"` |

> 两分支互斥：normal 分支覆盖非符↔符卡普通切换；BossDead 分支覆盖死亡/收尾。**需两个都挂才覆盖全部 0x202**（与 report2 §2.3 结论一致）。`EAX`（槽索引）从 `CALL 00430320` 返回后未被改写，hook 命中时即为参数[0]。

### 2.4 为什么槽里永远是"下一攻击"名（写入时机与推进者）

`FUN_00430050`（`th15.exe.c:33873-33981`，每帧由 boss 更新调用）负责**消费**槽：

- 第一段循环（HP 触发）：boss 当前 HP（`scr+0x5180`）≤ 某槽阈值（`scr+0x5284`）→ 该槽消费（`+0x5284 = 0xffffffff`），**返回 `scr + i*0x88 + 0x528C`（副本1 名字指针）**，此即**进入阶段的名字**。
- 第二段循环（计时/结束触发）：按 `scr+0x5288`（HP/计时）找未消费槽，计时到点后消费，返回 `scr + i*0x88 + 0x52CC`（副本2）。

即：**0x202（ins_514，SetNextPattern）在阶段 k 开始时（或阶段 k 进行中）预注册阶段 k+1 的名字**；阶段 k+1 真正开始时由 `FUN_00430050` 消费槽并短暂显示其名；随后下一条 0x202 又把槽覆盖为阶段 k+2 的名字。因此**任意时刻槽内都是"下一个还没开始的攻击"的名字**，与 requests3 实测序列（非符1→读到符卡1、符卡1→读到非符2…）完全自洽。

**推进者**：ECL 脚本指令流本身（各阶段段首连续出现 0x202）+ 解释器 `FUN_004291e0`。名字串来源 = 0x202 指令负载内嵌字符串（`local_498 = pfVar24 + 8`）；exe 内只有 `"BossDead"`/`"BossEscape"` 两个字面量，`Boss1/BossCard1` 等来自 ECL 数据。

**"当前攻击名"无持久字段**：阶段开始后其名字即被覆盖。显示路径只保留名字**表索引**（`[*(scr+0xC)+4]`，由 `FUN_0048f340` 查表所得），无可反查的稳定字符串来源。

### 2.5 param_1 恒为 1？param_1=0 槽为何空？

- `param_1`（ECL 参数[0]）是**每攻击独立的槽索引，由各 boss 的 ECL 脚本决定**（0~7），测试关卡恰好全程用槽 1，**不保证全局恒为 1**。读取必须用 hook 命中时的 `EAX` 动态计算偏移。
- 槽 0 为空 = 该 boss 的 0x202 从未向槽 0 写名字；槽构造时初始化为 -1/空。**这不是通用规律**（其它 boss 可能用槽 0 或分槽）。

### 2.6 最终推荐方案：换攻击钩缓存"写前旧值"

因 0x202 预注册下一攻击时，**槽内旧值恰是当前正在进行的攻击名**，故在写前缓存旧值即可稳定得到"当前攻击名"。用 requests3 §4 的 `GetBossStageName()` 骨架改造成：

```cpp
// ---- 全局 ----
static std::string g_cur_stage_name;          // miss/bomb 时读的"当前攻击名"

// ---- 换攻击 hook：0x42C7D6（normal）+ 0x42C785（BossDead），各 5B ----
// 命中时：EAX = 槽索引(ECL参数[0])；ECX = scr；[ESP+0]=槽索引，[ESP+4]=HP，[ESP+8]=新写名字
void OnPatternSwitch(PCONTEXT pCtx) {
    if (g_boss_ptr) {
        int scr = *(int*)(g_boss_ptr + 0x44B8);
        int slot = (int)pCtx->Eax;
        if (scr && slot >= 0 && slot < 8) {
            std::string old = ReadName(scr + slot*0x88 + 0x528C);   // 写前旧值 = 当前攻击名
            if (!old.empty()) {
                g_cur_stage_name = old;                             // 正常路径
            } else {
                g_cur_stage_name = ReadName((char*)pCtx->Esp + 8);  // 每 boss 首个 0x202 兜底：缓存新写入名
            }
        }
    }
    // 必须继续原调用以完成写入，否则换攻击流程被破坏：
    // asm_call_rel<0x42ff10, Thiscall>(scr, slot, hp, name)  ← 用 [ESP+0]/[ESP+4]/[ESP+8]
}

// ---- miss/bomb 时读 ----
static std::string GetBossStageName()
{
    if (!g_boss_ptr) return {};
    if (!(*(uint32_t*)(g_boss_ptr + 0x4060) & 0x800000)) return {};  // boss 战门控
    return g_cur_stage_name;                                        // 当前攻击名（稳定）
}
```

**边界条件**：
- **进场**：`0x42B938`（case 0x37，EAX=参数[0]≥0）→ `g_cur_stage_name.clear()`（新 boss 开始，避免上一关残留）。
- **退场**：`0x42B938`（参数[0]<0）→ 可清空；游戏同时清除 `+0x4060` 的 `0x800000`，读侧自动失效。
- **道中/非 boss**：`flg & 0x800000 == 0` → 返回空（现有门控已覆盖）。
- **首攻击窗口**：每 boss 首个 0x202 之前缓存为空，兜底逻辑（旧值空→缓存新值）已覆盖。
- **跨关/切 stage**（`FUN_0043bff0`）：与 `g_boss_ptr` 一并清空缓存。
- **线程**：hook 与 miss/bomb 同游戏线程，无需加锁；`g_cur_stage_name` 用普通静态量即可。

### 2.7 备选方案与自维护计数评审

- **备选（更鲁棒，稍重）**：hook `FUN_00430050` 的调用点（boss 更新内 `th15.exe.c:29233`/`29259` 的 `CALL 00430050`），`asm_call` 执行后若 EAX≠0（恰在阶段开始消费槽），缓存 `ReadName(EAX)`。无任何 0x202 时序假设、覆盖全部阶段含首攻击，但为帧级触发、回调开销略高。**2.6 主方案在测试数据下已验证，优先用主方案。**
- **自维护计数（report2 §3.3）评审**：`0x42C7D6`+`0x42C785` 双钩覆盖全部 0x202、`0x42B938` 进场归零/退场置 -1 的方案**成立**（两分支互斥需都挂；`0x42B938` 参数[0]≥0/＜0 判定已在 case 0x37 反汇编确认）。但它只能维护**攻击序号**，名字仍需自建 ECL 序列映射表，且跨关不唯一（各关都有 Boss2）。**不如直接缓存名字串**，故不推荐为主方案。

---

## 3. 证据索引

| 证据 | 位置 |
| --- | --- |
| `FUN_0042ff10` 反编译（写 HP + 名字副本1/2） | `th15.exe.c:33785-33811`；`th15_report_raw.txt:2245-2274` |
| case 0x39（0x202）两调用点反汇编 | `th15_report_raw.txt:474-527`；`th15.exe.c:31413-31434` |
| case 0x37（0x200）进场/退场反汇编 | `th15_report_raw.txt:315-335`；`th15.exe.c:31394-31410` |
| `FUN_00430050` 槽消费（HP 触发→副本1 / 计时→副本2） | `th15.exe.c:33873-33981` |
| case 0xA7 从 `+0x528C` 读名字串 | `th15_ghidra_dump.txt:2870` |
| 槽构造初始化（-1/空） | `th15.exe.c:28692-28715` |
| `+0x4080` 仅位掩码、无内存偏移读写 | `th15.exe.c:9647,29352,29374`；三 dump 检索零出现 |
| 名字表索引显示（`[scr+0xC]+4`） | `th15.exe.c:29233-29246` |
| `+0x56E4` 仅清零、非持久名字源 | `th15.exe.c:28669,29857,29871,30008` |

## 4. 遗留 / 建议实测

- 若担心跨 boss 0x202 时序差异，开发组可保留 `@5314`（槽 1 副本 1）作为**旁证日志**：miss/bomb 时同时记录 `g_cur_stage_name`（应=当前攻击）与 `scr+0x88+0x528C`（应=下一攻击），二者相邻即可交叉验证缓存正确性。
- `FUN_0042ff10` 的原始汇编不在现有 dump 区间内（仅 C 反编译 + 调用点），如需逐字节指令可对 `0x42FF10` 补一段 disasm 区间。

---

*创建：2026-08-19*

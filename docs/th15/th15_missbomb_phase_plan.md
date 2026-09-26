# TH15 非 boss 阶段（道中）记录：目标方案与当前进度

> 状态：**sub 方案已落地并通过构建**。sub 名 → 道中段落名映射表 + IGI 显示"上一次
> miss/bomb 位置"均已实现；道中 miss/bomb 恢复落盘。
> 范围：**仅 thprac 练习模式（thp）**；**仅 ST1~ST6**（ST7/EX 不做）。
> 相关：`docs/th15/th15_re_disasm_requests5.md`、`docs/th15/th15_re_disasm_report5.md`、
> `docs/th15/th15_missbomb_recording.md`、`thprac/src/thprac/thprac_th15_missbomb.{h,cpp}`、`thprac/src/thprac/thprac_th15.cpp`。

---

## 1. 目标

thp 模式下 miss/bomb 时，**非 boss 阶段（道中）**也要能记录/显示"当前游戏阶段"，
且标识只需**唯一区分**（命名不重要）。boss 阶段名逻辑已实现，不在本文范围。

**当前阶段性目标（本会话确定的做法）：**
先不做阶段映射，只做一件事——**读取并显示当前 ECL sub 名**（"当前章节"），
在 IGI 面板里显示，用于验证数据源正确。道中的 miss/bomb 落盘标记**暂时撤掉**（仍记 `-`）。

---

## 2. 为什么放弃"累计 ECL 时间"

原方案（report5）：读 `thread+0` 时间、与开始时间求差、对 ECL 脚本时间表判定阶段。
实测与反汇编确认**不可用**：

- `thread+0` 不是累计时钟：wait（`0x48CD51: subss`）把时间往下减；每帧只在
  "下一条指令 tag > time"（stall）时才 `time += delta`（`0x48E463: addss`；判断在
  `0x48E44B: comiss/jae`）。它是个"还欠多少帧"的值，在下一个指令 tag（多为 0，主线后半 60）
  附近来回摆，不随进度单调增长。
- 于是 `读值 - t0` 当 elapsed 会失效；report5 §4.4"段内时间 = at_frame + 帧数"与指令语义不符。
- 尝试改为"在 `0x48E463` 累计 tick"：虽然单调，但**练习跳转后入口偏移/重置**使其与脚本
  绝对时间轴对不上（跳转 tag 30/50/20/90… + `at_frame`/连接补丁），且需离线模拟/逐关标定，
  维护成本高、易错。**已整体删除。**

> 结论：不再用时间轴；改为直接看"脚本现在执行到哪个 sub"。

---

## 3. 当前方案：读当前 ECL sub 名

### 3.1 数据源（已由反汇编确认）

- 主线宿主：`g_main_ecl_host`（`thprac_th15_missbomb` 内 `mEclHost`），由 spawn `"main"` 的
  三个调用点捕获：
  | 钩子 | 地址 | 说明 |
  | --- | --- | --- |
  | `th15_ecl_host_capture` | `0x43BBC8` (2B) | `FUN_0043BBB0` 内（基本未走） |
  | `th15_ecl_host_capture2` | `0x43D48E` (5B) | 关卡进入路径（同时重置 0x4E73FC/0x4E7400） |
  | `th15_ecl_host_capture3` | `0x43D6BE` (5B) | 重开/重试路径 |
  命中时 `EAX` = 宿主对象。
- 读取表达式（`FUN_0048F340` 的 sub 表结构）：

  ```cpp
  host   = mEclHost;
  thread = *(uintptr_t*)(host + 0x0C);     // 当前线程
  subIdx = *(int32_t*)(thread + 4);        // 当前 sub 序号
  res    = *(uintptr_t*)(host + 0x11F8);   // ECL 资源对象（EclResourceInf）
  count  = *(int32_t*)(res + 8);           // sub 数量
  table  = *(uintptr_t*)(res + 0x8C);      // sub 表；表项 8B
  name   = *(const char**)(table + subIdx * 8);  // 表项 +0 = 名字指针（char*）
  ```
  （表项 `+4` = sub 起始指令地址；`FUN_0048f340` 是按名字二分查找 `table[i].name`。）

- 已实现 `TH15MissBombRecorder::GetEclSubName()`，含空指针/越界保护，不可用返回 `nullptr`。
- IGI（`TH15InGameInfo::OnContentUpdate`，miss/bomb 计数下方）新增一行显示：

  ```cpp
  const char* eclSub = g_th15mb.GetEclSubName();
  ImGui::Text("ECL sub"); ... ImGui::Text("%8s", eclSub ? eclSub : "-");
  ```

### 3.2 原始 ECL 文件（供后续阶段映射参考）

- 完整 raw：`D:\workshop\wind\th15decode\data\st0X.ecl`（**含 docs 目录缺的 st04/st06**）。
- 格式：`SCPT` 头 → `ANIM`/`ECLI` 字符串 → `ECLH` 偏移数组 → 名字 blob → 各 `ECLH` 块。
  - 块头 16B，之后指令 = `[int32 tag][uint16 opcode][uint16 size][args…]`（size 含 8B 头）。
  - wait = opcode `0x17`（参数在 payload `+8`）；goto = `0x0C`（dword4=相对偏移，dword5=绝对时间）；
    call = `0x0B`（内嵌 sub 序号 + 名字字符串）。

---

## 4. 本次已完成（构建通过）

- **删除 tick 方案**：`th15_ecl_time_advance`（`0x48E463`）钩子、`mEclTicks` /
  `OnEclTimeAdvance` / `GetEclTicks` / `NeedsEclClock` / `mEclTimeStr` 全部移除。
- **保留宿主捕获**（上述三个 spawn 点）→ `OnEclHostCaptured(host)`。
- **新增 `GetEclSubName()`**；IGI 显示当前 sub 名（`ECL sub` 一行）。
- **道中 miss/bomb 标记撤回**：`Record()` 非 boss 恢复记 `-`；boss 阶段名逻辑不动。
- 构建：`msbuild thprac\thprac.vcxproj /p:Configuration=Debug /p:Platform=Win32` 通过。

---

## 5. 已完成（本会话）

1. 修复 `GetEclSubName()` 内 `uintptr_t re s` 笔误（→ `res`）。
2. **道中章节映射表** `TH15_MB_MID_MAP`（`thprac_th15_missbomb.cpp`，格式同
   `Th15MbMapEntry`）：key = `"<stage>-<sub>"`，只收录主流程 `MainSubNN`；
   value = 练习菜单章节格式 `前半 #N` / `后半 #M`（ST6 `{4,0}` 用纯数字 `1..4`）。
   `LookupMidStageName()` 只查 `<stage>-<sub>`，查不到回退原始 sub 名。
   - **章节划分由 `THStageWarp` 各 portion 的 `ECLJump` 实际落点反推**（落点精确
     命中一条 `call MainSubNN`）：例如 ST1 p2→MainSub01b(前半2)、p3/p4→MainSub04/05、
     p5/p6→MainSub06/07。
   - `*b`/`*c` async sub 与 `main`/`MainFront`/`MainLatter` 等容器**不建条目**，
     命不中即回退原始名（跨章共享的 async sub 无法用单一 sub 名区分，故不映射）。
3. **恢复道中记录**：`Record()` 非 boss 分支读当前 ECL sub 名 → 映射 → 落盘
   （`ST1 m(前半 #2)`）；映射不可用时回退原始 sub 名；再无数据才记 `-`。
4. **IGI 改为显示"上一次 miss/bomb 位置"**：撤掉 `ECL sub` 实时行，新增 `Last m/b`
   行，内容为 `m(前半 #2)` / `b(BossCard1)`（记录器内 `mLastEvent`，每局开始/结束清空）。

构建：`msbuild thprac\thprac.vcxproj /p:Configuration=Debug /p:Platform=Win32` 通过。

---

## 6. 章节落点推导（ST1~ST6，依据 `THStageWarp` + 各 `st0X.ecl` 偏移解码）

| 关 | `mChapterSetup` | portion → 落点 sub | 映射 |
| --- | --- | --- | --- |
| ST1 | {2,4} | p2→MainSub01b；p3/p4→MainSub04/05；p5/p6→MainSub06/07 | 前半1=00；前半2=01/02/03；后1=04；后2=05；后3=06；后4=07 |
| ST2 | {3,3} | p2/p3→MainSub02/03；p4/p5/p6→MainSub04/05/06 | 前半1=00/01；前半2=02；前半3=03；后1=04；后2=05；后3=06 |
| ST3 | {3,3} | p2/p3→MainSub01/02；p4/p5/p6→MainSub03/04/05 | 前半1=00；前半2=01；前半3=02；后1=03；后2=04；后3=05 |
| ST4 | {3,3} | p2/p3→MainSub01/02；p4/p5/p6→MainSub04/05/06 | 前半1=00；前半2=01；前半3=02/03；后1=04；后2=05；后3=06 |
| ST5 | {4,6} | p2..p10→MainSub01..09 | 前半1..4=00..03；后半1..6=04..09 |
| ST6 | {4,0} | p2/p3/p4→MainSub02/03/04 | 1=00/01；2=02；3=03；4=04 |

---

## 7. 已知风险 / 待验证

1. **实测 IGI**：道中 miss/bomb 后 `Last m/b` 行应显示章节名；boss 战显示阶段名。
2. `host+0x11F8` 对主线宿主是否全程稳定（boss ECL 会替换该字段——但那是 boss 宿主）。
3. 练习补丁（`THStageWarp`/`THPatch`）只改 ECL 数据、不改 sub 结构，sub 名稳定；
   仍需实测跳章后名称正确。
4. 宿主指针失效窗口（换关/退场）：目前在一局开始/结束时清空 `mEclHost`。
5. 仅 ST1~ST6；ST7/EX 不纳入。

---

*创建：2026-09-26*

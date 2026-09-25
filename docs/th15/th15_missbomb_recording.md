# TH15 miss/bomb 记录功能：实现说明

> 状态：**已实现并通过真机功能测试（2026-08-22）。IGI 显示最近 5 条 miss/bomb 事件。**
> 待办：道中段落位置记录（依据 `th15_re_disasm_report4.md` 的章节号编码与各关映射）。

---

## 1. 需求

TH15（东方绀珠传）游戏中，自机 miss（掉命）或 bomb（放雷）时记录：

1. 当时所在**关卡序号**（`0x4E73F0`，1~7）；
2. 若处于 **boss 战阶段**，记录 boss 当前**阶段名**（`BossN`=非符N、`BossCardN`=符卡N、`BossDead`=死亡收尾；道中/非 boss 战记 `-`）。

按"一局游戏"分行保存到 `th15_missbomb.txt`（游戏 exe 同目录）。

---

## 2. 最终实现方案

### 2.1 钩子接入（`thprac_th15.cpp`）

| 地址 | 类型/长度 | 名称 | 作用 |
| --- | --- | --- | --- |
| `0x456398` | EHOOK 5B | `th15_life_dec` | **记录点（miss）**：`Record('m', stage, GetBossStageName())` |
| `0x41497A` | EHOOK 5B | `th15_bomb_dec` | **记录点（bomb）**：`Record('b', ...)` |
| `0x43E6EE` | EHOOK 7B | `th15_game_start` | 新一行边界（`OnStageStart`）+ 清 `g_boss_ptr`/`g_cur_stage_name`/`g_boss_enter_count` |
| `0x42C738` | EHOOK 4B | `th15_lock_timer2` | 0x202 handler 入口：缓存 `g_boss_ptr = pCtx->Edi` |
| `0x42B938` | EHOOK 2B | `th15_lock_timer3` | case 512（0x200）：缓存 `g_boss_ptr`；进场（EAX>=0）写死首阶段 `g_cur_stage_name = "Boss1"` + `++g_boss_enter_count` |
| `0x42C7D6` | EHOOK 5B | `th15_pattern_switch` | 换攻击钩 normal 分支 → `OnPatternSwitch` |
| `0x42C785` | EHOOK 5B | `th15_pattern_switch_bossdead` | 换攻击钩 BossDead 分支 → `OnPatternSwitch` |

### 2.2 阶段名来源

- `g_cur_stage_name` 由换攻击钩 `OnPatternSwitch`（0x42C7D6/0x42C785，`call 0x0042ff10` **写前**）维护：
  - **槽复用**：读槽写前旧值 `scr + 槽索引(EAX)*0x88 + 0x528C`，即当前攻击名（0x202 预注册下一攻击，旧值恰为当前攻击）；
  - **兜底**：槽空时仅当 `g_cur_stage_name` 为空（本 boss 首个 0x202）才缓存新写入名 `[ESP+8]`——已有当前名时槽空只说明该攻击用了新槽（预注册下一攻击），**不得覆盖当前名**。
- **第一阶段固定写死 `Boss1`**：TH15 各 boss 首阶段恒为非符1；首攻击注册 0x202 的时序不可靠，故在 boss 进场（case 512）直接置 `g_cur_stage_name = "Boss1"`。
- **收尾特例**：`g_cur_stage_name == "BossDead"/"BossEscape"` 时，查表 `g_th15_mb_map[st][which][count-1].name` 返回该 (关,段) 末符卡名（boss 死亡动画期间 miss 记末符卡）。
- boss 战门控：`[g_boss_ptr + 0x4060] & 0x800000` 未置位 → 返回空（记 `-`）。

### 2.3 道中/关底判定（`GetBossWhich`）

- 正常游玩：`g_boss_enter_count`（case 512 进场计数，game_start 清零）——首进场=道中(0)、次进场=关底(1)。
- 练习模式：`thPracParam.section` 查 `th_sections_cba` 段落表；章节练习（10000+ 编码）不匹配 → `-1` 保底。

### 2.4 记录模块 `TH15MissBombRecorder`

- `OnStageStart()`：开关开启时开新行（时间戳 + 缓冲）；`OnRunEnd()` 追加写盘。
- `OnPreUpdate` 轮询 `PLAYER_PTR` 归零（GameOver/回主菜单）→ `OnRunEnd()` 落盘。
- 边界：continue / FastRetry → 同一局不另起行；进程被强杀丢失未收尾行（可接受）。
- IGI 显示：`Record` 同时把 miss/bomb 事件压入 `mRecentMisses`（最近 5 条，miss/bomb 混合），供 InGameInfo 显示。

### 2.5 落盘格式

```
# th15 miss/bomb record (M=miss, B=bomb)
[2026-08-18 20:15:03] ST1 m(Boss1) b(Boss1) ST1 m(BossCard1)
[2026-08-18 20:16:22] ST2 m(BossCard2) ST2 b(-)
```

每行 = 一局，行首时间戳；每事件 = `ST<关卡> m(<阶段名>)/b(<阶段名>)`。

### 2.6 配置与界面

- `AdvancedGameOptions::th15_record_missbomb`（`thprac_games.h/.cpp`）+ `LauncherSettingGet("auto_th15_record_missbomb", ...)`。
- 高级选项（`TH_ADV_OPT`）Gameplay 分组开关 Checkbox；locale `THPRAC_INGAMEINFO_TH15_RECORD_MISSBOMB(_DESC)`（zh/en/ja）。
- IGI 显示（调试 UI 已移除）：miss/bomb 计数下方显示**最近 5 条事件**（`ST<关> m(<阶段>)/b(<阶段>)`，各 1 行；无事件显示 `NN`）。窗口高度固定为屏幕 38%（`SetSizeRel(360/1280, 0.38f)`）以容纳列表——曾因 auto-fit 仅出现头 2 帧生效、之后锁定，列表被裁到窗口底边之下不可见。

---

## 3. 关键内存事实（反编译确认）

- TH15 **无"当前攻击序号"内存字段**；唯一稳定标识 = 攻击名字串。
- 名字串写偏移 = `scr + 槽索引*0x88 + 0x528C`（副本2 `+0x52CC`）；`+0x528B` 是 HP dword 高字节（off-by-one）。
- 槽内恒为"下一攻击"名（0x202 预注册，`FUN_00430050` 消费槽时短暂成为当前名）。
- `[boss+0x4080]` 无写点（读取伪影）；`[boss+0x4070]` 仅进场/退场写 0/-1，不可作阶段号。
- 依据：`th15_re_disasm_report2.md` §3.2、`th15_re_disasm_report3.md` §2。

---

## 4. 验证清单（已通过真机功能测试）

1. 道中 miss/bomb → `ST<关卡> m(-)/b(-)`；
2. boss 战各阶段（含第一阶段 `Boss1`）miss/bomb → 阶段名与屏幕一致；
3. BossDead 动画期间 miss → 记末符卡名；
4. 练习模式直达道中/关底各段落 → 阶段名正确；章节练习/EX → 保底输出字段原值；
5. 过关/continue/FastRetry 同一行；GameOver/回主菜单落盘；新局新行；开关关闭不写文件。
6. IGI 显示：练习模式 miss/bomb 后显示最近 5 条事件（各 1 行），无事件显示 `NN`；窗口高度固定 38% 容纳列表。

---

## 5. 相关文件索引

| 文件 | 说明 |
| --- | --- |
| `th15_re_disasm_report2.md` | 反编译调查报告：无序号字段，确定名字串方案 |
| `th15_re_disasm_report3.md` | 反编译调查报告：名字串写入机制 + "缓存写前旧值"方案 |
| `th15_re_disasm_report4.md` | 反编译调查报告：道中章节号编码与各关映射（供道中段落记录落地） |
| `thprac_th15_hooks.md` | TH15 全部钩子整理文档 |
| `thprac/src/thprac/thprac_th15.cpp` | TH15 钩子实现源码 |
| `thprac/src/thprac/thprac_hook.h` | 钩子框架宏定义 |

---

*最后更新：2026-08-22 20:06*
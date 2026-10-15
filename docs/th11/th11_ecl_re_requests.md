# TH11 ECL 段落监控（读取 ECL 函数为手段） — 反编译/反汇编需求文档

> 总括方案：`docs/logger/logger_re_disasm_overview.md`。
> 需求来源：`docs/req_logger.md`（本迭代＝**仅监控**）。
> 二进制：`../exes/th11.exe`，688128 字节，PE `TimeDateStamp = 1217734394`，`ImageBase = 0x400000`，`RELOCS_STRIPPED`（`RVA = VA − 0x400000`）。
> ECL / decl 素材：`../exes/ecl/th11/`（原始 `.ecl` 20 份）、`../exes/ecl_dump/th11/`（`thecl -d` 反编译 `.decl` 20 份）。清单与来源见 `docs/logger/decompile_materials.md`。
> 引擎代：**B 代（第二代引擎）**；同代参照：th10 / th12 / th13。
> thprac 现有支持：`thprac/src/thprac/thprac_th11.cpp`。
> ⚠️ 本作 **stage 4 存在剧情分支**。

---

## 0. 本轮需要的东西

监控只需“**当前 ECL 函数名**”的地址/指针链/判据/hook 语义，不要求写实现。要求同总括 §8。

> **口径**：本迭代最终产物是**实时显示「当前段落」**，读 ECL sub 只是手段。本作读到 sub 后需按白名单映射为“段落入口”再显示（见总括 §1/§2.1）；并需回答 **Q9（段落判定口径）**。

---

## 1. 已知锚点

| 项 | 值 | 出处 |
| --- | --- | --- |
| ECL 解释器命中点 | `0x45d2a3`（EHOOK 2B，注释 “freeze ECL sub time for st4-6 main during midboss”） | `thprac_th11.cpp:611` |
| 命中时执行对象寄存器 | `pCtx->Edi` | 同上 |
| 执行对象已有字段 | `*(float*)pCtx->Edi` = 当前 ECL 时间（**已实际读取并参与比较**） | `:615-627` |
| 主线判据（现成） | `GetMemContent(pCtx->Edi + 0x1014, 0x103c + 0x1580) == 20512` | `:615-616` |
| ECL 数据区链 | `GetMemAddr(0x4a8d7c, 0x64, 0xC)` | `thprac_th11.cpp:2102`（`THSectionPatch`） |
| 指针常量 | `STAGE_PTR=0x4a8d60`、`ENEMY_MGR_PTR=0x4a8d7c`、`GAME_THREAD_PTR=0x4a8e88`、`REPLAY_MGR_PTR=0x4a8eb8`、`STAGE_NUM=0x4a5728`、`PLAYER_PTR=0x4a8eb4` | `thprac_th11.cpp:50-54`；`docs/gametime/gametime_thxx_flags.md` §3 th11 |
| 其它对象指针 | ReplayManager `0x4a8eb8` | 同上 |
| miss/bomb/边界钩子 | `th11_life_dec 0x4327f0`(6B)、`th11_bomb_dec 0x4311e6`(5B)、`th11_bomb_dec2`(dead bomb) `0x431293`(5B)、`th11_game_start 0x41fa6d`(3B) | `thprac_th11.cpp:2359-2374` |
| IGI 面板类 | `TH11InGameInfo`（`mMissCount`/`mBombCount`） | `thprac_th11.cpp:636` |

> 已有“主线判定”说明是在**boss 战期间识别主线执行对象**：`[Edi+0x1014]` 是某个管理器，`+0x103c+0x1580` 是敌人的 flags 字段（值 `20512`）。这条链很可能也通向 ECL 资源/sub 表，值得优先展开。

---

## 2. Q1 当前 sub：索引还是指令指针？（待答）

- `0x45d2a3` 命中时 `Edi`=主线脚本执行对象。请给出：“当前执行到哪个 sub/指令”的字段偏移与类型。
- 索引 → 读取表达式；指令指针 → sub 区间判定（参照 th14 sub 表）。

## 3. Q2 sub 表：索引 → 名字/起始（待答）

- 给出从 `Edi` 或全局出发到 **sub 表**（`索引 → 名字/起始`）的完整指针链。
- 线索：`[ENEMY_MGR_PTR(0x4a8d7c)+0x64]+0xC` 是 `THSectionPatch` 的 ECL 数据区；确认其上是否有 sub 表。
- 表项大小与 `+0`/`+4` 字段语义。

## 4. Q3 函数栈（待答）

- 是否存在可遍历的 ECL 子调用栈？链/遍历规则/深度上限；不存在则给出替代方案。

## 5. Q4 主线宿主捕获点（待答）

- 进关 / spawn “main” 时写入主线 ECL 宿主对象的 hook 点（地址 + 命中寄存器）。参考 TH15 `0x43bbc8/0x43d48e/0x43d6be`。

## 6. Q5 解释器命中点与寄存器语义（部分已知）

- 已有 `0x45d2a3`（2B），命中 `Edi`=执行对象。请补：所在函数入口与调用链、覆盖 2B 的指令语义、能否作每帧稳定触发点。

## 7. Q6 名字来源（待答）

- 运行期 ECL 资源是否保留 sub 名？不保留则给出“索引/偏移”字段，便于离线建名表。

## 8. Q7 机体/子机类型（待答）

- 请给出 `character` / `shotType` 地址（本作 thprac 未见明确常量）。

## 9. Q8/Q9 练习跳转补丁影响 + 段落判定口径（待答）

> **Q9（段落判定口径）**：本作走 **(A) 调用栈祖先法** 还是 **(B) 顺序 latch 法**？（定义见总括 §6 Q9；thprac 倾向 A，需结合 `.ecl` 脚本确认。）

- 练习 `ECLJump`（`thprac_th11.cpp`）是否改变 sub 索引/名字判读。
- ⚠️ **stage 4 分支**：确认运行期读取法在分支路径下仍正确。

---

## 10. 真机验证清单

- [ ] 道中/boss/不同 sub 采样，函数名与实际一致。
- [ ] **stage 4 两条分支**各走一遍，名字均正确。
- [ ] 练习模式跳章后名字正确；attract demo 行为确认。

## 11. 交付格式

- Q1–Q6 给出地址/指针链 + 判据 + 反汇编证据；Q7/Q8 视需要。RVA 表述。回填本文件 + 同步总括 §5。

## 12. 工具与复现

- `pefile` + `capstone` 只读反汇编。切入点：从 `0x45d2a3` 反汇编所在函数；展开 `[Edi+0x1014]` 链与 `[0x4a8d7c]+0x64` 链。

---

## 13. 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-14 | 建档（requests）。汇总已知锚点；列出 Q1–Q8 待答项。未跑逆向。 |

# AGENTS.md

本文件是**给 AI agent / 自动化工具**的协作约定，只讲「在这个仓库里怎么干活」。
**具体的需求内容与逆向结论不写在这里**，一律以 `docs/` 下文档为准。

---

## 0. 仓库产出形态

本仓库的绝大多数工作产出是 **`docs/` 下的分析文档与数据文件（Markdown / CSV）**，
而不是直接可编译运行的代码改动。动手前先读文档，产出后回填文档。

- 源码：`thprac/src/thprac/**`（C++，MSVC 工程 `thprac.sln`）。
- 文档：`docs/**`（需求 / 方案 / 逐作逆向 / 数据）。
- 仓库外的只读素材：`../exes/` —— **不进 git，不要提交**。

---

## 1. 环境与工具约束（重要）
- 编译 / CI 只发生在 Windows：`.github/workflows/main.yml`（`windows-latest` + MSBuild，仅 `master` push 触发）。
- 本机可用的逆向手段：`python` + `pefile` + `capstone`（x86-32 只读反汇编）、离线 ECL 解码。
- 素材清单与来源见 `docs/logger/decompile_materials.md`。

---

## 2. 文档驱动的工作方式（核心约定）

1. **`docs/` 是唯一事实来源。** 先读上位文档，再动手；有疑问回文档，不要凭记忆或猜测。
2. **分层与命名：**
   - 需求：`docs/req*.md`
   - 总括 / 方案：`docs/<需求>/<需求>_plan.md`，或 `docs/logger/logger_re_disasm_overview.md`
   - 逐作：`docs/thXX/thXX_*_requests.md`（要逆向什么）→ `docs/thXX/thXX_*_report.md`（逆向结论）
   - 数据：`docs/<需求>/thXX_*.csv` 等
3. **文档结构：** 头部引用块（上位文档 / 需求来源 / 二进制指纹 / 方法 / 状态 / 地址口径）→ 结论速览表 → 正文 → **变更记录表**（每次改动追加一行，含日期）。
4. **口径统一：** 地址一律同时给 `VA` 与 `RVA = VA − 0x400000`；术语（宿主 / 线程 / sub / checkpoint / 当前段落等）以总括文档的术语表为准，单篇不得自造或改写口径。
5. **证据要求：** 每个结论必须给地址 / 指针链 / 判据 / 证据；**未真机验证的地址不得写成结论**（按文档既有标记，如 🟡）。
6. **改动边界：** 新增 / 修订结论时，同步回填对应的总表与「变更记录」，不要各篇各写各的。

---

## 3. 当前进行中的需求：logger（ECL 段落监控 + 日志）

> 本节只做索引，不复述内容。

**需求与总括**

- 需求：`docs/req_logger.md`
- 总括（先读）：`docs/logger/logger_re_disasm_overview.md`
- 进度监控（活文档）：`docs/logger/logger_progress.md`
- 反编译素材清单与来源：`docs/logger/decompile_materials.md`
- 数据页「练习量」子页（launcher，hmaker 移植）：`docs/logger/launcher_data_practice.md`

**工作流与工具**

- checkpoint CSV 骨架生成：`docs/logger/checkpoint_csv_workflow.md`
- checkpoint「段落入口函数」回填：`docs/logger/checkpoint_entry_backfill_workflow.md`
- ECL dump 脚本：`docs/logger/tools/dump_ecl_decl.ps1`

**逐作数据（白名单 checkpoint）**

- `docs/logger/th10_checkpoints.csv` … `docs/logger/th18_checkpoints.csv`

**逐作逆向**

- `docs/th10/th10_ecl_re_requests.md` … `docs/th18/th18_ecl_re_requests.md`（需求）
- `docs/th10/th10_ecl_re_report.md` … `docs/th18/th18_ecl_re_report.md`（结论，九作齐全；th10–th13 的入口表小结另见 `docs/thXX/thXX_checkpoint_entry_report.md`）

---

## 4. 其它 / 已归档需求

- **gametime（全作游戏时长统计）**
  - 需求：`docs/req.md`
  - 方案与逐作标志位：`docs/gametime/gametime_plan.md`、`docs/gametime/gametime_thxx_flags.md`、`docs/gametime/gametime_runtime_verification.md`
  - 逐作逆向：`docs/thXX/thXX_re_disasm_requests.md` / `docs/thXX/thXX_re_disasm_report.md`

---

## 5. 代码与提交约定（只在确有必要时）

- 优先改 `docs/`；确需改代码时，遵循 `thprac/src/thprac/**` 既有风格，格式化用仓库根 `.clang-format`。
- 提交信息格式：`类型(范围): 说明`，例如 `docs(logger): ...`、`fix: ...`。
- 不要提交仓库外素材（`../exes`、`../th15decode`）以及构建产物。
- `docs/` 改动请一并更新相关文档的「变更记录」。

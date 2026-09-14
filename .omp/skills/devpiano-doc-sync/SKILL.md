---
name: devpiano-doc-sync
description: 对 devpiano 项目执行代码与 Markdown 文档的一致性审计与对齐更新——以当前真实代码（DSP/UI/数据模型/测试）为单一事实源，排查并平滑消除文档中的滞后断层与过时陈述；贯彻“保留物理/业务契约，弱化工程统计度量”防漂移准则；覆盖根目录 README、docs/reference/、docs/guides/、docs/issues/ 等现行文档；修改保留在工作区等待确认，禁止未经确认直接提交。
---

# devpiano 文档与代码一致性审计与对齐 (devpiano-doc-sync)

> 触发时机：每个重大开发阶段（Phase）结束、重大重构合并至主干后，或用户明确要求检查文档与代码一致性时。
> 核心原则：**以“当前真实代码”为唯一事实源（Source of Truth）**，全面排查并更新落后、过时的 Markdown 文档内容；贯彻**“保留物理/业务契约，弱化工程统计度量”**防漂移准则；**修改默认保留在本地工作区（Unstaged），严禁未经用户明确确认直接提交**。

---

## 1. 范围与边界

### 1.1 必须审计与对齐的现行文档（Active Docs）

1. **项目门户与总览**：
   - `README.md`（中文自述）与 `README_en.md`（英文自述）：特性矩阵、架构定位、测试命令与构建指南。
   - `docs/README.md`：文档中心总入口、三闸门基线说明、推荐阅读指引与 ADR 索引。
2. **核心规范与系统架构 (`docs/reference/`)**：
   - `docs/reference/project-scope.md`：项目定位、核心能力（发声引擎、输入、录制、UI）与明确非目标。
   - `docs/reference/architecture.md`：最新源码分层、模块职责（Audio/Input/Midi/Plugin/Recording/Export/Layout/Settings/UI/Locale）、DSP 拓扑图与主数据流。
   - `docs/reference/acceptance.md`：各阶段验收标准、历史里程碑验收记录与建议例行回归清单。
   - `docs/reference/brand-guidelines.md`：品牌视觉规范（如有更新）。
3. **功能特性与专项说明 (`docs/reference/features/`)**：
   - `builtin-piano-synthesis.md`：7 大声学系统、88 键参数表、DSP 拓扑结构、外部控制接口与物理声学测试套件。
   - `performance-presets.md`：`.devpiano.preset` JSON 规范（尤其是 `"acoustics"` 节点）、CRUD 编排流与向后兼容。
   - `performance-persistence.md`：`.devpiano` 原生演奏录制文件格式规范（Schema v2）。
   - `plugin-offline-rendering.md`：`RenderPipeline`、`WavExportOptions` 声学参数 1:1 对齐与 `RoomReverbEngine` 离线挂载。
   - `declarative-ui-and-theming.md`：JIVE 声明式 UI 运行时（已内化）、`ViewHost` 统一宿主门面、主窗口布局树与 `SettingsLayoutModel` 6 大卡片排版。
   - `internationalization.md`：`LocaleManager` 机制与 `zh_CN.loc` 翻译规范及典型词条。
   - `fixture-inventory.md`：MIDI/Performance 测试夹具清单及其与自动化单元测试的集成。
   - `keyboard-mapping.md`、`midi-channel-matrix.md`、`midi-file-import.md`、`per-key-customization.md`、`plugin-hosting.md`、`recording-playback.md` 等。
4. **已知问题与工程指南 (`docs/issues/` & `docs/guides/`)**：
   - `docs/issues/known-issues.md`：沉淀最新迭代/PR 审查中发现并修复的高价值物理/逻辑缺陷的回归线索与关联测试。
   - `docs/guides/quickstart.md`、`development.md`、`troubleshooting.md`、`wsl-windows-msvc-workflow.md`、`release-workflow.md`、`pr-agent.md`。
5. **决策记录与路线图 (`docs/decisions/` & `docs/roadmap/`)**：
   - `docs/decisions/README.md`：核对 ADR 编号完整性（当前至 ADR-014）。
   - `docs/roadmap/roadmap.md` & `current-iteration.md`：确认阶段完成状态、日期与交付描述一致性。

### 1.2 严格排除与禁止修改的目录（Forbidden Scope）

- **`docs/archive/`**：历史前期调研、RFC 选型讨论和已完成阶段归档。**必须保持历史原貌，只作参考，严禁主动修改**。
- **`docs/audit/`**：历史正式代码审计报告（如 `AUDIT-001`、`AUDIT-002`）。**属于时间戳固化的审计档案，严禁修改**。
- **`submodules/`**：外部第三方子模块代码与文档。**严禁修改**。

---

## 2. 核心原则：防漂移准则（Anti-Drift Policy）

在更新 Markdown 文档时，必须严格执行**“保留物理/业务契约，弱化工程统计度量”**法则：

### 2.1 必须系统性弱化（抽象化）的波动性工程度量

以下指标属于运行时产出或统计结果，随日常代码迭代频繁变动，**严禁在文档中硬编码具体个位数**：
1. **自动化测试断言数**：
   - ❌ *反模式*：`602,063+ 断言全部通过`
   - ✅ *防漂移*：`覆盖核心引擎、物理声学与 UI 全套自动化测试，零失败`
2. **测试用例条目数**：
   - ❌ *反模式*：`DamperReleaseTest (13项)`、`PianoSynthVoiceTest (14项)`
   - ✅ *防漂移*：`DamperReleaseTest`、`PianoSynthVoiceTest`（表格仅保留套件名称与验证指标）
3. **源码文件具体行数**：
   - ❌ *反模式*：`MainComponent.cpp 当前约 1310 行`
   - ✅ *防漂移*：`MainComponent.cpp 保持轻量装配职责，主体仅负责顶层装配与回调接线`
4. **特定环境单次耗时与临时单元数**：
   - ❌ *反模式*：`全量 68 个编译单元，冷启动约 5~6 分钟，缓存命中约 2 秒`
   - ✅ *防漂移*：`全量静态检查（迭代边界门禁，多核并行+本地缓存）`

### 2.2 坚决严格保留的契约性常量（Contractual Constants）

以下指标属于物理现实、声学模型、协议规范与 SLA 承诺，**必须 100% 保持精准数值**：
1. **键盘与物理规范**：88 键（A0 ~ C8，MIDI 21 ~ 108）、52 白键 + 36 黑键；
2. **声学物理参数**：16 峰正交云杉木物理模态（48Hz ~ 2250Hz）、4.2kHz 云杉木高频截止、纵波声速 $5100\text{ m/s}$、起音瞬态 $3\text{ ms}$、房间混响预设时间常数（Studio 0.6s / Chamber 1.5s / Concert Hall 2.4s）；
3. **协议与业务规范**：16 通道 MIDI 矩阵、全局调号范围（-7 .. +7 半音）、回放倍速（0.50x ~ 2.00x）、标准 MIDI Type 1 分辨率（960 PPQ）、参数归一化范围 `[0.0, 1.0]`、A4 基准基频调节范围 `400.0 ~ 480.0 Hz`、防抖写入间隔 `300ms`；
4. **硬实时硬性 SLA**：8 复音齐奏单核 CPU $\le 0.7\%$、零堆内存分配（No malloc）、零锁（No Lock）、零实时三角函数（0 `std::sin`）。

---

## 3. 标准执行流程 (Five-Step Workflow)

### Step 1: 增量与代码基线探测 (Code Survey)
1. 检查最近提交日志与变更范围：`git log -20 --oneline`、`git diff --stat <base-commit>..HEAD`；
2. 梳理本阶段在各模块引入的核心演进（如 DSP 拓扑、设置模型、预设 JSON 字段、UI 控件、离线渲染与新增测试）；
3. 记录新增或重构的核心类名、函数签名与状态枚举。

### Step 2: 现行文档扫描与落后点定位 (Doc Audit)
1. 对照 Step 1 的代码事实，逆向检索相关文档；
2. 重点排查：
   - 特性文档是否依然停留在旧 Phase 成果的陈旧表述；
   - 架构拓扑图是否反映最新的调用链与模块分层；
   - 预设 `.devpiano.preset` JSON Schema 是否缺少新字段；
   - `SettingsLayoutModel` 是否缺少新卡片与新控件；
   - 测试表格是否覆盖了新增的测试套件。

### Step 3: 精确编辑与防漂移平滑对齐 (Editing & Alignment)
1. 使用项目专用工具小步编辑，严格遵守 **7-bit ASCII** 编码规范（中文标点在引号内除外，严禁在英文上下文中引入不可见 Unicode 乱码）；
2. 逐一将落后描述、过时结构图、缺失字段与测试表格更新至当前代码状态；
3. 应用 §2 防漂移准则，剥离所有易变的测试断言数、用例项数与代码行数。

### Step 4: 质量门禁与回归验证 (Gate Verification)
1. 执行代码与文档格式检查：`./scripts/dev.sh format --check`；
2. 执行全量自动化单元测试：`./scripts/dev.sh test`；
3. 确保文档更新过程中未意外误触任何业务代码或破坏构建。

### Step 5: 交付汇总与等待确认 (Report & Confirmation)
1. 运行 `git status` 与 `git diff --stat` 确认修改范围；
2. 向用户输出详尽的审计与对齐报告：
   - 识别出的关键代码演进与落后断层；
   - 本次更新的文件清单与每个文件的核心改动点；
   - 防漂移准则落实情况；
   - 质量门禁执行结果；
3. **保持工作区修改未提交（Unstaged），明确等待用户审查并给出提交指示**。

---

## 4. 交付与行动准则

- **单一事实源**：一切以 `source/` 下编译通过的真实代码与测试为准，严禁把未实现能力写进文档。
- **免提交承诺**：文档更新工作完成后，只汇总报告，不自动执行 `git add` 或 `git commit`。
- **Conventional Commit 建议**：用户确认后，推荐使用格式：`docs: align markdown docs with code state and apply anti-drift policy`。

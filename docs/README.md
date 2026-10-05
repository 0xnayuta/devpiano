# devpiano 文档中心

> 用途：说明 `docs/` 的目录分层、阅读指引与各类文档的职责定位。
> 更新时机：新增、移动、重构或归档文档时。

---

## 推荐阅读指引

### 1. 新开发者 / 上手理解

- [`reference/project-scope.md`](reference/project-scope.md)：了解项目定位、核心能力与明确非目标。
- [`reference/brand-guidelines.md`](reference/brand-guidelines.md)：品牌标识标准、Logo 几何网格、字标、双蓝职责与图形语言规范。
- [`reference/architecture.md`](reference/architecture.md)：理解当前 JUCE 架构、DSP 物理建模、JIVE 声明式 UI 与模块职责。
- [`roadmap/roadmap.md`](roadmap/roadmap.md)：了解项目演进历史、当前全阶段完成状态与路线图。
- [`guides/quickstart.md`](guides/quickstart.md)：快速配置 WSL + Windows 镜像构建环境与常用命令速查。

### 2. 日常开发与工程纪律

- [`guides/wsl-windows-msvc-workflow.md`](guides/wsl-windows-msvc-workflow.md)：WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流详解。
- [`guides/development.md`](guides/development.md)：日常开发、构建与协作指引。
- [`roadmap/current-iteration.md`](roadmap/current-iteration.md)：当前本地化完整消息模板小阶段的任务与直接验收；已完成的 AUDIT-004 实施记录见 [归档](archive/audit-004-code-quality-fix-phases.md)。
- [`decisions/README.md`](decisions/README.md)：架构决策记录索引，含已接受的 [ADR-015 本地化完整消息模板与分类标点](decisions/ADR-015-localized-message-templates-and-punctuation.md)。
- [`guides/troubleshooting.md`](guides/troubleshooting.md)：WSL / Windows 镜像构建常见问题排查。
- [`guides/release-workflow.md`](guides/release-workflow.md)：Windows/Linux 正式 release、tag 与双平台打包 checklist。
- [`guides/pr-agent.md`](guides/pr-agent.md)：PR-Agent AI 代码审查工作流配置、命令与排障。

**工程三闸门基线**：
- **格式化**：`.clang-format`（WebKit 规范），`./scripts/dev.sh format --check`
- **单元测试**：Windows 镜像树配置 `BUILD_TESTS=ON` 并用 CTest 运行 `devpiano_tests`（命令见 [`guides/quickstart.md`](guides/quickstart.md)）；Linux CI 使用 `./scripts/dev.sh test`，本地 WSL 主树不运行该脚本
- **构建验证**：WSL 配置 `wsl-build --configure-only` + Windows 验证 `./scripts/dev.sh win-build`

---

### 3. 核心功能参考与专项测试（`reference/features/`）

| 领域 | 核心特性文档 | 主要内容与测试重点 |
|---|---|---|
| **发声引擎** | [`features/builtin-piano-synthesis.md`](reference/features/builtin-piano-synthesis.md) | 7 大声学系统全物理建模钢琴（`PianoSynthVoice`，88 键参数模型/非线性动力学/古典微律/双视角空间声学/微观机械拟真）与正弦合成 |
| **发声引擎** | [`features/plugin-hosting.md`](reference/features/plugin-hosting.md) | VST3 插件扫描、分片进度、XML 缓存恢复、增量崩溃安全持久化、乐器端点统一抽象、加载与生命周期专项回归 |
| **输入与映射** | [`features/keyboard-mapping.md`](reference/features/keyboard-mapping.md) | 电脑键盘映射、5 行 QWERTY 演奏看板、和弦识别与 HUD、击键动态力度和确定性微扰、发音身份快照、采样精确切分踏板及 88 键虚拟键盘 |
| **输入与映射** | [`features/per-key-customization.md`](reference/features/per-key-customization.md) | 128 项逐键自定义标签与颜色、按键绑定编辑对话框（`KeyBindingEditDialog`） |
| **输入与映射** | [`features/midi-channel-matrix.md`](reference/features/midi-channel-matrix.md) | 16 通道 MIDI 矩阵路由（移调/力度/音色/延音/按键跟随）与全局调号 |
| **录制与回放** | [`features/recording-playback.md`](reference/features/recording-playback.md) | 暂停捕获配对、0.5x–2.0x 回放、采样域重基准、Take-relative Seek / A-B 状态恢复、完整预备拍与标准 Type 1 MIDI 导出 |
| **录制与回放** | [`features/performance-persistence.md`](reference/features/performance-persistence.md) | `.devpiano` v3 JSON、JUCE 长度前缀二进制消息、内嵌快照、事务替换与 Take/原生文件绑定；旧纯 MIDI 文件兼容，旧数字预设事件拒绝 |
| **录制与回放** | [`features/midi-file-import.md`](reference/features/midi-file-import.md) | 标准 MIDI 文件导入、Type 0/1 全轨并轨、CC64 延音/弯音解析与回放 |
| **渲染与导出** | [`features/plugin-offline-rendering.md`](reference/features/plugin-offline-rendering.md) | VST3 插件与内置物理建模钢琴离线高保真渲染 WAV 导出（异步非阻塞任务流、`RenderPipeline`、`WavExportOptions` 声学参数 1:1 对齐） |
| **预设与状态** | [`features/performance-presets.md`](reference/features/performance-presets.md) | Performance Preset 预设系统（CRUD 编排、F1-F12 快捷键、录制中自动切调） |
| **UI 与交互** | [`features/declarative-ui-and-theming.md`](reference/features/declarative-ui-and-theming.md) | 内化的 JIVE 声明式 UI、ViewHost、主窗口节拍器控件与 QWERTY 和弦 HUD、时间轴、设计 Token 与通用弹窗 |
| **多语言** | [`features/internationalization.md`](reference/features/internationalization.md) | 运行时中英文双语即时切换（`LocaleManager` + 内嵌 `zh_CN.loc`） |
| **测试支撑** | [`features/fixture-inventory.md`](reference/features/fixture-inventory.md) | 固定 MIDI 与 Performance 测试夹具样本库清单 |

---

### 4. 质量审查、验收与问题追踪

- [`reference/acceptance.md`](reference/acceptance.md)：现行消费者验收、历史阶段交付记录与 Windows 软件/实机回归边界；历史勾选不外推本次硬件或所有厂商插件。
- [`audit/README.md`](audit/README.md)：审计与复审入口；AUDIT-004 保留首次基线并追加软件实施复审，问题状态只看第 8 章，直接修复证据见 [归档](archive/audit-004-code-quality-fix-phases.md)。
- [`issues/known-issues.md`](issues/known-issues.md)：已知问题、密集 MIDI 播放 CPU 深度剖析与已修复风险回归线索。

---

### 5. 历史档案（`archive/`）

- [`archive/README.md`](archive/README.md)：历史归档索引与现行替代关系表，含 Phase 35 完成计划；旧计划不再占用当前迭代入口。

---

## 文档职责原则

1. **唯一状态源**：[`roadmap/roadmap.md`](roadmap/roadmap.md) 是项目长期路线与全阶段完成状态的唯一权威来源；[`roadmap/current-iteration.md`](roadmap/current-iteration.md) 只记录当前正在推进的任务。
2. **架构客观性**：[`reference/architecture.md`](reference/architecture.md) 描述当前代码真实架构，不混入待办计划或历史方案对比。
3. **特性规范化**：`reference/features/` 下每个文档应合并该特性的现行行为说明与专项测试清单，剔除历史规划草案。
4. **历史进归档**：历史前期调研、RFC 选型讨论和已完成规划统一归档至 `archive/`。

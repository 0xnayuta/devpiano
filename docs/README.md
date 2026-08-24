# devpiano 文档中心

> 用途：说明 `docs/` 的目录分层、阅读指引与各类文档的职责定位。
> 更新时机：新增、移动、重构或归档文档时。

---

## 推荐阅读指引

### 1. 新开发者 / 上手理解
- [`reference/project-scope.md`](reference/project-scope.md)：了解项目定位、核心能力与明确非目标。
- [`reference/architecture.md`](reference/architecture.md)：理解当前 JUCE 架构、DSP 物理建模、JIVE 声明式 UI 与模块职责。
- [`roadmap/roadmap.md`](roadmap/roadmap.md)：了解项目演进历史、当前全阶段完成状态与路线图。
- [`guides/quickstart.md`](guides/quickstart.md)：快速配置 WSL + Windows 镜像构建环境与常用命令速查。

### 2. 日常开发与工程纪律
- [`guides/wsl-windows-msvc-workflow.md`](guides/wsl-windows-msvc-workflow.md)：WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流详解。
- [`guides/development.md`](guides/development.md)：日常开发、构建与协作指引。
- [`roadmap/current-iteration.md`](roadmap/current-iteration.md)：查看当前迭代正在推进的任务与验收状态。
- [`decisions/README.md`](decisions/README.md)：架构决策记录（ADR 001 ~ ADR 012）。
- [`guides/troubleshooting.md`](guides/troubleshooting.md)：WSL / Windows 镜像构建常见问题排查。
- [`guides/release-workflow.md`](guides/release-workflow.md)：Windows x64 正式 release、tag 与打包 checklist。
- [`guides/pr-agent.md`](guides/pr-agent.md)：PR-Agent AI 代码审查工作流配置、命令与排障。

**工程三闸门基线**：
- **格式化**：`.clang-format`（WebKit 规范），`./scripts/dev.sh format --check`
- **单元测试**：`devpiano_tests`（60 个测试套件、11989+ 断言），`./scripts/dev.sh test`
- **构建验证**：WSL 配置 `wsl-build --configure-only` + Windows 验证 `./scripts/dev.sh win-build`

---

### 3. 核心功能参考与专项测试（`reference/features/`）

| 领域 | 核心特性文档 | 主要内容与测试重点 |
|---|---|---|
| **发声引擎** | [`features/builtin-piano-synthesis.md`](reference/features/builtin-piano-synthesis.md) | 7 大声学系统全物理建模钢琴（`PianoSynthVoice`，88 键参数模型/非线性动力学/立体声共鸣）与正弦合成 |
| **发声引擎** | [`features/plugin-hosting.md`](reference/features/plugin-hosting.md) | VST3 插件扫描、分片进度、XML 缓存恢复、加载与生命周期专项回归 |
| **输入与映射** | [`features/keyboard-mapping.md`](reference/features/keyboard-mapping.md) | 电脑键盘映射系统、稳定 key code 路由、88 键虚拟键盘与输入法防御 |
| **输入与映射** | [`features/per-key-customization.md`](reference/features/per-key-customization.md) | 128 项逐键自定义标签与颜色、按键绑定编辑对话框（`KeyBindingEditDialog`） |
| **输入与映射** | [`features/midi-channel-matrix.md`](reference/features/midi-channel-matrix.md) | 16 通道 MIDI 矩阵路由（移调/力度/音色/延音/按键跟随）与全局调号 |
| **录制与回放** | [`features/recording-playback.md`](reference/features/recording-playback.md) | 实时演奏录制、多倍速回放控制（0.5x–2.0x）与标准 Type 1 MIDI 导出 |
| **录制与回放** | [`features/performance-persistence.md`](reference/features/performance-persistence.md) | `.devpiano` 原生演奏文件持久化（v2 JSON + Base64）、原子保存与最近文件 |
| **录制与回放** | [`features/midi-file-import.md`](reference/features/midi-file-import.md) | 标准 MIDI 文件导入、多轨自动选轨、CC64 延音/弯音解析与回放 |
| **渲染与导出** | [`features/plugin-offline-rendering.md`](reference/features/plugin-offline-rendering.md) | VST3 插件离线渲染与 WAV 导出（`RenderPipeline` 共享管线与后台多线程） |
| **预设与状态** | [`features/performance-presets.md`](reference/features/performance-presets.md) | Performance Preset 预设系统（CRUD 编排、F1-F12 快捷键、录制中自动切调） |
| **UI 与交互** | [`features/declarative-ui-and-theming.md`](reference/features/declarative-ui-and-theming.md) | JIVE 声明式 UI 架构、设计 Token、通用弹窗体系（`JiveModalDialog`）与静态资产内嵌 |
| **多语言** | [`features/internationalization.md`](reference/features/internationalization.md) | 运行时中英文双语即时切换（`LocaleManager` + 内嵌 `zh_CN.loc`） |
| **测试支撑** | [`features/fixture-inventory.md`](reference/features/fixture-inventory.md) | 固定 MIDI 与 Performance 测试夹具样本库清单 |

---

### 4. 质量审查、验收与问题追踪
- [`reference/acceptance.md`](reference/acceptance.md)：Phase 1–24 阶段性验收标准、v1.0.0 正式发布验收与全量回归清单。
- [`audit/README.md`](audit/README.md)：代码质量审计报告（`AUDIT-001` 全面审计看板与问题登记表）。
- [`issues/known-issues.md`](issues/known-issues.md)：已知问题、密集 MIDI 播放 CPU 深度剖析与已修复风险回归线索。

---

### 5. 历史档案（`archive/`）
- [`archive/README.md`](archive/README.md)：历史归档索引与现行替代关系表。

---

## 文档职责原则

1. **唯一状态源**：[`roadmap/roadmap.md`](roadmap/roadmap.md) 是项目长期路线与全阶段完成状态的唯一权威来源；[`roadmap/current-iteration.md`](roadmap/current-iteration.md) 只记录当前正在推进的任务。
2. **架构客观性**：[`reference/architecture.md`](reference/architecture.md) 描述当前代码真实架构，不混入待办计划或历史方案对比。
3. **特性规范化**：`reference/features/` 下每个文档应合并该特性的现行行为说明与专项测试清单，剔除历史规划草案。
4. **历史进归档**：历史前期调研、RFC 选型讨论和已完成规划统一归档至 `archive/`。

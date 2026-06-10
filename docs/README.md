# devpiano 文档入口

> 用途：说明 `docs/` 的分层、阅读顺序与每类文档的职责。
> 更新时机：新增、移动、归档文档时。

## 推荐阅读顺序

### 新开发者

- [`guides/quickstart.md`](guides/quickstart.md)：快速恢复环境与常用命令。
- [`reference/architecture.md`](reference/architecture.md)：理解当前 JUCE 架构与模块职责。
- [`roadmap/roadmap.md`](roadmap/roadmap.md)：了解当前状态、阶段路线与近期重点。


- [`guides/quickstart.md`](guides/quickstart.md)：命令速查。
- [`guides/development.md`](guides/development.md)：日常开发与构建细节。
- [`guides/wsl-windows-msvc-workflow.md`](guides/wsl-windows-msvc-workflow.md)：WSL 主工作树 + Windows 镜像树 + MSVC 验证的详细工作流。
- [`guides/release-workflow.md`](guides/release-workflow.md)：手工 release、tag 与 zip 打包 checklist。
- [`roadmap/current-iteration.md`](roadmap/current-iteration.md)：当前正在推进的任务。
- [`decisions/README.md`](decisions/README.md)：架构决策记录。

**代码格式**：`.clang-format`（WebKit 基），`./scripts/dev.sh format`
**静态分析**：`.clang-tidy`（bugprone/performance/readability/modernize），`clang-tidy-21`
**单元测试**：`cmake -DBUILD_TESTS=ON` → `devpiano_tests`，`./scripts/dev.sh test`

### 测试与验收

- [`reference/acceptance.md`](reference/acceptance.md)：阶段性验收标准。
- [`audit/README.md`](audit/README.md)：架构审查、设计评审与事后分析。
- [`issues/known-issues.md`](issues/known-issues.md)：已知问题、已修复风险的回归项与长期设计约束。
- [`reference/features/keyboard-mapping.md`](reference/features/keyboard-mapping.md)：键盘映射专项手工测试。
- [`reference/features/plugin-hosting.md`](reference/features/plugin-hosting.md)：插件宿主生命周期与退出稳定性专项测试。
- [`reference/features/layout-presets.md`](reference/features/layout-presets.md)：布局 preset 的保存、导入、重命名、删除与启动恢复专项手工测试。
- [`reference/features/recording-playback.md`](reference/features/recording-playback.md)：录制、回放与 MIDI 导出专项手工测试。
- [`reference/features/midi-file-import.md`](reference/features/midi-file-import.md)：MIDI 文件导入、回放、自动选轨与后续增强验收测试。
- [`reference/features/plugin-offline-rendering.md`](reference/features/plugin-offline-rendering.md)：插件离线渲染与文件输出专项测试。
- [`reference/features/fixture-inventory.md`](reference/features/fixture-inventory.md)：MIDI / Performance 测试夹具清单与用途说明。


### 功能设计

- [`reference/features/keyboard-mapping.md`](reference/features/keyboard-mapping.md)：电脑键盘到 MIDI note 的映射能力与边界。
- [`reference/features/plugin-hosting.md`](reference/features/plugin-hosting.md)：VST3 插件扫描、加载、处理、editor 和生命周期行为。
- [`reference/features/recording-playback.md`](reference/features/recording-playback.md)：录制 / 回放 / MIDI 导出的第一版设计与当前 MVP 行为。
- [`reference/features/layout-presets.md`](reference/features/layout-presets.md)：布局 preset 的文件格式、导入/保存/重命名/删除与恢复行为。
- [`reference/features/midi-file-import.md`](reference/features/midi-file-import.md)：MIDI 文件导入、回放兼容性、剩余边界与 FreePiano 差距。
- [`reference/features/performance-persistence.md`](reference/features/performance-persistence.md)：性能与持久化。
- [`reference/features/plugin-offline-rendering.md`](reference/features/plugin-offline-rendering.md)：插件离线渲染能力、文件输出格式与边界。
- [`reference/features/fixture-inventory.md`](reference/features/fixture-inventory.md)：MIDI / Performance 测试夹具清单。


### 历史资料

- `archive/`：历史迁移记录、旧规划文档与过期资料。

## 当前目录职责

| 目录 | 职责 |
|---|---|
| `guides/` | 快速开始、开发环境、构建、日常工作流、故障排查与协作规则。 |
| `reference/` | 系统架构 (`architecture.md`)、验收标准 (`acceptance.md`) 入口。 |
| `reference/features/` | 功能行为说明、专项测试与测试夹具清单。 |
| `issues/` | 已知问题与回归项。 |
| `tests/fixtures/` | 手工测试用例与测试 fixture 数据。 |
| `decisions/` | ADR，记录重要架构和工程决策。 |
| `roadmap/` | 路线图、当前迭代与后续任务规划。 |
| `planning/` | 未来阶段粗略规划与功能候选分析。 |
| `audit/` | 架构审查、设计评审、事后分析。 |
| `archive/` | 历史文档、旧方案、已被替代的资料。 |

## 文档职责原则

- [`roadmap/roadmap.md`](roadmap/roadmap.md) 是唯一的项目状态与路线图来源。
- [`roadmap/current-iteration.md`](roadmap/current-iteration.md) 只记录当前正在做的任务。
- [`reference/architecture.md`](reference/architecture.md) 只描述当前架构，不混入任务清单。
- `reference/features/` 下每个功能的设计、行为说明与专项测试合并在一个文件中。
- `issues/` 下只放已知问题与回归项。
- `archive/` 中的内容只作为历史参考，当前信息以现行文档为准。

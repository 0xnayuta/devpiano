# devpiano 文档入口

> 用途：说明 `docs/` 的分层、阅读顺序与每类文档的职责。  
> 读者：新接手项目的人、日常开发者、后续维护者。  
> 更新时机：新增、移动、归档文档时。

## 推荐阅读顺序

### 新开发者

1. [`getting-started/quickstart.md`](getting-started/quickstart.md)：快速恢复环境与常用命令。
2. [`architecture/overview.md`](architecture/overview.md)：理解当前 JUCE 架构与模块职责。
3. [`roadmap/roadmap.md`](roadmap/roadmap.md)：了解当前状态、阶段路线与近期重点。

### 日常开发

- [`getting-started/quickstart.md`](getting-started/quickstart.md)：命令速查。
- [`development/wsl-windows-msvc-workflow.md`](development/wsl-windows-msvc-workflow.md)：WSL 主工作树 + Windows 镜像树 + MSVC 验证的详细工作流。
- [`roadmap/current-iteration.md`](roadmap/current-iteration.md)：当前正在推进的任务。

### 测试与验收

- [`testing/acceptance.md`](testing/acceptance.md)：阶段性验收标准。
- [`testing/keyboard-mapping.md`](testing/keyboard-mapping.md)：键盘映射专项手工测试。
- [`testing/layout-presets.md`](testing/layout-presets.md)：布局 preset 的保存、导入、重命名、删除与启动恢复专项手工测试。
- [`testing/recording-playback.md`](testing/recording-playback.md)：录制、回放与 MIDI 导出专项手工测试。
- [`testing/midi-file-import.md`](testing/midi-file-import.md)：M8 MIDI 文件导入、回放、自动选轨与后续增强验收测试。
- [`testing/plugin-host-lifecycle.md`](testing/plugin-host-lifecycle.md)：插件宿主生命周期与退出稳定性专项测试。

### 功能设计

- [`features/M4-keyboard-mapping.md`](features/M4-keyboard-mapping.md)：电脑键盘到 MIDI note 的映射能力与边界。
- [`features/M8-midi-file-and-freepiano-gap.md`](features/M8-midi-file-and-freepiano-gap.md)：MIDI 文件导入、回放兼容性、剩余边界与 FreePiano 差距。
- [`features/M7-layout-presets.md`](features/M7-layout-presets.md)：布局 preset 的文件格式、导入/保存/重命名/删除与恢复行为。
- [`features/M6-recording-playback.md`](features/M6-recording-playback.md)：录制 / 回放 / MIDI 导出的第一版设计与当前 MVP 行为。
- [`features/M3-plugin-hosting.md`](features/M3-plugin-hosting.md)：VST3 插件扫描、加载、处理、editor 和生命周期行为。

### 旧代码迁移

- [`architecture/legacy-migration.md`](architecture/legacy-migration.md)：旧 FreePiano 源码的迁移边界与新旧模块对应关系。

### 历史资料

- `archive/`：历史迁移记录、旧规划文档与过期资料。

## 当前目录职责

| 目录 | 职责 |
|---|---|
| `index/` | 文档导航、doc-map、术语表。 |
| `getting-started/` | 项目概览、快速开始、当前状态入口。 |
| `development/` | 开发环境、构建、日常工作流、故障排查、协作规则入口。 |
| `architecture/` | 系统架构、模块职责、迁移边界与内部设计。 |
| `features/` | 功能行为说明；当前先作为入口，后续逐步从 roadmap/testing 中提炼。 |
| `testing/` | 阶段验收、回归测试、专项手工测试。 |
| `decisions/` | ADR，记录重要架构和工程决策。 |
| `roadmap/` | 路线图、当前迭代与后续任务规划。 |
| `archive/` | 历史文档、旧方案、已被替代的资料。 |

## 文档职责原则

- [`roadmap/roadmap.md`](roadmap/roadmap.md) 是唯一的项目状态与路线图来源。
- [`roadmap/current-iteration.md`](roadmap/current-iteration.md) 只记录当前正在做的任务。
- [`architecture/overview.md`](architecture/overview.md) 只描述当前架构，不混入任务清单。
- `testing/` 下只放验收与测试，不混入开发规划。
- `archive/` 中的内容只作为历史参考，当前信息以现行文档为准。

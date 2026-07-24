# devpiano

中文 | [English](README_en.md)

**devpiano** 是一款持续演进的电脑键盘钢琴应用——以 JUCE 为框架，VST3 插件为核心音源，聚焦软件键盘演奏与 MIDI 文件处理。

项目定位、核心能力与明确非目标详见 [`docs/reference/project-scope.md`](docs/reference/project-scope.md)。

本仓库是 devpiano 的主源码与文档仓库。

---

## 当前状态

当前主干已具备以下能力：

- JUCE GUI 应用可构建并启动
- 音频输出设备通过 JUCE 设备管理流程初始化
- 电脑键盘可触发基础 MIDI note
- 可调整主音量与 ADSR 参数
- 可显示虚拟钢琴键盘
- 可扫描 VST3 目录并列出插件名称
- 可加载已扫描的 VST3 插件实例并参与音频处理
- 可打开插件编辑器窗口（若插件提供 editor）
- 基础设置可持久化，包括音频设备状态、性能参数、输入映射与插件恢复信息
- 已支持布局 Preset 保存、导入、重命名、删除与启动恢复
- 已支持录制、停止、回放与 MIDI 文件导出 MVP 闭环
- 已支持 MIDI 文件导入、自动选轨、导入后回放与 playback 边界收敛
- 已支持录制/回放相关按钮状态统一管理，Recording 期间禁用 Import MIDI，Playing 期间仍可安全导入另一个 MIDI 替换 playback
- 已支持 MIDI playback 时虚拟键盘实时联动，以及主窗口尺寸持久化恢复
- 已支持运行时界面语言切换（中文/英文），JUCE `Translation` 机制替换旧 `language_strdef.h` 体系
- 已支持 VST3 离线 WAV 导出（非 UI 线程渲染 + 进度对话框）
- 已支持播放速度精确控制（Slider + atomic 线程安全）
- 已支持拖放文件（`.devpiano`/`.mid`/`.devpiano.preset`/`.vst3`），蓝色边框反馈
- `MainComponent.cpp` 从约 1587 行降至约 606 行，职责已下沉到专门模块
- 自动化单元测试框架已就位（`cmake -DBUILD_TESTS=ON` → `devpiano_tests`）

---

## 当前实现架构概览

当前主干可以粗略分为以下几层：

- `source/Main.cpp`
  - JUCE 应用入口与主窗口创建
- `source/MainComponent.*`
  - 主装配层，负责连接音频设备、输入、插件、设置与各 UI 面板
- `source/Audio/`
  - `AudioEngine`：处理 MIDI 汇总、插件音频处理与 fallback 内置合成器
- `source/Recording/`
  - `RecordingEngine` / `MidiFileExporter` / `MidiFileImporter`：处理演奏事件录制、回放调度、MIDI 文件导出与 MIDI 文件导入
- `source/Plugin/`
  - `PluginHost`：负责插件格式管理、VST3 扫描、实例加载、prepare/release 与卸载
- `source/Input/`
  - `KeyboardMidiMapper`：将电脑键盘输入转换为 MIDI note on/off
- `source/Locale/`
  - `LocaleManager`：语言切换基础设施，JUCE `LocalisedStrings` 激活与管理
- `source/Midi/`
  - `MidiChannelMapper`：将 MIDI 消息路由到独立的 MIDI 通道
- `source/UI/`
  - `HeaderPanel`、`PluginPanel`、`ControlsPanel`、`KeyboardPanel`、`PluginEditorWindow`
  - `ControlsPanel` 已统一管理 Record / Play / Stop / Back / Import MIDI / Export MIDI / Export WAV 按钮状态
- `source/Settings/`
  - `SettingsModel`、`SettingsStore`、`SettingsComponent`：负责设置建模、持久化与设置界面
- `source/Core/`
  - 放置轻量核心类型与状态聚合结构，如键位模型、MIDI 类型、AppState

当前主音频路径为：

```text
电脑键盘 -> MidiMessageCollector / MidiKeyboardState
-> AudioEngine
-> 已加载 VST3 插件（优先）或内置 Sine Synth（fallback）
-> JUCE 音频设备输出
```

---

## 目录结构

### 当前主代码
- `source/`
  - 当前 JUCE 主实现目录
  - 所有新的 `.cpp/.h` 应优先放在这里
  - 当前已覆盖键盘输入、插件宿主、录制/回放/导出、MIDI 文件导入与 UI 面板分层

### 外部子模块
- `submodules/`
  - 所有第三方依赖统一放置于此
  - `submodules/JUCE/` — JUCE 框架（AGPLv3 / 商业许可）
  - `submodules/JIVE/` — JIVE 声明式 UI 框架（MIT）
  - `submodules/melatonin_inspector/` — 运行时 Component 检查器（MIT）
  - **不要修改子模块中任何代码**

### 文档目录
- `docs/`
  - 项目目标、评估、规划、任务清单、测试用例、里程碑清单、M8 MIDI 文件导入与回放边界等文档
  - 当前 M8 已收尾，权威状态以 `docs/roadmap/roadmap.md` 与 `docs/roadmap/current-iteration.md` 为准

---

## 开发入口（WSL + Windows MSVC 混合工作流）

日常开发建议：

- 在 **WSL 主工作树**中编辑、搜索、运行脚本
- 用 `build-wsl-clang` 生成 `compile_commands.json` 供 clangd 使用
- 同步到 Windows 镜像树 `G:\source\projects\devpiano`
- 再用 Windows 的 **Developer PowerShell for VS** 环境做 MSVC 验证构建

```bash
# 自检当前开发环境
./scripts/dev.sh self-check

# 格式化 source/ 下所有 .cpp/.h
./scripts/dev.sh format

# 检查格式合规（CI 模式）
./scripts/dev.sh format --check

# WSL 本地 configure / build（Debug）
./scripts/dev.sh wsl-build --configure-only

# WSL 本地 configure / build（Release）
./scripts/dev.sh wsl-build --release --configure-only

# 运行单元测试（配置 BUILD_TESTS=ON → 构建 → 执行）
./scripts/dev.sh test

# Windows MSVC 验证构建（Debug，内置同步）
./scripts/dev.sh win-build

# Windows MSVC 验证构建（Release，内置同步）
./scripts/dev.sh win-build --release
```

更多细节见：

- [docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- [docs/guides/quickstart.md](docs/guides/quickstart.md)

> 说明：当前项目采用 **WSL 主工作树 + Windows 镜像树 + MSVC 验证** 的混合工作流。

---

## 构建工作流

### 依赖
- WSL：CMake 3.22+、Ninja、Clang/clangd
- 已初始化所有 git 子模块（`git submodule update --init --recursive`）

### 推荐命令

先自检当前环境：

```bash
./scripts/dev.sh self-check
```

格式化代码：

```bash
./scripts/dev.sh format
```

仅刷新 WSL configure / `compile_commands.json`：

```bash
./scripts/dev.sh wsl-build --configure-only
```

WSL 本地构建：

```bash
./scripts/dev.sh wsl-build
```

运行单元测试：

```bash
./scripts/dev.sh test
```

Windows MSVC 验证构建（内置同步，不需要单独 win-sync）：

```bash
./scripts/dev.sh win-build
```

Release 构建（WSL / Windows）：

```bash
./scripts/dev.sh wsl-build --release
./scripts/dev.sh win-build --release
```

### 当前主要产物路径

- WSL Debug：`build-wsl-clang/devpiano_artefacts/Debug/DevPiano`
- WSL Release：`build-wsl-clang-release/devpiano_artefacts/Release/DevPiano`
- Windows Debug：`G:\source\projects\devpiano\build-win-msvc\devpiano_artefacts\Debug\DevPiano.exe`
- Windows Release：`G:\source\projects\devpiano\build-win-msvc-release\devpiano_artefacts\Release\DevPiano.exe`

### 相关文档

- [docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- [docs/guides/quickstart.md](docs/guides/quickstart.md)
- [docs/reference/features/midi-file-import.md](docs/reference/features/midi-file-import.md)

---

## 当前推荐阅读顺序

完整文档入口见：[docs/README.md](docs/README.md)。

如果要了解项目当前规划，建议按以下顺序阅读：

- 快速恢复：[docs/guides/quickstart.md](docs/guides/quickstart.md)
- 详细工作流：[docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- 当前架构：[docs/reference/architecture.md](docs/reference/architecture.md)
- 项目定位：[`docs/reference/project-scope.md`](docs/reference/project-scope.md)
- 路线图与项目状态：[docs/roadmap/roadmap.md](docs/roadmap/roadmap.md)
- 当前迭代任务：[docs/roadmap/current-iteration.md](docs/roadmap/current-iteration.md)
- 阶段验收标准：[docs/reference/acceptance.md](docs/reference/acceptance.md)
- MIDI 文件导入与回放边界：[docs/reference/features/midi-file-import.md](docs/reference/features/midi-file-import.md)
- 键盘映射：[docs/reference/features/keyboard-mapping.md](docs/reference/features/keyboard-mapping.md)
- 布局 Preset：[docs/reference/features/layout-presets.md](docs/reference/features/layout-presets.md)
- 录制/回放：[docs/reference/features/recording-playback.md](docs/reference/features/recording-playback.md)
- 插件宿主：[docs/reference/features/plugin-hosting.md](docs/reference/features/plugin-hosting.md)
- 性能持久化：[docs/reference/features/performance-persistence.md](docs/reference/features/performance-persistence.md)
- VST3 离线渲染：[docs/reference/features/plugin-offline-rendering.md](docs/reference/features/plugin-offline-rendering.md)

---

## 开发注意事项

- 保持代码极简、现代、跨平台
- 不修改 `submodules/` 中的任何代码
- 修改后尽量通过 CMake 构建验证
- 迁移旧逻辑时优先"提炼行为"，不要直接搬运平台绑定实现

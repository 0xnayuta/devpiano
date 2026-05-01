# devpiano

中文 | [English](README_en.md)

基于 **JUCE + CMake + C++20** 的 FreePiano 现代化重构项目。

目标是将老旧的 Windows FreePiano 重构为一个更简洁、现代、可持续演进的音频/MIDI 应用，并逐步替换旧项目中的：

- 原生音频后端（WASAPI / ASIO / DirectSound）
- 旧式 VST 宿主逻辑
- Windows 原生 GUI / GDI
- 旧配置与序列化方式

新的主方向是：

- 使用 JUCE `AudioDeviceManager` 管理音频设备
- 使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance` 构建插件宿主
- 使用 JUCE `Component` 树构建 UI
- 使用 JUCE `ValueTree` / `ApplicationProperties` 管理状态与配置

---

## 当前状态

当前主干已经具备以下能力：

- JUCE GUI 应用可构建并启动
- 已使用 JUCE 音频设备管理流程初始化输出设备
- 电脑键盘可触发基础 MIDI note
- 可打开外部 MIDI 输入并转发到引擎
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
- 当前 M8 已收尾，M8-5 merge-all 已搁置，M6-6e 作为后续 backlog

当前仍在持续完善的能力：

- 更完整的键盘映射编辑能力
- 布局 Preset 的冲突提示、图形化编辑等体验增强
- WAV 离线渲染、复杂编辑、tempo map 等 M6+ 高级功能
- 更清晰的正式 UI 分层与交互细节
- 更系统化的稳定性验证与回归测试
- M8 后续 backlog，例如 M6-6e VST3 离线渲染与外部 MIDI 硬件验证

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
- `source/Midi/`
  - `MidiRouter`：打开外部 MIDI 输入并转发到 `MidiMessageCollector`
- `source/UI/`
  - `HeaderPanel`、`PluginPanel`、`ControlsPanel`、`KeyboardPanel`、`PluginEditorWindow`
  - 当前 `ControlsPanel` 已统一管理 Record / Play / Stop / Back / Import MIDI / Export MIDI / Export WAV 按钮状态
- `source/Settings/`
  - `SettingsModel`、`SettingsStore`、`SettingsComponent`：负责设置建模、持久化与设置界面
- `source/Core/`
  - 放置轻量核心类型与状态聚合结构，如键位模型、MIDI 类型、AppState

当前主音频路径为：

```text
电脑键盘 / 外部 MIDI -> MidiMessageCollector / MidiKeyboardState
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

### JUCE 子模块
- `JUCE/`
  - JUCE 框架子模块
  - **不要修改**

### 旧版参考源码
- `freepiano-src/`
  - 旧版 FreePiano 源码，仅作为迁移参考
  - **不参与当前主构建**
  - **不要直接复制其中平台相关实现到新架构中**

### 文档目录
- `docs/`
  - 项目目标、评估、规划、任务清单、测试用例、里程碑清单、M8 MIDI 文件导入与回放边界等文档
  - 当前 M8 已收尾，权威状态以 `docs/roadmap/roadmap.md` 与 `docs/roadmap/current-iteration.md` 为准

### 旧原型与过渡代码
- `source/Legacy/UnusedPrototypes/`
  - 当前不参与主构建的旧版/过渡版原型代码
  - 仅用于历史保留与迁移参考

---

## 开发入口（WSL + Windows MSVC 混合工作流）

日常开发建议：

- 在 **WSL 主工作树**中编辑、搜索、运行脚本
- 用 `build-wsl-clang` 生成 `compile_commands.json` 供 clangd 使用
- 同步到 Windows 镜像树 `G:\source\projects\devpiano`
- 再用 Windows 的 **Developer PowerShell for VS** 环境做 MSVC 验证构建

推荐入口命令：

```bash
# 自检当前开发环境
./scripts/dev.sh self-check

# WSL 本地 configure / build
./scripts/dev.sh wsl-build --configure-only

# Windows MSVC 验证构建（内置同步，不需要单独 win-sync）
./scripts/dev.sh win-build
```

更多细节见：

- [docs/development/wsl-windows-msvc-workflow.md](docs/development/wsl-windows-msvc-workflow.md)
- [docs/getting-started/quickstart.md](docs/getting-started/quickstart.md)

## 构建方式

> 说明：当前项目采用 **WSL 主工作树 + Windows 镜像树 + MSVC 验证** 的混合工作流。

### 依赖
- WSL：CMake 3.22+、Ninja、Clang/clangd
- Windows：Visual Studio 2026、MSVC、CMake、Ninja、PowerShell 7（推荐）
- 已初始化 JUCE 子模块

### 推荐命令

先自检当前环境：

```bash
./scripts/dev.sh self-check
```

仅刷新 WSL configure / `compile_commands.json`：

```bash
./scripts/dev.sh wsl-build --configure-only
```

WSL 本地构建：

```bash
./scripts/dev.sh wsl-build
```

Windows MSVC 验证构建（内置同步，不需要单独 win-sync）：

```bash
./scripts/dev.sh win-build
```

### 当前主要产物路径

- WSL：`build-wsl-clang/devpiano_artefacts/Debug/DevPiano`
- Windows：`G:\source\projects\devpiano\build-win-msvc\devpiano_artefacts\Debug\DevPiano.exe`

### 相关文档

- [docs/development/wsl-windows-msvc-workflow.md](docs/development/wsl-windows-msvc-workflow.md)
- [docs/getting-started/quickstart.md](docs/getting-started/quickstart.md)
- [docs/features/phase4-midi-file-import.md](docs/features/phase4-midi-file-import.md)
- [docs/testing/phase4-midi-file-import.md](docs/testing/phase4-midi-file-import.md)

---

## 旧代码使用原则

`freepiano-src/` 的定位是：**迁移参考资料**，不是当前实现的一部分。

请遵循以下原则：

- 可以阅读旧代码以理解历史功能和行为
- 不要直接把旧的 Windows 专用实现照搬进新代码
- 不要重新引入 `windows.h` 依赖链作为核心方案
- 旧模块应被“提炼逻辑、重建设计”，而不是原样复刻

典型替代方向：

- `output_wasapi.* / output_asio.* / output_dsound.*`
  -> JUCE `AudioDeviceManager`
- `synthesizer_vst.*`
  -> JUCE `AudioPluginFormatManager` + `AudioPluginInstance`
- `gui.*`
  -> JUCE `Component`
- `config.*`
  -> JUCE `ApplicationProperties` + `ValueTree`

更多映射说明见：

- [docs/architecture/legacy-migration.md](docs/architecture/legacy-migration.md)
- [docs/features/phase4-midi-file-import.md](docs/features/phase4-midi-file-import.md)

---

## 当前推荐阅读顺序

完整文档入口见：[docs/README.md](docs/README.md)。

如果要了解项目当前规划，建议按以下顺序阅读：

- 快速恢复：[docs/getting-started/quickstart.md](docs/getting-started/quickstart.md)
- 详细工作流：[docs/development/wsl-windows-msvc-workflow.md](docs/development/wsl-windows-msvc-workflow.md)
- 当前架构：[docs/architecture/overview.md](docs/architecture/overview.md)
- 路线图与项目状态：[docs/roadmap/roadmap.md](docs/roadmap/roadmap.md)
- 当前迭代任务：[docs/roadmap/current-iteration.md](docs/roadmap/current-iteration.md)
- 阶段验收标准：[docs/testing/acceptance.md](docs/testing/acceptance.md)
- MIDI 文件导入与回放边界：[docs/features/phase4-midi-file-import.md](docs/features/phase4-midi-file-import.md)
- MIDI 文件导入专项测试：[docs/testing/phase4-midi-file-import.md](docs/testing/phase4-midi-file-import.md)
- 专项测试：
  - [docs/testing/phase2-keyboard-mapping.md](docs/testing/phase2-keyboard-mapping.md)
  - [docs/testing/phase3-layout-presets.md](docs/testing/phase3-layout-presets.md)
  - [docs/testing/phase3-recording-playback.md](docs/testing/phase3-recording-playback.md)
  - [docs/testing/phase4-midi-file-import.md](docs/testing/phase4-midi-file-import.md)
  - [docs/testing/phase2-plugin-host-lifecycle.md](docs/testing/phase2-plugin-host-lifecycle.md)

---

## 开发注意事项

- 保持代码极简、现代、跨平台
- 新代码放入 `source/` 结构化子目录中
- 不修改 `JUCE/` 中的任何代码
- 修改后尽量通过 CMake 构建验证
- 迁移旧逻辑时优先“提炼行为”，不要直接搬运平台绑定实现
- 当前 M8 已收尾：MIDI 文件导入、自动选轨、导入后回放、Import MIDI 按钮状态收敛、playback 边界与主窗口尺寸恢复均已通过人工验收；M8-5 merge-all 已搁置，M6-6e 作为后续 backlog

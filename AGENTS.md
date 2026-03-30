# Project: FreePiano-JUCE
你正在协助我将老旧的 Windows FreePiano 项目重构为基于 JUCE 框架的现代 C++ 音频应用。
开发环境：CMake + Ninja（使用 Visual Studio 2026 的 MSVC 工具链）。

## 目录结构说明：
- `/JUCE/`：JUCE 框架的 git 子模块，不要修改里面的任何代码。
- `/Source/`：所有的源代码（.cpp, .h）必须存放在这个目录下。
- 根目录：用于存放顶层 `CMakeLists.txt` 和构建脚本。

## 核心架构要求：
1. 音频/MIDI 后端：使用 JUCE 的 `AudioDeviceManager` 替代原有的原生 WASAPI/ASIO 代码。
2. 插件宿主：使用 JUCE 的 `AudioPluginFormatManager` (支持 VST3/VST) 替代旧的 VST 加载逻辑。
3. 键盘输入：利用 JUCE 的 `KeyListener` 捕获电脑键盘事件，映射为 `MidiMessage`。
4. UI：使用 JUCE 的 `Component` 树替代原 Windows GDI/原生控件。

## 工作流规范：
- 保持代码极简、现代 (C++20/23)。
- 使用 `bash` 工具执行 `cmake --build .` 来验证编译。
- 使用 `read` 和 `edit` 逐步从旧仓库提取逻辑，不要一次性读取过多旧代码。
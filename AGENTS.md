# Project: FreePiano-JUCE
你正在协助我将老旧的 Windows FreePiano 项目重构为基于 JUCE 框架的现代 C++ 音频应用。
开发环境：CMake + Ninja（使用 Visual Studio 2026 的 MSVC 工具链）。

## 强制命令执行规则：
当你使用 `bash` 或 `run` 工具执行以下类型的命令时，**必须**在命令最前面加上 `rtk ` 前缀：
1. **编译与构建**：如 CMake 生成、MSBuild 编译、Ninja 构建。
	- ❌ 错误：`cmake --build build`
	- ✅ 正确：`rtk cmake --build build`
2. **文件内容读取**：当你必须使用 shell 查看超大文件或日志时。
	- ❌ 错误：`cat verbose.log` 或 `Get-Content CMakeCache.txt`
	- ✅ 正确：`rtk cat verbose.log`
3. **依赖安装与包管理**：
	- ❌ 错误：`vcpkg install xxx` 或 `npm install`
	- ✅ 正确：`rtk vcpkg install xxx`
4. **全局搜索与长列表**：
	- ✅ 正确：`rtk dir -Recurse` 或 `rtk Get-ChildItem -Recurse`

对于简单的、低输出的命令（如 `cd`, `mkdir`, `git status`），你可以正常执行，不需要加 `rtk`。

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
- 使用 `bash` 工具执行 `rtk cmake --build .` 来验证编译。
- 使用 `read` 和 `edit` 逐步从旧仓库提取逻辑，不要一次性读取过多旧代码。
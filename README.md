# devpiano

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

当前主干已经具备以下基础能力：

- JUCE GUI 应用可正常启动
- 音频设备可初始化
- 电脑键盘可触发基础 MIDI note
- 可打开外部 MIDI 输入
- 可调整基础音量与 ADSR 参数
- 可显示虚拟钢琴键盘
- 可扫描 VST3 目录并列出插件名称
- 基础设置可持久化

当前仍未完成的核心能力：

- 真实的 VST3 插件实例加载与发声
- 完整可配置的键盘映射系统
- 布局切换 / Preset
- 录制 / 回放 / 导出
- 更清晰的正式 UI 分层

---

## 目录结构

### 当前主代码
- `Source/`
  - 当前 JUCE 主实现目录
  - 所有新的 `.cpp/.h` 应优先放在这里

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
- `Doc/`
  - 项目目标、评估、规划、任务清单、测试用例、里程碑清单等文档

### 旧原型与过渡代码
- `Source/Legacy/UnusedPrototypes/`
  - 当前不参与主构建的旧版/过渡版原型代码
  - 仅用于历史保留与迁移参考

---

## 构建方式

> 说明：项目已从 CMake+MSBuild 迁移到 CMake+Ninja，当前仅维护 Ninja 预设。

### 依赖
- Ninja 1.11+
- CMake 3.22+
- Visual Studio 2026（仅作为 MSVC 工具链来源，建议在 x64 Native Tools 命令行中执行）
- 已初始化 JUCE 子模块

### 常用命令

配置工程：

```bash
cmake --preset ninja-x64
```

构建 Debug：

```bash
cmake --build --preset ninja-debug
```

构建 Release：

```bash
cmake --build --preset ninja-release
```

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

- `Doc/LegacyCodeNotes.md`

---

## 当前推荐阅读顺序

如果要了解项目当前规划，建议按以下顺序阅读：

1. `Doc/OverallGoal.md`
2. `Doc/CurrentAssessmentAndPlan.md`
3. `Doc/NextPhaseTaskChecklist.md`
4. `Doc/MilestoneChecklist.md`
5. `Doc/KeyboardMappingTestCases.md`

---

## 开发注意事项

- 保持代码极简、现代、跨平台
- 新代码放入 `Source/` 结构化子目录中
- 不修改 `JUCE/` 中的任何代码
- 修改后尽量通过 CMake 构建验证
- 迁移旧逻辑时优先“提炼行为”，不要直接搬运平台绑定实现

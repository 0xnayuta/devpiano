# Architecture Decision Records

> 用途：说明 ADR（架构决策记录）的存放规范与索引。
> 更新时机：ADR 新增或索引变化时。

本目录用于记录已经形成的关键架构和工程决策（ADR）。

当前 ADR：

- [`ADR-001-wsl-primary-windows-mirror-workflow.md`](ADR-001-wsl-primary-windows-mirror-workflow.md)：采用 WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流。
- [`ADR-002-legacy-code-as-reference-only.md`](ADR-002-legacy-code-as-reference-only.md)：旧 FreePiano 源码只作为迁移参考（**已废止**，`freepiano-src/` 已移除）。
- [`ADR-003-pluginflowsupport-pure-functions.md`](ADR-003-pluginflowsupport-pure-functions.md)：`PluginFlowSupport` 必须保持为纯函数命名空间，不持成员变量，通过 callback 或参数显式注入依赖。
- [`ADR-004-juce-audiodevicemanager-audio-backend.md`](ADR-004-juce-audiodevicemanager-audio-backend.md)：使用 JUCE `AudioDeviceManager` 作为音频设备管理主路径，不复刻旧 WASAPI / ASIO / DirectSound 后端。
- [`ADR-005-vst3-first-plugin-hosting.md`](ADR-005-vst3-first-plugin-hosting.md)：使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance` 作为插件宿主抽象，以 VST3 为当前主路径，不复刻旧 VST SDK 风格宿主。
- [`ADR-006-remove-external-midi-support.md`](ADR-006-remove-external-midi-support.md)：移除外部 MIDI 设备支持，聚焦电脑键盘演奏场景，删除相关死代码。
- [`ADR-007-clang-tidy-check-only-clang-format-fixes.md`](ADR-007-clang-tidy-check-only-clang-format-fixes.md)：clang-tidy 只做检查（禁用 `--fix`），格式化与机械修复交给 clang-format 与人工。
- [`ADR-008-jive-declarative-ui-framework.md`](ADR-008-jive-declarative-ui-framework.md)：采用 JIVE 声明式 UI 框架驱动界面排版与弹窗体系。
- [`ADR-009-physical-modeling-piano-synthesis.md`](ADR-009-physical-modeling-piano-synthesis.md)：采用增强模态物理建模合成作为自主拥有、零采样依赖的默认内置钢琴音源。
- [`ADR-010-binary-asset-bundling-with-cmake.md`](ADR-010-binary-asset-bundling-with-cmake.md)：采用 CMake BinaryData 静态打包关键资产，消除外部路径依赖。
- [`ADR-011-modern-build-pipeline-optimization-and-compiler-cache.md`](ADR-011-modern-build-pipeline-optimization-and-compiler-cache.md)：现代 C++ 构建流水线深度优化（MSVC /Z7 + /FS、STL PCH 隔离架构、极速链接器与 -ftime-trace 分析工具链）。
- [`ADR-012-header-iwyu-and-granular-include-discipline.md`](ADR-012-header-iwyu-and-granular-include-discipline.md)：头文件 IWYU（Include What You Use）细粒度包含纪律，禁止头文件展开 `<JuceHeader.h>`。

ADR 应记录已确定的决策、原因和影响，不用于描述未决定的计划。

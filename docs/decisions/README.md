# Architecture Decision Records

本目录用于记录已经形成的关键架构和工程决策（ADR）。

当前 ADR：

- [`0001-wsl-primary-windows-mirror-workflow.md`](0001-wsl-primary-windows-mirror-workflow.md)：采用 WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流。
- [`0002-legacy-code-as-reference-only.md`](0002-legacy-code-as-reference-only.md)：旧 FreePiano 源码只作为迁移参考，不直接复刻平台绑定实现。
- [`0003-pluginflowsupport-pure-functions.md`](0003-pluginflowsupport-pure-functions.md)：`PluginFlowSupport` 必须保持为纯函数命名空间，不持成员变量，通过 callback 或参数显式注入依赖。

- [`0004-juce-audiodevicemanager-audio-backend.md`](0004-juce-audiodevicemanager-audio-backend.md)：使用 JUCE `AudioDeviceManager` 作为音频设备管理主路径，不复刻旧 WASAPI / ASIO / DirectSound 后端。
- [`0005-vst3-first-plugin-hosting.md`](0005-vst3-first-plugin-hosting.md)：使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance` 作为插件宿主抽象，以 VST3 为当前主路径，不复刻旧 VST SDK 风格宿主。

ADR 应记录已确定的决策、原因和影响，不用于描述未决定的计划。

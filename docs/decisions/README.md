# Architecture Decision Records

本目录用于记录已经形成的关键架构和工程决策（ADR）。

当前 ADR：

- `0001-wsl-primary-windows-mirror-workflow.md`：采用 WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流。
- `0002-legacy-code-as-reference-only.md`：旧 FreePiano 源码只作为迁移参考，不直接复刻平台绑定实现。

后续建议补充：

- 使用 JUCE `AudioDeviceManager` 替代旧原生音频后端。
- 插件宿主优先使用 JUCE `AudioPluginFormatManager` / VST3。

ADR 应记录已确定的决策、原因和影响，不用于描述未决定的计划。

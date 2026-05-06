# Development

本目录放置开发环境、构建、同步、日常工作流与协作规则相关文档。

当前已有文档：

- [`wsl-windows-msvc-workflow.md`](wsl-windows-msvc-workflow.md)：WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流。
- [`release-workflow.md`](release-workflow.md)：当前 Windows x64 正式 release、tag 与手工打包 checklist；Linux 暂作为后续待验证平台。
- [`troubleshooting.md`](troubleshooting.md)：WSL 构建、Windows 镜像同步、MSVC 验证构建的常见问题排查，已覆盖 `.vs` 被误删、SQLite WAL 文件、`--check` 预览模式等问题。

相关入口：

- 快速开始：[`../getting-started/quickstart.md`](../getting-started/quickstart.md)
- Agent 协作规则：仓库根目录 [`../../AGENTS.md`](../../AGENTS.md)

后续可按需补充：

- [`agent-collaboration.md`](agent-collaboration.md)：从 [`../../AGENTS.md`](../../AGENTS.md) 提炼给人类开发者阅读的协作规则摘要。

# ADR 0001: WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流

## 状态

已采用。

## 背景

项目在 WSL2 主工作树中进行日常开发，同时需要用 Windows 侧 Visual Studio / MSVC 工具链验证构建。JUCE / CMake / Ninja / clangd 的日常开发体验与 Windows MSVC 验证需求需要同时满足。

如果直接让 Windows/MSVC 长期跨边界读取 WSL 源码树进行构建，容易带来路径、缓存、性能和构建产物混杂问题。

## 决策

采用以下工作流：

- WSL 主工作树是唯一主源码来源。
- 日常编辑、搜索、脚本执行、clangd/LSP 均以 WSL 主工作树为准。
- Windows 镜像树只用于同步后的 MSVC 构建验证。
- 默认同步方向为 WSL -> Windows。
- WSL 构建目录与 Windows 构建目录分离：
  - WSL：`build-wsl-clang`
  - Windows 镜像：`build-win-msvc`
- 优先使用项目脚本入口：
  - `./scripts/dev.sh self-check`
  - `./scripts/dev.sh wsl-build --configure-only`
  - `./scripts/dev.sh wsl-build`
  - `./scripts/dev.sh win-sync`
  - `./scripts/dev.sh win-build`

## 影响

正面影响：

- WSL 主树保持单一源码事实来源。
- clangd / compile commands / WSL 构建路径保持稳定。
- Windows 侧 MSVC 验证不污染 WSL 主构建目录。
- 同步和验证流程可脚本化，降低手工操作差异。

代价：

- 需要维护 WSL -> Windows 镜像同步流程。
- Windows 验证依赖镜像树是否已同步。
- 开发者需要区分 WSL 主构建和 Windows 验证构建。

## 当前实践

现行详细工作流见：[`../development/wsl-windows-msvc-workflow.md`](../development/wsl-windows-msvc-workflow.md)。

快速恢复入口见：[`../getting-started/quickstart.md`](../getting-started/quickstart.md)。

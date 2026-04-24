# CMake + Ninja 迁移说明（历史记录）

> 说明：本文件保留为历史迁移记录。当前项目已经不再以旧的 `ninja-x64 / ninja-debug / ninja-release` 作为日常主工作流，而是采用 **WSL 主工作树 + Windows 镜像树 + MSVC 验证** 的混合工作流。

## 当前结论

CMake + Ninja 迁移已经完成，项目当前的标准开发方式是：

- 在 **WSL 主工作树**中编辑、搜索、运行脚本、使用 clangd/LSP
- 使用 `build-wsl-clang` 作为 WSL 构建目录与 `compile_commands.json` 来源
- 通过脚本同步到 Windows 镜像树 `G:\source\projects\devpiano`
- 在 Windows 的 **Developer PowerShell for VS** 环境中，对镜像树执行 MSVC 验证构建

## 当前标准命令

### 1. 自检环境

```bash
./scripts/dev.sh self-check
```

### 2. WSL configure / 刷新编译数据库

```bash
./scripts/dev.sh wsl-build --configure-only
```

### 3. WSL 本地构建

```bash
./scripts/dev.sh wsl-build
```

### 4. 同步到 Windows 镜像树

```bash
./scripts/dev.sh win-sync
```

### 5. Windows MSVC 验证构建

```bash
./scripts/dev.sh win-build
```

## 当前主要目录

- WSL 主工作树：`/root/repos/devpiano`
- WSL 构建目录：`/root/repos/devpiano/build-wsl-clang`
- Windows 镜像树：`G:\source\projects\devpiano`
- Windows 构建目录：`G:\source\projects\devpiano\build-win-msvc`

## 当前主要产物路径

- WSL：`build-wsl-clang/devpiano_artefacts/Debug/DevPiano`
- Windows：`build-win-msvc/devpiano_artefacts/Debug/DevPiano.exe`

## 为什么保留本文件

本文件仍然有两个用途：

1. 说明项目曾经历过从旧构建方式向 CMake + Ninja 的迁移
2. 提醒后续维护者：不要再回到旧的直接手动 preset 驱动方式作为默认日常流程

## 现行文档入口

请优先参考：

- `docs/dev-workflow-wsl-windows-msvc.md`
- `docs/quickstart-dev.md`

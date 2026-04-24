# devpiano: WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流

> 如需更短的恢复说明，请优先看：[`../getting-started/quickstart.md`](../getting-started/quickstart.md)

## 目标

- **WSL 工作树**：唯一主源码来源，负责日常编辑、grep/rg、脚本、clangd/LSP。
- **Windows 镜像树**：只用于同步后的 MSVC 构建验证。
- **构建目录隔离**：
  - WSL：`build-wsl-clang`
  - Windows 镜像：`build-win-msvc`
- **默认同步方向**：WSL -> Windows
- **不同步内容**：`.git`、IDE 私有缓存、构建目录、CMake 缓存、MSVC 中间产物/二进制产物。

## 目录/脚本

- `scripts/configure_wsl.sh`：在 WSL 中执行 CMake configure
- `scripts/build_wsl.sh`：在 WSL 中执行 configure + build
- `scripts/sync_to_win.sh`：从 WSL 调用 Windows PowerShell + robocopy，同步到 Windows 镜像树
- `scripts/build_msvc_from_wsl.sh`：从 WSL 触发“同步 + Windows MSVC 验证构建”
- `scripts/dev.sh`：统一入口，便于从一个命令分发 WSL 构建 / Windows 同步 / Windows 验证
- [`../getting-started/quickstart.md`](../getting-started/quickstart.md)：快速恢复环境与常用命令速查
- `tools/sync-to-win.ps1`：Windows 侧 robocopy 同步脚本（保留镜像树下的 `build-win-msvc`，避免每次同步清空 Windows 构建缓存）
- `tools/build-windows.ps1`：Windows 侧 Developer PowerShell for VS + CMake + Ninja 构建脚本

## CMake Presets

- `linux-clang-debug`
  - 生成目录：`build-wsl-clang`
  - 打开 `CMAKE_EXPORT_COMPILE_COMMANDS=ON`
- `windows-msvc-debug`
  - 生成目录：`build-win-msvc`
  - 预期在 Windows 的 Visual Studio Developer PowerShell 环境中执行

## 环境变量

推荐在 WSL shell 中设置：

```bash
export WIN_MIRROR_DIR='G:\source\projects\devpiano'
# 如果自动探测不到 Visual Studio 安装，可额外在 Windows 环境变量中设置：
# VS_DEVCMD_PATH=C:\Program Files\Microsoft Visual Studio\2026\...\Common7\Tools\VsDevCmd.bat
```

如果未设置 `WIN_MIRROR_DIR`，脚本会使用示例默认值：`G:\source\projects\devpiano`。

## 推荐日常命令速查表

```bash
# WSL 侧只刷新 configure / compile_commands.json
./scripts/dev.sh wsl-build --configure-only

# WSL 侧正常构建
./scripts/dev.sh wsl-build

# 仅同步到 Windows 镜像树
./scripts/dev.sh win-sync

# Windows MSVC 正常验证
./scripts/dev.sh win-build

# Windows 镜像已最新时，跳过同步直接验证
./scripts/dev.sh win-build --no-sync

# CMake / 工具链变化后，强制重新配置 Windows 构建树
./scripts/dev.sh win-build --reconfigure

# Windows 构建缓存脏了时，彻底清空后重建
./scripts/dev.sh win-build --clean-win-build
```

## 日常命令（WSL）

### 1. 配置 Linux/WSL 构建

```bash
./scripts/configure_wsl.sh
```

### 2. 在 WSL 中构建

```bash
./scripts/build_wsl.sh
# 或
./scripts/dev.sh wsl-build
```

常用选项：

```bash
# 只执行 configure，不编译
./scripts/build_wsl.sh --configure-only

# 删除 WSL CMakeCache.txt / CMakeFiles 后重新 configure/build
./scripts/build_wsl.sh --reconfigure

# 清空整个 WSL 构建目录后重建
./scripts/build_wsl.sh --clean
```

### 3. 仅同步到 Windows 镜像树

```bash
./scripts/sync_to_win.sh
# 或
./scripts/dev.sh win-sync
```

### 4. 触发 Windows MSVC 验证构建

```bash
./scripts/build_msvc_from_wsl.sh
# 或
./scripts/dev.sh win-build
```

常用选项：

```bash
# 跳过同步，直接使用当前镜像树构建
./scripts/build_msvc_from_wsl.sh --no-sync

# 只做同步，不触发 Windows 构建
./scripts/build_msvc_from_wsl.sh --sync-only

# 重新执行 Windows configure（删除 CMakeCache.txt / CMakeFiles）
./scripts/build_msvc_from_wsl.sh --reconfigure

# 清空整个 Windows 构建目录后重建
./scripts/build_msvc_from_wsl.sh --clean-win-build
```

## Windows 侧要求

1. 已安装 Visual Studio 2026（含 MSVC 工具链）
2. 已安装 CMake
3. 已安装 Ninja，并能在 Developer PowerShell for VS 环境中使用
4. `vswhere.exe` 可用，或手动设置 `VS_DEVCMD_PATH`
5. `WIN_MIRROR_DIR` 指向一个普通 Windows 目录，而不是 WSL 源码目录

## 建议

- 始终在 **WSL 主工作树**中编辑源码。
- 不要让 Windows 的 IDE / MSVC 长期直接跨边界读取 WSL 源码树进行构建。
- 不要把 Windows 镜像树下的 `build-win-msvc` 作为日常搜索、索引或上下文重点目录。
- 如果修改了 CMake 或工具链相关内容，先执行：

```bash
./scripts/build_wsl.sh
./scripts/build_msvc_from_wsl.sh
```

## 说明

当前脚本默认通过 Windows PowerShell / PowerShell 7 从 WSL 触发 Windows 侧同步与构建；同步使用 `robocopy`，并显式保留镜像树下的 `build-win-msvc`，请确保 `WIN_MIRROR_DIR` 指向的是**专门用于镜像的目录**。

主要 WSL 脚本已带统一日志前缀与颜色输出；如需关闭颜色，可设置 `NO_COLOR=1`。

`scripts/build_wsl.sh` 支持：`--configure-only`、`--reconfigure`、`--clean`。

`scripts/build_msvc_from_wsl.sh` 支持：`--no-sync`、`--sync-only`、`--reconfigure`、`--clean-win-build`，用于更细粒度地控制 Windows 侧验证流程。

`scripts/dev.sh` 提供统一入口：`wsl-configure`、`wsl-build`、`win-sync`、`win-build`。

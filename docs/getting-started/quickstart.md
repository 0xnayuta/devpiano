# devpiano quickstart-dev

## 目标

这份文档用于**快速恢复开发环境**。默认工作流：

- 在 **WSL** 中编辑、搜索、运行脚本、使用 clangd
- 同步到 **Windows 镜像树**：`G:\source\projects\devpiano`
- 在 **Windows Developer PowerShell for VS** 环境下用 MSVC 做验证构建

## 一次性前提

### WSL

安装基础工具：

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build clang clangd lld pkg-config \
  libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
  libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libxkbcommon-dev \
  libxkbcommon-x11-dev libx11-xcb-dev libxfixes-dev libxi-dev libxss-dev \
  libgl1-mesa-dev libglu1-mesa-dev mesa-common-dev
```

### Windows

确认已安装：

1. Visual Studio 2026 + C++ 桌面开发工具链
2. Ninja
3. CMake
4. `vswhere.exe`
5. PowerShell 7（推荐）

镜像目录：

- `G:\source\projects\devpiano`

## 建议写入 WSL shell 配置

写入 `~/.bashrc`：

```bash
export WIN_MIRROR_DIR='G:\source\projects\devpiano'
export WINDOWS_PWSH_EXE='/mnt/c/Program Files/PowerShell/7/pwsh.exe'
```

生效：

```bash
source ~/.bashrc
```

## 首次恢复后推荐验证顺序

### 0. 先跑自检

```bash
./scripts/dev.sh self-check
```


### 1. WSL configure

```bash
./scripts/dev.sh wsl-build --configure-only
```

### 2. WSL build

```bash
./scripts/dev.sh wsl-build
```

### 3. Windows MSVC 验证构建（内置同步，不需要单独 win-sync）

```bash
./scripts/dev.sh win-build
```

## 常用命令

```bash
# 快速检查当前环境是否满足混合工作流要求
./scripts/dev.sh self-check
```

```bash
# ── WSL 本地构建（Debug，默认） ──
./scripts/dev.sh wsl-build
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh wsl-build --reconfigure
./scripts/dev.sh wsl-build --clean

# ── WSL 本地构建（Release） ──
./scripts/dev.sh wsl-build --release
./scripts/dev.sh wsl-build --release --configure-only
./scripts/dev.sh wsl-build --release --reconfigure
./scripts/dev.sh wsl-build --release --clean

# ── 同步 ──
# 仅在需要单独同步时（一般不需要）
./scripts/dev.sh win-sync
./scripts/dev.sh win-sync --check  # 零写入预览

# ── Windows MSVC 验证（Debug，默认） ──
./scripts/dev.sh win-build
./scripts/dev.sh win-build --no-sync
./scripts/dev.sh win-build --reconfigure
./scripts/dev.sh win-build --clean-win-build

# ── Windows MSVC 验证（Release） ──
./scripts/dev.sh win-build --release
./scripts/dev.sh win-build --release --no-sync
./scripts/dev.sh win-build --release --reconfigure
./scripts/dev.sh win-build --release --clean-win-build
```

## 关键目录

- WSL 主工作树：`/root/repos/devpiano`
- WSL Debug 构建目录：`/root/repos/devpiano/build-wsl-clang`
- WSL Release 构建目录：`/root/repos/devpiano/build-wsl-clang-release`
- Windows 镜像树：`G:\source\projects\devpiano`
- Windows Debug 构建目录：`G:\source\projects\devpiano\build-win-msvc`
- Windows Release 构建目录：`G:\source\projects\devpiano\build-win-msvc-release`

## clangd

仓库根的 `.clangd` 已指向：

- `build-wsl-clang`

因此只要先执行一次：

```bash
./scripts/dev.sh wsl-build --configure-only
```

clangd 就能自动读取新的 `compile_commands.json`。

> **注意**：Release 构建（`--release`）同样会生成 `compile_commands.json`（位于 `build-wsl-clang-release`），但 `.clangd` 默认指向 Debug 目录。如需 clangd 索引 Release 配置，可临时修改 `.clangd` 或手动指定 `--compile-commands-dir`。

## 说明

- 日常只在 **WSL 主工作树**修改源码
- Windows 镜像树只用于 MSVC 验证
- 同步脚本会保留 Windows 侧 `build-win-msvc`，避免每次同步清空构建缓存
- 如日志颜色不需要，可设置：

```bash
export NO_COLOR=1
```

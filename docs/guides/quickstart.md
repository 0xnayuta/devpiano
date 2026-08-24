# devpiano quickstart-dev

> 用途：快速恢复开发环境与常用命令。
> 更新时机：开发环境、依赖、构建命令或工作流有实质变化时。

## 目标

这份文档用于**快速恢复开发环境**。默认工作流：

- 在 **WSL** 中编辑、搜索、运行脚本、使用 clangd
- 同步到 **Windows 镜像树**：`G:\source\projects\devpiano`
- 在 **Windows Developer PowerShell for VS** 环境下用 MSVC 做验证构建

## 一次性前提

### WSL


```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build clang clangd-21 clang-format-21 clang-tidy-21 lld mold ccache pkg-config \
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
# devpiano environment
export WIN_MIRROR_DIR='G:\source\projects\devpiano'
export WINDOWS_PWSH_EXE='/mnt/c/Program Files/PowerShell/7/pwsh.exe'
```

如果你的 Visual Studio 是 prerelease 安装或安装路径不在 C 盘默认位置，额外添加：

```bash
export VSINSTALLDIR='D:\Program Files\Microsoft Visual Studio\'
export WSLENV="${WSLENV:+$WSLENV:}VSINSTALLDIR"
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

## 常见问题与排查

### 1. JUCE 子模块未初始化

**现象**：构建失败，提示缺失 JUCE 头文件或 `JuceHeader.h` 找不到。

**原因**：克隆仓库后未初始化 git 子模块，`JUCE/` 目录为空或不完整。

**修复**：

```bash
git submodule update --init --recursive
```

如需更新到 develop 分支最新提交（本仓库推荐），在初始化后执行：

```bash
git submodule update --remote --merge JUCE
```

### 2. `juceaide` 子构建：CC flag 不识别

**现象**：`./scripts/dev.sh wsl-build --configure-only` 失败，错误信息：

```
cc: error: unrecognized command-line option '-Wshadow-all'; did you mean '-Wshadow'?
cc: error: unrecognized command-line option '-Wshorten-64-to-32'
```

**原因**：`CMakePresets.json` 中 `linux-clang-debug` preset 指定了 `CMAKE_C_COMPILER=clang`，但 JUCE 的 `juceaide` 内部子构建未继承该变量，fallback 到系统默认的 `cc`——Ubuntu 26.04 上 `/usr/bin/cc` 链接到 GCC-15。GCC 不认识 Clang 专属 warning flag，导致编译失败。

**修复**：将系统 `cc` 替代项指向 Clang：

```bash
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang-21 100
sudo update-alternatives --set cc /usr/bin/clang-21
```

验证：

```bash
cc --version
# 应输出 "Ubuntu clang version 21..."
```

修正后清除缓存重新 configure：

```bash
./scripts/dev.sh wsl-build --reconfigure
```

### 3. Windows MSVC 构建：vswhere 找不到 VS 2026

**现象**：`./scripts/dev.sh win-build` 报错：

```
build-windows.ps1: Index was outside the bounds of the array.
```

**原因**：VS 2026 是 prerelease 版本（`18.9.0-insiders`），`build-windows.ps1` 中 `vswhere` 调用缺 `-prerelease` 参数，找不到安装实例，返回 `[]`。在 `Set-StrictMode` 下触发数组越界异常。

**修复**：设置 `VSINSTALLDIR` 环境变量绕过 vswhere，并通过 `WSLENV` 透传给 Windows 进程：

写入 `~/.bashrc`：

```bash
export VSINSTALLDIR='D:\Program Files\Microsoft Visual Studio\'
export WSLENV="${WSLENV:+$WSLENV:}VSINSTALLDIR"
```

> **注意**：尾部 `\` 不可省略——`VsDevCmd.bat` 的子脚本通过拼接 `%VSINSTALLDIR%VC\...` 构造路径，缺少反斜杠会产生 `Microsoft Visual StudioVC\...` 无效路径。

生效后重新执行：

```bash
source ~/.bashrc
./scripts/dev.sh win-build
```

### 4. Windows MSVC 构建：`VsDevCmd.bat` 内部错误

**现象**：与上述问题 #3 同样的入口，同步成功但构建脚本报错：

```
[ERROR:VsDevCmd.bat] *** VsDevCmd.bat encountered errors. Environment may be incomplete and/or incorrect. ***
```

**原因**（两种可能）：

1. VS 安装为 prerelease 版本，`vswhere -latest` 无法定位（见问题 #3）
2. `VSINSTALLDIR` 尾部缺少 `\`，导致路径拼接错误（见问题 #3 注意项）

**修复**：确认 `VSINSTALLDIR` 包含尾部 `\`，且通过 `WSLENV` 透传。重新执行 `./scripts/dev.sh win-build`。

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
# 仅在需要单独同步时（win-build 已内置快速智能同步）
./scripts/dev.sh win-sync          # 快速智能同步（日常业务代码，< 1s）
./scripts/dev.sh win-sync --check  # 零写入预览
./scripts/dev.sh win-sync --full   # 强制全量同步（含 submodules，~9s）

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

# ── 代码格式 ──
./scripts/dev.sh format
./scripts/dev.sh format --check       # CI 模式，只检查不改

# ── 静态检查（clang-tidy，ADR-007：只检查，不用 --fix）──
./scripts/dev.sh tidy                 # 增量：仅未提交改动文件（秒级~分钟级）
./scripts/dev.sh tidy <file...>       # 指定文件/路径
./scripts/dev.sh tidy --all           # 全量 63 个编译单元（约 19 分钟，迭代边界跑）
# 编辑器内：clangd 已启用 clang-tidy 集成（.clangd），保存即增量波浪线提示；
# pre-commit 只做 format 检查（tidy 单文件 18-211s 实测，不阻塞提交）

# ── 单元测试 ──
./scripts/dev.sh test
# 测试二进制参数（直接运行 build-wsl-clang/devpiano_tests_artefacts/Debug/devpiano_tests）：
#   --include-juce        额外运行 JUCE 库自带内部测试（默认仅项目测试，节省 ~95s）
#   --include-files       不跳过 Files 类别（WSL root 下 JUCE Files 测试会失败）
#   --category <name>     仅运行指定类别（DevPiano/Core|Recording|Engine|UI）
#   --name <name>         仅运行指定名称的测试类
# ── 编译耗时性能剖析与火焰图分析 (-ftime-trace) ──
./scripts/dev.sh time-trace                   # 增量分析最新构建热点（最耗时文件/头文件/模板实例化）
./scripts/dev.sh time-trace --clean           # 清理后全量剖析并导出 Perfetto 火焰图 (combined_time_trace.json)
./scripts/dev.sh time-trace --target devpiano # 指定针对 GUI 主程序进行剖析

# ── 正式发布打包（Windows x64 zip + sha256） ──
./scripts/dev.sh package                      # 打包当前 Release 产物（自动解析 CMakeLists.txt 版本）
./scripts/dev.sh package --version 1.0.0
./scripts/dev.sh package --local-dist         # 输出至 WSL 本地 dist/ 目录
```
### 格式化与静态检查时机

| 阶段 | clang-format | clang-tidy |
| --- | --- | --- |
| 编辑期 | 编辑器 Format on Save | clangd 波浪线实时提示（`.clangd` 已启用 `Diagnostics.ClangTidy`，与全量 tidy 同源 `.clang-tidy` 配置） |
| 提交前 (pre-commit) | 自动检查 staged 文件（`--dry-run --Werror`，失败手动 `./scripts/dev.sh format`） | **不做**——单文件实测 18–211s，成本不成比例（AGENTS.md 第 3 节纪律） |
| 大迭代边界 | `./scripts/dev.sh format --check` | `./scripts/dev.sh tidy --all` 全量 0 诊断（约 19 分钟，唯一例行执行点） |
| CI（已落地，`.github/workflows/ci.yml`） | `format --check` 门禁（clang-format-21） | Linux Clang 单元测试门禁 + Windows MSVC 构建与单元测试门禁（push/PR 至 `main` 自动触发，ccache/sccache 加速） |

例外：修改 `.clang-tidy` / `.clang-format` 配置后，可用 `./scripts/dev.sh tidy <file>` 单文件快速验证配置效果。
决策背景：[`ADR-007`](../decisions/ADR-007-clang-tidy-check-only-clang-format-fixes.md)（clang-tidy 只做检查、clang-format 负责机械修复）。

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

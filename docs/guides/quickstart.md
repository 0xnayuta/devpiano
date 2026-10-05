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

WSL 主工作树只用于编辑源码与刷新 `compile_commands.json`；构建及软件测试在 Windows 镜像树执行。

### 2. Windows MSVC Debug 验证构建（内置同步，不需要单独 win-sync）

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
./scripts/dev.sh wsl-build --configure-only --reconfigure
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
# ── WSL 只执行 Debug configure（供 clangd 读取编译数据库） ──
./scripts/dev.sh wsl-build --configure-only

# ── 同步 ──
# 仅在需要单独同步时（win-build 已内置快速智能同步）
./scripts/dev.sh win-sync          # 快速智能同步（仅同步日常业务代码改动）
./scripts/dev.sh win-sync --check  # 零写入预览
./scripts/dev.sh win-sync --full   # 强制全量同步（含 submodules 全部文件）

# ── Windows MSVC 验证（Debug，默认） ──
./scripts/dev.sh win-build
./scripts/dev.sh win-build --no-sync
./scripts/dev.sh win-build --reconfigure
./scripts/dev.sh win-build --clean-win-build

# ── Windows MSVC Release（仅明确发布需求时） ──
./scripts/dev.sh win-build --release
./scripts/dev.sh win-build --release --no-sync
./scripts/dev.sh win-build --release --reconfigure
./scripts/dev.sh win-build --release --clean-win-build

# ── 代码格式 ──
./scripts/dev.sh format
./scripts/dev.sh format --check       # CI 模式，只检查不改

# ── clang-tidy 仅在迭代边界执行全量检查 ──
./scripts/dev.sh tidy --all

# ── 正式发布打包（Windows x64 zip / Linux x64 tar.gz + sha256） ──
./scripts/dev.sh package                      # 打包当前 Windows Release 产物（自动解析 CMakeLists.txt 版本）
./scripts/dev.sh package --version 1.0.0
./scripts/dev.sh package --local-dist         # 输出至 WSL 本地 dist/ 目录
./scripts/dev.sh package --linux              # 打包 Linux x64 Release 产物（tar.gz + sha256）
```

### Windows Debug 单元测试

在镜像树的 **Developer PowerShell for VS** 中执行（先用 `./scripts/dev.sh win-build` 同步并构建 Debug；如覆盖了 `WIN_MIRROR_DIR`，替换下面的默认路径）：

```powershell
Set-Location 'G:\source\projects\devpiano'
cmake --preset windows-msvc-debug -DBUILD_TESTS=ON
cmake --build --preset windows-msvc-debug --target devpiano_tests
ctest --test-dir build-win-msvc --output-on-failure
```

默认门禁选择 `DevPiano/` 前缀类别，`ChordRecognitionTest` 属于 `DevPiano/Core`。需确认实际执行范围时使用 `ctest --test-dir build-win-msvc --verbose` 或检查 `Testing/Temporary/LastTest.log`；不能把编译接入或另行 category 补跑当作默认覆盖。

文件测试使用 `devpiano::test::ScopedTempDir`，不构造默认生产日志或调用会创建真实用户目录的预设探针。音频 fixture 必须在 buffer 到达调用者后构造 `AudioSourceChannelInfo`，不依赖具名返回的可选 NRVO。

若需禁 NRVO 验证，或默认树仍有旧 Ninja 路径缓存，可在 **被同步保留的 `build-win-msvc/` 下**新建独立 Debug 树，不删除旧缓存。新树首次 configure 前设置 `CXXFLAGS` 中的 `/Zc:nrvo-`，并核对实际 compile command；已配置树不会重新读取该环境变量。具体构建、私有 TEMP/TMP、真实用户目录只读快照和消费者配方见 [Phase 0 实施记录](../roadmap/current-iteration.md#phase-0-实施记录与直接验证2026-10-02)。Windows JUCE 查询系统应用数据路径，仅覆盖 `APPDATA` 环境变量不构成可靠隔离。

最终软件验收沿用内容匹配的 Debug 子树构建 app/tests，再检查默认执行日志、用户目录快照与全量静态 receipt；实际 recipe 和未验证硬件范围见 [Phase H 复审入口](../reference/acceptance.md#audit-004-当前复审入口与契约边界)。独立子树通过不表示旧默认 Ninja 缓存已修复；不得为了确认原失败而重跑或删除用户缓存。

Linux CI 使用 `./scripts/dev.sh test`。在本地 WSL 主树执行该脚本会配置、编译并运行 `build-wsl-clang` 的测试，不适用于本项目 Windows 镜像验证工作流。

### 格式化与静态检查时机

| 阶段 | clang-format | clang-tidy |
| --- | --- | --- |
| 编辑期 | 编辑器 Format on Save | clangd 波浪线实时提示（`.clangd` 已启用 `Diagnostics.ClangTidy`，与全量 tidy 同源 `.clang-tidy` 配置） |
| 提交前 (pre-commit) | 自动检查 staged 文件（`--dry-run --Werror`，失败手动 `./scripts/dev.sh format`） | **不做**——全量静态分析成本高，由迭代边界与 CI 保证（AGENTS.md 第 3 节纪律） |
| 大迭代边界 | `./scripts/dev.sh format --check` | `./scripts/dev.sh tidy --all` 全量 0 诊断（迭代边界门禁，多核并行+本地缓存，唯一例行执行点） |
| CI（已落地，`.github/workflows/ci.yml`） | `format --check` 门禁（clang-format-21） | `tidy` 增量静态检查门禁（clang-tidy-21，actions/cache 加速）+ Linux Clang 单元测试门禁 + Windows MSVC 构建与单元测试门禁（push/PR 自动触发，ccache/sccache 加速） |

例外：修改 `.clang-tidy` / `.clang-format` 配置后，可用 `./scripts/dev.sh tidy <file>` 单文件快速验证配置效果。
决策背景：[`ADR-007`](../decisions/ADR-007-clang-tidy-check-only-clang-format-fixes.md)（clang-tidy 只做检查、clang-format 负责机械修复）。

## 关键目录

- WSL 主工作树：`~/repos/devpiano`
- WSL Debug 构建目录：`~/repos/devpiano/build-wsl-clang`
- WSL Release 构建目录：`~/repos/devpiano/build-wsl-clang-release`
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

# devpiano: WSL 主工作树 + Windows 镜像树 + MSVC 验证工作流

> 用途：说明 WSL 主工作树 + Windows 镜像树 + MSVC 验证的完整工作流与工具链。
> 更新时机：工作流步骤、同步工具或验证流程变化时。

> 如需更短的恢复说明，请优先看：[`quickstart.md`](quickstart.md)

## 目标

- **WSL 工作树**：唯一主源码来源，负责日常编辑、grep/rg、脚本、clangd/LSP。
- **Windows 镜像树**：只用于同步后的 MSVC 构建验证。
- **构建目录隔离**：
  - WSL Debug：`build-wsl-clang`
  - WSL Release：`build-wsl-clang-release`
  - Windows Debug：`build-win-msvc`
  - Windows Release：`build-win-msvc-release`
- **默认同步方向**：WSL -> Windows
- **不同步内容**：`.git`、IDE 私有缓存、构建目录、CMake 缓存、MSVC 中间产物/二进制产物。
  具体排除的目录和文件由 `tools/sync-to-win.ps1` 中的 `$excludeDirNames`、`$excludeSourceDirs`、`$excludeMirrorDirs` 与 `$excludeFiles` 控制，当前包括：

  **排除的目录（源端 + 镜像端）**
  - `.git`、`.vs`、`.idea`、`.vscode`
  - `build`、`build-*`、`out`、`bin`、`obj`、`CMakeFiles`
  - `.cache`、`__pycache__`
  - 镜像端：`build-win-msvc`、`build-win-msvc-release`、`dist`

  **排除的文件**
  - `CMakeCache.txt`、`compile_commands.json`
  - `*.suo`、`*.user`、`*.userosscache`、`*.rsuser`、`*.vcxproj.user`
  - `*.VC.db`、`*.VC.opendb`、`*.opendb`、`*.opensdf`、`*.sdf`、`*.ipch`
  - `*.db-shm`、`*.db-wal`、`*.db-journal`（SQLite WAL 模式文件，Browse.VC.db、CopilotIndices 等会生成）
  - `*.obj`、`*.iobj`、`*.pch`、`*.pdb`、`*.ilk`、`*.idb`、`*.exp`、`*.lib`、`*.dll`、`*.exe`
  - `*.a`、`*.so`、`*.dylib`、`*.app`、`*.out`、`*.o`、`*.lo`、`*.la`
  - `*.cache`、`*.tmp`、`*.log`、`*.binlog`

  > **注意**：IDE 状态目录（`.vs`、`.idea`、`.vscode`）在源端和镜像端**双方**都会被排除——这确保 robocopy `/MIR` 不会把镜像端 Windows 本地 IDE 生成的工作文件误认为"多余"而删除它。

## 目录/脚本

- `scripts/configure_wsl.sh`：在 WSL 中执行 CMake configure
- `scripts/build_wsl.sh`：在 WSL 中执行 configure + build
- `scripts/sync_to_win.sh`：从 WSL 调用 Windows PowerShell + robocopy，同步到 Windows 镜像树
- `scripts/build_msvc_from_wsl.sh`：从 WSL 触发"同步 + Windows MSVC 验证构建"
- `scripts/win_clean_guard.sh`：`--clean-win-build` 的删除前守卫（路径身份、目录内容检查，fail-closed）；
  测试：`bash scripts/tests/win_clean_guard_test.sh`（隔离可抛弃目录，不触碰真实构建目录）
- `scripts/dev.sh`：统一入口，便于从一个命令分发 WSL 构建 / Windows 同步 / Windows 验证
- [`quickstart.md`](quickstart.md)：快速恢复环境与常用命令速查
- `tools/sync-to-win.ps1`：Windows 侧 robocopy 同步脚本（保留镜像树下的 `build-win-msvc`，避免每次同步清空 Windows 构建缓存）
- `tools/build-windows.ps1`：Windows 侧 Developer PowerShell for VS + CMake + Ninja 构建脚本

## CMake Presets

| Preset | 生成目录 | 用途 |
|---|---|---|
| `linux-clang-debug` | `build-wsl-clang` | WSL 主树仅用于 Debug configure，生成 clangd 的 `compile_commands.json` |
| `linux-clang-release` | `build-wsl-clang-release` | Linux Release preset，非 WSL 主树日常工作流 |
| `windows-msvc-debug` | `build-win-msvc` | Windows MSVC 验证（默认） |
| `windows-msvc-release` | `build-win-msvc-release` | Windows MSVC Release 验证 |

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
# ── WSL 主树仅执行 Debug configure ──
./scripts/dev.sh wsl-build --configure-only   # 刷新 compile_commands.json

# ── Windows MSVC Debug（默认） ──
./scripts/dev.sh win-build                    # 正常验证（内置快速智能同步）
./scripts/dev.sh win-build --full             # 强制全量同步并验证构建
./scripts/dev.sh win-build --no-sync          # 镜像已最新时跳过同步
./scripts/dev.sh win-build --reconfigure      # 强制重新配置
./scripts/dev.sh win-build --clean-win-build  # 清空构建树后重建（含删除守卫）

# ── Windows MSVC Release（仅明确发布需求时） ──
./scripts/dev.sh win-build --release
./scripts/dev.sh win-build --release --no-sync
./scripts/dev.sh win-build --release --reconfigure
./scripts/dev.sh win-build --release --clean-win-build

# ── 同步 ──
./scripts/dev.sh win-sync --check  # 零写入预览
./scripts/dev.sh win-sync          # 快速智能同步（仅同步日常业务代码改动）
./scripts/dev.sh win-sync --full   # 强制全量同步（含 submodules 全部文件）
```

## 日常命令（WSL 调度，Windows 镜像树验证）

### 1. 刷新 WSL Debug 编译数据库

```bash
./scripts/dev.sh wsl-build --configure-only
```

WSL 主工作树只负责源码编辑和 clangd 编译数据库；不要在此运行 `wsl-build` 完整构建、Release 构建或 `dev.sh test`。日常 Debug 构建与软件测试在 Windows 镜像树进行（Windows 单测命令见 [`quickstart.md`](quickstart.md)）。

### 2. 仅同步到 Windows 镜像树

```bash
./scripts/sync_to_win.sh
# 或
./scripts/dev.sh win-sync

# 强制全量同步（包含 submodules 下全部文件）
./scripts/sync_to_win.sh --full
./scripts/dev.sh win-sync --full

# 预览模式：不复制/不删除任何文件，仅列出待同步变更（零写入）
./scripts/sync_to_win.sh --check
./scripts/dev.sh win-sync --check
```

### 4. 触发 Windows MSVC Debug 验证构建

```bash
./scripts/build_msvc_from_wsl.sh
# 或
./scripts/dev.sh win-build
```

常用选项：

```bash
# 跳过同步，直接使用当前镜像树构建
./scripts/build_msvc_from_wsl.sh --no-sync

# 强制全量同步（含 submodules）后构建
./scripts/build_msvc_from_wsl.sh --full

# 只做同步，不触发 Windows 构建
./scripts/build_msvc_from_wsl.sh --sync-only

# 重新执行 Windows configure（删除 CMakeCache.txt / CMakeFiles，保留 Ninja 缓存）
./scripts/build_msvc_from_wsl.sh --reconfigure

# 清空整个 Windows 构建目录后重建
./scripts/build_msvc_from_wsl.sh --clean-win-build

# 使用 Release preset（构建目录：build-win-msvc-release）
./scripts/build_msvc_from_wsl.sh --release
./scripts/build_msvc_from_wsl.sh --release --no-sync
```

## Windows 构建目录清理安全门禁

`--clean-win-build` 会删除整个 Windows 构建目录（`build-win-msvc` / `build-win-msvc-release`），
执行前必须通过 `scripts/win_clean_guard.sh` 的 fail-closed 校验：

- **路径身份**：镜像根必须规范化解析后位于 `/mnt/<盘符>/...`，目标必须是镜像根的**直接子目录**
  （逐组件判定，不用字符串前缀），目录名只允许 `build-win-msvc` 与 `build-win-msvc-release`；
  拒绝空路径、`/`、盘符根、WSL 原生路径（含 WSL 主源码根）、符号链接目标与非目录目标。
- **目录内容**：目标内每个顶层条目必须是已知 CMake/Ninja/MSVC/JUCE 构建产物；命中历史证据、
  归档或任何未知条目时**默认拒绝删除**并逐条打印。
- 确需删除时须显式追加 `--allow-non-build-assets`（二次确认，仅在确定这些条目已归档/可弃时使用），
  该参数必须与 `--clean-win-build` 同时出现。
- 守卫会打印最终解析路径、删除范围（顶层条目）与拒绝原因；未通过任何一条即不删除。

```bash
# 人工预检（只读，不删除）
bash scripts/win_clean_guard.sh /mnt/g/source/projects/devpiano build-win-msvc

# 行为测试（隔离临时目录；覆盖放行、路径逃逸、符号链接、证据目录与未知条目等用例，零失败）
bash scripts/tests/win_clean_guard_test.sh
```

**不要**把正式证据长期唯一保存在构建目录内：只要 `build-win-msvc` 中存在 `phase38-*` / `phase39-*`
等证据目录，`--clean-win-build` 就会拒绝执行，直到这些数据被归档。

## Windows 侧证据归档与目录约定

| 问题 | 约定 |
|---|---|
| 新的 Windows 构建产物放哪 | 镜像树 `build-win-msvc`（Debug）/ `build-win-msvc-release`（Release）；可随时重建，不对其做长期保证 |
| 临时 GUI 测试结果放哪 | 任务临时根（`G:\tmp\...` 或构建目录内的 `phaseNN-*` 工作目录）；验收后归档，随后可清理 |
| 哪类数据必须正式归档 | 原始参考 WAV/MIDI、真实 GUI 截图、实验输入与复算结果、运行日志、receipt/manifest、配置与版本证据、受控参考输出 |
| 什么时候需要独立备份 | 需要介质故障隔离时（异盘/异介质）。归档到 `G:\source\projects\devpiano-evidence` 与原件同盘，只解决"证据散落在构建目录与磁盘根"的问题，**不提供故障隔离** |
| 每次验收记录什么 | 源 commit、构建预设、工具链版本、生产/参考样本逐文件 SHA-256、批次时间戳；缺项显式标记 unknown |
| 什么算可重建产物 | CMake/Ninja/MSVC 中间文件、编译缓存、纯临时 EXE/对象文件、第三方工具运行时与可重新下载的安装包 |
| 什么必须保留 | 唯一实验输出（如修复前后基线的差异渲染）、真实 GUI 证据、失败/拒绝记录及其根因分析 |
| 何时允许清理临时目录 | 归档完成、源/目标 SHA-256 核验通过、引用已解除，并经人工批准（本项目接受同盘归档作为删除前提，决策记录见证据库 `_index\evidence-catalog.md` §6） |
| 历史绝对路径失效怎么办 | 查 `devpiano-evidence\_index\migration-map.json`（旧→新逐文件映射）；不就地改写历史记录 |
| 如何避免重复目录 | 归档目录沿用原始批次标识与时间戳，先查 `_index\classification.csv` 是否存在同批次 |
| 如何检查 MCP 部署未被破坏 | 核验 `G:\tools\windows-mcp\1.3.28\Sbroenne.WindowsMcp.exe` 的 SHA-256 与 `.omp\mcp.json` 的 `cwd`；详见 [windows-mcp-omp.md](windows-mcp-omp.md) |

归档根：`G:\source\projects\devpiano-evidence\`（在镜像树**之外**，因此不会被 `win-sync` 的
`/MIR` 删除；反向也不要把它并入镜像树）。索引与逐批次说明见该目录 `_index\` 与 `README.md`。

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
./scripts/build_wsl.sh --configure-only
./scripts/build_msvc_from_wsl.sh
```

## 说明

当前脚本默认通过 Windows PowerShell / PowerShell 7 从 WSL 触发 Windows 侧同步与构建；同步使用 `robocopy`，并显式保留镜像树下的 `build-win-msvc` 和 `build-win-msvc-release`，请确保 `WIN_MIRROR_DIR` 指向的是**专门用于镜像的目录**。

主要 WSL 脚本已带统一日志前缀与颜色输出；如需关闭颜色，可设置 `NO_COLOR=1`。

`scripts/build_wsl.sh` 支持：`--release`、`--configure-only`、`--reconfigure`、`--clean`。

`scripts/build_msvc_from_wsl.sh` 支持：`--release`、`--no-sync`、`--full`、`--sync-only`、`--reconfigure`、`--clean-win-build`、`--allow-non-build-assets`（仅随 `--clean-win-build` 使用，用于显式放行含非构建资产的构建目录），用于更细粒度地控制 Windows 侧验证流程。

`scripts/dev.sh` 提供统一入口：`wsl-configure`、`wsl-build`、`win-sync`、`win-build`。configure/build 子命令可用 `--release` 选择 Release（仅明确要求时）；同步只接受 `--check` / `--full`，不接受 `--release`。WSL 主工作树仍只做 Debug configure，软件构建与测试在 Windows 镜像树执行。

`scripts/sync_to_win.sh` / `scripts/dev.sh win-sync` 支持 `--check` 参数，以 `robocopy /L` 零写入模式列出待同步变更，不实际复制或删除任何文件。

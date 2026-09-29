# 开发环境与构建问题排查

> 用途：记录 WSL 构建、Windows 镜像同步、MSVC 验证等开发环境常见问题与解决方案。
> 更新时机：新增排查项、解决方案变化或已验证问题时。

> 当前推荐恢复入口见 [`quickstart.md`](quickstart.md)，详细 WSL / Windows / MSVC 工作流见 [`wsl-windows-msvc-workflow.md`](wsl-windows-msvc-workflow.md)。

---

## Windows 镜像同步问题

### `.vs` 目录（Visual Studio 工作状态）被同步脚本删除

**现象**：运行 `sync_to_win.sh` 后，镜像端的 `.vs` 目录（及其下的 `Browse.VC.db`、`CopilotIndices`、`FileContentIndex` 等文件）被 robocopy 识别为"多余"并尝试删除。VS 正在打开项目时会导致 "file in use" 错误。

**原因**：`robocopy /MIR` 做镜像同步——源码（WSDL）没有 `.vs`，镜像端（Windows）有 `.vs`，所以 robocopy 把 `.vs` 视为"多余"并尝试删除。脚本在源码端排除了 `.vs`，但没有在镜像端排除。

**修复**：已在 `tools/sync-to-win.ps1` 的 `$excludeDirPaths` 中添加镜像端 IDE 目录排除（`.vs`、`.idea`、`.vscode`），同步脚本不再处理这些目录。

**验证**：运行 `win-sync --check`，确认输出中不再出现 `.vs` 相关条目。

---

### 同步前预览变更，避免误操作

**方法**：使用 `--check` 参数以零写入模式预览待同步内容。

```bash
./scripts/dev.sh win-sync --check
```

这会调用 `robocopy /L`，列出所有待复制和待删除的文件，**不实际写入任何内容**。适合在执行前确认同步范围是否符合预期。

---

### SQLite WAL 文件（`.db-shm`、`.db-wal`）被同步脚本处理

**现象**：`sync_to_win.sh` 输出中出现了类似 `Browse.VC.db-shm`、`CodeChunks.db-wal` 等文件。

**原因**：这些是 SQLite WAL 模式产生的临时文件（`*-shm` 是共享内存文件，`*-wal` 是预写日志文件），正常情况下不应参与同步。

**修复**：已在 `tools/sync-to-win.ps1` 的 `$excludeFiles` 中添加 `*.db-shm`、`*.db-wal`、`*.db-journal`、`*.db-corrupt` 排除规则。

---

## WSL configure / build 问题

### 1. JUCE 子模块未初始化导致头文件缺失

**现象**：`./scripts/dev.sh wsl-build` 失败，提示缺失 JUCE 头文件或 `JuceHeader.h` 找不到。

**原因**：克隆仓库后未拉取 git 子模块，`JUCE/` 目录为空。

**修复**：

```bash
git submodule update --init --recursive
```

### 2. juceaide 子构建：CC flag 不识别（GCC vs Clang）

**现象**：`./scripts/dev.sh wsl-build --configure-only` 报错：

```text
cc: error: unrecognized command-line option '-Wshadow-all'; did you mean '-Wshadow'?
cc: error: unrecognized command-line option '-Wshorten-64-to-32'
```

**原因**：`CMakePresets.json` 中配置了 `CMAKE_C_COMPILER=clang`，但 JUCE 的 `juceaide` 辅助工具子构建在特定环境下 fallback 到系统的 `/usr/bin/cc`（如 Ubuntu 26.04 默认链接到 GCC-15）。GCC 无法识别 Clang 专属告警选项。

**修复**：使用 `update-alternatives` 将系统默认 `cc` 指向 Clang-21：

```bash
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang-21 100
sudo update-alternatives --set cc /usr/bin/clang-21
./scripts/dev.sh wsl-build --reconfigure
```

### 3. Ubuntu 26.04 单元测试文本渲染缺失（Noto CJK .ttc 扫描）

**现象**：Ubuntu 26.04 本地 Debug 运行单测时 `JiveRenderTest` 报错渲染可见像素为 0。

**原因与修复**：JUCE FreeType 字体扫描器仅匹配 `.ttf`/`.otf`，而 Ubuntu 26.04 的 `system-ui` 指向 Noto CJK `.ttc`。通过在 `~/.local/share/fonts` 建立 `.ttf` 镜像副本并 `fc-cache -f` 即可解决，详见 [`../issues/known-issues.md`](../issues/known-issues.md#ubuntu-2604-下-jive-文本不渲染juce-字体扫描不识别-ttcsystem-ui--noto-cjk)。
---

## MSVC 验证构建问题

### CMake 缓存未追踪源文件变更，导致旧目标文件未重新编译

**现象**：源文件已修改，但 `win-build` 报告 "ninja: no work to do"；调试时出现 `WeakReference::SharedPointer::get()` 访问冲突（`this == 0x100000000`），程序启动即崩溃。

**原因**：CMake 缓存（`build-win-msvc/CMakeCache.txt`）在某些情况下未正确记录源文件的修改时间戳，导致 Ninja 判断目标文件已是最新的，不再触发重新编译。链接时新旧目标文件混用，产生不一致状态（如 `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` 宏展开与实际类定义不匹配）。

**影响范围**：此问题与具体代码无关，是构建系统缓存问题。任何源文件变更后若缓存失效不完整，都可能出现类似症状。

**处理方法**：

1. **快速修复**（推荐）：使用 reconfigure 模式重新生成 Windows 构建系统

   ```bash
   ./scripts/dev.sh win-build --reconfigure
   # 或清空 Windows 构建目录重建：
   ./scripts/dev.sh win-build --clean-win-build
   ```

   亦可在 Windows 侧显式删除缓存文件：
   ```bash
   powershell.exe -Command "Remove-Item -Path 'G:\source\projects\devpiano\build-win-msvc\CMakeCache.txt' -Force"
   ./scripts/dev.sh win-build
   ```

2. **验证是否解决了问题**：重新编译应覆盖全部编译单元（主程序与测试目标），而不是只有零星 1-3 个增量文件。

3. **预防**：在 `win-build` 之后，务必确认输出中编译的文件数是否符合预期。若改动涉及 UI 文件（`.h`/`.cpp`）且只编译了零星几个文件，应按上述方法删除缓存后重新构建。

**诊断信号**：
- `ninja: no work to do` 但实际上源文件已修改
- 调试时出现 `WeakReference`、`SharedPointer` 相关的空指针/无效指针访问
- 崩溃堆栈指向 JUCE 内部 `Component::addChildComponent` / `internalHierarchyChanged` 等组件树构建阶段
- 崩溃仅出现在 Windows MSVC 侧，WSL clang 侧正常

**根因补充**：Windows MSVC 侧使用 UNC 路径（`\\wsl.localhost\Ubuntu\...`）访问 WSL 源文件，CMake 的文件指纹机制在跨文件系统（WSL/NTFS）场景下可能存在精度问题，导致缓存失效不完全。

**关联条目**：本问题属于构建系统环境问题，与 Phase 6-1 代码无关，但因调试 Phase 6-1 时暴露，故记录于此。

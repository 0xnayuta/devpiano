# 开发环境与构建问题排查

> 当前推荐恢复入口见 [`../getting-started/quickstart.md`](../getting-started/quickstart.md)，详细 WSL / Windows / MSVC 工作流见 [`wsl-windows-msvc-workflow.md`](wsl-windows-msvc-workflow.md)。

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

> 待补充

---

## MSVC 验证构建问题

### CMake 缓存未追踪源文件变更，导致旧目标文件未重新编译

**现象**：源文件已修改，但 `win-build` 报告 "ninja: no work to do"；调试时出现 `WeakReference::SharedPointer::get()` 访问冲突（`this == 0x100000000`），程序启动即崩溃。

**原因**：CMake 缓存（`build-win-msvc/CMakeCache.txt`）在某些情况下未正确记录源文件的修改时间戳，导致 Ninja 判断目标文件已是最新的，不再触发重新编译。链接时新旧目标文件混用，产生不一致状态（如 `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` 宏展开与实际类定义不匹配）。

**影响范围**：此问题与具体代码无关，是构建系统缓存问题。任何源文件变更后若缓存失效不完整，都可能出现类似症状。

**处理方法**：

1. **快速修复**（推荐每次）：删除 CMake 缓存后重新构建
   ```bash
   # 在 Windows 侧删除缓存
   powershell.exe -Command "Remove-Item -Path 'G:\source\projects\devpiano\build-win-msvc\CMakeCache.txt' -Force"
   ./scripts/dev.sh win-build
   ```

2. **验证是否解决了问题**：重新编译的文件数应接近全部编译单元数（55 个左右），而不是只有 1-3 个。

3. **预防**：在 `win-build` 之后，务必确认输出中编译的文件数是否符合预期。若改动涉及 UI 文件（`.h`/`.cpp`）且只编译了零星几个文件，应按上述方法删除缓存后重新构建。

**诊断信号**：
- `ninja: no work to do` 但实际上源文件已修改
- 调试时出现 `WeakReference`、`SharedPointer` 相关的空指针/无效指针访问
- 崩溃堆栈指向 JUCE 内部 `Component::addChildComponent` / `internalHierarchyChanged` 等组件树构建阶段
- 崩溃仅出现在 Windows MSVC 侧，WSL clang 侧正常

**根因补充**：Windows MSVC 侧使用 UNC 路径（`\\wsl.localhost\Ubuntu\...`）访问 WSL 源文件，CMake 的文件指纹机制在跨文件系统（WSL/NTFS）场景下可能存在精度问题，导致缓存失效不完全。

**关联条目**：本问题属于构建系统环境问题，与 Phase 6-1 代码无关，但因调试 Phase 6-1 时暴露，故记录于此。

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

> 待补充

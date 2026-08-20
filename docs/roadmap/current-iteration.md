# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**[待规划 / 新迭代准备]**

Phase 15（UI 架构统一至 JIVE：声明式弹窗与设置面板重构）已圆满完成（2026-08-19）：`JiveModalDialog` 通用弹窗基础设施、预设与歌曲信息弹窗统一迁移、`SettingsLayoutModel` 网格化设置面板重构、`WavExportTask` 声明式进度浮层全部落地，三闸门全绿，全量 3101 断言通过。历史细节已归档至 [`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)。

---

## Phase XX：[新迭代主题名称] [待启动]

### 背景与目标

<!-- 简要描述本轮迭代解决的核心问题、业务场景与预期交付成果 -->

1. **背景痛点**：...
2. **阶段目标**：
   - 目标 1：...
   - 目标 2：...

---

### 前置事实（已核实代码）

| 事实 | 位置 | 影响 |
|---|---|---|
| ... | `source/...` | ... |

---

### 子任务排期

**Phase XX-A：[子任务 A] [待启动]**

- [ ] 任务项 1
- [ ] 任务项 2

**Phase XX-B：[子任务 B] [待启动]**

- [ ] 任务项 1
- [ ] 任务项 2

**Phase XX-C：测试补齐、三闸门与 Windows 验证 [待启动]**

- [ ] 确定性单元测试补齐
- [ ] 三闸门验证（`wsl-build --configure-only` / `test` / `format --check` / `win-build`）
- [ ] Windows 侧手工回归清单验证

---

### 验收标准

- [ ] 验收条件 1
- [ ] 验收条件 2
- [ ] 全量单元测试通过，三闸门全绿

---

### 验证命令

```bash
# 刷新 WSL 配置 / compile_commands.json
./scripts/dev.sh wsl-build --configure-only

# 运行全量单元测试
./scripts/dev.sh test

# 检查代码格式
./scripts/dev.sh format --check

# Windows MSVC 验证构建
./scripts/dev.sh win-build
```

---

## 历史实现 Backlog

- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)
- Phase 10 完成记录：[`../archive/phase10-ui-modernization.md`](../archive/phase10-ui-modernization.md)

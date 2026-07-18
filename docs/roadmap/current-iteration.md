# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

架构优化 Backlog 七项（P0/P1/P2）已全部完成，累计 8 个提交（P1-B、P2-A、P2-B 为本次迭代新增）。

详情见 [`roadmap.md`](roadmap.md) §阶段路线图。

### 提交摘要

| 优先级 | 项目 | 提交 |
|---|---|---|
| P0-C | 最近文件列表 UI | `0454b75` (feat) / `3b7e16a` (fix) |
| P0-B | PluginOfflineRenderer 生命周期注释 | `0046435` |
| P0-A | PerformanceFile Base64 序列化 | `d40845c` (refactor) |
| P1-A | Diagnostics 日志层迁移 | `7c3f2c1` (refactor) |
| P1-B | WavExportOptions 独立头文件 | `4456f43` (refactor) |
| P2-A | SettingsComponent ValueTree::Listener | `af5644a` (refactor) |
| P2-B | MainComponent 瘦身 | `4b4e1af` (refactor) |

## 本轮决策

- **Phase 7-5（Metadata 编辑对话框）** — 明确搁置。`PerformanceFileMetadata` struct + JSON 序列化基础设施已就位，UI 编辑对话框不开发现阶段无价值。
- **Phase 7-7（全屏模式）** — 不实现。`resizable` toggle + OS 最大化窗口可替代，且无 kiosk mode 边界问题。

## 已修复缺陷（回归提醒）

各缺陷的完整修复记录与回归条件见 [`../issues/known-issues.md`](../issues/known-issues.md) §2：

- 启动早期首音音高异常（保留 `25ms` audio warmup）。
- MIDI 导入播放首音无声（playback-start pre-roll / arming）。
- 辅助窗口键盘焦点冲突（restoreKeyboardFocus guard）。
- Phase 6-2 播放速度控制（公式修正 + position 重校准 + atomic 化）。


## 验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

涉及环境或路径问题时执行：

```bash
./scripts/dev.sh self-check
```

## 相关文档

- 项目路线图：[`roadmap.md`](roadmap.md)
- 架构概览：[`../reference/architecture.md`](../reference/architecture.md)
- Phase 5 完成记录：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)
- Phase 6-7 完成记录：[`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)

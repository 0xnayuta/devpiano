# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 6 进行中**

**Phase 5 已完成**（5.1-5.8 + 人工回归均通过）。`MainComponent.cpp` 从 1587 行降至 606 行，远低于 1200 行目标。详细完成记录见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

**Phase 6-1 已完成**：演奏文件 Save/Open 核心功能。
  - `PerformanceFile.h/.cpp` JSON 序列化/反序列化。
  - ControlsPanel Save/Open 按钮（布局待美化）。
  - `RecordingSessionController` 接入 Save/Open 流程。
  - 详细说明见 [`roadmap.md`](roadmap.md) Phase 6 章节。

**Phase 6-2 下一阶段**：播放速度控制（0.5x ~ 2.0x）。

---

## 已修复缺陷（保留回归项）

**启动 / 音频重建早期首音音高异常**：已修复并通过人工验证；保留 `25ms` audio warmup。详细记录见 [`../testing/known-issues.md`](../testing/known-issues.md) §2。

**MIDI 导入播放首音无声**：已通过 playback-start pre-roll / arming 修复并完成人工回归；后续触及 MIDI import / playback 启动链路时执行 Phase 4 §11.1 回归。详细记录见 [`../testing/known-issues.md`](../testing/known-issues.md) §8。

---

**搁置说明**：外部 MIDI 硬件依赖验证和 VST3 插件离线渲染（Phase 3-5）因当前条件暂无法测试，状态已记录至 [`../testing/known-issues.md`](../testing/known-issues.md) §1 和 §3。

---

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
- 架构概览：[`../architecture/overview.md`](../architecture/overview.md)
- Phase 5 完成记录：[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)

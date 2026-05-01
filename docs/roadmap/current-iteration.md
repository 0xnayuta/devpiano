# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃阶段：**Phase 5：架构收敛与 MainComponent 瘦身**

Phase 5.1-5.7 已完成（2026-05-01）：MainComponent 职责下沉，包括录制会话状态结构化、导出流程统一、布局 CRUD 收敛、设置窗口收敛、AppState 清理、ControlsPanel 按钮状态统一、MIDI 导入流程下沉。

当前 `MainComponent.cpp` 约 1587 行，目标降至 1200 行以下。

---

**搁置说明**：外部 MIDI 硬件依赖验证和 VST3 插件离线渲染（Phase 3-5）因当前条件暂无法测试，状态已记录至 `docs/testing/phase3-recording-playback.md`。

## Phase 5.8+ 待规划提取目标

- 设置窗口管理提取（约 100 行）
- 布局管理继续收敛（约 100 行）
- 插件操作提取（约 130 行）
- 录制/回放编排提取（约 100 行）
- 插件 editor window 生命周期提取（约 50 行）

## 完成标准

- [~] Phase 5.8+ 待规划：MainComponent.cpp 继续收敛，目标从约 1587 行降至 1200 行以下。

## 本轮计划验证命令

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

- 项目路线图：`docs/roadmap/roadmap.md`
- 架构概览：`docs/architecture/overview.md`
- Phase 5.1-5.7 完成记录：`docs/archive/phase5-architecture-convergence.md`

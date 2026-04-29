# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃迭代：**M6 录制 / 回放 / MIDI 导出 MVP（含 WAV 离线渲染 M6-6a/b/c/d）全部完成，下一目标待定**。

上一轮已完成：MainComponent 插件流程初步收敛、键盘映射默认布局回归、布局 Preset 核心能力、录制 / 停止 / 回放 / MIDI 导出 / WAV 离线渲染（M6-6a/b/c/d）全部收尾，E.1–E.9 全部通过。

当前项目已经从"主链路打通 / MVP 恢复"进入下一阶段。

---

**搁置说明**：M6 录制/回放稳定化（见本轮 Section 1 最后 1 项边界检查）和外部 MIDI 硬件依赖验证（见 Section 2）因当前无真实外部 MIDI 设备，暂无法测试。已将状态记录至 `docs/testing/recording-playback.md`（包 C、7.1、已知限制）。这两个方向暂从本迭代移除，恢复时间取决于硬件条件。

## 本轮目标

本轮已完成 M6 录制 / 回放 / MIDI 导出 MVP（含 WAV 离线渲染 M6-6a/b/c/d），E.1–E.9 全部通过。

剩余方向：

- Section 4：插件扫描产品化增强排期（不阻塞当前迭代）。
- Section 5：MainComponent 录制 / 回放 / 导出状态流收敛（小步进行，不大规模重写）。

已搁置（待硬件条件恢复）：

- M6 录制/回放稳定化最后 1 项边界检查。
- 外部 MIDI 硬件依赖验证（已记录至 `docs/testing/recording-playback.md`）。

## 本轮优先任务

### 3. WAV 离线渲染 MVP 设计

对应后续方向：[`roadmap.md`](roadmap.md) 的 M6-6。

对应文档：

- [`../features/recording-playback.md`](../features/recording-playback.md)
- [`../testing/acceptance.md`](../testing/acceptance.md)

**状态**：M6-6a/b/c/d 全部完成，E.1–E.9 全部通过。VST3 插件离线渲染（M6-6e）后置。

已完成范围：

- [x] 定义 `RecordingTake -> offline render loop -> WAV file` 的最小链路。
- [x] 第一切片支持 fallback synth 离线渲染（M6-6b）。
- [x] UI 接入 Export WAV 按钮 + FileChooser + 选项收集（M6-6c）。
- [x] 专项测试包 E（9 项）全部通过（M6-6d）。

后置（M6-6e）：

- [~] 第二切片：当前已加载 VST3 插件的离线渲染（暂不阻塞 MVP）。

明确不做：

- [x] 不做 MP4 / 视频导出。
- [x] 不做复杂编辑、钢琴卷帘、多轨工程、tempo map。
- [x] 不复刻旧 FreePiano `song.*` / `export.*` 的平台相关结构。

完成标准：

- [x] 形成可执行的 M6-6 实现切片。
- [x] 验收文档中 WAV 导出已标记为 `[x]` 完成。

### 4. 插件扫描产品化增强排期

对应文档：

- [`../features/plugin-hosting.md`](../features/plugin-hosting.md)
- [`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)
- [`roadmap.md`](roadmap.md) 的 M2 / M3 后续增强。

本轮不优先实现大改，只做排期和小步准备：

- [ ] 记录扫描失败文件，而不只是失败数量。
- [ ] 明确多目录扫描的输入格式、UI 表达和持久化方式。
- [ ] 评估 `KnownPluginList` / 扫描结果持久化，降低启动恢复对同步扫描的依赖。
- [ ] 补充空状态、失败状态和恢复失败提示的 UI 需求。

完成标准：

- 插件扫描增强进入 backlog，且不阻塞当前迭代目标。

### 5. MainComponent 职责继续收敛

对应代码：

- `source/MainComponent.*`
- `source/Plugin/PluginFlowSupport.*`
- 后续可能新增 `source/Recording/*Flow*` 或等价轻量 helper。

重点：

- [ ] 优先抽离录制 / 回放 / 导出状态流，而不是大规模重写 `MainComponent`。
- [ ] 保留 `MainComponent` 对 UI 组件拥有权、窗口生命周期和顶层装配职责。
- [x] 新增 WAV 导出时不得把完整离线渲染流程直接堆入 `MainComponent`。（已按 M6-6c 执行）

完成标准：

- 新增流程有清晰边界。
- 现有键盘演奏、插件加载、录制 / 回放 / MIDI / WAV 导出行为不回退。

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

## 后续候选文档任务

以下不是本轮代码实现的优先项，仅在需要时补充：

- 继续提炼 ADR：
  - JUCE 音频后端决策。
  - VST3-first 插件宿主决策。
  - WAV 离线渲染边界决策。
- 按需补充轻量文档入口：
  - [`../testing/known-issues.md`](../testing/known-issues.md)：已知问题与待验证风险。
  - [`../development/troubleshooting.md`](../development/troubleshooting.md)：开发环境与构建问题排查。
- 后续如架构文档继续增长，再拆分音频链路、插件宿主、录制 / 导出、状态模型等专题。

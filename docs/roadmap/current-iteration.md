# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。  
> 读者：当前开发者、协作者、代码审查者。  
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前状态

当前活跃迭代：**M6 录制 / 回放稳定化、外部 MIDI 验证补齐、WAV 离线渲染准备**。

上一轮已完成：MainComponent 插件流程初步收敛、键盘映射默认布局回归、布局 Preset 核心能力、录制 / 回放 / MIDI 导出 MVP 和对应阶段文档收尾。

当前项目已经从“主链路打通 / MVP 恢复”进入“稳定化、硬件依赖回归和下一项高级功能设计”的阶段。

## 本轮目标

本轮不扩大功能面，优先把已恢复的 M6 主链路变得更稳，再启动 WAV 离线渲染的最小设计。

核心目标：

- 收紧 `RecordingEngine`、`AudioEngine` 与 `MainComponent` 之间的实时音频线程边界。
- 补齐外部 MIDI 相关验证，包括录制 / 回放和插件生命周期退出路径。
- 在不引入旧 `song.*` 结构、不做 MP4 / 多轨 / 复杂编辑的前提下，设计 WAV 离线渲染 MVP。
- 继续避免 `MainComponent` 膨胀；新增录制 / 导出状态优先通过小型 helper / coordinator、`AppState` / builder 或 UI 子组件边界表达。
- 同步维护路线图、功能说明、验收和专项测试文档，避免“已完成 / 待验证 / 后续增强”口径漂移。

## 本轮优先任务

### 1. M6 录制 / 回放稳定化

对应代码：

- `source/Recording/RecordingEngine.*`
- `source/Audio/AudioEngine.*`
- `source/MainComponent.*`
- `source/UI/ControlsPanel.*`

对应文档：

- [`../features/recording-playback.md`](../features/recording-playback.md)
- [`../testing/recording-playback.md`](../testing/recording-playback.md)

重点：

- [x] 复核 audio callback 中访问 `RecordingEngine` 的边界，避免文件 IO、UI 操作、阻塞等待或非受控分配进入实时路径。
- [x] 将回放结束通知收敛为轻量状态传递；避免 audio thread 直接触发复杂 UI / `std::function` 逻辑。
- [x] 复核采样率变化时的回放时间缩放与播放结束判断。
- [x] 更新 `RecordingEngine.h` 中关于 “Minimal skeleton / not thread-safe” 的注释，使其准确描述当前 MVP 状态和剩余约束。
- [ ] 对空 take、容量耗尽、note on/off 成对、Stop 清理悬挂音等边界做代码级检查或补充轻量测试入口。

完成标准：

- audio callback 与 UI / 文件 / 设置写入边界清晰。
- 回放结束、停止回放、采样率变化路径没有明显悬挂音或状态错乱风险。
- 文档中的线程安全约束与代码实现一致。
- WSL configure 和 Windows MSVC 验证构建通过。

### 2. 外部 MIDI 硬件依赖回归补齐

对应文档：

- [`../testing/recording-playback.md`](../testing/recording-playback.md)
- [`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)
- [`../features/plugin-hosting.md`](../features/plugin-hosting.md)

重点：

- [ ] 使用真实外部 MIDI 设备验证录制 / 停止 / 回放。
- [ ] 如暂时没有真实设备，先用虚拟 MIDI loopback 做替代验证，并在测试记录中明确标注。
- [ ] 补齐插件生命周期测试 `6.3`：外部 MIDI 打开状态下退出程序。
- [ ] 验证外部 MIDI + 已加载插件 + editor 打开时的 Play / Stop / 退出组合路径。

完成标准：

- 外部 MIDI 录制 / 回放结果记录到专项测试文档。
- 插件生命周期 `6.3` 不再是未验证项，或明确记录替代验证方式和剩余硬件限制。

### 3. WAV 离线渲染 MVP 设计

对应后续方向：[`roadmap.md`](roadmap.md) 的 M6-6。

对应文档：

- [`../features/recording-playback.md`](../features/recording-playback.md)
- [`../testing/acceptance.md`](../testing/acceptance.md)

本轮只做设计收口和最小切片拆分；实现可以在稳定化完成后开始。

建议范围：

- [ ] 定义 `RecordingTake -> offline render loop -> WAV file` 的最小链路。
- [ ] 第一切片优先支持 fallback synth 离线渲染。
- [ ] 第二切片再评估当前已加载 VST3 插件的离线渲染。
- [ ] 明确导出期间 UI、实时音频设备、插件 editor 和当前插件状态的边界。
- [ ] 明确失败策略：插件不支持离线渲染、目标路径无权限、空 take、用户取消保存。

明确不做：

- [ ] 不做 MP4 / 视频导出。
- [ ] 不做复杂编辑、钢琴卷帘、多轨工程、tempo map。
- [ ] 不复刻旧 FreePiano `song.*` / `export.*` 的平台相关结构。

完成标准：

- 形成可执行的 M6-6 实现切片。
- 验收文档中 WAV 导出仍标记为未完成，但其后续路径和边界清晰。

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

- 插件扫描增强进入 backlog，且不阻塞 M6 稳定化与 WAV 设计。

### 5. MainComponent 职责继续收敛

对应代码：

- `source/MainComponent.*`
- `source/Plugin/PluginFlowSupport.*`
- 后续可能新增 `source/Recording/*Flow*` 或等价轻量 helper。

重点：

- [ ] 优先抽离录制 / 回放 / 导出状态流，而不是大规模重写 `MainComponent`。
- [ ] 保留 `MainComponent` 对 UI 组件拥有权、窗口生命周期和顶层装配职责。
- [ ] 新增 WAV 导出时不得把完整离线渲染流程直接堆入 `MainComponent`。

完成标准：

- 新增流程有清晰边界。
- 现有键盘演奏、插件加载、录制 / 回放 / MIDI 导出行为不回退。

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

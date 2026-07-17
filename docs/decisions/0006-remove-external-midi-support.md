# ADR 0006: 移除外部 MIDI 设备支持，聚焦电脑键盘演奏场景

## 状态

已采用。

## 背景

项目初始目标是"将旧版 Windows FreePiano 重构为基于 JUCE 的现代 C++ 音频应用"。旧 FreePiano 支持外部 MIDI 硬件输入（如电子钢琴键盘、MIDI 控制器）作为核心输入路径之一。

当前代码中包含完整的 `MidiRouter` 类（`source/Midi/MidiRouter.h/.cpp`），负责枚举、打开 Juce `MidiInput` 设备、接收消息、通过 `MidiChannelMapper` 做通道变换、推入音频线程。此外，`RecordingEventSource` 枚举包含 `externalMidi` 值，`PerformanceFile` 序列化包含 `sourceExternalMidi` 常量。

该项目实际拥有者并未拥有任何外部 MIDI 设备（电子钢琴键盘、MIDI 控制器等），使用场景为"使用电脑键盘演奏钢琴，支持 MIDI 文件读取、导入、导出，以及音频导出"。外部 MIDI 设备相关代码在无硬件的机器上编译后永不执行（`MidiRouter::openAllInputs()` 打开 0 个设备），属于永久死代码。

## 决策

移除外部 MIDI 设备相关功能代码，不再支持外部 MIDI 硬件输入路径。

具体范围：

1. **删除 `MidiRouter` 类** — 整个外部 MIDI 设备枚举、打开、接收消息的基础设施。
2. **简化 `MainComponent`** — 移除 `MidiRouter` 成员变量、`setupMidi()` 中的 router 配置、`shutdown()` 中的 router 清理、`externalMidiMessageCount` / `lastExternalMidiMessage` 计数器。
3. **简化 `AppState` / `AppStateBuilder`** — 移除 `InputState::openMidiInputCount` 字段和 `MidiRouter` 参数。
4. **简化 `HeaderPanel` 状态显示** — 移除或简化为仅显示键盘输入活动。
5. **清理 `RecordingEventSource` 枚举** — 移除 `externalMidi` 值；对应的 `PerformanceFile` 序列化分支同步清理。
6. **删除无调用方法** — `RecordingEngine::recordEventAtCurrentPosition()` 无任何调用者，一并删除。

`MidiChannelMapper` 共享基础设施（应用于电脑键盘、鼠标点击、外部 MIDI 三路输入）**不删除**，因为它仍然是电脑键盘输入和鼠标点击的核心路由组件。

## 影响

### 正面影响

- 删除约 150 行永久死代码（MidiRouter 92 行 + 集成 ~60 行），降低维护负担。
- 消除 HeaderPanel 上始终显示 `"MIDI Inputs: 0"` 的无意义信息。
- 简化 `MainComponent::setupMidi()`（减少约 25 行），使电脑键盘输入的主路径更加清晰。
- 简化 `AppState` / `InputState` 模型，移除永远不会被使用的字段。
- 使项目定位更加明确：电脑键盘钢琴工具，非通用 MIDI 工作站。

### 代价

- 如果将来需要外部 MIDI 硬件支持，需要重新实现或回退代码。但 `MidiRouter` 的接口（`juce::MidiInputCallback`）是标准的 JUCE API，重新实现成本与删除前一样低。
- 与旧 FreePiano 的功能边界产生偏离。旧 FreePiano 支持外部 MIDI 输入，新项目明确不支持。

### 风险评估

- 向后兼容：追踪确认没有任何代码路径使用 `RecordingEventSource::externalMidi` 写入 `.devpiano` 文件。删除该枚举值不影响任何现有文件。
- 无测试覆盖风险：`MidiRouter` 无任何测试引用，`externalMidi` 枚举值无任何测试覆盖。
- 误伤风险：`MidiChannelMapper` 是共享基础设施，参与电脑键盘和鼠标点击路径，已排除在清理范围外。

## 对项目长期目标与定位的影响

### 项目定位的明确化

原始目标"将旧版 FreePiano 重构为现代 C++ 应用"隐含了"完整替代旧 FreePiano 功能"的预期。移除外 MIDI 支持标志着从**功能替代**转向**场景聚焦**：

| 维度 | 原隐含定位 | 现在定位 |
|---|---|---|
| 硬件依赖 | 支持外部 MIDI 键盘/控制器 | 不依赖任何外部硬件 |
| 目标用户 | 原 FreePiano 用户（含硬件用户） | 电脑键盘演奏用户 |
| 功能边界 | 通用 MIDI 输入工具 | 纯软件键盘钢琴 + MIDI 文件处理 |
| 维护负担 | 需维护多条输入路径 | 单一输入路径（电脑键盘） |

### 对阶段路线图的影响

- `roadmap.md` 中"长期观察项"和"主要风险"中的"外部 MIDI 硬件依赖验证"可以删除。
- 无需再为"外部 MIDI 硬件条件恢复"保留任何计划或占位。
- 团队注意力完全集中在电脑键盘输入、MIDI 文件处理、音频导出等核心路径上。

### 是否应写入 ADR

该决策符合 ADR 记录标准：

1. **已确定的决策** — 不是计划，是已采纳的清理方向。
2. **影响项目范围** — 改变了功能边界和项目定位。
3. **有不可逆性** — 代码删除后需重构才能恢复。
4. **需要记录原因** — 对其他贡献者和未来回顾有价值。

因此应该写入 ADR，本文件即为此记录。

## 清理执行计划

具体执行步骤、文件清单、验证标准见 [`../roadmap/cleanup-external-midi-scope.md`](../roadmap/cleanup-external-midi-scope.md)。

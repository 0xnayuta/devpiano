# ADR-012: 头文件 IWYU（Include What You Use）与细粒度包含纪律

## 状态

**已接受**（2026-08-23 实施）

## 背景

在 `devpiano` 早期重构阶段，为了开发便利，许多业务头文件直接包含了全局生成的巨型聚合头文件 `#include <JuceHeader.h>`。这种做法带来了严重的工程隐患与编译雪崩效应：

1. **级联头文件污染**：任意一个头文件（如 `Core/KeyMapTypes.h` 或 `Audio/AudioEngine.h`）只要包含了 `<JuceHeader.h>`，就会将 JUCE 全量模块（`juce_audio_processors`, `juce_dsp`, `juce_gui_extra` 等）数十万行的庞大 AST 强行展开到所有下游包含者中；
2. **增量编译雪崩**：修改底层头文件中的微小字段或类型，会迫使上层几十个编译单元重新展开并解析数十万行代码，导致单文件改动的增量编译退化为全量编译；
3. **架构边界侵蚀**：核心领域模型（如 `MidiTypes`, `ChannelMatrix`）无意识地与庞大的 GUI 和宿主模块强耦合，破坏了纯领域层的独立性。

## 决策

为了确保代码架构的长治久安，建立以下 **IWYU（Include What You Use）细粒度包含纪律**：

### 1. 业务头文件严格禁止包含 `<JuceHeader.h>`
- 所有 `source/` 目录下的 `.h` 头文件**禁止出现 `#include <JuceHeader.h>`**；
- 头文件若需使用 JUCE 类型，**必须按需引入具体的细粒度模块头文件**：
  - 核心基础与字符串：`<juce_core/juce_core.h>`
  - MIDI 与基础音频：`<juce_audio_basics/juce_audio_basics.h>`
  - 音频设备管理：`<juce_audio_devices/juce_audio_devices.h>`
  - 插件宿主与处理器：`<juce_audio_processors/juce_audio_processors.h>`
  - GUI 基础组件：`<juce_gui_basics/juce_gui_basics.h>`
  - 数据结构与 ValueTree：`<juce_data_structures/juce_data_structures.h>`
  - 事件与定时器：`<juce_events/juce_events.h>`
  - 图形与颜色：`<juce_graphics/juce_graphics.h>`

### 2. 优先采用前向声明（Forward Declarations）
- 对于仅在头文件中以指针或引用形式传递的类型（如 `class AudioEngine;`, `class PluginHost;`, `struct SettingsModel;`），优先使用前向声明，避免在头文件中包含对方的完整定义；
- 具体实现依赖移入对应的 `.cpp` 源码文件中。

### 3. `<JuceHeader.h>` 的使用边界
- `<JuceHeader.h>` **仅允许在业务实现源文件（`.cpp`）中使用**（或直接在 `.cpp` 中按需引入具体模块）；
- 严格杜绝在公共头文件或导出接口中再泄漏 `<JuceHeader.h>`。

### 4. 纯数据模型与领域层零无关模块耦合
- 纯数据结构（如 `ChannelMatrix`, `MidiTypes`）优先基于 C++ 标准库类型构建，消除对复杂 GUI/Audio 宿主子系统的偶然依赖。

## 影响与收益

- **极速增量编译**：修改单一头文件时，下游各源文件仅重新解析其实际依赖的细粒度头文件，增量构建响应速度压制在秒级以内；
- **清晰的架构依赖图谱**：各模块的物理依赖关系一目了然，消除了由于单一大头文件掩盖的循环依赖和隐式耦合；
- **全项目 55 个文件规范化**：已全量完成 `source/` 下所有头文件向细粒度包含的迁移，并保持单元测试 100% 通过。

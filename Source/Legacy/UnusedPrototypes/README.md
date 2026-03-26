# Unused Prototypes

该目录存放当前 **不参与主构建** 的旧版/过渡版原型代码，用于迁移参考与历史保留。

当前已移入的文件：

- `AudioEngine.h/.cpp`
- `MidiRouter.h/.cpp`
- `SongEngine.h/.cpp`

说明：

- 这些文件不是当前 JUCE 主实现的一部分
- 当前主实现请使用：
  - `Source/Audio/AudioEngine.*`
  - `Source/Midi/MidiRouter.*`
- `SongEngine` 目前仍是未接线的早期雏形，仅保留作后续录制/回放设计参考
- 如需重新启用其中任何原型，应先评估是否与当前架构冲突，再决定是迁移、重写还是删除

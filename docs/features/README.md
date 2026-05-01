# Features

本目录用于放置面向功能行为的说明文档。

当前文档：

- [`M4-keyboard-mapping.md`](M4-keyboard-mapping.md)：电脑键盘到 MIDI note 的映射能力、当前边界和后续方向。
- [`M3-plugin-hosting.md`](M3-plugin-hosting.md)：VST3 插件扫描、加载、处理、editor 和生命周期相关功能行为。
- [`M6-recording-playback.md`](M6-recording-playback.md)：录制 / 回放 / MIDI 导出的第一版设计与当前 MVP 行为说明。
- [`M7-layout-presets.md`](M7-layout-presets.md)：布局 preset 当前行为、文件格式、导入/保存/重命名/删除与恢复说明。
- [`M8-midi-file-and-freepiano-gap.md`](M8-midi-file-and-freepiano-gap.md)：MIDI 文件导入、回放兼容性、剩余边界与 FreePiano 差距。

相关入口：

- 当前状态与计划：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)
- 键盘映射测试：[`../testing/keyboard-mapping.md`](../testing/keyboard-mapping.md)
- 布局 preset 测试：[`../testing/layout-presets.md`](../testing/layout-presets.md)
- 录制 / 回放 / MIDI 导出测试：[`../testing/recording-playback.md`](../testing/recording-playback.md)
- MIDI 文件导入测试：[`../testing/midi-file-import.md`](../testing/midi-file-import.md)
- 插件宿主生命周期测试：[`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)
- 旧功能迁移边界：[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)

后续可按真实实现进展逐步补充：

- [`runtime-language-switching.md`](runtime-language-switching.md)

未明确实现的功能不应在本目录中写成已完成能力。

# Features

本目录用于放置面向功能行为的说明文档。

当前文档：

- [`keyboard-mapping.md`](keyboard-mapping.md)：电脑键盘到 MIDI note 的映射能力、当前边界和后续方向。
- [`plugin-hosting.md`](plugin-hosting.md)：VST3 插件扫描、加载、处理、editor 和生命周期相关功能行为。
- [`recording-playback.md`](recording-playback.md)：录制 / 回放 / MIDI 导出的第一版设计与当前 MVP 行为说明。
- [`layout-presets.md`](layout-presets.md)：布局 preset 当前行为、文件格式、导入/保存/重命名/删除与恢复说明。

相关入口：

- 当前状态与计划：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)
- 键盘映射测试：[`../testing/keyboard-mapping.md`](../testing/keyboard-mapping.md)
- 布局 preset 测试：[`../testing/layout-presets.md`](../testing/layout-presets.md)
- 录制 / 回放 / MIDI 导出测试：[`../testing/recording-playback.md`](../testing/recording-playback.md)
- 插件宿主生命周期测试：[`../testing/plugin-host-lifecycle.md`](../testing/plugin-host-lifecycle.md)
- 旧功能迁移边界：[`../architecture/legacy-migration.md`](../architecture/legacy-migration.md)

后续可按真实实现进展逐步补充：

- [`runtime-language-switching.md`](runtime-language-switching.md)

未明确实现的功能不应在本目录中写成已完成能力。

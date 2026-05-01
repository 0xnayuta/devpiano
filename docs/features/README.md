# Features

本目录用于放置面向功能行为的说明文档。

## Phase 2：插件系统与键盘映射

- [`phase2-plugin-hosting.md`](phase2-plugin-hosting.md)：VST3 插件扫描、加载、处理、editor 和生命周期相关功能行为。
- [`phase2-keyboard-mapping.md`](phase2-keyboard-mapping.md)：电脑键盘到 MIDI note 的映射能力、当前边界和后续方向。

## Phase 3：UI 与高级功能

- [`phase3-recording-playback.md`](phase3-recording-playback.md)：录制 / 回放 / MIDI 导出的第一版设计与当前 MVP 行为说明。
- [`phase3-layout-presets.md`](phase3-layout-presets.md)：布局 preset 当前行为、文件格式、导入/保存/重命名/删除与恢复说明。
- [`phase3-plugin-offline-rendering.md`](phase3-plugin-offline-rendering.md)：VST3 插件离线渲染评估与设计（Phase 3-2，后置）。

## Phase 4：MIDI 文件导入

- [`phase4-midi-file-import.md`](phase4-midi-file-import.md)：MIDI 文件导入、回放兼容性、剩余边界与 FreePiano 差距。

## 其他

- [`runtime-language-switching.md`](runtime-language-switching.md)：运行时语言切换（占位文档，功能细节后续补充）。

## 相关入口

- 当前状态与计划：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)
- 键盘映射测试：[`../testing/phase2-keyboard-mapping.md`](../testing/phase2-keyboard-mapping.md)
- 插件宿主生命周期测试：[`../testing/phase2-plugin-host-lifecycle.md`](../testing/phase2-plugin-host-lifecycle.md)
- 布局 preset 测试：[`../testing/phase3-layout-presets.md`](../testing/phase3-layout-presets.md)
- 录制 / 回放 / MIDI 导出测试：[`../testing/phase3-recording-playback.md`](../testing/phase3-recording-playback.md)
- MIDI 文件导入测试：[`../testing/phase4-midi-file-import.md`](../testing/phase4-midi-file-import.md)

未明确实现的功能不应在本目录中写成已完成能力。

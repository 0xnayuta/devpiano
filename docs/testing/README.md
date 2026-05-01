# Testing

本目录放置阶段验收、专项手工测试和回归测试清单。

当前文档：

- [`acceptance.md`](acceptance.md)：阶段验收标准。
- [`keyboard-mapping.md`](keyboard-mapping.md)：键盘映射专项手工测试。
- [`layout-presets.md`](layout-presets.md)：布局 preset 的保存、导入、重命名、删除与启动恢复手工测试。
- [`recording-playback.md`](recording-playback.md)：录制、回放与 MIDI 导出专项手工测试。
- [`midi-file-import.md`](midi-file-import.md)：M8 MIDI 文件导入、回放、自动选轨与后续增强验收测试。
- [`plugin-host-lifecycle.md`](plugin-host-lifecycle.md)：插件扫描、加载、卸载、editor 与退出稳定性专项测试。

使用建议：

- 修改键盘输入、布局或焦点逻辑后，优先执行 [`keyboard-mapping.md`](keyboard-mapping.md)。
- 修改布局 preset 文件格式、发现机制、Save / Import / Rename / Delete / Startup Restore 逻辑后，优先执行 [`layout-presets.md`](layout-presets.md)。
- 修改录制、回放、MIDI 导出或 `AudioEngine` 录制边界后，优先执行 [`recording-playback.md`](recording-playback.md)。
- 修改 MIDI 文件导入、导入后回放、自动选轨或 playback 切换逻辑后，优先执行 [`midi-file-import.md`](midi-file-import.md)。
- 修改插件宿主、插件 editor、音频设备重建或退出流程后，优先执行 [`plugin-host-lifecycle.md`](plugin-host-lifecycle.md)。
- 关键阶段验收以 [`acceptance.md`](acceptance.md) 为准。

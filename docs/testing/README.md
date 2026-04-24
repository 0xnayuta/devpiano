# Testing

本目录放置阶段验收、专项手工测试和回归测试清单。

当前文档：

- [`acceptance.md`](acceptance.md)：M0-M6 阶段验收标准。
- [`keyboard-mapping.md`](keyboard-mapping.md)：键盘映射专项手工测试。
- [`plugin-host-lifecycle.md`](plugin-host-lifecycle.md)：插件扫描、加载、卸载、editor 与退出稳定性专项测试。

使用建议：

- 修改键盘输入、布局或焦点逻辑后，优先执行 [`keyboard-mapping.md`](keyboard-mapping.md)。
- 修改插件宿主、插件 editor、音频设备重建或退出流程后，优先执行 [`plugin-host-lifecycle.md`](plugin-host-lifecycle.md)。
- 关键阶段验收以 [`acceptance.md`](acceptance.md) 为准。

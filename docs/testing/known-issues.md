# 已知问题与待验证风险

> 状态：占位文档，后续按需从测试记录和路线图中提炼。

当前项目状态与风险以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准；阶段验收见 [`acceptance.md`](acceptance.md)。

## 待补充

- 键盘映射待验证风险
- 插件宿主生命周期待验证风险
- WSL / Windows 镜像构建环境问题

## 当前已知问题 / 待确认风险

- 音频设置窗口中的 `buffer size` 选项在当前 Windows 侧手工回归里只能选择 `10 ms`。
  - 现象来源：[`testing/plugin-host-lifecycle.md`](plugin-host-lifecycle.md) 的 5.1 手工测试。
  - 当前判断：从项目实现与 JUCE `AudioDeviceSelectorComponent` 的行为看，更像是当前音频后端 / 设备能力限制，或设置恢复后的 fallback 行为，而不是已确认的项目业务逻辑缺陷。
  - 后续动作：在记录当前音频后端 / 设备类型的前提下继续复现，区分是 Windows / JUCE 后端限制，还是项目保存 / 恢复路径导致的体验问题。

- 插件生命周期测试 6.3（外部 MIDI 打开状态下退出程序）因当前缺少外部 MIDI 设备，仍未完成手工验证。

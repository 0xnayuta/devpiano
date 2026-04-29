# 已知问题与待验证风险

> 状态：轻量风险清单；详细项目状态以路线图和当前迭代文档为准。

当前项目状态与风险以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准；阶段验收见 [`acceptance.md`](acceptance.md)。

## 待补充

- WSL / Windows 镜像构建环境问题：见 [`../development/troubleshooting.md`](../development/troubleshooting.md)。
- 后续如风险条目增多，再按键盘映射、插件宿主、录制 / 导出、构建环境拆分。

## 当前已知问题 / 待确认风险

- 插件生命周期测试 6.3（外部 MIDI 打开状态下退出程序）因当前缺少外部 MIDI 设备，仍未完成手工验证。
- 录制 / 回放 MVP 已恢复，但实时音频线程边界、回放结束通知、采样率变化和 Stop 清理悬挂音路径仍是下一阶段稳定化重点。
- 外部 MIDI 录制 / 回放仍待真实硬件或虚拟 MIDI loopback 验证。
- WAV 离线渲染尚未实现；下一阶段先做 MVP 设计和 fallback synth 第一切片，MP4 / 多轨 / tempo map 不在近期范围。

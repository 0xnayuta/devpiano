# 已知问题与待验证风险

> 状态：轻量风险清单；详细项目状态以路线图和当前迭代文档为准。
> 更新时机：发现新问题、完成验证、搁置项恢复时。

当前项目状态与风险以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准；阶段验收见 [`acceptance.md`](acceptance.md)。

---

## 1. 外部 MIDI 硬件依赖（搁置）

> 触发条件：当前无真实外部 MIDI 设备，部分验证无法执行。
> 恢复时机：硬件条件恢复后优先补齐。

- **外部 MIDI 录制**：外部 MIDI 设备输入到同一 take 的通路代码已接入，但录制效果待真实设备验证。
- **外部 MIDI 回放**：回放事件重新进入插件路径已实现，外部 MIDI 设备驱动插件发声待验证。
- **插件生命周期测试 6.3**（外部 MIDI 打开状态下退出程序）：因缺少外部 MIDI 设备，尚未完成手工验证。
- **替代方案**：可先配置虚拟 MIDI loopback 软件（如 loopMIDI）做基础通路验证，不依赖真实硬件。

详见：

- [`recording-playback.md`](phase3-recording-playback.md) §7（外部 MIDI 输入）
- [`plugin-host-lifecycle.md`](phase2-plugin-host-lifecycle.md) §6.3
- [`keyboard-mapping.md`](phase2-keyboard-mapping.md) §9.2（混合输入）

---

## 2. VST3 插件离线渲染（Phase 3-5，后置）

> 触发条件：用户期望用已加载的 VST3 插件音色导出 WAV。
> 影响范围：当前 WAV 导出仅支持 fallback synth。

- **现状**：WAV 离线渲染 MVP（Phase 3-4）已完成，fallback synth 导出和 UI 接入均已验证通过。
- **限制**：WAV 导出不支持已加载 VST3 插件音色；VST3 插件离线渲染（Phase 3-5）暂不阻塞 MVP，已排入后续 backlog。
- **影响**：需要插件音色的用户暂时无法使用 WAV 导出功能。

详见：

- [`../features/phase3-recording-playback.md`](../features/phase3-recording-playback.md)（Phase 3-5 设计备注）
- [`../roadmap/roadmap.md`](../roadmap/roadmap.md)（Phase 3 当前状态）

---

## 3. 录制 / 回放下一阶段重点

> 触发条件：Phase 3 MVP 主链路已接入，但实时音频线程边界、回放结束通知、采样率缩放和 Stop 清理悬挂音路径仍是下一阶段稳定化重点。

- **回放结束通知**：回放结束后 UI 状态转换和焦点恢复路径待进一步验证。
- **采样率缩放**：不同采样率下录制事件的回放时间对齐待验证。
- **Stop 清理悬挂音**：Stop 时悬挂音（sustained notes）的清理路径已实现但需更多边界条件验证。
- **离线渲染采样率**：当前 WAV 导出固定 44100Hz，后续可考虑提供选项。

详见：

- [`recording-playback.md`](phase3-recording-playback.md)（包 A–E 测试结果）
- [`../roadmap/roadmap.md`](../roadmap/roadmap.md)（Phase 3 当前状态与近期重点）

---

## 4. 插件生命周期退出告警（低优先级持续观察）

> 触发条件：特定插件或 Debug 注入环境下退出阶段可能出现 JUCE / VST3 调试告警。

- **现状**：scan / load / unload / editor / 重扫 / 直接退出等主要生命周期组合路径已完成一轮人工回归，未发现功能性问题。
- **观察项**：特定插件 / Debug 注入环境下退出阶段可能仍有 JUCE / VST3 调试告警；低优先级持续观察，暂不安排专项修复。
- **缓解**：主要路径已稳定；异常场景更多与插件自身实现质量相关，而非宿主代码问题。

详见：

- [`plugin-host-lifecycle.md`](phase2-plugin-host-lifecycle.md)（专项生命周期测试）
- [`../roadmap/roadmap.md`](../roadmap/roadmap.md)（Phase 2 当前状态）

---

## 5. 布局 Preset ID 冲突（低优先级）

> 触发条件：用户导入同名 preset 文件时。

- **现状**：用户 preset id 由文件名派生；导入同名文件时覆盖行为已实现，暂无非覆盖时的冲突提醒 UI。
- **影响**：正常使用场景不受影响；重命名等功能正常。
- **后续**：可按需补充冲突提示 UI，低优先级。

详见：

- [`../features/phase3-layout-presets.md`](../features/phase3-layout-presets.md)
- [`../testing/phase3-layout-presets.md`](../testing/phase3-layout-presets.md)

---

## 6. 构建与环境

> WSL / Windows 镜像构建环境问题见 [`../development/troubleshooting.md`](../development/troubleshooting.md)。

---

## 7. 已完成验证项（不作为风险）

以下条目已通过 2026-04-30 人工验证，无已知明显问题：

- **Phase 2-1**：扫描失败文件记录（`lastScanFailedFiles` + Logger 写入 + UI 摘要提示）。
- **Phase 2-2**：多目录扫描输入格式与持久化（`FileSearchPath` 分隔 + 过滤无效目录）。
- **Phase 2-3**：扫描结果持久化（`KnownPluginList` XML 缓存 + 启动优先恢复）。
- **Phase 2-4**：空 / 失败 / 恢复失败状态 UI（区分 6 种状态文本）。
- **Phase 5-1**：RecordingFlowSupport 录制 / 回放 UI 流程。
- **Phase 5-2**：ExportFlowSupport 导出选项与默认文件名。
- **Phase 5-3**：PluginFlowSupport scan / restore / cache 收敛。
- **Phase 5-4**：MainComponent 状态刷新边界命名清理。
- **Phase 5**：Phase 5-1..5-4 全部完成，人工回归通过。

详见：

- [`../roadmap/current-iteration.md`](../roadmap/current-iteration.md)（Phase 5-1..5-4 人工回归记录）
- [`phase2-plugin-host-lifecycle.md`](phase2-plugin-host-lifecycle.md)（Phase 2-1..2-4 测试结果）
# devpiano 项目定位与范围

> 用途：定义 devpiano 的项目身份、核心能力、明确非目标和长期演进方向。
> 更新时机：项目定位变化、核心能力增减、非目标调整时。
> 关联文档：[`architecture.md`](architecture.md)（架构描述）、[`../roadmap/roadmap.md`](../roadmap/roadmap.md)（路线图与阶段状态）

---

## 1. 一句话定位

**devpiano** 是一款个人主导、持续演进的电脑键盘钢琴应用——以 JUCE 为框架，VST3 插件为核心音源，聚焦软件键盘演奏与 MIDI 文件处理。

## 2. 项目起源

devpiano 源于对旧版 Windows FreePiano 的现代化重构尝试。旧 FreePiano 源码（`freepiano-src/`）在本仓库中保留为迁移参考资料，不参与当前构建。

**devpiano 不是 FreePiano 的超集或替代品**，而是一个独立项目——聚焦电脑键盘演奏场景，有自己的设计方向和判断标准。当 FreePiano 中有价值、有意义的功能全部在新项目中实现后，FreePiano 的参考使命即告完成。

## 3. 核心能力

### 演奏与输入

- 电脑键盘触发 MIDI note on/off，支持可配置键位映射
- 虚拟钢琴键盘可视化（classic / channel / velocity 着色模式，Do Re Mi / 固定 Do / 音符名称显示模式）
- 16 通道 MIDI 矩阵路由（`ChannelMatrix`），默认透传，完全向后兼容
- ADSR 包络控制

### 插件宿主

- VST3 插件扫描、加载、卸载、editor 窗口（核心功能）
- 插件扫描 UX 增强（分片进度、失败列表可发现性、扫描中状态区分）

### 演奏录制与文件

- 演奏录制、回放、播放速度精确控制（Slider + atomic 线程安全）
- `.devpiano` 演奏文件格式（JSON 序列化，含 events / sampleRate / 元数据）
- MIDI 文件导入（含 CC / pitch bend / program change 事件）
- MIDI 文件导出
- WAV 导出（含 VST3 离线渲染 + 进度对话框）

### UI 与体验

- 布局 Preset 系统（JSON 格式、自动发现、导入/保存/重命名/删除）
- 拖放文件支持（`.devpiano` / `.mid` / `.freepiano.layout` / `.vst3`）
- 运行时中英文语言切换（JUCE `Translation` 机制）
- 设置持久化（音频设备状态、性能参数、输入映射、插件恢复信息）
- 最近文件列表（`juce::RecentlyOpenedFilesList`）

## 4. 明确非目标

以下能力明确不属于 devpiano 的规划范围：

| 非目标 | 理由 |
|---|---|
| 外部 MIDI 输入路径 | 已移除（聚焦电脑键盘演奏场景，详见 ADR 0006） |
| 多轨 / 完整 DAW 功能 | 超出项目范围 |
| MIDI 编辑（delete notes / piano roll） | 重录成本低，编辑 UI 价值不足 |
| 旧 FreePiano `.fpm` 格式兼容 | 平台耦合、格式复杂 |
| 非 VST3 插件格式（VST2 / AU / AAX） | 聚焦 VST3 单一格式 |
| 导出到 MIDI 以外的外部硬件输出 | 超出项目范围 |

## 5. 设计原则

- **电脑键盘优先**：所有输入路径以电脑键盘为核心场景设计，其他路径（鼠标点击、文件导入）为辅助。
- **JUCE 标准化**：优先使用 JUCE 标准 API 和抽象，减少自定义实现。
- **持续演进**：项目不功能冻结，新功能以"对电脑键盘演奏场景有价值"为判断标准。
- **个人主导**：功能优先级由项目主导者根据实际使用需求决定，不受外部用户需求驱动。

## 6. 长期演进方向

- 架构优化（减少自定义实现、对齐 JUCE 标准 API、清晰化生命周期边界）作为持续主题。
- 新功能按"场景价值"评估，而非"与 FreePiano 的差距"。
- 在 FreePiano 参考使命完成后，放开对旧项目功能的追溯约束，完全以 devpiano 自身方向演进。

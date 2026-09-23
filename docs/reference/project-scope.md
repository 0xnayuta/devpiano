# devpiano 项目定位与范围

> 用途：定义 devpiano 的项目身份、核心能力、明确非目标和长期演进方向。
> 更新时机：项目定位变化、核心能力增减、非目标调整时。
> 关联文档：[`architecture.md`](architecture.md)（架构描述）、[`../roadmap/roadmap.md`](../roadmap/roadmap.md)（路线图与阶段状态）

---

## 1. 一句话定位

**devpiano** 是一款个人主导、持续演进的电脑键盘钢琴应用——以 JUCE 为框架，VST3 插件为核心音源，内置自主研发物理建模钢琴，聚焦软件键盘演奏与 MIDI 文件处理。

## 2. 项目起源

devpiano 源于对旧版 Windows FreePiano 的现代化重构。所有有价值的功能已通过各阶段在 JUCE 架构上完整重建，旧参考源码（`freepiano-src/`）已完全移除。参考使命已完成，devpiano 完全以自身方向独立演进。

## 3. 核心能力

### 演奏与输入

- 电脑键盘触发 MIDI note on/off，支持基于稳定 key code 的键位映射
- 5 行 ANSI 物理键盘映射看板（`QwertyComponent`），自适应网格排版、50fps 荧光余晖动画与 12-TET 和声色彩投影
- 88 键拟真虚拟钢琴键盘（classic / channel / velocity / harmony 4 种着色模式，Do Re Mi / 固定 Do / 音符名称 3 种标注模式）
- 轻量键位分组（`KeyGroup`，支持 4 组）即时切组与发音身份快照（Note-off Identity Preservation，彻底封死悬挂音）
- 采样精确的切分延音踏板调度（`SustainPolicy` / `SyncPedalProcessor`，消除连奏断音空洞）
- 瞬态演奏修饰键变换管道（`PerformanceModifierState`：Shift 力度拉满 / Alt 高八度平移，纯事件变换零配置污染）
- 逐键个性化标签（`customKeyLabels`）与逐键颜色（`customKeyColours`）定制
- 16 通道 MIDI 矩阵路由（`ChannelMatrix`），支持每通道移调、力度、音色、延音与按键跟随
- 全局调号控制（Key Signature，-7..+7 半音）与 MIDI 移调开关
- ADSR 包络控制与主增益平滑调节

### 发声引擎

- **内置物理建模钢琴**：自主拥有、纯 C++ 算法、零外部采样依赖的 7 大声学系统全物理建模钢琴合成器（`PianoSynthVoice`，涵盖击弦、弦体、共鸣、空气、琴盖、微观机械物理拟真、古典微调律制、双视角空间声学与房间混响）
- **内置正弦波合成器**：基准正弦合成（`SineSynthVoice`），支持平滑切换
- **统一乐器端点与 VST3 宿主**：统一 `InstrumentEndpoint` 领域抽象；VST3 插件扫描、异步分片进度、增量崩溃安全持久化（Crash-safe State Persistence）、XML 缓存恢复、加载、卸载与独立 Editor 窗口托管

### 演奏录制与文件

- 演奏录制、回放、播放速度精确控制（0.50x–2.00x，线程安全原子变速）
- `.devpiano` 原生演奏文件格式（v2 JSON 序列化，含 Base64 编码、events、采样率与元数据）
- 标准 MIDI 文件导入：Type 0/1 全轨并轨，解析 CC64 延音 / pitch bend / program change 事件
- 标准 MIDI 文件导出（Type 1，960 PPQ）
- WAV 音频离线导出（共享 `RenderPipeline` 管线与 `InstrumentEndpoint` 统一路由，异步非阻塞 `WavExportTask`，支持 VST3 独立离线实例与物理建模钢琴离线渲染，带 JIVE 声明式进度浮层）

### UI 与体验

- **JIVE 声明式 UI 架构**：`juce::ValueTree` 驱动整棵界面树，FlexBox / CSS Grid 自适应布局
- **通用声明式模态弹窗（`JiveModalDialog`）**：预设管理、元数据编辑与导出进度统一暗黑主题浮层
- **Performance Preset 预设系统**：`.devpiano.preset` JSON 格式、自动发现、CRUD 操作、F1-F12 快捷键与录制时自动切换预设
- **静态资产构建期内嵌**：设计 Token、样式表与中文语言包由 CMake `BinaryData` 编译期静态打包，单文件免安装绿色运行
- **运行时国际化**：中英文双语即时切换（JUCE `Translation` 机制）
- **文件拖放支持**：支持拖放 `.devpiano`、`.mid`、`.devpiano.preset` 与 `.vst3` 文件即时加载
- **设置持久化与最近文件**：音频设备状态、性能参数、输入映射、插件恢复信息与最近 10 个打开文件列表

---

## 4. 明确非目标

以下能力明确不属于 devpiano 的规划范围：

| 非目标 | 理由 |
|---|---|
| 外部 MIDI 硬件输入 | 已移除（聚焦电脑键盘演奏场景，详见 ADR 006） |
| 多轨音序器 / 完整 DAW 工作站功能（非多轨并轨导入） | 超出键盘钢琴定位，保持专用钢琴演奏宿主（Dedicated Piano Performance Host）轻量纯粹；支持标准多轨 MIDI 文件的智能并轨导入与统一演奏回放，但不做多轨音频轨/MIDI 轨编辑、多轨混音工作站与通用 Patchbay 连线图 |
| 复杂 MIDI 编辑（卷帘窗 / 量化 / 剪辑） | 电脑键盘演奏重录成本极低，图形化编辑投入产出比不足 |
| 旧 FreePiano `.fpm` / `.lyt` 格式兼容 | 旧格式平台耦合严重、格式陈旧 |
| 非 VST3 格式插件（VST2 / AU / AAX） | 聚焦 VST3 单一现代标准 |
| 导出到外部 MIDI 硬件设备 | 超出项目范围 |

---

## 5. 设计原则

- **电脑键盘优先**：所有输入路径与交互设计以电脑键盘为第一优先级，其他路径（鼠标点击、文件导入）为辅助。
- **纯粹与自包含**：核心能力力求自主拥有，不引入臃肿的外部采样库或庞杂依赖，保持极速冷启动与高便携性。
- **JUCE / 现代 C++ 标准化**：优先使用 JUCE 标准抽象与 C++20/23 语言特性，避免与特定平台强耦合。
- **声明式与低耦合**：UI 采用声明式 ValueTree 排版，业务逻辑下沉至独立 Controller，音频线程与 UI 线程严格隔离。
- **持续演进**：功能演进以"对电脑键盘演奏与练琴场景有实质价值"为衡量标准。

---

## 6. 长期演进方向

- **持续性能优化**：针对极端密集多复音 MIDI 播放场景，进一步推进 UI 局部脏矩形渲染与模态分音衰减剪枝。
- **架构一致性**：保持主装配层极简，维持高覆盖率的确定性单元测试与严格的静态检查门禁。

# VST3 插件离线渲染与 WAV 音频导出功能说明

> 用途：说明 devpiano 的非实时音频离线渲染管线（`RenderPipeline`）、独立离线 VST3 插件实例管理（`PluginOfflineRenderer`）、后台多线程导出任务（`WavExportTask`）与 JIVE 声明式进度条交互。
> 当前状态：已全量实现并稳定服务于 WAV 导出（Phase 7 & Phase 15 成果）。
> 更新时机：离线渲染管线、插件状态快照、导出进度交互或音频格式参数发生变化时。

---

## 1. 概述与设计定位

当用户录制了一段演奏或导入了 MIDI 文件后，需要将演奏内容导出为高质量的 `.wav` 音频文件分享或存档。
为了实现高保真、高稳定性与非阻塞的导出体验，devpiano 建立了专用的**非实时离线渲染子系统**：

1. **独立离线 VST3 实例**：离线渲染在独立的非实时插件实例中执行，与当前前台实时发声链路（`AudioDeviceManager`）及 Editor 窗口完全解耦，导出期间用户依然可以实时试听；
2. **公共离线渲染管线（`RenderPipeline`）**：集中处理事件时间戳缩放、排序、音频块切分与尾部 panic note-off 注入，为插件渲染与内置物理建模合成器消除重复代码；
3. **后台多线程与 JIVE 进度交互**：`WavExportTask` 在后台工作线程执行密集音频渲染，消息线程以 30 fps 平滑刷新基于 `JiveModalDialog` 驱动的暗黑主题进度条；
4. **安全取消与异常残留清理**：用户中途取消或发生 IO 异常时，自动停止后台线程并删除未写完的残留临时文件；
5. **优雅降级（Graceful Degradation）**：若插件不支持离线渲染或实例创建失败，系统自动安全降级至内置物理建模钢琴（`PianoSynthVoice`），保证导出永远可用。

---

## 2. 核心架构与主运行流程

```text
[用户点击 Export WAV] ──► ExportFlowSupport::buildWavExportOptions()
    │
    ▼
RecordingSessionController::exportTakeAsWav() ──► 弹出文件保存对话框 FileChooser
    │
    ▼
WavExportTask::startExport() (后台独立工作线程启动)
    │
    ├── JiveModalDialog::makeProgressLayout() (弹出现代化 JIVE 进度浮层)
    ├── 准备渲染引擎:
    │    ├── [已加载 VST3 插件] ──► PluginOfflineRenderer::renderTakeWithOfflinePlugin()
    │    │                         ├── 独立创建 AudioPluginInstance
    │    │                         ├── prepareToPlay(sampleRate, blockSize=512)
    │    │                         └── 渲染完成后 releaseResources() 并安全析构
    │    │
    │    └── [未加载插件 / 降级] ──► WavFileExporter (离线构建 PianoSynthVoice / Sine 渲染)
    │
    ├── RenderPipeline (统一调度事件时间戳缩放、按 samplePosition 排序并分块送入 processBlock)
    ├── juce::WavAudioFormat 写入目标文件 (16-bit / 24-bit / 32-bit float, 双声道 Stereo)
    └── 导出完成: 自动关闭进度弹窗 / 取消时自动清理残留文件
```

---

## 3. 核心机制与关键技术细节

### 3.1 独立离线插件实例生命周期（`PluginOfflineRenderer`）
- **独立性**：基于当前已加载插件的 `PluginDescription`，通过 `formatManager.createPluginInstance()` 创建一个全新的离线实例；
- **无 Editor 开销**：离线实例不创建任何 UI 窗口，避免跨线程 GUI 句柄死锁；
- **状态快照**：若实时插件支持状态保存，通过 `instance->getStateInformation()` 抓取当前音色参数并注入离线实例；
- **受控生命周期**：严格遵循 `create` → `prepareToPlay` → 逐 block `processBlock` → `releaseResources` → `delete` 的确定性生命周期。

### 3.2 共享渲染管线（`RenderPipeline`）
`source/Recording/RenderPipeline.cpp` 提供了实时与离线一致的事件调度抽象：
- **采样率自适应换算**：录制时的采样率（如 48 kHz）与导出目标采样率（如 44.1 kHz）不一致时，精确按比例换算每个事件的 `timestampSamples`；
- **事件绝对排序**：确保同一个 audio block 内的事件严格按 `samplePosition` 升序排列；
- **尾部防挂音注入**：在渲染结尾自动注入全通道 `allNotesOff` 与 `sustainOff`，消除由于 MIDI 数据不完整可能导致的尾部悬挂音。

### 3.3 后台任务与 JIVE 进度反馈（`WavExportTask`）
在 Phase 15-D 中，`WavExportTask` 彻底移除了 JUCE 原生陈旧的 `AlertWindow`，全面采用 JIVE 声明式进度弹窗：
- **无锁进度传递**：后台线程通过 `std::atomic<float> currentProgress` 和 `std::atomic<bool> cancelRequested` 与主线程通信；
- **安全取消机制**：用户点击 [Cancel] 按钮或按 ESC 键时，`cancelRequested` 置位，后台线程在下一个 block 循环立即退出，并在 `finally` 块中调用 `destinationFile.deleteFile()` 删除半截文件。

---

## 4. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **WAV-001** | 内置物理建模钢琴离线导出 | 在未加载插件下录制演奏并导出 WAV，导出的音频具有真实的物理建模钢琴音色 | [x] 已通过 |
| **WAV-002** | VST3 插件音色离线导出 | 加载 VST3 插件后录制演奏并导出 WAV，导出的音频为该 VST3 插件的真实音色 | [x] 已通过 |
| **WAV-003** | 导出期间实时弹奏解耦 | 导出长时间 WAV 期间，在前台按键盘弹奏，实时发声不受影响，导出音频中无键盘杂音 | [x] 已通过 |
| **WAV-004** | JIVE 进度条平滑刷新 | 导出过程中观察 JIVE 进度条从 0% 平滑推进至 100%，状态文本实时显示进度百分比 | [x] 已通过 |
| **WAV-005** | 中途取消与残留文件清理 | 导出推进到 50% 时点击 [Cancel]，导出立即中止，目标目录下无残缺 `.wav` 文件生成 | [x] 已通过 |
| **WAV-006** | 目标路径无权限容错 | 导出至只读目录或非法路径，弹窗提示错误，Logger 记录日志，程序不崩溃 | [x] 已通过 |
| **WAV-007** | 空 Take 导出拦截 | 在无录制且无导入状态下，Export WAV 按钮自动保持 Disabled | [x] 已通过 |

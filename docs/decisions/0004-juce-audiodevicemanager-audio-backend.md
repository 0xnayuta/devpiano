# ADR 0004: JUCE AudioDeviceManager 作为音频设备管理主路径

## 状态

已采用。

## 背景

旧版 FreePiano 使用 Windows 原生音频后端（WASAPI / ASIO / DirectSound）直接管理音频设备。这些实现与平台深度耦合，换到非 Windows 环境即不可用，且调试和维护成本高。

JUCE 提供了跨平台的 `AudioDeviceManager`，封装了各平台音频后端差异，支持 ASIO、WASAPI、Core Audio、ALSA、PulseAudio 等。重构项目使用 JUCE 框架，应优先使用 JUCE 原生抽象。

## 决策

`devpiano` 的音频设备管理统一使用 JUCE `AudioDeviceManager`，不直接引入 WASAPI / ASIO / DirectSound 依赖链。

具体约束：

- 音频设备初始化、切换、buffer size / sample rate 配置均通过 `AudioDeviceManager` 接口完成。
- 不在 source/ 下新增原生平台音频后端封装代码。
- 音频设备诊断信息通过 JUCE API 获取，不自行调用平台 SDK。

## 原因

- **跨平台**：同一代码可在 Windows/macOS/Linux 编译运行。
- **JUCE 一致性**：项目已使用 JUCE 框架，音频后端使用 JUCE 抽象保持技术路线统一。
- **维护成本**：平台 SDK 直接调用带来额外维护负担（设备枚举、格式转换、错误处理），JUCE 已封装这些路径。
- **旧代码参考原则**：旧 FreePiano 的 WASAPI / ASIO / DSound 后端仅作为行为参考（ADR-0002），不复制其实现。

## 影响

正面影响：

- 音频设备管理代码简洁，平台差异由 JUCE 处理。
- 未来扩展到 macOS/Linux 成本低。
- `AudioDeviceManager` 支持的设备类型（WASAPI/ASIO/CoreAudio）已在 JUCE 层统一。

代价：

- 高级 ASIO 功能（如 custom sample rate、device-specific latency）受限于 JUCE 抽象层级。
- 当前专注 Windows 平台，macOS/Linux 支持为远期目标。

## 当前实践

`source/Audio/AudioEngine.*` 通过 `juce::AudioDeviceManager` 管理音频设备：

- `AudioEngine::initialiseAudio()` 创建并配置 `AudioDeviceManager`。
- 音频设备设置（buffer size、sample rate）通过 `AudioDeviceManager::setAudioDeviceSetup()` 调整。
- 设备诊断信息通过 `AudioDeviceManager::getDeviceDescriptions()` 等 JUCE API 获取，写入 Logger。

## 非目标

- 不复刻旧 FreePiano 的 WASAPI / ASIO / DSound 实现。
- 不直接调用 Windows Core Audio API 或 ASIO SDK，均通过 JUCE 抽象层间接使用。
- 不在 source/ 下新增平台相关的音频设备封装。
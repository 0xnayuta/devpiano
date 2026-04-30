# ADR 0005: VST3-first 插件宿主

## 状态

已采用。

## 背景

旧版 FreePiano 使用旧式 VST SDK 风格接口（`AEffect*`、`dispatch()`、`process()` 函数指针）管理插件生命周期。旧 VST SDK 接口与现代 JUCE `AudioPluginInstance` 接口不兼容，直接复制会带来兼容性问题和技术债务。

JUCE 提供了跨平台的 `AudioPluginFormatManager` / `AudioPluginInstance` 抽象，支持 VST3 和 VST2（需要 Steinberg SDK），并封装了格式特定的细节。重构项目以 JUCE 为框架，应优先使用 JUCE 原生插件宿主能力。

## 决策

`devpiano` 的插件宿主以 VST3 为主路径，使用 JUCE `AudioPluginFormatManager` / `AudioPluginInstance` 作为插件加载和管理抽象。

具体约束：

- 插件扫描、实例化、prepare/release、process、editor 管理均通过 JUCE 插件宿主接口。
- 当前优先支持 VST3，不强制引入 VST2（VST2 需要单独配置 Steinberg SDK）。
- 不在 source/ 下直接调用旧 VST SDK 函数或复制 VST SDK 类型。

## 原因

- **技术一致性**：项目使用 JUCE 框架，插件宿主使用 JUCE 抽象保持技术路线统一。
- **跨平台**：VST3 是跨平台格式，JUCE 封装了 Windows/macOS/Linux 的 VST3 加载差异。
- **维护成本**：旧 VST SDK 接口（`AEffect*`/dispatch）是 C 风格接口，与 C++ RAII 原则不兼容，直接复制会导致生命周期管理问题。
- **旧代码参考原则**：旧 FreePiano 的 `synthesizer_vst.*` / `vst/*` 仅作为行为参考（ADR-0002），不复制其 VST SDK 风格实现。

## 影响

正面影响：

- 插件宿主代码简洁，格式差异由 JUCE 处理。
- 支持的插件格式由 JUCE `AudioPluginFormatManager` 统一管理，未来扩展成本低。
- JUCE 的 `AudioPluginInstance` 接口与现代 C++ 风格一致，生命周期清晰。

代价：

- 只能加载 JUCE 支持的插件格式（当前 VST3 可用，VST2 需要额外 SDK 配置）。
- 某些特定插件的旧 VST2 实现可能不兼容（这类插件在现代 DAW 中同样存在问题）。

## 当前实践

`source/Plugin/PluginHost.*` 使用 JUCE 插件宿主能力：

- `PluginHost::PluginHost()` 注册 `VST3PluginFormat`（`formatManager.addFormat(new VST3PluginFormat())`）。
- 扫描：`PluginDirectoryScanner` + `KnownPluginList`。
- 加载：`formatManager.createPluginInstance()` → `AudioPluginInstance`。
- prepare/release：`instance->prepareToPlay()` / `instance->releaseResources()`。
- editor：`instance->hasEditor()` → `createEditor()` / `deleteEditor()`。

## 非目标

- 不复刻旧 FreePiano 的 `synthesizer_vst.*` / `vst/*` 中的 VST SDK 风格接口。
- 不直接调用 VST SDK 函数（`AEffect*`、`dispatch()`、`process()` 等）。
- 不强制要求 VST2 支持（VST2 宿主需要 Steinberg SDK 和 CMake 配置，当前 `JUCE_PLUGINHOST_VST=1` 已留注释）。
# devpiano 旧代码迁移说明（Legacy Code Notes）

> 用途：说明 `freepiano-src/` 中旧模块在新 JUCE 架构中的定位、迁移边界与推荐替代方向。

## 构建基线（统一）
- 配置：`cmake --preset ninja-x64`
- Debug 构建：`cmake --build --preset ninja-debug`
- Release 构建：`cmake --build --preset ninja-release`

---

## 1. 文档目的

`freepiano-src/` 保存了旧版 FreePiano 的历史源码。它对于理解原始功能很有价值，但并不是当前主构建的一部分。

本文件用于明确：

- 哪些旧模块仍可作为行为参考
- 哪些旧模块不应直接复用
- 旧模块在新架构中的对应迁移方向

---

## 2. 使用边界

### 允许做的事情
- 阅读旧代码以理解历史行为
- 提取业务规则、消息语义、默认布局、功能边界
- 参考旧项目模块职责拆分方式
- 对照旧项目验证新实现行为是否一致

### 不建议做的事情
- 直接复制旧的 Windows 平台实现到新代码中
- 直接延续旧的宏式、全局函数式设计
- 重新引入以 `windows.h` 为中心的依赖链作为核心架构
- 将旧的音频后端、GUI、VST 实现原样照搬

### 核心原则

> 旧代码用于“提炼逻辑”，不是用于“直接复刻实现”。

---

## 3. 旧模块与新架构映射

## 3.1 音频输出后端

### 旧模块
- `freepiano-src/output_wasapi.*`
- `freepiano-src/output_dsound.*`
- `freepiano-src/output_asio.*`
- `freepiano-src/asio/*`

### 新架构替代方向
- JUCE `AudioDeviceManager`
- JUCE `AudioIODevice`
- JUCE `AudioAppComponent` / 音频回调链路

### 迁移建议
- 不再保留独立的 WASAPI / ASIO / DSound 代码路径
- 使用 JUCE 统一封装跨平台设备管理
- 如果需要设备配置 UI，优先使用 `AudioDeviceSelectorComponent`

---

## 3.2 VST / 乐器宿主

### 旧模块
- `freepiano-src/synthesizer_vst.*`
- `freepiano-src/vst/*`

### 新架构替代方向
- JUCE `AudioPluginFormatManager`
- JUCE `KnownPluginList`
- JUCE `PluginDirectoryScanner`
- JUCE `AudioPluginInstance`

### 迁移建议
- 不再沿用旧版 VST SDK 风格接口
- 使用 JUCE 完成扫描、实例化、prepare、processBlock、editor 管理
- 旧代码主要用于理解历史功能边界与插件交互预期

---

## 3.3 键盘输入与映射

### 旧模块
- `freepiano-src/keyboard.*`
- `freepiano-src/config.*`

### 新架构替代方向
- `Source/Input/KeyboardMidiMapper.*`
- 后续建议新增：`Source/Core/KeyMapTypes.h`
- JUCE `KeyPress` / `KeyListener` / `Component` 焦点机制

### 迁移建议
- 重点提取默认布局、映射规则、功能行为
- 不直接沿用旧扫描码处理与平台绑定输入逻辑
- 新实现应以可配置、可持久化、跨平台为优先目标

---

## 3.4 MIDI 输入输出

### 旧模块
- `freepiano-src/midi.*`

### 新架构替代方向
- `Source/Midi/MidiRouter.*`
- JUCE `MidiInput`
- JUCE `MidiMessageCollector`
- JUCE `MidiBuffer`

### 迁移建议
- 使用 JUCE MIDI API 统一管理设备枚举与消息输入
- 旧代码仅用于参考消息流向、功能需求与历史兼容行为

---

## 3.5 GUI 与显示层

### 旧模块
- `freepiano-src/gui.*`
- `freepiano-src/display.*`

### 新架构替代方向
- `Source/MainComponent.*`
- 后续建议拆分到 `Source/UI/*`
- JUCE `Component` 树
- JUCE `MidiKeyboardComponent`

### 迁移建议
- 不保留旧 Windows GUI / GDI 方案
- 使用 JUCE 组件化方式重建界面与交互
- 优先恢复功能，不优先复刻旧外观细节

---

## 3.6 配置与状态管理

### 旧模块
- `freepiano-src/config.*`
- `freepiano-src/json_loader.*`

### 新架构替代方向
- `Source/Settings/SettingsModel.h`
- `Source/Settings/SettingsStore.*`
- JUCE `ApplicationProperties`
- JUCE `ValueTree`

### 迁移建议
- 将旧配置项拆解为现代状态模型
- 避免延续旧式全局状态与宏配置方式
- 对键位映射、插件状态、音频设置采用明确的序列化结构

---

## 3.7 录制、回放与导出

### 旧模块
- `freepiano-src/song.*`
- `freepiano-src/export.*`
- `freepiano-src/export_wav.*`
- `freepiano-src/export_mp4.*`

### 新架构替代方向
- 当前仅有过渡雏形：`Source/Legacy/UnusedPrototypes/SongEngine.*`
- 后续建议使用：
  - 自定义现代事件模型，或
  - JUCE `MidiMessageSequence` / `MidiFile`

### 迁移建议
- 不直接延续旧项目的裸消息码事件系统作为唯一内部格式
- 先完成录制/回放的数据模型设计，再实现导出
- 优先实现 MIDI / WAV，MP4 后置

---

## 4. 当前新旧模块对应关系一览

| 旧模块 | 当前/目标新模块 | 说明 |
|---|---|---|
| `output_wasapi.* / output_asio.* / output_dsound.*` | `AudioDeviceManager` | 统一音频设备管理 |
| `synthesizer_vst.*` | `Source/Plugin/PluginHost.*` | 目前仅完成扫描，后续接入实例化 |
| `keyboard.*` | `Source/Input/KeyboardMidiMapper.*` | 当前仅最小映射，后续需重构 |
| `midi.*` | `Source/Midi/MidiRouter.*` | 已有基础输入路由 |
| `gui.* / display.*` | `Source/MainComponent.*` / 后续 `Source/UI/*` | UI 正在重建 |
| `config.*` | `Source/Settings/*` | 状态管理已开始，但未统一 |
| `song.*` | 后续重建 | 当前只有早期原型，不参与主构建 |

---

## 5. 当前仓库中的历史原型代码

除 `freepiano-src/` 外，当前仓库还保留了一组不参与主构建的过渡原型代码：

- `Source/Legacy/UnusedPrototypes/AudioEngine.*`
- `Source/Legacy/UnusedPrototypes/MidiRouter.*`
- `Source/Legacy/UnusedPrototypes/SongEngine.*`

说明：
- 这些代码已从 `Source/` 根目录移出，避免与当前主实现混淆
- 它们仅用于历史保留与迁移参考
- 若未来需要重新使用，应优先考虑“重建设计”，而不是直接恢复旧原型

---

## 6. 推荐迁移顺序

建议按以下顺序从旧代码中提炼逻辑：

1. `config.*` / `keyboard.*`
   - 提取默认键位布局、键位语义、状态定义
2. `synthesizer_vst.*`
   - 提取插件宿主功能边界与交互预期
3. `song.*`
   - 提取录制/回放的功能需求，而不是直接继承内部表示
4. `export_*.*`
   - 最后再看导出逻辑

---

## 7. 结论

`freepiano-src/` 是迁移参考库，不是当前实现库。

在后续开发中，应始终坚持以下原则：

- 优先迁移“行为”和“规则”
- 不直接迁移平台绑定实现
- 优先以 JUCE 原生能力重建核心模块
- 避免把旧项目结构性问题带入新代码

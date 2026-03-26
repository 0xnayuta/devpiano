# devpiano 项目现状评估与后续规划（执行版）

> 用途：本文件用于记录当前项目评估结果、总体规划，以及后续阶段推进状态。
>
> 状态标记说明：
> - [x] 已完成
> - [ ] 未开始 / 未完成
> - [~] 进行中 / 部分完成

---

## 1. 项目概况

`devpiano` 当前处于从旧版 Windows FreePiano 向基于 JUCE 的现代 C++ 音频应用迁移的中前期阶段。

### 当前主干基础能力状态

- [x] JUCE GUI 应用可正常启动
- [x] 音频设备可初始化
- [x] 电脑键盘可触发 MIDI note
- [x] 可打开外部 MIDI 输入
- [x] 可调整基础音量与 ADSR 参数
- [x] 可显示虚拟钢琴键盘
- [x] 可扫描 VST3 目录并列出插件名称
- [x] 基础设置可持久化
- [x] Debug 构建已验证通过

### 已验证构建命令

```bash
cmake --build Build/vs2026-x64 --config Debug
```

构建结果：成功生成 `Build/vs2026-x64/devpiano_artefacts/Debug/DevPiano.exe`

---

## 2. 当前代码结构分析

### 2.1 顶层目录职责

- `Source/`：当前 JUCE 主代码目录
- `freepiano-src/`：旧版 FreePiano 参考源码，不参与当前主构建
- `JUCE/`：JUCE 子模块，禁止修改
- `Doc/`：规划与说明文档
- 根目录：CMake、预设、仓库说明文件

### 2.2 Source 当前模块划分

#### 应用入口
- `Source/Main.cpp`
  - JUCEApplication 启动入口
  - 创建主窗口并承载 `MainComponent`

#### 主界面与应用装配
- `Source/MainComponent.h`
- `Source/MainComponent.cpp`
  - 负责 UI 布局
  - 初始化音频设备
  - 连接音频、MIDI、插件扫描、设置存储
  - 当前承担过多职责

#### 音频引擎
- `Source/Audio/AudioEngine.h`
- `Source/Audio/AudioEngine.cpp`
  - 当前基于 `juce::Synthesiser`
  - 内置 `SimpleSineVoice` 作为临时发声器
  - 通过 `MidiMessageCollector` 与 `MidiKeyboardState` 处理 MIDI
  - 目前尚未接入真正的 VST 乐器实例

#### 电脑键盘映射
- `Source/Input/KeyboardMidiMapper.h`
- `Source/Input/KeyboardMidiMapper.cpp`
  - 实现最小固定按键映射
  - 基于字符而非稳定物理键/扫描码
  - 仅覆盖基础白键布局

#### MIDI 输入路由
- `Source/Midi/MidiRouter.h`
- `Source/Midi/MidiRouter.cpp`
  - 打开全部可用 MIDI 输入
  - 将消息送入 `MidiMessageCollector`
  - 可附加消息回调

#### 插件宿主扫描层
- `Source/Plugin/PluginHost.h`
- `Source/Plugin/PluginHost.cpp`
  - 注册插件格式
  - 扫描 VST3
  - 维护 `KnownPluginList`
  - 目前仅是“扫描器”，不是完整插件宿主

#### 设置与持久化
- `Source/Settings/SettingsModel.h`
- `Source/Settings/SettingsStore.h`
- `Source/Settings/SettingsStore.cpp`
- `Source/Settings/SettingsComponent.h`
  - 保存音频设备 XML
  - 保存采样率、缓冲区大小、ADSR、主音量
  - 预留 `keyMap` 持久化能力
  - 设置页使用 JUCE 标准音频设备选择组件

### 2.3 旧版参考代码结构

`freepiano-src/` 中仍保留大量旧模块：

- `keyboard.*`：旧按键输入系统
- `midi.*`：旧 MIDI 输入输出
- `output_wasapi.* / output_dsound.* / output_asio.*`：旧音频后端
- `synthesizer_vst.*`：旧 VST 乐器逻辑
- `song.*`：旧录制/播放系统
- `config.*`：旧配置与键位定义
- `gui.*`：旧 Windows GUI
- `export_wav.* / export_mp4.*`：旧导出模块

这些文件应作为迁移参考，而非直接复用。

---

## 3. 当前功能模块完成度评估

### 3.1 已实现能力

#### 应用与设备
- [x] JUCE GUI 程序可启动
- [x] 音频设备初始化正常
- [x] MIDI 输入设备可枚举并打开

#### 演奏基础链路
- [x] 电脑键盘可触发 MIDI note on/off
- [x] 虚拟钢琴键盘可显示按键状态
- [x] 外部 MIDI 输入可进入音频引擎
- [x] 可通过滑块控制 ADSR 与主音量

#### 插件相关
- [x] 可枚举插件格式
- [x] 可扫描 VST3 搜索路径
- [x] 可显示扫描到的插件名称列表

#### 设置
- [x] 可保存基础音频设备状态
- [x] 可保存基础 UI 参数
- [x] 具备简易延迟保存机制

### 3.2 尚未完成的核心能力

#### 插件宿主主链路
- [x] 创建 `AudioPluginInstance` 的最小宿主接口已接入
- [x] 已支持按名称或描述加载指定 VST3 乐器的第一轮能力
- [x] `AudioEngine` 已具备将 MIDI 送入插件实例的主路径
- [x] `AudioEngine` 已具备通过插件 `processBlock` 输出音频的第一轮实现
- [x] 插件生命周期管理已完成第一轮接入
- [~] 主链路代码已接通并完成第一轮加固，仍待真实插件手工验证
- [ ] 插件 editor 嵌入显示

#### 键盘映射系统
- [ ] 基于稳定物理键的映射
- [ ] 用户自定义映射
- [ ] 布局切换
- [ ] 设置与映射逻辑打通

#### 状态模型统一
- [x] `SettingsModel.keyMap` 已完成第一轮驱动 `KeyboardMidiMapper`
- [ ] 插件路径、插件选择、扫描缓存的统一持久化

#### 录制与导出
- [ ] 演奏录制
- [ ] 回放
- [ ] MIDI/WAV 导出
- [ ] 布局 Preset 导入导出

---

## 4. 技术栈评估

### 4.1 当前已采用技术

- [x] C++20
- [x] JUCE
- [x] CMake 3.22+
- [x] Visual Studio 2026
- [x] `AudioDeviceManager`
- [x] `AudioPluginFormatManager`
- [x] `KnownPluginList`
- [x] `MidiMessageCollector`
- [x] `MidiKeyboardState`
- [x] `ApplicationProperties`
- [x] `ValueTree`

### 4.2 技术方向判断

当前技术路线与总体目标一致，主要替代关系如下：

- 旧音频后端（WASAPI/ASIO/DSound） -> JUCE `AudioDeviceManager`
- 旧 VST 宿主逻辑 -> JUCE `AudioPluginFormatManager` / `AudioPluginInstance`
- 旧 Windows 控件/GDI -> JUCE `Component`
- 旧配置序列化 -> JUCE `ApplicationProperties` + `ValueTree`

结论：
- [x] 技术方向总体正确
- [ ] 核心宿主链路已经完整落地

---

## 5. 风险评估

### 5.1 高风险项

#### 风险 A：新旧实现并存，容易造成结构混乱
当前 `Source/` 下仍存在未参与构建的旧/过渡文件：

- `Source/AudioEngine.*`
- `Source/MidiRouter.*`
- `Source/SongEngine.*`

风险：
- IDE 跳转和搜索混淆
- 容易误改错误文件
- 后续 include 与命名冲突概率升高

处理状态：
- [x] 已完成第一轮处理：未参与构建的 `Source/AudioEngine.*`、`Source/MidiRouter.*`、`Source/SongEngine.*` 已移入 `Source/Legacy/UnusedPrototypes/`，并补充目录说明文档

#### 风险 B：当前发声链路仍是临时实现
当前实际链路为：

`键盘 -> MIDI -> 内置正弦波 Synth -> 音频输出`

目标链路应为：

`键盘 -> MIDI -> VST 插件实例 -> 音频输出`

处理状态：
- [ ] 尚未切换到插件主链路

#### 风险 C：按键映射实现过于简化
当前映射基于字符输入：
- 受系统键盘布局影响
- 不利于恢复旧版复杂布局
- 与 `OverallGoal.md` 的重构目标不完全一致

处理状态：
- [ ] 尚未重构

#### 风险 D：插件宿主仅有扫描功能
当前已完成第一轮实例化与基础生命周期接入，但仍没有 processBlock 主链路、完整恢复机制、editor 管理。

处理状态：
- [~] 已完成 D-1、D-2，后续继续推进 D-3 / D-6

### 5.2 中高风险项

#### 风险 E：状态模型未统一
- `SettingsModel.keyMap` 尚未被业务层使用
- 插件状态未系统化建模
- UI 状态与引擎状态未来可能分裂

处理状态：
- [ ] 尚未统一

#### 风险 F：MainComponent 职责过重
当前 `MainComponent` 同时承担：
- UI 布局
- 音频设备管理
- 键盘输入处理
- 插件扫描触发
- 设置同步

处理状态：
- [ ] 尚未拆分

#### 风险 G：录制/导出路径缺少现代设计
旧项目有复杂历史实现，新项目若不先定义新数据模型，容易再次复制旧的宏式设计。

处理状态：
- [ ] 尚未启动设计

### 5.3 中风险项

#### 风险 H：缺少测试与验收基线
当前未见：
- 单元测试
- 键位映射测试
- 设置序列化测试
- 插件扫描/加载回归测试

处理状态：
- [ ] 尚未建立

#### 风险 I：README 信息不足
处理状态：
- [x] 已完成第一轮补充，并新增 `Doc/LegacyCodeNotes.md` 说明旧代码迁移边界

---

## 6. 对照 OverallGoal 的阶段性判断

### 第 1 步：重构核心数据结构
- [~] 已进入第一轮实现，尚未系统完成

说明：
- 已有 `SettingsModel`
- 已新增 `Source/Core/KeyMapTypes.h` 作为第一版核心键位模型
- 尚无完整统一的核心枚举、常量、事件模型层

### 第 2 步：重构按键映射逻辑
- [~] 已完成最小可运行版本、第一轮内部模型重构、基础设置接线与输入捕获收敛

说明：
- `KeyboardMidiMapper` 已接入 `Source/Core/KeyMapTypes.h`
- 已支持 `setLayout()` / `getLayout()` / `resetToDefaultLayout()`
- 已与 `SettingsModel.keyMap` 打通基础持久化闭环
- 当前已改为优先使用 `key.getKeyCode()`，不再依赖 `getTextCharacter()` 作为主路径
- 焦点切换与极端输入场景仍需手工测试验证

### 第 3 步：重构音频与 VST 宿主层
- [~] 已完成扫描、实例化、基础生命周期管理、音频接线与最小加载 UI

说明：
- `PluginHost` 可扫描 VST3
- 已支持 `loadPluginByName()` / `loadPluginByDescription()` / `unloadPlugin()`
- 已支持 `prepareToPlay()` / `releaseResources()` 与基础 bus 配置
- `AudioEngine` 已优先尝试走插件 `processBlock()`，无插件时 fallback 到内置 synth
- `MainComponent` 已提供最小插件选择/加载/卸载 UI
- 仍需通过真实插件加载与手工验证确认主链路稳定性

### 第 4 步：串联事件循环让程序发声
- [~] 主链路代码已基本接通，等待真实插件验证

说明：
- 程序当前一定可以通过内置正弦波发声
- 代码已优先尝试使用已加载 VST 插件发声
- 仍需通过真实插件加载与手工试弹确认最终效果

### 第 5 步：重构 UI
- [~] 已完成初步界面雏形，并补齐最小插件加载操作区

说明：
- 已有钢琴键盘、基础控制区域、插件扫描区
- 已增加插件选择 / Load / Unload 最小操作区
- 但整体仍偏调试面板

### 第 6 步：高阶功能恢复
- [ ] 仅有设置存储雏形

说明：
- 布局切换、录制、导出等尚未真正启动

---

## 7. 后续优化与改进规划

## 7.1 阶段 0：结构清障

### 阶段状态
- [x] 已完成（A-1、A-2 已完成）

### 目标
清理当前仓库中的重复、过渡和歧义结构，为后续重构降低风险。

### 建议动作
- [x] 明确标记或移走未参与构建的过渡文件
- [x] 保证 `Source/` 根目录中的主实现路径更清晰
- [x] 为 `freepiano-src/` 增加“只读参考”说明
- [x] 补充 `README.md` 基础说明

### 预期输出
- 更干净的源码树
- 更清晰的开发边界

---

## 7.2 阶段 1：建立核心模型层

### 阶段状态
- [~] 进行中（`Source/Core/` 与 `KeyMapTypes.h` 已创建）

### 目标
建立平台无关、现代化的核心类型层，替代旧项目中的宏与裸值。

### 建议新增目录
- [x] `Source/Core/`

### 建议新增文件方向
- [ ] `Source/Core/Constants.h`
- [ ] `Source/Core/MidiTypes.h`
- [x] `Source/Core/KeyMapTypes.h`
- [ ] `Source/Core/AppState.h`

### 计划内容
- [ ] 定义音符、通道、八度等强类型结构
- [x] 定义键位绑定数据结构
- [ ] 定义应用状态数据模型
- [ ] 明确序列化格式

### 验收标准
- [~] 已开始减少键位领域对裸 `int`/简单 map 的直接依赖，后续仍需扩展到更多核心领域

---

## 7.3 阶段 2：重构键盘映射系统

### 阶段状态
- [~] 进行中（已完成模型切换、基础设置接线与输入捕获第一轮收敛）

### 目标
将当前硬编码字符映射升级为可配置、可持久化、可切换布局的键盘映射系统。

### 计划内容
- [x] 将 `KeyboardMidiMapper` 改造为基于 `KeyboardLayout` 的状态化模块
- [~] 从稳定 key code/物理键角度建模
- [x] 将 `SettingsModel.keyMap` 接入业务
- [~] 支持默认布局、保存布局、加载布局

### 验收标准
- [~] 主路径已不再依赖字符文本，后续仍需手工验证输入法/焦点场景
- [x] 当前映射已具备基础保存并恢复能力
- [ ] 键盘与虚拟钢琴联动稳定

---

## 7.4 阶段 3：升级插件宿主

### 阶段状态
- [ ] 未开始

### 目标
把当前 `PluginHost` 从“扫描器”升级为真正的插件宿主。

### 建议拆分
- [ ] `PluginScanner`：负责扫描与插件列表
- [ ] `PluginInstanceHost`：负责实例创建、prepare、processBlock、销毁

### 计划内容
- [ ] 允许用户从扫描列表中选择插件
- [ ] 创建 `AudioPluginInstance`
- [ ] 处理插件生命周期
- [ ] 将 MIDI 送入插件
- [ ] 将插件输出接入音频回调
- [ ] 视条件支持插件 editor

### 验收标准
- [ ] 至少能够加载一个 VST3 乐器并正常发声

---

## 7.5 阶段 4：重构主音频链路

### 阶段状态
- [ ] 未开始

### 目标
将主发声链路改为插件驱动，而不是内置正弦波。

### 计划内容
- [ ] 保留 `AudioEngine` 作为总协调层
- [ ] 将插件实例音频处理接入 `getNextAudioBlock`
- [ ] 将 `SimpleSineVoice` 下沉为 fallback/debug 功能

### 验收标准
- [ ] `A/S/D/F` 等按键可驱动选中的 VST3 乐器发声
- [ ] 外部 MIDI 输入同样可驱动插件发声

---

## 7.6 阶段 5：UI 重构与组件拆分

### 阶段状态
- [ ] 未开始

### 目标
从调试面板演进为结构清晰的正式界面。

### 建议拆分方向
- [ ] `StatusBar`
- [ ] `PluginPanel`
- [ ] `KeyboardPanel`
- [ ] `PerformancePanel`
- [ ] `SettingsPanel`

### 计划内容
- [ ] 插件扫描、选择、加载、editor 控制独立成区
- [ ] 演奏参数与虚拟键盘独立成区
- [ ] 状态显示更明确
- [ ] 减轻 `MainComponent` 负担

### 验收标准
- [ ] `MainComponent` 主要负责装配，不再承载所有细节逻辑

---

## 7.7 阶段 6：恢复高级功能

### 阶段状态
- [ ] 未开始

### 目标
逐步恢复旧版的实用功能，但基于现代 JUCE 架构重建。

### 计划内容

#### 布局切换与 Preset
- [ ] 保存/加载键盘布局
- [ ] 恢复默认布局
- [ ] 增加布局文件格式

#### 录制与回放
- [ ] 定义现代录制事件模型
- [ ] 接入 MIDI 录制
- [ ] 支持回放

#### 导出
- [ ] 优先实现 MIDI/WAV
- [ ] MP4 后置，不作为近期主目标

### 验收标准
- [ ] 具备最基本的录制、回放、导出闭环

---

## 8. 推荐优先级排序

### 第一优先级
- [x] 结构清障
- [ ] 重构键盘映射系统
- [ ] 升级插件宿主到可实例化
- [ ] 完成插件主发声链路

### 第二优先级
- [ ] 统一状态模型
- [ ] 拆分 `MainComponent`
- [ ] UI 分区重构

### 第三优先级
- [ ] 布局 Preset
- [ ] 录制/回放
- [ ] 导出功能
- [ ] README 与测试完善

---

## 9. 结论与近期突破口

当前 `devpiano` 已经不是空项目，而是一个能够运行、能够发声、能够扫描插件的 JUCE 重构骨架。

但从总体目标来看，项目仍处于“骨架已搭好、核心宿主链路尚未完成”的阶段。当前最关键的突破口是：

- [ ] 用更稳定的方式重做键盘映射系统
- [ ] 把插件扫描升级为真正的插件实例宿主
- [ ] 让主音频链路从“内置正弦波”切换到“VST 乐器驱动”

只要这三点落地，项目就会跨过最关键的里程碑，进入可替代旧 FreePiano 核心体验的阶段。

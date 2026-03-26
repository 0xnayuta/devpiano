# devpiano 下一阶段任务清单（按文件级别拆解，执行版）

> 用途：本文件用于作为下一阶段开发执行清单，支持直接勾选任务状态。
>
> 状态标记说明：
> - [x] 已完成
> - [ ] 未开始 / 未完成
> - [~] 进行中 / 部分完成

---

## 1. 目标说明

本清单用于把当前“分析与规划”落到可执行的下一阶段开发任务上。

下一阶段的主目标聚焦于四件事：

- [ ] 清理当前结构中的重复与歧义
- [ ] 建立统一核心模型层
- [ ] 将键盘映射升级为可配置系统
- [ ] 将插件扫描升级为真正可发声的插件宿主主链路

---

## 2. 阶段划分总览

### 阶段 A：结构清障
- [ ] 清理重复文件与过渡实现
- [ ] 降低后续重构风险

### 阶段 B：核心模型层建设
- [ ] 建立强类型、跨平台的数据结构

### 阶段 C：键盘映射系统升级
- [ ] 从硬编码映射升级为可配置映射

### 阶段 D：插件宿主主链路落地
- [ ] 从“仅扫描”升级到“可实例化并发声”

### 阶段 E：UI 与状态接线
- [ ] 将上面能力接入当前界面与持久化系统

### 阶段 F：文档与验收基线补充
- [ ] 补全文档与阶段验收标准

---

# 3. 文件级任务清单

## 阶段 A：结构清障

### A-1. 审核并处理未参与当前构建的遗留文件

#### 状态
- [ ] 未开始

#### 文件
- `Source/AudioEngine.h`
- `Source/AudioEngine.cpp`
- `Source/MidiRouter.h`
- `Source/MidiRouter.cpp`
- `Source/SongEngine.h`
- `Source/SongEngine.cpp`

#### 任务
- [ ] 确认这些文件是否仍需保留
- [ ] 若仅为历史过渡实现，移入 `Legacy/` / `Scratch/` 或明确标记为未启用
- [ ] 避免与当前主实现重名并存

#### 输出结果
- [ ] `Source/` 下只保留当前有效主实现，或至少通过命名/目录明确区分

#### 优先级
高

---

### A-2. 为旧版参考源码补充使用边界说明

#### 状态
- [ ] 未开始

#### 文件
- `README.md`
- 可选新增：`Doc/LegacyCodeNotes.md`

#### 任务
- [ ] 在 README 中说明 `freepiano-src/` 仅供迁移参考
- [ ] 在 README 中说明 `freepiano-src/` 不参与当前主构建
- [ ] 在 README 中说明不应直接复制平台相关旧实现
- [ ] 如有必要，新增 `Doc/LegacyCodeNotes.md` 记录旧模块与新模块映射关系

#### 输出结果
- [ ] 降低误读旧代码的风险

#### 优先级
中高

---

## 阶段 B：核心模型层建设

### B-1. 新建核心类型目录

#### 状态
- [ ] 未开始

#### 新增目录
- `Source/Core/`

#### 任务
- [ ] 在 CMake 中纳入新目录下的头文件/源文件
- [ ] 将核心数据类型集中到 `Source/Core/`

#### 输出结果
- [ ] 项目形成清晰的 Core / Input / Audio / Plugin / Settings 分层

#### 优先级
高

---

### B-2. 建立键位映射核心类型

#### 状态
- [ ] 未开始

#### 建议新增文件
- `Source/Core/KeyMapTypes.h`

#### 任务
- [ ] 定义 `KeyBinding`
- [ ] 定义 `KeyboardLayout`
- [ ] 定义 `MappedAction` / `KeyAction`
- [ ] 定义默认布局生成函数
- [ ] 设计物理键标识或稳定 key code 表示方式
- [ ] 纳入 MIDI note / channel / velocity / 触发方式等字段

#### 输出结果
- [ ] 键位映射不再只是 `unordered_map<int, int>`

#### 优先级
最高

---

### B-3. 建立 MIDI 相关基础类型

#### 状态
- [ ] 未开始

#### 建议新增文件
- `Source/Core/MidiTypes.h`

#### 任务
- [ ] 定义 `MidiNoteNumber`
- [ ] 定义 `MidiChannel`
- [ ] 定义 `Velocity`
- [ ] 定义 `NoteRange`
- [ ] 让类型保持轻量，不做过度设计

#### 输出结果
- [ ] 减少代码中散落的裸 `int` / `float`

#### 优先级
中高

---

### B-4. 建立应用状态聚合模型

#### 状态
- [ ] 未开始

#### 建议新增文件
- `Source/Core/AppState.h`

#### 任务
- [ ] 设计音频设置聚合结构
- [ ] 设计插件选择状态结构
- [ ] 设计键盘布局状态结构
- [ ] 设计演奏参数状态结构
- [ ] 明确后续与 `SettingsModel` 的迁移关系

#### 输出结果
- [ ] 为后续 UI / 引擎 / 设置三方同步做准备

#### 优先级
中

---

## 阶段 C：键盘映射系统升级

### C-1. 改造 KeyboardMidiMapper 的内部模型

#### 状态
- [ ] 未开始

#### 文件
- `Source/Input/KeyboardMidiMapper.h`
- `Source/Input/KeyboardMidiMapper.cpp`

#### 任务
- [ ] 去掉对固定字符映射的强依赖
- [ ] 支持可注入映射表
- [ ] 接入 `Source/Core/KeyMapTypes.h`
- [ ] 增加 `setLayout(...)`
- [ ] 增加 `getLayout()`
- [ ] 增加 `resetToDefaultLayout()`

#### 输出结果
- [ ] `KeyboardMidiMapper` 从硬编码工具类演进为真正的状态化模块

#### 优先级
最高

---

### C-2. 将 SettingsModel.keyMap 与 KeyboardMidiMapper 接线

#### 状态
- [ ] 未开始

#### 文件
- `Source/Settings/SettingsModel.h`
- `Source/Settings/SettingsStore.cpp`
- `Source/Input/KeyboardMidiMapper.h`
- `Source/Input/KeyboardMidiMapper.cpp`
- `Source/MainComponent.cpp`

#### 任务
- [ ] 定义 `SettingsModel.keyMap` 与新布局模型之间的转换关系
- [ ] 程序启动时从设置恢复布局并注入 `KeyboardMidiMapper`
- [ ] 程序退出或保存时将当前布局写回设置
- [ ] 验证默认布局与保存布局之间的覆盖顺序

#### 输出结果
- [ ] 当前键位映射可真正持久化

#### 优先级
最高

---

### C-3. 修正输入捕获方式，减少字符布局依赖

#### 状态
- [ ] 未开始

#### 文件
- `Source/Input/KeyboardMidiMapper.cpp`
- `Source/MainComponent.cpp`

#### 任务
- [ ] 评估当前 `key.getTextCharacter()` 的局限
- [ ] 优先改用更稳定的 key code 方案
- [ ] 保留必要的规范化逻辑
- [ ] 验证按键释放检测逻辑是否稳定
- [ ] 验证焦点切换后 held key 状态是否残留

#### 输出结果
- [ ] 基础映射在不同输入法/大小写状态下更可靠

#### 优先级
最高

---

### C-4. 为键盘映射补充最小测试思路文档

#### 状态
- [ ] 未开始

#### 建议新增文件
- `Doc/KeyboardMappingTestCases.md`

#### 任务
- [ ] 列出 A/S/D/F 的 note on/off 验收用例
- [ ] 列出长按重复触发检测用例
- [ ] 列出焦点切换状态恢复用例
- [ ] 列出映射保存/恢复一致性用例

#### 输出结果
- [ ] 后续重构有明确验收基线

#### 优先级
中

---

## 阶段 D：插件宿主主链路落地

### D-1. 扩展 PluginHost，使其支持插件实例化

#### 状态
- [ ] 未开始

#### 文件
- `Source/Plugin/PluginHost.h`
- `Source/Plugin/PluginHost.cpp`

#### 任务
- [ ] 根据 `KnownPluginList` 中的 `PluginDescription` 创建插件实例
- [ ] 管理当前活动插件实例
- [ ] 提供当前插件名称/状态查询接口
- [ ] 增加 `loadPluginByName(...)`
- [ ] 增加 `loadPluginByDescription(...)`
- [ ] 增加 `unloadPlugin()`
- [ ] 增加 `hasLoadedPlugin() const`
- [ ] 增加 `getInstance()`

#### 输出结果
- [ ] `PluginHost` 从扫描器升级为最小宿主控制层

#### 优先级
最高

---

### D-2. 在 PluginHost 中加入生命周期管理接口

#### 状态
- [ ] 未开始

#### 文件
- `Source/Plugin/PluginHost.h`
- `Source/Plugin/PluginHost.cpp`

#### 任务
- [ ] 为当前插件实例增加 `prepareToPlay(double sampleRate, int blockSize)`
- [ ] 增加 `releaseResources()`
- [ ] 处理 bus/layout 初始化
- [ ] 加入加载失败时的错误信息记录
- [ ] 明确重复加载/卸载时的状态切换逻辑

#### 输出结果
- [ ] 插件实例能正确进入音频处理状态

#### 优先级
最高

---

### D-3. 在 AudioEngine 中接入插件处理链路

#### 状态
- [ ] 未开始

#### 文件
- `Source/Audio/AudioEngine.h`
- `Source/Audio/AudioEngine.cpp`

#### 任务
- [ ] 让 `AudioEngine` 支持注入插件宿主引用
- [ ] 优先把 MIDI 喂给已加载插件实例
- [ ] 将插件输出写入目标 buffer
- [ ] 无插件时保留 fallback 发声或静音
- [ ] 明确插件模式与 fallback 模式切换逻辑

#### 输出结果
- [ ] 音频主链路具备插件驱动能力

#### 优先级
最高

---

### D-4. 处理 MIDI 到插件的音频线程链路

#### 状态
- [ ] 未开始

#### 文件
- `Source/Audio/AudioEngine.cpp`
- `Source/Midi/MidiRouter.cpp`
- `Source/MainComponent.cpp`

#### 任务
- [ ] 打通 `KeyboardMidiMapper -> MidiMessageCollector -> AudioEngine -> PluginInstance->processBlock`
- [ ] 打通 `MidiRouter -> MidiMessageCollector -> AudioEngine -> PluginInstance->processBlock`
- [ ] 检查 `MidiBuffer` 生命周期
- [ ] 检查 block 内消息收集方式
- [ ] 检查 `processBlock` 调用时机
- [ ] 检查 buffer 清理策略

#### 输出结果
- [ ] 外部 MIDI 与电脑键盘输入都能驱动插件

#### 优先级
最高

---

### D-5. 选择最小插件加载 UI 方案

#### 状态
- [ ] 未开始

#### 文件
- `Source/MainComponent.h`
- `Source/MainComponent.cpp`

#### 任务
- [ ] 在当前扫描结果基础上增加插件选择控件
- [ ] 增加加载插件按钮
- [ ] 增加卸载插件按钮
- [ ] 显示当前插件加载状态
- [ ] 评估使用 `ComboBox + Load/Unload Button` 的最小方案

#### 输出结果
- [ ] 用户无需手动看文本框猜测插件状态

#### 优先级
高

---

### D-6. 评估并接入插件 editor（可选第二步）

#### 状态
- [ ] 未开始

#### 文件
- `Source/Plugin/PluginHost.h`
- `Source/Plugin/PluginHost.cpp`
- `Source/MainComponent.cpp`

#### 任务
- [ ] 在插件实例稳定加载后评估 `createEditorIfNeeded()`
- [ ] 设计 editor 窗口托管方式
- [ ] 增加 editor 打开/关闭操作
- [ ] 验证 editor 生命周期与插件卸载一致性

#### 输出结果
- [ ] 插件 UI 可打开

#### 优先级
中

---

## 阶段 E：UI 与设置系统接线

### E-1. 扩展 SettingsModel，纳入插件相关状态

#### 状态
- [ ] 未开始

#### 文件
- `Source/Settings/SettingsModel.h`
- `Source/Settings/SettingsStore.cpp`

#### 任务
- [ ] 增加最近使用的插件搜索路径字段
- [ ] 增加上次加载的插件名或唯一标识字段
- [ ] 增加最近使用的布局名称或布局标识字段
- [ ] 完成对应持久化读写逻辑

#### 输出结果
- [ ] 插件与布局使用状态可恢复

#### 优先级
高

---

### E-2. 将插件路径与扫描结果状态纳入 UI 同步

#### 状态
- [ ] 未开始

#### 文件
- `Source/MainComponent.cpp`
- `Source/MainComponent.h`

#### 任务
- [ ] 初始化时恢复插件扫描路径
- [ ] 扫描后更新状态标签
- [ ] 加载插件后更新当前插件状态
- [ ] 出错时显示清晰错误信息
- [ ] 明确扫描成功、加载成功、加载失败三类提示文案

#### 输出结果
- [ ] UI 反馈不再只停留在“扫描完成”层面

#### 优先级
高

---

### E-3. 拆分 MainComponent 的局部职责（第一步）

#### 状态
- [ ] 未开始

#### 文件
- `Source/MainComponent.h`
- `Source/MainComponent.cpp`
- 可选新增：
  - `Source/UI/PluginPanel.h`
  - `Source/UI/PerformancePanel.h`

#### 任务
- [ ] 把插件区逻辑单独封装
- [ ] 把参数控制区逻辑单独封装
- [ ] 降低 `MainComponent` 中的事件处理与 UI 绑定复杂度

#### 输出结果
- [ ] `MainComponent` 体积与职责开始下降

#### 优先级
中高

---

## 阶段 F：文档与验收基线补充

### F-1. 更新 README

#### 状态
- [ ] 未开始

#### 文件
- `README.md`

#### 任务
- [ ] 补充项目目标
- [ ] 补充当前已实现能力
- [ ] 补充当前未实现能力
- [ ] 补充构建命令
- [ ] 补充 JUCE 子模块说明
- [ ] 补充 `freepiano-src/` 的定位说明

#### 输出结果
- [ ] 仓库对外说明更完整

#### 优先级
中

---

### F-2. 建立阶段验收清单

#### 状态
- [ ] 未开始

#### 建议新增文件
- `Doc/MilestoneChecklist.md`

#### 任务
- [ ] 定义“能扫描到 VST3”的验收项
- [ ] 定义“能加载一个 VST3 乐器”的验收项
- [ ] 定义“A/S/D/F 可触发插件发声”的验收项
- [ ] 定义“外部 MIDI 输入可驱动插件”的验收项
- [ ] 定义“映射与音频设置重启后可恢复”的验收项

#### 输出结果
- [ ] 每次迭代有明确验收标准

#### 优先级
中

---

# 4. 建议执行顺序

## 第一组：必须先做
- [ ] A-1 清理重复/遗留文件
- [ ] B-1 新建 `Source/Core/`
- [ ] B-2 建立键位映射核心类型
- [ ] C-1 改造 `KeyboardMidiMapper`
- [ ] C-2 接入 `SettingsModel.keyMap`
- [ ] D-1 扩展 `PluginHost` 支持实例化
- [ ] D-2 增加插件 prepare/release 生命周期
- [ ] D-3 在 `AudioEngine` 接入插件链路
- [ ] D-4 打通 MIDI 到插件的主音频链路

## 第二组：紧随其后
- [ ] D-5 增加最小插件加载 UI
- [ ] E-1 扩展插件状态持久化
- [ ] E-2 完善 UI 状态反馈
- [ ] F-1 更新 README
- [ ] F-2 建立阶段验收清单

## 第三组：后续优化
- [ ] E-3 局部拆分 `MainComponent`
- [ ] D-6 接入插件 editor
- [ ] C-4 键盘映射测试文档
- [ ] 录制/回放/导出功能设计与实现

---

# 5. 最近一轮建议落地范围

如果下一轮开发希望控制范围、快速见效，建议只聚焦以下文件。

## 第一批重点文件
- [ ] `Source/Input/KeyboardMidiMapper.h`
- [ ] `Source/Input/KeyboardMidiMapper.cpp`
- [ ] `Source/Plugin/PluginHost.h`
- [ ] `Source/Plugin/PluginHost.cpp`
- [ ] `Source/Audio/AudioEngine.h`
- [ ] `Source/Audio/AudioEngine.cpp`
- [ ] `Source/MainComponent.h`
- [ ] `Source/MainComponent.cpp`
- [ ] `Source/Settings/SettingsModel.h`
- [ ] `Source/Settings/SettingsStore.cpp`
- [ ] `CMakeLists.txt`

## 第一批重点新增文件
- [ ] `Source/Core/KeyMapTypes.h`
- [ ] `Source/Core/MidiTypes.h`
- [ ] `Doc/MilestoneChecklist.md`
- [ ] `Doc/KeyboardMappingTestCases.md`

---

# 6. 本清单的使用方式

- [ ] 每开始一轮开发前，从“建议执行顺序”里选择一个小批次任务
- [ ] 完成后更新对应任务状态
- [ ] 如果新增模块，及时补充到本清单中
- [ ] 每完成一个阶段，就补充新的验收项文档

这样可以保证重构过程持续可控，避免再次回到“旧项目功能很多但结构失控”的状态。

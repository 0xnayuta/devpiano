# devpiano 下一阶段任务清单（按文件级别拆解，执行版）

> 用途：本文件用于作为下一阶段开发执行清单，支持直接勾选任务状态。
>
> 状态标记说明：
> - [x] 已完成
> - [ ] 未开始 / 未完成
> - [~] 进行中 / 部分完成

## 构建基线（统一）
- 配置：`cmake --preset ninja-x64`
- Debug 构建：`cmake --build --preset ninja-debug`
- Release 构建：`cmake --build --preset ninja-release`

---

## 1. 目标说明

本清单用于把当前“分析与规划”落到可执行的下一阶段开发任务上。

下一阶段的主目标聚焦于四件事：

- [~] 清理当前结构中的重复与歧义
- [~] 建立统一核心模型层
- [ ] 将键盘映射升级为可配置系统
- [ ] 将插件扫描升级为真正可发声的插件宿主主链路

---

## 2. 阶段划分总览

### 阶段 A：结构清障
- [x] 清理重复文件与过渡实现
- [x] 降低后续重构风险

### 阶段 B：核心模型层建设
- [~] 建立强类型、跨平台的数据结构

### 阶段 C：键盘映射系统升级
- [~] 从硬编码映射升级为可配置映射

### 阶段 D：插件宿主主链路落地
- [~] 从“仅扫描”升级到“可实例化并发声”

### 阶段 E：UI 与状态接线
- [ ] 将上面能力接入当前界面与持久化系统

### 阶段 F：文档与验收基线补充
- [x] 构建系统迁移：已切换为 CMake + Ninja 预设（`ninja-x64` / `ninja-debug` / `ninja-release`）
- [ ] 补全文档与阶段验收标准

---

# 3. 文件级任务清单

## 阶段 A：结构清障

### 阶段状态
- [x] 已完成（A-1、A-2 已完成）

### A-1. 审核并处理未参与当前构建的遗留文件

#### 状态
- [x] 已完成

#### 文件
- `Source/AudioEngine.h` -> `Source/Legacy/UnusedPrototypes/AudioEngine.h`
- `Source/AudioEngine.cpp` -> `Source/Legacy/UnusedPrototypes/AudioEngine.cpp`
- `Source/MidiRouter.h` -> `Source/Legacy/UnusedPrototypes/MidiRouter.h`
- `Source/MidiRouter.cpp` -> `Source/Legacy/UnusedPrototypes/MidiRouter.cpp`
- `Source/SongEngine.h` -> `Source/Legacy/UnusedPrototypes/SongEngine.h`
- `Source/SongEngine.cpp` -> `Source/Legacy/UnusedPrototypes/SongEngine.cpp`
- 新增说明：`Source/Legacy/UnusedPrototypes/README.md`

#### 任务
- [x] 确认这些文件不参与当前主构建
- [x] 将历史过渡实现移入 `Source/Legacy/UnusedPrototypes/`
- [x] 避免与当前主实现重名并存在 `Source/` 根目录
- [x] 为遗留原型补充目录说明文档

#### 输出结果
- [x] `Source/` 根目录已不再混放这些旧原型文件
- [x] 当前有效主实现路径更清晰：`Source/Audio/*`、`Source/Midi/*`
- [x] Debug 构建复验通过

#### 优先级
高

---

### A-2. 为旧版参考源码补充使用边界说明

#### 状态
- [x] 已完成

#### 文件
- `README.md`
- 新增：`Doc/LegacyCodeNotes.md`

#### 任务
- [x] 在 README 中说明 `freepiano-src/` 仅供迁移参考
- [x] 在 README 中说明 `freepiano-src/` 不参与当前主构建
- [x] 在 README 中说明不应直接复制平台相关旧实现
- [x] 新增 `Doc/LegacyCodeNotes.md` 记录旧模块与新模块映射关系

#### 输出结果
- [x] 降低误读旧代码的风险
- [x] 为后续从旧代码提炼逻辑提供结构化说明

#### 优先级
中高

---

## 阶段 B：核心模型层建设

### 阶段状态
- [~] 进行中（B-1、B-2 已完成，B-4 已进入第一轮落地，B-3 未开始）

### B-1. 新建核心类型目录

#### 状态
- [x] 已完成

#### 新增目录
- `Source/Core/`

#### 任务
- [x] 在 CMake 中纳入新目录下的头文件/源文件
- [x] 将核心数据类型集中到 `Source/Core/`

#### 输出结果
- [x] 项目已开始形成更清晰的 Core / Input / Audio / Plugin / Settings 分层

#### 优先级
高

---

### B-2. 建立键位映射核心类型

#### 状态
- [x] 已完成第一版

#### 建议新增文件
- `Source/Core/KeyMapTypes.h`

#### 任务
- [x] 定义 `KeyBinding`
- [x] 定义 `KeyboardLayout`
- [x] 定义 `KeyAction`
- [x] 定义默认布局生成函数
- [x] 设计基于稳定 key code 的键位表示方式
- [x] 纳入 MIDI note / channel / velocity / 触发方式等字段

#### 输出结果
- [~] 已建立独立键位模型，后续 C-1 将把 `KeyboardMidiMapper` 切换到这套模型

#### 优先级
最高

---

### B-3. 建立 MIDI 相关基础类型

#### 状态
- [~] 进行中（已完成第一版轻量类型头文件）

#### 建议新增文件
- `Source/Core/MidiTypes.h`

#### 任务
- [x] 定义 `MidiNoteNumber`
- [x] 定义 `MidiChannel`
- [x] 定义 `Velocity`
- [x] 定义 `NoteRange`
- [x] 让类型保持轻量，不做过度设计

#### 输出结果
- [~] 已建立第一版轻量 MIDI 强类型入口，且 `Source/Core/KeyMapTypes.h` 已开始以兼容方式轻量接入这些类型；`KeyboardMidiMapper` 的最小取值路径与 `SettingsModel` 的布局转换辅助也已开始优先使用强类型 helper；后续可逐步替换代码中散落的裸 `int` / `float`

#### 优先级
中高

---

### B-4. 建立应用状态聚合模型

#### 状态
- [~] 进行中（已完成第一版轻量聚合头文件与快照入口）

#### 建议新增文件
- `Source/Core/AppState.h`

#### 任务
- [x] 设计音频设置聚合结构
- [x] 设计插件选择状态结构
- [x] 设计键盘布局状态结构
- [x] 设计演奏参数状态结构
- [~] 明确后续与 `SettingsModel` 的迁移关系

#### 输出结果
- [~] 已新增 `Source/Core/AppState.h`，并由 `MainComponent::createAppStateSnapshot()` 提供第一轮快照入口，为后续 UI / 引擎 / 设置三方同步做准备
- [~] `HeaderPanel` 的 MIDI 状态已开始优先经由 AppState 快照驱动，作为第一块消费 AppState 的 UI 区域
- [x] `AppState::PluginState` 已补齐第一轮插件展示字段，并已开始驱动 `PluginPanel` 的只读展示状态
- [x] `PluginPanelStateBuilder` 已增加基于 `AppState` / `PluginState` 的构造入口
- [x] 已新增轻量 `AppStateBuilder`，开始明确区分 persisted settings 基线与 runtime overlays
- [x] 已补充 `SettingsModel` / `AppState` / `AppStateBuilder` 的职责边界注释与结构说明
- [x] 已在 `SettingsModel` 内完成轻量逻辑分区 view（audio / performance / plugin recovery / input mapping）第一轮整理，且不改持久化键名与序列化格式
- [x] `MainComponent` 中少量 persisted 读取路径（插件恢复路径、性能参数恢复、音频基线 fallback、启动布局恢复）已开始优先走 grouped view
- [x] `SettingsModel` 已新增轻量 grouped write helper，`syncSettingsFromUi()`、`prepareToPlay()`、启动恢复与扫描相关的少量 persisted 写路径，以及音频设备序列化状态写入已开始优先走分区/helper 写入口
- [x] 已新增基于单次 AppState 快照的只读 UI 应用入口，用于统一分发 `HeaderPanel` / `PluginPanel` 状态
- [x] `SettingsDialog` 保存/关闭流程已开始收口为更明确的 helper

#### 优先级
中

---

## 阶段 C：键盘映射系统升级

### 阶段状态
- [~] 进行中（C-1、C-2、C-3 已完成，C-4 未完成）

### C-1. 改造 KeyboardMidiMapper 的内部模型

#### 状态
- [x] 已完成第一轮改造

#### 文件
- `Source/Input/KeyboardMidiMapper.h`
- `Source/Input/KeyboardMidiMapper.cpp`

#### 任务
- [x] 去掉对固定 `unordered_map<int, int>` 内部模型的依赖
- [x] 支持可注入映射表
- [x] 接入 `Source/Core/KeyMapTypes.h`
- [x] 增加 `setLayout(...)`
- [x] 增加 `getLayout()`
- [x] 增加 `resetToDefaultLayout()`

#### 输出结果
- [x] `KeyboardMidiMapper` 已从硬编码工具类演进为基于 `KeyboardLayout` 的状态化模块
- [~] 仍保留少量字符归一化逻辑，后续 C-3 继续收敛输入捕获方式

#### 优先级
最高

---

### C-2. 将 SettingsModel.keyMap 与 KeyboardMidiMapper 接线

#### 状态
- [x] 已完成第一轮接线

#### 文件
- `Source/Settings/SettingsModel.h`
- `Source/Settings/SettingsStore.cpp`
- `Source/Input/KeyboardMidiMapper.h`
- `Source/Input/KeyboardMidiMapper.cpp`
- `Source/MainComponent.cpp`

#### 任务
- [x] 定义 `SettingsModel.keyMap` 与新布局模型之间的转换关系
- [x] 程序启动时从设置恢复布局并注入 `KeyboardMidiMapper`
- [x] 程序退出或保存时将当前布局写回设置
- [x] 默认布局为空时回退到内建默认布局

#### 输出结果
- [x] 当前键位映射已具备基础持久化闭环
- [~] 目前仍复用 legacy `keyMap` 存储形态，后续可再升级为完整布局序列化

#### 优先级
最高

---

### C-3. 修正输入捕获方式，减少字符布局依赖

#### 状态
- [x] 已完成第一轮收敛

#### 文件
- `Source/Input/KeyboardMidiMapper.cpp`
- `Source/Core/KeyMapTypes.h`

#### 任务
- [x] 评估当前 `key.getTextCharacter()` 的局限
- [x] 改用更稳定的 `key.getKeyCode()` 方案作为主路径
- [x] 保留必要的 key code 规范化逻辑
- [~] 按键释放检测逻辑已保留现有稳定路径，仍需后续手工测试验证
- [ ] 焦点切换后 held key 状态残留问题仍待专项验证

#### 输出结果
- [~] 基础映射对输入法/字符文本的依赖已降低，后续仍需结合手工测试继续验证

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

### 阶段状态
- [~] 进行中（D-1、D-2、D-3、D-4、D-5 已完成，D-6 已完成第一轮）

### D-1. 扩展 PluginHost，使其支持插件实例化

#### 状态
- [x] 已完成第一轮

#### 文件
- `Source/Plugin/PluginHost.h`
- `Source/Plugin/PluginHost.cpp`

#### 任务
- [x] 根据 `KnownPluginList` 中的 `PluginDescription` 创建插件实例
- [x] 管理当前活动插件实例
- [x] 提供当前插件名称/状态查询接口
- [x] 增加 `loadPluginByName(...)`
- [x] 增加 `loadPluginByDescription(...)`
- [x] 增加 `unloadPlugin()`
- [x] 增加 `hasLoadedPlugin() const`
- [x] 增加 `getInstance()`

#### 输出结果
- [x] `PluginHost` 已从纯扫描器升级为最小宿主控制层
- [~] 当前仍未接入 `prepareToPlay` / `releaseResources` / `processBlock`，这些将在 D-2 / D-3 中完成

#### 优先级
最高

---

### D-2. 在 PluginHost 中加入生命周期管理接口

#### 状态
- [x] 已完成第一轮

#### 文件
- `Source/Plugin/PluginHost.h`
- `Source/Plugin/PluginHost.cpp`

#### 任务
- [x] 为当前插件实例增加 `prepareToPlay(double sampleRate, int blockSize)`
- [x] 增加 `releaseResources()`
- [x] 处理基础 bus/layout 初始化
- [x] 加入加载失败时的错误信息记录
- [x] 明确重复加载/卸载时的状态切换逻辑

#### 输出结果
- [x] 插件实例已具备基础生命周期管理接口
- [~] 真正的音频处理接线仍需在 D-3 中完成

#### 优先级
最高

---

### D-3. 在 AudioEngine 中接入插件处理链路

#### 状态
- [x] 已完成第一轮接入

#### 文件
- `Source/Audio/AudioEngine.h`
- `Source/Audio/AudioEngine.cpp`

#### 任务
- [x] 让 `AudioEngine` 支持注入插件宿主引用
- [x] 优先把 MIDI 喂给已加载插件实例
- [x] 将插件输出写入目标 buffer
- [x] 无插件时保留 fallback 发声路径
- [x] 明确插件模式与 fallback 模式切换逻辑

#### 输出结果
- [x] 音频主链路已具备插件驱动能力的第一轮实现
- [~] 仍需通过实际插件加载与手工演奏验证效果

#### 优先级
最高

---

### D-4. 处理 MIDI 到插件的音频线程链路

#### 状态
- [~] 已完成第一轮链路加固，并补充外部 MIDI 活动反馈；因暂时无外部 MIDI 设备，功能项暂无法手工验证

#### 文件
- `Source/Audio/AudioEngine.cpp`
- `Source/Midi/MidiRouter.cpp`
- `Source/MainComponent.cpp`

#### 任务
- [x] 打通 `KeyboardMidiMapper -> MidiMessageCollector -> AudioEngine -> PluginInstance->processBlock`
- [x] 打通 `MidiRouter -> MidiMessageCollector -> AudioEngine -> PluginInstance->processBlock`
- [x] 检查 `MidiBuffer` 生命周期
- [x] 检查 block 内消息收集方式
- [x] 检查 `processBlock` 调用时机
- [x] 检查 buffer 清理策略
- [x] 加入运行时 sample rate / block size 获取逻辑用于插件加载
- [x] 在 UI 中补充已加载插件的 prepared 状态反馈

#### 输出结果
- [~] 代码路径已具备外部 MIDI 与电脑键盘驱动插件的能力，并已补充活动状态反馈；因暂时无外部 MIDI 设备，功能项暂无法手工验证

#### 优先级
最高

---

### D-5. 选择最小插件加载 UI 方案

#### 状态
- [x] 已完成第一轮

#### 文件
- `Source/MainComponent.h`
- `Source/MainComponent.cpp`

#### 任务
- [x] 在当前扫描结果基础上增加插件选择控件
- [x] 增加加载插件按钮
- [x] 增加卸载插件按钮
- [x] 显示当前插件加载状态
- [x] 使用 `ComboBox + Load/Unload Button` 完成最小方案

#### 输出结果
- [x] 用户已无需只看文本框猜测插件状态
- [~] 仍需结合真实插件加载做手工验证

#### 优先级
高

---

### D-6. 评估并接入插件 editor（可选第二步）

#### 状态
- [x] 已完成第一轮

#### 文件
- `Source/Plugin/PluginHost.cpp`
- `Source/MainComponent.h`
- `Source/MainComponent.cpp`

#### 任务
- [x] 评估并改用支持 editor 的默认插件格式注册路径
- [x] 设计 editor 独立窗口托管方式
- [x] 增加 editor 打开/关闭操作
- [x] 验证 editor 生命周期与插件卸载一致性（第一轮）
- [x] 验证轻量插件 Surge XT editor 可正常弹出并操作

#### 输出结果
- [x] 插件 UI 已可打开并操作
- [~] Debug 退出阶段在特定插件/系统注入环境下仍可能出现 leak detector 告警，暂记为已知调试告警

#### 优先级
中

---

## 阶段 E：UI 与设置系统接线

### E-1. 扩展 SettingsModel，纳入插件相关状态

#### 状态
- [x] 已完成第一轮

#### 文件
- `Source/Settings/SettingsModel.h`
- `Source/Settings/SettingsStore.cpp`

#### 任务
- [x] 增加最近使用的插件搜索路径字段
- [x] 增加上次加载的插件名字段
- [x] 增加最近使用的布局标识字段
- [x] 完成对应持久化读写逻辑

#### 输出结果
- [x] 插件与布局使用状态已具备基础恢复能力
- [x] 启动时可自动恢复上次插件扫描路径与最近一次插件选择/加载尝试
- [x] 已支持恢复上次插件扫描路径与上次插件选择/加载的第一轮自动恢复

#### 优先级
高

---

### E-2. 将插件路径与扫描结果状态纳入 UI 同步

#### 状态
- [x] 已完成第一轮

#### 文件
- `Source/MainComponent.cpp`
- `Source/MainComponent.h`

#### 任务
- [x] 初始化时恢复插件扫描路径
- [x] 扫描后更新状态标签
- [x] 加载插件后更新当前插件状态
- [x] 出错时显示清晰错误信息
- [x] 明确扫描成功、加载成功、加载失败三类提示文案（第一轮）

#### 输出结果
- [x] UI 反馈已超出“仅扫描完成”层面，并同步最近插件/编辑器状态
- [x] 启动阶段已接入自动扫描与恢复最近插件状态的第一轮逻辑- [x] 启动时已支持自动扫描上次路径并尝试恢复上次插件

#### 优先级
高

---

### E-3. 拆分 MainComponent 的局部职责（第一步）

#### 状态
- [x] 已完成第一轮拆分与 helper 收敛阶段（插件区 + 参数区 + 头部状态区 + 键盘区 + AppState 接线 + 生命周期/插件恢复 helper 收口）

#### 文件
- `Source/MainComponent.h`
- `Source/MainComponent.cpp`
- 新增：
  - `Source/UI/PluginPanel.h`
  - `Source/UI/PluginPanel.cpp`
  - `Source/UI/ControlsPanel.h`
  - `Source/UI/ControlsPanel.cpp`
  - `Source/UI/HeaderPanel.h`
  - `Source/UI/HeaderPanel.cpp`
  - `Source/UI/KeyboardPanel.h`
  - `Source/UI/KeyboardPanel.cpp`

#### 任务
- [x] 把插件区逻辑单独封装
- [x] 把参数控制区逻辑单独封装
- [x] 把头部状态区与设置入口单独封装
- [x] 把键盘区单独封装
- [x] 降低 `MainComponent` 中的插件事件处理与 UI 绑定复杂度

#### 输出结果
- [x] `MainComponent` 体积与职责已开始下降
- [x] 插件区、参数区、头部状态区与键盘区已完成第一轮组件化拆分
- [x] MIDI / 插件状态展示更新逻辑已开始向 `HeaderPanel` / `PluginPanel` 收敛
- [x] 插件区刷新已统一为单一入口，减少重复的状态刷新调用
- [x] 插件区状态组装已抽离到轻量 `PluginPanelStateBuilder`
- [x] 头部 MIDI 状态组装已抽离到轻量 `HeaderPanelStateBuilder`
- [x] 设置收集 / 立即保存 / 延迟保存路径已完成第一轮轻量收敛
- [x] 运行时音频设备重建前后的状态抓取 / shutdown / reinit 路径已完成第一轮轻量收敛
- [x] 插件相关 UI 收尾动作（保存 / 刷新 / 焦点恢复）已完成第一轮轻量收敛
- [x] 插件操作主流程（运行时参数获取 / 设备重建 / 插件动作）已完成第一轮轻量收敛
- [x] 启动阶段的插件扫描 / 恢复上次插件路径已完成第一轮轻量收敛
- [x] `ControlsPanel` 的演奏参数读写已开始收口为性能设置 helper，`onValuesChanged` 路径重复同步已减少
- [x] `MainComponent` 构造流程已进一步拆分为输入映射恢复 / UI 初始化 / MIDI 路由初始化三个 helper，降低单函数装配复杂度
- [x] 启动阶段的插件恢复流程已进一步拆分为“扫描路径恢复”和“上次插件恢复”两个 helper，并收敛扫描路径解析入口，降低启动链路阅读与改动风险
- [x] 插件相关状态写回 settings 的重复逻辑已收口为轻量 helper，减少插件扫描 / 启动恢复 / 手动加载路径中的重复字段拼装
- [x] 插件恢复 settings 的读取 + fallback 逻辑已抽到轻量 helper，减少默认路径回退规则的分散实现
- [x] 插件操作后的“状态写回 + UI 收尾”已开始收口到统一 helper，减少 scan/load 路径重复收尾调用
- [x] `loadSelectedPlugin()` 已按“选择名解析 + 实际加载提交”两段式 helper 轻量拆分，降低单函数分支复杂度
- [x] `togglePluginEditor()` 已按“editor 可创建性检查 + 窗口构建”两段式 helper 轻量拆分，降低早退分支密度
- [x] `unloadCurrentPlugin()` 已通过 `unloadPluginAndCommitState()` 收口为统一路径，与 scan/load 的状态提交风格对齐
- [x] `scanPlugins()` 已拆分为“路径解析 + `scanPluginsAtPathAndCommitState(...)` 扫描提交”两段式 helper，降低函数职责耦合
- [x] 启动恢复路径中的扫描逻辑已复用 `scanPluginsAtPathAndApplyRecoveryState(...)`，减少启动/手动扫描重复实现
- [x] 插件扫描空路径保护已统一收口为 `isUsablePluginScanPath(...)`，减少启动恢复与手动扫描分散判定
- [x] `runPluginActionWithAudioDeviceRebuild(void)` 已复用带 `RuntimeAudioConfig` 的主实现，减少设备重建包裹重复代码
- [x] `restoreLastPluginOnStartup()` 已按“恢复名称读取 + 实际恢复加载”两段式 helper 拆分，统一 last plugin name 读取命名
- [x] `PluginRecoverySettingsView` 的若干结构体字面量已开始收口为轻量 builder/helper，减少路径与 last plugin name 字段重复拼装
- [x] `getPluginRecoverySettingsWithFallback()` 已复用 `makePluginRecoverySettings(...)`，进一步统一 recovery settings 构造入口
- [x] plugin editor 窗口托管已下沉到 `Source/UI/PluginEditorWindow.*`，`MainComponent` 不再内嵌窗口类实现
- [x] plugin editor 标题生成逻辑已下沉到 `PluginEditorWindow`，`MainComponent` 不再拼接 editor 窗口标题
- [x] plugin editor close 后的异步收尾已收口为 `MainComponent::handlePluginEditorWindowClosedAsync()`，进一步缩短 `openPluginEditorWindow()`
- [x] plugin editor 打开后的只读展示刷新已统一改走 `refreshReadOnlyUiState()`，减少 `MainComponent` 直接刷新 `PluginPanel` 展示细节
- [x] 已完成一轮 `MainComponent` 收敛阶段整理：插件恢复 / 扫描 / load / unload / editor / recovery settings / editor window 托管 等相关 helper 路径已形成较清晰边界，后续可转入其他更有功能收益的目标

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
- [x] A-1 清理重复/遗留文件
- [x] A-2 为旧版参考源码补充使用边界说明
- [x] B-1 新建 `Source/Core/`
- [x] B-2 建立键位映射核心类型
- [x] C-1 改造 `KeyboardMidiMapper`
- [x] C-2 接入 `SettingsModel.keyMap`
- [x] D-1 扩展 `PluginHost` 支持实例化
- [x] D-2 增加插件 prepare/release 生命周期
- [x] D-3 在 `AudioEngine` 接入插件链路
- [~] D-4 打通 MIDI 到插件的主音频链路

## 第二组：紧随其后
- [x] D-5 增加最小插件加载 UI
- [ ] E-1 扩展插件状态持久化
- [ ] E-2 完善 UI 状态反馈
- [ ] F-1 更新 README
- [ ] F-2 建立阶段验收清单

## 第三组：后续优化
- [ ] E-3 局部拆分 `MainComponent` (进一步优化)
- [ ] D-6 接入插件 editor (进一步细化)
- [ ] C-4 键盘映射测试文档 (实测与完成)
- [ ] 录制/回放/导出功能设计与实现 (预研中)

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

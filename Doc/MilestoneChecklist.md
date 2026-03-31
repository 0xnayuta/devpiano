# devpiano 阶段里程碑验收清单（执行更新版）

> 用途：本文件用于定义各阶段的可验证验收标准，避免重构过程中只“写了代码”但没有形成可确认的阶段成果。
>
> 状态标记说明：
> - [x] 已通过
> - [ ] 未通过 / 未开始验证
> - [~] 部分通过 / 待补充验证

## 构建基线（统一）
- 配置：`cmake --preset ninja-x64`
- Debug 构建：`cmake --build --preset ninja-debug`
- Release 构建：`cmake --build --preset ninja-release`

---

## 1. 当前里程碑总览

### M0：工程骨架可运行
- [x] JUCE GUI 程序可正常启动
- [x] Debug 构建可通过
- [x] 主窗口可显示
- [x] 音频设备能初始化

### M1：最小演奏链路成立
- [x] 电脑键盘可触发 note on/off
- [x] 虚拟钢琴键盘可联动显示
- [x] 外部 MIDI 输入可打开
- [x] 程序能够发出声音（当前为内置正弦波或已加载插件）

### M2：最小插件扫描能力成立
- [x] 可识别插件格式管理器
- [x] VST3 格式可用
- [x] 可扫描默认或指定目录
- [x] 可显示扫描到的插件列表

### M3：插件实例化并发声
- [x] 可选择并加载一个 VST3 乐器
- [x] 键盘输入可驱动该插件发声
- [ ] 外部 MIDI 输入可驱动该插件发声
- [x] 插件卸载后程序仍稳定

### M4：键盘映射系统可配置
- [x] 映射主路径不再依赖字符输入
- [x] 映射可保存
- [x] 映射可恢复
- [x] 默认布局可重置（代码路径已具备）

### M5：UI 进入正式可用阶段
- [x] 插件状态显示清晰
- [x] 插件选择/加载/卸载路径完整
- [x] `MainComponent` 职责得到初步拆分
- [x] 用户无需依赖调试性文本框理解插件状态
- [x] 已支持打开并操作已加载插件的 editor 窗口

### M6：高级功能恢复
- [ ] 支持布局 Preset 保存/加载
- [ ] 支持录制与回放
- [ ] 支持 MIDI 或 WAV 导出

---

## 2. 近期重点里程碑验收项

## M0：工程骨架可运行

### 验收目标
确保当前主干具备稳定开发基础。

### 验收项
- [x] `cmake --preset ninja-x64 && cmake --build --preset ninja-debug` 构建成功
- [x] 生成 `DevPiano.exe`
- [x] 程序启动后主窗口正常显示
- [x] 没有因缺失 JUCE 子模块导致构建失败

### 验收记录
- 当前状态：已通过
- 备注：当前构建基线已统一为 CMake + Ninja（`ninja-x64` / `ninja-debug`），可作为后续迭代的基础基线

---

## M1：最小演奏链路成立

### 验收目标
确保“输入 -> MIDI -> 声音输出”的基本闭环存在。

### 验收项
- [x] 按下 `A/S/D/F` 等基础按键时可触发 note on
- [x] 松开对应按键时可触发 note off
- [x] 虚拟钢琴键盘组件可高亮联动
- [x] 外部 MIDI 设备打开后，消息能进入音频引擎
- [x] 调整音量与 ADSR 滑块后可听到参数变化
- [x] 切换窗口焦点后 held key 不残留
- [x] 长按某按键时不会异常重复触发

### 验收记录
- 当前状态：已通过
- 备注：键盘基本演奏稳定性已完成一轮手工验证

---

## M2：最小插件扫描能力成立

### 验收目标
确保项目已经具备插件生态入口，而非纯内置演示程序。

### 验收项
- [x] 程序能识别可用插件格式
- [x] `PluginHost` 能报告 VST3 可用状态
- [x] 输入默认 VST3 搜索路径后可执行扫描
- [x] 扫描完成后可在界面列出插件名称
- [x] 扫描失败不会导致程序崩溃

### 待补充验证
- [ ] 扫描结果是否能持久化
- [ ] 扫描失败文件是否能正确记录
- [ ] 多目录搜索路径是否表现稳定

### 验收记录
- 当前状态：部分通过（扫描能力已稳定，持久化与错误处理待完善）

---

## M3：插件实例化并发声

### 验收目标
完成当前最关键的产品级突破：从“会扫描插件”升级为“真正使用插件发声”。

### 验收项
- [x] 扫描结果中可选择一个 VST3 乐器
- [x] 点击加载后能成功创建 `AudioPluginInstance`
- [x] 插件加载成功后 UI 显示明确状态
- [x] 按下 `A/S/D/F` 等按键时插件能够发声
- [~] 外部 MIDI 输入通路已接通，当前已增加活动反馈；因暂时无外部 MIDI 设备，功能项暂无法手工验证
- [x] 插件处理链路接入 `processBlock`
- [x] 插件卸载后不会崩溃或留下非法状态
- [x] 未加载插件时程序仍保持可运行

### 已验证插件类型
- [x] 选择一个简单的 VST3 乐器作为首测目标（Surge XT）
- [x] 选择一个加载较慢/较复杂的插件观察行为（Kontakt 曾暴露通道与稳定性问题）
- [~] 加载失败场景的错误提示已有基础支持，仍可继续补充边界验证

### 验收记录
- 当前状态：基本通过
- 备注：轻量插件已完成成功加载与发声验证；外部 MIDI 驱动插件的代码路径与活动反馈已具备，但因暂时无外部 MIDI 设备，功能项暂无法手工验证

---

## M4：键盘映射系统可配置

### 验收目标
把当前硬编码键盘映射升级为可持续演进的用户输入系统。

### 验收项
- [x] 映射内部模型不再只是 `unordered_map<int, int>`
- [x] 映射主路径不再强依赖 `key.getTextCharacter()`
- [x] 默认布局可加载
- [x] 当前布局可保存到设置
- [x] 重启程序后布局可恢复
- [x] 可恢复默认布局（代码路径已具备）
- [x] 焦点切换后按键状态仍稳定

### 建议手工用例
- [x] `A/S/D/F/G/H/J` 行映射连续正确
- [ ] `Q/W/E/R/T/Y/U` 行映射连续正确
- [ ] `Z/X/C/V/B/N/M` 行映射连续正确
- [ ] 保存布局后重启程序验证一致性

### 验收记录
- 当前状态：部分通过
- 备注：主路径、持久化与稳定性已完成一轮验证，完整默认布局全量回归仍可继续补齐

---

## M5：UI 进入正式可用阶段

### 验收目标
让当前界面从“调试面板”迈向“可用产品界面”。

### 验收项
- [x] 插件扫描、选择、加载、卸载路径完整
- [x] 插件状态标签表达清晰
- [x] MIDI 状态反馈清晰
- [x] 用户可以理解当前发声来源（未加载插件时 fallback；加载插件时显示 Loaded）
- [x] `MainComponent` 已有局部职责拆分
- [x] 主要交互控件布局稳定，不明显拥挤
- [x] 设置窗口可通过 Save / X / Esc 关闭
- [x] 设置窗口关闭时可自动保存修改后的设备状态
- [x] 设置窗口通过 X / Esc 关闭时不会导致程序崩溃

### 待补充项
- [ ] 更好的错误提示与空状态提示
- [~] Debug 退出阶段在特定插件/系统注入环境下，仍可能出现 `VST3HostContextHeadless` / `AsyncUpdater` leak detector 告警，但当前不影响程序正常退出与核心功能验证

### 验收记录
- 当前状态：部分通过
- 备注：最小可用 UI 与插件 editor 路径已成型；当前已完成插件区、参数区、头部状态区与键盘区拆分，并已将插件区刷新统一为单一入口、将插件区与头部 MIDI 状态组装抽离到轻量 builder，同时完成设置保存路径、运行时音频设备重建路径、插件相关 UI 收尾动作、插件操作主流程、启动阶段扫描恢复路径，以及轻量 AppState 聚合入口的第一轮实现；其中 `HeaderPanel` 的 MIDI 状态与 `PluginPanel` 的只读展示状态已开始优先经由 AppState 快照驱动，并通过 `AppStateBuilder` 开始明确区分 persisted settings 基线与 runtime overlays；本轮已补充相关职责边界注释说明，在 `SettingsModel` 内完成轻量逻辑分区 view 的第一轮整理，并让更多 persisted 读取路径（含启动布局恢复与启动插件恢复）开始优先走 grouped view，同时让 `syncSettingsFromUi()`、`prepareToPlay()`、启动恢复与扫描相关的少量 persisted 写路径，以及音频设备序列化状态写入开始优先走 grouped/helper 写入口；此外已新增 `Source/Core/MidiTypes.h` 作为第一版轻量 MIDI 强类型入口，并让 `Source/Core/KeyMapTypes.h` 开始以兼容方式轻量接入这些类型，`KeyboardMidiMapper` 的最小取值路径与 `SettingsModel` 的布局转换辅助也已开始优先使用 MIDI 强类型 helper；同时 `MainComponent` 已增加基于单次 AppState 快照的只读 UI 应用入口，`SettingsDialog` 相关保存/关闭流程与 `ControlsPanel` 演奏参数读写流程也已开始收口为更明确的 helper。下一步重点是状态持久化与进一步减少 `MainComponent` 状态装配负担

---

## M6：高级功能恢复

### 验收目标
基于新架构逐步恢复旧版实用功能。

### 验收项

#### 布局 Preset
- [ ] 可保存当前键盘布局
- [ ] 可加载历史布局
- [ ] 可恢复默认布局

#### 录制 / 回放
- [ ] 可开始录制
- [ ] 可停止录制
- [ ] 可回放录制结果
- [ ] 回放期间事件时间顺序正确

#### 导出
- [ ] 可导出 MIDI
- [ ] 可导出 WAV
- [ ] 导出失败时有清晰错误提示

### 验收记录
- 当前状态：未开始

---

## 3. 近期建议作为“完成标准”的最小集合

如果下一轮目标是明确、聚焦并且可交付，建议把以下项目视为最近一次迭代的“完成标准”：

### 最小完成标准（建议优先）
- [x] 能扫描到至少一个 VST3 插件
- [x] 能从扫描结果中选择并加载一个 VST3 乐器
- [x] `A/S/D/F` 能驱动该插件发声
- [~] 外部 MIDI 输入通路已接通并增加活动状态反馈；因暂时无外部 MIDI 设备，功能项暂无法手工验证
- [x] 卸载插件后程序保持稳定
- [x] 键盘映射可保存并恢复
- [x] 中文输入法激活时仍可发声，且不弹出候选词栏

---

## 4. 每轮迭代结束时建议补充的记录项

每轮迭代完成后，建议在本文件补充：

- [x] 本轮目标
- [x] 实际完成项
- [x] 未完成原因
- [x] 新发现风险
- [x] 下一轮入口任务

可按以下模板追加：

```md
## Iteration YYYY-MM-DD
- 目标：
- 完成：
- 未完成：
- 风险：
- 下一步：
```

---

## Iteration 2026-03-26
- 目标：打通最小插件宿主主链路，修复键盘映射与 IME 问题，形成可验证闭环。
- 完成：
  - 完成插件扫描、选择、加载、卸载最小 UI
  - 完成 `PluginHost` 实例化、prepare/release 与基础 bus 配置
  - 完成 `AudioEngine -> plugin processBlock` 第一轮接线
  - 修复键盘按下不释放导致的持续发音问题
  - 修复中文输入法激活时无法发声的问题
  - 修复中文输入法候选词栏被触发的问题
  - 修复设置窗口只能 Save 退出的问题，并补充 X/Esc 自动保存行为
  - 修复设置窗口通过 X 关闭时的生命周期崩溃问题
  - 完成插件 editor 支持，Surge XT editor 可正常打开并操作
  - 使用 Surge XT 成功验证加载与发声
- 未完成：
  - 外部 MIDI 驱动插件发声尚未手工验证
  - 插件状态持久化尚未补充
- 风险：
  - Debug 环境下仍存在与文本/字体渲染相关的 JUCE 断言噪音
  - 在特定插件与系统注入环境下，Debug 退出阶段仍可能出现 `VST3HostContextHeadless` / `AsyncUpdater` leak detector 告警，但当前不影响程序正常退出与核心功能验证
  - 复杂插件（如 Kontakt 类）可能仍需进一步总线/兼容性打磨
- 下一步：
  - 优先考虑 E-1 / E-2：插件状态持久化与 UI 状态同步
  - 或继续推进 `MainComponent` 职责拆分（如键盘区）

## Iteration 2026-03-30
- 目标：在不改变功能行为的前提下，继续小步收敛 `MainComponent` 装配复杂度。
- 完成：
  - 将构造函数中的输入映射恢复逻辑抽离为 `initialiseInputMappingFromSettings()`
  - 将构造函数中的 UI 组件装配与回调绑定抽离为 `initialiseUi()`
  - 将构造函数中的 MIDI 路由初始化抽离为 `initialiseMidiRouting()`
  - 将启动阶段插件恢复路径继续拆分为 `restorePluginScanPathOnStartup()` / `restoreLastPluginOnStartup()`，并收敛扫描路径解析入口 `resolvePluginScanPath()`
  - 将插件相关 settings 写回收口为 `applyPluginRecoverySettings()` / `getPluginRecoverySettingsFromUi()`，减少扫描、启动恢复与手动加载路径中的重复字段拼装
  - 将插件恢复 settings 的读取 + fallback 收口为 `getPluginRecoverySettingsWithFallback()`，统一默认路径回退规则
  - 将插件操作后的状态写回 + UI 收尾收口为 `commitPluginRecoveryStateAndFinishUi(...)`，减少 scan/load 路径重复收尾代码
  - 将 `loadSelectedPlugin()` 轻量拆分为 `getSelectedPluginNameForLoad()` + `loadPluginByNameAndCommitState(...)`，降低单函数分支复杂度
  - 将 `togglePluginEditor()` 轻量拆分为 `tryCreatePluginEditor()` + `openPluginEditorWindow(...)`，降低早退分支密度
  - 将 `unloadCurrentPlugin()` 收口为 `unloadPluginAndCommitState()`，使 unload 与 scan/load 的状态提交路径保持一致
  - 将 `scanPlugins()` 轻量拆分为 `resolvePluginScanPath()` + `scanPluginsAtPathAndCommitState(...)`，降低路径解析与状态提交耦合
  - 将启动恢复扫描逻辑复用到 `scanPluginsAtPathAndApplyRecoveryState(...)`，减少启动与手动扫描路径的重复实现
  - 将插件扫描空路径保护统一到 `isUsablePluginScanPath(...)`，减少启动恢复与手动扫描分散判定
  - 将 `runPluginActionWithAudioDeviceRebuild(void)` 复用到带 `RuntimeAudioConfig` 的主实现，减少设备重建包裹重复代码
  - 将 `restoreLastPluginOnStartup()` 轻量拆分为 `getLastPluginNameForStartupRestore()` + `restorePluginByNameOnStartup(...)`，统一 last plugin name 恢复命名
  - 将部分 `PluginRecoverySettingsView` 结构体字面量收口为 `makePluginRecoverySettings(...)` / `getPersistedPluginSearchPath()` 等 helper，减少字段重复拼装
  - 将 `getPluginRecoverySettingsWithFallback()` 复用到 `makePluginRecoverySettings(...)`，进一步统一 recovery settings 构造入口
  - 将 plugin editor 窗口托管下沉到 `Source/UI/PluginEditorWindow.*`，进一步缩短 `MainComponent` 中的 editor 窗口实现体积
  - 将 plugin editor 标题生成逻辑下沉到 `PluginEditorWindow`，`MainComponent` 不再负责 editor 窗口标题拼接
  - 将 plugin editor close 后的异步收尾收口为 `handlePluginEditorWindowClosedAsync()`，进一步缩短 `openPluginEditorWindow()`
  - 保持插件扫描/加载/发声/editor 与键盘/IME 已有行为不变
  - 执行 `cmake --build --preset ninja-debug` 验证通过
- 未完成：
  - 外部 MIDI 实机发声验证仍待设备条件
  - `MainComponent` 后续仍需继续拆分状态装配与流程控制
- 风险：
  - 当前改动为结构性重排，低风险；但后续继续拆分时需持续防止生命周期时序回归
- 下一步：
  - 继续压缩 `MainComponent` 中插件扫描/恢复与 UI 刷新流程耦合
  - 视情况继续下沉插件 UI 状态同步与 settings 写回边界
  - 在有外部设备后补齐外部 MIDI 驱动插件发声手工验收

## Iteration 2026-03-31 (上午)
- 目标：总结 `MainComponent` 收敛进展，并进行阶段收尾。
- 完成：
  - 将 plugin editor 打开后的只读展示更新统一到 `AppState / PluginPanelStateBuilder` 路径（`refreshReadOnlyUiState()`）。
  - 在文档中确认 `MainComponent` 的多项收敛（包含：状态展示分离、初始化辅助函数提取、流程状态提交辅助函数提取、设置写入收口、插件 editor 托管拆分）。
  - 更新项目任务清单，将 `E-3 拆分 MainComponent 的局部职责（第一步）` 及相关文档节点标记为 `已完成`（作为当前这轮小步重构的阶段收尾）。
- 未完成：
  - （无特定未完成项，本阶段主要为代码和结构清理、职责收敛）。
- 风险：
  - （无新风险）。
- 下一步：
  - 转向更有功能收益的目标，如：完善录制回放模型、进一步重做/完善键盘布局映射模型（解决焦点状态残留等测试问题）或外部 MIDI 发声实测等。

## Iteration 2026-03-31 (晚间)
- 目标：键盘布局切换 UI 闭环，补充布局选择与 Full Piano Layout 预设。
- 完成：
  - 在 `KeyMapTypes.h` 增加 `makeFullPianoLayout()` 提供 C3-C6 扩展键位。
  - 在 `ControlsPanel` 增加 Layout ComboBox 用于选择键盘布局。
  - 将布局切换信号 `handleLayoutChanged` 接回 `MainComponent` 并触发 `appSettings` 保存与 `keyboardMidiMapper` 的布局更新。
- 未完成：
  - Windows 系统环境下的焦点/held keys 残留彻底测试仍待补充专项自动化或侦听器。
- 风险：
  - （无新风险）。
- 下一步：
  - 正式启动录制/回放模型数据结构的定义。

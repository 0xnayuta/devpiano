# VST3 插件宿主与生命周期管理功能说明

> 用途：说明 devpiano 的 VST3 插件扫描（`PluginHost`）、分片扫描会话、XML 缓存恢复、插件实例加载/卸载生命周期、独立 Editor 窗口托管、分层实时架构与高风险路径专项回归清单。
> 验证边界：核心宿主抽象与守护机制通过原生 VST3 测试夹具验证；商业厂商插件与物理声卡热插拔保留为手工回归边界。项目状态以 [roadmap](../../roadmap/roadmap.md) 为准。
> 更新时机：插件扫描算法、缓存持久化协议、Editor 托管逻辑、分层实时契约或生命周期钩子发生变化时。

---

## 1. 概述与设计定位

VST3 插件宿主是 devpiano 连接现代专业音频制作与高品质虚拟乐器音色扩展的核心桥梁：

1. **现代 VST3-First 架构**：基于 JUCE `AudioPluginFormatManager` 与 `AudioPluginInstance` 标准宿主抽象，聚焦 VST3 单一现代插件格式；测试验证基于自建原生 VST3 真实二进制（Phase C Twin / Phase D / Phase E 夹具），不将 mock 格式或测试回声当成商业厂商插件兼容性证明；
2. **消息线程分片扫描**：各 tick 之间更新状态与接受取消；单个插件探测仍是同步调用，永久阻塞时不能保证界面继续响应；
3. **XML 缓存持久化与极速冷启动**：已扫描插件列表以 `KnownPluginList` XML 持久化于设置中，冷启动时秒级恢复缓存，避免每次启动重复重扫；
4. **失败文件细粒度追踪**：清晰记录每个扫描失败的文件路径（`lastScanFailedFiles`），Logger 详细输出失败原因，UI 友好提示 `(see log)`；
5. **生命周期所有权**：实例替换、重扫与退出先关闭 Editor、停止 callback，再释放实例，消除宿主已知的并发卸载路径；不承诺解决第三方插件内部 deadlock；
6. **增量扫描持久化**：已发现 description 同步保存；dead-man's pedal 恢复时把故障项加入黑名单并推迟。正常返回的保存结果有测试依据，进程崩溃、强杀和断电的端到端结果仍单列未验证。

---

## 2. 核心架构与主运行链路

```text
[插件扫描链路]
用户触发扫描 ──► PluginOperationController::scanPlugins()
                     │
                     ▼
                 停设备守卫 ──► PluginHost::beginVst3ScanSession() (消息线程分片推进)
                     ├── 恢复 dead-man's pedal 崩溃记录 ──► 崩溃插件列入黑名单推迟至末尾
                     ├── 遍历 FileSearchPath (支持多目录与规范化过滤)
                     ├── 逐个探测 VST3 ──► 成功项加入 KnownPluginList
                     │                        └──► 增量回调 ──► 立即写入 XML（崩溃安全）
                     └── 失败项记录至 lastScanFailedFiles ──► UI 显示摘要
                     │
[插件加载、替换与发声链路]
用户选择插件 ──► PluginOperationController::loadSelectedPlugin()
                     │
                     ├── 1. 关闭已有 Editor 窗口并解绑 OS 视图句柄
                     ├── 2. AudioPluginFormatManager::createPluginInstance() (消息线程同步创建)
                     ├── 3. PluginHost::prepareToPlay(sampleRate, blockSize)
                     ├── 4. AudioEngine 经 InstrumentEndpoint 将发声切换至插件实例
                     └── 5. 电脑键盘/MIDI 弹奏 ──► 驱动插件合成音频
                     │
[Editor 独立窗口托管]
用户点击 Open Editor ──► PluginOperationController::togglePluginEditor()
                             │
                             └── 创建 PluginEditorWindow (独立顶层窗口托管 plugin->createEditor())
                     │
[重扫/退出与协作收尾链路]
重扫/退出应用 ──► runPluginActionWithAudioDeviceRebuild / MainComponent 析构
                     │
                     ├── 1. closePluginEditorWindow() 销毁 Editor 窗口与底层句柄
                     ├── 2. shutdownAudio() 切断实时音频硬件回调
                     ├── 3. pluginHost.unloadPlugin() 执行 releaseResources() 并释放实例
                     └── 4. 退出应用时协作等待后台工作线程退出，杜绝强杀破坏堆状态
```

---

## 3. 关键机制与鲁棒性设计

### 3.1 多目录扫描与路径持久化

- 路径输入支持 `juce::FileSearchPath` 语法（分号或逗号分隔多个路径）；
- 扫描前自动过滤不存在的非法路径，并将规范化后的有效路径持久化到 `SettingsModel::pluginSearchPath`。

### 3.2 扫描状态机互斥保护

扫描期间，`PluginOperationController` 自动拦截并忽略以下操作，防止状态机错乱：
- 重复点击 Scan 按钮；
- 尝试加载或卸载插件；
- 尝试打开 Editor 窗口。

### 3.3 辅助窗口与键盘焦点防抢夺

打开插件 Editor 窗口后，用户可能需要使用电脑键盘在插件内输入参数或试弹。`MainComponent` 的异步焦点恢复机制（`restoreKeyboardFocus`）在检测到 Editor 窗口处于激活状态时会自动跳过抢焦动作，防止主窗口将辅助窗口顶到后台。

### 3.4 退出与音频设备重建安全序列（`runPluginActionWithAudioDeviceRebuild`）

加载/卸载/替换插件与设备重建均通过 `runPluginActionWithAudioDeviceRebuild`；最终应用析构只关闭 Editor、停 callback 并释放实例，不重新启动设备。守卫操作按以下确定性顺序执行：
1. `prepareForAudioDeviceRebuild()`：
   - 捕获当前音频设备配置快照；
   - `closePluginEditorWindow()` 销毁 UI 窗口与底层 OS 视图句柄；
   - `shutdownAudio()` 切断实时音频硬件回调，杜绝音频线程并发访问；
2. 在保护作用域内安全执行插件生命周期变更（`pluginHost.unloadPlugin()` 执行 `releaseResources()` 释放 `AudioPluginInstance`，或创建新实例并执行 `prepareToPlay`）；
3. `finishAudioDeviceRebuild()`：
   - `initialiseAudioDevice()` 重建并启动音频硬件；
   - `restoreKeyboardFocus()` 恢复键盘焦点；
   - `updateStatusBar()` 刷新 UI 状态栏；
   - 发布最新宿主与 Editor 只读状态，避免音色切换、重扫或导出快照关闭 Editor 后仍显示旧状态。

在已有插件加载且 Editor 打开的状态下，用户直接点击 Scan 重新扫描或加载新插件时，守卫会自动关闭已有 Editor 并安全释放旧实例，然后再启动扫描或新实例构建；程序退出时直接复用该释放序列，杜绝跨线程悬挂指针。
### 3.5 description 身份与重复文件发现

- 下拉菜单每项携带 `PluginDescription::createIdentifierString()` 身份；同名不同文件/ID/类型不折叠，必要时显示标识符作区分。加载使用选中项的身份，不从 ComboBox 文本反查第一个同名插件。
- 乐器/效果过滤直接消费 description 的 `isInstrument`；空过滤结果保持空列表并禁用 Load，不回退到全部类型。
- `SettingsModel::lastPluginIdentifier` 贯穿成功加载、快照保存与启动恢复，仅消费当前 description identifier；不从旧 `lastPluginName` 推导恢复目标，缺少当前标识时不加载历史名称对应的插件。插件缓存继续独立持久化。当前可选字段缺省不是旧版本兼容，不执行任何向后兼容迁移。
- `addVst3FileToKnownList()` 返回此次有效探测的 descriptions，不依赖是否新增列表条目。重复文件可再次加载，缓存 metadata 更新仍触发持久化。

### 3.6 崩溃安全扫描持久化与 dead-man's pedal

- **增量持久化**：`PluginHost::advanceVst3ScanStep()` 每推进一个插件即比对新旧类型数，一旦命中新插件，立即通过 `ScanIncrementalCallback` 交给 `PluginOperationController` 同步写入设置（`knownPluginListState`），而不是等扫描全部结束才落盘。扫描被中断或第三方插件崩溃时，已扫到的插件不会一起丢失；
- **扫描目标先行落盘**：`scanPlugins()` 在首个插件被探测前就持久化扫描路径，崩溃后下次启动仍知道要恢复哪个目录；
- **dead-man's pedal 恢复**：`beginVst3ScanSession()` 读取 dead-man's pedal 文件，将其记录的崩溃插件加入 `KnownPluginList` 黑名单，使其被推迟到扫描序列末尾——单个劣质插件不再让每次扫描都卡在同一位置；
- **取消同样保留成果**：`cancelVst3ScanSession()` 与单文件导入（`addVst3FileToKnownList()`）也触发增量回调，中途取消或导入不会丢弃已发现的插件。

上述行为由 `source/tests/PluginScanPersistenceTest.cpp` 以可注入的 dead-man's pedal 路径做确定性验证；真实进程强杀与崩溃的端到端表现仍需手工回归（见 §4 PLG-009 / PLG-010）。

### 3.7 加载慢插件与取消限制（Slow Plugin Load & Cancellation Limitations）

- **同步创建与无强杀保证**：第三方 VST3 插件的实例化（`formatManager.createPluginInstance()`）与初始化（`prepareToPlay()`）在消息线程上的音频重建守卫中同步执行。若第三方插件在构造函数或初始化中永久卡死，宿主无法在不损坏进程堆状态的情况下强行打断（Win32 `TerminateThread` 会破坏互斥量与 CRT 堆，被严格禁止）；
- **分片扫描取消边界**：`cancelVst3ScanSession()` 在分片之间由消息线程调用，保留已发现成果并触发 XML 持久化回调；这不是插件加载的可中断取消按钮。单个 `scanNextFile` 同步探测永久阻塞时，消息线程无法处理下一次取消调用。

### 3.8 分层实时架构与框架锁边界（Layered Realtime Architecture & Framework Lock Boundaries）

devpiano 的实时发声系统采用严格的分层验收契约：
1. **产品自有实时链路（Layer 1）**：发声与调度回调保持零堆分配、零锁、零库函数三角调用；8 复音单核 CPU $\le 0.7\%$ 为保留的物理 SLA，测量必须注明机型/采样率/块长，不能用本轮文档验证替代性能实测；
2. **第三方 JUCE VST3 适配器框架层（Layer 2）**：当加载第三方 VST3 插件时，音频回调经 `InstrumentEndpoint` 转发至 `AudioPluginInstance::processBlock()`。JUCE 原生适配器在框架层存在以下固有约束（单独记录，不假装已零锁）：
   - `juce_VST3PluginFormatImpl.h` 的 `processBlock()` 获取 `SpinLock processMutex`，与消息线程的 `updateMidiMappings()` 共享同一把锁；
   - `juce_VST3Common.h` 的 MIDI 事件转换使用带 `CriticalSection` 的容器，且单块存在 **2048 条 MIDI 消息上限**（`enum { maxNumEvents = 2048 }`），单块超额消息会被框架直接截断；
3. **第三方插件二进制内部（Layer 3）**：第三方商业插件自身的内存分配、工作线程、锁机制与 CPU 开销完全超出宿主控制。产品自有链路的硬实时承诺不外推至第三方插件内部。

### 3.9 缓存与状态线程所有权（Cache & State Thread Ownership）

- **状态快照所有权**：插件参数状态读取（`getStateInformation()`）与恢复（`setStateInformation()`）属于**消息线程专有操作**，仅在音频回调处于静止状态（音频设备重建守卫内，或离线导出准备阶段在 worker 启动前）执行，音频实时线程严禁调用状态序列化接口；
- **插件清单 XML 持久化**：`KnownPluginList` 的 XML 导入与导出（`createKnownPluginListXml()`、`restoreKnownPluginListFromXml()`）强制断言在消息线程执行（`assertMessageThread()`）。扫描会话推进中的增量写入通过消息线程回调通知设置模型。

---

## 4. 插件生命周期专项回归清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **PLG-001** | 扫描后加载插件 | 扫描本地 VST3 目录 → 选择插件点击 Load → 试弹发声正常，状态栏显示 `Loaded: <Name>` | [x] 已通过 (Phase C 原生 VST3 夹具验证；未验证商业厂商插件) |
| **PLG-002** | 连续重复加载与卸载 | 连续加载/卸载同一插件，无内存泄漏、无界面卡死，状态恢复正常 | [x] 已通过 (原生 VST3 夹具与控制器验证) |
| **PLG-003** | 加载状态下重新扫描 | 在已有插件加载并演奏状态下再次点击 Scan，旧插件安全卸载，扫描平稳完成 | [x] 已通过 (Phase C 原生 VST3 阻塞交错验证) |
| **PLG-004** | 打开并关闭 Editor | 点击 Open Editor 打开插件原生界面，操作旋钮与音色切换正常，关闭窗口无报错 | [x] 已通过 (原生 VST3 与生产 Main 验证) |
| **PLG-005** | 打开 Editor 状态下卸载插件 | 在 Editor 窗口保持打开时点击 Unload 按钮，Editor 窗口自动关闭，插件安全释放 | [x] 已通过 (原生 VST3 验证) |
| **PLG-006** | 打开 Editor 状态下退出程序 | 在 Editor 窗口打开状态下直接点击主窗口右上角关闭按钮，程序平稳退出，无崩溃与报错 | [x] 已通过 (Phase C 原生 VST3 协作退出验证) |
| **PLG-007** | XML 缓存冷启动秒级恢复 | 首次扫描完成后重启应用，下拉菜单立即呈现已缓存插件列表，无需重新扫描 | [x] 已通过 (原生 VST3 与设置恢复验证) |
| **PLG-008** | 损坏/不兼容 VST3 容错 | 扫描包含损坏或 32-bit 的非法 VST3 文件，扫描跳过该文件并不崩溃，Logger 准确记录路径 | [x] 已通过 (损坏文件隔离验证) |
| **PLG-009** | 崩溃后扫描成果保留 | 扫描较大插件目录过程中强制结束进程，重启后设置中仍缓存崩溃前已扫描到的插件，而非全部丢失 | [ ] 待手工验证 (单元测试由 PluginScanPersistenceTest 注入验证，真实进程异常终止待实机手工复核) |
| **PLG-010** | 崩溃插件推迟到扫描末尾 | 劣质插件导致扫描崩溃后重启再扫描，该插件被推迟到序列末尾，其余插件优先完成扫描 | [ ] 待手工验证 (单元测试由 PluginScanPersistenceTest 验证，真实插件崩溃待实机手工复核) |
| **PLG-011** | 同名身份与类型过滤 | 同名不同 description/文件分别选择与加载；重启恢复原身份；乐器过滤不载入同名效果 | [x] 已通过 (Phase C Twin 原生 VST3 与实际菜单验证) |
| **PLG-012** | 已发现文件再次拖入 | 已扫描/缓存插件卸载后再次拖入；仍返回有效身份并更新 metadata，不误报没有类型 | [x] 已通过 (Phase C 原生 VST3 与实际拖放验证) |

### 4.1 验收证据依据与未验证范围说明

1. **原生 VST3 实施依据**：PLG-001～PLG-007、PLG-011 与 PLG-012 的验证证据源自基于真实 JUCE VST3 wrapper 构建的本地独立插件包（`PhaseCTwinA`、`PhaseCTwinB`、`PhaseCTwinEffect`，见 [Phase C 实施记录](../../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03) EVID-020/021/024），包含真实的 Win32 事件阻塞交错、已加载＋Editor＋重扫交错与真实退出测试；测试聚焦行为与状态转换，字段/getter 声明复制不作为生命周期或稳定性保证；
2. **严禁外推的未验证范围**：
   - **第三方商业厂商插件**：未在各类商业插件（如 Pianoteq, Kontakt, Surge XT, Spitfire LABS）上执行兼容性认证，不可将自建原生夹具通过等同于第三方厂商全兼容；
   - **物理声卡热插拔与驱动抖动**：未在真实物理硬件 ASIO/CoreAudio 声卡插拔或驱动崩溃下执行破坏性测试；
   - **操作系统级异常终止**：未模拟断电、内核 OOM 或直接杀进程对 XML 文件写入的极端损坏；
   - **第三方插件永久死锁**：若第三方插件在 `createPluginInstance` 内部死锁，宿主因无强杀机制将处于等待状态。

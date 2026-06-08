# 插件宿主功能说明

> 用途：说明当前 VST3 插件扫描、加载、处理、editor 和生命周期相关功能行为。包含功能说明与专项测试。  
> 状态来源：[`../../roadmap/roadmap.md`](../../roadmap/roadmap.md)、[`../architecture.md`](../architecture.md)。

## 当前能力

当前插件宿主已经具备最小可用能力：

- 可识别并使用 VST3 插件格式。
- 可扫描默认或指定 VST3 目录。
- 插件路径输入支持 `juce::FileSearchPath` 语义的多目录字符串（分号或逗号分隔）；扫描前会过滤不存在的目录并持久化规范化后的路径。
- 可显示扫描到的插件名称列表。
- 可从扫描结果中选择并加载 VST3 乐器。
- 已加载插件可参与 `processBlock` 并发声。
- 电脑键盘输入可驱动已加载插件发声。
- 外部 MIDI 输入到插件的代码通路已接入，但仍待更多真实设备手工验证。
- 插件卸载后程序保持可用。
- 可打开支持 editor 的插件窗口。
- 基础插件恢复信息可持久化，例如最近扫描路径和上次插件名称。
- 已扫描插件列表会以 JUCE `KnownPluginList` XML 缓存到设置；启动时优先恢复缓存，避免每次启动都同步重扫。
- 扫描失败时会保留失败文件路径明细，并将每个失败文件写入 Logger（UI 摘要显示失败数量）。
- 扫描状态会区分尚未扫描、无可用扫描目录、扫描成功但无插件、扫描失败文件、缓存恢复为空和上次插件加载失败。
- 未加载插件时，程序可使用内置 fallback synth 保持最小可发声能力。

## 主要链路

### 插件扫描与加载

```text
Plugin scan path
  -> PluginHost scan
  -> KnownPluginList
  -> selected PluginDescription
  -> AudioPluginInstance
  -> prepareToPlay / processBlock / releaseResources
```

### 演奏到插件发声

```text
Computer keyboard / external MIDI
  -> MidiMessageCollector / MidiKeyboardState
  -> AudioEngine
  -> loaded VST3 plugin
  -> AudioDeviceManager
  -> Audio Output
```

## 当前实现边界

当前插件宿主以 JUCE 插件宿主能力为基础：

- `PluginHost` 负责插件格式管理、VST3 扫描、实例加载、prepare/release 与卸载。
- `AudioEngine` 优先驱动已加载插件实例的 `processBlock`。
- 没有加载插件时，`AudioEngine` 回退到内置 fallback synth。
- 插件 editor 由独立窗口托管。

## 当前已验证行为

已在项目状态中确认的能力包括：

- VST3 扫描、选择、加载路径已经成立。
- 已加载 VST3 插件可参与音频处理并发声。
- 支持打开并操作插件 editor。
- 插件卸载后程序保持可用。
- 已完成一轮插件生命周期基线人工回归，当前已验证：
  - 扫描后加载插件并试弹。
  - 连续加载 / 卸载同一插件 3~5 次。
  - 已加载插件状态下重新扫描。
  - 打开并关闭 editor。
  - 打开 editor 后卸载插件。
  - 打开 editor 后重新扫描。
  - 连续执行 load / unload / scan。
  - 加载插件后直接退出程序。
  - 打开 editor 后直接退出程序。

当前仍待继续补齐或复核的生命周期相关场景：

- 外部 MIDI 打开状态下直接退出程序：因当前缺少外部 MIDI 设备，尚未完成手工验证。

详见：[`./plugin-hosting.md`](./plugin-hosting.md)。

## 当前限制和风险

当前仍需继续完善或观察：

- 扫描失败记录、多目录扫描、扫描结果持久化仍可继续完善。
- 外部 MIDI 输入到插件的真实设备场景仍待更多手工验证。
- 不同 `Audio device type` 下的 `buffer size` / `sample rate` 可调范围存在后端语义差异；当前已确认这不构成插件生命周期缺陷，但仍需在用户文档或设置说明中逐步提高可解释性。
- 特定插件 / Debug 注入环境下退出阶段可能仍有 JUCE / VST3 调试告警，需要持续观察。
- 外部 MIDI 打开状态下退出程序（生命周期测试 6.3）仍待设备条件补齐。
- 后续视需要拆分 `PluginScanner` 与实例生命周期宿主职责。

## 与旧 FreePiano 的关系

旧 FreePiano 中的 `synthesizer_vst.*` / `vst/*` 可用于理解历史功能边界和插件交互预期。

新实现不沿用旧版 VST SDK 风格接口，而是使用 JUCE：

- `AudioPluginFormatManager`
- `KnownPluginList`
- `PluginDirectoryScanner`
- `AudioPluginInstance`

## 后续方向

### 插件扫描产品化 backlog（排期）

以下条目是产品化增强排期，不阻塞当前 Phase 3 MVP：

1. **Phase 2-1：扫描失败文件明细**（已实现）
   - 目标：记录 `PluginDirectoryScanner::getFailedFiles()` 中的具体文件路径，而不是只显示失败数量。
   - UI：`PluginPanel` 先显示摘要，例如 `2 failed`；详细路径先进入 Logger，后续再考虑展开面板或导出日志。
   - 约束：不要在 audio callback 中写日志；扫描仍由现有 scan action 触发。
   - 验证：准备一个损坏或不兼容的 VST3 文件，确认失败文件路径可追踪且程序不崩溃。

2. **Phase 2-2：多目录扫描输入与持久化**（已实现）
   - 目标：明确用户可输入多个 VST3 搜索目录，并按 `juce::FileSearchPath` 语义持久化。
   - UI：继续复用当前路径输入框，使用平台路径分隔符；后续如需要再升级为目录列表编辑器。
   - 约束：保留默认 VST3 搜索路径 fallback；无效目录不应阻塞其他有效目录扫描。
   - 验证：输入两个以上目录，其中一个为空或无效，扫描仍能完成并给出可理解摘要。

3. **Phase 2-3：扫描结果持久化 / 启动恢复优化**（已实现）
   - 目标：评估并实现 `KnownPluginList` 持久化，降低启动恢复对同步重扫的依赖。
   - 策略：先保存 `KnownPluginList::createXml()` 结果或等价 XML；启动时优先加载缓存，必要时再重扫。
   - 约束：缓存失效时必须安全回退到重新扫描；不得加载已经不存在或格式不匹配的插件描述。
   - 验证：重启后无需手动扫描即可看到上次插件列表；删除插件文件后不会崩溃，状态可恢复。

4. **Phase 2-4：扫描空状态 / 失败状态 / 恢复失败提示**（已实现）
   - 目标：让用户区分"尚未扫描""扫描成功但无插件""扫描失败""上次插件恢复失败"。
   - UI：`PluginPanel` 状态文本增加清晰短句；错误细节仍可先写 Logger。
   - 约束：不引入阻塞式弹窗作为主反馈；避免让状态文本过长。
   - 验证：空目录、无权限目录、插件恢复失败、手动取消 / 修改路径等路径都有可理解反馈。

> 2026-04-30：Phase 2-1..2-4 已完成 Windows 人工验证，未发现明显问题。详见 [`./plugin-hosting.md`](./plugin-hosting.md)。

### Phase 2-5：扫描 UX 增强（已实现）

以下条目为插件扫描体验增强，已实现：

1. **扫描进度状态反馈** ✅
   - 实现方式：采用JUCE `AsyncUpdater` 驱动的消息线程分片扫描（chunked scan session）。
   - `PluginHost` 新增 `beginVst3ScanSession()` / `advanceVst3ScanStep()` / `cancelVst3ScanSession()` API，`PluginDirectoryScanner` 由同步阻塞 loop 改为每 tick 推进一个插件。
   - UI 状态栏实时显示 `Scanning: {pluginName}.vst3...`。
   - 限制：单个插件探测本身仍可能短暂阻塞 UI，但多插件扫描时可逐项更新进度。

2. **扫描前清空旧列表，区分扫描中阶段** ✅
   - `PluginHost::beginVst3ScanSession()` 开头执行 `knownPluginList.clear()`，确保重新扫描时列表干净。
   - UI 在 `isCurrentlyScanning=true` 时清空 `pluginSelector` 和 `pluginListEditor`，明确区分"扫描中"状态。

3. **插件列表支持 Enter 键直接加载（onChange 自动加载）** ✅
   - `PluginSelector.onChange` 绑定到 `onLoadRequested`，选中插件自动触发加载，无需额外点击 Load 按钮。

4. **扫描失败文件列表可发现性提升** ✅
   - 已有 `lastScanFailedFiles`，UI 状态栏显示失败数量及 `(see log)` 提示，Logger 中记录每个失败文件路径。

此外，`PluginOperationController` 在扫描期间对以下操作进行 guard 保护，防止状态机混乱：
- Load / Unload / Open Editor / 重复 Scan 均在扫描中直接 return。

### 其他后续方向

1. 在具备外部 MIDI 设备后补齐生命周期测试 6.3 与更多真实设备场景验证。
2. 观察并收敛退出阶段 Debug 告警。
3. 按需拆分扫描职责与实例生命周期职责。

---

# devpiano 插件宿主生命周期与退出稳定性测试清单

> 用途：对插件扫描、加载、卸载、editor、音频设备重建与退出等高风险路径进行手工验证。  
> 读者：修改 `PluginHost`、插件 editor、音频设备重建或退出流程的开发者。  
> 更新时机：插件宿主生命周期或相关 UI / 音频流程变化时。

状态标记：
- [x] 已通过
- [ ] 未通过 / 未开始验证
- [~] 部分通过 / 待补充验证

说明：本文件记录专项生命周期回归测试，不等同于"插件功能是否已经可用"。即使 [`../../roadmap/roadmap.md`](../../roadmap/roadmap.md) 中插件发声已通过，本文件中的组合压力场景仍可能未完整执行。

相关文档：
- 阶段验收：[`acceptance.md`](acceptance.md)
- 项目路线图：[`../../roadmap/roadmap.md`](../../roadmap/roadmap.md)

---

## 1. 测试目标

本测试文档主要验证以下内容：

- 插件扫描、加载、卸载过程中的生命周期顺序是否稳定
- 插件 editor 窗口与插件实例之间是否存在悬空引用风险
- 音频设备重建前后，插件宿主是否能正确释放并恢复资源
- 程序退出时，MIDI 输入、音频设备、插件实例、editor 窗口是否按预期关闭
- 在 Debug 环境下，是否仍出现明显的崩溃、卡死或高概率资源告警

---

## 2. 测试前提

### 环境前提
- [x] 项目可成功构建
- [x] WSL 构建基线可用：`./scripts/dev.sh wsl-build`
- [x] Windows MSVC 验证基线可用：`./scripts/dev.sh win-build`
- [x] 程序可启动
- [x] 音频设备可初始化
- [x] 至少有一个可加载的 VST3 插件

### 建议测试插件
优先准备两类插件：

- 轻量插件：如 `Surge XT`
- 较复杂或启动较慢的插件：用于观察 editor、prepare/release、退出阶段行为

### 代码级 / UI 观察基线

> 说明：当前插件生命周期回归仍以手工测试为主，但建议先确认以下代码与 UI 观察点，避免把"状态展示不清晰"误判成真实生命周期故障。

建议先复核以下实现边界：

- `source/Plugin/PluginHost.*`
  - 承载 `scanVst3Plugins()`、`loadPluginByName()`、`prepareToPlay()`、`releaseResources()`、`unloadPlugin()`。
- `source/MainComponent.*`
  - 承载 scan / load / unload / editor / shutdown 的主协调逻辑。
  - 当前 `prepareForAudioDeviceRebuild()` 会先执行 `closePluginEditorWindow()` 与 `shutdownAudio()`。
  - 当前 `runPluginActionWithAudioDeviceRebuild()` 包裹 scan / load / unload 路径。
  - 当前析构顺序为：关闭 editor -> 关闭音频/MIDI -> `pluginHost.unloadPlugin()`。
- `source/Audio/AudioEngine.cpp`
  - 音频初始化时会对已加载插件执行 `prepareToPlay()`；释放时会走 `releaseResources()`。
- `source/UI/PluginPanel.cpp`
  - 生命周期手工回归时，优先观察顶部状态标签是否正确显示以下关键信息：
    - `Loaded: <plugin>`
    - `@ <sampleRate> Hz / <blockSize>`
    - `Editor open`
    - `Load error: ...`
    - 最新 scan summary

当前手工基线建议区分两类结论：

- 代码级 / 静态基线：确认 scan / load / unload / editor / shutdown 路径的主协调顺序仍存在。
- Windows 手工回归：实际运行程序，观察 UI 状态、发声、editor 行为、重扫、退出与 Debug 告警。

---

## 3. 基础插件生命周期测试

## 3.1 扫描后加载插件

### 目标
验证扫描完成后可正常加载插件，并进入可演奏状态。

### 测试步骤
- [x] 启动程序
- [x] 扫描 VST3 路径
- [x] 从列表中选择一个插件
- [x] 点击 `Load`
- [x] 使用 `A/S/D/F` 试弹

### 预期结果
- [x] 插件可成功加载
- [x] UI 显示 Loaded / prepared 等状态正常
- [x] 可正常发声
- [x] 无明显卡顿或崩溃

### 状态
- [x] 已验证

---

## 3.2 连续加载同一插件

### 目标
验证重复加载路径不会留下脏状态。

### 测试步骤
- [x] 加载一个插件
- [x] 卸载该插件
- [x] 再次加载同一插件
- [x] 重复 3~5 次

### 预期结果
- [x] 每次都能成功加载/卸载
- [x] 无崩溃、无明显状态错乱
- [x] editor 状态不会异常残留

### 状态
- [x] 已验证

---

## 3.3 在已加载插件状态下重新扫描

### 目标
验证"扫描会先释放当前插件"这一高风险路径是否稳定。

### 测试步骤
- [x] 加载一个插件并确认可发声
- [x] 不关闭程序，直接再次执行插件扫描
- [x] 扫描完成后观察 UI 状态
- [x] 重新加载一个插件试弹

### 预期结果
- [x] 扫描过程无崩溃
- [x] 扫描前已加载插件被安全卸载
- [x] 扫描后 UI 状态一致
- [x] 重新加载插件后仍可正常发声

### 状态
- [x] 已验证

---

## 4. 插件 editor 生命周期测试

## 4.1 打开并关闭 editor

### 目标
验证 editor 可正常创建和销毁。

### 测试步骤
- [x] 加载支持 editor 的插件
- [x] 点击 `Open Editor`
- [x] 观察 editor 窗口是否正常显示
- [x] 点击 editor 窗口关闭按钮

### 预期结果
- [x] editor 窗口可正常打开
- [x] 关闭后程序无崩溃
- [x] UI 状态能正确反映 editor 已关闭

### 状态
- [x] 已验证

---

## 4.2 打开 editor 后卸载插件

### 目标
验证 editor 打开时卸载插件不会留下悬空窗口或失效引用。

### 测试步骤
- [x] 加载支持 editor 的插件
- [x] 打开 editor
- [x] 不手动关闭 editor，直接点击 `Unload`

### 预期结果
- [x] editor 窗口被安全关闭
- [x] 插件被安全卸载
- [x] 程序无崩溃、无明显卡死

### 状态
- [x] 已验证

---

## 4.3 打开 editor 后重新扫描插件

### 目标
验证 editor + scan 的组合路径是否稳定。

### 测试步骤
- [x] 加载支持 editor 的插件
- [x] 打开 editor
- [x] 不关闭 editor，直接执行插件扫描

### 预期结果
- [x] editor 会先被关闭或安全销毁
- [x] 扫描过程无崩溃
- [x] 扫描后程序仍可继续使用

### 状态
- [x] 已验证

---

## 5. 音频设备重建相关测试

## 5.1 加载插件后切换音频设备设置

### 目标
验证音频设备变化后插件 prepare/release 路径稳定。

### 测试步骤
- [x] 加载一个插件
- [x] 打开设置窗口
- [x] 修改音频设备、buffer size 或 sample rate（若环境允许）
- [x] 保存并关闭设置
- [x] 再次试弹

### 预期结果
- [x] 程序无崩溃
- [x] 设置关闭后音频恢复正常
- [x] 插件仍可继续发声或给出清晰失败状态
- [x] 已确认不同 `Audio device type` 下的 `buffer size` / `sample rate` 可调范围受当前 Windows / JUCE 音频后端模式影响；`Windows Audio` 与 `Windows Audio (Low Latency Mode)` 下出现 `480 samples (10 ms)` 固定值属于当前后端语义范围内的正常现象

### 状态
- [x] 已验证

---

## 5.2 连续执行 load / unload / scan

### 目标
验证频繁设备重建与插件切换不会迅速积累异常状态。

### 测试步骤
- [x] 扫描插件
- [x] 加载插件 A
- [x] 卸载插件 A
- [x] 重新扫描
- [x] 加载插件 B
- [x] 打开/关闭 editor
- [x] 再卸载插件
- [x] 重复 2~3 轮

### 预期结果
- [x] 程序持续稳定
- [x] 不出现明显资源未释放导致的异常
- [x] UI 状态与实际插件状态一致

### 状态
- [x] 已验证

---

## 5.3 音频重建后立即试弹首音

### 目标
验证启动、插件加载 / 卸载等音频重建路径完成后，首个音符不会进入尚未稳定的 audio path 并产生音高异常。

### 测试步骤
- [x] 启动程序后立即点击虚拟键盘试弹
- [x] 启动程序后立即按实体键盘试弹
- [x] 加载 VST3 插件后立即试弹
- [x] 卸载 VST3 插件回到内置 synth 后立即试弹
- [x] 分别在 Windows Audio 与 DirectSound 下重复关键路径

### 预期结果
- [x] warmup 窗口内可短暂静音，但不会发出异常音高
- [x] warmup 结束后首音音高稳定且正确
- [x] 快速连续按多个音时不压缩、不乱触发、不丢 note-off
- [x] 当前正式保留 `25ms` audio warmup

### 状态
- [x] 已验证

---

## 6. 程序退出稳定性测试

## 6.1 加载插件后直接退出程序

### 目标
验证最常见退出路径是否稳定。

### 测试步骤
- [x] 启动程序
- [x] 扫描并加载一个插件
- [x] 直接关闭主窗口退出程序

### 预期结果
- [x] 程序正常退出
- [x] 无明显崩溃
- [~] 本轮未观察到明显异常；如后续在特定插件 / Debug 环境仍出现告警，应继续记录插件名称与复现条件

### 状态
- [x] 已验证

---

## 6.2 打开 editor 后直接退出程序

### 目标
验证带 editor 的退出路径是否稳定。

### 测试步骤
- [x] 加载支持 editor 的插件
- [x] 打开 editor
- [x] 不手动关闭 editor，直接关闭主窗口退出程序

### 预期结果
- [x] 程序正常退出
- [x] editor 与插件实例被安全销毁
- [~] 本轮未观察到明显异常；如后续出现 Debug 告警，需记录是否可稳定复现

### 状态
- [x] 已验证

---

## 6.3 外部 MIDI 打开状态下退出程序

### 目标
验证 MIDI 输入、音频设备与插件在退出阶段的关闭顺序是否稳定。

### 测试步骤
- [ ] 保持外部 MIDI 输入已打开
- [ ] 加载一个插件
- [ ] 直接关闭程序

### 预期结果
- [ ] 程序正常退出
- [ ] 无明显回调落到已析构对象上的异常
- [ ] 无高概率卡死

### 状态
- [ ] 因缺少外部 MIDI 设备暂未验证

---

## 7. 建议优先测试包

### 优先测试包 A：最关键生命周期路径
- [x] 3.1 扫描后加载插件
- [x] 3.3 在已加载插件状态下重新扫描
- [x] 4.2 打开 editor 后卸载插件
- [x] 6.1 加载插件后直接退出程序

### 优先测试包 B：高风险 editor / 退出组合
- [x] 4.3 打开 editor 后重新扫描插件
- [x] 6.2 打开 editor 后直接退出程序
- [ ] 6.3 外部 MIDI 打开状态下退出程序（缺少设备，暂未验证）

### 优先测试包 C：长期回归
- [x] 3.2 连续加载同一插件
- [x] 5.1 加载插件后切换音频设备设置
- [x] 5.2 连续执行 load / unload / scan
- [x] 5.3 音频重建后立即试弹首音

---

## 8. 每轮测试记录模板

建议每次对生命周期相关代码做改动后，在本文件底部追加一次记录：

```md
## Lifecycle Test Run YYYY-MM-DD
- 变更内容：
- 测试插件：
- 执行用例：
- 通过项：
- 失败项：
- Debug 告警：
- 复现条件：
- 后续修复建议：
```

---

## Lifecycle Test Run 2026-04-25
- 变更内容：
  - 按 [`../../roadmap/current-iteration.md`](../../roadmap/current-iteration.md) 开始推进"插件生命周期基线回归"。
  - 先补齐代码级 / UI 观察基线，明确本轮优先关注 scan / load / unload / editor / exit 组合路径。
- 测试插件：
  - 尚未执行 Windows 侧实际插件回归；建议优先使用轻量、支持 editor 的 VST3（如 `Surge XT`）。
- 执行用例：
  - 代码级复核 `source/Plugin/PluginHost.*` 的 scan / load / prepare / release / unload 路径。
  - 代码级复核 `source/MainComponent.*` 的 `runPluginActionWithAudioDeviceRebuild()`、`prepareForAudioDeviceRebuild()`、editor 关闭与析构顺序。
  - 代码级复核 `source/Audio/AudioEngine.cpp` 的插件 prepare/release 接入。
  - 代码级复核 `source/UI/PluginPanel.cpp` 的状态标签输出项，确认手工回归时有明确 UI 观察点。
- 通过项：
  - 当前代码中 scan / load / unload 会经过统一的 audio rebuild 包装路径。
  - 当前代码中 scan / load / unload 前会先关闭 plugin editor 窗口。
  - 当前析构路径中已包含 editor 关闭、MIDI 输入关闭、音频关闭和 `pluginHost.unloadPlugin()`。
  - 当前 UI 已暴露 Loaded / prepared / editor-open / last scan summary / load error 等手工观察信号。
- 失败项：
  - 尚未执行 7 节优先测试包 A 的 Windows 手工回归，暂无实际通过/失败结论。
- Debug 告警：
  - 本次仅完成代码级基线整理，未采集新的运行时 Debug 告警。
- 复现条件：
  - 待后续在 Windows/MSVC 验证环境下执行 3.1、3.3、4.2、6.1 后补充。
- 后续修复建议：
  - 下一步优先执行优先测试包 A，并把插件名称、是否支持 editor、是否可稳定复现告警一并记录到本文件。

---

## Lifecycle Test Run 2026-04-26
- 变更内容：
  - 根据 Windows 侧人工验证结果，回填插件生命周期基线回归结论。
  - 本轮重点覆盖基础插件生命周期、editor 生命周期、连续 load / unload / scan，以及直接退出路径。
- 测试插件：
  - 已使用支持 editor 的 VST3 插件完成手工回归；具体插件名称未单独记录。
- 执行用例：
  - 3.1 扫描后加载插件。
  - 3.2 连续加载 / 卸载同一插件 3~5 次。
  - 3.3 已加载插件状态下重新扫描。
  - 4.1 打开并关闭 editor。
  - 4.2 打开 editor 后卸载插件。
  - 4.3 打开 editor 后重新扫描插件。
  - 5.1 加载插件后切换音频设备设置。
  - 5.2 连续执行 load / unload / scan。
  - 6.1 加载插件后直接退出程序。
  - 6.2 打开 editor 后直接退出程序。
- 通过项：
  - 3 节基础插件生命周期测试均未发现明显问题。
  - 4 节 plugin editor 生命周期测试均未发现明显问题。
  - 5.1 音频设备设置切换路径可稳定保存、关闭并恢复发声，当前观察到的 `buffer size` 差异已确认与所选 Windows 音频后端模式有关，不构成产品缺陷。
  - 5.2 连续执行 load / unload / scan 未发现明显问题。
  - 6.1 / 6.2 退出路径未发现明显问题。
- 失败项：
  - 无稳定复现的产品缺陷。
- Debug 告警：
  - 本轮未记录到必须单独追踪的稳定 Debug 告警。
- 复现条件：
  - 已复核 `Windows Audio`、`Windows Audio (Exclusive Mode)`、`Windows Audio (Low Latency Mode)`、`Direct Sound` 的后端差异；可调范围与当前后端模式一致。
  - 6.3 因缺少外部 MIDI 设备，当前无法补齐退出阶段验证。
- 后续修复建议：
  - 在具备外部 MIDI 设备后补齐 6.3 手工回归。

---

## Lifecycle Test Run 2026-04-30
- 变更内容：
  - 完成插件扫描产品化增强 Phase 2-1..2-4：失败文件明细、多目录扫描输入与持久化、`KnownPluginList` 缓存恢复、扫描空/失败/恢复失败状态提示。
- 测试插件：
  - 使用当前 Windows 验证环境中的 VST3 插件与测试目录完成手工回归；具体插件名称未单独记录。
- 执行用例：
  - Phase 2-1：含失败文件或不兼容插件路径的扫描结果摘要与 Logger 记录。
  - Phase 2-2：多目录路径输入、无效目录过滤、规范化路径回写与持久化。
  - Phase 2-3：扫描后关闭并重启，插件列表从缓存恢复，并可继续加载上次插件。
  - Phase 2-4：空路径、无可用目录、空目录、失败文件、缓存恢复、上次插件加载失败等状态文本。
- 通过项：
  - Phase 2-1..2-4 人工验证均未发现明显问题。
  - 插件列表缓存恢复后可直接显示已扫描插件列表。
  - 多目录路径与无效目录过滤行为符合预期。
  - 扫描空状态 / 失败状态 / 恢复失败提示可理解。
- 失败项：
  - 无稳定复现的产品缺陷。
- Debug 告警：
  - 本轮未记录到必须单独追踪的稳定 Debug 告警。
- 复现条件：
  - 外部 MIDI 打开状态下退出程序（6.3）仍因缺少外部 MIDI 设备暂未验证。
- 后续修复建议：
  - 插件扫描产品化 backlog Phase 2-1..2-4 当前可视为完成；后续若需要更强 UI，可再评估失败明细展开面板或扫描日志导出。

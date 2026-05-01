# 已知问题与待验证风险

> 状态：轻量风险清单；详细项目状态以路线图和当前迭代文档为准。
> 更新时机：发现新问题、完成验证、搁置项恢复时。

当前项目状态与风险以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准；阶段验收见 [`acceptance.md`](acceptance.md)。

---

## 1. 外部 MIDI 硬件依赖（搁置）

> 触发条件：当前无真实外部 MIDI 设备，部分验证无法执行。
> 恢复时机：硬件条件恢复后优先补齐。

- **外部 MIDI 录制**：外部 MIDI 设备输入到同一 take 的通路代码已接入，但录制效果待真实设备验证。
- **外部 MIDI 回放**：回放事件重新进入插件路径已实现，外部 MIDI 设备驱动插件发声待验证。
- **插件生命周期测试 6.3**（外部 MIDI 打开状态下退出程序）：因缺少外部 MIDI 设备，尚未完成手工验证。
- **替代方案**：可先配置虚拟 MIDI loopback 软件（如 loopMIDI）做基础通路验证，不依赖真实硬件。

详见：

- [`recording-playback.md`](phase3-recording-playback.md) §7（外部 MIDI 输入）
- [`plugin-host-lifecycle.md`](phase2-plugin-host-lifecycle.md) §6.3
- [`keyboard-mapping.md`](phase2-keyboard-mapping.md) §9.2（混合输入）

---

## 2. 启动 / 音频重建早期首音音高异常（已修复，保留回归项）

> 触发条件：程序启动后尽快弹奏，或手动加载 / 卸载插件导致音频设备重建后立即弹奏。  
> 影响范围：无外部 MIDI 设备时也可复现；鼠标点击虚拟键盘、实体电脑键盘、内置 fallback synth、VST3 插件、Windows Audio / DirectSound 均受影响。  
> 当前观察环境：live audio device 为 `48000 Hz / 480 samples`。  
> 修复状态：已通过多轮人工验证；正式保留 `25ms` audio warmup。

### 现象

- 打开 devpiano 后，立即按下的第一个音，或启动后数秒内按下的多个音，音调会改变并伴随异常音色。
- 等待数秒后再演奏，音高恢复稳定且正确。
- 手动加载 / 卸载插件后，类似异常会再次出现。

### 根因判断与修复结论

外部 MIDI pitch wheel / controller 脏状态已基本排除：当前无外部 MIDI 设备，且内置 synth 与 VST3 插件均可复现。

本 bug 的共同触发点不是具体输入来源或具体发声引擎，而是**音频设备启动 / 重建后的最早几个 audio blocks**：

- 虚拟键盘和实体电脑键盘最终都会向同一个 `MidiKeyboardState` 写入 note on / off。
- `AudioEngine::getNextAudioBlock()` 会在每个 audio block 开头把 `MidiKeyboardState` 中的 pending note 注入到当前 `MidiBuffer`，然后立刻交给 VST3 插件或内置 synth 渲染。
- 程序启动、自动恢复插件、手动 load / unload 插件都会触发音频设备或音频处理链路重新 prepare。
- 如果用户在 prepare 后极早期弹奏，首批 note 会进入尚未完全稳定的音频输出路径；此时可能叠加驱动 / 后端缓冲、JUCE `MidiMessageCollector` 时间基线、`MidiKeyboardState` wall-clock 事件缩放、插件 / synth 首批处理状态等 transient，表现为音高异常或怪音。

因此，同一个问题会同时出现在虚拟键盘、实体键盘、VST3 插件和内置 synth 上；这也是最终判断其根因在 audio lifecycle 早期窗口，而不是键盘映射、外部 MIDI 或某个特定插件。

调查过程中曾修正 `MainComponent::initialiseAudioDevice()` 双阶段初始化：

1. 先调用 `setAudioChannels(0, 2)`，JUCE 会初始化音频设备、注册 audio callback 并触发 `prepareToPlay()`。
2. 如果存在保存的 audio device XML，又直接调用 `deviceManager.initialise(0, 2, xml, true)`，导致设备在 callback 已挂载后再次初始化。
3. 启动 / 重建早期可能经历临时 sample rate / buffer，例如默认 `44100 / 512`，随后才进入最终 live 配置 `48000 / 480`。
4. 内置 `SimpleSineVoice::startNote()` 使用 voice 的 sample rate 计算 oscillator increment；VST3 插件也依赖 prepare sample rate。若首音跨越临时 / 最终 sample rate，听感会表现为音高偏移或怪音。

但人工测试显示，单独移除双初始化后异常更明显，说明双初始化不是主因，而是曾偶然提供额外预热时间。

最终有效修复为：在 `AudioEngine::prepareToPlay()` 后启动短暂 warmup；warmup 期间输出静音并清理 pending keyboard / MIDI state。500ms、250ms、100ms 和 25ms 均经人工验证稳定；正式值保留为 `25ms`，约等于 `48000 / 480` 环境下 2-3 个 audio blocks，用户体感不可察觉且保留必要启动余量。

### 修复原理

`25ms` warmup 的作用不是改变音高计算，也不是延迟整个程序启动，而是在每次 `AudioEngine::prepareToPlay()` 后建立一个很短的“不可听稳定窗口”：

1. **让音频设备 / 回调先跑过几个空 block**  
   在 `48000 Hz / 480 samples` 下，一个 block 约 `10ms`，`25ms` 约覆盖 2-3 个 blocks。这样可以避开设备刚启动、后端缓冲刚填充、插件 / synth 刚 prepare 后的 transient。

2. **丢弃 warmup 窗口内的 pending 输入事件**  
   warmup 期间持续清理 `MidiKeyboardState`、`MidiMessageCollector`、实时 / playback MIDI buffer，避免用户极早期按下的 note 被压缩到首个可听 block，或携带音频重建前后的旧时间基线进入渲染。

3. **保持输出静音并清理发声状态**  
   warmup 期间 `getNextAudioBlock()` 在清空输出后直接返回，并执行 `synth.allNotesOff()`。因此异常 transient 不会进入声卡输出，也不会留下 hanging note。

4. **覆盖所有相关生命周期入口**  
   由于启动、插件自动恢复、手动加载 / 卸载、音频设备重建最终都会重新触发 `AudioEngine::prepareToPlay()`，warmup 放在 `AudioEngine` 层可以同时覆盖 VST3 和内置 synth 两条发声路径。

`25ms` 被选为正式值的原因：它明显大于单个 10ms block，能提供最小必要余量；同时远低于用户可明显感知的演奏延迟窗口。人工验证显示 500ms、250ms、100ms、25ms 均稳定，因此不再继续压到 10ms，避免只剩 1 个 block 的容错导致不同后端或负载下偶发复现。

### 已实施修复

1. **修正音频设备双初始化**
   - 文件：`source/MainComponent.cpp`
   - 函数：`MainComponent::initialiseAudioDevice()`
   - 做法：将保存的 XML 直接传给 `setAudioChannels(0, 2, state)`，删除后续手动 `deviceManager.initialise(...)`。

   ```cpp
   void MainComponent::initialiseAudioDevice()
   {
       const auto audioSettings = appSettings.getAudioSettingsView();
       const auto* state = (audioSettings.hasSerializedDeviceState && appSettings.audioDeviceState != nullptr)
                           ? appSettings.audioDeviceState.get()
                           : nullptr;

       setAudioChannels(0, 2, state);

       captureAudioDeviceState();
       logCurrentAudioDeviceDiagnostics("initialiseAudioDevice");
   }
   ```

2. **增加 25ms audio warmup**
   - 文件：`source/Audio/AudioEngine.h/.cpp`
   - 函数：`AudioEngine::prepareToPlay()` / `AudioEngine::getNextAudioBlock()`
   - 做法：`prepareToPlay()` 后设置 `warmupBlocksRemaining`；`getNextAudioBlock()` 在 warmup 期间保持输出静音，清理 `MidiKeyboardState`、`MidiMessageCollector`、实时 / playback MIDI buffer，并执行 `synth.allNotesOff()`。
   - 正式参数：`warmupSeconds = 0.025`。

### 已通过人工回归

- 无 VST，启动后立即点击虚拟键盘：首音稳定。
- 无 VST，启动后立即按实体键盘：首音稳定。
- 自动恢复 VST 后立即弹奏：首音稳定。
- 手动加载 VST 后立即弹奏：首音稳定。
- 手动卸载回内置 synth 后立即弹奏：首音稳定。
- Windows Audio 与 DirectSound 下均不再出现启动早期音高异常。
- 快速连续按多个音：不压缩、不乱触发、不丢 note-off。

---

## 3. VST3 插件离线渲染（Phase 3-5，后置）

> 触发条件：用户期望用已加载的 VST3 插件音色导出 WAV。
> 影响范围：当前 WAV 导出仅支持 fallback synth。

- **现状**：WAV 离线渲染 MVP（Phase 3-4）已完成，fallback synth 导出和 UI 接入均已验证通过。
- **限制**：WAV 导出不支持已加载 VST3 插件音色；VST3 插件离线渲染（Phase 3-5）暂不阻塞 MVP，已排入后续 backlog。
- **影响**：需要插件音色的用户暂时无法使用 WAV 导出功能。

详见：

- [`../features/phase3-recording-playback.md`](../features/phase3-recording-playback.md)（Phase 3-5 设计备注）
- [`../roadmap/roadmap.md`](../roadmap/roadmap.md)（Phase 3 当前状态）

---

## 4. 录制 / 回放下一阶段重点

> 触发条件：Phase 3 MVP 主链路已接入，但实时音频线程边界、回放结束通知、采样率缩放和 Stop 清理悬挂音路径仍是下一阶段稳定化重点。

- **回放结束通知**：回放结束后 UI 状态转换和焦点恢复路径待进一步验证。
- **采样率缩放**：不同采样率下录制事件的回放时间对齐待验证。
- **Stop 清理悬挂音**：Stop 时悬挂音（sustained notes）的清理路径已实现但需更多边界条件验证。
- **离线渲染采样率**：当前 WAV 导出固定 44100Hz，后续可考虑提供选项。

详见：

- [`recording-playback.md`](phase3-recording-playback.md)（包 A–E 测试结果）
- [`../roadmap/roadmap.md`](../roadmap/roadmap.md)（Phase 3 当前状态与近期重点）

---

## 5. 插件生命周期退出告警（低优先级持续观察）

> 触发条件：特定插件或 Debug 注入环境下退出阶段可能出现 JUCE / VST3 调试告警。

- **现状**：scan / load / unload / editor / 重扫 / 直接退出等主要生命周期组合路径已完成一轮人工回归，未发现功能性问题。
- **观察项**：特定插件 / Debug 注入环境下退出阶段可能仍有 JUCE / VST3 调试告警；低优先级持续观察，暂不安排专项修复。
- **缓解**：主要路径已稳定；异常场景更多与插件自身实现质量相关，而非宿主代码问题。

详见：

- [`plugin-host-lifecycle.md`](phase2-plugin-host-lifecycle.md)（专项生命周期测试）
- [`../roadmap/roadmap.md`](../roadmap/roadmap.md)（Phase 2 当前状态）

---

## 6. 布局 Preset ID 冲突（低优先级）

> 触发条件：用户导入同名 preset 文件时。

- **现状**：用户 preset id 由文件名派生；导入同名文件时覆盖行为已实现，暂无非覆盖时的冲突提醒 UI。
- **影响**：正常使用场景不受影响；重命名等功能正常。
- **后续**：可按需补充冲突提示 UI，低优先级。

详见：

- [`../features/phase3-layout-presets.md`](../features/phase3-layout-presets.md)
- [`../testing/phase3-layout-presets.md`](../testing/phase3-layout-presets.md)

---

## 7. 构建与环境

> WSL / Windows 镜像构建环境问题见 [`../development/troubleshooting.md`](../development/troubleshooting.md)。

---

## 8. 已完成验证项（不作为风险）

以下条目已通过 2026-04-30 人工验证，无已知明显问题：

- **Phase 2-1**：扫描失败文件记录（`lastScanFailedFiles` + Logger 写入 + UI 摘要提示）。
- **Phase 2-2**：多目录扫描输入格式与持久化（`FileSearchPath` 分隔 + 过滤无效目录）。
- **Phase 2-3**：扫描结果持久化（`KnownPluginList` XML 缓存 + 启动优先恢复）。
- **Phase 2-4**：空 / 失败 / 恢复失败状态 UI（区分 6 种状态文本）。
- **Phase 5-1**：RecordingFlowSupport 录制 / 回放 UI 流程。
- **Phase 5-2**：ExportFlowSupport 导出选项与默认文件名。
- **Phase 5-3**：PluginFlowSupport scan / restore / cache 收敛。
- **Phase 5-4**：MainComponent 状态刷新边界命名清理。
- **Phase 5**：Phase 5-1..5-4 全部完成，人工回归通过。
- **启动 / 音频重建早期首音音高异常**：已通过音频设备初始化顺序修正 + `25ms` audio warmup 修复，并完成启动、虚拟键盘、实体键盘、VST load/unload、Windows Audio / DirectSound 人工回归。

详见：

- [`../roadmap/current-iteration.md`](../roadmap/current-iteration.md)（Phase 5-1..5-4 人工回归记录）
- [`phase2-plugin-host-lifecycle.md`](phase2-plugin-host-lifecycle.md)（Phase 2-1..2-4 测试结果）

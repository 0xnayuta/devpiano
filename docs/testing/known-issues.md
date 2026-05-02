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

## 8. MIDI 导入播放首音无声（已修复，保留回归项）

> 触发条件：导入的 MIDI 文件首个音符起始时间接近 0 秒（如 0 秒）时。
> 影响范围：MIDI 文件导入后播放，首个音符几乎无声，但虚拟键盘可视化显示按键已按下。
> 当前状态：已按 playback-start pre-roll / arming 方向修复，并通过 Windows 侧人工回归；暂时收尾，仅保留回归项。

### 现象

- 导入一个 MIDI 文件（单轨），如果该文件的首个音符在非常接近 0 秒的位置开始（例如 0 秒瞬间），播放时该音符几乎听不到声音。
- 虚拟键盘可视化正常显示该音符对应的按键被按下（有按下效果）。
- 后续音符播放正常，问题仅影响首个音符。

### 复现步骤

1. 准备一个单轨 MIDI 文件，确保首个音符起始时间为 0 秒或非常接近 0 秒。
2. 启动 DevPiano。
3. 点击 Import MIDI 按钮，选择该 MIDI 文件。
4. 观察播放行为：首个音符几乎无声，但虚拟键盘显示按键已按下。

### 预期行为

- 首个音符应正常发声，与后续音符一致。

### 实际行为

- 首个音符几乎无声，但虚拟键盘可视化正常。

### Bug 原因

根因判断：问题主要暴露在 **MIDI import playback 启动边界**，不是导入器丢失首个 note。

证据链：

1. `MidiFileImporter` 保留 MIDI 文件原始时间线，将首个 note-on 的时间转换为 `RecordingTake` 事件；首个音在 0 秒时会得到 `timestampSamples == 0`。
2. `RecordingSessionController::startInternalPlayback()` 在开始播放前会请求 `all-notes-off`，并通过 `runPluginActionWithAudioDeviceRebuild()` 包裹 `recordingEngine.startPlayback(...)`。
3. `runPluginActionWithAudioDeviceRebuild()` 会先 `shutdownAudio()`，执行播放启动动作，再重新 `initialiseAudioDevice()`，从而触发 `AudioEngine::prepareToPlay()`。
4. `AudioEngine::prepareToPlay()` 会设置约 `25ms` 的 audio warmup；warmup block 期间 `getNextAudioBlock()` 直接静音返回，不调用 `renderPlaybackEventsIfNeeded()`，因此 playback position 不前进。
5. warmup 结束后的第一个可渲染 block 仍从 playback sample `0` 开始；`RecordingEngine::renderPlaybackBlock()` 会把 `timestampSamples == 0` 的首个 note-on 放在该 block 的 sample offset `0`。
6. 虚拟键盘可视化能显示按键被按下，说明首个 note-on 已被 playback scheduler 取出，并已进入 `MidiKeyboardState`；故障点更可能位于之后的 audio MIDI buffer / synth / plugin 渲染路径。

因此，原有 `25ms` warmup 解决的是音频设备早期 transient 外泄问题，但对导入播放来说，它没有让 synth/plugin 在真正接收首个 playback note 前完成一段“空 MIDI 渲染预热”；首个 0 秒 note 仍会成为 rebuild 后第一个进入渲染链路的播放事件。

附加风险：`startInternalPlayback()` 提前调用的 `requestAllNotesOff()` 可能在 warmup 后第一个可听 block 中与首个 playback note-on 同时落在 sample offset `0`。不同插件或渲染路径对同 sample 的 all-notes-off / note-on 排序处理可能不同，可能进一步放大首音被吞或几乎无声的问题。

导入器本身丢弃首个 note 的可能性较低，因为 UI 可视化已证明首个 note-on 进入了 playback visual path。不过，“接近 0 秒”的事件经采样转换后可能被截断到 sample `0`，这是合法时间戳，但会触发上述启动边界。

### 修复原理

修复方向：在 audio/session playback 启动层增加安全 pre-roll / arming 机制，而不是修改导入后的 MIDI 事件时间。

1. **保留导入时间线语义**
   - 不在 `MidiFileImporter` 中强行给所有事件加固定偏移。
   - `timestampSamples == 0` 应保持合法，避免破坏 MIDI 文件的音乐时间线。

2. **在播放启动层增加 playback-start safety margin**
   - `RecordingSessionController::startInternalPlayback()` 在 `recordingEngine.startPlayback(...)` 后调用 `AudioEngine::armPlaybackStartPreRoll(...)`。
   - `AudioEngine` 在 warmup 结束后先消耗约 `25ms` 的 playback-start pre-roll blocks。
   - pre-roll 期间不调用 `renderPlaybackEventsIfNeeded()`，因此 playback position 不前进，首个 0 秒 note 不会成为设备重建后的第一个渲染事件。
   - pre-roll block 仍继续进入 synth/plugin 渲染路径，使其在首个 playback note 前处理空 MIDI 或清理 MIDI。

3. **处理 pending all-notes-off 与首个 note 的同 sample 冲突**
   - `getNextAudioBlock()` 在判断 playback pre-roll 前先执行 `injectPendingAllNotesOffIfNeeded()`。
   - 因此启动清理用 all-notes-off 会在 pre-roll block 中进入 synth/plugin，而不是与首个导入 note-on 同时进入同一个可听 block 的 sample offset `0`。

4. **保持 `RecordingEngine` 的事件调度语义**
   - `RecordingEngine::renderPlaybackBlock()` 中 `[blockStartSamples, blockEndSamples)` 的事件选择逻辑应继续支持 `timestampSamples == 0`。
   - 修复重点应放在 playback 启动前的音频链路稳定与事件 arming，而不是改变事件选择条件。

5. **增加回归覆盖**
   - 已在 Phase 4 MIDI import 测试文档补充专项首音回归清单。
   - Windows 侧人工验证显示测试样本均不再复现首音无声问题。

### 已通过人工回归

- 首个 note-on 恰好在 tick/time `0` 的单轨 MIDI：首音可听，虚拟键盘显示一致。
- 首个 note-on 非常接近 0 秒的单轨 MIDI：首音可听。
- 零时刻和弦：和弦音正常触发，虚拟键盘显示一致。
- 多个测试样本重复导入 / 播放：未再出现首音无声。
- 修复没有改写 MIDI 导入时间线，`timestampSamples == 0` 仍是合法 playback 事件。

### 后续观察

该问题已修复，但凡是后续改动触及 MIDI import / playback 启动链路，都应重新执行 Phase 4 MIDI import 测试文档 §11.1。重点关注：

- MIDI 导入时间转换、事件过滤、事件排序或 `RecordingTake` 构建。
- playback start / stop / restart、playback position 推进和 block 调度。
- audio warmup、playback-start pre-roll / arming、all-notes-off、音频设备 rebuild。
- VST3 load / unload 后立即播放导入 MIDI 的路径。

观察目标是确认首个 `timestampSamples == 0` 或近 0 秒 note 仍能稳定发声，且不会与启动清理用 all-notes-off 在同一个可听 block 内互相抵消。

### 验证注意事项

- 测试素材应保证首个 note 速度正常、持续时间足够长，排除“低 velocity / 超短音符本来就听不清”的干扰。
- 需要分别覆盖内置 fallback synth 和至少一个 VST3 乐器；部分插件可能有慢 attack、首次 preset 初始化或 first-block 行为差异。
- 虚拟键盘可视化只能证明 engine-side note scheduling 正常，不能证明插件已经发声。

### 关联文档

- [`phase4-midi-file-import.md`](phase4-midi-file-import.md)（Phase 4 测试）
- [`../features/phase4-midi-file-import.md`](../features/phase4-midi-file-import.md)（Phase 4 功能）

---

## 9. 辅助窗口与主窗口键盘焦点恢复冲突（已修复，保留架构约束）

> 触发条件：程序打开独立顶层辅助窗口，例如插件 editor、settings dialog、未来的 preset manager / about window / browser-like 窗口等。  
> 影响范围：Windows 下主窗口为了支持电脑键盘演奏，会在 `WM_ACTIVATE` / `WM_SETFOCUS` / JUCE active window 变化后异步恢复 `MainComponent` 键盘焦点。若辅助窗口打开期间仍执行 `grabKeyboardFocus()`，主窗口可能重新成为前台窗口，把辅助窗口顶到后面。  
> 当前状态：插件 editor 被主窗口顶到后面的 bug 已按最小修复处理；settings / plugin editor 打开时均会跳过主窗口焦点恢复。该条目保留为后续新增窗口的设计约束。

### 现象

- 用户加载 VST3 音源后点击 `Open Editor`。
- 插件 editor 窗口刚打开时短暂位于最前端。
- 约数百毫秒后，主窗口自动跳到最前端，插件 editor 被压到后面。
- 该问题与此前 settings 窗口打开后被主窗口顶到后端的问题同源。

### 已确认根因

主窗口存在一套主动恢复键盘焦点的机制，目的是让电脑键盘输入持续用于演奏：

1. Windows 主窗口收到 `WM_ACTIVATE` / `WM_SETFOCUS`，或 JUCE `MainWindow::activeWindowStatusChanged()` 认为主窗口 active。
2. 代码通过 `MessageManager::callAsync(...)` 异步调度 `MainComponent::restoreKeyboardFocus()`。
3. 插件 editor 创建期间，主窗口可能先排队一个焦点恢复任务。
4. editor 已经 `setVisible(true)` 并成为 active window 后，先前排队的异步任务才执行。
5. 如果 `restoreKeyboardFocus()` 没有识别当前有辅助窗口打开，就会调用 `grabKeyboardFocus()`。
6. Windows 会随之重新激活主窗口，导致辅助窗口被顶到后面。

人工诊断日志曾确认以下关键链路：

```text
PluginEditorWindow activeWindowStatusChanged, active=true
MainWindow activeWindowStatusChanged, active=false
scheduleKeyboardFocusRestore executing, reason=WM_ACTIVATE
MainComponent restoreKeyboardFocus enter, pluginEditorOpen=true
MainComponent restoreKeyboardFocus calling grabKeyboardFocus
PluginEditorWindow activeWindowStatusChanged, active=false
MainWindow activeWindowStatusChanged, active=true
```

### 已实施最小修复

- `MainComponent::restoreKeyboardFocus()`：当 settings 窗口或插件 editor 窗口打开时，直接跳过 `grabKeyboardFocus()`。
- `MainComponent::focusGained()`：当 settings 窗口或插件 editor 窗口打开时，同样跳过二次 `grabKeyboardFocus()` 同步。
- 修复后人工验证：打开插件 editor 后，主窗口不再自动顶到 editor 前面。

### 长期设计约束

后续任何“打开独立顶层窗口”的功能，都不应只在局部窗口代码中处理前置行为，而必须纳入统一的主窗口键盘焦点恢复策略。否则同类 bug 可能在新窗口上复现。

建议将焦点恢复判断集中成一个语义明确的 helper，例如：

```cpp
bool MainComponent::shouldRestoreMainKeyboardFocus() const;
```

该 helper 至少应覆盖：

- 主窗口是否正在显示。
- settings 窗口是否打开。
- 插件 editor 窗口是否打开。
- 是否存在其他由应用创建、预期保持前台或接收键盘输入的辅助顶层窗口。
- 当前 active window 是否确实是主窗口，而不是辅助窗口。
- 本次焦点恢复是否来自用户主动回到主窗口，而不是打开辅助窗口时遗留的异步 `WM_ACTIVATE` / `WM_SETFOCUS`。

推荐抽象方向：

1. **集中判断**：`restoreKeyboardFocus()` / `focusGained()` 不再各自散落窗口判断，而是统一调用 `shouldRestoreMainKeyboardFocus()`。
2. **窗口注册**：新增辅助窗口时，将其纳入“阻止主窗口抢焦点”的状态来源；可以先用各 manager 暴露的 `isOpen()`，后续再抽象为统一窗口/焦点 guard。
3. **异步任务防陈旧**：对 `scheduleKeyboardFocusRestore()` 排队的任务，在执行时重新检查当前窗口状态；不要假设排队时的 active 状态仍然有效。
4. **只在主窗口确实 active 时恢复**：焦点恢复应服务于“用户回到主窗口后继续电脑键盘演奏”，不应在辅助窗口刚打开或正在交互时抢回焦点。

### 新增窗口时的检查清单

新增任何顶层窗口前，必须检查：

- 该窗口打开后是否应保持在前台。
- 该窗口是否需要接收键盘输入，例如搜索、文本框、快捷键、插件 UI。
- 打开期间是否允许电脑键盘继续演奏；若不允许，应阻止主窗口 `grabKeyboardFocus()`。
- 关闭窗口后是否需要恢复主窗口键盘焦点；若需要，应只在关闭后显式恢复。
- Windows 侧手工测试是否覆盖：打开窗口、等待 1 秒、点击窗口内部、切回主窗口、关闭窗口、再次演奏。

### 回归建议

修改以下代码时应重新验证该项：

- `Main.cpp` 中 Windows WndProc hook、`scheduleKeyboardFocusRestore()`、`activeWindowStatusChanged()`。
- `MainComponent::restoreKeyboardFocus()` / `focusGained()` / `focusLost()`。
- settings 窗口、插件 editor 窗口或任何新增顶层窗口。
- 键盘映射、IME 抑制、主窗口焦点恢复策略。

最小人工回归：

1. 启动程序并加载一个带 editor 的 VST3 插件。
2. 点击 `Open Editor`。
3. 确认 editor 打开后保持在最前端，等待至少 1 秒主窗口不会自动顶上来。
4. 在 editor 内点击或操作控件，确认主窗口不会抢焦点。
5. 关闭 editor 后，确认主窗口可恢复电脑键盘演奏。
6. 打开 settings 窗口重复同类检查。

---

## 10. 已完成验证项（不作为风险）

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
- **MIDI 导入播放首音无声**：已通过 playback-start pre-roll / arming 修复，人工验证测试样本均不再复现。
- **插件 editor 被主窗口顶到后面**：已通过辅助窗口打开时跳过主窗口 `grabKeyboardFocus()` 修复，人工验证不再复现；长期约束见 §9。

详见：

- [`../roadmap/current-iteration.md`](../roadmap/current-iteration.md)（Phase 5-1..5-4 人工回归记录）
- [`phase2-plugin-host-lifecycle.md`](phase2-plugin-host-lifecycle.md)（Phase 2-1..2-4 测试结果）

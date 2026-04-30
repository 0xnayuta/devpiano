# M6-6e：VST3 插件离线渲染评估与设计

> 用途：评估 VST3 插件离线渲染的技术路径与设计边界，为后续实现提供决策依据。
> 读者：准备实现 M6-6e 或需要理解插件离线渲染设计选择的开发者。
> 状态：评估阶段，不阻塞当前 WAV MVP（M6-6a/b/c/d）。

相关文档：

- 功能说明：[`recording-playback.md`](M6-recording-playback.md)
- 测试文档：[`../testing/recording-playback.md`](../testing/recording-playback.md)
- 插件宿主：[`../features/M3-plugin-hosting.md`](../features/M3-plugin-hosting.md)
- 路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 1. 背景与问题

当前 WAV 导出（M6-6a/b/c/d）仅支持 fallback synth 音色。用户加载了 VST3 插件（如钢琴音色）后点击"Export WAV"，导出的仍是 fallback synth 音色，而不是当前已加载插件的音色。

**核心问题**：离线渲染链路中如何让已加载的 VST3 插件参与渲染，而不影响实时音频设备状态和插件 editor 窗口。

---

## 2. 技术约束

### 2.1 离线渲染与实时音频设备的边界

- 离线渲染不走实时 `AudioDeviceManager` 链路。
- WAV 导出期间不应影响实时音频播放或插件状态。
- 导出完成后实时音频链路应保持原状态。

### 2.2 VST3 插件实例生命周期

- VST3 插件实例通过 `AudioPluginFormatManager::createPluginInstance()` 创建。
- 插件需要 `prepareToPlay(sampleRate, blockSize)` 后才能处理 MIDI。
- 离线渲染完成后需要 `releaseResources()`。
- 已加载插件的 editor 窗口（若有）在离线渲染期间不能被销毁或阻塞。

### 2.3 状态冻结

- 插件在离线渲染期间需要保持稳定的处理状态。
- 插件的 DAW 状态（如住音、调制轮、预设切换）不应影响离线渲染结果的一致性。
- 需要决定：离线渲染使用插件当前状态，还是重置为默认状态。

### 2.4 editor 生命周期

- 若插件 editor 正在打开，离线渲染不能影响 editor 窗口。
- JUCE `AudioPluginInstance::hasEditor()` 为 true 时，实例可能持有 editor 窗口句柄。
- 需要明确离线渲染期间 editor 是否可见、是否需要隐藏。

### 2.5 失败策略

- 插件不支持离线渲染（`offlineRenderingSupported()` 返回 false）时如何处理。
- 插件加载或 prepare 失败时的降级方案（fallback synth 或报错）。
- 目标路径无权限、用户取消、空 take 等常规失败已在 M6-6a 处理。

---

## 3. 核心设计问题与可选路径

### 3.1 是否需要独立的离线渲染插件实例？

**问题**：是用实时已加载的 `AudioPluginInstance`，还是创建第二个独立实例用于离线渲染？

**选项 A：复用已加载的插件实例**

- 优点：不需要重新创建实例，节省初始化时间；使用用户当前的音色设置。
- 缺点：需要在离线渲染期间暂停实时音频链路，避免状态冲突；editor 生命周期复杂。
- 风险：用户切换音色或卸载插件会直接影响离线渲染结果的一致性。

**选项 B：创建独立的离线渲染实例**

- 优点：与实时音频链路完全解耦；用户可以继续正常使用实时音频而导出不受影响。
- 缺点：需要重新 create + prepare，占用额外内存；需要获取插件的默认音色状态。
- 关键：离线实例应使用与已加载插件相同的插件描述（`PluginDescription`），但持有独立实例。

**初步倾向**：选项 B（独立实例），因为离线渲染与实时音频的边界更清晰，风险更低。实时已加载实例若在导出期间被用户操作（切换音色/卸载），会导致导出结果不一致。

### 3.2 如何处理插件状态冻结？

**问题**：离线渲染时，插件的住音、调制轮、预设等状态应该冻结在哪个时间点？

- **方案 1**：记录离线渲染开始时插件的当前状态快照，导出完成后恢复。
- **方案 2**：离线实例始终重置为插件的默认状态（program 0，CC=0），不依赖实时状态。
- **方案 3**：用户在导出选项中自行选择"使用当前插件状态"或"重置为默认"。

**初步倾向**：方案 2（重置为默认状态），简化设计，避免状态同步复杂度。

### 3.3 editor 窗口如何处理？

**问题**：当 `instance->hasEditor()` 为 true 时，离线渲染期间 editor 窗口应该如何处理？

- **选项 A**：离线渲染期间暂时隐藏 editor（`editor->setVisible(false)`），导出完成后恢复。
- **选项 B**：离线渲染使用无 editor 的内部实例（`createPluginInstance` 不创建 editor）。
- **选项 C**：离线渲染期间禁止打开 editor（通过 UI 状态禁用"Open Editor"按钮）。

**初步倾向**：选项 B，在离线渲染时不创建 editor 窗口，避免窗口句柄生命周期问题。

### 3.4 降级方案

- 若插件 `offlineRenderingSupported()` 返回 false，或实例创建/prepare 失败，降级到 fallback synth 并记录日志。
- 不因插件问题阻塞 WAV 导出，用户仍可得到 fallback synth 的 WAV。

---

## 4. 推荐的实现路径

### 路径：独立离线实例 + 重置状态 + 无 editor

1. **离线实例创建**：使用与已加载插件相同的 `PluginDescription`，调用 `formatManager.createPluginInstance()` 创建独立实例。
2. **状态重置**：创建后立即重置为插件默认状态（program 0，所有 CC=0），不依赖实时插件状态。
3. **无 editor**：离线实例创建时不触发 editor 创建（`createPluginInstance` 可控制此行为）。
4. **prepare**：使用导出时的采样率和固定 blockSize（如 512）调用 `prepareToPlay()`。
5. **渲染**：与 fallback synth WAV 导出相同的 block 循环逻辑，将 `RecordingTake` 事件写入 `MidiBuffer`，送入离线实例 `processBlock()`。
6. **finish**：渲染完成后调用 `releaseResources()`，删除实例（通过 `std::unique_ptr` 或 `ScopedJuceDecl`。

### 降级处理

- 离线实例创建或 prepare 失败 → 降级到 fallback synth，记录日志。
- 导出选项中不提供"使用当前插件状态"的开关，简化设计。

---

## 5. 实施前的验证项

在进入实现之前，需要验证以下问题（可手工测试）：

1. 在已有插件加载状态下，执行导出操作是否会影响实时音频（停顿/爆音）？
2. `createPluginInstance` 在同一 `PluginDescription` 上调用两次（实时实例 + 离线实例）是否会有问题？
3. 插件的 `prepareToPlay` 在非实时 sampleRate（如 44100）下是否能正常初始化？
4. 离线渲染完成后原实时插件实例是否仍然正常工作？

---

## 6. 与 fallback synth WAV MVP 的差异

| 维度 | fallback synth（M6-6b） | VST3 插件离线渲染（M6-6e） |
|---|---|---|
| 发声体 | `juce::Synthesiser` 离线构建 | 独立 `AudioPluginInstance` |
| 状态 | 内置 ADSR / gain | 插件自身处理链 |
| 状态冻结 | N/A（内置） | 需决定重置策略 |
| editor | N/A | 离线实例不创建 editor |
| 失败降级 | 无（fallback 即全部） | 降级到 fallback synth |
| 实现复杂度 | 低 | 中 |

---

## 7. 非目标（明确不做）

- 不在同一实时已加载实例上同时进行实时音频和离线渲染。
- 不在离线渲染期间保持插件的实时 DAW 状态（如 CC/mod wheel）。
- 不支持 VST2 插件离线渲染（当前 VST3-first）。
- 不在离线渲染期间打开或操作插件 editor。

---

## 8. 后续行动

- 确认路径 B（独立离线实例 + 重置状态 + 无 editor）是否可接受。
- 在有真实 VST3 插件环境下验证"实施前的验证项"。
- 若验证通过，再拆分为 M6-6e 实现切片（参考 M6-6a/b/c/d 的切片方式）。
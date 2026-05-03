# 演奏数据持久化与播放体验增强验收测试（Phase 6）

> 用途：记录 Phase 6 演奏文件保存/打开、播放速度控制、最近文件列表、基础编辑和 MIDI 导入增强的专项验收测试。
> 读者：开发者、测试者、阶段验收者。
> 更新时机：Phase 6 各子阶段实现状态变化、验收结果更新时。

相关文档：

- 功能说明：[`../features/phase6-performance-persistence.md`](../features/phase6-performance-persistence.md)
- 路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)
- 阶段验收：[`../testing/acceptance.md`](../testing/acceptance.md)

---

## 1. 测试范围

覆盖：

- Phase 6-1：演奏文件保存/打开（`.devpiano` 格式）。
- Phase 6-2：播放速度控制（0.5x–2.0x）。
- Phase 6-3：最近文件列表 + 拖拽打开。
- Phase 6-4：基础 MIDI 编辑（delete notes）。
- Phase 6-5：MIDI 导入增强（sustain CC64、pitch bend、program change）。

不覆盖：

- 完整工程文件（`.devpiano-project`，Phase 7）。
- 多轨 / tempo map（Phase 7+）。
- Piano roll / 事件编辑器（Phase 8）。
- 旧 FreePiano `.fpm` 格式兼容。

---

## 2. 测试环境

- 在 Windows 镜像树中运行 `DevPiano.exe`。
- 构建验证优先使用：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

- 如链接失败并提示 `DevPiano.exe` 无法写入，先关闭正在运行的程序后重试。

---

## 3. 测试素材建议

### Phase 6-1 素材

1. **新录制的 take**：录制几秒电脑键盘演奏，用于保存测试。
2. **已保存的 `.devpiano` 文件**：由本程序保存生成，用于打开测试。
3. **损坏的 `.devpiano` 文件**：手动编辑 JSON 破坏结构，用于错误路径测试。
4. **空 JSON 文件**：内容为 `{}`，用于边界测试。
5. **非 JSON 文件**：扩展名改为 `.devpiano` 的文本文件，用于格式错误测试。

### Phase 6-2 素材

1. **任意已录制或已导入的 take**：用于播放速度测试。

### Phase 6-3 素材

1. **多个 `.devpiano` 文件**：用于最近文件列表测试。
2. **多个 `.mid` 文件**：用于混合最近列表测试。
3. **用于拖拽测试的 `.devpiano` 和 `.mid` 文件**。

### Phase 6-4 素材

1. **包含多个音符的录制 take**：用于选中和删除测试。

### Phase 6-5 素材

1. **包含 sustain CC64 的 MIDI 文件**。
2. **包含 pitch bend 的 MIDI 文件**。
3. **包含 program change 的 MIDI 文件**。
4. **仅包含 note on/off 的 MIDI 文件**（回归测试）。

---

## 4. Phase 6-1：演奏文件保存/打开

状态：✅ 已完成。

### 4.1 保存功能

验收项：

- [x] 有录制 take 且状态为 idle/stopped 时，Save 按钮 enabled。
- [x] 无录制 take 时，Save 按钮 disabled。
- [x] Recording 期间，Save 按钮 disabled。
- [x] Playing 期间，Save 按钮 disabled。
- [x] 点击 Save 按钮后 FileChooser 打开。
- [x] FileChooser 默认文件名包含时间戳（如 `performance-20260503-120000.devpiano`）。
- [ ] FileChooser 默认目录为上次保存路径（首次为系统默认目录）。**→ Phase 6-3**
- [x] 选择保存位置后，文件成功写入。
- [x] 保存的文件可用文本编辑器打开，内容为合法 JSON。
- [x] JSON 包含 `version` 字段，值为 `1`。
- [x] JSON 包含 `format` 字段，值为 `"devpiano-performance"`。
- [x] JSON 包含 `sampleRate` 字段，值与录制时采样率一致。
- [x] JSON 包含 `lengthSamples` 字段，值与录制时长度一致。
- [x] JSON 包含 `events` 数组，事件数与录制 take 一致。
- [x] 每个 event 包含 `timestampSamples`、`source`、`midiData` 字段。
- [x] `midiData` 字段为字节数组，可还原为 `juce::MidiMessage`。
- [x] 保存完成后 Logger 输出文件路径和事件数。

测试步骤：

1. 启动 DevPiano。
2. 点击 Record，用电脑键盘演奏几个音符（如 C-E-G-C），点击 Stop。
3. 确认 Save 按钮 enabled。
4. 点击 Save，FileChooser 打开。
5. 确认默认文件名包含时间戳。
6. 选择保存位置，确认保存。
7. 用文本编辑器打开保存的文件，检查 JSON 结构。
8. 确认 Logger 输出了文件路径和事件数。

### 4.2 打开功能

验收项：

- [x] 非录制/播放状态时，Open 按钮 enabled。
- [x] Recording 期间，Open 按钮 disabled。
- [x] Playing 期间，Open 按钮 disabled。
- [x] 点击 Open 按钮后 FileChooser 打开，文件过滤器为 `*.devpiano`。
- [ ] FileChooser 默认目录为上次打开路径。**→ Phase 6-3**
- [x] 选择有效的 `.devpiano` 文件后，文件成功读取并解析。
- [x] 打开后自动开始回放。
- [x] 回放内容与原始录制一致（音符、时序）。
- [x] 打开后 Logger 输出文件路径和事件数。
- [x] 打开后 Export MIDI 按钮行为与录制 take 一致（可导出 MIDI）。
- [x] 打开后 Export WAV 按钮可用。

测试步骤：

1. 使用 4.1 中保存的 `.devpiano` 文件。
2. 启动 DevPiano（或先录制一段再停止，确保有初始状态）。
3. 点击 Open，FileChooser 打开。
4. 确认文件过滤器为 `*.devpiano`。
5. 选择之前保存的文件，确认打开。
6. 确认自动开始回放，音符内容与原始录制一致。
7. 确认 Logger 输出了文件路径和事件数。
8. Stop 后确认 Export MIDI 和 Export WAV 按钮可用。

### 4.3 错误路径（暂缓 → Phase 6-3）

### 4.4 路径记忆（暂缓 → Phase 6-3）

### 4.5 Roundtrip 一致性（暂缓 → Phase 6-3）

---

## 5. Phase 6-2：播放速度控制

状态：暂缓（尽快做）。

验收项：

- [ ] ControlsPanel 显示当前播放速度，默认 `1.00x`。
- [ ] `-` 按钮可降低速度，最低到 `0.50x`。
- [ ] `+` 按钮可提高速度，最高到 `2.00x`。
- [ ] 速度到达边界时对应按钮 disabled。
- [ ] 播放中调整速度，回放速率立即变化。
- [ ] 0.50x 慢速播放时音符间隔明显拉长，无卡顿。
- [ ] 2.00x 快速播放时音符间隔明显缩短，无爆音。
- [ ] 速度值在程序重启后自动恢复。
- [ ] 打开 `.devpiano` 文件或导入 `.mid` 文件后，速度控制同样生效。

---

## 6. Phase 6-3：最近文件列表 + 拖拽打开

状态：暂缓。

### 6.1 最近文件列表

验收项：

- [ ] 打开 `.devpiano` 文件后，最近文件列表更新。
- [ ] 导入 `.mid` 文件后，最近文件列表更新。
- [ ] 最近文件列表最多显示 10 条。
- [ ] 同一文件重复打开时，列表中只保留一条（移到最前）。
- [ ] 点击最近文件列表项可打开对应文件。
- [ ] 列表中文件不存在时，点击后提示文件不存在并从列表移除。
- [ ] 最近文件列表在程序重启后仍然有效。

### 6.2 拖拽打开

验收项：

- [ ] 拖拽 `.devpiano` 文件到主窗口，触发打开演奏文件。
- [ ] 拖拽 `.mid` 文件到主窗口，触发 MIDI 导入。
- [ ] 拖拽不支持的文件类型时，无反应或提示不支持。
- [ ] Recording / Playing 期间拖拽文件，忽略或提示。

---

## 7. Phase 6-4：基础 MIDI 编辑（delete notes）

状态：暂缓。

验收项：

- [ ] 打开或录制的演奏数据中，可选中单个音符。
- [ ] 选中的音符有视觉高亮。
- [ ] 按 Delete 键或点击 Delete 按钮可删除选中音符。
- [ ] 删除后回放不再包含该音符。
- [ ] 删除操作不破坏其他事件的时间线和顺序。
- [ ] 删除后可正常保存为 `.devpiano` 文件，保存的文件不含已删除事件。
- [ ] 删除后可正常导出 MIDI 和 WAV。

---

## 8. Phase 6-5：MIDI 导入增强

状态：暂缓。

验收项：

- [ ] 导入包含 sustain CC64 的 MIDI 文件后，回放时延音踏板效果可听（依赖插件支持）。
- [ ] 导入包含 pitch bend 的 MIDI 文件后，回放时弯音效果可听（依赖插件支持）。
- [ ] 导入包含 program change 的 MIDI 文件后，回放时音色变化可听（依赖插件支持）。
- [ ] 导入仅包含 note on/off 的 MIDI 文件时，行为与 Phase 4 一致，无回退。
- [ ] Logger 输出导入的非 note 事件数量。
- [ ] 导入后保存为 `.devpiano` 文件，非 note 事件正确保留。
- [ ] 打开包含非 note 事件的 `.devpiano` 文件后，回放时非 note 事件正确还原。

---

## 9. 回归项

以下为 Phase 6 各子阶段实现后需持续关注的回归项：

- [ ] Phase 3 录制/回放/MIDI 导出/WAV 导出主链路不受影响。
- [ ] Phase 4 MIDI 导入/回放/自动选轨行为不受影响。
- [ ] Phase 4 导入 playback take 禁止 MIDI 再导出的边界不受影响。
- [ ] Phase 5 架构收敛后的模块职责边界不受影响。
- [ ] 插件加载/卸载/editor 窗口行为不受影响。
- [ ] 键盘映射和虚拟键盘显示行为不受影响。
- [ ] 布局 Preset 保存/加载/恢复行为不受影响。

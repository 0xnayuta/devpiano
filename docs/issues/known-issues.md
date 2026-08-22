# 已知问题与待验证风险

> 状态：轻量风险清单。详细项目状态以路线图和当前迭代文档为准。
> 更新时机：发现新问题、完成验证、搁置项恢复时。

当前项目状态与风险以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准；阶段验收见 [`../reference/acceptance.md`](../reference/acceptance.md)。

---

## 1. 当前限制与未修复问题

功能缺口和已确认但尚未修复的缺陷。

### 插件生命周期退出告警

> scan / load / unload / editor / 重扫 / 直接退出等主要生命周期路径已通过人工回归，未发现功能性问题。特定插件或 Debug 注入环境下退出阶段可能仍有 JUCE / VST3 调试告警，低优先级持续观察。

详见：[`../reference/features/plugin-hosting.md`](../reference/features/plugin-hosting.md)

---

## 2. 已修复问题（回归参考）

以下问题已修复，保留简要记录用于回归识别。详细根因分析和修复实现见各功能文档。

### 启动早期首音音高异常

启动后或插件加载/卸载后立即弹奏，前几个音音高异常。根因：音频设备 prepare 后首批 audio blocks 经过未完全稳定的渲染路径。修复：`AudioEngine` 增加 `25ms` warmup（静音 + 清理 pending MIDI），修正设备初始化顺序（`setAudioChannels` 直接传入保存的 XML）。

- **回归线索**：启动 / 插件重建后首音音调错误
- **关联**：`AudioEngine::prepareToPlay()` warmup 机制

### MIDI 导入播放首音无声

导入的 MIDI 文件首个音符起始时间接近 0s 时播放几乎无声。根因：音频设备重建 + warmup 后首个 0s note 与清理用 all-notes-off 在同一个可听 block 内冲突。修复：playback-start pre-roll / arming 机制。

- **回归线索**：导入首个 note 在 0s 的 MIDI 文件，播放后首音无声
- **关联**：`AudioEngine::armPlaybackStartPreRoll()`，[`../reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)

### 辅助窗口键盘焦点冲突

打开插件 editor 或 settings 窗口后，主窗口异步抢回焦点将辅助窗口顶到后面。根因：`WM_ACTIVATE` / `activeWindowStatusChanged` 触发的异步焦点恢复任务在辅助窗口已打开后才执行 `grabKeyboardFocus()`。修复：`restoreKeyboardFocus()` / `focusGained()` 在 settings 或 plugin editor 打开时跳过 `grabKeyboardFocus()`。

- **回归线索**：打开插件 editor 后主窗口自动跳到前台
- **关联**：`MainComponent::restoreKeyboardFocus()`；新增顶层窗口时须纳入统一焦点恢复策略

### Phase 6-2 播放速度控制

含三个子问题：(1) 倍率公式反用（0.5x 反而加快）；(2) 速度切换时 note-off 丢失导致音长时间悬停；(3) 播放状态三成员跨线程数据竞争（裸 `double` / `std::int64_t` 无同步）。修复：(1) 乘法改除法；(2) 速度切换时重校准 `playbackPositionSamples`；(3) 全部改为 `std::atomic<>`。

- **回归线索**：播放中切换速度 → 方向反向 / 悬挂音 / 数据竞争 UB
- **关联**：`RecordingEngine::setPlaybackSpeedMultiplier()`，[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)
### 虚拟键盘音域标准 88 键收敛（A0~C8）

原虚拟键盘默认硬编码全量 128 键（0~127），导致超出物理大三角钢琴 88 键（MIDI 21~108）的两端琴键（如 F#8 / MIDI 114 等高频音区）在特定音频硬件/分频器或 88 键 VST3 插件下无法正常发声或存在声学盲区。修复：将 `CustomKeyboard` 及 `KeyboardSettings` 默认可用范围严格收敛至真实大三角钢琴的 88 键标准音域（MIDI 21 A0 到 MIDI 108 C8），52 白键 + 36 黑键精确 1:1 对齐，彻底消除两端无效音区与声学陷波盲区。

- **回归线索**：虚拟键盘首尾键分别为 A0(21) 与 C8(108)，点击各音区均发声正常且无多余超声/次声键位
- **关联**：`CustomKeyboard::setAvailableRange(21, 108)`，`KeyboardTypes.h`，`KeyboardHitMappingTest.cpp`

### 非 ASCII UTF-8 字符显示乱码（最近文件菜单音符图标）

最近文件下拉菜单中 `.mid` / `.devpiano` 文件名前的 ♪ / ♫ 图标显示为 `â™ª` / `â™«` 等乱码。根因：`showRecentFilesMenu()` 用裸 `const char*` 字面量（`"\xe2\x99\xaa"`）构造 `juce::String`，MSVC 按系统代码页（Windows-1252）而非 UTF-8 解读多字节序列。修复：统一用 `juce::String::fromUTF8()` 显式指定 UTF-8 编码，与 `LocaleManager.h` 中非 ASCII 字符串的处理方式一致。

- **回归线索**：最近文件菜单中 `.mid` 文件前出现 `â` 等乱码字符
- **关联**：`MainComponent::showRecentFilesMenu()`，`juce::String::fromUTF8()`，`Locale/LocaleManager.h`

### Performance Preset 导入同名覆盖确认（Phase 16 已解决）

导入同名 `.devpiano.preset` 文件时无确认提示直接覆盖。修复：在 `PresetFlowSupport::handleImportPresetFile()` 中检测目标预设文件是否存在，存在时调用 `PresetConfirmDialog::show` 弹出声明式覆盖确认对话框（`TRANS("Overwrite Preset?")`），用户确认后覆盖，取消则安全放弃。

- **回归线索**：导入同名预设文件直接覆盖而无弹窗提示
- **关联**：`PresetFlowSupport::handleImportPresetFile()`，`PresetConfirmDialog`，[`../reference/features/performance-presets.md`](../reference/features/performance-presets.md)

### 虚拟键盘高频 MIDI 播放 CPU 占用（Phase 14 + Phase 16 已解决）

在播放密集 MIDI 文件时，音频 DSP 与 UI 渲染占用大量 CPU。修复：
1. **DSP 层（Phase 14-A）**：`PianoSynthVoice` 采用 Magic Circle 二阶递归正弦振荡器，消除 `std::sin`，音频线程单核 CPU 降至 ~0.7%；
2. **UI 渲染层（Phase 16-A）**：`CustomKeyboard` 引入局部脏矩形重绘（`repaintKey(k)`）与 `g.getClipBounds()` 快速裁剪早退，消灭全量 88 键 `repaint()`，UI 光栅化渲染耗时降低 70% 以上。

- **回归线索**：密集 MIDI 播放时 UI 线程满载 / 虚拟键盘按键残影
- **关联**：`CustomKeyboard::timerCallback()`，`CustomKeyboard::repaintKey()`，[`../reference/features/builtin-piano-synthesis.md`](../reference/features/builtin-piano-synthesis.md)

---

## 3. 环境说明

### 构建与环境

WSL / Windows 镜像构建环境问题见 [`../guides/troubleshooting.md`](../guides/troubleshooting.md)。

Windows MSVC 侧 CMake 缓存未追踪源文件变更可能导致旧目标文件未重新编译，运行时出现 `WeakReference::SharedPointer::get()` 访问冲突。快速修复：删除 `build-win-msvc/CMakeCache.txt` 后重新 `./scripts/dev.sh win-build`。

### WSL 环境 JUCE Files/Writing 单元测试失败

以 root 用户（`uid=0`）在 WSL 中运行单元测试时，`tempFile.setReadOnly(true)` 移除了文件写权限，但 `tempFile.hasWriteAccess()` 因 POSIX `access(path, W_OK)` 对 superuser 始终返回成功而返回 `true`。**不影响任何项目功能**——该测试为 JUCE 自带文件系统验证，项目代码不依赖 `setReadOnly` / `hasWriteAccess`。非 root 用户下该测试自动通过。

- **缓解**：`devpiano_tests` 默认只运行项目自身测试（类别白名单 `DevPiano/Core` / `DevPiano/Recording` / `DevPiano/Engine` / `DevPiano/UI`，`Files` 默认跳过），该问题不再触发。仅当显式 `--include-juce --include-files` 全量运行时才会遇到，非 root 用户或跳过该组合即可。
  - 注：旧缓解命令 `--category "DevPiano"` 已失效（`juce::UnitTest::getTestsInCategory` 精确匹配，"DevPiano" 不匹配任何项目类别），请使用上述默认行为或精确类别名。

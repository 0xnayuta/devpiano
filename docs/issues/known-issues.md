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

### C++ 构建耗时热点：std::regex 重复模板实例化与重型 UI 测试单元

> 通过 `-ftime-trace` 与 `scripts/analyze_build_time.py` 首次构建全量微观剖析（142 个编译单元，累计 1034s CPU 耗时）发现两个主要编译期性能瓶颈：
> 1. **`std::regex` 模板递归膨胀**：`std::basic_regex<char>` 及其内部编译器 `std::__detail::_Compiler` 在 42 个编译单元中被反复递归实例化，累计耗费高达 **30.29 秒** 的 CPU 编译时间；
> 2. **重型 UI 测试编译单元**：`SettingsLayoutModelTest.cpp`（14.01s）与 `JiveModalDialogTest.cpp`（13.17s）因集中实例化了复杂的 JIVE 声明式解释器、样式引擎与全部组件工厂，成为业务层最慢的编译单元。
>
> **优化方向与跟进计划**：
> - 将涉及正则表达式的业务逻辑严格封装至独立 `.cpp` 中，阻断 `<regex>` 在头文件中对包含者的级联模板污染，或采用确定性状态机/轻量字符串匹配替代；
> - 针对测试工程后续可精细化拆分测试单元或引入针对业务层的 Unity Build 批处理。


### Linux/X11 窗口大小锁定（Resizable 开关）在框架层失效

> **现状与成因**：
> 已移除设置面板中的"可调整窗口大小"选项，主窗口保持始终可调。
> 根因在 JUCE X11 后端 `XWindowSystem::setBounds`（`juce_XWindowSystem_linux.cpp`）：
> 每次布局都会先调用 `updateConstraints` 写入 `WMNormalHints`（固定尺寸时
> `PMinSize == PMaxSize`），随后又用仅含 `USSize | USPosition` 的 `XSizeHints`
> 整体覆盖 `WM_NORMAL_HINTS`，导致尺寸锁定约束在同一次 `setBounds` 内即被
> 清除，KWin 始终认为窗口可自由缩放。
>
> **影响**：任何 `setResizable(false)` + `setResizeLimits(w,h,w,h)` 的组合在
> Linux/X11 下都无法真正锁定窗口大小（KWin + 125% 缩放实测确认）。
>
> **跟进计划**：若后续需要恢复该能力，可在应用层直接调用 Xlib
> `_MOTIF_WM_HINTS`（`MWM_FUNC_RESIZE` 关闭）或等待 JUCE 修复
> `setBounds` 的 hints 覆盖问题；届时需权衡原生代码侵入成本。

### Main.cpp 中残留的 Win32 原生 Hook 与平台特定依赖

> **现状与成因**：
> `source/Main.cpp` 目前在 `#if defined(JUCE_WINDOWS) && JUCE_WINDOWS` 条件下引入了 `<windows.h>`，并通过 Win32 API（`SetWindowLongPtrW` 子类化 Hook 顶层窗口的 `WNDPROC` 监听 `WM_SETFOCUS`/`WM_ACTIVATE`，以及通过 `AttachThreadInput` + `SetForegroundWindow`）确保 Windows 环境下启动即弹奏的物理键盘焦点获取。
>
> **架构瑕疵与潜在风险**：
> 1. **违背跨平台标准封装**：devpiano 作为基于 JUCE 的现代跨平台应用，顶层 Shell 应保持平台无关性，引入原生 Win32 Hook 属于侵入式的平台专有补丁；
> 2. **宏污染风险**：直接包含 `<windows.h>` 存在引入 Win32 全局宏污染（如 `min`/`max`/`ERROR`）的隐患；
> 3. **线程挂接侵入性**：跨线程 `AttachThreadInput` 强夺前台焦点可能在复杂的多窗口切换环境下产生焦点竞争。
>
> **重构方向与跟进计划**：
> 在后续的维护迭代中，将该部分彻底重构为纯 JUCE 跨平台标准机制：
> - 利用 JUCE 原生的 `DocumentWindow::activeWindowStatusChanged()` 配合 `juce::MessageManager::callAsync` 延迟分发 `restoreKeyboardFocus()`，天然解决 Windows 原生激活事件到达时的时序抖动，彻底废弃 `WNDPROC` Hook；
> - 采用 JUCE 标准的 `juce::Process::makeForegroundProcess()` 或 `toFront(true)` 替代 `AttachThreadInput`；
> - 最终实现 `source/Main.cpp` 乃至整个源码树 100% 纯净、无任何 `<windows.h>` 依赖的标准跨平台设计。

### 虚拟键盘 CustomKeyboard 在潜在多线程 MIDI 驱动场景下的线程隔离设计约束

> **现状与分析**：
> 目前在 devpiano 架构中：
> 1. MIDI 回放驱动（`RecordingSessionController::timerCallback`）运行在 UI 消息线程；
> 2. 电脑键盘弹奏（`MainComponent::keyPressed`）与鼠标点击（`CustomKeyboard::mouseDown`）均在 UI 消息线程分发；
> 因此 `MidiKeyboardState::Listener` 的 `handleNoteOn` 回调与 `CustomKeyboard::timerCallback` 的 `perKeyChannel` / `perKeyVelocity` 读取目前均在 UI 消息线程同步执行，不存在运行时数据竞争。
>
> **长期架构约束与演进建议**：
> 未来若拓展直接由音频硬件回调线程（如直接接管 `MidiInputCallback` 或在 `AudioEngine::audioDeviceIOCallbackWithContext` 内部）直接向共享 `MidiKeyboardState` 批量灌入 `processNextMidiBuffer` 时：
> - `handleNoteOn` 将在音频实时线程上同步触发；
> - 此时 `perKeyChannel` 应从裸 `std::array<uint8_t, 128>` 升级为显式 `std::array<std::atomic<uint8_t>, 128>`，或通过轻量 lock-free SPSC 队列投递至 UI 线程，彻底避免实时线程与 UI 渲染线程间的共享数据竞争。
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

### 失焦 panic 的适用范围（交互演奏 vs MIDI 回放）

失焦时全引擎静音（`requestAllNotesOff`）会误杀 MIDI 回放中的音符：回放由纯时间线驱动（`RecordingEngine::renderPlaybackBlock`），被 all-notes-off 杀掉的音符不会自动重新发声，声音会断到时间线上的下一个音符，表现为"极短暂停止"。修复（方案 A）：失焦处理改为异步判定——焦点转移到本进程其他顶层窗口（插件编辑器、设置窗口）时不打断任何演奏；焦点真正离开应用时只释放交互演奏音（`KeyboardMidiMapper::releaseAllHeldKeys` + `CustomKeyboard::releaseHeldMouseNote`），不再调用全引擎静音。`requestAllNotesOff` 保留给暂停/停止/倒带回放的显式场景。

- **回归线索**：① MIDI 回放中 Alt+Tab 或打开编辑器/设置窗口，声音短暂中断；② 打开插件编辑器时键盘演奏音被误切
- **关联**：`MainComponent::handleWindowFocusLost()`；行为矩阵见 [`../reference/features/keyboard-mapping.md`](../reference/features/keyboard-mapping.md)

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

### Ubuntu 26.04 下 JIVE 文本不渲染：JUCE 字体扫描不识别 .ttc（system-ui → Noto CJK）

> **现象**：Ubuntu 26.04（实测 26.04.1 LTS）本地 Debug 构建运行单元测试，`JiveRenderTest` 的 `header title renders visible pixels` 用例失败（`light=0`，标题文本一个像素都渲染不出来）；CI（Ubuntu 24.04）同代码通过，其他文本相关用例（按钮标签、卡片标题）因像素来自边框而未被暴露。
>
> **成因**（JUCE 子模块 `91ad83ae34` 与 Ubuntu 26.04 字体配置的交互）：
> 1. JIVE 的 `Text` 组件默认以 `Font("system-ui", …)` 创建字体；
> 2. Ubuntu 26.04 的 fontconfig 把 `system-ui` 解析为 **Noto Sans CJK（`.ttc` 集合）**，而 Ubuntu 24.04 解析为 DejaVu Sans（`.ttf`）；
> 3. 该版本 JUCE 的 `FTTypefaceList::scanFontPaths`（`juce_Fonts_freetype.cpp`）**只扫描 `.ttf` / `.otf` 扩展名，不扫 `.ttc`**，`matchTypeface` 无法命中 Noto CJK family；
> 4. `Font::getDefaultTypefaceForFont` 走 `findSystemTypeface` → fontconfig 拿到 Noto Regular（style 与请求的 Bold 不匹配）→ 递归 `createFace("Noto Sans CJK SC")` 失败 → fallback 也无法把 `system-ui` 映射为真实 family（JUCE 占位名是 `<Sans-Serif>`）→ **typeface 为 null → 无字形 → 文本不渲染**。
>
> **修复（环境级，不改子模块）**：将系统 `.ttc` 复制为 `.ttf` 后缀放入用户字体目录，仅改变扩展名使 JUCE 扫描列表包含 Noto CJK family（文件内容不变）：
> ```bash
> mkdir -p ~/.local/share/fonts
> cp /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc ~/.local/share/fonts/NotoSansCJK-Regular.ttf
> cp /usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc    ~/.local/share/fonts/NotoSansCJK-Bold.ttf
> fc-cache -f
> ```
> 修复后 `devpiano_tests` 全量 **11986/11986** 通过。
>
> - **回归线索**：JIVE 文本组件离屏渲染无像素（`light=0`）；`fc-match system-ui` 返回 `.ttc` 路径
> - **关联**：`source/tests/StyleCatalogTest.cpp`（`JiveRenderTest`），`juce_Fonts_freetype.cpp`（`scanFontPaths` / `matchTypeface`）；新装其他 Linux 发行版若 `system-ui` 被映射到 `.ttc` 字体，同样需要此修复

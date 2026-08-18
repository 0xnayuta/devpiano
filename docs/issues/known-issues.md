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

### Performance Preset ID 冲突

> 用户 preset ID 由文件名派生；导入同名文件时覆盖行为已实现，暂无明确冲突提示 UI。正常使用不受影响，低优先级。

详见：[`../reference/features/performance-presets.md`](../reference/features/performance-presets.md)

### 内置物理建模钢琴 MIDI 播放 CPU 占用率与优化空间

**现象**：在 AMD Ryzen 5 5600X（6 核心 12 线程）设备上，使用默认内置 Piano v2 音色播放音符密集的 MIDI 文件时，任务管理器显示 CPU 占用约为 7%~9%。由于 6C/12T 架构下单逻辑核心满载即对应约 $100\% / 12 \approx 8.33\%$，该占用率表明应用在播放密集 MIDI 时接近占满了一个完整 CPU 逻辑核心的算力。

**深度构成剖析**：
1. **UI 消息线程渲染（主要消耗源，约占 5%~7% 单核）**：
   - MIDI 播放时伴随大量密集的 `noteOn` / `noteOff` 事件，`CustomKeyboardComponent` 接收到消息后高频触发虚拟键盘 88 键的全量 `repaint()`；
   - JUCE 在 Windows 平台默认使用软件光栅化渲染器（Software Rasterizer），每秒数十次全量重绘包含圆角、文字排版、按键高亮、发光与抗锯齿的完整键盘，构成了主要的 CPU 消耗。
2. **音频合成线程（次要消耗源，约占 1.0%~1.5% 单核）**：
   - 8 复音并发时，每采样点需计算 64 次标准库 `std::sin` 与 24 次二阶带通滤波状态更新；
   - 44.1 kHz 采样率下每秒约执行 $2.82 \times 10^6$ 次 `std::sin`，在 Zen 3 架构（4.6 GHz）下单核耗时约 10~15 ms/秒，实际只占单核的 1.0%~1.5%。
3. **音频驱动缓冲区与线程切换调度（约占 0.5%~1.0% 单核）**。

**优化空间与演进路线（预计可将占用率压缩至 < 2%）**：
- **优化点 1：UI 脏矩形局部重绘与 60Hz 帧率节流（UI 层，收益最大）**：
  - 虚拟键盘仅对变动的按键区域调用局部 `repaint(keyBounds)`，或在 60Hz 定时器回调中批量合并重绘，避免每次 MIDI 消息触发 88 键全量刷新，预计可削减 UI 渲染耗时 70% 以上。
- **优化点 2：二阶递归正弦振荡器（DSP 层）**：
  - 借鉴 Mutable Instruments / DaisySP / JOS Magic Circle 工业级模态合成做法，使用二阶递归正弦振荡器（Magic Circle / Coupled Form）：
    $$u[n] = u[n-1] - \epsilon \cdot v[n-1], \quad v[n] = v[n-1] + \epsilon \cdot u[n]$$
    逐采样仅需 2 次乘加即可生成纯正弦波，**零三角函数调用**，消除 `std::sin` 的计算开销。
- **优化点 3：模态分音衰减动态剪枝（Partial Pruning）**：
  - 随着模态能量耗散，高次分音在几百毫秒内衰减至 -80 dB 以下，动态缩减各 voice 的 `numActivePartials`，避免无谓计算人耳已不可闻的高频分音。

**当前状态与优先级**：日常键盘实时弹奏（单音/和弦）CPU 占用极低（< 1%），仅在密集多复音 MIDI 连续播放时显现；无爆音或丢帧现象，属于中优先级性能优化项，拟在后续波导合成或 UI 渲染优化迭代中一并实施。

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

### 非 ASCII UTF-8 字符显示乱码（最近文件菜单音符图标）

最近文件下拉菜单中 `.mid` / `.devpiano` 文件名前的 ♪ / ♫ 图标显示为 `â™ª` / `â™«` 等乱码。根因：`showRecentFilesMenu()` 用裸 `const char*` 字面量（`"\xe2\x99\xaa"`）构造 `juce::String`，MSVC 按系统代码页（Windows-1252）而非 UTF-8 解读多字节序列。修复：统一用 `juce::String::fromUTF8()` 显式指定 UTF-8 编码，与 `LocaleManager.h` 中非 ASCII 字符串的处理方式一致。

- **回归线索**：最近文件菜单中 `.mid` 文件前出现 `â` 等乱码字符
- **关联**：`MainComponent::showRecentFilesMenu()`，`juce::String::fromUTF8()`，`Locale/LocaleManager.h`

---

## 3. 环境说明

### 构建与环境

WSL / Windows 镜像构建环境问题见 [`../guides/troubleshooting.md`](../guides/troubleshooting.md)。

Windows MSVC 侧 CMake 缓存未追踪源文件变更可能导致旧目标文件未重新编译，运行时出现 `WeakReference::SharedPointer::get()` 访问冲突。快速修复：删除 `build-win-msvc/CMakeCache.txt` 后重新 `./scripts/dev.sh win-build`。

### WSL 环境 JUCE Files/Writing 单元测试失败

以 root 用户（`uid=0`）在 WSL 中运行单元测试时，`tempFile.setReadOnly(true)` 移除了文件写权限，但 `tempFile.hasWriteAccess()` 因 POSIX `access(path, W_OK)` 对 superuser 始终返回成功而返回 `true`。**不影响任何项目功能**——该测试为 JUCE 自带文件系统验证，项目代码不依赖 `setReadOnly` / `hasWriteAccess`。非 root 用户下该测试自动通过。

- **缓解**：`devpiano_tests` 默认只运行项目自身测试（类别白名单 `DevPiano/Core` / `DevPiano/Recording` / `DevPiano/Engine` / `DevPiano/UI`，`Files` 默认跳过），该问题不再触发。仅当显式 `--include-juce --include-files` 全量运行时才会遇到，非 root 用户或跳过该组合即可。
  - 注：旧缓解命令 `--category "DevPiano"` 已失效（`juce::UnitTest::getTestsInCategory` 精确匹配，"DevPiano" 不匹配任何项目类别），请使用上述默认行为或精确类别名。

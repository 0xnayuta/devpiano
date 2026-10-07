# 已知问题与待验证风险

> 状态：轻量风险清单。详细项目状态以路线图和当前迭代文档为准。
> 更新时机：发现新问题、完成验证、搁置项恢复时。

当前项目状态与风险以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准；阶段验收见 [`../reference/acceptance.md`](../reference/acceptance.md)。

最新审计及软件实施复审见 [AUDIT-004](../audit/AUDIT-004-code-quality-audit-2026-10-02.md)，完整修复记录见 [AUDIT-004 Phase 归档](../archive/audit-004-code-quality-fix-phases.md)，当前任务见 [当前迭代](../roadmap/current-iteration.md)。本清单保留原编号与回归线索，不复制报告第 8 章状态；旧“已修复”遇新反证仍携原身份追踪。

---

## 1. 当前限制与未修复问题

功能缺口和已确认但尚未修复的缺陷。

### 原生演奏快照移调与 WAV 音高不一致（路线 A 已实施；路线 B 重构备忘）

- **优先级：P1，路线 A 修复已合入，实时与离线 WAV 基频已对齐**。
- **历史反证（Task 36-4，2026-10-07）**：快照移调启用且 offset=2 时，同一 Take 经实时 `AudioEngine` 发声音高为 MIDI71 (493.881 Hz)，而离线 WAV 仅发音原始 MIDI69 (440.015 Hz)。
- **路线 A 修复方案（已实施）**：
  - 在 `source/Recording/WavFileExporter.cpp` 与 `source/Recording/PluginOfflineRenderer.cpp` 中引入与 `AudioEngine::renderPlaybackEventsIfNeeded` 严格同构的快照移调处理：
    在消费 `presetChange` 事件时同步更新 `currentTransposeEnabled`、`currentTransposeOffset` 与 `currentFollowKeyMask`；
    在 `NoteOn` 时，依据 `currentTransposeEnabled && channelFollows` 计算 `candidatePitch = jlimit(0, 127, sourceNote + currentTransposeOffset)`，并通过 `identityTracker.noteOn(ch, sourceNote, candidatePitch)` 锁定发音身份；
    在 `NoteOff` 时，严格按 `identityTracker.noteOff(ch, sourceNote)` 锁定的原发音身份发出 NoteOff，保持 Note-off Identity Preservation 铁律；
    离线 WAV 导出与实时音频引擎完全达成 1:1 声学与音高同构（Rendering Parity）。
- **路线 A 的局限性**：
  - 路线 A 解决了实时回放与离线 WAV 导出之间的音高分裂，但未改变录制前置链路：
    当用户通过电脑键盘实时演奏录制时，若全局设置中已开启全局调号移调（`midiTranspose && followKey`），`KeyboardMidiMapper` / `MidiChannelMapper` 在键盘输入端已将音符音高加上了偏移量并写入 Take 的 MIDI 事件中；
    若该 Take 内嵌的快照同时记录了 `transposeEnabled = true`，则回放和离线导出均会在已变换音高上再次叠加一次快照偏移量（实时与离线行为完全一致，但在该叠加场景下音高偏离物理键盘原始键位音高）。
- **后续可能进行的彻底重构方案（路线 B 备忘）**：
  - **做法**：
    将“键盘物理输入”、“录制 Take 时间线”与“输出发声变换”彻底解耦。
    录制事件流中严格仅保存原始未移调的键位音符（Raw Key Note），移调与矩阵路由无论在实时演奏、时间线回放还是离线 WAV 导出阶段，均统一定位为下游单一且幂等的渲染变换层，从根源杜绝二次移调。
  - **风险与影响面（Blast Radius）**：
    需改动 `KeyboardMidiMapper`（输入解绑）、`RecordingEngine`（采集事件定义）、`MidiChannelMapper`（路由分层）以及标准 MIDI 导出（`MidiFileExporter`）；
    标准 MIDI 文件（Type 1 SMF）行业通用语义期望导出的音符为最终发声音高（Sounding Note）而非键盘键位，若录制域存储 Raw Note，导出 SMF 前必须增加音高烘焙（Bake Transposition）阶段；
    牵涉跨模块数据模型改动，回归测试覆盖面广泛。
  - **架构边界**：
    若未来推进路线 B，需严格定义 `.devpiano` 原生事件流与外部标准 MIDI 协议的音高语义契约，保持与标准 DAW 的导入导出兼容性。

### 插件生命周期退出告警

> 既有手工回归不能外推所有厂商插件。AUDIT-004 的 `AUDIT-001 THR-004` 重扫绕过已按 Phase C 收敛：真实原生 VST3、活动 callback＋Editor＋重扫及实际退出通过；基线反证与闭环证据见 [实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)。特定厂商退出告警、永久卡死、强杀和断电仍保留安全回归范围。

详见：[`../reference/features/plugin-hosting.md`](../reference/features/plugin-hosting.md)

### C++ 构建耗时热点：std::regex 重复模板实例化与重型 UI 测试单元

> 通过 `-ftime-trace` 与 `scripts/analyze_build_time.py` 构建微观剖析发现两个主要编译期性能瓶颈：
> 1. **`std::regex` 模板递归膨胀**：`std::basic_regex<char>` 及其内部编译器在多个编译单元中被反复递归实例化，消耗大量 CPU 编译时间；
> 2. **重型 UI 测试编译单元**：`SettingsLayoutModelTest.cpp` 与 `JiveModalDialogTest.cpp` 因集中实例化了复杂的 JIVE 声明式解释器、样式引擎与全部组件工厂，成为业务层编译热点。
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

### 第三方 VST3 插件适配器的框架级并发锁与事件限制

> **现状与分层验收（Phase E 明确边界）**：
> 在 Phase E 中，产品自有发声与调度路径（`BuiltinSynthesiser`、`AudioEngine`、`RealtimeQueue`、`MetronomeProcessor`）已实现完全无锁、零堆分配、全回调零三角函数，并通过消息线程 `dispatchPendingDisplayEvents()` 彻底解耦音频与 UI。
> 但针对第三方 VST3 插件宿主，根据用户批准的分层验收决策，项目保留使用 JUCE 原生 `AudioPluginFormatManager` 适配器（不修改 submodules），该适配器存在以下已知框架级行为：
> 1. `juce_VST3PluginFormatImpl.h` 的 `processBlock()` 仍获取 `SpinLock processMutex`，与消息线程的 `updateMidiMappings()` 共享同一把锁；
> 2. `juce_VST3Common.h` 的 MIDI 事件转换使用带 `CriticalSection` 的 `Array`，且单块存在 2048 条事件上限，超额事件被截断；
> 3. 第三方商业插件内部二进制实现超出宿主控制。
>
> **边界与约束**：产品自有引擎的无锁零分配契约不外推至宿主托管的第三方 VST3 插件；实机物理声卡热插拔仍作为独立硬件测试项保留。

### 最终实机与厂商补验边界

宿主插件在本进程执行；构造、探测或单次 processBlock 永久阻塞时不能安全抢占强杀，创建/探测还可能阻塞消息线程。协作取消只有在该调用返回后才能完成，不承诺有限期限。

原生 VST3 fixture 的 Editor/重扫/同名/重复拖放/offline 与慢任务退出已有直接证据；目标厂商插件、物理声卡密集回放/热插拔、辅助窗口和 IME 全组合以及强杀/断电/OOM 不由此认证。完整剩余矩阵与复审入口见 [当前契约验收](../reference/acceptance.md#audit-004-当前复审入口与契约边界)，不另造审计编号或把未验证范围降级。

---

## 2. 已修复问题（回归参考）

以下问题已修复，保留简要记录用于回归识别。详细根因分析和修复实现见各功能文档。

### 诊断预算、数值与声明式门面（Phase G）

- **修复**：会话日志活动/单备份合计 512 KiB，启动及持续写入有界；UTF-8 超长消息安全限幅，打开/裁剪/轮转失败停用文件 sink 并保留错误，debugger 继续收到完整消息。MIDI 力度直接使用 0..127 原始数值。
- **边界**：业务图标头使用细粒度模块与 BinaryData；热重载及内置 modal 经 ViewHost，删除 raw GuiItem 公共回调/根访问与文案、自造回调、赋值回读 oracle。
- **回归线索**：长会话多次轮转、旧大日志、被锁旧文件或被非空目录阻挡的备份；力度 0/1/64/127；主窗口热重载后保留组件身份；单行输入及确认/取消、Info Notes 保存/取消、真实 transpose/followKey 联动。完整直接证据见 [Phase G 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-g-实施记录与直接验证2026-10-05)。

### 映射看板、交互与声学边界（Phase F）

- **修复**：两张看板共同消费最终映射投影，矩阵输入与观察输出通道分开；绑定标签跨几何重建保持，既有逐键标签/颜色及新绑定编辑保留配置输入索引。静音绑定优先于 Shift 和固定矩阵力度；fade 系数统一收缩并终止 Timer；圆角使用新值立即重建；歌曲 Notes 可键入/保存，取消不提交，诊断列表仍只读；最低 MIDI 八度标签统一。
- **回归线索**：Group/Alt/矩阵/followKey 改动；Ch1→Ch2、Ch2→Ch3 重复鼠标点击和回放后点击；setLayout→setSettings/resize/viewport 后绑定提示与自定义标签/颜色；零力度＋Shift＋矩阵127；fade=1/超范围导入；固定 bounds 的 radius0→30→0；Info Notes 多行确认与取消；MIDI0/1/11/12 卡片和单音 HUD。
- **证据**：Windows Debug 默认回归、实际主窗口/Info/设置/绑定编辑及音频/MIDI/文件消费者见 [Phase F 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)。不外推 IME 全矩阵、真实声卡热插拔或第三方插件的框架实时限制。

### A4 基准音高范围与项目契约不一致

- **修复**：`TemperamentEngine` 范围统一为 **400.0 ~ 480.0 Hz**，默认 440.0 Hz；引擎、设置、预设、Take 快照和内置导出入口共用限幅。原 410.0/450.0 两端差距已消除。
- **回归线索与证据**：实际设置两端可选并提交；400/480 与 415/440/442 的实时 Sine 波形、离线 WAV、设置和预设读取均直接验证，越界请求收敛至对应端点，详见 [Phase F 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-f-实施记录与直接验证2026-10-05)。

### 预设永久身份与实时/离线执行闭包 (Phase E)

- **修复**：预设采用永久 UUID 身份，另存为生成新身份，重命名与自动保存保持身份；当前仅准入 v2 预设，不再执行旧 v1 或名称迁移。原生演奏仅准入 v3，内嵌不可变 `RecordedPreset` 表，按采样偏移同构执行声学快照与 Master/Reverb；内置音源为纯音频所有无锁调度，两预建音色银行平滑切换；全回调闭包达成零库函数三角调用；键盘输入经有界 SPSC 交换，视觉高亮由消息线程刷新，超协商几何安全静音并记录原子计数。
- **回归线索**：预设增删改后回放当前格式演奏；非当前格式拒绝且文件/会话不变；同块预设先于音符生效；实时与内置/VST3 离线 WAV 分段导出一致性；密集 MIDI 播放与未 drain 预设循环无堆增长；UI 线程持有键盘锁时不阻塞音频；超协商尺寸安全静音。
- **证据与边界**：用户批准分层验收；产品自有链路达成零分配、零锁、零库函数三角；真实原生 VST3 的框架观测不在产品自有零锁保证内。Windows Debug 默认测试、真实原生 VST3 与实际窗口快照见 [Phase E 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04)。

### 发音身份与采样级 Transport 边界

- **修复**：播放 FIFO 锁定最终身份，物理同音最后持有者释放；暂停/停止捕获补齐已录音符和踏板，保留显式 MIDI 配对；末尾事件在音频路径交付，设备切率保持 Take 时间域；Seek/回跳先恢复通道状态，完整预备拍在音频下拍开始；柔音由实时/离线乐器拥有者按通道继承。
- **回归线索**：On/Off 之间改 enabled/offset/mask；Q/K、矩阵合并与交错松键；暂停中新演奏与踏板释放；最后 Off 等于 Take 长度；48k↔44.1k、2x、暂停恢复；16 通道 bank/program/CC64/pitch；UI 不轮询就立即 Stop/Play；踏板先于和弦、偷声部与连续 CC67。
- **证据与边界**：AUDIT-004 Phase D 的 Windows Debug 默认门禁、真实音频/MIDI/WAV、原生 VST3 与实际控制器/窗口证据见 [Phase D 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)。设备切率使用生产 release/prepare 的安全 CPU 消费者；不外推声卡热插拔或所有厂商插件。本项只记录 Phase D，后续实时契约和双看板边界分别见 Phase E/F 实施记录。

### 插件/DSP/Transport 所有权与协作导出

- **修复**：重扫与音色重建先关 Editor/停 callback；变速、Seek 与 Stop 由音频块入口一致消费，结构操作与暂停快照有停机边界；离线实例 prepare 前声明 nonRealtime；导出取消只发布请求，实际工作退出后释放一次并回调，应用退出异步等待。
- **身份**：插件选择、加载、持久化与恢复仅消费当前 description identifier；同名不同文件/类型不折叠，重复文件仍可加载并更新 metadata，不再从旧插件名称迁移。
- **回归线索**：Editor 打开时重扫；持续 callback 中 --piano/--sine；缩放取整重播旧 On 或漏 Off；循环被消息线程 Stop 改游标；超过旧超时后取消提前回调/释放；重复拖入报无类型；同名效果被误载为乐器。
- **证据与边界**：`AUDIT-001 THR-004`、`AUDIT-002 THR-001`、`known-issues §2/Phase 6-2 播放速度控制`、`THR-002`、`QUAL-014/015`、`ARCH-002` 的直接程序/资源/窗口与复建输入见 [Phase C 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)。冷路径 GPU 后台句柄单列，不用进程总数增量直接定性业务泄漏；不关闭 Phase D/E 其他契约。


### 测试 fixture 的 NRVO 依赖、用户目录副作用与默认 Chord 漏跑

- **修复**：`AudioEngineTest` 的返回工厂不再携带自引用指针，调用者就地绑定 live buffer；删除默认日志/预设目录探针，文件测试复用 `ScopedTempDir`；Chord 注册为 `DevPiano/Core`，不扩展 runner 白名单。
- **回归线索**：禁可选 NRVO 后渲染失败；默认测试改变真实诊断日志或创建预设目录；Chord 单独补跑通过但默认日志缺失其子测试。
- **关联**：`AUDIT-004:TEST-001`、`AUDIT-004:TEST-002`、原 `AUDIT-002:TEST-014`；Windows Debug 直接验证及可复建配方见 [Phase 0 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-0-实施记录与直接验证2026-10-02)。原审计报告保留基线，不将这些修复外推为其余实时/并发风险已消除。


### 已有文件保护、Take 绑定与设置快照一致性

- **修复**：MIDI/内置及插件 WAV 使用同目录事务替换；预设重命名先确认独立冲突并区分同路径，失败提交恢复源；Take 替换解除旧文件绑定，成功 Save As 绑定新文件；同步设置保存取代旧 timer，深拷贝保留练琴字段和独立 XML；启动恢复预设身份在布局提交前一致。
- **回归线索**：第二次覆盖仍读旧音符/音频；失败任务删原文件；重命名自身后消失；打开 A 后导入 B 的信息编辑改写 A；新插件缓存被旧 timer 回滚；防抖丢失非默认 BPM；恢复 B 后立即编辑绑定未落盘。
- **关联**：AUDIT-004 `ERR-001`、`SEC-001`、`SEC-002`、`ERR-002`、`QUAL-006`、`QUAL-016`；直接验证及复建输入见 [Phase A 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-a-实施记录与直接验证2026-10-02)。此处 `ERR-002` 是同步/防抖顺序问题，不是 §1 的音频几何兜底同名历史编号。
- **边界**：跨文件 rename 若回滚也失败，保留源备份并记录路径；普通事务验证不保证断电/强杀完整性。协作插件生命周期与自有实时/离线闭包分别已有 Phase C/E 证据，不再写成这些阶段尚未实施。

### 原生/MIDI 文件准入与时间线数值安全

- **修复**：解码前校验 JUCE 长度前缀、负载预算/一致性和 MIDI 帧；验证原生采样率、长度/时间戳与整数缩放，稳定规范化乱序但保留同采样顺序；SMF 必须包含全部完整声明轨和合法固定 meta；WAV 在输出前检查最终事件/尾部加法，MIDI 写出检查 tick/VLQ 范围。
- **回归线索**：微小文件触发大分配；极小正率使一块回放结束；MAX 事件产生派生长度 1；未来 Off 排在 On 前面导致播放/seek 漏音；缺第二轨误报尾字节；拍号长度/指数未校验进入框架 accessor。
- **关联**：AUDIT-004 `SEC-003`、`SEC-004`、`SEC-005`、`SEC-006`、`QUAL-005`、`ERR-004`；独立 256 MiB 子进程、真实文件/播放/seek/界面与复建输入见 [Phase B 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-b-实施记录与直接验证2026-10-03)。原始审计发现保留，软件实施复审另列于原报告第 7～8 章。
- **边界**：文件/数值准入已有 Windows 直接证据，仍不外推存储硬件故障；活动所有权、末尾/设备域、实时快照闭包分别见 Phase C/D/E。第三方框架与实机限制见 §1，不用局部文件验收代替整机认证。

### Main.cpp 中残留的 Win32 原生 Hook 与平台特定依赖彻底清理 (PLAT-001)

`source/Main.cpp` 早期引入了 `<windows.h>`，并通过 Win32 API（`SetWindowLongPtrW` 子类化 Hook 顶层窗口的 `WNDPROC` 监听 `WM_SETFOCUS`/`WM_ACTIVATE`，以及通过 `AttachThreadInput` + `SetForegroundWindow`）确保 Windows 环境下的键盘焦点。
修复：彻底废除 `WNDPROC` Hook、全局静态变量与 `<windows.h>` 依赖；统一使用 JUCE 9 原生 `DocumentWindow::activeWindowStatusChanged()` 配合 `juce::MessageManager::callAsync` 延后分发 `restoreKeyboardFocus()`，窗口前台化统一采用 JUCE 标准的 `toFront(true)` 与 `juce::Process::makeForegroundProcess()`，顶层 Shell 达到 100% 纯净标准 JUCE 跨平台实现。

- **回归线索**：Windows 环境下窗口激活/切屏时物理键盘敲击无响应
- **关联**：`source/Main.cpp::MainWindow::scheduleKeyboardFocusRestore()`、`timerCallback()`

### WavExportTask 异步任务化与消除模态循环 (JUCE-002)

`source/Export/WavExportTask.cpp` 早期通过主线程调用 `runDispatchLoopUntil(10)` 与 `Thread::sleep(10)` 驱动导出进度并阻塞主线程，导致 `devpiano` 主应用目标必须开启 `JUCE_MODAL_LOOPS_PERMITTED=1`。
修复：彻底重构为现代化非阻塞异步任务流（`startAsync(onComplete)`），主线程通过回调响应完成或取消；该宏现仅由 CMake 的 `devpiano_tests` 测试目标定义，`devpiano` 主应用目标不定义。

- **回归线索**：导出 WAV 期间主界面卡死或模态循环递归异常
- **关联**：`source/Export/WavExportTask.h/.cpp`，`source/Recording/RecordingSessionController.cpp`

### 运行时数据与配置目录大小写统一 (PLAT-003)

`PluginHost.cpp` 与 `DevPianoLogger.cpp` 早期使用小写 `"devpiano"` 作为应用数据目录，而 `SettingsStore.cpp` 与 `PerformancePreset.cpp` 使用大写 `"DevPiano"`，导致 Linux / macOS 大小写敏感文件系统下用户主目录同时分裂出 `~/.config/DevPiano/` 和 `~/.config/devpiano/` 双目录。
修复：全库统一使用 `"DevPiano"`（PascalCase），彻底根治配置与日志目录分裂。

- **回归线索**：Linux 下配置、崩溃黑名单与日志分散在两个不同目录
- **关联**：`PluginHost.cpp`，`DevPianoLogger.cpp`，`SettingsComponent.cpp`

### 源码 7-bit ASCII 规范化与 LookAndFeel 遗留 AlertWindow 清理 (QUAL-001, QUAL-002)

测试代码中存在个别硬编码中文或 Unicode 字符（如 `→` 箭头），在 Windows/MSVC 环境下有潜在代码页解析异常与 Debug 崩溃断言隐患；`DevPianoLookAndFeel` 残留废弃的 `AlertWindow` 绘制方法与颜色定义。
修复：全库字符串字面量 100% 达到 Strict 7-bit ASCII 铁律，自然语言文案 100% 外部化；彻底删除 `DevPianoLookAndFeel` 中已无调用的 `AlertWindow` 重写方法。

- **回归线索**：Windows/MSVC 构建报告 C4819 警告或 Debug 模式命中 `juce_String.cpp:327` 断言
- **关联**：`StyleCatalogTest.cpp`，`MidiChannelMapperTest.cpp`，`DevPianoLookAndFeel.h/.cpp`

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

**后续所有权闭环（AUDIT-004 Phase C）**：仅把倍率/位置改 atomic 不能保护已经进入 render 的游标与循环标志。当前 setter 只发布命令，音频块入口提交有效速度、位置与待渲染游标；Stop 同边界执行 panic，结构暂停/恢复/清除使用停机守卫。实际双线程 callback 验证 NoteOff 与 A-B 回跳，取整边界不重播旧 On；见 [Phase C 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)，历史原子修复记录保留，不混同后续 Phase D 边界问题。

- **回归线索**：播放中切换速度 → 方向反向 / 悬挂音 / 数据竞争 UB
- **关联**：`RecordingEngine::setPlaybackSpeedMultiplier()`，[`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)

### 虚拟键盘音域标准 88 键收敛（A0~C8）

原虚拟键盘默认硬编码全量 128 键（0~127），导致超出物理大三角钢琴 88 键（MIDI 21~108）的两端琴键（如 F#8 / MIDI 114 等高频音区）在特定音频硬件/分频器或 88 键 VST3 插件下无法正常发声或存在声学盲区。修复：将 `CustomKeyboard` 及 `KeyboardSettings` 默认可用范围严格收敛至真实大三角钢琴的 88 键标准音域（MIDI 21 A0 到 MIDI 108 C8），52 白键 + 36 黑键精确 1:1 对齐，彻底消除两端无效音区与声学陷波盲区。

- **回归线索**：虚拟键盘首尾键分别为 A0(21) 与 C8(108)，点击各音区均发声正常且无多余超声/次声键位
- **关联**：`CustomKeyboard::setAvailableRange(21, 108)`，`KeyboardTypes.h`，`KeyboardHitMappingTest.cpp`

### 空闲延音踏板单声道下混与交感衰减中断

在单声道输出缓冲区（`numChannels == 1`）下，未按键直接踩下延音踏板无动作噪声；当没有按键处于发声状态时，延音踏板激发的交感共鸣池衰减尾音被 `!isVoiceActive()` 守卫条件提前拦截，导致共鸣冲击声提前被硬切断。

- **根因**：`PianoSynthVoice::renderNextBlock` 中单声道渲染路径遗漏了 `pedalSound` 的混入；且无音符发声时未将 `pedalTransient.isActive()` 与交感衰减计入活跃判定。
- **修复**：在单声道路径补齐 `pedalSound` 累加；重构 `isVoiceActive()` 与 `isPedalActive` 守卫，确保只要踏板瞬态或全开放共鸣池尚有能量，渲染循环持续迭代至自然完全衰减。
- **回归线索**：单声道音频设备或独立 voice 踩踏板无声音；踏板冲击尾音被突兀截断。
- **关联**：`PianoSynthVoice.h`，`source/tests/PedalAcousticsTest.cpp`。

### 离键速度动态阻尼跨音符 ADSR 泄漏

在实现基于离键速度的动态阻尼阻音时，`stopNote` 根据速度计算动态释放时间并覆写 `adsrGate.setParameters`。由于 `PianoSynthVoice` 未保存基准 ADSR 配置，且下一次 `startNote` 未重设参数，导致后续所有音符的释放时间被永久改写。

- **根因**：`PianoSynthVoice` 缺乏独立的 `configuredAdsr` 成员存储，动态阻尼覆写了门控实例且缺乏每音符起振时的基准重设。
- **修复**：引入 `configuredAdsr` 保存基准参数，在 `startNote` 起音前强制重新应用基准门控，在 `stopNote` 中仅以基准释放时间为底动态计算当前音符的释放时间。
- **回归线索**：一次高速快离键后，后续所有音符即使配置了长释放也迅速消音。
- **关联**：`PianoSynthVoice.h`，`source/tests/DamperReleaseTest.cpp`。

### 非 ASCII UTF-8 字符显示乱码（最近文件菜单音符图标）

最近文件下拉菜单中 `.mid` / `.devpiano` 文件名前的 ♪ / ♫ 图标显示为 `â™ª` / `â™«` 等乱码。根因：`showRecentFilesMenu()` 用裸 `const char*` 字面量（`"\xe2\x99\xaa"`）构造 `juce::String`，MSVC 按系统代码页（Windows-1252）而非 UTF-8 解读多字节序列。修复：统一用 `juce::String::fromUTF8()` 显式指定 UTF-8 编码，与 `LocaleManager.h` 中非 ASCII 字符串的处理方式一致。

- **回归线索**：最近文件菜单中 `.mid` 文件前出现 `â` 等乱码字符
- **关联**：`MainComponent::showRecentFilesMenu()`，`juce::String::fromUTF8()`，`Locale/LocaleManager.h`

### Performance Preset 导入同名覆盖确认（Phase 16 已解决）

导入同名 `.devpiano.preset` 文件时无确认提示直接覆盖。修复：在 `PresetFlowSupport::handleImportPresetFile()` 中检测目标预设文件是否存在，存在时调用 `JiveModalDialog::launchConfirm` 弹出声明式覆盖确认对话框（`TRANS("Overwrite Preset?")`），用户确认后覆盖，取消则安全放弃。

- **回归线索**：导入同名预设文件直接覆盖而无弹窗提示
- **关联**：`PresetFlowSupport::handleImportPresetFile()`，`JiveModalDialog::launchConfirm`，[`../reference/features/performance-presets.md`](../reference/features/performance-presets.md)

### 虚拟键盘高频 MIDI 播放 CPU 占用（Phase 14 + Phase 16 已解决）

在播放密集 MIDI 文件时，音频 DSP 与 UI 渲染占用大量 CPU。修复：
1. **DSP 层（Phase 14-A）**：`PianoSynthVoice` 采用 Magic Circle 二阶递归正弦振荡器，消除 `std::sin`，音频线程单核 CPU 降至 ~0.7%；
2. **UI 渲染层（Phase 16-A）**：`CustomKeyboard` 引入局部脏矩形重绘（`repaintKey(k)`）与 `g.getClipBounds()` 快速裁剪早退，消灭全量 88 键 `repaint()`，UI 光栅化渲染耗时降低 70% 以上。

- **回归线索**：密集 MIDI 播放时 UI 线程满载 / 虚拟键盘按键残影
- **关联**：`CustomKeyboard::timerCallback()`，`CustomKeyboard::repaintKey()`，[`../reference/features/builtin-piano-synthesis.md`](../reference/features/builtin-piano-synthesis.md)

### 节拍器与播放生命周期及状态机 P1 缺陷修复 (FIX-035)

节拍器在 Tap Tempo 测速边界、Count-in 预备拍倒计时与设备重建时存在时序不一致风险；倒计时期间关闭节拍器或重新触发录制可能产生脏状态。
- **根因**：倒计时未与全局 Transport 会话生命周期联动互斥；Tap Tempo 计算缺少有效时间戳保护。
- **修复**：在 `MetronomeProcessor` 引入原子运行状态机 `RunState`，倒计时期间切换节拍器立即取消倒计时；Tap Tempo 限制最近最多 3 间隔滑动均值并在 2 秒无点击后自动重置；音频块内采样精确混入阻尼正弦脉冲。
- **回归线索**：录音预备拍期间点停止/切换节拍器卡死；Tap Tempo 极值异常。
- **关联**：`MetronomeProcessor.h`，`RecordingSessionController.cpp`，`MetronomeTest.cpp`。

### QWERTY 击键力度与和弦 HUD 计时器竞争及淡出治理

打字演奏击键力度自适应（`TypingCadenceEstimator`）在停止演奏时未正确复位，和弦 HUD 徽章在音符释放后存在淡出计时器空跑与竞争。
- **根因**：击键间隙估算器缺少乐句停顿空闲超时复位；HUD 淡出定时器在完全透明后未停止自身驱动。
- **修复**：`TypingCadenceEstimator` 增加 1.0s 乐句空闲超时平滑复位至基准力度，且 Shift 键强制拉满 1.0f 具备最高优先级；和弦 HUD 在所有音符释放后约 300ms 淡出至完全透明并停止定时器驱动。
- **回归线索**：长时间停顿后首次按键仍被误判为极速击键；和弦释放后 HUD 残留或 UI 定时器高频空转。
- **关联**：`TypingCadenceEstimator.h`，`QwertyComponent.cpp`，`CadenceVelocityTest.cpp`，`ChordRecognitionTest.cpp`。

### MIDI A-B Loop 与 Seek 时间轴跨边界悬挂音防护

时间轴拖拽跳转 Seek 与 A-B 片段循环在回跳瞬间可能丢失 NoteOff 消息，导致跨循环点音符无限延音悬挂。
- **根因**：循环回跳与 Seek 跳转重置播放游标时，未向 16 通道注入 All-Notes-Off 和延音踏板释放。
- **修复**：`AbLoopEngine` 严格执行 `[A, B)` 半开区间有效性校验；`RecordingEngine` 与 `AudioEngine::getNextAudioBlock()` 在 Seek 和循环回跳点采样精确清空 16 个 MIDI 通道的发音与踏板状态。
- **回归线索**：A-B 循环回跳到 A 点后上一轮音符持续鸣响无法停止。
- **关联**：`AbLoopEngine.h`，`RecordingEngine.cpp`，`AudioEngine.cpp`，`RecordingEngineTest.cpp`。

### 最小窗口尺寸下虚拟键盘视口防遮挡 (UI-035)

主窗口在最小支持尺寸（QWERTY 展开 980 × 740，或折叠 980 × 580）下，88 键虚拟键盘的横向滚动条遮挡白键底部，导致低音区点击判定区域收缩。
- **根因**：`KeyboardViewport` 内部滚动条高度未从视口内容有效绘制区域中扣除，白键底部被滚动条覆盖。
- **修复**：重构键盘视口几何布局计算，白键高度完全位于滚动条可视区域上方，并为 QWERTY 展开与折叠提供自适应高度约束。
- **回归线索**：最小窗口尺寸下 88 键钢琴白键下半截被横向滚动条遮挡。
- **关联**：`KeyboardViewport.h`，`LayoutModel.cpp`，`LayoutGoldenTest.cpp`。

### BuiltinSynthesiser 采样块边界 MIDI 双重分发防护 (AUDIT-004-FIX-01)

当 MIDI 事件的采样点恰好等于音频块边界（`samplesToNextMidiMessage == numSamples`）时，该事件在提前处理一次后未步进迭代器，跳出循环后又在后置循环被二次处理。
- **根因**：第一阶段循环在块末尾满足条件后提前调用 `handleMidiMetadata()` 并 `break`，但后置循环对 `<= blockEndSample` 的事件再次执行分发。
- **修复**：删除首阶段边界处冗余的提前分发，音频渲染完成后直接 `break`，所有边界事件统一由后置循环分发，确保严格单次触发。
- **回归线索**：块末尾精确到达的 NoteOn 出现 voice 重启、尾音被掐或误触发偷音。
- **关联**：`BuiltinSynthesiser.cpp`，`AudioEngineTest.cpp`（`BuiltinSynthesiserExecutionTest`）。

### AudioEngine 键释放与 All-Notes-Off 虚假 NoteOn 毛刺消除 (AUDIT-004-FIX-02)

按键释放（NoteOff）或接收到 CC 120/123（All Notes Off）时，UI 键盘状态监听器收到虚假的瞬态 `noteOn` 通知。
- **根因**：原子状态数据在释放时 Bit 7（Active 标志）被清零，但低 7 位保留上次起音力度。消费侧基于 `velocity > 0.0f` 进行起音判定，导致先触发 `noteOn` 再触发 `noteOff`。
- **修复**：严格基于 Bit 7 (`(value & 0x80) != 0`) 门禁起音分支，Inactive 状态仅分发 `noteOff`。
- **回归线索**：UI 键盘监听器在按键释放时收到虚假起音事件流；监听事件序列出现多余的 `handleNoteOn`。
- **关联**：`AudioEngine.cpp`，`AudioEngineTest.cpp`（`AudioEnginePlaybackTransposeTest`）。

### JiveModalDialog 标题栏关闭取消回调失效防护 (AUDIT-004-FIX-03)

模态弹窗通过标题栏 X 按钮或外部程序化退出关闭时，注册的 `options.onCancel` 回调未被触发。
- **根因**：`options.create()` 转移了内容组件所有权并置空 `options.content`，后续 lambda 捕获的 `options.content.get()` 恒为 `nullptr`。
- **修复**：通过 `dialog->getContentComponent()` 获取实际内容指针并包装为 `juce::Component::SafePointer` 传入模态完成回调。
- **回归线索**：点击弹窗右上角 X 按钮关闭后，业务侧临时状态未还原、`onCancel` 丢失。
- **关联**：`JiveModalDialog.cpp`，`JiveModalDialogTest.cpp`。
---

## 3. 环境说明

### 构建与环境

WSL / Windows 镜像构建环境问题见 [`../guides/troubleshooting.md`](../guides/troubleshooting.md)。

Windows MSVC 侧 CMake 缓存未追踪源文件变更可能导致旧目标文件未重新编译，运行时出现 `WeakReference::SharedPointer::get()` 访问冲突。快速修复：删除 `build-win-msvc/CMakeCache.txt` 后重新 `./scripts/dev.sh win-build`。

### WSL 环境 JUCE Files/Writing 单元测试失败

以 root 用户（`uid=0`）在 WSL 中运行单元测试时，`tempFile.setReadOnly(true)` 移除了文件写权限，但 `tempFile.hasWriteAccess()` 因 POSIX `access(path, W_OK)` 对 superuser 始终返回成功而返回 `true`。**不影响任何项目功能**——该测试为 JUCE 自带文件系统验证，项目代码不依赖 `setReadOnly` / `hasWriteAccess`。非 root 用户下该测试自动通过。

- **缓解**：`devpiano_tests` 默认只运行 `DevPiano/` 前缀的项目类别，`Files` 默认跳过，该问题不再触发。仅当显式 `--include-juce --include-files` 运行框架文件测试时才会遇到，非 root 用户或跳过该组合即可；具体类别以注册项与实际默认日志为准。
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
> 修复后 `devpiano_tests` 全量测试通过，零失败。
>
> - **回归线索**：JIVE 文本组件离屏渲染无像素（`light=0`）；`fc-match system-ui` 返回 `.ttc` 路径
> - **关联**：`source/tests/StyleCatalogTest.cpp`（`JiveRenderTest`），`juce_Fonts_freetype.cpp`（`scanFontPaths` / `matchTypeface`）；新装其他 Linux 发行版若 `system-ui` 被映射到 `.ttc` 字体，同样需要此修复

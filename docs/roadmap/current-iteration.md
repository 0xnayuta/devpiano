# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 33：可观测性加固与生产级诊断基础设施 (Observability Hardening & Production-Grade Diagnostics Infrastructure) [进行中]**

*(注：Phase 30 ~ 32 古典调律、空间声学与微观机械拟真三部曲于 2026-09-13 全部胜利完成并归档，包含六大经典律制、A4 基准音高解耦、多视角立体声场、轻量算法混响网络、延音踏板气流啸声与共鸣冲击、离键木质轻撞与毛毡老化扰动，全栈打通 JIVE 面板与国际化。详细完成记录见 [`../archive/phase30-32-temperaments-spatial-mechanics.md`](../archive/phase30-32-temperaments-spatial-mechanics.md)。)*

经过 Phase 1 ~ 32 的持续演进，devpiano 已构建起涵盖 7 大声学系统物理建模钢琴、VST3 插件宿主、MIDI 多轨并轨、高精调律、空间声学与 JIVE 声明式 UI 的完备应用体系。在 AUDIT-001/002 质量治理中，项目代码层已达成生产代码 100% 统一路由至 `DP_LOG_*`、全工程零散落 `std::cout`/`DBG()`，且音频实时线程零锁零 I/O 纪律（ERR-001/ERR-002）。

然而，当前 `source/Diagnostics` 体系在**面向正式分发的桌面生产环境（尤其是 Windows MSVC Win32 GUI 正式发布版）**仍存在关键断层：
1. **Windows 正式分发版用户处于“排障黑盒状态”**：`DevPianoLogger` 当前写死调用 `juce::Logger::outputDebugString`。Linux 终端启动可打印至 `stderr`，但在 Windows 环境下，双击运行的 Win32 GUI 进程默认没有控制台终端，所有错误与告警（声卡初始化失败、VST3 插件加载/扫描崩溃、预设文件损坏回退等）仅发送给系统调试器端口，普通用户无从查看与复制日志，出现异常无法提供证据协助排查；
2. **缺乏应用级日志自动持久化与滚动截断**：缺少业界成熟音频宿主与 JUCE 衍生项目的标准标配——自动将日志写入规范系统数据目录（Windows: `%APPDATA%/devpiano/devpiano.log`，Linux: `~/.local/share/devpiano/devpiano.log`），并配合初始尺寸滚动上限（如 512KB），启动自适应截断历史超额日志，防止长期运行耗尽磁盘；
3. **设置界面诊断卡片欠缺用户直达通道**：当前设置面板的 `diagnostics-card` 仅用于展示静态的音频设备配置对比，用户遇到问题时无法在界面上一键直达日志文件目录；
4. **MidiTrace 协议反序列化与诊断基础设施单测保护缺口**：`source/Diagnostics/MidiTrace.cpp`（`describeMidiMessage`）负责解析 NoteOn/Off、CC、PitchBend、ProgramChange、Aftertouch、SysEx 等全量 MIDI 协议的人类可读诊断输出，但目前尚未在 `source/tests/` 下建立专属纯逻辑单元测试防线。

Phase 33 将对诊断可观测性基础设施进行生产级加固，全面消除用户端排障盲区。

---

## 核心边界与铁律约束 (Boundaries & Iron Rules)

在实施 Phase 33 全阶段开发过程中，必须无条件遵守以下核心边界与铁律：

1. **铁律 1（实时音频线程严格无锁与零 I/O 契约不可动摇）**：
   - 引入文件日志落盘（`juce::FileLogger`）后，底层涉及文件系统调用与 `CriticalSection` 互斥锁；
   - **绝对禁止在音频实时线程（`getNextAudioBlock`、`renderNextBlock`、`startNote` 等）直接或间接调用 `DP_LOG_*` 或 `Logger::writeToLog`**；
   - 实时线程若需上报事件或异常，必须严格延续既有原子计数器/无锁标记机制（如 `playbackEndedPending`、`pluginBufferResizeCount`），由 UI 线程或定时器以低频（如 10Hz~30Hz）在消息线程消费并记录日志。
2. **铁律 2（纯自包含、零外部依赖与日志体积硬限）**：
   - 严禁引入 spdlog、glog 等外部复杂第三方 C++ 日志库，必须基于 JUCE 内建的 `juce::FileLogger` 与 `juce::Logger` 基础设施扩展；
   - 必须配置严格的文件尺寸上限（单文件 512 KB），采用启动自适应滚动截断（`maxInitialFileSizeBytes = 512 * 1024`），防止长期使用占满用户磁盘；
   - 保持绿色便携特性，若特定环境因权限无法创建日志文件，必须优雅降级为纯调试器输出，绝不抛出未捕获异常或阻塞应用主流程。
3. **铁律 3（字符编码与严格 7-bit ASCII）**：
   - 严禁在 C++ 源码（`.cpp` / `.h`，包括单元测试）中书写裸多字节非 ASCII 字符；
   - UI 文本一律使用纯英文键名与 `TRANS()` 宏包装，中文译文统一在 `source/Locale/zh_CN.loc` 中维护；
   - 单元测试中严禁硬编码断言翻译文本。
4. **铁律 4（UI 基础设施接口冻结与布局金标遵从）**：
   - 严格遵守 Phase 28 确立的 UI Infrastructure API Freeze 冻结公约；
   - 诊断卡片新增控件与布局调整严格通过 `ViewHost` 与标准 JIVE ValueTree 模板构建，保护 `LayoutGoldenTest` 与 `SettingsLayoutModelTest` 100% 绿灯。
5. **铁律 5（严格三闸门基线与双平台验证）**：
   - 代码格式合规：`./scripts/dev.sh format --check` 100% 通过；
   - 单元测试全覆盖：`./scripts/dev.sh test` 100% 绿灯（断言数持续递增）；
   - 静态分析零警告：`./scripts/dev.sh tidy` 0 错误 0 警告；
   - Windows MSVC 验证：`./scripts/dev.sh win-build` 构建与测试通过。

---

## 阶段规划详案

### Phase 33-A：双通道生产级日志基础设施 (DevPianoLogger Dual-Sink Engine)

> 目标：将 `DevPianoLogger` 升级为双通道输出（文件持久化 + 调试器即时输出），解决 Windows Win32 GUI 正式分发版用户“排障黑盒”痛点。

- [ ] **Phase 33-A-1：DevPianoLogger 双通道实现与自动文件路径解析**：
  - 重构 `source/Diagnostics/DevPianoLogger.h` 与 `DevPianoLogger.cpp`；
  - 内部持有 `std::unique_ptr<juce::FileLogger> fileLogger`，通过 `juce::FileLogger::createDefaultAppLogger("devpiano", "devpiano.log", ...)` 自动解析操作系统标准日志存储目录（Windows: `%APPDATA%/devpiano/devpiano.log`，Linux: `~/.local/share/devpiano/devpiano.log`）；
  - 配置 `maxInitialFileSizeBytes = 512 * 1024`（512 KB）滚动截断，启动时写入会话起始标记（含时间戳与版本信息）；
  - 重写虚函数 `logMessage(const juce::String& message)` 实现 Dual-Sink 广播：优先由 `fileLogger->logMessage(message)` 写入磁盘，并同步调用 `juce::Logger::outputDebugString(message)` 保证 IDE 调试输出不丢失；
  - 若文件创建失败（如无权限），安全回退纯 `outputDebugString`，杜绝 crash；
  - 提供 `juce::File getLogFile() const` 与 `juce::File getLogDirectory() const` 供 UI 与诊断层查询。
- [ ] **Phase 33-A-2：MainComponent 生命周期与安全注销加固**：
  - 维持 `MainComponent` 构造初期尽早创建并安装 Logger 的机制（`juce::Logger::setCurrentLogger(devPianoLogger.get())`）；
  - 强化析构顺序，确保在 `devPianoLogger` 析构前调用 `juce::Logger::setCurrentLogger(nullptr)`，防止后台线程潜在的悬垂日志调用；
  - 确保 Debug-only 宏（`DP_DEBUG_LOG` / `DP_TRACE_MIDI`）在 Release 下维持 zero-cost 编译期消除。

---

### Phase 33-B：设置界面诊断卡片升级与一键访问日志目录 (UI & Diagnostics Accessibility)

> 目标：在设置界面诊断卡片中新增日志路径展示与“打开日志目录”操作按钮，打通普通用户导出日志的一键直达链路。

- [ ] **Phase 33-B-1：JIVE 声明式布局模板扩展 (SettingsLayoutModel)**：
  - 修改 `source/Settings/jive/SettingsLayoutModel.cpp` 中的 `makeDiagnosticsSectionTree()`；
  - 在现有 `diagnostics-editor` 诊断文本框下方增加操作行 `diagnostics-action-row`；
  - 声明“打开日志目录”操作按钮 `open-log-dir-button` 与日志路径描述标签 `log-path-label`；
  - 保持紧凑的 Flex 响应式排版风格与全局设计语言对齐。
- [ ] **Phase 33-B-2：SettingsComponent 事件绑定与系统文件管理器直达**：
  - 在 `source/Settings/SettingsComponent.cpp` 中安全查找到 `open-log-dir-button` 与 `log-path-label`；
  - 绑定点击事件：获取 `devPianoLogger->getLogDirectory()` 或 `getLogFile()`，调用 `juce::File::revealToUser()` 调起操作系统原生文件管理器（Windows 资源管理器 / Linux 文件管理器）并高亮选中日志文件；
  - 在 `updateDiagnostics()` 中动态刷新日志路径与文件大小展示；
  - 补充 `source/Locale/zh_CN.loc` 中英文本地化词条（"Open Log Folder"、"Log Path: " 等），严格杜绝源码裸中文字面量。
- [ ] **Phase 33-B-3：布局金标测试回归与保护**：
  - 更新并运行 `source/tests/SettingsLayoutModelTest.cpp` 与 `source/tests/LayoutGoldenTest.cpp`，确保新增布局节点符合金标规范。

---

### Phase 33-C：MidiTrace 与诊断体系单元测试防线 (Diagnostic Tests & Regression Coverage)

> 目标：补齐 `describeMidiMessage` 与 `DevPianoLogger` 单元测试，建立确定性的可观测性回归防线。

- [ ] **Phase 33-C-1：MidiTrace 全消息类型纯逻辑单测**：
  - 新增 `source/tests/DiagnosticsTest.cpp`；
  - 覆盖 `devpiano::diagnostics::describeMidiMessage` 对全部主要 MIDI 消息类型的解析与字符串格式化：
    - NoteOn：极端低音（A0）、中央 C（C4）、高音（C8）音名与八度转换精度，力度 0~127 换算；
    - NoteOff：音名、八度与通道；
    - Controller：CC 64 延音踏板、CC 67 弱音踏板、CC 1 调制轮等通道与数值；
    - PitchBend、ProgramChange、ChannelPressure、Aftertouch；
    - SysEx：数据包长度格式化；
    - Fallback：未知原始二进制消息的十六进制 Hex dump 格式化。
- [ ] **Phase 33-C-2：DevPianoLogger 双通道与生命周期单测**：
  - 验证 `DevPianoLogger` 初始化、日志文件生成、`DP_LOG_INFO/WARN/ERROR` 写入、多线程并发写日志安全性与滚动文件尺寸不溢出。

---

### Phase 33-D：双平台构建、三闸门回归与端到端验收 (Cross-Platform Verification & CI Gate)

> 目标：执行代码格式、单元测试、静态检查与 Windows 镜像构建，确保双平台 100% 绿灯交付。

- [ ] **Phase 33-D-1：代码格式与增量静态分析**：
  - `./scripts/dev.sh format --check` 保证 100% 格式对齐；
  - `./scripts/dev.sh tidy` 增量静态检查 0 错误 0 警告。
- [ ] **Phase 33-D-2：单元测试套件全量验证**：
  - `./scripts/dev.sh test` 确保包含新增测试在内的所有测试用例全量通过。
- [ ] **Phase 33-D-3：Windows MSVC 验证构建**：
  - `./scripts/dev.sh win-build` 验证 Windows 平台下文件日志创建、`revealToUser` 与 MSVC 编译链接。

---

## 历史实现 Backlog

- Phase 30 ~ 32 完成记录（古典调律、空间声学与微观机械拟真三部曲）：[`../archive/phase30-32-temperaments-spatial-mechanics.md`](../archive/phase30-32-temperaments-spatial-mechanics.md)
- Phase 29 完成记录（现实物理演奏交互与声学控制）：[`../archive/phase29-physical-voicing-and-acoustic-interaction.md`](../archive/phase29-physical-voicing-and-acoustic-interaction.md)
- Phase 28 完成记录（Devpiano 声明式 UI 基础设施深度治理与接口冻结）：[`../archive/phase28-ui-governance-and-api-freeze.md`](../archive/phase28-ui-governance-and-api-freeze.md)
- Phase 27 完成记录（JUCE 9.0.1 框架升级、UI 基础设施内化与全平台生态演进）：[`../archive/phase27-juce9-upgrade-and-ui-internalization.md`](../archive/phase27-juce9-upgrade-and-ui-internalization.md)
- ADR-014 实施归档（内化 Devpiano UI 基础设施与 JIVE 子模块退役治理）：[`../archive/adr-014-internalize-ui-infrastructure.md`](../archive/adr-014-internalize-ui-infrastructure.md)
- AUDIT-002 修复阶段归档（全量 62 项缺陷修复与质量门禁闭环）：[`../archive/audit-002-code-quality-fix-phases.md`](../archive/audit-002-code-quality-fix-phases.md)
- Phase 26 完成记录（MIDI 多轨并轨与综合时间线合并）：[`../archive/phase26-midi-multi-track-timeline-merge.md`](../archive/phase26-midi-multi-track-timeline-merge.md)
- Phase 25 完成记录（Linux 原生桌面构建与音频驱动适配）：[`../archive/phase25-linux-desktop-and-audio-path.md`](../archive/phase25-linux-desktop-and-audio-path.md)
- Post-v1.0.0 文档体系治理与打包流水线自动化完成记录：[`../guides/release-workflow.md`](../guides/release-workflow.md)
- Phase 24 完成记录（生命力与非线性动力学绽放）：[`../archive/phase24-vitality-and-dynamic-blooming.md`](../archive/phase24-vitality-and-dynamic-blooming.md)
- Phase 23 完成记录（大师级音色校准与 Pianoteq 对齐精调）：[`../archive/phase23-master-voicing-realism-calibration.md`](../archive/phase23-master-voicing-realism-calibration.md)
- Phase 22 完成记录（物理声学极致深化与机械拟真）：[`../archive/phase22-physical-modeling-acoustic-refinement.md`](../archive/phase22-physical-modeling-acoustic-refinement.md)
- Phase 21 完成记录（踏板交感共鸣与琴盖空间声学）：[`../archive/phase21-sympathetic-resonance-lid-acoustics.md`](../archive/phase21-sympathetic-resonance-lid-acoustics.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

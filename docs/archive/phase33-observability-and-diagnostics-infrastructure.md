# Phase 33 完成记录：可观测性加固与生产级诊断基础设施 (Observability Hardening & Production-Grade Diagnostics Infrastructure)

> 归档日期：2026-09-14
> 前序依赖：Phase 30 ~ 32（古典调律、空间声学与微观机械拟真三部曲）
> 实施范围：`source/Diagnostics/`、`source/Settings/`、`source/MainComponent.cpp`、`source/tests/`、`source/Locale/`
> 交付形式：统一分支 `feat/phase33-diagnostics-infrastructure`，Pull Request #16 验收合并

---

## 1. 目标与背景

经过 Phase 1 ~ 32 的持续演进，devpiano 已构建起涵盖 7 大声学系统物理建模钢琴、VST3 插件宿主、MIDI 多轨并轨、高精调律、空间声学与 JIVE 声明式 UI 的完备应用体系。在 AUDIT-001/002 质量治理中，项目代码层已达成生产代码 100% 统一路由至 `DP_LOG_*`、全工程零散落 `std::cout`/`DBG()`，且音频实时线程零锁零 I/O 纪律（ERR-001/ERR-002）。

然而，原 `source/Diagnostics` 体系在**面向正式分发的桌面生产环境（尤其是 Windows MSVC Win32 GUI 正式发布版）**仍存在关键断层：
1. **Windows 正式分发版用户处于“排障黑盒状态”**：`DevPianoLogger` 原写死调用 `juce::Logger::outputDebugString`。Linux 终端启动可打印至 `stderr`，但在 Windows 环境下，双击运行的 Win32 GUI 进程默认没有控制台终端，所有错误与告警（声卡初始化失败、VST3 插件加载/扫描崩溃、预设文件损坏回退等）仅发送给系统调试器端口，普通用户无从查看与复制日志，出现异常无法提供证据协助排查；
2. **缺乏应用级日志自动持久化与滚动截断**：缺少业界成熟音频宿主与 JUCE 衍生项目的标准标配——自动将日志写入规范系统数据目录（Windows: `%APPDATA%/devpiano/devpiano.log`，Linux: `~/.config/devpiano/devpiano.log`），并配合初始尺寸滚动上限（512KB），启动自适应截断历史超额日志，防止长期运行耗尽磁盘；
3. **设置界面诊断卡片欠缺用户直达通道**：原设置面板的 `diagnostics-card` 仅用于展示静态的音频设备配置对比，用户遇到问题时无法在界面上一键直达日志文件目录；
4. **MidiTrace 协议反序列化与诊断基础设施单测保护缺口**：`source/Diagnostics/MidiTrace.cpp`（`describeMidiMessage`）负责解析 NoteOn/Off、CC、PitchBend、ProgramChange、Aftertouch、SysEx 等全量 MIDI 协议的人类可读诊断输出，但之前尚未在 `source/tests/` 下建立专属纯逻辑单元测试防线。

Phase 33 对诊断可观测性基础设施进行了生产级加固，全面消除了用户端排障盲区。

---

## 2. 核心边界与铁律约束 (Boundaries & Iron Rules)

在实施 Phase 33 全阶段开发过程中，严格遵守了以下核心边界与铁律：

1. **铁律 1（实时音频线程严格无锁与零 I/O 契约不可动摇）**：
   - 引入文件日志落盘（`juce::FileLogger`）后，底层涉及文件系统调用与 `CriticalSection` 互斥锁；
   - **绝对禁止在音频实时线程（`getNextAudioBlock`、`renderNextBlock`、`startNote` 等）直接或间接调用 `DP_LOG_*` 或 `Logger::writeToLog`**；
   - 实时线程若需上报事件或异常，严格延续既有原子计数器/无锁标记机制（如 `playbackEndedPending`、`pluginBufferResizeCount`），由 UI 线程或定时器以低频在消息线程消费并记录日志。
2. **铁律 2（纯自包含、零外部依赖与日志体积硬限）**：
   - 严禁引入 spdlog、glog 等外部复杂第三方 C++ 日志库，完全基于 JUCE 内建的 `juce::FileLogger` 与 `juce::Logger` 基础设施扩展；
   - 配置严格的文件尺寸上限（单文件 512 KB），采用启动自适应滚动截断（`maxInitialFileSizeBytes = 512 * 1024`），防止长期使用占满用户磁盘；
   - 保持绿色便携特性，若特定环境因权限无法创建日志文件，优雅降级为纯调试器输出，不抛出未捕获异常或阻塞应用主流程。
3. **铁律 3（字符编码与严格 7-bit ASCII）**：
   - 严禁在 C++ 源码（`.cpp` / `.h`，包括单元测试）中书写裸多字节非 ASCII 字符；
   - UI 文本一律使用纯英文键名与 `TRANS()` 宏包装，中文译文统一在 `source/Locale/zh_CN.loc` 中维护；
   - 单元测试中严禁硬编码断言翻译文本。
4. **铁律 4（UI 基础设施接口冻结与布局金标遵从）**：
   - 严格遵守 Phase 28 确立的 UI Infrastructure API Freeze 冻结公约；
   - 诊断卡片新增控件与布局调整严格通过 `ViewHost` 与标准 JIVE ValueTree 模板构建，保护 `LayoutGoldenTest` 与 `SettingsLayoutModelTest` 100% 绿灯。
5. **铁律 5（严格三闸门基线与双平台验证）**：
   - 代码格式合规：`./scripts/dev.sh format --check` 100% 通过；
   - 单元测试全覆盖：`./scripts/dev.sh test` 100% 绿灯；
   - 静态分析零警告：`./scripts/dev.sh tidy` 0 错误 0 警告；
   - Windows MSVC 验证：`./scripts/dev.sh win-build` 构建与测试通过。

---

## 3. 实施细节 (Phase 33-A ~ Phase 33-D)

### Phase 33-A：双通道生产级日志基础设施 (DevPianoLogger Dual-Sink Engine)

- **Phase 33-A-1：DevPianoLogger 双通道实现与自动文件路径解析** [已完成，2026-09-14]：
  - 重构 `source/Diagnostics/DevPianoLogger.h` 与 `DevPianoLogger.cpp`；
  - 内部持有 `std::unique_ptr<juce::FileLogger> fileLogger`，自动解析操作系统标准日志存储目录（Windows: `%APPDATA%/devpiano/devpiano.log`，Linux: `~/.config/devpiano/devpiano.log`）；
  - 配置 `maxInitialFileSizeBytes = 512LL * 1024`（512 KB）滚动截断，启动时写入会话起始标记；
  - 重写虚函数 `logMessage(const juce::String& message)` 实现 Dual-Sink 广播：优先由 `fileLogger->logMessage(message)` 写入磁盘，并在非 Debug 构建中调用 `juce::Logger::outputDebugString(message)` 保证输出不丢失；
  - 若文件创建失败（如无权限），优雅回退纯 `outputDebugString`，杜绝 crash；
  - 提供 `juce::File getLogFile() const` 与 `juce::File getLogDirectory() const` 供 UI 与诊断层查询；
  - 提供静态辅助方法 `getCurrentDevPianoLogger()` 便于无侵入全局查询当前实例。
- **Phase 33-A-2：MainComponent 生命周期与安全注销加固** [已完成，2026-09-14]：
  - 维持 `MainComponent` 构造初期尽早创建并安装 Logger 的机制（`juce::Logger::setCurrentLogger(devPianoLogger.get())`）；
  - 调整析构顺序，将 `juce::Logger::setCurrentLogger(nullptr)` 移至 `~MainComponent()` 最末尾，确保退出期间的设置保存、插件卸载等关键日志均能完整持久化落盘；
  - `DevPianoLogger` 析构函数中增加安全防护（析构时若自身仍为 active logger 则自动注销置空，防止悬挂引用）；
  - 修复历史遗留的 5 处多字节 em-dash 字符字面量（`juce_String.cpp:327` 断言），彻底消除运行时字符编码断言告警。

### Phase 33-B：设置界面诊断卡片升级与一键访问日志目录 (UI & Diagnostics Accessibility)

- **Phase 33-B-1：JIVE 声明式布局模板扩展 (SettingsLayoutModel)** [已完成，2026-09-14]：
  - 修改 `source/Settings/jive/SettingsLayoutModel.cpp` 中的 `makeDiagnosticsSectionTree()`；
  - 在现有 `diagnostics-editor` 诊断文本框下方增加操作行 `diagnostics-action-row`；
  - 声明“打开日志目录”操作按钮 `open-log-dir-button`（右对齐自适应排版）；
  - 将 `kSettingsLayoutContentHeight` 自适应调整为 1360px，保持全局紧凑响应式排版风格。
- **Phase 33-B-2：SettingsComponent 事件绑定与系统文件管理器直达** [已完成，2026-09-14]：
  - 在 `source/Settings/SettingsComponent.cpp` 中安全查找到 `open-log-dir-button`；
  - 绑定点击事件：获取 `DevPianoLogger::getCurrentDevPianoLogger()` 日志文件，文件存在时调用 `revealToUser()` 调起系统原生文件管理器并高亮选中日志，文件尚不存在时优雅回退父目录 `startAsProcess()`；
  - 在 `updateDiagnostics()` 中动态追加实际日志文件绝对路径及磁盘占用大小（`getSize()`）；
  - 在 `source/Locale/zh_CN.loc` 中补充 `"Open Log Folder" = "打开日志目录"`，严格遵守 ASCII 源码与国际化分层准则。
- **Phase 33-B-3：布局金标测试回归与保护** [已完成，2026-09-14]：
  - 更新并运行 `source/tests/SettingsLayoutModelTest.cpp`（断言 `open-log-dir-button` 存在且为 Button 控件）与 `source/tests/LayoutGoldenTest.cpp`，确保布局 100% 绿灯。

### Phase 33-C：MidiTrace 与诊断体系单元测试防线 (Diagnostic Tests & Regression Coverage)

- **Phase 33-C-1：MidiTrace 全消息类型纯逻辑单测** [已完成，2026-09-14]：
  - 新增 `source/tests/DiagnosticsTest.cpp` 并纳入 `devpiano_tests` 编译目标；
  - 覆盖 `devpiano::diagnostics::describeMidiMessage` 对 NoteOn（A0/C4/C8 八度换算、力度百分比）、NoteOff、Controller（CC 64 延音踏板等）、PitchBend、ProgramChange 等全量协议格式化。
- **Phase 33-C-2：DevPianoLogger 双通道与生命周期单测** [已完成，2026-09-14]：
  - 验证 `DevPianoLogger` 默认路径解析、临时目录下文件落盘、会话启动头标记、`DP_LOG_INFO/WARN/ERROR` 路由与析构时自动解绑防护（ScopedTempDir 沙箱隔离）。

### Phase 33-D：双平台构建、三闸门回归与端到端验收 (Cross-Platform Verification & CI Gate)

- **Phase 33-D-1：代码格式与增量静态分析** [已完成，2026-09-14]：
  - `./scripts/dev.sh format --check` 保证 100% 格式对齐（通过）；
  - `./scripts/dev.sh tidy` 覆盖本轮 12 个变动文件增量静态检查，0 错误 0 警告通过。
- **Phase 33-D-2：单元测试套件全量验证** [已完成，2026-09-14]：
  - `./scripts/dev.sh test` 确保包含新增测试在内的所有测试用例全量通过（602,089 个断言 100% 绿灯）。
- **Phase 33-D-3：Windows MSVC 验证构建** [已完成，2026-09-14]：
  - `./scripts/dev.sh win-build` 增量同步镜像并验证 Windows MSVC 平台下编译与链接成功（0 错误 0 警告）。

---

## 4. 交付清单与文件变动

- 新增文件：
  - `source/tests/DiagnosticsTest.cpp`（MIDI 诊断描述与 DevPianoLogger 双通道单测）
- 核心修改文件：
  - `source/Diagnostics/DevPianoLogger.h` / `DevPianoLogger.cpp`（Dual-Sink 文件落盘与调试器输出）
  - `source/Settings/jive/SettingsLayoutModel.cpp`（添加打开日志目录按钮与高度自适应）
  - `source/Settings/SettingsComponent.cpp`（绑定原生文件管理器直达与日志文件大小展示）
  - `source/Locale/zh_CN.loc`（增补打开日志目录多语言译文）
  - `source/MainComponent.cpp`（安全日志注销生命周期与编码字符清理）
  - `source/tests/SettingsLayoutModelTest.cpp`（按钮控件回归断言）

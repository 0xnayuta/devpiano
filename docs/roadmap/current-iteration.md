# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**AUDIT-003：全面代码质量审计缺陷消除与架构对齐 (Code Quality Remediation & Architecture Alignment) [进行中，2026-09-15 ~]**

*(注：Phase 33 可观测性加固与生产级诊断基础设施于 2026-09-14 全部完成并归档，包含 DevPianoLogger Dual-Sink 双通道落盘与调试器输出、设置界面一键直达系统日志文件夹、全量 MidiTrace 单测与编码字符清理。详细完成记录见 [`../archive/phase33-observability-and-diagnostics-infrastructure.md`](../archive/phase33-observability-and-diagnostics-infrastructure.md)。)*

在 2026-09-15 触发的全面代码质量审计（[`docs/audit/AUDIT-003-code-quality-audit-2026-09-15.md`](../audit/AUDIT-003-code-quality-audit-2026-09-15.md)）中，devpiano 项目获得了 **`A-`** 评级。基线极佳（三闸门全绿、60.2 万断言 100% 通过、Windows MSVC 验证构建 0 错误 0 警告通过）。
根据审计第 8 章登记表与第 5 章修复路线图，本轮专项迭代旨在针对 AUDIT-003 登记的全部 6 项未处理问题（P1×1 / P2×2 / P3×3）开展集中治理，彻底清零未处理缺陷，达成质量全面闭环。

---

## 核心边界与铁律约束 (Boundaries & Iron Rules)

在实施 AUDIT-003 修复的全过程中，必须无条件遵守以下核心边界与铁律：

1. **铁律 1（实时音频线程无锁与零分配契约）**：
   - 在 `PluginOfflineRenderer` 中挂载 `RoomReverbEngine` 时，必须在离线渲染准备期（`prepare(options.sampleRate)`）预分配所有梳状/全通延迟缓冲区；
   - 块渲染音频循环内严格遵循零堆内存分配（No allocation in block rendering loop）。
2. **铁律 2（底层 Core 纯数据单向依赖与零外部业务依赖原则）**：
   - 重构 `source/Core/AppState.h` 时，严格剥离其对上层业务模型 `SettingsModel.h` 与 `ChannelMatrix.h` 的反向包含；
   - 确保 `Core/` 保持最底层纯净，只定义纯数据结构，绝不反向包含 `Settings/`、`Midi/`、`Audio/` 或 `UI/` 代码。
3. **铁律 3（UI 基础设施接口冻结与消息循环安全）**：
   - 修复无头单测中的 Linux socket 溢出告警（`TEST-001`）时，通过 `juce::MessageManager::getInstance()->deliverPendingMessages()` 在单测用例析构/清理时主动泵送并清空事件队列；
   - 严禁侵入修改 JIVE 内化核心源码，严格遵守 Phase 28 UI Infrastructure Freeze 接口冻结公约。
4. **铁律 4（字符编码与严格 7-bit ASCII）**：
   - 严禁在 C++ 源码（`.cpp` / `.h`，包括单元测试）中书写裸多字节非 ASCII 字符；
   - 特殊符号使用 Unicode 转义或十六进制，UI 文本通过 `TRANS()` 外部化维护。
5. **铁律 5（严格三闸门基线与双平台 MSVC 验证）**：
   - 代码格式合规：`./scripts/dev.sh format --check` 100% 通过；
   - 单元测试全覆盖：`./scripts/dev.sh test` 60.2 万断言 100% 绿灯；
   - 静态分析零警告：`./scripts/dev.sh tidy` 0 错误 0 警告；
   - Windows MSVC 验证：`./scripts/dev.sh win-build` 100% 编译链接通过。

---

## 阶段规划详案 (AUDIT-003 Fix Phases)

### AUDIT-003 Phase A：测试消息循环与离线混响对齐 (Test Event Loop & Offline Reverb Parity) [P1 / P2]

> 目标：消除 Linux 无头单测消息套接字溢出断言告警，对齐插件离线导出与实时演奏的房间混响行为。
- [x] **Phase A-1：Linux Headless 单测事件循环泵送与断言消除 (`TEST-001`, P1)** [已完成，2026-09-15]：
  - 在 `source/tests/TestHelpers.h` 引入 `ScopedMessageQueueFlush` 与 `drainMessages()` 辅助函数；
  - 在 `source/tests/TestRunner.cpp` 重写 `shouldAbortTests()` 在每个测试套件前自动执行 1ms 事件循环泵送；
  - 在 `SettingsLayoutModelTest.cpp`、`LayoutGoldenTest.cpp`、`PathEditorReproTest.cpp` 与 `StyleCatalogTest.cpp` 中精准补充 `drainMessages()`；
  - 彻底清空 Linux 内部 `InternalMessageQueue` 套接字管道，`juce_Messaging_linux.cpp:87` 断言告警从 514 处彻底归零。
- [x] **Phase A-2：PluginOfflineRenderer 挂载 RoomReverbEngine 混响网络 (`QUAL-001`, P2)** [已完成，2026-09-15]：
  - 在 `source/Recording/PluginOfflineRenderer.cpp` 中引入 `devpiano::audio::RoomReverbEngine` 实例；
  - 在离线准备期调用 `roomReverb.prepare(options.sampleRate)`、`roomReverb.setSpace(options.reverbSpace)` 与 `roomReverb.setWetLevel(options.reverbWet)`；
  - 在块渲染循环处理完插件 `processBlock` 与通道下混后，当 `options.numChannels >= 2 && options.reverbWet > 1e-4f` 时，在应用增益与软限幅之前执行 `roomReverb.processStereo(...)`；
  - 使 VST3 插件离线导出与内置音源离线导出（`WavFileExporter.cpp:146`）及实时主总线（`AudioEngine.cpp:144-150`）听感与行为 100% 对齐，满足 `docs/reference/features/plugin-offline-rendering.md:78` 契约。
- [x] **Phase A-3：离线混响导出单测与回归验证** [已完成，2026-09-15]：
  - 在 `source/tests/PluginOfflineRendererTest.cpp` 中新增 `testOfflineRenderingWithRoomReverb` 测试用例，断言干音与湿音导出差异及混响尾音扩散能量；
  - 全量运行 `./scripts/dev.sh test`（602,130 断言全绿），验证双平台 MSVC 构建成功。
---

### AUDIT-003 Phase B：底层架构解耦与解码性能微调 (Core Decoupling & Decoder Optimization) [P2 / P3]

> 目标：恢复 Core 基础设施纯数据模型的单向拓扑结构，微调 MIDI 文本双重编码多轮恢复缓冲区分配。

- [x] **Phase B-1：`source/Core/AppState.h` 依赖解耦与单向拓扑恢复 (`ARCH-001`, P2)** [已完成，2026-09-15]：
  - 在 `source/Core/AppState.h` 中独立定义 `BuiltinTone` 枚举，并在 `SettingsModel.h` 中建立 `using BuiltinTone` 别名映射；
  - 移除对 `Settings/SettingsModel.h` 与 `Midi/ChannelMatrix.h` 的反向包含，通过前向声明 `devpiano::midi::ChannelMatrix` 与 `std::shared_ptr` 持有快照；
  - 彻底消除 `Core/` 向上包含上层模块头文件的反向分层破坏，恢复 `Core/` 纯业务数据类型的单向拓扑。
- [x] **Phase B-2：`MidiTextDecoder.cpp` 预分配 scratch buffer 消除重复堆分配 (`PERF-001`, P3)** [已完成，2026-09-15]：
  - 重构 `source/Recording/MidiTextDecoder.cpp` 中的 `extractLegacyBytes` 接受外部目标缓冲区并返回状态；
  - 在 `tryRecoverLegacyDoubleEncoding` 预分配 `current` 与 `scratch` 双缓冲区，多轮恢复中通过 `std::swap` 复用内存；
  - 消除异常双重编码文本恢复循环内的重复堆内存分配与释放。
- [x] **Phase B-3：单向依赖与文本解码回归测试** [已完成，2026-09-15]：
  - 在 `AppStateAndSerializationTest.cpp` 补齐 `midiChannelMatrix` 共享快照断言；
  - 执行 `MidiTextDecoderTest`（12 个子测试）与全量单测（602,135 断言全绿），通过 Windows MSVC 纯净构建验证。

---

### AUDIT-003 Phase C：文档契约同步与双平台全量复验闭环 (Documentation Alignment & Full Verification) [P3]

> 目标：对齐架构与预设规范文档，执行全量双平台验证，闭环 AUDIT-003 所有登记项。

- [ ] **Phase C-1：更新 `docs/reference/architecture.md` 补齐新增模块架构拓扑 (`DOC-001`, P3)**：
  - 在 `docs/reference/architecture.md` 目录树与模块列表中增补 Phase 30~33 新增核心组件：`TemperamentEngine`（古典历史律制）、`PerspectiveProcessor`（双视角空间声像）、`RoomReverbEngine`（房间混响网络）与 `DevPianoLogger`（Dual-Sink 持久化日志）；
  - 更新架构数据流与模块依赖拓扑说明。
- [ ] **Phase C-2：修正 `performance-presets.md` 中 `reverbSpace` 预设枚举描述 (`DOC-002`, P3)**：
  - 将 `docs/reference/features/performance-presets.md:96` 表格中的 `"hall"` 修正为实际序列化与代码匹配的 `"concert_hall"`；
  - 可在 `RoomReverbEngine::fromIdentifier` 兼容 `"hall"` 别名作为额外健壮性容错保护。
- [ ] **Phase C-3：双平台全量构建、三闸门回归与 AUDIT-003 终审关闭**：
  - 执行 `./scripts/dev.sh format --check` 保证 0 差异；
  - 执行 `./scripts/dev.sh test` 保证全量断言通过且无 socket 溢出告警；
  - 执行 `./scripts/dev.sh win-build` 验证 Windows MSVC 纯净构建通过；
  - 同步更新 `docs/audit/AUDIT-003-code-quality-audit-2026-09-15.md` 第 8 章状态为已关闭，并在复审记录中登记。

---

## 历史实现 Backlog

- Phase 33 完成记录（可观测性加固与生产级诊断基础设施）：[`../archive/phase33-observability-and-diagnostics-infrastructure.md`](../archive/phase33-observability-and-diagnostics-infrastructure.md)
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

# AUDIT-001 Fix Phases A–H Completion Record — 代码质量审计修复阶段归档

> 归档日期：2026-08-17
> 来源：[`../roadmap/current-iteration.md`](../roadmap/current-iteration.md) AUDIT Phase A–H
> 审计报告：[`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)（85 项登记：56 项未处理 + 16 项已暂缓 + 13 项已关闭；§8 问题总表为登记来源）
> 后续方向：Phase 12 内置物理建模钢琴音源，详见 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 与 [`../roadmap/current-iteration.md`](../roadmap/current-iteration.md)

---

## 背景与动机

代码质量审计（`AUDIT-001`，2026-08-16）登记 85 项：56 项未处理（3 P1 / 16 P2 / 37 P3）+ 16 项已暂缓 + 13 项已关闭（TEST-011/012/017、THR-001、ERR-001、ENG-001~007、QUAL-014，2026-08-16）。修复按最佳排期分为 **AUDIT Phase A–H** 推进，每 Phase 完成即同步报告第 8 章登记表；16 项已暂缓维持不动（重开条件见报告 §8）。

**最终状态（2026-08-17）**：AUDIT Phase A–H 已全部完成，56 项未处理全关闭；16 项已暂缓经复核关闭 2 项（QUAL-019/PERF-002，复审 12），剩余 14 项维持。

---

## AUDIT Phase A — 实时线程稳定性 [已完成]

> 目标：消除实时音频线程的 2 处 P1（日志 I/O + 数据竞争），为核心路径稳定性打底；Phase D 的 AudioEngineTest 强化将回归验证本 Phase。
> 完成于 2026-08-16：THR-001（masterGain 改 `std::atomic<float>`）+ ERR-001（播放结束日志移至消息线程 `checkPlaybackEnded()`）+ 顺带加固 playbackSampleRateRatio（同类跨线程 double → `std::atomic<double>`）。验证：wsl-build / test（33 类 754 断言全绿）/ format --check / win-build 全通过，详见 AUDIT-001 §7 复审 3。

- [x] `THR-001`：`AudioEngine::masterGain` 改 `std::atomic<float>`（AudioEngine.h:63），消除音频回调（:209 applyGain 读）/消息线程（:232 setMasterGain 写）数据竞争。
- [x] `ERR-001`：`RecordingEngine::advancePlaybackPosition` 播放结束日志移出音频线程——实时线程仅置 `playbackEndedPending` 原子标志，`DP_LOG_INFO` 移至 `RecordingSessionController::checkPlaybackEnded()`（消息线程，:415-417）。

验证：`./scripts/dev.sh wsl-build`、`./scripts/dev.sh test`、`./scripts/dev.sh format --check`、`./scripts/dev.sh win-build`（全部通过，2026-08-16）。

## AUDIT Phase B — 工程化门禁与构建修复 [已完成]

> 目标：恢复三闸门全绿（format 门禁被 18 处违规击穿），建立 clang-tidy 基线，补齐 target_sources 与构建配置。
> 完成于 2026-08-16：format 归零 + pre-commit 钩子、clang-tidy 机械项修复（braces/loop-convert/qualified-auto 清零，bugprone 项 NOLINT 豁免）、MainComponentJiveAccessors 独立 TU、ComboSelection.h 入清单、tests 对齐警告选项（0 warning）、GLOB 去重。验证：format --check / wsl-build / test / win-build 全通过，详见 AUDIT-001 §7 复审 4。

- [x] `ENG-001`：`./scripts/dev.sh format` 批量修复 18 处违规（RenderPipeline 相关 16 处 + AudioDeviceDiagnostics.h + PerformanceFileTest.cpp），复核 `format --check` 归零；将 format 检查接入 pre-commit/CI 防再回归。（已落地：`.githooks/pre-commit` + `core.hooksPath`）
- [x] `ENG-002`：全量运行 `cmake --build build-wsl-clang --target clang-tidy` 建立诊断基线；先批量修机械项（braces / loop-convert / qualified-auto），再处理 `bugprone-easily-swappable-parameters`（MidiChannelMapper 构造参数重排或豁免）。（已落地：**全量 44 文件 0 诊断**——braces 614 经 clang-format InsertBraces 清零；named-parameter/inconsistent-declaration 禁用（--fix 破坏源）；designated-init/math-parentheses/redundant-inline/swappable 4 类存量噪音禁用；enum-size 68（5 个唯一枚举）改 uint8_t；正确性项 analyzer/narrowing/widening 等人工修复；详见 AUDIT-001 §7 复审 5）
- [x] `ENG-003` + `QUAL-014`：`MainComponentJiveAccessors.cpp` 迁移为独立 TU（纳入 target_sources）或改 `.h`，消除 `.cpp` 间 `#include`（MainComponent.cpp:1143，33.2KB）。（已落地：独立 TU + 补 MainComponent.h/Log.h/AdsrCurveComponent.h include）
- [x] `ENG-004`：`ComboSelection.h` 补入主 target_sources（UI 段）。
- [x] `ENG-005`：`devpiano_tests` 添加与主目标一致的 `-Wall -Wextra`（MSVC /W4）。（已落地：+ `juce_recommended_warning_flags`，tests 暴露的 6 处既有警告全部修复，项目代码 0 warning）
- [x] `ENG-006`：clang-tidy `file(GLOB_RECURSE)` 加 `CONFIGURE_DEPENDS`，文件列表按 compile_commands 去重。（已落地：50 唯一文件 / 15 重复记录归并）
- [x] `ENG-007`：删除 `.clang-tidy` 死 CheckOptions（readability-magic-numbers.IgnoredValues）。

验证：`./scripts/dev.sh format --check`（归零）、`./scripts/dev.sh wsl-build`（0 warning）、clang-tidy 目标、`./scripts/dev.sh win-build`（全部通过，2026-08-16）。

## AUDIT Phase C — 核心模块测试补强 [已完成]

> 目标：填补 3 个 P1 覆盖空洞（会话控制 / 通道矩阵 / 预设序列化）与导出、设置、插件操作层（P2），全部纯逻辑、无 GUI/设备依赖，可进 `devpiano_tests`。
> 完成于 2026-08-17：6 个测试文件全部落地（1486 行，断言总数 357 → 1322），**顺带发现并修复 1 个真实持久化 bug**（SettingsStore customKeyLabels/Colours 永远无法从磁盘恢复，见下 TEST-004）。验证：wsl-build 0 warning / test 1322 断言全绿 / format 归零 / 新文件 clang-tidy 0 诊断，详见 AUDIT-001 §7 复审 6。

- [x] `TEST-001`：`RecordingSessionControllerTest`——RecordingSession paused 语义矩阵（recordingPaused/playingPaused 流保持）、ui↔flow 状态映射 round-trip、chooseRecordingFlowCommand 全组合矩阵（record/playPause/stop × 5 状态 × hasTake）、getStateAfterCommand 全命令映射、last-MIDI 导出/导入目录解析（文件→父目录/目录→自身/过期路径→CWD fallback）。（可测性重构：toRecordingFlowState/toRecordingControlsState/makeRecordingFlowStatus 从 .cpp 匿名空间移入 RecordingFlowSupport；getLastMidiExportDirectory/ImportDirectory 移入 ExportFlowSupport；replaceTakeAndStartPlayback 需 MainComponent 不可直接测，其状态转换语义由命令组合测试覆盖）
- [x] `TEST-002`：`MidiChannelMapperTest`——inactive 透传（applyTransform 原样/全局移调在 sendNoteOn 仍生效）、outputChannel 重映射、transpose+octaveShift 边界钳制（0/127）、velocity 覆盖（64=不覆盖）、followKey+midiTranspose 组合、noteOn/Off 对称、非 note 消息透传、输入通道越界钳制、MidiKeyboardState 实际收/放。
- [x] `TEST-003`：`PerformancePresetTest`——全字段 save→load round-trip（含 128 customKeyLabels/Colours、bindings、channelMatrix、keyboard 子集）、sanitisePresetFileName 特殊字符/trim/空→untitled、损坏文件（不存在/空/无效 JSON/非对象/version=2）→ nullopt、扩展名自动补、display name、makeDefaultPreset。
- [x] `TEST-004`：`SettingsStoreTest`——临时 PropertiesFile（Options.folderName 注入）save→load round-trip（含 channelMatrix/labels/colours/knownPluginListXml XML 字段）、corrupted zero-state 恢复默认、scheduleSave 合并语义（SettingsDebounceTimer 公开 + 手动触发 timerCallback）。**发现真实 bug**：readNow 用 `note.isInt()` 判断 ValueTree 属性——fromXml 后属性为 String 类型，isInt() 恒 false → custom key labels/colours 持久化读回永远失效（已修复：`isInt() || isString()`）。
- [x] `TEST-005`：`ExportFlowTest`——buildWavExportOptions 组合（runtime SR 优先/take SR fallback/44100 默认/blockSize≥1/ADSR 透传）、canExportTake 边界、默认导出文件命名、日志前缀、MIDI 导出→读回事件匹配、WAV 导出→读回 header（sampleRate/channels/长度）+ 非零采样验证。
- [x] `TEST-006`：`PluginHostXmlTest`——createKnownPluginListXml→restore round-trip（空列表/手构插件 XML/幂等/垃圾 XML 不崩溃）、PluginPanelStateBuilder 状态映射（fresh host/preferredSelection/isEditorOpen/恢复列表）。PluginOperationController 依赖 MainComponent 不可测，如实记录。

验证：`./scripts/dev.sh test`（1322 断言全绿）、`./scripts/dev.sh format --check`（归零）、wsl-build（0 warning）、win-build（通过，2026-08-17）。

## AUDIT Phase D — 测试机制与回归强化 [已完成]

> 目标：修复 CI 静默丢覆盖（Files 类别跳过、空匹配假绿、类别命名混乱），强化 AudioEngine 断言区分力（验证 Phase A 修复）。
> TEST-011/012 已随 P0 落地，2026-08-16；TEST-010/008/009/007 完成于 2026-08-17。验证：断言总数 1322 → **2914 全绿**（PerformanceFile 套件回归 + 新增），wsl-build 0 warning / format 归零 / clang-tidy 0 诊断 / win-build 通过，详见 AUDIT-001 §7 复审 7。

- [x] `TEST-010`：PerformanceFileTest 改独立类别——`"Files"` → `"DevPiano/Recording"`，.devpiano 持久化回归（4 用例）进入默认运行（断言 1322 → 2914）。已确认默认套件执行并全绿；写盘走系统临时目录，WSL root 下安全。
- [x] `TEST-011`：TestRunner 空匹配/空注册时非零退出并输出实际测试数。（已落地：空匹配 exit=1；另默认只跑项目测试 + `--include-juce`，详见 AUDIT-001 §7 复审记录）
- [x] `TEST-012`：统一测试类别前缀（`DevPiano/Audio`、`DevPiano/Recording`、`DevPiano/UI`），补全 4 个无类别文件，同步修正 known-issues 过滤命令。（已落地：`DevPiano/Core|Recording|Engine|UI`，详见 AUDIT-001 §7 复审记录）
- [x] `TEST-008`：AudioEngineTest 注入按住音符后断言 warmup 块内静音 + warmup 后非零采样（消除"本来无声"假通过）。关键实现细节：`keyboardState.noteOn` + `processNextMidiBuffer(..., injectIndirectEvents=true)` 确定性注入（绕过 wall-clock 依赖的 MidiMessageCollector）；**warmup 期间 `discardWarmupInputState()` 会 reset keyboardState 丢弃输入（设计行为）**，因此注入必须发生在 warmup 结束之后。可测性重构：`calculateWarmupBlockCount`/`calculatePlaybackStartPreRollBlockCount` 从匿名空间提升为 AudioEngine 公开 static 纯函数。
- [x] `TEST-009`：AudioEngine 未覆盖 API——块计数纯函数边界（44100/512→3、48000/256→5、非法参数→1）、setAdsr 极端值钳制 + 输出有限、setPluginHost/setRecordingEngine 接线（null 安全 + 真实 RecordingEngine 实例）。
- [x] `TEST-007`：离屏键盘几何测试（`KeyboardHitMappingTest`）——白键命中（绝对 note 映射）、黑键优先（黑键区命中黑键、下方命中右白键）、范围外 -1、setAvailableRange 收缩命中区、八度滚动不影响命中映射（keys 覆盖全范围，滚动是 Viewport 概念）。可测性重构：`findNoteAt` 从 private 提升 public（纯几何）。**范围调整**：AUDIT 描述的"AdsrCurve 拖拽钳制"不适用——`AdsrCurveComponent` 是纯绘制组件（无鼠标交互），ADSR 钳制在 `AudioEngine::setAdsr`（已由 TEST-009 覆盖），如实记录。

验证：`./scripts/dev.sh test`（2914 断言全绿）、`./scripts/dev.sh format --check`（归零）、wsl-build（0 warning）、win-build（通过，2026-08-17）。

## AUDIT Phase E — 错误处理与失败路径 [已完成]

> 目标：消除实时/后台线程日志 I/O，补齐失败路径可观测性（插件加载、设置落盘、JSON 解析、WAV 导出），清理死 catch 与残留文件。
> 完成于 2026-08-17：ERR-002~015 全部处理（ERR-013 验证已不适用）。验证：wsl-build 0 warning / test 2914 断言全绿 / format 归零 / 改动文件 clang-tidy 0 诊断 / win-build 通过，详见 AUDIT-001 §7 复审 8。

- [x] `ERR-002`：`getNextAudioBlock` 安全网分支（pluginBuffer 实时 resized）`DP_LOG_WARN` 移除——改 `pluginBufferResizeCount` 原子计数，`consumePluginBufferResizeCount()` 由 `MainComponent::timerCallback()` 消息线程消费输出。
- [x] `ERR-003`：`recordEvent` 丢弃日志（`DP_DEBUG_LOG`）移除——丢弃只计入 `droppedEventCount` 原子，`stopRecording()` 已统一输出 dropped 数。
- [x] `ERR-004`：`loadPluginByNameAndCommitState` 检查加载返回值——失败时 `DP_LOG_ERROR`（含 `getLastLoadError()`）+ `finishPluginUiAction(false)` 且**不持久化失败插件名**（否则下次启动反复重试）；`restorePluginByNameOnStartup` 同模式（失败仅日志）。
- [x] `ERR-005`：`SettingsStore::save()`/`writeNow()` 返回 `bool`（`saveIfNeeded()` 结果），失败 `DP_LOG_ERROR`（含文件路径）。
- [x] `ERR-006`：`initialiseUi` 两处（tokens/style）`JSON::parse` 加 `isVoid()` 校验 + 失败 `DP_LOG_ERROR`；`reloadStylesAndTokens` 失败分支补日志（此前 isVoid 校验存在但无日志）。
- [x] `ERR-008`：`WavFileExporter` 失败分支补 `DP_LOG_ERROR`（参数拒绝/目录创建/打开失败/writer 创建/写入失败；取消回调返回 false 不打日志）。
- [x] `ERR-007`：3 处死 `catch(...)`（PerformanceFile ×2、PerformancePreset ×1）改 `JSON::parse(text, result)` Result 重载——`failed()` 时 `DP_LOG_WARN` 含 `getErrorMessage()`，删 catch。
- [x] `ERR-009`：`WavExportTask` 非取消失败路径（插件渲染/sine 渲染）也删除残留目标文件（此前仅取消路径清理）。
- [x] `ERR-010`：`addVst3FileToKnownList` 检查 `addType` 返回值——失败项 WARN，成功日志按实际成功数 + 跳过数计数。
- [x] `ERR-011`：`getPresetDirectory` 检查 `createDirectory` 返回值（失败 WARN）；`PresetFlowSupport` rename 的 `deleteFile` 失败 WARN、delete 的 `deleteFile` 失败时**不打成功日志**（改 WARN）。
- [x] `ERR-012`：`WavExportTask::run()` 补结果日志（成功 `DP_LOG_INFO` / 失败 `DP_LOG_WARN` + errorMessage）——线程内观测点。
- [x] `ERR-013`：验证**已不适用**——`source/tests/` 无 `writeToLog` 直用（仅 TestRunner.cpp:26 的 runner 基础设施，AUDIT 审计时点的 PathEditorReproTest/StyleCatalogTest 直用已不存在）。
- [x] `ERR-014`：PathEditorReproTest:20 与 StyleCatalogTest（shipped style sheet 用例）`JSON::parse` 补 `isVoid()` 校验 + expect + 失败提前 return。
- [x] `ERR-015`：`WavExportTask::run()` 包 try-catch（std::exception + ...）——捕获后 errorMessage + `DP_LOG_ERROR` + 清理残留文件，异常不逸出线程。

验证：`./scripts/dev.sh test`（2914 断言全绿）、`./scripts/dev.sh format --check`（归零）、wsl-build（0 warning）、win-build（通过，2026-08-17）。

## AUDIT Phase F — 死代码与重复清理 [已完成]

> 目标：批量清理死字段/死返回值/重复装配/冗余参数/过期注释（QUAL-014 已在 Phase B 联动处理）。
> 完成于 2026-08-17：QUAL-001~018 全部处理（QUAL-005/011/012 验证为已处理或已不适用）。验证：wsl-build 0 warning / test 全绿 / format 归零 / 改动文件 clang-tidy 0 诊断 / win-build 通过，详见 AUDIT-001 §7 复审 9。

- [x] `QUAL-001`：删除 `InputState::layoutId` 死字段 + AppStateBuilder 2 处赋值 + 因之变空的 `applyRuntimeInputState` 函数。
- [x] `QUAL-002`：`stopInternalPlayback` 改返回 `void`，删除 6 处 `const auto stoppedTake =` 与残留 ignoreUnused，消除大向量拷贝。
- [x] `QUAL-003`：删除 MainComponent.cpp 未使用 include（SettingsSerialization.h）；**PluginFlowSupport.h 保留**（`makePluginRecoverySettings` 实际使用，AUDIT 时点误判）。
- [x] `QUAL-004`：SettingsComponent 提取 `rebuildColourModeCombo`/`rebuildNoteDisplayCombo`/`rebuildKeySignatureCombo`——构造器与 refreshTexts() 复用，消除 38 处 addItem 重复。
- [x] `QUAL-005`：验证**已不适用**——无重复过时注释（AUDIT 时点内容已随此前重构消失）。
- [x] `QUAL-006`：PresetDialogs `complete()` 模式提取为 `DialogContentBase::completeWith()`（防 double-callback + SafePointer UAF 防护），PresetNameContent/PresetConfirmContent 复用。
- [x] `QUAL-007`：提取 `makeKeyboardSettings(view, keySignature)` 共享函数（SettingsModel.h inline）——MainComponent 与 SettingsWindowManager 两处重复装配复用。
- [x] `QUAL-008`：PerformanceFile 提取公共 `parsePerformanceFileRoot`（parse + format 校验），deserialiseTakeFromJson 与 loadPerformanceFileMetadata 复用。
- [x] `QUAL-009`：`chooseNoteRichTrack` 删除未实现 preferredTrack 参数 + `MidiImportOptions::preferTrack` 字段 + "instead of" 日志（同步清理测试）。
- [x] `QUAL-010`：`applyMatrixToNoteOn/Off` 删除未用 `originalChannel` 参数 + 全部调用点（MidiChannelMapper、测试）。
- [x] `QUAL-011`：验证**已随 Phase E 落地**——WavExportTask::run() 已重构为 try-catch + 单一取消路径，死预检查块不存在。
- [x] `QUAL-012`：验证**已接入应用路径**——`captureCurrentState` 读入 preset.keySignature/midiTranspose（:144-145），不删除。
- [x] `QUAL-013`：成员版 `buildCurrentAppStateSnapshot` 改名 `buildAppStateSnapshot`，消除与 core 自由函数同名混淆。
- [x] `QUAL-015`：`sourceToString` 删除冗余 `default:` 分支（枚举穷尽，-Wswitch-enum 保障）。
- [x] `QUAL-016`：删除 4 个 test-only API——`hasDroppedEvents`（测试改用 `getDroppedEventCount`）、`getLastScanFailedFiles`（访问器无消费者；`lastScanFailedFiles` 成员被生产扫描使用故保留）、`makeFullPianoLayout`、`setLowestVisibleNote`（同步清理 KeyMapTypesTest/KeyboardHitMappingTest 用例）。**保留** `NoteRange`/`isValid` 系列（MidiTypes 值类型体系，生产契约）。
- [x] `QUAL-017`：删除 CustomKeyboard.h 过期 Phase 6 开发步骤注释。
- [x] `QUAL-018`：MainComponent adsrCurve 怪 lambda 初始化改直接 `= nullptr`。

验证：`./scripts/dev.sh test`（全绿）、`./scripts/dev.sh format --check`（归零）、wsl-build（0 warning）、win-build（通过，2026-08-17）。

## AUDIT Phase G — 文档与配置契约 [已完成]

> 目标：补齐架构文档缺失章节、WAV 导出缺译，修正 ADR 失效链接（报告 3.10 事实性描述修正项）。
> 完成于 2026-08-17：DOC-001~008 + ADR-001/002 全部处理。验证：wsl-build 0 warning / test 2916 断言全绿 / format 归零 / 12 个改动文件 clang-tidy 0 诊断 / win-build 通过，详见 AUDIT-001 §7 复审 10。

- [x] `DOC-002`：architecture.md 补 Recording/Export/Layout/Diagnostics 四模块章节（Recording 9 文件、Export 3、Layout 2、Diagnostics 3；RenderPipeline、PerformanceFile、WavExportTask、SettingsSerialization 等全部收录）。
- [x] `DOC-003`：architecture.md Plugin 章节更新为已收敛现状（PluginFlowSupport + PluginOperationController 已落地，删"最小可用/待拆分"表述）。
- [x] `DOC-004`：zh_CN.loc.h 补 6 个 WAV 导出字符串译文（含 Phase E 新增的 "Export failed unexpectedly."，比 AUDIT 记录的 5 键多 1）。
- [x] `DOC-001`：architecture.md MainComponent 行数改描述性表述（不再写会漂移的具体数字）。
- [x] `DOC-005`：清理 zh_CN.loc.h 13 个死键（AUDIT 行号清单逐一对上；其余无 TRANS 引用键经核实为 JUCE 内置组件键，保留）。
- [x] `DOC-006`：SettingsModel 扁平键盘显示成员改持有单一 `KeyboardDisplaySettingsView` 实例（消除双默认值；序列化键名不变，兼容）。
- [x] `DOC-007`：style_sheets.json 有 token 对应的色值/字号改 `@token` 引用，StyleCatalog 加载时经 `DesignTokens::resolveToken` 解析（getter 回退、顺序无关）；#window font-size 14 → @font-size-label 消除与 default 13.0 冲突。
- [x] `DOC-008`：修正 LocaleManager.h 注释"project root"→ 实际语义"CWD"。
- [x] ADR 修正：ADR-001 引用更新为 `docs/guides/wsl-windows-msvc-workflow.md` 与 `docs/guides/quickstart.md`；ADR-002 更新为 `docs/reference/architecture.md`（目标存在性已验证）。

验证：链接检查（目标文件存在）、`./scripts/dev.sh test`（2916 断言全绿）、`./scripts/dev.sh format --check`（归零）、win-build 通过。

## AUDIT Phase H — 测试质量余项 [已完成]

> 目标：消除测试脆弱性（单例顺序依赖、CWD 依赖、OS 键盘状态依赖）与断言空洞、CLI 语义缺口。
> 完成于 2026-08-17：TEST-013~020 全部处理（TEST-017 已在复审 2 落地）。验证：wsl-build 0 warning / test 2921 断言全绿 / format 归零 / 12 个改动文件 clang-tidy 0 诊断 / win-build 通过 / CLI 退出码实测，详见 AUDIT-001 §7 复审 11。

- [x] `TEST-013`：StyleCatalog/DesignTokens 提供 `reset()`；StyleCatalogTest/PathEditorReproTest 开头 reset 建立独立基线，消除跨文件执行顺序依赖。
- [x] `TEST-014`：MidiFileImporterTest fixture / StyleCatalogTest findShippedStyleSheet / PathEditorReproTest style sheet 改 `__FILE__` 相对定位（缺失时显式 skip）；CWD/exe 上溯保留为兼容回退。
- [x] `TEST-015`：KeyboardMidiMapper 新增 `setKeyStatePredicate` 可注入键状态谓词（默认真实 OS 查询，生产行为不变）；KeyReleaseTest 注入确定性谓词。
- [x] `TEST-016`：5 处 `expect(true)` 空洞补可观察断言——gain>1.0 钳制（与 gain=1.0 逐样本一致）、requestAllNotesOff（释放尾音衰减后静音）、re-prepare（重备后出声）、搜索路径绝对性、空 XML 恢复语义。
- [x] `TEST-017`：MidiFileImporter velocity-channel 恒真断言拆分。（已落地：复审 2）
- [x] `TEST-018`：hasTake jassert 用例改验证安全查询路径（录制中 `getCurrentTake()` 空 take + capacity 可读）与停止后 `hasTake()` 正确，删除契约违反固化。
- [x] `TEST-019`：exhaustWarmup 硬编码 5 块改 `AudioEngine::calculateWarmupBlockCount`——生产改 warmupSeconds 自动跟随。
- [x] `TEST-020`：TestRunner `--category`/`--name` 互斥报错（EXIT_FAILURE）+ help 文档化优先级与空匹配报错（空匹配报错为 TEST-011 既有落地）。

验证：`./scripts/dev.sh test`（2921 断言全绿）、`./scripts/dev.sh format --check`（归零）。

---

## 暂缓项复核记录（2026-08-17）

16 项已暂缓经复核关闭 2 项（QUAL-019/PERF-002，复审 12），剩余 14 项维持不动（重开条件见报告 §8）。具体复核过程与结论见 [`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md) §7 复审 12 与 §8 登记表。

# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

Phase 11（声明式 UI 架构迁移，JIVE + melatonin_inspector）已全部完成并归档至 [`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)。v0.3.0 发布准备（版本号 / CHANGELOG / Release 构建 / 打包）为并行事项，见 [`../guides/release-workflow.md`](../guides/release-workflow.md)。

代码质量审计（[`AUDIT-001`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)，2026-08-16）登记 85 项：64 项未处理（3 P1 / 18 P2 / 43 P3）+ 16 项已暂缓 + 5 项已关闭（TEST-011/012/017、THR-001、ERR-001，2026-08-16）。修复按最佳排期分为 **AUDIT Phase A–H** 推进，每 Phase 完成即更新本文件状态并同步报告第 8 章登记表；16 项已暂缓维持不动（重开条件见报告 §8）。当前推进：**AUDIT Phase A 已全部完成**。

---

## AUDIT Phase A — 实时线程稳定性 [已完成]

> 目标：消除实时音频线程的 2 处 P1（日志 I/O + 数据竞争），为核心路径稳定性打底；Phase D 的 AudioEngineTest 强化将回归验证本 Phase。
> 完成于 2026-08-16：THR-001（masterGain 改 `std::atomic<float>`）+ ERR-001（播放结束日志移至消息线程 `checkPlaybackEnded()`）+ 顺带加固 playbackSampleRateRatio（同类跨线程 double → `std::atomic<double>`）。验证：wsl-build / test（33 类 754 断言全绿）/ format --check / win-build 全通过，详见 AUDIT-001 §7 复审 3。

- [x] `THR-001`：`AudioEngine::masterGain` 改 `std::atomic<float>`（AudioEngine.h:63），消除音频回调（:209 applyGain 读）/消息线程（:232 setMasterGain 写）数据竞争。
- [x] `ERR-001`：`RecordingEngine::advancePlaybackPosition` 播放结束日志移出音频线程——实时线程仅置 `playbackEndedPending` 原子标志，`DP_LOG_INFO` 移至 `RecordingSessionController::checkPlaybackEnded()`（消息线程，:415-417）。

验证：`./scripts/dev.sh wsl-build`、`./scripts/dev.sh test`、`./scripts/dev.sh format --check`、`./scripts/dev.sh win-build`（全部通过，2026-08-16）。

## AUDIT Phase B — 工程化门禁与构建修复 [未开始]

> 目标：恢复三闸门全绿（format 门禁被 18 处违规击穿），建立 clang-tidy 基线，补齐 target_sources 与构建配置。

- [ ] `ENG-001`：`./scripts/dev.sh format` 批量修复 18 处违规（RenderPipeline 相关 16 处 + AudioDeviceDiagnostics.h + PerformanceFileTest.cpp），复核 `format --check` 归零；将 format 检查接入 pre-commit/CI 防再回归。
- [ ] `ENG-002`：全量运行 `cmake --build build-wsl-clang --target clang-tidy` 建立诊断基线；先批量修机械项（braces / loop-convert / qualified-auto），再处理 `bugprone-easily-swappable-parameters`（MidiChannelMapper 构造参数重排或豁免）。
- [ ] `ENG-003` + `QUAL-014`：`MainComponentJiveAccessors.cpp` 迁移为独立 TU（纳入 target_sources）或改 `.h`，消除 `.cpp` 间 `#include`（MainComponent.cpp:1143，33.2KB）。
- [ ] `ENG-004`：`ComboSelection.h` 补入主 target_sources（UI 段）。
- [ ] `ENG-005`：`devpiano_tests` 添加与主目标一致的 `-Wall -Wextra`（MSVC /W4）。
- [ ] `ENG-006`：clang-tidy `file(GLOB_RECURSE)` 加 `CONFIGURE_DEPENDS`，文件列表按 compile_commands 去重。
- [ ] `ENG-007`：删除 `.clang-tidy` 死 CheckOptions（readability-magic-numbers.IgnoredValues）。

验证：`./scripts/dev.sh format --check`（归零）、`./scripts/dev.sh wsl-build`（0 warning）、clang-tidy 目标、`./scripts/dev.sh win-build`。

## AUDIT Phase C — 核心模块测试补强 [未开始]

> 目标：填补 3 个 P1 覆盖空洞（会话控制 / 通道矩阵 / 预设序列化）与导出、设置、插件操作层（P2），全部纯逻辑、无 GUI/设备依赖，可进 `devpiano_tests`。

- [ ] `TEST-001`：`RecordingSessionControllerTest`——`getLastMidiExportDirectory`（.cpp:52-64）、`toRecordingFlowState`/`makeRecordingFlowStatus` 组合、`RecordingSession::isRecording/isPlaying` paused 语义（.h:36-50）、`replaceTakeAndStartPlayback` 流程（FileChooser 注入桩）。
- [ ] `TEST-002`：`MidiChannelMapperTest`——matrix.active=false 透传、`applyMatrixToNoteOn/Off` 通道选择、transpose 边界钳制、followKey+midiTranspose 组合、noteOn/Off 对称变换。
- [ ] `TEST-003`：`PerformancePresetTest`——save→load round-trip（含 128 项 customKeyLabels/Colours）、`sanitisePresetFileName` 特殊字符、损坏文件返回 nullopt、formatVersion 校验。
- [ ] `TEST-004`：SettingsStore 测试——临时 PropertiesFile round-trip + scheduleSave 合并/延迟语义（timer 注入）。
- [ ] `TEST-005`：导出链纯逻辑测试——`buildWavExportOptions` 参数组合、`canExportTake` 边界、WAV/MIDI 头 round-trip 读回。
- [ ] `TEST-006`：PluginHost XML round-trip（createKnownPluginListXml→restore）与 PluginPanelStateBuilder 测试。

验证：`./scripts/dev.sh test`（新增测试进 `devpiano_tests`）、`./scripts/dev.sh format --check`。

## AUDIT Phase D — 测试机制与回归强化 [进行中]

> 目标：修复 CI 静默丢覆盖（Files 类别跳过、空匹配假绿、类别命名混乱），强化 AudioEngine 断言区分力（验证 Phase A 修复）。
> TEST-011/012 已随 P0（TestRunner 白名单 + 类别统一）落地，2026-08-16；余下 TEST-010/008/009/007 未开始。

- [ ] `TEST-010`：PerformanceFileTest 改独立类别（如 `DevPiano/Files`）或 TestRunner 增加精确文件过滤，使 `.devpiano` 持久化回归进入默认运行。
- [x] `TEST-011`：TestRunner 空匹配/空注册时非零退出并输出实际测试数。（已落地：空匹配 exit=1；另默认只跑项目测试 + `--include-juce`，详见 AUDIT-001 §7 复审记录）
- [x] `TEST-012`：统一测试类别前缀（`DevPiano/Audio`、`DevPiano/Recording`、`DevPiano/UI`），补全 4 个无类别文件，同步修正 known-issues 过滤命令。（已落地：`DevPiano/Core|Recording|Engine|UI`，详见 AUDIT-001 §7 复审记录）
- [ ] `TEST-008`：AudioEngineTest 注入 noteOn 后断言 warmup 块内静音、warmup 后非零采样（消除"本来无声"假通过；依赖 Phase A 完成）。
- [ ] `TEST-009`：AudioEngine 未覆盖 API——setAdsr、armPlaybackStartPreRoll 块计数纯函数、setPluginHost/setRecordingEngine 接线。
- [ ] `TEST-007`：离屏渲染测试 CustomKeyboard 命中映射/八度切换与 AdsrCurve 拖拽钳制。

验证：`./scripts/dev.sh test`、`./scripts/dev.sh format --check`。

## AUDIT Phase E — 错误处理与失败路径 [未开始]

> 目标：消除实时/后台线程日志 I/O，补齐失败路径可观测性（插件加载、设置落盘、JSON 解析、WAV 导出），清理死 catch 与残留文件。

- [ ] `ERR-002`：`getNextAudioBlock` 安全网分支（AudioEngine.cpp:188）移除 `DP_LOG_WARN`，改计数/标志由消息线程输出。
- [ ] `ERR-003`：`recordEvent` 丢弃日志（RecordingEngine.cpp:148）移 `stopRecording()` 消息线程统一输出。
- [ ] `ERR-004`：`loadPluginByNameAndCommitState` 检查加载返回值——失败时走 `finishPluginUiAction(false)` 且不持久化失败插件名；`restorePluginByNameOnStartup` 同模式。
- [ ] `ERR-005`：`SettingsStore::save()/writeNow()` 返回 `bool`/`juce::Result`，失败时 `DP_LOG_ERROR`（含文件路径）。
- [ ] `ERR-006`：`initialiseUi`/`reloadStylesAndTokens` 的 `JSON::parse` 结果加 `isVoid()` 校验 + 失败 `DP_LOG_ERROR`。
- [ ] `ERR-008`：`WavFileExporter` 各失败分支补 `DP_LOG_ERROR`（对齐 PluginOfflineRenderer 日志粒度）。
- [ ] `ERR-007`：移除 3 处死 `catch(...)`（本版本 JUCE JSON::parse 不抛异常，juce_JSON.cpp:552-559），或改用 `JSON::parse(text, result)` Result 重载并记录行:列。
- [ ] `ERR-009`：`WavExportTask` 非取消失败路径删除残留目标文件（或整体改 TemporaryFile + rename）。
- [ ] `ERR-010`：`addVst3FileToKnownList` 检查 `addType` 返回值，失败项 WARN 并按实际成功数计数。
- [ ] `ERR-011`：`getPresetDirectory`/`PresetFlowSupport` 检查 createDirectory/deleteFile 返回值；rename/delete 失败不打成功日志。
- [ ] `ERR-012`：修正 `WavExportTask.cpp:45` 注释与日志不一致（或补日志）。
- [ ] `ERR-013`：测试代码改用 DP_LOG_* 宏（PathEditorReproTest/StyleCatalogTest 直用 writeToLog 处）。
- [ ] `ERR-014`：测试中 JSON::parse 结果补 isVoid 校验并 expect（PathEditorReproTest:20、StyleCatalogTest:784）。
- [ ] `ERR-015`：`WavExportTask::run()` 内包 try-catch，捕获后设置 errorMessage + DP_LOG_ERROR + 清理部分文件。

验证：`./scripts/dev.sh test`、`./scripts/dev.sh format --check`、`./scripts/dev.sh win-build`。

## AUDIT Phase F — 死代码与重复清理 [未开始]

> 目标：批量清理死字段/死返回值/重复装配/冗余参数/过期注释（QUAL-014 已在 Phase B 联动处理）。

- [ ] `QUAL-001`：删除 `InputState::layoutId` 死字段与 `AppStateBuilder.h:83` 的 `lastActivePresetId` 误赋值。
- [ ] `QUAL-002`：`stopInternalPlayback` 改返回 void，删除 7 处 ignoreUnused 与大向量拷贝。
- [ ] `QUAL-003`：删除 MainComponent.cpp:4-5 未使用 include。
- [ ] `QUAL-004`：SettingsComponent.h ComboBox item 装配提取 `rebuildComboItems()` 供构造器与 refreshTexts() 复用。
- [ ] `QUAL-005`：合并 SettingsComponent.h 重复过时注释与拆段配置。
- [ ] `QUAL-006`：PresetDialogs.cpp `complete()` 模式上提到 `DialogContentBase`。
- [ ] `QUAL-007`：提取 `makeKeyboardSettings(view, keySignature)` 共享函数消除跨文件重复装配。
- [ ] `QUAL-008`：PerformanceFile 提取公共 `parsePerformanceFileRoot` 复用 metadata/事件解析。
- [ ] `QUAL-009`：`chooseNoteRichTrack` 实现 preferredTrack 平局语义或删除参数与"instead of"日志。
- [ ] `QUAL-010`：删除 `applyMatrixToNoteOn/Off` 未用 `originalChannel` 参数。
- [ ] `QUAL-011`：删除 `WavExportTask.cpp:45-47` 死预检查块（保留单一取消路径）。
- [ ] `QUAL-012`：PerformancePreset 移除 keySignature/midiTranspose 死配置字段（或补应用路径）。
- [ ] `QUAL-013`：成员版 `buildCurrentAppStateSnapshot` 改名消除与 core 自由函数同名混淆。
- [ ] `QUAL-015`：`sourceToString` 删除冗余 `default:` 分支（枚举已穷尽）。
- [ ] `QUAL-016`：逐个决策 test-only API 面（hasDroppedEvents/getLastScanFailedFiles/setLowestVisibleNote/makeFullPianoLayout/NoteRange/isValid 系列）——接入生产或删除并清理测试。
- [ ] `QUAL-017`：删除 CustomKeyboard.h 过期 Phase 6 开发步骤注释。
- [ ] `QUAL-018`：`MainComponent.cpp` adsrCurve 怪 lambda 初始化改直接 `= nullptr`。

验证：`./scripts/dev.sh wsl-build`、`./scripts/dev.sh test`、`./scripts/dev.sh format --check`。

## AUDIT Phase G — 文档与配置契约 [未开始]

> 目标：补齐架构文档缺失章节、WAV 导出缺译，修正 ADR 失效链接（报告 3.10 事实性描述修正项）。

- [ ] `DOC-002`：architecture.md 补 Recording/Export/Layout/Diagnostics 四模块章节（含 RenderPipeline、WavExportTask、SettingsSerialization 等新文件）。
- [ ] `DOC-003`：architecture.md Plugin 章节更新为已收敛现状（PluginFlowSupport + PluginOperationController 已落地）。
- [ ] `DOC-004`：zh_CN.loc.h 补 5 个 WAV 导出字符串译文（Exporting.../Export cancelled./Export failed during plugin/sine rendering./Export complete.）。
- [ ] `DOC-001`：architecture.md 更新 MainComponent 实际行数（1143）或改描述性表述。
- [ ] `DOC-005`：清理 zh_CN.loc.h 13 个死键。
- [ ] `DOC-006`：SettingsModel 扁平成员改持有单一 KeyboardDisplaySettingsView 实例（消除双默认值）。
- [ ] `DOC-007`：style_sheets.json 硬编码色值改引用 DesignTokens（消除双事实源）。
- [ ] `DOC-008`：修正 LocaleManager.h 头注释与实际搜索目录不符。
- [ ] ADR 修正：ADR-001 引用链接更新为 `docs/guides/wsl-windows-msvc-workflow.md` 与 `docs/guides/quickstart.md`；ADR-002 引用更新为 `docs/reference/architecture.md`。

验证：链接检查、`./scripts/dev.sh test`、`./scripts/dev.sh format --check`。

## AUDIT Phase H — 测试质量余项 [未开始]

> 目标：消除测试脆弱性（单例顺序依赖、CWD 依赖、OS 键盘状态依赖）与断言空洞、CLI 语义缺口。

- [ ] `TEST-013`：StyleCatalog/DesignTokens 提供 reset()，消除跨文件执行顺序依赖。
- [ ] `TEST-014`：测试 fixture/样式文件改为 `__FILE__` 相对定位或缺失时显式 skip。
- [ ] `TEST-015`：键盘状态查询抽象为可注入谓词，消除 OS 键盘依赖。
- [ ] `TEST-016`：AudioEngineTest/PluginHostTest 的 `expect(true)` 空洞断言补可观察结果校验。
- [x] `TEST-017`：MidiFileImporter velocity-channel 恒真断言拆分为独立可证伪断言。（已落地：`expect(foundVaryingVelocity)` 与 `expect(foundNonDefaultChannel)` 两条独立断言，详见 AUDIT-001 §7 复审 2）
- [ ] `TEST-018`：hasTake jassert 用例改为验证 RecordingSession 副本语义（Debug/Release 双配置 CI）。
- [ ] `TEST-019`：warmup 块数 magic number 改引用生产常量/注释说明。
- [ ] `TEST-020`：TestRunner --category/--name 冲突参数报错或文档化优先级。

验证：`./scripts/dev.sh test`、`./scripts/dev.sh format --check`。

---

## 验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

涉及环境或路径问题时执行：

```bash
./scripts/dev.sh self-check
```

## 相关文档

- 项目路线图：[`roadmap.md`](roadmap.md)
- 审计报告：[`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)（§8 问题总表 64 项未处理 + 16 项已暂缓追踪；§5 修复路线图为排期来源）
- 架构概览：[`../reference/architecture.md`](../reference/architecture.md)
- Phase 11 归档：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

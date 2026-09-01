# AUDIT-002 代码质量审计修复任务全量归档记录

> 归档时间：2026-09-01  
> 来源迭代：`docs/roadmap/current-iteration.md` (AUDIT-002 Fix Phases A–H)  
> 关联审计报告：[`docs/audit/AUDIT-002-code-quality-audit-2026-08-31.md`](../audit/AUDIT-002-code-quality-audit-2026-08-31.md)

---

## 阶段概述与目标达成

在完成 Phase 26（MIDI 多轨并轨与综合时间线合并）并发布 v1.0.1 后，依据项目质量门禁规范，对 `source/` 全量代码执行了代码质量审计（AUDIT-002）。
本轮迭代专项目标为**全面消化 AUDIT-002 登记的 62 项未处理问题（6 P1 / 13 P2 / 43 P3）**，按风险与模块依赖划分为 **AUDIT-002 Fix Phases A–H** 8 个子阶段，现已全量完成并通过门禁复验。

---

## 各子阶段完成清单（Fix Phases A–H）

### AUDIT-002 Phase A：实时线程与内存安全（P1 紧急缺陷修复） [已完成，2026-08-31]
> 目标：消除实时音频线程数据竞争与高频触发的内存 UAF / 崩溃漏洞，建立第一道安全防线。

- [x] `THR-001`：`AudioEngine` 参数更新（`setAdsr` / `setPianoParameters`）与音频渲染同步，消除锁外修改活跃 voice 状态的数据竞争
- [x] `THR-002`：WAV 导出 Phase 1 插件状态快照包进 `runPluginActionWithAudioDeviceRebuild`，严格遵守 `PluginHost` 线程契约
- [x] `SEC-001`：`SettingsComponent` 拆树前先置空 `Viewport::contentComp`，树重建延后 `callAsync`，根除语言切换/窗口关闭时的 Viewport 悬挂指针 UAF
- [x] `QUAL-001`：`MainComponent` 拖放 `.devpiano.preset` 扩展名判断改用 `getFileName().endsWithIgnoreCase(".devpiano.preset")`，修复预设拖放导入静默失效
- [x] `ERR-001`：`WavExportTask` 进度对话框关闭路径触发取消信号并置空 `activeDialog`，杜绝渲染期间点 X 导致的悬挂 UAF

### AUDIT-002 Phase B：架构重构与组件收敛（P2 结构化治理） [已完成，2026-08-31]
> 目标：解决装配层膨胀与庞大内联头文件，消除跨文件同构辅助代码复制。

- [x] `ARCH-002`：`SettingsComponent` 拆分为 `.h` 声明与 `.cpp` 实现，消除 759 行全内联头文件并按功能域拆解 `buildJiveUi`
- [x] `ARCH-001`：`MainComponent` 绑定编辑合并与预设自动落盘业务逻辑下沉至 `KeyboardMidiMapper` / `PresetFlowSupport`
- [x] `QUAL-002`：提取通用 JIVE 布局构建辅助头（`source/UI/jive/JiveBuilderHelpers.h`），消除 4 个文件的同构代码复制
- [x] `QUAL-007`：`MainComponent` 拆树逻辑复用 `JiveUtils.h` 实现，消除匿名命名空间冗余副本

### AUDIT-002 Phase C：核心编排与测试盲区补强（P1/P2 测试与状态机加固） [已完成，2026-08-31]
> 目标：填补插件操作控制器、导出后台任务、离线渲染器、正弦音源与状态构建的测试空白。

- [x] `TEST-001`：`PluginOperationController` 编排状态机测试（抽取纯函数决策层 + 异步提交顺序测试，P1）
- [x] `TEST-002`：`WavExportTask` 后台任务本体成功/取消/失败三分支 smoke 测试
- [x] `TEST-003`：`PluginOfflineRenderer` 无插件直调与结束静音安全测试
- [x] `TEST-004`：`SineSynthVoice` 确定性渲染与 ADSR/频率精度回归测试
- [x] `TEST-005`：`AppStateBuilder` 与 `SettingsSerialization` 纯函数 round-trip + 损坏输入测试
- [x] `TEST-006`：`PresetFlowSupport` 编排与预设 ID 缓存一致性测试
- [x] `TEST-007`：`PerformanceFileTest` 迁移至 `ScopedTempDir`，消除固定文件名并行与残留风险

### AUDIT-002 Phase D：音频/录制/导出管线质量加固（P2 管道缺陷与性能优化） [已完成，2026-08-31]
> 目标：消除音频回调堆分配，修正 MIDI 导出伪 SysEx 与通道/时间戳逻辑。

- [x] `PERF-001`：`AudioEngine` 回放移调路径消除每块堆分配，就地改写或复用预分配 buffer
- [x] `SEC-002`：`MidiFileExporter` 过滤非 MIDI 事件，`stopRecording` 合并 `pendingPresetEvents` 后按时间戳排序
- [x] `ERR-002`：`getCustomKeyboard` 增加判空降级防护，杜绝 Release 下空指针解引用
- [x] `ERR-003`：`KeyBindingEditDialog` Unbind 路径复用统一完成路径，保证单次回调契约
- [x] `QUAL-004`：插件离线渲染实现真实 down-mix / 显式告警，两导出路径软限幅行为对齐
- [x] `QUAL-006`：`MidiTrackMergeEngine` 负时间戳检查前移，全 t=0 take 长度对齐导出语义
- [x] `QUAL-005`：确认并清理生产链不可达的 `singleTrackOnly` 遗留分支

### AUDIT-002 Phase E：安全防御与输入边界加固（P3 健壮性与健壮序列化） [已完成，2026-08-31]
> 目标：强化用户可控文件大小校验、数值合法域 clamp、异常输入容错。

- [x] `SEC-003`：预设与 locale 文件读取前校验大小上限
- [x] `SEC-004`：`loadPreset` 版本号兼容前向扩展（`<= performancePresetFormatVersion`）并逐字段默认值填充
- [x] `SEC-005`：`SettingsSerialization` 与 `SettingsStore` 数值加载后 clamp 到合法域
- [x] `SEC-006`：`MidiTrackMergeEngine` 时间戳转换收敛为安全 `clampToInt64`
- [x] `SEC-007`：`SettingsStore::file()` 静默回退路径补 `jassert` 并在启动尽早安装 logger
- [x] `ERR-004`：清理 `WavExportTask` 死成员并更新 `WavFileExporter.h` 过期注释
- [x] `OBS-001`：`initialiseFromPreset` 失败路径补 `DP_LOG_WARN`（包含具体路径）

### AUDIT-002 Phase F：性能优化、资源管理与质量小项（P3 细节优化） [已完成，2026-08-31]
> 目标：提升执行效率、避免长会话资源累积、清理历史残留与样板代码。

- [x] `PERF-002`：回放渲染改用块游标扫描（复用 `WavFileExporter` 模式）
- [x] `PERF-003`：MIDI 导入移至后台线程或增加事件上限，避免大文件冻结 UI
- [x] `PERF-004`：master limiter 超阈 soft-knee 限幅优化多项式近似并注释设计取舍
- [x] `PERF-005`：预设目录扫描引入修改时间缓存机制
- [x] `RES-001`：Take 数据以 move 或 `std::shared_ptr<const RecordingTake>` 语义传递，降低内存峰值
- [x] `RES-002`：`StyleCatalog` 样式对象复用或按树生命周期释放
- [x] `QUAL-003`：`ChannelMatrix::active` 补齐检查逻辑或清理死字段与注释
- [x] `QUAL-008`：`refreshTitles` 重复条目去重，`MainComponentJiveAccessors` 样板提取模板辅助
- [x] `QUAL-009`：`getBuiltinToneFromUi` 改名为 `getBuiltinToneFromSettings` 对齐实际语义
- [x] `QUAL-010`：CJK 字体候选链统一收敛至 `DesignTokens`
- [x] `QUAL-011`：清理代码内滞留的历史 Phase 重构注释
- [x] `QUAL-012`：`SettingsComponent` toggle 仅保留单一 `onStateChange` 写路径
- [x] `QUAL-013`：`MidiTypes.h` 补充显式细粒度 JUCE include
- [x] `QUAL-014`：测试侧 `findNodeById` 副本清理并改用生产 helper
- [x] `QUAL-015`：`RecordingFlow` 状态机两份重复测试归并

### AUDIT-002 Phase G：测试基础设施与工程化合规（P3 门禁与环境健壮性） [已完成，2026-08-31]
> 目标：消除测试假绿与环境脆弱性、满足 ADR 决策与工程纪律。

- [x] `TEST-008`~`TEST-016`：测试断言可观察化（`KeyboardHitMapping`）、补全失败上下文（`StyleCatalog`）、补静音断言（`AudioEngine`）、像素断言对比化、全局 tokens/L&F/locale 变更 RAII 守卫还原、`TestRunner` 类别白名单前缀匹配与 `--verbose` 参数处理
- [x] `ENG-001`：多实例启动保护与参数转发/文件锁机制
- [x] `ENG-002`：设置窗口内容高度从布局树动态计算，消除 960 魔法数
- [x] `CMPL-001`：`TestHelpers.h` 迁移至细粒度包含，消除 ADR-012 字面违规
- [x] `THR-003`：`MidiKeyboardState` 监听器回调消息线程契约文档化

### AUDIT-002 Phase H：文档体系治理与全量复验（P3 文档与全套门禁闭环） [已完成，2026-08-31]
> 目标：文档与架构对齐、全量构建与三闸门及 Windows 验证闭环。

- [x] `DOC-001`：`docs/reference/architecture.md` 增补 `MidiTrackMergeEngine` 模块章节与管线架构
- [x] `DOC-002`：`docs/roadmap/roadmap.md` 风险表更新 `MainComponent` 实际行数描述
- [x] `DOC-003`：`docs/reference/project-scope.md` 澄清多轨并轨导入与 DAW 工作站的边界定义
- [x] `DOC-004`：`RecordingEngine.h` `smoothedPitchBend` 注释对齐实现
- [x] 三闸门与全量静态分析复验（`wsl-build` / `test` / `format --check` / `win-build` / `clang-tidy --all`）
- [x] 全量同步并签署 `AUDIT-002` 第 8 章问题总表与第 7 章复审记录

# devpiano 阶段验收标准

> 用途：定义各阶段的可验证完成标准与全量回归清单。
> 更新时机：阶段验收标准变化、新里程碑完成或测试基线更新时。

说明：本文件描述阶段验收标准与回归清单。项目状态与路线图以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准。

当前消费者标准见下方 AUDIT-004 复审入口；其余 Phase 与发布章节是对应时期的交付记录，不是当前全部平台、硬件或厂商插件的认证。历史 Phase 35 勾选保持原貌，最新反证与修复证据另列。

## AUDIT-004 当前复审入口与契约边界

全量原项身份、原优先级、逐项证据和可复建输入见 [Phase H 实施归档](../archive/audit-004-code-quality-fix-phases.md#phase-h-实施记录与最终集成验收2026-10-05)。AUDIT-004 保留首次基线并按用户授权追加软件实施复审，问题状态以其第 8 章为准；项目状态只在 roadmap 维护。

| 领域 | 当前必须满足的消费者不变量 | 直接证据与验证边界 |
| --- | --- | --- |
| 文件与会话 | 覆盖不追加；失败/提交前取消保留已有目标；A→B→Save As C 后信息编辑不改 A；同步保存不被旧 timer 回滚 | Phase A/B、H 的文件/会话/实际窗口；断电、磁盘耗尽与强杀不外推 |
| 格式准入 | 自有预设仅整数 v2、演奏及独立元数据仅整数 v3；不迁移旧数据；完整声明轨及合法 meta 后才导入标准 MIDI，稳定原生时间线不改变同采样顺序 | 现行格式准入按 Phase 36 Task 36-2 验证；文件保护沿用 Phase B/E/H；文档 JSON 示例须由生产 reader 实际准入 |
| 预设与调号 | UUID 在 rename/autosave 保持；独立目标覆盖确认、同路径不自删；普通选择保留全局调号，回放恢复 RecordedPreset.acoustic | Phase A/E/H；不将 JSON 含字段误写为普通选择必覆盖运行调号 |
| MIDI 导出 | Type 1、默认 960 PPQ、单轨、tick 0 120 BPM；保持显式起音/释放，不合成曲名或拍号；非 MIDI 与 SysEx 不输出 | Phase A/D/H，实际文件 header、消息和覆盖结果 |
| 身份与时序 | 原身份 FIFO/最后持有者释放；暂停捕获闭合；末尾 Off、设备时间域、Seek/loop 状态和完整预备拍在音频边界执行 | Phase C/D/E；CPU release/prepare 不等于物理热插拔 |
| 实时与 WAV | 自有回调零锁/零分配/零库函数三角；Take 参数按采样点执行，非零 startSample 不污染外部区间；完整音高同构仍是修复目标 | Phase E 的有限场景不覆盖当前 [P1 快照移调差异](../issues/known-issues.md#原生演奏快照移调与-wav-音高不一致)；不向 VST3 注入内置物理参数，不作逐比特保证；第三方框架锁/2048 消息上限分层 |
| UI 与诊断 | 双看板同一投影，输入身份不受输出反馈污染；静音最高优先，Notes 确认/取消正确；日志会话有界并报告故障，MIDI raw 力度准确 | Phase F/G/H 实际窗口、音频捕获、文件及像素 |
| 工程门禁 | Windows Debug app/tests 构建、默认 CTest、格式、迭代边界全量 tidy；确认 Chord/lifecycle 实际执行、用户目录无副作用、fixture 不依赖可选 NRVO | Phase G/H 的实际命令/receipt；不重写旧默认缓存失败为已消失 |

**实机补验必须独立记录**：按原报告 §4.5，在复制数据/可丢弃会话上执行目标厂商 VST3、辅助窗口失焦/IME、密集声卡回放及物理热插拔。未执行的项保持未验证；不能由默认测试或自建 native VST3 通过替代，也不能据此宣布整体平台验收完成。8 复音单核 CPU $\le 0.7\%$ 保持物理 SLA，本轮文档/文件验收不构成新性能测量。

## 本地化完整消息模板：当前消费者标准

现行单参数入口已使用 [ADR-015](../decisions/ADR-015-localized-message-templates-and-punctuation.md) 的完整消息模板；机制与刷新边界见 [国际化分册](features/internationalization.md)，已执行的窗口范围见 [声明式弹窗验收](features/declarative-ui-and-theming.md#33-内容定尺与统一底部操作区)。以下是持续回归不变量，不是待实现的通用 formatter：

- 删除/覆盖确认及相关提示按完整英文消息键查表，再进行一次 `{0}` 替换；不保留旧碎片别名。
- 英文原句回退、内嵌中文与外部缺键 fallback 保持；新弹窗按创建时 locale 查询，已有提示和原生窗口不保证即时重译。
- `{0}`、`{1}`、`%1`、中文及合法名称/路径原样插入，参数不二次解释、不自动全半角转换。
- 长名称、换行、确认/取消与 UUID/文件保护继续作为实际窗口回归项；历史内容缩放结果不外推物理 DPI／IME 全矩阵。
- 自动化覆盖语言机制和消费者边界，不钉具体译文；文档/文件示例验收不替代厂商插件或硬件认证。

## 状态标记

- [x] 已通过
- [ ] 未通过 / 未开始验证
- [~] 部分通过 / 待补充验证
- [-] 已废除 / 明确不实现

---

## AUDIT-004 Phase D：发音身份与采样级边界回归

直接验证见 [Phase D 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-d-实施记录与直接验证2026-10-04)，项目阶段状态仍只由 roadmap 管理。

- [x] On 与 Off 之间改变 offset/enabled/mask，FIFO 原身份仍释放；Q/K 与矩阵合并保留最后持有者。
- [x] 暂停捕获终结原身份/踏板、排除暂停中新演奏；Take、MIDI 写出及再导入不在重叠起音处合成额外 Off。
- [x] 最后 Off 等于 Take 长度/块末仍交付；实时/WAV 缩放采样点及声学结束一致，不依赖 UI Timer 清音。
- [x] 显式 keyUp/未知 trigger 拒绝，保留缺省 keyDown 与原文件/预设。
- [x] 48k↔44.1k、2x、播放/捕获暂停恢复保留 Take-relative 时长、位置与身份；设备故障/热插拔仍单独安全验收。
- [x] Seek/回跳先恢复 16 通道 program/bank/CC64/pitch，不重发历史 NoteOn。
- [x] 120 BPM 4/4 一/两小节在完整 2/4 秒后的音频下拍开始；跨多拍 UI 轮询、块对齐、取消/重建与零轮询后立即控制均有消费者证据。
- [x] CC67 先于和弦、新分配/偷声部/释放保留通道状态，连续值影响声学结果；原生 VST3 通道控制不改写。

## Phase 1-1：工程骨架可运行

状态：已通过。

- [x] `./scripts/dev.sh wsl-build` 构建成功。
- [x] `./scripts/dev.sh win-build` MSVC 验证成功。
- [x] JUCE GUI 程序可启动。
- [x] 主窗口正常显示。
- [x] 音频设备能初始化。
- [x] 没有因缺失 JUCE 子模块导致构建失败。

---

## Phase 1-2：最小演奏链路成立

状态：已通过。

- [x] 按下 `A/S/D/F` 等基础按键时可触发 note on。
- [x] 松开对应按键时可触发 note off。
- [x] 虚拟钢琴键盘组件可高亮联动。
- [x] 程序能够发声，来源为内置 fallback synth 或已加载插件。
- [x] 调整基础音量与 ADSR 参数后可听到变化。
- [x] 长按按键时不会异常重复触发。
- [x] 切换窗口焦点后 held key 不残留。

---

## Phase 2：插件系统与键盘映射

状态：已通过。

- [x] 程序能识别 VST3 插件格式。
- [x] 支持默认与自定义多目录扫描（`FileSearchPath` 规范化路径）。
- [x] 扫描完成后列表正常显示，失败文件记录至 Logger。
- [x] `KnownPluginList` XML 缓存启动恢复优化已就位。
- [x] 插件异步分片扫描（Chunked Scan），UI 显示扫描中状态与进度。
- [x] 成功加载 VST3 乐器并驱动发声。
- [x] 支持打开/关闭独立插件 Editor 窗口。
- [x] 默认键位映射覆盖 36 个字母数字键，采用稳定 key code。
- [x] 虚拟键盘翻页后映射稳定性通过回归验证。
- [-] 外部 MIDI 输入支持已移除（聚焦电脑键盘演奏场景，详见 ADR 006）。

专项测试见：[`./features/plugin-hosting.md`](./features/plugin-hosting.md)、[`./features/keyboard-mapping.md`](./features/keyboard-mapping.md)。

---

## Phase 3：UI 面板分层与录制回放 MVP

状态：已通过。

- [x] 主界面拆分为 Header / Plugin / Controls / Keyboard 基础分层。
- [x] 插件流程职责与只读 UI 状态流完成两轮收敛。
- [x] 可开始录制、停止录制并生成不可变 `RecordingTake`。
- [x] 回放 Take 重新注入发声链路（插件或 fallback synth）。
- [x] 录制内容可导出为标准 Type 1 MIDI 文件（960 PPQ）。
- [x] 离线渲染 Take 为 WAV 音频文件（fallback synth 路径）。
- [x] Performance Preset 系统支持新建/导入/切换/重命名/删除与 F1-F12 快捷键。

专项测试见：[`./features/recording-playback.md`](./features/recording-playback.md)、[`./features/performance-presets.md`](./features/performance-presets.md)。

---

## Phase 4：MIDI 文件导入与回放兼容性

状态：已通过。

- [x] 可通过 Import MIDI 打开标准 `.mid` 文件并回放。
- [x] 当前导入合并 Type 0/1 全部轨道，保留全轨 Tempo Map 与元数据；不再提供音符密度选轨模式。
- [x] Record / Playing 期间 Import MIDI 状态互斥保护。
- [x] 回放期间虚拟键盘联动高亮。
- [x] 回放中点击 Back 按钮可从开头重新播放。
- [x] 导入 playback take 禁止再次导出为 MIDI（Export MIDI 保持 disabled），允许导出 WAV。
- [x] 最近导入/导出路径已持久化并在 FileChooser 中复用。
- [x] 所有轨道进入统一 Take，完整声明结构之外的额外尾字节宽容；缺轨或截断拒绝，不提交部分内容。

专项测试见：[`./features/midi-file-import.md`](./features/midi-file-import.md)。

---

## Phase 5：架构收敛与 MainComponent 瘦身

状态：已完成。

- [x] 提取 `RecordingSessionController` 承载录制/回放/导入/导出编排。
- [x] 提取 `PluginOperationController` 承载扫描/加载/Editor 编排。
- [x] 提取 `SettingsWindowManager` 承载设置窗口生命周期。
- [x] 提取 `AppStateBuilder` 组装持久化基线与运行时快照。
- [x] `MainComponent.cpp` 大幅精简，保持轻量装配职责，主体仅负责顶层装配与回调接线，严格遵守生命周期与音频线程边界。

---

## Phase 6：数据持久化、调速与 MIDI 矩阵

状态：已完成。

- [x] `.devpiano` 原生演奏文件 JSON 序列化持久化保存与打开回放。
- [x] 打开损坏文件不崩溃，Logger 输出错误提示。
- [x] 播放速度 0.5x–2.0x 实时倍率调节；消息线程发布命令，音频块边界一致提交倍率、位置与游标，保留 NoteOff 与循环语义（直接验证见 [Phase C 实施记录](../archive/audit-004-code-quality-fix-phases.md#phase-c-实施记录与直接验证2026-10-03)）。
- [x] 16 通道 MIDI 矩阵路由（`ChannelMatrix` / `MidiChannelMapper`）。
- [x] 88 键拟真钢琴键盘（`CustomKeyboard`，支持 3 种着色与 3 种音符标注）。
- [x] 结构化日志与默认文件测试使用隔离目录；样本及实际消费者见 fixture-inventory，不把样本数量固化为门禁。
- [x] 最近文件列表（最多 10 条）与拖拽 `.devpiano` / `.mid` 文件即开即播。
- [-] 基础音符编辑器（Phase 6-4 永久搁置）。

专项测试见：[`./features/performance-persistence.md`](./features/performance-persistence.md)、[`./features/fixture-inventory.md`](./features/fixture-inventory.md)。

---

## Phase 7：VST3 离线渲染与国际化

状态：已完成。

- [x] `PluginOfflineRenderer` 独立创建离线 VST3 实例执行非实时音频渲染。
- [x] `WavExportTask` 后台多线程导出，支持取消与残留文件清理。
- [x] 运行时中英文双语即时切换（`LocaleManager` + `zh_CN.loc`）。
- [x] 拖放支持（`.devpiano` / `.mid` / `.devpiano.preset` / `.vst3`）。
- [x] Song Information 的标题与多行 Notes 经生产弹窗和会话事务提交；取消保持内存/绑定文件不变，诊断列表只读。
- [-] 全屏模式（Phase 7-7 明确不实现，窗口最大化即可替代）。

专项测试见：[`./features/plugin-offline-rendering.md`](./features/plugin-offline-rendering.md)。

---

## Phase 8–9：逐键个性化与配置快照

状态：已完成。

- [x] 128 项逐键自定义标签（`customKeyLabels`）与逐键颜色（`customKeyColours`）。
- [x] 全局调号控制（`keySignature`，-7..+7 半音）与 MIDI 移调开关。
- [x] 88 键虚拟键盘视觉交互与 `KeyBindingEditDialog` 绑定编辑。
- [x] 录制 Take 支持 `presetChange` 事件，回放时自动切换预设。

---

## Phase 10：主窗口 UI 现代化

状态：已完成。

- [x] 全局暗黑扁平化主题（`DevPianoLookAndFeel`）。
- [x] 旋钮化 ADSR 包络与主音量调节。
- [x] 拟真钢琴键盘黑白键发光与动态按压动画。
- [x] 底部状态栏与 Transport 播放控制图标化。

---

## Phase 11：声明式 UI 架构迁移（JIVE）

状态：已完成。

- [x] 引入 JIVE 框架，以 `juce::ValueTree` + JSON 样式表声明主窗口布局。
- [x] 彻底消除主窗口 5 个面板的 manual `setBounds()` 与像素手算代码。
- [x] `DesignTokens` 与 `StyleCatalog` 统一全局配色、字号与间距。
- [x] 原生自绘组件（`CustomKeyboard`、`AdsrCurve`、`StatusBarMidiDot`）通过工厂无缝注入 JIVE 树。

---

## 全面代码质量审计（AUDIT-001，2026-08-16）

状态：已通过。

- [x] 85 项登记问题全量闭环（56 项未处理全关闭，14 项低频/已缓解项维持暂缓）。
- [x] 消除音频回调堆分配与延迟 prepare。
- [x] 修复 `masterGain` 跨线程数据竞争与异步生命周期防护。
- [x] 提取公共离线渲染管线 `RenderPipeline`（时间戳缩放、排序与 panic 注入）。
- [x] 补齐核心控制器确定性测试，测试套件与断言全面覆盖。
- [x] 全量源码文件 clang-tidy 0 诊断，clang-format 零违规。

审计报告见：[`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)。

---

## Phase 12–14：内置物理建模钢琴音源

状态：已完成（2026-08-19）。

- [x] **Phase 12（谐波加法 v1）**：8 分音谐波加法合成、Velocity 响度/亮度双映射与 Tone 调节。
- [x] **Phase 13（刚性失谐与模态耗散 v2）**：JOS PASP 刚性琴弦失谐 $f_m = m f_0 \sqrt{1 + B m^2}$ 与 3 峰音板谐振器。
- [x] **Phase 14（增强模态合成 v3）**：
  - Magic Circle 零三角函数二阶递归振荡器，单核 CPU ≤ 0.7%；
  - 动态分音剪枝（20/14/8/6 分音按音高分区）；
  - 双阶段衰减（Two-stage decay）；
  - 同音三弦微失谐干涉拍频（Unison beating）；
  - 8 峰音板主模态组（75~950 Hz）。
- [x] 确立为唯一默认内置发声来源，经 Windows 侧人工听觉回归确认。

---

## Phase 15：UI 架构统一至 JIVE（声明式弹窗与设置面板）

状态：已完成（2026-08-19）。

- [x] **Phase 15-A**：构建通用的 `JiveModalDialog` 基础设施与声明式模板（SingleInput / Confirm / MetadataEdit / Progress）。
- [x] **Phase 15-B**：预设新建/重命名/删除与歌曲信息编辑弹窗全面迁移至 `JiveModalDialog`，消除手写坐标 Content 类。
- [x] **Phase 15-C**：设置面板迁移至 `SettingsLayoutModel`，16 通道跟随开关采用 JIVE CSS Grid（8 列 × 2 行）；历史原生音频选择器后来已由声明式音频设备卡片替换。
- [x] **Phase 15-D**：`WavExportTask` 导出进度接入 JIVE 声明式进度弹窗，维持多线程模型与取消清理逻辑。
- [x] **Phase 15-E**：单元测试全绿（零失败），三闸门与 Windows 验证通过。

---

## Phase 16：UI 局部脏矩形渲染与预设覆盖确认

状态：已完成（2026-08-20）。

- [x] **虚拟键盘脏矩形局部重绘（`CustomKeyboard`）**：引入 `repaintKey(k)` 与 `g.getClipBounds()` 相交判断，消灭全量 88 键重绘，UI 渲染耗时降低 70% 以上。
- [x] **预设导入同名覆盖确认**：`PresetFlowSupport::handleImportPresetFile` 接入 `PresetConfirmDialog` 声明式覆盖确认对话框。
- [x] **测试与回归**：全量单元测试与 MSVC 编译验证通过。

---

## Phase 17：真实物理打击感钢琴音源重构

状态：已完成（2026-08-22）。

- [x] **消灭锯齿波拉弦感**：击弦点几何梳状滤波（$d/L \approx 1/8 \sim 1/14$）与非线性琴槌毛毡硬化截止谱。
- [x] **重塑打击起音瞬态**：$\text{Attack} \le 0.2\text{ ms}$ 极速起振门控，注入 $2\sim 3\text{ ms}$ 毛毡撞击瞬态冲击核（Hammer Strike Click）。
- [x] **双阶段衰减强化**：快衰减权重提升至 $80\%\sim 88\%$，重构 8 峰云杉木音板模态并与 Resonance 旋钮动态绑定。
- [x] **单元测试验证**：`PianoSynthVoiceTest` 覆盖物理打击与声学参数回归。

---

## Phase 18：88 键物理参数化与微观相位色散

状态：已完成（2026-08-22）。

- [x] **88 键连续参数模型**：基于 Bensa & Steinway B 实测标定，连续插值刚度 $B$、击弦比 $d/L$、衰减 $\tau_{\text{slow}}$ 与 1/2/3 弦物理分区（`Piano88KeyTable.h`）。
- [x] **STFT 微初相色散矩阵**：内联 $3 \times 64$ 实测最优初始相位矩阵（`kOptPhaseTable`），消灭狄拉克脉冲式机械聚焦。
- [x] **空气阻尼与琴桥峰**：引入空气黏性阻尼与 1.8kHz Bridge Hill 琴桥共振峰。

---

## Phase 19：立体声音板共鸣箱与同音三弦微动力学

状态：已完成（2026-08-22）。

- [x] **16 峰物理云杉木音板模态**：覆盖 48Hz~2250Hz 底箱呼吸、长琴桥耦合与各向异性散射模态。
- [x] **琴桥立体声空间辐射**：88 键声像几何空间扩散，消灭单声道耳膜居中压迫感。
- [x] **同音三弦独立振荡器拍频**：中高音区三弦独立微失谐与 STFT 空间初相。

---

## Phase 20：微观物理动力学（纵波先驱声与击键混沌微扰）

状态：已完成（2026-08-22）。

- [x] **低音钢弦纵波先导声**：依据 $v_L \approx 5100\text{ m/s}$ 为低音弦（MIDI 21~52）注入极短金属张力冲击。
- [x] **击弦混沌微扰**：同音连续击键注入微秒级混沌微扰，消除快速轮指机械克隆感。

---

## Phase 21：踏板交感共鸣与琴盖空间声学

状态：已完成（2026-08-22）。

- [x] **延音踏板全局交感共鸣弦池**：12 半音基底谐振器响应 CC64 延音踏板，注入全开放弦共鸣。
- [x] **琴盖反射与近场微反射**：3 抽头近场微反射消除干燥贴耳感，重现真实空气深度。

---

## Phase 22：物理声学极致深化与机械拟真

状态：已完成（2026-08-22）。

- [x] **制音器落弦瞬态**：$80\sim 150\text{ Hz}$ 制音器落弦闷击与琴键释放机械声。
- [x] **琴盖开合度声学传递函数**：Full / Half / Closed 3 级开合高频滚降与箱体反射。
- [x] **琴桥断裂音色补偿**：MIDI 43~44（G2/G#2）琴桥交界弦长与刚度台阶式补偿。
- [x] **强击音高微漂移与软饱和**：$fff$ 强击瞬间 $2\sim 5$ 音分音高瞬态上浮与软饱和。
- [x] **单键开放弦交感**：按住低音键弹奏高音触发的开放弦局部交感。

---

## Phase 23：大师级音色校准与 Pianoteq 对齐精调

状态：已完成（2026-08-23）。

- [x] **动态琴槌非线性刚度**：三层毛毡动力学压实模型（$h_{\text{eff}}$）、动态接触时间 $T_c$ 与速度相关滚降指数。
- [x] **同音三弦 Mid-Side 展开**：左右声道差分拍频展开，单声道纯净抵消，立体声开阔呼吸。
- [x] **云杉木 4.2kHz 高频截止**：消除超高频铁皮盒共振，赋予深厚木质感。
- [x] **起音瞬态裂音微调**：前 $3\text{ ms}$ 高频冲击裂音（HF Crack），对齐真琴极速起振。

---

## Phase 24：生命力与非线性动力学绽放

状态：已完成（2026-08-23）。

- [x] **泛音时间滞后膨胀与绽放（Harmonic Blooming）**：$n \ge 3$ 阶高次分音非线性能量泵浦与 $10\sim 25\text{ ms}$ 上升绽放。
- [x] **琴槌接触微阻尼与脱离释放**：消灭 $t=0$ 正弦机械突兀开门感。
- [x] **动态声场空间漫射**：从击打点声源在 $25\text{ ms}$ 内平滑漫射为音板面声源包围场。
- [x] **确定性物理验证**：物理声学自动化测试覆盖动态空间漫射等关键行为，零失败。

---

## Phase 25：Linux 桌面与音频路径扩展

状态：已完成（2026-08-25）。

- [x] **ALSA/JACK 运行时适配**：Linux WSL/Desktop 音频设备探测、初始化与安全释放。
- [x] **无头与 CI 编译合规**：WSL 与 Linux 无头环境下单元测试执行零挂起。

---

## Phase 26：MIDI 多轨并轨合并引擎

状态：已完成（2026-08-27）。

- [x] **多轨事件流合并（MidiTrackMergeEngine）**：标准 Type 1 MIDI 全部音轨时序绝对排序与平滑合并。
- [x] **通道映射策略**：支持 passThrough、autoAssign 与 forceTrack 策略。

---

## Phase 27：JUCE 9 升级与 UI 基础设施内化

状态：已完成（2026-08-29）。

- [x] **JUCE 9.0.1 正式升级**：完成 API 弃用迁移与构建系统适配。
- [x] **ADR-014 UI 运行时内化**：退役 JIVE 外部子模块，内化至 `source/UI/jive/core/` 并实施 API Freeze。

---

## Phase 28：UI 治理与统一宿主门面

状态：已完成（2026-08-31）。

- [x] **ViewHost 统一宿主门面**：业务层解耦底层 JIVE 裸指针，强类型组件访问与生命周期安全。
- [x] **AUDIT-002 质量修复全量闭环**：21 项问题逐一清零。

---

## Phase 29：声学物理拟真交互与力度手感

状态：已完成（2026-09-12）。

- [x] **琴盖 3 态物理控制**：全开、半开与闭盖声学传递函数与箱体反射。
- [x] **弱音踏板物理拟真（Una Corda）**：击打位置微移、二弦振动与 MIDI CC 67 全流程联动。
- [x] **4 种触键力度曲线自适应**：标准、轻触、重触与宽动态 S 曲线。

---

## Phase 30：古典微调律制与基准音高

状态：已完成（2026-09-13）。

- [x] **6 大古典微律支持**：平均律、纯律、毕达哥拉斯律、1/4 中庸全音律、韦克迈斯特三律与基恩伯格三律。
- [x] **A4 基准音高微调已实现**：当前滑块与 `TemperamentEngine` 为 410.0 ~ 450.0 Hz；产品契约 400.0 ~ 480.0 Hz 的两端尚未覆盖，见 [`../issues/known-issues.md`](../issues/known-issues.md)。
- [x] **全栈持久化与预设联动**：SettingsStore 与 PerformancePreset 序列化及离线导出对齐。

---

## Phase 31：空间声学视角与房间混响环境

状态：已完成（2026-09-13）。

- [x] **演奏者 / 听众双视角成像（PerspectiveProcessor）**：立体声像镜像反转、距离高频吸收与单声道下混能量守恒。
- [x] **轻量数学算法房间混响网络（RoomReverbEngine）**：Studio、Chamber 与 Concert Hall 三大预设与干湿比调节。
- [x] **设置面板集成与离线对齐**：WavFileExporter 挂载独立混响引擎，保证 1:1 比特级一致性。

---

## Phase 32：微观机械物理拟真与声学瑕疵

状态：已完成（2026-09-13）。

- [x] **延音踏板扫掠声与共鸣冲击**：CC64 踏板下踏/抬起机械毛毡刮擦与开放弦冲击脉冲。
- [x] **制音器落木闷击与琴键摩擦**：快离键强烈撞击与慢离键毛毡摩擦延展，离键速度动态 ADSR 阻尼缩放。
- [x] **泛音刚度抖动与逐键毛毡老化**：确定性逐键不谐和度抖动与毛毡老化穿透力调节。
- [x] **全栈闭环与测试基线**：全套自动化单元测试满分通过，三闸门合规，零回归。

---

## Phase 33：可观测性加固与生产级诊断基础设施

状态：已完成（2026-09-14）。

- [x] **Dual-Sink 日志**：当前 DevPianoLogger 活动/单备份合计 512 KiB，各 256 KiB，会话内轮转；故障停用文件 sink 并留原因，debugger 收完整消息。历史 FileLogger 的构造裁剪不再冒称会话滚动。
- [x] **设置界面诊断卡片直达与系统文件管理器联动**：在诊断卡片中新增“打开日志目录”按钮，调用 `juce::File::revealToUser()` 调起原生文件管理器；
- [x] **MidiTrace 与诊断测试防线**：新增 `DiagnosticsTest`，全量覆盖 MIDI 协议反序列化与日志落盘安全；
- [x] **运行时字符编码断言消除**：消除多字节字符字面量，杜绝 `juce_String.cpp:327` 运行时断言。

---

## AUDIT-003 专项：全面代码质量审计缺陷消除与架构对齐

状态：已完成（2026-09-15）。

- [x] **Linux Headless 单测事件循环泵送（TEST-001）**：引入消息队列冲刷机制，彻底清空 Linux 内部套接字管道，`juce_Messaging_linux.cpp:87` 告警彻底归零；
- [x] **插件与内置 Master 混响对齐**：实时/离线使用相同 RoomReverb 参数与采样级快照语义；WAV 量化和插件 offline 行为不保证逐比特音频相同。
- [x] **底层 Core 单向拓扑恢复（ARCH-001）**：解耦 `AppState.h` 对上层 `SettingsModel` 与 `ChannelMatrix` 的反向包含；
- [x] **解码内存优化与预设字段对齐（PERF-001 / DOC-001 / DOC-002）**：优化 `MidiTextDecoder` 内存分配，对齐 `"concert_hall"` 空间标识。

---

## Phase 34：键盘演奏交互质变与演奏表现力增强

状态：已完成（2026-09-23）。

- [x] **Phase 34-A（QWERTY Visualizer 5 行 Performance Map 声明式卡片）**：
  - 5 行 ANSI 物理键盘网格自适应排版，物理击键下沉与 50fps 荧光余晖动画；
  - 12-TET 和声调色板（Pitch Class Harmony Hues）与三和弦几何色相投影；
  - 4 种按键着色模式（Classic / Channel / Velocity / Harmony），QWERTY 与 88 键钢琴同频联动。
- [x] **Phase 34-B（Layout Group 轻量多键组与 HeldKey Identity 发音身份快照）**：
  - 单预设支持 4 组键位分组（`KeyGroup`），反引号键（`）或 UI 按钮秒级切换；
  - 发音身份恒定原则（Note-off Identity Preservation）：NoteOff 100% 依据 NoteOn 触发时锁定的发音身份快照注销，彻底杜绝悬挂音。
- [x] **Phase 34-C（SustainPolicy 与 Sample-Accurate 事件级 Sync 切分踏板）**：
  - 音频块内部采样点级别调度 $\text{CC64}(0) \to \text{NoteOn} \to \text{CC64}(127)$，消除空格键踩放断音空洞，杜绝线程 Sleep；
  - 内置物理建模音源与 VST3 插件、录音引擎端到端对齐。
- [x] **Phase 34-D（PerformanceModifierState 瞬态 Press 修饰符）**：
  - Shift 键力度拉满（Velocity Boost）、Alt 键高八度平移（+8va），松开自动回弹；
  - 纯事件流变换（Event-time Transformation），基线配置 100% 零突变；UI 实时展示 HUD 标签。
- [x] **Phase 34-E（扫描器增量持久化与乐器端点概念收敛）**：
  - 插件扫描逐项增量持久化（Crash-safe State Persistence），dead-man's pedal 崩溃点记录与黑名单推迟；
  - `InstrumentEndpoint` 统一内置物理建模钢琴与 VST3 乐器端点抽象，解耦设备准备、实时发声与离线渲染。
- [x] **Phase 34-F（跨平台实现深度收敛与 JUCE 9 原生框架利用全面升级）**：
  - 彻底拔除 `Main.cpp` 中的 Win32 `WNDPROC` Hook、`AttachThreadInput` 与 `<windows.h>`，全平台统一采用 JUCE 9 原生事件与异步分发；
  - `WavExportTask` 完全非阻塞异步化（`startAsync`），主应用编译配置彻底移除 `JUCE_MODAL_LOOPS_PERMITTED=1`；
  - 全库字符串字面量 100% 达到 Strict 7-bit ASCII 铁律，删除 LookAndFeel 废弃 AlertWindow 绘制代码；
  - JUCE 9 原生 `createLegalFileName` 替换自造文件名过滤轮子，运行时配置目录统一为 `DevPiano`。

## Phase 35：键盘演奏表现力深水区与练琴基础设施

状态：已完成（Phase 35-A~35-D，2026-09-28）。

完成计划见 [Phase 35 归档](../archive/phase35-keyboard-expressive-dynamics-and-practice-infrastructure.md)，以下勾选保留原交付历史；后续反证及软件实施复审见 [AUDIT-004](../audit/AUDIT-004-code-quality-audit-2026-10-02.md)，完整修复任务与消费者输入见 [AUDIT-004 Phase 归档](../archive/audit-004-code-quality-fix-phases.md)。当前小阶段规划见 [当前迭代](../roadmap/current-iteration.md)。

- [x] **Phase 35-A（无锁采样级音频节拍器与视觉节拍指示）**：
  - 确定性纯数学阻尼正弦脉冲发生的采样级 Click Engine（强拍 1600Hz / 弱拍 800Hz / 6/8 次强拍 1100Hz），零堆分配、零锁、零外部采样依赖；
  - 支持 2/4、3/4、4/4、6/8 常见拍号与 40~280 BPM 无级可调；
  - 基于最近最多 3 个点击间隔滑动均值的 Tap Tempo 测速算法（最多保留 4 个时间戳），间隔超过 2 秒时重置；
  - 录音前预备拍（Count-in，支持 1~2 小节倒计时触发）；倒计时期间关闭节拍器会取消本次倒计时。
  - JIVE 传输卡片提供 Metro/BPM/Tap 控件；状态栏根据节拍序号显示强弱拍符号与渐隐反馈，传输按钮不显示同频闪烁节拍灯；
  - 键盘快捷键 `Ctrl+M` 切换与设置持久化落盘；
  - 专项自动化测试覆盖 Tap Tempo 三间隔滑动均值、BPM 限幅、2 秒超时重置，以及采样精确度、拍号循环、动态变速与零分配。
- [x] **Phase 35-B（打字击键动态力度与人性化微扰引擎）**：
  - 基于物理按键间隙 $\Delta t$ 的 `TypingCadenceEstimator`，快弹华彩（$\Delta t \le 60\text{ ms}$）自适应输出高力度（$122/127 \approx 0.960\text{f}$），慢按抒情（$\Delta t \ge 500\text{ ms}$）自适应回落至低力度（$76/127 \approx 0.598\text{f}$），乐句停顿（$> 1.0\text{ s}$）平滑复位基准力度；
  - `VelocityHumanizer` 采用确定性哈希伪随机微扰（默认 $\pm 0.035\text{f} \approx \pm 4.5$ 力度），输出钳制在 $[1/127, 1.0]$，不使用高斯分布；动态参数当前没有 UI 编辑控件；
  - 严格保障 Shift 键力度拉满 1.0f 的最高仲裁优先级；
  - 自动化测试覆盖击键间隔估算、力度微扰、Shift 最高优先级仲裁，以及关闭 cadence 动态后不再按动态基线重缩放绑定力度、零力度保持静音。
- [x] **Phase 35-C（实时和弦识别与乐理分析 HUD）**：
  - 基于音高类集合（Pitch Class Set）与循环位移掩码的乐理和弦识别纯函数 `detectChord`；
  - 高精度识别三和弦、七和弦、九和弦、挂留/减/增和弦、Power Chord 及带转位和低音倾向性仲裁的 Slash Chords；
  - `QwertyCard` 标题栏 `qwerty-chord-badge` 与 `QwertyComponent` 内部右上角毛玻璃 12-TET 色相 HUD 徽章联动显示；
  - 和弦持续按下时 HUD 保持显示；最后一个和弦音释放后约 300ms 淡出至近透明，并停止淡出计时。
  - 自动化测试覆盖原位、转位、八度音与杂音容错识别。
- [x] **Phase 35-D（MIDI 伴奏 A-B 片段循环跟练与进度自由跳转）**：
  - `TimelineBar` 支持 Take-relative 时间轴点击/拖拽 Seek 与 A/B 标记；有效循环区间为半开区间 `[A, B)`，无效或倒置区间不启用循环；
  - 音频块内按采样偏移处理循环边界，并在回跳至 A 前清理 16 个 MIDI 通道的延音与发音；回放倍速保持既有 `0.50x ~ 2.00x` 范围；
  - 自动化测试覆盖 Seek 边界、多通道时间线、循环边界与发音清理；REC-008、REC-009 已完成手工验证。
  - 播放游标在暂停/恢复与音频设备重建后保持速度缩放后的精确位置；Seek 越过 B 点时，在循环继续前按有效 A/B 区间归一化。
- [x] **Phase 35 回归项（键床布局防遮挡）**：自动化布局测试覆盖 QWERTY 与插件面板同时展开（980 × 740），以及 QWERTY 折叠时的 580 px 配对下限；88 键白键完整位于横向滚动条可视高度内。

---

## v1.0.0 正式发布验收标准

状态：已通过（2026-08-23）。

- [x] **三闸门基线**：
  - 格式化合规：`./scripts/dev.sh format --check` 0 违规；
  - 单元测试覆盖：`./scripts/dev.sh test` 覆盖核心引擎、物理声学与 UI 测试套件，零失败；
  - Windows 验证构建：`./scripts/dev.sh win-build` 与 `./scripts/dev.sh win-build --release` 成功生成 `DevPiano.exe`。
- [x] **Windows x64 手工冒烟测试**：
  - 程序启动、窗口居中自适应与音频设备初始化正常；
  - 电脑键盘 A/S/D/F 与 88 键虚拟键盘点击发声、动态高亮与音质纯净；
  - VST3 扫描、加载、Editor 打开、发声、卸载及退出无崩溃；
  - 演奏录制、回放、保存为 `.devpiano`、重新打开及 MIDI 文件导入正常；
  - 离线导出 WAV 进度条与文件生成正常；
  - 运行时中英文双语即时切换正常。
- [x] **发布产物与打包**：
  - `DevPiano-v1.0.0-win-x64.zip` 与 `DevPiano-v1.0.0-win-x64.sha256` 完整生成；
  - `CHANGELOG.md` 与 `CMakeLists.txt` 版本号对齐为 `1.0.0`。

---

## 建议例行最小回归集合

关键修改提交前，WSL 主工作树只刷新编译数据库；Windows 镜像树执行 Debug 构建及完整软件测试：

1. **三闸门检查**：

   ```bash
   ./scripts/dev.sh wsl-build --configure-only
   # Windows Debug 单元测试使用镜像树 Developer PowerShell，见下方链接
   ./scripts/dev.sh format --check
   ./scripts/dev.sh win-build             # Windows 镜像 MSVC 编译链接验证
   ```

   Windows 侧先以 `BUILD_TESTS=ON` 构建 `devpiano_tests`，再用 CTest 运行，具体命令见 [`../guides/quickstart.md`](../guides/quickstart.md)；Linux CI 使用 `./scripts/dev.sh test`，本地 WSL 主树不运行该脚本。

2. **冒烟手工回归**：
   - 启动程序，音频设备初始化正常；
   - `A/S/D/F` 触发物理建模钢琴发声，打字击键动态力度与 Shift 极值正常，虚拟键盘高亮正常；
   - 节拍器开关（`Ctrl+M`）与 Tap Tempo 测速准确；
   - 弹奏和弦时 Qwerty HUD 徽章实时显示识别和弦与淡出；
   - VST3 扫描、加载、Editor 打开、弹奏发声与卸载；
   - 录制一段演奏、回放、A-B 循环与时间轴 Seek 跳转、保存为 `.devpiano`、重新打开；
   - 导入标准 `.mid` 并回放；
   - 导出 WAV，观察 JIVE 进度条与文件生成；
   - 打开设置窗口，切换音频设备与语言（中英文即时切换无撕裂）；
   - 退出应用无崩溃、无挂起。

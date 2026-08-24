# devpiano 阶段验收标准

> 用途：定义各阶段的可验证完成标准与全量回归清单。
> 更新时机：阶段验收标准变化、新里程碑完成或测试基线更新时。

说明：本文件描述阶段验收标准与回归清单。项目状态与路线图以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准。

## 状态标记

- [x] 已通过
- [ ] 未通过 / 未开始验证
- [~] 部分通过 / 待补充验证
- [-] 已废除 / 明确不实现

---

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
- [x] 智能选择 Note 密度最高的轨道，跳过纯 tempo/meta 轨道。
- [x] Record / Playing 期间 Import MIDI 状态互斥保护。
- [x] 回放期间虚拟键盘联动高亮。
- [x] 回放中点击 Back 按钮可从开头重新播放。
- [x] 导入 playback take 禁止再次导出为 MIDI（Export MIDI 保持 disabled），允许导出 WAV。
- [x] 最近导入/导出路径已持久化并在 FileChooser 中复用。
- [~] 合并所有轨道至单一 timeline（Phase 4-6 明确搁置，保留单轨推荐模式）。

专项测试见：[`./features/midi-file-import.md`](./features/midi-file-import.md)。

---

## Phase 5：架构收敛与 MainComponent 瘦身

状态：已完成。

- [x] 提取 `RecordingSessionController` 承载录制/回放/导入/导出编排。
- [x] 提取 `PluginOperationController` 承载扫描/加载/Editor 编排。
- [x] 提取 `SettingsWindowManager` 承载设置窗口生命周期。
- [x] 提取 `AppStateBuilder` 组装持久化基线与运行时快照。
- [x] `MainComponent.cpp` 从 1587 行单体降至 446 行，严格遵守生命周期与音频线程边界。

---

## Phase 6：数据持久化、调速与 MIDI 矩阵

状态：已完成。

- [x] `.devpiano` 原生演奏文件 JSON 序列化持久化保存与打开回放。
- [x] 打开损坏文件不崩溃，Logger 输出错误提示。
- [x] 播放速度 0.5x–2.0x 实时倍率调节，线程安全且变速平滑校准。
- [x] 16 通道 MIDI 矩阵路由（`ChannelMatrix` / `MidiChannelMapper`）。
- [x] 88 键拟真钢琴键盘（`CustomKeyboard`，支持 3 种着色与 3 种音符标注）。
- [x] 结构化日志系统（`DP_LOG_*` / `DP_TRACE_MIDI`）与测试夹具库（8 MIDI + 1 Performance）。
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
- [-] Metadata 编辑独立对话框（Phase 7-5 明确搁置）。
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
- [x] 集成 `melatonin_inspector` 运行时可视化检查器。
- [x] 原生自绘组件（`CustomKeyboard`、`AdsrCurve`、`StatusBarMidiDot`）通过工厂无缝注入 JIVE 树。

---

## 全面代码质量审计（AUDIT-001，2026-08-16）

状态：已通过。

- [x] 85 项登记问题全量闭环（56 项未处理全关闭，14 项低频/已缓解项维持暂缓）。
- [x] 消除音频回调堆分配与延迟 prepare。
- [x] 修复 `masterGain` 跨线程数据竞争与异步生命周期防护。
- [x] 提取公共离线渲染管线 `RenderPipeline`（时间戳缩放、排序与 panic 注入）。
- [x] 补齐核心控制器确定性测试，断言总数升至 3100+。
- [x] 全量 44 源码文件 clang-tidy 0 诊断，clang-format 零违规。

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
- [x] **Phase 15-C**：设置面板重构为 `SettingsLayoutModel`，16 通道跟随开关采用 JIVE CSS Grid（8 列 × 2 行），`AudioDeviceSelectorComponent` 原生注入。
- [x] **Phase 15-D**：`WavExportTask` 导出进度接入 JIVE 声明式进度弹窗，维持多线程模型与取消清理逻辑。
- [x] **Phase 15-E**：单元测试全绿（3101 断言），三闸门与 Windows 验证通过。

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
- [x] **单元测试验证**：`PianoSynthVoiceTest.cpp` 确定性断言 100% 通过。

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
- [x] **确定性物理断言**：全量 60 类单元测试、11989+ 断言 100% 满分通过。

---

## v1.0.0 正式发布验收标准

状态：已通过（2026-08-23）。

- [x] **三闸门基线**：
  - 格式化合规：`./scripts/dev.sh format --check` 0 违规；
  - 单元测试覆盖：`./scripts/dev.sh test` 60 个测试套件 100% 通过；
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

关键修改提交前，在 WSL 与 Windows 侧执行以下基线验证：

1. **三闸门检查**：
   ```bash
   ./scripts/dev.sh wsl-build --configure-only
   ./scripts/dev.sh test
   ./scripts/dev.sh format --check
   ./scripts/dev.sh win-build
   ```
2. **冒烟手工回归**：
   - 启动程序，音频设备初始化正常；
   - `A/S/D/F` 触发物理建模钢琴发声，音质纯净无杂音，虚拟键盘高亮正常；
   - VST3 扫描、加载、Editor 打开、弹奏发声与卸载；
   - 录制一段演奏、回放、保存为 `.devpiano`、重新打开；
   - 导入标准 `.mid` 并回放；
   - 导出 WAV，观察 JIVE 进度条与文件生成；
   - 打开设置窗口，切换音频设备与语言（中英文即时切换无撕裂）；
   - 退出应用无崩溃、无挂起。

# devpiano Roadmap

> 用途：作为唯一的项目状态、阶段路线与近期重点来源。
> 更新时机：阶段目标变化、功能完成度变化、重大风险变化时。

## 1. 项目目标

devpiano 是一款基于 JUCE 的现代 C++ 电脑键盘钢琴应用，聚焦软件键盘演奏、高保真自主物理建模音源与 MIDI 文件处理。

核心替代方向：

- 旧 WASAPI / ASIO / DSound 后端 -> JUCE `AudioDeviceManager`。
- 旧 VST 加载逻辑 -> JUCE `AudioPluginFormatManager` / `AudioPluginInstance`。
- 旧 Windows 键盘输入逻辑 -> JUCE `KeyListener` / `KeyPress` + 可配置 MIDI 映射。
- 旧 GDI / 原生控件 UI -> JUCE `Component` 树 + JIVE 声明式 UI 体系。
- 旧配置系统 -> `ApplicationProperties` / `ValueTree` / 项目内状态模型。
- 旧 fallback 简单发声 -> 覆盖 7 大声学子系统的自主研发增强物理建模钢琴音源（`PianoSynthVoice`）。

---

## 2. 阶段路线图与版本里程碑

### Phase 1：工程骨架与最小演奏 [v0.1.0 已发布，2025-05-06]

JUCE GUI 启动、音频设备初始化、电脑键盘触发 note on/off、虚拟钢琴键盘联动、内置 fallback synth 发声。

### Phase 2：插件系统与键盘映射 [v0.1.0 已发布，2025-05-06]

VST3 插件扫描 / 加载 / 卸载 / editor 窗口、键盘映射系统（可配置）+ Performance Preset、扫描 UX 增强（分片进度、失败列表可发现性）。

详细完成记录见 [`../archive/phase2-3-implementation-backlog.md`](../archive/phase2-3-implementation-backlog.md)。

### Phase 3：UI 与高级功能 [v0.1.0 已发布，2025-05-06]

UI 拆分为头部 / 插件 / 参数 / 键盘区域、Performance Preset 系统（`.devpiano.preset` JSON / 自动发现 / CRUD）、录制 / 回放 / MIDI 导出 / WAV 离线渲染 MVP。

详细完成记录见 [`../archive/phase2-3-implementation-backlog.md`](../archive/phase2-3-implementation-backlog.md)。

### Phase 4：MIDI 文件导入 [v0.1.0 已发布，2025-05-06]

MIDI 文件导入、自动选轨、回放、虚拟键盘可视化、最近路径记忆、主窗口尺寸自适应与恢复。

功能与测试文档：[`../reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)。

---

### Phase 5：架构收敛与 MainComponent 瘦身 [v0.2.0 已发布，2026-07-19]

`MainComponent.cpp` 从 ~1587 行降至 ~446 行。
提取 `RecordingSessionController` / `PluginOperationController` / `SettingsWindowManager` / `AppStateBuilder`。

详细完成记录见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

### Phase 6：功能补齐——钢琴键盘、MIDI 矩阵、持久化、调速与 GUI 设置 [v0.2.0 已发布，2026-07-19]

自定义钢琴键盘（`CustomKeyboard`，支持 3 种着色 / 3 种音符显示模式）、16 通道 MIDI 矩阵（`ChannelMatrix`）、note-only 绑定编辑器。
演奏文件持久化、播放速度控制（0.5x–2.0x 原子变速）、MIDI 导入增强（CC/pitch bend/program change）、Diagnostics 层、测试夹具库。
5 项 GUI 设置控件（colourMode / noteDisplay / fadeSpeed / resizable / instrumentFilter toggle）。

详细完成记录见 [`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)。

### Phase 7：VST3 离线渲染与国际化 [v0.2.0 已发布，2026-07-19]

VST3 离线渲染（WAV 导出 + `ExportDialog` 进度）、播放速度精确控制（Slider + atomic 线程安全）、拖放文件支持、运行时中英文语言切换（JUCE `Translation`）。
Phase 7-5（Metadata 编辑对话框）— 明确搁置（基础设施已就位，UI 无现阶段价值）。
Phase 7-7（全屏模式）— 不实现（`resizable` toggle + OS 最大化可替代）。

详细完成记录见 [`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)。

---

### 架构优化 [v0.3.0 已发布，2026-08-16]

架构优化 Backlog 七项（P0/P1/P2）全部完成：最近文件列表 UI、PluginOfflineRenderer 生命周期注释、PerformanceFile Base64 序列化、Diagnostics 日志层迁移、WavExportOptions 独立头文件、SettingsComponent ValueTree::Listener、MainComponent 瘦身。

详细完成记录见 [`../archive/architecture-optimization-backlog.md`](../archive/architecture-optimization-backlog.md)。

### Phase 8：逐键个性化与调号系统 [v0.3.0 已发布，2026-08-16]

逐键自定义标签（Per-Key Labels）和颜色（Per-Key Colors），全局调号 + MIDI 移调开关。

详细完成记录见 [`../archive/phase8-9-completion.md`](../archive/phase8-9-completion.md)。

### Phase 9：配置快照与体验增强 [v0.3.0 已发布，2026-08-16]

Performance Preset、88 键完整钢琴键盘、Smooth Pitch Bend、乐曲信息编辑。

详细完成记录见 [`../archive/phase8-9-completion.md`](../archive/phase8-9-completion.md)。

### Phase 10：主窗口 UI 现代化 [v0.3.0 已发布，2026-08-16]

自定义 LookAndFeel 暗黑主题、旋钮化 ADSR/音量、插件面板折叠化、拟真键盘渲染、Transport 图标化、底部状态栏、动态布局尺寸规则。

详细完成记录见 [`../archive/phase10-ui-modernization.md`](../archive/phase10-ui-modernization.md)。

### Phase 11：声明式 UI 架构迁移（JIVE + melatonin_inspector） [v0.3.0 已发布，2026-08-16]

JIVE 声明式 UI 框架（`juce::ValueTree` 布局 + JSON 样式表 + Flex/Grid 自适应）替代 5 个面板的硬编码 `setBounds()` 布局；melatonin_inspector 运行时检查器加速 UI 迭代反馈；`design_tokens.json` 统一 JIVE 与原生组件样式来源；`Ctrl+R` / 文件监听热重载；`MainComponent::resized()` 缩减至 3 行（JIVE FlexBox 自动响应）。`CustomKeyboard` 与 ADSR 曲线经组件工厂原生注入，业务逻辑层零改动。

详细计划与完成记录见 [`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)。

### 全面代码质量审计 (AUDIT-001) [v0.3.0 已发布，2026-08-16]

代码质量审计（`AUDIT-001`，2026-08-16）登记 85 项全部闭环（56 项未处理全关闭，14 项已暂缓维持）；三闸门全绿 + win-build 通过 + 全量源码文件 clang-tidy 0 诊断。消除音频回调堆分配与延迟 prepare、修复 `masterGain` 跨线程数据竞争、提取公共离线渲染管线 `RenderPipeline`、断言总数提升至 3100+。

审计报告见 [`../audit/AUDIT-001-code-quality-audit-2026-08-16.md`](../audit/AUDIT-001-code-quality-audit-2026-08-16.md)，Phase A–H 逐项完成记录见 [`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)。

---

### Phase 12–14：内置物理建模钢琴音源（SineSynth → Enhanced Modal Piano v3） [v0.4.0 已发布，2026-08-20]

将内置 fallback 正弦合成器替换为自主拥有、纯 C++、零采样依赖的模态物理建模钢琴音源：
- **Phase 12（谐波钢琴 v1）**：8 分音谐波加法合成 `PianoSynthVoice`、velocity 响度/亮度双映射；
- **Phase 13（刚性失谐与模态耗散 v2）**：JOS PASP 刚性琴弦失谐公式（$f_m = m f_0 \sqrt{1 + B m^2}$）与 3 峰音板谐振器；
- **Phase 14（增强模态合成 v3）**：Magic Circle 递归振荡器（零 `std::sin`，单核 CPU ≤ 0.7%）+ 20/14/8/6 分音覆盖 + two-stage decay + 同音三弦微失谐拍频 + 8 峰音板主模态组（75~950 Hz）。

详细技术方案与逐项完成记录见 [`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)。

### Phase 15：UI 架构统一至 JIVE（声明式弹窗与设置面板重构） [v0.4.0 已发布，2026-08-20]

主窗口之外的手工像素排版与弹窗体系全面统一进 JIVE 声明式 UI 框架：
1. **通用 JiveModalDialog 基础设施**：以 JIVE ValueTree 模板驱动预设新建/重命名/删除弹窗及歌曲信息编辑弹窗；
2. **设置面板声明式重构（SettingsLayoutModel）**：JIVE CSS Grid（8 列 × 2 行）声明 16 通道跟随开关；
3. **模态操作与导出进度现代化**：`WavExportTask` 导出进度接入现代化 JIVE 暗黑 ProgressBar 声明式浮层。

详细技术方案与逐项完成记录见 [`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)。

### Phase 16：UI 性能优化（局部脏矩形重绘）与预设导入覆盖确认 [v0.4.0 已发布，2026-08-20]

1. **虚拟键盘脏矩形局部重绘（`CustomKeyboard`）**：引入 `repaintKey(k)` 与 `g.getClipBounds()` 区域相交快速早退裁剪，消灭密集 MIDI 播放时的全量 88 键 `repaint()`，UI 线程渲染负载降低 70% 以上；
2. **预设导入同名覆盖确认**：`PresetFlowSupport::handleImportPresetFile` 接入 `PresetConfirmDialog` 声明式覆盖确认对话框。

详细完成记录见 [`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)。

---

### Phase 17：真实物理打击感钢琴音源重构（Physical Strike & Non-linear Hammer） [v1.0.0 已发布，2026-08-23]

对标顶级物理建模钢琴（Pianoteq），重塑击弦打击感：
1. **消灭 $1/n$ 锯齿波拉弦感**：引入击弦点梳状滤波（$d/L \approx 1/8 \sim 1/14$）与非线性琴槌毛毡硬化截止谱；
2. **重塑真实打击物理起音**：消除 10ms 慢起音门控（Attack $\le 0.2\text{ ms}$ 极速起振），注入 $2\sim 3\text{ ms}$ 毛毡撞击物理瞬态冲击核（Hammer Strike Click）；
3. **强化双阶段衰减落差与音板共鸣**：提升早期快衰减权重至 $80\%\sim 88\%$，重构 8 峰云杉木音板模态并与 Resonance 动态绑定。

详细完成记录见 [`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)。

### Phase 18：88 键物理参数化与微观相位色散（Per-Note Voicing & Micro-Phases） [v1.0.0 已发布，2026-08-23]

消除 4 音区阶跃与 $t=0$ 相干波形：
1. **88 键连续物理参数映射（Bensa & Steinway B 实测标定）**：为 88 键建立连续刚度 $B$、击弦比 $d/L$、衰减 $\tau_{\text{slow}}$ 与单/双/三弦物理分区（`Piano88KeyTable.h`）；
2. **STFT 损失优化实测微相位表**：内联 $3 \times 64$ 最优初相矩阵，消灭狄拉克脉冲式波峰；
3. **空气黏性阻尼与 1.8kHz Bridge Hill 琴桥峰**：中频下凹歌唱性与中高音光泽感。

详细完成记录见 [`../archive/phase18-per-note-voicing-micro-phases.md`](../archive/phase18-per-note-voicing-micro-phases.md)。

### Phase 19：立体声音板共鸣箱与同音三弦微动力学 [v1.0.0 已发布，2026-08-23]

1. **16 峰物理云杉木音板模态组**：覆盖 48Hz~2250Hz 底箱呼吸模态、长琴桥耦合与各向异性散射模态；
2. **琴桥立体声空间辐射与非对称投影**：根据 88 键物理位置计算声像扩散，消灭单声道居中压迫感；
3. **同音三弦独立三振荡器非对称拍频**：中高音区三弦独立微失谐与 STFT 空间初相。

详细完成记录见 [`../archive/phase19-stereo-modal-soundboard.md`](../archive/phase19-stereo-modal-soundboard.md)。

### Phase 20：微观物理动力学（纵向波先驱声与击键混沌微扰） [v1.0.0 已发布，2026-08-23]

1. **低音钢弦纵向波先驱脉冲（Longitudinal Precursor Ping）**：依据 $v_L \approx 5100\text{ m/s}$ 为低音弦（MIDI 21~52）注入极速衰减的金属张力先导冲击；
2. **机械击弦混沌微扰（Micro-variation Jitter）**：为连续击打同一琴键赋予微秒级物理微扰，消除轮指机械感。

详细完成记录见 [`../archive/phase20-longitudinal-ping-micro-variation.md`](../archive/phase20-longitudinal-ping-micro-variation.md)。

### Phase 21：踏板交感共鸣与琴盖空间声学 [v1.0.0 已发布，2026-08-23]

1. **延音踏板全局交感共鸣弦池（Sympathetic Resonance Pool）**：12 半音基底谐振器响应 CC 64 踏板，注入全琴弦泛音交感振动；
2. **琴盖反射传递函数与木质近场微反射**：3 抽头近场微反射重现身临其境的空气深度。

详细完成记录见 [`../archive/phase21-sympathetic-resonance-lid-acoustics.md`](../archive/phase21-sympathetic-resonance-lid-acoustics.md)。

### Phase 22：物理声学极致深化与机械拟真 [v1.0.0 已发布，2026-08-23]

1. **制音器落弦与琴键释放机械瞬态（Damper Felt Fall）**：$80\sim 150\text{ Hz}$ 制音器落弦低频闷击声；
2. **琴盖开合度声学传递函数（Full / Half / Closed）**：不同开合角度的多级高频滚降与反射矩阵；
3. **长短琴桥断裂交界音色补偿（Bridge Break）**：针对 MIDI 43~44（G2/G#2）琴桥交界的弦长与刚度台阶式跳变；
4. **强击非线性微音高漂移与软饱和**：$fff$ 强击瞬间 $2\sim 5$ 音分音高瞬态上浮与软饱和；
5. **未踩踏板单键开放弦交感共鸣**：按住低音键弹奏高音触发的开放弦局部交感。

详细完成记录见 [`../archive/phase22-physical-modeling-acoustic-refinement.md`](../archive/phase22-physical-modeling-acoustic-refinement.md)。

### Phase 23：大师级音色校准与 Pianoteq 对齐精调 [v1.0.0 已发布，2026-08-23]

1. **动态琴槌非线性刚度与击弦点几何陷波**：三层毛毡动力学压实、动态接触时间 $T_c$ 与速度相关滚降指数；
2. **同音三弦立体声非对称微失谐与声相展开**：Mid-Side 差分多弦立体声展开模型；
3. **云杉木音板低通截止与木质腔体共鸣峰配平**：$4.2\text{ kHz}$ 云杉木纤维内耗低通滤波器；
4. **起音瞬态裂音与低音纵波微调**：前 $3\text{ ms}$ 高频冲击裂音 (HF Crack) 与紧凑型低音纵波先导声。

详细完成记录见 [`../archive/phase23-master-voicing-realism-calibration.md`](../archive/phase23-master-voicing-realism-calibration.md)。

### Phase 24：生命力与非线性动力学绽放 [v1.0.0 已发布，2026-08-23]

基于全物理有限元与耦合 PDE 声学机理：
1. **泛音时间滞后膨胀与绽放（Harmonic Blooming）**：中高力度高阶分音非线性能量泵浦与上升绽放（$10\sim 25\text{ ms}$）；
2. **琴槌接触微阻尼与脱离物理释放（Hammer Contact-Release Dynamics）**：消灭 $t=0$ 正弦波机械突兀开门感；
3. **动态声场空间漫射（Dynamic Spatial Diffusion）**：从击打点声源平滑漫射为音板面声源包围场。

详细完成记录见 [`../archive/phase24-vitality-and-dynamic-blooming.md`](../archive/phase24-vitality-and-dynamic-blooming.md)。

---

## 3. 后续阶段路线图（Post-v1.0.0 Roadmap）

在完成 v1.0.0 正式里程碑后，devpiano 进入 **Post-v1.0.0 平台拓展与高阶能力演进** 阶段：

### Phase 25：Linux 原生桌面构建与音频驱动适配（Linux Desktop & Audio Path Exploration） [已完成，2026-08-25]

1. **Phase 25-A（已完成）**：ALSA / JACK 音频驱动链路验证与设备管理机制审查，含 Linux 音频设备诊断单元测试（`AudioDeviceDiagnosticsLinuxTest`）；
2. **Phase 25-B（已完成）**：X11 / XCB 窗口系统与 JIVE 渲染适配——字体回退链、窗口原子映射、焦点管理（失焦 panic 不打断 MIDI 回放）等修复均经 CachyOS 实机交互回归验证通过；
3. **Phase 25-C（已完成）**：Linux Release 构建兼容性与分发规范——依赖查证结论：动态依赖（ALSA / fontconfig / freetype）soname 稳定且桌面发行版标配，**不做静态化**；真实门槛是构建环境 glibc / libstdc++ 版本，故正式产物由 GitHub Actions `release.yml` 的 `release-linux-x64` job 在 **`ubuntu-24.04` runner**（glibc 2.39）构建，支持矩阵为 glibc ≥ 2.39（Ubuntu 24.04+ / Debian 13+ / Fedora 41+ / Arch 系），配套门槛检查脚本 `scripts/check_linux_glibc_floor.sh` 与 `DevPiano-vX.Y.Z-linux-x64.tar.gz` + `.sha256` 归档规范；
4. **Phase 25-D（已完成）**：`ci.yml` 合并 Debug 测试与 Release 构建为单一 `linux-gate` job（ubuntu-24.04 共享 ccache，Debug 测试 + Release 构建/测试 + 门槛检查；Windows 门禁补 Release 构建验证）并扩展 `package_release.sh` 支持 `--linux` 打包选项（tar.gz + sha256，打包前自动执行 glibc 门槛检查）；
5. **Phase 25-E（已完成）**：三闸门基线验证（CI 全绿）、Linux 专项冒烟测试清单（CachyOS 2026-08-24 实机验证通过）与指南文档对齐（`release-workflow.md` 新增 §5A Linux 手工冒烟测试与双平台发布流程）。

> 基础设施已落地：`.github/workflows/ci.yml`（格式门禁 + `linux-gate` Debug 测试/Release 门槛 + Windows MSVC Debug/Release 构建测试门禁）、`.github/workflows/release.yml`（Tag 触发 Windows/Linux 双平台自动打包发布）与 `.github/workflows/pr-agent.yml`（PR-Agent AI 代码审查，DeepSeek v4 Flash）。当前子任务排期与验收状态见 [`current-iteration.md`](current-iteration.md)。

详细完成记录见 [`../archive/phase25-linux-desktop-and-audio-path.md`](../archive/phase25-linux-desktop-and-audio-path.md)。

### Phase 26：MIDI 多轨并轨与综合时间线合并（MIDI Multi-Track Timeline Merge） [已完成，2026-08-29]

1. **`MidiTrackMergeEngine` 多轨时间线精准合并内核**：实现统一多轨合并引擎，支持跨音轨 Tempo/Conductor、Meta、CC 与 Note 事件按绝对时间戳（`timestampSamples`）精准稳定归并；
2. **多轨通道智能策略与元数据解析**：支持原始通道保持（Pass-through）与音轨转通道自动重映射（Track-to-Channel Auto-Assignment），提取并整合乐曲标题、音轨名、Tempo Map 与调号拍号；
3. **16 通道矩阵与虚拟键盘综合回放联动**：16 通道矩阵对各轨独立移调/加权/静音控制，88 键虚拟键盘多音轨多着色高亮联动；
4. **全轨 WAV 离线渲染与多轨测试套件全覆盖**：支持全轨合并流直接离线导出高质量 WAV 音频（维持只读 Playback Take 契约），覆盖 Type 0 / Type 1 复杂多轨夹具。

### Phase 27：现实物理演奏交互与声学控制（Physical Voicing & Realistic Acoustic Interaction） [规划中]

1. **琴盖开合度交互式控制（Lid Position）**：在 JIVE UI 界面接入 Full Open / Half Stick / Closed 3 级琴盖开合切换与底层 `lidAcoustics` 声学传递函数实时生效；
2. **弱音/移位踏板物理拟真（Una Corda / Soft Pedal，CC 67）**：模拟击弦机右移 3 弦敲 2 弦与毛毡侧向软化物理机理，支持 CC 67 踏板与 UI 软踏板点亮；
3. **触键力度曲线（Touch Velocity Curve）**：支持 Standard / Light / Heavy / Wide Dynamic 4 种按键阻尼手感映射与动态调节；
4. **配置持久化与预设系统联动**：将琴盖开合度、Una Corda 状态与触键曲线完整纳入 `SettingsModel`、`SettingsSerialization` 与 Performance Preset 序列化。
---

## 4. 主要风险与应对

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 插件生命周期复杂 | 中 | 维护专项生命周期测试，重点覆盖 editor、卸载、重扫、退出。 |
| 键盘映射边界多 | 低 | 基础映射已全量验证；Performance Preset 已补充专项回归清单。 |
| 物理建模高负荷极端情况 | 极低 | 逐采样零三角函数 + 动态分音剪枝，8 复音齐奏单核 CPU $\le 0.7\%$。 |
| JIVE API 稳定性 | 极低 | 固定 git commit hash；持续维护单元测试回归。 |
| `MainComponent` 职责回流 | 低 | 当前 ~1310 行（`initialiseUi()` 承载 JIVE 树构建与回调接线），UI 布局仍完全下沉至 JIVE 与独立 Controller；持续监控，避免业务逻辑回流。 |
| 文档状态漂移 | 极低 | 本文件作为唯一 roadmap；当前任务只写入 [`current-iteration.md`](current-iteration.md)。 |

---

## 5. 完成标准与功能参考

- 阶段性验收标准见：[`../reference/acceptance.md`](../reference/acceptance.md)
- 核心功能参考与测试：
  - [`../reference/features/builtin-piano-synthesis.md`](../reference/features/builtin-piano-synthesis.md)（7 大声学系统全物理建模钢琴）
  - [`../reference/features/declarative-ui-and-theming.md`](../reference/features/declarative-ui-and-theming.md)（JIVE 声明式 UI）
  - [`../reference/features/midi-channel-matrix.md`](../reference/features/midi-channel-matrix.md)（16 通道 MIDI 矩阵）
  - [`../reference/features/per-key-customization.md`](../reference/features/per-key-customization.md)（逐键自定义）
  - [`../reference/features/internationalization.md`](../reference/features/internationalization.md)（运行时中英文双语）
  - [`../reference/features/keyboard-mapping.md`](../reference/features/keyboard-mapping.md)（电脑键盘稳定映射）
  - [`../reference/features/performance-presets.md`](../reference/features/performance-presets.md)（预设管理）
  - [`../reference/features/recording-playback.md`](../reference/features/recording-playback.md)（演奏录制回放）
  - [`../reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)（MIDI 导入）
  - [`../reference/features/performance-persistence.md`](../reference/features/performance-persistence.md)（原生演奏文件）
  - [`../reference/features/plugin-hosting.md`](../reference/features/plugin-hosting.md)（VST3 插件宿主）
  - [`../reference/features/plugin-offline-rendering.md`](../reference/features/plugin-offline-rendering.md)（WAV 离线渲染）
  - [`../reference/features/fixture-inventory.md`](../reference/features/fixture-inventory.md)（测试夹具清单）

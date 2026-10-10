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

**近期重点：Phase 39 可复核声学基线与三力度整音校准** [规划]。先复核外部研究的量测语义和参数可信边界，再校准逐键非谐性与连续力度整音，完成状态、生产声音及实时成本闭环；详细任务与直接验收见 [当前迭代](current-iteration.md#2-phase-39可复核声学基线与三力度整音校准)。本次为文档排期，不表示新声学能力已实现。

**最近软件交付：Phase 38 电脑键盘分区、三踏板与固定双层** [软件交付完成，2026-10-10]；Phase 37 [已完成，2026-10-08]。原任务、直接消费者、失败记录和后续修复归属见 [Phase 37/38 完成归档](../archive/phase37-38-piano-calibration-and-keyboard-performance.md)。CPU SLA 与商业厂商/物理硬件补验不由 Debug 门禁或本次归档认证；Phase 36 归档与 AUDIT-004 实机边界保持独立。

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

MIDI 文件导入、音轨解析、回放、虚拟键盘可视化、最近路径记忆、主窗口尺寸自适应与恢复；后续已由 Phase 26 升级为全轨并轨，不再提供选轨模式。

功能与测试文档：[`../reference/features/midi-file-import.md`](../reference/features/midi-file-import.md)。

---

### Phase 5：架构收敛与 MainComponent 瘦身 [v0.2.0 已发布，2026-07-19]

`MainComponent.cpp` 收敛为顶层装配、生命周期与回调接线职责。
提取 `RecordingSessionController` / `PluginOperationController` / `SettingsWindowManager` / `AppStateBuilder`。

详细完成记录见 [`../archive/phase5-architecture-convergence.md`](../archive/phase5-architecture-convergence.md)。

### Phase 6：功能补齐——钢琴键盘、MIDI 矩阵、持久化、调速与 GUI 设置 [v0.2.0 已发布，2026-07-19]

自定义钢琴键盘（`CustomKeyboard`，支持 3 种着色 / 3 种音符显示模式）、16 通道 MIDI 矩阵（`ChannelMatrix`）、note-only 绑定编辑器。
演奏文件持久化、播放速度控制（0.5x–2.0x 原子变速）、MIDI 导入增强（CC/pitch bend/program change）、Diagnostics 层、测试夹具库。
5 项 GUI 设置控件（colourMode / noteDisplay / fadeSpeed / resizable / instrumentFilter toggle）。

详细完成记录见 [`../archive/phase6-7-completion-detail.md`](../archive/phase6-7-completion-detail.md)。

### Phase 7：VST3 离线渲染与国际化 [v0.2.0 已发布，2026-07-19]

VST3 离线渲染（WAV 导出 + `ExportDialog` 进度）、播放速度精确控制（Slider + atomic 线程安全）、拖放文件支持、运行时中英文语言切换（JUCE `Translation`）。
Phase 7-5（Metadata 编辑对话框）当时搁置，后续已由 Phase 15 的 `JiveModalDialog` 实现。
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

JIVE 声明式 UI 框架（`juce::ValueTree` 布局 + JSON 样式表 + Flex/Grid 自适应）替代主窗口各面板的硬编码 `setBounds()` 布局；当时使用的 melatonin_inspector 运行时检查器后来已退役（ADR-013），JIVE 也已内化（ADR-014）。`design_tokens.json` 统一样式来源，原生键盘与 ADSR 曲线通过组件工厂注入，业务逻辑与排版解耦。

详细计划与完成记录见 [`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)。

### 全面代码质量审计 (AUDIT-001) [v0.3.0 已发布，2026-08-16]

代码质量审计（`AUDIT-001`，2026-08-16）登记问题按审计归档闭环；三闸门与 win-build 通过，全量源码文件 clang-tidy 无诊断。消除音频回调堆分配与延迟 prepare、修复 `masterGain` 跨线程数据竞争，并提取公共离线渲染管线 `RenderPipeline`。

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

> 基础设施已落地：`.github/workflows/ci.yml`（格式门禁 + `linux-gate` Debug 测试/Release 门槛 + Windows MSVC Debug/Release 构建测试门禁）、`.github/workflows/release.yml`（Tag 触发 Windows/Linux 双平台自动打包发布）与 `.github/workflows/pr-agent.yml`（PR-Agent AI 代码审查，配置以工作流文件为准）。后继迭代排期与直接验收见 [`current-iteration.md`](current-iteration.md)，本阶段完成记录见下方归档。

详细完成记录见 [`../archive/phase25-linux-desktop-and-audio-path.md`](../archive/phase25-linux-desktop-and-audio-path.md)。

### Phase 26：MIDI 多轨并轨与综合时间线合并（MIDI Multi-Track Timeline Merge） [已完成，2026-08-29]

1. **`MidiTrackMergeEngine` 多轨时间线精准合并内核**：实现统一多轨合并引擎，支持跨音轨 Tempo/Conductor、Meta、CC 与 Note 事件按绝对时间戳（`timestampSamples`）精准稳定归并；
2. **多轨通道智能策略与元数据解析**：支持原始通道保持（Pass-through）与音轨转通道自动重映射（Track-to-Channel Auto-Assignment），提取并整合乐曲标题、音轨名、Tempo Map 与调号拍号；
3. **MIDI 通道与虚拟键盘综合回放联动**：合并后的音符保留或按导入策略分配 MIDI 通道，回放时虚拟键盘同步高亮；通道的 `followKey` 掩码决定是否应用全局播放移调，不提供音轨级静音或混音编辑；
4. **全轨 WAV 离线渲染与多轨测试套件全覆盖**：支持全轨合并流直接离线导出高质量 WAV 音频（维持只读 Playback Take 契约），覆盖 Type 0 / Type 1 复杂多轨夹具。

### Phase 27：JUCE 9.0.1 框架升级、UI 基础设施内化与全平台生态演进（JUCE 9.0.1 Framework Upgrade & Internalized UI Governance） [已完成，2026-09-02]

1. **框架升级与构建基线更新（Phase 27-A）**：`submodules/JUCE` 升级至 JUCE 9.0.1（`e18f7f5`），工具链、CMake 选项与文档版本基线对齐；
2. **非 UI 领域 Breaking Changes 适配（Phase 27-B）**：适配 VST3 宿主与 `AudioPluginInstance` 生命周期，适配流式音频导出及现代音频设备管理；
3. **UI 基础设施与 JIVE 依赖治理（Phase 27-C）**：依据 ADR-014 彻底注销并退役 `submodules/JIVE` 外部子模块，内化核心声明式 UI 运行时与 CSS Grid 至 `source/UI/jive/core/`，全面完成 `FontOptions`、`GlyphArrangement` 与 `DrawableComponent` 现代排版渲染迁移；
4. **内化代码质量治理与全量 CI 门禁纳入（Phase 27-D）**：内化 UI 代码完成 C++20 规范现代化（`override`、`noexcept`、`const-ref`），移除静态分析豁免，与业务代码统一享有零警告检验；
5. **全系统功能回归、三闸门闭环与发布打包（Phase 27-E）**：核心引擎、物理声学与 UI 自动化测试通过，Windows MSVC 验证构建与分发打包通过，GitHub Actions 门禁通过后合入 `main`。

详细完成记录见 [`../archive/phase27-juce9-upgrade-and-ui-internalization.md`](../archive/phase27-juce9-upgrade-and-ui-internalization.md)。

### Phase 28：Devpiano 声明式 UI 基础设施深度治理与接口冻结（Declarative UI Infrastructure Governance & API Freeze） [已完成，2026-09-03]

1. **API 边界收敛与 ViewHost 门面构建（Phase 28-A）**：封装 `ViewHost`，彻底隔离业务代码对底层 `Interpreter` / `GuiItem` 的裸露直接依赖与析构 UAF 风险 [已完成，2026-09-03]；
2. **全量声明式 UI 布局金标测试（Phase 28-B）**：构建全应用 ValueTree 解释烟测、典型分辨率几何尺寸断言与焦点/滑音回归测试套件 [已完成，2026-09-03]；
3. **通用死重清理与规范化命名规整（Phase 28-C）**：剔除 JIVE 内嵌单测与孤立算法死代码，统一宏前缀（`DEVPIANO_UI_*`）与命名空间（`devpiano::ui`）[已完成，2026-09-03]；
4. **代码审查闭环与 UI 基础设施接口冻结（Phase 28-D）**：双端双配置三闸门闭环，正式确立 UI Infrastructure Freeze 冻结公约，研发重心全面重归物理建模算法 [已完成，2026-09-03]。

详细完成记录见 [`../archive/phase28-ui-governance-and-api-freeze.md`](../archive/phase28-ui-governance-and-api-freeze.md)。

### Phase 29：现实物理演奏交互与声学控制（Physical Voicing & Realistic Acoustic Interaction） [已完成，2026-09-12]

1. **琴盖开合度声学交互与 UI 穿透（Phase 29-A）**：在 JIVE UI 界面接入 Full Open / Half Stick / Closed 3 态直观选择，无缝驱动底层已实现的 `lidAcoustics` 多级高频滚降与近场反射，兼顾布局金标测试保护 [已完成，2026-09-12]；
2. **弱音/移位踏板物理拟真与状态联动（Phase 29-B）**：在 `PianoSynthVoice` 中模拟三角钢琴击弦机整体右移、3 弦敲 2 弦与毛毡侧面软化的物理机理，支持 MIDI CC 67 踏板信号、电脑键盘快捷触发与 UI 软踏板状态点亮 [已完成，2026-09-12]；
3. **触键力度曲线自适应映射（Phase 29-C）**：在 `KeyboardMidiMapper` / 输入层提供 Standard（线性）、Light（轻触感）、Heavy（重阻尼）、Wide Dynamic（宽动态 S 曲线）4 种手感映射，自适应薄膜/机械键盘及 MIDI 键盘 [已完成，2026-09-12]；
4. **声学配置持久化与预设系统全量联动（Phase 29-D）**：将琴盖开合度、Una Corda 默认态与触键曲线完整纳入 `SettingsModel`、`SettingsStore` 与 Performance Preset（`.devpiano.preset` JSON）序列化，确保向后兼容 [已完成，2026-09-12]；
5. **声学精调、三闸门闭环与构建验证（Phase 29-E）**：声学与演奏交互自动化测试通过，三闸门合规，双平台编译与打包验证，实机演奏手感与声学回归 [已完成，2026-09-12]。

详细完成记录见 [`../archive/phase29-physical-voicing-and-acoustic-interaction.md`](../archive/phase29-physical-voicing-and-acoustic-interaction.md)。

### Phase 30：历史调律体系与基准音高校准（Historical Temperaments & Reference Pitch Calibration） [已完成，2026-09-13]

1. **古典历史调律与平均律拓展**：支持十二平均律（Equal Temperament）、纯律（Just Intonation）、毕达哥拉斯律（Pythagorean）、中庸全音律（Meantone 1/4 comma）、魏克迈斯特律（Werckmeister III）、基恩伯格律（Kirnberger III），在物理弦模态基频生成链路上实现微音分高精度映射；
2. **A4 基准音高校准**：支持 415.0 Hz（巴洛克古典）、432.0 Hz（维尔第调律）、440.0 Hz（现代标准）、442.0 Hz（交响乐团）无级微调；
3. **裁剪与非目标**：依据项目定位裁剪外部 Scala (.scl/.kbm) 文件解析，坚守内置经典律制与纯自包含免安装绿色原则。

详细完成记录见 [`../archive/phase30-32-temperaments-spatial-mechanics.md`](../archive/phase30-32-temperaments-spatial-mechanics.md)。

### Phase 31：多视角空间声学与算法混响（Multi-Perspective Spatial Acoustics & Algorithmic Room Modeling） [已完成，2026-09-13]

1. **多视角立体声场（Player vs Audience Perspective）**：演奏者主观视角（宽立体声、左低右高）与观众/音乐厅远场客观反转视角的无爆音无锁平滑切换；
2. **轻量数学算法房间混响网络**：内置 Studio（0.6 s）、Chamber（1.5 s）、Concert Hall（2.4 s）三大经典空间预设，基于互质低通梳状滤波阵列与全通漫射矩阵，零外部采样依赖；
3. **JIVE 声学面板集成与预设联动**：在设置界面提供视角切换、空间模式选择器与混响电平滑块，并全面打通 SettingsStore 与 PerformancePreset 序列化及简体中文国际化。

详细完成记录见 [`../archive/phase30-32-temperaments-spatial-mechanics.md`](../archive/phase30-32-temperaments-spatial-mechanics.md)。

### Phase 32：机械物理噪声与琴体微衰退拟真（Mechanical Action Noise & Physical Imperfection） [已完成，2026-09-13]

1. **延音踏板机械气流与箱体共鸣冲击（Pedal Whoosh & Resonance Shock）**：MIDI CC 64 捕获、带通塑形白噪气流微啸短脉冲（踩下中心 ~1350 Hz / 抬起 ~950 Hz）、双模态低频共鸣冲击（58/116 Hz）与开放弦交感微扰，踩下/抬起速度自适应冲激强度；
2. **离键抬起与制音器落弦瞬态深化（Damper Drop Thump & Key Release）**：离键速度自适应毛毡摩擦持续时间与衰减速率、毛毡纤维高频摩擦微噪声（~2800 Hz）、木质键体落床轻撞声（140/270 Hz 双模态、全 88 键）与琴弦 ADSR 动态释放时间联动；
3. **琴槌毛毡微老化与调音离散度（Inharmonicity Jitter & Felt Ageing）**：确定性逐键哈希的基频微失谐（±0.3~1.2 cents）、不谐和刚度 B 离散（±4.5%）与逐键毛毡硬度/明暗偏置（默认 0.0 保护纯净基线）；
4. **全栈集成**：JIVE 声学卡片新增机械噪声与毛毡老化滑块，完整打通 `SettingsStore` / `PerformancePreset` / 离线 WAV 导出与简体中文国际化。

详细完成记录见 [`../archive/phase30-32-temperaments-spatial-mechanics.md`](../archive/phase30-32-temperaments-spatial-mechanics.md)。

### Phase 33：可观测性加固与生产级诊断基础设施（Observability Hardening & Production-Grade Diagnostics Infrastructure） [已完成，2026-09-14]

1. **双通道日志基础设施（DevPianoLogger Dual-Sink）**：写入系统应用目录并转发完整 debugger 消息；Phase G 已将构造时裁剪升级为会话内活动/单备份合计 512 KiB 的有界轮转，故障停用文件 sink 并保留原因。析构注销顺序保留退出期设置保存与插件卸载日志；
2. **设置界面诊断卡片直达与系统文件管理器联动**：在设置界面诊断卡片中新增操作行与“打开日志目录”按钮（`open-log-dir-button`），点击调用 `juce::File::revealToUser()` 调起系统原生文件管理器并高亮选中 `devpiano.log`；动态在诊断文本框中显示日志文件绝对路径与当前大小；
3. **MidiTrace 与诊断测试防线**：覆盖 NoteOn/Off、CC、PitchBend、ProgramChange；原始力度 0..127 不二次缩放，保留零力度 MIDI 语义。测试验证预算、UTF-8 边界、轮转故障及析构注销，不钉会话文案；
4. **运行时字符编码断言消除**：彻底修复历史遗留的 5 处多字节 em-dash 字符字面量，消除 `juce_String.cpp:327` 的运行时断言。

详细完成记录见 [`../archive/phase33-observability-and-diagnostics-infrastructure.md`](../archive/phase33-observability-and-diagnostics-infrastructure.md)。

### AUDIT-003 专项：全面代码质量审计缺陷消除与架构对齐（Code Quality Remediation & Architecture Alignment） [已完成，2026-09-15]

基于 2026-09-15 完成的 `AUDIT-003` 全面代码质量审计（`A-` 评级，6 项登记缺陷全部闭环），开展专项闭环治理：
1. **测试消息循环驱动与断言消除（Phase A / TEST-001）**：解决 Linux 无头单测 socket 管道溢出告警；
2. **插件离线导出房间混响对齐（Phase A / QUAL-001）**：补齐 `PluginOfflineRenderer` 混响浸润处理；
3. **底层 Core 单向拓扑恢复（Phase B / ARCH-001）**：解耦 `AppState.h` 对上层 `SettingsModel` 与 `ChannelMatrix` 的反向包含；
4. **解码内存优化与文档对齐（Phase B & C / PERF-001, DOC-001, DOC-002）**：优化 `MidiTextDecoder` 临时缓冲区分配，更新架构文档与预设规范。

详细完成记录见 [`../archive/audit-003-code-quality-fix-phases.md`](../archive/audit-003-code-quality-fix-phases.md)。

### Phase 34：键盘演奏交互质变与演奏表现力增强 (Keyboard Performance UX & Expressive Control) [已完成，2026-09-23]

基于专用钢琴演奏宿主定位，全面重塑电脑键盘演奏的人机交互与表现力：
1. **QWERTY Visualizer（5 行 Performance Map 声明式卡片与 12-TET 和声色彩投影）**：JIVE 声明式 5 行 ANSI 物理网格卡片（`QwertyComponent`），击键下沉与 50fps 荧光余晖动画；12-TET 和声色环算法（`MusicTheory.h`）驱动三和弦几何色相投影，4 种按键着色模式；
2. **Layout Group 轻量多键组与发音身份快照（Note-off Identity Preservation）**：单预设 4 组键位配置（Group A~D），反引号键（`）或 UI 按钮秒级切换；`HeldKeyIdentity` 锁定按键发音快照，切组/移调彻底杜绝悬挂音；
3. **SustainPolicy 与 Sample-Accurate 事件级 Sync 切分踏板**：音频块内部采样精确调度 $\text{CC64}(0) \to \text{NoteOn} \to \text{CC64}(127)$，消除空格键踩放断音空洞，杜绝线程 Sleep；
4. **PerformanceModifierState 瞬态 Press 修饰符**：Shift 力度拉满（Velocity Boost）、Alt 高八度平移（+8va），纯事件流变换零全局配置污染，UI HUD 实时标签；
5. **扫描器增量持久化（Crash-safe State Persistence）与乐器端点概念收敛**：插件扫描逐项即时持久化，dead-man's pedal 崩溃点记录与黑名单推迟；`InstrumentEndpoint` 统一乐器抽象，解耦设备准备、实时发声与离线渲染；
6. **跨平台实现深度收敛与 JUCE 9 原生框架利用全面升级（Phase 34-F）**：拔除主窗口 Win32 `WNDPROC` Hook、`AttachThreadInput` 与 `<windows.h>` 特化，全平台统一基于 JUCE 9 原生事件；`WavExportTask` 完全异步化（`startAsync`），主应用目标不再定义 `JUCE_MODAL_LOOPS_PERMITTED=1`（测试目标仍保留）；C++ 字符串字面量遵守 Strict 7-bit ASCII 约束；`createLegalFileName` 替换自造文件名过滤轮子，运行时配置目录统一为 `DevPiano`。

详细完成记录见 [`../archive/phase34-keyboard-performance-ux-and-expressive-control.md`](../archive/phase34-keyboard-performance-ux-and-expressive-control.md) 与 [`../archive/cross-platform-and-juce9-convergence.md`](../archive/cross-platform-and-juce9-convergence.md)。

### Phase 35：键盘演奏表现力深水区与练琴基础设施（Keyboard Expressive Dynamics & Practice Infrastructure）[已完成，2026-09-28]

聚焦于电脑键盘演奏中最核心的体验痛点——缺乏节奏基准工具、打字机式死板力度、缺乏实时乐理反馈以及缺少伴奏循环跟练手段：
1. **无锁采样级音频节拍器与视觉节拍指示（Phase 35-A）**：确定性采样级 Click Engine（强拍 1600Hz / 弱拍 800Hz，6/8 第四拍次重音 1100Hz，零外部采样依赖）、2/4、3/4、4/4、6/8 拍号、40~280 BPM 无级可调与 Tap Tempo 连续测速、状态栏节拍反馈与预备拍（Count-in）；
2. **打字击键动态力度与人性化微扰引擎（Phase 35-B）**：基于物理击键间隙 $\Delta t$ 的律动速度估算器（`TypingCadenceEstimator`，快弹与慢按力度分层）、确定性哈希微扰（`VelocityHumanizer`）及持久化的基础力度基线设置；关闭动态估算时保留绑定力度，静音绑定仍静音。当前 UI 未提供力度数值 HUD 或动态参数编辑控件；
3. **实时和弦识别与乐理分析 HUD（Phase 35-C）**：`MusicTheory.h` 根据按下音符的 Pitch Class Set 识别三和弦、七和弦、挂留和弦及转位低音；QWERTY 卡片标题 `qwerty-chord-badge` 与 `QwertyComponent` 内部 HUD 展示结果，状态栏保留节拍和音频信息；
4. **MIDI 伴奏 A-B 片段循环跟练与进度自由跳转（Phase 35-D）**：走带时间轴精细进度条（`TimelineBar`）与零爆音 Seek 机制、难点小节 A-B 无缝循环引擎（`AbLoopEngine`），配合 0.5x~2.0x 调速闭环键盘练习流。

Phase 35 原计划与完成勾选见 [完成计划归档](../archive/phase35-keyboard-expressive-dynamics-and-practice-infrastructure.md)。AUDIT-004 后续反证及修复分别见 [审计复审](../audit/AUDIT-004-code-quality-audit-2026-10-02.md) 与 [软件实施归档](../archive/audit-004-code-quality-fix-phases.md)；不回写 Phase 35 历史勾选。

### AUDIT-004 Phase：代码质量缺陷修复与消费者契约闭环 [软件实施已完成并归档，2026-10-05；实机补验保留]

[AUDIT-004](../audit/AUDIT-004-code-quality-audit-2026-10-02.md) 保留首次基线、原问题身份与优先级，并按完整实施证据追加复审。Phase 0/A/B 完成安全验证、文件保护与准入；Phase C/D 完成所有权、身份与 Transport；Phase E 按用户批准的分层边界完成 UUID/快照与实时闭包；Phase F/G 完成映射、交互、诊断与工程门禁；Phase H 对齐现行契约并完成 Windows 软件集成验收。所有原实施任务有直接证据，不等于第三方框架的锁/分配已消失或全部实机组合已认证。

1. **Phase 0（前置，已完成）**：音频测试由调用者绑定 live buffer，文件测试使用 ScopedTempDir，Chord 纳入默认 DevPiano/Core；Windows Debug 禁 NRVO 的默认测试、真实音频/文件消费者及用户目录无副作用验证通过，证据见实施归档。
2. **Phase A/B（已完成）**：已有文件事务、预设身份、Take 绑定与设置快照已闭环；原生/MIDI 完整准入、稳定时间线、拍号边界及输出前数值检查已通过 Windows Debug 默认测试、受限子进程、实际文件/播放/seek/拖放信息界面验证。输入格式不变，不以修改测试数值掩盖通用合成时间域失败。
3. **Phase C/D（已完成）**：原生 VST3 与实际生命周期/退出、活动命令已验证；播放原身份 FIFO、物理持有、暂停捕获配对、末尾 WAV 对齐、48k↔44.1k 时间域、16 通道目的状态、完整预备拍及通道柔音已通过生产消费者。
4. **Phase E（已完成，2026-10-04）**：预设 UUID 永久身份、v3 Take 内嵌可执行快照、采样点实时/离线同构、无锁发声、零库函数三角、SPSC 输入与消息线程视觉分发闭环。产品自有实时契约与第三方 JUCE VST3 框架限制按用户批准的分层验收分别记录。
5. **Phase F（已完成，2026-10-05）**：最终映射单一投影、配置输入与显示输出隔离、绑定/逐键定制跨几何保持、静音优先级、有界余晖、即时圆角、Notes 多行确认/取消、最低 MIDI 八度及 A4 400.0–480.0 Hz 已按实际消费者验证；不改原预设/演奏格式，不把瞬态演奏变换写回配置。
6. **Phase G（已完成，2026-10-05）**：日志预算和 MIDI 数值真实；业务头禁聚合头，样式刷新/内置 modal 完全经 ViewHost；删除译文、手工回调和赋值回读 oracle，默认 lifecycle 已执行；Windows Debug 构建、默认测试及实际全量 tidy 零项目诊断。失败输入与新树替代结果分开保留，真实 UI/日志/文件消费者证据见实施归档。
7. **Phase H（已完成，2026-10-05）**：预设普通选择/录制调号、rename 确认、v3/旧格式准入、MIDI 单轨及跨轨 Tempo Map、日志与插件取消/实时边界已按实际消费者对齐；原项与证据经固定集合核对，现行验收建立复审入口。Windows Debug/默认单测/格式与全量静态门禁按实际结果记录；实机未验证组合不勾选通过。

全部原项归属、优先级、EVID 与完整复建输入见 [AUDIT-004 实施归档](../archive/audit-004-code-quality-fix-phases.md)，当前问题状态只在 [审计第 8 章](../audit/AUDIT-004-code-quality-audit-2026-10-02.md#8-附录问题总表登记表) 维护；保留首次 S01～S29 反证和历史门禁失败，追加复审不抹去初审。剩余实机矩阵见 [acceptance](../reference/acceptance.md#audit-004-当前复审入口与契约边界)；用户授权归档软件记录，不以硬件未测阻塞历史归档，也不把归档当作全平台放行。

### 本地化完整消息模板收口 [代码迁移已提交]

在已有预设弹窗缺词条修复之上，按 [ADR-015](../decisions/ADR-015-localized-message-templates-and-punctuation.md) 将相关自然语言消息切换为完整 ASCII 英文模板和单参数 `{0}` 替换；中文文案分类使用标点，技术表达及用户名称/路径保持原样。不引入新格式化框架，不把所有 `+` 视为缺陷，不改变 CRUD、UUID、持久化或音频时序。

本地化软件迁移见 `de450e7`、`f408262`：预设删除／覆盖确认、相关成功提示和同类单参数消息使用整句模板；参数原样插入，旧碎片键清理。多参数统计及后继 ADR 边界继续按 ADR-015 管理；当前弹窗迭代补充实际窗口尺寸、长消息排版与操作区验收，不据此认证所有国际化场景。

### 自有弹窗尺寸统一与全局字体规范化 [已完成，2026-10-06]

原生标题栏模式确定后准确定尺并居中；`ViewHost::fitToContent()` 封装宽度约束及最终内容边界测量，预设、绑定两状态、歌曲信息和导出进度共用 28 逻辑像素操作区留白。对齐 LookAndFeel 菜单项字体，全面消除裸 `FontOptions` 硬编码，统一接入 `DesignTokens::getUnifiedUiFont` 保证中文字体族一致性（代码提交见 `829a40b`、`182196d`、`80dbd19`）。

### Phase 36：开发期减负与历史兼容性收敛（Development Overhead Reduction & Legacy Compatibility Deprecation）[已完成，2026-10-07]

开发期减负与 YAGNI 协作契约已固化于 `AGENTS.md`，自有格式已切换为当前预设 v2 / 演奏 v3，历史数据迁移分支与死代码已彻底清除，低价值测试已按行为风险完成剪枝。现行 13 本特性文档、架构说明、验收标准及指南已全面对齐当前代码事实，系统性消除了硬编码易变统计度量（断言数/用例数/代码行数/编译秒数），各分册中的重复“当前状态”与历史阶段括号已统一收敛指向 roadmap，严格维持历史审计与归档原貌不变。Task 36-1 至 36-5 闭环完成，完整任务与分步验证记录见 [Phase 36 完成归档](../archive/phase36-development-overhead-reduction-and-legacy-compatibility-deprecation.md)。

### Phase 37：物理建模调律与被动共鸣校准（Piano Tuning & Passive Resonance Calibration）[已完成，2026-10-08]

深化现有增强模态钢琴，以实际频率、被动共鸣、音色与实时成本为验收对象，不以“声学巅峰”或与商业产品微观机理一致作为交付承诺：

1. **Task 37-1：频率语义与声学基线**：区分名义柔弦频率、实际第一分音、刚度不谐和度与同音弦微失谐，核对参数来源并建立真实渲染基线；
2. **Task 37-2：受约束的八度拉伸调律**：独立于律制实现关闭/默认拉伸，锚定实际 A4 第一分音，按明确的分音匹配策略校准，不承诺消除全部音程拍频；Sine 与 VST3 不自动套用钢琴拉伸；
3. **Task 37-3：Duplex 非发音弦段被动共鸣**：固定容量、主弦能量耦合激励，与现有开放主弦交感区分；不混入 Blüthner 独立 Aliquot 第四弦模型；
4. **Task 37-4：可保存的钢琴风格预设**：补齐风格所需声学字段，提供经真实音频和试听区分的参数化风格，不将未标定的 Upright/Fortepiano 型号复刻包装成旋钮快照；
5. **Task 37-5：实时、录制与离线集成**：设置、普通预设、Take 快照、实时和内置 WAV 消费同一有效声学配置，验证采样级切换、原发音身份与实时预算。

**直接交付**：实际 A4/分音对校准、未踩踏板的有界 Duplex、五种参数化风格与真实 Save As/重启音色恢复、整段非零 wet 快照实时/WAV 对照及原身份释放已验证；离线混响初始状态不再污染起音。Windows Debug app/tests、默认 CTest、格式和全量 tidy 通过，零堆/对应锁与库三角观察有正调用自检。现有 2.0 秒 WAV 截断边界保留；未执行 Release 或 ≤0.7% CPU 认证，硬件/厂商补验不外推。完整任务与证据见 [Phase 37 完成归档](../archive/phase37-38-piano-calibration-and-keyboard-performance.md#2-phase-37物理建模调律与被动共鸣校准)。

### Phase 38：电脑键盘分区、三踏板控制与固定双层演奏（Keyboard Zones, Three-Pedal Control & Fixed Dual Layer）[软件交付完成，2026-10-10]

将已有配置表达力转化为完整演奏体验，固定双层仅位于 Instrument 内部，继续遵守 `Performance Input -> Instrument -> Master -> Output`：

1. **Task 38-1：一次性身份**：Take 保存区域、Group、修饰键、矩阵/跟随调号之后的最终音乐身份，原生回放、WAV/MIDI 不重复变换；当前整数 v4 拒绝旧义，不做 Raw Key Note 重构；
2. **Task 38-2：Sostenuto 与多区域踏板闭环**：复用已有 CC66 核心，补齐电脑键盘与 ViewModel/UI 入口，以及 CC64/66/67 的目标通道、原持有身份和 Transport 清理语义；
3. **Task 38-3：双模式物理键区、小键盘输入与统一映射看板**：支持仅主键盘/主键盘加数字小键盘两种固定布局，分离本机模式与预设音乐参数，补齐独立键码、Group 覆盖及统一看板；具体键位和默认绑定见 [归档任务契约](../archive/phase37-38-piano-calibration-and-keyboard-performance.md#task-38-3双模式物理键区小键盘输入与统一映射看板)；
4. **Task 38-4：固定 Piano + 单 VST3 双层发声**：同一规范 MIDI 输入驱动两个固定端点，预分配缓冲、混音和必要的增益配比，处理插件报告延迟与层/实例生命周期；不默认泛化为 1→N MIDI 路由或多插件槽位；
5. **Task 38-5：双层状态与录制/导出/UI 闭环**：保存层模式与配比，实时和独立离线实例均执行复合渲染，明确插件依赖、失败/取消与实际 UI 验证边界，不承诺跨机器还原原厂商音色。

**直接交付**：最终身份无重复变换、CC66 边沿与原通道释放、固定分区/完整 Num keyCode、非零区域音乐配置及本机模式独立恢复、端点所有权、采样延迟和实时/离线固定双层均有消费者证据。实际绑定、Editor、后台/进度窗取消、保持输入时重扫/加载及正常关闭通过，Windows Debug app/tests、默认 CTest、格式与全量 tidy 通过。硬件键盘/声卡、商业厂商、CPU SLA、跨机音色和超 2.0 秒尾音不外推。详细任务与证据见 [Phase 38 完成归档](../archive/phase37-38-piano-calibration-and-keyboard-performance.md#3-phase-38电脑键盘分区三踏板控制与固定双层演奏)，后续修复另见归档第 6 节，不改写原验证记录。

### Phase 39：可复核声学基线与三力度整音校准[规划]

以独立 `pianoteq9` 研究仓库的参数语义、候选方程与保留音频作为参考输入，不把商业软件输出或逆向摘要认证为真实钢琴逐键常数。先统一分音/中心弦、力度、频谱与包络的量测口径，复核有疑点的拟合及复现链路；只按可靠证据调整 `B` 锚点，并保留现行第一分音/A4/拉伸语义。在现有增强模态激振内深化三力度整音，同轮迁移预设、设置、Take 与实时/两个 WAV 消费者，完成受控试听、实际 UI 及完整 callback 成本验证。

详细 Task 39-1～39-5、依赖排期和直接验收只在 [当前迭代](current-iteration.md#2-phase-39可复核声学基线与三力度整音校准) 维护。本次只确认规划，不修改 DSP 或文件格式，不以研究参考、文档提交或历史门禁宣布任务通过。

### Phase 40：同音弦耦合、双阶段衰减与 Una Corda 深化[规划]

目标：深化已有多弦振荡器与双振幅包络，使拍频、衰减及辐射之间的关系可解释、可测量，不只替换两组时间常数。

1. **同音频率与有效身份**：定义 Width/Balance 的单位、范围及频率中心，保持单/双/三弦分区和原已起音身份，不把某一 C3 的拍频套用全琴。
2. **被动耦合与可辐射慢模态**：区分振幅、功率、能量及时间常数，采用固定容量和有界状态；验证单声道慢衰减、无输入不自激和失谐后的声音，不能用静态正交投影或额外混响冒充能量耦合。
3. **Una Corda 激振拓扑**：耦合成立后验证两弦受击、第三弦初始不受击而被动响应，保留逐通道 CC67、声部重用与采样级控制语义。

进入条件：Phase 39 的生产基线、有效频率/连续整音及状态/实时闭环完成。出口：拍频与快/慢衰减有真实音频依据，慢模态不因单声道求和消失，尾音/Panic/偷声部闭合，实时与 WAV 有一致消费者结果；不默认套用研究中的全琴统一权重或衰减常数。

### Phase 41：音板频变阻抗与开放弦共鸣校准[规划]

目标：深化现有音板与共鸣近似，验证频变耗散、开放状态和跨声部被动传能，而不是增加一套同名共鸣效果。

1. **音板阻抗与频变损耗**：区分输出低通、模态阻尼和琴弦向音板的能量泄漏，校准琴桥交界与参数极值；模态数量、内存大小或未经核实的频率表不是琴型标定证明。
2. **开放弦与跨声部激励**：验证按键、CC64、CC66 捕获及无制音高音区的门控和自然衰减；必要的共享琴桥总线只属于固定 Instrument 内部，不扩展为任意路由。
3. **共鸣分工与所有权**：保持交感主弦、Duplex 非发音段和独立 Aliquot 弦的概念边界；不以高通后的十二音级池替代已校准 Duplex，释放仍使用原通道和端点所有权。

进入条件：已有 Phase 39 频率基线及 Phase 40 主弦耦合/辐射定义；相关机理可独立调研，生产集成使用同一基线。出口：跨声部响应、自然耗散、无重复能量计入和原目标清理有证据，参数边界稳定，完整实时成本与实时/离线声音有效。

### Phase 42：能量驱动张力非线性与泛音绽放[规划]

目标：按实际衰减模态状态深化现有 Pitch Glide 与一阶 Blooming，替换被淘汰的经验路径，不再叠加一层不可归因的效果。

1. **张力与状态尺度**：定义模态位移、权重和能量尺度，用随包络衰减的状态驱动张力，不能把单位振荡器的 `cos²` 当作真实振动能量。
2. **瞬态频率轨迹**：校准力度、音区与采样率下的微漂移和回落，保留参考音高、旁路及弱奏基准，单独检查时变频率的稳定性和 Nyquist 边界。
3. **泛音时间轨迹**：由量测决定是否需要独立深度/惯性控制，采用有界递推，并检查非线性混叠及完整成本；有效切换后删除旧漂移/绽放分支，不保留兼容模型。

进入条件：Phase 39 的激振与频率校准、Phase 40/41 的耗散和耦合尺度成立。出口：效应随振动能量消退，旁路与弱奏有效，长时、多复音和极值不自激，实时/离线整段对照及产品自有实时契约有直接证据。

**近期不排期**：完整 Verlet 琴槌—琴弦求解器、连续弦长的 Upright↔Grand 琴型变形、五麦克风 3D 阵列和更多机械噪声旋钮。它们需要独立的位移/质量/几何或拾音依据与真实需求，不以参考报告的“极低代价”估计替代设计和验收，也不为这些展望预留实现。

## 4. 主要风险与应对

| 风险 | 当前判断 | 应对方向 |
|---|---|---|
| 用户文件与会话完整性 | 软件保护与准入有直接证据，异常存储边界保留 | 事务写出、身份/绑定/设置一致性及数值拒绝按 Phase A/B/H 验证；强杀、断电和硬件故障不外推为已认证。 |
| 插件/声部/Transport生命周期 | Phase C/D/E 边界已验证，仍有后续风险 | 停 callback 守卫、块入口命令、协作退出、采样级身份/时间域及预设快照闭包已直接验证；不外推厂商永久卡死、真实声卡热插拔或 JUCE VST3 框架锁。 |
| 键盘映射与发音身份边界 | Phase D/F 消费者已闭环 | 原身份 FIFO、同音持有、暂停/末尾与设备域保持；双看板共享最终投影，重复/回放后的鼠标输入不被输出反馈污染，绑定和逐键定制保留原输入索引。 |
| 物理建模与实时负荷 | 产品自有路径已有直接证据，性能 SLA 仍须实测 | 自有物理音源、正弦波、节拍器及机械闭包已有零库函数三角证据；8 复音单核 CPU $\le 0.7\%$ 保持为验收 SLA，本次文档复审未新增性能测量；第三方插件框架限制单列。 |
| UI与声明式门面 | Phase F/G/H 消费者已闭环 | 双投影/输入索引、标签与有界 fade、即时圆角、Notes 及严格 ViewHost 门面已有真实窗口证据；不扩展通用 UI 框架，IME/辅助窗口全组合另测。 |
| `MainComponent` 职责回流 | 低 | 保持轻量装配职责（主要由 `initialiseUi()` 承载 JIVE 树构建与回调接线，核心业务均已委托独立 Controller 与领域模块）；持续监控，避免业务逻辑回流。 |
| 硬实时契约差距 | 产品自有路径已达标，第三方框架单列 | 产品自有发声、调度与节拍器已实现无锁零分配；键盘输入与视觉高亮彻底解耦，超协商几何安全静音；用户批准分层验收，第三方 VST3 适配器的框架锁与 2048 消息限制单列，见 known-issues。 |
| A4 基准音高契约 | 400.0–480.0 Hz 已对齐 | 引擎/设置/预设/内置导出同限幅；两端、正常参考和越界钳制已按实时波形、离线 WAV 与实际设置验证。 |
| 门禁与最终集成范围 | 软件实施已归档，综合实机补验未完成 | Windows Debug/默认回归、用户目录保护、warning/tidy 与契约分别保留实际基线和输出；历史证据在 AUDIT-004 与 Phase 37/38 实施归档，当前任务页维护 Phase 39 任务与直接验收，不将规划或归档视为新软件、硬件或 CPU 认证。 |

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

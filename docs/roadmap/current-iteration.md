# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 30 ~ 32：古典调律、空间声学与微观机械拟真三部曲 (Historical Temperaments, Spatial Acoustics & Action Mechanics Trilogy) [规划与开发中]**

*(注：Phase 29：现实物理演奏交互与声学控制于 2026-09-12 全部胜利完成并归档，包含琴盖 3 态声学开合、Una Corda 弱音移位物理拟真、4 种触键力度曲线与全栈配置预设联动。详细记录见 [`../archive/phase29-physical-voicing-and-acoustic-interaction.md`](../archive/phase29-physical-voicing-and-acoustic-interaction.md)。)*

在完成琴盖开合、弱音移位与触键手感曲线等整音（Voicing）核心能力后，devpiano 自主研发的 7 大物理建模声学系统迈向音乐学调律、真实空间声学与微观机械仿真的终极闭环。

依据整体演进路线与用户听觉价值最大化原则，Phase 30、Phase 31 与 Phase 32 构成了现代顶级物理建模钢琴（如 Pianoteq）的三大核心支柱。为保证架构统一与音色系统的一体化演进，**Phase 30 ~ 32 将作为同一批次连贯研发，并在同一个统一 Pull Request 中提交与验收**。

---

## 核心边界与铁律约束 (Boundaries & Iron Rules)

在实施 Phase 30 ~ 32 全阶段开发过程中，必须无条件遵守以下核心边界与铁律：

1. **铁律 1（纯自包含与免安装绿色冷启动）**：
   - 严禁引入外部卷积脉冲响应（IR）音频采样文件、外部采样库或庞杂外部依赖；
   - 空间声学必须采用纯数学轻量算法立体声延迟网络与早期反射模型；
   - 调律系统坚决裁剪外部 Scala (.scl/.kbm) 复杂文件解析器，采用高效内置权威数学常数表；保持单文件免安装绿色发行版的核心优势。
2. **铁律 2（音频实时线程严格无锁与零动态分配）**：
   - 基频音分偏转计算、声像宽度缩放、早期反射与瞬态冲激生成在音频渲染热路径（`renderNextBlock` / `startNote`）中严禁任何堆内存分配（`malloc` / `new` / `std::vector::push_back` 等）；
   - 严禁互斥锁（Mutex）、严禁磁盘与文件 I/O，参数更新必须通过原子变量或双缓冲无锁传递。
3. **铁律 3（字符编码与严格 7-bit ASCII）**：
   - 严禁在 C++ 源码（`.cpp` / `.h`，包括单元测试）中书写裸多字节非 ASCII 字符；
   - UI 符号一律使用显式 Unicode 标量转义（如 `\u2022`、`juce::String::charToString(0x2022)`）；
   - 自然语言文本 100% 外部化至 `source/Locale/zh_CN.loc`，单测中严禁硬编码断言翻译文本。
4. **铁律 4（UI 基础设施接口冻结遵从）**：
   - 严格遵守 Phase 28 确立的 UI Infrastructure API Freeze 冻结公约；
   - 界面控件一律通过 `ViewHost` 与标准 JIVE ValueTree 模板构建，禁止调用内部未导出符号或引入裸指针强转，确保 `LayoutGoldenTest` 布局金标测试 100% 绿灯。
5. **铁律 5（严格三闸门基线与全平台闭环）**：
   - 代码格式合规：`./scripts/dev.sh format --check` 100% 通过；
   - 单元测试全覆盖：`./scripts/dev.sh test` 所有新增与存量测试 100% 绿灯；
   - 静态分析零警告：`./scripts/dev.sh tidy` 0 错误 0 警告；
   - Windows MSVC 验证：`./scripts/dev.sh win-build` 增量与正式链接验证通过。

---

## 阶段规划详案

### Phase 30：历史调律体系与基准音高校准 (Historical Temperaments & Reference Pitch Calibration)

> 目标：解耦固定十二平均律基频计算，构建纯 C++20 高精度调律引擎，内置六大经典历史律制与 A4 基准音高（415~442Hz）无级微调。

- [x] **Phase 30-A：高精度纯数学 TemperamentEngine 与六大经典律制实现** [已完成，2026-09-13]：
  - 在 `source/Audio/TemperamentEngine.h` 中设计纯 C++20 `TemperamentEngine`，采用 `constexpr` 静态音分常数表与无锁音高映射；
  - 完整实现六大经典律制：
    1. `Equal (十二平均律)`：现代工业标准，每个半音严格等于 100 音分；
    2. `Just Intonation (纯律)`：纯五度与纯大三度完全协和（如 $3/2$ 与 $5/4$ 纯比例），无拍频极度纯净；
    3. `Pythagorean (毕达哥拉斯律)`：基于连续纯五度叠置（$3:2$），大二度与五度纯正，适合早期中世纪与古乐；
    4. `Meantone 1/4 comma (中庸全音律)`：牺牲狼音五度换取大三度纯正，文艺复兴与早期巴洛克键盘标准；
    5. `Werckmeister III (魏克迈斯特律 III)`：巴赫《平均律键盘曲集》时代的良律（Well-Tempered），所有调性均可演奏且各具鲜明调性色彩；
    6. `Kirnberger III (基恩伯格律 III)`：以 4 个 1/4 柯马中庸五度与纯五度混合，纯正 C 大调与丰富调性张力并存；
  - **明确裁剪项**：裁剪复杂的外部 `.scl` / `.kbm` 文件解析器，消除外部文件依赖与低性价比容错负担，恪守轻量自包含原则。
- [x] **Phase 30-B：A4 基准音高校准与 PianoSynthVoice 基频解耦** [已完成，2026-09-13]：
  - 解耦 `PianoSynthVoice.h` 中硬编码的 `juce::MidiMessage::getMidiNoteInHertz`；
  - 引入 A4 基准音高调节（默认 440.0 Hz，可调范围 410.0 Hz ~ 450.0 Hz），支持快捷预设：
    - `415.0 Hz`（巴洛克古典低音高，约低半音）；
    - `432.0 Hz`（维尔第/自然哲学调音）；
    - `440.0 Hz`（现代国际标准）；
    - `442.0 Hz`（现代欧洲/交响乐团通透偏高标准）；
  - 琴弦物理模态公式 $f_m = m f_0 \sqrt{1 + B m^2}$ 与八度伸缩计算与调律引擎输出基频平滑对齐。
- [x] **Phase 30-C：JIVE 设置界面联动、持久化与单测防线** [已完成，2026-09-13]：
  - 在 `SettingsLayoutModel.cpp` 声学卡片中增加律制选择下拉框（`temperament-combo`）与 A4 基准音高微调滑块（`reference-pitch-slider`）；
  - 将调律参数完整纳入 `SettingsModel`、`SettingsStore`（Properties 序列化）与 `PerformancePreset`（`.devpiano.preset` JSON 序列化）；
  - 新增 `source/tests/TemperamentEngineTest.cpp`，覆盖六大律制数学音分准确度、A4 基频换算、全音域（A0~C8）单调性、跨会话持久化与预设向后兼容。

---

### Phase 31：多视角空间声学与算法混响 (Multi-Perspective Spatial Acoustics & Algorithmic Room Modeling)

> 目标：提供演奏者与观众双视角立体声场切换，接入低 CPU 开销的纯轻量算法房间空间混响网络，彻底消灭“贴耳干冷电子感”，赋予声音真实空气流动感。

- [x] **Phase 31-A：Player vs Audience 双视角立体声场与声像转换** [已完成，2026-09-13]：
  - 在 `PianoSynthVoice::SpatialDiffusionEngine` 之后接入双视角立体声像处理器（`PerspectiveProcessor`）：
    - **演奏者视角 (Player Perspective)**：模拟坐在钢琴琴凳上的近场主观听感——低音弦在左侧、高音弦在右侧（根据 88 键物理位置自然展开），立体声宽度宽广，近场直接击弦瞬态清晰；
    - **观众视角 (Audience Perspective)**：模拟音乐厅观众席的远场客观听感——左右立体声像适度反转（低音在右、高音在左对齐观众视角），声像收窄汇聚，高频受空气吸收呈现自然圆润衰减；
  - 视角切换时采用无锁系数平滑插值，确保演奏过程中切换无爆音、无相位抵消。
- [x] **Phase 31-B：纯轻量数学算法房间混响网络与三大空间预设 (Algorithmic Room Reverb)** [已完成，2026-09-13]：
  - 坚守纯数学轻量算法混响设计（基于高效反馈延迟网络 FDN / 梳状全通滤波矩阵，零外部采样依赖）；
  - 提供三大经典空间模式与湿声深度（Wet Ratio 0~100%）：
    1. `Studio (录音棚)`：短混响时间（$RT_{60} \approx 0.6\text{ s}$），早期反射紧致，保留极大干声音色纯度；
    2. `Chamber (室内乐厅)`：中等混响时间（$RT_{60} \approx 1.5\text{ s}$），木质反射温暖，适合独奏与重奏；
    3. `Concert Hall (音乐厅)`：长混响时间（$RT_{60} \approx 2.4\text{ s}$），声场宽阔开阔，高扩散尾音包围感强；
  - 优化算法计算性能，双声道处理开销维持在单核 CPU $\le 0.5\%$ 以内。
- [x] **Phase 31-C：JIVE 声学面板集成、预设联动与测试防线** [已完成，2026-09-13]：
  - 在 `SettingsLayoutModel.cpp` 中新增空间声学分组卡片（`spatial-room-card`），包含视角切换下拉框（`perspective-combo`）、空间模式选择器（`reverb-space-combo`）与混响电平滑块（`reverb-wet-slider`）；
  - 完整打通 SettingsStore 与 PerformancePreset 存取，支持一键在预设中切换干/湿声学场景；
  - 新增 `source/tests/SpatialAcousticsTest.cpp`，覆盖立体声像反转数学正确性、混响算法数值稳定性（长时静音衰减无下溢 denormal）、跨采样率（44.1k/48k/96k）不变性。

---

### Phase 32：机械物理噪声与琴体微衰退拟真 (Mechanical Action Noise & Physical Imperfection)

> 目标：物理模拟击弦机制音器升降空气啸声、离键木质轻撞声与琴体微老化扰动，赋予乐器鲜活的真实机械生命力。

- [x] **Phase 32-A：延音踏板机械气流与箱体共鸣冲击 (Pedal Whoosh & Resonance Shock)** [已完成，2026-09-13]：
  - 在 `PianoSynthVoice` 中捕获 MIDI CC 64 延音踏板踩下与抬起动作；
  - 当踏板快速踩下时，模拟全弦制音器同时抬起的空气流动物理微啸声（Whoosh，带通塑形白噪短脉冲）；
  - 激发共鸣弦列的极微弱低频瞬态冲击（Resonance Shock，模拟止音器离开琴弦时的瞬时机械微扰动）；
  - 踩下与抬起速度自适应调整冲激强度，并提供可配置的 `pedalNoiseLevel` 增益控制。
- [x] **Phase 32-B：离键抬起与制音器落弦瞬态深化 (Damper Drop Thump & Key Release)** [已完成，2026-09-13]：
  - 重构深化 `damperTransient` 物理模型：
    - 根据键盘离键速度（Note-Off Velocity）动态调整阻尼器毛毡贴回琴弦的摩擦衰减速度；
    - 引入木质键体落回键床底部的微弱撞击声（Key Release Thump）；
  - 纯物理瞬态合成算法，维持零采样依赖与低计算负荷。
- [x] **Phase 32-C：琴槌毛毡微老化扰动与调音离散度 (Inharmonicity Jitter & Felt Ageing)** [已完成，2026-09-13]：
  - 引入确定性伪随机微失谐扰动（Inharmonicity & Pitch Micro-Jitter，$\pm 0.3\sim 1.5\text{ cents}$），打破“绝对数学纯净”的冰冷感；
  - 模拟琴槌毛毡击弦受力不均造成的逐键微老化差异（Per-key Felt Ageing），使相邻琴键具备微妙的拟真生命力；
  - 提供总控开关与老化深度调节（`feltAgeingAmount`）。
- [ ] **Phase 32-D：UI 控件接入、全栈集成与全量三闸门闭环**：
  - 在设置界面声学卡片中提供机械噪声强度滑块（`action-noise-slider`）与物理老化开关；
  - 全流程通过 `SettingsModel`、`SettingsStore` 与 `PerformancePreset` 联动；
  - 新增 `source/tests/MechanicalAcousticsTest.cpp`，覆盖机械噪声触发、音量安全限幅、防爆音机制与单测验证；
  - 全面执行 `./scripts/dev.sh format --check`、`./scripts/dev.sh test`、`./scripts/dev.sh tidy` 与 `./scripts/dev.sh win-build` 最终双端验收。

---

## 历史实现 Backlog

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

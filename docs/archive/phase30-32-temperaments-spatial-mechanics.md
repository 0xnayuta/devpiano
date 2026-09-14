# Phase 30 ~ 32 完成记录：古典调律、空间声学与微观机械拟真三部曲 (Historical Temperaments, Spatial Acoustics & Action Mechanics Trilogy)

> 归档日期：2026-09-13
> 前序依赖：Phase 29（现实物理演奏交互与声学控制）
> 实施范围：`source/Audio/`、`source/Input/`、`source/Settings/`、`source/Layout/`、`source/UI/jive/`、`source/Locale/`、`source/tests/`
> 交付形式：统一分支 `feat/phase30-32-temperament-spatial-mechanics-trilogy`，Pull Request #15 验收合并

---

## 1. 目标与背景

在完成琴盖开合、弱音移位与触键手感曲线等整音（Voicing）核心能力（Phase 29）后，devpiano 自主研发的 7 大物理建模声学系统迈向音乐学调律、真实空间声学与微观机械仿真的终极闭环。

依据整体演进路线与用户听觉价值最大化原则，Phase 30、Phase 31 与 Phase 32 构成了现代顶级物理建模钢琴（如 Pianoteq）的三大核心支柱：
1. **调律体系解耦与拓展（Phase 30）**：打破单一十二平均律束缚，实现纯律、毕达哥拉斯律、中庸全音律、魏克迈斯特律 III、基恩伯格律 III 六大古典历史律制与 A4 基准音高（415~442Hz）无级微调；
2. **多视角空间声学与纯轻量混响（Phase 31）**：提供演奏者主观视角（近场宽立体声）与观众/音乐厅远场客观反转视角切换，接入低 CPU 开销（单核 $\le 0.5\%$）的纯数学轻量算法房间空间混响网络，彻底消除干冷电子感；
3. **机械物理噪声与琴体微衰退（Phase 32）**：物理模拟延音踏板气流啸声与共鸣冲击、离键木质轻撞与制音器落弦瞬态，以及琴槌毛毡微老化扰动与调音离散度，赋予乐器鲜活的真实机械生命力。

为保证架构统一与音色系统的一体化演进，Phase 30 ~ 32 作为同一批次连贯研发，并在同一个统一 Pull Request 中交付验收。

---

## 2. 实施细节 (Phase 30 ~ Phase 32)

### Phase 30：历史调律体系与基准音高校准 (Historical Temperaments & Reference Pitch Calibration)

- **Phase 30-A：高精度纯数学 TemperamentEngine 与六大经典律制实现** [已完成，2026-09-13]：
  - 在 `source/Audio/TemperamentEngine.h` 中设计纯 C++20 `TemperamentEngine`，采用 `constexpr` 静态音分常数表与无锁音高映射；
  - 完整实现六大经典律制：
    1. `Equal (十二平均律)`：现代工业标准，每个半音严格等于 100 音分；
    2. `Just Intonation (纯律)`：纯五度与纯大三度完全协和（如 $3/2$ 与 $5/4$ 纯比例），无拍频极度纯净；
    3. `Pythagorean (毕达哥拉斯律)`：基于连续纯五度叠置（$3:2$），大二度与五度纯正，适合早期中世纪与古乐；
    4. `Meantone 1/4 comma (中庸全音律)`：牺牲狼音五度换取大三度纯正，文艺复兴与早期巴洛克键盘标准；
    5. `Werckmeister III (魏克迈斯特律 III)`：巴赫《平均律键盘曲集》时代的良律（Well-Tempered），所有调性均可演奏且各具鲜明调性色彩；
    6. `Kirnberger III (基恩伯格律 III)`：以 4 个 1/4 柯马中庸五度与纯五度混合，纯正 C 大调与丰富调性张力并存；
  - **明确裁剪项**：裁剪复杂的外部 `.scl` / `.kbm` 文件解析器，消除外部文件依赖与低性价比容错负担，恪守轻量自包含原则。
- **Phase 30-B：A4 基准音高校准与 PianoSynthVoice 基频解耦** [已完成，2026-09-13]：
  - 解耦 `PianoSynthVoice.h` 中硬编码的 `juce::MidiMessage::getMidiNoteInHertz`；
  - 引入 A4 基准音高调节（默认 440.0 Hz，可调范围 410.0 Hz ~ 450.0 Hz），支持快捷预设：
    - `415.0 Hz`（巴洛克古典低音高，约低半音）；
    - `432.0 Hz`（维尔第/自然哲学调音）；
    - `440.0 Hz`（现代国际标准）；
    - `442.0 Hz`（现代欧洲/交响乐团通透偏高标准）；
  - 琴弦物理模态公式 $f_m = m f_0 \sqrt{1 + B m^2}$ 与八度伸缩计算与调律引擎输出基频平滑对齐。
- **Phase 30-C：JIVE 设置界面联动、持久化与单测防线** [已完成，2026-09-13]：
  - 在 `SettingsLayoutModel.cpp` 声学卡片中增加律制选择下拉框（`temperament-combo`）与 A4 基准音高微调滑块（`reference-pitch-slider`）；
  - 将调律参数完整纳入 `SettingsModel`、`SettingsStore`（Properties 序列化）与 `PerformancePreset`（`.devpiano.preset` JSON 序列化）；
  - 新增 `source/tests/TemperamentEngineTest.cpp`，覆盖六大律制数学音分准确度、A4 基频换算、全音域（A0~C8）单调性、跨会话持久化与预设向后兼容。

---

### Phase 31：多视角空间声学与算法混响 (Multi-Perspective Spatial Acoustics & Algorithmic Room Modeling)

- **Phase 31-A：Player vs Audience 双视角立体声场与声像转换** [已完成，2026-09-13]：
  - 在 `PianoSynthVoice::SpatialDiffusionEngine` 之后接入双视角立体声像处理器（`PerspectiveProcessor`）：
    - **演奏者视角 (Player Perspective)**：模拟坐在钢琴琴凳上的近场主观听感——低音弦在左侧、高音弦在右侧（根据 88 键物理位置自然展开），立体声宽度宽广，近场直接击弦瞬态清晰；
    - **观众视角 (Audience Perspective)**：模拟音乐厅观众席的远场客观听感——左右立体声像适度反转（低音在右、高音在左对齐观众视角），声像收窄汇聚，高频受空气吸收呈现自然圆润衰减；
  - 视角切换时采用无锁系数平滑插值，确保演奏过程中切换无爆音、无相位抵消。
- **Phase 31-B：纯轻量数学算法房间混响网络与三大空间预设 (Algorithmic Room Reverb)** [已完成，2026-09-13]：
  - 坚守纯数学轻量算法混响设计（基于高效反馈延迟网络 FDN / 梳状全通滤波矩阵，零外部采样依赖）；
  - 提供三大经典空间模式与湿声深度（Wet Ratio 0~100%）：
    1. `Studio (录音棚)`：短混响时间（$RT_{60} \approx 0.6\text{ s}$），早期反射紧致，保留极大干声音色纯度；
    2. `Chamber (室内乐厅)`：中等混响时间（$RT_{60} \approx 1.5\text{ s}$），木质反射温暖，适合独奏与重奏；
    3. `Concert Hall (音乐厅)`：长混响时间（$RT_{60} \approx 2.4\text{ s}$），声场宽阔开阔，高扩散尾音包围感强；
  - 优化算法计算性能，双声道处理开销维持在单核 CPU $\le 0.5\%$ 以内。
- **Phase 31-C：JIVE 声学面板集成、预设联动与测试防线** [已完成，2026-09-13]：
  - 在 `SettingsLayoutModel.cpp` 中新增空间声学分组卡片（`spatial-room-card`），包含视角切换下拉框（`perspective-combo`）、空间模式选择器（`reverb-space-combo`）与混响电平滑块（`reverb-wet-slider`）；
  - 完整打通 SettingsStore 与 PerformancePreset 存取，支持一键在预设中切换干/湿声学场景；
  - 新增 `source/tests/SpatialAcousticsTest.cpp`，覆盖立体声像反转数学正确性、混响算法数值稳定性（长时静音衰减无下溢 denormal）、跨采样率（44.1k/48k/96k）不变性。

---

### Phase 32：机械物理噪声与琴体微衰退拟真 (Mechanical Action Noise & Physical Imperfection)

- **Phase 32-A：延音踏板机械气流与箱体共鸣冲击 (Pedal Whoosh & Resonance Shock)** [已完成，2026-09-13]：
  - 在 `PianoSynthVoice` 中捕获 MIDI CC 64 延音踏板踩下与抬起动作；
  - 当踏板快速踩下时，模拟全弦制音器同时抬起的空气流动物理微啸声（Whoosh，带通塑形白噪短脉冲）；
  - 激发共鸣弦列的极微弱低频瞬态冲击（Resonance Shock，模拟止音器离开琴弦时的瞬时机械微扰动）；
  - 踩下与抬起速度自适应调整冲激强度，并提供可配置的 `pedalNoiseLevel` 增益控制。
- **Phase 32-B：离键抬起与制音器落弦瞬态深化 (Damper Drop Thump & Key Release)** [已完成，2026-09-13]：
  - 重构深化 `damperTransient` 物理模型：
    - 根据键盘离键速度（Note-Off Velocity）动态调整阻尼器毛毡贴回琴弦的摩擦衰减速度；
    - 引入木质键体落回键床底部的微弱撞击声（Key Release Thump）；
  - 纯物理瞬态合成算法，维持零采样依赖与低计算负荷。
- **Phase 32-C：琴槌毛毡微老化扰动与调音离散度 (Inharmonicity Jitter & Felt Ageing)** [已完成，2026-09-13]：
  - 引入确定性伪随机微失谐扰动（Inharmonicity & Pitch Micro-Jitter，$\pm 0.3\sim 1.5\text{ cents}$），打破“绝对数学纯净”的冰冷感；
  - 模拟琴槌毛毡击弦受力不均造成的逐键微老化差异（Per-key Felt Ageing），使相邻琴键具备微妙的拟真生命力；
  - 提供总控开关与老化深度调节（`feltAgeingAmount`）。
- **Phase 32-D：UI 控件接入、全栈集成与全量三闸门闭环** [已完成，2026-09-13]：
  - 在设置界面声学卡片中新增机械噪声强度滑块（`pedal-noise-slider`）与毛毡老化深度滑块（`felt-ageing-slider`）；
  - 全流程通过 `SettingsModel`、`SettingsStore` 与 `PerformancePreset` 联动（含离线 WAV 导出参数一致性）；
  - 新增 `source/tests/MechanicalAcousticsTest.cpp`，覆盖持久化往返、边界钳制、预设向前向后兼容、离线导出传递与极端参数安全限幅/防爆音验证；
  - 全面执行 `./scripts/dev.sh format --check`、`./scripts/dev.sh test`、`./scripts/dev.sh tidy` 与 `./scripts/dev.sh win-build` 最终双端验收。

---

## 3. 验收与三闸门防线验证

- **代码格式门禁**：`./scripts/dev.sh format --check` 100% 格式对齐（通过）；
- **单元测试套件**：`./scripts/dev.sh test`（新增 3 个测试套件，断言数从 12,089 增长至 12,200+，100% 绿灯）；
- **静态代码分析**：`./scripts/dev.sh tidy` 增量 0 警告；
- **Windows MSVC 验证**：Windows 侧 `build-win-msvc` 纯净构建与单元测试 100% 通过；
- **国际化与字符编码**：严格遵循 7-bit ASCII 与 Unicode 转义规范，无裸多字节字符泄漏，中英文语言包完全同步。

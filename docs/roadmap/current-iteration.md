# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 29：现实物理演奏交互与声学控制 (Physical Voicing & Realistic Acoustic Interaction) [规划中]**

*(前序里程碑说明：Phase 27 JUCE 9.0.1 升级与 UI 代码内化、Phase 28 ViewHost 门面封装、全量布局金标测试防线与 UI Infrastructure API Freeze 接口冻结公约已全部完成并归档)*

---

### Phase 29-A：琴盖开合度声学交互与 UI 穿透 (Acoustic Lid Position & JIVE UI Integration) [待启动]

> 目标：将底层已实现的 `PianoSynthVoice::LidPosition` 与 `lidAcoustics` 多级高频滚降/近场反射声学传递函数，完整穿透至 JIVE 声明式 UI 与 AudioEngine 调度链路，实现全开、半开、合盖的即时切换与直观视觉交互。

- [ ] **JIVE 声明式 UI 控件接入（遵守 UI Infrastructure Freeze 公约）**：
  - 在 `source/UI/LayoutModel.cpp` 的 `makeSettingsLayoutTree()` 中，通过 C++ DSL 在音频/声学参数区域声明琴盖位置选择器（`lid-position-combo`），包含 3 种直观选项：
    - `Full Open (全开)`：直达声主导，高频泛音通透，近场反射宽阔；
    - `Half Stick (半开/短支架)`：高频轻微遮蔽（$-3\text{ dB}$ 柔和滚降），直达声与短单反射融合；
    - `Closed (合盖)`：深沉包裹感，$-7\text{ dB}$ 显著高阶滚降与近场木质共鸣。
  - 在 `source/Settings/SettingsComponent.cpp` 中通过 `viewHost.find<juce::ComboBox>("lid-position-combo")` 安全获取控件句柄，配置选项文本并绑定监听回调；
  - 评估在主界面控制区域（`MainComponent` 声学状态区）提供直观的琴盖图标/状态指示与快速切换。
- [ ] **音频引擎与声学内核无缝贯通**：
  - 将 UI 变更实时转发至 `AudioEngine::setLidPosition`，由音频线程在原子标志下安全更新至每个活跃的 `PianoSynthVoice`；
  - 验证运行态切换琴盖时无音频爆音（Click/Pop）、无内存分配、无线程争用。
- [ ] **测试与布局金标防线回归**：
  - 新增 `source/tests/LidAcousticsInteractionTest.cpp`，断言 3 态切换下频响能量特征与冲激响应衰减符合物理预期；
  - 回归 `source/tests/LayoutGoldenTest.cpp`，确保声明式树注入后全应用 ValueTree 解释烟测与 1280x720 / 1920x1080 像素几何排版 100% 稳定。

---

### Phase 29-B：弱音/移位踏板物理拟真与状态联动 (Una Corda / Soft Pedal Physical Modeling & CC 67) [待启动]

> 目标：在 `PianoSynthVoice` 中建立三角钢琴击弦机整体右移、3 弦敲 2 弦与毛毡较软侧面击弦的物理机理，支持 MIDI CC 67 踏板信号、电脑键盘快捷触发与 UI 状态点亮。

- [ ] **三角钢琴移位（Una Corda）物理声学机理建模**：
  - 在 `PianoSynthVoice.h` 中实现击弦机偏移机理（引入 `softPedalActive` 与连续阻尼深度 `softPedalAmount` $\in [0.0, 1.0]$）：
    - **有效硬度软化与接触时间延长**：击弦点移至毛毡侧边较软区域，有效硬度 $H_{\text{eff}} = H \cdot (1.0 - 0.25 \mu)$，接触时间 $t_c$ 相应延长 $15\%\sim 25\%$，从震源抑制高阶泛音剧烈激发；
    - **中高音区三弦敲两弦（Trichord to Bichord）能量衰减**：Note 40 以上三弦组，击打能量衰减 $\approx -3.5\text{ dB}$，未被击打的一根琴弦经琴桥被动激发产生微弱空灵交感余音与微相位差；低音单弦/双弦区衰减 $\approx -2.0\text{ dB}$；
    - **分音衰减率动态重构**：高阶分音附加动态柔音滚降因子 $1.0 + 0.15 \mu \cdot (n - 1)$；
    - **琴槌敲击瞬态噪声软化**：$A_{\text{hammer}} = A_{\text{hammer}} \cdot (1.0 - 0.4 \mu)$。
- [ ] **MIDI CC 67 与控制器消息解析**：
  - 在 `PianoSynthVoice::controllerMoved` 中拦截 `controllerNumber == 67`（Soft Pedal），根据数值范围动态更新柔音状态；
  - 保证外部硬件 MIDI 键盘踏板信号与软件回放时间线精准解析 CC 67。
- [ ] **电脑键盘输入与 UI 柔音状态点亮**：
  - 在 `KeyboardMidiMapper` 中增加软踏板快捷键支持与 `SoftPedalCallback`；
  - 在主界面状态栏或虚拟键盘控制区增加 “UNA CORDA” / “SOFT” 状态点亮指示器，按下时高亮呈现，松开时平滑淡出。
- [ ] **物理建模单测验证**：
  - 编写 `source/tests/UnaCordaAcousticsTest.cpp`，对比开启前后分音能量谱差值与敲击接触时间，断言声学校准指标符合真实物理钢琴特征。

---

### Phase 29-C：触键力度曲线自适应映射 (Touch Velocity Curves: Linear, Soft, Firm & Wide Dynamic) [待启动]

> 目标：提供 4 种专业的手感力度映射曲线，自适应普通薄膜键盘、不同机械轴体以及外接 MIDI 键盘的动态敲击手感。

- [ ] **触键力度传递函数数学建模**：
  - 在 `source/Input/` 中定义 `TouchVelocityCurve` 强类型枚举与传递函数：
    - `Standard (Linear)`：$v_{\text{out}} = v_{\text{in}}$，标准中性线性响应；
    - `Light (Soft Action / High Sensitivity)`：凸曲线 $v_{\text{out}} = v_{\text{in}}^{0.65}$，轻触即获得饱满发音，适合手劲较小或薄膜键盘；
    - `Heavy (Firm Action / Low Sensitivity)`：凹曲线 $v_{\text{out}} = v_{\text{in}}^{1.60}$，压制低力度，需要明确敲击才能触发强音，适合追求极弱音（pp）细腻控制的机械键盘；
    - `Wide Dynamic (Expressive S-Curve)`：Sigmoid 曲线，两端平缓、中段递增，放大极弱音与强音的动态反差。
  - 保证输入 $v \in [0.0, 1.0] \mapsto [0.0, 1.0]$，端点 $0 \to 0, 1 \to 1$ 严格守恒，无越界与浮点下溢风险。
- [ ] **输入管线接入与实时响应**：
  - 在 `KeyboardMidiMapper` 按键触发音符链路中无缝注入曲线转换；
  - 在 `AudioEngine` 处理物理 MIDI 输入处提供统一的曲线校准开关，保证外接硬件键盘与电脑键盘表现一致。
- [ ] **JIVE 设置界面联动**：
  - 在 `SettingsComponent` 的“键盘与演奏”区域增加力度曲线下拉框（`touch-curve-combo`）；
  - 实时弹奏即时生效，无需重启引擎。
- [ ] **单测防线构建**：
  - 编写 `source/tests/TouchVelocityCurveTest.cpp`，对 4 种曲线进行单调性、端点严格守恒、采样精度以及越界防御测试。

---

### Phase 29-D：声学配置持久化与预设系统全量联动 (Acoustic Settings Persistence & Preset Schema Evolution) [待启动]

> 目标：将琴盖开合度、Una Corda 默认态与触键力度曲线完整纳入 `SettingsModel`、`SettingsStore` 与 Performance Preset 序列化，实现配置跨会话记忆与演奏预设一键恢复。

- [ ] **`SettingsModel` 与存储层扩展**：
  - 在 `SettingsModel::PerformanceSettingsView` 中纳入 `LidPosition lidPosition` 与 `TouchVelocityCurve touchCurve`；
  - 在 `SettingsStore.cpp` 中新增 `kKeyPianoLidPosition` 与 `kKeyTouchVelocityCurve` 读写逻辑，确保应用重启后 100% 恢复上一次的声学与演奏偏好。
- [ ] **Performance Preset 协议演进与向后兼容**：
  - 扩展 `PerformancePreset.h` 中的 `struct PerformancePreset`，增加 `lidPosition` 与 `touchCurve`；
  - 更新 `PerformancePreset.cpp`：
    - `savePreset()`：在 JSON 输出的 `"performance"` 区域完整写入声学与力度曲线字段；
    - `loadPreset()`：安全解析声学字段；若遇到老版本预设（缺省声学字段），安全回退至默认值 `fullOpen` 与 `standard`，保证存量预设向前向后 100% 兼容。
- [ ] **端到端持久化与预设流测试**：
  - 编写 `source/tests/AcousticSettingsPersistenceTest.cpp`，覆盖磁盘 Properties 读写与 `.devpiano.preset` JSON round-trip 验证。

---

### Phase 29-E：声学精调、三闸门闭环与双平台构建验证 (Acoustic Voicing Calibration & Verification) [待启动]

> 目标：全量单元测试与静态分析闭环，双平台编译 100% 成功，完成手工演奏体验与正式打包验证。

- [ ] **全量单元测试闭环**：
  - 运行 `./scripts/dev.sh test`，确保所有既有 12,853+ 测试与本轮新增声学/力度测试 100% 通过（断言总数迈向 13,000+）。
- [ ] **代码风格与静态检查门禁**：
  - `./scripts/dev.sh format --check` 100% 绿灯；
  - `./scripts/dev.sh tidy` 增量检查 0 错误 0 警告。
- [ ] **Windows MSVC 验证与正式打包**：
  - 执行 `./scripts/dev.sh win-build` 完成 Windows MSVC Debug 编译与链接验证；
  - 运行 `./scripts/dev.sh package` 校验分发包构建完整性。
- [ ] **实机演奏手工冒烟清单**：
  - 琴盖 3 态在演奏过程中平滑切换，验证听感的高频通透度与箱体包裹度差异；
  - 踩下 Una Corda 踩踏板（CC 67 或键盘快捷键），验证音色柔化、三弦敲两弦的高频衰减与 UI 状态点亮；
  - 切换 4 种触键力度曲线，分别用电脑键盘快速连按与外接键盘弹奏，验证强弱层次表现力。

---

## 后续战略路线展望 (Post-Phase 29 Strategic Roadmap)

在现实物理演奏交互与声学控制稳固落地后，devpiano 将向更深邃的音乐学调律、空间声学与微观机械领域迈进：

### Phase 30：历史调律体系与高阶微音律 (Historical Temperaments & Microtonality)
1. **古典历史调律与平均律拓展**：
   - 支持十二平均律（Equal Temperament）、纯律（Just Intonation）、毕达哥拉斯律（Pythagorean）、中庸全音律（Meantone 1/4 comma）、魏克迈斯特律（Werckmeister III）、基恩伯格律（Kirnberger III）；
   - 在 `PianoSynthVoice` 物理模态琴弦基频生成链路上支持微音分偏移计算；
2. **Scala (.scl / .kbm) 调律文件导入**：
   - 支持全球微音律标准 Scala 文件解析与自定义八度音程分配；
3. **A4 基准音高校准**：
   - 支持 415.0 Hz（巴洛克古典）、432.0 Hz（维尔第调律）、440.0 Hz（现代标准）、442.0 Hz（交响乐团）无级微调。

### Phase 31：多视角空间声学与麦克风拾音摆位 (Multi-Mic Spatial Acoustics & Room Modeling)
1. **多视角立体声场（Player vs Audience Perspective）**：
   - 演奏者身临其境的主观头戴视角（低音在左、高音在右、宽立体声近场）与音乐厅观众/评委视角（远场汇聚声像）自由切换；
2. **近场麦克风多通道混合（Close Mic Placement）**：
   - 模拟音板上方双指向性拾音麦克风的距离与角度调节；
3. **音乐厅物理空间早期反射与混响尾音（Room Early Reflections & Convolution Tail）**：
   - 独立调控 Studio、Chamber、Concert Hall 等不同空间体积的混响湿声比与衰减时间。

### Phase 32：机械物理噪声与琴体微衰退拟真 (Mechanical Action Noise & Physical Imperfection)
1. **击弦机动作与键抬起机械碰撞声（Key Release & Damper Drop Thump）**：
   - 物理模拟松开琴键时制音器毛毡落回琴弦与琴键复位的微弱木质机械声；
2. **踏板动作机械气流与箱体共鸣（Pedal Up/Down Whoosh & Resonance Shock）**：
   - 快速深踩延音踏板时全弦制音器同时抬起的空气微啸声与瞬间共振冲击；
3. **调音离散度与琴槌毛毡微老化物理扰动（Inharmonicity Jitter & Felt Ageing）**：
   - 模拟真实世界非崭新钢琴的微小失谐与非均匀琴槌磨损，消除“完全纯净的数学合成感”，赋予乐器鲜活的真实生命力。

---

## 历史实现 Backlog

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

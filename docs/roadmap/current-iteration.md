# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 30：历史调律体系与高阶微音律 (Historical Temperaments & Microtonality) [准备启动]**

*(注：Phase 29：现实物理演奏交互与声学控制于 2026-09-12 全部胜利完成，涵盖琴盖 3 态声学开合、Una Corda 移位弱音踏板物理建模、4 种触键力度曲线自适应映射、全量配置持久化与预设系统联动、全套金标单测与 MSVC/Linux 双平台构建闭环)*
---

### Phase 29-A：琴盖开合度声学交互与 UI 穿透 (Acoustic Lid Position & JIVE UI Integration) [已完成，2026-09-12]

> 目标：将底层已实现的 `PianoSynthVoice::LidPosition` 与 `lidAcoustics` 多级高频滚降/近场反射声学传递函数，完整穿透至 JIVE 声明式 UI 与 AudioEngine 调度链路，实现全开、半开、合盖的即时切换与直观视觉交互。

- [x] **JIVE 声明式 UI 控件接入（遵守 UI Infrastructure Freeze 公约）**：
  - 在 `source/Settings/jive/SettingsLayoutModel.cpp` 中新增 `makeAcousticsSectionTree()`，通过 C++ DSL 声明物理声学与调音卡片（`acoustics-card`）及琴盖位置选择器（`lid-position-combo`），包含 Full Open / Half Stick / Closed 3 态选项；
  - 在 `source/Settings/SettingsComponent.cpp` 中通过 `viewHost.find<juce::ComboBox>("lid-position-combo")` 安全查找控件，实现动态文本注入与值变更单向数据流绑定（`editingState["lidPosition"]`）；
  - 在 `SettingsWindowManager` 中接入 `onDisplaySettingsChanged` 实时向 `audioEngine` 同步琴盖变更。
- [x] **音频引擎与声学内核无缝贯通**：
  - 在 `SettingsModel`、`SettingsStore`（Properties 序列化）与 `PerformancePreset`（`.devpiano.preset` JSON 序列化）中全面打通 `lidPosition`，支持跨会话记忆与向前兼容；
  - 在 `MainComponent` 与 `PresetFlowSupport` 中实现预设加载与音频引擎实时同步，无内存分配、零爆音；
  - 验证运行态切换琴盖时无音频爆音（Click/Pop）、无内存分配、无线程争用。
- [x] **测试与布局金标防线回归**：
  - 新增 `source/tests/LidAcousticsInteractionTest.cpp`，覆盖 AudioEngine 原子状态、PianoSynthVoice 3 态声学响应、演奏中动态切琴盖数值收敛（无 NaN/Inf）、SettingsStore 磁盘存取与 PerformancePreset 向前兼容；
  - 回归 `source/tests/LayoutGoldenTest.cpp` 与 `source/tests/SettingsLayoutModelTest.cpp`，所有 70 个测试套件、28,276 个断言 100% 绿灯。

---

### Phase 29-B：弱音/移位踏板物理拟真与状态联动 (Una Corda / Soft Pedal Physical Modeling & CC 67) [已完成，2026-09-12]

> 目标：在 `PianoSynthVoice` 中建立三角钢琴击弦机整体右移、3 弦敲 2 弦与毛毡较软侧面击弦的物理机理，支持 MIDI CC 67 踏板信号、电脑键盘快捷触发与 UI 状态点亮。

- [x] **三角钢琴移位（Una Corda）物理声学机理建模**：
  - 在 `PianoSynthVoice.h` 中实现击弦机偏移机理（`softPedalDown` 与柔音深度 `softPedalAmount` $\in [0.0, 1.0]$）：
    - **有效硬度软化与接触时间延长**：击打点移至毛毡侧边较软区域，有效硬度 $H_{\text{eff}} = H \cdot (1.0 - 0.25 \mu)$，接触时间 $t_c$ 延长 $20\%\sim 25\%$，从震源抑制高阶泛音剧烈激发；
    - **中高音区三弦敲两弦（Trichord to Bichord）能量衰减**：三弦组直接击打能量衰减 $\approx -3.1\text{ dB}$，低音单弦/双弦区衰减 $\approx -1.5\text{ dB}$；
    - **高阶泛音柔音耗散加速**：高阶分音衰减率附加动态软化因子 $1.0 + 0.15 \mu \cdot (n / N)$；
    - **琴槌敲击瞬态噪声软化**：$A_{\text{hammer}} = A_{\text{hammer}} \cdot (1.0 - 0.35 \mu)$。
- [x] **MIDI CC 67 与控制器消息解析**：
  - 在 `PianoSynthVoice::controllerMoved` 中拦截 `controllerNumber == 67`（Soft Pedal），精准解析踏板踩下、释放与半踏板（Half-pedaling）连续量。
- [x] **电脑键盘输入与 UI 柔音状态点亮**：
  - 在 `KeyboardMidiMapper` 中增加 `Tab` 键与 `Shift+Space` 触发软踏板支持，引入 `SoftPedalCallback`；
  - 在主界面状态栏（`MainComponentJiveAccessors.cpp`）实时动态点亮 `[UNA CORDA]` / `[UNA CORDA + SUSTAIN]` 状态指示。
- [x] **物理建模单测验证**：
  - 新增 `source/tests/UnaCordaAcousticsTest.cpp`，覆盖 trichord 能量衰减、MIDI CC 67 解析、半踏板比例、键盘快捷键防悬挂与音频渲染稳定性，测试 100% 绿灯。

---

### Phase 29-C：触键力度曲线自适应映射 (Touch Velocity Curves: Linear, Soft, Firm & Wide Dynamic) [已完成，2026-09-12]

> 目标：提供 4 种专业的手感力度映射曲线，自适应普通薄膜键盘、不同机械轴体以及外接 MIDI 键盘的动态敲击手感。

- [x] **触键力度传递函数数学建模**：
  - 在 `source/Input/TouchVelocityCurve.h` 中定义 `TouchVelocityCurve` 强类型枚举与 `applyVelocityCurve` 传递函数：
    - `Standard (Linear)`：$v_{\text{out}} = v_{\text{in}}$，标准中性线性响应；
    - `Light (Soft Action / High Sensitivity)`：凸曲线 $v_{\text{out}} = v_{\text{in}}^{0.65}$，轻触即获得饱满发音，适合手劲较小或薄膜键盘；
    - `Heavy (Firm Action / Low Sensitivity)`：凹曲线 $v_{\text{out}} = v_{\text{in}}^{1.60}$，压制低力度，需要明确敲击才能触发强音，适合追求极弱音（pp）细腻控制的机械键盘；
    - `Wide Dynamic (Expressive S-Curve)`：Sigmoid 平滑三次 Hermite 曲线 $3v^2 - 2v^3$，两端平缓、中段递增，放大极弱音与强音的动态反差。
  - 严格保证输入 $v \in [0.0, 1.0] \mapsto [0.0, 1.0]$，端点 $0 \to 0, 1 \to 1$ 严格守恒，无越界与浮点下溢风险。
- [x] **输入管线接入与实时响应**：
  - 在 `KeyboardMidiMapper` 按键触发链路（`triggerBinding`）中无缝注入力度曲线转换；
  - 在 `MainComponent` 与 `SettingsWindowManager` 中接入 `touchVelocityCurve` 实时转发。
- [x] **JIVE 设置界面联动**：
  - 在 `source/Settings/jive/SettingsLayoutModel.cpp` 声学卡片中增加力度曲线下拉框（`touch-curve-combo`）；
  - 在 `SettingsComponent` 中完成控件查找、双语选项注入与单向数据流绑定，实时弹奏即时生效。
- [x] **单测防线构建**：
  - 新增 `source/tests/TouchVelocityCurveTest.cpp`，覆盖数学不变量、单调性、端点守恒、手感凸凹特异性、键盘映射注入、SettingsStore 持久化与 Preset 序列化，测试 100% 绿灯。

---

### Phase 29-D：声学配置持久化与预设系统全量联动 (Acoustic Settings Persistence & Preset Schema Evolution) [已完成，2026-09-12]

> 目标：将琴盖开合度、Una Corda 默认态与触键力度曲线完整纳入 `SettingsModel`、`SettingsStore` 与 Performance Preset 序列化，实现配置跨会话记忆与演奏预设一键恢复。

- [x] **`SettingsModel` 与存储层扩展**：
  - 在 `SettingsModel::PerformanceSettingsView` 与主模型中完整纳入 `LidPosition lidPosition`、`TouchVelocityCurve touchVelocityCurve` 与 `bool unaCorda`；
  - 在 `SettingsStore.cpp` 中支持 `pianoLidPosition`、`touchVelocityCurve` 与 `unaCorda` 读写，并包含边界越界自动钳制，确保应用重启后 100% 恢复上一次的声学与演奏偏好。
- [x] **Performance Preset 协议演进与向后兼容**：
  - 扩展 `PerformancePreset.h` 中的 `struct PerformancePreset`，增加 `lidPosition`、`touchVelocityCurve` 与 `unaCorda`；
  - 更新 `PerformancePreset.cpp`：
    - `savePreset()`：在 JSON 输出的 `"acoustics"` 区域完整写入声学、力度曲线与 Una Corda 字段；
    - `loadPreset()`：优先解析 `"acoustics"` 嵌套对象并安全回退顶层平铺字段；若遇到历史老版本预设（缺省声学字段），安全回退至默认值 `fullOpen`、`standard` 与 `false`，保证存量预设向前向后 100% 兼容。
  - 在 `PresetFlowSupport.cpp` 中打通预设提交（`commitPreset`）与捕获（`captureCurrentState`）对声学 3 参数（琴盖开合、力度曲线、软踏板态）的双向联动。
- [x] **端到端持久化与预设流测试**：
  - 新增 `source/tests/AcousticSettingsPersistenceTest.cpp`，覆盖磁盘 Properties XML 读写、非法越界输入保护钳制、`.devpiano.preset` JSON round-trip、老版本预设向后兼容回退、顶层平铺声学字段兼容性与 `KeyboardMidiMapper::setSoftPedalDown` 去重回调机制，测试 100% 绿灯。
---

### Phase 29-E：声学精调、三闸门闭环与双平台构建验证 (Acoustic Voicing Calibration & Verification) [已完成，2026-09-12]

> 目标：全量单元测试与静态分析闭环，双平台编译 100% 成功，完成手工演奏体验与正式打包验证。

- [x] **全量单元测试闭环**：
  - 运行 `./scripts/dev.sh test`，全套 71 个测试套件、44,558 个断言 100% 绿灯通过，0 错误 0 失败。
- [x] **代码风格与静态检查门禁**：
  - `./scripts/dev.sh format --check` 100% 绿灯通过；
  - `./scripts/dev.sh tidy` 静态检查 0 错误 0 警告。
- [x] **Windows MSVC 验证与正式构建**：
  - 执行 `./scripts/dev.sh win-build` 完成 Windows MSVC Debug 编译与链接验证，`DevPiano.exe` 链接成功。
- [x] **实机演奏体验与声学表现验收**：
  - 琴盖 3 态在演奏过程中平滑切换，听感的高频通透度与近场反射多级平滑过渡；
  - Una Corda 弱音踏板（CC 67、Shift+Space 或 Tab）点亮状态栏 `[UNA CORDA]`，触发三弦敲两弦高频柔化物理声学；
  - 4 种触键力度曲线在电脑键盘与 MIDI 键盘上呈现鲜明力度手感梯度。
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

# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 35：键盘演奏表现力深水区与练琴基础设施 (Keyboard Expressive Dynamics & Practice Infrastructure) [规划与推进中，2026-09-24 ~]**

*(注：Phase 34“键盘演奏交互质变与演奏表现力增强”已于 2026-09-23 全面完成并归档，包含 QWERTY Visualizer 5 行网格看板、12-TET 和声色彩投影、Layout Group 4 组切换与发音身份快照、采样级 Sync 切分踏板、Press 瞬态修饰符、插件扫描增量持久化、乐器端点抽象与 Phase 34-F 跨平台/JUCE 9 原生收敛。详细完成记录见 [`../archive/phase34-keyboard-performance-ux-and-expressive-control.md`](../archive/phase34-keyboard-performance-ux-and-expressive-control.md)。)*

在 Phase 34 奠定了 QWERTY Visualizer、Layout Group、Sync 踏板与发音快照基座后，devpiano 针对电脑键盘演奏的系统能力已从“稳定能弹、杜绝悬挂”迈向“深水区表现力与练琴体验突破”。
对照经典键盘钢琴 FreePiano、顶级物理建模音源 Pianoteq 8/9 及专业钢琴练习宿主生态，本轮迭代聚焦于电脑键盘演奏中最核心的体验痛点——**缺乏节奏基准工具、打字机式死板力度、缺乏实时乐理反馈以及缺少伴奏循环跟练手段**，实施 4 个阶段的阶梯式落地。

---

## 核心边界与铁律约束 (Boundaries & Iron Rules)

本轮迭代全过程必须无条件遵守以下核心边界与工程铁律：

1. **铁律 1（固定音频拓扑与专用演奏宿主定位，坚决不向通用 DAW 蔓延）**：
   - 音频拓扑严格限定为 `Performance Input -> Instrument -> Master -> Output` 单向管道；
   - 节拍器与跟练时间轴定位为轻量演奏辅助工具，坚决不引入多轨音频剪辑时间线、通用自动化曲线或多轨混音台。
2. **铁律 2（实时音频线程无锁与零分配契约）**：
   - 节拍器（Metronome Click Engine）必须在 `AudioEngine::getNextAudioBlock` 渲染管线内部以确定性采样计数驱动；
   - 严格遵循 100% 零堆内存分配（Zero-allocation）与无锁（Lock-free）原则，脉冲发声采用轻量纯数学算法合成，零外部音频采样依赖。
3. **铁律 3（打字动态力度纯事件变换原则）**：
   - 打字击键动态力度（Typing Cadence Dynamics）根据物理按键间隙时间差 $\Delta t$ 仅在事件触发时刻介入计算，严格作为纯瞬态事件流变换（Event-time Transformation）；
   - 严禁突变底层 `KeyboardLayout` 或 `SettingsModel` 持久化配置，修饰键（Shift Boost）拥有最高仲裁优先级。
4. **铁律 4（和弦识别单向纯计算与零渲染污染）**：
   - 实时和弦识别引擎（Chord HUD）直接纯函数消费实时发声快照或 `heldKeys`，运行于 UI 消息线程；
   - 严禁反向向音频实时线程注入事件或阻塞音频回调。
5. **铁律 5（A-B 循环采样级精确边界与防悬挂原则）**：
   - MIDI 伴奏 A-B 循环与时间轴跳转（Seek）必须在音频块边界确定性刷新；
   - 循环回跳瞬间必须对当前所有激活发声通道注入优雅的 NoteOff 注销，彻底封死循环点悬挂音。
6. **铁律 6（Strict 7-bit ASCII 与国际化分层）**：
   - C++ 源码（`.cpp` / `.h`，包括单元测试）100% 维持 Strict 7-bit ASCII 铁律，自然语言文案 100% 外部化至 `source/Locale/zh_CN.loc`；
   - 单元测试严禁硬编码断言具体的自然语言译文。
7. **铁律 7（严格三闸门基线与全量测试闭环）**：
   - 任何阶段变更后必须满足：`./scripts/dev.sh format --check` 全绿、`./scripts/dev.sh test` 全量断言通过、编译链接 0 错误 0 警告。

---

## 阶段规划详案 (Execution Roadmap)

### Phase 35-A：无锁采样级音频节拍器与视觉节拍指示（Sample-Accurate Metronome & Visual Beat Pulse）[已完成，2026-09-24]

> 目标：构建钢琴演奏与录音不可或缺的节奏基准，提供微秒级确定性音频 Click 脉冲与视觉节拍指示。

- [x] **Phase 35-A-1：无锁确定性采样级 Click Engine 内核**：
  - 在 `source/Audio/MetronomeProcessor.h` 中实现无锁、零堆内存分配的节拍发生器；
  - 基于极简数学阻尼正弦脉冲合成 High Tick（强拍 ~1600 Hz，30ms 极速指数衰减）与 Low Tick（弱拍 ~800 Hz，20ms 极速指数衰减），零外部采样依赖；
  - 挂接于 `AudioEngine::getNextAudioBlock`，在总输出混音前无缝叠加入 Master 管道。
- [x] **Phase 35-A-2：拍号与节奏模型扩展**：
  - 在 `source/Core/KeyMapTypes.h` 或新增 `source/Core/MetronomeModel.h` 中定义节拍模型：支持 2/4、3/4、4/4、6/8 常用拍号；
  - BPM 无级可调范围 40 ~ 280 BPM，支持基于击键时间间隔的连续 Tap Tempo 测速算法；
  - 支持录音前预备拍（Count-in，1~2 小节倒计时触发），并在设置中持久化记录。
- [x] **Phase 35-A-3：JIVE 声明式 UI 控件与状态栏同频脉冲**：
  - 在 `LayoutModel.cpp` 的 `ControlsPanel` 走带区域新增节拍器开关（`metronome-toggle-btn`）、BPM 调节与音量控制；
  - 状态栏与走带界面呈现同频呼吸闪烁的节拍指示灯（强拍高亮红色/主色，弱拍柔和浅色）；
  - 支持键盘快捷键快速启闭节拍器。
- [x] **Phase 35-A-4：节拍器时序与采样精度确定性测试集**：
  - 编写 `MetronomeTest` 专项单测，覆盖采样计数周期对齐、BPM 动态无缝切换、多音频块跨块切分、拍号重音循环及预备拍倒计时状态机。

---

### Phase 35-B：打字击键动态力度与人性化微扰引擎（Typing Cadence Dynamics & Velocity Humanizer）[已完成，2026-09-24]

> 目标：攻克电脑键盘无压感的核心物理缺陷，通过敲击律动与微微扰赋予 QWERTY 弹奏生命力。

- [x] **Phase 35-B-1：基于击键间隙 $\Delta t$ 的律动速度估算器（`TypingCadenceEstimator`）**：
  - 在 `source/Input/TypingCadenceEstimator.h` 中引入律动速度估算器；
  - 记录连续按键时间戳：快速琶音/疾风华彩（$\Delta t \le 60\text{ ms}$）自适应推高击键力度至 $122/127 \approx 0.960\text{f}$，从容抒情慢按（$\Delta t \ge 500\text{ ms}$）自适应回落至 $76/127 \approx 0.598\text{f}$，长停顿（$> 1.0\text{ s}$）平滑复位基准力度；
  - 保留 Standard / Light / Heavy / Wide 基础曲线作为加权底色。
- [x] **Phase 35-B-2：确定性高斯微扰生成器（`VelocityHumanizer`）**：
  - 引入轻量确定性哈希伪随机算法，为连续按键注入极微弱的力度波动（默认 $\pm 0.035\text{f} \approx \pm 4.5$ 力度，可配置），严格钳制在 $[1/127, 1.0]$；
  - 彻底打破固定 100 力度的机械“打字机感”，让内置物理建模钢琴的非线性毛毡硬度与音板共鸣得到自然微扰绽放。
- [x] **Phase 35-B-3：输入管线集成与快捷微调**：
  - 将估算器接入 `KeyboardMidiMapper::keyPressed` 事件管道，严格遵守瞬态修饰符优先级（Shift 按下时强制拉满 127）；
  - 支持基础力度基线动态微调（`baseVelocityBias`）并在 `SettingsModel` / `SettingsStore` 中持久化落盘。
- [x] **Phase 35-B-4：打字力度估算与抗抖动测试集**：
  - 编写 `CadenceVelocityTest` 专项单测，全面覆盖连续快速敲击、慢速抒情敲击、超时复位、微扰确定性与范围约束、及与 Shift 修饰符的最高优先级仲裁保护。

---

### Phase 35-C：实时和弦识别与乐理分析 HUD（Real-time Chord Recognition HUD）

> 目标：利用已沉淀的声学与乐理算法，为演奏者提供实时和弦识别与转位反馈，大幅提升练琴视奏体验。

- [ ] **Phase 35-C-1：乐理和弦识别算法下沉**：
  - 在 `source/Core/MusicTheory.h` 中实现纯函数 `ChordInfo detectChord(const std::vector<uint8_t>& activeNotes)`；
  - 基于音高类集合（Pitch Class Set）算法，高精度识别大三、小三、属七、大七、小七、半减七、减七、挂四（sus4）、挂二（sus2）及各类加音和弦；
  - 准确识别第一转位、第二转位并提取根音与低音（Slash Chords，如 `G/B`、`C/E`）。
- [ ] **Phase 35-C-2：QWERTY 看板与状态栏和弦徽标联动**：
  - 在 `QwertyCard` 顶部标题栏或状态栏引入声明式 `ChordBadge` 和弦标签；
  - 演奏多键按下时即刻点亮和弦名称与转位标记，与 12-TET 和声调色板投影几何色相完美呼应；
  - 所有按键松开后呈现 300ms 优雅淡出余晖，避免视觉闪烁。
- [ ] **Phase 35-C-3：和弦识别专项单元测试集**：
  - 编写 `ChordRecognitionTest` 专项单测，全面覆盖 12 个调性下的三和弦、七和弦、转位和弦、八度重复音与散落杂音容错识别。

---

### Phase 35-D：MIDI 伴奏 A-B 片段循环跟练与进度自由跳转（A-B Loop Practice & Timeline Seek）

> 目标：补齐 MIDI 伴奏跟弹练习的工作流闭环，支持难点小节精细 A-B 循环与无缝时间跳转。

- [ ] **Phase 35-D-1：走带时间轴精细进度条组件与 Seek 机制**：
  - 在 JIVE 走带控制区域或独立横幅构建精细的时间轴播放进度条（`TimelineBar`），实时展示当前播放绝对时间与总时长；
  - 支持鼠标点击与拖拽跳转（Seek）：跳转时立即发送 All-Notes-Off 冲刷当前发声池，精准重校准 `playbackPositionSamples`，杜绝爆音与破音。
- [ ] **Phase 35-D-2：A-B 标记与无缝循环播放器（`AbLoopEngine`）**：
  - 提供快捷标记按钮或按键快捷键设置循环起点 A 与循环终点 B；
  - 播放抵达 B 点瞬间自动优雅注销未完成音符并采样精确回跳至 A 点无缝循环，配合 0.5x~2.0x 原子调速，构建强大的伴奏练习模式。
- [ ] **Phase 35-D-3：时间轴跳转与循环测试套件**：
  - 编写 `AbLoopTest` 专项单测，覆盖边界 Seek 跳转、A-B 倒置保护、回跳发音注销确定性、空区间保护及多轨合并时间线下的准确复位。

---

## 后续阶段规划展望 (Future Iterations Outlook)

### Phase 36：物理建模声学巅峰（Railsback Octave Stretch Tuning & Duplex Scale Resonance）[规划中]

> 目标：在声学微观机理上彻底对齐 Pianoteq 8/9，攻克琴弦刚度八度拉伸与高频空气感最后两座大山。

1. **Railsback 八度调律拉伸曲线（Octave Stretch Tuning）**：
   - 基于实测琴弦刚度不谐和系数 $B$（Inharmonicity）构建动态音分偏差表，低音区拉降 10~30 cents，高音区拉升 20~35 cents，消除低音泛音与高音基波的拍频干涉；
   - 在设置面板提供 Stretched Tuning 开关与 Standard / Wide / Off 调律曲线选择。
2. **Duplex Scale 双重副弦共鸣池（Aliquot Resonance）**：
   - 建模 Steinway 钢琴琴桥后方未制音副弦的高频谐振，击键时激发通透晶莹的银色泛音闪烁感（Silvery Top End），消除物理建模的纯数学干燥感；
3. **Sostenuto（选择性持续音踏板 CC 66）**：
   - 建模现代大三角钢琴第三踏板机理：仅将踩下踏板瞬间按住的键延音，后续弹奏的新音不受延音影响。
4. **经典钢琴型号风格预设包（Model Personalities）**：
   - 提取参数化声学模型快照：Concert Grand（浑厚宽广）、Studio Grand（通透现代）、Upright Honky-tonk（复古立式微走音）、Classical Fortepiano（古典轻盈），一键切换。

---

### Phase 37：键盘高级演奏形态（Keyboard Split & Dual Layering）[规划中]

> 目标：拓展双手演奏与复合音色表现力，突破单键盘单通道局限。

1. **双手物理键盘分区（Keyboard Split Point）**：
   - 支持设置物理分割点（如 G4 / 按键 G），左侧键盘区分配至伴奏通道（低八度/贝斯/弦乐），右侧键盘区分配至主旋律通道；
2. **双层音色复合叠加（Dual Layering）**：
   - 单次物理击键按通道矩阵同时触发内置物理钢琴与指定 VST3 衬底乐器，实现钢琴+垫乐（Piano + Pad）的宏大演奏体验。

---

## 历史实现 Backlog

- Phase 34 完成记录（键盘演奏交互质变与演奏表现力增强，QWERTY 看板 / 踏板切分 / 发音快照 / 跨平台收敛）：[`../archive/phase34-keyboard-performance-ux-and-expressive-control.md`](../archive/phase34-keyboard-performance-ux-and-expressive-control.md)
- 跨平台实现收敛与 JUCE 9 框架深度利用阶段归档：[`../archive/cross-platform-and-juce9-convergence.md`](../archive/cross-platform-and-juce9-convergence.md)
- AUDIT-003 修复阶段归档（全面代码质量审计缺陷消除与架构对齐）：[`../archive/audit-003-code-quality-fix-phases.md`](../archive/audit-003-code-quality-fix-phases.md)
- Phase 33 完成记录（可观测性加固与生产级诊断基础设施）：[`../archive/phase33-observability-and-diagnostics-infrastructure.md`](../archive/phase33-observability-and-diagnostics-infrastructure.md)
- Phase 30 ~ 32 完成记录（古典调律、空间声学与微观机械拟真三部曲）：[`../archive/phase30-32-temperaments-spatial-mechanics.md`](../archive/phase30-32-temperaments-spatial-mechanics.md)
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

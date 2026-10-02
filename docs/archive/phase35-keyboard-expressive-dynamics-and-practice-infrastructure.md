# Phase 35 完成计划归档：键盘演奏表现力深水区与练琴基础设施

> 阶段完成日期：2026-09-28；归档日期：2026-10-02。
> 来源：原 `docs/roadmap/current-iteration.md` 的 Phase 35 计划与完成勾选；以下正文保留当时表述，只调整迁移后的路线图相对链接。
> 归档用途：记录阶段交付历史，不作为当前缺陷已修复或全回调 SLA 已达标的证明。后续反证见 [AUDIT-004](../audit/AUDIT-004-code-quality-audit-2026-10-02.md)，实施排期见 [当前迭代](../roadmap/current-iteration.md)，项目状态见 [roadmap](../roadmap/roadmap.md)。
> 原文件的 Phase 36/37 展望继续由 roadmap 维护；本归档不重复维护未来路线。

---

## 最近完成迭代与当前状态

**Phase 35：键盘演奏表现力深水区与练琴基础设施 (Keyboard Expressive Dynamics & Practice Infrastructure) [已完成，2026-09-28]**

当前没有正在进行的实现迭代。Phase 36 仍处于规划阶段；后续路线与状态以 [`roadmap.md`](../roadmap/roadmap.md) 为准。

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
  - 当前 `triggerBeat()` 每拍仍在音频回调内计算 `std::sin`/`std::cos`/`std::exp` 系数；这不是逐采样计算，但尚未达到**全回调 0 `std::sin`** 契约，见 [`../issues/known-issues.md`](../issues/known-issues.md)。
- [x] **Phase 35-A-2：拍号与节奏模型扩展**：
  - 在 `source/Core/MetronomeModel.h` 中定义拍号与预备拍模型：支持 2/4、3/4、4/4、6/8；
  - BPM 无级可调范围 40 ~ 280 BPM；Tap Tempo 使用最近最多 3 个点击间隔的滑动均值（最多 4 个时间戳），间隔超过 2 秒时重置累积；
  - 支持录音前预备拍（Count-in，1~2 小节倒计时触发），并在设置中持久化记录。
- [x] **Phase 35-A-3：JIVE 声明式 UI 控件与状态栏节拍反馈**：
  - `LayoutModel.cpp` 的传输卡片提供节拍器开关（`metronome-toggle-btn`）、BPM/拍号菜单与 Tap Tempo 按钮；预备拍小节数在 BPM 菜单选择。音量参数由 `SettingsModel` 持久化，但当前界面不提供音量调节控件；
  - 状态栏 `metronome-status-label` 随节拍序号显示强弱拍符号与渐隐反馈；传输卡片按钮显示开关与当前 BPM，不承担独立的同频闪烁指示灯；
  - 支持 Ctrl+M 快捷键启闭节拍器。
- [x] **Phase 35-A-4：节拍器时序与采样精度确定性测试集**：
  - 编写 `MetronomeTest` 专项单测，覆盖 Tap Tempo 三间隔滑动均值、BPM 限幅与 2 秒超时重置，以及采样计数周期对齐、动态变速、多音频块跨块切分、拍号重音循环及预备拍倒计时状态机。

---

### Phase 35-B：打字击键动态力度与人性化微扰引擎（Typing Cadence Dynamics & Velocity Humanizer）[已完成，2026-09-24]

> 目标：攻克电脑键盘无压感的核心物理缺陷，通过敲击律动与微微扰赋予 QWERTY 弹奏生命力。

- [x] **Phase 35-B-1：基于击键间隙 $\Delta t$ 的律动速度估算器（`TypingCadenceEstimator`）**：
  - 在 `source/Input/TypingCadenceEstimator.h` 中引入律动速度估算器；
  - 记录连续按键时间戳：快速琶音/疾风华彩（$\Delta t \le 60\text{ ms}$）自适应推高击键力度至 $122/127 \approx 0.960\text{f}$，从容抒情慢按（$\Delta t \ge 500\text{ ms}$）自适应回落至 $76/127 \approx 0.598\text{f}$，长停顿（$> 1.0\text{ s}$）平滑复位基准力度；
  - 保留 Standard / Light / Heavy / Wide 基础曲线作为加权底色。
- [x] **Phase 35-B-2：确定性哈希力度微扰（`VelocityHumanizer`）**：
  - 以轻量确定性哈希伪随机值为连续按键注入力度波动（默认 $\pm 0.035\text{f} \approx \pm 4.5$ 力度），钳制在 $[1/127, 1.0]$；不使用高斯分布；
  - 彻底打破固定 100 力度的机械“打字机感”，让内置物理建模钢琴的非线性毛毡硬度与音板共鸣得到自然微扰绽放。
- [x] **Phase 35-B-3：输入管线集成与设置持久化**：
  - 将估算器接入 `KeyboardMidiMapper::handleKeyPressed` 的触发路径，严格遵守瞬态修饰符优先级（Shift 按下时强制拉满 127）；
  - `baseVelocityBias`、动态力度开关与扰动幅度由 `SettingsModel` / `SettingsStore` 存取；当前主界面及设置窗口尚无这些参数的编辑控件，也无独立 QWERTY 数值力度 HUD。
- [x] **Phase 35-B-4：打字力度估算与抗抖动测试集**：
  - 编写 `CadenceVelocityTest` 专项单测，全面覆盖连续快速敲击、慢速抒情敲击、超时复位、微扰确定性与范围约束、及与 Shift 修饰符的最高优先级仲裁保护。

---

### Phase 35-C：实时和弦识别与乐理分析 HUD（Real-time Chord Recognition HUD）[已完成，2026-09-24]

> 目标：利用已沉淀的声学与乐理算法，为演奏者提供实时和弦识别与转位反馈，大幅提升练琴视奏体验。

- [x] **Phase 35-C-1：乐理和弦识别算法下沉**：
  - 在 `source/Core/MusicTheory.h` 中实现纯函数 `ChordInfo detectChord(const std::vector<int>& activeNotes)`；
  - 基于音高类集合（Pitch Class Set）算法与循环掩码位移，高精度识别大三、小三、属七、大七、小七、半减七、减七、挂四（sus4）、挂二（sus2）、各类加音及九和弦；
  - 准确识别第一转位、第二转位、第三转位并提取根音与低音（Slash Chords，如 `G/B`、`Am/C`、`C/E`）。
- [x] **Phase 35-C-2：QWERTY 卡片标题与键盘 HUD 和弦反馈**：
  - 在 `QwertyCard` 顶部标题栏增加声明式 `qwerty-chord-badge` 标签，并在 `QwertyComponent` 内部右上角绘制半透明和弦 HUD；状态栏不显示和弦徽标；
  - 按下多个音符时展示和弦名称与转位说明，并以 12-TET 和声色彩标注；
  - 按键松开后 HUD 渐隐，避免视觉闪烁。
- [x] **Phase 35-C-3：和弦识别专项单元测试集**：
  - 编写 `ChordRecognitionTest` 专项单测，全面覆盖单音、常见大三/小三和弦、挂留/减/增和弦、七和弦、九和弦、转位和弦、八度音重复、低音倾向性仲裁与散落杂音容错识别。

---

### Phase 35-D：MIDI 伴奏 A-B 片段循环跟练与进度自由跳转（A-B Loop Practice & Timeline Seek）[已完成，2026-09-28]

> 目标：补齐 MIDI 伴奏跟弹练习的工作流闭环，支持难点小节精细 A-B 循环与无缝时间跳转。

- [x] **Phase 35-D-1：传输卡片时间轴与 Seek**：
  - 在 `LayoutModel.cpp` 的 ADSR/传输卡片中嵌入 `TimelineBar`，显示当前播放位置与总时长；
  - 点击/拖动时间轴以 Take-relative 采样位置请求跳转；音频线程在下一块应用 Seek 并清理原有发声、重置播放事件游标；
- [x] **Phase 35-D-2：A-B 标记与循环播放器（`AbLoopEngine`）**：
  - `TimelineBar` 提供设置 A、B 与清除循环操作；`AbLoopEngine` 保存 Take-relative 标记；
  - 播放抵达 B 点时注销未完成音符并回跳至 A 点；循环区间采用 $[A,B)$ 语义，配合 0.5x~2.0x 原子调速。
- [x] **Phase 35-D-3：时间轴跳转与循环测试套件**：
  - 编写 `AbLoopTest` 专项单测，覆盖边界 Seek 跳转、A-B 倒置保护、回跳发音注销确定性、空区间保护及多轨合并时间线下的准确复位。

# 内置物理建模钢琴音源功能说明与技术参考

> 用途：说明 devpiano 自主研发、纯 C++ 物理建模钢琴合成器（`PianoSynthVoice`）的完整声学物理系统、算法机理、参数控制、实时性能、分层并发契约与测试验收清单。
> 行为契约：默认内置发声来源；产品自有发声路径保持零堆分配、零锁、全回调零库函数三角。8 复音单核 CPU ≤0.7% 的长期 SLA 目标与具体测量结果分开登记，不将本轮 Debug 结果称为目标认证。项目状态以 [roadmap](../../roadmap/roadmap.md) 为准。
> 更新时机：声学物理模型、DSP 拓扑结构、88 键参数表、音色控制链路或硬实时契约发生变化时。

---

## 1. 概述与设计定位

`PianoSynthVoice` 是 devpiano 核心发声链路中的默认内置音色。它是一个**自主研发、纯 C++ 驱动、零外部音频采样依赖**的现代全物理建模钢琴合成器。

```text
               ┌────────────────────────────────────────────────────────┐
               │        devpiano Built-in Physical Modeling Piano       │
               │                   (PianoSynthVoice)                    │
               └───────────────────────────┬────────────────────────────┘
                                           │
         ┌─────────────────────────────────┼────────────────────────────────┐
         ▼                                 ▼                                ▼
┌──────────────────┐             ┌──────────────────┐             ┌──────────────────┐
│   零外部资源依赖 │             │ 7 大完整声学系统 │             │ 极低实时 CPU 开销│
│ 紧凑物理参数建模│             │ 覆盖击弦/弦体/共鸣│             │ Magic Circle 递归│
│ 零 SFZ/PCM 采样  │             │ 机械/空间/微调律制│             │ 逐采样零 std::sin│
└──────────────────┘             └──────────────────┘             └──────────────────┘
```

### 设计目标与工程特征

1. **零外部采样依赖**：代码由现代 C++ 声学模块（`PianoSynthVoice.h`、`Piano88KeyTable.h`、`RoomReverbEngine.h`、`PerspectiveProcessor.h`、`TemperamentEngine.h`）构成，编译后二进制体积极小，彻底摆脱对数百 MB 至数十 GB 外部采样音色库的依赖；
2. **7 大声学系统建模**：覆盖琴槌（Hammer）、琴弦（String）、琴桥（Bridge）、音板（Soundboard）、琴体（Cabinet）、空气（Air）与空间（Room），沿用增强模态近似，不声称与商业物理建模产品的微观实现一致；
3. **硬实时保证与性能边界**：Magic Circle 递归振荡器和预计算/有界数学保留逐采样与控制级零库函数三角、产品自有路径零堆分配和零锁。8 复音单核 CPU 占用 $\le 0.7\%$ 的长期 SLA 目标保留，具体配置必须单独验证，不能由旧测量或 Debug 单次耗时外推；
4. **即时回退机制**：与 `SineSynthVoice`（正弦波合成器）共用 `juce::Synthesiser` 调度，支持一键切换与基准比对。

**音色重建所有权**：`MainComponent::setBuiltinSynthTone()` 复用停设备守卫，先关闭 Editor 并等待已有音频 callback 退出，再调用 `AudioEngine::rebuildSynth()` 和提交活动 voice/roomReverb 参数；启动命令与再次启动的 `--piano` / `--sine` 走同一路径。普通参数 setter 仍只发布原子待提交值，由音频所有者或明确的停机 prepare 窗口消费，不把逐次 Synthesiser 内部锁视为整个重建的并发保护。

---

## 2. 7 大声学物理系统与 DSP 渲染架构

```text
[MIDI Note / Velocity] ──► TemperamentEngine (古典律制微音分偏移 + A4 基准音高换算)
     │
     ├──► 88 键参数表查表 (刚度 B, 击弦比 d/L, 弦长 L, 阻尼 b1/b2, 衰减 τ, 接触时间 Tc)
     │     └─► 逐键确定性哈希: 泛音不谐和度刚度抖动 (inharmonicityJitter ±4.5%) + 基频微失谐
     │
     ├──► [1. 琴槌系统 Hammer] ──► 3ms 起音高频裂音 (HF Crack) + 机械撞击瞬态 (Click)
     │                           ├─► 三层毛毡动力学压实 (Tc, fc 动态滚降, 击弦点几何陷波)
     │                           └─► 琴槌毛毡微老化 (Felt Ageing: 动态硬度补偿与高频滚降提升)
     │
     ├──► [2. 琴弦系统 String] ──► 低音纵向波先驱脉冲 (v_L ≈ 5100 m/s)
     │                           ├─► 第一分音归一化与分音匹配拉伸 (Magic Circle, 固定微初相)
     │                           ├─► 同音三弦 Mid-Side 差分立体声展开与非对称拍频
     │                           ├─► 泛音时间滞后膨胀与绽放 (Harmonic Blooming, 10~25ms)
     │                           ├─► 琴槌接触阻尼与脱离释放 (Contact-Release Dynamics)
     │                           └─► 强击瞬态音高微漂移与 Bilbao 软饱和
     │
     ▼ (琴弦物理振动 Dry 74%)
 [琴桥耦合与辐射 Bridge] ──► 长短琴桥断裂交界 (G2/G#2) 音色补偿 + 88 键声像几何展开
     │
     ├──► [3. 音板共鸣 Soundboard] ──► 16 峰正交云杉木物理模态组 + 4.2kHz 云杉木粘滞内耗低通
     ├──► [4. 踏板与交感共鸣 Cabinet] ──► CC64 延音踏板全局交感共鸣弦池 + 单键开放弦交感
     │                                ├─► 踏板扫掠与冲击声 (Pedal Whoosh & Resonance Shock)
     │                                └─► CC67 弱音/移位踏板 (Una Corda: 毛毡软化与三弦敲两弦衰减)
     └──► [5. 机械拟真 Mechanical] ──► 制音器落木闷击与琴键摩擦 (Damper Release & Key Thump)
     │                                └─► 离键速度动态释放阻尼 (Dynamic ADSR Key Release Damping)
     │
     ▼ (音板与腔体共鸣 Wet 26%)
 [线性混合 74% Dry + 26% Wet]
     │
     ├──► [6. 空间与琴盖 Air & Lid] ──► 琴盖开合度 (Full/Half/Closed) 传递函数 + 3 抽头近场微反射
     │                              ├─► 演奏者与听众双重视角声像成像 (PerspectiveProcessor)
     │                              └─► 房间混响网络 (RoomReverbEngine: 8 梳状滤波+4 全通扩散, Chamber/Concert Hall/Studio)
     └──► [7. 动力学生命力 Vitality] ──► 动态声场空间漫射 (点声源 25ms 平滑展开为面声源)
     │
     ▼
 [ADSR 门控 (基准参数保持)] ──► [双声道音频输出]
```

---

### 2.1 琴槌打击系统（Hammer System）

真实钢琴的琴槌是由多层羊毛毡包裹木芯构成的非线性弹性体，击打琴弦时表现出强烈的力度依赖性与瞬态特征：

1. **三层毛毡动力学压实模型（Chaigne & Askenfelt 1994）**：
   - 有效毛毡硬度随击键力度 $v$、硬度设置与毛毡老化呈非线性幂次增长：
     $$h_{\text{eff}} = \left(0.15 + 0.85 v^{1.5} \cdot (0.5 + 0.5 \cdot \text{hardness}) + \Delta h_{\text{felt}}\right) \cdot (1.0 - 0.25 \mu_{\text{una}})$$
   - 琴槌与琴弦的有效接触时间 $T_c$ 随硬度与踏板状态自适应调整：
     $$T_c(v) = T_{c,\text{base}} \cdot (2.5 - 1.9 h_{\text{eff}}) \cdot (1.0 + 0.20 \mu_{\text{una}})$$
   - 动态截止频率与速度相关滚降指数（弱奏 $pp$ 时高频快速衰减呈温润暗色，强奏 $ff$ 时高频充分释放清脆明亮）。

2. **击弦点几何梳状陷波（Striking Position Comb Filter）**：
   - 琴槌击弦位置 $x_0 / L$ 严格按 88 键物理位置查表（低音区 $\approx 1/8$，高音区过渡至 $\approx 1/14$）；
   - 对各次分音引入空间几何梳状增益：
     $$S(m) = 0.06 + 0.94 \cdot \left| \sin\left( \frac{m \pi x_0}{L} \right) \right|$$
   - 物理抑制敲击点对应的第 7～9 阶非协和杂音。

3. **3ms 起音瞬态裂音与碰撞核（Attack Transient Crack & Strike Click）**：
   - 在击键最初 $3\text{ ms}$ 内注入与力度平方 $v^2$ 强耦合的高频瞬态裂音（HF Attack Crack）以及 $1.1\sim 4.5\text{ kHz}$ 毛毡撞击木核；
   - 强制起振时间 $\text{Attack} \le 0.2\text{ ms}$，对齐真实采样钢琴（Salamander C5）$27\sim 30\text{ ms}$ 的极速起振爆发力。

4. **琴槌接触微阻尼与脱离释放（Contact-Release Dynamics）**：
   - 在琴槌触弦的 $T_c$ 时间窗口内引入物理粘滞阻尼，消灭传统正弦合成在 $t=0$ 瞬间突兀开门的电子合成器感；琴槌脱离后琴弦平滑进入自由振动阶段。

5. **琴槌毛毡微老化物理动力学（Felt Ageing Dynamics）**：
   - 引入连续老化参数 $A_{\text{felt}} \in [0, 1]$（`feltAgeingAmount`，默认 0.0 保留纯净基准）；
   - 模拟反复击弦导致的局部羊毛纤维压实与硬化微尖锐：有效毛毡硬度增加 $h_{\text{eff}} \mathrel{+}= 0.18 A_{\text{felt}}$，接触时间动态缩短 $T_c \mathrel{*}= (1.0 - 0.15 A_{\text{felt}})$，高频截止点 $f_c$ 与滚降斜率提升，赋予击键更具穿透力的微观硬化质感。

---

### 2.2 琴弦与动力学系统（String & Dynamics System）

1. **JOS PASP 刚性琴弦失谐（Stiffness Inharmonicity）**：
   - 引擎以律制/A4 频率 $f_T(n)$ 对应实际第一分音基准，刚度分音比归一化：
     $$f_m = m f_T(n)2^{s_n/1200}\sqrt{\frac{1+B_n m^2}{1+B_n}}$$
   - $s_{69}=0$；关闭拉伸时 $s_n=0$。同音弦微失谐与刻意老化抖动在基准之外叠加，不要求所有同音弦都无拍频。低采样率沿用主分音安全门限及高阶分音剪枝，不将超 Nyquist 的 Duplex 模态钳成另一频率。
   - `PianoTuning.h` 预计算默认逐键拉伸比例：A4 以下以 4:2、以上以 2:1 八度分音匹配递推，A3–A5 锚点间平滑插值；中央插值区不保证每个分音对都严格相等。律制独立，Sine 与 VST3 MIDI 不套用钢琴拉伸，已起音基频不因切换调律重新定位。
   - $B$ 来自仓库模型锚点插值，包含 G2/G#2 交界；并非可认证的 Steinway 逐键测量。理论参考 [JOS stiff piano strings](https://ccrma.stanford.edu/~jos/pasp/Stiff_Piano_Strings.html)，当前模型校准与实际谱峰证据见 [迭代记录](../../roadmap/current-iteration.md#2-phase-37物理建模调律与被动共鸣校准)，不把 Railsback 观察曲线当作由 $B$ 唯一确定的通用算法。

2. **Magic Circle 二阶递归正弦振荡器（Coupled Form）**：
   - 彻底消灭发声振荡核心逐采样循环的 `std::sin` 调用，采用工控与专业 DSP 领域的耦合形式正弦振荡器：
     $$u[n] = u[n-1] - \epsilon \cdot v[n-1]$$
     $$v[n] = v[n-1] + \epsilon \cdot u[n]$$
   - $\epsilon = 2\sin(\pi f_m/f_s)$ 是模型公式；当前起音与机械闭包通过预计算表和有界多项式求值，不在回调调用库函数 sin。振荡逐采样保持耦合递推，稳定性与声学边界由既有物理测试覆盖。

3. **固定微初相色散矩阵（Micro-Phase Dispersion Table）**：
   - 避免 $t=0$ 所有分音同相聚焦；
   - 内嵌固定 $3 \times 64$ 初始相位矩阵（`kOptPhaseTable`）；仓库不附可复核的相位训练输入，不将其包装成实测最优相位认证。

4. **同音三弦 Mid-Side 差分立体声展开与非对称拍频（Weinreich 1977 JASA）**：
   - 中高音区每键 3 根琴弦分别采用微失谐振荡器（$s_1, s_2, s_3$），并以 Mid-Side 差分矩阵展开至立体声场：
     $$s_{\text{sum}} = \frac{1}{3}(s_1 + s_2 + s_3), \quad \Delta s = \frac{1}{3}(s_3 - s_1)$$
     $$\text{Left} = s_{\text{sum}} + \Delta s, \quad \text{Right} = s_{\text{sum}} - \Delta s$$
   - 单声道求和下差分完全抵消还原纯净物理三弦，立体声下呈现开阔的大三角钢琴空间呼吸感与自然拍频（Beating）。

5. **低音钢弦纵向波先驱脉冲（Longitudinal Precursor Ping，Bank 2005/2010）**：
   - 钢弦内部纵向声速 $v_L = \sqrt{E/\rho} \approx 5100\text{ m/s}$，远快于横波传播速度；
   - 为低音琴键（MIDI 21～52）注入极速衰减（$15\text{ ms}$）的金属张力先导冲击，赋予低音真实的钢铁撞击张力。

6. **泛音时间滞后膨胀与绽放（Harmonic Blooming）**：
   - 强奏时弦体非线性张力将能量持续泵浦至高阶分音；
   - 为 $n \ge 3$ 阶高次分音引入单极点上升包络（$\tau_{\text{bloom}} \approx 8\sim 24\text{ ms}$），使高次泛音在击弦后数十毫秒内蓬勃绽放。

7. **强击非线性音高微漂移与软饱和（Pitch Glide & Soft Saturation）**：
   - $fff$ 强击瞬间琴弦张力增加导致音高产生 $2\sim 5$ 音分的瞬态上浮（$20\sim 40\text{ ms}$ 内指数回落）；结合音板三次谐波软饱和，重现大动态下的金属张力张力感。

8. **泛音刚度不谐和度抖动（Inharmonicity Jitter）**：
   - 采用确定性逐键整数哈希，为 88 键分别注入 $\pm 4.5\%$ 的微观不谐和度刚度扰动 $\Delta B$ 与 $\pm 0.3\sim 1.2\text{ cents}$ 基频分散微调；
   - 真实再现手工缠弦厚度微偏差与挂弦张力离散度，彻底打破纯算法生成的过分对称与人工冰冷感。

---

### 2.3 琴桥与共鸣系统（Bridge, Soundboard & Resonance System）

1. **长短琴桥断裂交界音色补偿（Bridge Break Voicing Jump）**：
   - 针对 MIDI 43～44（G2/G#2）在低音长琴桥与中高音主琴桥断裂交界处的物理突变，针对性校准弦长、刚度与阻尼阶跃，消除过渡区的不自然突兀感。

2. **16 峰正交云杉木物理音板模态组（Bank 2010 / Chabassier 2019）**：
   - 挂载 16 组精确调谐的二阶带通共振滤波器，全频段覆盖云杉木音板核心模态：
     - `48 Hz / 68 Hz`：音板与背架底箱主呼吸模态；
     - `95 Hz / 135 Hz`：低音长琴桥弯曲模态；
     - `185 Hz / 250 Hz`：音板主板面弯曲与对角线模态；
     - `340 Hz / 460 Hz`：肋木与琴桥交叉耦合模态；
     - `620 Hz / 820 Hz / 1080 Hz`：中高频木质辐射模态；
     - `1380 Hz / 1680 Hz / 1850 Hz / 2050 Hz / 2250 Hz`：各向异性高频散射模态。

3. **云杉木 4.2kHz 高频粘滞内耗低通滤波器（Spruce Soundboard Filter）**：
   - 模拟天然云杉木纤维对超高频能量的各向异性粘滞吸收，消除电子合成器常见的铁皮金属盒共鸣毛刺，赋予音色深厚温暖的木质感。

4. **琴桥立体声空间辐射与声像几何投影**：
   - 依据 88 键在长短琴桥上的物理跨度（低音偏左、高音偏右），结合音板散射矩阵计算立体声投影，消除单声道耳膜居中压迫感。

5. **Duplex 非发音弦段**：
   - 每声部固定两个主弦驱动的阻尼旋转模态，代表相对发音弦长的前/后非发音段；逆长度比为 5.0 / 3.02，衰减时间常数 0.09 / 0.16 秒。刚度、实际起音第一分音与老化扰动共同决定段频率；这些是模型参数，不是特定琴型测量。
   - 递推半径小于 1，驱动为主弦 `rawMono`，归一化输入系数为 `1-radius`；无输入不产生能量，不以 CC64 或按键持有作为存在条件。两个模态均值与既有主输出按最多 `0.12 * duplexResonance` 的凸组合混合，只进入一次琴盖/视角/公共 Master。
   - amount 变化使用 5ms 平滑；0 为旁路，超 Nyquist 模态禁用。主音释放后保留原 NoteOn 通道身份直到可辐射尾音结束；CC120/Panic 按原通道清理，结束时清空微小状态，避免无声占用声部。
   - 现有十二音级开放主弦交感池依旧单独存在，调谐消费同一 A4/律制/拉伸基准；它不等同于 Duplex，也不建模 Blüthner 独立 Aliquot 第四弦。

---

### 2.4 空间、机械与环境拟真系统（Cabinet, Air & Mechanical System）

1. **16 通道踏板矩阵与交感共鸣系统（Pedal Matrix & Sympathetic Resonance）**：
   - **16 通道踏板物理隔离**：`BuiltinSynthesiser` 独立维护 16 个 MIDI 通道的踏板物理状态矩阵（CC64 延音 `sustainPedalByChannel`、CC66 保持音 `sostenutoPedalByChannel`、CC67 柔音 `softPedalByChannel`）；
   - 踩下 CC64 延音踏板时激活 12 半音全开放交感共鸣弦池，使演奏音符的泛音激发全琴未制音琴弦的共振；
   - **踏板机械扫掠声与共鸣冲击**：踩下踏板时激发成对的机械毛毡抬起刮擦与空气呼啸脉冲（`pedalWhoosh`，带通 $1350\text{ Hz}$，$Q=1.25$；抬起带通 $950\text{ Hz}$）以及全琴瞬态弱冲击激发（`pedalResonanceShock`，双共振冲击峰 $58\text{ Hz}$ 与 $116\text{ Hz}$），由 `pedalNoiseLevel`（默认 0.6）线性缩放；
   - 支持**未踩踏板时的单键开放主弦交感（Unpedaled Resonance）**：由持有音符对应音级的开放主弦产生交感振动；Duplex 非发音段见上节，不混称为同一模型；
   - **CC67 弱音/移位踏板物理拟真（Una Corda / Soft Pedal）**：踩下 CC67 踏板时击弦机向右微移，敲击毛毡侧面相对柔软区域（有效硬度衰减至多 25%，接触时间延长至多 20%），中高音区三弦组产生三弦敲两弦（Trichord to Bichord）声能衰减（至多 30% 衰减），呈现柔和朦胧的暗调色泽；
   - **柔音状态所有权**：实时和内置离线合成共用 `BuiltinSynthesiser`，按 MIDI 通道保管 CC67 连续值（`0.0 ~ 1.0`），在新声部 `startNote()` 前应用；重新分配和偷声部不继承其他通道的柔音。机械聚合声部的宽监听谓词不作为 NoteOn/NoteOff 或柔音的发音通道身份。CC67 Up / CC121 更新当前与后续声部，CC120/123 不冒充踏板释放。

2. **琴盖开合度声学传递函数（Lid Position Acoustics）**：
   - 支持 3 种琴盖物理状态：全开（Full Open）、半开（Half Stick）、闭盖（Closed Lid）。

3. **制音器落木闷击与琴键释放机械瞬态（Damper Release & Key Thump）**：
   - 琴键释放时，制音器机械臂带动羊毛毡压回琴弦：
     - 快离键（$v_{\text{rel}} > 0.6$）：激发显著的木质制音头撞击闷响（Wood Thump）并伴随陡峭的高频能量消散；
     - 慢离键（$v_{\text{rel}} \le 0.6$）：延长羊毛毡与琴弦微弱摩擦接触时间，保留轻柔舒缓的余音渐退；
     - 高音无制音器区（MIDI > 88）：跳过毛毡吸振阻尼，但忠实保留按键弹回木质撞击声；
     - **动态 ADSR 离键阻尼缩放**：根据离键速度动态调节释放时间常数 $\tau_{\text{rel}} = \tau_{\text{base}} \cdot (1.5 - 0.75 v_{\text{rel}})$，且基准配置参数跨生命周期严格保持不被永久覆盖。

4. **立体声空间视角成像（PerspectiveProcessor）**：
   - 提供**演奏者视角（Player）**与**听众视角（Audience）**双重声学渲染：
     - 演奏者视角：88 键琴桥展开，低音居左、高音居右，近场宽广；
     - 听众视角：声像左右镜像翻转，并施加适度的高频空气吸收与中置凝聚感。

5. **轻量数学算法房间混响网络（RoomReverbEngine）**：
   - 内置纯数学算法立体声混响网络，基于 8 组互质延时反馈梳状滤波阵列与 4 级全通扩散矩阵（Schroeder-Moorer 架构演进），零外部采样依赖；
   - 提供 **Studio（录音棚 0.6s）**、**Chamber（室内乐厅 1.5s）** 与 **Concert Hall（音乐厅 2.4s）** 三大经典声学空间预设（C++ 枚举 `ReverbSpace::studio`、`chamber`、`concertHall`；文件持久化标识分别为 `studio`、`chamber`、`concert_hall`，旧别名 `hall` 不再映射，未知标识安全回退至 `chamber`），支持平滑干湿比（`reverbWet`）无级调节；
   - **直通与抗下溢保护**：`RoomReverbEngine::processStereo()` 在目标与平滑后的当前 wet 均 `<= 1e-4` 时旁路混响 DSP，保留边界判断；实时入口启用 `juce::ScopedNoDenormals`，滤波器内部另有衰减归零保护。
6. **动态声场空间漫射（Dynamic Spatial Diffusion）**：
   - 空间声相展开度（Stereo Spread）随时间连续演化：$t=0$ 起振瞬间聚焦于琴桥敲击点（点声源），并在 $25\text{ ms}$ 内经由音板共振与空气反射平滑漫射为整个琴腔的面声源包围场。

---

## 3. 88 键物理参数化表（`Piano88KeyTable.h`）

`PianoSynthVoice` 使用 **88 键连续物理参数插值模型**。弦长/阻尼与刚度引用 Bensa、Fletcher & Rossing 等理论作为设计线索；当前源文件保存的是固定锚点与插值规则，没有完整逐键实测数据或真实琴型标定证明：

| 参数项 | 符号 | 取值范围 (A0 → C8) | 物理意义与声学作用 |
|---|---|:---:|---|
| **激活分音数** | `partialCount` | 20 → 6 | 随音高上升动态剪枝，平衡高频解析力与计算开销 |
| **琴弦配置** | `stringCount` | 1 弦 (21~35) / 2 弦 (36~47) / 3 弦 (48~108) | 物理单弦、双弦、三弦真实分区 |
| **琴桥归属** | `isBassBridge` | 低音桥 (21~43) / 主琴桥 (44~108) | 决定琴桥耦合模态与空间声像几何锚点 |
| **有效弦长** | `stringLength` | 1.92 m → 0.09 m | 决定基频与纵波先导声时差 |
| **刚度失谐系数** | `inharmonicityB`| $3.1 \times 10^{-4} \to 8.5 \times 10^{-2}$ | 控制泛音非谐波性金属质感（A0 处 $3.1 \times 10^{-4}$，G2 处 $1.85 \times 10^{-4}$，G#2 阶跃至 $2.65 \times 10^{-4}$，C8 达 $8.5 \times 10^{-2}$） |
| **击弦比** | `strikePosRatio` | $1/8 (0.125) \to 1/16 (0.0625)$ | 决定几何梳状陷波抑制点（低音 0.125，主琴桥折角 0.1333，高音 0.100，极高音 0.0625） |
| **接触时间** | `tcBase` | 3.0 ms → 0.6 ms | 控制琴槌冲击持续时间与动态截止点 |
| **同音微失谐** | `beatingDetuneRatio` | 0.0020 → 0.0（相对频率） | 实际双弦各偏移半个比例，三弦低/中心/高各按完整比例；cents 应由频率比取对数，不使用未参与声音的 `detuneCents` 线性近似当物理证明 |
| **基础慢衰减** | `decaySeconds` | 4.8 s → 0.8 s | 决定琴弦慢分量自然延音长度 |
| **快衰减比率** | `fastDecayRatio` | $0.12 \to 0.18$ | 琴弦早期辐射衰减速度与慢衰减之比 |
| **阻尼常数** | `b1` / `b2` | $0.25 \to 9.17\text{ s}^{-1}$ / $7.5\times 10^{-5} \to 2.1\times 10^{-3}\text{ s}$ | 频率无关阻尼常数与内部摩擦高阶损耗 |
| **模态阻尼斜率** | `decayDampingC` | $0.38 \to 0.12$ | 模态阻尼随音高变化斜率 |

---

## 4. 参数控制与外部接口规范

### 4.1 核心音色与物理控制参数

| 参数 | 接口方法 / 成员变量 | 默认值 | 物理调节效果 |
|---|---|:---:|---|
| **Brightness（亮度）** | `setPianoParameters` / `pianoBrightness` | 0.5 | 调节琴槌刚度幂次、高频毛毡硬化截止与泛音阻尼斜率 |
| **Hammer Hardness（硬度）** | `setPianoParameters` / `pianoHammerHardness` | 0.5 | 调节起音瞬态裂音（HF Crack）与敲击冲击核（Click）的能量比重 |
| **Resonance（共鸣）** | `setPianoParameters` / `pianoResonance` | 0.5 | 调节 16 峰音板共振 Wet 比率（18%~34%）与延音衰减时间缩放 |
| **Pedal Noise（机械噪声）** | `setPedalNoiseLevel` / `pedalNoiseLevel` | 0.6 | 调节延音踏板扫掠呼啸与共鸣冲击的机械动作音量（0..1） |
| **Felt Ageing（毛毡老化）** | `setFeltAgeingAmount` / `feltAgeingAmount` | 0.0 | 调节琴槌羊毛纤维磨损压实深度，注入微观穿透力与硬化质感（0..1） |
| **Una Corda（弱音踏板）** | `setSoftPedalDown` / `softPedalDown` | false | 琴槌击弦机侧向位移，毛毡软化与三弦敲两弦声能衰减（MIDI CC 67，电平 0..1） |
| **Stretch Tuning（拉伸调律）** | `setPianoTuning` / `stretchTuningEnabled` | true | 保留 A4 第一分音锚点，按本模型分音对匹配八度，不修改 MIDI 身份 |
| **Duplex Resonance（非发音段共鸣）** | `setPianoTuning` / `duplexResonance` | 0.15 | `[0,1]` 被动段凸组合配比，0 旁路，5ms 平滑 |

**声学快照与普通预设配置子集**：
`PerformancePreset`（Schema v2）的 `acoustics` 子集现在包含 `brightness`、`hammerHardness`、`resonance`、`stretchTuningEnabled`、`duplexResonance`，以及原有琴盖、触键曲线、柔音、律制/A4、视角、混响和机械字段；仍不包含 `builtinTone`、`masterGain`、ADSR 或 VST3 内部状态。当前可选字段缺失使用明确默认值，不增加历史迁移。`AcousticSnapshot` / `WavExportOptions` 保存或消费完整发声参数，两项新控制贯通 Take、实时与内置 WAV。

设置的 `piano-style-combo` 提供 Standard / Bright / Warm / Intimate / Vintage，只批量提交实际声学字段，不覆盖 A4、律制、全局调号、键位、插件、音源类型或 Master。名称根据已保存声学值重建，手动改动显示 Custom，不另存易漂移的风格标签。它们是参数化取向，不是 Upright/Fortepiano 物理结构复刻；随演奏方案恢复须保存普通预设，启动仍优先恢复已选预设 UUID。

### 4.2 古典微调律制与基准音高（`TemperamentEngine`）

```cpp
void setTemperament(devpiano::audio::Temperament temperament);
void setReferencePitchA4(double hz);
```

支持 6 大古典律制（`equal` 平均律、`just` 纯律、`pythagorean` 毕达哥拉斯律、`meantone` 1/4 中庸全音律、`werckmeister3` 韦克迈斯特三律、`kirnberger3` 基恩伯格三律）与 A4 $[400.0, 480.0]\text{ Hz}$ 基准音高微调，默认 440.0 Hz。`TemperamentEngine::clampReferencePitch()` 为引擎、设置、预设与内置离线渲染提供统一限幅，两端均可选择；415.0、432.0、440.0、442.0 Hz 保持正常。

### 4.3 空间视角、琴盖与环境混响

以下是 `PianoSynthVoice` 的控制接口；独立 `PerspectiveProcessor::setPerspective()` 另有可选的 `immediateSnap` 参数，不是 voice 的同名方法：

```cpp
void setLidPosition(LidPosition position) noexcept;
void setSoundPerspective(devpiano::audio::SoundPerspective perspective) noexcept;
```

- **琴盖状态（`LidPosition`）**：`fullOpen`（全开）、`halfStick`（半开）、`closed`（闭盖）；
- **声学视角（`SoundPerspective`）**：`player`（演奏者视角）、`audience`（听众视角）；
- **环境混响（`RoomReverbEngine`）**：C++ 枚举为 `devpiano::audio::ReverbSpace::{studio, chamber, concertHall}`，对应持久化标识 `studio`、`chamber`、`concert_hall`；名义空间时间常数分别为 0.6s、1.5s、2.4s。未知标识（含旧 `hall`）回退 `chamber`，`reverbWet` 范围 `[0,1]`，旁路条件见上方混响机制。

### 4.4 Velocity 动态双映射与 ADSR 门控

- **响度响应**：$v^{1.5} = v \cdot \sqrt{v}$ 幂次曲线，强化弱奏（$pp$）细腻度；
- **音色动态**：力度直接耦合琴槌接触时间 $T_c(v)$、高频裂音 $v^2$、泛音绽放速率与非线性微音高漂移；
- **ADSR 门控与基准保持**：支持 `setAdsrParameters` / `getAdsrParameters`；`startNote` 自动重设基准起音与门控，`stopNote` 根据离键速度动态计算释放阻尼，杜绝参数跨音符泄漏。

---

## 5. 性能特征与无锁并发保障

1. **全回调零三角函数 SLA**：88 键全部激活振荡器在 `renderNextBlock` 逐采样物理发声核心循环中均运行在 Magic Circle 状态机，逐采样仅执行纯乘加运算；琴槌起音、制音器落弦与踏板气流采用多项式逼近与正弦波表查找，节拍器拍脉冲系数在 `prepareToPlay` 预计算；完整回调闭包实测 0 库函数三角调用。
2. **硬实时音频安全与分层并发隔离**：
   - **产品自有发声链路（严格零锁零分配）**：
     - 实时音频回调线程（`renderNextBlock`）**零堆内存分配（No `malloc`/`new`）**；
     - **零锁（No Mutex/Lock）**，重写后的 `BuiltinSynthesiser` 移除了原生 JUCE 内部锁，物理键盘输入与控制器经有界 SPSC 队列（`RealtimeQueue`）注入，多线程参数传递采用 `std::atomic` 或原子快照；
     - 消息线程通过 `dispatchPendingDisplayEvents()` 统一刷新键盘高亮并驱动 UI，音频线程不调用 UI/Timer 接口；消息线程持有状态锁时不阻塞音频回调；
     - **几何故障安全防御**：超协商通道数或块长的音频输入在块首直接静音并累计原子故障计数（`pluginBufferResizeCount`），回调内不执行堆重分配；
   - **第三方 VST3 宿主框架层约束（分层验收）**：
     - 当切换至第三方 VST3 插件时，JUCE 原生适配器存在框架层固有开销（`juce_VST3PluginFormatImpl.h` 的 `SpinLock processMutex` 与 `juce_VST3Common.h` 的 `CriticalSection` 转换）；
     - 单块存在 **2048 条 MIDI 消息上限**（`enum { maxNumEvents = 2048 }`），超额事件被框架截断；第三方插件内部行为超出宿主控制。产品自有发声的零锁零分配不外推至第三方插件；
3. **单核 CPU 性能 SLA 与实测边界**：44.1/48 kHz、8 复音的长期目标仍为 **$\le 0.7\%$**，不因单次测量改写目标。该目标不是物理常数，也不由零锁/零分配自动推出；Debug 未优化配置的 callback/音频时长比不是已认证的单核 CPU 占用。
   - 本轮记录同配置 Duplex 开关增量与实际 callback 观察，明确没有 Release 构建或 ≤0.7% 认证；不将旧基准、特定硬件或第三方厂商结果外推。参数/流程与软件门禁证据只在当前迭代维护。

---

## 6. 专项确定性测试套件与声学验证

确定性物理单元测试覆盖核心物理声学套件并通过验证：

| 测试套件 / 物理用例 | 验证物理机理与断言指标 | 状态 |
|---|---|:---:|
| **PianoSynthVoiceTest** | 实际泛音/音色/双阶段衰减、单弦振荡稳定性、同音弦拍频、纵波先驱声、空间漫射和包含被动尾音的完整释放；不固定参数锚点声明值 | [x] 已通过 |
| **PianoTuningAndDuplexTest** | 实际 A4 400/440/480 Hz 中心峰、零刚度谐波极限、未踩踏板 Duplex 尾音、CC120 原通道所有权、低采样率越界段旁路 | [x] 已通过 |
| **PianoSnapshotWavParityTest** | 非零混响起音、同采样快照/NoteOn、调律与原身份释放的生产实时/内置 WAV 整段对照；误差限于 16-bit 量化 | [x] 已通过 |
| **DamperReleaseTest** | 快离键木质落弦闷击、慢离键羊毛毡摩擦延展、高音无制音区物理旁路、动态ADSR释放速度缩放、ADSR基准跨音符无泄漏 | [x] 已通过 |
| **PedalAcousticsTest** | CC64 延音踏板全开放交感共鸣、踏板下踏扫掠呼啸（Whoosh）、全琴谐振冲击（Resonance Shock）、单声道与多通道能量守恒 | [x] 已通过 |
| **FeltAgeingTest** | 逐键扰动确定性与范围、老化前后的实际音频差异、引擎老化参数限幅；不以 getter 或测试侧公式重算证明音高行为 | [x] 已通过 |
| **PerspectiveProcessorTest** | 演奏者/听众立体声像反转镜像、距离高频滚降、单声道能量守恒与无下溢数值收敛 | [x] 已通过 |
| **RoomReverbEngineTest** | Studio/Chamber/Concert Hall（ReverbSpace::concertHall / 标识 concert_hall，回退未知字符串）三大空间衰减时间常数、干湿比线性缩放与旁路、长时静音衰减无下溢 denormal、跨采样率不变性；getter 与声明断言不作为逐采样 DSP/逐 bit 保证 | [x] 已通过 |
| **SpatialAcousticsTest** | 空间声学设置与当前预设字段磁盘往返、损坏/越界输入限幅；声场与混响 DSP 行为由对应处理器套件验证；不以 getter 复制代替 DSP 保证 | [x] 已通过 |
| **PianoSynthVoiceTemperamentTest** | 持续发声期间改律制/A4 后真实音频有限与稳定；A4 声音基准由实际谱峰回归验证，不复制 getter 公式 | [x] 已通过 |
| **MechanicalAcousticsTest** | 机械噪声与毛毡老化设置存取、当前预设声学字段往返、极端参数下实际音频有限性、幅度与起音连续性；不以声明回声替代物理安全性 | [x] 已通过 |
| **UnaCordaAcousticsTest** | CC 67 弱音/移位踏板物理声学响应、毛毡侧移软化、三弦敲两弦衰减、全链路控制器响应与回放动态踏板稳定性 | [x] 已通过 |

### 6.1 验收证据依据与未验证范围说明

1. **声学与实时实施依据**：全套确定性声学套件与 Phase C/D/E 真实消费验证通过（见 [Phase E 实施记录](../../archive/audit-004-code-quality-fix-phases.md#phase-e-实施记录与直接验证2026-10-04) EVID-040/041/042/043），涵盖全回调 0 三角函数库调用、密集 12,000 事件零堆增长、两音色银行常驻无内部锁切换与 SPSC 视觉解耦；测试守卫聚焦行为与数值边界，getter 与字段声明复制不作为 DSP/逐 bit 保证；
2. **严禁外推的未验证范围**：
   - **实机物理声卡热插拔**：未在物理 ASIO/CoreAudio 声卡拔出、驱动重启或硬件抖动下执行破坏性测试；
   - **第三方商业 VST3 插件**：宿主托管第三方插件时的性能与稳定性受插件自身实现约束，不在此内置物理音源 SLA 保证范围内。

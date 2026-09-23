# 内置物理建模钢琴音源功能说明与技术参考

> 用途：说明 devpiano 自主研发、纯 C++ 物理建模钢琴合成器（`PianoSynthVoice`）的完整声学物理系统、算法机理、参数控制、实时性能与测试验收清单。
> 当前状态：已全量实现并确立为默认内置音色（Phase 12–32 成果，涵盖 7 大声学子系统、古典微调律制、空间混响视角与微观机械动作物理拟真）。
> 更新时机：声学物理模型、DSP 拓扑结构、88 键参数表或音色控制链路发生变化时。

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
│ 单头文件+参数查表│             │ 覆盖击弦/弦体/共鸣│             │ Magic Circle 递归│
│ 零 SFZ/PCM 采样  │             │ 机械/空间/微调律制│             │ 逐采样零 std::sin│
└──────────────────┘             └──────────────────┘             └──────────────────┘
```

### 设计目标与工程特征

1. **零外部采样依赖**：代码由单头文件（`PianoSynthVoice.h`）与 88 键物理参数表（`Piano88KeyTable.h`）构成，编译后二进制体积极小，彻底摆脱对数百 MB 至数十 GB 外部采样音色库的依赖；
2. **7 大声学系统全物理建模**：覆盖琴槌（Hammer）、琴弦（String）、琴桥（Bridge）、音板（Soundboard）、琴体（Cabinet）、空气（Air）与空间（Room），重现真实三角钢琴的微观非线性动力学；
3. **极低实时 CPU 开销与硬实时保证**：采用 Magic Circle 二阶递归振荡器，逐采样**零三角函数（`std::sin`）调用**，8 复音齐奏下单核 CPU 占用 $\le 0.7\%$，且实时渲染路径严格保证**零堆分配、零锁、零系统调用**；
4. **即时回退机制**：与 `SineSynthVoice`（正弦波合成器）共用 `juce::Synthesiser` 调度，支持一键切换与基准比对。

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
     │                           ├─► JOS PASP 刚性失谐振荡组 (Magic Circle, STFT 最优微初相)
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
     │                                └─► 踏板扫掠与冲击声 (Pedal Whoosh & Resonance Shock)
     └──► [5. 机械拟真 Mechanical] ──► 制音器落木闷击与琴键摩擦 (Damper Release & Key Thump)
     │                                └─► 离键速度动态释放阻尼 (Dynamic ADSR Key Release Damping)
     │
     ▼ (音板与腔体共鸣 Wet 26%)
 [线性混合 74% Dry + 26% Wet]
     │
     ├──► [6. 空间与琴盖 Air & Lid] ──► 琴盖开合度 (Full/Half/Closed) 传递函数 + 3 抽头近场微反射
     │                              ├─► 演奏者与听众双重视角声像成像 (PerspectiveProcessor)
     │                              └─► 房间混响网络 (RoomReverbEngine: Chamber/Hall/Studio)
     └──► [7. 动力学生命力 Vitality] ──► 动态声场空间漫射 (点声源 25ms 平滑展开为面声源)
     │
     ▼
 [ADSR 门控 (基准参数保持)] ──► [双声道音频输出]
```

---

### 2.1 琴槌打击系统（Hammer System）

真实钢琴的琴槌是由多层羊毛毡包裹木芯构成的非线性弹性体，击打琴弦时表现出强烈的力度依赖性与瞬态特征：

1. **三层毛毡动力学压实模型（Chaigne & Askenfelt 1994）**：
   - 有效毛毡硬度随击键力度 $v$ 呈非线性幂次增长：$h_{\text{eff}} = 0.15 + 0.85 v^{1.5}$；
   - 琴槌与琴弦的有效接触时间 $T_c$ 随力度增大而连续缩短：
     $$T_c(v) = T_{c,\text{base}} \cdot \left(0.70 + 0.30 (1.0 - v)^{1.2}\right)$$
   - 动态截止频率 $f_c = 2.5 / T_c(v)$ 与速度相关滚降指数 $p = 2.0 - 0.8 h_{\text{eff}}$（弱奏 $pp$ 时高频快速衰减呈温润暗色，强奏 $ff$ 时高频充分释放清脆明亮）。

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

5. **琴槌毛毡微老化物理动力学（Felt Ageing Dynamics，Phase 32-C）**：
   - 引入连续老化参数 $A_{\text{felt}} \in [0, 1]$（`feltAgeingAmount`，默认 0.0 保留纯净基准）；
   - 模拟反复击弦导致的局部羊毛纤维压实与硬化微尖锐：有效毛毡硬度增加 $h_{\text{eff}} \mathrel{+}= 0.18 A_{\text{felt}}$，接触时间动态缩短 $T_c \mathrel{*}= (1.0 - 0.15 A_{\text{felt}})$，高频截止点 $f_c$ 与滚降斜率提升，赋予击键更具穿透力的微观硬化质感。

---

### 2.2 琴弦与动力学系统（String & Dynamics System）

1. **JOS PASP 刚性琴弦失谐（Stiffness Inharmonicity）**：
   - 遵循 Julius O. Smith (JOS) PASP 弹性模量刚性公式：
     $$f_m = m \cdot f_0 \cdot \sqrt{1 + B \cdot m^2}$$
   - $B$ 为琴弦刚度系数，由 Steinway B 88 键实测标定连续曲线插值提供（包含 G2/G#2 琴桥交界突变）。

2. **Magic Circle 二阶递归正弦振荡器（Coupled Form）**：
   - 彻底消灭实时音频线程的 `std::sin` 调用，采用工控与专业 DSP 领域的耦合形式正弦振荡器：
     $$u[n] = u[n-1] - \epsilon \cdot v[n-1]$$
     $$v[n] = v[n-1] + \epsilon \cdot u[n]$$
   - 递归步长在按键瞬间预计算：$\epsilon = 2 \cdot \sin\left(\frac{\pi f_m}{f_s}\right)$；逐采样仅需 **2 次乘法 + 2 次加法**，幅度严格有界、零漂移。

3. **STFT 最优实测微初相矩阵（Micro-Phase Dispersion Table）**：
   - 消除 $t=0$ 所有分音同相机械聚焦造成的狄拉克脉冲式波峰；
   - 内嵌由 PyTorch STFT Loss 训练优化出的 $3 \times 64$ 实测最优初始相位矩阵（`kOptPhaseTable`），重现真实敲击的相位色散。

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

8. **泛音刚度不谐和度抖动（Inharmonicity Jitter，Phase 32-C）**：
   - 采用确定性逐键整数哈希，为 88 键分别注入 $\pm 4.5\%$ 的微观不谐和度刚度扰动 $\Delta B$ 与 $\pm 0.3\sim 1.2\text{ cents}$ 基频分散微调；
   - 真实再现手工缠弦厚度微偏差与挂弦张力离散度，彻底打破纯算法生成的过分对称与人工冰冷感。

---

### 2.3 琴桥与共鸣系统（Bridge, Soundboard & Resonance System）

1. **长短琴桥断裂交界音色补偿（Bridge Break Voicing Jump）**：
   - 针对 MIDI 43～44（G2/G#2）在低音长琴桥与中高音主琴桥断裂交界处的物理突变，针对性校准弦长、刚度与阻尼阶跃，消除过渡区的不自然突兀感。

2. **16 峰正交云杉木物理音板模态组（Bank 2010 / Chabassier 2019）**：
   - 挂载 16 组精确调谐的二阶带通共振滤波器，全频段覆盖云杉木音板核心模态：
     - `48 Hz / 75 Hz`：音板与背架底箱主呼吸模态；
     - `110 Hz / 130 Hz`：低音长琴桥弯曲模态；
     - `180 Hz / 210 Hz`：音板主板面弯曲与对角线模态；
     - `290 Hz / 360 Hz / 450 Hz`：肋木与琴桥交叉耦合模态；
     - `580 Hz / 720 Hz / 890 Hz`：中高频木质辐射模态；
     - `1120 Hz / 1450 Hz / 1850 Hz / 2250 Hz`：各向异性高频散射模态。

3. **云杉木 4.2kHz 高频粘滞内耗低通滤波器（Spruce Soundboard Filter）**：
   - 模拟天然云杉木纤维对超高频能量的各向异性粘滞吸收，消除电子合成器常见的铁皮金属盒共鸣毛刺，赋予音色深厚温暖的木质感。

4. **琴桥立体声空间辐射与声像几何投影**：
   - 依据 88 键在长短琴桥上的物理跨度（低音偏左、高音偏右），结合音板散射矩阵计算立体声投影，消除单声道耳膜居中压迫感。

---

### 2.4 空间、机械与环境拟真系统（Cabinet, Air & Mechanical System）
1. **CC64 延音踏板全局交感共鸣弦池与扫掠声（Sympathetic Resonance & Pedal Noise）**：
   - 踩下 CC64 延音踏板时激活 12 半音全开放交感共鸣弦池，使演奏音符的泛音激发全琴未制音琴弦的共振；
   - **踏板机械扫掠声与共鸣冲击（Phase 32-A）**：踩下/抬起踏板时激发成对的机械毛毡抬起刮擦与空气呼啸脉冲（`pedalWhoosh`，带通 $450\text{ Hz}$）以及对全体开放琴弦的瞬态弱冲击激发（`pedalResonanceShock`，低通 $280\text{ Hz}$），由 `pedalNoiseLevel`（默认 0.6）线性缩放；
   - 支持**未踩踏板时的单键开放弦交感（Duplex & Unpedaled Resonance）**：按住低音键弹奏高音时，低音键对应的开放琴弦产生物理交感振动。

2. **琴盖开合度声学传递函数（Lid Position Acoustics）**：
   - 支持 3 种琴盖物理状态：全开（Full Open）、半开（Half Stick）、闭盖（Closed Lid）。

3. **制音器落木闷击与琴键释放机械瞬态（Damper Release & Key Thump，Phase 32-B）**：
   - 琴键释放时，制音器机械臂带动羊毛毡压回琴弦：
     - 快离键（$v_{\text{rel}} > 0.6$）：激发显著的木质制音头撞击闷响（Wood Thump）并伴随陡峭的高频能量消散；
     - 慢离键（$v_{\text{rel}} \le 0.6$）：延长羊毛毡与琴弦微弱摩擦接触时间，保留轻柔舒缓的余音渐退；
     - 高音无制音器区（MIDI > 88）：跳过毛毡吸振阻尼，但忠实保留按键弹回木质撞击声；
     - **动态 ADSR 离键阻尼缩放**：根据离键速度动态调节释放时间常数 $\tau_{\text{rel}} = \tau_{\text{base}} \cdot (1.5 - 0.75 v_{\text{rel}})$，且基准配置参数跨生命周期严格保持不被永久覆盖。

4. **立体声空间视角成像（PerspectiveProcessor，Phase 31-A）**：
   - 提供**演奏者视角（Player）**与**听众视角（Audience）**双重声学渲染：
     - 演奏者视角：88 键琴桥展开，低音居左、高音居右，近场宽广；
     - 听众视角：声像左右镜像翻转，并施加适度的高频空气吸收与中置凝聚感。

5. **轻量数学算法房间混响网络（RoomReverbEngine，Phase 31-B）**：
   - 内置纯数学算法立体声混响网络，基于 4 组互质延时反馈梳状滤波阵列与 2 级全通扩散矩阵（Schroeder-Moorer 架构演进），零外部采样依赖；
   - 提供 **Studio（录音棚 0.6s）**、**Chamber（室内乐厅 1.5s）** 与 **Concert Hall（音乐厅 2.4s）** 三大经典声学空间预设，支持平滑干湿比（`reverbWet`）无级调节。

6. **动态声场空间漫射（Dynamic Spatial Diffusion）**：
   - 空间声相展开度（Stereo Spread）随时间连续演化：$t=0$ 起振瞬间聚焦于琴桥敲击点（点声源），并在 $25\text{ ms}$ 内经由音板共振与空气反射平滑漫射为整个琴腔的面声源包围场。

---

## 3. 88 键物理参数化表（`Piano88KeyTable.h`）

`PianoSynthVoice` 摒弃粗糙的 4 音区阶跃划分，全面引入 **88 键连续物理参数插值模型**（基于 Bensa et al. 2003 与 Steinway B 9 尺大三角实测数据）：

| 参数项 | 符号 | 取值范围 (A0 → C8) | 物理意义与声学作用 |
|---|---|:---:|---|
| **激活分音数** | `partialCount` | 20 → 6 | 随音高上升动态剪枝，平衡高频解析力与计算开销 |
| **琴弦配置** | `stringCount` | 1 弦 (21~35) / 2 弦 (36~47) / 3 弦 (48~108) | 物理单弦、双弦、三弦真实分区 |
| **琴桥归属** | `isBassBridge` | 低音桥 (21~43) / 主琴桥 (44~108) | 决定琴桥耦合模态与空间声像几何锚点 |
| **有效弦长** | `stringLength` | 1.92 m → 0.09 m | 决定基频与纵波先导声时差 |
| **刚度失谐系数** | `inharmonicityB`| $4.5 \times 10^{-4} \to 1.2 \times 10^{-5}$ | 控制泛音非谐波性金属质感（含 G2/G#2 阶跃） |
| **击弦比** | `strikePosRatio` | $1/8 (0.125) \to 1/16 (0.0625)$ | 决定几何梳状陷波抑制点 |
| **接触时间** | `tcBase` | 3.0 ms → 0.6 ms | 控制琴槌冲击持续时间与动态截止点 |
| **同音微失谐** | `detuneCents` | 0.0 → 0.45 cents | 控制同音三弦拍频干涉周期（1.5s～4s） |
| **基础慢衰减** | `decaySeconds` | 4.8 s → 0.8 s | 决定琴弦慢分量自然延音长度 |
| **快衰减比率** | `fastDecayRatio` | $0.12 \to 0.18$ | 琴弦早期辐射衰减速度与慢衰减之比 |

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

### 4.2 古典微调律制与基准音高（`TemperamentEngine`）

```cpp
void setTemperament(devpiano::audio::Temperament temperament);
void setReferencePitchA4(double hz);
```

支持 6 大古典律制（`equal` 平均律、`meantone` 1/4 中庸全音律、`werckmeister3` 韦克迈斯特三律、`kirnberger3` 基恩伯格三律、`just` 纯律）与基准音高微调（A4 = 415.3Hz, 432Hz, 440Hz, 442Hz 等）。

### 4.3 空间视角、琴盖与环境混响

```cpp
void setLidPosition(LidPosition position) noexcept;
void setPerspective(devpiano::audio::SoundPerspective perspective) noexcept;
```

- **琴盖状态（`LidPosition`）**：`fullOpen`（全开）、`halfStick`（半开）、`closed`（闭盖）；
- **声学视角（`SoundPerspective`）**：`player`（演奏者视角）、`audience`（听众视角）；
- **环境混响（`RoomReverbEngine`）**：支持 `studio`、`chamber`、`concertHall` 与 `reverbWet`（0..1）电平。

### 4.4 Velocity 动态双映射与 ADSR 门控

- **响度响应**：$v^{1.5} = v \cdot \sqrt{v}$ 幂次曲线，强化弱奏（$pp$）细腻度；
- **音色动态**：力度直接耦合琴槌接触时间 $T_c(v)$、高频裂音 $v^2$、泛音绽放速率与非线性微音高漂移；
- **ADSR 门控与基准保持**：支持 `setAdsrParameters` / `getAdsrParameters`；`startNote` 自动重设基准起音与门控，`stopNote` 根据离键速度动态计算释放阻尼，杜绝参数跨音符泄漏。

---

## 5. 性能特征与无锁并发保障

1. **零三角函数计算**：88 键全部激活振荡器均运行在 Magic Circle 状态机，逐采样纯乘加运算；
2. **硬实时音频安全**：
   - 实时音频回调线程（`renderNextBlock`）**零堆内存分配（No `malloc`/`new`）**；
   - **零锁（No Mutex/Lock）**，多线程参数传递采用 `std::atomic` 或原子快照；
   - 遇到极端异常输入时具备自适应静音重置与数值防爆（NaN/Inf 保护）。
3. **单核 CPU 消耗**：在 44.1 kHz / 48 kHz 采样率、8 复音齐奏（每音 20 分音 × 3 琴弦 + 16 模态音板）下，现代 x86_64 CPU 单核负载稳定在 **$\le 0.7\%$**。

---

## 6. 专项确定性测试套件与声学验证

确定性物理单元测试覆盖核心物理声学套件，全部满分通过：

| 测试套件 / 物理用例 | 验证物理机理与断言指标 | 状态 |
|---|---|:---:|
| **PianoSynthVoiceTest** | 88键参数连续性、刚度公式、Magic Circle稳定性、双阶段衰减、同音三弦拍频、纵波先驱声、空间漫射、无内存分配 | [x] 已通过 |
| **DamperReleaseTest** | 快离键木质落弦闷击、慢离键羊毛毡摩擦延展、高音无制音区物理旁路、动态ADSR释放速度缩放、ADSR基准跨音符无泄漏 | [x] 已通过 |
| **PedalAcousticsTest** | CC64 延音踏板全开放交感共鸣、踏板下踏扫掠呼啸（Whoosh）、全琴谐振冲击（Resonance Shock）、单声道与多通道能量守恒 | [x] 已通过 |
| **FeltAgeingTest** | 毛毡老化硬度单调递增、接触时间单调收缩、高频谐波截止点提升、确定性逐键老化扰动、基线 0.0 无偏置 | [x] 已通过 |
| **PerspectiveProcessorTest** | 演奏者/听众立体声像反转镜像、距离高频滚降、单声道能量守恒与无下溢数值收敛 | [x] 已通过 |
| **RoomReverbEngineTest** | Studio/Chamber/Hall 三大空间混响时间常数、干湿比线性与非线性过渡、长时静音衰减无下溢 denormal、跨采样率不变性 | [x] 已通过 |
| **SpatialAcousticsTest** | 空间声学全链路联动、琴盖开合/混响/视角多重组合声学衰减单调性 | [x] 已通过 |
| **PianoSynthVoiceTemperamentTest** | 古典微调律制微音分偏移、A4 基频换算、全音域单调性与跨律制即时切换 | [x] 已通过 |
| **MechanicalAcousticsTest** | 机械噪声与毛毡老化设置存取、边界钳制、预设向后兼容、离线导出参数传递与极端参数安全限幅 | [x] 已通过 |

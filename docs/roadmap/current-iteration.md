# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 18：88 键物理参数化与微观相位色散（Per-Note Voicing & Micro-Phase Dispersion） [推进中]**

本轮重点（深度吸收 `danielpodrazka/piano` 实测物理模型）：
1. **88 键连续物理参数映射（Bensa & Steinway B 实测标定）**：
   - 彻底消灭 4 大离散音区台阶，为 MIDI 21~108 的每一个按键建立基于实测物理数据的连续参数表；
   - 弦长 $L$（$1.92\text{m} \to 0.09\text{m}$）、基础阻尼 $b_1$（$0.25 \to 9.17\text{ s}^{-1}$）、内部摩擦损耗 $b_2$（$7.5\times 10^{-5} \to 2.1\times 10^{-3}\text{ s}$）；
   - 刚度失谐系数 $B$ 采用 Steinway B 实测 7 点对数插值，真实反映缠弦区低音极小值优化；
   - 严格遵循真实琴弦配置：低音单弦（Monochord）、中低音双弦（Bichord）、中高音三弦（Trichord）。
2. **实测最优微相位表内联（3 组同音弦 $\times$ 64 分音 STFT 优化矩阵）**：
   - 直接采纳 PyTorch STFT Loss 反向传播迭代收敛的 $3 \times 64$ 实测最优微相位矩阵（`kOptPhaseTable`）；
   - 消除 $t=0$ 正弦波机械同相叠加造成的狄拉克脉冲式波峰，使瞬态时域波形高度拟真。
3. **空气黏性阻尼与二次方内部损耗（Desvages & Bilbao 2016）**：
   - 引入空气阻尼项 $\sqrt{f_0 / f_n}$ 与内部摩擦 $(n\pi/L)^2$，形成中低频泛音（h2~h5）衰减比基频慢的“中频下凹”特征，呈现大三角钢琴的温暖“歌唱性（Singing Tone）”；
   - 超高频分音自然快速耗散，消灭金属杂音毛刺。
4. **琴槌弹性半余弦调制与 1.8kHz 琴桥共振峰（Bridge Hill）**：
   - 动态接触时间 $T_c$ 结合弹性半余弦调制因子，展现真实羊毛毡多层硬化挤压；
   - 引入 $1.8\text{ kHz}$（$Q\approx 2.25$）宽频琴桥增益峰，提升中高音区穿透力与华丽空间感。
5. **极简 C++20 性能纪律**：
   - 全部 88 键参数与相位矩阵以 `constexpr std::array` 静态编译期内联；
   - 运行时零堆分配、零三角函数运算、维持单核 CPU $\le 0.7\%$。

---

## Phase 18：88 键物理参数化与微观相位色散 [规划就绪]

### 背景与声学机理

在 Phase 17 完成后，内置音源已具备清脆逼真的琴槌击打起音与非线性谱形。然而，对照开源顶级物理建模成果 `danielpodrazka/piano` 与声学论文（Bensa et al. 2003, Bank 2010, Desvages & Bilbao 2016），仍有若干微观物理机制亟待补齐：

1. **4 音区粗粒度台阶**：离散音区切换导致相邻键物理参数突变，缺乏大三角钢琴 88 键平滑连贯的琴桥过渡感；
2. **$t=0$ 绝对零相位相干**：振荡器初值同相（$\cosState=1, \sinState=0$）导致能量瞬间相干聚焦，听感偏向“电子合成器”；
3. **缺少空气阻尼造成的“泛音歌唱性”**：真实钢琴琴弦在空气中振动时，黏性阻力与 $\sqrt{f}$ 相关，使得低音区的低次泛音衰减寿命长于基频，构成丰满温润的音色主体；
4. **高音区能量偏薄**：缺少音板向琴桥传递的 $1.8\text{ kHz}$ 宽频琴桥峰（Bridge Hill）。

---

### 系统架构与核心公式

```
                                [MIDI Note On: note 21~108, velocity]
                                                  │
                                                  ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│ 1. 88 键静态物理参数与琴弦配置查表 (Piano88KeyTable, constexpr)                             │
│    - 弦长 L, 阻尼 b1/b2, 刚度 B(7点对数插值), 击弦比 d/L, 基础接触时间 TcBase               │
│    - 弦数配置: [21-35: 1弦 Mono] | [36-47: 2弦 Bi, ±dc] | [48-108: 3弦 Tri, -dc/0/+dc]     │
└─────────────────────────────────────────────────┬───────────────────────────────────────────┘
                                                  │
                                                  ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│ 2. 模态能量与初始微相位设定 (Modal Amplitude & STFT-Optimized Phase)                        │
│    - 初始振荡器状态: cosState = cos(φ_opt[s][m]), sinState = sin(φ_opt[s][m]) (3x64矩阵)     │
│    - 琴槌滤波: 幂律滚降 + 弹性半余弦调制 M(f) = 0.7 + 0.3·min(1.0, |cos(πfTc)/(1-4f²Tc²)|)   │
│    - 琴桥共振峰注入: Bridge Hill (1.8kHz, BW 800Hz, +40% gain)                              │
└─────────────────────────────────────────────────┬───────────────────────────────────────────┘
                                                  │
                                                  ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│ 3. 空气黏性与二次方摩擦阻尼耗散 (Air Viscosity & Quadratic Loss Model)                      │
│    - α_n = b1·(1 - airFrac) + b1·airFrac·√(f0/fn) + b2·(nπ/L)²                               │
│    - 模态双阶段衰减: τ_prompt = 1 / (α_n · promptFactor · √sb), τ_after = 1 / (α_n · afterFactor)│
└─────────────────────────────────────────────────┬───────────────────────────────────────────┘
                                                  │
                                                  ▼
                                         [Magic Circle Loop]
```

---

### 数据结构与参数表定义

#### 1. 88 键物理参数结构体（`PianoNoteParams`）

```cpp
namespace devpiano::audio {

struct PianoNoteParams {
    int partialCount;           // 激活分音数 (低音 20 -> 高音 6)
    int stringCount;            // 琴弦数: 1 (MIDI 21-35), 2 (MIDI 36-47), 3 (MIDI 48-108)
    float stringLength;         // 振动弦长 L (米, 1.92m -> 0.09m)
    float b1;                   // 频率无关阻尼常数 (0.25 -> 9.17 s^-1)
    float b2;                   // 内部摩擦高阶损耗 (7.5e-5 -> 2.1e-3 s)
    double inharmonicityB;      // Steinway B 实测失谐系数 (3.1e-4 -> 4.0e-2)
    float strikePosRatio;       // 击弦比 d/L (0.125 -> 0.0625)
    float tcBase;               // 基础接触时间 (3.0ms -> 0.6ms)
    float detuneCents;          // 同音弦微失谐量 (0.0 -> 0.4 cents)
    float promptFactor;         // 快衰减倍率因子 (1.2 -> 1.5)
    float aftersoundFraction;   // 慢衰减初始能量占比 (0.18 -> 0.25)
};

} // namespace devpiano::audio
```

#### 2. 实测优化微相位表（`kOptPhaseTable` 3 弦 $\times$ 64 分音）

采用 PyTorch STFT Loss 训练优化生成的 $3 \times 64$ 弧度表，各弦各分音拥有精准的初始空间投影角度，消灭同相波峰。

---

### 子任务排期（Phase 18）

- [x] **Phase 18-A：88 键物理参数表构建与弦数分区（Mono/Bi/Trichord）**
  - 在 `source/Audio/` 下新增 `Piano88KeyTable.h`，实现 Bensa/Steinway 连续插值；
  - 接入单弦、双弦、三弦结构与同音微失谐；
- [x] **Phase 18-B：实测最优微相位表接入与 Magic Circle 状态初始化**
  - 内联 $3 \times 64$ 实测相位矩阵；
  - 在 `startNote` 中初始化各分音的 `cosState` 与 `sinState`；
- [ ] **Phase 18-C：空气黏性阻尼与 1.8kHz Bridge Hill 琴桥峰**
  - 落地 $\alpha_n$ 空气阻尼中频下凹公式，塑造歌唱性尾音；
  - 注入 1.8kHz 琴桥宽频共鸣峰与弹性半余弦琴槌调制；
- [ ] **Phase 18-D：确定性物理测试更新与三闸门交付**
  - 补充 88 键参数连续性测试、相位色散能量守恒测试与 MSVC 编译验证。

---

## Phase 19：立体声音板共鸣箱与弦槌微动力学（远期规划）

### 目标与核心技术

1. **16 峰物理音板模态组（Spruce Soundboard Resonance）**：从 8 峰扩充至 16 峰，覆盖高密度木质共振；
2. **真立体声空间辐射（Stereo Bridge Radiation Pan）**：根据琴弦在琴桥上的物理跨度分配左右声道能量，重现大三角钢琴琴盖开合下的宏大立体声包围感；
3. **同音三弦独立微动力学**：中高音区 3 根琴弦独立微失谐、微相位与空间扩散，模拟真实调律师的“合唱拍频”。

---

## 历史实现 Backlog

- Phase 17 完成记录（真实物理打击感钢琴音源重构）：[`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)
- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

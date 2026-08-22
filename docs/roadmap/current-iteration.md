# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 18：88 键物理参数化与微观相位色散（Per-Note Voicing & Micro-Phase Dispersion） [规划就绪，待执行]**

本轮重点：
1. **88 键静态物理映射（88-Key Per-Note Constexpr Table）**：彻底消灭 4 大音区台阶，为 MIDI 21~108 的每一个按键建立连续的刚度系数 $B(n)$、击弦比 $d/L(n)$、基准衰减 $\tau_{\text{base}}(n)$ 与同音弦配置；
2. **模态初始微相位表（Micro-Phase Table）**：为各分音引入基于物理冲击空间积分的初始微相位偏移，消灭正弦波在 $t=0$ 严格同相叠加产生的“数学/电子合成器”僵硬感；
3. **二次高阶模态阻尼曲率（Quadratic Modal Damping）**：阻尼模型升级为 $\tau_m = \tau_{\text{base}} / (1 + c_1 m + c_2 m^2)$，使极高频金属毛刺在 0.1s 内迅速衰退，留下大三角钢琴厚重纯净的歌唱性尾音；
4. **零堆分配与极低开销保证**：全部参数表以 `constexpr / constinit` 静态编译期内联，音频线程零新增开销，单核 CPU 维持 $\le 0.7\%$。

---

## Phase 18：88 键物理参数化与微观相位色散 [规划就绪]

### 背景与声学机理

在 Phase 17 完成后，内置音源已具备清脆逼真的琴槌击打感与非线性谱形。然而，对照业界顶级物理建模模型（如 `danielpodrazka/piano`、Pianoteq、Balázs Bank 2010/2019），当前模型与真实钢琴之间仍存在微观层面的“机械规则感”：

1. **4 音区粗粒度台阶**：当前采用 4 个离散音区（$<48, <72, <96, \ge 96$），导致相邻音符跨音区时物理参数（刚度 $B$、击弦比 $d/L$）存在微小跳跃；
2. **$t=0$ 绝对零相位相干**：所有模态振荡器初值均为 $u[0]=1, v[0]=0$（$\varphi_m = 0$）。真实钢琴琴槌毛毡具有宽度，撞击弦段时各模态在空间上的投影会形成天然的相位色散（Phase Dispersion）；
3. **高频阻尼线性化**：当前的线性阻尼使 15 次以上超高频分音残留略长，需要二次方高阶损耗曲率快速收敛。

---

### 设计方案与参数表架构

#### 1. 88 键静态参数表结构（`PianoNoteConfig`）

在编译期为 MIDI 21（A0）到 108（C8）共 88 个音符预计算物理常量表：

```cpp
struct PianoNoteConfig {
    int partialCount;             // 激活分音数 (低音 20 -> 高音 6)
    float baseDecaySeconds;       // 慢衰减基准时间常数 τ_slow (低音 5.0s -> 高音 0.7s)
    double inharmonicityB;        // 刚性弦失谐系数 B (低音 4.5e-4 -> 高音 8.0e-6)
    float dampingC1;              // 线性阻尼系数 c1 (0.20 ~ 0.35)
    float dampingC2;              // 二次高阶损耗系数 c2 (0.015 ~ 0.040)
    float fastDecayRatio;         // 快衰减时间比 τ_fast / τ_slow (0.10 ~ 0.16)
    float slowWeight;             // 慢分量初始权重 (0.12 ~ 0.22)
    float strikingPositionRatio;  // 击弦比 d/L (低音 0.125 -> 高音 0.0714)
    float beatingDetuneRatio;     // 同音微失谐比 (0.0008 ~ 0.0022)
    int beatingPartials;          // 拍频分音数 (低音 6 -> 高音 0)
};

// 88 键连续平滑映射表 (constexpr 零运行时开销)
static constexpr std::array<PianoNoteConfig, 88> k88NoteConfigs = ...;
```

#### 2. 微观初始相位色散表（`MicroPhaseTable`）

对第 $m$ 个分音引入空间激发微相位 $\varphi_m$：
- 避免所有正弦波在 $t=0$ 产生完全相干的狄拉克脉冲式波峰；
- 在 `startNote` 初始化振荡器状态：
  $$u[0] = \cos(\varphi_m), \quad v[0] = \sin(\varphi_m)$$
- 步进更新保持 Magic Circle coupled form，音频渲染循环完全无需计算 $\sin / \cos$。

#### 3. 二次方模态能量耗散模型（Quadratic Loss Curve）

升级时间常数公式：
$$\tau_m = \frac{\tau_{\text{base}}}{1.0 + c_1 \cdot (m - 1) + c_2 \cdot (m - 1)^2}$$
- 低次主谐波（$m \le 6$）由 $c_1$ 决定平缓衰减；
- 高次金属泛音（$m \ge 8$）由 $c_2 \cdot m^2$ 剧烈抑制，在 $0.05 \sim 0.15\text{ s}$ 内自然归于沉寂，展现纯净歌唱性。

---

### 子任务排期（Phase 18）

- [ ] **Phase 18-A：88 键静态参数表构建与连续化迁移**
  - 构建 `PianoNoteConfig` 88 键插值生成器，替换 4 大离散音区；
  - 接入 `startNote` 与查询接口，测试全键域物理参数平滑单调性；
- [ ] **Phase 18-B：微观初始相位色散表（Phase Table）引入**
  - 设计 88 键模态初始相位查找表，消除 $t=0$ 机械零相干；
  - 改造 Magic Circle 初始状态设定（`cosState = cos(φ)`, `sinState = sin(φ)`）；
- [ ] **Phase 18-C：高阶模态二次阻尼曲率调优**
  - 引入 $c_2 \cdot m^2$ 二次损耗项，消灭极高频金属毛刺长尾；
  - 协调 3 个 DevKnob 旋钮在 88 键上的精细映射；
- [ ] **Phase 18-D：确定性物理测试与基线验证**
  - 88 键全范围 DFT 与相干性测试，三闸门基线全绿交付。

---

## Phase 19：立体声音板共鸣箱与弦槌微动力学（远期规划）

### 目标与核心技术

1. **16 峰物理音板模态组（Spruce Soundboard Resonance）**：从 8 峰扩充至 16 峰，覆盖高密度中高频共鸣；
2. **真立体声空间辐射（Stereo Bridge Radiation Pan）**：根据音高与琴弦在琴桥上的物理跨度分配左右声道能量，重现大三角钢琴琴盖开合下的宏大立体声包围感；
3. **同音三弦独立微动力学**：中高音区 3 根琴弦独立微失谐、微相位与空间扩散，模拟真实调律师的“合唱拍频”。

---

## 历史实现 Backlog

- Phase 17 完成记录（真实物理打击感钢琴音源重构）：[`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)
- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

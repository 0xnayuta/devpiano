# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 17：真实物理打击感钢琴音源重构（Physical Strike & Non-linear Hammer Piano Synthesis） [规划就绪，待执行]**

本轮重点：
1. **消灭 $1/n$ 锯齿波拉弦感**：引入击弦位置梳状滤波（$d/L \approx 1/8 \sim 1/14$）与非线性琴槌毛毡硬化截止谱，彻底打破弦乐提琴谐波特征；
2. **重塑真实打击物理起音**：消除 10ms 慢起音门控（Attack $\le 0.2\text{ ms}$ 极速起振），注入 $2\sim 3\text{ ms}$ 毛毡撞击物理瞬态核（Hammer Strike Click），提供真实的敲击清脆质感；
3. **强化双阶段衰减落差与音板共鸣**：提升早期快衰减权重，增强三角钢琴长音的木质共鸣箱厚度；
4. **确定性测试与物理参数调优**：补齐击弦点陷波断言、起音冲击能量断言，通过三闸门基线验证。

---

## Phase 17：真实物理打击感钢琴音源重构 [规划就绪]

### 背景与目标

当前内置物理建模钢琴（`PianoSynthVoice`）在长尾衰减期具备良好的刚性失谐（Inharmonicity）、双阶段衰减（Two-stage decay）与同音弦拍频（Beating），但听觉特征偏向**提琴/拉弦乐器（Bowed String）**而非**钢琴/击弦打击乐器（Struck String）**。

#### 核心声学根因：
1. **谐波谱为标准锯齿波**：分音幅度简单按 $1/n$ 递减（$-6\text{ dB/octave}$），在声学物理上恰为小提琴亥姆霍兹擦弦运动的特征波形；
2. **起音存在拉弓渐入感**：默认 ADSR 门控带有 $10\text{ ms}$ Attack 渐入，抹平了琴槌击弦的瞬间爆发力；
3. **缺失琴槌物理敲击瞬态（Hammer Strike Transient）**：正弦振荡器纯音起振，缺少毛毡撞击钢弦与铸铁琴桥的初始冲击特征（Click / Thump）；
4. **缺少击弦点梳状滤波（Comb Filter）**：缺少琴槌敲击点位置对特定次谐波（如第 7/8 次）的天然物理陷波抑制。

#### 重构目标：
对标业界顶级物理建模钢琴（Pianoteq）与 Balázs Bank（2010 IEEE TASLP）模态钢琴理论，在保持纯 C++ 解析生成、零采样依赖、零堆分配及单核 CPU $\le 0.7\%$ 的前提下，彻底重构击弦起振物理，将音色蜕变为具有清脆木质敲击感与丰富共鸣的真实钢琴。

---

### 系统架构（物理子系统划分）

```
                                  [MIDI Note On / Velocity]
                                              │
                                              ▼
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│ 1. 击弦动力学与瞬态冲击核 (Hammer-String Dynamics & Strike Transient)                     │
│    ┌───────────────────────────────────┐     ┌──────────────────────────────────────────┐ │
│    │ 非线性琴槌击弦谱形生成器          │     │ 琴槌-琴弦物理碰撞瞬态核 (Hammer Click)   │ │
│    │ - 击弦位置 comb filter (d/L)      │     │ - 2~3ms 宽频带通冲击脉冲                 │ │
│    │ - 硬化弹簧谱形截止 (Harden Spring)│     │ - 力度敏感的毛毡/木质敲击瞬态能量        │ │
│    │ - 力度非线性泛音扩展 (Power Law)  │     │ - 与弦振动在 t=0 零延迟物理对齐          │ │
│    └─────────────────┬─────────────────┘     └───────────────────┬──────────────────────┘ │
└──────────────────────┼───────────────────────────────────────────┼────────────────────────┘
                       │ 驱动分音初始能量                          │ 瞬态直接注入
                       ▼                                           │
┌──────────────────────────────────────────────────────────────────┴────────────────────────┐
│ 2. 增强模态琴弦振荡网络 (Enhanced Modal Resonator Network)                                │
│    - 零渐入门控 (Instantaneous Attack Gate, attack <= 0.2ms，消灭拉弓感)                  │
│    - 刚性失谐频率 fm = m·f0·√(1 + B·m²) 与同音弦微失谐干涉 (Unison Beating)               │
│    - 模态双阶段能量耗散 (τ_fast 击弦辐射期 + τ_slow 长尾余韵期)                           │
│    - 各分音初始相位物理约束 (零点相位微扰，避免周期性锯齿波波形畸变)                      │
└──────────────────────────────────────┬────────────────────────────────────────────────────┘
                                       │ 琴弦振动速度信号 v_string(t)
                                       ▼
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│ 3. 弦桥传递与音板/琴体共振网络 (Soundboard Radiation & Body Model)                       │
│    - 弦桥低通/带通耦合滤波 (Bridge Impedance Filtering)                                   │
│    - 8 峰物理音板模态 (Soundboard Modal Bank, 75Hz~950Hz)                                 │
│    - 拟真木质箱体 Wet/Dry 混合动态响应                                                    │
└──────────────────────────────────────┬────────────────────────────────────────────────────┘
                                       │
                                       ▼
                              [Audio Output Stream]
```

---

### 子任务排期

**Phase 17-A：击弦点梳状滤波与非线性琴槌频谱重构**
- [ ] 引入音区击弦位置比 $d/L$ 查表：低音 $1/8 \to$ 中音 $1/7.5 \to$ 高音 $1/12 \sim 1/14$；
- [ ] 实现击弦点梳状滤波调制因子：$S(m) = \left|\sin\left(m \pi \frac{d}{L}\right)\right|$；
- [ ] 重构分音初始幅度公式：引入高次幂律滚降与非线性力度指数截止：
  $$A_m(v) = S(m) \cdot \frac{1}{(m+1)^\alpha} \cdot \exp\left(-\frac{m}{M_0(v)}\right)$$
- [ ] 彻底废除 $1/(n+1)$ 锯齿波线性幅度分布。

**Phase 17-B：琴槌敲击瞬态核（Hammer Strike Transient）与极速起振**
- [ ] 将 `adsrGate` 的起振门控缩短至 $\le 0.2\text{ ms}$（$\le 10$ 个采样点），消除渐入拉弦感；
- [ ] 构建微秒级木槌撞击瞬态生成器（$2\sim 3\text{ ms}$，带通滤波冲击脉冲）；
- [ ] 建立击弦瞬态能量随力度非线性缩放公式（强奏清脆有力，弱奏柔和温润）；
- [ ] 将瞬态冲击与正弦模态网络在 $t=0$ 严格物理对齐。

**Phase 17-C：双阶段能量衰减与音板共鸣增强调优**
- [ ] 提升快衰减（Fast Radiation）权重至 $80\%\sim 85\%$，增强击键后的动态落差与爆发感；
- [ ] 优化音板 8 峰谐振器的 Q 值与动态混合分布，增强木质共鸣箱体感；
- [ ] 协调 3 个 DevKnob 旋钮映射（Brightness 调控击弦截止频点，Hammer Hardness 调控打击瞬态与高频开放度，Resonance 调控音板衰减长尾）。

**Phase 17-D：确定性物理测试补齐、听觉回归与三闸门交付**
- [ ] 在 `PianoSynthVoiceTest.cpp` 中新增击弦点陷波断言、起音瞬态能量断言与力度谱非线性断言；
- [ ] 在 Windows MSVC 环境下进行实机人工 A/B 听觉盲测回归；
- [ ] 运行三闸门基线检查（`wsl-build --configure-only`, `test`, `format --check`, `win-build`）全绿交付。

---

### 验收标准

1. **听觉特征蜕变**：按下琴键瞬间具有清晰逼真的琴槌击弦打击感（Hammer Strike），彻底消除拉弦乐器的渐入感与嗡鸣感；
2. **力度响应逼真**：弱奏（pianissimo）音色圆润温暖、泛音内敛；强奏（fortissimo）清脆明亮、打击感强烈且泛音饱满；
3. **计算效率与纪律**：音频渲染线程维持单核 CPU $\le 0.7\%$，零堆分配、无锁、无三角函数调用；
4. **三闸门基线**：全量单元测试 100% 通过，Windows MSVC 构建成功。

---

## 历史实现 Backlog

- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

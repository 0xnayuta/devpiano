# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 23：大师级音色校准与 Pianoteq 对齐精调（Master Voicing & Physical Realism Calibration） [已完成，2026-08-23]**

在 Phase 17~22 建立了 7 大完整声学系统（Hammer、String、Bridge、Soundboard、Cabinet、Air、Room）后，参考 `../piano` 声学优化实验与 **Modartt Pianoteq** 商用标杆，开展了全面的音色校准与参数精调：

1. **Phase 23-A：动态琴槌非线性刚度与击弦点几何陷波 [已完成]**：
   - 引入三层毛毡动力学压实模型（$h_{\text{eff}} = 0.15 + 0.85 v^{1.5}$），接触时间 $T_c$ 随力度与音区连续缩短；
   - 动态截止频率 $f_c = 2.5 / T_c$ 与速度相关滚降指数 $p = 2.0 - 0.8 h_{\text{eff}}$（$pp$ 柔和醇厚，$ff$ 清脆明亮）；
   - 精确计算击弦点几何梳状陷波 $\sin(n \pi x_0/L)$，消除第 7~9 阶非协和杂音；
   - 88 键基底幂次滚降（低音 1.25 -> 高音 2.10），让低音深沉、高音纯净。
2. **Phase 23-B：同音三弦立体声非对称微失谐与声相展开 [已完成]**：
   - 基于 Weinreich (1977 JASA) 耦合琴弦理论，引入 Mid-Side 差分多弦立体声展开模型；
   - $s_{\text{sum}} = \frac{1}{3}(s_1 + s_2 + s_3)$ 与 $\Delta s = \frac{1}{3}(s_3 - s_1)$，左右声道反相注入差分拍频分量；
   - 单声道下差分完全抵消还原纯净物理三弦，立体声下呈现开阔的大三角钢琴声场与自然呼吸感。
3. **Phase 23-C：云杉木音板低通截止与木质腔体共鸣峰配平 [已完成]**：
   - 引入 $4.2\text{ kHz}$ 云杉木纤维高频粘滞吸收低通滤波器（`SpruceSoundboardFilter`）；
   - 优化 16 峰物理音板模态分布与权重平衡，彻底消除超高频铁皮盒共振，赋予大三角钢琴温润深厚的木质感。
4. **Phase 23-D：起音瞬态裂音与低音纵波微调 [已完成]**：
   - 在击键最初 $3\text{ ms}$ 注入与力度平方 $v^2$ 耦合的高频瞬态裂音（HF Attack Crack），彻底对齐真实采样钢琴（Salamander C5）的 $27\sim 30\text{ ms}$ 极速起振特性；
   - 微调低音钢弦纵波先驱声权重与衰减常数（$15\text{ ms}$ 紧凑收敛），强化低音区真实钢铁撞击张力。

---

## Phase 23-D：起音瞬态裂音与低音纵波微调 [已完成]

### 子任务排期（Phase 23-D）

- [x] **分析起音瞬态裂音（HF Attack Crack）与纵波先驱声机理**
- [x] **实现 3ms 高频裂音发生器（$v^2$ 与音区加权、零堆分配 LCG 脉冲）**
- [x] **微调低音纵波前 3 阶振幅权重与 $15\text{ ms}$ 快速衰减常数**
- [x] **更新 `PianoSynthVoiceTest.cpp` 确定性断言并通过测试**
- [x] **三闸门全绿验证（format, test, win-build）**

---

## 历史实现 Backlog

- Phase 22 完成记录（物理声学极致深化与机械拟真）：[`../archive/phase22-physical-modeling-acoustic-refinement.md`](../archive/phase22-physical-modeling-acoustic-refinement.md)
- Phase 21 完成记录（踏板交感共鸣与琴盖空间声学）：[`../archive/phase21-sympathetic-resonance-lid-acoustics.md`](../archive/phase21-sympathetic-resonance-lid-acoustics.md)
- Phase 20 完成记录（微观物理动力学：纵向波先驱声与击键混沌微扰）：[`../archive/phase20-longitudinal-ping-micro-variation.md`](../archive/phase20-longitudinal-ping-micro-variation.md)
- Phase 19 完成记录（立体声音板共鸣箱与同音三弦微动力学）：[`../archive/phase19-stereo-modal-soundboard.md`](../archive/phase19-stereo-modal-soundboard.md)
- Phase 18 完成记录（88 键物理参数化与微观相位色散）：[`../archive/phase18-per-note-voicing-micro-phases.md`](../archive/phase18-per-note-voicing-micro-phases.md)
- Phase 17 完成记录（真实物理打击感钢琴音源重构）：[`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)
- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 19：立体声音板共鸣箱与同音三弦微动力学（Stereo Modal Soundboard & Multi-String Dynamics） [已完成，2026-08-22]**

本轮重点（深度吸收 Bank 2010 IEEE TASLP 与 Chabassier 2019 IEEE SPM 音板模态与空间辐射理论）：
1. **16 峰物理云杉木音板模态组（Spruce Soundboard Modal Bank）**：
   - 基于 Bank 2010 (Sec. VII) 与 Chabassier 2019 (Sec. 3.3) 真实三角钢琴云杉木板正交各向异性（Orthotropic）模态数据，将音板谐振器从 8 峰扩充为 **16 峰**；
   - 覆盖 $48\text{ Hz} \sim 2250\text{ Hz}$ 完整物理频段（包含底箱呼吸模态、低音/高音长琴桥耦合模态、肋木加强筋横向模态与高频各向异性散射峰），极点严格在单位圆内，双声道权重严格归一化。
2. **琴桥立体声空间辐射（Stereo Bridge Panning & Asymmetric Soundboard Radiation）**：
   - 模拟大三角钢琴 $2\sim 3\text{ 米}$ 琴身低音在左、高音在右的物理跨度；
   - 根据琴键位置 $x_{\text{key}} \in [0.0, 1.0]$ 计算直达声声像：$\text{pan} = 0.20 + 0.60 \times x_{\text{key}}$（左耳 0.20 到右耳 0.80）；
   - 音板谐振器左右声道非对称空间投影：低频模态偏向左耳箱体，中高频模态偏向右侧开阔琴盖散射，彻底消灭单声道耳膜居中压迫感。
3. **同音三弦独立三振荡器非对称拍频（Trichord 3-Oscillator Asymmetric Beating）**：
   - 中高音区（MIDI 48~108）全面升级为 3 振荡器非对称微失谐拍频（$-\Delta c, 0, +\Delta c$）；
   - 3 根弦分别绑定 STFT 实测最优微初相矩阵 `kOptPhaseTable[0]`, `kOptPhaseTable[1]`, `kOptPhaseTable[2]`，展现真实调律师调出的“合唱拍频（Choral Unison）”。
4. **极简 C++20 性能纪律**：
   - 16 峰谐振器极点递推维持每采样纯加乘法，零堆分配、零锁、音频线程单核 CPU 维持 $\le 0.7\%$。

---

## Phase 19：立体声音板共鸣箱与同音三弦微动力学 [已完成]

### 子任务排期（Phase 19）

- [x] **Phase 19-A：16 峰云杉木物理音板模态重构（Spruce Soundboard Modal Bank）**
  - 基于 Bank 2010 / Chabassier 2019 实测数据构建 16 峰音板模态表（48Hz~2250Hz）；
  - 实现二阶并联滤波与极点稳定保证（$|r| < 1$），权重严格归一化。
- [x] **Phase 19-B：琴桥物理立体声空间辐射与非对称投影**
  - 在 `renderNextBlock` 中实现直达声琴桥声像定位与双声道非对称模态投影；
  - 彻底将单声道输出蜕变为宏大真实的立体声大三角钢琴声场。
- [x] **Phase 19-C：同音三弦独立三振荡器非对称拍频（Trichord 3-Oscillator Engine）**
  - 在 `PianoSynthVoice::Partial` 中扩展第 3 振荡器（`cosState3`, `sinState3`, `epsilon3`）；
  - 接入 `kOptPhaseTable[2]` 独立初相，实现三弦独立非对称拍频。
- [x] **Phase 19-D：确定性物理测试更新与三闸门交付**
  - 补充 16 峰音板参数断言、左右声道立体声分离度测试与三振荡器干涉测试，三闸门基线全绿交付。

---

## 历史实现 Backlog

- Phase 19 完成记录（立体声音板共鸣箱与同音三弦微动力学）：[`../archive/phase19-stereo-modal-soundboard.md`](../archive/phase19-stereo-modal-soundboard.md)
- Phase 18 完成记录（88 键物理参数化与微观相位色散）：[`../archive/phase18-per-note-voicing-micro-phases.md`](../archive/phase18-per-note-voicing-micro-phases.md)
- Phase 17 完成记录（真实物理打击感钢琴音源重构）：[`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)
- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

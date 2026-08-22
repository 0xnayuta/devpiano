# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 20：微观物理动力学（纵向波先驱声与击键混沌微扰） [已完成，2026-08-22]**

本轮重点（深度吸收 Bank 2005 JASA、Bank 2010 IEEE TASLP 与 Bank & Chabassier 2019 IEEE SPM）：
1. **低音钢弦纵向波先驱脉冲（Longitudinal Precursor Ping，Bank 2005/2010）**：
   - 依据钢弦纵波物理波速 $v_L = \sqrt{E/\rho} \approx 5100\text{ m/s}$，为低音区（MIDI 21~52，A0~E3）注入基频为 $f_{L,1} = v_L / (2L)$ 的前 3 阶极快速衰减纵向金属先驱脉冲（$\tau_{\text{long}} \approx 15\sim 25\text{ ms}$）；
   - 完美重现大三角钢琴低音区击键瞬间特有的紧绷金属撞击“哐”先导声。
2. **机械击弦微观混沌微扰（Micro-variation Jitter，Bank & Chabassier 2019 Sec. 4）**：
   - 为连续击打同一琴键（同音快速轮指、快速琶音）引入微秒级物理微扰（Jitter）：
     - 击弦位置微扰：$d/L \times (1 + \delta_1)$（$\delta_1 \in [-0.6\%, +0.6\%]$）；
     - 空间微初相微扰：$\varphi_n + \delta_2$（$\delta_2 \in [-0.012, +0.012]\text{ rad}$）；
     - 琴槌毛毡接触时间微扰：$T_c \times (1 + \delta_3)$（$\delta_3 \in [-0.8\%, +0.8\%]$）；
   - 彻底消灭连续快速击键时的“机械克隆感”，赋予每次发音独一无二的物理呼吸生命力。
3. **极简 C++20 性能纪律**：
   - 纵向波脉冲与打击核紧凑内联，零堆分配、零运行时三角函数计算，音频线程单核 CPU 维持 $\le 0.7\%$。

---

## Phase 20：微观物理动力学（纵向波先驱声与击键混沌微扰） [已完成]

### 子任务排期（Phase 20）

- [x] **Phase 20-A：低音钢弦纵向波先驱脉冲注入（Longitudinal Precursor Ping）**
  - 在 `PianoSynthVoice::HammerTransient` 或独立结构中实现纵向波前 3 阶模态脉冲；
  - 绑定 $v_L \approx 5100\text{ m/s}$ 与 88 键实测弦长 $L$ 计算 $f_{L,1}$，实现 $\le 20\text{ ms}$ 极速衰减。
- [x] **Phase 20-B：机械击弦微观混沌微扰引擎（Micro-variation Jitter）**
  - 在 `PianoSynthVoice::startNote` 中引入轻量伪随机/确定性混沌发生器（基于 note、velocity 与击键计数）；
  - 对 $d/L$、$\varphi$ 与 $T_c$ 施加微扰，赋予同音轮指自然的生命起伏。
- [x] **Phase 20-C：确定性物理测试更新与三闸门交付**
  - 补充低音纵向波频点与衰减测试、连续击键微扰方差测试，三闸门基线全绿交付。

---

## 远期规划 Backlog（Phase 21）

- **Phase 21：踏板交感共鸣与琴盖空间声学（Sympathetic Resonance Pool & Lid Reflection）**：
  1. 延音踏板全局交感共鸣弦池（Sympathetic Resonance Pool）；
  2. 三角钢琴琴盖反射传递函数与木质近场微反射（Lid Position & Early Reflections）。

---

## 历史实现 Backlog

- Phase 20 完成记录（微观物理动力学：纵向波先驱声与击键混沌微扰）：[`../archive/phase20-longitudinal-ping-micro-variation.md`](../archive/phase20-longitudinal-ping-micro-variation.md)
- Phase 19 完成记录（立体声音板共鸣箱与同音三弦微动力学）：[`../archive/phase19-stereo-modal-soundboard.md`](../archive/phase19-stereo-modal-soundboard.md)
- Phase 18 完成记录（88 键物理参数化与微观相位色散）：[`../archive/phase18-per-note-voicing-micro-phases.md`](../archive/phase18-per-note-voicing-micro-phases.md)
- Phase 17 完成记录（真实物理打击感钢琴音源重构）：[`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)
- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

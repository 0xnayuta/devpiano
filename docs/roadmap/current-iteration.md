# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 21：踏板交感共鸣与琴盖空间声学（Sympathetic Resonance Pool & Lid Reflection） [已完成，2026-08-22]**

本轮重点（深度吸收 Bank 2010 Sec. VI 与 Chabassier 2019 Sec. 3.4）：
1. **延音踏板全局交感共鸣弦池（Sympathetic Resonance Pool，Bank 2010 Sec. VI）**：
   - 内置 12 个半音基底谐振器（C2~B2，覆盖全音名）；
   - 响应 MIDI CC 64 延音踏板控制器：踏板踩下（`controllerValue >= 64`）时，琴弦总振动能量以受控弱增益（$0.08$）注入交感共鸣池，松开时快速阻尼收敛。
2. **三角钢琴琴盖反射模型与木质近场微反射（Lid Position & Early Reflections，Chabassier 2019 Sec. 3.4）**：
   - 环形微延迟线（1024 样本零堆分配）；
   - 3 抽头近场微反射（$3.2\text{ ms}, 7.8\text{ ms}, 11.5\text{ ms}$），提供演奏者身处琴凳前的真实物理空间空气深度，消灭干燥贴耳感。
3. **极简 C++20 性能纪律**：
   - 零堆分配、零锁、纯加乘法递推，音频线程单核 CPU 维持 $\le 0.7\%$。

---

## Phase 21：踏板交感共鸣与琴盖空间声学 [已完成]

### 子任务排期（Phase 21）

- [x] **Phase 21-A：延音踏板全局交感共鸣弦池（Sympathetic Resonance Pool）**
  - 接入 12 半音基底谐振器，响应 CC64 控制器；
  - 踏板踩下注入共鸣能量，松开快速阻尼收敛。
- [x] **Phase 21-B：三角钢琴琴盖反射与木质近场微反射（Lid Acoustics & Early Reflections）**
  - 构建 1024 样本零堆分配延迟线与 3 抽头近场微反射（$3.2\text{ms}, 7.8\text{ms}, 11.5\text{ms}$）；
  - 塑造琴盖高频反射与空气空间深度。
- [x] **Phase 21-C：确定性物理测试更新与三闸门交付**
  - 补充 CC64 踏板共鸣能量响应测试与近场反射数值稳定性测试，三闸门基线全绿交付。

---

## 历史实现 Backlog

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

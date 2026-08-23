# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**v1.0.0 发布收官与文档体系全面治理（Post-v1.0.0 Documentation Governance & Architecture Alignment） [进行中，2026-08-23]**

随着 **Phase 24（生命力与非线性动力学绽放）** 的全面落地与 **v1.0.0** 正式发布，devpiano 已完成了从基础键盘演奏器到包含 7 大物理声学系统、JIVE 声明式 UI 与完整 MIDI/VST3 工作流的成熟应用。本轮任务聚焦于消除代码演进与文档体系之间的滞后断层，将物理建模核心技术完整沉淀并确立长期质量验收基线：

1. **核心特性技术参考全面重构 [已完成]**：
   - 全面重构 [`../reference/features/builtin-piano-synthesis.md`](../reference/features/builtin-piano-synthesis.md)，详细阐述 7 大物理声学系统（Hammer、String、Bridge、Soundboard、Cabinet、Air、Room）、88 键连续参数化模型（`Piano88KeyTable.h`）、非线性动力学（Harmonic Blooming、Contact Dynamics、Spatial Diffusion）与 14 项确定性测试套件。
2. **路线图与里程碑对齐 [已完成]**：
   - 更新 [`roadmap.md`](roadmap.md)，标注 Phase 1~24 与各个发布版本（v0.1.0 ~ v1.0.0）的映射关系，规划 v1.0.0 后续维护与演进方向。
3. **阶段验收与架构文档同步 [已完成]**：
   - 补齐 [`../reference/acceptance.md`](../reference/acceptance.md) 中 Phase 16～24 及 v1.0.0 正式发布验收清单；
   - 更新 [`../reference/architecture.md`](../reference/architecture.md) 中 §3.2 Audio 章节，体现 7 大物理声学系统与 88 键参数模型；
   - 对齐 [`../README.md`](../README.md) 文档中心入口与各特性索引。
4. **发布打包流水线脚本化 [已完成]**：
   - 新增 [`../../scripts/package_release.sh`](../../scripts/package_release.sh)，并在 [`../../scripts/dev.sh`](../../scripts/dev.sh) 中接入 `package` 命令，实现版本一致性检查、产物收集、ZIP 压缩与 SHA256 自动化生成；
   - 同步更新 [`../guides/release-workflow.md`](../guides/release-workflow.md)、[`../guides/quickstart.md`](../guides/quickstart.md) 与 `AGENTS.md`。

---

## 本轮子任务排期

- [x] **重构 `docs/reference/features/builtin-piano-synthesis.md` 为 7 大声学系统完整技术参考**
- [x] **更新 `docs/roadmap/current-iteration.md` 归档 Phase 23/24 并切换至文档治理**
- [x] **更新 `docs/roadmap/roadmap.md`（补充版本映射与 v1.0.0 总结）**
- [x] **补齐 `docs/reference/acceptance.md`（Phase 16~24 与 v1.0.0 验收标准）**
- [x] **同步 `docs/reference/architecture.md`（更新 Audio 架构与数据流）**
- [x] **对齐 `docs/README.md` 文档中心入口索引**
- [x] **实现 `./scripts/dev.sh package` 发布打包流水线脚本化并更新相关指南**
- [x] **全量文档链接与格式校验**

---

## 历史实现 Backlog

- Phase 24 完成记录（生命力与非线性动力学绽放）：[`../archive/phase24-vitality-and-dynamic-blooming.md`](../archive/phase24-vitality-and-dynamic-blooming.md)
- Phase 23 完成记录（大师级音色校准与 Pianoteq 对齐精调）：[`../archive/phase23-master-voicing-realism-calibration.md`](../archive/phase23-master-voicing-realism-calibration.md)
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

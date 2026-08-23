# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Post-v1.0.0 质量保障与平台准备：GitHub Actions CI 流水线落地 [进行中，2026-08-23]**

在完成 Phase 25-A（ALSA/JACK 驱动链路与设备管理机制审查）后，鉴于本地暂无 Linux 物理桌面实机环境，Phase 25-B/C/D（Linux 桌面实机交互验证）适度暂缓；本轮优先落地 **GitHub Actions CI 质量门禁流水线（`.github/workflows/ci.yml`）**，建立云端自动化的跨平台质量与回归防线：

1. **代码格式门禁 (`format-gate`)**：在 Ubuntu Runner 上以 `clang-format-21` 自动执行 `./scripts/dev.sh format --check`；
2. **Linux 编译与单测门禁 (`linux-test-gate`)**：在 Ubuntu Runner 上拉取子模块、编译并执行全量 58 个测试套件（`./scripts/dev.sh test`）；
3. **Windows MSVC 构建门禁 (`windows-msvc-gate`)**：在 Windows 原生 Runner 上以 MSVC + Ninja 编译 Debug 目标并执行全量单测，杜绝跨编译器破坏。

---

## 本轮子任务排期

- [x] **Phase 25-A：ALSA / JACK 音频驱动链路与设备管理机制审查**
- [x] **编写并落地 `.github/workflows/ci.yml` 质量门禁流水线**
- [x] **YAML 语法解析与跨平台编译配置静态校验**
- [x] **更新相关开发与构建文档并提交**
- [~] **Phase 25-B~E：Linux 桌面实机与绿色分发（待 Linux 实机具备后推进）**

---

## 后续规划路线（Upcoming Backlog）

- **Phase 26：MIDI 多轨并轨与综合时间线合并（MIDI Multi-Track Timeline Merge）**：
  - 支持多轨标准 MIDI 文件全轨道智能并轨；
  - 16 通道矩阵自动重映射与多通道综合 Take 录制/回放；
  - 88 键虚拟键盘多音轨多着色高亮联动与全轨导出。
- **Phase 27：现实物理演奏交互与声学控制（Physical Voicing & Realistic Acoustic Interaction）**：
  - **琴盖开合度（Lid Position）**：在 UI（Controls/设置面板）提供 Full Open / Half Stick / Closed 3 态直观选择，无缝切换多级高频滚降与近场反射；
  - **弱音/移位踏板（Una Corda / Soft Pedal，CC 67）**：模拟大三角钢琴击弦机右移、3 弦敲 2 弦与毛毡侧面软化物理机理，支持 CC 67 踏板信号与 UI 软踏板状态点亮；
  - **触键力度曲线（Touch Velocity Curve）**：提供 Standard / Light / Heavy / Wide Dynamic 4 种配重手感映射，自适应薄膜/机械轴/MIDI 键盘；
  - **配置持久化与预设系统联动**：将琴盖位置、Una Corda 状态与触键曲线纳入 `SettingsModel` 与 Performance Preset 序列化。

---

## 历史实现 Backlog

- Post-v1.0.0 文档体系治理与打包流水线自动化完成记录：[`../guides/release-workflow.md`](../guides/release-workflow.md)
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

# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 25：Linux 原生桌面构建与音频驱动适配（Linux Desktop & Audio Path Exploration） [进行中，2026-08-23]**

在完成 v1.0.0 正式发布与发布打包流水线自动化后，devpiano 进入 **Post-v1.0.0 平台拓展与高阶能力演进** 阶段。本阶段聚焦于验证并完善 Linux 平台（WSL / 原生 Linux 发行版）下的原生构建、音频后端驱动（ALSA / JACK）适配与桌面运行体验，为 Linux 平台正式分发打下坚实基础：

1. **Phase 25-A：ALSA 与 JACK 音频后端驱动适配与设备枚举**：
   - 验证 JUCE `AudioDeviceManager` 在 Linux 环境下对 ALSA / JACK 驱动的初始化、缓冲区协商与动态设备切换；
   - 针对 Linux 音频回调的低延迟（< 10ms）与 xrun（欠载/过载）防护进行基线调优。
2. **Phase 25-B：X11 / XCB 窗口系统与 JIVE 声明式 UI 渲染验证**：
   - 验证 Linux 桌面环境下 X11 窗口创建、事件循环分发、键盘焦点维持与鼠标交互；
   - 确保 JIVE 声明式 UI 样式表、Design Tokens 与 跨平台字体回退（Noto Sans CJK SC / Source Han Sans 等）在 Linux 下字形清晰、弹窗无白底/黑块闪烁。
3. **Phase 25-C：Linux Release 构建兼容性与分发规范**：
   - 依赖兼容性查证（2026-08）：动态依赖 `libasound.so.2` / `libfontconfig.so.1` / `libfreetype.so.6` soname 稳定且为桌面发行版标配，**保持动态链接、不做静态化**（静态化解决的是不存在的兼容问题）；
   - **真正门槛是构建环境的 glibc / libstdc++ 版本**：正式 Linux 产物由 GitHub Actions `release.yml` 的 `release-linux-x64` job 在 **`ubuntu-24.04` runner**（glibc 2.39 / GCC 13 libstdc++）构建，产物门槛为 GLIBC_2.39 / GLIBCXX_3.4.32；
   - **支持矩阵（glibc ≥ 2.39）**：Ubuntu 24.04 LTS+、Debian 13+、Fedora 41+、Arch / CachyOS / Manjaro 滚动发行版；明确放弃维护期后期发行版（Ubuntu 22.04 / Debian 12 / Mint 21.x）；
   - 制定 Linux 分发归档规范（`DevPiano-vX.Y.Z-linux-x64.tar.gz` 与 `.sha256`）与门槛检查脚本 `scripts/check_linux_glibc_floor.sh`（`readelf --version-info` 对照支持矩阵，防 runner 镜像漂移）。
   - `ci.yml` 合并 Debug 测试与 Release 构建为单一 `linux-gate` job（`ubuntu-24.04`，共享 ccache；含 Release 编译 + 门槛检查，非 tag 触发提前暴露编译问题）；
   - 扩展 `scripts/package_release.sh` 支持 `--linux` 打包选项（tar.gz + sha256，本地备用路径）；
   - 补齐 Linux 平台专项冒烟测试清单（目标发行版实机验证）并全量通过三闸门（format, test, build）。
   - `ci.yml` 增加 `linux-release-gate`（`ubuntu-24.04` 上 Release 编译 + 门槛检查，非 tag 触发提前暴露编译问题）；
   - 扩展 `scripts/package_release.sh` 支持 `--linux` 打包选项（tar.gz + sha256，本地备用路径）；
   - 补齐 Linux 平台专项冒烟测试清单（目标发行版实机验证）并全量通过三闸门（format, test, build）。

---

## 本轮子任务排期（Phase 25）

- [x] **Phase 25-A：ALSA / JACK 音频驱动链路与设备管理机制审查**
- [x] **基础设施：落地 PR-Agent AI 代码审查工作流（DeepSeek v4 Flash）**
- [x] **Phase 25-B (部分)：Linux 桌面字体清晰度（Noto Sans CJK / Source Han Sans 回退链）与设置弹窗首帧防闪烁修复**
- [x] **Phase 25-B (持续)：X11 窗口与键盘事件（KeyPress / 焦点管理）Linux 桌面实机交互回归（CachyOS 实机验证通过：窗口生命周期、键盘演奏、焦点管理、虚拟键盘鼠标、字体渲染、弹窗呈现无问题；含失焦 panic 不打断 MIDI 回放修复）**
- [x] **Phase 25-C：Linux Release 构建兼容性与分发规范（ubuntu-24.04 runner 构建 + glibc 门槛检查脚本 + 归档规范；不做依赖静态化）**
- [ ] **Phase 25-D：ci.yml 合并 linux-gate（Debug 测试 + Release 门槛）+ `package_release.sh --linux` 打包**
- [ ] **Phase 25-E：三闸门基线验证、Linux 专项冒烟清单与指南文档对齐**
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

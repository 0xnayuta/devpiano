# Phase 25：Linux 原生桌面构建与音频驱动适配（完成记录）

> 归档说明：记录 Phase 25 针对 Linux 原生桌面构建、ALSA/JACK 音频驱动链路审查、X11 窗口与 JIVE 声明式 UI 渲染适配、Release 兼容性与 glibc 2.39 分发门槛验证、CI linux-gate 门禁与 release 打包流水线支持的全部交付与实机回归记录。
> 完成日期：2026-08-25
> 替代文档：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 1. 背景与目标

在完成 v1.0.0 正式里程碑与发布打包流水线自动化后，devpiano 进入 **Post-v1.0.0 平台拓展与高阶能力演进** 阶段。本项目在架构设计之初即立足于 JUCE 跨平台标准，但此前构建与验证重心集中在 Windows/MSVC 环境。

Phase 25 的核心目标是彻底打通 Linux 原生桌面环境下的完整体验：
1. **音频链路**：验证 ALSA 与 JACK 音频驱动链路，确保缓冲区协商与低延迟稳定发声；
2. **UI 与窗口**：适配 X11 / XCB 窗口系统、JIVE 声明式 UI 样式渲染、中文字体回退链与焦点事件管理；
3. **构建与分发**：确立 Linux 动态链接依赖策略，建立 glibc 2.39 门槛验证机制与 GitHub Actions 双平台自动构建/打包发布流水线。

```
                              ┌─────────────────────────────────────────────────────────┐
                              │     Phase 25 Linux 原生桌面与音频驱动全链路适配架构     │
                              └───────────────────────────┬─────────────────────────────┘
                                                          │
             ┌────────────────────────────┬───────────────┴───────────────┬────────────────────────────┐
             ▼                            ▼                               ▼                            ▼
┌───────────────────────────┐┌───────────────────────────┐┌───────────────────────────┐┌───────────────────────────┐
│        Phase 25-A         ││        Phase 25-B         ││        Phase 25-C         ││       Phase 25-D/E        │
│   ALSA / JACK 音频链路    ││  X11 / JIVE 桌面交互适配  ││  Release 规范与 glibc 门槛 ││  CI 门禁 / 打包与实机回归  │
│ 驱动初始化与诊断测试套件  ││ 字体回退/失焦panic防护修复││ 动态链接策略/glibc 2.39 检查││ linux-gate/tar.gz/CachyOS │
└───────────────────────────┘└───────────────────────────┘└───────────────────────────┘└───────────────────────────┘
```

---

## 2. 核心实现明细

### Phase 25-A：ALSA / JACK 音频驱动链路验证与设备管理机制审查
1. **音频驱动适配验证**：验证 JUCE `AudioDeviceManager` 在 Linux 环境下对 ALSA 与 JACK 音频后端的初始化、缓冲区协商（512/256/128 samples）与设备枚举；
2. **自动化诊断测试**：落地 Linux 平台专用的音频设备诊断单元测试（`AudioDeviceDiagnosticsLinuxTest`），验证动态设备切换与生命周期安全。

### 基础设施：PR-Agent AI 代码审查工作流
1. **自动化 PR 审查**：落地 `.github/workflows/pr-agent.yml`，接入 DeepSeek v4 Flash 模型为每一个 Pull Request 提供自动化架构审查、代码质量分析与潜在风险提示。

### Phase 25-B：X11 / XCB 窗口系统与 JIVE 声明式 UI 渲染适配
1. **跨平台中文字体回退链**：在 Design Tokens 与 JIVE 样式中配置 Noto Sans CJK SC / Source Han Sans 回退，消除 Linux 下中文排版缺失问题；
2. **失焦保护（Focus Panic）行为修正**：
   - 修复前：失焦时触发全引擎 `requestAllNotesOff` 导致 MIDI 播放被打断或声音吞字；
   - 修复后：失焦判定改为异步判定——进程内辅助窗口切换不打断演奏，真正离开应用时仅释放物理按键（`KeyboardMidiMapper::releaseAllHeldKeys`），保护 MIDI 回放时间线不中断；
3. **窗口交互回归**：CachyOS 实机回归验证窗口缩放、物理键盘演奏、虚拟键盘鼠标操作与弹窗首帧防闪烁。

### Phase 25-C：Linux Release 构建兼容性与分发规范
1. **依赖策略查证**：确认动态依赖（`libasound.so.2`、`libfontconfig.so.1`、`libfreetype.so.6`）soname 长期稳定且为桌面发行版标配，**确立保持动态链接、不做过度静态化**的技术决策；
2. **glibc 2.39 门槛与支持矩阵**：
   - 正式 Linux Release 产物由 GitHub Actions `ubuntu-24.04` runner（glibc 2.39 / GCC 13 libstdc++）构建；
   - 支持矩阵：Ubuntu 24.04 LTS+、Debian 13+、Fedora 41+、Arch / CachyOS / Manjaro；
   - 配套门槛检查脚本：`scripts/check_linux_glibc_floor.sh`（使用 `readelf --version-info` 校验符号版本，防 runner 镜像漂移）。

### Phase 25-D：打包脚本扩展与 CI 流水线合并
1. **`ci.yml` 单一 `linux-gate` 门禁**：合并 Linux Debug 测试与 Release 构建测试为单个 job，共享 ccache 缓存，集成 glibc floor 检查；
2. **`scripts/package_release.sh --linux`**：支持自动执行 glibc 门槛检查并打包生成 `DevPiano-vX.Y.Z-linux-x64.tar.gz` 与 `.sha256` 归档。

### Phase 25-E：三闸门基线验证与发布指南对齐
1. **发布文档对齐**：更新 [`docs/guides/release-workflow.md`](../guides/release-workflow.md) 新增 §5A Linux 手工冒烟测试清单与双平台发布操作流；
2. **环境与已知问题沉淀**：记录 Ubuntu 26.04 TTC 字体扫描环境特例至 [`docs/issues/known-issues.md`](../issues/known-issues.md)。

---

## 3. 验收与交付成果

- **CI 状态**：GitHub Actions `linux-gate`（Debug/Release/glibc floor）与 `windows-gate`（MSVC Debug/Release）全绿；
- **单元测试**：全量 58 类单元测试、11986 项断言在 Linux 环境下 100% 通过；
- **实机回归**：在 CachyOS（2026-08-24）实机环境下完成全功能手工冒烟测试（音频发声、键盘演奏、JIVE 弹窗、预设管理、离线导出）。

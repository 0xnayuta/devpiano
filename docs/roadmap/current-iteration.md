# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 16：UI 性能优化（虚拟键盘局部脏矩形重绘）与预设导入覆盖确认 [已完成，2026-08-20]**

本轮重点：
1. **`CustomKeyboard` 脏矩形局部重绘优化**：消灭虚拟键盘在按键动画与淡出衰减中的全量 `repaint()`，改为精确调用 `repaintKey(keyBounds)` 与早退裁剪，大幅降低密集 MIDI 播放时 UI 渲染线程的 CPU 开销；
2. **预设导入同名覆盖确认提示**：在导入 `.devpiano.preset` 文件时检测同名冲突，接入 `PresetConfirmDialog`（底层 `JiveModalDialog::launchConfirm`）向用户确认是否覆盖，避免静默覆盖已有预设。

---

## Phase 16：UI 性能优化与预设导入覆盖确认 [已完成]

### 背景与目标

1. **虚拟键盘 CPU 占用优化（known-issues §1.3）**：
   - 现象：在密集 MIDI 播放时，虚拟键盘高频收到 noteOn/noteOff，`timerCallback()` 在按键 fade 变化时触发整个键盘（88 键）的全量重绘，JUCE 软件光栅化渲染器造成单核 ~5%-7% CPU 消耗。
   - 目标：将 `repaint()` 改为局部脏矩形重绘 `repaintKey(k)` 与 `g.getClipBounds()` 区域相交快速裁剪，仅绘制发生状态变化的琴键区域，将 UI 线程渲染负载降低 70% 以上。
2. **预设导入同名覆盖保护（known-issues §1.2）**：
   - 现象：`PresetFlowSupport::handleImportPresetFile` 在遇到同名预设时直接静默覆盖本地文件。
   - 目标：检测目标文件是否已存在，若存在则弹出 `PresetConfirmDialog` 提示确认覆盖，提供取消保护。

---

### 前置事实（已核实代码）

| 事实 | 位置 | 影响 |
|---|---|---|
| `CustomKeyboard::timerCallback()` 调用全量 `repaint()` | `source/UI/CustomKeyboard.cpp` | 已优化为 `repaintKey(k)` 局部重绘 |
| `CustomKeyboard::mouseDown` / `mouseDrag` 调用全量 `repaint()` | `source/UI/CustomKeyboard.cpp` | 已优化为 `repaintKey(k)` 局部重绘 |
| `PresetFlowSupport::handleImportPresetFile()` 静默覆盖 | `source/Layout/PresetFlowSupport.cpp` | 已接入 `PresetConfirmDialog` 弹窗确认 |
| `PresetConfirmDialog` 声明式确认弹窗已就位 | `source/UI/PresetDialogs.h` | 已复用于覆盖确认 |

---

### 子任务排期

**Phase 16-A：`CustomKeyboard` 脏矩形局部重绘优化 [已完成]**
- [x] 在 `CustomKeyboard.cpp` 的 `timerCallback()` 中为所有 `fade` 发生改变的键触发 `repaintKey(k)`，不再执行无差别的全量 `repaint()`。
- [x] 在 `mouseDown()`、`mouseDrag()` 按下/滑动按键时仅重绘目标按键区域 `repaintKey(k)`。
- [x] 在 `paintWhiteKeys`、`paintBlackKeys`、`paintKeyLabels` 中引入 `g.getClipBounds().intersects(...)` 早退裁剪，完全跳过未在脏区内的按键渲染路径。

**Phase 16-B：预设导入同名覆盖确认提示 [已完成]**
- [x] 在 `PresetFlowSupport::handleImportPresetFile()` 中检测目标预设文件是否存在。
- [x] 若存在，调用 `PresetConfirmDialog::show` 提示用户是否覆盖同名预设（`TRANS("Overwrite Preset?")`）；
- [x] 用户确认后执行保存并刷新；用户取消则安全终止。

**Phase 16-C：测试补齐、三闸门与验证 [已完成]**
- [x] 在 `KeyboardHitMappingTest.cpp` 中补充 `CustomKeyboard` 脏矩形裁剪绘制测试。
- [x] 执行三闸门验证（`wsl-build --configure-only` / `test` 3101+ 断言全绿 / `format --check` 0 违规 / `win-build` MSVC 编译成功）。
- [x] 更新 `docs/issues/known-issues.md` 与 `docs/roadmap/roadmap.md`。

---

### 验收标准

- [x] 虚拟键盘在动画帧与按键点击时仅重绘受影响琴键的脏矩形区域，无视觉撕裂、残影或显示异常。
- [x] 导入同名预设时正确弹出覆盖确认对话框，点击覆盖正常导入，点击取消放弃操作。
- [x] 全量单元测试 100% 通过，三闸门无报错。

---

### 验证命令

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh test
./scripts/dev.sh format --check
./scripts/dev.sh win-build
```

---

## 历史实现 Backlog

- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

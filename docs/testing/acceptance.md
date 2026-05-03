# devpiano 阶段验收标准

> 用途：定义各阶段的可验证完成标准。  
> 读者：开发者、测试者、阶段验收者。  
> 更新时机：阶段验收标准变化或状态发生明确变化时。

说明：本文件只描述阶段验收，不承担长期规划职责。项目状态与路线图以 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 为准。

## 状态标记

- [x] 已通过
- [ ] 未通过 / 未开始验证
- [~] 部分通过 / 待补充验证

## Phase 1-1：工程骨架可运行

状态：已通过。

验收项：

- [x] `./scripts/dev.sh wsl-build` 构建成功。
- [x] `./scripts/dev.sh win-build` 验证成功。
- [x] JUCE GUI 程序可启动。
- [x] 主窗口正常显示。
- [x] 音频设备能初始化。
- [x] 没有因缺失 JUCE 子模块导致构建失败。

## Phase 1-2：最小演奏链路成立

状态：已通过。

验收项：

- [x] 按下 `A/S/D/F` 等基础按键时可触发 note on。
- [x] 松开对应按键时可触发 note off。
- [x] 虚拟钢琴键盘组件可高亮联动。
- [x] 程序能够发声，来源为内置 fallback synth 或已加载插件。
- [x] 调整基础音量与 ADSR 参数后可听到变化。
- [x] 长按按键时不会异常重复触发。
- [x] 切换窗口焦点后 held key 不残留。

## Phase 2：最小插件扫描能力成立

状态：已通过。

验收项：

- [x] 程序能识别可用插件格式。
- [x] VST3 格式可用。
- [x] 输入默认或指定 VST3 搜索路径后可执行扫描。
- [x] 扫描完成后可在界面列出插件名称。
- [x] 扫描失败不会导致程序崩溃。
- [x] 扫描失败文件可清晰记录（Phase 2-1：失败路径写入 Logger，UI 摘要提示 `see log`）。
- [x] 多目录搜索路径表现稳定（Phase 2-2：`FileSearchPath` 语义多目录字符串，过滤无效目录并持久化规范化路径）。
- [x] 扫描路径与最近插件恢复信息可持久化（Phase 2-3：`KnownPluginList` XML 缓存，启动优先恢复，失败时回退重扫）。

专项插件扫描产品化增强（Phase 2-1..2-4）已全部完成并通过 2026-04-30 人工验证。

## Phase 2：插件实例化并发声

状态：基本通过。

验收项：

- [x] 扫描结果中可选择一个 VST3 乐器。
- [x] 点击加载后能成功创建 `AudioPluginInstance`。
- [x] 插件加载成功后 UI 显示明确状态。
- [x] 电脑键盘输入可驱动插件发声。
- [x] 插件处理链路接入 `processBlock`。
- [x] 插件卸载后不会崩溃或留下非法状态。
- [x] 未加载插件时程序仍保持可运行。
- [x] 可打开支持 editor 的插件窗口。
- [~] 外部 MIDI 输入通路已接通，但真实设备验证因硬件条件暂缓（详见 [`known-issues.md`](known-issues.md) §1）。

专项生命周期回归见：[`plugin-host-lifecycle.md`](plugin-host-lifecycle.md)。

## Phase 2：键盘映射系统可配置

状态：已通过。

验收项：

- [x] 映射内部模型不再只是裸 `unordered_map<int, int>`。
- [x] 映射主路径不再强依赖 `key.getTextCharacter()`。
- [x] 默认布局可加载。
- [x] 当前布局可保存到设置。
- [x] 重启程序后布局可恢复。
- [x] 可恢复默认布局的代码路径已具备。
- [x] 已提供最小布局操作入口。
- [x] 默认布局全量回归已完成（Q/A/Z/数字行、多键组合、启动一致性均通过）。
- [x] 虚拟键盘翻页后映射稳定性问题已修复并验证。
- [x] 自定义 Preset 加载/保存/导入/重命名/删除与启动恢复已完成（Phase 3）。
- [~] 图形化布局编辑器（当前无 UI，不在近期范围）。

专项键盘回归见：[`keyboard-mapping.md`](keyboard-mapping.md)。

## Phase 3：UI 进入正式可用阶段

状态：已通过。

验收项：

- [x] 插件扫描、选择、加载、卸载路径完整。
- [x] 插件状态标签表达清晰。
- [x] MIDI 状态反馈清晰。
- [x] 用户可以理解当前发声来源。
- [x] 设置窗口可打开、保存并关闭。
- [x] 设置窗口关闭时可保存修改后的设备状态。
- [x] 已支持打开并操作已加载插件的 editor 窗口。
- [x] 已形成 `HeaderPanel` / `PluginPanel` / `ControlsPanel` / `KeyboardPanel` 的基础组件分层。
- [x] `MainComponent` 插件流程职责已完成两轮收敛（`PluginFlowSupport` 提取 + Phase 5-1..5-4 状态流收敛）。
- [~] 错误提示与空状态提示仍需完善（低优先级）。

## Phase 3：高级功能恢复

状态：MVP 已通过。布局 Preset 保存/加载/导入/重命名/删除/启动恢复已完成（Phase 3）；录制 / 停止 / 回放 / MIDI 导出最小闭环已接入（Phase 3-3..3-7）；WAV 离线渲染 MVP 已完成（Phase 3-1a..3-1d，E.1–E.9 全部通过）。下一阶段重点是录制 / 回放稳定化和外部 MIDI 硬件验证补齐（详见 [`known-issues.md`](known-issues.md) §1）。VST3 插件离线渲染（Phase 3-2）后置。MP4 导出、复杂编辑与 tempo map 等仍为后续增强。

验收项：

### 布局 Preset

- [x] 可保存当前键盘布局为 `.freepiano.layout` Preset。
- [x] 可导入并加载 `.freepiano.layout` 用户布局。
- [x] 可通过持久化的 `layoutId` 恢复内置或用户布局。
- [x] 可重命名用户布局的下拉菜单显示名称。
- [x] 可删除用户布局，内置布局不可删除。
- [x] 可恢复当前布局的默认映射。

专项布局 preset 回归见：[`phase3-layout-presets.md`](phase3-layout-presets.md)。

### 录制 / 回放

设计参考见：[`../features/phase3-recording-playback.md`](../features/phase3-recording-playback.md)。

- [x] 可开始录制。
- [x] 可停止录制。
- [x] 可回放录制结果。
- [x] 回放事件重新进入当前插件 / fallback synth 发声路径。
- [~] 回放期间事件时间顺序已由 sample-based timeline 设计覆盖，仍需按专项清单持续人工回归。
- [~] 实时音频线程边界、回放结束通知、采样率变化和 Stop 清理悬挂音路径仍作为下一阶段稳定化重点。

专项录制 / 回放回归见：[`recording-playback.md`](recording-playback.md)。

### 导出

设计参考见：[`../features/phase3-recording-playback.md`](../features/phase3-recording-playback.md)。

- [x] 可导出 MIDI。
- [x] 可导出 WAV（fallback synth 离线渲染；VST3 插件离线渲染后置为 Phase 3-2）。
- [x] 导出失败时有基础错误处理，已按专项清单验证无 take、取消保存、无权限路径等边界。
- [x] WAV 离线渲染 MVP 设计与实现切片已完成（Phase 3-1a..3-1d）；E.1–E.9 全部通过。

## Phase 4：MIDI 文件导入与回放兼容性

状态：核心能力已通过。Phase 4-1、Phase 4-2、Phase 4-3、Phase 4-4、Phase 4-5、Phase 4-7、Phase 4-8 已实现并通过 2026-05-01 人工验收；Phase 4-6 已搁置。

验收项：

- [x] 可通过 Import MIDI 打开标准 `.mid` 文件并在当前播放链路回放。
- [x] Record 期间 Import MIDI 禁用，避免录制与导入状态冲突。
- [x] 导入空文件、读取失败文件或无 note 文件时写 Logger，不崩溃。
- [x] 导入播放内容 Stop 后，Export MIDI 保持 disabled，Export WAV 可用。
- [x] Playing 期间 Import MIDI 禁用；需先 Stop 再导入另一个 MIDI，避免异步替换 playback 状态竞争。
- [x] 自动选择含 note 最多的轨道；track 0 只有 tempo/meta 时可选择后续有 note 的轨道。
- [x] MIDI playback 时虚拟键盘可随 note on/off 实时显示按下与松开。
- [x] 主窗口首次尺寸合适，用户手动调整后可持久化恢复。
- [x] 最近导入路径已持久化；最近导出路径已接入 Export MIDI / Export WAV FileChooser。
- [x] 播放中点击 `Back` 可从当前 take 开头重新播放。
- [x] PPQ/timeFormat 修正、轨道诊断和自动选轨已覆盖主要 playback/边界测试；导入 playback take 禁止再次导出 MIDI 是预期边界。
- [x] 导入 MIDI 后允许导出 WAV；VST3 插件音色离线渲染仍归 Phase 3-2 后续计划。
- [~] 合并所有轨道 note 到单一 timeline（Phase 4-6）未实现且已搁置；当前保留 note-rich 单轨选择为推荐模式。

专项 MIDI 文件导入回归见：[`midi-file-import.md`](midi-file-import.md)。

## Phase 6：演奏数据持久化与播放体验增强

状态：规划中，未开始实现。

验收项：

- [ ] 可将录制 take 保存为 `.devpiano` JSON 文件，包含完整事件数据。
- [ ] 可打开 `.devpiano` 文件并回放，内容与原始录制一致。
- [ ] 打开损坏或格式错误的 `.devpiano` 文件时不崩溃，Logger 输出错误。
- [ ] Save/Open 按钮状态与录制/播放状态正确联动。
- [ ] 保存和打开路径记忆生效。
- [ ] 播放速度可调（0.50x–2.00x），播放中变速立即生效。
- [ ] 最近文件列表最多 10 条，点击可打开。
- [ ] 拖拽 `.devpiano` / `.mid` 文件到窗口可打开。
- [ ] 可选中并删除单个音符，删除后回放和保存正确。
- [ ] 导入 MIDI 时可收集 sustain CC64、pitch bend、program change 并回放。

专项测试见：[`phase6-performance-persistence.md`](phase6-performance-persistence.md)。

## 建议最小回归集合

每次关键修改后，至少验证：

- [x] WSL 构建通过：`./scripts/dev.sh wsl-build --configure-only`。
- [x] `./scripts/dev.sh win-build` 验证成功。
- [x] 程序可启动并初始化音频设备。
- [x] `A/S/D/F` 可触发 note on / note off。
- [x] 虚拟键盘高亮与释放正常。
- [x] 启动或音频重建后立即弹奏首音稳定；当前通过 `25ms` audio warmup 避免早期音高异常。
- [x] 如修改插件相关代码：可扫描、加载、卸载一个 VST3 插件。
- [x] 如修改键盘相关代码：执行 [`keyboard-mapping.md`](keyboard-mapping.md) 中优先测试包。
- [x] 如修改插件生命周期相关代码：执行 [`plugin-host-lifecycle.md`](plugin-host-lifecycle.md) 中优先测试包。
- [x] 如修改录制、回放或 MIDI 导出相关代码：执行 [`recording-playback.md`](recording-playback.md) 中优先测试包。
- [x] 如修改 MIDI 文件导入、导入后回放、自动选轨或 playback 切换逻辑：执行 [`midi-file-import.md`](midi-file-import.md) 中建议最小回归集合。
- [ ] 如修改演奏文件保存/打开、播放速度、最近文件列表或基础编辑相关代码：执行 [`phase6-performance-persistence.md`](phase6-performance-persistence.md) 中建议最小回归集合。

已知硬件限制：

- 外部 MIDI 设备手工验证暂缓（无硬件），详见 [`known-issues.md`](known-issues.md) §1。

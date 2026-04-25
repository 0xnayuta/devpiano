# devpiano 插件宿主生命周期与退出稳定性测试清单

> 用途：对插件扫描、加载、卸载、editor、音频设备重建与退出等高风险路径进行手工验证。  
> 读者：修改 `PluginHost`、插件 editor、音频设备重建或退出流程的开发者。  
> 更新时机：插件宿主生命周期或相关 UI / 音频流程变化时。

状态标记：
- [x] 已通过
- [ ] 未通过 / 未开始验证
- [~] 部分通过 / 待补充验证

说明：本文件记录专项生命周期回归测试，不等同于“插件功能是否已经可用”。即使 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 中插件发声已通过，本文件中的组合压力场景仍可能未完整执行。

相关文档：
- 阶段验收：[`acceptance.md`](acceptance.md)
- 项目路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 1. 测试目标

本测试文档主要验证以下内容：

- 插件扫描、加载、卸载过程中的生命周期顺序是否稳定
- 插件 editor 窗口与插件实例之间是否存在悬空引用风险
- 音频设备重建前后，插件宿主是否能正确释放并恢复资源
- 程序退出时，MIDI 输入、音频设备、插件实例、editor 窗口是否按预期关闭
- 在 Debug 环境下，是否仍出现明显的崩溃、卡死或高概率资源告警

---

## 2. 测试前提

### 环境前提
- [x] 项目可成功构建
- [x] WSL 构建基线可用：`./scripts/dev.sh wsl-build`
- [x] Windows MSVC 验证基线可用：`./scripts/dev.sh win-build`
- [x] 程序可启动
- [x] 音频设备可初始化
- [x] 至少有一个可加载的 VST3 插件

### 建议测试插件
优先准备两类插件：

- 轻量插件：如 `Surge XT`
- 较复杂或启动较慢的插件：用于观察 editor、prepare/release、退出阶段行为

### 代码级 / UI 观察基线

> 说明：当前插件生命周期回归仍以手工测试为主，但建议先确认以下代码与 UI 观察点，避免把“状态展示不清晰”误判成真实生命周期故障。

建议先复核以下实现边界：

- `source/Plugin/PluginHost.*`
  - 承载 `scanVst3Plugins()`、`loadPluginByName()`、`prepareToPlay()`、`releaseResources()`、`unloadPlugin()`。
- `source/MainComponent.*`
  - 承载 scan / load / unload / editor / shutdown 的主协调逻辑。
  - 当前 `prepareForAudioDeviceRebuild()` 会先执行 `closePluginEditorWindow()` 与 `shutdownAudio()`。
  - 当前 `runPluginActionWithAudioDeviceRebuild()` 包裹 scan / load / unload 路径。
  - 当前析构顺序为：关闭 editor -> 关闭音频/MIDI -> `pluginHost.unloadPlugin()`。
- `source/Audio/AudioEngine.cpp`
  - 音频初始化时会对已加载插件执行 `prepareToPlay()`；释放时会走 `releaseResources()`。
- `source/UI/PluginPanel.cpp`
  - 生命周期手工回归时，优先观察顶部状态标签是否正确显示以下关键信息：
    - `Loaded: <plugin>`
    - `@ <sampleRate> Hz / <blockSize>`
    - `Editor open`
    - `Load error: ...`
    - 最新 scan summary

当前手工基线建议区分两类结论：

- 代码级 / 静态基线：确认 scan / load / unload / editor / shutdown 路径的主协调顺序仍存在。
- Windows 手工回归：实际运行程序，观察 UI 状态、发声、editor 行为、重扫、退出与 Debug 告警。

---

## 3. 基础插件生命周期测试

## 3.1 扫描后加载插件

### 目标
验证扫描完成后可正常加载插件，并进入可演奏状态。

### 测试步骤
- [x] 启动程序
- [x] 扫描 VST3 路径
- [x] 从列表中选择一个插件
- [x] 点击 `Load`
- [x] 使用 `A/S/D/F` 试弹

### 预期结果
- [x] 插件可成功加载
- [x] UI 显示 Loaded / prepared 等状态正常
- [x] 可正常发声
- [x] 无明显卡顿或崩溃

### 状态
- [x] 已验证

---

## 3.2 连续加载同一插件

### 目标
验证重复加载路径不会留下脏状态。

### 测试步骤
- [x] 加载一个插件
- [x] 卸载该插件
- [x] 再次加载同一插件
- [x] 重复 3~5 次

### 预期结果
- [x] 每次都能成功加载/卸载
- [x] 无崩溃、无明显状态错乱
- [x] editor 状态不会异常残留

### 状态
- [x] 已验证

---

## 3.3 在已加载插件状态下重新扫描

### 目标
验证“扫描会先释放当前插件”这一高风险路径是否稳定。

### 测试步骤
- [x] 加载一个插件并确认可发声
- [x] 不关闭程序，直接再次执行插件扫描
- [x] 扫描完成后观察 UI 状态
- [x] 重新加载一个插件试弹

### 预期结果
- [x] 扫描过程无崩溃
- [x] 扫描前已加载插件被安全卸载
- [x] 扫描后 UI 状态一致
- [x] 重新加载插件后仍可正常发声

### 状态
- [x] 已验证

---

## 4. 插件 editor 生命周期测试

## 4.1 打开并关闭 editor

### 目标
验证 editor 可正常创建和销毁。

### 测试步骤
- [x] 加载支持 editor 的插件
- [x] 点击 `Open Editor`
- [x] 观察 editor 窗口是否正常显示
- [x] 点击 editor 窗口关闭按钮

### 预期结果
- [x] editor 窗口可正常打开
- [x] 关闭后程序无崩溃
- [x] UI 状态能正确反映 editor 已关闭

### 状态
- [x] 已验证

---

## 4.2 打开 editor 后卸载插件

### 目标
验证 editor 打开时卸载插件不会留下悬空窗口或失效引用。

### 测试步骤
- [x] 加载支持 editor 的插件
- [x] 打开 editor
- [x] 不手动关闭 editor，直接点击 `Unload`

### 预期结果
- [x] editor 窗口被安全关闭
- [x] 插件被安全卸载
- [x] 程序无崩溃、无明显卡死

### 状态
- [x] 已验证

---

## 4.3 打开 editor 后重新扫描插件

### 目标
验证 editor + scan 的组合路径是否稳定。

### 测试步骤
- [x] 加载支持 editor 的插件
- [x] 打开 editor
- [x] 不关闭 editor，直接执行插件扫描

### 预期结果
- [x] editor 会先被关闭或安全销毁
- [x] 扫描过程无崩溃
- [x] 扫描后程序仍可继续使用

### 状态
- [x] 已验证

---

## 5. 音频设备重建相关测试

## 5.1 加载插件后切换音频设备设置

### 目标
验证音频设备变化后插件 prepare/release 路径稳定。

### 测试步骤
- [x] 加载一个插件
- [x] 打开设置窗口
- [~] 修改音频设备、buffer size 或 sample rate（若环境允许）
- [x] 保存并关闭设置
- [x] 再次试弹

### 预期结果
- [x] 程序无崩溃
- [x] 设置关闭后音频恢复正常
- [~] `buffer size` 选项当前无法选择除 `10 ms` 以外的其他值；插件仍可继续发声

### 状态
- [~] 部分验证：音频设置窗口可打开、保存、关闭且不会导致插件链路异常，但 `buffer size` 选项当前无法选择除 `10 ms` 以外的其他值，后续需继续调查

---

## 5.2 连续执行 load / unload / scan

### 目标
验证频繁设备重建与插件切换不会迅速积累异常状态。

### 测试步骤
- [x] 扫描插件
- [x] 加载插件 A
- [x] 卸载插件 A
- [x] 重新扫描
- [x] 加载插件 B
- [x] 打开/关闭 editor
- [x] 再卸载插件
- [x] 重复 2~3 轮

### 预期结果
- [x] 程序持续稳定
- [x] 不出现明显资源未释放导致的异常
- [x] UI 状态与实际插件状态一致

### 状态
- [x] 已验证

---

## 6. 程序退出稳定性测试

## 6.1 加载插件后直接退出程序

### 目标
验证最常见退出路径是否稳定。

### 测试步骤
- [x] 启动程序
- [x] 扫描并加载一个插件
- [x] 直接关闭主窗口退出程序

### 预期结果
- [x] 程序正常退出
- [x] 无明显崩溃
- [~] 本轮未观察到明显异常；如后续在特定插件 / Debug 环境仍出现告警，应继续记录插件名称与复现条件

### 状态
- [x] 已验证

---

## 6.2 打开 editor 后直接退出程序

### 目标
验证带 editor 的退出路径是否稳定。

### 测试步骤
- [x] 加载支持 editor 的插件
- [x] 打开 editor
- [x] 不手动关闭 editor，直接关闭主窗口退出程序

### 预期结果
- [x] 程序正常退出
- [x] editor 与插件实例被安全销毁
- [~] 本轮未观察到明显异常；如后续出现 Debug 告警，需记录是否可稳定复现

### 状态
- [x] 已验证

---

## 6.3 外部 MIDI 打开状态下退出程序

### 目标
验证 MIDI 输入、音频设备与插件在退出阶段的关闭顺序是否稳定。

### 测试步骤
- [ ] 保持外部 MIDI 输入已打开
- [ ] 加载一个插件
- [ ] 直接关闭程序

### 预期结果
- [ ] 程序正常退出
- [ ] 无明显回调落到已析构对象上的异常
- [ ] 无高概率卡死

### 状态
- [ ] 因缺少外部 MIDI 设备暂未验证

---

## 7. 建议优先测试包

### 优先测试包 A：最关键生命周期路径
- [x] 3.1 扫描后加载插件
- [x] 3.3 在已加载插件状态下重新扫描
- [x] 4.2 打开 editor 后卸载插件
- [x] 6.1 加载插件后直接退出程序

### 优先测试包 B：高风险 editor / 退出组合
- [x] 4.3 打开 editor 后重新扫描插件
- [x] 6.2 打开 editor 后直接退出程序
- [ ] 6.3 外部 MIDI 打开状态下退出程序（缺少设备，暂未验证）

### 优先测试包 C：长期回归
- [x] 3.2 连续加载同一插件
- [~] 5.1 加载插件后切换音频设备设置（`buffer size` 选项仍待调查）
- [x] 5.2 连续执行 load / unload / scan

---

## 8. 每轮测试记录模板

建议每次对生命周期相关代码做改动后，在本文件底部追加一次记录：

```md
## Lifecycle Test Run YYYY-MM-DD
- 变更内容：
- 测试插件：
- 执行用例：
- 通过项：
- 失败项：
- Debug 告警：
- 复现条件：
- 后续修复建议：
```

---

## Lifecycle Test Run 2026-04-25
- 变更内容：
  - 按 [`current-iteration.md`](../roadmap/current-iteration.md) 开始推进“插件生命周期基线回归”。
  - 先补齐代码级 / UI 观察基线，明确本轮优先关注 scan / load / unload / editor / exit 组合路径。
- 测试插件：
  - 尚未执行 Windows 侧实际插件回归；建议优先使用轻量、支持 editor 的 VST3（如 `Surge XT`）。
- 执行用例：
  - 代码级复核 `source/Plugin/PluginHost.*` 的 scan / load / prepare / release / unload 路径。
  - 代码级复核 `source/MainComponent.*` 的 `runPluginActionWithAudioDeviceRebuild()`、`prepareForAudioDeviceRebuild()`、editor 关闭与析构顺序。
  - 代码级复核 `source/Audio/AudioEngine.cpp` 的插件 prepare/release 接入。
  - 代码级复核 `source/UI/PluginPanel.cpp` 的状态标签输出项，确认手工回归时有明确 UI 观察点。
- 通过项：
  - 当前代码中 scan / load / unload 会经过统一的 audio rebuild 包装路径。
  - 当前代码中 scan / load / unload 前会先关闭 plugin editor 窗口。
  - 当前析构路径中已包含 editor 关闭、MIDI 输入关闭、音频关闭和 `pluginHost.unloadPlugin()`。
  - 当前 UI 已暴露 Loaded / prepared / editor-open / last scan summary / load error 等手工观察信号。
- 失败项：
  - 尚未执行 7 节优先测试包 A 的 Windows 手工回归，暂无实际通过/失败结论。
- Debug 告警：
  - 本次仅完成代码级基线整理，未采集新的运行时 Debug 告警。
- 复现条件：
  - 待后续在 Windows/MSVC 验证环境下执行 3.1、3.3、4.2、6.1 后补充。
- 后续修复建议：
  - 下一步优先执行优先测试包 A，并把插件名称、是否支持 editor、是否可稳定复现告警一并记录到本文件。

---

## Lifecycle Test Run 2026-04-26
- 变更内容：
  - 根据 Windows 侧人工验证结果，回填插件生命周期基线回归结论。
  - 本轮重点覆盖基础插件生命周期、editor 生命周期、连续 load / unload / scan，以及直接退出路径。
- 测试插件：
  - 已使用支持 editor 的 VST3 插件完成手工回归；具体插件名称未单独记录。
- 执行用例：
  - 3.1 扫描后加载插件。
  - 3.2 连续加载 / 卸载同一插件 3~5 次。
  - 3.3 已加载插件状态下重新扫描。
  - 4.1 打开并关闭 editor。
  - 4.2 打开 editor 后卸载插件。
  - 4.3 打开 editor 后重新扫描插件。
  - 5.1 加载插件后切换音频设备设置。
  - 5.2 连续执行 load / unload / scan。
  - 6.1 加载插件后直接退出程序。
  - 6.2 打开 editor 后直接退出程序。
- 通过项：
  - 3 节基础插件生命周期测试均未发现明显问题。
  - 4 节 plugin editor 生命周期测试均未发现明显问题。
  - 5.2 连续执行 load / unload / scan 未发现明显问题。
  - 6.1 / 6.2 退出路径未发现明显问题。
- 失败项：
  - 5.1 中设置窗口里的 `buffer size` 选项当前无法选择除 `10 ms` 以外的其他值。
- Debug 告警：
  - 本轮未记录到必须单独追踪的稳定 Debug 告警。
- 复现条件：
  - `buffer size` 选项问题出现在插件已加载、打开设置窗口并尝试修改音频设置的场景下。
  - 6.3 因缺少外部 MIDI 设备，当前无法补齐退出阶段验证。
- 后续修复建议：
  - 继续调查 `buffer size` 选项受当前 Windows / JUCE 音频后端限制，还是项目设置恢复路径导致的体验问题。
  - 在具备外部 MIDI 设备后补齐 6.3 手工回归。

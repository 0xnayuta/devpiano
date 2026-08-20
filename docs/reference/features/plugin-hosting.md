# VST3 插件宿主与生命周期管理功能说明

> 用途：说明 devpiano 的 VST3 插件扫描（`PluginHost`）、分片扫描会话、XML 缓存恢复、插件实例加载/卸载生命周期、独立 Editor 窗口托管与高风险路径专项回归清单。
> 当前状态：已全量实现并稳定服务于第三方音色扩展。
> 更新时机：插件扫描算法、缓存持久化协议、Editor 托管逻辑或生命周期钩子发生变化时。

---

## 1. 概述与设计定位

VST3 插件宿主是 devpiano 连接专业音乐制作与高品质虚拟乐器（如 Pianoteq, Kontakt, Surge XT, Spitfire LABS）的核心桥梁：

1. **现代 VST3-First 架构**：基于 JUCE `AudioPluginFormatManager` 与 `AudioPluginInstance` 标准宿主抽象，聚焦 VST3 单一现代插件格式；
2. **异步分片扫描（Chunked Scan Session）**：采用消息线程分片扫描机制，每 tick 推进一个插件，状态栏实时更新扫描进度与当前插件名称，消除传统扫描导致的界面假死；
3. **XML 缓存持久化与极速冷启动**：已扫描插件列表以 `KnownPluginList` XML 持久化于设置中，冷启动时秒级恢复缓存，避免每次启动重复重扫；
4. **失败文件细粒度追踪**：清晰记录每个扫描失败的文件路径（`lastScanFailedFiles`），Logger 详细输出失败原因，UI 友好提示 `(see log)`；
5. **严密的生命周期隔离与防御**：独立托管插件 Editor 窗口；在音频设备重建、重新扫描或退出应用时，严格遵循“关闭 Editor → 停止音频回调 → releaseResources → 卸载实例”的确定性顺序，杜绝悬挂指针与死锁。

---

## 2. 核心架构与主运行链路

```text
[插件扫描链路]
用户触发扫描 ──► PluginOperationController::scanVst3Plugins()
                     │
                     ▼
                 PluginHost::beginVst3ScanSession() (消息线程分片推进)
                     ├── 遍历 FileSearchPath (支持多目录与规范化过滤)
                     ├── 逐个探测 VST3 ──► 成功项加入 KnownPluginList ──► 写入 XML
                     └── 失败项记录至 lastScanFailedFiles ──► UI 显示摘要
                     │
[插件加载与发声链路]
用户选择插件 ──► PluginOperationController::loadPluginByName()
                     │
                     ├── 1. 关闭已有 Editor 窗口并解绑
                     ├── 2. AudioPluginFormatManager::createPluginInstance()
                     ├── 3. PluginHost::prepareToPlay(sampleRate, blockSize)
                     ├── 4. AudioEngine 将 processBlock() 切换至插件实例
                     └── 5. 电脑键盘弹奏 ──► 驱动插件合成高品质音频
                     │
[Editor 独立窗口托管]
用户点击 Open Editor ──► PluginOperationController::openPluginEditor()
                             │
                             └── 创建 PluginEditorWindow (独立顶层窗口托管 plugin->createEditor())
```

---

## 3. 关键机制与鲁棒性设计

### 3.1 多目录扫描与路径持久化
- 路径输入支持 `juce::FileSearchPath` 语法（分号或逗号分隔多个路径）；
- 扫描前自动过滤不存在的非法路径，并将规范化后的有效路径持久化到 `SettingsModel::pluginSearchPaths`。

### 3.2 扫描状态机互斥保护
扫描期间，`PluginOperationController` 自动拦截并忽略以下操作，防止状态机错乱：
- 重复点击 Scan 按钮；
- 尝试加载或卸载插件；
- 尝试打开 Editor 窗口。

### 3.3 辅助窗口与键盘焦点防抢夺
打开插件 Editor 窗口后，用户可能需要使用电脑键盘在插件内输入参数或试弹。`MainComponent` 的异步焦点恢复机制（`restoreKeyboardFocus`）在检测到 Editor 窗口处于激活状态时会自动跳过抢焦动作，防止主窗口将辅助窗口顶到后台。

### 3.4 退出与音频设备重建安全序列
应用析构或音频设备重建时，严格按照以下安全析构顺序执行：
1. `closePluginEditorWindow()`：销毁 UI 窗口与底层 OS 视图句柄；
2. `shutdownAudio()`：切断实时音频硬件回调；
3. `pluginHost.unloadPlugin()`：执行 `releaseResources()` 并释放 `AudioPluginInstance`。

---

## 4. 插件生命周期专项回归清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **PLG-001** | 扫描后加载插件 | 扫描本地 VST3 目录 → 选择插件点击 Load → 试弹发声正常，状态栏显示 `Loaded: <Name>` | [x] 已通过 |
| **PLG-002** | 连续重复加载与卸载 | 连续加载/卸载同一插件 5 次以上，无内存泄漏、无界面卡死，状态恢复正常 | [x] 已通过 |
| **PLG-003** | 加载状态下重新扫描 | 在已有插件加载并演奏状态下再次点击 Scan，旧插件安全卸载，扫描平稳完成 | [x] 已通过 |
| **PLG-004** | 打开并关闭 Editor | 点击 Open Editor 打开插件原生界面，操作旋钮与音色切换正常，关闭窗口无报错 | [x] 已通过 |
| **PLG-005** | 打开 Editor 状态下卸载插件 | 在 Editor 窗口保持打开时点击 Unload 按钮，Editor 窗口自动关闭，插件安全释放 | [x] 已通过 |
| **PLG-006** | 打开 Editor 状态下退出程序 | 在 Editor 窗口打开状态下直接点击主窗口右上角关闭按钮，程序平稳退出，无崩溃与报错 | [x] 已通过 |
| **PLG-007** | XML 缓存冷启动秒级恢复 | 首次扫描完成后重启应用，下拉菜单立即呈现已缓存插件列表，无需重新扫描 | [x] 已通过 |
| **PLG-008** | 损坏/不兼容 VST3 容错 | 扫描包含损坏或 32-bit 的非法 VST3 文件，扫描跳过该文件并不崩溃，Logger 准确记录路径 | [x] 已通过 |

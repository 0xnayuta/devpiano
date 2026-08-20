# 国际化与多语言机制功能说明

> 用途：说明 devpiano 的多语言国际化架构、`LocaleManager` 语言管理器、编译期二进制内嵌、运行时即时切换与新增语言包指南。
> 当前状态：已全量实现，支持英文（English）与简体中文（zh-CN）运行时零重启即时切换（Phase 7 成果）。
> 更新时机：语言枚举、语言包加载策略或本地化宏规范发生变化时。

---

## 1. 架构定位与设计目标

为了满足国内外用户的使用习惯，devpiano 建立了轻量、高可靠的国际化（i18n）子系统：

1. **零重启即时切换**：用户在设置窗口切换语言时，界面所有文本、弹窗、状态栏与菜单即时刷新，无需重启应用程序。
2. **零外部文件依赖（高可靠性）**：中文语言包（`zh_CN.loc`）通过 CMake `juce_add_binary_data` 编译期内嵌进二进制文件，确保程序在任意纯净机器绿色运行时 100% 具备完整的中文支持。
3. **外部补丁容错扩展**：支持从可执行文件同级目录或 `locales/` 目录动态加载外部 `.loc` 文件作为 Fallback，方便高级用户在不重新编译源码的前提下微调或补充翻译文本。

---

## 2. 国际化开发规范（`TRANS` 宏）

源码中所有面向用户的可见字符串一律使用 JUCE 原生 `TRANS()` 宏包裹（英文作为原生基准文本）：

```cpp
// 界面标签与按钮
auto buttonText = TRANS("Export WAV");
auto dialogTitle = TRANS("Preset Name:");

// 格式化与动态文本
auto status = TRANS("Scanning:") + " " + pluginName;
```

- 当语言为 **English** 时，JUCE 的 `LocalisedStrings::getCurrentMappings()` 为 `nullptr`，`TRANS("...")` 零开销直接返回原英文字符串；
- 当语言为 **简体中文 (zh-CN)** 时，`TRANS("...")` 自动查表转换为对应的中文字符串。

---

## 3. `LocaleManager` 双层加载与激活策略

`source/Locale/LocaleManager.h` 封装了完整的语言生命周期管理：

```text
[用户选择语言 zh-CN] ──► LocaleManager::activate(Language::zhCN)
    │
    ├── 1. Primary 层：从 BinaryData::zh_CN_loc (编译期内嵌) 构建 LocalisedStrings
    ├── 2. Secondary 层 (可选)：探测外部 "zh-CN.loc" / "zh_CN.loc" 文件并挂载为 Fallback
    └── 3. 生效：juce::LocalisedStrings::setCurrentMappings(zh.release())
```

### 3.1 语言枚举与代码映射
- `Language::en` ↔ `"en"`（English）
- `Language::zhCN` ↔ `"zh-CN"`（简体中文）

### 3.2 语言持久化
- 用户选择的语言代码保存于 `SettingsModel::language` 字段中；
- 应用冷启动时，`AppStateBuilder` 读取持久化配置并调用 `devpiano::locale::activate()`，在首个窗口展示前完成全系统语言注入。

---

## 4. 语言包文件规范（`zh_CN.loc`）

`source/Locale/zh_CN.loc` 遵循 JUCE 标准 `LocalisedStrings` 格式：

```text
language: zh-CN
countries: CN

"Record" = "录制"
"Stop" = "停止"
"Play" = "播放"
"Back" = "重放"
"Save As New" = "另存为新预设"
"Preset Name:" = "预设名称："
"Delete" = "删除"
"Cancel" = "取消"
"Custom Keyboard" = "自定义键盘"
"Audio Device" = "音频设备"
```

---

## 5. 扩展新语言指引

如需为 devpiano 新增一种语言（例如日语 `ja-JP` 或繁体中文 `zh-TW`）：

1. **编写 `.loc` 文件**：在 `source/Locale/` 下创建 `ja_JP.loc`；
2. **注册 CMake 二进制打包**：在 `CMakeLists.txt` 的 `juce_add_binary_data` 中追加该 `.loc` 文件；
3. **扩展 `LocaleManager.h`**：
   - 在 `Language` 枚举中添加对应项（如 `jaJP`）；
   - 在 `activate()` 中补充该语言的加载逻辑；
   - 在 `languageDisplayName()` 中添加该语言的原生显示名（如 `"日本語"`）。
4. **运行三闸门验证**并提交。

---

## 6. 确定性测试与回归清单

| 验证项 | 预期行为 | 状态 |
|---|---|:---:|
| **冷启动中文恢复** | 设置保存为 `zh-CN` 后重启应用，主界面、按钮与状态栏 100% 显示中文 | [x] 已通过 |
| **运行时即时双向切换** | 在设置窗口中从中文切到英文，再切回中文，界面即时更新无撕裂 | [x] 已通过 |
| **模态弹窗国际化** | 预设重命名、删除确认与歌曲信息编辑弹窗的标题、标签与按钮正确跟随语言 | [x] 已通过 |
| **脱离源码独立运行** | 将 `DevPiano.exe` 移动至独立空白目录，中文依然 100% 正常显示 | [x] 已通过 |

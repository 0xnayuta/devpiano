# 国际化与多语言机制功能说明

> 用途：说明 devpiano 的多语言国际化架构、`LocaleManager` 语言管理器、编译期二进制内嵌、运行时即时切换与新增语言包指南。
> 行为范围：支持英文（English）与简体中文（zh-CN）运行时零重启切换、语义标题刷新及单参数完整消息模板（[ADR-015](../../decisions/ADR-015-localized-message-templates-and-punctuation.md)）；已有提示文本和已打开原生窗口不保证即时重译。
> 更新时机：语言枚举、语言包加载策略或本地化宏与模板规范发生变化时。

---

## 1. 架构定位与设计目标

为了满足国内外用户的使用习惯，devpiano 建立了轻量、高可靠的国际化（i18n）子系统：

1. **零重启语言切换**：主界面和设置窗口沿用现有消息线程刷新链路；新弹窗按创建时的当前语言生成。不将所有已打开的原生窗口立即重译列为已验证能力。
2. **零外部文件依赖（高可靠性）**：中文基础语言包（`zh_CN.loc`）通过 CMake `juce_add_binary_data` 编译期内嵌进二进制文件（`BinaryData::zh_CN_loc`），确保程序在任意纯净机器绿色便携运行时具备开箱即用的基础中文支持（技术数据与第三方插件名保持原样，不属于本地化翻译范围）。
3. **外部缺键补充（Fallback）**：依次查找 `zh-CN.loc`、`zh_CN.loc`；每个文件名按可执行文件同级 `locales/`、当前工作目录 `locales/`、当前工作目录的顺序探测，单文件不超过 2 MiB。外部表仅补充内嵌表缺失的词条，不能覆盖主表已有译文（JUCE [`setFallback`](https://docs.juce.com/master/classjuce_1_1LocalisedStrings.html) 的查找语义）。

---

## 2. 国际化开发规范（`TRANS` 宏与消息模板）

源码中所有面向用户的可见自然语言字符串一律使用 JUCE 原生 `TRANS()` 宏包裹（英文作为原生基准文本）：

### 2.1 界面静态标签与按钮

```cpp
auto buttonText = TRANS("Export WAV");
auto dialogTitle = TRANS("Preset Name:");
```

### 2.2 动态参数消息（单参数完整模板）

预设删除/覆盖确认、保存/重命名/删除提示、扫描/加载状态、预备拍计数、绑定说明及 WAV 导出结果使用包含 `{0}` 的完整消息模板；目标语言控制语序、标点和空格。依据 [ADR-015](../../decisions/ADR-015-localized-message-templates-and-punctuation.md)，自然语言句子不使用 `+` 拼接词句碎片。

```cpp
auto status = TRANS("Scanning: {0}...").replace("{0}", pluginName);
auto prompt = TRANS("Delete preset \"{0}\"? This cannot be undone.").replace("{0}", presetName);
```

语言包 `source/Locale/zh_CN.loc` 中对应整句词条模板（中文仅存在于 `.loc` 文件与文档中）：
```text
"Scanning: {0}..." = "正在扫描：{0}..."
"Delete preset \"{0}\"? This cannot be undone." = "确定删除演奏预设“{0}”？此操作无法撤销。"
```

> **注意（实现与边界约束）**：
> - **单参数与原样插入**：当前调用仅对整句模板执行一次 `{0}` 替换；参数中的 `{0}`、`{1}`、`%1`、路径和标点不再解释。没有通用多参数 formatter，也不采用链式 `.replace()`。
> - **允许的技术拼接**：HTML/SVG 标记外壳（`"<span>" + text + "</span>"`）、独立技术信息分隔（`name + " | " + status`）或固定日志标签允许使用 `+` 拼接；禁止的仅为自然语言词句拼接。
> - **排版与标点**：中文提示/确认默认使用全角标点与弯引号 `“{0}”`，按钮标题不补末尾句号；技术数字、单位、路径、快捷键与用户输入原样保留，禁止全角强转。

### 2.3 运行时开销与线程红线

- 当语言为 **English** 时，当前映射为 `nullptr`，`TRANS("...")` 回退原英文文本；
- 当语言为 **简体中文 (zh-CN)** 时，`TRANS("...")` 在当前映射表中查找译文，缺键沿 fallback 链查询或回退原文。两种模式仍有函数调用、字符串及全局映射访问成本，不承诺零锁或零开销。
- **实时线程红线**：`TRANS()` 查表与 `.replace()` 限于消息线程或其他非实时工作路径（如 WAV 后台任务）；严禁在实时音频回调中执行。

---

## 3. `LocaleManager` 双层加载与激活策略

`source/Locale/LocaleManager.h` 通过 `devpiano::locale` 命名空间函数管理语言激活，不是 `LocaleManager` 类：

```text
[用户选择语言 zh-CN] ──► devpiano::locale::activate(Language::zhCN)
    │
    ├── 1. Primary 层：从 BinaryData::zh_CN_loc (编译期内嵌) 构建 LocalisedStrings
    ├── 2. Secondary 层（可选）：按上述顺序探测外部 .loc 并挂载为 Fallback
    └── 3. 生效：juce::LocalisedStrings::setCurrentMappings(zh.release())
```

### 3.1 语言枚举与代码映射

- `Language::en` ↔ `"en"`（English）
- `Language::zhCN` ↔ `"zh-CN"`（简体中文）

### 3.2 语言持久化

- 用户选择的语言代码保存于 `SettingsModel::languageCode` 字段中；
- `SettingsStore` 加载配置后，`MainComponent` 构造流程调用 `applyLanguage(languageCode)` 激活 locale 并刷新当前主界面；`AppStateBuilder` 只组装状态快照，不执行语言激活。

---

## 4. 语言包文件规范（`zh_CN.loc`）

`source/Locale/zh_CN.loc` 遵循 JUCE 标准 `LocalisedStrings` 格式：

以下是内嵌词条格式的节选，不是完整覆盖清单；上方 `{0}` 示例对应当前生产调用和语言包键：

```text
language: Chinese (Simplified)
countries: cn

"Record" = "录音"
"Stop" = "停止"
"Preset Name:" = "预设名称："
"Delete Preset" = "删除演奏预设"
"Cancel" = "取消"
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
   - 在 `languageDisplayName()` 中添加原生显示名；C++ 使用显式 Unicode 码点或 UTF-8 字节转义，不写裸多字节字符串字面量。
4. **运行三闸门验证**并提交。

---

## 6. 确定性测试与回归清单

| 验证项 | 预期行为 | 状态 |
|---|---|:---:|
| **冷启动语言恢复** | 持久化语言标识后重启，生产组件通过该 locale 查询词条；未翻译键回退英文，不钉完整译文或要求动态诊断全部翻译 | [x] 机制回归；历史界面手测保留 |
| **运行时即时双向切换** | 在设置窗口中从中文切到英文，再切回中文，界面即时更新无撕裂 | [x] 已通过 |
| **模态弹窗国际化** | 新建弹窗按当前语言查询标题、标签和按钮；所有已打开窗口即时重译未单独认证，完整消息迁移已落地 | [x] 已通过 |
| **脱离源码独立运行** | 将 `DevPiano.exe` 移动至独立空白目录，内嵌基础中文词条依然正常显示，不依赖外部 assets | [x] 已通过 |
| **JIVE 语义标题联动刷新** | 切换语言后通过 ViewHost 刷新静态/动态语义标题；自动化验证 locale 激活、键回退和组件机制，不断言可润色的中文译文 | [x] 机制回归 |
| **完整消息与参数原样** | 整句模板、缺键/回退、参数换位及 `{0}` / `%1` 等合法内容保持，真实窗口确认/取消与长名称排版不回归 | [x] 已通过 |

长名称、显式换行及中英混排的弹窗尺寸和已有直接窗口验证范围见 [统一底部操作区](declarative-ui-and-theming.md#33-内容定尺与统一底部操作区)。窗口消费者按所选 locale 新建界面；已有状态提示不会因语言切换重新生成，已打开原生弹窗也不在全量即时重译保证内。内容缩放验证不等于完整物理 DPI／IME 矩阵认证。

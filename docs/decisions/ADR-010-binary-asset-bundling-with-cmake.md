# ADR 010: 采用 CMake BinaryData 静态打包关键资产，消除外部路径依赖

## 状态

已采用（Phase 11 确立样式表与 Token 嵌入，Phase 15 完善多语言嵌入）。

## 背景

devpiano 包含多项核心运行时静态资产：
1. JIVE 声明式样式表（`style_sheets.json`）；
2. 全局设计系统 Token（`design_tokens.json`）；
3. 中文本地化语言包（`zh_CN.loc`）。

若将上述资产作为外部独立文件存放在文件系统中并通过相对/绝对路径在运行时动态读取，存在以下严重隐患：
- **分发与便携性脆弱**：用户移动单可执行文件、脱离源码目录运行或绿色分发时，一旦找不到外部 JSON/LOC 文件，会导致界面样式丢失、空白或回退到英文。
- **IO 失败与损坏风险**：启动阶段读取文件可能因权限、路径解析、字符编码问题发生异常。
- **跨平台路径差异**：WSL 与 Windows 镜像环境下相对路径锚点不同。

## 决策

1. **利用 JUCE 原生 CMake 工具 `juce_add_binary_data` 打包核心资产**：
   - 将 `source/UI/jive/design_tokens.json`、`source/UI/jive/style_sheets.json`、`source/Locale/zh_CN.loc` 注册为构建期二进制静态库 `devpiano_binary_data`。
2. **建立只读编译期内嵌 + 外部可选回退的加载策略**：
   - `StyleCatalog` / `DesignTokens` / `LocaleManager` 启动时直接读取 `BinaryData::style_sheets_json`、`BinaryData::design_tokens_json`、`BinaryData::zh_CN_loc`。
   - 保证单可执行文件（`DevPiano.exe`）在任何纯净无依赖环境下均 100% 具备完整的默认主题、字号、布局规则与中文支持。
   - 针对开发调试（热重载），保留通过文件监听器（`Ctrl+R`）从源文件加载最新修改的能力。

## 原因

- **单一分发**：发布产物无需额外携带零散配置文件，单文件即开即用。
- **零 IO 故障**：启动路径零文件系统读取，消除冷启动 IO 瓶颈与加载失败异常。
- **版本一致性**：静态资产与代码提交天然原子同步，避免旧版本资产与新代码不匹配。

## 影响

- 修改静态资产后需重新编译生成二进制数据（开发期可通过 hot-reload 绕过）。
- 生成的可执行文件尺寸增加极小（数 KB 级别），收益显著。

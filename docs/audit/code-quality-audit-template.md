# devpiano 代码质量审计报告 · <audit-date>

> 目标：对 `source/` 目录做一次全面、可复核的代码质量审计，覆盖架构、安全、资源、可维护性、测试与工程化。
>
> 使用规则：
>
> 1. **问题总表是唯一状态源**：第 8 章为 finding registry；首页、路线图、结论必须与第 8 章一致。
> 2. **Closed 必须有证据**：至少填写代码/测试/文档/命令之一；缺失项需说明原因。
> 3. **Deferred / Mitigated 必须可追踪**：必须写明风险接受原因、重开触发条件和复审时间。
> 4. **复审只追加不覆盖**：复审记录写入第 7 章，并同步更新第 8 章状态。
> 5. **状态枚举固定**：`Open / In Progress / Mitigated / Deferred / Closed`。

---

## 0. 审计看板

### 0.1 基本信息

| 字段 | 值 |
| --- | --- |
| 项目 | devpiano |
| 审计范围 | `source/` （含 14 个子模块 + tests/） |
| 审计日期 | `<YYYY-MM-DD>` |
| 审计基线 | `<branch / tag / commit>` |
| 审计人 | `<name>` |
| 复审状态 | `Initial` |

### 0.2 风险与状态汇总

| Priority | Total | Open | In Progress | Mitigated | Deferred | Closed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P0 | 0 | 0 | 0 | 0 | 0 | 0 |
| P1 | 0 | 0 | 0 | 0 | 0 | 0 |
| P2 | 0 | 0 | 0 | 0 | 0 | 0 |
| P3 | 0 | 0 | 0 | 0 | 0 | 0 |
| **合计** | 0 | 0 | 0 | 0 | 0 | 0 |

### 0.3 关键结论

- 总体评级：`<A / B / C / D>`
- 当前是否适合继续新增功能：`<Yes / No / Conditional>`
- 当前是否建议优先重构：`<Yes / No / Conditional>`
- 最大风险：`<one-line summary>`
- 下一步最高优先级：`<one-line action>`

### 0.4 Top Findings

| ID | Priority | Status | 标题 | 当前结论 |
| --- | --- | --- | --- | --- |
| `<ARCH-001>` | `<P0-P3>` | `<Status>` | `<title>` | `<summary>` |

---

## 1. 审计范围与方法

### 1.1 审计范围

审计 `source/` 下全部 14 个业务子模块：

| 模块 | 路径 | 职责 |
| --- | --- | --- |
| 应用入口 | `source/Main.cpp` | JUCEApplication 启动，创建主窗口 |
| 主装配层 | `source/MainComponent.*` | 装配 UI 子组件，初始化音频/MIDI/插件/设置，顶层协调 |
| UI | `source/UI/` | 虚拟键盘、控件面板、插件面板、头部面板、设置窗口 |
| Core | `source/Core/` | 数据类型定义（KeyMapTypes、AppState、KeyboardTypes、ChannelMatrix、MidiTypes） |
| Audio | `source/Audio/` | 音频引擎、设备诊断 |
| Plugin | `source/Plugin/` | VST3 插件扫描/加载/卸载/editor，流程编排 |
| Input | `source/Input/` | 电脑键盘到 MIDI 映射 |
| Midi | `source/Midi/` | MIDI 通道矩阵路由 |
| Recording | `source/Recording/` | 录制/回放引擎、MIDI 导入/导出、WAV 导出、离线渲染、会话控制器 |
| Layout | `source/Layout/` | Performance Preset CRUD、文件选择 |
| Settings | `source/Settings/` | 设置持久化、序列化、设置窗口管理 |
| Export | `source/Export/` | WAV 导出任务、导出流程支持 |
| Diagnostics | `source/Diagnostics/` | Logger 封装、MIDI trace |
| Locale | `source/Locale/` | 中文本地化、LocaleManager |
| tests | `source/tests/` | KeyMapTypesTest、MidiFileImporterTest、TestRunner |

不包括：

- `JUCE/` 子模块（禁止修改，不在审计范围）
- `scripts/`、`docs/`、构建脚本与配置文件
- 第三方依赖（JUCE 框架本身）

### 1.2 审计输入

| 类型 | 路径 / 命令 |
| --- | --- |
| 代码 | `source/**/*.cpp` `source/**/*.h` |
| 测试 | `source/tests/KeyMapTypesTest.cpp` `source/tests/MidiFileImporterTest.cpp` |
| 架构文档 | `docs/reference/architecture.md` |
| 项目定位 | `docs/reference/project-scope.md` |
| 路线图 | `docs/roadmap/roadmap.md` |
| 构建系统 | `CMakeLists.txt` |
| 构建验证 | `./scripts/dev.sh wsl-build` |
| 测试验证 | `./scripts/dev.sh test` |
| 格式化检查 | `./scripts/dev.sh format --check` |
| 静态分析 | `.clang-tidy`（bugprone/performance/readability/modernize）|
| LSP diagnostics | 通过 clangd 获取实时诊断 |

### 1.3 严重级别定义

| Priority | 定义 | 期望处理 |
| --- | --- | --- |
| P0 | 崩溃、数据损坏、音频毛刺/无声、内存泄漏、线程安全缺陷 | 立即修复，阻断开发 |
| P1 | 高概率稳定性/维护性风险，影响核心路径（演奏/录制/插件） | 当前迭代修复 |
| P2 | 中等风险，影响可维护性、模块边界、测试覆盖或协作效率 | 近期排期 |
| P3 | 低风险改进：命名一致性、注释质量、const 正确性、未使用代码 | 持续跟踪或后续优化 |

### 1.4 状态定义

| Status | 定义 |
| --- | --- |
| Open | 已确认问题，尚未开始处理 |
| In Progress | 已进入实现或验证阶段 |
| Mitigated | 已有缓解措施，但未完全根除 |
| Deferred | 明确暂缓，并记录风险接受原因 |
| Closed | 已完成修复/验证/文档同步，证据可追踪 |

---

## 2. 项目画像

### 2.1 项目类型与核心能力

- 项目类型：**桌面 GUI 应用**（JUCE 框架，VST3 插件宿主，独立可执行文件）
- 核心能力：
  1. 电脑键盘触发 MIDI note → VST3 插件发声
  2. VST3 插件扫描 / 加载 / 卸载 / editor 窗口
  3. 可配置键位映射 + Performance Preset 系统
  4. 16 通道 MIDI 矩阵路由
  5. 录制 / 回放 / MIDI 导入导出 / WAV 离线渲染
  6. 逐键个性化（颜色 + 标签）+ 调号系统
  7. 中文本地化（运行时切换）

### 2.2 技术栈与运行环境

| 类别 | 当前值 |
| --- | --- |
| 语言 | C++20 |
| 框架 | JUCE（git submodule） |
| 构建系统 | CMake + Ninja（WSL/Clang）；MSVC（Windows 验证） |
| 测试框架 | JUCE UnitTest（`devpiano_tests` 目标） |
| 格式化 | clang-format-21（WebKit 基，120 列，Attach 大括号） |
| 静态分析 | clang-tidy（bugprone/performance/readability/modernize） |
| 音频后端 | JUCE `AudioDeviceManager` |
| 插件格式 | VST3（主路径），通过 JUCE `AudioPluginFormatManager` |
| 数据格式 | JSON（Preset `.devpiano.preset`、录制 `.devpiano`、设置） |
| 开发环境 | WSL（编辑 + compile_commands.json）+ Windows/MSVC（构建验证 + 运行测试） |

### 2.3 目录与模块边界

```text
source/
├── Main.cpp                  # 应用入口
├── MainComponent.cpp/.h      # 主装配层（~765 行，已从 1587 行大幅瘦身）
├── UI/                       # 13 文件：虚拟键盘、控件面板、插件面板、头部、设置 UI
├── Core/                     # 7 文件：数据类型、状态模型
├── Audio/                    # 3 文件：音频引擎、设备诊断
├── Plugin/                   # 6 文件：插件扫描/加载/卸载/editor、流程编排
├── Input/                    # 2 文件：键盘→MIDI 映射
├── Midi/                     # 2 文件：通道矩阵路由
├── Recording/                # 12 文件：录制/回放引擎、MIDI 导入/导出、WAV、离线渲染、会话控制
├── Layout/                   # 4 文件：Performance Preset CRUD
├── Settings/                 # 8 文件：设置持久化、序列化、窗口管理
├── Export/                   # 5 文件：WAV 导出任务、流程支持
├── Diagnostics/              # 5 文件：Logger、MIDI trace
├── Locale/                   # 2 文件：中文本地化
└── tests/                    # 3 文件：单元测试
```

边界判断：

- 清晰边界：`Core/`（纯数据，零依赖）、`Input/`（单向键盘→MIDI）、`Midi/`（独立矩阵路由）、`Export/`（独立导出任务）
- 模糊边界：`MainComponent` 与各 FlowSupport / Controller 的职责切分是否彻底；`Recording/` 模块内部 12 文件的内聚性
- 高复杂度热点：`MainComponent.cpp`、`RecordingSessionController.cpp`（24KB）、`CustomKeyboard.cpp`（17.6KB）、`ControlsPanel.cpp`（14.2KB）

---

## 3. 分领域审计结果

> 本章只记录分析结论与证据摘要；具体问题必须进入第 8 章问题总表。

### 3.1 架构与模块边界

评估项：

- `MainComponent` 是否仍然承担了不属于装配层的逻辑
- FlowSupport / Controller 拆分是否彻底，是否存在循环依赖或隐性耦合
- 头文件依赖图是否合理（`#include` 深度、传递包含、前向声明使用）
- `Core/` 类型是否真正零业务逻辑、零 JUCE GUI 依赖

- 评级：`<A/B/C/D>`
- 结论：`<summary>`
- 关联问题：`<ARCH-XXX>`

### 3.2 代码质量与可维护性

评估项：

- RAII 与资源生命周期管理（`std::unique_ptr`、JUCE `OwnedArray`、文件句柄、插件实例）
- const 正确性（函数参数、成员函数、局部变量）
- 命名一致性（与 `docs/reference/architecture.md` 中约定是否一致）
- 注释质量（关键路径是否有意图说明，是否存在过期注释）
- 死代码 / 未使用函数 / 遗留 TODO
- 重复代码模式

- 评级：`<A/B/C/D>`
- 结论：`<summary>`
- 关联问题：`<QUAL-XXX>`

### 3.3 线程安全与并发

评估项：

- JUCE `MessageManager` / `MessageThread` 正确使用：UI 操作是否仅在消息线程
- 音频回调（`AudioEngine::audioDeviceIOCallback`）是否实时安全（无锁、无分配、无 I/O）
- `std::atomic` / `CriticalSection` 使用是否正确
- 插件回调线程与 UI 线程之间的数据竞争风险

- 评级：`<A/B/C/D>`
- 结论：`<summary>`
- 关联问题：`<SEC-XXX / ARCH-XXX>`

### 3.4 安全边界

检查项：

| 检查项 | 评估 |
| --- | --- |
| 文件系統边界：文件读写路径校验（Preset 加载、MIDI 导入、设置文件） | `<Pass/Fail/Partial>` |
| 插件加载安全：DLL/so 加载前校验、路径规范化 | `<Pass/Fail/Partial>` |
| 用户输入消毒：键位绑定配置解析、JSON 解析健壮性 | `<Pass/Fail/Partial>` |
| 缓冲区溢出：MIDI 数据数组访问、键盘状态数组边界 | `<Pass/Fail/Partial>` |
| 数值安全：类型转换、整数溢出、浮点精度 | `<Pass/Fail/Partial>` |

- 关联问题：`<SEC-XXX>`

### 3.5 资源与性能

评估项：

- 实时音频路径的内存分配（`malloc/new` 在 audio callback 中）
- 插件实例生命周期管理（加载/卸载泄漏、editor 窗口泄漏）
- 数组/容器默认大小与增长策略（`std::vector`、`std::array`、`juce::Array`）
- MIDI 事件缓冲区上限
- 大文件处理（MIDI 文件导入、WAV 导出）的内存峰值

- 评级：`<A/B/C/D>`
- 结论：`<summary>`
- 关联问题：`<RES-XXX / PERF-XXX>`

### 3.6 错误处理与可观测性

评估项：

- 异常安全性 vs JUCE 的无异常约定
- 错误传播路径：返回值 → Logger → 用户通知 是否完整
- `DevPianoLogger` 使用覆盖率（是否存在散落 `std::cout` / `DBG()`）
- 静默失败点（忽略返回值、吞异常、空 catch）

- 评级：`<A/B/C/D>`
- 结论：`<summary>`
- 关联问题：`<ERR-XXX / OBS-XXX>`

### 3.7 测试体系

评估项：

- 现有测试覆盖的核心行为（`KeyMapTypesTest`：键位布局；`MidiFileImporterTest`：MIDI 导入）
- 缺少测试的关键模块（音频引擎、录制引擎、插件宿主、键盘映射运行时）
- 测试可维护性（辅助函数复用、fixture 管理、magic number）
- 测试是否独立（不依赖全局状态、不依赖音频设备、不依赖文件系统副作用）

- 评级：`<A/B/C/D>`
- 结论：`<summary>`
- 关联问题：`<TEST-XXX>`

### 3.8 文档与配置契约

评估项：

- `docs/reference/architecture.md` 与源码模块拆分一致性
- 配置默认值漂移（`SettingsModel` 默认值与 `AppState` 初始值是否一致）
- 头文件注释与实现是否同步

- 评级：`<A/B/C/D>`
- 结论：`<summary>`
- 关联问题：`<DOC-XXX>`

### 3.9 工程化与构建

评估项：

- CMakeLists.txt 源文件列表完整性（是否存在未参与构建的孤立文件）
- clang-tidy 诊断清零状态
- clang-format 合规性
- 编译器警告清零状态（`-Wall -Wextra`）
- Debug / Release 构建一致性

- 评级：`<A/B/C/D>`
- 结论：`<summary>`
- 关联问题：`<ENG-XXX>`

---

## 4. 验证记录

### 4.1 命令执行结果

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `./scripts/dev.sh wsl-build` | `<Pass/Fail>` | Debug 构建 |
| `./scripts/dev.sh test` | `<Pass/Fail>` | 单元测试 |
| `./scripts/dev.sh format --check` | `<Pass/Fail>` | 格式合规 |
| `clang-tidy -p build-wsl-clang source/**/*.cpp` | `<Pass/Fail/Not Run>` | 静态分析 |

### 4.2 文件统计

| 指标 | 值 |
| --- | --- |
| 源文件总数（`.cpp`） | `<count>` |
| 头文件总数（`.h`） | `<count>` |
| 总代码行数 | `<count>` |
| 最大文件 | `<path>` (`<lines>` lines) |

### 4.3 未执行验证说明

- `<command>`：`<reason>`

---

## 5. 修复路线图

### 5.1 立即处理（P0）

- [ ] `<ID>`：`<action>`

### 5.2 当前迭代处理（P1）

- [ ] `<ID>`：`<action>`

### 5.3 近期排期（P2）

- [ ] `<ID>`：`<action>`

### 5.4 后续优化（P3）

- [ ] `<ID>`：`<action>`

---

## 6. 最终结论

### 6.1 当前判断

`<short overall assessment>`

### 6.2 是否建议继续新增功能

`<Yes / No / Conditional>`：`<reason>`

### 6.3 是否建议先重构 / 补测试 / 补文档

- 重构：`<Yes / No / Conditional>`：`<reason>`
- 补测试：`<Yes / No / Conditional>`：`<reason>`
- 补文档：`<Yes / No / Conditional>`：`<reason>`

### 6.4 下一步三件事

1. `<action>`
2. `<action>`
3. `<action>`

---

## 7. 复审记录

> 每次复审追加一个小节，不覆盖旧记录。复审后必须同步更新第 0 章汇总与第 8 章问题总表。

### 7.1 复审（YYYY-MM-DD）

- 复审基线：`<commit>`
- 已关闭问题：`<IDs>`
- 状态变化：`<IDs + old -> new>`
- 新增问题：`<IDs>`
- 验证命令：`<commands + result>`
- 复审结论：`<summary>`

---

## 8. 附录：问题总表（Finding Registry）

> 第 8 章是唯一状态源。新增、关闭、暂缓、缓解任何问题，都必须更新本表。

| ID | 领域 | 问题标题 | Priority | Status | 来源 | 影响摘要 | 证据 | 风险接受原因 | 重开条件 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SEC-001 | 安全 | `<title>` | P0 | Open | 审计 | `<impact>` | `<evidence>` | - | `<trigger>` | `<action>` |
| RES-001 | 资源 | `<title>` | P1 | Open | 审计 | `<impact>` | `<evidence>` | - | `<trigger>` | `<action>` |
| ARCH-001 | 架构 | `<title>` | P1 | Open | 审计 | `<impact>` | `<evidence>` | - | `<trigger>` | `<action>` |
| QUAL-001 | 质量 | `<title>` | P2 | Open | 审计 | `<impact>` | `<evidence>` | - | `<trigger>` | `<action>` |
| TEST-001 | 测试 | `<title>` | P2 | Open | 审计 | `<impact>` | `<evidence>` | - | `<trigger>` | `<action>` |
| DOC-001 | 文档 | `<title>` | P3 | Open | 审计 | `<impact>` | `<evidence>` | - | `<trigger>` | `<action>` |

### ID 命名与领域前缀

| Prefix | 领域 |
| --- | --- |
| `SEC` | 安全（缓冲区、文件路径、插件加载） |
| `RES` | 资源（内存泄漏、句柄泄漏、分配热点） |
| `PERF` | 性能（实时路径分配、容器策略） |
| `ARCH` | 架构（模块边界、依赖方向、职责切分） |
| `QUAL` | 代码质量（命名、const、RAII、死代码、重复） |
| `ERR` | 错误处理（返回值忽略、静默失败、Logger 覆盖） |
| `THR` | 线程安全（消息线程、音频回调、数据竞争） |
| `OBS` | 可观测性（日志完整性、diagnostics 覆盖） |
| `TEST` | 测试（覆盖缺口、测试质量、可维护性） |
| `DOC` | 文档（源码注释、架构文档一致性） |
| `ENG` | 工程化（CMake、clang-tidy、clang-format、警告） |
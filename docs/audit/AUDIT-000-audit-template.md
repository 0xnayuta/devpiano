# devpiano 代码质量审计报告 · <审计日期>

> 目标：对 `source/` 目录做一次全面、可复核的代码质量审计，覆盖架构、安全、资源、可维护性、测试与工程化。
>
> 使用规则：
>
> 1. **问题总表是唯一状态源**：第 8 章为登记表；首页、路线图、结论必须与第 8 章一致。
> 2. **已关闭必须有证据**：至少填写代码/测试/文档/命令之一；缺失项需说明原因。
> 3. **已暂缓 / 已缓解 必须可追踪**：必须写明风险接受原因、重开触发条件和复审时间。
> 4. **复审只追加不覆盖**：复审记录写入第 7 章，并同步更新第 8 章状态。
> 5. **状态枚举固定**：`未处理 / 处理中 / 已缓解 / 已暂缓 / 已关闭`。

---

## 0. 审计看板

### 0.1 基本信息

| 字段 | 值 |
| --- | --- |
| 项目 | devpiano |
| 审计范围 | `source/` （含 14 个子模块 + tests/） |
| 审计日期 | `<YYYY-MM-DD>` |
| 审计基线 | `<分支 / 标签 / 提交>` |
| 审计人 | `<姓名>` |
| 复审状态 | `初次` |

### 0.2 风险与状态汇总

| 优先级 | 合计 | 未处理 | 处理中 | 已缓解 | 已暂缓 | 已关闭 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P0 | 0 | 0 | 0 | 0 | 0 | 0 |
| P1 | 0 | 0 | 0 | 0 | 0 | 0 |
| P2 | 0 | 0 | 0 | 0 | 0 | 0 |
| P3 | 0 | 0 | 0 | 0 | 0 | 0 |
| **合计** | 0 | 0 | 0 | 0 | 0 | 0 |

### 0.3 关键结论

- 总体评级：`<A / B / C / D>`
- 当前是否适合继续新增功能：`<是 / 否 / 有条件>`
- 当前是否建议优先重构：`<是 / 否 / 有条件>`
- 最大风险：`<一句话摘要>`
- 下一步最高优先级：`<一句话行动项>`

### 0.4 重点发现

| ID | 优先级 | 状态 | 标题 | 当前结论 |
| --- | --- | --- | --- | --- |
| `<ARCH-001>` | `<P0-P3>` | `<状态>` | `<标题>` | `<摘要>` |

---

## 1. 审计范围与方法

### 1.1 审计范围

审计 `source/` 下全部 14 个业务子模块 + `tests/`：

| 模块 | 路径 | 文件数 | 职责 |
| --- | --- | ---: | --- |
| 应用入口 | `source/Main.cpp` | `<n>` | JUCEApplication 启动，创建主窗口 |
| 主装配层 | `source/MainComponent.*` | `<n>` | 装配 UI 子组件，初始化音频/MIDI/插件/设置，顶层协调 |
| UI | `source/UI/` | `<n>` | 虚拟键盘、控件面板、插件面板、头部面板、设置窗口 |
| Core | `source/Core/` | `<n>` | 数据类型定义（KeyMapTypes、AppState、KeyboardTypes、ChannelMatrix、MidiTypes） |
| Audio | `source/Audio/` | `<n>` | 音频引擎、设备诊断 |
| Plugin | `source/Plugin/` | `<n>` | VST3 插件扫描/加载/卸载/editor，流程编排 |
| Input | `source/Input/` | `<n>` | 电脑键盘到 MIDI 映射 |
| Midi | `source/Midi/` | `<n>` | MIDI 通道矩阵路由 |
| Recording | `source/Recording/` | `<n>` | 录制/回放引擎、MIDI 导入/导出、WAV 导出、离线渲染、会话控制器 |
| Layout | `source/Layout/` | `<n>` | Performance Preset CRUD、文件选择 |
| Settings | `source/Settings/` | `<n>` | 设置持久化、序列化、设置窗口管理 |
| Export | `source/Export/` | `<n>` | WAV 导出任务、导出流程支持 |
| Diagnostics | `source/Diagnostics/` | `<n>` | Logger 封装、MIDI trace |
| Locale | `source/Locale/` | `<n>` | 中文本地化、LocaleManager |
| tests | `source/tests/` | `<n>` | KeyMapTypesTest、MidiFileImporterTest、TestRunner |

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
| 决策记录 | `docs/decisions/` 全部 ADR（ADR-001~006；决策本体被取代=追加新 ADR，事实性描述被证伪=直接修正原文） |
| 构建系统 | `CMakeLists.txt` |
| 构建验证 | `./scripts/dev.sh wsl-build` |
| 测试验证 | `./scripts/dev.sh test` |
| 格式化检查 | `./scripts/dev.sh format --check` |
| 环境自检 | `./scripts/dev.sh self-check` |
| 静态分析 | `.clang-tidy`（bugprone/performance/readability/modernize）|
| LSP diagnostics | 通过 clangd 获取实时诊断 |

### 1.3 严重级别定义

| 优先级 | 定义 | 期望处理 |
| --- | --- | --- |
| P0 | 崩溃、数据损坏、音频毛刺/无声、内存泄漏、线程安全缺陷 | 立即修复，阻断开发 |
| P1 | 高概率稳定性/维护性风险，影响核心路径（演奏/录制/插件） | 当前迭代修复 |
| P2 | 中等风险，影响可维护性、模块边界、测试覆盖或协作效率 | 近期排期 |
| P3 | 低风险改进：命名一致性、注释质量、const 正确性、未使用代码 | 持续跟踪或后续优化 |

> **ADR 合规映射**：违反 ADR 决策本体 → 按上表级别定级并开 `CMPL` 问题；ADR 事实性描述被证伪 → 直接修正 ADR 原文，不开问题。

### 1.4 状态定义

| 状态 | 定义 |
| --- | --- |
| 未处理 | 已确认问题，尚未开始处理 |
| 处理中 | 已进入实现或验证阶段 |
| 已缓解 | 已有缓解措施，但未完全根除 |
| 已暂缓 | 明确暂缓，并记录风险接受原因 |
| 已关闭 | 已完成修复/验证/文档同步，证据可追踪 |

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
- 结论：`<摘要>`
- 关联问题：`<ARCH-XXX>`

### 3.2 代码质量与可维护性

评估项：

- RAII 与资源生命周期管理（`std::unique_ptr`、JUCE `OwnedArray`、文件句柄、插件实例）
- const 正确性（函数参数、成员函数、局部变量）
- 命名一致性（与 `docs/reference/architecture.md` 中约定是否一致）
- 注释质量（关键路径是否有意图说明，是否存在过期注释；注释遵循简体中文规范）
- 死代码 / 未使用函数 / 遗留 TODO
- 重复代码模式

- 评级：`<A/B/C/D>`
- 结论：`<摘要>`
- 关联问题：`<QUAL-XXX>`

### 3.3 线程安全与并发

评估项：

- JUCE `MessageManager` / `MessageThread` 正确使用：UI 操作是否仅在消息线程
- 音频回调（`AudioEngine::audioDeviceIOCallback`）是否实时安全（无锁、无分配、无 I/O）
- `std::atomic` / `CriticalSection` 使用是否正确
- 插件回调线程与 UI 线程之间的数据竞争风险

- 评级：`<A/B/C/D>`
- 结论：`<摘要>`
- 关联问题：`<THR-XXX / SEC-XXX>`

### 3.4 安全边界

检查项：

| 检查项 | 评估 |
| --- | --- |
| 文件系统边界：文件读写路径校验（Preset 加载、MIDI 导入、设置文件） | `<通过/失败/部分>` |
| 插件加载安全：DLL/so 加载前校验、路径规范化 | `<通过/失败/部分>` |
| 用户输入消毒：键位绑定配置解析 | `<通过/失败/部分>` |
| JSON 解析健壮性：Preset/录制/设置文件损坏或版本不兼容处理 | `<通过/失败/部分>` |
| MIDI 消息有效性：note 0–127、channel 0–15、velocity 钳制 | `<通过/失败/部分>` |
| 缓冲区溢出：MIDI 数据数组访问、键盘状态数组边界 | `<通过/失败/部分>` |
| 数值安全：类型转换、整数溢出、浮点精度 | `<通过/失败/部分>` |

- 关联问题：`<SEC-XXX>`

### 3.5 资源与性能

评估项：

- 实时音频路径的内存分配（`malloc/new` 在 audio callback 中）
- 插件实例生命周期管理（加载/卸载泄漏、editor 窗口泄漏）
- 数组/容器默认大小与增长策略（`std::vector`、`std::array`、`juce::Array`）
- MIDI 事件缓冲区上限
- 大文件处理（MIDI 文件导入、WAV 导出）的内存峰值

- 评级：`<A/B/C/D>`
- 结论：`<摘要>`
- 关联问题：`<RES-XXX / PERF-XXX>`

### 3.6 错误处理与可观测性

评估项：

- 异常安全性 vs JUCE 的无异常约定
- 错误传播路径：返回值 → Logger → 用户通知 是否完整
- `DevPianoLogger` 使用覆盖率（是否存在散落 `std::cout` / `DBG()`）
- 静默失败点（忽略返回值、吞异常、空 catch）

- 评级：`<A/B/C/D>`
- 结论：`<摘要>`
- 关联问题：`<ERR-XXX / OBS-XXX>`

### 3.7 测试体系

评估项：

- 现有测试覆盖的核心行为（`KeyMapTypesTest`：键位布局；`MidiFileImporterTest`：MIDI 导入）
- 缺少测试的关键模块（音频引擎、录制引擎、插件宿主、键盘映射运行时）
- 测试可维护性（辅助函数复用、fixture 管理、magic number）
- 测试是否独立（不依赖全局状态、不依赖音频设备、不依赖文件系统副作用）
- 测试是否接入 `devpiano_tests` 目标并随 `./scripts/dev.sh test` 运行（Windows/MSVC 验证侧同步）

- 评级：`<A/B/C/D>`
- 结论：`<摘要>`
- 关联问题：`<TEST-XXX>`

### 3.8 文档与配置契约

评估项：

- `docs/reference/architecture.md` 与源码模块拆分一致性
- 配置默认值漂移（`SettingsModel` 默认值与 `AppState` 初始值是否一致）
- 头文件注释与实现是否同步
- Locale 表与代码内字符串一致性（中英运行时切换无缺漏）

- 评级：`<A/B/C/D>`
- 结论：`<摘要>`
- 关联问题：`<DOC-XXX>`

### 3.9 工程化与构建

评估项：

- CMakeLists.txt 源文件列表完整性（是否存在未参与构建的孤立文件）
- clang-tidy 诊断清零状态
- clang-format 合规性
- 编译器警告清零状态（`-Wall -Wextra`）
- Debug / Release 构建一致性
- git 纪律：提交规范符合 AGENTS.md §4（Conventional Commits 子集，一个提交一件事）

- 评级：`<A/B/C/D>`
- 结论：`<摘要>`
- 关联问题：`<ENG-XXX>`

### 3.10 ADR 合规审计

> 全面审计的决策合规维度。每个 ADR 逐条核对，**合规状态枚举 `合规 / 部分合规 / 违反`**；违反/部分合规必须开 `CMPL-XXX` 问题并写证据。
> 区分两类处理：**ADR 事实性描述**（数值/状态描述）被证伪 → 直接修正 ADR 原文，不开问题；**实现违反 ADR 决策本体** → 开 `CMPL` 问题。
> ADR 数量随迭代增长，核对表按编号续行。

| ADR | 决策要点（一句话） | 审计证据（可执行检查） | 合规状态 |
| :--- | :--- | :--- | :--- |
| ADR-001 | WSL 主工作树 + Windows 镜像树 + MSVC 验证，Windows 不跨边界长期构建 | `scripts/dev.sh win-build` 走镜像树；`source/` 改动仅发生在 WSL 主工作树 | `<合规/部分合规/违反>` |
| ADR-002 | 旧 FreePiano 源码仅作迁移参考（已废止） | `freepiano-src/` 已移除，无旧源码引用残留 | `<合规/部分合规/违反>` |
| ADR-003 | `PluginFlowSupport` 保持纯函数命名空间，不持成员变量 | 无成员变量；依赖经 callback 或参数显式注入 | `<合规/部分合规/违反>` |
| ADR-004 | JUCE `AudioDeviceManager` 作为音频设备管理主路径 | 无旧 WASAPI/ASIO/DirectSound 原生后端残留（grep 零命中） | `<合规/部分合规/违反>` |
| ADR-005 | JUCE `AudioPluginFormatManager`/`AudioPluginInstance` 宿主，VST3 主路径 | 插件加载走 format manager；无旧 VST SDK 风格宿主代码 | `<合规/部分合规/违反>` |
| ADR-006 | 移除外部 MIDI 设备支持，聚焦电脑键盘演奏 | 无外 MIDI 输入枚举/处理代码残留 | `<合规/部分合规/违反>` |

- 评级：`<A/B/C/D>`
- 结论：`<摘要>`
- 关联问题：`<CMPL-XXX>`

---

## 4. 验证记录

### 4.1 命令执行结果

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `./scripts/dev.sh wsl-build` | `<通过/失败>` | Debug 构建 |
| `./scripts/dev.sh test` | `<通过/失败>` | 单元测试 |
| `./scripts/dev.sh format --check` | `<通过/失败>` | 格式合规 |
| `clang-tidy -p build-wsl-clang source/**/*.cpp` | `<通过/失败/未执行>` | 静态分析 |

### 4.2 文件统计

| 指标 | 值 |
| --- | --- |
| 源文件总数（`.cpp`） | `<数量>` |
| 头文件总数（`.h`） | `<数量>` |
| 总代码行数 | `<数量>` |
| 测试用例数 | `<数量>` |
| 最大文件 | `<路径>` (`<行数>` 行) |

### 4.3 未执行验证说明

- `<命令>`：`<原因>`

---

## 5. 修复路线图

### 5.1 立即处理（P0）

- [ ] `<ID>`：`<下一步>`

### 5.2 当前迭代处理（P1）

- [ ] `<ID>`：`<下一步>`

### 5.3 近期排期（P2）

- [ ] `<ID>`：`<下一步>`

### 5.4 后续优化（P3）

- [ ] `<ID>`：`<下一步>`

---

## 6. 最终结论

### 6.1 当前判断

`<总体评估>`

### 6.2 是否建议继续新增功能

`<是 / 否 / 有条件>`：`<原因>`

### 6.3 是否建议先重构 / 补测试 / 补文档

- 重构：`<是 / 否 / 有条件>`：`<原因>`
- 补测试：`<是 / 否 / 有条件>`：`<原因>`
- 补文档：`<是 / 否 / 有条件>`：`<原因>`

### 6.4 下一步三件事

1. `<下一步>`
2. `<下一步>`
3. `<下一步>`

---

## 7. 复审记录

> 每次复审追加一个小节，不覆盖旧记录。复审后必须同步更新第 0 章汇总与第 8 章问题总表。

### 7.1 复审（YYYY-MM-DD）

- 复审基线：`<提交>`
- 已关闭问题：`<编号>`
- 状态变化：`<编号 + 旧 -> 新>`
- 新增问题：`<编号>`
- 验证命令：`<命令 + 结果>`
- 复审结论：`<摘要>`

---

## 8. 附录：问题总表（登记表）

> 第 8 章是唯一状态源。新增、关闭、暂缓、缓解任何问题，都必须更新本表。

| ID | 领域 | 问题标题 | 优先级 | 状态 | 来源 | 影响摘要 | 证据 | 风险接受原因 | 重开条件 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SEC-001 | 安全 | `<标题>` | P0 | 未处理 | 审计 | `<影响摘要>` | `<证据>` | - | `<重开条件>` | `<下一步>` |
| RES-001 | 资源 | `<标题>` | P1 | 未处理 | 审计 | `<影响摘要>` | `<证据>` | - | `<重开条件>` | `<下一步>` |
| ARCH-001 | 架构 | `<标题>` | P1 | 未处理 | 审计 | `<影响摘要>` | `<证据>` | - | `<重开条件>` | `<下一步>` |
| QUAL-001 | 质量 | `<标题>` | P2 | 未处理 | 审计 | `<影响摘要>` | `<证据>` | - | `<重开条件>` | `<下一步>` |
| TEST-001 | 测试 | `<标题>` | P2 | 未处理 | 审计 | `<影响摘要>` | `<证据>` | - | `<重开条件>` | `<下一步>` |
| DOC-001 | 文档 | `<标题>` | P3 | 未处理 | 审计 | `<影响摘要>` | `<证据>` | - | `<重开条件>` | `<下一步>` |
| CMPL-001 | 决策合规 | `<标题>` | P1 | 未处理 | 审计 | `<影响摘要>` | `<证据>` | - | `<重开条件>` | `<下一步>` |

### ID 命名与领域前缀

| 前缀 | 领域 |
| --- | --- |
| `SEC` | 安全（缓冲区、文件路径、插件加载、MIDI 消息有效性、JSON 解析） |
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
| `CMPL` | 决策合规（ADR 决策被违反/部分遵守；ADR 事实性描述过时属修正原文，不开问题） |
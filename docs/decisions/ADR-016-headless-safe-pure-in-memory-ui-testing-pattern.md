# ADR-016: Headless-Safe 统一纯内存 UI 单元测试模式

## 状态

**已接受（2026-10-06）；全量实施。**

用户已批准并确立本决策为项目长期工程与测试铁律。全仓 49 个测试套件、287,000+ 项断言已 100% 统一遵循纯内存 Headless-safe 模式，废弃任何“Headless 环境下跳过测试”的妥协方案。

## 背景

devpiano 项目采用 **WSL 主工作树 + Windows 镜像树 + Linux Clang / Windows MSVC CI 自动化门禁** 的跨平台架构。在 Linux CI 环境（如 GitHub Actions `ubuntu-latest` 容器）中，Runner 默认处于**纯无头（Headless）状态**——没有物理显示器、没有运行 X Server，亦未配置虚拟帧缓冲（Xvfb）。

在 JUCE 框架的底层机制中：
1. `juce::Component` 及其内部声明式视图（如 `ViewHost`、`JiveDialogContent`）属于纯逻辑与离屏内存对象，完全可在无任何显示器的环境下构建、排版、派发事件与离屏测试；
2. 但 `juce::TopLevelWindow`、`juce::DialogWindow`、`juce::DocumentWindow` 或任何调用 `addToDesktop(true)` 的原生窗口，硬编码强制依赖宿主操作系统的桌面窗口管理器（在 Linux 下即 X11 / Wayland）。一旦在无头环境下实例化此类窗口，JUCE 的 X11 Peer 会因无法连接 X Server 而获得空句柄（`windowH == 0`），随后触发一系列 `jassert(windowH != 0)` 断言失败并在解引用时引发进程段错误（`SEGFAULT`，进程退出码 8）。

此前曾出现过一种妥协思路：在测试开头通过 `if (juce::Desktop::getInstance().isHeadless()) return;` 跳过相关逻辑。但经过综合架构审查，该做法存在严重弊端：
- **CI 门禁失去价值（假绿）**：测试在 CI 关键流水线上被静默跳过，回归覆盖率降为 0；
- **破坏全仓一致性**：全仓既有 49 个测试文件此前无一例外均能在无头环境下 100% 跑满断言，引入环境特例跳过属于反模式；
- **职责混淆**：单元测试的职责是验证领域状态机、组件事件流与回调契约，而非验证 Linux 窗口协议客户端能否正常启动。

同时，若为了运行测试而在 CI 中引入 Xvfb 等虚拟服务，则直接违反了项目《架构原则》§7（零冗余基础设施原则），徒增 CI 维护成本与偶发卡死风险。

因此，亟需确立统一的“Headless-Safe 纯内存 UI 单元测试模式”作为全仓长期规范。

## 决策

### 1. 核心铁律：所有 UI 单元测试必须 100% 纯内存且 Headless-Safe

- **严禁创建系统级桌面窗口**：单元测试中**绝对禁止**调用 `addToDesktop()`，绝对禁止直接或间接实例化 `juce::TopLevelWindow`、`juce::DialogWindow`（硬编码 `addToDesktop=true` 的特化类）等操作系统原生桌面窗口；
- **严禁环境特例早退（No Headless-Skips）**：单元测试中**严禁使用** `if (isHeadless()) return;` 逃避断言。所有测试必须在任何操作系统、无论有无显示器、无论本地还是 CI 均能 100% 无条件全量执行；
- **坚守零冗余基础设施**：不引入 Xvfb、VNC、虚拟显卡驱动等非必要 CI 基础设施，保持 Linux CI 运行极简、极速、无外界干扰。

### 2. 窗口外壳与业务逻辑契约彻底解耦

对于弹窗、对话框、浮层与独立窗口类功能，测试必须将“操作系统窗口外壳”与“内部组件业务契约”解耦：
- **测试 ViewHost / Component 声明式树**：使用 `devpiano::ui::ViewHost` 或具体内容组件在纯内存中装载布局树；
- **测试按钮、键鼠与回调交互**：通过 `host.find<juce::Button>("...")` 或组件方法注入交互，验证业务回调（如 `onComplete`、`onCancel`）的触发与参数；
- **测试生命周期与闭包安全性**：若涉及模态关闭回调（如 `ModalCallbackFunction`），使用 `juce::Component::SafePointer` 在纯内存闭包中模拟“存活分发”与“提前析构防御”两种边界场景，验证状态机不悬挂、不产生野指针；
- **显式泵送消息队列**：凡涉及 `triggerClick()` 或 `callAsync` 等异步消息派发，必须紧跟 `devpiano::test::drainMessages(N)` 确保事件队列完全清空后再进行状态断言。

### 3. 测试边界划分原则

| 测试类型 | 运行环境 | 测试范围与职责 |
| :--- | :--- | :--- |
| **自动化单元测试（devpiano_tests）** | 纯无头环境（CI / 本地） | 领域模型、声明式布局、几何边界（fitToContent）、按钮接线、键盘事件、生命周期闭包。**零 OS 窗口**。 |
| **阶段性端到端验收（Manual Regression）** | 实际物理环境（Windows / 真实桌面） | 操作系统原生标题栏、窗口拖拽、DPI 缩放、输入法焦点、厂商插件窗口托管。 |

## 后果与权衡

### 收益 (Benefits)

1. **绝对环境确定性**：消除了 Linux CI、Docker、WSL 与无显卡服务器之间的环境差异，彻底根除 X11 Peer 异常与段错误；
2. **全仓测试模式统一**：所有 UI 测试保持单一范式，风格一致，易于团队和 Agent 遵循维护；
3. **真实有效的 CI 门禁**：所有用例在 CI 均全量跑满断言，消除了“本地绿、CI 假绿”的虚假安全感；
4. **极速构建与零维护开销**：无需安装、维护和仲裁 Xvfb 虚拟进程，CI 耗时与故障率降至最低。

### 权衡与非目标 (Trade-offs & Non-goals)

- **原生操作系统窗口外观不在单元测试覆盖范围**：诸如 Windows DWM 窗口阴影、Linux X11 窗口重绘请求等底层行为，由阶段性手工回归覆盖，不纳入自动化单测；
- **弹窗启动器需保持良好的可测试解耦**：所有弹窗组件应优先暴露布局生成器（`make...Layout`）或纯内存内容组件，便于离屏装配与测试。

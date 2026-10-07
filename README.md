<p align="center">
  <a href="https://github.com/0xnayuta/devpiano"><img src="assets/branding/logo-horizontal-dark.svg" alt="devpiano" width="480"></a>
</p>

<p align="center">
  <b>全物理建模声学钢琴 · 现代电脑键盘演奏应用 · VST3 插件宿主</b><br>
  中文 | <a href="README_en.md">English</a>
</p>
devpiano 是一款基于 JUCE 9.0.1 框架的现代电脑键盘钢琴应用，聚焦电脑软件键盘演奏、高保真物理建模音源与 MIDI 文件处理。

应用内置自主研发、覆盖 **7 大声学子系统** 的纯 C++ 物理建模钢琴音源（`PianoSynthVoice`），同时提供 **VST3 插件宿主** 支持，结合标准 88 键虚拟键盘、16 通道 MIDI 路由矩阵、内生声明式 UI 运行时以及完整的演奏录制、回放、持久化与离线音频渲染工作流。

项目定位、核心能力与明确非目标详见 [`docs/reference/project-scope.md`](docs/reference/project-scope.md)。

---

## 核心特性矩阵

```text
                 ┌─────────────────────────────────────────────────┐
                 │              devpiano Architecture              │
                 └────────────────────────┬────────────────────────┘
                                          │
           ┌──────────────────────────────┼──────────────────────────────┐
           ▼                              ▼                              ▼
┌─────────────────────┐        ┌─────────────────────┐        ┌─────────────────────┐
│   发声与插件引擎    │        │   输入与路由矩阵    │        │   录制回放与渲染    │
│ 物理钢琴+VST3插件  │        │ 键盘分组+和弦/力度 │        │ 录制回放+A-B循环   │
│  乐器端点+节拍器   │        │   16通道矩阵+调号   │        │  异步后台WAV导出   │
└─────────────────────┘        └─────────────────────┘        └─────────────────────┘
```

### 🎹 高保真物理建模钢琴音源（Physical Modeling Piano Engine）

- **7 大声学物理子系统**：覆盖琴槌（Hammer）、琴弦（String）、琴桥（Bridge）、音板（Soundboard）、琴体（Cabinet）、空气（Air）与空间（Room）；
- **88 键连续物理参数映射**：基于 Bensa et al. (2003) 与 Steinway B 实测标定，连续插值琴弦刚度 $B$、击弦比 $d/L$、阻尼常数与 1/2/3 弦物理分区（`Piano88KeyTable.h`）；
- **非线性打击、毛毡老化与动力学绽放**：三层毛毡动力学压实、动态接触时间 $T_c$、击弦点几何梳状陷波、3ms 高频瞬态裂音（HF Crack）、琴槌毛毡微老化穿透力（`feltAgeingAmount`）、泛音时间滞后膨胀绽放（Harmonic Blooming）与强击软饱和；
- **真实共鸣与空间声学系统**：16 峰正交云杉木物理音板模态、4.2kHz 云杉木高频粘滞吸收滤波、琴桥立体声展开、同音三弦 Mid-Side 差分展开与非对称拍频、演奏者（Player）与听众（Audience）双视角声相变换（`PerspectiveProcessor`）以及内置纯算法房间混响网络（`RoomReverbEngine`: Studio / Chamber / Concert Hall）；
- **微观机械动作拟真与古典律制**：CC64 全局交感共鸣、延音踏板下踏/抬起机械扫掠呼啸（Whoosh）与共鸣冲击（Resonance Shock）、制音器落木闷击与琴键释放摩擦、离键速度动态 ADSR 阻尼缩放，以及 6 大古典微调律制（`TemperamentEngine`）与 A4 基准基频微调（400.0–480.0 Hz，默认 440.0 Hz；设置、预设与内置实时/离线音源使用同一限幅）。
- **物理发声核心与硬实时契约**：`PianoSynthVoice` 与节拍器全回调零三角函数（0 `std::sin`），8 复音齐奏单核 CPU $\le 0.7\%$；产品自有发声路径零堆分配、零锁；键盘输入经有界 SPSC 队列交换，音符高亮由消息线程刷新，彻底解耦音频线程与 UI；第三方 VST3 插件适配器的框架锁与扩容限制见 [`docs/issues/known-issues.md`](docs/issues/known-issues.md)；支持与内置正弦波（`SineSynthVoice`）切换。

### 🔌 VST3 插件宿主与乐器端点（VST3 Plugin Hosting & Instrument Endpoint）

- **安全生命周期管理**：支持默认及自定义多目录扫描、异步分片扫描（Chunked Scan）、XML 缓存恢复与失败文件追踪；
- **崩溃安全增量持久化（Crash-safe State Persistence）**：扫描中逐插件即时回调落盘，dead-man's pedal 崩溃点记录与黑名单推迟，避免反复卡死；
- **统一乐器端点（Instrument Endpoint）**：内置物理建模音源与 VST3 乐器共享领域端点职责，统一设备准备、实时发声与离线渲染路由；
- **插件加载与 Editor 托管**：按 `PluginDescription` identifier 加载与恢复 VST3 乐器，显示名仅用于展示；实例替换与重扫先关闭 Editor 并停 callback。插件在宿主进程内执行，不承诺隔离厂商崩溃或永久卡死。

### ⌨️ 电脑键盘演奏与 QWERTY 映射看板（Keyboard Performance & Visualizer）

- **5 行 ANSI 物理键盘映射卡片（QwertyComponent）**：在 Controls 与键盘区之间声明式嵌入自适应 QWERTY 看板，击键物理下沉并联动 50fps 荧光余晖动画；支持一键展开/折叠与设置持久化；
- **12-TET 和声色彩投影（Harmony Projection）**：静态呈现微妙和声色彩，击键与 88 键钢琴同频绽放三和弦几何色相；4 种按键着色模式（Classic / Channel / Velocity / Harmony）；
- **轻量键位分组与发音身份恒定（Layout Groups & HeldKeyIdentity）**：单预设支持 4 组键位配置（Group A~D），反引号键（`）或 UI 按钮秒级切换；物理键保留原发音快照，同一输出最后一个持有者释放才关音；回放侧以 FIFO 保存每次起音的最终身份；
- **采样精确切分延音踏板（SustainPolicy & Sync Pedal）**：音频块内部采样点级别调度 $\text{CC64}(0) \to \text{NoteOn} \to \text{CC64}(127)$，消除空格键踩放时的断音空洞，杜绝线程 Sleep；
- **瞬态演奏修饰键（PerformanceModifierState）**：Shift 键瞬态力度拉满（Velocity Boost）、Alt 键瞬态高八度平移（+8va），松开自动回弹，UI 实时展示 HUD 标签；
- **击键动态力度与实时和弦反馈**：可选 `TypingCadenceEstimator` 按击键间隔调整力度，`VelocityHumanizer` 叠加有界、确定性哈希微扰（Shift 力度拉满优先）；基于按下音符识别和弦及转位，在 QWERTY 卡片标题与键盘 HUD 显示，松键后渐隐。
- **稳定按键映射与 88 键虚拟键盘**：基于稳定 key code 路由，消除输入法干扰；标准 88 键虚拟键盘配备局部脏矩形剪裁（`repaintKey()`）与 3 种音符标注（DoReMi / FixedDo / NoteName）。

### 🥁 节拍与跟练工具（Metronome & Practice）

- **采样级节拍器**：`MetronomeProcessor` 在音频块内产生强弱拍，支持 2/4、3/4、4/4、6/8 拍号、40–280 BPM、Tap Tempo 与状态栏节拍反馈；录制前 1–2 小节预备拍在完整时段后的目标下拍开始，不依赖 UI 轮询；
- **MIDI A-B 循环与时间轴**：`TimelineBar` 支持点击/拖动跳转、设置与清除 A/B 标记；`RecordingEngine` 按 Take 时间轴调度循环与播放速度，跳转/回跳时清理当前发声，避免悬挂音。

### 🎛️ 16 通道 MIDI 矩阵与实时移调（16-Channel MIDI Matrix & Transposition）

- **16 通道独立矩阵**：每通道支持独立半音移调、八度偏移、力度覆盖、音色选择、音色库切换与按键跟随（`followKey`）；
- **全局调号控制**：支持 -7..+7 半音全局调号切换，配备 General MIDI (GM) 通道 10 打击乐直通保护。

### 🎙️ 演奏录制、回放与数据持久化（Recording, Playback & Persistence）

- **实时无锁采集**：音频线程无锁采集生成不可变 `RecordingTake` 数据结构，内嵌快照表隔离外部文件增删；
- **多倍速回放控制**：支持 0.5x–2.0x 音频块边界倍速调节、Back 从头回放，以及暂停/恢复、Take-relative 跳转与 A-B 循环；设备采样率变化保留 Take 时间域，跳转前恢复通道状态，末尾 NoteOff 由音频路径交付，同块预设先于音符生效；
- **原生演奏持久化**：仅支持当前 `.devpiano` v3 JSON，使用 JUCE 长度前缀二进制编码及内嵌 `RecordedPreset` 表；同目录临时文件成功写出后事务替换。不迁移旧格式，拒绝加载时保留原文件与当前会话；
- **标准 MIDI 文件支持**：导入 Type 0/1 全轨 MIDI，完整轨/chunk/meta 准入后保留跨轨 Tempo Map 的时间语义；导出 Type 1、960 PPQ 的单轨演奏流，tick 0 写入 120 BPM，不合成曲名、拍号或调号 meta；
- **Performance Preset 预设系统**：仅当前整数 v2、UUID 永久身份、CRUD、F1-F12 切换、独立重命名目标覆盖确认；普通切换保留应用全局调号，录制回放按内嵌声学快照执行当时的移调。当前可选字段缺省不代表历史版本兼容；

### 📦 离线高保真 WAV 导出（Offline WAV Export Pipeline）

- **现代化异步非阻塞渲染**：`WavExportTask` 演进为纯异步工作任务流（`startAsync`），彻底消除消息循环阻塞，端到端经 `InstrumentEndpoint` 统一调度；
- **双引擎参数与事件调度**：内置 Piano / Sine 或独立离线 VST3 实例消费 Take 快照、采样级事件及宿主 Master/混响；内置物理参数不写入厂商音源，不承诺逐比特输出一致。当前非零快照移调存在 [实时/WAV 音高差异](docs/issues/known-issues.md#原生演奏快照移调与-wav-音高不一致)，不能宣称完整渲染同构；
- **进度与协作取消**：显示导出进度，取消后等待实际工作退出再释放资源；失败或提交前取消保留原目标，仅清理任务自有临时文件。厂商永久阻塞时无法保证有限时间内结束。

### 🎨 内生声明式 UI 运行时与设计系统（Declarative UI & Design System）

- **声明式 UI 架构**：主界面、设置面板与弹窗容器使用内生 `source/UI/jive/core/`（ADR-014，外部 JIVE 子模块已退役），以 `ValueTree`、JSON 样式与 Flex/CSS Grid 排版；业务经 `ViewHost` 门面访问组件，Native 自绘组件保留各自几何与绘制职责。
- **现代化暗黑主题**：基于 `DevPianoLookAndFeel` 的旋钮化 ADSR/音量调节、演奏走带与节拍器控件、状态栏 MIDI 活动/节拍反馈、插件名称、音频指标与调号；
- **绿色单文件资产内嵌**：设计 Token（`design_tokens.json`）、样式表（`style_sheets.json`）与中文语言包（`zh_CN.loc`）由 CMake 编译期二进制静态内嵌，单文件绿色分发零外部文件依赖。

### 🌐 运行时国际化（Runtime i18n）

- **中英文即时切换**：基于 JUCE `Translation` 与 `LocaleManager`，支持简体中文与英文运行时无缝即时切换。

---

## 模块分层与代码架构

```text
source/
├── Main.cpp / MainComponent.*     # 应用生命周期、跨平台窗口与主装配协调层
├── Audio/                         # 音频引擎、物理建模、采样级节拍器、SyncPedalProcessor 与 InstrumentEndpoint
├── Input/                         # 稳定键位 MIDI 映射、击键动态力度与 QWERTY 快照生成
├── Midi/                          # 16 通道 MIDI 矩阵路由与实时移调映射
├── Plugin/                        # VST3 插件扫描、加载、生命周期与 Editor 托管
├── Recording/                     # 录制回放、Take-relative A-B 循环/Seek、MIDI I/O 与离线渲染管线
├── Export/                        # WAV 离线导出后台任务与选项构建
├── Layout/                        # Performance Preset 预设数据模型与 CRUD 编排
├── Settings/                      # 设置模型、持久化存储、独立窗口与 JIVE 声明式设置面板
├── UI/                            # 内生声明式 UI、走带时间轴、和弦 HUD、设计 Token 与 Native 组件
├── Locale/                        # 语言管理器与编译期内嵌语言包
├── Diagnostics/                   # 结构化日志系统、MidiTrace 与调试输出
└── Core/                          # 核心数据结构与强类型定义（AppState, KeyMapTypes, MetronomeModel, MusicTheory）
```

---

## 开发工作流（WSL + Windows MSVC 混合环境）

开发环境推荐采用：**WSL 主工作树 + Windows 镜像树 + CMake + Ninja + Windows/MSVC 验证构建**。

### 常用开发命令（`./scripts/dev.sh`）

```bash
# 1. 环境自检
./scripts/dev.sh self-check

# 2. 代码格式化（基于 WebKit 规范）
./scripts/dev.sh format               # 一键格式化 source/ 下所有 .cpp/.h
./scripts/dev.sh format --check       # 检查格式合规（CI 模式）

# 3. 静态检查（仅迭代边界执行全量 clang-tidy）
./scripts/dev.sh tidy --all

# 4. 刷新 WSL 编译数据库（供 clangd/LSP 使用）
./scripts/dev.sh wsl-build --configure-only

# 5. Windows MSVC Debug 验证构建（内置同步）
./scripts/dev.sh win-build

# 6. Windows Debug 单元测试：在镜像树 Developer PowerShell 中运行
#    BUILD_TESTS=ON + ctest；具体命令见 docs/guides/quickstart.md

# 7. Release 构建（仅发布时）
./scripts/dev.sh win-build --release

# 8. 正式发布打包（生成 Windows x64 zip 与 SHA256 校验和）
./scripts/dev.sh package              # 自动提取版本并打包
./scripts/dev.sh package --version 1.0.0
```

### 工程质量三闸门

每次关键修改提交前，必须满足以下三道质量门禁：
1. **代码格式**：`./scripts/dev.sh format --check` 零违规；
2. **单元测试**：在 Windows 镜像树用 `BUILD_TESTS=ON` 构建并以 CTest 运行 `devpiano_tests`，命令见 [`docs/guides/quickstart.md`](docs/guides/quickstart.md)；Linux CI 使用 `./scripts/dev.sh test`，在本地 WSL 调用该脚本会违反镜像验证工作流；
3. **构建验证**：WSL 配置 `wsl-build --configure-only` + Windows 镜像 `./scripts/dev.sh win-build` 编译成功。

---

## 构建产物路径

- **Windows Debug**：`<WIN_MIRROR_DIR>\build-win-msvc\devpiano_artefacts\Debug\DevPiano.exe`
- **Windows Release**：`<WIN_MIRROR_DIR>\build-win-msvc-release\devpiano_artefacts\Release\DevPiano.exe`
- **发布分发包**：`<WIN_MIRROR_DIR>\dist\v<VERSION>\DevPiano-v<VERSION>-win-x64.zip`

---

## 外部子模块（`submodules/`）

项目依赖的第三方框架以 Git Submodule 形式引入，**禁止直接修改子模块中的任何代码**：
- `submodules/JUCE/`：JUCE 跨平台音频/GUI 框架（AGPLv3 / 商业许可）。
> 注：UI 声明式基础设施已按 ADR-014 内化至 `source/UI/jive/` 自主维护，不再作为外部子模块引入。

---

## 文档中心与推荐阅读

完整文档索引见：[`docs/README.md`](docs/README.md)。

- **新开发者上手**：
  - 快速环境恢复：[`docs/guides/quickstart.md`](docs/guides/quickstart.md)
  - 混合工作流详解：[`docs/guides/wsl-windows-msvc-workflow.md`](docs/guides/wsl-windows-msvc-workflow.md)
  - 项目定位与非目标：[`docs/reference/project-scope.md`](docs/reference/project-scope.md)
  - 系统架构与模块设计：[`docs/reference/architecture.md`](docs/reference/architecture.md)
- **核心特性参考**：
  - 物理建模钢琴音源：[`docs/reference/features/builtin-piano-synthesis.md`](docs/reference/features/builtin-piano-synthesis.md)
  - VST3 插件宿主：[`docs/reference/features/plugin-hosting.md`](docs/reference/features/plugin-hosting.md)
  - 键盘映射与 88 键虚拟键盘：[`docs/reference/features/keyboard-mapping.md`](docs/reference/features/keyboard-mapping.md)
  - 16 通道 MIDI 矩阵：[`docs/reference/features/midi-channel-matrix.md`](docs/reference/features/midi-channel-matrix.md)
  - 录制、回放与导出：[`docs/reference/features/recording-playback.md`](docs/reference/features/recording-playback.md)
  - JIVE 声明式 UI 与设计 Token：[`docs/reference/features/declarative-ui-and-theming.md`](docs/reference/features/declarative-ui-and-theming.md)
- **质量与版本**：
  - 路线图与阶段状态：[`docs/roadmap/roadmap.md`](docs/roadmap/roadmap.md)
  - 阶段验收标准：[`docs/reference/acceptance.md`](docs/reference/acceptance.md)
  - 正式发布打包指南：[`docs/guides/release-workflow.md`](docs/guides/release-workflow.md)
  - 已知问题与回归线索：[`docs/issues/known-issues.md`](docs/issues/known-issues.md)

---

## 开源协议

本项目采用 **AGPLv3** 开源协议，第三方依赖与致谢详见 [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md)。

# Phase 29 完成记录：现实物理演奏交互与声学控制

> 归档日期：2026-09-12
> 前序依赖：Phase 28（Devpiano 声明式 UI 基础设施深度治理与接口冻结）
> 实施范围：`source/Audio/`、`source/Input/`、`source/Settings/`、`source/Layout/`、`source/MainComponent*`、`source/tests/`

---

## 1. 目标与背景

在 Phase 28 确立 Devpiano 声明式 UI 基础设施接口冻结公约（UI Infrastructure API Freeze）之后，项目研发重心彻底回归至核心发声链路与现实演奏交互。

此前自主研发的 7 大声学子系统物理建模钢琴（`PianoSynthVoice`）虽然具备了极其精细的琴槌击弦动力学、双阶段弦模态衰减、同音弦微失谐拍频与云杉木音板共鸣，但在演奏交互与空间控制层面仍存在明显缺失：
1. **琴盖声学无法直观交互**：底层已实现的 `LidAcoustics` 滤波器无法通过 UI 切换，用户只能固定在全开（Full Open）模式；
2. **弱音/移位踏板（Una Corda）机理缺失**：缺乏三角钢琴击弦机右移击打毛毡较软部位、三弦敲两弦（Trichord to Bichord）的高频柔化物理声学，且不支持 MIDI CC 67 信号；
3. **触键力度感单调**：电脑键盘与 MIDI 键盘共用单一粗暴的线性映射，无法适应不同按键轴体手感；
4. **声学偏好无法记忆**：琴盖状态、踏板偏好与力度曲线未纳入 `SettingsStore` 与 Performance Preset 序列化。

Phase 29 实现了现实物理演奏交互与声学控制的端到端打通，完成了琴盖 3 态控制、Una Corda 物理拟真、4 种触键力度曲线自适应映射以及全栈配置持久化与预设系统全量联动。

---

## 2. 实施细节 (Phase 29-A ~ 29-E)

### Phase 29-A：琴盖开合度声学交互与 UI 穿透

- **JIVE 声明式 UI 控件接入（遵守 UI Infrastructure Freeze 公约）**：
  - 在 `source/Settings/jive/SettingsLayoutModel.cpp` 中新增 `makeAcousticsSectionTree()`，通过 C++ DSL 声明物理声学与调音卡片（`acoustics-card`）及琴盖位置选择器（`lid-position-combo`），包含 Full Open / Half Stick / Closed 3 态选项；
  - 在 `source/Settings/SettingsComponent.cpp` 中通过 `viewHost.find<juce::ComboBox>("lid-position-combo")` 安全查找控件，实现动态文本注入与值变更单向数据流绑定（`editingState["lidPosition"]`）；
  - 在 `SettingsWindowManager` 中接入 `onDisplaySettingsChanged` 实时向 `audioEngine` 同步琴盖变更。
- **音频引擎与声学内核无缝贯通**：
  - 在 `SettingsModel`、`SettingsStore`（Properties 序列化）与 `PerformancePreset`（`.devpiano.preset` JSON 序列化）中全面打通 `lidPosition`，支持跨会话记忆与向前兼容；
  - 在 `MainComponent` 与 `PresetFlowSupport` 中实现预设加载与音频引擎实时同步，无内存分配、零爆音；
  - 验证运行态切换琴盖时无音频爆音（Click/Pop）、无内存分配、无线程争用。
- **测试与布局金标防线回归**：
  - 新增 `source/tests/LidAcousticsInteractionTest.cpp`，覆盖 AudioEngine 原子状态、PianoSynthVoice 3 态声学响应、演奏中动态切琴盖数值收敛（无 NaN/Inf）、SettingsStore 磁盘存取与 PerformancePreset 向前兼容；
  - 回归 `source/tests/LayoutGoldenTest.cpp` 与 `source/tests/SettingsLayoutModelTest.cpp`，测试套件 100% 绿灯。

### Phase 29-B：弱音/移位踏板物理拟真与状态联动 (Una Corda / Soft Pedal & CC 67)

- **三角钢琴移位（Una Corda）物理声学机理建模**：
  - 在 `PianoSynthVoice.h` 中实现击弦机偏移机理（`softPedalDown` 与柔音深度 `softPedalAmount` $\in [0.0, 1.0]$）：
    - **有效硬度软化与接触时间延长**：击打点移至毛毡侧边较软区域，有效硬度 $H_{\text{eff}} = H \cdot (1.0 - 0.25 \mu)$，接触时间 $t_c$ 延长 $20\%\sim 25\%$，从震源抑制高阶泛音剧烈激发；
    - **中高音区三弦敲两弦（Trichord to Bichord）能量衰减**：三弦组直接击打能量衰减 $\approx -3.1\text{ dB}$，低音单弦/双弦区衰减 $\approx -1.5\text{ dB}$；
    - **高阶泛音柔音耗散加速**：高阶分音衰减率附加动态软化因子 $1.0 + 0.15 \mu \cdot (n / N)$；
    - **琴槌敲击瞬态噪声软化**：$A_{\text{hammer}} = A_{\text{hammer}} \cdot (1.0 - 0.35 \mu)$。
- **MIDI CC 67 与控制器消息解析**：
  - 在 `PianoSynthVoice::controllerMoved` 中拦截 `controllerNumber == 67`（Soft Pedal），精准解析踏板踩下、释放与半踏板（Half-pedaling）连续量。
- **电脑键盘输入与 UI 柔音状态点亮**：
  - 在 `KeyboardMidiMapper` 中增加 `Tab` 键与 `Shift+Space` 触发软踏板支持，引入 `SoftPedalCallback`；
  - 在主界面状态栏（`MainComponentJiveAccessors.cpp`）实时动态点亮 `[UNA CORDA]` / `[UNA CORDA + SUSTAIN]` 状态指示（严格遵守 Unicode 转义铁律，杜绝裸非 ASCII 乱码）。
- **物理建模单测验证**：
  - 新增 `source/tests/UnaCordaAcousticsTest.cpp`，覆盖 trichord 能量衰减、MIDI CC 67 解析、半踏板比例、键盘快捷键防悬挂与音频渲染稳定性，测试 100% 绿灯。

### Phase 29-C：触键力度曲线自适应映射 (Touch Velocity Curves)

- **触键力度传递函数数学建模**：
  - 在 `source/Input/TouchVelocityCurve.h` 中定义 `TouchVelocityCurve` 强类型枚举与 `applyVelocityCurve` 传递函数：
    - `Standard (Linear)`：$v_{\text{out}} = v_{\text{in}}$，标准中性线性响应；
    - `Light (Soft Action / High Sensitivity)`：凸曲线 $v_{\text{out}} = v_{\text{in}}^{0.65}$，轻触即获得饱满发音，适合手劲较小或薄膜键盘；
    - `Heavy (Firm Action / Low Sensitivity)`：凹曲线 $v_{\text{out}} = v_{\text{in}}^{1.60}$，压制低力度，需要明确敲击才能触发强音，适合追求极弱音（pp）细腻控制的机械键盘；
    - `Wide Dynamic (Expressive S-Curve)`：Sigmoid 平滑三次 Hermite 曲线 $3v^2 - 2v^3$，两端平缓、中段递增，放大极弱音与强音的动态反差。
  - 严格保证输入 $v \in [0.0, 1.0] \mapsto [0.0, 1.0]$，端点 $0 \to 0, 1 \to 1$ 严格守恒，无越界与浮点下溢风险。
- **输入管线接入与实时响应**：
  - 在 `KeyboardMidiMapper` 按键触发链路（`triggerBinding`）中无缝注入力度曲线转换；
  - 在 `MainComponent` 与 `SettingsWindowManager` 中接入 `touchVelocityCurve` 实时转发。
- **JIVE 设置界面联动**：
  - 在 `source/Settings/jive/SettingsLayoutModel.cpp` 声学卡片中增加力度曲线下拉框（`touch-curve-combo`）；
  - 在 `SettingsComponent` 中完成控件查找、双语选项注入与单向数据流绑定，实时弹奏即时生效。
- **单测防线构建**：
  - 新增 `source/tests/TouchVelocityCurveTest.cpp`，覆盖数学不变量、单调性、端点守恒、手感凸凹特异性、键盘映射注入、SettingsStore 持久化与 Preset 序列化，测试 100% 绿灯。

### Phase 29-D：声学配置持久化与预设系统全量联动 (Acoustic Persistence & Preset Evolution)

- **`SettingsModel` 与存储层扩展**：
  - 在 `SettingsModel::PerformanceSettingsView` 与主模型中完整纳入 `LidPosition lidPosition`、`TouchVelocityCurve touchVelocityCurve` 与 `bool unaCorda`；
  - 在 `SettingsStore.cpp` 中支持 `pianoLidPosition`、`touchVelocityCurve` 与 `unaCorda` 读写，并包含边界越界自动钳制，确保应用重启后 100% 恢复上一次的声学与演奏偏好。
- **Performance Preset 协议演进与向后兼容**：
  - 扩展 `PerformancePreset.h` 中的 `struct PerformancePreset`，增加 `lidPosition`、`touchVelocityCurve` 与 `unaCorda`；
  - 更新 `PerformancePreset.cpp`：
    - `savePreset()`：在 JSON 输出的 `"acoustics"` 区域完整写入声学、力度曲线与 Una Corda 字段；
    - `loadPreset()`：优先解析 `"acoustics"` 嵌套对象并安全回退顶层平铺字段；若遇到历史老版本预设（缺省声学字段），安全回退至默认值 `fullOpen`、`standard` 与 `false`，保证存量预设向前向后 100% 兼容。
  - 在 `PresetFlowSupport.cpp` 中打通预设提交（`commitPreset`）与捕获（`captureCurrentState`）对声学 3 参数（琴盖开合、力度曲线、软踏板态）的双向联动。
- **端到端持久化与预设流测试**：
  - 新增 `source/tests/AcousticSettingsPersistenceTest.cpp`，覆盖磁盘 Properties XML 读写、非法越界输入保护钳制、`.devpiano.preset` JSON round-trip、老版本预设向后兼容回退、顶层平铺声学字段兼容性与 `KeyboardMidiMapper::setSoftPedalDown` 去重回调机制，测试 100% 绿灯。

### Phase 29-E：声学精调、三闸门闭环与双平台构建验证

- **全量单元测试闭环**：
  - 运行 `./scripts/dev.sh test`，全套 71 个测试套件、44,558 个断言 100% 绿灯通过，0 错误 0 失败。
- **代码风格与静态检查门禁**：
  - `./scripts/dev.sh format --check` 100% 绿灯通过；
  - `./scripts/dev.sh tidy` 静态分析在所有新增与修改源码上 0 错误 0 警告。
- **Windows MSVC 验证与正式构建**：
  - 执行 `./scripts/dev.sh win-build` 完成 Windows MSVC Debug 编译与链接验证，`DevPiano.exe` 链接成功。
- **实机演奏体验与声学表现验收**：
  - 琴盖 3 态在演奏过程中平滑切换，听感的高频通透度与近场反射多级平滑过渡；
  - Una Corda 弱音踏板（CC 67、Shift+Space 或 Tab）点亮状态栏 `[UNA CORDA]`，触发三弦敲两弦高频柔化物理声学；
  - 4 种触键力度曲线在电脑键盘与 MIDI 键盘上呈现鲜明力度手感梯度。

---

## 3. 产物与测试清单

| 类别 | 模块 / 路径 | 核心职责 |
|---|---|---|
| **物理声学内核** | `source/Audio/PianoSynthVoice.h` | 琴盖开合、Una Corda 击弦机移位物理机理、CC 67 拦截与中高频柔音 |
| **手感力度曲线** | `source/Input/TouchVelocityCurve.h` | 4 种无锁实时力度曲线数学传递函数（Linear, Soft, Firm, Wide Dynamic） |
| **输入与映射** | `source/Input/KeyboardMidiMapper.h/.cpp` | 力度曲线注入、Tab / Shift+Space 快捷键拦截、软踏板防悬挂回调 |
| **声明式 UI 控件** | `source/Settings/jive/SettingsLayoutModel.cpp` | 声学与调音卡片、琴盖与力度曲线 ComboBox 声明式排版 |
| **配置与持久化** | `source/Settings/SettingsModel.h`, `SettingsStore.cpp` | 声学 3 参数磁盘 Properties XML 存取与越界保护 |
| **演奏预设协议** | `source/Layout/PerformancePreset.h/.cpp` | `.devpiano.preset` JSON `"acoustics"` 嵌套字段序列化与向后兼容 |
| **单元测试** | `source/tests/LidAcousticsInteractionTest.cpp` | 琴盖开合声学与 UI 交互回归测试（11 个测试用例） |
| **单元测试** | `source/tests/UnaCordaAcousticsTest.cpp` | 弱音踏板物理声学与 MIDI CC 67 控制测试（11 个测试用例） |
| **单元测试** | `source/tests/TouchVelocityCurveTest.cpp` | 触键力度曲线数学不变量与端点守恒测试（12 个测试用例） |
| **单元测试** | `source/tests/AcousticSettingsPersistenceTest.cpp` | 声学配置跨会话持久化与预设迁移测试（15 个测试用例） |

---

## 4. 结论与历史意义

Phase 29 的圆满落地标志着 devpiano 自主研发的 7 大声学子系统物理建模钢琴具备了业界一流水准的**演奏交互直观性**与**手感自适应能力**。琴盖开合、弱音移位与触键曲线构成了乐器整音（Voicing）的核心能力，为后续 Phase 30（历史律制体系）、Phase 31（多视角空间声学）与 Phase 32（机械物理噪声）的纵深演进奠定了坚实稳固的基础。

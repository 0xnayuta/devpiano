# 电脑键盘映射与按键输入系统说明

> 用途：说明 devpiano 的电脑键盘事件捕获（`KeyboardMidiMapper`）、稳定 KeyCode 映射、默认键位布局、输入法防干扰机制与专项测试清单。
> 当前状态：已全量实现并经过充分的鲁棒性回归验证。
> 更新时机：键盘映射算法、默认键位布局、按键防抖或输入法兼容策略发生变化时。

---

## 1. 概述与设计定位

作为一款电脑键盘钢琴应用，**稳定、低延迟、零干扰的键盘输入**是整个系统的基石：

1. **基于稳定 KeyCode 路由**：彻底摒弃依赖字符输入的脆弱模式，统一采用物理键盘扫描码规范化后的 KeyCode，不受 CapsLock 大小写切换影响；
2. **中文输入法（IME）全面防御**：拦截并吸收按键事件，中文输入法处于激活状态下依然能稳定发声，且不弹出候选词输入框；
3. **发音身份恒定与绝对防悬挂（Note-off Identity Preservation）**：按键按下（NoteOn）时以 `HeldKeyIdentity` 锁定发声音高、通道与力度快照；松键（NoteOff）时 100% 依据按下时记录的快照注销。动态切换 Group、移调或松开修饰键，绝不篡改 NoteOff 身份，从数学状态机上彻底杜绝悬挂音；
4. **5 行 QWERTY 键盘映射看板（QwertyComponent）**：在主窗口 Controls 与键盘区之间声明式嵌入 5 行自适应 ANSI 物理键位网格，击键即时物理下沉并具备 50fps 荧光余晖平滑淡出，支持 12-TET 和声色彩投影与一键折叠；
5. **轻量键位分组（Layout Groups）**：单预设支持 4 组（Group A~D）独立移调、八度与通道配置，反引号键（`）或 UI 按钮秒级循环切组；
6. **采样精确切分延音踏板（SustainPolicy::syncPedal）**：音频块内部采样点级别调度 $\text{CC64}(0) \to \text{NoteOn} \to \text{CC64}(127)$，消除空格键踩放时的断音空洞，杜绝线程 Sleep；
7. **瞬态演奏修饰键（PerformanceModifierState）**：Shift 键瞬态力度拉满（Velocity Boost）、Alt 键瞬态高八度平移（+8va），纯事件流变换零全局配置污染，UI 实时展示 HUD 标签；
8. **焦点丢失自动 Panic 清理（区分内/外部切换）**：焦点**离开应用**（如 Alt+Tab 切到其他程序）时，自动释放交互演奏音（电脑键盘 held keys + 虚拟键盘鼠标按住的音符），防止后台一直鸣响；焦点转移到**本进程其他顶层窗口**（插件编辑器、设置窗口）属于应用内部切换，不打断任何演奏；**MIDI 回放不受失焦影响**；
9. **虚拟键盘显示与输入解耦**：虚拟键盘仅作为视觉反馈和鼠标演奏入口，电脑键盘演奏主路径由 `KeyboardMidiMapper` 独占，避免由于焦点切换引起重复触发。

---

## 2. 默认 36 键标准布局设计

默认键盘布局（`makeDefaultKeyboardLayout()`）覆盖美式标准键盘的 4 行共 36 个常用字母与数字键，构成横跨 4 个八度的阶梯式音阶：

```text
[数字行]  1(C6:84) 2(D6:86) 3(E6:88) 4(F6:89) 5(G6:91) 6(A6:93) 7(B6:95) 8(C7:96) 9(D7:98) 0(E7:100)
[ Q 行 ]  Q(C5:72) W(D5:74) E(E5:76) R(F5:77) T(G5:79) Y(A5:81) U(B5:83) I(C6:84) O(D6:86) P(E6:88)
[ A 行 ]  A(C4:60) S(D4:62) D(E4:64) F(F4:65) G(G4:67) H(A4:69) J(B4:71) K(C5:72) L(D5:74)
[ Z 行 ]  Z(C3:48) X(D3:50) C(E3:52) V(F3:53) B(G3:55) N(A3:57) M(B3:59)
```

- **A 行（中央 C 区）**：`A` 对应中央 C（MIDI Note 60），自然向上分布为 C4 ~ D5；
- **Z 行（低音区）**：`Z` 对应 C3（MIDI Note 48），覆盖低音伴奏；
- **Q 行（高音区）**：`Q` 对应 C5（MIDI Note 72），覆盖高音主旋律；
- **数字行（倍高音区）**：`1` 对应 C6（MIDI Note 84），提供超高音华彩。

---

## 3. 按键捕获与事件流向

```text
[物理键盘按下 / 释放]
    │
    ├── 0. 快捷键拦截: F1-F12 预设切换 ──► 路由至 PresetFlowSupport
    ├── 1. 键组切换: 反引号键 (`) ──► 循环切换 activeGroupIndex (0..3)
    ├── 2. 瞬态修饰键: Shift / Alt ──► 更新 PerformanceModifierState (纯事件流变换)
    ├── 3. 延音踏板: Space 键 ──► 依据 SustainPolicy 触发直接踏板或切分挂起标记
    ├── 4. keyCode 规范化: normaliseAlphaNumericKeyCode(key.getKeyCode())
    │
    ▼
KeyboardMidiMapper::handleKeyPressed() / handleKeyStateChanged()
    │
    ├── 5. 查表匹配当前 KeyboardLayout 绑定
    ├── 6. 结合当前 KeyGroup 计算发声音高 (soundingNote) 与通道 (soundingChannel)
    ├── 7. 结合 PerformanceModifierState 应用瞬态力度提升或八度平移
    ├── 8. NoteOn: 存入 HeldKeyIdentity 发音身份快照 (物理码/音高/通道/力度)
    ├── 9. NoteOff: 100% 按 HeldKeyIdentity 快照注销 (杜绝悬挂音)
    │
    ▼
MidiChannelMapper::sendNoteOn() / sendNoteOff() (经 16 通道矩阵变换)
    │
    ▼
AudioEngine::MidiMessageCollector ──► [音频回调线程]
    │
    ├── SyncPedalProcessor (采样精确调度 CC64 切分踏板时序)
    └── 发声处理 (InstrumentEndpoint) + QwertyViewModel / CustomKeyboard 视图刷新
```

---

## 4. QWERTY 演奏看板与和声色彩投影

在 Phase 34-A 中，devpiano 在主界面引入了基于 JIVE 声明式 UI 驱动的 5 行 ANSI 物理键盘映射卡片（`QwertyComponent`）：

1. **5 行物理网格**：涵盖功能行（Esc/F1-F12）、数字行、QWERTY 行、ASDF 行与 ZXCV 行（含 Space 与修饰键）；
2. **动态击键反馈与余晖**：物理按键按下时视觉方块下沉并高亮，与 88 键虚拟钢琴键盘同频联动；松开后由 50fps 定时器执行指数余晖淡出；
3. **12-TET 和声色彩投影（Harmony Projection）**：
   - 基于 `source/Core/MusicTheory.h` 建立的 12-TET 和声色环算法（`pitchClassHarmonyHues`）；
   - 静态按键文本呈现微妙和声色彩提示，击键时与 88 键钢琴键盘同频绽放三和弦几何色相；
4. **HUD 标签与切组指示**：
   - 按住 Shift 键显示 `Shift [BOOST]`，按住 Alt 键显示 `Alt [+8va]`；
   - 顶部胶囊按钮（`qwerty-group-btn`）实时指示当前激活的键位分组（`Group A/B/C/D`）；
5. **一键折叠与持久化**：支持点击标题栏右侧折叠按钮收起/展开，展开状态持久化于 `SettingsModel::qwertyVisualizerExpanded`。

---

## 5. 专项手工与鲁棒性测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **KBD-001** | 单键按下与释放配对 | 依次按下 `A`、`S`、`D`、`F` 并松开，按下时发声，松开时声音立即停止，无悬挂音 | [x] 已通过 |
| **KBD-002** | 多键和弦并发与交错释放 | 同时按下 `A+D+G` 三和弦，先松开 `D`，`A` 与 `G` 保持发声，随后全部松开声音完全停止 | [x] 已通过 |
| **KBD-003** | 长按防重复触发 | 按住 `A` 键保持 5 秒不放，仅触发一次 Note On 并持续发声，不产生高频断续杂音 | [x] 已通过 |
| **KBD-004** | 极速连击响应 | 快速连续敲击 `J` 键 10 次，每次敲击均产生清晰独立的起音与切音，无丢音与卡音 | [x] 已通过 |
| **KBD-005** | 中文输入法防御 | 开启微软拼音/搜狗输入法弹奏 `QWERTY`，正常弹奏发声，不弹出汉字输入框 | [x] 已通过 |
| **KBD-006** | CapsLock / Shift 状态独立 | 打开大写锁定 CapsLock 或按住 Shift 弹奏，键位映射依然 100% 准确生效 | [x] 已通过 |
| **KBD-007** | 窗口失焦自动 Panic | 按住 `A+S+D` 的同时按 Alt+Tab 切换至其他程序窗口：键盘演奏音立即释放（防悬挂）；MIDI 回放不受影响。切换至插件编辑器/设置窗口（应用内部）：键盘演奏与回放均不中断 | [x] 已通过 |
| **KBD-008** | 88 键虚拟键盘点击 | 鼠标左键点击虚拟键盘上的任意黑白键，正常触发对应音符发声并高亮显示 | [x] 已通过 |
| **KBD-009** | 空间键延音踏板 | 按住空格键触发 CC64 延音开启（音符自然延长），松开空格键延音关闭，与琴键释放严格配对，无悬挂 | [x] 已通过 |
| **KBD-010** | 失焦不打断 MIDI 回放 | 导入 MIDI 自动演奏中 Alt+Tab 切到其他程序、或打开/关闭插件编辑器与设置窗口，回放声音无任何中断 | [x] 已通过 |
| **KBD-011** | Layout Group 动态切组与防悬挂 | 按住 `A` 键（Group A 发声），按反引号键（`）切换至 Group B 并松开 `A` 键，声音干净切断，零悬挂音 | [x] 已通过 |
| **KBD-012** | 切分延音踏板（Sync Pedal）连奏 | 开启 `syncPedal` 模式，按住空格键并交替弹奏和弦，音符切换顺滑无断音空洞，踏板切断采样级精准 | [x] 已通过 |
| **KBD-013** | Shift / Alt 瞬态修饰键 | 按住 Shift 击键触发 fortissimo 最大力度；按住 Alt 击键触发高八度音；松开修饰键后再松按键无悬挂 | [x] 已通过 |
| **KBD-014** | QWERTY 和声投影与余晖 | 弹奏三和弦，QWERTY 键盘与 88 键钢琴同频呈现和声几何色相，松键后呈现平滑荧光余晖衰减 | [x] 已通过 |
| **KBD-015** | QWERTY 看板折叠持久化 | 点击折叠按钮收起 QWERTY 看板，重启应用后保持折叠；再次点击展开保持展开 | [x] 已通过 |

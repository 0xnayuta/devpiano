# 电脑键盘映射与按键输入系统说明

> 用途：说明 devpiano 的电脑键盘事件捕获（`KeyboardMidiMapper`）、稳定 KeyCode 映射、默认键位布局、输入法防干扰机制与专项测试清单。
> 验证边界：发音身份、最终投影和输入优先级有直接消费者证据；下方历史手工结果不等于本轮已覆盖全部 IME、DPI、声卡与厂商插件组合。
> 更新时机：键盘映射算法、默认键位布局、按键防抖或输入法兼容策略发生变化时。

---

## 1. 概述与设计定位

作为一款电脑键盘钢琴应用，**稳定、低延迟、零干扰的键盘输入**是整个系统的基石：

1. **基于稳定 KeyCode 路由**：彻底摒弃依赖字符输入的脆弱模式，统一采用物理键盘扫描码规范化后的 KeyCode，不受 CapsLock 大小写切换影响；
2. **中文输入法（IME）全面防御**：拦截并吸收按键事件，中文输入法处于激活状态下依然能稳定发声，且不弹出候选词输入框；
3. **发音身份恒定与重叠持有**：`HeldKeyIdentity` 锁定 Group、modifier 和矩阵变换后的原音高/通道。Q/K 同音及矩阵合并音高的其他持有者仍按住时，首个松键不发 NoteOff；最后释放或失焦才按原身份关音。重复按下已松开的物理键仍可重新起音；录制将最终释放规范化为所有已捕获起音的同采样配对 Off；
4. **5 行 QWERTY 键盘映射看板（QwertyComponent）**：在主窗口 Controls 与键盘区之间声明式嵌入 5 行自适应 ANSI 物理键位网格，击键即时物理下沉并具备 50fps 荧光余晖平滑淡出，支持 12-TET 和声色彩投影与一键折叠；
5. **轻量键位分组（Layout Groups）**：单预设支持 4 组（Group A~D）独立移调、八度与通道配置，反引号键（`）或 UI 按钮秒级循环切组；
6. **采样精确切分延音与柔音踏板**：音频块内部采样点级别调度 $\text{CC64}(0) \to \text{NoteOn} \to \text{CC64}(127)$，消除空格键踩放时的断音空洞，杜绝线程 Sleep；支持 Tab 键或 Shift+Space 组合键触发物理柔音（Soft Pedal / Una Corda），切组或 Panic 自动安全复位；
7. **瞬态演奏修饰键（PerformanceModifierState）**：Shift 对非静音绑定瞬态拉满力度，Alt 瞬态高八度平移（+8va）；静音绑定优先于 Shift、动态力度、微扰及矩阵固定力度。修饰符只变换事件，不改全局配置；
8. **焦点丢失自动 Panic 清理（区分内/外部切换）**：焦点**离开应用**（如 Alt+Tab 切到其他程序）时，自动释放交互演奏音（电脑键盘 held keys + 虚拟键盘鼠标按住的音符），防止后台一直鸣响；焦点转移到**本进程其他顶层窗口**（插件编辑器、设置窗口）属于应用内部切换，不打断任何演奏；**MIDI 回放不受失焦影响**；
9. **双演奏看板与输入解耦**：QWERTY 和虚拟钢琴共同消费映射层的最终投影；配置输入身份与观察到的输出通道分开，连续鼠标点击和 MIDI 回放不会改变后续输入路由；
10. **打字律动力度与人性化微扰（TypingCadenceEstimator & VelocityHumanizer）**：按键击键时间间隔（$\Delta t$）自适应估算演奏力度，高速连击/和弦齐奏（$\le 60\text{ms}$）赋予高动态力度（~122/127），慢速抒情（$\ge 500\text{ms}$）赋予轻柔力度（~76/127），空闲停顿（$> 1.0\text{s}$）重置为基准力度（100/127）。结合 FNV/Murmur 确定性伪随机微扰与触键力度曲线（Touch Velocity Curve），赋予物理键盘真实钢琴般的动态层次；计算结果直接注入发音与录制管线（UI 层不设置多余的数值力度 HUD）；
11. **绑定准入**：仅支持 `keyDown` 的“按下起音、松开释放”；预设显式 `keyUp` 或未知 trigger 拒绝，缺失 trigger 保持原 `keyDown` 默认。拒绝不改已有文件或当前预设。
12. **实时和弦识别与看板徽标（`devpiano::core::detectChord`）**：以当前被按住的键集（`heldKeys` 的实际发声音高）为输入，实时分析和声结构与低音转位，在 QWERTY 看板标题徽标（`qwerty-chord-badge`）与键盘内部 HUD 展示和弦名称；键盘内部 HUD 松键后平滑淡出。

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
    ├── 3. 延音与柔音踏板: Space 键依据 SustainPolicy 触发延音；Tab 或 Shift+Space 触发柔音 (Soft Pedal)
    ├── 4. keyCode 规范化: normaliseAlphaNumericKeyCode(key.getKeyCode())
    │
    ▼
KeyboardMidiMapper::handleKeyPressed() / handleKeyStateChanged()
    │
    ├── 5. 查表匹配当前 KeyboardLayout 绑定
    ├── 6. 结合当前 KeyGroup 计算发声音高 (baseSoundingNote) 与通道 (soundingChannel)
    ├── 7. 打字律动力度估算 (TypingCadenceEstimator): 依击键间隔 Δt 估算 dynamicVelocity
    ├── 8. 确定性力度微扰 (VelocityHumanizer): 结合音高与击键计数器施加确定性抖动
    ├── 9. 瞬态修饰与手感映射:
    │      ├── modifierState.transformPitch(baseSoundingNote) 瞬态八度平移
    │      ├── applyVelocityCurve(jitteredVelocity, touchVelocityCurve) 映射触键手感曲线
    │      └── modifierState.transformVelocity(...) 瞬态力度仲裁 (Shift 强制 1.0f 优先；静音绑定保持 0.0f)
    ├── 10. NoteOn: 经 MidiChannelMapper 变换并存入 HeldKeyIdentity 发音身份快照 (物理码/音高/通道/力度)，清空切分挂起
    ├── 11. NoteOff: 最后持有者按 HeldKeyIdentity 原身份注销
    ▼
MidiChannelMapper::sendNoteOn() (矩阵变换) / sendNoteOff() (直接消费锁定身份)
    │
    ▼
AudioEngine::liveMidiQueue（有界 SPSC）──► [音频回调线程]
    │
    ├── SyncPedalProcessor (采样精确调度 CC64 切分踏板时序)
    └── 发声处理 (InstrumentEndpoint)；消息线程消费同一 QwertyViewModel 的网格与 pianoKeys，
        MidiKeyboardState 只负责实际发音活动的视觉反馈，不作为鼠标配置输入来源
```

---

## 3.1 双看板最终投影与鼠标输入身份

- `KeyboardMidiMapper::createQwertySnapshot()` 在映射层完成 Group、modifier、触键曲线、通道矩阵和 followKey 投影；`MidiChannelMapper::sendNoteOn()` 与投影共用 `applyTransform()`。
- `QwertyViewModel` 同时提供电脑网格和按最终输出音高索引的 `pianoKeys`。两张看板显示同一最终音高/通道；UI 不反查原始布局或二次变换输出。
- 点击保存的矩阵输入音高、通道和力度只经矩阵一次，NoteOff 使用起音返回的最终身份。钢琴着色观察到 Ch11 回放时，随后点击仍按原配置输入路由，而不是从 Ch11 再映射。
- 未绑定琴键的可用输入也由映射层准备；超出 MIDI `0..127` 输入域而没有可用投影的琴键不发送 NoteOn。88 键键床不把范围外输出强行夹回可视区域。
- 同输出音高的绑定标签由映射层合并，钢琴点击/编辑使用列表首个绑定的输入身份。标签、逐键颜色与新绑定编辑沿用配置输入音符索引，Group/modifier/矩阵变化不改写预设数据。
- 快照中的力度是配置、曲线、modifier 与矩阵的静态投影；击键间隔与人性化动态力度仍在真实演奏事件中计算。零力度绑定在物理键盘、钢琴鼠标和 QWERTY 鼠标入口均保持静音。
- 音名采用 `MIDI / 12 - 1` 的科学八度：`0/1/11` 属于八度 `-1`，`12` 为 `C0`；唱名偏移与单音 HUD 共用相同边界。

## 4. QWERTY 演奏看板与和声色彩投影

devpiano 在主界面嵌入基于内生声明式 UI 驱动的 5 行 ANSI 物理键盘映射卡片（`QwertyComponent`）：

1. **5 行物理网格**：依次呈现数字、QWERTY、ASDF、ZXCV 与底部修饰行；底部行显示 Ctrl/Win/Alt 和 Space 踏板，不包含 Esc/F1-F12 功能行；
2. **动态击键反馈与余晖**：物理按键按下时视觉方块下沉并高亮，与 88 键虚拟钢琴键盘同频联动；松开后由 50fps 定时器执行指数余晖淡出；
3. **12-TET 和声色彩投影（Harmony Projection）**：
   - 基于 `source/Core/MusicTheory.h` 建立的 12-TET 和声色环算法（`pitchClassHarmonyHues`）；
   - 静态按键文本呈现微妙和声色彩提示，击键时与 88 键钢琴键盘同频绽放三和弦几何色相；
4. **HUD 标签、实时和弦识别与切组指示**：
   - 按键表面瞬态标签：按住 Shift 键时键位标注 `Shift [BOOST]`，按住 Alt 键时标注 `Alt [+8va]`；
   - 顶部胶囊按钮（`qwerty-group-btn`）：实时指示当前激活的键位分组（`[Group A/B/C/D]`）；
   - 顶部和弦徽标（`qwerty-chord-badge`）：`devpiano::core::detectChord` 根据 `heldKeys` 的发声音高识别和弦（单音、大/小三和弦、增/减三和弦、挂留、七和弦、九和弦、转位等），展示如 `[C]`、`[Am7]`、`[G/B]`；键盘内部和弦 HUD 松键后以定时器渐隐。注：看板不设置独立的数字力度 HUD；
5. **一键折叠与持久化**：支持点击标题栏右侧折叠按钮收起/展开，展开状态持久化于 `SettingsModel::qwertyVisualizerExpanded`；
6. **自适应虚拟钢琴键床**：88 键使用固定比例几何（白键宽 21.5 px、长宽比 6.4:1）；默认 1180 px 窗口完整显示键床，较窄窗口保留完整键床并横向滚动，较宽或较高视口内居中显示；
7. **打字律动力度与触键手感配置**：`SettingsModel` / `SettingsStore` 保存动态力度开关、基准力度偏置（`baseVelocityBias` 范围 -0.30 ~ +0.20）与人性化微扰量（`velocityHumanizeAmount` 0.0 ~ 0.15）；当前界面未提供这些参数的编辑控件。设置窗口提供 Standard / Light / Heavy / Wide Dynamic 触键曲线选择。

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
| **KBD-008** | 88 键虚拟键盘点击与窄窗口滚动 | 默认窗口完整显示 A0–C8；窄窗口横向滚动至两端，并点击黑白键验证发声与高亮 | [ ] 待手工验证 |
| **KBD-009** | 空间键延音踏板 | 按住空格键触发 CC64 延音开启（音符自然延长），松开空格键延音关闭，与琴键释放严格配对，无悬挂 | [x] 已通过 |
| **KBD-010** | 失焦不打断 MIDI 回放 | 导入 MIDI 自动演奏中 Alt+Tab 切到其他程序、或打开/关闭插件编辑器与设置窗口，回放声音无任何中断 | [x] 已通过 |
| **KBD-011** | Layout Group 动态切组与防悬挂 | 按住 `A` 键（Group A 发声），按反引号键（`）切换至 Group B 并松开 `A` 键，声音干净切断，零悬挂音 | [x] 已通过 |
| **KBD-012** | 切分延音踏板（Sync Pedal）连奏 | 开启 `syncPedal` 模式，按住空格键并交替弹奏和弦，音符切换顺滑无断音空洞，踏板切断采样级精准 | [x] 已通过 |
| **KBD-013** | Shift / Alt 瞬态修饰键 | 按住 Shift 击键触发 fortissimo 最大力度；按住 Alt 击键触发高八度音；松开修饰键后再松按键无悬挂 | [x] 已通过 |
| **KBD-014** | QWERTY 和声投影与余晖 | 弹奏三和弦，QWERTY 键盘与 88 键钢琴同频呈现和声几何色相，松键后呈现平滑荧光余晖衰减 | [x] 已通过 |
| **KBD-015** | QWERTY 看板折叠持久化 | 点击折叠按钮收起 QWERTY 看板，重启应用后保持折叠；再次点击展开保持展开 | [x] 已通过 |
| **KBD-016** | 打字律动力度与人性化微扰 | 快速连击（$\le 60\text{ms}$）触发高动态力度，慢速慢弹（$\ge 500\text{ms}$）触发轻柔力度，长暂停（$> 1.0\text{s}$）重置基准力度；Shift Boost 强制 1.0f | [ ] 待手工验证 |
| **KBD-017** | 实时和弦识别 HUD 徽标 | 同时按下 `A+D+G`（C 大三和弦），标题徽标显示 `[C]`；弹奏转位和弦显示斜杠标记（如 `[C/E]`）；释放所有琴键后键盘内部 HUD 渐隐 | [ ] 待手工验证 |
| **KBD-018** | 双看板与最终 MIDI 一致 | Group/Alt/矩阵/followKey 改动后，QWERTY 标签、钢琴绑定位置与实际录得 MIDI 的音高/通道一致；点击不二次变换输出 | [x] Windows 实际窗口/音频捕获验证通过 |
| **KBD-019** | 鼠标路由不受输出反馈污染 | Ch1→Ch2、Ch2→Ch3 下同键重复点击仍从配置输入路由；Ch11 回放后再次点击保持原路由 | [x] Windows 实际回放/鼠标消费者验证通过 |
| **KBD-020** | 静音绑定优先级 | 零力度绑定加 Shift，并将矩阵固定力度设为 127；物理、钢琴鼠标及 QWERTY 点击最终捕获均无 NoteOn | [x] Windows 实际音频/MIDI 捕获验证通过 |
| **KBD-021** | 最低 MIDI 八度 | MIDI 0/1/11/12 的卡片音名、唱名偏移和单音 HUD 使用同一正确八度边界 | [x] 默认回归与实际窗口验证通过 |

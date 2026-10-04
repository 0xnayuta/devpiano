# Performance Preset 预设系统与 CRUD 编排说明

> 用途：说明 devpiano 的 Performance Preset 预设系统、`.devpiano.preset` JSON 格式规范、CRUD 编排流（`PresetFlowSupport`）、F1-F12 快捷键与录制切调集成。
> 当前状态：已全量实现并稳定服务于演奏配置管理。
> 更新时机：预设数据模型、文件格式版本或快捷键调度规则发生变化时。

---

## 1. 概述与设计定位

为了让演奏者能够针对不同曲目快速切换键位映射、音色通道与调号，devpiano 建立了高度解耦的 **Performance Preset 预设系统**：

1. **完整演奏快照**：一份预设完整打包了当前键位绑定（`KeyboardLayout`，含 4 组 `KeyGroup` 键位分组配置与激活组索引）、16 通道矩阵（`ChannelMatrix`）、声学与物理参数（`acoustics`: 琴盖、触键曲线、弱音踏板、律制、基准音高、空间视角、混响类型与干湿比、机械噪声、毛毡老化）、全局调号与移调开关（`keySignature`）、键盘渲染模式以及 128 项逐键个性化标签与颜色；
2. **独立 JSON 文件（`.devpiano.preset`）**：采用规范的 JSON 格式存储于 `DevPiano/Presets/` 目录下，便于用户备份、分享与跨设备导入；
3. **一键 CRUD 与声明式弹窗**：通过 `ControlsPanel` 下拉菜单及 Save As New / Rename / Delete 按钮操作，全面接入 `JiveModalDialog` 声明式弹窗；
4. **F1-F12 极速快捷键**：演奏过程中按下 F1-F12 即可毫秒级无缝切换预设；
5. **录制自动切调**：切换时保存预设永久身份与当时声学快照；回放按采样点执行，不依赖录制后目录排序。

---

## 2. `.devpiano.preset` 文件格式规范（v2）

```json
{
  "version": 2,
  "name": "Pop Piano in D",
  "uuid": "963c9500-38d7-4e10-8025-2e61d4830084",
  "layout": {
    "id": "user.preset.pop-piano-in-d",
    "name": "Pop Piano in D",
    "bindings": [
      {
        "keyCode": 65,
        "displayText": "A",
        "action": {
          "type": "note",
          "trigger": "keyDown",
          "midiNote": 60,
          "midiChannel": 1,
          "velocity": 1.0
        }
      }
    ],
    "groups": [
      { "transposeOffset": 0, "octaveShift": 0, "channel": 0, "name": "A" },
      { "transposeOffset": 0, "octaveShift": 0, "channel": 0, "name": "B" },
      { "transposeOffset": 0, "octaveShift": 0, "channel": 0, "name": "C" },
      { "transposeOffset": 0, "octaveShift": 0, "channel": 0, "name": "D" }
    ],
    "activeGroupIndex": 0
  },
  "channelMatrix": {
    "active": true,
    "channels": [
      {
        "outputChannel": 0,
        "transpose": 2,
        "octaveShift": 0,
        "velocity": 64,
        "program": 0,
        "bankMSB": 0,
        "sustainCC": 64,
        "followKey": true
      }
    ]
  },
  "acoustics": {
    "lidPosition": 0,
    "touchVelocityCurve": 0,
    "unaCorda": false,
    "temperament": "equal",
    "referencePitchA4": 440.0,
    "soundPerspective": "player",
    "reverbSpace": "chamber",
    "reverbWet": 0.0,
    "pedalNoiseLevel": 0.6,
    "feltAgeingAmount": 0.0
  },
  "keyboard": {
    "keySignature": 2,
    "midiTranspose": true,
    "colourMode": 0,
    "noteDisplay": 0,
    "fadeSpeed": 0.92,
    "customKeyLabels": [],
    "customKeyColours": []
  }
}
```

### 2.0 永久身份与迁移

- `uuid` 是文件身份，`name` 是可变显示名称。另存为新预设生成新 UUID；重命名、自动保存和导入保留已有 UUID。
- 无 UUID 的旧 v1 文件按固定命名空间和名称派生稳定 UUID，首次保存写为 v2；v2 缺失/无效身份拒绝，不重新用当前名称生成身份。
- 旧启动设置中的名称仅在唯一匹配时迁移到 UUID；多义名称或重复 UUID 不猜测目标。列表显示名称，运行绑定保存 UUID，磁盘路径仍由当前名称规范化得到。
- 原生演奏另外保存 Take 内的预设和 `AcousticSnapshot`，包含当时的音源类型、Master/ADSR、物理/空间参数及回放移调；不能把预设文件自身的配置子集误写为这些全部运行时字段都已持久化在 `.devpiano.preset`。


键位 `action.trigger` 只接受 `"keyDown"`，缺省仍按原格式默认 `"keyDown"`。显式 `"keyUp"` 或未知值在加载时拒绝，保存也不把非法内存状态静默改写为有效绑定；原文件与当前应用预设保留。

---

### 2.1 声学与物理拟真对象（`acoustics`）

| 属性字段 | 数据类型 | 取值范围与默认值 | 含义与声学作用 |
|---|:---:|:---:|---|
| `lidPosition` | int | 0 (全开) / 1 (半开) / 2 (闭盖) | 琴盖开合度声学传递函数 |
| `touchVelocityCurve`| int | 0 (标准) / 1 (轻触) / 2 (重触) / 3 (宽动态) | 键盘触键力度响应非线性曲线 |
| `unaCorda` | bool | `true` / `false` | 弱音/移位踏板物理拟真（MIDI CC 67 联动） |
| `temperament` | string | `"equal"`, `"just"`, `"pythagorean"`, `"meantone"`, `"werckmeister3"`, `"kirnberger3"` | 古典微调律制选择 |
| `referencePitchA4` | double | 400.0 ~ 480.0 Hz（默认 440.0 Hz） | A4 基准基频换算；与设置、内置实时/离线音源共用限幅 |
| `soundPerspective` | string | `"player"` (演奏者) / `"audience"` (听众) | 立体声空间声像展开视角 |
| `reverbSpace` | string | `"chamber"` (室内乐) / `"concert_hall"` (音乐厅，兼容别名 `"hall"`) / `"studio"` (录音棚) | 房间混响网络预设空间 |
| `reverbWet` | float | 0.0 ~ 1.0（默认 0.0） | 房间混响干湿混合比 |
| `pedalNoiseLevel` | float | 0.0 ~ 1.0（默认 0.6） | 延音踏板扫掠呼啸与共鸣冲击机械动作音量 |
| `feltAgeingAmount` | float | 0.0 ~ 1.0（默认 0.0） | 琴槌羊毛纤维磨损压实老化深度 |

> **向后兼容性保证**：若读取的历史预设缺失 `"acoustics"` 节点或部分声学字段，系统自动填充出厂默认值并支持根节点平铺字段的安全回退解析，且反序列化时对所有枚举与数值实施合法区间 `jlimit` 钳制保护。

### 2.2 键位分组对象（`groups` 与 `activeGroupIndex`）

在 Phase 34-B 中，`layout` 节点引入了多键位分组持久化支持：

| 属性字段 | 数据类型 | 取值范围与默认值 | 含义与作用 |
|---|:---:|:---:|---|
| `transposeOffset` | int | -12 .. +12（默认 0） | 当前键组相对基准音高的半音移调量 |
| `octaveShift` | int | -3 .. +3（默认 0） | 当前键组相对基准音高的八度偏移量（每八度 12 半音） |
| `channel` | int | 0 .. 16（默认 0） | 目标 MIDI 通道覆盖（0 表示继承绑定本身通道，1~16 表示强制覆盖为指定通道） |
| `name` | string | 字符串（默认 `"A"`, `"B"`, `"C"`, `"D"`） | 键组标签显示名称 |
| `activeGroupIndex` | int | 0 .. 3（默认 0） | 当前预设激活的键组索引 |

> **向后兼容性保证**：旧预设无 `groups` 节点时，自动初始化为 4 组默认纯净 KeyGroup（A/B/C/D，偏移均为 0），`activeGroupIndex` 默认回落为 0，完全零破坏兼容既有预设文件。

### 2.3 键盘显示与输入索引

- `keyboard.fadeSpeed` 是每帧指数收缩系数，范围 `0.50 .. 0.99`，默认 `0.92`。设置、预设和显示组件使用同一限幅；旧文件中的 `1` 或更大值钳至 `0.99`，不会保留不收缩的动画端点。
- 绑定音符、逐键标签与颜色仍按配置输入 MIDI 音符保存，不把当前 Group/modifier/矩阵变换后的输出写回预设。两张看板通过映射层投影显示最终身份；在输出位置新建绑定时保存对应输入音符，避免后续再次移调。


---

## 3. 预设生命周期与 CRUD 编排（`PresetFlowSupport`）

### 3.1 内置 Default 预设

- 系统内置出厂默认预设 `[Default]`，作为最底层的基准配置；
- `[Default]` 不允许被重命名或删除（按钮自动置灰）；
- 当用户删除了当前活动预设时，系统自动安全回退至 `[Default]`。

### 3.2 预设操作流

- **自动发现**：启动时自动扫描 `DevPiano/Presets/` 目录下的全部 `.devpiano.preset` 文件并填充下拉列表；
- **Save As New（另存为）**：弹出 `JiveModalDialog::launchSingleInput`，输入名称后保存新文件并立即激活；
- **Rename（重命名）**：名称经 `resolvePresetFile()` 规范化，再用 JUCE `File` 路径身份比较；同一路径（包括 Windows 常规大小写等同路径）只事务更新内容，不删除目标。独立已有目标先显示覆盖确认，取消不写文件。不同路径先准备新内容并暂存源文件，目标提交失败时恢复源文件；不再先覆盖 B 再尝试删除 A。
- **Delete（删除）**：弹出 `JiveModalDialog::launchConfirm` 确认弹窗，确认后删除物理文件并安全切换预设；
- **拖放导入**：直接将 `.devpiano.preset` 拖入主窗口即可自动复制并立即应用。
- **启动恢复与自动保存**：恢复用户 B 与下拉列表选择走同一激活路径；在提交布局前同时设置控制器 `currentPresetId` 和设置 `lastActivePresetId`，立即编辑绑定仍保存到 B。文件身份由激活入口显式传入，不以 `layout.id` 推断；用户预设可以继承内置布局 ID，只有真正内置 Default 才无文件绑定。
- **故障恢复边界**：若文件系统同时阻止目标提交和源文件回滚，保留源备份并在日志记录路径，不删除唯一原始数据；提交成功后若备份清理被外部阻止，同样保留并记录。正常失败/取消场景的字节保留与临时文件清理已直接验证，不宣称断电或强杀具备跨文件原子性。

---

## 4. 录制与回放切调集成

1. **录制时入队**：`recordPresetChange(const RecordedPreset&)` 在消息线程注册不可变快照，发布有界 SPSC 槽位；音频线程在受影响的捕获块起点先写预设 variant，再写该块 MIDI。
2. **采样边界执行**：实时、内置 WAV 和 VST3 WAV 均在事件采样点切换声学参数；同采样预设先于 MIDI，后续 NoteOff 使用起音锁定的原输出身份。
3. **UI 独立通知**：消息线程消费最新状态通知并调用 `applyRecordedPresetUi()`；重复循环可合并视觉通知，但每次声学事件仍在音频路径执行，末块通知不因播放结束清空。该路径不重查目录，不再调用 `applyPresetByIndex()` 来晚到改变音频。

---

## 5. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **PST-001** | 新建预设 Save As New | 调整键位与调号 → 点击 Save As New → 输入 "Rock-D" 确认 → 下拉菜单显示并激活该预设 | [x] 已通过 |
| **PST-002** | F1-F12 快捷键即时切换 | 在预设列表中配置多个预设，按下 F1/F2/F3，键位与调号即时生效无卡顿 | [x] 已通过 |
| **PST-003** | 重命名与同名冲突提示 | 重命名当前预设，文件名与标题同步修改；若重命名为已有名称，弹出覆盖确认 | [x] 已通过 |
| **PST-004** | 删除当前预设回退 | 删除正在使用的用户预设，物理文件被删除，界面自动平稳回退至 `[Default]` | [x] 已通过 |
| **PST-005** | 内置 Default 保护 | 切换至 `[Default]`，确认 Rename 与 Delete 按钮处于 disabled 状态 | [x] 已通过 |
| **PST-006** | 拖放导入预设 | 从外部文件夹拖入 `.devpiano.preset` 文件，列表立即刷新并自动激活 | [x] 已通过 |
| **PST-007** | 录制中切调回放验证 | 录制中在第 5 秒按 F2 切调，回放到达第 5 秒时观察界面与发声自动完成切调 | [x] 已通过 |
| **PST-008** | 规范化同路径与大小写重命名 | 同名经非法字符清理后映射回自身，以及 Windows 仅大小写变化；更新名称后预设仍可加载且绑定保持 | [x] Windows 文件/实际界面验证通过 |
| **PST-009** | 重命名写入/提交失败 | 锁定源文件，或使目标被占用；源和已有目标内容保留，失败提交恢复源且无暂存文件残留 | [x] Windows 隔离文件验证通过 |
| **PST-010** | 恢复后立即编辑绑定 | 保存 B 为最后活动预设并重启；不手动选预设即修改音高/通道，列表、运行绑定与自动保存均为 B | [x] Windows 实际界面验证通过 |

除手工场景外，预设系统由自动化单元测试全面覆盖：`PerformancePresetTest`（格式 v1 序列化、磁盘文件安全读写与 KeyGroup 往返）、`AcousticSettingsPersistenceTest`（缺失 acoustics 节点向后兼容与区间钳制）、`TemperamentSettingsPersistenceTest`（律制标识符与 A4 基频持久化安全）。

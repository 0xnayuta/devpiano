# Performance Preset 预设系统与 CRUD 编排说明

> 用途：说明 devpiano 的 Performance Preset 预设系统、`.devpiano.preset` JSON 格式规范、CRUD 编排流（`PresetFlowSupport`）、F1-F12 快捷键与录制切调集成。
> 当前状态：已全量实现并稳定服务于演奏配置管理。
> 更新时机：预设数据模型、文件格式版本或快捷键调度规则发生变化时。

---

## 1. 概述与设计定位

为了让演奏者能够针对不同曲目快速切换键位映射、音色通道与调号，devpiano 建立了高度解耦的 **Performance Preset 预设系统**：

1. **完整演奏快照**：一份预设完整打包了当前键位绑定（`KeyboardLayout`）、16 通道矩阵（`ChannelMatrix`）、声学与物理参数（`acoustics`: 琴盖、触键曲线、弱音踏板、律制、基准音高、空间视角、混响类型与干湿比、机械噪声、毛毡老化）、全局调号与移调开关（`keySignature`）、键盘渲染模式以及 128 项逐键个性化标签与颜色；
3. **独立 JSON 文件（`.devpiano.preset`）**：采用规范的 JSON 格式存储于 `DevPiano/Presets/` 目录下，便于用户备份、分享与跨设备导入；
4. **一键 CRUD 与声明式弹窗**：通过 `ControlsPanel` 下拉菜单及 Save As New / Rename / Delete 按钮操作，全面接入 `JiveModalDialog` 声明式弹窗；
5. **F1-F12 极速快捷键**：演奏过程中按下 F1-F12 即可毫秒级无缝切换预设；
6. **录制自动切调**：录制过程中切换预设会自动向 Take 写入 `presetChange` 事件，回放至该时刻自动切调。

---

## 2. `.devpiano.preset` 文件格式规范（v1）

```json
{
  "version": 1,
  "name": "Pop Piano in D",
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
    ]
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
    "previewAlpha": 0.0,
    "customKeyLabels": [],
    "customKeyColours": []
  }
}
```

---

### 2.1 声学与物理拟真对象（`acoustics`）

| 属性字段 | 数据类型 | 取值范围与默认值 | 含义与声学作用 |
|---|:---:|:---:|---|
| `lidPosition` | int | 0 (全开) / 1 (半开) / 2 (闭盖) | 琴盖开合度声学传递函数 |
| `touchVelocityCurve`| int | 0 (标准) / 1 (轻触) / 2 (重触) / 3 (宽动态) | 键盘触键力度响应非线性曲线 |
| `unaCorda` | bool | `true` / `false` | 弱音/移位踏板物理拟真（MIDI CC 67 联动） |
| `temperament` | string | `"equal"`, `"meantone"`, `"werckmeister3"`, `"kirnberger3"`, `"just"` | 古典微调律制选择 |
| `referencePitchA4` | double | 400.0 ~ 480.0 Hz（默认 440.0） | A4 基准基频换算 |
| `soundPerspective` | string | `"player"` (演奏者) / `"audience"` (听众) | 立体声空间声像展开视角 |
| `reverbSpace` | string | `"chamber"` (室内乐) / `"hall"` (音乐厅) / `"studio"` (录音棚) | 房间混响网络预设空间 |
| `reverbWet` | float | 0.0 ~ 1.0（默认 0.0） | 房间混响干湿混合比 |
| `pedalNoiseLevel` | float | 0.0 ~ 1.0（默认 0.6） | 延音踏板扫掠呼啸与共鸣冲击机械动作音量 |
| `feltAgeingAmount` | float | 0.0 ~ 1.0（默认 0.0） | 琴槌羊毛纤维磨损压实老化深度 |

> **向后兼容性保证**：若读取的历史预设缺失 `"acoustics"` 节点或部分声学字段，系统自动填充出厂默认值并支持根节点平铺字段的安全回退解析，且反序列化时对所有枚举与数值实施合法区间 `jlimit` 钳制保护。

---

## 3. 预设生命周期与 CRUD 编排（`PresetFlowSupport`）

### 3.1 内置 Default 预设

- 系统内置出厂默认预设 `[Default]`，作为最底层的基准配置；
- `[Default]` 不允许被重命名或删除（按钮自动置灰）；
- 当用户删除了当前活动预设时，系统自动安全回退至 `[Default]`。

### 3.2 预设操作流

- **自动发现**：启动时自动扫描 `DevPiano/Presets/` 目录下的全部 `.devpiano.preset` 文件并填充下拉列表；
- **Save As New（另存为）**：弹出 `JiveModalDialog::launchSingleInput`，输入名称后保存新文件并立即激活；
- **Rename（重命名）**：弹出单行输入弹窗，原子修改文件名与内部 `name` 字段；
- **Delete（删除）**：弹出 `JiveModalDialog::launchConfirm` 确认弹窗，确认后删除物理文件并安全切换预设；
- **拖放导入**：直接将 `.devpiano.preset` 拖入主窗口即可自动复制并立即应用。

---

## 4. 录制与回放切调集成

1. **录制时入队**：演奏者在录制中按下 F1-F12 或通过下拉菜单切换预设，`PresetFlowSupport` 调用 `RecordingEngine::recordPresetChange(presetIndex, timestampSamples)`；
2. **回放时自动切调**：回放引擎推进到该时间戳时，自动调用 `applyPresetByIndex()`，实现自动伴奏切调演练。

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

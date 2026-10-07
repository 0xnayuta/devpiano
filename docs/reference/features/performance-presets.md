# Performance Preset 预设系统与 CRUD 编排说明

> 用途：说明 devpiano 的 Performance Preset 预设系统、`.devpiano.preset` JSON 格式规范、CRUD 编排流（`PresetFlowSupport`）、F1-F12 快捷键与录制切调集成。
> 当前状态：已全量实现并稳定服务于演奏配置管理。
> 更新时机：预设数据模型、文件格式版本或快捷键调度规则发生变化时。

---

## 1. 概述与设计定位

为了让演奏者能够针对不同曲目快速切换键位映射、音色通道与声学参数配置，devpiano 建立了高度解耦的 **Performance Preset 预设系统**：

1. **演奏映射与声学配置**：预设打包当前键位绑定（`KeyboardLayout`，含 4 组 `KeyGroup` 与激活组索引）、16 通道矩阵（`ChannelMatrix`）、`acoustics` 声学配置子集、键盘显示模式以及 128 项逐键标签与颜色；不保存音源类型、Master/ADSR 或 VST3 内部状态。JSON 中的 `keyboard.keySignature` / `midiTranspose` 记录保存上下文，普通激活不覆写应用全局调号；
2. **独立 JSON 文件（`.devpiano.preset`）**：采用规范的 JSON 格式存储于 `DevPiano/Presets/` 目录下，便于用户备份、分享与跨设备导入；
3. **一键 CRUD 与声明式弹窗**：通过 `ControlsPanel` 下拉菜单及 Save As New / Rename / Delete 按钮操作，全面接入 `JiveModalDialog` 声明式弹窗；
4. **F1-F12 快捷键**：按列表次序选择预设，键位、矩阵与声学配置经既有提交路径生效；全局调号保持当前设置，不承诺与硬件无关的毫秒级切换期限；
5. **录制切换与回放还原**：切换时保存预设永久身份与当时声学快照；回放按采样点执行（含快照内嵌的移调状态），不依赖录制后目录排序。

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

### 2.0 永久身份与当前格式

- `uuid` 是文件身份，`name` 是可变显示名称。另存为新预设生成新 UUID；重命名、自动保存和导入保留已有 UUID。
- 仅准入当前整数版本 `2`；浮点数（如 `2.0`）、字符串版本（如 `"2"`）及非 2 整数（如 `1`、`999`）一律拒绝。缺失或空白 UUID 拒绝（UUID 读取只校验字符串 trim 后非空，不作 UUID 格式正则认证），不根据名称补生成身份或迁移 v1 文件。内置 Default 使用固定身份，新文件预设显式生成 UUID。
- 启动恢复只匹配已保存 UUID；旧名称不作为第二条查找路径。目录扫描发现重复 UUID 时记录告警日志，按 UUID 激活时因身份歧义明确拒绝（`[Preset] ambiguous permanent identity rejected`），绝不猜测目标。列表显示名称，运行绑定保存 UUID，磁盘路径仍由当前名称规范化得到。
- 原生演奏另外保存 Take 内的预设和 `AcousticSnapshot`，包含当时的音源类型、Master/ADSR、物理/空间参数及回放移调；不能把预设文件自身的配置子集误写为这些全部运行时字段都已持久化在 `.devpiano.preset`。
- **VST3 宿主插件独立性**：`.devpiano.preset` 不包含、不切换也不持久化 VST3 实例或内部状态；插件由 `PluginHost` / `PluginOperationController` 和全局设置独立管理。内置物理参数不写入外部插件；回放快照向其转发 CC67，宿主仍执行 Master/房间混响。
- 键位 `action.trigger` 只接受 `"keyDown"`，该可选字段缺省时仍默认 `"keyDown"`。显式 `"keyUp"` 或未知值在加载时拒绝，保存也不把非法内存状态静默改写为有效绑定；原文件与当前应用预设保留。

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
| `reverbSpace` | string | `"chamber"` (室内乐) / `"concert_hall"` (音乐厅) / `"studio"` (录音棚) | 房间混响网络预设空间；不再识别旧 `"hall"` 别名 |
| `reverbWet` | float | 0.0 ~ 1.0（默认 0.0） | 房间混响干湿混合比 |
| `pedalNoiseLevel` | float | 0.0 ~ 1.0（默认 0.6） | 延音踏板扫掠呼啸与共鸣冲击机械动作音量 |
| `feltAgeingAmount` | float | 0.0 ~ 1.0（默认 0.0） | 琴槌羊毛纤维磨损压实老化深度 |

> **当前格式缺省与限幅**：声学字段从 `acoustics` 节点读取，`keyboard` 节点从 `keyboard` 节点读取，两者均不再解析旧根节点平铺字段；当前可选字段缺失时使用出厂默认值，枚举与数值仍按合法区间限幅。这不是对历史版本的兼容承诺。

### 2.2 键位分组对象（`groups` 与 `activeGroupIndex`）

在 Phase 34-B 中，`layout` 节点引入了多键位分组持久化支持：

| 属性字段 | 数据类型 | 取值范围与默认值 | 含义与作用 |
|---|:---:|:---:|---|
| `transposeOffset` | int | -12 .. +12（默认 0） | 当前键组相对基准音高的半音移调量 |
| `octaveShift` | int | -3 .. +3（默认 0） | 当前键组相对基准音高的八度偏移量（每八度 12 半音） |
| `channel` | int | 0 .. 16（默认 0） | 目标 MIDI 通道覆盖（0 表示继承绑定本身通道，1~16 表示强制覆盖为指定通道） |
| `name` | string | 字符串（默认 `"A"`, `"B"`, `"C"`, `"D"`） | 键组标签显示名称 |
| `activeGroupIndex` | int | 0 .. 3（默认 0） | 当前预设激活的键组索引 |

> **当前格式缺省**：当前 v2 预设的可选 `groups` 缺失时使用 4 组默认 KeyGroup（A/B/C/D、偏移为 0），`activeGroupIndex` 默认 0；不据此接收旧版文件。

### 2.3 键盘显示与调号契约（`keyboard` 节点与 `commitPreset` 行为）

- **调号与移调字段契约**：`captureCurrentState()` 会将当前运行期的 `appSettings.keySignature` 与 `appSettings.midiTranspose` 捕获至 JSON `"keyboard"` 节点，以记录保存时的调性上下文；但在 `PresetFlowSupport::commitPreset()` 激活预设时，这两项**故意不覆写**（intentionally not overwritten）`appSettings` 的全局调号与移调设置。全局调号属于全局设置对话框管理的运行级偏好，不随预设切换意外改变，确保启动恢复和常规切换不冲掉用户当前设置。相比之下，在演奏录制回放中，`PresetFlowSupport::applyRecordedPresetUi()` 会通过内嵌的 `RecordedPreset.acoustic` 恢复当时的移调 offset 与使能状态。
- **残影收缩系数（`fadeSpeed`）**：`keyboard.fadeSpeed` 是每帧指数收缩系数，范围 `0.50 .. 0.99`，默认 `0.92`。设置、预设和显示组件使用同一限幅；输入中的 `1` 或更大值钳至 `0.99`，不会保留不收缩的动画端点。
- **输入索引与投影映射**：绑定音符、逐键标签与颜色仍按配置输入 MIDI 音符保存，不把当前 Group/modifier/矩阵变换后的输出写回预设。两张看板通过映射层投影显示最终身份；在输出位置新建绑定时保存对应输入音符，避免后续再次移调。

---

## 3. 预设生命周期与 CRUD 编排（`PresetFlowSupport`）

### 3.1 内置 Default 预设

- 系统内置出厂默认预设 `[Default]`，作为最底层的基准配置；
- `[Default]` 不允许被重命名或删除（按钮自动置灰）；
- 当用户删除了当前活动预设时，系统自动安全回退至 `[Default]`。

### 3.2 预设操作流

- **自动发现**：启动时自动扫描 `DevPiano/Presets/` 目录下的全部 `.devpiano.preset` 文件并填充下拉列表；
- **Save As New（另存为）**：弹出 `JiveModalDialog::launchSingleInput`，输入名称后保存新文件并立即激活；
- **Rename（重命名）**：规范化名称后比较 JUCE File 路径身份；同路径（含 Windows 大小写等同路径）事务更新，不删除自身，也不弹独立目标覆盖确认。不同路径已有目标先显示确认，取消保留两份原数据；确认后先写临时目标并暂存源，再替换目标，失败恢复源。回滚自身失败时保留源备份并记录路径，成功则保持 UUID；不承诺跨文件断电原子性。
- **Delete（删除）**：弹出 `JiveModalDialog::launchConfirm` 确认弹窗，确认后删除物理文件并安全切换预设；
- **拖放导入**：拖入 `.devpiano.preset` 文件经 `loadPreset()` 严格校验版本（整数 2）与有效 UUID；目标路径由预设名称规范化得到（`resolvePresetFile(name)`）。若目标文件已存在，弹出 `JiveModalDialog::launchConfirm` 请求用户确认覆盖，取消则不触碰既有文件；确认或无同名时写入目标文件、刷新缓存并应用该预设。导入保留文件内的原有 UUID；若导入的预设与已有不同名预设存在相同 UUID，扫描目录时记录重复告警，后续通过 UUID 激活将因身份歧义被拒绝，不发生静默覆盖。
- **启动恢复与自动保存**：恢复用户 B 与下拉列表选择走同一激活路径；在提交布局前同时设置控制器 `currentPresetId` 和设置 `lastActivePresetId`，立即编辑绑定仍保存到 B。文件身份由激活入口显式传入，不以 `layout.id` 推断；用户预设可以继承内置布局 ID，只有真正内置 Default 才无文件绑定。
- **故障恢复边界**：若文件系统同时阻止目标提交和源文件回滚，保留源备份并在日志记录路径，不删除唯一原始数据；提交成功后若备份清理被外部阻止，同样保留并记录。正常失败/取消场景的字节保留与临时文件清理已直接验证，不宣称断电或强杀具备跨文件原子性。

---

## 4. 录制与回放切调集成

1. **录制时入队**：`recordPresetChange(const RecordedPreset&)` 在消息线程注册不可变快照，发布有界 SPSC 槽位；音频线程在受影响的捕获块起点先写预设 variant，再写该块 MIDI。
2. **采样边界执行**：实时、内置 WAV 和 VST3 WAV 在事件采样点切换各自支持的参数；同采样预设先于 MIDI，后续 NoteOff 使用起音锁定的原输出身份。参数执行不等于完整音高同构，当前非零快照移调差异见 [P1 回归线索](../../issues/known-issues.md#原生演奏快照移调与-wav-音高不一致)。
3. **UI 独立通知**：消息线程消费最新状态通知并调用 `applyRecordedPresetUi()`；重复循环可合并视觉通知，但每次声学事件仍在音频路径执行，末块通知不因播放结束清空。该路径不重查目录，不再调用 `applyPresetByIndex()` 来晚到改变音频。

---

## 5. 专项手工与边界测试清单

| 用例编号 | 测试场景 | 操作步骤与验证目标 | 状态 |
|---|---|---|:---:|
| **PST-001** | 新建预设 Save As New | 调整键位与声学参数 → 点击 Save As New → 输入 "Rock-D" 确认 → 下拉菜单显示并激活该预设，分配全新永久 UUID | [x] 已通过 |
| **PST-002** | F1-F12 快捷键即时切换 | 在预设列表中配置多个预设，按下 F1/F2/F3，键位映射、通道矩阵与声学参数即时生效，全局调号保持当前设置不被覆写 | [x] 已通过 |
| **PST-003** | 重命名与同名冲突提示 | 重命名当前预设，文件名与标题同步修改；若重命名为已有独立名称，弹出覆盖确认，取消保留原文件 | [x] 已通过 |
| **PST-004** | 删除当前预设回退 | 删除正在使用的用户预设，物理文件被删除，界面自动平稳回退至 `[Default]` | [x] 已通过 |
| **PST-005** | 内置 Default 保护 | 切换至 `[Default]`，确认 Rename 与 Delete 按钮处于 disabled 状态 | [x] 已通过 |
| **PST-006** | 拖放导入预设 | 从外部文件夹拖入 `.devpiano.preset` 文件，列表立即刷新并自动激活 | [x] 已通过 |
| **PST-007** | 录制中切预设与回放切调 | 录制中在第 5 秒按 F2 切换预设，回放到达第 5 秒时观察发声与键位按内嵌快照自动完成声学参数与移调还原 | [x] 已通过 |
| **PST-008** | 规范化同路径与大小写重命名 | 同名经非法字符清理后映射回自身，以及 Windows 仅大小写变化；更新名称后预设仍可加载且绑定保持 | [x] Windows 文件/实际界面验证通过 |
| **PST-009** | 重命名写入/提交失败 | 锁定源文件，或使目标被占用；源和已有目标内容保留，失败提交恢复源且无暂存文件残留 | [x] Windows 隔离文件验证通过 |
| **PST-010** | 恢复后立即编辑绑定 | 保存 B 为最后活动预设并重启；不手动选预设即修改音高/通道，列表、运行绑定与自动保存均为 B | [x] Windows 实际界面验证通过 |

自动化回归由 `PerformancePresetTest` 覆盖当前 v2 UUID、非当前版本拒绝、文件保护与 KeyGroup 往返，声学/律制持久化由相应测试套件覆盖；实际重命名确认、启动恢复及普通预设保留全局调号的历史直接证据见 Phase A/H 实施记录。

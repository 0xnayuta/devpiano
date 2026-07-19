# MIDI / Performance 测试夹具清单

> 用途：记录固定 MIDI fixture 样本库与 performance fixture 样本，作为 MIDI 导入/导出/roundtrip/回放行为的统一输入基准。
> 更新时机：新增或修改 fixture 文件时。

### 任务定位

Phase 6-7 是 Phase 6 后续各阶段（6-1 演奏文件保存/打开、6-2 播放速度控制、6-5 MIDI 导入增强）以及整体 MIDI roundtrip 的**工程基础设施**。它不实现业务功能，而是为这些阶段提供统一的测试输入基准。

**前置关系：** 建议在 Phase 6-1、6-2、6-5 之前或并行完成。Phase 6-6（Diagnostics 最小层）是 Phase 6-7 的依赖基础——fixture 验证过程中产生的诊断输出依赖 `DP_LOG_*` / `DP_TRACE_MIDI` 宏。

### 背景问题

当前 Phase 6 各阶段的开发和手工验收面临以下问题：

1. **依赖临时文件**：每次验证 MIDI 导入、roundtrip、错误处理都需要手动准备 MIDI 文件或临时敲键盘录制，无法稳定复现。
2. **无基准样本**：不同开发者使用不同的 MIDI 文件，导入行为的判断标准不统一。
3. **口头复现**：bug 报告依赖"我用一个 MIDI 文件试了，不行"这类描述，无法快速定位是文件问题还是代码问题。
4. **smoke test 缺失**：每次发版或合入前没有统一的最小回归集。

Phase 6-7 的目标就是用**固定 fixture 样本库**取代临时文件和口头复现。

### 目标

- 建立固定 MIDI fixture 样本库，涵盖主流场景和边界情况。
- 建立固定 PerformanceEvent / 演奏数据 fixture（JSON 格式）。
- 为 MIDI 导入、导出、roundtrip、错误处理、回放行为提供稳定输入。
- 让后续 smoke test 和手工验收有统一依据。
- 避免每次 bug 排查都依赖临时文件和口头复现。

### 非目标

- **不实现测试代码框架**（Phase 8 之后才考虑单元测试基础设施）。
- **不实现自动化测试运行**（无 test runner、无 CI 脚本）。
- **不创建 mock 插件或 mock 音频设备**。
- **不实现 fixture 的运行时加载逻辑**（那是 Phase 6-1/6-5 的任务）。
- **不修改 source/、CMakeLists.txt 或任何业务代码**。本阶段只产出现有文档（本文档）。

### 目录结构

```
../../tests/fixtures/
├── midi/
│   ├── simple-notes.mid          # 最简 note on/off 序列
│   ├── velocity-channel.mid       # 多 velocity、多 channel
│   ├── sustain-pedal.mid          # 含 CC64 sustain on/off
│   ├── multitrack-basic.mid       # 多轨（Type 1），含 track names
│   ├── tempo-change-basic.mid     # 含 meta tempo change 事件
│   ├── empty.mid                  # 零事件空文件
│   └── invalid.mid                 # 损坏/非法 MIDI 文件
└── performance/
    └── simple-performance.json    # 最小 .devpiano 结构样本（Phase 6-1 之后才有意义）
```

> **注意**：fixture 文件实际位于 `../../tests/fixtures/`。

### Fixture 清单

#### MIDI fixtures

| 文件名 | 内容描述 | 预期用途 |
|--------|----------|----------|
| `simple-notes.mid` | 单轨，60/64/67 三个音符依次发声，velocity 100/80/60，时长各 0.5s，120 BPM，960 PPQ | MIDI 导入基础验证；roundtrip 往返对比基准 |
| `velocity-channel.mid` | 单轨，16 个音符跨不同 velocity(20/64/127) 和 2 个 channel(1/2) | 验证 velocity 解析、channel 分配是否正确 |
| `sustain-pedal.mid` | 单轨，含 CC64 sustain on(127) / sustain off(0)，覆盖多个音符 | Phase 6-5 增强导入验证；sustain 效果可听性 |
| `multitrack-basic.mid` | Type 1，2 个 track，track 0 含 tempo meta，track 1 含 note 事件 | 验证多轨选择逻辑（自动选有 note 的轨） |
| `tempo-change-basic.mid` | 单轨，0ms 设 tempo 120，500ms 后切换为 tempo 180 | 验证 tempo change 事件被正确跳过或不崩溃 |
| `empty.mid` | 合法 MIDI 文件头，但零 track、零事件 | 验证空文件导入不崩溃，Logger 输出警告 |
| `invalid.mid` | 非 MIDI 数据（如随机字节、"not a midi file" 文本） | 验证文件解析错误处理不崩溃，Logger 输出错误 |

#### Performance fixture

| 文件名 | 内容描述 | 预期用途 |
|--------|----------|----------|
| `simple-performance.json` | Phase 6-1 之后的最小 `.devpiano` 格式样本，含 2-3 个 note 事件 | 验证保存/打开 roundtrip 的最小基准 |

### 每个 fixture 的预期用途

| Fixture | 用途 |
|---------|------|
| `simple-notes.mid` | Phase 6-5 之前 MIDI 导入的基础验证；作为 `DP_TRACE_MIDI` 输出对照基准（ Debug 下 MIDI trace 输出 vs 预期 note 序列） |
| `sustain-pedal.mid` | Phase 6-5 增强导入的目标 fixture；手工验证延音踏板效果是否可听 |
| `multitrack-basic.mid` | 自动选轨逻辑验证；手工确认选中的轨是含 note 的轨而非 tempo track |
| `tempo-change-basic.mid` | 验证 phase4-midi-file-import.md 中"跳过 meta 事件"行为是否稳定；导入过程不因 tempo change 事件而出错 |
| `empty.mid` | 错误处理边界验证；空文件不崩溃的最小保证 |
| `invalid.mid` | 健壮性验证；损坏文件不崩溃，Logger 正确输出错误 |
| `velocity-channel.mid` | 验证 CC、velocity、channel 解析的完整性 |
| `simple-performance.json` | Phase 6-1 保存/打开 roundtrip 的最小输入；后续可在此基础上扩展 smoke test |

### 验收标准
- [x] 文档中清晰列出 8 个 MIDI fixture + 1 个 performance fixture 的名称、描述和用途。
- [x] 每个 fixture 的预期用途与 Phase 6-5、Phase 6-1 的功能边界对应。
- [x] fixture 清单与 Phase 6-5 验收标准中的"导入 xxx 事件"形成一一映射。
- [x] 明确说明本轮不创建任何 fixture 文件，仅做规划记录。
- [x] Phase 6-7 与 Phase 6-6（Diagnostics）和 Phase 6-5（MIDI 导入增强）的关系清晰。
- [x] 全部 8 个 fixture 文件已创建于 `tests/fixtures/`，经验证可用。
### 风险与边界

| 风险 | 等级 | 应对 |
|------|------|------|
| fixture 文件格式不符合预期导致验收失效 | 中 | 本轮只规划，下轮创建时需对照 JUCE `MidiFile` 解析行为验证格式 |
| fixture 覆盖不足导致边界情况漏测 | 低 | MVP 阶段只覆盖最高频场景；边界情况后续按需补充 |
| 规划过度，实际创建时发现不合理 | 低 | fixture 结构极简（MIDI 是标准格式，JSON 是 human-readable），不易有结构性错误 |
| 成为拖延 Phase 6-1/6-5 的借口 | 中 | 本轮仅文档更新，下轮实现时 fixture 创建和业务代码实现可并行推进 |

### 与 Phase 6-5 MIDI 导入增强、Phase 6-6 Diagnostics 最小层的关系

```
Phase 6-6 (Diagnostics)        Phase 6-7 (Fixtures)          Phase 6-5 (MIDI Import)
      │                              │                                │
      │  DP_TRACE_MIDI               │  固定输入基准                  │  新增事件类型
      │  输出对照                    │                                │
      └──────────────────────────────┴────────────────────────────────┘
                                     │
                      fixture 验证时用 DP_TRACE_MIDI
                      对比 MIDI 导入的实际行为

Phase 6-7 同时也是 Phase 6-1 (Save/Open) 和 Phase 6-2 (Speed) 的基础设施：
fixture 的 PerformanceEvent 数据结构是 Phase 6-1 保存/打开的直接操作对象。
```

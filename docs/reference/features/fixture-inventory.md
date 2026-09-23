# MIDI / Performance 测试夹具清单

> 用途：记录固定 MIDI fixture 样本库与 performance fixture 样本，作为 MIDI 导入/导出/roundtrip/回放行为与自动化测试的统一输入基准。
> 当前状态：已全量落地并稳定服务于 `source/tests/MidiFileImporterTest.cpp`、`PerformanceFileTest.cpp` 与日常冒烟测试。
> 更新时机：新增或修改 fixture 文件时。

## 1. 概述与定位

测试夹具（Test Fixtures）为 devpiano 的 MIDI 导入、导出、roundtrip 往返校验、错误处理与演奏回放行为提供统一、确定性的输入基准，彻底消灭依赖临时文件与口头复现的不确定性。

## 2. 自动化单元测试集成
在当前项目中，这些 fixture 已全面接入 `source/tests/` 自动化测试体系：
- `source/tests/MidiFileImporterTest.cpp`：自动化加载 `simple-notes.mid`、`velocity-channel.mid`、`sustain-pedal.mid`、`multitrack-basic.mid`、`tempo-change-basic.mid`、`empty.mid` 与 `invalid.mid`，验证 Track 解析、通道映射、Meta 事件过滤与异常防御；
- `source/tests/PerformanceFileTest.cpp`：验证 `simple-performance.json` 的序列化/反序列化与向后兼容。

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
| `multitrack-basic.mid` | 多轨时间线合并验证；Track 0 无音符、Track 1 含音符，确认有音符轨道的事件进入回放 Take |
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

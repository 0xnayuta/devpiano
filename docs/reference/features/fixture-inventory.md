# MIDI / Performance 测试夹具清单

> 用途：记录固定 MIDI fixture 样本库与 performance fixture 样本，作为 MIDI 导入/导出/roundtrip/回放行为与自动化测试的统一输入基准。
> 当前状态：已全量落地并稳定服务于 `source/tests/MidiFileImporterTest.cpp`、`PerformanceFileTest.cpp` 与日常冒烟测试。
> 更新时机：新增或修改 fixture 文件时。

## 1. 概述与定位

固定 MIDI 样本与程序化构建的隔离临时输入共同提供可复建的导入、导出、准入和回放边界；临时文件由 ScopedTempDir 管理，不以真实用户文件作探针。

## 2. 自动化单元测试集成
在当前项目中，这些 fixture 与自动化测试体系紧密配合：
- `source/tests/MidiFileImporterTest.cpp`：自动化加载 `simple-notes.mid`、`velocity-channel.mid`、`sustain-pedal.mid`、`multitrack-basic.mid`、`tempo-change-basic.mid`、`empty.mid` 与 `invalid.mid`，验证 Track 解析、通道映射、Meta 事件过滤与异常防御；
- `source/tests/PerformanceFileTest.cpp`：针对 `.devpiano`（Schema v3）验证内嵌快照表序列化/反序列化、v1/v2 历史兼容与拒绝规则、TemporaryFile 事务替换及异常防御；`tests/fixtures/performance/simple-performance.json` 作为早期静态格式样本保留。

### 目录结构

```
tests/fixtures/
├── midi/
│   ├── simple-notes.mid          # 最简 note on/off 序列
│   ├── velocity-channel.mid       # 多 velocity、多 channel
│   ├── sustain-pedal.mid          # 含 CC64 sustain on/off
│   ├── multitrack-basic.mid       # 多轨（Type 1），含 track names
│   ├── tempo-change-basic.mid     # 含 meta tempo change 事件
│   ├── empty.mid                  # 零事件空文件
│   └── invalid.mid                 # 损坏/非法 MIDI 文件
└── performance/
    └── simple-performance.json    # 最小 .devpiano 录制数据样本
```

> **注意**：fixture 文件实际位于仓库根目录 `tests/fixtures/`，测试代码统一基于 `__FILE__` 相对寻址（TEST-014），脱离当前工作目录（CWD）依赖。

### Fixture 清单

#### MIDI fixtures

| 文件名 | 内容描述 | 预期用途 |
|--------|----------|----------|
| `simple-notes.mid` | 单轨，60/64/67 三个音符依次发声，velocity 100/80/60，时长各 0.5s，120 BPM，960 PPQ | MIDI 导入基础验证；roundtrip 往返对比基准 |
| `velocity-channel.mid` | 单轨，16 个音符跨不同 velocity(20/64/127) 和 2 个 channel(1/2) | 验证 velocity 解析、channel 分配是否正确 |
| `sustain-pedal.mid` | 单轨，含 CC64 sustain on(127) / sustain off(0)，覆盖多个音符 | Phase 6-5 增强导入验证；sustain 效果可听性 |
| `multitrack-basic.mid` | Type 1，2 个 track，track 0 含 tempo meta，track 1 含 note 事件 | 验证全轨并轨后 Track 1 音符进入统一 Take |
| `tempo-change-basic.mid` | 单轨，0ms 设 tempo 120，500ms 后切换为 tempo 180 | 验证 Tempo Map 元数据解析和播放事件中的 Meta 过滤 |
| `empty.mid` | 合法 MIDI 文件头，但零 track、零事件 | 验证空文件导入不崩溃，Logger 输出警告 |
| `invalid.mid` | 非 MIDI 数据（如随机字节、"not a midi file" 文本） | 验证文件解析错误处理不崩溃，Logger 输出错误 |

#### Performance fixture

| 文件名 | 内容描述 | 预期用途 |
|--------|----------|----------|
| `simple-performance.json` | 早期静态 `.devpiano` 格式参考样本，含 note 事件 | 历史格式参考样本；`PerformanceFileTest` 核心回归采用程序化构建与隔离临时目录事务往返验证 |

### 验收与维护标准

- [x] MIDI 与 Performance 夹具的名称、内容和用途均在上述表格中列明。
- [x] `MidiFileImporterTest` 消费固定 MIDI 样本；原生持久化回归使用程序化 Take 与隔离目录，静态 Performance 样本仅作早期格式参考。
- [x] 自动化测试通过 `source/tests/MidiFileImporterTest.cpp` 与 `PerformanceFileTest.cpp` 全面覆盖。
- [x] 测试夹具相对寻址遵循 TEST-014 纪律，不依赖执行时 CWD。

### 风险与维护边界

| 风险 | 等级 | 应对 |
|------|------|------|
| 修改既有 fixture 导致回归断言失真 | 高 | 既有 fixture 作为不可变测试基线，严禁在无整体迁移方案时修改内容 |
| 测试执行路径变化导致 fixture 找不到 | 低 | 已由 TEST-014（基于 `__FILE__` 向上相对定位）彻底根治，无论何处执行均能稳定定位 |
| 新增复杂 MIDI 特性缺乏测试样本 | 低 | 增量场景在 `tests/fixtures/` 目录下追加新文件，不破坏既有基线 |

### 与导入和持久化测试的关系

MIDI 样本用于 `MidiFileImporterTest` 的全轨并轨、Tempo Map、通道分配与结构错误防御回归；`PerformanceFileTest` 覆盖原生 Schema v3 序列化/反序列化、内嵌预设快照表、v1/v2 兼容规则与事务写出。测试套件严格执行防漂移准则，`DP_TRACE_MIDI` 仅辅助诊断，不作为断言数量或用例数的固定基线。

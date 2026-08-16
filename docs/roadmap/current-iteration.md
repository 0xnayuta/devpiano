# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

Phase 11（声明式 UI 架构迁移，JIVE + melatonin_inspector）已全部完成并归档至 [`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)。v0.3.0 发布准备（版本号 / CHANGELOG / Release 构建 / 打包）为并行事项，见 [`../guides/release-workflow.md`](../guides/release-workflow.md)。

三项审计修复（2026-08-16 复审选定）已全部完成并提交：

| ID | 优先级 | 标题 | 提交 |
| --- | --- | --- | --- |
| `AUDIT-REC-007` | P2 | 提取公共渲染管线（WavFileExporter / PluginOfflineRenderer 重复代码） | `85ff516` |
| `AUDIT-SEC-004` | P2 | PerformanceFile 原子文件写入（TemporaryFile + rename） | `ca317ab` + `b54ae07`（savePreset 扩展） |
| `AUDIT-SEC-001` | P2 | PerformancePreset 未知 KeyAction type 记录 WARN | `90a9552` |

验证：WSL 全量测试 100% 通过、`RenderPipelineTest`/`PerformanceFileTest` 新增用例通过、Windows MSVC 构建成功（详见各节）。剩余 16 项 Deferred 维持暂缓，重开条件见 [`../audit/AUDIT-001-code-quality-audit-2026-07-20.md`](../audit/AUDIT-001-code-quality-audit-2026-07-20.md)。

---

## AUDIT-SEC-001：PerformancePreset 未知 KeyAction type 记录 WARN [已完成]

> **状态**：已完成 (2026-08-16)，提交 `90a9552`。WSL 全量测试 + Windows MSVC 构建通过。

### 背景

审计项 `SEC-001`（P2）：`varToKeyAction` 解析 preset JSON 时，未知 `type` 值被静默强制为 `"note"`，掩盖数据损坏。2026-08-16 复审确认仍存在，且表达式已退化为恒等 ternary——`(typeStr == "note") ? note : note` 两个分支相同，属无意义代码。`KeyActionType` 当前仅 `note` 一个枚举值，故无实际行为损害；收益在于**可观测性**（数据损坏可见）与**代码清理**。

### 现状代码

`source/Layout/PerformancePreset.cpp:33-35`（`varToKeyAction`）：

```cpp
auto typeStr = obj->getProperty("type").toString();
action.type
    = (typeStr == "note") ? devpiano::core::KeyActionType::note : devpiano::core::KeyActionType::note;
```

### 实施步骤

1. `source/Layout/PerformancePreset.cpp` 顶部添加 `#include "Diagnostics/Log.h"`（DP_LOG 宏统一入口，项目惯例）。
2. 将 33-35 行替换为：

```cpp
const auto typeStr = obj->getProperty("type").toString();
if (typeStr != "note")
    DP_LOG_WARN("[Preset] unknown KeyAction type '" + typeStr + "', falling back to \"note\"");
action.type = devpiano::core::KeyActionType::note;
```

   - 行为不变：未知类型仍回退 `note`（**不拒绝整个 preset**——`KeyActionType` 仅一个枚举值，拒绝会丢弃整个 preset 且无任何收益）。
   - 日志含原始 `typeStr`，便于定位损坏来源（手改文件 / 旧版本格式）。
3. 不修改 `keyActionToVar`（写出路径无变化）。

### 验证

- 单元测试：`KeyMapTypesTest` 或新增用例——构造 `type="bogus"` 的 var → `varToKeyAction` → `action.type == KeyActionType::note` 且不抛异常。`varToKeyAction` 位于匿名命名空间，不可直接测试；等价替代：通过 preset JSON 文件加载（`loadPreset`）验证未知 type 的 preset 仍可加载且 action 为 note。若测试成本高于收益，允许仅手动验证 + `format --check`。
- 回归：`./scripts/dev.sh test`（KeyMapTypesTest round-trip 用例守护序列化一致性）、`./scripts/dev.sh format --check`、`./scripts/dev.sh win-build`。
- 行为回归：加载一个现存 `.devpiano.preset` 文件，确认无 WARN 输出（type 均为 "note"）。

### 风险与回滚

- 风险极低：仅新增日志分支，无控制流变化。回滚 = 还原 3 行。

---

## AUDIT-SEC-004：PerformanceFile 原子文件写入 [已完成]

> **状态**：已完成 (2026-08-16)，提交 `ca317ab`（主项）+ `b54ae07`（savePreset 扩展）。新增 `PerformanceFileTest`（Files category，4 用例 28 断言）；WSL 全量测试 + Windows MSVC 构建通过。

### 背景

审计项 `SEC-004`（P2）：`savePerformanceFile` 用 `destinationFile.replaceWithText(json)` 直接截断写入目标文件——写入中途崩溃（断电 / 进程被杀）会留下半截 JSON，`.devpiano` 录制文件损坏且无备份。调用方：`RecordingSessionController.cpp:316/633`（手动保存 + 元数据更新），属录制数据主保存路径。`PerformancePreset.cpp:346`（`savePreset`）存在同类问题，作为可选扩展。

### 现状代码

`source/Recording/PerformanceFile.cpp:207-217`：

```cpp
bool savePerformanceFile(const RecordingTake& take, const juce::File& destinationFile,
                         const PerformanceFileMetadata& metadata) {
    if (take.isEmpty() || take.sampleRate <= 0.0)
        return false;
    ...
    return destinationFile.replaceWithText(json);
}
```

### 方案：juce::TemporaryFile（JUCE 官方原子写入惯用法）

`juce::TemporaryFile(const File& targetFile)` 在目标同目录创建临时文件，`overwriteTargetFileWithTemporary()` 原子替换目标（跨平台实现：Linux `rename(2)`，Windows 失败时先删目标再 rename）。已确认 `submodules/JUCE/modules/juce_core/files/juce_TemporaryFile.h` 提供 `getFile()` / `overwriteTargetFileWithTemporary()` / `deleteTemporaryFile()`。

### 实施步骤

1. 修改 `savePerformanceFile` 尾部：

```cpp
    juce::TemporaryFile tempFile(destinationFile);
    if (!tempFile.getFile().replaceWithText(json)) {
        tempFile.deleteTemporaryFile();
        return false;
    }
    if (tempFile.overwriteTargetFileWithTemporary())
        return true;
    tempFile.deleteTemporaryFile();
    return false;
```

   - 写临时文件失败 → 删除临时文件、返回 false，**目标文件保持原样**（现行为是目标已被截断破坏）。
   - 替换失败 → 同样清理临时文件，不残留。
2. 函数签名与调用方零改动（`bool` + 参数不变）。
3. 可选扩展（同 commit 或后续）：`PerformancePreset.cpp:346` `savePreset` 的 `replaceWithText` 应用同一模式（同类崩溃风险，改动同构）。

### 验证

- 单元测试（新文件 `source/tests/PerformanceFileTest.cpp`，JUCE `Files` category + TestRunner `--include-files` 运行——默认跳过 Files category 是 WSL root 权限限制，测试需显式开启）：
  - round-trip：`savePerformanceFile` → `loadPerformanceFile` 内容一致（事件数、时间戳、元数据）。
  - 失败注入：目标目录设为只读（或目标为不存在目录）→ 保存返回 false → 目标文件（若存在）内容不被破坏、无 `.tmp` 残留文件。
  - 临时文件清理：成功路径下目录中无临时文件残留。
- 回归：`./scripts/dev.sh test`、`./scripts/dev.sh format --check`、`./scripts/dev.sh win-build`。
- 手动：录制 → 保存 → 重开加载（内容一致）；保存后目录内无临时文件。

### 风险与回滚

- 行为变化仅限失败路径（成功路径等价：内容相同的 JSON 落盘）。`TemporaryFile` 构造失败（目标目录不可写）时 `getFile()` 写入失败 → 干净返回 false。
- 回滚 = 还原 `replaceWithText` 单行。

---

## AUDIT-REC-007：提取公共渲染管线（RenderPipeline） [已完成]

> **状态**：已完成 (2026-08-16)，提交 `85ff516`。新增 `RenderPipelineTest`（10 用例 75 断言）；WSL 全量测试 + Windows MSVC 构建通过。待办：Windows 侧 GUI 手动导出对比验证（预期差异 ≤1 sample）。

### 背景

审计项 `REC-007`（P2）：`WavFileExporter.cpp` 与 `PluginOfflineRenderer.cpp` 重复定义 `RenderEvent`、`buildRenderEvents`、`getScaledTakeLengthSamples`、`addPanicMidi`（另有 `scaleTimestamp`、`hasUsableOptions` 亦重复），相似度 ~50%。2026-08-16 复审确认 7-20 后仅日志迁移类改动，结构原样——这是 19 项 Deferred 中**重构收益最明确**的一项（消除双份维护，未来改渲染语义只需改一处）。

### 现状差异分析（行为对齐是关键）

| 函数 | WavFileExporter.cpp | PluginOfflineRenderer.cpp | 差异 |
| --- | --- | --- | --- |
| `RenderEvent` | `{timestampSamples, message}` | `{message, timestampSamples}` | 字段顺序不同（等价） |
| `scaleTimestamp` | 含 `<=0 → 0` 短路 + `max(0, …)` | 无保护 | Wav 更防御，统一取 Wav 版 |
| `buildRenderEvents` | ratio 双零检查 `(take.sr>0 && target>0)` | ratio 仅查 `take.sr>0`；`stable_sort` | 统一取双零检查 + sort 两者都有 |
| `getScaledTakeLengthSamples` | `max(scale(lengthSamples), 各事件 ts)`，空事件返回 `scale(lengthSamples)` | `events.empty() → 0`；否则 `max(last+1, scale(lengthSamples))` | **语义差异**：`+1` 与空事件返回值不同 |
| `addPanicMidi` | CC64 + CC120 + allNotesOff × 16 通道 | 同左 | 完全一致 |
| `hasUsableOptions` | 4 字段检查 | 同左 | 完全一致 |

`getScaledTakeLengthSamples` 的差异：Wav 路径对"最后一事件时间戳"不 +1（可能截断最后事件？不——事件以 `timestampSamples < blockEnd` 注入，+1 只影响尾部 1 sample）；Plugin 路径 `last+1` 保证最后事件完整入块。统一语义：**`max(scaleTimestamp(lengthSamples), events.empty() ? 0 : events.back().timestampSamples + 1)`**（取更保守的 Plugin 语义）。对 Wav 路径影响 ≤1 sample（44.1kHz 下 ~23µs），且两路径均追加 2s tail，输出总时长不受影响——但仍需在验证步骤中实测对比。

### 实施步骤

1. 新建 `source/Recording/RenderPipeline.h/.cpp`（`namespace devpiano::recording` 或既有 `devpiano::exporting`——建议 `devpiano::recording`，取值为录制域公共设施）：
   - `struct RenderEvent { juce::MidiMessage message; std::int64_t timestampSamples = 0; };`
   - `[[nodiscard]] std::int64_t scaleTimestamp(std::int64_t, double ratio) noexcept;`（Wav 版防御语义）
   - `[[nodiscard]] bool hasUsableRenderOptions(const WavExportOptions&) noexcept;`（注意：`WavExportOptions` 在 `source/Export/WavExportOptions.h`，RenderPipeline 引入对 Export 的依赖——可接受，或保持 `hasUsableOptions` 各留一份；**建议一并提取**，两文件均已依赖该头）
   - `[[nodiscard]] std::vector<RenderEvent> buildRenderEvents(const RecordingTake&, double targetSampleRate);`（双零检查 ratio + `message.setTimeStamp(0)` + stable_sort）
   - `[[nodiscard]] std::int64_t getScaledTakeLengthSamples(const RecordingTake&, const std::vector<RenderEvent>&, double targetSampleRate) noexcept;`（统一语义如上）
   - `void addPanicMidi(juce::MidiBuffer&, int sampleOffset) noexcept;`
2. `WavFileExporter.cpp`：删除 6 个重复定义，`#include "Recording/RenderPipeline.h"`；调用点不变（同名函数）。
3. `PluginOfflineRenderer.cpp`：同上。
4. **渲染主循环不提取**：两循环音频源（synth vs plugin 实例）与输出通道处理不同，整体提取需模板/回调，属过度设计；仅提取纯函数层。若首轮重构后两循环剩余相似度仍高（事件注入 + panic 判定），可再提取 `fillBlockMidiEvents(renderEvents, blockStart, blockEnd, midiBuffer)` 小助手（第二迭代，非本轮必须）。
5. `CMakeLists.txt`：
   - 主 target `target_sources`（Recording 区，:83-98 附近）添加 `source/Recording/RenderPipeline.cpp` / `.h`。
   - tests target（:231-266 区域）添加 `source/Recording/RenderPipeline.cpp`（被测纯逻辑）与新测试文件。

### 验证

- 单元测试（新文件 `source/tests/RenderPipelineTest.cpp`，无 GUI/设备依赖，可进 tests target）：
  - `buildRenderEvents`：采样率缩放（take 44.1k → target 48k 时间戳按 48/44.1 缩放）、稳定排序（乱序输入 → 有序输出）、空 take → 空 events、`setTimeStamp(0)` 生效。
  - `scaleTimestamp`：负值/零 → 0；正常缩放；ratio 0/负防御。
  - `getScaledTakeLengthSamples`：空 events → `scale(lengthSamples)`；last 事件超 length → `last+1`；length 超 last → `scale(lengthSamples)`。
  - `addPanicMidi`：16 通道 × 3 控制器 = 48 事件，offset 正确。
- 行为回归（关键）：同一 `RecordingTake` 分别导出 WAV（synth 路径）与经 `PluginOfflineRenderer`（需真实 VST3 插件，手动路径）——对比重构前后输出采样数一致（或仅尾部 +1 sample，可接受并记录）。WAV 导出路径可在 Windows 侧手动导出同一录制对比文件时长。
- 全套：`./scripts/dev.sh test`、`./scripts/dev.sh format --check`、`./scripts/dev.sh win-build`。

### 风险与回滚

- 唯一行为风险：`getScaledTakeLengthSamples` 语义统一（Wav 路径可能 +1 sample）。通过重构后对比导出验证。
- 编译风险：匿名命名空间删除后符号进入头文件，`RenderEvent` 字段顺序统一为 `{message, timestampSamples}`（对 `.message`/`.timestampSamples` 成员访问无影响）。
- 回滚 = 还原两文件 + 删除 RenderPipeline 文件（纯新增 + 两处删除，边界清晰）。

---

## 验证命令

代码修改后优先执行：

```bash
./scripts/dev.sh wsl-build --configure-only
./scripts/dev.sh win-build
```

涉及环境或路径问题时执行：

```bash
./scripts/dev.sh self-check
```

## 相关文档

- 项目路线图：[`roadmap.md`](roadmap.md)
- 审计报告：[`../audit/AUDIT-001-code-quality-audit-2026-07-20.md`](../audit/AUDIT-001-code-quality-audit-2026-07-20.md)（§8 问题总表，19 项 Deferred 追踪）
- 架构概览：[`../reference/architecture.md`](../reference/architecture.md)
- Phase 11 归档：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

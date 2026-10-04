# devpiano Current Iteration

> 用途：本文件只记录当前实施排期与任务验收；项目状态和长期路线以 [roadmap](roadmap.md) 为准。
> 本轮名称：**AUDIT-004 Phase：代码质量缺陷修复与消费者契约闭环**。
> 当前状态：**Phase 0/A/B/C/D 已完成（Phase D：2026-10-04），Phase E 待开始**。只勾选有直接证据的本轮任务；其他阶段保持未勾选，不以默认测试通过代替其消费者契约闭环。

## 1. 输入、范围与历史归档

- 问题与优先级基线：[AUDIT-004 第8章](../audit/AUDIT-004-code-quality-audit-2026-10-02.md#8-附录问题总表登记表)，实施方向参考其第5章，复现与未验证范围参考第4章。
- 本计划完整纳入原审计基线的 **54个未闭环唯一项**（P1 25、P2 26、P3 3，本轮未登记P0）。这是固定排期覆盖集合，不是当前剩余任务数；Phase 0/A/B/C/D 的任务已完成，其他原项仍按下方未勾选任务推进，原报告不回写。
- 保留报告原登记 ID/历史命名空间/known-issues 标题引用，不重编号、不把同名 `ERR-002` 混成同一项。下方每个原 ID 只安排到一个阶段；排期不改变原优先级，跨阶段关联只说明依赖。
- [Phase 35 完成计划](../archive/phase35-keyboard-expressive-dynamics-and-practice-infrastructure.md)已归档，保留当时完成勾选和契约差距；阶段交付完成不等于后续审计风险已消除。
- Phase 36/37 继续在 roadmap 保留规划。在P1、关键消费者回归及安全门禁达标前不开始新增声学/分区叠层功能，本轮不缩减到只修P1而遗漏其他登记项。
- 本页勾选用于实施任务进度；原始 AUDIT-004 是基线快照，不因排期/修复回写。修复过程和证据记在实施记录并同步 roadmap，正式复审携原问题身份与直接验证，不改历史报告。

## 2. 执行边界与顺序

1. 先完成 **Phase 0** 的最小安全验证前置，随后最优先执行 **Phase A** 的已有文件保护；阶段按依赖推进，不编造工期或发布日期。
2. P2/P3 若是同一消费者的依赖可随阶段前置处理，但仍保留原级别；其余低优先级收口不能拖延已经具备验证条件的P1。新增P0反证优先处理并暂停新增功能。
3. 固定拓扑仍为 `Performance Input -> Instrument -> Master -> Output`；不引入Patchbay、多轨DAW、外MIDI硬件路径、视频栈或新的审计平台。
4. 发音身份、瞬态修饰符、采样偏移/排序、实时无锁零分配、双看板单一事实源与实时/离线同构是必须验收的契约，不能以注释、setter已atomic、预分配声明或一次不崩溃代替证明。
5. 实作前按现行工具规则核对消费者/影响面和第三方API。需要改变文件格式、公开行为或ADR决策时先明确契约/迁移，不保留静默旧索引重解释或用改ADR掩盖违例。
6. 遵守源码7-bit ASCII与国际化分层；测试保留机制/行为，删除具体译文和自证oracle，不重钉新文案/实现文本。

## 3. 阶段总览

| 阶段 | 目标 | 依赖 / 排序 | 实施状态 |
| --- | --- | --- | --- |
| AUDIT-004 Phase 0 | 安全验证前置 | 无；只建立可安全执行的 Debug 消费者验证基础。 | 已完成，2026-10-02 |
| AUDIT-004 Phase A | 已有用户数据保护与持久化一致性 | Phase 0；同一阶段先处理 ERR-001、SEC-001、SEC-002。 | 已完成，2026-10-02 |
| AUDIT-004 Phase B | 文件准入与时间线数值安全 | Phase A 的所有权和失败保留约束；数值检查应先于打开输出。 | 已完成，2026-10-03 |
| AUDIT-004 Phase C | 插件与活动 DSP / Transport 所有权 | Phase 0/A；冻结已有数据与旧实例生命周期，先收敛竞态再改事件执行。 | 已完成，2026-10-03 |
| AUDIT-004 Phase D | 发音身份与采样级 Transport 边界 | Phase B/C；游标/倍率/设备时间域的所有权先于新增边界逻辑。 | 已完成，2026-10-04 |
| AUDIT-004 Phase E | 预设永久身份与实时/离线执行闭包 | Phase A/C/D；先 ARCH-003，再 ARCH-004；发布/通知容量与监听器清理同步设计。 | 待开始 |
| AUDIT-004 Phase F | 映射看板、交互与声学边界 | Phase A/D/E；明确点击输入身份与显示输出身份，不以重复矩阵变换修显示。 | 待开始 |
| AUDIT-004 Phase G | 诊断资源、ADR 与工程门禁收敛 | 贯穿实施；Phase A-F 的消费者回归已有证据后收口，不用压制诊断掩盖问题。 | 待开始 |
| AUDIT-004 Phase H | 契约文档与最终集成验收 | Phase 0及A-G；文档修订不得代替实现修复。 | 待开始 |

## 4. 逐项修复与消费者验收

每项先按原报告证据复核/安全复现，再实施最小修复和覆盖原触发的回归。表内修复目标与可观察验收是契约，已勾选任务的实际结果见对应实施记录；未勾选不代表已通过。静态反证、离屏组件、独立声部与真实硬件验证范围分别记录。

### AUDIT-004 Phase 0：安全验证前置 [已完成，2026-10-02]

**目标**：避免验证本身修改用户数据、漏跑或依赖可选优化；完成后立即进入 Phase A。

**依赖**：无；只建立可安全执行的 Debug 消费者验证基础。

默认测试先安全隔离用户目录、修 fixture 生命周期与类别执行；这一步不是把单独补跑当默认覆盖。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [x] | `TEST-001` | P1 | 音频fixture依赖可选 NRVO 保持自引用指针。由调用者从live buffer构造info，或fixture移动显式重绑定；用合法禁NRVO配置验证指针与实际音频行为。 | 调用者就地构造 info 并绑定 live buffer；禁 NRVO 构建下真实音频渲染通过，见 EVID-001/003。 |
| [x] | `TEST-002` | P1 | 默认路径测试触碰真实用户日志/预设目录。删除只测incidental默认路径的探针或置于隔离profile；全部文件测试使用已有ScopedTempDir，不再修改用户诊断历史。 | 默认测试前后真实用户目录清单、时间戳与文件哈希一致；私有临时目录零残留，见 EVID-002/003。 |
| [x] | `AUDIT-002 TEST-014` | P2 | 和弦识别7个子测试被默认类别过滤漏跑。迁入既有DevPiano/Core类别并确认默认日志包含7个子测试；区分编译接入和执行接入。 | ChordRecognition 的全部子测试已进入默认 DevPiano/Core 执行；没有用单独 category 补跑代替，见 EVID-002。 |

#### Phase 0 实施记录与直接验证（2026-10-02）

**基线与边界**：`6f69f6242d53bc4b29f1cd453937f7692adf6bbb`；只修改测试基础设施与相关文档，不修 Phase A-H，不修改生产音频/文件实现或历史审计。以下 EVID 编号仅属于本实施记录；原问题身份和优先级不变。

- `TEST-001`：`AudioEngineTest.cpp::makeBlock()` 只返回拥有存储的 `AudioBuffer`；所有调用点在 buffer 到达调用者后构造 `AudioSourceChannelInfo`。删除自引用 pair、错误的“具名返回保证消除”注释及对应 `StackAddressEscape` 抑制，不新增移动重绑定类型。
- `TEST-002`：删除 `DiagnosticsTest` 的默认日志目录探针与 `PerformancePresetTest` 的默认预设目录探针，保留隔离目录中的真实日志/预设行为。AcousticSettingsPersistence、LidAcousticsInteraction、MechanicalAcoustics、SpatialAcoustics、TemperamentSettingsPersistence、TouchVelocityCurve 的文件测试统一使用已有 `ScopedTempDir`，由其在 store/writer 析构后清理。
- `AUDIT-002 TEST-014`：`ChordRecognitionTest` 注册为 `DevPiano/Core`，复用既有 `TestRunner` 默认前缀选择，不增加特殊白名单、别名或第二套执行入口。

| 证据 | 原问题 / 验证程度 | 执行与输入 | 实际观察 | 范围与限制 |
| --- | --- | --- | --- | --- |
| EVID-001 | TEST-001；运行确认 | 同一 Windows MSVC Debug preset，新树 `build-win-msvc/audit004-phase0`，实际 `AudioEngineTest.cpp` 编译命令含 `/Zc:nrvo-`；同时构建应用与测试。 | MSVC 19.51 构建通过，音频 fixture 的默认行为用例在禁可选 NRVO 条件下通过。 | 不依赖 NRVO；未运行 Release 或 WSL 软件验证，不改原默认树的失效 Ninja 缓存。 |
| EVID-002 | TEST-002 / AUDIT-002 TEST-014；运行确认 | `ctest` 默认入口，无 `--category`/`--name`；TEMP/TMP 指向新建私有目录，真实 `%APPDATA%/DevPiano` 保持原位置。 | 本次观测 96 套件、97,933 通过断言、零失败；Chord 的七个子测试均有默认执行记录。真实目录前后清单、属性、修改时间和文件 SHA256 一致；私有目录零残留。 | 数字是此环境的执行记录，不是后续固定门槛；仅检查已写入本轮日志的默认选择范围，不宣称所有消费者问题已修。 |
| EVID-003 | TEST-001 / TEST-002；运行确认 | 禁 NRVO 的独立程序复用实际 fixture 工厂代码，调用 `AudioEngine::getNextAudioBlock()`，并在 ScopedTempDir 中执行生产日志及预设写出/读取、正常退出和受控异常展开。 | `info_live=1 rendered_nonzero=1024 finite=1 muted=1 isolated_log=1 isolated_preset=1 normal_cleanup=1 unwind_cleanup=1`；真实用户目录未变、私有临时目录零残留。 | 验证真实 CPU 音频数据和文件行为；不外推为真实声卡、VST3、桌面 UI 或完整实时 SLA 已验证。 |
| EVID-004 | 环境 / 工具限制 | `self-check`、`format --check`；clangd 重载后使用绝对源码路径采样 diagnostics。 | 环境与格式检查通过；音频 fixture 采样 diagnostics 无错误。codegraph 未挂载；LSP references 仍遗漏局部调用点，已报告工具问题。 | 不以 LSP 采样代替编译/全量 tidy。全量 tidy 已知失败属于 Phase G，当前不是整个 AUDIT-004 迭代边界，未为确认旧失败而重跑。 |

**可复建构建与默认执行配方**（镜像树 Developer PowerShell for VS；新目录首次配置，已有缓存不会重新采用 `CXXFLAGS`）：

```powershell
Set-Location 'G:\source\projects\devpiano'
$build = 'build-win-msvc\audit004-phase0'
$env:CXXFLAGS = (($env:CXXFLAGS, '/Zc:nrvo-') -join ' ').Trim()
cmake --preset windows-msvc-debug -B $build -DBUILD_TESTS=ON
cmake --build $build --target devpiano_tests devpiano --parallel 4
```

1. 使用项目 `./scripts/dev.sh win-build --sync-only` 同步源码；独立树放在被同步脚本保留的 `build-win-msvc/` 下，避免顶层额外目录被 `/MIR` 清理。本轮默认构建缓存原失败没有被替代构建改写。
2. 在新树 `compile_commands.json` 中核对 `AudioEngineTest.cpp` 的实际 command 包含 `/Zc:nrvo-`；此选项只关闭可选 NRVO，不关闭标准要求的直接返回值消除。
3. 默认执行前后读取 `[Environment]::GetFolderPath('ApplicationData')/DevPiano` 的目录清单、文件 SHA256、长度、属性和修改时间，包含目录不存在的状态；只读比较，不用真实日志制造截减反例。JUCE Windows 使用系统 `CSIDL_APPDATA`，仅改 `APPDATA` 环境变量不能作为隔离证明。
4. 创建系统临时目录下 `devpiano-phase0-<GUID>`，把当前进程 `TEMP`、`TMP` 指向它，再执行 `ctest --test-dir $build -C Debug --verbose`。检查 `Testing/Temporary/LastTest.log` 中 Chord 所有子测试的开始/完成记录与最终零失败，不能只查套件注册或单独补跑。
5. 检查私有目录无子项后删除该自有目录；本轮保存的执行输出已提炼到 EVID-002，不依赖临时 GUID 路径继续存在。

**独立程序的可复建输入**：下面是 EVID-003 已执行的完整源码；`makeBlock()` 与本轮测试工厂一致，日志通过公共 Logger 入口分发。

```cpp
#include <JuceHeader.h>
#include "Audio/AudioEngine.h"
#include "Diagnostics/DevPianoLogger.h"
#include "Layout/PerformancePreset.h"
#include "tests/TestHelpers.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

juce::AudioBuffer<float> makeBlock(int numChannels, int numSamples) {
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    buffer.clear();
    return buffer;
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    AudioEngine engine;
    engine.prepareToPlay(512, 44100.0);
    engine.setMasterGain(1.0f);
    const auto warmup = AudioEngine::calculateWarmupBlockCount(44100.0, 512);
    for (int i = 0; i < warmup; ++i) {
        auto buffer = makeBlock(2, 512);
        const juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
        engine.getNextAudioBlock(info);
    }
    engine.getKeyboardState().noteOn(1, 60, 0.8f);
    auto buffer = makeBlock(2, 512);
    const juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
    engine.getNextAudioBlock(info);
    int nonZero = 0;
    bool finite = true;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            const auto value = buffer.getSample(channel, sample);
            nonZero += value != 0.0f;
            finite = finite && std::isfinite(value);
        }
    }
    engine.setMasterGain(0.0f);
    engine.getNextAudioBlock(info);
    const bool muted = buffer.getMagnitude(0, buffer.getNumSamples()) == 0.0f;
    engine.releaseResources();

    bool loggerWritten = false;
    bool presetLoaded = false;
    juce::File ownedDirectory;
    {
        devpiano::test::ScopedTempDir temp("phase0-consumer-smoke");
        ownedDirectory = temp.get();
        const auto logFile = temp.getChildFile("isolated.log");
        {
            devpiano::diagnostics::DevPianoLogger logger(logFile, 1024);
            juce::Logger::setCurrentLogger(&logger);
            juce::Logger::writeToLog("phase0-isolated-write");
            juce::Logger::setCurrentLogger(nullptr);
        }
        loggerWritten = logFile.loadFileAsString().contains("phase0-isolated-write");
        auto preset = devpiano::layout::makeDefaultPreset();
        preset.name = "phase0-private-preset";
        const auto presetFile = temp.getChildFile("private.devpiano.preset");
        if (devpiano::layout::savePreset(preset, presetFile)) {
            const auto loaded = devpiano::layout::loadPreset(presetFile);
            presetLoaded = loaded.has_value() && loaded->name == preset.name;
        }
    }
    const bool normalCleanup = ownedDirectory.exists() == false;
    juce::File unwoundDirectory;
    try {
        devpiano::test::ScopedTempDir temp("phase0-unwind-smoke");
        unwoundDirectory = temp.get();
        if (!temp.getChildFile("owned.tmp").replaceWithText("owned")) {
            return 2;
        }
        throw std::runtime_error("phase0-controlled-unwind");
    } catch (const std::runtime_error&) {
    }
    const bool unwindCleanup = unwoundDirectory.exists() == false;
    const bool liveBuffer = info.buffer == &buffer;
    std::cout << "PHASE0_SMOKE info_live=" << liveBuffer
              << " rendered_nonzero=" << nonZero << " finite=" << finite
              << " muted=" << muted << " isolated_log=" << loggerWritten
              << " isolated_preset=" << presetLoaded
              << " normal_cleanup=" << normalCleanup
              << " unwind_cleanup=" << unwindCleanup << '\n';
    return liveBuffer && nonZero > 0 && finite && muted && loggerWritten
               && presetLoaded && normalCleanup && unwindCleanup ? 0 : 1;
}
```

**独立程序构建/运行配方**：在 `$build/phase0-smoke/phase0_smoke.cpp` 保存上述代码，目录只包含自有探针文件；以下参数全部来自该 Debug 树，不猜 SDK 安装路径。

- 读取 `compile_commands.json` 中 `AudioEngineTest.cpp` 的唯一 command，保持定义、include、PCH、运行库及 `/Zc:nrvo-`；仅把末尾源文件 token、`/Fo`、`/Fd` 改为 `phase0-smoke/phase0_smoke.cpp`、`phase0-smoke/phase0_smoke.obj`、`phase0-smoke/phase0_compile.pdb`，写入 `phase0-smoke/compile-smoke.cmd`。Windows command 的源路径可用反斜杠，不直接假设其文本与 JSON 的 file 字段相同。
- 在新 `phase0-smoke.ninja` 中 `include build.ninja`，复用原测试 executable 的 linker rule 与输入/库，仅去除 `source/tests/` 下的测试 object，加入 `phase0-smoke/phase0_smoke.obj`。保留生产/JUCE object 和原库依赖，不运行任何 UnitTest。
- 将新 edge 的 `OBJECT_DIR`、`TARGET_SUPPORT_DIR` 指向 `phase0-smoke`，`TARGET_COMPILE_PDB` 指向其 compile PDB，`TARGET_FILE`、`TARGET_IMPLIB`、`TARGET_PDB`、`RSP_FILE` 分别指向此目录下 `phase0_smoke.exe/.lib/.pdb/.rsp`；其余 CONFIG、FLAGS、LINK_FLAGS、LINK_LIBRARIES、PRE_LINK、POST_BUILD 与原 edge 相同。不覆盖原 test linker edge、response file 或 manifest。
- 在该构建目录运行 `cmd.exe /D /C phase0-smoke\compile-smoke.cmd`；从 `CMakeCache.txt` 的 `CMAKE_MAKE_PROGRAM` 取 Ninja，运行 `ninja -f phase0-smoke.ninja phase0-smoke\phase0_smoke.exe`；同 EVID-002 设置私有 TEMP/TMP、读取真实用户目录快照后运行 `phase0-smoke\phase0_smoke.exe`，检查零退出码和 EVID-003 输出。
- 保留源码、配方和关键输出后清理自有探针及私有临时目录，构建缓存和原用户数据不作破坏性清理。本轮临时源码/程序已清理；此段是持久复建入口。

**探针预置错误单列**：初次独立程序直接调用受保护 `DevPianoLogger::logMessage()` 未编译；随后发现 eval 的 shell-escape 转换污染了字符串中的 `= !...`，已报告工具问题，并改为上方公共 Logger 入口与显式 `exists() == false`。这些是探针/工具错误，不作为产品失败；EVID-003 只引用修正后实际运行结果。


### AUDIT-004 Phase A：已有用户数据保护与持久化一致性 [已完成，2026-10-02]

**目标**：最先关闭会覆盖/删除原文件、回滚新设置或丢绑定编辑的路径。

**依赖**：Phase 0；同一阶段先处理 ERR-001、SEC-001、SEC-002。

已有目标写入统一事务边界；预设/Take/current identity 与设置深拷贝、同步/防抖提交保持一致。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [x] | `ERR-001` | P1 | 已有导出目标不是事务替换：成功追加，失败删除原文件。统一同目录 TemporaryFile 写出、关闭 writer 后替换，仅清理任务自有临时文件；保留取消/失败时原目标。 | MIDI/内置及测试插件 WAV 连续覆盖读到新内容；提交前取消、参数拒绝、目标锁定/替换失败保留原字节并清理临时文件，见 EVID-006。 |
| [x] | `SEC-001` | P1 | 预设重命名可覆盖另一预设或删除自身目标。比较规范化后的源/目标路径；同路径不得删除，独立已有目标先确认，失败保留两份原始数据。 | 实际已有目标确认/取消、Windows 大小写同路径、非法字符规范化及失败回滚通过；原数据保留，见 EVID-007。 |
| [x] | `SEC-002` | P1 | 原生文件绑定未随 Take 替换/Save As 更新。建立 Take 与 backing file/metadata 的一致所有权；替换时解除旧绑定，成功打开/保存后绑定新文件；验证 A 原字节不被 B 元数据编辑改写。 | 录制/导入 B、Save As C、信息编辑与失败/延迟结果均调用统一会话操作；A 原字节保留，实际主界面导入/信息路径通过，见 EVID-008。 |
| [x] | `ERR-002` | P1 | 即时同步保存未取代旧防抖快照。同步 save 成功后取代/撤销相同store的待写payload；定义直接保存与防抖保存的顺序语义。 | 真实消息循环中旧 timer 不回滚新缓存；后续调度及独立 store 正常提交，失败同步保存保留原待写任务，见 EVID-009。 |
| [x] | `QUAL-006` | P2 | Phase35字段在 SettingsModel 深拷贝中遗漏。补齐全部持久化字段的复制语义，同时保留 XML 独立所有权；验证首次及再次防抖写出的真实值。 | 复制构造/赋值和首次/再次快照保留非默认练琴字段（含 173 BPM）；源 XML 修改不污染已排队快照，见 EVID-009。 |
| [x] | `QUAL-016` | P2 | 启动恢复预设未设置控制器当前身份。统一启动/用户选择的预设激活操作及当前身份，保证列表选择、自动保存和运行布局一致。 | 实际启动恢复 B（继承内置布局 ID），不手动选择即编辑音高/通道，运行绑定与自动保存文件均为 B，见 EVID-007。 |

#### Phase A 实施记录与直接验证（2026-10-02）

**基线与边界**：`91d4abaf17c04815dc8752f10682f5e18ef8f442`（Phase 0 本地交付）；仅本阶段六项及其直接消费者、相关测试/文档，不改历史 AUDIT/ADR，不启动 Phase B。文件格式不变，未增加正式 profile/CLI 模式或测试专用公开 API。

- `ERR-001`：MIDI、内置/插件 WAV 先写同目录 `TemporaryFile`；检查写入/flush、关闭 writer/流后替换。WAV 提交前再接受一次进度取消；`failExport()` 不删除目标。原审计已有运行反证沿用，不为确认旧失败而再次破坏文件。
- `SEC-001`：规范化后用 JUCE `File` 身份比较，独立已有目标先确认。同路径只更新；不同路径先完成内容，再暂存源、提交目标，失败恢复源。若恢复或成功提交后的备份清理也被文件系统阻止，保留原源备份并明确日志路径；不承诺跨文件断电原子性。
- `SEC-002`：`RecordingSession` 的 `detachForNewRecording()`、`commitImportedMidi()`、`openFromFile()`、`saveToFile()`、`updateMetadata()` 是实际录制/导入/打开/保存/信息路径的共同操作；元数据和 Save As 在写入成功后提交，`takeGeneration` 拒绝另一 Take 的延迟结果。移除无调用的旧替换包装。
- `ERR-002`：成功同步保存停止此 store 的旧防抖任务；失败同步保存保留原任务。timer 将 optional payload 移到局部后保存，避免被同步保存取消时悬空。不增设自动重试；异步落盘失败仍由既有日志报告。
- `QUAL-006`：复制构造与赋值沿用单一实现，补齐练琴字段；XML 仍独立克隆。回归看真实 timer 后的文件，不保留只窥视 pendingPayload 的公开测试接口。
- `QUAL-016`：启动经 `applyPresetById()` 进入选择使用的激活操作，提交布局前设置当前/持久化身份。入口明确 `fileBacked`，用户保存的内置衍生布局不会被误判为无文件的 Default。

| 证据 | 原问题 / 验证程度 | 执行与输入 | 实际观察 | 范围与限制 |
| --- | --- | --- | --- | --- |
| EVID-005 | 构建/默认门禁；运行确认 | 复用 Phase 0 的 Windows Debug 子树 `build-win-msvc/audit004-phase0`，保留 `/Zc:nrvo-`，构建应用和测试；默认 `ctest`，无 category/name 补跑。 | 最终应用/测试构建通过，默认项目测试零失败，Chord 完整执行；真实用户目录前后清单、修改时间、属性和 SHA256 一致，私有 TEMP/TMP 零残留。 | 原默认 Ninja 缓存不改；未运行 Release/WSL 软件验证或重跑已知全量 tidy 失败；不是整个 AUDIT-004 迭代边界。 |
| EVID-006 | ERR-001；运行确认 | 真实 MIDI/WAV 导出连续替换已有目标；WAV 最终进度取消、锁定目标阻止替换、无效参数的 `WavExportTask::runSync()`；默认插件渲染测试也重复覆盖并取消。 | 新 MIDI 音高与 WAV header/payload 可读取；取消/替换失败/拒绝保留原字节；没有任务临时文件残留。 | 插件测试使用现有测试乐器，不等同真实厂商 VST3；未执行磁盘耗尽、强杀、断电或慢插件终止。 |
| EVID-007 | SEC-001 / QUAL-016；文件及实际界面确认 | 私有 profile 下构造本轮实际 `MainComponent`，恢复继承内置 layout.id 的 B；不手动选择即编辑音高/通道；实际 Rename 到 A、取消确认、仅大小写重命名；默认文件回归覆盖规范化/允许覆盖/锁定源/失败回滚。 | 下拉 B、运行绑定与立即自动保存均为 B，A 原字节保留；实际显示 Overwrite/Cancel 确认，取消无写入；大小写重命名后新名称与绑定仍可加载。 | profile 路径只在探针本身进程中定向；使用真实控件 callback/triggerClick 并观察像素图，不替换控制器、布局或文件算法。回滚自身失败的双故障只保证保留备份，未人为破坏存储介质。 |
| EVID-008 | SEC-002；文件及实际界面确认 | 默认测试调用生产会话方法，打开 A→录制/导入 B→Save As C→编辑信息；失败打开/保存、跨 Take 延迟结果；实际 Main 的 filesDropped A→MIDI B→Info 编辑。 | A 原字节保持；C 保存当前 B 事件和新信息，失败保留绑定；旧 generation 不能写入新 Take；实际导入/Info 不重写 A。 | 不以字段复制或模拟同条件 if 作为证明；原生系统文件选择器人工键入/关闭未重演，验证其调用的共同保存操作与 generation 守卫。 |
| EVID-009 | ERR-002 / QUAL-006；真实 timer/文件确认 | 旧快照排队→同步新缓存→消息循环；首次及替换快照、173 BPM 等非默认值、源 XML 后续修改、失败同步写入与不同 store。 | 新缓存不回滚；后续调度正常；全部练琴字段与独立 XML 落盘保持；失败同步保存后的旧任务仍可提交。 | 仅按既有消息线程保存契约执行，不引入跨线程设置提交 API。 |
| EVID-010 | 环境/工具与格式；直接检查 | `self-check`、`wsl-build --configure-only`、`format --check`；clangd 重载后的 controller diagnostics；本轮新增 C++ 行 ASCII 和历史报告哈希检查。 | 环境/格式通过，最终 controller diagnostics 无错误，新增 C++ 行无裸非 ASCII；历史 AUDIT 保持不变。 | codegraph 未挂载；重载前 LSP 引用遗漏与编译数据库错误已报告，不用该采样冒充全量静态检查。 |

**精确构建/默认执行配方**（镜像树 Developer PowerShell for VS；先在 WSL 用项目脚本 `./scripts/dev.sh win-build --sync-only` 同步，不清理旧缓存）：

```powershell
Set-Location 'G:\source\projects\devpiano'
$build = 'build-win-msvc\audit004-phase0'
cmake --preset windows-msvc-debug -B $build -DBUILD_TESTS=ON
cmake --build $build --target devpiano_tests devpiano --parallel 4
ctest --test-dir $build -C Debug --verbose
```

默认执行与独立程序外层保护沿用 Phase 0 EVID-002：前后读取真实 `[Environment]::GetFolderPath('ApplicationData')/DevPiano` 清单/属性/修改时间/SHA256；TEMP/TMP 仅指向新私有 GUID 目录，结束检查空目录后删除。不能仅靠修改 APPDATA 环境变量断言 JUCE 的路径已隔离。

**独立实际消费者与界面探针的完整复建源码**：保存为 `$build/phasea-smoke/phasea_smoke.cpp`。`ProfileDirectoryScope` 只在该一次性进程的 import slot 中将 `SHGetSpecialFolderPathW(CSIDL_APPDATA)` 定向到自有目录，其余 special folder 调用原 API；在构造 `MainComponent` 前必须检查 JUCE 实际解析路径，退出恢复 import slot，不修改系统 profile、主树源码或子模块。它只提供隔离路径输入，MIDI/WAV/设置/控制器/界面全部链接本轮应用真实 object。只允许用于此可丢弃验证，不作为产品能力。

参考官方契约：[JUCE TemporaryFile](https://docs.juce.com/master/classjuce_1_1TemporaryFile.html)、[OutputStream::writeText](https://docs.juce.com/master/classjuce_1_1OutputStream.html)、[Windows PE import directory](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#the-import-directory-table)、[VirtualProtect](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect)。此处进程内路径定向仅为保护真实用户数据的探针输入，不是被测功能的替代实现。

```cpp
#include <JuceHeader.h>
#include "MainComponent.h"
#include "Layout/PerformancePreset.h"
#include "Recording/MidiFileExporter.h"
#include "Recording/PerformanceFile.h"
#include "Recording/RecordingSessionController.h"
#include "Recording/WavFileExporter.h"
#include "Export/WavExportTask.h"
#include "Settings/SettingsStore.h"
#include <windows.h>
#include <shlobj.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void pump(int milliseconds = 100) {
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + milliseconds;
    while (juce::Time::getMillisecondCounterHiRes() < deadline) {
        MSG message {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    }
}
class PrivateDirectory {
public:
    PrivateDirectory() : directory(juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("phasea-consumer-" + juce::Uuid().toString())) {
        require(!directory.exists() && directory.createDirectory().wasOk(), "owned directory creation failed");
    }
    ~PrivateDirectory() {
        std::cout << "PHASE_A_PRIVATE_FILES_CLEAN=" << directory.deleteRecursively(false) << '\n';
    }
    juce::File getChildFile(const juce::String& name) const { return directory.getChildFile(name); }
    const juce::File& get() const { return directory; }
private:
    juce::File directory;
};
class ProfileDirectoryScope {
public:
    ~ProfileDirectoryScope() {
        if (slot == nullptr) return;
        DWORD protection = 0;
        const bool restored = VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection) != FALSE;
        if (restored) {
            *slot = reinterpret_cast<ULONG_PTR>(original);
            DWORD ignored = 0;
            VirtualProtect(slot, sizeof(*slot), protection, &ignored);
        }
        std::cout << "PHASE_A_PROFILE_SCOPE_RESTORED=" << restored << '\n';
    }
    bool redirect(const juce::File& directory) {
        privatePath = directory.getFullPathName().toWideCharPointer();
        if (privatePath.size() >= MAX_PATH) return false;
        auto* base = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        const auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if (rva == 0) return false;
        auto* imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + rva);
        for (; imports->Name != 0; ++imports) {
            if (imports->OriginalFirstThunk == 0) continue;
            auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->OriginalFirstThunk);
            auto* entries = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->FirstThunk);
            for (; names->u1.AddressOfData != 0; ++names, ++entries) {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                auto* imported = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                if (std::strcmp(imported->Name, "SHGetSpecialFolderPathW") != 0) continue;
                auto* candidate = &entries->u1.Function;
                DWORD protection = 0;
                if (!VirtualProtect(candidate, sizeof(*candidate), PAGE_READWRITE, &protection)) return false;
                original = reinterpret_cast<NativeFolder>(static_cast<ULONG_PTR>(*candidate));
                *candidate = reinterpret_cast<ULONG_PTR>(&privateFolder);
                DWORD ignored = 0;
                VirtualProtect(candidate, sizeof(*candidate), protection, &ignored);
                slot = candidate;
                return true;
            }
        }
        return false;
    }
private:
    using NativeFolder = BOOL (WINAPI*)(HWND, LPWSTR, int, BOOL);
    static BOOL WINAPI privateFolder(HWND window, LPWSTR destination, int kind, BOOL create) {
        if (kind != CSIDL_APPDATA) return original(window, destination, kind, create);
        std::copy(privatePath.begin(), privatePath.end(), destination);
        destination[privatePath.size()] = 0;
        return TRUE;
    }
    ULONG_PTR* slot = nullptr;
    static inline NativeFolder original = nullptr;
    static inline std::wstring privatePath;
};

template <typename T> T* find(juce::Component& root, const juce::String& id) {
    if (root.getComponentID() == id) {
        if (auto* found = dynamic_cast<T*>(&root)) return found;
    }
    for (int i = 0; i < root.getNumChildComponents(); ++i) {
        if (auto* found = find<T>(*root.getChildComponent(i), id)) return found;
    }
    return nullptr;
}
template <typename T> T* findType(juce::Component& root) {
    if (auto* found = dynamic_cast<T*>(&root)) return found;
    for (int i = 0; i < root.getNumChildComponents(); ++i) {
        if (auto* found = findType<T>(*root.getChildComponent(i))) return found;
    }
    return nullptr;
}
juce::Component& modal() {
    auto* component = juce::Component::getCurrentlyModalComponent();
    require(component != nullptr, "missing real modal");
    return *component;
}
void click(juce::Component& root, const juce::String& id) {
    auto* button = find<juce::Button>(root, id);
    require(button != nullptr && button->isEnabled(), "missing or disabled real button");
    button->triggerClick();
    pump();
}
class ModalCleanup {
public:
    ~ModalCleanup() {
        while (auto* component = juce::Component::getCurrentlyModalComponent()) component->exitModalState(0);
        pump();
    }
};
void snapshot(juce::Component& component, const juce::File& output) {
    auto image = component.createComponentSnapshot(component.getLocalBounds());
    juce::FileOutputStream stream(output);
    require(stream.openedOk(), "snapshot open failed");
    require(juce::PNGImageFormat().writeImageToStream(image, stream), "snapshot encode failed");
}

devpiano::recording::RecordingTake take(int pitch) {
    using namespace devpiano::recording;
    RecordingTake result;
    result.sampleRate = 44100.0;
    result.lengthSamples = 4410;
    result.events = {{0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                     juce::MidiMessage::noteOn(1, pitch, 0.8f)},
                    {4410, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                     juce::MidiMessage::noteOff(1, pitch)}};
    return result;
}
void exerciseFiles(const juce::File& directory) {
    using namespace devpiano::exporting;
    using namespace devpiano::recording;
    const auto midi = directory.getChildFile("replace.mid");
    require(exportTakeAsMidiFile(take(60), midi), "initial MIDI failed");
    require(exportTakeAsMidiFile(take(72), midi), "replacement MIDI failed");
    juce::MidiFile parsed;
    { juce::FileInputStream stream(midi); require(parsed.readFrom(stream), "MIDI read failed"); }
    bool fresh = false;
    for (int i = 0; i < parsed.getTrack(0)->getNumEvents(); ++i) {
        const auto& message = parsed.getTrack(0)->getEventPointer(i)->message;
        if (message.isNoteOn()) { require(message.getNoteNumber() == 72, "old MIDI content survived"); fresh = true; }
    }
    require(fresh, "replacement note missing");
    const auto wav = directory.getChildFile("replace.wav");
    WavExportOptions options;
    options.builtinTone = SettingsModel::BuiltinTone::sine;
    options.sampleRate = 48000.0;
    require(exportTakeAsWavFile(take(60), wav, options), "initial WAV failed");
    options.sampleRate = 44100.0;
    require(exportTakeAsWavFile(take(72), wav, options), "replacement WAV failed");
    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wav.createInputStream().release(), true));
        require(reader != nullptr && reader->sampleRate == 44100.0, "old WAV header survived");
        juce::AudioBuffer<float> audio(2, 4096);
        require(reader->read(&audio, 0, 4096, 0, true, true), "WAV payload read failed");
        require(audio.getMagnitude(0, 0, 4096) > 0.01f, "new WAV payload silent");
    }
    juce::MemoryBlock original;
    require(wav.loadFileAsData(original), "WAV bytes read failed");
    require(!exportTakeAsWavFile(take(60), wav, options, [](double progress) { return progress < 1.0; }), "cancel committed");
    juce::MemoryBlock cancelled;
    require(wav.loadFileAsData(cancelled) && cancelled == original, "cancel damaged original");
    { juce::FileOutputStream lock(wav);
      require(lock.openedOk(), "target lock failed");
      require(!exportTakeAsWavFile(take(60), wav, options), "locked target overwritten"); }
    juce::MemoryBlock locked;
    require(wav.loadFileAsData(locked) && locked == original, "replacement failure damaged original");
    options.blockSize = 0;
    WavExportTask task(take(60), wav, options, nullptr, nullptr);
    require(!task.runSync(), "rejected task succeeded");
    juce::MemoryBlock rejected;
    require(wav.loadFileAsData(rejected) && rejected == original, "rejected task deleted original");
    std::cout << "PHASE_A_EXPORT midi_replace=1 wav_replace=1 audible=1 cancel_preserved=1 locked_preserved=1 rejected_task_preserved=1\n";
}
void exerciseSettings(const juce::File& directory) {
    const auto file = directory.getChildFile("settings.xml");
    SettingsStore store(file);
    SettingsModel model;
    model.lastPluginName = "old-cache";
    store.scheduleSave(model, 1);
    model.lastPluginName = "new-cache";
    model.metronomeEnabled = true;
    model.metronomeBpm = 173.0;
    model.metronomeTimeSignature = devpiano::core::TimeSignature::sixEight;
    model.metronomeVolume = 0.83f;
    model.metronomeCountIn = devpiano::core::CountInBars::twoBars;
    model.cadenceDynamicsEnabled = false;
    model.velocityHumanizeAmount = 0.08f;
    model.baseVelocityBias = 0.12f;
    model.audioDeviceState = juce::parseXML("<DEVICE value=\"initial\"/>");
    model.knownPluginListState = juce::parseXML("<CACHE value=\"new-cache\"/>");
    require(store.save(model), "sync settings save failed");
    pump(150);
    SettingsModel loaded;
    SettingsStore(file).load(loaded);
    require(loaded.lastPluginName == "new-cache" && loaded.metronomeBpm == 173.0, "old timer rolled back settings");
    store.scheduleSave(model, 1);
    model.audioDeviceState->setAttribute("value", "mutated");
    model.knownPluginListState->setAttribute("value", "mutated");
    pump(150);
    SettingsStore(file).load(loaded);
    require(loaded.audioDeviceState->getStringAttribute("value") == "initial", "device XML snapshot aliased");
    require(loaded.knownPluginListState->getStringAttribute("value") == "new-cache", "cache XML snapshot aliased");
    model.metronomeBpm = 201.0;
    store.scheduleSave(model, 1000);
    model.metronomeBpm = 149.0;
    store.scheduleSave(model, 1);
    pump(150);
    SettingsStore(file).load(loaded);
    require(loaded.metronomeEnabled && loaded.metronomeBpm == 149.0
            && loaded.metronomeTimeSignature == devpiano::core::TimeSignature::sixEight
            && std::abs(loaded.metronomeVolume - 0.83f) < 0.001f
            && loaded.metronomeCountIn == devpiano::core::CountInBars::twoBars
            && !loaded.cadenceDynamicsEnabled && std::abs(loaded.velocityHumanizeAmount - 0.08f) < 0.001f
            && std::abs(loaded.baseVelocityBias - 0.12f) < 0.001f, "practice snapshot lost fields");
    std::cout << "PHASE_A_SETTINGS sync_supersedes=1 real_timer=1 first_snapshot=1 replacement_snapshot=1 independent_xml=1\n";
}
void exerciseOwner(const juce::File& directory, const juce::File& images) {
    using namespace devpiano::layout;
    using namespace devpiano::recording;
    auto a = makeDefaultPreset(); a.name = "PresetA"; a.layout.name = a.name;
    auto b = makeDefaultPreset(); b.name = "PresetB"; b.layout.name = b.name;
    b.layout.bindings = {{65, "A", {devpiano::core::KeyActionType::note, devpiano::core::KeyTrigger::keyDown, 73, 1, 0.55f}}};
    const auto presetDir = getPresetDirectory();
    const auto fileA = resolvePresetFile(a.name, presetDir);
    const auto fileB = resolvePresetFile(b.name, presetDir);
    require(savePreset(a, fileA) && savePreset(b, fileB), "private preset seed failed");
    const auto originalPresetA = fileA.loadFileAsString();
    SettingsModel settings;
    settings.lastActivePresetId = "PresetB";
    settings.languageCode = "en";
    settings.masterGain = 0.0f;
    settings.metronomeEnabled = false;
    { SettingsStore initial; require(initial.save(settings), "private startup settings failed"); }
    MainComponent owner;
    ModalCleanup cleanup;
    owner.setSize(1280, 800);
    owner.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    owner.setVisible(true);
    pump(200);
    auto* combo = find<juce::ComboBox>(owner, "preset-combo");
    require(combo != nullptr && combo->getText() == "PresetB", "startup selection not B");
    require(owner.getAppSettings().lastActivePresetId == "PresetB", "startup identity not B");
    auto* keyboard = findType<CustomKeyboard>(owner);
    require(keyboard != nullptr && static_cast<bool>(keyboard->onBindingEditRequested), "actual keyboard editor missing");
    keyboard->onBindingEditRequested(73);
    pump();
    auto* note = find<juce::Slider>(modal(), "note-slider");
    auto* channel = find<juce::ComboBox>(modal(), "channel-combo");
    require(note != nullptr && note->getValue() == 73.0 && channel != nullptr, "runtime binding not restored from B");
    note->setValue(74.0);
    channel->setSelectedId(4);
    click(modal(), "dialog-ok-btn");
    auto savedB = loadPreset(fileB);
    require(savedB.has_value() && savedB->layout.bindings[0].action.midiNote == 74
            && savedB->layout.bindings[0].action.midiChannel == 4, "immediate binding autosave did not target B");
    require(fileA.loadFileAsString() == originalPresetA, "B autosave touched A");
    snapshot(owner, images.getChildFile("phasea-startup-binding.png"));
    const auto unchangedB = fileB.loadFileAsString();
    click(owner, "rename-preset-btn");
    auto* renameEditor = find<juce::TextEditor>(modal(), "dialog-editor");
    require(renameEditor != nullptr, "real rename input missing");
    renameEditor->setText("PresetA");
    click(modal(), "dialog-ok-btn");
    require(juce::Component::getCurrentlyModalComponent() != nullptr, "independent collision had no confirmation");
    snapshot(modal(), images.getChildFile("phasea-rename-confirm.png"));
    click(modal(), "dialog-cancel-btn");
    require(fileA.loadFileAsString() == originalPresetA && fileB.loadFileAsString() == unchangedB, "declined rename changed originals");
    click(owner, "rename-preset-btn");
    renameEditor = find<juce::TextEditor>(modal(), "dialog-editor");
    require(renameEditor != nullptr, "second rename input missing");
    renameEditor->setText("presetb");
    click(modal(), "dialog-ok-btn");
    require(juce::Component::getCurrentlyModalComponent() == nullptr, "case-only rename requested collision permission");
    const auto caseFile = resolvePresetFile("presetb", presetDir);
    auto renamed = loadPreset(caseFile);
    require(renamed.has_value() && renamed->name == "presetb" && renamed->layout.bindings[0].action.midiNote == 74, "case-only rename lost data");
    const auto nativeA = directory.getChildFile("nativeA.devpiano");
    const auto midiB = directory.getChildFile("importB.mid");
    require(savePerformanceFile(take(60), nativeA, {}), "native A seed failed");
    require(devpiano::exporting::exportTakeAsMidiFile(take(67), midiB), "import B seed failed");
    const auto bytesA = nativeA.loadFileAsString();
    owner.filesDropped(juce::StringArray{nativeA.getFullPathName()}, 0, 0); pump();
    owner.filesDropped(juce::StringArray{midiB.getFullPathName()}, 0, 0); pump();
    click(owner, "song-info-btn");
    auto* title = find<juce::TextEditor>(modal(), "title-editor");
    require(title != nullptr, "actual song info editor missing");
    title->setText("Current B Information");
    click(modal(), "dialog-ok-btn");
    require(nativeA.loadFileAsString() == bytesA, "real import then info rewrote native A");
    std::cout << "PHASE_A_UI restored_B=1 inherited_builtin_layout=1 runtime_binding=1 immediate_autosave_B=1 rename_confirm=1 declined_preserved=1 case_only_preserved=1 real_take_import_detached=1\n";
}
int main() {
    try {
        PrivateDirectory privateDirectory;
        const auto privateAppData = privateDirectory.getChildFile("Roaming");
        require(privateAppData.createDirectory().wasOk(), "private Roaming creation failed");
        ProfileDirectoryScope profile;
        require(profile.redirect(privateAppData), "process-only profile redirect failed");
        const auto actualAppData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
        std::cout << "PHASE_A_RESOLVED_PROFILE=" << actualAppData.getFullPathName() << '\n';
        require(actualAppData == privateAppData, "unsafe real profile; owner not constructed");
        juce::ScopedJuceInitialiser_GUI gui;
        exerciseFiles(privateDirectory.get());
        exerciseSettings(privateDirectory.get());
        const auto images = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        exerciseOwner(privateDirectory.get(), images);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PHASE_A_SMOKE_ERROR=" << error.what() << '\n';
        return 2;
    }
}
```

**独立程序构建/运行配方**：

1. 读取此树 `compile_commands.json` 中 `MainComponent.cpp` 的唯一 command，保留 app 的定义、include、PCH、运行库和 `/Zc:nrvo-`；仅替换末尾 `/c` 源 token、`/Fo`、`/Fd` 为 `phasea-smoke/phasea_smoke.cpp/.obj` 与 compile PDB，写入 `phasea-smoke/compile-smoke.cmd`。主应用未启用 modal loops，故探针使用源码中的原生消息 pump，不向 app 加宏或更改 PCH。
2. 在 `phasea-smoke.ninja` 中 `include build.ninja`，复制实际 `DevPiano.exe` 的 linker rule、输入及库；只去掉 `source/Main.cpp.obj` 应用入口，加入探针 object，将 `LINK_FLAGS` 的 `/subsystem:windows` 改为 `/subsystem:console`。不替换任何业务/JUCE object，不覆盖原 app edge。
3. 仅将 `OBJECT_DIR`、`TARGET_SUPPORT_DIR`、`TARGET_COMPILE_PDB`、`TARGET_FILE`、`TARGET_IMPLIB`、`TARGET_PDB`、`RSP_FILE` 指向自有 `phasea-smoke` 目录；保留其余 CONFIG/FLAGS/LINK_LIBRARIES/PRE_LINK/POST_BUILD。
4. 在该树执行 `cmd.exe /D /C phasea-smoke\compile-smoke.cmd`，从 `CMakeCache.txt` 的 `CMAKE_MAKE_PROGRAM` 获取 Ninja，执行 `ninja -f phasea-smoke.ninja phasea-smoke\phasea_smoke.exe`；按上方用户目录保护与私有 TEMP/TMP 规则运行程序，必须观察零退出码及下方标记。
5. PNG 在自有探针目录生成；本次直接检查了实际主界面与覆盖确认的像素图。保存源码/配方和事实后删除该目录、Ninja 扩展及一次性脚本；不删除构建缓存/用户数据。

```text
PHASE_A_EXPORT midi_replace=1 wav_replace=1 audible=1 cancel_preserved=1 locked_preserved=1 rejected_task_preserved=1
PHASE_A_SETTINGS sync_supersedes=1 real_timer=1 first_snapshot=1 replacement_snapshot=1 independent_xml=1
PHASE_A_UI restored_B=1 inherited_builtin_layout=1 runtime_binding=1 immediate_autosave_B=1 rename_confirm=1 declined_preserved=1 case_only_preserved=1 real_take_import_detached=1
PHASE_A_PROFILE_SCOPE_RESTORED=1
PHASE_A_PRIVATE_FILES_CLEAN=1
```

**失败记录单列**：首轮应用编译暴露录制停止弹窗漏传新 generation 参数，补齐后最终构建和默认执行通过。探针最初沿用测试用 modal-loop helper，但 app 未定义该宏，改用上方原生 pump；只做进程 HKCU 注册表映射仍得到真实 CSIDL 路径，因此安全守卫拒绝构造 owner，并清理自有键/文件，再采用可验证的探针内路径定向。它们均不被隐藏为第一次即通过，不回写原审计或将路径隔离伪装为产品功能。

### AUDIT-004 Phase B：文件准入与时间线数值安全 [已完成，2026-10-03]

**目标**：畸形/部分文件在准入失败，不放大分配、不破坏当前 Take，不让无效时间线进入渲染。

**依赖**：Phase A 的所有权和失败保留约束；数值检查应先于打开输出。

原生/MIDI 输入在可表示与资源预算内进入消费者，拒绝失败不替换旧会话；保留完整文件后缀兼容。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [x] | `SEC-003` | P1 | 原生 MIDI 编码信任未校验的解码长度前缀。解码前验证 JUCE 特有编码的长度、数据预算和负载一致性；校验解码后的 MIDI 帧形状并明确传播读取失败。 | 负/超额/不一致长度前缀、截断帧在分配/构造前拒绝；256 MiB 独立子进程均返回 nullopt，无异常逃逸，原 Take/文件保留，见 EVID-012/016。 |
| [x] | `SEC-004` | P1 | 原生采样率/长度缺少可表示范围验证。准入时检查有限、支持范围的采样率、非负且一致的长度/时间戳；消费端使用检查后的比例和整数转换。 | 极小正率、非有限值、负/不一致长度或不可表示缩放拒绝；正常录制/播放及 WAV 时长不变，原合成时间域回归保留，见 EVID-013。 |
| [x] | `SEC-005` | P1 | 饱和时间戳后 +1/尾部采样加法仍溢出。在打开输出前验证最终事件与尾部长度均可表示；检查加法，不将转换饱和视为整个时间线已安全。 | 同率 INT64_MAX 最后事件与尾部相加拒绝，不打开输出；内置/插件路径保留原目标，MIDI writer 整数/VLQ 范围也在写出前检查，见 EVID-013。 |
| [x] | `SEC-006` | P1 | MIDI 拍号元数据未验证负载及位移指数。读取前验证固定宽度 meta 长度及拍号分母指数；畸形值拒绝/显式忽略并报告，不直接进入框架 accessor。 | 非法 0x58 长度/指数、原始截断 meta VLQ 在 accessor 前拒绝并诊断；合法拍号正确，见 EVID-014。 |
| [x] | `QUAL-005` | P1 | 原生加载保留乱序事件但播放器假定有序。原生文件准入拒绝或稳定规范化非单调时间线；保留同采样语义顺序并测试真正播放/seek。 | 乱序旧文件稳定规范化；实际播放交付早期 NoteOn，真实 seek 后同采样 Off/On 顺序保持，见 EVID-015。 |
| [x] | `ERR-004` | P2 | MIDI宽容尾字节也误接收缺失/截断轨。仅完整声明结构后额外后缀允许宽容，缺失/短chunk应拒绝且保留现有Take；保留真实CRLF后缀兼容。 | 缺第二声明轨、短 chunk/事件及缺/提前 EOT 导入失败，旧 Take 不变；完整多轨+CRLF 正确提交，完整扩展块不抵充声明轨，见 EVID-014/016。 |

#### Phase B 实施记录与直接验证（2026-10-03）

**基线与范围**：`a8336e446b0ebc4fac37403335a1f3b3460009e7`（Phase A 本地交付）；仅本阶段六项、共享数值工具及直接文件/播放/渲染消费者。历史 AUDIT/ADR 保持原样，文件版本不变；Phase C 尚未启动。

- `SEC-003`：原生文件/JSON 32 MiB、单帧 1 MiB、累计解码 32 MiB；在 JUCE 解码分配前线性检查长度前缀、精确字符数、字符表和填充位，构造消息前检查原始 MIDI 帧。文件/流长度、完整读取及状态不一致即失败；元数据失败也不替换当前会话。保存前使用同一帧/数值边界，避免写出自身无法准入的文件。
- `SEC-004`：文件支持有限 8000–384000 Hz，整数长度/时间戳必须非负、一致且可表示。`TimelineValidation.h` 将文件策略与通用时间域检查分开，保留现有合成时间域回归；最坏设备倍率/0.5x 播放长度及块余量在结构提交前检查，非有限倍速拒绝。
- `SEC-005`：`prepareRenderTimeline()` 完整检查目标缩放、最后事件 `+1` 与尾部加法后才打开 WAV 输出；两个渲染循环按实际 `blockEnd` 推进。MIDI 写出先验证 PPQ、JUCE `int` tick 和 4 字节 VLQ delta，不进入不满足前提的 writer 转换。旧饱和 API/callers/tests 完整切换，不留兼容别名。
- `SEC-006 / ERR-004`：SMF 声明轨、chunk、VLQ、消息和 EOT 在 JUCE 前验证；time division 在 tick→秒入口验证，固定 meta 的长度/拍号指数在 accessor 前验证。已验证完整扩展块不抵充声明轨；仅有扩展块时规范化一次，普通路径不额外复制。完整结构后的 CRLF 保留，失败不提交新 Take。
- `QUAL-005`：只在原生准入稳定规范化乱序事件，同采样顺序不变；序列化不复制并重排整个 Take。删除旧 fixture 的偶然原数组索引断言，不将其重新钉为新索引；保留真正播放/seek 和同采样顺序的回归。

**可复核证据**：本表只记录本轮实际运行，不将次数/耗时作为长期门槛。最小输入和消费者调用都包含在下方完整探针源码中。

| 证据 ID | 对应问题 / 类型 | 精确执行或输入 | 实际观察 | 证明边界 |
| --- | --- | --- | --- | --- |
| EVID-011 | 构建 / 默认门禁 | 复用 `build-win-msvc/audit004-phase0` Windows MSVC Debug 子树与 `/Zc:nrvo-`；构建 app/tests，默认 `ctest` 无 category/name 补跑。 | 最终构建通过；本次默认 98 套件、98,257 通过断言、零失败，Chord 完整执行；真实用户目录清单/属性/mtime/SHA256 一致，私有 TEMP/TMP 零残留。 | 原默认缓存失败不改写；未运行 Release/WSL 软件测试或全量 tidy，不宣称全项目 warning/tidy 清零。 |
| EVID-012 | SEC-003；受限进程及文件确认 | 下方 14 个原生畸形输入；每个通过 Windows Job Object 在运行前附加 256 MiB committed memory 限制，5 秒等待；包括 `-1.`、超额前缀、短负载、填充位、截断帧及非法 meta。 | 子进程均正常返回 nullopt，无异常/崩溃退出；实际 `RecordingSession::openFromFile()` 拒绝，generation/绑定/元数据/音符和 A 原字节保持。 | 防止长度前缀放大，不在真实用户文件或无约束 OOM 进程上试验；有限预算不等于所有可用内存不足都已模拟。 |
| EVID-013 | SEC-004/005；数值和输出消费者 | 极小/非有限率、负长度、越界/分数时间戳；48k→44.1k 的 2x resume；同率事件 MAX 与 LEN=MAX-88199 加两秒尾部；MIDI 巨大 tick。 | 正常 resume=11025、播放结束=22050、可听 WAV=132300 samples；拒绝保留原 WAV/MIDI 字节，未创建输出目录；默认插件测试的实际渲染函数也拒绝并保留目标。 | 最后事件 MAX 用同率输入验证 `+1` 溢出，不把下采样后仍可表示的超长值误称溢出。插件测试使用现有测试乐器，未验证真实厂商 VST3。 |
| EVID-014 | SEC-006/ERR-004；标准 MIDI 文件与直接并轨 | 缺第二 MTrk、短 chunk/事件、非法 0x58 长度/指数、EOT 缺失或提前、原始 meta VLQ 截断；完整多轨+CRLF、完整扩展块。 | 畸形输入拒绝且有诊断；完整两轨+CRLF 的拍号 4/4、时长 24000 samples；默认回归验证扩展块不抵充声明轨及合法完整输入。 | 不以“已有任意轨有内容”推断完整；RIFF/SMPTE 既有支持保留，未声称任意外部制作器格式均已手工覆盖。 |
| EVID-015 | QUAL-005；真实播放和 seek | 原生文件将未来 Off 放在 On@0 前面，Off@1000 和 On@1000 同采样；实际 `RecordingEngine::renderPlaybackBlock()` 与 request/apply seek。 | On@0 在首块交付；seek 到 1000 后顺序为 Off(60)→On(64)，不漏早期音符、不重排同采样语义。 | 本项验证准入排序，不关闭 Phase D 的末尾 NoteOff、pause 或发音身份问题。 |
| EVID-016 | 六项；实际主界面与数据保护 | 私有 profile 验证 JUCE 解析路径后构造真实 MainComponent，实际 filesDropped 逐一投递坏原生/MIDI 文件，打开 Info；随后完整 CRLF 文件。 | 拒绝后 Info 仍为 `Kept take`；完整多轨提交为 `complete-crlf`；实际界面与弹窗 PNG 已直接检查，import slot 恢复、私有文件清理，真实用户目录未变。 | 仅进程内路径输入隔离，不是被测功能替代实现；未对原生系统文件选择器逐项人工操作。 |
| EVID-017 | 环境 / 失败与纠正 | 见下方失败记录；format check 与 WSL configure-only。 | 格式通过，编译数据库刷新；最后增量构建未再出现本轮新增 C4244。codegraph 未挂载，LSP reload 后仍报告错误 include 路径并遗漏 references，已报告。 | MSVC 实际构建/消费者是证据；不把 LSP 失败伪装为零诊断，也不重跑已知 Phase G 全量 tidy 失败。 |

**精确构建与默认运行**：先在 WSL 执行 `./scripts/dev.sh win-build --sync-only`。镜像 Developer PowerShell 保留现有子树，不清理原缓存：

```powershell
Set-Location 'G:\source\projects\devpiano'
$build = 'build-win-msvc\audit004-phase0'
cmake --build $build --target devpiano devpiano_tests --parallel 4
```

默认运行与独立程序的外层保护使用下方一次性脚本（保存为系统临时目录下的 `phaseb-windows.ps1`，不提交到产品）：`-Mode Test` 运行默认 ctest；`-Mode Smoke` 运行下方链接真实 app object 的独立程序。首次配置、`/Zc:nrvo-` 核对沿用 EVID-001 配方。

```powershell
param([ValidateSet('Build','Test','Smoke')][string]$Mode)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$mirror = 'G:\source\projects\devpiano'
$build = Join-Path $mirror 'build-win-msvc\audit004-phase0'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$instances = @((& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -format json) | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $instances.Count -ne 1) { throw 'VS discovery failed' }
Import-Module (Join-Path $instances[0].installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll') -Force
Enter-VsDevShell -VsInstallPath $instances[0].installationPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
Set-Location $mirror
if ($Mode -eq 'Build') {
    & cmake --build $build --target devpiano devpiano_tests --parallel 4
    if ($LASTEXITCODE -ne 0) { throw "Debug build failed: $LASTEXITCODE" }
    Write-Output 'PHASE_B_DEBUG_BUILD_PASSED=1'
    exit 0
}
function Get-UserSnapshot([string]$Path) {
    $rows = [System.Collections.Generic.List[object]]::new()
    $exists = Test-Path -LiteralPath $Path
    if ($exists) {
        foreach ($entry in @(Get-Item -LiteralPath $Path -Force) + @(Get-ChildItem -LiteralPath $Path -Force -Recurse | Sort-Object FullName)) {
            $hash = if ($entry.PSIsContainer) { '' } else { (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash }
            $length = if ($entry.PSIsContainer) { 0 } else { $entry.Length }
            $rows.Add([ordered]@{ name=$entry.FullName; directory=$entry.PSIsContainer; length=$length; attributes=[int]$entry.Attributes; modified=$entry.LastWriteTimeUtc.Ticks; hash=$hash })
        }
    }
    return (ConvertTo-Json -Depth 8 -Compress -InputObject ([ordered]@{ exists=$exists; rows=$rows.ToArray() }))
}
$userDir = Join-Path ([Environment]::GetFolderPath('ApplicationData')) 'DevPiano'
$before = Get-UserSnapshot $userDir
$private = Join-Path ([System.IO.Path]::GetTempPath()) ('devpiano-phaseb-' + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($private) | Out-Null
$oldTemp = $env:TEMP
$oldTmp = $env:TMP
$env:TEMP = $private
$env:TMP = $private
$exitCode = -1
try {
    if ($Mode -eq 'Test') {
        & ctest --test-dir $build -C Debug --verbose 2>&1 | Tee-Object -FilePath (Join-Path $build 'phaseb-default-tests.log')
        $exitCode = $LASTEXITCODE
    } else {
        & (Join-Path $build 'phaseb-smoke\phaseb_smoke.exe') 2>&1 | Tee-Object -FilePath (Join-Path $build 'phaseb-smoke.log')
        $exitCode = $LASTEXITCODE
    }
} finally {
    $env:TEMP = $oldTemp
    $env:TMP = $oldTmp
    $after = Get-UserSnapshot $userDir
    $remaining = @(Get-ChildItem -LiteralPath $private -Force -Recurse).Count
    $record = [ordered]@{ mode=$Mode; buildDir=$build; exitCode=$exitCode; userDirectory=$userDir; userDirectoryUnchanged=($before -ceq $after); privateTempDirectory=$private; remainingTempEntries=$remaining }
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build ('phaseb-' + $Mode.ToLowerInvariant() + '-verification.json')) -Encoding utf8
    Write-Output ('PHASE_B_VERIFICATION=' + ($record | ConvertTo-Json -Compress))
    if ($remaining -eq 0) { Remove-Item -LiteralPath $private }
    if ($before -cne $after) { throw 'Protected real user directory changed' }
    if ($remaining -ne 0) { throw 'Private scratch entries remain' }
}
if ($exitCode -ne 0) { throw "Consumer execution failed: $exitCode" }

```

**独立真实消费者源码**：保存为 `$build/phaseb-smoke/phaseb_smoke.cpp`。ProfileDirectoryScope 沿用 Phase A 的进程内 import slot 路径隔离，构造 owner 前检查实际 JUCE 路径，退出恢复；系统 profile、业务实现及子模块均不改。`--reject` 子模式只调用生产加载器；父模式在 CREATE_SUSPENDED 后先施加 Job Object 资源限制再恢复执行。

参考官方契约：[JUCE MemoryBlock](https://docs.juce.com/master/classjuce_1_1MemoryBlock.html)、[MidiFile](https://docs.juce.com/master/classjuce_1_1MidiFile.html)、[Job Object extended limits](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_extended_limit_information)、[SetInformationJobObject](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-setinformationjobobject)、[Windows import table](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#the-import-directory-table)。实际项目以本地 JUCE 子模块签名为准。

```cpp
#include <JuceHeader.h>
#include "MainComponent.h"
#include "Recording/PerformanceFile.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RecordingSessionController.h"
#include "Recording/MidiFileImporter.h"
#include "Recording/RenderPipeline.h"
#include "Recording/WavFileExporter.h"
#include "Recording/MidiFileExporter.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include <windows.h>
#include <shlobj.h>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void pump(int milliseconds = 100) {
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + milliseconds;
    while (juce::Time::getMillisecondCounterHiRes() < deadline) {
        MSG message {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    }
}
class PrivateDirectory {
public:
    PrivateDirectory() : directory(juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("phaseb-consumer-" + juce::Uuid().toString())) {
        require(!directory.exists() && directory.createDirectory().wasOk(), "owned directory creation failed");
    }
    ~PrivateDirectory() {
        std::cout << "PHASE_B_PRIVATE_FILES_CLEAN=" << directory.deleteRecursively(false) << '\n';
    }
    juce::File getChildFile(const juce::String& name) const { return directory.getChildFile(name); }
    const juce::File& get() const { return directory; }
private:
    juce::File directory;
};
class ProfileDirectoryScope {
public:
    ~ProfileDirectoryScope() {
        if (slot == nullptr) return;
        DWORD protection = 0;
        const bool restored = VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection) != FALSE;
        if (restored) {
            *slot = reinterpret_cast<ULONG_PTR>(original);
            DWORD ignored = 0;
            VirtualProtect(slot, sizeof(*slot), protection, &ignored);
        }
        std::cout << "PHASE_B_PROFILE_SCOPE_RESTORED=" << restored << '\n';
    }
    bool redirect(const juce::File& directory) {
        privatePath = directory.getFullPathName().toWideCharPointer();
        if (privatePath.size() >= MAX_PATH) return false;
        auto* base = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        const auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if (rva == 0) return false;
        auto* imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + rva);
        for (; imports->Name != 0; ++imports) {
            if (imports->OriginalFirstThunk == 0) continue;
            auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->OriginalFirstThunk);
            auto* entries = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->FirstThunk);
            for (; names->u1.AddressOfData != 0; ++names, ++entries) {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                auto* imported = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                if (std::strcmp(imported->Name, "SHGetSpecialFolderPathW") != 0) continue;
                auto* candidate = &entries->u1.Function;
                DWORD protection = 0;
                if (!VirtualProtect(candidate, sizeof(*candidate), PAGE_READWRITE, &protection)) return false;
                original = reinterpret_cast<NativeFolder>(static_cast<ULONG_PTR>(*candidate));
                *candidate = reinterpret_cast<ULONG_PTR>(&privateFolder);
                DWORD ignored = 0;
                VirtualProtect(candidate, sizeof(*candidate), protection, &ignored);
                slot = candidate;
                return true;
            }
        }
        return false;
    }
private:
    using NativeFolder = BOOL (WINAPI*)(HWND, LPWSTR, int, BOOL);
    static BOOL WINAPI privateFolder(HWND window, LPWSTR destination, int kind, BOOL create) {
        if (kind != CSIDL_APPDATA) return original(window, destination, kind, create);
        std::copy(privatePath.begin(), privatePath.end(), destination);
        destination[privatePath.size()] = 0;
        return TRUE;
    }
    ULONG_PTR* slot = nullptr;
    static inline NativeFolder original = nullptr;
    static inline std::wstring privatePath;
};

template <typename T> T* find(juce::Component& root, const juce::String& id) {
    if (root.getComponentID() == id) {
        if (auto* found = dynamic_cast<T*>(&root)) return found;
    }
    for (int i = 0; i < root.getNumChildComponents(); ++i) {
        if (auto* found = find<T>(*root.getChildComponent(i), id)) return found;
    }
    return nullptr;
}
template <typename T> T* findType(juce::Component& root) {
    if (auto* found = dynamic_cast<T*>(&root)) return found;
    for (int i = 0; i < root.getNumChildComponents(); ++i) {
        if (auto* found = findType<T>(*root.getChildComponent(i))) return found;
    }
    return nullptr;
}
juce::Component& modal() {
    auto* component = juce::Component::getCurrentlyModalComponent();
    require(component != nullptr, "missing real modal");
    return *component;
}
void click(juce::Component& root, const juce::String& id) {
    auto* button = find<juce::Button>(root, id);
    require(button != nullptr && button->isEnabled(), "missing or disabled real button");
    button->triggerClick();
    pump();
}
class ModalCleanup {
public:
    ~ModalCleanup() {
        while (auto* component = juce::Component::getCurrentlyModalComponent()) component->exitModalState(0);
        pump();
    }
};
void snapshot(juce::Component& component, const juce::File& output) {
    auto image = component.createComponentSnapshot(component.getLocalBounds());
    juce::FileOutputStream stream(output);
    require(stream.openedOk(), "snapshot open failed");
    require(juce::PNGImageFormat().writeImageToStream(image, stream), "snapshot encode failed");
}

using namespace devpiano::recording;
RecordingTake normalTake() {
    RecordingTake result;
    result.sampleRate = 48000.0;
    result.lengthSamples = 48000;
    result.events = {
        { 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOn(1, 64, 0.8f) },
        { 24000, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOff(1, 64) }
    };
    return result;
}
juce::String encoded(std::initializer_list<uint8_t> bytes) {
    return juce::MemoryBlock(bytes.begin(), bytes.size()).toBase64Encoding();
}
juce::String nativeJson(const juce::String& midi, const juce::String& rate = "48000", const juce::String& length = "48000", const juce::String& timestamp = "0") {
    return "{\"version\":2,\"format\":\"devpiano-performance\",\"sampleRate\":" + rate
        + ",\"lengthSamples\":" + length + ",\"events\":[{\"timestampSamples\":" + timestamp
        + ",\"type\":\"midi\",\"midiData\":\"" + midi + "\"}]}";
}
class WinHandle {
public:
    explicit WinHandle(HANDLE value) : handle(value) {}
    ~WinHandle() { if (handle != nullptr && handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;
    HANDLE handle;
};
void limitedReject(const juce::File& input) {
    WinHandle job(CreateJobObjectW(nullptr, nullptr));
    require(job.handle != nullptr, "job creation failed");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    limits.ProcessMemoryLimit = 256ULL * 1024 * 1024;
    require(SetInformationJobObject(job.handle, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE, "job memory limit failed");
    const auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName();
    const auto command = "\"" + exe + "\" --reject \"" + input.getFullPathName() + "\"";
    std::wstring mutableCommand(command.toWideCharPointer());
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    require(CreateProcessW(exe.toWideCharPointer(), mutableCommand.data(), nullptr, nullptr, FALSE,
                           CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE, "child creation failed");
    WinHandle child(process.hProcess);
    WinHandle thread(process.hThread);
    if (AssignProcessToJobObject(job.handle, child.handle) == FALSE) {
        TerminateProcess(child.handle, 125);
        throw std::runtime_error("child job assignment failed");
    }
    require(ResumeThread(thread.handle) != static_cast<DWORD>(-1), "child resume failed");
    require(WaitForSingleObject(child.handle, 5000) == WAIT_OBJECT_0, "bounded child timed out");
    DWORD code = 0;
    require(GetExitCodeProcess(child.handle, &code) != FALSE && code == 0, "bounded child rejected via exception/crash instead of nullopt");
}
juce::File writeHex(const juce::File& directory, const juce::String& name, const char* hex) {
    juce::MemoryBlock bytes;
    bytes.loadFromHexString(hex);
    const auto output = directory.getChildFile(name);
    require(output.replaceWithData(bytes.getData(), bytes.getSize()), "binary fixture write failed");
    return output;
}
struct InputFiles {
    juce::File native;
    juce::File legalMidi;
    std::vector<juce::File> rejected;
};
InputFiles exerciseAdmission(const juce::File& directory) {
    InputFiles files;
    files.native = directory.getChildFile("kept.devpiano");
    PerformanceFileMetadata metadata;
    metadata.title = "Kept take";
    require(savePerformanceFile(normalTake(), files.native, metadata), "native seed failed");
    const auto originalBytes = files.native.loadFileAsString();
    RecordingSessionController::RecordingSession session;
    require(session.openFromFile(files.native), "valid session admission failed");
    const auto generation = session.takeGeneration;
    const auto goodMidi = encoded({ 0x90, 64, 100 });
    std::cout << "PHASE_B_ENCODING noteOn=" << encoded({ 0x90, 0x3c, 0x64 })
              << " noteOff=" << encoded({ 0x80, 0x3c, 0x00 }) << '\n';
    const std::vector<juce::String> malformed = {
        nativeJson("-1."), nativeJson("2147483647."), nativeJson("3.A"), nativeJson("3.@@@@"),
        nativeJson("1.AD"), nativeJson(encoded({ 0x90, 64 })), nativeJson(encoded({ 0x90, 0x80, 100 })),
        nativeJson(encoded({ 0xFF, 0x58, 4, 4, 255, 24, 8 })), nativeJson(goodMidi, "1e-300"),
        nativeJson(goodMidi, "1e309"), nativeJson(goodMidi, "48000", "-1"),
        nativeJson(goodMidi, "48000", "9223372036854775807"), nativeJson(goodMidi, "48000", "48000", "48001"),
        nativeJson(goodMidi, "48000", "48000", "0.5")
    };
    for (size_t i = 0; i < malformed.size(); ++i) {
        const auto input = directory.getChildFile("reject-" + juce::String(static_cast<int>(i)) + ".devpiano");
        require(input.replaceWithText(malformed[i]), "native fixture write failed");
        limitedReject(input);
        require(!session.openFromFile(input), "malformed native file admitted");
        require(session.takeGeneration == generation && session.currentPerformanceFile == files.native
                && session.currentMetadata.title == metadata.title && session.take.events.front().message.getNoteNumber() == 64,
                "failed native admission changed owned take");
        files.rejected.push_back(input);
    }
    require(files.native.loadFileAsString() == originalBytes, "failed admission modified original native bytes");
    const char* headerTwo = "4D546864000000060001000201E04D54726B0000000D00903C648360803C0000FF2F00";
    files.rejected.push_back(writeHex(directory, "missing-track.mid", headerTwo));
    files.rejected.push_back(writeHex(directory, "short-chunk.mid", "4D546864000000060000000101E04D54726B0000004000903C6400803C0000FF"));
    files.rejected.push_back(writeHex(directory, "short-event.mid", "4D546864000000060000000101E04D54726B0000000300903C"));
    files.rejected.push_back(writeHex(directory, "invalid-meter.mid", "4D546864000000060000000101E04D54726B0000000C00FF580404FF180800FF2F00"));
    files.rejected.push_back(writeHex(directory, "short-meter.mid", "4D546864000000060000000101E04D54726B0000000A00FF5802040200FF2F00"));
    for (const auto& input : files.rejected) {
        if (input.hasFileExtension("mid")) require(!importMidiFileWithMetadata(input, 48000.0), "partial MIDI admitted");
    }
    files.legalMidi = writeHex(directory, "complete-crlf.mid", "4D546864000000060001000201E04D54726B0000001300FF58040402180800FF510307A12000FF2F004D54726B0000000D00903C648360803C0000FF2F000D0A");
    const auto imported = importMidiFileWithMetadata(files.legalMidi, 48000.0);
    require(imported.has_value() && imported->stats.trackCount == 2 && imported->metadata.initialTimeSignature.has_value(), "complete multitrack CRLF import failed");
    require(imported->metadata.initialTimeSignature->numerator == 4 && imported->metadata.initialTimeSignature->denominator == 4, "legal meter wrong");
    require(imported->take.lengthSamples == 24000, "normal MIDI duration changed");
    std::cout << "PHASE_B_ADMISSION limited_native=14 memory_limit_mib=256 nullopt_without_exception=1 retained_take=1 retained_file=1 incomplete_midi_rejected=1 complete_crlf=1 legal_meter=1\n";
    return files;
}
void exercisePlayback(const juce::File& directory) {
    auto take = normalTake();
    take.lengthSamples = 48000;
    take.events = {
        { 1000, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOff(1, 60) },
        { 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOn(1, 60, 0.8f) },
        { 1000, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOn(1, 64, 0.8f) }
    };
    const auto unordered = directory.getChildFile("legacy-unordered.devpiano");
    require(unordered.replaceWithText(serialiseTakeToJson(take)), "legacy input write failed");
    const auto loaded = loadPerformanceFile(unordered);
    require(loaded.has_value(), "legacy unordered admission failed");
    RecordingEngine engine;
    engine.startPlayback(*loaded, 48000.0);
    juce::MidiBuffer buffer;
    engine.renderPlaybackBlock(buffer, 0, 128);
    bool firstNoteOn = false;
    for (const auto event : buffer) firstNoteOn = event.getMessage().isNoteOn();
    require(buffer.getNumEvents() == 1 && firstNoteOn, "early note silently missed");
    engine.requestPlaybackSeek(1000);
    buffer.clear();
    require(engine.applyPendingPlaybackSeek(buffer), "seek not consumed");
    buffer.clear();
    engine.renderPlaybackBlock(buffer, 1000, 128);
    std::vector<int> order;
    for (const auto event : buffer) order.push_back(event.getMessage().isNoteOff() ? -event.getMessage().getNoteNumber() : event.getMessage().getNoteNumber());
    require(order == std::vector<int>({-60, 64}), "stable equal-sample MIDI order lost after real seek");
    take = normalTake();
    take.events[0].timestampSamples = 24000;
    take.events[1].timestampSamples = 30000;
    engine.setPlaybackSpeedMultiplier(2.0);
    engine.startPlaybackAtTakeSample(take, 44100.0, 24000);
    require(engine.getPlaybackPositionSamples() == 11025, "scaled resume changed");
    buffer.clear();
    engine.renderPlaybackBlock(buffer, 11025, 128);
    require(buffer.getNumEvents() == 1, "normal scaled playback wrong");
    engine.advancePlaybackPosition(11025);
    require(engine.consumePlaybackEndedFlag() && engine.getPlaybackPositionSamples() == 22050, "normal scaled duration changed");
    std::cout << "PHASE_B_PLAYBACK unordered_normalized=1 early_note=1 real_seek=1 equal_sample_order=1 scaled_resume=11025 scaled_duration=22050\n";
}
void exerciseRendering(const juce::File& directory) {
    auto take = normalTake();
    devpiano::exporting::WavExportOptions options;
    options.sampleRate = 44100.0;
    options.builtinTone = SettingsModel::BuiltinTone::sine;
    options.reverbWet = 0.0f;
    const auto target = directory.getChildFile("valid.wav");
    require(devpiano::exporting::exportTakeAsWavFile(take, target, options), "normal WAV failed");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(target.createInputStream().release(), true));
    require(reader != nullptr && reader->sampleRate == 44100.0 && reader->lengthInSamples == 132300, "normal WAV duration wrong");
    juce::AudioBuffer<float> samples(2, 4096);
    require(reader->read(&samples, 0, 4096, 4096, true, true) && samples.getMagnitude(0, 4096) > 0.001f, "normal WAV payload silent");
    reader.reset();
    juce::MemoryBlock original;
    require(target.loadFileAsData(original), "render snapshot failed");
    const auto sameBytes = [&] { juce::MemoryBlock current; return target.loadFileAsData(current) && current == original; };
    take.sampleRate = options.sampleRate;
    take.lengthSamples = std::numeric_limits<std::int64_t>::max();
    take.events.back().timestampSamples = take.lengthSamples;
    require(!devpiano::exporting::exportTakeAsWavFile(take, target, options) && sameBytes(), "final event overflow touched output");
    take.events.back().timestampSamples = 24000;
    take.sampleRate = 44100.0;
    take.lengthSamples = std::numeric_limits<std::int64_t>::max() - 88199;
    require(!devpiano::exporting::exportTakeAsWavFile(take, target, options) && sameBytes(), "tail overflow touched output");
    take = normalTake(); take.sampleRate = 1e-300;
    const auto untouched = directory.getChildFile("uncreated").getChildFile("invalid.wav");
    require(!devpiano::exporting::exportTakeAsWavFile(take, untouched, options) && !untouched.getParentDirectory().exists(), "invalid rate created output directory");
    const auto midiTarget = directory.getChildFile("valid.mid");
    require(devpiano::exporting::exportTakeAsMidiFile(normalTake(), midiTarget), "normal MIDI export failed");
    require(importMidiFile(midiTarget, 48000.0).has_value(), "normal MIDI export did not reimport");
    juce::MemoryBlock midiOriginal;
    require(midiTarget.loadFileAsData(midiOriginal), "MIDI snapshot failed");
    auto midiTake = normalTake();
    midiTake.lengthSamples = 48000LL * 1000000;
    midiTake.events.back().timestampSamples = midiTake.lengthSamples;
    require(!devpiano::exporting::exportTakeAsMidiFile(midiTake, midiTarget), "unrepresentable MIDI ticks admitted");
    juce::MemoryBlock midiCurrent;
    require(midiTarget.loadFileAsData(midiCurrent) && midiCurrent == midiOriginal, "MIDI rejection modified original bytes");
    std::cout << "PHASE_B_MIDI_EXPORT real_roundtrip=1 writer_tick_limit_checked=1 original_bytes_retained=1\n";
    std::cout << "PHASE_B_RENDER wav_duration=132300 audible=1 final_plus_one_rejected=1 tail_add_rejected=1 original_bytes_retained=1 output_not_opened=1\n";
}
void exerciseOwner(const InputFiles& files) {
    SettingsModel settings;
    settings.languageCode = "en";
    settings.masterGain = 0.0f;
    settings.metronomeEnabled = false;
    { SettingsStore store; require(store.save(settings), "private settings seed failed"); }
    MainComponent owner;
    ModalCleanup cleanup;
    owner.setSize(1280, 800);
    owner.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    owner.setVisible(true);
    pump(200);
    owner.filesDropped(juce::StringArray{files.native.getFullPathName()}, 0, 0); pump();
    for (const auto& input : files.rejected) {
        owner.filesDropped(juce::StringArray{input.getFullPathName()}, 0, 0); pump(20);
        click(owner, "song-info-btn");
        auto* title = find<juce::TextEditor>(modal(), "title-editor");
        require(title != nullptr && title->getText() == "Kept take", "actual file rejection replaced current take title");
        click(modal(), "dialog-cancel-btn");
    }
    const auto images = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    snapshot(owner, images.getChildFile("phaseb-rejection-retained-take.png"));
    owner.filesDropped(juce::StringArray{files.legalMidi.getFullPathName()}, 0, 0); pump();
    click(owner, "song-info-btn");
    auto* title = find<juce::TextEditor>(modal(), "title-editor");
    require(title != nullptr && title->getText() == "complete-crlf", "actual complete MIDI import rejected");
    snapshot(modal(), images.getChildFile("phaseb-accepted-crlf-info.png"));
    click(modal(), "dialog-cancel-btn");
    std::cout << "PHASE_B_UI real_files_dropped=1 rejected_title_retained=1 valid_multitrack_crlf_committed=1 private_profile=1\n";
}
}
int main(int argc, char** argv) {
    try {
        std::cout << std::unitbuf;
        if (argc == 3 && std::strcmp(argv[1], "--reject") == 0) {
            return loadPerformanceFile(juce::File(argv[2])).has_value() ? 2 : 0;
        }
        PrivateDirectory privateDirectory;
        const auto roaming = privateDirectory.getChildFile("Roaming");
        require(roaming.createDirectory().wasOk(), "private profile creation failed");
        ProfileDirectoryScope profile;
        require(profile.redirect(roaming), "private profile redirect failed");
        require(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory) == roaming, "unsafe real profile; owner not constructed");
        juce::ScopedJuceInitialiser_GUI gui;
        const auto files = exerciseAdmission(privateDirectory.get());
        exercisePlayback(privateDirectory.get());
        exerciseRendering(privateDirectory.get());
        exerciseOwner(files);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PHASE_B_SMOKE_ERROR=" << error.what() << '\n';
        return 1;
    }
}

```

**独立程序构建/执行配方**（只读借用 app 构建参数，不替换被测实现）：

1. 读取此树 `compile_commands.json` 中 `MainComponent.cpp` 唯一 command，保留所有定义/include/PCH/运行库与 `/Zc:nrvo-`；仅替换末尾 `-c` 源 token、`/Fo`、`/Fd` 为 `phaseb-smoke/phaseb_smoke.cpp/.obj` 和 compile PDB，写 `phaseb-smoke/compile-smoke.cmd`。
2. `phaseb-smoke.ninja` 中 `include build.ninja`，复制 `DevPiano.exe` 的真实 linker rule、全部输入与库，仅去掉 `source/Main.cpp.obj` 入口，加入探针 object；`/subsystem:windows` 改为 `/subsystem:console`。不去掉任何其他业务/JUCE object，不链接 UnitTest 替代实现。
3. 将 `OBJECT_DIR`、`TARGET_SUPPORT_DIR`、`TARGET_COMPILE_PDB`、`TARGET_FILE`、`TARGET_IMPLIB`、`TARGET_PDB`、`RSP_FILE` 定向到自有 `phaseb-smoke` 目录，保留其余 CONFIG/FLAGS/LINK_LIBRARIES/PRE_LINK/POST_BUILD。不覆盖原 app edge/response file。
4. 在该构建树执行 `cmd.exe /D /C phaseb-smoke\compile-smoke.cmd`；从 `CMakeCache.txt` 的 `CMAKE_MAKE_PROGRAM` 取 Ninja，执行 `ninja -f phaseb-smoke.ninja phaseb-smoke/phaseb_smoke.exe`。用上方保护脚本 `-Mode Smoke` 运行，观察零退出码及下方标记；`-Mode Test` 必须使用默认全套入口。
5. 保存事实与本段完整输入后清理一次性源码/程序、Ninja 扩展、调试辅助及私有临时目录，不清理构建缓存/真实用户数据。此段不依赖临时 GUID 路径继续存在。

```text
PHASE_B_ENCODING noteOn=3.PxCY noteOff=3..xC.
PHASE_B_ADMISSION limited_native=14 memory_limit_mib=256 nullopt_without_exception=1 retained_take=1 retained_file=1 incomplete_midi_rejected=1 complete_crlf=1 legal_meter=1
PHASE_B_PLAYBACK unordered_normalized=1 early_note=1 real_seek=1 equal_sample_order=1 scaled_resume=11025 scaled_duration=22050
PHASE_B_MIDI_EXPORT real_roundtrip=1 writer_tick_limit_checked=1 original_bytes_retained=1
PHASE_B_RENDER wav_duration=132300 audible=1 final_plus_one_rejected=1 tail_add_rejected=1 original_bytes_retained=1 output_not_opened=1
PHASE_B_UI real_files_dropped=1 rejected_title_retained=1 valid_multitrack_crlf_committed=1 private_profile=1
PHASE_B_PROFILE_SCOPE_RESTORED=1
PHASE_B_PRIVATE_FILES_CLEAN=1
```

**失败记录单列**：首轮 MSVC 发现 `ssize_t` 非跨平台全局类型；核对本地 JUCE 后改为在已验证 32 MiB 范围内使用 `std::ptrdiff_t`，新增 optional 窄化 C4244 使用明确 uint8_t 常量消除。首轮默认测试 33 个失败断言来自物理率策略误伤通用合成播放（32）及扩展块虽预检完整但 JUCE 按 chunk 计轨（1）；修正分层/规范化，保留原播放/循环断言，不改成新的期望值。首次独立探针误用 48k→44.1k 下采样的 MAX 输入作为必溢出案例，缩放后仍可表示，导致极长渲染超时；Windows 栈确认在实际 WAV 写出/limiter，停止的仅是已核对命令行的自有 probe，清理其约 44.7 GB 临时 WAV 及私有目录。随后按原边界改为同率 MAX，启用即时 stdout；最终源码、零退出与保护结果见上，不把此探针构造错误写成产品溢出反证或隐去临时资源风险。

### AUDIT-004 Phase C：插件与活动 DSP / Transport 所有权 [已完成，2026-10-03]

**目标**：所有实例/声部/活动游标变更有明确停机或音频所有者边界，异步导出协作收尾。

**依赖**：Phase 0/A；冻结已有数据与旧实例生命周期，先收敛竞态再改事件执行。

先关 Editor/停 callback 或发布音频所有者命令，再修改实例、voice 或游标；独立离线实例声明正确模式。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [x] | `AUDIT-001 THR-004` | P1 | 增量重扫绕过停音频/关 Editor 守卫。复用设备重建守卫，在扫描卸载前关闭 Editor 并停止 callback；覆盖已加载+Editor 打开时重扫。 | 真实 native callback 被 event 阻塞时重扫等待；旧 Editor 在卸载前关闭，实际加载/重扫/卸载/退出通过，见 EVID-021/024。 |
| [x] | `AUDIT-002 THR-001` | P1 | 音色重建仍从消息线程应用活动 DSP 参数。将重建与参数提交放入明确停音频窗口，或仅由音频所有者完成受控切换；不能只依赖单次 getVoice/clear/add 的内部锁。 | 启动 --sine、再次启动 --piano/--sine 走守卫；真实正在执行的 callback 未退出时参数/重建操作不返回，见 EVID-021/024。 |
| [x] | `known-issues §2/Phase 6-2 播放速度控制` | P1 | 活动变速/Stop 在消息线程改写音频游标。发布 transport 命令，在音频块边界一致应用倍率、位置和游标；结构性停止复用停机守卫；补真实双线程回归。 | 真正 AudioEngine 双线程交错保留当前块位置；下一块提交 2x，交付 NoteOff、完成 A-B 回跳后响应 Stop，不留已发音，见 EVID-022。 |
| [x] | `THR-002` | P1 | 取消/析构 WAV 任务可能强制终止工作线程。仅协作取消，异步等待实际工作线程退出后再释放任务/插件/文件所有权；验证慢 processBlock 的取消和退出。 | 原生 processBlock 被 event 阻塞超过旧 3 秒窗口后仍保留所有者；正式连续取消无句柄/临时文件增量，实际退出等待 worker，见 EVID-023/024。 |
| [x] | `QUAL-014` | P2 | 离线插件实例未声明 nonRealtime 模式。prepare前setNonRealtime(true)，保证setup和process一致；选择依赖offline模式的真实VST3对照验证。 | 同一原生 VST3 realtime 输出 0.125、offline 输出及实际 WAV 为 0.5；原生 setup/process 均报告 offline，见 EVID-018/020。 |
| [x] | `QUAL-015` | P2 | 再次拖入已经发现的 VST3 被误判为没有类型。分离探测到的有效类型与是否新增列表条目，重复文件也返回可加载身份并保留metadata更新。 | 重复探测返回有效 description；过期 version 恢复后探测真实更新，卸载再拖入已缓存 B 正确加载，见 EVID-020/024。 |
| [x] | `ARCH-002` | P2 | 插件选择/恢复以显示名代替 description 身份。贯穿选择/加载/持久化稳定description身份，显示名仅展示；验证同名不同ID及乐器/效果过滤。 | 同名两个乐器与一个效果保留三份真实身份；实际菜单/类型过滤/恢复 B 均选对，旧设置只唯一迁移，见 EVID-020/024。 |

#### Phase C 实施记录与直接验证（2026-10-03）

**基线与范围**：`55dc0d27b02fce5a338678fa866e348535b3e5a9`（Phase B 本地交付）；仅本阶段七项、共同生命周期/身份边界和直接消费者。历史 AUDIT/ADR 不回写；Phase D 尚未启动。上方 Phase A/B 源码是各自基线的原样执行输入，当前 API 已清切至 description 身份与统一 Transport 入口，不为历史程序保留别名；当前复建使用本节配方。

- **实例/活动 DSP**：增量扫描开始与 startup 扫描复用 `runPluginActionWithAudioDeviceRebuild`；内置音色重建、startup/another-instance 命令同样先关闭 Editor、等待 callback 退出。重启设备后发布真实 Editor/宿主 UI 状态。
- **Transport**：目标速度、Take-relative Seek 和 Stop 发布到有界原子邮箱；`AudioEngine::getNextAudioBlock` 是唯一消费点。Audio 只用已提交的有效倍率/位置与下一未渲染事件；纯变速不 lower_bound 回退，避免 101→50 的取整重播。Stop 在同边界优先并 panic；清除/启动/暂停快照有停机守卫。getter 区分目标与有效速度，不用 paused/stopped 原子值推断旧 callback 已退出。
- **后台导出**：取消仅 signal；`isThreadRunning()` 真正结束后 Timer 才关闭进度窗、释放离线实例一次并回调。应用 quit 保持消息循环等待，直接析构/runSync 用无限协作等待兜底；没有有限超时的强杀，直接渲染函数不再重复接管 release。永久卡死插件可能永久等待，这是所有权安全边界，不假装可安全 TerminateThread。
- **离线模式/身份**：prepare 前 `setNonRealtime(true)`；UI/加载/设置/恢复按 description identifier，名称仅显示。重复探测仍返回 description 并更新 metadata；旧 name 只在缓存唯一匹配时迁移并在保存后移除，多义/缺失有诊断。
- **测试**：移除默认状态/字段转发与先取消再 start 的错误 oracle，不重钉文案。保留真正的默认双线程事件、缩放取整、缓存身份迁移和后台 WAV header/payload 回归；不把 mock AudioPluginInstance 的 setter 回声当成 native offline-mode 证明。

| 证据 | 对应契约 / 类型 | 执行入口与输入 | 观察结果 | 边界 |
| --- | --- | --- | --- | --- |
| EVID-018 | 原基线；运行复现 | 未同步前的 Phase B app objects＋下方 baseline；三个真正的同名 native VST3。 | `repeated_types=0 descriptions=3 selectable_name_count=1 offline_flag=0 setup=0 process=0 sample=0.125`；发布速度直接把位置 `100→50`、Stop 立即生效。 | 不故意触发 UAF/强杀；重扫/tone switch 的原竞争以原基线静态证据保存，修复后做真实交错。 |
| EVID-019 | 构建 / 默认门禁；运行确认 | `build-win-msvc/audit004-phase0` Windows MSVC Debug，保留 `/Zc:nrvo-`，app/tests 两目标；默认 ctest，无 category/name 补跑。 | 最终构建通过；本次默认执行 97 套件、98,281 通过断言、零失败，Chord 完整；真实用户目录前后清单/属性/mtime/SHA256 一致，私有 TEMP/TMP 零残留。 | 数字只记录这次执行，不是日常固定门槛；未跑 Release/WSL 软件测试或重复已知 Phase G tidy 失败。 |
| EVID-020 | QUAL-014/015、ARCH-002；native/file 确认 | 官方 JUCE VST3 wrapper 构建两个 synth＋一个 effect，显示名全部 `Phase C Twin`、文件/FUID 不同；实际 factory、stale metadata 重探测与 WAV 读取。 | 重复文件返回有效 description、过期 version 更新；三种身份保留且 B 正确加载；同一插件 realtime sample=0.125，offline sample/WAV=0.5，native setup/process 都为 offline。 | 自建真实 .vst3 binary，不是 mock 格式或字段转发；未外推任意商业厂商插件。 |
| EVID-021 | 历史 THR-004/THR-001；真实交错 | 已加载 native＋Editor，processBlock 被 Win32 event 阻塞；Main 的扫描与 another-instance --piano 经真实守卫执行，独立控制线程等 mutation 完成 event。 | 控制线程等待 150 ms 仍未收到 mutation 完成；释放 native event 才完成。旧 Editor SafePointer 失效，重扫保留三份身份；--sine/--piano 再次启动可提交。 | 150 ms 是本次观察窗口，不是性能 SLA；不以单次 clear/add/getVoice 的内部锁作证明。 |
| EVID-022 | Phase 6-2；实际 AudioEngine 双线程 | Renderer 调用真正 `getNextAudioBlock`；UI 在当前块阶段发布 2x 与 Stop；Take/loop=48000 Hz，A=0/B=16000。 | 发布时旧位置与有效 1x 不变；下一边界有效 2x，两个 NoteOff 正常交付，完成 A-B 回跳后 Stop 释放仍发音的 62。默认回归也验证 101→50 不重播旧 On。 | 仅本阶段所有权/变速/Stop，不关闭 Phase D 重叠身份、pause/末尾/设备时域等问题。 |
| EVID-023 | THR-002；真实慢插件 / 资源确认 | Native offline processBlock 等 event；取消后每轮保持 3300 ms（超过旧 3000 ms 强停），消息 pump；两次同取消 UI 路径初始化后正式三轮。 | 正式三轮进程句柄均 `1152→1152`，临时文件 delta=0；worker 未退出前回调不发生，releaseResources 一次，回调内 task.reset 安全，原 canary 目标保留。成功热身 WAV 可读且 sample=0.5。 | 冷路径总数 `1141→1143→1152` 单列；新增 thread 的真实入口来自 AMD `amdihk64.dll`，含图形后台资源，不把进程总数差直接叫业务泄漏。正式比较没有放宽阈值；不测所有 GPU/厂商。 |
| EVID-024 | 七项；实际应用 / 窗口 / quit | 探针保留生产 Main.cpp 应用类，仅替换程序入口；私有 profile 验证 JUCE 解析路径后 startup B、类型菜单、重扫、重复拖入、原生保存与阻塞导出 quit。 | 同名菜单=3、instrument=2、effect=1；加载/恢复身份准确；Save 选择 FileNameControlHost 下 Edit 1001、Save 1，status=0；quit 3300 ms 仍响应并显示 Cancelling export，释放 event 后才发送 quit，无未完成输出。实际 PNG 已检查，profile import slot 恢复，真实用户目录未变。 | 不使用 private/public 重定义、不替换业务实现；操作真实 UI/文件框。系统断电、用户强杀及声卡毛刺未外推。 |
| EVID-025 | 工程 / 失败纠正；直接检查 | `self-check`、WSL configure-only、format check，新增 C++ ASCII、历史哈希和下方失败记录。 | 环境/格式通过，新增 .cpp/.h 行无裸非 ASCII；历史 AUDIT/ADR 不变；最后 app 增量构建与完整 smoke 输入一致。 | codegraph 未挂载；LSP reload 后仍误报 include 缺失并只返回单定义引用，AST .h 解析失败，均已报告。实际 MSVC/消费者是证据，不伪装为 LSP 零诊断或全量 warning/tidy 清零。 |

**失败与用户截图纠正单列**：首次集成编译发现移除旧游标 rewind 时一并删掉 `totalEvents` 定义，按原消费者恢复；独立程序首次把新增 object 放进 Ninja 的 implicit dependency 而非 linker input，链接报 main 缺失，已修复。最初菜单探针误把“选中已有相同索引”当成必触发 onChange，改为实际选择＋Load。导入 Take 自动播放时 Export WAV 按既有状态机禁用，验证先 Stop 而非强行点击 disabled 控件。用户截图确认保存自动化命中了文件列表的 `System.ItemNameDisplay`“名称”重命名控件，错误写入完整路径触发“重命名：指定的文件名无效或太长”；仅修探针，精确定位 filename host 下 `1001`，保存按钮 `1`，不修改产品来绕过失败。Native SDK 诊断 enum 在 Windows public header 不完整，按官方 API＋phnt 校验后仅在自有诊断程序动态查询；不改 SDK。资源冷初始化、7 秒空等不能替代同取消 UI 路径热身，保留冷差异与 AMD 线程归属，正式三轮仍严格比较完整进程总数。以上失败不是最终通过证据，最终全程序 exit=0。

**验证隔离**：TEMP/TMP 指向新 GUID 目录；真实 `[Environment]::GetFolderPath('ApplicationData')/DevPiano` 不重定向、不写测试 canary，只读前后完整快照。实际 Main 的一次性 import-slot scope 将自己的 `SHGetSpecialFolderPathW(CSIDL_APPDATA)` 输入指向自有 profile，先确认 JUCE 实际解析再构造应用，退出恢复；不改系统 profile 或子模块。Windows UIA 仅匹配本进程，filename 控件按 AutomationId/祖先筛选，不接触列表项、剪贴板或其他应用。

参考官方契约：[JUCE AudioDeviceManager](https://docs.juce.com/master/classjuce_1_1AudioDeviceManager.html)、[Thread](https://docs.juce.com/master/classjuce_1_1Thread.html)、[PluginDescription](https://docs.juce.com/master/classjuce_1_1PluginDescription.html)、[AudioProcessor](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html)、[UI Automation Value](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationclient/nf-uiautomationclient-iuiautomationvaluepattern-setvalue)、[GetProcessHandleCount](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesshandlecount)、[NtQueryObject](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryobject)、[NtQueryInformationThread](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationthread)、[诊断 enum 的 phnt 定义](https://ntdoc.m417z.com/threadinfoclass)。以本地 JUCE/Windows SDK 签名为准；private NT 查询只存在于一次性验收程序。

##### Windows 构建与隔离执行配方

1. WSL 使用 `./scripts/dev.sh win-build --sync-only` 同步；保留 Phase 0 Debug 子树和 `/Zc:nrvo-`（首次 configure 配方见 EVID-001），不清理原默认 Ninja 缓存、不进行 Release 或 WSL 软件验证。
2. 把下方 native CMake/CPP 保存为 `$build/phasec-smoke/native-src/CMakeLists.txt`、`NativePlugin.cpp`。把下面的 native 构建脚本保存为临时 `phasec-native.ps1`，执行后在 native-build 下得到三个真正的 VST3 package；不安装到用户插件目录。
3. `phasec-windows.ps1 -Mode Build` 构建 app/tests；`-Mode Test` 执行默认全套。实际消费者源码保存为 `$build/phasec-smoke/phasec_smoke.cpp`；先按下方 exact object linkage 生成 compile-smoke.cmd 和 phasec_smoke.ninja，再用 build-smoke.ps1 -Mode Smoke 编译/链接。
4. `phasec-windows.ps1 -Mode Smoke -NativeRoot "$build/phasec-smoke/native-build"` 运行完整程序；观察 Native/Transport/CANCEL_RESOURCES/APPLICATION 标记以及 mode JSON 的 exitCode=0、userDirectoryUnchanged=true、remainingTempEntries=0。UIA 只操作本进程实际控件。
5. baseline 源码按下方单列，只与 **同步前的 Phase B objects/headers** 链接；复建原基线应使用基线 commit 的独立镜像/构建目录，不能把新 API 的对象混入 baseline。当前 smoke 只链接当前 app objects，不链接 UnitTest 替代实现。
6. 以下完整输入/关键输出已经持久保存；本轮一次性源码、Ninja edge、二进制/图片与私有 scratch 已在验证后清理，原产品构建缓存和真实用户数据保留。

##### 完整 native VST3 配置

```cmake
cmake_minimum_required(VERSION 3.22)

# Global policy and debug format defaults
set(CMAKE_POLICY_DEFAULT_CMP0141 NEW)
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

# Force Debug build and Embedded /Z7 debug symbols for MSVC
set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>" CACHE STRING "" FORCE)

project(PhaseCTwinNativePlugins VERSION 1.0.0 LANGUAGES C CXX)

if (MSVC)
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/FS>)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/Zc:nrvo->)
    add_link_options("/INCREMENTAL:NO")
endif()

add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)

# Resolve devpiano workspace root and incorporate unmodified mirror JUCE
if (NOT DEFINED DEVPIANO_ROOT)
    get_filename_component(DEVPIANO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
endif()

if (NOT TARGET juce_audio_processors)
    add_subdirectory("${DEVPIANO_ROOT}/submodules/JUCE" "${CMAKE_CURRENT_BINARY_DIR}/JUCE_build")
endif()

# ==============================================================================
# Phase C Twin VST3 Verification Targets
# Three targets sharing NativePlugin.cpp and identical display name 'Phase C Twin'
# Target 1: PhaseCTwinA      (PLUGIN_CODE "Cone", IS_SYNTH TRUE)
# Target 2: PhaseCTwinB      (PLUGIN_CODE "Ctwo", IS_SYNTH TRUE)
# Target 3: PhaseCTwinEffect (PLUGIN_CODE "Cfxe", IS_SYNTH FALSE)
# ==============================================================================

# Target 1: PhaseCTwinA
juce_add_plugin(PhaseCTwinA
    COMPANY_NAME "DevPiano"
    PRODUCT_NAME "Phase C Twin"
    PLUGIN_NAME "Phase C Twin"
    DESCRIPTION "Phase C Twin"
    PLUGIN_MANUFACTURER_CODE "DevP"
    PLUGIN_CODE "Cone"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
)

target_sources(PhaseCTwinA PRIVATE NativePlugin.cpp)
target_compile_features(PhaseCTwinA PRIVATE cxx_std_20)
set_target_properties(PhaseCTwinA PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_definitions(PhaseCTwinA PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(PhaseCTwinA PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Target 2: PhaseCTwinB
juce_add_plugin(PhaseCTwinB
    COMPANY_NAME "DevPiano"
    PRODUCT_NAME "Phase C Twin"
    PLUGIN_NAME "Phase C Twin"
    DESCRIPTION "Phase C Twin"
    PLUGIN_MANUFACTURER_CODE "DevP"
    PLUGIN_CODE "Ctwo"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
)

target_sources(PhaseCTwinB PRIVATE NativePlugin.cpp)
target_compile_features(PhaseCTwinB PRIVATE cxx_std_20)
set_target_properties(PhaseCTwinB PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_definitions(PhaseCTwinB PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(PhaseCTwinB PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Target 3: PhaseCTwinEffect
juce_add_plugin(PhaseCTwinEffect
    COMPANY_NAME "DevPiano"
    PRODUCT_NAME "Phase C Twin"
    PLUGIN_NAME "Phase C Twin"
    DESCRIPTION "Phase C Twin"
    PLUGIN_MANUFACTURER_CODE "DevP"
    PLUGIN_CODE "Cfxe"
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
)

target_sources(PhaseCTwinEffect PRIVATE NativePlugin.cpp)
target_compile_features(PhaseCTwinEffect PRIVATE cxx_std_20)
set_target_properties(PhaseCTwinEffect PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_definitions(PhaseCTwinEffect PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(PhaseCTwinEffect PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Convenience target to build all three VST3 binaries
add_custom_target(phasec_twins
    DEPENDS PhaseCTwinA_VST3 PhaseCTwinB_VST3 PhaseCTwinEffect_VST3
)
```

```cpp
#include <atomic>
#include <cstdint>
#include <cwchar>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if __has_include(<JuceHeader.h>)
#include <JuceHeader.h>
#else
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#endif

#ifndef JucePlugin_Name
#define JucePlugin_Name "Phase C Twin"
#endif

#ifndef JucePlugin_IsSynth
#define JucePlugin_IsSynth 1
#endif

#ifndef JucePlugin_WantsMidiInput
#define JucePlugin_WantsMidiInput 1
#endif

class NativePhaseCTwinProcessor : public juce::AudioProcessor {
public:
    static constexpr uint32_t kStateMagic = 0x50484331u; // ASCII 'PHC1'
    static constexpr size_t kStateWords = 6;

    NativePhaseCTwinProcessor()
#if JucePlugin_IsSynth
        : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#else
        : juce::AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
    {
        gainParameter = new juce::AudioParameterFloat (juce::ParameterID { "gain", 1 }, "Gain", 0.0f, 1.0f, 0.5f);
        addParameter (gainParameter);
        offlineGateParameter = new juce::AudioParameterBool (juce::ParameterID { "gateOffline", 1 },
                                                             "Gate Offline Export", false);
        addParameter (offlineGateParameter);
        realtimeGateParameter = new juce::AudioParameterBool (juce::ParameterID { "gateRealtime", 1 },
                                                              "Gate Realtime Once", false);
        addParameter (realtimeGateParameter);

#if defined(_WIN32)
        const DWORD pid = GetCurrentProcessId();
        wchar_t enteredName[128] = { 0 };
        wchar_t releaseName[128] = { 0 };
        swprintf_s (enteredName, L"Local\\DevPianoPhaseC-%lu-entered", static_cast<unsigned long> (pid));
        swprintf_s (releaseName, L"Local\\DevPianoPhaseC-%lu-release", static_cast<unsigned long> (pid));

        // Create manual-reset events in constructor (owned and closed in destructor).
        // Handles will connect to existing events if parent created them first.
        enteredEvent = CreateEventW (nullptr, TRUE, FALSE, enteredName);
        releaseEvent = CreateEventW (nullptr, TRUE, FALSE, releaseName);
#endif
    }

    ~NativePhaseCTwinProcessor() override {
#if defined(_WIN32)
        if (enteredEvent != nullptr && enteredEvent != INVALID_HANDLE_VALUE) {
            CloseHandle (enteredEvent);
            enteredEvent = nullptr;
        }
        if (releaseEvent != nullptr && releaseEvent != INVALID_HANDLE_VALUE) {
            CloseHandle (releaseEvent);
            releaseEvent = nullptr;
        }
#endif
    }

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        juce::ignoreUnused (sampleRate, samplesPerBlock);
        // Mode comes exclusively from real host calls: record isNonRealtime()
        const bool offline = isNonRealtime();
        lastPrepareOffline.store (offline ? 1u : 0u, std::memory_order_release);
    }

    void releaseResources() override {
        // Atomic release counter for lifecycle verification
        releaseCount.fetch_add (1, std::memory_order_relaxed);
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {
        // Output must be stereo
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;

#if ! JucePlugin_IsSynth
        // Effect accepts stereo input (or disabled)
        if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()
            && layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
            return false;
#endif
        return true;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        juce::ScopedNoDenormals noDenormals;

        // Deterministic slow/callback ownership verification:
        // gateRequested applies once: native processBlock exchanges true->false,
        // signals named Win32 event then waits on a named release event.
#if defined(_WIN32)
        const auto realtimeGate = realtimeGateParameter->get();
        const auto gateByParameter = !isNonRealtime() && realtimeGate && !handledRealtimeGate;
        handledRealtimeGate = realtimeGate;
        if (gateByParameter || gateRequested.exchange (0u, std::memory_order_acq_rel) != 0u) {
            if (enteredEvent != nullptr && enteredEvent != INVALID_HANDLE_VALUE) {
                SetEvent (enteredEvent);
            }
            if (releaseEvent != nullptr && releaseEvent != INVALID_HANDLE_VALUE) {
                WaitForSingleObject (releaseEvent, INFINITE);
            }
        }
#else
        gateRequested.store (0u, std::memory_order_relaxed);
#endif

        const bool processOffline = isNonRealtime();
        lastProcessOffline.store (processOffline ? 1u : 0u, std::memory_order_release);
        processCount.fetch_add (1, std::memory_order_relaxed);

        const bool prepareOffline = (lastPrepareOffline.load (std::memory_order_acquire) != 0u);

        // Audible amplitude differences:
        // realtime: 0.125f, offline: 0.5f, mode disagreement: -0.5f
        float sampleValue = 0.0f;
        if (prepareOffline != processOffline) {
            sampleValue = -0.5f;
        } else if (processOffline) {
            sampleValue = 0.5f;
        } else {
            sampleValue = 0.125f;
        }
        sampleValue *= gainParameter->get() * 2.0f;

        const int totalNumInputChannels = getTotalNumInputChannels();
        const int totalNumOutputChannels = getTotalNumOutputChannels();

        for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear (i, 0, buffer.getNumSamples());

        for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
            auto* writePtr = buffer.getWritePointer (ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s) {
                writePtr[s] = sampleValue;
            }
        }

        // MIDI acceptance for synth and no MIDI output
        midiMessages.clear();
    }

    //==============================================================================
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override {
        return new juce::GenericAudioProcessorEditor (*this);
    }

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override {
#if JucePlugin_IsSynth
        return true;
#else
        return false;
#endif
    }

    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    // Stable explicit state diagnostics:
    // uint32 array [magic=0x50484331, gateRequested (0/1), lastPrepareOffline (0/1),
    //               lastProcessOffline (0/1), processCount, releaseCount]
    void getStateInformation (juce::MemoryBlock& destData) override {
        const uint32_t words[kStateWords] = {
            kStateMagic,
            (gateRequested.load (std::memory_order_acquire) != 0u || offlineGateParameter->get()) ? 1u : 0u,
            lastPrepareOffline.load (std::memory_order_acquire),
            lastProcessOffline.load (std::memory_order_acquire),
            processCount.load (std::memory_order_relaxed),
            releaseCount.load (std::memory_order_relaxed)
        };
        destData.replaceAll (words, sizeof (words));
    }

    void setStateInformation (const void* data, int sizeInBytes) override {
        if (data == nullptr || static_cast<size_t> (sizeInBytes) != sizeof (uint32_t) * kStateWords)
            return;

        const auto* words = static_cast<const uint32_t*> (data);
        if (words[0] != kStateMagic)
            return;

        // setState accepts same magic/6-word length and changes ONLY gateRequested.
        // Must NOT restore or rewrite current mode; mode comes exclusively from real host calls.
        gateRequested.store (words[1] != 0u ? 1u : 0u, std::memory_order_release);
    }

private:
#if defined(_WIN32)
    HANDLE enteredEvent { nullptr };
    HANDLE releaseEvent { nullptr };
#endif

    juce::AudioParameterFloat* gainParameter = nullptr;
    juce::AudioParameterBool* offlineGateParameter = nullptr;
    juce::AudioParameterBool* realtimeGateParameter = nullptr;
    bool handledRealtimeGate = false;
    std::atomic<uint32_t> gateRequested { 0 };
    std::atomic<uint32_t> lastPrepareOffline { 0 };
    std::atomic<uint32_t> lastProcessOffline { 0 };
    std::atomic<uint32_t> processCount { 0 };
    std::atomic<uint32_t> releaseCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativePhaseCTwinProcessor)
};

//==============================================================================
// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new NativePhaseCTwinProcessor();
}
```

```powershell
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    $vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe not found at $vswhere"
}

$instances = @((& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -format json) | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $instances.Count -lt 1) {
    throw 'VS discovery failed'
}

$devShellDll = Join-Path $instances[0].installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
if (-not (Test-Path -LiteralPath $devShellDll)) {
    throw "Microsoft.VisualStudio.DevShell.dll not found at $devShellDll"
}

Import-Module $devShellDll -Force
Enter-VsDevShell -VsInstallPath $instances[0].installationPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$probe = 'G:\source\projects\devpiano\build-win-msvc\audit004-phase0\phasec-smoke'
& cmake -S (Join-Path $probe 'native-src') -B (Join-Path $probe 'native-build') -G Ninja -DCMAKE_BUILD_TYPE=Debug -DDEVPIANO_ROOT='G:\source\projects\devpiano'
if ($LASTEXITCODE -ne 0) { throw 'Native VST3 configure failed' }
& cmake --build (Join-Path $probe 'native-build') --target phasec_twins --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Native VST3 build failed' }
Write-Output 'PHASE_C_NATIVE_VST3_BUILT=1'
```

##### 完整 Windows 保护脚本

```powershell
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet('Build', 'Test', 'Smoke', 'Baseline')]
    [string]$Mode,

    [Parameter(Position = 1)]
    [string]$NativeRoot = '',

    [Parameter()]
    [string]$Mirror = 'G:\source\projects\devpiano',

    [Parameter()]
    [string]$BuildDir = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$mirror = $Mirror
$build = if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    Join-Path $mirror 'build-win-msvc\audit004-phase0'
} else {
    if ([System.IO.Path]::IsPathRooted($BuildDir)) {
        $BuildDir
    } else {
        Join-Path $mirror $BuildDir
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    $vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe not found at $vswhere"
}

$instances = @((& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -format json) | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $instances.Count -lt 1) {
    throw 'VS discovery failed'
}

$devShellDll = Join-Path $instances[0].installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
if (-not (Test-Path -LiteralPath $devShellDll)) {
    throw "Microsoft.VisualStudio.DevShell.dll not found at $devShellDll"
}

Import-Module $devShellDll -Force
Enter-VsDevShell -VsInstallPath $instances[0].installationPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
Set-Location -LiteralPath $mirror

if ($Mode -eq 'Build') {
    $buildLog = Join-Path $build 'phasec-build.log'
    $origEap = $ErrorActionPreference
    $buildExit = -1
    try {
        $ErrorActionPreference = 'Continue'
        & cmake --build $build --target devpiano devpiano_tests --parallel 4 2>&1 | Tee-Object -FilePath $buildLog
        $buildExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $origEap
        if ($buildExit -eq -1 -and $null -ne $LASTEXITCODE) {
            $buildExit = $LASTEXITCODE
        }
    }
    $buildRecord = [ordered]@{
        mode = 'Build'
        buildDir = $build
        exitCode = $buildExit
    }
    $buildRecord | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'phasec-build-verification.json') -Encoding utf8
    if ($buildExit -ne 0) {
        throw "Debug build failed: $buildExit"
    }
    Write-Output 'PHASE_C_DEBUG_BUILD_PASSED=1'
    exit 0
}

function Get-UserSnapshot([string]$Path) {
    $rows = [System.Collections.Generic.List[object]]::new()
    $exists = Test-Path -LiteralPath $Path
    if ($exists) {
        foreach ($entry in @(Get-Item -LiteralPath $Path -Force) + @(Get-ChildItem -LiteralPath $Path -Force -Recurse | Sort-Object FullName)) {
            $hash = if ($entry.PSIsContainer) { '' } else { (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash }
            $length = if ($entry.PSIsContainer) { 0 } else { $entry.Length }
            $rows.Add([ordered]@{
                name = $entry.FullName
                directory = $entry.PSIsContainer
                length = $length
                attributes = [int]$entry.Attributes
                modified = $entry.LastWriteTimeUtc.Ticks
                hash = $hash
            })
        }
    }
    return (ConvertTo-Json -Depth 8 -Compress -InputObject ([ordered]@{ exists = $exists; rows = $rows.ToArray() }))
}

$userDir = Join-Path ([Environment]::GetFolderPath('ApplicationData')) 'DevPiano'
$before = Get-UserSnapshot $userDir
$private = Join-Path ([System.IO.Path]::GetTempPath()) ('devpiano-phasec-' + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($private) | Out-Null
$oldTemp = $env:TEMP
$oldTmp = $env:TMP
$env:TEMP = $private
$env:TMP = $private
$exitCode = -1

try {
    $origEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        switch ($Mode) {
            'Test' {
                $testLog = Join-Path $build 'phasec-default-tests.log'
                & ctest --test-dir $build -C Debug --verbose 2>&1 | Tee-Object -FilePath $testLog
                $exitCode = $LASTEXITCODE
            }
            'Smoke' {
                $smokeExe = Join-Path $build 'phasec-smoke\phasec_smoke.exe'
                $smokeLog = Join-Path $build 'phasec-smoke.log'
                $smokeArgs = @()
                if (-not [string]::IsNullOrWhiteSpace($NativeRoot)) {
                    $smokeArgs += $NativeRoot
                }
                & $smokeExe @smokeArgs 2>&1 | Tee-Object -FilePath $smokeLog
                $exitCode = $LASTEXITCODE
            }
            'Baseline' {
                $baselineExe = Join-Path $build 'phasec-smoke\phasec_baseline.exe'
                $baselineLog = Join-Path $build 'phasec-baseline.log'
                $baselineArgs = @()
                if (-not [string]::IsNullOrWhiteSpace($NativeRoot)) {
                    $baselineArgs += $NativeRoot
                }
                & $baselineExe @baselineArgs 2>&1 | Tee-Object -FilePath $baselineLog
                $exitCode = $LASTEXITCODE
            }
        }
    } finally {
        $ErrorActionPreference = $origEap
        if ($exitCode -eq -1 -and $null -ne $LASTEXITCODE) {
            $exitCode = $LASTEXITCODE
        }
    }
} finally {
    $env:TEMP = $oldTemp
    $env:TMP = $oldTmp
    $after = Get-UserSnapshot $userDir
    $remaining = @(Get-ChildItem -LiteralPath $private -Force -Recurse).Count
    $record = [ordered]@{
        mode = $Mode
        buildDir = $build
        exitCode = $exitCode
        userDirectory = $userDir
        userDirectoryUnchanged = ($before -ceq $after)
        privateTempDirectory = $private
        remainingTempEntries = $remaining
    }
    $recordJson = $record | ConvertTo-Json
    $recordFile = Join-Path $build ('phasec-' + $Mode.ToLowerInvariant() + '-verification.json')
    Set-Content -LiteralPath $recordFile -Value $recordJson -Encoding utf8
    Write-Output ('PHASE_C_VERIFICATION=' + ($record | ConvertTo-Json -Compress))
    if ($remaining -eq 0) {
        Remove-Item -LiteralPath $private
    }
    if ($before -cne $after) {
        throw 'Protected real user directory changed'
    }
    if ($remaining -ne 0) {
        throw 'Private scratch entries remain'
    }
}

if ($exitCode -ne 0) {
    throw "Consumer execution failed: $exitCode"
}
```

##### 完整实际消费者（生产应用类＋真实 VST3/音频/UI）

```cpp
#include <JuceHeader.h>
#include "MainComponent.h"
#include "Plugin/PluginHost.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/PerformanceFile.h"
#include "Recording/RecordingEngine.h"
#include "Export/WavExportTask.h"
#include "Settings/SettingsStore.h"
#include "UI/PluginPanelStateBuilder.h"
#include <windows.h>
#include <shlobj.h>
#include <uiautomation.h>
#include <winternl.h>
#include <map>
#include <array>
#include <cstring>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#undef START_JUCE_APPLICATION
#define START_JUCE_APPLICATION(AppClass)
#include "Main.cpp"

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void pump(int milliseconds = 50) {
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+milliseconds;
    while (juce::Time::getMillisecondCounterHiRes() < deadline) {
        MSG message {};
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        MsgWaitForMultipleObjects(0,nullptr,FALSE,3,QS_ALLINPUT);
    }
}
template<class Predicate> void until(Predicate predicate, const char* failure, int timeout=10000) {
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+timeout;
    while (!predicate() && juce::Time::getMillisecondCounterHiRes() < deadline) pump(10);
    require(predicate(),failure);
}
struct OwnedDirectory {
    juce::File directory=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("phasec-consumer-"+juce::Uuid().toString());
    OwnedDirectory() { require(directory.createDirectory().wasOk(),"scratch create failed"); }
    ~OwnedDirectory() { std::cout << "PHASE_C_PRIVATE_FILES_CLEAN=" << directory.deleteRecursively() << '\n'; }
};
class ProfileDirectoryScope {
public:
    ~ProfileDirectoryScope() {
        if (slot == nullptr) return;
        DWORD protection = 0;
        const bool restored = VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection) != FALSE;
        if (restored) {
            *slot = reinterpret_cast<ULONG_PTR>(original);
            DWORD ignored = 0;
            VirtualProtect(slot, sizeof(*slot), protection, &ignored);
        }
        std::cout << "PHASE_C_PROFILE_SCOPE_RESTORED=" << restored << '\n';
    }
    bool redirect(const juce::File& directory) {
        privatePath = directory.getFullPathName().toWideCharPointer();
        if (privatePath.size() >= MAX_PATH) return false;
        auto* base = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        const auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if (rva == 0) return false;
        auto* imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + rva);
        for (; imports->Name != 0; ++imports) {
            if (imports->OriginalFirstThunk == 0) continue;
            auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->OriginalFirstThunk);
            auto* entries = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->FirstThunk);
            for (; names->u1.AddressOfData != 0; ++names, ++entries) {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
                auto* imported = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
                if (std::strcmp(imported->Name, "SHGetSpecialFolderPathW") != 0) continue;
                auto* candidate = &entries->u1.Function;
                DWORD protection = 0;
                if (!VirtualProtect(candidate, sizeof(*candidate), PAGE_READWRITE, &protection)) return false;
                original = reinterpret_cast<NativeFolder>(static_cast<ULONG_PTR>(*candidate));
                *candidate = reinterpret_cast<ULONG_PTR>(&privateFolder);
                DWORD ignored = 0;
                VirtualProtect(candidate, sizeof(*candidate), protection, &ignored);
                slot = candidate;
                return true;
            }
        }
        return false;
    }
private:
    using NativeFolder = BOOL (WINAPI*)(HWND, LPWSTR, int, BOOL);
    static BOOL WINAPI privateFolder(HWND window, LPWSTR destination, int kind, BOOL create) {
        if (kind != CSIDL_APPDATA) return original(window, destination, kind, create);
        std::copy(privatePath.begin(), privatePath.end(), destination);
        destination[privatePath.size()] = 0;
        return TRUE;
    }
    ULONG_PTR* slot = nullptr;
    static inline NativeFolder original = nullptr;
    static inline std::wstring privatePath;
};

template<typename T> T* find(juce::Component& root, const juce::String& id) {
    if (root.getComponentID()==id) if (auto* candidate=dynamic_cast<T*>(&root)) return candidate;
    for (int i=0;i<root.getNumChildComponents();++i)
        if (auto* candidate=find<T>(*root.getChildComponent(i),id)) return candidate;
    return nullptr;
}
void click(juce::Component& root, const char* id) {
    auto* button=find<juce::Button>(root,id);
    std::cout << "PHASE_C_UI_ACTION id=" << id << " found=" << (button != nullptr)
              << " enabled=" << (button != nullptr && button->isEnabled()) << '\n';
    if (button == nullptr || !button->isEnabled()) throw std::runtime_error(std::string("production button unavailable: ")+id);
    button->triggerClick(); pump(100);
}
void screenshot(juce::Component& component,const char* fileName) {
    const auto image=component.createComponentSnapshot(component.getLocalBounds());
    auto file=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile(fileName);
    auto output=file.createOutputStream(); require(output != nullptr,"snapshot output failed");
    juce::PNGImageFormat format; require(format.writeImageToStream(image,*output),"snapshot encode failed");
}
struct Events {
    HANDLE entered=nullptr, released=nullptr;
    Events() {
        const auto prefix=juce::String("Local\\DevPianoPhaseC-")+juce::String(GetCurrentProcessId());
        entered=CreateEventW(nullptr,TRUE,FALSE,(prefix+"-entered").toWideCharPointer());
        released=CreateEventW(nullptr,TRUE,FALSE,(prefix+"-release").toWideCharPointer());
        require(entered != nullptr && released != nullptr,"event creation failed"); reset();
    }
    ~Events() { SetEvent(released); CloseHandle(entered); CloseHandle(released); }
    void reset() { require(ResetEvent(entered) && ResetEvent(released),"event reset failed"); }
    bool active() const { return WaitForSingleObject(entered,0)==WAIT_OBJECT_0; }
    void release() { require(SetEvent(released) != FALSE,"event release failed"); }
};
std::array<std::uint32_t,6> nativeWords(juce::AudioPluginInstance& instance) {
    juce::MemoryBlock state; instance.getStateInformation(state);
    auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
    require(xml != nullptr,"VST3 state envelope absent");
    auto* component=xml->getChildByName("IComponent"); require(component != nullptr,"component state absent");
    juce::MemoryBlock payload; require(payload.fromBase64Encoding(component->getAllSubText()),"component decode failed");
    std::array<std::uint32_t,6> words {};
    require(payload.getSize()>=sizeof(words),"native state truncated");
    std::memcpy(words.data(),payload.getData(),sizeof(words));
    require(words[0]==0x50484331u,"native state magic wrong"); return words;
}
void armNativeGate(juce::AudioPluginInstance& instance) {
    juce::MemoryBlock state; instance.getStateInformation(state);
    auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
    require(xml != nullptr,"gate envelope absent");
    auto* component=xml->getChildByName("IComponent"); require(component != nullptr,"gate component absent");
    juce::MemoryBlock payload; require(payload.fromBase64Encoding(component->getAllSubText()),"gate payload decode failed");
    require(payload.getSize()>=24,"gate payload short");
    const std::uint32_t requested=1; std::memcpy(static_cast<char*>(payload.getData())+4,&requested,sizeof(requested));
    component->deleteAllChildElements(); component->addTextElement(payload.toBase64Encoding());
    juce::AudioProcessor::copyXmlToBinary(*xml,state);
    instance.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
}
std::unique_ptr<juce::AudioPluginInstance> makeOffline(PluginHost& host,const juce::PluginDescription& description) {
    juce::String error; auto instance=devpiano::exporting::createOfflinePluginInstance(host.getFormatManager(),description,48000.0,512,error);
    require(instance != nullptr,"production offline factory failed"); return instance;
}
devpiano::recording::RecordingTake makeTake() {
    using namespace devpiano::recording;
    RecordingTake take; take.sampleRate=48000; take.lengthSamples=48000;
    take.events={{0,PerformanceEventType::midi,0,RecordingEventSource::playback,juce::MidiMessage::noteOn(1,60,0.8f)},
                 {10000,PerformanceEventType::midi,0,RecordingEventSource::playback,juce::MidiMessage::noteOff(1,60)},
                 {12000,PerformanceEventType::midi,0,RecordingEventSource::playback,juce::MidiMessage::noteOn(1,62,0.8f)},
                 {24000,PerformanceEventType::midi,0,RecordingEventSource::playback,juce::MidiMessage::noteOff(1,62)}};
    return take;
}
float readSample(const juce::File& file) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    require(reader != nullptr && reader->sampleRate==48000.0,"final WAV header invalid");
    juce::AudioBuffer<float> audio(2,512); require(reader->read(&audio,0,512,0,true,true),"WAV payload read failed");
    return audio.getSample(0,0);
}
std::array<juce::PluginDescription,3> verifyNative(PluginHost& host,const juce::File& root,const juce::File& scratch) {
    const auto a=root.getChildFile("PhaseCTwinA_artefacts/Debug/VST3/Phase C Twin.vst3");
    const auto b=root.getChildFile("PhaseCTwinB_artefacts/Debug/VST3/Phase C Twin.vst3");
    const auto effect=root.getChildFile("PhaseCTwinEffect_artefacts/Debug/VST3/Phase C Twin.vst3");
    const auto foundA=host.addVst3FileToKnownList(a), repeated=host.addVst3FileToKnownList(a);
    const auto foundB=host.addVst3FileToKnownList(b), foundEffect=host.addVst3FileToKnownList(effect);
    require(foundA.size()==1 && repeated.size()==1 && foundB.size()==1 && foundEffect.size()==1,"native identity detection failed");
    const std::array<juce::PluginDescription,3> descriptions {foundA[0],foundB[0],foundEffect[0]};
    auto xml=host.createKnownPluginListXml(); require(xml != nullptr,"native cache absent");
    for (auto* element:xml->getChildIterator()) if(element->getStringAttribute("file")==a.getFullPathName()) element->setAttribute("version","stale");
    require(host.restoreKnownPluginListFromXml(*xml),"stale metadata cache restore failed");
    require(host.addVst3FileToKnownList(a).size()==1,"duplicate metadata redetection failed");
    bool updated=false;
    for(const auto& description:host.getKnownPluginDescriptions()) if(description.isDuplicateOf(foundA[0])) updated=description.version==foundA[0].version;
    require(updated,"native metadata not updated");
    const auto panel=buildPluginPanelState(host,foundB[0].createIdentifierString(),false);
    require(panel.availablePlugins.size()==3,"same-name native choices collapsed");
    require(foundA[0].createIdentifierString()!=foundB[0].createIdentifierString(),"native IDs collide");
    require(foundA[0].isInstrument && foundB[0].isInstrument && !foundEffect[0].isInstrument,"native types misclassified");
    require(host.loadPluginByIdentifier(foundB[0].createIdentifierString(),48000.0,512),"native B load failed");
    require(host.getLoadedPluginDescription()->isDuplicateOf(foundB[0]),"selected native identity wrong");
    juce::AudioBuffer<float> audio(2,512); juce::MidiBuffer midi; audio.clear(); host.getInstance()->processBlock(audio,midi);
    require(std::abs(audio.getSample(0,0)-0.125f)<0.00001f,"native realtime output wrong");
    auto offline=makeOffline(host,foundB[0]); audio.clear(); offline->processBlock(audio,midi);
    const auto words=nativeWords(*offline);
    require(offline->isNonRealtime() && words[2]==1 && words[3]==1,"native setup/process offline modes disagree");
    require(std::abs(audio.getSample(0,0)-0.5f)<0.00001f,"native offline branch not used");
    const auto wav=scratch.getChildFile("native-offline.wav");
    devpiano::exporting::WavExportOptions options; options.sampleRate=48000;
    require(devpiano::exporting::renderTakeWithOfflinePlugin(makeTake(),wav,options,*offline),"native WAV render failed");
    require(std::abs(readSample(wav)-0.5f)<0.0001f,"WAV did not contain offline-branch samples");
    offline->releaseResources(); offline.reset(); host.unloadPlugin();
    std::cout << "PHASE_C_NATIVE repeated_types=1 descriptions=3 choices=3 metadata_updated=1 selected_B=1 offline_setup=1 offline_process=1 realtime_sample=0.125 offline_sample=0.5 wav_sample=0.5\n";
    return descriptions;
}
void verifyTransport() {
    devpiano::recording::RecordingEngine recording; AudioEngine audio; audio.setRecordingEngine(&recording);
    audio.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::sine); audio.prepareToPlay(512,48000);
    recording.setPlaybackLoopStartSample(0);recording.setPlaybackLoopEndSample(16000);
    recording.startPlayback(makeTake(),48000);
    juce::WaitableEvent firstBlock, speedPublished, secondNote, stopPublished;
    struct Gates { juce::WaitableEvent& speed; juce::WaitableEvent& stop; ~Gates(){ speed.signal(); stop.signal(); } } gates {speedPublished,stopPublished};
    auto renderer=std::async(std::launch::async,[&] {
        juce::AudioBuffer<float> buffer(2,512); const juce::AudioSourceChannelInfo info(&buffer,0,512);
        while(recording.getPlaybackPositionSamples()==0) audio.getNextAudioBlock(info);
        firstBlock.signal(); require(speedPublished.wait(10000),"speed publication wait failed");
        bool observedOff=false,observedOff62=false,loopRestarted=false,previous62=false;
        for(int block=0;block<48;++block) {
            const auto position=recording.getPlaybackPositionSamples();
            audio.getNextAudioBlock(info);
            if(!audio.getKeyboardState().isNoteOn(1,60)) observedOff=true;
            const auto note62=audio.getKeyboardState().isNoteOn(1,62);
            if(previous62 && !note62) observedOff62=true;
            previous62=note62;
            if(recording.getPlaybackPositionSamples()<position && audio.getKeyboardState().isNoteOn(1,60)) loopRestarted=true;
            if(loopRestarted && note62) break;
        }
        require(observedOff && observedOff62 && loopRestarted && audio.getKeyboardState().isNoteOn(1,62),
                "real callback skipped NoteOff or lost the rescaled loop");
        secondNote.signal(); require(stopPublished.wait(10000),"stop publication wait failed");
        audio.getNextAudioBlock(info);
        require(!recording.isPlaying() && !audio.getKeyboardState().isNoteOn(1,62),"real callback stop left a sounding note");
    });
    require(firstBlock.wait(10000),"first actual callback did not render");
    const auto oldPosition=recording.getPlaybackPositionSamples(); recording.setPlaybackSpeedMultiplier(2.0);
    require(recording.getPlaybackPositionSamples()==oldPosition && recording.getEffectivePlaybackSpeedMultiplier()==1.0,"speed publication modified in-flight block");
    speedPublished.signal(); require(secondNote.wait(10000),"second actual callback note missing");
    recording.requestPlaybackStop(); require(recording.isPlaying(),"stop publication modified in-flight callback");
    stopPublished.signal(); renderer.get(); audio.releaseResources();
    std::cout << "PHASE_C_TRANSPORT real_audio_callback=1 queued_speed=1 old_block_position_retained=1 effective_speed=2 NoteOff_delivered=1 loop_restarted=1 queued_stop=1 sounding_note_released=1\n";
}
using HandleSnapshot = std::map<std::uintptr_t,juce::String>;
HandleSnapshot handleSnapshot() {
    using Query = NTSTATUS (NTAPI*)(HANDLE,OBJECT_INFORMATION_CLASS,PVOID,ULONG,PULONG);
    const auto query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryObject"));
    require(query != nullptr,"handle type query unavailable");
    HandleSnapshot result;
    for(std::uintptr_t value=4;value<0x20000;value+=4) {
        alignas(void*) std::array<unsigned char,4096> buffer {};
        ULONG returned=0;
        if(query(reinterpret_cast<HANDLE>(value),ObjectTypeInformation,buffer.data(),
                 static_cast<ULONG>(buffer.size()),&returned)<0) continue;
        const auto* information=reinterpret_cast<const PUBLIC_OBJECT_TYPE_INFORMATION*>(buffer.data());
        result.emplace(value,juce::String(information->TypeName.Buffer,information->TypeName.Length/sizeof(wchar_t)));
    }
    return result;
}
void printHandleDelta(const HandleSnapshot& before,const HandleSnapshot& after,int cycle) {
    std::map<juce::String,int> delta;
    for(const auto& [handle,type]:before) --delta[type];
    for(const auto& [handle,type]:after) ++delta[type];
    for(const auto& [type,count]:delta) if(count != 0)
        std::cout << "PHASE_C_HANDLE_TYPE cycle=" << cycle << " type=" << type << " delta=" << count << '\n';
    for(const auto& [handle,type]:after) if(!before.contains(handle)) {
        std::cout << "PHASE_C_NEW_HANDLE cycle=" << cycle << " value=" << handle << " type=" << type;
        if(type=="Thread") {
            PWSTR description=nullptr;
            if(SUCCEEDED(GetThreadDescription(reinterpret_cast<HANDLE>(handle),&description))) {
                std::cout << " name=" << juce::String(description);
                LocalFree(description);
            }
            std::cout << " tid=" << GetThreadId(reinterpret_cast<HANDLE>(handle));
            using QueryThread=NTSTATUS (NTAPI*)(HANDLE,THREADINFOCLASS,PVOID,ULONG,PULONG);
            const auto queryThread=reinterpret_cast<QueryThread>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationThread"));
            void* start=nullptr;
            if(queryThread != nullptr && queryThread(reinterpret_cast<HANDLE>(handle),static_cast<THREADINFOCLASS>(9),
                                                      &start,sizeof(start),nullptr)>=0){
                HMODULE module=nullptr;
                if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                      reinterpret_cast<LPCWSTR>(start),&module)){
                    wchar_t path[MAX_PATH] {};GetModuleFileNameW(module,path,MAX_PATH);
                    std::cout << " start_module=" << juce::String(path);
                }
            }
        }
        std::cout << '\n';
    }
}
bool handleResourcesStable=true;
void verifyCancellation(PluginHost& host,const juce::PluginDescription& description,const juce::File& scratch) {
    Events events;
    devpiano::exporting::WavExportOptions options; options.sampleRate=48000;
    const auto warmupTarget=scratch.getChildFile("warmup-task.wav");
    auto warmup=std::make_unique<WavExportTask>(makeTake(),warmupTarget,options,makeOffline(host,description));
    bool warmupComplete=false;
    warmup->startAsync([&](bool ok,const juce::String&){require(ok,"native async warmup failed");warmupComplete=true;warmup.reset();});
    until([&]{return warmupComplete;},"native async warmup timeout");pump(150);
    require(std::abs(readSample(warmupTarget)-0.5f)<0.0001f,"background native WAV header/payload invalid");
    for (int cycle=0;cycle<5;++cycle) {
        events.reset();
        const auto detailedBefore=handleSnapshot();
        DWORD handlesBefore=0;require(GetProcessHandleCount(GetCurrentProcess(),&handlesBefore)!=FALSE,"handle count failed");
        auto plugin=makeOffline(host,description); armNativeGate(*plugin); auto* pointer=plugin.get();
        const auto releaseBefore=nativeWords(*pointer)[5];
        const auto target=scratch.getChildFile("cancel-retained.wav"); require(target.replaceWithText("retained-user-target"),"canary write failed");
        const auto filesBefore=scratch.findChildFiles(juce::File::findFiles,false);
        auto task=std::make_unique<WavExportTask>(makeTake(),target,options,std::move(plugin));
        bool completed=false, result=true; std::uint32_t releaseAfter=0;
        task->startAsync([&](bool ok,const juce::String&) {
            result=ok; releaseAfter=nativeWords(*pointer)[5]; completed=true;
            task.reset();
        });
        until([&]{return events.active();},"native slow worker did not enter");
        task->requestCancellation();
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+3300.0; int pumps=0;
        while(juce::Time::getMillisecondCounterHiRes()<deadline) { pump(20);++pumps; require(!completed,"slow worker completed before event release"); }
        require(task != nullptr && task->isRunning() && pumps>100,"async cancellation blocked message loop or released ownership");
        events.release(); until([&]{return completed;},"cooperative worker never completed");
        require(!result && releaseAfter==releaseBefore+1,"worker completion or resource release wrong");
        require(target.loadFileAsString()=="retained-user-target","cancelled worker altered old target");
        pump(150);
        DWORD handlesAfter=0;require(GetProcessHandleCount(GetCurrentProcess(),&handlesAfter)!=FALSE,"final handle count failed");
        const auto filesAfter=scratch.findChildFiles(juce::File::findFiles,false);
        const auto detailedAfter=handleSnapshot();
        printHandleDelta(detailedBefore,detailedAfter,cycle);
        std::cout << "PHASE_C_CANCEL_RESOURCES cycle=" << cycle << " warmup=" << (cycle<2)
                  << " handles_before=" << handlesBefore << " handles_after=" << handlesAfter
                  << " temporary_file_delta=" << filesAfter.size()-filesBefore.size() << '\n';
        require(filesAfter.size()==filesBefore.size(),"cancelled render left temporary writer files");
        if (cycle>=2 && handlesAfter>handlesBefore) handleResourcesStable=false;
        std::cout << "PHASE_C_CANCEL gate_held_ms=3300 responsive_pumps=" << pumps << " no_early_completion=1 native_released_once=1 self_deleting_callback=1 original_target_retained=1\n";
    }
}

template<class T> class ComPtr {
public:
    ~ComPtr(){ if(pointer != nullptr) pointer->Release(); }
    T** out(){ require(pointer==nullptr,"COM output already owned");return &pointer; }
    T* get()const{return pointer;}
    T* operator->()const{return pointer;}
    void swap(ComPtr& other) noexcept { std::swap(pointer,other.pointer); }
private:T* pointer=nullptr;
};
struct Automation {
    ComPtr<IUIAutomation> instance;
    Automation(){ require(SUCCEEDED(CoCreateInstance(__uuidof(CUIAutomation),nullptr,CLSCTX_INPROC_SERVER,__uuidof(IUIAutomation),reinterpret_cast<void**>(instance.out()))),"UI automation init failed"); }
    void ownedElements(ComPtr<IUIAutomationElementArray>& elements,HWND window=nullptr){
        ComPtr<IUIAutomationElement> root;
        if(window != nullptr) require(SUCCEEDED(instance->ElementFromHandle(window,root.out())),"dialog automation root failed");
        else require(SUCCEEDED(instance->GetRootElement(root.out())),"desktop automation root failed");
        VARIANT value {};value.vt=VT_I4;value.lVal=static_cast<LONG>(GetCurrentProcessId());
        ComPtr<IUIAutomationCondition> condition;
        require(SUCCEEDED(instance->CreatePropertyCondition(UIA_ProcessIdPropertyId,value,condition.out())),"process condition failed");
        require(SUCCEEDED(root->FindAll(TreeScope_Descendants,condition.get(),elements.out())),"automation descendant query failed");
    }
};
juce::String automationName(IUIAutomationElement& element){
    BSTR name=nullptr; element.get_CurrentName(&name); const juce::String text(name);SysFreeString(name);return text;
}
void toggleNative(const char* labelText){
    auto action=std::async(std::launch::async,[labelText]{
        const auto initialised=CoInitializeEx(nullptr,COINIT_MULTITHREADED);require(SUCCEEDED(initialised),"automation COM init failed");
        struct Uninitialise{~Uninitialise(){CoUninitialize();}} uninitialise;
        Automation automation; ComPtr<IUIAutomationElementArray> elements;automation.ownedElements(elements);
        int count=0;elements->get_Length(&count);RECT label {};
        bool foundLabel=false;
        for(int i=0;i<count;++i){ComPtr<IUIAutomationElement> element;elements->GetElement(i,element.out());
            if(automationName(*element.get())==labelText){element->get_CurrentBoundingRectangle(&label);foundLabel=true;break;}}
        require(foundLabel,"native parameter label absent");
        for(int i=0;i<count;++i){ComPtr<IUIAutomationElement> element;elements->GetElement(i,element.out());
            RECT bounds {};element->get_CurrentBoundingRectangle(&bounds);
            if(bounds.left < label.right || bounds.bottom <= label.top || bounds.top >= label.bottom)continue;
            ComPtr<IUnknown> pattern;
            if(FAILED(element->GetCurrentPattern(UIA_TogglePatternId,pattern.out())) || pattern.get()==nullptr)continue;
            ComPtr<IUIAutomationTogglePattern> toggle;
            require(SUCCEEDED(pattern->QueryInterface(__uuidof(IUIAutomationTogglePattern),reinterpret_cast<void**>(toggle.out()))),"toggle pattern unavailable");
            require(SUCCEEDED(toggle->Toggle()),"native parameter toggle failed");return;
        }
        throw std::runtime_error("native boolean control absent");
    });
    until([&]{return action.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;},"native control action timed out");action.get();pump(100);
}
HWND ownedFileDialog(){
    HWND result=nullptr;
    EnumWindows([](HWND window,LPARAM destination)->BOOL{
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);wchar_t name[128] {};
        GetClassNameW(window,name,128);
        if(pid==GetCurrentProcessId() && IsWindowVisible(window) && std::wcscmp(name,L"#32770")==0){*reinterpret_cast<HWND*>(destination)=window;return FALSE;}
        return TRUE;
    },reinterpret_cast<LPARAM>(&result));
    return result;
}
void acceptFileDialog(const juce::File& file){
    HWND dialog=nullptr;until([&]{dialog=ownedFileDialog();return dialog!=nullptr;},"native save dialog did not appear");
    auto action=std::async(std::launch::async,[dialog,file]{
        require(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)),"save automation COM init failed");
        struct Uninitialise{~Uninitialise(){CoUninitialize();}} uninitialise;
        Automation automation;ComPtr<IUIAutomationElementArray> elements;automation.ownedElements(elements,dialog);
        int count=0;elements->get_Length(&count);bool setFile=false;
        ComPtr<IUIAutomationTreeWalker> walker;
        require(SUCCEEDED(automation.instance->get_RawViewWalker(walker.out())),"save ancestry walker unavailable");
        ComPtr<IUIAutomationElement> saveButton;
        for(int i=0;i<count;++i){
            ComPtr<IUIAutomationElement> element;elements->GetElement(i,element.out());
            BSTR rawId=nullptr;element->get_CurrentAutomationId(&rawId);
            const juce::String identifier(rawId);SysFreeString(rawId);
            CONTROLTYPEID control=0;element->get_CurrentControlType(&control);
            if(identifier=="1" && control==UIA_ButtonControlTypeId)
                require(SUCCEEDED(element->QueryInterface(__uuidof(IUIAutomationElement),reinterpret_cast<void**>(saveButton.out()))),"save button binding failed");
            if(control!=UIA_EditControlTypeId)continue;
            std::cout << "PHASE_C_SAVE_EDIT id=" << identifier << " name=" << automationName(*element.get()) << '\n';
            if(identifier!="1001" && identifier!="1148")continue;
            ComPtr<IUIAutomationElement> parent;walker->GetParentElement(element.get(),parent.out());
            bool filenameHost=false;
            while(parent.get()!=nullptr){
                BSTR parentId=nullptr;parent->get_CurrentAutomationId(&parentId);
                const juce::String hostId(parentId);SysFreeString(parentId);
                if(hostId=="FileNameControlHost" || hostId=="1148"){filenameHost=true;break;}
                ComPtr<IUIAutomationElement> next;walker->GetParentElement(parent.get(),next.out());
                if(next.get()==nullptr)break;
                BSTR className=nullptr;next->get_CurrentClassName(&className);
                const bool desktop=juce::String(className)=="#32769";SysFreeString(className);
                if(desktop)break;
                parent.swap(next);
            }
            if(!filenameHost)continue;
            ComPtr<IUnknown> pattern;
            if(FAILED(element->GetCurrentPattern(UIA_ValuePatternId,pattern.out())) || pattern.get()==nullptr)continue;
            ComPtr<IUIAutomationValuePattern> value;
            require(SUCCEEDED(pattern->QueryInterface(__uuidof(IUIAutomationValuePattern),reinterpret_cast<void**>(value.out()))),"filename value pattern absent");
            BOOL readOnly=TRUE,enabled=FALSE;
            value->get_CurrentIsReadOnly(&readOnly);element->get_CurrentIsEnabled(&enabled);
            require(!readOnly && enabled,"filename edit is not writable");
            auto target=SysAllocString(file.getFullPathName().toWideCharPointer());
            const auto status=value->SetValue(target);SysFreeString(target);
            std::cout << "PHASE_C_SAVE_FILENAME id=" << identifier << " status=" << std::hex << status << std::dec << '\n';
            require(SUCCEEDED(status),"filename edit failed");setFile=true;
        }
        require(setFile && saveButton.get()!=nullptr,"precise filename or save control absent");
        ComPtr<IUnknown> savePattern;
        require(SUCCEEDED(saveButton->GetCurrentPattern(UIA_InvokePatternId,savePattern.out())) && savePattern.get()!=nullptr,"save action pattern absent");
        ComPtr<IUIAutomationInvokePattern> invoke;
        require(SUCCEEDED(savePattern->QueryInterface(__uuidof(IUIAutomationInvokePattern),reinterpret_cast<void**>(invoke.out()))),"save invocation binding failed");
        require(SUCCEEDED(invoke->Invoke()),"native save button invocation failed");
    });
    until([&]{return action.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;},"save dialog action timed out");action.get();
    until([&]{return ownedFileDialog()==nullptr;},"save dialog did not close");
}
MainComponent& applicationMain(){
    auto& desktop=juce::Desktop::getInstance();
    for(int i=0;i<desktop.getNumComponents();++i)
        if(auto* window=dynamic_cast<juce::DocumentWindow*>(desktop.getComponent(i)))
            if(auto* main=dynamic_cast<MainComponent*>(window->getContentComponent()))return *main;
    throw std::runtime_error("production main window absent");
}
PluginEditorWindow* editorWindow(){
    auto& desktop=juce::Desktop::getInstance();
    for(int i=0;i<desktop.getNumComponents();++i)
        if(auto* editor=dynamic_cast<PluginEditorWindow*>(desktop.getComponent(i)))return editor;
    return nullptr;
}
void selectIdentity(MainComponent& main,const char* suffix,const juce::String& expected){
    auto* selector=find<juce::ComboBox>(main,"plugin-selector");require(selector != nullptr,"actual selector missing");
    int index=-1;for(int i=0;i<selector->getNumItems();++i)if(selector->getItemText(i).contains(suffix)){index=i;break;}
    require(index>=0,"actual native choice missing");selector->setSelectedItemIndex(index,juce::sendNotificationSync);pump(150);
    click(main,"load-btn");
    std::cout << "PHASE_C_UI_SELECTION expected=" << expected << " actual=" << main.getAppSettings().lastPluginIdentifier << '\n';
    require(main.getAppSettings().lastPluginIdentifier==expected,"actual selection loaded or persisted wrong native identity");
}
void gatedMutation(Events& events,const std::function<void()>& mutate){
    events.reset();toggleNative("Gate Realtime Once");until([&]{return events.active();},"real audio callback gate did not enter");
    auto completed=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(completed!=nullptr,"mutation event failed");
    auto controller=std::async(std::launch::async,[&]{
        const auto result=WaitForSingleObject(completed,150);
        events.release();return result;
    });
    mutate();SetEvent(completed);const auto result=controller.get();CloseHandle(completed);
    require(result==WAIT_TIMEOUT,"mutation bypassed active callback stop boundary");
}
void verifyApplication(const juce::File& profile,const juce::File& nativeRoot,
                       const std::array<juce::PluginDescription,3>& descriptions,const juce::File& scratch){
    SettingsModel model;model.languageCode="en";model.masterGain=0.0f;
    model.pluginSearchPath=nativeRoot.getFullPathName();model.lastPluginIdentifier=descriptions[1].createIdentifierString();
    juce::KnownPluginList list;for(const auto& description:descriptions)list.addType(description);
    model.knownPluginListState=list.createXml();
    { SettingsStore store;require(store.save(model),"private startup settings write failed"); }
    Events events;
    juce::JUCEApplicationBase::createInstance=[]()->juce::JUCEApplicationBase*{return new DevPianoApplication();};
    DevPianoApplication application;
    struct Shutdown { Events& events;DevPianoApplication& application;~Shutdown(){events.release();application.shutdown();} } shutdown {events,application};
    application.initialise("--sine");pump(200);
    auto& main=applicationMain();
    require(main.getAppSettings().lastPluginIdentifier==descriptions[1].createIdentifierString(),"startup did not restore B identity");
    auto* selector=find<juce::ComboBox>(main,"plugin-selector");auto* filter=find<juce::ComboBox>(main,"plugin-filter-combo");
    require(selector != nullptr && selector->getNumItems()==3 && filter != nullptr,"same-name actual selector collapsed");
    filter->setSelectedId(2,juce::sendNotificationSync);pump(100);require(selector->getNumItems()==2,"instrument filter admitted an effect");
    selectIdentity(main,descriptions[0].createIdentifierString().toRawUTF8(),descriptions[0].createIdentifierString());
    filter->setSelectedId(3,juce::sendNotificationSync);pump(100);require(selector->getNumItems()==1,"effect filter lost or admitted wrong types");
    selectIdentity(main,descriptions[2].createIdentifierString().toRawUTF8(),descriptions[2].createIdentifierString());
    filter->setSelectedId(1,juce::sendNotificationSync);pump(100);
    selectIdentity(main,descriptions[1].createIdentifierString().toRawUTF8(),descriptions[1].createIdentifierString());
    screenshot(main,"phasec-same-name-selection.png");
    click(main,"editor-btn");require(editorWindow()!=nullptr,"real native editor did not open");
    juce::Component::SafePointer<PluginEditorWindow> oldEditor(editorWindow());
    gatedMutation(events,[&]{auto* button=find<juce::Button>(main,"scan-btn");require(button && button->onClick,"scan consumer missing");button->onClick();});
    require(oldEditor==nullptr,"scan retained native editor to old instance");
    until([&]{auto* button=find<juce::Button>(main,"scan-btn");return button != nullptr && button->isEnabled();},"incremental scan did not finish");pump(200);
    require(selector->getNumItems()==3,"rescan lost native identities");
    selectIdentity(main,descriptions[1].createIdentifierString().toRawUTF8(),descriptions[1].createIdentifierString());
    click(main,"editor-btn");require(editorWindow()!=nullptr,"native editor did not reopen");
    gatedMutation(events,[&]{application.anotherInstanceStarted("--piano");});
    require(main.getAppSettings().builtinTone==SettingsModel::BuiltinTone::piano,"another-instance tone command failed");
    application.anotherInstanceStarted("--sine");require(main.getAppSettings().builtinTone==SettingsModel::BuiltinTone::sine,"second tone rebuild failed");
    click(main,"unload-btn");
    main.filesDropped({descriptions[1].fileOrIdentifier},0,0);pump(200);
    require(main.getAppSettings().lastPluginIdentifier==descriptions[1].createIdentifierString(),"rediscovered duplicate failed to load B");
    const auto performance=scratch.getChildFile("application-take.devpiano");
    require(devpiano::recording::savePerformanceFile(makeTake(),performance),"native Take file write failed");
    main.filesDropped({performance.getFullPathName()},0,0);pump(200);
    if (auto* stop=find<juce::Button>(main,"stop-btn"); stop != nullptr && stop->isEnabled()) click(main,"stop-btn");
    until([&]{auto* button=find<juce::Button>(main,"export-wav-btn");return button != nullptr && button->isEnabled();},"export did not become available after playback stop");
    click(main,"editor-btn");require(editorWindow()!=nullptr,"export native editor absent");
    toggleNative("Gate Offline Export");
    events.reset();
    main.getAppSettings().lastMidiExportPath=scratch.getChildFile("last.wav").getFullPathName();
    const auto target=scratch.getChildFile("app-cancel.wav");
    click(main,"export-wav-btn");acceptFileDialog(target);
    until([&]{return events.active();},"actual application offline export did not enter gate");
    application.systemRequestedQuit();
    require(!juce::MessageManager::getInstance()->hasStopMessageBeenSent(),"application quit before real export exit");
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+3300;int pumps=0;
    while(juce::Time::getMillisecondCounterHiRes()<deadline){pump(20);++pumps;require(!juce::MessageManager::getInstance()->hasStopMessageBeenSent(),"pending quit forcibly ended worker");}
    screenshot(main,"phasec-quit-waits-for-export.png");
    for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)
        if(auto* dialog=dynamic_cast<juce::DialogWindow*>(juce::Desktop::getInstance().getComponent(i)))
            screenshot(*dialog,"phasec-cancelling-progress.png");
    events.release();
    until([&]{return juce::MessageManager::getInstance()->hasStopMessageBeenSent();},"application did not quit after cooperative exit");
    require(!target.existsAsFile(),"cancelled app export committed an unfinished file");
    std::cout << "PHASE_C_APPLICATION startup_B=1 choices=3 instrument_filter=2 effect_filter=1 loaded_id_exact=1 editor_rescan_guard_wait=1 tone_callback_guard_wait=1 piano_sine_commands=1 duplicate_redrop=1 actual_save_dialog=1 quit_deferred_ms=3300 responsive_pumps=" << pumps << " quit_after_real_exit=1 unfinished_output_absent=1\n";
    static_cast<void>(profile);
}
}
int main(int argc,char** argv){
    try{
        juce::ScopedJuceInitialiser_GUI initialise;require(argc==2,"native fixture root required");
        OwnedDirectory owned; const juce::File nativeRoot(argv[1]);
        const auto profile=owned.directory.getChildFile("profile");require(profile.createDirectory().wasOk(),"profile create failed");
        ProfileDirectoryScope scope;require(scope.redirect(profile),"profile import-slot redirect failed");
        require(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)==profile,"JUCE private profile was not resolved");
        PluginHost host;host.setDeadMansPedalFile(owned.directory.getChildFile("pedal.txt"));
        const auto descriptions=verifyNative(host,nativeRoot,owned.directory);
        verifyTransport();verifyCancellation(host,descriptions[0],owned.directory);
        verifyApplication(profile,nativeRoot,descriptions,owned.directory);
        require(handleResourcesStable,"handle attribution remains unresolved");
        return 0;
    }catch(const std::exception& error){std::cerr << "PHASE_C_SMOKE_ERROR=" << error.what() << '\n';return 1;}
}
```

##### 基线真实消费者

```cpp
#include <JuceHeader.h>
#include "Plugin/PluginHost.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/RecordingEngine.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct OwnedDirectory {
    juce::File file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("phasec-baseline-" + juce::Uuid().toString());
    OwnedDirectory() { require(file.createDirectory().wasOk(), "scratch create failed"); }
    ~OwnedDirectory() { std::cout << "PHASE_C_BASELINE_PRIVATE_CLEAN=" << file.deleteRecursively() << '\n'; }
};
}
int main(int argc, char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI initialise;
        require(argc == 2, "native root argument required");
        OwnedDirectory scratch;
        const juce::File root(argv[1]);
        PluginHost host;
        host.setDeadMansPedalFile(scratch.file.getChildFile("pedal.txt"));
        const auto nativeA = root.getChildFile("PhaseCTwinA_artefacts/Debug/VST3/Phase C Twin.vst3");
        const auto first = host.addVst3FileToKnownList(nativeA);
        const auto repeated = host.addVst3FileToKnownList(nativeA);
        const auto second = host.addVst3FileToKnownList(root.getChildFile("PhaseCTwinB_artefacts/Debug/VST3/Phase C Twin.vst3"));
        const auto effect = host.addVst3FileToKnownList(root.getChildFile("PhaseCTwinEffect_artefacts/Debug/VST3/Phase C Twin.vst3"));
        require(first.size()==1 && second.size()==1 && effect.size()==1, "three native types required");
        const auto xml = host.createKnownPluginListXml();
        require(xml != nullptr && xml->getNumChildElements()==3, "distinct native descriptions required");
        juce::PluginDescription description;
        require(description.loadFromXml(*xml->getChildElement(0)), "description decode failed");
        juce::String error;
        auto offline = devpiano::exporting::createOfflinePluginInstance(host.getFormatManager(), description, 48000.0, 128, error);
        require(offline != nullptr, "production offline factory failed");
        juce::AudioBuffer<float> buffer(2,128);
        juce::MidiBuffer midi;
        buffer.clear();
        offline->processBlock(buffer, midi);
        juce::MemoryBlock state;
        offline->getStateInformation(state);
        const auto envelope = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
        require(envelope != nullptr, "native VST3 envelope absent");
        const auto* component = envelope->getChildByName("IComponent");
        require(component != nullptr, "native component state absent");
        juce::MemoryBlock payload;
        require(payload.fromBase64Encoding(component->getAllSubText()), "native component state decode failed");
        std::array<std::uint32_t,6> words {};
        require(payload.getSize() >= sizeof(words), "native diagnostic state absent");
        std::memcpy(words.data(), payload.getData(), sizeof(words));
        require(words[0] == 0x50484331u, "native diagnostic magic invalid");
        std::cout << "PHASE_C_BASELINE_PLUGIN repeated_types=" << repeated.size()
            << " native_description_count=" << xml->getNumChildElements()
            << " selectable_name_count=" << host.getKnownPluginNames().size()
            << " offline_flag=" << offline->isNonRealtime()
            << " native_setup_offline=" << words[2] << " native_process_offline=" << words[3]
            << " sample=" << buffer.getSample(0,0) << '\n';
        offline->releaseResources();
        offline.reset();

        devpiano::recording::RecordingTake take;
        take.sampleRate = 1000.0;
        take.lengthSamples=1000;
        take.events.push_back({0, devpiano::recording::PerformanceEventType::midi,0,
            devpiano::recording::RecordingEventSource::playback, juce::MidiMessage::noteOn(1,60,1.0f)});
        take.events.push_back({700, devpiano::recording::PerformanceEventType::midi,0,
            devpiano::recording::RecordingEventSource::playback, juce::MidiMessage::noteOff(1,60)});
        devpiano::recording::RecordingEngine engine;
        engine.startPlayback(take,1000.0);
        engine.renderPlaybackBlock(midi,0,100);
        engine.advancePlaybackPosition(100);
        const auto before=engine.getPlaybackPositionSamples();
        engine.setPlaybackSpeedMultiplier(2.0);
        const auto after=engine.getPlaybackPositionSamples();
        engine.stopPlayback();
        std::cout << "PHASE_C_BASELINE_TRANSPORT position_before=" << before
            << " position_after_publish=" << after << " stop_immediate=" << !engine.isPlaying() << '\n';
        require(repeated.isEmpty() && host.getKnownPluginNames().size()==1 && !words[2] && !words[3]
            && before==100 && after==50, "expected baseline observations absent");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PHASE_C_BASELINE_ERROR=" << error.what() << '\n';
        return 1;
    }
}
```

##### 精确编译与链接复建

- 从该树 compile_commands.json 取 MainComponent.cpp 的唯一 app command，保持全部 define/include/PCH/运行库与 `/Zc:nrvo-`；仅替换 `/Fo`、`/Fd` 和末尾 `-c` 源 token，分别指向 phasec-smoke/phasec_smoke.obj、phasec_smoke_compile.pdb、phasec_smoke.cpp。baseline 用相应 stem。
- 新 Ninja 中 `include build.ninja`，复制真实 DevPiano.exe linker edge、属性与全部业务/JUCE object，只移除 Main.cpp.obj 原入口，加入探针 object；**必须放在第一个 ` | ` 之前的 explicit input**，不是 implicit library dependency。`/subsystem:windows` 改为 `/subsystem:console`。
- OBJECT_DIR、TARGET_SUPPORT_DIR、TARGET_COMPILE_PDB、TARGET_FILE、TARGET_IMPLIB、TARGET_PDB、RSP_FILE 全部指向自有 phasec-smoke/stem 路径，其余 CONFIG/FLAGS/LINK_LIBRARIES/PRE_LINK/POST_BUILD 保留；不改原 app edge、PCH、manifest 或 response file。
- 下面 driver 仅编译上述 command 并链接自有 edge；产品 app/tests 构建和默认执行仍走上方保护脚本。

```powershell
param([ValidateSet('Baseline','Smoke')][string]$Mode)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    $vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe not found at $vswhere"
}

$instances = @((& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -format json) | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0 -or $instances.Count -lt 1) {
    throw 'VS discovery failed'
}

$devShellDll = Join-Path $instances[0].installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
if (-not (Test-Path -LiteralPath $devShellDll)) {
    throw "Microsoft.VisualStudio.DevShell.dll not found at $devShellDll"
}

Import-Module $devShellDll -Force
Enter-VsDevShell -VsInstallPath $instances[0].installationPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$build='G:\source\projects\devpiano\build-win-msvc\audit004-phase0'
Set-Location -LiteralPath $build
$stem = if ($Mode -eq 'Baseline') { 'phasec_baseline' } else { 'phasec_smoke' }
$cmdFile = if ($Mode -eq 'Baseline') { 'phasec-smoke\compile-baseline.cmd' } else { 'phasec-smoke\compile-smoke.cmd' }
& cmd.exe /D /C $cmdFile
if ($LASTEXITCODE -ne 0) { throw 'Consumer compile failed' }
$make = (Select-String -LiteralPath (Join-Path $build 'CMakeCache.txt') -Pattern '^CMAKE_MAKE_PROGRAM:FILEPATH=').Line.Split('=',2)[1]
& $make -f ($stem + '.ninja') ('phasec-smoke\' + $stem + '.exe')
if ($LASTEXITCODE -ne 0) { throw 'Consumer link failed' }
```

**最终关键输出（一次性观察，不是固定门槛）**：

```text
PHASE_C_NATIVE repeated_types=1 descriptions=3 choices=3 metadata_updated=1 selected_B=1 offline_setup=1 offline_process=1 realtime_sample=0.125 offline_sample=0.5 wav_sample=0.5
PHASE_C_TRANSPORT real_audio_callback=1 queued_speed=1 old_block_position_retained=1 effective_speed=2 NoteOff_delivered=1 loop_restarted=1 queued_stop=1 sounding_note_released=1
PHASE_C_CANCEL_RESOURCES cycle=2 warmup=0 handles_before=1152 handles_after=1152 temporary_file_delta=0
PHASE_C_CANCEL_RESOURCES cycle=3 warmup=0 handles_before=1152 handles_after=1152 temporary_file_delta=0
PHASE_C_CANCEL_RESOURCES cycle=4 warmup=0 handles_before=1152 handles_after=1152 temporary_file_delta=0
PHASE_C_SAVE_FILENAME id=1001 status=0
PHASE_C_APPLICATION startup_B=1 choices=3 instrument_filter=2 effect_filter=1 loaded_id_exact=1 editor_rescan_guard_wait=1 tone_callback_guard_wait=1 piano_sine_commands=1 duplicate_redrop=1 actual_save_dialog=1 quit_deferred_ms=3300 responsive_pumps=154 quit_after_real_exit=1 unfinished_output_absent=1
PHASE_C_PROFILE_SCOPE_RESTORED=1
PHASE_C_PRIVATE_FILES_CLEAN=1
```


### AUDIT-004 Phase D：发音身份与采样级 Transport 边界 [已完成，2026-10-04]

**目标**：重叠同音、移调、pause、末尾、seek/loop、count-in、设备重建和踏板均有确定性语义。

**依赖**：Phase B/C；游标/倍率/设备时间域的所有权先于新增边界逻辑。

NoteOff 永远对应原发音；捕获/播放/跳转/节拍/设备采样域边界有采样级可观察验收。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [x] | `QUAL-001` | P1 | 播放移调/掩码变化不锁定已发音身份。播放侧也保存 NoteOn 最终输出身份，NoteOff按原身份；定义重叠同音及变化时正在发声的处理，不用当前映射重算。 | On 与 Off 之间改 offset/enabled/mask，Off 仍释放原输出身份；不依赖稍后 panic。 |
| [x] | `QUAL-002` | P1 | 重复物理键同音在首个松键时被提前关闭。在最终发音身份层维护重叠持有者，最后释放再NoteOff；保留每个物理键原身份。 | 默认 Q/K 同音及矩阵合并音高交错松键，剩余持有者仍响，最后释放才关闭。 |
| [x] | `QUAL-003` | P1 | 暂停录制丢掉期间唯一的 NoteOff/踏板释放。在冻结捕获时间轴的边界补齐已录身份/踏板终结状态；暂停期间排除新演奏，但保证保留Take配对。 | 暂停捕获期间释放先前已录音符/踏板，保留 Take 和 MIDI 导出仍配对；暂停中新演奏不混入。 |
| [x] | `QUAL-004` | P1 | 精确播放末尾的 NoteOff 未在音频路径交付。播放长度包含最后事件或在音频边界明确终结；对齐实时/离线可听结束和最终NoteOff采样点。 | 最后 Off 精确等于 Take 长度/块末时仍在音频路径交付，结束不等待 UI timer 才清音。 |
| [x] | `ERR-003` | P1 | 接受并序列化的 keyUp 绑定没有执行入口。兑现公开keyUp触发的可配对事件语义或在准入明确拒绝；不能接受文件配置后静默丢弃。 | 接受的 keyUp 绑定有完整触发/配对语义；若产品边界不支持，则准入显式拒绝，不接受后无声。 |
| [x] | `QUAL-018` | P1 | 设备采样率变更未重基准活动播放/录制时间域。设备prepare时统一重基准活动Transport；录制按固定Take域换算，或明确先结束会话；验证位置/倍速/NoteOff连续性。 | 活动录制/回放切 48k↔44.1k、暂停/恢复及倍速仍保持 Take-relative 时长、位置和发音身份。 |
| [x] | `QUAL-017` | P2 | Seek/循环回跳未恢复目的位置控制器及音色状态。在目标音符前恢复目的位置的状态快照，保持不自动重发历史NoteOn的现有策略；测试program/bank/CC64及pitch。 | seek/回跳在目标音符前恢复 program/bank/CC64/pitch 状态，不意外重发历史 NoteOn；16通道独立验证。 |
| [x] | `FIX-035` | P2 | 预备拍在最后一拍起音而非下一下拍完成。以完整音频节拍时段/下一个目标downbeat完成并消费序号差；实际控制器验证一/两小节及跨多拍poll。 | 120 BPM、4/4 一小节在完整2秒后的目标 downbeat 开始；多拍序号跳变、取消/重建不改变预备拍时长。 |
| [x] | `QUAL-019` | P2 | 踩下柔音踏板后新分配声部不继承CC67状态。由乐器拥有者维护当前踏板状态，保证每个新起声部继承；保留VST3通道语义并验证踏板先于和弦、换声部及释放。 | 先CC67再和弦、重分配/偷声部和释放，所有当前/新起物理声部继承正确柔音状态，不破坏VST3通道。 |

#### Phase D 实施记录与直接验证（2026-10-04）

**基线与范围**：`794518f6f613801fedeb6ad250446c43e2273f2c`（Phase C 本地交付）；仅本阶段九项及其捕获/回放/导出/乐器拥有者边界。原 AUDIT/ADR/Phase 35 档案不回写，Phase E 尚未开始。原计划 54 个 ID/优先级完整保留，本阶段只勾选原九行。

- **原身份与持有**：播放按 source channel/pitch 的预分配 FIFO 保存每次起音的最终输出身份，按最终输出计持有；Off 不用当前 enabled/offset/mask 重算。物理键保留各自快照，首个同音松键不关剩余持有者，最终释放/失焦才关音。重起音保留 articulation；捕获把实际最后松键在该采样点规范化为所有已录起音的配对 Off。
- **捕获与设备域**：暂停/停止在 callback 停机守卫内闭合已录身份和 CC64/66/67，再冻结/快照；暂停中新演奏及无已录身份的 Off 不混入。Take rate 固定，设备样本和事件偏移换算并保留小数余量；活动/暂停播放重基准设备位置与倍率，下一未渲染游标和同代身份不清空。
- **末尾与目的状态**：实时与离线同一缩放取整，最后事件 `+1` 覆盖；音频终结在有效事件边界交付，整块边界仍在下一 callback 偏移 0 收尾，再通知 UI。自动结束 UI 不重复 Stop，保留释放尾音。Seek/回跳在目标音符前恢复 16 通道 bank/program/CC/pitch，历史 NoteOn 不重发。
- **预备拍**：停机预分配/arm；音频完整时段到达后返回块内捕获起点，起点前事件排除、起点事件记 0。UI 延迟/跨多拍不启动第二次录制；音频已开始时立即 Stop/Play 等先接管该会话。取消、重新 arm、静音和真实 release/prepare 不重新计算已经消耗的时段。活动 warmup 继续 MIDI/DSP/计时，节拍仍混入；新播放 pre-roll 不被 warmup 提前消耗。
- **绑定与乐器**：显式 keyUp/未知 trigger 准入拒绝；缺省仍 keyDown，不静默迁移非法绑定。`BuiltinSynthesiser.h/.cpp` 按通道拥有 CC67 连续值，新 Piano 起音前继承；机械聚合监听不冒充真实起音/释放/柔音通道。内置 WAV 共用拥有者。Sine voice 在采样率更新时重算 ADSR 系数。MIDI 读写不调用自动配对来插入重复起音的假 Off。

| 证据 | 原问题 / 直接消费者 | 实际观察 | 范围与限制 |
| --- | --- | --- | --- |
| EVID-026 | 修复前 Phase C 生产 app objects；下方 baseline | pause 后 on=1/off=0/pedal_up=0；精确末尾 ended=1/off=0；seek 无 bank/program/pedal/pitch 恢复；Q/K 剩一键但 sounding=0；移调后原音仍 on；切率后 playback=24000、capture=44100。 | 不混用新 header/objects，不执行 UAF/OOM；复建原基线需该 commit 的独立镜像。 |
| EVID-027 | Windows MSVC Debug `build-win-msvc/audit004-phase0`，保留 `/Zc:nrvo-`；app/tests；默认 ctest 无 category/name | 最终构建通过；本次默认 98 套件、99,279 通过断言、零失败，Chord 完整进入默认执行。真实用户目录前后清单/mtime/属性/SHA256 相同；私有 TEMP/TMP 零残留。 | 数字仅这次执行，非固定门槛；无 Release/WSL 软件测试，无全量 tidy 或项目 warning 清零声明。 |
| EVID-028 | QUAL-001/002；真实 MidiKeyboardState、生产 AudioEngine、实际 native VST3 | Q/K 双释放次序、矩阵合并、切组/替换布局与失焦通过；native 重叠攻击 FIFO、offset/enabled/mask 变化仍释放原身份，最终音频静音。 | native fixture 使用官方 JUCE wrapper 的真实 binary，不是 mock format；不外推所有厂商插件。 |
| EVID-029 | QUAL-003；生产捕获状态机 → MIDI 写出/读回/再导入 | on=2/off=2，暂停边界为 24000，踏板 up=1，暂停中新演奏排除；固定 48000 Take rate、总长120000；显式 MIDI 配对保留。 | 既有 960 PPQ tick 量化不变；不宣称亚 tick 采样时差可无损写入 MIDI。 |
| EVID-030 | QUAL-004；同输入生产 AudioEngine 与 WavFileExporter 真正样本/文件 | 48k Off@128、有效长129；44.1k Off@118、有效长119；实时/WAV 最大差分别 `3.04207e-05`、`3.02829e-05`（16-bit 量化），零 UI poll 即交付。 | 对照为相同 Sine/ADSR 参数；物理/VST3完整声学快照与预设闭包仍属 Phase E。 |
| EVID-031 | ERR-003；loadPreset、savePreset 与实际 Main filesDropped | 显式 keyUp/未知拒绝，缺省 keyDown 正常；原文件字节与实际 `preset-combo` 选择保持；没有把接受失败写成无声成功。 | 不新增 keyUp 发音生命周期；原合法文件格式保留。 |
| EVID-032 | QUAL-018；生产 prepare/release、录制/回放及默认 2x/暂停回归 | 48k↔44.1k 固定 Take 域与位置保持；native 切率后原 63 Off 真正静音；暂停不推进捕获。 | 安全 CPU 模拟设备 lifecycle，不等同物理声卡热插拔/异常尺寸验证。 |
| EVID-033 | QUAL-017；生产 render MIDI 的 16 通道目的状态消费者 | 目标音符前 bank/program/CC64/pitch 均正确；原历史音不重发；分块跨 B 后在同块重新应用 A 状态与事件。 | 复用 `[A,B)` 与最小一块的循环边界，不扩展通用 DAW 状态图。 |
| EVID-034 | FIX-035；实际 RecordingSessionController ＋ Main 窗口、注入的生产 AudioEngine/RecordingEngine CPU 块 | 128/512 blocks、一/两小节：96000/192000 正确起点，零 UI poll 后立即 Play 正确接管/暂停/恢复；半程 release/prepare 后新44.1k域剩余44100；静音、取消重启及第一块早于起点排除/正好起点记0通过。 | controller 用其公开依赖注入，Main 为实际原生窗口/守卫/状态消费者；不把 CPU 时间推进说成声卡 callback 实机切率。 |
| EVID-035 | QUAL-019；实际物理声部、偷声部、WAV writer 与 native 16通道 | chord 起音前继承连续 CC67；0/80/127 的能量 `5.94625/4.03106/3.05399`；重分配/释放通道隔离；native 原通道 CC67 全部正确，未改写 VST3 通道。 | 能量为固定测试输入观察，不是声学 SLA；全回调无锁/无分配/零三角仍未验收。 |
| EVID-036 | 工具、门禁与可视输入 | self-check/format --check 通过；当前 RecordingEngine clangd diagnostics OK；实际 Main 快照含 Count-in Cancelled，原用户目录不变。 | references 仍漏调用点，使用 codegraph blast radius 补充；Windows 编译为最终依据，不以局部诊断代全量静态门禁。 |

**中途失败与实际修复单列**：默认旧用例把所有 MIDI 消息都当音符、固定总事件数，已移除 incidental 总数约束而保留音符采样/身份 oracle；新 16通道循环输入改为真实一块大小，未放宽最小循环限制。完整预备拍整块对齐曾延迟一拍，已保留修复后边界回归。实际文件消费者发现 MIDI 自动配对插入 Off，writer/importer改为保留显式流；WAV对照发现 Sine ADSR 旧采样域，更新 rate hook 后真正样本对齐，不改原时间域测试数值。复用输出 buffer 的边界清零也有永久回归。

**探针/环境错误单列**：探针起初用 Piano 机械宽监听判断真实 MIDI 通道、把 selector 猜成 preset-selector；最终使用基类真实通道身份与已有 preset-combo，未改产品绕过。一次 C1356：mspdbcore.dll 实际存在，PDB DLL/srv 版本同为14.51.36260.0且直接加载成功；最终使用同版本 compiler 的长路径、原定义/include/PCH/运行库/flags复编成功，不宣称根因已确认或已修复 MSVC。native构建的子模块 C4819告警保留，未修改第三方源码。

##### Windows 复建与隔离执行配方

1. 用项目 `./scripts/dev.sh win-build --sync-only` 同步主树；沿用 Phase 0 Debug 子树与 `/Zc:nrvo-`（首次 configure 见 EVID-001），不改原默认树失效缓存。
2. 在 `$build/phased-smoke/native-src/` 保存下方 native CMake/CPP，执行下方 native-driver.ps1 构建真实 package；不安装到用户插件目录。
3. Windows 保护 driver 的 Build 构建 app/tests，Test 运行默认 ctest；真实用户目录只读取快照，TEMP/TMP指向可删除私有目录。
4. Consumer 保存为 `$build/phased-smoke/phased_smoke.cpp`。从编译数据库 MainComponent.cpp 的唯一 app command 保留所有 flags/PCH，替换末尾 `-c` 源、`/Fo`、`/Fd` 指向该 consumer；compiler token 用同版本 `Get-Command cl.exe` 长路径并加引号，写 `compile-smoke-long.cmd`。
5. 新 `phased_smoke.ninja` 中 `include build.ninja`，复制实际 DevPiano.exe linker edge 与全部业务/JUCE inputs，仅移除 Main.cpp.obj，consumer obj插入第一个 ` | ` 之前的 explicit input；`/subsystem:windows`改console。OBJECT_DIR/TARGET_SUPPORT_DIR/TARGET_COMPILE_PDB/TARGET_FILE/TARGET_IMPLIB/TARGET_PDB/RSP_FILE只定向自有 phased-smoke，其他属性不变。Smoke 仅链接当前 app objects，不用 UnitTest 替代实现。
6. driver -Mode Smoke 必须观察全部下方标记、exitCode=0、userDirectoryUnchanged=true、remainingTempEntries=0；检查实际控制器快照。保存完整输入后清理自有源码/binary/Ninja/图片/临时目录，不删除产品缓存/用户文件。以下代码和关键输出为持久复建入口；临时 GUID 路径不构成依赖。
7. baseline 只用同步前 Phase C objects/headers；当前代码的调用契约已切换，不为历史消费者保留别名。复建基线需独立镜像，不能用当前对象回跑原 baseline。

##### 完整原生 VST3 配置与消费者

```cmake
cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_DEFAULT_CMP0141 NEW)
set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>" CACHE STRING "" FORCE)
project(PhaseDNativeFixture VERSION 1.0.0 LANGUAGES C CXX)
if(MSVC)
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/FS> $<$<COMPILE_LANGUAGE:C,CXX>:/Zc:nrvo->)
    add_link_options("/INCREMENTAL:NO")
endif()
add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)
add_subdirectory("G:/source/projects/devpiano/submodules/JUCE" "${CMAKE_CURRENT_BINARY_DIR}/JUCE_build")
juce_add_plugin(PhaseDNative
    COMPANY_NAME "DevPiano"
    PRODUCT_NAME "Phase D Native MIDI"
    PLUGIN_NAME "Phase D Native MIDI"
    DESCRIPTION "Phase D Native MIDI"
    PLUGIN_MANUFACTURER_CODE "DevP"
    PLUGIN_CODE "Dmid"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
)
target_sources(PhaseDNative PRIVATE NativePlugin.cpp)
target_compile_features(PhaseDNative PRIVATE cxx_std_20)
target_link_libraries(PhaseDNative PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
```

```cpp
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <cstring>

class NativeMidiInstrument final : public juce::AudioProcessor {
public:
    NativeMidiInstrument() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
    const juce::String getName() const override { return "Phase D Native MIDI"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layout) const override {
        return layout.getMainInputChannelSet().isDisabled() && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "MIDI"; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void getStateInformation(juce::MemoryBlock& destination) override {
        destination.setSize(sizeof(channels)); std::memcpy(destination.getData(), channels.data(), sizeof(channels));
    }
    void setStateInformation(const void* data, int size) override {
        if (size == sizeof(channels)) std::memcpy(channels.data(), data, sizeof(channels));
    }
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override {
        audio.clear();
        int rendered = 0;
        for (auto item : midi) {
            const int at = std::clamp(item.samplePosition, rendered, audio.getNumSamples());
            render(audio, rendered, at); rendered = at;
            const auto message = item.getMessage();
            const int channel = message.getChannel() - 1;
            if (channel < 0 || channel >= 16) continue;
            auto& state = channels[static_cast<std::size_t>(channel)];
            if (message.isNoteOn()) state.notes[static_cast<std::size_t>(message.getNoteNumber())] = true;
            else if (message.isNoteOff()) state.notes[static_cast<std::size_t>(message.getNoteNumber())] = false;
            else if (message.isController()) {
                const int controller = message.getControllerNumber();
                if (controller == 67) state.soft = message.getControllerValue();
                if (controller == 0) state.bank = message.getControllerValue();
                if (controller == 32) state.lsb = message.getControllerValue();
                if (controller == 64) state.sustain = message.getControllerValue();
                if (controller == 120 || controller == 123) state.notes.fill(false);
                if (controller == 121) { state.soft = 0; state.sustain = 0; state.pitch = 8192; }
            } else if (message.isProgramChange()) state.program = message.getProgramChangeNumber();
            else if (message.isPitchWheel()) state.pitch = message.getPitchWheelValue();
        }
        render(audio, rendered, audio.getNumSamples());
    }
private:
    struct ChannelState {
        std::array<bool, 128> notes {};
        int soft = 0, sustain = 0, bank = 0, lsb = 0, program = 0, pitch = 8192;
    };
    std::array<ChannelState, 16> channels {};
    void render(juce::AudioBuffer<float>& audio, int start, int end) {
        float noteSignal = 0.0f, stateSignal = 0.0f;
        for (std::size_t channel = 0; channel < channels.size(); ++channel) {
            const auto& state = channels[channel];
            for (std::size_t note = 0; note < state.notes.size(); ++note) {
                if (state.notes[note]) {
                    noteSignal += (0.02f + 0.001f * static_cast<float>(channel + 1) + 0.0001f * static_cast<float>(note))
                        * (1.0f - 0.3f * static_cast<float>(state.soft) / 127.0f);
                    stateSignal += 0.0001f * static_cast<float>(state.program) + 0.00001f * static_cast<float>(state.bank)
                        + 0.000001f * static_cast<float>(state.lsb) + 0.001f * static_cast<float>(state.sustain) / 127.0f
                        + 0.0000001f * static_cast<float>(state.pitch);
                }
            }
        }
        for (int sample = start; sample < end; ++sample) {
            audio.setSample(0, sample, noteSignal);
            if (audio.getNumChannels() > 1) audio.setSample(1, sample, stateSignal);
        }
    }
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NativeMidiInstrument(); }
```

```powershell
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$instances=@((& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -format json)|ConvertFrom-Json)
if($LASTEXITCODE -ne 0 -or $instances.Count -lt 1){throw 'VS discovery failed'}
Import-Module (Join-Path $instances[0].installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll') -Force
Enter-VsDevShell -VsInstallPath $instances[0].installationPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'|Out-Null
$probe='G:\source\projects\devpiano\build-win-msvc\audit004-phase0\phased-smoke'
& cmake -S (Join-Path $probe 'native-src') -B (Join-Path $probe 'native-build') -G Ninja -DCMAKE_BUILD_TYPE=Debug
if($LASTEXITCODE -ne 0){throw 'Native Debug configuration failed'}
& cmake --build (Join-Path $probe 'native-build') --target PhaseDNative_VST3 --parallel 4
if($LASTEXITCODE -ne 0){throw 'Native Debug build failed'}
```

##### 完整 Windows 保护 driver

```powershell
param([ValidateSet('Build','Test','Baseline','Smoke')][string]$Mode)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$mirror='G:\source\projects\devpiano'
$build=Join-Path $mirror 'build-win-msvc\audit004-phase0'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$instances=@((& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -format json)|ConvertFrom-Json)
if($LASTEXITCODE -ne 0 -or $instances.Count -lt 1){throw 'VS discovery failed'}
Import-Module (Join-Path $instances[0].installationPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll') -Force
Enter-VsDevShell -VsInstallPath $instances[0].installationPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'|Out-Null
Set-Location -LiteralPath $build
if($Mode -eq 'Build') {
    & cmake --build $build --target devpiano devpiano_tests --parallel 4 2>&1|Tee-Object -FilePath (Join-Path $build 'phased-build.log')
    if($LASTEXITCODE -ne 0){throw 'Phase D Debug build failed'}
    Write-Output 'PHASE_D_DEBUG_BUILD_PASSED=1'
    exit 0
}
if($Mode -ne 'Test') {
    $stem=if($Mode -eq 'Baseline'){'phased_baseline'}else{'phased_smoke'}
    $cmd=if($Mode -eq 'Baseline'){'phased-smoke\compile-baseline.cmd'}else{'phased-smoke\compile-smoke-long.cmd'}
    & cmd.exe /D /C $cmd
    if($LASTEXITCODE -ne 0){throw 'Consumer compile failed'}
    $make=(Select-String -LiteralPath (Join-Path $build 'CMakeCache.txt') -Pattern '^CMAKE_MAKE_PROGRAM:FILEPATH=').Line.Split('=',2)[1]
    & $make -f ($stem+'.ninja') ('phased-smoke\'+$stem+'.exe')
    if($LASTEXITCODE -ne 0){throw 'Consumer link failed'}
}
function Get-UserSnapshot([string]$Path) {
    $rows=[System.Collections.Generic.List[object]]::new()
    $exists=Test-Path -LiteralPath $Path
    if($exists) {
        foreach($entry in @(Get-Item -LiteralPath $Path -Force)+@(Get-ChildItem -LiteralPath $Path -Force -Recurse|Sort-Object FullName)) {
            $hash=if($entry.PSIsContainer){''}else{(Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash}
            $length=if($entry.PSIsContainer){0}else{$entry.Length}
            $rows.Add([ordered]@{name=$entry.FullName;directory=$entry.PSIsContainer;length=$length;attributes=[int]$entry.Attributes;modified=$entry.LastWriteTimeUtc.Ticks;hash=$hash})
        }
    }
    return (ConvertTo-Json -Depth 8 -Compress -InputObject ([ordered]@{exists=$exists;rows=$rows.ToArray()}))
}
$userDir=Join-Path ([Environment]::GetFolderPath('ApplicationData')) 'DevPiano'
$before=Get-UserSnapshot $userDir
$private=Join-Path ([System.IO.Path]::GetTempPath()) ('devpiano-phased-'+[guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($private)|Out-Null
$oldTemp=$env:TEMP;$oldTmp=$env:TMP;$env:TEMP=$private;$env:TMP=$private
$exitCode=-1
try {
    if($Mode -eq 'Test') {
        & ctest --test-dir $build -C Debug --verbose 2>&1|Tee-Object -FilePath (Join-Path $build 'phased-default-tests.log')
    } else {
        & (Join-Path $build ('phased-smoke\'+$stem+'.exe')) 2>&1|Tee-Object -FilePath (Join-Path $build ('phased-'+$Mode.ToLowerInvariant()+'.log'))
    }
    $exitCode=$LASTEXITCODE
} finally {
    $env:TEMP=$oldTemp;$env:TMP=$oldTmp
    $after=Get-UserSnapshot $userDir
    $remaining=@(Get-ChildItem -LiteralPath $private -Force -Recurse).Count
    $record=[ordered]@{mode=$Mode;buildDir=$build;exitCode=$exitCode;userDirectory=$userDir;userDirectoryUnchanged=($before -ceq $after);remainingTempEntries=$remaining}
    $record|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $build ('phased-'+$Mode.ToLowerInvariant()+'-verification.json')) -Encoding utf8
    Write-Output ('PHASE_D_VERIFICATION='+($record|ConvertTo-Json -Compress))
    if($remaining -eq 0){Remove-Item -LiteralPath $private}
    if($before -cne $after){throw 'Protected real user directory changed'}
    if($remaining -ne 0){throw 'Private scratch entries remain'}
}
if($exitCode -ne 0){throw ('Consumer failed: '+$exitCode)}
```

##### 完整实际生产消费者

```cpp
#include <JuceHeader.h>
#include "Audio/AudioEngine.h"
#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PianoSynthVoice.h"
#include "Input/KeyboardMidiMapper.h"
#include "Layout/PerformancePreset.h"
#include "MainComponent.h"
#include "Midi/MidiChannelMapper.h"
#include "Plugin/PluginHost.h"
#include "Recording/MidiFileExporter.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RecordingSessionController.h"
#include "Recording/RenderPipeline.h"
#include "Recording/WavFileExporter.h"
#include "Settings/SettingsStore.h"
#include <windows.h>
#include <shlobj.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#undef START_JUCE_APPLICATION
#define START_JUCE_APPLICATION(AppClass)
#include "Main.cpp"

using namespace devpiano::recording;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void printNativeAddress(void* address) {
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+512]{};
    auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=511;
    DWORD64 displacement=0;
    if(SymFromAddr(GetCurrentProcess(),reinterpret_cast<DWORD64>(address),&displacement,symbol))
        std::fprintf(stderr,"PHASE_D_NATIVE_STACK=%s+%llu
",symbol->Name,static_cast<unsigned long long>(displacement));
    else std::fprintf(stderr,"PHASE_D_NATIVE_STACK_ADDRESS=%p
",address);
}
LONG WINAPI reportNativeFailure(EXCEPTION_POINTERS* failure) {
    std::fprintf(stderr,"PHASE_D_NATIVE_EXCEPTION=%08lx address=%p
",failure->ExceptionRecord->ExceptionCode,failure->ExceptionRecord->ExceptionAddress);
    printNativeAddress(failure->ExceptionRecord->ExceptionAddress);
    void* frames[48]{};const auto count=CaptureStackBackTrace(0,48,frames,nullptr);
    for(USHORT n=0;n<count;++n)printNativeAddress(frames[n]);
    std::fflush(stderr);return EXCEPTION_EXECUTE_HANDLER;
}
PerformanceEvent event(std::int64_t sample, juce::MidiMessage message) {
    return {sample, PerformanceEventType::midi, 0, RecordingEventSource::playback, message};
}
RecordingTake takeWith(std::int64_t length, std::initializer_list<PerformanceEvent> events) {
    return {48000.0, length, events};
}
struct OwnedDirectory {
    juce::File directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("phased-consumer-"+juce::Uuid().toString());
    OwnedDirectory() { require(directory.createDirectory().wasOk(),"scratch create failed"); }
    ~OwnedDirectory() { std::cout << "PHASE_D_PRIVATE_FILES_CLEAN=" << directory.deleteRecursively() << '
'; }
};
class ProfileScope {
    using NativeFolder=BOOL(WINAPI*)(HWND,LPWSTR,int,BOOL);
    static inline NativeFolder original=nullptr;
    static inline std::wstring privatePath;
    ULONG_PTR* slot=nullptr;
    static BOOL WINAPI folder(HWND window,LPWSTR destination,int kind,BOOL create) {
        if(kind!=CSIDL_APPDATA)return original(window,destination,kind,create);
        std::copy(privatePath.begin(),privatePath.end(),destination);destination[privatePath.size()]=0;return TRUE;
    }
public:
    bool redirect(const juce::File& directory) {
        privatePath=directory.getFullPathName().toWideCharPointer();
        auto* base=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
        auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto* nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
        auto rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if(rva==0 || privatePath.size()>=MAX_PATH)return false;
        auto* imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);
        for(;imports->Name!=0;++imports) {
            if(imports->OriginalFirstThunk==0)continue;
            auto* names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+imports->OriginalFirstThunk);
            auto* entries=reinterpret_cast<IMAGE_THUNK_DATA*>(base+imports->FirstThunk);
            for(;names->u1.AddressOfData!=0;++names,++entries) {
                if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
                auto* imported=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
                if(std::strcmp(imported->Name,"SHGetSpecialFolderPathW")!=0)continue;
                DWORD protection=0;
                if(!VirtualProtect(&entries->u1.Function,sizeof(entries->u1.Function),PAGE_READWRITE,&protection))return false;
                slot=&entries->u1.Function;original=reinterpret_cast<NativeFolder>(*slot);*slot=reinterpret_cast<ULONG_PTR>(&folder);
                DWORD ignored=0;VirtualProtect(slot,sizeof(*slot),protection,&ignored);return true;
            }
        }
        return false;
    }
    ~ProfileScope() {
        if(slot==nullptr)return;
        DWORD protection=0;const bool restored=VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&protection)!=FALSE;
        if(restored){*slot=reinterpret_cast<ULONG_PTR>(original);DWORD ignored=0;VirtualProtect(slot,sizeof(*slot),protection,&ignored);}
        std::cout << "PHASE_D_PROFILE_RESTORED=" << restored << '
';
    }
};
void pump(int milliseconds) {
    const auto end=juce::Time::getMillisecondCounterHiRes()+milliseconds;
    while(juce::Time::getMillisecondCounterHiRes()<end) {
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        MsgWaitForMultipleObjects(0,nullptr,FALSE,2,QS_ALLINPUT);
    }
}
template<typename T>T* find(juce::Component& root,const juce::String& id) {
    if(root.getComponentID()==id)if(auto* match=dynamic_cast<T*>(&root))return match;
    for(int i=0;i<root.getNumChildComponents();++i)if(auto* match=find<T>(*root.getChildComponent(i),id))return match;
    return nullptr;
}
MainComponent& applicationMain() {
    for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i) {
        if(auto* window=dynamic_cast<juce::DocumentWindow*>(juce::Desktop::getInstance().getComponent(i)))
            if(auto* main=dynamic_cast<MainComponent*>(window->getContentComponent()))return *main;
    }
    throw std::runtime_error("actual Main window missing");
}
void closeDialogs() {
    for(int i=juce::Desktop::getInstance().getNumComponents()-1;i>=0;--i)
        if(auto* dialog=dynamic_cast<juce::DialogWindow*>(juce::Desktop::getInstance().getComponent(i)))dialog->closeButtonPressed();
}
void runBlock(AudioEngine& audio,juce::AudioBuffer<float>& buffer) { audio.getNextAudioBlock({&buffer,0,buffer.getNumSamples()}); }
void warmIdle(AudioEngine& audio,int block=128,double rate=48000.0) {
    juce::AudioBuffer<float> buffer(2,block);
    for(int n=0;n<AudioEngine::calculateWarmupBlockCount(rate,block);++n)runBlock(audio,buffer);
}

juce::File verifyInput(const juce::File& scratch) {
    for(const bool releaseQFirst:{true,false}) {
        KeyboardMidiMapper mapper;juce::MidiKeyboardState state;std::set<int> down{'Q','K'};
        mapper.setKeyStatePredicate([&](int key){return down.contains(key);});
        require(mapper.handleKeyPressed(juce::KeyPress('Q'),state),"Q not handled");
        require(mapper.handleKeyPressed(juce::KeyPress('K'),state),"K not handled");
        mapper.switchToNextGroup();mapper.setLayout(devpiano::core::makeDefaultKeyboardLayout());
        down.erase(releaseQFirst?'Q':'K');mapper.handleKeyStateChanged(state);
        require(state.isNoteOn(1,72),"first holder release closed live note");
        down.clear();mapper.handleKeyStateChanged(state);require(!state.isNoteOn(1,72),"last holder did not close live note");
    }
    devpiano::midi::ChannelMatrix matrix;matrix.channels[0].outputChannel=0;matrix.channels[1].outputChannel=0;matrix.channels[1].transpose=2;
    devpiano::midi::MidiChannelMapper service(matrix,false,0);
    KeyboardMidiMapper mapper;auto layout=devpiano::core::makeDefaultKeyboardLayout();layout.bindings={devpiano::core::makeNoteBinding('A',60,1),devpiano::core::makeNoteBinding('S',58,2)};
    mapper.setLayout(layout);mapper.setChannelMapper(&service);std::set<int> down{'A','S'};
    mapper.setKeyStatePredicate([&](int key){return down.contains(key);});juce::MidiKeyboardState state;
    mapper.handleKeyPressed(juce::KeyPress('A'),state);mapper.handleKeyPressed(juce::KeyPress('S'),state);
    down.erase('A');mapper.handleKeyStateChanged(state);require(state.isNoteOn(1,60),"matrix collapsed holder closed");
    mapper.releaseAllHeldKeys(state);require(!state.isNoteOn(1,60),"focus loss left merged holder on");
    const auto bad=scratch.getChildFile("bad-keyup.devpiano.preset");
    const auto valid=scratch.getChildFile("valid.devpiano.preset");
    auto preset=devpiano::layout::makeDefaultPreset();preset.name="Phase D Preset";
    require(devpiano::layout::savePreset(preset,valid),"valid preset save failed");
    auto json=juce::JSON::parse(valid);auto* bindings=json.getProperty("layout",{}).getProperty("bindings",{}).getArray();
    require(bindings && !bindings->isEmpty(),"preset has no bindings");
    (*bindings)[0].getProperty("action",{}).getDynamicObject()->setProperty("trigger","keyUp");
    require(bad.replaceWithText(juce::JSON::toString(json)),"invalid input write failed");const auto original=bad.loadFileAsString();
    require(!devpiano::layout::loadPreset(bad).has_value(),"keyUp accepted");require(bad.loadFileAsString()==original,"rejection changed input");
    require(devpiano::layout::loadPreset(valid).has_value(),"valid keyDown rejected");
    std::cout << "PHASE_D_INPUT QK_both_orders=1 matrix_merge=1 group_layout_snapshot=1 focus_release=1 keyup_rejected=1 input_retained=1
";
    return bad;
}
void verifyCapture(const juce::File& scratch) {
    {
        RecordingEngine first;first.reserveEvents(16);first.armRecording(48000.0);first.startArmedRecording();
        juce::MidiBuffer input;input.addEvent(juce::MidiMessage::noteOn(1,60,0.8f),63);input.addEvent(juce::MidiMessage::noteOn(1,64,0.8f),64);input.addEvent(juce::MidiMessage::noteOff(1,64),80);
        first.recordMidiBufferBlock(input,RecordingEventSource::realtimeMidiBuffer,0,64);first.advanceRecordingPosition(64);
        const auto take=first.stopRecording();require(take.events.size()==2 && take.lengthSamples==64,"first count-in block captured wrong range");
        for(const auto& item:take.events)require(item.message.getNoteNumber()==64 && item.timestampSamples==(item.message.isNoteOn()?0:16),"exact downbeat capture offset changed");
        std::cout << "PHASE_D_COUNTIN first_block_before_excluded=1 exact_downbeat_note_at_zero=1
";
    }
    RecordingEngine record;record.reserveEvents(64);record.startRecording(48000.0);
    record.recordEvent(juce::MidiMessage::noteOn(3,60,0.8f),RecordingEventSource::computerKeyboard,0);
    record.recordEvent(juce::MidiMessage::noteOn(3,60,0.9f),RecordingEventSource::computerKeyboard,8);
    record.recordEvent(juce::MidiMessage::controllerEvent(3,64,127),RecordingEventSource::computerKeyboard,16);
    record.advanceRecordingPosition(24000);record.prepareForAudioDevice(44100.0);
    record.pauseRecording();
    record.recordEvent(juce::MidiMessage::noteOff(3,60),RecordingEventSource::computerKeyboard,24000);
    record.recordEvent(juce::MidiMessage::noteOn(3,72,0.8f),RecordingEventSource::computerKeyboard,24000);
    record.advanceRecordingPosition(44100);record.resumeRecording();
    record.advanceRecordingPosition(44100);record.pauseRecording();record.prepareForAudioDevice(48000.0);record.resumeRecording();record.advanceRecordingPosition(48000);
    const auto take=record.stopRecording();int on=0,off=0,pedal=0;
    for(const auto& item:take.events) {
        on+=item.message.isNoteOn();off+=item.message.isNoteOff();
        if(item.message.isNoteOff())require(item.timestampSamples==24000 && item.message.getChannel()==3 && item.message.getNoteNumber()==60,"capture closed at wrong boundary");
        if(item.message.isNoteOn())require(item.message.getNoteNumber()!=72,"paused new note leaked");
        pedal+=item.message.isController() && item.message.getControllerNumber()==64 && item.message.getControllerValue()==0;
    }
    require(on==2 && off==2 && pedal==1 && take.lengthSamples==120000 && take.sampleRate==48000.0,"capture identity/rate mismatch");
    const auto file=scratch.getChildFile("capture.mid");require(devpiano::exporting::exportTakeAsMidiFile(take,file,960),"MIDI export failed");
    juce::FileInputStream input(file);juce::MidiFile midi;require(midi.readFrom(input,false),"raw MIDI readback failed");int midiOn=0,midiOff=0;
    for(int track=0;track<midi.getNumTracks();++track)for(int n=0;n<midi.getTrack(track)->getNumEvents();++n) {
        const auto& msg=midi.getTrack(track)->getEventPointer(n)->message;midiOn+=msg.isNoteOn();midiOff+=msg.isNoteOff();
    }
    require(midiOn==2 && midiOff==2,"MIDI lost capture pairing");
    std::cout << "PHASE_D_CAPTURE paired_on=" << on << " paired_off=" << off << " pedal_up=" << pedal << " fixed_rate=" << take.sampleRate << " length=" << take.lengthSamples << " midi_paired=1
";
}
void verifyEnd(const juce::File& scratch) {
    const auto take=takeWith(128,{event(0,juce::MidiMessage::noteOn(1,60,0.8f)),event(128,juce::MidiMessage::noteOff(1,60))});
    for(double rate:{48000.0,44100.0}) {
        RecordingEngine record;AudioEngine audio;audio.setRecordingEngine(&record);audio.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::sine);audio.setMasterGain(1.0f);audio.setAdsr(0.01f,0.2f,0.8f,0.3f);
        audio.prepareToPlay(64,rate);warmIdle(audio,64,rate);record.startPlayback(take,rate);
        juce::AudioBuffer<float> samples(2,256),block(2,64);samples.clear();
        for(int at=0;at<256;at+=64){runBlock(audio,block);for(int channel=0;channel<2;++channel)samples.copyFrom(channel,at,block,channel,0,64);}
        require(!audio.getKeyboardState().isNoteOn(1,60) && record.consumePlaybackEndedFlag(),"terminal live note waited for UI");
        devpiano::exporting::WavExportOptions options;options.sampleRate=rate;options.blockSize=64;options.builtinTone=SettingsModel::BuiltinTone::sine;options.adsr={0.01f,0.2f,0.8f,0.3f};options.masterGain=1.0f;
        const auto file=scratch.getChildFile("terminal-"+juce::String(static_cast<int>(rate))+".wav");
        require(devpiano::exporting::exportTakeAsWavFile(take,file,options),"terminal WAV export failed");
        juce::AudioFormatManager manager;manager.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(file));
        require(reader!=nullptr,"WAV reader failed");juce::AudioBuffer<float> decoded(2,256);require(reader->read(&decoded,0,256,0,true,true),"WAV samples unreadable");
        double delta=0;for(int channel=0;channel<2;++channel)for(int at=0;at<256;++at)delta=std::max(delta,std::abs(static_cast<double>(samples.getSample(channel,at)-decoded.getSample(channel,at))));
        require(delta<0.00005,"realtime/offline terminal samples differ");
        const auto timeline=prepareRenderTimeline(take,rate,0.0);require(timeline.has_value(),"terminal timeline rejected");
        std::cout << "PHASE_D_END rate=" << rate << " off_sample=" << timeline->events.back().timestampSamples << " event_inclusive_length=" << timeline->takeLengthSamples << " wav_delta=" << delta << " ui_poll=0
";
        audio.releaseResources();
    }
}
void verifySeek() {
    auto take=takeWith(512,{});
    for(int channel=1;channel<=16;++channel) {
        take.events.push_back(event(0,juce::MidiMessage::controllerEvent(channel,0,channel)));
        take.events.push_back(event(0,juce::MidiMessage::controllerEvent(channel,32,channel+1)));
        take.events.push_back(event(0,juce::MidiMessage::programChange(channel,channel+16)));
        take.events.push_back(event(0,juce::MidiMessage::controllerEvent(channel,64,channel%2==0?127:0)));
        take.events.push_back(event(0,juce::MidiMessage::pitchWheel(channel,8192+channel*100)));
        take.events.push_back(event(10,juce::MidiMessage::noteOn(channel,60,0.8f)));
        take.events.push_back(event(100,juce::MidiMessage::noteOn(channel,64,0.8f)));
        take.events.push_back(event(180,juce::MidiMessage::noteOff(channel,64)));
        take.events.push_back(event(200,juce::MidiMessage::programChange(channel,channel+32)));
        take.events.push_back(event(200,juce::MidiMessage::controllerEvent(channel,64,0)));
        take.events.push_back(event(200,juce::MidiMessage::pitchWheel(channel,4096)));
    }
    std::ranges::stable_sort(take.events,[](const auto& a,const auto& b){return a.timestampSamples<b.timestampSamples;});
    RecordingEngine record;record.setPlaybackLoopStartSample(100);record.setPlaybackLoopEndSample(300);
    record.startPlayback(take,48000.0);record.requestPlaybackSeek(100);
    juce::MidiBuffer stream;require(record.applyPendingTransportCommands(stream).seekApplied,"seek command not consumed");
    record.renderPlaybackBlock(stream,100,128);record.advancePlaybackPosition(128);
    juce::MidiBuffer next;record.renderPlaybackBlock(next,record.getPlaybackPositionSamples(),128);stream.addEvents(next,0,128,128);
    struct State {int bank=-1,lsb=-1,program=-1,pedal=-1,pitch=-1;};std::array<State,16> states;int targets=0;
    for(auto item:stream) {
        auto msg=item.getMessage();int ch=msg.getChannel();auto& state=states[static_cast<std::size_t>(ch-1)];
        if(msg.isController()) {
            const int cc=msg.getControllerNumber(),value=msg.getControllerValue();
            if(cc==0)state.bank=value;if(cc==32)state.lsb=value;if(cc==64)state.pedal=value;
        }else if(msg.isProgramChange())state.program=msg.getProgramChangeNumber();
        else if(msg.isPitchWheel())state.pitch=msg.getPitchWheelValue();
        else if(msg.isNoteOn()) {
            ++targets;require(msg.getNoteNumber()==64 && (item.samplePosition==0 || item.samplePosition==200),"historical NoteOn replayed");
            require(state.bank==ch && state.lsb==ch+1 && state.program==ch+16 && state.pedal==(ch%2==0?127:0) && state.pitch==8192+ch*100,"destination channel state not restored before note");
        }
    }
    require(targets==32,"seek or in-block loop lost destination notes");
    std::cout << "PHASE_D_SEEK channels=16 program_bank_pedal_pitch=1 before_destination=1 history_on=0 in_block_wrap=1
";
}

void verifySoft(const juce::File& scratch) {
    std::array<double,3> energies{};
    const std::array<int,3> values{0,80,127};
    for(std::size_t test=0;test<values.size();++test) {
        devpiano::audio::BuiltinSynthesiser synth;synth.setCurrentPlaybackSampleRate(48000.0);synth.addSound(new PianoSynthSound());
        for(int n=0;n<4;++n){auto* voice=new PianoSynthVoice();voice->setVoiceIndex(n);voice->setPedalNoiseLevel(0.0f);synth.addVoice(voice);}
        juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::controllerEvent(1,67,values[test]),0);
        for(int note:{60,64,67})midi.addEvent(juce::MidiMessage::noteOn(1,note,0.8f),8);
        juce::AudioBuffer<float> block(2,4096);block.clear();synth.renderNextBlock(block,midi,0,4096);
        for(int n=0;n<4096;++n)energies[test]+=static_cast<double>(block.getSample(0,n))*block.getSample(0,n);
        int inherited=0;for(int n=0;n<synth.getNumVoices();++n)if(auto* voice=dynamic_cast<PianoSynthVoice*>(synth.getVoice(n));voice && voice->isVoiceActive()){
            ++inherited;require(voice->isSoftPedalDown()==(values[test]>=64),"new chord voice did not inherit pedal");
            if(values[test]>=64)require(std::abs(voice->getSoftPedalAmount()-static_cast<float>(values[test])/127.0f)<0.00001f,"analog pedal lost");
        }
        require(inherited==3,"physical chord not allocated");
        synth.handleController(1,67,127);synth.noteOn(2,72,0.8f);synth.noteOn(2,74,0.8f);
        for(int n=0;n<synth.getNumVoices();++n)if(auto* voice=dynamic_cast<PianoSynthVoice*>(synth.getVoice(n));voice && voice->juce::SynthesiserVoice::isPlayingChannel(2))require(!voice->isSoftPedalDown(),"stolen voice inherited wrong channel");
        synth.handleController(1,67,0);for(int n=0;n<synth.getNumVoices();++n)if(auto* voice=dynamic_cast<PianoSynthVoice*>(synth.getVoice(n));voice && voice->juce::SynthesiserVoice::isPlayingChannel(1))require(!voice->isSoftPedalDown(),"pedal release left active voice soft");
        auto take=takeWith(4800,{event(0,juce::MidiMessage::controllerEvent(1,67,values[test])),event(8,juce::MidiMessage::noteOn(1,60,0.8f)),event(4800,juce::MidiMessage::noteOff(1,60))});
        devpiano::exporting::WavExportOptions options;options.sampleRate=48000.0;options.pedalNoiseLevel=0.0f;
        require(devpiano::exporting::exportTakeAsWavFile(take,scratch.getChildFile("soft-"+juce::String(values[test])+".wav"),options),"physical soft WAV failed");
    }
    require(energies[2]<energies[0] && energies[1]<energies[0],"inherited soft pedal did not change acoustic energy");
    std::cout << "PHASE_D_SOFT chord_inherit=1 analog=1 stolen_channel=1 release=1 acoustic_energy=" << energies[0] << ',' << energies[1] << ',' << energies[2] << " offline_writer=1
";
}
void verifyNative(const juce::File& scratch) {
    const juce::File plugin("G:/source/projects/devpiano/build-win-msvc/audit004-phase0/phased-smoke/native-build/PhaseDNative_artefacts/Debug/VST3/Phase D Native MIDI.vst3");
    PluginHost host;host.setDeadMansPedalFile(scratch.getChildFile("native-scan.txt"));const auto types=host.addVst3FileToKnownList(plugin);
    require(types.size()==1 && host.loadPluginByDescription(types[0],48000.0,128),"native VST3 load failed");
    RecordingEngine record;AudioEngine audio;audio.setRecordingEngine(&record);audio.setPluginHost(&host);audio.setMasterGain(1.0f);audio.prepareToPlay(128,48000.0);warmIdle(audio);
    juce::AudioBuffer<float> block(2,128);
    for(int channel=1;channel<=16;++channel) {
        record.clear();audio.requestAllNotesOff();auto take=takeWith(4800,{});
        for(int ch=1;ch<=16;++ch)take.events.push_back(event(0,juce::MidiMessage::controllerEvent(ch,67,ch==channel?80:0)));
        take.events.push_back(event(16,juce::MidiMessage::noteOn(channel,60,0.8f)));take.events.push_back(event(400,juce::MidiMessage::noteOff(channel,60)));
        audio.setPlaybackTranspose(true,2,0xFFFF);record.startPlayback(take,48000.0);runBlock(audio,block);
        const float expected=(0.02f+0.001f*channel+0.0001f*62.0f)*(1.0f-0.3f*80.0f/127.0f);
        require(std::abs(block.getSample(0,64)-expected)<0.00001f,"native channel CC67 or transpose changed");
        record.requestPlaybackStop();runBlock(audio,block);require(block.getMagnitude(0,128)<0.000001f,"native stop did not silence");
    }
    record.clear();audio.requestAllNotesOff();audio.setPlaybackTranspose(true,0,0xFFFF);
    record.startPlayback(takeWith(4096,{event(0,juce::MidiMessage::noteOn(1,60,0.8f)),event(128,juce::MidiMessage::noteOn(1,60,0.8f)),event(256,juce::MidiMessage::noteOff(1,60)),event(384,juce::MidiMessage::noteOff(1,60))}),48000.0);
    runBlock(audio,block);audio.setPlaybackTranspose(true,1,0xFFFF);runBlock(audio,block);require(audio.getKeyboardState().isNoteOn(1,60) && audio.getKeyboardState().isNoteOn(1,61),"native FIFO attacks not active");
    audio.setPlaybackTranspose(false,7,0);runBlock(audio,block);require(!audio.getKeyboardState().isNoteOn(1,60) && audio.getKeyboardState().isNoteOn(1,61),"FIFO old attack not released");
    runBlock(audio,block);require(!audio.getKeyboardState().isNoteOn(1,61) && block.getMagnitude(0,128)<0.000001f,"native final FIFO release missing");
    record.requestPlaybackStop();runBlock(audio,block);
    record.clear();audio.requestAllNotesOff();audio.setPlaybackTranspose(true,3,0xFFFF);
    record.startPlayback(takeWith(48000,{event(0,juce::MidiMessage::noteOn(1,60,0.8f)),event(24000,juce::MidiMessage::noteOff(1,60))}),48000.0);runBlock(audio,block);
    audio.releaseResources();audio.prepareToPlay(128,44100.0);audio.setPlaybackTranspose(false,0,0);
    while(record.getPlaybackPositionInTakeSamples()<25000)runBlock(audio,block);
    require(!audio.getKeyboardState().isNoteOn(1,63),"rate rebuild forgot original identity");
    require(block.getMagnitude(0,128)<0.000001f,"native rate-rebased Off missing");
    record.requestPlaybackStop();runBlock(audio,block);audio.releaseResources();host.unloadPlugin();
    std::cout << "PHASE_D_NATIVE vst3=1 original_cc_channels=16 offset_enabled_mask=1 FIFO=1 rate_off_identity=1 actual_audio_silenced=1
";
}
void verifyCountIn(MainComponent& main,const juce::File& scratch) {
    auto& settings=main.getAppSettings();
    for(const int bars:{1,2})for(const int blockSize:{128,512}) {
        RecordingEngine record;AudioEngine audio;audio.setRecordingEngine(&record);audio.setMasterGain(1.0f);audio.prepareToPlay(blockSize,48000.0);warmIdle(audio,blockSize);
        audio.setMetronomeBpm(120.0);audio.setMetronomeTimeSignature(devpiano::core::TimeSignature::fourFour);
        settings.metronomeCountIn=bars==1?devpiano::core::CountInBars::oneBar:devpiano::core::CountInBars::twoBars;
        RecordingSessionController controller(main,record,audio,settings);controller.handleRecordClicked();
        const auto rate=record.getSampleRate();audio.prepareToPlay(blockSize,rate);juce::AudioBuffer<float> buffer(2,blockSize);
        const auto expected=static_cast<std::int64_t>(rate*2.0*bars);std::int64_t elapsed=0,start=-1;
        while(elapsed<=expected+blockSize && start<0) {
            runBlock(audio,buffer);elapsed+=blockSize;
            if(record.isRecording())start=elapsed-record.getCurrentPositionSamples();
        }
        require(start==expected,"actual controller count-in started off downbeat");
        require(controller.getSession().state==devpiano::ui::RecordingState::idle,"UI secretly polled count-in");
        controller.handlePlayClicked();require(record.getState()==RecordingState::recordingPaused && controller.getSession().isRecording(),"zero-poll Play did not adopt/pause capture");
        controller.handlePlayClicked();require(record.isRecording(),"controller capture resume failed");
        controller.handleStopClicked();require(!record.isRecording() && controller.getSession().hasTake()==false,"empty controller take stop failed");closeDialogs();
        std::cout << "PHASE_D_COUNTIN bars=" << bars << " block=" << blockSize << " rate=" << rate << " exact_start=" << start << " ui_polls=0 pause_resume=1
";
    }
    {
        RecordingEngine record;AudioEngine audio;audio.setRecordingEngine(&record);audio.prepareToPlay(128,48000.0);warmIdle(audio);audio.setMetronomeBpm(120.0);audio.setMetronomeVolume(0.0f);
        record.reserveEvents(32);record.armRecording(48000.0);audio.getMetronomeProcessor().armCountIn(4);audio.setMetronomeEnabled(true);juce::AudioBuffer<float> block(2,128);
        for(int n=0;n<375;++n)runBlock(audio,block);
        audio.releaseResources();audio.prepareToPlay(128,44100.0);std::int64_t elapsed=0,start=-1;
        while(elapsed<45000 && start<0){runBlock(audio,block);elapsed+=128;if(record.isRecording())start=elapsed-static_cast<std::int64_t>(std::llround(record.getCurrentPositionSamples()*44100.0/48000.0));}
        require(start==44100,"real release/prepare changed count-in remaining second");require(block.getMagnitude(0,128)==0.0f,"silent metronome emitted audio");
        record.pauseRecording();record.stopRecording();audio.releaseResources();
        std::cout << "PHASE_D_COUNTIN rate_switch_halfway=1 silent=1 second_domain_start=" << start << "
";
    }
    for(int stopCycle=0;stopCycle<5;++stopCycle) {
        RecordingEngine record;AudioEngine audio;audio.setRecordingEngine(&record);audio.prepareToPlay(512,48000.0);warmIdle(audio,512);
        audio.setMetronomeBpm(120.0);audio.setMetronomeTimeSignature(devpiano::core::TimeSignature::fourFour);
        RecordingSessionController controller(main,record,audio,settings);settings.metronomeCountIn=devpiano::core::CountInBars::oneBar;controller.handleRecordClicked();
        juce::AudioBuffer<float> buffer(2,512);for(int n=0;n<200 && !record.isRecording();++n)runBlock(audio,buffer);
        require(record.isRecording() && controller.getSession().isIdle(),"zero-poll Stop precondition lost");
        record.recordEvent(juce::MidiMessage::noteOn(1,60,0.8f),RecordingEventSource::computerKeyboard,record.getCurrentPositionSamples());
        controller.handleStopClicked();require(record.getState()==RecordingState::stopped && controller.getSession().isIdle() && controller.getSession().hasTake(),"zero-poll Stop did not finalise capture");
        closeDialogs();std::cout << "PHASE_D_COUNTIN immediate_stop_without_poll=1 retained_take=1 cycle=" << stopCycle << "
";
    }
    {
        RecordingEngine record;AudioEngine audio;audio.setRecordingEngine(&record);audio.prepareToPlay(128,48000.0);warmIdle(audio);
        RecordingSessionController controller(main,record,audio,settings);settings.metronomeCountIn=devpiano::core::CountInBars::oneBar;controller.handleRecordClicked();
        require(controller.isCountingIn(),"controller did not arm");controller.handleStopClicked();
        require(record.getState()==RecordingState::idle && controller.getSession().isIdle(),"cancelled count-in started");
        controller.handleRecordClicked();juce::AudioBuffer<float> buffer(2,128);runBlock(audio,buffer);controller.handleStopClicked();
        require(!record.isRecording() && !controller.isCountingIn(),"cancel/rearm stale command started capture");
    }
    auto image=main.createComponentSnapshot(main.getLocalBounds());juce::PNGImageFormat png;
    auto stream=juce::File("G:/source/projects/devpiano/build-win-msvc/audit004-phase0/phased-smoke/phased-controller.png").createOutputStream();
    require(stream && png.writeImageToStream(image,*stream),"actual controller UI snapshot failed");
    std::cout << "PHASE_D_CONTROLLER cancel_rearm=1 actual_main_view=1
";static_cast<void>(scratch);
}
int main() {
    std::cout.setf(std::ios::unitbuf);
    SymInitialize(GetCurrentProcess(),"G:\source\projects\devpiano\build-win-msvc\audit004-phase0\phased-smoke",TRUE);
    SetUnhandledExceptionFilter(reportNativeFailure);
    try {
        juce::ScopedJuceInitialiser_GUI gui;OwnedDirectory owned;const auto profile=owned.directory.getChildFile("profile");require(profile.createDirectory().wasOk(),"private profile failed");
        ProfileScope redirect;require(redirect.redirect(profile),"private profile import redirect failed");
        require(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)==profile,"JUCE profile not private");
        const auto bad=verifyInput(owned.directory);verifyCapture(owned.directory);verifyEnd(owned.directory);verifySeek();verifySoft(owned.directory);verifyNative(owned.directory);
        SettingsModel settings;settings.languageCode="en";settings.masterGain=0.0f;{SettingsStore store;require(store.save(settings),"private startup write failed");}
        juce::JUCEApplicationBase::createInstance=[]()->juce::JUCEApplicationBase*{return new DevPianoApplication();};
        DevPianoApplication application;struct Shutdown{DevPianoApplication& app;~Shutdown(){closeDialogs();app.shutdown();}}shutdown{application};
        application.initialise("--sine");pump(100);auto& main=applicationMain();
        auto* selector=find<juce::ComboBox>(main,"preset-combo");require(selector!=nullptr,"actual preset selector missing");const auto previous=selector->getText();
        const auto bytes=bad.loadFileAsString();main.filesDropped({bad.getFullPathName()},0,0);pump(50);
        require(selector->getText()==previous && bad.loadFileAsString()==bytes,"invalid dropped preset replaced application identity");
        verifyCountIn(main,owned.directory);
        std::cout << "PHASE_D_APPLICATION preset_drop_rejected=1 prior_identity_retained=1 private_main_exit=1
";
        return 0;
    } catch(const std::exception& failure) {std::cerr << "PHASE_D_SMOKE_ERROR=" << failure.what() << '
';return 1;}
}
```

##### 修复前基线消费者（Phase C对象）

```cpp
#include <JuceHeader.h>
#include "Audio/AudioEngine.h"
#include "Input/KeyboardMidiMapper.h"
#include "Recording/RecordingEngine.h"
#include <iostream>
#include <set>
using namespace devpiano::recording;
RecordingTake takeWith(std::int64_t length, std::initializer_list<PerformanceEvent> events) {
    return { 48000.0, length, events };
}
PerformanceEvent event(std::int64_t sample, juce::MidiMessage message) {
    return { sample, PerformanceEventType::midi, 0, RecordingEventSource::playback, message };
}
int countOff(const juce::MidiBuffer& buffer) {
    int count = 0;
    for (const auto item : buffer) if (item.getMessage().isNoteOff()) ++count;
    return count;
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    {
        RecordingEngine engine;
        engine.reserveEvents(32);
        engine.startRecording(48000.0);
        engine.recordEvent(juce::MidiMessage::noteOn(1,60,0.8f), RecordingEventSource::realtimeMidiBuffer,0);
        engine.recordEvent(juce::MidiMessage::controllerEvent(1,64,127), RecordingEventSource::realtimeMidiBuffer,1);
        engine.advanceRecordingPosition(128);
        engine.pauseRecording();
        engine.recordEvent(juce::MidiMessage::noteOff(1,60), RecordingEventSource::realtimeMidiBuffer,128);
        engine.recordEvent(juce::MidiMessage::controllerEvent(1,64,0), RecordingEventSource::realtimeMidiBuffer,128);
        auto take=engine.stopRecording();
        int on=0,off=0,pedalUp=0;
        for(const auto& item:take.events) {
            on+=item.message.isNoteOn(); off+=item.message.isNoteOff();
            pedalUp+=item.message.isController() && item.message.getControllerNumber()==64 && item.message.getControllerValue()==0;
        }
        std::cout << "PHASE_D_BASE_CAPTURE on=" << on << " off=" << off << " pedal_up=" << pedalUp << " length=" << take.lengthSamples << "\n";
    }
    {
        RecordingEngine engine;
        engine.startPlayback(takeWith(128,{event(0,juce::MidiMessage::noteOn(1,60,0.8f)),event(128,juce::MidiMessage::noteOff(1,60))}),48000.0);
        juce::MidiBuffer midi;
        engine.renderPlaybackBlock(midi,0,128); engine.advancePlaybackPosition(128);
        const auto ended=engine.consumePlaybackEndedFlag();
        engine.renderPlaybackBlock(midi,128,128);
        std::cout << "PHASE_D_BASE_END ended=" << ended << " off=" << countOff(midi) << "\n";
    }
    {
        RecordingEngine engine;
        engine.startPlayback(takeWith(512,{event(0,juce::MidiMessage::controllerEvent(2,0,3)),event(0,juce::MidiMessage::programChange(2,8)),event(0,juce::MidiMessage::controllerEvent(2,64,127)),event(0,juce::MidiMessage::pitchWheel(2,10000)),event(32,juce::MidiMessage::noteOn(2,60,0.8f)),event(64,juce::MidiMessage::noteOn(2,64,0.8f)),event(128,juce::MidiMessage::noteOff(2,64))}),48000.0);
        engine.requestPlaybackSeek(64); juce::MidiBuffer midi;
        static_cast<void>(engine.applyPendingTransportCommands(midi));
        engine.renderPlaybackBlock(midi,64,64);
        int bank=0,program=0,pedal=0,pitch=0,oldOn=0;
        for (auto item:midi) {
            auto msg=item.getMessage();
            bank+=msg.isController() && msg.getControllerNumber()==0 && msg.getControllerValue()==3;
            program+=msg.isProgramChange() && msg.getProgramChangeNumber()==8;
            pedal+=msg.isController() && msg.getControllerNumber()==64 && msg.getControllerValue()==127;
            pitch+=msg.isPitchWheel();
            oldOn+=msg.isNoteOn() && msg.getNoteNumber()==60;
        }
        std::cout << "PHASE_D_BASE_SEEK bank=" << bank << " program=" << program << " pedal=" << pedal << " pitch=" << pitch << " history_on=" << oldOn << "\n";
    }
    {
        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState keyboard;
        std::set<int> keys{'Q','K'};
        mapper.setKeyStatePredicate([&](int key){return keys.contains(key);});
        mapper.handleKeyPressed(juce::KeyPress('Q',0,'Q'),keyboard);
        mapper.handleKeyPressed(juce::KeyPress('K',0,'K'),keyboard);
        keys.erase('Q'); mapper.handleKeyStateChanged(keyboard);
        std::cout << "PHASE_D_BASE_HOLD remaining_key=" << mapper.isKeyHeld('K') << " sounding=" << keyboard.isNoteOn(1,72) << "\n";
        mapper.releaseAllHeldKeys(keyboard);
    }
    {
        RecordingEngine engine; AudioEngine audio; audio.setRecordingEngine(&engine);
        juce::AudioBuffer<float> output(2,64); juce::AudioSourceChannelInfo info(&output,0,64);
        audio.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::sine); audio.prepareToPlay(64,48000.0);
        for(int n=0;n<AudioEngine::calculateWarmupBlockCount(48000.0,64);++n)audio.getNextAudioBlock(info);
        engine.startPlayback(takeWith(512,{event(0,juce::MidiMessage::noteOn(1,60,0.8f)),event(128,juce::MidiMessage::noteOff(1,60))}),48000.0);
        audio.getNextAudioBlock(info); audio.setPlaybackTranspose(true,1);
        audio.getNextAudioBlock(info); audio.getNextAudioBlock(info);
        std::cout << "PHASE_D_BASE_IDENTITY original_still_on=" << audio.getKeyboardState().isNoteOn(1,60) << "\n";
        audio.releaseResources();
    }
    {
        RecordingEngine engine; AudioEngine audio; audio.setRecordingEngine(&engine);
        engine.startPlaybackAtTakeSample(takeWith(48000,{event(0,juce::MidiMessage::noteOn(1,60,0.8f)),event(48000,juce::MidiMessage::noteOff(1,60))}),48000.0,24000);
        audio.prepareToPlay(128,44100.0);
        std::cout << "PHASE_D_BASE_RATE playback_device_sample=" << engine.getPlaybackPositionSamples() << " take_sample=" << engine.getPlaybackPositionInTakeSamples() << "\n";
        audio.releaseResources();
        engine.reserveEvents(32);engine.startRecording(48000.0);audio.prepareToPlay(128,44100.0);engine.advanceRecordingPosition(44100);
        std::cout << "PHASE_D_BASE_RATE capture_take_sample=" << engine.getCurrentPositionSamples() << "\n";
        engine.stopRecording();audio.releaseResources();
    }
}
```

##### 最终关键输出（单次观察，不是固定门槛）

```text
PHASE_D_INPUT QK_both_orders=1 matrix_merge=1 group_layout_snapshot=1 focus_release=1 keyup_rejected=1 input_retained=1
PHASE_D_COUNTIN first_block_before_excluded=1 exact_downbeat_note_at_zero=1
PHASE_D_CAPTURE paired_on=2 paired_off=2 pedal_up=1 fixed_rate=48000 length=120000 midi_paired=1
PHASE_D_END rate=48000 off_sample=128 event_inclusive_length=129 wav_delta=3.04207e-05 ui_poll=0
PHASE_D_END rate=44100 off_sample=118 event_inclusive_length=119 wav_delta=3.02829e-05 ui_poll=0
PHASE_D_SEEK channels=16 program_bank_pedal_pitch=1 before_destination=1 history_on=0 in_block_wrap=1
PHASE_D_SOFT chord_inherit=1 analog=1 stolen_channel=1 release=1 acoustic_energy=5.94625,4.03106,3.05399 offline_writer=1
PHASE_D_NATIVE vst3=1 original_cc_channels=16 offset_enabled_mask=1 FIFO=1 rate_off_identity=1 actual_audio_silenced=1
PHASE_D_COUNTIN bars=1 block=128 rate=48000 exact_start=96000 ui_polls=0 pause_resume=1
PHASE_D_COUNTIN bars=1 block=512 rate=48000 exact_start=96000 ui_polls=0 pause_resume=1
PHASE_D_COUNTIN bars=2 block=128 rate=48000 exact_start=192000 ui_polls=0 pause_resume=1
PHASE_D_COUNTIN bars=2 block=512 rate=48000 exact_start=192000 ui_polls=0 pause_resume=1
PHASE_D_COUNTIN rate_switch_halfway=1 silent=1 second_domain_start=44100
PHASE_D_COUNTIN immediate_stop_without_poll=1 retained_take=1 cycle=0
PHASE_D_COUNTIN immediate_stop_without_poll=1 retained_take=1 cycle=1
PHASE_D_COUNTIN immediate_stop_without_poll=1 retained_take=1 cycle=2
PHASE_D_COUNTIN immediate_stop_without_poll=1 retained_take=1 cycle=3
PHASE_D_COUNTIN immediate_stop_without_poll=1 retained_take=1 cycle=4
PHASE_D_CONTROLLER cancel_rearm=1 actual_main_view=1
PHASE_D_APPLICATION preset_drop_rejected=1 prior_identity_retained=1 private_main_exit=1
PHASE_D_PROFILE_RESTORED=1
PHASE_D_PRIVATE_FILES_CLEAN=1
```


### AUDIT-004 Phase E：预设永久身份与实时/离线执行闭包 [待开始]

**目标**：稳定预设身份与可执行快照同构消费，实时交换有界、无锁、无分配，完整回调 SLA 可观测。

**依赖**：Phase A/C/D；先 ARCH-003，再 ARCH-004；发布/通知容量与监听器清理同步设计。

先稳定预设身份再携准备快照执行；回调/显示/预设通知按预分配有界交换，验证整个执行闭包。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [ ] | `ARCH-003` | P2 | 录制预设事件用可变目录索引作为永久身份。保存稳定预设身份/Take内映射或快照并定义缺失行为；迁移格式时不得静默重解释旧数字。 | 保存演奏后增/删/重命名预设不重定向旧事件；旧数字格式迁移和缺失预设策略显式。 |
| [ ] | `ARCH-004` | P2 | 预设事件丢失可执行时序和离线语义。保留事件variant与准备好的声学快照，按采样偏移执行实时/离线同构语义，UI通知独立且不丢末块。 | 同块 preset→note 用新快照，末块通知不丢；实时与两条离线路径按记录边界执行同一声学变化。 |
| [ ] | `THR-001` | P1 | 实时回调常规路径仍有阻塞锁。把演奏事件、显示快照和预设通知收敛到预分配无锁通道；避免 UI 与音频共享可阻塞状态锁。 | 完整回调调用闭包不含 UI 共享阻塞锁；消息线程持有可视/参数工作时音频不等它释放。 |
| [ ] | `PERF-001` | P1 | 密集播放和重复预设循环突破回调预分配。确定每块容量与有界溢出策略、复用预分配通知存储；覆盖合法密集事件及消息线程尚未drain的重复循环。 | 准备后密集合法事件及未 drain 的重复预设循环不发生堆增长；溢出策略有界、可观察且不丢必需释放。 |
| [ ] | `AUDIT-002 THR-003` | P1 | 音频线程 MIDI Listener 同步进入 UI/Timer。实时Listener仅有界快照/通知，消息线程处理UI和Timer；覆盖电脑/鼠标/回放/失焦。 | 电脑、鼠标、文件回放的实时 Listener 仅有界通知/快照；UI/Timer 从消息线程更新，Debug 无线程断言。 |
| [ ] | `known-issues ERR-002` | P1 | 异常插件缓冲尺寸仍保留重分配兜底。先明确定义设备/插件异常几何的安全处理并保持观测计数；对目标声卡热插拔验证，不把正常块plugin_resize=0当异常已修。 | 超协商通道/块长的故障策略不越界、不在回调重分配，并保留计数与消息线程诊断；实机异常尺寸单独验证。 |
| [ ] | `known-issues §1/节拍器每拍三角函数与全回调零三角 SLA 不一致` | P2 | 全回调零三角函数 SLA 尚未达到。按完整调用闭包界定/验证SLA；优先预计算或递归机械振荡，保持听感及踏板语义；CPU期限效果另测。 | 覆盖机械起音/释放/踏板及节拍器完整回调闭包，实际零实时 sin 等目标；不能只看分音循环或旧 CPU 测量。 |

### AUDIT-004 Phase F：映射看板、交互与声学边界 [待开始]

**目标**：两个视图投影最终映射，鼠标输入不被输出反馈污染，UI 状态/输入及调律边界一致。

**依赖**：Phase A/D/E；明确点击输入身份与显示输出身份，不以重复矩阵变换修显示。

双看板、鼠标输入、绑定标签与元数据编辑直接消费正确模型；调律范围与所有入口一致。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [ ] | `ARCH-001` | P2 | 两张演奏映射看板未共同消费最终映射投影。映射层输出两个视图共享的最终只读投影；同时明确点击输入身份，避免展示修复后再次矩阵变换。 | Group/modifier/矩阵/followKey 改动时两张看板与实际输出身份一致；点击不二次变换已显示的输出。 |
| [ ] | `QUAL-007` | P2 | 鼠标输入通道被观察到的输出通道反向污染。分开配置输入身份与显示用输出通道；鼠标始终从当前映射输入身份触发并保存最终输出。 | Ch1→Ch2、Ch2→Ch3 下同键连续鼠标点击始终按配置输入路由；回放不改随后鼠标通道。 |
| [ ] | `QUAL-008` | P2 | 几何重建清空钢琴绑定标签。几何重建保留或重新消费已有映射视图标签，不把标签仅存于一次临时KeyRenderState赋值。 | setLayout→setSettings、resize、viewport 更新后逐键绑定标签保留，几何变化不清映射提示。 |
| [ ] | `QUAL-009` | P2 | 静音绑定在 Shift/QWERTY 鼠标入口变为满力度。复用同一静音优先级规则生成快照和点击事件；验证最终MIDI而不仅held.velocity。 | 零力度绑定在物理/鼠标/QWERTY＋Shift 路径最终无可听 NoteOn；验证实际 MIDI 而非仅 held 值。 |
| [ ] | `QUAL-010` | P2 | fadeSpeed=1 合法端点不衰减且计时器不停止。统一UI/加载器的收缩系数范围，或为端点定义显式可终止动画；验证停止和有界alpha。 | UI 与导入端点的 fade 始终有界且收缩，释放后到目标并停 Timer；1及大于1输入策略明确。 |
| [ ] | `QUAL-012` | P2 | 实时圆角样式路径落后一版。先更新radii再重建路径；在不改变bounds情况下连续改两次半径，验证真实角像素/路径。 | 固定 bounds 连续 radius0→30→0，真实角像素立即与当前值相符，无一版滞后。 |
| [ ] | `QUAL-013` | P2 | 歌曲信息 Notes 继承只读 ListEditor。仅元数据Notes使用可编辑工厂/显式恢复输入能力，保留诊断列表只读；测试走生产ViewHost并注入用户键入。 | 生产 ViewHost 的 Notes 可键入/多行/保存，取消不改元数据；诊断 ListEditor 保持只读。 |
| [ ] | `QUAL-011` | P3 | MIDI 1至11的八度标注高一组。使用等价数学floor的MIDI八度换算，覆盖0/1/11/12边界和唱名偏移。 | MIDI0/1/11/12 标签为同一正确八度边界，唱名/单音 HUD 相符。 |
| [ ] | `known-issues §1/A4 基准音高范围与项目契约不一致` | P2 | A4实际410..450Hz未覆盖400..480Hz契约。以原已知项统一修正调律引擎、设置、预设与导出边界并测试两端；此前文档标注保持真实。 | 400..480 Hz 在引擎/设置/预设/导出同限幅，测试400/480端点和正常415/440/442参考。 |

### AUDIT-004 Phase G：诊断资源、ADR 与工程门禁收敛 [待开始]

**目标**：诊断数值及资源预算真实，细粒度 include/门面合规，测试与静态门禁不提供假覆盖。

**依赖**：贯穿实施；Phase A-F 的消费者回归已有证据后收口，不用压制诊断掩盖问题。

诊断内容/文件预算真实，业务 include 与门面遵守现行 ADR，测试 oracle 和编译/静态诊断收口。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [ ] | `RES-001` | P2 | 日志大小上限仅构造时截减而非会话滚动。实现可观测的会话内有界轮转，或明确真实只在启动裁剪的契约与风险；验证长会话及轮转故障。 | 长会话日志按真实预算轮转，失败有诊断；上限作用于会话内写入而不只是启动裁剪。 |
| [ ] | `OBS-001` | P2 | MIDI诊断将已是0..127的力度再次乘127。直接展示原始整数力度或正确使用getFloatVelocity换算；验证边界及中间值，不钉完整自然语言日志。 | MIDI 原始64力度诊断仍为64，0..127边界和中间值一致，不钉整个文案。 |
| [ ] | `CMPL-001` | P2 | 业务头 WindowIconUtils 重新引入 JuceHeader。以实际需要的细粒度模块头替换并检查消费者；不使用测试头例外为业务头开豁免。 | WindowIconUtils 使用实际细粒度头，消费者独立编译；source 业务头不再传递 JuceHeader。 |
| [ ] | `CMPL-002` | P2 | 声明式业务仍使用 raw GuiItem 逃逸接口。将业务样式刷新/内置modal初始化封装在ViewHost边界内；如确需例外则另行明确批准决策，而非保留无约束逃逸。 | 业务样式刷新及内置 modal 通过明确门面，禁止改 ADR 掩盖违例；必要例外先独立决策评审。 |
| [ ] | `ENG-001` | P2 | 编译零警告及全量 tidy 清零门禁不成立。逐项评估编译/静态诊断并小步修正，必要规则争议如实记录；禁止--fix自动改源码或压制未知风险。 | 项目编译警告与实际全量 tidy 诊断清零；唯一位点与框架输出分开计，默认缓存原失败/替代结果保留。 |
| [ ] | `TEST-003` | P3 | 测试存在译文/自证断言及未调用行为用例。删除copy-pinning/自证测试，不重新钉新文本/数值；保留语言机制和生产组件行为；接入确定性的真实生命周期用例。 | 删除译文/自证 oracle，生产语言机制保留；真正 lifecycle 方法被默认执行，不改成新的文案钉死。 |

### AUDIT-004 Phase H：契约文档与最终集成验收 [待开始]

**目标**：现行功能/验收文档与真实实现同步，全部原登记项有直接闭环证据；再评估后续功能阶段。

**依赖**：Phase 0及A-G；文档修订不得代替实现修复。

以修复后的真实消费者证据更新功能/验收说明，汇总全部原项，不用文档纠错冒充代码修复。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [ ] | `DOC-001` | P3 | 现行行为说明含已被源码证伪的承诺。修复实现后按真实契约同步功能/手工验收；事实描述不另开CMPL；本轮不改任何既有文档。 | 预设调号/rename确认、MIDI轨与meta、日志轮转及插件/测试行为按已验证实现说明，不把计划写已完成。 |

## 5. 每阶段门禁与最终闭环

- 默认使用 [quickstart 的 Windows Debug 流程](../guides/quickstart.md#windows-debug-单元测试)：WSL仅编辑/configure和静态检查，Windows镜像构建/软件验证，格式检查只检查不修文件；完整WSL验证或Release仅明确要求时执行。
- 原审计记录的默认Windows缓存路径失败、项目编译warning、全量tidy失败及当时Chord漏跑都保留为真实历史输入。Phase 0/A 已用安全子树完成本阶段消费者与默认执行；不把子树通过写成原默认构建命令通过，不将未触及的 ENG 或其他 TEST 风险标为已修。
- 每阶段先验证实际修改消费者，再观察默认门禁的真实执行集合；在迭代边界由集成负责人统一全量 `./scripts/dev.sh tidy --all`，禁止自动 `--fix` 和每个切片重复门禁。编译/静态诊断、测试覆盖与命令退出码分开记录。
- 使用可丢弃profile/复制文件/隔离输出。无法安全执行的OOM、强杀、真实VST3或声卡实验说明受限，不能用真实用户文件/日志冒险或无证据降级风险。
- 按新审计模板保留证据ID、基线、最小输入/代码、精确构建运行配方、预期/实际及未验证范围；清理临时探针前保存可复建内容。不把源文本/mock echo/默认参数往返视作消费者证明。
- 关闭标准：原触发条件直接通过，或有可复核调用链证明路径已消除；默认测试绿灯/文档润色不能关闭数据完整性、并发或发音缺陷。完整ID和原证据保留，历史重开、已知引用不变成新增发现。

### 最终实施验收（尚未整体达标）

- [ ] Phase 0及A-H全部原项有对应修复/验证记录；原始54项与任务ID经去重及 `comm` 对照零缺失/零多余，原优先级不变。
- [ ] Windows Debug构建/默认单测/格式及全量静态检查记录实际结果；默认选择含漏跑套件，用户数据无副作用，fixture不依赖可选优化。
- [ ] 原文件保护、文件准入、NoteOff配对、同步/防抖顺序及完整实时/离线执行闭包有消费者反例修复后的直接证据。
- [ ] 真实插件Editor/重扫/同名/再次拖放/offline模式、慢插件取消及声卡密集/热插拔组合按原报告4.5完成安全手工复核；未验证的项不假填已通过。
- [ ] 修复后的功能/验收、known-issues和roadmap相互一致；不回写历史Phase35勾选或AUDIT-004基线结论，完成后再归档本实施记录并建立新复审入口。

## 6. 后续路线与历史入口

- Phase 36/37的范围与顺序只在 [roadmap](roadmap.md)维护，本计划不再复制未来功能草案。
- [Phase 35完成计划归档](../archive/phase35-keyboard-expressive-dynamics-and-practice-infrastructure.md)、[Phase 34完成记录](../archive/phase34-keyboard-performance-ux-and-expressive-control.md)、[AUDIT-003历史修复记录](../archive/audit-003-code-quality-fix-phases.md)。
- [审计报告入口](../audit/README.md)、[当前已知风险](../issues/known-issues.md)、[阶段验收标准](../reference/acceptance.md)。

# devpiano Current Iteration

> 用途：本文件只记录当前实施排期与任务验收；项目状态和长期路线以 [roadmap](roadmap.md) 为准。
> 本轮名称：**AUDIT-004 Phase：代码质量缺陷修复与消费者契约闭环**。
> 当前状态：**Phase 0 与 Phase A 已完成（2026-10-02），Phase B 待开始**。只勾选有直接证据的本轮任务；其他阶段保持未勾选，不以默认测试通过代替其消费者契约闭环。

## 1. 输入、范围与历史归档

- 问题与优先级基线：[AUDIT-004 第8章](../audit/AUDIT-004-code-quality-audit-2026-10-02.md#8-附录问题总表登记表)，实施方向参考其第5章，复现与未验证范围参考第4章。
- 本计划完整纳入原审计基线的 **54个未闭环唯一项**（P1 25、P2 26、P3 3，本轮未登记P0）。这是固定排期覆盖集合，不是当前剩余任务数；Phase 0 与 Phase A 的任务已完成，其他原项仍按下方未勾选任务推进，原报告不回写。
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
| AUDIT-004 Phase B | 文件准入与时间线数值安全 | Phase A 的所有权和失败保留约束；数值检查应先于打开输出。 | 待开始 |
| AUDIT-004 Phase C | 插件与活动 DSP / Transport 所有权 | Phase 0/A；冻结已有数据与旧实例生命周期，先收敛竞态再改事件执行。 | 待开始 |
| AUDIT-004 Phase D | 发音身份与采样级 Transport 边界 | Phase B/C；游标/倍率/设备时间域的所有权先于新增边界逻辑。 | 待开始 |
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

### AUDIT-004 Phase B：文件准入与时间线数值安全 [待开始]

**目标**：畸形/部分文件在准入失败，不放大分配、不破坏当前 Take，不让无效时间线进入渲染。

**依赖**：Phase A 的所有权和失败保留约束；数值检查应先于打开输出。

原生/MIDI 输入在可表示与资源预算内进入消费者，拒绝失败不替换旧会话；保留完整文件后缀兼容。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [ ] | `SEC-003` | P1 | 原生 MIDI 编码信任未校验的解码长度前缀。解码前验证 JUCE 特有编码的长度、数据预算和负载一致性；校验解码后的 MIDI 帧形状并明确传播读取失败。 | 负/超额/不一致长度前缀及截断 MIDI 帧在分配/构造前拒绝，原 Take 保留；独立受限进程验证异常传播。 |
| [ ] | `SEC-004` | P1 | 原生采样率/长度缺少可表示范围验证。准入时检查有限、支持范围的采样率、非负且一致的长度/时间戳；消费端使用检查后的比例和整数转换。 | 极小正率、非有限值、负/不一致长度或不可表示缩放被拒绝，正常录制和回放时长不变。 |
| [ ] | `SEC-005` | P1 | 饱和时间戳后 +1/尾部采样加法仍溢出。在打开输出前验证最终事件与尾部长度均可表示；检查加法，不将转换饱和视为整个时间线已安全。 | INT64_MAX 最后事件、末尾 +1 和尾部相加均检查可表示性；不出现派生长度 1 或带损坏长度的输出。 |
| [ ] | `SEC-006` | P1 | MIDI 拍号元数据未验证负载及位移指数。读取前验证固定宽度 meta 长度及拍号分母指数；畸形值拒绝/显式忽略并报告，不直接进入框架 accessor。 | 非法 0x58 长度/分母指数不进入未定义位移；合法拍号仍提取正确，异常 meta 策略有诊断。 |
| [ ] | `QUAL-005` | P1 | 原生加载保留乱序事件但播放器假定有序。原生文件准入拒绝或稳定规范化非单调时间线；保留同采样语义顺序并测试真正播放/seek。 | 原生乱序事件明确拒绝或稳定规范化；真正回放和 seek 不静默漏掉早期 NoteOn。 |
| [ ] | `ERR-004` | P2 | MIDI宽容尾字节也误接收缺失/截断轨。仅完整声明结构后额外后缀允许宽容，缺失/短chunk应拒绝且保留现有Take；保留真实CRLF后缀兼容。 | 缺第二声明轨或短 chunk 导入失败且旧 Take 不变；仅完整结构后的真实 CRLF 后缀可宽容。 |

### AUDIT-004 Phase C：插件与活动 DSP / Transport 所有权 [待开始]

**目标**：所有实例/声部/活动游标变更有明确停机或音频所有者边界，异步导出协作收尾。

**依赖**：Phase 0/A；冻结已有数据与旧实例生命周期，先收敛竞态再改事件执行。

先关 Editor/停 callback 或发布音频所有者命令，再修改实例、voice 或游标；独立离线实例声明正确模式。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [ ] | `AUDIT-001 THR-004` | P1 | 增量重扫绕过停音频/关 Editor 守卫。复用设备重建守卫，在扫描卸载前关闭 Editor 并停止 callback；覆盖已加载+Editor 打开时重扫。 | 已加载＋Editor＋重扫时，先关闭 Editor 并停止 callback 再卸载；load/unload/重扫/退出均无悬垂实例。 |
| [ ] | `AUDIT-002 THR-001` | P1 | 音色重建仍从消息线程应用活动 DSP 参数。将重建与参数提交放入明确停音频窗口，或仅由音频所有者完成受控切换；不能只依赖单次 getVoice/clear/add 的内部锁。 | 持续渲染中切 Piano/Sine、启动/再次启动参数提交不并发写活跃 voice/roomReverb；提供真实交错证据。 |
| [ ] | `known-issues §2/Phase 6-2 播放速度控制` | P1 | 活动变速/Stop 在消息线程改写音频游标。发布 transport 命令，在音频块边界一致应用倍率、位置和游标；结构性停止复用停机守卫；补真实双线程回归。 | 播放中变速/Stop 的倍率、位置、游标在同一音频边界生效；双线程交错不跳过 NoteOff/破坏循环。 |
| [ ] | `THR-002` | P1 | 取消/析构 WAV 任务可能强制终止工作线程。仅协作取消，异步等待实际工作线程退出后再释放任务/插件/文件所有权；验证慢 processBlock 的取消和退出。 | 慢插件/输出操作超过旧超时后取消仍等待真实工作退出；无 TerminateThread、句柄泄漏或未完成文件头。 |
| [ ] | `QUAL-014` | P2 | 离线插件实例未声明 nonRealtime 模式。prepare前setNonRealtime(true)，保证setup和process一致；选择依赖offline模式的真实VST3对照验证。 | 独立 VST3 在 prepare 前即获 offline mode，setup/process 一致；真实依赖 offline 分支的插件对照。 |
| [ ] | `QUAL-015` | P2 | 再次拖入已经发现的 VST3 被误判为没有类型。分离探测到的有效类型与是否新增列表条目，重复文件也返回可加载身份并保留metadata更新。 | 扫描/缓存已存在的插件在卸载后再次拖入可加载，metadata 更新与是否新增列表分离。 |
| [ ] | `ARCH-002` | P2 | 插件选择/恢复以显示名代替 description 身份。贯穿选择/加载/持久化稳定description身份，显示名仅展示；验证同名不同ID及乐器/效果过滤。 | 同名不同文件/ID/类型插件均可选、正确恢复，乐器过滤不加载同名效果。 |

### AUDIT-004 Phase D：发音身份与采样级 Transport 边界 [待开始]

**目标**：重叠同音、移调、pause、末尾、seek/loop、count-in、设备重建和踏板均有确定性语义。

**依赖**：Phase B/C；游标/倍率/设备时间域的所有权先于新增边界逻辑。

NoteOff 永远对应原发音；捕获/播放/跳转/节拍/设备采样域边界有采样级可观察验收。

| 执行 | 原登记 ID | 原优先级 | 修复目标 | 可观察验收 |
| --- | --- | --- | --- | --- |
| [ ] | `QUAL-001` | P1 | 播放移调/掩码变化不锁定已发音身份。播放侧也保存 NoteOn 最终输出身份，NoteOff按原身份；定义重叠同音及变化时正在发声的处理，不用当前映射重算。 | On 与 Off 之间改 offset/enabled/mask，Off 仍释放原输出身份；不依赖稍后 panic。 |
| [ ] | `QUAL-002` | P1 | 重复物理键同音在首个松键时被提前关闭。在最终发音身份层维护重叠持有者，最后释放再NoteOff；保留每个物理键原身份。 | 默认 Q/K 同音及矩阵合并音高交错松键，剩余持有者仍响，最后释放才关闭。 |
| [ ] | `QUAL-003` | P1 | 暂停录制丢掉期间唯一的 NoteOff/踏板释放。在冻结捕获时间轴的边界补齐已录身份/踏板终结状态；暂停期间排除新演奏，但保证保留Take配对。 | 暂停捕获期间释放先前已录音符/踏板，保留 Take 和 MIDI 导出仍配对；暂停中新演奏不混入。 |
| [ ] | `QUAL-004` | P1 | 精确播放末尾的 NoteOff 未在音频路径交付。播放长度包含最后事件或在音频边界明确终结；对齐实时/离线可听结束和最终NoteOff采样点。 | 最后 Off 精确等于 Take 长度/块末时仍在音频路径交付，结束不等待 UI timer 才清音。 |
| [ ] | `ERR-003` | P1 | 接受并序列化的 keyUp 绑定没有执行入口。兑现公开keyUp触发的可配对事件语义或在准入明确拒绝；不能接受文件配置后静默丢弃。 | 接受的 keyUp 绑定有完整触发/配对语义；若产品边界不支持，则准入显式拒绝，不接受后无声。 |
| [ ] | `QUAL-018` | P1 | 设备采样率变更未重基准活动播放/录制时间域。设备prepare时统一重基准活动Transport；录制按固定Take域换算，或明确先结束会话；验证位置/倍速/NoteOff连续性。 | 活动录制/回放切 48k↔44.1k、暂停/恢复及倍速仍保持 Take-relative 时长、位置和发音身份。 |
| [ ] | `QUAL-017` | P2 | Seek/循环回跳未恢复目的位置控制器及音色状态。在目标音符前恢复目的位置的状态快照，保持不自动重发历史NoteOn的现有策略；测试program/bank/CC64及pitch。 | seek/回跳在目标音符前恢复 program/bank/CC64/pitch 状态，不意外重发历史 NoteOn；16通道独立验证。 |
| [ ] | `FIX-035` | P2 | 预备拍在最后一拍起音而非下一下拍完成。以完整音频节拍时段/下一个目标downbeat完成并消费序号差；实际控制器验证一/两小节及跨多拍poll。 | 120 BPM、4/4 一小节在完整2秒后的目标 downbeat 开始；多拍序号跳变、取消/重建不改变预备拍时长。 |
| [ ] | `QUAL-019` | P2 | 踩下柔音踏板后新分配声部不继承CC67状态。由乐器拥有者维护当前踏板状态，保证每个新起声部继承；保留VST3通道语义并验证踏板先于和弦、换声部及释放。 | 先CC67再和弦、重分配/偷声部和释放，所有当前/新起物理声部继承正确柔音状态，不破坏VST3通道。 |

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

# devpiano 代码质量审计报告 · 2026-10-02

> 只读审计：仅新增本报告。第8章是唯一状态源，结论区分实际Windows观测、静态证据与未验证影响；修复另开迭代。

---

## 0. 审计看板

### 0.1 基本信息

| 字段 | 值 |
| --- | --- |
| 项目 / 版本 | devpiano / CMake项目版本1.3.0 |
| 审计范围 | source/ 全部289个.cpp/.h，含内化UI运行时110文件及tests/52文件 |
| 审计日期 | 2026-10-02 |
| 审计基线 | main @ `732cde18dc8116cb21f0325377415cedfef03811`，开始时工作树干净 |
| 审计人 | devpiano-audit，主审与6个独立只读切片，主审统一复核 |
| 复审状态 | 初次；继AUDIT-003后覆盖当前基线，不改写历史报告 |
| 写入范围 | 仅本报告；源码、ADR、既有文档和配置未改；构建缓存及自有临时探针不属业务实现 |

### 0.2 风险与状态汇总

| 优先级 | 合计 | 未处理 | 处理中 | 已缓解 | 已暂缓 | 已关闭 |
| --- | --- | --- | --- | --- | --- | --- |
| P0 | 0 | 0 | 0 | 0 | 0 | 0 |
| P1 | 25 | 25 | 0 | 0 | 0 | 0 |
| P2 | 26 | 26 | 0 | 0 | 0 | 0 |
| P3 | 3 | 3 | 0 | 0 | 0 | 0 |
| **合计** | 54 | 54 | 0 | 0 | 0 | 0 |

共**54项未处理**：**45项本轮新发现，9项既有问题引用或新反证重开**。计数包含第8章全部行，历史/known-issues不重新编号。状态、首页、结论及路线图均由第8章派生。

### 0.3 关键结论

- 总体评级：**C**。固定拓扑、正常所有权及已有行为测试可复用，但存在直接数据完整性、实时契约和并发边界缺陷。
- 当前是否适合继续新增功能：**否**。先处理P1，默认测试绿灯不能代替消费者路径证据。
- 当前是否建议优先重构：**有条件**。只重构输出事务、活动Transport/voice所有权与预设事件边界，不做全局架构重写。
- 最大风险：覆盖/取消、预设重命名、旧Take绑定可误写或删除已有文件；重扫和tone switch存在实例/DSP状态跨线程风险。
- 下一步最高优先级：先保护已有文件，再恢复单一音频所有者/插件停机边界，把相应行为回归接入默认门禁。
- 三闸门命令通过，但WSL编译不是零warning，全量tidy失败。默认Chord套件漏跑；本轮Windows补跑不表示默认门禁已修好。

### 0.4 重点发现

| ID | 优先级 | 状态 | 标题 | 当前结论 |
| --- | --- | --- | --- | --- |
| ERR-001 | P1 | 未处理 | 已有导出目标不是事务替换：成功追加，失败删除原文件 | Windows 冒烟确认内置 WAV、MIDI 覆盖及失败删原文件；插件覆盖同根静态证据 |
| SEC-001 | P1 | 未处理 | 预设重命名可覆盖另一预设或删除自身目标 | 静态确认；运行触发未验证 |
| SEC-002 | P1 | 未处理 | 原生文件绑定未随 Take 替换/Save As 更新 | 静态确认；运行触发未验证 |
| SEC-003 | P1 | 未处理 | 原生 MIDI 编码信任未校验的解码长度前缀 | Windows 冒烟观察到 bad_alloc；实际主窗口异常处理结果未验证 |
| AUDIT-001 THR-004 | P1 | 未处理 | 增量重扫绕过停音频/关 Editor 守卫 | 静态确认；运行触发未验证 |
| THR-001 | P1 | 未处理 | 实时回调常规路径仍有阻塞锁 | Windows 人为争用冒烟确认 callback 等待；实际声卡毛刺未测 |
| PERF-001 | P1 | 未处理 | 密集播放和重复预设循环突破回调预分配 | Windows 密集 MIDI 冒烟确认分配；重复预设循环为静态证据 |
| AUDIT-002 TEST-014 | P2 | 未处理 | 和弦识别7个子测试被默认类别过滤漏跑 | 默认日志及Windows --category devpiano 执行确认 |

---

## 1. 审计范围与方法

### 1.1 审计范围

逐文件覆盖`source/**/*.cpp`与`source/**/*.h`。下表计含空行/注释的物理行；模板历史数量不是本轮基线。

| 模块/路径 | 文件数 | 物理行数 | 职责 |
| --- | --- | --- | --- |
| source/Audio | 13 | 4378 | 音频、物理建模、调律、空间、踏板、节拍器 |
| source/Core | 6 | 1063 | 键位/状态/乐理/节拍/视图数据 |
| source/Diagnostics | 5 | 257 | 日志与MIDI描述 |
| source/Export | 5 | 555 | WAV任务、选项与流程 |
| source/Input | 4 | 828 | 电脑键盘映射、修饰符与触键动态 |
| source/Layout | 4 | 1154 | 预设格式、发现与CRUD |
| source/Locale | 1 | 93 | 语言映射与内嵌语言包 |
| source/root | 5 | 2770 | 应用入口、MainComponent装配/访问器、STL PCH |
| source/Midi | 3 | 206 | 通道矩阵与发音身份路由 |
| source/Plugin | 6 | 1122 | VST3扫描、加载、Editor与恢复 |
| source/Recording | 23 | 4239 | 捕获、Transport、文件及离线渲染 |
| source/Settings | 13 | 2905 | 持久化、快照与设置窗口 |
| source/UI | 149 | 16003 | 业务/原生UI与内化声明式运行时 |
| source/tests | 52 | 18005 | 基础设施及行为测试 |

UI的149文件中110文件/9,494行属于`source/UI/jive/core/`，已内化，**在审计范围内**。排除第三方submodules实现、scripts/、docs/及构建脚本/配置的缺陷面；只读核对它们以确认API/接入/文档契约，不审查或修改JUCE自身。

### 1.2 审计输入与执行顺序

1. 阅读项目范围、architecture、AGENTS、全部ADR-001~014、known-issues、roadmap/current-iteration、模板及既有审计登记表。
2. 按顺序执行WSL Debug构建→默认测试→格式检查。用户本次skill明确要求完整WSL门禁，按此执行，不把它当日常Windows镜像工作流的改变。补self-check、完整只读tidy和Windows验证。
3. 无重叠所有权分为输入/看板35、持久化26、插件10、录制导出18、UI运行时122、测试52；主审音频/入口/MIDI/诊断26。并集289，未分配0。切片不执行检查，主审统一运行。
4. 复核可达生产消费者，在Windows调用实际业务对象，执行§4.4探针。不以mock echo、源文本或编译成功作功能证明。
5. 第8章收敛/去重，第5章覆盖全部未处理ID，统计/状态/首页由同一登记表派生。

**工具限制**：规定的codegraph设备已尝试但未挂载，已提报并回退find/精确源码。LSP references遗漏可见跨TU调用，不能把局部结果当完整blast radius；AudioEngine采样diagnostics不替代tidy。JUCE API先经Context7官方文档再核对本地9.0.1源码，第三方路径仅作API事实证据。

### 1.3 严重级别定义

沿用模板P0~P3。崩溃/数据损坏/音频毛刺无声/泄漏/线程安全缺陷从严列P0~P1；未实测主窗口崩溃不使已证明危险路径降为P2。P2为中等功能/边界/维护缺口，P3为低风险契约/测试整理。本轮未登记P0，不表示不存在未发现的P0。

### 1.4 状态与确定性定义

固定状态：未处理/处理中/已缓解/已暂缓/已关闭。本轮不修复、不关闭问题，全部未处理。Windows冒烟只证明所列输入/观测；静态事实与尚未运行场景分别标注，未观测的崩溃/性能/插件影响使用[未验证]/[推断]。历史新反证保留原ID，既有报告状态不覆盖。

---

## 2. 项目画像

### 2.1 项目类型与核心能力

专用电脑键盘钢琴桌面宿主：默认7声学系统物理钢琴、可切Sine、VST3扩展；Group/修饰符/发音身份、双映射看板、16通道矩阵、录制/MIDI/原生文件/WAV、节拍/count-in、Seek/A-B练琴、双语。坚持`Performance Input -> Instrument -> Master -> Output`，不要求外部MIDI、Patchbay、多轨DAW或视频栈。

### 2.2 技术栈与运行环境

| 类别 | 当前核对 |
| --- | --- |
| 语言/框架 | C++20，JUCE 9.0.1，内化声明式UI |
| 构建 | CMake+Ninja；WSL Debug/mold；Windows MSVC 19.51.36260.0 Debug |
| 测试 | JUCE UnitTest聚合目标devpiano_tests；默认类别仍有遗漏 |
| 门禁 | clang-format-21；clang-tidy-21只检查，不自动fix |
| 资产/状态 | BinaryData、ValueTree/JSON/XML；原生MIDI编码是JUCE特有格式，不是标准Base64 |
| 验证 | 用户要求的WSL三闸门，Windows镜像独立Debug构建/测试/消费者冒烟；未运行Release |

### 2.3 目录与模块边界

Core/AppState已消除原Settings/Midi逆向include；PluginFlowSupport无持有状态、显式注入。正常插件加载/卸载/离线快照有停设备守卫，ViewHost拆树执行有序清理，不能泛化指控裸指针/StyleSheet必然UAF。问题是例外路径：重扫未进guard、tone switch直接提交、活动Transport消息线程改游标，以及预设/看板身份和执行边界。MainComponent.cpp为1,354行、访问器930行；规模本身不登记，不用整文件重写替代窄边界修复。

---

## 3. 分领域审计结果

本章只有分析摘要，状态以第8章为准。

### 3.1 架构与模块边界

- 评级：**C**。
- 结论：固定拓扑和分层可复用，缺口是最终映射SSOT、稳定插件/预设身份及实时/离线可执行事件。
- 关联问题：`ARCH-001`、`ARCH-002`、`ARCH-003`、`ARCH-004`。


### 3.2 代码质量与可维护性

- 评级：**C**。
- 结论：RAII和值快照不保证文件所有权、复制完整性或反馈单向性。标签、力度、动画、Notes与柔音声部均有可见反例。
- 关联问题：`QUAL-006`、`QUAL-007`、`QUAL-008`、`QUAL-009`、`QUAL-010`、`QUAL-013`、`QUAL-019`。


### 3.3 线程安全与并发

- 评级：**D**。
- 结论：常规callback阻塞锁、游标所有权混用、重扫和tone switch例外违反边界。只改atomic不能保证一致时序快照；普通guard安全不代表所有修改安全。
- 关联问题：`THR-001`、`known-issues §2/Phase 6-2 播放速度控制`、`AUDIT-001 THR-004`、`AUDIT-002 THR-001`、`THR-002`、`AUDIT-002 THR-003`。


### 3.4 安全边界

- 评级：**C**。
- 结论：主要威胁是用户选取本地文件与已有数据保护，不夸大为远程执行。框架accessor的前提须由准入保证。
- 关联问题：`SEC-001`、`SEC-002`、`SEC-003`、`SEC-004`、`SEC-005`、`SEC-006`。

| 检查项 | 结果 | 事实边界 |
| --- | --- | --- |
| 文件/已有数据所有权 | 失败 | 目标追加/删除、rename和旧Take绑定，不夸大为路径越权 |
| 插件加载 | 部分 | 进程内受信VST3是产品选择；重扫生命周期另列，不要求未规划沙箱 |
| 键位/矩阵/数组 | 部分 | 多数钳制有效，最终投影/静音/keyUp语义有缺口 |
| JSON/版本/原生消息 | 部分 | 根/格式/版本有检查，长度前缀及时间域缺预算/一致性 |
| MIDI结构/meta | 失败 | 缺失声明轨误接收；0x58指数未验证 |
| 资源准入 | 部分 | Preset1MiB/MIDI32MiB/Locale2MiB守卫已有，原生解码放大独立 |
| 时间线数值 | 失败 | 极小正率及饱和后加法破坏派生长度 |

### 3.5 资源与性能

- 评级：**C**。
- 结论：prepare/捕获容量守卫有效，但密集播放和重复预设通知突破预分配；日志是启动裁剪；强杀不保证RAII。未测本轮CPU0.7% SLA，不以旧实测代当前证据。
- 关联问题：`PERF-001`、`RES-001`、`THR-002`、`known-issues §1/节拍器每拍三角函数与全回调零三角 SLA 不一致`。


### 3.6 错误处理与可观测性

- 评级：**C**。
- 结论：普通保存/渲染失败有日志。真正问题是部分MIDI误称尾字节、重复插件误称无类型、新保存被回滚和力度数字错误，不重新开泛化无日志项。
- 关联问题：`ERR-001`、`ERR-002`、`ERR-004`、`QUAL-015`、`OBS-001`、`ERR-003`。


### 3.7 测试体系

- 评级：**C**。
- 结论：有真实频谱/衰减/身份/A-B/往返测试，但类别、fixture、用户目录副作用及自证/copy-pinning会制造假信心。controller测试主要是flow/startup helper或嵌套Session类型，未执行完整controller方法；缺口附功能回归，不重复开零覆盖项。
- 关联问题：`AUDIT-002 TEST-014`、`TEST-001`、`TEST-002`、`TEST-003`。

补消费者边界：On/Off之间改映射、同身份交错释放、生产ViewHost键入、旧文件覆盖/失败保留、完整MTrk失败、pause终结、活动Transport交错和准备后分配计数。删除文案/自证测试，不重钉新实现。
### 3.8 文档与配置契约

- 评级：**C**。
- 结论：roadmap已有多项诚实限制。真实配置缺陷是copy漏字段和同步/防抖顺序；功能文档仍含rename确认、预设改调号、MIDI双轨等反证。历史ADR数值不自动构成决策违反。
- 关联问题：`QUAL-006`、`ERR-002`、`DOC-001`、`known-issues §1/A4 基准音高范围与项目契约不一致`。


### 3.9 工程化与构建

- 评级：**C**。
- 结论：144/144 source cpp有编译数据库记录，无孤立cpp；Debug双平台及格式通过。warning/tidy失败必须记录，默认Windows缓存坏是排除范围内环境输入，不另开脚本缺陷。Release未执行。
- 关联问题：`ENG-001`。

tidy数万warnings generated不是项目可见诊断数；按实际source位点5项错误计。最近基线subject符合chore规则，本轮不提交、不以单条推断全历史纪律。
### 3.10 ADR 合规审计

- 评级：**C**。全部14条按决策本体核对，含废止/取代记录；仅本体违例开CMPL。本轮不修改ADR，不因历史规模/状态变化开CMPL。

| ADR | 决策要点 | 审计证据 | 合规状态 |
| --- | --- | --- | --- |
| ADR-001 | WSL主源与Windows镜像分离 | win-build路由G盘镜像；旧缓存失败、新镜像Debug验证成功；source无编辑 | 合规 |
| ADR-002 | 旧源码仅历史参考、已移除 | freepiano-src不存在；source旧引用/原生入口复合grep零命中 | 合规 |
| ADR-003 | 无持有状态、显式注入 | PluginFlowSupport.h:17-40/.cpp:29-54，free functions，host/settings/callback参数注入 | 合规 |
| ADR-004 | JUCE AudioDeviceManager | MainComponent.cpp:1256-1281；SettingsComponent.cpp:132-162；旧原生入口/SDK include检查零命中 | 合规 |
| ADR-005 | JUCE管理VST3实例 | PluginHost.cpp:18-19,294-313,334-386；旧AEffect/effOpen/audioMaster零命中 | 合规 |
| ADR-006 | 无外MIDI硬件输入 | MidiInput枚举/open、MidiInputCallback、MidiRouter、externalMidi残留检查零命中 | 合规 |
| ADR-007 | tidy只检查、不自动fix | 本轮全量无--fix；零诊断失败登记ENG，不是自动修复纪律违例 | 合规 |
| ADR-008 | 已被014替代 | 按废止关系核对，声明式理念保留，不恢复退役子模块 | 合规 |
| ADR-009 | 增强模态Piano默认、Sine可切 | AudioEngine.cpp:261-271，Magic Circle分音循环；机械辅助全回调SLA另按known-issues登记 | 合规 |
| ADR-010 | BinaryData嵌入资产 | CMakeLists.txt:328-334；Locale/DesignTokens/StyleCatalog读嵌入资产 | 合规 |
| ADR-011 | Embedded/PCH/快速链接器 | CMakeLists.txt:3-25,30-38,106-116,384-385；pch无JUCE聚合头，mold日志及真实MSVC Debug | 合规 |
| ADR-012 | 业务头禁JuceHeader | WindowIconUtils.h:3违例；TestHelpers已细粒度，不重开旧helper例外；CMPL-001 | 部分合规 |
| ADR-013 | 移除inspector | 目录不存在；source/.gitmodules/CMake残留检查零命中 | 合规 |
| ADR-014 | 内化许可与严格ViewHost门面 | 110文件保留版权/MIT、外JIVE已移除；Main/raw modal逃逸违strict facade；CMPL-002 | 部分合规 |

旧后端检测针对IAudioClient/IMMDeviceEnumerator/DirectSoundCreate/ASIOCreateBuffers及原生include等，不把WASAPI/ASIO诊断名称/注释误报。ADR合规不表示全局Realtime Safety已达标，THR/PERF保留真实差距。

---

## 4. 验证记录

### 4.1 命令执行结果

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| ./scripts/dev.sh wsl-build | 通过（非零warning） | Debug完成148.68s，20次warning/2源位点，刷新compile_commands |
| ./scripts/dev.sh test | 通过（默认选择范围） | CTest1/1，18.42s，95套件530子测试97,823通过0失败；未含Chord |
| ./scripts/dev.sh format --check | 通过 | clang-format-21，0差异，1.00s |
| ./scripts/dev.sh self-check | 通过 | PowerShell、G盘镜像、vswhere可用，默认变量提示非失败 |
| ./scripts/dev.sh tidy --all | 失败 exit1 | 全144cpp，824.22s，MidiTextDecoder:200/211/289/297与PerformanceModifierTest:220，共5项目诊断 |
| clang-tidy -p build-wsl-clang source/**/*.cpp | 未执行原字面写法 | 用项目标准tidy --all全量替代，不重复可能漏glob的扫描 |
| ./scripts/dev.sh win-build | 失败（原命令） | 同步成功，旧CMAKE_MAKE_PROGRAM的D:/PROGRA~1/MICROS~2/18/ENTERP~1/.../ninja.exe不存在 |
| Windows cmake --preset windows-msvc-debug -B build-win-msvc-audit004 -DBUILD_TESTS=ON；cmake --build ... | 通过（镜像独立Debug） | 相同preset、新树避旧缓存，不删原树、不改配置；MSVC19.51，含build/test395.90s |
| Windows ctest --test-dir build-win-msvc-audit004 -C Debug --output-on-failure | 通过 | 1/1，63.38s，95套件530子测试97,822通过0失败 |
| Windows devpiano_tests.exe --category devpiano | 通过补跑漏项 | 1套件7子测试，115通过0失败，0.46s；默认类别未修 |
| Windows临时消费者探针S01-S29 | 已执行，复现缺陷 | 真实业务对象/生产工厂/绘制及编译边界；见§4.4，不是修复后通过 |
| LSP diagnostics/references | 采样/不完整 | AudioEngine TU查询未报错误，但references漏跨TU，不能作全量零诊断/完整影响面 |
| Release/TSAN/真声卡/真实VST3组合 | 未执行 | 未要求Release；未在用户硬件/插件上冒险强杀/崩溃重现 |

三闸门是本轮真实基线复验，未修源码，不作修复后全绿承诺。两端断言相差1是实际结果，均0失败；额外Chord补跑不修复默认选择集合。

### 4.2 文件统计

| 指标 | 值 |
| --- | --- |
| cpp/h/审计并集 | 144 / 145 / 289 |
| 总物理行（含空行/注释，排除.inl/.loc/.json） | 53578 |
| tests cpp/h | 51 / 1 |
| 默认套件/唯一子测试 | 95 / 530 |
| 额外Chord套件/子测试 | 1 / 7（默认漏跑） |
| beginTest源码调用数 | 344，testCase动态子测试另计，不能当执行数 |
| WSL/Windows默认断言通过 | 97,823 / 97,822，各自0失败 |
| 最大文件 | source/Audio/PianoSynthVoice.h，1,734行 |
| 次大文件 | source/tests/StyleCatalogTest.cpp，1,713行 |
| 编译数据库实际source cpp/未编译cpp | 144 / 0 |
| 内化UI文件/行 | 110 / 9,494 |

统计：枚举source/.cpp/.h，UTF-8 splitlines计物理行，与compile_commands实际file集合做差集。子测试取LastTest.log唯一Starting tests in: suite/name观测，避免stdout/logger重复；不引用旧报告断言数。

### 4.3 未执行验证、环境与只读边界

- 原win-build失败单列，新树通过不覆盖原失败。下一轮修复默认缓存工具路径，或显式继续独立树；本轮未删原缓存、未改镜像源码/配置。
- 未执行Release、本轮CPU0.7% SLA、真声卡延迟/热插拔、真实VST3 Editor重扫崩溃/同名/再次拖放/offline听感、慢插件超3秒取消及TSAN。影响为[未验证]，静态危险路径不降级。
- UI是实际组件离屏绘制/生产工厂键入，非完整Windows桌面/DPI/输入法手测。Canvas PNG已查看；Notes只读已直接键入证实。
- 单测默认logger触碰用户目录已登记TEST-002，不再用用户真实日志做破坏性重现。业务探针输出/日志在自有临时目录。
- 初始临时探针缺audio_formats include、Notes未注册工厂；补齐探针前置后只执行对应路径，不记成项目缺陷。
- SHA-256核对289源码及86既有docs/配置与基线一致。仅新增本报告；自有探针/产物清理，Windows独立Debug树保留为验证缓存。

### 4.4 Windows 消费者冒烟证据与复核输入

沿用MSVC Debug测试目标宏/PCH和当前项目对象，不替换映射/写出/生产工厂。S24只对探针用/Zc:nrvo-；S28只算长度，不渲染巨大时间线；S20仅捕获独立进程异常，不执行2GiB负载/主窗口破坏性输入。

| 观测ID | 复核输入/调用 | 实际输出 |
| --- | --- | --- |
| S01 | 播放 NoteOn60→offset+1→原NoteOff@256 | old_note_after_off=1；new_note=0；position=384 |
| S02 | prepare(128,48000)后首块5000个有效CC | callback_alloc_or_realloc=10；plugin_resize=0 |
| S03 | 另一线程在keyboard Listener中持有状态锁 | callback在50ms观察窗口未完成；释放竞争者后完成 |
| S04 | 独立日志limit1024，64×128字节写入 | observed_bytes=8465 |
| S05 | 默认Q/K同为Ch1 MIDI72，只松Q | K仍held=1；pitch72=off |
| S06 | 真实鼠标C4两次触发Ch1→2、Ch2→3矩阵 | first_click_ch2=1；second_click_ch3=1 |
| S07 | setKeyboardLayout后setKeyboardSettings | 绑定标签数31→0 |
| S08 | fadeSpeed=1，松键后推进120帧 | fade=1.000000；目标previewAlpha=0 |
| S09 | 静音A binding.velocity=0并启用Shift | snapshot.velocity=1.000000 |
| S10 | A经矩阵transpose+12/outputCh4 | 显示note60/ch1，实际ch4/note72=on |
| S11 | 173BPM/启用/两小节/关闭cadence经store防抖保存 | 保存后120BPM/disabled/countin0/cadence1 |
| S12 | schedule旧设置→save新设置→pump timer | new-cache被旧payload写回old-cache |
| S13 | 同WAV路径先8kHz再16kHz导出 | 两次成功；64,904→194,608字节；reader仍8,000Hz |
| S14 | 同MIDI路径先note60再note72导出 | 两次成功；完整readFrom=false；已解析首note仍60 |
| S15 | 预置目标文件，WavExportTask以sampleRate0失败 | export_ok=0；原文件survives=0 |
| S16 | 录On60→pause期间Off60→resume→stop | 保留take On=1/Off=0，length256 |
| S17 | length128、Off@128，128帧块渲染/advance | ended=1，On=1/Off=0 |
| S18 | 原生JSON序列Off@1024排在On@0前面 | 加载admitted=1，播放交付On=0 |
| S19 | 原生sampleRate=1e-300 | 加载admitted=1；派生位置1即ended |
| S20 | 原生midiData="-1."，仅在独立进程边界捕获异常 | std::bad_alloc=1；未测试主窗口退出表现 |
| S21 | Type1 header声明2轨，只提供完整第1 MTrk | import_success=1；parsed_tracks=1 |
| S22 | 真实BackgroundCanvas固定100×100，半径0→30→0 | corner alpha依次255→0，均与当前半径相反；离屏图片已查看 |
| S23 | 注册生产ViewHost默认工厂后解释metadata布局，键入x | readonly=1；key_handled=0；text保持before |
| S24 | 复制实际makeBlock工厂，仅探针使用/Zc:nrvo- | info_bound_to_live_buffer=0；未解引用悬垂指针 |
| S25 | 真实describeMidiMessage(noteOn velocity0.5) | raw_velocity=64；日志vel=8128 |
| S26 | keyUp A绑定，完整按下/释放周期 | down_handled=0；up_handled=0；emitted_note=0；held=0 |
| S27 | 真实MusicTheory标签0/1/11/12边界 | C-1 / C#0 / B0 / C0，1和11八度错误 |
| S28 | finalEvent.timestamp=INT64_MAX，仅计算渲染长度 | 最终时间戳9223372036854775807；derived_length=1 |
| S29 | 当前8voice PianoSynth拓扑，CC67先于同通道两个音 | sounding_voices=2；soft_pedal_voices=1 |

### 4.5 待用户Windows实机验证

| 组合 | 应验证不变量 |
| --- | --- |
| 已加载VST3+Editor+重扫 | 可丢弃会话观察editor/callback在unload前停止；本轮未实测插件崩溃 |
| 同名/再次拖入已知VST3 | 乐器/效果过滤与真实description一致；unload后拖回已扫描插件可加载 |
| 替换原生Take/Save As/预设rename | 用复制数据验证，文件A不被B元数据编辑改写；规范化等路径不删除、碰撞先确认 |
| 慢插件取消/退出 | 记录取消完成、实际线程退出、句柄/头关闭，不冒险强杀正式会话 |
| Count-in/采样率切换 | 一/两完整小节到下一downbeat；活动48k↔44.1k位置/时长不变形 |
| 辅助窗口/失焦/输入法 | 按住一键转到settings/editor后再松，确认后续释放；静态时间窗未另立确认缺陷 |
| 密集回放/热插拔/声卡 | 记录真实分配、Listener/UI、异常几何计数和声音；人为50ms锁争用不等于声卡实测毛刺 |

---

## 5. 修复路线图

只排期、不实施，状态以第8章为准，历史/known引用也纳入清单。

### 5.1 立即处理（P0）

本轮无P0登记；新实机崩溃/不可恢复损坏证据应升级第8章，不降低静态风险。

### 5.2 当前迭代处理（P1）

**推荐顺序**：已有文件保护/准入预算 → 插件/voice/Transport所有权 → NoteOff和实时有界交换 → 对应默认消费者回归/隔离门禁。下列每ID均需自身验收。

- [ ] `ERR-001`：统一同目录 TemporaryFile 写出、关闭 writer 后替换，仅清理任务自有临时文件；保留取消/失败时原目标。
- [ ] `SEC-001`：比较规范化后的源/目标路径；同路径不得删除，独立已有目标先确认，失败保留两份原始数据。
- [ ] `SEC-002`：建立 Take 与 backing file/metadata 的一致所有权；替换时解除旧绑定，成功打开/保存后绑定新文件；验证 A 原字节不被 B 元数据编辑改写。
- [ ] `SEC-003`：解码前验证 JUCE 特有编码的长度、数据预算和负载一致性；校验解码后的 MIDI 帧形状并明确传播读取失败。
- [ ] `SEC-004`：准入时检查有限、支持范围的采样率、非负且一致的长度/时间戳；消费端使用检查后的比例和整数转换。
- [ ] `SEC-005`：在打开输出前验证最终事件与尾部长度均可表示；检查加法，不将转换饱和视为整个时间线已安全。
- [ ] `SEC-006`：读取前验证固定宽度 meta 长度及拍号分母指数；畸形值拒绝/显式忽略并报告，不直接进入框架 accessor。
- [ ] `AUDIT-001 THR-004`：复用设备重建守卫，在扫描卸载前关闭 Editor 并停止 callback；覆盖已加载+Editor 打开时重扫。
- [ ] `AUDIT-002 THR-001`：将重建与参数提交放入明确停音频窗口，或仅由音频所有者完成受控切换；不能只依赖单次 getVoice/clear/add 的内部锁。
- [ ] `THR-001`：把演奏事件、显示快照和预设通知收敛到预分配无锁通道；避免 UI 与音频共享可阻塞状态锁。
- [ ] `PERF-001`：确定每块容量与有界溢出策略、复用预分配通知存储；覆盖合法密集事件及消息线程尚未drain的重复循环。
- [ ] `known-issues §2/Phase 6-2 播放速度控制`：发布 transport 命令，在音频块边界一致应用倍率、位置和游标；结构性停止复用停机守卫；补真实双线程回归。
- [ ] `THR-002`：仅协作取消，异步等待实际工作线程退出后再释放任务/插件/文件所有权；验证慢 processBlock 的取消和退出。
- [ ] `ERR-002`：同步 save 成功后取代/撤销相同store的待写payload；定义直接保存与防抖保存的顺序语义。
- [ ] `QUAL-001`：播放侧也保存 NoteOn 最终输出身份，NoteOff按原身份；定义重叠同音及变化时正在发声的处理，不用当前映射重算。
- [ ] `QUAL-002`：在最终发音身份层维护重叠持有者，最后释放再NoteOff；保留每个物理键原身份。
- [ ] `QUAL-003`：在冻结捕获时间轴的边界补齐已录身份/踏板终结状态；暂停期间排除新演奏，但保证保留Take配对。
- [ ] `QUAL-004`：播放长度包含最后事件或在音频边界明确终结；对齐实时/离线可听结束和最终NoteOff采样点。
- [ ] `QUAL-005`：原生文件准入拒绝或稳定规范化非单调时间线；保留同采样语义顺序并测试真正播放/seek。
- [ ] `ERR-003`：兑现公开keyUp触发的可配对事件语义或在准入明确拒绝；不能接受文件配置后静默丢弃。
- [ ] `QUAL-018`：设备prepare时统一重基准活动Transport；录制按固定Take域换算，或明确先结束会话；验证位置/倍速/NoteOff连续性。
- [ ] `TEST-001`：由调用者从live buffer构造info，或fixture移动显式重绑定；用合法禁NRVO配置验证指针与实际音频行为。
- [ ] `TEST-002`：删除只测incidental默认路径的探针或置于隔离profile；全部文件测试使用已有ScopedTempDir，不再修改用户诊断历史。
- [ ] `AUDIT-002 THR-003`：实时Listener仅有界快照/通知，消息线程处理UI和Timer；覆盖电脑/鼠标/回放/失焦。
- [ ] `known-issues ERR-002`：先明确定义设备/插件异常几何的安全处理并保持观测计数；对目标声卡热插拔验证，不把正常块plugin_resize=0当异常已修。

### 5.3 近期排期（P2）

- [ ] `QUAL-006`：补齐全部持久化字段的复制语义，同时保留 XML 独立所有权；验证首次及再次防抖写出的真实值。
- [ ] `QUAL-007`：分开配置输入身份与显示用输出通道；鼠标始终从当前映射输入身份触发并保存最终输出。
- [ ] `ARCH-001`：映射层输出两个视图共享的最终只读投影；同时明确点击输入身份，避免展示修复后再次矩阵变换。
- [ ] `QUAL-008`：几何重建保留或重新消费已有映射视图标签，不把标签仅存于一次临时KeyRenderState赋值。
- [ ] `QUAL-009`：复用同一静音优先级规则生成快照和点击事件；验证最终MIDI而不仅held.velocity。
- [ ] `QUAL-010`：统一UI/加载器的收缩系数范围，或为端点定义显式可终止动画；验证停止和有界alpha。
- [ ] `QUAL-012`：先更新radii再重建路径；在不改变bounds情况下连续改两次半径，验证真实角像素/路径。
- [ ] `QUAL-013`：仅元数据Notes使用可编辑工厂/显式恢复输入能力，保留诊断列表只读；测试走生产ViewHost并注入用户键入。
- [ ] `QUAL-014`：prepare前setNonRealtime(true)，保证setup和process一致；选择依赖offline模式的真实VST3对照验证。
- [ ] `QUAL-015`：分离探测到的有效类型与是否新增列表条目，重复文件也返回可加载身份并保留metadata更新。
- [ ] `ARCH-002`：贯穿选择/加载/持久化稳定description身份，显示名仅展示；验证同名不同ID及乐器/效果过滤。
- [ ] `ARCH-003`：保存稳定预设身份/Take内映射或快照并定义缺失行为；迁移格式时不得静默重解释旧数字。
- [ ] `QUAL-016`：统一启动/用户选择的预设激活操作及当前身份，保证列表选择、自动保存和运行布局一致。
- [ ] `ARCH-004`：保留事件variant与准备好的声学快照，按采样偏移执行实时/离线同构语义，UI通知独立且不丢末块。
- [ ] `QUAL-017`：在目标音符前恢复目的位置的状态快照，保持不自动重发历史NoteOn的现有策略；测试program/bank/CC64及pitch。
- [ ] `FIX-035`：以完整音频节拍时段/下一个目标downbeat完成并消费序号差；实际控制器验证一/两小节及跨多拍poll。
- [ ] `ERR-004`：仅完整声明结构后额外后缀允许宽容，缺失/短chunk应拒绝且保留现有Take；保留真实CRLF后缀兼容。
- [ ] `RES-001`：实现可观测的会话内有界轮转，或明确真实只在启动裁剪的契约与风险；验证长会话及轮转故障。
- [ ] `CMPL-001`：以实际需要的细粒度模块头替换并检查消费者；不使用测试头例外为业务头开豁免。
- [ ] `CMPL-002`：将业务样式刷新/内置modal初始化封装在ViewHost边界内；如确需例外则另行明确批准决策，而非保留无约束逃逸。
- [ ] `ENG-001`：逐项评估编译/静态诊断并小步修正，必要规则争议如实记录；禁止--fix自动改源码或压制未知风险。
- [ ] `AUDIT-002 TEST-014`：迁入既有DevPiano/Core类别并确认默认日志包含7个子测试；区分编译接入和执行接入。
- [ ] `known-issues §1/节拍器每拍三角函数与全回调零三角 SLA 不一致`：按完整调用闭包界定/验证SLA；优先预计算或递归机械振荡，保持听感及踏板语义；CPU期限效果另测。
- [ ] `known-issues §1/A4 基准音高范围与项目契约不一致`：以原已知项统一修正调律引擎、设置、预设与导出边界并测试两端；此前文档标注保持真实。
- [ ] `OBS-001`：直接展示原始整数力度或正确使用getFloatVelocity换算；验证边界及中间值，不钉完整自然语言日志。
- [ ] `QUAL-019`：由乐器拥有者维护当前踏板状态，保证每个新起声部继承；保留VST3通道语义并验证踏板先于和弦、换声部及释放。

### 5.4 后续优化（P3）

- [ ] `QUAL-011`：使用等价数学floor的MIDI八度换算，覆盖0/1/11/12边界和唱名偏移。
- [ ] `TEST-003`：删除copy-pinning/自证测试，不重新钉新文本/数值；保留语言机制和生产组件行为；接入确定性的真实生命周期用例。
- [ ] `DOC-001`：修复实现后按真实契约同步功能/手工验收；事实描述不另开CMPL；本轮不改任何既有文档。

### 5.5 覆盖率校验

登记表未处理ID与路线项按完整命名空间提取、去重并排序，实际执行 `LC_ALL=C comm -3 /tmp/devpiano-audit004-registry.ids /tmp/devpiano-audit004-roadmap.ids`：空输出（exit 0），54/54覆盖，缺失/多余均0。新前缀连续，历史/known保留原ID/heading。

---

## 6. 最终结论

### 6.1 当前判断

**C，先处理P1，非新增功能就绪。** 固定拓扑、正常RAII/guard和已有行为测试保留；值快照/atomic/unique_ptr/预分配不等于已有数据、线程或事件语义正确。54项未处理，未修复/关闭。

### 6.2 是否建议继续新增功能

**否**。Phase36声学新增/Phase37分区叠层会放大已有声部、预设身份和通道/线程问题。先完成§5.2；未实测崩溃不是扩展安全证明。

### 6.3 是否建议先重构 / 补测试 / 补文档

- 重构：**有条件，窄边界优先**；输出事务、音频所有权、稳定身份/快照，不引入通用DAW/UI框架。
- 补测试：**是**；将已复现消费者边界固化为隔离、确定性回归，修类别/fixture/用户目录副作用，删文案和自证用例。
- 补文档：**是，修复后同步**；状态仍由roadmap管理。本次不改ADR/roadmap/known-issues或历史报告。

### 6.4 下一步三件事

1. **最推荐**：已有文件保护专项，事务式MIDI/WAV替换、rename与Take/backing-file绑定，覆盖失败/取消原字节保留，拦截原生解码放大。
2. 恢复活动Transport、tone switch和插件重扫单一所有权，修NoteOff及实时无锁有界交换；可丢弃Windows会话回归Editor/退出。
3. 修默认Chord、fixture/用户目录隔离及编译/tidy，补实际controller/生产工厂回归；按§4.5完成声卡/插件手测后再评估下一功能阶段。

---

## 7. 复审记录

### 7.1 首次审计登记（2026-10-02）

- 基线：main @ `732cde18dc8116cb21f0325377415cedfef03811`。
- 已关闭：无。没有源码/测试/既有文档修复，不声明历史问题本轮已修。
- 新反证的历史问题保留原ID列未处理，既有报告状态不覆盖。
- 验证：§4.1三闸门/独立Windows默认测试/Chord补跑及§4.4消费者冒烟；tidy和原win-build失败如实保留。
- 结论：54项未处理，45新发现、9引用/重开。下次复审追加小节，第8章状态驱动首页/路线图；修复历史写实现迭代/roadmap，不堆入登记表。

---

## 8. 附录：问题总表（登记表）

唯一状态源，54行全部未处理。关闭须代码/测试/运行证据；暂缓/缓解须真实风险接受、触发条件和复审时间，不由审计代用户接受。新反证保留原ID/来源命名空间，避免与本报告新编号混淆。

| ID | 领域 | 问题标题 | 优先级 | 状态 | 来源 | 影响摘要 | 证据 | 风险接受原因 | 重开条件 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ERR-001 | 错误处理/持久化 | 已有导出目标不是事务替换：成功追加，失败删除原文件 | P1 | 未处理 | 审计 | 覆盖 WAV/MIDI 时仍读取旧音频/音符；WAV 失败或取消可删除用户原文件。 | source/Recording/WavFileExporter.cpp:77-91；source/Recording/MidiFileExporter.cpp:47-49；source/Recording/PluginOfflineRenderer.cpp:87-100；source/Export/WavExportTask.cpp:207-216；S13-S15；[Windows 冒烟确认内置 WAV、MIDI 覆盖及失败删原文件；插件覆盖同根静态证据] | - | 未处理项不适用；未接受风险 | 统一同目录 TemporaryFile 写出、关闭 writer 后替换，仅清理任务自有临时文件；保留取消/失败时原目标。 |
| SEC-001 | 安全/数据完整性 | 预设重命名可覆盖另一预设或删除自身目标 | P1 | 未处理 | 审计 | A→已有 B 无确认覆盖；A→A?、Windows 大小写等同路径重命名可能随后删除新文件。 | source/Layout/PresetFlowSupport.cpp:302-330；source/Layout/PerformancePreset.cpp:209-223；docs/reference/features/performance-presets.md:156；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 比较规范化后的源/目标路径；同路径不得删除，独立已有目标先确认，失败保留两份原始数据。 |
| SEC-002 | 安全/数据完整性 | 原生文件绑定未随 Take 替换/Save As 更新 | P1 | 未处理 | 审计 | 打开原生 A 后导入/录制 B，再保存歌曲信息会把 B 全量事件覆盖到 A；Save As 也不重绑定。 | source/Recording/RecordingSessionController.cpp:66-71,350-359,370-373,396-403,667-695,765-775；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 建立 Take 与 backing file/metadata 的一致所有权；替换时解除旧绑定，成功打开/保存后绑定新文件；验证 A 原字节不被 B 元数据编辑改写。 |
| SEC-003 | 安全/数据完整性 | 原生 MIDI 编码信任未校验的解码长度前缀 | P1 | 未处理 | 审计 | 极小 .devpiano 的 midiData="-1." 可请求 SIZE_MAX 并抛 bad_alloc；正的大长度可放大内存；未声明主 GUI 的退出表现。 | source/Recording/PerformanceFile.cpp:70-79,269-279；JUCE API 证据 juce_MemoryBlock.cpp:394-419；S20；[Windows 冒烟观察到 bad_alloc；实际主窗口异常处理结果未验证] | - | 未处理项不适用；未接受风险 | 解码前验证 JUCE 特有编码的长度、数据预算和负载一致性；校验解码后的 MIDI 帧形状并明确传播读取失败。 |
| SEC-004 | 安全/数据完整性 | 原生采样率/长度缺少可表示范围验证 | P1 | 未处理 | 审计 | sampleRate=1e-300 通过正数检查；播放缩放得到无穷并转换为 int64，MSVC 冒烟一块即结束，时长错误。 | source/Recording/PerformanceFile.cpp:206-213；source/Recording/RecordingEngine.cpp:225-231,643-654；S19；[Windows 冒烟确认极小正率被接受并错误结束；C++ 越界转换为静态证据] | - | 未处理项不适用；未接受风险 | 准入时检查有限、支持范围的采样率、非负且一致的长度/时间戳；消费端使用检查后的比例和整数转换。 |
| SEC-005 | 安全/数据完整性 | 饱和时间戳后 +1/尾部采样加法仍溢出 | P1 | 未处理 | 审计 | INT64_MAX 经 clampToInt64 后，finalEvent+1 和两秒尾部加法可能有符号溢出，输出时间线/终止条件失效。 | source/Recording/RenderPipeline.cpp:40-47；source/Recording/WavFileExporter.cpp:105-107；source/Recording/PluginOfflineRenderer.cpp:108-112；相关 AUDIT-002 SEC-006 的转换限幅仍有效；S28；[Windows MSVC边界冒烟确认最终时间戳MAX却派生长度1；有符号溢出为静态证据] | - | 未处理项不适用；未接受风险 | 在打开输出前验证最终事件与尾部长度均可表示；检查加法，不将转换饱和视为整个时间线已安全。 |
| SEC-006 | 安全/数据完整性 | MIDI 拍号元数据未验证负载及位移指数 | P1 | 未处理 | 审计 | 普通小 .mid 的 0x58 元数据可令 JUCE 执行 1<<255，属于未定义位移；未声称已复现崩溃。 | source/Recording/MidiTrackMergeEngine.cpp:226-234；JUCE API 证据 juce_MidiMessage.cpp:867-873；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 读取前验证固定宽度 meta 长度及拍号分母指数；畸形值拒绝/显式忽略并报告，不直接进入框架 accessor。 |
| AUDIT-001 THR-004 | 线程安全 | 增量重扫绕过停音频/关 Editor 守卫 | P1 | 未处理 | 历史问题重开（新绕过路径） | Scan 直接卸载仍被音频回调和 Editor 使用的实例；存在 UAF/崩溃风险，真实 VST3 崩溃未实测。 | source/MainComponent.cpp:219；source/Plugin/PluginOperationController.cpp:128-145；source/Plugin/PluginHost.cpp:82-83；source/Audio/AudioEngine.cpp:129-146；AUDIT-003:450；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 复用设备重建守卫，在扫描卸载前关闭 Editor 并停止 callback；覆盖已加载+Editor 打开时重扫。 |
| AUDIT-002 THR-001 | 线程安全 | 音色重建仍从消息线程应用活动 DSP 参数 | P1 | 未处理 | 历史问题重开（tone switch 新反证） | 普通参数 setter 已原子化，但 rebuildSynth 最后直接 applyPendingParameters，可能与音频渲染/快照应用并发写 voice、roomReverb 和派生参数。 | source/Audio/AudioEngine.cpp:248-276,315-378；source/Main.cpp:31-38,52-63；source/MainComponent.cpp:1094-1097；AUDIT-002 第8章 THR-001；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 将重建与参数提交放入明确停音频窗口，或仅由音频所有者完成受控切换；不能只依赖单次 getVoice/clear/add 的内部锁。 |
| THR-001 | 线程安全 | 实时回调常规路径仍有阻塞锁 | P1 | 未处理 | 审计 | collector、keyboard state、Synthesiser 与 presetChangeLock 均可在 audio callback 阻塞；试验观察至少50ms争用等待。 | source/Audio/AudioEngine.cpp:117-118,159,491；source/Recording/RecordingEngine.cpp:547-549,635-640；本地 JUCE collector:86、keyboard state:152、Synthesiser:193；S03；[Windows 人为争用冒烟确认 callback 等待；实际声卡毛刺未测] | - | 未处理项不适用；未接受风险 | 把演奏事件、显示快照和预设通知收敛到预分配无锁通道；避免 UI 与音频共享可阻塞状态锁。 |
| PERF-001 | 性能/实时性 | 密集播放和重复预设循环突破回调预分配 | P1 | 未处理 | 审计 | 有效5000 CC/128帧块触发10次alloc/realloc，plugin resize为0；循环反复排同一预设也超过单次Take计数reserve。 | source/Audio/AudioEngine.cpp:77-81,452-492；source/Recording/RecordingEngine.cpp:247-255,547-549,635-640；S02；[Windows 密集 MIDI 冒烟确认分配；重复预设循环为静态证据] | - | 未处理项不适用；未接受风险 | 确定每块容量与有界溢出策略、复用预分配通知存储；覆盖合法密集事件及消息线程尚未drain的重复循环。 |
| known-issues §2/Phase 6-2 播放速度控制 | 线程安全 | 活动变速/Stop 在消息线程改写音频游标 | P1 | 未处理 | 已修复项新游标反证 | 原子倍率并未保护 playbackEventIndex/hasRenderedPlaybackBlock/loopWrapPending；已进入render的callback不会被 stopped 原子写等待。 | source/Recording/RecordingEngine.cpp:409-442,519-566,577-590,694-704；source/Recording/RecordingSessionController.cpp:440-443,594-600；known-issues §2 Phase 6-2 播放速度控制；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 发布 transport 命令，在音频块边界一致应用倍率、位置和游标；结构性停止复用停机守卫；补真实双线程回归。 |
| THR-002 | 线程安全 | 取消/析构 WAV 任务可能强制终止工作线程 | P1 | 未处理 | 审计 | stopThread(3000) 超时可进入 Windows TerminateThread；writer/stream/plugin 栈析构被跳过，句柄/文件收尾风险。 | source/Export/WavExportTask.cpp:74-87,163-166,169-188；JUCE Thread.cpp:250-274 与 Windows thread kill API 证据；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 仅协作取消，异步等待实际工作线程退出后再释放任务/插件/文件所有权；验证慢 processBlock 的取消和退出。 |
| ERR-002 | 错误处理/持久化 | 即时同步保存未取代旧防抖快照 | P1 | 未处理 | 审计 | 新扫描缓存/设置同步落盘后，旧timer仍可整份回写旧值，破坏增量崩溃安全及新恢复信息。 | source/Settings/SettingsStore.cpp:136-148,439-450；source/Plugin/PluginOperationController.cpp:21-23,150-153；S12；[Windows 独立设置文件冒烟确认 new-cache 被旧timer回滚成 old-cache] | - | 未处理项不适用；未接受风险 | 同步 save 成功后取代/撤销相同store的待写payload；定义直接保存与防抖保存的顺序语义。 |
| QUAL-001 | 质量/功能 | 播放移调/掩码变化不锁定已发音身份 | P1 | 未处理 | 审计 | NoteOn60 后将offset改成+1，NoteOff发61；原60仍亮/发声到后续panic。enabled/mask切换同样存在身份不一致。 | source/Audio/AudioEngine.cpp:455-483；source/MainComponent.cpp:163-174；S01；[Windows AudioEngine/RecordingEngine 冒烟确认原音未释放] | - | 未处理项不适用；未接受风险 | 播放侧也保存 NoteOn 最终输出身份，NoteOff按原身份；定义重叠同音及变化时正在发声的处理，不用当前映射重算。 |
| QUAL-002 | 质量/功能 | 重复物理键同音在首个松键时被提前关闭 | P1 | 未处理 | 审计 | 默认 Q/K 都为Ch1 MIDI72，松Q时K仍held却pitch72已off；K自动重复被抑制，不能自动补响。 | source/Core/KeyMapTypes.h 默认布局；source/Input/KeyboardMidiMapper.cpp:267-291；S05；[Windows 真实 MidiKeyboardState 冒烟确认] | - | 未处理项不适用；未接受风险 | 在最终发音身份层维护重叠持有者，最后释放再NoteOff；保留每个物理键原身份。 |
| QUAL-003 | 质量/功能 | 暂停录制丢掉期间唯一的 NoteOff/踏板释放 | P1 | 未处理 | 审计 | 暂停前记录On、暂停期间真实松键Off不采集，保留Take出现孤儿On；导出的MIDI也无法配对。 | source/Recording/RecordingEngine.cpp:176-197,391-406；source/Audio/AudioEngine.cpp:436-444；S16；[Windows 捕获状态机冒烟确认 on=1/off=0] | - | 未处理项不适用；未接受风险 | 在冻结捕获时间轴的边界补齐已录身份/踏板终结状态；暂停期间排除新演奏，但保证保留Take配对。 |
| QUAL-004 | 质量/功能 | 精确播放末尾的 NoteOff 未在音频路径交付 | P1 | 未处理 | 审计 | length=128 且Off@128时半开首块排除Off，advance随即停止，下块再不渲染Off，依赖30Hz UI后续panic。 | source/Recording/RecordingEngine.cpp:543-545,611-617；source/Recording/RenderPipeline.cpp:40-47；S17；[Windows 终端事件冒烟确认 ended=1、off=0] | - | 未处理项不适用；未接受风险 | 播放长度包含最后事件或在音频边界明确终结；对齐实时/离线可听结束和最终NoteOff采样点。 |
| QUAL-005 | 质量/功能 | 原生加载保留乱序事件但播放器假定有序 | P1 | 未处理 | 审计 | 未来Off排在On@0前面时加载成功但On永不交付；seek lower_bound 同样依赖有序。 | source/Recording/PerformanceFile.cpp:225-235；source/Recording/RecordingEngine.cpp:535-545,694-704；S18；[Windows 原生往返后播放冒烟确认 admitted=1/on=0] | - | 未处理项不适用；未接受风险 | 原生文件准入拒绝或稳定规范化非单调时间线；保留同采样语义顺序并测试真正播放/seek。 |
| ERR-003 | 错误处理/持久化 | 接受并序列化的 keyUp 绑定没有执行入口 | P1 | 未处理 | 审计 | down时trigger不匹配不进入heldKeys，up时wasHeld仍false，完整按/松周期无声且无反馈。 | source/Input/KeyboardMidiMapper.cpp:267-280,324-328；source/Layout/PerformancePreset.cpp:45-46；source/Core/KeyMapTypes.h:15-17,161-177；S26；[Windows 完整按下/释放周期确认未处理、未发音] | - | 未处理项不适用；未接受风险 | 兑现公开keyUp触发的可配对事件语义或在准入明确拒绝；不能接受文件配置后静默丢弃。 |
| QUAL-018 | 质量/功能 | 设备采样率变更未重基准活动播放/录制时间域 | P1 | 未处理 | 审计 | 设置允许活动时48k→44.1k；prepare仅更改AudioEngine及blockSize，播放ratio和Take采样率不更新，余下播放变慢/录制时长变形。 | source/Settings/SettingsComponent.cpp:149-156；source/Audio/AudioEngine.cpp:57-62,436-444；source/Recording/RecordingEngine.cpp:149-156,225-228；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 设备prepare时统一重基准活动Transport；录制按固定Take域换算，或明确先结束会话；验证位置/倍速/NoteOff连续性。 |
| TEST-001 | 测试 | 音频fixture依赖可选 NRVO 保持自引用指针 | P1 | 未处理 | 审计 | makeBlock返回具名pair，移动后info.buffer不会重绑定；C++17不保证NRVO，关闭可选NRVO后可能向callback传悬垂栈指针。 | source/tests/AudioEngineTest.cpp:28-46；Microsoft /Zc:nrvo文档；S24；[Windows /Zc:nrvo- 冒烟确认info.buffer不指向调用者live buffer；未解引用悬垂指针制造崩溃] | - | 未处理项不适用；未接受风险 | 由调用者从live buffer构造info，或fixture移动显式重绑定；用合法禁NRVO配置验证指针与实际音频行为。 |
| TEST-002 | 测试 | 默认路径测试触碰真实用户日志/预设目录 | P1 | 未处理 | 审计 | DiagnosticsTest构造生产logger可截减用户既有>512KiB日志并追加banner；Preset目录probe也创建真实用户目录。 | source/tests/DiagnosticsTest.cpp:17-21；source/Diagnostics/DevPianoLogger.cpp:5-16；source/tests/PerformancePresetTest.cpp:274-278；FileLogger构造裁剪契约；[测试调用链静态确认；未用用户真实日志做破坏性重现] | - | 未处理项不适用；未接受风险 | 删除只测incidental默认路径的探针或置于隔离profile；全部文件测试使用已有ScopedTempDir，不再修改用户诊断历史。 |
| AUDIT-002 THR-003 | 线程安全 | 音频线程 MIDI Listener 同步进入 UI/Timer | P1 | 未处理 | known-issues §1 原问题引用 | Main断言消息线程但播放callback同步通知；CustomKeyboard启动Timer。原子颜色数组已存在，不能声称再原子化即修复。 | docs/issues/known-issues.md:58-64；source/Audio/AudioEngine.cpp:118,491；source/MainComponent.cpp:597-601；source/UI/CustomKeyboard.cpp:653-666；[既有确认问题；本轮静态复核，不重新编号] | - | 未处理项不适用；未接受风险 | 实时Listener仅有界快照/通知，消息线程处理UI和Timer；覆盖电脑/鼠标/回放/失焦。 |
| known-issues ERR-002 | 性能/实时性 | 异常插件缓冲尺寸仍保留重分配兜底 | P1 | 未处理 | known-issues 原编号引用 | 超过协商预分配几何时callback setSize；计数/异步日志只降低I/O影响，不使异常帧零分配。 | docs/issues/known-issues.md:66-77（ERR-002）；source/Audio/AudioEngine.cpp:133-143；[既有风险；本轮未触发异常驱动块] | - | 未处理项不适用；未接受风险 | 先明确定义设备/插件异常几何的安全处理并保持观测计数；对目标声卡热插拔验证，不把正常块plugin_resize=0当异常已修。 |
| QUAL-006 | 质量/功能 | Phase35字段在 SettingsModel 深拷贝中遗漏 | P2 | 未处理 | 审计 | 防抖快照将173 BPM/启用/count-in/动态力度等还原为默认或旧值；正常退出直接保存不消除中途快照错误。 | source/Settings/SettingsModel.h:135-143,232-283；source/Settings/SettingsStore.cpp:136-148；S11；[Windows store+timer 落盘/读取冒烟确认] | - | 未处理项不适用；未接受风险 | 补齐全部持久化字段的复制语义，同时保留 XML 独立所有权；验证首次及再次防抖写出的真实值。 |
| QUAL-007 | 质量/功能 | 鼠标输入通道被观察到的输出通道反向污染 | P2 | 未处理 | 审计 | 同一C4在Ch1→2、Ch2→3矩阵下两次点击分别发Ch2/Ch3；回放也能改随后鼠标路由。 | source/UI/CustomKeyboard.cpp:544-546,596-598,653-660；source/MainComponent.cpp:377-395；S06；[Windows 真实 CustomKeyboard+MidiChannelMapper 冒烟确认] | - | 未处理项不适用；未接受风险 | 分开配置输入身份与显示用输出通道；鼠标始终从当前映射输入身份触发并保存最终输出。 |
| ARCH-001 | 架构 | 两张演奏映射看板未共同消费最终映射投影 | P2 | 未处理 | 审计 | 实际A→Ch4/C5，QWERTY仍Ch1/C4，钢琴自行按原始binding反向匹配；违反Live Performance Map单一事实源。 | source/Input/KeyboardMidiMapper.cpp:434-447；source/UI/CustomKeyboard.cpp:249-293；source/MainComponentJiveAccessors.cpp:545-550,650-667；S10；[Windows 矩阵映射/快照冒烟确认；钢琴分支静态证据] | - | 未处理项不适用；未接受风险 | 映射层输出两个视图共享的最终只读投影；同时明确点击输入身份，避免展示修复后再次矩阵变换。 |
| QUAL-008 | 质量/功能 | 几何重建清空钢琴绑定标签 | P2 | 未处理 | 审计 | 先setKeyboardLayout再setKeyboardSettings即标签31→0，resize/viewport变化也丢标签。 | source/UI/CustomKeyboard.cpp:280-303；source/MainComponent.cpp:1214-1217；S07；[Windows 真实 CustomKeyboard 冒烟确认] | - | 未处理项不适用；未接受风险 | 几何重建保留或重新消费已有映射视图标签，不把标签仅存于一次临时KeyRenderState赋值。 |
| QUAL-009 | 质量/功能 | 静音绑定在 Shift/QWERTY 鼠标入口变为满力度 | P2 | 未处理 | 审计 | 物理trigger保留rawVelocity=0，快照却transformVelocity(0)=1，鼠标使用该值发声；物理与鼠标优先级不同。 | source/Input/KeyboardMidiMapper.cpp:366,442；source/UI/QwertyComponent.cpp:347；source/MainComponent.cpp:417-431；S09；[Windows 快照冒烟确认0→1；鼠标发声消费端静态确认] | - | 未处理项不适用；未接受风险 | 复用同一静音优先级规则生成快照和点击事件；验证最终MIDI而不仅held.velocity。 |
| QUAL-010 | 质量/功能 | fadeSpeed=1 合法端点不衰减且计时器不停止 | P2 | 未处理 | 审计 | 设置UI允许1.00，松键120帧后fade仍1；加载器还允许>1，导致非收缩动画/越界alpha风险。 | source/Settings/SettingsComponent.cpp:243-250；source/UI/CustomKeyboard.cpp:617-641；source/Settings/SettingsStore.cpp:237-239；source/Layout/PerformancePreset.cpp:418；S08；[Windows 1.00端点冒烟确认；>1影响为静态证据] | - | 未处理项不适用；未接受风险 | 统一UI/加载器的收缩系数范围，或为端点定义显式可终止动画；验证停止和有界alpha。 |
| QUAL-012 | 质量/功能 | 实时圆角样式路径落后一版 | P2 | 未处理 | 审计 | setBorderRadii先用旧值重建shape再赋新值；radius30角alpha255，改回0反而alpha0，未resize即显示反向。 | source/UI/jive/core/jive_BackgroundCanvas.cpp:87-93,158-173；source/UI/jive/core/jive_StyleSheet.cpp applyStyles；S22；[Windows 真实组件绘制与像素检查确认] | - | 未处理项不适用；未接受风险 | 先更新radii再重建路径；在不改变bounds情况下连续改两次半径，验证真实角像素/路径。 |
| QUAL-013 | 质量/功能 | 歌曲信息 Notes 继承只读 ListEditor | P2 | 未处理 | 审计 | 生产factory设置readOnly=true，Metadata初始化未恢复可写；普通按键不改变Notes。 | source/UI/ViewHost.cpp:58-67；source/UI/jive/JiveModalDialog.cpp:286-298,439-444；source/tests/JiveModalDialogTest.cpp:264-289；S23；[Windows 生产 ViewHost 工厂冒烟确认 readonly=1、text不变] | - | 未处理项不适用；未接受风险 | 仅元数据Notes使用可编辑工厂/显式恢复输入能力，保留诊断列表只读；测试走生产ViewHost并注入用户键入。 |
| QUAL-014 | 质量/功能 | 离线插件实例未声明 nonRealtime 模式 | P2 | 未处理 | 审计 | 独立导出实例默认仍向VST3传kRealtime，依赖offline分支的质量/流式等待行为不会启用。 | source/Recording/PluginOfflineRenderer.cpp:41-60；本地JUCE AudioProcessor.h默认false及VST3 setup/process mode消费点；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | prepare前setNonRealtime(true)，保证setup和process一致；选择依赖offline模式的真实VST3对照验证。 |
| QUAL-015 | 质量/功能 | 再次拖入已经发现的 VST3 被误判为没有类型 | P2 | 未处理 | 审计 | addType重复身份返回false但仍有有效description；返回names仅包含新增项，导致扫描后unload再拖入无法加载。 | source/Plugin/PluginHost.cpp:181-190；source/Plugin/PluginOperationController.cpp:87-97；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 分离探测到的有效类型与是否新增列表条目，重复文件也返回可加载身份并保留metadata更新。 |
| ARCH-002 | 架构 | 插件选择/恢复以显示名代替 description 身份 | P2 | 未处理 | 审计 | 同名不同文件/ID的插件在列表去重且load首条匹配，另一插件不可选；过滤乐器时也可能载入同名效果。 | source/Plugin/PluginHost.cpp:getKnownPluginNames/getInstrumentPluginNames/getEffectPluginNames,296-300；source/MainComponentJiveAccessors.cpp:75-78；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 贯穿选择/加载/持久化稳定description身份，显示名仅展示；验证同名不同ID及乐器/效果过滤。 |
| ARCH-003 | 架构 | 录制预设事件用可变目录索引作为永久身份 | P2 | 未处理 | 审计 | [A,B]中B索引1，新增AA后索引1变AA；保存演奏回放到错误预设；uint8也会截断大索引。 | source/Layout/PresetFlowSupport.cpp:88-113；source/Layout/PerformancePreset.cpp:579-580；source/Recording/RecordingEngine.h:17-37；source/MainComponent.cpp:659-661；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 保存稳定预设身份/Take内映射或快照并定义缺失行为；迁移格式时不得静默重解释旧数字。 |
| QUAL-016 | 质量/功能 | 启动恢复预设未设置控制器当前身份 | P2 | 未处理 | 审计 | 启动applyPresetData只写Settings.lastActivePresetId；currentPresetId仍空，combo回退首项，随后绑定编辑autoSave直接失败，重启丢改动。 | source/MainComponent.cpp:145-153,501-537；source/Layout/PresetFlowSupport.cpp:99-117,163-164,203-209；source/MainComponentJiveAccessors.cpp:373-385；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 统一启动/用户选择的预设激活操作及当前身份，保证列表选择、自动保存和运行布局一致。 |
| ARCH-004 | 架构 | 预设事件丢失可执行时序和离线语义 | P2 | 未处理 | 审计 | 实时事件只传presetId等30Hz UI处理，块内后续音符用旧参数，末块queue可被Stop清掉；离线RenderEvent只复制默认MIDI，整段未执行声学预设变化。 | source/Recording/RecordingEngine.cpp:547-549,409-413；source/MainComponent.cpp:606-607,659-662；source/Recording/RenderPipeline.cpp:28-31；source/Recording/WavFileExporter.cpp:129-145；source/Recording/PluginOfflineRenderer.cpp:151-168；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 保留事件variant与准备好的声学快照，按采样偏移执行实时/离线同构语义，UI通知独立且不丢末块。 |
| QUAL-017 | 质量/功能 | Seek/循环回跳未恢复目的位置控制器及音色状态 | P2 | 未处理 | 审计 | panic释放踏板后只lower_bound游标，A前program/CC/pitch状态不chase；循环内后来的program可污染下一轮A前音符。 | source/Recording/RecordingEngine.cpp:335-340,478-507,569-574,694-704；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 在目标音符前恢复目的位置的状态快照，保持不自动重发历史NoteOn的现有策略；测试program/bank/CC64及pitch。 |
| FIX-035 | 质量/功能 | 预备拍在最后一拍起音而非下一下拍完成 | P2 | 未处理 | 已修复项时长反证 | 120BPM/4/4一小节按第4个click在1.5s开始，而非2s；跨多拍的sequence变化只减1，UI延迟会改变倒计时。 | source/Recording/RecordingSessionController.cpp:55-62,802-815；source/Audio/MetronomeProcessor.h:154-163,219-221；known-issues §2 FIX-035；[静态确认；运行触发未验证] | - | 未处理项不适用；未接受风险 | 以完整音频节拍时段/下一个目标downbeat完成并消费序号差；实际控制器验证一/两小节及跨多拍poll。 |
| ERR-004 | 错误处理/持久化 | MIDI宽容尾字节也误接收缺失/截断轨 | P2 | 未处理 | 审计 | readFrom失败后只要任一既有轨有事件就成功，2轨声明实际仅1轨仍报告导入成功。 | source/Recording/MidiFileImporter.cpp:16-23,33-40；JUCE MidiFile.cpp:385-412；S21；[Windows 缺失第二MTrk文件冒烟确认 import_success=1/trackCount=1] | - | 未处理项不适用；未接受风险 | 仅完整声明结构后额外后缀允许宽容，缺失/短chunk应拒绝且保留现有Take；保留真实CRLF后缀兼容。 |
| RES-001 | 资源 | 日志大小上限仅构造时截减而非会话滚动 | P2 | 未处理 | 审计 | maxInitialFileSizeBytes不会限制后续logMessage；配置1024字节后同一会话写出8465字节，长期日志可无限增长。 | source/Diagnostics/DevPianoLogger.cpp:10-18,47-59；source/Diagnostics/DevPianoLogger.h:8-16；docs/reference/architecture.md:288；官方FileLogger构造契约；S04；[Windows 自有临时日志冒烟确认增长] | - | 未处理项不适用；未接受风险 | 实现可观测的会话内有界轮转，或明确真实只在启动裁剪的契约与风险；验证长会话及轮转故障。 |
| CMPL-001 | 决策合规 | 业务头 WindowIconUtils 重新引入 JuceHeader | P2 | 未处理 | 审计 | 违反ADR-012所有source头禁聚合头的决策本体，增加全量模块传递依赖。 | source/UI/WindowIconUtils.h:3；source/Main.cpp:10；docs/decisions/ADR-012-header-iwyu-and-granular-include-discipline.md:19-40；[精确grep及实际应用消费者确认] | - | 未处理项不适用；未接受风险 | 以实际需要的细粒度模块头替换并检查消费者；不使用测试头例外为业务头开豁免。 |
| CMPL-002 | 决策合规 | 声明式业务仍使用 raw GuiItem 逃逸接口 | P2 | 未处理 | 审计 | Main热重载直接rootItem->state，内置modal保留raw回调；ViewHost advanced接口与ADR-014 strict facade冲突，不宣称UAF。 | source/MainComponent.cpp:862-865；source/UI/ViewHost.h:122-126；source/UI/jive/JiveModalDialog.h:39-49；source/UI/jive/JiveModalDialog.cpp:286-312；ADR-014:113-128；[决策与源码逐条核对] | - | 未处理项不适用；未接受风险 | 将业务样式刷新/内置modal初始化封装在ViewHost边界内；如确需例外则另行明确批准决策，而非保留无约束逃逸。 |
| ENG-001 | 工程化 | 编译零警告及全量 tidy 清零门禁不成立 | P2 | 未处理 | 审计 | WSL Debug build有20次warning/2个源位点；全量144cpp tidy以5处项目诊断exit1，格式/测试通过不能替代这些门禁。 | Audio/MetronomeProcessor.h:183；tests/MetronomeTest.cpp:200；Recording/MidiTextDecoder.cpp:200,211,289,297；tests/PerformanceModifierTest.cpp:220；§4.1输出；[本轮命令输出确认] | - | 未处理项不适用；未接受风险 | 逐项评估编译/静态诊断并小步修正，必要规则争议如实记录；禁止--fix自动改源码或压制未知风险。 |
| AUDIT-002 TEST-014 | 测试 | 和弦识别7个子测试被默认类别过滤漏跑 | P2 | 未处理 | 历史类别门禁问题重开 | ChordRecognitionTest类别为小写devpiano，不满足DevPiano/白名单；默认门禁95套件未含它，额外定向运行115断言通过仍未修默认缺口。 | source/tests/ChordRecognitionTest.cpp:8-19；source/tests/TestRunner.cpp:141-146；CMakeLists.txt:588,629-631；§4.1定向Windows category结果；[默认日志及Windows --category devpiano 执行确认] | - | 未处理项不适用；未接受风险 | 迁入既有DevPiano/Core类别并确认默认日志包含7个子测试；区分编译接入和执行接入。 |
| known-issues §1/节拍器每拍三角函数与全回调零三角 SLA 不一致 | 性能/实时性 | 全回调零三角函数 SLA 尚未达到 | P2 | 未处理 | known-issues 原heading，扩充回调闭包证据 | 已知每拍Metronome sin/cos/exp；另有Piano机械瞬态逐采样sin与起音系数计算。Magic Circle分音循环不等于完整callback零sin。 | docs/issues/known-issues.md:52-56；source/Audio/MetronomeProcessor.h:209-216；source/Audio/PianoSynthVoice.h:1081-1107,1232-1272,1426-1433；SineSynthVoice.h:104；[源码调用确认；当前CPU/声卡毛刺未测] | - | 未处理项不适用；未接受风险 | 按完整调用闭包界定/验证SLA；优先预计算或递归机械振荡，保持听感及踏板语义；CPU期限效果另测。 |
| known-issues §1/A4 基准音高范围与项目契约不一致 | 质量/功能 | A4实际410..450Hz未覆盖400..480Hz契约 | P2 | 未处理 | known-issues 原heading引用 | 设置/预设/离线统一钳制到较窄范围，两端目标频率无法选择。 | docs/issues/known-issues.md:46-50；source/Audio/TemperamentEngine.h:34-48；SettingsComponent使用同常量；[既有确认问题；本轮静态复核，不重新编号] | - | 未处理项不适用；未接受风险 | 以原已知项统一修正调律引擎、设置、预设与导出边界并测试两端；此前文档标注保持真实。 |
| OBS-001 | 可观测性 | MIDI诊断将已是0..127的力度再次乘127 | P2 | 未处理 | 审计 | raw_velocity=64显示成vel=8128，错误日志会误导击键动态/音色排障；现有测试未断言力度数值。 | source/Diagnostics/MidiTrace.cpp:26-30；source/tests/DiagnosticsTest.cpp:68-76；S25；[Windows describeMidiMessage真实调用输出确认] | - | 未处理项不适用；未接受风险 | 直接展示原始整数力度或正确使用getFloatVelocity换算；验证边界及中间值，不钉完整自然语言日志。 |
| QUAL-019 | 质量/功能 | 踩下柔音踏板后新分配声部不继承CC67状态 | P2 | 未处理 | 审计 | 相同PianoSynthSound/8voice拓扑下先CC67再两音，仅1/2活跃声部为soft pedal；和弦一部分不执行Una Corda，区别于触键曲线。 | source/Audio/AudioEngine.cpp:261-267；source/Audio/PianoSynthVoice.h:118-129,188-193,203-239,485-490；JUCE Synthesiser.cpp:340-349,424-428；S29；[Windows 当前PianoSynthVoice+真实JUCE Synthesiser拓扑冒烟确认；主窗口演奏听感未测] | - | 未处理项不适用；未接受风险 | 由乐器拥有者维护当前踏板状态，保证每个新起声部继承；保留VST3通道语义并验证踏板先于和弦、换声部及释放。 |
| QUAL-011 | 质量/功能 | MIDI 1至11的八度标注高一组 | P3 | 未处理 | 审计 | 整数向零截断将C#-1…B-1显示成C#0…B0；编辑器/卡片接受0..127，不仅88键物理键盘范围。 | source/Core/MusicTheory.h:28-55；source/Input/KeyboardMidiMapper.cpp:444-447；source/UI/KeyBindingEditDialog.cpp:103；S27；[Windows 真实标签函数边界输出确认] | - | 未处理项不适用；未接受风险 | 使用等价数学floor的MIDI八度换算，覆盖0/1/11/12边界和唱名偏移。 |
| TEST-003 | 测试 | 测试存在译文/自证断言及未调用行为用例 | P3 | 未处理 | 审计 | UTF8转义仍硬钉中文文案；若干手动赋值后读取自身不约束生产，Metronome lifecycle私有方法未被runTest调度。 | source/tests/StyleCatalogTest.cpp:1679-1682,1472-1494；source/tests/JiveModalDialogTest.cpp:337-372；source/tests/SettingsLayoutModelTest.cpp:126-136；source/tests/MetronomeTest.cpp:15-28,384-456；AGENTS.md §6；[逐段测试审查及runTest调用清单确认] | - | 未处理项不适用；未接受风险 | 删除copy-pinning/自证测试，不重新钉新文本/数值；保留语言机制和生产组件行为；接入确定性的真实生命周期用例。 |
| DOC-001 | 文档/契约 | 现行行为说明含已被源码证伪的承诺 | P3 | 未处理 | 审计 | 预设文档称切换调号/重命名确认，MIDI导出描述两轨/拍号名称；实现分别是app-global调号、无rename冲突确认、单轨仅tempo；日志滚动已关联RES。 | docs/reference/features/performance-presets.md:11-17,145-162 对 PresetFlowSupport.cpp:131-134,302-330；docs/reference/features/recording-playback.md:101-104 对 MidiFileExporter.cpp:25-45；docs/reference/features/plugin-hosting.md 重扫/控制器测试说明；[现行文档与实际消费者对照] | - | 未处理项不适用；未接受风险 | 修复实现后按真实契约同步功能/手工验收；事实描述不另开CMPL；本轮不改任何既有文档。 |

### ID 命名与领域前缀

| 前缀 | 领域 |
| --- | --- |
| `SEC` | 安全（缓冲区、文件路径、插件加载、MIDI 消息有效性、JSON 解析） |
| `RES` | 资源（内存泄漏、句柄泄漏、分配热点） |
| `PERF` | 性能（实时路径分配、容器策略） |
| `ARCH` | 架构（模块边界、依赖方向、职责切分） |
| `QUAL` | 代码质量（命名、const、RAII、死代码、重复） |
| `ERR` | 错误处理（返回值忽略、静默失败、Logger 覆盖） |
| `THR` | 线程安全（消息线程、音频回调、数据竞争） |
| `OBS` | 可观测性（日志完整性、diagnostics 覆盖） |
| `TEST` | 测试（覆盖缺口、测试质量、可维护性） |
| `DOC` | 文档（源码注释、架构文档一致性） |
| `ENG` | 工程化（CMake、clang-tidy、clang-format、警告） |
| `CMPL` | 决策合规（ADR 决策被违反/部分遵守；ADR 事实性描述过时属修正原文，不开问题） |

### 去重与未升级结论

- Listener、ERR-002、A4、零三角SLA及Phase6-2/FIX-035保留原编号/heading。同根不重编号；裸实例只重开具体绕过guard，不否定正常guard。
- AUDIT-003 Core逆向include、room reverb、解码scratch优化和headless socket无新反证；32MiB MIDI上限、128固定数组、find观察指针和事件驱动扫描仍合理。
- Chord补跑通过不关闭默认漏项；cpp编译接入不证明case执行。controller helper/Notes假factory作为对应功能回归要求，不另开泛化零覆盖。
- 未证实的Type2拒绝政策、bank/program排序、任意动态em/transition、回调任意自毁、插件沙箱等不作确认漏洞。历史ADR规模变化不因数值过时开CMPL。

### 官方API事实来源（结合本地版本核对）

- [MidiMessageCollector](https://docs.juce.com/master/classjuce_1_1MidiMessageCollector.html)、[Synthesiser](https://docs.juce.com/master/classjuce_1_1Synthesiser.html)、[MidiKeyboardState](https://docs.juce.com/master/classjuce_1_1MidiKeyboardState.html)：线程安全不等于lock-free，具体锁/Listener线程核对本地实现。
- [MemoryBlock](https://docs.juce.com/master/classjuce_1_1MemoryBlock.html)：JUCE长度前缀编码非标准Base64。
- [FileOutputStream](https://docs.juce.com/master/classjuce_1_1FileOutputStream.html)、[TemporaryFile](https://docs.juce.com/master/classjuce_1_1TemporaryFile.html)：已有文件位置与替换边界。
- [FileLogger](https://docs.juce.com/master/classjuce_1_1FileLogger.html)：初始尺寸仅构造检查。
- [AudioProcessor](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html)：setNonRealtime告知离线模式。
- [Microsoft /Zc:nrvo](https://learn.microsoft.com/en-us/cpp/build/reference/zc-nrvo)：可选NRVO与强制prvalue消除不同。

---

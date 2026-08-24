# ADR-011: 现代 C++ 构建流水线深度优化与编译器缓存规范

## 状态

**已接受**（2026-08-23 实施）

## 背景

`devpiano` 是一款以 JUCE 8 为框架、包含物理建模 DSP、JIVE 声明式 UI 与内嵌二进制资产的现代 C++ 桌面应用。随着项目功能演进，构建体系面临以下瓶颈与痛点：

1. **Windows MSVC 与 `sccache` 兼容性故障**：默认 CMake MSVC Debug 配置注入 `/Zi`（共享单个 `.pdb` 文件），导致 `sccache` 无法缓存对象文件，且并发编译时多个 `cl.exe` 进程争抢写入 `.pdb` 文件锁触发 `fatal error C1041`；
2. **JUCE 框架自动注入 `/Zi` 冲突**：JUCE 子模块内置 `cmake_minimum_required(VERSION 3.15)` 重置了 CMake 策略（CMP0141 变为 OLD），导致 JUCE 自动向接口属性注入 `/Zi`，覆盖顶层编译选项并触发 `D9025` 告警；
3. **重复头文件解析开销**：每个编译单元重复展开包含 C++ STL 标准库与模板元，造成大量 CPU 算力浪费；
4. **链接阶段瓶颈**：传统 GNU `ld.bfd` 为单线程/弱并发链接器，链接数十个静态库与目标文件耗时长达数秒；
5. **编译耗时黑盒**：缺乏定量的微观编译耗时剖析工具，无法精准识别重度模板与低效头文件。

## 决策

为了实现跨平台（WSL Clang + Windows MSVC + GitHub Actions CI）秒级构建与全并发无锁缓存，制定以下四项工程规范：

### 1. MSVC 全局 `/Z7` (Embedded) 调试符号与 `/FS` 并发防护
- **首选路径（CMake 3.25+ 官方抽象，2026-08 修订）**：`CMAKE_POLICY_DEFAULT_CMP0141 NEW` + `cmake_policy(SET CMP0141 NEW)` 强制所有子目录（JUCE, JIVE 等）继承 `CMP0141=NEW`，配合 `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT="$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"`（`CACHE STRING "" FORCE`）——用 CMake 为 `/Z7` 提供的一等抽象（`Embedded` = `/Z7`）而非手工 flag 替换，避免工具链或子项目在替换之后注入 `/Zi` 导致 `/Z7 ... /Zi` 后者覆盖前者（CMP0141 正是为此引入）；
- **兜底清洗（防御路径）**：显式清洗 `juce_recommended_config_flags` 接口选项与 `CMAKE_{C,CXX}_FLAGS_{DEBUG,RELWITHDEBINFO}` 中残留的 `/Zi`→`/Z7`，拦截历史/第三方注入路径；主路径为上述官方抽象；
- `/Z7` 将调试符号直接内嵌进 `.obj`（不产生共享 `.pdb`），使 `sccache`/`ccache` 可原子缓存对象文件、命中率 100%，并消除多 `cl.exe` 并发争抢 `.pdb` 文件锁的 `fatal error C1041`（业界共识：缓存工具无法可靠缓存共享 PDB 产物）；
- 全局追加 `/FS`（Force Synchronous PDB Writes）作为并发写防护兜底——`/Z7` 下已无 PDB 产生，保留为防御性双保险，成本可忽略。

### 2. STL PCH 预编译头隔离架构（`source/pch.h`）
- 建立专门的 `source/pch.h` 预编译头文件，预编译高频 C++ 标准库头文件（`<vector>`, `<string>`, `<algorithm>`, `<cmath>`, `<memory>`, `<atomic>`, `<tuple>`, `<unordered_map>` 等）；
- **严格禁止在 PCH 中预编译全局 `<JuceHeader.h>`**：因为 JUCE 框架模块内部的 `.cpp` 依赖严格的自包含顺序，预先包含 `<JuceHeader.h>` 会触发 JUCE 内部的 `#error "Incorrect use of JUCE cpp file"` 守卫。将 `<JuceHeader.h>` 排除在 PCH 之外，使得全项目（业务代码 + JUCE 模块 + JIVE）均能安全、零冲突地共享 STL PCH 编译加速。
- **PCH 与编译缓存协同（2026-08 补充，ccache 官方手册）**：Clang PCH 默认内嵌创建时间戳，ccache 缓存重放的 PCH 文件时间戳与原始不同，会触发 `file modified since precompiled header was built` 使 PCH 缓存全部失效——所有 Clang target（`devpiano` / `devpiano_tests`）必须注入 `-fno-pch-timestamp`（关闭时间戳校验，内容哈希校验仍生效）。若未来引入 GCC 构建须补 `-fpch-preprocess`；ccache `sloppiness=pch_defines` 仅 GCC PCH 需要，Clang 路径不启用（保持严格哈希与正确性）。

### 3. Linux/WSL 高速链接器自动探测与启用（`mold` / `lld`）
- 在 CMake 中配置链接器探测逻辑：在 Linux/WSL 环境下自动探测并优先启用 `mold`，次选 `lld`，将最终可执行文件链接时间从秒级压缩至数十毫秒。
- **CI 执行要求（2026-08 补充）**：Linux 门禁与发布 runner（`ubuntu-24.04`）必须同时安装 `mold` 与 `lld`（apt），使探测逻辑取到首选 mold；探测不到任何高速链接器而回退 `ld.bfd` 属于降级状态，应在 CI 日志中视为异常排查线索。WSL 本机推荐同样安装（quickstart 依赖清单含 `lld mold ccache`）：2026-08-25 实测安装 `mold` 后 configure 输出 `High-speed linker: mold enabled (/usr/bin/mold)`，Debug 链接切换至 mold（lld 为次选，mold 命中后无需安装）。

### 4. 编译耗时性能跟踪与火焰图工具链（`-ftime-trace`）
- 在 CMake 中提供 `ENABLE_TIME_TRACE` 选项，为 Clang 编译器注入 `-ftime-trace`；
- 在 `scripts/dev.sh` 中提供 `time-trace` 命令，搭配原生 Python 分析脚本 `scripts/analyze_build_time.py`，自动聚合最耗时 Translation Units、最耗时头文件、最耗时模板实例化排行，并导出兼容 Chrome Tracing / Perfetto 的 `combined_time_trace.json` 交互式火焰图。

## 影响与收益

- **构建速度飞跃**：
  - Windows CI 与本地 MSVC 构建全面实现 `sccache` 100% 缓存命中，彻底消除 C1041 锁死；
  - WSL 本地测试链接耗时降至毫秒级；
  - 增量编译单文件改动实现秒级响应。
- **可观测性**：通过 `time-trace` 成功捕获到 `std::basic_regex` 在编译期实例化 42 次耗时 30+ 秒的隐蔽性能瓶颈，为后续模板与代码优化提供数据支撑。

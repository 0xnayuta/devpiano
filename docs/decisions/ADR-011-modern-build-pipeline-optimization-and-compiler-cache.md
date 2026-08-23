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
- 在 `CMakeLists.txt` 第一行引入 `set(CMAKE_POLICY_DEFAULT_CMP0141 NEW)`，强制所有子目录（JUCE, JIVE 等）默认继承 `CMP0141=NEW`，阻止 JUCE 注入 `/Zi`；
- 在 `CMakeLists.txt` 中显式清洗 `juce_recommended_config_flags` 的接口编译选项，将残留 `/Zi` 替换为 `/Z7`；
- 全局将 Debug / RelWithDebInfo 下的 `/Zi` 替换为 `/Z7`（调试符号直接内嵌进 `.obj`），使 `sccache` 缓存命中率达到 100%；
- 全局追加 `/FS`（Force Synchronous PDB Writes）编译选项，彻底根治多进程并发文件锁冲突。

### 2. STL PCH 预编译头隔离架构（`source/pch.h`）
- 建立专门的 `source/pch.h` 预编译头文件，预编译高频 C++ 标准库头文件（`<vector>`, `<string>`, `<algorithm>`, `<cmath>`, `<memory>`, `<atomic>`, `<tuple>`, `<unordered_map>` 等）；
- **严格禁止在 PCH 中预编译全局 `<JuceHeader.h>`**：因为 JUCE 框架模块内部的 `.cpp` 依赖严格的自包含顺序，预先包含 `<JuceHeader.h>` 会触发 JUCE 内部的 `#error "Incorrect use of JUCE cpp file"` 守卫。将 `<JuceHeader.h>` 排除在 PCH 之外，使得全项目（业务代码 + JUCE 模块 + JIVE）均能安全、零冲突地共享 STL PCH 编译加速。

### 3. Linux/WSL 高速链接器自动探测与启用（`mold` / `lld`）
- 在 CMake 中配置链接器探测逻辑：在 Linux/WSL 环境下自动探测并优先启用 `mold`，次选 `lld`，将最终可执行文件链接时间从秒级压缩至数十毫秒。

### 4. 编译耗时性能跟踪与火焰图工具链（`-ftime-trace`）
- 在 CMake 中提供 `ENABLE_TIME_TRACE` 选项，为 Clang 编译器注入 `-ftime-trace`；
- 在 `scripts/dev.sh` 中提供 `time-trace` 命令，搭配原生 Python 分析脚本 `scripts/analyze_build_time.py`，自动聚合最耗时 Translation Units、最耗时头文件、最耗时模板实例化排行，并导出兼容 Chrome Tracing / Perfetto 的 `combined_time_trace.json` 交互式火焰图。

## 影响与收益

- **构建速度飞跃**：
  - Windows CI 与本地 MSVC 构建全面实现 `sccache` 100% 缓存命中，彻底消除 C1041 锁死；
  - WSL 本地测试链接耗时降至毫秒级；
  - 增量编译单文件改动实现秒级响应。
- **可观测性**：通过 `time-trace` 成功捕获到 `std::basic_regex` 在编译期实例化 42 次耗时 30+ 秒的隐蔽性能瓶颈，为后续模板与代码优化提供数据支撑。

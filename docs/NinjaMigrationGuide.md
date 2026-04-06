# CMake + Ninja 迁移指南（devpiano）

> 目标：将本项目从 **CMake + MSBuild(Visual Studio Generator)** 迁移到 **CMake + Ninja**，并保持现有 JUCE 依赖、编译选项、产物路径逻辑可用。

## 构建基线（统一）
- 配置：`cmake --preset ninja-x64`
- Debug 构建：`cmake --build --preset ninja-debug`
- Release 构建：`cmake --build --preset ninja-release`

---

## 1. 迁移结论（当前仓库状态）

已完成迁移：

- `CMakePresets.json` 已切换为 Ninja 预设：
  - `ninja-x64`（主构建）
  - `ninja-debug` / `ninja-release`
- README 构建命令已改为 Ninja 预设
- 里程碑/评估文档中的主验证命令已改为 Ninja
- 已实测通过：
  - `cmake --preset ninja-x64`
  - `cmake --build --preset ninja-debug`
  - `cmake --build --preset ninja-release`

---

## 2. 迁移后的标准命令

### 配置

```bash
cmake --preset ninja-x64
```

### 构建（Debug）

```bash
cmake --build --preset ninja-debug
```

### 构建（Release）

```bash
cmake --build --preset ninja-release
```

产物：

- `build/ninja-x64/devpiano_artefacts/Debug/DevPiano.exe`
- `build/ninja-x64/devpiano_artefacts/Release/DevPiano.exe`

---

## 3. 预设设计说明

### 为什么使用 Ninja Multi-Config

本项目此前习惯 `--config Debug/Release` 的多配置工作流。选择 `Ninja Multi-Config` 的好处：

- 与旧流程最接近（仍有 Debug/Release 配置）
- 不需要为每个配置维护独立 build 目录
- 迁移风险低

### 关键 cache 变量

- `CMAKE_EXPORT_COMPILE_COMMANDS=ON`
  - 便于 clangd/LSP 工具读取编译参数
- `CMAKE_INSTALL_PREFIX=${sourceDir}/build/install`
  - 保持安装目录语义稳定

---

## 4. 一次性迁移步骤（建议）

1. 拉取最新代码并确认子模块完整：

```bash
git submodule update --init --recursive
```

2. （可选）删除旧 VS 生成目录，避免误用：

```bash
# 根据需要执行
# rmdir /s /q Build\vs2026-x64
```

3. 用 Ninja 预设重新配置：

```bash
cmake --preset ninja-x64
```

4. 构建 Debug/Release：

```bash
cmake --build --preset ninja-debug
cmake --build --preset ninja-release
```

5. 运行与冒烟测试：

- 程序可启动
- 音频设备可初始化
- 插件扫描/加载基本路径可用
- 键盘输入与 MIDI 状态显示可用

---

## 5. 注意事项（非常重要）

### 5.1 工具链环境

在 Windows 下，Ninja 只是构建驱动，编译器仍来自 MSVC。请在以下环境运行：

- Visual Studio 2026 Developer Command Prompt
- 或已正确加载 MSVC 环境变量的终端

否则可能出现 `cl.exe` / Windows SDK 找不到的问题。

### 5.2 不要混用构建目录

- Ninja 构建目录：`build/ninja-x64`
- 旧 VS 构建目录：`build/vs2026-x64`

不要交叉复用中间产物，避免奇怪的增量编译问题。

### 5.3 JUCE 头文件与生成头

`juce_generate_juce_header(devpiano)` 仍然生效，Ninja 下同样会生成 JuceHeader。若出现头文件错误，优先检查：

- 是否执行过 `cmake --preset ninja-x64`
- 是否在正确的 build 目录中构建

### 5.4 插件/运行时行为

构建系统迁移不应改变运行时逻辑。若迁移后出现行为差异，优先排查：

- Debug/Release 宏差异
- 旧目录残留 DLL/资源干扰
- 插件扫描路径是否来自旧设置缓存

---

## 6. 回退策略（应急）

若团队短期需要回退：

1. 恢复旧 `CMakePresets.json`（Visual Studio Generator）
2. 使用旧目录重新配置：`build/vs2026-x64`
3. 仅作为临时应急，不建议长期双轨维护

---

## 7. 迁移后的推荐 CI/本地检查顺序

1. `cmake --preset ninja-x64`
2. `cmake --build --preset ninja-debug`
3. `cmake --build --preset ninja-release`
4. 启动 `DevPiano.exe` 做最小冒烟

---

如后续需要，我可以继续补一版：

- 基于 Ninja 的 GitHub Actions / Azure Pipelines 示例
- 统一 Debug/Release 产物打包脚本（含插件白名单或测试资源）

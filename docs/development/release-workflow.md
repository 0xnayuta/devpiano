# Release Workflow Checklist

> 用途：记录 devpiano 手工 release / tag / 打包流程。  
> 读者：准备发布版本的维护者。  
> 更新时机：发布步骤、产物命名、验证范围或版本策略变化时。

## 1. 发布原则

- WSL 主工作树是唯一源码编辑来源。
- 当前正式 release 产物仅面向 Windows x64。
- Windows release 产物由 Windows 镜像树执行 MSVC Release 构建、手工验证和最终打包。
- Linux 暂不作为正式 release 产物；WSL / Linux Release 构建仅用于开发验证或后续 Linux 发布准备。
- 正式发布才使用 Release 构建；日常开发仍默认使用 Debug。
- tag 使用 annotated tag，格式为 `vMAJOR.MINOR.PATCH`。
- tag 推送后不重写；若已发布版本有严重问题，发布新的 patch 版本修复。
- 发布文档只记录流程和该版本说明，不替代 [`../roadmap/roadmap.md`](../roadmap/roadmap.md) 的项目状态来源。

## 2. 平台策略

### 2.1 Windows：当前正式发布平台

当前版本线只承诺 Windows x64 发布包：

```text
DevPiano-vX.Y.Z-win-x64.zip
DevPiano-vX.Y.Z-win-x64.sha256
```

Windows 发布路径：

```text
WSL 主工作树
↓ sync
Windows 镜像树
↓ MSVC Release build
Windows 手工冒烟测试
↓
zip + sha256
```

对应命令：

```bash
./scripts/dev.sh win-build --release
```

### 2.2 Linux：暂不作为正式发布产物

Linux 暂不打正式 release 包，也不在 Windows 镜像树中构建。

如需验证 Linux 构建，可在 WSL / 原生 Linux / Linux CI 中执行 Release 构建，例如：

```bash
./scripts/dev.sh wsl-build --release
```

但该结果只视为开发验证或后续 Linux 发布准备。正式 Linux 发布前，需要补充原生 Linux 桌面环境下的 GUI、音频设备、MIDI、VST3 路径、文件打开保存和退出稳定性验证，并另行定义 Linux 打包格式。

## 3. 版本号规则

版本号采用 SemVer 风格：

```text
vMAJOR.MINOR.PATCH
```

示例：

```text
v0.6.0
v0.6.1
v1.0.0
```

发布前确认：

- Git tag 为 `vX.Y.Z`。
- `CMakeLists.txt` 中 `project(devpiano VERSION X.Y.Z)` 与 tag 对齐。
- release notes 中的版本号与 tag 对齐。

## 4. 发布前检查

在 WSL 主工作树执行：

```bash
git status --short
git log --oneline -5
```

确认：

- 当前分支正确。
- 工作树干净，或只有本次发布预期内的版本 / 文档变更。
- 没有误改 `JUCE/` 子模块内容。

运行环境自检和构建准备：

```bash
./scripts/dev.sh self-check
./scripts/dev.sh wsl-build --configure-only
```

执行 Windows Release 构建。当前正式 release 只使用 Windows 镜像树产物：

```bash
./scripts/dev.sh win-build --release
```

如怀疑 Windows Release 构建目录缓存异常，可显式重配：

```bash
./scripts/dev.sh win-build --release --reconfigure
```

## 5. Windows 手工冒烟测试

在 Windows 镜像树的 Release 构建产物中启动程序。产物路径以实际构建输出为准，通常位于：

```text
<WIN_MIRROR_DIR>\build-win-msvc-release\devpiano_artefacts\Release\DevPiano.exe
```

至少验证：

- 应用可启动，主窗口正常显示。
- 默认 fallback synth 可发声。
- 电脑键盘触发 note on / note off 正常，虚拟键盘显示联动正常。
- VST3 扫描、加载、卸载、editor 打开/关闭正常。
- 录制、回放、保存 `.devpiano`、打开 `.devpiano` 正常。
- 导入 `.mid` 并播放正常。
- 音频设备重建后首音无明显异常。
- 退出应用无明显崩溃或挂起。

如本版本修改了特定功能，还应执行对应专项测试文档中的相关回归项。

## 6. 打包约定

当前正式发布产物仅包含 Windows x64 zip：

```text
DevPiano-vX.Y.Z-win-x64.zip
DevPiano-vX.Y.Z-win-x64.sha256
```

zip 内容建议包含：

```text
DevPiano.exe
RELEASE-NOTES.txt 或同等说明
LICENSE / third-party notice（如适用）
运行时需要的默认 layout / preset / 资源文件（如适用）
```

如果当前版本没有额外运行时资源依赖，可以先只包含 `DevPiano.exe` 和简短说明文件。

生成校验值时，建议在 Windows 或 WSL 中对最终 zip 文件计算 SHA-256，并将结果保存到 `.sha256` 文件。

### 6.1 后续 package 脚本 TODO

当前阶段保持手工打包，不实现自动化脚本。后续若手工流程稳定、重复发布需求增加，可再考虑新增 package 脚本。

候选目标：

- 增加 `./scripts/dev.sh package --version vX.Y.Z` 或独立 `scripts/package_release.sh`。
- 默认只打包 Windows x64 正式产物。
- 从 Windows 镜像树 Release 输出目录收集 `DevPiano.exe`。
- 校验 `CMakeLists.txt` 版本、release notes 文件名和传入版本号一致。
- 生成 `dist/vX.Y.Z/DevPiano-vX.Y.Z-win-x64.zip`。
- 生成对应 `.sha256` 文件。
- 明确不创建 git tag、不 push、不上传 GitHub/GitLab Release。
- Linux 打包在正式支持 Linux release 前不纳入脚本默认路径。

## 7. Release notes 建议格式

每个版本准备一段简短说明：

```markdown
## DevPiano vX.Y.Z

### Added
- ...

### Changed
- ...

### Fixed
- ...

### Known Issues
- ...
```

注意：

- 只描述本版本变化和已知问题。
- 不复制长期路线图内容。
- 不把未实现能力写成已实现能力。

## 8. Tag 与发布顺序

推荐顺序：

1. 确认工作树和版本号。
2. 准备 release notes。
3. 执行 Windows Release 构建。
4. 完成 Windows 手工冒烟测试。
5. 生成 zip 和 sha256。
6. 创建 annotated tag。
7. push commit 和 tag。
8. 在 GitHub / GitLab Release 页面上传 zip、sha256 和 release notes。

示例命令：

```bash
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin <branch>
git push origin vX.Y.Z
```

推送 tag 前可检查：

```bash
git show vX.Y.Z
```

## 9. 修复策略

- 已推送 tag 不重写、不移动。
- 如果发布后发现严重问题，修复后发布 patch 版本，例如 `v0.6.1`。
- 如 zip 打包错误但 tag 对应源码无误，可在 Release 页面替换附件，并在 release notes 中说明原因。
- 如源码 tag 本身错误，保留问题 tag，发布新的修正 tag。

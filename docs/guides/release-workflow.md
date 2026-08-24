# Release Workflow Checklist

> 用途：记录 devpiano 手工 release / tag / 打包流程。
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

#### 2.2.1 构建环境兼容性约束（2026-08 查证）

- **动态依赖**：Release 产物仅动态依赖 `libasound.so.2`（ALSA）、`libfontconfig.so.1`、`libfreetype.so.6` 与基础运行库（`libstdc++` / `libm` / `libgcc_s` / `libc`）。前三者 soname 多年稳定且为所有桌面发行版标配，**跨发行版字体/音频兼容性无需静态化处理**。
- **glibc / libstdc++ 门槛**：产物要求目标系统 glibc ≥ 构建环境。滚动发行版（Arch / CachyOS，glibc 2.44）构建的产物**仅 Arch 系可运行**；Ubuntu 22.04（2.35）、24.04（2.39）、Debian 12（2.36）、13（2.41）、Fedora 41（2.40）均无法运行。
- **正式 Linux 分发必须在保守环境构建**：推荐 Docker 容器（Ubuntu 22.04 LTS，glibc 2.35）执行 Release 构建，产物可覆盖全部常见桌面发行版。构建后检查门槛：

  ```bash
  readelf --version-info DevPiano | grep -o 'GLIBC_[0-9.]*' | sort -V | uniq | tail -1
  readelf --version-info DevPiano | grep -o 'GLIBCXX_[0-9.]*' | sort -V | uniq | tail -1
  ```

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

## 6. 打包流程

当前正式发布产物包含 Windows x64 zip 及 SHA256 校验文件：

```text
DevPiano-vX.Y.Z-win-x64.zip
DevPiano-vX.Y.Z-win-x64.sha256
```

zip 内容包含：

```text
DevPiano.exe
CHANGELOG.md
```

### 6.1 使用 dev.sh package 自动化打包

完成 Windows 手工冒烟测试后，在 WSL 主工作树中执行标准打包命令：

```bash
# 默认自动提取 CMakeLists.txt 中的版本并打包
./scripts/dev.sh package

# 或显式指定版本号
./scripts/dev.sh package --version 1.0.0
```

脚本会自动执行以下前置检查与流水线步骤：
1. **依赖检查**：检查 `wslpath`、`zip`、`sha256sum` 工具链；
2. **版本一致性校验**：确认 `CMakeLists.txt` 版本与 `CHANGELOG.md` 中对应版本条目存在；
3. **产物校验**：定位 Windows 镜像树下 `build-win-msvc-release/devpiano_artefacts/Release/DevPiano.exe`，若不存在则提示先构建；
4. **归档与压缩**：创建 `dist/vX.Y.Z` 目录，复制 `DevPiano.exe` 与 `CHANGELOG.md`，执行高压缩比 zip 打包；
5. **校验和生成**：计算并生成 `DevPiano-vX.Y.Z-win-x64.sha256`；
6. **输出摘要与引导**：打印产物清单、SHA256 校验值及后续 `git tag` 与 `gh release create` 推荐命令。

### 6.2 产物输出结构

默认输出位于 Windows 镜像目录的 `dist/vX.Y.Z/` 下（方便 Windows 侧直接归档与分发）：

```text
<WIN_MIRROR_DIR>\dist\vX.Y.Z\
├── DevPiano.exe
├── CHANGELOG.md
├── DevPiano-vX.Y.Z-win-x64.zip
└── DevPiano-vX.Y.Z-win-x64.sha256
```

可选高级参数：

```bash
# 指定输出到 WSL 本地仓库下的 dist/ 目录
./scripts/dev.sh package --local-dist

# 覆盖 Windows 镜像目录路径
./scripts/dev.sh package --win-mirror-dir 'D:\projects\devpiano'
```

### 6.3 Package 脚本设计约束

- **专注打包，不隐式提交/推送**：脚本仅负责收集产物、校验版本、生成 zip 和 sha256，**明确不创建 git tag、不 push、不自动创建 GitHub Release**（保持发布决策的人工把控）；
- **平台策略**：默认仅打包 Windows x64 正式 Release 产物；Linux 正式发布前不纳入默认路径；
- **独立与透传**：既可通过 `./scripts/dev.sh package` 统一入口调用，也可直接运行 `./scripts/package_release.sh`。

## 7. Release notes 格式

项目使用单文件 `CHANGELOG.md`，遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 格式：

```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [X.Y.Z] - YYYY-MM-DD

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

- 新版本条目追加在 `# Changelog` 标题下方、旧版本之上（最新版本在最顶部）。
- 只描述本版本变化和已知问题。
- 不复制长期路线图内容。
- 不把未实现能力写成已实现能力。

## 8. Tag 与发布顺序

推荐顺序：

1. 确认工作树和版本号（`CMakeLists.txt` 版本与 tag 对齐）；
2. 在 `CHANGELOG.md` 顶部追加新版本条目；
3. 提交版本变更并推送到 `main` 分支（触发 CI 质量门禁全绿验证）；
4. 创建 annotated tag 并推送到远程（**自动触发 GitHub Actions `.github/workflows/release.yml` 自动化发布流水线**）：
   ```bash
   git tag -a vX.Y.Z -m "Release vX.Y.Z"
   git push origin main
   git push origin vX.Y.Z
   ```
5. 推送后 GitHub Actions 自动完成：
   - Windows 原生 MSVC Release 纯净构建；
   - 自动打包 `DevPiano-vX.Y.Z-win-x64.zip` 与 `DevPiano-vX.Y.Z-win-x64.sha256`；
   - 自动创建 GitHub Release 并挂载分发包。

### 8.1 备用方案：本地手工打包与 CLI 发布

若需要在本地完成打包或进行离线分发，可执行本地流水线：

```bash
# 1. 本地执行打包
./scripts/dev.sh package --version X.Y.Z

# 2. 使用 GitHub CLI 上传（若已安装 gh）
gh release create "v${VERSION}" \
  --title "DevPiano v${VERSION}" \
  --notes-file "CHANGELOG.md" \
  "dist/v${VERSION}/DevPiano-v${VERSION}-win-x64.zip" \
  "dist/v${VERSION}/DevPiano-v${VERSION}-win-x64.sha256"
```
## 9. 修复策略

- 已推送 tag 不重写、不移动。
- 如果发布后发现严重问题，修复后发布 patch 版本，例如 `v0.6.1`。
- 如 zip 打包错误但 tag 对应源码无误，可在 Release 页面替换附件，并在 release notes 中说明原因。
- 如源码 tag 本身错误，保留问题 tag，发布新的修正 tag。

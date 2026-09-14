---
name: devpiano-release
description: devpiano 官方正式发布与打包工作流——智能对比上一个发布版本（tag）以来的 Git 增量提交，基于 Conventional Commits 自动推导推荐版本号（Major/Minor/Patch）、自动提炼发布信息并起草 Keep a Changelog 格式的 CHANGELOG 块；支持用户显式版本覆盖；编排工作树预检、CMakeLists/CHANGELOG 版本对齐、Windows MSVC Release 纯净构建、双平台冒烟验证阻断、dev.sh package 自动化打包与校验和、两步走安全推送（推 main 待 CI 绿灯再推 tag）、GitHub CLI 备用通道与容灾资产替换；未获确认严禁直接推送到远端。
---

# devpiano 智能增量发布与打包工作流 (devpiano-release)

> 用途：编排 devpiano 正式版本发布（Release）的全自动化工作流——从 Git 增量拓扑中自动感知提交变化、智能推导目标版本号、起草结构化 Changelog，到双平台生产构建、冒烟验证、打包归档、两步走 CI 绿灯守卫与 Git Annotated Tag 推送。
> 核心依据：[`docs/guides/release-workflow.md`](docs/guides/release-workflow.md)。
> 安全铁律：**必须在主分支（main）纯净工作树上执行；正式发布产物必须使用 Release 构建；推送 tag 前必须先推 main 验证 CI 绿灯；创建的 Git Tag 必须在用户最终明确授权后方可推送到远程仓库**。

---

## 1. 增量感知与版本/Changelog 智能推导引擎

在进入任何文件修改与构建前，本 Skill 必须首先通过 Git 历史执行**增量分析**：

```text
[触发 devpiano-release]
    │
    ▼
┌────────────────────────────────────────────────────────┐
│ Step 1: 自动定位上一个发布版本 (Last Release Tag)      │
│ LAST_TAG = git describe --tags --abbrev=0 2>/dev/null  │
│ 若无 tag，则 fallback 到 root commit (设为空)          │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ Step 2: 提取并分类增量提交 (Git Revision Inspection)   │
│ COMMITS = git log ${LAST_TAG}..HEAD --no-merges        │
│ MERGES  = git log ${LAST_TAG}..HEAD --merges           │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ Step 3: 基于 Conventional Commits 的 SemVer 自动决议   │
│ • 用户显式输入优先 (如 "v1.2.0") ──► 采纳用户指定版本  │
│ • 包含 BREAKING CHANGE / '!' ──► 推荐 Major Bump (vX+1)│
│ • 包含 feat: / feat(...)     ──► 推荐 Minor Bump (vX.Y+1)│
│ • 全为 fix / refactor / perf ──► 推荐 Patch Bump (vX.Y.Z+1)│
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ Step 4: 自动分类并起草 Keep a Changelog 新版本条目     │
│ • feat(...) ──► [Added] (提炼为人类可读的功能描述)     │
│ • refactor / perf ──► [Changed] (性能提升与架构治理)   │
│ • fix(...) ──► [Fixed] (修复的缺陷与边界安全加固)      │
│ • known-issues ──► [Known Issues] (未决已知限制)       │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ Step 5: 呈送起草报告并请求用户确认 (Draft Confirmation)│
│ 展示推导依据与 CHANGELOG 预览 ──► 用户确认后自动应用   │
└────────────────────────────────────────────────────────┘
```

### 1.1 增量提交提取命令
```bash
# 1. 获取上一个发布标签
LAST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "")

# 2. 提取自上个标签以来的增量提交列表
if [ -n "$LAST_TAG" ]; then
    git log "${LAST_TAG}..HEAD" --oneline
else
    git log --oneline -30
fi
```

### 1.2 SemVer 自动推导决策表
设上一个版本号为 `M.N.P`（如从 `v1.1.0` 提取为 `1.1.0`）：

| 增量提交特征 | 推荐升级级别 | 推导公式 | 示例 (基准 1.1.0) |
|---|:---:|:---:|:---:|
| 提交信息包含 `BREAKING CHANGE` 或类型后带 `!`（重大架构/破坏性变更） | **Major** | `(M+1).0.0` | `v2.0.0` |
| 包含任意 `feat:` 或 `feat(...):` 提交（新增业务/声学功能） | **Minor** | `M.(N+1).0` | `v1.2.0` |
| 仅包含 `fix:`、`refactor:`、`perf:`、`docs:`、`chore:`（纯修复/优化） | **Patch** | `M.N.(P+1)` | `v1.1.1` |

> **用户覆盖规则**：若用户在输入中明确指定了版本（如 *“发布 v1.3.0”* 或 *“帮我打一个 patch 版本”*），则直接遵从用户意图，但仍执行后续的 Changelog 自动生成。

### 1.3 Keep a Changelog 起草规范
根据增量提交前缀与内容，自动生成如下标准的 Markdown 块：

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- [根据 feat 提交总结的业务与物理声学新特性清单]

### Changed
- [根据 refactor/perf 提交总结的架构演进与优化]

### Fixed
- [根据 fix 提交总结的缺陷修复与稳定性改进]

### Known Issues
- [来自 docs/issues/known-issues.md 的重要未决限制（如有）]
```

---

## 2. 全流程发布流水线（Seven-Stage Pipeline）

```text
Stage 0: 增量分析与发布草稿智能生成 (Git Revision & Auto-Drafting)
    │
    ▼
Stage 1: 用户审查确认与文件自动对齐 (User Review & CMake/Changelog Apply)
    │
    ▼
Stage 2: 源码与分支预检 (Branch & Clean Working Tree)
    │
    ▼
Stage 3: 生产纯净构建 (win-build --release [--reconfigure])
    │
    ▼
Stage 4: 双平台冒烟测试阻断确认 (Windows Release 产物自检 + Linux 本地验证)
    │
    ▼
Stage 5: 本地自动化打包与校验和生成 (dev.sh package --version [--local-dist])
    │
    ▼
Stage 6: 两步走安全推送门禁 (Push main ──► Wait CI Green ──► Push Tag)
```

---

### Stage 0: 增量分析与发布草稿智能生成
1. 自动执行 `git describe --tags --abbrev=0` 定位 `LAST_TAG`；
2. 分析 `LAST_TAG..HEAD` 范围内的所有提交类型与主体；
3. 推导出推荐的 `VERSION = X.Y.Z` 与 `TAG = vX.Y.Z`；
4. 结构化起草 `CHANGELOG.md` 顶层增量区块；
5. **向用户输出推导依据与草稿预览**。

### Stage 1: 用户审查确认与文件自动对齐
1. 询问用户是否同意推荐的版本号与 Changelog 草稿内容；
2. 收到用户确认后（或根据用户调整意见微调后）：
   - 使用 `edit` 工具将 `CMakeLists.txt` 中的 `project(devpiano VERSION ...)` 更新为 `VERSION`；
   - 使用 `edit` 工具将生成的 `## [X.Y.Z] - YYYY-MM-DD` 区块插入到 `CHANGELOG.md` 顶部（紧随 `# Changelog` 规范声明之后）；
3. 刷新 WSL 编译配置：
   ```bash
   ./scripts/dev.sh wsl-build --configure-only
   ```

### Stage 2: 源码与分支预检
在 WSL 主工作树中执行预检命令：
```bash
git branch --show-current
git status --short
./scripts/dev.sh self-check
```
- 确认当前处于 `main` 主分支；
- 确认工作树干净，除本次发布的预期修改（`CMakeLists.txt`、`CHANGELOG.md`）外，无未提交的脏文件；
- 确认 `submodules/JUCE/` 无修改且环境自检通过。

### Stage 3: Windows MSVC Release 纯净构建
正式发布产物必须使用 Windows 镜像树的 MSVC **Release** 配置进行构建：
```bash
# 常规生产构建
./scripts/dev.sh win-build --release

# 若修改了 CMakeLists.txt 版本号且怀疑缓存未捕获，显式重配构建：
./scripts/dev.sh win-build --release --reconfigure
```
- 确认同步成功，MSVC 顺利编译链接出生产级 `DevPiano.exe`；
- 终端确认输出 `MSVC validation build completed successfully`。

### Stage 4: 双平台冒烟测试阻断确认（Gate Keeper）

在推进打包前，向用户明确列出双平台手工冒烟验证步骤并**硬性阻断等待确认**：

#### 4.1 Windows 手工冒烟测试
- **产物位置**：`<WIN_MIRROR_DIR>\build-win-msvc-release\devpiano_artefacts\Release\DevPiano.exe`
- **自检清单**：
  - [ ] 双击应用秒级启动，主窗口自适应居中；
  - [ ] 默认物理建模钢琴发声正常，按键无杂音爆音；
  - [ ] 电脑键盘弹奏（A/S/D/F）与 88 键虚拟键盘高亮正常；
  - [ ] VST3 扫描、加载、Editor 打开无崩溃；
  - [ ] 演奏录制、回放与离线导出 WAV 生成正常；
  - [ ] 退出应用无挂起、无崩溃提示。

#### 4.2 Linux 本地构建与冒烟验证（可选但推荐）
- **本地开发验证命令**：
  ```bash
  ./scripts/dev.sh wsl-build --release
  # 产物：build-wsl-clang-release/devpiano_artefacts/Release/DevPiano
  ```
- **核心关注点**：
  - ALSA / JACK 音频设备驱动初始化正常；
  - CJK 字体渲染清晰（Noto Sans CJK SC / Source Han Sans 回退链），弹窗无黑块；
  - 窗口焦点切换不打断回放，生命周期（缩放/关闭）正常；
  - glibc 门槛检查工具验证：`scripts/check_linux_glibc_floor.sh build-wsl-clang-release/devpiano_artefacts/Release/DevPiano`。

> **确认闸门**：收到用户明确的冒烟测试通过回复后，方可进入 Stage 5。

### Stage 5: 本地自动化打包与校验和生成
执行标准打包命令：
```bash
# 1. Windows x64 默认打包（产物输出至 Windows 镜像目录 dist/v${VERSION}/）
./scripts/dev.sh package --version ${VERSION}

# 可选：直接输出至 WSL 本地仓库的 dist/ 目录（便于后续 gh cli 上传）
./scripts/dev.sh package --version ${VERSION} --local-dist

# 可选：Linux 本地打包（tar.gz + sha256，打包前自动执行 glibc 门槛检查）
./scripts/dev.sh package --linux --version ${VERSION}
```
- 自动校验 `CMakeLists.txt` 与 `CHANGELOG.md` 版本一致性；
- 自动抓取生产产物并在 `dist/v${VERSION}\` 下生成 zip / tar.gz 与 sha256 校验和文件；
- 汇报 SHA256 哈希值供安全核验。

### Stage 6: 两步走安全推送门禁（Two-Step Push with CI Gate）

为了彻底杜绝“带着编译错误或格式违规推 tag 导致发布坏包”的事故，必须严格遵循**两步推送法**：

#### Step 6.1：提交版本文件并先行推送 main 分支
```bash
git add CMakeLists.txt CHANGELOG.md
git commit -m "chore(release): prepare release v${VERSION}"
git push origin main
```

#### Step 6.2：【CI 绿灯阻断守卫】
- 提示用户（或使用 `gh run list --branch main`）观察 GitHub Actions 主干 CI（`.github/workflows/ci.yml`）：
  - [ ] Linux Clang 单元测试与格式化检查通过；
  - [ ] Windows MSVC 纯净构建通过。
- **必须在 main 分支 CI 全绿通过后，方可推进至 Step 6.3**。

#### Step 6.3：Annotated Tag 创建与正式发布推送
经用户明确授权后，创建并推送正式发布标签：
```bash
# 1. 创建本地带注释 Git Tag
git tag -a ${TAG} -m "Release ${TAG}"

# 2. 核验本地 Tag 信息
git tag -n1 -l ${TAG}

# 3. 正式推送到远程仓库
git push origin ${TAG}
```
> **自动触发效果**：推送 `${TAG}` 将自动触发 GitHub Actions `.github/workflows/release.yml`，在云端官方 runner 自动编译 Windows MSVC 与 Linux Clang，打包双平台分发包并正式发布 GitHub Release。

---

## 3. 备用发布通道（CLI）与异常容灾策略

### 3.1 备用方案：GitHub CLI (`gh`) 本地直传发布
若 GitHub Actions 因云端故障不可用，或需离线发布本地打包产物，可使用备用 CLI 通道：
```bash
# 1. 创建 GitHub Release 并挂载 Windows 产物
gh release create "${TAG}" \
  --title "DevPiano ${TAG}" \
  --notes-file "CHANGELOG.md" \
  "dist/v${VERSION}/DevPiano-${TAG}-win-x64.zip" \
  "dist/v${VERSION}/DevPiano-${TAG}-win-x64.sha256"

# 2. 若存在本地打包的 Linux 产物，追加上传
gh release upload "${TAG}" \
  "dist/v${VERSION}/DevPiano-${TAG}-linux-x64.tar.gz" \
  "dist/v${VERSION}/DevPiano-${TAG}-linux-x64.sha256"
```

### 3.2 异常容灾与修复细则（依据 release-workflow.md §9）
- **场景 A：仅 zip / tar.gz / sha256 压缩打包或校验错误，源码与 Tag 完全正确**：
  - **严禁重新打 Tag**；
  - 重新执行打包，在 GitHub Release 页面编辑并直接替换对应的附件文件，在 release notes 末尾简要说明附件替换原因即可。
- **场景 B：源码存在逻辑缺陷或构建失败**：
  - **已推送到远端的 Tag 严禁删除、重写或移动**（保证版本历史单调递增）；
  - 保留该问题 Tag，在 `main` 分支修复缺陷、补充单元测试并通过三闸门验证后，递增发布新的 Patch 版本（如从 `v1.2.0` 发 `v1.2.1`）。
- **场景 C：Tag 仅在本地创建，尚未 push 到远程**：
  - 经用户明确确认后，可直接在本地删除重打：`git tag -d ${TAG}`。

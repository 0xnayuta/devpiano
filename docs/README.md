# devpiano 文档入口

> 用途：说明 `docs/` 的分层、阅读顺序与每份文档的唯一职责。  
> 读者：新接手项目的人、日常开发者、后续维护者。  
> 更新时机：新增、移动、归档文档时。

## 推荐阅读顺序

### 新开发者
1. `quickstart-dev.md`：快速恢复环境与常用命令。
2. `architecture.md`：理解当前 JUCE 架构与模块职责。
3. `roadmap.md`：了解当前状态、阶段路线与近期重点。

### 日常开发
- `quickstart-dev.md`：命令速查。
- `dev-workflow-wsl-windows-msvc.md`：WSL 主工作树 + Windows 镜像树 + MSVC 验证的详细工作流。
- `current-iteration.md`：当前正在推进的任务。

### 测试与验收
- `testing/acceptance.md`：阶段性验收标准。
- `testing/keyboard-mapping.md`：键盘映射专项手工测试。
- `testing/plugin-host-lifecycle.md`：插件宿主生命周期与退出稳定性专项测试。

### 旧代码迁移
- `legacy-migration.md`：旧 FreePiano 源码的迁移边界与新旧模块对应关系。

### 历史资料
- `archive/`：历史迁移记录、旧规划文档与过期资料。

## 文档职责原则

- `roadmap.md` 是唯一的项目状态与路线图来源。
- `current-iteration.md` 只记录当前正在做的任务。
- `architecture.md` 只描述当前架构，不混入任务清单。
- `testing/` 下只放验收与测试，不混入开发规划。
- 历史和过期内容全部进入 `archive/`。

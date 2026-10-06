# PR-Agent AI 代码审查工作流

> 用途：说明 devpiano 仓库中 PR-Agent（AI 代码审查）的部署方式、配置项、使用命令与故障排查。
> 更新时机：调整 workflow / `.pr_agent.toml` 配置或升级 action 版本时。

## 概述

PR-Agent 是开源 AI 代码审查 agent（[The-PR-Agent/pr-agent](https://github.com/The-PR-Agent/pr-agent)，MIT）。devpiano 通过 GitHub Action 方式部署：

- PR 打开 / 重新打开 / 转为 ready / push 新提交（`synchronize`）时自动执行 `/describe`（AI 生成 PR 描述）与 `/review`（代码审查）。
- 模型：`openai/deepseek-v4-flash-free`，通过 KiosAPI OpenAI-compatible endpoint 调用。
- 审查输出为 `github-actions[bot]` 的 PR 评论，不参与 required checks，**不阻塞合并**。

## 相关文件

| 文件 | 职责 |
|---|---|
| `.github/workflows/pr-agent.yml` | GitHub Actions 工作流：事件触发、权限、模型与自动工具开关 |
| `.pr_agent.toml` | 仓库级配置（从 main 分支读取）：响应语言、忽略路径、review 额外指令 |
| `docs/guides/pr-agent.md` | 本文档 |

## 工作流触发

| 事件 | 行为 |
|---|---|
| `pull_request: opened / reopened / ready_for_review` | 自动 describe + review |
| `pull_request: synchronize`（push 新提交） | 自动增量 review（merge commit / bot 提交自动跳过） |
| `issue_comment: created / edited`（PR 内评论命令） | 执行 `/review` `/describe` `/improve` `/ask` 等命令 |

`if: github.event.sender.type != 'Bot'` 防止 bot 自身评论再次触发（防循环）。

## 手动命令（PR 内评论）

| 命令 | 作用 |
|---|---|
| `/review` | 代码审查（可加 `--pr_reviewer.extra_instructions="..."` 单次覆盖配置） |
| `/describe` | 重新生成 PR 描述 |
| `/improve` | 逐行代码改进建议（默认不自动执行，按需触发） |
| `/ask "问题"` | 针对 PR 内容提问 |
| `/config` | 查看当前生效配置 |

## 配置说明（.pr_agent.toml）

- `[config] response_language = "zh-CN"`：审查评论使用中文。
- `[ignore] glob = ["submodules/**", "docs/archive/**", "docs/audit/**", "*.patch", "*.lock"]`：忽略第三方子模块、归档文档、审计报告、补丁与 lock 文件变更（不应进入审查范围）。
- `[pr_reviewer] extra_instructions`：追加审查关注点——实时音频契约（无锁/零堆分配/安全 fallback）、AGENTS.md 12 项核心架构要求（固定拓扑/防悬挂/UI 单一事实源）、7-bit ASCII 与 ADR-015 本地化规范、内化声明式 UI 边界与 Conventional Commits 提交规范。
- `AGENTS.md` 默认作为 repo context 自动注入 review / describe / improve 的提示词（v0.39+ 行为），仓库规范无需重复配置。

## 模型与密钥

- 当前模型：`openai/deepseek-v4-flash-free`（KiosAPI OpenAI-compatible 路由）。
- Base URL：`https://kiosapi.com/v1/`。
- 密钥：workflow 将 GitHub Actions Secret `KIOSAPI_API_KEY` 注入 `OPENAI_KEY`；密钥不写入仓库，需在仓库设置中手动填写。
- 模型 ID 必须对该 API key 的 Token Group 可用。KiosAPI 支持用 `GET /v1/models` 查询 key 可访问的精确 ID；当前未配置 key，因此尚未验证该模型 ID。
- 切换模型：修改 workflow 中 `config.model` 与对应密钥 env 变量（参考官方 [changing_a_model](https://docs.pr-agent.ai/usage-guide/changing_a_model/) 文档）。

## 升级与维护

- Action 版本 pin 在 `the-pr-agent/pr-agent@v0.41.0`；升级时修改版本 tag 并回归验证一次。
- Docker Hub 命名空间自 0.34.2 起迁移为 `pragent/pr-agent`，仅影响手动 `docker pull` 场景，GitHub Action 引用无需改动。

## 故障排查

| 现象 | 处理 |
|---|---|
| 新 PR 无自动评论 | 确认 workflow 已合并到 main；查看 Actions 运行日志中的模型/密钥报错 |
| 配置修改不生效 | `.pr_agent.toml` 需在 main 分支生效；修改后对 PR 评论 `/review` 或重新触发 |
| 报模型/密钥错误 | 检查 `KIOSAPI_API_KEY` secret 是否已配置；确认 Token Group 包含模型、`/v1/models` 返回对应 ID，并检查 KiosAPI 配额 |
| bot 评论后不再触发 | 正常行为——`sender.type != 'Bot'` 防循环 |

# Windows-MCP × OMP：接入、使用与维护

> 用途：说明 devpiano 在 WSL2 + OMP 下通过 `windows-mcp` 操作 Windows 原生 GUI 的架构、配置、使用边界、失败处理与升级回滚。
> 更新时机：MCP 服务端版本、`.omp/mcp.json` 注册、审批策略、GUI 验证流程或归档约定变化时。
> 关联：项目规则见 [`AGENTS.md`](../../AGENTS.md) §3；批次证据见 `G:\source\projects\devpiano-evidence\windows-mcp\`。

## 1. 连接架构

```
WSL2 (Ubuntu)                          Windows 11
┌────────────────────────┐            ┌──────────────────────────────────────┐
│ OMP 会话               │  stdio     │ Sbroenne.WindowsMcp.exe (1.3.28)     │
│  .omp/mcp.json ────────┼───────────>│  cwd = G:\tools\windows-mcp\1.3.28   │
│   18 个 mcp__windows_  │  经 WSL     │  UIA / Win32 / 截图 / 输入注入       │
│   mcp_* 工具           │  interop    │  ↓ 操作目标窗口                      │
└────────────────────────┘            │  DevPiano.exe（镜像树 Debug 构建）    │
                                      │  Pianoteq 9.exe（必要的第三方参考）    │
                                      └──────────────────────────────────────┘
```

- 传输：OMP 以 **stdio** 启动 Windows 原生 EXE；不使用 HTTP 端口。
- 服务端不负责启动/关闭被操作程序：进程生命周期仍由既有脚本（如 `owned-session.ps1`）或人工控制。
- 目标程序身份由外部证据（EXE 路径、SHA-256、PID、创建时间、HWND）锁定，不依赖窗口标题。

## 2. 固定服务端与版本核验

| 项 | 值 |
|---|---|
| 部署目录 | `G:\tools\windows-mcp\1.3.28\`（仅含 EXE，属**正式运行资产，禁止移动/覆盖/清理**） |
| 可执行文件 | `Sbroenne.WindowsMcp.exe`，59,903,263 字节 |
| SHA-256 | `0e6a20a9eeebce4e292fa357f8348bce9d9c0f1e1780667d8c547f2fdb743295` |
| 签名 | NotSigned（自包含 .NET 单文件），因此只能用版本目录 + SHA-256 锁定 |
| 服务标识 | `sbroenne.windows-mcp`，版本 `1.3.28+655b9fc92d03ad52a270067c6844ea1231c90559` |
| 协议版本 | `2025-11-25`，18 个工具 |

核验命令（WSL）：

```bash
sha256sum /mnt/g/tools/windows-mcp/1.3.28/Sbroenne.WindowsMcp.exe
# 期望：0e6a20a9…295
```

来源证据（release 归档 sha256 `9ca69685222d3c4934680218b4a2b3b7ae5bd0c8d2ce8bc2220925f4bd924fe4`、
静态协议记录、Notepad 对照）保存在 `devpiano-evidence\windows-mcp\discovery-and-preflight\`。

## 3. 项目级注册与配置优先级

`.omp/mcp.json`（**本机配置，被 `.gitignore` 的 `.omp/*` 忽略，不进入仓库**）：

```json
"windows-mcp": {
  "type": "stdio",
  "command": "/mnt/g/tools/windows-mcp/1.3.28/Sbroenne.WindowsMcp.exe",
  "cwd": "/mnt/g/tools/windows-mcp/1.3.28",
  "timeout": 120000,
  "instructions": false,
  "env": { "MCP_WINDOWS_WINDOW_ACTIVATION_MAX_RETRIES": "2" }
}
```

- 项目级配置优先于全局；同一 server 名以项目级条目为准。
- `instructions: false`：不把服务端自带说明书注入每次会话上下文（18 个工具的 schema 仍然注册）。
- `timeout: 120000`：stdio 首次握手与 UIA 长操作的实际余量（实测就绪 7.98 s）。
- 修改 `.omp/mcp.json` 后需要 `/mcp reload` 或新会话才生效（工具在会话启动时注册）。

## 4. 逐工具审批策略（`.omp/config.yml`）

18 个工具分三层（键名与注册名逐字一致，均为 `mcp__windows_mcp_<tool>`）：

| 策略 | 工具 | 用途 |
|---|---|---|
| allow（6） | `ui_snapshot` `ui_find` `ui_read` `ui_read_table` `ui_wait` `screenshot_control` | 只读观察：快照、精确查找、读值、等待条件、截图 |
| prompt（7） | `app` `window_management` `ui_click` `ui_type` `ui_select` `keyboard_control` `process` | 变更类：启动/关闭窗口、点击、输入、选择、按键、进程操作；主会话逐次人工确认，无头子 agent 直接拒绝 |
| deny（5） | `mouse_control` `clipboard` `file_open` `file_save` `ui_batch` | 本项目不需要：坐标注入、剪贴板、文件对话框、批量动作 |

- **不要为方便临时放宽**：提高权限需要同时说明场景、影响面与回退方式，并同步更新本表。
- 本轮静态核验：`config.yml` 的 18 个键与本会话实际注册的 18 个 `mcp__windows_mcp_*` 工具名一一对应；
  执行层策略（deny/allow/prompt 各自被拒/被执行/被要求审批）已在 Task 40 实测。

## 5. 工作目录（cwd）硬约束

**本机实测结论**：经 WSL interop 启动的 Windows 进程，若工作目录是 Linux 原生路径（如 `/root/repos/devpiano`、`/tmp`），
.NET Generic Host 会**永久挂起**：进程存活、无 stdout/stderr、不响应 JSON-RPC。改用 drvfs 路径（`/mnt/g/...`）后 0.6 s 完成握手。

- 因此 `mcp.json` 必须显式设置 `cwd` 为 Windows 可映射路径；当前值 `/mnt/g/tools/windows-mcp/1.3.28`。
- 该结论是本机 WSL2 环境的实验事实（同一二进制、仅改变 cwd 的对照实验），**不宣称适用于所有 Windows-MCP/WSL2 组合**；
  更换盘符（如 ReFS→NTFS）或迁移服务端时应复测。
- 排查顺序：先做"改变 cwd"的二分，再查二进制哈希、ACL、桌面/窗口站、UIA 子系统与网络。

## 6. 真实 GUI 调用的操作流程

每个动作序列固定为：

1. **窗口发现**：`window_management` 列出候选窗口（handle、pid、class、title）。
2. **身份核验**：核对目标 EXE 路径、PID、**进程创建时间**、HWND、前台状态；与外部记录（生命周期 JSON / SHA-256）一致才继续。
   JUCE 窗口类名形如 `JUCE_1a12c1c3371`，**类名不构成身份**，只用于辅助定位。
3. **UIA 快照 / 精确查找**：先 `ui_snapshot` 拿整体结构，再用 `ui_find` 按名称/类型精确定位；
   匿名控件用其在父节点中的位置与可读值间接定位。
4. **授权动作**：只调用审批策略允许或已获批的动作；`prompt` 类动作在**主会话**逐次确认（不委托无头子 agent）。
5. **状态回读**：动作后必须用 `ui_read` / `ui_snapshot` / 截图**独立回读**确认业务状态；
   "MCP 已派发/返回 success" **不等于**业务成功（例：`ui_select` 返回成功但组合框值未变）。
6. **证据留存**：原始 JSON-RPC 请求、UIA 快照、截图、审批记录写入该任务的证据目录并归档。

## 7. JUCE 与桌面框架的已知兼容性问题（实测）

| 现象 | 实测表现 | 处理 |
|---|---|---|
| ComboBox 不支持 SelectionPattern | `ui_select` 返回 `pattern_not_supported` | 不重试同类调用；改用键盘/菜单路径或（获批后）坐标兜底 |
| JUCE 弹出菜单不属于窗口 `active_dialog` | `ui_find(scope=active_dialog)` 返回 `window_not_found` | 先按 owner PID 找到 popup HWND，再在该范围内查找 |
| 菜单项 enabled 状态与点击时状态不一致 | 查找时 enabled，`ui_click` 返回 disabled，popup 消失 | 视为未完成；回读确认后暂停并报告，不做自动重试循环 |
| 匿名控件 | 自绘控件常无名称（ComboBox/Button） | 用类型 + 位置 + 可读值定位；不要把它们统计为"控件缺失" |
| 截图标注索引与图内标签不一致 | `screenshot_control` 标注编号与 PNG 内容对不上（已向工具问题入口报告） | 不依赖标注编号；使用原始截图 + 精确名称 + clickablePoint |
| 独立 popup HWND 不进入父窗口截图 | 菜单弹出的图像不含菜单本体 | 菜单状态用 UIA 回读证明，不用父窗口截图证明 |

## 8. 坐标兜底的授权条件

只有在语义路径（`ui_click` / `ui_select`）实测失败，且**另行获批**后才允许 `mouse_control`，并同时满足：

- 使用**新鲜截图**（当轮采集，不用历史标注）推导物理坐标；
- 换算链完整记录：物理屏幕点 → 目标窗口物理左上角 → 窗口内坐标；
- 调用带 `windowHandle` + 预期标题 + 进程名 + 前台保护；
- **单次执行**，失败不自动重试，立即暂停并报告；
- 之后必须用 UIA 独立回读确认结果（不得以工具返回 success 为准）。

## 9. 与默认 CTest / ADR-016 的边界

- 默认 CTest（`devpiano_tests`）是**纯内存离屏**测试：不得依赖真实桌面、不得调用 `addToDesktop()`。
- Windows-MCP GUI 冒烟是**人工/受控的原生表面验证**，只用于：
  - 验证真实构建产物的界面、布局、控件可见状态、真实输入路径；
  - 观察代码阅读与单元测试无法覆盖的交互（焦点、弹窗、自绘控件）。
- 两条路径**互不替代**：GUI 冒烟不进 CI、不写成 CTest；单元测试不因"GUI 已验证"而降低覆盖面。
- 需要观察真实界面时使用本工具；纯内存逻辑与 Linux CI 已覆盖的内容不额外启动 Windows GUI 空转。

## 10. 失败、断连与卡住的处理

| 情况 | 处理 |
|---|---|
| 工具返回 `pattern_not_supported` / `window_not_found` | 不改写 GUI 脚本凑合通过；记录原始失败，改用不依赖该 pattern 的路径或暂停等授权 |
| 审批被拒绝 | 视为正常结果，记录并停止该动作；不绕过审批（例如不做等价坐标点击） |
| MCP 断连 / 会话中不可用 | 用 `/mcp reload` 或新会话重连；重连后重新核验目标窗口身份再继续 |
| 服务端卡住（无响应、无输出） | 先确认 Windows 侧进程状态与 cwd 是否被改变；必要时结束该 MCP 进程让 OMP 重启它；不要向目标程序发送强制终止 |
| 窗口失焦 / 按键未释放 | 动作前用前台保护并核验前台窗口；失焦后重新核验身份再操作；`keyboard_control` 后必须回读确认键已释放，避免残留修饰键 |
| 目标程序非正常退出 | 保留原现场（进程枚举、窗口枚举、私有状态散列），报告并停止，不自动重启 |

## 11. 禁用、重连、升级与回滚

```bash
# 禁用（临时）：从项目 .omp/mcp.json 删除或注释 windows-mcp 条目；重载或新会话生效
# 重连：会话内执行 /mcp reload；或退出后重新启动 OMP 会话
# 查看当前工具：新会话中确认 18 个 mcp__windows_mcp_* 工具已注册
```

升级流程（版本目录策略，禁止原地覆盖）：

1. 新版本解压到 `G:\tools\windows-mcp\<new-version>\`，仅保留 EXE；
2. 记录并核验新 EXE 的 SHA-256 与 release 归档身份；
3. 修改 `.omp/mcp.json` 的 `command` 与 `cwd` 指向新版本目录；
4. 新会话重新枚举工具：确认工具名集合（期望 18 个）与旧版一致，逐字比对 `.omp/config.yml` 的键；
5. 做一次最小冒烟（窗口发现 → 快照 → 一次 allow 类读取 → 正常关闭），证据归档；
6. 保留旧版本目录，直至新版本通过验收。

回滚：把 `.omp/mcp.json` 的 `command`/`cwd` 改回 `1.3.28` 目录即可（版本目录从未被覆盖）。

## 12. 证据与归档约定

- 每次 GUI 验证产出：原始 JSON-RPC 请求记录、UIA 快照、截图、审批记录、失败原文、生命周期与进程/配置保护比对。
- 批次落在任务临时根，验收后归档到 `G:\source\projects\devpiano-evidence\windows-mcp\<area>\<batch>\`；
  索引见该目录 `_index\`（inventory / classification / migration-map / integrity-manifest / evidence-catalog）。
- 不复制：与正式部署相同的服务端二进制副本、可重新下载的 release 归档、一次性 OMP 会话数据库、Windows 系统缓存（只登记 sha256 与来源）。
- 归档文件内的旧绝对路径不就地改写；换算通过 `_index\migration-map.json` 完成。
- 归档与清理规则见 [`wsl-windows-msvc-workflow.md`](wsl-windows-msvc-workflow.md) 的"证据归档"一节。

## 13. 实测与未验证边界（诚实清单）

已实测：连接与注册（18 工具，7.98 s）、三层审批策略的实际执行、DevPiano Debug 产物 7 步 GUI 冒烟
（窗口发现 → 快照 → 截图 → 点击展开 → 按键派发 → 复读 → 关闭）、Pianoteq 9.2.5 受限 GUI 观察与历史批次的一次获批坐标兜底、
私有状态与用户配置的散列保护、正常退出后无残留进程。交互式会话的真实审批弹窗确认已由用户在独立会话完成，不再列为“未实测”。

本轮另验证了 Standard 模式切换、官方 JSON-RPC 必要参数写回、完整 FXP 保存/对质恢复，以及先切 Standard 再加载 FXP 的冷启动 GUI 读回；不据此宣称 MCP 可通用编辑全部自绘控件。

未实测（不得写成通过）：

- 按键是否产生**声音**（本轮工具范围不含音频观测）；
- NTFS 盘符上的 cwd 行为（当前镜像盘为 ReFS）；
- 超出当前已验证控件的自绘参数编辑能力、`Random seed` 数字读回、确定性重复与完整声学验收。

已测但未通过：官方 headless 冷启动的完整执行状态一致性。该路径未恢复必要参数，因此没有启动 v2 Pilot；GUI 工具迁移不能替代声学准入。具体证据与下一步见 [Task 39-2 当前迭代记录](../roadmap/current-iteration.md#task-39-2-实施记录官方-925-状态闭环2026-10-12)。

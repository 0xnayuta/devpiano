# devpiano 布局 Preset 手工测试清单

> 用途：对布局 preset 的保存、导入、重命名、删除与启动恢复行为进行手工验证。  
> 读者：修改 `LayoutPreset`、`LayoutDirectoryScanner`、布局下拉框 UI 或布局恢复逻辑的开发者。  
> 更新时机：布局 preset 文件格式、交互行为、发现机制或启动恢复路径变化时。

状态标记：
- [x] 已通过
- [ ] 未通过 / 未开始验证
- [~] 部分通过 / 待补充验证

相关文档：
- 功能说明：[`../features/phase3-layout-presets.md`](../features/phase3-layout-presets.md)
- 阶段验收：[`acceptance.md`](acceptance.md)
- 项目路线图：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 1. 测试目标

本测试文档主要验证以下内容：

- 内置 preset 与用户 preset 能否在下拉框中稳定切换
- `Save Layout` 是否能生成合法的 `.freepiano.layout` 文件并保持合理的显示名称
- `Import Layout` 是否能把外部 preset 复制到用户布局目录并立即应用
- `Rename` 是否只修改显示名称，不改变文件名与稳定 `layoutId`
- `Delete` 是否只删除用户 preset，并在删除当前活动布局时安全回退
- 启动后是否能基于持久化的 `layoutId` 和 `keyMap` 正确恢复布局

---

## 2. 测试前提

### 环境前提
- [x] 项目可成功构建并启动
- [x] WSL 构建基线已可用：`./scripts/dev.sh wsl-build`
- [x] Windows 验证基线已可用：`./scripts/dev.sh win-build`
- [x] 主窗口可见
- [x] `ControlsPanel` 的 layout 下拉框与 `Save / Import / Rename / Delete / Reset` 按钮可见

### 建议准备
- [x] 确认用户布局目录可访问：`%APPDATA%\DevPiano\Layouts\`
- [x] 准备至少一个外部 `.freepiano.layout` 文件用于导入测试
- [x] 若要验证启动恢复，确保程序能正常关闭并再次启动

### 代码级 / 行为观察基线

> 说明：当前布局 preset 回归仍以手工测试为主，但建议先确认以下实现边界，避免把“当前设计如此”误判成运行故障。

建议先复核以下代码行为：

- `source/Layout/LayoutPreset.*`
  - 承载 preset 的 JSON 读写，当前会读写 `version`、`id`、`name`、`displayName` 与 `bindings`
- `source/Layout/LayoutDirectoryScanner.*`
  - 承载用户布局目录扫描与 `layoutId` 查找，当前只扫描用户布局目录中的 `.freepiano.layout`
- `source/MainComponent.*`
  - 承载 Save / Import / Rename / Delete / Startup Restore 的主协调逻辑
  - 当前 `Import` 会把外部文件复制到用户布局目录；`Save Layout` 则保存到用户选定路径本身
- `source/Settings/SettingsModel.h`
  - 承载 `layoutId -> KeyboardLayout` 恢复逻辑；当用户 preset 无法从用户布局目录找到时，会回退到内置布局基础并叠加已保存的 `keyMap`

### 观察基线

建议优先观察以下行为是否一致：

- 下拉框当前选中项与实际活动布局是否一致
- 用户 preset 的显示名称是否来自 JSON，而不是单纯来自文件名
- 内置 preset 是否始终表现为 `FreePiano Minimal` / `FreePiano Full`
- `Rename` / `Delete` 在内置 preset 选中时是否保持禁用

---

## 3. Save Layout 测试

## 3.1 保存内置 preset 到用户布局目录

### 目标
验证从内置 preset 生成用户 preset 时，文件可正常写入、布局立即切换，且显示名称风格一致。

### 测试步骤
- [x] 启动程序
- [x] 在下拉框中选择 `FreePiano Minimal`
- [x] 点击 `Save Layout`
- [x] 在用户布局目录中保存为 `minimal-test.freepiano.layout`
- [x] 观察保存完成后的下拉框当前项
- [x] 再切换到 `FreePiano Full`
- [x] 重复保存为 `full-test.freepiano.layout`

### 预期结果
- [x] 两次保存都成功生成 `.freepiano.layout` 文件
- [x] 保存完成后，当前活动布局立即切换为新保存的用户 preset
- [x] 两个新文件都出现在下拉列表中
- [x] 新文件的默认显示名称分别为 `FreePiano Minimal` / `FreePiano Full`，而不是旧的历史字符串

### 状态
- [x] 已验证

---

## 3.2 保存到用户布局目录之外

### 目标
验证 `Save Layout` 在用户布局目录之外保存时的当前行为边界。

### 测试步骤
- [x] 选择任意内置或用户布局
- [x] 点击 `Save Layout`
- [x] 将文件保存到用户布局目录之外的位置
- [x] 观察保存后的当前活动布局与下拉列表
- [x] 关闭并重新启动程序

### 预期结果
- [x] 文件在用户选择的位置成功生成
- [x] 程序会立即切换到该新保存布局
- [x] 该文件不会因为“保存”动作自动复制到用户布局目录
- [x] 重启后，该外部文件不会被用户布局目录扫描逻辑重新发现为同一个用户 preset
- [x] 若该外部保存布局此前已写入设置，启动后仍可能基于持久化 `keyMap` 回退恢复到一个可用基础布局；此时应重点确认“原用户 preset 是否未被重新发现”而不是简单判断为“完全不恢复”

### 状态
- [x] 已验证

---

## 4. Import Layout 测试

## 4.1 从外部路径导入 preset

### 目标
验证导入外部 `.freepiano.layout` 时，文件会复制到用户布局目录并立即应用。

### 测试步骤
- [x] 准备一个位于用户布局目录之外的 `.freepiano.layout` 文件
- [x] 启动程序
- [x] 点击 `Import`
- [x] 选择该外部文件
- [x] 观察导入后的下拉框与当前活动布局
- [x] 检查用户布局目录中的文件列表

### 预期结果
- [x] 外部文件成功复制到用户布局目录
- [x] 复制后的文件名与原始文件名一致
- [x] 导入完成后，该布局立即成为当前活动布局
- [x] 下拉列表中出现该用户 preset
- [x] 若用户布局目录中已存在同名文件，本次导入会覆盖该同名用户 preset，测试时应提前确认该覆盖行为是否符合预期

### 状态
- [x] 已验证

---

## 4.2 导入已有显示名称的 preset

### 目标
验证导入时优先保留源文件中的 `displayName` / `name`。

### 测试步骤
- [x] 准备一个 JSON 内含 `displayName` 的 `.freepiano.layout` 文件
- [x] 点击 `Import` 并导入该文件
- [x] 观察导入后的下拉菜单文本

### 预期结果
- [x] 下拉菜单显示源文件中已有的显示名称
- [x] 不会因为导入而强制退回到文件名派生名称（除非源文件名称字段为空）

### 状态
- [x] 已验证

---

## 5. Rename Layout 测试

## 5.1 重命名用户 preset 的显示名称

### 目标
验证 `Rename` 只修改显示名称，不修改文件名与稳定 `layoutId`。

### 测试步骤
- [x] 在下拉框中选择一个用户 preset
- [x] 点击 `Rename`
- [x] 在弹出的输入框中输入新的显示名称
- [x] 确认保存
- [x] 观察下拉列表中的名称变化
- [x] 检查用户布局目录中文件名是否变化

### 预期结果
- [x] 下拉列表立即显示新的名称
- [x] 文件名保持不变
- [x] 该用户 preset 仍可被正常选中、删除和重启恢复
- [x] 重命名后再次 `Save Layout` 到同一文件时，显示名称不会被旧逻辑覆盖

### 状态
- [x] 已验证

---

## 5.2 内置 preset 不可重命名

### 目标
验证内置 preset 不暴露错误的重命名路径。

### 测试步骤
- [x] 在下拉框中选择 `FreePiano Minimal`
- [x] 观察 `Rename` 按钮状态
- [x] 再选择 `FreePiano Full`
- [x] 再次观察 `Rename` 按钮状态

### 预期结果
- [x] 两个内置 preset 选中时 `Rename` 按钮均为禁用状态
- [x] 不存在点击后静默无反应的错误交互

### 状态
- [x] 已验证

---

## 6. Delete Layout 测试

## 6.1 删除非当前活动用户 preset

### 目标
验证删除普通用户 preset 时，文件和列表同步更新。

### 测试步骤
- [x] 确保下拉列表中至少有两个用户 preset
- [x] 切换到其中一个以外的布局
- [x] 选中待删除的用户 preset
- [x] 点击 `Delete`
- [x] 在确认对话框中确认删除
- [x] 检查下拉列表与用户布局目录

### 预期结果
- [x] 目标用户 preset 从下拉列表中消失
- [x] 在磁盘权限正常、文件未被占用的情况下，对应文件应从用户布局目录中删除
- [x] 当前活动布局若不是被删除项，则保持不变

### 状态
- [x] 已验证

---

## 6.2 删除当前活动用户 preset

### 目标
验证删除当前活动用户 preset 时会安全回退到内置默认布局。

### 测试步骤
- [x] 选择一个用户 preset 并确保其为当前活动布局
- [x] 点击 `Delete`
- [x] 在确认对话框中确认删除
- [x] 观察下拉框当前选中项

### 预期结果
- [x] 在磁盘权限正常、文件未被占用的情况下，对应文件应被删除
- [x] 程序回退到 `FreePiano Minimal`
- [x] 下拉框与实际活动布局一致
- [x] 程序不崩溃，不留下错误状态

### 状态
- [x] 已验证

---

## 6.3 内置 preset 不可删除

### 目标
验证内置 preset 不暴露错误的删除路径。

### 测试步骤
- [x] 选择 `FreePiano Minimal`
- [x] 观察 `Delete` 按钮状态
- [x] 选择 `FreePiano Full`
- [x] 再次观察 `Delete` 按钮状态

### 预期结果
- [x] 两个内置 preset 选中时 `Delete` 按钮均为禁用状态
- [x] 不存在误删内置 preset 的入口

### 状态
- [x] 已验证

---

## 7. Startup Restore 测试

## 7.1 恢复内置 preset

### 目标
验证程序能根据持久化的 `layoutId` 恢复内置布局。

### 测试步骤
- [x] 选择 `FreePiano Full`
- [x] 正常关闭程序
- [x] 重新启动程序
- [x] 观察下拉框当前选中项

### 预期结果
- [x] 程序启动后恢复到 `FreePiano Full`
- [x] 下拉框选中项与实际活动布局一致

### 状态
- [x] 已验证

---

## 7.2 恢复用户 preset

### 目标
验证程序能从用户布局目录恢复上次选中的用户 preset。

### 测试步骤
- [x] 选择一个位于用户布局目录中的用户 preset
- [x] 正常关闭程序
- [x] 重新启动程序
- [x] 观察下拉框当前选中项与显示名称

### 预期结果
- [x] 程序启动后恢复到同一个用户 preset
- [x] 显示名称与重命名后的 JSON 名称一致（若该 preset 曾被重命名）
- [x] 不会错误回退到其他用户 preset 或内置 preset

### 状态
- [x] 已验证

---

## 7.3 恢复时重新叠加持久化 keyMap

### 目标
验证启动恢复时不仅恢复 `layoutId`，还会叠加已保存的 `keyMap`。

### 测试步骤
- [x] 准备一个已保存并可稳定复现的用户布局
- [x] 确认其音高映射与当前下拉框名称一致
- [x] 正常关闭程序
- [x] 重新启动程序
- [x] 用 `A/S/D/F` 等基础键位试弹

### 预期结果
- [x] 启动后恢复的布局不只是“名称恢复”，其实际键位映射也与关闭前一致
- [x] 不出现恢复了正确名字但映射已退回默认布局的情况

### 状态
- [x] 已验证

---

## 8. 建议最小回归集合

每次修改以下任一逻辑后，至少执行本文件中的对应用例：

- `source/Layout/LayoutPreset.*`
- `source/Layout/LayoutDirectoryScanner.*`
- `source/MainComponent.*` 中的布局保存 / 导入 / 删除 / 重命名 / 恢复路径
- `source/Settings/SettingsModel.h` 中的 `layoutId -> KeyboardLayout` 恢复逻辑
- `source/UI/ControlsPanel.*` 中的布局下拉框与按钮状态逻辑

建议最小回归包：

- [x] 3.1 保存内置 preset 到用户布局目录
- [x] 4.1 从外部路径导入 preset
- [x] 5.1 重命名用户 preset 的显示名称
- [x] 6.2 删除当前活动用户 preset
- [x] 7.2 恢复用户 preset

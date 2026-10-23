# devpiano Current Iteration

> 用途：维护当前候选阶段、恢复条件与实施顺序；项目状态和长期阶段顺序以 [roadmap](roadmap.md) 为准。
> 归档口径：Phase 39 与 R0–R11 的任务、实施记录、验收、失败历史和证据已迁入 `docs/archive/`。本页不重复维护历史状态，也不把归档记录当作当前默认或已通过结论。
> 当前状态：Phase 39.5（JIVE 内核精简与 JIVE v2 精准移植）**已完成（2026-10-23）**：39.5-1（基线与未用能力闭包精简）、39.5-2（属性、对象与 Length 安全切换）、39.5-3（集中属性分发与 BoxModel 去污染）、39.5-4a/4b/4c/4d（构建守卫与脏标记、Item 缓存失效、TextLayout 缓存、Flex gap 修复）与 39.5-5（目录归位与最终集成）全部完成；用户人工表面验收确认无明显问题，Windows Debug app/tests、默认 CTest 与修复后的增量 clang-tidy 通过。Phase 39 已冻结，Task 39-2 仍为 **inconclusive**，Phase 39 不记为通过；R0–R11 已完成并归档；Phase 40–42 及后续声学能力仍暂缓，恢复须由用户明确决定；E3 有限尾音维护任务保持独立，不受本阶段影响。

## 0. 归档入口

- Phase 39 可复核声学基线与三力度整音校准：[历史归档](../archive/phase39-acoustic-baseline-and-voicing-calibration.md)。Task 39-2 外部参考直接验收缺口、已消费的资格/采集/判决和“不注入 B 表”的收尾结论保持有效。
- R0–R11 内置钢琴音源结构整理：[历史归档](../archive/r0-r11-physical-piano-source-refactor.md)。R5/R6 踏板所有权迁移与 R7 新声部 sustain 初始化是已批准行为变化；R1/R2/R3/R9/R10 为组件提取与封装。
- Phase 37/38 历史任务、声音基线和后续修复：[完成归档](../archive/phase37-38-piano-calibration-and-keyboard-performance.md)。

## 1. 当前候选阶段：Phase 39.5

### 1.1 基线、目标与边界

本页是**经源码复审的实施计划，不是实施完成记录**。对照来源为本机 `/root/repos/JIVE`（目录实际为大写；非新增构建依赖）的 `v2` 分支，固定参考提交 [`e25b4d57636ac4e48a30453f1542d29a655c7a83`](https://github.com/ImJimmi/JIVE/commit/e25b4d57636ac4e48a30453f1542d29a655c7a83)。后续不能把浮动的“最新 v2”作为同一份已审查输入。

**目标**：先按产品消费闭包精简，再移植有实际收益的值、属性、布局与文本实现，最后整理目录。业务继续经 `ViewHost` 使用声明式 UI；不恢复外部 JIVE 子模块，不建设通用 UI 框架。Phase 39.5 是 [ADR-014](../decisions/ADR-014-internalize-ui-infrastructure-and-deprecate-jive-submodule.md) 底层封存规则的一次明确专项维护，不永久解除日常业务开发的门面约束。

- **保留产品契约**：`StyleCatalog` / `StyleBootstrap` / `DevPianoLookAndFeel` / `LocaleManager`、现有类型/伪类选择器、对象式 Gradient/Fill、`Text` 的 `justification` / `direction` / `line-spacing` / `word-wrap`、四个无前缀 `ideal-*` / `component-*` 属性及 `ViewHost` 的内容定尺语义；保留 Transitions/Easing/Bezier/TransferFunction 与 Block 布局闭包。
- **不移植整套上游样式系统**：不引入 `jive::LookAndFeel`、SourceFile/FileObserver、外部 XML/JSON 解释与热重载、逐节点语言文件；不搬字体属性改名、CSS 渐变语法、类型选择器改义或 JUCE 版本兼容宏。允许为属性观察安全修改**本地** `StyleSheet` 的内部计算状态，不把“不移植样式层”误解为“不得适配其消费者”。
- **不增加动态 DOM 契约**：当前生产路径是 C++ 构建布局树、解释后更新属性；结构替换经 `ViewHost::loadLayout()` 重建。精简不再维护没有生产调用的 View/makeView、alias、自定义 decorator、树结构监听与声明式事件入口；保留解释构建所需的组件树操作。
- **显式区分变化类型**：`Length` 收敛到 px/% 是删除无产品消费者的 em/rem；Flex gap 开始生效是绑定弹窗的行为修复。不得再承诺“除 em/rem 外全部零行为变化”，也不得把全局禁止 flex-shrink 隐藏在性能移植中。

### 1.2 复审裁决与证据

| 原规划问题 | 真实代码依据 | 修正 |
|---|---|---|
| 以内部引用或工厂注册证明“不可精简” | `MainComponent::wirePluginPanel/wireControlsPanel`、`SettingsComponent`、`JiveModalDialog` 使用原生 JUCE 回调；`on-click/on-change` 树事件没有业务订阅。Label 仅剩工厂/解释器注册，View 仅剩 PassiveView 与内部扩展链。 | 删除的是完整未用能力闭包，不是孤立删头文件；原生回调、组件工厂和状态同步必须保留。 |
| StringStreams 被 Fill/Gradient 引用，所以必须保留 | 这些引用用于诊断 `operator<<`，不是 `VariantConverter`、渐变计算或绘制。 | 同时删除无消费者的诊断流重载与 include，保留 Fill/Gradient 的真实功能。 |
| 删除 Remover 必须新增三个 Interpreter 回调和 `ViewHost::listenTo` | 生产没有调用 `listenTo`；上游 Interpreter 也没有 `valueTreeChildOrderChanged`。JUCE `ValueTree::addListener` 绑定具体 handle，拷贝不复制 listener，handle 析构会注销。 | 删除未用结构监听链，不新增动态树引擎；撤销“副本 observedTree 可解绑另一 handle”和“现行 reset 必然悬空”的错误推断。 |
| transition 快路径消除临时 listener | 当前 `getTransition()` 的临时 `TransitionsProperty` 已为 `Responsiveness::ignoreChanges`。 | 收益是少建临时属性对象、少查询/持有/析构，不是少注册 listener；标识符必须先于使用它的提交引入。 |
| Object 不递归转换，且已证明发生引用计数提前释放 | 当前 `replaceDynamicObjectsWithJiveObjects()` 已递归对象属性及数组；JUCE `var(ReferenceCountedObject*)` 会增引用。实际缺口是解析用构造函数没有初始化 `internalListener` 和 child-parent 关系。 | 修复解析对象的观察/父链；同步更正 `StyleCatalog.h/.cpp` 的错误所有权注释，不顺手删对象复用缓存或现有存活容器。 |
| `observeHeldObject()` 可独立搬入 | 本地 `StyleSheet::style.onValueChange → updateStyles → applyStyles` 会向同一个样式对象写 `calculated-*`；上游 held-object 观察会把这些写入再次传给 `onValueChange`。 | 对象观察与本地样式计算状态隔离必须同一原子切片，否则会引入同步递归及共享样式间串扰。 |
| Length 值类型化自然缓存解析，BlockItem 没有属性 | `Property::get()` 仍逐次 fromVar；BlockItem 的 `x/y/centreX/centreY/width/height` 都是继承 Property 的 Length。 | 不宣称解析缓存；一次更新所有保留成员类型、转换器、插值、回调与调用点，不能只改 `toPixels()`。 |
| 统一 `minWidth=width/minHeight=height` 是无行为变化修复 | `LayoutModel` 的 controls-panel 显式允许在 height 与 min-height 之间收缩；keyboard-area 也有独立 min/max。 | 不移植该全局规则，不覆盖显式 min/max；维持主窗口可压缩布局。 |
| 写构建标记、增加 dirty flag 就能只测一次 | `Text::updateTextComponent`、`CommonGuiItem::boxModelChanged/childrenChanged` 另有传播入口；Flex/Grid 的同名 override 会截断基类通知。 | 迁移实际生产与消费路由，构建结束只设一个根级收敛入口，不保证每个节点仅测一次。 |
| Flex gap 只是无消费者的新语法 | `KeyBindingEditDialog.cpp` 的 `custom-colour-palette` 已声明 `gap="4"`，当前 Flex 不解释它；Clear 按钮还另有左 margin。 | 单列行为修复，消除双重间距并验证色板完整占宽，不能混进缓存提交。 |
| Widget 迁移可选但全节点 listener 必须降到固定个位数；目录拆分取决于速度收益 | 保留的 widget、Child、Flex/Grid/Block、样式属性仍有监听；目录整理解决维护边界，不直接提高运行速度。 | 不设未经量测的 listener 总量/降幅承诺；覆盖保留属性闭包，目录整理作为本阶段必做的独立结构步骤。 |

### 1.3 实施顺序与任务

顺序为 **39.5-1 → 39.5-2 → 39.5-3 → 39.5-4 → 39.5-5**。每个子提交须包含完整调用闭包且可编译；不以半迁移状态交付。阶段内可先小步审查，构建/格式/测试在集成边界统一执行，不每改一个文件就全量运行。

#### Phase 39.5-1：先建立基线，再精简未用能力闭包

- **先量测、后修改**：记录主窗口、设置、绑定/长消息弹窗的实际组件几何、首次解释与典型属性更新；量测 listener 注册/通知、布局重测与分配热点。采用本地一次性计数/日志，不修改 JUCE 子模块，不新增性能框架；记录相同 Windows Debug 条件及重复运行差异，不写未经证实的百分比。
- **Find / StringStreams / Label**：删除 `jive_Find.*`；删除 `jive_StringStreams.*` 及 Fill/Gradient 等无消费者的诊断流声明、定义和 include；删除 `jive_Label.*` 及其工厂/解释器/聚合头注册。保留 JUCE 原生 Label、Fill/Gradient 的转换与绘制，不因名字相同误删原生控件。
- **Event 与 ComboBox**：整闭包移除 `jive_Event.h`、widget 内 `Event onClick/onChange`、构造/trigger 路径和无用触发辅助函数；保留 `buttonClicked/sliderValueChanged/comboBoxChanged` 中真实的 toggle/value/selection 同步及 JUCE 原生回调。ComboBox 的条目由业务 `addItem/clear/setSelectedId` 管理，删除无生产消费者的 Option/Header/Separator 树模型、选项监听/重建和 selected-child 联动，不误删原生 `onChange` 或必要的 selected 属性桥接。
- **View 与节点生命周期**：GuiItem 直接持有 state，移除 View/PassiveView/makeView、view-object/make-view、仅服务该链的 wrapper、`setupItemsRecursive` 与 ancestor View 工厂回退；保留 `ComponentFactory` 和 `ViewHost::configureComponentFactory`。同步删除无调用的 alias/custom-decorator 扩展、Interpreter 的 `listenTo/observedItem` 及结构回调、各 decorator 重复分配的 Remover；不补通用增删/重排/redirect 功能。
- **一起清理全部当前调用方**：更新共享 CMake 源列表与聚合头；删除 `StyleCatalogTest.cpp::testComboOptionsEnabled` 这类仅人工构造已淘汰 Option 协议的用例，不为留测试保留死路径。保留原生 ComboRebuild/FilterCombo/PresetCombo、真实按钮动作与 teardown 的行为回归。

**出口**：先证明解释、业务点击、下拉重建/选中、滑块同步与多次 load/reset/move 仍有效；旧布局 ValueTree 被外部继续持有时销毁宿主不留下活动回调；没有以新动态树接口替代删掉的旧能力。

#### Phase 39.5-2：属性、对象与 Length 的完整安全切换

- **标识符先行**：引入当前保留闭包实际需要的 `jive::ids`，先于任何 `ids::transition()` 使用；四个 `ideal-*` / `component-*` 值保持现名。不搬 View/文件监控等已裁掉的 identifiers，不全仓机械替换。
- **Property 与 transition**：移植 PassiveProperty/handlePropertyChange/短路 forwardPropertyChange，并保留继承、累积、Object source 与动画语义。无 transition 快路径只负责减少临时工作；替换/删除 transition 的升级须同时带入拥有容器的强引用、旧监听解绑、缓存清理和新定义衔接，不能只复制裸指针代码。正常值变化允许回调销毁 owner 后立即返回；不为无实际消费者的复制/赋值能力新增泛化实现。
- **删除通知必须覆盖消费者**：采用移除属性也发通知的语义，同提交更新 BlockItem 的互斥 x/centre-x、y/centre-y 回调，只有当前坐标仍 exists 才清另一项，防止删除回调反过来清掉刚设置的新坐标。验证删除/替换 transition 和清空 held object，不只验证设置新值。
- **Object 与本地 StyleSheet 原子适配**：修复解析构造时的 nested Object 观察和 parent 初始化；直接子 Object 的订阅在替换/移除/析构时成对解除，不能照搬上游未完整解绑的 InternalListener；不把共享样式强行改成多 parent 所有权。数组维持现有递归转换，不扩展为任意对象图或数组元素冒泡框架。引入 `observeHeldObject()` 时，将本地每个 StyleSheet 的 `calculated-*` 隔离为实例私有计算状态，不再写回共享声明式样式对象，并正确同步有效的 transition 定义/来源；不能用吞掉所有通知或重入告警掩盖循环。验证实际 hover/active/disabled/checked、样式对象替换与清空、共享规则及旧对象留存后的析构；更正 StyleCatalog 所有权注释，保留现有缓存/存活策略，除非另有直接证据要求调整。
- **Length 单次 cutover**：同时修改 Length 值类型、VariantConverter、Interpolate，以及 BoxModel、CommonGuiItem、ContainerItemChild/Pimpl、BlockItem、Slider、ComboBox、ProgressBar 的全部 Length 成员、构造与使用；可先用主动 `Property<Length>`，再于 39.5-3 合并监听。真正的像素换算在 BoxModel/ContainerItemChild/BlockItem；GridVariantConverters 没有此依赖，不顺带修改。auto 仍由 Property 缺省/auto 字符串表达，不能把 auto 当绝对零；宽高/位置分别使用正确轴，Block 坐标动画继续消费 `calculateCurrent()`，不改为立即读目标 `.get()`。验证百分比随父尺寸、dummy/real 测量、同单位插值及单位/auto 切换，避免把上游同单位断言当作所有过渡都安全。

**出口**：本地样式重算没有 `calculated-*` 递归或跨组件污染；解析对象的父链/事件确实有效；px/%/auto、坐标互斥及动画行为经实际消费者验证。Object/held-object 改动与 StyleSheet 适配不可拆成一个可触发递归的中间提交。

#### Phase 39.5-3：保留节点的集中属性分发与 BoxModel 去污染

- **覆盖完整保留面**：BoxModel、CommonGuiItem、ContainerItem/Child 查询属性、Flex/Grid 容器与子项、BlockItem、Button/Slider/ComboBox/ProgressBar/Text 中实际保留的属性分别改为 PassiveProperty，由 owner 集中分发；只读临时查询不注册监听。不能把 widget 留作可选，再承诺全节点 listener 总量。
- **复用现有回调**：优先复用 `onValueChange/onTransitionProgressed` 与固定分发，不照搬每节点动态 `vector<pair<Identifier,std::function<...>>>` 表。每个 passive 成员都须有明确的通知路由，包括 width/height/display/opacity、`transition` 定义广播，以及 Button text 的子树累积语义；禁止只更新 opacity 而漏掉其余 transition 状态。
- **默认值不作伪契约**：本轮以保留控件实际默认行为为准；Button padding/min-size、display、Text 对齐等已有内部读取依赖的写回先保留。`toggle-on-click` 是布局显式声明，不是“上游默认值改变会破坏测试”的证据。不做全量 getOr 迁移，也不新增默认属性赋值回读测试；直接受影响的纯实现断言删除，不重新钉住树的 incidental 内容。
- **BoxModel**：移除 `box-model-callback-lock` 属性，改为实例局部状态；明确解释节点由 CommonGuiItem 唯一拥有 BoxModel，其余通过引用使用，不增加共享锁表。核对实际重入/嵌套锁范围，不宣称上游 bool 解决了嵌套锁。`getParentBounds()` 直接读父节点尺寸，避免临时主动 Property；保留尺寸 transition source 的绑定。
- **约束与缓存接口**：为真正影响父布局的输入维护 constraintsVersion，锁内修改也不能丢失版本失效；保留 margin 动画、删除、百分比父尺寸、方向和函数式 ideal-height 的独立失效路由。引入 `getExplicitConstraints()` 时明确处理当前无 max 的负值哨兵，不把它直接求 rectangle intersection，也不顺手把所有旧 max 语义改成上游上限。

**出口**：保留生产树的注册/通知实测下降，无固定 listener 数字或“零开销”承诺；属性/动画变化仍到达真实控件。属性传播验证必须走 `setProperty` 等被改路径，不能只用 `ViewHost::setEnabled/setVisible` 的原生直接赋值掩盖漏分发。

#### Phase 39.5-4：布局收敛、缓存与明确分离的 gap 修复

- **4a 构建与脏标记原子切片**：一起迁移 Interpreter 构建守卫、ContainerItem dirty/约束测量、Child 通知、CommonGuiItem 的尺寸/childrenChanged 传播、Text/widget 的构建期重测入口，以及 Flex/Grid override 对 `ContainerItem::valueTreePropertyChanged` 的显式转发。构建期间只标记/延迟向上重测，结束后进入根级收敛；不承诺全树每节点只测一次。失败/退出不能残留构建中状态。
- **门面与测量出口同步迁移**：`ViewHost::fitToContent()` 保留按给定宽度测高、读取 ideal-height、再用子节点 bottom/margin 与根 padding/border 校正高度的契约；`relayoutContainer()` 必须消费相关 dirty 状态，不能仅调用旧 layOutChildren。同步所有当前调用方并删除旧 `updateIdealSizeUnrestrained/WithinConstraints` API，不留兼容包装。BlockContainer 的 ideal-size 语义保持现状，不顺带扩成按子节点包围盒定尺。
- **4b Item 缓存**：保留 bounds/strategy 键，补齐方向、constraintsVersion、flex/grid 属性、margin tick、父尺寸引起的百分比约束和文本测量失效；方向在查缓存前比较，文本即使自然宽度未变也要通知 `idealSizeChanged()`。采用简单有界存储与数值 hash，避免字符串化键和反复无谓分配；容量是实现参数，不把上游的 8/16 项写成产品契约。
- **4c TextLayout 缓存**：先复用测量，再接入绘制共享，分别验证；首次采用相同约束宽度的复用，不直接启用上游跨宽度启发式。缓存源包含完整 attributed runs 的 range/font/colour、对齐、方向、换行与行距；`setTextColour`、`clearAttributes`、`append` 以及保留的父 Text 合成源同样必须失效。纯颜色变化不强迫容器重测。缓存引用不能跨下一次 `getTextLayout()` 或 setter 保存。Canvas/direct paint 是实现选项，不是产品契约；默认保留当前绘制层级，不为追上游强删，也不能以缓存替换改变 clip/z-order。比较新鲜布局与缓存的中英长文本、换行、centred/right/RTL、多段落和颜色绘制。
- **不得偷带约束改义**：不全局执行上游的 `minWidth=width/minHeight=height` 或未核实的 clamp 规则。保留 controls-panel 的显式收缩范围与 keyboard-area 的独立 min/max；具体控件真有挤压缺陷时按消费者单独修复，不改整个 Flex/Grid 的语义。
- **4d Flex gap 独立行为修复**：保留上游半 margin/扩展 bounds 的实现思路，与真实 Grid gap 语法一致；单独验证绑定弹窗 `custom-colour-palette`。其现有宽度 220，8 个 18 宽色块、44 宽 Clear 按钮和相邻 4 间距恰好占满；移除 Clear 另设的 4 左 margin，避免新 gap 与旧 margin 双计。验证无外围 gap、按钮不被挤压/裁剪、色板选择与 Clear 仍有效；不趁机全应用 margin→gap 重写。

**出口**：首次解释、相同 bounds 改方向/文本/字体、收窄再放宽、插件/QWERTY 折叠、筛选下拉宽度与长消息定尺均正确；gap 修复有独立前后几何及真实弹窗证据。4a 是完整消费链的切换，4b/4c/4d 分开验证，不能打成一个“无行为变化”大提交。

#### Phase 39.5-5：目录归位与最终集成

- **必须完成，但只改结构**：`values/` 放标识符、Property/Object、转换器与值/过渡基础工具；`geometry/` 放 Length/BoxModel/Orientation/BorderRadii/Bezier；`style/` 放本地样式、选择器、交互状态、Fill/Gradient/Colours/FontUtilities 与背景绘制；`layout/` 放 Interpreter/GuiItem/Factory、容器/子项与保留控件。聚合头保留为内部入口。这是四个职责目录，不是新 CMake module，也不伪称严格单向依赖分层。
- 通过 LSP 文件移动/引用工具同步路径；更新跨目录 include、聚合头、CMake 的 app/tests 共用源列表和必要的 include 根，不靠增加四个裸搜索目录掩盖旧 include。只刷新 WSL configure/编译数据库，构建与测试仍在 Windows；与裁剪、API 改名、逻辑优化分开提交。
- 在实现获得烟测证据后最小同步 [UI 功能文档](../reference/features/declarative-ui-and-theming.md)、直接受影响的 ADR-014 现行维护边界和英文 `CHANGELOG.md`；`THIRD-PARTY-NOTICES.md` 保留原内化来源，追加本次 v2 **部分移植**提交来源与 MIT 声明，不将所有内核伪标成同一 v2 快照。当前只修规划，不提前把这些实现记成完成。

### 1.4 验证、失败处置与完成条件

- **Windows Debug app 和 tests 都要构建**：`./scripts/dev.sh win-build` 不代替确认测试目标已构建；镜像树按 [quickstart 的 Windows Debug 单元测试](../guides/quickstart.md#windows-debug-单元测试) 启用 `BUILD_TESTS`、构建 `devpiano_tests`，再运行默认 `ctest --test-dir build-win-msvc --output-on-failure`。不在本地 WSL 跑产品测试，不默认 Release。
- **定向命令按注册名，不按文件名**：测试可执行文件用 `--category DevPiano/UI`，或用 `--name` 单独选择 `LayoutGoldenTest`、`StyleCatalog`、`ViewHost`、`JiveModalDialog`、`SettingsLayoutModel`、`JiveTeardownOrder`、`ComboRebuild`、`FilterComboDefault`、`FilterComboVisibilityAdaptation`、`PresetComboRebuild` 等实际注册名。CTest 当前只注册总测试入口，不能用 `ctest -R StyleCatalogTest` 冒充跑了该 suite；核对日志中确有执行。
- **风险回归而非重钉实现**：复用现有几何/原生交互用例；为样式观察递归、transition 删除/替换、坐标互斥、缓存失效和色板 gap 等可信消费者边界保留必要回归。所有永久 UI 测试纯内存、离屏、无原生顶层窗口，异步按钮显式 drainMessages；不复制上游 `#if JIVE_UNIT_TESTS` / `JIVE_TRACE`，不保留已裁协议或 wording/wiring 测试。
- **真实表面独立验收**：按 [Windows MCP 工作流](../guides/windows-mcp-omp.md) 在主会话操作镜像 Debug 产物；验证窗口身份和前后 `mode='auto'` 快照/截图，覆盖主窗口收缩、设置语言切换、绑定色板、长消息、折叠、样式状态和正常关闭，不拿单测代替真实表面，也不认证硬件/商业插件矩阵。
- **集成门禁**：格式在本轮集成后统一执行，clang-tidy 仅在迭代边界全量执行；源文件增删/移动后刷新编译数据库。一次性计数/烟测脚手架在证据记录后移除。记录同配置基线对照、波动和未测边界，不把规划中的收益当量测结果。
- **失败处置**：按完整依赖闭包修复或回退该逻辑批次，不留下新旧 API/shim/双路径；不要为保留已证明有风险的半套上游实现放宽回归。文本跨宽度复用等非必要启发式可以不引入；目录拆分是维护目标，不以 UI 加速幅度决定是否完成。
- **完成条件**：39.5-1 至 39.5-5 全部按上述闭包交付，生产消费者、Windows Debug app/tests、默认 CTest、定向风险路径和真实 UI 证据齐备后才记完成。本次规划复审不等于启动实现、构建通过或性能认证；Phase 39/40–42 的既有结论不变。

### 1.5 Phase 39.5-1 修改前基线(2026-10-23)

在 Windows Debug(MSVC、`./scripts/dev.sh win-build`)上以临时探针 `JiveBaselineProbe` 运行 5 次;探针、`DEVPIANO_JIVE_BASELINE` 宏与 `jive_BaselineProbe.h` 在证据记录后随 39.5-1 一并移除,不进入永久测试。耗时取 5 次中位数并给出最小–最大;结构计数(Property/GuiItem/Remover、通知次数)在各次运行间完全一致,分配次数有少量波动。计时已用 `std::chrono::steady_clock` 与 QPC 对照校准。

| 场景 | 解释 ms | 常驻 Property | Property 构造 | 分配次数 | 分配字节 | GuiItem | Remover | Box/State 监听 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 主窗口 1280x720 | 1.299(1.269–1.366) | 9067 | 41229 | 283,338–283,959 | 12.31–12.34 MB | 511 | 511 | 323 / 83 |
| 主窗口 1920x1080 | 1.306(1.278–1.321) | 9067 | 41229 | 283,338 | 12.31 MB | 511 | 511 | 323 / 83 |
| 设置 680x900 | 1.166(1.145–1.225) | 12004 | 44733 | 214,602–214,838 | 11.88–11.90 MB | 720 | 720 | 446 / 134 |
| 长消息确认弹窗 | 0.057(0.055–0.061) | 654 | 2389 | 17,156–17,169 | 1.26–1.27 MB | 37 | 37 | 23 / 6 |
| 键位绑定弹窗 | 0.168(0.162–0.184) | 2622 | 9086 | 38,830–38,858 | 2.27–2.28 MB | 154 | 154 | 100 / 28 |

“Box/State 监听”是 `BoxModel::Listener` 与 `state.addListener` 的注册数,与每个 `Property` 自身的监听、每个 GuiItem 的 Remover 分开统计,不是 listener 总量。主窗口一次解释同时发生 30,161 次 `Property::get`、6,909 次 `set`、8,037 次有效通知与 29,311 次 transition 查询。

| 二次布局 / 更新 | ms | 分配 | 临时 Property | 通知 | transition 查询 | 布局次数 | ideal 重测 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 主窗口 `setBounds(1280x720)` | 0.043 | 5,698 | 1,757 | 84 | 1,291 | 29 | 3 |
| 50 轮文本/高度/启用更新 | 0.152 | 37,054 | 2,536 | 727 | 2,258 | 105 | 54 |
| `refreshTitles()` | 0.003 | 67 | 1 | 1 | 1 | 0 | 0 |

修改前几何(主窗口 1280x720):`main-area` 1280x696@(0,0)、`header` 1248x36@(16,16)、`plugin-panel` 1248x42@(16,62)、`controls-panel` 1248x220@(16,116)、`custom-keyboard` 1248x146@(16,540)、`status-bar` 1280x24@(0,696)。绑定弹窗根 460x282;`custom-colour-palette` 220x24@(224,188),其中 Clear 44x22@(372,189)——Flex gap 当前不生效,Clear 的 4px 左 margin 是唯一间距来源。

未测量:JUCE 内部 listener 容器的真实容量、音频线程行为、物理设备与插件矩阵。上述数字是同一镜像树、同一 Debug 配置下的相对基线,不写成产品性能承诺。

**39.5-1 精简后复测(同一探针、同一配置)**:GuiItem 数、全部几何、二次布局计数与 50 轮更新路径完全不变;差异仅来自被删除的未用闭包。

| 场景 | Remover 注册 | 常驻 Property | state 监听 | 解释分配 | 分配字节 | 解释 ms |
| --- | --- | --- | --- | --- | --- | --- |
| 主窗口 1280x720 | 511 → 0 | 9067 → 9043 | 83 → 80 | 283,338–283,959 → 282,286–282,900 | 12.31–12.34 → 12.25–12.29 MB | 1.299 → 1.261 |
| 设置 680x900 | 720 → 0 | 12004 → 11947 | 134 → 115 | 214,602–214,838 → 212,809–213,045 | 11.88–11.90 → 11.80 MB | 1.166 → 1.126 |
| 键位绑定弹窗 | 154 → 0 | 2622 → 2615 | 28 → 27 | 38,830–38,858 → 38,515–38,543 | 2.27–2.28 → 2.26 MB | 0.168 → 0.165 |

`state` 监听减少量与各布局的 ComboBox 数一致(主窗口 3、设置 19、绑定弹窗 1),即删除的是 `Option/Header/Separator` 重建链;Property 与分配的下降来自 `Event`、View 链和选项属性。二次布局(0.043→0.049 ms)与 50 轮更新(0.152→0.154 ms、分配 37,054 不变)属于运行波动,不记为改进。探针在该复测后删除。

### 1.6 Phase 39.5-1 实施结果与验证

- **删除的未用闭包**:`jive_Find.*`、`jive_StringStreams.*`、`jive_Label.*`、`jive_Event.h`、`jive_View.*`、`jive_ReferenceCountedValueTreeWrapper.h`;Fill/Gradient/BorderRadii 的无调用诊断流;Interpreter 的 alias/customDecorator/listenTo/结构回调/`setupItemsRecursive`;GuiItem 的 View 链与 Remover;Button/Slider/ComboBox 的声明式 Event;ComboBox 的 Option/Header/Separator 树模型、选项监听与重建;`Button::updateTriggerEvent` 死声明。
- **保留**:`ComponentFactory`/`ViewHost::configureComponentFactory`、原生 JUCE 回调、ComboBox 原生条目管理与 `selected` 属性桥接、Button toggle/radio 同步、Slider 值回写、样式与过渡、Text/Block/Flex/Grid 布局。
- **验证**:Windows Debug app 与 `devpiano_tests` 构建通过;默认 `ctest` 通过(约 40 s);定向 `LayoutGoldenTest` 391、`JiveModalDialog` 417、`StyleCatalog` 176、`JiveRender` 13、`PluginPanelToggle` 34、`ViewHost` 35、`SettingsLayoutModel` 66、`JiveTeardownOrder` 24、`ComboRebuild` 7、`FilterComboDefault` 8、`FilterComboVisibilityAdaptation` 14、`PresetComboRebuild` 9 项断言全部通过;主窗口/设置/弹窗几何与精简前逐像素一致。`ViewHost` 新增“重载、移动、宿主销毁后继续变更外部布局树”的生命周期回归与“滑块原生值回写布局属性”回归。
- **测试调整**:删除只构造已淘汰 Option 协议的 `testComboOptionsEnabled`;删除变为恒真的“无 Option 子节点/无 selected”断言;4 处 `expectEquals<juce::Colour>` 改为 `expect(...)`,不再依赖已删除的诊断流。原生下拉重建与二次选中回归保留。
- **未覆盖**:windows-mcp 真实表面验收、语言切换、绑定色板、真实插件与音频设备;这些仍按 1.4 在后续任务/阶段出口执行。39.5-1 不宣称 listener 总量或性能收益,探针已移除。

### 1.7 Phase 39.5-2 实施结果与验证

- **标识符**:新增 `jive_Identifiers.h`,当前只收录 `transition`、`ideal-width/height`、`component-width/height`;四个内部布局属性保持无前缀原名,不做全仓机械替换。
- **Property**:`getTransition()` 直接从源读取或解析定义并持有 `Transitions` 强引用,不再创建临时 `Property`;transition 属性变更走 `transitionsChanged()`,从旧过渡当前值继续且带空值守卫;`calculateCurrent()` 在端点缺失时回退 `get()`。替换/移除 transition、属性被移除后再出现均不再断言或悬空。
- **删除通知**:`valueTreePropertyChanged`/`propertyChanged` 不再因属性不存在而提前返回;`BlockItem` 的 x/centre-x、y/centre-y 只在当前坐标仍存在时清除另一项。
- **Object**:解析构造改为先转换嵌套对象再创建监听器,嵌套对象获得正确父链;InternalListener 记录直接嵌套集合,替换/移除时成对解绑并清除父指针;数组维持递归转换,未扩展任意对象图。
- **StyleSheet**:每个实例持有私有 `calculatedState` 与克隆的过渡定义;`getBackground()` 等不再把 `calculated-*` 写回共享样式对象。`observeHeldObject()` 随样式对象替换/清空重新绑定。`StyleCatalog` 所有权注释同步更正。
- **Length**:收敛为 `{ float value; Unit px/% }` 值类型,删除 em/rem 与 `isPixels/isPercent/isEm/isRem`;`Interpolate<Length>` 在单位不同时保持起点至过渡完成;BoxModel、ContainerItemChild、BlockItem、CommonGuiItem、Slider、ComboBox、ProgressBar 全部改为 `Property<Length>`。动画消费仍走 `calculateCurrent()`。
- **验证**:Windows Debug app/tests 构建通过;默认 `ctest` 通过(41.6 s);`JiveCoreValues` 42 项断言覆盖 px/%/auto、单位切换、transition 删除/替换、属性删除通知、Block 坐标互斥、解析父链与成对解绑、共享样式下的 calculated 隔离与替换/清空;既有 `StyleCatalog` 176、`ViewHost` 35、`LayoutGoldenTest` 391、`JiveRender` 13、`JiveModalDialog` 417、`SettingsLayoutModel` 66、`JiveTeardownOrder` 24、`ComboRebuild` 7、`FilterComboDefault` 8、`PresetComboRebuild` 9、`PluginPanelToggle` 34 全部通过。
- **保留给 39.5-3**:属性仍为主动监听;本阶段只提供 `PassiveProperty`/`handlePropertyChange`/`forwardPropertyChange` 原语,尚未合并监听。真实 windows-mcp 表面验收仍按阶段出口执行。

### 1.8 Phase 39.5-3 实施结果与验证

- **完整保留面改为被动属性**:BoxModel、CommonGuiItem、ContainerItem、ContainerItem::Child(Pimpl)、Flex/Grid 容器与子项、BlockItem、Button、Slider、ComboBox、ProgressBar、Text 的保留属性均改为 `PassiveProperty`,由各 owner 的单个 `ValueTree::Listener` 固定转发;现有 `onValueChange`/`onTransitionProgressed` 继续承担具体行为,未引入动态属性表。
- **容器继承钩子**:`ContainerItem` 与 `Child` 以 protected 方式继承监听器并提供可覆写转发入口,派生类调用基类转发后再处理自身属性,避免重复继承同一 Listener 造成歧义。
- **BoxModel 去污染**:删除 `box-model-callback-lock` 树属性,改为实例 `bool`;`getParentBounds()` 直接读取父节点的 `component-width/height`,不再构造两个临时 `Property`;新增 `getConstraintsVersion()`,尺寸/min/max/ideal/margin 变更(包括锁内)都会递增;新增 `getExplicitConstraints()`,未设置 max 时保持无上限,不把 `-1` 哨兵带入矩形交集。
- **Text 例外**:`line-spacing` 仍依赖祖先继承,保留其主动监听;其余 Text 属性转为被动。`StyleSelectors`、`StyleSheet` 的计算属性不在本任务范围,仍为主动监听。
- **实测(临时探针,加载完整样式)**:主窗口 1280x720(122 个实际节点)Property 监听注册 9043 → 3957(−56%);设置 680x900(166 个节点)12004 → 3799(−68%)。owner 状态监听为 389/554(约 3.2/节点),BoxModel 监听 122/166(1/节点);不设固定 listener 总数或零开销承诺。探针与计数宏在记录后移除。
- **验证**:Windows Debug app/tests 构建通过;默认 `ctest` 通过(28.0 s);`JiveCoreValues` 64 项断言全部通过,新增 `setProperty` 驱动路径覆盖 enabled/opacity/toggled/width、Text、Slider、ComboBox、ProgressBar;既有 `LayoutGoldenTest` 391、`StyleCatalog` 176、`ViewHost` 35、`JiveRender` 13、`JiveModalDialog` 417、`SettingsLayoutModel` 66、`JiveTeardownOrder` 24、`ComboRebuild` 7、`FilterComboDefault` 8、`FilterComboVisibilityAdaptation` 14、`PresetComboRebuild` 9、`PluginPanelToggle` 34 全部通过。
- **未覆盖**:windows-mcp 真实表面验收、语言切换与绑定色板仍按 1.4 在阶段出口执行;本任务不重测音频/插件矩阵。

### 1.9 Phase 39.5-4a/4d 实施结果与验证

- **构建守卫**:`Interpreter::interpret` 用 RAII 在节点上写入 `setup-in-progress`,装饰/子节点构建期间只标记失效;结束(含异常路径)清除。顶层解释结束后只做一次根容器 `updateIdealSize()` 收敛。
- **脏标记与约束测量**:`ContainerItem` 新增 `idealSizeIsOutOfDate` / `childrenMayBeOutOfDate` / 测量时约束快照与 `idealSizeWasLimitedByConstraints`;`updateIdealSize(informParent, measureChildren)`、`invalidateIdealSize()`、`updateIdealSizeIfOutOfDate()`、`updateChildrensIdealSizesIfOutOfDate()`、`refreshLayout()` 取代旧的 `updateIdealSizeUnrestrained/WithinConstraints`。构建期 `CommonGuiItem::boxModelChanged/childrenChanged`、`Child::componentMovedOrResized` 与 `Text` 的父级重排均按守卫延迟。
- **门面同步**:`ViewHost::fitToContent()` 按给定宽度设置组件后调用 `updateIdealSizeWithinContentBounds()`,保留 ideal-height 与子节点 bottom/margin、根 padding/border 校正;`relayoutContainer()` 改为经由容器 `refreshLayout()` 消费脏状态,不再只调用 `layOutChildren()`。
- **Flex gap**:`FlexContainer` 支持 `gap`(半 margin + 布局区扩展);`custom-colour-palette` 的 220px 现在精确容纳 8 个 18px 色块、7 个 4px 间距与 44px Clear,Clear 的最右边界与色板右边界重合;删除 Clear 重复的 4px 左 margin。未做全应用 margin→gap 重写。
- **验证**:Windows Debug app/tests 构建通过;默认 `ctest` 通过(25.9 s);`JiveCoreValues` 71 项(新增色板 gap 几何:首个色块 x=0、第 8 个 x=154、Clear 176..220、单一 4px 间距);`ViewHost` 37 项(新增解释后全树无 `setup-in-progress` 残留);`LayoutGoldenTest` 391、`JiveModalDialog` 417、`SettingsLayoutModel` 66、`StyleCatalog` 176、`JiveRender` 13、`JiveTeardownOrder` 24、`ComboRebuild` 7、`FilterComboDefault` 8、`FilterComboVisibilityAdaptation` 14、`PresetComboRebuild` 9、`PluginPanelToggle` 34 全部通过。
- **后续**:4b 已在 1.10 完成;4c(TextLayout 缓存)仍未启动,真实 windows-mcp 表面证据仍在阶段出口补足。

### 1.10 Phase 39.5-4b 实施结果与验证

- **完整失效键**:`FlexItem` 的几何缓存以盒模型 `constraintsVersion`、父级 `flex-direction` 推导的 `Orientation` 共同判定失效;`GridItem` 以 `constraintsVersion` 判定。版本覆盖 width/height/min/max/ideal/margin 变化(含 margin 过渡 tick),方向变化不再复用旧轴约束。
- **文本测量**:`FlexItem`/`GridItem` 实现 `idealSizeChanged()` 清空缓存;`Text::updateTextComponent()` 仅在自然尺寸或 line-spacing 真正变化时 `updateIdealSize()`,否则只 `invalidateIdealSize()`,避免无变化重排。`GridItem` 额外监听自身 `ideal-width`,覆盖字体解析后宽度变化但状态未变的路径。
- **监听与内存**:移除每个 Flex/Grid item 额外的 `BoxModel::Listener` 注册,缓存写入前限制为 8 项,避免连续尺寸变化导致无界增长。
- **验证**:Windows Debug app/tests 构建通过;默认 `ctest` 通过(28.9 s);`JiveCoreValues` 85 项(新增方向、margin、文本换行、line-spacing 失效回归);`LayoutGoldenTest` 391、`JiveModalDialog` 417、`SettingsLayoutModel` 66、`ViewHost` 37、`StyleCatalog` 176、`JiveRender` 13、`JiveTeardownOrder` 24、`ComboRebuild` 7、`FilterComboDefault` 8、`FilterComboVisibilityAdaptation` 14、`PresetComboRebuild` 9、`PluginPanelToggle` 34 全部通过。负向验证:临时移除方向/版本判定后方向与 margin 断言失败;临时清空 `idealSizeChanged()` 后 line-spacing 断言失败,确认测试能拦截真实回归。
- **后续**:4c 已在 1.11 完成。

### 1.11 Phase 39.5-4c 实施结果与验证

- **测量复用**:`TextComponent::getTextLayout(maxWidth)` 按最大宽度缓存 `juce::TextLayout`;`Text` 的 ideal-height native function 与自然尺寸测量都改为读取该缓存,不再每次重新 shaping。
- **绘制共享**:Canvas 的 `onPaint` 使用同一 `getTextLayout(当前宽度)`,绘制与测量共享缓存条目。
- **复用与失效**:布局在 `word-wrap: none`、相同宽度,或换行结果不变(布局宽 ≤ 目标宽 ≤ 创建宽)时复用;文本、字体、对齐、换行、方向、颜色、行距或附录变化都会标记源失效。缓存最多 16 条,最近使用条目提升到末尾,满时淘汰最旧条目。
- **验证**:Windows Debug app/tests 构建通过;默认 `ctest` 通过(26.5 s);`JiveCoreValues` 93 项(新增同宽复用、窄宽复用、文本变化重建、离屏绘制有字迹);`LayoutGoldenTest` 391、`JiveModalDialog` 417、`SettingsLayoutModel` 66、`ViewHost` 37、`StyleCatalog` 176、`JiveRender` 13、`JiveTeardownOrder` 24、`ComboRebuild` 7、`FilterComboDefault` 8、`FilterComboVisibilityAdaptation` 14、`PresetComboRebuild` 9、`PluginPanelToggle` 34 全部通过。
- **后续**:39.5-5 已在 1.12 完成;真实 windows-mcp 表面验收仍待执行。

### 1.12 Phase 39.5-5 实施结果与验证

- **目录归位**:101 个 core 文件按职责移入四个子目录:`values/` 23 个、`geometry/` 10 个、`style/` 18 个、`layout/` 46 个;`jive_core.h`、`jive_components.h`、`jive_layouts.h`、`jive_style_sheets.h` 保留在 core 根作为聚合入口。未新增独立 CMake module,未改变任何布局/样式逻辑。
- **include 与构建**:core 内部与外部消费者统一改为从 `source/` 出发的显式 `UI/jive/core/<group>/...` 路径(内部 190 处、外部 22 处);CMake 的 42 个 core 源文件路径同步更新;保留既有 `source/UI/jive/core` include 根仅用于聚合头,未增加四个裸搜索目录。WSL configure/compile database 已刷新。
- **验证**:Windows Debug app/tests 构建通过;默认 `ctest` 通过(25.4 s);`LayoutGoldenTest` 391、`JiveModalDialog` 417、`SettingsLayoutModel` 66、`ViewHost` 37、`StyleCatalog` 176、`JiveRender` 13、`JiveTeardownOrder` 24、`ComboRebuild` 7、`FilterComboDefault` 8、`FilterComboVisibilityAdaptation` 14、`PresetComboRebuild` 9、`PluginPanelToggle` 34、`JiveCoreValues` 93 全部通过。
- **文档**:同步 `docs/reference/features/declarative-ui-and-theming.md` 的四目录职责、ADR-014 维护边界、`THIRD-PARTY-NOTICES.md` 的 v2 部分移植来源与 MIT 声明,以及 `CHANGELOG.md`。
- **未完成**:真实 windows-mcp 主窗口/设置/弹窗/绑定色板表面验收;按 1.4 完成条件,Phase 39.5 暂不记整体完成。

### 1.13 Phase 39.5 tidy 门禁、修复与阶段收口

- **全量扫描首轮**:`./scripts/dev.sh tidy --all` 发现四类问题:`Property::calculateCurrent` 缺少依赖模板 `template` 限定(43 个 TU 报同一处)、`Text::updateTextComponent` 条件分支重复、BoxModel 两处冗余 `float` 转换,以及 transition 替换路径的旧容器解引用分析告警。
- **修复**:
  - 依赖调用改为 `transition->template calculateCurrent<ValueType>()`。
  - `transitionsChanged()` 在替换前快照旧过渡的当前值/target,并提取 `readTransitions()`;替换后不再解引用旧容器内对象,同时保留“同一过渡不重启动画”的判定。
  - 合并 Text 的 setup/测量未变化分支;删除 BoxModel 的冗余转换。
- **验证**:按用户要求,修复后未重复全量扫描,只对受影响 TU(`jive_Text.cpp`、`jive_BoxModel.cpp`,覆盖 `jive_Property.h`)执行增量 `tidy`,通过;Windows Debug app/tests 构建通过;默认 CTest 通过(26.9 s);工作区无残留改动。
- **人工验收**:用户对主窗口收缩、设置语言切换、绑定色板、长消息、折叠与正常关闭等表面完成人工测试,确认无明显问题;本文不虚构逐项截图或硬件/商业插件矩阵结论。
- **阶段结论**:Phase 39.5 的 39.5-1 至 39.5-5、静态检查修复与用户人工表面验收均已完成,阶段收口;未覆盖项仅为真实硬件设备与商业插件矩阵,不属于本阶段退出条件。

## 2. 后续候选阶段(暂缓)

### Phase 40：同音弦耦合、双阶段衰减与 Una Corda 深化

深化已有同音弦振荡器与快/慢振幅包络，验证拍频、衰减和辐射之间的关系；区分振幅、功率、能量和时间常数，并验证两弦受击、第三弦被动响应的 Una Corda 拓扑。进入条件仍包含 Phase 39 的生产基线、有效频率、连续整音与状态/实时闭环；当前未满足，不启动。

### Phase 41：音板频变阻抗与开放弦共鸣校准

深化音板频变损耗、琴桥交界和开放弦/跨声部被动传能，区分输出低通、模态阻尼与弦向音板泄漏；共享琴桥只允许存在于固定 Instrument 内部，不扩展为通用路由。进入条件依赖 Phase 39 频率基线及 Phase 40 的主弦耦合/辐射定义。

### Phase 42：能量驱动张力非线性与泛音绽放

按实际衰减模态状态深化 Pitch Glide 与 Blooming，定义模态位移、权重和能量尺度；瞬态频率与泛音轨迹须由量测决定，并保持旁路、弱奏、Nyquist 边界和实时/离线对照。进入条件依赖 Phase 39 激振/频率校准及 Phase 40/41 的耗散与耦合尺度。

## 3. 恢复规则

- Phase 40–42 及“近期不排期”内容全部保留为候选；恢复须由用户明确决定，不因 Phase 39.5、R0–R11 归档、构建通过或时间推移自动启动。
- Phase 39.5 是 UI 基础设施阶段，不改变 Phase 39 的结论，也不作为 Phase 40–42 的进入条件。
- Phase 39 不补记为通过；已消费的独立验证、采集和判决不重放、不补批或放宽。未来外部参考校准须另立新任务。
- 实施时的构建、测试、实时/离线、文件保护和验证纪律仍按 [quickstart](../guides/quickstart.md)、[acceptance](../reference/acceptance.md) 与 [AGENTS.md](../../AGENTS.md)；默认仅 Windows Debug 验证，Release 仅在用户明确要求时执行。

## 4. 当前维护任务：统一有限导出尾音（E3）

- E1 已完成正常 take-end 释放的客观验证，保留未提交改动；E2 已完成有限窗口测量、方案比较与 E2 证据归档。两者不改变 Phase 40–42 的暂缓状态。
- E3 实现 `WavExportOptions::tailSeconds`：默认 6.0 秒、闭区间 0–30 秒；内置、插件及双层路径共享该选项；0 表示不追加尾音，文件长度仍由时间线确定性计算。
- 当前 UI 使用默认值，尚无时长控件；不新增全局设置、持久化字段、Take/Preset 格式、自动检测或淡出。有限窗口仍可能在超窗处截断，不承诺任意音源或插件的自然尾音完整。
- 验证边界：Windows Debug 定向测试、默认 CTest、格式与 diff 检查；不执行 Release、WSL 全量构建、物理声卡或商业插件矩阵。
- 与 Phase 39.5 的关系：不人为设置 E3 完成或再次申请并行许可的技术前置；如同轮实施，两者的改动与验证证据分开，尤其核对 WAV 进度内容使用 ViewHost 的生命周期。本次复审不推断 E3 是否已交付。

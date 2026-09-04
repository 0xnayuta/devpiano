# devpiano 品牌标识与设计规范 (Brand Guidelines)

> 用途：定义 devpiano 的视觉资产标准、Logo 几何体系、字标排版、双蓝色彩分工、图形语言与跨尺寸渲染规范。
> 归档位置：`assets/branding/`（源资产）与 `docs/reference/brand-guidelines.md`（规范）。

---

## 1. 品牌核心定位与设计哲学

devpiano 是一款聚焦现代电脑键盘演奏与纯 C++ 物理建模钢琴的专业数字乐器应用。

在视觉形象上，**彻底抛弃传统音乐软件的拟物套路**（不使用音符、五线谱、写实琴键、耳机、杂乱波形、拟物阴影与渐变）。其视觉语言追求**极简、高密度、严谨的现代工业与开发者工具质感**（类似 Braun、Teenage Engineering、Elektron 与现代代码编辑工具）。

---

## 2. Logo Symbol 构造与语义

Symbol 构建在 **64×64 绝对几何网格** 中，由四个关键视觉语义要素组成：

```text
┌────────────── 64 × 64 Grid ──────────────┐
│                                          │
│  [10..19]  [23..32]       ╭───────────╮  │
│    ▌         ▌            │           │  │  ← 上半 D (声学共振腔体)
│    ▌         ▌            │           │  │
│    ▌         ▌ ═ ═ ═ ═ ═  │     █     │  │  ← 4px 中央通道 + Performance Event Tick
│    ▌         ▌            │           │  │
│    ▌         ▌            │           │  │  ← 下半 D (结构基座)
│    ▌         ▌            ╰───────────╯  │
│                                          │
└──────────────────────────────────────────┘
```

1. **左侧双竖向柱体**：琴键黑白键的抽象离散投影（主干宽度 9px，间隙 4px）。
2. **右侧上下两段对称弧形**：代表声学系统共振腔与大写字母 `D` 的抽象解构。
3. **4px 水平中央通道（Channel）**：贯穿 Symbol 核心的信号流缝隙，象征数字键盘信号传输。
4. **离散性能事件 Tick（Event Tick）**：位于通道中央的垂直脉冲矩形，象征离散的 MIDI Note On/Off 与 Performance Event。

### 光学尺寸规范（Optical Sizing）

经过 16 / 20 / 24 / 32 / 48 / 64px 真实栅格化验证，Symbol 分为两级光学校准变体：

| 规格 | 适用尺寸 | Tick 参数 | 通道/间隙特征 | 对应资产 |
|---|---|---|---|---|
| **Master** | ≥ 32px | `width: 4px, height: 10px` | 保持标准 4px 精密通道 | `symbol-master.svg` |
| **Micro** | 16px ~ 24px | `width: 5px, height: 12px` (等效) | Tick 加粗、通道防粘连扩展 | `symbol-micro.svg` |
| **Monochrome** | 全尺寸 | 无 Tick (统一为镂空缝隙) | `currentColor` 自适应反色 | `symbol-mono.svg` |

---

## 3. Wordmark（字标）规范

字标采用 **全小写一体化 `devpiano`**，以 **Inter SemiBold** 为几何美学基底（Concept 1: Balanced Neutral），完全由纯矢量路径（Vector Contour）构建，不依赖客户端本地字体安装。

* **x-height**：32.0px（在 64px 网格体系中，Symbol 有效高度 44px : x-height 比值约为 `1.38`）。
* **主笔画粗细（Stem Width）**：`5.4px`，与 Symbol 的 9px 主干形成视觉平衡（比值 ~0.60）。
* **字距（Tracking & Kerning）**：
  * 基础间隙：`3.5px`
  * 特殊字偶微调：`p-i` 设为 `4.0px`，`i-a` 设为 `4.2px`，保障视觉律动平稳。
* **对齐锚定**：
  * Wordmark 垂直中心线严格锚定在 `y = 32.0`（与 Symbol 中央通道中心绝对齐平）。
  * 升部顶端（`d`）延伸至 `y = 8.8`，降部底端（`p`）延伸至 `y = 45.5`。

---

## 4. 组合标识（Lockup）规范

### 4.1 横向组合（Horizontal Lockup）

* **画布尺寸**：`336 × 64` (长宽比 5.25 : 1)。
* **间距（Clearspace）**：Symbol 右边缘至 Wordmark 左边缘为 `18px`（独立间距步长）。
* **适用场景**：GitHub README 顶部 Banner、官网/文档 Header、应用主界面标题栏。
* **对应资产**：
  * `assets/branding/logo-horizontal-dark.svg`
  * `assets/branding/logo-horizontal-mono.svg`

### 4.2 纵向组合（Vertical Lockup）

* **画布尺寸**：`288 × 160` (长宽比 1.8 : 1)。
* **对齐方式**：Symbol 水平居中置顶，Wordmark 水平居中置底；两者垂直净空为 `20px`。
* **适用场景**：GitHub Social Preview / Open Graph 卡片、产品关于弹窗（About Dialog）、发布海报。
* **对应资产**：
  * `assets/branding/logo-vertical-dark.svg`
  * `assets/branding/logo-vertical-mono.svg`

---

## 5. 色彩系统（Color Hierarchy）与双蓝职责

为避免品牌与功能界面色彩混淆，确立**双蓝职责边界**：

| 色彩角色 | 色值 (HEX) | 语义定位 | 应用边界与准则 |
|---|---|---|---|
| **Deep Obsidian** | `#0B0E12` | 极黑背景 | 应用主底色、App 图标底板、品牌物料暗黑画布 |
| **Acoustic White** | `#FFFFFF` | 主结构白 | 琴键结构主体、Logo 主字标、高亮正文（无色偏） |
| **Brand Primary** | `#4DA3FF` | 电气蓝 (Electric Blue) | **品牌专属象征色**：仅用于 Logo Tick、App 图标核心脉冲、品牌徽标与对外宣传物料 |
| **Product Accent** | `#00C8F0` | 电光青 (Cyan Accent) | **应用 UI 功能色**：用于 JIVE 旋钮高亮、输入框聚焦环、演奏通道激活、进度条填充 |
| **Structure Neutral** | `#2B2F38` | 结构边界灰 | 卡片边框、分隔线、网格参考线 |

> **禁止事项**：禁止将 `#4DA3FF` 滥用为日常按钮或常规表格文字；禁止将 `#00C8F0` 替代 Logo 内部的 Event Tick。

---

## 6. 图形语言（Graphic Language）

devpiano 的图形衍生规范严格围绕三大几何母题展开：

1. **4px 网格模数（Modular Grid）**：所有 UI 间距、卡片倒角（4px/8px）、线条粗细均基于 4px 整数倍延伸。
2. **离散事件脉冲（Discrete Event Tick）**：在文档示意图、数据流图或卡片状态指示中，使用类似 Logo Tick 的高对比度蓝色短方块表示离散时钟、状态触发或原子操作。
3. **琴键负空间（Piano-Key Negative Space）**：使用等宽垂直矩形阵列（如两长一短、三长两短的比例节奏）构筑背景水印与几何装饰。
4. **明确限制**：
   * **严禁滥用平滑声学正弦波（Waveform）** 或音频均衡器柱状图作为大面积装饰。
   * **严禁引入拟物化渐变、浮雕或五线谱音符**。

---

## 7. 资产清单索引

| 文件路径 | 尺寸/格式 | 用途 |
|---|---|---|
| `assets/branding/symbol-master.svg` | 64×64 SVG | 主符号矢量（深底 + 蓝 Tick），用于 ≥32px 场景 |
| `assets/branding/symbol-micro.svg` | 64×64 SVG | 微尺寸优化符号（增强 Tick），用于 16~24px 场景 |
| `assets/branding/symbol-mono.svg` | 64×64 SVG | 单色自适应符号（`currentColor`），黑白反色通用 |
| `assets/branding/logo-horizontal-dark.svg` | 336×64 SVG | 横向组合标识（暗黑版，带电气蓝 Tick） |
| `assets/branding/logo-horizontal-mono.svg` | 336×64 SVG | 横向组合标识（单色自适应） |
| `assets/branding/logo-vertical-dark.svg` | 288×160 SVG | 纵向居中组合标识（暗黑版，带电气蓝 Tick） |
| `assets/branding/logo-vertical-mono.svg` | 288×160 SVG | 纵向居中组合标识（单色自适应） |
| `assets/branding/app-icon/devpiano.ico` | 多尺寸 ICO | Windows 应用程序图标（16, 24, 32, 48, 64, 128, 256px） |
| `assets/branding/app-icon/icon-256.png` | 256×256 PNG | 高清应用图标底图 |
| `assets/branding/hero-cover.svg` | 1280×640 SVG | GitHub Social Preview / 宣传大图 |

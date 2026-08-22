# Phase 17：真实物理打击感钢琴音源重构（完成记录）

> 归档说明：记录 Phase 17 物理击打起音、非线性琴槌毛毡谱形、击弦点梳状滤波与双阶段衰减调优的完整实现与验证记录。
> 完成日期：2026-08-22
> 替代文档：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 背景与核心问题

在 Phase 12–14 建立的模态钢琴音源虽然具备了刚性失谐、双阶段衰减与同音拍频，但在听觉上偏向**提琴/拉弦乐器（Bowed String）**而非**钢琴击打乐器（Struck String）**。

### 四大声学根因剖析：
1. **$1/n$ 锯齿波频谱**：谐波幅度按 $1/n$ 递减在声学物理上是小提琴亥姆霍兹擦弦运动的特征波形；
2. **$10\text{ ms}$ 起音门控**：平滑渐入抹平了琴槌瞬间敲击的爆发力，产生了拉弓渐入感；
3. **缺失琴槌碰撞瞬态（Hammer Strike Transient）**：正弦振荡器纯音起振，缺少毛毡木槌碰撞钢弦与琴桥的敲击瞬态（Click/Thump）；
4. **缺失击弦位置梳状滤波**：未模拟敲击点（$d/L \approx 1/8$）对第 7/8 次谐波的物理陷波抑制。

---

## 落地架构与关键技术

```
[MIDI Note On] ──┬──> [Hammer Transient 2~3ms Click] ───────────────┬──> [Body Resonators] ──> Output
                 └──> [Striking Comb + Nonlinear Felt Spectrum] ────┘
```

1. **击弦位置梳状滤波（Striking Position Comb Filter）**：
   - 音区击弦比 $d/L$ 查表（低音 $1/8 \to$ 中音 $1/7.5 \to$ 高音 $1/10 \to$ 极高音 $1/14$）；
   - 梳状增益 $S(m) = 0.06 + 0.94 \cdot |\sin(m \pi d/L)|$；
2. **非线性毛毡硬化截止（Nonlinear Hammer Felt）**：
   - 幂律滚降 $1/m^{1.35}$ 结合力度动态截止 $\exp(-m / \text{cutoff})$，消灭锯齿波；
3. **琴槌敲击瞬态冲击核（Hammer Strike Transient Generator）**：
   - $2\sim 3\text{ ms}$ 双共振峰（$1.1\sim 2.0\text{ kHz}$ 毛毡冲击 $+ 2.5\sim 4.5\text{ kHz}$ 钢丝初振）脉冲，随力度（$v^{1.6}$）非线性爆发；
4. **极速起振门控**：
   - 强制 Attack $\le 0.2\text{ ms}$，瞬时释放全额敲击动能；
5. **双阶段衰减再调优与云杉木音板 8 峰模态**：
   - 快衰减权重提升至 $80\%\sim 88\%$；音板模态重构（68Hz~1050Hz），Wet 比率与 Resonance 旋钮动态绑定（$18\%\sim 34\%$）。

---

## 子任务完成记录

- [x] **Phase 17-A**：击弦点梳状滤波与非线性琴槌频谱重构（Commit `81c6414`）；
- [x] **Phase 17-B**：琴槌敲击瞬态核（Hammer Strike Transient）与极速起振（Commit `81c6414`）；
- [x] **Phase 17-C**：双阶段能量衰减与音板共鸣增强调优（Commit `d85ac5d`）；
- [x] **Phase 17-D**：确定性物理测试补齐、听觉回归与三闸门交付（Commit `d85ac5d`）。

---

## 验证结果

- **三闸门基线**：`format --check` 0 违规，单元测试 57 类 11550+ 断言 100% 通过，Windows MSVC 构建成功生成 `DevPiano.exe`。
- **听觉实测**：消灭拉弦渐入与蜂鸣感，琴键呈现清晰逼真的击弦木质打击感，力度动态响应丰富。

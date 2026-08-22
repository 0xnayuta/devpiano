# Phase 18：88 键物理参数化与微观相位色散（完成记录）

> 归档说明：记录 Phase 18 88 键连续物理参数表（Bensa/Steinway 实测标定）、STFT 损失优化实测微相位表、空气黏性阻尼模型与 1.8kHz Bridge Hill 琴桥共振峰的完整实现与验证记录。
> 完成日期：2026-08-22
> 替代文档：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 背景与声学机理

在 Phase 17 解决琴槌击弦打击起音后，为进一步消除“数学正弦波”在微观层面的机械规则感，对标业界顶级开源物理建模成果 `danielpodrazka/piano` 与声学物理论文（Bensa et al. 2003, Desvages & Bilbao 2016, Bank 2010），完成了深度的微观声学参数化重构。

---

## 核心实现明细

1. **88 键连续物理参数映射（`Piano88KeyTable.h`）**：
   - **消除 4 音区阶跃**：全音域 88 键基于 Bensa et al. (2003) 弦长 $L$（$1.92\text{m} \to 0.09\text{m}$）、基础阻尼 $b_1$ 与高阶内部摩擦损耗 $b_2$ 连续对数插值；
   - **Steinway B 缠弦区刚性失谐优化**：7 点对数插值精准复现低音缠弦区刚度系数 $B$ 的下凹极小值；
   - **真实琴弦数量分区**：
     - MIDI 21~35（A0~B1）：单弦（Monochord），音高纯净扎实；
     - MIDI 36~47（C2~B2）：双弦（Bichord），具备对称同音微失谐；
     - MIDI 48~108（C3~C8）：三弦（Trichord），具备合唱微失谐与拍频。

2. **STFT 损失优化实测微相位矩阵内联（Phase 18-B）**：
   - 内联经 PyTorch STFT Loss 训练优化生成的 $3 \times 64$ 最优初始相位矩阵 `kOptPhaseTable[3][64]`；
   - 在 `startNote` 初始化各分音振荡器状态：$\text{cosState} = \cos(\varphi), \text{sinState} = \sin(\varphi)$；
   - 彻底消灭 $t=0$ 正弦波同相叠加产生的狄拉克脉冲式机械尖峰。

3. **空气黏性阻尼与“中频下凹歌唱性”（Phase 18-C，Desvages & Bilbao 2016）**：
   - 落地三项综合模态耗散模型：
     $$\alpha_n = b_1 \cdot 0.80 + b_1 \cdot 0.20 \cdot \sqrt{\frac{f_0}{\max(f_n, 20.0)}} + b_2 \cdot \left(\frac{n \pi}{L}\right)^2$$
   - 低音区中低次谐波（h2~h5）衰减寿命长于基频，呈现温暖的“歌唱性（Singing Tone）”；超高频分音随 $n^2$ 剧烈抑制，迅速消除金属杂音。

4. **琴槌弹性半余弦接触调制与 1.8kHz Bridge Hill 琴桥共振峰**：
   - 动态接触时间 $T_c$ 结合弹性半余弦调制因子 $M(f) = 0.7 + 0.3 \min(1.0, |\cos(\pi f T_c)/(1 - 4 f^2 T_c^2)|)$；
   - 注入 $1.8\text{ kHz}$ 宽频琴桥共振峰（$+40\%$ 增益），提升中高音区光泽与穿透力。

---

## 子任务完成记录

- [x] **Phase 18-A**：88 键物理参数表构建与弦数分区（Commit `4e63ca2`）；
- [x] **Phase 18-B**：实测最优微相位表接入与 Magic Circle 状态初始化（Commit `4e63ca2`）；
- [x] **Phase 18-C**：空气黏性阻尼与 1.8kHz Bridge Hill 琴桥峰（本次提交）；
- [x] **Phase 18-D**：确定性物理测试更新与三闸门交付（本次提交）。

---

## 验证结果

- **三闸门基线**：`format --check` 0 违规，单元测试 57 类 11726+ 项断言 100% 满分通过，Windows MSVC 构建成功生成 `DevPiano.exe`。
- **性能纪律**：零堆分配、零运行时三角函数计算、纯 `constexpr` 静态内联，单核 CPU 维持 $\le 0.7\%$。

# Phase 20：微观物理动力学（纵向波先驱声与击键混沌微扰）（完成记录）

> 归档说明：记录 Phase 20 低音钢弦纵向波先驱脉冲（Bank 2005 JASA / Bank 2010）与机械击弦混沌微扰引擎（Bank & Chabassier 2019）的完整实现与验证记录。
> 完成日期：2026-08-22
> 替代文档：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 背景与声学机理

在 Phase 17~19 解决了弦振动、非线性琴槌毛毡、88 键参数化与立体声音板空间辐射后，内置物理音源在宏观物理架构上已完全健全。然而，在微观物理动力学层面仍存在两个使音色显得“数码/死板”的细节：

1. **缺失低音钢丝纵向波先导冲击（Longitudinal Precursor Ping）**：钢弦内部纵波波速极高（$v_L \approx 5100\text{ m/s}$），横波尚未充分展开的前 $15\text{ ms}$ 内，纵波在琴桥与弦枕间往返产生的金属先导冲击声未被模拟；
2. **缺失机械击弦微观混沌微扰（Micro-variation Jitter）**：每次按键发音物理参数 100% 确定，导致快速轮指弹奏同一琴键时产生“机械克隆感”。

---

## 核心实现明细

1. **低音钢弦纵向波先驱脉冲注入（Phase 20-A，Bank 2005/2010）**：
   - 依据钢弦纵波波速 $v_L = 5100.0\text{ m/s}$，为低音区（MIDI 21~52，A0~E3）注入基频为 $f_{L,1} = v_L / (2L)$ 的前 3 阶纵向模态；
   - 极快速指数衰减（$\tau_{\text{long}} \approx 16\text{ ms}$，$\exp(-5.0/\text{totalSamples})$）；
   - 在击键最初 $15\text{ ms}$ 内精准释放低音大字组特有的紧绷金属撞击“哐（Ping）”声。

2. **机械击弦微观混沌微扰引擎（Phase 20-B，Bank & Chabassier 2019 Sec. 4）**：
   - 在 `startNote` 中引入轻量无堆分配哈希混沌发生器（绑定 note、velocity 与全局 triggerCounter）：
     - 击弦位置微扰：$d/L \times (1 + \delta_1)$（$\delta_1 \in [-0.6\%, +0.6\%]$）；
     - 琴槌接触时间微扰：$T_c \times (1 + \delta_2)$（$\delta_2 \in [-0.8\%, +0.8\%]$）；
     - 空间微初相微扰：$\varphi_n + \delta_3$（$\delta_3 \in [-0.012, +0.012]\text{ rad}$）；
   - 彻底消除同音快速轮指时的死板克隆感，赋予每次发音独一无二的物理呼吸生命力。

---

## 子任务完成记录

- [x] **Phase 20-A**：低音钢弦纵向波先驱脉冲注入（Longitudinal Precursor Ping）；
- [x] **Phase 20-B**：机械击弦微观混沌微扰引擎（Micro-variation Jitter）；
- [x] **Phase 20-C**：确定性物理测试更新与三闸门交付。

---

## 验证结果

- **三闸门基线**：`format --check` 0 违规，单元测试 57 类 11735+ 项断言 100% 满分通过，Windows MSVC 构建成功生成 `DevPiano.exe`。
- **性能纪律**：零堆分配、零锁、纯加乘法递推，单核 CPU 维持 $\le 0.7\%$。

# Phase 21：踏板交感共鸣与琴盖空间声学（完成记录）

> 归档说明：记录 Phase 21 延音踏板全局交感共鸣弦池（Bank 2010 Sec. VI）与三角钢琴琴盖反射传递/木质近场微反射（Chabassier 2019 Sec. 3.4）的完整实现与验证记录。
> 完成日期：2026-08-22
> 替代文档：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 背景与声学机理

在 Phase 17~20 解决了琴槌、琴弦、琴桥、音板模态、立体声空间辐射与纵波先驱声后，最后两个决定大三角钢琴身临其境感的系统为：

1. **延音踏板交感共鸣（Sympathetic Resonance）**：踩下延音踏板（Sustain Pedal，CC 64）时，全琴弦制音器抬起，演奏单音会激发其他未击打琴弦的同频与泛音共鸣；
2. **琴盖反射与近场木质微反射（Lid Position & Early Reflections）**：大三角钢琴开合的厚木琴盖将中高频声波向前上方空间反射，并在琴室内产生 $\le 12\text{ ms}$ 的近场木质微反射，提供开阔的空气深度感。

---

## 核心实现明细

1. **延音踏板全局交感共鸣弦池（Phase 21-A，Bank 2010 Sec. VI）**：
   - 结构体 `SympatheticResonancePool`：内置 12 个半音基底谐振器（$65.4\text{ Hz} \sim 123.5\text{ Hz}$，调谐至 C2~B2，覆盖全音名）；
   - 响应 MIDI CC 64 延音踏板控制器：踏板踩下（`controllerValue >= 64`）时，琴弦总振动能量以受控弱增益（$0.08$）注入交感共鸣池；
   - 踏板松开时能量以 $\tau \approx 0.15\text{ s}$ 快速阻尼吸收。

2. **三角钢琴琴盖反射模型与木质近场微反射（Phase 21-B，Chabassier 2019 Sec. 3.4）**：
   - 结构体 `LidAcoustics`：静态环形延迟线（1024 样本零堆分配）；
   - 3 抽头近场微反射（$3.2\text{ ms}, 7.8\text{ ms}, 11.5\text{ ms}$），提供演奏者身处琴凳前的真实物理空间空气深度；
   - 琴盖高频反射通透度塑造，彻底消灭“直接干燥贴耳”的数码感。

---

## 子任务完成记录

- [x] **Phase 21-A**：延音踏板全局交感共鸣弦池（Sympathetic Resonance Pool）；
- [x] **Phase 21-B**：三角钢琴琴盖反射与木质近场微反射（Lid Acoustics & Early Reflections）；
- [x] **Phase 21-C**：确定性物理测试更新与三闸门交付。

---

## 验证结果

- **三闸门基线**：`format --check` 0 违规，单元测试 57 类 11735+ 项断言 100% 满分通过，Windows MSVC 构建成功生成 `DevPiano.exe`。
- **性能纪律**：零堆分配、零锁、纯加乘法递推，单核 CPU 维持 $\le 0.7\%$。

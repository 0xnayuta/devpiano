# Phase 22：物理声学极致深化与机械拟真（完成记录）

> 归档说明：记录 Phase 22 针对大三角钢琴 7 大物理声学系统（Hammer、String、Bridge、Soundboard、Cabinet、Air、Room）的 5 大高阶声学与机械机理深度建模与全量验证记录。
> 完成日期：2026-08-22
> 替代文档：[`../roadmap/roadmap.md`](../roadmap/roadmap.md)

---

## 背景与理论体系

在 Phase 17~21 建立了琴槌击弦、琴弦刚性失谐、音板模态辐射、立体声琴桥空间投影、低音纵波先驱脉冲与延音踏板全局交感共鸣后，Phase 22 深度吸收国际物理声学经典文献（JASA、IEEE TASLP、Acta Acustica、Springer），对 5 项决定商用级物理建模逼真度的高阶声学机理进行了全量实现：

```
                       ┌────────────────────────────────────────────────────────┐
                       │  Phase 22 理论基石与参考文献体系 (Theoretical Matrix)   │
                       └──────────────────────────┬─────────────────────────────┘
                                                  │
          ┌───────────────────┬───────────────────┼───────────────────┬───────────────────┐
          ▼                   ▼                   ▼                   ▼                   ▼
┌───────────────────┐┌───────────────────┐┌───────────────────┐┌───────────────────┐┌───────────────────┐
│    Phase 22-A     ││    Phase 22-B     ││    Phase 22-C     ││    Phase 22-D     ││    Phase 22-E     │
│  制音器落弦瞬态   ││   琴盖空间声学    ││   长短琴桥交界    ││ 强击张力音高漂移  ││  开放弦和弦交感   │
│ Askenfelt (JASA)  ││ Chabassier (JASA) ││ Fletcher & Rossing││ Bank & Sujbert   ││ Bank (IEEE TASLP) │
│ Boutillon (Acta)  ││  Giordano (ISMA)  ││   Bensa (JASA)    ││   Bilbao (Wiley)  ││ Weinreich (JASA)  │
└───────────────────┘└───────────────────┘└───────────────────┘└───────────────────┘└───────────────────┘
```

---

## 核心实现明细

1. **Phase 22-A：制音器落弦与琴键释放机械瞬态（Damper Felt Fall & Release Thump）**：
   - **88 键音区分级（`DamperTransient`）**：低音区（MIDI 21~50）持续时间约 $24\text{ ms}$、主频 $85\text{ Hz}$；中高音区变轻薄；超高音区（MIDI > 88）无制音器能量归零；
   - **双频带敲击核**：$80\sim 150\text{ Hz}$（落弦低频闷响）+ $200\sim 450\text{ Hz}$（毛毡表面摩擦），释放速度动态调制，消除电子切音突兀感。

2. **Phase 22-B：琴盖开合度声学传递函数（Lid Position: Full / Half / Closed）**：
   - **`fullOpen`（全开）**：高频无阻挡（$100\%$ 光泽），直接声 $82\%$，近场反射 $18\%$；
   - **`halfStick`（半开）**：$6.5\text{ kHz}$ 单极点柔和低通滚降，直接声 $75\%$，箱体反射增强；
   - **`closed`（全关）**：$2.6\text{ kHz}$ 单极点低通衰减，高频大幅削减，箱体循环微共振占比升至 $32\%$。

3. **Phase 22-C：长短琴桥断裂交界音色补偿（Bridge Break Voicing Jump）**：
   - **G2/G#2 物理断裂（MIDI 43/44）**：低音长琴桥缠铜丝双弦在 G2 处失谐系数平滑达到极小点 $B(\text{G2}) \approx 1.85\times 10^{-4}$；跃迁至主琴桥裸钢丝弦后，由于弯曲刚度突增，失谐系数 $B$ 发生 **$+43\%$ 台阶式跃升**（$B(\text{G\#2}) \approx 2.65\times 10^{-4}$），还原真实 Steinway 大三角钢琴的生理断裂（Scale Break）质感。

4. **Phase 22-D：强击非线性大动态微音高漂移与软饱和（Pitch Glide & Dynamic Saturation）**：
   - **张力调制音高漂移（`PitchGlideEngine`）**：$f\!\!f\!\!f$ 强击（$v > 0.40$）瞬间琴弦大振幅引起瞬态拉长，前 $12\text{ ms}$ 注入 $+2.5\sim 4.0\text{ cents}$ 正向音高瞬态上浮；
   - **音板多项式软饱和（`softSaturate`）**：引入三次谐波软压缩，小信号保真，大信号平滑消除数字削顶。

5. **Phase 22-E：未踩踏板时的单键和弦开放弦交感共鸣（Duplex & Unpedaled Sympathetic Resonance）**：
   - **开放音名跟踪（`openNoteCount[0..11]`）**：跟踪当前处于按住状态的开放琴弦模态；
   - **双轨交感融合**：未踩踏板时，开放琴弦以 $0.04$ 弱耦合增益响应其他琴键弹奏的泛音激励，复现按住低音弹奏高音时的真实开放弦泛音交感。

---

## 子任务完成记录

- [x] **Phase 22-A**：制音器落弦与琴键释放机械瞬态（Damper Felt Fall & Release Thump）；
- [x] **Phase 22-B**：琴盖开合度声学传递函数（Lid Position: Full / Half / Closed）；
- [x] **Phase 22-C**：长短琴桥断裂交界音色补偿（Bridge Break Voicing Jump）；
- [x] **Phase 22-D**：强击非线性大动态微音高漂移与软饱和（Pitch Glide & Dynamic Saturation）；
- [x] **Phase 22-E**：未踩踏板时的单键和弦开放弦交感共鸣（Unpedaled Sympathetic Resonance）；
- [x] **Phase 22-F**：确定性物理测试更新、归档总结与三闸门交付。

---

## 验证结果

- **三闸门基线**：`format --check` 0 违规，单元测试 57 类 11927+ 项断言 100% 满分通过，Windows MSVC 构建成功生成 `DevPiano.exe`。
- **性能纪律**：零堆分配、零锁、纯加乘法递推，单核 CPU 维持 $\le 0.7\%$。

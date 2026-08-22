# devpiano Current Iteration

> 用途：只记录当前正在推进的一轮任务。
> 更新时机：开始新一轮任务、完成当前任务、调整本轮范围时。

## 当前方向

**Phase 22：物理声学极致深化与机械拟真（Physical Modeling Acoustic Refinement & Mechanical Realism） [排期规划中 / 准备推进]**

本轮重点聚焦 7 大物理声学系统（Hammer、String、Bridge、Soundboard、Cabinet、Air、Room）深度评估后识别出的 5 大高阶物理机理：

1. **制音器落弦与琴键释放机械瞬态（Phase 22-A, Damper Felt Fall & Release Thump）**：
   - 建模松开琴键时，制音器（Damper）回落击打琴弦的 $80\sim 150\text{ Hz}$ 极短促（$\le 25\text{ ms}$）木质/毛毡低频闷击声；
   - 随 88 键物理音区分级（低音区制音器大而厚重，高音区无制音器，MIDI > 88 释放能量归零）；
   - 消除音符释放时的电子突兀感，赋予松键机械物理回弹触感。

2. **琴盖开合度声学传递函数与空间滤镜（Phase 22-B, Lid Position Acoustics: Full / Half / Closed）**：
   - 模拟大三角钢琴琴盖不同开合角度对高频声波空间反射与空气散射的物理滤波传递函数；
   - **Full Open（全开）**：高频通透，明亮空气感，直接声与近场反射比例平衡；
   - **Half Stick（半开）**：中高频 $4\sim 6\text{ kHz}$ 柔和滚降，近场木质反射占比提升；
   - **Closed（全关）**：深沉醇厚，$\sim 2.5\text{ kHz}$ 低通衰减，箱体微共振主导。

3. **长短琴桥断裂交界音色物理补偿（Phase 22-C, Bridge Break & Inharmonicity Voicing Jump）**：
   - 真实三角钢琴在低音单弦长琴桥与中高音短琴桥交界处（MIDI 43~44 / G2~G#2）存在物理断裂间隙；
   - 精细化交界处的弦长、琴桥导纳阻抗与刚性失谐系数 $B$ 的台阶式物理跳变，还原真实大三角钢琴的声学生理特征。

4. **强击非线性大动态微音高漂移与软饱和（Phase 22-D, Pitch Glide & Non-linear Tension Modulation）**：
   - $f\!\!f\!\!f$ 强击瞬间琴弦横向大振幅振动导致琴弦微观瞬态拉长，在前 $5\sim 10\text{ ms}$ 注入 $2\sim 5$ 音分的瞬态音高微浮（Pitch Glide）与音板微弱三次谐波软饱和，重现重击钢琴时的物理爆发张力。

5. **未踩踏板时的单键和弦开放弦交感共鸣（Phase 22-E, Duplex & Unpedaled Sympathetic Resonance）**：
   - 持续按住低音键时，弹奏高音键将激发低音开放弦的同频泛音共振，实现无踏板状态下的双音/和弦交感。

---

## Phase 22：物理声学极致深化与机械拟真 [排期规划]

### 系统架构与物理模块映射

```
[MIDI Note On / Velocity]
       │
       ├─► [Phase 22-D: Pitch Glide (2~5 cents) & Non-linear Tension] ──► [String Modes]
       ├─► [Phase 22-C: Bridge Break Inharmonicity Jump (G2/G#2)] ─────► [Bridge Hill & Stereo Panning]
       └─► [Phase 22-E: Duplex / Unpedaled Sympathetic Resonance] ─────► [Open String Harmonic Pool]

[MIDI Note Off / Release]
       │
       └─► [Phase 22-A: Damper Fall & Release Thump (80~150 Hz)] ──────► [Stereo Master Out]

[Lid Position State (Full / Half / Closed)]
       │
       └─► [Phase 22-B: Lid Transfer Function & 3-Tap Early Reflections] ► [Spatial Air Depth]
```

### 子任务排期（Phase 22）

- [x] **Phase 22-A：制音器落弦与琴键释放机械瞬态（Damper Felt Fall & Release Thump）**
  - 在 `PianoSynthVoice::stopNote` 与渲染器中实现制音器毛毡落弦物理敲击核；
  - 接入 88 键制音器物理分布（低音重、高音轻、超高音无）。
- [x] **Phase 22-B：琴盖开合度声学传递函数（Lid Position: Full / Half / Closed）**
  - 在 `LidAcoustics` 中引入多级高频滚降滤波器与近场反射增益矩阵；
  - 提供运行时琴盖开合参数调谐。
- [ ] **Phase 22-C：长短琴桥断裂交界音色补偿（Bridge Break Voicing Jump）**
  - 在 `Piano88KeyTable.h` 中针对 MIDI 43~44（G2/G#2）长短琴桥交界重构失谐与导纳阶跃。
- [ ] **Phase 22-D：强击非线性大动态微音高漂移与软饱和（Pitch Glide & Dynamic Saturation）**
  - 在 `PianoSynthVoice::startNote` 对极高力度按键注入微秒级瞬态音高上浮与音板三次谐波软饱和。
- [ ] **Phase 22-E：未踩踏板时的单键和弦开放弦交感共鸣（Unpedaled Sympathetic Resonance）**
  - 维护当前处于按住状态的开放琴弦模态，并在弹奏新音符时注入开放弦交感振荡。
- [ ] **Phase 22-F：确定性物理测试更新与三闸门交付**
  - 补充 5 大物理深化子项的数学断言测试，三闸门基线全绿交付。

---

## 历史实现 Backlog

- Phase 21 完成记录（踏板交感共鸣与琴盖空间声学）：[`../archive/phase21-sympathetic-resonance-lid-acoustics.md`](../archive/phase21-sympathetic-resonance-lid-acoustics.md)
- Phase 20 完成记录（微观物理动力学：纵向波先驱声与击键混沌微扰）：[`../archive/phase20-longitudinal-ping-micro-variation.md`](../archive/phase20-longitudinal-ping-micro-variation.md)
- Phase 19 完成记录（立体声音板共鸣箱与同音三弦微动力学）：[`../archive/phase19-stereo-modal-soundboard.md`](../archive/phase19-stereo-modal-soundboard.md)
- Phase 18 完成记录（88 键物理参数化与微观相位色散）：[`../archive/phase18-per-note-voicing-micro-phases.md`](../archive/phase18-per-note-voicing-micro-phases.md)
- Phase 17 完成记录（真实物理打击感钢琴音源重构）：[`../archive/phase17-physical-strike-hammer-piano.md`](../archive/phase17-physical-strike-hammer-piano.md)
- Phase 16 完成记录（虚拟键盘局部脏矩形重绘与预设覆盖确认）：[`../archive/phase16-keyboard-dirty-repaint-preset-confirm.md`](../archive/phase16-keyboard-dirty-repaint-preset-confirm.md)
- Phase 15 完成记录（声明式弹窗与设置面板重构）：[`../archive/phase15-declarative-dialogs-and-settings-jive.md`](../archive/phase15-declarative-dialogs-and-settings-jive.md)
- Phase 12–14 完成记录（内置物理建模钢琴音源三部曲）：[`../archive/phase12-14-builtin-piano-synthesis.md`](../archive/phase12-14-builtin-piano-synthesis.md)
- AUDIT-001 修复阶段归档：[`../archive/audit-001-code-quality-fix-phases.md`](../archive/audit-001-code-quality-fix-phases.md)
- Phase 11 完成记录（声明式 UI 架构）：[`../archive/phase11-declarative-ui-jive.md`](../archive/phase11-declarative-ui-jive.md)

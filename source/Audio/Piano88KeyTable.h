#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace devpiano::audio {

//==============================================================================
/// 88 键物理声学参数结构体 (Phase 18-A/B, Phase 22-C)
struct PianoNoteParams {
    int partialCount; ///< 激活分音数 (低音 36 -> 28 -> 18 -> 14 -> 8 -> 6)
    int stringCount; ///< 琴弦数: 1 (MIDI 21~35), 2 (MIDI 36~47), 3 (MIDI 48~108)
    float stringLength; ///< 振动弦长 L (米, 1.92m -> 0.09m)
    float b1; ///< 频率无关阻尼常数 (0.25 -> 9.17 s^-1)
    float b2; ///< 内部摩擦高阶损耗 (7.5e-5 -> 2.1e-3 s)
    double inharmonicityB;
    float strikePosRatio; ///< 击弦比 d/L (低音 0.125 -> 极高音 0.0625)
    float tcBase; ///< 基础接触时间 (低音 2.2ms -> 0.6ms)
    float decaySeconds; ///< τ_slow 基础慢衰减时间常数 (低音 4.8s -> 极高音 0.8s)
    float fastDecayRatio; ///< 快衰减比率 τ_fast / τ_slow (0.12 -> 0.18)
    float slowWeight; ///< 慢分量初始权重 (0.14 -> 0.22)
    float beatingDetuneRatio; ///< 同音拍频相对频偏
    int beatingPartials; ///< 启用拍频振荡器的分音数
};

//==============================================================================
/// 消除 t=0 所有正弦分音同相机械聚焦造成的狄拉克脉冲式波峰，赋予真实敲击相位色散。
inline constexpr float kOptPhaseTable[64]
    = { 4.0132f, 2.4928f, 1.6716f, 0.8096f, 0.9535f, 5.3968f, 3.8574f, 2.4742f, 4.9981f, 1.8611f, 3.7908f,
        5.9255f, 0.6455f, 0.3276f, 5.8913f, 5.4893f, 4.6771f, 1.2357f, 3.2581f, 2.7561f, 0.8428f, 4.0558f,
        5.2799f, 0.9260f, 0.6861f, 5.1939f, 1.2753f, 1.4505f, 0.1271f, 3.7157f, 0.7629f, 1.4660f, 0.2760f,
        2.8631f, 4.7170f, 4.0562f, 1.4049f, 2.9267f, 2.4257f, 0.3066f, 1.6580f, 1.7747f, 5.1121f, 0.7141f,
        0.1317f, 3.3997f, 4.9164f, 4.2310f, 0.9365f, 1.5663f, 6.2455f, 1.8572f, 2.0187f, 5.6055f, 1.2101f,
        0.2159f, 2.9470f, 3.6037f, 1.2411f, 3.0013f, 2.1811f, 4.8246f, 5.0224f, 3.2362f };

//==============================================================================
// 42-2 low-register partial budget. The bands avoid the old 36-to-14
// cliff at MIDI 48; beatingPartials remains capped at 6 separately.
[[nodiscard]] constexpr int pianoPartialCount(int midiNoteNumber) noexcept {
    if (midiNoteNumber <= 35) {
        return 36;
    }
    if (midiNoteNumber <= 47) {
        return 28;
    }
    if (midiNoteNumber <= 59) {
        return 18;
    }
    if (midiNoteNumber <= 71) {
        return 14;
    }
    if (midiNoteNumber <= 95) {
        return 8;
    }
    return 6;
}

namespace detail {

// 线性插值辅助
constexpr float lerp(float a, float b, float t) noexcept {
    return a + t * (b - a);
}

// 分段线性插值
inline float pieceWiseInterp(float midi, const float* midiPts, const float* valPts, std::size_t count) noexcept {
    if (midi <= midiPts[0]) {
        return valPts[0];
    }
    if (midi >= midiPts[count - 1]) {
        return valPts[count - 1];
    }
    for (std::size_t i = 1; i < count; ++i) {
        if (midi <= midiPts[i]) {
            const auto t = (midi - midiPts[i - 1]) / (midiPts[i] - midiPts[i - 1]);
            return lerp(valPts[i - 1], valPts[i], t);
        }
    }
    return valPts[count - 1];
}

// 分段对数插值
inline float logPieceWiseInterp(float midi, const float* midiPts, const float* valPts, std::size_t count) noexcept {
    if (midi <= midiPts[0]) {
        return valPts[0];
    }
    if (midi >= midiPts[count - 1]) {
        return valPts[count - 1];
    }
    for (std::size_t i = 1; i < count; ++i) {
        if (midi <= midiPts[i]) {
            const auto t = (midi - midiPts[i - 1]) / (midiPts[i] - midiPts[i - 1]);
            const auto logA = std::log(valPts[i - 1]);
            const auto logB = std::log(valPts[i]);
            return std::exp(logA + t * (logB - logA));
        }
    }
    return valPts[count - 1];
}

// 88 键独立物理参数生成 (B 表为 Task 39-2 实测锚点)
inline PianoNoteParams computePianoNoteParams(int midiNoteNumber) noexcept {
    const auto midi = static_cast<float>(std::clamp(midiNoteNumber, 21, 108));

    PianoNoteParams p {};

    // 1. 琴弦数配置
    if (midiNoteNumber < 36) {
        p.stringCount = 1; // 单缠弦 Monochord
    } else if (midiNoteNumber < 48) {
        p.stringCount = 2; // 双弦 Bichord
    } else {
        p.stringCount = 3; // 三弦 Trichord
    }

    // 2. 激活分音数 (42-2: 36/28/18/14/8/6 分段阶梯)
    p.partialCount = pianoPartialCount(midiNoteNumber);

    // 3. 弦长 L 与阻尼 b1, b2 (Bensa et al. 2003 三点对数插值)
    constexpr float bensaMidi[3] = { 36.0f, 60.0f, 96.0f };
    constexpr float bensaL[3] = { 1.92f, 0.62f, 0.09f };
    constexpr float bensaB1[3] = { 0.25f, 1.10f, 9.17f };
    constexpr float bensaB2[3] = { 7.5e-5f, 2.7e-4f, 2.1e-3f };

    p.stringLength = logPieceWiseInterp(midi, bensaMidi, bensaL, 3);
    p.b1 = logPieceWiseInterp(midi, bensaMidi, bensaB1, 3);
    p.b2 = logPieceWiseInterp(midi, bensaMidi, bensaB2, 3);

    // 4. 刚性失谐系数 B (Task 39-2: 官方参考实测锚点表)
    // 21-84 为 17 个实测锚点（21/24/26/28 取 v41 弱力度政策，其余取全部独立通过族中位数；
    // log 段间插值）；84 以上按高音桥
    // 标准斜率外推（Young 1952 / Rigaud-David-Daudet 2013：ln B 斜率 0.0926/键，
    // 即每八度 3.04 倍，六台钢琴拟合并与参考实测斜率一致），自 C6 实测锚点起算：
    // B(m) = 3.164862e-3 * exp(0.0926 * (m - 84))，即 96 -> 9.614857e-3、
    // 108 -> 2.920995e-2（替换无来源的 4e-2 / 8.5e-2 保留模型上界）。
    // 实测否定原琴桥 +43% 阶跃假设（43/45/47/48 平滑递增）。
    // interp-training 扩充（26/52/57）：24-28 段实测剖面为 V 形（26 最低），
    // 48-60 段由 48-52-57-60 细分；两段插值偏差已由新点消解/收窄
    // （准入分析 devpiano-evidence/phase39/task39-2/admission-analysis-20261014-094045）。
    {
        constexpr float bMidi[19] = { 21.0f, 24.0f, 26.0f, 28.0f, 33.0f, 36.0f, 40.0f, 43.0f, 45.0f, 47.0f,
                                      48.0f, 52.0f, 57.0f, 60.0f, 69.0f, 72.0f, 84.0f, 96.0f, 108.0f };
        constexpr float bVals[19]
            = { 1.367893e-4f, 8.800733e-5f, 6.868533e-5f, 7.330751e-5f, 4.767319e-5f, 5.080298e-5f, 6.263947e-5f,
                7.985170e-5f, 8.625150e-5f, 1.059587e-4f, 1.172690e-4f, 1.617091e-4f, 2.914628e-4f, 3.529958e-4f,
                8.203508e-4f, 1.093004e-3f, 3.164862e-3f, 9.614857e-3f, 2.920995e-2f };
        p.inharmonicityB = static_cast<double>(logPieceWiseInterp(midi, bMidi, bVals, 19));
    }

    // 5. 击弦位置比 d/L (低音斜跨桥 0.125 -> 主琴桥折角 0.1333 -> 高音 0.100 -> 极高音 0.0625)
    constexpr float strikeMidi[4] = { 36.0f, 60.0f, 80.0f, 96.0f };
    constexpr float strikeRatios[4] = { 0.125f, 0.1333f, 0.100f, 0.0625f };
    p.strikePosRatio = pieceWiseInterp(midi, strikeMidi, strikeRatios, 4);

    // 6. 基础接触时间 TcBase (42-2 low-end 2.2ms -> 1.8ms -> 0.6ms)
    constexpr float tcMidi[3] = { 36.0f, 60.0f, 96.0f };
    constexpr float tcVals[3] = { 0.0022f, 0.0018f, 0.0006f };
    p.tcBase = logPieceWiseInterp(midi, tcMidi, tcVals, 3);

    // 7. 同音微失谐与拍频 (低音 0.0020 -> 中音 0.0015 -> 高音 0.0010 -> 极高音 0)
    // 多弦配置由 stringCount 决定；拍频分音使用统一上限 6，不再按 MIDI 音区硬切。
    // Phase 44-3 adds a tiny residual only at MIDI 100-108. Keeping the old
    // 80-to-100 interpolation for notes below 100 avoids changing the
    // already-approved C7/C8 boundary; the residual is a disabled-string
    // shimmer, not a pitch correction.
    constexpr float beatMidi[4] = { 36.0f, 60.0f, 80.0f, 100.0f };
    constexpr float beatRatios[4] = { 0.0020f, 0.0015f, 0.0010f, 0.0f };
    p.beatingDetuneRatio = pieceWiseInterp(midi, beatMidi, beatRatios, 4);
    if (midi >= 100.0f) {
        p.beatingDetuneRatio = 0.00030f;
        // The undamped top octave has no damper and only six partials. Detuning
        // every partial would make the near-coherent triad lose too much level;
        // a fundamental-only residual keeps the shimmer without a register trim.
        p.beatingPartials = 1;
    } else {
        p.beatingPartials = std::min(p.partialCount, 6);
    }

    // 8. 双阶段衰减常数 (τ_slow 与 τ_fast 比率)
    constexpr float decayMidi[6] = { 21.0f, 36.0f, 60.0f, 80.0f, 100.0f, 108.0f };
    constexpr float decayVals[6] = { 4.8f, 4.5f, 2.8f, 1.6f, 0.9f, 0.8f };
    constexpr float fastVals[6] = { 0.12f, 0.12f, 0.15f, 0.18f, 0.15f, 0.15f };
    constexpr float slowVals[6] = { 0.20f, 0.20f, 0.18f, 0.15f, 0.12f, 0.12f };

    p.decaySeconds = pieceWiseInterp(midi, decayMidi, decayVals, 6);
    p.fastDecayRatio = pieceWiseInterp(midi, decayMidi, fastVals, 6);
    p.slowWeight = pieceWiseInterp(midi, decayMidi, slowVals, 6);
    return p;
}

} // namespace detail

//==============================================================================
/// 88 键预计算物理常量表 (MIDI 21=A0 到 108=C8)
inline const std::array<PianoNoteParams, 88>& get88KeyParamsTable() noexcept {
    static const auto table = [] {
        std::array<PianoNoteParams, 88> t {};
        for (int i = 0; i < 88; ++i) {
            t[static_cast<std::size_t>(i)] = detail::computePianoNoteParams(21 + i);
        }
        return t;
    }();
    return table;
}

/// 获取指定 MIDI 音符的物理声学参数
[[nodiscard]] inline const PianoNoteParams& getNoteParams(int midiNoteNumber) noexcept {
    const auto clamped = std::clamp(midiNoteNumber, 21, 108);
    return get88KeyParamsTable()[static_cast<std::size_t>(clamped - 21)];
}

} // namespace devpiano::audio

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace devpiano::audio {

//==============================================================================
/// 88 键物理声学参数结构体 (Phase 18-A/B, Phase 22-C)
/// 基于 Bensa et al. (2003)、Fletcher & Rossing (1998 Chapter 12)、Steinway B 实测标定。
struct PianoNoteParams {
    int partialCount; ///< 激活分音数 (低音 20 -> 极高音 6)
    int stringCount; ///< 琴弦数: 1 (MIDI 21~35), 2 (MIDI 36~47), 3 (MIDI 48~108)
    bool isBassBridge; ///< 是否属于低音长琴桥 (MIDI 21~43=true, MIDI 44~108=false)
    float stringLength; ///< 振动弦长 L (米, 1.92m -> 0.09m)
    float b1; ///< 频率无关阻尼常数 (0.25 -> 9.17 s^-1)
    float b2; ///< 内部摩擦高阶损耗 (7.5e-5 -> 2.1e-3 s)
    double inharmonicityB; ///< Steinway B 实测失谐系数 (含 G2/G#2 琴桥断裂阶跃)
    float strikePosRatio; ///< 击弦比 d/L (低音 0.125 -> 极高音 0.0625)
    float tcBase; ///< 基础接触时间 (3.0ms -> 0.6ms)
    float detuneCents; ///< 同音弦微失谐量 (0.0 -> 0.4 cents)
    float decaySeconds; ///< τ_slow 基础慢衰减时间常数 (低音 4.8s -> 极高音 0.8s)
    float decayDampingC; ///< 模态阻尼斜率 (低音 0.38 -> 高音 0.15)
    float fastDecayRatio; ///< 快衰减比率 τ_fast / τ_slow (0.12 -> 0.18)
    float slowWeight; ///< 慢分量初始权重 (0.14 -> 0.22)
    float beatingDetuneRatio; ///< 同音拍频相对频偏
    int beatingPartials; ///< 启用拍频振荡器的分音数
};

//==============================================================================
/// 经 PyTorch STFT Loss 训练优化出的 3 组琴弦 x 64 分音实测最优初始相位 (弧度, Phase 18-B)
/// 消除 t=0 所有正弦分音同相机械聚焦造成的狄拉克脉冲式波峰，赋予真实敲击相位色散。
inline constexpr float kOptPhaseTable[3][64]
    = { { 4.0132f, 2.4928f, 1.6716f, 0.8096f, 0.9535f, 5.3968f, 3.8574f, 2.4742f, 4.9981f, 1.8611f, 3.7908f,
          5.9255f, 0.6455f, 0.3276f, 5.8913f, 5.4893f, 4.6771f, 1.2357f, 3.2581f, 2.7561f, 0.8428f, 4.0558f,
          5.2799f, 0.9260f, 0.6861f, 5.1939f, 1.2753f, 1.4505f, 0.1271f, 3.7157f, 0.7629f, 1.4660f, 0.2760f,
          2.8631f, 4.7170f, 4.0562f, 1.4049f, 2.9267f, 2.4257f, 0.3066f, 1.6580f, 1.7747f, 5.1121f, 0.7141f,
          0.1317f, 3.3997f, 4.9164f, 4.2310f, 0.9365f, 1.5663f, 6.2455f, 1.8572f, 2.0187f, 5.6055f, 1.2101f,
          0.2159f, 2.9470f, 3.6037f, 1.2411f, 3.0013f, 2.1811f, 4.8246f, 5.0224f, 3.2362f },
        { 1.3617f, 4.1407f, 3.1679f, 2.0926f, 2.8498f, 5.8210f, 4.4870f, 2.3379f, 5.2825f, 2.0538f, 4.1686f,
          6.1295f, 0.6556f, 0.3439f, 5.9858f, 5.2892f, 4.1228f, 0.8159f, 2.6249f, 2.3138f, 0.0429f, 3.2546f,
          4.8054f, 1.0011f, 5.9662f, 4.5308f, 0.1382f, 0.4243f, 5.1007f, 2.6385f, 5.3386f, 6.0777f, 4.8098f,
          2.4797f, 3.5437f, 2.2902f, 5.5527f, 0.8090f, 0.7646f, 4.7900f, 5.6842f, 5.9511f, 2.8493f, 4.6506f,
          4.0068f, 1.0178f, 2.3836f, 1.5110f, 4.2781f, 5.6876f, 3.9259f, 5.7782f, 5.7022f, 2.6450f, 4.3279f,
          3.0211f, 5.3764f, 6.0428f, 3.6381f, 5.4519f, 4.6440f, 0.9308f, 1.0450f, 5.1668f },
        { 5.3995f, 5.6364f, 4.5929f, 3.5345f, 4.3810f, 0.1389f, 5.3429f, 2.5298f, 5.5007f, 2.4105f, 4.6763f,
          0.2695f, 0.9782f, 0.8100f, 0.1505f, 5.2394f, 3.1700f, 0.0055f, 1.8871f, 2.4310f, 2.5955f, 1.1359f,
          4.7438f, 1.2807f, 0.7266f, 3.9978f, 5.2003f, 5.8520f, 3.4716f, 1.8749f, 3.4160f, 4.3617f, 2.2013f,
          2.4004f, 2.6537f, 0.4599f, 3.2560f, 4.7558f, 5.4229f, 3.1209f, 2.9916f, 3.3205f, 0.2833f, 2.1776f,
          1.5710f, 4.9177f, 6.1946f, 5.1710f, 0.3391f, 0.8279f, 0.8513f, 4.7880f, 0.2794f, 6.2582f, 3.6578f,
          5.5488f, 4.6602f, 1.7406f, 2.8100f, 1.5382f, 3.6269f, 5.0149f, 3.2789f, 5.5434f } };

//==============================================================================
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

// 88 键独立物理参数生成 (含 Phase 22-C 琴桥交界断裂阶跃)
inline PianoNoteParams computePianoNoteParams(int midiNoteNumber) noexcept {
    const auto midi = static_cast<float>(std::clamp(midiNoteNumber, 21, 108));

    PianoNoteParams p {};

    // 1. 琴桥归属与琴弦数配置 (Phase 22-C: G2/MIDI 43 为低音长琴桥末端，G#2/MIDI 44 为主琴桥开端)
    p.isBassBridge = (midiNoteNumber <= 43);

    if (midiNoteNumber < 36) {
        p.stringCount = 1; // 单缠弦 Monochord
    } else if (midiNoteNumber < 48) {
        p.stringCount = 2; // 双弦 Bichord
    } else {
        p.stringCount = 3; // 三弦 Trichord
    }

    // 2. 激活分音数 (低音 20 -> 中音 14 -> 高音 8 -> 极高音 6)
    if (midiNoteNumber < 48) {
        p.partialCount = 20;
    } else if (midiNoteNumber < 72) {
        p.partialCount = 14;
    } else if (midiNoteNumber < 96) {
        p.partialCount = 8;
    } else {
        p.partialCount = 6;
    }

    // 3. 弦长 L 与阻尼 b1, b2 (Bensa et al. 2003 三点对数插值)
    constexpr float bensaMidi[3] = { 36.0f, 60.0f, 96.0f };
    constexpr float bensaL[3] = { 1.92f, 0.62f, 0.09f };
    constexpr float bensaB1[3] = { 0.25f, 1.10f, 9.17f };
    constexpr float bensaB2[3] = { 7.5e-5f, 2.7e-4f, 2.1e-3f };

    p.stringLength = logPieceWiseInterp(midi, bensaMidi, bensaL, 3);
    p.b1 = logPieceWiseInterp(midi, bensaMidi, bensaB1, 3);
    p.b2 = logPieceWiseInterp(midi, bensaMidi, bensaB2, 3);

    // 4. 刚性失谐系数 B (Phase 22-C: Fletcher & Rossing 1998 琴桥断裂阶跃)
    // 低音长琴桥 (MIDI 21~43) 随粗缠丝线密度增加，B 从 3.1e-4 降至 G2 的 1.85e-4 极小点；
    // 跃迁至主琴桥 (MIDI 44+) 变为裸钢丝且弯曲刚度突增，B 发生 +43% 物理台阶式跃升至 2.65e-4。
    if (p.isBassBridge) {
        constexpr float bassBMidi[3] = { 21.0f, 33.0f, 43.0f };
        constexpr float bassBVals[3] = { 3.1e-4f, 2.4e-4f, 1.85e-4f };
        p.inharmonicityB = static_cast<double>(logPieceWiseInterp(midi, bassBMidi, bassBVals, 3));
    } else {
        constexpr float tenorBMidi[6] = { 44.0f, 57.0f, 69.0f, 84.0f, 96.0f, 108.0f };
        constexpr float tenorBVals[6] = { 2.65e-4f, 3.2e-4f, 8.5e-4f, 5.0e-3f, 4.0e-2f, 8.5e-2f };
        p.inharmonicityB = static_cast<double>(logPieceWiseInterp(midi, tenorBMidi, tenorBVals, 6));
    }

    // 5. 击弦位置比 d/L (低音斜跨桥 0.125 -> 主琴桥折角 0.1333 -> 高音 0.100 -> 极高音 0.0625)
    constexpr float strikeMidi[4] = { 36.0f, 60.0f, 80.0f, 96.0f };
    constexpr float strikeRatios[4] = { 0.125f, 0.1333f, 0.100f, 0.0625f };
    p.strikePosRatio = pieceWiseInterp(midi, strikeMidi, strikeRatios, 4);

    // 6. 基础接触时间 TcBase (3.0ms -> 0.6ms)
    constexpr float tcMidi[3] = { 36.0f, 60.0f, 96.0f };
    constexpr float tcVals[3] = { 0.0030f, 0.0018f, 0.0006f };
    p.tcBase = logPieceWiseInterp(midi, tcMidi, tcVals, 3);

    // 7. 同音微失谐与拍频 (低音 0.0020 -> 中音 0.0015 -> 高音 0.0010 -> 极高音 0)
    constexpr float beatMidi[4] = { 36.0f, 60.0f, 80.0f, 100.0f };
    constexpr float beatRatios[4] = { 0.0020f, 0.0015f, 0.0010f, 0.0f };
    p.beatingDetuneRatio = pieceWiseInterp(midi, beatMidi, beatRatios, 4);
    p.detuneCents = p.beatingDetuneRatio * 1200.0f;
    if (midiNoteNumber < 72) {
        p.beatingPartials = 6;
    } else if (midiNoteNumber < 96) {
        p.beatingPartials = 4;
    } else {
        p.beatingPartials = 0;
    }

    // 8. 双阶段衰减常数 (τ_slow 与 τ_fast 比率)
    constexpr float decayMidi[6] = { 21.0f, 36.0f, 60.0f, 80.0f, 100.0f, 108.0f };
    constexpr float decayVals[6] = { 4.8f, 4.5f, 2.8f, 1.6f, 0.9f, 0.8f };
    constexpr float dampVals[6] = { 0.38f, 0.38f, 0.28f, 0.20f, 0.15f, 0.12f };
    constexpr float fastVals[6] = { 0.12f, 0.12f, 0.15f, 0.18f, 0.15f, 0.15f };
    constexpr float slowVals[6] = { 0.20f, 0.20f, 0.18f, 0.15f, 0.12f, 0.12f };

    p.decaySeconds = pieceWiseInterp(midi, decayMidi, decayVals, 6);
    p.decayDampingC = pieceWiseInterp(midi, decayMidi, dampVals, 6);
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

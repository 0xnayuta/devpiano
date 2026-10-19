#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace devpiano::audio {

/// 三力度琴槌整音锚点。
/// 锚点值相对现有力度基线做增量，默认全零表示不改变当前发声。
struct HammerVoicingAnchor {
    float midiVelocity = 0.0f;
    float hardnessDelta = 0.0f; ///< 相对基准有效硬度的增量
    float loudnessDeltaDb = 0.0f; ///< 相对基准力度增益的 dB 增量
    float rolloffDelta = 0.0f; ///< 相对琴槌毛毡低通滚降指数的增量
};

/// Piano / Mezzo forte / Forte 三锚点配置。
struct HammerVoicing {
    std::array<HammerVoicingAnchor, 3> anchors { {
        { 41.0f, 0.0f, 0.0f, 0.0f },
        { 70.0f, 0.0f, 0.0f, 0.0f },
        { 98.0f, 0.0f, 0.0f, 0.0f },
    } };
};

/// 生产默认三力度整音。校准于 C4 41/70/98，55/85 仅作留出验证。
/// 数值相对现有力度基线，后续校准只改这里，不增加第二条 DSP 路径。
inline constexpr HammerVoicing kCalibratedHammerVoicing { .anchors = { {
                                                              { 41.0f, -0.20f, -4.75f, 1.2f },
                                                              { 70.0f, 0.30f, -0.6f, 1.5f },
                                                              { 98.0f, 0.30f, 2.5f, 0.0f },
                                                          } } };

struct HammerVoicingSample {
    float hardnessDelta = 0.0f;
    float loudnessGain = 1.0f;
    float rolloffDelta = 0.0f;
};

inline constexpr float kHammerVoicingMaxHardnessDelta = 0.30f;
inline constexpr float kHammerVoicingMaxLoudnessDeltaDb = 12.0f;
inline constexpr float kHammerVoicingMaxRolloffDelta = 2.0f;

[[nodiscard]] inline bool isFiniteHammerVoicingValue(float value) noexcept {
    // 不依赖 std::isfinite 的实时路径可预测性。
    return value == value && value >= -3.4e38f && value <= 3.4e38f;
}

/// 校验锚点顺序、范围与有限性。非法配置由 setter 拒绝，sample 回退到中性值。
[[nodiscard]] inline bool isValidHammerVoicing(const HammerVoicing& voicing) noexcept {
    const auto& anchors = voicing.anchors;
    for (std::size_t i = 0; i < anchors.size(); ++i) {
        const auto& anchor = anchors[i];
        if (!isFiniteHammerVoicingValue(anchor.midiVelocity) || !isFiniteHammerVoicingValue(anchor.hardnessDelta)
            || !isFiniteHammerVoicingValue(anchor.loudnessDeltaDb)
            || !isFiniteHammerVoicingValue(anchor.rolloffDelta)) {
            return false;
        }
        if (anchor.midiVelocity < 1.0f || anchor.midiVelocity > 127.0f) {
            return false;
        }
        if (anchor.hardnessDelta < -kHammerVoicingMaxHardnessDelta
            || anchor.hardnessDelta > kHammerVoicingMaxHardnessDelta) {
            return false;
        }
        if (anchor.loudnessDeltaDb < -kHammerVoicingMaxLoudnessDeltaDb
            || anchor.loudnessDeltaDb > kHammerVoicingMaxLoudnessDeltaDb) {
            return false;
        }
        if (anchor.rolloffDelta < -kHammerVoicingMaxRolloffDelta
            || anchor.rolloffDelta > kHammerVoicingMaxRolloffDelta) {
            return false;
        }
        if (i > 0 && anchor.midiVelocity <= anchors[i - 1].midiVelocity) {
            return false;
        }
    }
    return true;
}

/// 比较两个整音配置是否在锚点容差内一致，用于风格识别。
[[nodiscard]] inline bool isSameHammerVoicing(const HammerVoicing& a, const HammerVoicing& b) noexcept {
    constexpr auto tolerance = 1e-4f;
    const auto close = [](float lhs, float rhs) noexcept {
        const auto delta = lhs > rhs ? lhs - rhs : rhs - lhs;
        return delta <= tolerance;
    };
    for (std::size_t i = 0; i < a.anchors.size(); ++i) {
        const auto& lhs = a.anchors[i];
        const auto& rhs = b.anchors[i];
        if (!close(lhs.midiVelocity, rhs.midiVelocity) || !close(lhs.hardnessDelta, rhs.hardnessDelta)
            || !close(lhs.loudnessDeltaDb, rhs.loudnessDeltaDb) || !close(lhs.rolloffDelta, rhs.rolloffDelta)) {
            return false;
        }
    }
    return true;
}

/// 在 MIDI 力度坐标上分段线性插值；1 以下和 127 以上夹到端点。
/// 默认零偏移返回 {0.0f, 1.0f, 0.0f}，保持现有力度响应。
[[nodiscard]] inline HammerVoicingSample sampleHammerVoicing(const HammerVoicing& voicing,
                                                             float midiVelocity) noexcept {
    if (!isValidHammerVoicing(voicing)) {
        return {};
    }

    const auto velocity = std::clamp(midiVelocity, 1.0f, 127.0f);
    const auto& anchors = voicing.anchors;
    if (velocity <= anchors.front().midiVelocity) {
        const auto& anchor = anchors.front();
        return { anchor.hardnessDelta, std::pow(10.0f, anchor.loudnessDeltaDb / 20.0f), anchor.rolloffDelta };
    }
    if (velocity >= anchors.back().midiVelocity) {
        const auto& anchor = anchors.back();
        return { anchor.hardnessDelta, std::pow(10.0f, anchor.loudnessDeltaDb / 20.0f), anchor.rolloffDelta };
    }

    for (std::size_t i = 1; i < anchors.size(); ++i) {
        const auto& lower = anchors[i - 1];
        const auto& upper = anchors[i];
        if (velocity <= upper.midiVelocity) {
            const auto span = upper.midiVelocity - lower.midiVelocity;
            const auto t = (velocity - lower.midiVelocity) / span;
            const auto hardnessDelta = lower.hardnessDelta + t * (upper.hardnessDelta - lower.hardnessDelta);
            const auto loudnessDeltaDb = lower.loudnessDeltaDb + t * (upper.loudnessDeltaDb - lower.loudnessDeltaDb);
            const auto rolloffDelta = lower.rolloffDelta + t * (upper.rolloffDelta - lower.rolloffDelta);
            return { hardnessDelta, std::pow(10.0f, loudnessDeltaDb / 20.0f), rolloffDelta };
        }
    }

    return {};
}

} // namespace devpiano::audio

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace devpiano::audio {

/// 连续音区整音锚点。锚点只表达声部输出级的相对响度，不改变弦列、调律、
/// 分音权重或力度响应。锚点之间使用 dB 线性插值，避免逐键增益表和音区硬切。
struct PianoRegisterVoicingAnchor {
    int midiNote;
    float loudnessDeltaDb;
};

inline constexpr std::array<PianoRegisterVoicingAnchor, 8> kCalibratedPianoRegisterVoicing { {
    { 21, 10.0f },
    { 36, 8.0f },
    { 48, 1.0f },
    { 60, 0.0f },
    { 72, 0.0f },
    { 84, 0.0f },
    { 96, -3.0f },
    { 108, -4.0f },
} };

/// 返回 MIDI 音高的连续线性 dB 增益。该曲线是工程整音值，不是实琴逐键测量，
/// 也不是等响曲线的直接实现。
[[nodiscard]] inline float pianoRegisterLoudnessGain(int midiNoteNumber) noexcept {
    const auto& first = kCalibratedPianoRegisterVoicing.front();
    const auto& last = kCalibratedPianoRegisterVoicing.back();
    const auto clipped = std::clamp(midiNoteNumber, first.midiNote, last.midiNote);

    if (clipped <= first.midiNote) {
        return std::pow(10.0f, first.loudnessDeltaDb / 20.0f);
    }

    for (std::size_t index = 1; index < kCalibratedPianoRegisterVoicing.size(); ++index) {
        const auto& upper = kCalibratedPianoRegisterVoicing[index];
        if (clipped > upper.midiNote) {
            continue;
        }
        const auto& lower = kCalibratedPianoRegisterVoicing[index - 1];
        const auto span = static_cast<float>(upper.midiNote - lower.midiNote);
        const auto position = static_cast<float>(clipped - lower.midiNote) / span;
        const auto loudnessDb = lower.loudnessDeltaDb + position * (upper.loudnessDeltaDb - lower.loudnessDeltaDb);
        return std::pow(10.0f, loudnessDb / 20.0f);
    }

    return std::pow(10.0f, last.loudnessDeltaDb / 20.0f);
}

} // namespace devpiano::audio

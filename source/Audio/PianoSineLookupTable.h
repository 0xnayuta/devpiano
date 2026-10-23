#pragma once

#include "PianoDspMath.h"

#include <cmath>

// Shared sine lookup table extracted verbatim from PianoSynthVoice.h for R3.
// Size, element type, interpolation, construction formula, and namespace-local
// single instance are unchanged; this is the sole piano-DSP definition.
namespace devpiano::audio::dsp {

struct SineLookupTable {
    static constexpr int kSize = 4096;
    float table[kSize + 2] {};

    SineLookupTable() noexcept {
        for (int i = 0; i < kSize; ++i) {
            const auto angle = (kTwoPi * static_cast<double>(i)) / static_cast<double>(kSize);
            table[i] = static_cast<float>(std::sin(angle));
        }
        table[kSize] = table[0];
        table[kSize + 1] = table[1];
    }

    [[nodiscard]] float lookupNorm(float normPhase) const noexcept {
        float p = normPhase;
        if (p < 0.0f || p >= 1.0f) {
            p -= std::floor(p);
        }
        const auto scaled = p * static_cast<float>(kSize);
        auto idx = static_cast<int>(scaled);
        if (idx < 0) {
            idx = 0;
        } else if (idx > kSize) {
            idx = kSize;
        }
        const auto frac = scaled - static_cast<float>(idx);
        return table[idx] + frac * (table[idx + 1] - table[idx]);
    }

    [[nodiscard]] float lookupNorm(double normPhase) const noexcept {
        double p = normPhase;
        if (p < 0.0 || p >= 1.0) {
            p -= std::floor(p);
        }
        const auto scaled = p * static_cast<double>(kSize);
        auto idx = static_cast<int>(scaled);
        if (idx < 0) {
            idx = 0;
        } else if (idx > kSize) {
            idx = kSize;
        }
        const auto frac = static_cast<float>(scaled - static_cast<double>(idx));
        return table[idx] + frac * (table[idx + 1] - table[idx]);
    }
};

inline const SineLookupTable gSineLookupTable;

} // namespace devpiano::audio::dsp

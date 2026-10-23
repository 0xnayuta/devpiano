#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

// Minimal bounded trigonometric helpers shared by the piano DSP components.
// Extracted verbatim from PianoSynthVoice.h for R1; expressions and constants
// are intentionally unchanged.
namespace devpiano::audio::dsp {

inline constexpr double kPi = std::numbers::pi;
inline constexpr double kTwoPi = 6.283185307179586476925286766559;
inline constexpr double kHalfPi = 1.5707963267948966192313216916398;
inline constexpr double kInvTwoPi = 1.0 / kTwoPi;
inline constexpr double kInvHalfPi = 1.0 / kHalfPi;

struct SinCosResult {
    double sinVal = 0.0;
    double cosVal = 1.0;
};

[[nodiscard]] inline SinCosResult boundedSinCos(double x) noexcept {
    if (!std::isfinite(x)) {
        return { 0.0, 1.0 };
    }
    double k = std::floor(x * kInvTwoPi);
    x -= k * kTwoPi;
    if (x < 0.0 || x >= kTwoPi) {
        x = 0.0;
    }

    const int q = std::clamp(static_cast<int>(x * kInvHalfPi), 0, 3);

    double t = x - static_cast<double>(q) * kHalfPi;
    if (t < 0.0) {
        t = 0.0;
    } else if (t > kHalfPi) {
        t = kHalfPi;
    }

    const double z = t * t;
    const double sinPoly = t
        * (1.0
           - z
               * (1.0 / 6.0
                  - z
                      * (1.0 / 120.0
                         - z
                             * (1.0 / 5040.0
                                - z
                                    * (1.0 / 362880.0
                                       - z
                                           * (1.0 / 39916800.0
                                              - z * (1.0 / 6227020800.0 - z * (1.0 / 1307674368000.0))))))));
    const double cosPoly = 1.0
        - z
            * (1.0 / 2.0
               - z
                   * (1.0 / 24.0
                      - z
                          * (1.0 / 720.0
                             - z
                                 * (1.0 / 40320.0
                                    - z * (1.0 / 3628800.0 - z * (1.0 / 479001600.0 - z * (1.0 / 87178291200.0)))))));

    switch (q) {
    case 0:
        return { sinPoly, cosPoly };
    case 1:
        return { cosPoly, -sinPoly };
    case 2:
        return { -sinPoly, -cosPoly };
    default:
        return { -cosPoly, sinPoly };
    }
}

[[nodiscard]] inline double boundedSin(double x) noexcept {
    return boundedSinCos(x).sinVal;
}

[[nodiscard]] inline double boundedCos(double x) noexcept {
    return boundedSinCos(x).cosVal;
}

} // namespace devpiano::audio::dsp

namespace devpiano::audio {

// Deterministic per-note hash used by the felt-ageing perturbation path.
// Moves the existing PianoSynthVoice helper here so note setup calculations do
// not depend on the voice class.
[[nodiscard]] inline float deterministicNoteJitter(int midiNoteNumber, std::uint32_t salt) noexcept {
    std::uint32_t x = static_cast<std::uint32_t>(midiNoteNumber) ^ salt;
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return static_cast<float>(static_cast<std::int32_t>(x)) * (1.0f / 2147483648.0f);
}

} // namespace devpiano::audio

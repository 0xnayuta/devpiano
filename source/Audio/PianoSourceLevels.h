#pragma once

// Source-level gain constants shared by the main string budget and the
// hammer/damper mechanical transients. Extracted verbatim from
// PianoSynthVoice.h for R3; the single compile-time definitions and their
// values remain unchanged.
namespace devpiano::audio {

inline constexpr auto nominalPeakLevelAtFullVelocity = 0.95f;
inline constexpr auto globalSourceGain = 0.49f;
inline constexpr auto peakLevelAtFullVelocity = nominalPeakLevelAtFullVelocity * globalSourceGain;

} // namespace devpiano::audio

#pragma once

#include <algorithm>
#include <array>

namespace devpiano::audio {

// Shared piano voice budget. Keep the default and the selectable set in one
// place so settings, the realtime engine, offline export and the acoustic
// probe cannot drift.
inline constexpr int kDefaultPianoVoiceCount = 16;
inline constexpr int kMinPianoVoiceCount = 12;
inline constexpr int kMaxPianoVoiceCount = 24;
inline constexpr std::array<int, 4> kSupportedPianoVoiceCounts { 12, 16, 20, 24 };

[[nodiscard]] constexpr bool isSupportedPianoVoiceCount(int count) noexcept {
    return std::any_of(kSupportedPianoVoiceCounts.begin(), kSupportedPianoVoiceCounts.end(),
                       [count](int supported) { return supported == count; });
}

[[nodiscard]] constexpr int clampPianoVoiceCount(int count) noexcept {
    return isSupportedPianoVoiceCount(count) ? count : kDefaultPianoVoiceCount;
}

} // namespace devpiano::audio
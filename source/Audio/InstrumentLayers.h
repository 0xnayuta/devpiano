#pragma once

namespace devpiano::audio {

struct InstrumentLayers {
    bool enabled = false;
    bool pianoEnabled = true;
    bool pluginEnabled = true;
    float pianoGain = 0.5f;
    float pluginGain = 0.5f;

    [[nodiscard]] constexpr bool operator==(const InstrumentLayers& other) const noexcept = default;
};

} // namespace devpiano::audio

#pragma once

#include "../Core/AppState.h"
#include "InstrumentLayers.h"
#include "PerspectiveProcessor.h"
#include "RoomReverbEngine.h"
#include "TemperamentEngine.h"
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>

namespace devpiano::audio {

struct AcousticSnapshot {
    devpiano::core::BuiltinTone builtinTone = devpiano::core::BuiltinTone::piano;
    float masterGain = 1.0f;
    juce::ADSR::Parameters adsr { 0.01f, 0.2f, 0.8f, 0.3f };
    float brightness = 0.5f;
    float hammerHardness = 0.5f;
    float resonance = 0.5f;
    std::uint8_t lidPosition = 0;
    Temperament temperament = Temperament::equal;
    double referencePitchA4 = TemperamentEngine::kDefaultReferencePitch;
    SoundPerspective soundPerspective = SoundPerspective::player;
    ReverbSpace reverbSpace = ReverbSpace::chamber;
    float reverbWet = 0.0f;
    float pedalNoiseLevel = 0.6f;
    float feltAgeingAmount = 0.0f;
    bool unaCorda = false;
    devpiano::core::SustainPolicy sustainPolicy = devpiano::core::SustainPolicy::normal;
    bool transposeEnabled = false;
    int transposeOffset = 0;
    std::uint16_t channelFollowKeyMask = 0b1111110111111111;
    bool stretchTuningEnabled = true;
    float duplexResonance = 0.15f;
    InstrumentLayers layers;
};

} // namespace devpiano::audio

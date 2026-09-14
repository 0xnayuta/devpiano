#pragma once

#include "../Audio/RoomReverbEngine.h"
#include "../Audio/TemperamentEngine.h"
#include "../Settings/SettingsModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace devpiano::exporting {

struct WavExportOptions {
    double sampleRate = 44100.0;
    int numChannels = 2;
    int blockSize = 512;
    int bitsPerSample = 16;
    float masterGain = 1.0f;
    juce::ADSR::Parameters adsr;
    // Builtin fallback tone (Phase 12-3): match realtime parameters for export parity.
    SettingsModel::BuiltinTone builtinTone = SettingsModel::BuiltinTone::piano;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
    // Historical temperaments and reference pitch (Phase 30, export parity)
    devpiano::audio::Temperament temperament = devpiano::audio::Temperament::equal;
    double referencePitchA4 = devpiano::audio::TemperamentEngine::kDefaultReferencePitch;
    // Spatial acoustics and dual perspective imaging (Phase 31, export parity)
    devpiano::audio::SoundPerspective soundPerspective = devpiano::audio::SoundPerspective::player;
    devpiano::audio::ReverbSpace reverbSpace = devpiano::audio::ReverbSpace::chamber;
    float reverbWet = 0.0f;
    SettingsModel::LidPosition lidPosition = SettingsModel::LidPosition::fullOpen;
    // Mechanical action noise and felt ageing (Phase 32-D): match realtime parameters for export parity.
    float pedalNoiseLevel = 0.6f;
    float feltAgeingAmount = 0.0f;
};

} // namespace devpiano::exporting

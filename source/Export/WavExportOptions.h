#pragma once

#include "../Audio/HammerVoicing.h"
#include "../Audio/InstrumentLayers.h"
#include "../Audio/RoomReverbEngine.h"
#include "../Audio/TemperamentEngine.h"
#include "../Settings/SettingsModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace devpiano::exporting {

// Export-only finite tail. These are product/engineering bounds, not a claim
// about the natural decay time of any source or plugin.
inline constexpr double kDefaultWavTailSeconds = 6.0;
inline constexpr double kMinWavTailSeconds = 0.0;
inline constexpr double kMaxWavTailSeconds = 30.0;

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
    devpiano::audio::HammerVoicing hammerVoicing = devpiano::audio::kCalibratedHammerVoicing;
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
    bool stretchTuningEnabled = true;
    float duplexResonance = 0.15f;
    devpiano::audio::InstrumentLayers layers;
    // Finite post-take render window. 0.0 disables the extra tail.
    double tailSeconds = kDefaultWavTailSeconds;
};

} // namespace devpiano::exporting

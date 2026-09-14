#pragma once

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
    // Mechanical action noise and felt ageing (Phase 32-D): match realtime parameters for export parity.
    float pedalNoiseLevel = 0.6f;
    float feltAgeingAmount = 0.0f;
};

} // namespace devpiano::exporting

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
    // 内置 fallback 音色（Phase 12-3）：导出路径与实时路径同参数，保证音色一致。
    SettingsModel::BuiltinTone builtinTone = SettingsModel::BuiltinTone::piano;
    float pianoBrightness = 0.5f;
    float pianoHammerHardness = 0.5f;
    float pianoResonance = 0.5f;
};

} // namespace devpiano::exporting

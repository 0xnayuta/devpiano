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
    // 机械物理噪声与琴体微衰退 (Phase 32-D)：离线渲染与实时路径参数一致
    float pedalNoiseLevel = 0.6f;
    float feltAgeingAmount = 0.0f;
};

} // namespace devpiano::exporting

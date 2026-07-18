#pragma once

#include <JuceHeader.h>

namespace devpiano::exporting {

struct WavExportOptions {
    double sampleRate = 44100.0;
    int numChannels = 2;
    int blockSize = 512;
    int bitsPerSample = 16;
    float masterGain = 0.8f;
    juce::ADSR::Parameters adsr;
};

} // namespace devpiano::exporting

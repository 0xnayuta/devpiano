#pragma once

#include <JuceHeader.h>

namespace devpiano::recording
{
struct RecordingTake;
}

namespace devpiano::exporting
{

struct WavExportOptions
{
    double sampleRate = 44100.0;
    int numChannels = 2;
    int blockSize = 512;
    int bitsPerSample = 16;
    float masterGain = 0.8f;
    juce::ADSR::Parameters adsr;
};

bool exportTakeAsWavFile(const devpiano::recording::RecordingTake& take,
                         const juce::File& destinationFile,
                         const WavExportOptions& options);

} // namespace devpiano::exporting

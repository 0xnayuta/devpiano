#pragma once

#include <JuceHeader.h>

#include <string>

namespace devpiano::recording
{
struct RecordingTake;
}

namespace devpiano::exporting
{

bool exportTakeAsMidiFile(const devpiano::recording::RecordingTake& take,
                          const juce::File& destinationFile,
                          int ppq = 960);

}

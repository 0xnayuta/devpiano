#pragma once

#include <juce_core/juce_core.h>
#include <string>

namespace devpiano::recording {
struct RecordingTake;
}

namespace devpiano::exporting {

bool exportTakeAsMidiFile(const devpiano::recording::RecordingTake& take, const juce::File& destinationFile,
                          int ppq = 960);

}

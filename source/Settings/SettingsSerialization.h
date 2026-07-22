#pragma once

#include <JuceHeader.h>

#include "../Midi/ChannelMatrix.h"

namespace devpiano::settings {

// ---- Channel matrix serialization ----
[[nodiscard]] juce::ValueTree channelMatrixToValueTree(const devpiano::midi::ChannelMatrix& cm);
[[nodiscard]] devpiano::midi::ChannelMatrix valueTreeToChannelMatrix(const juce::ValueTree& t);

} // namespace devpiano::settings

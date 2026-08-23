#pragma once

#include "../Midi/ChannelMatrix.h"
#include <juce_data_structures/juce_data_structures.h>

namespace devpiano::settings {

// ---- Channel matrix serialization ----
[[nodiscard]] juce::ValueTree channelMatrixToValueTree(const devpiano::midi::ChannelMatrix& cm);
[[nodiscard]] devpiano::midi::ChannelMatrix valueTreeToChannelMatrix(const juce::ValueTree& t);

} // namespace devpiano::settings

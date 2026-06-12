#pragma once

#include <JuceHeader.h>

#include <unordered_map>

#include "Core/ChannelMatrix.h"
#include "Core/KeyMapTypes.h"

namespace devpiano::core {
struct KeyboardLayout;
} // namespace devpiano::core

namespace devpiano::settings {

// ---- Channel matrix serialization ----
[[nodiscard]] juce::ValueTree channelMatrixToValueTree(const devpiano::midi::ChannelMatrix& cm);
[[nodiscard]] devpiano::midi::ChannelMatrix valueTreeToChannelMatrix(const juce::ValueTree& t);

// ---- Key map serialization ----
[[nodiscard]] juce::ValueTree keyMapToValueTree(const std::unordered_map<int, int>& m);
[[nodiscard]] std::unordered_map<int, int> valueTreeToKeyMap(const juce::ValueTree& t);

// ---- KeyboardLayout <-> legacy keyMap conversion ----
[[nodiscard]] std::unordered_map<int, int> layoutToKeyMap(const devpiano::core::KeyboardLayout& layout);
[[nodiscard]] devpiano::core::KeyboardLayout keyMapToLayout(const std::unordered_map<int, int>& map,
                                                            const juce::String& layoutId);

} // namespace devpiano::settings

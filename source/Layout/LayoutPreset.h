#pragma once

#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"

namespace devpiano::layout
{
constexpr int presetFormatVersion = 1;

[[nodiscard]] juce::String getLayoutPresetDisplayNameForFile(const juce::File& path);

[[nodiscard]] juce::String getUserLayoutIdForFile(const juce::File& path);

[[nodiscard]] std::optional<devpiano::core::KeyboardLayout>
loadLayoutPreset(const juce::File& path);

[[nodiscard]] bool saveLayoutPreset(const devpiano::core::KeyboardLayout& layout,
                                    const juce::File& path);
} // namespace devpiano::layout

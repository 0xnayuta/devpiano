#pragma once

#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"

namespace devpiano::layout
{
[[nodiscard]] juce::File getUserLayoutDirectory();

[[nodiscard]] std::vector<devpiano::core::KeyboardLayout> scanUserLayoutDirectory();

[[nodiscard]] std::optional<devpiano::core::KeyboardLayout> loadUserLayoutById(const juce::String& layoutId);
} // namespace devpiano::layout

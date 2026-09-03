//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_FontUtilities.h"

namespace jive {
int parseFontStyleFlags(const juce::String& styleString) {
    int flags = juce::Font::plain;
    const auto tokens = juce::StringArray::fromTokens(styleString, false);

    if (tokens.contains("bold")) {
        flags += juce::Font::bold;
    }
    if (tokens.contains("italic")) {
        flags += juce::Font::italic;
    }
    if (tokens.contains("underlined")) {
        flags += juce::Font::underlined;
    }

    return flags;
}

float calculateStringWidth(const juce::String& text, const juce::Font& font) {
    return juce::GlyphArrangement::getStringWidth(font, text);
}
} // namespace jive

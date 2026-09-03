//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Colours.h"

namespace jive {
[[nodiscard]] static auto getTokensBetweenParentheses(const juce::String& text) {
    static constexpr auto includeSubString = false;
    static constexpr auto ignoreCase = true;
    const auto values = text.fromLastOccurrenceOf("(", includeSubString, ignoreCase)
                            .upToFirstOccurrenceOf(")", includeSubString, ignoreCase);
    return juce::StringArray::fromTokens(values, ",", "");
}

[[nodiscard]] static auto parseRgbColourString(const juce::String& text) {
    const auto tokens = getTokensBetweenParentheses(text);
    jassert(tokens.size() >= 3);

    const auto red = static_cast<juce::uint8>(tokens[0].getIntValue());
    const auto green = static_cast<juce::uint8>(tokens[1].getIntValue());
    const auto blue = static_cast<juce::uint8>(tokens[2].getIntValue());
    const auto alpha
        = static_cast<juce::uint8>(juce::roundToInt(255.0f * (tokens.size() > 3 ? tokens[3].getFloatValue() : 1.0f)));

    return juce::Colour::fromRGBA(red, green, blue, alpha);
}

[[nodiscard]] static auto parseHslColourString(const juce::String& text) {
    const auto tokens = getTokensBetweenParentheses(text);
    jassert(tokens.size() >= 3);

    const auto hue = tokens[0].getFloatValue() / 360.0f;
    const auto saturation = tokens[1].getFloatValue() / 100.0f;
    const auto lightness = tokens[2].getFloatValue() / 100.0f;
    const auto alpha = tokens.size() > 3 ? tokens[3].getFloatValue() : 1.0f;

    return juce::Colour::fromHSL(hue, saturation, lightness, alpha);
}

[[nodiscard]] static auto parseHexColourString(juce::String text) {
    text = text.fromLastOccurrenceOf("#", false, true);

    if (text.length() < 3 || text.length() == 5 || text.length() == 7) {
        return juce::Colour {};
    }

    if (text.length() < 5) {
        const auto length = text.length();

        for (auto i = length - 1; i >= 0; i--) {
            const auto replacement = text.substring(i, i + 1) + text.substring(i, i + 1);
            text = text.replaceSection(i, 1, replacement);
        }
    }

    text = text.paddedRight('F', 8);
    const auto red = static_cast<juce::uint8>(text.substring(0, 2).getHexValue32());
    const auto green = static_cast<juce::uint8>(text.substring(2, 4).getHexValue32());
    const auto blue = static_cast<juce::uint8>(text.substring(4, 6).getHexValue32());
    const auto alpha = static_cast<juce::uint8>(text.substring(6, 8).getHexValue32());

    return juce::Colour { red, green, blue, alpha };
}

[[nodiscard]] juce::Colour parseColour(const juce::String& colourString) {
    if (colourString.startsWith("rgb")) {
        return parseRgbColourString(colourString);
    }
    if (colourString.startsWith("hsl")) {
        return parseHslColourString(colourString);
    }
    if (colourString.startsWith("#")) {
        return parseHexColourString(colourString);
    }

    static const juce::Colour defaultIfNotFound { 0x12345670 };
    if (auto colour = juce::Colours::findColourForName(colourString, defaultIfNotFound); colour != defaultIfNotFound) {
        return colour;
    }

    return juce::Colour::fromString(colourString);
}
} // namespace jive

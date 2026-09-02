//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_StringStreams.h"

namespace juce {
String& operator<<(String& str, const Colour& colour) {
    return str << "juce::Colour { 0x" << colour.toDisplayString(true) << " }";
}

String& operator<<(String& str, const Font& font) {
    return str << "juce::Font { \"" << font.toString() << "\" }";
}

String& operator<<(String& str, const Image& image) {
    return str << "juce::Image { " << image.getWidth() << ", " << image.getHeight() << " }";
}

String& operator<<(String& str, const var& value) {
    if (value.isString()) {
        return str << "juce::var{ \"" << value.toString() << "\" }";
    }

    return str << "juce::var{ " << value.toString() << " }";
}

String& operator<<(String& str, const Time& time) {
    static constexpr auto includeDividers = true;
    return str << "juce::Time{ " << time.toISO8601(includeDividers) << " }";
}

String& operator<<(String& str, long double value) {
    return str << juce::String { static_cast<double>(value) };
}

String& operator<<(String& str, const RelativeTime& relativeTime) {
    return str << juce::String { relativeTime.inSeconds() } << "s";
}

#if JIVE_ENABLE_GRID
String& operator<<(String& str, juce::Grid::Px px) {
    return str << "juce::Grid::Px{ " << px.pixels << " }";
}

String& operator<<(String& str, const juce::Grid::TrackInfo& track) {
    return str << "juce::Grid::TrackInfo{ " << track.getSize() << " }";
}
#endif

String& operator<<(String& str, const juce::ValueTree& tree) {
    return str << tree.toXmlString();
}
} // namespace juce

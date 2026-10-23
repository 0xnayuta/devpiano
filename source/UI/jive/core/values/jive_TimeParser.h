//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include <optional>

namespace jive {
[[nodiscard]] static inline bool isValidTimeString(const juce::String& str) noexcept {
    const auto len = str.length();
    if (len < 2) {
        return false;
    }

    int suffixLen = 0;
    if (str.endsWith("ms")) {
        suffixLen = 2;
    } else if (str.endsWith("s")) {
        suffixLen = 1;
    } else {
        return false;
    }

    const auto numLen = len - suffixLen;
    if (numLen <= 0) {
        return false;
    }

    auto ptr = str.getCharPointer();
    if (!juce::CharacterFunctions::isDigit(*ptr)) {
        return false;
    }

    bool hasDot = false;
    for (int i = 0; i < numLen; ++i) {
        const auto c = *ptr++;
        if (juce::CharacterFunctions::isDigit(c)) {
            continue;
        }
        if (c == '.') {
            if (hasDot || i == numLen - 1) {
                return false;
            }
            hasDot = true;
            if (!juce::CharacterFunctions::isDigit(*ptr)) {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

[[nodiscard, maybe_unused]] static inline std::optional<juce::RelativeTime> parseTime(const juce::String& timeString) {
    if (!isValidTimeString(timeString)) {
        return std::nullopt;
    }

    if (timeString.endsWith("ms")) {
        return juce::RelativeTime::milliseconds(juce::roundToInt(timeString.getDoubleValue()));
    }

    if (timeString.endsWith("s")) {
        return juce::RelativeTime::seconds(timeString.getDoubleValue());
    }

    return std::nullopt;
}
} // namespace jive

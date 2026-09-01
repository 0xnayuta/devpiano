//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace jive {
enum class Overflow {
    hidden,
    scroll,
};
} // namespace jive

namespace juce {
template <> struct VariantConverter<jive::Overflow> {
public:
    static var toVar(const jive::Overflow& overflow) {
        jassert(stringsMap.count(overflow) == 1);
        return stringsMap.at(overflow);
    }

    static jive::Overflow fromVar(const var& v) {
        auto overflowStringPair = std::find_if(std::begin(stringsMap), std::end(stringsMap),
                                               [v](const auto& pair) { return pair.second == v.toString(); });
        jassert(overflowStringPair != std::end(stringsMap));

        return overflowStringPair->first;
    }

private:
    static const std::unordered_map<jive::Overflow, String> stringsMap;
};
} // namespace juce

//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

namespace jive {
enum class LayoutStrategy {
    real,
    dummy,
};

[[nodiscard]] inline juce::String toString(LayoutStrategy strategy) {
    switch (strategy) {
    case LayoutStrategy::real:
        return "real";
    case LayoutStrategy::dummy:
        return "dummy";
    }

    jassertfalse;
    return "";
}
} // namespace jive

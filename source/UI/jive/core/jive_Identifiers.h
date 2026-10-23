//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_core/juce_core.h>

namespace jive::ids {
// 仅收录当前保留闭包已经消费的标识符；新增调用点必须显式使用，
// 不做全仓字符串机械替换。
[[nodiscard]] inline const juce::Identifier& transition() {
    static const juce::Identifier identifier { "transition" };
    return identifier;
}

[[nodiscard]] inline const juce::Identifier& idealWidth() {
    static const juce::Identifier identifier { "ideal-width" };
    return identifier;
}

[[nodiscard]] inline const juce::Identifier& idealHeight() {
    static const juce::Identifier identifier { "ideal-height" };
    return identifier;
}

[[nodiscard]] inline const juce::Identifier& componentWidth() {
    static const juce::Identifier identifier { "component-width" };
    return identifier;
}

[[nodiscard]] inline const juce::Identifier& componentHeight() {
    static const juce::Identifier identifier { "component-height" };
    return identifier;
}
} // namespace jive::ids

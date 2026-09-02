//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#ifndef JIVE_ENABLE_GRID
#define JIVE_ENABLE_GRID 1
#endif

#include "jive_AttributedStringVariantConverters.h"
#include "jive_FlexVariantConverters.h"
#if JIVE_ENABLE_GRID
#include "jive_GridVariantConverters.h"
#endif
#include "jive_MiscVariantConverters.h"

#include <juce_data_structures/juce_data_structures.h>

namespace jive {
template <typename Value> [[nodiscard]] auto toVar(const Value& value) {
    if constexpr (std::is_array<Value>()) {
        return juce::var { value };
    } else {
        return juce::VariantConverter<Value>::toVar(value);
    }
}

template <typename... Values> [[nodiscard]] auto toVar(const Values&... values) {
    return juce::var {
        juce::Array { toVar(values)... },
    };
}

template <typename Value> [[nodiscard]] auto toVar(const std::optional<Value>& value) {
    if (value.has_value()) {
        return toVar(*value);
    }

    return juce::var {};
}

template <typename Value> [[nodiscard]] auto fromVar(const juce::var& value) {
    return juce::VariantConverter<Value>::fromVar(value);
}

template <typename FirstValue, typename SecondValue, typename... OtherValues>
[[nodiscard]] auto fromVar(const juce::var& value) {
    auto next = [i = 0](const juce::Array<juce::var>& values) mutable { return values.getReference(i++); };
    return std::make_tuple(fromVar<FirstValue>(next(*value.getArray())), fromVar<SecondValue>(next(*value.getArray())),
                           fromVar<OtherValues>(next(*value.getArray()))...);
}
} // namespace jive

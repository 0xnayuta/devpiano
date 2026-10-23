//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "UI/jive/core/geometry/jive_Interpolate.h"

#include <juce_core/juce_core.h>

namespace jive {
/** CSS 风格长度值。当前产品只消费 px 与 %；auto 由 Property 的缺省/字符串语义表达。 */
struct Length {
    enum class Unit { absolute, percentage };

    Length() = default;
    Length(const Length&) = default;
    Length& operator=(const Length&) = default;
    ~Length() = default;

    Length(float initialValue, Unit initialUnit)
        : value { initialValue }
        , unit { initialUnit } {
    }

    template <typename Arithmetic>
    Length(Arithmetic absoluteValue)
        : value { static_cast<float>(absoluteValue) } {
    }

    Length(const juce::String& text)
        : Length { fromString(text) } {
    }

    Length(const char* text)
        : Length { juce::String { text } } {
    }

    [[nodiscard]] float toPixels(float percentageScale) const {
        return unit == Unit::percentage ? (percentageScale * value / 100.0f) : value;
    }

    [[nodiscard]] bool operator==(const Length&) const;

    [[nodiscard]] juce::String toString() const;
    [[nodiscard]] static Length fromString(const juce::String&);

    float value { 0.0f };
    Unit unit = Unit::absolute;
};

template <> struct Interpolate<Length> {
    [[nodiscard]] auto operator()(const Length& start, const Length& end, double proportion) const {
        // 不同单位之间没有共同量纲：保持起点直到过渡完成，再切换为目标值，
        // 不把 px 数值直接当作 % 数值插值。
        if (start.unit != end.unit) {
            return proportion >= 1.0 ? end : start;
        }

        return Length {
            interpolate(start.value, end.value, proportion),
            start.unit,
        };
    }
};
} // namespace jive

namespace juce {
template <> struct VariantConverter<jive::Length> {
    static var toVar(const jive::Length& length);
    static jive::Length fromVar(const var& value);
};
} // namespace juce

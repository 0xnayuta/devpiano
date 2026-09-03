//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Length.h"

#include "jive_VariantConvertion.h"

namespace jive {
[[nodiscard]] float Length::toPixels(const juce::Rectangle<float>& parentBounds) const {
    const auto getCurrent = [this] {
        if (auto* transition = getTransition()) {
            return transition->calculateCurrent<float>();
        }

        return get().getFloatValue();
    };

    if (isAuto()) {
        return pixelValueWhenAuto;
    }

    if (isPixels()) {
        return getCurrent();
    }

    if (isPercent()) {
        const auto scale = static_cast<double>(getCurrent()) * 0.01;
        return static_cast<float>(scale * getRelativeParentLength(parentBounds.toDouble()));
    }

    const auto fontSize = isRem() ? getRootFontSize() : getFontSize();
    return fontSize * getCurrent();
}

[[nodiscard]] bool Length::isPixels() const {
    return !isAuto() && !isPercent() && !isEm() && !isRem();
}

[[nodiscard]] bool Length::isPercent() const {
    return !isAuto() && toString().endsWith("%");
}

[[nodiscard]] bool Length::isEm() const {
    return !isAuto() && !isRem() && toString().endsWithIgnoreCase("em");
}

[[nodiscard]] bool Length::isRem() const {
    return !isAuto() && toString().endsWithIgnoreCase("rem");
}

[[nodiscard]] double Length::getRelativeParentLength(const juce::Rectangle<double>& parentBounds) const {
    jassert(isValid(getParent(source)));

    if (id.toString().containsIgnoreCase("width") || id.toString().containsIgnoreCase("x")) {
        return parentBounds.getWidth();
    }

    return parentBounds.getHeight();
}

[[nodiscard]] float Length::getFontSize() const {
    for (auto toSearch = source; isValid(source); toSearch = getParent(source)) {
        if (const auto style = getVar(toSearch, "style"); style.isObject()) {
            if (const auto fontSize = style["font-size"]; fontSize != juce::var {}) {
                return fromVar<float>(fontSize);
            }
        }
    }

    return 0.0f;
}

[[nodiscard]] float Length::getRootFontSize() const {
    if (const auto style = getVar(getRoot(source), "style"); style.isObject()) {
        if (const auto fontSize = style["font-size"]; fontSize != juce::var {}) {
            return fromVar<float>(fontSize);
        }
    }

    return 0.0f;
}
} // namespace jive

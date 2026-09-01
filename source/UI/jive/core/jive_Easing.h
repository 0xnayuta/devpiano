//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_Bezier.h"
#include "jive_TransferFunction.h"

namespace jive {
using Easing = std::function<double(double)>;

namespace easing {
static constexpr auto linear = [](double x) { return x; };
static constexpr auto inOut = [](double x) {
    static const auto transferFunction = jive::cubicBezier({ 0.4f, 0.0f }, { 0.2f, 1.0f });
    return solveFor(transferFunction, x);
};
static constexpr auto out = [](double x) {
    static const auto transferFunction = jive::cubicBezier({ 0.0f, 0.0f }, { 0.2f, 1.0f });
    return solveFor(transferFunction, x);
};
static constexpr auto in = [](double x) {
    static const auto transferFunction = jive::cubicBezier({ 0.4f, 0.0f }, { 1.0f, 1.0f });
    return solveFor(transferFunction, x);
};

[[nodiscard]] std::optional<Easing> fromString(const juce::String& name);
} // namespace easing
} // namespace jive

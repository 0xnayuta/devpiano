//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_NormalisedProgressBar.h"

namespace jive {
NormalisedProgressBar::NormalisedProgressBar()
    : juce::ProgressBar { value } {
}

void NormalisedProgressBar::setValue(double normalisedValue) {
    jassert(normalisedValue >= 0.0 && normalisedValue <= 1.0);
    value = normalisedValue;
}

double NormalisedProgressBar::getValue() const {
    return value;
}
} // namespace jive

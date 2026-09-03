//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_ProgressBar.h"

namespace jive {
ProgressBar::ProgressBar(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator { std::move(itemToDecorate) }
    , value { state, "value" }
    , width { state, "width" }
    , height { state, "height" }
    , focusable { state, "focusable" } {
    const BoxModel::ScopedCallbackLock boxModelLock { boxModel(*this) };

    if (!focusable.exists()) {
        focusable = true;
    }

    value.onValueChange = [this]() { getProgressBar().setValue(juce::jlimit(0.0, 1.0, value.get())); };
    getProgressBar().setValue(juce::jlimit(0.0, 1.0, value.get()));

    getProgressBar().setPercentageDisplay(false);

    if (width.isAuto()) {
        width = "135";
    }
    if (height.isAuto()) {
        height = "20";
    }
}

bool ProgressBar::isContainer() const {
    return false;
}

NormalisedProgressBar& ProgressBar::getProgressBar() {
    return *dynamic_cast<NormalisedProgressBar*>(getComponent().get());
}

const NormalisedProgressBar& ProgressBar::getProgressBar() const {
    return *dynamic_cast<const NormalisedProgressBar*>(getComponent().get());
}
} // namespace jive

//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_ComboBox.h"

namespace jive {
ComboBox::ComboBox(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator(std::move(itemToDecorate))
    , editable { state, "editable" }
    , tooltip { state, "tooltip" }
    , selected { state, "selected" }
    , width { state, "width" }
    , height { state, "height" }
    , focusable { state, "focusable" } {
    const BoxModel::ScopedCallbackLock boxModelLock { boxModel(*this) };

    if (!focusable.exists()) {
        focusable = true;
    }

    editable.onValueChange = [this]() { getComboBox().setEditableText(editable); };
    getComboBox().setEditableText(editable);

    tooltip.onValueChange = [this]() { getComboBox().setTooltip(tooltip); };
    getComboBox().setTooltip(tooltip);

    selected.onValueChange = [this]() { getComboBox().setSelectedItemIndex(selected); };
    getComboBox().setSelectedItemIndex(selected);

    getComboBox().addListener(this);

    if (width.isAuto()) {
        width = 50.0f;
    }
    if (height.isAuto()) {
        height = 20.0f;
    }
}

bool ComboBox::isContainer() const {
    return false;
}

#if JIVE_IS_PLUGIN_PROJECT
void ComboBox::attachToParameter(juce::RangedAudioParameter* parameter, juce::UndoManager* undoManager) {
    if (parameter != nullptr) {
        parameterAttachment
            = std::make_unique<juce::ComboBoxParameterAttachment>(*parameter, getComboBox(), undoManager);
    } else {
        parameterAttachment = nullptr;
    }
}
#endif

juce::ComboBox& ComboBox::getComboBox() {
    return *dynamic_cast<juce::ComboBox*>(getComponent().get());
}

const juce::ComboBox& ComboBox::getComboBox() const {
    return *dynamic_cast<const juce::ComboBox*>(getComponent().get());
}

void ComboBox::comboBoxChanged(juce::ComboBox* box) {
    jassertquiet(box == &getComboBox());

    selected = getComboBox().getSelectedItemIndex();
}
} // namespace jive

//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_ComboBox.h"

namespace jive {
ComboBox::Option::Option(juce::ValueTree sourceTree, int itemIndex, juce::ComboBox& box)
    : tree { sourceTree }
    , comboBox { box }
    , index { itemIndex }
    , id { index + 1 }
    , text { tree, "text" }
    , enabled { tree, "enabled" }
    , selected { tree, "selected" } {
    comboBox.addItem(text, id);

    text.onValueChange = [this]() { comboBox.changeItemText(id, text); };

    selected.onValueChange = [this]() {
        if (selected) {
            comboBox.setSelectedItemIndex(index);
            wasSelected = true;
        } else if (wasSelected) {
            comboBox.setSelectedId(0);
            wasSelected = false;
        }
    };

    if (selected) {
        comboBox.setSelectedItemIndex(index);
        wasSelected = true;
    }

    enabled.onValueChange = [this]() { comboBox.setItemEnabled(id, enabled); };
    comboBox.setItemEnabled(id, enabled);
}

void ComboBox::Option::setSelected(bool shouldBeSelected) {
    selected = shouldBeSelected;
}

ComboBox::Header::Header(juce::ValueTree sourceTree, ComboBox& box)
    : comboBox { box }
    , text { sourceTree, "text" } {
    box.getComboBox().addSectionHeading(text);

    text.onValueChange = [this]() { comboBox.updateItems(); };
}

ComboBox::ComboBox(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator(std::move(itemToDecorate))
    , editable { state, "editable" }
    , tooltip { state, "tooltip" }
    , selected { state, "selected" }
    , width { state, "width" }
    , height { state, "height" }
    , focusable { state, "focusable" }
    , onChange { state, "on-change" } {
    const BoxModel::ScopedCallbackLock boxModelLock { boxModel(*this) };

    if (!focusable.exists()) {
        focusable = true;
    }

    editable.onValueChange = [this]() { getComboBox().setEditableText(editable); };
    getComboBox().setEditableText(editable);

    tooltip.onValueChange = [this]() { getComboBox().setTooltip(tooltip); };
    getComboBox().setTooltip(tooltip);

    selected.onValueChange = [this]() {
        getComboBox().setSelectedItemIndex(selected);

        auto currentlySelectedOption = state.getChildWithProperty("selected", true);

        if (currentlySelectedOption.isValid()) {
            currentlySelectedOption.setProperty("selected", false, nullptr);
        }

        if (selected < options.size()) {
            if (auto* selectedOption = options[selected]) {
                selectedOption->setSelected(true);
            }
        }
    };

    updateItems();
    getComboBox().setSelectedItemIndex(selected);
    getComboBox().addListener(this);

    state.addListener(this);

    if (width.isAuto()) {
        width = "50";
    }
    if (height.isAuto()) {
        height = "20";
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

void ComboBox::updateItems() {
    options.clear();
    getComboBox().clear();

    for (auto childState : state) {
        if (childState.hasType("Option")) {
            options.add(std::make_unique<Option>(childState, options.size(), getComboBox()));
        } else if (childState.hasType("Header")) {
            headers.add(std::make_unique<Header>(childState, *this));
        } else if (childState.hasType("Separator")) {
            getComboBox().addSeparator();
        }
    }
}

void ComboBox::valueTreeChildAdded(juce::ValueTree& parentState, juce::ValueTree& /* child */) {
    if (parentState != state) {
        return;
    }

    updateItems();
}

void ComboBox::valueTreeChildRemoved(juce::ValueTree& parentState, juce::ValueTree& /* child */, int /* index */) {
    if (parentState != state) {
        return;
    }

    updateItems();
}

void ComboBox::comboBoxChanged(juce::ComboBox* box) {
    jassertquiet(box == &getComboBox());

    selected = getComboBox().getSelectedItemIndex();
    onChange.triggerWithoutSelfCallback();
}
} // namespace jive

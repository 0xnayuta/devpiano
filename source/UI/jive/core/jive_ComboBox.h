//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_GuiItemDecorator.h"

namespace jive {
class ComboBox : public GuiItemDecorator, private juce::ComboBox::Listener {
public:
    explicit ComboBox(std::unique_ptr<GuiItem> itemToDecorate);

    bool isContainer() const override;

#if JIVE_IS_PLUGIN_PROJECT
    void attachToParameter(juce::RangedAudioParameter*, juce::UndoManager*) override;
#endif

    juce::ComboBox& getComboBox();
    const juce::ComboBox& getComboBox() const;

private:
    void comboBoxChanged(juce::ComboBox* box) final;

    Property<bool> editable;
    Property<juce::String> tooltip;
    Property<int> selected;
    Property<Length> width;
    Property<Length> height;
    Property<bool> focusable;

#if JIVE_IS_PLUGIN_PROJECT
    std::unique_ptr<juce::ComboBoxParameterAttachment> parameterAttachment;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComboBox)
};
} // namespace jive

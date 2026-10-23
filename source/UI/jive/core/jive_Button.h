//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_GuiItemDecorator.h"

namespace jive {
class Button : public GuiItemDecorator,
               private juce::Button::Listener,
               private juce::ComponentListener,
               private juce::ValueTree::Listener {
public:
    enum class TriggerEvent { mouseUp, mouseDown };

    explicit Button(std::unique_ptr<GuiItem> itemToDecorate);
    ~Button() override;

#if JIVE_IS_PLUGIN_PROJECT
    void attachToParameter(juce::RangedAudioParameter*, juce::UndoManager*) override;
#endif

    juce::Button& getButton();
    const juce::Button& getButton() const;

private:
    void buttonClicked(juce::Button* button) final;
    void componentParentHierarchyChanged(juce::Component& component) final;
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) final;

    PassiveProperty<bool> toggleable;
    PassiveProperty<bool> toggled;
    PassiveProperty<bool> toggleOnClick;
    PassiveProperty<int> radioGroup;
    PassiveProperty<TriggerEvent> triggerEvent;
    PassiveProperty<juce::String> tooltip;
    PassiveProperty<juce::String, Accumulation::accumulate> text;
    PassiveProperty<juce::FlexBox::Direction> flexDirection;
    PassiveProperty<juce::FlexBox::JustifyContent> justifyContent;
    PassiveProperty<juce::BorderSize<float>> padding;
    PassiveProperty<float> minWidth;
    PassiveProperty<float> minHeight;
    PassiveProperty<bool> focusable;

#if JIVE_IS_PLUGIN_PROJECT
    std::unique_ptr<juce::ButtonParameterAttachment> parameterAttachment;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Button)
};
} // namespace jive

namespace juce {
template <> class VariantConverter<jive::Button::TriggerEvent> {
public:
    static jive::Button::TriggerEvent fromVar(const var& v);
    static var toVar(const jive::Button::TriggerEvent& event);

private:
    static const Array<var> options;
};
} // namespace juce

//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_components.h"

#include "jive_GuiItemDecorator.h"

namespace jive {
class Text : public GuiItemDecorator, private TextComponent::Listener {
public:
    explicit Text(std::unique_ptr<GuiItem> itemToDecorate);
    ~Text() override;

    void insertChild(std::unique_ptr<GuiItem> child, int index) override;
    void setChildren(std::vector<std::unique_ptr<GuiItem>>&& newChildren) override;

    bool isContainer() const override;
    bool isContent() const override;

#if JIVE_IS_PLUGIN_PROJECT
    void attachToParameter(juce::RangedAudioParameter*, juce::UndoManager*) override;
#endif

    TextComponent& getTextComponent();
    const TextComponent& getTextComponent() const;

private:
    void textFontChanged(TextComponent& text) final;

    juce::TextLayout buildTextLayout(float maxWidth = -1.0f) const;

    void updateTextComponent();

    Property<juce::String> text;
    Property<float, Inheritance::inheritFromAncestors> lineSpacing;
    Property<juce::Justification> justification;
    Property<juce::AttributedString::WordWrap> wordWrap;
    Property<juce::AttributedString::ReadingDirection> direction;
    Property<float> idealWidth;
    Property<float> idealHeight;

#if JIVE_IS_PLUGIN_PROJECT
    juce::RangedAudioParameter* parameter;
    std::unique_ptr<juce::ParameterAttachment> parameterAttachment;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Text)
};

const Text* findFirstTextContent(const GuiItem& item);
} // namespace jive

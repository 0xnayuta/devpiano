//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_components.h"

#include "UI/jive/core/layout/jive_GuiItemDecorator.h"

namespace jive {
class Text : public GuiItemDecorator, private TextComponent::Listener, private juce::ValueTree::Listener {
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
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) final;

    void updateTextComponent();

    PassiveProperty<juce::String> text;
    // line-spacing 仍需向祖先继承，保留其主动监听。
    Property<float, Inheritance::inheritFromAncestors> lineSpacing;
    PassiveProperty<juce::Justification> justification;
    PassiveProperty<juce::AttributedString::WordWrap> wordWrap;
    PassiveProperty<juce::AttributedString::ReadingDirection> direction;
    PassiveProperty<float> idealWidth;
    PassiveProperty<float> idealHeight;
    juce::Point<float> previousNaturalSize { -1.0f, -1.0f };
    float previousLineSpacing = -1.0f;
    juce::AttributedString::WordWrap previousWordWrap = juce::AttributedString::WordWrap::byWord;
    juce::AttributedString::ReadingDirection previousDirection = juce::AttributedString::ReadingDirection::natural;

#if JIVE_IS_PLUGIN_PROJECT
    juce::RangedAudioParameter* parameter;
    std::unique_ptr<juce::ParameterAttachment> parameterAttachment;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Text)
};

const Text* findFirstTextContent(const GuiItem& item);
} // namespace jive

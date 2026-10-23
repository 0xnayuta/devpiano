//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_Canvas.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jive {
class TextComponent : public juce::Component {
public:
    class AccessibilityHandler;
    class AccessibilityInterface;

    struct Listener {
        virtual ~Listener() = default;

        virtual void textFontChanged(TextComponent& text) = 0;
    };

    TextComponent();

    void resized() override;

    void setDirection(juce::AttributedString::ReadingDirection direction);

    [[nodiscard]] juce::Font getFont() const;
    void setFont(const juce::Font& font);

    void setJustification(juce::Justification justification);

    void setLineSpacing(float spacing);

    [[nodiscard]] const juce::String& getText() const;
    void setText(const juce::String& text);

    [[nodiscard]] juce::Colour getTextColour() const;
    void setTextColour(juce::Colour newColour);

    void setWordWrap(juce::AttributedString::WordWrap wrap);

    void clearAttributes();
    void append(const juce::AttributedString& attributedStringToAppend);

    [[nodiscard]] juce::AttributedString getAttributedString() const;

    /** 返回按给定最大宽度缓存的布局；引用仅在下次调用前有效。 */
    [[nodiscard]] const juce::TextLayout& getTextLayout(float maxWidth) const;

    void addListener(Listener&) const;
    void removeListener(Listener&) const;

private:
    struct CachedTextLayout {
        float maxWidth;
        juce::TextLayout layout;
    };

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    void updateTextLayoutsSource(const juce::AttributedString& attributedString) const;

    static constexpr std::size_t maxNumCachedTextLayouts = 16;

    juce::AttributedString::ReadingDirection direction { juce::AttributedString::ReadingDirection::natural };
    juce::Font font { juce::FontOptions {} };
    juce::Justification justification { juce::Justification::topLeft };
    float lineSpacing { 0.0f };
    juce::String text;
    std::optional<juce::Colour> textColour { juce::Colours::black };
    juce::AttributedString::WordWrap wordWrap { juce::AttributedString::WordWrap::byWord };
    juce::Array<juce::AttributedString> appendices;

    mutable juce::AttributedString textLayoutsSource;
    mutable bool textLayoutsSourceMayBeOutOfDate { true };
    mutable std::vector<CachedTextLayout> textLayouts;

    Canvas canvas;

    mutable juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TextComponent)
};
} // namespace jive

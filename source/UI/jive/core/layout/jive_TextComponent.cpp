//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "UI/jive/core/layout/jive_TextComponent.h"

#include <algorithm>
#include <iterator>

namespace jive {
TextComponent::TextComponent() {
    canvas.onPaint = [this](juce::Graphics& g) {
        if (dynamic_cast<TextComponent*>(getParentComponent()) != nullptr) {
            return;
        }

        const auto bounds = getLocalBounds().toFloat();
        getTextLayout(bounds.getWidth()).draw(g, bounds);
    };
    canvas.setAlwaysOnTop(true);
    canvas.setBufferedToImage(true);
    addAndMakeVisible(canvas);

    setInterceptsMouseClicks(false, false);
}

void TextComponent::resized() {
    canvas.setBounds(getLocalBounds());
}

const juce::String& TextComponent::getText() const {
    return text;
}

void TextComponent::setText(const juce::String& newText) {
    if (newText != text) {
        text = newText;
        textLayoutsSourceMayBeOutOfDate = true;
        canvas.repaint();
    }
}

juce::Font TextComponent::getFont() const {
    return font;
}

void TextComponent::setFont(const juce::Font& newFont) {
    if (newFont != font) {
        font = newFont;
        textLayoutsSourceMayBeOutOfDate = true;
        listeners.call(&Listener::textFontChanged, *this);
        canvas.repaint();
    }
}

void TextComponent::setJustification(juce::Justification newJustification) {
    if (newJustification != justification) {
        justification = newJustification;
        textLayoutsSourceMayBeOutOfDate = true;
        canvas.repaint();
    }
}

void TextComponent::setWordWrap(juce::AttributedString::WordWrap newWordWrap) {
    if (newWordWrap != wordWrap) {
        wordWrap = newWordWrap;
        textLayoutsSourceMayBeOutOfDate = true;
        canvas.repaint();
    }
}

void TextComponent::setDirection(juce::AttributedString::ReadingDirection newDirection) {
    if (newDirection != direction) {
        direction = newDirection;
        textLayoutsSourceMayBeOutOfDate = true;
        canvas.repaint();
    }
}

void TextComponent::setLineSpacing(float newLineSpacing) {
    if (!juce::approximatelyEqual(newLineSpacing, lineSpacing)) {
        lineSpacing = newLineSpacing;
        textLayoutsSourceMayBeOutOfDate = true;
        canvas.repaint();
    }
}

juce::Colour TextComponent::getTextColour() const {
    return textColour.value_or(juce::Colours::transparentBlack);
}

void TextComponent::setTextColour(juce::Colour newColour) {
    if (newColour != textColour) {
        textColour = newColour;
        textLayoutsSourceMayBeOutOfDate = true;
        canvas.repaint();
    }
}

void TextComponent::clearAttributes() {
    appendices.clear();
    textLayoutsSourceMayBeOutOfDate = true;
    canvas.repaint();
}

void TextComponent::append(const juce::AttributedString& attributedStringToAppend) {
    appendices.add(attributedStringToAppend);
    textLayoutsSourceMayBeOutOfDate = true;
    canvas.repaint();
}

juce::AttributedString TextComponent::getAttributedString() const {
    juce::AttributedString attributedString;

    attributedString.setText(text);

    attributedString.setFont(font);
    attributedString.setJustification(justification);
    attributedString.setLineSpacing(lineSpacing);
    attributedString.setReadingDirection(direction);
    attributedString.setWordWrap(wordWrap);

    for (const auto& appendix : appendices) {
        attributedString.append(appendix);
    }

    if (textColour.has_value()) {
        attributedString.setColour(*textColour);
    } else if (appendices.size() == 0) {
        attributedString.setColour(juce::Colours::black);
    }

    return attributedString;
}

static bool isEquivalent(const juce::AttributedString& a, const juce::AttributedString& b) {
    if (a.getText() != b.getText() || a.getJustification() != b.getJustification() || a.getWordWrap() != b.getWordWrap()
        || a.getReadingDirection() != b.getReadingDirection()
        || !juce::exactlyEqual(a.getLineSpacing(), b.getLineSpacing())
        || a.getNumAttributes() != b.getNumAttributes()) {
        return false;
    }

    for (auto i = 0; i < a.getNumAttributes(); ++i) {
        const auto& first = a.getAttribute(i);
        const auto& second = b.getAttribute(i);

        if (first.range != second.range || first.colour != second.colour || first.font != second.font) {
            return false;
        }
    }

    return true;
}

// 较窄宽度下的换行位置与创建宽度一致时可直接复用，避免重复 shaping。
static bool canBeReusedAt(const juce::TextLayout& layout, float createdAtMaxWidth, float maxWidth,
                          juce::AttributedString::WordWrap wordWrap) {
    return wordWrap == juce::AttributedString::WordWrap::none || juce::exactlyEqual(maxWidth, createdAtMaxWidth)
        || (layout.getWidth() <= maxWidth && maxWidth <= createdAtMaxWidth);
}

const juce::TextLayout& TextComponent::getTextLayout(float maxWidth) const {
    if (textLayoutsSourceMayBeOutOfDate) {
        updateTextLayoutsSource(getAttributedString());
    }

    const auto reusable
        = std::find_if(textLayouts.begin(), textLayouts.end(), [this, maxWidth](const CachedTextLayout& cached) {
              return canBeReusedAt(cached.layout, cached.maxWidth, maxWidth, textLayoutsSource.getWordWrap());
          });

    if (reusable != textLayouts.end()) {
        std::rotate(reusable, std::next(reusable), textLayouts.end());
        return textLayouts.back().layout;
    }

    if (textLayouts.size() >= maxNumCachedTextLayouts) {
        textLayouts.erase(textLayouts.begin());
    }

    auto& created = textLayouts.emplace_back(CachedTextLayout { maxWidth, {} });
    created.layout.createLayout(textLayoutsSource, maxWidth);
    return created.layout;
}

void TextComponent::updateTextLayoutsSource(const juce::AttributedString& attributedString) const {
    if (!isEquivalent(textLayoutsSource, attributedString)) {
        textLayoutsSource = attributedString;
        textLayouts.clear();
    }

    textLayoutsSourceMayBeOutOfDate = false;
}

void TextComponent::addListener(Listener& listener) const {
    listeners.add(&listener);
}

void TextComponent::removeListener(Listener& listener) const {
    listeners.remove(&listener);
}

class TextComponent::AccessibilityInterface : public juce::AccessibilityTextValueInterface {
public:
    explicit AccessibilityInterface(TextComponent& componentToRepresent)
        : textComponent { componentToRepresent } {
    }

    bool isReadOnly() const override {
        return true;
    }

    juce::String getCurrentValueAsString() const override {
        return textComponent.getText();
    }

    void setValueAsString(const juce::String& newValue) override {
        textComponent.setText(newValue);
    }

private:
    TextComponent& textComponent;
};

class TextComponent::AccessibilityHandler : public juce::AccessibilityHandler {
public:
    explicit AccessibilityHandler(TextComponent& componentToRepresent)
            : juce::AccessibilityHandler{
                componentToRepresent,
                juce::AccessibilityRole::staticText,
                juce::AccessibilityActions{},
                {
                    std::make_unique<AccessibilityInterface>(componentToRepresent),
                },
            }
            , textComponent{ componentToRepresent }
        {
    }

    juce::String getTitle() const override {
        if (textComponent.getTitle().isNotEmpty()) {
            return textComponent.getTitle();
        }

        return textComponent.getText();
    }

    juce::String getDescription() const override {
        if (textComponent.getDescription().isNotEmpty()) {
            return textComponent.getDescription();
        }

        if (textComponent.getTitle().isNotEmpty()) {
            return textComponent.getText();
        }

        return "";
    }

private:
    TextComponent& textComponent;
};

std::unique_ptr<juce::AccessibilityHandler> TextComponent::createAccessibilityHandler() {
    return std::make_unique<AccessibilityHandler>(*this);
}
} // namespace jive

//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Text.h"

#include "jive_CommonGuiItem.h"
#include "jive_ContainerItem.h"

namespace jive {
Text::Text(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator { std::move(itemToDecorate) }
    , text { state, "text" }
    , lineSpacing { state, "line-spacing" }
    , justification { state, "justification" }
    , wordWrap { state, "word-wrap" }
    , direction { state, "direction" }
    , idealWidth { state, "ideal-width" }
    , idealHeight { state, "ideal-height" } {
    const BoxModel::ScopedCallbackLock boxModelLock { boxModel(*this) };

    if (!justification.exists()) {
        justification = juce::Justification::centredLeft;
    }
    if (!wordWrap.exists()) {
        wordWrap = juce::AttributedString::WordWrap::byWord;
    }
    if (!direction.exists()) {
        direction = juce::AttributedString::ReadingDirection::natural;
    }

    text.onValueChange = [this]() { updateTextComponent(); };
    justification.onValueChange = [this]() { updateTextComponent(); };
    wordWrap.onValueChange = [this]() { updateTextComponent(); };
    direction.onValueChange = [this]() { updateTextComponent(); };
    lineSpacing.onValueChange = [this]() { updateTextComponent(); };

    state.setProperty("ideal-height", juce::var { [this](const juce::var::NativeFunctionArgs& args) {
                          const auto layout = buildTextLayout(args.arguments[0]);
                          return std::ceil(layout.getHeight());
                      } },
                      nullptr);

    updateTextComponent();
    getTextComponent().addListener(*this);
}

Text::~Text() {
    getTextComponent().removeListener(*this);
}

void Text::insertChild(std::unique_ptr<GuiItem> child, int index) {
    GuiItemDecorator::insertChild(std::move(child), index);
    updateTextComponent();
}

void Text::setChildren(std::vector<std::unique_ptr<GuiItem>>&& newChildren) {
    GuiItemDecorator::setChildren(std::move(newChildren));
    updateTextComponent();
}

bool Text::isContainer() const {
    return false;
}

bool Text::isContent() const {
    return true;
}

#if JIVE_IS_PLUGIN_PROJECT
void Text::attachToParameter(juce::RangedAudioParameter* newParameter, juce::UndoManager* undoManager) {
    parameter = newParameter;

    if (parameter != nullptr) {
        const auto onChange = [this](float) { text = parameter->getCurrentValueAsText(); };
        parameterAttachment = std::make_unique<juce::ParameterAttachment>(*parameter, onChange, undoManager);
        text = parameter->getCurrentValueAsText();
    } else {
        parameterAttachment = nullptr;
    }
}
#endif

TextComponent& Text::getTextComponent() {
    return dynamic_cast<TextComponent&>(*getComponent());
}

const TextComponent& Text::getTextComponent() const {
    return dynamic_cast<const TextComponent&>(*getComponent());
}

void Text::textFontChanged(TextComponent&) {
    updateTextComponent();
}

juce::TextLayout Text::buildTextLayout(float maxWidth) const {
    for (auto* parentItem = getParent(); maxWidth < 0.0f && parentItem != nullptr;
         parentItem = parentItem->getParent()) {
        if (const auto& parentBoxModel
            = dynamic_cast<const GuiItemDecorator*>(getParent())->toType<CommonGuiItem>()->boxModel;
            !parentBoxModel.hasAutoWidth()) {
            maxWidth = parentBoxModel.getContentBounds().getWidth();
        }
    }

    juce::TextLayout layout;
    layout.createLayout(getTextComponent().getAttributedString(), maxWidth);

    return layout;
}

template <typename T> [[nodiscard]] static auto nextWholeNumberAbove(T value) {
    static_assert(std::is_floating_point<T>());
    const auto ceiled = std::ceil(value);

    if (juce::approximatelyEqual(value, ceiled)) {
        return ceiled + static_cast<T>(1);
    }

    return ceiled;
}

void Text::updateTextComponent() {
    getTextComponent().setDirection(direction);
    getTextComponent().setJustification(justification);
    getTextComponent().setLineSpacing(lineSpacing);
    getTextComponent().setText(text);
    getTextComponent().setWordWrap(wordWrap);
    getTextComponent().clearAttributes();

    for (auto* child : getChildren()) {
        if (const auto* nestedText = dynamic_cast<const GuiItemDecorator*>(child)->toType<const Text>()) {
            getTextComponent().append(nestedText->getTextComponent().getAttributedString());
        } else {
            jassertfalse;
        }
    }

    idealWidth = nextWholeNumberAbove(
        buildTextLayout(static_cast<float>(std::numeric_limits<juce::uint16>::max())).getWidth());

    if (auto* parentItem = getParent()) {
        if (!parentItem->isContainer()) {
            getTextComponent().setAccessible(false);
        }

        if (auto* containerParent
            = dynamic_cast<GuiItemDecorator&>(*parentItem).getTopLevelDecorator().toType<ContainerItem>()) {
            containerParent->updateIdealSizeUnrestrained();
        }
    }
}

const Text* findFirstTextContent(const GuiItem& item) {
    if (auto* text = dynamic_cast<const Text*>(&item)) {
        return text;
    }

    for (const auto* child : item.getChildren()) {
        auto* text = findFirstTextContent(*child);

        if (text != nullptr) {
            return text;
        }
    }

    return nullptr;
}
} // namespace jive

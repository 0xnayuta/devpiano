//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Button.h"

namespace jive {
static void triggerClick(juce::Button& button) {
    button.triggerClick();
}

[[nodiscard]] static juce::Button* findFirstChildButton(const juce::Component& container) {
    for (auto* child : container.getChildren()) {
        if (auto* button = dynamic_cast<juce::Button*>(child)) {
            return button;
        }
    }

    return nullptr;
}

Button::Button(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator { std::move(itemToDecorate) }
    , toggleable { state, "toggleable" }
    , toggled { state, "toggled" }
    , toggleOnClick { state, "toggle-on-click" }
    , radioGroup { state, "radio-group" }
    , triggerEvent { state, "trigger-event" }
    , tooltip { state, "tooltip" }
    , text { state, "text" }
    , flexDirection { state, "flex-direction" }
    , justifyContent { state, "justify-content" }
    , padding { state, "padding" }
    , minWidth { state, "min-width" }
    , minHeight { state, "min-height" }
    , focusable { state, "focusable" }
    , onClick { state, "on-click" } {
    const BoxModel::ScopedCallbackLock boxModelLock { boxModel(*this) };

    if (!triggerEvent.exists()) {
        triggerEvent = TriggerEvent::mouseUp;
    }
    if (!padding.exists()) {
        padding = juce::BorderSize { 0.0f, 5.0f, 0.0f, 5.0f };
    }
    if (!minWidth.exists()) {
        minWidth = 50.0f;
    }
    if (!minHeight.exists()) {
        minHeight = 20.0f;
    }
    if (!focusable.exists()) {
        focusable = true;
    }

    toggleable.onValueChange = [this]() { getButton().setToggleable(toggleable); };
    getButton().setToggleable(toggleable);

    toggled.onValueChange = [this]() { getButton().setToggleState(toggled, juce::sendNotification); };
    getButton().setToggleState(toggled, juce::sendNotification);

    toggleOnClick.onValueChange = [this]() { getButton().setClickingTogglesState(toggleOnClick); };
    getButton().setClickingTogglesState(toggleOnClick);

    radioGroup.onValueChange = [this]() { getButton().setRadioGroupId(radioGroup); };
    getButton().setRadioGroupId(radioGroup);

    triggerEvent.onValueChange
        = [this]() { getButton().setTriggeredOnMouseDown(triggerEvent == TriggerEvent::mouseDown); };
    getButton().setTriggeredOnMouseDown(triggerEvent == TriggerEvent::mouseDown);

    tooltip.onValueChange = [this]() { getButton().setTooltip(tooltip); };
    getButton().setTooltip(tooltip);

    text.onValueChange = [this]() { getButton().setTitle(text); };
    getButton().setTitle(text);

    onClick.onTrigger = [this]() { triggerClick(getButton()); };
    getButton().addListener(this);
    getButton().addComponentListener(this);
}

Button::~Button() {
    getButton().removeComponentListener(this);
    getButton().removeListener(this);
}

#if JIVE_IS_PLUGIN_PROJECT
void Button::attachToParameter(juce::RangedAudioParameter* parameter, juce::UndoManager* undoManager) {
    if (parameter != nullptr) {
        parameterAttachment = std::make_unique<juce::ButtonParameterAttachment>(*parameter, getButton(), undoManager);
    } else {
        parameterAttachment = nullptr;
    }
}
#endif

juce::Button& Button::getButton() {
    return *dynamic_cast<juce::Button*>(getComponent().get());
}

const juce::Button& Button::getButton() const {
    return *dynamic_cast<const juce::Button*>(getComponent().get());
}

void Button::buttonClicked(juce::Button* button) {
    jassertquiet(button == &getButton());

    toggled = getButton().getToggleState();
    onClick.triggerWithoutSelfCallback();
}

void Button::componentParentHierarchyChanged(juce::Component& comp) {
    jassertquiet(&comp == &getButton());

    if (radioGroup.get() != 0) {
        if (auto* parentComponent = getButton().getParentComponent()) {
            toggled = findFirstChildButton(*parentComponent) == &getButton();
        }
    }
}
} // namespace jive

namespace juce {
const Array<var> VariantConverter<jive::Button::TriggerEvent>::options = {
    "mouse-up",
    "mouse-down",
};

jive::Button::TriggerEvent VariantConverter<jive::Button::TriggerEvent>::fromVar(const var& v) {
    jassert(options.contains(v));
    return static_cast<jive::Button::TriggerEvent>(options.indexOf(v));
}

var VariantConverter<jive::Button::TriggerEvent>::toVar(const jive::Button::TriggerEvent& event) {
    const auto index = static_cast<int>(event);

    jassert(options.size() >= index);
    return options[index];
}
} // namespace juce

//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Slider.h"

#include "jive_CommonGuiItem.h"

namespace jive {
Slider::Slider(std::unique_ptr<GuiItem> itemToDecorate)
    : Slider { std::move(itemToDecorate), 135.0f, 20.0f } {
}

Slider::Slider(std::unique_ptr<GuiItem> itemToDecorate, float defaultWidth, float defaultHeight)
    : GuiItemDecorator { std::move(itemToDecorate) }
    , value { state, "value" }
    , min { state, "min" }
    , max { state, "max" }
    , mid { state, "mid" }
    , interval { state, "interval" }
    , orientation { state, "orientation" }
    , width { state, "width" }
    , height { state, "height" }
    , sensitivity { state, "sensitivity" }
    , isInVelocityMode { state, "velocity-mode" }
    , velocitySensitivity { state, "velocity-sensitivity" }
    , velocityThreshold { state, "velocity-threshold" }
    , velocityOffset { state, "velocity-offset" }
    , snapToMouse { state, "snap-to-mouse" }
    , focusable { state, "focusable" }
    , onChange { state, "on-change" } {
    const BoxModel::ScopedCallbackLock boxModelLock { boxModel(*this) };

    if (!max.exists()) {
        max = "1.0";
    }
    if (!sensitivity.exists()) {
        sensitivity = 1.0;
    }
    if (!velocitySensitivity.exists()) {
        velocitySensitivity = 1.0;
    }
    if (!velocityThreshold.exists()) {
        velocityThreshold = 1;
    }
    if (!snapToMouse.exists()) {
        snapToMouse = true;
    }
    if (!focusable.exists()) {
        focusable = true;
    }

    min.onValueChange = [this]() { updateRange(); };
    max.onValueChange = [this]() { updateRange(); };
    mid.onValueChange = [this]() { updateRange(); };
    interval.onValueChange = [this]() { updateRange(); };
    updateRange();

    value.onValueChange = [this]() { getSlider().setValue(getSlider().getValueFromText(value)); };
    getSlider().setValue(getSlider().getValueFromText(value));

    orientation.onValueChange = [this]() { updateStyle(); };
    width.onValueChange = [this]() { updateStyle(); };
    height.onValueChange = [this]() { updateStyle(); };
    updateStyle();

    sensitivity.onValueChange
        = [this]() { getSlider().setMouseDragSensitivity(juce::roundToInt(250.0 / sensitivity)); };
    getSlider().setMouseDragSensitivity(juce::roundToInt(250.0 / sensitivity));

    isInVelocityMode.onValueChange = [this]() { getSlider().setVelocityBasedMode(isInVelocityMode); };
    getSlider().setVelocityBasedMode(isInVelocityMode);

    velocitySensitivity.onValueChange
        = [this]() { getSlider().setVelocityModeParameters(velocitySensitivity, velocityThreshold, velocityOffset); };
    velocityThreshold.onValueChange
        = [this]() { getSlider().setVelocityModeParameters(velocitySensitivity, velocityThreshold, velocityOffset); };
    velocityOffset.onValueChange
        = [this]() { getSlider().setVelocityModeParameters(velocitySensitivity, velocityThreshold, velocityOffset); };
    getSlider().setVelocityModeParameters(velocitySensitivity, velocityThreshold, velocityOffset);

    snapToMouse.onValueChange = [this]() { getSlider().setSliderSnapsToMousePosition(snapToMouse); };
    getSlider().setSliderSnapsToMousePosition(snapToMouse);

    getSlider().setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    getSlider().addListener(this);

    if (width.isAuto()) {
        width = juce::String { defaultWidth };
    }
    if (height.isAuto()) {
        height = juce::String { defaultHeight };
    }
}

bool Slider::isContainer() const {
    return false;
}

#if JIVE_IS_PLUGIN_PROJECT
void Slider::attachToParameter(juce::RangedAudioParameter* parameter, juce::UndoManager* undoManager) {
    if (parameter != nullptr) {
        parameterAttachment = std::make_unique<juce::SliderParameterAttachment>(*parameter, getSlider(), undoManager);
    } else {
        parameterAttachment = nullptr;
    }
}
#endif

juce::Slider& Slider::getSlider() {
    return *dynamic_cast<juce::Slider*>(getComponent().get());
}

const juce::Slider& Slider::getSlider() const {
    return *dynamic_cast<const juce::Slider*>(getComponent().get());
}

void Slider::updateStyle() {
    Orientation ori;

    if (orientation.isAuto()) {
        const auto& boxModel = toType<CommonGuiItem>()->boxModel;

        if (boxModel.getWidth() < boxModel.getHeight()) {
            ori = Orientation::vertical;
        } else {
            ori = Orientation::horizontal;
        }
    } else {
        ori = orientation.getOr(Orientation::horizontal);
    }

    getSlider().setSliderStyle(getStyleForOrientation(ori));
}

void Slider::sliderValueChanged(juce::Slider* slider) {
    if (slider != &getSlider()) {
        return;
    }

    value = getSlider().getTextFromValue(getSlider().getValue());
    onChange.triggerWithoutSelfCallback();
}

juce::Slider::SliderStyle Slider::getStyleForOrientation(Orientation ori) {
    switch (ori) {
    case Orientation::horizontal:
        return juce::Slider::SliderStyle::LinearHorizontal;
    case Orientation::vertical:
        return juce::Slider::SliderStyle::LinearVertical;
    }

    jassertfalse;
    return {};
}

void Slider::updateRange() {
    auto& slider = getSlider();
    juce::NormalisableRange<double> range { slider.getValueFromText(min), slider.getValueFromText(max) };

    if (mid.exists()) {
        range.setSkewForCentre(slider.getValueFromText(mid));
    }
    if (interval.exists()) {
        range.interval = slider.getValueFromText(interval);
    }

    slider.setNormalisableRange(range);
}
} // namespace jive

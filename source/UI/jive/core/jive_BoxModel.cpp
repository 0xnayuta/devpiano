//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_BoxModel.h"

namespace jive {
BoxModel::BoxModel(juce::ValueTree stateSource)
    : state { stateSource }
    , width { state, "width" }
    , height { state, "height" }
    , minWidth { state, "min-width" }
    , minHeight { state, "min-height" }
    , maxWidth { state, "max-width" }
    , maxHeight { state, "max-height" }
    , idealWidth { state, "ideal-width" }
    , idealHeight { state, "ideal-height" }
    , componentWidth { state, "component-width" }
    , componentHeight { state, "component-height" }
    , padding { state, "padding" }
    , border { state, "border-width" }
    , margin { state, "margin" }
    , callbackLock { state, "box-model-callback-lock" } {
    if (!width.exists()) {
        width.setAuto();
    }
    if (!height.exists()) {
        height.setAuto();
    }

    if (width.isAuto()) {
        componentWidth.clear();
    } else {
        componentWidth = width.toPixels(getParentBounds());
    }

    if (height.isAuto()) {
        componentHeight.clear();
    } else {
        componentHeight = height.toPixels(getParentBounds());
    }

    const auto informBoxModelChanged = [this]() { listeners.call(&Listener::boxModelChanged, *this); };
    const auto onBoxModelChanged = [this, informBoxModelChanged]() {
        if (callbackLock.get()) {
            return;
        }

        informBoxModelChanged();
    };
    const auto handleOnValueChange
        = [this, informBoxModelChanged](auto& property, bool updateWidth, bool updateHeight) {
              property.onValueChange = [this, informBoxModelChanged, &property, updateWidth, updateHeight] {
                  if (callbackLock.get()) {
                      return;
                  }

                  if (updateWidth && !width.isAuto()) {
                      componentWidth = width.toPixels(getParentBounds());
                  }

                  if (updateHeight && !height.isAuto()) {
                      componentHeight = height.toPixels(getParentBounds());
                  }

                  if (!property.isTransitioning()) {
                      if (!(updateWidth || updateHeight)) {
                          informBoxModelChanged();
                      }
                  }
              };
          };

    handleOnValueChange(width, true, false);
    handleOnValueChange(height, false, true);
    handleOnValueChange(padding, false, false);
    handleOnValueChange(border, false, false);
    handleOnValueChange(minWidth, false, false);
    handleOnValueChange(minHeight, false, false);
    handleOnValueChange(maxWidth, false, false);
    handleOnValueChange(maxHeight, false, false);

    idealWidth.onValueChange = onBoxModelChanged;
    idealHeight.onValueChange = onBoxModelChanged;
    componentWidth.onValueChange = [this, onBoxModelChanged]() {
        if (!componentWidth.isTransitioning()) {
            onBoxModelChanged();
        }
    };
    componentHeight.onValueChange = [this, onBoxModelChanged]() {
        if (!componentHeight.isTransitioning()) {
            onBoxModelChanged();
        }
    };
    margin.onValueChange = [this, onBoxModelChanged]() {
        if (!margin.isTransitioning()) {
            onBoxModelChanged();
        }
    };

    componentWidth.setTransitionSourceProperty(width.id);
    componentHeight.setTransitionSourceProperty(height.id);

    componentWidth.onTransitionProgressed = informBoxModelChanged;
    componentHeight.onTransitionProgressed = informBoxModelChanged;
    padding.onTransitionProgressed = informBoxModelChanged;
    border.onTransitionProgressed = informBoxModelChanged;
    margin.onTransitionProgressed = informBoxModelChanged;
}

float BoxModel::getWidth() const {
    if (auto* transition = componentWidth.getTransition()) {
        return transition->calculateCurrent<float>();
    }

    return componentWidth;
}

void BoxModel::setWidth(float newWidth) {
    componentWidth = newWidth;

    if (!state.getParent().isValid()) {
        width = juce::String { juce::roundToInt(newWidth) };
    }
}

bool BoxModel::hasAutoWidth() const {
    return width.isAuto();
}

float BoxModel::getHeight() const {
    if (auto* transition = componentHeight.getTransition()) {
        return transition->calculateCurrent<float>();
    }

    return componentHeight;
}

void BoxModel::setHeight(float newHeight) {
    componentHeight = newHeight;

    if (!state.getParent().isValid()) {
        height = juce::String { juce::roundToInt(newHeight) };
    }
}

bool BoxModel::hasAutoHeight() const {
    return height.isAuto();
}

void BoxModel::setSize(float newWidth, float newHeight) {
    const auto widthChanged = !juce::approximatelyEqual(newWidth, componentWidth.get());
    const auto heightChanged = !juce::approximatelyEqual(newHeight, componentHeight.get());
    std::unique_ptr<ScopedCallbackLock> scopedLock;

    if (widthChanged && heightChanged) {
        scopedLock = std::make_unique<ScopedCallbackLock>(*this);
    }

    if (widthChanged) {
        setWidth(newWidth);
    }

    scopedLock = nullptr;

    if (heightChanged) {
        setHeight(newHeight);
    }
}

juce::BorderSize<float> BoxModel::getPadding() const {
    if (auto* transition = padding.getTransition()) {
        return transition->calculateCurrent<juce::BorderSize<float>>();
    }

    return padding;
}

juce::BorderSize<float> BoxModel::getBorder() const {
    if (auto* transition = border.getTransition()) {
        return transition->calculateCurrent<juce::BorderSize<float>>();
    }

    return border;
}

juce::BorderSize<float> BoxModel::getMargin() const {
    if (auto* transition = margin.getTransition()) {
        return transition->calculateCurrent<juce::BorderSize<float>>();
    }

    return margin;
}

juce::Rectangle<float> BoxModel::getOuterBounds() const {
    return { getWidth(), getHeight() };
}

juce::Rectangle<float> BoxModel::getContentBounds() const {
    const auto bounds = getPadding().subtractedFrom(getBorder().subtractedFrom(getOuterBounds()));
    return bounds.withSize(juce::jmax(0.0f, bounds.getWidth()), juce::jmax(0.0f, bounds.getHeight()));
}

juce::Rectangle<float> BoxModel::getMinimumBounds() const {
    return juce::Rectangle<float> {
        minWidth.toPixels(getParentBounds()),
        minHeight.toPixels(getParentBounds()),
    };
}

juce::Rectangle<float> BoxModel::getMaximumBounds() const {
    return {
        maxWidth.exists() ? maxWidth.toPixels(getParentBounds()) : -1.0f,
        maxHeight.exists() ? maxHeight.toPixels(getParentBounds()) : -1.0f,
    };
}

void BoxModel::addListener(Listener& listener) const {
    const_cast<juce::ListenerList<Listener>*>(&listeners)->add(&listener);
}

void BoxModel::removeListener(Listener& listener) const {
    const_cast<juce::ListenerList<Listener>*>(&listeners)->remove(&listener);
}

juce::Rectangle<float> BoxModel::getParentBounds() const {
    if (state.getParent().isValid()) {
        const Property<float> parentWidth { state.getParent(), componentWidth.id };
        const Property<float> parentHeight { state.getParent(), componentHeight.id };
        return { parentWidth.get(), parentHeight.get() };
    }

    return {};
}

BoxModel::ScopedCallbackLock::ScopedCallbackLock(BoxModel& boxModelToLock)
    : boxModel { boxModelToLock } {
    boxModel.lock();
}

BoxModel::ScopedCallbackLock::~ScopedCallbackLock() {
    boxModel.unlock();
}

void BoxModel::lock() {
    callbackLock = true;
}

void BoxModel::unlock() {
    callbackLock.clear();
}
} // namespace jive

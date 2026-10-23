//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "UI/jive/core/geometry/jive_BoxModel.h"

namespace jive {
BoxModel::BoxModel(juce::ValueTree stateSource)
    : state { std::move(stateSource) }
    , width { state, "width" }
    , height { state, "height" }
    , minWidth { state, "min-width" }
    , minHeight { state, "min-height" }
    , maxWidth { state, "max-width" }
    , maxHeight { state, "max-height" }
    , idealWidth { state, ids::idealWidth() }
    , idealHeight { state, ids::idealHeight() }
    , componentWidth { state, ids::componentWidth() }
    , componentHeight { state, ids::componentHeight() }
    , padding { state, "padding" }
    , border { state, "border-width" }
    , margin { state, "margin" } {
    if (!width.exists()) {
        width.setAuto();
    }
    if (!height.exists()) {
        height.setAuto();
    }

    if (width.isAuto()) {
        componentWidth.clear();
    } else {
        componentWidth = width.get().toPixels(getParentBounds().getWidth());
    }

    if (height.isAuto()) {
        componentHeight.clear();
    } else {
        componentHeight = height.get().toPixels(getParentBounds().getHeight());
    }

    const auto informBoxModelChanged = [this]() { listeners.call(&Listener::boxModelChanged, *this); };
    const auto onBoxModelChanged = [this, informBoxModelChanged]() {
        if (callbackLock) {
            return;
        }

        informBoxModelChanged();
    };
    const auto handleOnValueChange
        = [this, informBoxModelChanged](auto& property, bool updateWidth, bool updateHeight) {
              property.onValueChange = [this, informBoxModelChanged, &property, updateWidth, updateHeight] {
                  if (callbackLock) {
                      return;
                  }

                  if (updateWidth && !width.isAuto()) {
                      componentWidth = width.get().toPixels(getParentBounds().getWidth());
                  }

                  if (updateHeight && !height.isAuto()) {
                      componentHeight = height.get().toPixels(getParentBounds().getHeight());
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

    state.addListener(this);
}

BoxModel::~BoxModel() {
    state.removeListener(this);
}

float BoxModel::getWidth() const {
    if (const auto* transition = componentWidth.getTransition()) {
        return transition->calculateCurrent<float>();
    }

    return componentWidth;
}

void BoxModel::setWidth(float newWidth) {
    componentWidth = newWidth;

    if (!state.getParent().isValid()) {
        width = Length { std::round(newWidth), Length::Unit::absolute };
    }
}

bool BoxModel::hasAutoWidth() const {
    return width.isAuto();
}

float BoxModel::getHeight() const {
    if (const auto* transition = componentHeight.getTransition()) {
        return transition->calculateCurrent<float>();
    }

    return componentHeight;
}

void BoxModel::setHeight(float newHeight) {
    componentHeight = newHeight;

    if (!state.getParent().isValid()) {
        height = Length { std::round(newHeight), Length::Unit::absolute };
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
    if (const auto* transition = padding.getTransition()) {
        return transition->calculateCurrent<juce::BorderSize<float>>();
    }

    return padding;
}

juce::BorderSize<float> BoxModel::getBorder() const {
    if (const auto* transition = border.getTransition()) {
        return transition->calculateCurrent<juce::BorderSize<float>>();
    }

    return border;
}

juce::BorderSize<float> BoxModel::getMargin() const {
    if (const auto* transition = margin.getTransition()) {
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
        minWidth.get().toPixels(getParentBounds().getWidth()),
        minHeight.get().toPixels(getParentBounds().getHeight()),
    };
}

juce::Rectangle<float> BoxModel::getMaximumBounds() const {
    return {
        maxWidth.exists() ? maxWidth.get().toPixels(getParentBounds().getWidth()) : -1.0f,
        maxHeight.exists() ? maxHeight.get().toPixels(getParentBounds().getHeight()) : -1.0f,
    };
}

juce::Rectangle<float> BoxModel::getExplicitConstraints() const {
    auto explicitSize = juce::Rectangle<float> {
        hasAutoWidth() ? static_cast<float>(std::numeric_limits<juce::uint16>::max()) : getWidth(),
        hasAutoHeight() ? static_cast<float>(std::numeric_limits<juce::uint16>::max()) : getHeight(),
    };

    const auto maximum = getMaximumBounds();

    // -1 表示未设置上限，保持无上限，不把它直接带进 rectangle 交集。
    if (maximum.getWidth() >= 0.0f) {
        explicitSize.setWidth(juce::jmin(explicitSize.getWidth(), maximum.getWidth()));
    }
    if (maximum.getHeight() >= 0.0f) {
        explicitSize.setHeight(juce::jmin(explicitSize.getHeight(), maximum.getHeight()));
    }

    return explicitSize;
}

std::uint64_t BoxModel::getConstraintsVersion() const {
    return constraintsVersion;
}

void BoxModel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    if (tree == state
        && (property == width.id || property == height.id || property == minWidth.id || property == minHeight.id
            || property == maxWidth.id || property == maxHeight.id || property == idealWidth.id
            || property == idealHeight.id || property == margin.id)) {
        // 锁内修改也必须让父布局看到输入失效。
        constraintsVersion++;
    }

    forwardPropertyChange(tree, property, componentWidth, componentHeight, width, height, idealWidth, idealHeight,
                          padding, border, margin, minWidth, minHeight, maxWidth, maxHeight);
}

void BoxModel::addListener(Listener& listener) const {
    const_cast<juce::ListenerList<Listener>*>(&listeners)->add(&listener);
}

void BoxModel::removeListener(Listener& listener) const {
    const_cast<juce::ListenerList<Listener>*>(&listeners)->remove(&listener);
}

juce::Rectangle<float> BoxModel::getParentBounds() const {
    if (const auto parent = state.getParent(); parent.isValid()) {
        return {
            fromVar<float>(parent[ids::componentWidth()]),
            fromVar<float>(parent[ids::componentHeight()]),
        };
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
    callbackLock = false;
}
} // namespace jive

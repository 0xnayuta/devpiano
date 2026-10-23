//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "UI/jive/core/layout/jive_ContainerItem.h"

#include "UI/jive/core/layout/jive_CommonGuiItem.h"

namespace jive {
ContainerItem::ContainerItem(std::unique_ptr<GuiItem> itemToDecorate)
    : GuiItemDecorator { std::move(itemToDecorate) }
    , box { boxModel(*this) }
    , idealWidth { state, ids::idealWidth() }
    , idealHeight { state, ids::idealHeight() } {
    box.addListener(*this);
    state.addListener(this);
}

ContainerItem::~ContainerItem() {
    state.removeListener(this);
    box.removeListener(*this);
}

bool ContainerItem::isSetupInProgress() const {
    return static_cast<bool>(state[ids::setupInProgress()]);
}

void ContainerItem::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    forwardPropertyChange(tree, property, idealWidth, idealHeight);

    const auto isOwnProperty = tree == state;

    if (property == idealWidth.id || property == idealHeight.id) {
        if (!isOwnProperty) {
            idealSizeIsOutOfDate = true;
        }
        return;
    }

    if (property == ids::componentWidth() || property == ids::componentHeight()) {
        if (isOwnProperty) {
            idealSizeIsOutOfDate = true;
        }
        childrenMayBeOutOfDate = true;
        return;
    }

    idealSizeIsOutOfDate = true;
    childrenMayBeOutOfDate = true;
}

void ContainerItem::insertChild(std::unique_ptr<GuiItem> child, int index) {
    const auto numChildrenBefore = getChildren().size();
    GuiItemDecorator::insertChild(std::move(child), index);

    if (getChildren().size() != numChildrenBefore && !isSetupInProgress()) {
        updateIdealSize();
    }
}

void ContainerItem::setChildren(std::vector<std::unique_ptr<GuiItem>>&& newChildren) {
    {
        const BoxModel::ScopedCallbackLock boxModelLock { box };
        GuiItemDecorator::setChildren(std::move(newChildren));
    }

    if (!getChildren().isEmpty() && !isSetupInProgress()) {
        updateIdealSize();
    }
}

juce::Rectangle<float> ContainerItem::getContentConstraints() const {
    auto constraints = box.getExplicitConstraints();

    if (const auto* thisParent = dynamic_cast<const GuiItemDecorator*>(getParent())) {
        if (const auto* parentContainer = thisParent->getTopLevelDecorator().toType<ContainerItem>()) {
            // 父内容约束带有 padding/border 平移，子项约束以零原点表达；
            // 先归零再比较尺寸，避免把 padding 二次计入子项宽度。
            constraints = constraints.getIntersection(parentContainer->getContentConstraints().withZeroOrigin());
        }
    }

    return box.getPadding().subtractedFrom(box.getBorder().subtractedFrom(constraints));
}

void ContainerItem::applyIdealSize(juce::Rectangle<float> newIdealSize, juce::Rectangle<float> constraints,
                                   bool informParent) {
    idealSizeWasLimitedByConstraints
        = newIdealSize.getWidth() >= constraints.getWidth() || newIdealSize.getHeight() >= constraints.getHeight();

    const auto widthChanged = !juce::approximatelyEqual(newIdealSize.getWidth(), idealWidth.get());
    const auto heightChanged = !juce::approximatelyEqual(newIdealSize.getHeight(), idealHeight.get());

    if (!(widthChanged || heightChanged)) {
        callLayoutChildrenWithRecursionLock();
        return;
    }

    if (widthChanged && heightChanged) {
        {
            BoxModel::ScopedCallbackLock boxModelLock { box };
            idealWidth = newIdealSize.getWidth();
        }
        idealHeight = newIdealSize.getHeight();
    } else if (widthChanged) {
        idealWidth = newIdealSize.getWidth();
    } else {
        idealHeight = newIdealSize.getHeight();
    }

    if (informParent && getParent() != nullptr && !isSetupInProgress()) {
        if (auto* containerParent
            = dynamic_cast<GuiItemDecorator&>(*getParent()).getTopLevelDecorator().toType<ContainerItem>()) {
            containerParent->updateIdealSize(true, false);
        }
    }

    callLayoutChildrenWithRecursionLock();
}

void ContainerItem::updateIdealSize(bool informParentOfChanges, bool measureChildren) {
    if (measureChildren) {
        updateChildrensIdealSizesIfOutOfDate();
    }

    explicitConstraintsWhenIdealSizeWasMeasured = box.getExplicitConstraints();

    const auto constraints = getContentConstraints();

    if (!measureChildren && constraints != constraintsWhenIdealSizeWasMeasured) {
        childrenMayBeOutOfDate = true;
    }

    constraintsWhenIdealSizeWasMeasured = constraints;
    idealSizeIsOutOfDate = false;

    applyIdealSize(calculateIdealSize(constraints), constraints, informParentOfChanges);
}

void ContainerItem::updateIdealSizeWithinContentBounds(bool informParentOfChanges) {
    updateChildrensIdealSizesIfOutOfDate();

    const auto constraints = box.getContentBounds();
    explicitConstraintsWhenIdealSizeWasMeasured = box.getExplicitConstraints();
    constraintsWhenIdealSizeWasMeasured = constraints;
    idealSizeIsOutOfDate = false;

    applyIdealSize(calculateIdealSize(constraints), constraints, informParentOfChanges);
}

void ContainerItem::invalidateIdealSize() {
    idealSizeIsOutOfDate = true;

    for (auto* ancestor = getParent(); ancestor != nullptr; ancestor = ancestor->getParent()) {
        if (auto* decorator = dynamic_cast<GuiItemDecorator*>(ancestor)) {
            if (auto* container = decorator->getTopLevelDecorator().toType<ContainerItem>()) {
                container->childrenMayBeOutOfDate = true;
            }
        }
    }
}

void ContainerItem::refreshLayout() {
    updateIdealSizeIfOutOfDate();
    callLayoutChildrenWithRecursionLock();
}

void ContainerItem::updateIdealSizeIfOutOfDate() {
    if (idealSizeIsOutOfDate || getContentConstraints() != constraintsWhenIdealSizeWasMeasured) {
        updateIdealSize(false);
        return;
    }

    if (!childrenMayBeOutOfDate) {
        return;
    }

    updateChildrensIdealSizesIfOutOfDate();

    if (idealSizeIsOutOfDate) {
        updateIdealSize(false, false);
    }
}

void ContainerItem::updateChildrensIdealSizesIfOutOfDate() {
    childrenMayBeOutOfDate = false;

    for (auto* child : getChildren()) {
        if (auto* decorator = dynamic_cast<GuiItemDecorator*>(child)) {
            if (auto* container = decorator->getTopLevelDecorator().toType<ContainerItem>()) {
                container->updateIdealSizeIfOutOfDate();
            }
        }
    }
}

void ContainerItem::boxModelChanged(BoxModel& boxModelThatChanged) {
    if (!idealSizeWasLimitedByConstraints || boxModelThatChanged.state != state || isSetupInProgress()) {
        return;
    }

    const auto constraints = box.getExplicitConstraints();

    if (constraints.getWidth() > explicitConstraintsWhenIdealSizeWasMeasured.getWidth()
        || constraints.getHeight() > explicitConstraintsWhenIdealSizeWasMeasured.getHeight()) {
        updateIdealSize();
    }
}
} // namespace jive

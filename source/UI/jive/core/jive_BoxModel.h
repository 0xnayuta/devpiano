//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "jive_Length.h"

#include "jive_Property.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jive {
class BoxModel : private juce::ValueTree::Listener {
public:
    struct Listener {
        virtual ~Listener() = default;

        virtual void boxModelChanged(BoxModel&) {
        }
    };

    /** Prevents any callbacks being invoked on the given BoxModel object
        until the lock instance is destroyed.

        Use with caution! This may cause things to break if the BoxModel
        isn't able to properly respond to vital changes in its given
        ValueTree. However, so long as you're sure you know what you're
        doing, this can be very useful when setting several properties at
        once and you don't want callbacks firing for each one.
    */
    class ScopedCallbackLock {
    public:
        explicit ScopedCallbackLock(BoxModel& boxModelToLock);
        ~ScopedCallbackLock();

    private:
        BoxModel& boxModel;
    };

    explicit BoxModel(juce::ValueTree sourceState);
    ~BoxModel() override;

    [[nodiscard]] float getWidth() const;
    void setWidth(float newWidth);
    [[nodiscard]] bool hasAutoWidth() const;
    [[nodiscard]] float getHeight() const;
    void setHeight(float newHeight);
    [[nodiscard]] bool hasAutoHeight() const;
    void setSize(float newWidth, float newHeight);

    [[nodiscard]] juce::BorderSize<float> getPadding() const;
    [[nodiscard]] juce::BorderSize<float> getBorder() const;
    [[nodiscard]] juce::BorderSize<float> getMargin() const;

    /** Returns the outer bounds of this item.
        The outer bounds is the area including the padding and border (but
        excluding the margin), so you can think of this as the sort of
        "physical" bounds of the item, i.e. the bounds of the component.
    */
    [[nodiscard]] juce::Rectangle<float> getOuterBounds() const;

    /** Returns the inner bounds of this item.
        This is the area that the item's content should occupy, i.e. the
        inner-most area within the border and padding.
     */
    [[nodiscard]] juce::Rectangle<float> getContentBounds() const;

    [[nodiscard]] juce::Rectangle<float> getMinimumBounds() const;
    [[nodiscard]] juce::Rectangle<float> getMaximumBounds() const;

    /** 返回显式尺寸约束；未设置 max 时保持无上限，不把负值哨兵参与交集。 */
    [[nodiscard]] juce::Rectangle<float> getExplicitConstraints() const;

    /** 父布局输入（尺寸、min/max、margin 等）的失效计数。 */
    [[nodiscard]] std::uint64_t getConstraintsVersion() const;

    void addListener(Listener& listener) const;
    void removeListener(Listener& listener) const;

    juce::ValueTree state;

private:
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) final;

    void lock();
    void unlock();

    juce::Rectangle<float> getParentBounds() const;

    PassiveProperty<Length> width;
    PassiveProperty<Length> height;
    PassiveProperty<Length> minWidth;
    PassiveProperty<Length> minHeight;
    PassiveProperty<Length> maxWidth;
    PassiveProperty<Length> maxHeight;
    PassiveProperty<float> idealWidth;
    PassiveProperty<float> idealHeight;
    PassiveProperty<float> componentWidth;
    PassiveProperty<float> componentHeight;
    PassiveProperty<juce::BorderSize<float>> padding;
    PassiveProperty<juce::BorderSize<float>> border;
    PassiveProperty<juce::BorderSize<float>> margin;
    bool callbackLock = false;
    std::uint64_t constraintsVersion = 0;

    juce::ListenerList<Listener> listeners;

    JUCE_LEAK_DETECTOR(BoxModel)
};
} // namespace jive

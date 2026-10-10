#pragma once

#include "UI/CustomKeyboard.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
// Viewport that owns a CustomKeyboard and injects it into a JIVE layout via
// the ComponentFactory.
//
// The keybed keeps its natural proportions and centres within taller or wider
// viewports. Narrow viewports retain the full keybed width for horizontal
// scrolling. The keyboard receives a visible height with the horizontal
// scrollbar thickness always reserved, so its vertical position depends only on
// the viewport frame and never shifts when the scrollbar appears or disappears.
// ============================================================================
class KeyboardViewport final : public juce::Viewport {
public:
    explicit KeyboardViewport(juce::MidiKeyboardState& keyboardState)
        : keyboard(std::make_unique<CustomKeyboard>(keyboardState)) {
        setScrollBarsShown(false, true, false, true); // horizontal only
        setViewedComponent(keyboard.get(), false);
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void resized() override {
        juce::Viewport::resized();

        const auto visibleWidth = getMaximumVisibleWidth();

        // 竖直几何必须与横向滚动条的出现与否解耦：滚动条只在键床宽于视口时出现，
        // 而这取决于窗口宽度。getMaximumVisibleHeight() 会随滚动条收缩，直接消费它
        // 会让键床在拖动窗口宽度跨越阈值时整体上下跳动。这里恒定预留横向滚动条厚度，
        // 使键床的竖直位置只由视口外框高度决定。
        const auto reservedForScrollbar = isHorizontalScrollBarShown() ? getScrollBarThickness() : 0;
        const auto stableVisibleHeight = juce::jmax(0, getHeight() - reservedForScrollbar);
        if (stableVisibleHeight > 0 || visibleWidth > 0) {
            keyboard->updateViewportBounds(visibleWidth, stableVisibleHeight);
        }
    }

    [[nodiscard]] CustomKeyboard& getCustomKeyboard() noexcept {
        return *keyboard;
    }

private:
    std::unique_ptr<CustomKeyboard> keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardViewport)
};

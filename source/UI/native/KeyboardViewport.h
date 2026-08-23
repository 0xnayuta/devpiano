#pragma once

#include "UI/CustomKeyboard.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
// Viewport that owns a CustomKeyboard and injects it into a JIVE layout via
// the ComponentFactory.
//
// The keyboard must exactly fill the visible height (horizontal scrolling
// only): JUCE's Viewport does not resize a viewed component by itself, so
// resized() syncs the keyboard height to the viewport's visible height —
// this was a root cause of the invisible/zero-height keyboard in the first
// JIVE migration attempt.
// ============================================================================
class KeyboardViewport final : public juce::Viewport {
public:
    explicit KeyboardViewport(juce::MidiKeyboardState& keyboardState)
        : keyboard(std::make_unique<CustomKeyboard>(keyboardState)) {
        setScrollBarsShown(false, true, false, true); // horizontal only
        setViewedComponent(keyboard.get(), false);
    }

    void resized() override {
        juce::Viewport::resized();

        const auto visibleWidth = getMaximumVisibleWidth();
        const auto visibleHeight = getMaximumVisibleHeight();
        if (visibleHeight > 0 || visibleWidth > 0) {
            keyboard->updateViewportBounds(visibleWidth, visibleHeight);
        }
    }

    [[nodiscard]] CustomKeyboard& getCustomKeyboard() noexcept {
        return *keyboard;
    }

private:
    std::unique_ptr<CustomKeyboard> keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardViewport)
};

#pragma once

#include "UI/jive/DesignTokens.h"
#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================================
// Small activity dot for the status bar, injected into a JIVE layout via the
// ComponentFactory. Dim when inactive, full colour when active.
// ============================================================================
class StatusBarMidiDot final : public juce::Component {
public:
    StatusBarMidiDot() {
        setInterceptsMouseClicks(false, false);
    }

    void setActive(bool active) {
        if (active != isActive) {
            isActive = active;
            repaint();
        }
    }

    [[nodiscard]] bool getIsActive() const noexcept {
        return isActive;
    }

    /// Trigger MIDI activity with a specified number of decay frames (~130ms at 30Hz by default).
    void triggerActivity(int frames = 4) {
        activityFramesRemaining = juce::jmax(activityFramesRemaining, frames);
        if (!isActive) {
            isActive = true;
            repaint();
        }
    }

    /// Step one frame of activity decay (called from 30Hz UI timer).
    void decayFrame() {
        if (activityFramesRemaining > 0) {
            --activityFramesRemaining;
            if (activityFramesRemaining == 0 && isActive) {
                isActive = false;
                repaint();
            }
        }
    }
    void paint(juce::Graphics& g) override {
        const auto colour = devpiano::jive::DesignTokens::get().playActive();
        g.setColour(isActive ? colour : colour.withAlpha(0.25f));
        g.fillEllipse(getLocalBounds().toFloat());
    }

private:
    bool isActive = false;
    int activityFramesRemaining = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBarMidiDot)
};

#pragma once

#include <JuceHeader.h>

#include "UI/jive/DesignTokens.h"

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

    void paint(juce::Graphics& g) override {
        const auto colour = devpiano::jive::DesignTokens::get().playActive();
        g.setColour(isActive ? colour : colour.withAlpha(0.25f));
        g.fillEllipse(getLocalBounds().toFloat());
    }

private:
    bool isActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBarMidiDot)
};

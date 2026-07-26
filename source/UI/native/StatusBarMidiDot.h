#pragma once

#include <JuceHeader.h>

#include "UI/jive/DesignTokens.h"

// ============================================================================
// Tiny native Component for the MIDI activity dot in the status bar.
//
// JIVE has no built-in circle primitive, so we use a small paint-only Component.
// Registered in ComponentFactory as "StatusBarMidiDot" and findable by ID "midi-dot".
// ============================================================================
class StatusBarMidiDot final : public juce::Component {
public:
    StatusBarMidiDot() {
        setOpaque(false);
    }

    void paint(juce::Graphics& g) override {
        const auto color = ::devpiano::jive::DesignTokens::get().playActive();
        g.setColour(active ? color : color.withAlpha(0.25f));
        g.fillEllipse(getLocalBounds().toFloat());
    }

    void setActive(bool a) {
        if (active != a) {
            active = a;
            repaint();
        }
    }

private:
    bool active = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBarMidiDot)
};

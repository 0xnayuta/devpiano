#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
// ============================================================================
// A self-contained Component that draws the ADSR envelope curve.
//
// Extracted from ControlsPanel::paint() / ControlsPanel::drawAdsrCurve()
// so that it can be injected into a JIVE layout via the ComponentFactory
// while the ControlsPanel itself is replaced by a ValueTree declaration.
// ============================================================================
class AdsrCurveComponent final : public juce::Component {
public:
    AdsrCurveComponent();

    void paint(juce::Graphics& g) override;

    void setParameters(float attack, float decay, float sustain, float release);

private:
    void drawAdsrCurve(juce::Graphics& g, float a, float d, float s, float r);

    float attack = 0.1f;
    float decay = 0.3f;
    float sustain = 0.7f;
    float release = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdsrCurveComponent)
};

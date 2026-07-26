#include "AdsrCurveComponent.h"

#include "UI/jive/DesignTokens.h"

AdsrCurveComponent::AdsrCurveComponent() {
    setOpaque(false);
}

void AdsrCurveComponent::paint(juce::Graphics& g) {
    drawAdsrCurve(g, attack, decay, sustain, release);
}

void AdsrCurveComponent::setParameters(float a, float d, float s, float r) {
    if (juce::approximatelyEqual(attack, a) && juce::approximatelyEqual(decay, d)
        && juce::approximatelyEqual(sustain, s) && juce::approximatelyEqual(release, r)) {
        return;
    }
    attack = a;
    decay = d;
    sustain = s;
    release = r;
    repaint();
}

void AdsrCurveComponent::drawAdsrCurve(juce::Graphics& g, float a, float d, float s, float r) {
    auto area = getLocalBounds();
    if (area.isEmpty()) {
        return;
    }

    // Scale each parameter to millisecond range for visual proportions
    const float attackMs = a * 1000.0f;
    const float decayMs = d * 1000.0f;
    const float releaseMs = r * 1000.0f;
    constexpr float sustainWeight = 200.0f; // fixed visual weight for sustain phase

    float totalX = attackMs + decayMs + sustainWeight + releaseMs;
    if (totalX < 1.0f) {
        return;
    }

    auto rect = area.toFloat().reduced(12.0f, 6.0f);
    const float w = rect.getWidth();
    const float h = rect.getHeight();
    const float x0 = rect.getX();
    const float y0 = rect.getY();

    float xA = w * (attackMs / totalX);
    float xD = w * (decayMs / totalX);
    float xS = w * (sustainWeight / totalX);
    float peakY = y0; // attack peak at top
    float susY = y0 + ((1.0f - s) * h); // sustain level
    float baseY = y0 + h; // zero level at bottom

    juce::Path path;
    path.startNewSubPath(x0, baseY);
    path.lineTo(x0 + xA, peakY); // attack
    path.lineTo(x0 + xA + xD, susY); // decay
    path.lineTo(x0 + xA + xD + xS, susY); // sustain
    path.lineTo(rect.getRight(), baseY); // release
    path.closeSubPath();

    const auto primary = devpiano::jive::DesignTokens::get().primary();
    g.setColour(primary.withAlpha(0.15f));
    g.fillPath(path);
    g.setColour(primary);
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

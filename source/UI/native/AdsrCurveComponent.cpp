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
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty() || bounds.getWidth() < 20.0f || bounds.getHeight() < 20.0f) {
        return;
    }

    // ── 1. Background box with subtle depth ──
    const auto cardBorder = juce::Colour(0xFF282D36);
    juce::ColourGradient bgGrad(juce::Colour(0xFF111316), bounds.getX(), bounds.getY(), juce::Colour(0xFF16181D),
                                bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(cardBorder);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Reserved area for the curve: leave room at bottom for phase labels
    constexpr float bottomLabelHeight = 16.0f;
    auto curveArea = bounds.reduced(10.0f, 6.0f);
    curveArea.removeFromBottom(bottomLabelHeight);

    const float w = curveArea.getWidth();
    const float h = curveArea.getHeight();
    const float x0 = curveArea.getX();
    const float y0 = curveArea.getY();
    const float baseY = y0 + h - 2.0f;

    // Proportional phase widths
    const float attackMs = juce::jlimit(0.001f, 2.0f, a) * 1000.0f;
    const float decayMs = juce::jlimit(0.001f, 2.0f, d) * 1000.0f;
    const float releaseMs = juce::jlimit(0.001f, 3.0f, r) * 1000.0f;
    constexpr float sustainWeight = 300.0f;

    const float totalWeight = attackMs + decayMs + sustainWeight + releaseMs;
    const float xA = w * (attackMs / totalWeight);
    const float xD = w * (decayMs / totalWeight);
    const float xS = w * (sustainWeight / totalWeight);
    const float xR = w - (xA + xD + xS);

    const float peakY = y0 + 6.0f;
    const float susY = peakY + ((1.0f - juce::jlimit(0.0f, 1.0f, s)) * (baseY - peakY));

    const juce::Point<float> p0 { x0, baseY };
    const juce::Point<float> pA { x0 + xA, peakY };
    const juce::Point<float> pD { x0 + xA + xD, susY };
    const juce::Point<float> pS { x0 + xA + xD + xS, susY };
    const juce::Point<float> pR { x0 + w, baseY };

    // ── 2. Subtle Grid Lines (Horizontal & Vertical) ──
    g.setColour(juce::Colour(0x14FFFFFF)); // faint dashed grid
    const float gridYs[] = { peakY, peakY + (baseY - peakY) * 0.5f, baseY };
    for (float gy : gridYs) {
        g.drawDashedLine(juce::Line<float>(x0, gy, x0 + w, gy), juce::Array<float> { 2.0f, 3.0f }.data(), 2, 0.6f);
    }
    const float gridXs[] = { pA.x, pD.x, pS.x };
    for (float gx : gridXs) {
        g.drawDashedLine(juce::Line<float>(gx, y0 + 2.0f, gx, baseY), juce::Array<float> { 2.0f, 3.0f }.data(), 2,
                         0.6f);
    }
    // Horizontal sustain guideline
    g.setColour(juce::Colour(0x2000C8F0));
    g.drawHorizontalLine(static_cast<int>(susY), x0, x0 + w);

    // Labels at bottom
    const auto labelFont = juce::Font(juce::FontOptions(10.0f));
    g.setFont(labelFont);
    g.setColour(juce::Colour(0xFF888E9B));

    const auto drawPhaseLabel = [&](const juce::String& text, float startX, float endX) {
        // Skip labels whose phase is narrower than the text itself: drawing
        // them centred still overflows into the neighbouring phases (e.g. a
        // 5 px attack segment under a 30 px "Attack" label).
        const auto phaseWidth = endX - startX;
        if (phaseWidth < juce::GlyphArrangement::getStringWidth(labelFont, text) + 4.0f) {
            return;
        }
        const auto labelBounds = juce::Rectangle<float>(startX, baseY + 2.0f, phaseWidth, bottomLabelHeight);
        g.drawText(text, labelBounds, juce::Justification::centred, false);
    };

    drawPhaseLabel(TRANS("Attack"), p0.x, pA.x);
    drawPhaseLabel(TRANS("Decay"), pA.x, pD.x);
    drawPhaseLabel(TRANS("Sustain"), pD.x, pS.x);
    drawPhaseLabel(TRANS("Release"), pS.x, pR.x);

    // ── 3. Smooth Bezier Curve Path ──
    juce::Path curvePath;
    curvePath.startNewSubPath(p0);
    // Smooth S-curve attack
    curvePath.cubicTo(p0.x + xA * 0.35f, p0.y, pA.x - xA * 0.15f, pA.y, pA.x, pA.y);
    // Exponential decay curve
    curvePath.cubicTo(pA.x + xD * 0.3f, pA.y + (pD.y - pA.y) * 0.75f, pD.x - xD * 0.15f, pD.y, pD.x, pD.y);
    // Sustain straight line
    curvePath.lineTo(pS);
    // Exponential release curve
    curvePath.cubicTo(pS.x + xR * 0.35f, pS.y + (pR.y - pS.y) * 0.75f, pR.x - xR * 0.15f, pR.y, pR.x, pR.y);

    // ── 4. Neon Gradient Fill ──
    juce::Path fillPath = curvePath;
    fillPath.lineTo(p0.x + w, baseY);
    fillPath.lineTo(p0.x, baseY);
    fillPath.closeSubPath();

    const auto primary = devpiano::jive::DesignTokens::get().primary();
    juce::ColourGradient fillGrad(primary.withAlpha(0.38f), 0.0f, peakY, primary.withAlpha(0.01f), 0.0f, baseY, false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    // ── 5. Glowing Stroke ──
    // Outer soft bloom
    g.setColour(primary.withAlpha(0.28f));
    g.strokePath(curvePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // Core sharp line
    g.setColour(primary);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ── 6. Glowing Control Points ──
    const juce::Point<float> controlPoints[] = { p0, pA, pD, pS, pR };
    for (const auto& pt : controlPoints) {
        // Outer halo
        g.setColour(primary.withAlpha(0.45f));
        g.fillEllipse(pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);
        // Inner white/cyan dot
        g.setColour(juce::Colours::white);
        g.fillEllipse(pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);
    }
}

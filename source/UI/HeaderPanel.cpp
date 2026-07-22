#include "HeaderPanel.h"
#include "DevPianoLookAndFeel.h"

// ── Gear icon path (simple cog / settings icon) ──
static std::unique_ptr<juce::Drawable> createGearIcon(juce::Colour colour) {
    juce::Path p;
    // Outer ring + four teeth as a single non-zero-winding composite
    p.addEllipse(-8, -8, 16, 16);
    for (int i = 0; i < 4; ++i) {
        auto angle = juce::MathConstants<float>::halfPi * static_cast<float>(i) - juce::MathConstants<float>::pi / 4.0f;
        auto cx = 9.0f * std::cos(angle);
        auto cy = 9.0f * std::sin(angle);
        p.addRectangle(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    }
    p.setUsingNonZeroWinding(true);
    auto drawable = std::make_unique<juce::DrawablePath>();
    drawable->setPath(p);
    drawable->setFill(colour);
    return drawable;
}

HeaderPanel::HeaderPanel() {
    titleLabel.setText("devpiano", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    settingsButton.setImages(createGearIcon(DevPianoLookAndFeel::kTextSecondary).get(),
                             createGearIcon(DevPianoLookAndFeel::kPrimary).get(), nullptr);
    settingsButton.setTooltip(TRANS("Settings"));
    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this] {
        if (onSettingsRequested)
            onSettingsRequested();
    };
}

void HeaderPanel::resized() {
    auto area = getLocalBounds();
    settingsButton.setBounds(area.removeFromRight(36));
    area.removeFromRight(8);
    titleLabel.setBounds(area);
}

void HeaderPanel::refreshTexts() {
    settingsButton.setTooltip(TRANS("Settings"));
}

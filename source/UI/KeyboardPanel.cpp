#include "KeyboardPanel.h"

#include "UI/CustomKeyboard.h"

KeyboardPanel::KeyboardPanel(juce::MidiKeyboardState& keyboardState)
    : customKeyboard(std::make_unique<CustomKeyboard>(keyboardState)) {
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(customKeyboard.get(), false);
    viewport->setScrollBarsShown(false, true, false, true); // horizontal only
    addAndMakeVisible(viewport.get());
}

KeyboardPanel::~KeyboardPanel() = default;

void KeyboardPanel::resized() {
    viewport->setBounds(getLocalBounds());

    // Sync CustomKeyboard height to Viewport so keys fill the available
    // vertical space without clipping.  The Viewport handles horizontal
    // scrolling; vertical should always be exact-fit.
    auto vpHeight = viewport->getMaximumVisibleHeight();
    if (vpHeight > 0 && customKeyboard->getHeight() != vpHeight)
        customKeyboard->setSize(customKeyboard->getWidth(), vpHeight);
}

void KeyboardPanel::setKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    customKeyboard->setKeyboardLayout(layout);
}

void KeyboardPanel::setViewPosition(int midiNote, int pixelOffset) {
    if (viewport == nullptr || customKeyboard == nullptr)
        return;

    if (pixelOffset >= 0) {
        // Restore from persisted pixel offset (0 is a valid position)
        viewport->setViewPosition(pixelOffset, 0);
    } else if (midiNote >= 0 && midiNote <= 127) {
        // Compute pixel offset: count white keys from 0 to midiNote
        int whiteCount = 0;
        for (int n = 0; n < midiNote; ++n)
            if (devpiano::ui::isWhiteKey(n))
                ++whiteCount;
        auto x = static_cast<int>(whiteCount * customKeyboard->getKeyboardSettings().keyWidth);
        viewport->setViewPosition(x, 0);
    }
}

int KeyboardPanel::getViewPositionX() const noexcept {
    if (viewport != nullptr)
        return viewport->getViewPositionX();
    return 0;
}

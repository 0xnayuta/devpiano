#pragma once

#include <JuceHeader.h>

namespace devpiano::ui {

/**
 * Applies the embedded application icon (icon-256.png) to the native peer of the
 * given window/component. On Linux X11, this populates _NET_WM_ICON and WM_HINTS;
 * on Windows, this updates WM_SETICON.
 */
inline void applyAppWindowIcon(juce::Component& window) {
    if (auto* peer = window.getPeer()) {
        const auto icon = juce::ImageCache::getFromMemory(BinaryData::icon256_png, BinaryData::icon256_pngSize);
        if (icon.isValid()) {
            peer->setIcon(icon);
        }
    }
}

} // namespace devpiano::ui

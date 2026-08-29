#include "UI/CustomKeyboard.h"
#include "DevPianoLookAndFeel.h"
#include "UI/jive/DesignTokens.h"

#include <cmath>

// ============================================================================
// Helpers
// ============================================================================

namespace {

// Colour helpers (classic mode: warm orange)
juce::Colour classicColourTop(float fade) {
    auto s = 0.3f + 0.7f * fade;
    return juce::Colour::fromHSV(27.0f / 360.0f, s, 1.0f, fade);
}

// Channel colour mode: 16 predefined hues
constexpr float channelHues[16] = { 30, 10, 350, 330, 310, 290, 270, 210, 190, 170, 150, 130, 110, 90, 70, 50 };

// Velocity colour mode: green (h=64) → red (h=0)
float velocityHue(float velocity) {
    return (1.0f - juce::jlimit(0.0f, 1.0f, velocity)) * 64.0f;
}

// Fade threshold below which we consider the key fully settled
constexpr float fadeEpsilon = 0.001f;

// Animation interval (~30 fps)
constexpr int timerIntervalMs = 33;

// Default size for the component
constexpr int defaultHeight = 128;

// Map each black-key semitone to the MIDI note of the white key immediately
// to its left.  Indexed by (semitone % 12).  -1 = not a black key.
//  C# (1) → C (0),  D# (3) → D (2),  F# (6) → F (5),
//  G# (8) → G (7),  A# (10) → A (9)
constexpr int blackKeyLeftWhiteNote[12] = { -1, 0, -1, 2, -1, -1, 5, -1, 7, -1, 9, -1 };

} // namespace

// ============================================================================
// Construction
// ============================================================================

CustomKeyboard::CustomKeyboard(juce::MidiKeyboardState& state)
    : keyboardState(state) {
    setOpaque(false);
    setSize(800, defaultHeight); // reasonable default, resized by parent
    setAvailableRange(21, 108); // standard 88-key grand piano range (A0 to C8)
    keyboardState.addListener(this);
    startTimer(timerIntervalMs);
}

CustomKeyboard::~CustomKeyboard() {
    keyboardState.removeListener(this);
}

void CustomKeyboard::setKeyboardSettings(const devpiano::ui::KeyboardSettings& s) {
    settings = s;
    recalculateKeyBounds();
    repaint();
}

const devpiano::ui::KeyboardSettings& CustomKeyboard::getKeyboardSettings() const noexcept {
    return settings;
}
void CustomKeyboard::setAvailableRange(int low, int high) {
    rangeLow = juce::jlimit(0, 127, low);
    rangeHigh = juce::jlimit(0, 127, high);
    recalculateKeyBounds();
    repaint();
}

void CustomKeyboard::setKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    // Reset per-key data
    for (auto& ch : perKeyChannel) {
        ch.store(0);
    }
    perKeyVelocity.fill(1.0f);

    // Reverse-map: for each piano key (MIDI note), find the computer-key binding
    // whose action.midiNote matches.
    for (const auto& binding : layout.bindings) {
        if (binding.action.type != devpiano::core::KeyActionType::note) {
            continue;
        }

        auto note = binding.action.midiNote;
        if (note < 0 || note > 127) {
            continue;
        }

        auto idx = static_cast<std::size_t>(note);
        perKeyChannel[idx].store(static_cast<uint8_t>(binding.action.midiChannel - 1));
        perKeyVelocity[idx] = binding.action.velocity;
    }

    // Ensure keys are populated before labelling them.
    // First call from syncUiFromSettings() happens before setKeyboardSettings()
    // triggers recalculateKeyBounds(), so keys is empty without this guard.
    if (keys.empty()) {
        recalculateKeyBounds();
    }

    // Apply labels to displayed keys
    for (auto& k : keys) {
        k.keyLabel = {};

        for (const auto& binding : layout.bindings) {
            if (binding.action.type != devpiano::core::KeyActionType::note) {
                continue;
            }

            if (binding.action.midiNote == k.midiNote) {
                k.keyLabel = binding.displayText;
                break;
            }
        }
    }
    repaint();
}

// ============================================================================
// Geometry
// ============================================================================

void CustomKeyboard::recalculateKeyBounds() {
    keys.clear();

    auto totalHeight = static_cast<float>(lastVisibleHeight > 0 ? lastVisibleHeight : getHeight());
    if (totalHeight < 1.0f) {
        totalHeight = static_cast<float>(defaultHeight);
    }

    // Count all white keys in the full range (not just visible window)
    int whiteKeyCount = 0;
    for (int n = rangeLow; n <= rangeHigh; ++n) {
        if (devpiano::ui::isWhiteKey(n)) {
            ++whiteKeyCount;
        }
    }

    if (whiteKeyCount == 0) {
        return;
    }

    auto whiteKeyWidth = settings.keyWidth;
    auto actualWhiteWidth = whiteKeyWidth;
    auto blackKeyWidth = actualWhiteWidth * 0.6f;
    auto whiteKeyHeight = totalHeight;
    auto blackKeyHeight = totalHeight * 0.6f;

    auto totalContentWidth = actualWhiteWidth * static_cast<float>(whiteKeyCount);
    auto availableWidth = static_cast<float>(lastVisibleWidth > 0 ? lastVisibleWidth : getWidth());

    // 当窗口宽度大于琴键内容总宽时，水平居中对齐
    keybedOffsetX = (availableWidth > totalContentWidth) ? (availableWidth - totalContentWidth) * 0.5f : 0.0f;
    auto targetComponentWidth = std::max(totalContentWidth, availableWidth);

    // First pass: assign white-key positions.
    int whiteIdx = 0;
    for (int n = rangeLow; n <= rangeHigh; ++n) {
        if (!devpiano::ui::isWhiteKey(n)) {
            continue;
        }

        devpiano::ui::KeyRenderState k;
        k.midiNote = n;
        k.isWhite = true;
        k.fade = 0.0f;
        k.bounds = { keybedOffsetX + actualWhiteWidth * static_cast<float>(whiteIdx), 0.0f, actualWhiteWidth,
                     whiteKeyHeight };

        keys.push_back(k);
        ++whiteIdx;
    }

    // Second pass: insert black-key bounds.
    for (int n = rangeLow; n <= rangeHigh; ++n) {
        if (devpiano::ui::isWhiteKey(n)) {
            continue;
        }
        if (n <= 0 || n > 127) {
            continue;
        }

        devpiano::ui::KeyRenderState k;
        k.midiNote = n;
        k.isWhite = false;
        k.fade = 0.0f;

        auto semi = n % 12;
        auto leftWhiteNote = blackKeyLeftWhiteNote[semi];
        if (leftWhiteNote < 0) {
            continue;
        }

        auto octaveBase = n - semi;
        auto leftWhiteMidi = octaveBase + leftWhiteNote;

        int whiteVecIdx = 0;
        for (int m = rangeLow; m <= leftWhiteMidi; ++m) {
            if (devpiano::ui::isWhiteKey(m)) {
                ++whiteVecIdx;
            }
        }

        auto idx = static_cast<std::size_t>(whiteVecIdx - 1);
        if (whiteVecIdx > 0 && idx < keys.size()) {
            auto leftX = keys[idx].bounds.getX();
            auto leftWidth = keys[idx].bounds.getWidth();

            auto centreX = leftX + leftWidth;
            k.bounds = { centreX - blackKeyWidth * 0.5f, 0.0f, blackKeyWidth, blackKeyHeight };
            keys.push_back(k);
        }
    }

    std::ranges::sort(keys, [](const auto& a, const auto& b) { return a.midiNote < b.midiNote; });

    // Expand component to full key width so parent Viewport can scroll;
    // guard against resized() → recalculateKeyBounds() recursion.
    resizing = true;
    setSize(static_cast<int>(targetComponentWidth), static_cast<int>(totalHeight));
    resizing = false;
}

int CustomKeyboard::findNoteAt(juce::Point<int> position) const {
    auto pos = position.toFloat();

    // Check black keys first (they render above white keys).
    for (const auto& k : keys) {
        if (!k.isWhite && k.bounds.contains(pos)) {
            return k.midiNote;
        }
    }
    // Then white keys.
    for (const auto& k : keys) {
        if (k.isWhite && k.bounds.contains(pos)) {
            return k.midiNote;
        }
    }
    return -1;
}

// ============================================================================
// Painting
// ============================================================================

void CustomKeyboard::paint(juce::Graphics& g) {
    // ── 1. Crimson felt strip at top edge of keybed (#8A1515) ──
    g.setColour(juce::Colour(0xFF8A1515));
    g.fillRect(0, 0, getWidth(), 2);

    paintWhiteKeys(g);
    paintBlackKeys(g);
    paintKeyLabels(g);
}

void CustomKeyboard::paintWhiteKeys(juce::Graphics& g) {
    const auto clip = g.getClipBounds();
    for (const auto& k : keys) {
        if (!k.isWhite) {
            continue;
        }
        if (!clip.intersects(k.bounds.toNearestInt().expanded(2))) {
            continue;
        }
        const auto& b = k.bounds;
        juce::Path keyPath;
        keyPath.addRoundedRectangle(b.getX(), b.getY(), b.getWidth(), b.getHeight(), 2.5f, 2.5f, false, false, true,
                                    true);

        // Base fill: custom colour or realistic ivory-white gradient
        auto customColour = settings.customKeyColours[static_cast<std::size_t>(k.midiNote)];
        if (!customColour.isTransparent()) {
            g.setColour(customColour);
            g.fillPath(keyPath);
        } else {
            juce::ColourGradient whiteGrad(juce::Colour(0xFFFAFBFC), b.getX(), b.getY(), juce::Colour(0xFFDDE1E8),
                                           b.getX(), b.getBottom(), false);
            g.setGradientFill(whiteGrad);
            g.fillPath(keyPath);
        }

        // Neon Cyan Glow bloom on key press
        if (k.fade > fadeEpsilon) {
            float vel = perKeyVelocity[static_cast<std::size_t>(k.midiNote)].get();
            if (vel <= 0.001f) {
                vel = 0.8f;
            }
            const auto baseGlow = (settings.colourMode == devpiano::ui::KeyColourMode::classic)
                ? devpiano::jive::DesignTokens::get().primary()
                : k.colour1;
            auto glowColour = baseGlow.interpolatedWith(juce::Colours::white, vel * 0.45f).withAlpha(k.fade * 0.85f);

            // Soft bloom from bottom upward
            juce::ColourGradient glowGrad(glowColour, b.getCentreX(), b.getBottom() - 10.0f,
                                          glowColour.withAlpha(0.05f), b.getCentreX(), b.getY() + 10.0f, false);
            g.setGradientFill(glowGrad);
            g.fillPath(keyPath);

            // Pressed key top sink shadow (simulating physical mechanical dip)
            if (k.fade > 0.3f) {
                const float shadowH = juce::jmin(14.0f, b.getHeight() * 0.18f);
                juce::ColourGradient sinkGrad(juce::Colours::black.withAlpha(0.32f * k.fade), b.getX(), b.getY(),
                                              juce::Colours::black.withAlpha(0.0f), b.getX(), b.getY() + shadowH,
                                              false);
                g.setGradientFill(sinkGrad);
                g.fillRect(juce::Rectangle<float>(b.getX() + 1.0f, b.getY(), b.getWidth() - 2.0f, shadowH));
            }
        }

        // Subtle key border outline
        g.setColour(juce::Colour(0xFFB4BAC6));
        g.strokePath(keyPath, juce::PathStrokeType(0.8f));
    }
}

void CustomKeyboard::paintBlackKeys(juce::Graphics& g) {
    const auto clip = g.getClipBounds();
    for (const auto& k : keys) {
        if (k.isWhite) {
            continue;
        }
        if (!clip.intersects(k.bounds.toNearestInt().expanded(4))) {
            continue;
        }
        const auto& b = k.bounds;

        // Gradual shrink proportional to fade (0→2px)
        auto keyRect = b.withHeight(b.getHeight() - k.fade * 2.0f);

        // Shadow: two soft layers beneath the key (sides + bottom)
        for (int layer = 0; layer < 2; ++layer) {
            float expand = 1.0f + static_cast<float>(layer) * 0.5f;
            float alpha = 0.12f - static_cast<float>(layer) * 0.05f;
            juce::Path shadowPath;
            shadowPath.addRoundedRectangle(keyRect.getX() - expand, keyRect.getY() + 1.5f + static_cast<float>(layer),
                                           keyRect.getWidth() + expand * 2.0f, keyRect.getHeight() + expand,
                                           2.0f + expand, 2.0f + expand, false, false, true, true);
            g.setColour(juce::Colours::black.withAlpha(alpha));
            g.fillPath(shadowPath);
        }

        // Key path with bottom-only rounded corners
        juce::Path keyPath;
        keyPath.addRoundedRectangle(keyRect.getX(), keyRect.getY(), keyRect.getWidth(), keyRect.getHeight(), 2.0f, 2.0f,
                                    false, false, true, true);

        // Base fill: custom colour or obsidian matte gradient
        auto customColour = settings.customKeyColours[static_cast<std::size_t>(k.midiNote)];
        if (!customColour.isTransparent()) {
            g.setColour(customColour);
            g.fillPath(keyPath);
        } else {
            juce::ColourGradient blackGrad(juce::Colour(0xFF383C45), keyRect.getX(), keyRect.getY(),
                                           juce::Colour(0xFF131417), keyRect.getX(), keyRect.getBottom(), false);
            g.setGradientFill(blackGrad);
            g.fillPath(keyPath);
        }

        // Glow on key press
        if (k.fade > fadeEpsilon) {
            float vel = perKeyVelocity[static_cast<std::size_t>(k.midiNote)].get();
            if (vel <= 0.001f) {
                vel = 0.8f;
            }
            const auto baseGlow = (settings.colourMode == devpiano::ui::KeyColourMode::classic)
                ? devpiano::jive::DesignTokens::get().primary()
                : k.colour1;
            auto glowColour = baseGlow.interpolatedWith(juce::Colours::white, vel * 0.4f).withAlpha(k.fade * 0.75f);

            juce::ColourGradient fadeGrad(glowColour, keyRect.getCentreX(), keyRect.getBottom() - 6.0f,
                                          glowColour.withAlpha(0.0f), keyRect.getCentreX(), keyRect.getY(), false);
            g.setGradientFill(fadeGrad);
            g.fillPath(keyPath);
        }

        // Border outline
        g.setColour(juce::Colour(0xFF2B2F38));
        g.strokePath(keyPath, juce::PathStrokeType(0.8f));
        // Top edge bevel highlight
        g.setColour(juce::Colour(0xFF606674));
        g.drawHorizontalLine(static_cast<int>(keyRect.getY()), keyRect.getX() + 1.0f, keyRect.getRight() - 1.0f);
    }
}

void CustomKeyboard::paintKeyLabels(juce::Graphics& g) {
    const auto clip = g.getClipBounds();
    for (const auto& k : keys) {
        if (!clip.intersects(k.bounds.toNearestInt().expanded(2))) {
            continue;
        }
        auto& customLabel = settings.customKeyLabels[static_cast<std::size_t>(k.midiNote)];

        if (k.isWhite) {
            // ── White key labels: bottom of key ──
            auto fontH = static_cast<float>(juce::jlimit(9, 13, static_cast<int>(settings.keyWidth * 0.42f)));
            g.setFont(juce::FontOptions(fontH));

            // Adaptive text color for high contrast against cyan glow
            const auto labelColor = (k.fade > 0.45f) ? juce::Colour(0xFF0C2B38) : juce::Colour(0xFF606674);
            g.setColour(labelColor);

            if (customLabel.isNotEmpty()) {
                auto area = k.bounds.withTrimmedTop(k.bounds.getHeight() * 0.65f).reduced(1, 3);
                g.drawText(customLabel, area, juce::Justification::centred, false);
            } else if (k.keyLabel.isNotEmpty()) {
                auto area = k.bounds.withTrimmedTop(k.bounds.getHeight() * 0.65f).reduced(1, 3);
                g.drawText(k.keyLabel, area, juce::Justification::centred, false);
            } else if (k.midiNote >= 0) {
                auto name = devpiano::ui::getNoteDisplayName(k.midiNote, settings.noteDisplay, settings.keySignature);

                auto plusPos = name.indexOfChar('+');
                auto minusPos = name.indexOfChar('-');
                auto splitPos = (plusPos >= 0) ? plusPos : minusPos;

                if (splitPos > 0) {
                    auto topLine = name.substring(0, splitPos);
                    auto bottomLine = name.substring(splitPos);
                    auto area = k.bounds.withTrimmedTop(k.bounds.getHeight() * 0.65f).reduced(1, 2);
                    auto lineH = fontH * 1.15f;
                    auto topArea = area.withHeight(lineH).translated(0, (area.getHeight() - lineH * 2.0f) * 0.5f);
                    auto bottomArea = topArea.translated(0, lineH);
                    g.drawText(topLine, topArea, juce::Justification::centred, false);
                    g.drawText(bottomLine, bottomArea, juce::Justification::centred, false);
                } else {
                    auto area = k.bounds.withTrimmedTop(k.bounds.getHeight() * 0.65f).reduced(1, 3);
                    g.drawText(name, area, juce::Justification::centred, false);
                }
            }
        } else {
            // ── Black key labels: upper portion, binding label only ──
            if (customLabel.isEmpty() && k.keyLabel.isEmpty()) {
                continue;
            }

            auto bkFontH = static_cast<float>(juce::jmin(10, static_cast<int>(settings.keyWidth * 0.38f)));
            g.setFont(juce::FontOptions(bkFontH));
            g.setColour(juce::Colour(0xFFD4D8E0));
            auto label = customLabel.isNotEmpty() ? customLabel : k.keyLabel;
            auto area = k.bounds.withTrimmedBottom(k.bounds.getHeight() * 0.4f).reduced(1, 2);
            g.drawText(label, area, juce::Justification::centred, false);
        }
    }
}

// ============================================================================
// Mouse interaction
// ============================================================================

void CustomKeyboard::mouseDown(const juce::MouseEvent& e) {
    auto note = findNoteAt(e.getPosition());
    if (note < 0) {
        return;
    }

    // 右键单击：打开按键绑定编辑器，不触发发音。
    // 左键双击容易与快速连按同一键冲突（第一击会先发一个音，连按会误开
    // 编辑器），因此编辑器改由右键独占触发，左键只负责弹琴。
    if (e.mods.isRightButtonDown()) {
        if (onBindingEditRequested) {
            onBindingEditRequested(note);
        }
        return;
    }

    // 仅左键参与弹琴；中键等其它按钮不发声、不改变状态。
    if (!e.mods.isLeftButtonDown()) {
        return;
    }

    lastMouseDownNote = note;

    // Immediately visualise the press
    for (auto& k : keys) {
        if (k.midiNote == note) {
            k.fade = 1.0f;
            k.colour1 = classicColourTop(1.0f);
            repaintKey(k);
            break;
        }
    }
    if (onNoteOn) {
        auto ch = (note >= 0 && note < 128) ? static_cast<int>(perKeyChannel[static_cast<std::size_t>(note)]) : 0;
        onNoteOn(note, ch);
    }
    ensureTimerRunning();
}

void CustomKeyboard::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    releaseHeldMouseNote();
}

void CustomKeyboard::releaseHeldMouseNote() {
    if (lastMouseDownNote < 0) {
        return;
    }

    auto note = lastMouseDownNote;
    lastMouseDownNote = -1;

    // Let fade decay naturally; timer will handle it.
    ensureTimerRunning();

    if (onNoteOff) {
        auto ch = (note >= 0 && note < 128) ? static_cast<int>(perKeyChannel[static_cast<std::size_t>(note)]) : 0;
        onNoteOff(note, ch);
    }
}

void CustomKeyboard::mouseDrag(const juce::MouseEvent& e) {
    if (lastMouseDownNote < 0) {
        return;
    }

    auto note = findNoteAt(e.getPosition());
    if (note == lastMouseDownNote || note < 0) {
        return;
    }

    // Release the old note
    if (onNoteOff) {
        auto oldCh = static_cast<int>(perKeyChannel[static_cast<std::size_t>(lastMouseDownNote)]);
        onNoteOff(lastMouseDownNote, oldCh);
    }

    // Press the new note
    lastMouseDownNote = note;
    for (auto& k : keys) {
        if (k.midiNote == note) {
            k.fade = 1.0f;
            k.colour1 = classicColourTop(1.0f);
            repaintKey(k);
            break;
        }
    }
    if (onNoteOn) {
        auto ch = (note >= 0 && note < 128) ? static_cast<int>(perKeyChannel[static_cast<std::size_t>(note)]) : 0;
        onNoteOn(note, ch);
    }
    ensureTimerRunning();
}

// ============================================================================
// Fade animation
// ============================================================================

void CustomKeyboard::repaintKey(const devpiano::ui::KeyRenderState& k) {
    const auto extra = k.isWhite ? 2 : 4;
    repaint(k.bounds.toNearestInt().expanded(extra));
}

void CustomKeyboard::timerCallback() {
    bool anyActive = false;

    for (auto& k : keys) {
        auto before = k.fade;

        // Check if the note is held on any MIDI channel (1-16).
        bool noteHeld = false;
        for (int ch = 1; ch <= 16; ++ch) {
            if (keyboardState.isNoteOn(ch, k.midiNote)) {
                noteHeld = true;
                break;
            }
        }

        if (noteHeld) {
            // Key is currently held down → full brightness
            k.fade = 1.0f;
        } else {
            // Key released → exponential decay toward previewAlpha
            k.fade = settings.previewAlpha + settings.fadeSpeed * (k.fade - settings.previewAlpha);
        }

        // Stop tracking when fade has converged to its target.
        auto target = noteHeld ? 1.0f : settings.previewAlpha;
        if (noteHeld || std::abs(k.fade - target) > fadeEpsilon) {
            anyActive = true;
        }
        const bool changed = std::abs(k.fade - before) > fadeEpsilon;

        // Recompute colour: custom colour takes priority, else colourMode
        if (k.fade > fadeEpsilon) {
            auto customColour = settings.customKeyColours[static_cast<std::size_t>(k.midiNote)];
            if (!customColour.isTransparent()) {
                k.colour1 = customColour.withAlpha(k.fade);
            } else {
                switch (settings.colourMode) {
                case devpiano::ui::KeyColourMode::channel:
                    if (k.midiNote >= 0 && k.midiNote < 128) {
                        auto idx = static_cast<std::size_t>(k.midiNote);
                        auto ch = perKeyChannel[idx].load() % 16;
                        k.colour1 = juce::Colour::fromHSV(channelHues[ch] / 360.0f, 0.7f, 1.0f, k.fade);
                    }
                    break;

                case devpiano::ui::KeyColourMode::velocity:
                    if (k.midiNote >= 0 && k.midiNote < 128) {
                        auto idx = static_cast<std::size_t>(k.midiNote);
                        auto h = velocityHue(perKeyVelocity[idx].get());
                        k.colour1 = juce::Colour::fromHSV(h / 360.0f, 0.7f, 1.0f, k.fade);
                    }
                    break;

                case devpiano::ui::KeyColourMode::classic:
                default:
                    k.colour1 = classicColourTop(k.fade);
                    break;
                }
            }
        }

        if (changed) {
            repaintKey(k);
        }
    }

    if (!anyActive) {
        stopTimer();
    }
}
void CustomKeyboard::ensureTimerRunning() {
    if (!isTimerRunning()) {
        startTimer(timerIntervalMs);
    }
}
void CustomKeyboard::notifyNoteActivity() {
    ensureTimerRunning();
}

void CustomKeyboard::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
    if (midiNoteNumber >= 0 && midiNoteNumber < 128) {
        if (velocity > 0.0f) {
            perKeyVelocity[static_cast<std::size_t>(midiNoteNumber)] = velocity;
        }
        if (midiChannel >= 1 && midiChannel <= 16) {
            perKeyChannel[static_cast<std::size_t>(midiNoteNumber)].store(static_cast<uint8_t>(midiChannel - 1));
        }
    }
    ensureTimerRunning();
}

void CustomKeyboard::handleNoteOff(juce::MidiKeyboardState*, int, int, float) {
    ensureTimerRunning();
}

// ============================================================================
// Resize
// ============================================================================

void CustomKeyboard::resized() {
    if (!resizing) {
        recalculateKeyBounds();
    }
}

void CustomKeyboard::updateViewportBounds(int visibleWidth, int visibleHeight) {
    lastVisibleWidth = visibleWidth;
    if (visibleHeight > 0) {
        lastVisibleHeight = visibleHeight;
    }
    if ((lastVisibleHeight > 0 && getHeight() != lastVisibleHeight)
        || (lastVisibleWidth > 0 && getWidth() != lastVisibleWidth)) {
        recalculateKeyBounds();
        repaint();
    }
}

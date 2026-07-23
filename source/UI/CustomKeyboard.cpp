#include "UI/CustomKeyboard.h"
#include "DevPianoLookAndFeel.h"

#include <cmath>

// ============================================================================
// Helpers
// ============================================================================

namespace {

// Colour helpers (classic mode: warm orange)
static juce::Colour classicColourTop(float fade) {
    auto s = 0.3f + 0.7f * fade;
    return juce::Colour::fromHSV(27.0f / 360.0f, s, 1.0f, fade);
}

// Channel colour mode: 16 predefined hues
constexpr float channelHues[16] = { 30, 10, 350, 330, 310, 290, 270, 210, 190, 170, 150, 130, 110, 90, 70, 50 };

// Velocity colour mode: green (h=64) → red (h=0)
static float velocityHue(float velocity) {
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
    setAvailableRange(0, 127); // full MIDI range (unified full-range keyboard)
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
void CustomKeyboard::setLowestVisibleNote(int note) {
    note = juce::jlimit(rangeLow, rangeHigh, note);

    // Use parent (Viewport) visible width for clamping, not content width
    auto visibleW = 800.0f;
    if (auto* parent = getParentComponent())
        visibleW = static_cast<float>(parent->getWidth());
    if (visibleW < 1.0f)
        visibleW = 800.0f;
    auto kw = settings.keyWidth;
    auto maxVisible = static_cast<int>(visibleW / kw);
    if (maxVisible < 1)
        maxVisible = 1;

    // Find the latest start note so the rightmost key is still within rangeHigh
    int whiteCount = 0;
    int upperBound = rangeHigh;
    for (int n = rangeHigh; n >= rangeLow; --n) {
        if (devpiano::ui::isWhiteKey(n)) {
            ++whiteCount;
            if (whiteCount == maxVisible) {
                upperBound = n;
                break;
            }
        }
    }
    note = juce::jlimit(rangeLow, upperBound, note);

    lowestVisibleNote = note;
    repaint();
}

void CustomKeyboard::setAvailableRange(int low, int high) {
    rangeLow = juce::jlimit(0, 127, low);
    rangeHigh = juce::jlimit(0, 127, high);
    recalculateKeyBounds();
    repaint();
}

void CustomKeyboard::setKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    // Reset per-key data
    perKeyChannel.fill(0);
    perKeyVelocity.fill(1.0f);

    // Reverse-map: for each piano key (MIDI note), find the computer-key binding
    // whose action.midiNote matches.
    for (const auto& binding : layout.bindings) {
        if (binding.action.type != devpiano::core::KeyActionType::note)
            continue;

        auto note = binding.action.midiNote;
        if (note < 0 || note > 127)
            continue;

        auto idx = static_cast<std::size_t>(note);
        perKeyChannel[idx] = static_cast<uint8_t>(binding.action.midiChannel - 1);
        perKeyVelocity[idx] = binding.action.velocity;
    }

    // Ensure keys are populated before labelling them.
    // First call from syncUiFromSettings() happens before setKeyboardSettings()
    // triggers recalculateKeyBounds(), so keys is empty without this guard.
    if (keys.empty())
        recalculateKeyBounds();

    // Apply labels to displayed keys
    for (auto& k : keys) {
        k.keyLabel = {};

        for (const auto& binding : layout.bindings) {
            if (binding.action.type != devpiano::core::KeyActionType::note)
                continue;

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

    auto totalHeight = static_cast<float>(getHeight());
    if (totalHeight < 1.0f)
        totalHeight = static_cast<float>(defaultHeight);

    // Count all white keys in the full range (not just visible window)
    int whiteKeyCount = 0;
    for (int n = rangeLow; n <= rangeHigh; ++n)
        if (devpiano::ui::isWhiteKey(n))
            ++whiteKeyCount;

    if (whiteKeyCount == 0)
        return;

    auto whiteKeyWidth = settings.keyWidth;
    auto actualWhiteWidth = whiteKeyWidth;
    auto blackKeyWidth = actualWhiteWidth * 0.6f;
    auto whiteKeyHeight = totalHeight;
    auto blackKeyHeight = totalHeight * 0.6f;

    auto totalContentWidth = actualWhiteWidth * static_cast<float>(whiteKeyCount);

    // First pass: assign white-key positions.
    int whiteIdx = 0;
    for (int n = rangeLow; n <= rangeHigh; ++n) {
        if (!devpiano::ui::isWhiteKey(n))
            continue;

        devpiano::ui::KeyRenderState k;
        k.midiNote = n;
        k.isWhite = true;
        k.fade = 0.0f;
        k.bounds = { actualWhiteWidth * static_cast<float>(whiteIdx), 0.0f, actualWhiteWidth, whiteKeyHeight };

        keys.push_back(k);
        ++whiteIdx;
    }

    // Second pass: insert black-key bounds.
    for (int n = rangeLow; n <= rangeHigh; ++n) {
        if (devpiano::ui::isWhiteKey(n))
            continue;
        if (n <= 0 || n > 127)
            continue;

        devpiano::ui::KeyRenderState k;
        k.midiNote = n;
        k.isWhite = false;
        k.fade = 0.0f;

        auto semi = n % 12;
        auto leftWhiteNote = blackKeyLeftWhiteNote[semi];
        if (leftWhiteNote < 0)
            continue;

        auto octaveBase = n - semi;
        auto leftWhiteMidi = octaveBase + leftWhiteNote;

        int whiteVecIdx = 0;
        for (int m = rangeLow; m <= leftWhiteMidi; ++m)
            if (devpiano::ui::isWhiteKey(m))
                ++whiteVecIdx;

        auto idx = static_cast<std::size_t>(whiteVecIdx - 1);
        if (whiteVecIdx > 0 && idx < keys.size()) {
            auto leftX = keys[idx].bounds.getX();
            auto leftWidth = keys[idx].bounds.getWidth();

            auto centreX = leftX + leftWidth;
            k.bounds = { centreX - blackKeyWidth * 0.5f, 0.0f, blackKeyWidth, blackKeyHeight };
            keys.push_back(k);
        }
    }

    std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.midiNote < b.midiNote; });

    // Expand component to full key width so parent Viewport can scroll;
    // guard against resized() → recalculateKeyBounds() recursion.
    resizing = true;
    setSize(static_cast<int>(totalContentWidth), static_cast<int>(totalHeight));
    resizing = false;
}

int CustomKeyboard::findNoteAt(juce::Point<int> position) const {
    auto pos = position.toFloat();

    // Check black keys first (they render above white keys).
    for (const auto& k : keys) {
        if (!k.isWhite && k.bounds.contains(pos))
            return k.midiNote;
    }
    // Then white keys.
    for (const auto& k : keys) {
        if (k.isWhite && k.bounds.contains(pos))
            return k.midiNote;
    }
    return -1;
}

// ============================================================================
// Painting
// ============================================================================

void CustomKeyboard::paint(juce::Graphics& g) {
    // ── 1. Crimson felt strip at top edge of keybed (#9e1b1b) ──
    g.setColour(juce::Colour(0xff9e1b1b));
    g.fillRect(0, 0, getWidth(), 2);

    paintWhiteKeys(g);
    paintBlackKeys(g);
    paintKeyLabels(g);
}

void CustomKeyboard::paintWhiteKeys(juce::Graphics& g) {
    for (const auto& k : keys) {
        if (!k.isWhite)
            continue;

        auto& b = k.bounds;
        juce::Path keyPath;
        keyPath.addRoundedRectangle(b.getX(), b.getY(), b.getWidth(), b.getHeight(), 2.0f, 2.0f, false, false, true,
                                    true);

        // Base fill: custom colour or vertical gradient
        auto customColour = settings.customKeyColours[static_cast<std::size_t>(k.midiNote)];
        if (!customColour.isTransparent()) {
            g.setColour(customColour);
            g.fillPath(keyPath);
        } else {
            juce::ColourGradient whiteGrad(juce::Colour(0xfff0f0f0), b.getX(), b.getY(), juce::Colour(0xffd8d8d8),
                                           b.getX(), b.getBottom(), false);
            g.setGradientFill(whiteGrad);
            g.fillPath(keyPath);
        }

        // Fade overlay & Velocity dynamic glow (ice blue -> bright white based on velocity)
        if (k.fade > fadeEpsilon) {
            float vel = perKeyVelocity[static_cast<std::size_t>(k.midiNote)].get();
            if (vel <= 0.001f)
                vel = 0.8f;
            auto baseGlow = (settings.colourMode == devpiano::ui::KeyColourMode::classic)
                ? DevPianoLookAndFeel::kPrimary
                : k.colour1;
            auto glowColour = baseGlow.interpolatedWith(juce::Colours::white, vel * 0.7f).withAlpha(k.fade);

            juce::ColourGradient fadeGrad(glowColour, b.getCentreX(), b.getCentreY(), glowColour.withAlpha(0.0f),
                                          b.getCentreX(), b.getY(), false);
            g.setGradientFill(fadeGrad);
            g.fillPath(keyPath);

            // Pressed key top inner shadow simulating 1.5px sink effect
            if (k.fade > 0.5f) {
                auto shadowH = juce::jmin(12.0f, b.getHeight() * 0.15f);
                juce::ColourGradient sinkGrad(juce::Colours::black.withAlpha(0.35f * k.fade), b.getX(), b.getY(),
                                              juce::Colours::black.withAlpha(0.0f), b.getX(), b.getY() + shadowH,
                                              false);
                g.setGradientFill(sinkGrad);
                g.fillRect(juce::Rectangle<float>(b.getX() + 1.0f, b.getY(), b.getWidth() - 2.0f, shadowH));
            }
        }

        // Border
        g.setColour(juce::Colour(0xffaaaaaa));
        g.strokePath(keyPath, juce::PathStrokeType(1.0f));
    }
}
void CustomKeyboard::paintBlackKeys(juce::Graphics& g) {
    for (const auto& k : keys) {
        if (k.isWhite)
            continue;

        auto& b = k.bounds;

        // Gradual shrink proportional to fade (0→2px)
        auto keyRect = b.withHeight(b.getHeight() - k.fade * 2.0f);

        // Shadow: two layers beneath the key (sides + bottom, confined within key width)
        for (int layer = 0; layer < 2; ++layer) {
            float expand = 1.0f + static_cast<float>(layer) * 0.5f;
            float alpha = 0.08f - static_cast<float>(layer) * 0.03f;
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

        // Base fill: custom colour or vertical gradient
        auto customColour = settings.customKeyColours[static_cast<std::size_t>(k.midiNote)];
        if (!customColour.isTransparent()) {
            g.setColour(customColour);
            g.fillPath(keyPath);
        } else {
            juce::ColourGradient blackGrad(juce::Colour(0xff444444), keyRect.getX(), keyRect.getY(),
                                           juce::Colour(0xff1a1a1a), keyRect.getX(), keyRect.getBottom(), false);
            g.setGradientFill(blackGrad);
            g.fillPath(keyPath);
        }

        // Fade overlay: gradient from key centre upward
        if (k.fade > fadeEpsilon) {
            juce::ColourGradient fadeGrad(k.colour1, keyRect.getCentreX(), keyRect.getCentreY(),
                                          k.colour1.withAlpha(0.0f), keyRect.getCentreX(), keyRect.getY(), false);
            g.setGradientFill(fadeGrad);
            g.fillPath(keyPath);
        }

        // Border
        g.setColour(juce::Colour(0xff333333));
        g.strokePath(keyPath, juce::PathStrokeType(1.0f));
        // 1px ebony bevel edge highlight (#666666) on top edge
        g.setColour(juce::Colour(0xff666666));
        g.drawHorizontalLine(static_cast<int>(keyRect.getY()), keyRect.getX() + 1.0f, keyRect.getRight() - 1.0f);
    }
}

void CustomKeyboard::paintKeyLabels(juce::Graphics& g) {
    for (const auto& k : keys) {
        auto& customLabel = settings.customKeyLabels[static_cast<std::size_t>(k.midiNote)];

        if (k.isWhite) {
            // ── White key labels: bottom half ──
            auto fontH = static_cast<float>(juce::jlimit(8, 14, static_cast<int>(settings.keyWidth * 0.45f)));
            g.setFont(fontH);
            g.setColour(juce::Colour(0xff888888));

            if (customLabel.isNotEmpty()) {
                auto area = k.bounds.withTrimmedTop(k.bounds.getHeight() * 0.5f).reduced(1, 2);
                g.drawText(customLabel, area, juce::Justification::centred, false);
            } else if (k.keyLabel.isNotEmpty()) {
                auto area = k.bounds.withTrimmedTop(k.bounds.getHeight() * 0.5f).reduced(1, 2);
                g.drawText(k.keyLabel, area, juce::Justification::centred, false);
            } else if (k.midiNote >= 0) {
                auto name = devpiano::ui::getNoteDisplayName(k.midiNote, settings.noteDisplay, settings.keySignature);

                auto plusPos = name.indexOfChar('+');
                auto minusPos = name.indexOfChar('-');
                auto splitPos = (plusPos >= 0) ? plusPos : minusPos;

                if (splitPos > 0) {
                    auto topLine = name.substring(0, splitPos);
                    auto bottomLine = name.substring(splitPos);
                    auto area = k.bounds.withTrimmedTop(k.bounds.getHeight() * 0.5f).reduced(1, 2);
                    auto lineH = fontH * 1.25f;
                    auto topArea = area.withHeight(lineH).translated(0, (area.getHeight() - lineH * 2.0f) * 0.5f);
                    auto bottomArea = topArea.translated(0, lineH);
                    g.drawText(topLine, topArea, juce::Justification::centred, false);
                    g.drawText(bottomLine, bottomArea, juce::Justification::centred, false);
                } else {
                    auto area = k.bounds.withTrimmedTop(k.bounds.getHeight() * 0.5f).reduced(1, 2);
                    g.drawText(name, area, juce::Justification::centred, false);
                }
            }
        } else {
            // ── Black key labels: upper portion, binding label only ──
            if (customLabel.isEmpty() && k.keyLabel.isEmpty())
                continue;

            auto bkFontH = static_cast<float>(juce::jmin(10, static_cast<int>(settings.keyWidth * 0.4f)));
            g.setFont(bkFontH);
            g.setColour(juce::Colour(0xffcccccc));
            auto label = customLabel.isNotEmpty() ? customLabel : k.keyLabel;
            auto area = k.bounds.withTrimmedBottom(k.bounds.getHeight() * 0.35f).reduced(1, 2);
            g.drawText(label, area, juce::Justification::centred, false);
        }
    }
}

// ============================================================================
// Mouse interaction
// ============================================================================

void CustomKeyboard::mouseDown(const juce::MouseEvent& e) {
    auto note = findNoteAt(e.getPosition());
    if (note < 0)
        return;

    lastMouseDownNote = note;

    // Immediately visualise the press
    for (auto& k : keys) {
        if (k.midiNote == note) {
            k.fade = 1.0f;
            k.colour1 = classicColourTop(1.0f);
            break;
        }
    }
    repaint();

    if (onNoteOn) {
        auto ch = (note >= 0 && note < 128) ? static_cast<int>(perKeyChannel[static_cast<std::size_t>(note)]) : 0;
        onNoteOn(note, ch);
    }
    ensureTimerRunning();
}

void CustomKeyboard::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);

    if (lastMouseDownNote >= 0) {
        auto note = lastMouseDownNote;
        lastMouseDownNote = -1;

        // Let fade decay naturally; timer will handle it.
        ensureTimerRunning();

        if (onNoteOff) {
            auto ch = (note >= 0 && note < 128) ? static_cast<int>(perKeyChannel[static_cast<std::size_t>(note)]) : 0;
            onNoteOff(note, ch);
        }
    }
}

void CustomKeyboard::mouseDrag(const juce::MouseEvent& e) {
    if (lastMouseDownNote < 0)
        return;

    auto note = findNoteAt(e.getPosition());
    if (note == lastMouseDownNote || note < 0)
        return;

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
            break;
        }
    }
    repaint();

    if (onNoteOn) {
        auto ch = (note >= 0 && note < 128) ? static_cast<int>(perKeyChannel[static_cast<std::size_t>(note)]) : 0;
        onNoteOn(note, ch);
    }
    ensureTimerRunning();
}

void CustomKeyboard::mouseDoubleClick(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);

    auto note = findNoteAt(e.getPosition());
    if (note >= 0 && onBindingEditRequested)
        onBindingEditRequested(note);
}

// ============================================================================
// Fade animation
// ============================================================================

void CustomKeyboard::timerCallback() {
    bool anyActive = false;
    bool anyChanged = false;

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
        if (noteHeld || std::abs(k.fade - target) > fadeEpsilon)
            anyActive = true;
        if (std::abs(k.fade - before) > fadeEpsilon)
            anyChanged = true;

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
                        auto ch = perKeyChannel[idx] % 16;
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
    }

    if (anyActive && anyChanged)
        repaint();

    if (!anyActive)
        stopTimer();
}

void CustomKeyboard::ensureTimerRunning() {
    if (!isTimerRunning())
        startTimer(timerIntervalMs);
}
void CustomKeyboard::notifyNoteActivity() {
    ensureTimerRunning();
}

void CustomKeyboard::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity) {
    if (midiNoteNumber >= 0 && midiNoteNumber < 128 && velocity > 0.0f)
        perKeyVelocity[static_cast<std::size_t>(midiNoteNumber)] = velocity;
    ensureTimerRunning();
}

void CustomKeyboard::handleNoteOff(juce::MidiKeyboardState*, int, int, float) {
    ensureTimerRunning();
}

// ============================================================================
// Resize
// ============================================================================

void CustomKeyboard::resized() {
    if (!resizing)
        recalculateKeyBounds();
}

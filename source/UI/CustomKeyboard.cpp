#include "UI/CustomKeyboard.h"

#include <cmath>

// ============================================================================
// Helpers
// ============================================================================

namespace {

// Colour helpers (FreePiano classic mode: warm orange)
static juce::Colour classicColourTop(float fade) {
    auto s = 0.3f + 0.7f * fade;
    return juce::Colour::fromHSV(27.0f / 360.0f, s, 1.0f, fade);
}

// Channel colour mode: 16 predefined hues (FreePiano values)
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
    lowestVisibleNote = note;
    recalculateKeyBounds();
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

    auto totalWidth = static_cast<float>(getWidth());
    if (totalWidth < 1.0f)
        totalWidth = 800.0f;

    // --- White keys ---

    // Count white keys in range so we can distribute them evenly.
    int whiteKeyCount = 0;
    for (int n = rangeLow; n <= rangeHigh; ++n)
        if (devpiano::ui::isWhiteKey(n))
            ++whiteKeyCount;

    if (whiteKeyCount == 0)
        return;

    auto whiteKeyWidth = settings.keyWidth;
    auto totalWhiteWidth = whiteKeyWidth * static_cast<float>(whiteKeyCount);

    // If the total white-key width is smaller than the component, use the
    // computed width (keys align left). Otherwise scale to fit.
    float scale = 1.0f;
    if (totalWhiteWidth > totalWidth)
        scale = totalWidth / totalWhiteWidth;

    auto actualWhiteWidth = whiteKeyWidth * scale;
    auto blackKeyWidth = actualWhiteWidth * 0.6f;
    auto whiteKeyHeight = totalHeight;
    auto blackKeyHeight = totalHeight * 0.6f;

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

        // Map black-key semitone to the white key immediately to its left.
        auto semi = n % 12;
        auto leftWhiteNote = blackKeyLeftWhiteNote[semi];
        if (leftWhiteNote < 0)
            continue;

        // leftWhiteNote is the semitone within the octave (0-11).
        // Compute the actual MIDI note of that white key in the same octave.
        auto octaveBase = n - semi;
        auto leftWhiteMidi = octaveBase + leftWhiteNote;

        // Count white keys from rangeLow to leftWhiteMidi to get the vector index.
        int whiteVecIdx = 0;
        for (int m = rangeLow; m <= leftWhiteMidi; ++m)
            if (devpiano::ui::isWhiteKey(m))
                ++whiteVecIdx;

        // whiteVecIdx is 1-based count; convert to 0-based index.
        auto idx = static_cast<std::size_t>(whiteVecIdx - 1);
        if (whiteVecIdx > 0 && idx < keys.size()) {
            auto leftX = keys[idx].bounds.getX();
            auto leftWidth = keys[idx].bounds.getWidth();

            // Black key centred at the right edge of the left white key.
            auto centreX = leftX + leftWidth;
            k.bounds = { centreX - blackKeyWidth * 0.5f, 0.0f, blackKeyWidth, blackKeyHeight };
            keys.push_back(k);
        }
    }

    // Sort keys so that lower notes come first (black keys were appended after
    // their white-key neighbours; this sort ensures paint order is correct).
    std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.midiNote < b.midiNote; });
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
    paintWhiteKeys(g);
    paintBlackKeys(g);
    paintKeyLabels(g);
}

void CustomKeyboard::paintWhiteKeys(juce::Graphics& g) {
    for (const auto& k : keys) {
        if (!k.isWhite)
            continue;

        auto& b = k.bounds;

        // Base white key fill
        g.setColour(juce::Colour(0xffe8e8e8));
        g.fillRect(b);

        // Fade overlay (pressed or recently pressed)
        if (k.fade > fadeEpsilon) {
            g.setColour(k.colour1);
            g.fillRect(b);
        }

        // Border
        g.setColour(juce::Colour(0xff888888));
        g.drawRect(b, 1.0f);
    }
}

void CustomKeyboard::paintBlackKeys(juce::Graphics& g) {
    for (const auto& k : keys) {
        if (k.isWhite)
            continue;

        auto& b = k.bounds;

        // Base black key fill
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(b);

        // Fade overlay
        if (k.fade > fadeEpsilon) {
            g.setColour(k.colour1);
            g.fillRect(b);
        }

        // Border
        g.setColour(juce::Colour(0xff555555));
        g.drawRect(b, 1.0f);
    }
}

void CustomKeyboard::paintKeyLabels(juce::Graphics& g) {
    g.setFont(static_cast<float>(juce::jlimit(8, 16, static_cast<int>(settings.keyWidth * 0.5f))));
    g.setColour(juce::Colour(0xff888888));

    for (const auto& k : keys) {
        if (!k.isWhite)
            continue;

        if (k.keyLabel.isNotEmpty()) {
            g.drawText(k.keyLabel, k.bounds.reduced(2).withTrimmedBottom(2), juce::Justification::centredBottom, false);
        } else if (k.midiNote >= 0) {
            auto name = devpiano::ui::getNoteDisplayName(k.midiNote, settings.noteDisplay, settings.keySignature);
            g.drawText(name, k.bounds.reduced(2).withTrimmedBottom(2), juce::Justification::centredBottom, false);
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

        // Recompute colour based on active colour mode
        if (k.fade > fadeEpsilon) {
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
                    auto h = velocityHue(perKeyVelocity[idx]);
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

void CustomKeyboard::handleNoteOn(juce::MidiKeyboardState*, int, int, float) {
    ensureTimerRunning();
}

void CustomKeyboard::handleNoteOff(juce::MidiKeyboardState*, int, int, float) {
    ensureTimerRunning();
}

// ============================================================================
// Resize
// ============================================================================

void CustomKeyboard::resized() {
    recalculateKeyBounds();
}

#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>

namespace devpiano::ui {

// ============================================================================
// Keyboard rendering enums and data types
// ============================================================================

// Per-key colouring mode (mirrors old FreePiano AUTO_COLOR_*)
enum class KeyColourMode : uint8_t {
    classic = 0, // warm orange hue
    channel = 1, // 16-channel hue palette
    velocity = 2, // green-red velocity gradient
};

// Note-display mode (mirrors old FreePiano NOTE_DISPLAY_*)
enum class NoteDisplayMode : uint8_t {
    doReMi = 0, // "1" "#1" "2" … (solfège numbers, absolute)
    fixedDo = 1, // same as doReMi with key-signature offset applied
    noteName = 2, // "C" "#C" "D" … (standard note names)
};

// Per-key rendering state (recalculated every frame, not persisted)
struct KeyRenderState {
    int midiNote = -1;
    float fade = 0.0f; // [0, 1], decays when key not pressed
    juce::Colour colour1 { 0x00000000 };
    juce::Rectangle<float> bounds;
    juce::String keyLabel; // computer-key binding label ("A", "S", …)
    bool isWhite = false;
};

// Keyboard display settings (persisted via SettingsModel)
struct KeyboardSettings {
    int lowNote = 24;
    int highNote = 96;
    float keyWidth = 24.0f;

    KeyColourMode colourMode = KeyColourMode::classic;
    NoteDisplayMode noteDisplay = NoteDisplayMode::doReMi;

    float fadeSpeed = 0.92f; // per-tick fade decay factor
    float previewAlpha = 0.0f; // fade floor after release
    int keySignature = 0; // semitone offset for fixedDo / noteName
    int baseOctave = 4; // reference octave for note names

    // Per-key custom colour (transparent = not set, use colourMode instead)
    std::array<juce::Colour, 128> customKeyColours;

    // Per-key custom label (empty = not set, use binding displayText or note name)
    std::array<juce::String, 128> customKeyLabels;
};

// ============================================================================
// Note display helpers
// ============================================================================

// Solfège number names per semitone (absolute)
constexpr const char* doReMiNames[12] = { "1", "#1", "2", "#2", "3", "4", "#4", "5", "#5", "6", "#6", "7" };

// Standard note names per semitone
constexpr const char* noteLetterNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// Return the display name for a MIDI note in the given mode.
// Octave convention matches FreePiano:
//   (midiNote - 12) / 12  is the octave index, with index 4 = C4 (MIDI 60).
//   doReMi/fixedDo offset is relative to C4 (index 4 → offset "0").
//   noteName uses the standard octave number (C4 → "C4").
inline juce::String getNoteDisplayName(int midiNote, NoteDisplayMode mode, int keySignature = 0) {
    if (midiNote < 0 || midiNote > 127)
        return {};

    // FreePiano-style octave index: C0 starts at MIDI 12.
    auto octaveIndex = (midiNote - 12) / 12;
    auto noteIndex = midiNote % 12;

    switch (mode) {
    case NoteDisplayMode::doReMi: {
        // doReMi offset: C4 = index 4 → offset 0.
        auto offset = octaveIndex - 4;
        juce::String offStr;
        if (offset >= 0)
            offStr = "+" + juce::String(offset);
        else
            offStr = juce::String(offset);
        return doReMiNames[noteIndex] + offStr;
    }
    case NoteDisplayMode::fixedDo: {
        auto shiftedNoteIndex = (noteIndex + keySignature) % 12;
        if (shiftedNoteIndex < 0)
            shiftedNoteIndex += 12;
        auto offset = octaveIndex - 4;
        juce::String offStr;
        if (offset >= 0)
            offStr = "+" + juce::String(offset);
        else
            offStr = juce::String(offset);
        return doReMiNames[shiftedNoteIndex] + offStr;
    }
    case NoteDisplayMode::noteName:
    default: {
        // Standard octave number: MIDI 60 = C4 → octave 4.
        return juce::String(noteLetterNames[noteIndex]) + juce::String(octaveIndex);
    }
    }
}

// ============================================================================
// Piano key geometry
// ============================================================================

// Number of white keys within a full octave
inline constexpr int whiteKeysPerOctave = 7;

// White-key index within octave for each semitone
//  C=0, C#=-1, D=1, D#=-1, E=2, F=3, F#=-1, G=4, G#=-1, A=5, A#=-1, B=6
inline constexpr int whiteKeyIndexForNote[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };

// Returns true when the given MIDI note is a white (natural) key.
inline constexpr bool isWhiteKey(int midiNote) {
    return whiteKeyIndexForNote[midiNote % 12] >= 0;
}

} // namespace devpiano::ui

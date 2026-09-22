#pragma once

#include <cstdint>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace devpiano::core {

// ============================================================================
// Note-display mode
// ============================================================================
enum class NoteDisplayMode : uint8_t {
    doReMi = 0, // "1" "#1" "2" ... (solfege numbers, absolute)
    fixedDo = 1, // same as doReMi with key-signature offset applied
    noteName = 2, // "C" "#C" "D" ... (standard note names)
};

// Solfege number names per semitone (absolute)
constexpr const char* doReMiNames[12] = { "1", "#1", "2", "#2", "3", "4", "#4", "5", "#5", "6", "#6", "7" };

// Standard note names per semitone
constexpr const char* noteLetterNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// Return the display name for a MIDI note in the given mode.
// Octave convention: C0 starts at MIDI 12.
//   (midiNote - 12) / 12  is the octave index, with index 4 = C4 (MIDI 60).
//   doReMi/fixedDo offset is relative to C4 (index 4 -> offset "0").
//   noteName uses the standard octave number (C4 -> "C4").
inline juce::String getNoteDisplayName(int midiNote, NoteDisplayMode mode, int keySignature = 0) {
    if (midiNote < 0 || midiNote > 127) {
        return {};
    }

    auto octaveIndex = (midiNote - 12) / 12;
    auto noteIndex = midiNote % 12;

    switch (mode) {
    case NoteDisplayMode::doReMi: {
        auto offset = octaveIndex - 4;
        return doReMiNames[noteIndex] + juce::String(offset >= 0 ? "+" : "") + juce::String(offset);
    }
    case NoteDisplayMode::fixedDo: {
        auto shiftedNoteIndex = (noteIndex + keySignature) % 12;
        if (shiftedNoteIndex < 0) {
            shiftedNoteIndex += 12;
        }
        auto offset = octaveIndex - 4;
        return doReMiNames[shiftedNoteIndex] + juce::String(offset >= 0 ? "+" : "") + juce::String(offset);
    }
    case NoteDisplayMode::noteName:
    default: {
        return juce::String(noteLetterNames[noteIndex]) + juce::String(octaveIndex);
    }
    }
}

// Number of white keys within a full octave
inline constexpr int whiteKeysPerOctave = 7;

// White-key index within octave for each semitone
//  C=0, C#=-1, D=1, D#=-1, E=2, F=3, F#=-1, G=4, G#=-1, A=5, A#=-1, B=6
inline constexpr int whiteKeyIndexForNote[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };

// Returns true when the given MIDI note is a white (natural) key.
inline constexpr bool isWhiteKey(int midiNote) {
    return whiteKeyIndexForNote[midiNote % 12] >= 0;
}

// ============================================================================
// 12-TET Pitch-Class Chromatic Harmony Color Palette
// ============================================================================
// Symmetric chromatic hue wheel (30 deg intervals).
// Triads (e.g. C-E-G) form visually balanced triangular triads on the wheel,
// while tritones (C-F#) form opposing complementary colors.
constexpr float pitchClassHarmonyHues[12] = {
    0.0f, // C:  Coral Red (0 deg)
    30.0f, // C#: Vermilion (30 deg)
    60.0f, // D:  Amber Orange (60 deg)
    90.0f, // D#: Golden Yellow (90 deg)
    120.0f, // E:  Lime Green (120 deg)
    150.0f, // F:  Emerald Green (150 deg)
    180.0f, // F#: Mint Cyan (180 deg)
    210.0f, // G:  Cerulean Sky Blue (210 deg)
    240.0f, // G#: Cobalt Blue (240 deg)
    270.0f, // A:  Indigo Violet (270 deg)
    300.0f, // A#: Purple Orchid (300 deg)
    330.0f // B:  Magenta Rose (330 deg)
};

[[nodiscard]] inline juce::Colour getPitchClassHarmonyColour(int midiNote, float saturation = 0.82f,
                                                             float brightness = 0.98f, float alpha = 1.0f) {
    if (midiNote < 0 || midiNote > 127) {
        return juce::Colours::transparentBlack;
    }
    const auto pitchClass = (midiNote % 12 + 12) % 12;
    return juce::Colour::fromHSV(pitchClassHarmonyHues[pitchClass] / 360.0f, saturation, brightness, alpha);
}

[[nodiscard]] inline juce::Colour getContrastingTextColour(juce::Colour bg) {
    const auto luminance = (0.299f * static_cast<float>(bg.getRed()) + 0.587f * static_cast<float>(bg.getGreen())
                            + 0.114f * static_cast<float>(bg.getBlue()))
        / 255.0f;
    return (luminance > 0.58f) ? juce::Colour(0xFF0F172A) : juce::Colours::white;
}
} // namespace devpiano::core

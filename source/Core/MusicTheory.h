#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

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

// ============================================================================
// Chord Recognition & Harmonic Analysis (Phase 35-C)
// ============================================================================

enum class ChordQuality : uint8_t {
    unknown = 0,
    singleNote,
    powerChord,
    majorTriad,
    minorTriad,
    diminishedTriad,
    augmentedTriad,
    sus4,
    sus2,
    dominant7th,
    major7th,
    minor7th,
    halfDiminished7th,
    diminished7th,
    minorMajor7th,
    add9,
    major6th,
    minor6th,
    dominant9th,
    major9th,
    minor9th
};

enum class ChordInversion : uint8_t {
    rootPosition = 0,
    firstInversion = 1,
    secondInversion = 2,
    thirdInversion = 3,
    customSlash = 4
};

struct ChordInfo {
    bool isValid = false;
    int rootPitchClass = -1; // 0..11, 0 = C
    int bassPitchClass = -1; // 0..11, lowest sounding note's pitch class
    int lowestMidiNote = -1;
    ChordQuality quality = ChordQuality::unknown;
    ChordInversion inversion = ChordInversion::rootPosition;
    juce::String rootName; // "C", "F#", "Eb"
    juce::String chordName; // "C", "Am7", "G/B", "Fsus4"
    juce::String qualityDescription; // "Major Triad", "Minor 7th", etc.
    juce::String inversionDescription; // "Root Position", "1st Inversion", etc.
    uint16_t pitchClassMask = 0; // 12-bit mask of active pitch classes
    int activeNoteCount = 0;
};

// Standard chord root naming (7-bit ASCII standard musical names)
constexpr const char* chordPitchClassNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

struct ChordPattern {
    ChordQuality quality;
    uint16_t mask;
    const char* suffix;
    const char* description;
    int priority; // Higher is preferred when subsets conflict
};

inline const std::vector<ChordPattern>& getChordPatterns() {
    static const std::vector<ChordPattern> patterns
        = { // 9th chords (highest priority)
            { ChordQuality::major9th, 0x895, "maj9", "Major 9th", 90 },
            { ChordQuality::dominant9th, 0x495, "9", "Dominant 9th", 88 },
            { ChordQuality::minor9th, 0x48D, "m9", "Minor 9th", 86 },
            { ChordQuality::add9, 0x095, "add9", "Add 9", 84 },

            // 7th chords
            { ChordQuality::major7th, 0x891, "maj7", "Major 7th", 78 },
            { ChordQuality::minor7th, 0x489, "m7", "Minor 7th", 76 },
            { ChordQuality::dominant7th, 0x491, "7", "Dominant 7th", 74 },
            { ChordQuality::halfDiminished7th, 0x449, "m7b5", "Half-Diminished 7th", 72 },
            { ChordQuality::diminished7th, 0x249, "dim7", "Diminished 7th", 70 },
            { ChordQuality::minorMajor7th, 0x889, "m(maj7)", "Minor Major 7th", 68 },

            // 6th chords
            { ChordQuality::major6th, 0x291, "6", "Major 6th", 65 },
            { ChordQuality::minor6th, 0x289, "m6", "Minor 6th", 64 },

            // Shell voicings (no 5th)
            { ChordQuality::dominant7th, 0x411, "7(no5)", "Dominant 7th (no 5th)", 60 },
            { ChordQuality::major7th, 0x811, "maj7(no5)", "Major 7th (no 5th)", 59 },
            { ChordQuality::minor7th, 0x409, "m7(no5)", "Minor 7th (no 5th)", 58 },

            // Triads
            { ChordQuality::majorTriad, 0x091, "", "Major Triad", 50 },
            { ChordQuality::minorTriad, 0x089, "m", "Minor Triad", 48 },
            { ChordQuality::sus4, 0x0A1, "sus4", "Suspended 4th", 46 },
            { ChordQuality::sus2, 0x085, "sus2", "Suspended 2nd", 44 },
            { ChordQuality::diminishedTriad, 0x049, "dim", "Diminished Triad", 42 },
            { ChordQuality::augmentedTriad, 0x111, "aug", "Augmented Triad", 40 },

            // Power chord (2 unique notes)
            { ChordQuality::powerChord, 0x081, "5", "Power Chord (5th)", 30 }
          };
    return patterns;
}

[[nodiscard]] inline ChordInfo detectChord(const std::vector<int>& activeMidiNotes) {
    ChordInfo info;
    info.activeNoteCount = static_cast<int>(activeMidiNotes.size());
    if (activeMidiNotes.empty()) {
        return info;
    }

    // Find lowest MIDI note and compute pitch class bitmask
    int lowestNote = 128;
    uint16_t mask = 0;
    int uniquePitchClasses = 0;

    for (int note : activeMidiNotes) {
        if (note < 0 || note > 127) {
            continue;
        }
        if (note < lowestNote) {
            lowestNote = note;
        }
        const int pc = (note % 12 + 12) % 12;
        if ((mask & (1u << pc)) == 0) {
            mask |= static_cast<uint16_t>(1u << pc);
            ++uniquePitchClasses;
        }
    }

    if (lowestNote > 127 || mask == 0) {
        return info;
    }

    info.lowestMidiNote = lowestNote;
    info.bassPitchClass = (lowestNote % 12 + 12) % 12;
    info.pitchClassMask = mask;
    info.isValid = true;

    // Single note case
    if (uniquePitchClasses == 1) {
        info.rootPitchClass = info.bassPitchClass;
        info.rootName = chordPitchClassNames[info.rootPitchClass];
        info.quality = ChordQuality::singleNote;
        info.inversion = ChordInversion::rootPosition;
        info.chordName = getNoteDisplayName(lowestNote, NoteDisplayMode::noteName);
        info.qualityDescription = "Single Note";
        info.inversionDescription = "Root Position";
        return info;
    }

    const auto& patterns = getChordPatterns();

    // Matching candidate structure
    struct Candidate {
        int root = 0;
        const ChordPattern* pattern = nullptr;
        int score = 0;
        bool exact = false;
    };

    std::vector<Candidate> candidates;

    // Test each of the 12 possible root notes
    for (int root = 0; root < 12; ++root) {
        // Rotate mask so root becomes bit 0
        const uint16_t rotated = static_cast<uint16_t>(((mask >> root) | (mask << (12 - root))) & 0x0FFF);

        for (const auto& pat : patterns) {
            if (rotated == pat.mask) {
                // Exact pitch class set match!
                int score = pat.priority * 10;
                // Bass note preference: if bass note is root, boost score
                if (root == info.bassPitchClass) {
                    score += 25;
                }
                candidates.push_back({ root, &pat, score, true });
            } else if ((rotated & pat.mask) == pat.mask) {
                // Superset match: active notes contain all pattern notes, plus extra note(s)
                const int extraNotes = uniquePitchClasses - std::popcount(pat.mask);
                int score = pat.priority * 5 - extraNotes * 15;
                if (root == info.bassPitchClass) {
                    score += 15;
                }
                candidates.push_back({ root, &pat, score, false });
            }
        }
    }

    if (!candidates.empty()) {
        // Sort by exact match first, then by score descending
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.exact != b.exact) {
                return a.exact > b.exact;
            }
            return a.score > b.score;
        });

        const auto& best = candidates.front();
        info.rootPitchClass = best.root;
        info.rootName = chordPitchClassNames[best.root];
        info.quality = best.pattern->quality;
        info.qualityDescription = best.pattern->description;

        // Determine inversion based on bass note
        const int bassInterval = (info.bassPitchClass - best.root + 12) % 12;
        if (bassInterval == 0) {
            info.inversion = ChordInversion::rootPosition;
            info.inversionDescription = "Root Position";
            info.chordName = info.rootName + best.pattern->suffix;
        } else {
            // Check inversion type
            if (bassInterval == 3 || bassInterval == 4) {
                info.inversion = ChordInversion::firstInversion;
                info.inversionDescription = "1st Inversion";
            } else if (bassInterval == 6 || bassInterval == 7 || bassInterval == 8) {
                info.inversion = ChordInversion::secondInversion;
                info.inversionDescription = "2nd Inversion";
            } else if (bassInterval == 9 || bassInterval == 10 || bassInterval == 11) {
                info.inversion = ChordInversion::thirdInversion;
                info.inversionDescription = "3rd Inversion";
            } else {
                info.inversion = ChordInversion::customSlash;
                info.inversionDescription = "Slash Chord";
            }
            info.chordName = info.rootName + best.pattern->suffix + "/" + chordPitchClassNames[info.bassPitchClass];
        }
        return info;
    }

    // Fallback: unrecognised combination
    info.rootPitchClass = info.bassPitchClass;
    info.rootName = chordPitchClassNames[info.rootPitchClass];
    info.quality = ChordQuality::unknown;
    info.inversion = ChordInversion::rootPosition;
    info.chordName = info.rootName + " (Cluster)";
    info.qualityDescription = "Cluster";
    info.inversionDescription = "Root Position";
    return info;
}
} // namespace devpiano::core

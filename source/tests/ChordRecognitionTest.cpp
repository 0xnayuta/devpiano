#include "Core/MusicTheory.h"
#include <JuceHeader.h>

namespace {

class ChordRecognitionTest final : public juce::UnitTest {
public:
    ChordRecognitionTest()
        : juce::UnitTest("ChordRecognition: Harmonic Analysis & Pitch Class Set", "devpiano") {
    }

    void runTest() override {
        testEmptyAndSingleNote();
        testMajorAndMinorTriadsWithInversions();
        testSuspendedAndDiminishedTriads();
        testSeventhAndNinthChords();
        testOctaveDuplicatesAndPowerChords();
        testSlashChordsAndBassPreference();
        testNoiseToleranceAndSupersetMatching();
    }

private:
    void testEmptyAndSingleNote() {
        beginTest("Empty and single note detection");

        // 1. Empty input
        const auto emptyChord = devpiano::core::detectChord({});
        expect(!emptyChord.isValid);
        expectEquals(emptyChord.activeNoteCount, 0);

        // 2. Single note: C4 (MIDI 60)
        const auto c4 = devpiano::core::detectChord({ 60 });
        expect(c4.isValid);
        expectEquals(c4.rootPitchClass, 0);
        expectEquals(c4.bassPitchClass, 0);
        expect(c4.quality == devpiano::core::ChordQuality::singleNote);
        expectEquals(c4.chordName, juce::String("C4"));

        // 3. Single note: F#3 (MIDI 54)
        const auto fs3 = devpiano::core::detectChord({ 54 });
        expect(fs3.isValid);
        expectEquals(fs3.rootPitchClass, 6);
        expectEquals(fs3.bassPitchClass, 6);
        expectEquals(fs3.chordName, juce::String("F#3"));
    }

    void testMajorAndMinorTriadsWithInversions() {
        beginTest("Major and minor triads: Root position and inversions");

        // 1. C Major root position: C4 (60), E4 (64), G4 (67)
        const auto cMajor = devpiano::core::detectChord({ 60, 64, 67 });
        expect(cMajor.isValid);
        expectEquals(cMajor.rootPitchClass, 0);
        expectEquals(cMajor.bassPitchClass, 0);
        expect(cMajor.quality == devpiano::core::ChordQuality::majorTriad);
        expect(cMajor.inversion == devpiano::core::ChordInversion::rootPosition);
        expectEquals(cMajor.chordName, juce::String("C"));

        // 2. C Major 1st inversion (C/E): E3 (52), G3 (55), C4 (60)
        const auto cMajor1st = devpiano::core::detectChord({ 52, 55, 60 });
        expect(cMajor1st.isValid);
        expectEquals(cMajor1st.rootPitchClass, 0);
        expectEquals(cMajor1st.bassPitchClass, 4); // E
        expect(cMajor1st.quality == devpiano::core::ChordQuality::majorTriad);
        expect(cMajor1st.inversion == devpiano::core::ChordInversion::firstInversion);
        expectEquals(cMajor1st.chordName, juce::String("C/E"));

        // 3. C Major 2nd inversion (C/G): G3 (55), C4 (60), E4 (64)
        const auto cMajor2nd = devpiano::core::detectChord({ 55, 60, 64 });
        expect(cMajor2nd.isValid);
        expectEquals(cMajor2nd.rootPitchClass, 0);
        expectEquals(cMajor2nd.bassPitchClass, 7); // G
        expect(cMajor2nd.quality == devpiano::core::ChordQuality::majorTriad);
        expect(cMajor2nd.inversion == devpiano::core::ChordInversion::secondInversion);
        expectEquals(cMajor2nd.chordName, juce::String("C/G"));

        // 4. A Minor root position: A3 (57), C4 (60), E4 (64)
        const auto aMinor = devpiano::core::detectChord({ 57, 60, 64 });
        expect(aMinor.isValid);
        expectEquals(aMinor.rootPitchClass, 9); // A
        expectEquals(aMinor.bassPitchClass, 9);
        expect(aMinor.quality == devpiano::core::ChordQuality::minorTriad);
        expect(aMinor.inversion == devpiano::core::ChordInversion::rootPosition);
        expectEquals(aMinor.chordName, juce::String("Am"));

        // 5. A Minor 1st inversion (Am/C): C4 (60), E4 (64), A4 (69)
        const auto aMinor1st = devpiano::core::detectChord({ 60, 64, 69 });
        expect(aMinor1st.isValid);
        expectEquals(aMinor1st.rootPitchClass, 9);
        expectEquals(aMinor1st.bassPitchClass, 0); // C
        expect(aMinor1st.quality == devpiano::core::ChordQuality::minorTriad);
        expect(aMinor1st.inversion == devpiano::core::ChordInversion::firstInversion);
        expectEquals(aMinor1st.chordName, juce::String("Am/C"));
    }

    void testSuspendedAndDiminishedTriads() {
        beginTest("Suspended and diminished triads");

        // 1. Dsus4: D4 (62), G4 (67), A4 (69)
        const auto dsus4 = devpiano::core::detectChord({ 62, 67, 69 });
        expect(dsus4.isValid);
        expectEquals(dsus4.rootPitchClass, 2); // D
        expect(dsus4.quality == devpiano::core::ChordQuality::sus4);
        expectEquals(dsus4.chordName, juce::String("Dsus4"));

        // 2. Dsus2: D4 (62), E4 (64), A4 (69)
        const auto dsus2 = devpiano::core::detectChord({ 62, 64, 69 });
        expect(dsus2.isValid);
        expectEquals(dsus2.rootPitchClass, 2); // D
        expect(dsus2.quality == devpiano::core::ChordQuality::sus2);
        expectEquals(dsus2.chordName, juce::String("Dsus2"));

        // 3. B diminished: B3 (59), D4 (62), F4 (65)
        const auto bDim = devpiano::core::detectChord({ 59, 62, 65 });
        expect(bDim.isValid);
        expectEquals(bDim.rootPitchClass, 11); // B
        expect(bDim.quality == devpiano::core::ChordQuality::diminishedTriad);
        expectEquals(bDim.chordName, juce::String("Bdim"));

        // 4. C augmented: C4 (60), E4 (64), G#4 (68)
        const auto cAug = devpiano::core::detectChord({ 60, 64, 68 });
        expect(cAug.isValid);
        expectEquals(cAug.rootPitchClass, 0); // C
        expect(cAug.quality == devpiano::core::ChordQuality::augmentedTriad);
        expectEquals(cAug.chordName, juce::String("Caug"));
    }

    void testSeventhAndNinthChords() {
        beginTest("Seventh, sixth, and ninth chords");

        // 1. G7 (Dominant 7th): G3 (55), B3 (59), D4 (62), F4 (65)
        const auto g7 = devpiano::core::detectChord({ 55, 59, 62, 65 });
        expect(g7.isValid);
        expectEquals(g7.rootPitchClass, 7); // G
        expect(g7.quality == devpiano::core::ChordQuality::dominant7th);
        expectEquals(g7.chordName, juce::String("G7"));

        // 2. Cmaj7 (Major 7th): C4 (60), E4 (64), G4 (67), B4 (71)
        const auto cmaj7 = devpiano::core::detectChord({ 60, 64, 67, 71 });
        expect(cmaj7.isValid);
        expectEquals(cmaj7.rootPitchClass, 0); // C
        expect(cmaj7.quality == devpiano::core::ChordQuality::major7th);
        expectEquals(cmaj7.chordName, juce::String("Cmaj7"));

        // 3. Dm7 (Minor 7th): D4 (62), F4 (65), A4 (69), C5 (72)
        const auto dm7 = devpiano::core::detectChord({ 62, 65, 69, 72 });
        expect(dm7.isValid);
        expectEquals(dm7.rootPitchClass, 2); // D
        expect(dm7.quality == devpiano::core::ChordQuality::minor7th);
        expectEquals(dm7.chordName, juce::String("Dm7"));

        // 4. Bm7b5 (Half-diminished): B3 (59), D4 (62), F4 (65), A4 (69)
        const auto bm7b5 = devpiano::core::detectChord({ 59, 62, 65, 69 });
        expect(bm7b5.isValid);
        expectEquals(bm7b5.rootPitchClass, 11); // B
        expect(bm7b5.quality == devpiano::core::ChordQuality::halfDiminished7th);
        expectEquals(bm7b5.chordName, juce::String("Bm7b5"));

        // 5. Cdim7 (Diminished 7th): C4 (60), Eb4 (63), Gb4 (66), A4 (69)
        const auto cdim7 = devpiano::core::detectChord({ 60, 63, 66, 69 });
        expect(cdim7.isValid);
        expectEquals(cdim7.rootPitchClass, 0); // C
        expect(cdim7.quality == devpiano::core::ChordQuality::diminished7th);
        expectEquals(cdim7.chordName, juce::String("Cdim7"));

        // 6. Cadd9: C4 (60), D4 (62), E4 (64), G4 (67)
        const auto cadd9 = devpiano::core::detectChord({ 60, 62, 64, 67 });
        expect(cadd9.isValid);
        expectEquals(cadd9.rootPitchClass, 0); // C
        expect(cadd9.quality == devpiano::core::ChordQuality::add9);
        expectEquals(cadd9.chordName, juce::String("Cadd9"));
    }

    void testOctaveDuplicatesAndPowerChords() {
        beginTest("Octave duplicated voicing and power chords");

        // 1. C major with multiple octaves: C3 (48), G3 (55), C4 (60), E4 (64), G4 (67), C5 (72)
        const auto cVoicing = devpiano::core::detectChord({ 48, 55, 60, 64, 67, 72 });
        expect(cVoicing.isValid);
        expectEquals(cVoicing.rootPitchClass, 0);
        expectEquals(cVoicing.bassPitchClass, 0); // C3 is lowest
        expect(cVoicing.quality == devpiano::core::ChordQuality::majorTriad);
        expect(cVoicing.inversion == devpiano::core::ChordInversion::rootPosition);
        expectEquals(cVoicing.chordName, juce::String("C"));

        // 2. Power chord C5: C3 (48), G3 (55), C4 (60)
        const auto c5 = devpiano::core::detectChord({ 48, 55, 60 });
        expect(c5.isValid);
        expectEquals(c5.rootPitchClass, 0);
        expect(c5.quality == devpiano::core::ChordQuality::powerChord);
        expectEquals(c5.chordName, juce::String("C5"));
    }

    void testSlashChordsAndBassPreference() {
        beginTest("Slash chords and bass note disambiguation");

        // 1. G/B (G major over B): B2 (47), G3 (55), D4 (62)
        const auto gOverB = devpiano::core::detectChord({ 47, 55, 62 });
        expect(gOverB.isValid);
        expectEquals(gOverB.rootPitchClass, 7); // G
        expectEquals(gOverB.bassPitchClass, 11); // B
        expectEquals(gOverB.chordName, juce::String("G/B"));

        // 2. Am7 vs C6 disambiguation:
        // Notes: A, C, E, G. If bass is A -> Am7; if bass is C -> C6.
        const auto am7 = devpiano::core::detectChord({ 45, 60, 64, 67 }); // A2 bass
        expect(am7.isValid);
        expectEquals(am7.rootPitchClass, 9); // A
        expectEquals(am7.chordName, juce::String("Am7"));

        const auto c6 = devpiano::core::detectChord({ 48, 57, 64, 67 }); // C3 bass
        expect(c6.isValid);
        expectEquals(c6.rootPitchClass, 0); // C
        expectEquals(c6.chordName, juce::String("C6"));

        const auto am7b5 = devpiano::core::detectChord({ 45, 60, 63, 67 }); // A2 bass
        expect(am7b5.isValid);
        expectEquals(am7b5.rootPitchClass, 9); // A
        expect(am7b5.quality == devpiano::core::ChordQuality::halfDiminished7th);
        expectEquals(am7b5.chordName, juce::String("Am7b5"));

        const auto cm6 = devpiano::core::detectChord({ 48, 57, 63, 67 }); // C3 bass
        expect(cm6.isValid);
        expectEquals(cm6.rootPitchClass, 0); // C
        expect(cm6.quality == devpiano::core::ChordQuality::minor6th);
        expectEquals(cm6.chordName, juce::String("Cm6"));
    }

    void testNoiseToleranceAndSupersetMatching() {
        beginTest("Noise tolerance and superset matching");

        // C major (60, 64, 67) with an extraneous passing note D (62)
        // Should still recognize C as the primary harmonic root
        const auto cWithPassingNote = devpiano::core::detectChord({ 48, 60, 62, 64, 67 });
        expect(cWithPassingNote.isValid);
        expectEquals(cWithPassingNote.rootPitchClass, 0); // C
        const auto g7OverB = devpiano::core::detectChord({ 47, 55, 62, 65 });
        expect(g7OverB.isValid);
        expectEquals(g7OverB.rootPitchClass, 7); // G
        expect(g7OverB.quality == devpiano::core::ChordQuality::dominant7th);
        expectEquals(g7OverB.chordName, juce::String("G7/B"));
    }
};

ChordRecognitionTest chordRecognitionTest;

} // namespace

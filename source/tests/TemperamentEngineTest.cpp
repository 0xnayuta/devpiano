#include <JuceHeader.h>

#include "Audio/TemperamentEngine.h"

// ============================================================================
/// TemperamentEngineTest (Phase 30-A)
///
/// Validates mathematical invariants, A4 reference pitch anchoring,
/// octave doubling invariance, monotonic pitch ordering, cent offsets,
/// and identifier serialization across all 6 classical temperaments.
// ============================================================================
class TemperamentEngineTest final : public juce::UnitTest {
public:
    TemperamentEngineTest()
        : juce::UnitTest("TemperamentEngine", "DevPiano/Audio") {
    }

    void runTest() override {
        testEqualTemperamentProperties();
        testAnchorA4Invariance();
        testReferencePitchClampingAndPresets();
        testMonotonicityAcrossAllTemperaments();
        testOctaveInvariance();
        testSpecificTemperamentAcousticCharacteristics();
        testPitchClassAndOctaveHelpers();
        testIdentifierAndDisplayName();
    }

private:
    void testEqualTemperamentProperties() {
        beginTest("TemperamentEngine: 12-TET properties and frequency mapping");

        using namespace devpiano::audio;

        const auto equalOffsets = TemperamentEngine::getCentOffsets(Temperament::equal);
        for (int i = 0; i < TemperamentEngine::kNumPitchClasses; ++i) {
            expectEquals(equalOffsets[static_cast<std::size_t>(i)], 0.0f);
        }

        // Standard 12-TET A4 is 440 Hz
        const auto freqA4 = TemperamentEngine::getFrequency(69, Temperament::equal, 440.0);
        expectWithinAbsoluteError(freqA4, 440.0, 1e-6);

        // Standard 12-TET C4 (MIDI 60) = 440 * 2^(-9/12) ~ 261.6255653
        const auto freqC4 = TemperamentEngine::getFrequency(60, Temperament::equal, 440.0);
        expectWithinAbsoluteError(freqC4, 261.6255653, 1e-4);

        // A3 (MIDI 57) = 220 Hz
        const auto freqA3 = TemperamentEngine::getFrequency(57, Temperament::equal, 440.0);
        expectWithinAbsoluteError(freqA3, 220.0, 1e-6);

        // A5 (MIDI 81) = 880 Hz
        const auto freqA5 = TemperamentEngine::getFrequency(81, Temperament::equal, 440.0);
        expectWithinAbsoluteError(freqA5, 880.0, 1e-6);
    }

    void testAnchorA4Invariance() {
        beginTest("TemperamentEngine: A4 reference pitch anchor invariance");

        using namespace devpiano::audio;

        const std::array<Temperament, TemperamentEngine::kNumTemperaments> allTemperaments {
            Temperament::equal,    Temperament::just,          Temperament::pythagorean,
            Temperament::meantone, Temperament::werckmeister3, Temperament::kirnberger3
        };

        const std::array<double, 5> testPitches { 415.0, 432.0, 440.0, 442.0, 445.0 };

        for (const auto temp : allTemperaments) {
            // In all temperaments, pitch class 9 (A) must have exactly 0.0 cent deviation
            const auto centOffsets = TemperamentEngine::getCentOffsets(temp);
            expectEquals(centOffsets[9], 0.0f);

            for (const auto refPitch : testPitches) {
                // Note 69 (A4) must strictly equal reference pitch
                const auto fA4 = TemperamentEngine::getFrequency(69, temp, refPitch);
                expectWithinAbsoluteError(fA4, refPitch, 1e-6);

                // Note 57 (A3) must strictly equal reference pitch / 2
                const auto fA3 = TemperamentEngine::getFrequency(57, temp, refPitch);
                expectWithinAbsoluteError(fA3, refPitch * 0.5, 1e-6);

                // Note 81 (A5) must strictly equal reference pitch * 2
                const auto fA5 = TemperamentEngine::getFrequency(81, temp, refPitch);
                expectWithinAbsoluteError(fA5, refPitch * 2.0, 1e-6);
            }
        }
    }

    void testReferencePitchClampingAndPresets() {
        beginTest("TemperamentEngine: Reference pitch clamping and presets");

        using namespace devpiano::audio;

        expectEquals(TemperamentEngine::clampReferencePitch(440.0), 440.0);
        expectEquals(TemperamentEngine::clampReferencePitch(415.0), 415.0);
        expectEquals(TemperamentEngine::clampReferencePitch(432.0), 432.0);
        expectEquals(TemperamentEngine::clampReferencePitch(442.0), 442.0);

        // Clamping bounds [410.0, 450.0]
        expectEquals(TemperamentEngine::clampReferencePitch(300.0), 410.0);
        expectEquals(TemperamentEngine::clampReferencePitch(409.9), 410.0);
        expectEquals(TemperamentEngine::clampReferencePitch(450.1), 450.0);
        expectEquals(TemperamentEngine::clampReferencePitch(500.0), 450.0);

        // Verification of preset constants
        expectEquals(TemperamentEngine::kDefaultReferencePitch, 440.0);
        expectEquals(TemperamentEngine::kBaroquePitch, 415.0);
        expectEquals(TemperamentEngine::kVerdiPitch, 432.0);
        expectEquals(TemperamentEngine::kConcertPitch, 442.0);
    }

    void testMonotonicityAcrossAllTemperaments() {
        beginTest("TemperamentEngine: Monotonic frequency progression across entire MIDI range");

        using namespace devpiano::audio;

        const std::array<Temperament, TemperamentEngine::kNumTemperaments> allTemperaments {
            Temperament::equal,    Temperament::just,          Temperament::pythagorean,
            Temperament::meantone, Temperament::werckmeister3, Temperament::kirnberger3
        };

        for (const auto temp : allTemperaments) {
            double prevFreq = 0.0;
            for (int note = 0; note <= 127; ++note) {
                const auto freq = TemperamentEngine::getFrequency(note, temp, 440.0);
                expectGreaterThan(freq, 0.0);

                if (note > 0) {
                    // Each subsequent semitone must be strictly higher in frequency
                    expectGreaterThan(freq, prevFreq);

                    // Interval should be reasonably close to a semitone (~100 cents, between 50 and 150 cents)
                    const auto centsStep = 1200.0 * std::log2(freq / prevFreq);
                    expectGreaterThan(centsStep, 50.0);
                    expectLessThan(centsStep, 160.0);
                }
                prevFreq = freq;
            }
        }
    }

    void testOctaveInvariance() {
        beginTest("TemperamentEngine: Exact octave doubling invariance (2:1 ratio)");

        using namespace devpiano::audio;

        const std::array<Temperament, TemperamentEngine::kNumTemperaments> allTemperaments {
            Temperament::equal,    Temperament::just,          Temperament::pythagorean,
            Temperament::meantone, Temperament::werckmeister3, Temperament::kirnberger3
        };

        for (const auto temp : allTemperaments) {
            for (int note = 0; note <= 115; ++note) {
                const auto fLow = TemperamentEngine::getFrequency(note, temp, 440.0);
                const auto fHigh = TemperamentEngine::getFrequency(note + 12, temp, 440.0);
                const auto ratio = fHigh / fLow;

                // Octaves must be exactly 2.0
                expectWithinAbsoluteError(ratio, 2.0, 1e-6);
            }
        }
    }

    void testSpecificTemperamentAcousticCharacteristics() {
        beginTest("TemperamentEngine: Acoustic characteristics of specific temperaments");

        using namespace devpiano::audio;

        // 1. Just Intonation (5-limit): pure intervals
        // In 5-limit Just Intonation with A=5/3 (anchor A4=440), C4 = 440 * (3/5) = 264 Hz exactly!
        const auto justC4 = TemperamentEngine::getFrequency(60, Temperament::just, 440.0);
        expectWithinAbsoluteError(justC4, 264.0, 0.05);

        // G4 (pure fifth above C4, ratio 3/2): 264 * 1.5 = 396 Hz exactly!
        const auto justG4 = TemperamentEngine::getFrequency(67, Temperament::just, 440.0);
        expectWithinAbsoluteError(justG4, 396.0, 0.05);

        // E4 (pure major third above C4, ratio 5/4): 264 * 1.25 = 330 Hz exactly!
        const auto justE4 = TemperamentEngine::getFrequency(64, Temperament::just, 440.0);
        expectWithinAbsoluteError(justE4, 330.0, 0.05);

        // 2. Meantone 1/4 comma: pure major third C-E (386.31 cents)
        const auto meantoneC4 = TemperamentEngine::getFrequency(60, Temperament::meantone, 440.0);
        const auto meantoneE4 = TemperamentEngine::getFrequency(64, Temperament::meantone, 440.0);
        const auto meantoneThirdCents = 1200.0 * std::log2(meantoneE4 / meantoneC4);
        // In 1/4 meantone, major third C-E is pure 5/4 (~386.31 cents)
        expectWithinAbsoluteError(meantoneThirdCents, 386.31, 0.5);

        // In contrast, 12-TET major third is 400.0 cents
        const auto equalC4 = TemperamentEngine::getFrequency(60, Temperament::equal, 440.0);
        const auto equalE4 = TemperamentEngine::getFrequency(64, Temperament::equal, 440.0);
        const auto equalThirdCents = 1200.0 * std::log2(equalE4 / equalC4);
        expectWithinAbsoluteError(equalThirdCents, 400.0, 1e-4);

        // 3. Pythagorean: pure fifth C-G (701.96 cents, 3:2 ratio)
        const auto pythC4 = TemperamentEngine::getFrequency(60, Temperament::pythagorean, 440.0);
        const auto pythG4 = TemperamentEngine::getFrequency(67, Temperament::pythagorean, 440.0);
        const auto pythFifthCents = 1200.0 * std::log2(pythG4 / pythC4);
        expectWithinAbsoluteError(pythFifthCents, 701.955, 0.1);

        // 4. Kirnberger III: pure major third C-E (5/4 ratio, ~386.31 cents)
        const auto kirnC4 = TemperamentEngine::getFrequency(60, Temperament::kirnberger3, 440.0);
        const auto kirnE4 = TemperamentEngine::getFrequency(64, Temperament::kirnberger3, 440.0);
        const auto kirnThirdCents = 1200.0 * std::log2(kirnE4 / kirnC4);
        expectWithinAbsoluteError(kirnThirdCents, 386.3137, 0.05);
    }

    void testPitchClassAndOctaveHelpers() {
        beginTest("TemperamentEngine: Pitch class and octave helpers");

        using namespace devpiano::audio;

        // C4 is MIDI 60 (pitch class 0, octave 4)
        expectEquals(TemperamentEngine::getPitchClass(60), 0);
        expectEquals(TemperamentEngine::getOctave(60), 4);

        // A4 is MIDI 69 (pitch class 9, octave 4)
        expectEquals(TemperamentEngine::getPitchClass(69), 9);
        expectEquals(TemperamentEngine::getOctave(69), 4);

        // A0 is MIDI 21 (pitch class 9, octave 0)
        expectEquals(TemperamentEngine::getPitchClass(21), 9);
        expectEquals(TemperamentEngine::getOctave(21), 0);

        // C8 is MIDI 108 (pitch class 0, octave 8)
        expectEquals(TemperamentEngine::getPitchClass(108), 0);
        expectEquals(TemperamentEngine::getOctave(108), 8);

        // Clamping protection for boundary values
        expectEquals(TemperamentEngine::getPitchClass(-10), 0);
        expectEquals(TemperamentEngine::getPitchClass(150), 7); // 127 % 12 = 7 (G)
    }

    void testIdentifierAndDisplayName() {
        beginTest("TemperamentEngine: Identifier serialization and display names");

        using namespace devpiano::audio;

        const std::array<Temperament, TemperamentEngine::kNumTemperaments> allTemperaments {
            Temperament::equal,    Temperament::just,          Temperament::pythagorean,
            Temperament::meantone, Temperament::werckmeister3, Temperament::kirnberger3
        };

        for (const auto temp : allTemperaments) {
            const auto id = TemperamentEngine::getIdentifier(temp);
            expect(!id.empty());

            const auto recovered = TemperamentEngine::fromIdentifier(id);
            expect(recovered == temp);

            const auto displayName = TemperamentEngine::getDisplayName(temp);
            expect(!displayName.empty());
        }

        // Unknown identifier fallbacks to equal
        expect(TemperamentEngine::fromIdentifier("unknown_temperament") == Temperament::equal);
        expect(TemperamentEngine::fromIdentifier("") == Temperament::equal);
    }
};

static TemperamentEngineTest temperamentEngineTest;

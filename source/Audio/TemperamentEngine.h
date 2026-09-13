#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace devpiano::audio {

/**
 * Historical and modern tuning temperaments (Phase 30-A).
 * Provides mathematically grounded intonation systems with A4 reference anchor.
 */
enum class Temperament : std::uint8_t {
    equal = 0, ///< 12-Tone Equal Temperament (12-EDO standard)
    just = 1, ///< 5-limit Just Intonation (C-based harmonic pure ratios)
    pythagorean = 2, ///< Pythagorean tuning (stacked pure 3:2 fifths)
    meantone = 3, ///< 1/4-comma Meantone (Pietro Aaron 1523, pure major thirds)
    werckmeister3 = 4, ///< Werckmeister III (Andreas Werckmeister 1691, well-tempered)
    kirnberger3 = 5, ///< Kirnberger III (Johann Philipp Kirnberger 1779, well-tempered)
};

/**
 * Pure mathematical high-precision tuning engine.
 *
 * All cent offsets are relative to 12-TET with pitch class 9 (A) anchored at 0.0 cents,
 * ensuring that MIDI note 69 (A4) always precisely equals the configured reference pitch.
 *
 * Thread-safe, lock-free, zero-allocation, and suitable for real-time audio computation.
 */
class TemperamentEngine final {
public:
    static constexpr double kDefaultReferencePitch = 440.0;
    static constexpr double kMinReferencePitch = 410.0;
    static constexpr double kMaxReferencePitch = 450.0;
    static constexpr double kBaroquePitch = 415.0;
    static constexpr double kVerdiPitch = 432.0;
    static constexpr double kConcertPitch = 442.0;

    static constexpr int kNumTemperaments = 6;
    static constexpr int kNumPitchClasses = 12;

    /**
     * Clamps reference pitch to the valid acoustic range [410.0, 450.0] Hz.
     */
    [[nodiscard]] static constexpr double clampReferencePitch(double pitch) noexcept {
        return std::clamp(pitch, kMinReferencePitch, kMaxReferencePitch);
    }

    /**
     * Resolves the chromatic pitch class (0 = C, 1 = C#, ..., 9 = A, 10 = A#, 11 = B).
     */
    [[nodiscard]] static constexpr int getPitchClass(int midiNoteNumber) noexcept {
        const auto note = std::clamp(midiNoteNumber, 0, 127);
        return (note % kNumPitchClasses + kNumPitchClasses) % kNumPitchClasses;
    }

    /**
     * Resolves the octave number according to scientific pitch notation (C4 is octave 4, A4 is octave 4).
     */
    [[nodiscard]] static constexpr int getOctave(int midiNoteNumber) noexcept {
        const auto note = std::clamp(midiNoteNumber, 0, 127);
        return (note / kNumPitchClasses) - 1;
    }

    /**
     * Returns the 12-element cent deviation array relative to 12-TET (A = 0.0 cents anchor).
     * Pitch class index: 0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B.
     */
    [[nodiscard]] static constexpr std::array<float, kNumPitchClasses>
    getCentOffsets(Temperament temperament) noexcept {
        switch (temperament) {
        case Temperament::equal:
            return { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

        case Temperament::just:
            // 5-limit Just Intonation anchored at A=5/3 (0.0 cents deviation at A):
            // C: 15.641, C#: 27.373, D: 19.551, D#: 31.283, E: 1.955, F: 13.686,
            // F#: 5.865, G: 17.596, G#: 29.328, A: 0.0, A#: 33.238, B: 3.910
            return { 15.6413f, 27.3726f, 19.5513f, 31.2826f, 1.9550f,  13.6863f,
                     5.8650f,  17.5963f, 29.3276f, 0.0f,     33.2376f, 3.9100f };

        case Temperament::pythagorean:
            // Pythagorean tuning with stacked pure 3:2 fifths, anchored at A=0:
            // C: -5.865, C#: 7.820, D: -1.955, D#: -11.730, E: 1.955, F: -7.820,
            // F#: 5.865, G: -3.910, G#: 9.775, A: 0.0, A#: -9.775, B: 3.910
            return { -5.8650f, 7.8200f,  -1.9550f, -11.7300f, 1.9550f,  -7.8200f,
                     5.8650f,  -3.9100f, 9.7750f,  0.0f,      -9.7750f, 3.9100f };

        case Temperament::meantone:
            // 1/4-comma Meantone (Pietro Aaron, 1523) yielding pure major thirds, anchored at A=0:
            // C: +10.265, C#: -13.686, D: +3.422, D#: +20.529, E: -3.422, F: +13.686,
            // F#: -10.265, G: +6.843, G#: -17.108, A: 0.0, A#: +17.108, B: -6.843
            return { 10.2647f,  -13.6863f, 3.4216f,   20.5294f, -3.4216f, 13.6863f,
                     -10.2647f, 6.8431f,   -17.1078f, 0.0f,     17.1078f, -6.8431f };

        case Temperament::werckmeister3:
            // Werckmeister III (Andreas Werckmeister, 1691), anchored at A=0:
            // C: +11.73, C#: +1.96, D: +3.91, D#: +5.87, E: +1.96, F: +9.78,
            // F#: 0.0, G: +7.82, G#: +3.91, A: 0.0, A#: +7.82, B: +3.91
            return { 11.7300f, 1.9550f, 3.9100f, 5.8650f, 1.9550f, 9.7750f,
                     0.0f,     7.8200f, 3.9100f, 0.0f,    7.8200f, 3.9100f };

        case Temperament::kirnberger3:
            // Kirnberger III (Johann Philipp Kirnberger, 1779), anchored at A=0:
            // C: +10.265, C#: +0.978, D: +3.422, D#: +4.400, E: -2.933, F: +8.314,
            // F#: +0.978, G: +6.843, G#: +1.955, A: 0.0, A#: +5.865, B: -1.955
            return { 10.2647f, 0.9775f, 3.4216f, 4.4000f, -2.9325f, 8.3137f,
                     0.9775f,  6.8431f, 1.9550f, 0.0f,    5.8650f,  -1.9550f };
        }
        return { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    }

    /**
     * Calculates the exact frequency in Hertz for any given MIDI note number.
     *
     * @param midiNoteNumber The MIDI note index [0, 127]. Clamped safely if out of range.
     * @param temperament The selected historical or modern temperament.
     * @param referencePitchA4 Reference frequency for note A4 (MIDI 69), defaults to 440.0 Hz.
     * @return Fundamental frequency f0 in Hertz.
     */
    [[nodiscard]] static double getFrequency(int midiNoteNumber, Temperament temperament = Temperament::equal,
                                             double referencePitchA4 = kDefaultReferencePitch) noexcept {
        const auto note = std::clamp(midiNoteNumber, 0, 127);
        const auto refPitch = clampReferencePitch(referencePitchA4);

        // 12-TET nominal frequency: f_nom = A4 * 2^((note - 69) / 12)
        const auto semitonesFromA4 = static_cast<double>(note - 69);
        const auto nominalFrequency = refPitch * std::pow(2.0, semitonesFromA4 / 12.0);

        if (temperament == Temperament::equal) {
            return nominalFrequency;
        }

        const auto pitchClass = getPitchClass(note);
        const auto centOffsets = getCentOffsets(temperament);
        const auto cents = static_cast<double>(centOffsets[static_cast<std::size_t>(pitchClass)]);

        // Frequency with cent deviation: f = f_nom * 2^(cents / 1200.0)
        return nominalFrequency * std::pow(2.0, cents / 1200.0);
    }

    /**
     * Returns the programmatic identifier for serialisation (e.g. "equal", "werckmeister3").
     */
    [[nodiscard]] static constexpr std::string_view getIdentifier(Temperament temperament) noexcept {
        switch (temperament) {
        case Temperament::equal:
            return "equal";
        case Temperament::just:
            return "just";
        case Temperament::pythagorean:
            return "pythagorean";
        case Temperament::meantone:
            return "meantone";
        case Temperament::werckmeister3:
            return "werckmeister3";
        case Temperament::kirnberger3:
            return "kirnberger3";
        }
        return "equal";
    }

    /**
     * Resolves a temperament enum from string identifier, safe fallback to equal.
     */
    [[nodiscard]] static constexpr Temperament fromIdentifier(std::string_view id) noexcept {
        if (id == "just") {
            return Temperament::just;
        }
        if (id == "pythagorean") {
            return Temperament::pythagorean;
        }
        if (id == "meantone") {
            return Temperament::meantone;
        }
        if (id == "werckmeister3") {
            return Temperament::werckmeister3;
        }
        if (id == "kirnberger3") {
            return Temperament::kirnberger3;
        }
        return Temperament::equal;
    }

    /**
     * Returns the human-readable English name.
     */
    [[nodiscard]] static constexpr std::string_view getDisplayName(Temperament temperament) noexcept {
        switch (temperament) {
        case Temperament::equal:
            return "Equal (12-EDO)";
        case Temperament::just:
            return "Just Intonation";
        case Temperament::pythagorean:
            return "Pythagorean";
        case Temperament::meantone:
            return "Meantone (1/4 comma)";
        case Temperament::werckmeister3:
            return "Werckmeister III";
        case Temperament::kirnberger3:
            return "Kirnberger III";
        }
        return "Equal (12-EDO)";
    }
};

} // namespace devpiano::audio

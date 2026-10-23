#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PianoSynthVoice.h"
#include "Audio/SineSynthVoice.h"
#include "Recording/RenderPipeline.h"

#include <array>

// ==============================================================================
// R5 whole-piano pedal acoustic tests.
// Pedal sound is owned by BuiltinSynthesiser, not by a PianoSynthVoice slot.
// ==============================================================================
class PedalAcousticsTest final : public juce::UnitTest {
public:
    PedalAcousticsTest()
        : juce::UnitTest("PedalAcoustics", "DevPiano/Audio") {
    }

    void runTest() override {
        testIdlePedalDoesNotOccupyVoice();
        testDoubleBufferRendersOwner();
        testEightNotesUnderPedal();
        testStealDoesNotTruncateOwner();
        testNoteOnDoesNotRetrigger();
        testMultiChannelAggregationAndSequence();
        testSameSampleOrdering();
        testCc120Scope();
        testCc121Scope();
        testCc123PreservesPedal();
        testPanicResetsAndRetriggers();
        testSineDoesNotGetPedalAcoustics();
        testMonoAndFiniteOutput();
        testSampleRateReprepare();
        testNewVoiceAfterPedalDown();
        testExistingVoicePedalUp();
        testChannelIsolation();
        testVoiceReuseAcrossPedalStates();
    }

private:
    struct Fixture {
        static constexpr int kVoiceCount = 8;
        devpiano::audio::BuiltinSynthesiser synth;
        std::array<PianoSynthVoice*, kVoiceCount> voices {};
        juce::AudioBuffer<float> buffer { 2, 512 };

        explicit Fixture(double sampleRate = 48000.0) {
            synth.setCurrentPlaybackSampleRate(sampleRate);
            synth.addSound(new PianoSynthSound());
            for (int i = 0; i < kVoiceCount; ++i) {
                auto* voice = new PianoSynthVoice();
                voice->setVoiceIndex(i);
                synth.addVoice(voice);
                voices[static_cast<std::size_t>(i)] = voice;
            }
            synth.setPedalAcousticsEnabled(true);
            synth.setPedalNoiseLevel(0.8f);
        }

        void render(int samples = 512, const juce::MidiBuffer& midi = {}) {
            buffer.clear();
            synth.renderNextBlock(buffer, midi, 0, samples);
        }
    };

    static float peak(const juce::AudioBuffer<float>& buffer, int channel = 0) {
        return buffer.getMagnitude(channel, 0, buffer.getNumSamples());
    }

    static int activeVoiceCount(const Fixture& fixture) {
        auto count = 0;
        for (const auto* voice : fixture.voices) {
            if (voice->isVoiceActive()) {
                ++count;
            }
        }
        return count;
    }

    static void down(devpiano::audio::BuiltinSynthesiser& synth, int channel, int value = 127) {
        synth.handleController(channel, 64, value);
    }

    static void up(devpiano::audio::BuiltinSynthesiser& synth, int channel) {
        synth.handleController(channel, 64, 0);
    }

    void testIdlePedalDoesNotOccupyVoice() {
        beginTest("Idle pedal renders without occupying a PianoSynthVoice");
        Fixture f;
        down(f.synth, 1);
        expect(f.synth.isPedalAcousticsEnabled());
        expect(f.synth.isPedalAcousticsActive());
        expect(f.synth.getPedalAcoustics().shockTail.isShockActive());
        expect(!f.voices[0]->sympatheticPool.isShockActive(), "voice note pool must not receive the pedal shock");
        expectEquals(activeVoiceCount(f), 0, "pedal must not mark a note voice active");

        f.render();
        expectGreaterThan(peak(f.buffer, 0), 0.001f);
        expectGreaterThan(peak(f.buffer, 1), 0.001f);

        auto remaining = 0;
        while (f.synth.isPedalAcousticsActive() && remaining < 48000 * 2) {
            f.render();
            remaining += 512;
        }
        expect(!f.synth.isPedalAcousticsActive(), "pedal tail must reach its bounded end");
        f.render();
        expectEquals(peak(f.buffer, 0), 0.0f);
    }

    void testDoubleBufferRendersOwner() {
        beginTest("Double-buffer render advances the pedal owner");
        devpiano::audio::BuiltinSynthesiser synth;
        synth.setCurrentPlaybackSampleRate(48000.0);
        synth.setPedalAcousticsEnabled(true);
        synth.setPedalNoiseLevel(0.8f);
        down(synth, 1);

        juce::AudioBuffer<double> buffer(2, 512);
        buffer.clear();
        synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, 512);
        expectGreaterThan(buffer.getMagnitude(0, 0, 512), 0.001);

        auto blocks = 1;
        while (synth.isPedalAcousticsActive() && blocks < 4096) {
            buffer.clear();
            synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, 512);
            ++blocks;
        }
        expect(!synth.isPedalAcousticsActive(), "double-buffer render must drain the owner tail");
    }

    void testEightNotesUnderPedal() {
        beginTest("Eight different notes consume all eight note voices under pedal");
        Fixture f;
        down(f.synth, 1);
        for (int note = 60; note < 68; ++note) {
            f.synth.noteOn(1, note, 0.8f);
        }
        expectEquals(activeVoiceCount(f), Fixture::kVoiceCount);
        expect(f.synth.isPedalAcousticsActive());
        f.render();
        expectGreaterThan(peak(f.buffer, 0), 0.01f);
    }

    void testStealDoesNotTruncateOwner() {
        beginTest("Voice-zero reuse during a pedal tail does not truncate the owner");
        Fixture f;
        down(f.synth, 1);
        f.render();
        expect(f.synth.isPedalAcousticsActive());

        for (int note = 60; note < 68; ++note) {
            f.synth.noteOn(1, note, 0.8f);
            f.render();
        }

        // Request the existing low pitch so the same-note path must reuse voice
        // zero even while the sustain pedal keeps all voices active.
        const auto previousTrigger = f.voices[0]->triggerCounter;
        f.synth.noteOn(1, 60, 0.8f);
        f.render();
        expectGreaterThan(static_cast<juce::uint64>(f.voices[0]->triggerCounter),
                          static_cast<juce::uint64>(previousTrigger), "voice zero must be reused");
        expectEquals(activeVoiceCount(f), Fixture::kVoiceCount);
        expect(f.synth.isPedalAcousticsActive(), "owner tail must outlive voice-zero reuse");
    }

    void testNoteOnDoesNotRetrigger() {
        beginTest("NoteOn while pedal is down does not retrigger the mechanical owner");
        Fixture f;
        down(f.synth, 1);
        f.render(128);
        const auto before = f.synth.getPedalAcoustics().mechanical.whooshSamplesRemaining;
        f.synth.noteOn(1, 60, 0.8f);
        const auto after = f.synth.getPedalAcoustics().mechanical.whooshSamplesRemaining;
        expect(after <= before, "NoteOn must not restart the pedal whoosh");
        expect(f.synth.isPedalAcousticsActive());
    }

    void testMultiChannelAggregationAndSequence() {
        beginTest("Multi-channel pedal aggregation preserves the final channel state");
        Fixture f;
        down(f.synth, 1, 100);
        down(f.synth, 2, 127);
        expect(f.synth.isSustainPedalDown(1));
        expect(f.synth.isSustainPedalDown(2));
        f.render();
        expect(f.synth.isPedalAcousticsActive());

        up(f.synth, 1);
        down(f.synth, 1, 127);
        up(f.synth, 2);
        expect(f.synth.isSustainPedalDown(1), "ch1 remains down after ch2 up");
        expect(!f.synth.isSustainPedalDown(2));
        expect(f.synth.isPedalAcousticsActive(), "aggregate down must keep the owner alive");

        up(f.synth, 1);
        expect(!f.synth.isSustainPedalDown(1));
        expect(f.synth.isPedalAcousticsActive(), "final up must produce a release tail");
    }

    void testSameSampleOrdering() {
        beginTest("Same-sample down/up/down is consumed in order, not deduplicated");
        Fixture ordered;
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        ordered.render(512, midi);
        expect(ordered.synth.isSustainPedalDown(1));
        expect(ordered.synth.isPedalAcousticsActive());

        Fixture sequential;
        down(sequential.synth, 1);
        up(sequential.synth, 1);
        down(sequential.synth, 1);
        sequential.render(512);

        auto maxDifference = 0.0f;
        for (int channel = 0; channel < 2; ++channel) {
            for (int sample = 0; sample < 512; ++sample) {
                maxDifference = std::max(
                    maxDifference,
                    std::abs(ordered.buffer.getSample(channel, sample) - sequential.buffer.getSample(channel, sample)));
            }
        }
        expectLessThan(maxDifference, 1e-7f, "same-sample events must match sequential dispatch");
    }

    void testCc120Scope() {
        beginTest("CC120 clears only its target channel and resets owner only when aggregate is up");
        Fixture f;
        down(f.synth, 1);
        down(f.synth, 2);
        f.synth.handleController(1, 120, 0);
        expect(!f.synth.isSustainPedalDown(1));
        expect(f.synth.isSustainPedalDown(2));
        expect(f.synth.isPedalAcousticsActive());

        f.synth.handleController(2, 120, 0);
        expect(!f.synth.isSustainPedalDown(1));
        expect(!f.synth.isSustainPedalDown(2));
        expect(!f.synth.isPedalAcousticsActive(), "CC120 on last down channel must hard-clear the tail");

        down(f.synth, 1);
        expect(f.synth.isPedalAcousticsActive(), "same down value must trigger after a hard reset");
    }

    void testCc121Scope() {
        beginTest("CC121 resets its target channel and emits one release when aggregate falls");
        Fixture f;
        down(f.synth, 1);
        down(f.synth, 2);
        f.synth.handleController(1, 121, 0);
        expect(!f.synth.isSustainPedalDown(1));
        expect(f.synth.isSustainPedalDown(2));
        expect(f.synth.isPedalAcousticsActive());

        up(f.synth, 2);
        expect(!f.synth.isSustainPedalDown(1));
        expect(!f.synth.isSustainPedalDown(2));
        expect(f.synth.isPedalAcousticsActive(), "release tail must remain after final up");
    }

    void testCc123PreservesPedal() {
        beginTest("CC123 releases notes but preserves pedal history and owner tail");
        Fixture f;
        down(f.synth, 1);
        f.synth.noteOn(1, 60, 0.8f);
        f.render();
        expect(f.synth.isSustainPedalDown(1));
        f.synth.handleController(1, 123, 0);
        expect(f.synth.isSustainPedalDown(1), "CC123 must not clear sustain history");
        expect(f.synth.isPedalAcousticsActive(), "CC123 must not flush the owner tail");
    }

    void testPanicResetsAndRetriggers() {
        beginTest("Panic clears owner/control state and a repeated down triggers again");
        Fixture f;
        down(f.synth, 1);
        f.render();
        juce::MidiBuffer panic;
        devpiano::recording::addPanicMidi(panic, 0);
        f.render(512, panic);
        expect(!f.synth.isPedalAcousticsActive());
        expect(!f.synth.isSustainPedalDown(1));

        down(f.synth, 1);
        expect(f.synth.isSustainPedalDown(1));
        expect(f.synth.isPedalAcousticsActive());
    }

    void testSineDoesNotGetPedalAcoustics() {
        beginTest("Sine synthesizer does not receive Piano pedal acoustics");
        devpiano::audio::BuiltinSynthesiser sine;
        sine.setCurrentPlaybackSampleRate(48000.0);
        sine.addSound(new SineSynthSound());
        sine.addVoice(new SineSynthVoice());
        expect(!sine.isPedalAcousticsEnabled());
        sine.handleController(1, 64, 127);
        expect(!sine.isPedalAcousticsActive());
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        sine.renderNextBlock(buffer, {}, 0, 512);
        expectEquals(peak(buffer, 0), 0.0f);
    }

    void testSampleRateReprepare() {
        beginTest("Short pedal tail closes at 48/44.1/96 kHz and reprepare clears it");
        for (const auto rate : { 48000.0, 44100.0, 96000.0 }) {
            beginTest("Rate " + juce::String(rate));
            Fixture f(rate);
            down(f.synth, 1);
            expect(f.synth.isPedalAcousticsActive());
            auto blocks = 0;
            while (f.synth.isPedalAcousticsActive() && blocks < 4096) {
                f.render();
                ++blocks;
            }
            expect(!f.synth.isPedalAcousticsActive(), "bounded tail must close at " + juce::String(rate));

            f.synth.setCurrentPlaybackSampleRate(rate == 44100.0 ? 48000.0 : 44100.0);
            expect(!f.synth.isPedalAcousticsActive(), "reprepare must clear old tail");
            expect(!f.synth.isSustainPedalDown(1), "reprepare must clear channel pedal truth");
            down(f.synth, 1);
            expect(f.synth.isPedalAcousticsActive(), "same value must retrigger after reprepare");
        }
    }

    static PianoSynthVoice* findVoiceForNote(Fixture& fixture, int note, int channel) {
        for (auto* voice : fixture.voices) {
            if (voice != nullptr && voice->getCurrentlyPlayingNote() == note && voice->isPlayingChannel(channel)) {
                return voice;
            }
        }
        return nullptr;
    }

    void testNewVoiceAfterPedalDown() {
        beginTest("New voice after CC64 down inherits channel sustain before first audio");
        Fixture f;
        down(f.synth, 1);
        f.synth.noteOn(1, 60, 0.8f);
        auto* voice = findVoiceForNote(f, 60, 1);
        expect(voice != nullptr);
        if (voice != nullptr) {
            expect(voice->isSustainPedalDown(), "JUCE voice sustain flag must inherit ch1");
            expect(voice->sympatheticPool.pedalDown, "note-driven pool must inherit ch1 before rendering");
        }
        expectEquals(activeVoiceCount(f), 1);
    }

    void testExistingVoicePedalUp() {
        beginTest("Existing voice still follows CC64 down/up after the initialization fix");
        Fixture f;
        f.synth.noteOn(1, 60, 0.8f);
        down(f.synth, 1);
        auto* voice = findVoiceForNote(f, 60, 1);
        expect(voice != nullptr);
        if (voice != nullptr) {
            expect(voice->isSustainPedalDown());
            expect(voice->sympatheticPool.pedalDown);
        }
        up(f.synth, 1);
        if (voice != nullptr) {
            expect(!voice->isSustainPedalDown());
            expect(!voice->sympatheticPool.pedalDown);
        }
    }

    void testChannelIsolation() {
        beginTest("New notes inherit their own channel sustain state, not the aggregate");
        Fixture f;
        down(f.synth, 1);
        up(f.synth, 2);
        f.synth.noteOn(1, 60, 0.8f);
        f.synth.noteOn(2, 61, 0.8f);
        auto* ch1 = findVoiceForNote(f, 60, 1);
        auto* ch2 = findVoiceForNote(f, 61, 2);
        expect(ch1 != nullptr);
        expect(ch2 != nullptr);
        if (ch1 != nullptr) {
            expect(ch1->isSustainPedalDown());
            expect(ch1->sympatheticPool.pedalDown);
        }
        if (ch2 != nullptr) {
            expect(!ch2->isSustainPedalDown());
            expect(!ch2->sympatheticPool.pedalDown);
        }
    }

    void testVoiceReuseAcrossPedalStates() {
        beginTest("Voice reuse across pedal states does not retain the previous channel state");
        for (const auto fromChannel : { 1, 2 }) {
            for (const auto toChannel : { 1, 2 }) {
                devpiano::audio::BuiltinSynthesiser synth;
                synth.setCurrentPlaybackSampleRate(48000.0);
                synth.addSound(new PianoSynthSound());
                auto* voice = new PianoSynthVoice();
                synth.addVoice(voice);
                down(synth, 1);
                if (fromChannel == 2) {
                    up(synth, 2);
                } else {
                    down(synth, 2);
                }
                if (toChannel == 1) {
                    down(synth, 1);
                } else {
                    up(synth, 1);
                }
                // A single voice makes the steal source and target unambiguous.
                synth.noteOn(fromChannel, 60, 0.8f);
                expect(voice->getCurrentlyPlayingNote() == 60);
                synth.noteOn(toChannel, 61, 0.8f);
                expect(voice->getCurrentlyPlayingNote() == 61, "the single voice must be reused");
                expect(voice->isPlayingChannel(toChannel));
                expect(voice->isSustainPedalDown() == synth.isSustainPedalDown(toChannel),
                       "voice sustain flag must match the target channel");
                expect(voice->sympatheticPool.pedalDown == synth.isSustainPedalDown(toChannel),
                       "voice pool pedal state must match the target channel");
            }
        }
    }

    void testMonoAndFiniteOutput() {
        beginTest("Pedal owner output is finite and mono downmix is bounded");
        Fixture f;
        down(f.synth, 1);
        juce::AudioBuffer<float> mono(1, 512);
        mono.clear();
        f.synth.renderNextBlock(mono, {}, 0, 512);
        expectGreaterThan(peak(mono, 0), 0.0f);
        expectLessThan(peak(mono, 0), 1.5f);
        for (int i = 0; i < 512; ++i) {
            expect(std::isfinite(mono.getSample(0, i)));
        }
    }
};

static PedalAcousticsTest pedalAcousticsTest;

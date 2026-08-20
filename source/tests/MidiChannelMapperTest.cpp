#include <JuceHeader.h>

#include "Midi/MidiChannelMapper.h"

using namespace devpiano::midi;

// =============================================================================
// Tests for the matrix-aware MIDI routing service:
//   - default matrix passes through (applyTransform; sendNoteOn/Off)
//   - melodic channels follow key by default, Ch10 drum bypasses by default
//   - per-channel followKey override works on both melodic and drum channels
//   - applyMatrixToNoteOn/Off channel selection (outputChannel remap)
//   - transpose + octaveShift boundary clamping (note + keySignature overflow)
//   - note-on / note-off symmetric transformation
//   - non-note messages pass through unchanged
// =============================================================================

class MidiChannelMapperTest final : public juce::UnitTest {
public:
    MidiChannelMapperTest()
        : juce::UnitTest("MidiChannelMapper: matrix routing and transpose", "DevPiano/Core") {
    }

    void runTest() override {
        testDefaultPassThroughWithoutTranspose();
        testDefaultMatrixWithGlobalTranspose();
        testOutputChannelRemap();
        testTransposeAndOctaveClamping();
        testVelocityOverride();
        testFollowKeyWithGlobalTranspose();
        testNoteOnOffSymmetry();
        testNonNoteMessagesPassThrough();
        testOutOfRangeInputChannelClamps();
        testKeyboardStateReceivesTransformedNotes();
    }

private:
    void testDefaultPassThroughWithoutTranspose() {
        testCase("default matrix without transpose: applyTransform returns the message unchanged", [&] {
            ChannelMatrix matrix;
            const bool midiTranspose = false;
            const int keySignature = 0;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            const auto in = juce::MidiMessage::noteOn(3, 60, 0.5f);
            const auto out = mapper.applyTransform(in);
            expectEquals(out.getChannel(), 3);
            expectEquals(out.getNoteNumber(), 60);
            expectWithinAbsoluteError(out.getFloatVelocity(), 0.5f, 0.01f);
        });
    }

    void testDefaultMatrixWithGlobalTranspose() {
        testCase("default matrix with transpose: melodic channels transpose, Ch10 drum bypasses", [&] {
            ChannelMatrix matrix; // Ch1..9 and Ch11..16 followKey=true, Ch10 followKey=false
            const bool midiTranspose = true;
            const int keySignature = 7;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            juce::MidiKeyboardState ks;
            // Channel 1 (index 0): melodic -> 60 + 7 = 67
            mapper.sendNoteOn(0, 60, 0.8f, ks);
            expect(ks.isNoteOn(1, 67), "melodic channel 1 must transpose 60 -> 67");
            expect(!ks.isNoteOn(1, 60));

            // Channel 10 (index 9): drum -> stays at 36
            mapper.sendNoteOn(9, 36, 0.8f, ks);
            expect(ks.isNoteOn(10, 36), "drum channel 10 must bypass transpose and stay at 36");
            expect(!ks.isNoteOn(10, 43));
        });

        testCase("default matrix without transpose keeps the original note", [&] {
            ChannelMatrix matrix;
            const bool midiTranspose = false;
            const int keySignature = 7;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            juce::MidiKeyboardState ks;
            mapper.sendNoteOn(4, 60, 0.8f, ks);
            expect(ks.isNoteOn(5, 60), "note-on must land on the original channel");
            mapper.sendNoteOff(4, 60, 0.0f, ks);
            expect(!ks.isNoteOn(5, 60), "note-off must release the same original note");
        });
    }

    void testOutputChannelRemap() {
        testCase("active matrix: outputChannel remaps the note-on channel", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[2].outputChannel = 7; // input channel 3 → output channel 8

            const bool midiTranspose = false;
            const int keySignature = 0;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            const auto out = mapper.applyTransform(juce::MidiMessage::noteOn(3, 60, 0.5f));
            expectEquals(out.getChannel(), 8, "output channel must be remapped");
            expectEquals(out.getNoteNumber(), 60, "note must be unchanged");
        });

        testCase("active matrix: note-off uses the same remapped channel", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].outputChannel = 3;

            const bool midiTranspose = false;
            const int keySignature = 0;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            const auto out = mapper.applyTransform(juce::MidiMessage::noteOff(1, 60));
            expectEquals(out.getChannel(), 4);
            expectEquals(out.getNoteNumber(), 60);
        });
    }

    void testTransposeAndOctaveClamping() {
        testCase("transpose + octaveShift add up", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].transpose = 12;
            matrix.channels[0].octaveShift = 1; // +12 semitones each → +24 total

            const bool midiTranspose = false;
            const int keySignature = 0;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            const auto out = mapper.applyTransform(juce::MidiMessage::noteOn(1, 60, 0.5f));
            expectEquals(out.getNoteNumber(), 84);
        });

        testCase("note above 127 clamps to 127", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].transpose = 48;

            const auto out = applyMatrixToNoteOn(matrix.channels[0], 120, 0.5f);
            expectEquals(out.getNoteNumber(), 127, "transposed note must clamp to 127");
        });

        testCase("note below 0 clamps to 0", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].transpose = -48;

            const auto out = applyMatrixToNoteOff(matrix.channels[0], 10, 0.5f);
            expectEquals(out.getNoteNumber(), 0, "transposed note must clamp to 0");
        });

        testCase("keySignature overflow clamps inside sendNoteOn", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].followKey = true;

            const bool midiTranspose = true;
            const int keySignature = 48;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            juce::MidiKeyboardState ks;
            mapper.sendNoteOn(0, 120, 0.8f, ks);
            expect(ks.isNoteOn(1, 127), "followKey + transpose must clamp to 127");
        });
    }

    void testVelocityOverride() {
        testCase("velocity field overrides the original velocity", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].velocity = 100;

            const auto out = applyMatrixToNoteOn(matrix.channels[0], 60, 0.5f);
            expectEquals(static_cast<int>(out.getVelocity()), 100);
        });

        testCase("velocity 64 means no override", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].velocity = 64; // default = use original

            const auto out = applyMatrixToNoteOn(matrix.channels[0], 60, 0.5f);
            expectEquals(static_cast<int>(out.getVelocity()), 63, "0.5 * 127 truncates to 63");
        });
    }

    void testFollowKeyWithGlobalTranspose() {
        testCase("followKey channel applies the global transpose", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].outputChannel = 2;
            matrix.channels[0].followKey = true;

            const bool midiTranspose = true;
            const int keySignature = 7;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            juce::MidiKeyboardState ks;
            mapper.sendNoteOn(0, 60, 0.8f, ks);
            expect(ks.isNoteOn(3, 67), "followKey must transpose note 60 → 67 on the remapped channel");
        });

        testCase("non-followKey channel ignores the global transpose", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].followKey = false;

            const bool midiTranspose = true;
            const int keySignature = 7;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            juce::MidiKeyboardState ks;
            mapper.sendNoteOn(0, 60, 0.8f, ks);
            expect(ks.isNoteOn(1, 60), "non-followKey must keep the note");
        });
    }

    void testNoteOnOffSymmetry() {
        testCase("note-on and note-off produce the same channel/note", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[4].outputChannel = 9;
            matrix.channels[4].transpose = 2;

            const auto on = applyMatrixToNoteOn(matrix.channels[4], 60, 0.5f);
            const auto off = applyMatrixToNoteOff(matrix.channels[4], 60, 0.5f);
            expectEquals(on.getChannel(), off.getChannel(), "on/off must share the output channel");
            expectEquals(on.getNoteNumber(), off.getNoteNumber(), "on/off must share the output note");
        });
    }

    void testNonNoteMessagesPassThrough() {
        testCase("CC messages pass through unchanged when active", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].outputChannel = 5;

            const bool midiTranspose = false;
            const int keySignature = 0;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            const auto cc = juce::MidiMessage::controllerEvent(2, 64, 100);
            const auto out = mapper.applyTransform(cc);
            expectEquals(out.getChannel(), 2);
            expectEquals(out.getControllerNumber(), 64);
        });
    }

    void testOutOfRangeInputChannelClamps() {
        testCase("input channel beyond 15 clamps to channel 15 config", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[15].outputChannel = 3;

            const bool midiTranspose = false;
            const int keySignature = 0;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            juce::MidiKeyboardState ks;
            mapper.sendNoteOn(20, 60, 0.8f, ks); // invalid input channel
            expect(ks.isNoteOn(4, 60), "out-of-range input must clamp to the last channel config");
        });
    }

    void testKeyboardStateReceivesTransformedNotes() {
        testCase("sendNoteOn then sendNoteOff drives the keyboard state", [&] {
            ChannelMatrix matrix;
            matrix.active = true;
            matrix.channels[0].outputChannel = 1;

            const bool midiTranspose = false;
            const int keySignature = 0;
            MidiChannelMapper mapper(matrix, midiTranspose, keySignature);

            juce::MidiKeyboardState ks;
            mapper.sendNoteOn(0, 64, 0.8f, ks);
            expect(ks.isNoteOn(2, 64), "note-on must reach the keyboard state on the remapped channel");
            mapper.sendNoteOff(0, 64, 0.0f, ks);
            expect(!ks.isNoteOn(2, 64), "note-off must release the note");
        });
    }
};

static MidiChannelMapperTest midiChannelMapperTest;

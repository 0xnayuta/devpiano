#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"
#include "Core/MidiTypes.h"

// =============================================================================
// Tests for core data types and keyboard layout functions
// =============================================================================

class MidiNoteNumberTest : public juce::UnitTest {
public:
    MidiNoteNumberTest()
        : juce::UnitTest("MidiNoteNumber", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("fromClamped clamps below minimum", [&] {
            auto n = devpiano::core::MidiNoteNumber::fromClamped(-42);
            expectEquals(n.value, 0);
        });

        testCase("fromClamped clamps above maximum", [&] {
            auto n = devpiano::core::MidiNoteNumber::fromClamped(200);
            expectEquals(n.value, 127);
        });

        testCase("fromClamped preserves in-range", [&] {
            expectEquals(devpiano::core::MidiNoteNumber::fromClamped(60).value, 60);
            expectEquals(devpiano::core::MidiNoteNumber::fromClamped(0).value, 0);
            expectEquals(devpiano::core::MidiNoteNumber::fromClamped(127).value, 127);
        });

        testCase("isValid", [&] {
            expect(devpiano::core::MidiNoteNumber { 0 }.isValid());
            expect(devpiano::core::MidiNoteNumber { 60 }.isValid());
            expect(devpiano::core::MidiNoteNumber { 127 }.isValid());
            expect(!devpiano::core::MidiNoteNumber { -1 }.isValid());
            expect(!devpiano::core::MidiNoteNumber { 128 }.isValid());
        });

        testCase("min/max are correct", [&] {
            expectEquals(devpiano::core::MidiNoteNumber::minValue(), 0);
            expectEquals(devpiano::core::MidiNoteNumber::maxValue(), 127);
        });
    }
};

static MidiNoteNumberTest midiNoteNumberTest;

// =============================================================================

class MidiChannelTest : public juce::UnitTest {
public:
    MidiChannelTest()
        : juce::UnitTest("MidiChannel", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("fromClamped clamps below minimum", [&] {
            auto ch = devpiano::core::MidiChannel::fromClamped(0);
            expectEquals(ch.value, 1);
        });

        testCase("fromClamped clamps above maximum", [&] {
            auto ch = devpiano::core::MidiChannel::fromClamped(99);
            expectEquals(ch.value, 16);
        });

        testCase("fromClamped preserves in-range", [&] {
            expectEquals(devpiano::core::MidiChannel::fromClamped(1).value, 1);
            expectEquals(devpiano::core::MidiChannel::fromClamped(10).value, 10);
            expectEquals(devpiano::core::MidiChannel::fromClamped(16).value, 16);
        });

        testCase("toZeroBased", [&] {
            expectEquals(devpiano::core::MidiChannel::fromClamped(1).toZeroBased(), 0);
            expectEquals(devpiano::core::MidiChannel::fromClamped(10).toZeroBased(), 9);
            expectEquals(devpiano::core::MidiChannel::fromClamped(16).toZeroBased(), 15);
        });

        testCase("isValid", [&] {
            expect(devpiano::core::MidiChannel { 1 }.isValid());
            expect(devpiano::core::MidiChannel { 16 }.isValid());
            expect(!devpiano::core::MidiChannel { 0 }.isValid());
            expect(!devpiano::core::MidiChannel { 17 }.isValid());
        });
    }
};

static MidiChannelTest midiChannelTest;

// =============================================================================

class VelocityTest : public juce::UnitTest {
public:
    VelocityTest()
        : juce::UnitTest("Velocity", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("fromClamped clamps below minimum", [&] {
            auto v = devpiano::core::Velocity::fromClamped(-0.5f);
            expectEquals(v.value, 0.0f);
        });

        testCase("fromClamped clamps above maximum", [&] {
            auto v = devpiano::core::Velocity::fromClamped(1.5f);
            expectEquals(v.value, 1.0f);
        });

        testCase("fromClamped preserves in-range", [&] {
            expectEquals(devpiano::core::Velocity::fromClamped(0.0f).value, 0.0f);
            expectEquals(devpiano::core::Velocity::fromClamped(0.5f).value, 0.5f);
            expectEquals(devpiano::core::Velocity::fromClamped(1.0f).value, 1.0f);
        });

        testCase("toMidiByte", [&] {
            expectEquals(devpiano::core::Velocity::fromClamped(0.0f).toMidiByte(), juce::uint8(0));
            expectEquals(devpiano::core::Velocity::fromClamped(0.5f).toMidiByte(), juce::uint8(63));
            expectEquals(devpiano::core::Velocity::fromClamped(1.0f).toMidiByte(), juce::uint8(127));
        });

        testCase("isValid", [&] {
            expect(devpiano::core::Velocity { 0.0f }.isValid());
            expect(devpiano::core::Velocity { 0.5f }.isValid());
            expect(devpiano::core::Velocity { 1.0f }.isValid());
            expect(!devpiano::core::Velocity { -0.01f }.isValid());
            expect(!devpiano::core::Velocity { 1.01f }.isValid());
        });
    }
};

static VelocityTest velocityTest;

// =============================================================================

class NoteRangeTest : public juce::UnitTest {
public:
    NoteRangeTest()
        : juce::UnitTest("NoteRange", "DevPiano/Core") {
    }

    void runTest() override {
        using devpiano::core::MidiNoteNumber;
        using devpiano::core::NoteRange;

        testCase("contains works correctly", [&] {
            NoteRange range { MidiNoteNumber::fromClamped(36), MidiNoteNumber::fromClamped(96) };
            expect(range.contains(MidiNoteNumber::fromClamped(60)));
            expect(range.contains(MidiNoteNumber::fromClamped(36)));
            expect(range.contains(MidiNoteNumber::fromClamped(96)));
            expect(!range.contains(MidiNoteNumber::fromClamped(35)));
            expect(!range.contains(MidiNoteNumber::fromClamped(97)));
        });

        testCase("isValid rejects reversed range", [&] {
            NoteRange reversed { MidiNoteNumber::fromClamped(96), MidiNoteNumber::fromClamped(36) };
            expect(!reversed.isValid());
        });

        testCase("isValid accepts equal bounds", [&] {
            NoteRange single { MidiNoteNumber::fromClamped(60), MidiNoteNumber::fromClamped(60) };
            expect(single.isValid());
            expect(single.contains(MidiNoteNumber::fromClamped(60)));
            expect(!single.contains(MidiNoteNumber::fromClamped(61)));
        });

        testCase("full range is valid", [&] {
            NoteRange full { MidiNoteNumber::fromClamped(0), MidiNoteNumber::fromClamped(127) };
            expect(full.isValid());
            expect(full.contains(MidiNoteNumber::fromClamped(0)));
            expect(full.contains(MidiNoteNumber::fromClamped(127)));
        });
    }
};

static NoteRangeTest noteRangeTest;

// =============================================================================

class KeyActionTest : public juce::UnitTest {
public:
    KeyActionTest()
        : juce::UnitTest("KeyAction", "DevPiano/Core") {
    }

    void runTest() override {
        using devpiano::core::KeyAction;
        using devpiano::core::MidiChannel;
        using devpiano::core::MidiNoteNumber;
        using devpiano::core::Velocity;

        testCase("default construction has sensible defaults", [&] {
            KeyAction action;
            expectEquals(action.midiNote, 60);
            expectEquals(action.midiChannel, 1);
            expectEquals(action.velocity, 1.0f);
        });

        testCase("getMidiNoteNumber returns clamped", [&] {
            KeyAction action;
            action.midiNote = 200;
            auto note = action.getMidiNoteNumber();
            expectEquals(note.value, 127);
        });

        testCase("setMidiNoteNumber round-trips", [&] {
            KeyAction action;
            action.setMidiNoteNumber(MidiNoteNumber::fromClamped(42));
            expectEquals(action.midiNote, 42);
            expectEquals(action.getMidiNoteNumber().value, 42);
        });

        testCase("setMidiChannel round-trips", [&] {
            KeyAction action;
            action.setMidiChannel(MidiChannel::fromClamped(5));
            expectEquals(action.midiChannel, 5);
            expectEquals(action.getMidiChannel().value, 5);
        });

        testCase("setVelocity round-trips", [&] {
            KeyAction action;
            action.setVelocity(Velocity::fromClamped(0.75f));
            expectEquals(action.velocity, 0.75f);
            expectEquals(action.getVelocity().value, 0.75f);
        });
    }
};

static KeyActionTest keyActionTest;

// =============================================================================

class KeyBindingTest : public juce::UnitTest {
public:
    KeyBindingTest()
        : juce::UnitTest("KeyBinding", "DevPiano/Core") {
    }

    void runTest() override {
        using devpiano::core::KeyActionType;
        using devpiano::core::KeyBinding;
        using devpiano::core::KeyboardLayout;
        using devpiano::core::KeyTrigger;
        using devpiano::core::makeAlphaNumericKeyCode;
        using devpiano::core::makeNoteBinding;
        using devpiano::core::normaliseAlphaNumericKeyCode;

        testCase("makeAlphaNumericKeyCode normalises to uppercase",
                 [&] { expectEquals(makeAlphaNumericKeyCode('a'), makeAlphaNumericKeyCode('A')); });

        testCase("normaliseAlphaNumericKeyCode returns 0 for non-alphanumeric", [&] {
            expectEquals(normaliseAlphaNumericKeyCode(0), 0);
            expectEquals(normaliseAlphaNumericKeyCode(' '), 0);
            expectEquals(normaliseAlphaNumericKeyCode(0x1B), 0); // ESC
        });

        testCase("normaliseAlphaNumericKeyCode returns non-zero for alphanumeric", [&] {
            expect(normaliseAlphaNumericKeyCode('a') != 0);
            expect(normaliseAlphaNumericKeyCode('Z') != 0);
            expect(normaliseAlphaNumericKeyCode('5') != 0);
        });

        testCase("makeNoteBinding creates correct binding", [&] {
            auto binding = makeNoteBinding('C', 72, 2, 0.8f, KeyTrigger::keyDown);

            expect(binding.keyCode != 0);
            expectEquals(binding.displayText, juce::String("C"));
            expect(binding.action.type == KeyActionType::note);
            expectEquals(binding.action.midiNote, 72);
            expectEquals(binding.action.midiChannel, 2);
            expectEquals(binding.action.velocity, 0.8f);
            expect(binding.action.trigger == KeyTrigger::keyDown);
        });

        testCase("makeNoteBinding with strong types", [&] {
            using devpiano::core::MidiChannel;
            using devpiano::core::MidiNoteNumber;
            using devpiano::core::Velocity;

            auto binding = makeNoteBinding('D', MidiNoteNumber::fromClamped(50), MidiChannel::fromClamped(3),
                                           Velocity::fromClamped(0.5f));

            expectEquals(binding.action.midiNote, 50);
            expectEquals(binding.action.midiChannel, 3);
            expectEquals(binding.action.velocity, 0.5f);
        });

        testCase("findByKeyCode returns null for missing key", [&] {
            KeyboardLayout layout;
            expect(layout.findByKeyCode(-1) == nullptr);
            expect(layout.findByKeyCode(9999) == nullptr);
        });
    }
};

static KeyBindingTest keyBindingTest;

// =============================================================================

class DefaultKeyboardLayoutTest : public juce::UnitTest {
public:
    DefaultKeyboardLayoutTest()
        : juce::UnitTest("DefaultLayout", "DevPiano/Core") {
    }

    void runTest() override {
        using devpiano::core::KeyboardLayout;
        using devpiano::core::KeyTrigger;
        using devpiano::core::makeAlphaNumericKeyCode;
        using devpiano::core::makeDefaultKeyboardLayout;

        testCase("default layout has correct number of keys", [&] {
            auto layout = makeDefaultKeyboardLayout();
            // 10 (123 row) + 10 (QWERTY row) + 9 (ASDF row) + 7 (ZXCV row) = 36
            expectEquals(layout.bindings.size(), size_t(36));
        });

        testCase("default layout has correct id", [&] {
            auto layout = makeDefaultKeyboardLayout();
            expectEquals(layout.id, juce::String("default.freepiano.minimal"));
        });

        testCase("default layout has correct name", [&] {
            auto layout = makeDefaultKeyboardLayout();
            expectEquals(layout.name, juce::String("FreePiano Minimal"));
        });

        testCase("A key maps to C3 (MIDI note 60)", [&] {
            auto layout = makeDefaultKeyboardLayout();
            auto* binding = layout.findByKeyCode(makeAlphaNumericKeyCode('A'));
            expect(binding != nullptr);
            expectEquals(binding->action.midiNote, 60);
            expectEquals(binding->action.midiChannel, 1);
            expect(binding->action.trigger == KeyTrigger::keyDown);
        });

        testCase("W key maps to D#4 (MIDI note 74)", [&] {
            auto layout = makeDefaultKeyboardLayout();
            auto* binding = layout.findByKeyCode(makeAlphaNumericKeyCode('W'));
            expect(binding != nullptr);
            expectEquals(binding->action.midiNote, 74); // C4 (72) + 2 = D4 = 74
        });

        testCase("Z key maps to C2 (MIDI note 48)", [&] {
            auto layout = makeDefaultKeyboardLayout();
            auto* binding = layout.findByKeyCode(makeAlphaNumericKeyCode('Z'));
            expect(binding != nullptr);
            expectEquals(binding->action.midiNote, 48);
        });

        testCase("1 key maps to C5 (MIDI note 84)", [&] {
            auto layout = makeDefaultKeyboardLayout();
            auto* binding = layout.findByKeyCode(makeAlphaNumericKeyCode('1'));
            expect(binding != nullptr);
            expectEquals(binding->action.midiNote, 84);
        });

        testCase("all letter keys have bindings", [&] {
            auto layout = makeDefaultKeyboardLayout();
            const juce::String letters("QWERTYUIOPASDFGHJKLZXCVBNM");
            for (auto c : letters) {
                auto* binding = layout.findByKeyCode(makeAlphaNumericKeyCode(c));
                expect(binding != nullptr, "Missing binding for key '" + juce::String::charToString(c) + "'");
            }
        });
    }
};

static DefaultKeyboardLayoutTest defaultKeyboardLayoutTest;

// =============================================================================

class FullKeyboardLayoutTest : public juce::UnitTest {
public:
    FullKeyboardLayoutTest()
        : juce::UnitTest("FullLayout", "DevPiano/Core") {
    }

    void runTest() override {
        using devpiano::core::KeyboardLayout;
        using devpiano::core::makeAlphaNumericKeyCode;
        using devpiano::core::makeFullPianoLayout;

        testCase("full layout has correct number of keys", [&] {
            auto layout = makeFullPianoLayout();
            expectEquals(layout.bindings.size(), size_t(36));
        });

        testCase("full layout has correct id and name", [&] {
            auto layout = makeFullPianoLayout();
            expectEquals(layout.id, juce::String("default.freepiano.full"));
            expectEquals(layout.name, juce::String("FreePiano Full"));
        });

        testCase("full layout has higher octave range", [&] {
            auto layout = makeFullPianoLayout();
            // In full layout, A maps to C4 (72), Z maps to C3 (60)
            auto* aBinding = layout.findByKeyCode(makeAlphaNumericKeyCode('A'));
            expect(aBinding != nullptr);
            expectEquals(aBinding->action.midiNote, 72); // C4 = 72

            auto* zBinding = layout.findByKeyCode(makeAlphaNumericKeyCode('Z'));
            expect(zBinding != nullptr);
            expectEquals(zBinding->action.midiNote, 60); // C3 = 60
        });
    }
};

static FullKeyboardLayoutTest fullKeyboardLayoutTest;

// =============================================================================

class MixedCaseKeyLookupTest : public juce::UnitTest {
public:
    MixedCaseKeyLookupTest()
        : juce::UnitTest("KeyLookup", "DevPiano/Core") {
    }

    void runTest() override {
        using devpiano::core::makeDefaultKeyboardLayout;
        using devpiano::core::normaliseAlphaNumericKeyCode;

        testCase("lowercase and uppercase find same binding", [&] {
            auto layout = makeDefaultKeyboardLayout();
            auto upperCode = normaliseAlphaNumericKeyCode('G');
            auto lowerCode = normaliseAlphaNumericKeyCode('g');
            expectEquals(upperCode, lowerCode);

            auto* upperBinding = layout.findByKeyCode(upperCode);
            auto* lowerBinding = layout.findByKeyCode(lowerCode);
            expect(upperBinding != nullptr);
            expect(lowerBinding != nullptr);
            expect(upperBinding == lowerBinding);
        });
    }
};

static MixedCaseKeyLookupTest mixedCaseKeyLookupTest;

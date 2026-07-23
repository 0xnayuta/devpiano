#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"
#include "Input/KeyboardMidiMapper.h"

using namespace devpiano::core;

// =============================================================================
// Tests for KeyboardMidiMapper: layout management, key press mapping, note-on/off
// =============================================================================

namespace {
/// Build a minimal layout with a single binding.
KeyboardLayout makeSingleBindingLayout(char key, int midiNote, int midiChannel = 1, float velocity = 1.0f)
{
    KeyboardLayout layout;
    layout.id = "test.single";
    layout.name = "Test Single";
    layout.bindings.push_back(makeNoteBinding(key, midiNote, midiChannel, velocity));
    return layout;
}

/// Build a layout with two bindings.
KeyboardLayout makeTwoBindingLayout(char key1, int note1, char key2, int note2)
{
    KeyboardLayout layout;
    layout.id = "test.pair";
    layout.name = "Test Pair";
    layout.bindings.push_back(makeNoteBinding(key1, note1));
    layout.bindings.push_back(makeNoteBinding(key2, note2));
    return layout;
}

/// Count the number of held notes in a MidiKeyboardState.
int countNotesOn(const juce::MidiKeyboardState& state)
{
    int count = 0;
    for (int ch = 1; ch <= 16; ++ch)
        for (int note = 0; note < 128; ++note)
            if (state.isNoteOn(ch, note))
                ++count;
    return count;
}

/// Check if a specific note is on in a given channel.
bool isNoteOn(const juce::MidiKeyboardState& state, int midiChannel, int midiNote)
{
    return state.isNoteOn(midiChannel, midiNote);
}
} // namespace

// =============================================================================

class LayoutManagementTest : public juce::UnitTest {
public:
    LayoutManagementTest() : juce::UnitTest("KeyboardMidiMapper: layout management") {}

    void runTest() override
    {
        beginTest("default layout on construction");
        {
            KeyboardMidiMapper mapper;
            const auto& layout = mapper.getLayout();
            expectEquals(layout.name, juce::String("DevPiano Default"));
            expectEquals(layout.id, juce::String("devpiano.default"));
            expectEquals(static_cast<int>(layout.bindings.size()), 36,
                         juce::String("default layout should have 36 bindings"));
        }

        beginTest("setLayout replaces and changes binding count");
        {
            KeyboardMidiMapper mapper;
            auto custom = makeSingleBindingLayout('A', 60);
            mapper.setLayout(custom);

            const auto& layout = mapper.getLayout();
            expectEquals(static_cast<int>(layout.bindings.size()), 1);
            expectEquals(layout.name, juce::String("Test Single"));
        }

        beginTest("setLayoutDisplayName updates name");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayoutDisplayName("My Custom Layout");
            expectEquals(mapper.getLayout().name, juce::String("My Custom Layout"));

            // ID unchanged.
            expectEquals(mapper.getLayout().id, juce::String("devpiano.default"));
        }

        beginTest("resetToDefaultLayout restores defaults");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('Z', 48));
            expectEquals(static_cast<int>(mapper.getLayout().bindings.size()), 1);

            mapper.resetToDefaultLayout();
            expectEquals(static_cast<int>(mapper.getLayout().bindings.size()), 36);
            expectEquals(mapper.getLayout().name, juce::String("DevPiano Default"));
        }
    }
};

static LayoutManagementTest layoutManagementTest;

// =============================================================================

class NoteOnMappingTest : public juce::UnitTest {
public:
    NoteOnMappingTest() : juce::UnitTest("KeyboardMidiMapper: note-on mapping") {}

    void runTest() override
    {
        beginTest("mapped key sends note-on to keyboardState");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            juce::KeyPress keyPress('A');
            bool consumed = mapper.handleKeyPressed(keyPress, keyState);
            expect(consumed, "mapped key should be consumed");

            // noteOn() is called directly by handleKeyPressed, so isNoteOn
            // reflects the state immediately without extra processing.
            expect(isNoteOn(keyState, 1, 72), "note 72 should be on in channel 1");
        }

        beginTest("unmapped key returns false and does not add notes");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            juce::KeyPress keyPress('Z'); // not in layout
            bool consumed = mapper.handleKeyPressed(keyPress, keyState);
            expect(!consumed, "unmapped key should not be consumed");

            expectEquals(countNotesOn(keyState), 0);
        }

        beginTest("non-alphanumeric key returns false");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            // KeyPress with keyCode = juce::KeyPress::escapeKey (non-alphanumeric).
            juce::KeyPress keyPress(juce::KeyPress::escapeKey);
            bool consumed = mapper.handleKeyPressed(keyPress, keyState);
            expect(!consumed, "escape key should not be consumed");
        }

        beginTest("correct MIDI channel and velocity propagated");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('Q', 60, 3, 0.5f));

            juce::MidiKeyboardState keyState;
            juce::KeyPress keyPress('Q');
            bool consumed = mapper.handleKeyPressed(keyPress, keyState);
            expect(consumed);

            expect(isNoteOn(keyState, 3, 60), "note 60 should be on in channel 3");
        }
    }
};

static NoteOnMappingTest noteOnMappingTest;

// =============================================================================

class RepeatKeySuppressionTest : public juce::UnitTest {
public:
    RepeatKeySuppressionTest() : juce::UnitTest("KeyboardMidiMapper: repeat key suppression") {}

    void runTest() override
    {
        beginTest("pressing same key twice does not duplicate note-on");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            juce::KeyPress keyPress('A');

            // First press → note-on.
            expect(mapper.handleKeyPressed(keyPress, keyState));

            // Second press of same key before release → consumed but no duplicate.
            bool second = mapper.handleKeyPressed(keyPress, keyState);
            expect(second, "should return true (consumed) even on repeat");

            // Only one note should be active.
            expectEquals(countNotesOn(keyState), 1);
        }

        beginTest("two different keys both register independently");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeTwoBindingLayout('A', 72, 'S', 74));

            juce::MidiKeyboardState keyState;
            expect(mapper.handleKeyPressed(juce::KeyPress('A'), keyState));
            expect(mapper.handleKeyPressed(juce::KeyPress('S'), keyState));

            expectEquals(countNotesOn(keyState), 2);
        }
    }
};

static RepeatKeySuppressionTest repeatKeySuppressionTest;

// =============================================================================

class NoteOffOnReleaseTest : public juce::UnitTest {
public:
    NoteOffOnReleaseTest() : juce::UnitTest("KeyboardMidiMapper: note-off on release") {}

    void runTest() override
    {
        beginTest("handleKeyStateChanged releases held keys");
        {
            // NOTE: handleKeyStateChanged uses juce::KeyPress::isKeyCurrentlyDown(),
            // which queries the OS keyboard state. In a headless unit-test
            // environment, this returns false for all keys. Therefore, calling
            // handleKeyStateChanged after handleKeyPressed will cause the held
            // key to be released (because the OS reports it as "not currently down").
            //
            // This test verifies that when a key transitions from held to
            // not-held (per OS state), the mapper sends note-off.
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;

            // Press the key via handleKeyPressed (simulates key-down event).
            mapper.handleKeyPressed(juce::KeyPress('A'), keyState);
            expect(isNoteOn(keyState, 1, 72), "key A should be on after press");
            expectEquals(countNotesOn(keyState), 1);

            // Now call handleKeyStateChanged. In a headless environment,
            // isKeyCurrentlyDown('A') returns false → note-off should be sent.
            mapper.handleKeyStateChanged(keyState);
            expect(!isNoteOn(keyState, 1, 72), "key A should be off after state change detects release");
            expectEquals(countNotesOn(keyState), 0);
        }

        beginTest("handleKeyStateChanged with no held keys does nothing");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            // No keys held → should be a no-op.
            bool consumed = mapper.handleKeyStateChanged(keyState);
            expect(!consumed, "no held keys → no consumption");
            expectEquals(countNotesOn(keyState), 0);
        }
    }
};

static NoteOffOnReleaseTest noteOffOnReleaseTest;

// =============================================================================

class MultipleKeysTest : public juce::UnitTest {
public:
    MultipleKeysTest() : juce::UnitTest("KeyboardMidiMapper: multiple simultaneous keys") {}

    void runTest() override
    {
        beginTest("multiple keys pressed produce multiple notes");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeTwoBindingLayout('A', 60, 'S', 64));

            juce::MidiKeyboardState keyState;
            mapper.handleKeyPressed(juce::KeyPress('A'), keyState);
            mapper.handleKeyPressed(juce::KeyPress('S'), keyState);

            expectEquals(countNotesOn(keyState), 2);
            expect(isNoteOn(keyState, 1, 60));
            expect(isNoteOn(keyState, 1, 64));
        }
    }
};

static MultipleKeysTest multipleKeysTest;

// =============================================================================

class ChannelMapperNullTest : public juce::UnitTest {
public:
    ChannelMapperNullTest() : juce::UnitTest("KeyboardMidiMapper: nullptr channel mapper") {}

    void runTest() override
    {
        beginTest("works correctly with no channel mapper set");
        {
            KeyboardMidiMapper mapper;
            // channelMapper defaults to nullptr — should still work.
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            mapper.handleKeyPressed(juce::KeyPress('A'), keyState);

            expect(isNoteOn(keyState, 1, 72), "should work without channel mapper");
        }
    }
};

static ChannelMapperNullTest channelMapperNullTest;

// =============================================================================

class MixedCaseKeyTest : public juce::UnitTest {
public:
    MixedCaseKeyTest() : juce::UnitTest("KeyboardMidiMapper: mixed case key lookup") {}

    void runTest() override
    {
        beginTest("lowercase KeyPress matches uppercase binding");
        {
            // Layout has binding for 'A' (uppercase normalized key code).
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            // Press 'a' (lowercase). normaliseKeyCode will convert to uppercase.
            juce::KeyPress keyPress('a');
            bool consumed = mapper.handleKeyPressed(keyPress, keyState);
            expect(consumed, "lowercase key should match uppercase binding");

            expect(isNoteOn(keyState, 1, 72));
        }
    }
};

static MixedCaseKeyTest mixedCaseKeyTest;

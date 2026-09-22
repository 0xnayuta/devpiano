#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"
#include "Input/KeyboardMidiMapper.h"

namespace {

int countNotesOn(const juce::MidiKeyboardState& state) {
    int count = 0;
    for (int ch = 1; ch <= 16; ++ch) {
        for (int note = 0; note < 128; ++note) {
            if (state.isNoteOn(ch, note)) {
                ++count;
            }
        }
    }
    return count;
}

class PerformanceModifierTest final : public juce::UnitTest {
public:
    PerformanceModifierTest()
        : juce::UnitTest("PerformanceModifier: Transient Event Pipeline", "DevPiano/Input") {
    }

    void runTest() override {
        testShiftVelocityBoost();
        testAltOctaveShift();
        testModifierReleasedWhileKeyHeldPreservesIdentity();
        testQwertySnapshotReflectsModifiers();
        testModifierKeysChangedUpdatesSnapshotAndPedal();
    }

private:
    void testShiftVelocityBoost() {
        beginTest("Shift modifier transients boost velocity to 1.0f without mutating baseline");

        KeyboardMidiMapper mapper;
        devpiano::core::KeyboardLayout layout;
        // Key 'A' with baseline velocity 0.5f
        layout.bindings.push_back(devpiano::core::makeNoteBinding('A', 60, 1, 0.5f));
        mapper.setLayout(layout);

        juce::MidiKeyboardState state;

        // 1. Normal press: velocity is baseline
        const juce::KeyPress normalPress('a');
        mapper.handleKeyPressed(normalPress, state);
        expect(state.isNoteOn(1, 60));
        mapper.releaseAllHeldKeys(state);

        // 2. Press with Shift modifier: velocity boosted to 1.0f
        const juce::KeyPress shiftPress('a', juce::ModifierKeys::shiftModifier, 0);
        mapper.handleKeyPressed(shiftPress, state);
        expect(state.isNoteOn(1, 60));

        // The held key identity must record boosted velocity 1.0f
        const auto* held = mapper.findHeldKey(devpiano::core::makeAlphaNumericKeyCode('A'));
        expect(held != nullptr);
        if (held != nullptr) {
            expectEquals(held->velocity, 1.0f, "Shift must boost velocity to 1.0f");
        }
        mapper.releaseAllHeldKeys(state);

        // 3. Baseline verification: binding in layout must be 100% untouched
        const auto* binding = mapper.getLayout().findByKeyCode(devpiano::core::makeAlphaNumericKeyCode('A'));
        expect(binding != nullptr);
        if (binding != nullptr) {
            expectEquals(binding->action.velocity, 0.5f, "Baseline binding velocity must remain unmutated");
        }
    }

    void testAltOctaveShift() {
        beginTest("Alt modifier transients shift pitch by +1 octave without mutating baseline");

        KeyboardMidiMapper mapper;
        devpiano::core::KeyboardLayout layout;
        layout.bindings.push_back(devpiano::core::makeNoteBinding('A', 60, 1, 0.8f)); // C4
        mapper.setLayout(layout);

        juce::MidiKeyboardState state;

        // Press with Alt modifier: pitch shifted from 60 to 72 (+12 semitones)
        const juce::KeyPress altPress('a', juce::ModifierKeys::altModifier, 0);
        mapper.handleKeyPressed(altPress, state);

        expect(state.isNoteOn(1, 72), "Sounding pitch must be shifted to 72");
        expect(!state.isNoteOn(1, 60), "Pitch 60 must not sound");

        const auto* held = mapper.findHeldKey(devpiano::core::makeAlphaNumericKeyCode('A'));
        expect(held != nullptr);
        if (held != nullptr) {
            expectEquals(held->soundingMidiNote, 72, "Sounding identity must lock shifted pitch 72");
        }
        mapper.releaseAllHeldKeys(state);

        // Baseline verification: binding pitch must remain 60
        const auto* binding = mapper.getLayout().findByKeyCode(devpiano::core::makeAlphaNumericKeyCode('A'));
        expect(binding != nullptr);
        if (binding != nullptr) {
            expectEquals(binding->action.midiNote, 60, "Baseline binding pitch must remain 60");
        }
    }

    void testModifierReleasedWhileKeyHeldPreservesIdentity() {
        beginTest("NoteOff preserves sounding pitch even if modifier released before key release");

        KeyboardMidiMapper mapper;
        devpiano::core::KeyboardLayout layout;
        layout.bindings.push_back(devpiano::core::makeNoteBinding('A', 60, 1, 0.8f));
        mapper.setLayout(layout);

        bool isAHeld = true;
        mapper.setKeyStatePredicate(
            [&](int keyCode) { return (keyCode == devpiano::core::makeAlphaNumericKeyCode('A')) && isAHeld; });

        juce::MidiKeyboardState state;

        // 1. Press 'A' while Alt is held -> sounds 72
        const juce::KeyPress altPress('a', juce::ModifierKeys::altModifier, 0);
        mapper.handleKeyPressed(altPress, state);
        expect(state.isNoteOn(1, 72));
        expectEquals(countNotesOn(state), 1);

        // 2. Release Alt modifier while 'A' key is STILL held down
        devpiano::core::PerformanceModifierState resetMods;
        resetMods.altActive = false;
        mapper.setModifierState(resetMods);

        // 3. Now release physical 'A' key
        isAHeld = false;
        mapper.handleKeyStateChanged(state);

        // 4. Must release pitch 72 cleanly, zero hanging notes!
        expect(!state.isNoteOn(1, 72), "Sounding pitch 72 must be released");
        expect(!state.isNoteOn(1, 60), "Pitch 60 was never sounding and must be untouched");
        expectEquals(countNotesOn(state), 0, "Zero hanging notes remain");
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);
    }

    void testQwertySnapshotReflectsModifiers() {
        beginTest("Qwerty snapshot accurately reflects modifier states and projected pitch preview");

        KeyboardMidiMapper mapper;
        devpiano::core::PerformanceModifierState mods;
        mods.shiftActive = true;
        mods.altActive = true;
        mapper.setModifierState(mods);

        const auto vm = mapper.createQwertySnapshot(0);
        expect(vm.isShiftActive);
        expect(vm.isAltActive);
        expect(!vm.isCtrlActive);

        // With Alt active, 'A' (base 60 / C4) projects to 72 / C5 in snapshot
        const auto* binding = mapper.getLayout().findByKeyCode(devpiano::core::makeAlphaNumericKeyCode('A'));
        expect(binding != nullptr);

        bool foundProjectedA = false;
        for (const auto& row : vm.rows) {
            for (const auto& k : row.keys) {
                if (k.keyCode == devpiano::core::makeAlphaNumericKeyCode('A')) {
                    expectEquals(k.mappedMidiNote, 72, "A must project to octave-shifted pitch 72");
                    expect(k.noteName.contains("5"), "Pitch label must reflect shifted octave");
                    foundProjectedA = true;
                }
            }
        }
        expect(foundProjectedA);
    }
    void testModifierKeysChangedUpdatesSnapshotAndPedal() {
        beginTest("handleModifierKeysChanged accurately clears modifier highlight and updates pedals");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;
        bool isSpaceDown = false;
        mapper.setKeyStatePredicate([&](int keyCode) { return (keyCode == juce::KeyPress::spaceKey) && isSpaceDown; });

        // 1. Simulate Ctrl+Space press: Space key down with Ctrl modifier
        isSpaceDown = true;
        const juce::KeyPress ctrlSpace(' ', juce::ModifierKeys::ctrlModifier, 0);
        mapper.handleKeyPressed(ctrlSpace, state);

        auto vm = mapper.createQwertySnapshot(0);
        expect(vm.isCtrlActive, "Ctrl must be active when pressed with Space");
        expect(vm.isSustainPedalDown, "Sustain pedal must be active when Space is down");
        // 2. Space released first, while Ctrl modifier is still maintained
        isSpaceDown = false;
        mapper.handleModifierKeysChanged(juce::ModifierKeys::ctrlModifier, state);

        vm = mapper.createQwertySnapshot(0);
        expect(!vm.isSustainPedalDown, "Sustain pedal must release when Space is released");
        expect(vm.isCtrlActive, "Ctrl modifier remains active while Ctrl is held");
        // 3. User releases Ctrl: OS/JUCE sends modifierKeysChanged with zero flags
        mapper.handleModifierKeysChanged(juce::ModifierKeys(0), state);

        vm = mapper.createQwertySnapshot(0);
        expect(!vm.isCtrlActive, "Ctrl highlight must be cleared immediately upon Ctrl release");
        expect(!vm.isShiftActive);
        expect(!vm.isAltActive);

        // 4. Shift+Space soft pedal dynamic interaction
        isSpaceDown = true;
        mapper.handleKeyPressed(juce::KeyPress(' ', juce::ModifierKeys::shiftModifier, 0), state);
        vm = mapper.createQwertySnapshot(0);
        expect(vm.isSoftPedalDown, "Shift+Space triggers soft pedal");
        expect(vm.isShiftActive, "Shift modifier must be active");

        // Space released, then Shift released
        isSpaceDown = false;
        mapper.handleKeyStateChanged(state);
        mapper.handleModifierKeysChanged(juce::ModifierKeys(0), state);

        vm = mapper.createQwertySnapshot(0);
        expect(!vm.isSoftPedalDown, "Soft pedal released");
        expect(!vm.isShiftActive, "Shift modifier cleared");
    }
};

static PerformanceModifierTest performanceModifierTest;

} // namespace

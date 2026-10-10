#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"
#include "Input/KeyboardMidiMapper.h"
#include "Midi/MidiChannelMapper.h"

using namespace devpiano::core;

// =============================================================================
// KeyboardMidiMapper 测试：布局管理、按键映射、note-on/off
// =============================================================================

namespace {
/// 构建只含单个绑定（binding）的最小布局。
KeyboardLayout makeSingleBindingLayout(char key, int midiNote, int midiChannel = 1, float velocity = 1.0f) {
    KeyboardLayout layout;
    layout.id = "test.single";
    layout.name = "Test Single";
    layout.bindings.push_back(makeNoteBinding(key, midiNote, midiChannel, velocity));
    return layout;
}

/// 构建含两个绑定的布局。
KeyboardLayout makeTwoBindingLayout(char key1, int note1, char key2, int note2) {
    KeyboardLayout layout;
    layout.id = "test.pair";
    layout.name = "Test Pair";
    layout.bindings.push_back(makeNoteBinding(key1, note1));
    layout.bindings.push_back(makeNoteBinding(key2, note2));
    return layout;
}

/// 统计 MidiKeyboardState 中按住的音符数量。
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

/// 检查指定通道上的某个音符是否处于按下状态。
bool isNoteOn(const juce::MidiKeyboardState& state, int midiChannel, int midiNote) {
    return state.isNoteOn(midiChannel, midiNote);
}
} // namespace

// =============================================================================

class KeyMappingTest : public juce::UnitTest {
public:
    KeyMappingTest()
        : juce::UnitTest("KeyboardMidiMapper: key mapping", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("mapped key sends note-on to keyboardState");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            juce::KeyPress keyPress('A');
            bool consumed = mapper.handleKeyPressed(keyPress, keyState);
            expect(consumed, "mapped key should be consumed");

            // noteOn() 由 handleKeyPressed 直接调用，因此 isNoteOn
            // 无需额外处理即可立即反映状态。
            expect(isNoteOn(keyState, 1, 72), "note 72 should be on in channel 1");
        }

        beginTest("unmapped key returns false and does not add notes");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            juce::KeyPress keyPress('Z'); // 不在布局中
            bool consumed = mapper.handleKeyPressed(keyPress, keyState);
            expect(!consumed, "unmapped key should not be consumed");

            expectEquals(countNotesOn(keyState), 0);
        }

        beginTest("non-alphanumeric key returns false");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            // KeyPress 的 keyCode 为 juce::KeyPress::escapeKey（非字母数字）。
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

        beginTest("pressing same key twice does not duplicate note-on");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            juce::KeyPress keyPress('A');

            // 第一次按下 → note-on。
            expect(mapper.handleKeyPressed(keyPress, keyState));

            // 释放前再次按下同一按键 → 被消费但不产生重复 note-on。
            bool second = mapper.handleKeyPressed(keyPress, keyState);
            expect(second, "should return true (consumed) even on repeat");

            // 只应有一个音符处于激活状态。
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

        beginTest("lowercase KeyPress matches uppercase binding");
        {
            // 布局包含 'A'（大写规范化后的 keyCode）的绑定。
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            // 按下 'a'（小写）。normaliseKeyCode 会将其转换为大写。
            juce::KeyPress keyPress('a');
            bool consumed = mapper.handleKeyPressed(keyPress, keyState);
            expect(consumed, "lowercase key should match uppercase binding");

            expect(isNoteOn(keyState, 1, 72));
        }
    }
};

static KeyMappingTest keyMappingTest;

// =============================================================================

class KeyReleaseTest : public juce::UnitTest {
public:
    KeyReleaseTest()
        : juce::UnitTest("KeyboardMidiMapper: key release", "DevPiano/Engine") {
    }

    void runTest() override {
        // TEST-015：注入确定性键状态谓词（全 false = 所有键未按住），
        // 消除对真实 OS 键盘状态的依赖——无头环境下原实现依赖
        // isKeyCurrentlyDown() 恒 false，桌面环境物理按住 'A' 时会误报失败。
        const auto allKeysReleased = [](int) { return false; };

        beginTest("handleKeyStateChanged releases held keys");
        {
            // handleKeyStateChanged 通过可注入谓词判断按键当前是否按下：
            // 无头单元测试中所有按键都报告"未按下"，因此 handleKeyPressed
            // 之后调用 handleKeyStateChanged 会释放按住的按键并发送 note-off。
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));
            mapper.setKeyStatePredicate(allKeysReleased);

            juce::MidiKeyboardState keyState;

            // 通过 handleKeyPressed 按下按键（模拟 key-down 事件）。
            mapper.handleKeyPressed(juce::KeyPress('A'), keyState);
            expect(isNoteOn(keyState, 1, 72), "key A should be on after press");
            expectEquals(countNotesOn(keyState), 1);

            // 谓词报告 'A' 未按住 → 应发送 note-off。
            mapper.handleKeyStateChanged(keyState);
            expect(!isNoteOn(keyState, 1, 72), "key A should be off after state change detects release");
            expectEquals(countNotesOn(keyState), 0);
        }

        beginTest("releaseAllHeldKeys releases notes and sustain pedal");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeTwoBindingLayout('A', 72, 'S', 74));
            mapper.setKeyStatePredicate(allKeysReleased);

            juce::MidiKeyboardState keyState;

            // 按下两个琴键并踩下延音踏板。
            mapper.handleKeyPressed(juce::KeyPress('A'), keyState);
            mapper.handleKeyPressed(juce::KeyPress('S'), keyState);
            mapper.handleKeyPressed(juce::KeyPress(juce::KeyPress::spaceKey), keyState);
            expectEquals(countNotesOn(keyState), 2);
            expect(mapper.isSustainPedalDown(), "sustain pedal should be down");

            // 模拟窗口失焦：一次释放全部持有的键与踏板（Panic 防悬挂音）。
            mapper.releaseAllHeldKeys(keyState);
            expectEquals(countNotesOn(keyState), 0, "all held notes must be released");
            expect(!mapper.isSustainPedalDown(), "sustain pedal must be released");

            // 释放后的 heldKeys 已清空：再次调用应为 no-op，不产生重复 note-off。
            mapper.releaseAllHeldKeys(keyState);
            expectEquals(countNotesOn(keyState), 0, "second release must be a no-op");
        }

        beginTest("handleKeyStateChanged with no held keys does nothing");
        {
            KeyboardMidiMapper mapper;
            mapper.setLayout(makeSingleBindingLayout('A', 72));
            mapper.setKeyStatePredicate(allKeysReleased);

            juce::MidiKeyboardState keyState;
            // 没有按住的按键 → 应为 no-op。
            bool consumed = mapper.handleKeyStateChanged(keyState);
            expect(!consumed, "no held keys -> no consumption");
            expectEquals(countNotesOn(keyState), 0);
        }

        beginTest("works correctly with no channel mapper set");
        {
            KeyboardMidiMapper mapper;
            // channelMapper 默认为 nullptr——仍应正常工作。
            mapper.setLayout(makeSingleBindingLayout('A', 72));

            juce::MidiKeyboardState keyState;
            mapper.handleKeyPressed(juce::KeyPress('A'), keyState);

            expect(isNoteOn(keyState, 1, 72), "should work without channel mapper");
        }
    }
};

static KeyReleaseTest keyReleaseTest;
// =============================================================================

class SustainPedalKeyMappingTest : public juce::UnitTest {
public:
    SustainPedalKeyMappingTest()
        : juce::UnitTest("KeyboardMidiMapper: sustain pedal space key", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("space key down triggers sustain pedal callback");
        {
            KeyboardMidiMapper mapper;
            bool lastPedalState = false;
            int callbackCount = 0;
            mapper.setSustainPedalCallback([&](bool isDown) {
                lastPedalState = isDown;
                ++callbackCount;
            });

            juce::MidiKeyboardState keyState;
            juce::KeyPress spacePress(juce::KeyPress::spaceKey);
            bool consumed = mapper.handleKeyPressed(spacePress, keyState);

            expect(consumed, "space key press should be consumed");
            expect(mapper.isSustainPedalDown(), "mapper reports sustain pedal is down");
            expect(lastPedalState, "callback received isDown = true");
            expectEquals(callbackCount, 1, "callback called exactly once");

            // Key repeat: pressing space again while held does not duplicate callback
            consumed = mapper.handleKeyPressed(spacePress, keyState);
            expect(consumed, "space key repeat is still consumed");
            expectEquals(callbackCount, 1, "key repeat does not trigger duplicate callback");
        }

        beginTest("space key up releases sustain pedal");
        {
            KeyboardMidiMapper mapper;
            bool isSpaceHeld = true;
            mapper.setKeyStatePredicate(
                [&](int keyCode) { return (keyCode == juce::KeyPress::spaceKey) && isSpaceHeld; });

            bool lastPedalState = false;
            int callbackCount = 0;
            mapper.setSustainPedalCallback([&](bool isDown) {
                lastPedalState = isDown;
                ++callbackCount;
            });

            juce::MidiKeyboardState keyState;
            mapper.handleKeyPressed(juce::KeyPress(juce::KeyPress::spaceKey), keyState);
            expect(mapper.isSustainPedalDown());

            // Release space key
            isSpaceHeld = false;
            bool consumed = mapper.handleKeyStateChanged(keyState);

            expect(consumed, "space key release should be consumed");
            expect(!mapper.isSustainPedalDown(), "mapper reports sustain pedal is released");
            expect(!lastPedalState, "callback received isDown = false");
            expectEquals(callbackCount, 2, "callback called on press and release");
        }
    }
};

// =============================================================================
// Phase 34-B: Layout Group & HeldKey Identity Preservation (No Hanging Notes)
// =============================================================================

class LayoutGroupAndHeldKeyIdentityTest : public juce::UnitTest {
public:
    LayoutGroupAndHeldKeyIdentityTest()
        : juce::UnitTest("LayoutGroup: Identity Preservation", "DevPiano/Input") {
    }

    void runTest() override {
        testGroupSwitchingCyclesAndCallbacks();
        testNoteOffIdentityPreservedAcrossGroupSwitch();
        testSoundingChannelOverridePreservation();
        testFinalOutputIdentitySurvivesMatrixReplacement();
        testLayoutReplacementPreservesHeldIdentity();
        testMultipleHeldKeysAcrossDifferentGroupsReleaseCleanly();
        testBacktickGroupCyclingAndRepeatLatch();
        testReleaseAllHeldKeysClearsTransientModifiers();
        testDefaultQKAlternatedReleasesKeepSoundingUntilLastHolder();
        testMatrixMergedPitchesKeepSoundingUntilLastHolder();
        testOverlappingHeldNotesGroupAndLayoutAndMapperChange();
        testOverlappingHeldNotesFocusLossReleasesExactlyOnce();
        testRepeatedAttackInterleavingWhileHeld();
    }

private:
    void testGroupSwitchingCyclesAndCallbacks() {
        beginTest("Group switching cycles 0..3 and triggers callback");

        KeyboardMidiMapper mapper;
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 0);

        int callbackCount = 0;
        uint8_t lastGroup = 255;
        mapper.setGroupChangeCallback([&](uint8_t g) {
            lastGroup = g;
            ++callbackCount;
        });

        mapper.switchToNextGroup();
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 1);
        expectEquals(static_cast<int>(lastGroup), 1);
        expectEquals(callbackCount, 1);

        mapper.switchToNextGroup();
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 2);
        mapper.switchToNextGroup();
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 3);
        mapper.switchToNextGroup();
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 0);

        mapper.switchToPreviousGroup();
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 3);
    }

    void testNoteOffIdentityPreservedAcrossGroupSwitch() {
        beginTest("NoteOff preserves sounding pitch across group octave shift");

        KeyboardMidiMapper mapper;
        auto layout = makeSingleBindingLayout('A', 60); // C4 base
        layout.groups[0] = { 0, 0, 0, "Base" };
        layout.groups[1] = { 0, 1, 0, "+1 Octave" }; // +12 semitones
        mapper.setLayout(layout);

        bool isAHeld = true;
        mapper.setKeyStatePredicate([&](int keyCode) { return (keyCode == makeAlphaNumericKeyCode('A')) && isAHeld; });

        juce::MidiKeyboardState state;

        // 1. Press 'A' in Group 0 (pitch 60)
        const juce::KeyPress aPress('a');
        expect(mapper.handleKeyPressed(aPress, state));
        expect(state.isNoteOn(1, 60), "Sounding pitch 60 must be on in group 0");
        expectEquals(countNotesOn(state), 1);

        // 2. Switch to Group 1 (+1 octave, where 'A' maps to 72)
        mapper.switchToNextGroup();
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 1);

        // 3. Release 'A' key
        isAHeld = false;
        expect(mapper.handleKeyStateChanged(state));

        // 4. Verification: NoteOff must target the locked identity (pitch 60), NOT 72
        expect(!state.isNoteOn(1, 60), "Pitch 60 must be released");
        expect(!state.isNoteOn(1, 72), "Pitch 72 was never sounding and must be untouched");
        expectEquals(countNotesOn(state), 0, "No hanging notes remain");
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);
    }

    void testSoundingChannelOverridePreservation() {
        beginTest("NoteOff preserves sounding channel across group switch");

        KeyboardMidiMapper mapper;
        auto layout = makeSingleBindingLayout('A', 60, 1);
        layout.groups[0] = { 0, 0, 0, "Ch1" };
        layout.groups[1] = { 0, 0, 5, "Ch5" }; // override to channel 5
        mapper.setLayout(layout);

        bool isAHeld = false;
        mapper.setKeyStatePredicate([&](int keyCode) { return (keyCode == makeAlphaNumericKeyCode('A')) && isAHeld; });

        juce::MidiKeyboardState state;

        // Switch to Group 1 (Ch 5 override)
        mapper.setActiveGroupIndex(1);
        isAHeld = true;

        // Press 'A' -> should sound on channel 5
        mapper.handleKeyPressed(juce::KeyPress('a'), state);
        expect(state.isNoteOn(5, 60), "Note must sound on overridden channel 5");
        expect(!state.isNoteOn(1, 60), "Channel 1 must not have note");

        // Switch back to Group 0 (Channel 1) while holding 'A'
        mapper.setActiveGroupIndex(0);

        // Release 'A'
        isAHeld = false;
        mapper.handleKeyStateChanged(state);

        // NoteOff must have been sent on channel 5, clearing the note
        expect(!state.isNoteOn(5, 60), "Channel 5 note must be released cleanly");
        expectEquals(countNotesOn(state), 0, "Zero hanging notes");
    }
    void testFinalOutputIdentitySurvivesMatrixReplacement() {
        beginTest("held notes release on their original output identity after matrix replacement");

        devpiano::midi::ChannelMatrix initialMatrix;
        initialMatrix.active = true;
        initialMatrix.channels[0].outputChannel = 3;
        initialMatrix.channels[0].transpose = 12;
        devpiano::midi::MidiChannelMapper initialMapper(initialMatrix, false, 0);

        devpiano::midi::ChannelMatrix updatedMatrix;
        updatedMatrix.active = true;
        updatedMatrix.channels[0].outputChannel = 8;
        updatedMatrix.channels[0].transpose = -12;
        devpiano::midi::MidiChannelMapper updatedMapper(updatedMatrix, false, 0);

        KeyboardMidiMapper mapper;
        mapper.setLayout(makeTwoBindingLayout('A', 60, 'S', 62));
        mapper.setChannelMapper(&initialMapper);

        bool isAHeld = true;
        bool isSHeld = true;
        mapper.setKeyStatePredicate([&](int keyCode) {
            return (keyCode == makeAlphaNumericKeyCode('A') && isAHeld)
                || (keyCode == makeAlphaNumericKeyCode('S') && isSHeld);
        });

        juce::MidiKeyboardState state;
        expect(mapper.handleKeyPressed(juce::KeyPress('a'), state));
        expect(mapper.handleKeyPressed(juce::KeyPress('s'), state));
        expect(state.isNoteOn(4, 72));
        expect(state.isNoteOn(4, 74));

        mapper.setChannelMapper(&updatedMapper);
        isAHeld = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!state.isNoteOn(4, 72), "physical key-up must release the original mapped note");
        expect(state.isNoteOn(4, 74), "the other held note must remain active");
        expect(!state.isNoteOn(9, 48), "the replacement mapping must not receive the old note-off");

        mapper.releaseAllHeldKeys(state);
        expect(!state.isNoteOn(4, 74), "panic release must use the original mapped identity");
        expectEquals(countNotesOn(state), 0);
        expect(!state.isNoteOn(9, 50), "panic release must not target the replacement mapping");
    }

    void testLayoutReplacementPreservesHeldIdentity() {
        beginTest("layout replacement retains a held note until its physical key-up");

        KeyboardMidiMapper mapper;
        mapper.setLayout(makeSingleBindingLayout('A', 60));

        bool isAHeld = true;
        mapper.setKeyStatePredicate([&](int keyCode) { return keyCode == makeAlphaNumericKeyCode('A') && isAHeld; });

        juce::MidiKeyboardState state;
        expect(mapper.handleKeyPressed(juce::KeyPress('a'), state));
        expect(state.isNoteOn(1, 60));

        mapper.setLayout(makeSingleBindingLayout('S', 62));
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);

        isAHeld = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!state.isNoteOn(1, 60), "layout replacement must not discard the old note identity");
        expect(!state.isNoteOn(1, 62), "the new layout must not receive an unmatched note-off");
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);
    }

    void testMultipleHeldKeysAcrossDifferentGroupsReleaseCleanly() {
        beginTest("Multi-key held across alternating groups release cleanly with releaseAllHeldKeys");

        KeyboardMidiMapper mapper;
        KeyboardLayout layout;
        layout.bindings.push_back(makeNoteBinding('A', 60));
        layout.bindings.push_back(makeNoteBinding('S', 62));
        layout.bindings.push_back(makeNoteBinding('D', 64));
        layout.groups[0] = { 0, 0, 1, "G0" };
        layout.groups[1] = { 0, 1, 2, "G1" }; // +12 st, Ch 2
        layout.groups[2] = { -12, 0, 3, "G2" }; // -12 st, Ch 3
        mapper.setLayout(layout);

        juce::MidiKeyboardState state;

        // Key 1: 'A' in G0 -> sounds (Ch 1, 60)
        mapper.setActiveGroupIndex(0);
        mapper.handleKeyPressed(juce::KeyPress('a'), state);
        expect(state.isNoteOn(1, 60));

        // Key 2: 'S' in G1 -> sounds (Ch 2, 74)
        mapper.setActiveGroupIndex(1);
        mapper.handleKeyPressed(juce::KeyPress('s'), state);
        expect(state.isNoteOn(2, 74));

        // Key 3: 'D' in G2 -> sounds (Ch 3, 52)
        mapper.setActiveGroupIndex(2);
        mapper.handleKeyPressed(juce::KeyPress('d'), state);
        expect(state.isNoteOn(3, 52));

        expectEquals(countNotesOn(state), 3);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 3);

        // Panic release
        mapper.releaseAllHeldKeys(state);
        expectEquals(countNotesOn(state), 0, "All 3 heterogeneous notes must be released");
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);
    }

    void testBacktickGroupCyclingAndRepeatLatch() {
        beginTest("Backtick key cycles group and suppresses auto-repeat until key release");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        bool isBacktickDown = false;
        mapper.setKeyStatePredicate([&](int kc) { return kc == '`' && isBacktickDown; });

        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 0);

        // 1. Initial press of '`' cycles to group 1
        isBacktickDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('`'), state));
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 1);

        // 2. Simulated auto-repeat while '`' is still held must be consumed but NOT cycle again
        expect(mapper.handleKeyPressed(juce::KeyPress('`'), state));
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 1, "Auto-repeat must not cycle group again");

        // 3. Key release clears the latch
        isBacktickDown = false;
        mapper.handleKeyStateChanged(state);

        // 4. Pressing again cycles to group 2
        isBacktickDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('`'), state));
        expectEquals(static_cast<int>(mapper.getActiveGroupIndex()), 2);

        // 5. Panic release clears repeat latch
        mapper.releaseAllHeldKeys(state);
        isBacktickDown = false;
    }

    void testReleaseAllHeldKeysClearsTransientModifiers() {
        beginTest("releaseAllHeldKeys resets transient modifiers to prevent stuck octave or velocity");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        // Simulate Alt and Shift held
        devpiano::core::PerformanceModifierState mods;
        mods.shiftActive = true;
        mods.altActive = true;
        mapper.setModifierState(mods);

        const auto snapshotBefore = mapper.createQwertySnapshot(0);
        expect(snapshotBefore.isAltActive);
        expect(snapshotBefore.isShiftActive);

        // Panic release (e.g. Alt-Tab focus loss)
        mapper.releaseAllHeldKeys(state);

        const auto snapshotAfter = mapper.createQwertySnapshot(0);
        expect(!snapshotAfter.isAltActive, "Alt modifier must be cleared on panic");
        expect(!snapshotAfter.isShiftActive, "Shift modifier must be cleared on panic");
        expect(!snapshotAfter.isCtrlActive, "Ctrl modifier must be cleared on panic");
    }

    void testDefaultQKAlternatedReleasesKeepSoundingUntilLastHolder() {
        beginTest("QUAL-002: Default Q/K both MIDI72 Ch1 stay sounding on first release, close on last release");

        KeyboardMidiMapper mapper;
        mapper.resetToDefaultLayout();

        bool isQDown = false;
        bool isKDown = false;
        mapper.setKeyStatePredicate([&](int kc) {
            return (kc == makeAlphaNumericKeyCode('Q') && isQDown) || (kc == makeAlphaNumericKeyCode('K') && isKDown);
        });

        juce::MidiKeyboardState state;

        // Sequence 1: Press Q then K, release Q first then K
        isQDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        expect(isNoteOn(state, 1, 72));
        expectEquals(countNotesOn(state), 1);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);

        isKDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('k'), state));
        expect(isNoteOn(state, 1, 72));
        expectEquals(countNotesOn(state), 1);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 2);

        // Release Q: K is still held down, so note 72 must remain sounding!
        isQDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(isNoteOn(state, 1, 72), "Note 72 must stay sounding after Q release because K is still held");
        expectEquals(countNotesOn(state), 1);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);
        expect(mapper.findHeldKey(makeAlphaNumericKeyCode('Q')) == nullptr);
        expect(mapper.findHeldKey(makeAlphaNumericKeyCode('K')) != nullptr);

        // Release K: K was the last holder, so note 72 must close!
        isKDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!isNoteOn(state, 1, 72), "Note 72 must close when last holder K is released");
        expectEquals(countNotesOn(state), 0);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);

        // Sequence 2 (reverse): Press Q then K, release K first then Q
        isQDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        isKDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('k'), state));
        expect(isNoteOn(state, 1, 72));
        expectEquals(countNotesOn(state), 1);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 2);

        // Release K: Q is still held, note 72 must remain sounding!
        isKDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(isNoteOn(state, 1, 72), "Note 72 must stay sounding after K release because Q is still held");
        expectEquals(countNotesOn(state), 1);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);

        // Release Q: last holder, note 72 closes!
        isQDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!isNoteOn(state, 1, 72), "Note 72 must close when last holder Q is released");
        expectEquals(countNotesOn(state), 0);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);
    }

    void testMatrixMergedPitchesKeepSoundingUntilLastHolder() {
        beginTest("QUAL-002: Inputs collapsed by channel matrix stay sounding on first release, close on last release");

        devpiano::midi::ChannelMatrix matrix;
        matrix.active = true;
        matrix.channels[0].outputChannel = 0;
        matrix.channels[0].transpose = 0;
        matrix.channels[1].outputChannel = 0;
        matrix.channels[1].transpose = 2;
        devpiano::midi::MidiChannelMapper mapperService(matrix, false, 0);

        KeyboardMidiMapper mapper;
        KeyboardLayout layout;
        layout.bindings.push_back(makeNoteBinding('A', 60, 1));
        layout.bindings.push_back(makeNoteBinding('S', 58, 2));
        mapper.setLayout(layout);
        mapper.setChannelMapper(&mapperService);

        bool isADown = false;
        bool isSDown = false;
        mapper.setKeyStatePredicate([&](int kc) {
            return (kc == makeAlphaNumericKeyCode('A') && isADown) || (kc == makeAlphaNumericKeyCode('S') && isSDown);
        });

        juce::MidiKeyboardState state;

        isADown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('a'), state));
        expect(isNoteOn(state, 1, 60));
        expectEquals(countNotesOn(state), 1);

        isSDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('s'), state));
        expect(isNoteOn(state, 1, 60));
        expectEquals(countNotesOn(state), 1);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 2);

        isADown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(isNoteOn(state, 1, 60), "Collapsed note (1, 60) must remain sounding while S is held");
        expectEquals(countNotesOn(state), 1);

        isSDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!isNoteOn(state, 1, 60), "Collapsed note (1, 60) must close when S is released");
        expectEquals(countNotesOn(state), 0);
    }

    void testOverlappingHeldNotesGroupAndLayoutAndMapperChange() {
        beginTest("QUAL-002: Group, layout, and mapper changes preserve overlapping held identities until last holder");

        KeyboardMidiMapper mapper;
        mapper.resetToDefaultLayout();

        bool isQDown = true;
        bool isKDown = true;
        mapper.setKeyStatePredicate([&](int kc) {
            return (kc == makeAlphaNumericKeyCode('Q') && isQDown) || (kc == makeAlphaNumericKeyCode('K') && isKDown);
        });

        juce::MidiKeyboardState state;
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        expect(mapper.handleKeyPressed(juce::KeyPress('k'), state));
        expect(isNoteOn(state, 1, 72));

        mapper.switchToNextGroup();

        devpiano::midi::ChannelMatrix matrix;
        matrix.active = true;
        matrix.channels[0].outputChannel = 5;
        devpiano::midi::MidiChannelMapper newMapper(matrix, false, 0);
        mapper.setChannelMapper(&newMapper);

        mapper.setLayout(makeTwoBindingLayout('Z', 48, 'X', 50));

        isQDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(isNoteOn(state, 1, 72), "Original identity (1, 72) must survive group/mapper/layout changes");
        expectEquals(countNotesOn(state), 1);

        isKDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!isNoteOn(state, 1, 72), "Original identity (1, 72) must close on last holder release");
        expectEquals(countNotesOn(state), 0);
    }

    void testOverlappingHeldNotesFocusLossReleasesExactlyOnce() {
        beginTest("QUAL-002: Focus loss / releaseAllHeldKeys releases overlapping notes exactly once");

        KeyboardMidiMapper mapper;
        mapper.resetToDefaultLayout();

        juce::MidiKeyboardState state;
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        expect(mapper.handleKeyPressed(juce::KeyPress('k'), state));
        expect(mapper.handleKeyPressed(juce::KeyPress('a'), state));
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 3);
        expectEquals(countNotesOn(state), 2);

        mapper.releaseAllHeldKeys(state);
        expectEquals(countNotesOn(state), 0, "All notes must be released cleanly on focus loss");
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);

        mapper.releaseAllHeldKeys(state);
        expectEquals(countNotesOn(state), 0);
    }

    void testRepeatedAttackInterleavingWhileHeld() {
        beginTest("QUAL-002: Repeated attack interleaving while note held maintains last-holder invariant");

        KeyboardMidiMapper mapper;
        mapper.resetToDefaultLayout();

        bool isQDown = false;
        bool isKDown = false;
        mapper.setKeyStatePredicate([&](int kc) {
            return (kc == makeAlphaNumericKeyCode('Q') && isQDown) || (kc == makeAlphaNumericKeyCode('K') && isKDown);
        });

        juce::MidiKeyboardState state;

        isQDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        isKDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('k'), state));
        expect(isNoteOn(state, 1, 72));
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 2);

        isQDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(isNoteOn(state, 1, 72));
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);

        isQDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        expect(isNoteOn(state, 1, 72));
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 2);

        isKDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(isNoteOn(state, 1, 72));
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);

        isQDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!isNoteOn(state, 1, 72));
        expectEquals(countNotesOn(state), 0);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);
    }
};

// =============================================================================
// KeyboardPartitionAndNumpadTest: Partition modes, numpad input, NumLock, and CC66
// =============================================================================
class KeyboardPartitionAndNumpadTest : public juce::UnitTest {
public:
    KeyboardPartitionAndNumpadTest()
        : juce::UnitTest("KeyboardPartitionAndNumpad", "DevPiano/Input") {
    }

    void runTest() override {
        testPartitionModesRouting();
        testNumpadPitchDefaults();
        testNumLockGatingAndRelease();
        testDownEdgeBarrierOnModeAndGroupSwitch();
        testMutedMatrixNoteDoesNotReleaseOtherSoundingNotes();
    }

private:
    void testPartitionModesRouting() {
        beginTest("Partition Modes Routing and Zone Offsets");

        KeyboardMidiMapper mapper;
        mapper.setKeyStatePredicate([](int) { return false; });
        mapper.setNumLockPredicate([] { return true; });
        juce::MidiKeyboardState state;

        KeyboardLayout layout;
        layout.bindings = { makeNoteBinding('Q', 72, 1),
                            makeNoteBinding('P', 76, 1),
                            { .keyCode = juce::KeyPress::numberPad1,
                              .displayText = "Num 1",
                              .action = { .midiNote = 60, .midiChannel = 1, .velocity = 1.0f } } };
        layout.regionA.transposeOffset = -12; // Octave down for Accompaniment
        layout.regionA.channel = 2; // Channel 2 override
        layout.regionB.transposeOffset = 12; // Octave up for Melody
        layout.regionB.channel = 3; // Channel 3 override
        mapper.setLayout(layout);

        // 1. Off mode: no region transforms applied, numpad keys produce no sound
        mapper.setPartitionMode(KeyboardPartitionMode::off);
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        expect(state.isNoteOn(1, 72));
        // Numpad 1 in off mode should produce no note
        expect(!mapper.handleKeyPressed(juce::KeyPress(juce::KeyPress::numberPad1), state));
        mapper.releaseAllHeldKeys(state);
        expectEquals(countNotesOn(state), 0);

        // 2. MainOnly mode: Q in Area A (transposed -12, ch 2); P in Area B (transposed +12, ch 3)
        mapper.setPartitionMode(KeyboardPartitionMode::mainOnly);
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        expect(state.isNoteOn(2, 60)); // 72 - 12 = 60, channel 2
        expect(mapper.handleKeyPressed(juce::KeyPress('p'), state));
        expect(state.isNoteOn(3, 88));
        // Numpad in mainOnly mode produces no sound
        expect(!mapper.handleKeyPressed(juce::KeyPress(juce::KeyPress::numberPad1), state));
        mapper.releaseAllHeldKeys(state);
        expectEquals(countNotesOn(state), 0);

        // 3. MainAndNumpad mode: Q in Area A (ch 2, -12); Num 1 in Area B (ch 3, +12)
        mapper.setPartitionMode(KeyboardPartitionMode::mainAndNumpad);
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        expect(state.isNoteOn(2, 60));
        // Num 1 default binding is 60 (C4) -> transposed +12 = 72, channel 3
        expect(mapper.handleKeyPressed(juce::KeyPress(juce::KeyPress::numberPad1), state));
        expect(state.isNoteOn(3, 72));
        mapper.releaseAllHeldKeys(state);
        expectEquals(countNotesOn(state), 0);
    }

    void testNumpadPitchDefaults() {
        beginTest("15 Default Numpad Pitches");

        KeyboardMidiMapper mapper;
        mapper.setKeyStatePredicate([](int) { return false; });
        mapper.setNumLockPredicate([] { return true; });
        juce::MidiKeyboardState state;
        mapper.setPartitionMode(KeyboardPartitionMode::mainAndNumpad);

        const struct {
            int keyCode;
            int expectedPitch;
        } kExpected[] = { { juce::KeyPress::numberPad1, 60 },           { juce::KeyPress::numberPad2, 62 },
                          { juce::KeyPress::numberPad3, 64 },           { juce::KeyPress::numberPad4, 65 },
                          { juce::KeyPress::numberPad5, 67 },           { juce::KeyPress::numberPad6, 69 },
                          { juce::KeyPress::numberPad7, 71 },           { juce::KeyPress::numberPad8, 72 },
                          { juce::KeyPress::numberPad9, 74 },           { juce::KeyPress::numberPad0, 76 },
                          { juce::KeyPress::numberPadDivide, 61 },      { juce::KeyPress::numberPadMultiply, 63 },
                          { juce::KeyPress::numberPadSubtract, 66 },    { juce::KeyPress::numberPadAdd, 68 },
                          { juce::KeyPress::numberPadDecimalPoint, 70 } };

        for (const auto& item : kExpected) {
            expect(mapper.handleKeyPressed(juce::KeyPress(item.keyCode), state));
            expect(state.isNoteOn(1, item.expectedPitch));
            mapper.releaseAllHeldKeys(state);
            expectEquals(countNotesOn(state), 0);
        }
    }

    void testNumLockGatingAndRelease() {
        beginTest("NumLock Gating on NoteOn and Safe NoteOff on Release");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;
        mapper.setPartitionMode(KeyboardPartitionMode::mainAndNumpad);

        bool numLock = true;
        mapper.setNumLockPredicate([&] { return numLock; });

        // With NumLock on, Num 1 triggers
        bool isNum1Down = true;
        mapper.setKeyStatePredicate([&](int code) { return code == juce::KeyPress::numberPad1 && isNum1Down; });

        expect(mapper.handleKeyPressed(juce::KeyPress(juce::KeyPress::numberPad1), state));
        expect(state.isNoteOn(1, 60));
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);

        // NumLock turns off while key is held
        numLock = false;
        // An attempt to press another numpad key is suppressed
        expect(!mapper.handleKeyPressed(juce::KeyPress(juce::KeyPress::numberPad2), state));

        // Key release must still safely release Num 1 with original identity!
        isNum1Down = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!state.isNoteOn(1, 60));
        expectEquals(countNotesOn(state), 0);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);
    }

    void testDownEdgeBarrierOnModeAndGroupSwitch() {
        beginTest("Down-edge Barrier Prevents Spurious Retrigger on Mode Switch");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        bool isQDown = true;
        mapper.setKeyStatePredicate([&](int code) { return (code == 'Q' || code == 'q') && isQDown; });

        // Key Q pressed in off mode
        mapper.setPartitionMode(KeyboardPartitionMode::off);
        expect(mapper.handleKeyPressed(juce::KeyPress('q'), state));
        expect(state.isNoteOn(1, 72));
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);

        // User changes partition mode to mainOnly while holding Q
        mapper.setPartitionMode(KeyboardPartitionMode::mainOnly);

        // Key state changed runs: must NOT retrigger or duplicate NoteOn!
        mapper.handleKeyStateChanged(state);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 1);
        expect(state.isNoteOn(1, 72)); // Still sounding original identity

        // Release Q: must cleanly release original note (72 on ch 1)
        isQDown = false;
        expect(mapper.handleKeyStateChanged(state));
        expect(!state.isNoteOn(1, 72));
        expectEquals(countNotesOn(state), 0);
        expectEquals(static_cast<int>(mapper.getNumHeldKeys()), 0);
    }

    void testMutedMatrixNoteDoesNotReleaseOtherSoundingNotes() {
        beginTest("Muted Matrix Note Does Not Trigger NoteOff on Other Active Notes");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        devpiano::midi::ChannelMatrix matrix;
        matrix.active = true;
        // Channel 1 outputChannel is 1 (normal)
        matrix.channels[0].outputChannel = 0;
        // Channel 2 outputChannel is 1, but velocity is 0 (muted)
        matrix.channels[1].outputChannel = 0;
        matrix.channels[1].velocity = 0;
        devpiano::midi::MidiChannelMapper chanMapper(matrix, false, 0);
        mapper.setChannelMapper(&chanMapper);

        // Setup two bindings: key 'A' on ch 1 (audible), key 'B' on ch 2 (muted by matrix)
        KeyboardLayout layout;
        layout.bindings.push_back(makeNoteBinding('A', 60, 1));
        layout.bindings.push_back(makeNoteBinding('B', 60, 2));
        mapper.setLayout(layout);

        bool isADown = true;
        bool isBDown = false;
        mapper.setKeyStatePredicate([&](int code) {
            if (code == 'A' || code == 'a') {
                return isADown;
            }
            if (code == 'B' || code == 'b') {
                return isBDown;
            }
            return false;
        });

        // 1. Play 'A' -> sounds on ch 1, note 60
        expect(mapper.handleKeyPressed(juce::KeyPress('a'), state));
        expect(state.isNoteOn(1, 60));

        // 2. Press 'B' -> muted by channel 2 matrix velocity 0
        isBDown = true;
        expect(mapper.handleKeyPressed(juce::KeyPress('b'), state));
        expect(state.isNoteOn(1, 60)); // Still sounding 'A'

        // 3. Release 'B' -> must NOT send NoteOff for note 60 on ch 1, preserving 'A'
        isBDown = false;
        mapper.handleKeyStateChanged(state);
        expect(state.isNoteOn(1, 60)); // 'A' remains sounding!

        // 4. Release 'A' -> cleanly stops
        isADown = false;
        mapper.handleKeyStateChanged(state);
        expect(!state.isNoteOn(1, 60));
        expectEquals(countNotesOn(state), 0);
    }
};

static KeyboardPartitionAndNumpadTest keyboardPartitionAndNumpadTest;

static LayoutGroupAndHeldKeyIdentityTest layoutGroupAndHeldKeyIdentityTest;
static SustainPedalKeyMappingTest sustainPedalKeyMappingTest;

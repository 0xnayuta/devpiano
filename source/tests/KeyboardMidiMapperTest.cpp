#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"
#include "Input/KeyboardMidiMapper.h"

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

class LayoutManagementTest : public juce::UnitTest {
public:
    LayoutManagementTest()
        : juce::UnitTest("KeyboardMidiMapper: layout management", "DevPiano/Engine") {
    }

    void runTest() override {
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

            // ID 不变。
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
            expect(!consumed, "no held keys → no consumption");
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

static SustainPedalKeyMappingTest sustainPedalKeyMappingTest;

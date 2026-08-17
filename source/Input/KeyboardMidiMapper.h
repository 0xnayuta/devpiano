#pragma once

#include <JuceHeader.h>

#include <functional>
#include <unordered_set>

#include "Core/KeyMapTypes.h"

namespace devpiano::midi {
class MidiChannelMapper;
}

class KeyboardMidiMapper {
public:
    /// 键当前状态查询谓词（TEST-015）：handleKeyStateChanged 用它判断按键当前
    /// 是否被按住。默认委托 juce::KeyPress::isKeyCurrentlyDown() 查询真实 OS
    /// 键盘状态；测试可注入确定性谓词，消除桌面环境物理按键导致的误报。
    using KeyStatePredicate = std::function<bool(int keyCode)>;

    KeyboardMidiMapper();

    void setLayout(devpiano::core::KeyboardLayout newLayout);
    void setLayoutDisplayName(juce::String newDisplayName);
    [[nodiscard]] const devpiano::core::KeyboardLayout& getLayout() const noexcept;
    void resetToDefaultLayout();

    bool handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState);
    bool handleKeyStateChanged(juce::MidiKeyboardState& keyboardState);
    void setChannelMapper(devpiano::midi::MidiChannelMapper* mapper) noexcept;

    /// 注入键状态谓词（测试用）：null/未设置时回退真实 OS 键盘查询。
    void setKeyStatePredicate(KeyStatePredicate predicate) noexcept;

private:
    [[nodiscard]] int normaliseKeyCode(const juce::KeyPress& key) const;
    bool triggerBinding(const devpiano::core::KeyBinding& binding, juce::MidiKeyboardState& keyboardState,
                        bool isKeyDownEvent);
    void sendNoteOff(int midiChannel, int midiNote, float velocity, juce::MidiKeyboardState& keyboardState);
    [[nodiscard]] bool isKeyCurrentlyDown(int keyCode) const;

    devpiano::midi::MidiChannelMapper* channelMapper = nullptr;
    devpiano::core::KeyboardLayout layout;
    std::unordered_set<int> heldKeys;
    KeyStatePredicate keyStatePredicate;
};

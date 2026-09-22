#include "KeyboardMidiMapper.h"

#include "Midi/MidiChannelMapper.h"

using namespace devpiano::core;

KeyboardMidiMapper::KeyboardMidiMapper() {
    resetToDefaultLayout();
}

void KeyboardMidiMapper::setLayout(KeyboardLayout newLayout) {
    layout = std::move(newLayout);
    heldKeys.clear();
    if (sustainPedalDown) {
        sustainPedalDown = false;
        if (sustainPedalCallback) {
            sustainPedalCallback(false);
        }
    }
    physicalSoftPedalHeld = false;
    updateSoftPedalState();
}

void KeyboardMidiMapper::setLayoutDisplayName(juce::String newDisplayName) {
    layout.name = std::move(newDisplayName);
}

const KeyboardLayout& KeyboardMidiMapper::getLayout() const noexcept {
    return layout;
}

void KeyboardMidiMapper::setChannelMapper(devpiano::midi::MidiChannelMapper* mapper) noexcept {
    channelMapper = mapper;
}
void KeyboardMidiMapper::setSustainPedalCallback(SustainPedalCallback callback) noexcept {
    sustainPedalCallback = std::move(callback);
}

bool KeyboardMidiMapper::isSustainPedalDown() const noexcept {
    return sustainPedalDown;
}
void KeyboardMidiMapper::setSoftPedalCallback(SoftPedalCallback callback) noexcept {
    softPedalCallback = std::move(callback);
}

bool KeyboardMidiMapper::isSoftPedalDown() const noexcept {
    return softPedalDown;
}
void KeyboardMidiMapper::setSoftPedalDown(bool down) {
    programmaticSoftPedal = down;
    updateSoftPedalState();
}

void KeyboardMidiMapper::updateSoftPedalState() {
    const auto target = physicalSoftPedalHeld || programmaticSoftPedal;
    if (softPedalDown == target) {
        return;
    }
    softPedalDown = target;
    if (softPedalCallback != nullptr) {
        softPedalCallback(softPedalDown);
    }
}
void KeyboardMidiMapper::setTouchVelocityCurve(devpiano::input::TouchVelocityCurve curve) noexcept {
    touchVelocityCurve = curve;
}

devpiano::input::TouchVelocityCurve KeyboardMidiMapper::getTouchVelocityCurve() const noexcept {
    return touchVelocityCurve;
}
void KeyboardMidiMapper::resetToDefaultLayout() {
    setLayout(makeDefaultKeyboardLayout());
}

void KeyboardMidiMapper::setActiveGroupIndex(uint8_t groupIndex) {
    const auto target = static_cast<uint8_t>(groupIndex % 4);
    if (layout.activeGroupIndex == target) {
        return;
    }
    layout.activeGroupIndex = target;
    if (groupChangeCallback != nullptr) {
        groupChangeCallback(layout.activeGroupIndex);
    }
}

uint8_t KeyboardMidiMapper::getActiveGroupIndex() const noexcept {
    return layout.activeGroupIndex;
}

void KeyboardMidiMapper::switchToNextGroup() {
    setActiveGroupIndex((layout.activeGroupIndex + 1) % 4);
}

void KeyboardMidiMapper::switchToPreviousGroup() {
    setActiveGroupIndex((layout.activeGroupIndex + 3) % 4);
}

const devpiano::core::KeyGroup& KeyboardMidiMapper::getActiveGroup() const noexcept {
    return layout.getActiveGroup();
}

void KeyboardMidiMapper::setGroupChangeCallback(GroupChangeCallback callback) noexcept {
    groupChangeCallback = std::move(callback);
}

bool KeyboardMidiMapper::isKeyHeld(int keyCode) const noexcept {
    return findHeldKey(keyCode) != nullptr;
}

const devpiano::core::HeldKeyIdentity* KeyboardMidiMapper::findHeldKey(int keyCode) const noexcept {
    for (const auto& held : heldKeys) {
        if (held.physicalKeyCode == keyCode) {
            return &held;
        }
    }
    return nullptr;
}

size_t KeyboardMidiMapper::getNumHeldKeys() const noexcept {
    return heldKeys.size();
}

bool KeyboardMidiMapper::handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState) {
    const auto isShift = key.getModifiers().isShiftDown();
    const auto isSpace = (key.getKeyCode() == juce::KeyPress::spaceKey || key.getTextCharacter() == ' ');
    const auto isTab = (key.getKeyCode() == juce::KeyPress::tabKey);

    if (isTab || (isShift && isSpace)) {
        physicalSoftPedalHeld = true;
        updateSoftPedalState();
        return true;
    }

    if (isSpace && !isShift) {
        if (!sustainPedalDown) {
            sustainPedalDown = true;
            if (sustainPedalCallback) {
                sustainPedalCallback(true);
            }
        }
        return true;
    }
    // 支持反引号 ` 键作为快捷切组键（在未绑定音符时有效）
    if (key.getKeyCode() == '`' || key.getTextCharacter() == '`') {
        if (layout.findByKeyCode('`') == nullptr) {
            switchToNextGroup();
            return true;
        }
    }

    const auto keyCode = normaliseKeyCode(key);
    if (keyCode == 0) {
        return false;
    }

    const auto* binding = layout.findByKeyCode(keyCode);
    if (binding == nullptr) {
        return false;
    }

    if (isKeyHeld(keyCode)) {
        return true;
    }

    return triggerBinding(*binding, keyboardState, true);
}

bool KeyboardMidiMapper::handleKeyStateChanged(juce::MidiKeyboardState& keyboardState) {
    auto consumed = false;

    const auto isSpaceDown = isKeyCurrentlyDown(juce::KeyPress::spaceKey);
    const auto isTabDown = isKeyCurrentlyDown(juce::KeyPress::tabKey);
    const auto isShiftDown = juce::ModifierKeys::getCurrentModifiers().isShiftDown();

    // 1. 物理软踏板检测: Tab 键或 Shift+Space 组合
    const auto physicalSoftActive = isTabDown || (isSpaceDown && isShiftDown);
    if (physicalSoftActive != physicalSoftPedalHeld) {
        physicalSoftPedalHeld = physicalSoftActive;
        updateSoftPedalState();
        consumed = true;
    }

    // 2. 物理延音踏板检测: 仅当 Space 按下且未按住 Shift 时激活，避免 Shift+Space 误激活延音
    const auto physicalSustainActive = isSpaceDown && !isShiftDown;
    if (physicalSustainActive && !sustainPedalDown) {
        sustainPedalDown = true;
        if (sustainPedalCallback) {
            sustainPedalCallback(true);
        }
        consumed = true;
    } else if (!physicalSustainActive && sustainPedalDown) {
        sustainPedalDown = false;
        if (sustainPedalCallback) {
            sustainPedalCallback(false);
        }
        consumed = true;
    }
    for (const auto& binding : layout.bindings) {
        const auto keyCode = binding.keyCode;
        if (keyCode == 0) {
            continue;
        }

        const auto isCurrentlyDown = isKeyCurrentlyDown(keyCode);

        const auto wasHeld = isKeyHeld(keyCode);

        if (isCurrentlyDown && !wasHeld) {
            consumed = triggerBinding(binding, keyboardState, true) || consumed;
            continue;
        }

        if (!isCurrentlyDown && wasHeld) {
            if (binding.action.type == KeyActionType::note) {
                if (const auto* held = findHeldKey(keyCode)) {
                    sendNoteOff(held->soundingMidiChannel, held->soundingMidiNote, held->velocity, keyboardState);
                    std::erase_if(heldKeys, [keyCode](const auto& h) { return h.physicalKeyCode == keyCode; });
                    consumed = true;
                }
            } else {
                consumed = triggerBinding(binding, keyboardState, false) || consumed;
            }
        }
    }

    // 额外防呆：检查 heldKeys 中由于切组或绑定删除而成为孤儿的按键
    for (auto it = heldKeys.begin(); it != heldKeys.end();) {
        if (!isKeyCurrentlyDown(it->physicalKeyCode)) {
            sendNoteOff(it->soundingMidiChannel, it->soundingMidiNote, it->velocity, keyboardState);
            it = heldKeys.erase(it);
            consumed = true;
        } else {
            ++it;
        }
    }

    return consumed;
}
void KeyboardMidiMapper::releaseAllHeldKeys(juce::MidiKeyboardState& keyboardState) {
    for (const auto& held : heldKeys) {
        sendNoteOff(held.soundingMidiChannel, held.soundingMidiNote, held.velocity, keyboardState);
    }
    heldKeys.clear();
    if (sustainPedalDown) {
        sustainPedalDown = false;
        if (sustainPedalCallback) {
            sustainPedalCallback(false);
        }
    }
    physicalSoftPedalHeld = false;
    programmaticSoftPedal = false;
    updateSoftPedalState();
}

int KeyboardMidiMapper::normaliseKeyCode(const juce::KeyPress& key) const {
    return normaliseAlphaNumericKeyCode(key.getKeyCode());
}

bool KeyboardMidiMapper::triggerBinding(const KeyBinding& binding, juce::MidiKeyboardState& keyboardState,
                                        bool isKeyDownEvent) {
    const auto expectedTrigger = isKeyDownEvent ? KeyTrigger::keyDown : KeyTrigger::keyUp;
    if (binding.action.trigger != expectedTrigger) {
        return false;
    }

    if (binding.action.type != KeyActionType::note) {
        return false;
    }

    const auto rawVelocity = binding.action.getVelocity().value;
    const auto velocity
        = isKeyDownEvent ? devpiano::input::applyVelocityCurve(rawVelocity, touchVelocityCurve) : rawVelocity;

    if (isKeyDownEvent) {
        // 计算当前激活 Group 下的发声音高与通道
        const auto soundingNote
            = devpiano::core::calculateSoundingNote(binding.action.getMidiNoteNumber().value, layout.getActiveGroup());
        const auto soundingChannel
            = devpiano::core::calculateSoundingChannel(binding.action.getMidiChannel().value, layout.getActiveGroup());

        if (channelMapper != nullptr) {
            channelMapper->sendNoteOn(devpiano::core::MidiChannel::fromClamped(soundingChannel).toZeroBased(),
                                      soundingNote, velocity, keyboardState);
        } else {
            keyboardState.noteOn(soundingChannel, soundingNote, velocity);
        }

        // 记录发音身份快照，严格保护 NoteOff 一致性
        heldKeys.push_back({ binding.keyCode, soundingNote, soundingChannel, velocity });
    } else {
        // NoteOff：优先依据按下时记录的快照注销
        if (const auto* held = findHeldKey(binding.keyCode)) {
            sendNoteOff(held->soundingMidiChannel, held->soundingMidiNote, held->velocity, keyboardState);
            std::erase_if(heldKeys, [k = binding.keyCode](const auto& h) { return h.physicalKeyCode == k; });
        } else {
            const auto soundingNote = devpiano::core::calculateSoundingNote(binding.action.getMidiNoteNumber().value,
                                                                            layout.getActiveGroup());
            const auto soundingChannel = devpiano::core::calculateSoundingChannel(binding.action.getMidiChannel().value,
                                                                                  layout.getActiveGroup());
            sendNoteOff(soundingChannel, soundingNote, rawVelocity, keyboardState);
        }
    }

    return true;
}

void KeyboardMidiMapper::sendNoteOff(int midiChannel, int midiNote, float velocity,
                                     juce::MidiKeyboardState& keyboardState) {
    if (channelMapper != nullptr) {
        channelMapper->sendNoteOff(devpiano::core::MidiChannel::fromClamped(midiChannel).toZeroBased(), midiNote,
                                   velocity, keyboardState);
    } else {
        keyboardState.noteOff(midiChannel, midiNote, velocity);
    }
}

void KeyboardMidiMapper::setKeyStatePredicate(KeyStatePredicate predicate) noexcept {
    keyStatePredicate = std::move(predicate);
}

bool KeyboardMidiMapper::isKeyCurrentlyDown(int keyCode) const {
    // 注入的谓词优先；未注入时回退真实 OS 键盘状态（生产行为）。
    if (keyStatePredicate) {
        return keyStatePredicate(keyCode);
    }
    return juce::KeyPress::isKeyCurrentlyDown(keyCode);
}

devpiano::core::QwertyViewModel KeyboardMidiMapper::createQwertySnapshot(int keySignature) const {
    auto vm = devpiano::core::makeDefaultQwertyLayoutTemplate();
    vm.isSustainPedalDown = sustainPedalDown;
    vm.isSoftPedalDown = softPedalDown;
    vm.activeGroupIndex = layout.activeGroupIndex;
    vm.activeGroupName = layout.getActiveGroup().name;

    const auto& activeGroup = layout.getActiveGroup();

    for (auto& row : vm.rows) {
        for (auto& key : row.keys) {
            if (key.isSustainPedal) {
                key.isDown = sustainPedalDown;
            } else if (key.isSoftPedal) {
                key.isDown = softPedalDown;
            } else if (key.keyCode != 0) {
                key.isDown = isKeyHeld(key.keyCode);
            }

            if (key.keyCode != 0) {
                if (const auto* binding = layout.findByKeyCode(key.keyCode)) {
                    if (binding->action.type == devpiano::core::KeyActionType::note) {
                        // 依据当前 Group 实时计算音符投影
                        key.mappedMidiNote
                            = devpiano::core::calculateSoundingNote(binding->action.midiNote, activeGroup);
                        key.mappedMidiChannel
                            = devpiano::core::calculateSoundingChannel(binding->action.midiChannel, activeGroup);
                        key.velocity = binding->action.velocity;

                        key.noteName = devpiano::core::getNoteDisplayName(
                            key.mappedMidiNote, devpiano::core::NoteDisplayMode::noteName, keySignature);
                        key.solfegeLabel = devpiano::core::getNoteDisplayName(
                            key.mappedMidiNote, devpiano::core::NoteDisplayMode::fixedDo, keySignature);
                    }
                }
            }
        }
    }

    return vm;
}

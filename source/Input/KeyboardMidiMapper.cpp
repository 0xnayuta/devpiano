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

    const auto keyCode = normaliseKeyCode(key);
    if (keyCode == 0) {
        return false;
    }

    const auto* binding = layout.findByKeyCode(keyCode);
    if (binding == nullptr) {
        return false;
    }

    if (!heldKeys.insert(keyCode).second) {
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

        const auto wasHeld = heldKeys.contains(keyCode);

        if (isCurrentlyDown && !wasHeld) {
            heldKeys.insert(keyCode);
            consumed = triggerBinding(binding, keyboardState, true) || consumed;
            continue;
        }

        if (!isCurrentlyDown && wasHeld) {
            if (binding.action.type == KeyActionType::note) {
                const auto midiChannel = binding.action.getMidiChannel().value;
                const auto midiNote = binding.action.getMidiNoteNumber().value;
                const auto velocity = binding.action.getVelocity().value;
                sendNoteOff(midiChannel, midiNote, velocity, keyboardState);
                consumed = true;
            } else {
                consumed = triggerBinding(binding, keyboardState, false) || consumed;
            }

            heldKeys.erase(keyCode);
        }
    }

    return consumed;
}
void KeyboardMidiMapper::releaseAllHeldKeys(juce::MidiKeyboardState& keyboardState) {
    for (const auto keyCode : heldKeys) {
        if (const auto* binding = layout.findByKeyCode(keyCode)) {
            if (binding->action.type == KeyActionType::note) {
                const auto midiChannel = binding->action.getMidiChannel().value;
                const auto midiNote = binding->action.getMidiNoteNumber().value;
                const auto velocity = binding->action.getVelocity().value;
                sendNoteOff(midiChannel, midiNote, velocity, keyboardState);
            }
        }
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

    const auto midiChannel = binding.action.getMidiChannel().value; // 1-based
    const auto midiNote = binding.action.getMidiNoteNumber().value;
    const auto rawVelocity = binding.action.getVelocity().value;
    const auto velocity
        = isKeyDownEvent ? devpiano::input::applyVelocityCurve(rawVelocity, touchVelocityCurve) : rawVelocity;

    if (channelMapper != nullptr) {
        // Convert 1-based binding channel to 0-based matrix input channel
        if (isKeyDownEvent) {
            channelMapper->sendNoteOn(midiChannel - 1, midiNote, velocity, keyboardState);
        } else {
            sendNoteOff(midiChannel, midiNote, velocity, keyboardState);
        }
    } else {
        if (isKeyDownEvent) {
            keyboardState.noteOn(midiChannel, midiNote, velocity);
        } else {
            sendNoteOff(midiChannel, midiNote, velocity, keyboardState);
        }
    }

    return true;
}

void KeyboardMidiMapper::sendNoteOff(int midiChannel, int midiNote, float velocity,
                                     juce::MidiKeyboardState& keyboardState) {
    if (channelMapper != nullptr) {
        channelMapper->sendNoteOff(midiChannel - 1, midiNote, velocity, keyboardState);
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

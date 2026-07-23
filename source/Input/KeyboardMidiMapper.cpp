#include "KeyboardMidiMapper.h"

#include "Midi/MidiChannelMapper.h"

using namespace devpiano::core;

KeyboardMidiMapper::KeyboardMidiMapper() {
    resetToDefaultLayout();
}

void KeyboardMidiMapper::setLayout(KeyboardLayout newLayout) {
    layout = std::move(newLayout);
    heldKeys.clear();
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

void KeyboardMidiMapper::resetToDefaultLayout() {
    setLayout(makeDefaultKeyboardLayout());
}

bool KeyboardMidiMapper::handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState) {
    const auto keyCode = normaliseKeyCode(key);
    if (keyCode == 0)
        return false;

    const auto* binding = layout.findByKeyCode(keyCode);
    if (binding == nullptr)
        return false;

    if (!heldKeys.insert(keyCode).second)
        return true;

    return triggerBinding(*binding, keyboardState, true);
}

bool KeyboardMidiMapper::handleKeyStateChanged(juce::MidiKeyboardState& keyboardState) {
    auto consumed = false;

    for (const auto& binding : layout.bindings) {
        const auto keyCode = binding.keyCode;
        if (keyCode == 0)
            continue;

        const auto isCurrentlyDown = juce::KeyPress::isKeyCurrentlyDown(keyCode);

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

int KeyboardMidiMapper::normaliseKeyCode(const juce::KeyPress& key) const {
    return normaliseAlphaNumericKeyCode(key.getKeyCode());
}

bool KeyboardMidiMapper::triggerBinding(const KeyBinding& binding, juce::MidiKeyboardState& keyboardState,
                                        bool isKeyDownEvent) {
    const auto expectedTrigger = isKeyDownEvent ? KeyTrigger::keyDown : KeyTrigger::keyUp;
    if (binding.action.trigger != expectedTrigger)
        return false;

    if (binding.action.type != KeyActionType::note)
        return false;

    const auto midiChannel = binding.action.getMidiChannel().value; // 1-based
    const auto midiNote = binding.action.getMidiNoteNumber().value;
    const auto velocity = binding.action.getVelocity().value;

    if (channelMapper != nullptr) {
        // Convert 1-based binding channel to 0-based matrix input channel
        if (isKeyDownEvent)
            channelMapper->sendNoteOn(midiChannel - 1, midiNote, velocity, keyboardState);
        else
            sendNoteOff(midiChannel, midiNote, velocity, keyboardState);
    } else {
        if (isKeyDownEvent)
            keyboardState.noteOn(midiChannel, midiNote, velocity);
        else
            sendNoteOff(midiChannel, midiNote, velocity, keyboardState);
    }

    return true;
}

void KeyboardMidiMapper::sendNoteOff(int midiChannel, int midiNote, float velocity,
                                     juce::MidiKeyboardState& keyboardState) {
    if (channelMapper != nullptr)
        channelMapper->sendNoteOff(midiChannel - 1, midiNote, velocity, keyboardState);
    else
        keyboardState.noteOff(midiChannel, midiNote, velocity);
}

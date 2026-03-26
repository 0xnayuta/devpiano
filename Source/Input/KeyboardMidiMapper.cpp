#include "KeyboardMidiMapper.h"

using namespace devpiano::core;

KeyboardMidiMapper::KeyboardMidiMapper()
{
    resetToDefaultLayout();
}

void KeyboardMidiMapper::setLayout(KeyboardLayout newLayout)
{
    layout = std::move(newLayout);
    heldKeys.clear();
}

const KeyboardLayout& KeyboardMidiMapper::getLayout() const noexcept
{
    return layout;
}

void KeyboardMidiMapper::resetToDefaultLayout()
{
    setLayout(makeDefaultKeyboardLayout());
}

bool KeyboardMidiMapper::handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState)
{
    const auto keyCode = normaliseKeyCode(key);
    if (keyCode == 0)
        return false;

    const auto* binding = layout.findByKeyCode(keyCode);
    if (binding == nullptr)
        return false;

    if (! heldKeys.insert(keyCode).second)
        return true;

    return triggerBinding(*binding, keyboardState, true);
}

bool KeyboardMidiMapper::handleKeyStateChanged(juce::MidiKeyboardState& keyboardState)
{
    auto consumed = false;

    for (const auto& binding : layout.bindings)
    {
        const auto keyCode = binding.keyCode;
        if (keyCode == 0)
            continue;

        const auto isCurrentlyDown = juce::KeyPress::isKeyCurrentlyDown(keyCode);
        const auto wasHeld = heldKeys.contains(keyCode);

        if (isCurrentlyDown && ! wasHeld)
        {
            heldKeys.insert(keyCode);
            consumed = triggerBinding(binding, keyboardState, true) || consumed;
            continue;
        }

        if (! isCurrentlyDown && wasHeld)
        {
            if (binding.action.type == KeyActionType::note)
            {
                const auto midiChannel = juce::jlimit(1, 16, binding.action.midiChannel);
                const auto midiNote = juce::jlimit(0, 127, binding.action.midiNote);
                const auto velocity = juce::jlimit(0.0f, 1.0f, binding.action.velocity);
                keyboardState.noteOff(midiChannel, midiNote, velocity);
                consumed = true;
            }
            else
            {
                consumed = triggerBinding(binding, keyboardState, false) || consumed;
            }

            heldKeys.erase(keyCode);
        }
    }

    return consumed;
}

int KeyboardMidiMapper::normaliseKeyCode(const juce::KeyPress& key) const
{
    return normaliseAlphaNumericKeyCode(key.getKeyCode());
}

bool KeyboardMidiMapper::triggerBinding(const KeyBinding& binding,
                                        juce::MidiKeyboardState& keyboardState,
                                        bool isKeyDownEvent)
{
    const auto expectedTrigger = isKeyDownEvent ? KeyTrigger::keyDown : KeyTrigger::keyUp;
    if (binding.action.trigger != expectedTrigger)
        return false;

    if (binding.action.type != KeyActionType::note)
        return false;

    const auto midiChannel = juce::jlimit(1, 16, binding.action.midiChannel);
    const auto midiNote = juce::jlimit(0, 127, binding.action.midiNote);
    const auto velocity = juce::jlimit(0.0f, 1.0f, binding.action.velocity);

    if (isKeyDownEvent)
        keyboardState.noteOn(midiChannel, midiNote, velocity);
    else
        keyboardState.noteOff(midiChannel, midiNote, velocity);

    return true;
}

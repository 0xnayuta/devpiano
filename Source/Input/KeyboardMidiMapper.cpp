#include "KeyboardMidiMapper.h"

#include <cctype>

KeyboardMidiMapper::KeyboardMidiMapper()
{
    initialiseDefaultMap();
}

bool KeyboardMidiMapper::handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState)
{
    const auto character = key.getTextCharacter();
    const auto keyCode = normaliseKeyCode(character);
    if (keyCode == 0)
        return false;

    const auto it = keyToNote.find(keyCode);
    if (it == keyToNote.end())
        return false;

    if (heldKeys.insert(keyCode).second)
        keyboardState.noteOn(1, it->second, 1.0f);

    return true;
}

bool KeyboardMidiMapper::handleKeyStateChanged(juce::MidiKeyboardState& keyboardState)
{
    auto consumed = false;

    for (auto it = heldKeys.begin(); it != heldKeys.end();)
    {
        const auto keyCode = *it;
        if (! juce::KeyPress::isKeyCurrentlyDown(keyCode))
        {
            if (const auto noteIt = keyToNote.find(keyCode); noteIt != keyToNote.end())
            {
                keyboardState.noteOff(1, noteIt->second, 1.0f);
                consumed = true;
            }

            it = heldKeys.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return consumed;
}

void KeyboardMidiMapper::initialiseDefaultMap()
{
    auto addMapping = [this](char character, int midiNote)
    {
        const auto upper = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        const auto keyCode = juce::KeyPress(static_cast<int>(upper), 0, 0).getKeyCode();
        keyToNote[keyCode] = midiNote;
    };

    const auto c5 = baseC123Row;
    addMapping('1', c5 + 0);
    addMapping('2', c5 + 2);
    addMapping('3', c5 + 4);
    addMapping('4', c5 + 5);
    addMapping('5', c5 + 7);
    addMapping('6', c5 + 9);
    addMapping('7', c5 + 11);
    addMapping('8', c5 + 12);
    addMapping('9', c5 + 14);
    addMapping('0', c5 + 16);

    const auto c4 = baseCQweRow;
    addMapping('Q', c4 + 0);
    addMapping('W', c4 + 2);
    addMapping('E', c4 + 4);
    addMapping('R', c4 + 5);
    addMapping('T', c4 + 7);
    addMapping('Y', c4 + 9);
    addMapping('U', c4 + 11);
    addMapping('I', c5 + 0);
    addMapping('O', c5 + 2);
    addMapping('P', c5 + 4);

    const auto c3 = baseCAsdRow;
    addMapping('A', c3 + 0);
    addMapping('S', c3 + 2);
    addMapping('D', c3 + 4);
    addMapping('F', c3 + 5);
    addMapping('G', c3 + 7);
    addMapping('H', c3 + 9);
    addMapping('J', c3 + 11);
    addMapping('K', c4 + 0);
    addMapping('L', c4 + 2);

    const auto c2 = baseCZxcRow;
    addMapping('Z', c2 + 0);
    addMapping('X', c2 + 2);
    addMapping('C', c2 + 4);
    addMapping('V', c2 + 5);
    addMapping('B', c2 + 7);
    addMapping('N', c2 + 9);
    addMapping('M', c2 + 11);
}

int KeyboardMidiMapper::normaliseKeyCode(juce_wchar character) const
{
    if (! std::isalnum(static_cast<unsigned char>(character)))
        return 0;

    const auto upper = static_cast<int>(std::toupper(static_cast<unsigned char>(character)));
    return juce::KeyPress(upper, 0, 0).getKeyCode();
}

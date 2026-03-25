#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <unordered_set>

class KeyboardMidiMapper
{
public:
    KeyboardMidiMapper();

    bool handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState);
    bool handleKeyStateChanged(juce::MidiKeyboardState& keyboardState);

private:
    void initialiseDefaultMap();
    int normaliseKeyCode(juce_wchar character) const;

    std::unordered_map<int, int> keyToNote;
    std::unordered_set<int> heldKeys;

    int baseC123Row = 84;
    int baseCQweRow = 72;
    int baseCAsdRow = 60;
    int baseCZxcRow = 48;
};

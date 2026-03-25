#pragma once
#include <JuceHeader.h>
#include <unordered_map>

struct SettingsModel
{
    // Audio device state (serialized XML from AudioDeviceManager)
    std::unique_ptr<juce::XmlElement> audioDeviceState;

    // General audio params (mirrors device when applicable)
    double sampleRate = 44100.0;
    int bufferSize = 512;

    // UI synth params
    float masterGain = 0.8f;
    float adsrAttack = 0.01f;
    float adsrDecay = 0.20f;
    float adsrSustain = 0.80f;
    float adsrRelease = 0.30f;

    // Physical key to MIDI note map (stores KeyPress keyCodes to midi notes)
    std::unordered_map<int,int> keyMap;

    // Convert keyMap to a ValueTree for persistence
    static juce::ValueTree keyMapToValueTree(const std::unordered_map<int,int>& m)
    {
        juce::ValueTree t{"keymap"};
        for (auto& kv : m)
        {
            juce::ValueTree n{"k"};
            n.setProperty("code", kv.first, nullptr);
            n.setProperty("note", kv.second, nullptr);
            t.appendChild(n, nullptr);
        }
        return t;
    }

    static std::unordered_map<int,int> valueTreeToKeyMap(const juce::ValueTree& t)
    {
        std::unordered_map<int,int> m;
        if (! t.isValid()) return m;
        for (int i = 0; i < t.getNumChildren(); ++i)
        {
            auto c = t.getChild(i);
            int code = (int) c.getProperty("code", 0);
            int note = (int) c.getProperty("note", -1);
            if (note >= 0) m[code] = note;
        }
        return m;
    }
};

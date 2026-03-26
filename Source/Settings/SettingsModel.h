#pragma once

#include <JuceHeader.h>

#include <unordered_map>

#include "Core/KeyMapTypes.h"

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

    // Physical key to MIDI note map (legacy persistence shape, still used as current on-disk format)
    std::unordered_map<int, int> keyMap;

    static juce::ValueTree keyMapToValueTree(const std::unordered_map<int, int>& m)
    {
        juce::ValueTree t { "keymap" };
        for (const auto& kv : m)
        {
            juce::ValueTree n { "k" };
            n.setProperty("code", kv.first, nullptr);
            n.setProperty("note", kv.second, nullptr);
            t.appendChild(n, nullptr);
        }
        return t;
    }

    static std::unordered_map<int, int> valueTreeToKeyMap(const juce::ValueTree& t)
    {
        std::unordered_map<int, int> m;
        if (! t.isValid())
            return m;

        for (int i = 0; i < t.getNumChildren(); ++i)
        {
            auto c = t.getChild(i);
            const auto code = static_cast<int>(c.getProperty("code", 0));
            const auto note = static_cast<int>(c.getProperty("note", -1));
            if (note >= 0)
                m[code] = note;
        }

        return m;
    }

    static std::unordered_map<int, int> layoutToKeyMap(const devpiano::core::KeyboardLayout& layout)
    {
        std::unordered_map<int, int> map;

        for (const auto& binding : layout.bindings)
        {
            if (binding.action.type != devpiano::core::KeyActionType::note)
                continue;

            if (binding.action.trigger != devpiano::core::KeyTrigger::keyDown)
                continue;

            map[binding.keyCode] = binding.action.midiNote;
        }

        return map;
    }

    static devpiano::core::KeyboardLayout keyMapToLayout(const std::unordered_map<int, int>& map)
    {
        auto layout = devpiano::core::makeDefaultKeyboardLayout();
        if (map.empty())
            return layout;

        for (auto& binding : layout.bindings)
        {
            if (binding.action.type != devpiano::core::KeyActionType::note)
                continue;

            if (const auto it = map.find(binding.keyCode); it != map.end())
                binding.action.midiNote = it->second;
        }

        return layout;
    }
};

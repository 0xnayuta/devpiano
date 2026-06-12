#include "Settings/SettingsSerialization.h"

#include "Layout/LayoutDirectoryScanner.h"
#include "Layout/LayoutPreset.h"

namespace devpiano::settings {

juce::ValueTree channelMatrixToValueTree(const devpiano::midi::ChannelMatrix& cm) {
    juce::ValueTree root { "channelMatrix" };
    root.setProperty("active", cm.active, nullptr);
    for (int i = 0; i < 16; ++i) {
        juce::ValueTree ch { "ch" };
        auto& c = cm.channels[i];
        ch.setProperty("oc", c.outputChannel, nullptr);
        ch.setProperty("tr", c.transpose, nullptr);
        ch.setProperty("os", c.octaveShift, nullptr);
        ch.setProperty("ve", c.velocity, nullptr);
        ch.setProperty("pg", c.program, nullptr);
        ch.setProperty("bn", c.bankMSB, nullptr);
        ch.setProperty("sc", c.sustainCC, nullptr);
        root.appendChild(ch, nullptr);
    }
    return root;
}

devpiano::midi::ChannelMatrix valueTreeToChannelMatrix(const juce::ValueTree& t) {
    devpiano::midi::ChannelMatrix cm;
    if (!t.isValid())
        return cm;
    cm.active = t.getProperty("active", false);
    for (int i = 0; i < 16 && i < t.getNumChildren(); ++i) {
        auto ch = t.getChild(i);
        auto& c = cm.channels[i];
        c.outputChannel = static_cast<uint8_t>(static_cast<int>(ch.getProperty("oc", 0)));
        c.transpose = static_cast<int8_t>(static_cast<int>(ch.getProperty("tr", 0)));
        c.octaveShift = static_cast<int8_t>(static_cast<int>(ch.getProperty("os", 0)));
        c.velocity = static_cast<uint8_t>(static_cast<int>(ch.getProperty("ve", 64)));
        c.program = static_cast<uint8_t>(static_cast<int>(ch.getProperty("pg", 0)));
        c.bankMSB = static_cast<uint8_t>(static_cast<int>(ch.getProperty("bn", 0)));
        c.sustainCC = static_cast<uint8_t>(static_cast<int>(ch.getProperty("sc", 64)));
    }
    return cm;
}

juce::ValueTree keyMapToValueTree(const std::unordered_map<int, int>& m) {
    juce::ValueTree t { "keymap" };
    for (const auto& kv : m) {
        juce::ValueTree n { "k" };
        n.setProperty("code", kv.first, nullptr);
        n.setProperty("note", kv.second, nullptr);
        t.appendChild(n, nullptr);
    }
    return t;
}

std::unordered_map<int, int> valueTreeToKeyMap(const juce::ValueTree& t) {
    std::unordered_map<int, int> m;
    if (!t.isValid())
        return m;

    for (int i = 0; i < t.getNumChildren(); ++i) {
        auto c = t.getChild(i);
        const auto code = static_cast<int>(c.getProperty("code", 0));
        const auto note = static_cast<int>(c.getProperty("note", -1));
        if (note >= 0)
            m[code] = note;
    }

    return m;
}

std::unordered_map<int, int> layoutToKeyMap(const devpiano::core::KeyboardLayout& layout) {
    std::unordered_map<int, int> map;

    for (const auto& binding : layout.bindings) {
        if (binding.action.type != devpiano::core::KeyActionType::note)
            continue;

        if (binding.action.trigger != devpiano::core::KeyTrigger::keyDown)
            continue;

        map[binding.keyCode] = binding.action.getMidiNoteNumber().value;
    }

    return map;
}

devpiano::core::KeyboardLayout keyMapToLayout(const std::unordered_map<int, int>& map, const juce::String& layoutId) {
    auto layout = (layoutId == "default.freepiano.full") ? devpiano::core::makeFullPianoLayout()
                                                         : devpiano::core::makeDefaultKeyboardLayout();

    if (!layoutId.isEmpty() && !layoutId.startsWith("default.")) {
        if (auto userLayout = devpiano::layout::loadUserLayoutById(layoutId); userLayout.has_value())
            layout = *userLayout;
    }

    if (map.empty())
        return layout;

    for (auto& binding : layout.bindings) {
        if (binding.action.type != devpiano::core::KeyActionType::note)
            continue;

        if (const auto it = map.find(binding.keyCode); it != map.end())
            binding.action.setMidiNoteNumber(devpiano::core::MidiNoteNumber::fromClamped(it->second));
    }

    return layout;
}

} // namespace devpiano::settings

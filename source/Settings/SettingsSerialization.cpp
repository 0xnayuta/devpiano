#include "Settings/SettingsSerialization.h"

namespace devpiano::settings {

// ---- Channel matrix serialization ----

juce::ValueTree channelMatrixToValueTree(const devpiano::midi::ChannelMatrix& cm) {
    juce::ValueTree root { "channelMatrix" };
    root.setProperty("active", cm.active, nullptr);
    for (std::size_t i = 0; i < 16; ++i) {
        juce::ValueTree ch { "ch" };
        const auto& c = cm.channels[i];
        ch.setProperty("outputChannel", c.outputChannel, nullptr);
        ch.setProperty("transpose", c.transpose, nullptr);
        ch.setProperty("octaveShift", c.octaveShift, nullptr);
        ch.setProperty("velocity", c.velocity, nullptr);
        ch.setProperty("program", c.program, nullptr);
        ch.setProperty("bankMSB", c.bankMSB, nullptr);
        ch.setProperty("sustainCC", c.sustainCC, nullptr);
        ch.setProperty("followKey", c.followKey, nullptr);
        root.appendChild(ch, nullptr);
    }
    return root;
}

devpiano::midi::ChannelMatrix valueTreeToChannelMatrix(const juce::ValueTree& t) {
    devpiano::midi::ChannelMatrix cm;
    if (!t.isValid()) {
        return cm;
    }
    cm.active = t.getProperty("active", true);
    for (int i = 0; i < 16 && i < t.getNumChildren(); ++i) {
        auto ch = t.getChild(i);
        auto& c = cm.channels[static_cast<std::size_t>(i)];
        c.outputChannel = static_cast<uint8_t>(static_cast<int>(ch.getProperty("outputChannel", i)));
        c.transpose = static_cast<int8_t>(static_cast<int>(ch.getProperty("transpose", 0)));
        c.octaveShift = static_cast<int8_t>(static_cast<int>(ch.getProperty("octaveShift", 0)));
        c.velocity = static_cast<uint8_t>(static_cast<int>(ch.getProperty("velocity", 64)));
        c.program = static_cast<uint8_t>(static_cast<int>(ch.getProperty("program", 0)));
        c.bankMSB = static_cast<uint8_t>(static_cast<int>(ch.getProperty("bankMSB", 0)));
        c.sustainCC = static_cast<uint8_t>(static_cast<int>(ch.getProperty("sustainCC", 64)));
        const bool defaultFollowKey = (i != 9);
        c.followKey = ch.hasProperty("followKey") ? static_cast<bool>(static_cast<int>(ch.getProperty("followKey")))
                                                  : defaultFollowKey;
    }
    return cm;
}
} // namespace devpiano::settings

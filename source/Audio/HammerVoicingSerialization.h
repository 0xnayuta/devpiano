#pragma once

#include "HammerVoicing.h"

#include <juce_core/juce_core.h>

namespace devpiano::audio {

/// 将三力度整音编码为当前格式对象：
/// { "anchors": [ { "velocity", "hardnessDelta", "loudnessDeltaDb", "rolloffDelta" }, ... ] }
[[nodiscard]] inline juce::var hammerVoicingToVar(const HammerVoicing& voicing) {
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    juce::Array<juce::var> anchors;
    anchors.ensureStorageAllocated(static_cast<int>(voicing.anchors.size()));
    for (const auto& anchor : voicing.anchors) {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("velocity", anchor.midiVelocity);
        obj->setProperty("hardnessDelta", anchor.hardnessDelta);
        obj->setProperty("loudnessDeltaDb", anchor.loudnessDeltaDb);
        obj->setProperty("rolloffDelta", anchor.rolloffDelta);
        anchors.add(juce::var(obj.get()));
    }
    root->setProperty("anchors", juce::var(anchors));
    return { root.get() };
}

/// 严格解析当前格式。缺失字段、错误类型、非有限值或非法锚点顺序均返回 false。
[[nodiscard]] inline bool hammerVoicingFromVar(const juce::var& value, HammerVoicing& out) noexcept {
    if (!value.isObject()) {
        return false;
    }
    auto* root = value.getDynamicObject();
    if (root == nullptr) {
        return false;
    }
    const auto anchorsValue = root->getProperty("anchors");
    if (!anchorsValue.isArray()) {
        return false;
    }
    const auto* anchors = anchorsValue.getArray();
    if (anchors == nullptr || anchors->size() != static_cast<int>(out.anchors.size())) {
        return false;
    }

    const auto finiteNumber = [](const juce::var& v) {
        return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite(static_cast<double>(v));
    };

    for (std::size_t i = 0; i < out.anchors.size(); ++i) {
        const auto& item = (*anchors)[static_cast<int>(i)];
        auto* obj = item.getDynamicObject();
        if (obj == nullptr) {
            return false;
        }
        const auto velocity = obj->getProperty("velocity");
        const auto hardness = obj->getProperty("hardnessDelta");
        const auto loudness = obj->getProperty("loudnessDeltaDb");
        const auto rolloff = obj->getProperty("rolloffDelta");
        if (!finiteNumber(velocity) || !finiteNumber(hardness) || !finiteNumber(loudness) || !finiteNumber(rolloff)) {
            return false;
        }
        out.anchors[i] = { static_cast<float>(velocity), static_cast<float>(hardness), static_cast<float>(loudness),
                           static_cast<float>(rolloff) };
    }
    return isValidHammerVoicing(out);
}

} // namespace devpiano::audio

#pragma once

#include <JuceHeader.h>

#include <unordered_map>

#include "Core/KeyMapTypes.h"
#include "Layout/LayoutDirectoryScanner.h"

// Persisted settings model.
//
// 职责边界：
// - 仅表示“需要落盘并在下次启动时恢复”的配置基线
// - 不应承载只在本次运行期间存在的瞬时状态
// - 运行态聚合请使用 devpiano::core::AppState
//
// 典型 persisted 内容：
// - 音频设备序列化状态
// - ADSR / Gain 参数
// - 上次插件搜索路径 / 上次插件名
// - 键盘布局持久化形态
struct SettingsModel
{
    struct AudioSettingsView
    {
        double sampleRate = 44100.0;
        int bufferSize = 512;
        bool hasSerializedDeviceState = false;
    };

    struct PerformanceSettingsView
    {
        float masterGain = 0.8f;
        float adsrAttack = 0.01f;
        float adsrDecay = 0.20f;
        float adsrSustain = 0.80f;
        float adsrRelease = 0.30f;
    };

    struct PluginRecoverySettingsView
    {
        juce::String pluginSearchPath;
        juce::String lastPluginName;
    };

    struct InputMappingSettingsView
    {
        juce::String layoutId { "default.freepiano.minimal" };
        std::unordered_map<int, int> keyMap;
    };

    // Persisted audio device state (serialized XML from AudioDeviceManager)
    std::unique_ptr<juce::XmlElement> audioDeviceState;

    // Persisted audio baseline.
    // 这些值用于启动恢复与无设备时的后备值，不代表当前运行时设备一定已经采用它们。
    double sampleRate = 44100.0;
    int bufferSize = 512;

    // Persisted performance parameters.
    float masterGain = 0.8f;
    float adsrAttack = 0.01f;
    float adsrDecay = 0.20f;
    float adsrSustain = 0.80f;
    float adsrRelease = 0.30f;

    // Persisted ,UI recovery state.
    juce::String pluginSearchPath;
    juce::String lastPluginName;
    juce::String lastLayoutId { "default.freepiano.minimal" };

    // Persisted keyboard layout in legacy on-disk shape.
    // 运行态请优先使用 KeyboardLayout / AppState.input.keyboardLayout。
    std::unordered_map<int, int> keyMap;

    [[nodiscard]] AudioSettingsView getAudioSettingsView() const
    {
        return { .sampleRate = sampleRate,
                 .bufferSize = bufferSize,
                 .hasSerializedDeviceState = audioDeviceState != nullptr };
    }

    void applyAudioSettingsView(const AudioSettingsView& view)
    {
        sampleRate = view.sampleRate;
        bufferSize = view.bufferSize;
    }

    void setSerializedAudioDeviceState(std::unique_ptr<juce::XmlElement> state)
    {
        audioDeviceState = std::move(state);
    }

    [[nodiscard]] PerformanceSettingsView getPerformanceSettingsView() const
    {
        return { .masterGain = masterGain,
                 .adsrAttack = adsrAttack,
                 .adsrDecay = adsrDecay,
                 .adsrSustain = adsrSustain,
                 .adsrRelease = adsrRelease };
    }

    void applyPerformanceSettingsView(const PerformanceSettingsView& view)
    {
        masterGain = view.masterGain;
        adsrAttack = view.adsrAttack;
        adsrDecay = view.adsrDecay;
        adsrSustain = view.adsrSustain;
        adsrRelease = view.adsrRelease;
    }

    [[nodiscard]] PluginRecoverySettingsView getPluginRecoverySettingsView() const
    {
        return { .pluginSearchPath = pluginSearchPath,
                 .lastPluginName = lastPluginName };
    }

    void applyPluginRecoverySettingsView(const PluginRecoverySettingsView& view)
    {
        pluginSearchPath = view.pluginSearchPath;
        lastPluginName = view.lastPluginName;
    }

    [[nodiscard]] InputMappingSettingsView getInputMappingSettingsView() const
    {
        return { .layoutId = lastLayoutId,
                 .keyMap = keyMap };
    }

    void applyInputMappingSettingsView(const InputMappingSettingsView& view)
    {
        lastLayoutId = view.layoutId;
        keyMap = view.keyMap;
    }

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

            map[binding.keyCode] = binding.action.getMidiNoteNumber().value;
        }

        return map;
    }

    static devpiano::core::KeyboardLayout keyMapToLayout(const std::unordered_map<int, int>& map, const juce::String& layoutId)
    {
        auto layout = (layoutId == "default.freepiano.full") ? devpiano::core::makeFullPianoLayout()
                                                              : devpiano::core::makeDefaultKeyboardLayout();

        if (!layoutId.isEmpty() && !layoutId.startsWith("default."))
        {
            if (auto userLayout = devpiano::layout::loadUserLayoutById(layoutId); userLayout.has_value())
                layout = *userLayout;
        }

        if (map.empty())
            return layout;

        for (auto& binding : layout.bindings)
        {
            if (binding.action.type != devpiano::core::KeyActionType::note)
                continue;

            if (const auto it = map.find(binding.keyCode); it != map.end())
                binding.action.setMidiNoteNumber(devpiano::core::MidiNoteNumber::fromClamped(it->second));
        }

        return layout;
    }
};

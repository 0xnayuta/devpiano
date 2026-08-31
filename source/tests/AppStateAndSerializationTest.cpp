#include <JuceHeader.h>

#include "Core/AppState.h"
#include "Core/KeyMapTypes.h"
#include "Midi/ChannelMatrix.h"
#include "Settings/AppStateBuilder.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsSerialization.h"

// =============================================================================
// Unit tests for AppStateBuilder and SettingsSerialization (TEST-005):
// - ChannelMatrix to/from ValueTree round-trip and corruption resilience
// - createPersistedAppState baseline generation
// - applyRuntime*State overlay application and zero-sampleRate guards
// =============================================================================

class AppStateAndSerializationTest final : public juce::UnitTest {
public:
    AppStateAndSerializationTest()
        : juce::UnitTest("AppStateAndSerialization", "DevPiano/Settings") {
    }

    void runTest() override {
        testChannelMatrixSerializationRoundTrip();
        testChannelMatrixCorruptedValueTreeFallback();
        testAppStateBuilderPersistedBaseline();
        testAppStateBuilderRuntimeOverlays();
    }

private:
    void testChannelMatrixSerializationRoundTrip() {
        beginTest("ChannelMatrix ValueTree serialization round-trip");

        devpiano::midi::ChannelMatrix original;
        original.active = false;
        for (std::size_t i = 0; i < 16; ++i) {
            auto& ch = original.channels[i];
            ch.outputChannel = static_cast<uint8_t>((i + 1) % 16);
            ch.transpose = static_cast<int8_t>(i - 8);
            ch.octaveShift = static_cast<int8_t>((i % 3) - 1);
            ch.velocity = static_cast<uint8_t>(40 + i * 5);
            ch.program = static_cast<uint8_t>(i * 2);
            ch.bankMSB = static_cast<uint8_t>(i);
            ch.sustainCC = static_cast<uint8_t>(60 + i);
            ch.followKey = (i % 2 == 0);
        }

        const auto tree = devpiano::settings::channelMatrixToValueTree(original);
        expect(tree.isValid());
        expectEquals(tree.getType().toString(), juce::String("channelMatrix"));
        expectEquals(tree.getNumChildren(), 16);

        const auto restored = devpiano::settings::valueTreeToChannelMatrix(tree);
        expect(restored.active == original.active, "Channel matrix active flag mismatch");
        for (std::size_t i = 0; i < 16; ++i) {
            const auto& o = original.channels[i];
            const auto& r = restored.channels[i];
            expectEquals(static_cast<int>(r.outputChannel), static_cast<int>(o.outputChannel));
            expectEquals(static_cast<int>(r.transpose), static_cast<int>(o.transpose));
            expectEquals(static_cast<int>(r.octaveShift), static_cast<int>(o.octaveShift));
            expectEquals(static_cast<int>(r.velocity), static_cast<int>(o.velocity));
            expectEquals(static_cast<int>(r.program), static_cast<int>(o.program));
            expectEquals(static_cast<int>(r.bankMSB), static_cast<int>(o.bankMSB));
            expectEquals(static_cast<int>(r.sustainCC), static_cast<int>(o.sustainCC));
            expect(r.followKey == o.followKey, "FollowKey mismatch on channel " + juce::String(i));
        }
    }

    void testChannelMatrixCorruptedValueTreeFallback() {
        beginTest("ChannelMatrix fallback with corrupt/partial ValueTree");

        // Invalid ValueTree returns default ChannelMatrix safely
        const juce::ValueTree invalidTree;
        const auto defaultMatrix = devpiano::settings::valueTreeToChannelMatrix(invalidTree);
        expect(defaultMatrix.active, "Default channel matrix must be active");
        expectEquals(static_cast<int>(defaultMatrix.channels[0].outputChannel), 0);

        // Empty root without children
        const juce::ValueTree emptyTree("channelMatrix");
        const auto fromEmpty = devpiano::settings::valueTreeToChannelMatrix(emptyTree);
        expect(fromEmpty.active);

        // Partial children (only 1 channel with missing properties)
        juce::ValueTree partialTree("channelMatrix");
        juce::ValueTree ch0("ch");
        ch0.setProperty("outputChannel", 5, nullptr);
        partialTree.appendChild(ch0, nullptr);

        const auto fromPartial = devpiano::settings::valueTreeToChannelMatrix(partialTree);
        expectEquals(static_cast<int>(fromPartial.channels[0].outputChannel), 5);
        expectEquals(static_cast<int>(fromPartial.channels[0].velocity), 64, "Missing velocity defaults to 64");
        expect(fromPartial.channels[0].followKey, "Channel 0 defaults to followKey=true");
        expect(!fromPartial.channels[9].followKey, "Channel 9 (Drums) defaults to followKey=false");
    }

    void testAppStateBuilderPersistedBaseline() {
        beginTest("AppState baseline creation from SettingsModel");

        SettingsModel settings;
        settings.sampleRate = 48000.0;
        settings.bufferSize = 256;
        settings.audioDeviceState = std::make_unique<juce::XmlElement>("DEVICESETUP");
        settings.builtinTone = SettingsModel::BuiltinTone::piano;
        settings.pianoBrightness = 0.85f;
        settings.pianoHammerHardness = 0.70f;
        settings.pianoResonance = 0.60f;

        const auto layout = devpiano::core::makeDefaultKeyboardLayout();
        const auto appState = devpiano::core::createPersistedAppState(settings, layout);

        expectEquals(appState.audio.sampleRate, 48000.0);
        expectEquals(appState.audio.bufferSize, 256);
        expect(appState.audio.hasSerializedDeviceState);
        expect(!appState.audio.hasLiveDevice, "Baseline state has no live device yet");
        expect(appState.performance.builtinTone == SettingsModel::BuiltinTone::piano);
        expectEquals(appState.performance.pianoBrightness, 0.85f);
        expectEquals(appState.performance.pianoHammerHardness, 0.70f);
        expectEquals(appState.performance.pianoResonance, 0.60f);
        expectEquals(appState.input.keyboardLayout.id, layout.id);
    }

    void testAppStateBuilderRuntimeOverlays() {
        beginTest("AppState runtime state overlays");

        SettingsModel settings;
        const auto layout = devpiano::core::makeDefaultKeyboardLayout();
        auto appState = devpiano::core::createPersistedAppState(settings, layout);

        // Runtime Audio overlay
        devpiano::core::RuntimeAudioState audioState;
        audioState.hasLiveDevice = true;
        audioState.sampleRate = 96000.0;
        audioState.bufferSize = 128;
        audioState.backendName = "ASIO";
        audioState.deviceName = "Virtual Audio";
        audioState.restoreOutcome = "success";

        devpiano::core::applyRuntimeAudioState(appState, audioState);
        expect(appState.audio.hasLiveDevice);
        expectEquals(appState.audio.sampleRate, 96000.0);
        expectEquals(appState.audio.bufferSize, 128);
        expectEquals(appState.audio.backendName, juce::String("ASIO"));
        expectEquals(appState.audio.deviceName, juce::String("Virtual Audio"));
        expectEquals(appState.audio.restoreOutcome, juce::String("success"));

        // When runtime sample rate is <= 0, baseline sample rate must be preserved
        devpiano::core::RuntimeAudioState zeroRateState;
        zeroRateState.hasLiveDevice = true;
        zeroRateState.sampleRate = 0.0;
        zeroRateState.bufferSize = 0;
        devpiano::core::applyRuntimeAudioState(appState, zeroRateState);
        expectEquals(appState.audio.sampleRate, 96000.0, "Zero sampleRate must not overwrite existing rate");

        // Runtime Plugin overlay
        devpiano::core::RuntimePluginState pluginState;
        pluginState.hasLoadedPlugin = true;
        pluginState.currentPluginName = "TestSynth";
        pluginState.isEditorOpen = true;
        pluginState.supportsVst3 = true;
        pluginState.scanPluginCount = 12;

        devpiano::core::applyRuntimePluginState(appState, pluginState);
        expect(appState.plugin.hasLoadedPlugin);
        expectEquals(appState.plugin.currentPluginName, juce::String("TestSynth"));
        expect(appState.plugin.isEditorOpen);
        expect(appState.plugin.supportsVst3);
        expectEquals(appState.plugin.scanPluginCount, 12);
    }
};

static AppStateAndSerializationTest appStateAndSerializationTest;

#include <JuceHeader.h>

#include <array>
#include <vector>

#include "Audio/PianoSynthVoice.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "TestHelpers.h"

// ============================================================================
/// LidAcousticsInteractionTest (Phase 29-A)
///
/// 验证现实物理琴盖开合度（Lid Position）在声学内核、音频引擎、配置持久化
/// 与预设系统间的全链路状态流转与高频能量响应特性。
// ============================================================================
class LidAcousticsInteractionTest final : public juce::UnitTest {
public:
    LidAcousticsInteractionTest()
        : juce::UnitTest("LidAcousticsInteraction", "DevPiano/Acoustics") {
    }

    void runTest() override {
        testPianoSynthVoiceAcousticResponse();
        testDynamicLidSwitchDuringPlayback();
        testSettingsModelAndStorePersistence();
        testPerformancePresetAcousticRoundTrip();
        testSampleRateNormalizedTaps();
    }

private:
    void testPianoSynthVoiceAcousticResponse() {
        beginTest("PianoSynthVoice: Acoustic filtering variation across 3 lid positions");

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 1024;

        const auto renderNoteWithLid = [&](PianoSynthVoice::LidPosition lid, float& outRms, float& outDirectPeak) {
            juce::Synthesiser synth;
            synth.setCurrentPlaybackSampleRate(sampleRate);
            synth.addSound(new PianoSynthSound());
            auto* voice = new PianoSynthVoice();
            voice->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
            voice->setLidPosition(lid);
            synth.addVoice(voice);
            expect(voice->getLidPosition() == lid);

            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.8f), 0); // C5, forte

            juce::AudioBuffer<float> buffer(2, blockSize);
            buffer.clear();
            synth.renderNextBlock(buffer, midi, 0, blockSize);

            outRms = buffer.getRMSLevel(0, 0, blockSize);
            // 在前 120 个采样点内（44.1 kHz 首个反射 tap 约 130 采样），输出纯由 gDirect 直达声决定
            outDirectPeak = buffer.getMagnitude(0, 0, 120);
        };

        float rmsOpen = 0.0f;
        float peakOpen = 0.0f;
        float rmsHalf = 0.0f;
        float peakHalf = 0.0f;
        float rmsClosed = 0.0f;
        float peakClosed = 0.0f;

        renderNoteWithLid(PianoSynthVoice::LidPosition::fullOpen, rmsOpen, peakOpen);
        renderNoteWithLid(PianoSynthVoice::LidPosition::halfStick, rmsHalf, peakHalf);
        renderNoteWithLid(PianoSynthVoice::LidPosition::closed, rmsClosed, peakClosed);

        expect(rmsOpen > 0.001f);
        expect(rmsHalf > 0.001f);
        expect(rmsClosed > 0.001f);

        // 数值物理不变量：前 120 采样内直达声比例严格遵循 fullOpen(0.82) > halfStick(0.75) > closed(0.68)
        expect(peakOpen > peakHalf);
        expect(peakHalf > peakClosed);
    }

    void testSampleRateNormalizedTaps() {
        beginTest("PianoLidAcoustics: reflection taps follow time constants across sample rates");

        struct TapCase {
            double sampleRate = 48000.0;
            std::size_t tap1 = 141;
            std::size_t tap2 = 344;
            std::size_t tap3 = 507;
        };
        const std::array<TapCase, 4> cases { {
            { 44100.0, 130, 316, 466 },
            { 48000.0, 141, 344, 507 },
            { 96000.0, 282, 688, 1014 },
            { 192000.0, 564, 1376, 2028 },
        } };

        for (const auto& c : cases) {
            devpiano::audio::LidAcoustics lid;
            lid.setPosition(devpiano::audio::LidPosition::fullOpen, c.sampleRate);
            const auto count = c.tap3 + 2;
            std::vector<float> left(count, 0.0f);
            std::vector<float> right(count, 0.0f);
            for (std::size_t sample = 0; sample < count; ++sample) {
                auto l = sample == 0 ? 1.0f : 0.0f;
                auto r = 0.0f;
                lid.processStereo(l, r);
                left[sample] = l;
                right[sample] = r;
            }

            expectWithinAbsoluteError(left[0], 0.82f, 1e-6f, "direct left gain at sample 0");
            expectWithinAbsoluteError(right[0], 0.0f, 1e-6f, "direct right gain at sample 0");
            expectWithinAbsoluteError(left[c.tap1], 0.10f, 1e-6f, "tap 1 follows 141/48000 s");
            expectWithinAbsoluteError(right[c.tap2], 0.06f, 1e-6f, "tap 2 follows 344/48000 s");
            expectWithinAbsoluteError(left[c.tap3], 0.04f, 1e-6f, "tap 3 follows 507/48000 s");
        }
    }

    void testDynamicLidSwitchDuringPlayback() {
        beginTest("PianoSynthVoice: Dynamic lid switching during note playback without NaN or pop");

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(sampleRate);
        synth.addSound(new PianoSynthSound());
        auto* voice = new PianoSynthVoice();
        voice->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
        voice->setLidPosition(PianoSynthVoice::LidPosition::fullOpen);
        synth.addVoice(voice);

        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.9f), 0); // C5, ff

        juce::AudioBuffer<float> buffer(2, blockSize);

        for (int block = 0; block < 10; ++block) {
            buffer.clear();
            if (block == 2) {
                voice->setLidPosition(PianoSynthVoice::LidPosition::halfStick);
            } else if (block == 5) {
                voice->setLidPosition(PianoSynthVoice::LidPosition::closed);
            } else if (block == 8) {
                voice->setLidPosition(PianoSynthVoice::LidPosition::fullOpen);
            }

            synth.renderNextBlock(buffer, midi, 0, blockSize);
            midi.clear();

            // 严禁产生 NaN / Inf 异常浮点数
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                const auto* samples = buffer.getReadPointer(ch);
                for (int i = 0; i < blockSize; ++i) {
                    expect(!std::isnan(samples[i]));
                    expect(!std::isinf(samples[i]));
                    expect(std::abs(samples[i]) <= 2.0f); // 无超调爆音
                }
            }
        }
    }

    void testSettingsModelAndStorePersistence() {
        beginTest("SettingsStore: Persist and restore pianoLidPosition with boundary clamping");

        devpiano::test::ScopedTempDir tempDir("lid-settings");
        const auto settingsFile = tempDir.getChildFile("lid_test.settings");

        juce::PropertiesFile::Options opts;
        opts.storageFormat = juce::PropertiesFile::storeAsXML;
        opts.commonToAllUsers = false;

        {
            SettingsStore store(settingsFile);
            SettingsModel model;
            model.lidPosition = SettingsModel::LidPosition::closed;
            expect(store.save(model));
        }

        {
            SettingsStore store(settingsFile);
            SettingsModel loadedModel;
            loadedModel.lidPosition = SettingsModel::LidPosition::fullOpen;
            store.load(loadedModel);
            expect(loadedModel.lidPosition == SettingsModel::LidPosition::closed);
        }

        // 越界值容错测试
        {
            auto props = std::make_unique<juce::PropertiesFile>(settingsFile, opts);
            props->setValue("pianoLidPosition", 999);
            props->save();
        }

        {
            SettingsStore store(settingsFile);
            SettingsModel clampedModel;
            store.load(clampedModel);
            expect(clampedModel.lidPosition == SettingsModel::LidPosition::closed);
        }
    }

    void testPerformancePresetAcousticRoundTrip() {
        beginTest("PerformancePreset: Acoustic lid round-trip and optional acoustics fallback");

        devpiano::test::ScopedTempDir tempDir("lid-preset");
        const auto presetFile = tempDir.getChildFile("acoustic_preset.devpiano.preset");

        // 1. 保存包含 HalfStick 琴盖位置的预设
        devpiano::layout::PerformancePreset originalPreset = devpiano::layout::makeDefaultPreset();
        originalPreset.name = "AcousticStudio";
        originalPreset.lidPosition = SettingsModel::LidPosition::halfStick;

        expect(devpiano::layout::savePreset(originalPreset, presetFile));

        // 2. 加载预设并验证
        auto loadedOpt = devpiano::layout::loadPreset(presetFile);
        expect(loadedOpt.has_value());
        if (loadedOpt.has_value()) {
            expectEquals(loadedOpt->name, juce::String("AcousticStudio"));
            expect(loadedOpt->lidPosition == SettingsModel::LidPosition::halfStick);
        }

        // 3. Safe fallback for presets lacking acoustics field
        const juce::String missingAcousticsJson = R"({
            "version": 2,
            "uuid": "44444444-5555-6666-7777-888888888888",
            "name": "DefaultAcousticsPreset",
            "layout": { "id": "default.1", "name": "Default", "bindings": [] },
            "keyboard": { "keySignature": 0, "midiTranspose": false }
        })";

        const auto missingAcousticsFile = tempDir.getChildFile("default_acoustics_preset.devpiano.preset");
        expect(missingAcousticsFile.replaceWithText(missingAcousticsJson));

        auto missingAcousticsPreset = devpiano::layout::loadPreset(missingAcousticsFile);
        expect(missingAcousticsPreset.has_value());
        if (missingAcousticsPreset.has_value()) {
            expectEquals(missingAcousticsPreset->name, juce::String("DefaultAcousticsPreset"));
            // 缺省声学字段安全回退到 fullOpen
            expect(missingAcousticsPreset->lidPosition == SettingsModel::LidPosition::fullOpen);
        }
    }
};

static LidAcousticsInteractionTest lidAcousticsInteractionTest;

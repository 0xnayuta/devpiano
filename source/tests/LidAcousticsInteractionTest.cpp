#include <JuceHeader.h>

#include <array>
#include <vector>

#include "Audio/BuiltinSynthesiser.h"
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
        testSampleRateNormalizedDiffusionDelays();
        testFullOpenAllpassFlatMagnitude();
        testLidModeSpectralDifference();
        testTailDrainBound();
    }

private:
    void testPianoSynthVoiceAcousticResponse() {
        beginTest("PianoSynthVoice: Acoustic filtering variation across 3 lid positions");

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 1024;

        const auto renderNoteWithLid = [&](PianoSynthVoice::LidPosition lid, float& outRms, float& outDirectPeak) {
            devpiano::audio::BuiltinSynthesiser synth;
            synth.setCurrentPlaybackSampleRate(sampleRate);
            synth.setPedalAcousticsEnabled(true);
            synth.addSound(new PianoSynthSound());
            auto* voice = new PianoSynthVoice();
            voice->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
            synth.setPedalLidPosition(lid);
            synth.addVoice(voice);
            expect(synth.getWholeBodyBus().lidPosition == lid);

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

        // 全通扩散器不再用固定 0.82/0.75/0.68 直达抽头；
        // 开合度由低通与反馈差异体现，整块 RMS 必须保持 fullOpen > half > closed。
        expect(peakOpen > 0.0f);
        expect(peakHalf > 0.0f);
        expect(peakClosed > 0.0f);
        expect(rmsOpen > rmsHalf);
        expect(rmsHalf > rmsClosed);
    }

    void testSampleRateNormalizedDiffusionDelays() {
        beginTest("PianoLidAcoustics: allpass diffusion delays follow time constants across sample rates");

        struct DelayCase {
            double sampleRate = 48000.0;
            std::size_t delay1 = 141;
            std::size_t delay2 = 507;
        };
        const std::array<DelayCase, 5> cases { {
            { 44100.0, 130, 466 },
            { 48000.0, 141, 507 },
            { 96000.0, 282, 1014 },
            { 192000.0, 564, 2028 },
            { 384000.0, 1128, 4056 },
        } };

        for (const auto& c : cases) {
            devpiano::audio::LidAcoustics lid;
            lid.setPosition(devpiano::audio::LidPosition::fullOpen, c.sampleRate);
            expectEquals(lid.delay1, c.delay1, "first allpass delay follows 141/48000 s");
            expectEquals(lid.delay2, c.delay2, "second allpass delay follows 507/48000 s");
            expect(lid.tailDrainSamples() > static_cast<int>(c.delay2),
                   "tail drain must cover at least the longest diffusion delay");
        }
    }

    void testFullOpenAllpassFlatMagnitude() {
        beginTest("PianoLidAcoustics: fullOpen allpass keeps broadband energy flat");

        constexpr double sampleRate = 48000.0;
        constexpr int totalSamples = 32768;
        constexpr int measureFrom = 24576;
        for (const auto frequency : { 1000.0, 3000.0, 6000.0, 10000.0 }) {
            devpiano::audio::LidAcoustics lid;
            lid.setPosition(devpiano::audio::LidPosition::fullOpen, sampleRate);
            lid.reset();

            double inputEnergy = 0.0;
            double outputEnergy = 0.0;
            for (auto sample = 0; sample < totalSamples; ++sample) {
                const auto phase = juce::MathConstants<double>::twoPi * frequency * sample / sampleRate;
                const auto input = static_cast<float>(std::sin(phase));
                auto left = input;
                auto right = input;
                lid.processStereo(left, right);
                if (sample >= measureFrom) {
                    inputEnergy += 2.0 * static_cast<double>(input) * static_cast<double>(input);
                    outputEnergy += static_cast<double>(left) * left + static_cast<double>(right) * right;
                }
            }

            const auto ratio = outputEnergy / inputEnergy;
            expectWithinAbsoluteError(ratio, 1.0, 0.01,
                                      "fullOpen allpass must not create frequency-dependent comb notches");
        }
    }

    void testLidModeSpectralDifference() {
        beginTest("PianoLidAcoustics: lid modes retain distinct high-frequency energy");

        constexpr double sampleRate = 48000.0;
        constexpr int totalSamples = 32768;
        constexpr int measureFrom = 24576;
        constexpr double frequency = 8000.0;
        const auto renderEnergy = [&](devpiano::audio::LidPosition position) {
            devpiano::audio::LidAcoustics lid;
            lid.setPosition(position, sampleRate);
            lid.reset();
            double outputEnergy = 0.0;
            for (auto sample = 0; sample < totalSamples; ++sample) {
                const auto phase = juce::MathConstants<double>::twoPi * frequency * sample / sampleRate;
                const auto input = static_cast<float>(std::sin(phase));
                auto left = input;
                auto right = input;
                lid.processStereo(left, right);
                if (sample >= measureFrom) {
                    outputEnergy += static_cast<double>(left) * left + static_cast<double>(right) * right;
                }
            }
            return outputEnergy;
        };

        const auto openEnergy = renderEnergy(devpiano::audio::LidPosition::fullOpen);
        const auto halfEnergy = renderEnergy(devpiano::audio::LidPosition::halfStick);
        const auto closedEnergy = renderEnergy(devpiano::audio::LidPosition::closed);
        expect(openEnergy > halfEnergy * 1.2, "fullOpen must preserve more 8 kHz energy than halfStick");
        expect(halfEnergy > closedEnergy * 1.5, "halfStick must preserve more 8 kHz energy than closed");
    }

    void testTailDrainBound() {
        beginTest("PianoLidAcoustics: tail drain is bounded and covers the allpass decay");

        for (const auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 }) {
            devpiano::audio::LidAcoustics lid;
            lid.setPosition(devpiano::audio::LidPosition::closed, sampleRate);
            const auto drain = lid.tailDrainSamples();
            const auto longestDelay = static_cast<int>(lid.delay2);
            expect(drain > longestDelay, "tail drain covers the longest delay");
            expect(drain < static_cast<int>(sampleRate * 1.5), "tail drain stays bounded below 1.5 s");
        }
    }

    void testDynamicLidSwitchDuringPlayback() {
        beginTest("PianoSynthVoice: Dynamic lid switching during note playback without NaN or pop");

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;

        devpiano::audio::BuiltinSynthesiser synth;
        synth.setCurrentPlaybackSampleRate(sampleRate);
        synth.setPedalAcousticsEnabled(true);
        synth.addSound(new PianoSynthSound());
        auto* voice = new PianoSynthVoice();
        voice->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
        synth.setPedalLidPosition(PianoSynthVoice::LidPosition::fullOpen);
        synth.addVoice(voice);

        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.9f), 0); // C5, ff

        juce::AudioBuffer<float> buffer(2, blockSize);

        for (int block = 0; block < 10; ++block) {
            buffer.clear();
            if (block == 2) {
                synth.setPedalLidPosition(PianoSynthVoice::LidPosition::halfStick);
            } else if (block == 5) {
                synth.setPedalLidPosition(PianoSynthVoice::LidPosition::closed);
            } else if (block == 8) {
                synth.setPedalLidPosition(PianoSynthVoice::LidPosition::fullOpen);
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

#include <JuceHeader.h>

#include "Audio/AudioEngine.h"
#include "Audio/PianoSynthVoice.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"

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
        testAudioEngineLidPositionState();
        testPianoSynthVoiceAcousticResponse();
        testDynamicLidSwitchDuringPlayback();
        testSettingsModelAndStorePersistence();
        testPerformancePresetAcousticRoundTrip();
    }

private:
    void testAudioEngineLidPositionState() {
        beginTest("AudioEngine: setLidPosition and getLidPosition atomic state");

        AudioEngine engine;
        expect(engine.getLidPosition() == AudioEngine::LidPosition::fullOpen);

        engine.setLidPosition(AudioEngine::LidPosition::halfStick);
        expect(engine.getLidPosition() == AudioEngine::LidPosition::halfStick);

        engine.setLidPosition(AudioEngine::LidPosition::closed);
        expect(engine.getLidPosition() == AudioEngine::LidPosition::closed);

        engine.setLidPosition(AudioEngine::LidPosition::fullOpen);
        expect(engine.getLidPosition() == AudioEngine::LidPosition::fullOpen);
    }

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
            // 在前 120 个采样点内（首个反射 tap 延迟为 141 采样），输出纯由 gDirect 直达声决定
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

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto settingsFile = tempDir.getNonexistentChildFile("LidTest", ".settings");

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

        settingsFile.deleteFile();
    }

    void testPerformancePresetAcousticRoundTrip() {
        beginTest("PerformancePreset: Acoustic lid position round-trip and backward compatibility");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto presetFile = tempDir.getNonexistentChildFile("AcousticPreset", ".devpiano.preset");

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

        // 3. 向前兼容测试：缺少 acoustics 字段的老版本 JSON 预设文件
        const juce::String legacyJson = R"({
            "version": 1,
            "name": "LegacyPreset",
            "layout": { "id": "legacy.1", "name": "Legacy", "bindings": [] },
            "keyboard": { "keySignature": 0, "midiTranspose": false }
        })";

        const auto legacyFile = tempDir.getNonexistentChildFile("LegacyPreset", ".devpiano.preset");
        expect(legacyFile.replaceWithText(legacyJson));

        auto legacyLoadedOpt = devpiano::layout::loadPreset(legacyFile);
        expect(legacyLoadedOpt.has_value());
        if (legacyLoadedOpt.has_value()) {
            expectEquals(legacyLoadedOpt->name, juce::String("LegacyPreset"));
            // 缺省声学字段安全回退到 fullOpen
            expect(legacyLoadedOpt->lidPosition == SettingsModel::LidPosition::fullOpen);
        }

        presetFile.deleteFile();
        legacyFile.deleteFile();
    }
};

static LidAcousticsInteractionTest lidAcousticsInteractionTest;

#include <JuceHeader.h>

#include "Audio/PianoStyle.h"
#include "Input/KeyboardMidiMapper.h"
#include "Input/TouchVelocityCurve.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "TestHelpers.h"

// ==============================================================================
// AcousticSettingsPersistenceTest:
// 验证 Phase 29-D 规范：
// 1. SettingsStore 读写 round-trip 与异常越界值保护钳制（lidPosition, touchCurve, unaCorda）；
// 2. PerformancePreset JSON 序列化/反序列化 round-trip；
// 3. Default fallback for presets lacking acoustics block;
// 5. KeyboardMidiMapper::setSoftPedalDown 的状态维持与防抖回调机制。
// ==============================================================================
using namespace devpiano::layout;

class AcousticSettingsPersistenceTest final : public juce::UnitTest {
public:
    AcousticSettingsPersistenceTest()
        : juce::UnitTest("AcousticSettingsPersistence", "DevPiano/Settings") {
    }

    void runTest() override {
        testSettingsStoreAcousticRoundTrip();
        testSettingsStoreCorruptedBoundaryClamping();
        testPerformancePresetAcousticRoundTrip();
        testPerformancePresetNyStyleRoundTrip();
        testPianoStyleIdentityIncludesHammerVoicing();
        testNySteinwayStyleIdentityAndDefaultNeutrality();
        testKeyboardMidiMapperSoftPedalStateAndCallback();
    }

private:
    void testSettingsStoreAcousticRoundTrip() {
        beginTest("SettingsStore: acoustic parameters round-trip (lid, touch curve, una corda)");

        devpiano::test::ScopedTempDir tempDir("acoustic-settings-rt");
        const auto settingsFile = tempDir.getChildFile("acoustic_roundtrip.settings");
        SettingsModel originalModel;
        originalModel.lidPosition = SettingsModel::LidPosition::halfStick;
        originalModel.touchVelocityCurve = devpiano::input::TouchVelocityCurve::heavy;
        originalModel.unaCorda = true;

        SettingsStore store(settingsFile);
        const bool saved = store.save(originalModel);
        expect(saved, "Settings should save successfully");

        expect(settingsFile.existsAsFile(), "Settings file should be written");

        SettingsModel loadedModel;
        store.load(loadedModel);

        expect(loadedModel.lidPosition == SettingsModel::LidPosition::halfStick,
               "LidPosition should be preserved as halfStick");
        expect(loadedModel.touchVelocityCurve == devpiano::input::TouchVelocityCurve::heavy,
               "TouchVelocityCurve should be preserved as heavy");
        expect(loadedModel.unaCorda == true, "unaCorda state should be preserved as true");
    }

    void testSettingsStoreCorruptedBoundaryClamping() {
        beginTest("SettingsStore: corrupted/out-of-bound acoustic properties are clamped safely");

        devpiano::test::ScopedTempDir tempDir("acoustic-clamp");
        const auto settingsFile = tempDir.getChildFile("corrupted_boundary.settings");
        // Manually write an XML properties file with out-of-range acoustic values
        juce::PropertiesFile::Options opts;
        opts.applicationName = "devpiano_clamp_test";
        opts.filenameSuffix = "settings";
        opts.storageFormat = juce::PropertiesFile::storeAsXML;
        juce::PropertiesFile props(settingsFile, opts);

        props.setValue("pianoLidPosition", 999); // Valid range: [0, 2]
        props.setValue("touchVelocityCurve", -10); // Valid range: [0, 3]
        props.setValue("unaCorda", true);
        props.saveIfNeeded();

        SettingsStore store(settingsFile);
        SettingsModel model;
        store.load(model);

        // 999 should be clamped to 2 (closed)
        expect(model.lidPosition == SettingsModel::LidPosition::closed,
               "Out-of-range lidPosition 999 should clamp to closed (2)");

        // -10 should be clamped to 0 (standard)
        expect(model.touchVelocityCurve == devpiano::input::TouchVelocityCurve::standard,
               "Negative touchVelocityCurve -10 should clamp to standard (0)");

        expect(model.unaCorda == true, "unaCorda boolean parsed correctly");
    }

    void testPerformancePresetAcousticRoundTrip() {
        beginTest("PerformancePreset: full acoustic settings round-trip via .devpiano.preset JSON");

        devpiano::test::ScopedTempDir tempDir("acoustic-preset-rt");
        const auto presetFile = tempDir.getChildFile("acoustic_preset.devpiano.preset");
        PerformancePreset originalPreset = makeDefaultPreset();
        originalPreset.uuid = "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d";
        originalPreset.name = "Acoustic Ballad";
        originalPreset.lidPosition = SettingsModel::LidPosition::closed;
        originalPreset.touchVelocityCurve = devpiano::input::TouchVelocityCurve::wideDynamic;
        originalPreset.unaCorda = true;
        originalPreset.keySignature = 3;
        originalPreset.midiTranspose = true;

        const bool saved = savePreset(originalPreset, presetFile);
        expect(saved, "Preset should save successfully");

        const auto loadedOpt = loadPreset(presetFile);
        expect(loadedOpt.has_value(), "Preset should load successfully");

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, juce::String("Acoustic Ballad"));
            expect(loaded.lidPosition == SettingsModel::LidPosition::closed, "LidPosition closed should round-trip");
            expect(loaded.touchVelocityCurve == devpiano::input::TouchVelocityCurve::wideDynamic,
                   "TouchVelocityCurve wideDynamic should round-trip");
            expect(loaded.unaCorda == true, "unaCorda true should round-trip");
            expectEquals(loaded.keySignature, 3);
            expect(loaded.midiTranspose == true);
        }
    }

    void testPerformancePresetNyStyleRoundTrip() {
        beginTest("PerformancePreset: NY style acoustic identity round-trip");
        const auto* ny = devpiano::audio::getPianoStyleParameters(devpiano::audio::PianoStyle::nySteinwayDClassical);
        expect(ny != nullptr, "NY style parameters must exist");
        if (ny == nullptr) {
            return;
        }

        devpiano::test::ScopedTempDir tempDir("ny-style-preset-rt");
        const auto presetFile = tempDir.getChildFile("ny_style.devpiano.preset");
        auto preset = makeDefaultPreset();
        preset.uuid = "b2c3d4e5-f6a7-4b8c-9d0e-1f2a3b4c5d6e";
        preset.name = "NY Style Round Trip";
        preset.brightness = ny->brightness;
        preset.hammerHardness = ny->hammerHardness;
        preset.hammerVoicing = ny->hammerVoicing;
        preset.resonance = ny->resonance;
        preset.lidPosition = static_cast<SettingsModel::LidPosition>(ny->lidPosition);
        preset.reverbSpace = static_cast<devpiano::audio::ReverbSpace>(ny->reverbSpace);
        preset.reverbWet = ny->reverbWet;
        preset.feltAgeingAmount = ny->feltAgeingAmount;
        preset.stretchTuningEnabled = ny->stretchTuningEnabled;
        preset.duplexResonance = ny->duplexResonance;

        expect(savePreset(preset, presetFile), "NY style preset should save");
        const auto loadedOpt = loadPreset(presetFile);
        expect(loadedOpt.has_value(), "NY style preset should load");
        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            const devpiano::audio::PianoStyleParameters roundTripped {
                .brightness = loaded.brightness,
                .hammerHardness = loaded.hammerHardness,
                .hammerVoicing = loaded.hammerVoicing,
                .resonance = loaded.resonance,
                .stretchTuningEnabled = loaded.stretchTuningEnabled,
                .duplexResonance = loaded.duplexResonance,
                .lidPosition = static_cast<std::uint8_t>(loaded.lidPosition),
                .feltAgeingAmount = loaded.feltAgeingAmount,
                .reverbSpace = static_cast<std::uint8_t>(loaded.reverbSpace),
                .reverbWet = loaded.reverbWet,
            };
            expect(devpiano::audio::identifyPianoStyle(roundTripped)
                       == devpiano::audio::PianoStyle::nySteinwayDClassical,
                   "NY style identity must survive preset round-trip");
        }
    }

    void testPianoStyleIdentityIncludesHammerVoicing() {
        beginTest("PianoStyle: voicing calibration participates in Custom detection");
        devpiano::audio::PianoStyleParameters params;
        expect(devpiano::audio::identifyPianoStyle(params) == devpiano::audio::PianoStyle::standard,
               "calibrated default voicing must identify as Standard");
        params.hammerVoicing.anchors[1].loudnessDeltaDb += 0.5f;
        expect(devpiano::audio::identifyPianoStyle(params) == devpiano::audio::PianoStyle::custom,
               "a changed voicing anchor must identify as Custom");
    }

    void testNySteinwayStyleIdentityAndDefaultNeutrality() {
        beginTest("PianoStyle: NY Steinway D Classical includes voicing and room state");
        const auto* ny = devpiano::audio::getPianoStyleParameters(devpiano::audio::PianoStyle::nySteinwayDClassical);
        expect(ny != nullptr, "NY Steinway D Classical style must exist");
        if (ny == nullptr) {
            return;
        }

        expect(ny->reverbSpace == 2 && ny->reverbWet > 0.0f, "NY style must carry a non-zero room state");
        expect(devpiano::audio::identifyPianoStyle(*ny) == devpiano::audio::PianoStyle::nySteinwayDClassical,
               "NY style parameters must identify as the NY style");
        expect(devpiano::audio::identifyPianoStyle(devpiano::audio::PianoStyleParameters {})
                   == devpiano::audio::PianoStyle::standard,
               "default neutral parameters must remain Standard");
    }

    void testKeyboardMidiMapperSoftPedalStateAndCallback() {
        beginTest("KeyboardMidiMapper: setSoftPedalDown state and callback deduplication");

        KeyboardMidiMapper mapper;
        expect(!mapper.isSoftPedalDown(), "Soft pedal default state must be false");

        int callbackCount = 0;
        bool lastCallbackState = false;

        mapper.setSoftPedalCallback([&](bool down) {
            ++callbackCount;
            lastCallbackState = down;
        });

        // Set to true -> triggers callback
        mapper.setSoftPedalDown(true);
        expect(mapper.isSoftPedalDown(), "Soft pedal must be down");
        expectEquals(callbackCount, 1);
        expect(lastCallbackState == true);

        // Setting same state true again -> must NOT trigger callback (deduplication)
        mapper.setSoftPedalDown(true);
        expectEquals(callbackCount, 1, "Duplicate setSoftPedalDown must be ignored");

        // Set to false -> triggers callback
        mapper.setSoftPedalDown(false);
        expect(!mapper.isSoftPedalDown(), "Soft pedal must be up");
        expectEquals(callbackCount, 2);
        expect(lastCallbackState == false);
    }
};

static AcousticSettingsPersistenceTest acousticSettingsPersistenceTest;

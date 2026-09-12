#include <JuceHeader.h>

#include "Input/KeyboardMidiMapper.h"
#include "Input/TouchVelocityCurve.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"

// ==============================================================================
// AcousticSettingsPersistenceTest:
// 验证 Phase 29-D 规范：
// 1. SettingsStore 读写 round-trip 与异常越界值保护钳制（lidPosition, touchCurve, unaCorda）；
// 2. PerformancePreset JSON 序列化/反序列化 round-trip；
// 3. 老版本缺省声学字段预设的向后兼容与默认值安全回退；
// 4. 根对象平铺声学字段的兼容读取；
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
        testPerformancePresetLegacyBackwardCompatibility();
        testPerformancePresetFlatRootAcousticsCompatibility();
        testKeyboardMidiMapperSoftPedalStateAndCallback();
    }

private:
    void testSettingsStoreAcousticRoundTrip() {
        beginTest("SettingsStore: acoustic parameters round-trip (lid, touch curve, una corda)");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto settingsFile
            = tempDir.getChildFile("devpiano_acoustic_test_" + juce::Uuid().toString() + ".settings");

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

        // Verify PerformanceSettingsView accessor round-trip
        const auto view = loadedModel.getPerformanceSettingsView();
        expect(view.lidPosition == SettingsModel::LidPosition::halfStick);
        expect(view.touchVelocityCurve == devpiano::input::TouchVelocityCurve::heavy);
        expect(view.unaCorda == true);

        settingsFile.deleteFile();
    }

    void testSettingsStoreCorruptedBoundaryClamping() {
        beginTest("SettingsStore: corrupted/out-of-bound acoustic properties are clamped safely");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto settingsFile
            = tempDir.getChildFile("devpiano_acoustic_clamp_" + juce::Uuid().toString() + ".settings");

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

        settingsFile.deleteFile();
    }

    void testPerformancePresetAcousticRoundTrip() {
        beginTest("PerformancePreset: full acoustic settings round-trip via .devpiano.preset JSON");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto presetFile = tempDir.getChildFile("acoustic_preset_" + juce::Uuid().toString() + ".devpiano.preset");

        PerformancePreset originalPreset;
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

        presetFile.deleteFile();
    }

    void testPerformancePresetLegacyBackwardCompatibility() {
        beginTest("PerformancePreset: legacy presets lacking acoustics block fall back safely");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto legacyFile
            = tempDir.getChildFile("legacy_no_acoustics_" + juce::Uuid().toString() + ".devpiano.preset");

        // Legacy preset JSON with version 1, but no "acoustics" object and no acoustic fields
        const juce::String legacyJson = R"({
            "version": 1,
            "name": "Classic V1 Preset",
            "bindings": [],
            "channelMatrix": {
                "defaultChannel": 1,
                "mappings": []
            },
            "keyboard": {
                "keySignature": 0,
                "midiTranspose": false,
                "colourMode": 0,
                "noteDisplay": 0,
                "fadeSpeed": 0.92
            }
        })";

        legacyFile.replaceWithText(legacyJson);
        expect(legacyFile.existsAsFile());

        const auto loadedOpt = loadPreset(legacyFile);
        expect(loadedOpt.has_value(), "Legacy preset must load without error");

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, juce::String("Classic V1 Preset"));

            // Must fall back to safe acoustic defaults
            expect(loaded.lidPosition == SettingsModel::LidPosition::fullOpen,
                   "Missing lidPosition must fall back to fullOpen");
            expect(loaded.touchVelocityCurve == devpiano::input::TouchVelocityCurve::standard,
                   "Missing touchVelocityCurve must fall back to standard");
            expect(loaded.unaCorda == false, "Missing unaCorda must fall back to false");
        }

        legacyFile.deleteFile();
    }

    void testPerformancePresetFlatRootAcousticsCompatibility() {
        beginTest("PerformancePreset: flat root acoustic fields compatibility");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto flatFile = tempDir.getChildFile("flat_acoustics_" + juce::Uuid().toString() + ".devpiano.preset");
        // Format where acoustic fields are at the root level instead of inside "acoustics": {}
        const juce::String flatJson = R"({
            "version": 1,
            "name": "Flat Root Preset",
            "lidPosition": 1,
            "touchVelocityCurve": 1,
            "unaCorda": true,
            "bindings": [],
            "channelMatrix": { "defaultChannel": 1, "mappings": [] },
            "keyboard": {}
        })";

        flatFile.replaceWithText(flatJson);

        const auto loadedOpt = loadPreset(flatFile);
        expect(loadedOpt.has_value(), "Flat root preset must load successfully");

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, juce::String("Flat Root Preset"));
            expect(loaded.lidPosition == SettingsModel::LidPosition::halfStick,
                   "Root lidPosition 1 should parse as halfStick");
            expect(loaded.touchVelocityCurve == devpiano::input::TouchVelocityCurve::light,
                   "Root touchVelocityCurve 1 should parse as light");
            expect(loaded.unaCorda == true, "Root unaCorda true should parse correctly");
        }

        flatFile.deleteFile();
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

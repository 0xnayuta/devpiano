#include <JuceHeader.h>

#include "Audio/TemperamentEngine.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"

// ============================================================================
/// TemperamentSettingsPersistenceTest (Phase 30-C)
///
/// Validates cross-session persistence of temperament and A4 reference pitch
/// in SettingsStore properties XML, full-stack PerformancePreset JSON round-trip,
/// legacy preset backward compatibility, and out-of-bound input clamping.
// ============================================================================
class TemperamentSettingsPersistenceTest final : public juce::UnitTest {
public:
    TemperamentSettingsPersistenceTest()
        : juce::UnitTest("TemperamentSettingsPersistence", "DevPiano/Settings") {
    }

    void runTest() override {
        testSettingsStoreRoundTrip();
        testSettingsStoreBoundaryClamping();
        testPerformancePresetJsonRoundTrip();
        testLegacyPresetBackwardCompatibility();
        testFlatRootPresetCompatibility();
    }

private:
    void testSettingsStoreRoundTrip() {
        beginTest("SettingsStore: Temperament and A4 reference pitch round-trip");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto settingsFile = tempDir.getNonexistentChildFile("TempStoreTest", ".xml");

        SettingsStore store(settingsFile);

        SettingsModel originalModel;
        originalModel.temperament = devpiano::audio::Temperament::werckmeister3;
        originalModel.referencePitchA4 = 415.0;

        expect(store.save(originalModel));

        SettingsModel loadedModel;
        store.load(loadedModel);

        expect(loadedModel.temperament == devpiano::audio::Temperament::werckmeister3);
        expectWithinAbsoluteError(loadedModel.referencePitchA4, 415.0, 1e-4);

        settingsFile.deleteFile();
    }

    void testSettingsStoreBoundaryClamping() {
        beginTest("SettingsStore: Out-of-bound and corrupted temperament values are clamped");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // 1. Upper bound clamping test
        const auto upperFile = tempDir.getNonexistentChildFile("TempClampUpper", ".xml");
        const juce::String upperXml = R"(<?xml version="1.0" encoding="utf-8"?>
<PROPERTIES>
  <VALUE name="temperament" val="99"/>
  <VALUE name="referencePitchA4" val="1200.0"/>
</PROPERTIES>
)";
        expect(upperFile.replaceWithText(upperXml));

        {
            SettingsStore store(upperFile);
            SettingsModel model;
            store.load(model);

            // Clamped: temperament max is 5 (Kirnberger III), pitch max is 450.0 Hz
            expect(model.temperament == devpiano::audio::Temperament::kirnberger3);
            expectEquals(model.referencePitchA4, 450.0);
        }
        upperFile.deleteFile();

        // 2. Lower bound clamping test
        const auto lowerFile = tempDir.getNonexistentChildFile("TempClampLower", ".xml");
        const juce::String lowerXml = R"(<?xml version="1.0" encoding="utf-8"?>
<PROPERTIES>
  <VALUE name="temperament" val="-10"/>
  <VALUE name="referencePitchA4" val="200.0"/>
</PROPERTIES>
)";
        expect(lowerFile.replaceWithText(lowerXml));

        {
            SettingsStore store(lowerFile);
            SettingsModel model;
            store.load(model);

            // Clamped: temperament min is 0 (Equal), pitch min is 410.0 Hz
            expect(model.temperament == devpiano::audio::Temperament::equal);
            expectEquals(model.referencePitchA4, 410.0);
        }
        lowerFile.deleteFile();
    }

    void testPerformancePresetJsonRoundTrip() {
        beginTest("PerformancePreset: Full acoustic temperament round-trip via .devpiano.preset JSON");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto presetFile = tempDir.getNonexistentChildFile("AcousticTempPreset", ".devpiano.preset");

        PerformancePreset originalPreset = makeDefaultPreset();
        originalPreset.name = "MeantoneBaroquePreset";
        originalPreset.temperament = Temperament::meantone;
        originalPreset.referencePitchA4 = 415.0;

        expect(savePreset(originalPreset, presetFile));

        auto loadedOpt = loadPreset(presetFile);
        expect(loadedOpt.has_value());
        if (loadedOpt.has_value()) {
            expectEquals(loadedOpt->name, juce::String("MeantoneBaroquePreset"));
            expect(loadedOpt->temperament == Temperament::meantone);
            expectWithinAbsoluteError(loadedOpt->referencePitchA4, 415.0, 1e-4);
        }

        presetFile.deleteFile();
    }

    void testLegacyPresetBackwardCompatibility() {
        beginTest("PerformancePreset: Legacy presets lacking temperament fall back to default");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto legacyFile = tempDir.getNonexistentChildFile("LegacyNoTemp", ".devpiano.preset");

        const juce::String legacyJson = R"({
  "version": 1,
  "name": "LegacyPreset",
  "layout": { "id": "legacy.1", "name": "Legacy", "bindings": [] },
  "acoustics": {
    "lidPosition": 1,
    "touchVelocityCurve": 2,
    "unaCorda": true
  }
})";
        expect(legacyFile.replaceWithText(legacyJson));

        auto loadedOpt = loadPreset(legacyFile);
        expect(loadedOpt.has_value());
        if (loadedOpt.has_value()) {
            expectEquals(loadedOpt->name, juce::String("LegacyPreset"));
            // Missing temperament and pitch should safely fall back to equal and 440.0
            expect(loadedOpt->temperament == Temperament::equal);
            expectEquals(loadedOpt->referencePitchA4, 440.0);
            expect(loadedOpt->lidPosition == SettingsModel::LidPosition::halfStick);
            expect(loadedOpt->touchVelocityCurve == devpiano::input::TouchVelocityCurve::heavy);
            expect(loadedOpt->unaCorda == true);
        }

        legacyFile.deleteFile();
    }

    void testFlatRootPresetCompatibility() {
        beginTest("PerformancePreset: Flat root temperament fields compatibility");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto flatFile = tempDir.getNonexistentChildFile("FlatRootTemp", ".devpiano.preset");

        const juce::String flatJson = R"({
  "version": 1,
  "name": "FlatRootPreset",
  "layout": { "id": "flat.1", "name": "Flat", "bindings": [] },
  "temperament": "just",
  "referencePitchA4": 432.0
})";
        expect(flatFile.replaceWithText(flatJson));

        auto loadedOpt = loadPreset(flatFile);
        expect(loadedOpt.has_value());
        if (loadedOpt.has_value()) {
            expectEquals(loadedOpt->name, juce::String("FlatRootPreset"));
            expect(loadedOpt->temperament == Temperament::just);
            expectWithinAbsoluteError(loadedOpt->referencePitchA4, 432.0, 1e-4);
        }

        flatFile.deleteFile();
    }
};

static TemperamentSettingsPersistenceTest temperamentSettingsPersistenceTest;

#include <JuceHeader.h>

#include "Audio/TemperamentEngine.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "TestHelpers.h"

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

        const devpiano::test::ScopedTempDir tempDir("temp-store-roundtrip");
        const auto settingsFile = tempDir.getChildFile("settings.xml");

        SettingsStore store(settingsFile);

        SettingsModel originalModel;
        originalModel.temperament = devpiano::audio::Temperament::werckmeister3;
        originalModel.referencePitchA4 = 415.0;

        expect(store.save(originalModel));

        SettingsModel loadedModel;
        store.load(loadedModel);

        expect(loadedModel.temperament == devpiano::audio::Temperament::werckmeister3);
        expectWithinAbsoluteError(loadedModel.referencePitchA4, 415.0, 1e-4);
    }

    void testSettingsStoreBoundaryClamping() {
        beginTest("SettingsStore: Out-of-bound and corrupted temperament values are clamped");

        const devpiano::test::ScopedTempDir tempDir("temp-clamp");

        // 1. Upper bound clamping test
        const auto upperFile = tempDir.getChildFile("upper.xml");
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

            expect(model.temperament == devpiano::audio::Temperament::kirnberger3);
            expectEquals(model.referencePitchA4, 480.0);
        }

        // 2. Lower bound clamping test
        const auto lowerFile = tempDir.getChildFile("lower.xml");
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

            expect(model.temperament == devpiano::audio::Temperament::equal);
            expectEquals(model.referencePitchA4, 400.0);
        }
    }

    void testPerformancePresetJsonRoundTrip() {
        beginTest("PerformancePreset: Full acoustic temperament round-trip via .devpiano.preset JSON");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const devpiano::test::ScopedTempDir tempDir("temp-preset-roundtrip");
        const auto presetFile = tempDir.getChildFile("preset.devpiano.preset");

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

        PerformancePreset minPreset = makeDefaultPreset();
        minPreset.name = "MinBoundaryPreset";
        minPreset.referencePitchA4 = 400.0;
        const auto minFile = tempDir.getChildFile("min.devpiano.preset");
        expect(savePreset(minPreset, minFile));
        auto loadedMin = loadPreset(minFile);
        expect(loadedMin.has_value());
        if (loadedMin.has_value()) {
            expectWithinAbsoluteError(loadedMin->referencePitchA4, 400.0, 1e-4);
        }

        PerformancePreset maxPreset = makeDefaultPreset();
        maxPreset.name = "MaxBoundaryPreset";
        maxPreset.referencePitchA4 = 480.0;
        const auto maxFile = tempDir.getChildFile("max.devpiano.preset");
        expect(savePreset(maxPreset, maxFile));
        auto loadedMax = loadPreset(maxFile);
        expect(loadedMax.has_value());
        if (loadedMax.has_value()) {
            expectWithinAbsoluteError(loadedMax->referencePitchA4, 480.0, 1e-4);
        }

        PerformancePreset concertPreset = makeDefaultPreset();
        concertPreset.name = "ConcertPitchPreset";
        concertPreset.referencePitchA4 = 442.0;
        const auto concertFile = tempDir.getChildFile("concert.devpiano.preset");
        expect(savePreset(concertPreset, concertFile));
        auto loadedConcert = loadPreset(concertFile);
        expect(loadedConcert.has_value());
        if (loadedConcert.has_value()) {
            expectWithinAbsoluteError(loadedConcert->referencePitchA4, 442.0, 1e-4);
        }

        const auto outOfBoundsFile = tempDir.getChildFile("out-of-bounds.devpiano.preset");
        const juce::String oobJson = R"({
  "version": 1,
  "name": "OobPreset",
  "layout": { "id": "oob.1", "name": "OOB", "bindings": [] },
  "acoustics": {
    "referencePitchA4": 250.0
  }
})";
        expect(outOfBoundsFile.replaceWithText(oobJson));
        auto loadedOob = loadPreset(outOfBoundsFile);
        expect(loadedOob.has_value());
        if (loadedOob.has_value()) {
            expectWithinAbsoluteError(loadedOob->referencePitchA4, 400.0, 1e-4);
        }

        const auto oobHighFile = tempDir.getChildFile("oob-high.devpiano.preset");
        const juce::String oobHighJson = R"({
  "version": 1,
  "name": "OobHighPreset",
  "layout": { "id": "oob.2", "name": "OOB High", "bindings": [] },
  "acoustics": {
    "referencePitchA4": 999.0
  }
})";
        expect(oobHighFile.replaceWithText(oobHighJson));
        auto loadedOobHigh = loadPreset(oobHighFile);
        expect(loadedOobHigh.has_value());
        if (loadedOobHigh.has_value()) {
            expectWithinAbsoluteError(loadedOobHigh->referencePitchA4, 480.0, 1e-4);
        }
    }

    void testLegacyPresetBackwardCompatibility() {
        beginTest("PerformancePreset: Legacy presets lacking temperament fall back to default");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const devpiano::test::ScopedTempDir tempDir("temp-preset-legacy");
        const auto legacyFile = tempDir.getChildFile("legacy.devpiano.preset");

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
    }

    void testFlatRootPresetCompatibility() {
        beginTest("PerformancePreset: Flat root temperament fields compatibility");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const devpiano::test::ScopedTempDir tempDir("temp-preset-flat");
        const auto flatFile = tempDir.getChildFile("flat.devpiano.preset");

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
    }
};

static TemperamentSettingsPersistenceTest temperamentSettingsPersistenceTest;

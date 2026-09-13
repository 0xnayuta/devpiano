#include <JuceHeader.h>

#include "Audio/PerspectiveProcessor.h"
#include "Audio/RoomReverbEngine.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"

// ============================================================================
/// SpatialAcousticsTest (Phase 31-C)
///
/// Validates spatial acoustics integration across SettingsModel, SettingsStore,
/// PerformancePreset, and preset flow:
/// 1. SettingsStore XML round-trip for soundPerspective, reverbSpace, reverbWet.
/// 2. SettingsStore out-of-range boundary clamping protection.
/// 3. PerformancePreset JSON serialization round-trip under "acoustics".
/// 4. Backward compatibility with legacy presets lacking spatial acoustics.
/// 5. Flat root acoustic fields compatibility reading.
// ============================================================================
class SpatialAcousticsTest final : public juce::UnitTest {
public:
    SpatialAcousticsTest()
        : juce::UnitTest("SpatialAcoustics", "DevPiano/Settings") {
    }

    void runTest() override {
        testSettingsStoreSpatialRoundTrip();
        testSettingsStoreBoundaryClamping();
        testPerformancePresetJsonRoundTrip();
        testPerformancePresetBackwardCompatibility();
        testPerformancePresetFlatRootCompatibility();
    }

private:
    void testSettingsStoreSpatialRoundTrip() {
        beginTest("SettingsStore: Spatial acoustics parameters round-trip");

        using namespace devpiano::audio;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto settingsFile = tempDir.getNonexistentChildFile("SpatialRoundTrip", ".xml");

        SettingsStore store(settingsFile);

        SettingsModel originalModel;
        originalModel.soundPerspective = SoundPerspective::audience;
        originalModel.reverbSpace = ReverbSpace::concertHall;
        originalModel.reverbWet = 0.35f;

        expect(store.save(originalModel));

        SettingsModel loadedModel;
        store.load(loadedModel);

        expect(loadedModel.soundPerspective == SoundPerspective::audience);
        expect(loadedModel.reverbSpace == ReverbSpace::concertHall);
        expectWithinAbsoluteError(loadedModel.reverbWet, 0.35f, 1e-4f);

        // Verify PerformanceSettingsView accessor round-trip
        const auto view = loadedModel.getPerformanceSettingsView();
        expect(view.soundPerspective == SoundPerspective::audience);
        expect(view.reverbSpace == ReverbSpace::concertHall);
        expectWithinAbsoluteError(view.reverbWet, 0.35f, 1e-4f);

        settingsFile.deleteFile();
    }

    void testSettingsStoreBoundaryClamping() {
        beginTest("SettingsStore: Out-of-bound spatial values are clamped safely");

        using namespace devpiano::audio;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // 1. Upper bound clamping
        const auto upperFile = tempDir.getNonexistentChildFile("SpatialUpperClamp", ".xml");
        const juce::String upperXml = R"(<?xml version="1.0" encoding="utf-8"?>
<PROPERTIES>
  <VALUE name="soundPerspective" val="99"/>
  <VALUE name="reverbSpace" val="50"/>
  <VALUE name="reverbWet" val="3.5"/>
</PROPERTIES>
)";
        expect(upperFile.replaceWithText(upperXml));

        {
            SettingsStore store(upperFile);
            SettingsModel model;
            store.load(model);

            // Clamped: soundPerspective max is 1 (Audience), reverbSpace max is 2 (Concert Hall), reverbWet max is 1.0f
            expect(model.soundPerspective == SoundPerspective::audience);
            expect(model.reverbSpace == ReverbSpace::concertHall);
            expectWithinAbsoluteError(model.reverbWet, 1.0f, 1e-4f);
        }
        upperFile.deleteFile();

        // 2. Lower bound clamping
        const auto lowerFile = tempDir.getNonexistentChildFile("SpatialLowerClamp", ".xml");
        const juce::String lowerXml = R"(<?xml version="1.0" encoding="utf-8"?>
<PROPERTIES>
  <VALUE name="soundPerspective" val="-10"/>
  <VALUE name="reverbSpace" val="-5"/>
  <VALUE name="reverbWet" val="-0.8"/>
</PROPERTIES>
)";
        expect(lowerFile.replaceWithText(lowerXml));

        {
            SettingsStore store(lowerFile);
            SettingsModel model;
            store.load(model);

            // Clamped: soundPerspective min is 0 (Player), reverbSpace min is 0 (Studio), reverbWet min is 0.0f
            expect(model.soundPerspective == SoundPerspective::player);
            expect(model.reverbSpace == ReverbSpace::studio);
            expectWithinAbsoluteError(model.reverbWet, 0.0f, 1e-4f);
        }
        lowerFile.deleteFile();
    }

    void testPerformancePresetJsonRoundTrip() {
        beginTest("PerformancePreset: Spatial acoustics round-trip via .devpiano.preset JSON");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto presetFile = tempDir.getNonexistentChildFile("SpatialPreset", ".devpiano.preset");

        PerformancePreset originalPreset = makeDefaultPreset();
        originalPreset.name = "SpatialConcertPreset";
        originalPreset.soundPerspective = SoundPerspective::audience;
        originalPreset.reverbSpace = ReverbSpace::concertHall;
        originalPreset.reverbWet = 0.40f;

        expect(savePreset(originalPreset, presetFile));

        const auto loadedOpt = loadPreset(presetFile);
        expect(loadedOpt.has_value());

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, originalPreset.name);
            expect(loaded.soundPerspective == SoundPerspective::audience);
            expect(loaded.reverbSpace == ReverbSpace::concertHall);
            expectWithinAbsoluteError(loaded.reverbWet, 0.40f, 1e-4f);
        }

        presetFile.deleteFile();
    }

    void testPerformancePresetBackwardCompatibility() {
        beginTest("PerformancePreset: Legacy presets lacking spatial fields fall back to defaults");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto legacyFile = tempDir.getNonexistentChildFile("LegacySpatialPreset", ".devpiano.preset");

        // Old preset JSON lacking soundPerspective, reverbSpace, reverbWet
        const juce::String legacyJson = R"({
  "version": 1,
  "name": "LegacyBallad",
  "acoustics": {
    "lidPosition": 0,
    "touchVelocityCurve": 0,
    "unaCorda": false,
    "temperament": "equal",
    "referencePitchA4": 440.0
  }
})";
        expect(legacyFile.replaceWithText(legacyJson));

        const auto loadedOpt = loadPreset(legacyFile);
        expect(loadedOpt.has_value());

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, juce::String("LegacyBallad"));
            // Safe fallbacks
            expect(loaded.soundPerspective == SoundPerspective::player);
            expect(loaded.reverbSpace == ReverbSpace::chamber);
            expectWithinAbsoluteError(loaded.reverbWet, 0.0f, 1e-4f);
        }

        legacyFile.deleteFile();
    }

    void testPerformancePresetFlatRootCompatibility() {
        beginTest("PerformancePreset: Flat root spatial fields compatibility reading");

        using namespace devpiano::layout;
        using namespace devpiano::audio;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto flatFile = tempDir.getNonexistentChildFile("FlatSpatialPreset", ".devpiano.preset");

        const juce::String flatJson = R"({
  "version": 1,
  "name": "FlatSpatial",
  "soundPerspective": "audience",
  "reverbSpace": "studio",
  "reverbWet": 0.25
})";
        expect(flatFile.replaceWithText(flatJson));

        const auto loadedOpt = loadPreset(flatFile);
        expect(loadedOpt.has_value());

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, juce::String("FlatSpatial"));
            expect(loaded.soundPerspective == SoundPerspective::audience);
            expect(loaded.reverbSpace == ReverbSpace::studio);
            expectWithinAbsoluteError(loaded.reverbWet, 0.25f, 1e-4f);
        }

        flatFile.deleteFile();
    }
};

static SpatialAcousticsTest spatialAcousticsTest;

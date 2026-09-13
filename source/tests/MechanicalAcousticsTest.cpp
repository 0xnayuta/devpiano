#include <JuceHeader.h>

#include "Audio/PianoSynthVoice.h"
#include "Export/ExportFlowSupport.h"
#include "Layout/PerformancePreset.h"
#include "Recording/RecordingEngine.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"

#include <cmath>

// ============================================================================
/// MechanicalAcousticsTest (Phase 32-D)
///
/// Validates full-stack integration of mechanical action noise and felt ageing
/// across settings persistence, preset serialization, offline export parity and
/// extreme-parameter safety:
/// 1. SettingsStore XML round-trip for pedalNoiseLevel / feltAgeingAmount.
/// 2. SettingsStore out-of-range boundary clamping protection.
/// 3. PerformancePreset JSON serialization round-trip under "acoustics".
/// 4. Backward compatibility with legacy presets lacking mechanical fields.
/// 5. Flat root mechanical field compatibility with clamping.
/// 6. WAV export option propagation (offline render parity with realtime).
/// 7. Extreme-parameter safety: bounded output, finite samples, click-free attack.
// ============================================================================
class MechanicalAcousticsTest final : public juce::UnitTest {
public:
    MechanicalAcousticsTest()
        : juce::UnitTest("MechanicalAcoustics", "DevPiano/Settings") {
    }

    void runTest() override {
        testSettingsStoreMechanicalRoundTrip();
        testSettingsStoreBoundaryClamping();
        testPerformancePresetJsonRoundTrip();
        testPerformancePresetBackwardCompatibility();
        testPerformancePresetFlatRootCompatibility();
        testWavExportOptionsPropagation();
        testExtremeParameterSafetyAndClickFreeAttack();
    }

private:
    void testSettingsStoreMechanicalRoundTrip() {
        beginTest("SettingsStore: Mechanical acoustics parameters round-trip");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto settingsFile = tempDir.getNonexistentChildFile("MechanicalRoundTrip", ".xml");

        SettingsStore store(settingsFile);

        SettingsModel originalModel;
        originalModel.pedalNoiseLevel = 0.35f;
        originalModel.feltAgeingAmount = 0.6f;

        expect(store.save(originalModel));

        SettingsModel loadedModel;
        store.load(loadedModel);

        expectWithinAbsoluteError(loadedModel.pedalNoiseLevel, 0.35f, 1e-4f);
        expectWithinAbsoluteError(loadedModel.feltAgeingAmount, 0.6f, 1e-4f);

        // Verify PerformanceSettingsView accessor round-trip
        const auto view = loadedModel.getPerformanceSettingsView();
        expectWithinAbsoluteError(view.pedalNoiseLevel, 0.35f, 1e-4f);
        expectWithinAbsoluteError(view.feltAgeingAmount, 0.6f, 1e-4f);

        settingsFile.deleteFile();
    }

    void testSettingsStoreBoundaryClamping() {
        beginTest("SettingsStore: Out-of-bound mechanical values are clamped safely");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // 1. Upper bound clamping
        const auto upperFile = tempDir.getNonexistentChildFile("MechanicalUpperClamp", ".xml");
        const juce::String upperXml = R"(<?xml version="1.0" encoding="utf-8"?>
<PROPERTIES>
  <VALUE name="pedalNoiseLevel" val="3.5"/>
  <VALUE name="feltAgeingAmount" val="9.0"/>
</PROPERTIES>
)";
        expect(upperFile.replaceWithText(upperXml));

        {
            SettingsStore store(upperFile);
            SettingsModel model;
            store.load(model);

            // Clamped to maximum 1.0f
            expectWithinAbsoluteError(model.pedalNoiseLevel, 1.0f, 1e-4f);
            expectWithinAbsoluteError(model.feltAgeingAmount, 1.0f, 1e-4f);
        }
        upperFile.deleteFile();

        // 2. Lower bound clamping
        const auto lowerFile = tempDir.getNonexistentChildFile("MechanicalLowerClamp", ".xml");
        const juce::String lowerXml = R"(<?xml version="1.0" encoding="utf-8"?>
<PROPERTIES>
  <VALUE name="pedalNoiseLevel" val="-0.8"/>
  <VALUE name="feltAgeingAmount" val="-3.0"/>
</PROPERTIES>
)";
        expect(lowerFile.replaceWithText(lowerXml));

        {
            SettingsStore store(lowerFile);
            SettingsModel model;
            store.load(model);

            // Clamped to minimum 0.0f
            expectWithinAbsoluteError(model.pedalNoiseLevel, 0.0f, 1e-4f);
            expectWithinAbsoluteError(model.feltAgeingAmount, 0.0f, 1e-4f);
        }
        lowerFile.deleteFile();
    }

    void testPerformancePresetJsonRoundTrip() {
        beginTest("PerformancePreset: Mechanical acoustics round-trip via .devpiano.preset JSON");

        using namespace devpiano::layout;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto presetFile = tempDir.getNonexistentChildFile("MechanicalPreset", ".devpiano.preset");

        PerformancePreset originalPreset = makeDefaultPreset();
        originalPreset.name = "MechanicalConcertPreset";
        originalPreset.pedalNoiseLevel = 0.42f;
        originalPreset.feltAgeingAmount = 0.77f;

        expect(savePreset(originalPreset, presetFile));

        const auto loadedOpt = loadPreset(presetFile);
        expect(loadedOpt.has_value());

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, originalPreset.name);
            expectWithinAbsoluteError(loaded.pedalNoiseLevel, 0.42f, 1e-4f);
            expectWithinAbsoluteError(loaded.feltAgeingAmount, 0.77f, 1e-4f);
        }

        presetFile.deleteFile();
    }

    void testPerformancePresetBackwardCompatibility() {
        beginTest("PerformancePreset: Legacy presets lacking mechanical fields fall back to defaults");

        using namespace devpiano::layout;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto legacyFile = tempDir.getNonexistentChildFile("LegacyMechanicalPreset", ".devpiano.preset");

        // Old preset JSON lacking pedalNoiseLevel and feltAgeingAmount
        const juce::String legacyJson = R"({
  "version": 1,
  "name": "LegacyMechanical",
  "acoustics": {
    "lidPosition": 0,
    "touchVelocityCurve": 0,
    "unaCorda": false,
    "temperament": "equal",
    "referencePitchA4": 440.0,
    "soundPerspective": "player",
    "reverbSpace": "chamber",
    "reverbWet": 0.2
  }
})";
        expect(legacyFile.replaceWithText(legacyJson));

        const auto loadedOpt = loadPreset(legacyFile);
        expect(loadedOpt.has_value());

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, juce::String("LegacyMechanical"));
            // Safe fallbacks: default pedal noise 0.6f, ageing off (0.0f)
            expectWithinAbsoluteError(loaded.pedalNoiseLevel, 0.6f, 1e-4f);
            expectWithinAbsoluteError(loaded.feltAgeingAmount, 0.0f, 1e-4f);
        }

        legacyFile.deleteFile();
    }

    void testPerformancePresetFlatRootCompatibility() {
        beginTest("PerformancePreset: Flat root mechanical fields compatibility with clamping");

        using namespace devpiano::layout;

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto flatFile = tempDir.getNonexistentChildFile("FlatMechanicalPreset", ".devpiano.preset");

        // Flat root fields: pedalNoiseLevel in range, feltAgeingAmount out of range
        const juce::String flatJson = R"({
  "version": 1,
  "name": "FlatMechanical",
  "pedalNoiseLevel": 0.25,
  "feltAgeingAmount": 1.5
})";
        expect(flatFile.replaceWithText(flatJson));

        const auto loadedOpt = loadPreset(flatFile);
        expect(loadedOpt.has_value());

        if (loadedOpt.has_value()) {
            const auto& loaded = *loadedOpt;
            expectEquals(loaded.name, juce::String("FlatMechanical"));
            expectWithinAbsoluteError(loaded.pedalNoiseLevel, 0.25f, 1e-4f);
            // Out-of-range value clamped to 1.0f
            expectWithinAbsoluteError(loaded.feltAgeingAmount, 1.0f, 1e-4f);
        }

        flatFile.deleteFile();
    }

    void testWavExportOptionsPropagation() {
        beginTest("WAV export options: Mechanical parameters propagate for offline parity");

        devpiano::recording::RecordingTake take;
        take.sampleRate = 44100.0;

        SettingsModel::PerformanceSettingsView perf;
        perf.pedalNoiseLevel = 0.45f;
        perf.feltAgeingAmount = 0.8f;

        const auto options = devpiano::exporting::buildWavExportOptions(take, perf, 44100.0, 512);
        expectWithinAbsoluteError(options.pedalNoiseLevel, 0.45f, 1e-4f);
        expectWithinAbsoluteError(options.feltAgeingAmount, 0.8f, 1e-4f);
    }

    void testExtremeParameterSafetyAndClickFreeAttack() {
        beginTest("Mechanical transients: extreme parameters stay bounded, finite and click-free");

        PianoSynthVoice voice;
        voice.setCurrentPlaybackSampleRate(48000.0);
        voice.setVoiceIndex(0);
        voice.setPedalNoiseLevel(1.0f);
        voice.setFeltAgeingAmount(1.0f);

        juce::AudioBuffer<float> buffer(2, 512);

        // Click-free attack: first non-zero sample after pedal press must be tiny
        // (12 ms raised-cosine envelope ramp means no instantaneous step).
        buffer.clear();
        voice.controllerMoved(64, 127);
        voice.renderNextBlock(buffer, 0, 512);

        float firstNonZero = 0.0f;
        float maxPeak = 0.0f;
        bool allFinite = true;

        for (int i = 0; i < 512; ++i) {
            const auto sample = buffer.getSample(0, i);
            if (firstNonZero == 0.0f && sample != 0.0f) {
                firstNonZero = std::abs(sample);
            }
            if (!std::isfinite(sample)) {
                allFinite = false;
            }
        }
        maxPeak = std::max(maxPeak, buffer.getMagnitude(0, 0, 512));
        maxPeak = std::max(maxPeak, buffer.getMagnitude(1, 0, 512));

        expect(allFinite, "extreme mechanical parameters must never emit NaN/Inf");
        expectGreaterThan(firstNonZero, 0.0f, "pedal noise must trigger audible output");
        expectLessThan(firstNonZero, 0.01f, "attack must ramp from silence without a click step");

        // Rapid press/release cycling under maximum noise level: bounded output.
        for (int cycle = 0; cycle < 8; ++cycle) {
            voice.controllerMoved(64, cycle % 2 == 0 ? 0 : 127);
            for (int block = 0; block < 4; ++block) {
                buffer.clear();
                voice.renderNextBlock(buffer, 0, 512);
                for (int ch = 0; ch < 2; ++ch) {
                    maxPeak = std::max(maxPeak, buffer.getMagnitude(ch, 0, 512));
                    for (int i = 0; i < 512; ++i) {
                        if (!std::isfinite(buffer.getSample(ch, i))) {
                            allFinite = false;
                        }
                    }
                }
            }
        }

        expect(allFinite, "rapid pedal cycling must stay finite");
        expectLessThan(maxPeak, 0.5f, "mechanical transients must stay well below clipping");
    }
};

static MechanicalAcousticsTest mechanicalAcousticsTest;

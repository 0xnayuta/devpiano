#include <JuceHeader.h>

#include "Layout/PerformancePreset.h"
#include "TestHelpers.h"

using namespace devpiano::layout;
using namespace devpiano::midi;

// =============================================================================
// Tests for PerformancePreset persistence (AUDIT TEST-003):
//   - save → load round-trip of every field (including all 128 custom key
//     labels and colours)
//   - sanitisePresetFileName special characters / trimming / empty fallback
//   - resolvePresetFile canonical name-to-path derivation
//   - corrupt or invalid files return nullopt
//   - formatVersion mismatch rejection
// =============================================================================

namespace {

// 构造一个全字段填充的预设（round-trip 用）。
PerformancePreset makeFullPreset() {
    PerformancePreset p;
    p.uuid = "12345678-1234-5678-1234-567812345678";
    p.name = "My Preset";
    p.layout.id = "user.test";
    p.layout.name = "Test Layout";
    p.layout.bindings = {
        { 65, "A", { devpiano::core::KeyActionType::note, devpiano::core::KeyTrigger::keyDown, 60, 1, 1.0f } },
        { 83, "S", { devpiano::core::KeyActionType::note, devpiano::core::KeyTrigger::keyDown, 62, 1, 0.9f } },
    };
    p.channelMatrix.active = true;
    p.channelMatrix.channels[0].outputChannel = 5;
    p.channelMatrix.channels[0].transpose = 3;
    p.channelMatrix.channels[0].followKey = true;
    p.channelMatrix.channels[7].velocity = 100;
    p.keySignature = 7;
    p.midiTranspose = true;
    p.colourMode = devpiano::ui::KeyColourMode::velocity;
    p.noteDisplay = devpiano::ui::NoteDisplayMode::noteName;
    p.fadeSpeed = 0.85f;
    p.previewAlpha = 0.0f;

    p.customKeyLabels[60] = "Middle C";
    p.customKeyLabels[72] = "High C";
    p.customKeyColours[60] = juce::Colour(0xff112233);
    p.customKeyColours[72] = juce::Colour(0xff445566);
    return p;
}

void expectPresetsEqual(juce::UnitTest& ut, const PerformancePreset& a, const PerformancePreset& b) {
    ut.expectEquals(a.uuid, b.uuid);
    ut.expectEquals(a.name, b.name);
    ut.expectEquals(a.layout.id, b.layout.id);
    ut.expectEquals(a.layout.name, b.layout.name);
    ut.expectEquals(a.layout.bindings.size(), b.layout.bindings.size());
    if (a.layout.bindings.size() == b.layout.bindings.size()) {
        for (std::size_t i = 0; i < a.layout.bindings.size(); ++i) {
            ut.expectEquals(a.layout.bindings[i].keyCode, b.layout.bindings[i].keyCode);
            ut.expectEquals(a.layout.bindings[i].displayText, b.layout.bindings[i].displayText);
            ut.expect(a.layout.bindings[i].action.type == b.layout.bindings[i].action.type);
            ut.expect(a.layout.bindings[i].action.trigger == b.layout.bindings[i].action.trigger);
            ut.expectEquals(a.layout.bindings[i].action.midiNote, b.layout.bindings[i].action.midiNote);
            ut.expectEquals(a.layout.bindings[i].action.midiChannel, b.layout.bindings[i].action.midiChannel);
            ut.expectWithinAbsoluteError(a.layout.bindings[i].action.velocity, b.layout.bindings[i].action.velocity,
                                         0.0001f);
        }
    }

    ut.expect(a.channelMatrix.active == b.channelMatrix.active);
    for (std::size_t i = 0; i < 16; ++i) {
        ut.expectEquals(static_cast<int>(a.channelMatrix.channels[i].outputChannel),
                        static_cast<int>(b.channelMatrix.channels[i].outputChannel));
        ut.expectEquals(static_cast<int>(a.channelMatrix.channels[i].transpose),
                        static_cast<int>(b.channelMatrix.channels[i].transpose));
        ut.expectEquals(static_cast<int>(a.channelMatrix.channels[i].octaveShift),
                        static_cast<int>(b.channelMatrix.channels[i].octaveShift));
        ut.expectEquals(static_cast<int>(a.channelMatrix.channels[i].velocity),
                        static_cast<int>(b.channelMatrix.channels[i].velocity));
        ut.expectEquals(static_cast<int>(a.channelMatrix.channels[i].program),
                        static_cast<int>(b.channelMatrix.channels[i].program));
        ut.expectEquals(static_cast<int>(a.channelMatrix.channels[i].bankMSB),
                        static_cast<int>(b.channelMatrix.channels[i].bankMSB));
        ut.expectEquals(static_cast<int>(a.channelMatrix.channels[i].sustainCC),
                        static_cast<int>(b.channelMatrix.channels[i].sustainCC));
        ut.expect(a.channelMatrix.channels[i].followKey == b.channelMatrix.channels[i].followKey);
    }

    ut.expectEquals(a.keySignature, b.keySignature);
    ut.expect(a.midiTranspose == b.midiTranspose);
    ut.expectEquals(static_cast<int>(a.colourMode), static_cast<int>(b.colourMode));
    ut.expectEquals(static_cast<int>(a.noteDisplay), static_cast<int>(b.noteDisplay));
    ut.expectWithinAbsoluteError(a.fadeSpeed, b.fadeSpeed, 0.0001f);
    ut.expectWithinAbsoluteError(a.previewAlpha, b.previewAlpha, 0.0001f);

    for (int i = 0; i < 128; ++i) {
        ut.expectEquals(a.customKeyLabels[static_cast<std::size_t>(i)], b.customKeyLabels[static_cast<std::size_t>(i)]);
        ut.expect(a.customKeyColours[static_cast<std::size_t>(i)] == b.customKeyColours[static_cast<std::size_t>(i)]);
    }
}

} // namespace

class PerformancePresetRoundTripTest final : public juce::UnitTest {
public:
    PerformancePresetRoundTripTest()
        : juce::UnitTest("PerformancePreset: save/load round-trip", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("full field round-trip survives save/load", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-roundtrip");
            auto path = tempDir.getChildFile("test.devpiano.preset");

            const auto original = makeFullPreset();
            expect(savePreset(original, path), "save must succeed");

            auto loaded = loadPreset(path);
            expect(loaded.has_value(), "load must succeed");
            if (loaded.has_value()) {
                expectPresetsEqual(*this, original, *loaded);
            }
        });

        testCase("savePreset appends the missing extension", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-ext");
            auto path = tempDir.getChildFile("bare-name"); // 无扩展名

            const auto original = makeFullPreset();
            expect(savePreset(original, path), "save must succeed");
            expect(path.withFileExtension("devpiano.preset").existsAsFile(), "file must get the preset extension");
        });

        testCase("loadPreset rejects a missing file", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-missing");
            expect(!loadPreset(tempDir.getChildFile("nope.devpiano.preset")).has_value());
        });

        testCase("loadPreset rejects an empty file", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-empty");
            auto path = tempDir.getChildFile("empty.devpiano.preset");
            path.replaceWithText("");
            expect(!loadPreset(path).has_value());
        });

        testCase("loadPreset rejects invalid JSON", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-badjson");
            auto path = tempDir.getChildFile("bad.devpiano.preset");
            path.replaceWithText("{ this is not json !!");
            expect(!loadPreset(path).has_value());
        });

        testCase("loadPreset rejects a non-object root", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-array");
            auto path = tempDir.getChildFile("arr.devpiano.preset");
            path.replaceWithText("[1, 2, 3]");
            expect(!loadPreset(path).has_value());
        });

        testCase("loadPreset rejects an oversized file (SEC-003)", [&] {
            devpiano::test::ScopedTempDir tempDir("oversized-preset");
            auto path = tempDir.getChildFile("huge.devpiano.preset");
            // Write > 1 MB payload
            juce::FileOutputStream out(path);
            expect(out.openedOk());
            juce::MemoryBlock block(1024 * 1024 + 128, true);
            out.write(block.getData(), block.getSize());
            out.flush();

            expect(!loadPreset(path).has_value(), "Oversized preset file (>1MB) must be rejected");
        });

        testCase("loadPreset rejects invalid version and clamps out-of-range fields (SEC-004, SEC-005)", [&] {
            devpiano::test::ScopedTempDir tempDir("compat-preset");
            auto pathInvalidVer = tempDir.getChildFile("future.devpiano.preset");
            pathInvalidVer.replaceWithText(R"({ "version": 999, "name": "Future" })");
            expect(!loadPreset(pathInvalidVer).has_value(), "Version > current must be rejected");

            auto pathClamped = tempDir.getChildFile("clamped.devpiano.preset");
            pathClamped.replaceWithText(
                R"({ "version": 1, "name": "Clamped", "keyboard": { "keySignature": 100, "fadeSpeed": 99.0, "previewAlpha": -5.0 } })");
            auto loaded = loadPreset(pathClamped);
            expect(loaded.has_value(), "Valid version 1 preset must be loaded");
            if (loaded.has_value()) {
                expect(loaded->uuid == generateDeterministicPresetUuid("Clamped"),
                       "v1 legacy preset gets deterministic UUID");
                expectEquals(loaded->keySignature, 7, "keySignature must be clamped to 7");
                expect(loaded->fadeSpeed >= 0.5f && loaded->fadeSpeed < 1.0f,
                       "imported fade must use a bounded contraction");
                expectEquals(loaded->previewAlpha, 0.0f, "previewAlpha must be clamped to 0.0");
            }
        });

        testCase("loadPreset rejects keyUp trigger and unknown trigger values (ERR-003)", [&] {
            devpiano::test::ScopedTempDir tempDir("trigger-preset");

            // 1. Preset with keyUp trigger must be rejected, preserving file intact
            auto pathKeyUp = tempDir.getChildFile("keyup.devpiano.preset");
            pathKeyUp.replaceWithText(
                R"({ "version": 1, "name": "KeyUpTest", "layout": { "bindings": [ { "keyCode": 65, "displayText": "A", "action": { "type": "note", "trigger": "keyUp", "midiNote": 60, "midiChannel": 1, "velocity": 1.0 } } ] } })");
            expect(!loadPreset(pathKeyUp).has_value(), "Preset with keyUp trigger must be rejected");
            expect(pathKeyUp.existsAsFile(), "Rejected file must be preserved intact on disk");
            expect(pathKeyUp.loadFileAsString().contains("keyUp"), "File content must not be rewritten");

            // 2. Preset with unknown trigger value must be rejected
            auto pathUnknown = tempDir.getChildFile("unknown.devpiano.preset");
            pathUnknown.replaceWithText(
                R"({ "version": 1, "name": "UnknownTest", "layout": { "bindings": [ { "keyCode": 65, "displayText": "A", "action": { "type": "note", "trigger": "onPress", "midiNote": 60, "midiChannel": 1, "velocity": 1.0 } } ] } })");
            expect(!loadPreset(pathUnknown).has_value(), "Preset with unknown trigger must be rejected");
            expect(pathUnknown.existsAsFile(), "Rejected file must be preserved intact on disk");

            // 3. Preset omitting optional trigger field must default to supported keyDown
            auto pathMissing = tempDir.getChildFile("missing.devpiano.preset");
            pathMissing.replaceWithText(
                R"({ "version": 1, "name": "MissingTest", "layout": { "bindings": [ { "keyCode": 65, "displayText": "A", "action": { "type": "note", "midiNote": 60, "midiChannel": 1, "velocity": 1.0 } } ] } })");
            auto loadedMissing = loadPreset(pathMissing);
            expect(loadedMissing.has_value(),
                   "Legacy preset omitting optional trigger field must be admitted with default keyDown");
            if (loadedMissing.has_value()) {
                expect(loadedMissing->layout.bindings[0].action.trigger == devpiano::core::KeyTrigger::keyDown);
            }
            // 4. Valid preset with keyDown trigger must load and execute successfully
            auto pathValid = tempDir.getChildFile("valid.devpiano.preset");
            pathValid.replaceWithText(
                R"({ "version": 1, "name": "ValidTest", "layout": { "bindings": [ { "keyCode": 65, "displayText": "A", "action": { "type": "note", "trigger": "keyDown", "midiNote": 60, "midiChannel": 1, "velocity": 1.0 } } ] } })");
            auto loaded = loadPreset(pathValid);
            expect(loaded.has_value(), "Valid preset with keyDown trigger must be loaded");
            if (loaded.has_value()) {
                expectEquals(loaded->layout.bindings.size(), static_cast<std::size_t>(1));
                expect(loaded->layout.bindings[0].action.trigger == devpiano::core::KeyTrigger::keyDown);

                // 5. Roundtrip saving preserves supported preset format
                auto pathSaved = tempDir.getChildFile("saved.devpiano.preset");
                expect(savePreset(*loaded, pathSaved), "Saving valid preset must succeed");
                auto reloaded = loadPreset(pathSaved);
                expect(reloaded.has_value(), "Reloading saved preset must succeed");
                if (reloaded.has_value()) {
                    expect(reloaded->layout.bindings[0].action.trigger == devpiano::core::KeyTrigger::keyDown);
                }
                const auto rawSaved = pathSaved.loadFileAsString();
                expect(rawSaved.contains(R"("trigger": "keyDown")"), "Saved JSON must serialize keyDown trigger");
            }
        });
        testCase("display name strips the preset extension", [&] {
            expectEquals(getPresetDisplayNameForFile(juce::File("/tmp/My Song.devpiano.preset")),
                         juce::String("My Song"));
        });

        testCase("makeDefaultPreset has the built-in identity", [&] {
            const auto preset = makeDefaultPreset();
            expectEquals(preset.name, juce::String("Default"));
            expectEquals(preset.uuid, generateDeterministicPresetUuid("Default"));
            expectEquals(preset.layout.id, juce::String("default.preset.builtin"));
            expect(preset.channelMatrix.active, "default matrix must be active");
            expectEquals(static_cast<int>(preset.colourMode), static_cast<int>(devpiano::ui::KeyColourMode::classic));
        });

        testCase("ARCH-003: persistent UUID preserved across save/load and deterministic for legacy v1", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-arch003");

            // 1. In-memory round-trip via performancePresetToVar / performancePresetFromVar
            const auto original = makeFullPreset();
            const auto varObj = performancePresetToVar(original);
            const auto fromVar = performancePresetFromVar(varObj);
            expect(fromVar.has_value(), "performancePresetFromVar must succeed");
            if (fromVar.has_value()) {
                expectPresetsEqual(*this, original, *fromVar);
            }

            // 2. Legacy v1 preset without uuid gets deterministic uuid derived from name
            const auto legacyFile = tempDir.getChildFile("legacy.devpiano.preset");
            legacyFile.replaceWithText(R"({ "version": 1, "name": "Jazz Grand" })");
            const auto loaded1 = loadPreset(legacyFile);
            expect(loaded1.has_value());
            if (loaded1.has_value()) {
                expectEquals(loaded1->uuid, generateDeterministicPresetUuid("Jazz Grand"));
                const auto loaded2 = loadPreset(legacyFile);
                expect(loaded2.has_value());
                if (loaded2.has_value()) {
                    expectEquals(loaded1->uuid, loaded2->uuid);
                }
            }

            // 3. Different legacy names produce distinct deterministic uuids
            expect(generateDeterministicPresetUuid("Grand A") != generateDeterministicPresetUuid("Grand B"));
        });
    }
};

static PerformancePresetRoundTripTest performancePresetRoundTripTest;

// -----------------------------------------------------------------------------

class PresetFileNameSanitiseTest final : public juce::UnitTest {
public:
    PresetFileNameSanitiseTest()
        : juce::UnitTest("PerformancePreset: file-name sanitising", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("reserved path characters are stripped",
                 [&] { expectEquals(sanitisePresetFileName(R"(a/b\c:d*e?f"g<h>i|j)"), juce::String("abcdefghij")); });

        testCase("alphanumerics, spaces, hyphens and underscores survive",
                 [&] { expectEquals(sanitisePresetFileName("My Song - 01_2"), juce::String("My Song - 01_2")); });

        testCase("whitespace-only name is trimmed to the fallback",
                 [&] { expectEquals(sanitisePresetFileName("   "), juce::String("untitled")); });

        testCase("empty name falls back to untitled",
                 [&] { expectEquals(sanitisePresetFileName(""), juce::String("untitled")); });

        testCase("trailing spaces are trimmed",
                 [&] { expectEquals(sanitisePresetFileName("Name  "), juce::String("Name")); });

        testCase("non-ASCII letters and Unicode survive", [&] {
            // juce::File::createLegalFileName preserves Unicode characters
            const auto unicodeName = juce::String::fromUTF8("\xe6\xbc\x94\xe5\xa5\x8f 01");
            expectEquals(sanitisePresetFileName(unicodeName), unicodeName);
        });
    }
};
static PresetFileNameSanitiseTest presetFileNameSanitiseTest;

// -----------------------------------------------------------------------------

class PresetDirectoryScanTest final : public juce::UnitTest {
public:
    PresetDirectoryScanTest()
        : juce::UnitTest("PerformancePreset: directory scan and cache consistency", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("scan directory ignores non-preset files and loads valid presets", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-dir-scan");

            // 1. Create a non-preset file
            tempDir.getChildFile("notes.txt").replaceWithText("some text");

            // 2. Create two valid preset files
            auto p1 = makeFullPreset();
            p1.name = "Preset Alpha";
            expect(savePreset(p1, tempDir.getChildFile("alpha.devpiano.preset")));

            auto p2 = makeFullPreset();
            p2.name = "Preset Beta";
            expect(savePreset(p2, tempDir.getChildFile("beta.devpiano.preset")));

            // 3. Scan directory
            const auto scanned = scanPresetDirectory(tempDir.get());
            expectEquals(static_cast<int>(scanned.size()), 2, "Must find exactly 2 valid preset files");

            juce::StringArray names;
            for (const auto& p : scanned) {
                names.add(p.name);
            }
            expect(names.contains("Preset Alpha"));
            expect(names.contains("Preset Beta"));
        });
    }
};

static PresetDirectoryScanTest presetDirectoryScanTest;

// -----------------------------------------------------------------------------

class PresetFileResolveTest final : public juce::UnitTest {
public:
    PresetFileResolveTest()
        : juce::UnitTest("PerformancePreset: preset file path resolution", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("name maps to the canonical path without creating anything", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-resolve");

            const auto resolved = resolvePresetFile("v2.1 Take", tempDir.get());
            expectEquals(resolved.getFileName(), juce::String("v2.1 Take.devpiano.preset"));
            expect(!resolved.existsAsFile(), "resolve must not create the file");
        });
    }
};

static PresetFileResolveTest presetFileResolveTest;

// -----------------------------------------------------------------------------

class PresetRenameTest final : public juce::UnitTest {
public:
    PresetRenameTest()
        : juce::UnitTest("PerformancePreset: rename persistence and collision protection (SEC-001)", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("rename A -> new B succeeds, preserves data/new metadata, cleans up old file", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-rename-new");
            auto pA = makeFullPreset();
            pA.name = "PresetA";
            pA.lidPosition = SettingsModel::LidPosition::halfStick;
            const auto fileA = tempDir.getChildFile("PresetA.devpiano.preset");
            expect(savePreset(pA, fileA));

            const auto res = renamePreset("PresetA", "PresetB", false, tempDir.get());
            expect(res == PresetRenameResult::success, "Rename to new file must succeed");

            const auto fileB = tempDir.getChildFile("PresetB.devpiano.preset");
            expect(fileB.existsAsFile(), "New preset file must exist");
            expect(!fileA.existsAsFile(), "Old preset file must be deleted");

            auto loadedB = loadPreset(fileB);
            expect(loadedB.has_value());
            if (loadedB.has_value()) {
                expectEquals(loadedB->uuid, pA.uuid, "UUID must be preserved across rename");
                expectEquals(loadedB->name, juce::String("PresetB"));
                expectEquals(loadedB->layout.name, juce::String("PresetB"));
                expectEquals(static_cast<int>(loadedB->lidPosition),
                             static_cast<int>(SettingsModel::LidPosition::halfStick));
            }
        });

        testCase("rename A -> existing independent B without permission is rejected and retains bytes (SEC-001)", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-rename-collision");
            auto pA = makeFullPreset();
            pA.name = "PresetA";
            pA.pedalNoiseLevel = 0.12f;
            const auto fileA = tempDir.getChildFile("PresetA.devpiano.preset");
            expect(savePreset(pA, fileA));
            const auto bytesA = fileA.loadFileAsString();

            auto pB = makeFullPreset();
            pB.name = "PresetB";
            pB.pedalNoiseLevel = 0.88f;
            const auto fileB = tempDir.getChildFile("PresetB.devpiano.preset");
            expect(savePreset(pB, fileB));
            const auto bytesB = fileB.loadFileAsString();

            const auto res = renamePreset("PresetA", "PresetB", false, tempDir.get());
            expect(res == PresetRenameResult::targetAlreadyExists,
                   "Declined/unpermitted collision must return targetAlreadyExists");

            expect(fileA.existsAsFile(), "Source file must remain");
            expect(fileB.existsAsFile(), "Target file must remain");
            expectEquals(fileA.loadFileAsString(), bytesA, "Source file bytes must be unchanged");
            expectEquals(fileB.loadFileAsString(), bytesB, "Target file bytes must be unchanged");

            const auto allFiles = tempDir.get().findChildFiles(juce::File::findFiles, false);
            expectEquals(allFiles.size(), 2, "Only PresetA and PresetB files should exist, no temp residue");
        });

        testCase("rename A -> existing independent B with permission overwrites target and cleans up source", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-rename-overwrite");
            auto pA = makeFullPreset();
            pA.name = "PresetA";
            pA.feltAgeingAmount = 0.95f;
            const auto fileA = tempDir.getChildFile("PresetA.devpiano.preset");
            expect(savePreset(pA, fileA));

            auto pB = makeFullPreset();
            pB.name = "PresetB";
            pB.feltAgeingAmount = 0.10f;
            const auto fileB = tempDir.getChildFile("PresetB.devpiano.preset");
            expect(savePreset(pB, fileB));

            const auto res = renamePreset("PresetA", "PresetB", true, tempDir.get());
            expect(res == PresetRenameResult::success, "Permitted collision must succeed");

            expect(!fileA.existsAsFile(), "Source file must be deleted");
            expect(fileB.existsAsFile(), "Target file must exist");
            auto loadedB = loadPreset(fileB);
            expect(loadedB.has_value());
            if (loadedB.has_value()) {
                expectEquals(loadedB->name, juce::String("PresetB"));
                expectEquals(loadedB->feltAgeingAmount, 0.95f, "Target must have new preset data from A");
            }
        });

        testCase(
            "rename to same normalized path via sanitization collision preserves target and updates metadata (SEC-001)",
            [&] {
                devpiano::test::ScopedTempDir tempDir("preset-rename-sanitise-collision");
                auto p = makeFullPreset();
                p.name = "PresetAlpha";
                p.pedalNoiseLevel = 0.44f;
                const auto file = tempDir.getChildFile("PresetAlpha.devpiano.preset");
                expect(savePreset(p, file));

                const auto res = renamePreset("PresetAlpha", "PresetAlpha?", false, tempDir.get());
                expect(res == PresetRenameResult::success, "Same normalized path rename must succeed");

                expect(file.existsAsFile(), "Same-path target file must NOT be deleted");
                auto loaded = loadPreset(file);
                expect(loaded.has_value());
                if (loaded.has_value()) {
                    expectEquals(loaded->name, juce::String("PresetAlpha?"), "Metadata name must reflect new name");
                    expectEquals(loaded->layout.name, juce::String("PresetAlpha?"));
                    expectEquals(loaded->pedalNoiseLevel, 0.44f, "Data must be preserved");
                }
            });

        testCase("case-only rename preserves target and updates metadata (SEC-001)", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-rename-case");
            auto p = makeFullPreset();
            p.name = "CaseTest";
            p.reverbWet = 0.73f;
            const auto fileOld = tempDir.getChildFile("CaseTest.devpiano.preset");
            expect(savePreset(p, fileOld));

            const auto res = renamePreset("CaseTest", "casetest", false, tempDir.get());
            expect(res == PresetRenameResult::success, "Case-only rename must succeed");

            const auto fileNew = resolvePresetFile("casetest", tempDir.get());
            expect(fileNew.existsAsFile(), "Target file must exist and not be deleted");

            auto loaded = loadPreset(fileNew);
            expect(loaded.has_value());
            if (loaded.has_value()) {
                expectEquals(loaded->name, juce::String("casetest"), "Metadata name must reflect new case");
                expectEquals(loaded->reverbWet, 0.73f, "Preset data must be preserved");
            }
        });
        testCase("renaming to a target with intermediate suffix does not collide with staging file", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-rename-staging-collision");
            auto p = makeFullPreset();
            p.name = "MyPreset";
            p.reverbWet = 0.55f;
            const auto fileOld = resolvePresetFile("MyPreset", tempDir.get());
            expect(savePreset(p, fileOld));
            expect(fileOld.existsAsFile());

            // Target name "MyPreset.devpiano_rename" would previously collide with
            // oldFile.getFileNameWithoutExtension() + "_rename" + kPresetFileExtension.
            const auto res = renamePreset("MyPreset", "MyPreset.devpiano_rename", false, tempDir.get());
            expect(res == PresetRenameResult::success, "Rename to dotted/intermediate name must succeed");

            const auto fileNew = resolvePresetFile("MyPreset.devpiano_rename", tempDir.get());
            expect(!fileOld.existsAsFile(), "Old source file must be moved/removed");
            expect(fileNew.existsAsFile(), "New target file must exist and NOT be deleted");

            auto loaded = loadPreset(fileNew);
            expect(loaded.has_value());
            if (loaded.has_value()) {
                expectEquals(loaded->name, juce::String("MyPreset.devpiano_rename"));
                expectEquals(loaded->reverbWet, 0.55f);
            }
        });

        testCase("validation and failure handling preserve original files and clean up temp files", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-rename-fail");
            auto p = makeFullPreset();
            p.name = "FailSource";
            const auto fileSource = tempDir.getChildFile("FailSource.devpiano.preset");
            expect(savePreset(p, fileSource));
            const auto sourceBytes = fileSource.loadFileAsString();

            expect(renamePreset("NonExistent", "Target", false, tempDir.get()) == PresetRenameResult::sourceNotFound);
            expect(renamePreset("", "Target", false, tempDir.get()) == PresetRenameResult::invalidName);
            expect(renamePreset("FailSource", "", false, tempDir.get()) == PresetRenameResult::invalidName);
            const auto blockedTarget = resolvePresetFile("Blocked", tempDir.get());
            expect(blockedTarget.createDirectory().wasOk());
            const auto targetData = blockedTarget.getChildFile("original");
            expect(targetData.replaceWithText("owned-target-data"));
            expect(renamePreset("FailSource", "Blocked", false, tempDir.get()) == PresetRenameResult::saveFailed);
            expectEquals(targetData.loadFileAsString(), juce::String("owned-target-data"));

            expect(fileSource.existsAsFile());
            expectEquals(fileSource.loadFileAsString(), sourceBytes);

            expectEquals(tempDir.get().getNumberOfChildFiles(juce::File::findFilesAndDirectories), 2,
                         "failed commit must restore the source without leaving staged files");
        });
#if JUCE_WINDOWS
        testCase("locked source prevents a rename without modifying either original preset", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-rename-locked-source");
            auto preset = makeFullPreset();
            preset.name = "Source";
            const auto source = resolvePresetFile(preset.name, tempDir.get());
            expect(savePreset(preset, source));
            preset.name = "Target";
            const auto target = resolvePresetFile(preset.name, tempDir.get());
            expect(savePreset(preset, target));
            const auto sourceBytes = source.loadFileAsString();
            const auto targetBytes = target.loadFileAsString();
            {
                juce::FileOutputStream lockSource(source);
                expect(lockSource.openedOk());
                expect(renamePreset("Source", "Target", true, tempDir.get()) == PresetRenameResult::sourceMoveFailed);
            }
            expectEquals(source.loadFileAsString(), sourceBytes);
            expectEquals(target.loadFileAsString(), targetBytes);
            expectEquals(tempDir.get().getNumberOfChildFiles(juce::File::findFiles), 2);
        });
#endif
    }
};

static PresetRenameTest presetRenameTest;

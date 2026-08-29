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
//   - corrupt or invalid files return nullopt
//   - formatVersion mismatch rejection
// =============================================================================

namespace {

// 构造一个全字段填充的预设（round-trip 用）。
PerformancePreset makeFullPreset() {
    PerformancePreset p;
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

        testCase("loadPreset rejects an unknown format version", [&] {
            devpiano::test::ScopedTempDir tempDir("preset-version");
            auto path = tempDir.getChildFile("v2.devpiano.preset");
            path.replaceWithText(R"({ "version": 2, "name": "future" })");
            expect(!loadPreset(path).has_value(), "version mismatch must be rejected");
        });

        testCase("display name strips the preset extension", [&] {
            expectEquals(getPresetDisplayNameForFile(juce::File("/tmp/My Song.devpiano.preset")),
                         juce::String("My Song"));
        });

        testCase("makeDefaultPreset has the built-in identity", [&] {
            const auto preset = makeDefaultPreset();
            expectEquals(preset.name, juce::String("Default"));
            expectEquals(preset.layout.id, juce::String("default.preset.builtin"));
            expect(preset.channelMatrix.active, "default matrix must be active");
            expectEquals(static_cast<int>(preset.colourMode), static_cast<int>(devpiano::ui::KeyColourMode::classic));
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
        testCase("reserved path characters become underscores", [&] {
            expectEquals(sanitisePresetFileName(R"(a/b\c:d*e?f"g<h>i|j)"), juce::String("a_b_c_d_e_f_g_h_i_j"));
        });

        testCase("alphanumerics, spaces, hyphens and underscores survive",
                 [&] { expectEquals(sanitisePresetFileName("My Song - 01_2"), juce::String("My Song - 01_2")); });

        testCase("whitespace-only name is trimmed to the fallback",
                 [&] { expectEquals(sanitisePresetFileName("   "), juce::String("untitled")); });

        testCase("empty name falls back to untitled",
                 [&] { expectEquals(sanitisePresetFileName(""), juce::String("untitled")); });

        testCase("trailing spaces are trimmed",
                 [&] { expectEquals(sanitisePresetFileName("Name  "), juce::String("Name")); });

        testCase("non-ASCII letters fall back to underscores", [&] {
            // isLetterOrDigit 为 ASCII 语义；Unicode 中文字符按文件名安全策略替换为下划线
            expectEquals(sanitisePresetFileName(juce::String::fromUTF8("演奏 01")), juce::String("__ 01"));
        });
    }
};

static PresetFileNameSanitiseTest presetFileNameSanitiseTest;

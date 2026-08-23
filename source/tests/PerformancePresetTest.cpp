#include <JuceHeader.h>

#include "Layout/PerformancePreset.h"

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

[[nodiscard]] juce::File makeScratchDir(const juce::String& tag) {
    auto dir
        = juce::File::getSpecialLocation(juce::File::tempDirectory)
              .getChildFile("devpiano-test-" + tag + "-" + juce::String(juce::Random::getSystemRandom().nextInt64()));
    dir.createDirectory();
    return dir;
}

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
    p.fadeSpeed = 0.42f;
    p.previewAlpha = 0.33f;
    for (int i = 0; i < 128; ++i) {
        p.customKeyLabels[static_cast<std::size_t>(i)] = "L" + juce::String(i);
        p.customKeyColours[static_cast<std::size_t>(i)]
            = juce::Colour(static_cast<uint8_t>(i), static_cast<uint8_t>(255 - i), static_cast<uint8_t>(100),
                           static_cast<uint8_t>(200));
    }
    return p;
}

void expectPresetsEqual(juce::UnitTest& ut, const PerformancePreset& a, const PerformancePreset& b) {
    ut.expectEquals(a.name, b.name);
    ut.expectEquals(a.layout.id, b.layout.id);
    ut.expectEquals(a.layout.name, b.layout.name);
    ut.expectEquals(static_cast<int>(a.layout.bindings.size()), static_cast<int>(b.layout.bindings.size()));
    for (std::size_t i = 0; i < a.layout.bindings.size(); ++i) {
        ut.expectEquals(a.layout.bindings[i].keyCode, b.layout.bindings[i].keyCode);
        ut.expectEquals(a.layout.bindings[i].displayText, b.layout.bindings[i].displayText);
        ut.expectEquals(a.layout.bindings[i].action.midiNote, b.layout.bindings[i].action.midiNote);
        ut.expectEquals(a.layout.bindings[i].action.midiChannel, b.layout.bindings[i].action.midiChannel);
    }
    ut.expect(a.channelMatrix.active == b.channelMatrix.active);
    for (int ch = 0; ch < 16; ++ch) {
        const auto& x = a.channelMatrix.channels[static_cast<std::size_t>(ch)];
        const auto& y = b.channelMatrix.channels[static_cast<std::size_t>(ch)];
        ut.expectEquals(static_cast<int>(x.outputChannel), static_cast<int>(y.outputChannel));
        ut.expectEquals(static_cast<int>(x.transpose), static_cast<int>(y.transpose));
        ut.expectEquals(static_cast<int>(x.octaveShift), static_cast<int>(y.octaveShift));
        ut.expectEquals(static_cast<int>(x.velocity), static_cast<int>(y.velocity));
        ut.expect(x.followKey == y.followKey);
    }
    ut.expectEquals(a.keySignature, b.keySignature);
    ut.expect(a.midiTranspose == b.midiTranspose);
    ut.expectEquals(static_cast<int>(a.colourMode), static_cast<int>(b.colourMode));
    ut.expectEquals(static_cast<int>(a.noteDisplay), static_cast<int>(b.noteDisplay));
    ut.expectWithinAbsoluteError(a.fadeSpeed, b.fadeSpeed, 0.0001f);
    // previewAlpha 有意不序列化（SettingsModel 无对应字段），读回保持默认 0。
    ut.expectWithinAbsoluteError(b.previewAlpha, 0.0f, 0.0001f);
    for (int i = 0; i < 128; ++i) {
        ut.expectEquals(a.customKeyLabels[static_cast<std::size_t>(i)], b.customKeyLabels[static_cast<std::size_t>(i)]);
        ut.expectEquals(static_cast<int>(a.customKeyColours[static_cast<std::size_t>(i)].getARGB()),
                        static_cast<int>(b.customKeyColours[static_cast<std::size_t>(i)].getARGB()));
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
            auto dir = makeScratchDir("preset-roundtrip");
            auto path = dir.getChildFile("test.devpiano.preset");

            const auto original = makeFullPreset();
            expect(savePreset(original, path), "save must succeed");

            auto loaded = loadPreset(path);
            expect(loaded.has_value(), "load must succeed");
            if (loaded.has_value()) {
                expectPresetsEqual(*this, original, *loaded);
            }
        });

        testCase("savePreset appends the missing extension", [&] {
            auto dir = makeScratchDir("preset-ext");
            auto path = dir.getChildFile("bare-name"); // 无扩展名

            const auto original = makeFullPreset();
            expect(savePreset(original, path), "save must succeed");
            expect(path.withFileExtension("devpiano.preset").existsAsFile(), "file must get the preset extension");
        });

        testCase("loadPreset rejects a missing file", [&] {
            auto dir = makeScratchDir("preset-missing");
            expect(!loadPreset(dir.getChildFile("nope.devpiano.preset")).has_value());
        });

        testCase("loadPreset rejects an empty file", [&] {
            auto dir = makeScratchDir("preset-empty");
            auto path = dir.getChildFile("empty.devpiano.preset");
            path.replaceWithText("");
            expect(!loadPreset(path).has_value());
        });

        testCase("loadPreset rejects invalid JSON", [&] {
            auto dir = makeScratchDir("preset-badjson");
            auto path = dir.getChildFile("bad.devpiano.preset");
            path.replaceWithText("{ this is not json !!");
            expect(!loadPreset(path).has_value());
        });

        testCase("loadPreset rejects a non-object root", [&] {
            auto dir = makeScratchDir("preset-array");
            auto path = dir.getChildFile("arr.devpiano.preset");
            path.replaceWithText("[1, 2, 3]");
            expect(!loadPreset(path).has_value());
        });

        testCase("loadPreset rejects an unknown format version", [&] {
            auto dir = makeScratchDir("preset-version");
            auto path = dir.getChildFile("v2.devpiano.preset");
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

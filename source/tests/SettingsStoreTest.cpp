#include <JuceHeader.h>

#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"

// =============================================================================
// Tests for SettingsStore persistence (AUDIT TEST-004):
//   - save → load round-trip through a temporary PropertiesFile
//   - corrupted zero-state performance values fall back to defaults
//   - scheduleSave merge semantics (debounce: nothing written before the
//     timer fires, only the latest payload is saved)
//
// The storage location is injected via PropertiesFile::Options::folderName
// pointing at a scratch directory, so real user settings are never touched.
// SettingsDebounceTimer::timerCallback() is public, so the debounce sequence
// is driven directly without a message loop.
// =============================================================================

namespace {

[[nodiscard]] juce::File makeScratchDir(const juce::String& tag) {
    auto dir
        = juce::File::getSpecialLocation(juce::File::tempDirectory)
              .getChildFile("devpiano-test-" + tag + "-" + juce::String(juce::Random::getSystemRandom().nextInt64()));
    dir.createDirectory();
    return dir;
}

// PropertiesFile::Options routed into a scratch directory.  getChildFile()
// returns an absolute path verbatim (juce_File.cpp:436), so folderName as an
// absolute path works on both Linux and Windows.
[[nodiscard]] juce::PropertiesFile::Options makeTestOptions(const juce::File& dir) {
    juce::PropertiesFile::Options opts;
    opts.applicationName = "DevPianoTests";
    opts.folderName = dir.getFullPathName();
    opts.filenameSuffix = ".settings";
    opts.commonToAllUsers = false;
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    return opts;
}

[[nodiscard]] juce::File settingsFileFor(const juce::File& dir) {
    return dir.getChildFile("DevPianoTests.settings");
}

SettingsModel makePopulatedModel() {
    SettingsModel m;
    m.masterGain = 0.55f;
    m.adsrAttack = 0.02f;
    m.adsrDecay = 0.30f;
    m.adsrSustain = 0.70f;
    m.adsrRelease = 0.40f;
    m.builtinTone = SettingsModel::BuiltinTone::piano;
    m.pianoBrightness = 0.70f;
    m.pianoHammerHardness = 0.35f;
    m.pianoResonance = 0.90f;
    m.keySignature = 5;
    m.midiTranspose = true;
    m.channelMatrix.active = true;
    m.channelMatrix.channels[0].outputChannel = 3;
    m.channelMatrix.channels[2].velocity = 90;
    m.keyboardDisplay.customKeyLabels[5] = "hi";
    m.keyboardDisplay.customKeyLabels[100] = "C";
    m.keyboardDisplay.customKeyColours[10] = juce::Colour(0xff00ff00);
    m.languageCode = "zh-CN";
    m.lastMidiExportPath = "/tmp/last-export.mid";
    m.lastMidiImportPath = "/tmp/last-import.mid";
    m.keyboardDisplay.resizableWindow = false;
    m.keyboardDisplay.showInstrumentFilter = false;
    m.pluginPanelExpanded = true;
    m.knownPluginListState = juce::parseXML(R"(<KNOWNPLUGINS><PLUGIN name="X" file="x.vst3"/></KNOWNPLUGINS>)");
    return m;
}

} // namespace

// -----------------------------------------------------------------------------

class SettingsStoreRoundTripTest final : public juce::UnitTest {
public:
    SettingsStoreRoundTripTest()
        : juce::UnitTest("SettingsStore: save/load round-trip", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("save then load restores every persisted field", [&] {
            auto dir = makeScratchDir("store-roundtrip");
            const auto options = makeTestOptions(dir);

            const auto original = makePopulatedModel();
            {
                SettingsStore store(options);
                store.save(original);
            }
            expect(settingsFileFor(dir).existsAsFile(), "settings file must be written");

            SettingsModel loaded; // 默认值模型
            {
                SettingsStore store(options);
                store.load(loaded);
            }

            expectWithinAbsoluteError(loaded.masterGain, 0.55f, 0.0001f);
            expectWithinAbsoluteError(loaded.adsrAttack, 0.02f, 0.0001f);
            expectWithinAbsoluteError(loaded.adsrDecay, 0.30f, 0.0001f);
            expectWithinAbsoluteError(loaded.adsrSustain, 0.70f, 0.0001f);
            expectWithinAbsoluteError(loaded.adsrRelease, 0.40f, 0.0001f);
            expectEquals(static_cast<int>(loaded.builtinTone), static_cast<int>(SettingsModel::BuiltinTone::piano),
                         "builtin tone must round-trip");
            expectWithinAbsoluteError(loaded.pianoBrightness, 0.70f, 0.0001f);
            expectWithinAbsoluteError(loaded.pianoHammerHardness, 0.35f, 0.0001f);
            expectWithinAbsoluteError(loaded.pianoResonance, 0.90f, 0.0001f);
            expectEquals(loaded.keySignature, 5);
            expect(loaded.midiTranspose, "midiTranspose must round-trip");
            expect(loaded.channelMatrix.active);
            expectEquals(static_cast<int>(loaded.channelMatrix.channels[0].outputChannel), 3);
            expectEquals(static_cast<int>(loaded.channelMatrix.channels[2].velocity), 90);
            expectEquals(loaded.keyboardDisplay.customKeyLabels[5], juce::String("hi"));
            expectEquals(loaded.keyboardDisplay.customKeyLabels[100], juce::String("C"));
            expect(loaded.keyboardDisplay.customKeyColours[10] == juce::Colour(0xff00ff00), "colour must round-trip");
            expectEquals(loaded.languageCode, juce::String("zh-CN"));
            expectEquals(loaded.lastMidiExportPath, juce::String("/tmp/last-export.mid"));
            expectEquals(loaded.lastMidiImportPath, juce::String("/tmp/last-import.mid"));
            expect(!loaded.keyboardDisplay.resizableWindow);
            expect(!loaded.keyboardDisplay.showInstrumentFilter);
            expect(loaded.pluginPanelExpanded);
            expect(loaded.knownPluginListState != nullptr, "known-plugin XML must round-trip");
            if (loaded.knownPluginListState != nullptr) {
                expect(loaded.knownPluginListState->toString().contains("KNOWNPLUGINS"));
            }
        });

        testCase("load keeps model defaults for fields never written", [&] {
            auto dir = makeScratchDir("store-defaults");
            const auto options = makeTestOptions(dir);

            {
                SettingsStore store(options);
                SettingsModel m; // 全默认，但 masterGain 显式非零以跳过零态恢复
                m.masterGain = 0.8f;
                store.save(m);
            }

            SettingsModel loaded;
            loaded.masterGain = 0.1f; // 现值应被文件值覆盖
            {
                SettingsStore store(options);
                store.load(loaded);
            }
            expectWithinAbsoluteError(loaded.masterGain, 0.8f, 0.0001f);
            expect(!loaded.keyboardDisplay.resizableWindow, "resizableWindow default must be false");
            expect(loaded.keyboardDisplay.showInstrumentFilter, "showInstrumentFilter default must be true");
        });

        testCase("legacy file without piano keys falls back to defaults", [&] {
            auto dir = makeScratchDir("store-legacy");
            const auto options = makeTestOptions(dir);

            {
                // 手工构造“旧版本”文件：只写 ADSR/gain，不含 builtinTone /
                // pianoBrightness / pianoHammerHardness / pianoResonance。
                juce::PropertiesFile legacy(options);
                legacy.setValue("masterGain", 0.6);
                legacy.setValue("adsrAttack", 0.02);
                legacy.setValue("adsrDecay", 0.2);
                legacy.setValue("adsrSustain", 0.8);
                legacy.setValue("adsrRelease", 0.3);
                legacy.saveIfNeeded();
            }

            SettingsModel loaded;
            {
                SettingsStore store(options);
                store.load(loaded);
            }
            expectWithinAbsoluteError(loaded.masterGain, 0.6f, 0.0001f, "legacy gain must still load");
            expectWithinAbsoluteError(loaded.pianoBrightness, 0.5f, 0.0001f,
                                      "missing piano brightness must fall back to default");
            expectWithinAbsoluteError(loaded.pianoHammerHardness, 0.5f, 0.0001f);
            expectWithinAbsoluteError(loaded.pianoResonance, 0.5f, 0.0001f);
            expectEquals(static_cast<int>(loaded.builtinTone), static_cast<int>(SettingsModel::BuiltinTone::piano),
                         "missing tone must fall back to default piano");
        });

        testCase("corrupted zero-state performance falls back to defaults", [&] {
            auto dir = makeScratchDir("store-zerostate");
            const auto options = makeTestOptions(dir);

            {
                SettingsStore store(options);
                SettingsModel m;
                m.masterGain = 0.0f;
                m.adsrAttack = 0.0f;
                m.adsrDecay = 0.0f;
                m.adsrSustain = 0.0f;
                m.adsrRelease = 0.0f;
                store.save(m);
            }

            SettingsModel loaded;
            {
                SettingsStore store(options);
                store.load(loaded);
            }
            expectWithinAbsoluteError(loaded.masterGain, 0.8f, 0.0001f, "zero gain must be treated as corrupt");
            expectWithinAbsoluteError(loaded.adsrAttack, 0.01f, 0.0001f);
            expectWithinAbsoluteError(loaded.adsrSustain, 0.80f, 0.0001f);
        });
    }
};

static SettingsStoreRoundTripTest settingsStoreRoundTripTest;

// -----------------------------------------------------------------------------

class SettingsStoreDebounceTest final : public juce::UnitTest {
public:
    SettingsStoreDebounceTest()
        : juce::UnitTest("SettingsStore: scheduleSave merge semantics", "DevPiano/Core") {
    }

    void runTest() override {
        testCase("scheduleSave writes nothing before the timer fires", [&] {
            auto dir = makeScratchDir("store-debounce-1");
            const auto options = makeTestOptions(dir);

            SettingsStore store(options);
            SettingsModel m;
            m.masterGain = 0.5f;
            store.scheduleSave(m, 300);

            expect(!settingsFileFor(dir).existsAsFile(), "nothing may hit disk before the debounce timer fires");
        });

        testCase("debounce timer saves the latest payload only", [&] {
            auto dir = makeScratchDir("store-debounce-2");
            const auto options = makeTestOptions(dir);

            SettingsStore store(options);
            SettingsDebounceTimer timer(store);

            SettingsModel m1;
            m1.masterGain = 0.25f;
            SettingsModel m2;
            m2.masterGain = 0.75f;

            timer.setPayload(m1);
            timer.start(300);
            timer.setPayload(m2); // 合并：第二次调用覆盖 payload
            timer.start(300);

            expect(!settingsFileFor(dir).existsAsFile(), "still nothing before the timer fires");

            timer.timerCallback(); // 手动触发（无消息循环）

            expect(settingsFileFor(dir).existsAsFile(), "firing the timer must persist");

            SettingsModel loaded;
            {
                SettingsStore reader(options);
                reader.load(loaded);
            }
            expectWithinAbsoluteError(loaded.masterGain, 0.75f, 0.0001f, "only the latest payload may reach disk");
        });

        testCase("timer without a payload writes nothing", [&] {
            auto dir = makeScratchDir("store-debounce-3");
            const auto options = makeTestOptions(dir);

            SettingsStore store(options);
            SettingsDebounceTimer timer(store);
            timer.start(300);
            timer.timerCallback();
            expect(!settingsFileFor(dir).existsAsFile(), "no payload must mean no save");
        });
    }
};

static SettingsStoreDebounceTest settingsStoreDebounceTest;

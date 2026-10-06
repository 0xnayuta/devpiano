#include <JuceHeader.h>

#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "TestHelpers.h"

// =============================================================================
// Tests for SettingsStore persistence (AUDIT TEST-004):
//   - save → load round-trip through a temporary PropertiesFile
//   - corrupted zero-state performance values fall back to defaults
//   - scheduleSave merge semantics (debounce: nothing written before the
//     timer fires, only the latest payload is saved)
//
// Storage is fully isolated within a RAII ScopedTempDir under /tmp,
// ensuring user application data is never touched or polluted.
// SettingsDebounceTimer::timerCallback() is public, so the debounce sequence
// is driven directly without a message loop.
// =============================================================================

namespace {

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
    m.keyboardDisplay.showInstrumentFilter = false;
    m.pluginPanelExpanded = true;
    m.knownPluginListState = juce::parseXML(R"(<KNOWNPLUGINS><PLUGIN name="X" file="x.vst3"/></KNOWNPLUGINS>)");
    m.metronomeEnabled = true;
    m.metronomeBpm = 173.0;
    m.metronomeTimeSignature = devpiano::core::TimeSignature::sixEight;
    m.metronomeVolume = 0.85f;
    m.metronomeCountIn = devpiano::core::CountInBars::twoBars;
    m.cadenceDynamicsEnabled = false;
    m.velocityHumanizeAmount = 0.08f;
    m.baseVelocityBias = 0.12f;
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
            devpiano::test::ScopedTempDir tempDir("store-roundtrip");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");

            const auto original = makePopulatedModel();
            {
                SettingsStore store(settingsFile);
                store.save(original);
            }
            expect(settingsFile.existsAsFile(), "settings file must be written");

            SettingsModel loaded; // 默认值模型
            {
                SettingsStore store(settingsFile);
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
            expect(!loaded.keyboardDisplay.showInstrumentFilter);
            expect(loaded.pluginPanelExpanded);
            expect(loaded.knownPluginListState != nullptr, "known-plugin XML must round-trip");
            if (loaded.knownPluginListState != nullptr) {
                expect(loaded.knownPluginListState->toString().contains("KNOWNPLUGINS"));
            }
            expect(loaded.metronomeEnabled, "metronomeEnabled must round-trip");
            expectWithinAbsoluteError(loaded.metronomeBpm, 173.0, 0.0001, "metronomeBpm must round-trip");
            expectEquals(static_cast<int>(loaded.metronomeTimeSignature),
                         static_cast<int>(devpiano::core::TimeSignature::sixEight),
                         "metronomeTimeSignature must round-trip");
            expectWithinAbsoluteError(loaded.metronomeVolume, 0.85f, 0.0001f, "metronomeVolume must round-trip");
            expectEquals(static_cast<int>(loaded.metronomeCountIn),
                         static_cast<int>(devpiano::core::CountInBars::twoBars), "metronomeCountIn must round-trip");
            expect(!loaded.cadenceDynamicsEnabled, "cadenceDynamicsEnabled must round-trip");
            expectWithinAbsoluteError(loaded.velocityHumanizeAmount, 0.08f, 0.0001f,
                                      "velocityHumanizeAmount must round-trip");
            expectWithinAbsoluteError(loaded.baseVelocityBias, 0.12f, 0.0001f, "baseVelocityBias must round-trip");
        });

        testCase("load keeps model defaults for fields never written", [&] {
            devpiano::test::ScopedTempDir tempDir("store-defaults");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");

            {
                SettingsStore store(settingsFile);
                SettingsModel m; // 全默认，但 masterGain 显式非零以跳过零态恢复
                m.masterGain = 0.8f;
                store.save(m);
            }

            SettingsModel loaded;
            loaded.masterGain = 0.1f; // 现值应被文件值覆盖
            {
                SettingsStore store(settingsFile);
                store.load(loaded);
            }
            expectWithinAbsoluteError(loaded.masterGain, 0.8f, 0.0001f);
            expect(loaded.keyboardDisplay.showInstrumentFilter, "showInstrumentFilter default must be true");
        });

        testCase("corrupted all-zero performance values fall back to defaults (BUG-008)", [&] {
            devpiano::test::ScopedTempDir tempDir("store-zerostate");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");

            // 构造损坏的零态 XML 并写入属性文件
            {
                juce::PropertiesFile::Options opts;
                opts.storageFormat = juce::PropertiesFile::storeAsXML;
                juce::PropertiesFile f(settingsFile, opts);
                f.setValue("masterGain", 0.0);
                f.setValue("adsrAttack", 0.0);
                f.setValue("adsrDecay", 0.0);
                f.setValue("adsrSustain", 0.0);
                f.setValue("adsrRelease", 0.0);
                f.saveIfNeeded();
            }

            SettingsModel loaded;
            // 预设非默认值，验证 load 会触发默认值恢复
            loaded.masterGain = 0.99f;
            loaded.adsrAttack = 0.99f;
            {
                SettingsStore store(settingsFile);
                store.load(loaded);
            }

            // 零态被拦截，恢复为 makeDefaultPerformanceSettings()
            const auto expectedDefaults = SettingsModel::PerformanceSettingsView {};
            expectWithinAbsoluteError(loaded.masterGain, expectedDefaults.masterGain, 0.0001f,
                                      "zero masterGain must fall back to default");
            expectWithinAbsoluteError(loaded.adsrAttack, expectedDefaults.adsrAttack, 0.0001f,
                                      "zero attack must fall back to default");
            expectWithinAbsoluteError(loaded.adsrDecay, expectedDefaults.adsrDecay, 0.0001f,
                                      "zero decay must fall back to default");
            expectWithinAbsoluteError(loaded.adsrSustain, expectedDefaults.adsrSustain, 0.0001f,
                                      "zero sustain must fall back to default");
            expectWithinAbsoluteError(loaded.adsrRelease, expectedDefaults.adsrRelease, 0.0001f,
                                      "zero release must fall back to default");
        });

        testCase("save creates valid non-empty file on disk", [&] {
            devpiano::test::ScopedTempDir tempDir("store-perms");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");

            SettingsModel m;
            m.masterGain = 0.7f;
            {
                SettingsStore store(settingsFile);
                store.save(m);
            }

            expect(settingsFile.existsAsFile(), "settings file must exist as file");
            expect(settingsFile.getSize() > 0, "settings file must not be empty");
#if JUCE_POSIX
            struct stat st {};
            if (::stat(settingsFile.getFullPathName().toRawUTF8(), &st) == 0) {
                expect((st.st_mode & S_IRUSR) != 0, "file must be readable by owner");
            }
#endif
        });
        testCase("legacy plugin recovery migrates only an unambiguous cached description", [&] {
            devpiano::test::ScopedTempDir tempDir("plugin-identity-migration");
            juce::PluginDescription first;
            first.name = "Twin";
            first.pluginFormatName = "VST3";
            first.fileOrIdentifier = tempDir.getChildFile("first.vst3").getFullPathName();
            first.uniqueId = 101;
            first.isInstrument = true;
            auto second = first;
            second.fileOrIdentifier = tempDir.getChildFile("second.vst3").getFullPathName();
            second.uniqueId = 202;
            second.isInstrument = false;

            for (int scenario = 0; scenario < 4; ++scenario) {
                const auto file = tempDir.getChildFile("case-" + juce::String(scenario) + ".settings");
                juce::KnownPluginList plugins;
                plugins.addType(first);
                if (scenario >= 1) {
                    plugins.addType(second);
                }
                const auto xml = plugins.createXml();
                juce::PropertiesFile::Options options;
                options.storageFormat = juce::PropertiesFile::storeAsXML;
                {
                    juce::PropertiesFile legacy(file, options);
                    legacy.setValue("knownPluginListXml", xml->toString());
                    legacy.setValue("lastPluginName", scenario == 3 ? "Missing" : "Twin");
                    if (scenario == 2) {
                        legacy.setValue("lastPluginIdentifier", second.createIdentifierString());
                    }
                    expect(legacy.saveIfNeeded());
                }

                SettingsStore store(file);
                SettingsModel restored;
                store.load(restored);
                juce::String expected;
                if (scenario == 0) {
                    expected = first.createIdentifierString();
                } else if (scenario == 2) {
                    expected = second.createIdentifierString();
                }
                expectEquals(restored.lastPluginIdentifier, expected);
                expect(store.save(restored));
                {
                    juce::PropertiesFile modern(file, options);
                    expect(!modern.containsKey("lastPluginName"), "legacy name must not remain a recovery path");
                    expectEquals(modern.getValue("lastPluginIdentifier"), expected);
                }
            }
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
            devpiano::test::ScopedTempDir tempDir("store-debounce-1");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");

            SettingsStore store(settingsFile);
            SettingsModel m;
            m.masterGain = 0.5f;
            store.scheduleSave(m, 300);

            expect(!settingsFile.existsAsFile(), "nothing may hit disk before the debounce timer fires");
        });

        testCase("debounce timer saves the latest payload only", [&] {
            devpiano::test::ScopedTempDir tempDir("store-debounce-2");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");

            SettingsStore store(settingsFile);
            SettingsDebounceTimer timer(store);

            SettingsModel m1;
            m1.masterGain = 0.25f;
            SettingsModel m2;
            m2.masterGain = 0.75f;

            timer.setPayload(m1);
            timer.setPayload(m2); // 合并：第二次调用覆盖 payload
            expect(!settingsFile.existsAsFile(), "still nothing before the timer fires");

            timer.timerCallback(); // 手动触发（无消息循环）

            expect(settingsFile.existsAsFile(), "firing the timer must persist");

            SettingsModel loaded;
            {
                SettingsStore reader(settingsFile);
                reader.load(loaded);
            }
            expectWithinAbsoluteError(loaded.masterGain, 0.75f, 0.0001f, "only the latest payload may reach disk");
        });

        testCase("timer without a payload writes nothing", [&] {
            devpiano::test::ScopedTempDir tempDir("store-debounce-3");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");

            SettingsStore store(settingsFile);
            SettingsDebounceTimer timer(store);
            timer.timerCallback();
            expect(!settingsFile.existsAsFile(), "no payload must mean no save");
        });
        testCase("debounce timer retains payload by value when source is destroyed", [&] {
            devpiano::test::ScopedTempDir tempDir("store-debounce-4");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");

            SettingsStore store(settingsFile);
            SettingsDebounceTimer timer(store);

            {
                SettingsModel tempModel;
                tempModel.masterGain = 0.62f;
                timer.setPayload(tempModel);
            } // tempModel destroyed here

            timer.timerCallback(); // must safely persist without dangling

            expect(settingsFile.existsAsFile());
            SettingsModel loaded;
            {
                SettingsStore reader(settingsFile);
                reader.load(loaded);
            }
            expectWithinAbsoluteError(loaded.masterGain, 0.62f, 0.0001f);
        });

        testCase("a synchronous save supersedes an older scheduled snapshot", [&] {
            devpiano::test::ScopedTempDir tempDir("store-debounce-supersede");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");
            SettingsStore store(settingsFile);
            SettingsModel oldModel;
            oldModel.masterGain = 0.20f;
            oldModel.lastPluginIdentifier = "old-plugin-identifier";
            oldModel.knownPluginListState = juce::parseXML(R"(<KNOWNPLUGINS><PLUGIN name="Old"/></KNOWNPLUGINS>)");
            store.scheduleSave(oldModel, 1);

            SettingsModel newModel;
            newModel.masterGain = 0.85f;
            newModel.lastPluginIdentifier = "new-plugin-identifier";
            newModel.knownPluginListState = juce::parseXML(R"(<KNOWNPLUGINS><PLUGIN name="New"/></KNOWNPLUGINS>)");
            expect(store.save(newModel));
            devpiano::test::drainMessages(100);

            SettingsModel loaded;
            SettingsStore(settingsFile).load(loaded);
            expectWithinAbsoluteError(loaded.masterGain, 0.85f, 0.0001f);
            expectEquals(loaded.lastPluginIdentifier, juce::String("new-plugin-identifier"));
            expect(loaded.knownPluginListState != nullptr);
            if (loaded.knownPluginListState != nullptr) {
                const auto* plugin = loaded.knownPluginListState->getChildByName("PLUGIN");
                expect(plugin != nullptr);
                if (plugin != nullptr) {
                    expectEquals(plugin->getStringAttribute("name"), juce::String("New"));
                }
            }
        });

        testCase("later schedules still commit and saves on another store cannot cancel them", [&] {
            devpiano::test::ScopedTempDir tempDir("store-debounce-independent");
            const auto fileA = tempDir.getChildFile("StoreA.settings");
            const auto fileB = tempDir.getChildFile("StoreB.settings");
            SettingsStore storeA(fileA);
            SettingsStore storeB(fileB);
            SettingsModel modelA;
            modelA.masterGain = 0.15f;
            storeA.scheduleSave(modelA, 1);
            modelA.masterGain = 0.60f;
            expect(storeA.save(modelA));

            modelA.masterGain = 0.33f;
            storeA.scheduleSave(modelA, 1);
            SettingsModel modelB;
            modelB.masterGain = 0.77f;
            expect(storeB.save(modelB));
            devpiano::test::drainMessages(100);

            SettingsModel finalA;
            SettingsModel finalB;
            SettingsStore(fileA).load(finalA);
            SettingsStore(fileB).load(finalB);
            expectWithinAbsoluteError(finalA.masterGain, 0.33f, 0.0001f);
            expectWithinAbsoluteError(finalB.masterGain, 0.77f, 0.0001f);
        });

        testCase("a failed synchronous save preserves the older scheduled write", [&] {
            devpiano::test::ScopedTempDir tempDir("store-debounce-failure");
            const auto settingsFile = tempDir.getChildFile("DirTarget.settings");
            expect(settingsFile.createDirectory().wasOk());
            SettingsStore store(settingsFile);
            SettingsModel pendingModel;
            pendingModel.masterGain = 0.42f;
            store.scheduleSave(pendingModel, 1);
            SettingsModel syncModel;
            syncModel.masterGain = 0.99f;
            expect(!store.save(syncModel));
            expect(settingsFile.deleteFile());
            devpiano::test::drainMessages(100);

            SettingsModel loaded;
            SettingsStore(settingsFile).load(loaded);
            expectWithinAbsoluteError(loaded.masterGain, 0.42f, 0.0001f);
        });

        testCase("first and replacement snapshots preserve practice fields and independent XML", [&] {
            devpiano::test::ScopedTempDir tempDir("store-debounce-phase35");
            const auto settingsFile = tempDir.getChildFile("DevPianoTests.settings");
            auto model = makePopulatedModel();
            model.audioDeviceState = juce::parseXML(R"(<DEVICES name="DeviceA"/>)");
            SettingsModel copyConstructed(model);
            SettingsModel copyAssigned;
            copyAssigned = copyConstructed;
            model.audioDeviceState->setAttribute("name", "ChangedOriginal");
            copyConstructed.audioDeviceState->setAttribute("name", "ChangedCopy");

            const auto checkPracticeFields = [this](const SettingsModel& loaded, double bpm) {
                expect(loaded.metronomeEnabled);
                expectWithinAbsoluteError(loaded.metronomeBpm, bpm, 0.0001);
                expect(loaded.metronomeTimeSignature == devpiano::core::TimeSignature::sixEight);
                expectWithinAbsoluteError(loaded.metronomeVolume, 0.85f, 0.0001f);
                expect(loaded.metronomeCountIn == devpiano::core::CountInBars::twoBars);
                expect(!loaded.cadenceDynamicsEnabled);
                expectWithinAbsoluteError(loaded.velocityHumanizeAmount, 0.08f, 0.0001f);
                expectWithinAbsoluteError(loaded.baseVelocityBias, 0.12f, 0.0001f);
            };
            SettingsStore store(settingsFile);
            store.scheduleSave(copyAssigned, 1);
            copyAssigned.metronomeBpm = 91.0;
            copyAssigned.audioDeviceState->setAttribute("name", "ChangedAfterSchedule");
            copyAssigned.knownPluginListState->setAttribute("changed", true);
            devpiano::test::drainMessages(100);

            SettingsModel first;
            SettingsStore(settingsFile).load(first);
            checkPracticeFields(first, 173.0);
            expect(first.audioDeviceState != nullptr && first.knownPluginListState != nullptr);
            if (first.audioDeviceState != nullptr && first.knownPluginListState != nullptr) {
                expectEquals(first.audioDeviceState->getStringAttribute("name"), juce::String("DeviceA"));
                expect(!first.knownPluginListState->hasAttribute("changed"));
            }

            copyAssigned.metronomeBpm = 201.0;
            store.scheduleSave(copyAssigned, 1000);
            copyAssigned.metronomeBpm = 149.0;
            store.scheduleSave(copyAssigned, 1);
            copyAssigned.metronomeBpm = 91.0;
            devpiano::test::drainMessages(100);
            SettingsModel second;
            SettingsStore(settingsFile).load(second);
            checkPracticeFields(second, 149.0);
        });
    }
};

static SettingsStoreDebounceTest settingsStoreDebounceTest;

#include <JuceHeader.h>

#include "Plugin/PluginFlowSupport.h"
#include "Plugin/PluginHost.h"
#include "Settings/SettingsModel.h"
#include "TestHelpers.h"
class PluginOperationControllerTest final : public juce::UnitTest {
public:
    PluginOperationControllerTest()
        : juce::UnitTest("PluginOperationController: decisions and restore plans", "DevPiano/Plugin") {
    }

    void runTest() override {
        testPluginRecoverySettingsHelpers();
        testPluginScanPathUsability();
        testStartupPluginRestorePlan();
        testCachedPluginListRestoration();
    }

private:
    void testPluginRecoverySettingsHelpers() {
        beginTest("Plugin recovery settings construction and fallback");

        devpiano::test::ScopedTempDir customDir("custom-vst3");
        devpiano::test::ScopedTempDir defaultDir("default-vst3");
        const auto customPathStr = customDir.get().getFullPathName();
        const auto defaultPathStr = defaultDir.get().getFullPathName();

        const auto recovery = devpiano::plugin::makePluginRecoverySettings(customPathStr, "GrandPiano");
        expectEquals(recovery.pluginSearchPath, customPathStr);
        expectEquals(recovery.lastPluginName, juce::String("GrandPiano"));

        // Path fallback: preserve non-empty search path
        juce::FileSearchPath defaultPath(defaultPathStr);
        const auto withExisting = devpiano::plugin::withPluginRecoveryPathFallback(recovery, defaultPath);
        expectEquals(withExisting.pluginSearchPath, customPathStr);

        // Path fallback: fill empty search path with default
        const auto emptyRecovery = devpiano::plugin::makePluginRecoverySettings("", "GrandPiano");
        const auto filled = devpiano::plugin::withPluginRecoveryPathFallback(emptyRecovery, defaultPath);
        expectEquals(filled.pluginSearchPath, defaultPath.toString());
        expectEquals(filled.lastPluginName, juce::String("GrandPiano"));
    }

    void testPluginScanPathUsability() {
        beginTest("Plugin scan path normalisation and usability");

        juce::FileSearchPath emptyPath;
        expect(!devpiano::plugin::isUsablePluginScanPath(emptyPath), "Empty path must not be usable");

        devpiano::test::ScopedTempDir validDir("valid-vst3");
        juce::FileSearchPath validPath(validDir.get().getFullPathName());
        expect(devpiano::plugin::isUsablePluginScanPath(validPath), "Valid path must be usable");

        // Normalise path fallback
        const auto normalisedWithFallback = devpiano::plugin::normalisePluginScanPath(emptyPath, validPath);
        expectEquals(normalisedWithFallback.toString(), validPath.toString());

        const auto normalisedBothEmpty = devpiano::plugin::normalisePluginScanPath(emptyPath, emptyPath);
        expect(normalisedBothEmpty.getNumPaths() == 0);
    }

    void testStartupPluginRestorePlan() {
        beginTest("StartupPluginRestorePlan decision logic");

        devpiano::test::ScopedTempDir defaultDir("default-vst3");
        devpiano::test::ScopedTempDir customDir("custom-vst3");
        juce::FileSearchPath defaultPath(defaultDir.get().getFullPathName());
        const auto customPathStr = customDir.get().getFullPathName();

        // 1. Empty settings, valid default path -> plan should scan default path
        SettingsModel::PluginRecoverySettingsView emptySettings;
        const auto plan1 = devpiano::plugin::buildStartupPluginRestorePlan(emptySettings, defaultPath);
        expect(plan1.shouldScan, "Should scan when default search path is available");
        expect(!plan1.shouldLoadLastPlugin, "Should not load last plugin when name is empty");
        expectEquals(plan1.recovery.pluginSearchPath, defaultPath.toString());

        // 2. Settings with last plugin name -> should load last plugin
        SettingsModel::PluginRecoverySettingsView namedSettings;
        namedSettings.pluginSearchPath = customPathStr;
        namedSettings.lastPluginName = "Synth1";
        const auto plan2 = devpiano::plugin::buildStartupPluginRestorePlan(namedSettings, defaultPath);
        expect(plan2.shouldScan);
        expect(plan2.shouldLoadLastPlugin);
        expectEquals(plan2.recovery.pluginSearchPath, customPathStr);
        expectEquals(plan2.recovery.lastPluginName, juce::String("Synth1"));

        // 3. Completely empty settings and empty default path -> no scan, no load
        juce::FileSearchPath noDefault;
        const auto plan3 = devpiano::plugin::buildStartupPluginRestorePlan(emptySettings, noDefault);
        expect(!plan3.shouldScan, "Should not scan when no search path is available");
        expect(!plan3.shouldLoadLastPlugin);
    }

    void testCachedPluginListRestoration() {
        beginTest("Cached known plugin list restoration");

        PluginHost host;
        SettingsModel settings;

        devpiano::plugin::StartupPluginRestorePlan plan;
        plan.shouldScan = true;
        plan.shouldLoadLastPlugin = false;

        // 1. Without cached XML -> tryRestoreCachedPluginList returns false
        expect(!devpiano::plugin::tryRestoreCachedPluginList(host, settings, plan),
               "Must return false when no XML is cached in settings");

        // 2. With valid XML -> tryRestoreCachedPluginList populates host and returns true
        auto xml = std::make_unique<juce::XmlElement>("KNOWNPLUGINS");
        auto* pluginXml = new juce::XmlElement("PLUGIN");
        pluginXml->setAttribute("name", "TestCachedSynth");
        pluginXml->setAttribute("desc", "TestCachedSynth");
        pluginXml->setAttribute("category", "Synth");
        pluginXml->setAttribute("file", "/path/to/test.vst3");
        pluginXml->setAttribute("uid", "12345");
        pluginXml->setAttribute("isInstrument", "1");
        xml->addChildElement(pluginXml);
        settings.knownPluginListState = std::move(xml);

        const bool restored = devpiano::plugin::tryRestoreCachedPluginList(host, settings, plan);
        expect(restored, "Must return true when valid known plugin list XML is present");
        expect(host.getKnownPluginNames().contains("TestCachedSynth"),
               "Known plugins in host must contain the restored plugin name");
    }
};

static PluginOperationControllerTest pluginOperationControllerTest;

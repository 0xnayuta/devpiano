#include <JuceHeader.h>

#include "Plugin/PluginHost.h"
#include "UI/PluginPanelStateBuilder.h"

// =============================================================================
// Tests for the plugin-persistence XML round-trip and the panel-state builder
// (AUDIT TEST-006):
//   - createKnownPluginListXml → restoreKnownPluginListFromXml round-trip
//   - restore accepts a hand-built KnownPluginList XML and rehydrates names
//   - restore rejects garbage without crashing
//   - buildPluginPanelState maps host state into the panel view model
// =============================================================================

namespace {

// 构造一个含单个插件的 KnownPluginList XML（模拟已扫描的缓存）。
std::unique_ptr<juce::XmlElement> makeSinglePluginListXml() {
    auto root = std::make_unique<juce::XmlElement>("KNOWNPLUGINS");
    auto* plugin = new juce::XmlElement("PLUGIN");
    plugin->setAttribute("name", "Test Synth");
    plugin->setAttribute("desc", "Test Synth");
    plugin->setAttribute("category", "Synth");
    plugin->setAttribute("manufacturer", "Test");
    plugin->setAttribute("version", "1.0.0");
    plugin->setAttribute("file", "/tmp/test.vst3");
    plugin->setAttribute("uid", "12345");
    plugin->setAttribute("isInstrument", "1");
    plugin->setAttribute("numInputChannels", "2");
    plugin->setAttribute("numOutputChannels", "2");
    root->addChildElement(plugin);
    return root;
}

} // namespace

class PluginXmlRoundTripTest final : public juce::UnitTest {
public:
    PluginXmlRoundTripTest()
        : juce::UnitTest("PluginHost: known-list XML round-trip", "DevPiano/Engine") {
    }

    void runTest() override {
        testCase("fresh host serialises an empty list and restore reports it", [&] {
            PluginHost host;
            auto xml = host.createKnownPluginListXml();
            expect(xml != nullptr, "empty list must still serialise to XML");
            if (xml == nullptr) {
                return;
            }

            expect(xml->hasTagName("KNOWNPLUGINS"), "root element must be KNOWNPLUGINS");

            PluginHost restored;
            expect(!restored.restoreKnownPluginListFromXml(*xml), "empty list restore must report zero plugins");
            expect(restored.getKnownPluginNames().isEmpty());
        });

        testCase("hand-built plugin XML rehydrates the plugin names", [&] {
            auto xml = makeSinglePluginListXml();

            PluginHost host;
            expect(host.restoreKnownPluginListFromXml(*xml), "restore must succeed with one plugin");
            expectEquals(host.getLastScanPluginCount(), 1);

            const auto names = host.getKnownPluginNames();
            expectEquals(names.size(), 1);
            if (names.size() == 1) {
                expectEquals(names[0], juce::String("Test Synth"));
            }

            const auto instruments = host.getInstrumentPluginNames();
            expectEquals(instruments.size(), 1, "isInstrument=1 must land in the instrument list");

            // 序列化回来应保持同一插件
            auto recreated = host.createKnownPluginListXml();
            expect(recreated != nullptr);
            if (recreated != nullptr) {
                expectEquals(recreated->getNumChildElements(), 1);
            }
        });

        testCase("restore round-trip is idempotent", [&] {
            auto xml = makeSinglePluginListXml();

            PluginHost host;
            expect(host.restoreKnownPluginListFromXml(*xml));
            auto recreated = host.createKnownPluginListXml();
            expect(recreated != nullptr);
            if (recreated == nullptr) {
                return;
            }

            PluginHost second;
            expect(second.restoreKnownPluginListFromXml(*recreated), "re-serialised XML must restore again");
            expectEquals(second.getKnownPluginNames().size(), 1);
        });

        testCase("garbage XML is rejected without crashing", [&] {
            juce::XmlElement garbage("NOT_A_PLUGIN_LIST");
            garbage.setAttribute("foo", "bar");

            PluginHost host;
            juce::ignoreUnused(host.restoreKnownPluginListFromXml(garbage));
            expect(host.getKnownPluginNames().isEmpty(), "garbage must not produce plugin names");
        });
    }
};

static PluginXmlRoundTripTest pluginXmlRoundTripTest;

// -----------------------------------------------------------------------------

class PluginPanelStateBuilderTest final : public juce::UnitTest {
public:
    PluginPanelStateBuilderTest()
        : juce::UnitTest("PluginPanelStateBuilder: state mapping", "DevPiano/Engine") {
    }

    void runTest() override {
        testCase("fresh host maps to default panel state", [&] {
            PluginHost host;
            const auto state = buildPluginPanelState(host, {}, false);

            expect(!state.hasLoadedPlugin);
            expect(!state.isPrepared);
            expect(state.supportsVst3, "VST3 is compiled in (JUCE_PLUGINHOST_VST3=1)");
            expect(state.availablePluginNames.isEmpty());
            expect(state.currentPluginName.isEmpty());
            expect(state.lastPluginName.isEmpty());
            expect(state.preferredSelection.isEmpty(), "empty preferred selection without a last plugin");
            expect(!state.isEditorOpen);
            expect(!state.isCurrentlyScanning);
            expectEquals(state.scanPluginCount, 0);
            expectEquals(state.scanFailedCount, 0);
        });

        testCase("last plugin name becomes the preferred selection", [&] {
            PluginHost host;
            const auto state = buildPluginPanelState(host, "My Favourite Synth", false);
            expectEquals(state.preferredSelection, juce::String("My Favourite Synth"));
            expectEquals(state.lastPluginName, juce::String("My Favourite Synth"));
        });

        testCase("isEditorOpen flows into the panel state", [&] {
            PluginHost host;
            const auto state = buildPluginPanelState(host, {}, true);
            expect(state.isEditorOpen, "editor-open flag must map through");
        });

        testCase("restored plugin list populates the name lists", [&] {
            auto xml = makeSinglePluginListXml();
            PluginHost host;
            expect(host.restoreKnownPluginListFromXml(*xml));

            const auto state = buildPluginPanelState(host, {}, false);
            expectEquals(state.availablePluginNames.size(), 1);
            expectEquals(state.instrumentPluginNames.size(), 1);
            expectEquals(state.effectPluginNames.size(), 0);
            expectEquals(state.scanPluginCount, 1);
        });
    }
};

static PluginPanelStateBuilderTest pluginPanelStateBuilderTest;

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
        testCase("same-name cached plugins retain independent choices and classification", [&] {
            juce::KnownPluginList plugins;
            juce::PluginDescription instrument;
            instrument.name = "Twin";
            instrument.pluginFormatName = "VST3";
            instrument.fileOrIdentifier = "/plugins/first.vst3";
            instrument.uniqueId = 101;
            instrument.isInstrument = true;
            auto otherInstrument = instrument;
            otherInstrument.fileOrIdentifier = "/plugins/second.vst3";
            otherInstrument.uniqueId = 202;
            auto effect = instrument;
            effect.fileOrIdentifier = "/plugins/effect.vst3";
            effect.uniqueId = 303;
            effect.isInstrument = false;
            plugins.addType(instrument);
            plugins.addType(otherInstrument);
            plugins.addType(effect);

            PluginHost restored;
            const auto xml = plugins.createXml();
            expect(restored.restoreKnownPluginListFromXml(*xml));
            const auto state = buildPluginPanelState(restored, otherInstrument.createIdentifierString(), false);
            expectEquals(state.availablePlugins.size(), 3, "same display name must not collapse selectable identities");
            juce::StringArray identifiers;
            for (const auto& choice : state.availablePlugins) {
                expect(!identifiers.contains(choice.identifier), "each selectable identity must be distinct");
                identifiers.add(choice.identifier);
                if (choice.identifier == effect.createIdentifierString()) {
                    expect(!choice.isInstrument, "effect classification must survive cache restoration");
                } else {
                    expect(choice.isInstrument, "instrument classification must survive cache restoration");
                }
            }
            expect(identifiers.contains(state.preferredSelection), "restored selection must identify an actual choice");
            expectEquals(state.preferredSelection, otherInstrument.createIdentifierString());
        });
    }
};

static PluginXmlRoundTripTest pluginXmlRoundTripTest;

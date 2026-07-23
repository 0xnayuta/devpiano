#include <JuceHeader.h>

#include "Plugin/PluginHost.h"

// =============================================================================
// Tests for PluginHost: default state, readonly queries, error messages,
// scanning state, format/plugin-list queries.
//
// The core plugin lifecycle (loadPlugin, scanVst3Plugins, prepareToPlay)
// requires real VST3 files on disk and is NOT tested here — that path is
// covered by integration / manual testing.
//
// What IS tested:
//   - Default constructed state (hasLoadedPlugin, isPrepared, scanning state)
//   - Error message defaults and immutability without load
//   - Empty plugin lists on fresh construction
//   - Scan summary defaults
//   - supportsVst3() reflects the compile-time config
//   - getAvailableFormatsDescription() is non-empty
//   - getDefaultVst3SearchPath() does not crash
//   - createKnownPluginListXml() produces valid empty XML
// =============================================================================

// =============================================================================

class DefaultStateTest : public juce::UnitTest {
public:
    DefaultStateTest() : juce::UnitTest("PluginHost: default state") {}

    void runTest() override
    {
        beginTest("fresh PluginHost has no loaded plugin");
        {
            PluginHost host;
            expect(!host.hasLoadedPlugin(), "should not have loaded plugin");
            expect(!host.isPrepared(), "should not be prepared");
        }

        beginTest("getCurrentPluginName returns empty for fresh host");
        {
            PluginHost host;
            expect(host.getCurrentPluginName().isEmpty(), "plugin name should be empty");
        }

        beginTest("getInstance returns nullptr for fresh host");
        {
            PluginHost host;
            expect(host.getInstance() == nullptr, "instance should be null");
        }

        beginTest("getLoadedPluginDescription returns nullptr for fresh host");
        {
            PluginHost host;
            expect(host.getLoadedPluginDescription() == nullptr, "description should be null");
        }

        beginTest("getPreparedSampleRate / getPreparedBlockSize defaults");
        {
            PluginHost host;
            expectEquals(host.getPreparedSampleRate(), 44100.0);
            expectEquals(host.getPreparedBlockSize(), 512);
        }
    }
};

static DefaultStateTest defaultStateTest;

// =============================================================================

class LastLoadErrorTest : public juce::UnitTest {
public:
    LastLoadErrorTest() : juce::UnitTest("PluginHost: last load error") {}

    void runTest() override
    {
        beginTest("default load error message is set");
        {
            PluginHost host;
            expect(host.getLastLoadError().isNotEmpty(),
                   "should have a default error message");
        }
    }
};

static LastLoadErrorTest lastLoadErrorTest;

// =============================================================================

class PluginListEmptyTest : public juce::UnitTest {
public:
    PluginListEmptyTest() : juce::UnitTest("PluginHost: empty plugin lists") {}

    void runTest() override
    {
        beginTest("getKnownPluginNames is empty for fresh host");
        {
            PluginHost host;
            expect(host.getKnownPluginNames().isEmpty(),
                   "known plugin names should be empty");
        }

        beginTest("getInstrumentPluginNames is empty for fresh host");
        {
            PluginHost host;
            expect(host.getInstrumentPluginNames().isEmpty(),
                   "instrument names should be empty");
        }

        beginTest("getEffectPluginNames is empty for fresh host");
        {
            PluginHost host;
            expect(host.getEffectPluginNames().isEmpty(),
                   "effect names should be empty");
        }

        beginTest("getPluginListDescription is non-empty (describes empty state)");
        {
            PluginHost host;
            expect(host.getPluginListDescription().isNotEmpty(),
                   "list description should describe state even when empty");
        }
    }
};

static PluginListEmptyTest pluginListEmptyTest;

// =============================================================================

class ScanSummaryTest : public juce::UnitTest {
public:
    ScanSummaryTest() : juce::UnitTest("PluginHost: scan summary defaults") {}

    void runTest() override
    {
        beginTest("getLastScanSummary returns default message");
        {
            PluginHost host;
            expect(host.getLastScanSummary().isNotEmpty(),
                   "should have a default scan summary message");
        }

        beginTest("getLastScanPluginCount returns 0 by default");
        {
            PluginHost host;
            expectEquals(host.getLastScanPluginCount(), 0);
        }

        beginTest("getLastScanFailedCount returns 0 by default");
        {
            PluginHost host;
            expectEquals(host.getLastScanFailedCount(), 0);
        }
    }
};

static ScanSummaryTest scanSummaryTest;

// =============================================================================

class ScanningStateTest : public juce::UnitTest {
public:
    ScanningStateTest() : juce::UnitTest("PluginHost: scanning state") {}

    void runTest() override
    {
        beginTest("isCurrentlyScanning is false by default");
        {
            PluginHost host;
            expect(!host.isCurrentlyScanning(), "should not be scanning");
        }

        beginTest("getScanningPluginName is empty by default");
        {
            PluginHost host;
            expect(host.getScanningPluginName().isEmpty(),
                   "scanning name should be empty");
        }

        beginTest("getLastScanFailedFiles is empty by default");
        {
            PluginHost host;
            expect(host.getLastScanFailedFiles().isEmpty(),
                   "failed files list should be empty");
        }
    }
};

static ScanningStateTest scanningStateTest;

// =============================================================================

class SupportsVst3Test : public juce::UnitTest {
public:
    SupportsVst3Test() : juce::UnitTest("PluginHost: supports VST3") {}

    void runTest() override
    {
        beginTest("supportsVst3 reflects compile-time config");
        {
            PluginHost host;
            // JUCE_PLUGINHOST_VST3=1 is set in CMake → VST3 support enabled.
            expect(host.supportsVst3(),
                   "VST3 should be supported when JUCE_PLUGINHOST_VST3=1");
        }
    }
};

static SupportsVst3Test supportsVst3Test;

// =============================================================================

class FormatsAndPathsTest : public juce::UnitTest {
public:
    FormatsAndPathsTest() : juce::UnitTest("PluginHost: formats and paths") {}

    void runTest() override
    {
        beginTest("getAvailableFormatsDescription is non-empty");
        {
            PluginHost host;
            expect(host.getAvailableFormatsDescription().isNotEmpty(),
                   "should describe available formats");
        }

        beginTest("getDefaultVst3SearchPath does not crash");
        {
            PluginHost host;
            // On Linux/WSL this may return an empty or default path.
            // Just verify it doesn't crash.
            auto path = host.getDefaultVst3SearchPath();
            juce::ignoreUnused(path);
            expect(true);
        }
    }
};

static FormatsAndPathsTest formatsAndPathsTest;

// =============================================================================

class KnownPluginListXmlTest : public juce::UnitTest {
public:
    KnownPluginListXmlTest() : juce::UnitTest("PluginHost: known plugin list XML") {}

    void runTest() override
    {
        beginTest("createKnownPluginListXml returns valid XML for empty list");
        {
            PluginHost host;
            auto xml = host.createKnownPluginListXml();
            expect(xml != nullptr, "XML should not be null for empty list");
        }

        beginTest("restoreKnownPluginListFromXml with null is safe");
        {
            PluginHost host;
            juce::XmlElement elem("dummy");
            bool result = host.restoreKnownPluginListFromXml(elem);
            juce::ignoreUnused(result);
            expect(true); // survived
        }
    }
};

static KnownPluginListXmlTest knownPluginListXmlTest;

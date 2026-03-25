#pragma once

#include <JuceHeader.h>

class PluginHost
{
public:
    PluginHost();
    ~PluginHost() = default;

    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }
    juce::String getAvailableFormatsDescription() const;
    bool supportsVst3() const;

    juce::FileSearchPath getDefaultVst3SearchPath() const;
    int scanVst3Plugins(const juce::FileSearchPath& searchPath, bool recursive);
    juce::StringArray getKnownPluginNames() const;
    juce::String getPluginListDescription() const;
    juce::String getLastScanSummary() const;

private:
    juce::AudioPluginFormat* getVst3Format() const;
    juce::File getDeadMansPedalFile() const;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::String lastScanSummary { "VST3 scan not run yet." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};

#pragma once

#include <JuceHeader.h>

class PluginHost
{
public:
    PluginHost();
    ~PluginHost();

    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }
    juce::String getAvailableFormatsDescription() const;
    bool supportsVst3() const;

    juce::FileSearchPath getDefaultVst3SearchPath() const;
    int scanVst3Plugins(const juce::FileSearchPath& searchPath, bool recursive);
    juce::StringArray getKnownPluginNames() const;
    juce::String getPluginListDescription() const;
    juce::String getLastScanSummary() const;

    bool loadPluginByName(const juce::String& pluginName,
                          double initialSampleRate = 44100.0,
                          int initialBufferSize = 512);
    bool loadPluginByDescription(const juce::PluginDescription& description,
                                 double initialSampleRate = 44100.0,
                                 int initialBufferSize = 512);
    bool prepareToPlay(double sampleRate, int blockSize);
    void releaseResources();
    void unloadPlugin();

    [[nodiscard]] bool hasLoadedPlugin() const noexcept;
    [[nodiscard]] bool isPrepared() const noexcept;
    [[nodiscard]] juce::AudioPluginInstance* getInstance() const noexcept;
    [[nodiscard]] juce::String getCurrentPluginName() const;
    [[nodiscard]] juce::String getLastLoadError() const;
    [[nodiscard]] double getPreparedSampleRate() const noexcept;
    [[nodiscard]] int getPreparedBlockSize() const noexcept;

private:
    juce::AudioPluginFormat* getVst3Format() const;
    juce::File getDeadMansPedalFile() const;
    bool configureDefaultBuses(juce::AudioPluginInstance& instance);

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::String lastScanSummary { "VST3 scan not run yet." };
    juce::String lastLoadError { "No plugin load attempted yet." };
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<juce::PluginDescription> loadedPluginDescription;
    double preparedSampleRate = 44100.0;
    int preparedBlockSize = 512;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};

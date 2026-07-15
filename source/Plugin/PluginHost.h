#pragma once

#include <JuceHeader.h>

class PluginHost {
public:
    PluginHost();
    ~PluginHost();

    juce::AudioPluginFormatManager& getFormatManager() noexcept {
        return formatManager;
    }
    juce::String getAvailableFormatsDescription() const;
    bool supportsVst3() const;

    juce::FileSearchPath getDefaultVst3SearchPath() const;
    int scanVst3Plugins(const juce::FileSearchPath& searchPath, bool recursive);

    bool beginVst3ScanSession(const juce::FileSearchPath& searchPath, bool recursive);
    bool advanceVst3ScanStep();
    void cancelVst3ScanSession();

    juce::StringArray getKnownPluginNames() const;
    juce::String getPluginListDescription() const;
    juce::String getLastScanSummary() const;
    juce::StringArray getLastScanFailedFiles() const;
    bool isCurrentlyScanning() const noexcept {
        return isScanning;
    }

    // Scans a single .vst3 file and adds it to the known plugin list (without clearing existing entries).
    // Returns the real plugin names extracted from the file metadata.
    juce::StringArray addVst3FileToKnownList(const juce::File& vst3File);
    juce::String getScanningPluginName() const noexcept {
        return scanningPluginName;
    }
    std::unique_ptr<juce::XmlElement> createKnownPluginListXml() const;
    bool restoreKnownPluginListFromXml(const juce::XmlElement& xml);
    void markPluginScanSkipped(juce::String reason);

    bool loadPluginByName(const juce::String& pluginName, double initialSampleRate = 44100.0,
                          int initialBufferSize = 512);
    bool loadPluginByDescription(const juce::PluginDescription& description, double initialSampleRate = 44100.0,
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
    [[nodiscard]] static bool configureDefaultBuses(juce::AudioPluginInstance& instance);
    [[nodiscard]] const juce::PluginDescription* getLoadedPluginDescription() const noexcept;

private:
    juce::AudioPluginFormat* getVst3Format() const;
    juce::File getDeadMansPedalFile() const;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::String lastScanSummary { "VST3 scan not run yet." };
    juce::StringArray lastScanFailedFiles;
    juce::String lastLoadError { "No plugin load attempted yet." };
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<juce::PluginDescription> loadedPluginDescription;
    double preparedSampleRate = 44100.0;
    int preparedBlockSize = 512;
    bool prepared = false;
    bool isScanning = false;
    juce::String scanningPluginName;
    juce::FileSearchPath activeScanPath;
    bool activeScanRecursive = false;
    std::unique_ptr<juce::PluginDirectoryScanner> activeScanner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};

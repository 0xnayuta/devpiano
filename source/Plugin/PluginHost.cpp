#include "PluginHost.h"

#include "Diagnostics/DebugLog.h"

PluginHost::PluginHost()
{
    juce::addDefaultFormatsToManager(formatManager);
}

PluginHost::~PluginHost()
{
    unloadPlugin();
}

juce::String PluginHost::getAvailableFormatsDescription() const
{
    juce::StringArray names;

    for (auto index = 0; index < formatManager.getNumFormats(); ++index)
        if (auto* format = formatManager.getFormat(index))
            names.add(format->getName());

    if (names.isEmpty())
        return "Plugin formats: none";

    return "Plugin formats: " + names.joinIntoString(", ");
}

bool PluginHost::supportsVst3() const
{
    return getVst3Format() != nullptr;
}

juce::FileSearchPath PluginHost::getDefaultVst3SearchPath() const
{
    if (auto* format = getVst3Format())
        return format->getDefaultLocationsToSearch();

    return {};
}

int PluginHost::scanVst3Plugins(const juce::FileSearchPath& searchPath, bool recursive)
{
    lastScanFailedFiles.clear();

    auto* format = getVst3Format();
    if (format == nullptr)
    {
        lastScanSummary = "VST3 format unavailable.";
        return 0;
    }

    if (! format->canScanForPlugins())
    {
        lastScanSummary = "Current VST3 format cannot scan for plugins.";
        return 0;
    }

    unloadPlugin();
    knownPluginList.clear();

    juce::PluginDirectoryScanner scanner(knownPluginList,
                                         *format,
                                         searchPath,
                                         recursive,
                                         getDeadMansPedalFile(),
                                         false);

    juce::String pluginBeingScanned;
    while (scanner.scanNextFile(true, pluginBeingScanned))
    {
    }

    lastScanFailedFiles = scanner.getFailedFiles();

    for (const auto& failedFile : lastScanFailedFiles)
        DP_LOG_WARN(("[PluginScan] Failed file: " + failedFile).toRawUTF8());

    const auto failedCount = lastScanFailedFiles.size();
    const auto pluginCount = knownPluginList.getNumTypes();

    if (pluginCount > 0)
    {
        lastScanSummary = "VST3 scan complete: " + juce::String(pluginCount)
                        + " plugin(s), " + juce::String(failedCount) + " failed"
                        + (failedCount > 0 ? " (see log)." : ".");
        DP_LOG_INFO(("VST3 scan complete: " + juce::String(pluginCount)
                     + " plugin(s), " + juce::String(failedCount) + " failed").toRawUTF8());
    }
    else if (failedCount > 0)
    {
        lastScanSummary = "VST3 scan found no plugins; " + juce::String(failedCount)
                        + " failed (see log).";
        DP_LOG_WARN(("VST3 scan found no plugins: " + juce::String(failedCount)
                     + " failed files").toRawUTF8());
    }
    else
    {
        lastScanSummary = "VST3 scan complete: no plugins found.";
        DP_LOG_INFO("VST3 scan complete: no plugins found.");
    }

    return knownPluginList.getNumTypes();
}

juce::StringArray PluginHost::getKnownPluginNames() const
{
    juce::StringArray names;

    for (const auto& description : knownPluginList.getTypes())
        names.add(description.name);

    names.removeDuplicates(false);
    names.sort(true);
    return names;
}

juce::String PluginHost::getPluginListDescription() const
{
    const auto names = getKnownPluginNames();
    if (names.isEmpty())
    {
        if (lastScanSummary == "VST3 scan not run yet.")
            return "No plugins scanned yet.";

        return "No plugins available. " + lastScanSummary;
    }

    return names.joinIntoString("\n");
}

juce::String PluginHost::getLastScanSummary() const
{
    return lastScanSummary;
}

juce::StringArray PluginHost::getLastScanFailedFiles() const
{
    return lastScanFailedFiles;
}

std::unique_ptr<juce::XmlElement> PluginHost::createKnownPluginListXml() const
{
    return knownPluginList.createXml();
}

bool PluginHost::restoreKnownPluginListFromXml(const juce::XmlElement& xml)
{
    knownPluginList.recreateFromXml(xml);
    lastScanFailedFiles.clear();

    const auto count = knownPluginList.getNumTypes();
    if (count <= 0)
    {
        lastScanSummary = "Cached plugin list was empty.";
        return false;
    }

    lastScanSummary = "Loaded cached plugin list: " + juce::String(count) + " plugin(s).";
    return true;
}

void PluginHost::markPluginScanSkipped(juce::String reason)
{
    knownPluginList.clear();
    lastScanFailedFiles.clear();
    lastScanSummary = reason.trim().isNotEmpty() ? std::move(reason)
                                                  : juce::String("VST3 scan skipped.");
}

bool PluginHost::loadPluginByName(const juce::String& pluginName,
                                  double initialSampleRate,
                                  int initialBufferSize)
{
    const auto trimmedName = pluginName.trim();
    for (const auto& description : knownPluginList.getTypes())
        if (description.name.equalsIgnoreCase(trimmedName))
            return loadPluginByDescription(description, initialSampleRate, initialBufferSize);

    lastLoadError = "Plugin not found in known list: " + trimmedName;
    return false;
}

bool PluginHost::loadPluginByDescription(const juce::PluginDescription& description,
                                         double initialSampleRate,
                                         int initialBufferSize)
{
    unloadPlugin();

    juce::String errorMessage;
    pluginInstance = formatManager.createPluginInstance(description,
                                                        initialSampleRate,
                                                        initialBufferSize,
                                                        errorMessage);

    if (pluginInstance == nullptr)
    {
        lastLoadError = errorMessage.isNotEmpty() ? errorMessage
                                                  : "Unknown plugin load failure.";
        loadedPluginDescription.reset();
        DP_LOG_ERROR(("[PluginHost] Plugin load failed: " + lastLoadError).toRawUTF8());
        return false;
    }

    loadedPluginDescription = std::make_unique<juce::PluginDescription>(description);
    DP_LOG_INFO(("[PluginHost] Plugin loaded: " + description.name).toRawUTF8());

    if (! prepareToPlay(initialSampleRate, initialBufferSize))
    {
        unloadPlugin();
        return false;
    }

    lastLoadError = {};
    return true;
}

bool PluginHost::prepareToPlay(double sampleRate, int blockSize)
{
    preparedSampleRate = sampleRate > 0.0 ? sampleRate : preparedSampleRate;
    preparedBlockSize = blockSize > 0 ? blockSize : preparedBlockSize;

    if (pluginInstance == nullptr)
    {
        prepared = false;
        lastLoadError = "No plugin instance available for prepareToPlay.";
        return false;
    }

    pluginInstance->suspendProcessing(true);
    releaseResources();

    if (! configureDefaultBuses(*pluginInstance))
    {
        prepared = false;
        lastLoadError = "Failed to configure plugin buses for playback.";
        return false;
    }

    pluginInstance->setRateAndBufferSizeDetails(preparedSampleRate, preparedBlockSize);
    pluginInstance->prepareToPlay(preparedSampleRate, preparedBlockSize);
    pluginInstance->reset();
    pluginInstance->suspendProcessing(false);
    prepared = true;

    DP_LOG_INFO(("[PluginHost] Plugin prepared: rate=" + juce::String(preparedSampleRate)
                  + "Hz, block=" + juce::String(preparedBlockSize)).toRawUTF8());

    return true;
}

void PluginHost::releaseResources()
{
    if (pluginInstance != nullptr)
        pluginInstance->suspendProcessing(true);

    if (pluginInstance != nullptr && prepared)
        pluginInstance->releaseResources();

    prepared = false;
}

void PluginHost::unloadPlugin()
{
    const auto hadPlugin = hasLoadedPlugin();
    const auto pluginName = getCurrentPluginName();

    releaseResources();
    pluginInstance.reset();
    loadedPluginDescription.reset();

    if (hadPlugin)
        DP_LOG_INFO(("[PluginHost] Plugin unloaded: " + pluginName).toRawUTF8());
}

bool PluginHost::hasLoadedPlugin() const noexcept
{
    return pluginInstance != nullptr;
}

bool PluginHost::isPrepared() const noexcept
{
    return prepared;
}

juce::AudioPluginInstance* PluginHost::getInstance() const noexcept
{
    return pluginInstance.get();
}

juce::String PluginHost::getCurrentPluginName() const
{
    if (loadedPluginDescription != nullptr)
        return loadedPluginDescription->name;

    return {};
}

juce::String PluginHost::getLastLoadError() const
{
    return lastLoadError;
}

double PluginHost::getPreparedSampleRate() const noexcept
{
    return preparedSampleRate;
}

int PluginHost::getPreparedBlockSize() const noexcept
{
    return preparedBlockSize;
}

juce::AudioPluginFormat* PluginHost::getVst3Format() const
{
    for (auto index = 0; index < formatManager.getNumFormats(); ++index)
        if (auto* format = formatManager.getFormat(index))
            if (format->getName().containsIgnoreCase("VST3"))
                return format;

    return nullptr;
}

juce::File PluginHost::getDeadMansPedalFile() const
{
    auto directory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("devpiano");
    directory.createDirectory();
    return directory.getChildFile("vst3-dead-mans-pedal.txt");
}

bool PluginHost::configureDefaultBuses(juce::AudioPluginInstance& instance)
{
    instance.enableAllBuses();

    auto layout = instance.getBusesLayout();
    if (layout.outputBuses.isEmpty())
        return true;

    layout.outputBuses.getReference(0) = juce::AudioChannelSet::stereo();

    if (! layout.inputBuses.isEmpty() && layout.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        layout.inputBuses.getReference(0) = juce::AudioChannelSet::stereo();

    return instance.setBusesLayout(layout);
}

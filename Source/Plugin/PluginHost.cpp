#include "PluginHost.h"

PluginHost::PluginHost()
{
    juce::addHeadlessDefaultFormatsToManager(formatManager);
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

    const auto failedCount = scanner.getFailedFiles().size();
    lastScanSummary = "VST3 scan complete: " + juce::String(knownPluginList.getNumTypes())
                    + " plugin(s), " + juce::String(failedCount) + " failed.";

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
        return "No plugins scanned.";

    return names.joinIntoString("\n");
}

juce::String PluginHost::getLastScanSummary() const
{
    return lastScanSummary;
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

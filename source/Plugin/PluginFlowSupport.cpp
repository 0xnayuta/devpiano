#include "PluginFlowSupport.h"

#include "Diagnostics/DebugLog.h"

#include "Plugin/PluginHost.h"

namespace devpiano::plugin
{
namespace
{
juce::FileSearchPath filterExistingDirectories(const juce::FileSearchPath& path)
{
    juce::FileSearchPath filtered;

    for (auto index = 0; index < path.getNumPaths(); ++index)
    {
        const auto directory = path[index];
        if (directory.isDirectory())
        {
            filtered.addIfNotAlreadyThere(directory);
            continue;
        }

        const auto rawPath = path.getRawString(index).trim();
        DP_LOG_WARN(("[PluginScan] Ignoring invalid scan directory: "
                             + (rawPath.isNotEmpty() ? rawPath : directory.getFullPathName())).toRawUTF8());
    }

    filtered.removeRedundantPaths();
    return filtered;
}
} // namespace

juce::FileSearchPath normalisePluginScanPath(const juce::FileSearchPath& requestedPath,
                                             const juce::FileSearchPath& defaultSearchPath);

void restorePluginsAtPath(PluginHost& pluginHost,
                          const juce::FileSearchPath& path,
                          const SettingsModel::PluginRecoverySettingsView& recovery,
                          const std::function<void(const SettingsModel::PluginRecoverySettingsView&)>& applySettingsCallback)
{
    pluginHost.scanVst3Plugins(path, true);
    applySettingsCallback(recovery);
}

bool tryRestoreCachedPluginList(PluginHost& pluginHost,
                                SettingsModel& settings,
                                const StartupPluginRestorePlan& plan)
{
    if (settings.knownPluginListState == nullptr)
        return false;

    if (! pluginHost.restoreKnownPluginListFromXml(*settings.knownPluginListState))
        return false;

    settings.applyPluginRecoverySettingsView(plan.recovery);
    return true;
}

void scanPluginsAtPathAndUpdateRecovery(PluginHost& pluginHost,
                                        SettingsModel& settings,
                                        const juce::FileSearchPath& path,
                                        const juce::String& lastPluginName)
{
    const auto recovery = makePluginRecoverySettings(path.toString(), lastPluginName);
    pluginHost.scanVst3Plugins(path, true);
    settings.applyPluginRecoverySettingsView(recovery);
    settings.knownPluginListState = pluginHost.createKnownPluginListXml();
}

SettingsModel::PluginRecoverySettingsView makePluginRecoverySettings(juce::String pluginSearchPath,
                                                                     juce::String lastPluginName)
{
    return { .pluginSearchPath = std::move(pluginSearchPath),
             .lastPluginName = std::move(lastPluginName) };
}

SettingsModel::PluginRecoverySettingsView withPluginRecoveryPathFallback(const SettingsModel::PluginRecoverySettingsView& recovery,
                                                                          const juce::FileSearchPath& defaultSearchPath)
{
    const auto pluginSearchPath = normalisePluginScanPath(juce::FileSearchPath(recovery.pluginSearchPath),
                                                          defaultSearchPath)
                                      .toString();

    return makePluginRecoverySettings(pluginSearchPath,
                                       recovery.lastPluginName);
}

juce::FileSearchPath normalisePluginScanPath(const juce::FileSearchPath& requestedPath,
                                             const juce::FileSearchPath& defaultSearchPath)
{
    const auto sourcePath = requestedPath.toString().trim().isNotEmpty() ? requestedPath
                                                                         : defaultSearchPath;

    return filterExistingDirectories(sourcePath);
}

bool isUsablePluginScanPath(const juce::FileSearchPath& path)
{
    return path.getNumPaths() > 0 && path.toString().trim().isNotEmpty();
}

StartupPluginRestorePlan buildStartupPluginRestorePlan(const SettingsModel::PluginRecoverySettingsView& persistedRecovery,
                                                       const juce::FileSearchPath& defaultSearchPath)
{
    const auto recovery = withPluginRecoveryPathFallback(persistedRecovery, defaultSearchPath);
    const auto scanPath = juce::FileSearchPath(recovery.pluginSearchPath);

    return { .recovery = recovery,
             .shouldScan = isUsablePluginScanPath(scanPath),
             .shouldLoadLastPlugin = recovery.lastPluginName.trim().isNotEmpty() };
}
} // namespace devpiano::plugin

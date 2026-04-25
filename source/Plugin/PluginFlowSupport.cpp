#include "PluginFlowSupport.h"

#include "Plugin/PluginHost.h"

namespace devpiano::plugin
{
void restorePluginsAtPath(PluginHost& pluginHost,
                          const juce::FileSearchPath& path,
                          const SettingsModel::PluginRecoverySettingsView& recovery,
                          const std::function<void(const SettingsModel::PluginRecoverySettingsView&)>& applySettingsCallback)
{
    pluginHost.scanVst3Plugins(path, true);
    applySettingsCallback(recovery);
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
    auto pluginSearchPath = recovery.pluginSearchPath;
    if (pluginSearchPath.trim().isEmpty())
        pluginSearchPath = defaultSearchPath.toString();

    return makePluginRecoverySettings(std::move(pluginSearchPath),
                                      recovery.lastPluginName);
}

bool isUsablePluginScanPath(const juce::FileSearchPath& path)
{
    return path.toString().trim().isNotEmpty();
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

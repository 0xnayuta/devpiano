#pragma once

#include <JuceHeader.h>

#include "Settings/SettingsModel.h"
#include "Plugin/PluginHost.h"

namespace devpiano::plugin
{
struct StartupPluginRestorePlan
{
    SettingsModel::PluginRecoverySettingsView recovery;
    bool shouldScan = false;
    bool shouldLoadLastPlugin = false;
};

// Scans VST3 plugins at the given path and applies the given recovery settings.
// This is an all-in-one helper that sequences: prepare → scan → apply.
// The caller provides the already-resolved pluginSearchPath (with fallback already applied).
void restorePluginsAtPath(PluginHost& pluginHost,
                          const juce::FileSearchPath& path,
                          const SettingsModel::PluginRecoverySettingsView& recovery,
                          const std::function<void(const SettingsModel::PluginRecoverySettingsView&)>& applySettingsCallback);

[[nodiscard]] SettingsModel::PluginRecoverySettingsView makePluginRecoverySettings(juce::String pluginSearchPath,
                                                                                   juce::String lastPluginName);

[[nodiscard]] SettingsModel::PluginRecoverySettingsView withPluginRecoveryPathFallback(const SettingsModel::PluginRecoverySettingsView& recovery,
                                                                                       const juce::FileSearchPath& defaultSearchPath);

[[nodiscard]] bool isUsablePluginScanPath(const juce::FileSearchPath& path);

[[nodiscard]] StartupPluginRestorePlan buildStartupPluginRestorePlan(const SettingsModel::PluginRecoverySettingsView& persistedRecovery,
                                                                    const juce::FileSearchPath& defaultSearchPath);
} // namespace devpiano::plugin

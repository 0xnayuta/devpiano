#include "PluginPanelStateBuilder.h"

PluginPanel::State buildPluginPanelState(const PluginHost& pluginHost,
                                         const juce::String& lastPluginName,
                                         bool isEditorOpen)
{
    const auto preferredSelection = pluginHost.hasLoadedPlugin() ? pluginHost.getCurrentPluginName()
                                                                 : lastPluginName;

    return { .availablePluginNames = pluginHost.getKnownPluginNames(),
             .preferredSelection = preferredSelection,
             .pluginListText = pluginHost.getPluginListDescription(),
             .availableFormatsDescription = pluginHost.getAvailableFormatsDescription(),
             .lastScanSummary = pluginHost.getLastScanSummary(),
             .currentPluginName = pluginHost.getCurrentPluginName(),
             .lastLoadError = pluginHost.getLastLoadError(),
             .lastPluginName = lastPluginName,
             .preparedSampleRate = pluginHost.getPreparedSampleRate(),
             .preparedBlockSize = pluginHost.getPreparedBlockSize(),
             .supportsVst3 = pluginHost.supportsVst3(),
             .hasLoadedPlugin = pluginHost.hasLoadedPlugin(),
             .isPrepared = pluginHost.isPrepared(),
             .isEditorOpen = isEditorOpen };
}

PluginPanel::State buildPluginPanelState(const devpiano::core::PluginState& pluginState)
{
    const auto preferredSelection = pluginState.hasLoadedPlugin ? pluginState.currentPluginName
                                                                : pluginState.lastPluginName;

    return { .availablePluginNames = pluginState.availablePluginNames,
             .preferredSelection = preferredSelection,
             .pluginListText = pluginState.pluginListText,
             .availableFormatsDescription = pluginState.availableFormatsDescription,
             .lastScanSummary = pluginState.lastScanSummary,
             .currentPluginName = pluginState.currentPluginName,
             .lastLoadError = pluginState.lastLoadError,
             .lastPluginName = pluginState.lastPluginName,
             .preparedSampleRate = pluginState.preparedSampleRate,
             .preparedBlockSize = pluginState.preparedBlockSize,
             .supportsVst3 = pluginState.supportsVst3,
             .hasLoadedPlugin = pluginState.hasLoadedPlugin,
             .isPrepared = pluginState.isPrepared,
             .isEditorOpen = pluginState.isEditorOpen };
}

PluginPanel::State buildPluginPanelState(const devpiano::core::AppState& appState)
{
    return buildPluginPanelState(appState.plugin);
}

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

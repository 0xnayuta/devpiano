#include "PluginPanelStateBuilder.h"
#include <algorithm>

devpiano::ui::PluginPanelState buildPluginPanelState(const PluginHost& pluginHost,
                                                     const juce::String& lastPluginIdentifier, bool isEditorOpen) {
    const auto descriptions = pluginHost.getKnownPluginDescriptions();
    juce::Array<devpiano::ui::PluginChoice> choices;
    juce::String lastPluginName;
    for (const auto& description : descriptions) {
        auto label = description.name;
        const auto duplicateNames = std::ranges::count_if(
            descriptions, [&description](const auto& other) { return other.name.equalsIgnoreCase(description.name); });
        const auto identifier = description.createIdentifierString();
        if (duplicateNames > 1) {
            label << " [" << identifier << "]";
        }
        choices.add({ identifier, label, description.isInstrument });
        if (description.matchesIdentifierString(lastPluginIdentifier)) {
            lastPluginName = description.name;
        }
    }
    std::ranges::sort(choices, [](const auto& lhs, const auto& rhs) {
        const auto compared = lhs.displayName.compareIgnoreCase(rhs.displayName);
        return compared != 0 ? compared < 0 : lhs.identifier < rhs.identifier;
    });
    const auto preferredSelection
        = pluginHost.hasLoadedPlugin() ? pluginHost.getCurrentPluginIdentifier() : lastPluginIdentifier;

    return { .availablePlugins = std::move(choices),
             .preferredSelection = preferredSelection,
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
             .isEditorOpen = isEditorOpen,
             .isCurrentlyScanning = pluginHost.isCurrentlyScanning(),
             .scanPluginCount = pluginHost.getLastScanPluginCount(),
             .scanFailedCount = pluginHost.getLastScanFailedCount(),
             .scanningPluginName = pluginHost.getScanningPluginName() };
}

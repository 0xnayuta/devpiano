#pragma once

#include <JuceHeader.h>

// ============================================================================
// Plugin panel view-model state.
//
// Previously PluginPanel::State; extracted so PluginPanelStateBuilder
// does not depend on the PluginPanel Component class (deleted in Phase 11d).
// ============================================================================

struct PluginPanelState {
    juce::StringArray availablePluginNames;
    juce::StringArray instrumentPluginNames;
    juce::StringArray effectPluginNames;
    juce::String preferredSelection;
    juce::String pluginListText;
    juce::String availableFormatsDescription;
    juce::String lastScanSummary;
    juce::String currentPluginName;
    juce::String lastLoadError;
    juce::String lastPluginName;
    double preparedSampleRate = 0.0;
    int preparedBlockSize = 0;
    bool supportsVst3 = false;
    bool hasLoadedPlugin = false;
    bool isPrepared = false;
    bool isEditorOpen = false;
    bool isCurrentlyScanning = false;
    int scanPluginCount = 0;
    int scanFailedCount = 0;
    juce::String scanningPluginName;
};

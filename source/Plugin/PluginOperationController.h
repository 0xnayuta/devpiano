#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

#include "Settings/SettingsModel.h"

class AudioEngine;
class MainComponent;
class PluginHost;
class PluginPanel;
class PluginEditorWindow;

namespace devpiano::plugin
{
struct StartupPluginRestorePlan;
}

namespace devpiano::plugin
{

class PluginOperationController final : private juce::AsyncUpdater
{
public:
    PluginOperationController(MainComponent& owner,
                              PluginHost& pluginHost,
                              SettingsModel& appSettings,
                              PluginPanel& pluginPanel);
    ~PluginOperationController();

    void restorePluginStateOnStartup();

    void loadSelectedPlugin();
    void unloadCurrentPlugin();
    void togglePluginEditor();
    void scanPlugins();
    void closePluginEditorWindow();

    [[nodiscard]] bool hasEditorWindowOpen() const noexcept;

private:
    void restorePluginScanPathOnStartup(const StartupPluginRestorePlan& plan);
    void restoreLastPluginOnStartup(const StartupPluginRestorePlan& plan);
    void restorePluginByNameOnStartup(const juce::String& pluginName);

    [[nodiscard]] juce::FileSearchPath resolvePluginScanPath() const;
    [[nodiscard]] juce::String getSelectedPluginNameForLoad() const;
    void loadPluginByNameAndCommitState(const juce::String& pluginName);
    void unloadPluginAndCommitState();

    [[nodiscard]] std::unique_ptr<juce::AudioProcessorEditor> tryCreatePluginEditor() const;
    void handlePluginEditorWindowClosedAsync();
    void openPluginEditorWindow(std::unique_ptr<juce::AudioProcessorEditor> editor);

    void scanPluginsAtPathAndApplyRecoveryState(const juce::FileSearchPath& path,
                                                const juce::String& lastPluginName);
    void scanPluginsAtPathAndCommitState(const juce::FileSearchPath& path);

    void commitPluginRecoveryStateAndFinishUi(const SettingsModel::PluginRecoverySettingsView& pluginRecovery,
                                              bool shouldSaveSettings);

    void handleAsyncUpdate() override;
    void finishScanSessionAndCommitState();

    MainComponent& owner;
    PluginHost& pluginHost;
    SettingsModel& appSettings;
    PluginPanel& pluginPanel;

    std::unique_ptr<PluginEditorWindow> pluginEditorWindow;

    juce::FileSearchPath pendingScanPath;
    juce::String pendingScanLastPluginName;
    bool scanStepInProgress = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginOperationController)
};

} // namespace devpiano::plugin

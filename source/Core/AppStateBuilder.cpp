#include "Core/AppStateBuilder.h"

#include "Audio/AudioDeviceDiagnostics.h"
#include "Input/KeyboardMidiMapper.h"
#include "Midi/MidiRouter.h"
#include "Plugin/PluginHost.h"

namespace devpiano::core
{
namespace
{
void assertMessageThreadSnapshotAccess()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
}
} // namespace

RuntimeAudioState buildRuntimeAudioStateSnapshot(const SettingsModel& settings,
                                                 const juce::AudioDeviceManager& deviceManager)
{
    assertMessageThreadSnapshotAccess();

    const auto diagnostics = devpiano::audio::buildAudioDeviceDiagnostics(settings.audioDeviceState.get(), deviceManager);

    return { .hasLiveDevice = diagnostics.live.hasLiveDevice,
             .sampleRate = diagnostics.live.sampleRate,
             .bufferSize = diagnostics.live.bufferSize,
             .backendName = diagnostics.live.backendName,
             .deviceName = diagnostics.live.deviceName,
             .availableBufferSizesText = devpiano::audio::formatBufferSizes(diagnostics.live.availableBufferSizes),
             .restoreOutcome = diagnostics.restoreOutcome,
             .mismatchReasons = diagnostics.mismatchReasons };
}

RuntimePluginState buildRuntimePluginStateSnapshot(const PluginHost& pluginHost, bool isEditorOpen)
{
    assertMessageThreadSnapshotAccess();

    return { .currentPluginName = pluginHost.getCurrentPluginName(),
             .availablePluginNames = pluginHost.getKnownPluginNames(),
             .lastScanSummary = pluginHost.getLastScanSummary(),
             .lastLoadError = pluginHost.getLastLoadError(),
             .preparedSampleRate = pluginHost.getPreparedSampleRate(),
             .preparedBlockSize = pluginHost.getPreparedBlockSize(),
             .supportsVst3 = pluginHost.supportsVst3(),
             .hasLoadedPlugin = pluginHost.hasLoadedPlugin(),
             .isPrepared = pluginHost.isPrepared(),
             .isEditorOpen = isEditorOpen };
}

RuntimeInputState buildRuntimeInputStateSnapshot(const KeyboardMidiMapper& keyboardMidiMapper,
                                                 const MidiRouter& midiRouter,
                                                 int midiActivityCount,
                                                 const juce::String& lastMidiMessage)
{
    assertMessageThreadSnapshotAccess();

    return { .keyboardLayout = keyboardMidiMapper.getLayout(),
             .openMidiInputCount = midiRouter.getOpenInputCount(),
             .midiActivityCount = midiActivityCount,
             .lastMidiMessage = lastMidiMessage };
}

AppState buildCurrentAppStateSnapshot(const SettingsModel& settings,
                                      const juce::AudioDeviceManager& deviceManager,
                                      const PluginHost& pluginHost,
                                      bool isEditorOpen,
                                      const KeyboardMidiMapper& keyboardMidiMapper,
                                      const MidiRouter& midiRouter,
                                      int midiActivityCount,
                                      const juce::String& lastMidiMessage)
{
    return buildAppState(settings,
                         buildRuntimeAudioStateSnapshot(settings, deviceManager),
                         buildRuntimePluginStateSnapshot(pluginHost, isEditorOpen),
                         buildRuntimeInputStateSnapshot(keyboardMidiMapper,
                                                        midiRouter,
                                                        midiActivityCount,
                                                        lastMidiMessage));
}

} // namespace devpiano::core

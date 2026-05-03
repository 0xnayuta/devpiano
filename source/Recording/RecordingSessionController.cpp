#include "RecordingSessionController.h"

#include "Audio/AudioEngine.h"
#include "Diagnostics/DebugLog.h"
#include "Export/ExportFlowSupport.h"
#include "MainComponent.h"
#include "Recording/MidiFileExporter.h"
#include "Recording/MidiFileImporter.h"
#include "Recording/PerformanceFile.h"
#include "Recording/RecordingFlowSupport.h"
#include "Recording/WavFileExporter.h"

namespace devpiano::recording
{
namespace
{
constexpr std::size_t defaultRecordingEventsPerSecond = 100;
constexpr std::size_t defaultRecordingCapacitySeconds = 30 * 60;

[[nodiscard]] RecordingFlowState toRecordingFlowState(ControlsPanel::RecordingState state) noexcept
{
    switch (state)
    {
        case ControlsPanel::RecordingState::idle: return RecordingFlowState::idle;
        case ControlsPanel::RecordingState::recording: return RecordingFlowState::recording;
        case ControlsPanel::RecordingState::playing: return RecordingFlowState::playing;
    }

    return RecordingFlowState::idle;
}

[[nodiscard]] ControlsPanel::RecordingState toControlsPanelRecordingState(RecordingFlowState state) noexcept
{
    switch (state)
    {
        case RecordingFlowState::idle: return ControlsPanel::RecordingState::idle;
        case RecordingFlowState::recording: return ControlsPanel::RecordingState::recording;
        case RecordingFlowState::playing: return ControlsPanel::RecordingState::playing;
    }

    return ControlsPanel::RecordingState::idle;
}

[[nodiscard]] RecordingFlowStatus makeRecordingFlowStatus(ControlsPanel::RecordingState state,
                                                          bool hasTake) noexcept
{
    return { .currentState = toRecordingFlowState(state),
             .hasTake = hasTake };
}

[[nodiscard]] juce::File getLastMidiExportDirectory(const SettingsModel& settings)
{
    if (settings.lastMidiExportPath.isNotEmpty())
    {
        const auto lastFile = juce::File(settings.lastMidiExportPath);
        if (lastFile.existsAsFile())
            return lastFile.getParentDirectory();

        if (lastFile.isDirectory())
            return lastFile;

        const auto parent = lastFile.getParentDirectory();
        if (parent.isDirectory())
            return parent;
    }

    return juce::File::getCurrentWorkingDirectory();
}

[[nodiscard]] juce::File getLastMidiImportDirectory(const SettingsModel& settings)
{
    if (settings.lastMidiImportPath.isNotEmpty())
    {
        const auto lastFile = juce::File(settings.lastMidiImportPath);
        if (lastFile.exists())
            return lastFile.getParentDirectory();
    }

    return juce::File {};
}

[[nodiscard]] juce::File makeDefaultMidiExportFile(const SettingsModel& settings,
                                                   devpiano::exporting::ExportFileType type)
{
    return devpiano::exporting::makeDefaultRecordingExportFile(type, getLastMidiExportDirectory(settings));
}
} // namespace

RecordingSessionController::RecordingSessionController(MainComponent& ownerIn,
                                                       RecordingEngine& recordingEngineIn,
                                                       AudioEngine& audioEngineIn,
                                                       SettingsModel& appSettingsIn,
                                                       ControlsPanel& controlsPanelIn)
    : owner(ownerIn)
    , recordingEngine(recordingEngineIn)
    , audioEngine(audioEngineIn)
    , appSettings(appSettingsIn)
    , controlsPanel(controlsPanelIn)
{
}

RecordingSessionController::~RecordingSessionController() = default;

void RecordingSessionController::handleRecordClicked()
{
    const auto command = chooseRecordingFlowCommand(RecordingFlowIntent::record,
                                                   makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));
    if (command != RecordingFlowCommand::startRecording)
        return;

    recordingEngine.clear();
    recordingSession.take = {};
    recordingSession.canExportMidi = false;
    startInternalRecording(0);
    recordingSession.state = toControlsPanelRecordingState(getStateAfterCommand(command,
                                                                                toRecordingFlowState(recordingSession.state)));
    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command))
        owner.restoreKeyboardFocus();
}

void RecordingSessionController::handlePlayClicked()
{
    const auto command = chooseRecordingFlowCommand(RecordingFlowIntent::play,
                                                   makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));
    if (command != RecordingFlowCommand::startPlayback)
        return;

    startInternalPlayback(recordingSession.take);
    recordingSession.state = toControlsPanelRecordingState(getStateAfterCommand(command,
                                                                                toRecordingFlowState(recordingSession.state)));
    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command))
        owner.restoreKeyboardFocus();
}

void RecordingSessionController::handleStopClicked()
{
    const auto command = chooseRecordingFlowCommand(RecordingFlowIntent::stop,
                                                   makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));

    if (command == RecordingFlowCommand::stopRecording)
    {
        recordingSession.take = stopInternalRecording();
        recordingSession.canExportMidi = recordingSession.hasTake();
    }
    else if (command == RecordingFlowCommand::stopPlayback)
    {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
    }
    else
    {
        return;
    }

    recordingSession.state = toControlsPanelRecordingState(getStateAfterCommand(command,
                                                                                toRecordingFlowState(recordingSession.state)));
    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command))
        owner.restoreKeyboardFocus();
}

void RecordingSessionController::handleBackToStartClicked()
{
    if (! recordingSession.hasTake() || recordingSession.isRecording())
        return;

    if (recordingSession.isPlaying())
    {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        startInternalPlayback(recordingSession.take);
        recordingSession.state = ControlsPanel::RecordingState::playing;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[Playback] Restarted from beginning");
    }
    else
    {
        audioEngine.requestAllNotesOff();
        DP_LOG_INFO("[Playback] Already at beginning");
    }

    owner.restoreKeyboardFocus();
}

void RecordingSessionController::handleExportMidiClicked()
{
    using devpiano::exporting::ExportFileType;

    runExportRecordingFlow(ExportFileType::midi,
                           exportMidiChooser,
                           "Export MIDI Recording",
                           "*.mid",
                           [this](const juce::File& file)
    {
        return devpiano::exporting::exportTakeAsMidiFile(recordingSession.take, file);
    });
}

void RecordingSessionController::handleExportWavClicked()
{
    using devpiano::exporting::ExportFileType;

    runExportRecordingFlow(ExportFileType::wav,
                           exportWavChooser,
                           "Export WAV Recording",
                           "*.wav",
                           [this](const juce::File& file)
    {
        const auto options = devpiano::exporting::buildWavExportOptions(recordingSession.take,
                                                                        appSettings.getPerformanceSettingsView(),
                                                                        getCurrentRuntimeSampleRate(),
                                                                        getCurrentRuntimeBlockSize());
        return devpiano::exporting::exportTakeAsWavFile(recordingSession.take, file, options);
    });
}

void RecordingSessionController::handleImportMidiClicked()
{
    if (recordingSession.isRecording())
    {
        DP_LOG_INFO("[MIDI Import] import skipped while recording");
        owner.restoreKeyboardFocus();
        return;
    }

    if (recordingSession.isPlaying())
    {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        recordingSession.state = ControlsPanel::RecordingState::idle;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[MIDI Import] stopped current playback before opening importer");
    }

    const auto startDir = getLastMidiImportDirectory(appSettings);

    importMidiChooser = std::make_unique<juce::FileChooser>("Import MIDI File", startDir, "*.mid;*.midi");
    importMidiChooser->launchAsync(juce::FileBrowserComponent::openMode
                                       | juce::FileBrowserComponent::canSelectFiles,
                                   [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (!file.exists())
        {
            importMidiChooser.reset();
            return;
        }

        appSettings.lastMidiImportPath = file.getFullPathName();
        owner.saveSettingsSoon();

        auto take = tryImportMidiFile(file);
        if (!take.has_value())
        {
            importMidiChooser.reset();
            return;
        }

        replaceTakeAndStartPlayback(std::move(*take));

        DP_LOG_INFO(("[MIDI Import] imported: " + file.getFullPathName()
                          + ", events=" + juce::String(static_cast<int>(recordingSession.take.events.size()))).toRawUTF8());

        importMidiChooser.reset();
        owner.restoreKeyboardFocus();
    });
}

void RecordingSessionController::handleSavePerformanceClicked()
{
    if (! recordingSession.hasTake())
    {
        DP_LOG_INFO("[Performance File] save skipped: no take available");
        return;
    }

    const auto defaultDir = juce::File::getCurrentWorkingDirectory();
    const juce::String defaultFileName { "performance-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S") + ".devpiano" };
    const auto defaultFile = defaultDir.getChildFile(defaultFileName);

    performanceFileChooser = std::make_unique<juce::FileChooser>("Save Performance", defaultFile, "*.devpiano");
    performanceFileChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                            | juce::FileBrowserComponent::canSelectFiles
                                            | juce::FileBrowserComponent::warnAboutOverwriting,
                                        [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
        {
            DP_LOG_INFO("[Performance File] save cancelled by user");
            performanceFileChooser.reset();
            return;
        }

        const juce::String timestamp = juce::Time::getCurrentTime().toISO8601(true);
        const PerformanceFileMetadata metadata {
            .createdAt = timestamp,
            .title = {},
            .notes = {}
        };

        if (devpiano::recording::savePerformanceFile(recordingSession.take, file, metadata))
            DP_LOG_INFO(("[Performance File] saved: " + file.getFullPathName()).toRawUTF8());
        else
            DP_LOG_ERROR(("[Performance File] save FAILED: " + file.getFullPathName()).toRawUTF8());

        performanceFileChooser.reset();
    });
}

void RecordingSessionController::handleOpenPerformanceClicked()
{
    if (recordingSession.isRecording())
    {
        DP_LOG_INFO("[Performance File] open skipped while recording");
        return;
    }

    if (recordingSession.isPlaying())
    {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        recordingSession.state = ControlsPanel::RecordingState::idle;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[Performance File] stopped current playback before opening");
    }

    const auto startDir = juce::File::getCurrentWorkingDirectory();

    performanceFileChooser = std::make_unique<juce::FileChooser>("Open Performance", startDir, "*.devpiano");
    performanceFileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                            | juce::FileBrowserComponent::canSelectFiles,
                                        [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (! file.exists())
        {
            performanceFileChooser.reset();
            return;
        }

        auto take = devpiano::recording::loadPerformanceFile(file);
        if (! take.has_value() || take->isEmpty())
        {
            DP_LOG_ERROR(("[Performance File] load failed or produced empty take: " + file.getFullPathName()).toRawUTF8());
            performanceFileChooser.reset();
            return;
        }

        recordingSession.take = std::move(*take);
        recordingSession.state = ControlsPanel::RecordingState::idle;
        syncRecordingSessionToUi();

        startInternalPlayback(recordingSession.take);
        recordingSession.state = ControlsPanel::RecordingState::playing;
        syncRecordingSessionToUi();

        DP_LOG_INFO(("[Performance File] opened: " + file.getFullPathName()
                          + ", events=" + juce::String(static_cast<int>(recordingSession.take.events.size()))).toRawUTF8());

        performanceFileChooser.reset();
    });
}

void RecordingSessionController::checkPlaybackEnded()
{
    if (! recordingEngine.consumePlaybackEndedFlag())
        return;

    if (! recordingSession.isPlaying())
        return;

    const auto stoppedTake = stopInternalPlayback();
    juce::ignoreUnused(stoppedTake);

    recordingSession.state = ControlsPanel::RecordingState::idle;
    syncRecordingSessionToUi();
    owner.restoreKeyboardFocus();
}

double RecordingSessionController::getCurrentRuntimeSampleRate() const
{
    return owner.getCurrentRuntimeSampleRate();
}

int RecordingSessionController::getCurrentRuntimeBlockSize() const
{
    return owner.getCurrentRuntimeBlockSize();
}

void RecordingSessionController::startInternalRecording(std::size_t expectedEventCapacity)
{
    const auto capacity = expectedEventCapacity > 0
                              ? expectedEventCapacity
                              : defaultRecordingEventsPerSecond * defaultRecordingCapacitySeconds;

    owner.runPluginActionWithAudioDeviceRebuild([this, capacity](const MainComponent::RuntimeAudioConfig& config)
    {
        recordingEngine.reserveEvents(capacity);
        recordingEngine.startRecording(config.sampleRate);
    });

    DP_LOG_INFO(("[Recording] Internal recording started; reserved events=" + juce::String(static_cast<int>(recordingEngine.getReservedEventCapacity()))).toRawUTF8());
}

RecordingTake RecordingSessionController::stopInternalRecording()
{
    RecordingTake take;

    owner.runPluginActionWithAudioDeviceRebuild([this, &take](const MainComponent::RuntimeAudioConfig&)
    {
        take = recordingEngine.stopRecording();
    });

    DP_LOG_INFO(("[Recording] Internal recording stopped; events="
                       + juce::String(static_cast<int>(take.events.size()))
                       + ", dropped="
                       + juce::String(static_cast<int>(recordingEngine.getDroppedEventCount()))).toRawUTF8());
    return take;
}

void RecordingSessionController::startInternalPlayback(const RecordingTake& take)
{
    if (take.isEmpty())
    {
        DP_LOG_WARN("[Playback] startInternalPlayback called with empty take - ignoring");
        return;
    }

    audioEngine.requestAllNotesOff();

    owner.runPluginActionWithAudioDeviceRebuild([this, &take](const MainComponent::RuntimeAudioConfig& config)
    {
        recordingEngine.startPlayback(take, config.sampleRate);
        audioEngine.armPlaybackStartPreRoll(config.sampleRate, config.blockSize);
    });

    DP_LOG_INFO(("[Playback] Internal playback started; take events="
                       + juce::String(static_cast<int>(take.events.size()))
                       + ", sampleRate="
                       + juce::String(take.sampleRate)).toRawUTF8());
}

RecordingTake RecordingSessionController::stopInternalPlayback()
{
    auto take = recordingEngine.getCurrentTake();
    DP_LOG_INFO("[Playback] stopInternalPlayback: calling requestAllNotesOff then stopPlayback");
    audioEngine.requestAllNotesOff();
    recordingEngine.stopPlayback();

    DP_LOG_INFO("[Playback] Internal playback stopped");
    return take;
}

void RecordingSessionController::syncRecordingSessionToUi()
{
    controlsPanel.setRecordingControlsState({ .state = recordingSession.state,
                                              .hasTake = recordingSession.hasTake(),
                                              .canExportMidiTake = recordingSession.hasTake(),
                                              .canExportWavTake = recordingSession.hasTake() });
}

void RecordingSessionController::runExportRecordingFlow(devpiano::exporting::ExportFileType type,
                                                        std::unique_ptr<juce::FileChooser>& chooser,
                                                        const juce::String& dialogTitle,
                                                        const juce::String& filePattern,
                                                        std::function<bool(const juce::File&)> doExport)
{
    const auto hasExportableTake = devpiano::exporting::canExportTake(recordingSession.take);
    const auto canExportRequestedType = type == devpiano::exporting::ExportFileType::midi
                                            ? recordingSession.hasTake()
                                            : hasExportableTake;

    if (! canExportRequestedType || ! hasExportableTake)
    {
        DP_LOG_INFO((devpiano::exporting::makeExportLogPrefix(type)
                     + " export skipped: recordingSession.take is empty or not exportable").toRawUTF8());
        return;
    }

    const auto defaultFile = makeDefaultMidiExportFile(appSettings, type);

    chooser = std::make_unique<juce::FileChooser>(dialogTitle, defaultFile, filePattern);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this, type, &chooser, doExport = std::move(doExport)](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (file == juce::File())
        {
            DP_LOG_INFO((devpiano::exporting::makeExportLogPrefix(type)
                         + " export cancelled by user").toRawUTF8());
            chooser.reset();
            return;
        }

        appSettings.lastMidiExportPath = file.getFullPathName();
        owner.saveSettingsSoon();

        if (doExport(file))
            DP_LOG_INFO((devpiano::exporting::makeExportLogPrefix(type)
                         + " exported: " + file.getFullPathName()).toRawUTF8());
        else
            DP_LOG_ERROR((devpiano::exporting::makeExportLogPrefix(type)
                          + " export FAILED: " + file.getFullPathName()).toRawUTF8());

        chooser.reset();
    });
}

std::optional<RecordingTake> RecordingSessionController::tryImportMidiFile(const juce::File& file) const
{
    const auto sampleRate = getCurrentRuntimeSampleRate();
    auto take = devpiano::recording::importMidiFile(file, sampleRate);

    if (!take.has_value() || take->isEmpty())
    {
        DP_LOG_ERROR(("[MIDI Import] import failed or produced empty take: " + file.getFullPathName()).toRawUTF8());
        return std::nullopt;
    }

    return take;
}

void RecordingSessionController::replaceTakeAndStartPlayback(RecordingTake take)
{
    if (recordingSession.isPlaying())
    {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        recordingSession.state = ControlsPanel::RecordingState::idle;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[MIDI Import] stopped current playback before replacing take");
    }

    recordingSession.take = std::move(take);
    recordingSession.canExportMidi = false;
    recordingSession.state = ControlsPanel::RecordingState::idle;
    syncRecordingSessionToUi();

    startInternalPlayback(recordingSession.take);
    recordingSession.state = ControlsPanel::RecordingState::playing;
    syncRecordingSessionToUi();
}

} // namespace devpiano::recording

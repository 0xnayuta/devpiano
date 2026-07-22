#include "RecordingSessionController.h"

#include "Audio/AudioEngine.h"
#include "Diagnostics/Log.h"
#include "Export/ExportFlowSupport.h"
#include "Export/WavExportTask.h"

#include "MainComponent.h"
#include "Plugin/PluginHost.h"
#include "Recording/MidiFileExporter.h"
#include "Recording/MidiFileImporter.h"
#include "Recording/PerformanceFile.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/RecordingFlowSupport.h"
#include "Recording/WavFileExporter.h"
#include "UI/PerformanceMetadataDialog.h"

namespace devpiano::recording {
namespace {
constexpr std::size_t defaultRecordingEventsPerSecond = 100;
constexpr std::size_t defaultRecordingCapacitySeconds = 30 * 60;

[[nodiscard]] RecordingFlowState toRecordingFlowState(ControlsPanel::RecordingState state) noexcept {
    switch (state) {
    case ControlsPanel::RecordingState::idle:
        return RecordingFlowState::idle;
    case ControlsPanel::RecordingState::recording:
        return RecordingFlowState::recording;
    case ControlsPanel::RecordingState::playing:
        return RecordingFlowState::playing;
    }

    return RecordingFlowState::idle;
}

[[nodiscard]] ControlsPanel::RecordingState toControlsPanelRecordingState(RecordingFlowState state) noexcept {
    switch (state) {
    case RecordingFlowState::idle:
        return ControlsPanel::RecordingState::idle;
    case RecordingFlowState::recording:
        return ControlsPanel::RecordingState::recording;
    case RecordingFlowState::playing:
        return ControlsPanel::RecordingState::playing;
    }

    return ControlsPanel::RecordingState::idle;
}

[[nodiscard]] RecordingFlowStatus makeRecordingFlowStatus(ControlsPanel::RecordingState state, bool hasTake) noexcept {
    return { .currentState = toRecordingFlowState(state), .hasTake = hasTake };
}

[[nodiscard]] juce::File getLastMidiExportDirectory(const SettingsModel& settings) {
    if (settings.lastMidiExportPath.isNotEmpty()) {
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

[[nodiscard]] juce::File getLastMidiImportDirectory(const SettingsModel& settings) {
    if (settings.lastMidiImportPath.isNotEmpty()) {
        const auto lastFile = juce::File(settings.lastMidiImportPath);
        if (lastFile.exists())
            return lastFile.getParentDirectory();
    }

    return juce::File {};
}

[[nodiscard]] juce::File makeDefaultMidiExportFile(const SettingsModel& settings,
                                                   devpiano::exporting::ExportFileType type) {
    return devpiano::exporting::makeDefaultRecordingExportFile(type, getLastMidiExportDirectory(settings));
}
} // namespace

RecordingSessionController::RecordingSessionController(MainComponent& ownerIn, RecordingEngine& recordingEngineIn,
                                                       AudioEngine& audioEngineIn, SettingsModel& appSettingsIn,
                                                       ControlsPanel& controlsPanelIn)
    : owner(ownerIn)
    , recordingEngine(recordingEngineIn)
    , audioEngine(audioEngineIn)
    , appSettings(appSettingsIn)
    , controlsPanel(controlsPanelIn)
    , aliveFlag_(std::make_shared<bool>(true)) {
}

RecordingSessionController::~RecordingSessionController() {
    if (aliveFlag_)
        *aliveFlag_ = false;
}

void RecordingSessionController::handleRecordClicked() {
    const auto command = chooseRecordingFlowCommand(
        RecordingFlowIntent::record, makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));
    if (command != RecordingFlowCommand::startRecording)
        return;

    recordingEngine.clear();
    recordingSession.take = {};
    recordingSession.canExportMidi = false;
    startInternalRecording(0);
    recordingSession.state
        = toControlsPanelRecordingState(getStateAfterCommand(command, toRecordingFlowState(recordingSession.state)));
    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command))
        owner.restoreKeyboardFocus();
}

void RecordingSessionController::handlePlayClicked() {
    const auto command = chooseRecordingFlowCommand(
        RecordingFlowIntent::play, makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));
    if (command != RecordingFlowCommand::startPlayback)
        return;

    startInternalPlayback(recordingSession.take);
    recordingSession.state
        = toControlsPanelRecordingState(getStateAfterCommand(command, toRecordingFlowState(recordingSession.state)));
    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command))
        owner.restoreKeyboardFocus();
}

void RecordingSessionController::handleStopClicked() {
    const auto command = chooseRecordingFlowCommand(
        RecordingFlowIntent::stop, makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));

    if (command == RecordingFlowCommand::stopRecording) {
        recordingSession.take = stopInternalRecording();
        recordingSession.canExportMidi = recordingSession.hasTake();
        // Pop up metadata dialog so the user can title the recording.
        PerformanceMetadataDialog::launch(
            recordingSession.currentMetadata, &owner,
            [this, aliveFlag = aliveFlag_](std::optional<PerformanceFileMetadata> result) {
                if (!*aliveFlag)
                    return;
                if (result.has_value())
                    recordingSession.currentMetadata = std::move(*result);
            });
    } else if (command == RecordingFlowCommand::stopPlayback) {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
    } else {
        return;
    }

    recordingSession.state
        = toControlsPanelRecordingState(getStateAfterCommand(command, toRecordingFlowState(recordingSession.state)));
    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command))
        owner.restoreKeyboardFocus();
}

void RecordingSessionController::handleBackToStartClicked() {
    if (!recordingSession.hasTake() || recordingSession.isRecording())
        return;

    if (recordingSession.isPlaying()) {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        startInternalPlayback(recordingSession.take);
        recordingSession.state = ControlsPanel::RecordingState::playing;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[Playback] Restarted from beginning");
    } else {
        audioEngine.requestAllNotesOff();
        DP_LOG_INFO("[Playback] Already at beginning");
    }

    owner.restoreKeyboardFocus();
}

void RecordingSessionController::handleExportMidiClicked() {
    using devpiano::exporting::ExportFileType;

    runExportRecordingFlow(ExportFileType::midi, exportMidiChooser, TRANS("Export MIDI Recording"), "*.mid",
                           [this](const juce::File& file) {
                               return devpiano::exporting::exportTakeAsMidiFile(recordingSession.take, file);
                           });
}
void RecordingSessionController::handleExportWavClicked() {
    using devpiano::exporting::ExportFileType;

    // Snapshot the take on the message thread for thread-safe background export.
    auto takeCopy = recordingSession.take;

    runExportRecordingFlow(
        ExportFileType::wav, exportWavChooser, TRANS("Export WAV Recording"), "*.wav",
        [this, take = std::move(takeCopy)](const juce::File& file) mutable {
            auto options = devpiano::exporting::buildWavExportOptions(take, appSettings.getPerformanceSettingsView(),
                                                                      getCurrentRuntimeSampleRate(),
                                                                      getCurrentRuntimeBlockSize());

            // Phase 1: Create offline plugin instance (MUST be on message thread)
            std::unique_ptr<juce::AudioPluginInstance> offlinePlugin;

            auto* pluginHost = audioEngine.getPluginHost();
            if (pluginHost != nullptr && pluginHost->hasLoadedPlugin()) {
                auto* liveInstance = pluginHost->getInstance();
                auto* desc = pluginHost->getLoadedPluginDescription();
                if (liveInstance != nullptr && desc != nullptr) {
                    auto state = devpiano::exporting::snapshotPluginState(*liveInstance);

                    juce::String error;
                    offlinePlugin = devpiano::exporting::createOfflinePluginInstance(
                        pluginHost->getFormatManager(), *desc, options.sampleRate, options.blockSize, error);

                    if (offlinePlugin != nullptr) {
                        offlinePlugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                    } else {
                        DP_LOG_WARN("[Export] Offline plugin instance creation failed: " + error
                                    + " — falling back to sine synth");
                    }
                }
            }

            // Phase 2: Background export with progress dialog
            WavExportTask task(std::move(take), file, options, std::move(offlinePlugin), &owner);
            task.runThread(); // blocks message thread via nested message loop

            if (task.wasSuccessful()) {
                DP_LOG_INFO("[Export] WAV exported: " + file.getFullPathName());
                return true;
            }

            DP_LOG_WARN("[Export] WAV export " + task.getErrorMessage());
            return false;
        });
}

void RecordingSessionController::handleImportMidiClicked() {
    const auto startDir = getLastMidiImportDirectory(appSettings);
    runImportOpenFlow("MIDI Import", TRANS("Import MIDI File"), startDir, "*.mid;*.midi", importMidiChooser,
                      [this](const juce::File& file) -> std::optional<RecordingTake> {
                          appSettings.lastMidiImportPath = file.getFullPathName();
                          owner.saveSettingsSoon();
                          return tryImportMidiFile(file);
                      });
}

void RecordingSessionController::handleSavePerformanceClicked() {
    if (!recordingSession.hasTake()) {
        DP_LOG_INFO("[Performance File] save skipped: no take available");
        return;
    }

    const auto defaultDir = juce::File::getCurrentWorkingDirectory();
    const juce::String defaultFileName { "performance-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")
                                         + ".devpiano" };
    const auto defaultFile = defaultDir.getChildFile(defaultFileName);

    performanceFileChooser = std::make_unique<juce::FileChooser>("Save Performance", defaultFile, "*.devpiano");
    performanceFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, aliveFlag = aliveFlag_](const juce::FileChooser& fc) {
            if (!*aliveFlag)
                return;
            auto file = fc.getResult();
            if (file == juce::File()) {
                DP_LOG_INFO("[Performance File] save cancelled by user");
                performanceFileChooser.reset();
                return;
            }

            auto metadata = recordingSession.currentMetadata;
            metadata.createdAt = juce::Time::getCurrentTime().toISO8601(true);

            if (devpiano::recording::savePerformanceFile(recordingSession.take, file, metadata))
                DP_LOG_INFO("[Performance File] saved: " + file.getFullPathName());
            else
                DP_LOG_ERROR("[Performance File] save FAILED: " + file.getFullPathName());

            performanceFileChooser.reset();
        });
}

void RecordingSessionController::handleOpenPerformanceClicked() {
    runImportOpenFlow("Performance File", TRANS("Open Performance"), juce::File::getCurrentWorkingDirectory(),
                      "*.devpiano", performanceFileChooser,
                      [this](const juce::File& file) -> std::optional<RecordingTake> {
                          auto metadata = devpiano::recording::loadPerformanceFileMetadata(file);
                          if (metadata.has_value()) {
                              recordingSession.currentMetadata = std::move(*metadata);
                              recordingSession.currentPerformanceFile = file;
                          }
                          return devpiano::recording::loadPerformanceFile(file);
                      });
}

void RecordingSessionController::handleOpenPerformanceFile(const juce::File& file) {
    if (recordingSession.isRecording()) {
        DP_LOG_INFO("[Performance File] open dropped file skipped while recording");
        return;
    }

    if (recordingSession.isPlaying()) {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        recordingSession.state = ControlsPanel::RecordingState::idle;
        syncRecordingSessionToUi();
    }

    auto take = devpiano::recording::loadPerformanceFile(file);
    if (!take.has_value() || take->isEmpty()) {
        DP_LOG_ERROR("[Performance File] dropped file failed or produced empty take: " + file.getFullPathName());
        return;
    }

    // Load metadata only after take is confirmed valid.
    auto metadata = devpiano::recording::loadPerformanceFileMetadata(file);
    if (metadata.has_value()) {
        recordingSession.currentMetadata = std::move(*metadata);
        recordingSession.currentPerformanceFile = file;
    }

    replaceTakeAndStartPlayback(std::move(*take));
    DP_LOG_INFO("[Performance File] loaded from dropped file: " + file.getFullPathName());
    owner.restoreKeyboardFocus();
    if (onFileOpened)
        onFileOpened(file);
}

void RecordingSessionController::handleImportMidiFile(const juce::File& file) {
    if (recordingSession.isRecording()) {
        DP_LOG_INFO("[MIDI Import] dropped MIDI file skipped while recording");
        return;
    }

    if (recordingSession.isPlaying()) {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        recordingSession.state = ControlsPanel::RecordingState::idle;
        syncRecordingSessionToUi();
    }

    appSettings.lastMidiImportPath = file.getFullPathName();
    owner.saveSettingsSoon();

    auto take = tryImportMidiFile(file);
    if (!take.has_value() || take->isEmpty())
        return;

    replaceTakeAndStartPlayback(std::move(*take));
    DP_LOG_INFO("[MIDI Import] imported from dropped file: " + file.getFullPathName());
    owner.restoreKeyboardFocus();
    if (onFileOpened)
        onFileOpened(file);
}

void RecordingSessionController::handlePlaybackSpeedChange(double speed) {
    recordingEngine.setPlaybackSpeedMultiplier(speed);
    controlsPanel.setPlaybackSpeed(speed);
    DP_DEBUG_LOG("[Playback] speed changed to " + juce::String(speed, 2) + "x");
}

void RecordingSessionController::checkPlaybackEnded() {
    if (!recordingEngine.consumePlaybackEndedFlag())
        return;

    if (!recordingSession.isPlaying())
        return;

    const auto stoppedTake = stopInternalPlayback();
    juce::ignoreUnused(stoppedTake);

    recordingSession.state = ControlsPanel::RecordingState::idle;
    syncRecordingSessionToUi();
    owner.restoreKeyboardFocus();
}

double RecordingSessionController::getCurrentRuntimeSampleRate() const {
    return owner.getCurrentRuntimeSampleRate();
}

int RecordingSessionController::getCurrentRuntimeBlockSize() const {
    return owner.getCurrentRuntimeBlockSize();
}

void RecordingSessionController::startInternalRecording(std::size_t expectedEventCapacity) {
    const auto capacity = expectedEventCapacity > 0 ? expectedEventCapacity
                                                    : defaultRecordingEventsPerSecond * defaultRecordingCapacitySeconds;

    owner.runPluginActionWithAudioDeviceRebuild([this, capacity](const MainComponent::RuntimeAudioConfig& config) {
        recordingEngine.reserveEvents(capacity);
        recordingEngine.startRecording(config.sampleRate);
    });

    DP_LOG_INFO("[Recording] Internal recording started; reserved events="
                + juce::String(static_cast<int>(recordingEngine.getReservedEventCapacity())));
}

RecordingTake RecordingSessionController::stopInternalRecording() {
    RecordingTake take;

    owner.runPluginActionWithAudioDeviceRebuild(
        [this, &take](const MainComponent::RuntimeAudioConfig&) { take = recordingEngine.stopRecording(); });

    DP_LOG_INFO("[Recording] Internal recording stopped; events=" + juce::String(static_cast<int>(take.events.size()))
                + ", dropped=" + juce::String(static_cast<int>(recordingEngine.getDroppedEventCount())));
    return take;
}

void RecordingSessionController::startInternalPlayback(const RecordingTake& take) {
    if (take.isEmpty()) {
        DP_LOG_WARN("[Playback] startInternalPlayback called with empty take - ignoring");
        return;
    }

    audioEngine.requestAllNotesOff();

    owner.runPluginActionWithAudioDeviceRebuild([this, &take](const MainComponent::RuntimeAudioConfig& config) {
        recordingEngine.startPlayback(take, config.sampleRate);
        audioEngine.armPlaybackStartPreRoll(config.sampleRate, config.blockSize);
    });

    DP_LOG_INFO("[Playback] Internal playback started; take events="
                + juce::String(static_cast<int>(take.events.size())) + ", sampleRate=" + juce::String(take.sampleRate));
}

RecordingTake RecordingSessionController::stopInternalPlayback() {
    auto take = recordingEngine.getCurrentTake();
    DP_LOG_INFO("[Playback] stopInternalPlayback: calling requestAllNotesOff then stopPlayback");
    audioEngine.requestAllNotesOff();
    recordingEngine.stopPlayback();

    DP_LOG_INFO("[Playback] Internal playback stopped");
    return take;
}

void RecordingSessionController::syncRecordingSessionToUi() {
    controlsPanel.setRecordingControlsState({ .state = recordingSession.state,
                                              .hasTake = recordingSession.hasTake(),
                                              .canExportMidiTake = recordingSession.canExportMidi,
                                              .canExportWavTake = recordingSession.hasTake() });
}

void RecordingSessionController::runExportRecordingFlow(devpiano::exporting::ExportFileType type,
                                                        std::unique_ptr<juce::FileChooser>& chooser,
                                                        const juce::String& dialogTitle,
                                                        const juce::String& filePattern,
                                                        std::function<bool(const juce::File&)> doExport) {
    const auto hasExportableTake = devpiano::exporting::canExportTake(recordingSession.take);
    const auto canExportRequestedType
        = type == devpiano::exporting::ExportFileType::midi ? recordingSession.canExportMidi : hasExportableTake;

    if (!canExportRequestedType || !hasExportableTake) {
        DP_LOG_INFO(devpiano::exporting::makeExportLogPrefix(type)
                    + " export skipped: recordingSession.take is empty or not exportable");
        return;
    }

    const auto defaultFile = makeDefaultMidiExportFile(appSettings, type);

    chooser = std::make_unique<juce::FileChooser>(dialogTitle, defaultFile, filePattern);
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, type, &chooser, doExportFn = std::move(doExport), aliveFlag = aliveFlag_](const juce::FileChooser& fc) {
            if (!*aliveFlag)
                return;
            auto file = fc.getResult();

            if (file == juce::File()) {
                DP_LOG_INFO(devpiano::exporting::makeExportLogPrefix(type) + " export cancelled by user");
                chooser.reset();
                return;
            }

            appSettings.lastMidiExportPath = file.getFullPathName();
            owner.saveSettingsSoon();

            if (doExportFn(file))
                DP_LOG_INFO(devpiano::exporting::makeExportLogPrefix(type) + " exported: " + file.getFullPathName());
            else
                DP_LOG_ERROR(devpiano::exporting::makeExportLogPrefix(type)
                             + " export FAILED: " + file.getFullPathName());

            chooser.reset();
        });
}

std::optional<RecordingTake> RecordingSessionController::tryImportMidiFile(const juce::File& file) const {
    const auto sampleRate = getCurrentRuntimeSampleRate();
    auto take = devpiano::recording::importMidiFile(file, sampleRate);

    if (!take.has_value() || take->isEmpty()) {
        DP_LOG_ERROR("[MIDI Import] import failed or produced empty take: " + file.getFullPathName());
        return std::nullopt;
    }

    return take;
}

void RecordingSessionController::replaceTakeAndStartPlayback(RecordingTake take) {
    if (recordingSession.isPlaying()) {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        recordingSession.state = ControlsPanel::RecordingState::idle;
        syncRecordingSessionToUi();
    }

    recordingSession.take = std::move(take);
    recordingSession.canExportMidi = false;
    recordingSession.state = ControlsPanel::RecordingState::idle;
    syncRecordingSessionToUi();

    startInternalPlayback(recordingSession.take);
    recordingSession.state = ControlsPanel::RecordingState::playing;
    syncRecordingSessionToUi();
}

void RecordingSessionController::runImportOpenFlow(
    const juce::String& logPrefix, const juce::String& dialogTitle, const juce::File& startDir,
    const juce::String& filePattern, std::unique_ptr<juce::FileChooser>& chooser,
    std::function<std::optional<RecordingTake>(const juce::File&)> loadTake) {
    if (recordingSession.isRecording()) {
        DP_LOG_INFO("[" + logPrefix + "] skipped while recording");
        owner.restoreKeyboardFocus();
        return;
    }

    if (recordingSession.isPlaying()) {
        const auto stoppedTake = stopInternalPlayback();
        juce::ignoreUnused(stoppedTake);
        recordingSession.state = ControlsPanel::RecordingState::idle;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[" + logPrefix + "] stopped current playback before opening");
    }

    chooser = std::make_unique<juce::FileChooser>(dialogTitle, startDir, filePattern);
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, logPrefix, &chooser, loadTakeFn = std::move(loadTake),
         aliveFlag = aliveFlag_](const juce::FileChooser& fc) {
            if (!*aliveFlag)
                return;
            auto file = fc.getResult();
            if (!file.exists()) {
                chooser.reset();
                return;
            }

            auto take = loadTakeFn(file);
            if (!take.has_value() || take->isEmpty()) {
                DP_LOG_ERROR("[" + logPrefix + "] failed or produced empty take: " + file.getFullPathName());
                chooser.reset();
                return;
            }

            replaceTakeAndStartPlayback(std::move(*take));

            DP_LOG_INFO("[" + logPrefix + "] " + file.getFullPathName()
                        + ", events=" + juce::String(static_cast<int>(recordingSession.take.events.size())));

            chooser.reset();
            owner.restoreKeyboardFocus();
            if (onFileOpened)
                onFileOpened(file);
        });
}

void RecordingSessionController::handleSongInfoClicked() {
    PerformanceMetadataDialog::launch(
        recordingSession.currentMetadata, &owner,
        [this, aliveFlag = aliveFlag_](std::optional<PerformanceFileMetadata> result) {
            if (!*aliveFlag)
                return;
            if (!result.has_value()) {
                owner.restoreKeyboardFocus();
                return; // cancelled
            }

            recordingSession.currentMetadata = std::move(*result);

            // If we have a backing .devpiano file, rewrite it with updated metadata.
            if (recordingSession.currentPerformanceFile.existsAsFile() && recordingSession.hasTake()) {
                auto metadata = recordingSession.currentMetadata;
                if (metadata.createdAt.isEmpty())
                    metadata.createdAt = juce::Time::getCurrentTime().toISO8601(true);

                if (devpiano::recording::savePerformanceFile(recordingSession.take,
                                                             recordingSession.currentPerformanceFile, metadata))
                    DP_LOG_INFO("[Performance File] metadata updated: "
                                + recordingSession.currentPerformanceFile.getFullPathName());
                else
                    DP_LOG_WARN("[Performance File] metadata update FAILED: "
                                + recordingSession.currentPerformanceFile.getFullPathName());
            }

            owner.restoreKeyboardFocus();
        });
}

} // namespace devpiano::recording

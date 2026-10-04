#include <algorithm>
#include <cmath>

#include "RecordingSessionController.h"

#include "Audio/AudioEngine.h"
#include "Audio/InstrumentEndpoint.h"
#include "Core/MetronomeModel.h"
#include "Diagnostics/Log.h"
#include "Export/ExportFlowSupport.h"
#include "Export/WavExportTask.h"

#include "Layout/PresetFlowSupport.h"
#include "MainComponent.h"
#include "Plugin/PluginHost.h"
#include "Recording/MidiFileExporter.h"
#include "Recording/MidiFileImporter.h"
#include "Recording/PerformanceFile.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/RecordingFlowSupport.h"
#include "Recording/WavFileExporter.h"
#include "UI/jive/JiveModalDialog.h"

namespace devpiano::recording {
namespace {
constexpr std::size_t defaultRecordingEventsPerSecond = 100;
constexpr std::size_t defaultRecordingCapacitySeconds = 1800;
} // namespace

RecordingSessionController::RecordingSessionController(MainComponent& ownerIn, RecordingEngine& recordingEngineIn,
                                                       AudioEngine& audioEngineIn, SettingsModel& appSettingsIn)
    : owner(ownerIn)
    , recordingEngine(recordingEngineIn)
    , audioEngine(audioEngineIn)
    , appSettings(appSettingsIn)
    , aliveFlag_(std::make_shared<bool>(true)) {
}

RecordingSessionController::~RecordingSessionController() {
    if (aliveFlag_) {
        *aliveFlag_ = false;
    }
}
bool RecordingSessionController::prepareForShutdown() {
    *aliveFlag_ = false;
    cancelCountIn();
    if (activeWavExportTask == nullptr) {
        return true;
    }
    activeWavExportTask->requestCancellation();
    return !activeWavExportTask->isRunning();
}

void RecordingSessionController::handleRecordClicked() {
    if (cancelCountIn(true)) {
        return;
    }

    const auto command = chooseRecordingFlowCommand(
        RecordingFlowIntent::record, makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));
    if (command != RecordingFlowCommand::startRecording) {
        return;
    }

    const auto barCount = devpiano::core::getCountInBarCount(appSettings.metronomeCountIn);
    if (barCount > 0 && recordingSession.state == ui::RecordingState::idle) {
        const auto beatsPerBar = devpiano::core::getTimeSignatureNumerator(audioEngine.getMetronomeTimeSignature());
        countInRemainingBeats = barCount * beatsPerBar;

        const auto capacity = defaultRecordingEventsPerSecond * defaultRecordingCapacitySeconds;
        owner.runPluginActionWithAudioDeviceRebuild([this, capacity](const MainComponent::RuntimeAudioConfig& config) {
            recordingEngine.reserveEvents(capacity);
            recordingEngine.armRecording(config.sampleRate);
            recordInitialPresetSnapshot();
        });

        audioEngine.getMetronomeProcessor().armCountIn(countInRemainingBeats);
        audioEngine.setMetronomeEnabled(true);
        owner.updateMetronomeUi();
        lastCountInSequence = audioEngine.getMetronomeBeatSequence();
        owner.showStatusMessage(TRANS("Count-in:") + " " + juce::String(countInRemainingBeats), 1200);
        return;
    }

    idleSeekPositionSamples.reset();
    pausedPlaybackCursor.reset();
    recordingSession.detachForNewRecording();
    startInternalRecording(0);
    recordingSession.state
        = toRecordingControlsState(getStateAfterCommand(command, toRecordingFlowState(recordingSession.state)));
    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command)) {
        owner.restoreKeyboardFocus();
    }
}

void RecordingSessionController::handlePlayClicked() {
    cancelCountIn();

    const auto command = chooseRecordingFlowCommand(
        RecordingFlowIntent::playPause, makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));
    if (command == RecordingFlowCommand::none) {
        return;
    }

    switch (command) {
    case RecordingFlowCommand::startPlayback:
        startInternalPlayback(recordingSession.take, idleSeekPositionSamples.value_or(0));
        idleSeekPositionSamples.reset();
        recordingSession.state = ui::RecordingState::playing;
        break;
    case RecordingFlowCommand::pausePlayback: {
        owner.runPluginActionWithAudioDeviceRebuild([this](const MainComponent::RuntimeAudioConfig& config) {
            recordingEngine.applyPendingTransportCommandsQuiescent();
            recordingEngine.pausePlayback();
            const auto sampleRateRatio = recordingSession.take.sampleRate > 0.0 && config.sampleRate > 0.0
                ? config.sampleRate / recordingSession.take.sampleRate
                : 1.0;
            pausedPlaybackCursor = { recordingEngine.getPlaybackPositionSamples(),
                                     sampleRateRatio / recordingEngine.getEffectivePlaybackSpeedMultiplier() };
        });
        recordingSession.state = ui::RecordingState::playingPaused;
        break;
    }
    case RecordingFlowCommand::resumePlayback: {
        std::int64_t resumeFromTakeSamples = 0;
        if (recordingEngine.getPendingPlaybackSeekSample(resumeFromTakeSamples)) {
            pausedPlaybackCursor.reset();
            startInternalPlayback(recordingSession.take, resumeFromTakeSamples);
        } else if (pausedPlaybackCursor.has_value()) {
            startInternalPlayback(recordingSession.take, 0, pausedPlaybackCursor);
        } else {
            resumeFromTakeSamples = recordingEngine.getPlaybackPositionInTakeSamples();
            startInternalPlayback(recordingSession.take, resumeFromTakeSamples);
        }
        pausedPlaybackCursor.reset();
        idleSeekPositionSamples.reset();
        recordingSession.state = ui::RecordingState::playing;
        break;
    }
    case RecordingFlowCommand::pauseRecording:
        owner.runPluginActionWithAudioDeviceRebuild(
            [this](const MainComponent::RuntimeAudioConfig&) { recordingEngine.pauseRecording(); });
        recordingSession.state = ui::RecordingState::recordingPaused;
        break;
    case RecordingFlowCommand::resumeRecording:
        owner.runPluginActionWithAudioDeviceRebuild(
            [this](const MainComponent::RuntimeAudioConfig&) { recordingEngine.resumeRecording(); });
        recordingSession.state = ui::RecordingState::recording;
        break;
    // 其余命令在此函数中无副作用（none 已提前返回；startRecording /
    // stopRecording / stopPlayback 由录制/停止路径处理），显式列出以满足
    // -Wswitch-enum 全枚举覆盖。
    case RecordingFlowCommand::none:
    case RecordingFlowCommand::startRecording:
    case RecordingFlowCommand::stopRecording:
    case RecordingFlowCommand::stopPlayback:
        return;
    }

    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command)) {
        owner.restoreKeyboardFocus();
    }
}

void RecordingSessionController::handleStopClicked() {
    cancelCountIn(true);

    const auto command = chooseRecordingFlowCommand(
        RecordingFlowIntent::stop, makeRecordingFlowStatus(recordingSession.state, recordingSession.hasTake()));

    if (command == RecordingFlowCommand::stopRecording) {
        recordingSession.commitRecordedTake(stopInternalRecording());
        const auto expectedGeneration = recordingSession.takeGeneration;
        // Pop up metadata dialog so the user can title the recording.
        devpiano::ui::jive::JiveModalDialog::launchMetadataEdit({
            .title = TRANS("Song Information"),
            .initialTitle = recordingSession.currentMetadata.title,
            .initialNotes = recordingSession.currentMetadata.notes,
            .componentToCentreAround = &owner,
            .onComplete =
                [this, expectedGeneration,
                 aliveFlag = aliveFlag_](std::optional<devpiano::ui::jive::JiveModalDialog::MetadataResult> result) {
                    if (!*aliveFlag) {
                        return;
                    }
                    if (recordingSession.takeGeneration != expectedGeneration) {
                        owner.restoreKeyboardFocus();
                        return;
                    }
                    if (result.has_value()) {
                        recordingSession.updateMetadata(std::move(result->title), std::move(result->notes),
                                                        expectedGeneration);
                    }
                    owner.restoreKeyboardFocus();
                },
        });
    } else if (command == RecordingFlowCommand::stopPlayback) {
        stopInternalPlayback();
    } else {
        return;
    }

    recordingSession.state
        = toRecordingControlsState(getStateAfterCommand(command, toRecordingFlowState(recordingSession.state)));
    syncRecordingSessionToUi();
    if (shouldRestoreKeyboardFocus(command)) {
        owner.restoreKeyboardFocus();
    }
}

void RecordingSessionController::handleBackToStartClicked() {
    cancelCountIn();

    if (!recordingSession.hasTake() || recordingSession.isRecording()) {
        return;
    }
    if (recordingSession.state == ui::RecordingState::playing) {
        idleSeekPositionSamples.reset();
        pausedPlaybackCursor.reset();
        stopInternalPlayback();
        startInternalPlayback(recordingSession.take, 0);
        recordingSession.state = ui::RecordingState::playing;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[Playback] Restarted from beginning");
    } else if (recordingSession.state == ui::RecordingState::playingPaused) {
        idleSeekPositionSamples.reset();
        pausedPlaybackCursor.reset();
        startInternalPlayback(recordingSession.take, 0);
        recordingEngine.pausePlayback();
        recordingSession.state = ui::RecordingState::playingPaused;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[Playback] Rewound to beginning (paused)");
    } else {
        idleSeekPositionSamples = 0;
        pausedPlaybackCursor.reset();
        audioEngine.requestAllNotesOff();
        DP_LOG_INFO("[Playback] Rewound to beginning");
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

    const auto hasExportableTake = devpiano::exporting::canExportTake(recordingSession.take);
    if (!hasExportableTake) {
        DP_LOG_INFO(devpiano::exporting::makeExportLogPrefix(ExportFileType::wav)
                    + " export skipped: recordingSession.take is empty or not exportable");
        return;
    }

    const auto defaultFile = devpiano::exporting::makeDefaultRecordingExportFile(
        ExportFileType::wav, devpiano::exporting::getLastMidiExportDirectory(appSettings));

    exportWavChooser = std::make_unique<juce::FileChooser>(TRANS("Export WAV Recording"), defaultFile, "*.wav");
    exportWavChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, aliveFlag = aliveFlag_](const juce::FileChooser& fc) {
            if (!*aliveFlag) {
                return;
            }
            auto file = fc.getResult();
            if (file == juce::File()) {
                DP_LOG_INFO("[Export] WAV export cancelled by user");
                exportWavChooser.reset();
                return;
            }

            appSettings.lastMidiExportPath = file.getFullPathName();
            owner.saveSettingsSoon();

            // Snapshot the take on the message thread for thread-safe background export.
            auto take = recordingSession.take;
            auto options = devpiano::exporting::buildWavExportOptions(take, appSettings.getPerformanceSettingsView(),
                                                                      getCurrentRuntimeSampleRate(),
                                                                      getCurrentRuntimeBlockSize());

            // Phase 1: Create offline plugin instance under audio device rebuild guard
            // (MUST pause audio callback while snapshotting live plugin state, PluginHost.h)
            std::unique_ptr<juce::AudioPluginInstance> offlinePlugin;

            owner.runPluginActionWithAudioDeviceRebuild([&] {
                auto* pluginHost = audioEngine.getPluginHost();
                const auto endpoint = devpiano::audio::resolveInstrumentEndpoint(pluginHost);
                if (!endpoint.isHostedPlugin() || endpoint.hostedDescription == nullptr) {
                    return;
                }

                auto state = devpiano::exporting::snapshotPluginState(*endpoint.hostedInstance);

                juce::String error;
                offlinePlugin = devpiano::exporting::createOfflinePluginInstance(
                    pluginHost->getFormatManager(), *endpoint.hostedDescription, options.sampleRate, options.blockSize,
                    error);

                if (offlinePlugin != nullptr) {
                    offlinePlugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
                } else {
                    DP_LOG_WARN("[Export] Offline plugin instance creation failed: " + error
                                + " - falling back to sine synth");
                }
            });

            // Phase 2: Launch asynchronous background export task
            activeWavExportTask
                = std::make_unique<WavExportTask>(std::move(take), file, options, std::move(offlinePlugin), &owner);
            activeWavExportTask->startAsync([this, aliveFlag, file](bool ok, const juce::String& errorMsg) {
                if (!*aliveFlag) {
                    return;
                }
                if (ok) {
                    DP_LOG_INFO("[Export] WAV exported: " + file.getFullPathName());
                    owner.showStatusMessage(TRANS("WAV export completed: ") + file.getFileName(), 2500);
                } else {
                    if (!errorMsg.isEmpty()) {
                        owner.showStatusMessage(TRANS("Export failed: ") + errorMsg, 3000);
                    }
                    DP_LOG_WARN("[Export] WAV export " + errorMsg);
                }
                activeWavExportTask.reset();
                exportWavChooser.reset();
            });
        });
}

void RecordingSessionController::handleImportMidiClicked() {
    cancelCountIn();
    const auto startDir = devpiano::exporting::getLastMidiImportDirectory(appSettings);
    runImportOpenFlow("MIDI Import", TRANS("Import MIDI File"), startDir, "*.mid;*.midi", importMidiChooser,
                      [this](const juce::File& file) -> bool { return commitImportedMidiFile(file); });
}

void RecordingSessionController::handleSavePerformanceClicked() {
    cancelCountIn();
    if (!recordingSession.hasTake()) {
        DP_LOG_INFO("[Performance File] save skipped: no take available");
        return;
    }

    const auto defaultDir = juce::File::getCurrentWorkingDirectory();
    const juce::String defaultFileName { "performance-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")
                                         + ".devpiano" };
    const auto defaultFile = defaultDir.getChildFile(defaultFileName);

    const auto expectedGeneration = recordingSession.takeGeneration;
    performanceFileChooser = std::make_unique<juce::FileChooser>("Save Performance", defaultFile, "*.devpiano");
    performanceFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, expectedGeneration, aliveFlag = aliveFlag_](const juce::FileChooser& fc) {
            if (!*aliveFlag) {
                return;
            }
            if (recordingSession.takeGeneration != expectedGeneration) {
                DP_LOG_INFO("[Performance File] save skipped: take replaced during chooser");
                performanceFileChooser.reset();
                return;
            }
            auto file = fc.getResult();
            if (file == juce::File()) {
                DP_LOG_INFO("[Performance File] save cancelled by user");
                performanceFileChooser.reset();
                return;
            }

            if (recordingSession.saveToFile(file, expectedGeneration)) {
                DP_LOG_INFO("[Performance File] saved: " + file.getFullPathName());
            } else {
                DP_LOG_ERROR("[Performance File] save FAILED: " + file.getFullPathName());
            }

            performanceFileChooser.reset();
        });
}

void RecordingSessionController::handleOpenPerformanceClicked() {
    cancelCountIn();
    runImportOpenFlow("Performance File", TRANS("Open Performance"), juce::File::getCurrentWorkingDirectory(),
                      "*.devpiano", performanceFileChooser,
                      [this](const juce::File& file) -> bool { return commitOpenedPerformanceFile(file); });
}

void RecordingSessionController::handleOpenPerformanceFile(const juce::File& file) {
    cancelCountIn();
    if (recordingSession.isRecording()) {
        DP_LOG_INFO("[Performance File] open dropped file skipped while recording");
        return;
    }

    if (!commitOpenedPerformanceFile(file)) {
        return;
    }

    startPlaybackAfterTakeCommitted();
    DP_LOG_INFO("[Performance File] loaded from dropped file: " + file.getFullPathName());
    owner.restoreKeyboardFocus();
    if (onFileOpened) {
        onFileOpened(file);
    }
}

void RecordingSessionController::handleImportMidiFile(const juce::File& file) {
    cancelCountIn();
    if (recordingSession.isRecording()) {
        DP_LOG_INFO("[MIDI Import] dropped MIDI file skipped while recording");
        return;
    }

    if (!commitImportedMidiFile(file)) {
        return;
    }

    startPlaybackAfterTakeCommitted();
    DP_LOG_INFO("[MIDI Import] imported from dropped file: " + file.getFullPathName());
    owner.restoreKeyboardFocus();
    if (onFileOpened) {
        onFileOpened(file);
    }
}

void RecordingSessionController::handlePlaybackSpeedChange(double speed) {
    recordingEngine.setPlaybackSpeedMultiplier(speed);
    owner.setControlsPlaybackSpeed(speed);
    DP_DEBUG_LOG("[Playback] speed changed to " + juce::String(speed, 2) + "x");
}

void RecordingSessionController::seekPlaybackToSample(std::int64_t takeSample) {
    cancelCountIn();
    const auto timeline = getPlaybackTimelineSnapshot();
    if (!timeline.enabled) {
        return;
    }

    const auto clampedSample = juce::jlimit<std::int64_t>(0, timeline.lengthSamples, takeSample);
    if (recordingSession.isPlaying()) {
        idleSeekPositionSamples.reset();
        recordingEngine.requestPlaybackSeek(clampedSample);
    } else {
        idleSeekPositionSamples = clampedSample;
        audioEngine.requestAllNotesOff();
    }
}

void RecordingSessionController::setPlaybackLoopStart() {
    const auto timeline = getPlaybackTimelineSnapshot();
    if (timeline.enabled) {
        recordingEngine.setPlaybackLoopStartSample(timeline.positionSamples);
    }
}

void RecordingSessionController::setPlaybackLoopEnd() {
    const auto timeline = getPlaybackTimelineSnapshot();
    if (timeline.enabled) {
        recordingEngine.setPlaybackLoopEndSample(timeline.positionSamples);
    }
}

void RecordingSessionController::clearPlaybackLoop() {
    recordingEngine.clearPlaybackLoop();
}

RecordingSessionController::PlaybackTimelineSnapshot
RecordingSessionController::getPlaybackTimelineSnapshot() const noexcept {
    PlaybackTimelineSnapshot snapshot;
    snapshot.lengthSamples = std::max<std::int64_t>(recordingSession.take.lengthSamples, 0);
    snapshot.sampleRate = recordingSession.take.sampleRate;
    snapshot.loopRange = recordingEngine.getPlaybackLoopRange();
    snapshot.enabled = snapshot.lengthSamples > 0 && !recordingSession.isRecording();
    if (!snapshot.enabled) {
        return snapshot;
    }

    if (recordingSession.isPlaying()) {
        if (!recordingEngine.getPendingPlaybackSeekSample(snapshot.positionSamples)) {
            snapshot.positionSamples = recordingEngine.getPlaybackPositionInTakeSamples();
        }
    } else if (idleSeekPositionSamples.has_value()) {
        snapshot.positionSamples = *idleSeekPositionSamples;
    } else if (recordingEngine.getPlaybackTakeLengthSamples() == snapshot.lengthSamples) {
        snapshot.positionSamples = recordingEngine.getPlaybackPositionInTakeSamples();
    }

    snapshot.positionSamples = juce::jlimit<std::int64_t>(0, snapshot.lengthSamples, snapshot.positionSamples);
    return snapshot;
}

void RecordingSessionController::checkPlaybackEnded() {
    checkCountIn();

    if (!recordingEngine.consumePlaybackEndedFlag()) {
        return;
    }

    // Only a genuinely playing session ends; a paused one must not be kicked
    // back to idle by a stale flag.
    if (recordingSession.state != ui::RecordingState::playing) {
        return;
    }

    // 实时线程仅置 playbackEndedPending 标志；日志在消息线程输出,
    // 避免音频回调内 juce::Logger 互斥锁 + 磁盘 I/O(ERR-001)。
    DP_LOG_INFO("[RecordingEngine] playback ENDED at pos=" + juce::String(recordingEngine.getPlaybackPositionSamples())
                + " (speed=" + juce::String(recordingEngine.getPlaybackSpeedMultiplier()) + "x)");

    pausedPlaybackCursor.reset();

    recordingSession.state = ui::RecordingState::idle;
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
        recordingEngine.clear();
        recordingEngine.reserveEvents(capacity);
        recordingEngine.startRecording(config.sampleRate);
        recordInitialPresetSnapshot();
    });

    DP_LOG_INFO("[Recording] Internal recording started; reserved events="
                + juce::String(static_cast<int>(recordingEngine.getReservedEventCapacity())));
}

void RecordingSessionController::recordInitialPresetSnapshot() {
    RecordedPreset snapshot;
    snapshot.preset = owner.presetFlowSupport->captureCurrentState(owner.keyboardMidiMapper.getLayout().name,
                                                                   owner.presetFlowSupport->getCurrentPresetId());
    snapshot.acoustic = audioEngine.captureAcousticSnapshot();
    snapshot.acoustic.unaCorda = appSettings.unaCorda;
    recordingEngine.recordPresetChange(snapshot);
}

RecordingTake RecordingSessionController::stopInternalRecording() {
    RecordingTake take;

    owner.runPluginActionWithAudioDeviceRebuild(
        [this, &take](const MainComponent::RuntimeAudioConfig&) { take = recordingEngine.stopRecording(); });

    DP_LOG_INFO("[Recording] Internal recording stopped; events=" + juce::String(static_cast<int>(take.events.size()))
                + ", dropped=" + juce::String(static_cast<int>(recordingEngine.getDroppedEventCount())));
    return take;
}

void RecordingSessionController::startInternalPlayback(const RecordingTake& take, std::int64_t resumeFromTakeSamples,
                                                       std::optional<PausedPlaybackCursor> pausedCursor) {
    if (take.isEmpty()) {
        DP_LOG_WARN("[Playback] startInternalPlayback called with empty take - ignoring");
        return;
    }

    audioEngine.requestAllNotesOff();
    pausedPlaybackCursor.reset();

    owner.runPluginActionWithAudioDeviceRebuild(
        [this, &take, resumeFromTakeSamples, pausedCursor](const MainComponent::RuntimeAudioConfig& config) {
            if (pausedCursor.has_value() && pausedCursor->combinedRatio > 0.0) {
                const auto sampleRateRatio
                    = (take.sampleRate > 0.0 && config.sampleRate > 0.0) ? config.sampleRate / take.sampleRate : 1.0;
                const auto combinedRatio = sampleRateRatio / recordingEngine.getPlaybackSpeedMultiplier();
                const auto scale = combinedRatio / pausedCursor->combinedRatio;
                const auto scaledPosition = static_cast<std::int64_t>(
                    std::llround(static_cast<double>(pausedCursor->scaledPositionSamples) * scale));
                recordingEngine.startPlayback(take, config.sampleRate, scaledPosition);
            } else {
                recordingEngine.startPlaybackAtTakeSample(take, config.sampleRate, resumeFromTakeSamples);
            }
            audioEngine.preparePlaybackResources();
            audioEngine.armPlaybackStartPreRoll(config.sampleRate, config.blockSize);
        });

    DP_LOG_INFO(juce::String("[Playback] Internal playback started")
                + (pausedCursor.has_value() || resumeFromTakeSamples > 0 ? " (resumed)" : "") + "; take events="
                + juce::String(static_cast<int>(take.events.size())) + ", sampleRate=" + juce::String(take.sampleRate));
}

void RecordingSessionController::stopInternalPlayback() {
    DP_LOG_INFO("[Playback] stopInternalPlayback: publishing block-boundary stop");
    recordingEngine.requestPlaybackStop();
    pausedPlaybackCursor.reset();

    DP_LOG_INFO("[Playback] Internal playback stopped");
}

void RecordingSessionController::syncRecordingSessionToUi() {
    owner.setRecordingControlsState({ .state = recordingSession.state,
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

    const auto defaultFile = devpiano::exporting::makeDefaultRecordingExportFile(
        type, devpiano::exporting::getLastMidiExportDirectory(appSettings));

    chooser = std::make_unique<juce::FileChooser>(dialogTitle, defaultFile, filePattern);
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, type, &chooser, doExportFn = std::move(doExport), aliveFlag = aliveFlag_](const juce::FileChooser& fc) {
            if (!*aliveFlag) {
                return;
            }
            auto file = fc.getResult();

            if (file == juce::File()) {
                DP_LOG_INFO(devpiano::exporting::makeExportLogPrefix(type) + " export cancelled by user");
                chooser.reset();
                return;
            }

            appSettings.lastMidiExportPath = file.getFullPathName();
            owner.saveSettingsSoon();

            if (doExportFn(file)) {
                DP_LOG_INFO(devpiano::exporting::makeExportLogPrefix(type) + " exported: " + file.getFullPathName());
            } else {
                DP_LOG_ERROR(devpiano::exporting::makeExportLogPrefix(type)
                             + " export FAILED: " + file.getFullPathName());
            }

            chooser.reset();
        });
}

bool RecordingSessionController::commitImportedMidiFile(const juce::File& file) {
    const auto sampleRate = getCurrentRuntimeSampleRate();
    auto result = devpiano::recording::importMidiFileWithMetadata(file, sampleRate);

    if (!result.has_value() || result->take.isEmpty()) {
        DP_LOG_ERROR("[MIDI Import] import failed or produced empty take: " + file.getFullPathName());
        return false;
    }

    appSettings.lastMidiImportPath = file.getFullPathName();
    owner.saveSettingsSoon();

    const auto songTitle
        = result->metadata.songTitle.isNotEmpty() ? result->metadata.songTitle : file.getFileNameWithoutExtension();

    return recordingSession.commitImportedMidi(std::move(result->take), songTitle);
}

bool RecordingSessionController::commitOpenedPerformanceFile(const juce::File& file) {
    if (!recordingSession.openFromFile(file)) {
        DP_LOG_ERROR("[Performance File] open failed or produced empty take: " + file.getFullPathName());
        return false;
    }
    return true;
}

void RecordingSessionController::startPlaybackAfterTakeCommitted() {
    cancelCountIn();
    if (recordingSession.isPlaying()) {
        stopInternalPlayback();
        recordingSession.state = ui::RecordingState::idle;
        syncRecordingSessionToUi();
    }

    recordingEngine.clearPlaybackLoop();
    idleSeekPositionSamples.reset();
    recordingSession.state = ui::RecordingState::idle;
    syncRecordingSessionToUi();

    startInternalPlayback(recordingSession.take);
    recordingSession.state = ui::RecordingState::playing;
    syncRecordingSessionToUi();
}

void RecordingSessionController::runImportOpenFlow(const juce::String& logPrefix, const juce::String& dialogTitle,
                                                   const juce::File& startDir, const juce::String& filePattern,
                                                   std::unique_ptr<juce::FileChooser>& chooser,
                                                   std::function<bool(const juce::File&)> loadAndCommit) {
    cancelCountIn();
    if (recordingSession.isRecording()) {
        DP_LOG_INFO("[" + logPrefix + "] skipped while recording");
        owner.restoreKeyboardFocus();
        return;
    }

    if (recordingSession.isPlaying()) {
        stopInternalPlayback();
        recordingSession.state = ui::RecordingState::idle;
        syncRecordingSessionToUi();
        DP_LOG_INFO("[" + logPrefix + "] stopped current playback before opening");
    }

    const auto expectedGeneration = recordingSession.takeGeneration;
    chooser = std::make_unique<juce::FileChooser>(dialogTitle, startDir, filePattern);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, logPrefix, expectedGeneration, &chooser, loadAndCommitFn = std::move(loadAndCommit),
                          aliveFlag = aliveFlag_](const juce::FileChooser& fc) {
                             if (!*aliveFlag) {
                                 return;
                             }
                             if (recordingSession.takeGeneration != expectedGeneration) {
                                 DP_LOG_INFO("[" + logPrefix + "] open cancelled: take replaced during chooser");
                                 chooser.reset();
                                 return;
                             }
                             if (recordingSession.isRecording()) {
                                 DP_LOG_INFO("[" + logPrefix + "] open cancelled: recording in progress");
                                 chooser.reset();
                                 return;
                             }
                             auto file = fc.getResult();
                             if (!file.exists()) {
                                 chooser.reset();
                                 return;
                             }

                             if (!loadAndCommitFn(file)) {
                                 chooser.reset();
                                 return;
                             }

                             startPlaybackAfterTakeCommitted();

                             DP_LOG_INFO("[" + logPrefix + "] " + file.getFullPathName() + ", events="
                                         + juce::String(static_cast<int>(recordingSession.take.events.size())));

                             chooser.reset();
                             owner.restoreKeyboardFocus();
                             if (onFileOpened) {
                                 onFileOpened(file);
                             }
                         });
}

void RecordingSessionController::handleSongInfoClicked() {
    cancelCountIn();
    const auto expectedGeneration = recordingSession.takeGeneration;
    devpiano::ui::jive::JiveModalDialog::launchMetadataEdit({
        .title = TRANS("Song Information"),
        .initialTitle = recordingSession.currentMetadata.title,
        .initialNotes = recordingSession.currentMetadata.notes,
        .componentToCentreAround = &owner,
        .onComplete =
            [this, expectedGeneration,
             aliveFlag = aliveFlag_](std::optional<devpiano::ui::jive::JiveModalDialog::MetadataResult> result) {
                if (!*aliveFlag) {
                    return;
                }
                if (!result.has_value()) {
                    owner.restoreKeyboardFocus();
                    return; // cancelled
                }
                if (recordingSession.takeGeneration != expectedGeneration) {
                    owner.restoreKeyboardFocus();
                    return; // take changed while modal was open
                }

                if (!recordingSession.updateMetadata(std::move(result->title), std::move(result->notes),
                                                     expectedGeneration)) {
                    DP_LOG_WARN("[Performance File] metadata update FAILED: "
                                + recordingSession.currentPerformanceFile.getFullPathName());
                }

                owner.restoreKeyboardFocus();
            },
    });
}

bool RecordingSessionController::syncAudioRecordingStartIfNeeded() {
    if (recordingEngine.getState() == RecordingState::recording && recordingSession.state == ui::RecordingState::idle) {
        countInRemainingBeats = 0;
        lastCountInSequence = 0;
        audioEngine.getMetronomeProcessor().cancelCountIn();
        idleSeekPositionSamples.reset();
        pausedPlaybackCursor.reset();
        recordingSession.detachForNewRecording();
        recordingSession.state = ui::RecordingState::recording;
        syncRecordingSessionToUi();
        owner.showStatusMessage(TRANS("Recording Started"), 1000);
        owner.restoreKeyboardFocus();
        return true;
    }
    return false;
}

void RecordingSessionController::checkCountIn() {
    if (syncAudioRecordingStartIfNeeded()) {
        return;
    }

    if (countInRemainingBeats <= 0 && recordingEngine.getState() != RecordingState::countingIn) {
        return;
    }

    const auto engineState = recordingEngine.getState();
    const bool engineCanContinueCountIn
        = (engineState == RecordingState::countingIn || engineState == RecordingState::idle
           || engineState == RecordingState::stopped);
    if (!shouldContinueCountIn(toRecordingFlowState(recordingSession.state), engineCanContinueCountIn,
                               audioEngine.isMetronomeEnabled())) {
        cancelCountIn(true);
        return;
    }

    const auto currentSeq = audioEngine.getMetronomeBeatSequence();
    if (currentSeq != lastCountInSequence) {
        lastCountInSequence = currentSeq;
        const auto remaining = audioEngine.getMetronomeProcessor().getCountInRemainingBeats();
        if (remaining > 0) {
            countInRemainingBeats = remaining;
            owner.showStatusMessage(TRANS("Count-in:") + " " + juce::String(countInRemainingBeats), 1000);
        }
    }
}

bool RecordingSessionController::cancelCountIn(bool notifyUser) {
    if (syncAudioRecordingStartIfNeeded()) {
        return false;
    }

    const bool wasArmedOrCountingIn = countInRemainingBeats > 0
        || (audioEngine.getMetronomeProcessor().getCountInRemainingBeats() > 0)
        || (recordingEngine.getState() == RecordingState::countingIn);

    if (!wasArmedOrCountingIn) {
        return false;
    }

    bool audioWon = false;
    bool disarmed = false;

    owner.runPluginActionWithAudioDeviceRebuild([this, &audioWon, &disarmed](const MainComponent::RuntimeAudioConfig&) {
        if (recordingEngine.getState() == RecordingState::recording) {
            audioWon = true;
        } else {
            recordingEngine.cancelArmedRecording();
            audioEngine.getMetronomeProcessor().cancelCountIn();
            disarmed = true;
        }
    });

    if (audioWon) {
        syncAudioRecordingStartIfNeeded();
        return false;
    }

    if (disarmed) {
        countInRemainingBeats = 0;
        lastCountInSequence = 0;
        if (notifyUser) {
            owner.showStatusMessage(TRANS("Count-in Cancelled"), 800);
        }
        return true;
    }

    return false;
}

} // namespace devpiano::recording

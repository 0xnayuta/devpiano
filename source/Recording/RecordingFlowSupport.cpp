#include "RecordingFlowSupport.h"

namespace devpiano::recording {

RecordingFlowCommand chooseRecordingFlowCommand(RecordingFlowIntent intent, RecordingFlowStatus status) noexcept {
    switch (intent) {
    case RecordingFlowIntent::record:
        if (status.currentState == RecordingFlowState::idle) {
            return RecordingFlowCommand::startRecording;
        }
        return RecordingFlowCommand::none;

    case RecordingFlowIntent::playPause:
        switch (status.currentState) {
        case RecordingFlowState::idle:
            return status.hasTake ? RecordingFlowCommand::startPlayback : RecordingFlowCommand::none;
        case RecordingFlowState::playing:
            return RecordingFlowCommand::pausePlayback;
        case RecordingFlowState::playingPaused:
            return RecordingFlowCommand::resumePlayback;
        case RecordingFlowState::recording:
            return RecordingFlowCommand::pauseRecording;
        case RecordingFlowState::recordingPaused:
            return RecordingFlowCommand::resumeRecording;
        }
        return RecordingFlowCommand::none;

    case RecordingFlowIntent::stop:
        if (status.currentState == RecordingFlowState::recording
            || status.currentState == RecordingFlowState::recordingPaused) {
            return RecordingFlowCommand::stopRecording;
        }

        if (status.currentState == RecordingFlowState::playing
            || status.currentState == RecordingFlowState::playingPaused) {
            return RecordingFlowCommand::stopPlayback;
        }

        return RecordingFlowCommand::none;
    }

    return RecordingFlowCommand::none;
}

RecordingFlowState getStateAfterCommand(RecordingFlowCommand command, RecordingFlowState fallbackState) noexcept {
    switch (command) {
    case RecordingFlowCommand::startRecording:
        return RecordingFlowState::recording;
    case RecordingFlowCommand::startPlayback:
    case RecordingFlowCommand::resumePlayback:
        return RecordingFlowState::playing;
    case RecordingFlowCommand::pausePlayback:
        return RecordingFlowState::playingPaused;
    case RecordingFlowCommand::pauseRecording:
        return RecordingFlowState::recordingPaused;
    case RecordingFlowCommand::resumeRecording:
        return RecordingFlowState::recording;
    case RecordingFlowCommand::stopRecording:
    case RecordingFlowCommand::stopPlayback:
        return RecordingFlowState::idle;
    case RecordingFlowCommand::none:
        return fallbackState;
    }

    return fallbackState;
}

bool shouldRestoreKeyboardFocus(RecordingFlowCommand command) noexcept {
    return command != RecordingFlowCommand::none;
}

} // namespace devpiano::recording

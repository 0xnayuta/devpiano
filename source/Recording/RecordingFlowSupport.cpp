#include "RecordingFlowSupport.h"

namespace devpiano::recording
{

RecordingFlowCommand chooseRecordingFlowCommand(RecordingFlowIntent intent,
                                               RecordingFlowStatus status) noexcept
{
    switch (intent)
    {
        case RecordingFlowIntent::record:
            return status.currentState == RecordingFlowState::idle ? RecordingFlowCommand::startRecording
                                                                   : RecordingFlowCommand::none;

        case RecordingFlowIntent::play:
            return status.currentState == RecordingFlowState::idle && status.hasTake ? RecordingFlowCommand::startPlayback
                                                                                     : RecordingFlowCommand::none;

        case RecordingFlowIntent::stop:
            if (status.currentState == RecordingFlowState::recording)
                return RecordingFlowCommand::stopRecording;

            if (status.currentState == RecordingFlowState::playing)
                return RecordingFlowCommand::stopPlayback;

            return RecordingFlowCommand::none;
    }

    return RecordingFlowCommand::none;
}

RecordingFlowState getStateAfterCommand(RecordingFlowCommand command,
                                        RecordingFlowState fallbackState) noexcept
{
    switch (command)
    {
        case RecordingFlowCommand::startRecording: return RecordingFlowState::recording;
        case RecordingFlowCommand::startPlayback: return RecordingFlowState::playing;
        case RecordingFlowCommand::stopRecording:
        case RecordingFlowCommand::stopPlayback: return RecordingFlowState::idle;
        case RecordingFlowCommand::none: return fallbackState;
    }

    return fallbackState;
}

bool shouldRestoreKeyboardFocus(RecordingFlowCommand command) noexcept
{
    return command != RecordingFlowCommand::none;
}

} // namespace devpiano::recording

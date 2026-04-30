#pragma once

namespace devpiano::recording
{

enum class RecordingFlowState
{
    idle,
    recording,
    playing
};

enum class RecordingFlowIntent
{
    record,
    play,
    stop
};

enum class RecordingFlowCommand
{
    none,
    startRecording,
    startPlayback,
    stopRecording,
    stopPlayback
};

struct RecordingFlowStatus
{
    RecordingFlowState currentState = RecordingFlowState::idle;
    bool hasTake = false;
};

[[nodiscard]] RecordingFlowCommand chooseRecordingFlowCommand(RecordingFlowIntent intent,
                                                             RecordingFlowStatus status) noexcept;

[[nodiscard]] RecordingFlowState getStateAfterCommand(RecordingFlowCommand command,
                                                      RecordingFlowState fallbackState) noexcept;

[[nodiscard]] bool shouldRestoreKeyboardFocus(RecordingFlowCommand command) noexcept;

} // namespace devpiano::recording

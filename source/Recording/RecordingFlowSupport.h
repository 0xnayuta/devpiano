#pragma once

#include <cstdint>

namespace devpiano::recording {

enum class RecordingFlowState : std::uint8_t { idle, recording, recordingPaused, playing, playingPaused };

// playPause mirrors the industry-standard combined transport button: in an
// active playback/recording it pauses, in a paused state it resumes.
enum class RecordingFlowIntent : std::uint8_t { record, playPause, stop };

enum class RecordingFlowCommand : std::uint8_t {
    none,
    startRecording,
    startPlayback,
    pausePlayback,
    resumePlayback,
    pauseRecording,
    resumeRecording,
    stopRecording,
    stopPlayback
};

struct RecordingFlowStatus {
    RecordingFlowState currentState = RecordingFlowState::idle;
    bool hasTake = false;
};

[[nodiscard]] RecordingFlowCommand chooseRecordingFlowCommand(RecordingFlowIntent intent,
                                                              RecordingFlowStatus status) noexcept;

[[nodiscard]] RecordingFlowState getStateAfterCommand(RecordingFlowCommand command,
                                                      RecordingFlowState fallbackState) noexcept;

[[nodiscard]] bool shouldRestoreKeyboardFocus(RecordingFlowCommand command) noexcept;

} // namespace devpiano::recording

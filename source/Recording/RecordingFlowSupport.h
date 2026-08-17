#pragma once

#include <cstdint>

#include "UI/RecordingTypes.h"

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

// ---- UI state ↔ flow-state mapping ----
// Exposed for the RecordingSessionController and its unit tests: the transport
// buttons operate on ui::RecordingState while the flow state machine works on
// RecordingFlowState.  The two enums are structurally identical but kept
// separate so the UI layer never depends on the flow machinery directly.

[[nodiscard]] RecordingFlowState toRecordingFlowState(ui::RecordingState state) noexcept;

[[nodiscard]] ui::RecordingState toRecordingControlsState(RecordingFlowState state) noexcept;

[[nodiscard]] RecordingFlowStatus makeRecordingFlowStatus(ui::RecordingState state, bool hasTake) noexcept;

} // namespace devpiano::recording

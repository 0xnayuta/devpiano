#pragma once

#include <cstdint>

// ============================================================================
// Recording state enum & UI control state struct.
//
// Previously nested inside ControlsPanel; extracted so RecordingSessionController
// does not depend on the ControlsPanel Component class (deleted in Phase 11d).
// ============================================================================

enum class RecordingState : std::uint8_t { idle, recording, playing };

struct RecordingControlsState {
    RecordingState state = RecordingState::idle;
    bool hasTake = false;
    bool canExportMidiTake = false;
    bool canExportWavTake = false;
};

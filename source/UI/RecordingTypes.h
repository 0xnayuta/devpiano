#pragma once

#include <cstdint>

// ============================================================================
// 录制 UI 状态枚举与控制面板状态结构体。
// 置于 devpiano::ui 命名空间，与 devpiano::recording::RecordingState 解耦。
// ============================================================================

namespace devpiano::ui {

// 行业标准 transport 语义：播放/录制中可原位暂停（保留进度），
// Stop 完全停止并重置进度。
enum class RecordingState : std::uint8_t { idle, recording, recordingPaused, playing, playingPaused };

struct RecordingControlsState {
    RecordingState state = RecordingState::idle;
    bool hasTake = false;
    bool canExportMidiTake = false;
    bool canExportWavTake = false;
};

} // namespace devpiano::ui

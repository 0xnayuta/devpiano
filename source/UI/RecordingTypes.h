#pragma once

#include <cstdint>

// ============================================================================
// 录制 UI 状态枚举与控制面板状态结构体。
//
// 原为 ControlsPanel 内部类型；Phase 11d 删除 ControlsPanel 组件类后抽出，
// 使 RecordingSessionController 不再依赖该组件类。与 KeyboardTypes.h 等
// UI 状态类型一致，置于 devpiano::ui 命名空间，避免与
// devpiano::recording::RecordingState（RecordingEngine 状态机）同名冲突。
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

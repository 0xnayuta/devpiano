#pragma once

#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

#include "Export/WavExportOptions.h"

namespace devpiano::recording {
struct RecordingTake;
}

namespace devpiano::recording {

// ============================================================================
// 离线渲染公共管线（AUDIT-REC-007）
//
// WavFileExporter（fallback synth）与 PluginOfflineRenderer（VST3 插件）此前
// 各自重复定义 RenderEvent / buildRenderEvents / getScaledTakeLengthSamples /
// addPanicMidi / scaleTimestamp / hasUsableRenderOptions。此处统一为单一实现，
// 避免双份维护；两个渲染主循环因音频源不同（synth vs 插件实例）仍各自保留。
//
// 语义约定：
// - buildRenderEvents 输出按 timestampSamples 稳定排序，message 时间戳归一为 0。
// - getScaledTakeLengthSamples 取 take.lengthSamples 缩放值与最后一事件
//   timestamp+1 的较大者（保证最后事件完整落入渲染块）。
// ============================================================================

struct RenderEvent {
    juce::MidiMessage message;
    std::int64_t timestampSamples = 0;
};

// 按 ratio 缩放时间戳；非正值输入与负结果均归零（防御性）。
[[nodiscard]] std::int64_t scaleTimestamp(std::int64_t timestampSamples, double ratio) noexcept;

// WavExportOptions 四项基础参数有效性检查（sampleRate/channels/blockSize/bitsPerSample > 0）。
[[nodiscard]] bool hasUsableRenderOptions(const devpiano::exporting::WavExportOptions& options) noexcept;

// 将 RecordingTake 事件转为按目标采样率缩放的 RenderEvent 列表（稳定排序）。
[[nodiscard]] std::vector<RenderEvent> buildRenderEvents(const RecordingTake& take, double targetSampleRate);

// 渲染总时长（缩放后的 take 长度 vs 最后事件时间戳+1，取大者）。
[[nodiscard]] std::int64_t getScaledTakeLengthSamples(const RecordingTake& take, const std::vector<RenderEvent>& events,
                                                      double targetSampleRate) noexcept;

// 向 midiBuffer 注入全 16 通道 panic 控制器事件（CC64 延音 / CC120 全关 / all-notes-off）。
void addPanicMidi(juce::MidiBuffer& midiBuffer, int sampleOffset) noexcept;

} // namespace devpiano::recording

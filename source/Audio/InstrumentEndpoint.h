#pragma once

#include "Plugin/PluginHost.h"
#include <cstdint>
#include <juce_audio_processors/juce_audio_processors.h>

namespace devpiano::audio {

// 乐器端点（Instrument Endpoint）
// ============================================================================
// 宿主固定音频拓扑 Performance Input -> Instrument -> Master -> Output 中的
// "Instrument" 环节。内置全物理建模钢琴与托管 VST3 乐器共享这一端点职责
// （AGENTS.md 核心架构要求 9）：juce::AudioProcessor 只是 VST3 的适配器实现
// 细节，不反向污染宿主。
//
// 端点把"当前这块音频由谁发声"收敛成一次解析，取代散落在设备准备、实时渲染
// 与离线导出路径上重复出现的 hasLoadedPlugin() + getInstance() + isPrepared()
// 组合判断。
//
// 线程契约：resolveInstrumentEndpoint() 只做与既有 PluginHost 只读访问器相同
// 的无锁读取，可在实时音频线程调用；返回的裸指针仅在调用方自身持有的同步
// 窗口内有效（写侧仍由音频设备重建暂停保证）。
struct InstrumentEndpoint {
    enum class Kind : std::uint8_t {
        builtin, // 内置 Synthesiser（物理建模钢琴，sine 为回退音色）
        hostedPlugin, // 托管 VST3 乐器实例
    };

    Kind kind = Kind::builtin;
    juce::AudioPluginInstance* hostedInstance = nullptr;
    const juce::PluginDescription* hostedDescription = nullptr;
    // 实例已完成 prepareToPlay，可在实时回调中安全 processBlock。
    bool hostedInstanceReady = false;

    [[nodiscard]] bool isHostedPlugin() const noexcept {
        return kind == Kind::hostedPlugin && hostedInstance != nullptr;
    }

    // 端点是否已可渲染；未托管插件时内置合成器始终就绪。
    [[nodiscard]] bool isRenderable() const noexcept {
        return !isHostedPlugin() || hostedInstanceReady;
    }

    // 端点自身需要的通道数。缓冲区预分配余量由调用方叠加
    // （见 AudioEngine::prepareToPlay）。
    [[nodiscard]] int getChannelCount() const noexcept {
        if (!isHostedPlugin()) {
            return 2;
        }

        return juce::jmax(
            1, juce::jmax(hostedInstance->getTotalNumInputChannels(), hostedInstance->getTotalNumOutputChannels()));
    }
};

// 解析当前乐器端点：host 为空或未加载插件时返回内置端点。
[[nodiscard]] inline InstrumentEndpoint resolveInstrumentEndpoint(const PluginHost* host) noexcept {
    InstrumentEndpoint endpoint;

    if (host == nullptr || !host->hasLoadedPlugin()) {
        return endpoint;
    }

    auto* instance = host->getInstance();
    if (instance == nullptr) {
        return endpoint;
    }

    endpoint.kind = InstrumentEndpoint::Kind::hostedPlugin;
    endpoint.hostedInstance = instance;
    endpoint.hostedDescription = host->getLoadedPluginDescription();
    endpoint.hostedInstanceReady = host->isPrepared();
    return endpoint;
}

} // namespace devpiano::audio

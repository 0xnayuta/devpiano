#include <JuceHeader.h>

#include "Audio/InstrumentEndpoint.h"
#include "Export/ExportFlowSupport.h"
#include "Export/WavExportOptions.h"
#include "Plugin/PluginHost.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/RecordingEngine.h"
#include "TestHelpers.h"

// =============================================================================
// 乐器端点（InstrumentEndpoint）测试：
//   - 端点解析：空宿主 / 未加载插件时回落到内置端点
//   - 端点几何与就绪语义：通道数取自托管实例总线几何，就绪跟随 prepare 状态
//   - 离线端点路由：实例为空走内置渲染，实例可用走插件渲染
// =============================================================================

namespace {

juce::AudioChannelSet channelSetFor(int numChannels) {
    return numChannels <= 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
}

// 最小乐器插件桩：只暴露总线几何，用于验证端点通道数与就绪语义。
class StubInstrumentPlugin final : public juce::AudioPluginInstance {
public:
    explicit StubInstrumentPlugin(int numChannels)
        : juce::AudioPluginInstance(BusesProperties()
                                        .withInput("Input", channelSetFor(numChannels), true)
                                        .withOutput("Output", channelSetFor(numChannels), true)) {
    }

    const juce::String getName() const override {
        return "StubInstrumentPlugin";
    }

    void prepareToPlay(double, int) override {
    }
    void releaseResources() override {
    }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {
    }

    double getTailLengthSeconds() const override {
        return 0.0;
    }
    bool acceptsMidi() const override {
        return true;
    }
    bool producesMidi() const override {
        return false;
    }
    juce::AudioProcessorEditor* createEditor() override {
        return nullptr;
    }
    bool hasEditor() const override {
        return false;
    }
    int getNumPrograms() override {
        return 1;
    }
    int getCurrentProgram() override {
        return 0;
    }
    void setCurrentProgram(int) override {
    }
    const juce::String getProgramName(int) override {
        return {};
    }
    void changeProgramName(int, const juce::String&) override {
    }
    void getStateInformation(juce::MemoryBlock&) override {
    }
    void setStateInformation(const void*, int) override {
    }

    void fillInPluginDescription(juce::PluginDescription& description) const override {
        description.name = getName();
        description.pluginFormatName = "VST3";
        description.numInputChannels = getTotalNumInputChannels();
        description.numOutputChannels = getTotalNumOutputChannels();
    }
};

// 半秒单音 take：与离线渲染测试保持同一形状，便于两条路由都可产出真实 WAV。
devpiano::recording::RecordingTake makeRouterTestTake() {
    devpiano::recording::RecordingTake take;
    take.sampleRate = 44100.0;
    take.lengthSamples = 44100;

    devpiano::recording::PerformanceEvent noteOn;
    noteOn.timestampSamples = 0;
    noteOn.type = devpiano::recording::PerformanceEventType::midi;
    noteOn.source = devpiano::recording::RecordingEventSource::computerKeyboard;
    noteOn.message = juce::MidiMessage::noteOn(1, 60, 0.8f);

    devpiano::recording::PerformanceEvent noteOff;
    noteOff.timestampSamples = 22050;
    noteOff.type = devpiano::recording::PerformanceEventType::midi;
    noteOff.source = devpiano::recording::RecordingEventSource::computerKeyboard;
    noteOff.message = juce::MidiMessage::noteOff(1, 60, 0.0f);

    take.events = { noteOn, noteOff };
    return take;
}

} // namespace

class InstrumentEndpointTest final : public juce::UnitTest {
public:
    InstrumentEndpointTest()
        : juce::UnitTest("InstrumentEndpoint: resolution and offline routing", "DevPiano/Engine") {
    }

    void runTest() override {
        testResolutionFallsBackToBuiltin();
        testGeometryAndReadinessFollowTheInstance();
        testOfflineRoutingFollowsEndpointKind();
    }

private:
    void testResolutionFallsBackToBuiltin() {
        beginTest("null host and plugin-less host resolve to the builtin endpoint");
        {
            const auto detached = devpiano::audio::resolveInstrumentEndpoint(nullptr);
            expect(detached.kind == devpiano::audio::InstrumentEndpoint::Kind::builtin,
                   "no host means the builtin endpoint");
            expect(!detached.isHostedPlugin(), "no host must never report a hosted plugin");
            expect(detached.isRenderable(), "the builtin endpoint is always renderable");
            expectEquals(detached.getChannelCount(), 2);
            expect(detached.hostedInstance == nullptr);
            expect(detached.hostedDescription == nullptr);
        }

        {
            PluginHost host;
            const auto endpoint = devpiano::audio::resolveInstrumentEndpoint(&host);
            expect(!endpoint.isHostedPlugin(), "a host without a loaded plugin must stay on the builtin endpoint");
            expect(endpoint.isRenderable());
            expectEquals(endpoint.getChannelCount(), 2);
        }
    }

    void testGeometryAndReadinessFollowTheInstance() {
        beginTest("hosted endpoint geometry and readiness follow the plugin instance");
        {
            StubInstrumentPlugin stereoPlugin(2);
            devpiano::audio::InstrumentEndpoint endpoint;
            endpoint.kind = devpiano::audio::InstrumentEndpoint::Kind::hostedPlugin;
            endpoint.hostedInstance = &stereoPlugin;
            endpoint.hostedInstanceReady = false;

            expect(endpoint.isHostedPlugin());
            expect(!endpoint.isRenderable(), "a hosted endpoint is not renderable before prepareToPlay");
            expectEquals(endpoint.getChannelCount(), 2, "stereo plugin geometry");

            endpoint.hostedInstanceReady = true;
            expect(endpoint.isRenderable(), "a prepared hosted endpoint is renderable");

            StubInstrumentPlugin monoPlugin(1);
            endpoint.hostedInstance = &monoPlugin;
            expectEquals(endpoint.getChannelCount(), 1, "mono plugin geometry must be honoured");

            // 防御：种类声明为托管但实例缺失时必须回落到内置语义。
            endpoint.hostedInstance = nullptr;
            expect(!endpoint.isHostedPlugin(), "a hosted endpoint without an instance is not hosted");
            expect(endpoint.isRenderable(), "the fallback endpoint is still renderable");
            expectEquals(endpoint.getChannelCount(), 2);
        }
    }

    void testOfflineRoutingFollowsEndpointKind() {
        beginTest("offline routing renders through the builtin endpoint when no instance is supplied");
        {
            devpiano::test::ScopedTempDir tempDir("instrument-endpoint");

            devpiano::exporting::WavExportOptions options;
            options.sampleRate = 44100.0;
            options.numChannels = 2;
            options.blockSize = 512;
            options.bitsPerSample = 16;
            options.masterGain = 1.0f;

            const auto take = makeRouterTestTake();

            const auto builtinFile = tempDir.getChildFile("builtin.wav");
            expect(devpiano::exporting::renderTakeThroughInstrumentEndpoint(take, builtinFile, options, nullptr),
                   "a null instance must route to the builtin piano renderer");
            expect(builtinFile.existsAsFile(), "builtin routing must produce a WAV");

            StubInstrumentPlugin plugin(2);
            const auto pluginFile = tempDir.getChildFile("plugin.wav");
            expect(devpiano::exporting::renderTakeThroughInstrumentEndpoint(take, pluginFile, options, &plugin),
                   "a supplied instance must route to the plugin renderer");
            expect(pluginFile.existsAsFile(), "plugin routing must produce a WAV");

            // 空 take 在两条路由上都必须被拒绝，而不是静默产出空文件。
            devpiano::recording::RecordingTake emptyTake;
            const auto rejectedFile = tempDir.getChildFile("rejected.wav");
            expect(!devpiano::exporting::renderTakeThroughInstrumentEndpoint(emptyTake, rejectedFile, options, nullptr),
                   "empty take must be rejected on the builtin route");
            expect(!devpiano::exporting::renderTakeThroughInstrumentEndpoint(emptyTake, rejectedFile, options, &plugin),
                   "empty take must be rejected on the plugin route");
        }
    }
};

static InstrumentEndpointTest instrumentEndpointTest;

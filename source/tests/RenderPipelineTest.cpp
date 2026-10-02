#include <JuceHeader.h>

#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"

// =============================================================================
// Tests for the shared offline render pipeline (AUDIT-REC-007): event
// timestamp scaling / sorting, scaled take-length computation, and panic
// MIDI injection shared by WavFileExporter and PluginOfflineRenderer.
// =============================================================================

namespace {

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — 测试辅助，调用处字面量语义清晰
devpiano::recording::RecordingTake makeTake(double sampleRate, std::int64_t lengthSamples,
                                            std::vector<devpiano::recording::PerformanceEvent> events) {
    devpiano::recording::RecordingTake take;
    take.sampleRate = sampleRate;
    take.lengthSamples = lengthSamples;
    take.events = std::move(events);
    return take;
}

devpiano::recording::PerformanceEvent makeEvent(std::int64_t timestampSamples) {
    devpiano::recording::PerformanceEvent event;
    event.timestampSamples = timestampSamples;
    event.type = devpiano::recording::PerformanceEventType::midi;
    event.source = devpiano::recording::RecordingEventSource::computerKeyboard;
    event.message = juce::MidiMessage::noteOn(1, 60, 0.8f);
    return event;
}

} // namespace

class RenderPipelineTest : public juce::UnitTest {
public:
    RenderPipelineTest()
        : juce::UnitTest("RenderPipeline", "DevPiano/Recording") {
    }

    void runTest() override {
        using namespace devpiano::recording;

        testCase("render timeline preserves scaled duration and includes the final event", [&] {
            const auto take = makeTake(44100.0, 44100, { makeEvent(0), makeEvent(22050), makeEvent(44100) });
            const auto timeline = prepareRenderTimeline(take, 88200.0, 2.0);
            expect(timeline.has_value());
            if (!timeline.has_value()) {
                return;
            }
            expectEquals(timeline->events[1].timestampSamples, std::int64_t { 44100 });
            expectEquals(timeline->events[2].timestampSamples, std::int64_t { 88200 });
            expectEquals(timeline->takeLengthSamples, std::int64_t { 88201 });
            expectEquals(timeline->totalSamples, std::int64_t { 264601 });
        });

        testCase("render timeline retains silence and stable simultaneous MIDI order", [&] {
            auto first = makeEvent(100);
            first.message = juce::MidiMessage::noteOff(1, 60);
            auto second = makeEvent(100);
            const auto take = makeTake(44100.0, 44100, { makeEvent(200), first, second });
            const auto timeline = prepareRenderTimeline(take, 44100.0, 0.0);
            expect(timeline.has_value());
            if (!timeline.has_value()) {
                return;
            }
            expect(timeline->events[0].message.isNoteOff());
            expect(timeline->events[1].message.isNoteOn());
            expectEquals(timeline->events[2].timestampSamples, std::int64_t { 200 });
            expectEquals(timeline->totalSamples, std::int64_t { 44100 });
        });

        testCase("unrepresentable final event and tail reject rather than wrap", [&] {
            constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
            expect(!prepareRenderTimeline(makeTake(44100.0, maximum, { makeEvent(maximum) }), 44100.0, 0.0));
            expect(!prepareRenderTimeline(makeTake(44100.0, maximum - 88199, { makeEvent(0) }), 44100.0, 2.0));
            const auto boundary
                = prepareRenderTimeline(makeTake(44100.0, maximum - 88200, { makeEvent(0) }), 44100.0, 2.0);
            expect(boundary.has_value());
            if (boundary.has_value()) {
                expectEquals(boundary->totalSamples, maximum);
            }
            expect(!prepareRenderTimeline(makeTake(44100.0, maximum / 2 + 1, { makeEvent(0) }), 88200.0, 0.0));
        });

        testCase("invalid source target or event domain cannot create a render timeline", [&] {
            for (const auto rate :
                 { 1e-300, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
                expect(!prepareRenderTimeline(makeTake(rate, 1000, { makeEvent(0) }), 44100.0, 2.0));
                expect(!prepareRenderTimeline(makeTake(44100.0, 1000, { makeEvent(0) }), rate, 2.0));
            }
            expect(!prepareRenderTimeline(makeTake(44100.0, -1, { makeEvent(0) }), 44100.0, 2.0));
            expect(!prepareRenderTimeline(makeTake(44100.0, 1000, { makeEvent(-1) }), 44100.0, 2.0));
            expect(!prepareRenderTimeline(makeTake(44100.0, 1000, { makeEvent(1001) }), 44100.0, 2.0));
            expect(!prepareRenderTimeline(makeTake(44100.0, 1000, { makeEvent(0) }), 44100.0, -1.0));
        });

        testCase("addPanicMidi injects 16 channels x 3 controllers at the given offset", [&] {
            juce::MidiBuffer midiBuffer;
            addPanicMidi(midiBuffer, 42);
            expectEquals(midiBuffer.getNumEvents(), 48);

            auto eventCount = 0;
            auto sawSustainRelease = false;
            auto sawAllControllersOff = false;
            auto sawAllNotesOff = false;
            for (const auto metadata : midiBuffer) {
                ++eventCount;
                expectEquals(metadata.samplePosition, 42);
                const auto message = metadata.getMessage();
                if (message.isController() && message.getControllerNumber() == 64
                    && message.getControllerValue() == 0) {
                    sawSustainRelease = true;
                }
                if (message.isController() && message.getControllerNumber() == 120
                    && message.getControllerValue() == 0) {
                    sawAllControllersOff = true;
                }
                if (message.isAllNotesOff()) {
                    sawAllNotesOff = true;
                }
            }
            expectEquals(eventCount, 48);
            expect(sawSustainRelease);
            expect(sawAllControllersOff);
            expect(sawAllNotesOff);
        });
    }
};

static RenderPipelineTest renderPipelineTest;

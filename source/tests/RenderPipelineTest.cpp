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

        testCase("scaleTimestamp clamps non-positive inputs to zero", [&] {
            expect(scaleTimestamp(0, 1.0) == 0);
            expect(scaleTimestamp(-100, 1.0) == 0);
            expect(scaleTimestamp(1000, 0.0) == 0); // zero ratio -> zero
            expect(scaleTimestamp(1000, -2.0) == 0); // negative ratio -> clamped
        });

        testCase("scaleTimestamp scales by the sample-rate ratio", [&] {
            expect(scaleTimestamp(44100, 1.0) == 44100);
            expect(scaleTimestamp(44100, 2.0) == 88200);
            expect(scaleTimestamp(44100, 0.5) == 22050);
            // Rounds to nearest sample.
            expect(scaleTimestamp(1, 1.5) == 2);
        });

        testCase("buildRenderEvents rescales timestamps to target rate", [&] {
            auto take = makeTake(44100.0, 44100, { makeEvent(0), makeEvent(22050), makeEvent(44100) });
            const auto events = buildRenderEvents(take, 88200.0); // 2x ratio
            expectEquals(static_cast<int>(events.size()), 3);
            expect(events[0].timestampSamples == 0);
            expect(events[1].timestampSamples == 44100);
            expect(events[2].timestampSamples == 88200);
        });

        testCase("buildRenderEvents sorts events stably by timestamp", [&] {
            auto take = makeTake(44100.0, 100, { makeEvent(300), makeEvent(100), makeEvent(200) });
            const auto events = buildRenderEvents(take, 44100.0);
            expectEquals(static_cast<int>(events.size()), 3);
            expect(events[0].timestampSamples == 100);
            expect(events[1].timestampSamples == 200);
            expect(events[2].timestampSamples == 300);
        });

        testCase("buildRenderEvents normalises message timestamps to zero", [&] {
            auto take = makeTake(44100.0, 100, { makeEvent(50) });
            const auto events = buildRenderEvents(take, 44100.0);
            expectEquals(static_cast<int>(events.size()), 1);
            expect(events[0].message.getTimeStamp() == 0.0);
        });

        testCase("buildRenderEvents returns empty for an empty take", [&] {
            auto take = makeTake(44100.0, 0, {});
            const auto events = buildRenderEvents(take, 44100.0);
            expect(events.empty());
        });

        testCase("getScaledTakeLengthSamples honours take length when it is the longer end", [&] {
            auto take = makeTake(44100.0, 44100, { makeEvent(100) });
            const auto events = buildRenderEvents(take, 88200.0);
            expect(getScaledTakeLengthSamples(take, events, 88200.0) == 88200);
        });

        testCase("getScaledTakeLengthSamples extends past the last event plus one sample", [&] {
            auto take = makeTake(44100.0, 100, { makeEvent(44100) });
            const auto events = buildRenderEvents(take, 44100.0);
            // Last event at 44100 -> length must cover timestamp+1 so the event
            // falls fully inside the rendered range.
            expect(getScaledTakeLengthSamples(take, events, 44100.0) == 44101);
        });

        testCase("getScaledTakeLengthSamples falls back to scaled take length when events are empty", [&] {
            auto take = makeTake(44100.0, 88200, {});
            const auto events = buildRenderEvents(take, 88200.0);
            expect(getScaledTakeLengthSamples(take, events, 88200.0) == 176400);
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

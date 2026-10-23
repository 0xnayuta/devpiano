#include <JuceHeader.h>

#include "Export/ExportFlowSupport.h"
#include "Export/WavExportOptions.h"
#include "Export/WavExportTask.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"
#include "Recording/WavFileExporter.h"
#include "TestHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

// Permanent E3 regressions for the unified finite export tail.
namespace {

constexpr auto kRate = 48000.0;
constexpr int kBlock = 512;

struct TestEvent {
    std::int64_t sample = 0;
    int channel = 1;
    int number = 60;
    float velocity = 0.75f;
    bool noteOff = false;
};

devpiano::recording::RecordingTake makeTake(std::int64_t length, const std::vector<TestEvent>& events) {
    devpiano::recording::RecordingTake take;
    take.sampleRate = kRate;
    take.lengthSamples = length;
    for (const auto& spec : events) {
        devpiano::recording::PerformanceEvent event;
        event.timestampSamples = spec.sample;
        event.type = devpiano::recording::PerformanceEventType::midi;
        event.source = devpiano::recording::RecordingEventSource::playback;
        event.message = spec.noteOff ? juce::MidiMessage::noteOff(spec.channel, spec.number, 0.0f)
                                     : juce::MidiMessage::noteOn(spec.channel, spec.number, spec.velocity);
        take.events.push_back(event);
    }
    return take;
}

devpiano::exporting::WavExportOptions makeOptions(int blockSize = kBlock) {
    devpiano::exporting::WavExportOptions options;
    options.sampleRate = kRate;
    options.numChannels = 2;
    options.blockSize = blockSize;
    options.bitsPerSample = 24;
    options.masterGain = 1.0f;
    options.adsr = { 0.01f, 0.20f, 0.80f, 0.30f };
    options.reverbWet = 0.0f;
    return options;
}

bool readWav(const juce::File& file, juce::AudioBuffer<float>& buffer, double& sampleRate) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr) {
        return false;
    }
    sampleRate = reader->sampleRate;
    buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    return reader->read(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), 0, buffer.getNumSamples());
}

float maximumDifference(const juce::AudioBuffer<float>& left, const juce::AudioBuffer<float>& right, int frames) {
    auto difference = 0.0f;
    for (int channel = 0; channel < left.getNumChannels(); ++channel) {
        for (int sample = 0; sample < frames; ++sample) {
            difference
                = std::max(difference, std::abs(left.getSample(channel, sample) - right.getSample(channel, sample)));
        }
    }
    return difference;
}

float rms(const juce::AudioBuffer<float>& buffer, int start, int end) {
    if (end <= start) {
        return 0.0f;
    }
    double sum = 0.0;
    std::int64_t count = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        for (int sample = start; sample < end; ++sample) {
            const auto value = static_cast<double>(buffer.getSample(channel, sample));
            sum += value * value;
            ++count;
        }
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

class TailProbePlugin final : public juce::AudioPluginInstance {
public:
    TailProbePlugin()
        : AudioPluginInstance(BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    }

    const juce::String getName() const override {
        return "E3TailProbePlugin";
    }
    void prepareToPlay(double newRate, int) override {
        rate = newRate;
        active = false;
        releaseRemaining = 0;
        totalRelease = rate * 4.0;
        noteOnCount = 0;
        noteOffCount = 0;
        controllerCount = 0;
    }
    void releaseResources() override {
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        auto event = midiMessages.begin();
        const auto end = midiMessages.end();
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            while (event != end && (*event).samplePosition <= sample) {
                consume((*event).getMessage());
                ++event;
            }
            auto value = 0.0f;
            if (active) {
                value = 0.25f;
            } else if (releaseRemaining > 0.0) {
                value = static_cast<float>(0.25 * releaseRemaining / totalRelease);
                releaseRemaining -= 1.0;
            }
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
                buffer.setSample(channel, sample, value);
            }
        }
        while (event != end) {
            consume((*event).getMessage());
            ++event;
        }
    }
    double getTailLengthSeconds() const override {
        return 1.0;
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
        description.numInputChannels = 2;
        description.numOutputChannels = 2;
    }

    int noteOnCount = 0;
    int noteOffCount = 0;
    int controllerCount = 0;

private:
    void consume(const juce::MidiMessage& message) {
        if (message.isNoteOn()) {
            active = true;
            releaseRemaining = 0.0;
            ++noteOnCount;
        } else if (message.isNoteOff()) {
            active = false;
            releaseRemaining = totalRelease;
            ++noteOffCount;
        } else if (message.isController()) {
            ++controllerCount;
        } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
            active = false;
            releaseRemaining = 0.0;
        }
    }

    double rate = kRate;
    double totalRelease = 0.0;
    double releaseRemaining = 0.0;
    bool active = false;
};

class ExportTailDurationTest final : public juce::UnitTest {
public:
    ExportTailDurationTest()
        : juce::UnitTest("Export: finite tail duration", "DevPiano/Recording") {
    }

    void runTest() override {
        testOptionBoundsAndDefaults();
        testTimelineLengths();
        testDefaultProductionPaths();
        testExplicitTwoAndPrefix();
        testZeroTailBoundary();
        testBlockIndependence();
        testDoubleLayerUsesSameTail();
        testInvalidOptionsPreserveTarget();
        testTaskPassesTail();
        testCancelDuringTail();
    }

private:
    void testOptionBoundsAndDefaults() {
        beginTest("tailSeconds defaults to six seconds and accepts the closed range");
        devpiano::exporting::WavExportOptions options;
        expectWithinAbsoluteError(options.tailSeconds, devpiano::exporting::kDefaultWavTailSeconds, 1.0e-12);
        expectWithinAbsoluteError(options.tailSeconds, 6.0, 1.0e-12);

        for (const auto value : { 0.0, 2.0, 30.0 }) {
            options.tailSeconds = value;
            expect(devpiano::recording::hasUsableRenderOptions(options), "valid tail must be accepted");
        }
        for (const auto value : { -1.0, 30.0001, std::numeric_limits<double>::quiet_NaN(),
                                  std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() }) {
            options.tailSeconds = value;
            expect(!devpiano::recording::hasUsableRenderOptions(options), "invalid tail must be rejected");
        }

        const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false } });
        SettingsModel::PerformanceSettingsView performance;
        const auto built = devpiano::exporting::buildWavExportOptions(take, performance, kRate, kBlock);
        expectWithinAbsoluteError(built.tailSeconds, 6.0, 1.0e-12, "builder must retain default tail");
    }

    void testTimelineLengths() {
        beginTest("timeline computes exact tail samples, ceil, and final-event +1");
        auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false }, { 1000, 1, 60, 0.0f, true } });
        auto timeline = devpiano::recording::prepareRenderTimeline(take, kRate, 2.5);
        expect(timeline.has_value());
        if (timeline.has_value()) {
            expectEquals(timeline->takeLengthSamples, std::int64_t { 1001 });
            expectEquals(timeline->totalSamples, std::int64_t { 1001 + 120000 });
        }

        timeline = devpiano::recording::prepareRenderTimeline(take, kRate, 2.00001);
        expect(timeline.has_value());
        if (timeline.has_value()) {
            expectEquals(timeline->totalSamples, std::int64_t { 1001 + 96001 });
        }

        timeline = devpiano::recording::prepareRenderTimeline(take, kRate, 0.0);
        expect(timeline.has_value());
        if (timeline.has_value()) {
            expectEquals(timeline->takeLengthSamples, std::int64_t { 1001 });
            expectEquals(timeline->totalSamples, std::int64_t { 1001 });
        }
    }

    void testDefaultProductionPaths() {
        beginTest("built-in and plugin exporters use the default six-second tail");
        devpiano::test::ScopedTempDir tempDir("e3-default-tail");
        const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false }, { 1000, 1, 60, 0.0f, true } });
        const auto options = makeOptions();
        const auto expected = std::int64_t { 1001 + 288000 };

        const auto builtInFile = tempDir.getChildFile("builtin-six.wav");
        TailProbePlugin plugin;
        const auto pluginFile = tempDir.getChildFile("plugin-six.wav");
        expect(devpiano::exporting::exportTakeAsWavFile(take, builtInFile, options));
        expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, pluginFile, options, plugin));

        juce::AudioBuffer<float> builtInAudio;
        juce::AudioBuffer<float> pluginAudio;
        double builtInRate = 0.0;
        double pluginRate = 0.0;
        expect(readWav(builtInFile, builtInAudio, builtInRate));
        expect(readWav(pluginFile, pluginAudio, pluginRate));
        expectWithinAbsoluteError(builtInRate, kRate, 0.001);
        expectWithinAbsoluteError(pluginRate, kRate, 0.001);
        expectEquals(static_cast<std::int64_t>(builtInAudio.getNumSamples()), expected);
        expectEquals(static_cast<std::int64_t>(pluginAudio.getNumSamples()), expected);
        expectEquals(plugin.noteOnCount, 1);
        expectEquals(plugin.noteOffCount, 1);
        expectEquals(plugin.controllerCount, 48);
    }

    void testExplicitTwoAndPrefix() {
        beginTest("two- and six-second exports share an identical audio prefix");
        devpiano::test::ScopedTempDir tempDir("e3-prefix");
        const auto take = makeTake(48000, { { 0, 1, 60, 0.5f, false }, { 24000, 1, 60, 0.0f, true } });
        auto twoOptions = makeOptions();
        twoOptions.tailSeconds = 2.0;
        twoOptions.adsr.release = 3.0f;
        twoOptions.reverbSpace = devpiano::audio::ReverbSpace::concertHall;
        twoOptions.reverbWet = 0.35f;
        auto sixOptions = twoOptions;
        sixOptions.tailSeconds = 6.0;

        const auto twoFile = tempDir.getChildFile("room-two.wav");
        const auto sixFile = tempDir.getChildFile("room-six.wav");
        expect(devpiano::exporting::exportTakeAsWavFile(take, twoFile, twoOptions));
        expect(devpiano::exporting::exportTakeAsWavFile(take, sixFile, sixOptions));

        juce::AudioBuffer<float> twoAudio;
        juce::AudioBuffer<float> sixAudio;
        double twoRate = 0.0;
        double sixRate = 0.0;
        expect(readWav(twoFile, twoAudio, twoRate));
        expect(readWav(sixFile, sixAudio, sixRate));
        expectEquals(static_cast<std::int64_t>(twoAudio.getNumSamples()), std::int64_t { 48000 + 96000 });
        expectEquals(static_cast<std::int64_t>(sixAudio.getNumSamples()), std::int64_t { 48000 + 288000 });
        expectLessThan(maximumDifference(twoAudio, sixAudio, twoAudio.getNumSamples()), 1.0e-6f,
                       "shared 2 s tail prefix must remain sample-identical");
        expectGreaterThan(rms(sixAudio, twoAudio.getNumSamples(), sixAudio.getNumSamples()), 1.0e-4f,
                          "six-second version must contain additional audible Room/release output");
    }

    void testZeroTailBoundary() {
        beginTest("zero tail keeps take length and does not synthesize an end release block");
        {
            devpiano::test::ScopedTempDir tempDir("e3-zero-tail-held");
            const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false } });
            auto options = makeOptions();
            options.tailSeconds = 0.0;
            TailProbePlugin plugin;
            const auto file = tempDir.getChildFile("held-zero.wav");
            expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, file, options, plugin));
            expectEquals(plugin.noteOnCount, 1);
            expectEquals(plugin.noteOffCount, 0);
            expectEquals(plugin.controllerCount, 0);
            juce::AudioBuffer<float> audio;
            double rate = 0.0;
            expect(readWav(file, audio, rate));
            expectEquals(static_cast<std::int64_t>(audio.getNumSamples()), std::int64_t { 1000 });
        }
        {
            devpiano::test::ScopedTempDir tempDir("e3-zero-tail-final-event");
            const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false }, { 1000, 1, 60, 0.0f, true } });
            auto options = makeOptions();
            options.tailSeconds = 0.0;
            TailProbePlugin plugin;
            const auto file = tempDir.getChildFile("final-event-zero.wav");
            expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, file, options, plugin));
            expectEquals(plugin.noteOnCount, 1);
            expectEquals(plugin.noteOffCount, 1);
            expectEquals(plugin.controllerCount, 0);
            juce::AudioBuffer<float> audio;
            double rate = 0.0;
            expect(readWav(file, audio, rate));
            expectEquals(static_cast<std::int64_t>(audio.getNumSamples()), std::int64_t { 1001 });
        }
    }

    void testBlockIndependence() {
        beginTest("tail file length is independent of blockSize");
        devpiano::test::ScopedTempDir tempDir("e3-block-independence");
        const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false }, { 1000, 1, 60, 0.0f, true } });
        const auto expected = std::int64_t { 1001 + 288000 };
        for (const auto block : { 512, 257 }) {
            auto options = makeOptions(block);
            TailProbePlugin plugin;
            const auto file = tempDir.getChildFile("block-" + juce::String(block) + ".wav");
            expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, file, options, plugin));
            juce::AudioBuffer<float> audio;
            double rate = 0.0;
            expect(readWav(file, audio, rate));
            expectEquals(static_cast<std::int64_t>(audio.getNumSamples()), expected);
        }
    }

    void testDoubleLayerUsesSameTail() {
        beginTest("dual-layer export uses the same six-second option");
        devpiano::test::ScopedTempDir tempDir("e3-dual-tail");
        const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false }, { 1000, 1, 60, 0.0f, true } });
        auto options = makeOptions();
        options.layers.enabled = true;
        options.layers.pianoEnabled = true;
        options.layers.pluginEnabled = true;
        TailProbePlugin plugin;
        const auto file = tempDir.getChildFile("dual-six.wav");
        expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, file, options, plugin));
        juce::AudioBuffer<float> audio;
        double rate = 0.0;
        expect(readWav(file, audio, rate));
        expectEquals(static_cast<std::int64_t>(audio.getNumSamples()), std::int64_t { 1001 + 288000 });
    }

    void testInvalidOptionsPreserveTarget() {
        beginTest("invalid tailSeconds rejects before replacing an existing target");
        devpiano::test::ScopedTempDir tempDir("e3-invalid-tail");
        const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false }, { 1000, 1, 60, 0.0f, true } });
        auto options = makeOptions();
        options.tailSeconds = -0.1;
        const auto target = tempDir.getChildFile("target.wav");
        expect(target.replaceWithText("retained target"));
        const auto original = target.loadFileAsString();
        expect(!devpiano::exporting::exportTakeAsWavFile(take, target, options));
        expectEquals(target.loadFileAsString(), original);

        TailProbePlugin plugin;
        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(take, target, options, plugin));
        expectEquals(target.loadFileAsString(), original);
    }

    void testTaskPassesTail() {
        beginTest("WavExportTask copies and passes the six-second option");
        devpiano::test::ScopedTempDir tempDir("e3-task-tail");
        const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false }, { 1000, 1, 60, 0.0f, true } });
        const auto options = makeOptions();
        const auto file = tempDir.getChildFile("task-six.wav");
        WavExportTask task(take, file, options);
        expect(task.runSync());
        juce::AudioBuffer<float> audio;
        double rate = 0.0;
        expect(readWav(file, audio, rate));
        expectEquals(static_cast<std::int64_t>(audio.getNumSamples()), std::int64_t { 1001 + 288000 });
    }

    void testCancelDuringTail() {
        beginTest("cancellation during the tail preserves the existing target");
        devpiano::test::ScopedTempDir tempDir("e3-tail-cancel");
        const auto take = makeTake(1000, { { 0, 1, 60, 0.5f, false }, { 1000, 1, 60, 0.0f, true } });
        const auto options = makeOptions();
        const auto target = tempDir.getChildFile("cancel-target.wav");
        expect(target.replaceWithText("retained cancel target"));
        const auto original = target.loadFileAsString();
        auto observedTail = false;
        expect(!devpiano::exporting::exportTakeAsWavFile(take, target, options, [&observedTail](double progress) {
            if (progress > 0.1) {
                observedTail = true;
                return false;
            }
            return true;
        }));
        expect(observedTail, "cancellation must be checked after the take end");
        expectEquals(target.loadFileAsString(), original);
    }
};

ExportTailDurationTest exportTailDurationTest;

} // namespace

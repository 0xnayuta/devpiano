#include <JuceHeader.h>

#include "Export/ExportFlowSupport.h"
#include "Export/WavExportOptions.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/RecordingEngine.h"
#include "TestHelpers.h"

// =============================================================================
// Unit tests for PluginOfflineRenderer (TEST-003):
// - Parameter validation and error rejection
// - Offline rendering execution with audio generation and WAV verification
// - Cancellation handling via progress callback
// - snapshotPluginState state capture verification
// =============================================================================

namespace {

class DummyOfflineTestPlugin final : public juce::AudioPluginInstance {
public:
    DummyOfflineTestPlugin()
        : AudioPluginInstance(BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    }

    const juce::String getName() const override {
        return "DummyOfflineTestPlugin";
    }

    void prepareToPlay(double, int) override {
    }
    void releaseResources() override {
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        for (const auto metadata : midiMessages) {
            const auto msg = metadata.getMessage();
            if (msg.isNoteOn()) {
                activeNote = msg.getNoteNumber();
            } else if ((msg.isNoteOff() && msg.getNoteNumber() == activeNote) || msg.isAllNotesOff()
                       || msg.isAllSoundOff()) {
                activeNote = -1;
            }
        }

        if (activeNote >= 0) {
            // Fill with DC offset or constant signal for easy detection
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                auto* writePtr = buffer.getWritePointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i) {
                    writePtr[i] += 0.5f;
                }
            }
        }
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

    void getStateInformation(juce::MemoryBlock& destData) override {
        const char dummyPayload[] = "DUMMY_PLUGIN_STATE_123";
        destData.replaceAll(dummyPayload, sizeof(dummyPayload));
    }

    void setStateInformation(const void*, int) override {
    }

    void fillInPluginDescription(juce::PluginDescription& desc) const override {
        desc.name = getName();
        desc.pluginFormatName = "VST3";
        desc.numInputChannels = 2;
        desc.numOutputChannels = 2;
    }

private:
    int activeNote = -1;
};

class DummyMonoOfflineTestPlugin final : public juce::AudioPluginInstance {
public:
    DummyMonoOfflineTestPlugin()
        : AudioPluginInstance(BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::mono(), true)
                                  .withOutput("Output", juce::AudioChannelSet::mono(), true)) {
    }

    const juce::String getName() const override {
        return "DummyMonoOfflineTestPlugin";
    }

    void prepareToPlay(double, int) override {
    }
    void releaseResources() override {
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        for (const auto metadata : midiMessages) {
            const auto msg = metadata.getMessage();
            if (msg.isNoteOn()) {
                activeNote = msg.getNoteNumber();
            } else if ((msg.isNoteOff() && msg.getNoteNumber() == activeNote) || msg.isAllNotesOff()
                       || msg.isAllSoundOff()) {
                activeNote = -1;
            }
        }

        if (activeNote >= 0) {
            auto* writePtr = buffer.getWritePointer(0);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                writePtr[i] += 0.6f;
            }
        }
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
    bool hasEditor() const override {
        return false;
    }
    juce::AudioProcessorEditor* createEditor() override {
        return nullptr;
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

    void fillInPluginDescription(juce::PluginDescription& desc) const override {
        desc.name = getName();
        desc.pluginFormatName = "VST3";
        desc.numInputChannels = 1;
        desc.numOutputChannels = 1;
    }

private:
    int activeNote = -1;
};

devpiano::recording::RecordingTake makeSimpleRenderTake() {
    devpiano::recording::RecordingTake take;
    take.sampleRate = 44100.0;
    take.lengthSamples = 44100; // 1 second

    devpiano::recording::PerformanceEvent noteOn;
    noteOn.timestampSamples = 0;
    noteOn.type = devpiano::recording::PerformanceEventType::midi;
    noteOn.source = devpiano::recording::RecordingEventSource::computerKeyboard;
    noteOn.message = juce::MidiMessage::noteOn(1, 60, 0.8f);

    devpiano::recording::PerformanceEvent noteOff;
    noteOff.timestampSamples = 22050; // 0.5s note
    noteOff.type = devpiano::recording::PerformanceEventType::midi;
    noteOff.source = devpiano::recording::RecordingEventSource::computerKeyboard;
    noteOff.message = juce::MidiMessage::noteOff(1, 60, 0.0f);

    take.events = { noteOn, noteOff };
    return take;
}

} // namespace

class PluginOfflineRendererTest final : public juce::UnitTest {
public:
    PluginOfflineRendererTest()
        : juce::UnitTest("PluginOfflineRenderer", "DevPiano/Export") {
    }

    void runTest() override {
        testParameterValidation();
        testOfflineRenderingExecution();
        testMonoPluginStereoDownmix();
        testMasterSoftLimiterBehavior();
        testProgressCancellation();
        testSnapshotPluginState();
    }
    void testParameterValidation() {
        beginTest("Parameter validation and error rejection");

        DummyOfflineTestPlugin plugin;
        devpiano::exporting::WavExportOptions validOptions;
        validOptions.sampleRate = 44100.0;
        validOptions.numChannels = 2;
        validOptions.blockSize = 512;
        validOptions.bitsPerSample = 16;

        devpiano::test::ScopedTempDir tempDir("offline-param-test");
        const auto validFile = tempDir.getChildFile("test.wav");

        // 1. Empty take
        devpiano::recording::RecordingTake emptyTake;
        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(emptyTake, validFile, validOptions, plugin));

        // 2. Invalid take sample rate
        auto invalidRateTake = makeSimpleRenderTake();
        invalidRateTake.sampleRate = 0.0;
        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(invalidRateTake, validFile, validOptions, plugin));

        // 3. Invalid export options
        auto invalidOptions = validOptions;
        invalidOptions.sampleRate = 0.0;
        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(makeSimpleRenderTake(), validFile, invalidOptions,
                                                                 plugin));

        // 4. Empty destination file
        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(makeSimpleRenderTake(), juce::File(), validOptions,
                                                                 plugin));
    }

    void testOfflineRenderingExecution() {
        beginTest("Offline rendering execution and WAV verification");

        DummyOfflineTestPlugin plugin;
        devpiano::exporting::WavExportOptions options;
        options.sampleRate = 44100.0;
        options.numChannels = 2;
        options.blockSize = 512;
        options.bitsPerSample = 16;
        options.masterGain = 1.0f;

        devpiano::test::ScopedTempDir tempDir("offline-render-exec");
        const auto outFile = tempDir.getChildFile("rendered_output.wav");

        const auto take = makeSimpleRenderTake();
        const bool success = devpiano::exporting::renderTakeWithOfflinePlugin(take, outFile, options, plugin);
        expect(success, "renderTakeWithOfflinePlugin must succeed with valid inputs");
        expect(outFile.existsAsFile(), "Output file must exist");
        expect(outFile.getSize() > 1024, "Output file size must be greater than header size");

        // Verify the created WAV file with AudioFormatReader
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatReader> reader(
            wavFormat.createReaderFor(outFile.createInputStream().release(), true));
        expect(reader != nullptr, "WAV reader must successfully open the generated file");
        if (reader != nullptr) {
            expectEquals(reader->sampleRate, 44100.0);
            expectEquals(static_cast<int>(reader->numChannels), 2);
            expectEquals(static_cast<int>(reader->bitsPerSample), 16);
            expect(reader->lengthInSamples > 44100, "Rendered WAV must include take content and tail");

            // Verify non-zero samples were generated
            juce::AudioBuffer<float> readBuffer(2, 512);
            reader->read(&readBuffer, 0, 512, 0, true, true);
            expect(readBuffer.getMagnitude(0, 0, 512) > 0.1f, "Generated WAV must contain audio data from plugin");
        }
    }

    void testProgressCancellation() {
        beginTest("Progress callback cancellation");

        DummyOfflineTestPlugin plugin;
        devpiano::exporting::WavExportOptions options;
        options.sampleRate = 44100.0;
        options.numChannels = 2;
        options.blockSize = 256;

        devpiano::test::ScopedTempDir tempDir("offline-cancel-test");
        const auto outFile = tempDir.getChildFile("cancel_output.wav");

        int progressCalls = 0;
        auto cancelCallback = [&progressCalls]([[maybe_unused]] double progress) {
            ++progressCalls;
            // Cancel after 2nd progress block
            return progressCalls < 2;
        };

        const auto take = makeSimpleRenderTake();
        const bool success
            = devpiano::exporting::renderTakeWithOfflinePlugin(take, outFile, options, plugin, cancelCallback);
        expect(!success, "Render must abort and return false when progressCallback returns false");
        expect(progressCalls >= 2, "Progress callback should have been invoked at least twice before aborting");
    }

    void testSnapshotPluginState() {
        beginTest("snapshotPluginState verification");

        DummyOfflineTestPlugin plugin;
        const auto state = devpiano::exporting::snapshotPluginState(plugin);
        expect(state.getSize() > 0, "Captured state must be non-empty");

        const juce::String text(static_cast<const char*>(state.getData()));
        expect(text.contains("DUMMY_PLUGIN_STATE"), "Captured memory block must match plugin state");
    }

    void testMonoPluginStereoDownmix() {
        beginTest("Mono plugin rendered to stereo output populates both channels (QUAL-004)");

        DummyMonoOfflineTestPlugin monoPlugin;
        devpiano::exporting::WavExportOptions options;
        options.sampleRate = 44100.0;
        options.numChannels = 2;
        options.blockSize = 512;
        options.bitsPerSample = 16;
        options.masterGain = 1.0f;

        devpiano::test::ScopedTempDir tempDir("offline-render-mono-stereo");
        const auto outFile = tempDir.getChildFile("mono_to_stereo.wav");

        const auto take = makeSimpleRenderTake();
        const bool success = devpiano::exporting::renderTakeWithOfflinePlugin(take, outFile, options, monoPlugin);
        expect(success, "renderTakeWithOfflinePlugin must succeed for mono plugin");

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatReader> reader(
            wavFormat.createReaderFor(outFile.createInputStream().release(), true));
        expect(reader != nullptr);
        if (reader != nullptr) {
            expectEquals(static_cast<int>(reader->numChannels), 2);
            juce::AudioBuffer<float> readBuffer(2, 512);
            reader->read(&readBuffer, 0, 512, 0, true, true);
            expect(readBuffer.getMagnitude(0, 0, 512) > 0.1f, "Left channel must have audio");
            expect(readBuffer.getMagnitude(1, 0, 512) > 0.1f,
                   "Right channel must have mirrored audio (not silent right channel)");
        }
    }

    void testMasterSoftLimiterBehavior() {
        beginTest("applyMasterSoftLimiter soft-knees peaks above threshold (QUAL-004)");

        juce::AudioBuffer<float> testBuffer(2, 4);
        testBuffer.setSample(0, 0, 0.5f); // below 0.85 threshold
        testBuffer.setSample(0, 1, 1.2f); // above threshold
        testBuffer.setSample(0, 2, -1.5f); // negative peak
        testBuffer.setSample(0, 3, 2.5f); // extreme peak

        devpiano::exporting::applyMasterSoftLimiter(testBuffer, 4);

        expectWithinAbsoluteError(testBuffer.getSample(0, 0), 0.5f, 0.0001f, "Below threshold untouched");
        expect(testBuffer.getSample(0, 1) < 0.98f && testBuffer.getSample(0, 1) > 0.85f, "Limited within ceiling");
        expect(testBuffer.getSample(0, 2) > -0.98f && testBuffer.getSample(0, 2) < -0.85f,
               "Negative peak limited within ceiling");
        expect(testBuffer.getSample(0, 3) <= 0.98f, "Extreme peak stays <= 0.98 ceiling");
    }
};

static PluginOfflineRendererTest pluginOfflineRendererTest;

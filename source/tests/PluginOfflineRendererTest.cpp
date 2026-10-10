#include <JuceHeader.h>

#include "Audio/BoundedDelayLine.h"
#include "Audio/RoomReverbEngine.h"
#include "Export/ExportFlowSupport.h"
#include "Export/WavExportOptions.h"
#include "Recording/PluginOfflineRenderer.h"
#include "Recording/RecordingEngine.h"
#include "Recording/WavFileExporter.h"
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

class DummyLatencyOfflineTestPlugin final : public juce::AudioPluginInstance {
public:
    explicit DummyLatencyOfflineTestPlugin(int latency)
        : AudioPluginInstance(BusesProperties()
                                  .withInput("Input", juce::AudioChannelSet::stereo(), true)
                                  .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
        setLatencySamples(latency);
        delay.assign(static_cast<std::size_t>(std::max(0, latency)) + 1, 0.0f);
    }

    const juce::String getName() const override {
        return "DummyLatencyOfflineTestPlugin";
    }

    void prepareToPlay(double, int) override {
        std::ranges::fill(delay, 0.0f);
        writePosition = 0;
        activeNote = -1;
    }
    void releaseResources() override {
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {
        auto event = midiMessages.begin();
        const auto end = midiMessages.end();
        const auto consume = [&](const juce::MidiMessage& message) {
            if (message.isNoteOn()) {
                activeNote = message.getNoteNumber();
                impulse = true;
            } else if ((message.isNoteOff() && message.getNoteNumber() == activeNote) || message.isAllNotesOff()
                       || message.isAllSoundOff()) {
                activeNote = -1;
            }
        };
        if (buffer.getNumSamples() == 0) {
            for (; event != end; ++event) {
                consume((*event).getMessage());
            }
            return;
        }
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            while (event != end && (*event).samplePosition <= sample) {
                consume((*event).getMessage());
                ++event;
            }
            const auto input = impulse ? 0.4f : 0.0f;
            impulse = false;
            delay[writePosition] = input;
            const auto readPosition = (writePosition + 1) % delay.size();
            const auto value = delay[readPosition];
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
                buffer.setSample(channel, sample, value);
            }
            writePosition = readPosition;
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
    void getStateInformation(juce::MemoryBlock&) override {
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
    std::vector<float> delay;
    std::size_t writePosition = 0;
    bool impulse = false;
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
        testInvalidTimeline();
        testOfflineRenderingExecution();
        testMonoPluginStereoDownmix();
        testMasterSoftLimiterBehavior();
        testOfflineRenderingWithRoomReverb();
        testProgressCancellation();
        testPresetSnapshotRejection();
        testDualLayerMissingPluginProtection();
        testDualLayerDelayedImpulse();
        testDualLayerPresetTransitionsAndNoResurrection();
        testSameSamplePresetBoundaryEventDelivery();
        testTakePresetPluginMissingRejection();
        testLatencyCapacityAndFaultBlocking();
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

    void testInvalidTimeline() {
        beginTest("Invalid numeric timeline rejects before output without replacing user data");
        DummyOfflineTestPlugin plugin;
        devpiano::exporting::WavExportOptions options;
        devpiano::test::ScopedTempDir tempDir("offline-timeline");
        const auto target = tempDir.getChildFile("original.wav");
        expect(target.replaceWithText("retained plugin render"));
        const auto original = target.loadFileAsString();
        auto take = makeSimpleRenderTake();
        take.lengthSamples = std::numeric_limits<std::int64_t>::max();
        take.events.back().timestampSamples = take.lengthSamples;
        auto progressCalled = false;
        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(take, target, options, plugin, [&](double) {
            progressCalled = true;
            return false;
        }));
        expect(!progressCalled);
        expectEquals(target.loadFileAsString(), original);
        take.events.back().timestampSamples = 44100;
        take.lengthSamples = std::numeric_limits<std::int64_t>::max() - 88199;
        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(take, target, options, plugin));
        expectEquals(target.loadFileAsString(), original);
        take = makeSimpleRenderTake();
        take.sampleRate = std::numeric_limits<double>::infinity();
        const auto missingParent = tempDir.getChildFile("not-created").getChildFile("invalid.wav");
        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(take, missingParent, options, plugin));
        expect(!missingParent.getParentDirectory().exists());
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
        options.sampleRate = 48000.0;
        expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, outFile, options, plugin));
        options.sampleRate = 44100.0;
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
        expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, outFile, options, plugin));
        juce::MemoryBlock original;
        expect(outFile.loadFileAsData(original));
        const bool success
            = devpiano::exporting::renderTakeWithOfflinePlugin(take, outFile, options, plugin, cancelCallback);
        expect(!success, "Render must abort and return false when progressCallback returns false");
        expect(progressCalls >= 2, "Progress callback should have been invoked at least twice before aborting");
        juce::MemoryBlock afterCancel;
        expect(outFile.loadFileAsData(afterCancel));
        expect(afterCancel == original, "cancelled plugin render must preserve the existing WAV");
        expectEquals(tempDir.get().getNumberOfChildFiles(juce::File::findFiles), 1,
                     "cancelled plugin render must remove only its temporary output");
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

    void testOfflineRenderingWithRoomReverb() {
        beginTest("Offline rendering with RoomReverbEngine generates diffused tail and valid audio (QUAL-001)");

        DummyOfflineTestPlugin plugin;
        devpiano::exporting::WavExportOptions dryOptions;
        dryOptions.sampleRate = 44100.0;
        dryOptions.numChannels = 2;
        dryOptions.blockSize = 512;
        dryOptions.bitsPerSample = 16;
        dryOptions.masterGain = 1.0f;
        dryOptions.reverbWet = 0.0f;

        devpiano::exporting::WavExportOptions wetOptions = dryOptions;
        wetOptions.reverbSpace = devpiano::audio::ReverbSpace::concertHall;
        wetOptions.reverbWet = 0.5f;

        devpiano::test::ScopedTempDir tempDir("offline-reverb-test");
        const auto dryFile = tempDir.getChildFile("dry.wav");
        const auto wetFile = tempDir.getChildFile("wet.wav");

        const auto take = makeSimpleRenderTake();

        expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, dryFile, dryOptions, plugin),
               "dry render succeeds");
        expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, wetFile, wetOptions, plugin),
               "wet render succeeds");

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatReader> dryReader(
            wavFormat.createReaderFor(dryFile.createInputStream().release(), true));
        std::unique_ptr<juce::AudioFormatReader> wetReader(
            wavFormat.createReaderFor(wetFile.createInputStream().release(), true));

        expect(dryReader != nullptr && wetReader != nullptr);
        if (dryReader != nullptr && wetReader != nullptr) {
            expectEquals(dryReader->lengthInSamples, wetReader->lengthInSamples);

            // Read the tail region (after note-off, where plugin outputs 0 but reverb continues to ring)
            const auto noteOffSample = static_cast<std::int64_t>(44100.0 * 0.5); // note-off at 0.5s
            const auto tailCheckStart = noteOffSample + 2048;
            const auto checkLength = 4096;

            juce::AudioBuffer<float> dryTail(2, checkLength);
            juce::AudioBuffer<float> wetTail(2, checkLength);

            dryReader->read(&dryTail, 0, checkLength, tailCheckStart, true, true);
            wetReader->read(&wetTail, 0, checkLength, tailCheckStart, true, true);

            // In the dry render, DummyOfflineTestPlugin stops immediately on note-off so tail is silent
            expectEquals(dryTail.getMagnitude(0, 0, checkLength), 0.0f, "dry tail must be silent after note-off");

            // In the wet render, RoomReverbEngine diffuses the preceding note into the tail region
            expect(wetTail.getMagnitude(0, 0, checkLength) > 1e-4f, "wet tail must contain diffuse reverb energy");
        }
    }
    void testPresetSnapshotRejection() {
        beginTest("Missing or malformed preset snapshot rejects before touching output file");
        DummyOfflineTestPlugin plugin;
        devpiano::exporting::WavExportOptions options;
        devpiano::test::ScopedTempDir tempDir("offline-snapshot-reject");
        const auto target = tempDir.getChildFile("protected.wav");
        expect(target.replaceWithText("user data must be preserved"));

        // Missing presets vector for presetChange event
        auto takeNoPresets = makeSimpleRenderTake();
        devpiano::recording::PerformanceEvent presetEv;
        presetEv.type = devpiano::recording::PerformanceEventType::presetChange;
        presetEv.timestampSamples = 0;
        presetEv.presetId = 0;
        takeNoPresets.events.insert(takeNoPresets.events.begin(), presetEv);

        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(takeNoPresets, target, options, plugin));
        expectEquals(target.loadFileAsString(), juce::String("user data must be preserved"));

        // Malformed snapshot (NaN masterGain)
        auto takeNan = takeNoPresets;
        devpiano::recording::RecordedPreset badPreset;
        badPreset.acoustic.masterGain = std::numeric_limits<float>::quiet_NaN();
        takeNan.presets.push_back(badPreset);

        expect(!devpiano::exporting::renderTakeWithOfflinePlugin(takeNan, target, options, plugin));
        expectEquals(target.loadFileAsString(), juce::String("user data must be preserved"));
    }

    void testDualLayerMissingPluginProtection() {
        beginTest("Dual layer export with missing plugin fails explicitly and preserves target file");

        devpiano::exporting::WavExportOptions options;
        options.sampleRate = 44100.0;
        options.numChannels = 2;
        options.blockSize = 512;
        options.bitsPerSample = 16;
        options.masterGain = 1.0f;
        options.layers.enabled = true;
        options.layers.pianoEnabled = true;
        options.layers.pluginEnabled = true;

        devpiano::test::ScopedTempDir tempDir("dual-layer-protection");
        const auto target = tempDir.getChildFile("protected_dual.wav");
        expect(target.replaceWithText("user data must remain intact"));
        const auto take = makeSimpleRenderTake();

        // 1. renderTakeThroughInstrumentEndpoint with nullptr plugin must return false
        expect(!devpiano::exporting::renderTakeThroughInstrumentEndpoint(take, target, options, nullptr),
               "missing plugin in dual-layer export must fail");
        expectEquals(target.loadFileAsString(), juce::String("user data must remain intact"),
                     "target file must be preserved after rejected dual-layer export");

        // 2. exportTakeAsWavFile with dual-layer plugin enabled must return false
        expect(!devpiano::exporting::exportTakeAsWavFile(take, target, options),
               "exportTakeAsWavFile must reject dual layer requiring plugin");
        expectEquals(target.loadFileAsString(), juce::String("user data must remain intact"),
                     "target file must be preserved after rejected exportTakeAsWavFile");

        // 3. Dual layer with plugin disabled (piano-only dual layer) succeeds with nullptr
        options.layers.pluginEnabled = false;
        options.layers.pianoEnabled = true;
        const auto pianoOnlyFile = tempDir.getChildFile("piano_only_dual.wav");
        expect(devpiano::exporting::renderTakeThroughInstrumentEndpoint(take, pianoOnlyFile, options, nullptr),
               "piano-only dual layer must succeed without plugin");
        expect(pianoOnlyFile.existsAsFile());
    }

    void testDualLayerDelayedImpulse() {
        beginTest("Both real delayed outputs align across blocks and sum within PCM quantization");
        devpiano::test::ScopedTempDir tempDir("delayed-two-layer");
        devpiano::recording::RecordingTake take;
        take.sampleRate = 48000.0;
        take.lengthSamples = 2048;
        take.events
            = { { 64, devpiano::recording::PerformanceEventType::midi, 0,
                  devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOn(1, 60, 0.8f) },
                { 1536, devpiano::recording::PerformanceEventType::midi, 0,
                  devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOff(1, 60) } };
        const auto render = [&](float pianoGain, float pluginGain, const char* fileName) {
            DummyLatencyOfflineTestPlugin plugin(256);
            devpiano::exporting::WavExportOptions options;
            options.sampleRate = 48000.0;
            options.blockSize = 128;
            options.bitsPerSample = 24;
            options.layers = { true, true, true, pianoGain, pluginGain };
            const auto file = tempDir.getChildFile(fileName);
            expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, file, options, plugin));
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatReader> reader(
                format.createReaderFor(file.createInputStream().release(), true));
            juce::AudioBuffer<float> audio(2, 2048);
            audio.clear();
            expect(reader != nullptr);
            if (reader != nullptr) {
                reader->read(&audio, 0, 2048, 0, true, true);
            }
            return audio;
        };
        const auto piano = render(0.3f, 0.0f, "piano.wav");
        const auto plugin = render(0.0f, 0.4f, "plugin.wav");
        const auto combined = render(0.3f, 0.4f, "combined.wav");
        expectEquals(plugin.getMagnitude(0, 0, 320), 0.0f);
        expectWithinAbsoluteError(plugin.getSample(0, 320), 0.16f, 3.0f / 8388608.0f);
        expectEquals(piano.getMagnitude(0, 0, 320), 0.0f);
        expect(piano.getMagnitude(0, 320, 256) > 0.001f);
        for (int channel = 0; channel < 2; ++channel) {
            for (int sample = 0; sample < 2048; ++sample) {
                expectWithinAbsoluteError(combined.getSample(channel, sample),
                                          piano.getSample(channel, sample) + plugin.getSample(channel, sample),
                                          3.0f / 8388608.0f);
            }
        }
    }

    void testDualLayerPresetTransitionsAndNoResurrection() {
        beginTest("Preset layer transitions do not resurrect old held notes and honour layer states");

        DummyOfflineTestPlugin plugin;
        devpiano::exporting::WavExportOptions options;
        options.sampleRate = 44100.0;
        options.numChannels = 2;
        options.blockSize = 512;
        options.bitsPerSample = 16;
        options.masterGain = 1.0f;
        options.layers.enabled = false;

        devpiano::test::ScopedTempDir tempDir("layer-transitions");
        const auto outFile = tempDir.getChildFile("transitions.wav");

        devpiano::recording::RecordingTake take;
        take.sampleRate = 44100.0;
        take.lengthSamples = 44100;

        devpiano::recording::RecordedPreset preset0;
        preset0.acoustic.layers.enabled = false;

        devpiano::recording::RecordedPreset preset1;
        preset1.acoustic.layers.enabled = true;
        preset1.acoustic.layers.pianoEnabled = true;
        preset1.acoustic.layers.pluginEnabled = true;
        preset1.acoustic.layers.pianoGain = 0.5f;
        preset1.acoustic.layers.pluginGain = 0.5f;

        devpiano::recording::RecordedPreset preset2;
        preset2.acoustic.layers.enabled = true;
        preset2.acoustic.layers.pianoEnabled = false;
        preset2.acoustic.layers.pluginEnabled = true;
        preset2.acoustic.layers.pluginGain = 0.8f;

        auto mutedPreset = preset1;
        mutedPreset.acoustic.layers.pianoEnabled = false;
        mutedPreset.acoustic.layers.pluginEnabled = false;
        take.presets = { preset0, preset1, preset2, mutedPreset };

        take.events = {
            { 0,
              devpiano::recording::PerformanceEventType::presetChange,
              0,
              devpiano::recording::RecordingEventSource::computerKeyboard,
              {} },
            { 1000, devpiano::recording::PerformanceEventType::midi, 0,
              devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOn(1, 60, 0.8f) },
            { 10000,
              devpiano::recording::PerformanceEventType::presetChange,
              1,
              devpiano::recording::RecordingEventSource::computerKeyboard,
              {} },
            { 11000, devpiano::recording::PerformanceEventType::midi, 0,
              devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOff(1, 60, 0.0f) },
            { 12000, devpiano::recording::PerformanceEventType::midi, 0,
              devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOn(1, 64, 0.8f) },
            { 14000,
              devpiano::recording::PerformanceEventType::presetChange,
              3,
              devpiano::recording::RecordingEventSource::computerKeyboard,
              {} },
            { 16000, devpiano::recording::PerformanceEventType::midi, 0,
              devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOff(1, 64, 0.0f) },
            { 18000,
              devpiano::recording::PerformanceEventType::presetChange,
              1,
              devpiano::recording::RecordingEventSource::computerKeyboard,
              {} },
            { 25000,
              devpiano::recording::PerformanceEventType::presetChange,
              2,
              devpiano::recording::RecordingEventSource::computerKeyboard,
              {} },
            { 27000, devpiano::recording::PerformanceEventType::midi, 0,
              devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOn(1, 67, 0.8f) },
            { 33000, devpiano::recording::PerformanceEventType::midi, 0,
              devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOff(1, 67, 0.0f) },
        };

        expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, outFile, options, plugin));

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatReader> reader(
            wavFormat.createReaderFor(outFile.createInputStream().release(), true));
        expect(reader != nullptr);
        if (reader != nullptr) {
            juce::AudioBuffer<float> gapBuffer(2, 1024);
            expect(reader->read(&gapBuffer, 0, 1024, 10000, true, true));
            expectEquals(gapBuffer.getMagnitude(0, 0, 1024), 0.0f,
                         "a mode change must not replay the held single-layer note");
            expect(reader->read(&gapBuffer, 0, 1024, 18000, true, true));
            expectEquals(gapBuffer.getMagnitude(0, 0, 1024), 0.0f,
                         "reenabling layers must not resurrect a note released while both were disabled");
            expect(reader->read(&gapBuffer, 0, 1024, 28000, true, true));
            expectWithinAbsoluteError(gapBuffer.getSample(0, 512), 0.4f, 2.0f / 32768.0f,
                                      "the enabled plugin must still sound a subsequent fresh note");
        }
    }

    void testSameSamplePresetBoundaryEventDelivery() {
        beginTest("Same-sample preset boundary delivers old NoteOff and controls without dropping");

        DummyOfflineTestPlugin plugin;
        devpiano::exporting::WavExportOptions options;
        options.sampleRate = 44100.0;
        options.numChannels = 2;
        options.blockSize = 512;
        options.bitsPerSample = 16;
        options.masterGain = 1.0f;
        options.layers.enabled = true;
        options.layers.pianoEnabled = true;
        options.layers.pluginEnabled = true;

        devpiano::test::ScopedTempDir tempDir("boundary-midi");
        const auto outFile = tempDir.getChildFile("boundary.wav");

        devpiano::recording::RecordingTake take;
        take.sampleRate = 44100.0;
        take.lengthSamples = 22050;

        devpiano::recording::RecordedPreset preset0;
        preset0.acoustic.layers.enabled = true;
        preset0.acoustic.layers.pianoGain = 0.5f;

        devpiano::recording::RecordedPreset preset1;
        preset1.acoustic.layers.enabled = true;
        preset1.acoustic.layers.pianoGain = 0.8f;

        take.presets = { preset0, preset1 };

        take.events
            = { { 0,
                  devpiano::recording::PerformanceEventType::presetChange,
                  0,
                  devpiano::recording::RecordingEventSource::computerKeyboard,
                  {} },
                { 0, devpiano::recording::PerformanceEventType::midi, 0,
                  devpiano::recording::RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOn(1, 60, 0.8f) },
                { 10240, devpiano::recording::PerformanceEventType::midi, 0,
                  devpiano::recording::RecordingEventSource::computerKeyboard,
                  juce::MidiMessage::noteOff(1, 60, 0.0f) },
                { 10240,
                  devpiano::recording::PerformanceEventType::presetChange,
                  1,
                  devpiano::recording::RecordingEventSource::computerKeyboard,
                  {} } };

        expect(devpiano::exporting::renderTakeWithOfflinePlugin(take, outFile, options, plugin));
        expect(outFile.existsAsFile());
    }

    void testTakePresetPluginMissingRejection() {
        beginTest("Take preset requiring dual plugin fails export when offline instance is null");

        devpiano::exporting::WavExportOptions options;
        options.sampleRate = 44100.0;
        options.layers.enabled = false;

        devpiano::test::ScopedTempDir tempDir("preset-plugin-missing");
        const auto target = tempDir.getChildFile("target.wav");
        expect(target.replaceWithText("protected content"));

        devpiano::recording::RecordingTake take;
        take.sampleRate = 44100.0;
        take.lengthSamples = 4410;

        devpiano::recording::RecordedPreset p;
        p.acoustic.layers.enabled = true;
        p.acoustic.layers.pluginEnabled = true;
        take.presets = { p };
        take.events = { { 0,
                          devpiano::recording::PerformanceEventType::presetChange,
                          0,
                          devpiano::recording::RecordingEventSource::computerKeyboard,
                          {} } };

        expect(!devpiano::exporting::renderTakeThroughInstrumentEndpoint(take, target, options, nullptr),
               "preset requiring plugin must reject export when plugin instance is null");
        expectEquals(target.loadFileAsString(), juce::String("protected content"));

        expect(!devpiano::exporting::exportTakeAsWavFile(take, target, options),
               "exportTakeAsWavFile must reject when preset requires plugin");
        expectEquals(target.loadFileAsString(), juce::String("protected content"));
    }

    void testLatencyCapacityAndFaultBlocking() {
        beginTest("Latency capacity guards against negative, huge values, and arithmetic overflow");

        using namespace devpiano::audio;
        expectEquals(sanitizePluginLatency(-100), 0, "negative latency must sanitize to 0");
        expectEquals(sanitizePluginLatency(0), 0);
        expectEquals(sanitizePluginLatency(512), 512);
        expectEquals(sanitizePluginLatency(1000000), kMaxLatencyCapacity, "huge latency must clamp to max capacity");

        expectEquals(calculateDelayCapacity(-50), kMinLatencyCapacity);
        expectEquals(calculateDelayCapacity(0), kMinLatencyCapacity);
        expectEquals(calculateDelayCapacity(100), kMinLatencyCapacity);
        expectEquals(calculateDelayCapacity(20000), 40000);
        expectEquals(calculateDelayCapacity(kMaxLatencyCapacity), kMaxLatencyCapacity);
        expectEquals(calculateDelayCapacity(std::numeric_limits<int>::max()), kMaxLatencyCapacity,
                     "max int latency must not overflow multiplication and clamp to max capacity");

        BoundedStereoDelayLine delayLine;
        delayLine.prepare(100, 2);
        expectEquals(delayLine.getCapacity(), kMinLatencyCapacity);
        expect(!delayLine.isOverCapacity(100));
        expect(delayLine.isOverCapacity(kMinLatencyCapacity + 1));
    }
};

static PluginOfflineRendererTest pluginOfflineRendererTest;

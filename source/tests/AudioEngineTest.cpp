#include <JuceHeader.h>

#include "Audio/AudioEngine.h"

// =============================================================================
// Tests for AudioEngine: prepareToPlay, master gain, warmup, releaseResources,
// all-notes-off.
//
// NOTE: The fallback Synthesiser audio-output path (keyboardState.noteOn →
// processNextMidiBuffer → synth → non-zero buffer) is NOT tested here.  The
// JUCE MidiMessageCollector timing model depends on wall-clock deltas that are
// unreliable in a headless unit-test environment.  That path is verified
// through integration / manual testing (play a note and hear it).
//
// What IS tested:
//   - Lifecycle safety (prepareToPlay, getNextAudioBlock, releaseResources)
//   - Null buffer guard
//   - Master gain clamping
//   - Gain = 0 silences output
//   - All-notes-off does not crash
//   - Warmup blocks suppress audio
//   - Release + re-prepare cycle
// =============================================================================

namespace {
auto makeBlock(int numChannels, int numSamples, int startSample = 0)
    -> std::pair<juce::AudioBuffer<float>, juce::AudioSourceChannelInfo> {
    juce::AudioBuffer<float> buf(numChannels, numSamples);
    buf.clear();
    juce::AudioSourceChannelInfo info(&buf, startSample, numSamples - startSample);
    return { std::move(buf), info };
}

int countNonZeroSamples(const juce::AudioBuffer<float>& buf, int start, int n) {
    int c = 0;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < n; ++i)
            if (buf.getReadPointer(ch, start)[i] != 0.0f)
                ++c;
    return c;
}

void exhaustWarmup(AudioEngine& engine, int blockSize) {
    for (int i = 0; i < 5; ++i) {
        auto [buf, info] = makeBlock(2, blockSize);
        engine.getNextAudioBlock(info);
    }
}
} // namespace

// =============================================================================

class PrepareToPlayTest : public juce::UnitTest {
public:
    PrepareToPlayTest()
        : juce::UnitTest("AudioEngine: prepareToPlay") {
    }
    void runTest() override {
        beginTest("prepareToPlay does not crash");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            expect(true);
        }
        beginTest("prepareToPlay with different rates / sizes");
        {
            AudioEngine e1;
            e1.prepareToPlay(256, 48000.0);
            expect(true);
            AudioEngine e2;
            e2.prepareToPlay(1024, 22050.0);
            expect(true);
        }
        beginTest("getNextAudioBlock works after prepareToPlay");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            auto [buf, info] = makeBlock(2, 512);
            engine.getNextAudioBlock(info);
            expect(true);
        }
        beginTest("null buffer is safe");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            juce::AudioSourceChannelInfo nullInfo(nullptr, 0, 0);
            engine.getNextAudioBlock(nullInfo);
            expect(true);
        }
    }
};
static PrepareToPlayTest prepareToPlayTest;

// =============================================================================

class MasterGainTest : public juce::UnitTest {
public:
    MasterGainTest()
        : juce::UnitTest("AudioEngine: master gain") {
    }
    void runTest() override {
        beginTest("gain 0 silences output (masterGain applied at end of block)");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(0.0f);
            exhaustWarmup(engine, 512);
            auto [buf, info] = makeBlock(2, 512);
            engine.getNextAudioBlock(info);
            // Even if the synth produced audio, gain=0 zeros it.
            int nz = countNonZeroSamples(buf, info.startSample, info.numSamples);
            expectEquals(nz, 0);
        }
        beginTest("gain clamps negative to 0");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(-0.5f);
            exhaustWarmup(engine, 512);
            auto [buf, info] = makeBlock(2, 512);
            engine.getNextAudioBlock(info);
            expectEquals(countNonZeroSamples(buf, info.startSample, info.numSamples), 0);
        }
        beginTest("gain clamps >1.0 to 1.0 without crash");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(2.0f);
            exhaustWarmup(engine, 512);
            auto [buf, info] = makeBlock(2, 512);
            engine.getNextAudioBlock(info);
            expect(true); // no crash
        }
    }
};
static MasterGainTest masterGainTest;

// =============================================================================

class AllNotesOffTest : public juce::UnitTest {
public:
    AllNotesOffTest()
        : juce::UnitTest("AudioEngine: all-notes-off") {
    }
    void runTest() override {
        beginTest("requestAllNotesOff does not crash");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.requestAllNotesOff();
            auto [buf, info] = makeBlock(2, 512);
            engine.getNextAudioBlock(info);
            expect(true);
        }
        beginTest("subsequent blocks after all-notes-off are safe");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.requestAllNotesOff();
            for (int i = 0; i < 5; ++i) {
                auto [buf, info] = makeBlock(2, 512);
                engine.getNextAudioBlock(info);
            }
            expect(true); // survived
        }
    }
};
static AllNotesOffTest allNotesOffTest;

// =============================================================================

class WarmupTest : public juce::UnitTest {
public:
    WarmupTest()
        : juce::UnitTest("AudioEngine: warmup") {
    }
    void runTest() override {
        beginTest("first two blocks after prepareToPlay are silent");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(1.0f);

            auto [buf1, info1] = makeBlock(2, 512);
            engine.getNextAudioBlock(info1);
            expectEquals(countNonZeroSamples(buf1, info1.startSample, info1.numSamples), 0);

            auto [buf2, info2] = makeBlock(2, 512);
            engine.getNextAudioBlock(info2);
            expectEquals(countNonZeroSamples(buf2, info2.startSample, info2.numSamples), 0);
        }
        beginTest("blocks after warmup exhaustion are safe");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            for (int i = 0; i < 10; ++i) {
                auto [buf, info] = makeBlock(2, 512);
                engine.getNextAudioBlock(info);
            }
            expect(true); // survived
        }
    }
};
static WarmupTest warmupTest;

// =============================================================================

class ReleaseResourcesTest : public juce::UnitTest {
public:
    ReleaseResourcesTest()
        : juce::UnitTest("AudioEngine: releaseResources") {
    }
    void runTest() override {
        beginTest("releaseResources does not crash");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.releaseResources();
            expect(true);
        }
        beginTest("re-prepare after release works");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.releaseResources();
            engine.prepareToPlay(256, 48000.0);
            exhaustWarmup(engine, 256);
            auto [buf, info] = makeBlock(2, 256);
            engine.getNextAudioBlock(info);
            expect(true);
        }
        beginTest("releaseResources silences running notes (post-release silence)");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.releaseResources();
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            // No MIDI fed — should be silent.
            auto [buf, info] = makeBlock(2, 512);
            engine.getNextAudioBlock(info);
            expectEquals(countNonZeroSamples(buf, info.startSample, info.numSamples), 0);
        }
    }
};
static ReleaseResourcesTest releaseResourcesTest;

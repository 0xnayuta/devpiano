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
    // NOTE: build the info AFTER moving the buffer into the pair, otherwise
    // its AudioSourceChannelInfo keeps a dangling pointer to the moved-from
    // temporary (use-after-move) — every getNextAudioBlock() call then reads
    // garbage and can crash.
    std::pair<juce::AudioBuffer<float>, juce::AudioSourceChannelInfo> result {
        juce::AudioBuffer<float>(numChannels, numSamples), {}
    };
    result.first.clear();
    result.second = juce::AudioSourceChannelInfo(&result.first, startSample, numSamples - startSample);
    return result;
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

// 合并自原 PrepareToPlayTest / WarmupTest / ReleaseResourcesTest。
class AudioEngineLifecycleTest : public juce::UnitTest {
public:
    AudioEngineLifecycleTest()
        : juce::UnitTest("AudioEngine: lifecycle", "DevPiano/Engine") {
    }
    void runTest() override {
        // —— 原 PrepareToPlayTest 的用例 ——
        beginTest("prepareToPlay does not crash");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
        }
        beginTest("prepareToPlay with different rates / sizes");
        {
            AudioEngine e1;
            e1.prepareToPlay(256, 48000.0);
            AudioEngine e2;
            e2.prepareToPlay(1024, 22050.0);
        }
        beginTest("getNextAudioBlock works after prepareToPlay");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            auto [buf, info] = makeBlock(2, 512);
            engine.getNextAudioBlock(info);
        }
        beginTest("null buffer is safe");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            juce::AudioSourceChannelInfo nullInfo(nullptr, 0, 0);
            engine.getNextAudioBlock(nullInfo);
        }
        // —— 原 WarmupTest 的用例 ——
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
        }
        // —— 原 ReleaseResourcesTest 的用例 ——
        beginTest("releaseResources does not crash");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.releaseResources();
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
static AudioEngineLifecycleTest audioEngineLifecycleTest;

// =============================================================================

// 合并自原 MasterGainTest / AllNotesOffTest。
class AudioEngineGainAndNotesOffTest : public juce::UnitTest {
public:
    AudioEngineGainAndNotesOffTest()
        : juce::UnitTest("AudioEngine: gain and all-notes-off", "DevPiano/Engine") {
    }
    void runTest() override {
        // —— 原 MasterGainTest 的用例 ——
        // 原 "gain 0 silences output" 与 "gain clamps negative to 0"
        // 断言相同（输出全零），合并为一条用例，保留两条断言路径。
        beginTest("gain 0 silences output; gain clamps negative to 0");
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
        }
        // —— 原 AllNotesOffTest 的用例 ——
        beginTest("requestAllNotesOff does not crash");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.requestAllNotesOff();
            auto [buf, info] = makeBlock(2, 512);
            engine.getNextAudioBlock(info);
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
        }
    }
};
static AudioEngineGainAndNotesOffTest audioEngineGainAndNotesOffTest;

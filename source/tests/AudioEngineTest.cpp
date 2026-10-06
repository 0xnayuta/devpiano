#include <JuceHeader.h>

#include "Audio/AudioDeviceDiagnostics.h"
#include "Audio/AudioEngine.h"
#include "Audio/PianoSynthVoice.h"
#include "Recording/RecordingEngine.h"

namespace {
juce::AudioBuffer<float> makeBlock(int numChannels, int numSamples) {
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    buffer.clear();
    return buffer;
}

int countNonZeroSamples(const juce::AudioBuffer<float>& buf, int start, int n) {
    int c = 0;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch) {
        for (int i = 0; i < n; ++i) {
            if (buf.getReadPointer(ch, start)[i] != 0.0f) {
                ++c;
            }
        }
    }
    return c;
}

void exhaustWarmup(AudioEngine& engine, int blockSize) {
    // TEST-019：消费生产定义的 warmup 块数（warmupSeconds=0.025 → 44.1k/512
    // 下 3 块），而非硬编码 5——生产改 warmupSeconds 时测试自动跟随。
    const auto warmupBlocks = AudioEngine::calculateWarmupBlockCount(44100.0, blockSize);
    for (int i = 0; i < warmupBlocks; ++i) {
        auto buf = makeBlock(2, blockSize);
        const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
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
        beginTest("physical voice zero preserves channel identity for soft pedal and release");
        {
            devpiano::audio::BuiltinSynthesiser synth;
            synth.setCurrentPlaybackSampleRate(48000.0);
            synth.addSound(new PianoSynthSound());
            auto* first = new PianoSynthVoice();
            auto* second = new PianoSynthVoice();
            first->setVoiceIndex(0);
            second->setVoiceIndex(1);
            synth.addVoice(first);
            synth.addVoice(second);
            synth.handleController(1, 67, 80);
            synth.noteOn(1, 60, 0.8f);
            synth.handleController(2, 67, 0);
            synth.noteOn(2, 60, 0.8f);
            expect(first->isSoftPedalDown());
            expectEquals(first->getSoftPedalAmount(), 80.0f / 127.0f);
            expect(!second->isSoftPedalDown());
            synth.noteOff(2, 60, 0.0f, false);
            expect(first->getCurrentlyPlayingNote() == 60 && first->isKeyDown());
            expectEquals(second->getCurrentlyPlayingNote(), -1);
            synth.noteOff(1, 60, 0.0f, false);
            expectEquals(first->getCurrentlyPlayingNote(), -1);
        }
        beginTest("reused callback buffers are overwritten without touching surrounding samples");
        {
            AudioEngine engine;
            engine.setBuiltinSynthTone(AudioEngine::BuiltinSynthTone::sine);
            engine.prepareToPlay(512, 44100.0);
            juce::AudioBuffer<float> buffer(2, 520);
            for (int channel = 0; channel < 2; ++channel) {
                std::fill_n(buffer.getWritePointer(channel), 520, 0.75f);
            }
            const juce::AudioSourceChannelInfo info(&buffer, 4, 512);
            engine.getNextAudioBlock(info);
            expectEquals(buffer.getMagnitude(4, 512), 0.0f);
            for (int channel = 0; channel < 2; ++channel) {
                expectEquals(buffer.getSample(channel, 0), 0.75f);
                expectEquals(buffer.getSample(channel, 519), 0.75f);
            }
            exhaustWarmup(engine, 512);
            engine.getKeyboardState().noteOn(1, 60, 0.8f);
            engine.getNextAudioBlock(info);
            engine.requestAllNotesOff();
            engine.getNextAudioBlock(info);
            expectEquals(buffer.getMagnitude(4, 512), 0.0f);
            expectEquals(buffer.getSample(0, 0), 0.75f);
            expectEquals(buffer.getSample(1, 519), 0.75f);
        }
        beginTest("first two blocks after prepareToPlay are silent");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(1.0f);

            auto buf1 = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info1(&buf1, 0, buf1.getNumSamples());
            engine.getNextAudioBlock(info1);
            expectEquals(countNonZeroSamples(buf1, info1.startSample, info1.numSamples), 0);

            auto buf2 = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info2(&buf2, 0, buf2.getNumSamples());
            engine.getNextAudioBlock(info2);
            expectEquals(countNonZeroSamples(buf2, info2.startSample, info2.numSamples), 0);
        }
        beginTest("re-prepare after release works");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.releaseResources();
            engine.prepareToPlay(256, 48000.0);
            exhaustWarmup(engine, 256);
            engine.getKeyboardState().noteOn(1, 60, 0.8f);
            auto buf = makeBlock(2, 256);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info);
            expect(countNonZeroSamples(buf, info.startSample, info.numSamples) > 0,
                   "re-prepared engine must render a held note (synth usable again)");
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
            auto buf = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info);
            expectEquals(countNonZeroSamples(buf, info.startSample, info.numSamples), 0);
        }
    }
};
static AudioEngineLifecycleTest audioEngineLifecycleTest;

// =============================================================================
class BuiltinSynthesiserExecutionTest : public juce::UnitTest {
public:
    BuiltinSynthesiserExecutionTest()
        : juce::UnitTest("BuiltinSynthesiser: execution and lock-free SLA", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("THR-001/PERF-001: sustain pedal CC64 holds notes past noteOff until pedal release");
        {
            devpiano::audio::BuiltinSynthesiser synth;
            synth.setCurrentPlaybackSampleRate(44100.0);
            synth.addSound(new PianoSynthSound());
            auto* v0 = new PianoSynthVoice();
            v0->setVoiceIndex(0);
            synth.addVoice(v0);

            synth.handleController(1, 64, 127);
            expect(synth.isSustainPedalDown(1));

            synth.noteOn(1, 60, 0.8f);
            expect(v0->isVoiceActive());
            expectEquals(v0->getCurrentlyPlayingNote(), 60);
            expect(v0->isKeyDown());
            expect(v0->isSustainPedalDown());

            // Release key while pedal is held
            synth.noteOff(1, 60, 0.0f, false);
            expect(!v0->isKeyDown());
            expect(v0->isSustainPedalDown());
            expect(v0->isVoiceActive());
            expectEquals(v0->getCurrentlyPlayingNote(), 60);

            // Release sustain pedal
            synth.handleController(1, 64, 0);
            expect(!synth.isSustainPedalDown(1));
            expect(!v0->isSustainPedalDown());
        }

        beginTest("sostenuto pedal CC66 latches only keys held down when engaged");
        {
            devpiano::audio::BuiltinSynthesiser synth;
            synth.setCurrentPlaybackSampleRate(44100.0);
            synth.addSound(new PianoSynthSound());
            auto* v0 = new PianoSynthVoice();
            auto* v1 = new PianoSynthVoice();
            v0->setVoiceIndex(0);
            v1->setVoiceIndex(1);
            synth.addVoice(v0);
            synth.addVoice(v1);

            // Note 60 held down first
            synth.noteOn(1, 60, 0.8f);
            expect(v0->isKeyDown());

            // Sostenuto pressed while note 60 is held
            synth.handleController(1, 66, 127);
            expect(synth.isSostenutoPedalDown(1));
            expect(v0->isSostenutoPedalDown());

            // Note 64 pressed AFTER sostenuto was engaged
            synth.noteOn(1, 64, 0.8f);
            expect(v1->isKeyDown());
            expect(!v1->isSostenutoPedalDown());

            // Release both keys
            synth.noteOff(1, 60, 0.0f, false);
            synth.noteOff(1, 64, 0.0f, false);

            // v0 was latched by sostenuto, so it stays active
            expect(v0->isVoiceActive());
            expectEquals(v0->getCurrentlyPlayingNote(), 60);

            // v1 was not latched by sostenuto, so it stops immediately
            expect(!v1->isVoiceActive());
            expectEquals(v1->getCurrentlyPlayingNote(), -1);

            // Release sostenuto pedal
            synth.handleController(1, 66, 0);
            expect(!synth.isSostenutoPedalDown(1));
        }

        beginTest("voice stealing prioritizes target pitch, released voices, and unprotected voices");
        {
            devpiano::audio::BuiltinSynthesiser synth;
            synth.setCurrentPlaybackSampleRate(44100.0);
            synth.addSound(new PianoSynthSound());
            auto* v0 = new PianoSynthVoice();
            auto* v1 = new PianoSynthVoice();
            v0->setVoiceIndex(0);
            v1->setVoiceIndex(1);
            synth.addVoice(v0);
            synth.addVoice(v1);

            // Start notes 60 and 72
            synth.noteOn(1, 60, 0.8f);
            synth.noteOn(1, 72, 0.8f);
            expect(v0->isVoiceActive() && v1->isVoiceActive());

            // Release key 72 with sustain UP (voice 1 enters released phase)
            synth.noteOff(1, 72, 0.5f, true);
            expect(v1->isPlayingButReleased());
            expect(!v0->isPlayingButReleased());

            // New note 65 should steal released voice (v1) rather than held key (v0)
            synth.noteOn(1, 65, 0.8f);
            expectEquals(v0->getCurrentlyPlayingNote(), 60);
            expectEquals(v1->getCurrentlyPlayingNote(), 65);

            // Re-trigger note 60: should steal voice currently playing note 60 (v0)
            synth.noteOn(1, 60, 0.9f);
            expectEquals(v0->getCurrentlyPlayingNote(), 60);
            expectEquals(v1->getCurrentlyPlayingNote(), 65);
        }

        beginTest("zero-length renderNextBlock consumes explicit zero-offset MIDI without audio advance");
        {
            devpiano::audio::BuiltinSynthesiser synth;
            synth.setCurrentPlaybackSampleRate(44100.0);
            synth.addSound(new PianoSynthSound());
            auto* v0 = new PianoSynthVoice();
            v0->setVoiceIndex(0);
            synth.addVoice(v0);

            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 67, 0.75f), 0);

            juce::AudioBuffer<float> buf(2, 64);
            buf.clear();
            synth.renderNextBlock(buf, midi, 0, 0);

            expect(v0->isVoiceActive());
            expectEquals(v0->getCurrentlyPlayingNote(), 67);
            expectEquals(buf.getMagnitude(0, 64), 0.0f);
        }

        beginTest("block boundary MIDI event executes exactly once without duplicate dispatch");
        {
            class CountingVoice final : public juce::SynthesiserVoice {
            public:
                bool canPlaySound(juce::SynthesiserSound*) override {
                    return true;
                }
                void startNote(int, float, juce::SynthesiserSound*, int) override {
                    ++noteOnCount;
                }
                void stopNote(float, bool) override {
                    ++noteOffCount;
                    clearCurrentNote();
                }
                void pitchWheelMoved(int) override {
                }
                void controllerMoved(int, int) override {
                    ++controllerCount;
                }
                void renderNextBlock(juce::AudioBuffer<float>&, int, int) override {
                }
                void renderNextBlock(juce::AudioBuffer<double>&, int, int) override {
                }

                int noteOnCount = 0;
                int noteOffCount = 0;
                int controllerCount = 0;
            };

            devpiano::audio::BuiltinSynthesiser synth;
            synth.setCurrentPlaybackSampleRate(44100.0);
            synth.addSound(new PianoSynthSound());
            auto* voice = new CountingVoice();
            synth.addVoice(voice);

            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 128);
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 64), 128);

            juce::AudioBuffer<float> buf(2, 128);
            buf.clear();

            // Render block [0, 128). The events at sample 128 sit exactly on the block boundary.
            synth.renderNextBlock(buf, midi, 0, 128);

            expectEquals(voice->noteOnCount, 1, "Boundary NoteOn must be dispatched exactly once");
            expectEquals(voice->noteOffCount, 0, "No duplicate noteOn should force an early voice stop");
            expectEquals(voice->controllerCount, 1, "Boundary CC must be dispatched exactly once");
        }
    }
};
static BuiltinSynthesiserExecutionTest builtinSynthesiserExecutionTest;

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
            auto buf = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
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
            auto buf = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info);
            expectEquals(countNonZeroSamples(buf, info.startSample, info.numSamples), 0);
        }
        beginTest("gain clamps >1.0 to 1.0");
        {
            // 渲染同一个按住音符：gain=2.0 必须钳制到 1.0（setMasterGain 内部
            // jlimit 0..1），输出与 gain=1.0 逐样本一致——可证伪的行为断言。
            AudioEngine engineHigh;
            engineHigh.prepareToPlay(512, 44100.0);
            engineHigh.setMasterGain(2.0f);
            exhaustWarmup(engineHigh, 512);
            engineHigh.getKeyboardState().noteOn(1, 60, 0.8f);

            AudioEngine engineUnit;
            engineUnit.prepareToPlay(512, 44100.0);
            engineUnit.setMasterGain(1.0f);
            exhaustWarmup(engineUnit, 512);
            engineUnit.getKeyboardState().noteOn(1, 60, 0.8f);

            auto bufHigh = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo infoHigh(&bufHigh, 0, bufHigh.getNumSamples());
            engineHigh.getNextAudioBlock(infoHigh);
            auto bufUnit = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo infoUnit(&bufUnit, 0, bufUnit.getNumSamples());
            engineUnit.getNextAudioBlock(infoUnit);

            expect(countNonZeroSamples(bufHigh, infoHigh.startSample, infoHigh.numSamples) > 0,
                   "held note must render after warmup");

            bool identical = true;
            for (int ch = 0; ch < bufHigh.getNumChannels(); ++ch) {
                for (int i = 0; i < infoHigh.numSamples; ++i) {
                    if (!juce::approximatelyEqual(bufHigh.getReadPointer(ch, infoHigh.startSample)[i],
                                                  bufUnit.getReadPointer(ch, infoUnit.startSample)[i])) {
                        identical = false;
                    }
                }
            }
            expect(identical, "gain >1.0 must clamp to 1.0: output identical to gain=1.0");
        }
        // —— 原 AllNotesOffTest 的用例 ——
        beginTest("requestAllNotesOff silences held notes after release");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(1.0f);
            exhaustWarmup(engine, 512);
            engine.getKeyboardState().noteOn(1, 60, 0.8f);
            auto buf0 = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info0(&buf0, 0, buf0.getNumSamples());
            engine.getNextAudioBlock(info0);
            expect(countNonZeroSamples(buf0, info0.startSample, info0.numSamples) > 0,
                   "held note must render before all-notes-off");

            engine.requestAllNotesOff();
            // ADSR release 默认 0.30s → 44.1k/512 ≈ 26 块；渲染 40 块让释放尾音
            // 完全衰减，之后必须静音。
            for (int i = 0; i < 40; ++i) {
                auto buf = makeBlock(2, 512);
                const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
                engine.getNextAudioBlock(info);
            }
            auto bufEnd = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo infoEnd(&bufEnd, 0, bufEnd.getNumSamples());
            engine.getNextAudioBlock(infoEnd);
            expectEquals(countNonZeroSamples(bufEnd, infoEnd.startSample, infoEnd.numSamples), 0,
                         "all-notes-off must silence held notes once the release tail decays");
        }
        beginTest("subsequent blocks after all-notes-off are safe and silent");
        {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            engine.requestAllNotesOff();
            int nonZeroTotal = 0;
            for (int i = 0; i < 5; ++i) {
                auto buf = makeBlock(2, 512);
                const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
                engine.getNextAudioBlock(info);
                nonZeroTotal += countNonZeroSamples(buf, info.startSample, info.numSamples);
            }
            expectEquals(nonZeroTotal, 0, "subsequent idle blocks after all-notes-off must remain completely silent");
        }
    }
};
static AudioEngineGainAndNotesOffTest audioEngineGainAndNotesOffTest;

// =============================================================================
// AUDIT TEST-008 / TEST-009：
//   - warmup 块在注入按住音符时仍静音（消除"本来无声"假通过）
//   - warmup 结束后合成器渲染出非零采样
//   - 块计数纯函数（warmup / playback-start pre-roll 时长 → 块数）
//   - setAdsr 参数钳制与极端值稳定性
//   - setPluginHost / setRecordingEngine 接线（null 安全 + 真实实例）
// =============================================================================

class AudioEngineWarmupAndCoverageTest final : public juce::UnitTest {
public:
    AudioEngineWarmupAndCoverageTest()
        : juce::UnitTest("AudioEngine: warmup, adsr and wiring", "DevPiano/Engine") {
    }

    void runTest() override {
        testWarmupSuppressesHeldNote();
        testAudioAfterWarmupWithHeldNote();
        testBlockCountFunctions();
        testSetAdsr();
        testWiring();
    }

private:
    void testWarmupSuppressesHeldNote() {
        testCase("warmup blocks stay silent even with a pressed note", [&] {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(1.0f);
            engine.getKeyboardState().noteOn(1, 60, 0.8f); // 注入按住音

            const auto warmupBlocks = AudioEngine::calculateWarmupBlockCount(44100.0, 512);
            expect(warmupBlocks > 0, "warmup must span at least one block");
            for (int i = 0; i < warmupBlocks; ++i) {
                auto buf = makeBlock(2, 512);
                const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
                engine.getNextAudioBlock(info);
                expectEquals(countNonZeroSamples(buf, info.startSample, info.numSamples), 0,
                             "warmup blocks must stay silent even with input pending");
            }
        });
    }

    void testAudioAfterWarmupWithHeldNote() {
        testCase("held note renders audio after warmup", [&] {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setMasterGain(1.0f);

            // 精确消费 warmup 块。注：warmup 期间的 discardWarmupInputState()
            // 会 reset keyboardState 丢弃输入（设计行为），因此注入必须发生在
            // warmup 结束之后，否则事件被丢弃、断言块无声。
            const auto warmupBlocks = AudioEngine::calculateWarmupBlockCount(44100.0, 512);
            for (int i = 0; i < warmupBlocks; ++i) {
                auto buf = makeBlock(2, 512);
                const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
                engine.getNextAudioBlock(info);
            }

            engine.getKeyboardState().noteOn(1, 60, 0.8f);

            auto buf = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info);
            expect(countNonZeroSamples(buf, info.startSample, info.numSamples) > 0,
                   "the synth must render the held note after warmup");
        });
    }

    void testBlockCountFunctions() {
        testCase("warmup block count maps duration to whole blocks", [&] {
            expectEquals(AudioEngine::calculateWarmupBlockCount(44100.0, 512), 3); // ceil(0.025 * 44100 / 512)
            expectEquals(AudioEngine::calculateWarmupBlockCount(48000.0, 256), 5); // ceil(0.025 * 48000 / 256)
            expectEquals(AudioEngine::calculateWarmupBlockCount(44100.0, 44100), 1);
        });

        testCase("pre-roll block count matches the same duration mapping", [&] {
            expectEquals(AudioEngine::calculatePlaybackStartPreRollBlockCount(44100.0, 512), 3);
            expectEquals(AudioEngine::calculatePlaybackStartPreRollBlockCount(48000.0, 256), 5);
        });

        testCase("invalid rates or block sizes fall back to one block", [&] {
            expectEquals(AudioEngine::calculateWarmupBlockCount(0.0, 512), 1);
            expectEquals(AudioEngine::calculateWarmupBlockCount(44100.0, 0), 1);
            expectEquals(AudioEngine::calculatePlaybackStartPreRollBlockCount(-1.0, -1), 1);
        });

        testCase("armPlaybackStartPreRoll sets a finite block count", [&] {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.armPlaybackStartPreRoll(44100.0, 512);
            // 消费 pre-roll 块不崩溃（无播放 take 时静音路径）
            for (int i = 0; i < 5; ++i) {
                auto buf = makeBlock(2, 512);
                const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
                engine.getNextAudioBlock(info);
            }
        });
    }

    void testSetAdsr() {
        testCase("setAdsr clamps extreme values without crashing", [&] {
            AudioEngine engine;
            engine.prepareToPlay(512, 44100.0);
            engine.setAdsr(0.0f, 0.0f, 0.0f, 0.0f); // 下限
            engine.setAdsr(10.0f, 10.0f, 2.0f, 10.0f); // 上限（sustain 钳到 1）
            engine.getKeyboardState().noteOn(1, 60, 0.8f);
            exhaustWarmup(engine, 512);
            for (int i = 0; i < 3; ++i) {
                auto buf = makeBlock(2, 512);
                const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
                engine.getNextAudioBlock(info);
                const auto* ch0 = buf.getReadPointer(0);
                for (int s = 0; s < info.numSamples; ++s) {
                    expect(!std::isnan(ch0[s]), "output must stay finite");
                }
            }
        });
    }

    void testWiring() {
        testCase("null host / engine wiring is safe", [&] {
            AudioEngine engine;
            engine.setPluginHost(nullptr);
            engine.setRecordingEngine(nullptr);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            auto buf = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info);
        });

        testCase("a live RecordingEngine can be wired in", [&] {
            devpiano::recording::RecordingEngine rec;
            AudioEngine engine;
            engine.setRecordingEngine(&rec);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);
            auto buf = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info); // 未播放 → 渲染跳过，安全
            expect(engine.getPluginHost() == nullptr);
        });
    }
};

static AudioEngineWarmupAndCoverageTest audioEngineWarmupAndCoverageTest;
class AudioEnginePlaybackTransposeTest final : public juce::UnitTest {
public:
    AudioEnginePlaybackTransposeTest()
        : juce::UnitTest("AudioEngine: playback transpose", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("playback transpose state getter and setter");
        {
            AudioEngine engine;
            expect(!engine.isPlaybackTransposeEnabled());
            expectEquals(engine.getPlaybackTransposeOffset(), 0);

            engine.setPlaybackTranspose(true, 5);
            expect(engine.isPlaybackTransposeEnabled());
            expectEquals(engine.getPlaybackTransposeOffset(), 5);

            engine.setPlaybackTranspose(false, -3);
            expect(!engine.isPlaybackTransposeEnabled());
            expectEquals(engine.getPlaybackTransposeOffset(), -3);
        }

        beginTest("playback transpose applies to melodic channels and bypasses channel 10 drums");
        {
            devpiano::recording::RecordingEngine rec;
            AudioEngine engine;
            engine.setRecordingEngine(&rec);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);

            // Create take with Channel 1 (piano C4=60) and Channel 10 (drums kick=36)
            devpiano::recording::RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 2048;
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 60, 0.8f),
            });
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(10, 36, 0.8f), // Drum channel
            });

            // Start playback with transpose = +2 (D major)
            engine.setPlaybackTranspose(true, 2);
            rec.startPlayback(take, 44100.0);

            // Render block containing the events (samples 0..512)
            auto buf = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info);
            engine.dispatchPendingDisplayEvents();

            // Check keyboardState:
            // Channel 1 note 60 should be transposed to 62 (D4)
            // Channel 10 drum note 36 should stay at 36 (Bypassed)
            expect(engine.getKeyboardState().isNoteOn(1, 62), "Channel 1 note 60 should be transposed to 62 (+2)");
            expect(!engine.getKeyboardState().isNoteOn(1, 60), "Channel 1 note 60 should NOT be on");
            expect(engine.getKeyboardState().isNoteOn(10, 36), "Channel 10 drum note 36 must NOT be transposed");
            expect(!engine.getKeyboardState().isNoteOn(10, 38), "Channel 10 note 38 should NOT be on");
            rec.stopPlaybackQuiescent();
        }

        beginTest("playback transpose respects custom per-channel mask overrides");
        {
            devpiano::recording::RecordingEngine rec;
            AudioEngine engine;
            engine.setRecordingEngine(&rec);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);

            devpiano::recording::RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 2048;
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 60, 0.8f),
            });
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(10, 36, 0.8f),
            });

            // Set custom mask where Channel 1 is bypassed (bit 0 = 0) and Channel 10 is transposed (bit 9 = 1)
            const auto customMask = static_cast<std::uint16_t>(1U << 9);
            engine.setPlaybackTranspose(true, 3, customMask);
            rec.startPlayback(take, 44100.0);

            auto buf = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info);
            engine.dispatchPendingDisplayEvents();

            // Channel 1 was disabled in mask -> remains 60
            expect(engine.getKeyboardState().isNoteOn(1, 60), "Channel 1 note 60 should remain 60 (disabled in mask)");
            expect(!engine.getKeyboardState().isNoteOn(1, 63));
            // Channel 10 was enabled in mask -> transposed 36 + 3 = 39
            expect(engine.getKeyboardState().isNoteOn(10, 39), "Channel 10 note 36 should be transposed to 39");
            expect(!engine.getKeyboardState().isNoteOn(10, 36));

            rec.stopPlaybackQuiescent();
        }
        beginTest("playback seek releases held notes and applies the requested take sample");
        {
            devpiano::recording::RecordingEngine rec;
            AudioEngine engine;
            engine.setRecordingEngine(&rec);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);

            devpiano::recording::RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 4096;
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 60, 0.8f),
            });
            rec.startPlayback(take, 44100.0);

            auto playBuffer = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo playInfo(&playBuffer, 0, playBuffer.getNumSamples());
            engine.getNextAudioBlock(playInfo);
            engine.dispatchPendingDisplayEvents();
            expect(engine.getKeyboardState().isNoteOn(1, 60), "the playback note should be sounding before seek");

            rec.pausePlayback();
            rec.requestPlaybackSeek(1234);
            auto seekBuffer = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo seekInfo(&seekBuffer, 0, seekBuffer.getNumSamples());
            engine.getNextAudioBlock(seekInfo);
            engine.dispatchPendingDisplayEvents();

            expectEquals(static_cast<std::int64_t>(1234), rec.getPlaybackPositionInTakeSamples());
            expect(!engine.getKeyboardState().isNoteOn(1, 60), "seek must clear the currently sounding note");
            rec.stopPlaybackQuiescent();
        }
        beginTest("QUAL-001: NoteOn emitted under one mapping releases original output identity when transpose changes "
                  "before NoteOff");
        {
            devpiano::recording::RecordingEngine rec;
            AudioEngine engine;
            engine.setRecordingEngine(&rec);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);

            devpiano::recording::RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 2048;
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 60, 0.8f),
            });
            take.events.push_back({
                .timestampSamples = 600,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOff(1, 60, 0.0f),
            });

            engine.setPlaybackTranspose(true, 0);
            rec.startPlayback(take, 44100.0);
            engine.prepareToPlay(512, 44100.0);
            auto buf1 = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info1(&buf1, 0, buf1.getNumSamples());
            engine.getNextAudioBlock(info1);
            engine.dispatchPendingDisplayEvents();
            expect(engine.getKeyboardState().isNoteOn(1, 60), "Note 60 should be active in keyboard state");

            engine.setPlaybackTranspose(true, 1);

            auto buf2 = makeBlock(2, 512);
            const juce::AudioSourceChannelInfo info2(&buf2, 0, buf2.getNumSamples());
            engine.getNextAudioBlock(info2);
            engine.dispatchPendingDisplayEvents();

            expect(!engine.getKeyboardState().isNoteOn(1, 60),
                   "Note 60 must be released using original snapshot identity");
            expect(!engine.getKeyboardState().isNoteOn(1, 61), "Note 61 must never be active");

            rec.stopPlaybackQuiescent();
        }
        beginTest("QUAL-001: Overlapping same-source notes under different mappings paired FIFO");
        {
            devpiano::recording::RecordingEngine rec;
            AudioEngine engine;
            engine.setRecordingEngine(&rec);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);

            devpiano::recording::RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 3000;
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 60, 0.8f),
            });
            take.events.push_back({
                .timestampSamples = 600,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 60, 0.8f),
            });
            take.events.push_back({
                .timestampSamples = 1100,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOff(1, 60, 0.0f),
            });
            take.events.push_back({
                .timestampSamples = 1600,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOff(1, 60, 0.0f),
            });

            engine.setPlaybackTranspose(true, 0);
            rec.startPlayback(take, 44100.0);
            engine.prepareToPlay(512, 44100.0);
            auto buf1 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf1, 0, 512 });
            engine.dispatchPendingDisplayEvents();
            expect(engine.getKeyboardState().isNoteOn(1, 60));

            engine.setPlaybackTranspose(true, 2);

            auto buf2 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf2, 0, 512 });
            engine.dispatchPendingDisplayEvents();
            expect(engine.getKeyboardState().isNoteOn(1, 60), "Attack 1 (60) still active");
            expect(engine.getKeyboardState().isNoteOn(1, 62), "Attack 2 (62) active under +2 mapping");

            auto buf3 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf3, 0, 512 });
            engine.dispatchPendingDisplayEvents();
            expect(!engine.getKeyboardState().isNoteOn(1, 60), "Attack 1 (60) released first in FIFO order");
            expect(engine.getKeyboardState().isNoteOn(1, 62), "Attack 2 (62) still sounding");

            auto buf4 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf4, 0, 512 });
            engine.dispatchPendingDisplayEvents();
            expect(!engine.getKeyboardState().isNoteOn(1, 62), "Attack 2 (62) released second in FIFO order");

            rec.stopPlaybackQuiescent();
        }
        beginTest("QUAL-001: Clamp collisions last owner off");
        {
            devpiano::recording::RecordingEngine rec;
            AudioEngine engine;
            engine.setRecordingEngine(&rec);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);

            devpiano::recording::RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 3000;
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 126, 0.8f),
            });
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 127, 0.8f),
            });
            take.events.push_back({
                .timestampSamples = 600,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOff(1, 126, 0.0f),
            });
            take.events.push_back({
                .timestampSamples = 1100,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOff(1, 127, 0.0f),
            });

            engine.setPlaybackTranspose(true, 2);
            rec.startPlayback(take, 44100.0);
            engine.prepareToPlay(512, 44100.0);
            auto buf1 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf1, 0, 512 });
            engine.dispatchPendingDisplayEvents();
            expect(engine.getKeyboardState().isNoteOn(1, 127));

            auto buf2 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf2, 0, 512 });
            engine.dispatchPendingDisplayEvents();
            expect(engine.getKeyboardState().isNoteOn(1, 127), "Output note 127 must still be held by second owner");

            auto buf3 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf3, 0, 512 });
            engine.dispatchPendingDisplayEvents();
            expect(!engine.getKeyboardState().isNoteOn(1, 127),
                   "Output note 127 must be released when last owner releases");

            rec.stopPlaybackQuiescent();
        }
        beginTest("QUAL-018: Device rate switching retains identity releases without UI panic");
        {
            devpiano::recording::RecordingEngine rec;
            AudioEngine engine;
            engine.setRecordingEngine(&rec);
            engine.prepareToPlay(512, 44100.0);
            exhaustWarmup(engine, 512);

            devpiano::recording::RecordingTake take;
            take.sampleRate = 44100.0;
            take.lengthSamples = 3000;
            take.events.push_back({
                .timestampSamples = 10,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOn(1, 60, 0.8f),
            });
            take.events.push_back({
                .timestampSamples = 800,
                .type = devpiano::recording::PerformanceEventType::midi,
                .source = devpiano::recording::RecordingEventSource::playback,
                .message = juce::MidiMessage::noteOff(1, 60, 0.0f),
            });

            engine.setPlaybackTranspose(true, 3);
            rec.startPlayback(take, 44100.0);
            engine.prepareToPlay(512, 44100.0);
            auto buf1 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf1, 0, 512 });
            engine.dispatchPendingDisplayEvents();
            expect(engine.getKeyboardState().isNoteOn(1, 63));

            engine.prepareToPlay(512, 48000.0);
            exhaustWarmup(engine, 512);

            auto buf2 = makeBlock(2, 512);
            engine.getNextAudioBlock({ &buf2, 0, 512 });
            engine.dispatchPendingDisplayEvents();

            expect(!engine.getKeyboardState().isNoteOn(1, 63), "Device rate switch must retain identity release");

            rec.stopPlaybackQuiescent();
        }
    }
};

static AudioEnginePlaybackTransposeTest audioEnginePlaybackTransposeTest;

// =============================================================================
// Phase 25-A: AudioDeviceDiagnostics & Linux ALSA/JACK Driver State Robustness
// =============================================================================

class AudioDeviceDiagnosticsLinuxTest final : public juce::UnitTest {
public:
    AudioDeviceDiagnosticsLinuxTest()
        : juce::UnitTest("AudioEngine: Linux ALSA/JACK Diagnostics & Negotiation", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("Linux native ALSA device state exact match diagnosis");
        {
            devpiano::audio::SavedAudioDeviceState saved {
                .hasSavedDeviceStateXml = true,
                .deviceType = "ALSA",
                .inputDeviceName = "",
                .outputDeviceName = "default",
                .sampleRate = 48000.0,
                .bufferSize = 256,
            };
            devpiano::audio::LiveAudioDeviceState live {
                .hasLiveDevice = true,
                .backendName = "ALSA",
                .deviceName = "default",
                .inputDeviceName = "",
                .outputDeviceName = "default",
                .sampleRate = 48000.0,
                .bufferSize = 256,
                .defaultBufferSize = 256,
                .availableBufferSizes = { 64, 128, 256, 512, 1024 },
            };

            const auto diag = devpiano::audio::buildAudioDeviceDiagnostics(saved, live);
            expectEquals(diag.restoreOutcome, juce::String("exact"), "Native ALSA match should be exact");
            expect(diag.mismatchReasons.isEmpty(), "No mismatch reasons on exact ALSA match");
            expect(diag.compactSummary.contains("ALSA"), "Compact summary should mention ALSA");
            expect(diag.compactSummary.contains("48000 Hz"), "Compact summary should mention 48000 Hz");
            expect(diag.compactSummary.contains("256 smp"), "Compact summary should mention 256 smp");
        }

        beginTest("Cross-platform migration from Windows to Linux ALSA detects backend mismatch");
        {
            devpiano::audio::SavedAudioDeviceState savedFromWindows {
                .hasSavedDeviceStateXml = true,
                .deviceType = "Windows Audio",
                .inputDeviceName = "",
                .outputDeviceName = "Speakers (Realtek Audio)",
                .sampleRate = 44100.0,
                .bufferSize = 512,
            };
            devpiano::audio::LiveAudioDeviceState liveOnLinuxAlsa {
                .hasLiveDevice = true,
                .backendName = "ALSA",
                .deviceName = "PulseAudio Sound Server",
                .inputDeviceName = "",
                .outputDeviceName = "default",
                .sampleRate = 48000.0,
                .bufferSize = 256,
                .defaultBufferSize = 256,
                .availableBufferSizes = { 128, 256, 512 },
            };

            const auto diag = devpiano::audio::buildAudioDeviceDiagnostics(savedFromWindows, liveOnLinuxAlsa);
            expectEquals(diag.restoreOutcome, juce::String("fallback suspected"),
                         "Cross-platform migration to ALSA should identify fallback");
            expect(diag.mismatchReasons.contains("backend"), "Should identify backend mismatch");
            expect(diag.mismatchReasons.contains("output device"), "Should identify output device mismatch");
        }

        beginTest("Linux buffer size and sample rate negotiation calculations");
        {
            // Low latency Linux buffer (e.g. 64 or 128 samples under JACK / ALSA)
            const auto warmup64 = AudioEngine::calculateWarmupBlockCount(48000.0, 64);
            expectGreaterThan(warmup64, 0, "Warmup block count must be positive for 64-sample buffer");

            const auto preroll64 = AudioEngine::calculatePlaybackStartPreRollBlockCount(48000.0, 64);
            expectGreaterThan(preroll64, 0, "Preroll block count must be positive for 64-sample buffer");

            // High sample rate (96kHz / 192kHz) under Linux pro-audio
            const auto warmup96k = AudioEngine::calculateWarmupBlockCount(96000.0, 256);
            expectGreaterThan(warmup96k, 0, "Warmup block count must be positive for 96kHz");

            // Exercise AudioEngine lifecycle with Linux typical 48kHz / 128 samples
            AudioEngine engine;
            engine.prepareToPlay(128, 48000.0);
            auto buf = makeBlock(2, 128);
            const juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
            engine.getNextAudioBlock(info);
            expectEquals(buf.getNumSamples(), 128);
            engine.releaseResources();
        }
    }
};

static AudioDeviceDiagnosticsLinuxTest audioDeviceDiagnosticsLinuxTest;

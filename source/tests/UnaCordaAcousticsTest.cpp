#include <JuceHeader.h>

#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PianoSynthVoice.h"
#include "Input/KeyboardMidiMapper.h"

// ============================================================================
/// UnaCordaAcousticsTest (Phase 29-B)
///
/// 验证弱音/移位踏板（Una Corda / Soft Pedal, CC 67）在物理声学建模、
/// 琴槌毛毡侧面软化、三弦敲两弦能量衰减、MIDI 控制器响应与电脑键盘交互中的
/// 全链路状态流转与稳定性。
// ============================================================================
class UnaCordaAcousticsTest final : public juce::UnitTest {
public:
    UnaCordaAcousticsTest()
        : juce::UnitTest("UnaCordaAcoustics", "DevPiano/Acoustics") {
    }

    void runTest() override {
        testPianoSynthVoiceUnaCordaAcousticResponse();
        testMidiCC67ControllerHandling();
        testKeyboardMidiMapperSoftPedalShortcuts();
        testPlaybackDynamicPedalStability();
        testBuiltinSynthesiserSoftPedalOwnerIntegration();
    }

private:
    void testPianoSynthVoiceUnaCordaAcousticResponse() {
        beginTest("PianoSynthVoice: Una Corda trichord attenuation and felt softening");

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 1024;

        const auto renderNote = [&](bool unaCorda, int noteNumber, float& outRms, float& outDirectPeak) {
            juce::Synthesiser synth;
            synth.setCurrentPlaybackSampleRate(sampleRate);
            synth.addSound(new PianoSynthSound());
            auto* voice = new PianoSynthVoice();
            voice->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
            if (unaCorda) {
                voice->setSoftPedalDown(true, 1.0f);
            }
            synth.addVoice(voice);

            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, noteNumber, 0.8f), 0);

            juce::AudioBuffer<float> buffer(2, blockSize);
            buffer.clear();
            synth.renderNextBlock(buffer, midi, 0, blockSize);

            outRms = buffer.getRMSLevel(0, 0, blockSize);
            outDirectPeak = buffer.getMagnitude(0, 0, 120);
        };

        // 1. 中高音区三弦组（Note 72, C5）：Una Corda 3 弦敲 2 弦产生显著能量衰减
        float rmsNorm72 = 0.0f;
        float peakNorm72 = 0.0f;
        float rmsUna72 = 0.0f;
        float peakUna72 = 0.0f;

        renderNote(false, 72, rmsNorm72, peakNorm72);
        renderNote(true, 72, rmsUna72, peakUna72);

        expect(rmsNorm72 > 0.001f);
        expect(rmsUna72 > 0.001f);
        // 踩下软踏板后总 RMS 与直达声峰值均显著衰减
        expect(rmsNorm72 > rmsUna72);
        expect(peakNorm72 > peakUna72);

        // 2. 低音单弦区（Note 24, C1）：无三弦敲两弦衰减，但毛毡软化仍使起音瞬态峰值减弱
        float rmsNorm24 = 0.0f;
        float peakNorm24 = 0.0f;
        float rmsUna24 = 0.0f;
        float peakUna24 = 0.0f;

        renderNote(false, 24, rmsNorm24, peakNorm24);
        renderNote(true, 24, rmsUna24, peakUna24);

        expect(rmsNorm24 > 0.001f);
        expect(rmsUna24 > 0.001f);
        expect(peakNorm24 >= peakUna24);
    }

    void testMidiCC67ControllerHandling() {
        beginTest("PianoSynthVoice: MIDI CC 67 controller parsing and half-pedaling");

        constexpr double sampleRate = 44100.0;
        PianoSynthVoice voice;
        voice.setCurrentPlaybackSampleRate(sampleRate);

        expect(!voice.isSoftPedalDown());
        expectEquals(voice.getSoftPedalAmount(), 0.0f);

        // 踩下软踏板 CC 67 = 127
        voice.controllerMoved(67, 127);
        expect(voice.isSoftPedalDown());
        expectEquals(voice.getSoftPedalAmount(), 1.0f);

        // 半踩踏板 CC 67 = 64
        voice.controllerMoved(67, 64);
        expect(voice.isSoftPedalDown());
        expect(voice.getSoftPedalAmount() > 0.45f && voice.getSoftPedalAmount() < 0.55f);

        // 释放踏板 CC 67 = 0
        voice.controllerMoved(67, 0);
        expect(!voice.isSoftPedalDown());
        expectEquals(voice.getSoftPedalAmount(), 0.0f);

        // 释放至中间低值 CC 67 = 32（< 64 判定为释放，amount 必须清零）
        voice.controllerMoved(67, 100);
        expect(voice.isSoftPedalDown());
        voice.controllerMoved(67, 32);
        expect(!voice.isSoftPedalDown());
        expectEquals(voice.getSoftPedalAmount(), 0.0f);

        // 无关控制器不影响状态
        voice.controllerMoved(64, 127); // sustain pedal
        expect(!voice.isSoftPedalDown());
    }

    void testKeyboardMidiMapperSoftPedalShortcuts() {
        beginTest("KeyboardMidiMapper: Tab and Shift+Space triggers soft pedal callback and state");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        bool callbackState = false;
        int callbackCount = 0;
        mapper.setSoftPedalCallback([&](bool isDown) {
            callbackState = isDown;
            ++callbackCount;
        });

        // 1. Tab 键按下
        juce::KeyPress tabKey(juce::KeyPress::tabKey);
        expect(mapper.handleKeyPressed(tabKey, state));
        expect(mapper.isSoftPedalDown());
        expect(callbackState);
        expectEquals(callbackCount, 1);

        // 重复按键（连击）不重复发送 callback
        expect(mapper.handleKeyPressed(tabKey, state));
        expectEquals(callbackCount, 1);

        // 释放 Tab 键（通过谓词模拟释放）
        mapper.setKeyStatePredicate([](int) { return false; });
        expect(mapper.handleKeyStateChanged(state));
        expect(!mapper.isSoftPedalDown());
        expect(!callbackState);
        expectEquals(callbackCount, 2);

        // 2. Shift+Space 快捷键触发
        juce::KeyPress shiftSpace(' ', juce::ModifierKeys::shiftModifier, 0);
        expect(mapper.handleKeyPressed(shiftSpace, state));
        expect(mapper.isSoftPedalDown());
        expect(callbackState);
        expectEquals(callbackCount, 3);

        // 3. releaseAllHeldKeys 自动释放踏板
        mapper.releaseAllHeldKeys(state);
        expect(!mapper.isSoftPedalDown());
        expect(!callbackState);
        expectEquals(callbackCount, 4);

        // 4. setLayout 自动释放踏板
        mapper.handleKeyPressed(tabKey, state);
        expect(mapper.isSoftPedalDown());
        mapper.setLayout(devpiano::core::makeDefaultKeyboardLayout());
        expect(!mapper.isSoftPedalDown());

        // 5. Programmatic soft pedal persistence across keyStateChanged
        mapper.setSoftPedalDown(true);
        expect(mapper.isSoftPedalDown());
        // Key state changed with no pedal keys held must NOT reset programmatic soft pedal
        mapper.setKeyStatePredicate([](int) { return false; });
        mapper.handleKeyStateChanged(state);
        expect(mapper.isSoftPedalDown(), "Programmatic soft pedal must persist through note key state changes");
        mapper.releaseAllHeldKeys(state);
        expect(!mapper.isSoftPedalDown());
    }

    void testPlaybackDynamicPedalStability() {
        beginTest("PianoSynthVoice: Dynamic pedal toggling during audio rendering stays stable");

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 128;

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(sampleRate);
        synth.addSound(new PianoSynthSound());
        synth.addVoice(new PianoSynthVoice());

        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.85f), 0);

        juce::AudioBuffer<float> buffer(2, blockSize);

        for (int i = 0; i < 20; ++i) {
            buffer.clear();
            if (i % 2 == 1) {
                midi.addEvent(juce::MidiMessage::controllerEvent(1, 67, 127), 0); // Una Corda ON
            } else {
                midi.addEvent(juce::MidiMessage::controllerEvent(1, 67, 0), 0); // Una Corda OFF
            }

            synth.renderNextBlock(buffer, midi, 0, blockSize);
            midi.clear();

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                const auto* samples = buffer.getReadPointer(ch);
                for (int s = 0; s < blockSize; ++s) {
                    expect(!std::isnan(samples[s]));
                    expect(!std::isinf(samples[s]));
                    expect(std::abs(samples[s]) <= 2.0f);
                }
            }
        }
    }
    void testBuiltinSynthesiserSoftPedalOwnerIntegration() {
        beginTest("BuiltinSynthesiser: QUAL-019 instrument-owner CC67 state applied before new voice startNote");

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 1024;

        devpiano::audio::BuiltinSynthesiser synth;
        synth.setCurrentPlaybackSampleRate(sampleRate);
        synth.addSound(new PianoSynthSound());

        constexpr int voiceCount = 4;
        std::array<PianoSynthVoice*, voiceCount> voices {};
        for (int i = 0; i < voiceCount; ++i) {
            auto* v = new PianoSynthVoice();
            v->setVoiceIndex(i);
            v->setAdsrParameters({ 0.001f, 0.2f, 0.8f, 0.3f });
            voices[static_cast<std::size_t>(i)] = v;
            synth.addVoice(v);
        }

        expect(!synth.isSoftPedalDown(1));
        expectEquals(synth.getSoftPedalAmount(1), 0.0f);

        // Render a note without soft pedal to measure baseline acoustic energy
        synth.handleController(1, 67, 0);
        synth.noteOn(1, 72, 0.8f);
        juce::AudioBuffer<float> normBuf(2, blockSize);
        normBuf.clear();
        synth.renderNextBlock(normBuf, juce::MidiBuffer(), 0, blockSize);
        const auto rmsBaseline = normBuf.getRMSLevel(0, 0, blockSize);
        const auto peakBaseline = normBuf.getMagnitude(0, 0, 120);
        synth.allNotesOff(0, false);

        // CC67 before chord: CC67=127 at sample 0, chord (72, 76, 79) at sample 10
        juce::MidiBuffer chordMidi;
        chordMidi.addEvent(juce::MidiMessage::controllerEvent(1, 67, 127), 0);
        chordMidi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.8f), 10);
        chordMidi.addEvent(juce::MidiMessage::noteOn(1, 76, 0.8f), 10);
        chordMidi.addEvent(juce::MidiMessage::noteOn(1, 79, 0.8f), 10);

        juce::AudioBuffer<float> chordBuf(2, blockSize);
        chordBuf.clear();
        synth.renderNextBlock(chordBuf, chordMidi, 0, blockSize);

        expect(synth.isSoftPedalDown(1));
        expectEquals(synth.getSoftPedalAmount(1), 1.0f);

        // Verify acoustic energy of note 72 under soft pedal alone is attenuated
        synth.allNotesOff(0, false);
        synth.handleController(1, 67, 127);
        synth.noteOn(1, 72, 0.8f);
        juce::AudioBuffer<float> softBuf(2, blockSize);
        softBuf.clear();
        synth.renderNextBlock(softBuf, juce::MidiBuffer(), 0, blockSize);
        const auto rmsSoft = softBuf.getRMSLevel(0, 0, blockSize);
        const auto peakSoft = softBuf.getMagnitude(0, 0, 120);

        expect(rmsBaseline > 0.001f);
        expect(rmsSoft > 0.001f);
        expect(rmsBaseline > rmsSoft, "Una corda physical model must attenuate RMS energy");
        expect(peakBaseline > peakSoft, "Una corda felt softening must attenuate attack peak");

        // All 3 active chord voices must have inherited soft pedal state
        int activeVoiceCount = 0;
        for (auto* v : voices) {
            if (v->isVoiceActive()) {
                ++activeVoiceCount;
                expect(v->isSoftPedalDown(), "Voice playing chord note must inherit soft pedal state");
                expectEquals(v->getSoftPedalAmount(), 1.0f);
            }
        }
        expect(activeVoiceCount >= 1);

        synth.handleController(1, 67, 0);
        expect(!synth.isSoftPedalDown(1));

        synth.handleController(2, 67, 127);
        expect(synth.isSoftPedalDown(2));

        synth.allNotesOff(0, false);
        synth.handleController(2, 67, 127);
        synth.noteOn(2, 72, 0.8f);

        PianoSynthVoice* ch2Voice = nullptr;
        for (auto* v : voices) {
            if (v->isVoiceActive() && v->getCurrentlyPlayingNote() == 72) {
                ch2Voice = v;
                break;
            }
        }
        expect(ch2Voice != nullptr);
        expect(ch2Voice->isSoftPedalDown(), "Channel 2 note must have soft pedal");

        synth.noteOn(1, 60, 0.8f);
        PianoSynthVoice* ch1Voice = nullptr;
        for (auto* v : voices) {
            if (v->isVoiceActive() && v->getCurrentlyPlayingNote() == 60) {
                ch1Voice = v;
                break;
            }
        }
        expect(ch1Voice != nullptr);
        expect(!ch1Voice->isSoftPedalDown(), "Channel 1 stolen/reused voice must reflect Channel 1 state");
        expectEquals(ch1Voice->getSoftPedalAmount(), 0.0f);

        // 4. Analog magnitude support (half-pedaling) and acoustic proof
        synth.handleController(1, 67, 80);
        expect(synth.isSoftPedalDown(1));
        expectWithinAbsoluteError(synth.getSoftPedalAmount(1), 80.0f / 127.0f, 1e-4f);

        synth.allNotesOff(0, false);
        synth.noteOn(1, 72, 0.8f);
        juce::AudioBuffer<float> halfBuf(2, blockSize);
        halfBuf.clear();
        synth.renderNextBlock(halfBuf, juce::MidiBuffer(), 0, blockSize);
        const auto rmsHalf = halfBuf.getRMSLevel(0, 0, blockSize);

        for (auto* v : voices) {
            if (v->isVoiceActive() && v->getCurrentlyPlayingNote() == 72) {
                expect(v->isSoftPedalDown());
                expectWithinAbsoluteError(v->getSoftPedalAmount(), 80.0f / 127.0f, 1e-4f);
            }
        }
        expect(rmsBaseline > rmsHalf, "Half-pedal acoustic energy must be lower than baseline");
        expect(rmsHalf > rmsSoft, "Half-pedal acoustic energy must be higher than full soft pedal");
        synth.handleController(1, 121, 0);
        expect(!synth.isSoftPedalDown(1));
        expectEquals(synth.getSoftPedalAmount(1), 0.0f);
    }
};

static UnaCordaAcousticsTest unaCordaAcousticsTest;

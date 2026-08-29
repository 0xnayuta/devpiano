#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
// 内置 fallback 正弦合成器：实时路径（AudioEngine）与离线 WAV 导出路径
// （WavFileExporter）共用同一实现，保证两路径音色一致（Phase 12-1）。
// 继承 juce::SynthesiserVoice，由 juce::Synthesiser 管理 voice 生命周期。

class SineSynthSound final : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override {
        return true;
    }
    bool appliesToChannel(int) override {
        return true;
    }
};

class SineSynthVoice final : public juce::SynthesiserVoice {
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<SineSynthSound*>(sound) != nullptr;
    }

    void setAdsrParameters(const juce::ADSR::Parameters& parameters) {
        // addVoice 会用 synth 的当前 sampleRate 覆盖 voice（构造期为 0），
        // 采样率无效时跳过，ADSR 内部默认 44100 无断言。
        if (getSampleRate() > 0.0) {
            adsr.setSampleRate(getSampleRate());
        }
        adsr.setParameters(parameters);
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        const auto sampleRate = getSampleRate();
        if (sampleRate <= 0.0) {
            return;
        }

        level = velocity * 0.50f;
        frequency = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
        phase = 0.0;
        increment
            = static_cast<float>(juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / sampleRate);

        adsr.setSampleRate(sampleRate);
        adsr.noteOn();
    }

    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) {
            adsr.noteOff();
            return;
        }

        adsr.reset();
        clearCurrentNote();
    }

    void pitchWheelMoved(int) override {
    }
    void controllerMoved(int, int) override {
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        if (!isVoiceActive()) {
            return;
        }

        for (auto sample = 0; sample < numSamples; ++sample) {
            const auto envelope = adsr.getNextSample();
            if (envelope <= 0.0f && !adsr.isActive()) {
                clearCurrentNote();
                break;
            }

            const auto value = static_cast<float>(std::sin(phase) * level * envelope);
            phase += increment;
            if (phase >= juce::MathConstants<double>::twoPi) {
                phase -= juce::MathConstants<double>::twoPi;
            }

            const auto sampleIndex = startSample + sample;
            for (auto channel = 0; channel < outputBuffer.getNumChannels(); ++channel) {
                outputBuffer.addSample(channel, sampleIndex, value);
            }
        }
    }

private:
    double phase = 0.0;
    float increment = 0.0f;
    float frequency = 440.0f;
    float level = 0.0f;
    juce::ADSR adsr;
};

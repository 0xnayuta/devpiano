#include <JuceHeader.h>

#include "Audio/PianoSynthVoice.h"
#include "Audio/TemperamentEngine.h"

// ============================================================================
/// PianoSynthVoiceTemperamentTest (Phase 30-B)
///
// ============================================================================
class PianoSynthVoiceTemperamentTest final : public juce::UnitTest {
public:
    PianoSynthVoiceTemperamentTest()
        : juce::UnitTest("PianoSynthVoiceTemperament", "DevPiano/Audio") {
    }

    void runTest() override {
        testVoiceDynamicTuningSwitching();
    }

private:
    void testVoiceDynamicTuningSwitching() {
        beginTest("PianoSynthVoice: Dynamic temperament switching during playback renders cleanly");

        using namespace devpiano::audio;

        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 512;

        juce::Synthesiser synth;
        synth.setCurrentPlaybackSampleRate(sampleRate);
        synth.addSound(new PianoSynthSound());

        auto* voice = new PianoSynthVoice();
        voice->setTemperament(Temperament::equal);
        voice->setReferencePitchA4(440.0);
        synth.addVoice(voice);

        // Trigger middle C
        synth.noteOn(1, 60, 0.8f);

        juce::AudioBuffer<float> buffer(2, blockSize);

        // Render standard block
        buffer.clear();
        synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, blockSize);
        expectGreaterThan(buffer.getMagnitude(0, blockSize), 0.0f);

        // Dynamically switch temperament and reference pitch while note is ringing
        voice->setTemperament(Temperament::meantone);
        voice->setReferencePitchA4(415.0);

        // Render subsequent blocks to ensure numerical stability (no NaN, no Inf)
        for (int b = 0; b < 10; ++b) {
            buffer.clear();
            synth.renderNextBlock(buffer, juce::MidiBuffer(), 0, blockSize);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                const auto* readPtr = buffer.getReadPointer(ch);
                for (int s = 0; s < blockSize; ++s) {
                    expect(!std::isnan(readPtr[s]));
                    expect(!std::isinf(readPtr[s]));
                    expectLessThan(std::abs(readPtr[s]), 10.0f);
                }
            }
        }

        synth.allNotesOff(1, false);
    }
};

static PianoSynthVoiceTemperamentTest pianoSynthVoiceTemperamentTest;

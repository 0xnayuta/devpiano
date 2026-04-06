#pragma once

#include <JuceHeader.h>

class MidiRouter : public juce::MidiInputCallback
{
public:
    MidiRouter() = default;
    ~MidiRouter() override { closeInputs(); }

    void openInputs();
    void closeInputs();

    void setCollector(juce::MidiMessageCollector* c) { collector = c; }

    // MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    juce::MidiMessageCollector* collector = nullptr;
    std::vector<std::unique_ptr<juce::MidiInput>> inputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiRouter)
};

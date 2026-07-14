#pragma once

#include <JuceHeader.h>
#include <functional>

using MessageTransformer = std::function<juce::MidiMessage(const juce::MidiMessage&)>;

class MidiRouter : public juce::MidiInputCallback {

public:
    MidiRouter() = default;
    ~MidiRouter() override;

    void setCollector(juce::MidiMessageCollector* newCollector) noexcept;
    void setMessageCallback(std::function<void(const juce::MidiMessage&)> callback);

    int openAllInputs();
    void setTransformer(MessageTransformer transformer);
    void closeInputs();
    int getOpenInputCount() const noexcept;

    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    MessageTransformer transformer;
    juce::MidiMessageCollector* collector = nullptr;
    std::function<void(const juce::MidiMessage&)> onMessage;
    std::vector<std::unique_ptr<juce::MidiInput>> inputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiRouter)
};

#include "MidiRouter.h"

void MidiRouter::openInputs()
{
    closeInputs();

    auto devices = juce::MidiInput::getAvailableDevices();
    for (auto& d : devices) {
        auto input = juce::MidiInput::openDevice(d.identifier, this);
        if (input) {
            inputs.emplace_back(std::move(input));
            inputs.back()->start();
        }
    }
}

void MidiRouter::closeInputs()
{
    for (auto& in : inputs) {
        if (in) in->stop();
    }
    inputs.clear();
}

void MidiRouter::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (collector) {
        collector->addMessageToQueue(message);
    }
}

#include "MidiRouter.h"

MidiRouter::~MidiRouter()
{
    closeInputs();
}

void MidiRouter::setCollector(juce::MidiMessageCollector* newCollector) noexcept
{
    collector = newCollector;
}

void MidiRouter::setMessageCallback(std::function<void(const juce::MidiMessage&)> callback)
{
    onMessage = std::move(callback);
}

int MidiRouter::openAllInputs()
{
    closeInputs();

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (auto input = juce::MidiInput::openDevice(device.identifier, this))
        {
            input->start();
            inputs.push_back(std::move(input));
        }
    }

    return getOpenInputCount();
}

void MidiRouter::closeInputs()
{
    for (auto& input : inputs)
        if (input != nullptr)
            input->stop();

    inputs.clear();
}

int MidiRouter::getOpenInputCount() const noexcept
{
    return static_cast<int>(inputs.size());
}

void MidiRouter::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    juce::ignoreUnused(source);

    if (collector != nullptr)
        collector->addMessageToQueue(message);

    if (onMessage)
        onMessage(message);
}

#pragma once

#include <JuceHeader.h>
#include "SettingsModel.h"

class SettingsStore
{
public:
    SettingsStore();

    void load(SettingsModel& model);
    void save(const SettingsModel& model);

    // Debounced save helper (call on UI thread)
    void scheduleSave(const SettingsModel& model, int msDelay = 300);

private:
    std::unique_ptr<juce::ApplicationProperties> appProps;
    std::unique_ptr<juce::Timer> saverTimer; // created lazily

    void ensureProps();
    juce::PropertiesFile& file();

    void writeNow(const SettingsModel& model);
    void readNow(SettingsModel& model);
};

#pragma once

#include "SettingsModel.h"
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
class SettingsStore;

// Debounced save helper backing SettingsStore::scheduleSave.
//
// Extracted from the .cpp-local implementation so unit tests can drive
// timerCallback() directly (juce::Timer::timerCallback is public) and verify
// the merge semantics without a running message loop.
class SettingsDebounceTimer final : public juce::Timer {
public:
    explicit SettingsDebounceTimer(SettingsStore& store);

    void setPayload(const SettingsModel& model);
    void start(int ms);

    void timerCallback() override;

private:
    SettingsStore& store;
    const SettingsModel* modelPtr = nullptr;
};

class SettingsStore {
public:
    // `options` overrides the storage location; leave empty to use the
    // production location (user application-data directory).  Tests inject a
    // temporary-directory options set to avoid touching real user settings.
    explicit SettingsStore(juce::PropertiesFile::Options options = {});

    // Explicit file constructor (for isolated storage and test harnesses).
    // Directly binds to customFile without relying on ApplicationProperties folder defaults.
    explicit SettingsStore(const juce::File& customFile);

    void load(SettingsModel& model);
    // Persists synchronously; false means the write failed (caller logs the
    // path — see writeNow's DP_LOG_ERROR).
    bool save(const SettingsModel& model);

    // Debounced save helper (call on UI thread)
    void scheduleSave(const SettingsModel& model, int msDelay = 300);

private:
    juce::PropertiesFile::Options storedOptions;
    juce::File customFile;
    std::unique_ptr<juce::ApplicationProperties> appProps;
    std::unique_ptr<juce::PropertiesFile> customPropsFile;
    std::unique_ptr<SettingsDebounceTimer> saverTimer; // created lazily

    void ensureProps();
    juce::PropertiesFile& file();

    bool writeNow(const SettingsModel& model);
    void readNow(SettingsModel& model);
};

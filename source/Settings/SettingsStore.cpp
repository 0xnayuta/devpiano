#include "SettingsStore.h"

namespace {
    const char* kSectionApp = "DevPiano";
    const char* kKeyAudioXml = "audioDeviceXml";
    const char* kKeySampleRate = "sampleRate";
    const char* kKeyBufferSize = "bufferSize";
    const char* kKeyGain = "masterGain";
    const char* kKeyA = "adsrAttack";
    const char* kKeyD = "adsrDecay";
    const char* kKeyS = "adsrSustain";
    const char* kKeyR = "adsrRelease";
    const char* kKeyPluginSearchPath = "pluginSearchPath";
    const char* kKeyLastPluginName = "lastPluginName";
    const char* kKeyKnownPluginListXml = "knownPluginListXml";
    const char* kKeyLastLayoutId = "lastLayoutId";
    const char* kKeyMap = "keymapVT"; // stored as ValueTree XML
    const char* kKeyLastMidiImportPath = "lastMidiImportPath";
    const char* kKeyLastMidiExportPath = "lastMidiExportPath";
    const char* kKeyMainWindowWidth = "mainWindowWidth";
    const char* kKeyMainWindowHeight = "mainWindowHeight";

    [[nodiscard]] SettingsModel::PerformanceSettingsView makeDefaultPerformanceSettings() noexcept
    {
        return {};
    }

    void readPerformanceSettings(juce::PropertiesFile& file, SettingsModel& model)
    {
        auto performance = SettingsModel::PerformanceSettingsView {
            .masterGain = static_cast<float>(file.getDoubleValue(kKeyGain, model.masterGain)),
            .adsrAttack = static_cast<float>(file.getDoubleValue(kKeyA, model.adsrAttack)),
            .adsrDecay = static_cast<float>(file.getDoubleValue(kKeyD, model.adsrDecay)),
            .adsrSustain = static_cast<float>(file.getDoubleValue(kKeyS, model.adsrSustain)),
            .adsrRelease = static_cast<float>(file.getDoubleValue(kKeyR, model.adsrRelease))
        };

        const auto looksLikeCorruptedZeroState = performance.masterGain == 0.0f
                                             && performance.adsrAttack == 0.0f
                                             && performance.adsrDecay == 0.0f
                                             && performance.adsrSustain == 0.0f
                                             && performance.adsrRelease == 0.0f;
        if (looksLikeCorruptedZeroState)
            performance = makeDefaultPerformanceSettings();

        performance.masterGain = juce::jlimit(0.0f, 1.0f, performance.masterGain);
        performance.adsrAttack = juce::jlimit(0.001f, 2.0f, performance.adsrAttack);
        performance.adsrDecay = juce::jlimit(0.001f, 2.0f, performance.adsrDecay);
        performance.adsrSustain = juce::jlimit(0.0f, 1.0f, performance.adsrSustain);
        performance.adsrRelease = juce::jlimit(0.001f, 3.0f, performance.adsrRelease);

        model.applyPerformanceSettingsView(performance);
    }
}

SettingsStore::SettingsStore() = default;

void SettingsStore::ensureProps()
{
    if (appProps) return;
    juce::PropertiesFile::Options opts;
    opts.applicationName     = kSectionApp;
    opts.filenameSuffix      = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    opts.commonToAllUsers    = false;
    opts.storageFormat       = juce::PropertiesFile::storeAsXML;
    appProps = std::make_unique<juce::ApplicationProperties>();
    appProps->setStorageParameters(opts);
}

juce::PropertiesFile& SettingsStore::file()
{
    ensureProps();
    return *appProps->getUserSettings();
}

void SettingsStore::readNow(SettingsModel& m)
{
    auto& f = file();

    // audio device xml
    {
        std::unique_ptr<juce::XmlElement> xml = f.getXmlValue(kKeyAudioXml);
        if (xml)
            m.audioDeviceState = std::move(xml);
    }

    m.sampleRate = f.getDoubleValue(kKeySampleRate, m.sampleRate);
    m.bufferSize = f.getIntValue(kKeyBufferSize, m.bufferSize);

    readPerformanceSettings(f, m);

    m.pluginSearchPath = f.getValue(kKeyPluginSearchPath, m.pluginSearchPath);
    m.lastPluginName = f.getValue(kKeyLastPluginName, m.lastPluginName);
    m.knownPluginListState = f.getXmlValue(kKeyKnownPluginListXml);
    m.lastLayoutId = f.getValue(kKeyLastLayoutId, m.lastLayoutId);

    // MIDI import/export paths
    m.lastMidiImportPath = f.getValue(kKeyLastMidiImportPath, m.lastMidiImportPath);
    m.lastMidiExportPath = f.getValue(kKeyLastMidiExportPath, m.lastMidiExportPath);

    // Main content size
    m.mainWindowWidth = f.getIntValue(kKeyMainWindowWidth, m.mainWindowWidth);
    m.mainWindowHeight = f.getIntValue(kKeyMainWindowHeight, m.mainWindowHeight);

    // keymap as ValueTree XML
    if (auto keyXml = f.getXmlValue(kKeyMap))
    {
        juce::ValueTree t = juce::ValueTree::fromXml(*keyXml);
        m.keyMap = SettingsModel::valueTreeToKeyMap(t);
    }
}

void SettingsStore::writeNow(const SettingsModel& m)
{
    auto& f = file();

    if (m.audioDeviceState)
        f.setValue(kKeyAudioXml, m.audioDeviceState->toString());

    f.setValue(kKeySampleRate, m.sampleRate);
    f.setValue(kKeyBufferSize, m.bufferSize);

    f.setValue(kKeyGain, m.masterGain);
    f.setValue(kKeyA, m.adsrAttack);
    f.setValue(kKeyD, m.adsrDecay);
    f.setValue(kKeyS, m.adsrSustain);
    f.setValue(kKeyR, m.adsrRelease);

    f.setValue(kKeyPluginSearchPath, m.pluginSearchPath);
    f.setValue(kKeyLastPluginName, m.lastPluginName);
    if (m.knownPluginListState)
        f.setValue(kKeyKnownPluginListXml, m.knownPluginListState->toString());
    f.setValue(kKeyLastLayoutId, m.lastLayoutId);

    // MIDI import/export paths
    f.setValue(kKeyLastMidiImportPath, m.lastMidiImportPath);
    f.setValue(kKeyLastMidiExportPath, m.lastMidiExportPath);

    // Main content size
    f.setValue(kKeyMainWindowWidth, m.mainWindowWidth);
    f.setValue(kKeyMainWindowHeight, m.mainWindowHeight);

    // keymap serialize to ValueTree XML
    {
        auto t = SettingsModel::keyMapToValueTree(m.keyMap);
        if (auto xml = t.createXml())
            f.setValue(kKeyMap, xml->toString());
    }

    f.saveIfNeeded();
}

void SettingsStore::load(SettingsModel& model)
{
    readNow(model);
}

void SettingsStore::save(const SettingsModel& model)
{
    writeNow(model);
}

void SettingsStore::scheduleSave(const SettingsModel& model, int msDelay)
{
    class DebounceTimer : public juce::Timer
    {
    public:
        DebounceTimer(SettingsStore& s) : store(s) {}
        void setPayload(const SettingsModel& m) { modelPtr = &m; }
        void start(int ms) { startTimer(ms); }
        void timerCallback() override { stopTimer(); if (modelPtr) store.save(*modelPtr); }
    private:
        SettingsStore& store;
        const SettingsModel* modelPtr = nullptr;
    };

    if (! saverTimer)
        saverTimer.reset(new DebounceTimer(*this));

    auto* t = static_cast<DebounceTimer*>(saverTimer.get());
    t->setPayload(model);
    t->start(msDelay);
}

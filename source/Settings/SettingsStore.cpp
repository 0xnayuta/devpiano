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

    m.masterGain = (float) f.getDoubleValue(kKeyGain, m.masterGain);
    m.adsrAttack = (float) f.getDoubleValue(kKeyA, m.adsrAttack);
    m.adsrDecay  = (float) f.getDoubleValue(kKeyD, m.adsrDecay);
    m.adsrSustain= (float) f.getDoubleValue(kKeyS, m.adsrSustain);
    m.adsrRelease= (float) f.getDoubleValue(kKeyR, m.adsrRelease);

    m.pluginSearchPath = f.getValue(kKeyPluginSearchPath, m.pluginSearchPath);
    m.lastPluginName = f.getValue(kKeyLastPluginName, m.lastPluginName);
    m.knownPluginListState = f.getXmlValue(kKeyKnownPluginListXml);
    m.lastLayoutId = f.getValue(kKeyLastLayoutId, m.lastLayoutId);

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

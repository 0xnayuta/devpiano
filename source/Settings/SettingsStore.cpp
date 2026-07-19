#include "SettingsStore.h"
#include "Settings/SettingsSerialization.h"

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
const char* kKeyRecentFiles = "recentFiles";
const char* kKeyMainWindowWidth = "mainWindowWidth";
const char* kKeyMainWindowHeight = "mainWindowHeight";
const char* kKeyColourMode = "keyboardColourMode";
const char* kKeyNoteDisplay = "keyboardNoteDisplay";
const char* kKeyFadeSpeed = "keyboardFadeSpeed";
const char* kKeyResizableWindow = "resizableWindow";
const char* kKeyShowInstrumentFilter = "showInstrumentFilter";
const char* kKeyChannelMatrix = "channelMatrix";
const char* kKeyLanguageCode = "languageCode";
const char* kKeyCustomLabels = "customKeyLabels";
const char* kKeyCustomColours = "customKeyColours";
const char* kKeyKeySignature = "keySignature";
const char* kKeyMidiTranspose = "midiTranspose";

[[nodiscard]] SettingsModel::PerformanceSettingsView makeDefaultPerformanceSettings() noexcept {
    return {};
}

void readPerformanceSettings(juce::PropertiesFile& file, SettingsModel& model) {
    auto performance = SettingsModel::PerformanceSettingsView {
        .masterGain = static_cast<float>(file.getDoubleValue(kKeyGain, model.masterGain)),
        .adsrAttack = static_cast<float>(file.getDoubleValue(kKeyA, model.adsrAttack)),
        .adsrDecay = static_cast<float>(file.getDoubleValue(kKeyD, model.adsrDecay)),
        .adsrSustain = static_cast<float>(file.getDoubleValue(kKeyS, model.adsrSustain)),
        .adsrRelease = static_cast<float>(file.getDoubleValue(kKeyR, model.adsrRelease))
    };

    const auto looksLikeCorruptedZeroState = performance.masterGain == 0.0f && performance.adsrAttack == 0.0f
        && performance.adsrDecay == 0.0f && performance.adsrSustain == 0.0f && performance.adsrRelease == 0.0f;
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

void SettingsStore::ensureProps() {
    if (appProps)
        return;
    juce::PropertiesFile::Options opts;
    opts.applicationName = kSectionApp;
    opts.filenameSuffix = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    opts.commonToAllUsers = false;
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    appProps = std::make_unique<juce::ApplicationProperties>();
    appProps->setStorageParameters(opts);
}

juce::PropertiesFile& SettingsStore::file() {
    ensureProps();
    return *appProps->getUserSettings();
}

void SettingsStore::readNow(SettingsModel& m) {
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

    // Recently-opened files list
    m.recentFilesSerialized = f.getValue(kKeyRecentFiles, m.recentFilesSerialized);

    // Main content size
    m.mainWindowWidth = f.getIntValue(kKeyMainWindowWidth, m.mainWindowWidth);
    m.mainWindowHeight = f.getIntValue(kKeyMainWindowHeight, m.mainWindowHeight);

    // Keyboard display settings
    {
        int cm = f.getIntValue(kKeyColourMode, static_cast<int>(m.keyboardColourMode));
        m.keyboardColourMode = static_cast<devpiano::ui::KeyColourMode>(cm);
    }
    {
        int nd = f.getIntValue(kKeyNoteDisplay, static_cast<int>(m.keyboardNoteDisplay));
        m.keyboardNoteDisplay = static_cast<devpiano::ui::NoteDisplayMode>(nd);
    }
    m.keyboardFadeSpeed = static_cast<float>(f.getDoubleValue(kKeyFadeSpeed, static_cast<double>(m.keyboardFadeSpeed)));

    // Channel matrix as ValueTree XML.
    if (auto cmXml = f.getXmlValue(kKeyChannelMatrix)) {
        juce::ValueTree t = juce::ValueTree::fromXml(*cmXml);

        m.channelMatrix = devpiano::settings::valueTreeToChannelMatrix(t);
    }

    m.keySignature = f.getIntValue(kKeyKeySignature, 0);
    m.midiTranspose = f.getBoolValue(kKeyMidiTranspose, false);

    m.resizableWindow = f.getBoolValue(kKeyResizableWindow, m.resizableWindow);
    m.showInstrumentFilter = f.getBoolValue(kKeyShowInstrumentFilter, m.showInstrumentFilter);
    m.languageCode = f.getValue(kKeyLanguageCode, m.languageCode);

    // keymap as ValueTree XML
    if (auto keyXml = f.getXmlValue(kKeyMap)) {
        juce::ValueTree t = juce::ValueTree::fromXml(*keyXml);
        m.keyMap = devpiano::settings::valueTreeToKeyMap(t);
    }

    // custom key labels as ValueTree XML (sparse: only non-empty labels stored)
    if (auto labelsXml = f.getXmlValue(kKeyCustomLabels)) {
        juce::ValueTree t = juce::ValueTree::fromXml(*labelsXml);
        m.customKeyLabels.fill({});
        for (int i = 0; i < t.getNumChildren(); ++i) {
            auto c = t.getChild(i);
            auto note = c.getProperty("note");
            if (note.isInt()) {
                auto n = static_cast<int>(note);
                if (n >= 0 && n < 128)
                    m.customKeyLabels[static_cast<std::size_t>(n)] = c.getProperty("text").toString();
            }
        }
    }

    // custom key colours as ValueTree XML (sparse: only non-transparent colours stored)
    if (auto coloursXml = f.getXmlValue(kKeyCustomColours)) {
        juce::ValueTree t = juce::ValueTree::fromXml(*coloursXml);
        m.customKeyColours.fill(juce::Colour(0x00000000));
        for (int i = 0; i < t.getNumChildren(); ++i) {
            auto c = t.getChild(i);
            auto note = c.getProperty("note");
            if (note.isInt()) {
                auto n = static_cast<int>(note);
                if (n >= 0 && n < 128)
                    m.customKeyColours[static_cast<std::size_t>(n)]
                        = juce::Colour::fromString(c.getProperty("argb").toString());
            }
        }
    }
}

void SettingsStore::writeNow(const SettingsModel& m) {
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

    // Recently-opened files list
    f.setValue(kKeyRecentFiles, m.recentFilesSerialized);

    // Main content size
    f.setValue(kKeyMainWindowWidth, m.mainWindowWidth);
    f.setValue(kKeyMainWindowHeight, m.mainWindowHeight);

    // Keyboard display settings
    f.setValue(kKeyColourMode, static_cast<int>(m.keyboardColourMode));
    f.setValue(kKeyNoteDisplay, static_cast<int>(m.keyboardNoteDisplay));
    f.setValue(kKeyFadeSpeed, m.keyboardFadeSpeed);

    // Channel matrix as ValueTree XML.
    {
        auto t = devpiano::settings::channelMatrixToValueTree(m.channelMatrix);
        if (auto xml = t.createXml())
            f.setValue(kKeyChannelMatrix, xml->toString());
    }

    f.setValue(kKeyKeySignature, m.keySignature);
    f.setValue(kKeyMidiTranspose, m.midiTranspose);

    // keymap serialize to ValueTree XML
    {
        auto t = devpiano::settings::keyMapToValueTree(m.keyMap);
        if (auto xml = t.createXml())
            f.setValue(kKeyMap, xml->toString());
    }

    // custom key labels as ValueTree XML (sparse: only non-empty labels stored)
    {
        juce::ValueTree t("customKeyLabels");
        for (int n = 0; n < 128; ++n) {
            auto& lbl = m.customKeyLabels[static_cast<std::size_t>(n)];
            if (lbl.isNotEmpty()) {
                auto c = juce::ValueTree("label");
                c.setProperty("note", n, nullptr);
                c.setProperty("text", lbl, nullptr);
                t.appendChild(c, nullptr);
            }
        }
        if (t.getNumChildren() > 0) {
            if (auto xml = t.createXml())
                f.setValue(kKeyCustomLabels, xml->toString());
        } else {
            f.removeValue(kKeyCustomLabels);
        }
    }

    // custom key colours as ValueTree XML (sparse: only non-transparent stored)
    {
        juce::ValueTree t("customKeyColours");
        for (int n = 0; n < 128; ++n) {
            auto& col = m.customKeyColours[static_cast<std::size_t>(n)];
            if (!col.isTransparent()) {
                auto c = juce::ValueTree("colour");
                c.setProperty("note", n, nullptr);
                c.setProperty("argb", col.toString(), nullptr);
                t.appendChild(c, nullptr);
            }
        }
        if (t.getNumChildren() > 0) {
            if (auto xml = t.createXml())
                f.setValue(kKeyCustomColours, xml->toString());
        } else {
            f.removeValue(kKeyCustomColours);
        }
    }
    f.setValue(kKeyResizableWindow, m.resizableWindow);
    f.setValue(kKeyLanguageCode, m.languageCode);
    f.setValue(kKeyShowInstrumentFilter, m.showInstrumentFilter);

    f.saveIfNeeded();
}

void SettingsStore::load(SettingsModel& model) {
    readNow(model);
}

void SettingsStore::save(const SettingsModel& model) {
    writeNow(model);
}

void SettingsStore::scheduleSave(const SettingsModel& model, int msDelay) {
    class DebounceTimer : public juce::Timer {
    public:
        DebounceTimer(SettingsStore& s)
            : store(s) {
        }
        void setPayload(const SettingsModel& m) {
            modelPtr = &m;
        }
        void start(int ms) {
            startTimer(ms);
        }
        void timerCallback() override {
            stopTimer();
            if (modelPtr)
                store.save(*modelPtr);
        }

    private:
        SettingsStore& store;
        const SettingsModel* modelPtr = nullptr;
    };

    if (!saverTimer)
        saverTimer.reset(new DebounceTimer(*this));

    auto* t = static_cast<DebounceTimer*>(saverTimer.get());
    t->setPayload(model);
    t->start(msDelay);
}

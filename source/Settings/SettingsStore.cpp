#include "SettingsStore.h"
#include "Settings/SettingsSerialization.h"

#include "Diagnostics/Log.h"

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
const char* kKeyBuiltinTone = "builtinTone";
const char* kKeyPianoBrightness = "pianoBrightness";
const char* kKeyPianoHammerHardness = "pianoHammerHardness";
const char* kKeyPianoResonance = "pianoResonance";
const char* kKeyPluginSearchPath = "pluginSearchPath";
const char* kKeyLastPluginName = "lastPluginName";
const char* kKeyKnownPluginListXml = "knownPluginListXml";
const char* kKeyLastActivePresetId = "lastActivePresetId";
const char* kKeyLastMidiImportPath = "lastMidiImportPath";
const char* kKeyLastMidiExportPath = "lastMidiExportPath";
const char* kKeyRecentFiles = "recentFiles";
const char* kKeyMainWindowWidth = "mainWindowWidth";
const char* kKeyMainWindowHeight = "mainWindowHeight";
const char* kKeyColourMode = "keyboardColourMode";
const char* kKeyNoteDisplay = "keyboardNoteDisplay";
const char* kKeyFadeSpeed = "keyboardFadeSpeed";
const char* kKeyShowInstrumentFilter = "showInstrumentFilter";
const char* kKeyChannelMatrix = "channelMatrix";
const char* kKeyLanguageCode = "languageCode";
const char* kKeyCustomLabels = "customKeyLabels";
const char* kKeyCustomColours = "customKeyColours";
const char* kKeyKeySignature = "keySignature";
const char* kKeyMidiTranspose = "midiTranspose";
const char* kKeyKeyboardScrollX = "keyboardScrollX";
const char* kKeyPluginPanelExpanded = "pluginPanelExpanded";

[[nodiscard]] SettingsModel::PerformanceSettingsView makeDefaultPerformanceSettings() noexcept {
    return {};
}

void readPerformanceSettings(juce::PropertiesFile& file, SettingsModel& model) {
    auto performance = SettingsModel::PerformanceSettingsView {
        .masterGain = static_cast<float>(file.getDoubleValue(kKeyGain, model.masterGain)),
        .adsrAttack = static_cast<float>(file.getDoubleValue(kKeyA, model.adsrAttack)),
        .adsrDecay = static_cast<float>(file.getDoubleValue(kKeyD, model.adsrDecay)),
        .adsrSustain = static_cast<float>(file.getDoubleValue(kKeyS, model.adsrSustain)),
        .adsrRelease = static_cast<float>(file.getDoubleValue(kKeyR, model.adsrRelease)),
        .builtinTone = static_cast<SettingsModel::BuiltinTone>(
            file.getIntValue(kKeyBuiltinTone, static_cast<int>(model.builtinTone))),
        .pianoBrightness = static_cast<float>(file.getDoubleValue(kKeyPianoBrightness, model.pianoBrightness)),
        .pianoHammerHardness
        = static_cast<float>(file.getDoubleValue(kKeyPianoHammerHardness, model.pianoHammerHardness)),
        .pianoResonance = static_cast<float>(file.getDoubleValue(kKeyPianoResonance, model.pianoResonance))
    };

    const auto looksLikeCorruptedZeroState = performance.masterGain == 0.0f && performance.adsrAttack == 0.0f
        && performance.adsrDecay == 0.0f && performance.adsrSustain == 0.0f && performance.adsrRelease == 0.0f;
    if (looksLikeCorruptedZeroState) {
        performance = makeDefaultPerformanceSettings();
    }

    performance.masterGain = juce::jlimit(0.0f, 1.0f, performance.masterGain);
    performance.adsrAttack = juce::jlimit(0.001f, 2.0f, performance.adsrAttack);
    performance.adsrDecay = juce::jlimit(0.001f, 2.0f, performance.adsrDecay);
    performance.adsrSustain = juce::jlimit(0.0f, 1.0f, performance.adsrSustain);
    performance.adsrRelease = juce::jlimit(0.001f, 3.0f, performance.adsrRelease);
    // 旧序列化数据缺失字段时回退默认（DOC-006 模式）；越界值钳制。
    const auto rawTone = static_cast<int>(performance.builtinTone);
    performance.builtinTone = (rawTone == static_cast<int>(SettingsModel::BuiltinTone::piano))
        ? SettingsModel::BuiltinTone::piano
        : SettingsModel::BuiltinTone::sine;
    performance.pianoBrightness = juce::jlimit(0.0f, 1.0f, performance.pianoBrightness);
    performance.pianoHammerHardness = juce::jlimit(0.0f, 1.0f, performance.pianoHammerHardness);
    performance.pianoResonance = juce::jlimit(0.0f, 1.0f, performance.pianoResonance);

    model.applyPerformanceSettingsView(performance);
}
}

SettingsStore::SettingsStore(juce::PropertiesFile::Options options)
    : storedOptions(std::move(options)) {
}

SettingsStore::SettingsStore(const juce::File& file)
    : customFile(file) {
}

SettingsDebounceTimer::SettingsDebounceTimer(SettingsStore& s)
    : store(s) {
}

void SettingsDebounceTimer::setPayload(const SettingsModel& m) {
    modelPtr = &m;
}

void SettingsDebounceTimer::start(int ms) {
    startTimer(ms);
}

void SettingsDebounceTimer::timerCallback() {
    stopTimer();
    if (modelPtr) {
        store.save(*modelPtr);
    }
}

void SettingsStore::ensureProps() {
    if (customPropsFile != nullptr || appProps != nullptr) {
        return;
    }

    if (customFile != juce::File {}) {
        juce::PropertiesFile::Options opts = storedOptions;
        opts.storageFormat = juce::PropertiesFile::storeAsXML;
        customPropsFile = std::make_unique<juce::PropertiesFile>(customFile, opts);
        return;
    }

    auto opts = storedOptions;
    if (opts.applicationName.isEmpty()) {
        // Production location: user application-data directory.
        opts.applicationName = kSectionApp;
        opts.filenameSuffix = ".settings";
        opts.osxLibrarySubFolder = "Application Support";
        opts.commonToAllUsers = false;
        opts.storageFormat = juce::PropertiesFile::storeAsXML;
    }
    appProps = std::make_unique<juce::ApplicationProperties>();
    appProps->setStorageParameters(opts);
}

juce::PropertiesFile& SettingsStore::file() {
    ensureProps();
    if (customPropsFile != nullptr) {
        return *customPropsFile;
    }
    if (appProps != nullptr) {
        if (auto* userSettings = appProps->getUserSettings()) {
            return *userSettings;
        }
    }
    static auto fallbackFile = std::make_unique<juce::PropertiesFile>(juce::PropertiesFile::Options {});
    return *fallbackFile;
}

void SettingsStore::readNow(SettingsModel& m) {
    auto& f = file();

    // audio device xml
    {
        std::unique_ptr<juce::XmlElement> xml = f.getXmlValue(kKeyAudioXml);
        if (xml) {
            m.audioDeviceState = std::move(xml);
        }
    }

    m.sampleRate = f.getDoubleValue(kKeySampleRate, m.sampleRate);
    m.bufferSize = f.getIntValue(kKeyBufferSize, m.bufferSize);

    readPerformanceSettings(f, m);

    m.pluginSearchPath = f.getValue(kKeyPluginSearchPath, m.pluginSearchPath);
    m.lastPluginName = f.getValue(kKeyLastPluginName, m.lastPluginName);
    m.knownPluginListState = f.getXmlValue(kKeyKnownPluginListXml);
    m.lastActivePresetId = f.getValue(kKeyLastActivePresetId, m.lastActivePresetId);
    // MIDI import/export paths
    m.lastMidiImportPath = f.getValue(kKeyLastMidiImportPath, m.lastMidiImportPath);
    m.lastMidiExportPath = f.getValue(kKeyLastMidiExportPath, m.lastMidiExportPath);

    // Recently-opened files list
    m.recentFilesSerialized = f.getValue(kKeyRecentFiles, m.recentFilesSerialized);

    // Main content size
    m.mainWindowWidth = f.getIntValue(kKeyMainWindowWidth, m.mainWindowWidth);
    m.mainWindowHeight = f.getIntValue(kKeyMainWindowHeight, m.mainWindowHeight);
    m.keyboardScrollOffsetX = f.getIntValue(kKeyKeyboardScrollX, -1);

    // Keyboard display settings
    {
        int cm = f.getIntValue(kKeyColourMode, static_cast<int>(m.keyboardDisplay.colourMode));
        m.keyboardDisplay.colourMode = static_cast<devpiano::ui::KeyColourMode>(cm);
    }
    {
        int nd = f.getIntValue(kKeyNoteDisplay, static_cast<int>(m.keyboardDisplay.noteDisplay));
        m.keyboardDisplay.noteDisplay = static_cast<devpiano::ui::NoteDisplayMode>(nd);
    }
    m.keyboardDisplay.fadeSpeed
        = static_cast<float>(f.getDoubleValue(kKeyFadeSpeed, static_cast<double>(m.keyboardDisplay.fadeSpeed)));

    // Channel matrix as ValueTree XML.
    if (auto cmXml = f.getXmlValue(kKeyChannelMatrix)) {
        juce::ValueTree t = juce::ValueTree::fromXml(*cmXml);

        m.channelMatrix = devpiano::settings::valueTreeToChannelMatrix(t);
    }

    m.keySignature = f.getIntValue(kKeyKeySignature, 0);
    m.midiTranspose = f.getBoolValue(kKeyMidiTranspose, false);

    m.keyboardDisplay.showInstrumentFilter
        = f.getBoolValue(kKeyShowInstrumentFilter, m.keyboardDisplay.showInstrumentFilter);
    m.pluginPanelExpanded = f.getBoolValue(kKeyPluginPanelExpanded, m.pluginPanelExpanded);
    m.languageCode = f.getValue(kKeyLanguageCode, m.languageCode);
    // custom key labels as ValueTree XML (sparse: only non-empty labels stored)
    if (auto labelsXml = f.getXmlValue(kKeyCustomLabels)) {
        juce::ValueTree t = juce::ValueTree::fromXml(*labelsXml);
        m.keyboardDisplay.customKeyLabels.fill({});
        for (int i = 0; i < t.getNumChildren(); ++i) {
            auto c = t.getChild(i);
            auto note = c.getProperty("note");
            // ValueTree::fromXml 将 XML 属性还原为 String 类型（isInt() 恒 false），
            // 需同时接受 String 与 int 两种形态（内存直构 vs XML round-trip）。
            if (note.isInt() || note.isString()) {
                auto n = static_cast<int>(note);
                if (n >= 0 && n < 128) {
                    m.keyboardDisplay.customKeyLabels[static_cast<std::size_t>(n)] = c.getProperty("text").toString();
                }
            }
        }
    }

    // custom key colours as ValueTree XML (sparse: only non-transparent colours stored)
    if (auto coloursXml = f.getXmlValue(kKeyCustomColours)) {
        juce::ValueTree t = juce::ValueTree::fromXml(*coloursXml);
        m.keyboardDisplay.customKeyColours.fill(juce::Colour(0x00000000));
        for (int i = 0; i < t.getNumChildren(); ++i) {
            auto c = t.getChild(i);
            auto note = c.getProperty("note");
            // 同 labels：XML round-trip 后属性为 String 类型。
            if (note.isInt() || note.isString()) {
                auto n = static_cast<int>(note);
                if (n >= 0 && n < 128) {
                    m.keyboardDisplay.customKeyColours[static_cast<std::size_t>(n)]
                        = juce::Colour::fromString(c.getProperty("argb").toString());
                }
            }
        }
    }
}

bool SettingsStore::writeNow(const SettingsModel& m) {
    auto& f = file();

    if (m.audioDeviceState) {
        f.setValue(kKeyAudioXml, m.audioDeviceState->toString());
    }

    f.setValue(kKeySampleRate, m.sampleRate);
    f.setValue(kKeyBufferSize, m.bufferSize);

    f.setValue(kKeyGain, m.masterGain);
    f.setValue(kKeyA, m.adsrAttack);
    f.setValue(kKeyD, m.adsrDecay);

    f.setValue(kKeyS, m.adsrSustain);
    f.setValue(kKeyR, m.adsrRelease);
    f.setValue(kKeyBuiltinTone, static_cast<int>(m.builtinTone));
    f.setValue(kKeyPianoBrightness, m.pianoBrightness);
    f.setValue(kKeyPianoHammerHardness, m.pianoHammerHardness);
    f.setValue(kKeyPianoResonance, m.pianoResonance);
    f.setValue(kKeyPluginSearchPath, m.pluginSearchPath);
    f.setValue(kKeyLastPluginName, m.lastPluginName);
    if (m.knownPluginListState) {
        f.setValue(kKeyKnownPluginListXml, m.knownPluginListState->toString());
    }

    // MIDI import/export paths
    f.setValue(kKeyLastMidiImportPath, m.lastMidiImportPath);
    f.setValue(kKeyLastMidiExportPath, m.lastMidiExportPath);

    f.setValue(kKeyLastActivePresetId, m.lastActivePresetId);
    f.setValue(kKeyRecentFiles, m.recentFilesSerialized);

    // Main content size
    f.setValue(kKeyMainWindowWidth, m.mainWindowWidth);
    f.setValue(kKeyMainWindowHeight, m.mainWindowHeight);
    f.setValue(kKeyKeyboardScrollX, m.keyboardScrollOffsetX);

    // Keyboard display settings
    f.setValue(kKeyColourMode, static_cast<int>(m.keyboardDisplay.colourMode));
    f.setValue(kKeyNoteDisplay, static_cast<int>(m.keyboardDisplay.noteDisplay));
    f.setValue(kKeyFadeSpeed, m.keyboardDisplay.fadeSpeed);

    // Channel matrix as ValueTree XML.
    {
        auto t = devpiano::settings::channelMatrixToValueTree(m.channelMatrix);
        if (auto xml = t.createXml()) {
            f.setValue(kKeyChannelMatrix, xml->toString());
        }
    }

    f.setValue(kKeyKeySignature, m.keySignature);
    f.setValue(kKeyMidiTranspose, m.midiTranspose);

    // custom key labels as ValueTree XML (sparse: only non-empty labels stored)
    {
        juce::ValueTree t("customKeyLabels");
        for (int n = 0; n < 128; ++n) {
            const auto& lbl = m.keyboardDisplay.customKeyLabels[static_cast<std::size_t>(n)];
            if (lbl.isNotEmpty()) {
                auto c = juce::ValueTree("label");
                c.setProperty("note", n, nullptr);
                c.setProperty("text", lbl, nullptr);
                t.appendChild(c, nullptr);
            }
        }
        if (t.getNumChildren() > 0) {
            if (auto xml = t.createXml()) {
                f.setValue(kKeyCustomLabels, xml->toString());
            }
        } else {
            f.removeValue(kKeyCustomLabels);
        }
    }

    // custom key colours as ValueTree XML (sparse: only non-transparent stored)
    {
        juce::ValueTree t("customKeyColours");
        for (int n = 0; n < 128; ++n) {
            const auto& col = m.keyboardDisplay.customKeyColours[static_cast<std::size_t>(n)];
            if (!col.isTransparent()) {
                auto c = juce::ValueTree("colour");
                c.setProperty("note", n, nullptr);
                c.setProperty("argb", col.toString(), nullptr);
                t.appendChild(c, nullptr);
            }
        }
        if (t.getNumChildren() > 0) {
            if (auto xml = t.createXml()) {
                f.setValue(kKeyCustomColours, xml->toString());
            }
        } else {
            f.removeValue(kKeyCustomColours);
        }
    }
    f.setValue(kKeyLanguageCode, m.languageCode);
    f.setValue(kKeyShowInstrumentFilter, m.keyboardDisplay.showInstrumentFilter);
    f.setValue(kKeyPluginPanelExpanded, m.pluginPanelExpanded);

    const auto saved = f.saveIfNeeded();
    if (!saved) {
        DP_LOG_ERROR("[Settings] failed to persist settings to: " + f.getFile().getFullPathName());
    }
    return saved;
}

void SettingsStore::load(SettingsModel& model) {
    readNow(model);
}

bool SettingsStore::save(const SettingsModel& model) {
    return writeNow(model);
}

void SettingsStore::scheduleSave(const SettingsModel& model, int msDelay) {
    if (!saverTimer) {
        saverTimer = std::make_unique<SettingsDebounceTimer>(*this);
    }

    saverTimer->setPayload(model);
    saverTimer->start(msDelay);
}

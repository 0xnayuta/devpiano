#include "Layout/PerformancePreset.h"

#include "../Audio/PerspectiveProcessor.h"
#include "../Audio/RoomReverbEngine.h"
#include "Diagnostics/Log.h"

#include <algorithm>
#include <cstring>
#include <unordered_set>
namespace {

constexpr auto kPresetFileExtension = ".devpiano.preset";
constexpr auto kMaxPresetFileSizeBytes = 1024 * 1024; // 1 MB (SEC-003)
// ---- File naming helpers ----

[[nodiscard]] juce::String stripPresetExtension(const juce::String& fileName) {
    if (fileName.endsWithIgnoreCase(kPresetFileExtension)) {
        return fileName.dropLastCharacters(juce::String(kPresetFileExtension).length());
    }
    return juce::File::createLegalFileName(fileName).upToLastOccurrenceOf(".", false, false);
}

// ---- KeyAction / KeyBinding serialisation (ported from old LayoutPreset.cpp) ----

[[nodiscard]] juce::var keyActionToVar(const devpiano::core::KeyAction& action) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("type", action.type == devpiano::core::KeyActionType::note ? "note" : "unknown");
    obj->setProperty("trigger", "keyDown");
    obj->setProperty("midiNote", action.midiNote);
    obj->setProperty("midiChannel", action.midiChannel);
    obj->setProperty("velocity", action.velocity);
    return obj.get();
}

[[nodiscard]] std::optional<devpiano::core::KeyAction> varToKeyAction(const juce::var& v) {
    if (!v.isObject()) {
        return std::nullopt;
    }
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) {
        return std::nullopt;
    }

    const auto typeStr = obj->getProperty("type").toString();
    if (typeStr != "note") {
        DP_LOG_WARN("[Preset] unknown KeyAction type '" + typeStr + "', falling back to \"note\"");
    }

    if (obj->hasProperty("trigger")) {
        const auto triggerStr = obj->getProperty("trigger").toString();
        if (triggerStr != "keyDown") {
            DP_LOG_ERROR("[Preset] admission rejected: unsupported trigger '" + triggerStr
                         + "'; only press-to-sound ('keyDown') is supported");
            return std::nullopt;
        }
    }

    devpiano::core::KeyAction action;
    action.type = devpiano::core::KeyActionType::note;
    action.trigger = devpiano::core::KeyTrigger::keyDown;
    action.midiNote = static_cast<int>(obj->getProperty("midiNote"));
    action.midiChannel = static_cast<int>(obj->getProperty("midiChannel"));
    action.velocity = static_cast<float>(obj->getProperty("velocity"));
    return action;
}

[[nodiscard]] juce::var keyBindingToVar(const devpiano::core::KeyBinding& binding) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("keyCode", binding.keyCode);
    obj->setProperty("displayText", binding.displayText);
    obj->setProperty("action", keyActionToVar(binding.action));
    return obj.get();
}

[[nodiscard]] std::optional<devpiano::core::KeyBinding> varToKeyBinding(const juce::var& v) {
    if (!v.isObject()) {
        return std::nullopt;
    }
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) {
        return std::nullopt;
    }

    auto actionOpt = varToKeyAction(obj->getProperty("action"));
    if (!actionOpt.has_value()) {
        return std::nullopt;
    }

    devpiano::core::KeyBinding binding;
    binding.keyCode = static_cast<int>(obj->getProperty("keyCode"));
    binding.displayText = obj->getProperty("displayText").toString();
    binding.action = *actionOpt;
    return binding;
}

// ---- KeyGroup serialisation (Phase 34-B groups + activeGroupIndex) ----

[[nodiscard]] juce::var keyGroupToVar(const devpiano::core::KeyGroup& group) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("transposeOffset", static_cast<int>(group.transposeOffset));
    obj->setProperty("octaveShift", static_cast<int>(group.octaveShift));
    obj->setProperty("channel", static_cast<int>(group.channel));
    obj->setProperty("name", group.name);
    return obj.get();
}

[[nodiscard]] devpiano::core::KeyGroup varToKeyGroup(const juce::var& v) {
    devpiano::core::KeyGroup group;
    if (v.isObject()) {
        auto* obj = v.getDynamicObject();
        if (obj != nullptr) {
            group.transposeOffset = static_cast<std::int8_t>(
                juce::jlimit(-12, 12, static_cast<int>(obj->getProperty("transposeOffset"))));
            group.octaveShift
                = static_cast<std::int8_t>(juce::jlimit(-3, 3, static_cast<int>(obj->getProperty("octaveShift"))));
            group.channel
                = static_cast<std::uint8_t>(juce::jlimit(0, 16, static_cast<int>(obj->getProperty("channel"))));
            group.name = obj->getProperty("name").toString();
        }
    }
    return group;
}

// ---- ChannelMatrix serialisation (JSON, not ValueTree) ----

[[nodiscard]] juce::var channelToVar(const devpiano::midi::PerChannelConfig& c) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("outputChannel", static_cast<int>(c.outputChannel));
    obj->setProperty("transpose", static_cast<int>(c.transpose));
    obj->setProperty("octaveShift", static_cast<int>(c.octaveShift));
    obj->setProperty("velocity", static_cast<int>(c.velocity));
    obj->setProperty("program", static_cast<int>(c.program));
    obj->setProperty("bankMSB", static_cast<int>(c.bankMSB));
    obj->setProperty("sustainCC", static_cast<int>(c.sustainCC));
    obj->setProperty("followKey", static_cast<bool>(c.followKey));
    return obj.get();
}
[[nodiscard]] devpiano::midi::PerChannelConfig varToChannel(const juce::var& v) {
    devpiano::midi::PerChannelConfig c;
    if (v.isObject()) {
        auto* obj = v.getDynamicObject();
        if (obj != nullptr) {
            c.outputChannel = static_cast<uint8_t>(static_cast<int>(obj->getProperty("outputChannel")));
            c.transpose = static_cast<int8_t>(static_cast<int>(obj->getProperty("transpose")));
            c.octaveShift = static_cast<int8_t>(static_cast<int>(obj->getProperty("octaveShift")));
            c.velocity = static_cast<uint8_t>(static_cast<int>(obj->getProperty("velocity")));
            c.program = static_cast<uint8_t>(static_cast<int>(obj->getProperty("program")));
            c.bankMSB = static_cast<uint8_t>(static_cast<int>(obj->getProperty("bankMSB")));
            c.sustainCC = static_cast<uint8_t>(static_cast<int>(obj->getProperty("sustainCC")));
            c.followKey = static_cast<bool>(static_cast<int>(obj->getProperty("followKey")));
        }
    }
    return c;
}

[[nodiscard]] juce::var channelMatrixToVar(const devpiano::midi::ChannelMatrix& cm) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("active", cm.active);

    juce::Array<juce::var> channels;
    for (const auto& ch : cm.channels) {
        channels.add(channelToVar(ch));
    }
    obj->setProperty("channels", juce::var(channels));

    return obj.get();
}

[[nodiscard]] devpiano::midi::ChannelMatrix varToChannelMatrix(const juce::var& v) {
    devpiano::midi::ChannelMatrix cm;
    if (!v.isObject()) {
        return cm;
    }

    auto* obj = v.getDynamicObject();
    if (obj == nullptr) {
        return cm;
    }

    cm.active = static_cast<bool>(obj->getProperty("active"));

    auto channelsVar = obj->getProperty("channels");
    if (channelsVar.isArray()) {
        auto* arr = channelsVar.getArray();
        auto count = std::min(arr->size(), 16);
        for (int i = 0; i < count; ++i) {
            cm.channels[static_cast<std::size_t>(i)] = varToChannel((*arr)[i]);
        }
    }

    return cm;
}

// ---- Colour helpers ----

// juce::Colour::toString() formats as 8-char hex (AARRGGBB), but skips leading zeros.
// Use explicit formatting so "00000000" is always valid.
[[nodiscard]] juce::String colourToArgbHex(juce::Colour c) {
    return juce::String::formatted("%02x%02x%02x%02x", c.getAlpha(), c.getRed(), c.getGreen(), c.getBlue());
}

[[nodiscard]] juce::Colour argbHexToColour(const juce::String& hex) {
    if (hex.length() != 8) {
        return juce::Colour(0x00000000);
    }
    auto a = static_cast<uint8_t>(hex.substring(0, 2).getHexValue32());
    auto r = static_cast<uint8_t>(hex.substring(2, 4).getHexValue32());
    auto g = static_cast<uint8_t>(hex.substring(4, 6).getHexValue32());
    auto b = static_cast<uint8_t>(hex.substring(6, 8).getHexValue32());
    return { r, g, b, a };
}

} // anonymous namespace

namespace devpiano::layout {

// ---- File management ----

juce::File getPresetDirectory() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("DevPiano")
                   .getChildFile("Presets");
    if (!dir.exists() && !dir.createDirectory()) {
        DP_LOG_WARN("[Preset] failed to create preset directory: " + dir.getFullPathName());
    }
    return dir;
}

juce::String sanitisePresetFileName(const juce::String& name) {
    auto legal = juce::File::createLegalFileName(name).trim();
    if (legal.isEmpty()) {
        legal = "untitled";
    }
    return legal;
}

juce::String getPresetDisplayNameForFile(const juce::File& path) {
    auto displayName = stripPresetExtension(path.getFileName());
    return displayName.isNotEmpty() ? displayName : "Untitled Preset";
}

juce::File resolvePresetFile(const juce::String& name, const juce::File& dir) {
    return dir.getChildFile(sanitisePresetFileName(name) + kPresetFileExtension);
}

// ---- Load ----

juce::var performancePresetToVar(const PerformancePreset& preset) {
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("version", performancePresetFormatVersion);
    root->setProperty("uuid", preset.uuid);
    root->setProperty("name", preset.name);

    // --- layout ---
    {
        juce::DynamicObject::Ptr lo = new juce::DynamicObject();
        lo->setProperty("id", preset.layout.id);
        lo->setProperty("name", preset.layout.name);

        juce::Array<juce::var> bindings;
        for (const auto& binding : preset.layout.bindings) {
            bindings.add(keyBindingToVar(binding));
        }
        lo->setProperty("bindings", juce::var(bindings));

        // Phase 34-B: persist the four KeyGroups + active group index so that
        // a preset loaded back into the host reproduces the user's per-group
        // octave/transpose/channel overrides.
        juce::Array<juce::var> groups;
        for (const auto& group : preset.layout.groups) {
            groups.add(keyGroupToVar(group));
        }
        lo->setProperty("groups", juce::var(groups));
        lo->setProperty("activeGroupIndex", static_cast<int>(preset.layout.activeGroupIndex));

        root->setProperty("layout", juce::var(lo));
    }

    // --- channelMatrix ---
    root->setProperty("channelMatrix", channelMatrixToVar(preset.channelMatrix));

    // --- acoustics ---
    {
        juce::DynamicObject::Ptr aco = new juce::DynamicObject();
        aco->setProperty("brightness", preset.brightness);
        aco->setProperty("hammerHardness", preset.hammerHardness);
        aco->setProperty("resonance", preset.resonance);
        aco->setProperty("lidPosition", static_cast<int>(preset.lidPosition));
        aco->setProperty("touchVelocityCurve", static_cast<int>(preset.touchVelocityCurve));
        aco->setProperty("unaCorda", preset.unaCorda);
        const auto tempId = devpiano::audio::TemperamentEngine::getIdentifier(preset.temperament);
        aco->setProperty("temperament", juce::String(tempId.data(), tempId.size()));
        aco->setProperty("referencePitchA4", preset.referencePitchA4);
        const auto perspId = devpiano::audio::PerspectiveProcessor::toIdentifier(preset.soundPerspective);
        aco->setProperty("soundPerspective", juce::String(perspId.data(), perspId.size()));
        const auto spaceId = devpiano::audio::RoomReverbEngine::toIdentifier(preset.reverbSpace);
        aco->setProperty("reverbSpace", juce::String(spaceId.data(), spaceId.size()));
        aco->setProperty("reverbWet", preset.reverbWet);
        aco->setProperty("pedalNoiseLevel", preset.pedalNoiseLevel);
        aco->setProperty("feltAgeingAmount", preset.feltAgeingAmount);
        aco->setProperty("stretchTuningEnabled", preset.stretchTuningEnabled);
        aco->setProperty("duplexResonance", preset.duplexResonance);
        root->setProperty("acoustics", juce::var(aco));
    }

    // --- keyboard ---
    {
        juce::DynamicObject::Ptr kbo = new juce::DynamicObject();
        kbo->setProperty("keySignature", preset.keySignature);
        kbo->setProperty("midiTranspose", preset.midiTranspose);
        kbo->setProperty("colourMode", static_cast<int>(preset.colourMode));
        kbo->setProperty("noteDisplay", static_cast<int>(preset.noteDisplay));
        kbo->setProperty("fadeSpeed", preset.fadeSpeed);

        // Custom key labels: write all 128 entries for simplicity
        {
            juce::Array<juce::var> labels;
            for (const auto& label : preset.customKeyLabels) {
                labels.add(juce::var(label));
            }
            kbo->setProperty("customKeyLabels", juce::var(labels));
        }

        // Sparse-ish customKeyColours: write "AARRGGBB" hex for all 128 entries
        {
            juce::Array<juce::var> colours;
            for (const auto& c : preset.customKeyColours) {
                colours.add(juce::var(colourToArgbHex(c)));
            }
            kbo->setProperty("customKeyColours", juce::var(colours));
        }

        root->setProperty("keyboard", juce::var(kbo));
    }

    return { root.get() };
}

std::optional<PerformancePreset> performancePresetFromVar(const juce::var& v) {
    if (!v.isObject()) {
        return std::nullopt;
    }

    auto* obj = v.getDynamicObject();
    if (obj == nullptr) {
        return std::nullopt;
    }

    const auto versionVar = obj->getProperty("version");
    if (!versionVar.isInt() && !versionVar.isInt64()) {
        return std::nullopt;
    }
    const auto version = static_cast<juce::int64>(versionVar);
    if (version != performancePresetFormatVersion) {
        return std::nullopt;
    }
    PerformancePreset preset = makeDefaultPreset();
    if (obj->hasProperty("name")) {
        preset.name = obj->getProperty("name").toString();
    }
    const auto uuid = obj->getProperty("uuid");
    if (uuid.isString() && uuid.toString().trim().isNotEmpty()) {
        preset.uuid = uuid.toString().trim();
    } else {
        DP_LOG_WARN("[Preset] admission rejected: missing or invalid permanent identity");
        return std::nullopt;
    }

    // --- layout ---
    auto layoutVar = obj->getProperty("layout");
    if (layoutVar.isObject()) {
        auto* lo = layoutVar.getDynamicObject();
        if (lo != nullptr) {
            preset.layout.id = lo->getProperty("id").toString();
            preset.layout.name = lo->getProperty("name").toString();

            auto bindingsVar = lo->getProperty("bindings");
            if (bindingsVar.isArray()) {
                preset.layout.bindings.clear();
                for (const auto& bv : *bindingsVar.getArray()) {
                    auto bindingOpt = varToKeyBinding(bv);
                    if (!bindingOpt.has_value()) {
                        DP_LOG_ERROR("[Preset] admission rejected: contains unsupported or invalid key binding");
                        return std::nullopt;
                    }
                    preset.layout.bindings.push_back(*bindingOpt);
                }
            }

            // Phase 34-B: round-trip the four KeyGroups so user-tuned octave /
            // transpose / channel overrides survive preset load/save.
            auto groupsVar = lo->getProperty("groups");
            if (groupsVar.isArray()) {
                const auto* arr = groupsVar.getArray();
                const auto count = juce::jmin(arr->size(), static_cast<int>(preset.layout.groups.size()));
                for (int i = 0; i < count; ++i) {
                    preset.layout.groups[static_cast<std::size_t>(i)] = varToKeyGroup((*arr)[i]);
                }
            }
            if (lo->hasProperty("activeGroupIndex")) {
                preset.layout.activeGroupIndex
                    = static_cast<std::uint8_t>(juce::jlimit(0, static_cast<int>(preset.layout.groups.size() - 1),
                                                             static_cast<int>(lo->getProperty("activeGroupIndex"))));
            }
        }
    }
    // Fallback: id/name from top-level if layout section absent
    if (preset.layout.id.isEmpty()) {
        preset.layout.id = "user.preset." + sanitisePresetFileName(preset.name);
    }
    if (preset.layout.name.isEmpty()) {
        preset.layout.name = preset.name;
    }

    // --- channelMatrix ---
    preset.channelMatrix = varToChannelMatrix(obj->getProperty("channelMatrix"));
    // --- acoustics ---
    auto acVar = obj->getProperty("acoustics");
    if (acVar.isObject()) {
        if (auto* aco = acVar.getDynamicObject()) {
            if (aco->hasProperty("brightness")) {
                const auto val = static_cast<double>(aco->getProperty("brightness"));
                if (std::isfinite(val)) {
                    preset.brightness = juce::jlimit(0.0f, 1.0f, static_cast<float>(val));
                }
            }
            if (aco->hasProperty("hammerHardness")) {
                const auto val = static_cast<double>(aco->getProperty("hammerHardness"));
                if (std::isfinite(val)) {
                    preset.hammerHardness = juce::jlimit(0.0f, 1.0f, static_cast<float>(val));
                }
            }
            if (aco->hasProperty("resonance")) {
                const auto val = static_cast<double>(aco->getProperty("resonance"));
                if (std::isfinite(val)) {
                    preset.resonance = juce::jlimit(0.0f, 1.0f, static_cast<float>(val));
                }
            }
            if (aco->hasProperty("lidPosition")) {
                const auto val = static_cast<int>(aco->getProperty("lidPosition"));
                preset.lidPosition = static_cast<SettingsModel::LidPosition>(juce::jlimit(0, 2, val));
            }
            if (aco->hasProperty("touchVelocityCurve")) {
                const auto val = static_cast<int>(aco->getProperty("touchVelocityCurve"));
                preset.touchVelocityCurve = static_cast<devpiano::input::TouchVelocityCurve>(juce::jlimit(0, 3, val));
            }
            if (aco->hasProperty("unaCorda")) {
                preset.unaCorda = static_cast<bool>(aco->getProperty("unaCorda"));
            }
            if (aco->hasProperty("temperament")) {
                preset.temperament = devpiano::audio::TemperamentEngine::fromIdentifier(
                    aco->getProperty("temperament").toString().toStdString());
            }
            if (aco->hasProperty("referencePitchA4")) {
                preset.referencePitchA4 = devpiano::audio::TemperamentEngine::clampReferencePitch(
                    static_cast<double>(aco->getProperty("referencePitchA4")));
            }
            if (aco->hasProperty("soundPerspective")) {
                preset.soundPerspective = devpiano::audio::PerspectiveProcessor::fromIdentifier(
                    aco->getProperty("soundPerspective").toString().toStdString());
            }
            if (aco->hasProperty("reverbSpace")) {
                preset.reverbSpace = devpiano::audio::RoomReverbEngine::fromIdentifier(
                    aco->getProperty("reverbSpace").toString().toStdString());
            }
            if (aco->hasProperty("reverbWet")) {
                preset.reverbWet = juce::jlimit(0.0f, 1.0f, static_cast<float>(aco->getProperty("reverbWet")));
            }
            if (aco->hasProperty("pedalNoiseLevel")) {
                preset.pedalNoiseLevel
                    = juce::jlimit(0.0f, 1.0f, static_cast<float>(aco->getProperty("pedalNoiseLevel")));
            }
            if (aco->hasProperty("feltAgeingAmount")) {
                preset.feltAgeingAmount
                    = juce::jlimit(0.0f, 1.0f, static_cast<float>(aco->getProperty("feltAgeingAmount")));
            }
            if (aco->hasProperty("stretchTuningEnabled")) {
                preset.stretchTuningEnabled = static_cast<bool>(aco->getProperty("stretchTuningEnabled"));
            }
            if (aco->hasProperty("duplexResonance")) {
                const auto val = static_cast<double>(aco->getProperty("duplexResonance"));
                if (std::isfinite(val)) {
                    preset.duplexResonance = juce::jlimit(0.0f, 1.0f, static_cast<float>(val));
                }
            }
        }
    }

    // --- keyboard ---
    auto kbVar = obj->getProperty("keyboard");
    if (kbVar.isObject()) {
        auto* kbo = kbVar.getDynamicObject();
        if (kbo != nullptr) {
            if (kbo->hasProperty("keySignature")) {
                preset.keySignature = juce::jlimit(-7, 7, static_cast<int>(kbo->getProperty("keySignature")));
            }
            if (kbo->hasProperty("midiTranspose")) {
                preset.midiTranspose = static_cast<bool>(kbo->getProperty("midiTranspose"));
            }
            if (kbo->hasProperty("colourMode")) {
                int cm = static_cast<int>(kbo->getProperty("colourMode"));
                if (cm < 0 || cm > static_cast<int>(devpiano::ui::KeyColourMode::harmony)) {
                    cm = static_cast<int>(devpiano::ui::KeyColourMode::classic);
                }
                preset.colourMode = static_cast<devpiano::ui::KeyColourMode>(cm);
            }
            if (kbo->hasProperty("noteDisplay")) {
                int nd = static_cast<int>(kbo->getProperty("noteDisplay"));
                if (nd < 0 || nd > static_cast<int>(devpiano::ui::NoteDisplayMode::noteName)) {
                    nd = static_cast<int>(devpiano::ui::NoteDisplayMode::doReMi);
                }
                preset.noteDisplay = static_cast<devpiano::ui::NoteDisplayMode>(nd);
            }
            if (kbo->hasProperty("fadeSpeed")) {
                preset.fadeSpeed
                    = devpiano::ui::KeyboardSettings::clampFadeSpeed(static_cast<float>(kbo->getProperty("fadeSpeed")));
            }
            if (kbo->hasProperty("previewAlpha")) {
                preset.previewAlpha = juce::jlimit(0.0f, 1.0f, static_cast<float>(kbo->getProperty("previewAlpha")));
            }
            // customKeyLabels (sparse array)
            auto labelsVar = kbo->getProperty("customKeyLabels");
            if (labelsVar.isArray()) {
                auto* arr = labelsVar.getArray();
                auto count = std::min(arr->size(), 128);
                for (int i = 0; i < count; ++i) {
                    preset.customKeyLabels[static_cast<std::size_t>(i)] = (*arr)[i].toString();
                }
            }

            // customKeyColours (sparse array of "AARRGGBB" hex)
            auto coloursVar = kbo->getProperty("customKeyColours");
            if (coloursVar.isArray()) {
                auto* arr = coloursVar.getArray();
                auto count = std::min(arr->size(), 128);
                for (int i = 0; i < count; ++i) {
                    preset.customKeyColours[static_cast<std::size_t>(i)] = argbHexToColour((*arr)[i].toString());
                }
            }
        }
    }

    return preset;
}

std::optional<PerformancePreset> loadPreset(const juce::File& path) {
    if (!path.existsAsFile() || path.getSize() > kMaxPresetFileSizeBytes) {
        return std::nullopt;
    }

    auto raw = path.loadFileAsString();
    if (raw.isEmpty()) {
        return std::nullopt;
    }

    juce::var jsonResult;
    // ERR-007: JUCE JSON::parse returns Result
    const auto parseResult = juce::JSON::parse(raw, jsonResult);
    if (parseResult.failed()) {
        DP_LOG_WARN("[Preset] JSON parse failed: " + parseResult.getErrorMessage());
        return std::nullopt;
    }

    return performancePresetFromVar(jsonResult);
}

// ---- Save ----

namespace {
bool writePresetData(const PerformancePreset& preset, const juce::File& path) {
    const auto varObj = performancePresetToVar(preset);
    auto jsonString = juce::JSON::toString(varObj);
    if (jsonString.isEmpty()) {
        return false;
    }

    juce::FileOutputStream output(path);
    if (!output.openedOk() || !output.writeText(jsonString, false, false, nullptr)) {
        return false;
    }
    output.flush();
    return output.getStatus().wasOk();
}
}

bool savePreset(const PerformancePreset& preset, const juce::File& path) {
    if (path == juce::File()) {
        return false;
    }
    for (const auto& binding : preset.layout.bindings) {
        if (binding.action.trigger != devpiano::core::KeyTrigger::keyDown) {
            DP_LOG_ERROR(
                "[Preset] save rejected: preset contains unsupported key binding trigger; only 'keyDown' is supported");
            return false;
        }
    }
    auto targetFile = path;
    if (!targetFile.hasFileExtension(kPresetFileExtension)) {
        targetFile = targetFile.withFileExtension(kPresetFileExtension);
    }

    auto dir = targetFile.getParentDirectory();
    if (!dir.exists() && !dir.createDirectory()) {
        return false;
    }
    auto presetToSave = preset;
    if (presetToSave.uuid.trim().isEmpty()) {
        presetToSave.uuid = juce::Uuid().toDashedString();
    }

    juce::TemporaryFile tempFile(targetFile);
    if (!writePresetData(presetToSave, tempFile.getFile())) {
        return false;
    }
    return tempFile.overwriteTargetFileWithTemporary();
}

// ---- Rename ----

PresetRenameResult renamePreset(const juce::String& oldName, const juce::String& newName, bool allowOverwriteExisting,
                                const juce::File& dir) {
    const auto trimmedOld = oldName.trim();
    const auto trimmedNew = newName.trim();
    if (trimmedOld.isEmpty() || trimmedNew.isEmpty()) {
        return PresetRenameResult::invalidName;
    }

    const auto oldFile = resolvePresetFile(trimmedOld, dir);
    if (!oldFile.existsAsFile()) {
        return PresetRenameResult::sourceNotFound;
    }

    const auto presetOpt = loadPreset(oldFile);
    if (!presetOpt.has_value()) {
        return PresetRenameResult::sourceNotFound;
    }

    const auto newFile = resolvePresetFile(trimmedNew, dir);

    const bool isSamePath = oldFile == newFile;

    if (!isSamePath && newFile.existsAsFile() && !allowOverwriteExisting) {
        return PresetRenameResult::targetAlreadyExists;
    }

    auto updatedPreset = *presetOpt;
    updatedPreset.name = trimmedNew;
    updatedPreset.layout.name = trimmedNew;

    juce::TemporaryFile temporaryTarget(newFile);
    if (!writePresetData(updatedPreset, temporaryTarget.getFile())) {
        return PresetRenameResult::saveFailed;
    }

    if (isSamePath) {
        return temporaryTarget.overwriteTargetFileWithTemporary() ? PresetRenameResult::success
                                                                  : PresetRenameResult::saveFailed;
    }

    const auto stagedSource = dir.getNonexistentChildFile(
        oldFile.getFileName() + ".staging." + juce::Uuid().toDashedString(), ".tmp", false);
    if (!oldFile.moveFileTo(stagedSource)) {
        return PresetRenameResult::sourceMoveFailed;
    }

    if (!temporaryTarget.overwriteTargetFileWithTemporary()) {
        if (oldFile.exists() || !stagedSource.moveFileTo(oldFile)) {
            DP_LOG_ERROR("[Preset] rename rollback failed; original source retained at: "
                         + stagedSource.getFullPathName());
            return PresetRenameResult::sourceRestoreFailed;
        }
        return PresetRenameResult::saveFailed;
    }

    if (stagedSource != newFile && !stagedSource.deleteFile()) {
        DP_LOG_WARN("[Preset] rename committed; original source backup retained at: " + stagedSource.getFullPathName());
    }

    return PresetRenameResult::success;
}

// ---- Directory scanning ----

std::vector<PerformancePreset> scanPresetDirectory(const juce::File& dir) {
    if (!dir.exists()) {
        return {};
    }

    std::vector<PerformancePreset> results;
    std::unordered_set<std::string> seenUuids;

    for (const auto& entry : dir.findChildFiles(juce::File::TypesOfFileToFind::findFiles, false,
                                                "*" + juce::String(kPresetFileExtension))) {
        auto loaded = loadPreset(entry);
        if (loaded.has_value()) {
            const auto uuidStr = loaded->uuid.toStdString();
            if (!uuidStr.empty() && !seenUuids.insert(uuidStr).second) {
                DP_LOG_WARN("[Preset] Duplicate preset UUID detected: " + loaded->uuid
                            + " in file: " + entry.getFullPathName());
            }
            results.push_back(*loaded);
        }
    }

    std::ranges::sort(results, [](const PerformancePreset& a, const PerformancePreset& b) {
        return a.name.compareIgnoreCase(b.name) < 0;
    });

    return results;
}
// ---- Built-in defaults ----

PerformancePreset makeDefaultPreset() {
    PerformancePreset preset;
    preset.uuid = "d226a702-8342-5184-87c7-9852be35aa65";
    preset.name = "Default";
    preset.layout = devpiano::core::makeDefaultKeyboardLayout();
    preset.layout.id = "default.preset.builtin";
    preset.layout.name = "Default";
    // channelMatrix stays default (all zeroes, inactive)
    // keyboard stays default (classic colour, doReMi display, etc.)
    return preset;
}

} // namespace devpiano::layout

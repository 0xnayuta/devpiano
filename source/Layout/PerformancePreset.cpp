#include "Layout/PerformancePreset.h"
#include <algorithm>

namespace {

constexpr auto kPresetFileExtension = ".devpiano.preset";

// ---- File naming helpers ----

[[nodiscard]] juce::String stripPresetExtension(const juce::String& fileName) {
    if (fileName.endsWithIgnoreCase(kPresetFileExtension))
        return fileName.dropLastCharacters(juce::String(kPresetFileExtension).length());
    return juce::File::createLegalFileName(fileName).upToLastOccurrenceOf(".", false, false);
}

// ---- KeyAction / KeyBinding serialisation (ported from old LayoutPreset.cpp) ----

[[nodiscard]] juce::var keyActionToVar(const devpiano::core::KeyAction& action) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("type", action.type == devpiano::core::KeyActionType::note ? "note" : "unknown");
    obj->setProperty("trigger", action.trigger == devpiano::core::KeyTrigger::keyDown ? "keyDown" : "keyUp");
    obj->setProperty("midiNote", action.midiNote);
    obj->setProperty("midiChannel", action.midiChannel);
    obj->setProperty("velocity", action.velocity);
    return obj.get();
}

[[nodiscard]] devpiano::core::KeyAction varToKeyAction(const juce::var& v) {
    devpiano::core::KeyAction action;
    if (v.isObject()) {
        auto* obj = v.getDynamicObject();
        if (obj != nullptr) {
            auto typeStr = obj->getProperty("type").toString();
            action.type
                = (typeStr == "note") ? devpiano::core::KeyActionType::note : devpiano::core::KeyActionType::note;
            auto triggerStr = obj->getProperty("trigger").toString();
            action.trigger
                = (triggerStr == "keyDown") ? devpiano::core::KeyTrigger::keyDown : devpiano::core::KeyTrigger::keyUp;
            action.midiNote = static_cast<int>(obj->getProperty("midiNote"));
            action.midiChannel = static_cast<int>(obj->getProperty("midiChannel"));
            action.velocity = static_cast<float>(obj->getProperty("velocity"));
        }
    }
    return action;
}

[[nodiscard]] juce::var keyBindingToVar(const devpiano::core::KeyBinding& binding) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("keyCode", binding.keyCode);
    obj->setProperty("displayText", binding.displayText);
    obj->setProperty("action", keyActionToVar(binding.action));
    return obj.get();
}

[[nodiscard]] devpiano::core::KeyBinding varToKeyBinding(const juce::var& v) {
    devpiano::core::KeyBinding binding;
    if (v.isObject()) {
        auto* obj = v.getDynamicObject();
        if (obj != nullptr) {
            binding.keyCode = static_cast<int>(obj->getProperty("keyCode"));
            binding.displayText = obj->getProperty("displayText").toString();
            binding.action = varToKeyAction(obj->getProperty("action"));
        }
    }
    return binding;
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
    for (const auto& ch : cm.channels)
        channels.add(channelToVar(ch));
    obj->setProperty("channels", juce::var(channels));

    return obj.get();
}

[[nodiscard]] devpiano::midi::ChannelMatrix varToChannelMatrix(const juce::var& v) {
    devpiano::midi::ChannelMatrix cm;
    if (!v.isObject())
        return cm;

    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return cm;

    cm.active = static_cast<bool>(obj->getProperty("active"));

    auto channelsVar = obj->getProperty("channels");
    if (channelsVar.isArray()) {
        auto* arr = channelsVar.getArray();
        auto count = std::min(static_cast<int>(arr->size()), 16);
        for (int i = 0; i < count; ++i)
            cm.channels[static_cast<std::size_t>(i)] = varToChannel((*arr)[i]);
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
    if (hex.length() != 8)
        return juce::Colour(0x00000000);
    auto a = static_cast<uint8_t>(hex.substring(0, 2).getHexValue32());
    auto r = static_cast<uint8_t>(hex.substring(2, 4).getHexValue32());
    auto g = static_cast<uint8_t>(hex.substring(4, 6).getHexValue32());
    auto b = static_cast<uint8_t>(hex.substring(6, 8).getHexValue32());
    return juce::Colour(r, g, b, a);
}

} // anonymous namespace

namespace devpiano::layout {

// ---- File management ----

juce::File getPresetDirectory() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("DevPiano")
                   .getChildFile("Presets");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

juce::String sanitisePresetFileName(const juce::String& name) {
    // Keep only alphanumerics, spaces, hyphens, underscores; fold to legal file name.
    juce::String out;
    for (auto ch : name) {
        if (juce::CharacterFunctions::isLetterOrDigit(ch) || ch == ' ' || ch == '-' || ch == '_')
            out << ch;
        else
            out << '_';
    }
    out = out.trim();
    if (out.isEmpty())
        out = "untitled";
    return out;
}

juce::String getPresetDisplayNameForFile(const juce::File& path) {
    auto displayName = stripPresetExtension(path.getFileName());
    return displayName.isNotEmpty() ? displayName : "Untitled Preset";
}

// ---- Load ----

std::optional<PerformancePreset> loadPreset(const juce::File& path) {
    if (!path.existsAsFile())
        return std::nullopt;

    auto raw = path.loadFileAsString();
    if (raw.isEmpty())
        return std::nullopt;

    auto jsonResult = juce::JSON::parse(raw);
    if (!jsonResult.isObject())
        return std::nullopt;

    auto* obj = jsonResult.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    auto version = static_cast<int>(obj->getProperty("version"));
    if (version != performancePresetFormatVersion)
        return std::nullopt;

    PerformancePreset preset;
    preset.name = obj->getProperty("name").toString();

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
                for (const auto& bv : *bindingsVar.getArray())
                    preset.layout.bindings.push_back(varToKeyBinding(bv));
            }
        }
    }
    // Fallback: id/name from top-level if layout section absent
    if (preset.layout.id.isEmpty())
        preset.layout.id = "user.preset." + sanitisePresetFileName(preset.name);
    if (preset.layout.name.isEmpty())
        preset.layout.name = preset.name;

    // --- channelMatrix ---
    preset.channelMatrix = varToChannelMatrix(obj->getProperty("channelMatrix"));

    // --- keyboard ---
    auto kbVar = obj->getProperty("keyboard");
    if (kbVar.isObject()) {
        auto* kbo = kbVar.getDynamicObject();
        if (kbo != nullptr) {
            preset.keySignature = static_cast<int>(kbo->getProperty("keySignature"));
            preset.midiTranspose = static_cast<bool>(kbo->getProperty("midiTranspose"));
            preset.colourMode = static_cast<devpiano::ui::KeyColourMode>(
                static_cast<int>(kbo->getProperty("colourMode")));
            preset.noteDisplay = static_cast<devpiano::ui::NoteDisplayMode>(
                static_cast<int>(kbo->getProperty("noteDisplay")));
            preset.fadeSpeed = static_cast<float>(kbo->getProperty("fadeSpeed"));
            preset.previewAlpha = static_cast<float>(kbo->getProperty("previewAlpha"));

            // customKeyLabels (sparse array)
            auto labelsVar = kbo->getProperty("customKeyLabels");
            if (labelsVar.isArray()) {
                auto* arr = labelsVar.getArray();
                auto count = std::min(static_cast<int>(arr->size()), 128);
                for (int i = 0; i < count; ++i)
                    preset.customKeyLabels[static_cast<std::size_t>(i)] = (*arr)[i].toString();
            }

            // customKeyColours (sparse array of "AARRGGBB" hex)
            auto coloursVar = kbo->getProperty("customKeyColours");
            if (coloursVar.isArray()) {
                auto* arr = coloursVar.getArray();
                auto count = std::min(static_cast<int>(arr->size()), 128);
                for (int i = 0; i < count; ++i)
                    preset.customKeyColours[static_cast<std::size_t>(i)]
                        = argbHexToColour((*arr)[i].toString());
            }
        }
    }

    return preset;
}

// ---- Save ----

bool savePreset(const PerformancePreset& preset, const juce::File& path) {
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("version", performancePresetFormatVersion);
    root->setProperty("name", preset.name);

    // --- layout ---
    {
        juce::DynamicObject::Ptr lo = new juce::DynamicObject();
        lo->setProperty("id", preset.layout.id);
        lo->setProperty("name", preset.layout.name);

        juce::Array<juce::var> bindings;
        for (const auto& binding : preset.layout.bindings)
            bindings.add(keyBindingToVar(binding));
        lo->setProperty("bindings", juce::var(bindings));

        root->setProperty("layout", juce::var(lo));
    }

    // --- channelMatrix ---
    root->setProperty("channelMatrix", channelMatrixToVar(preset.channelMatrix));

    // --- keyboard ---
    {
        juce::DynamicObject::Ptr kbo = new juce::DynamicObject();
        kbo->setProperty("keySignature", preset.keySignature);
        kbo->setProperty("midiTranspose", preset.midiTranspose);
        kbo->setProperty("colourMode", static_cast<int>(preset.colourMode));
        kbo->setProperty("noteDisplay", static_cast<int>(preset.noteDisplay));
        kbo->setProperty("fadeSpeed", preset.fadeSpeed);
        // previewAlpha intentionally not serialised — SettingsModel has no corresponding
        // field; the value is reserved for future use.

        // Custom key labels: write all 128 entries for simplicity
        {
            juce::Array<juce::var> labels;
            for (const auto& label : preset.customKeyLabels)
                labels.add(juce::var(label));
            kbo->setProperty("customKeyLabels", juce::var(labels));
        }

        // Sparse-ish customKeyColours: write "AARRGGBB" hex for all 128 entries
        // (128 ARGB strings is ~1KB; sparse optimisation not worth the code complexity)
        {
            juce::Array<juce::var> colours;
            for (const auto& c : preset.customKeyColours)
                colours.add(juce::var(colourToArgbHex(c)));
            kbo->setProperty("customKeyColours", juce::var(colours));
        }

        root->setProperty("keyboard", juce::var(kbo));
    }

    auto jsonString = juce::JSON::toString(juce::var(root));
    if (jsonString.isEmpty())
        return false;

    auto targetFile = path;
    if (!targetFile.hasFileExtension(kPresetFileExtension))
        targetFile = targetFile.withFileExtension(kPresetFileExtension);

    auto dir = targetFile.getParentDirectory();
    if (!dir.exists() && !dir.createDirectory())
        return false;

    return targetFile.replaceWithText(jsonString);
}

// ---- Directory scanning ----

std::vector<PerformancePreset> scanPresetDirectory() {
    auto dir = getPresetDirectory();
    if (!dir.exists())
        return {};

    std::vector<PerformancePreset> results;

    for (const auto& entry :
         dir.findChildFiles(juce::File::TypesOfFileToFind::findFiles, false,
                            "*" + juce::String(kPresetFileExtension))) {
        auto loaded = loadPreset(entry);
        if (loaded.has_value())
            results.push_back(*loaded);
    }

    std::sort(results.begin(), results.end(),
              [](const PerformancePreset& a, const PerformancePreset& b) {
                  return a.name.compareIgnoreCase(b.name) < 0;
              });

    return results;
}

// ---- Built-in defaults ----

PerformancePreset makeDefaultPreset() {
    PerformancePreset preset;
    preset.name = "Default";
    preset.layout = devpiano::core::makeDefaultKeyboardLayout();
    preset.layout.id = "default.preset.builtin";
    preset.layout.name = "Default";
    // channelMatrix stays default (all zeroes, inactive)
    // keyboard stays default (classic colour, doReMi display, etc.)
    return preset;
}

} // namespace devpiano::layout

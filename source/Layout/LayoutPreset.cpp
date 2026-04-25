#include "Layout/LayoutPreset.h"

namespace
{
constexpr auto kLayoutFileExtension = ".freepiano.layout";

[[nodiscard]] juce::String stripLayoutPresetExtension(const juce::String& fileName)
{
    if (fileName.endsWithIgnoreCase(kLayoutFileExtension))
        return fileName.dropLastCharacters(juce::String(kLayoutFileExtension).length());

    return juce::File::createLegalFileName(fileName).upToLastOccurrenceOf(".", false, false);
}

[[nodiscard]] juce::String sanitiseUserLayoutToken(juce::String value)
{
    juce::String token;
    auto previousWasSeparator = false;

    for (const auto character : value)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(character))
        {
            token << juce::CharacterFunctions::toLowerCase(character);
            previousWasSeparator = false;
            continue;
        }

        if (! previousWasSeparator)
        {
            token << '.';
            previousWasSeparator = true;
        }
    }

    while (token.startsWithChar('.'))
        token = token.substring(1);

    while (token.endsWithChar('.'))
        token = token.dropLastCharacters(1);

    return token.isNotEmpty() ? token : "layout";
}

[[nodiscard]] juce::var keyActionToVar(const devpiano::core::KeyAction& action)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("type", action.type == devpiano::core::KeyActionType::note ? "note" : "unknown");
    obj->setProperty("trigger",
                     action.trigger == devpiano::core::KeyTrigger::keyDown ? "keyDown" : "keyUp");
    obj->setProperty("midiNote", action.midiNote);
    obj->setProperty("midiChannel", action.midiChannel);
    obj->setProperty("velocity", action.velocity);
    return obj.get();
}

[[nodiscard]] devpiano::core::KeyAction varToKeyAction(const juce::var& v)
{
    devpiano::core::KeyAction action;
    if (v.isObject())
    {
        auto* obj = v.getDynamicObject();
        if (obj != nullptr)
        {
            auto typeStr = obj->getProperty("type").toString();
            action.type = (typeStr == "note") ? devpiano::core::KeyActionType::note
                                              : devpiano::core::KeyActionType::note;
            auto triggerStr = obj->getProperty("trigger").toString();
            action.trigger = (triggerStr == "keyDown") ? devpiano::core::KeyTrigger::keyDown
                                                      : devpiano::core::KeyTrigger::keyUp;
            action.midiNote = static_cast<int>(obj->getProperty("midiNote"));
            action.midiChannel = static_cast<int>(obj->getProperty("midiChannel"));
            action.velocity = static_cast<float>(obj->getProperty("velocity"));
        }
    }
    return action;
}

[[nodiscard]] juce::var keyBindingToVar(const devpiano::core::KeyBinding& binding)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("keyCode", binding.keyCode);
    obj->setProperty("displayText", binding.displayText);
    obj->setProperty("action", keyActionToVar(binding.action));
    return obj.get();
}

[[nodiscard]] devpiano::core::KeyBinding varToKeyBinding(const juce::var& v)
{
    devpiano::core::KeyBinding binding;
    if (v.isObject())
    {
        auto* obj = v.getDynamicObject();
        if (obj != nullptr)
        {
            binding.keyCode = static_cast<int>(obj->getProperty("keyCode"));
            binding.displayText = obj->getProperty("displayText").toString();
            binding.action = varToKeyAction(obj->getProperty("action"));
        }
    }
    return binding;
}
} // anonymous namespace

namespace devpiano::layout
{
juce::String getLayoutPresetDisplayNameForFile(const juce::File& path)
{
    auto displayName = stripLayoutPresetExtension(path.getFileName());
    return displayName.isNotEmpty() ? displayName : "User Layout";
}

juce::String getUserLayoutIdForFile(const juce::File& path)
{
    return "user.layout." + sanitiseUserLayoutToken(getLayoutPresetDisplayNameForFile(path));
}

std::optional<devpiano::core::KeyboardLayout> loadLayoutPreset(const juce::File& path)
{
    if (!path.existsAsFile())
        return std::nullopt;

    auto readResult = path.loadFileAsString();
    if (readResult.isEmpty())
        return std::nullopt;

    auto jsonResult = juce::JSON::parse(readResult);
    if (!jsonResult.isObject())
        return std::nullopt;

    auto* obj = jsonResult.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    auto version = static_cast<int>(obj->getProperty("version"));
    if (version != presetFormatVersion)
        return std::nullopt;

    devpiano::core::KeyboardLayout layout;
    layout.id = obj->getProperty("id").toString();
    layout.name = obj->getProperty("name").toString();

    auto displayNameVar = obj->getProperty("displayName");
    if (displayNameVar.toString().isNotEmpty())
        layout.name = displayNameVar.toString();

    auto bindingsVar = obj->getProperty("bindings");
    if (!bindingsVar.isArray())
        return std::nullopt;

    layout.bindings.clear();
    for (const auto& bindingVar : *bindingsVar.getArray())
        layout.bindings.push_back(varToKeyBinding(bindingVar));

    return layout;
}

bool saveLayoutPreset(const devpiano::core::KeyboardLayout& layout, const juce::File& path)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("version", presetFormatVersion);
    obj->setProperty("id", layout.id);
    obj->setProperty("name", layout.name);
    obj->setProperty("displayName", layout.name);

    juce::Array<juce::var> bindingsArray;
    for (const auto& binding : layout.bindings)
        bindingsArray.add(keyBindingToVar(binding));
    obj->setProperty("bindings", juce::var(bindingsArray));

    auto jsonString = juce::JSON::toString(juce::var(obj));
    if (jsonString.isEmpty())
        return false;

    auto targetFile = path;
    if (!targetFile.hasFileExtension(".freepiano.layout"))
        targetFile = targetFile.withFileExtension(".freepiano.layout");

    auto dir = targetFile.getParentDirectory();
    if (!dir.exists() && !dir.createDirectory())
    {
        return false;
    }

    return targetFile.replaceWithText(jsonString);
}
} // namespace devpiano::layout

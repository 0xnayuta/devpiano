#include "Layout/LayoutDirectoryScanner.h"
#include "Layout/LayoutPreset.h"

namespace
{
constexpr auto kLayoutFileExtension = ".freepiano.layout";
}

namespace devpiano::layout
{
juce::File getUserLayoutDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DevPiano")
        .getChildFile("Layouts");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

std::vector<devpiano::core::KeyboardLayout> scanUserLayoutDirectory()
{
    auto dir = getUserLayoutDirectory();
    if (!dir.exists())
        return {};

    std::vector<devpiano::core::KeyboardLayout> results;

    for (const auto& entry : dir.findChildFiles(juce::File::TypesOfFileToFind::findFiles, false, "*" + juce::String(kLayoutFileExtension)))
    {
        auto loaded = loadLayoutPreset(entry);
        if (loaded.has_value())
        {
            loaded->id = getUserLayoutIdForFile(entry);
            if (loaded->name.trim().isEmpty())
                loaded->name = getLayoutPresetDisplayNameForFile(entry);
            results.push_back(*loaded);
        }
    }

    return results;
}

std::optional<devpiano::core::KeyboardLayout> loadUserLayoutById(const juce::String& layoutId)
{
    if (layoutId.isEmpty() || layoutId.startsWith("default."))
        return std::nullopt;

    for (auto layout : scanUserLayoutDirectory())
    {
        if (layout.id == layoutId)
            return layout;
    }

    return std::nullopt;
}
} // namespace devpiano::layout

#pragma once

#include <JuceHeader.h>

#include "Locale/zh_CN.loc.h"

namespace devpiano::locale {

enum class Language : uint8_t {
    en, // English (no translation table — translate() returns original)
    zhCN, // Chinese (Simplified)
};

// Try to load a .loc file from several common paths relative to the
// executable / working directory.  Returns nullptr on failure.
[[nodiscard]] inline std::unique_ptr<juce::LocalisedStrings> tryLoadLocaleFile(const juce::String& fileName) {
    // Search order: next to executable, CWD/locales/, project root
    const juce::Array<juce::File> searchDirs = {
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("locales"),
        juce::File::getCurrentWorkingDirectory().getChildFile("locales"),
        juce::File::getCurrentWorkingDirectory(),
    };

    for (const auto& dir : searchDirs) {
        auto file = dir.getChildFile(fileName);
        if (file.existsAsFile()) {
            auto result = std::make_unique<juce::LocalisedStrings>(file, false);
            // Basic validation: language name should be non-empty if file parsed
            if (result->getLanguageName().isNotEmpty())
                return result;
        }
    }
    return nullptr;
}

// Activate the given language by setting JUCE's global LocalisedStrings.
// Call this any time the language should change at runtime.
// Primary path: compiled-in constant (reliable).  Falls back to file loading
// so that advanced users can add strings not present in the embedded table without recompiling.
inline void activate(Language lang) {
    if (lang == Language::zhCN) {
        // Primary: embedded constant (UTF-8 via u8 prefix, works on all toolchains)
        auto zh = std::make_unique<juce::LocalisedStrings>(
            juce::String::fromUTF8(zhCNLocale, (int)sizeof(zhCNLocale) - 1), false);

        // Secondary: overlay translations from a .loc file if it exists
        auto fileLocale = tryLoadLocaleFile("zh-CN.loc");
        if (fileLocale != nullptr)
            zh->setFallback(fileLocale.release());

        juce::LocalisedStrings::setCurrentMappings(zh.release());
    } else {
        // English: clear any translation table so translate() returns original strings.
        // (This also allows switching from zh-CN back to English.)
        juce::LocalisedStrings::setCurrentMappings(nullptr);
    }
}

inline Language codeToLanguage(const juce::String& code) {
    if (code == "zh-CN")
        return Language::zhCN;
    return Language::en;
}

inline juce::String languageToCode(Language lang) {
    switch (lang) {
    case Language::zhCN:
        return "zh-CN";
    default:
        return "en";
    }
}

// Human-readable labels for the settings ComboBox.
// Return the display name in the language's own script.
inline juce::String languageDisplayName(Language lang) {
    switch (lang) {
    case Language::zhCN:
        return juce::String::fromUTF8("\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"); // 简体中文
    default:
        return "English";
    }
}

} // namespace devpiano::locale

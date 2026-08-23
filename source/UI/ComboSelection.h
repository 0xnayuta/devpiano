#pragma once

#include <juce_core/juce_core.h>

namespace devpiano::ui {

// Pure selection logic shared between the JIVE accessors and the unit tests,
// so the preferred/empty branches are exercised by production code rather
// than replayed inside the tests.

/// Index of the entry in `names` matching `preferredSelection`
/// (case-insensitive), or -1 when nothing matches.
[[nodiscard]] inline int preferredNameIndex(const juce::StringArray& names,
                                            const juce::String& preferredSelection) noexcept {
    for (int i = 0; i < names.size(); ++i) {
        if (names[i].equalsIgnoreCase(preferredSelection)) {
            return i;
        }
    }
    return -1;
}

/// Index of `presetIds` equal to `currentPresetId`, or 0 when absent (the
/// caller selects -1 itself when the list is empty).
[[nodiscard]] inline int presetIdIndex(const juce::StringArray& presetIds,
                                       const juce::String& currentPresetId) noexcept {
    for (int i = 0; i < presetIds.size(); ++i) {
        if (presetIds[i] == currentPresetId) {
            return i;
        }
    }
    return 0;
}

} // namespace devpiano::ui

#include "UI/PerformanceMetadataDialog.h"

#include "UI/jive/JiveModalDialog.h"

// ============================================================================
// PerformanceMetadataDialog Implementation
//
// Refactored in Phase 15-B to use JiveModalDialog declarative templates,
// eliminating redundant MetadataEditContent and manual pixel calculations.
// ============================================================================

void PerformanceMetadataDialog::launch(
    const devpiano::recording::PerformanceFileMetadata& initialMetadata, juce::Component* componentToCentreAround,
    std::function<void(std::optional<devpiano::recording::PerformanceFileMetadata>)> onComplete) {
    devpiano::ui::jive::JiveModalDialog::launchMetadataEdit(
        TRANS("Song Information"), initialMetadata.title, initialMetadata.notes, componentToCentreAround,
        [callback = std::move(onComplete)](std::optional<devpiano::ui::jive::JiveModalDialog::MetadataResult> result) {
            if (!result.has_value()) {
                if (callback) {
                    callback(std::nullopt);
                }
                return;
            }

            devpiano::recording::PerformanceFileMetadata meta;
            meta.title = result->title;
            meta.notes = result->notes;
            if (callback) {
                callback(std::move(meta));
            }
        });
}

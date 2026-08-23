#pragma once

#include <functional>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

#include "UI/jive/JiveUtils.h"

namespace devpiano::ui::jive {

// ============================================================================
/// Declarative JIVE-based modal dialog launcher and standard templates.
///
/// Provides reusable, dark-theme consistent modal dialogs powered by JIVE's
/// ValueTree layout engine and StyleCatalog rules. Eliminates manual setBounds
/// coordinate calculations in popup dialogs.
// ============================================================================
class JiveModalDialog {
public:
    JiveModalDialog() = delete;

    /// Options for launching a custom JIVE modal dialog.
    struct LaunchOptions {
        juce::String title;
        juce::ValueTree layoutTree;
        juce::Component* componentToCentreAround = nullptr;
        int defaultWidth = 380;
        int defaultHeight = 160;
        bool isResizable = false;

        /// Called when the dialog is initialized to set initial properties,
        /// text editor values, or listeners on the GuiItem tree.
        std::function<void(::jive::GuiItem&)> onInit;

        /// Called when the OK / Confirm button is clicked, or Return key is pressed.
        /// Returning false keeps the dialog open (e.g. on validation error).
        std::function<bool(::jive::GuiItem&)> onConfirm;

        /// Called on Cancel button click, Escape key, or title bar close (X).
        std::function<void()> onCancel;
        /// Optional hook to register custom component types with the dialog's
        /// JIVE interpreter factory before the layout tree is interpreted.
        std::function<void(::jive::ComponentFactory&)> configureFactory;
    };

    /// Launch a modal dialog with custom JIVE ValueTree layout.
    static void launchCustom(const LaunchOptions& options);

    // ── Pre-built Declarative Templates & Launchers ──

    /// 1. Single-line Text Input Dialog (e.g. Preset Rename / Save As New).
    /// Calls `onComplete` with the trimmed string, or std::nullopt if cancelled.
    static void launchSingleInput(const juce::String& title, const juce::String& labelText,
                                  const juce::String& initialValue, juce::Component* componentToCentreAround,
                                  std::function<void(std::optional<juce::String>)> onComplete, int maxChars = 64,
                                  const juce::String& okButtonText = TRANS("OK"),
                                  const juce::String& cancelButtonText = TRANS("Cancel"));

    /// 2. Confirmation Dialog (e.g. Delete Preset / Confirm Action).
    /// Calls `onComplete(true)` if confirmed, `onComplete(false)` if cancelled/closed.
    static void launchConfirm(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                              const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                              std::function<void(bool)> onComplete);

    /// Metadata result structure for song information dialog.
    struct MetadataResult {
        juce::String title;
        juce::String notes;
    };

    /// 3. Metadata Edit Dialog (Song Title + Notes).
    /// Calls `onComplete` with MetadataResult, or std::nullopt if cancelled.
    static void launchMetadataEdit(const juce::String& title, const juce::String& initialTitle,
                                   const juce::String& initialNotes, juce::Component* componentToCentreAround,
                                   std::function<void(std::optional<MetadataResult>)> onComplete);

    // ── Template ValueTree Builders (exposed for testing & customization) ──

    [[nodiscard]] static juce::ValueTree makeSingleInputLayout(const juce::String& labelText, int width = 380,
                                                               int height = 150,
                                                               const juce::String& okText = TRANS("OK"),
                                                               const juce::String& cancelText = TRANS("Cancel"));

    [[nodiscard]] static juce::ValueTree makeConfirmLayout(const juce::String& message, int width = 380,
                                                           int height = 140, const juce::String& okText = TRANS("OK"),
                                                           const juce::String& cancelText = TRANS("Cancel"));

    [[nodiscard]] static juce::ValueTree makeMetadataEditLayout(int width = 420, int height = 260,
                                                                const juce::String& okText = TRANS("OK"),
                                                                const juce::String& cancelText = TRANS("Cancel"));

    [[nodiscard]] static juce::ValueTree makeProgressLayout(const juce::String& initialMessage = TRANS("Exporting..."),
                                                            int width = 380, int height = 140,
                                                            const juce::String& cancelText = TRANS("Cancel"));
    // ── Component Retrieval Helpers ──

    [[nodiscard]] static ::jive::GuiItem* findGuiItemById(::jive::GuiItem& root, const juce::String& id) {
        return devpiano::ui::jive::findGuiItemById(root, id);
    }
    [[nodiscard]] static juce::Button* findButtonById(::jive::GuiItem& root, const juce::String& id) {
        return devpiano::ui::jive::findButtonById(root, id);
    }
    [[nodiscard]] static juce::TextEditor* findTextEditorById(::jive::GuiItem& root, const juce::String& id) {
        return devpiano::ui::jive::findTextEditorById(root, id);
    }

private:
    JUCE_DECLARE_NON_COPYABLE(JiveModalDialog)
};

} // namespace devpiano::ui::jive

#pragma once

#include <functional>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

namespace jive {
class ComponentFactory;
} // namespace jive

namespace devpiano::ui {
class ViewHost;
} // namespace devpiano::ui

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
        bool isResizable = false;

        /// Modern facade callback passing the ViewHost facade.
        std::function<void(const devpiano::ui::ViewHost&)> onInitHost;

        /// Modern facade callback passing the ViewHost facade.
        /// Returning false keeps the dialog open (e.g. on validation error).
        std::function<bool(const devpiano::ui::ViewHost&)> onConfirmHost;

        /// Called on Cancel button click, Escape key, or title bar close (X).
        std::function<void()> onCancel;
        /// Optional hook to register custom component types with the dialog's
        /// JIVE interpreter factory before the layout tree is interpreted.
        std::function<void(::jive::ComponentFactory&)> configureFactory;
    };

    /// Launch a modal dialog with custom JIVE ValueTree layout.
    static void launchCustom(const LaunchOptions& options);
    static juce::DialogWindow* launchWindow(juce::DialogWindow::LaunchOptions& options);

    // -- Pre-built Declarative Templates & Launchers --

    struct SingleInputOptions {
        juce::String title;
        juce::String labelText;
        juce::String initialValue;
        juce::Component* componentToCentreAround = nullptr;
        std::function<void(std::optional<juce::String>)> onComplete = nullptr;
        int maxChars = 64;
        juce::String okButtonText = TRANS("OK");
        juce::String cancelButtonText = TRANS("Cancel");
    };

    /// 1. Single-line Text Input Dialog (options-based modern launcher).
    static void launchSingleInput(const SingleInputOptions& options);

    /// 1. Single-line Text Input Dialog (legacy convenience overload).
    static void launchSingleInput(const juce::String& title, const juce::String& labelText,
                                  const juce::String& initialValue, juce::Component* componentToCentreAround,
                                  const std::function<void(std::optional<juce::String>)>& onComplete, int maxChars = 64,
                                  const juce::String& okButtonText = TRANS("OK"),
                                  const juce::String& cancelButtonText = TRANS("Cancel"));

    struct ConfirmOptions {
        juce::String title;
        juce::String message;
        juce::String okLabel = TRANS("OK");
        juce::String cancelLabel = TRANS("Cancel");
        juce::Component* componentToCentreAround = nullptr;
        std::function<void(bool)> onComplete = nullptr;
    };

    /// 2. Confirmation Dialog (options-based modern launcher).
    static void launchConfirm(const ConfirmOptions& options);

    /// 2. Confirmation Dialog (legacy convenience overload).
    static void launchConfirm(const juce::String& title, const juce::String& message, const juce::String& okLabel,
                              const juce::String& cancelLabel, juce::Component* componentToCentreAround,
                              const std::function<void(bool)>& onComplete);

    /// Metadata result structure for song information dialog.
    struct MetadataResult {
        juce::String title;
        juce::String notes;
    };

    struct MetadataEditOptions {
        juce::String title;
        juce::String initialTitle;
        juce::String initialNotes;
        juce::Component* componentToCentreAround = nullptr;
        std::function<void(std::optional<MetadataResult>)> onComplete = nullptr;
    };

    /// 3. Metadata Edit Dialog (options-based modern launcher).
    static void launchMetadataEdit(const MetadataEditOptions& options);

    /// 3. Metadata Edit Dialog (legacy convenience overload).
    static void launchMetadataEdit(const juce::String& title, const juce::String& initialTitle,
                                   const juce::String& initialNotes, juce::Component* componentToCentreAround,
                                   const std::function<void(std::optional<MetadataResult>)>& onComplete);
    // -- Template ValueTree Builders (exposed for testing & customization) --

    [[nodiscard]] static juce::ValueTree makeDialogRoot(int width, int padding = 12);
    [[nodiscard]] static juce::ValueTree makeDialogButtons(const juce::String& okText, const juce::String& cancelText,
                                                           juce::ValueTree leadingAction = {});

    [[nodiscard]] static juce::ValueTree makeSingleInputLayout(const juce::String& labelText, int width = 380,
                                                               const juce::String& okText = TRANS("OK"),
                                                               const juce::String& cancelText = TRANS("Cancel"));

    [[nodiscard]] static juce::ValueTree makeConfirmLayout(const juce::String& message, int width = 380,
                                                           const juce::String& okText = TRANS("OK"),
                                                           const juce::String& cancelText = TRANS("Cancel"));

    [[nodiscard]] static juce::ValueTree makeMetadataEditLayout(int width = 420,
                                                                const juce::String& okText = TRANS("OK"),
                                                                const juce::String& cancelText = TRANS("Cancel"));

    [[nodiscard]] static juce::ValueTree makeProgressLayout(const juce::String& initialMessage = TRANS("Exporting..."),
                                                            int width = 380,
                                                            const juce::String& cancelText = TRANS("Cancel"));

private:
    JUCE_DECLARE_NON_COPYABLE(JiveModalDialog)
};

} // namespace devpiano::ui::jive

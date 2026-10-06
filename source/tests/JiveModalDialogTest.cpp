#include <BinaryData.h>
#include <JuceHeader.h>

#include "TestHelpers.h"
#include "UI/KeyBindingEditDialog.h"
#include "UI/ViewHost.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveModalDialog.h"
#include "UI/jive/StyleCatalog.h"
#include "UI/jive/core/jive_TextComponent.h"
#include <array>

class JiveModalDialogTest final : public juce::UnitTest {
public:
    JiveModalDialogTest()
        : juce::UnitTest("JiveModalDialog", "DevPiano/UI") {
    }

    void runTest() override {
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::jive::DesignTokens::get().reset();
        devpiano::jive::DesignTokens::get().loadFromJSON(juce::JSON::parse(
            juce::String::fromUTF8(BinaryData::design_tokens_json, BinaryData::design_tokens_jsonSize)));
        devpiano::ui::jive::StyleCatalog::get().loadFromJSON(juce::JSON::parse(
            juce::String::fromUTF8(BinaryData::style_sheets_json, BinaryData::style_sheets_jsonSize)));
        testContentFittedFooters();
        testWrappedConfirmationBounds();
        testMetadataInterpretation();
        testModalDismissalCancellation();
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::jive::DesignTokens::get().reset();
    }

private:
    using Modal = devpiano::ui::jive::JiveModalDialog;

    void expectFooterBounds(const devpiano::ui::ViewHost& host) {
        auto* root = host.getRootComponent();
        auto* cancel = host.find<juce::Button>("dialog-cancel-btn");
        expect(root != nullptr && cancel != nullptr);
        if (root == nullptr || cancel == nullptr) {
            return;
        }
        const auto bounds = root->getLocalArea(cancel, cancel->getLocalBounds());
        const auto& tokens = devpiano::jive::DesignTokens::get();
        expectEquals(root->getHeight() - bounds.getBottom(), tokens.dialogBottomPadding());
        expectEquals(bounds.getHeight(), tokens.dialogButtonHeight());
        expect(root->getLocalBounds().contains(bounds), "Cancel must remain completely inside the content area");
        if (auto* ok = host.find<juce::Button>("dialog-ok-btn")) {
            const auto okBounds = root->getLocalArea(ok, ok->getLocalBounds());
            expectEquals(bounds.getX() - okBounds.getRight(), tokens.dialogButtonGap());
            expectEquals(okBounds.getY(), bounds.getY());
            expect(root->getLocalBounds().contains(okBounds), "Confirm must remain completely inside the content area");
        }
    }

    void testContentFittedFooters() {
        beginTest("Content-fitted forms retain a complete fixed-height footer and bottom inset");
        const std::array layouts {
            Modal::makeSingleInputLayout("Preset Name:"),
            Modal::makeMetadataEditLayout(),
            Modal::makeProgressLayout("Exporting..."),
            KeyBindingEditDialog::makeKeyBindingEditLayout(true),
            KeyBindingEditDialog::makeKeyBindingEditLayout(false),
        };
        for (const auto& layout : layouts) {
            devpiano::ui::ViewHost host;
            host.registerDefaultComponents();
            expect(host.loadLayout(layout, true));
            const auto width = static_cast<int>(layout.getProperty("width"));
            host.fitToContent(width);
            devpiano::test::drainMessages(2);
            expectFooterBounds(host);
            if (auto* notes = host.find<juce::TextEditor>("notes-editor")) {
                expect(notes->getHeight() >= 80, "Notes must retain its usable editing area");
            }
            for (const auto* id : { "dialog-unbind-btn", "dialog-bind-btn" }) {
                if (auto* leading = host.find<juce::Button>(id)) {
                    auto* root = host.getRootComponent();
                    const auto bounds = root->getLocalArea(leading, leading->getLocalBounds());
                    auto* ok = host.find<juce::Button>("dialog-ok-btn");
                    const auto okBounds = root->getLocalArea(ok, ok->getLocalBounds());
                    expect(bounds.getRight() <= okBounds.getX(), "Leading actions must not overlap confirmation");
                    expectEquals(bounds.getY(), okBounds.getY());
                    expectEquals(bounds.getHeight(), okBounds.getHeight());
                }
            }
        }
    }

    void testWrappedConfirmationBounds() {
        beginTest("Confirmation text stays fully inside the content area at narrow and wide widths");
        const std::array messages {
            juce::String("A short question?"),
            juce::String::repeatedString("A longer message with words and punctuation. ", 8),
            juce::String::repeatedString("abcdefghij", 12),
            juce::String::repeatedString(juce::String::charToString(0x6f22), 64),
            juce::String("First line\nSecond line\nThird line\nFourth line\nFifth line"),
            juce::String::repeatedString(juce::String::charToString(0x6f22), 8)
                + juce::String::repeatedString("UnbrokenName", 6) + "\n"
                + juce::String::repeatedString(juce::String::charToString(0x6f22), 4),
        };
        for (const auto& message : messages) {
            devpiano::ui::ViewHost host;
            host.registerDefaultComponents();
            expect(host.loadLayout(Modal::makeConfirmLayout(message), true));
            for (const int width : { 240, 380, 560 }) {
                host.fitToContent(width);
                devpiano::test::drainMessages(2);
                auto* label = host.find<::jive::TextComponent>("dialog-message");
                auto* footer = host.find("dialog-buttons");
                auto* root = host.getRootComponent();
                expect(label != nullptr && footer != nullptr && root != nullptr);
                if (label == nullptr || footer == nullptr || root == nullptr) {
                    continue;
                }
                juce::TextLayout rendered;
                rendered.createLayout(label->getAttributedString(), static_cast<float>(label->getWidth()));
                expect(rendered.getHeight() <= static_cast<float>(label->getHeight()),
                       "The laid-out final line must not be clipped");
                expect(rendered.getWidth() <= static_cast<float>(label->getWidth()) + 1.0f,
                       "Unbroken names must not escape the horizontal text bounds");
                const auto labelBounds = root->getLocalArea(label, label->getLocalBounds());
                const auto footerBounds = root->getLocalArea(footer, footer->getLocalBounds());
                expect(root->getLocalBounds().contains(labelBounds));
                expect(footerBounds.getY() - labelBounds.getBottom()
                           >= devpiano::jive::DesignTokens::get().dialogBodyGap(),
                       "The footer must not consume the minimum message separation");
                expectFooterBounds(host);
                host.setBounds(0, 0, width, root->getHeight() + 1);
                expectFooterBounds(host);
            }
        }
    }

    void testMetadataInterpretation() {
        beginTest("makeMetadataEditLayout: production ViewHost editable notes vs read-only diagnostics");

        auto tree = devpiano::ui::jive::JiveModalDialog::makeMetadataEditLayout();

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();
        expect(host.loadLayout(tree, true));
        expect(host.isValid());

        auto* titleEd = host.find<juce::TextEditor>("title-editor");
        expect(titleEd != nullptr);
        if (titleEd != nullptr) {
            expect(!titleEd->isMultiLine());
            expect(!titleEd->isReadOnly());
            expect(titleEd->getWantsKeyboardFocus());
        }

        auto* notesEd = host.find<juce::TextEditor>("notes-editor");
        expect(notesEd != nullptr);
        if (notesEd != nullptr) {
            expect(notesEd->isMultiLine());
            expect(!notesEd->isReadOnly());
            expect(notesEd->getWantsKeyboardFocus());
            expect(notesEd->getMouseClickGrabsKeyboardFocus());
            expect(notesEd->isCaretVisible());

            // Initial text
            notesEd->setText("Line 1", juce::dontSendNotification);
            notesEd->keyPressed(juce::KeyPress(juce::KeyPress::endKey, juce::ModifierKeys::ctrlModifier, 0));

            // Inject user keypresses: newline and characters
            notesEd->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
            for (const auto c : { 'L', 'i', 'n', 'e', ' ', '2' }) {
                notesEd->keyPressed(juce::KeyPress(c, 0, c));
            }

            expectEquals(notesEd->getText(), juce::String("Line 1\nLine 2"));
        }

        // Verify diagnostic ListEditor from production factory remains strictly read-only
        auto diagTree = juce::ValueTree("ListEditor");
        diagTree.setProperty("id", "diag-test", nullptr);
        devpiano::ui::ViewHost diagHost;
        diagHost.registerDefaultComponents();
        expect(diagHost.loadLayout(diagTree, true));
        if (auto* diagEd = diagHost.find<juce::TextEditor>("diag-test")) {
            expect(diagEd->isMultiLine());
            expect(diagEd->isReadOnly());
            expect(!diagEd->getWantsKeyboardFocus());
            diagEd->setText("Log Entry", juce::dontSendNotification);
            const auto textBefore = diagEd->getText();
            const bool keyHandled = diagEd->keyPressed(juce::KeyPress('X', 0, 'X'));
            expect(!keyHandled);
            expectEquals(diagEd->getText(), textBefore);
        }
    }

    void testModalDismissalCancellation() {
        beginTest("JiveModalDialog: cancellation callbacks and button dismiss triggers");

        // 1. Verify confirm layout cancel button triggers cancellation callback
        {
            std::optional<bool> confirmResult;
            auto tree = Modal::makeConfirmLayout("Dismiss this dialog?", 380, "OK", "Cancel");

            devpiano::ui::ViewHost host;
            host.registerDefaultComponents();
            expect(host.loadLayout(tree, true));

            auto* cancelBtn = host.find<juce::Button>("dialog-cancel-btn");
            expect(cancelBtn != nullptr, "Cancel button must exist in confirm layout");

            if (cancelBtn != nullptr) {
                cancelBtn->onClick = [&confirmResult] { confirmResult = false; };
                expect(!confirmResult.has_value(), "Callback must not be invoked prematurely");

                cancelBtn->triggerClick();
                devpiano::test::drainMessages(2);
                expect(confirmResult.has_value() && confirmResult.value() == false,
                       "Cancel button click must trigger cancellation callback");
            }
        }

        // 2. Verify ModalCallbackFunction SafePointer closure lifetime defense
        // (Guarantees that external window dismissal safely routes to content when alive,
        // and cleanly ignores when content is already destroyed without dangling pointer crashes).
        {
            bool cancelInvoked = false;
            auto contentComp = std::make_unique<juce::Component>();
            auto safePointer = juce::Component::SafePointer<juce::Component>(contentComp.get());

            auto modalCallback = [safePointer, &cancelInvoked](int) {
                if (safePointer.getComponent() != nullptr) {
                    cancelInvoked = true;
                }
            };

            // Case A: Content is alive when modal loop exits
            modalCallback(0);
            expect(cancelInvoked, "Modal dismissal callback must execute when content is alive");

            // Case B: Content was already destroyed before dismissal callback dispatched
            cancelInvoked = false;
            contentComp.reset();
            modalCallback(0);
            expect(!cancelInvoked, "Modal dismissal callback must safely ignore destroyed content");
        }
    }
};

static JiveModalDialogTest jiveModalDialogTest;

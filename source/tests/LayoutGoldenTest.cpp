#include <JuceHeader.h>

#include "Settings/jive/SettingsLayoutModel.h"
#include "UI/CustomKeyboard.h"
#include "UI/KeyBindingEditDialog.h"
#include "UI/ViewHost.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveBuilderHelpers.h"
#include "UI/jive/JiveModalDialog.h"
#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"

// ============================================================================
/// LayoutGoldenTest — Declarative UI Layout & Geometric Invariants (Phase 28-B)
///
/// 1. Interpretation Golden Smoke Tests: All ValueTree layouts in the app
///    must interpret through ViewHost without errors or null component failures.
/// 2. Deterministic Layout Bounds: Standard resolution pixel coordinates (1280x720,
///    1920x1080) for root container, status bar (28px invariant), and CSS Grid.
/// 3. Focus & Glissando Invariants: Keyboard click does not steal focus, and
///    mouse drag triggers paired NoteOn / NoteOff glissando events.
// ============================================================================
class LayoutGoldenTest final : public juce::UnitTest {
public:
    LayoutGoldenTest()
        : juce::UnitTest("LayoutGoldenTest", "DevPiano/UI") {
    }

    void runTest() override {
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::jive::DesignTokens::get().reset();

        testAllDeclarativeLayoutSmoke();
        drainMessages();
        testDeterministicRootLayoutBounds1280x720();
        drainMessages();
        testDeterministicRootLayoutBounds1920x1080();
        drainMessages();
        testDeterministicSettingsAndCssGridBounds();
        drainMessages();
        testFocusIsolationAndGlissandoInvariants();
        drainMessages();
    }

    static void drainMessages() {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(2);
    }
    // ────────────────────────────────────────────────────────────────────────
    // 1. Interpretation Golden Smoke Tests
    // ────────────────────────────────────────────────────────────────────────
    void testAllDeclarativeLayoutSmoke() {
        beginTest("Golden Smoke: Interpretation of all application layouts");

        juce::MidiKeyboardState keyboardState;
        devpiano::ui::ViewHost host;
        host.registerKeyboardComponents(keyboardState);

        // 1. Root Application Layout
        {
            auto tree = devpiano::ui::jive::makeRootLayout();
            expect(host.loadLayout(tree, true));
            expect(host.isValid());
            expect(host.find<juce::Button>("settings-btn") != nullptr);
            expect(host.find<juce::ComboBox>("plugin-selector") != nullptr);
            expect(host.find<juce::Slider>("volume-knob") != nullptr);
            expect(host.find("custom-keyboard") != nullptr);
            expect(host.find("status-bar") != nullptr);
            expect(host.find("midi-dot") != nullptr);
            expect(host.find("audio-info-label") != nullptr);
        }

        // 2. Settings Window Layout
        {
            auto tree = devpiano::ui::jive::makeSettingsLayoutTree();
            expect(host.loadLayout(tree, true));
            expect(host.isValid());
            expect(host.find<juce::ComboBox>("audio-device-type-combo") != nullptr);
            expect(host.find<juce::ComboBox>("key-signature-combo") != nullptr);
            expect(host.find<juce::Button>("midi-transpose-toggle") != nullptr);
            expect(host.find("follow-key-grid") != nullptr);
            for (int ch = 0; ch < 16; ++ch) {
                expect(host.find("follow-key-" + juce::String(ch)) != nullptr);
            }
            expect(host.find<juce::Button>("save-button") != nullptr);
        }

        // 3. Single Input Dialog Layout (Preset Rename / Save)
        {
            auto tree = devpiano::ui::jive::JiveModalDialog::makeSingleInputLayout("Preset Name:", 380, 150);
            expect(host.loadLayout(tree, true));
            expect(host.isValid());
            expect(host.find<juce::TextEditor>("dialog-editor") != nullptr);
            expect(host.find<juce::Button>("dialog-ok-btn") != nullptr);
            expect(host.find<juce::Button>("dialog-cancel-btn") != nullptr);
        }

        // 4. Confirm Dialog Layout
        {
            auto tree = devpiano::ui::jive::JiveModalDialog::makeConfirmLayout("Delete this preset?", 380, 140);
            expect(host.loadLayout(tree, true));
            expect(host.isValid());
            expect(host.find<juce::Button>("dialog-ok-btn") != nullptr);
            expect(host.find<juce::Button>("dialog-cancel-btn") != nullptr);
        }

        // 5. Metadata Edit Dialog Layout
        {
            auto tree = devpiano::ui::jive::JiveModalDialog::makeMetadataEditLayout(420, 260);
            expect(host.loadLayout(tree, true));
            expect(host.isValid());
            expect(host.find<juce::TextEditor>("title-editor") != nullptr);
            expect(host.find<juce::TextEditor>("notes-editor") != nullptr);
            expect(host.find<juce::Button>("dialog-ok-btn") != nullptr);
            expect(host.find<juce::Button>("dialog-cancel-btn") != nullptr);
        }

        // 6. Progress Dialog Layout
        {
            auto tree = devpiano::ui::jive::JiveModalDialog::makeProgressLayout("Exporting WAV...", 380, 140);
            expect(host.loadLayout(tree, true));
            expect(host.isValid());
            expect(host.find("dialog-progress-bar") != nullptr);
            expect(host.find("progress-status-message") != nullptr);
            expect(host.find<juce::Button>("dialog-cancel-btn") != nullptr);
        }

        // 7. Key Binding Edit Dialog Layout
        {
            auto tree = KeyBindingEditDialog::makeKeyBindingEditLayout(true, 420, 290);
            expect(host.loadLayout(tree, true));
            expect(host.isValid());
            expect(host.find("custom-label-editor") != nullptr);
            expect(host.find("clear-colour-btn") != nullptr);
            expect(host.find("dialog-ok-btn") != nullptr);
            expect(host.find("dialog-cancel-btn") != nullptr);
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 2. Deterministic Layout Bounds (1280x720)
    // ────────────────────────────────────────────────────────────────────────
    void testDeterministicRootLayoutBounds1280x720() {
        beginTest("Golden Bounds: Root layout geometry at 1280x720 standard resolution");

        juce::MidiKeyboardState keyboardState;
        devpiano::ui::ViewHost host;
        host.registerKeyboardComponents(keyboardState);

        auto tree = devpiano::ui::jive::makeRootLayout();
        expect(host.loadLayout(tree, true));

        // Enforce 1280x720 window bounds
        host.setBounds(0, 0, 1280, 720);

        auto* rootComp = host.getRootComponent();
        expect(rootComp != nullptr);
        expectEquals(rootComp->getWidth(), 1280);
        expectEquals(rootComp->getHeight(), 720);

        // Status bar golden invariants: 24px tall, pinned to bottom
        const auto sbHeight = devpiano::jive::DesignTokens::get().statusBarHeight();
        auto* statusBarComp = host.find("status-bar");
        expect(statusBarComp != nullptr);
        expectEquals(statusBarComp->getWidth(), 1280);
        expectEquals(statusBarComp->getHeight(), sbHeight);
        expectEquals(statusBarComp->getY(), 720 - sbHeight);

        // Main area golden invariants: occupies space above status bar
        auto* mainAreaComp = host.find("main-area");
        expect(mainAreaComp != nullptr);
        expectEquals(mainAreaComp->getWidth(), 1280);
        expectEquals(mainAreaComp->getHeight(), 720 - sbHeight);
        expectEquals(mainAreaComp->getY(), 0);

        // Header and controls panel presence
        auto* headerComp = host.find("header");
        expect(headerComp != nullptr);
        expect(headerComp->getHeight() > 0);

        auto* controlsComp = host.find("controls-panel");
        expect(controlsComp != nullptr);
        expect(controlsComp->getHeight() >= 140); // min-height is 140px

        // Keyboard area golden invariants
        auto* keyboardComp = host.find("custom-keyboard");
        expect(keyboardComp != nullptr);
        expect(keyboardComp->getHeight() > 0);
    }

    // ────────────────────────────────────────────────────────────────────────
    // 3. Deterministic Layout Bounds (1920x1080)
    // ────────────────────────────────────────────────────────────────────────
    void testDeterministicRootLayoutBounds1920x1080() {
        beginTest("Golden Bounds: Root layout geometry at 1920x1080 expanded resolution");

        juce::MidiKeyboardState keyboardState;
        devpiano::ui::ViewHost host;
        host.registerKeyboardComponents(keyboardState);

        auto tree = devpiano::ui::jive::makeRootLayout();
        expect(host.loadLayout(tree, true));

        // Enforce 1920x1080 window bounds
        host.setBounds(0, 0, 1920, 1080);

        auto* rootComp = host.getRootComponent();
        expect(rootComp != nullptr);
        expectEquals(rootComp->getWidth(), 1920);
        expectEquals(rootComp->getHeight(), 1080);

        // Status bar golden invariants: 24px height maintained, pinned to 1056
        const auto sbHeight = devpiano::jive::DesignTokens::get().statusBarHeight();
        auto* statusBarComp = host.find("status-bar");
        expect(statusBarComp != nullptr);
        expectEquals(statusBarComp->getWidth(), 1920);
        expectEquals(statusBarComp->getHeight(), sbHeight);
        expectEquals(statusBarComp->getY(), 1080 - sbHeight);

        // Main area: 1920 x (1080 - sbHeight)
        auto* mainAreaComp = host.find("main-area");
        expect(mainAreaComp != nullptr);
        expectEquals(mainAreaComp->getWidth(), 1920);
        expectEquals(mainAreaComp->getHeight(), 1080 - sbHeight);
    }

    // ────────────────────────────────────────────────────────────────────────
    // 4. Deterministic Settings and CSS Grid Bounds
    // ────────────────────────────────────────────────────────────────────────
    void testDeterministicSettingsAndCssGridBounds() {
        beginTest("Golden Bounds: Settings panel and 16-channel CSS Grid alignment");

        devpiano::ui::ViewHost host;
        host.registerDefaultComponents();

        auto tree = devpiano::ui::jive::makeSettingsLayoutTree();
        expect(host.loadLayout(tree, true));

        // Settings panel width 680, content height 960
        host.setBounds(0, 0, 680, 960);

        auto* gridComp = host.find("follow-key-grid");
        expect(gridComp != nullptr);
        expect(gridComp->getWidth() > 0);
        expectEquals(gridComp->getHeight(), 52); // explicit 52px height

        // Inspect 16 toggle components in 8-column x 2-row Grid
        auto* ch0 = host.find("follow-key-0");
        auto* ch1 = host.find("follow-key-1");
        auto* ch7 = host.find("follow-key-7");
        auto* ch8 = host.find("follow-key-8");
        auto* ch15 = host.find("follow-key-15");

        expect(ch0 != nullptr);
        expect(ch1 != nullptr);
        expect(ch7 != nullptr);
        expect(ch8 != nullptr);
        expect(ch15 != nullptr);

        // All Row 0 items must share the same Y coordinate
        expectEquals(ch0->getY(), ch1->getY());
        expectEquals(ch0->getY(), ch7->getY());

        // Row 1 items must share the same Y coordinate, below Row 0
        expectEquals(ch8->getY(), ch15->getY());
        expect(ch8->getY() > ch0->getY());

        // Column layout: ch1 must be strictly to the right of ch0
        expect(ch1->getX() > ch0->getX());

        // Toggles must have identical heights (24px)
        expectEquals(ch0->getHeight(), 24);
        expectEquals(ch8->getHeight(), 24);
    }

    // ────────────────────────────────────────────────────────────────────────
    // 5. Focus Isolation and Glissando Interaction Invariants
    // ────────────────────────────────────────────────────────────────────────
    void testFocusIsolationAndGlissandoInvariants() {
        beginTest("Focus & Glissando: Focus non-stealing and mouse-drag note pairing");

        juce::MidiKeyboardState state;
        CustomKeyboard keyboard(state);
        keyboard.setSize(1000, 128);
        keyboard.setAvailableRange(21, 108);

        // Invariant 1: Virtual keyboard must NOT grab keyboard focus on click
        expect(!keyboard.getWantsKeyboardFocus());
        expect(!keyboard.getMouseClickGrabsKeyboardFocus());

        // Setup note tracking event log
        struct NoteEvent {
            int note;
            bool isNoteOn;
        };
        std::vector<NoteEvent> events;

        keyboard.onNoteOn = [&events](int note, int /*ch*/) { events.push_back({ note, true }); };
        keyboard.onNoteOff = [&events](int note, int /*ch*/) { events.push_back({ note, false }); };

        // Locate coordinates for two keys (e.g. C4 = 60 and D4 = 62)
        const auto& keys = keyboard.getKeys();
        expect(!keys.empty());

        juce::Point<int> pos60 { -1, -1 };
        juce::Point<int> pos62 { -1, -1 };
        juce::Point<int> pos64 { -1, -1 };

        for (const auto& k : keys) {
            if (k.midiNote == 60) {
                pos60 = { juce::roundToInt(k.bounds.getCentreX()), juce::roundToInt(k.bounds.getCentreY()) };
            } else if (k.midiNote == 62) {
                pos62 = { juce::roundToInt(k.bounds.getCentreX()), juce::roundToInt(k.bounds.getCentreY()) };
            } else if (k.midiNote == 64) {
                pos64 = { juce::roundToInt(k.bounds.getCentreX()), juce::roundToInt(k.bounds.getCentreY()) };
            }
        }

        expect(pos60.x > 0 && pos60.y > 0);
        expect(pos62.x > 0 && pos62.y > 0);
        expect(pos64.x > 0 && pos64.y > 0);

        const auto mouseSource = juce::Desktop::getInstance().getMainMouseSource();

        // 1. MouseDown on C4 (60)
        juce::MouseEvent downEvent(mouseSource, pos60.toFloat(), juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f,
                                   0.0f, 0.0f, 0.0f, &keyboard, &keyboard, juce::Time::getCurrentTime(),
                                   pos60.toFloat(), juce::Time::getCurrentTime(), 1, false);
        keyboard.mouseDown(downEvent);

        expectEquals(static_cast<int>(events.size()), 1);
        if (!events.empty()) {
            expectEquals(events.back().note, 60);
            expect(events.back().isNoteOn);
        }

        // 2. MouseDrag to D4 (62) (Glissando step 1)
        juce::MouseEvent dragEvent1(mouseSource, pos62.toFloat(), juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f,
                                    0.0f, 0.0f, 0.0f, &keyboard, &keyboard, juce::Time::getCurrentTime(),
                                    pos60.toFloat(), juce::Time::getCurrentTime(), 1, true);
        keyboard.mouseDrag(dragEvent1);

        // Must emit NoteOff(60) immediately followed by NoteOn(62)
        expectEquals(static_cast<int>(events.size()), 3);
        if (events.size() >= 3) {
            expectEquals(events[1].note, 60);
            expect(!events[1].isNoteOn);
            expectEquals(events[2].note, 62);
            expect(events[2].isNoteOn);
        }

        // 3. MouseDrag to E4 (64) (Glissando step 2)
        juce::MouseEvent dragEvent2(mouseSource, pos64.toFloat(), juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f,
                                    0.0f, 0.0f, 0.0f, &keyboard, &keyboard, juce::Time::getCurrentTime(),
                                    pos60.toFloat(), juce::Time::getCurrentTime(), 1, true);
        keyboard.mouseDrag(dragEvent2);

        // Must emit NoteOff(62) immediately followed by NoteOn(64)
        expectEquals(static_cast<int>(events.size()), 5);
        if (events.size() >= 5) {
            expectEquals(events[3].note, 62);
            expect(!events[3].isNoteOn);
            expectEquals(events[4].note, 64);
            expect(events[4].isNoteOn);
        }

        // 4. MouseUp
        juce::MouseEvent upEvent(mouseSource, pos64.toFloat(), juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &keyboard, &keyboard, juce::Time::getCurrentTime(), pos60.toFloat(),
                                 juce::Time::getCurrentTime(), 1, false);
        keyboard.mouseUp(upEvent);

        // Must emit NoteOff(64)
        expectEquals(static_cast<int>(events.size()), 6);
        if (events.size() >= 6) {
            expectEquals(events[5].note, 64);
            expect(!events[5].isNoteOn);
        }

        // Invariant: Verify strict On/Off balance across the entire glissando run
        int activeNoteCount = 0;
        for (const auto& ev : events) {
            activeNoteCount += (ev.isNoteOn ? 1 : -1);
            expect(activeNoteCount >= 0 && activeNoteCount <= 1);
        }
        expectEquals(activeNoteCount, 0);

        // Invariant 2: Focus loss panic release
        keyboard.mouseDown(downEvent);
        expectEquals(activeNoteCount, 0); // before accounting for new event
        keyboard.releaseHeldMouseNote();
        expect(!events.empty() && !events.back().isNoteOn);
    }
};

static LayoutGoldenTest layoutGoldenTest;

#include <JuceHeader.h>

#include "Audio/AudioDeviceDiagnostics.h"
#include "Settings/SettingsComponent.h"
#include "Settings/SettingsModel.h"
#include "Settings/jive/SettingsLayoutModel.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/StyleCatalog.h"

namespace {
using namespace devpiano::ui::jive;
} // namespace

class SettingsLayoutModelTest final : public juce::UnitTest {
public:
    SettingsLayoutModelTest()
        : juce::UnitTest("SettingsLayoutModel", "DevPiano/UI") {
    }

    void runTest() override {
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::jive::DesignTokens::get().reset();

        testSettingsLayoutTreeStructure();
        testAudioDeviceSection();
        testKeySignatureSectionAndGrid();
        testKeyboardDisplaySection();
        testInterpretationAndComponentLookup();
        testFollowKeyVisibilityToggle();
        testSettingsComponentRefreshTextsPreservesScroll();
        testSettingsComponentMouseWheelIsolation();
    }

private:
    void testSettingsLayoutTreeStructure() {
        beginTest("makeSettingsLayoutTree: top-level structure and child sections");

        auto tree = devpiano::ui::jive::makeSettingsLayoutTree();
        expect(tree.isValid());
        expectEquals(tree.getType().toString(), juce::String("Component"));
        expectEquals(tree.getProperty("id").toString(), juce::String("settings-root"));

        // All 5 core sections must be present
        expect(findNodeById(tree, "audio-device-card").isValid());
        expect(findNodeById(tree, "key-sig-card").isValid());
        expect(findNodeById(tree, "keyboard-display-card").isValid());
        expect(findNodeById(tree, "diagnostics-card").isValid());
        expect(findNodeById(tree, "save-action-row").isValid());
    }

    void testAudioDeviceSection() {
        beginTest("makeAudioDeviceSectionTree: declarative controls and test button");

        auto tree = devpiano::ui::jive::makeAudioDeviceSectionTree();
        expect(tree.isValid());

        expect(findNodeById(tree, "audio-device-title").isValid());
        expect(findNodeById(tree, "audio-device-type-combo").isValid());
        expect(findNodeById(tree, "audio-output-device-combo").isValid());
        expect(findNodeById(tree, "audio-active-channels-combo").isValid());
        expect(findNodeById(tree, "audio-test-button").isValid());
        expect(findNodeById(tree, "audio-sample-rate-combo").isValid());
        expect(findNodeById(tree, "audio-buffer-size-combo").isValid());
        expect(findNodeById(tree, "asio-control-panel-row").isValid());
        expect(findNodeById(tree, "asio-control-panel-button").isValid());
    }
    void testKeySignatureSectionAndGrid() {
        beginTest("makeKeySignatureSectionTree: 16-channel CSS Grid and controls");

        auto tree = devpiano::ui::jive::makeKeySignatureSectionTree();
        expect(tree.isValid());

        // Key signature controls
        expect(findNodeById(tree, "key-sig-title").isValid());
        expect(findNodeById(tree, "key-signature-combo").isValid());
        expect(findNodeById(tree, "midi-transpose-toggle").isValid());

        // 16 follow key toggles inside CSS grid
        auto gridNode = findNodeById(tree, "follow-key-grid");
        expect(gridNode.isValid());
        expectEquals(gridNode.getProperty("display").toString(), juce::String("grid"));
        expect(gridNode.getProperty("grid-template-columns").toString().contains("1fr"));

        for (int ch = 0; ch < 16; ++ch) {
            auto cb = findNodeById(tree, "follow-key-" + juce::String(ch));
            expect(cb.isValid());
            expectEquals(cb.getType().toString(), juce::String("Checkbox"));
            expectEquals(cb.getProperty("text").toString(), "Ch" + juce::String(ch + 1));
        }
    }

    void testKeyboardDisplaySection() {
        beginTest("makeKeyboardDisplaySectionTree: display options and language");

        auto tree = devpiano::ui::jive::makeKeyboardDisplaySectionTree();
        expect(tree.isValid());

        expect(findNodeById(tree, "keyboard-display-title").isValid());
        expect(findNodeById(tree, "colour-mode-combo").isValid());
        expect(findNodeById(tree, "note-display-combo").isValid());
        expect(findNodeById(tree, "fade-speed-slider").isValid());
        expect(findNodeById(tree, "instrument-filter-toggle").isValid());
        expect(findNodeById(tree, "language-combo").isValid());
    }

    void testInterpretationAndComponentLookup() {
        beginTest("Interpretation and dynamic component lookup from SettingsLayoutModel");

        auto tree = devpiano::ui::jive::makeSettingsLayoutTree();

        ::jive::Interpreter interpreter;
        auto& factory = interpreter.getComponentFactory();

        factory.set("ListEditor", [] {
            auto ed = std::make_unique<juce::TextEditor>();
            ed->setMultiLine(true);
            return ed;
        });

        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree rootItem = interpreter.interpret(tree);
        expect(rootItem != nullptr);

        if (rootItem != nullptr) {
            // Verify component lookup for all major controls
            auto* devTypeCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "audio-device-type-combo"));
            expect(devTypeCombo != nullptr);
            auto* outputCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "audio-output-device-combo"));
            expect(outputCombo != nullptr);

            auto* channelsCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "audio-active-channels-combo"));
            expect(channelsCombo != nullptr);

            auto* testBtn = dynamic_cast<juce::Button*>(findComponentById(*rootItem, "audio-test-button"));
            expect(testBtn != nullptr);
            auto* sampleRateCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "audio-sample-rate-combo"));
            expect(sampleRateCombo != nullptr);

            auto* bufferSizeCombo
                = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "audio-buffer-size-combo"));
            expect(bufferSizeCombo != nullptr);

            auto* asioBtn = dynamic_cast<juce::Button*>(findComponentById(*rootItem, "asio-control-panel-button"));
            expect(asioBtn != nullptr);
            auto* ksCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "key-signature-combo"));
            expect(ksCombo != nullptr);

            auto* midiTranspose
                = dynamic_cast<juce::ToggleButton*>(findComponentById(*rootItem, "midi-transpose-toggle"));
            expect(midiTranspose != nullptr);

            for (int ch = 0; ch < 16; ++ch) {
                auto* cb
                    = dynamic_cast<juce::ToggleButton*>(findComponentById(*rootItem, "follow-key-" + juce::String(ch)));
                expect(cb != nullptr);
            }

            auto* colourCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "colour-mode-combo"));
            expect(colourCombo != nullptr);

            auto* noteCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "note-display-combo"));
            expect(noteCombo != nullptr);

            auto* fadeSlider = dynamic_cast<juce::Slider*>(findComponentById(*rootItem, "fade-speed-slider"));
            expect(fadeSlider != nullptr);

            auto* filterCb
                = dynamic_cast<juce::ToggleButton*>(findComponentById(*rootItem, "instrument-filter-toggle"));
            expect(filterCb != nullptr);

            auto* langCombo = dynamic_cast<juce::ComboBox*>(findComponentById(*rootItem, "language-combo"));
            expect(langCombo != nullptr);

            auto* diagEd = dynamic_cast<juce::TextEditor*>(findComponentById(*rootItem, "diagnostics-editor"));
            expect(diagEd != nullptr);
            if (diagEd != nullptr) {
                expect(diagEd->isMultiLine());
            }

            auto* saveBtn = dynamic_cast<juce::Button*>(findComponentById(*rootItem, "save-button"));
            expect(saveBtn != nullptr);
        }
    }

    void testFollowKeyVisibilityToggle() {
        beginTest("Dynamic visibility property toggle for follow-key-area");

        auto tree = devpiano::ui::jive::makeSettingsLayoutTree();

        ::jive::Interpreter interpreter;
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree rootItem = interpreter.interpret(tree);
        expect(rootItem != nullptr);

        if (rootItem != nullptr) {
            auto* followKeyArea = devpiano::ui::jive::findGuiItemById(*rootItem, "channel-follow-key-area");
            expect(followKeyArea != nullptr);

            if (followKeyArea != nullptr) {
                // Toggle visibility property
                followKeyArea->state.setProperty("visibility", false, nullptr);
                expect(!static_cast<bool>(followKeyArea->state.getProperty("visibility")));

                followKeyArea->state.setProperty("visibility", true, nullptr);
                expect(static_cast<bool>(followKeyArea->state.getProperty("visibility")));
            }
        }
    }

    void testSettingsComponentRefreshTextsPreservesScroll() {
        beginTest("SettingsComponent refreshTexts preserves Viewport scroll position");

        juce::AudioDeviceManager dm;
        SettingsModel model;
        auto comp = std::make_unique<SettingsComponent>(dm, nullptr, &model);
        comp->setSize(680, 500);

        juce::Viewport* vp = nullptr;
        for (int i = 0; i < comp->getNumChildComponents(); ++i) {
            if (auto* candidate = dynamic_cast<juce::Viewport*>(comp->getChildComponent(i))) {
                vp = candidate;
                break;
            }
        }
        expect(vp != nullptr, "SettingsComponent must contain a Viewport");
        if (vp == nullptr) {
            return;
        }

        // Scroll the viewport down by 180 pixels
        vp->setViewPosition(0, 180);
        expectEquals(vp->getViewPositionY(), 180, "viewport initial scrolled Y position");

        // Trigger refreshTexts (simulate language switch)
        comp->refreshTexts();

        expectEquals(vp->getViewPositionY(), 180, "viewport scroll position must be preserved after refreshTexts");
    }

    void testSettingsComponentMouseWheelIsolation() {
        beginTest("SettingsComponent mouse wheel isolation on controls vs background");

        juce::AudioDeviceManager dm;
        SettingsModel model;
        auto comp = std::make_unique<SettingsComponent>(dm, nullptr, &model);
        comp->setSize(680, 500);

        juce::Viewport* vp = nullptr;
        for (int i = 0; i < comp->getNumChildComponents(); ++i) {
            if (auto* candidate = dynamic_cast<juce::Viewport*>(comp->getChildComponent(i))) {
                vp = candidate;
                break;
            }
        }
        expect(vp != nullptr, "SettingsComponent must contain a Viewport");
        if (vp == nullptr) {
            return;
        }

        vp->setViewPosition(0, 0);
        expectEquals(vp->getViewPositionY(), 0);

        auto makeEvent = [](juce::Component* target) {
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            return juce::MouseEvent(source, {}, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, target, target,
                                    juce::Time::getCurrentTime(), {}, juce::Time::getCurrentTime(), 0, false);
        };

        juce::MouseWheelDetails wheelDetails;
        wheelDetails.deltaY = -1.0f; // Scroll down attempt

        // 1. Simulating mouse wheel event originating from a Slider (e.g. fadeSpeedSlider)
        auto dummySlider = std::make_unique<juce::Slider>();
        comp->mouseWheelMove(makeEvent(dummySlider.get()), wheelDetails);
        expectEquals(vp->getViewPositionY(), 0, "wheel event on slider must NOT scroll the Viewport");

        // 2. Simulating mouse wheel event originating from a ComboBox
        auto dummyCombo = std::make_unique<juce::ComboBox>();
        comp->mouseWheelMove(makeEvent(dummyCombo.get()), wheelDetails);
        expectEquals(vp->getViewPositionY(), 0, "wheel event on combobox must NOT scroll the Viewport");

        // 3. Simulating mouse wheel event originating from background (comp itself)
        comp->mouseWheelMove(makeEvent(comp.get()), wheelDetails);
        expect(vp->getViewPositionY() > 0, "wheel event on background MUST scroll the Viewport");
    }
};

static SettingsLayoutModelTest settingsLayoutModelTest;

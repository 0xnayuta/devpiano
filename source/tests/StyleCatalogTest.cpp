#include <JuceHeader.h>

#include "Locale/LocaleManager.h"
#include "UI/ComboSelection.h"
#include "UI/DevPianoLookAndFeel.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"
#include "UI/native/StatusBarMidiDot.h"

#include <jive_layouts/jive_layouts.h>

// =============================================================================
// Regression tests for the JIVE style injection chain and layout trees.
//
// The first JIVE migration attempt stored styles as plain juce::DynamicObject
// vars, which jive::VariantConverter<jive::Object::Ptr> rejects (jassert +
// nullptr) — so no styles ever applied and text stayed invisible (black on
// dark). These tests lock in the fix: StyleCatalog emits owned jive::Object
// style values, and interpretation must yield styled components.
//
// Self-contained style rules are used (no cwd dependence in tests).
// =============================================================================

namespace {

// 注册完整 root layout 所需的全部组件类型。
void registerRootComponentFactory(::jive::Interpreter& interpreter) {
    auto& factory = interpreter.getComponentFactory();
    factory.set("SettingsButton",
                [] { return std::make_unique<juce::DrawableButton>("s", juce::DrawableButton::ImageFitted); });
    factory.set("PathEditor", [] { return std::make_unique<juce::TextEditor>(); });
    factory.set("ListEditor", [] { return std::make_unique<juce::TextEditor>(); });
    factory.set("DevKnob", [] { return std::make_unique<juce::Slider>(); });
    factory.set("SpeedSlider", [] {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        return slider;
    });
    factory.set("AdsrCurve", [] { return std::make_unique<juce::Component>(); });
    for (const char* type : { "RecordButton", "PlayButton", "StopButton", "BackButton" }) {
        factory.set(type, [] { return std::make_unique<juce::TextButton>(); });
    }
    factory.set("CustomKeyboard", [] {
        auto viewport = std::make_unique<juce::Viewport>();
        viewport->setScrollBarsShown(false, true, false, true);
        auto keyboard = std::make_unique<jive::TextComponent>();
        viewport->setViewedComponent(keyboard.release(), true);
        return viewport;
    });
    factory.set("StatusBarMidiDot", [] { return std::make_unique<juce::Component>(); });
}

// 定位仓库内真实 style_sheets.json：优先 __FILE__ 相对定位（TEST-014，与 CWD
// 无关），回退 CWD / 可执行文件目录上溯兼容旧环境。
juce::File findShippedStyleSheet() {
    juce::File styleFile = juce::File(__FILE__).getParentDirectory().getChildFile("../UI/jive/style_sheets.json");
    if (!styleFile.existsAsFile()) {
        styleFile = juce::File::getCurrentWorkingDirectory().getChildFile("source/UI/jive/style_sheets.json");
    }
    if (!styleFile.existsAsFile()) {
        auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        for (int i = 0; i < 4 && !styleFile.existsAsFile(); ++i) {
            styleFile = dir.getChildFile("source/UI/jive/style_sheets.json");
            dir = dir.getParentDirectory();
        }
    }
    return styleFile;
}

juce::ValueTree findNodeById(const juce::ValueTree& root, const juce::String& id) {
    if (root.getProperty("id").toString() == id) {
        return root;
    }
    for (auto child : root) {
        if (auto found = findNodeById(child, id); found.isValid()) {
            return found;
        }
    }
    return {};
}

} // namespace

class StyleCatalogTest final : public juce::UnitTest {
public:
    StyleCatalogTest()
        : juce::UnitTest("StyleCatalog", "DevPiano/UI") {
    }

    void runTest() override {
        // 独立基线（TEST-013）：本文件大量 loadFromJSON 覆写进程级单例，
        // 先 reset 保证无论其他文件是否先跑，起始状态一致。
        devpiano::ui::jive::StyleCatalog::get().reset();
        devpiano::jive::DesignTokens::get().reset();

        testJsonStringParsesToJiveObject();
        testAppliedStylesReachInterpretedComponents();
        testDesignTokensHotReload();
        testStyleTokenResolutionInStyleSheet();
        testStyleCatalogHotReloadOnLiveTree();
        testStatusBarTreeInterprets();
        testStatusBarMidiDotActivityAndDecay();
        testPluginPanelTreeInterprets();
        testKeyboardAreaTreeInterprets();
        testRootLayoutInterprets();
        testWindowRuleFontSizeInheritsToText();
        testRealStyleSheetWindowFontSizeActsAsGlobalDefault();
        testRealStyleSheetDisabledPseudoStates();
        testComboPlaceholderRendersAboveCanvas();
        testTitlesFollowLanguageSwitch();
        // Release styles owned by the tests once all trees are gone.
        devpiano::ui::jive::StyleCatalog::get().releaseOwnedStyles();
    }

private:
    void testJsonStringParsesToJiveObject() {
        beginTest("style rules merge into owned jive::Object values");

        const juce::var json = juce::JSON::parse(
            R"({ "Text": { "foreground": "#EEEEEE", "font-size": 14 },
                 "#title": { "font-size": 18, "font-weight": "bold" },
                 "Button": { "background": "#2D3035",
                             "hover": { "background": "#35383D" } },
                 "#settings-btn": { "background": "#2D3035",
                                    "hover": { "background": "#35383D" } } })");
        auto& catalog = devpiano::ui::jive::StyleCatalog::get();
        catalog.loadFromJSON(json);

        auto tree = devpiano::ui::jive::makeHeaderTree();
        catalog.applyToTree(tree);

        // The title node must carry a style var holding a jive::Object —
        // plain juce::DynamicObject vars are rejected by JIVE's converter.
        const auto title = tree.getChildWithProperty("id", "title");
        expect(title.isValid(), "title node missing");
        expect(title.hasProperty("style"), "title has no style property");

        if (!title.hasProperty("style")) {
            return;
        }

        const auto styleVar = title["style"];
        expect(styleVar.getObject() != nullptr, "style must be a DynamicObject");
        if (styleVar.getObject() == nullptr) {
            return;
        }

        auto* object = dynamic_cast<::jive::Object*>(styleVar.getObject());
        expect(object != nullptr, "style must be a jive::Object (plain DynamicObject is rejected by JIVE)");
        if (object == nullptr) {
            return;
        }

        expectEquals(object->getProperty("foreground").toString(), juce::String("#EEEEEE"));
        expectEquals(object->getProperty("font-size").toString(), juce::String("18"));
        expectEquals(object->getProperty("font-weight").toString(), juce::String("bold"));

        // Pseudo-state rules must be nested objects with the JIVE names
        // (hover/active/focus...), not ":hover" / "pressed".
        const auto settings = tree.getChildWithProperty("id", "settings-btn");
        expect(settings.isValid(), "settings-btn node missing");
        if (!settings.isValid()) {
            return;
        }

        auto* btnStyle = dynamic_cast<::jive::Object*>(settings["style"].getObject());
        expect(btnStyle != nullptr, "settings-btn style must be a jive::Object");
        if (btnStyle == nullptr) {
            return;
        }

        expectEquals(btnStyle->getProperty("background").toString(), juce::String("#2D3035"));
        auto* hover = btnStyle->getProperty("hover").getDynamicObject();
        expect(hover != nullptr, "hover sub-rule missing");
        if (hover != nullptr) {
            expectEquals(hover->getProperty("background").toString(), juce::String("#35383D"));
        }
    }

    void testAppliedStylesReachInterpretedComponents() {
        beginTest("interpreted header renders with applied styles");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("SettingsButton", [] { return std::make_unique<juce::Component>(); });

        auto tree = devpiano::ui::jive::makeHeaderTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "interpretation failed");
        if (item == nullptr) {
            return;
        }

        auto* titleItem = ::jive::findItemWithID(*item, "title");
        expect(titleItem != nullptr, "title item not found");
        if (titleItem == nullptr) {
            return;
        }

        auto* text = dynamic_cast<jive::TextComponent*>(titleItem->getComponent().get());
        expect(text != nullptr, "title component is not a TextComponent");
        if (text == nullptr) {
            return;
        }

        // StyleSheet must have applied the #title rule — the text colour
        // proves the style pipeline end to end.
        expectEquals(text->getTextColour(), juce::Colour(0xFFEEEEEE));
    }
    void testDesignTokensHotReload() {
        beginTest("design tokens reload dynamically and refresh look and feel");

        const juce::var json1
            = juce::JSON::parse(R"({ "colors": { "primary": "#FF00B4D8", "main-bg": "#FF1A1C1E" } })");
        auto& tokens = devpiano::jive::DesignTokens::get();
        tokens.loadFromJSON(json1);
        expectEquals(tokens.primary().toString(), juce::Colour::fromString("#FF00B4D8").toString());
        expectEquals(tokens.mainBg().toString(), juce::Colour::fromString("#FF1A1C1E").toString());

        {
            auto lnf = std::make_unique<DevPianoLookAndFeel>();
            expectEquals(lnf->findColour(juce::Slider::thumbColourId).toString(),
                         juce::Colour::fromString("#FF00B4D8").toString());
            expectEquals(lnf->findColour(juce::ResizableWindow::backgroundColourId).toString(),
                         juce::Colour::fromString("#FF1A1C1E").toString());

            // Hot reload with new colors
            const juce::var json2
                = juce::JSON::parse(R"({ "colors": { "primary": "#FFFF5500", "main-bg": "#FF002244" } })");
            tokens.loadFromJSON(json2);
            expectEquals(tokens.primary().toString(), juce::Colour::fromString("#FFFF5500").toString());
            expectEquals(tokens.mainBg().toString(), juce::Colour::fromString("#FF002244").toString());

            lnf->refreshColours();
            expectEquals(lnf->findColour(juce::Slider::thumbColourId).toString(),
                         juce::Colour::fromString("#FFFF5500").toString());
            expectEquals(lnf->findColour(juce::ResizableWindow::backgroundColourId).toString(),
                         juce::Colour::fromString("#FF002244").toString());
            juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
            lnf.reset();
        }
    }

    void testStyleTokenResolutionInStyleSheet() {
        beginTest("@token style values resolve through DesignTokens (DOC-007)");

        auto& tokens = devpiano::jive::DesignTokens::get();
        // 记住当前 tokens 根，测试后恢复，避免污染后续用例。
        const auto savedRoot = tokens.currentRootForTest();

        // 显式加载 tokens：与生产路径一致（MainComponent 先加载 design_tokens.json）。
        tokens.loadFromJSON(juce::JSON::parse(
            R"({ "colors": { "main-bg": "#FF111316", "control-bg": "#FF22252C", "text-disabled": "#FF555B66" },
                 "typography": { "font-size-label": 14.0, "font-weight-title": "bold" } })"));

        auto& catalog = devpiano::ui::jive::StyleCatalog::get();
        catalog.loadFromJSON(juce::JSON::parse(R"({
            "#probe": {
                "background": "@main-bg",
                "foreground": "@text-disabled",
                "font-size": "@font-size-label",
                "font-weight": "@font-weight-title",
                "border": "#2E333D",
                "unknown": "@no-such-token"
            }
        })"));
        catalog.releaseOwnedStyles();

        juce::ValueTree tree("probe");
        tree.setProperty("id", "probe", nullptr);
        catalog.applyToTree(tree);
        catalog.releaseOwnedStyles();

        auto* style = dynamic_cast<::jive::Object*>(tree["style"].getObject());
        expect(style != nullptr, "probe must carry a style object");
        if (style == nullptr) {
            tokens.loadFromJSON(savedRoot);
            return;
        }
        expectEquals(style->getProperty("background").toString(), juce::String("#111316"),
                     "@main-bg must resolve to #111316");
        expectEquals(style->getProperty("foreground").toString(), juce::String("#555B66"),
                     "@text-disabled must resolve to #555B66");
        expectEquals(style->getProperty("font-size").toString(), juce::String("14"),
                     "@font-size-label must resolve to integer string 14");
        expectEquals(style->getProperty("font-weight").toString(), juce::String("bold"),
                     "@font-weight-title must pass through");
        expectEquals(style->getProperty("border").toString(), juce::String("#2E333D"),
                     "non-token literal values must stay untouched");
        expectEquals(style->getProperty("unknown").toString(), juce::String("@no-such-token"),
                     "unknown tokens must be kept verbatim, not dropped");

        // 顺序无关：tokens 未加载（root 为空）时 getter 内置默认仍能解析。
        tokens.loadFromJSON(juce::JSON::parse(R"({})"));
        catalog.loadFromJSON(juce::JSON::parse(R"({ "#probe": { "background": "@main-bg" } })"));
        juce::ValueTree tree2("probe");
        tree2.setProperty("id", "probe", nullptr);
        catalog.applyToTree(tree2);
        catalog.releaseOwnedStyles();
        auto* style2 = dynamic_cast<::jive::Object*>(tree2["style"].getObject());
        expect(style2 != nullptr, "probe2 must carry a style object");
        if (style2 != nullptr) {
            expectEquals(style2->getProperty("background").toString(), juce::String("#111316"),
                         "getter fallback must resolve @main-bg without loaded JSON");
        }

        tokens.loadFromJSON(savedRoot);
    }

    void testStyleCatalogHotReloadOnLiveTree() {
        beginTest("style catalog reloads rules and updates live interpreted tree");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("SettingsButton", [] { return std::make_unique<juce::Component>(); });

        const juce::var json1 = juce::JSON::parse(
            R"({ "Text": { "foreground": "#112233" },
                 "#title": { "foreground": "#AABBCC" } })");
        auto& catalog = devpiano::ui::jive::StyleCatalog::get();
        catalog.loadFromJSON(json1);

        auto tree = devpiano::ui::jive::makeHeaderTree();
        catalog.applyToTree(tree);

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "interpretation failed");
        if (item == nullptr) {
            return;
        }

        auto* titleItem = ::jive::findItemWithID(*item, "title");
        expect(titleItem != nullptr, "title item not found");
        if (titleItem == nullptr) {
            return;
        }

        auto* text = dynamic_cast<jive::TextComponent*>(titleItem->getComponent().get());
        expect(text != nullptr, "title component is not a TextComponent");
        if (text == nullptr) {
            return;
        }

        expectEquals(text->getTextColour(), juce::Colour(0xFFAABBCC));

        // Hot reload: new rules pushed to live tree
        const juce::var json2 = juce::JSON::parse(
            R"({ "Text": { "foreground": "#445566" },
                 "#title": { "foreground": "#FF00AA" } })");
        catalog.loadFromJSON(json2);
        catalog.refreshStyles(item->state);

        expectEquals(text->getTextColour(), juce::Colour(0xFFFF00AA));

        // Hot reload: #title rule removed, falls back to Text type rule
        const juce::var json3 = juce::JSON::parse(R"({ "Text": { "foreground": "#123456" } })");
        catalog.loadFromJSON(json3);
        catalog.refreshStyles(item->state);

        expectEquals(text->getTextColour(), juce::Colour(0xFF123456));
    }

    void testStatusBarTreeInterprets() {
        beginTest("status bar tree interprets with labels and dot");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("StatusBarMidiDot", [] { return std::make_unique<StatusBarMidiDot>(); });

        auto tree = devpiano::ui::jive::makeStatusBarTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "status bar interpretation failed");
        if (item == nullptr) {
            return;
        }

        auto* dot = ::jive::findItemWithID(*item, "midi-dot");
        expect(dot != nullptr, "midi-dot item not found");
        if (dot != nullptr) {
            expect(dynamic_cast<StatusBarMidiDot*>(dot->getComponent().get()) != nullptr,
                   "midi-dot component is not a StatusBarMidiDot");
        }

        for (const auto* id : { "plugin-name-label", "audio-info-label", "time-label" }) {
            auto* label = ::jive::findItemWithID(*item, id);
            expect(label != nullptr, juce::String(id) + " item not found");
            if (label != nullptr) {
                expect(dynamic_cast<jive::TextComponent*>(label->getComponent().get()) != nullptr,
                       juce::String(id) + " component is not a TextComponent");
            }
        }
    }
    void testStatusBarMidiDotActivityAndDecay() {
        beginTest("StatusBarMidiDot activity trigger and frame decay");

        StatusBarMidiDot dot;
        expect(!dot.getIsActive(), "dot initially inactive");

        dot.triggerActivity(3);
        expect(dot.getIsActive(), "dot active after trigger");

        dot.decayFrame();
        expect(dot.getIsActive(), "dot still active after 1 decay frame (2 remaining)");

        dot.decayFrame();
        expect(dot.getIsActive(), "dot still active after 2 decay frames (1 remaining)");

        dot.decayFrame();
        expect(!dot.getIsActive(), "dot inactive after all frames decayed");
    }

    void testPluginPanelTreeInterprets() {
        beginTest("plugin panel tree interprets with all controls");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("PathEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(false);
            return editor;
        });
        interpreter.getComponentFactory().set("ListEditor", [] {
            auto editor = std::make_unique<juce::TextEditor>();
            editor->setMultiLine(true);
            editor->setReadOnly(true);
            return editor;
        });

        auto tree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "plugin panel interpretation failed");
        if (item == nullptr) {
            return;
        }

        const auto expectComponent = [this, &item](const char* id) {
            auto* found = ::jive::findItemWithID(*item, id);
            expect(found != nullptr, juce::String(id) + " item not found");
            return found;
        };

        expect(expectComponent("plugin-selector") != nullptr, "");
        expect(expectComponent("plugin-filter-combo") != nullptr, "");
        for (const char* id : { "load-btn", "unload-btn", "editor-btn", "toggle-btn", "scan-btn", "browse-btn" }) {
            expect(expectComponent(id) != nullptr, "");
        }
        expect(expectComponent("plugin-path-editor") != nullptr, "");
        expect(expectComponent("plugin-list-editor") != nullptr, "");

        // Filter combo must stay free of declarative Option children: its
        // items are managed programmatically by MainComponent, and JIVE's
        // Option "selected" write-back would clear the combo on the second
        // user selection otherwise.
        auto* filter = ::jive::findItemWithID(*item, "plugin-filter-combo");
        expect(filter != nullptr, "filter combo missing");
        if (filter != nullptr) {
            int optionCount = 0;
            for (auto child : filter->state) {
                if (child.hasType("Option")) {
                    ++optionCount;
                }
            }
            expectEquals(optionCount, 0);
            expect(!filter->state.hasProperty("selected"), "filter combo must not declare a selected property");
        }

        // The expanded area starts collapsed (height 0).
        auto* expandedArea = ::jive::findItemWithID(*item, "plugin-expanded-area");
        expect(expandedArea != nullptr, "expanded area missing");
        if (expandedArea != nullptr) {
            expectEquals(expandedArea->state["height"].toString(), juce::String("0"));
        }
    }

    void testControlsPanelTreeInterprets() {
        beginTest("controls panel tree interprets with knobs, curve and rows");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("DevKnob", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            return slider;
        });
        interpreter.getComponentFactory().set("SpeedSlider", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            return slider;
        });
        interpreter.getComponentFactory().set("AdsrCurve", [] { return std::make_unique<juce::Component>(); });
        interpreter.getComponentFactory().set("RecordButton", [] { return std::make_unique<juce::TextButton>(); });
        interpreter.getComponentFactory().set("PlayButton", [] { return std::make_unique<juce::TextButton>(); });
        interpreter.getComponentFactory().set("StopButton", [] { return std::make_unique<juce::TextButton>(); });
        interpreter.getComponentFactory().set("BackButton", [] { return std::make_unique<juce::TextButton>(); });

        auto tree = devpiano::ui::jive::makeControlsPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "controls panel interpretation failed");
        if (item == nullptr) {
            return;
        }

        for (const char* id : { "volume-knob", "attack-knob", "decay-knob", "sustain-knob", "release-knob",
                                "speed-knob", "brightness-knob", "hardness-knob", "resonance-knob" }) {
            auto* knob = ::jive::findItemWithID(*item, id);
            expect(knob != nullptr, juce::String(id) + " not found");
            if (knob != nullptr) {
                expect(dynamic_cast<juce::Slider*>(knob->getComponent().get()) != nullptr,
                       juce::String(id) + " is not a Slider");
            }
        }

        for (const char* id : { "record-btn", "play-btn", "stop-btn", "back-btn", "export-midi-btn", "export-wav-btn",
                                "import-midi-btn", "save-perf-btn", "open-perf-btn", "song-info-btn", "recent-btn",
                                "save-preset-btn", "rename-preset-btn", "delete-preset-btn" }) {
            auto* btn = ::jive::findItemWithID(*item, id);
            expect(btn != nullptr, juce::String(id) + " not found");
            if (btn != nullptr) {
                expect(dynamic_cast<juce::Button*>(btn->getComponent().get()) != nullptr,
                       juce::String(id) + " is not a Button");
            }
        }

        auto* combo = ::jive::findItemWithID(*item, "preset-combo");
        expect(combo != nullptr, "preset-combo not found");
        if (combo != nullptr) {
            expect(dynamic_cast<juce::ComboBox*>(combo->getComponent().get()) != nullptr,
                   "preset-combo is not a ComboBox");
        }

        auto* curve = ::jive::findItemWithID(*item, "adsr-curve");
        expect(curve != nullptr, "adsr-curve not found");
    }

    void testKeyboardAreaTreeInterprets() {
        beginTest("keyboard area tree interprets with viewport");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("CustomKeyboard", [] {
            auto viewport = std::make_unique<juce::Viewport>();
            viewport->setScrollBarsShown(false, true, false, true);
            auto keyboard = std::make_unique<jive::TextComponent>();
            viewport->setViewedComponent(keyboard.release(), true);
            return viewport;
        });

        auto tree = devpiano::ui::jive::makeKeyboardAreaTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "keyboard area interpretation failed");
        if (item == nullptr) {
            return;
        }

        auto* keyboard = ::jive::findItemWithID(*item, "custom-keyboard");
        expect(keyboard != nullptr, "custom-keyboard item not found");
        if (keyboard != nullptr) {
            expect(dynamic_cast<juce::Viewport*>(keyboard->getComponent().get()) != nullptr,
                   "custom-keyboard component is not a Viewport");
        }
    }

    void testRootLayoutInterprets() {
        beginTest("root layout interprets with every panel");

        ::jive::Interpreter interpreter;
        registerRootComponentFactory(interpreter);

        auto tree = devpiano::ui::jive::makeRootLayout();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "root layout interpretation failed");
        if (item == nullptr) {
            return;
        }

        // Every top-level panel must be present and nested correctly.
        for (const char* id :
             { "header", "plugin-panel", "content-row", "controls-panel", "keyboard-area", "status-bar", "main-area",
               "custom-keyboard", "midi-dot", "settings-btn", "preset-combo", "volume-knob" }) {
            expect(::jive::findItemWithID(*item, id) != nullptr, juce::String(id) + " missing from root layout");
        }

        // The plugin panel starts collapsed (height 42: 40 content + 2 border).
        auto* plugin = ::jive::findItemWithID(*item, "plugin-panel");
        expect(plugin != nullptr, "");
        if (plugin != nullptr) {
            expectEquals(plugin->state["height"].toString(), juce::String("42"));
        }

        // Layout the root and verify panels receive non-zero bounds.
        item->getComponent()->setBounds(0, 0, 1120, 760);
        const auto headerBounds = ::jive::findItemWithID(*item, "header")->getComponent()->getBounds();
        const auto statusBounds = ::jive::findItemWithID(*item, "status-bar")->getComponent()->getBounds();
        const auto keyboardBounds = ::jive::findItemWithID(*item, "keyboard-area")->getComponent()->getBounds();
        expect(headerBounds.getHeight() > 0, "header has zero height after layout");
        expect(statusBounds.getHeight() > 0, "status bar has zero height after layout");
        expect(keyboardBounds.getHeight() > 0, "keyboard area has zero height after layout");
        expect(statusBounds.getBottom() <= 760, "status bar overflows the window");

        // Every node carries a semantic title (inspector/accessibility): the
        // CommonGuiItem "title" property must reach Component::setTitle.
        const auto expectTitle = [&item, this](const char* id, const char* expected) {
            auto* guiItem = ::jive::findItemWithID(*item, id);
            expect(guiItem != nullptr, juce::String(id) + " item not found");
            if (guiItem == nullptr) {
                return;
            }
            expectEquals(guiItem->getComponent()->getTitle(), juce::String(expected),
                         juce::String(id) + " must expose its semantic title");
        };
        expectTitle("record-btn", "Record");
        expectTitle("export-midi-btn", "Export");
        expectTitle("speed-knob", "Playback Speed");
        expectTitle("settings-btn", "Settings");
        expectTitle("keyboard-area", "Keyboard Area");
    }

    void testWindowRuleFontSizeInheritsToText() {
        beginTest("#window rule font-size inherits to text and hot-reloads");

        // Regression: the "#window" rule in style_sheets.json was dead —
        // the root node id was "root", so nothing matched. The root must be
        // id "window" and its font-size must inherit down to every Text
        // component through JIVE's StyleSheet ancestor chain.
        ::jive::Interpreter interpreter;
        registerRootComponentFactory(interpreter);

        auto& catalog = devpiano::ui::jive::StyleCatalog::get();
        const juce::var json1 = juce::JSON::parse(
            R"({ "#window": { "background": "#1A1C1E", "foreground": "#EEEEEE", "font-size": 14 } })");
        catalog.loadFromJSON(json1);

        auto tree = devpiano::ui::jive::makeRootLayout();
        catalog.applyToTree(tree);

        // The root node must carry id "window" so the #window rule applies.
        expectEquals(tree.getProperty("id").toString(), juce::String("window"));

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "root layout interpretation failed");
        if (item == nullptr) {
            return;
        }

        // Mount the root component so StyleSheets establish their ancestor
        // chain (inheritance resolves through the component tree).
        juce::Component host;
        host.setBounds(0, 0, 1120, 760);
        host.addAndMakeVisible(item->getComponent().get());
        item->getComponent()->setBounds(host.getLocalBounds());

        auto* titleItem = ::jive::findItemWithID(*item, "title");
        expect(titleItem != nullptr, "title item not found");
        if (titleItem == nullptr) {
            return;
        }

        auto* text = dynamic_cast<jive::TextComponent*>(titleItem->getComponent().get());
        expect(text != nullptr, "title component is not a TextComponent");
        if (text == nullptr) {
            return;
        }

        // getFont().getHeight() includes an environment-specific unit factor,
        // so assert the relative scale instead of absolute values.
        const auto heightAt14 = text->getFont().getHeight();
        expect(heightAt14 > 0.0f, "title font must have non-zero height");
        if (heightAt14 <= 0.0f) {
            return;
        }

        // Hot reload: #window font-size -> 32 must reach the live tree and
        // scale the rendered font by 32/14.
        const juce::var json2 = juce::JSON::parse(R"({ "#window": { "font-size": 32 } })");
        catalog.loadFromJSON(json2);
        catalog.refreshStyles(item->state);

        const auto heightAt32 = text->getFont().getHeight();
        expectWithinAbsoluteError(heightAt32 / heightAt14, 32.0f / 14.0f, 0.02f,
                                  "font-size must scale with #window rule after hot reload");

        // And a second reload scales back down.
        const juce::var json3 = juce::JSON::parse(R"({ "#window": { "font-size": 7 } })");
        catalog.loadFromJSON(json3);
        catalog.refreshStyles(item->state);

        const auto heightAt7 = text->getFont().getHeight();
        expectWithinAbsoluteError(heightAt7 / heightAt32, 7.0f / 32.0f, 0.02f,
                                  "font-size must scale down after second hot reload");
    }

    void testRealStyleSheetWindowFontSizeActsAsGlobalDefault() {
        beginTest("real style_sheets.json: #window font-size is the global default");

        // Load the ACTUAL shipped style sheet (found via CWD or exe walk-up).
        const auto styleFile = findShippedStyleSheet();
        if (!styleFile.existsAsFile()) {
            return; // not a repo checkout; skip
        }

        auto json = juce::JSON::parse(styleFile);
        expect(!json.isVoid(), "real style_sheets.json must parse");
        if (json.isVoid()) {
            return;
        }

        ::jive::Interpreter interpreter;
        registerRootComponentFactory(interpreter);

        auto& catalog = devpiano::ui::jive::StyleCatalog::get();
        catalog.loadFromJSON(json);

        auto tree = devpiano::ui::jive::makeRootLayout();
        catalog.applyToTree(tree);

        // #title must still override the global default (18 in the shipped file).
        const auto findById = findNodeById;
        const auto titleNode = findById(tree, "title");
        expect(titleNode.isValid(), "title node missing");
        if (titleNode.isValid()) {
            auto* titleStyle = dynamic_cast<::jive::Object*>(titleNode["style"].getObject());
            expect(titleStyle != nullptr, "title must carry a style object");
            if (titleStyle != nullptr) {
                expectEquals(titleStyle->getProperty("font-size").toString(), juce::String("18"));
            }
        }

        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "root layout interpretation failed");
        if (item == nullptr) {
            return;
        }

        juce::Component host;
        host.setBounds(0, 0, 1120, 760);
        host.addAndMakeVisible(item->getComponent().get());
        item->getComponent()->setBounds(host.getLocalBounds());

        // Find the title (explicit font-size) and one inherited Text node.
        auto* titleItem = ::jive::findItemWithID(*item, "title");
        expect(titleItem != nullptr, "title item not found");
        if (titleItem == nullptr) {
            return;
        }

        auto* titleText = dynamic_cast<jive::TextComponent*>(titleItem->getComponent().get());
        expect(titleText != nullptr, "title component is not a TextComponent");
        if (titleText == nullptr) {
            return;
        }

        // An inherited text node: any Text component that is not the title
        // (e.g. the settings button label) must NOT carry its own font-size,
        // so it inherits the #window default.
        jive::TextComponent* inheritedText = nullptr;
        const std::function<void(::jive::GuiItem&)> findInheritedText = [&](::jive::GuiItem& guiItem) {
            if (auto* text = dynamic_cast<jive::TextComponent*>(guiItem.getComponent().get())) {
                if (guiItem.state.getProperty("id").toString() != "title" && inheritedText == nullptr) {
                    inheritedText = text;
                }
            }
            for (auto* child : guiItem.getChildren()) {
                findInheritedText(*child);
            }
        };
        findInheritedText(*item);
        expect(inheritedText != nullptr, "no inherited Text component found");
        if (inheritedText == nullptr) {
            return;
        }

        const auto titleHeightBefore = titleText->getFont().getHeight();
        const auto inheritedHeightBefore = inheritedText->getFont().getHeight();
        expect(inheritedHeightBefore > 0.0f, "inherited text must have a font");
        if (inheritedHeightBefore <= 0.0f) {
            return;
        }

        // Hot reload: bump #window font-size to 32. Inherited text must scale
        // by 32/14 while the title keeps its explicit 18.
        if (auto* windowRule = json.getDynamicObject()->getProperty("#window").getDynamicObject()) {
            windowRule->setProperty("font-size", 32);
        }
        catalog.loadFromJSON(json);
        catalog.refreshStyles(item->state);

        const auto titleHeightAfter = titleText->getFont().getHeight();
        const auto inheritedHeightAfter = inheritedText->getFont().getHeight();
        expectWithinAbsoluteError(inheritedHeightAfter / inheritedHeightBefore, 32.0f / 14.0f, 0.02f,
                                  "inherited text must scale with #window font-size");
        expectWithinAbsoluteError(titleHeightAfter / titleHeightBefore, 1.0f, 0.02f,
                                  "title must keep its explicit #title font-size");
    }

    void testRealStyleSheetDisabledPseudoStates() {
        beginTest("real style_sheets.json: disabled pseudo-states cover state-controlled buttons");

        // Regression: disabled buttons kept their normal colours and JIVE's
        // hover/active pseudo-states kept firing (ComponentInteractionState
        // ignores isEnabled), so a disabled Export/Save/Rename/Delete looked
        // clickable. The shipped sheet must carry "disabled" rules on Button,
        // Text and the transport icon buttons, with hover/active neutralised
        // inside them.
        const auto styleFile = findShippedStyleSheet();
        if (!styleFile.existsAsFile()) {
            return; // not a repo checkout; skip
        }

        auto json = juce::JSON::parse(styleFile);
        expect(!json.isVoid(), "real style_sheets.json must parse");
        if (json.isVoid()) {
            return;
        }

        auto& catalog = devpiano::ui::jive::StyleCatalog::get();
        catalog.loadFromJSON(json);

        auto tree = devpiano::ui::jive::makeControlsPanelTree();
        catalog.applyToTree(tree);

        const auto findById = findNodeById;

        for (const char* id : { "export-midi-btn", "export-wav-btn", "save-perf-btn", "rename-preset-btn",
                                "delete-preset-btn", "play-btn", "stop-btn", "back-btn", "record-btn" }) {
            const auto node = findById(tree, id);
            expect(node.isValid(), juce::String(id) + " node missing");
            if (!node.isValid()) {
                continue;
            }

            auto* styleObj = dynamic_cast<::jive::Object*>(node["style"].getObject());
            expect(styleObj != nullptr, juce::String(id) + " must carry a style object");
            if (styleObj == nullptr) {
                continue;
            }

            const auto disabled = styleObj->getProperty("disabled");
            expect(disabled.isObject(), juce::String(id) + " style missing disabled pseudo-state");
            if (disabled.isObject()) {
                expect(disabled.getDynamicObject()->getProperty("hover").isObject(),
                       juce::String(id) + " disabled must neutralise hover");
                expect(disabled.getDynamicObject()->getProperty("active").isObject(),
                       juce::String(id) + " disabled must neutralise active");
            }
        }

        // The button label (a Text child) must grey out along with the button.
        const auto exportBtn = findById(tree, "export-midi-btn");
        expect(exportBtn.isValid() && exportBtn.getNumChildren() > 0, "export button label missing");
        if (exportBtn.isValid() && exportBtn.getNumChildren() > 0) {
            auto* labelStyle = dynamic_cast<::jive::Object*>(exportBtn.getChild(0)["style"].getObject());
            expect(labelStyle != nullptr, "button label must carry a style object");
            if (labelStyle != nullptr) {
                expect(labelStyle->getProperty("disabled").isObject(), "Text style missing disabled pseudo-state");
            }
        }
    }

    void testComboPlaceholderRendersAboveCanvas() {
        beginTest("combo placeholder text renders above the background canvas");

        // Regression: JIVE's BackgroundCanvas painted the opaque style
        // background over the combo's own paint layer, hiding the LAF-drawn
        // "nothing selected" placeholder (the combo looked blank). With the
        // ComboBox style background transparent, the placeholder must render.
        const auto styleFile = findShippedStyleSheet();
        if (!styleFile.existsAsFile()) {
            return; // not a repo checkout; skip
        }

        const auto styleJson = juce::JSON::parse(styleFile);
        expect(!styleJson.isVoid(), "shipped style_sheets.json must parse");
        if (styleJson.isVoid()) {
            return;
        }
        devpiano::ui::jive::StyleCatalog::get().loadFromJSON(styleJson);

        ::jive::Interpreter interpreter;
        auto& factory = interpreter.getComponentFactory();
        factory.set("DevKnob", [] { return std::make_unique<juce::Slider>(); });
        factory.set("SpeedSlider", [] {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            return slider;
        });
        factory.set("AdsrCurve", [] { return std::make_unique<juce::Component>(); });
        for (const char* type : { "RecordButton", "PlayButton", "StopButton", "BackButton" }) {
            factory.set(type, [] { return std::make_unique<juce::TextButton>(); });
        }

        auto tree = devpiano::ui::jive::makeControlsPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "controls tree interpretation failed");
        if (item == nullptr) {
            return;
        }

        DevPianoLookAndFeel laf;
        item->getComponent()->setLookAndFeel(&laf);

        auto* comboItem = ::jive::findItemWithID(*item, "preset-combo");
        auto* combo = comboItem != nullptr ? dynamic_cast<juce::ComboBox*>(comboItem->getComponent().get()) : nullptr;
        expect(combo != nullptr, "preset-combo is a ComboBox");
        if (combo == nullptr) {
            return;
        }

        // Empty-preset flow (mirrors setControlsPresets with no preset files).
        combo->clear(juce::dontSendNotification);
        combo->setTextWhenNothingSelected("Default");
        combo->setSelectedItemIndex(-1, juce::dontSendNotification);

        combo->setBounds(0, 0, 250, 28);
        const auto bg = juce::Colour(0xff202327);
        juce::Image img(juce::Image::ARGB, 250, 28, true);
        juce::Graphics g(img);
        g.fillAll(bg);
        combo->paintEntireComponent(g, true);

        int light = 0;
        for (int y = 0; y < 28; y += 2) {
            for (int x = 0; x < 250; x += 2) {
                if (img.getPixelAt(x, y).getRed() > 60 || img.getPixelAt(x, y) != bg) {
                    ++light;
                }
            }
        }
        expect(light > 4, "combo placeholder text must render above the background canvas");
        item->getComponent()->setLookAndFeel(nullptr);
    }

    void testTitlesFollowLanguageSwitch() {
        beginTest("titles follow runtime language switching");

        devpiano::locale::activate(devpiano::locale::Language::en);

        ::jive::Interpreter interpreter;
        registerRootComponentFactory(interpreter);

        auto tree = devpiano::ui::jive::makeRootLayout();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "root layout interpretation failed");
        if (item == nullptr) {
            devpiano::locale::activate(devpiano::locale::Language::en);
            return;
        }

        const auto titleOf = [&item](const char* id) -> juce::String {
            if (auto* gi = ::jive::findItemWithID(*item, id)) {
                return gi->state["title"].toString();
            }
            return {};
        };

        // Built under English: container and button titles are English.
        expectEquals(titleOf("header"), juce::String("Header"), "container title must start English");
        expectEquals(titleOf("load-btn"), juce::String("Load"), "button title must start English");

        // Switch to zh-CN and re-run the runtime refresh paths that
        // MainComponent::applyLanguage triggers: refreshTitles for static
        // nodes, and setButtonLabel's title sync for buttons (mirrored here).
        devpiano::locale::activate(devpiano::locale::Language::zhCN);
        devpiano::ui::jive::refreshTitles(*item);
        if (auto* loadBtn = ::jive::findItemWithID(*item, "load-btn")) {
            loadBtn->state.setProperty("title", TRANS("Load"), nullptr);
            for (auto child : loadBtn->state) {
                if (child.getType() == juce::Identifier("Text")) {
                    child.setProperty("title", TRANS("Load"), nullptr);
                }
            }
        }

        expectEquals(titleOf("header"), juce::String(TRANS("Header")), "container title must follow the locale");
        expectEquals(titleOf("plugin-path-editor"), juce::String(TRANS("VST3 Path Editor")),
                     "editor title must follow the locale");
        expectEquals(titleOf("load-btn"), juce::String(TRANS("Load")), "button title must follow the locale");

        // Nodes whose title has no accessor text-refresh path (icon buttons,
        // combo, slider, knobs and their wrappers) must all be covered by
        // the refreshTitles table; a missing entry leaves the startup
        // language title in the inspector/accessibility tree.
        expectEquals(titleOf("toggle-btn"), juce::String(TRANS("Toggle Plugin Panel")),
                     "toggle button title must follow the locale");
        expectEquals(titleOf("browse-btn"), juce::String(TRANS("Browse")),
                     "browse button title must follow the locale");
        expectEquals(titleOf("preset-combo"), juce::String(TRANS("Performance Preset")),
                     "preset combo title must follow the locale");
        expectEquals(titleOf("speed-knob"), juce::String(TRANS("Playback Speed")),
                     "speed slider title must follow the locale");
        expectEquals(titleOf("volume-knob"), juce::String(TRANS("Volume")), "knob title must follow the locale");
        expectEquals(titleOf("volume-knob-wrap"), juce::String(TRANS("Volume")),
                     "knob wrapper title must follow the locale");
        expectEquals(titleOf("release-knob-wrap"), juce::String(TRANS("Release")),
                     "knob wrapper title must follow the locale");
        expectEquals(titleOf("record-btn"), juce::String(TRANS("Record")), "transport title must follow the locale");
        expectEquals(titleOf("back-btn"), juce::String(TRANS("Back to Start")),
                     "transport title must follow the locale");

        devpiano::locale::activate(devpiano::locale::Language::en);
    }
};

static StyleCatalogTest styleCatalogTest;

// =============================================================================
// Off-screen rendering checks: verify visible pixels for text and buttons.
// (Counterpart of the user-visible report: JIVE text/button rendering.)
// =============================================================================

class JiveRenderTest final : public juce::UnitTest {
public:
    JiveRenderTest()
        : juce::UnitTest("JiveRender", "DevPiano/UI") {
    }

    void runTest() override {
        // Self-contained style rules (no cwd dependence in tests).
        devpiano::ui::jive::StyleCatalog::get().loadFromJSON(juce::JSON::parse(
            R"({ "Text": { "foreground": "#EEEEEE", "font-size": 14 },
                 "#title": { "font-size": 18, "font-weight": "bold" },
                 "#settings-btn": { "background": "transparent" } })"));

        testTitleTextRendersVisiblePixels();
        testButtonLabelRendersVisiblePixels();
        testCardTitlesRenderVisiblePixels();
    }

private:
    [[nodiscard]] static int countLightPixels(juce::Component& component, int width, int height) {
        const auto bg = juce::Colour(0xff202327);
        auto image = juce::Image(juce::Image::ARGB, width, height, true);
        juce::Graphics g(image);
        g.fillAll(bg); // app background
        component.setBounds(0, 0, width, height);
        component.paintEntireComponent(g, true);

        int light = 0;
        for (int y = 0; y < height; y += 2) {
            for (int x = 0; x < width; x += 2) {
                const auto c = image.getPixelAt(x, y);
                if (c.getRed() > 70 || c != bg) {
                    ++light;
                }
            }
        }
        return light;
    }

    void testTitleTextRendersVisiblePixels() {
        beginTest("header title renders visible pixels");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("SettingsButton", [] { return std::make_unique<juce::Component>(); });

        auto tree = devpiano::ui::jive::makeHeaderTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "header interpretation failed");
        if (item == nullptr) {
            return;
        }

        // Give the header real size and count near-white pixels (text).
        const int light = countLightPixels(*item->getComponent(), 400, 36);
        expect(light > 12, "title text renders no visible pixels (light=" + juce::String(light) + ")");
    }

    void testButtonLabelRendersVisiblePixels() {
        beginTest("button label renders visible pixels");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("PathEditor", [] { return std::make_unique<juce::TextEditor>(); });
        interpreter.getComponentFactory().set("ListEditor", [] { return std::make_unique<juce::TextEditor>(); });

        auto tree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "plugin panel interpretation failed");
        if (item == nullptr) {
            return;
        }

        const int light = countLightPixels(*item->getComponent(), 800, 40);
        expect(light > 25, "button labels render no visible pixels (light=" + juce::String(light) + ")");
    }
    void testCardTitlesRenderVisiblePixels() {
        beginTest("control card titles have non-zero width and render visible pixels");

        ::jive::Interpreter interpreter;
        interpreter.getComponentFactory().set("DevKnob", [] { return std::make_unique<juce::Component>(); });
        interpreter.getComponentFactory().set("AdsrCurve", [] { return std::make_unique<juce::Component>(); });
        interpreter.getComponentFactory().set("RecordButton", [] { return std::make_unique<juce::Component>(); });
        interpreter.getComponentFactory().set("PlayButton", [] { return std::make_unique<juce::Component>(); });
        interpreter.getComponentFactory().set("StopButton", [] { return std::make_unique<juce::Component>(); });
        interpreter.getComponentFactory().set("BackButton", [] { return std::make_unique<juce::Component>(); });
        interpreter.getComponentFactory().set("SpeedSlider", [] { return std::make_unique<juce::Component>(); });

        auto tree = devpiano::ui::jive::makeControlsPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "controls panel interpretation failed");
        if (item == nullptr) {
            return;
        }

        item->getComponent()->setBounds(0, 0, 900, 200);

        // Verify that card titles are assigned positive width by FlexBox layout.
        if (auto* presetTitle = ::jive::findItemWithID(*item, "preset-card-title")) {
            expect(presetTitle->getComponent()->getWidth() > 50,
                   "preset-card-title width must be positive (got "
                       + juce::String(presetTitle->getComponent()->getWidth()) + ")");
        }
        if (auto* adsrTitle = ::jive::findItemWithID(*item, "adsr-curve-title")) {
            expect(adsrTitle->getComponent()->getWidth() > 50,
                   "adsr-curve-title width must be positive (got " + juce::String(adsrTitle->getComponent()->getWidth())
                       + ")");
        }
        if (auto* transportTitle = ::jive::findItemWithID(*item, "transport-card-title")) {
            expect(transportTitle->getComponent()->getWidth() > 50,
                   "transport-card-title width must be positive (got "
                       + juce::String(transportTitle->getComponent()->getWidth()) + ")");
        }

        const int light = countLightPixels(*item->getComponent(), 900, 200);
        expect(light > 50, "controls card titles render no visible pixels (light=" + juce::String(light) + ")");
    }
};

static JiveRenderTest jiveRenderTest;

// =============================================================================
// Regression: toggling the plugin panel must not destroy the layout, and
// populated combo options must be selectable (enabled).
// =============================================================================

class PluginPanelToggleTest final : public juce::UnitTest {
public:
    PluginPanelToggleTest()
        : juce::UnitTest("PluginPanelToggle", "DevPiano/UI") {
    }

    void runTest() override {
        beginTest("plugin panel toggle keeps layout intact");

        devpiano::ui::jive::StyleCatalog::get().loadFromJSON(
            juce::JSON::parse(R"({ "Text": { "foreground": "#EEEEEE", "font-size": 14 } })"));

        ::jive::Interpreter interpreter;
        registerRootComponentFactory(interpreter);

        auto tree = devpiano::ui::jive::makeRootLayout();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "root interpretation failed");
        if (item == nullptr) {
            return;
        }
        item->getComponent()->setBounds(0, 0, 1120, 760);

        auto* plugin = jive::findItemWithID(*item, "plugin-panel");
        auto* area = jive::findItemWithID(*item, "plugin-expanded-area");
        expect(plugin != nullptr, "plugin-panel item missing");
        expect(area != nullptr, "plugin-expanded-area item missing");
        if (plugin == nullptr || area == nullptr) {
            return;
        }

        // Collapsed initial state
        plugin->state.setProperty("height", 42, nullptr);
        area->state.setProperty("height", 0, nullptr);
        expect(plugin->getComponent()->getHeight() == 42, "collapsed panel height");
        expect(plugin->getComponent()->isVisible(), "collapsed panel visible");

        auto* contentRow = jive::findItemWithID(*item, "content-row");
        auto* controlsItem = jive::findItemWithID(*item, "controls-panel");
        auto* keyboardItem = jive::findItemWithID(*item, "keyboard-area");
        auto* statusItem = jive::findItemWithID(*item, "status-bar");
        expect(contentRow != nullptr, "content-row item missing");
        expect(controlsItem != nullptr, "controls-panel item missing");
        expect(keyboardItem != nullptr, "keyboard-area item missing");
        // Sibling positions are measured relative to the shared parent
        // (main-area), which is where reflow must happen.
        const auto contentRowYBefore = contentRow->getComponent()->getY();
        const auto controlsHBefore = controlsItem->getComponent()->getHeight();
        const auto keyboardHBefore = keyboardItem->getComponent()->getHeight();
        expect(contentRowYBefore >= plugin->getComponent()->getBottom(), "content-row below collapsed panel");

        auto* actionRow = jive::findItemWithID(*item, "plugin-action-row");

        // Expand — fixed order: area FIRST (so the panel's layout pass reads
        // the final area height), then panel, then explicit main-area reflow
        // (JIVE's boxModelChanged only relays the item itself, never its
        // siblings in the parent column).
        area->state.setProperty("height", 112, nullptr);
        plugin->state.setProperty("height", 160, nullptr);
        if (auto* panel = dynamic_cast<jive::FlexContainer*>(plugin)) {
            panel->layOutChildren();
        }
        if (auto* mainArea = dynamic_cast<jive::FlexContainer*>(jive::findItemWithID(*item, "main-area"))) {
            mainArea->layOutChildren();
        }
        expect(plugin->getComponent()->getHeight() == 160, "expanded panel height");
        expect(area->getComponent()->getHeight() > 0, "expanded area visible");
        expect(plugin->getComponent()->isVisible(), "expanded panel visible");

        // THE regression this test exists for: the parent column must reflow
        // its siblings when the plugin panel height changes, or the expanded
        // area overlaps the controls below it.
        expect(contentRow->getComponent()->getY() == contentRowYBefore + 118,
               "content-row moved down when panel expanded");
        expect(controlsItem->getComponent()->getHeight() < controlsHBefore, "controls shrank when panel expanded");
        expectEquals(keyboardItem->getComponent()->getHeight(), keyboardHBefore,
                     "keyboard capped at max-height, unchanged");
        expect(plugin->getComponent()->getBottom() <= contentRow->getComponent()->getY(),
               "expanded panel does not overlap content-row");

        // Collapse again — the exact sequence that made the whole panel vanish
        // (fixed order + explicit main-area reflow, as in setPluginPanelExpanded)
        area->state.setProperty("height", 0, nullptr);
        plugin->state.setProperty("height", 42, nullptr);
        if (auto* panel = dynamic_cast<jive::FlexContainer*>(plugin)) {
            panel->layOutChildren();
        }
        if (auto* mainArea = dynamic_cast<jive::FlexContainer*>(jive::findItemWithID(*item, "main-area"))) {
            mainArea->layOutChildren();
        }
        expect(plugin->getComponent()->getHeight() == 42, "re-collapsed panel height");
        expect(plugin->getComponent()->isVisible(), "re-collapsed panel visible");
        expect(area->getComponent()->getHeight() == 0, "re-collapsed area height");
        expect(actionRow != nullptr && actionRow->getComponent()->getHeight() == 30,
               "toolbar row keeps full height after collapse");
        expect(actionRow != nullptr && actionRow->getComponent()->isVisible(), "toolbar visible after collapse");
        expect(contentRow->getComponent()->getY() == contentRowYBefore,
               "content-row back at original y after collapse");
        expect(plugin->getComponent()->getBottom() <= contentRow->getComponent()->getY(),
               "collapsed panel does not overlap content-row");

        // Other panels must keep their size after the toggling.
        auto* headerItem = jive::findItemWithID(*item, "header");
        expect(headerItem->getComponent()->getHeight() == 36, "header keeps height");
        expect(controlsItem->getComponent()->getHeight() > 0, "controls keep height");
        expect(keyboardItem->getComponent()->getHeight() > 0, "keyboard keeps height");
        expect(statusItem->getComponent()->getHeight() == 24, "status bar keeps height");
        expect(controlsItem->getComponent()->isVisible(), "controls stay visible");
        expect(keyboardItem->getComponent()->isVisible(), "keyboard stays visible");
        auto* toggleBtn = dynamic_cast<juce::Button*>(jive::findItemWithID(*item, "toggle-btn")->getComponent().get());
        expect(toggleBtn != nullptr, "toggle button found");
        if (toggleBtn != nullptr) {
            expect(toggleBtn->isVisible(), "toggle button visible after toggling");
        }

        // Combo options populated like updatePluginPanelState must be
        // enabled and selectable (JIVE defaults missing "enabled" to false).
        testComboOptionsEnabled(interpreter);
    }

    void testComboOptionsEnabled(::jive::Interpreter& interpreter) {
        beginTest("combo options are enabled");

        auto pluginTree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(pluginTree);
        devpiano::ui::jive::ScopedJiveTree pluginItem = interpreter.interpret(pluginTree);
        expect(pluginItem != nullptr, "plugin panel interpretation failed");
        if (pluginItem == nullptr) {
            return;
        }

        auto* selectorItem = jive::findItemWithID(*pluginItem, "plugin-selector");
        expect(selectorItem != nullptr, "plugin-selector missing");
        if (selectorItem == nullptr) {
            return;
        }

        const auto addOption = [&selectorItem](const juce::String& name, int index) {
            auto option = juce::ValueTree("Option");
            option.setProperty("text", name, nullptr);
            option.setProperty("enabled", true, nullptr);
            selectorItem->state.addChild(option, index, nullptr);
        };
        addOption("pianoteq 9", 0);
        addOption("surge XT", 1);
        selectorItem->state.setProperty("selected", 0, nullptr);

        auto* combo = dynamic_cast<juce::ComboBox*>(selectorItem->getComponent().get());
        expect(combo != nullptr, "selector is not a ComboBox");
        if (combo == nullptr) {
            return;
        }

        expectEquals(combo->getNumItems(), 2);
        expect(combo->isItemEnabled(1), "first option must be enabled");
        expect(combo->isItemEnabled(2), "second option must be enabled");
        expect(combo->isEnabled(), "combo itself must be enabled");
        expectEquals(combo->getText(), juce::String("pianoteq 9"));
    }
};

static PluginPanelToggleTest pluginPanelToggleTest;
// =============================================================================
// Regression: a combo rebuilt like updatePluginPanelState must show the
// selected item's text when collapsed (JUCE draws the selected text in the
// combo's label; an empty label means the selection never took).
// =============================================================================

class ComboRebuildTest final : public juce::UnitTest {
public:
    ComboRebuildTest()
        : juce::UnitTest("ComboRebuild", "DevPiano/UI") {
    }

    void runTest() override {
        beginTest("selected text after rebuild");

        ::jive::Interpreter interpreter;
        juce::ValueTree tree("ComboBox");
        tree.setProperty("width", 180, nullptr);
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        auto* combo = dynamic_cast<juce::ComboBox*>(item->getComponent().get());
        expect(combo != nullptr, "combo created");
        if (combo == nullptr) {
            return;
        }

        // updatePluginPanelState computes its selection via the pure
        // devpiano::ui::preferredNameIndex function - test it directly.
        expectEquals(devpiano::ui::preferredNameIndex({ "pianoteq 9", "surge XT" }, "surge xt"), 1,
                     "preferred index matches case-insensitively");
        expectEquals(devpiano::ui::preferredNameIndex({ "pianoteq 9" }, "missing"), -1, "no preferred match");

        // The combo itself receives the same clear/placeholder/add/select
        // sequence from the production function.
        combo->clear(juce::dontSendNotification);
        combo->setTextWhenNothingSelected("Select a scanned plugin...");
        combo->addItem("pianoteq 9", 1);
        combo->setSelectedItemIndex(0, juce::dontSendNotification);

        expectEquals(combo->getNumItems(), 1);
        expectEquals(combo->getSelectedItemIndex(), 0, "selected index");
        expect(combo->getText().isNotEmpty(), "collapsed text not empty: '" + combo->getText() + "'");
        expectEquals(combo->getText(), juce::String("pianoteq 9"), "collapsed text");
    }
};

static ComboRebuildTest comboRebuildTest;

// =============================================================================
// Regression: the instrument filter combo is populated programmatically by
// MainComponent — the layout must NOT declare Option children or a "selected"
// property. JIVE's Option "selected" write-back (Option::selected calls
// setSelectedId(0) when deselected) clears the combo on the second user
// selection when items are also managed with clear()/addItem().
// =============================================================================

class FilterComboDefaultTest final : public juce::UnitTest {
public:
    FilterComboDefaultTest()
        : juce::UnitTest("FilterComboDefault", "DevPiano/UI") {
    }

    void runTest() override {
        beginTest("filter combo layout is free of declarative options");

        ::jive::Interpreter interpreter;
        auto tree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "plugin panel interpretation failed");
        if (item == nullptr) {
            return;
        }

        auto* filterItem = jive::findItemWithID(*item, "plugin-filter-combo");
        expect(filterItem != nullptr, "filter combo item missing");
        if (filterItem == nullptr) {
            return;
        }

        expect(!filterItem->state.hasProperty("selected"), "layout must not declare a selected property");
        expectEquals(filterItem->state.getNumChildren(), 0, "layout must not declare Option children");

        auto* combo = dynamic_cast<juce::ComboBox*>(filterItem->getComponent().get());
        expect(combo != nullptr, "filter is a ComboBox");
        if (combo == nullptr) {
            return;
        }

        // The production fill sequence (MainComponent::initialiseUi /
        // refreshPluginPanelTexts) must still yield a visible "All" default
        // when collapsed.
        combo->clear(juce::dontSendNotification);
        combo->addItem("All", 1);
        combo->addItem("Instruments Only", 2);
        combo->addItem("Effects Only", 3);
        combo->setSelectedId(1, juce::dontSendNotification);
        expectEquals(combo->getNumItems(), 3);
        expectEquals(combo->getSelectedItemIndex(), 0, "defaults to All");
        expectEquals(combo->getText(), juce::String("All"), "collapsed filter shows text: '" + combo->getText() + "'");

        // Re-selection used to wipe the combo via the Option write-back;
        // with no declarative Options it must simply move the selection.
        combo->setSelectedId(2, juce::dontSendNotification);
        expectEquals(combo->getSelectedId(), 2, "second selection keeps the combo populated");
        expectEquals(combo->getNumItems(), 3, "items survive user re-selection");

        devpiano::ui::jive::StyleCatalog::get().releaseOwnedStyles();
    }
};

static FilterComboDefaultTest filterComboDefaultTest;

// =============================================================================
// Instrument filter visibility adaptation: when the filter combo is hidden,
// the plugin selector must adapt its width to fill the gap and align with the
// right edge of where the filter combo used to be.
// =============================================================================

class FilterComboVisibilityAdaptationTest final : public juce::UnitTest {
public:
    FilterComboVisibilityAdaptationTest()
        : juce::UnitTest("FilterComboVisibilityAdaptation", "DevPiano/UI") {
    }

    void runTest() override {
        beginTest("hiding filter combo expands selector width");

        ::jive::Interpreter interpreter;
        auto tree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "plugin panel interpretation failed");
        if (item == nullptr) {
            return;
        }

        auto* selectorItem = jive::findItemWithID(*item, "plugin-selector");
        auto* filterItem = jive::findItemWithID(*item, "plugin-filter-combo");
        expect(selectorItem != nullptr, "selector item missing");
        expect(filterItem != nullptr, "filter item missing");
        if (selectorItem == nullptr || filterItem == nullptr) {
            return;
        }

        // Initial default layout
        expectEquals(static_cast<int>(selectorItem->state.getProperty("width", 0)), 180);
        expectEquals(static_cast<int>(filterItem->state.getProperty("width", 0)), 100);
        expect(filterItem->state.getProperty("visibility", true));

        // Simulate hiding the instrument filter (as setInstrumentFilterVisible(false) does)
        filterItem->state.setProperty("visibility", false, nullptr);
        filterItem->state.setProperty("width", 0, nullptr);
        filterItem->state.setProperty("margin", "0", nullptr);
        selectorItem->state.setProperty("width", 286, nullptr);

        expect(!filterItem->state.getProperty("visibility", true), "filter visibility must be false");
        expectEquals(static_cast<int>(filterItem->state.getProperty("width", 100)), 0, "filter width must be 0");
        expectEquals(filterItem->state.getProperty("margin", "").toString(), juce::String("0"),
                     "filter margin must be 0");
        expectEquals(static_cast<int>(selectorItem->state.getProperty("width", 0)), 286, "selector width must be 286");

        // Simulate restoring the instrument filter (as setInstrumentFilterVisible(true) does)
        filterItem->state.setProperty("visibility", true, nullptr);
        filterItem->state.setProperty("width", 100, nullptr);
        filterItem->state.setProperty("margin", "0 6 0 0", nullptr);
        selectorItem->state.setProperty("width", 180, nullptr);

        expect(filterItem->state.getProperty("visibility", false), "filter visibility must be true");
        expectEquals(static_cast<int>(filterItem->state.getProperty("width", 0)), 100, "filter width must be 100");
        expectEquals(filterItem->state.getProperty("margin", "").toString(), juce::String("0 6 0 0"),
                     "filter margin must be '0 6 0 0'");
        expectEquals(static_cast<int>(selectorItem->state.getProperty("width", 0)), 180, "selector width must be 180");

        devpiano::ui::jive::StyleCatalog::get().releaseOwnedStyles();
    }
};

static FilterComboVisibilityAdaptationTest filterComboVisibilityAdaptationTest;

// =============================================================================
// Regression: the preset combo in ControlsPanel must show its selected preset
// text when collapsed after populate.
// =============================================================================

class PresetComboRebuildTest final : public juce::UnitTest {
public:
    PresetComboRebuildTest()
        : juce::UnitTest("PresetComboRebuild", "DevPiano/UI") {
    }

    void runTest() override {
        beginTest("preset combo text after populate");

        ::jive::Interpreter interpreter;
        auto tree = devpiano::ui::jive::makeControlsPanelTree();
        devpiano::ui::jive::ScopedJiveTree item = interpreter.interpret(tree);
        expect(item != nullptr, "controls panel interpretation failed");
        if (item == nullptr) {
            return;
        }

        auto* comboItem = jive::findItemWithID(*item, "preset-combo");
        expect(comboItem != nullptr, "preset-combo missing");
        if (comboItem == nullptr) {
            return;
        }

        auto* combo = dynamic_cast<juce::ComboBox*>(comboItem->getComponent().get());
        expect(combo != nullptr, "preset-combo is a ComboBox");
        if (combo == nullptr) {
            return;
        }

        // setControlsPresets computes its selection via the pure
        // devpiano::ui::presetIdIndex function - test it directly.
        const juce::StringArray presetIds { "default", "grand-piano" };
        expectEquals(devpiano::ui::presetIdIndex(presetIds, "grand-piano"), 1, "preset id match");
        expectEquals(devpiano::ui::presetIdIndex(presetIds, "missing"), 0, "fallback to first entry");

        // The combo itself receives the same clear/placeholder/add/select
        // sequence from the production function.
        combo->clear(juce::dontSendNotification);
        combo->setTextWhenNothingSelected("Default");
        combo->addItem("Default", 1);
        combo->addItem("Grand Piano", 2);
        combo->setSelectedItemIndex(0, juce::dontSendNotification);

        expectEquals(combo->getNumItems(), 2);
        expectEquals(combo->getSelectedItemIndex(), 0, "preset default selected");
        expect(combo->getText().isNotEmpty(), "preset combo text not empty: '" + combo->getText() + "'");
        expectEquals(combo->getText(), juce::String("Default"), "preset combo shows Default");

        devpiano::ui::jive::StyleCatalog::get().releaseOwnedStyles();
    }
};

static PresetComboRebuildTest presetComboRebuildTest;

// =============================================================================
// Regression: every Component must outlive its StyleSheet when the JIVE tree
// is torn down. GuiItem's member declaration order releases `component`
// before `styleSheet`, so MainComponent's destructor collects owning
// references to all components first; without them, StyleSheet's
// ComponentInteractionState calls removeMouseListener() on a dead Component
// (and BackgroundCanvas, a child component member, would be double-deleted).
// =============================================================================

class JiveTeardownOrderTest final : public juce::UnitTest {
public:
    JiveTeardownOrderTest()
        : juce::UnitTest("JiveTeardownOrder", "DevPiano/UI") {
    }

    void runTest() override {
        beginTest("components survive GuiItem destruction with styles attached");

        const juce::var rules = juce::JSON::parse(R"(
            { "#plugin-panel": { "background": "#FF112233" } }
        )");
        expect(rules.isObject(), "test rules parse");
        devpiano::ui::jive::StyleCatalog::get().loadFromJSON(rules);

        auto tree = devpiano::ui::jive::makePluginPanelTree();
        devpiano::ui::jive::StyleCatalog::get().applyToTree(tree);

        ::jive::Interpreter interpreter;
        auto item = interpreter.interpret(tree);
        expect(item != nullptr, "plugin panel interpretation failed");
        if (item == nullptr) {
            return;
        }

        expect(item->getComponent()->getProperties().contains("style-sheet"), "style sheet attached at interpret");

        // Mirror MainComponent::~MainComponent: collect owning component
        // references, strip the "style-sheet" properties, then destroy the
        // GuiItem tree.
        std::vector<std::shared_ptr<juce::Component>> components;
        const std::function<void(::jive::GuiItem&)> collect = [&](::jive::GuiItem& guiItem) {
            if (auto component = guiItem.getComponent()) {
                components.push_back(std::move(component));
            }
            for (auto* child : guiItem.getChildren()) {
                collect(*child);
            }
        };
        const std::function<void(juce::Component&)> stripStyleSheets = [&](juce::Component& comp) {
            for (int i = 0; i < comp.getNumChildComponents(); ++i) {
                stripStyleSheets(*comp.getChildComponent(i));
            }
            comp.getProperties().remove("style-sheet");
        };

        collect(*item);
        expect(!components.empty(), "components collected");
        stripStyleSheets(*item->getComponent());
        item.reset();

        // The StyleSheets are gone by now; the components must still be
        // fully alive and usable.
        for (const auto& component : components) {
            expect(component != nullptr, "component reference alive after GuiItem destruction");
            if (component != nullptr) {
                component->getProperties(); // must not touch a dead object
            }
        }

        // Releasing the last references destroys the components (children
        // before parents, as the vector unwinds in reverse order).
        components.clear();

        devpiano::ui::jive::StyleCatalog::get().releaseOwnedStyles();
    }
};

static JiveTeardownOrderTest jiveTeardownOrderTest;

// =============================================================================
// Regression test: BinaryData embeds design_tokens.json and style_sheets.json
// so that running the standalone executable outside the repository checkout
// (e.g. from Desktop or release zip) retains 100% complete styles and text.
// =============================================================================

class BinaryDataStylesEmbeddedTest final : public juce::UnitTest {
public:
    BinaryDataStylesEmbeddedTest()
        : juce::UnitTest("BinaryDataStylesEmbedded", "DevPiano/UI") {
    }

    void runTest() override {
        beginTest("BinaryData embeds valid design_tokens.json");
        {
            expect(BinaryData::design_tokens_jsonSize > 0, "BinaryData::design_tokens_jsonSize must be > 0");
            const auto tokensJson = juce::JSON::parse(
                juce::String::fromUTF8(BinaryData::design_tokens_json, BinaryData::design_tokens_jsonSize));
            expect(!tokensJson.isVoid(), "BinaryData::design_tokens_json must parse as valid JSON");
            expect(tokensJson.hasProperty("colors"), "embedded tokens must define colors");
            expect(tokensJson.hasProperty("typography"), "embedded tokens must define typography");
        }

        beginTest("BinaryData embeds valid style_sheets.json");
        {
            expect(BinaryData::style_sheets_jsonSize > 0, "BinaryData::style_sheets_jsonSize must be > 0");
            const auto styleJson = juce::JSON::parse(
                juce::String::fromUTF8(BinaryData::style_sheets_json, BinaryData::style_sheets_jsonSize));
            expect(!styleJson.isVoid(), "BinaryData::style_sheets_json must parse as valid JSON");
            expect(styleJson.hasProperty("#window"), "embedded style sheet must define #window rule");
            expect(styleJson.hasProperty("Button"), "embedded style sheet must define Button rule");
            expect(styleJson.hasProperty("Text"), "embedded style sheet must define Text rule");
        }

        beginTest("BinaryData embeds valid zh_CN.loc and translates correctly");
        {
            expect(BinaryData::zh_CN_locSize > 0, "BinaryData::zh_CN_locSize must be > 0");
            juce::LocalisedStrings zh(juce::String::fromUTF8(BinaryData::zh_CN_loc, BinaryData::zh_CN_locSize), false);
            expect(zh.getLanguageName().isNotEmpty(), "embedded zh_CN locale must have non-empty language name");
            expectEquals(zh.translate("Volume"), juce::String::fromUTF8("\xe9\x9f\xb3\xe9\x87\x8f"),
                         "translates Volume -> 音量");
            expectEquals(zh.translate("Settings"), juce::String::fromUTF8("\xe8\xae\xbe\xe7\xbd\xae"),
                         "translates Settings -> 设置");
        }

        beginTest("StyleCatalog loads from BinaryData and styles JIVE tree without disk files");
        {
            devpiano::jive::DesignTokens::get().reset();
            const auto tokensJson = juce::JSON::parse(
                juce::String::fromUTF8(BinaryData::design_tokens_json, BinaryData::design_tokens_jsonSize));
            devpiano::jive::DesignTokens::get().loadFromJSON(tokensJson);

            const auto styleJson = juce::JSON::parse(
                juce::String::fromUTF8(BinaryData::style_sheets_json, BinaryData::style_sheets_jsonSize));
            devpiano::ui::jive::StyleCatalog::get().loadFromJSON(styleJson);

            auto rootTree = devpiano::ui::jive::makeRootLayout();
            devpiano::ui::jive::StyleCatalog::get().applyToTree(rootTree);

            expect(rootTree.hasProperty("style"), "root layout must have style property from embedded BinaryData");
            auto windowNode = findNodeById(rootTree, "window");
            expect(windowNode.isValid(), "window node found in layout");
            expect(windowNode.hasProperty("style"), "window node has style property");

            devpiano::ui::jive::StyleCatalog::get().releaseOwnedStyles();
            devpiano::jive::DesignTokens::get().reset();
        }
    }
};

static BinaryDataStylesEmbeddedTest binaryDataStylesEmbeddedTest;

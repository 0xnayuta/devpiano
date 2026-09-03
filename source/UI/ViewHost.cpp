#include "UI/ViewHost.h"

#include "UI/ColourSwatchButton.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveComponentRegistry.h"
#include "UI/jive/JiveUtils.h"
#include "UI/jive/LayoutModel.h"
#include "UI/jive/StyleCatalog.h"
#include "UI/jive/core/jive_ComponentFactory.h"
#include "UI/jive/core/jive_FlexContainer.h"
#include "UI/jive/core/jive_GuiItem.h"
#include "UI/jive/core/jive_Interpreter.h"
#include "UI/native/AdsrCurveComponent.h"

namespace devpiano::ui {

ViewHost::ViewHost() = default;

ViewHost::~ViewHost() {
    reset();
}

ViewHost::ViewHost(ViewHost&& other) noexcept
    : interpreter(std::move(other.interpreter))
    , rootItem(std::move(other.rootItem)) {
}

ViewHost& ViewHost::operator=(ViewHost&& other) noexcept {
    if (this != &other) {
        reset();
        interpreter = std::move(other.interpreter);
        rootItem = std::move(other.rootItem);
    }
    return *this;
}

void ViewHost::ensureInterpreter() {
    if (interpreter == nullptr) {
        interpreter = std::make_unique<::jive::Interpreter>();
    }
}

void ViewHost::registerDefaultComponents() {
    ensureInterpreter();
    auto& factory = interpreter->getComponentFactory();

    factory.set("PathEditor", [] {
        auto editor = std::make_unique<juce::TextEditor>();
        editor->setMultiLine(false);
        editor->setReturnKeyStartsNewLine(false);
        editor->setWantsKeyboardFocus(true);
        editor->setMouseClickGrabsKeyboardFocus(true);
        return editor;
    });

    factory.set("ListEditor", [] {
        auto editor = std::make_unique<juce::TextEditor>();
        editor->setMultiLine(true);
        editor->setReadOnly(true);
        editor->setScrollbarsShown(true);
        editor->setCaretVisible(false);
        editor->setPopupMenuEnabled(true);
        editor->setWantsKeyboardFocus(false);
        editor->setMouseClickGrabsKeyboardFocus(false);
        return editor;
    });

    factory.set("TextEditor", [] {
        auto editor = std::make_unique<juce::TextEditor>();
        editor->setMultiLine(false);
        editor->setWantsKeyboardFocus(true);
        editor->setMouseClickGrabsKeyboardFocus(true);
        return editor;
    });

    factory.set("DevKnob", [] {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 16);
        slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f,
                                    true);
        return slider;
    });

    factory.set("SpeedSlider", [] {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 16);
        return slider;
    });

    factory.set("ColourSwatch", [] { return std::make_unique<devpiano::ui::ColourSwatchButton>(); });
    factory.set("AdsrCurve", [] { return std::make_unique<AdsrCurveComponent>(); });
}

void ViewHost::registerKeyboardComponents(juce::MidiKeyboardState& keyboardState) {
    ensureInterpreter();
    devpiano::ui::jive::JiveComponentRegistry::registerCustomComponents(*interpreter, keyboardState);
}

void ViewHost::configureComponentFactory(const std::function<void(::jive::ComponentFactory&)>& configurator) {
    ensureInterpreter();
    if (configurator) {
        configurator(interpreter->getComponentFactory());
    }
}

bool ViewHost::loadLayout(juce::ValueTree layoutTree, bool applyStyles) {
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED;
    if (rootItem != nullptr) {
        devpiano::ui::jive::safeCleanupJiveTree(rootItem);
    }
    ensureInterpreter();

    if (applyStyles) {
        devpiano::ui::jive::StyleCatalog::get().applyToTree(layoutTree);
    }

    rootItem = interpreter->interpret(layoutTree);
    return rootItem != nullptr;
}

void ViewHost::reset() noexcept {
    JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED;
    if (rootItem != nullptr) {
        devpiano::ui::jive::safeCleanupJiveTree(rootItem);
    }
    interpreter.reset();
}

juce::Component* ViewHost::getRootComponent() const noexcept {
    if (rootItem != nullptr) {
        if (auto comp = rootItem->getComponent()) {
            return comp.get();
        }
    }
    return nullptr;
}

bool ViewHost::isValid() const noexcept {
    return getRootComponent() != nullptr;
}

void ViewHost::setBounds(juce::Rectangle<int> bounds) const {
    if (auto* comp = getRootComponent()) {
        comp->setBounds(bounds);
    }
}

void ViewHost::setBounds(int x, int y, int width, int height) const {
    if (auto* comp = getRootComponent()) {
        comp->setBounds(x, y, width, height);
    }
}

juce::Component* ViewHost::findComponentById(const juce::String& id) const {
    if (rootItem == nullptr) {
        return nullptr;
    }
    return devpiano::ui::jive::findComponentById(*rootItem, id);
}

::jive::GuiItem* ViewHost::findItem(const juce::String& id) const {
    if (rootItem == nullptr) {
        return nullptr;
    }
    return devpiano::ui::jive::findGuiItemById(*rootItem, id);
}

::jive::GuiItem* ViewHost::getRootItem() const noexcept {
    return rootItem.get();
}

bool ViewHost::setProperty(const juce::String& id, const juce::Identifier& name, const juce::var& value) const {
    if (auto* item = findItem(id)) {
        item->state.setProperty(name, value, nullptr);
        return true;
    }
    return false;
}

juce::var ViewHost::getProperty(const juce::String& id, const juce::Identifier& name,
                                const juce::var& defaultValue) const {
    if (auto* item = findItem(id)) {
        return item->state.getProperty(name, defaultValue);
    }
    return defaultValue;
}

bool ViewHost::setText(const juce::String& id, const juce::String& text) const {
    if (auto* item = findItem(id)) {
        item->state.setProperty("text", text, nullptr);
        item->state.setProperty("title", text, nullptr);
        return true;
    }
    return false;
}

bool ViewHost::setButtonLabel(const juce::String& id, const juce::String& text) const {
    if (auto* item = findItem(id)) {
        item->state.setProperty("title", text, nullptr);
        for (auto child : item->state) {
            if (child.getType() == juce::Identifier("Text")) {
                child.setProperty("text", text, nullptr);
                child.setProperty("title", text, nullptr);
                child.setProperty("word-wrap", "none", nullptr);
                return true;
            }
        }
        return true;
    }
    return false;
}

bool ViewHost::setEnabled(const juce::String& id, bool enabled) const {
    if (auto* item = findItem(id)) {
        item->state.setProperty("enabled", enabled, nullptr);
        if (auto comp = item->getComponent()) {
            comp->setEnabled(enabled);
        }
        return true;
    }
    return false;
}

bool ViewHost::setVisible(const juce::String& id, bool visible) const {
    if (auto* item = findItem(id)) {
        item->state.setProperty("visibility", visible, nullptr);
        if (auto comp = item->getComponent()) {
            comp->setVisible(visible);
        }
        return true;
    }
    return false;
}

double ViewHost::getSliderValue(const juce::String& id, double defaultValue) const {
    if (auto* slider = find<juce::Slider>(id)) {
        return slider->getValue();
    }
    return defaultValue;
}

bool ViewHost::setSliderValue(const juce::String& id, double value, juce::NotificationType notify) const {
    if (auto* slider = find<juce::Slider>(id)) {
        slider->setValue(value, notify);
        return true;
    }
    return false;
}

void ViewHost::relayoutContainer(const juce::String& containerId) const {
    if (auto* item = findItem(containerId)) {
        if (auto* flex = dynamic_cast<::jive::FlexContainer*>(item)) {
            flex->layOutChildren();
        }
    }
}

void ViewHost::refreshTitles() {
    if (rootItem != nullptr) {
        devpiano::ui::jive::refreshTitles(*rootItem);
    }
}

} // namespace devpiano::ui

#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <type_traits>

namespace jive {
class Interpreter;
class GuiItem;
class ComponentFactory;
} // namespace jive

namespace juce {
class MidiKeyboardState;
} // namespace juce

namespace devpiano::ui {

// ============================================================================
/// ViewHost — Devpiano Declarative UI Unified Host & Lifecycle Facade
///
/// Encapsulates the underlying declarative DOM interpreter, GuiItem tree,
/// custom component factory registrations, and safe teardown sequencing.
///
/// Business components (MainComponent, SettingsComponent, modal dialogs)
/// should only interact with ViewHost, eliminating raw pointer leaks,
/// manual safeCleanupJiveTree invocations, and unsafe dynamic_cast calls.
// ============================================================================
class ViewHost final {
public:
    ViewHost();
    ~ViewHost();

    ViewHost(const ViewHost&) = delete;
    ViewHost& operator=(const ViewHost&) = delete;
    ViewHost(ViewHost&&) noexcept;
    ViewHost& operator=(ViewHost&&) noexcept;

    /// Registers standard custom component factories (knobs, text editors, ADSR curve).
    void registerDefaultComponents();

    /// Registers custom components with an active keyboard state (CustomKeyboard, StatusBarMidiDot, icons).
    void registerKeyboardComponents(juce::MidiKeyboardState& keyboardState);

    /// Allows custom configuration of the component factory before layout load.
    void configureComponentFactory(const std::function<void(::jive::ComponentFactory&)>& configurator);

    /// Loads, styles, and interprets a declarative layout ValueTree.
    /// Safely cleans up any existing layout tree before interpreting the new one.
    /// Enforces UI thread execution via JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED.
    bool loadLayout(juce::ValueTree layoutTree, bool applyStyles = true);

    /// Safely performs ordered teardown of the current UI tree.
    void reset() noexcept;

    /// Returns the root JUCE Component if a layout is loaded, or nullptr.
    [[nodiscard]] juce::Component* getRootComponent() const noexcept;

    /// Returns true if a valid root component has been interpreted.
    [[nodiscard]] bool isValid() const noexcept;
    explicit operator bool() const noexcept {
        return isValid();
    }

    /// Resizes the root component to the specified bounds.
    void setBounds(juce::Rectangle<int> bounds) const;
    void setBounds(int x, int y, int width, int height) const;

    /// Finds a typed JUCE Component inside the interpreted tree by its string ID.
    template <typename ComponentType = juce::Component>
    [[nodiscard]] ComponentType* find(const juce::String& id) const {
        auto* comp = findComponentById(id);
        if (comp == nullptr) {
            return nullptr;
        }
        if constexpr (std::is_same_v<ComponentType, juce::Component>) {
            return comp;
        } else {
            return dynamic_cast<ComponentType*>(comp);
        }
    }

    /// Finds an untyped JUCE Component by ID.
    [[nodiscard]] juce::Component* findComponentById(const juce::String& id) const;

    /// Sets a property on the ValueTree node matching the given ID.
    bool setProperty(const juce::String& id, const juce::Identifier& name, const juce::var& value) const;

    /// Gets a property from the ValueTree node matching the given ID.
    [[nodiscard]] juce::var getProperty(const juce::String& id, const juce::Identifier& name,
                                        const juce::var& defaultValue = {}) const;

    /// Convenience helper to set text on a node (also syncs title property).
    bool setText(const juce::String& id, const juce::String& text) const;

    /// Convenience helper to set visible label and accessibility title on a button node.
    bool setButtonLabel(const juce::String& id, const juce::String& text) const;

    /// Convenience helper to set enabled state on a node.
    bool setEnabled(const juce::String& id, bool enabled) const;

    /// Convenience helper to set visibility on a node.
    bool setVisible(const juce::String& id, bool visible) const;

    /// Gets slider value if found, or returns defaultValue.
    [[nodiscard]] double getSliderValue(const juce::String& id, double defaultValue = 0.0) const;

    /// Sets slider value if found.
    bool setSliderValue(const juce::String& id, double value,
                        juce::NotificationType notify = juce::dontSendNotification) const;

    /// Requests re-layout on a Flex or Grid container by ID (e.g. "main-area", "plugin-action-row").
    void relayoutContainer(const juce::String& containerId) const;

    /// Re-evaluates all semantic TRANS() titles across the active layout tree.
    void refreshTitles();

    /// Advanced: returns the underlying GuiItem root (for internal/interop use).
    [[nodiscard]] ::jive::GuiItem* getRootItem() const noexcept;

    /// Advanced: finds a GuiItem node by ID (for internal/interop use).
    [[nodiscard]] ::jive::GuiItem* findItem(const juce::String& id) const;

private:
    void ensureInterpreter();

    std::unique_ptr<::jive::Interpreter> interpreter;
    std::unique_ptr<::jive::GuiItem> rootItem;
};

} // namespace devpiano::ui

namespace devpiano::ui::jive {
using ViewHost = devpiano::ui::ViewHost;
} // namespace devpiano::ui::jive

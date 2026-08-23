#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include <jive_layouts/jive_layouts.h>

namespace devpiano::ui::jive {

// ============================================================================
// JIVE Component Tree Safety & Teardown Helpers (ERR-009 / Lifetime Safety)
// ============================================================================

/// Recursively removes style-sheet properties from a component and its children
/// before GuiItem destruction, preventing dangling component interaction listeners.
inline void clearJiveStyleSheets(juce::Component* comp) {
    if (comp == nullptr) {
        return;
    }
    for (int i = 0; i < comp->getNumChildComponents(); ++i) {
        clearJiveStyleSheets(comp->getChildComponent(i));
    }
    if (comp->getProperties().contains("style-sheet")) {
        comp->getProperties().remove("style-sheet");
    }
}

/// Recursively collects shared pointers to all JUCE Components in a GuiItem hierarchy.
inline void collectJiveComponents(::jive::GuiItem& item, std::vector<std::shared_ptr<juce::Component>>& components) {
    if (auto component = item.getComponent()) {
        components.push_back(std::move(component));
    }
    for (auto* child : item.getChildren()) {
        collectJiveComponents(*child, components);
    }
}

/// Performs safe, ordered destruction of a JIVE GuiItem tree to ensure
/// components outlive style sheets (preventing UAF / double-free on teardown).
inline void safeCleanupJiveTree(std::unique_ptr<::jive::GuiItem>& rootItem) {
    if (rootItem != nullptr) {
        std::vector<std::shared_ptr<juce::Component>> jiveComponents;
        collectJiveComponents(*rootItem, jiveComponents);
        if (auto comp = rootItem->getComponent()) {
            clearJiveStyleSheets(comp.get());
        }
        rootItem.reset();
    }
}

/// RAII wrapper for a JIVE GuiItem tree that ensures safe, ordered teardown
/// on destruction (preventing Component / StyleSheet listener UAF).
class ScopedJiveTree final {
public:
    ScopedJiveTree() = default;
    /* implicit */ ScopedJiveTree(std::unique_ptr<::jive::GuiItem> item)
        : rootItem(std::move(item)) {
    }

    ~ScopedJiveTree() {
        reset();
    }

    ScopedJiveTree(const ScopedJiveTree&) = delete;
    ScopedJiveTree& operator=(const ScopedJiveTree&) = delete;

    ScopedJiveTree(ScopedJiveTree&& other) noexcept
        : rootItem(std::move(other.rootItem)) {
    }

    ScopedJiveTree& operator=(ScopedJiveTree&& other) noexcept {
        if (this != &other) {
            reset();
            rootItem = std::move(other.rootItem);
        }
        return *this;
    }

    ScopedJiveTree& operator=(std::unique_ptr<::jive::GuiItem> item) noexcept {
        reset(std::move(item));
        return *this;
    }

    void reset(std::unique_ptr<::jive::GuiItem> newItem = nullptr) {
        if (rootItem != nullptr) {
            safeCleanupJiveTree(rootItem);
        }
        rootItem = std::move(newItem);
    }

    [[nodiscard]] ::jive::GuiItem* get() const noexcept {
        return rootItem.get();
    }
    [[nodiscard]] ::jive::GuiItem& operator*() const noexcept {
        return *rootItem;
    }
    [[nodiscard]] ::jive::GuiItem* operator->() const noexcept {
        return rootItem.get();
    }
    [[nodiscard]] bool operator==(std::nullptr_t) const noexcept {
        return rootItem == nullptr;
    }
    [[nodiscard]] bool operator!=(std::nullptr_t) const noexcept {
        return rootItem != nullptr;
    }
    explicit operator bool() const noexcept {
        return rootItem != nullptr;
    }

    std::unique_ptr<::jive::GuiItem>& releaseAsUniquePtr() noexcept {
        return rootItem;
    }

private:
    std::unique_ptr<::jive::GuiItem> rootItem;
};

// ============================================================================
// JIVE Element & ValueTree ID Lookup Helpers
// ============================================================================

/// Finds a GuiItem with the specified ID property within the GuiItem hierarchy.
[[nodiscard]] inline ::jive::GuiItem* findGuiItemById(::jive::GuiItem& root, const juce::String& id) {
    if (root.state.getProperty("id").toString() == id) {
        return &root;
    }
    for (auto* child : root.getChildren()) {
        if (auto* found = findGuiItemById(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

/// Finds the underlying JUCE Component of a GuiItem with the specified ID property.
[[nodiscard]] inline juce::Component* findComponentById(::jive::GuiItem& root, const juce::String& id) {
    if (auto* item = findGuiItemById(root, id)) {
        return item->getComponent().get();
    }
    return nullptr;
}

/// Finds a juce::Button component of a GuiItem with the specified ID property.
[[nodiscard]] inline juce::Button* findButtonById(::jive::GuiItem& root, const juce::String& id) {
    return dynamic_cast<juce::Button*>(findComponentById(root, id));
}

/// Finds a juce::TextEditor component of a GuiItem with the specified ID property.
[[nodiscard]] inline juce::TextEditor* findTextEditorById(::jive::GuiItem& root, const juce::String& id) {
    return dynamic_cast<juce::TextEditor*>(findComponentById(root, id));
}

/// Finds a ValueTree node with the specified ID property in a ValueTree hierarchy.
[[nodiscard]] inline juce::ValueTree findNodeById(const juce::ValueTree& root, const juce::String& id) {
    if (root.getProperty("id").toString() == id) {
        return root;
    }
    for (const auto child : root) {
        if (auto found = findNodeById(child, id); found.isValid()) {
            return found;
        }
    }
    return {};
}

} // namespace devpiano::ui::jive

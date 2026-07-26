#include "KeyboardInputHandler.h"

#include "MainComponent.h"
#include "KeyboardMidiMapper.h"
#include "UI/CustomKeyboard.h"

#if JUCE_WINDOWS
struct HWND__;
using HWND = HWND__*;
struct HIMC__;
using HIMC = HIMC__*;
extern "C" HIMC __stdcall ImmAssociateContext(HWND, HIMC);
namespace {
void suppressImeForPeer(juce::ComponentPeer* peer) {
    if (peer == nullptr)
        return;
    if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
        ImmAssociateContext(hwnd, nullptr);
}
} // namespace
#endif

namespace devpiano::input {

KeyboardInputHandler::KeyboardInputHandler(MainComponent& owner)
    : owner_(owner) {
}

bool KeyboardInputHandler::isKeyboardInputSuppressed() const noexcept {
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent()) {
        if (focused == &owner_ || !owner_.isParentOf(focused))
            return false;
        if (dynamic_cast<const juce::TextEditor*>(focused) != nullptr)
            return true;
        if (auto* label = dynamic_cast<const juce::Label*>(focused))
            return label->isBeingEdited();
        return false;
    }
    return false;
}

bool KeyboardInputHandler::shouldTakeKeyboardFocus() const noexcept {
    if (auto* mcm = juce::ModalComponentManager::getInstanceWithoutCreating();
        mcm != nullptr && mcm->getNumModalComponents() > 0)
        return false;
    if (owner_.isSettingsWindowOpen())
        return false;
    if (owner_.pluginOperationController != nullptr && owner_.pluginOperationController->hasEditorWindowOpen())
        return false;
    return true;
}

bool KeyboardInputHandler::keyPressed(const juce::KeyPress& key) {
    if (isKeyboardInputSuppressed())
        return false;

    // F1-F12 preset shortcuts (no modifiers)
    if (!key.getModifiers().isAnyModifierKeyDown()) {
        for (int i = 0; i < 12; ++i) {
            if (key == juce::KeyPress(static_cast<int>(juce::KeyPress::F1Key) + i)) {
                owner_.handlePresetShortcut(i);
                return true;
            }
        }
    }

    const auto handled = owner_.keyboardMidiMapper.handleKeyPressed(key, owner_.audioEngine.getKeyboardState());
    if (handled) {
        owner_.getCustomKeyboard().notifyNoteActivity();
        suppressTextInputMethods();
    }
    return handled;
}

bool KeyboardInputHandler::keyStateChanged(bool isKeyDown) {
    juce::ignoreUnused(isKeyDown);
    if (isKeyboardInputSuppressed())
        return false;

    const auto handled = owner_.keyboardMidiMapper.handleKeyStateChanged(owner_.audioEngine.getKeyboardState());
    if (handled) {
        owner_.getCustomKeyboard().notifyNoteActivity();
        suppressTextInputMethods();
    }
    return handled;
}

void KeyboardInputHandler::focusGained(juce::Component::FocusChangeType cause) {
    owner_.juce::AudioAppComponent::focusGained(cause);
    if (!shouldTakeKeyboardFocus())
        return;
    if (juce::Component::getCurrentlyFocusedComponent() != &owner_)
        owner_.grabKeyboardFocus();
}

void KeyboardInputHandler::focusLost(juce::Component::FocusChangeType cause) {
    owner_.juce::AudioAppComponent::focusLost(cause);
}

void KeyboardInputHandler::visibilityChanged() {
    if (owner_.isShowing()) {
        suppressTextInputMethods();
        restoreKeyboardFocus();
    }
}

void KeyboardInputHandler::suppressTextInputMethods() {
    if (auto* peer = owner_.getPeer()) {
        peer->refreshTextInputTarget();
#if JUCE_WINDOWS
        suppressImeForPeer(peer);
#endif
    }
}

void KeyboardInputHandler::restoreKeyboardFocus() {
    if (!shouldTakeKeyboardFocus())
        return;
    if (owner_.isShowing() && juce::Component::getCurrentlyFocusedComponent() != &owner_)
        owner_.grabKeyboardFocus();
    suppressTextInputMethods();
}

} // namespace devpiano::input

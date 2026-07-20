#include "SettingsWindowManager.h"

#include "MainComponent.h"
#include "Settings/SettingsComponent.h"
#include "UI/CustomKeyboard.h"

namespace devpiano::settings {
namespace {
const auto backgroundColour = juce::Colour(0xff202225);

class SettingsDialogWindow final : public juce::DialogWindow {
public:
    SettingsDialogWindow(const juce::String& title, juce::Colour background, std::function<void()> onClose)
        : juce::DialogWindow(title, background, true)
        , closeCallback(std::move(onClose)) {
    }

    void closeButtonPressed() override {
        if (closeCallback)
            closeCallback();
    }

    bool escapeKeyPressed() override {
        closeButtonPressed();
        return true;
    }

private:
    std::function<void()> closeCallback;
};
} // namespace

struct SettingsWindowManager::State {
    std::unique_ptr<juce::DialogWindow> window;
    std::function<void()> onSaveRequested;
    std::function<void()> onClosed;
    bool closePending = false;
};

SettingsWindowManager::SettingsWindowManager()
    : state(std::make_shared<State>()) {
}

SettingsWindowManager::~SettingsWindowManager() {
    if (state->window != nullptr)
        state->window->setVisible(false);

    state->window.reset();
}

void SettingsWindowManager::show(ShowOptions options) {
    if (isOpen()) {
        state->window->toFront(true);
        return;
    }

    state->onSaveRequested = std::move(options.onSaveRequested);
    state->onClosed = std::move(options.onClosed);
    state->closePending = false;

    auto closeWindow = [this, weakState = std::weak_ptr<State>(state)] {
        if (auto lockedState = weakState.lock()) {
            if (lockedState->window == nullptr || lockedState->closePending)
                return;

            if (auto* content = dynamic_cast<SettingsComponent*>(
                    lockedState->window != nullptr ? lockedState->window->getContentComponent() : nullptr)) {
                if (content->isDirty() && lockedState->onSaveRequested)
                    lockedState->onSaveRequested();
            }

            closeAsync();
        }
    };

    auto content = std::make_unique<SettingsComponent>(options.deviceManager, options.savedAudioDeviceState,
                                                       options.displaySettingsModel);
    auto* contentPtr = content.get();

    contentPtr->onSaveRequested = [this, weakState = std::weak_ptr<State>(state)] {
        if (auto lockedState = weakState.lock()) {
            if (lockedState->window == nullptr || lockedState->closePending)
                return;

            if (lockedState->onSaveRequested)
                lockedState->onSaveRequested();

            closeAsync();
        }
    };

    contentPtr->onDisplaySettingsChanged = options.onDisplaySettingsChanged;
    contentPtr->onLanguageChanged = options.onLanguageChanged;

    contentPtr->onRefreshTexts = [weakState = std::weak_ptr<State>(state)] {
        if (auto locked = weakState.lock())
            if (locked->window)
                locked->window->setName(TRANS("Audio Settings"));
    };

    state->window = std::make_unique<SettingsDialogWindow>(TRANS("Audio Settings"), backgroundColour, closeWindow);
    state->window->setUsingNativeTitleBar(true);
    state->window->setContentOwned(content.release(), true);
    state->window->centreAroundComponent(&options.parent, 620, 760);
    state->window->setResizable(true, true);
    state->window->setVisible(true);
    state->window->toFront(true);
}

bool SettingsWindowManager::isDirty() const {
    if (auto* settingsContent = getSettingsContent())
        return settingsContent->isDirty();

    return false;
}

bool SettingsWindowManager::isOpen() const {
    return state->window != nullptr && state->window->isShowing();
}

void SettingsWindowManager::close() {
    if (state->window == nullptr || state->closePending)
        return;

    if (isDirty() && state->onSaveRequested)
        state->onSaveRequested();

    closeAsync();
}

void SettingsWindowManager::closeAsync() {
    if (state->window == nullptr || state->closePending)
        return;

    state->closePending = true;

    auto weakState = std::weak_ptr<State>(state);
    juce::MessageManager::callAsync([weakState] {
        if (auto lockedState = weakState.lock()) {
            if (lockedState->window == nullptr)
                return;

            if (lockedState->window != nullptr)
                lockedState->window->setVisible(false);

            lockedState->window.reset();
            lockedState->closePending = false;

            if (lockedState->onClosed)
                lockedState->onClosed();
        }
    });
}

void SettingsWindowManager::saveAndClose() {
    if (state->window == nullptr || state->closePending)
        return;

    if (state->onSaveRequested)
        state->onSaveRequested();

    closeAsync();
}

SettingsComponent* SettingsWindowManager::getSettingsContent() const {
    if (state->window == nullptr)
        return nullptr;

    return dynamic_cast<SettingsComponent*>(state->window->getContentComponent());
}

void SettingsWindowManager::showFor(MainComponent& owner) {
    auto onDisplaySettingsChanged
        = [safe = juce::Component::SafePointer<MainComponent>(&owner), lastResizable = true]() mutable {
              if (safe == nullptr)
                  return;

              auto kbs = safe->appSettings.getKeyboardDisplaySettingsView();
              devpiano::ui::KeyboardSettings ks;
              ks.colourMode = kbs.colourMode;
              ks.noteDisplay = kbs.noteDisplay;
              ks.fadeSpeed = kbs.fadeSpeed;
              ks.keySignature = safe->appSettings.keySignature;
              safe->resized();
              ks.customKeyLabels = kbs.customKeyLabels;
              ks.customKeyColours = kbs.customKeyColours;

              safe->keyboardPanel.getCustomKeyboard().setKeyboardSettings(ks);
              safe->pluginPanel.setInstrumentFilterVisible(kbs.showInstrumentFilter);

              // Only recreate desktop window when resize preference changes
              if (kbs.resizableWindow != lastResizable) {
                  lastResizable = kbs.resizableWindow;
                  if (auto* topLevel = safe->getTopLevelComponent()) {
                      if (auto* dw = dynamic_cast<juce::DocumentWindow*>(topLevel))
                          dw->setResizable(kbs.resizableWindow, kbs.resizableWindow);
                  }
              }
          };

    show({ .parent = owner,
           .deviceManager = owner.deviceManager,
           .savedAudioDeviceState = owner.appSettings.audioDeviceState.get(),
           .displaySettingsModel = &owner.appSettings,
           .onSaveRequested =
               [safe = juce::Component::SafePointer<MainComponent>(&owner)] {
                   if (safe != nullptr)
                       safe->saveSettingsNow();
               },
           .onClosed =
               [safe = juce::Component::SafePointer<MainComponent>(&owner)] {
                   if (safe != nullptr)
                       safe->restoreKeyboardFocus();
               },
           .onDisplaySettingsChanged = std::move(onDisplaySettingsChanged),
           .onLanguageChanged =
               [safe = juce::Component::SafePointer<MainComponent>(&owner)](const juce::String& code) {
                   if (safe != nullptr) {
                       safe->appSettings.languageCode = code;
                       safe->applyLanguage(code);
                       safe->saveSettingsSoon();
                   }
               } });
}
} // namespace devpiano::settings

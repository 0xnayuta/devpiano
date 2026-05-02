#include "SettingsWindowManager.h"

#include "Settings/SettingsComponent.h"

namespace devpiano::settings
{
namespace
{
const auto backgroundColour = juce::Colour(0xff202225);

class SettingsDialogWindow final : public juce::DialogWindow
{
public:
    SettingsDialogWindow(const juce::String& title,
                         juce::Colour background,
                         std::function<void()> onClose)
        : juce::DialogWindow(title, background, true), closeCallback(std::move(onClose))
    {
    }

    void closeButtonPressed() override
    {
        if (closeCallback)
            closeCallback();
    }

    bool escapeKeyPressed() override
    {
        closeButtonPressed();
        return true;
    }

private:
    std::function<void()> closeCallback;
};
} // namespace

struct SettingsWindowManager::State
{
    std::unique_ptr<juce::DialogWindow> window;
    std::function<void()> onSaveRequested;
    std::function<void()> onClosed;
    bool closePending = false;
};

SettingsWindowManager::SettingsWindowManager()
    : state(std::make_shared<State>())
{
}

SettingsWindowManager::~SettingsWindowManager()
{
    if (state->window != nullptr)
        state->window->setVisible(false);

    state->window.reset();
}

void SettingsWindowManager::show(ShowOptions options)
{
    if (isOpen())
    {
        state->window->toFront(true);
        return;
    }

    state->onSaveRequested = std::move(options.onSaveRequested);
    state->onClosed = std::move(options.onClosed);
    state->closePending = false;

    const auto requestCloseAsync = [](std::weak_ptr<State> weakState)
    {
        if (auto lockedState = weakState.lock())
        {
            if (lockedState->window == nullptr || lockedState->closePending)
                return;

            lockedState->closePending = true;
        }

        juce::MessageManager::callAsync([weakState]
        {
            if (auto lockedState = weakState.lock())
            {
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
    };

    auto content = std::make_unique<SettingsComponent>(options.deviceManager, options.savedAudioDeviceState);
    auto* contentPtr = content.get();

    contentPtr->onSaveRequested = [requestCloseAsync, weakState = std::weak_ptr<State>(state)]
    {
        if (auto lockedState = weakState.lock())
        {
            if (lockedState->window == nullptr || lockedState->closePending)
                return;

            if (lockedState->onSaveRequested)
                lockedState->onSaveRequested();

            requestCloseAsync(weakState);
        }
    };

    auto closeWindow = [requestCloseAsync, weakState = std::weak_ptr<State>(state)]
    {
        if (auto lockedState = weakState.lock())
        {
            if (lockedState->window == nullptr || lockedState->closePending)
                return;

            if (auto* content = dynamic_cast<SettingsComponent*>(lockedState->window != nullptr
                                                                      ? lockedState->window->getContentComponent()
                                                                      : nullptr))
            {
                if (content->isDirty() && lockedState->onSaveRequested)
                    lockedState->onSaveRequested();
            }

            requestCloseAsync(weakState);
        }
    };

    state->window = std::make_unique<SettingsDialogWindow>("Audio Settings", backgroundColour, closeWindow);
    state->window->setUsingNativeTitleBar(true);
    state->window->setContentOwned(content.release(), true);
    state->window->centreAroundComponent(&options.parent, 620, 560);
    state->window->setResizable(true, true);
    state->window->setVisible(true);
    state->window->toFront(true);
}

bool SettingsWindowManager::isDirty() const
{
    if (auto* settingsContent = getSettingsContent())
        return settingsContent->isDirty();

    return false;
}

bool SettingsWindowManager::isOpen() const
{
    return state->window != nullptr && state->window->isShowing();
}

void SettingsWindowManager::close()
{
    if (state->window == nullptr || state->closePending)
        return;

    if (isDirty() && state->onSaveRequested)
        state->onSaveRequested();

    closeAsync();
}

void SettingsWindowManager::closeAsync()
{
    if (state->window == nullptr || state->closePending)
        return;

    state->closePending = true;

    auto weakState = std::weak_ptr<State>(state);
    juce::MessageManager::callAsync([weakState]
    {
        if (auto lockedState = weakState.lock())
        {
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

void SettingsWindowManager::saveAndClose()
{
    if (state->window == nullptr || state->closePending)
        return;

    if (state->onSaveRequested)
        state->onSaveRequested();

    closeAsync();
}

SettingsComponent* SettingsWindowManager::getSettingsContent() const
{
    if (state->window == nullptr)
        return nullptr;

    return dynamic_cast<SettingsComponent*>(state->window->getContentComponent());
}

} // namespace devpiano::settings

/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MainComponent.h"

//==============================================================================
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS

#include <windows.h>

// WNDPROC hook state
static WNDPROC g_originalWndProc = nullptr;
static HWND g_hwnd = nullptr;
static MainComponent* g_mainComponent = nullptr;
static bool g_focusRestorePending = false;

static void scheduleKeyboardFocusRestore(const char* reason)
{
    if (g_mainComponent == nullptr)
        return;

    if (g_focusRestorePending)
        return;

    g_focusRestorePending = true;

    auto safeMainComponent = juce::Component::SafePointer<MainComponent>(g_mainComponent);
    const juce::String restoreReason(reason);

    juce::MessageManager::callAsync([safeMainComponent, restoreReason]
    {
        g_focusRestorePending = false;

        if (safeMainComponent == nullptr)
            return;

        juce::ignoreUnused(restoreReason);
        safeMainComponent->restoreKeyboardFocus();
    });
}

static LRESULT CALLBACK DevPianoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_SETFOCUS)
    {
        scheduleKeyboardFocusRestore("WM_SETFOCUS");
    }
    if (msg == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE)
    {
        scheduleKeyboardFocusRestore("WM_ACTIVATE");
    }
    if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
    {
        juce::ignoreUnused(hwnd, wParam);
    }
    return CallWindowProc(g_originalWndProc, hwnd, msg, wParam, lParam);
}

static void installWndProcHook(juce::ComponentPeer* peer)
{
    if (peer == nullptr || g_originalWndProc != nullptr)
        return;
    HWND hwnd = reinterpret_cast<HWND>(peer->getNativeHandle());
    if (hwnd == nullptr)
        return;
    g_hwnd = hwnd;
    g_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(&DevPianoWndProc)));
}

static void uninstallWndProcHook()
{
    if (g_hwnd != nullptr && g_originalWndProc != nullptr)
    {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_originalWndProc = nullptr;
        g_hwnd = nullptr;
    }
}

#endif // JUCE_WINDOWS

//==============================================================================
class DevPianoApplication  : public juce::JUCEApplication
{
public:
    //==============================================================================
    DevPianoApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override
    {
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
        g_mainComponent = nullptr;
        uninstallWndProcHook();
#endif
        mainWindow = nullptr;
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);
    }

    //==============================================================================
    class MainWindow    : public juce::DocumentWindow,
                          private juce::Timer
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            g_mainComponent = dynamic_cast<MainComponent*> (getContentComponent());
#endif

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            if (auto* mainComponent = dynamic_cast<MainComponent*> (getContentComponent()))
            {
                const auto limits = MainComponent::getMainContentResizeLimits();
                setResizeLimits (limits.getX(), limits.getY(), limits.getWidth(), limits.getHeight());
                mainComponent->persistMainContentSize (mainComponent->getWidth(), mainComponent->getHeight());
            }
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);

#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            startTimer (100);
#endif
        }

        ~MainWindow() override
        {
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            if (g_mainComponent == getContentComponent())
                g_mainComponent = nullptr;
#endif
        }

        void timerCallback() override
        {
            stopTimer();
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            installWndProcHook (getPeer());
#endif
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void resized() override
        {
            DocumentWindow::resized();

            if (! isVisible())
                return;

            if (auto* mainComponent = dynamic_cast<MainComponent*> (getContentComponent()))
                mainComponent->persistMainContentSize (mainComponent->getWidth(), mainComponent->getHeight());
        }

        void activeWindowStatusChanged() override
        {
            DocumentWindow::activeWindowStatusChanged();

            if (! isActiveWindow())
                return;

            if (auto* mainComponent = dynamic_cast<MainComponent*>(getContentComponent()))
            {
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
                scheduleKeyboardFocusRestore("activeWindowStatusChanged");
#else
                mainComponent->restoreKeyboardFocus();
#endif
            }
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (DevPianoApplication)

/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include "MainComponent.h"
#include <JuceHeader.h>

//==============================================================================
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS

#include <windows.h>

// WNDPROC hook state
static WNDPROC g_originalWndProc = nullptr;
static HWND g_hwnd = nullptr;
static MainComponent* g_mainComponent = nullptr;
static bool g_focusRestorePending = false;

static void scheduleKeyboardFocusRestore(const char* reason) {
    if (g_mainComponent == nullptr) {
        return;
    }

    if (g_focusRestorePending) {
        return;
    }

    g_focusRestorePending = true;

    auto safeMainComponent = juce::Component::SafePointer<MainComponent>(g_mainComponent);
    const juce::String restoreReason(reason);

    juce::MessageManager::callAsync([safeMainComponent, restoreReason] {
        g_focusRestorePending = false;

        if (safeMainComponent == nullptr) {
            return;
        }

        juce::ignoreUnused(restoreReason);
        safeMainComponent->restoreKeyboardFocus();
    });
}

static LRESULT CALLBACK DevPianoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // WM_SETFOCUS / WM_ACTIVATE → schedule keyboard focus restoration.
    //
    // JUCE's focusGained() / activeWindowStatusChanged() fire for most activation
    // scenarios, but on Windows they can arrive *before* the JUCE component tree
    // has finished processing the native focus event.  In those cases
    // grabKeyboardFocus() is silently dropped.  By posting the restore into
    // MessageManager::callAsync we give the component tree a chance to settle
    // before restoring keyboard focus to the MainComponent.
    if (msg == WM_SETFOCUS) {
        scheduleKeyboardFocusRestore("WM_SETFOCUS");
    }
    if (msg == WM_ACTIVATE && LOWORD(wParam) != WA_INACTIVE) {
        scheduleKeyboardFocusRestore("WM_ACTIVATE");
    }
    return CallWindowProc(g_originalWndProc, hwnd, msg, wParam, lParam);
}

static void installWndProcHook(juce::ComponentPeer* peer) {
    if (peer == nullptr || g_originalWndProc != nullptr) {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(peer->getNativeHandle());
    if (hwnd == nullptr) {
        return;
    }
    g_hwnd = hwnd;
    g_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&DevPianoWndProc)));
}

static void uninstallWndProcHook() {
    if (g_hwnd != nullptr && g_originalWndProc != nullptr) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_originalWndProc = nullptr;
        g_hwnd = nullptr;
    }
}

#endif // JUCE_WINDOWS

//==============================================================================
class DevPianoApplication : public juce::JUCEApplication {
public:
    //==============================================================================
    DevPianoApplication() = default;

    const juce::String getApplicationName() override {
        return ProjectInfo::projectName;
    }
    const juce::String getApplicationVersion() override {
        return ProjectInfo::versionString;
    }
    bool moreThanOneInstanceAllowed() override {
        return true;
    }

    //==============================================================================
    void initialise(const juce::String& commandLine) override {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        if (commandLine.containsIgnoreCase("--sine") || commandLine.containsIgnoreCase("--tone=sine")) {
            if (auto* mainComponent = dynamic_cast<MainComponent*>(mainWindow->getContentComponent())) {
                mainComponent->setBuiltinSynthTone(SettingsModel::BuiltinTone::sine);
            }
        } else if (commandLine.containsIgnoreCase("--piano") || commandLine.containsIgnoreCase("--tone=piano")) {
            if (auto* mainComponent = dynamic_cast<MainComponent*>(mainWindow->getContentComponent())) {
                mainComponent->setBuiltinSynthTone(SettingsModel::BuiltinTone::piano);
            }
        }
    }

    void shutdown() override {
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
        g_mainComponent = nullptr;
        uninstallWndProcHook();
#endif
        mainWindow = nullptr;
    }

    //==============================================================================
    void systemRequestedQuit() override {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override {
        juce::ignoreUnused(commandLine);
    }

    //==============================================================================
    class MainWindow : public juce::DocumentWindow, private juce::Timer {
    public:
        MainWindow(const juce::String& name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                                 juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons, false) {
            // 延迟桌面挂载（addToDesktop=false）：在内容、尺寸、位置与
            // Resizable 约束全部就绪后一次性创建并映射 X11 窗口，避免
            // 启动时左上角小窗 → 跳变 → 内容就绪的三阶段闪烁
            // （与 SettingsWindowManager 的修复同源）。
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            g_mainComponent = dynamic_cast<MainComponent*>(getContentComponent());
#endif

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
#else
            if (auto* mainComponent = dynamic_cast<MainComponent*>(getContentComponent())) {
                // 窗口始终可调：JUCE X11 的 XWindowSystem::setBounds 会在每次
                // 布局后以 USSize|USPosition 覆盖 updateConstraints 写入的
                // WMNormalHints（min==max），导致窗口大小锁定在框架层失效
                // （见 known-issues.md），因此不再提供"锁定窗口大小"选项。
                setResizable(true, true);
                const auto limits = MainComponent::getMainContentResizeLimits();
                setResizeLimits(limits.getX(), limits.getY(), limits.getWidth(), limits.getHeight());
                mainComponent->persistMainContentSize(mainComponent->getWidth(), mainComponent->getHeight());
            }

            // 预置屏幕居中位置，避免 KWin 对未定位窗口的放置/移动跳变。
            centreWithSize(getWidth(), getHeight());

            // 一次性桌面挂载：peer 在最终 bounds 下创建，map 即完整呈现。
            addToDesktop(getDesktopWindowStyleFlags());
#endif

            setVisible(true);

#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            // 仅 Windows 需要：先置顶避免被 Explorer 遮挡，100ms 后再恢复
            // 正常 z-order（Linux 上该双切换会引发 KWin 多次重绘闪烁）。
            setAlwaysOnTop(true);
            startTimer(100);
#endif
        }

        ~MainWindow() override {
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            if (g_mainComponent == getContentComponent()) {
                g_mainComponent = nullptr;
            }
#endif
        }

        void timerCallback() override {
            stopTimer();
            setAlwaysOnTop(false);
#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            installWndProcHook(getPeer());

            // The window is now visually on top (via HWND_TOPMOST) but lacks
            // keyboard focus — SetWindowPos with SWP_NOACTIVATE explicitly
            // avoids activation. Use AttachThreadInput to share input state
            // with the foreground thread, which grants SetForegroundWindow the
            // rights it needs to activate our window and deliver key events.
            if (auto* peer = getPeer()) {
                if (auto hwnd = reinterpret_cast<HWND>(peer->getNativeHandle())) {
                    auto fgHwnd = GetForegroundWindow();
                    if (fgHwnd != nullptr && fgHwnd != hwnd) {
                        const auto fgThreadId = GetWindowThreadProcessId(fgHwnd, nullptr);
                        const auto ourThreadId = GetCurrentThreadId();
                        AttachThreadInput(ourThreadId, fgThreadId, TRUE);
                        SetForegroundWindow(hwnd);
                        AttachThreadInput(ourThreadId, fgThreadId, FALSE);
                    }
                }
            }
#endif
        }

        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void resized() override {
            DocumentWindow::resized();

            if (!isVisible()) {
                return;
            }

            if (auto* mainComponent = dynamic_cast<MainComponent*>(getContentComponent())) {
                mainComponent->persistMainContentSize(mainComponent->getWidth(), mainComponent->getHeight());
            }
        }

        void activeWindowStatusChanged() override {
            DocumentWindow::activeWindowStatusChanged();

            if (auto* mainComponent = dynamic_cast<MainComponent*>(getContentComponent())) {
                if (!isActiveWindow()) {
                    mainComponent->handleWindowFocusLost();
                    return;
                }

#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
                scheduleKeyboardFocusRestore("activeWindowStatusChanged");
#else
                mainComponent->restoreKeyboardFocus();
#endif
            }
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION(DevPianoApplication)

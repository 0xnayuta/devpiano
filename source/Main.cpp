/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include "MainComponent.h"
#include "UI/WindowIconUtils.h"
#include <JuceHeader.h>

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
        return false;
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
        mainWindow = nullptr;
    }

    //==============================================================================
    void systemRequestedQuit() override {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override {
        if (mainWindow != nullptr) {
            mainWindow->toFront(true);
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
                const int minH
                    = mainComponent->isQwertyVisualizerExpanded() ? limits.getY() : juce::jmin(limits.getY(), 510);
                setResizeLimits(limits.getX(), minH, limits.getWidth(), limits.getHeight());
                mainComponent->persistMainContentSize(mainComponent->getWidth(), mainComponent->getHeight());
            }

            // 预置屏幕居中位置，避免 KWin 对未定位窗口的放置/移动跳变。
            centreWithSize(getWidth(), getHeight());

            // 一次性桌面挂载：peer 在最终 bounds 下创建，map 即完整呈现。
            addToDesktop(getDesktopWindowStyleFlags());
            devpiano::ui::applyAppWindowIcon(*this);
#endif

            setVisible(true);

#if defined(JUCE_WINDOWS) && JUCE_WINDOWS
            // 仅 Windows 需要：先置顶避免被 Explorer 遮挡，100ms 后再恢复
            // 正常 z-order（Linux 上该双切换会引发 KWin 多次重绘闪烁）。
            setAlwaysOnTop(true);
            startTimer(100);
#endif
        }

        void timerCallback() override {
            stopTimer();
            setAlwaysOnTop(false);
            toFront(true);
            juce::Process::makeForegroundProcess();

            if (auto* mainComponent = dynamic_cast<MainComponent*>(getContentComponent())) {
                scheduleKeyboardFocusRestore(*mainComponent);
            }
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

                scheduleKeyboardFocusRestore(*mainComponent);
            }
        }

    private:
        void scheduleKeyboardFocusRestore(MainComponent& mainComponent) {
            if (focusRestorePending) {
                return;
            }

            focusRestorePending = true;

            juce::MessageManager::callAsync([safeMain = juce::Component::SafePointer<MainComponent>(&mainComponent),
                                             safeWindow = juce::Component::SafePointer<MainWindow>(this)] {
                if (safeWindow != nullptr) {
                    safeWindow->focusRestorePending = false;
                }

                if (safeMain != nullptr) {
                    safeMain->restoreKeyboardFocus();
                }
            });
        }

        bool focusRestorePending = false;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION(DevPianoApplication)

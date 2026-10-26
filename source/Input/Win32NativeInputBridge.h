#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <optional>

#include "Core/KeyMapTypes.h"

#if JUCE_WINDOWS
// clang-format off
#include <windows.h>
#include <commctrl.h>
// clang-format on
#endif

namespace devpiano::input {

class Win32NativeInputBridge {
public:
    using KeyDownCallback = std::function<bool(int keyCode, const juce::ModifierKeys& mods)>;
    using KeyUpCallback = std::function<void(int keyCode)>;
    // Called periodically while the native Windows move/size modal loop is
    // active. Windows does not deliver WM_KEYDOWN/WM_KEYUP to the window
    // during that loop, so the app reconciles physical key state from here.
    using SizingPollCallback = std::function<void()>;

    [[nodiscard]] static int normalizeWin32KeyEvent(int wParam, int lParam) noexcept {
        const bool isExtended = (lParam & (1 << 24)) != 0;
        const int scanCode = (lParam >> 16) & 0xFF;

        if (wParam == 0x14 /*VK_CAPITAL*/ || scanCode == 0x3A) {
            return devpiano::core::kCapsLockKeyCode;
        }

        if (!isExtended) {
            switch (scanCode) {
            case 0x52:
                return juce::KeyPress::numberPad0;
            case 0x4F:
                return juce::KeyPress::numberPad1;
            case 0x50:
                return juce::KeyPress::numberPad2;
            case 0x51:
                return juce::KeyPress::numberPad3;
            case 0x4B:
                return juce::KeyPress::numberPad4;
            case 0x4C:
                return juce::KeyPress::numberPad5;
            case 0x4D:
                return juce::KeyPress::numberPad6;
            case 0x47:
                return juce::KeyPress::numberPad7;
            case 0x48:
                return juce::KeyPress::numberPad8;
            case 0x49:
                return juce::KeyPress::numberPad9;
            case 0x53:
                return juce::KeyPress::numberPadDecimalPoint;
            case 0x4A:
                return juce::KeyPress::numberPadSubtract;
            case 0x4E:
                return juce::KeyPress::numberPadAdd;
            case 0x37:
                return juce::KeyPress::numberPadMultiply;
            default:
                break;
            }
        } else {
            if (scanCode == 0x35) {
                return juce::KeyPress::numberPadDivide;
            }
        }

        switch (wParam) {
        case 0x60 /*VK_NUMPAD0*/:
            return juce::KeyPress::numberPad0;
        case 0x61 /*VK_NUMPAD1*/:
            return juce::KeyPress::numberPad1;
        case 0x62 /*VK_NUMPAD2*/:
            return juce::KeyPress::numberPad2;
        case 0x63 /*VK_NUMPAD3*/:
            return juce::KeyPress::numberPad3;
        case 0x64 /*VK_NUMPAD4*/:
            return juce::KeyPress::numberPad4;
        case 0x65 /*VK_NUMPAD5*/:
            return juce::KeyPress::numberPad5;
        case 0x66 /*VK_NUMPAD6*/:
            return juce::KeyPress::numberPad6;
        case 0x67 /*VK_NUMPAD7*/:
            return juce::KeyPress::numberPad7;
        case 0x68 /*VK_NUMPAD8*/:
            return juce::KeyPress::numberPad8;
        case 0x69 /*VK_NUMPAD9*/:
            return juce::KeyPress::numberPad9;
        case 0x6B /*VK_ADD*/:
            return juce::KeyPress::numberPadAdd;
        case 0x6D /*VK_SUBTRACT*/:
            return juce::KeyPress::numberPadSubtract;
        case 0x6A /*VK_MULTIPLY*/:
            return juce::KeyPress::numberPadMultiply;
        case 0x6F /*VK_DIVIDE*/:
            return juce::KeyPress::numberPadDivide;
        case 0x6E /*VK_DECIMAL*/:
            return juce::KeyPress::numberPadDecimalPoint;
        default:
            break;
        }

        return 0;
    }

    [[nodiscard]] static std::optional<bool> getPhysicalKeyState(int keyCode) noexcept {
#if JUCE_WINDOWS
        const auto index = physicalKeyIndex(keyCode);
        if (index < 0) {
            return std::nullopt;
        }
        const auto slot = static_cast<std::size_t>(index);
        // With NumLock on, numpad keys map one-to-one to VK_NUMPADx, so the
        // live OS state is unambiguous. Reading it directly also covers the
        // native move/size modal loop, which delivers no WM_KEYDOWN to refresh
        // the cache. With NumLock off, VK_NUMPADx overlaps the navigation keys
        // (VK_INSERT, ...), so keep the WM_KEYDOWN cache there to avoid
        // treating a navigation key as numpad input. The alternate VK still
        // keeps a latched key alive if NumLock changes between the OS key
        // event and this query; a real release clears both virtual keys.
        if (keyCode == devpiano::core::kCapsLockKeyCode || (GetKeyState(VK_NUMLOCK) & 1) != 0) {
            const bool primaryDown = (GetAsyncKeyState(virtualKeys[slot]) & 0x8000) != 0;
            const bool alternateDown
                = alternateVirtualKeys[slot] != 0 && (GetAsyncKeyState(alternateVirtualKeys[slot]) & 0x8000) != 0;
            physicalKeysDown[slot] = primaryDown || (physicalKeysDown[slot] && alternateDown);
            return physicalKeysDown[slot];
        }
        if (physicalKeysDown[slot] && (GetAsyncKeyState(virtualKeys[slot]) & 0x8000) == 0
            && (alternateVirtualKeys[slot] == 0 || (GetAsyncKeyState(alternateVirtualKeys[slot]) & 0x8000) == 0)) {
            physicalKeysDown[slot] = false;
        }
        return physicalKeysDown[slot];
#else
        juce::ignoreUnused(keyCode);
        return std::nullopt;
#endif
    }

#if JUCE_WINDOWS
    static void attach(juce::Component& component, KeyDownCallback onKeyDown, KeyUpCallback onKeyUp,
                       SizingPollCallback onSizingPoll = {}) {
        auto* peer = component.getPeer();
        if (peer == nullptr) {
            return;
        }
        auto hwnd = static_cast<HWND>(peer->getNativeHandle());
        if (hwnd == nullptr || !IsWindow(hwnd)) {
            return;
        }

        DWORD_PTR reference = 0;
        if (GetWindowSubclass(hwnd, subclassProc, kSubclassId, &reference)) {
            auto* state = reinterpret_cast<SubclassState*>(reference);
            state->owner = &component;
            state->onKeyDown = std::move(onKeyDown);
            state->onKeyUp = std::move(onKeyUp);
            state->onSizingPoll = std::move(onSizingPoll);
            return;
        }
        auto state = std::make_unique<SubclassState>();
        state->owner = &component;
        state->onKeyDown = std::move(onKeyDown);
        state->onKeyUp = std::move(onKeyUp);
        state->onSizingPoll = std::move(onSizingPoll);
        if (SetWindowSubclass(hwnd, subclassProc, kSubclassId, reinterpret_cast<DWORD_PTR>(state.get()))) {
            static_cast<void>(state.release());
        }
    }
#else
    static void attach(juce::Component& component, const KeyDownCallback& onKeyDown, const KeyUpCallback& onKeyUp,
                       const SizingPollCallback& onSizingPoll = {}) {
        juce::ignoreUnused(component, onKeyDown, onKeyUp, onSizingPoll);
    }
#endif

    static void detach(juce::Component& component) {
#if JUCE_WINDOWS
        auto* peer = component.getPeer();
        if (peer == nullptr) {
            return;
        }
        auto hwnd = static_cast<HWND>(peer->getNativeHandle());
        if (hwnd == nullptr || !IsWindow(hwnd)) {
            return;
        }

        DWORD_PTR reference = 0;
        if (GetWindowSubclass(hwnd, subclassProc, kSubclassId, &reference)) {
            auto* state = reinterpret_cast<SubclassState*>(reference);
            if (state->owner == &component && RemoveWindowSubclass(hwnd, subclassProc, kSubclassId)) {
                cancelSizingPoll(hwnd, *state);
                delete state;
            }
        }
#else
        juce::ignoreUnused(component);
#endif
    }

private:
#if JUCE_WINDOWS
    static constexpr UINT_PTR kSubclassId = 0xDE701A10;
    // Dedicated timer for the move/size modal loop. WM_TIMER keeps being
    // dispatched inside that loop, unlike WM_KEYDOWN/WM_KEYUP.
    static constexpr UINT_PTR kSizingPollTimerId = 0xDE701A11;
    static constexpr UINT kSizingPollIntervalMs = 16;
    inline static const std::array<int, 16> physicalKeys {
        juce::KeyPress::numberPad0,        juce::KeyPress::numberPad1,      juce::KeyPress::numberPad2,
        juce::KeyPress::numberPad3,        juce::KeyPress::numberPad4,      juce::KeyPress::numberPad5,
        juce::KeyPress::numberPad6,        juce::KeyPress::numberPad7,      juce::KeyPress::numberPad8,
        juce::KeyPress::numberPad9,        juce::KeyPress::numberPadDivide, juce::KeyPress::numberPadMultiply,
        juce::KeyPress::numberPadSubtract, juce::KeyPress::numberPadAdd,    juce::KeyPress::numberPadDecimalPoint,
        devpiano::core::kCapsLockKeyCode
    };
    inline static constexpr std::array<int, 16> virtualKeys { VK_NUMPAD0,  VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3,
                                                              VK_NUMPAD4,  VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD7,
                                                              VK_NUMPAD8,  VK_NUMPAD9, VK_DIVIDE,  VK_MULTIPLY,
                                                              VK_SUBTRACT, VK_ADD,     VK_DECIMAL, VK_CAPITAL };
    inline static constexpr std::array<int, 16> alternateVirtualKeys { VK_INSERT, VK_END,   VK_DOWN,   VK_NEXT,
                                                                       VK_LEFT,   VK_CLEAR, VK_RIGHT,  VK_HOME,
                                                                       VK_UP,     VK_PRIOR, 0,         0,
                                                                       0,         0,        VK_DELETE, 0 };
    inline static std::array<bool, 16> physicalKeysDown {};

    static int physicalKeyIndex(int code) noexcept {
        for (std::size_t i = 0; i < physicalKeys.size(); ++i) {
            if (physicalKeys[i] == code) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    struct SubclassState {
        juce::Component::SafePointer<juce::Component> owner;
        KeyDownCallback onKeyDown;
        KeyUpCallback onKeyUp;
        SizingPollCallback onSizingPoll;
        std::array<std::uint16_t, 16> pendingCharacters {};
        bool suppressAltComposition = false;
        bool sizingLoopActive = false;
    };

    static void startSizingPoll(HWND hwnd, SubclassState& state) noexcept {
        if (state.sizingLoopActive || !state.onSizingPoll) {
            return;
        }
        state.sizingLoopActive = true;
        SetTimer(hwnd, kSizingPollTimerId, kSizingPollIntervalMs, nullptr);
        state.onSizingPoll();
    }

    static void stopSizingPoll(HWND hwnd, SubclassState& state) noexcept {
        if (!state.sizingLoopActive) {
            return;
        }
        state.sizingLoopActive = false;
        KillTimer(hwnd, kSizingPollTimerId);
        // Final reconcile: a key released between the last tick and the end
        // of the modal loop must still produce its note-off.
        if (state.onSizingPoll) {
            state.onSizingPoll();
        }
    }

    // Teardown variant: never invokes the callback while the owner is being
    // destroyed or the HWND is going away.
    static void cancelSizingPoll(HWND hwnd, SubclassState& state) noexcept {
        state.sizingLoopActive = false;
        KillTimer(hwnd, kSizingPollTimerId);
    }

    static juce::ModifierKeys getCurrentWin32Modifiers() noexcept {
        int flags = 0;
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            flags |= juce::ModifierKeys::shiftModifier;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            flags |= juce::ModifierKeys::ctrlModifier;
        }
        if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
            flags |= juce::ModifierKeys::altModifier;
        }
        return juce::ModifierKeys(flags);
    }

    static LRESULT CALLBACK subclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                         DWORD_PTR dwRefData) {
        auto* state = reinterpret_cast<SubclassState*>(dwRefData);

        if (uMsg == WM_NCDESTROY) {
            if (state != nullptr) {
                cancelSizingPoll(hwnd, *state);
            }
            RemoveWindowSubclass(hwnd, subclassProc, uIdSubclass);
            delete state;
            return DefSubclassProc(hwnd, uMsg, wParam, lParam);
        }

        if (state != nullptr && state->owner != nullptr) {
            if (uMsg == WM_ENTERSIZEMOVE) {
                startSizingPoll(hwnd, *state);
            } else if (uMsg == WM_EXITSIZEMOVE) {
                stopSizingPoll(hwnd, *state);
            } else if (uMsg == WM_CAPTURECHANGED) {
                // Safety net: if the mouse capture is lost without a matching
                // WM_EXITSIZEMOVE, stop the poll timer instead of leaving it
                // running forever.
                if (GetCapture() != hwnd) {
                    stopSizingPoll(hwnd, *state);
                }
            } else if (uMsg == WM_TIMER && wParam == kSizingPollTimerId) {
                // The native modal loop dispatches WM_TIMER but swallows key
                // messages; poll physical key state so notes still start and
                // stop while the user drags a window edge.
                if (state->sizingLoopActive && state->onSizingPoll) {
                    state->onSizingPoll();
                }
                return 0;
            } else if (uMsg == WM_CHAR || uMsg == WM_SYSCHAR || uMsg == WM_DEADCHAR || uMsg == WM_SYSDEADCHAR) {
                const auto code = normalizeWin32KeyEvent(0, static_cast<int>(lParam));
                const auto index = physicalKeyIndex(code);
                if (index >= 0) {
                    auto& pending = state->pendingCharacters[static_cast<std::size_t>(index)];
                    if (pending > 0) {
                        --pending;
                        return 0;
                    }
                }
                if (state->suppressAltComposition && ((lParam >> 16) & 0xff) == 0x38) {
                    state->suppressAltComposition = false;
                    return 0;
                }
            } else if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
                const auto keyCode = normalizeWin32KeyEvent(static_cast<int>(wParam), static_cast<int>(lParam));
                const auto index = physicalKeyIndex(keyCode);
                if (index >= 0) {
                    const auto slot = static_cast<std::size_t>(index);
                    physicalKeysDown[slot] = true;
                    const auto mods = getCurrentWin32Modifiers();
                    const auto handled = state->onKeyDown && state->onKeyDown(keyCode, mods);
                    if (handled) {
                        if (devpiano::core::isNumpadKeyCode(keyCode) && state->pendingCharacters[slot] < 0xffff) {
                            ++state->pendingCharacters[slot];
                            state->suppressAltComposition = state->suppressAltComposition || mods.isAltDown();
                        }
                        return 0;
                    }
                    state->pendingCharacters[slot] = 0;
                    state->suppressAltComposition = false;
                }
            } else if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) {
                const auto keyCode = normalizeWin32KeyEvent(static_cast<int>(wParam), static_cast<int>(lParam));
                const auto index = physicalKeyIndex(keyCode);
                if (index >= 0) {
                    physicalKeysDown[static_cast<std::size_t>(index)] = false;
                    if (state->onKeyUp) {
                        state->onKeyUp(keyCode);
                    }
                }
            }
        }

        return DefSubclassProc(hwnd, uMsg, wParam, lParam);
    }
#endif
};

} // namespace devpiano::input

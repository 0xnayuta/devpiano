//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include <juce_events/juce_events.h>

namespace jive {

class Timer : private juce::Timer {
public:
    using Callback = std::function<void(juce::Time)>;

    Timer(Callback timerCallback, juce::RelativeTime callbackInterval);
    ~Timer() override = default;

private:
    void timerCallback() final;

    Callback callback;
    juce::RelativeTime interval;
    juce::Time timeLastCallbackInvoked;
};

[[nodiscard]] static inline juce::Time now() noexcept {
    return juce::Time::getCurrentTime();
}

} // namespace jive

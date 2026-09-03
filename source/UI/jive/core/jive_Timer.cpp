//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Timer.h"

namespace jive {

Timer::Timer(Timer::Callback timerCallback, juce::RelativeTime callbackInterval)
    : callback { timerCallback }
    , interval { callbackInterval } {
    timeLastCallbackInvoked = juce::Time::getCurrentTime();
    startTimer(static_cast<int>(interval.inMilliseconds()));
}

void Timer::timerCallback() {
    callback(juce::Time::getCurrentTime());
}

} // namespace jive

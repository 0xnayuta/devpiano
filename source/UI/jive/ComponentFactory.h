#pragma once

#include <jive_layouts/jive_layouts.h>

namespace juce {
class MidiKeyboardState;
}

class AdsrCurveComponent;

namespace devpiano::ui::jive {

/// Register native Components (CustomKeyboard, AdsrCurveComponent, StatusBarMidiDot)
/// with the JIVE interpreter's ComponentFactory.
/// Called once during MainComponent::initialiseUi().
void registerNativeComponents(::jive::Interpreter& interpreter, juce::MidiKeyboardState& keyboardState,
                              AdsrCurveComponent& adsrCurve);

} // namespace devpiano::ui::jive

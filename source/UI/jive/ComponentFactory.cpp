#include "ComponentFactory.h"

#include "UI/CustomKeyboard.h"
#include "UI/native/AdsrCurveComponent.h"
#include "UI/native/StatusBarMidiDot.h"
#include "jive_layouts/layout/jive_Interpreter.h"

namespace devpiano::ui::jive {

namespace {

/// Placeholder Component that owns an AdsrCurveComponent and fills its bounds.
class AdsrCurvePlaceholder final : public juce::Component {
public:
    explicit AdsrCurvePlaceholder(AdsrCurveComponent& c)
        : curve(c) {
        setOpaque(false);
        addAndMakeVisible(curve);
    }

    void resized() override {
        curve.setBounds(getLocalBounds());
    }

private:
    AdsrCurveComponent& curve;
};

} // namespace

void registerNativeComponents(::jive::Interpreter& interpreter, juce::MidiKeyboardState& keyboardState,
                              AdsrCurveComponent& adsrCurve) {
    auto& factory = interpreter.getComponentFactory();

    // CustomKeyboard: full native, zero changes.
    // Wrapped in a Viewport for horizontal scrolling.
    factory.set("CustomKeyboard", [&keyboardState] {
        auto viewport = std::make_unique<juce::Viewport>();
        auto ck = std::make_unique<CustomKeyboard>(keyboardState);
        viewport->setViewedComponent(ck.release(), true);
        viewport->setScrollBarsShown(false, true, false, true);
        return viewport;
    });

    // AdsrCurve: pure-draw Component, placed in a placeholder.
    factory.set("AdsrCurve", [&adsrCurve] { return std::make_unique<AdsrCurvePlaceholder>(adsrCurve); });

    // MIDI activity dot: tiny native Component (no JIVE built-in circle).
    factory.set("StatusBarMidiDot", [] { return std::make_unique<StatusBarMidiDot>(); });
}

} // namespace devpiano::ui::jive

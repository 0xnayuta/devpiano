#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"
#include "Input/KeyboardMidiMapper.h"
#include "Input/TouchVelocityCurve.h"
#include "Input/TypingCadenceEstimator.h"

namespace {

class CadenceVelocityTest final : public juce::UnitTest {
public:
    CadenceVelocityTest()
        : juce::UnitTest("CadenceVelocity: Typing Dynamics & Humanizer", "DevPiano/Input") {
    }

    void runTest() override {
        testCadenceDynamicsTimingCurve();
        testVelocityHumanizerDeterministicJitter();
        testKeyboardMidiMapperPipelineIntegration();
        testAuthoredVelocityAndSilentBinding();
        testShiftModifierArbitrationPriority();
    }

private:
    void testCadenceDynamicsTimingCurve() {
        beginTest("TypingCadenceEstimator: Rapid vs slow typing dynamics and phrase timeout");

        devpiano::input::TypingCadenceEstimator estimator;
        estimator.setEnabled(true);
        estimator.reset();

        // 1. Initial stroke starts at baseline (~100 / 127)
        const float firstVel = estimator.estimateVelocity(10.0);
        expectWithinAbsoluteError(firstVel, devpiano::input::TypingCadenceEstimator::kDefaultBaseVelocity, 0.001f);

        // 2. Rapid keystroke (delta = 40ms <= 60ms fast threshold) -> peak velocity (~122 / 127)
        const float fastVel = estimator.estimateVelocity(10.040);
        expectWithinAbsoluteError(fastVel, devpiano::input::TypingCadenceEstimator::kMaxDynamicVelocity, 0.001f);
        expect(fastVel > firstVel);

        // 3. Another rapid keystroke (delta = 50ms) -> still peak velocity
        const float fastVel2 = estimator.estimateVelocity(10.090);
        expectWithinAbsoluteError(fastVel2, devpiano::input::TypingCadenceEstimator::kMaxDynamicVelocity, 0.001f);

        // 4. Slow, lyrical keystroke (delta = 600ms >= 500ms slow threshold) -> gentle low velocity (~76 / 127)
        const float slowVel = estimator.estimateVelocity(10.690);
        expectWithinAbsoluteError(slowVel, devpiano::input::TypingCadenceEstimator::kMinDynamicVelocity, 0.001f);
        expect(slowVel < firstVel);

        // 5. Moderate tempo keystroke (delta = 200ms) -> intermediate velocity
        const float midVel = estimator.estimateVelocity(10.890);
        expect(midVel > slowVel);
        expect(midVel < fastVel);

        // 6. Idle pause (> 1.0s timeout) -> phrase boundary resets to standard baseline
        const float timeoutVel = estimator.estimateVelocity(12.500); // 1.61s gap
        expectWithinAbsoluteError(timeoutVel, devpiano::input::TypingCadenceEstimator::kDefaultBaseVelocity, 0.001f);

        // 7. When disabled, always outputs baseline
        estimator.setEnabled(false);
        const float disabledFast = estimator.estimateVelocity(12.540); // 40ms gap
        expectWithinAbsoluteError(disabledFast, devpiano::input::TypingCadenceEstimator::kDefaultBaseVelocity, 0.001f);
    }

    void testVelocityHumanizerDeterministicJitter() {
        beginTest("VelocityHumanizer: Determinism, bounds, and boundary clamping");

        devpiano::input::VelocityHumanizer humanizer;
        humanizer.setEnabled(true);
        humanizer.setAmount(0.04f); // +/- ~5 velocity steps
        expect(humanizer.isEnabled());

        // 1. Identical inputs yield 100% deterministic outputs across repeated runs
        const float out1 = humanizer.applyHumanize(0.70f, 60, 100);
        const float out2 = humanizer.applyHumanize(0.70f, 60, 100);
        expectEquals(out1, out2);

        // 2. Different note or counter yields different jitter
        const float outDifferentNote = humanizer.applyHumanize(0.70f, 61, 100);
        const float outDifferentCounter = humanizer.applyHumanize(0.70f, 60, 101);
        expect(std::abs(outDifferentNote - out1) > 0.0001f || std::abs(outDifferentCounter - out1) > 0.0001f);

        // 3. Jitter stays strictly bounded within +/- amount
        for (int note = 21; note <= 108; ++note) {
            for (std::uint32_t c = 1; c <= 20; ++c) {
                const float in = 0.50f;
                const float out = humanizer.applyHumanize(in, note, c);
                expect(std::abs(out - in) <= 0.0401f);
                expect(out >= 1.0f / 127.0f && out <= 1.0f);
            }
        }

        // 4. Clamping bounds: high velocity never exceeds 1.0f, low never drops below 1/127
        constexpr float kMinMidi = 1.0f / 127.0f;
        for (std::uint32_t c = 0; c < 50; ++c) {
            expect(humanizer.applyHumanize(0.99f, 72, c) <= 1.0f);
            expect(humanizer.applyHumanize(kMinMidi, 48, c) >= kMinMidi);
        }

        // 5. Bypass when disabled
        humanizer.setEnabled(false);
        expectEquals(humanizer.applyHumanize(0.65f, 60, 12), 0.65f);
    }

    void testKeyboardMidiMapperPipelineIntegration() {
        beginTest("KeyboardMidiMapper: End-to-end cadence and humanizer pipeline");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        mapper.setCadenceDynamicsEnabled(true);
        mapper.setVelocityHumanizerEnabled(true);
        expect(mapper.isCadenceDynamicsEnabled());
        expect(mapper.isVelocityHumanizerEnabled());

        const auto keyCodeA = devpiano::core::makeAlphaNumericKeyCode('A');

        // Press 'A' key (note C3 / 60)
        juce::KeyPress keyA('a');
        mapper.handleKeyPressed(keyA, state);

        const auto* held = mapper.findHeldKey(keyCodeA);
        expect(held != nullptr);
        if (held != nullptr) {
            expect(held->velocity > 0.1f && held->velocity <= 1.0f);
        }

        // Release 'A' key
        mapper.releaseAllHeldKeys(state);
        expect(mapper.findHeldKey(keyCodeA) == nullptr);
    }

    void testAuthoredVelocityAndSilentBinding() {
        beginTest("Cadence opt-out preserves authored velocity and zero remains silent");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;
        devpiano::core::KeyboardLayout layout;
        layout.bindings.push_back(devpiano::core::makeNoteBinding('A', 60, 1, 1.0f));
        mapper.setLayout(layout);
        mapper.setCadenceDynamicsEnabled(false);
        mapper.setVelocityHumanizerEnabled(false);
        mapper.setTouchVelocityCurve(devpiano::input::TouchVelocityCurve::standard);

        const auto keyCodeA = devpiano::core::makeAlphaNumericKeyCode('A');
        mapper.handleKeyPressed(juce::KeyPress('a'), state);
        const auto* held = mapper.findHeldKey(keyCodeA);
        expect(held != nullptr);
        if (held != nullptr) {
            expectEquals(held->velocity, 1.0f);
        }
        mapper.releaseAllHeldKeys(state);

        layout.bindings[0] = devpiano::core::makeNoteBinding('A', 60, 1, 0.0f);
        mapper.setLayout(layout);
        mapper.setCadenceDynamicsEnabled(true);
        mapper.setVelocityHumanizerEnabled(true);
        mapper.setVelocityHumanizeAmount(0.15f);

        mapper.handleKeyPressed(juce::KeyPress('a'), state);
        held = mapper.findHeldKey(keyCodeA);
        expect(held != nullptr);
        if (held != nullptr) {
            expectEquals(held->velocity, 0.0f);
        }
        mapper.releaseAllHeldKeys(state);
    }

    void testShiftModifierArbitrationPriority() {
        beginTest("PerformanceModifier: Shift boost retains highest arbitration priority");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        mapper.setCadenceDynamicsEnabled(true);
        mapper.setVelocityHumanizerEnabled(true);

        const auto keyCodeA = devpiano::core::makeAlphaNumericKeyCode('A');

        // Press 'A' key with Shift held
        juce::KeyPress keyShiftA('a', juce::ModifierKeys::shiftModifier, 0);
        mapper.handleKeyPressed(keyShiftA, state);

        const auto* held = mapper.findHeldKey(keyCodeA);
        expect(held != nullptr);
        if (held != nullptr) {
            // Shift boost must strictly force velocity to exactly 1.0f regardless of cadence or jitter
            expectEquals(held->velocity, 1.0f);
        }

        mapper.releaseAllHeldKeys(state);
    }
};

CadenceVelocityTest cadenceVelocityTest;

} // namespace

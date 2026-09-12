#include <JuceHeader.h>

#include "Input/KeyboardMidiMapper.h"
#include "Input/TouchVelocityCurve.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"

// ============================================================================
/// TouchVelocityCurveTest (Phase 29-C)
///
/// 验证触键力度曲线（Standard, Light, Heavy, Wide Dynamic）的数学特性、
/// 单调性、端点守恒、键盘映射注入与全链路持久化/预设流。
// ============================================================================
class TouchVelocityCurveTest final : public juce::UnitTest {
public:
    TouchVelocityCurveTest()
        : juce::UnitTest("TouchVelocityCurve", "DevPiano/Input") {
    }

    void runTest() override {
        testMathematicalCurveProperties();
        testKeyboardMidiMapperCurveInjection();
        testSettingsStoreAndPresetRoundTrip();
    }

private:
    void testMathematicalCurveProperties() {
        beginTest("TouchVelocityCurve: Mathematical invariants, monotonicity, and clamping");

        using namespace devpiano::input;

        const std::array<TouchVelocityCurve, 4> allCurves { TouchVelocityCurve::standard, TouchVelocityCurve::light,
                                                            TouchVelocityCurve::heavy,
                                                            TouchVelocityCurve::wideDynamic };

        for (const auto curve : allCurves) {
            // 1. 端点严格守恒
            expectEquals(applyVelocityCurve(0.0f, curve), 0.0f);
            expectEquals(applyVelocityCurve(1.0f, curve), 1.0f);

            // 2. 越界输入严格钳制保护
            expectEquals(applyVelocityCurve(-0.5f, curve), 0.0f);
            expectEquals(applyVelocityCurve(1.5f, curve), 1.0f);

            // 3. 严格单调不减 [0.0, 1.0]
            float prevOutput = 0.0f;
            for (int i = 0; i <= 100; ++i) {
                const auto v = static_cast<float>(i) / 100.0f;
                const auto out = applyVelocityCurve(v, curve);
                expect(out >= 0.0f && out <= 1.0f);
                expect(out >= prevOutput);
                prevOutput = out;
            }

            // 4. Minimum positive velocity invariant: ensure non-zero note-on is never rounded to 0
            constexpr float kMinMidiVelocity = 1.0f / 127.0f;
            const auto minOut = applyVelocityCurve(kMinMidiVelocity, curve);
            expect(minOut >= kMinMidiVelocity);
            expect(juce::roundToInt(minOut * 127.0f) >= 1);
        }

        // 4. 手感曲线特性校验（中间点与强弱区特异性）
        const auto midStd = applyVelocityCurve(0.5f, TouchVelocityCurve::standard);
        const auto midLight = applyVelocityCurve(0.5f, TouchVelocityCurve::light);
        const auto midHeavy = applyVelocityCurve(0.5f, TouchVelocityCurve::heavy);
        const auto midWide = applyVelocityCurve(0.5f, TouchVelocityCurve::wideDynamic);

        expectEquals(midStd, 0.5f);
        expectEquals(midWide, 0.5f);
        // Light 曲线向上凸起补偿，中力度发音更饱满
        expect(midLight > midStd);
        // Heavy 曲线向下凹陷压制，需要更重触键才能触发响亮发音
        expect(midHeavy < midStd);

        // Wide Dynamic: S 型曲线在弱音区 (0.2) 衰减更深，在强音区 (0.8) 提升更强
        const auto softWide = applyVelocityCurve(0.2f, TouchVelocityCurve::wideDynamic);
        const auto loudWide = applyVelocityCurve(0.8f, TouchVelocityCurve::wideDynamic);
        expect(softWide < 0.2f); // 极弱更柔
        expect(loudWide > 0.8f); // 强奏更具冲击力

        // 6. String helper verification
        expectEquals(juce::String(touchVelocityCurveToString(TouchVelocityCurve::standard)), juce::String("Standard"));
        expectEquals(juce::String(touchVelocityCurveToString(TouchVelocityCurve::light)), juce::String("Light"));
        expectEquals(juce::String(touchVelocityCurveToString(TouchVelocityCurve::heavy)), juce::String("Heavy"));
        expectEquals(juce::String(touchVelocityCurveToString(TouchVelocityCurve::wideDynamic)),
                     juce::String("Wide Dynamic"));
    }

    void testKeyboardMidiMapperCurveInjection() {
        beginTest("KeyboardMidiMapper: Velocity curve injection during note triggering");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        expect(mapper.getTouchVelocityCurve() == devpiano::input::TouchVelocityCurve::standard);

        // 绑定一个基准力度为 0.8f 的按键
        devpiano::core::KeyboardLayout layout;
        layout.name = "CurveTestLayout";
        layout.bindings.push_back(devpiano::core::makeNoteBinding('A', 60, 1, 0.80f));
        mapper.setLayout(layout);

        // 监听 state 触发的 noteOn velocity
        struct TestListener final : public juce::MidiKeyboardState::Listener {
            float lastVelocity = 0.0f;
            void handleNoteOn(juce::MidiKeyboardState*, int, int, float vel) override {
                lastVelocity = vel;
            }
            void handleNoteOff(juce::MidiKeyboardState*, int, int, float) override {
            }
        } listener;

        state.addListener(&listener);

        const juce::KeyPress keyA('A', 0, 0);

        // 1. Standard: 输出严格等于 0.80f
        mapper.setTouchVelocityCurve(devpiano::input::TouchVelocityCurve::standard);
        mapper.handleKeyPressed(keyA, state);
        expectWithinAbsoluteError(listener.lastVelocity, 0.80f, 0.001f);
        mapper.releaseAllHeldKeys(state);

        // 2. Light: 0.80^0.65 ≈ 0.865f > 0.80f
        mapper.setTouchVelocityCurve(devpiano::input::TouchVelocityCurve::light);
        mapper.handleKeyPressed(keyA, state);
        expect(listener.lastVelocity > 0.80f);
        expectWithinAbsoluteError(listener.lastVelocity, std::pow(0.80f, 0.65f), 0.001f);
        mapper.releaseAllHeldKeys(state);

        // 3. Heavy: 0.80^1.60 ≈ 0.699f < 0.80f
        mapper.setTouchVelocityCurve(devpiano::input::TouchVelocityCurve::heavy);
        mapper.handleKeyPressed(keyA, state);
        expect(listener.lastVelocity < 0.80f);
        expectWithinAbsoluteError(listener.lastVelocity, std::pow(0.80f, 1.60f), 0.001f);
        mapper.releaseAllHeldKeys(state);

        // 4. Wide Dynamic: S 型在 0.80 处提升: 0.8^2 * (3 - 1.6) = 0.896f > 0.80f
        mapper.setTouchVelocityCurve(devpiano::input::TouchVelocityCurve::wideDynamic);
        mapper.handleKeyPressed(keyA, state);
        expect(listener.lastVelocity > 0.80f);
        expectWithinAbsoluteError(listener.lastVelocity, 0.80f * 0.80f * (3.0f - 2.0f * 0.80f), 0.001f);
        mapper.releaseAllHeldKeys(state);

        state.removeListener(&listener);
    }

    void testSettingsStoreAndPresetRoundTrip() {
        beginTest("SettingsStore & PerformancePreset: Touch velocity curve round-trip and compatibility");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        const auto settingsFile = tempDir.getNonexistentChildFile("TouchSettingsTest", ".settings");
        const auto presetFile = tempDir.getNonexistentChildFile("TouchPresetTest", ".devpiano.preset");

        // 1. SettingsStore persistence
        {
            SettingsStore store(settingsFile);
            SettingsModel model;
            model.touchVelocityCurve = devpiano::input::TouchVelocityCurve::heavy;
            expect(store.save(model));
        }

        {
            SettingsStore store(settingsFile);
            SettingsModel loaded;
            loaded.touchVelocityCurve = devpiano::input::TouchVelocityCurve::standard;
            store.load(loaded);
            expect(loaded.touchVelocityCurve == devpiano::input::TouchVelocityCurve::heavy);
        }

        // 2. Preset serialization round-trip
        {
            auto preset = devpiano::layout::makeDefaultPreset();
            preset.name = "ExpressiveWide";
            preset.touchVelocityCurve = devpiano::input::TouchVelocityCurve::wideDynamic;
            expect(devpiano::layout::savePreset(preset, presetFile));

            auto loadedOpt = devpiano::layout::loadPreset(presetFile);
            expect(loadedOpt.has_value());
            if (loadedOpt.has_value()) {
                expect(loadedOpt->touchVelocityCurve == devpiano::input::TouchVelocityCurve::wideDynamic);
            }
        }

        // 3. Preset backward compatibility (legacy preset without touchVelocityCurve defaults to standard)
        {
            const juce::String legacyJson = R"({
                "version": 1,
                "name": "LegacyWithoutTouch",
                "layout": { "id": "l.1", "name": "L", "bindings": [] },
                "keyboard": { "keySignature": 0, "midiTranspose": false }
            })";

            const auto legacyFile = tempDir.getNonexistentChildFile("LegacyTouch", ".devpiano.preset");
            expect(legacyFile.replaceWithText(legacyJson));

            auto legacyLoaded = devpiano::layout::loadPreset(legacyFile);
            expect(legacyLoaded.has_value());
            if (legacyLoaded.has_value()) {
                expect(legacyLoaded->touchVelocityCurve == devpiano::input::TouchVelocityCurve::standard);
            }

            legacyFile.deleteFile();
        }

        settingsFile.deleteFile();
        presetFile.deleteFile();
    }
};

static TouchVelocityCurveTest touchVelocityCurveTest;

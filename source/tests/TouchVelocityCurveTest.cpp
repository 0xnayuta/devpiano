#include <JuceHeader.h>

#include "Input/TouchVelocityCurve.h"
#include "Layout/PerformancePreset.h"
#include "Settings/SettingsModel.h"
#include "Settings/SettingsStore.h"
#include "TestHelpers.h"

// ============================================================================
/// TouchVelocityCurveTest (Phase 29-C)
///
// ============================================================================
class TouchVelocityCurveTest final : public juce::UnitTest {
public:
    TouchVelocityCurveTest()
        : juce::UnitTest("TouchVelocityCurve", "DevPiano/Input") {
    }

    void runTest() override {
        testMathematicalCurveProperties();
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
    }

    void testSettingsStoreAndPresetRoundTrip() {
        beginTest("SettingsStore & PerformancePreset: Touch velocity curve round-trip");

        const devpiano::test::ScopedTempDir tempDir("touch-velocity-curve");
        const auto settingsFile = tempDir.getChildFile("settings.settings");
        const auto presetFile = tempDir.getChildFile("preset.devpiano.preset");

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
    }
};

static TouchVelocityCurveTest touchVelocityCurveTest;

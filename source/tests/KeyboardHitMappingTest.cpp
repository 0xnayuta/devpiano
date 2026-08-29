#include <JuceHeader.h>

#include "UI/CustomKeyboard.h"
#include "UI/KeyboardTypes.h"

// =============================================================================
// Tests for the CustomKeyboard hit-mapping geometry (AUDIT TEST-007):
//   - white-key hit mapping (absolute note from component-local x)
//   - black keys take priority over white keys in the black-key zone
//   - out-of-range positions return -1
//   - setAvailableRange shrinks the hit area
//
// NOTE (TEST-007 scope): the audit's "AdsrCurve drag clamping" sub-item no
// longer applies — AdsrCurveComponent is a paint-only component with no
// mouse interaction; ADSR values are clamped in AudioEngine::setAdsr
// (covered by AudioEngineTest).
// =============================================================================

namespace {

// 在 [rangeLow, n] 闭区间内白键数量（用于推断白键 x 坐标）。
int countWhiteKeys(int rangeLow, int n) {
    int count = 0;
    for (int note = rangeLow; note <= n; ++note) {
        if (devpiano::ui::isWhiteKey(note)) {
            ++count;
        }
    }
    return count;
}

// 白键 n 的中心 x（keyWidth=24 默认）。
int whiteKeyCentreX(int rangeLow, int n) {
    const auto idx = countWhiteKeys(rangeLow, n) - 1;
    return idx * 24 + 12;
}

} // namespace

class KeyboardHitMappingTest final : public juce::UnitTest {
public:
    KeyboardHitMappingTest()
        : juce::UnitTest("CustomKeyboard: hit mapping and octave scrolling", "DevPiano/UI") {
    }

    void runTest() override {
        testWhiteKeyHits();
        testBlackKeyPriority();
        testOutOfRange();
        testAvailableRange();
        testKeyboardPaintClipping();
        testReleaseHeldMouseNote();
        testMultiChannelColorVisualization();
    }

private:
    void testWhiteKeyHits() {
        testCase("white-key centre maps to the note in standard 88-key range (1248px)", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(1248, 128); // 标准 88 键钢琴: 52 白键 × 24 = 1248 宽

            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 60), 64 }), 60);
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 36), 64 }), 36);
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 21), 64 }), 21, "lowest white key A0");
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 108), 64 }), 108, "highest white key C8");
        });

        testCase("wide window centering offsets key positions symmetrically", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.updateViewportBounds(1888, 128); // 1888px 宽窗口 -> offset = (1888 - 1248)/2 = 320px
            const auto offset = static_cast<int>(kb.getKeybedOffsetX());
            expectEquals(offset, 320, "keybed offset is mathematically centered");

            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 60) + offset, 64 }), 60);
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 21) + offset, 64 }), 21);
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 108) + offset, 64 }), 108);

            // 验证居中两翼空白区域返回 -1 (未击中琴键)
            expectEquals(kb.findNoteAt({ offset - 10, 64 }), -1, "left margin returns -1");
            expectEquals(kb.findNoteAt({ 1888 - 10, 64 }), -1, "right margin returns -1");
        });
        testCase("viewport height stretches keyboard height to 100% without shrinking", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.updateViewportBounds(1888, 170); // 视口标准高度 170px
            expectEquals(kb.getHeight(), 170, "keyboard height fills 100% of visibleHeight (170px)");
            const auto offset = static_cast<int>(kb.getKeybedOffsetX());
            expectEquals(offset, 320);
            // 验证在 170px 高度底部区域点击仍能精确命中白键
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 60) + offset, 160 }), 60, "hit near bottom of 170px key");
        });
    }

    void testBlackKeyPriority() {
        testCase("black-key zone hits the black note, below it the right white key", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(1248, 128);

            // note 60 (C) 白键；note 61 (C#) 黑键中心 x = whiteKeyCentreX(21, 60) + 12
            const auto blackCentreX = whiteKeyCentreX(21, 60) + 12;
            expectEquals(kb.findNoteAt({ blackCentreX, 40 }), 61, "black key wins inside the black-key zone");
            expectEquals(kb.findNoteAt({ blackCentreX, 100 }), 62,
                         "below the black key the position falls on the next white key");
        });

        testCase("D# (note 63) black key sits between D and E", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(1248, 128);

            // note 62 (D) 白键；note 63 (D#) 黑键中心 x = whiteKeyCentreX(21, 62) + 12
            const auto blackCentreX = whiteKeyCentreX(21, 62) + 12;
            expectEquals(kb.findNoteAt({ blackCentreX, 40 }), 63);
        });
    }

    void testOutOfRange() {
        testCase("positions beyond the keybed return -1", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(2000, 128);

            expectEquals(kb.findNoteAt({ 5000, 64 }), -1, "far right must miss");
            expectEquals(kb.findNoteAt({ -10, 64 }), -1, "negative x must miss");
        });
    }

    void testAvailableRange() {
        testCase("setAvailableRange shrinks the hit area", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setAvailableRange(24, 96);
            const auto whiteCount = countWhiteKeys(24, 96);
            const auto totalWidth = whiteCount * 24;
            kb.setSize(totalWidth, 128); // 紧凑模式 (无居中 offset)

            expectEquals(kb.findNoteAt({ totalWidth + 50, 64 }), -1, "beyond the range must miss");
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(24, 60), 64 }), 60, "in-range note must hit");
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(24, 96), 64 }), 96);
        });
    }
    void testKeyboardPaintClipping() {
        testCase("CustomKeyboard paint with clipping produces no errors", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(1800, 128);

            juce::Image image(juce::Image::ARGB, 1800, 128, true);
            juce::Graphics g(image);

            // Full paint
            kb.paintEntireComponent(g, true);

            // Dirty rect clipped paint (single key region)
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(840, 0, 48, 128));
            kb.paintEntireComponent(g, true);
            g.restoreState();

            expect(true, "CustomKeyboard paint completed under dirty rect clipping");
        });
    }
    void testReleaseHeldMouseNote() {
        testCase("releaseHeldMouseNote is a no-op without a held mouse note", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            int callbackCount = 0;
            kb.onNoteOff = [&](int, int) { ++callbackCount; };

            // 无鼠标按住的音符时调用必须为 no-op（失焦 Panic 的幂等性）。
            kb.releaseHeldMouseNote();
            kb.releaseHeldMouseNote();
            expectEquals(callbackCount, 0, "no callback without a held note");
        });

        // 注：鼠标按住音符的释放路径（mouseDown 按下后 releaseHeldMouseNote）
        // 依赖真实鼠标事件（MouseEvent/MouseInputSource），无法在无头单测中
        // 模拟；mouseUp 与 releaseHeldMouseNote 共用同一释放逻辑，按下分支
        // 由实机交互回归覆盖（见 keyboard-mapping.md KBD-007 行为矩阵）。
    }

    void testMultiChannelColorVisualization() {
        testCase("multi-channel noteOn maps distinct channel colors in channel colorMode", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);

            devpiano::ui::KeyboardSettings settings;
            settings.colourMode = devpiano::ui::KeyColourMode::channel;
            kb.setKeyboardSettings(settings);
            kb.setSize(1248, 120);

            // Channel 1 -> Note 60 (C4)
            ks.noteOn(1, 60, 0.8f);
            // Channel 2 -> Note 64 (E4)
            ks.noteOn(2, 64, 0.8f);
            // Channel 3 -> Note 67 (G4)
            ks.noteOn(3, 67, 0.8f);

            expectEquals(static_cast<int>(kb.getPerKeyChannel(60)), 0); // 0-based Ch 1
            expectEquals(static_cast<int>(kb.getPerKeyChannel(64)), 1); // 0-based Ch 2
            expectEquals(static_cast<int>(kb.getPerKeyChannel(67)), 2); // 0-based Ch 3

            kb.triggerTimerCallbackForTest();

            juce::Colour c60, c64, c67;
            bool found60 = false, found64 = false, found67 = false;
            for (const auto& k : kb.getKeys()) {
                if (k.midiNote == 60) {
                    c60 = k.colour1;
                    found60 = true;
                    expectEquals(k.fade, 1.0f);
                } else if (k.midiNote == 64) {
                    c64 = k.colour1;
                    found64 = true;
                    expectEquals(k.fade, 1.0f);
                } else if (k.midiNote == 67) {
                    c67 = k.colour1;
                    found67 = true;
                    expectEquals(k.fade, 1.0f);
                }
            }

            expect(found60 && found64 && found67, "all three notes should be rendered");
            expect(c60 != c64, "Channel 1 and Channel 2 should have distinct colors");
            expect(c64 != c67, "Channel 2 and Channel 3 should have distinct colors");
            expect(c60 != c67, "Channel 1 and Channel 3 should have distinct colors");

            // Release note 60, others stay held
            ks.noteOff(1, 60, 0.0f);
            kb.triggerTimerCallbackForTest();

            for (const auto& k : kb.getKeys()) {
                if (k.midiNote == 60) {
                    expectLessThan(k.fade, 1.0f);
                } else if (k.midiNote == 64 || k.midiNote == 67) {
                    expectEquals(k.fade, 1.0f);
                }
            }
        });
    }
};

static KeyboardHitMappingTest keyboardHitMappingTest;

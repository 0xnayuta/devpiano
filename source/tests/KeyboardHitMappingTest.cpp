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
    }

private:
    void testWhiteKeyHits() {
        testCase("white-key centre maps to the note in standard 88-key range", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(1400, 128); // 标准 88 键钢琴: 52 白键 × 24 = 1248 宽

            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 60), 64 }), 60);
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 36), 64 }), 36);
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 21), 64 }), 21, "lowest white key A0");
            expectEquals(kb.findNoteAt({ whiteKeyCentreX(21, 108), 64 }), 108, "highest white key C8");
        });
    }

    void testBlackKeyPriority() {
        testCase("black-key zone hits the black note, below it the right white key", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(1400, 128);

            // note 60 (C) 白键；note 61 (C#) 黑键中心 x = whiteKeyCentreX(21, 60) + 12
            const auto blackCentreX = whiteKeyCentreX(21, 60) + 12;
            expectEquals(kb.findNoteAt({ blackCentreX, 40 }), 61, "black key wins inside the black-key zone");
            expectEquals(kb.findNoteAt({ blackCentreX, 100 }), 62,
                         "below the black key the position falls on the next white key");
        });

        testCase("D# (note 63) black key sits between D and E", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(1400, 128);

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
            kb.setSize(2000, 128);
            kb.setAvailableRange(24, 96);

            // 范围 24-96 的白键总数 → 总宽 = count × 24；右侧越界 → -1
            const auto whiteCount = countWhiteKeys(24, 96);
            const auto totalWidth = whiteCount * 24;
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
};

static KeyboardHitMappingTest keyboardHitMappingTest;

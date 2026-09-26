#include <JuceHeader.h>
#include <cmath>

#include "Midi/MidiChannelMapper.h"
#include "UI/CustomKeyboard.h"
#include "UI/KeyboardTypes.h"
#include "UI/jive/DesignTokens.h"

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

int countWhiteKeys(int rangeLow, int n) {
    int count = 0;
    for (int note = rangeLow; note <= n; ++note) {
        if (devpiano::ui::isWhiteKey(note)) {
            ++count;
        }
    }
    return count;
}

const devpiano::ui::KeyRenderState* keyForNote(const CustomKeyboard& keyboard, int note) {
    for (const auto& key : keyboard.getKeys()) {
        if (key.midiNote == note) {
            return &key;
        }
    }
    return nullptr;
}

juce::Point<int> keyCentre(const CustomKeyboard& keyboard, int note) {
    if (const auto* key = keyForNote(keyboard, note)) {
        return { juce::roundToInt(key->bounds.getCentreX()), juce::roundToInt(key->bounds.getCentreY()) };
    }
    return { -1, -1 };
}

} // namespace

class KeyboardHitMappingTest final : public juce::UnitTest {
public:
    KeyboardHitMappingTest()
        : juce::UnitTest("CustomKeyboard: hit mapping and octave scrolling", "DevPiano/UI") {
    }

    void runTest() override {
        testWhiteKeyHits();
        testDefaultKeyboardFitsViewport();
        testBlackKeyPriority();
        testOutOfRange();
        testAvailableRange();
        testKeyboardPaintClipping();
        testReleaseHeldMouseNote();
        testMultiChannelColorVisualization();
        testMouseDragGlissando();
        testMouseReleaseUsesMappedIdentityAfterMapperChange();
    }

private:
    void testWhiteKeyHits() {
        testCase("white-key centres map to notes in the standard 88-key range", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);

            expectEquals(kb.findNoteAt(keyCentre(kb, 60)), 60);
            expectEquals(kb.findNoteAt(keyCentre(kb, 36)), 36);
            expectEquals(kb.findNoteAt(keyCentre(kb, 21)), 21, "lowest white key A0");
            expectEquals(kb.findNoteAt(keyCentre(kb, 108)), 108, "highest white key C8");
        });

        testCase("wide viewport centres the keybed and preserves empty side margins", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            constexpr int visibleWidth = 1888;
            kb.updateViewportBounds(visibleWidth, 138);

            const auto contentWidth = static_cast<float>(countWhiteKeys(21, 108)) * kb.getKeyboardSettings().keyWidth;
            const auto expectedOffset = (static_cast<float>(visibleWidth) - contentWidth) * 0.5f;
            expect(std::abs(kb.getKeybedOffsetX() - expectedOffset) < 0.01f, "keybed is horizontally centred");
            expectEquals(kb.findNoteAt(keyCentre(kb, 60)), 60);
            expectEquals(kb.findNoteAt({ juce::roundToInt(expectedOffset) - 10, 69 }), -1, "left margin misses");
            expectEquals(kb.findNoteAt({ visibleWidth - 10, 69 }), -1, "right margin misses");
        });

        testCase("taller viewport centres a proportionate keybed without stretching it", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.updateViewportBounds(1888, 170);

            const auto* middleC = keyForNote(kb, 60);
            expect(middleC != nullptr);
            if (middleC == nullptr) {
                return;
            }

            const auto keyWidth = kb.getKeyboardSettings().keyWidth;
            expectEquals(kb.getHeight(), 170, "component fills the taller viewport");
            expect(std::abs(middleC->bounds.getHeight() - keyWidth * 6.4f) < 0.01f,
                   "white-key length remains proportional to its width");
            const auto centre = keyCentre(kb, 60);
            expectEquals(kb.findNoteAt(centre), 60);
            expectEquals(kb.findNoteAt({ centre.x, 160 }), -1, "centred vertical padding is not a key");
        });
    }

    void testDefaultKeyboardFitsViewport() {
        testCase("default viewport fits the full keybed while a narrow viewport requires scrolling", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            const auto* lowestKey = keyForNote(kb, 21);
            expect(lowestKey != nullptr);
            if (lowestKey == nullptr) {
                return;
            }

            const auto visibleHeight = juce::roundToInt(lowestKey->bounds.getHeight());
            const auto defaultWidth = devpiano::ui::DesignTokens::get().windowDefaultWidth() - 32;
            const auto narrowWidth = devpiano::ui::DesignTokens::get().windowMinWidth() - 32;
            const auto keybedWidth
                = juce::roundToInt(static_cast<float>(countWhiteKeys(21, 108)) * kb.getKeyboardSettings().keyWidth);

            kb.updateViewportBounds(defaultWidth, visibleHeight);
            expectEquals(kb.getWidth(), defaultWidth, "default window displays the complete keybed");
            expect(kb.getWidth() >= keybedWidth);

            kb.updateViewportBounds(narrowWidth, visibleHeight);
            expectEquals(kb.getWidth(), keybedWidth, "narrow viewport keeps the full scrollable keybed width");
            expect(kb.getWidth() > narrowWidth, "narrow viewport leaves horizontal content to scroll");
        });
    }

    void testBlackKeyPriority() {
        testCase("black-key zone hits the black note, below it the right white key", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            const auto* cSharp = keyForNote(kb, 61);
            expect(cSharp != nullptr);
            if (cSharp == nullptr) {
                return;
            }

            const auto centre = keyCentre(kb, 61);
            expectEquals(kb.findNoteAt(centre), 61, "black key wins inside its bounds");
            const auto belowBlack = juce::Point<int> { centre.x, juce::roundToInt(cSharp->bounds.getBottom() + 5.0f) };
            expectEquals(kb.findNoteAt(belowBlack), 62, "below the black key the right white key receives the hit");
        });

        testCase("D# (note 63) black key sits between D and E", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            expectEquals(kb.findNoteAt(keyCentre(kb, 63)), 63);
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
            const auto totalWidth
                = juce::roundToInt(static_cast<float>(countWhiteKeys(24, 96)) * kb.getKeyboardSettings().keyWidth);
            kb.setSize(totalWidth, 138);

            expectEquals(kb.findNoteAt({ totalWidth + 50, 69 }), -1, "beyond the range must miss");
            expectEquals(kb.findNoteAt(keyCentre(kb, 60)), 60, "in-range note must hit");
            expectEquals(kb.findNoteAt(keyCentre(kb, 96)), 96);
        });
    }
    void testKeyboardPaintClipping() {
        testCase("CustomKeyboard paint with clipping produces no errors", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setVisible(true);
            kb.setSize(1800, 128);
            juce::Image image(juce::Image::ARGB, 1800, 128, true);
            {
                juce::Graphics g(image);

                // Full paint
                kb.paintEntireComponent(g, true);

                // Dirty rect clipped paint (single key region)
                g.saveState();
                g.reduceClipRegion(juce::Rectangle<int>(840, 0, 48, 128));
                kb.paintEntireComponent(g, true);
                g.restoreState();
            }
            // Observable assertion (TEST-008): verify painted pixels exist in rendered buffer
            // (sampled across keybed region x in [276, 1524])
            bool hasNonTransparentPixels = false;
            for (int y = 0; y < 128 && !hasNonTransparentPixels; y += 8) {
                for (int x = 276; x < 1524 && !hasNonTransparentPixels; x += 16) {
                    if (image.getPixelAt(x, y).getAlpha() > 0) {
                        hasNonTransparentPixels = true;
                    }
                }
            }
            expect(hasNonTransparentPixels,
                   "CustomKeyboard paint rendered visible pixels into image under dirty rect clipping");
        });
    }

    void testReleaseHeldMouseNote() {
        testCase("releaseHeldMouseNote is a no-op without a held mouse note", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            int callbackCount = 0;
            kb.onNoteOff = [&](const devpiano::core::MidiNoteIdentity&) { ++callbackCount; };

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

    void testMouseDragGlissando() {
        testCase("mouse drag glissando across keys triggers noteOff and noteOn", [&] {
            juce::MidiKeyboardState ks;
            CustomKeyboard kb(ks);
            kb.setSize(1248, 120);

            // Focus configuration check
            expect(!kb.getWantsKeyboardFocus(), "CustomKeyboard does not want keyboard focus");
            expect(!kb.getMouseClickGrabsKeyboardFocus(), "CustomKeyboard does not grab focus on mouse click");

            std::vector<int> notesOn;
            std::vector<int> notesOff;
            kb.onNoteOn = [&](int note, int sourceChannel) {
                notesOn.push_back(note);
                return devpiano::core::MidiNoteIdentity { devpiano::core::MidiNoteNumber::fromClamped(note),
                                                          devpiano::core::MidiChannel::fromClamped(sourceChannel + 1) };
            };
            kb.onNoteOff
                = [&](const devpiano::core::MidiNoteIdentity& identity) { notesOff.push_back(identity.note.value); };

            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const auto xC4 = keyCentre(kb, 60).x;
            const auto xD4 = keyCentre(kb, 62).x;
            const auto xE4 = keyCentre(kb, 64).x;

            // 1. Mouse down on C4 (60)
            juce::MouseEvent downEvent(source, { static_cast<float>(xC4), 80.0f },
                                       juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &kb, &kb,
                                       juce::Time::getCurrentTime(), { static_cast<float>(xC4), 80.0f },
                                       juce::Time::getCurrentTime(), 1, false);
            static_cast<juce::Component&>(kb).mouseDown(downEvent);

            expectEquals(notesOn.size(), static_cast<size_t>(1));
            if (!notesOn.empty()) {
                expectEquals(notesOn.back(), 60);
            }

            // 2. Drag to D4 (62)
            juce::MouseEvent dragEvent1(source, { static_cast<float>(xD4), 80.0f },
                                        juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &kb, &kb,
                                        juce::Time::getCurrentTime(), { static_cast<float>(xC4), 80.0f },
                                        juce::Time::getCurrentTime(), 1, true);
            static_cast<juce::Component&>(kb).mouseDrag(dragEvent1);

            expectEquals(notesOn.size(), static_cast<size_t>(2));
            expectEquals(notesOff.size(), static_cast<size_t>(1));
            if (notesOn.size() >= 2) {
                expectEquals(notesOn.back(), 62);
            }
            if (!notesOff.empty()) {
                expectEquals(notesOff.back(), 60);
            }

            // 3. Drag to E4 (64)
            juce::MouseEvent dragEvent2(source, { static_cast<float>(xE4), 80.0f },
                                        juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &kb, &kb,
                                        juce::Time::getCurrentTime(), { static_cast<float>(xC4), 80.0f },
                                        juce::Time::getCurrentTime(), 1, true);
            static_cast<juce::Component&>(kb).mouseDrag(dragEvent2);

            expectEquals(notesOn.size(), static_cast<size_t>(3));
            expectEquals(notesOff.size(), static_cast<size_t>(2));
            if (notesOn.size() >= 3) {
                expectEquals(notesOn.back(), 64);
            }
            if (notesOff.size() >= 2) {
                expectEquals(notesOff.back(), 62);
            }

            // 4. Mouse up releases E4 (64)
            juce::MouseEvent upEvent(source, { static_cast<float>(xE4), 80.0f }, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f,
                                     0.0f, 0.0f, &kb, &kb, juce::Time::getCurrentTime(),
                                     { static_cast<float>(xC4), 80.0f }, juce::Time::getCurrentTime(), 1, false);
            static_cast<juce::Component&>(kb).mouseUp(upEvent);

            expectEquals(notesOff.size(), static_cast<size_t>(3));
            if (notesOff.size() >= 3) {
                expectEquals(notesOff.back(), 64);
            }
        });
    }
    void testMouseReleaseUsesMappedIdentityAfterMapperChange() {
        testCase("mouse release uses the mapped identity captured at press time", [&] {
            devpiano::midi::ChannelMatrix initialMatrix;
            initialMatrix.active = true;
            initialMatrix.channels[0].outputChannel = 3;
            initialMatrix.channels[0].transpose = 12;
            devpiano::midi::MidiChannelMapper initialMapper(initialMatrix, false, 0);

            devpiano::midi::ChannelMatrix updatedMatrix;
            updatedMatrix.active = true;
            updatedMatrix.channels[0].outputChannel = 8;
            updatedMatrix.channels[0].transpose = -12;
            devpiano::midi::MidiChannelMapper updatedMapper(updatedMatrix, false, 0);

            auto* activeMapper = &initialMapper;
            juce::MidiKeyboardState state;
            CustomKeyboard keyboard(state);
            keyboard.setSize(1248, 120);
            keyboard.onNoteOn = [&](int note, int sourceChannel) {
                return activeMapper->sendNoteOn(sourceChannel, note, 1.0f, state);
            };
            keyboard.onNoteOff = [&](const devpiano::core::MidiNoteIdentity& identity) {
                activeMapper->sendNoteOff(identity, 1.0f, state);
            };

            const auto x = keyCentre(keyboard, 60).x;
            const auto source = juce::Desktop::getInstance().getMainMouseSource();
            const juce::MouseEvent pressEvent(source, { static_cast<float>(x), 80.0f },
                                              juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                              &keyboard, &keyboard, juce::Time::getCurrentTime(),
                                              { static_cast<float>(x), 80.0f }, juce::Time::getCurrentTime(), 1, false);
            keyboard.mouseDown(pressEvent);

            expect(state.isNoteOn(4, 72));
            activeMapper = &updatedMapper;
            keyboard.releaseHeldMouseNote();

            expect(!state.isNoteOn(4, 72), "the original mapped note must be released");
            expect(!state.isNoteOn(9, 48), "the replacement mapping must not receive the old note-off");
        });
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

            juce::Colour c60;
            juce::Colour c64;
            juce::Colour c67;
            bool found60 = false;
            bool found64 = false;
            bool found67 = false;
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

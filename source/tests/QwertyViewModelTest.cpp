#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"
#include "Core/QwertyModel.h"
#include "Input/KeyboardMidiMapper.h"
#include "Midi/MidiChannelMapper.h"
#include "UI/QwertyComponent.h"

namespace {

class QwertyViewModelTest final : public juce::UnitTest {
public:
    QwertyViewModelTest()
        : juce::UnitTest("QwertyViewModel", "DevPiano/Input") {
    }

    void runTest() override {
        testAnsiTemplateGeometry();
        testSnapshotDefaultBindings();
        testKeySignatureShift();
        testHeldKeyStateReflection();
        testPedalStateReflection();
        testComponentHitTestingAndInteraction();
        testMouseInteractionWithMidiChannelMapper();
        testPitchClassHarmonyPalette();
    }

private:
    void testAnsiTemplateGeometry() {
        beginTest("ANSI template has 5 rows and strict 15.0f total width weight");

        const auto vm = devpiano::core::makeDefaultQwertyLayoutTemplate();

        // 5 rows
        expectEquals(static_cast<int>(vm.rows.size()), 5);

        // Row counts: Number(14), QWERTY(14), ASDF(13), ZXCV(12), Bottom(7)
        expectEquals(static_cast<int>(vm.rows[0].keys.size()), 14);
        expectEquals(static_cast<int>(vm.rows[1].keys.size()), 14);
        expectEquals(static_cast<int>(vm.rows[2].keys.size()), 13);
        expectEquals(static_cast<int>(vm.rows[3].keys.size()), 12);
        expectEquals(static_cast<int>(vm.rows[4].keys.size()), 7);

        // Verify width weight sum per row is exactly 15.0f (+-0.001)
        for (std::size_t r = 0; r < 5; ++r) {
            float totalWeight = 0.0f;
            for (const auto& key : vm.rows[r].keys) {
                totalWeight += key.widthWeight;
            }
            expect(std::abs(totalWeight - 15.0f) < 0.001f,
                   "Row " + juce::String(static_cast<int>(r)) + " sum must be 15.0f");
        }

        // Space key must be marked as sustain pedal in bottom row
        const auto& r4 = vm.rows[4].keys;
        bool foundSustain = false;
        for (const auto& k : r4) {
            if (k.isSustainPedal) {
                foundSustain = true;
                expectEquals(k.keyCode, static_cast<int>(juce::KeyPress::spaceKey));
                expect(k.widthWeight >= 5.0f);
            }
        }
        expect(foundSustain, "Row 4 must contain sustain pedal (Space)");

        // Tab key must be marked as soft pedal in row 1
        const auto& r1 = vm.rows[1].keys;
        bool foundSoft = false;
        for (const auto& k : r1) {
            if (k.isSoftPedal) {
                foundSoft = true;
                expectEquals(k.keyCode, static_cast<int>(juce::KeyPress::tabKey));
            }
        }
        expect(foundSoft, "Row 1 must contain soft pedal (Tab)");
    }

    void testSnapshotDefaultBindings() {
        beginTest("Default layout maps alphanumeric keys to notes with pitch names");

        KeyboardMidiMapper mapper;
        const auto vm = mapper.createQwertySnapshot(0);

        // In default layout, 'Q' is mapped to C4 (MIDI 60) or C5 (MIDI 72)
        // Let's locate 'Q' in row 1
        const auto& r1 = vm.rows[1].keys;
        const devpiano::core::QwertyKeyVisualState* qKey = nullptr;
        for (const auto& k : r1) {
            if (k.keyCode == 'Q') {
                qKey = &k;
                break;
            }
        }
        expect(qKey != nullptr, "Q key must exist in row 1");
        if (qKey != nullptr) {
            expect(qKey->mappedMidiNote >= 0, "Q must be mapped to a MIDI note");
            expect(qKey->noteName.isNotEmpty(), "Q must have a pitch name");
            expect(qKey->solfegeLabel.isNotEmpty(), "Q must have a solfege label");
            expect(!qKey->isDown, "Q must be initially up");
        }

        // 'A' key in row 2
        const auto& r2 = vm.rows[2].keys;
        const devpiano::core::QwertyKeyVisualState* aKey = nullptr;
        for (const auto& k : r2) {
            if (k.keyCode == 'A') {
                aKey = &k;
                break;
            }
        }
        expect(aKey != nullptr, "A key must exist in row 2");
        if (aKey != nullptr) {
            expect(aKey->mappedMidiNote >= 0, "A must be mapped to a MIDI note");
            expect(aKey->noteName.isNotEmpty());
        }
    }

    void testKeySignatureShift() {
        beginTest("Snapshot reflects keySignature shifts in solfege labels");

        KeyboardMidiMapper mapper;
        const auto vm0 = mapper.createQwertySnapshot(0);
        const auto vm1 = mapper.createQwertySnapshot(2); // D major (+2 semitones)

        // Compare 'A' key
        const auto findKey = [](const devpiano::core::QwertyViewModel& model,
                                int code) -> const devpiano::core::QwertyKeyVisualState* {
            for (const auto& row : model.rows) {
                for (const auto& k : row.keys) {
                    if (k.keyCode == code) {
                        return &k;
                    }
                }
            }
            return nullptr;
        };

        const auto* k0 = findKey(vm0, 'A');
        const auto* k1 = findKey(vm1, 'A');

        expect(k0 != nullptr && k1 != nullptr);
        if (k0 != nullptr && k1 != nullptr) {
            // Note pitch remains identical (C3), but solfege label shifts
            expectEquals(k0->mappedMidiNote, k1->mappedMidiNote);
            expect(k0->solfegeLabel != k1->solfegeLabel, "Solfege should reflect key signature shift");
        }
    }

    void testHeldKeyStateReflection() {
        beginTest("Pressed keys update isDown in snapshot and release cleanly");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        // Press 'Q'
        juce::KeyPress qPress('q');
        expect(mapper.handleKeyPressed(qPress, state));

        // Snapshot should show 'Q' down
        auto vm = mapper.createQwertySnapshot(0);
        bool qIsDown = false;
        for (const auto& row : vm.rows) {
            for (const auto& k : row.keys) {
                if (k.keyCode == 'Q') {
                    qIsDown = k.isDown;
                }
            }
        }
        expect(qIsDown, "Q must be marked down after press");

        // Release all
        mapper.releaseAllHeldKeys(state);
        vm = mapper.createQwertySnapshot(0);
        qIsDown = false;
        for (const auto& row : vm.rows) {
            for (const auto& k : row.keys) {
                if (k.keyCode == 'Q') {
                    qIsDown = k.isDown;
                }
            }
        }
        expect(!qIsDown, "Q must be marked up after releaseAllHeldKeys");
    }

    void testPedalStateReflection() {
        beginTest("Pedal presses reflect on Space and Tab states");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        // Space -> Sustain Pedal
        juce::KeyPress spacePress(juce::KeyPress::spaceKey);
        expect(mapper.handleKeyPressed(spacePress, state));

        auto vm = mapper.createQwertySnapshot(0);
        expect(vm.isSustainPedalDown, "Sustain pedal state must be true");

        bool spaceDown = false;
        for (const auto& k : vm.rows[4].keys) {
            if (k.isSustainPedal) {
                spaceDown = k.isDown;
            }
        }
        expect(spaceDown, "Space key must be down when sustain is active");

        // Release space via releaseAllHeldKeys
        mapper.releaseAllHeldKeys(state);
        vm = mapper.createQwertySnapshot(0);
        expect(!vm.isSustainPedalDown);
    }

    void testComponentHitTestingAndInteraction() {
        beginTest("QwertyComponent hit-testing, mouse clicks, and animation timers");

        KeyboardMidiMapper mapper;
        const auto vm = mapper.createQwertySnapshot(0);

        devpiano::ui::QwertyComponent comp;
        comp.setSize(750, 150);
        comp.updateViewModel(vm);

        // Find centre of component (Row 2, near middle G or H key)
        const auto hit = comp.findKeyAt({ 375, 75 });
        expect(hit.key != nullptr, "Middle point must hit a valid key");

        // Verify noteOn/noteOff callbacks
        int noteOnTriggered = -1;
        int noteOffTriggered = -1;
        comp.onNoteOn = [&](int note, int, float) { noteOnTriggered = note; };
        comp.onNoteOff = [&](int note, int) { noteOffTriggered = note; };

        // Simulate mouse down on a note-mapped key
        // Let's find coordinate of 'Q'
        juce::Point<int> qPos;
        for (int r = 0; r < 5; ++r) {
            for (int k = 0; k < static_cast<int>(vm.rows[static_cast<std::size_t>(r)].keys.size()); ++k) {
                if (vm.rows[static_cast<std::size_t>(r)].keys[static_cast<std::size_t>(k)].keyCode == 'Q') {
                    // Search using probe points
                    for (int x = 10; x < 200; x += 5) {
                        const auto h = comp.findKeyAt({ x, 45 });
                        if (h.key != nullptr && h.key->keyCode == 'Q') {
                            qPos = { x, 45 };
                            break;
                        }
                    }
                }
            }
        }

        if (qPos.getX() > 0) {
            auto mouseSource = juce::Desktop::getInstance().getMainMouseSource();
            const juce::MouseEvent pressEv(mouseSource, qPos.toFloat(), juce::ModifierKeys::leftButtonModifier, 1.0f,
                                           0.0f, 0.0f, 0.0f, 0.0f, &comp, &comp, juce::Time::getCurrentTime(),
                                           qPos.toFloat(), juce::Time::getCurrentTime(), 1, false);
            comp.mouseDown(pressEv);
            expect(noteOnTriggered >= 0, "Mouse down on Q must trigger onNoteOn");

            const juce::MouseEvent releaseEv(mouseSource, qPos.toFloat(), juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, &comp, &comp, juce::Time::getCurrentTime(), qPos.toFloat(),
                                             juce::Time::getCurrentTime(), 1, false);
            comp.mouseUp(releaseEv);
            expectEquals(noteOffTriggered, noteOnTriggered);
        }

        // Trigger timer callback multiple times to simulate decay
        for (int i = 0; i < 30; ++i) {
            comp.triggerTimerForTest();
        }
    }

    void testPitchClassHarmonyPalette() {
        beginTest("12-TET Pitch-Class Chromatic Harmony Palette symmetry and contrast");

        // 12 semitones must be 30 deg apart
        for (int i = 0; i < 12; ++i) {
            expectEquals(devpiano::core::pitchClassHarmonyHues[i], static_cast<float>(i * 30));
        }

        // Octave invariance: C3 (48), C4 (60), C5 (72) should produce identical hues
        const auto c3Colour = devpiano::core::getPitchClassHarmonyColour(48);
        const auto c4Colour = devpiano::core::getPitchClassHarmonyColour(60);
        const auto c5Colour = devpiano::core::getPitchClassHarmonyColour(72);
        expectEquals(c3Colour.getHue(), c4Colour.getHue());
        expectEquals(c4Colour.getHue(), c5Colour.getHue());

        // Tritone complement: C (0 deg) and F# (180 deg)
        const auto fSharpColour = devpiano::core::getPitchClassHarmonyColour(66); // F#4
        expect(std::abs(fSharpColour.getHue() - c4Colour.getHue() - 0.5f) < 0.01f,
               "C and F# must be tritone complements (180 deg / 0.5 hue distance)");

        // Contrast luminance
        const auto darkBg = juce::Colour(0xFF181A1F);
        const auto brightBg = juce::Colour(0xFFFAFAFA);
        expect(devpiano::core::getContrastingTextColour(darkBg) == juce::Colours::white);
        expect(devpiano::core::getContrastingTextColour(brightBg) == juce::Colour(0xFF0F172A));
    }

    void testMouseInteractionWithMidiChannelMapper() {
        beginTest("Qwerty mouse callback correctly routes through MidiChannelMapper");

        devpiano::midi::ChannelMatrix matrix;
        matrix.active = true;
        // Map input channel 1 (0-based index 0) to output channel 4 with +12 semitone transpose
        matrix.channels[0].outputChannel = 3; // 0-based 3 maps to MIDI channel 4
        matrix.channels[0].transpose = 12;

        devpiano::midi::MidiChannelMapper channelMapper(matrix, false, 0);
        juce::MidiKeyboardState keyboardState;

        devpiano::ui::QwertyComponent comp;
        comp.setSize(750, 150);

        KeyboardMidiMapper mapper;
        comp.updateViewModel(mapper.createQwertySnapshot(0));

        // Simulate the MainComponent wiring:
        comp.onNoteOn = [&](int midiNote, int midiChannel, float velocity) {
            const auto zeroBasedCh = juce::jlimit(0, 15, midiChannel - 1);
            channelMapper.sendNoteOn(zeroBasedCh, midiNote, velocity, keyboardState);
        };
        comp.onNoteOff = [&](int midiNote, int midiChannel) {
            const auto zeroBasedCh = juce::jlimit(0, 15, midiChannel - 1);
            channelMapper.sendNoteOff(zeroBasedCh, midiNote, 1.0f, keyboardState);
        };

        // Trigger note on C4 (60) on channel 1
        comp.onNoteOn(60, 1, 0.9f);
        expect(keyboardState.isNoteOn(4, 72), "Channel 1 note 60 must be transposed to channel 4 note 72");
        expect(!keyboardState.isNoteOn(1, 60), "Raw channel 1 note 60 must not be triggered directly");

        comp.onNoteOff(60, 1);
        expect(!keyboardState.isNoteOn(4, 72), "Transposed note on channel 4 must be released");
    }
};

static QwertyViewModelTest qwertyViewModelTest;

} // namespace

#include <JuceHeader.h>

#include "Core/KeyMapTypes.h"
#include "Core/QwertyModel.h"
#include "Input/KeyboardMidiMapper.h"
#include "Midi/MidiChannelMapper.h"
#include "UI/CustomKeyboard.h"
#include "UI/QwertyComponent.h"

namespace {

class QwertyViewModelTest final : public juce::UnitTest {
public:
    QwertyViewModelTest()
        : juce::UnitTest("QwertyViewModel", "DevPiano/Input") {
    }

    void runTest() override {
        testKeySignatureShift();
        testHeldKeyStateReflection();
        testPedalStateReflection();
        testComponentHitTestingAndInteraction();
        testMouseInteractionWithMidiChannelMapper();
        testPerformanceMapConsumers();
        testMutedMapConsumers();
        testLowestOctaveLabels();
        testPitchClassHarmonyPalette();
        testChordHudAndFadeout();
        testNumpadAndPartitionViewModelSnapshot();
        testMouseInteractionPreservesPhysicalKeyIdentityAndZone();
        testNarrowWindowHorizontalScrolling();
    }

private:
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
        comp.onNoteOn = [&](int note, int channel, float) {
            noteOnTriggered = note;
            return devpiano::core::MidiNoteIdentity { devpiano::core::MidiNoteNumber::fromClamped(note),
                                                      devpiano::core::MidiChannel::fromClamped(channel) };
        };
        comp.onNoteOff
            = [&](const devpiano::core::MidiNoteIdentity& identity) { noteOffTriggered = identity.note.value; };

        // Simulate mouse down on a note-mapped key
        // Let's find coordinate of 'Q'
        juce::Point<int> qPos;
        for (const auto& row : vm.rows) {
            for (const auto& key : row.keys) {
                if (key.keyCode == 'Q') {
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

        expect(qPos.getX() > 0, "Q key coordinate resolution must succeed");
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
        beginTest("Qwerty mouse release uses its press-time mapped identity");

        KeyboardMidiMapper mapper;
        const auto vm = mapper.createQwertySnapshot(0);

        devpiano::ui::QwertyComponent comp;
        comp.setSize(750, 150);
        comp.updateViewModel(vm);

        juce::Point<int> qPos;
        for (int x = 10; x < 200; x += 5) {
            const auto hit = comp.findKeyAt({ x, 45 });
            if (hit.key != nullptr && hit.key->keyCode == 'Q') {
                qPos = { x, 45 };
                break;
            }
        }
        expect(qPos.getX() > 0, "Q key coordinate resolution must succeed");
        if (qPos.getX() <= 0) {
            return;
        }

        const auto qHit = comp.findKeyAt(qPos);
        expect(qHit.key != nullptr);
        if (qHit.key == nullptr) {
            return;
        }

        const auto inputChannel = juce::jlimit(1, 16, qHit.key->mappedMidiChannel);
        const auto inputChannelIndex = inputChannel - 1;
        const auto inputNote = qHit.key->mappedMidiNote;

        devpiano::midi::ChannelMatrix initialMatrix;
        initialMatrix.active = true;
        initialMatrix.channels[static_cast<std::size_t>(inputChannelIndex)].outputChannel = 3;
        initialMatrix.channels[static_cast<std::size_t>(inputChannelIndex)].transpose = 12;
        devpiano::midi::MidiChannelMapper initialMapper(initialMatrix, false, 0);

        auto updatedMatrix = initialMatrix;
        updatedMatrix.channels[static_cast<std::size_t>(inputChannelIndex)].outputChannel = 8;
        updatedMatrix.channels[static_cast<std::size_t>(inputChannelIndex)].transpose = -12;
        devpiano::midi::MidiChannelMapper updatedMapper(updatedMatrix, false, 0);

        auto* activeMapper = &initialMapper;
        juce::MidiKeyboardState keyboardState;
        comp.onNoteOn = [&](int note, int channel, float velocity) {
            const auto zeroBasedChannel = juce::jlimit(0, 15, channel - 1);
            return activeMapper->sendNoteOn(zeroBasedChannel, note, velocity, keyboardState);
        };
        comp.onNoteOff = [&](const devpiano::core::MidiNoteIdentity& identity) {
            activeMapper->sendNoteOff(identity, 1.0f, keyboardState);
        };

        const auto position = qPos.toFloat();
        const auto mouseSource = juce::Desktop::getInstance().getMainMouseSource();
        const juce::MouseEvent pressEvent(mouseSource, position, juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f,
                                          0.0f, 0.0f, 0.0f, &comp, &comp, juce::Time::getCurrentTime(), position,
                                          juce::Time::getCurrentTime(), 1, false);
        comp.mouseDown(pressEvent);

        const auto originalNote = juce::jlimit(0, 127, inputNote + 12);
        expect(keyboardState.isNoteOn(4, originalNote), "Q note must use the initial channel matrix");

        activeMapper = &updatedMapper;
        const juce::MouseEvent releaseEvent(mouseSource, position, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                            &comp, &comp, juce::Time::getCurrentTime(), position,
                                            juce::Time::getCurrentTime(), 1, false);
        comp.mouseUp(releaseEvent);

        expect(!keyboardState.isNoteOn(4, originalNote), "Q note-off must release its original mapped note");
        expect(!keyboardState.isNoteOn(9, juce::jlimit(0, 127, inputNote - 12)),
               "Q note-off must not use the replacement matrix");
    }

    static juce::MouseEvent pressAt(juce::Component& component, juce::Point<int> point) {
        const auto time = juce::Time::getCurrentTime();
        return { juce::Desktop::getInstance().getMainMouseSource(),
                 point.toFloat(),
                 juce::ModifierKeys::leftButtonModifier,
                 1.0f,
                 0.0f,
                 0.0f,
                 0.0f,
                 0.0f,
                 &component,
                 &component,
                 time,
                 point.toFloat(),
                 time,
                 1,
                 false };
    }

    static juce::Point<int> pianoPosition(const CustomKeyboard& piano, int note) {
        for (const auto& key : piano.getKeys()) {
            if (key.midiNote == note) {
                return { juce::roundToInt(key.bounds.getCentreX()), juce::roundToInt(key.bounds.getBottom() - 3.0f) };
            }
        }
        return { -1, -1 };
    }

    static juce::Point<int> qwertyPosition(const devpiano::ui::QwertyComponent& qwerty) {
        for (int x = 0; x < qwerty.getWidth(); ++x) {
            const auto hit = qwerty.findKeyAt({ x, 75 });
            if (hit.key != nullptr && hit.key->keyCode == 'A') {
                return { x, 75 };
            }
        }
        return { -1, -1 };
    }

    void testPerformanceMapConsumers() {
        testCase("both maps follow final identity without reapplying group, modifiers or matrix", [&] {
            struct Scenario {
                int group;
                bool alt;
                bool matrixActive;
                int note;
                int channel;
            };
            for (const auto scenario : { Scenario { 0, false, true, 72, 2 }, Scenario { 1, true, true, 85, 3 },
                                         Scenario { 1, true, false, 86, 2 } }) {
                devpiano::midi::ChannelMatrix matrix;
                matrix.active = scenario.matrixActive;
                matrix.channels[0].outputChannel = 1;
                matrix.channels[0].transpose = 12;
                matrix.channels[0].followKey = false;
                matrix.channels[1].outputChannel = 2;
                matrix.channels[1].transpose = -3;
                devpiano::midi::MidiChannelMapper channelMapper(matrix, true, 2);
                KeyboardMidiMapper mapper;
                auto layout = devpiano::core::makeDefaultKeyboardLayout();
                layout.bindings = { devpiano::core::makeNoteBinding('A', 60) };
                layout.groups[1].transposeOffset = 2;
                layout.groups[1].octaveShift = 1;
                layout.groups[1].channel = 2;
                layout.activeGroupIndex = static_cast<uint8_t>(scenario.group);
                mapper.setLayout(layout);
                mapper.setChannelMapper(&channelMapper);
                mapper.setModifierState({ .altActive = scenario.alt });
                const auto snapshot = mapper.createQwertySnapshot(2);
                const auto& a = snapshot.rows[2].keys[1];
                expectEquals(a.mappedMidiNote, scenario.note);
                expectEquals(a.mappedMidiChannel, scenario.channel);
                juce::MidiKeyboardState state;
                CustomKeyboard piano(state);
                piano.setKeyboardLayout(snapshot);
                auto display = piano.getKeyboardSettings();
                display.keyWidth += 2.0f;
                piano.setKeyboardSettings(display);
                piano.setSize(1100, 220);
                piano.updateViewportBounds(1200, 180);
                for (const auto& key : piano.getKeys()) {
                    if (key.midiNote == scenario.note) {
                        expectEquals(key.keyLabel, juce::String("A"));
                    }
                }
                devpiano::ui::QwertyComponent qwerty;
                qwerty.setSize(750, 150);
                qwerty.updateViewModel(snapshot);
                const auto send = [&](int note, int channel, float velocity) {
                    return channelMapper.sendNoteOn(channel - 1, note, velocity, state);
                };
                const auto release = [&](const devpiano::core::MidiNoteIdentity& identity) {
                    channelMapper.sendNoteOff(identity, 1.0f, state);
                };
                piano.onNoteOn = send;
                qwerty.onNoteOn = send;
                piano.onNoteOff = release;
                qwerty.onNoteOff = release;
                const auto assertMidi = [&](int expectedNote) {
                    juce::MidiBuffer midi;
                    state.processNextMidiBuffer(midi, 0, 64, true);
                    int ons = 0;
                    for (const auto event : midi) {
                        const auto message = event.getMessage();
                        if (message.isNoteOn()) {
                            ++ons;
                            expectEquals(message.getNoteNumber(), expectedNote);
                            expectEquals(message.getChannel(), scenario.channel);
                        }
                    }
                    expectEquals(ons, 1);
                };
                const juce::ModifierKeys mods(scenario.alt ? juce::ModifierKeys::altModifier : 0);
                mapper.handleKeyPressed(juce::KeyPress('A', mods, 'a'), state);
                assertMidi(scenario.note);
                mapper.releaseAllHeldKeys(state);
                const auto pianoPress = pressAt(piano, pianoPosition(piano, scenario.note));
                piano.mouseDown(pianoPress);
                assertMidi(scenario.note);
                piano.mouseUp(pianoPress);
                piano.mouseDown(pianoPress);
                assertMidi(scenario.note);
                piano.mouseUp(pianoPress);
                state.noteOn(11, scenario.note, 0.5f);
                juce::MidiBuffer ignored;
                state.processNextMidiBuffer(ignored, 0, 64, true);
                piano.mouseDown(pianoPress);
                assertMidi(scenario.note);
                piano.mouseUp(pianoPress);
                state.noteOff(11, scenario.note, 1.0f);
                qwerty.mouseDown(pressAt(qwerty, qwertyPosition(qwerty)));
                assertMidi(scenario.note);
                qwerty.releaseHeldMouseNote();
                expect(!state.isNoteOn(scenario.channel, scenario.note));
                const auto unboundPress = pressAt(piano, pianoPosition(piano, scenario.note + 1));
                piano.mouseDown(unboundPress);
                assertMidi(scenario.note + 1);
                piano.mouseUp(unboundPress);
            }
        });
    }

    void testMutedMapConsumers() {
        testCase("silent bindings outrank Shift and matrix velocity on every input surface", [&] {
            devpiano::midi::ChannelMatrix matrix;
            matrix.channels[0].velocity = 127;
            devpiano::midi::MidiChannelMapper channelMapper(matrix, false, 0);
            KeyboardMidiMapper mapper;
            auto layout = devpiano::core::makeDefaultKeyboardLayout();
            layout.bindings = { devpiano::core::makeNoteBinding('A', 60, 1, 0.0f) };
            mapper.setLayout(layout);
            mapper.setChannelMapper(&channelMapper);
            mapper.setModifierState({ .shiftActive = true });
            const auto snapshot = mapper.createQwertySnapshot();
            juce::MidiKeyboardState state;
            CustomKeyboard piano(state);
            piano.setKeyboardLayout(snapshot);
            devpiano::ui::QwertyComponent qwerty;
            qwerty.setSize(750, 150);
            qwerty.updateViewModel(snapshot);
            const auto send = [&](int note, int channel, float velocity) {
                return channelMapper.sendNoteOn(channel - 1, note, velocity, state);
            };
            piano.onNoteOn = send;
            qwerty.onNoteOn = send;
            mapper.handleKeyPressed(juce::KeyPress('A', juce::ModifierKeys::shiftModifier, 'A'), state);
            piano.mouseDown(pressAt(piano, pianoPosition(piano, 60)));
            qwerty.mouseDown(pressAt(qwerty, qwertyPosition(qwerty)));
            juce::MidiBuffer midi;
            state.processNextMidiBuffer(midi, 0, 64, true);
            for (const auto event : midi) {
                expect(!event.getMessage().isNoteOn(), "no audible note may escape a silent binding");
            }
            expect(!state.isNoteOn(1, 60));
            mapper.releaseAllHeldKeys(state);
            piano.releaseHeldMouseNote();
            qwerty.releaseHeldMouseNote();
        });
    }

    void testLowestOctaveLabels() {
        testCase("lowest MIDI octave and solfege offsets agree across the C0 boundary", [&] {
            using namespace devpiano::core;
            expectEquals(getNoteDisplayName(0, NoteDisplayMode::noteName), juce::String("C-1"));
            expectEquals(getNoteDisplayName(1, NoteDisplayMode::noteName), juce::String("C#-1"));
            expectEquals(getNoteDisplayName(11, NoteDisplayMode::noteName), juce::String("B-1"));
            expectEquals(getNoteDisplayName(12, NoteDisplayMode::noteName), juce::String("C0"));
            expect(getNoteDisplayName(1, NoteDisplayMode::doReMi).endsWith("-5"));
            expect(getNoteDisplayName(11, NoteDisplayMode::fixedDo, 2).endsWith("-5"));
            expect(getNoteDisplayName(12, NoteDisplayMode::fixedDo, 2).endsWith("-4"));
        });
    }

    void testChordHudAndFadeout() {
        beginTest("QwertyComponent & KeyboardMidiMapper: Chord HUD and 300ms fadeout");

        KeyboardMidiMapper mapper;
        juce::MidiKeyboardState state;

        // Setup C major chord: C3(48), E3(52), G3(55)
        devpiano::core::KeyboardLayout layout;
        layout.bindings.push_back(devpiano::core::makeNoteBinding('Z', 48, 1, 0.8f));
        layout.bindings.push_back(devpiano::core::makeNoteBinding('X', 52, 1, 0.8f));
        layout.bindings.push_back(devpiano::core::makeNoteBinding('C', 55, 1, 0.8f));
        mapper.setLayout(layout);

        // Press Z, X, C
        mapper.handleKeyPressed(juce::KeyPress('z'), state);
        mapper.handleKeyPressed(juce::KeyPress('x'), state);
        mapper.handleKeyPressed(juce::KeyPress('c'), state);

        // Snapshot must have detected C Major chord
        const auto snapshot = mapper.createQwertySnapshot(0);
        expect(snapshot.detectedChord.isValid);
        expectEquals(snapshot.detectedChord.chordName, juce::String("C"));
        expectEquals(snapshot.detectedChord.rootPitchClass, 0);

        // Update QwertyComponent
        devpiano::ui::QwertyComponent comp;
        comp.setSize(700, 140);
        comp.updateViewModel(snapshot);
        expect(!comp.isTimerRunningForTest());

        expectEquals(comp.getLastDisplayedChord().chordName, juce::String("C"));
        expectEquals(comp.getChordFadeAlpha(), 1.0f);

        // Release all keys
        mapper.releaseAllHeldKeys(state);
        const auto releasedSnapshot = mapper.createQwertySnapshot(0);
        expect(!releasedSnapshot.detectedChord.isValid);

        comp.updateViewModel(releasedSnapshot);
        expect(comp.isTimerRunningForTest());

        // Trigger timer frames: alpha must decay smoothly
        float prevAlpha = comp.getChordFadeAlpha();
        for (int frame = 0; frame < 15; ++frame) {
            comp.triggerTimerForTest();
            const float curAlpha = comp.getChordFadeAlpha();
            expect(curAlpha <= prevAlpha);
            prevAlpha = curAlpha;
        }
        expect(comp.getChordFadeAlpha() <= 0.005f);

        // Eventually alpha fades to 0.0f
        for (int frame = 0; frame < 30; ++frame) {
            comp.triggerTimerForTest();
        }
        expectEquals(comp.getChordFadeAlpha(), 0.0f);
        expect(!comp.isTimerRunningForTest());
    }

    void testNumpadAndPartitionViewModelSnapshot() {
        beginTest("Snapshot reflects partition mode, numpad layout, and Sostenuto pedal");

        KeyboardMidiMapper mapper;
        mapper.setPartitionMode(devpiano::core::KeyboardPartitionMode::mainAndNumpad);
        mapper.setSostenutoPedalDown(true, false);

        const auto vm = mapper.createQwertySnapshot(0);
        expect(vm.showNumpad);
        expectEquals(static_cast<int>(vm.partitionMode),
                     static_cast<int>(devpiano::core::KeyboardPartitionMode::mainAndNumpad));
        expect(vm.isSostenutoPedalDown);
        expect(vm.isNumLockOn);

        // Verify numpadRows have the 15 keys
        int numpadKeyCount = 0;
        for (const auto& row : vm.numpadRows) {
            numpadKeyCount += static_cast<int>(row.keys.size());
        }
        expect(numpadKeyCount >= 15);

        // Switch to off mode -> showNumpad must be false
        mapper.setPartitionMode(devpiano::core::KeyboardPartitionMode::off);
        const auto vmOff = mapper.createQwertySnapshot(0);
        expect(!vmOff.showNumpad);
    }

    void testMouseInteractionPreservesPhysicalKeyIdentityAndZone() {
        beginTest("Mouse clicks on numpad preserve physical key identity and zone");

        KeyboardMidiMapper mapper;
        mapper.setPartitionMode(devpiano::core::KeyboardPartitionMode::mainAndNumpad);

        auto layout = devpiano::core::makeDefaultKeyboardLayout();
        layout.regionB.channel = 5;
        layout.regionB.transposeOffset = 12;
        mapper.setLayout(layout);

        devpiano::ui::QwertyComponent comp;
        comp.setSize(1000, 160);
        comp.updateViewModel(mapper.createQwertySnapshot(0));

        int receivedPhysicalKey = 0;
        int receivedNote = -1;
        int receivedChannel = -1;
        comp.onPhysicalNoteOn = [&](int physicalKeyCode, int note, int channel, float) {
            receivedPhysicalKey = physicalKeyCode;
            receivedNote = note;
            receivedChannel = channel;
            return devpiano::core::MidiNoteIdentity { devpiano::core::MidiNoteNumber::fromClamped(note),
                                                      devpiano::core::MidiChannel::fromClamped(channel) };
        };

        int releasedPhysicalKey = 0;
        int releasedNote = -1;
        int releasedChannel = -1;
        comp.onPhysicalNoteOff = [&](const devpiano::core::MidiNoteIdentity& identity, int physicalKeyCode) {
            releasedPhysicalKey = physicalKeyCode;
            releasedNote = identity.note.value;
            releasedChannel = identity.channel.value;
        };

        // Find Num 1 on numpad
        const auto& np3 = comp.getViewModel().numpadRows[3].keys;
        expect(np3.size() >= 3);
        expectEquals(np3[0].inputMidiNote, 72);
        expectEquals(np3[0].inputMidiChannel, 5);
        expectEquals(np3[0].keyCode, static_cast<int>(juce::KeyPress::numberPad1));

        // Find coordinate for Num 1 and dispatch actual mouseDown / mouseUp
        juce::Point<int> clickPos;
        bool found = false;
        for (int y = 20; y < 150 && !found; y += 10) {
            for (int x = 600; x < 990 && !found; x += 10) {
                const auto hit = comp.findKeyAt({ x, y });
                if (hit.key != nullptr && hit.key->keyCode == static_cast<int>(juce::KeyPress::numberPad1)) {
                    clickPos = { x, y };
                    found = true;
                }
            }
        }
        expect(found, "Num 1 must be positioned and findable in QwertyComponent");

        const juce::MouseEvent downEv(juce::Desktop::getInstance().getMainMouseSource(), clickPos.toFloat(),
                                      juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &comp, &comp,
                                      juce::Time::getCurrentTime(), clickPos.toFloat(), juce::Time::getCurrentTime(), 1,
                                      false);
        comp.mouseDown(downEv);
        expectEquals(receivedPhysicalKey, static_cast<int>(juce::KeyPress::numberPad1));
        expectEquals(receivedNote, 72);
        expectEquals(receivedChannel, 5);

        comp.mouseUp(downEv);
        expectEquals(releasedPhysicalKey, static_cast<int>(juce::KeyPress::numberPad1));
        expectEquals(releasedNote, 72);
        expectEquals(releasedChannel, 5);
    }

    void testNarrowWindowHorizontalScrolling() {
        beginTest("Narrow Window Enables Horizontal Scrolling For QwertyComponent");

        KeyboardMidiMapper mapper;
        mapper.setPartitionMode(devpiano::core::KeyboardPartitionMode::mainAndNumpad);

        devpiano::ui::QwertyComponent comp;
        // Very narrow width (500px) when numpad is active
        comp.setSize(500, 140);
        comp.updateViewModel(mapper.createQwertySnapshot(0));

        // Horizontal scroll via mouse wheel
        const juce::MouseEvent wheelEv(juce::Desktop::getInstance().getMainMouseSource(), juce::Point<float>(250, 70),
                                       juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &comp, &comp,
                                       juce::Time::getCurrentTime(), juce::Point<float>(250, 70),
                                       juce::Time::getCurrentTime(), 1, false);
        comp.mouseWheelMove(wheelEv, { 0.0f, -1.0f, false, false });
        // Component should still paint cleanly and find keys
        const auto hit = comp.findKeyAt(juce::Point<int>(20, 20));
        if (hit.key != nullptr) {
            expect(hit.rowIndex >= 0 && hit.rowIndex < 5);
        }
    }
};

QwertyViewModelTest qwertyViewModelTest;

} // namespace

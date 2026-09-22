#include <JuceHeader.h>

#include "Audio/SyncPedalProcessor.h"
#include "Core/KeyMapTypes.h"
#include "Input/KeyboardMidiMapper.h"

namespace {

class SyncPedalTest final : public juce::UnitTest {
public:
    SyncPedalTest()
        : juce::UnitTest("SyncPedal: Sample-Accurate Legato Scheduler", "DevPiano/Audio") {
    }

    void runTest() override {
        testNormalPolicyPassthrough();
        testSampleAccurateSyncPedalSequence();
        testPendingCutTriggerOnNextNote();
        testMultipleNotesInBlock();
        testKeyboardMidiMapperSustainPolicyIntegration();
    }

private:
    void testNormalPolicyPassthrough() {
        beginTest("Normal policy leaves MidiBuffer untouched");

        devpiano::audio::SyncPedalProcessor processor;
        processor.setPolicy(devpiano::core::SustainPolicy::normal);
        processor.setPedalDown(true);

        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 32);

        juce::MidiBuffer tempBuffer;
        tempBuffer.ensureSize(1024);

        processor.processMidiBlock(buffer, tempBuffer);

        expectEquals(buffer.getNumEvents(), 1);
        auto it = buffer.begin();
        expect((*it).getMessage().isNoteOn());
        expectEquals((*it).samplePosition, 32);
    }

    void testSampleAccurateSyncPedalSequence() {
        beginTest("Sync pedal emits CC64(0) -> NoteOn -> CC64(127) at identical sample offset");

        devpiano::audio::SyncPedalProcessor processor;
        processor.setPolicy(devpiano::core::SustainPolicy::syncPedal);
        processor.setPedalDown(true);

        juce::MidiBuffer buffer;
        const int targetSample = 64;
        buffer.addEvent(juce::MidiMessage::noteOn(1, 72, 0.9f), targetSample);

        juce::MidiBuffer tempBuffer;
        tempBuffer.ensureSize(1024);

        processor.processMidiBlock(buffer, tempBuffer);

        // Must produce exactly 3 events
        expectEquals(buffer.getNumEvents(), 3);

        auto it = buffer.begin();
        const auto ev1 = *it;
        ++it;
        const auto ev2 = *it;
        ++it;
        const auto ev3 = *it;

        // 1. CC64 = 0
        expect(ev1.getMessage().isController(), "Event 1 must be Controller");
        expectEquals(ev1.getMessage().getControllerNumber(), 64);
        expectEquals(ev1.getMessage().getControllerValue(), 0);
        expectEquals(ev1.samplePosition, targetSample, "Event 1 sample offset must be 64");

        // 2. NoteOn
        expect(ev2.getMessage().isNoteOn(), "Event 2 must be NoteOn");
        expectEquals(ev2.getMessage().getNoteNumber(), 72);
        expectEquals(ev2.samplePosition, targetSample, "Event 2 sample offset must be 64");

        // 3. CC64 = 127
        expect(ev3.getMessage().isController(), "Event 3 must be Controller");
        expectEquals(ev3.getMessage().getControllerNumber(), 64);
        expectEquals(ev3.getMessage().getControllerValue(), 127);
        expectEquals(ev3.samplePosition, targetSample, "Event 3 sample offset must be 64");
    }

    void testPendingCutTriggerOnNextNote() {
        beginTest("Pedal release sets pending cut; triggers on next NoteOn");

        devpiano::audio::SyncPedalProcessor processor;
        processor.setPolicy(devpiano::core::SustainPolicy::syncPedal);

        // Pedal pressed then released -> cutPending
        processor.setPedalDown(true);
        expect(!processor.isCutPending());
        processor.setPedalDown(false);
        expect(processor.isCutPending(), "Release in sync mode must hang cut");

        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOn(1, 64, 0.7f), 128);

        juce::MidiBuffer tempBuffer;
        tempBuffer.ensureSize(1024);

        processor.processMidiBlock(buffer, tempBuffer);

        // The sequence must fire at sample 128
        expectEquals(buffer.getNumEvents(), 3);
        expect(!processor.isCutPending(), "Pending cut must be cleared after execution");
    }

    void testMultipleNotesInBlock() {
        beginTest("Multiple notes within block each receive clean sync pedaling");

        devpiano::audio::SyncPedalProcessor processor;
        processor.setPolicy(devpiano::core::SustainPolicy::syncPedal);
        processor.setPedalDown(true);

        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 10);
        buffer.addEvent(juce::MidiMessage::noteOn(1, 64, 0.8f), 50);

        juce::MidiBuffer tempBuffer;
        tempBuffer.ensureSize(1024);

        processor.processMidiBlock(buffer, tempBuffer);

        // 2 notes -> 2 * 3 = 6 events
        expectEquals(buffer.getNumEvents(), 6);
    }

    void testKeyboardMidiMapperSustainPolicyIntegration() {
        beginTest("KeyboardMidiMapper reflects sustain policy and cut pending status");

        KeyboardMidiMapper mapper;
        expect(mapper.getSustainPolicy() == devpiano::core::SustainPolicy::normal);

        mapper.setSustainPolicy(devpiano::core::SustainPolicy::syncPedal);
        expect(mapper.getSustainPolicy() == devpiano::core::SustainPolicy::syncPedal);
        expect(!mapper.isSyncPedalCutPending());

        bool isSpaceDown = true;
        mapper.setKeyStatePredicate([&](int keyCode) { return (keyCode == juce::KeyPress::spaceKey) && isSpaceDown; });

        juce::MidiKeyboardState state;

        // 1. Press Space in sync mode
        mapper.handleKeyPressed(juce::KeyPress(juce::KeyPress::spaceKey), state);
        expect(mapper.isSustainPedalDown());
        expect(!mapper.isSyncPedalCutPending());

        // Snapshot reflects active pedal
        auto vm = mapper.createQwertySnapshot(0);
        expect(vm.isSustainPedalDown);
        expect(!vm.isSyncPedalCutPending);
        expect(vm.sustainPolicy == devpiano::core::SustainPolicy::syncPedal);

        // 2. Release Space in sync mode -> cutPending
        isSpaceDown = false;
        mapper.handleKeyStateChanged(state);
        expect(!mapper.isSustainPedalDown());
        expect(mapper.isSyncPedalCutPending(), "Space release must set cut pending");

        vm = mapper.createQwertySnapshot(0);
        expect(!vm.isSustainPedalDown);
        expect(vm.isSyncPedalCutPending);

        // 3. Reset to normal policy clears pending cut
        mapper.setSustainPolicy(devpiano::core::SustainPolicy::normal);
        expect(!mapper.isSyncPedalCutPending());
    }
};

static SyncPedalTest syncPedalTest;

} // namespace

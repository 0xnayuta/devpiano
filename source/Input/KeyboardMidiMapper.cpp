#include "KeyboardMidiMapper.h"
#include "Input/Win32NativeInputBridge.h"

#include "Midi/MidiChannelMapper.h"

using namespace devpiano::core;

KeyboardMidiMapper::KeyboardMidiMapper() {
    resetToDefaultLayout();
}

void KeyboardMidiMapper::setLayout(KeyboardLayout newLayout, bool notifyPerformance) {
    layout = std::move(newLayout);
    captureCurrentlyDownKeysBarrier();
    if (!notifyPerformance) {
        return;
    }

    if (sustainPedalDown) {
        sustainPedalDown = false;
        if (sustainPedalCallback) {
            sustainPedalCallback(false);
        }
    }
    if (sostenutoPedalDown) {
        sostenutoPedalDown = false;
        if (sostenutoPedalCallback) {
            sostenutoPedalCallback(false);
        }
    }
    physicalSoftPedalHeld = false;
    updateSoftPedalState();
}

void KeyboardMidiMapper::setLayoutDisplayName(juce::String newDisplayName) {
    layout.name = std::move(newDisplayName);
}

const KeyboardLayout& KeyboardMidiMapper::getLayout() const noexcept {
    return layout;
}

void KeyboardMidiMapper::setChannelMapper(devpiano::midi::MidiChannelMapper* mapper) noexcept {
    channelMapper = mapper;
}
void KeyboardMidiMapper::setSustainPedalCallback(SustainPedalCallback callback) noexcept {
    sustainPedalCallback = std::move(callback);
}
void KeyboardMidiMapper::setSyncPedalResetCallback(SyncPedalResetCallback callback) noexcept {
    syncPedalResetCallback = std::move(callback);
}

bool KeyboardMidiMapper::isSustainPedalDown() const noexcept {
    return sustainPedalDown;
}
void KeyboardMidiMapper::setSoftPedalCallback(SoftPedalCallback callback) noexcept {
    softPedalCallback = std::move(callback);
}

bool KeyboardMidiMapper::isSoftPedalDown() const noexcept {
    return softPedalDown;
}
void KeyboardMidiMapper::setSoftPedalDown(bool down, bool notifyPerformance) {
    programmaticSoftPedal = down;
    if (notifyPerformance) {
        updateSoftPedalState();
    } else {
        softPedalDown = physicalSoftPedalHeld || programmaticSoftPedal;
    }
}
void KeyboardMidiMapper::setPartitionMode(devpiano::core::KeyboardPartitionMode mode) {
    if (partitionMode == mode) {
        return;
    }
    partitionMode = mode;
    captureCurrentlyDownKeysBarrier();
}

devpiano::core::KeyboardPartitionMode KeyboardMidiMapper::getPartitionMode() const noexcept {
    return partitionMode;
}

void KeyboardMidiMapper::setSostenutoPedalCallback(SostenutoPedalCallback callback) noexcept {
    sostenutoPedalCallback = std::move(callback);
}

bool KeyboardMidiMapper::isSostenutoPedalDown() const noexcept {
    return sostenutoPedalDown;
}

void KeyboardMidiMapper::setSostenutoPedalDown(bool down, bool notifyPerformance) {
    if (sostenutoPedalDown == down) {
        return;
    }
    sostenutoPedalDown = down;
    if (notifyPerformance && sostenutoPedalCallback) {
        sostenutoPedalCallback(down);
    }
}

void KeyboardMidiMapper::setNumLockPredicate(NumLockPredicate predicate) noexcept {
    numLockPredicate = std::move(predicate);
}

bool KeyboardMidiMapper::isNumLockActive() const noexcept {
    if (numLockPredicate) {
        return numLockPredicate();
    }
#if JUCE_WINDOWS
    return (GetKeyState(VK_NUMLOCK) & 1) != 0;
#else
    return true;
#endif
}

void KeyboardMidiMapper::captureCurrentlyDownKeysBarrier() {
    std::erase_if(physicalKeysDownState, [this](int code) { return !isKeyCurrentlyDown(code); });
    for (const auto& binding : layout.bindings) {
        if (binding.keyCode != 0 && isKeyCurrentlyDown(binding.keyCode)) {
            physicalKeysDownState.insert(binding.keyCode);
        }
    }
}

void KeyboardMidiMapper::updateSoftPedalState() {
    const auto target = physicalSoftPedalHeld || programmaticSoftPedal;
    if (softPedalDown == target) {
        return;
    }
    softPedalDown = target;
    if (softPedalCallback != nullptr) {
        softPedalCallback(softPedalDown);
    }
}
void KeyboardMidiMapper::setTouchVelocityCurve(devpiano::input::TouchVelocityCurve curve) noexcept {
    touchVelocityCurve = curve;
}

devpiano::input::TouchVelocityCurve KeyboardMidiMapper::getTouchVelocityCurve() const noexcept {
    return touchVelocityCurve;
}
void KeyboardMidiMapper::resetToDefaultLayout() {
    setLayout(makeDefaultKeyboardLayout());
    captureCurrentlyDownKeysBarrier();
}

void KeyboardMidiMapper::setActiveGroupIndex(uint8_t groupIndex) {
    const auto target = static_cast<uint8_t>(groupIndex % 4);
    if (layout.activeGroupIndex == target) {
        return;
    }
    layout.activeGroupIndex = target;
    captureCurrentlyDownKeysBarrier();
    if (groupChangeCallback != nullptr) {
        groupChangeCallback(layout.activeGroupIndex);
    }
}

uint8_t KeyboardMidiMapper::getActiveGroupIndex() const noexcept {
    return layout.activeGroupIndex;
}

void KeyboardMidiMapper::switchToNextGroup() {
    setActiveGroupIndex((layout.activeGroupIndex + 1) % 4);
}

void KeyboardMidiMapper::switchToPreviousGroup() {
    setActiveGroupIndex((layout.activeGroupIndex + 3) % 4);
}

const devpiano::core::KeyGroup& KeyboardMidiMapper::getActiveGroup() const noexcept {
    return layout.getActiveGroup();
}

void KeyboardMidiMapper::setGroupChangeCallback(GroupChangeCallback callback) noexcept {
    groupChangeCallback = std::move(callback);
}

bool KeyboardMidiMapper::isKeyHeld(int keyCode) const noexcept {
    return findHeldKey(keyCode) != nullptr;
}

const devpiano::core::HeldKeyIdentity* KeyboardMidiMapper::findHeldKey(int keyCode) const noexcept {
    for (const auto& held : heldKeys) {
        if (held.physicalKeyCode == keyCode) {
            return &held;
        }
    }
    return nullptr;
}

size_t KeyboardMidiMapper::getNumHeldKeys() const noexcept {
    return heldKeys.size();
}
void KeyboardMidiMapper::setSustainPolicy(devpiano::core::SustainPolicy policy) noexcept {
    sustainPolicy = policy;
    if (policy == devpiano::core::SustainPolicy::normal) {
        syncPedalCutPending = false;
    }
}

devpiano::core::SustainPolicy KeyboardMidiMapper::getSustainPolicy() const noexcept {
    return sustainPolicy;
}

bool KeyboardMidiMapper::isSyncPedalCutPending() const noexcept {
    return syncPedalCutPending;
}
void KeyboardMidiMapper::setModifierState(devpiano::core::PerformanceModifierState state) noexcept {
    modifierState = state;
}

const devpiano::core::PerformanceModifierState& KeyboardMidiMapper::getModifierState() const noexcept {
    return modifierState;
}

void KeyboardMidiMapper::updateModifiersFromJuce(const juce::ModifierKeys& mods) noexcept {
    modifierState.shiftActive = mods.isShiftDown();
    modifierState.altActive = mods.isAltDown();
    modifierState.ctrlActive = mods.isCtrlDown();
}

bool KeyboardMidiMapper::handleKeyPressed(const juce::KeyPress& key, juce::MidiKeyboardState& keyboardState) {
    updateModifiersFromJuce(key.getModifiers());

    const auto isShift = key.getModifiers().isShiftDown();
    const auto isSpace = (key.getKeyCode() == juce::KeyPress::spaceKey || key.getTextCharacter() == ' ');
    const auto isTab = (key.getKeyCode() == juce::KeyPress::tabKey);

    if (isTab || (isShift && isSpace)) {
        physicalSoftPedalHeld = true;
        updateSoftPedalState();
        return true;
    }

    if (isSpace && !isShift) {
        syncPedalCutPending = false;
        if (!sustainPedalDown) {
            sustainPedalDown = true;
            if (sustainPedalCallback) {
                sustainPedalCallback(true);
            }
        }
        return true;
    }
    // CapsLock acts as Sostenuto pedal entry point when unbound (Task 38-2)
    if (key.getKeyCode() == kCapsLockKeyCode) {
        if (layout.findByKeyCode(kCapsLockKeyCode) == nullptr) {
            if (!sostenutoPedalDown) {
                sostenutoPedalDown = true;
                if (sostenutoPedalCallback) {
                    sostenutoPedalCallback(true);
                }
            }
            return true;
        }
    }

    // Backtick key shortcut for layout group switching (when unbound)
    if (key.getKeyCode() == '`' || key.getTextCharacter() == '`') {
        if (layout.findByKeyCode('`') == nullptr) {
            if (groupCycleShortcutHeld) {
                return true;
            }
            groupCycleShortcutHeld = true;
            switchToNextGroup();
            return true;
        }
    }

    const auto keyCode = normalisePhysicalKeyCode(key.getKeyCode());
    if (keyCode == 0) {
        return false;
    }

    const auto* binding = layout.findByKeyCode(keyCode);
    if (binding == nullptr) {
        return false;
    }

    if (isKeyHeld(keyCode)) {
        physicalKeysDownState.insert(keyCode);
        return true;
    }
    if (physicalKeysDownState.contains(keyCode)) {
        return true;
    }
    physicalKeysDownState.insert(keyCode);
    return triggerBinding(*binding, keyboardState);
}

bool KeyboardMidiMapper::handleModifierKeysChanged(const juce::ModifierKeys& modifiers,
                                                   juce::MidiKeyboardState& keyboardState) {
    updateModifiersFromJuce(modifiers);
    return processKeyStateChangedInternal(keyboardState, true);
}

bool KeyboardMidiMapper::handleKeyStateChanged(juce::MidiKeyboardState& keyboardState) {
    updateModifiersFromJuce(juce::ModifierKeys::getCurrentModifiers());
    return processKeyStateChangedInternal(keyboardState, true);
}

bool KeyboardMidiMapper::reconcileReleasedKeys(juce::MidiKeyboardState& keyboardState) {
    updateModifiersFromJuce(juce::ModifierKeys::getCurrentModifiers());
    return processKeyStateChangedInternal(keyboardState, false);
}

bool KeyboardMidiMapper::processKeyStateChangedInternal(juce::MidiKeyboardState& keyboardState, bool allowNoteOn) {
    auto consumed = false;

    const auto isSpaceDown = isKeyCurrentlyDown(juce::KeyPress::spaceKey);
    const auto isTabDown = isKeyCurrentlyDown(juce::KeyPress::tabKey);
    const auto isShiftDown = modifierState.shiftActive;
    // 0. backtick latch release: when the user lets go of ` the next
    // handleKeyPressed must re-fire switchToNextGroup() instead of being
    // suppressed by groupCycleShortcutHeld.
    if (groupCycleShortcutHeld && !isKeyCurrentlyDown('`')) {
        groupCycleShortcutHeld = false;
    }

    // 1. Soft pedal detection: Tab key or Shift+Space combination
    const auto physicalSoftActive = isTabDown || (isSpaceDown && isShiftDown);
    if (physicalSoftActive != physicalSoftPedalHeld && (allowNoteOn || !physicalSoftActive)) {
        physicalSoftPedalHeld = physicalSoftActive;
        updateSoftPedalState();
        consumed = true;
    }

    // 2. Sustain pedal detection: Space key without Shift
    const auto physicalSustainActive = isSpaceDown && !isShiftDown;
    if (allowNoteOn && physicalSustainActive && !sustainPedalDown) {
        sustainPedalDown = true;
        syncPedalCutPending = false;
        if (sustainPedalCallback) {
            sustainPedalCallback(true);
        }
        consumed = true;
    } else if (!physicalSustainActive && sustainPedalDown) {
        sustainPedalDown = false;
        if (sustainPolicy == devpiano::core::SustainPolicy::syncPedal) {
            syncPedalCutPending = true;
        }
        if (sustainPedalCallback) {
            sustainPedalCallback(false);
        }
        consumed = true;
    }
    // 3. Sostenuto pedal detection via CapsLock (kCapsLockKeyCode = 20)
    // Active only when CapsLock is not explicitly bound to a note (Task 38-2)
    if (layout.findByKeyCode(kCapsLockKeyCode) == nullptr) {
        const auto physicalSostenutoActive = isKeyCurrentlyDown(kCapsLockKeyCode);
        if (allowNoteOn && physicalSostenutoActive && !sostenutoPedalDown) {
            sostenutoPedalDown = true;
            if (sostenutoPedalCallback) {
                sostenutoPedalCallback(true);
            }
            consumed = true;
        } else if (!physicalSostenutoActive && sostenutoPedalDown) {
            sostenutoPedalDown = false;
            if (sostenutoPedalCallback) {
                sostenutoPedalCallback(false);
            }
            consumed = true;
        }
    }

    std::erase_if(physicalKeysDownState, [this](int code) { return !isKeyCurrentlyDown(code); });
    for (const auto& binding : layout.bindings) {
        const auto keyCode = binding.keyCode;
        if (keyCode == 0) {
            continue;
        }

        const auto isCurrentlyDown = isKeyCurrentlyDown(keyCode);
        const auto wasDownBefore = physicalKeysDownState.contains(keyCode);

        if (isCurrentlyDown && !wasDownBefore) {
            physicalKeysDownState.insert(keyCode);
            if (allowNoteOn && !isKeyHeld(keyCode)) {
                consumed = triggerBinding(binding, keyboardState) || consumed;
            }
        } else if (!isCurrentlyDown && wasDownBefore) {
            physicalKeysDownState.erase(keyCode);
        }
    }

    for (size_t i = 0; i < heldKeys.size(); ++i) {
        const auto& held = heldKeys[i];
        if (held.velocity <= 0.0f) {
            consumed = true;
            continue;
        }
        if (held.isMouseHeld) {
            continue;
        }
        if (isKeyCurrentlyDown(held.physicalKeyCode)) {
            continue;
        }

        bool hasOtherActiveHolder = false;
        for (size_t j = 0; j < heldKeys.size(); ++j) {
            if (j == i) {
                continue;
            }
            if (heldKeys[j].velocity > 0.0f && heldKeys[j].soundingMidiChannel == held.soundingMidiChannel
                && heldKeys[j].soundingMidiNote == held.soundingMidiNote
                && (heldKeys[j].isMouseHeld || isKeyCurrentlyDown(heldKeys[j].physicalKeyCode))) {
                hasOtherActiveHolder = true;
                break;
            }
        }

        if (!hasOtherActiveHolder) {
            bool alreadySentNoteOff = false;
            for (size_t k = 0; k < i; ++k) {
                if (heldKeys[k].velocity > 0.0f && !isKeyCurrentlyDown(heldKeys[k].physicalKeyCode)
                    && heldKeys[k].soundingMidiChannel == held.soundingMidiChannel
                    && heldKeys[k].soundingMidiNote == held.soundingMidiNote) {
                    alreadySentNoteOff = true;
                    break;
                }
            }

            if (!alreadySentNoteOff) {
                sendNoteOff(held.soundingMidiChannel, held.soundingMidiNote, held.velocity, keyboardState);
            }
        }
        consumed = true;
    }

    std::erase_if(heldKeys, [this](const auto& h) { return !h.isMouseHeld && !isKeyCurrentlyDown(h.physicalKeyCode); });

    return consumed;
}
void KeyboardMidiMapper::releaseAllHeldKeys(juce::MidiKeyboardState& keyboardState) {
    for (size_t i = 0; i < heldKeys.size(); ++i) {
        const auto& held = heldKeys[i];
        if (held.velocity <= 0.0f) {
            continue;
        }
        bool alreadySent = false;
        for (size_t j = 0; j < i; ++j) {
            if (heldKeys[j].velocity > 0.0f && heldKeys[j].soundingMidiChannel == held.soundingMidiChannel
                && heldKeys[j].soundingMidiNote == held.soundingMidiNote) {
                alreadySent = true;
                break;
            }
        }
        if (!alreadySent) {
            sendNoteOff(held.soundingMidiChannel, held.soundingMidiNote, held.velocity, keyboardState);
        }
    }
    heldKeys.clear();
    physicalKeysDownState.clear();
    if (sostenutoPedalDown) {
        sostenutoPedalDown = false;
        if (sostenutoPedalCallback) {
            sostenutoPedalCallback(false);
        }
    }
    if (sustainPedalDown) {
        sustainPedalDown = false;
        if (sustainPedalCallback) {
            sustainPedalCallback(false);
        }
    }
    syncPedalCutPending = false;
    physicalSoftPedalHeld = false;
    programmaticSoftPedal = false;
    groupCycleShortcutHeld = false;
    modifierState = {};
    updateSoftPedalState();
    if (syncPedalResetCallback) {
        syncPedalResetCallback();
    }
}

bool KeyboardMidiMapper::triggerBinding(const KeyBinding& binding, juce::MidiKeyboardState& keyboardState) {
    if (binding.action.trigger != KeyTrigger::keyDown) {
        return false;
    }

    if (binding.action.type != KeyActionType::note) {
        return false;
    }

    // 1. Validate physical partition region and numpad constraints (Task 38-3)
    const auto region = devpiano::core::getRegionForKeyCode(binding.keyCode, partitionMode);
    if (devpiano::core::isNumpadKeyCode(binding.keyCode)) {
        // Numpad sounds only in mainAndNumpad mode with NumLock enabled
        if (partitionMode != devpiano::core::KeyboardPartitionMode::mainAndNumpad) {
            return false;
        }
        if (!isNumLockActive()) {
            return false;
        }
    }

    const auto rawVelocity = binding.action.getVelocity().value;

    // 2. Compute physical region transformation (Task 38-3)
    int regionNote = binding.action.getMidiNoteNumber().value;
    int regionChannel = binding.action.getMidiChannel().value;
    if (region == devpiano::core::KeyboardRegion::regionA) {
        regionNote = devpiano::core::calculateSoundingNoteWithRegion(regionNote, layout.regionA);
        regionChannel = devpiano::core::calculateSoundingChannelWithRegion(regionChannel, layout.regionA);
    } else if (region == devpiano::core::KeyboardRegion::regionB) {
        regionNote = devpiano::core::calculateSoundingNoteWithRegion(regionNote, layout.regionB);
        regionChannel = devpiano::core::calculateSoundingChannelWithRegion(regionChannel, layout.regionB);
    }

    // 3. Compute active KeyGroup pitch and channel (Group channel overrides regions if 1..16)
    const auto baseSoundingNote = devpiano::core::calculateSoundingNote(regionNote, layout.getActiveGroup());
    const auto soundingChannel = devpiano::core::calculateSoundingChannel(regionChannel, layout.getActiveGroup());

    // 4. Typing cadence dynamics and humanizer estimation (Phase 35-B)
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const float dynamicVelocity = cadenceEstimator.estimateVelocity(now);
    float scaledVelocity = rawVelocity;
    if (rawVelocity > 0.0f && cadenceEstimator.isEnabled()) {
        const auto useDynamicVelocity = std::abs(rawVelocity - 1.0f) < 0.001f
            || std::abs(rawVelocity - devpiano::input::TypingCadenceEstimator::kDefaultBaseVelocity) < 0.01f;
        if (useDynamicVelocity) {
            scaledVelocity = dynamicVelocity;
        } else {
            scaledVelocity = std::clamp(
                dynamicVelocity * (rawVelocity / devpiano::input::TypingCadenceEstimator::kDefaultBaseVelocity),
                1.0f / 127.0f, 1.0f);
        }
    }

    const float jitteredVelocity
        = velocityHumanizer.applyHumanize(scaledVelocity, baseSoundingNote, ++keystrokeCounter);

    // 5. Transient modifiers and touch velocity curve transformation
    const auto soundingNote = modifierState.transformPitch(baseSoundingNote);
    const auto curveVelocity = devpiano::input::applyVelocityCurve(jitteredVelocity, touchVelocityCurve);
    const auto velocity = rawVelocity > 0.0f ? modifierState.transformVelocity(curveVelocity) : 0.0f;
    lastTriggeredVelocity = velocity;
    auto message = juce::MidiMessage::noteOn(soundingChannel, soundingNote, velocity);
    if (channelMapper != nullptr) {
        message = channelMapper->applyTransform(message);
    }
    const auto identity = MidiNoteIdentity { MidiNoteNumber::fromClamped(message.getNoteNumber()),
                                             MidiChannel::fromClamped(message.getChannel()) };
    const auto finalVelocity = message.getFloatVelocity();
    lastTriggeredVelocity = finalVelocity;
    if (finalVelocity > 0.0f) {
        if (notePreparationCallback) {
            notePreparationCallback(identity.channel.value);
        }
        keyboardState.noteOn(identity.channel.value, identity.note.value, finalVelocity);
    }

    heldKeys.push_back({ binding.keyCode, identity.note.value, identity.channel.value, finalVelocity });
    physicalKeysDownState.insert(binding.keyCode);

    // 6. Sync pedal cut flag: NoteOn consumes pending cut
    syncPedalCutPending = false;

    return true;
}

void KeyboardMidiMapper::sendNoteOff(int midiChannel, int midiNote, float velocity,
                                     juce::MidiKeyboardState& keyboardState) {
    keyboardState.noteOff(midiChannel, midiNote, velocity);
}

void KeyboardMidiMapper::setKeyStatePredicate(KeyStatePredicate predicate) noexcept {
    keyStatePredicate = std::move(predicate);
}

bool KeyboardMidiMapper::isKeyCurrentlyDown(int keyCode) const {
    // 注入的谓词优先；未注入时回退真实 OS 键盘状态（生产行为）。
    if (keyStatePredicate) {
        return keyStatePredicate(keyCode);
    }
    if (const auto state = devpiano::input::Win32NativeInputBridge::getPhysicalKeyState(keyCode)) {
        return *state;
    }
    return juce::KeyPress::isKeyCurrentlyDown(keyCode);
}

devpiano::core::QwertyViewModel KeyboardMidiMapper::createQwertySnapshot(int keySignature) const {
    auto vm = devpiano::core::makeDefaultQwertyLayoutTemplate();
    vm.isSustainPedalDown = sustainPedalDown;
    vm.isSoftPedalDown = softPedalDown;
    vm.isSyncPedalCutPending = syncPedalCutPending;
    vm.sustainPolicy = sustainPolicy;
    vm.activeGroupIndex = layout.activeGroupIndex;
    vm.activeGroupName = layout.getActiveGroup().name;
    vm.isShiftActive = modifierState.shiftActive;
    vm.isAltActive = modifierState.altActive;
    vm.isCtrlActive = modifierState.ctrlActive;
    vm.isSostenutoPedalDown = sostenutoPedalDown;
    vm.partitionMode = partitionMode;
    vm.showNumpad = (partitionMode == devpiano::core::KeyboardPartitionMode::mainAndNumpad);
    vm.isNumLockOn = isNumLockActive();

    const auto& activeGroup = layout.getActiveGroup();
    const auto projectNote = [this](int note, int channel, float velocity) {
        auto message = juce::MidiMessage::noteOn(channel, note, velocity);
        if (channelMapper != nullptr) {
            message = channelMapper->applyTransform(message);
        }
        return message;
    };
    const auto defaultInputChannel = calculateSoundingChannel(1, activeGroup);
    const auto defaultInputVelocity = modifierState.transformVelocity(1.0f);
    for (int note = 0; note < 128; ++note) {
        const auto inputNote = modifierState.transformPitch(calculateSoundingNote(note, activeGroup));
        const auto output = projectNote(inputNote, defaultInputChannel, defaultInputVelocity);
        auto& pianoKey = vm.pianoKeys[static_cast<std::size_t>(output.getNoteNumber())];
        if (pianoKey.inputMidiNote < 0) {
            pianoKey = { inputNote,
                         defaultInputChannel,
                         defaultInputVelocity,
                         output.getChannel(),
                         output.getFloatVelocity(),
                         note,
                         false,
                         {} };
        }
    }
    const auto projectBinding = [&](const KeyBinding& binding) {
        const auto bindingNote = binding.action.getMidiNoteNumber().value;
        const auto region = devpiano::core::getRegionForKeyCode(binding.keyCode, partitionMode);
        int regionNote = bindingNote;
        int regionChannel = binding.action.getMidiChannel().value;
        if (region == devpiano::core::KeyboardRegion::regionA) {
            regionNote = devpiano::core::calculateSoundingNoteWithRegion(regionNote, layout.regionA);
            regionChannel = devpiano::core::calculateSoundingChannelWithRegion(regionChannel, layout.regionA);
        } else if (region == devpiano::core::KeyboardRegion::regionB) {
            regionNote = devpiano::core::calculateSoundingNoteWithRegion(regionNote, layout.regionB);
            regionChannel = devpiano::core::calculateSoundingChannelWithRegion(regionChannel, layout.regionB);
        }
        const auto inputNote = modifierState.transformPitch(calculateSoundingNote(regionNote, activeGroup));
        const auto inputChannel = calculateSoundingChannel(regionChannel, activeGroup);
        const auto inputVelocity = modifierState.transformVelocity(
            devpiano::input::applyVelocityCurve(binding.action.getVelocity().value, touchVelocityCurve));
        const auto output = projectNote(inputNote, inputChannel, inputVelocity);
        return std::pair { PianoKeyVisualState { inputNote, inputChannel, inputVelocity, output.getChannel(),
                                                 output.getFloatVelocity(), bindingNote, true, binding.displayText,
                                                 binding.keyCode },
                           output.getNoteNumber() };
    };
    for (const auto& binding : layout.bindings) {
        const auto inactiveNumpad = isNumpadKeyCode(binding.keyCode)
            && (partitionMode != KeyboardPartitionMode::mainAndNumpad || !vm.isNumLockOn);
        if (binding.action.type != KeyActionType::note) {
            continue;
        }
        const auto [projection, outputNote] = projectBinding(binding);
        if (!inactiveNumpad && projection.velocity > 0.0f) {
            vm.outputChannelMask |= static_cast<std::uint16_t>(1U << (projection.mappedMidiChannel - 1));
        }
        if (!inactiveNumpad) {
            auto& pianoKey = vm.pianoKeys[static_cast<std::size_t>(outputNote)];
            if (!pianoKey.hasBinding) {
                pianoKey = projection;
            } else {
                pianoKey.keyLabel += "/" + binding.displayText;
            }
        }
        for (auto& row : vm.rows) {
            for (auto& key : row.keys) {
                if (key.keyCode == 0 || key.keyCode != binding.keyCode || key.bindingMidiNote >= 0) {
                    continue;
                }
                key.inputMidiNote = projection.inputMidiNote;
                key.inputMidiChannel = projection.inputMidiChannel;
                key.inputVelocity = projection.inputVelocity;
                key.bindingMidiNote = projection.bindingMidiNote;
                key.mappedMidiNote = outputNote;
                key.mappedMidiChannel = projection.mappedMidiChannel;
                key.velocity = projection.velocity;
                key.noteName = getNoteDisplayName(outputNote, NoteDisplayMode::noteName, keySignature);
                key.solfegeLabel = getNoteDisplayName(outputNote, NoteDisplayMode::fixedDo, keySignature);
            }
        }
        for (auto& row : vm.numpadRows) {
            for (auto& key : row.keys) {
                if (key.keyCode == 0 || key.keyCode != binding.keyCode || key.bindingMidiNote >= 0) {
                    continue;
                }
                key.inputMidiNote = projection.inputMidiNote;
                key.inputMidiChannel = projection.inputMidiChannel;
                key.inputVelocity = projection.inputVelocity;
                key.bindingMidiNote = projection.bindingMidiNote;
                key.mappedMidiNote = outputNote;
                key.mappedMidiChannel = projection.mappedMidiChannel;
                key.velocity = projection.velocity;
                key.region = devpiano::core::getRegionForKeyCode(binding.keyCode, partitionMode);
                key.noteName = getNoteDisplayName(outputNote, NoteDisplayMode::noteName, keySignature);
                key.solfegeLabel = getNoteDisplayName(outputNote, NoteDisplayMode::fixedDo, keySignature);
            }
        }
    }
    for (auto& row : vm.rows) {
        for (auto& key : row.keys) {
            if (key.keyCode == kCapsLockKeyCode) {
                key.isSostenutoPedal = layout.findByKeyCode(kCapsLockKeyCode) == nullptr;
            }
            key.region = getRegionForKeyCode(key.keyCode, partitionMode);
            if (key.isSustainPedal) {
                key.isDown = sustainPedalDown;
            } else if (key.isSoftPedal) {
                key.isDown = softPedalDown;
            } else if (key.isSostenutoPedal) {
                key.isDown = sostenutoPedalDown;
            } else if (key.keyCode != 0) {
                key.isDown = isKeyHeld(key.keyCode);
            }
        }
    }
    for (auto& row : vm.numpadRows) {
        for (auto& key : row.keys) {
            if (key.keyCode != 0) {
                key.isDown = isKeyHeld(key.keyCode);
            }
        }
    }

    std::vector<int> soundingNotes;
    soundingNotes.reserve(heldKeys.size());
    for (const auto& held : heldKeys) {
        if (held.velocity > 0.0f) {
            soundingNotes.push_back(held.soundingMidiNote);
        }
    }
    vm.detectedChord = devpiano::core::detectChord(soundingNotes);
    for (const auto& key : vm.pianoKeys) {
        if (key.inputMidiNote >= 0 && key.velocity > 0.0f) {
            vm.outputChannelMask |= static_cast<std::uint16_t>(1U << (key.mappedMidiChannel - 1));
        }
    }
    for (const auto& held : heldKeys) {
        if (held.velocity > 0.0f) {
            vm.outputChannelMask |= static_cast<std::uint16_t>(1U << (held.soundingMidiChannel - 1));
        }
    }
    return vm;
}

const devpiano::core::HeldKeyIdentity* KeyboardMidiMapper::getHeldKeyByIndex(size_t index) const noexcept {
    if (index < heldKeys.size()) {
        return &heldKeys[index];
    }
    return nullptr;
}

devpiano::core::MidiNoteIdentity KeyboardMidiMapper::triggerMouseKeyDown(int physicalKeyCode, int inputNote,
                                                                         int inputChannel, float velocity,
                                                                         juce::MidiKeyboardState& keyboardState) {
    if (physicalKeyCode != 0) {
        for (auto& held : heldKeys) {
            if (held.physicalKeyCode == physicalKeyCode) {
                held.isMouseHeld = true;
                return { MidiNoteNumber::fromClamped(held.soundingMidiNote),
                         MidiChannel::fromClamped(held.soundingMidiChannel) };
            }
        }
    }
    auto identity = devpiano::core::MidiNoteIdentity { devpiano::core::MidiNoteNumber::fromClamped(inputNote),
                                                       devpiano::core::MidiChannel::fromClamped(inputChannel) };
    if (isNumpadKeyCode(physicalKeyCode)
        && (partitionMode != KeyboardPartitionMode::mainAndNumpad || !isNumLockActive())) {
        return identity;
    }
    auto message = juce::MidiMessage::noteOn(identity.channel.value, identity.note.value, velocity);
    if (channelMapper != nullptr) {
        message = channelMapper->applyTransform(message);
    }
    identity = { MidiNoteNumber::fromClamped(message.getNoteNumber()), MidiChannel::fromClamped(message.getChannel()) };
    const auto finalVelocity = message.getFloatVelocity();
    if (finalVelocity > 0.0f) {
        if (notePreparationCallback) {
            notePreparationCallback(identity.channel.value);
        }
        keyboardState.noteOn(identity.channel.value, identity.note.value, finalVelocity);
    }

    bool found = false;
    for (auto& held : heldKeys) {
        if (held.physicalKeyCode == physicalKeyCode && held.soundingMidiNote == identity.note.value
            && held.soundingMidiChannel == identity.channel.value) {
            held.isMouseHeld = true;
            found = true;
            break;
        }
    }
    if (!found) {
        heldKeys.push_back({ physicalKeyCode, identity.note.value, identity.channel.value, finalVelocity, true });
    }

    syncPedalCutPending = false;
    return identity;
}

void KeyboardMidiMapper::releaseMouseKeyUp(const devpiano::core::MidiNoteIdentity& identity, int physicalKeyCode,
                                           juce::MidiKeyboardState& keyboardState) {
    for (auto it = heldKeys.begin(); it != heldKeys.end(); ++it) {
        if (it->isMouseHeld && it->soundingMidiNote == identity.note.value
            && it->soundingMidiChannel == identity.channel.value && it->physicalKeyCode == physicalKeyCode) {
            it->isMouseHeld = false;
            if (it->physicalKeyCode != 0 && isKeyCurrentlyDown(it->physicalKeyCode)) {
                return;
            }
            bool hasOtherHolder = false;
            for (const auto& other : heldKeys) {
                if (&other != &(*it) && other.velocity > 0.0f && other.soundingMidiNote == identity.note.value
                    && other.soundingMidiChannel == identity.channel.value) {
                    if (other.isMouseHeld
                        || (other.physicalKeyCode != 0 && isKeyCurrentlyDown(other.physicalKeyCode))) {
                        hasOtherHolder = true;
                        break;
                    }
                }
            }
            if (!hasOtherHolder && it->velocity > 0.0f) {
                sendNoteOff(identity.channel.value, identity.note.value, 1.0f, keyboardState);
            }
            heldKeys.erase(it);
            return;
        }
    }
}

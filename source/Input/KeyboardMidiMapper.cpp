#include "KeyboardMidiMapper.h"

#include "Midi/MidiChannelMapper.h"

using namespace devpiano::core;

KeyboardMidiMapper::KeyboardMidiMapper() {
    resetToDefaultLayout();
}

void KeyboardMidiMapper::setLayout(KeyboardLayout newLayout, bool notifyPerformance) {
    layout = std::move(newLayout);
    if (!notifyPerformance) {
        return;
    }

    if (sustainPedalDown) {
        sustainPedalDown = false;
        if (sustainPedalCallback) {
            sustainPedalCallback(false);
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
}

void KeyboardMidiMapper::setActiveGroupIndex(uint8_t groupIndex) {
    const auto target = static_cast<uint8_t>(groupIndex % 4);
    if (layout.activeGroupIndex == target) {
        return;
    }
    layout.activeGroupIndex = target;
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
    // 支持反引号 ` 键作为快捷切组键（在未绑定音符时有效）
    if (key.getKeyCode() == '`' || key.getTextCharacter() == '`') {
        if (layout.findByKeyCode('`') == nullptr) {
            // 抑制 OS 自动重复：仅在按键 down-edge 切换组，重复触发被吞掉。
            if (groupCycleShortcutHeld) {
                return true;
            }
            groupCycleShortcutHeld = true;
            switchToNextGroup();
            return true;
        }
    }

    const auto keyCode = normaliseKeyCode(key);
    if (keyCode == 0) {
        return false;
    }

    const auto* binding = layout.findByKeyCode(keyCode);
    if (binding == nullptr) {
        return false;
    }

    if (isKeyHeld(keyCode)) {
        return true;
    }

    return triggerBinding(*binding, keyboardState);
}

bool KeyboardMidiMapper::handleModifierKeysChanged(const juce::ModifierKeys& modifiers,
                                                   juce::MidiKeyboardState& keyboardState) {
    updateModifiersFromJuce(modifiers);
    return processKeyStateChangedInternal(keyboardState);
}

bool KeyboardMidiMapper::handleKeyStateChanged(juce::MidiKeyboardState& keyboardState) {
    updateModifiersFromJuce(juce::ModifierKeys::getCurrentModifiers());
    return processKeyStateChangedInternal(keyboardState);
}

bool KeyboardMidiMapper::processKeyStateChangedInternal(juce::MidiKeyboardState& keyboardState) {
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

    // 1. 物理软踏板检测: Tab 键或 Shift+Space 组合
    const auto physicalSoftActive = isTabDown || (isSpaceDown && isShiftDown);
    if (physicalSoftActive != physicalSoftPedalHeld) {
        physicalSoftPedalHeld = physicalSoftActive;
        updateSoftPedalState();
        consumed = true;
    }

    // 2. 物理延音踏板检测: 仅当 Space 按下且未按住 Shift 时激活，避免 Shift+Space 误激活延音
    const auto physicalSustainActive = isSpaceDown && !isShiftDown;
    if (physicalSustainActive && !sustainPedalDown) {
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
    for (const auto& binding : layout.bindings) {
        const auto keyCode = binding.keyCode;
        if (keyCode == 0) {
            continue;
        }

        const auto isCurrentlyDown = isKeyCurrentlyDown(keyCode);
        const auto wasHeld = isKeyHeld(keyCode);

        if (isCurrentlyDown && !wasHeld) {
            consumed = triggerBinding(binding, keyboardState) || consumed;
        }
    }

    for (size_t i = 0; i < heldKeys.size(); ++i) {
        const auto& held = heldKeys[i];
        if (held.velocity <= 0.0f) {
            consumed = true;
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
                && isKeyCurrentlyDown(heldKeys[j].physicalKeyCode)) {
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

    std::erase_if(heldKeys, [this](const auto& h) { return !isKeyCurrentlyDown(h.physicalKeyCode); });

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

int KeyboardMidiMapper::normaliseKeyCode(const juce::KeyPress& key) const {
    return normaliseAlphaNumericKeyCode(key.getKeyCode());
}

bool KeyboardMidiMapper::triggerBinding(const KeyBinding& binding, juce::MidiKeyboardState& keyboardState) {
    if (binding.action.trigger != KeyTrigger::keyDown) {
        return false;
    }

    if (binding.action.type != KeyActionType::note) {
        return false;
    }

    const auto rawVelocity = binding.action.getVelocity().value;

    // 1. 计算当前激活 Group 下的发声音高与通道
    const auto baseSoundingNote
        = devpiano::core::calculateSoundingNote(binding.action.getMidiNoteNumber().value, layout.getActiveGroup());
    const auto soundingChannel
        = devpiano::core::calculateSoundingChannel(binding.action.getMidiChannel().value, layout.getActiveGroup());

    // 2. 打字律动力度与人性化微扰估算 (Phase 35-B: Typing Cadence Dynamics & Humanizer)
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

    // 3. 瞬态修饰符与手感曲线事件流变换
    const auto soundingNote = modifierState.transformPitch(baseSoundingNote);
    const auto curveVelocity = devpiano::input::applyVelocityCurve(jitteredVelocity, touchVelocityCurve);
    const auto velocity = rawVelocity > 0.0f ? modifierState.transformVelocity(curveVelocity) : 0.0f;
    lastTriggeredVelocity = velocity;
    auto identity
        = MidiNoteIdentity { MidiNoteNumber::fromClamped(soundingNote), MidiChannel::fromClamped(soundingChannel) };
    if (channelMapper != nullptr) {
        identity = channelMapper->sendNoteOn(MidiChannel::fromClamped(soundingChannel).toZeroBased(), soundingNote,
                                             velocity, keyboardState);
    } else if (velocity > 0.0f) {
        keyboardState.noteOn(identity.channel.value, identity.note.value, velocity);
    }

    heldKeys.push_back({ binding.keyCode, identity.note.value, identity.channel.value, velocity });

    // 4. 同步切分标记：NoteOn 即消费一次未决的 sync-pedal cut，
    // 与 SyncPedalProcessor::processMidiBlock 在音频线程上同样清零
    // cutPending 的语义保持一致，避免 QWERTY 卡片长期高亮 "[Sync Cut]"。
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
        const auto inputNote = modifierState.transformPitch(calculateSoundingNote(bindingNote, activeGroup));
        const auto inputChannel = calculateSoundingChannel(binding.action.getMidiChannel().value, activeGroup);
        const auto inputVelocity = modifierState.transformVelocity(
            devpiano::input::applyVelocityCurve(binding.action.getVelocity().value, touchVelocityCurve));
        const auto output = projectNote(inputNote, inputChannel, inputVelocity);
        return std::pair { PianoKeyVisualState { inputNote, inputChannel, inputVelocity, output.getChannel(),
                                                 output.getFloatVelocity(), bindingNote, true, binding.displayText },
                           output.getNoteNumber() };
    };
    for (const auto& binding : layout.bindings) {
        if (binding.action.type != KeyActionType::note) {
            continue;
        }
        const auto [projection, outputNote] = projectBinding(binding);
        auto& pianoKey = vm.pianoKeys[static_cast<std::size_t>(outputNote)];
        if (!pianoKey.hasBinding) {
            pianoKey = projection;
        } else {
            pianoKey.keyLabel += "/" + binding.displayText;
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
    }
    for (auto& row : vm.rows) {
        for (auto& key : row.keys) {
            if (key.isSustainPedal) {
                key.isDown = sustainPedalDown;
            } else if (key.isSoftPedal) {
                key.isDown = softPedalDown;
            } else if (key.keyCode != 0) {
                key.isDown = isKeyHeld(key.keyCode);
            }
        }
    }

    std::vector<int> soundingNotes;
    soundingNotes.reserve(heldKeys.size());
    for (const auto& held : heldKeys) {
        soundingNotes.push_back(held.soundingMidiNote);
    }
    vm.detectedChord = devpiano::core::detectChord(soundingNotes);

    return vm;
}

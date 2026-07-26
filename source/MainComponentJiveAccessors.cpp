
// ═══════════════════════════════════════════════════════════════════════════
// JIVE component accessors (Phase 11d)
// ═══════════════════════════════════════════════════════════════════════════

namespace {
template <typename T> T* jc(jive::GuiItem& root, const juce::String& id) {
    return dynamic_cast<T*>(root.getComponent()->findChildWithID(id));
}
} // namespace

CustomKeyboard& MainComponent::getCustomKeyboard() {
    if (auto* ck = jc<CustomKeyboard>(*jiveRootItem, "custom-keyboard"))
        return *ck;
    auto* vp = jc<juce::Viewport>(*jiveRootItem, "custom-keyboard");
    if (vp != nullptr)
        if (auto* ck = dynamic_cast<CustomKeyboard*>(vp->getViewedComponent()))
            return *ck;
    jassertfalse;
    static CustomKeyboard dummy(audioEngine.getKeyboardState());
    return dummy;
}

void MainComponent::setCustomKeyboardLayout(const devpiano::core::KeyboardLayout& layout) {
    getCustomKeyboard().setKeyboardLayout(layout);
}

void MainComponent::setCustomKeyboardSettings(const devpiano::ui::KeyboardSettings& ks) {
    getCustomKeyboard().setKeyboardSettings(ks);
}

void MainComponent::setKeyboardViewPosition(int midiNote, int pixelOffset) {
    auto* vp = jc<juce::Viewport>(*jiveRootItem, "custom-keyboard");
    if (vp == nullptr)
        return;
    auto* ck = dynamic_cast<CustomKeyboard*>(vp->getViewedComponent());
    if (ck == nullptr)
        return;
    if (pixelOffset >= 0) {
        vp->setViewPosition(pixelOffset, 0);
    } else if (midiNote >= 0 && midiNote <= 127) {
        int whiteCount = 0;
        for (int n = 0; n < midiNote; ++n)
            if (devpiano::ui::isWhiteKey(n))
                ++whiteCount;
        auto x = static_cast<int>(whiteCount * ck->getKeyboardSettings().keyWidth);
        vp->setViewPosition(x, 0);
    }
}

int MainComponent::getKeyboardViewPositionX() const {
    auto* vp = dynamic_cast<juce::Viewport*>(jiveRootItem->getComponent()->findChildWithID("custom-keyboard"));
    return vp != nullptr ? vp->getViewPositionX() : 0;
}

juce::String MainComponent::getPluginPanelPath() const {
    if (auto* ed = jc<juce::TextEditor>(*jiveRootItem, "plugin-path-editor"))
        return ed->getText();
    return {};
}

void MainComponent::setPluginPanelPath(const juce::String& path) {
    if (auto* ed = jc<juce::TextEditor>(*jiveRootItem, "plugin-path-editor"))
        ed->setText(path);
}

juce::String MainComponent::getSelectedPluginName() const {
    if (auto* cb = jc<juce::ComboBox>(*jiveRootItem, "plugin-selector"))
        return cb->getText();
    return {};
}

void MainComponent::setInstrumentFilterVisible(bool visible) {
    // JIVE layout does not currently have an instrument filter ComboBox;
    // when added, wire it here via findChildWithID("instrument-filter").
    juce::ignoreUnused(visible);
}

void MainComponent::updatePluginPanelState(const PluginPanelState& state) {
    if (auto* cb = jc<juce::ComboBox>(*jiveRootItem, "plugin-selector")) {
        cb->clear();
        int id = 1;
        for (const auto& name : state.availablePluginNames)
            cb->addItem(name, id++);
        if (state.preferredSelection.isNotEmpty())
            cb->setText(state.preferredSelection);
        cb->setEnabled(!state.isCurrentlyScanning);
    }
}

void MainComponent::setPluginPanelExpanded(bool expanded) {
    if (auto* area = jc<juce::Component>(*jiveRootItem, "plugin-expanded-area"))
        area->setVisible(expanded);
}

bool MainComponent::isPluginPanelExpanded() const {
    if (auto* area = jc<juce::Component>(*jiveRootItem, "plugin-expanded-area"))
        return area->isVisible();
    return false;
}

void MainComponent::setControlsValues(float gain, float a, float d, float s, float r) {
    auto sk = [&](const juce::String& id, float v) {
        if (auto* sl = jc<juce::Slider>(*jiveRootItem, id))
            sl->setValue(v, juce::dontSendNotification);
    };
    sk("volume-knob", gain);
    sk("attack-knob", a);
    sk("decay-knob", d);
    sk("sustain-knob", s);
    sk("release-knob", r);
    adsrCurve.setParameters(a, d, s, r);
}

float MainComponent::getControlsMasterGain() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "volume-knob");
    return s ? (float)s->getValue() : 0.8f;
}
float MainComponent::getControlsAttack() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "attack-knob");
    return s ? (float)s->getValue() : 0.1f;
}
float MainComponent::getControlsDecay() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "decay-knob");
    return s ? (float)s->getValue() : 0.3f;
}
float MainComponent::getControlsSustain() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "sustain-knob");
    return s ? (float)s->getValue() : 0.7f;
}
float MainComponent::getControlsRelease() const {
    auto* s = jc<juce::Slider>(*jiveRootItem, "release-knob");
    return s ? (float)s->getValue() : 0.5f;
}

void MainComponent::setControlsPresets(const juce::StringArray& ids, const juce::String& current,
                                       const juce::StringArray& names) {
    if (auto* cb = jc<juce::ComboBox>(*jiveRootItem, "preset-combo")) {
        cb->clear();
        for (int i = 0; i < ids.size() && i < names.size(); ++i)
            cb->addItem(names[i], i + 1);
        if (current.isNotEmpty()) {
            auto idx = ids.indexOf(current);
            if (idx >= 0)
                cb->setSelectedItemIndex(idx, juce::dontSendNotification);
        }
    }
}

juce::String MainComponent::getControlsSelectedPresetId() const {
    return {};
}

void MainComponent::setControlsRecordingState(RecordingControlsState state) {
    auto se = [&](const juce::String& id, bool en) {
        if (auto* btn = jc<juce::Button>(*jiveRootItem, id))
            btn->setEnabled(en);
    };
    se("record-btn", state.state != RecordingState::playing);
    se("play-btn", state.hasTake && state.state != RecordingState::recording);
    se("stop-btn", state.state != RecordingState::idle);
    se("back-btn", state.hasTake);
    se("export-midi-btn", state.canExportMidiTake && state.state == RecordingState::idle);
    se("export-wav-btn", state.canExportWavTake && state.state == RecordingState::idle);
}

void MainComponent::setControlsPlaybackSpeed(double speed) {
    if (auto* sl = jc<juce::Slider>(*jiveRootItem, "speed-knob"))
        sl->setValue(juce::jlimit(0.5, 2.0, speed), juce::dontSendNotification);
}

juce::Rectangle<int> MainComponent::getRecentFilesButtonScreenBounds() const {
    if (auto* btn = jc<juce::Button>(*jiveRootItem, "recent-btn"))
        return btn->getScreenBounds();
    return {};
}

void MainComponent::setStatusPluginName(const juce::String& name) {
    if (auto* lbl = jc<juce::Label>(*jiveRootItem, "plugin-name-label"))
        lbl->setText(name, juce::dontSendNotification);
}
void MainComponent::setStatusAudioInfo(const juce::String& info) {
    if (auto* lbl = jc<juce::Label>(*jiveRootItem, "audio-info-label"))
        lbl->setText(info, juce::dontSendNotification);
}
void MainComponent::setStatusTimeDisplay(const juce::String& time) {
    if (auto* lbl = jc<juce::Label>(*jiveRootItem, "time-label"))
        lbl->setText(time, juce::dontSendNotification);
}
void MainComponent::setStatusMidiActivity(bool /*active*/) {
    if (jiveRootItem == nullptr)
        return;
    if (auto* dot = dynamic_cast<juce::Component*>(jiveRootItem->getComponent()->findChildWithID("midi-dot")))
        dot->repaint();
}

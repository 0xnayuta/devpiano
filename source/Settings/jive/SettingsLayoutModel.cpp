#include "Settings/jive/SettingsLayoutModel.h"

#include "Locale/LocaleManager.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/JiveBuilderHelpers.h"

namespace devpiano::ui::jive {

// ============================================================================
// Section Builders
// ============================================================================

juce::ValueTree makeAudioDeviceSectionTree() {
    auto card = flexColumn("audio-device-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);
    auto title = text(TRANS("Audio Device"), "audio-device-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("audio-device-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    // Row 1: Audio Device Type (ComboBox)
    auto typeCombo = node("ComboBox", "audio-device-type-combo");
    typeCombo.setProperty("width", 300, nullptr);
    typeCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Audio Device Type:"), typeCombo, "audio-device-type-label"), nullptr);

    // Row 2: Output Device (ComboBox + Test Button)
    auto outputRow = flexRow("audio-output-row");
    outputRow.setProperty("height", 28, nullptr);
    outputRow.setProperty("margin", "0 0 6 0", nullptr);

    auto outputLbl = text(TRANS("Output Device:"), "audio-output-label");
    outputLbl.setProperty("flex-grow", 1.0, nullptr);
    outputLbl.setProperty("height", 22, nullptr);
    outputLbl.setProperty("font-size", 14, nullptr);
    outputLbl.setProperty("justification", "centred-left", nullptr);
    outputRow.appendChild(outputLbl, nullptr);

    auto outputControls = flexRow("audio-output-controls");
    outputControls.setProperty("width", 300, nullptr);

    auto outputCombo = node("ComboBox", "audio-output-device-combo");
    outputCombo.setProperty("width", 236, nullptr);
    outputCombo.setProperty("height", 24, nullptr);
    outputCombo.setProperty("margin", "0 8 0 0", nullptr);
    outputControls.appendChild(outputCombo, nullptr);

    auto testBtn = button(TRANS("Test"), "audio-test-button");
    testBtn.setProperty("width", 56, nullptr);
    testBtn.setProperty("height", 24, nullptr);
    outputControls.appendChild(testBtn, nullptr);
    outputRow.appendChild(outputControls, nullptr);
    content.appendChild(outputRow, nullptr);
    // Row 3: Active Output Channels (ComboBox)
    auto channelsCombo = node("ComboBox", "audio-active-channels-combo");
    channelsCombo.setProperty("width", 300, nullptr);
    channelsCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Active output channels:"), channelsCombo, "audio-active-channels-label"),
                        nullptr);

    // Row 4: Sample Rate (ComboBox)
    auto sampleRateCombo = node("ComboBox", "audio-sample-rate-combo");
    sampleRateCombo.setProperty("width", 300, nullptr);
    sampleRateCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Sample Rate:"), sampleRateCombo, "audio-sample-rate-label"), nullptr);

    // Row 5: Buffer Size (ComboBox)
    auto bufferSizeCombo = node("ComboBox", "audio-buffer-size-combo");
    bufferSizeCombo.setProperty("width", 300, nullptr);
    bufferSizeCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Audio Buffer Size:"), bufferSizeCombo, "audio-buffer-size-label"), nullptr);

    // Row 6: ASIO Control Panel (optional, collapsed when not in ASIO mode)
    auto asioRow = flexRow("asio-control-panel-row");
    asioRow.setProperty("height", 0, nullptr);
    asioRow.setProperty("max-height", 0, nullptr);
    asioRow.setProperty("margin", "0 0 0 0", nullptr);
    asioRow.setProperty("visibility", false, nullptr);
    auto asioLbl = text(TRANS("Device Control Panel:"), "asio-control-panel-label");
    asioLbl.setProperty("flex-grow", 1.0, nullptr);
    asioLbl.setProperty("height", 22, nullptr);
    asioLbl.setProperty("font-size", 14, nullptr);
    asioLbl.setProperty("justification", "centred-left", nullptr);
    asioRow.appendChild(asioLbl, nullptr);

    auto asioBtn = button(TRANS("Open Control Panel"), "asio-control-panel-button");
    asioBtn.setProperty("width", 300, nullptr);
    asioBtn.setProperty("height", 24, nullptr);
    asioRow.appendChild(asioBtn, nullptr);

    content.appendChild(asioRow, nullptr);

    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeKeySignatureSectionTree() {
    auto card = flexColumn("key-sig-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Key Signature"), "key-sig-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("key-sig-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    // Row 1: Key Signature combo
    auto ksCombo = node("ComboBox", "key-signature-combo");
    ksCombo.setProperty("width", 300, nullptr);
    ksCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Key Signature:"), ksCombo, "key-signature-label"), nullptr);

    // Row 2: MIDI Transpose toggle
    auto transposeToggle = node("Checkbox", "midi-transpose-toggle");
    transposeToggle.setProperty("text", TRANS("MIDI Transpose"), nullptr);
    transposeToggle.setProperty("toggleable", true, nullptr);
    transposeToggle.setProperty("toggle-on-click", true, nullptr);
    transposeToggle.setProperty("width", 300, nullptr);
    transposeToggle.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("MIDI Transpose:"), transposeToggle, "midi-transpose-label"), nullptr);
    // Row 3: Channel Follow Key (16-channel Grid layout)
    auto followKeyArea = flexColumn("channel-follow-key-area");
    followKeyArea.setProperty("margin", "6 0 0 0", nullptr);

    auto followKeyLbl = text(TRANS("Channel Follow Key:"), "channel-follow-key-label");
    followKeyLbl.setProperty("height", 20, nullptr);
    followKeyLbl.setProperty("margin", "0 0 4 0", nullptr);
    followKeyLbl.setProperty("font-size", 14, nullptr);
    followKeyArea.appendChild(followKeyLbl, nullptr);

    auto grid = node("Component", "follow-key-grid");
    grid.setProperty("display", "grid", nullptr);
    grid.setProperty("grid-template-columns", "1fr 1fr 1fr 1fr 1fr 1fr 1fr 1fr", nullptr);
    grid.setProperty("gap", "4", nullptr);
    grid.setProperty("height", 52, nullptr);

    for (int ch = 0; ch < 16; ++ch) {
        auto cb = node("Checkbox", "follow-key-" + juce::String(ch));
        cb.setProperty("text", "Ch" + juce::String(ch + 1), nullptr);
        cb.setProperty("title", TRANS("Follow Key"), nullptr);
        cb.setProperty("toggleable", true, nullptr);
        cb.setProperty("toggle-on-click", true, nullptr);
        cb.setProperty("height", 24, nullptr);
        grid.appendChild(cb, nullptr);
    }
    followKeyArea.appendChild(grid, nullptr);
    content.appendChild(followKeyArea, nullptr);
    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeKeyboardDisplaySectionTree() {
    auto card = flexColumn("keyboard-display-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Keyboard Display"), "keyboard-display-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("keyboard-display-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    // Colour Mode
    auto colourCombo = node("ComboBox", "colour-mode-combo");
    colourCombo.setProperty("width", 300, nullptr);
    colourCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Colour Mode:"), colourCombo, "colour-mode-label"), nullptr);

    // Note Display
    auto noteCombo = node("ComboBox", "note-display-combo");
    noteCombo.setProperty("width", 300, nullptr);
    noteCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Note Display:"), noteCombo, "note-display-label"), nullptr);

    // Fade Speed
    auto fadeSlider = node("Slider", "fade-speed-slider");
    fadeSlider.setProperty("width", 300, nullptr);
    fadeSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Fade Speed:"), fadeSlider, "fade-speed-label"), nullptr);

    // Instrument Filter
    auto filterCb = node("Checkbox", "instrument-filter-toggle");
    filterCb.setProperty("text", TRANS("Show MIDI/VSTi Instrument Filter"), nullptr);
    filterCb.setProperty("toggleable", true, nullptr);
    filterCb.setProperty("toggle-on-click", true, nullptr);
    filterCb.setProperty("width", 300, nullptr);
    filterCb.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Show MIDI/VSTi Instrument Filter:"), filterCb, "instrument-filter-label"),
                        nullptr);

    // Sustain Pedal Policy (Phase 34-C)
    auto sustainCombo = node("ComboBox", "sustain-policy-combo");
    sustainCombo.setProperty("width", 300, nullptr);
    sustainCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Sustain Pedal Mode:"), sustainCombo, "sustain-policy-label"), nullptr);
    // Keyboard Partition Mode (Phase 38-3)
    auto partitionCombo = node("ComboBox", "partition-mode-combo");
    partitionCombo.setProperty("width", 300, nullptr);
    partitionCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Keyboard Partition Mode:"), partitionCombo, "partition-mode-label"), nullptr);
    // Channel Routing & Group Override Hint (Phase 38-3)
    auto hintText
        = text(TRANS("Group Channel: Inherited (Region A: Ch 1, Region B: Ch 1)"), "group-override-hint-label");
    hintText.setProperty("width", 300, nullptr);
    hintText.setProperty("height", 24, nullptr);
    hintText.setProperty("font-size", 12, nullptr);
    content.appendChild(settingRow(TRANS("Channel Routing:"), hintText, "channel-routing-label"), nullptr);

    // Region A Controls
    auto regAChannelCombo = node("ComboBox", "region-a-channel-combo");
    regAChannelCombo.setProperty("width", 300, nullptr);
    regAChannelCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Region A Channel:"), regAChannelCombo, "region-a-channel-label"), nullptr);

    auto regATransposeSlider = node("Slider", "region-a-transpose-slider");
    regATransposeSlider.setProperty("width", 300, nullptr);
    regATransposeSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Region A Transpose:"), regATransposeSlider, "region-a-transpose-label"),
                        nullptr);

    auto regAOctaveSlider = node("Slider", "region-a-octave-slider");
    regAOctaveSlider.setProperty("width", 300, nullptr);
    regAOctaveSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Region A Octave:"), regAOctaveSlider, "region-a-octave-label"), nullptr);

    // Region B Controls
    auto regBChannelCombo = node("ComboBox", "region-b-channel-combo");
    regBChannelCombo.setProperty("width", 300, nullptr);
    regBChannelCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Region B Channel:"), regBChannelCombo, "region-b-channel-label"), nullptr);

    auto regBTransposeSlider = node("Slider", "region-b-transpose-slider");
    regBTransposeSlider.setProperty("width", 300, nullptr);
    regBTransposeSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Region B Transpose:"), regBTransposeSlider, "region-b-transpose-label"),
                        nullptr);

    auto regBOctaveSlider = node("Slider", "region-b-octave-slider");
    regBOctaveSlider.setProperty("width", 300, nullptr);
    regBOctaveSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Region B Octave:"), regBOctaveSlider, "region-b-octave-label"), nullptr);

    // Language
    auto langCombo = node("ComboBox", "language-combo");
    langCombo.setProperty("width", 300, nullptr);
    langCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Language:"), langCombo, "language-label"), nullptr);
    card.appendChild(content, nullptr);
    return card;
}
juce::ValueTree makeInstrumentLayersSectionTree() {
    auto card = flexColumn("instrument-layers-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Instrument Layers"), "instrument-layers-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    auto content = flexColumn("instrument-layers-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    // Row 1: Dual-Layer Enabled Checkbox
    auto enabledCb = node("Checkbox", "layers-enabled-toggle");
    enabledCb.setProperty("text", TRANS("Dual-Layer Enabled (Piano + VST3)"), nullptr);
    enabledCb.setProperty("toggleable", true, nullptr);
    enabledCb.setProperty("toggle-on-click", true, nullptr);
    enabledCb.setProperty("width", 300, nullptr);
    enabledCb.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Dual-Layer Mode:"), enabledCb, "layers-enabled-label"), nullptr);

    // Row 2: Piano Layer Enabled Checkbox
    auto pianoCb = node("Checkbox", "layers-piano-toggle");
    pianoCb.setProperty("text", TRANS("Piano Layer Enabled"), nullptr);
    pianoCb.setProperty("toggleable", true, nullptr);
    pianoCb.setProperty("toggle-on-click", true, nullptr);
    pianoCb.setProperty("width", 300, nullptr);
    pianoCb.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Piano Layer:"), pianoCb, "layers-piano-label"), nullptr);

    // Row 3: Piano Layer Gain Slider
    auto pianoGainSlider = node("Slider", "layers-piano-gain-slider");
    pianoGainSlider.setProperty("width", 300, nullptr);
    pianoGainSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Piano Gain:"), pianoGainSlider, "layers-piano-gain-label"), nullptr);

    // Row 4: Plugin Layer Enabled Checkbox
    auto pluginCb = node("Checkbox", "layers-plugin-toggle");
    pluginCb.setProperty("text", TRANS("VST3 Plugin Layer Enabled"), nullptr);
    pluginCb.setProperty("toggleable", true, nullptr);
    pluginCb.setProperty("toggle-on-click", true, nullptr);
    pluginCb.setProperty("width", 300, nullptr);
    pluginCb.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Plugin Layer:"), pluginCb, "layers-plugin-label"), nullptr);

    // Row 5: Plugin Layer Gain Slider
    auto pluginGainSlider = node("Slider", "layers-plugin-gain-slider");
    pluginGainSlider.setProperty("width", 300, nullptr);
    pluginGainSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Plugin Gain:"), pluginGainSlider, "layers-plugin-gain-label"), nullptr);

    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeAcousticsSectionTree() {
    auto card = flexColumn("acoustics-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Acoustics & Voicing"), "acoustics-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("acoustics-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    // Row 1: Piano Style (ComboBox)
    auto styleCombo = node("ComboBox", "piano-style-combo");
    styleCombo.setProperty("width", 300, nullptr);
    styleCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Piano Style:"), styleCombo, "piano-style-label"), nullptr);

    // Row 2: Lid Position (ComboBox)
    auto lidCombo = node("ComboBox", "lid-position-combo");
    lidCombo.setProperty("width", 300, nullptr);
    lidCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Lid Position:"), lidCombo, "lid-position-label"), nullptr);

    // Row 3: Stretch Tuning (Checkbox)
    auto stretchToggle = node("Checkbox", "stretch-tuning-toggle");
    stretchToggle.setProperty("text", TRANS("Stretch Tuning"), nullptr);
    stretchToggle.setProperty("toggleable", true, nullptr);
    stretchToggle.setProperty("toggle-on-click", true, nullptr);
    stretchToggle.setProperty("width", 300, nullptr);
    stretchToggle.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Stretch Tuning:"), stretchToggle, "stretch-tuning-label"), nullptr);

    // Row 4: Duplex Resonance (Slider)
    auto duplexSlider = node("Slider", "duplex-resonance-slider");
    duplexSlider.setProperty("width", 300, nullptr);
    duplexSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Duplex Resonance:"), duplexSlider, "duplex-resonance-label"), nullptr);

    // Row 2: Touch Velocity Curve (ComboBox)
    auto curveCombo = node("ComboBox", "touch-curve-combo");
    curveCombo.setProperty("width", 300, nullptr);
    curveCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Touch Curve:"), curveCombo, "touch-curve-label"), nullptr);

    // Row 3: Temperament (ComboBox)
    auto temperamentCombo = node("ComboBox", "temperament-combo");
    temperamentCombo.setProperty("width", 300, nullptr);
    temperamentCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Temperament:"), temperamentCombo, "temperament-label"), nullptr);

    // Row 4: A4 Reference Pitch (Slider)
    auto refPitchSlider = node("Slider", "reference-pitch-slider");
    refPitchSlider.setProperty("width", 300, nullptr);
    refPitchSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("A4 Reference Pitch:"), refPitchSlider, "reference-pitch-label"), nullptr);

    // Row 5: Sound Perspective (ComboBox)
    auto perspectiveCombo = node("ComboBox", "perspective-combo");
    perspectiveCombo.setProperty("width", 300, nullptr);
    perspectiveCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Sound Perspective:"), perspectiveCombo, "perspective-label"), nullptr);

    // Row 6: Reverb Space (ComboBox)
    auto reverbSpaceCombo = node("ComboBox", "reverb-space-combo");
    reverbSpaceCombo.setProperty("width", 300, nullptr);
    reverbSpaceCombo.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Reverb Space:"), reverbSpaceCombo, "reverb-space-label"), nullptr);

    // Row 7: Reverb Level (Slider)
    auto reverbWetSlider = node("Slider", "reverb-wet-slider");
    reverbWetSlider.setProperty("width", 300, nullptr);
    reverbWetSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Reverb Level:"), reverbWetSlider, "reverb-wet-label"), nullptr);

    // Row 8: Pedal Mechanical Noise Level (Slider, Phase 32-D)
    auto pedalNoiseSlider = node("Slider", "pedal-noise-slider");
    pedalNoiseSlider.setProperty("width", 300, nullptr);
    pedalNoiseSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Mechanical Noise:"), pedalNoiseSlider, "pedal-noise-label"), nullptr);

    // Row 9: Felt Ageing Amount (Slider, Phase 32-D)
    auto feltAgeingSlider = node("Slider", "felt-ageing-slider");
    feltAgeingSlider.setProperty("width", 300, nullptr);
    feltAgeingSlider.setProperty("height", 24, nullptr);
    content.appendChild(settingRow(TRANS("Felt Ageing:"), feltAgeingSlider, "felt-ageing-label"), nullptr);

    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeDiagnosticsSectionTree() {
    auto card = flexColumn("diagnostics-card");
    card.setProperty("margin", "0 0 14 0", nullptr);
    card.setProperty("padding", "10 14 10 14", nullptr);
    card.setProperty("border-width", "1", nullptr);
    card.setProperty("border-radius", "6", nullptr);
    card.setProperty("background", devpiano::jive::DesignTokens::get().panelBg().toDisplayString(true), nullptr);

    auto title = text(TRANS("Diagnostics"), "diagnostics-title");
    title.setProperty("width", "100%", nullptr);
    title.setProperty("font-weight", "bold", nullptr);
    title.setProperty("font-size", 15, nullptr);
    title.setProperty("height", 22, nullptr);
    title.setProperty("margin", "0 0 8 0", nullptr);
    card.appendChild(title, nullptr);

    // Indented content container (16px indent)
    auto content = flexColumn("diagnostics-content");
    content.setProperty("padding", "0 0 0 16", nullptr);

    auto editor = node("ListEditor", "diagnostics-editor");
    editor.setProperty("height", 96, nullptr);
    editor.setProperty("focusable", true, nullptr);
    content.appendChild(editor, nullptr);

    auto actionRow = flexRow("diagnostics-action-row");
    actionRow.setProperty("margin", "8 0 0 0", nullptr);
    actionRow.setProperty("justify-content", "flex-end", nullptr);

    auto openLogBtn = button(TRANS("Open Log Folder"), "open-log-dir-button");
    openLogBtn.setProperty("width", 140, nullptr);
    openLogBtn.setProperty("height", 24, nullptr);
    actionRow.appendChild(openLogBtn, nullptr);

    content.appendChild(actionRow, nullptr);
    card.appendChild(content, nullptr);
    return card;
}

juce::ValueTree makeSaveActionSectionTree() {
    auto row = flexRow("save-action-row");
    row.setProperty("justify-content", "flex-end", nullptr);
    row.setProperty("height", 36, nullptr);
    row.setProperty("margin", "4 0 16 0", nullptr);

    auto saveBtn = button(TRANS("Save"), "save-button");
    saveBtn.setProperty("width", 110, nullptr);
    saveBtn.setProperty("height", 28, nullptr);
    row.appendChild(saveBtn, nullptr);

    return row;
}

juce::ValueTree makeSettingsLayoutTree() {
    auto root = flexColumn("settings-root");
    root.setProperty("width", 680, nullptr);
    root.setProperty("height", kSettingsLayoutContentHeight, nullptr);
    root.setProperty("padding", "10", nullptr);
    root.appendChild(makeAudioDeviceSectionTree(), nullptr);
    root.appendChild(makeKeySignatureSectionTree(), nullptr);
    root.appendChild(makeKeyboardDisplaySectionTree(), nullptr);
    root.appendChild(makeInstrumentLayersSectionTree(), nullptr);
    root.appendChild(makeAcousticsSectionTree(), nullptr);
    root.appendChild(makeDiagnosticsSectionTree(), nullptr);
    root.appendChild(makeSaveActionSectionTree(), nullptr);

    return root;
}

} // namespace devpiano::ui::jive

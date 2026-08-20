# Changelog

All notable changes to this project will be documented in this file.

## [0.4.0] - 2026-08-20

Enhanced physical modeling piano synthesizer (Enhanced Modal Piano v3), real-time global key signature and playback transposition pipeline, virtual keyboard dirty rectangle optimization, preset overwrite confirmation, 16-channel routing matrix, dual tone engine switching, and comprehensive UI/visual polish.

### Added

- **Enhanced Modal Piano Synthesizer v3** (`PianoSynthVoice`) — a high-fidelity physical modeling modal synthesizer as the default fallback instrument without external plugins.
  - **Magic Circle coupled-form recursive oscillators** — zero per-sample `std::sin()` calls with strict amplitude bounds and exact frequency tracking.
  - **Stiff-string inharmonicity modeling** ($f_m = m \cdot f_0 \cdot \sqrt{1 + B \cdot m^2}$) across 4 distinct keyboard acoustic regions.
  - **Two-stage modal decay envelope** (fast strike radiation vs long-tailed polarization tail) with high-frequency modal damping slopes.
  - **Triple-string unison beating doublet** with microscopic detuning ($0.10\%\sim 0.20\%$) on bass/midrange partials.
  - **Soundboard modal resonator bank** (8 resonant modal poles from $75\text{ Hz}$ to $950\text{ Hz}$) with wet/dry body coupling.
  - **Dual-mapping velocity response** ($v^{1.5}$ loudness curve + progressive high-frequency strike brightness).
- **Real-time Global Key Signature & Transposition Pipeline** — seamless real-time pitch shifting across live keyboard playing, virtual piano mouse clicks, and imported multi-track MIDI file playback with full $[0, 127]$ safe clamping.
- **General MIDI (GM) Channel 10 Percussion Bypass** — hardware-workstation-grade drum channel protection ensuring rhythm kits remain completely unshifted during transposition.
- **16-Channel Follow Key Matrix Unification** — independent per-channel transpose masks allowing fine-grained control over which MIDI channels follow global key signature changes.
- **Virtual Keyboard Dirty Rectangle Repainting** (`CustomKeyboard`) — localized `repaintKey()` and `clip.intersects()` bounding box checks eliminating full-component redraw overhead during high-speed playback and chord play.
- **Performance Preset Overwrite Confirmation** (`PresetConfirmDialog`) — safeguards user preset files from accidental overwrites during rename or save operations.
- **Dual built-in tone switching** — seamless switching between Physical Piano and Sine synth fallbacks via UI dropdown, CLI flags (`--piano`, `--sine`), and persisted configuration.
- **4x4 symmetrical controls panel layout** — redesigned ControlsPanel with an upper Piano tone row (Volume, Brightness, Hammer, Resonance) and a lower ADSR envelope row (Attack, Decay, Sustain, Release).
- **Offline WAV export tone fidelity** — WAV export options propagate built-in tone selection and physical piano parameters to offline rendering.
- **Expanded deterministic test suite** (`PianoSynthVoiceTest`, `AudioEnginePlaybackTransposeTest`, `MidiChannelMapperTest`) — comprehensive unit tests covering partial frequencies, inharmonicity, decay slopes, playback transposition, and channel 10 bypass.

### Changed

- Default built-in fallback instrument changed from Sine synth to Enhanced Modal Piano v3.
- `SettingsModel` and `SettingsStore` extended with `builtinTone` and piano physical parameters with backwards-compatible migration and value clamping.
- Dialog typography enlarged to 15pt/16pt (`KeyBindingEditDialog`, `PerformanceMetadataDialog`, `PresetDialogs`, `DevPianoLookAndFeel`) for improved high-DPI legibility.
- Settings dialog content wrapped in a scrollable Viewport container with dedicated row spacing.
- Status bar redesigned as a symmetrical 3-column layout (left: MIDI activity & plugin/preset; centre: audio engine info; right: key signature & layout) mathematically centered at 50% window width.
- ComboBox outline color mapped to `cardBorder` to eliminate 4-corner highlight artifacts.
- PopupMenu checkmarks moved to the right edge with aligned label padding to prevent text overlap.

### Fixed

- Text input editors (`PathEditor`, `ListEditor`, `MetadataEditor`) in JIVE modal dialogs and panels now properly grab focus and respond to keyboard input.
- File button row rounded corner clipping in preset card resolved.
- App title "devpiano" 'p' descender clipping in window header resolved by expanding line height.
- Numerical safety guards in synth voices against non-positive sample rates and unconstrained Nyquist frequencies.
- Voice retrigger transient clicks eliminated by resetting resonator filter states.
- Custom key label editor 32-character length restriction restored.
- Live settings reconfiguration hooks added for instant auditioning of key signature and channel matrix changes.
## [0.3.0] - 2026-08-16

Performance presets, per-key personalization, dark UI modernization, JIVE declarative UI migration, and the first full code-quality audit closure.

### Added

- **Performance Preset system** — data model, CRUD orchestration, F1–F12 shortcuts, and preset-change events recorded into performances.
- **Per-key customisation** — per-key labels and colours with a dialog-based editor.
- **Key signature with MIDI transpose** and per-channel follow-key mode.
- **Song metadata editing** dialog.
- **Dark visual theme** (`DevPianoLookAndFeel`) with rotary knobs for ADSR/volume and realistic piano keyboard rendering (gradients, rounded corners, shadows).
- **Collapsible plugin panel** with persisted expand/collapse state.
- **Transport button icons, bottom status bar**, and dynamic layout sizing rules.
- **Declarative UI migration** — five panels rebuilt on JIVE (`juce::ValueTree` layout + JSON style sheets + Flex/Grid), with `design_tokens.json` as the single colour source of truth shared by JIVE and native components.
- **Live style hot reload** (`Ctrl+R` and file-watch) in Debug builds.
- **Runtime component inspector** (melatonin_inspector) in Debug builds.
- **Industry-standard play/pause transport semantics**.
- **Auto-load of the first plugin** after a user-initiated scan completes.
- **Unified submodule layout** (`submodules/JIVE`, `submodules/melatonin_inspector`).

### Changed

- Key binding editor opens on **right-click** instead of double-click.
- Preset dialogs replaced with dark-themed native dialogs; button order and alignment unified.
- Speed slider made horizontal with labelled tick marks; speed readout rounding corrected.
- Audio callback no longer allocates or defers `prepareToPlay` (audit fix).
- PluginHost gained a documented thread-safety contract with assertions (audit fix).
- Recording engine fields narrowed to atomics; async lambdas guarded by an alive-flag (audit fix).
- clang-tidy integrated (bugprone/performance/readability/modernize) with zero-diagnostic gate.
- Documentation numbering unified (AUDIT-XXX audit reports, ADR-XXX decisions).

### Fixed

- Keyboard glow not syncing from white keys to black keys.
- Smooth pitch bend data race on stop.
- Preset switch use-after-free; combo requiring double-click.
- Key signature / MIDI transpose not persisting.
- JIVE combo blank labels, greyed options, recursion loops, and status text overflow.
- Plugin panel collapse breaking layout and overlapping content.
- Tooltip background and JIVE panel border rendering.
- Text editor context menu not localised.
- Various JIVE layout/style regressions across the migration.

### Known Issues

- Official release artifact is Windows x64 only.
- Linux package is not provided yet.
- External MIDI hardware remains unsupported (removed in v0.2.0).

## [0.2.0] - 2026-07-19

VST3 offline rendering, internationalization, drag-and-drop, and architecture hardening.

### Added

- **VST3 plugin offline rendering** for WAV export — plugins process recorded takes during export, resolving the deferred item from v0.1.0.
- **Internationalization (i18n)**: locale switching infrastructure, language selector in Settings, and Chinese (`zh`) UI localization across all panels (PluginPanel, ControlsPanel, HeaderPanel, KeyBindingEditDialog, Layout/Recording/Editor dialogs).
- **Drag-and-drop file support** — MIDI (`.mid`) and performance (`.devpiano`) files can be dropped onto the main window to open them.
- **Playback speed Slider + TextBox** replacing coarse step buttons for precise tempo control.
- **WAV export progress dialog** with cancel support during offline rendering.
- **Instrument filter ComboBox** in PluginPanel, replacing the show/hide toggle for finer plugin browsing.
- **Recent files list UI** via `juce::RecentlyOpenedFilesList` with auto-persistence.
- **Keyboard display settings UI** controls (note labels, highlight colours, key size).
- **Plugin scan count display** (`scanPluginCount` / `scanFailedCount`) in the data layer.
- **Developer tooling**: `.clang-format` (WebKit-based, 120 col), `.clang-tidy` (bugprone/performance/readability/modernize), unit test framework (`KeyMapTypesTest` 45 cases, `MidiFileImporterTest` 17 cases), `./scripts/dev.sh test` one-shot command.

### Changed

- **External MIDI hardware support removed** — `MidiRouter` class deleted, MIDI status display removed from HeaderPanel, related AppState fields and documentation references cleaned up.
- **Diagnostics logging** migrated from custom `DebugLog.h`/`.cpp` macros to `juce::Logger` + `DevPianoLogger` subclass.
- **PerformanceFile MIDI serialization** switched from manual int-array encoding to `MemoryBlock::toBase64Encoding()` for smaller JSON.
- **`WavExportOptions`** extracted to standalone `Export/WavExportOptions.h`, eliminating cross-module dependency on `WavFileExporter.h`.
- **SettingsComponent callbacks** migrated from manual `onChange` lambdas to `ValueTree::Listener` declarative binding; fixed a missing `setDirty(true)` on fade speed slider.
- **`MainComponent` slimmed** — `showSettingsDialog()` body (~47 lines) extracted to `SettingsWindowManager::showFor()`, reducing `MainComponent.cpp` from 812 to 765 lines.
- **JUCE submodule** updated to latest develop branch.
- **`-Wall -Wextra`** enabled for Clang; all warnings eliminated from project source.
- **All source code** formatted with `clang-format`.

### Fixed

- Settings window i18n labels now refresh in real time on language switch.
- Window foreground, keyboard focus, and virtual-keyboard playback issues resolved.
- Settings button crash when `state->window` is null in `show()`.
- Main window no longer calls `toFront()` on every Settings ComboBox change.
- Deprecated `Font` constructors migrated to `FontOptions` API for JUCE 8 compatibility.
- Missing `setText()` call for `playbackSpeedLabel` on init.
- Music note symbols in recent files menu fixed with `fromUTF8()`.

### Removed

- External MIDI hardware support (`MidiRouter`, status display, related AppState fields and documentation).

## [0.1.1] - 2025-05-06

License-compliance patch release. No functional changes from v0.1.0.

### Changed

- Project license upgraded from **GPLv3** to **AGPLv3** to align with JUCE's open-source licensing requirements (JUCE is dual-licensed under AGPLv3 and a commercial licence).
- Added `THIRD-PARTY-NOTICES.md` documenting third-party code attribution (JUCE framework, FreePiano reference code).
- Added BSD 3-Clause license for the FreePiano reference source under `freepiano-src/LICENSE`.
- Removed Steinberg proprietary ASIO and VST2 SDK headers from `freepiano-src/` (reference-only directory).

## [0.1.0] - 2025-05-06

First planned Windows x64 release candidate for the JUCE-based DevPiano rewrite.

### Added

- JUCE-based Windows desktop application shell.
- Computer keyboard to MIDI note performance path.
- Built-in fallback synth output for basic sound validation.
- VST3 plugin scan, load, unload and editor lifecycle support.
- Recording and playback workflow.
- MIDI export and MIDI file import support.
- `.devpiano` performance save/open support.
- Layout preset support.
- Windows MSVC Release build and manual release checklist.

### Known Issues

- Official release artifact is Windows x64 only.
- Linux package is not provided yet; Linux remains a future validation target.
- External MIDI hardware validation is pending.
- VST3 offline rendering remains deferred.

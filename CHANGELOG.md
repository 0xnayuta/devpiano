## [Unreleased]

## [1.0.2] - 2026-08-31

Full code quality audit closure (AUDIT-002), realtime thread safety and plugin lifecycle hardening, SettingsComponent modularization, headless WAV export testing support, Direct2D test rendering stabilization, obsolete submodule cleanup (ADR-013), and MSVC Release build polish.

### Added

- **PR-Agent AI Code Review Workflow** (`.github/workflows/pr-agent.yml`) — automated Gemini 3.7 Flash powered PR review, description generation, and interactive on-demand commands.
- **Headless WAV Audio Export Mode** (`WavExportTask`) — headless execution support for batch and test runs, eliminating GUI progress dialog creation in non-interactive environments.
- **Expanded Deterministic Test Suite** — comprehensive unit tests covering physical modeling parameters, settings persistence, performance presets, and serialization flows.

### Changed

- **SettingsComponent Architecture Modularization** — extracted monolithic 760-line inline header into dedicated `SettingsComponent.cpp`, cleanly decoupling audio, display, and channel matrix view builders.
- **Submodule Tree Lean Optimization (ADR-013)** — removed obsolete `melatonin_inspector` submodule and cleaned up repository build configuration and documentation references.
- **MSVC Release Linker & Concurrency Tuning** — enabled `/CGTHREADS:8` for multi-threaded link-time code generation and unbuffered stdout in test runners under Windows.

### Fixed

- **Realtime Thread Safety & Plugin Lifecycle Hardening** — reinforced destruction order and state guards in `PluginHost` and `MainComponent` to eliminate potential race conditions during audio device reconfiguration and app teardown.
- **Audio Device Diagnostics Robustness** — hardened `AudioDeviceDiagnostics` against null device states and hardware disconnections.
- **Direct2D Off-Screen Render Test Stability** — scoped `juce::Graphics` contexts with RAII blocks across `KeyboardHitMappingTest` and `StyleCatalogTest`, ensuring Direct2D flushes before pixel sampling assertions.
- **MSVC Release Compiler Warning C4505** — suppressed harmless unreferenced static function removal warnings for third-party C translation units (SheenBidi) in Release builds.

## [1.0.1] - 2026-08-31

Multi-track MIDI timeline merge engine, flexible multi-channel routing strategies, dynamically-colored virtual keyboard playback, physical modeling synth gain staging refinement with master soft-knee limiter, Linux native windowing and CJK typography polish, robust test fixture path resolution, and modern compiler cache build acceleration pipeline.

### Added

- **Multi-Track MIDI Timeline Merge Engine** (`MidiTrackMergeEngine`) — robust absolute-timestamp timeline merging across all tracks in multi-track MIDI files with nanosecond precision, deterministic same-tick event priority ordering (Tempo/TimeSig > SysEx > CC > NoteOff > NoteOn), and cross-track global metadata extraction.
- **Flexible Channel Mapping Strategies** — 4 distinct channel strategies (`preserveOriginalChannels`, `autoAssignIfSingleChannel`, `remapSequential`, `mergeToSingleChannel`) adapting seamlessly to single-channel multi-track, multi-channel polyphonic, and live-merge playback scenarios.
- **Multi-Channel Playback Keyboard Visualisation** (`CustomKeyboard`) — virtual keyboard keys dynamically adapt to active MIDI event channel colors during multi-track playback, fully decoupled from physical keyboard and mouse click interactions.
- **Multi-Track Offline WAV Audio Rendering** (`RenderPipeline`) — full multi-track timeline mixing support for offline WAV audio rendering.
- **Physical Modeling Piano Gain Staging & Master Soft-Knee Limiter** (`PianoSynthVoice`) — recalibrated 7-subsystem acoustic gain staging and hammer transient balance, paired with an integrated master soft-knee dynamic limiter to prevent clipping on extreme dynamics.
- **PR-Agent AI Code Review Workflow** (`.github/workflows/pr-agent.yml`) — automated DeepSeek v4 Flash powered PR review, description generation, and interactive on-demand commands.

### Changed

- **Compiler Cache & Modern Build Acceleration Pipeline (ADR-011 / ADR-012)** — integrated STL precompiled headers (PCH), mold/lld high-speed linker auto-detection, MSVC `/Z7` (Embedded) + `/FS` flags, and optimized ccache/sccache configuration, cutting CI compilation time by >55% with sub-second test rebuilds.
- **Automated Dual-Platform Release Packaging** (`scripts/dev.sh package`) — unified packaging command producing Windows x64 zip and Linux x64 tar.gz archives with SHA256 checksums.

### Fixed

- **Linux Native Windowing & CJK Typography Polish** — eliminated initial frame black artifacts, ensured sharp CJK font rendering via Noto Sans CJK SC / Source Han Sans fallback chains, and enabled atomic window mapping.
- **Linux Focus-Loss Panic & Hanging Notes** — resolved focus-loss panic interrupting MIDI playback and causing hanging notes, smoothing internal window switching key states.
- **Test Fixture Path Resolution Robustness** — implemented multi-tier repository root resolution (`findRepoRoot`) in `TestHelpers.h`, resolving relative `__FILE__` issues under compiler caching (`CCACHE_BASEDIR`) and eliminating potential null optional dereferences.

## [1.0.0] - 2026-08-23

Official v1.0.0 milestone release of devpiano — modern computer keyboard piano application featuring high-fidelity physical modeling piano synthesis, VST3 instrument hosting, full 88-key grand piano keybed with wide-window dynamic centering, 16-channel MIDI key signature & transposition pipeline, JIVE declarative UI modernization, and robust multi-track performance recording & playback.

### Added

- **Standard 88-Key Grand Piano Range (A0–C8 / MIDI 21–108)** — full 88-key piano keyboard layout mapping seamlessly across physical keyboard, virtual keyboard mouse interaction, and MIDI file playback.
- **Wide-Window Dynamic Centering** (`CustomKeyboard`) — mathematical symmetrical centering for 88-key piano bed on wide screens and maximized windows with preserved 100% viewport vertical height fill ($170\text{ px}$) and full viewport felt strip rendering.
- **Real-Time 3-Column Status Bar** (`MainComponent`) — active display showing live MIDI activity indicator, active plugin/preset name, audio engine metrics (sample rate, buffer size, latency, CPU usage), and active key signature/layout mode.
- **Virtual Keyboard Dirty Rectangle Optimization** — fast-path clipped dirty rectangle repainting (`repaintKey()`) eliminating UI lag during virtuosic piano playback.
- **Performance Preset Overwrite Confirmation Dialog** (`PresetConfirmDialog`) — safeguards user preset library against unintended file overwrites.
- **Enhanced Modal Piano Synthesizer v3** (`PianoSynthVoice`) — coupled-form recursive oscillators, stiff-string inharmonicity across 4 register zones, soundboard modal resonator bank, and two-stage decay envelope.
- **16-Channel Follow Key Transposition Matrix** — real-time transposition engine with GM Channel 10 percussion bypass.

### Changed

- Default keyboard layout converged to standard 88-key grand piano range (MIDI 21 to 108).
- LookAndFeel ComboBox outlines aligned with `cardBorder` to eliminate 4-corner highlight artifacts.
- PopupMenu checkmarks right-aligned with proper label margins to prevent text clipping.
- Text input editors (`PathEditor`, `ListEditor`, `MetadataEditor`) across modal dialogs granted explicit focusability and standard text cursors.
- Button row heights adjusted to eliminate rounded corner clipping on preset and setting cards.

### Fixed

- Virtual keyboard height clipping fixed, ensuring 100% vertical viewport fill without shrinking.
- App title "devpiano" 'p' descender clipping in window header resolved.
- Keyboard hit detection geometry updated to accurately handle symmetrical horizontal centering offsets.

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

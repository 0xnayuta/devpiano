## [Unreleased]

### Changed

- **Current-Format Development Cutover** — require integer v2 presets and integer v3 native performance files, including metadata reads; remove name-based plugin/preset recovery, generated legacy identities, flat-root acoustic parsing, and the `hall` reverb alias. Preserve current defaults, permanent identities, and transactional file/session protection; retire obsolete compatibility fixtures and retain current-format safety regressions.
- **Lean Development Policy** — establish KISS/DRY/LOD/YAGNI, clean cutovers, risk-driven testing, minimal documentation updates, and proportionate Windows Debug verification in `AGENTS.md`.
- **Risk-Driven Test Pruning** — remove declaration/copywriting snapshots, setter/getter and option-forwarding echoes, misleading allocation/render self-oracles, and duplicate round-trips. Retain performance identity/timing, current-format admission, numeric bounds, lifecycle, transactional file/session protection, and real audio/WAV behavior; keep allocation measurement separate from event-count assertions.

- **AUDIT-004 Review and Localization Policy** — archive the completed software remediation phases with reproducible consumer inputs; reconcile the audit against recorded evidence without erasing the initial baseline or claiming hardware certification. Accept ADR-015 for complete localized messages and category-specific punctuation; schedule the existing-call migration as a small iteration, not an implemented formatter.

- **Complete Localized Message Templates** — adopt full sentence templates with `{0}` placeholders across preset delete/overwrite confirmations, save/rename/delete status toasts, count-in counters, export notifications, key bindings, and plugin statuses; clean up obsolete string fragments in `zh_CN.loc` and enforce single-pass parameter substitution without recursive expansion.
- **Synchronized Active Feature Documentation and Compatibility Statements** — systematically audit all 13 active feature documents, project portals, architecture references, and checklists against production code. Enforce integer v2 presets and integer v3 performance files, trimmed non-empty UUIDs, canonical reverb identifiers (`concert_hall`), single-parameter full message templates (ADR-015), fixed 2.0s WAV export tail bounds, and VST3 host boundaries while preserving historical audits and archives.
- **Documentation Maintenance Surface Streamlining & Deduplication** — enforce the anti-drift policy across active reference documentation and ADRs; replace volatile engineering metrics (specific assertion counts, test case quantities, source line counts, and build durations) with stable qualitative invariants. Consolidate duplicate current status recitals across all 13 feature documents to single source of truth references in `roadmap.md`, while preserving historical audit reports and archives unchanged.
- **Header Include Decoupling & Regex Elimination** — replace `std::regex` in `jive_TimeParser.h` with a zero-dependency deterministic string scanner and remove `#include <regex>`, eliminating recursive template instantiation overhead across JIVE UI compilation units.
- **Phase 36 Archive and Phase 37/38 Planning** — preserve the completed Phase 36 task and verification record, and revise the follow-on tasks, dependency schedule, and acceptance for piano tuning/passive resonance, keyboard zones, three-pedal control, and fixed dual-layer performance. This is a documentation-only change, not a runtime feature implementation.

### Fixed

- **Offline Snapshot Transposition Parity** — apply snapshot transposeOffset and channelFollowKeyMask during offline WAV export across builtin and VST3 renderers, achieving 1:1 pitch parity with realtime playback while preserving Note-off identity.
- **Keyboard Pedal Documentation** — remove the incorrect claim that switching layout groups resets the soft pedal; retain the Panic reset contract without changing performance behavior.
- **Content-Sized Dialogs and Consistent Footers** — fit preset, binding, metadata, and export dialogs to their rendered content; preserve a 28 logical-pixel bottom inset with full-height actions across scaling. Correct native-titlebar content sizing, wrap long confirmation names without clipping, and retain asynchronous confirmation and cooperative export cancellation.
- **Verified Consumer Contracts** — align active documentation with permanent preset identities, native v3 snapshots and legacy admission, app-global transpose, single-track MIDI export, bounded diagnostics, and cooperative plugin cancellation; retain explicit hardware and vendor verification limits.

- **Bounded Session Diagnostics** — rotate active and single-backup logs within a combined 512 KiB budget; retain UTF-8 boundaries, report file-sink failures, and keep complete debugger output.
- **Raw MIDI Velocity Traces** — report integer velocities without multiplying by 127 and preserve velocity-zero NoteOn-as-NoteOff semantics.
- **Strict Declarative UI Facade** — encapsulate live style refresh and modal initialization/confirmation in ViewHost; remove raw GuiItem escape APIs and use granular modules in the application icon header.
- **Trustworthy Test and Static Gates** — remove translation/callback/assignment self-oracles, retain executed lifecycle regressions, and fix project compiler/static diagnostics without rule suppression or third-party edits.
- **Shared Performance Map Projection** — show the same final Group/modifier/matrix/followKey mapping on QWERTY and piano views; preserve configured input identities for clicks, binding edits, and per-key customization.
- **Mouse Routing and Binding Labels** — prevent observed playback/output channels from changing subsequent mouse routing and retain binding labels through settings, resize, and viewport updates.
- **Silent-Binding Priority and Bounded Fades** — keep zero-velocity bindings silent across physical and mouse input even with Shift or fixed matrix velocity; clamp fade coefficients to 0.50–0.99 and stop timers at the preview floor.
- **Live Corner Paths and Editable Song Notes** — rebuild rounded paths using the current radii and enable multiline Notes through the production ViewHost without making diagnostic lists editable.
- **Tuning and Lowest-Octave Boundaries** — support A4 400–480 Hz consistently across settings, presets, and built-in real-time/offline rendering; fix MIDI 0–11 scientific octave labels and solfege offsets.
- **Permanent Preset Identity and Migration** — assign RFC 4122 v5/v4 UUIDs to PerformancePresets; preserve identity across renames and autosave, migrate unambiguous legacy names, and reject duplicate identities.
- **Embedded Performance Preset Snapshots** — save immutable RecordedPreset tables in v3 .devpiano performance files; execute presets by recorded sample offset, and reject legacy numeric preset formats explicitly.
- **Segmented Offline Acoustic Parity** — apply acoustic snapshots, master gain, and room reverb at exact block-relative sample offsets in both builtin and VST3 offline WAV exporters.
- **Lock-Free Audio-Owned Builtin Synthesiser** — schedule voice allocation, sustain/sostenuto/soft pedals, and pitch bend without JUCE framework locks or dynamic allocation; support zero-length event processing.
- **Zero Real-Time Trigonometry Synthesis** — precalculate metronome beat coefficients in prepareToPlay and replace runtime trigonometric calls with wavetables and bounded polynomials across all owned audio DSP paths.
- **Message-Thread Visual Dispatch** — decouple audio rendering from UI listeners via atomic display bitmasks and bounded SPSC input queues; dispatch keyboard visual updates solely from the message thread.
- **Non-Allocating Geometry Fault Handling** — silence oversized buffers and unnegotiated channel configurations at block boundaries without runtime reallocation while logging atomic diagnostic counters.
- **Metronome Audio-Thread Start Synchronization** — moved phase reset and initial beat triggering into the audio callback, removing message/audio thread races.
- **Bass-Rooted Exact Chord Recognition** — prefer bass-rooted exact matches over higher-priority inverted matches, restoring C6/Cm6 detection.
- **Count-In Transport Cancellation** — cancel pending count-ins on competing transport or Take replacement actions so delayed recording cannot overwrite the current Take.
- **Bounded A-B Loop Cleanup** — disable effective loop ranges shorter than one scaled audio block to prevent repeated all-channel cleanup bursts.
- **NRVO-Safe Audio Test Fixtures** — construct borrowed audio block descriptors only after their owning buffers reach the caller, removing self-referential return values and dependence on optional named return value optimization.
- **Isolated File Test Ownership** — remove production default-path probes and use scoped temporary directories for log, preset, and acoustic settings tests without modifying real user data.
- **Default Chord Test Coverage** — register chord recognition under `DevPiano/Core` so the standard project test run executes the complete chord suite.
- **Transactional Export Replacement** — write MIDI and WAV exports to owned sibling temporary files, close writers before replacement, and preserve existing targets on rejection, cancellation, or I/O failure.
- **Protected Preset Renames and Restored Identity** — confirm independent filename collisions, preserve normalized same-file renames, roll back failed commits, and restore the active preset before immediate binding autosave.
- **Coherent Take File Ownership** — detach stale native-file bindings on recording/import replacement, rebind successful Save As operations, and reject delayed metadata or chooser completions for another Take.
- **Ordered Settings Persistence and Complete Snapshots** — successful synchronous saves supersede older scheduled writes; deep copies preserve practice settings and independent audio-device/plugin-cache XML.
- **Bounded Native Performance Admission** — validate JUCE-encoded MIDI lengths, payloads and frame shapes before decoding; reject invalid sample rates or timelines and stably normalize legacy event order without replacing the current Take.
- **Complete MIDI File Admission** — reject missing tracks, truncated events and malformed fixed-width metadata before JUCE accessors; preserve complete multitrack files with trailing CRLF and validated extension chunks.
- **Checked Export Timelines** — reject unrepresentable sample scaling, final-event and tail additions before opening WAV output; check MIDI writer tick limits and preserve existing targets on numeric rejection.
- **Guarded Instrument Mutation** — close editors and stop active audio callbacks before rescanning or rebuilding builtin voices, then publish the updated runtime UI state.
- **Block-Boundary Transport Commands** — apply playback speed, seek and Stop commands on the audio owner; preserve next-unrendered events across speed rounding and use quiescent structural transitions.
- **Cooperative Export Shutdown** — retain task, plugin and writer ownership until background rendering actually exits; keep cancellation and application quit asynchronous without timed thread termination.
- **Native Offline Plugin Mode** — declare non-realtime operation before preparing independent VST3 instances so offline setup and processing use the same mode.
- **Stable Plugin Description Identity** — select, load and restore plugins by description identifiers instead of display names; preserve duplicate-file discovery and metadata updates, and migrate only unambiguous legacy recovery names.
- **Locked Playback Note Identity** — pair overlapping attacks FIFO and retain final output identities across transpose, mask and device-rate changes; release merged outputs only after their final holder.
- **Paired Capture Pauses and MIDI** — close recorded notes and pedals at frozen capture boundaries, exclude paused performance, and preserve explicit rearticulation/release events during MIDI export and import.
- **Sample-Domain Transport Boundaries** — deliver exact final events before audio-owned completion, rebase active transport on device-rate changes, and restore channel program/bank/controller/pitch state before seek and loop destinations.
- **Full-Period Count-In** — start recording on the audio-owned downbeat after complete one/two-bar periods, including block-aligned boundaries, delayed UI polls, cancellation and device rebuilds.
- **Instrument-Owned Soft Pedal** — apply per-channel analog CC67 state before new builtin voices start, isolate reused voices and channel releases, and share the instrument owner with offline WAV rendering.
- **Consistent Sine Envelope Rates** — recalculate ADSR coefficients when the voice sample rate changes so realtime and offline terminal samples use the same time domain.

## [1.3.0] - 2026-09-21

Modern audio defaults standardization (48000 Hz / 128 samples), Linux desktop integration with embedded application icon, realtime DSP denormal elimination, oscillator numerical stability guarding, status bar height invariant protection, and diagnostics logging infrastructure.

### Added

- **Modern Audio Default Standard (`48000 Hz / 128 samples`)** — aligned global baseline audio configuration across AudioEngine, AppState, SettingsModel, MainComponent, and PluginHost; provides responsive ultra-low latency (2.67 ms) while eliminating system-level fractional resampling jitter with automatic hardware fallback and a "Recommended" settings dropdown badge.
- **Linux Desktop & Application Icon Integration** — embedded multi-resolution PNG branding assets directly into `BinaryData` with runtime X11 `_NET_WM_ICON` property injection, FreeDesktop `devpiano.desktop` launcher specification, and automated desktop registration scripts for Ubuntu Dock and Alt+Tab legibility.
- **Diagnostics Infrastructure & File Logging (Phase 33)** — introduced `DevPianoLogger` dual-sink rolling file logger, dedicated settings diagnostics card with real-time log path inspection, and one-click "Open Log Folder" action across platforms.
- **Offline Room Reverb Parity** — wired algorithmic stereo room reverberation into the offline WAV rendering pipeline alongside a test event loop drain phase for bit-accurate offline export parity.

### Changed

- **Realtime DSP Performance & Denormal Elimination** — activated `juce::ScopedNoDenormals` across audio processing and voice rendering pipelines to eliminate x86 microcode traps; cached resonator channel weights, pruned inactive sympathetic resonance pool states, and clamped decaying envelopes below -140 dBFS.
- **MainComponent Solid Token Background** — simplified `MainComponent::paint` by eliminating obscured radial gradient computations in favor of a solid design token background.
- **AppState Architectural Decoupling (AUDIT-003 Phase B)** — decoupled `AppState` from `SettingsModel`, optimizing state snapshot construction and unit test isolation.

### Fixed

- **Magic Circle Oscillator Stability & Nyquist Limiting** — clamped oscillator step coefficients (`effEps`) to `[-1.995, 1.995]` and tuned partial cutoff threshold to 0.48x sample rate, eliminating numerical divergence on A#6, A#7, and F#7 while ensuring fundamental note generation for extreme high notes (B7/C8) at low sample rates (8000 Hz).
- **Sympathetic Resonance Pool Double Damping** — resolved unintended 0.81x double decay per sample when the sustain pedal is released and notes are inactive, restoring the designed 0.90x single decay rate and bypassing redundant loop passes.
- **Status Bar Window Height Compression Invariant** — locked status bar height invariant to 24px (`flex-shrink: 0.0`) and activated flexible virtual keyboard height shrinking, preventing status bar information clipping upon vertical window reduction.
- **CustomKeyboard Centered Felt Strip Leak** — eliminated dead crimson felt strip drawing in `CustomKeyboard` that previously leaked red horizontal lines into margin gutters upon window maximization.
- **Cross-Platform MIDI Metadata Text Decoding** — introduced `MidiTextDecoder` to cleanly decode GBK/Latin-1 metadata, rejected single-byte title misinterpretations, and tolerated trailing CRLF bytes after final MIDI chunks.

## [1.2.0] - 2026-09-14

Physical modeling piano engine depth expansion, historical microtonal tuning temperaments, dual sound perspective spatial imaging, algorithmic room reverberation, micro-mechanical action dynamics, inharmonicity jitter, and declarative acoustics voicing integration.

### Added

- **Historical Temperaments & Microtonal Tuning Engine (`TemperamentEngine`)** — introduced dedicated classical temperament system supporting Equal, 1/4 Meantone, Werckmeister III, Kirnberger III, and Just intonations, alongside continuous A4 reference pitch calibration (400.0 to 480.0 Hz).
- **Dual Sound Perspective Imaging (`PerspectiveProcessor`)** — added player-perspective (wide stereo field, left-to-right piano soundboard projection) and audience-perspective (mirrored stereo imaging with gentle high-frequency distance absorption) acoustic imaging modes.
- **Algorithmic Room Reverb Engine (`RoomReverbEngine`)** — integrated lightweight, zero-sample algorithmic stereo reverberation network with Chamber (1.5s), Concert Hall (2.4s), and Studio (0.6s) acoustic presets and smooth dry/wet level blending.
- **Physical Acoustic Lid & Una Corda Modeling** — implemented 3-state lid acoustic transfer functions (Full Open, Half Stick, Closed Lid) and physical una corda soft pedal modeling (strike position shift, duplex unison reduction, and MIDI CC 67 mapping).
- **Micro-Mechanical Action Dynamics & Transient Physics** — simulated sustain pedal mechanical whoosh and full-string resonance shock pulses (CC 64 pedalNoiseLevel), velocity-sensitive damper wood thumps and key release friction, and dynamic key release ADSR damping.
- **Inharmonicity Jitter & Per-Key Felt Ageing** — added deterministic per-key string stiffness jitter (±4.5%) and key-strike felt compaction aging dynamics (`feltAgeingAmount`) for enhanced micro-organic realism.
- **Acoustics & Voicing Settings Card (`SettingsLayoutModel`)** — introduced dedicated 9-row acoustics configuration card in the JIVE declarative settings interface.
- **Performance Preset Schema Extension (`PerformancePreset`)** — expanded `.devpiano.preset` JSON format with nested `"acoustics"` block, ensuring backward and forward compatibility with numerical clamping guards.
- **Offline WAV Export Acoustic Parity (`WavFileExporter`)** — wired all physical modeling parameters, historical temperaments, perspective imaging, and room reverberation into offline export pipeline for bit-accurate realtime parity.

### Changed

- **Documentation Consistency & Anti-Drift Governance** — overhauled all active Markdown documentation against current C++ codebase, institutionalizing anti-drift guidelines by abstracting empirical metrics while preserving physical/architectural constants.
- **Skill Infrastructure Versioning** — tracked project-tailored Oh My Pi skills (`devpiano-audit`, `devpiano-doc-sync`, `devpiano-release`) under `.omp/skills/` via selective `.gitignore` rules.
- **JIVE UI Infrastructure Internalization (ADR-014)** — internalized declarative UI runtime into `source/UI/jive/core/` with ViewHost facade and frozen API surface.

### Fixed

- **Idle Sustain Pedal Mono Downmix & Resonance Truncation** — eliminated premature decay cutoff in idle sympathetic resonance pool and fixed mono output buffer pedal noise accumulation.
- **Dynamic Release Damping ADSR Override Leak** — preserved baseline ADSR parameters in `PianoSynthVoice::configuredAdsr` and re-applied them on `startNote` to prevent high-velocity release settings from leaking across note lifecycles.
- **Kirnberger III Temperament Scale Calibration** — corrected accidental cent offsets across black keys for authentic historical temperaments.
- **Non-ASCII Unicode Symbol Compatibility** — replaced raw multibyte characters with strict 7-bit ASCII Unicode scalar representations across UI and status bar elements to prevent MSVC Latin-1 assertion failures.

## [1.1.0] - 2026-09-04

Comprehensive brand identity system introduction, zero-dependency pure geometric vector logo lockups, native multi-scale application icon pipeline with Windows ICO packaging, dual-blue color hierarchy, and brand guidelines documentation.

### Added

- **Brand Identity Visual Assets** (`assets/branding/`) — established complete geometric modernist brand identity system with 7 master and optical-variant SVGs (symbol-master, symbol-micro, symbol-mono, horizontal/vertical lockups in dark and monochrome, and 1280x640 social preview hero-cover).
- **Native Multi-Resolution Application Icons** (`assets/branding/app-icon/`) — generated high-precision multi-scale icons (16, 24, 32, 48, 64, 128, 256px PNGs) and assembled standard Windows `devpiano.ico` containing all 7 resolutions with dedicated micro-optical size tuning for sub-24px taskbar legibility.
- **CMake Icon Integration** (`CMakeLists.txt`) — integrated `ICON_BIG` and `ICON_SMALL` directly into `juce_add_gui_app(devpiano ...)` for native Windows `.exe` resource injection and cross-platform icon embedding.
- **Brand Guidelines & Design System Specification** ([`docs/reference/brand-guidelines.md`](docs/reference/brand-guidelines.md)) — documented design philosophy, 64x64 geometric symbol grid semantics, Inter SemiBold wordmark typography, lockup proportions, dual-blue boundary rules, and graphic language constraints.

### Changed

- **Project Version Bump** — advanced project version to `v1.1.0` in `CMakeLists.txt` and updated changelog.
- **Branded README Header** — upgraded both Chinese and English README header sections with high-contrast, centered horizontal dark logo artwork.

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

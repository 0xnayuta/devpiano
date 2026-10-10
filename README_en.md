<p align="center">
  <a href="https://github.com/0xnayuta/devpiano"><img src="assets/branding/logo-horizontal-dark.svg" alt="devpiano" width="480"></a>
</p>

<p align="center">
  <b>Physical Modeling Acoustic Piano · Modern Keyboard Instrument · VST3 Host</b><br>
  <a href="README.md">中文</a> | English
</p>
devpiano is a modern computer-keyboard piano application built on the JUCE 9.0.1 framework, focused on software keyboard performance, high-fidelity physical modeling synthesis, and MIDI file processing.

The application features a self-developed, pure C++ physical modeling piano synthesizer (`PianoSynthVoice`) covering **7 complete acoustic subsystems**, along with **VST3 instrument plugin hosting**, a standard 88-key virtual keybed, a 16-channel MIDI routing matrix, an internalized declarative UI runtime, and full-loop performance recording, playback, persistence, and offline audio rendering pipelines.

For project scope, core capabilities, and explicit non-goals, see [`docs/reference/project-scope.md`](docs/reference/project-scope.md).

---

## Core Feature Matrix

```text
                 ┌─────────────────────────────────────────────────┐
                 │              devpiano Architecture              │
                 └────────────────────────┬────────────────────────┘
                                          │
           ┌──────────────────────────────┼──────────────────────────────┐
           ▼                              ▼                              ▼
┌─────────────────────┐        ┌─────────────────────┐        ┌─────────────────────┐
│   Audio & Plugin    │        │   Input & Routing   │        │   Record & Render   │
│  Piano+VST3 Host    │        │ Groups+Chords+Touch │        │ Take+A-B Loop       │
│  Endpoint+Metronome │        │  16-Ch Matrix+Key   │        │  Async WAV Task     │
└─────────────────────┘        └─────────────────────┘        └─────────────────────┘
```

### 🎹 High-Fidelity Physical Modeling Piano Engine (`PianoSynthVoice`)

- **7 Complete Acoustic Physical Subsystems**: covers Hammer, String, Bridge, Soundboard, Cabinet, Air, and Room acoustics;
- **88-Key Continuous Physical Parameter Mapping**: fixed model anchors informed by stiff-string and damping literature interpolate stiffness $B$, striking ratio $d/L$, damping constants, and 1/2/3-string zones (`Piano88KeyTable.h`); the repository does not certify measured per-key calibration for a particular piano model;
- **Nonlinear Strike Dynamics & Harmonic Blooming**: 3-layer felt dynamic compaction, velocity-dependent contact time $T_c$, striking point geometric comb notch filtering, 3ms high-frequency attack crack (HF Crack), felt ageing dynamics (`feltAgeingAmount`), delayed harmonic energy blooming (Harmonic Blooming, 10–25ms), and $fff$ pitch glide with soft saturation;
- **First-Partial Tuning and Saved Acoustic Styles**: anchored A4, switchable partial-matching stretch, and main-string-driven passive Duplex segments. Standard / Bright / Warm / Intimate / Vintage styles save actual acoustic fields in performance presets and Takes, without claiming physical replicas of named piano models;
- **Acoustic Resonance & Spatial Radiation**: 16-pole orthogonal spruce soundboard modal bank, 4.2kHz spruce viscous absorption lowpass filter, bridge stereo spatial radiation, triple-string Mid-Side differential expansion with natural beating, dual perspective imaging (Player vs Audience via `PerspectiveProcessor`), and lightweight algorithmic room reverberation (`RoomReverbEngine`: Studio / Chamber / Concert Hall);
- **Mechanical Action Realism & Historical Temperaments**: CC64 sustain pedal global sympathetic resonance, pedal action whoosh and resonance shock (`pedalNoiseLevel`), damper wood thump and key release friction, dynamic release velocity ADSR damping, plus 6 temperaments (`TemperamentEngine`) and A4 reference pitch tuning (400–480 Hz, default 440 Hz, with shared limits across settings, presets, and built-in real-time/offline rendering);
- **Physical-Voice Core & Real-Time Contract**: owned voice and metronome paths use no real-time library trigonometry, heap allocation, or locks. The long-term 8-voice single-core CPU target remains $\le 0.7\%$; Debug callback measurements do not certify that value (see [verification boundaries](docs/reference/features/builtin-piano-synthesis.md#5-性能特征与无锁并发保障)). Bounded SPSC input and message-thread highlighting remain separate; third-party VST3 framework limits are tracked in [known issues](docs/issues/known-issues.md), and `SineSynthVoice` remains available.

### 🔌 VST3 Plugin Hosting & Instrument Endpoint

- **Robust Lifecycle Management**: supports default and custom multi-directory scanning, asynchronous chunked scanning, XML cache startup restoration, and failed file logging;
- **Crash-Safe State Persistence**: incremental callback persistence per detected plugin, dead-man's pedal crash-point logging, and crash blacklist deferral;
- **Unified Instrument Endpoint**: shared domain endpoint abstraction across the built-in physical modeling piano and VST3 plugins, unifying device preparation, real-time dispatch, and offline rendering;
- **Plugin Loading & Editor Hosting**: select and restore VST3 instruments by `PluginDescription` identifier rather than display name; close the editor and stop callbacks before replacing or rescanning an instance. Plugins run in-process, without isolation from vendor crashes or permanent hangs.

### ⌨️ Computer Keyboard Performance & QWERTY Performance Map

- **5-Row ANSI Physical Visualizer Card (QwertyComponent)**: declaratively embedded between Controls and Keyboard areas, featuring interactive physical key depression and 50fps phosphor afterglow decay; supports instant expand/collapse and settings persistence;
- **12-TET Pitch-Class Harmony Color Projection**: subtle chromatic harmonic hues on key labels and dynamic triadic chord geometric color blooming linked with the 88-key piano bed; 4 key color modes (Classic / Channel / Velocity / Harmony);
- **Lightweight Layout Groups & Note-off Identity Preservation**: up to 4 key groups (Group A~D), cycled via backtick or UI; physical keys retain original sounding identities until the final output holder releases, while playback pairs attacks FIFO using locked output identities;
- **Fixed Physical Zones & Three Pedals**: choose no split, main-keyboard split, or main keyboard + numeric keypad. The local mode is independent of saved musical region parameters. All 15 keypad note keys retain distinct identities; Space, Tab/Shift+Space, and unbound CapsLock control CC64, CC67, and CC66, releasing their original output channels.
- **Sample-Accurate Syncopated Legato Pedal (SustainPolicy & Sync Pedal)**: sample-accurate intra-block scheduling for $\text{CC64}(0) \to \text{NoteOn} \to \text{CC64}(127)$, eliminating legato gaps when restriking keys with pedal held without thread sleep;
- **Transient Performance Modifiers (PerformanceModifierState)**: Shift key triggers transient maximum velocity boost (127), Alt key triggers transient octave shift (+8va), auto-rebounding on release, with real-time UI HUD badges;
- **Cadence Dynamics & Chord Feedback**: optional `TypingCadenceEstimator` adapts velocity to key intervals; `VelocityHumanizer` applies bounded, deterministic hash jitter without overriding Shift's maximum-velocity boost. Held notes drive chord/inversion labels in the QWERTY card header and keyboard HUD, which fade after release.
- **Stable Key Code Routing & 88-Key Bed**: routes input via normalized key codes to eliminate IME and CapsLock interference; standard 88-key bed with localized dirty rectangle repainting (`repaintKey()`) and 3 note display modes.

### 🥁 Metronome & Practice Tools

- **Sample-Accurate Metronome**: `MetronomeProcessor` synthesizes accented clicks inside audio blocks; supports 2/4, 3/4, 4/4 and 6/8, 40–280 BPM, tap tempo, status-bar beat feedback, and an optional 1–2 bar recording count-in.
- **MIDI A-B Loops & Timeline**: `TimelineBar` supports click/drag seek and A/B markers; `RecordingEngine` schedules take-relative loops with playback-speed scaling and clears sounding notes on seeks and loop wraps.

### 🎛️ 16-Channel MIDI Matrix & Real-Time Transposition

- **16 Independent Channels**: per-channel semitone transpose, octave shift, velocity override, program selection, bank MSB switching, and key-following mode (`followKey`);
- **Global Key Signature Control**: -7..+7 semitone global key signature shifting with General MIDI (GM) Channel 10 percussion bypass protection.

### 🎙️ Performance Recording, Playback & Persistence

- **Real-Time Lock-Free Capture**: lock-free MIDI event collection on the audio thread generating immutable `RecordingTake` snapshots;
- **Variable-Speed Playback**: 0.5x–2.0x speed and Stop commands applied at audio-block boundaries, Back-to-start, pause/resume and take-relative seek/A-B loops; device-rate changes retain the Take time domain, destination channel state is restored before notes, and terminal NoteOff is delivered by audio rather than a UI timer;
- **Native Performance Files**: only current integer v4 `.devpiano` files, with JUCE length-prefixed messages, embedded snapshots, and final sounding pitch/channel/velocity. Native playback and WAV/MIDI export do not reapply zones, matrix routing, or snapshot transposition. Transactional writes and rejected loads preserve existing files and the active session without legacy-semantic migration.
- **Standard MIDI Files**: import all tracks from complete Type 0/1 files with validated chunks and metadata, retaining timing across the global tempo map; export a single-track Type 1 performance at 960 PPQ with 120 BPM at tick zero, without synthesizing title, time-signature, or key-signature metadata.
- **Performance Presets**: current integer v2, permanent UUIDs, CRUD, F1–F12 selection, and overwrite confirmation; save bindings, musical regions, acoustics, and layer switches/gains. Ordinary selection retains global key settings and the local layout mode. Playback executes acoustic/layer snapshots without transforming stored notes again.

### 📦 Offline High-Fidelity WAV Export Pipeline

- **Modern Asynchronous Non-Blocking Pipeline**: `WavExportTask` runs as a purely asynchronous workflow (`startAsync`), eliminating modal event loops and routed uniformly via `InstrumentEndpoint`;
- **Single & Fixed Dual-Layer Rendering**: retain built-in Piano/Sine or current VST3 single-layer selection, and add fixed Piano + one VST3. Independent buffers, layer gains, and bounded sample-delay alignment feed one shared master/reverb chain. Real-time and independent offline instances consume final MIDI identities; required missing plugins fail without overwriting targets or silently exporting piano alone. Vendor audio is not guaranteed bit-identical.
- **Progress & Cooperative Cancellation**: display progress and wait for the actual worker to exit before releasing resources. Failure or pre-commit cancellation preserves existing targets and cleans only task-owned temporary files; permanently blocked vendor code has no bounded cancellation deadline.

### 🎨 Internalized Declarative UI Runtime & Design System

- **Declarative UI Architecture**: main, settings, and modal containers use internal `source/UI/jive/core/` (ADR-014; the external JIVE submodule is retired), with `ValueTree`, JSON styles, and Flex/CSS Grid layout. Business code accesses components through `ViewHost`; native drawing components retain their own geometry and painting responsibilities.
- **Modern Dark Theme**: `DevPianoLookAndFeel` rotary ADSR/volume controls, transport and metronome controls, and status-bar MIDI/beat feedback, plugin name, audio metrics, and key signature;
- **Zero-External-Asset Bundling**: design tokens (`design_tokens.json`), style sheets (`style_sheets.json`), and Chinese localization resources (`zh_CN.loc`) are statically bundled as binary data at compile time, enabling single-file distribution.

### 🌐 Runtime Internationalization (i18n)

- **Instant Language Switching**: powered by JUCE `Translation` and `LocaleManager`, allowing seamless real-time switching between Simplified Chinese and English.

---

## Module Layering & Code Architecture

```text
source/
├── Main.cpp / MainComponent.*     # Application entry, cross-platform window & assembly coordinator
├── Audio/                         # AudioEngine, physical piano, metronome, SyncPedalProcessor & InstrumentEndpoint
├── Input/                         # Stable key mapping, cadence velocity & QWERTY snapshot generation
├── Midi/                          # 16-channel MIDI matrix routing & real-time transposition
├── Plugin/                        # VST3 plugin scanning, loading, lifecycle & editor hosting
├── Recording/                     # Recording, take-relative A-B loop/seek, MIDI I/O & offline rendering
├── Export/                        # Offline WAV export background task & options builder
├── Layout/                        # Performance Preset data model & CRUD orchestration
├── Settings/                      # Settings model, persistence, window manager & declarative layout
├── UI/                            # Internalized declarative UI, timeline, chord HUD, design tokens & native components
├── Locale/                        # LocaleManager & embedded binary localization tables
├── Diagnostics/                   # Structured logging system, MidiTrace & debug utilities
└── Core/                          # Core data structures (AppState, KeyMapTypes, MetronomeModel, MusicTheory)
```

---

## Development Workflow (WSL + Windows MSVC Hybrid Environment)

Recommended development setup: **WSL primary working tree + Windows mirror tree + CMake + Ninja + Windows/MSVC validation build**.

### Common Developer Commands (`./scripts/dev.sh`)

```bash
# 1. Environment self-check
./scripts/dev.sh self-check

# 2. Code formatting (WebKit-based clang-format-21)
./scripts/dev.sh format               # Format all .cpp/.h files under source/
./scripts/dev.sh format --check       # Check compliance (CI mode)

# 3. Static analysis (full clang-tidy only at iteration boundaries)
./scripts/dev.sh tidy --all

# 4. Refresh WSL compilation database (for clangd / LSP)
./scripts/dev.sh wsl-build --configure-only

# 5. Windows MSVC Debug validation (includes mirror sync)
./scripts/dev.sh win-build

# 6. Windows Debug unit tests: in the mirror's Developer PowerShell,
#    configure BUILD_TESTS=ON and run CTest (see docs/guides/quickstart.md).

# 7. Release build (only when preparing a release)
./scripts/dev.sh win-build --release

# 8. Official release packaging (generates Windows x64 zip & SHA256)
./scripts/dev.sh package              # Automatically extracts version and packages
./scripts/dev.sh package --version 1.0.0
```

### Three-Gate Quality Baseline

Every critical commit must satisfy the following three gates:
1. **Formatting**: `./scripts/dev.sh format --check` passes with zero violations;
2. **Unit Tests**: build with `BUILD_TESTS=ON` and run CTest for `devpiano_tests` in the Windows mirror; see [`docs/guides/quickstart.md`](docs/guides/quickstart.md). Linux CI uses `./scripts/dev.sh test`; running that script locally in WSL would build in the primary tree instead of the Windows mirror;
3. **Build Validation**: WSL `./scripts/dev.sh wsl-build --configure-only` + Windows `./scripts/dev.sh win-build` compile successfully.

---

## Build Artifact Paths

- **Windows Debug**: `<WIN_MIRROR_DIR>\build-win-msvc\devpiano_artefacts\Debug\DevPiano.exe`
- **Windows Release**: `<WIN_MIRROR_DIR>\build-win-msvc-release\devpiano_artefacts\Release\DevPiano.exe`
- **Distribution Package**: `<WIN_MIRROR_DIR>\dist\v<VERSION>\DevPiano-v<VERSION>-win-x64.zip`

---

## External Submodules (`submodules/`)

All third-party dependencies are tracked as Git Submodules. **Do not modify any code inside submodules**:
- `submodules/JUCE/`: JUCE cross-platform audio/GUI framework (AGPLv3 / commercial license).
> Note: Declarative UI infrastructure has been internalized into `source/UI/jive/` under ADR-014 and is no longer an external submodule.

---

## Documentation Portal & Recommended Reading

The full documentation index is available at [`docs/README.md`](docs/README.md).

- **New Developers**:
  - Quickstart & Environment Setup: [`docs/guides/quickstart.md`](docs/guides/quickstart.md)
  - Hybrid Workflow Guide: [`docs/guides/wsl-windows-msvc-workflow.md`](docs/guides/wsl-windows-msvc-workflow.md)
  - Project Scope & Non-Goals: [`docs/reference/project-scope.md`](docs/reference/project-scope.md)
  - System Architecture & Design: [`docs/reference/architecture.md`](docs/reference/architecture.md)
- **Core Feature References**:
  - Physical Modeling Piano Synthesis: [`docs/reference/features/builtin-piano-synthesis.md`](docs/reference/features/builtin-piano-synthesis.md)
  - VST3 Plugin Hosting: [`docs/reference/features/plugin-hosting.md`](docs/reference/features/plugin-hosting.md)
  - Keyboard Mapping & 88-Key Bed: [`docs/reference/features/keyboard-mapping.md`](docs/reference/features/keyboard-mapping.md)
  - 16-Channel MIDI Matrix: [`docs/reference/features/midi-channel-matrix.md`](docs/reference/features/midi-channel-matrix.md)
  - Recording, Playback & Export: [`docs/reference/features/recording-playback.md`](docs/reference/features/recording-playback.md)
  - JIVE Declarative UI & Theming: [`docs/reference/features/declarative-ui-and-theming.md`](docs/reference/features/declarative-ui-and-theming.md)
- **Quality & Releases**:
  - Roadmap & Project Status: [`docs/roadmap/roadmap.md`](docs/roadmap/roadmap.md)
  - Acceptance Criteria: [`docs/reference/acceptance.md`](docs/reference/acceptance.md)
  - Official Release Packaging Workflow: [`docs/guides/release-workflow.md`](docs/guides/release-workflow.md)
  - Known Issues & Regression Keys: [`docs/issues/known-issues.md`](docs/issues/known-issues.md)

---

## License

This project is licensed under the **AGPLv3** license. For third-party notices and attribution, see [`THIRD-PARTY-NOTICES.md`](THIRD-PARTY-NOTICES.md).

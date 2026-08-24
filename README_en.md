# devpiano

[中文](README.md) | English

**devpiano** is a modern computer-keyboard piano application built on the JUCE framework, focused on software keyboard performance, high-fidelity physical modeling synthesis, and MIDI file processing.

The application features a self-developed, pure C++ physical modeling piano synthesizer (`PianoSynthVoice`) covering **7 complete acoustic subsystems**, along with **VST3 instrument plugin hosting**, a standard 88-key virtual keybed, a 16-channel MIDI routing matrix, JIVE declarative UI, and full-loop performance recording, playback, persistence, and offline audio rendering pipelines.

For project scope, core capabilities, and explicit non-goals, see [`docs/reference/project-scope.md`](docs/reference/project-scope.md).

---

## Core Feature Matrix

```text
               ┌────────────────────────────────────────────────────────┐
               │              devpiano v1.0.0 Architecture              │
               └───────────────────────────┬────────────────────────────┘
                                           │
         ┌─────────────────────────────────┼────────────────────────────────┐
         ▼                                 ▼                                ▼
┌──────────────────┐             ┌──────────────────┐             ┌──────────────────┐
│  Audio & Plugin  │             │ Input & Routing  │             │ Record & Render  │
│ 7 Acoustic Sys PM│             │ Stable Key Map+88│             │ Lock-free Take   │
│ VST3 Host+Editor │             │ 16-Ch Matrix+Key │             │ Offline WAV Task │
└──────────────────┘             └──────────────────┘             └──────────────────┘
```

### 🎹 High-Fidelity Physical Modeling Piano Engine (`PianoSynthVoice`)
- **7 Complete Acoustic Physical Subsystems**: covers Hammer, String, Bridge, Soundboard, Cabinet, Air, and Room acoustics;
- **88-Key Continuous Physical Parameter Mapping**: calibrated based on Bensa et al. (2003) and Steinway B 9-foot concert grand measurements, continuously interpolating string stiffness $B$, striking ratio $d/L$, damping constants, and 1/2/3 string unison zones (`Piano88KeyTable.h`);
- **Nonlinear Strike Dynamics & Harmonic Blooming**: 3-layer felt dynamic compaction, velocity-dependent contact time $T_c$, striking point geometric comb notch filtering, 3ms high-frequency attack crack (HF Crack), delayed harmonic energy blooming (Harmonic Blooming, 10–25ms), and $fff$ pitch glide with soft saturation;
- **Acoustic Resonance & Spatial Radiation**: 16-pole orthogonal spruce soundboard modal bank, 4.2kHz spruce viscous absorption lowpass filter, bridge stereo spatial radiation, and triple-string Mid-Side differential expansion with natural beating;
- **Mechanical & Sympathetic Realism**: CC64 sustain pedal global sympathetic resonance pool, unpedaled single-key open string duplex resonance, multi-stage lid position acoustic transfer functions (Full / Half / Closed), and damper felt fall mechanical thumps;
- **Hard Real-Time Guarantees**: Magic Circle coupled-form recursive oscillators, **zero per-sample trigonometric calls (zero `std::sin`)**, $\le 0.7\%$ single-core CPU load under 8-voice polyphony, strictly **zero heap allocations and zero locks** on the real-time audio thread; supports seamless auditioning with the built-in sine synthesizer (`SineSynthVoice`).

### 🔌 VST3 Plugin Hosting System
- **Robust Lifecycle Management**: supports default and custom multi-directory scanning, asynchronous chunked scanning, XML cache startup restoration, and failed file logging;
- **Plugin Loading & Editor Hosting**: loads VST3 instrument plugins into the real-time audio pipeline with isolated top-level editor window lifecycle management and exception safety guards.

### ⌨️ Computer Keyboard Performance & 88-Key Bed
- **Stable Key Code Routing**: routes input via normalized key codes to eliminate IME and CapsLock interference; enforces strictly paired note on/off tracking with automatic panic clearing on window focus loss;
- **Standard 88-Key Grand Piano Keybed**: covers the full A0–C8 (MIDI 21–108) range with wide-window dynamic symmetrical centering and felt strip auto-fill;
- **Localized Dirty Rectangle Repainting**: `CustomKeyboard` uses `repaintKey()` and viewport intersection clipping, completely eliminating full-component redraws during virtuosic MIDI playback with zero UI stutter;
- **Flexible Visual Customization**: supports 3 key color modes (Classic / Channel / Velocity) and 3 note label modes (DoReMi / FixedDo / NoteName).

### 🎛️ 16-Channel MIDI Matrix & Real-Time Transposition
- **16 Independent Channels**: per-channel semitone transpose, octave shift, velocity override, program selection, bank MSB switching, and key-following mode (`followKey`);
- **Global Key Signature Control**: -7..+7 semitone global key signature shifting with General MIDI (GM) Channel 10 percussion bypass protection.

### 🎙️ Performance Recording, Playback & Persistence
- **Real-Time Lock-Free Capture**: lock-free MIDI event collection on the audio thread generating immutable `RecordingTake` snapshots;
- **Variable-Speed Playback**: 0.5x–2.0x atomic smooth playback tempo scaling with instantaneous restart (Back);
- **Native Performance File Persistence**: `.devpiano` native file format (v2 JSON + Base64 encoding + `juce::TemporaryFile` atomic writing);
- **Standard MIDI File Interoperability**: exports standard Type 1 MIDI files (960 PPQ); imports standard `.mid` files with automatic track selection and CC64 sustain/pitch-bend parsing;
- **Performance Preset System**: full preset CRUD orchestration, F1–F12 hotkey switching, recorded preset-change automation, and overwrite confirmation dialogs (`PresetConfirmDialog`).

### 📦 Offline High-Fidelity WAV Export Pipeline
- **Multi-Threaded Offline Rendering**: `WavExportTask` runs on a dedicated background thread, unified under `RenderPipeline` for timeline scaling, event sorting, and tail panic injection;
- **Dual-Engine Export**: automatically renders via the built-in physical modeling piano when no plugin is loaded, or creates isolated offline VST3 instances for plugin rendering;
- **Modern Dark Progress Dialog**: real-time progress bar with cancellation support and automatic temporary file cleanup.

### 🎨 JIVE Declarative UI & Design System
- **Declarative UI Architecture**: main window, settings dialog, and modal dialogs are fully unified under the JIVE framework (`juce::ValueTree` layouts + JSON style sheets + Flex/Grid adaptive flow), eliminating manual coordinate calculations;
- **Modern Dark Theme**: rotary knobs for ADSR/volume based on `DevPianoLookAndFeel`, and a symmetrical 3-column status bar (live MIDI activity dot, plugin/preset label, audio metrics, and key signature);
- **Zero-External-Asset Bundling**: design tokens (`design_tokens.json`), style sheets (`style_sheets.json`), and Chinese localization resources (`zh_CN.loc`) are statically bundled as binary data at compile time, enabling single-file distribution.

### 🌐 Runtime Internationalization (i18n)
- **Instant Language Switching**: powered by JUCE `Translation` and `LocaleManager`, allowing seamless real-time switching between Simplified Chinese and English.

---

## Module Layering & Code Architecture

```text
source/
├── Main.cpp / MainComponent.*     # Application entry & main assembly coordinator
├── Audio/                         # AudioEngine, PianoSynthVoice physical modeling & SineSynthVoice
├── Input/                         # Computer keyboard capture & stable key-to-MIDI mapping
├── Midi/                          # 16-channel MIDI matrix routing & real-time transposition
├── Plugin/                        # VST3 plugin scanning, loading, lifecycle & editor hosting
├── Recording/                     # Performance recording, playback, MIDI I/O & offline pipeline
├── Export/                        # Offline WAV export background task & options builder
├── Layout/                        # Performance Preset data model & CRUD orchestration
├── Settings/                      # Settings model, persistence, window manager & declarative layout
├── UI/                            # JIVE layout models, design tokens, modal dialogs & native components
├── Locale/                        # LocaleManager & embedded binary localization tables
├── Diagnostics/                   # Structured logging system, MidiTrace & debug utilities
└── Core/                          # Core data structures & strong types (AppState, KeyMapTypes)
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

# 3. Static analysis (clang-tidy)
./scripts/dev.sh tidy                 # Incremental scan on uncommitted changes
./scripts/dev.sh tidy --all           # Full scan across all source files

# 4. Refresh WSL compilation database (for clangd / LSP)
./scripts/dev.sh wsl-build --configure-only

# 5. Run unit test suite (60 test suites, 11989+ assertions)
./scripts/dev.sh test

# 6. Windows MSVC validation build (built-in intelligent sync)
./scripts/dev.sh win-build            # Debug build (day-to-day development)
./scripts/dev.sh win-build --release  # Release build (release preparation)

# 7. Build-time profiling (-ftime-trace: slowest files / headers / templates)
./scripts/dev.sh time-trace           # Incremental analysis of latest build hotspots
./scripts/dev.sh time-trace --clean   # Clean full profile + Perfetto flame graph export

# 8. Official release packaging (generates Windows x64 zip & SHA256)
./scripts/dev.sh package              # Automatically extracts version and packages
./scripts/dev.sh package --version 1.0.0
```

### Three-Gate Quality Baseline

Every critical commit must satisfy the following three gates:
1. **Formatting**: `./scripts/dev.sh format --check` passes with zero violations;
2. **Unit Tests**: `./scripts/dev.sh test` 100% passes all test suites;
3. **Build Validation**: WSL `./scripts/dev.sh wsl-build --configure-only` + Windows `./scripts/dev.sh win-build` compile successfully.

---

## Build Artifact Paths

- **WSL Debug**: `build-wsl-clang/devpiano_artefacts/Debug/DevPiano`
- **WSL Release**: `build-wsl-clang-release/devpiano_artefacts/Release/DevPiano`
- **Windows Debug**: `<WIN_MIRROR_DIR>\build-win-msvc\devpiano_artefacts\Debug\DevPiano.exe`
- **Windows Release**: `<WIN_MIRROR_DIR>\build-win-msvc-release\devpiano_artefacts\Release\DevPiano.exe`
- **Distribution Package**: `<WIN_MIRROR_DIR>\dist\v<VERSION>\DevPiano-v<VERSION>-win-x64.zip`

---

## External Submodules (`submodules/`)

All third-party dependencies are tracked as Git Submodules. **Do not modify any code inside submodules**:
- `submodules/JUCE/`: JUCE cross-platform audio/GUI framework (AGPLv3 / commercial license);
- `submodules/JIVE/`: JIVE declarative UI framework (MIT);
- `submodules/melatonin_inspector/`: runtime Component inspector (MIT).

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

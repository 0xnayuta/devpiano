# devpiano

中文 | [English](README_en.md)

**devpiano** is a continuously evolving computer keyboard piano application—built with the JUCE framework, utilizing VST3 plugins as the core sound source, and focusing on software keyboard performance and MIDI file processing.

For project positioning, core capabilities, and explicit non-goals, please refer to [`docs/reference/project-scope.md`](docs/reference/project-scope.md).

This repository serves as the main source code and documentation repository for devpiano.

---

## Current Status

The current main branch possesses the following capabilities:

- JUCE GUI application can be built and started
- Audio output devices are initialized via the JUCE device management flow
- Computer keyboard can trigger basic MIDI notes
- Master volume and ADSR parameters are adjustable
- Virtual piano keyboard display is available
- VST3 directories can be scanned to list plugin names
- Scanned VST3 plugin instances can be loaded and participate in audio processing
- Plugin editor windows can be opened (if provided by the plugin)
- Basic settings can be persisted, including audio device status, performance parameters, input mapping, and plugin restoration info
- Layout Preset saving, importing, renaming, deleting, and startup restoration are supported
- MVP closed-loop for recording, stopping, playback, and MIDI file export is supported
- MIDI file import, automatic track selection, post-import playback, and playback boundary convergence are supported
- Unified management of recording/playback button states: Import MIDI is disabled during Recording, but another MIDI can still be safely imported during Playing to replace playback
- Real-time virtual keyboard linkage during MIDI playback and persistence/restoration of main window dimensions are supported
- Runtime UI language switching (Chinese/English) is supported, replacing the old `language_strdef.h` system with the JUCE `Translation` mechanism
- VST3 offline WAV export is supported (non-UI thread rendering + progress dialog)
- Precise playback speed control is supported (Slider + atomic thread-safety)
- Drag-and-drop file support (`.devpiano`/`.mid`/`.devpiano.preset`/`.vst3`) with blue border feedback
- `MainComponent.cpp` reduced from approximately 1587 lines to about 606 lines, with responsibilities delegated to specialized modules
- Automated unit testing framework is in place (`cmake -DBUILD_TESTS=ON` → `devpiano_tests`)

---

## Current Architecture Overview

The current main branch can be roughly divided into the following layers:

- `source/Main.cpp`
  - JUCE application entry point and main window creation
- `source/MainComponent.*`
  - Main assembly layer, responsible for connecting audio devices, input, plugins, settings, and various UI panels
- `source/Audio/`
  - `AudioEngine`: Handles MIDI aggregation, plugin audio processing, and fallback built-in synthesizer
- `source/Recording/`
  - `RecordingEngine` / `MidiFileExporter` / `MidiFileImporter`: Handles recording of performance events, playback scheduling, and MIDI file export/import
- `source/Plugin/`
  - `PluginHost`: Responsible for plugin format management, VST3 scanning, instance loading, prepare/release, and unloading
- `source/Input/`
  - `KeyboardMidiMapper`: Converts computer keyboard input to MIDI note on/off
- `source/Locale/`
  - `LocaleManager`: Language switching infrastructure, activation and management of JUCE `LocalisedStrings`
- `source/Midi/`
  - `MidiChannelMapper`: Routes MIDI messages to independent MIDI channels
- `source/UI/`
  - `HeaderPanel`, `PluginPanel`, `ControlsPanel`, `KeyboardPanel`, `PluginEditorWindow`
  - `ControlsPanel` unified management of Record / Play / Stop / Back / Import MIDI / Export MIDI / Export WAV button states
- `source/Settings/`
  - `SettingsModel`, `SettingsStore`, `SettingsComponent`: Responsible for settings modeling, persistence, and settings UI
- `source/Core/`
  - Contains lightweight core types and state aggregation structures, such as key models, MIDI types, and AppState

Current main audio path:

```text
Computer Keyboard -> MidiMessageCollector / MidiKeyboardState
-> AudioEngine
-> Loaded VST3 Plugin (Priority) or Built-in Sine Synth (Fallback)
-> JUCE Audio Device Output
```

---

## Directory Structure

### Main Source Code
- `source/`
  - Current JUCE main implementation directory
  - All new `.cpp/.h` files should be placed here preferentially
  - Currently covers keyboard input, plugin hosting, recording/playback/export, MIDI file import, and UI panel layering

### External Submodules
- `submodules/`
  - All third-party dependencies are located here
  - `submodules/JUCE/` — JUCE Framework (AGPLv3 / Commercial License)
  - `submodules/JIVE/` — JIVE Declarative UI Framework (MIT)
  - `submodules/melatonin_inspector/` — Runtime Component Inspector (MIT)
  - **Do not modify any code within submodules**

### Documentation
- `docs/`
  - Project goals, evaluations, planning, task lists, test cases, milestone lists, M8 MIDI file import and playback boundaries, etc.
  - M8 is currently wrapped up; the authoritative status is based on `docs/roadmap/roadmap.md` and `docs/roadmap/current-iteration.md`

---

## Development Entry (WSL + Windows MSVC Hybrid Workflow)

Daily development recommendations:

- Edit, search, and run scripts within the **WSL main work tree**
- Use `build-wsl-clang` to generate `compile_commands.json` for clangd
- Sync to the Windows mirror tree `G:\source\projects\devpiano`
- Use the Windows **Developer PowerShell for VS** environment for MSVC validation builds

```bash
# Self-check current development environment
./scripts/dev.sh self-check

# Format all .cpp/.h files under source/
./scripts/dev.sh format

# Check format compliance (CI mode)
./scripts/dev.sh format --check

# WSL local configure / build (Debug)
./scripts/dev.sh wsl-build --configure-only

# WSL local configure / build (Release)
./scripts/dev.sh wsl-build --release --configure-only

# Run unit tests (Configure BUILD_TESTS=ON → Build → Execute)
./scripts/dev.sh test

# Windows MSVC validation build (Debug, built-in sync)
./scripts/dev.sh win-build

# Windows MSVC validation build (Release, built-in sync)
./scripts/dev.sh win-build --release
```

For more details, see:

- [docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- [docs/guides/quickstart.md](docs/guides/quickstart.md)

> Note: The current project uses a hybrid workflow of **WSL Main Work Tree + Windows Mirror Tree + MSVC Validation**.

---

## Build Workflow

### Dependencies
- WSL: CMake 3.22+, Ninja, Clang/clangd
- All git submodules initialized (`git submodule update --init --recursive`)

### Recommended Commands

First, self-check the current environment:

```bash
./scripts/dev.sh self-check
```

Format code:

```bash
./scripts/dev.sh format
```

Refresh only WSL configure / `compile_commands.json`:

```bash
./scripts/dev.sh wsl-build --configure-only
```

WSL local build:

```bash
./scripts/dev.sh wsl-build
```

Run unit tests:

```bash
./scripts/dev.sh test
```

Windows MSVC validation build (built-in sync, no need for separate win-sync):

```bash
./scripts/dev.sh win-build
```

Release build (WSL / Windows):

```bash
./scripts/dev.sh wsl-build --release
./scripts/dev.sh win-build --release
```

### Main Artifact Paths

- WSL Debug: `build-wsl-clang/devpiano_artefacts/Debug/DevPiano`
- WSL Release: `build-wsl-clang-release/devpiano_artefacts/Release/DevPiano`
- Windows Debug: `G:\source\projects\devpiano\build-win-msvc\devpiano_artefacts\Debug\DevPiano.exe`
- Windows Release: `G:\source\projects\devpiano\build-win-msvc-release\devpiano_artefacts\Release\DevPiano.exe`

### Related Documentation

- [docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- [docs/guides/quickstart.md](docs/guides/quickstart.md)
- [docs/reference/features/midi-file-import.md](docs/reference/features/midi-file-import.md)

---

## Recommended Reading Order

For the complete documentation entry, see: [docs/README.md](docs/README.md).

To understand the current project planning, it is recommended to read in the following order:

- Quick Start: [docs/guides/quickstart.md](docs/guides/quickstart.md)
- Detailed Workflow: [docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- Current Architecture: [docs/reference/architecture.md](docs/reference/architecture.md)
- Project Positioning: [`docs/reference/project-scope.md`](docs/reference/project-scope.md)
- Roadmap & Project Status: [docs/roadmap/roadmap.md](docs/roadmap/roadmap.md)
- Current Iteration Tasks: [docs/roadmap/current-iteration.md](docs/roadmap/current-iteration.md)
- Phase Acceptance Criteria: [docs/reference/acceptance.md](docs/reference/acceptance.md)
- MIDI File Import & Playback Boundaries: [docs/reference/features/midi-file-import.md](docs/reference/features/midi-file-import.md)
- Keyboard Mapping: [docs/reference/features/keyboard-mapping.md](docs/reference/features/keyboard-mapping.md)
- Layout Presets: [docs/reference/features/layout-presets.md](docs/reference/features/layout-presets.md)
- Recording/Playback: [docs/reference/features/recording-playback.md](docs/reference/features/recording-playback.md)
- Plugin Hosting: [docs/reference/features/plugin-hosting.md](docs/reference/features/plugin-hosting.md)
- Performance Persistence: [docs/reference/features/performance-persistence.md](docs/reference/features/performance-persistence.md)
- VST3 Offline Rendering: [docs/reference/features/plugin-offline-rendering.md](docs/reference/features/plugin-offline-rendering.md)

---

## Development Notes

- Keep code minimal, modern, and cross-platform
- Do not modify any code in `submodules/`
- Verify changes via CMake build as much as possible
- When migrating old logic, prioritize "refining behavior" rather than directly porting platform-bound implementations

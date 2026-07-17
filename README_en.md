# devpiano

[中文](README.md) | English

**devpiano** is a personally-led, continuously evolving computer-keyboard piano application — built on JUCE, with VST3 plugins as its core sound source, focused on software keyboard performance and MIDI file processing.

For project scope, core capabilities, and explicit non-goals, see [`docs/reference/project-scope.md`](docs/reference/project-scope.md).

This repository is the main source and documentation tree for devpiano. The legacy FreePiano source (`freepiano-src/`) is retained as migration reference and does not participate in the current build.

---

## Current Status

The current main branch already provides the following capabilities:

- the JUCE GUI application builds and launches
- audio output devices are initialized through the JUCE device management flow
- the computer keyboard can trigger basic MIDI notes
- external MIDI input can be opened and forwarded into the engine
- master volume and ADSR parameters can be adjusted
- a virtual piano keyboard is displayed
- VST3 directories can be scanned and plugin names can be listed
- scanned VST3 plugin instances can be loaded and participate in audio processing
- plugin editor windows can be opened when a plugin provides an editor
- basic settings are persisted, including audio device state, performance parameters, input mapping, and plugin restore information
- layout presets can be saved, imported, renamed, deleted, and restored on startup
- the recording / stop / playback / MIDI export MVP loop is available
- MIDI file import, automatic track selection, imported playback, and playback boundary stabilization are available
- unified recording/playback button state management is available; Import MIDI is disabled during Recording and remains available during Playing to safely replace playback with another MIDI file
- MIDI playback visualization on the virtual keyboard and main window size persistence are available
- runtime UI language switching (Chinese/English) via JUCE `Translation` mechanism, replacing the old `language_strdef.h` system
- VST3 offline WAV export (non-UI-thread rendering + progress dialog)
- playback speed precision control (Slider + atomic thread safety)
- drag-and-drop file support (`.devpiano`/`.mid`/`.freepiano.layout`/`.vst3`), blue border feedback
- `MainComponent.cpp` reduced from ~1587 lines to ~606 lines; responsibilities extracted into dedicated modules
- automated unit testing framework ready (`cmake -DBUILD_TESTS=ON` → `devpiano_tests`)

---

## Current Architecture Overview

The current main branch can be roughly divided into the following layers:

- `source/Main.cpp`
  - JUCE application entry point and main window creation
- `source/MainComponent.*`
  - the main composition layer that connects audio devices, input, plugins, settings, and UI panels
- `source/Audio/`
  - `AudioEngine`: handles MIDI aggregation, plugin audio processing, and the built-in fallback synth
- `source/Recording/`
  - `RecordingEngine` / `MidiFileExporter` / `MidiFileImporter`: handle performance event recording, playback scheduling, MIDI file export, and MIDI file import
- `source/Plugin/`
  - `PluginHost`: manages plugin formats, VST3 scanning, instance loading, prepare/release, and unloading
- `source/Input/`
  - `KeyboardMidiMapper`: converts computer keyboard input into MIDI note on/off
- `source/Locale/`
  - `LocaleManager`: language switching infrastructure, JUCE `LocalisedStrings` activation and management
- `source/Midi/`
  - `MidiRouter`: opens external MIDI input and forwards it to `MidiMessageCollector`
- `source/UI/`
  - `HeaderPanel`, `PluginPanel`, `ControlsPanel`, `KeyboardPanel`, `PluginEditorWindow`
  - `ControlsPanel` now provides unified button state management for Record / Play / Stop / Back / Import MIDI / Export MIDI / Export WAV
- `source/Settings/`
  - `SettingsModel`, `SettingsStore`, `SettingsComponent`: responsible for settings modeling, persistence, and settings UI
- `source/Core/`
  - lightweight core types and aggregated state structures such as key models, MIDI types, and `AppState`

The current main audio path is:

```text
Computer keyboard -> MidiMessageCollector / MidiKeyboardState
-> AudioEngine
-> loaded VST3 plugin (preferred) or built-in Sine Synth (fallback)
-> JUCE audio device output
```

---

## Directory Structure

### Main Code
- `source/`
  - the current JUCE main implementation directory
  - all new `.cpp/.h` files should be placed here first
  - currently covers keyboard input, plugin hosting, recording/playback/export, MIDI file import, and UI panel layering

### JUCE Submodule
- `JUCE/`
  - JUCE framework submodule
  - **do not modify**

### Legacy Reference Source
- `freepiano-src/`
  - legacy FreePiano source code, kept only as a migration reference
  - **not part of the current main build**
  - **do not directly copy platform-specific implementations from it into the new architecture**

### Documentation
- `docs/`
  - project goals, assessments, plans, task lists, test cases, milestone checklists, M8 MIDI file import and playback boundaries, and related docs
  - M8 is now stabilized; authoritative status is maintained in `docs/roadmap/roadmap.md` and `docs/roadmap/current-iteration.md`

---

## Development Entry (WSL + Windows MSVC Hybrid Workflow)

Recommended day-to-day development approach:

- edit, search, and run scripts in the **WSL primary working tree**
- use `build-wsl-clang` to generate `compile_commands.json` for clangd
- sync to the Windows mirror tree at `G:\source\projects\devpiano`
- use **Developer PowerShell for VS** on Windows for MSVC validation builds

Recommended entry commands:

```bash
# self-check the current development environment
./scripts/dev.sh self-check

# format all .cpp/.h under source/
./scripts/dev.sh format

# check format compliance (CI mode)
./scripts/dev.sh format --check

# local WSL configure / build (Debug)
./scripts/dev.sh wsl-build --configure-only

# local WSL configure / build (Release)
./scripts/dev.sh wsl-build --release --configure-only

# run unit tests (configures BUILD_TESTS=ON → builds → executes)
./scripts/dev.sh test

# Windows MSVC validation build (Debug, sync built-in)
./scripts/dev.sh win-build

# Windows MSVC validation build (Release, sync built-in)
./scripts/dev.sh win-build --release
```
For more details, see:

- [docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- [docs/guides/quickstart.md](docs/guides/quickstart.md)


## Build Workflow

> Note: the project now uses a **WSL primary working tree + Windows mirror tree + MSVC validation** hybrid workflow.

### Dependencies
- WSL: CMake 3.22+, Ninja, Clang/clangd
- Windows: Visual Studio 2026, MSVC, CMake, Ninja, PowerShell 7 (recommended)
- JUCE submodule initialized

### Recommended Commands

First, self-check the current environment:

```bash
./scripts/dev.sh self-check
```

Refresh only WSL configure / `compile_commands.json`:

```bash
./scripts/dev.sh wsl-build --configure-only
```

Build locally in WSL:

```bash
./scripts/dev.sh wsl-build
```

Run Windows MSVC validation build (sync built-in, no separate win-sync needed):

```bash
./scripts/dev.sh win-build
```

Release builds (WSL / Windows):

```bash
./scripts/dev.sh wsl-build --release
./scripts/dev.sh win-build --release
```

### Main Output Paths

- WSL Debug: `build-wsl-clang/devpiano_artefacts/Debug/DevPiano`
- WSL Release: `build-wsl-clang-release/devpiano_artefacts/Release/DevPiano`
- Windows Debug: `G:\source\projects\devpiano\build-win-msvc\devpiano_artefacts\Debug\DevPiano.exe`
- Windows Release: `G:\source\projects\devpiano\build-win-msvc-release\devpiano_artefacts\Release\DevPiano.exe`

### Related Documents

- [docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- [docs/guides/quickstart.md](docs/guides/quickstart.md)
- [docs/reference/features/midi-file-import.md](docs/reference/features/midi-file-import.md)

---
## Legacy Code (FreePiano Reference Source)

`freepiano-src/` is **migration reference material**, not part of the current implementation.

**devpiano is an independent project, not a superset or replacement of FreePiano.** When all valuable and meaningful features from FreePiano have been implemented in devpiano, the reference mission is complete and `freepiano-src/` can be retired.

Until then, when working with legacy code:
- legacy modules should be used to extract behavior and redesign it, not to be replicated as-is

Typical replacement directions:

- `output_wasapi.* / output_asio.* / output_dsound.*`
  -> JUCE `AudioDeviceManager`
- `synthesizer_vst.*`
  -> JUCE `AudioPluginFormatManager` + `AudioPluginInstance`
- `gui.*`
  -> JUCE `Component`
- `config.*`
  -> JUCE `ApplicationProperties` + `ValueTree`

For more mapping details, see:

- [docs/reference/features/midi-file-import.md](docs/reference/features/midi-file-import.md)

---

## Recommended Documentation Reading Order

The full documentation index is available at: [docs/README.md](docs/README.md).

If you want to understand the current project plan, the recommended reading order is:

- Quick start / environment recovery: [docs/guides/quickstart.md](docs/guides/quickstart.md)
- Detailed workflow: [docs/guides/wsl-windows-msvc-workflow.md](docs/guides/wsl-windows-msvc-workflow.md)
- Current architecture: [docs/reference/architecture.md](docs/reference/architecture.md)
- Roadmap and project status: [docs/roadmap/roadmap.md](docs/roadmap/roadmap.md)
- Current iteration tasks: [docs/roadmap/current-iteration.md](docs/roadmap/current-iteration.md)
- Acceptance criteria: [docs/reference/acceptance.md](docs/reference/acceptance.md)
- Project scope: [`docs/reference/project-scope.md`](docs/reference/project-scope.md)
- MIDI file import and playback boundaries: [docs/reference/features/midi-file-import.md](docs/reference/features/midi-file-import.md)
- Keyboard mapping: [docs/reference/features/keyboard-mapping.md](docs/reference/features/keyboard-mapping.md)
- Layout presets: [docs/reference/features/layout-presets.md](docs/reference/features/layout-presets.md)
- Recording/playback: [docs/reference/features/recording-playback.md](docs/reference/features/recording-playback.md)
- Plugin hosting: [docs/reference/features/plugin-hosting.md](docs/reference/features/plugin-hosting.md)
- Performance persistence: [docs/reference/features/performance-persistence.md](docs/reference/features/performance-persistence.md)
- VST3 offline rendering: [docs/reference/features/plugin-offline-rendering.md](docs/reference/features/plugin-offline-rendering.md)

---

## Development Notes

- keep the code minimal, modern, and cross-platform
- place new code into structured subdirectories under `source/`
- do not modify anything inside `JUCE/`
- validate changes through the build workflow whenever possible
- when migrating legacy logic, prioritize extracting behavior rather than directly copying platform-specific implementations

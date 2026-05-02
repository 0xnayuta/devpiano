# devpiano

[中文](README.md) | English

A modernized FreePiano refactoring project based on **JUCE + CMake + C++20**.

The goal is to refactor the legacy Windows FreePiano codebase into a cleaner, more modern, and more maintainable audio/MIDI application, gradually replacing the old project's:

- native audio backends (WASAPI / ASIO / DirectSound)
- legacy VST hosting logic
- native Windows GUI / GDI
- old configuration and serialization approach

The new primary direction is to:

- use JUCE `AudioDeviceManager` to manage audio devices
- use JUCE `AudioPluginFormatManager` / `AudioPluginInstance` to build the plugin host
- use the JUCE `Component` tree to build the UI
- use JUCE `ValueTree` / `ApplicationProperties` to manage state and configuration

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
- Phase 4 is now stabilized; Phase 4-6 merge-all is deferred and Phase 3-2 remains a future backlog item

Areas still being improved:

- more complete keyboard mapping editing
- layout preset enhancements such as conflict warnings and graphical editing
- M6+ advanced features such as WAV offline rendering, editing, and tempo maps
- clearer formal UI layering and interaction details
- more systematic stability validation and regression testing
- Phase 4 backlog items such as Phase 3-2 VST3 offline rendering and external MIDI hardware validation

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
Computer keyboard / external MIDI -> MidiMessageCollector / MidiKeyboardState
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

# local WSL configure / build (Debug)
./scripts/dev.sh wsl-build --configure-only

# local WSL configure / build (Release)
./scripts/dev.sh wsl-build --release --configure-only

# Windows MSVC validation build (Debug, sync built-in)
./scripts/dev.sh win-build

# Windows MSVC validation build (Release, sync built-in)
./scripts/dev.sh win-build --release
```

For more details, see:

- [docs/development/wsl-windows-msvc-workflow.md](docs/development/wsl-windows-msvc-workflow.md)
- [docs/getting-started/quickstart.md](docs/getting-started/quickstart.md)

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

- [docs/development/wsl-windows-msvc-workflow.md](docs/development/wsl-windows-msvc-workflow.md)
- [docs/getting-started/quickstart.md](docs/getting-started/quickstart.md)
- [docs/features/phase4-midi-file-import.md](docs/features/phase4-midi-file-import.md)
- [docs/testing/phase4-midi-file-import.md](docs/testing/phase4-midi-file-import.md)

---

## How to Use the Legacy Code

`freepiano-src/` is **migration reference material**, not part of the current implementation.

Please follow these principles:

- you may read the legacy code to understand historical behavior and features
- do not directly copy old Windows-specific implementations into the new code
- do not reintroduce a `windows.h`-centered dependency chain as the core architecture
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

- [docs/features/phase4-midi-file-import.md](docs/features/phase4-midi-file-import.md)

---

## Recommended Documentation Reading Order

The full documentation index is available at: [docs/README.md](docs/README.md).

If you want to understand the current project plan, the recommended reading order is:

- Quick start / environment recovery: [docs/getting-started/quickstart.md](docs/getting-started/quickstart.md)
- Detailed workflow: [docs/development/wsl-windows-msvc-workflow.md](docs/development/wsl-windows-msvc-workflow.md)
- Current architecture: [docs/architecture/overview.md](docs/architecture/overview.md)
- Roadmap and project status: [docs/roadmap/roadmap.md](docs/roadmap/roadmap.md)
- Current iteration tasks: [docs/roadmap/current-iteration.md](docs/roadmap/current-iteration.md)
- Acceptance criteria: [docs/testing/acceptance.md](docs/testing/acceptance.md)
- MIDI file import and playback boundaries: [docs/features/phase4-midi-file-import.md](docs/features/phase4-midi-file-import.md)
- MIDI file import specialized tests: [docs/testing/phase4-midi-file-import.md](docs/testing/phase4-midi-file-import.md)
- Specialized test docs:
  - [docs/testing/phase2-keyboard-mapping.md](docs/testing/phase2-keyboard-mapping.md)
  - [docs/testing/phase3-layout-presets.md](docs/testing/phase3-layout-presets.md)
  - [docs/testing/phase3-recording-playback.md](docs/testing/phase3-recording-playback.md)
  - [docs/testing/phase4-midi-file-import.md](docs/testing/phase4-midi-file-import.md)
  - [docs/testing/phase2-plugin-host-lifecycle.md](docs/testing/phase2-plugin-host-lifecycle.md)

---

## Development Notes

- keep the code minimal, modern, and cross-platform
- place new code into structured subdirectories under `source/`
- do not modify anything inside `JUCE/`
- validate changes through the build workflow whenever possible
- when migrating legacy logic, prioritize extracting behavior rather than directly copying platform-specific implementations
- Phase 4 is now stabilized: MIDI file import, automatic track selection, imported playback, Import MIDI button state convergence, playback boundaries, and main window size restoration have all passed manual validation; Phase 4-6 merge-all is deferred and Phase 3-2 remains a future backlog item

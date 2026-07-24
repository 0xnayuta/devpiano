# devpiano

[中文](README.md) | English

**devpiano** is a personally-led, continuously evolving computer-keyboard piano application — built on JUCE, with VST3 plugins as its core sound source, focused on software keyboard performance and MIDI file processing.

For project scope, core capabilities, and explicit non-goals, see [`docs/reference/project-scope.md`](docs/reference/project-scope.md).

This repository is the main source and documentation tree for devpiano.

---

## Current Status

The current main branch already provides the following capabilities:

- the JUCE GUI application builds and launches
- audio output devices are initialized through the JUCE device management flow
- the computer keyboard can trigger basic MIDI notes
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
- drag-and-drop file support (`.devpiano`/`.mid`/`.devpiano.preset`/`.vst3`), blue border feedback
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
  - `MidiChannelMapper`: routes MIDI messages to independent MIDI channels
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

### External Submodules
- `submodules/`
  - all third-party dependencies live under this directory
  - `submodules/JUCE/` — JUCE framework (AGPLv3 / commercial license)
  - `submodules/JIVE/` — JIVE declarative UI framework (MIT)
  - `submodules/melatonin_inspector/` — runtime Component inspector (MIT)
  - **do not modify any code inside submodules**

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

---

## Build Workflow

> Note: the project now uses a **WSL primary working tree + Windows mirror tree + MSVC validation** hybrid workflow.

### Dependencies
- all git submodules initialized (`git submodule update --init --recursive`)

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

- do not modify any code inside `submodules/`
- validate changes through the build workflow whenever possible
- when migrating legacy logic, prioritize extracting behavior rather than directly copying platform-specific implementations

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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

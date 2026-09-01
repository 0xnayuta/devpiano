# Third-Party Notices

This file contains notices and licensing terms for third-party software components included in, derived from, or utilized by the **devpiano** project.

---

## 1. JIVE (Declarative UI Library for JUCE)

- **Repository**: <https://github.com/ImJimmi/JIVE>
- **Original Author**: Copyright (c) 2021 James Johnson
- **License**: MIT License
- **Snapshot Version**: Commit `89d5787a762e674ee8b7141031a99e6743948f05` (based on `v1.3.0-29-g89d5787`, `main` branch)
- **Usage in devpiano**: Core layout primitives (`BoxModel`, `Property`, `FlexContainer`, `FlexItem`, `GuiItem`, `Interpreter`, `Object`, `ComponentFactory`) internalized into `source/UI/jive/core/` and adapted under `devpiano::ui` pursuant to ADR-014.

```text
MIT License

Copyright (c) 2021 James Johnson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 2. JUCE Framework

The `submodules/JUCE/` directory contains the JUCE framework, providing core audio, MIDI, plugin hosting, and graphics infrastructure.

- **Repository**: <https://github.com/juce-framework/JUCE>
- **Copyright**: Raw Material Software / PACE Anti-Piracy, Inc.
- **License**: Dual-licensed under AGPLv3 and commercial JUCE licence
- **devpiano project license**: AGPLv3 (see root `LICENSE`)
- **Full licence terms**: See `submodules/JUCE/LICENSE.md` for complete details including all bundled dependencies (VST3 SDK, zlib, FLAC, libpng, etc.)

---

## 3. Steinberg VST3 SDK

- **Repository**: <https://github.com/steinbergmedia/vst3sdk>
- **Copyright**: Steinberg Media Technologies GmbH
- **License**: Proprietary / GPLv3 License

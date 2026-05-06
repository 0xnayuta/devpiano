# Third-Party Notices

This file contains notices for third-party software components used by the
devpiano project.

---

## JUCE Framework

The `JUCE/` directory is a git submodule containing the JUCE framework, which
provides the core audio, MIDI, plugin hosting, and UI infrastructure for this
project.

- **Repository**: <https://github.com/juce-framework/JUCE>
- **License**: Dual-licensed under AGPLv3 and the commercial JUCE licence
- **Full licence terms**: See `JUCE/LICENSE.md` for complete details including
  all bundled dependencies (VST3 SDK, zlib, FLAC, libpng, etc.)

---

## FreePiano (Reference Code)

The `freepiano-src/` directory contains source code from the FreePiano project,
included **solely as migration reference**. This code does not participate in the
current JUCE-based build.

- **Source**: <https://sourceforge.net/p/freepiano/code/ci/master/tree/>
- **Website**: <http://freepiano.tiwb.com>
- **Copyright**: Copyright (c) 2013, FreePiano
- **License**: BSD 3-Clause

### BSD 3-Clause License (FreePiano)

```
Copyright (c) 2013, FreePiano
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the freepiano nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL FREEPIANO BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

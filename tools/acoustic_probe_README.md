# 47-1 acoustic probe

Development-only offline measurement harness for the built-in piano. It does
not enter the product build or package.

## Pieces

- `devpiano_tests --acoustic-probe` (C++): renders deterministic single-note
  WAV cases through the production `exportTakeAsWavFile` path and writes
  `manifest.json`.
- `tools/acoustic_probe.py`: decodes the 24-bit WAVs and reports K-weighted
  short-term LUFS, 4x true peak, attack/steady RMS, late/early ratio, HF ratio,
  spectral centroid, adjacent-semitone deltas, and optional reference checks.
- `tools/acoustic_probe_reference.json`: Phase 44-3 Standard targets used for a
  first-pass protocol check. Treat it as a drift alarm, not a universal
  certification.

## Render

Build the test binary on Windows first:

```bash
./scripts/dev.sh win-test --no-sync
```

The default Phase 40 set is 88 keys at v70 plus 16 representative keys at
v41/v98. Run it from WSL through PowerShell:

```bash
/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -Command \
  "& 'G:\source\projects\devpiano\build-win-msvc\devpiano_tests_artefacts\Debug\devpiano_tests.exe' \
   --acoustic-probe \
   --out 'G:\source\projects\devpiano\build-win-msvc\acoustic-probe\phase40' \
   --benchmark"
```

For a quick single case:

```bash
... --acoustic-probe --keys 60 --velocities 41,70,98 --out 'G:\...\acoustic-probe\c4'
```

For the 45-1 same-note retrigger protocol (eight C4 strikes at v41/v70/v98
with 30/60/120/200 ms intervals):

```bash
... --acoustic-probe --retrigger --out 'G:\...\acoustic-probe\retrigger'
```

The standard analyzer windows cover the first strike only. Use a short
per-strike script against the manifest intervals when evaluating all eight
strikes; the WAVs are deterministic and can also be auditioned directly.

## Analyze

```bash
python3 tools/acoustic_probe.py \
  --input /mnt/g/source/projects/devpiano/build-win-msvc/acoustic-probe/phase40 \
  --reference tools/acoustic_probe_reference.json \
  --strict
```

Outputs `report.json` next to `manifest.json`. Use `--strict` when the reference
check should fail the run; omit it while establishing a new protocol baseline.

## Protocol

The `phase40` protocol fixes 48 kHz, 24-bit, block 512, neutral HammerVoicing,
Brightness/Hardness/Resonance 0.5, felt ageing 0, Duplex 0.15, lid full open,
Player perspective, Room wet 0, master gain 1, pedal noise 0, dual layer off,
ADSR 0.01/0.20/0.80/0.30 s, held note 2.0 s, and no extra tail.

The 16 representative keys are `21, 24, 28, 33, 36, 40, 45, 48, 52, 57, 60, 69,
72, 84, 96, 108`. Change them only with a documented protocol revision.

## Limitations

- Debug measurements are not a Release SLA.
- The true-peak path is a 4x windowed-sinc estimate, not a certified meter.
- The K-weighting and window definitions are the tool's documented protocol;
  they are not a claim of BS.1770 conformance for arbitrary material.
- The reference JSON is derived from archived Phase 40/44 numbers. A mismatch
  means protocol drift or a real change; investigate before changing DSP.

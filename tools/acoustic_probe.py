#!/usr/bin/env python3
"""Offline acoustic analyzer for the 47-1 probe.

Reads the manifest written by `devpiano_tests --acoustic-probe`, decodes the
rendered WAV cases, and reports BS.1770-style short-term LUFS, true peak,
attack/steady RMS, high-frequency ratio, spectral centroid, and adjacent
semitone deltas.

This is a development tool. It uses only NumPy and the Python standard library
so it can run in the WSL workspace without extra packages.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import wave
from pathlib import Path

import numpy as np

# ITU-R BS.1770-4 K-weighting coefficients at 48 kHz.
K_B1 = (1.53512485958697, -2.69169618940638, 1.19839281085285)
K_A1 = (1.0, -1.69065929318241, 0.73248077421585)
K_B2 = (1.0, -2.0, 1.0)
K_A2 = (1.0, -1.99004745483398, 0.99007225036621)

BOUNDARY_PAIRS = ((35, 36), (47, 48), (71, 72), (80, 81), (88, 89), (95, 96), (100, 101), (107, 108))


def read_wav(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        frames = wav.getnframes()
        raw = wav.readframes(frames)

    if width == 3:
        octets = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        values = (
            octets[:, 0].astype(np.int32)
            | (octets[:, 1].astype(np.int32) << 8)
            | (octets[:, 2].astype(np.int32) << 16)
        )
        values = np.where(values & 0x800000, values - 0x1000000, values).astype(np.float64) / 8388608.0
    elif width == 2:
        values = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    else:
        raise ValueError(f"{path}: unsupported sample width {width}")

    if channels > 1:
        values = values.reshape(-1, channels)
    else:
        values = values.reshape(-1, 1)
    return values, rate


def biquad_response(freqs: np.ndarray, b: tuple[float, ...], a: tuple[float, ...], rate: float) -> np.ndarray:
    z = np.exp(-1j * 2.0 * math.pi * freqs / rate)
    return (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)


def k_weighted_lufs(segment: np.ndarray, rate: float) -> float:
    if segment.shape[0] == 0:
        return float("-inf")
    n = segment.shape[0]
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    response = biquad_response(freqs, K_B1, K_A1, rate) * biquad_response(freqs, K_B2, K_A2, rate)
    mean_square = 0.0
    for channel in range(segment.shape[1]):
        spectrum = np.fft.rfft(segment[:, channel])
        filtered = np.fft.irfft(spectrum * response, n)
        mean_square += float(np.mean(filtered * filtered))
    if mean_square <= 0.0:
        return float("-inf")
    return -0.691 + 10.0 * math.log10(mean_square)


def true_peak_4x(segment: np.ndarray) -> float:
    # Windowed-sinc 4x interpolation is a conservative true-peak estimate.
    taps = 32
    offsets = np.arange(-taps // 2 + 1, taps // 2 + 1, dtype=np.float64)
    kernel = np.sinc(offsets / 4.0) * np.hanning(taps)
    kernel /= kernel.sum()

    peak = 0.0
    for channel in range(segment.shape[1]):
        upsampled = np.zeros(segment.shape[0] * 4, dtype=np.float64)
        upsampled[::4] = segment[:, channel]
        # Zero-stuffing reduces the interpolation gain by 4; compensate so the
        # estimate is at least the original sample peak.
        filtered = np.convolve(upsampled, kernel, mode="same") * 4.0
        peak = max(peak, float(np.max(np.abs(filtered))))
    return peak


def rms(segment: np.ndarray) -> float:
    if segment.shape[0] == 0:
        return 0.0
    return float(math.sqrt(np.mean(segment * segment)))


def spectrum_metrics(segment: np.ndarray, rate: float, hf_hz: float = 2500.0) -> tuple[float, float]:
    if segment.shape[0] < 8:
        return 0.0, 0.0
    n = segment.shape[0]
    window = np.hanning(n)
    power = np.zeros(n // 2 + 1, dtype=np.float64)
    for channel in range(segment.shape[1]):
        spectrum = np.fft.rfft(segment[:, channel] * window)
        power += np.abs(spectrum) ** 2
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    total = float(power.sum())
    if total <= 0.0:
        return 0.0, 0.0
    hf_ratio = float(power[freqs >= hf_hz].sum() / total)
    centroid = float((freqs * power).sum() / total)
    return hf_ratio, centroid


def window(segment: np.ndarray, rate: float, start: float, end: float) -> np.ndarray:
    first = max(0, int(round(start * rate)))
    last = min(segment.shape[0], int(round(end * rate)))
    return segment[first:last]


def analyze_case(path: Path, midi: int, velocity: int) -> dict:
    audio, rate = read_wav(path)
    if rate != 48000:
        raise ValueError(f"{path}: expected 48000 Hz, got {rate}")

    attack = window(audio, rate, 0.0, 0.05)
    lufs_window = window(audio, rate, 0.0, 0.4)
    steady = window(audio, rate, 0.1, 0.3)
    late = window(audio, rate, 1.1, 1.3)

    hf_steady, centroid_steady = spectrum_metrics(steady, rate)
    hf_attack, centroid_attack = spectrum_metrics(attack, rate)
    steady_rms = rms(steady)
    late_rms = rms(late)

    return {
        "midi": midi,
        "velocity": velocity,
        "file": path.name,
        "lufs_0_400ms": k_weighted_lufs(lufs_window, rate),
        "peak_0_50ms": float(np.max(np.abs(attack))) if attack.size else 0.0,
        "true_peak_4x": true_peak_4x(audio),
        "rms_100_300ms": steady_rms,
        "rms_1100_1300ms": late_rms,
        "late_over_early_rms": (late_rms / steady_rms) if steady_rms > 0.0 else 0.0,
        "hf_ratio_2500_steady": hf_steady,
        "centroid_hz_steady": centroid_steady,
        "hf_ratio_2500_attack": hf_attack,
        "centroid_hz_attack": centroid_attack,
    }


def summarize(cases: list[dict], velocity: int) -> dict:
    subset = sorted((case for case in cases if case["velocity"] == velocity), key=lambda case: case["midi"])
    if not subset:
        return {}

    lufs_values = [case["lufs_0_400ms"] for case in subset if math.isfinite(case["lufs_0_400ms"])]
    lufs_range = (max(lufs_values) - min(lufs_values)) if len(lufs_values) > 1 else 0.0

    adjacent = []
    for left, right in zip(subset, subset[1:]):
        if right["midi"] - left["midi"] == 1:
            adjacent.append(abs(right["lufs_0_400ms"] - left["lufs_0_400ms"]))

    by_midi = {case["midi"]: case for case in subset}
    boundaries = {}
    for low, high in BOUNDARY_PAIRS:
        if low in by_midi and high in by_midi:
            boundaries[f"{low}/{high}"] = abs(by_midi[high]["lufs_0_400ms"] - by_midi[low]["lufs_0_400ms"])

    return {
        "count": len(subset),
        "lufs_min": min(lufs_values) if lufs_values else None,
        "lufs_max": max(lufs_values) if lufs_values else None,
        "lufs_range_db": lufs_range,
        "adjacent_max_db": max(adjacent) if adjacent else 0.0,
        "boundaries_db": boundaries,
        "true_peak_max": max((case["true_peak_4x"] for case in subset), default=0.0),
        "peak_0_50ms_max": max((case["peak_0_50ms"] for case in subset), default=0.0),
    }


def load_reference(path: Path | None) -> dict | None:
    if path is None:
        return None
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def validate(summaries: dict, reference: dict | None) -> dict | None:
    if reference is None:
        return None
    expected = reference.get("phase40_44", reference)
    actual = summaries.get("70", {})
    result = {}

    def compare(name: str, actual_key: str, expected_key: str, tolerance: float) -> None:
        if expected_key not in expected or actual_key not in actual:
            return
        target = float(expected[expected_key])
        observed = float(actual[actual_key])
        delta = abs(observed - target)
        result[name] = {
            "expected": target,
            "observed": observed,
            "delta": delta,
            "tolerance": tolerance,
            "pass": delta <= tolerance,
        }

    compare("lufsRange", "lufs_range_db", "fullKeyV70LufsRangeDb", float(expected.get("toleranceLufsDb", 0.25)))
    compare("adjacentMax", "adjacent_max_db", "maxAdjacentSemitoneDb", float(expected.get("toleranceAdjacentDb", 0.20)))

    if "maxTruePeak" in expected:
        target = float(expected["maxTruePeak"])
        observed = max((summary.get("true_peak_max", 0.0) for summary in summaries.values() if summary), default=0.0)
        tolerance = float(expected.get("toleranceTruePeak", 0.03))
        delta = abs(observed - target)
        result["truePeakMax"] = {
            "expected": target,
            "observed": observed,
            "delta": delta,
            "tolerance": tolerance,
            "pass": delta <= tolerance,
        }

    result["pass"] = all(item["pass"] for item in result.values())
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze 47-1 acoustic probe WAV output.")
    parser.add_argument("--input", required=True, type=Path, help="Probe output directory or manifest.json")
    parser.add_argument("--output", type=Path, help="Report JSON path (default: <input>/report.json)")
    parser.add_argument("--reference", type=Path, help="Optional Phase 40/44 reference JSON")
    parser.add_argument("--strict", action="store_true", help="Exit non-zero when reference validation fails")
    args = parser.parse_args()

    manifest_path = args.input / "manifest.json" if args.input.is_dir() else args.input
    with manifest_path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)

    base_directory = manifest_path.parent
    analyzed = []
    for entry in manifest.get("cases", []):
        case_path = base_directory / entry["file"]
        analyzed.append(analyze_case(case_path, int(entry["midi"]), int(entry["velocity"])))

    summaries = {str(velocity): summarize(analyzed, velocity) for velocity in sorted({c["velocity"] for c in analyzed})}
    validation = validate(summaries, load_reference(args.reference))

    report = {
        "manifest": str(manifest_path),
        "protocol": manifest.get("protocol"),
        "sampleRate": manifest.get("sampleRate"),
        "bitsPerSample": manifest.get("bitsPerSample"),
        "caseCount": len(analyzed),
        "summaries": summaries,
        "validation": validation,
        "benchmark": manifest.get("benchmark"),
        "cases": analyzed,
    }

    output_path = args.output or (base_directory / "report.json")
    output_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print(f"cases: {len(analyzed)}")
    for velocity, summary in summaries.items():
        if not summary:
            continue
        print(
            f"v{velocity}: n={summary['count']} "
            f"LUFS range={summary['lufs_range_db']:.3f} dB "
            f"adjacent max={summary['adjacent_max_db']:.3f} dB "
            f"TP max={summary['true_peak_max']:.3f}"
        )
    if validation is not None:
        print(f"reference validation: {'PASS' if validation.get('pass') else 'FAIL'}")
        for name, item in validation.items():
            if isinstance(item, dict):
                print(
                    f"  {name}: expected={item['expected']:.3f} observed={item['observed']:.3f} "
                    f"delta={item['delta']:.3f} tol={item['tolerance']:.3f} "
                    f"{'ok' if item['pass'] else 'FAIL'}"
                )
    print(f"wrote {output_path}")

    if args.strict and validation is not None and not validation.get("pass", False):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
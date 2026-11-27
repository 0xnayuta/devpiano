#!/usr/bin/python3
"""Independent 45-2 phase-table optimizer for the built-in piano.

Development-only tool. It uses the current Piano88KeyTable/PianoNoteSetup
formulas for partial frequencies, L1 weights, two-stage decays and bloom, then
fits only the initial phase table against Salamander Grand Piano V3 attacks.

Run with /usr/bin/python3 (PyTorch CPU + NumPy). The production build does not
depend on this script.

Reference: Salamander Grand Piano V3, Alexander Holm, CC BY 3.0.
The A/B tables are MIT-licensed upstream data; see tools/phase_candidates.json.
"""

from __future__ import annotations

import argparse
import ast
import json
import math
import re
import wave
from pathlib import Path

import numpy as np
import torch

SAMPLE_RATE = 48000.0
ATTACK_SAMPLES = 2400
MAX_PARTIALS = 36
PEAK_LEVEL_AT_FULL_VELOCITY = 0.95 * 0.49

NOTE_NAMES = {36: "C2", 48: "C3", 60: "C4", 72: "C5", 84: "C6", 96: "C7", 108: "C8"}
VELOCITY_LAYERS = {41: 4, 70: 9, 98: 13}


def lerp(a: float, b: float, t: float) -> float:
    return a + t * (b - a)


def piecewise(midi: float, points: list[float], values: list[float]) -> float:
    if midi <= points[0]:
        return values[0]
    if midi >= points[-1]:
        return values[-1]
    for index in range(1, len(points)):
        if midi <= points[index]:
            t = (midi - points[index - 1]) / (points[index] - points[index - 1])
            return lerp(values[index - 1], values[index], t)
    return values[-1]


def log_piecewise(midi: float, points: list[float], values: list[float]) -> float:
    if midi <= points[0]:
        return values[0]
    if midi >= points[-1]:
        return values[-1]
    for index in range(1, len(points)):
        if midi <= points[index]:
            t = (midi - points[index - 1]) / (points[index] - points[index - 1])
            a = math.log(values[index - 1])
            b = math.log(values[index])
            return math.exp(a + t * (b - a))
    return values[-1]


def partial_count(midi: int) -> int:
    if midi <= 35:
        return 36
    if midi <= 47:
        return 28
    if midi <= 59:
        return 18
    if midi <= 71:
        return 14
    if midi <= 95:
        return 8
    return 6


def note_params(midi: int) -> dict[str, float]:
    m = float(np.clip(midi, 21, 108))
    if midi < 36:
        string_count = 1
    elif midi < 48:
        string_count = 2
    else:
        string_count = 3

    p: dict[str, float] = {"string_count": string_count, "partial_count": partial_count(midi)}
    bensa_midi = [36.0, 60.0, 96.0]
    bensa_l = [1.92, 0.62, 0.09]
    bensa_b1 = [0.25, 1.10, 9.17]
    bensa_b2 = [7.5e-5, 2.7e-4, 2.1e-3]
    p["string_length"] = log_piecewise(m, bensa_midi, bensa_l)
    p["b1"] = log_piecewise(m, bensa_midi, bensa_b1)
    p["b2"] = log_piecewise(m, bensa_midi, bensa_b2)

    b_midi = [21, 24, 26, 28, 33, 36, 40, 43, 45, 47, 48, 52, 57, 60, 69, 72, 84, 96, 108]
    b_vals = [
        1.367893e-4,
        8.800733e-5,
        6.868533e-5,
        7.330751e-5,
        4.767319e-5,
        5.080298e-5,
        6.263947e-5,
        7.985170e-5,
        8.625150e-5,
        1.059587e-4,
        1.172690e-4,
        1.617091e-4,
        2.914628e-4,
        3.529958e-4,
        8.203508e-4,
        1.093004e-3,
        3.164862e-3,
        9.614857e-3,
        2.920995e-2,
    ]
    p["B"] = log_piecewise(m, b_midi, b_vals)
    p["strike"] = piecewise(m, [36.0, 60.0, 80.0, 96.0], [0.125, 0.1333, 0.100, 0.0625])
    p["tc_base"] = log_piecewise(m, [36.0, 60.0, 96.0], [0.0022, 0.0018, 0.0006])

    beat = piecewise(m, [36.0, 60.0, 80.0, 100.0], [0.0020, 0.0015, 0.0010, 0.0])
    if m >= 100.0:
        beat = 0.00030
        p["beating_partials"] = 1
    else:
        p["beating_partials"] = min(p["partial_count"], 6)
    p["beating_detune"] = beat

    p["decay"] = piecewise(m, [21, 36, 60, 80, 100, 108], [4.8, 4.5, 2.8, 1.6, 0.9, 0.8])
    p["fast_ratio"] = piecewise(m, [21, 36, 60, 80, 100, 108], [0.12, 0.12, 0.15, 0.18, 0.15, 0.15])
    p["slow_base"] = piecewise(m, [21, 36, 60, 80, 100, 108], [0.20, 0.20, 0.18, 0.15, 0.12, 0.12])
    return p


def normalized_ratio(stiffness: float, partial: int) -> float:
    b = max(0.0, stiffness)
    m = max(1, partial)
    return m * math.sqrt((1.0 + b * m * m) / (1.0 + b))


def octave_cents(lower_note: int) -> float:
    lower_partial = 4 if lower_note < 69 else 2
    upper_partial = lower_partial // 2
    lower_ratio = normalized_ratio(note_params(lower_note)["B"], lower_partial)
    upper_ratio = normalized_ratio(note_params(lower_note + 12)["B"], upper_partial)
    return 1200.0 * math.log2(lower_ratio / (2.0 * upper_ratio))


def stretch_ratios() -> list[float]:
    cents = [0.0] * 128
    lower_anchor = -octave_cents(57)
    upper_anchor = octave_cents(69)
    for note in range(57, 70):
        cents[note] = lower_anchor * (69 - note) / 12.0
    for note in range(70, 82):
        cents[note] = upper_anchor * (note - 69) / 12.0
    for note in range(56, 20, -1):
        cents[note] = cents[note + 12] - octave_cents(note)
    for note in range(82, 109):
        cents[note] = cents[note - 12] + octave_cents(note - 12)
    for note in range(0, 21):
        cents[note] = cents[21]
    for note in range(109, 128):
        cents[note] = cents[108]
    return [2.0 ** (value / 1200.0) for value in cents]


STRETCH = stretch_ratios()


def compute_setup(midi: int, velocity: int) -> tuple[dict[str, float], list[float], list[float]]:
    p = note_params(midi)
    v = float(np.clip(velocity / 127.0, 0.0, 1.0))
    first = 440.0 * 2.0 ** ((midi - 69) / 12.0) * STRETCH[midi]
    base_frequency = first / math.sqrt(1.0 + p["B"])
    key_pos = float(np.clip((midi - 21.0) / 87.0, 0.0, 1.0))

    # Default HammerVoicing is zero; the same single hardness path as production.
    hammer_hardness = float(np.clip(0.15 + 0.85 * v**1.5 * 0.75, 0.05, 1.0))
    contact_hardness = hammer_hardness
    tc = p["tc_base"] * (2.5 - 1.9 * contact_hardness)

    # Same deterministic jitter seed as PianoNoteSetup with triggerSequence=0.
    rng = (midi * 1009 + int(v * 1000.0)) & 0xFFFFFFFF

    def hash_jitter() -> float:
        nonlocal rng
        rng = (rng * 1664525 + 1013904223) & 0xFFFFFFFF
        return (rng >> 16) / 65535.0 * 2.0 - 1.0

    strike_pos = p["strike"] * (1.0 + 0.006 * hash_jitter())
    effective_tc = tc * (1.0 + 0.008 * hash_jitter())
    jitter_phase = 0.012 * hash_jitter()

    slow_t = float(np.clip((v - 41.0 / 127.0) / (57.0 / 127.0), 0.0, 1.0))
    slow_weight = p["slow_base"] * (1.15 - 0.30 * slow_t)
    velocity_level = v**1.20
    k1 = (math.pi / p["string_length"]) ** 2
    alpha1 = p["b1"] + p["b2"] * k1
    setup = {
        "velocity": v,
        "velocity_level": velocity_level,
        "key_pos": key_pos,
        "contact_hardness": contact_hardness,
        "effective_tc": effective_tc,
        "effective_strike_pos": strike_pos,
        "base_frequency": base_frequency,
        "jitter_phase": jitter_phase,
        "slow_weight": slow_weight,
        "k1": k1,
        "alpha1": alpha1,
        "base_decay": p["decay"],
        "fast_ratio": p["fast_ratio"],
        "brightness": 0.5,
    }

    partials = p["partial_count"]
    coherent_ceiling = SAMPLE_RATE * 0.477485
    frequencies = []
    admitted = 0
    for index in range(partials):
        number = index + 1
        frequency = base_frequency * number * math.sqrt(1.0 + p["B"] * number * number)
        frequencies.append(frequency)
        if index == admitted and frequency < coherent_ceiling:
            admitted = index + 1
    if admitted <= 0:
        frequencies[0] = coherent_ceiling
        admitted = 1
    frequencies = frequencies[:admitted]
    return setup, frequencies, frequencies


def retained_weight(index: int, frequency: float, setup: dict[str, float], partials: int) -> float:
    m = index + 1
    key_pos = setup["key_pos"]
    hardness = setup["contact_hardness"]
    tc = setup["effective_tc"]
    strike = setup["effective_strike_pos"]
    brightness = setup["brightness"]

    comb = max(abs(math.sin(math.pi * m * strike)), 0.03)
    current_rolloff = 1.20 + 0.30 * key_pos + 1.20 * key_pos * key_pos
    bass_t = float(np.clip(key_pos / 0.45, 0.0, 1.0))
    bass_blend = bass_t * bass_t * (3.0 - 2.0 * bass_t)
    low_rolloff = 0.90 + 0.30 * key_pos + 1.20 * key_pos * key_pos
    rolloff = low_rolloff + (current_rolloff - low_rolloff) * bass_blend
    base_power = 1.0 / (m**rolloff)
    cutoff = max(2.5 / max(tc, 1e-4), (1.8 + 2.6 * hardness * hardness) * (frequency / m))
    exponent = float(np.clip(2.0 - 0.80 * hardness, 0.80, 6.0))
    felt = 1.0 / (1.0 + (frequency / cutoff) ** exponent)

    diff = (frequency - 1800.0) / 800.0
    bridge_hill = 1.0 + 0.40 * math.exp(-0.5 * diff * diff)
    f_tc = frequency * tc
    denom = 1.0 - 4.0 * f_tc * f_tc
    if abs(denom) < 1e-4:
        elastic = 1.0
    else:
        elastic = min(abs(math.cos(math.pi * f_tc)) / abs(denom), 1.0)
    elastic = 0.7 + 0.3 * elastic
    low_ratio = max(frequency, 1.0) / 70.0
    low_radiation = low_ratio / math.sqrt(1.0 + low_ratio * low_ratio)
    boost = 1.0 + (brightness - 0.5) * 0.5 * (index / partials)
    return comb * base_power * felt * bridge_hill * elastic * low_radiation * boost


def build_case(midi: int, velocity: int) -> dict[str, object]:
    params = note_params(midi)
    setup, frequencies, _ = compute_setup(midi, velocity)

    freqs = np.zeros((MAX_PARTIALS, 3), dtype=np.float32)
    decay_fast = np.ones(MAX_PARTIALS, dtype=np.float32)
    decay_slow = np.ones(MAX_PARTIALS, dtype=np.float32)
    bloom_initial = np.ones(MAX_PARTIALS, dtype=np.float32)
    bloom_tau = np.full(MAX_PARTIALS, 1.0, dtype=np.float32)

    for index, frequency in enumerate(frequencies):
        m = index + 1
        alpha_n = params["b1"] * (0.80 + 0.20 * math.sqrt(params["string_length"] / max(frequency, 20.0))) + params[
            "b2"
        ] * m * m * setup["k1"]
        damping = alpha_n / max(1e-9, setup["alpha1"]) * (1.5 - setup["brightness"])
        tau = setup["base_decay"] / damping
        fast = tau * params["fast_ratio"]
        decay_fast[index] = math.exp(-1.0 / (fast * SAMPLE_RATE))
        decay_slow[index] = math.exp(-1.0 / (tau * SAMPLE_RATE))

        beat = params["beating_detune"]
        if params["string_count"] <= 1 or index >= params["beating_partials"] or beat <= 0.0:
            count = 1
        else:
            count = int(params["string_count"])
        for string in range(count):
            if count == 1:
                ratio = 1.0
            elif count == 2:
                ratio = 1.0 - beat * 0.5 if string == 0 else 1.0 + beat * 0.5
            else:
                ratio = 1.0 - beat if string == 0 else (1.0 if string == 1 else 1.0 + beat)
            freqs[index, string] = frequency * ratio

        if index >= 2 and setup["velocity"] > 0.40:
            depth = 0.35 * (setup["velocity"] - 0.40) / 0.60
            bloom_initial[index] = 1.0 - depth
            bloom_tau[index] = 0.008 + 0.016 * (1.0 - index / max(1, params["partial_count"]))
        else:
            bloom_initial[index] = 1.0
            bloom_tau[index] = 1.0

    return {
        "midi": midi,
        "velocity": velocity,
        "freqs": freqs,
        "decay_fast": decay_fast,
        "decay_slow": decay_slow,
        "bloom_initial": bloom_initial,
        "bloom_tau": bloom_tau,
        "tc": float(setup["effective_tc"]),
    }


def synth_case(case: dict[str, object], phases: torch.Tensor, level_fast: torch.Tensor,
               level_slow: torch.Tensor) -> torch.Tensor:
    freqs = torch.tensor(case["freqs"], dtype=torch.float32)
    decay_fast = torch.tensor(case["decay_fast"], dtype=torch.float32)
    decay_slow = torch.tensor(case["decay_slow"], dtype=torch.float32)
    bloom_initial = torch.tensor(case["bloom_initial"], dtype=torch.float32)
    bloom_tau = torch.tensor(case["bloom_tau"], dtype=torch.float32)

    t = torch.arange(ATTACK_SAMPLES, dtype=torch.float32) / SAMPLE_RATE
    sample_index = torch.arange(ATTACK_SAMPLES, dtype=torch.float32)[None, :]
    row_indices = torch.arange(3).clamp_max(phases.shape[0] - 1)
    phase_rows = phases[row_indices]
    phase_for_string = phase_rows[:, torch.arange(MAX_PARTIALS) % 64].transpose(0, 1)

    osc = torch.sin(2.0 * math.pi * freqs[:, :, None] * t[None, None, :] + phase_for_string[:, :, None])
    env_fast = decay_fast[:, None] ** sample_index
    env_slow = decay_slow[:, None] ** sample_index
    env = (level_fast[:, None] * env_fast + level_slow[:, None] * env_slow)[:, None, :]
    bloom = 1.0 - (1.0 - bloom_initial[:, None]) * torch.exp(-t[None, :] / bloom_tau[:, None])
    osc = osc * env * bloom[:, None, :]
    mixed = osc.sum(dim=(0, 1))

    tc = max(float(case["tc"]), 1e-4)
    contact = torch.clamp(0.55 + 0.45 * t / tc, max=1.0)
    attack = torch.clamp(t / 0.0002, max=1.0)
    mixed = mixed * contact * attack
    # Same cubic soft saturation as the production voice.
    mixed = mixed * (1.0 - 0.12 * torch.clamp(mixed * mixed, 0.0, 1.0))
    return mixed


def load_wav(path: Path) -> np.ndarray:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        width = wav.getsampwidth()
        rate = wav.getframerate()
        raw = wav.readframes(wav.getnframes())
    if rate != 48000:
        raise ValueError(f"{path}: expected 48000 Hz, got {rate}")
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
    values = values.reshape(-1, channels).mean(axis=1)
    peak = float(np.max(np.abs(values)))
    onset = max(0, int(np.argmax(np.abs(values) > peak * 1e-3)) - 24)
    return values[onset : onset + ATTACK_SAMPLES]


def mel_filterbank() -> torch.Tensor:
    n_fft = 512
    n_mels = 80
    freqs = np.linspace(0, SAMPLE_RATE / 2, n_fft // 2 + 1)
    mel = lambda f: 2595.0 * np.log10(1.0 + f / 700.0)
    imel = lambda m: 700.0 * (10.0 ** (m / 2595.0) - 1.0)
    points = imel(np.linspace(mel(20.0), mel(8000.0), n_mels + 2))
    bank = np.zeros((n_mels, len(freqs)), dtype=np.float64)
    for index in range(n_mels):
        lo, center, hi = points[index : index + 3]
        left = (freqs >= lo) & (freqs <= center)
        right = (freqs > center) & (freqs <= hi)
        bank[index, left] = (freqs[left] - lo) / (center - lo)
        bank[index, right] = (hi - freqs[right]) / (hi - center)
    bank /= np.maximum(bank.sum(axis=1, keepdims=True), 1e-12)
    return torch.tensor(bank, dtype=torch.float32)


MEL = mel_filterbank()
WINDOW = torch.hann_window(512)


def log_mel(signal: torch.Tensor) -> torch.Tensor:
    signal = signal[:ATTACK_SAMPLES]
    signal = signal / (signal.abs().max() + 1e-12)
    spec = torch.stft(signal, n_fft=512, hop_length=128, window=WINDOW, center=False, return_complex=True).abs()
    return torch.log1p(torch.einsum("mf,ft->mt", MEL, spec) * 10.0)


def target_metrics(reference: Path) -> torch.Tensor:
    signal = torch.tensor(load_wav(reference), dtype=torch.float32)
    return log_mel(signal)


def target_stats(reference: Path) -> dict[str, float]:
    signal = torch.tensor(load_wav(reference), dtype=torch.float32)
    peak = float(signal.abs().max())
    rms = float(torch.sqrt(torch.mean(signal * signal)) + 1e-12)
    log = log_mel(signal)
    flux = float(torch.mean(torch.relu(log[:, 1:] - log[:, :-1])))
    return {"peak": peak, "rms": rms, "flux": flux}


def case_tensors(case: dict[str, object]) -> tuple[torch.Tensor, torch.Tensor]:
    params = note_params(int(case["midi"]))
    setup, frequencies, _ = compute_setup(int(case["midi"]), int(case["velocity"]))
    level_fast = torch.zeros(MAX_PARTIALS, dtype=torch.float32)
    level_slow = torch.zeros(MAX_PARTIALS, dtype=torch.float32)
    weights = [retained_weight(i, f, setup, params["partial_count"]) for i, f in enumerate(frequencies)]
    scale = PEAK_LEVEL_AT_FULL_VELOCITY / max(1e-6, sum(weights))
    for index, weight in enumerate(weights):
        level = weight * scale * setup["velocity_level"]
        level_fast[index] = level * (1.0 - setup["slow_weight"])
        level_slow[index] = level * setup["slow_weight"]
    return level_fast, level_slow


def loss_for(phases: torch.Tensor, cases: list[dict[str, object]], targets: list[torch.Tensor],
             target_stats_list: list[dict[str, float]], baseline_peaks: list[float],
             case_levels: list[tuple[torch.Tensor, torch.Tensor]]) -> tuple[torch.Tensor, dict[str, float]]:
    spec_loss = torch.zeros((), dtype=torch.float32)
    peak_penalty = torch.zeros((), dtype=torch.float32)
    peak_factor_penalty = torch.zeros((), dtype=torch.float32)
    flux_penalty = torch.zeros((), dtype=torch.float32)
    for case, target, stats, baseline_peak, levels in zip(
        cases, targets, target_stats_list, baseline_peaks, case_levels
    ):
        level_fast, level_slow = levels
        signal = synth_case(case, phases, level_fast, level_slow)
        peak = signal.abs().max()
        rms = torch.sqrt(torch.mean(signal * signal) + 1e-12)
        model_log = log_mel(signal)
        spec_loss = spec_loss + torch.mean((model_log - target) ** 2)
        peak_penalty = peak_penalty + (peak / max(baseline_peak, 1e-9) - 1.0) ** 2
        peak_factor_penalty = peak_factor_penalty + (peak / rms - stats["peak"] / stats["rms"]) ** 2
        model_flux = torch.mean(torch.relu(model_log[:, 1:] - model_log[:, :-1]))
        flux_penalty = flux_penalty + (model_flux / max(stats["flux"], 1e-9) - 1.0) ** 2
    count = max(1, len(cases))
    total = (
        spec_loss / count
        + 0.35 * peak_penalty / count
        + 0.10 * peak_factor_penalty / count
        + 0.05 * flux_penalty / count
    )
    return total, {
        "spec": float((spec_loss / count).detach()),
        "peak": float((peak_penalty / count).detach()),
        "peak_factor": float((peak_factor_penalty / count).detach()),
        "flux": float((flux_penalty / count).detach()),
    }


def load_candidates(path: Path) -> dict[str, list[list[float]]]:
    data = json.loads(path.read_text())
    result = {}
    for candidate in data["candidates"]:
        result[candidate["name"]] = candidate["rows"]
    return result


def optimize(name: str, initial: list[list[float]], cases: list[dict[str, object]], targets: list[torch.Tensor],
             target_stats_list: list[dict[str, float]], baseline_peaks: list[float],
             case_levels: list[tuple[torch.Tensor, torch.Tensor]],
             iterations: int) -> tuple[list[list[float]], dict[str, float]]:
    phases = torch.tensor(initial, dtype=torch.float32, requires_grad=True)
    optimizer = torch.optim.Adam([phases], lr=0.03)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=iterations)
    best = phases.detach().clone()
    best_loss = float("inf")
    best_metrics: dict[str, float] = {}
    for iteration in range(iterations):
        optimizer.zero_grad()
        loss, metrics = loss_for(phases, cases, targets, target_stats_list, baseline_peaks, case_levels)
        loss.backward()
        optimizer.step()
        scheduler.step()
        if float(loss) < best_loss:
            best_loss = float(loss)
            best = phases.detach().clone()
            best_metrics = metrics
        if (iteration + 1) % 100 == 0 or iteration == 0:
            print(f"  {name} {iteration + 1}/{iterations} loss={float(loss):.5f} best={best_loss:.5f}")
    return (best % (2.0 * math.pi)).tolist(), best_metrics


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--reference-dir", type=Path, required=True)
    parser.add_argument("--candidates", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=500)
    args = parser.parse_args()

    candidates = load_candidates(args.candidates)
    notes = list(NOTE_NAMES)
    cases: list[dict[str, object]] = []
    targets: list[torch.Tensor] = []
    target_stats_list: list[dict[str, float]] = []
    case_levels: list[tuple[torch.Tensor, torch.Tensor]] = []
    for midi in notes:
        for velocity, layer in VELOCITY_LAYERS.items():
            reference = args.reference_dir / f"C{midi // 12 - 1}v{layer}.wav"
            if not reference.exists():
                raise FileNotFoundError(reference)
            case = build_case(midi, velocity)
            case["reference"] = str(reference)
            cases.append(case)
            targets.append(target_metrics(reference))
            target_stats_list.append(target_stats(reference))
            case_levels.append(case_tensors(case))

    # Baseline A peaks define the loudness penalty; phase-only optimization
    # must not buy spectral fit by cancelling the string bank.
    a_phases = torch.tensor(candidates["A"], dtype=torch.float32)
    baseline_peaks = []
    for case, levels in zip(cases, case_levels):
        level_fast, level_slow = levels
        signal = synth_case(case, a_phases, level_fast, level_slow)
        baseline_peaks.append(float(signal.abs().max()))

    print("Optimizing C1 from A")
    c1, c1_metrics = optimize(
        "C1", candidates["A"], cases, targets, target_stats_list, baseline_peaks, case_levels, args.iterations
    )
    print("Optimizing C2 from B")
    c2, c2_metrics = optimize(
        "C2", candidates["B"], cases, targets, target_stats_list, baseline_peaks, case_levels, args.iterations
    )

    candidates["C1"] = c1
    candidates["C2"] = c2
    output = {
        "source": {
            "reference": "Salamander Grand Piano V3, Alexander Holm, CC BY 3.0",
            "A": "devpiano current kOptPhaseTable row 0; upstream danielpodrazka/piano MIT",
            "B": "danielpodrazka/piano _PHASE_TABLE 3x64, MIT",
            "C1": "independently optimized against Salamander V3 48 kHz/24-bit; single row",
            "C2": "independently optimized against Salamander V3 48 kHz/24-bit; three rows",
        },
        "candidates": [{"name": name, "rows": rows} for name, rows in candidates.items()],
        "metrics": {"C1": c1_metrics, "C2": c2_metrics},
    }
    args.output.write_text(json.dumps(output, indent=2))
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

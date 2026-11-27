#!/usr/bin/python3
"""Evaluate A/B/C1/C2 phase candidates rendered by the built-in engine.

Reference: Salamander Grand Piano V3, Alexander Holm, CC BY 3.0.
Run with /usr/bin/python3 (PyTorch CPU). Development-only.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import torch

sys.path.insert(0, str(Path(__file__).parent))
from phase_optimizer import VELOCITY_LAYERS, load_wav, log_mel, target_stats  # noqa: E402


def evaluate(reference_dir: Path, experiment_dir: Path) -> dict[str, dict[str, float]]:
    manifest = json.loads((experiment_dir / "phase_experiment_manifest.json").read_text())
    rows: dict[str, list[dict[str, float]]] = {}
    for case in manifest["cases"]:
        candidate = case["candidate"]
        midi = int(case["midi"])
        velocity = int(case["velocity"])
        layer = VELOCITY_LAYERS[velocity]
        reference = reference_dir / f"C{midi // 12 - 1}v{layer}.wav"
        target_log = log_mel(torch.tensor(load_wav(reference), dtype=torch.float32))
        stats = target_stats(reference)
        actual = torch.tensor(load_wav(experiment_dir / case["file"]), dtype=torch.float32)
        actual_log = log_mel(actual)
        peak = float(actual.abs().max())
        rms = float(torch.sqrt(torch.mean(actual * actual)) + 1e-12)
        flux = float(torch.mean(torch.relu(actual_log[:, 1:] - actual_log[:, :-1])))
        rows.setdefault(candidate, []).append(
            {
                "spec": float(torch.mean((actual_log - target_log) ** 2)),
                "peak": peak,
                "peak_factor_ratio": (peak / rms) / max(stats["peak"] / stats["rms"], 1e-12),
                "flux_ratio": flux / max(stats["flux"], 1e-12),
            }
        )

    result: dict[str, dict[str, float]] = {}
    for candidate, values in sorted(rows.items()):
        count = len(values)
        result[candidate] = {
            "count": count,
            "mean_spec": sum(v["spec"] for v in values) / count,
            "mean_peak": sum(v["peak"] for v in values) / count,
            "mean_peak_factor_ratio": sum(v["peak_factor_ratio"] for v in values) / count,
            "mean_flux_ratio": sum(v["flux_ratio"] for v in values) / count,
        }
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--reference-dir", type=Path, required=True)
    parser.add_argument("--experiment-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    result = evaluate(args.reference_dir, args.experiment_dir)
    args.output.write_text(json.dumps(result, indent=2))
    for candidate, values in result.items():
        print(
            f"{candidate}: spec={values['mean_spec']:.4f} peak={values['mean_peak']:.4f} "
            f"peak_factor={values['mean_peak_factor_ratio']:.3f} flux={values['mean_flux_ratio']:.3f}"
        )
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

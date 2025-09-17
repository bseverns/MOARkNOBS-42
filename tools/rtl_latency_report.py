#!/usr/bin/env python3
"""Round-trip latency analyzer for DAW impulse captures.

This script chews through WAV files produced by the "Oblique RTL + DAW impulse"
loopback test. It spots the send/return transients, measures the delta in
samples, and spits out a nerd-friendly report (and optional JSON) that folds
right into our bench logs.

Usage example:

    python tools/rtl_latency_report.py \
        --baseline captures/baseline/*.wav \
        --tuned captures/tuned/*.wav \
        --buffers 256 64 \
        --json docs/bench/latency/rtl_latest.json

The output prints a table plus per-label stats so you can drop them straight
into `docs/bench/latency/`.
"""

from __future__ import annotations

import argparse
import dataclasses
import glob
import json
import math
import statistics
import sys
from array import array
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple
import wave


@dataclasses.dataclass
class LatencyMeasurement:
    """Container for a single file's latency calculation."""

    label: str
    path: Path
    sample_rate: float
    latency_samples: int
    latency_ms: float
    impulse_indices: Tuple[int, int]
    peak_amplitudes: Tuple[float, float]
    buffer_equivalents: Dict[str, float]

    def to_dict(self) -> Dict[str, object]:
        return {
            "label": self.label,
            "file": str(self.path),
            "sample_rate_hz": self.sample_rate,
            "latency_samples": self.latency_samples,
            "latency_ms": self.latency_ms,
            "impulse_indices": list(self.impulse_indices),
            "peak_amplitudes": list(self.peak_amplitudes),
            "buffer_equivalents": self.buffer_equivalents,
        }


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Estimate round-trip latency from impulse-response WAV captures. "
            "Feed baseline and tuned sets (or raw paths) and the script "
            "spits back per-run stats plus aggregate summaries."
        )
    )
    parser.add_argument(
        "paths",
        nargs="*",
        help="Standalone WAV paths or globs (no label applied).",
    )
    parser.add_argument(
        "--baseline",
        nargs="+",
        action="append",
        metavar="PATTERN",
        help="WAV paths or globs tagged as baseline buffer settings. Repeatable.",
    )
    parser.add_argument(
        "--tuned",
        nargs="+",
        action="append",
        metavar="PATTERN",
        help="WAV paths or globs tagged as tuned buffer settings. Repeatable.",
    )
    parser.add_argument(
        "--label",
        nargs=2,
        action="append",
        metavar=("NAME", "PATTERN"),
        help=(
            "Custom label + glob pair. Use multiple times for additional "
            "groupings when baseline/tuned isn't enough."
        ),
    )
    parser.add_argument(
        "--buffers",
        nargs="*",
        metavar="NAME=SIZE",
        help=(
            "Optional buffer definitions reported in the output. Format as "
            "label=size (e.g. baseline=256 tuned=64 daw=128)."
        ),
    )
    parser.add_argument(
        "--min-gap-ms",
        type=float,
        default=0.5,
        help="Minimum separation between impulses in milliseconds (default: 0.5).",
    )
    parser.add_argument(
        "--peak-window-ms",
        type=float,
        default=0.4,
        help="Window for local peak refinement around threshold crossings.",
    )
    parser.add_argument(
        "--threshold-ratio",
        type=float,
        default=0.18,
        help=(
            "Fraction of the strongest peak used as the detection threshold. "
            "Default 0.18 keeps quiet returns visible even when the send spike "
            "is loud."
        ),
    )
    parser.add_argument(
        "--json",
        type=Path,
        help="Optional path to dump machine-readable results.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress the table output (useful when only JSON matters).",
    )
    return parser.parse_args(argv)


def expand_patterns(patterns: Iterable[str]) -> List[Path]:
    """Expand glob patterns into real file paths, preserving order."""

    results: List[Path] = []
    for pattern in patterns:
        matches = [Path(p) for p in glob.glob(pattern)]
        if not matches:
            candidate = Path(pattern)
            if candidate.exists():
                matches = [candidate]
            else:
                raise FileNotFoundError(f"No files matched pattern: {pattern}")
        matches.sort()
        results.extend(matches)
    return results


def decode_wav(path: Path) -> Tuple[List[float], float]:
    """Load a WAV file and return mono samples normalised to [-1, 1]."""

    with wave.open(str(path), "rb") as wav_file:
        sample_rate = float(wav_file.getframerate())
        n_channels = wav_file.getnchannels()
        sampwidth = wav_file.getsampwidth()
        n_frames = wav_file.getnframes()
        raw = wav_file.readframes(n_frames)

    if sampwidth == 1:
        data = array("B")
        data.frombytes(raw)
        # Unsigned 8-bit PCM. Shift to signed range.
        samples = [(x - 128) / 128.0 for x in data]
    elif sampwidth == 2:
        data = array("h")
        data.frombytes(raw)
        samples = [x / 32768.0 for x in data]
    elif sampwidth == 3:
        samples = _decode_24_bit(raw)
    elif sampwidth == 4:
        samples = _decode_32_bit(raw)
    else:
        raise ValueError(f"Unsupported sample width ({sampwidth * 8} bits) in {path}")

    if n_channels == 1:
        mono = samples
    else:
        mono = _fold_down(samples, n_channels)

    return mono, sample_rate


def _decode_24_bit(raw: bytes) -> List[float]:
    if len(raw) % 3:
        raise ValueError("24-bit PCM chunk is misaligned; corrupt file?")
    out: List[float] = []
    for i in range(0, len(raw), 3):
        chunk = raw[i : i + 3]
        value = chunk[0] | (chunk[1] << 8) | (chunk[2] << 16)
        if value & 0x800000:
            value -= 0x1000000
        out.append(value / 8388608.0)
    return out


def _decode_32_bit(raw: bytes) -> List[float]:
    # Try float32 first; fall back to signed PCM if the amplitudes look bonkers.
    float_data = array("f")
    try:
        float_data.frombytes(raw)
    except OverflowError as exc:
        raise ValueError("32-bit PCM chunk is malformed") from exc

    # If everything stays within ±1.5 we're comfortable treating it as float32.
    if float_data and max(abs(v) for v in float_data) <= 1.5:
        return list(float_data)

    int_data = array("i")
    int_data.frombytes(raw)
    return [value / 2147483648.0 for value in int_data]


def _fold_down(samples: Sequence[float], channels: int) -> List[float]:
    folded: List[float] = []
    for i in range(0, len(samples), channels):
        frame = samples[i : i + channels]
        folded.append(sum(frame) / float(channels))
    return folded


def find_impulses(
    samples: Sequence[float],
    threshold_ratio: float,
    min_gap_ms: float,
    sample_rate: float,
    peak_window_ms: float,
) -> Tuple[Tuple[int, int], Tuple[float, float]]:
    """Locate the send and return impulses.

    Returns the indices and amplitudes of the two strongest impulses separated by
    at least the requested gap. Raises a ValueError if we fail to find two
    candidates.
    """

    if not samples:
        raise ValueError("Empty sample buffer")

    absolute = [abs(x) for x in samples]
    peak_value = max(absolute)
    if peak_value <= sys.float_info.min:
        raise ValueError("Signal appears silent; cannot locate impulses")

    threshold = peak_value * threshold_ratio
    min_gap = max(1, int(sample_rate * (min_gap_ms / 1000.0)))
    refine = max(1, int(sample_rate * (peak_window_ms / 1000.0)))

    candidates: List[Tuple[float, int]] = []
    for index in range(1, len(absolute) - 1):
        value = absolute[index]
        if value < threshold:
            continue
        if value >= absolute[index - 1] and value > absolute[index + 1]:
            candidates.append((value, index))

    # Fallback: if no local maxima crossed the threshold, use the global top bins.
    if not candidates:
        candidates = sorted(
            ((value, idx) for idx, value in enumerate(absolute)),
            reverse=True,
        )

    picked: List[Tuple[float, int]] = []
    for value, index in sorted(candidates, reverse=True):
        if all(abs(index - other_idx) >= min_gap for _, other_idx in picked):
            # Re-scan around the candidate to refine the exact peak location.
            start = max(0, index - refine)
            end = min(len(samples), index + refine)
            local_index = max(
                range(start, end), key=lambda i: abs(samples[i])
            )
            picked.append((abs(samples[local_index]), local_index))
        if len(picked) == 2:
            break

    if len(picked) < 2:
        raise ValueError("Could not locate two distinct impulses. Try lowering --threshold-ratio.")

    picked.sort(key=lambda item: item[1])
    indices = (picked[0][1], picked[1][1])
    amplitudes = (picked[0][0], picked[1][0])
    return indices, amplitudes


def calculate_latency(
    label: str,
    path: Path,
    buffers: Dict[str, int],
    threshold_ratio: float,
    min_gap_ms: float,
    peak_window_ms: float,
) -> LatencyMeasurement:
    samples, sample_rate = decode_wav(path)
    (first, second), amplitudes = find_impulses(
        samples=samples,
        threshold_ratio=threshold_ratio,
        min_gap_ms=min_gap_ms,
        sample_rate=sample_rate,
        peak_window_ms=peak_window_ms,
    )

    latency_samples = second - first
    latency_ms = (latency_samples / sample_rate) * 1000.0
    buffer_equivalents = {
        name: latency_samples / size for name, size in buffers.items()
    }

    return LatencyMeasurement(
        label=label,
        path=path,
        sample_rate=sample_rate,
        latency_samples=latency_samples,
        latency_ms=latency_ms,
        impulse_indices=(first, second),
        peak_amplitudes=amplitudes,
        buffer_equivalents=buffer_equivalents,
    )


def gather_sets(args: argparse.Namespace) -> List[LatencyMeasurement]:
    buffers = parse_buffers(args.buffers or [])

    worklist: List[Tuple[str, Path]] = []
    for pattern in args.paths:
        for path in expand_patterns([pattern]):
            worklist.append(("run", path))

    def _append(label: str, raw_patterns: Sequence[Sequence[str]] | None):
        if not raw_patterns:
            return
        for pattern_group in raw_patterns:
            for path in expand_patterns(pattern_group):
                worklist.append((label, path))

    _append("baseline", args.baseline)
    _append("tuned", args.tuned)

    if args.label:
        for label, pattern in args.label:
            for path in expand_patterns([pattern]):
                worklist.append((label, path))

    if not worklist:
        raise SystemExit("No input files provided. Pass paths or --baseline/--tuned globs.")

    measurements: List[LatencyMeasurement] = []
    for label, path in worklist:
        measurement = calculate_latency(
            label=label,
            path=path,
            buffers=buffers,
            threshold_ratio=args.threshold_ratio,
            min_gap_ms=args.min_gap_ms,
            peak_window_ms=args.peak_window_ms,
        )
        measurements.append(measurement)
    return measurements


def parse_buffers(raw_buffers: Sequence[str]) -> Dict[str, int]:
    parsed: Dict[str, int] = {}
    for item in raw_buffers:
        if "=" not in item:
            raise ValueError("Buffers must be NAME=SIZE (e.g. baseline=256)")
        name, size_str = item.split("=", 1)
        try:
            size = int(size_str)
        except ValueError as exc:
            raise ValueError(f"Buffer size must be integer, got {size_str}") from exc
        if size <= 0:
            raise ValueError("Buffer size must be positive")
        parsed[name] = size
    return parsed


def summarise(measurements: List[LatencyMeasurement]) -> Dict[str, Dict[str, float]]:
    grouped: Dict[str, List[LatencyMeasurement]] = {}
    for measurement in measurements:
        grouped.setdefault(measurement.label, []).append(measurement)

    summary: Dict[str, Dict[str, float]] = {}
    for label, runs in grouped.items():
        latencies = [m.latency_ms for m in runs]
        latency_samples = [m.latency_samples for m in runs]
        latencies_sorted = sorted(latencies)
        summary[label] = {
            "count": len(runs),
            "min_ms": min(latencies),
            "max_ms": max(latencies),
            "median_ms": statistics.median(latencies),
            "mean_ms": statistics.fmean(latencies),
            "p95_ms": percentile(latencies_sorted, 95),
            "min_samples": min(latency_samples),
            "max_samples": max(latency_samples),
        }
        if len(latencies) > 1:
            summary[label]["stdev_ms"] = statistics.pstdev(latencies)
    return summary


def percentile(sorted_values: Sequence[float], percentile_value: float) -> float:
    if not sorted_values:
        raise ValueError("Cannot compute percentile of empty list")
    if len(sorted_values) == 1:
        return sorted_values[0]
    rank = (len(sorted_values) - 1) * (percentile_value / 100.0)
    lower = math.floor(rank)
    upper = math.ceil(rank)
    if lower == upper:
        return sorted_values[int(rank)]
    lower_value = sorted_values[lower]
    upper_value = sorted_values[upper]
    return lower_value + (upper_value - lower_value) * (rank - lower)


def render_table(measurements: Sequence[LatencyMeasurement]) -> str:
    buffer_names: List[str] = []
    for measurement in measurements:
        for key in measurement.buffer_equivalents:
            if key not in buffer_names:
                buffer_names.append(key)

    headers = [
        "Label",
        "Latency (ms)",
        "Latency (samples)",
        "Sample rate",
    ]
    headers.extend(f"~{name} buffers" for name in buffer_names)
    headers.append("File")

    rows: List[List[str]] = [headers]
    for measurement in sorted(measurements, key=lambda m: (m.label, m.latency_ms)):
        row = [
            measurement.label,
            f"{measurement.latency_ms:8.3f}",
            str(measurement.latency_samples),
            f"{measurement.sample_rate:7.1f} Hz",
        ]
        for name in buffer_names:
            value = measurement.buffer_equivalents.get(name)
            row.append(f"{value:6.2f}" if value is not None else "   n/a")
        row.append(str(measurement.path))
        rows.append(row)

    # Calculate column widths.
    widths = [max(len(row[i]) for row in rows) for i in range(len(headers))]
    lines = []
    for idx, row in enumerate(rows):
        padded = [cell.ljust(widths[i]) for i, cell in enumerate(row)]
        line = "  ".join(padded)
        if idx == 0:
            lines.append(line)
            lines.append("  ".join("-" * width for width in widths))
        else:
            lines.append(line)
    return "\n".join(lines)


def render_summary(summary: Dict[str, Dict[str, float]]) -> str:
    lines = ["", "Summary by label:"]
    for label, stats in sorted(summary.items()):
        lines.append(f"  {label}:")
        lines.append(
            "    runs={count} | median={median_ms:.3f} ms | mean={mean_ms:.3f} ms | "
            "p95={p95_ms:.3f} ms".format(**stats)
        )
        lines.append(
            "    min/max={min_ms:.3f}/{max_ms:.3f} ms | samples={min_samples}/{max_samples}".format(
                **stats
            )
        )
        if "stdev_ms" in stats:
            lines.append(f"    σ={stats['stdev_ms']:.3f} ms")
    return "\n".join(lines)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    try:
        measurements = gather_sets(args)
    except Exception as exc:  # pragma: no cover - CLI guardrail
        raise SystemExit(str(exc)) from exc

    summary = summarise(measurements)

    if not args.quiet:
        print(render_table(measurements))
        print(render_summary(summary))

    if args.json:
        payload = {
            "measurements": [m.to_dict() for m in measurements],
            "summary": summary,
        }
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    return 0


if __name__ == "__main__":  # pragma: no cover - CLI entrypoint
    raise SystemExit(main())


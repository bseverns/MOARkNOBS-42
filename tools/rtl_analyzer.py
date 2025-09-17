#!/usr/bin/env python3
"""Round-trip latency analyzer for DAW loopback captures.

This script chews on impulse-response WAV files produced by the
"Oblique RTL + DAW impulse" loopback method. It hunts for the outbound
and return spikes, estimates the round-trip latency, and rolls the
results into terse summaries that can gate tests.

The implementation leans on Python's standard library so you can run it
anywhere PlatformIO runs—no numpy, no SciPy, no drama.
"""
from __future__ import annotations

import argparse
import audioop
import json
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple
import wave

# ---------------------------------------------------------------------------
# Data containers
# ---------------------------------------------------------------------------


@dataclass
class PeakPair:
    """Capture the send/return peaks we care about."""

    first_index: int
    second_index: int
    sample_rate: float
    amplitude: float

    @property
    def latency_samples(self) -> int:
        return self.second_index - self.first_index

    @property
    def latency_ms(self) -> float:
        return self.latency_samples / self.sample_rate * 1000.0


@dataclass
class FileReport:
    """Full report for a single WAV capture."""

    path: Path
    peak_pair: PeakPair
    channels: int
    sample_width: int

    @property
    def latency_ms(self) -> float:
        return self.peak_pair.latency_ms


# ---------------------------------------------------------------------------
# WAV ingestion utilities
# ---------------------------------------------------------------------------


def _load_wav(path: Path) -> Tuple[List[float], int, int, int]:
    """Return mono samples, sample rate, channel count, and sample width."""

    with wave.open(str(path), "rb") as wav_file:
        sample_rate = wav_file.getframerate()
        channels = wav_file.getnchannels()
        sample_width = wav_file.getsampwidth()
        frame_count = wav_file.getnframes()
        raw = wav_file.readframes(frame_count)

    if channels > 1:
        raw = audioop.tomono(raw, sample_width, 0.5, 0.5)
        channels = 1

    # Convert to 32-bit signed integers for consistent scaling.
    converted = audioop.lin2lin(raw, sample_width, 4)
    sample_total = len(converted) // 4
    samples = list(audioop.getsample(converted, 4, i) for i in range(sample_total))

    # Normalize to +/- 1.0
    scale = float(1 << (8 * 4 - 1))
    normalized = [sample / scale for sample in samples]
    return normalized, sample_rate, channels, sample_width


# ---------------------------------------------------------------------------
# Peak hunting
# ---------------------------------------------------------------------------


def _find_peak_pair(
    samples: Sequence[float],
    sample_rate: float,
    min_gap_ms: float,
    threshold_ratio: float,
) -> PeakPair:
    """Locate the two loudest, well-separated peaks in the signal."""

    if not samples:
        raise ValueError("WAV file had no PCM samples")

    max_amp = max(abs(value) for value in samples)
    if max_amp == 0:
        raise ValueError("Impulse capture appears to be silent")

    threshold = max_amp * threshold_ratio
    min_gap_samples = max(1, int(sample_rate * min_gap_ms / 1000.0))

    candidate_indices: List[int] = []
    index = 0
    total = len(samples)

    while index < total:
        value = samples[index]
        if abs(value) >= threshold:
            # Search a small neighborhood around the trigger point so we land on the
            # actual peak rather than the first crossing of the threshold.
            search_end = min(total, index + min_gap_samples // 2 + 1)
            search_slice = samples[index:search_end]
            local_offset = max(range(len(search_slice)), key=lambda i: abs(search_slice[i]))
            candidate = index + local_offset
            if candidate_indices and candidate - candidate_indices[-1] < min_gap_samples:
                # Keep the larger of the two peaks if they overlap.
                previous = candidate_indices[-1]
                if abs(samples[candidate]) > abs(samples[previous]):
                    candidate_indices[-1] = candidate
            else:
                candidate_indices.append(candidate)
            index = candidate + min_gap_samples
        else:
            index += 1

    if len(candidate_indices) < 2:
        raise ValueError(
            "Could not find two well-separated peaks. Try lowering --threshold or --min-gap-ms."
        )

    first, second = candidate_indices[:2]
    return PeakPair(
        first_index=first,
        second_index=second,
        sample_rate=float(sample_rate),
        amplitude=max_amp,
    )


# ---------------------------------------------------------------------------
# Reporting helpers
# ---------------------------------------------------------------------------


def _collect_files(paths: Iterable[Path]) -> List[Path]:
    files: List[Path] = []
    for entry in paths:
        if entry.is_dir():
            files.extend(sorted(p for p in entry.rglob("*.wav") if p.is_file()))
        elif entry.suffix.lower() == ".wav" and entry.is_file():
            files.append(entry)
        else:
            raise FileNotFoundError(f"No WAV file at {entry}")
    if not files:
        raise FileNotFoundError("No WAV files discovered for a group")
    return files


def _parse_group_specs(specs: Iterable[str]) -> Dict[str, List[Path]]:
    groups: Dict[str, List[Path]] = {}
    for raw in specs:
        if "=" not in raw:
            raise ValueError(f"Group spec '{raw}' must be LABEL=PATH[,PATH...] format")
        label, remainder = raw.split("=", 1)
        label = label.strip()
        if not label:
            raise ValueError("Group label may not be empty")
        path_tokens = [token.strip() for token in remainder.split(",") if token.strip()]
        if not path_tokens:
            raise ValueError(f"Group '{label}' did not include any paths")
        paths = [Path(token) for token in path_tokens]
        groups[label] = _collect_files(paths)
    return groups


def _summarize_group(reports: Sequence[FileReport]) -> Dict[str, float]:
    latencies = [report.latency_ms for report in reports]
    return {
        "count": len(latencies),
        "min_ms": min(latencies),
        "max_ms": max(latencies),
        "median_ms": statistics.median(latencies),
        "mean_ms": statistics.fmean(latencies),
        "stdev_ms": statistics.pstdev(latencies) if len(latencies) > 1 else 0.0,
    }


def _format_ms(value: float) -> str:
    return f"{value:.3f} ms"


def _render_console_summary(
    group_reports: Dict[str, List[FileReport]],
    buffer_sizes: Dict[str, int],
) -> None:
    for label, reports in group_reports.items():
        summary = _summarize_group(reports)
        sample_rate = reports[0].peak_pair.sample_rate
        buffer = buffer_sizes.get(label)
        theoretical = (
            2 * buffer / sample_rate * 1000.0 if buffer and sample_rate else None
        )

        print(f"\n== {label.upper()} ==")
        print(f"Files analyzed: {summary['count']}")
        print(f"Latency median: {_format_ms(summary['median_ms'])}")
        print(f"Latency mean:   {_format_ms(summary['mean_ms'])}")
        print(f"Latency stdev:  {_format_ms(summary['stdev_ms'])}")
        print(f"Latency min:    {_format_ms(summary['min_ms'])}")
        print(f"Latency max:    {_format_ms(summary['max_ms'])}")
        if theoretical is not None:
            delta = summary["median_ms"] - theoretical
            print(
                f"Buffer @ {buffer} frames predicts ~{_format_ms(theoretical)} round trip (Δ={delta:+.3f} ms)"
            )

        for report in reports:
            pair = report.peak_pair
            print(
                f"  - {report.path}: {_format_ms(pair.latency_ms)} (samples {pair.first_index}→{pair.second_index})"
            )


def _export_json(
    destination: Path,
    group_reports: Dict[str, List[FileReport]],
    buffer_sizes: Dict[str, int],
) -> None:
    payload = {}
    for label, reports in group_reports.items():
        payload[label] = {
            "summary": _summarize_group(reports),
            "files": [
                {
                    "path": str(report.path),
                    "latency_ms": report.latency_ms,
                    "latency_samples": report.peak_pair.latency_samples,
                    "sample_rate": report.peak_pair.sample_rate,
                    "amplitude": report.peak_pair.amplitude,
                    "channels": report.channels,
                    "sample_width_bytes": report.sample_width,
                }
                for report in reports
            ],
        }
        if label in buffer_sizes:
            payload[label]["buffer_frames"] = buffer_sizes[label]
    destination.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _export_markdown(
    destination: Path,
    group_reports: Dict[str, List[FileReport]],
    buffer_sizes: Dict[str, int],
) -> None:
    lines: List[str] = []
    lines.append("# Round-Trip Latency Summary\n")
    for label, reports in group_reports.items():
        summary = _summarize_group(reports)
        lines.append(f"## {label}\n")
        lines.append("| Metric | Value |\n")
        lines.append("| --- | --- |\n")
        lines.append(f"| Files | {summary['count']} |\n")
        lines.append(f"| Median | {_format_ms(summary['median_ms'])} |\n")
        lines.append(f"| Mean | {_format_ms(summary['mean_ms'])} |\n")
        lines.append(f"| Std Dev | {_format_ms(summary['stdev_ms'])} |\n")
        lines.append(f"| Min | {_format_ms(summary['min_ms'])} |\n")
        lines.append(f"| Max | {_format_ms(summary['max_ms'])} |\n")
        buffer = buffer_sizes.get(label)
        if buffer is not None:
            sample_rate = reports[0].peak_pair.sample_rate
            theoretical = 2 * buffer / sample_rate * 1000.0
            lines.append(
                f"| Buffer theory ({buffer} frames) | {_format_ms(theoretical)} |\n"
            )
        lines.append("\n")
        lines.append("### Captures\n")
        lines.append("| File | Latency (ms) | Sample Rate | Peak Amplitude |\n")
        lines.append("| --- | --- | --- | --- |\n")
        for report in reports:
            pair = report.peak_pair
            lines.append(
                f"| `{report.path}` | {pair.latency_ms:.3f} | {pair.sample_rate:.1f} | {pair.amplitude:.3f} |\n"
            )
        lines.append("\n")
    destination.write_text("".join(lines), encoding="utf-8")


# ---------------------------------------------------------------------------
# CLI plumbing
# ---------------------------------------------------------------------------


def _parse_buffer_sizes(raw_specs: Iterable[str]) -> Dict[str, int]:
    buffers: Dict[str, int] = {}
    for spec in raw_specs:
        if "=" not in spec:
            raise ValueError(f"Buffer spec '{spec}' must be LABEL=FRAMES")
        label, frames = spec.split("=", 1)
        label = label.strip()
        frames = frames.strip()
        if not frames.isdigit():
            raise ValueError(f"Buffer size '{frames}' for label '{label}' is not an integer")
        buffers[label] = int(frames)
    return buffers


def _build_reports(
    groups: Dict[str, List[Path]],
    min_gap_ms: float,
    threshold: float,
) -> Dict[str, List[FileReport]]:
    reports: Dict[str, List[FileReport]] = {}
    for label, files in groups.items():
        file_reports: List[FileReport] = []
        for path in files:
            samples, sample_rate, channels, sample_width = _load_wav(path)
            peak_pair = _find_peak_pair(samples, sample_rate, min_gap_ms, threshold)
            file_reports.append(
                FileReport(
                    path=path,
                    peak_pair=peak_pair,
                    channels=channels,
                    sample_width=sample_width,
                )
            )
        reports[label] = sorted(file_reports, key=lambda report: report.latency_ms)
    return reports


def _check_thresholds(reports: Dict[str, List[FileReport]], max_latency: float) -> None:
    offenders: List[Tuple[str, FileReport]] = []
    for label, file_reports in reports.items():
        for report in file_reports:
            if report.latency_ms > max_latency:
                offenders.append((label, report))
    if offenders:
        lines = [
            "Round-trip latency exceeded the threshold ({} ms):".format(max_latency)
        ]
        for label, report in offenders:
            lines.append(
                f"  - {label}: {report.path} = {report.latency_ms:.3f} ms"
            )
        raise SystemExit("\n".join(lines))


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Estimate round-trip latency from loopback impulse captures.",
    )
    parser.add_argument(
        "--group",
        dest="group_specs",
        action="append",
        metavar="LABEL=PATH[,PATH...]",
        help="Label a set of WAV files for analysis. Repeat for baseline/tuned runs.",
    )
    parser.add_argument(
        "--min-gap-ms",
        type=float,
        default=1.0,
        help="Minimum separation between peaks (default: 1.0 ms).",
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=0.6,
        help="Fraction of the max amplitude that counts as a peak (default: 0.6).",
    )
    parser.add_argument(
        "--buffer",
        dest="buffers",
        action="append",
        metavar="LABEL=FRAMES",
        default=[],
        help="Expected buffer size in frames per group (repeatable).",
    )
    parser.add_argument(
        "--max-ms",
        type=float,
        default=None,
        help="Fail if any capture reports latency above this value (ms).",
    )
    parser.add_argument(
        "--export-json",
        type=Path,
        help="Write a machine-friendly JSON summary to this path.",
    )
    parser.add_argument(
        "--export-markdown",
        type=Path,
        help="Write a Markdown report for logs or docs.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress console tables (still honors export flags).",
    )
    args = parser.parse_args(argv)

    if not args.group_specs:
        parser.error("At least one --group must be supplied")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])

    try:
        groups = _parse_group_specs(args.group_specs)
        buffer_sizes = _parse_buffer_sizes(args.buffers)
        group_reports = _build_reports(groups, args.min_gap_ms, args.threshold)
    except Exception as error:  # pragma: no cover - CLI surface
        raise SystemExit(str(error))

    if args.max_ms is not None:
        _check_thresholds(group_reports, args.max_ms)

    if not args.quiet:
        _render_console_summary(group_reports, buffer_sizes)

    if args.export_json:
        _export_json(args.export_json, group_reports, buffer_sizes)
    if args.export_markdown:
        _export_markdown(args.export_markdown, group_reports, buffer_sizes)

    return 0


if __name__ == "__main__":  # pragma: no cover - CLI entry
    raise SystemExit(main())

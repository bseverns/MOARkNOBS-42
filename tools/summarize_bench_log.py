#!/usr/bin/env python3
"""Summarize MN42 bench CSV captures into a compact human-readable report."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import pathlib
import statistics
import sys
from collections import defaultdict
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Sequence


@dataclass
class Row:
    record_type: str
    phase: str
    mode: str
    lock: str
    host_ts_iso: str
    host_ts_epoch_ms: int
    uptime_ms: Optional[int]
    brightness: Optional[float]
    brownouts: Optional[int]
    fps: Optional[float]
    writes_per_sec: Optional[float]
    passes_per_sec: Optional[float]
    peak_fps: Optional[float]
    peak_writes_per_sec: Optional[float]
    peak_passes_per_sec: Optional[float]
    vref: Optional[float]
    vref_min: Optional[float]
    vref_max: Optional[float]
    frames: Optional[int]
    phase_frames: Optional[int]
    pixel_writes: Optional[int]
    random_writes: Optional[int]
    passes: Optional[int]
    random_passes: Optional[int]
    max_loop_us: Optional[int]
    midi_drops: Optional[int]
    loop_overruns: Optional[int]
    reset: str
    raw_line: str


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize a CSV produced by tools/capture_bench_log.py"
    )
    parser.add_argument("csv_path", type=pathlib.Path, help="Bench CSV to summarize.")
    parser.add_argument(
        "--format",
        choices=("text", "markdown"),
        default="text",
        help="Output format. Default: text",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=None,
        help="Optional destination file. Defaults to stdout.",
    )
    return parser.parse_args(list(argv) if argv is not None else None)


def parse_int(value: str) -> Optional[int]:
    value = value.strip()
    if not value:
        return None
    try:
        return int(float(value))
    except ValueError:
        return None


def parse_float(value: str) -> Optional[float]:
    value = value.strip()
    if not value:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def load_rows(path: pathlib.Path) -> List[Row]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        rows: List[Row] = []
        for raw in reader:
            rows.append(
                Row(
                    record_type=raw.get("record_type", ""),
                    phase=raw.get("phase", ""),
                    mode=raw.get("mode", ""),
                    lock=raw.get("lock", ""),
                    host_ts_iso=raw.get("host_ts_iso", ""),
                    host_ts_epoch_ms=parse_int(raw.get("host_ts_epoch_ms", "")) or 0,
                    uptime_ms=parse_int(raw.get("uptime_ms", "")),
                    brightness=parse_float(raw.get("brightness", "")),
                    brownouts=parse_int(raw.get("brownouts", "")),
                    fps=parse_float(raw.get("fps", "")),
                    writes_per_sec=parse_float(raw.get("writes_per_sec", "")),
                    passes_per_sec=parse_float(raw.get("passes_per_sec", "")),
                    peak_fps=parse_float(raw.get("peak_fps", "")),
                    peak_writes_per_sec=parse_float(raw.get("peak_writes_per_sec", "")),
                    peak_passes_per_sec=parse_float(raw.get("peak_passes_per_sec", "")),
                    vref=parse_float(raw.get("vref", "")),
                    vref_min=parse_float(raw.get("vref_min", "")),
                    vref_max=parse_float(raw.get("vref_max", "")),
                    frames=parse_int(raw.get("frames", "")),
                    phase_frames=parse_int(raw.get("phase_frames", "")),
                    pixel_writes=parse_int(raw.get("pixel_writes", "")),
                    random_writes=parse_int(raw.get("random_writes", "")),
                    passes=parse_int(raw.get("passes", "")),
                    random_passes=parse_int(raw.get("random_passes", "")),
                    max_loop_us=parse_int(raw.get("max_loop_us", "")),
                    midi_drops=parse_int(raw.get("midi_drops", "")),
                    loop_overruns=parse_int(raw.get("loop_overruns", "")),
                    reset=raw.get("reset", ""),
                    raw_line=raw.get("raw_line", ""),
                )
            )
    return rows


def mean_or_blank(values: Iterable[Optional[float]]) -> str:
    usable = [value for value in values if value is not None]
    if not usable:
        return "-"
    return f"{statistics.fmean(usable):.1f}"


def max_or_blank(values: Iterable[Optional[float]]) -> str:
    usable = [value for value in values if value is not None]
    if not usable:
        return "-"
    return f"{max(usable):.1f}"


def min_or_blank(values: Iterable[Optional[float]]) -> str:
    usable = [value for value in values if value is not None]
    if not usable:
        return "-"
    return f"{min(usable):.3f}" if any(v < 10 for v in usable) else f"{min(usable):.1f}"


def max_int_or_blank(values: Iterable[Optional[int]]) -> str:
    usable = [value for value in values if value is not None]
    if not usable:
        return "-"
    return str(max(usable))


def last_nonempty(values: Iterable[str]) -> str:
    last = ""
    for value in values:
        if value:
            last = value
    return last or "-"


def duration_seconds(rows: List[Row]) -> float:
    if len(rows) < 2:
        return 0.0
    start = min(row.host_ts_epoch_ms for row in rows)
    end = max(row.host_ts_epoch_ms for row in rows)
    return max(0.0, (end - start) / 1000.0)


def summarize_phase_rows(rows: List[Row]) -> Dict[str, str]:
    return {
        "samples": str(len(rows)),
        "duration_s": f"{duration_seconds(rows):.1f}",
        "mode": last_nonempty(row.mode for row in rows),
        "lock": last_nonempty(row.lock for row in rows),
        "brightness_min": min_or_blank(row.brightness for row in rows),
        "brightness_max": max_or_blank(row.brightness for row in rows),
        "vref_min": min_or_blank(
            value for row in rows for value in (row.vref_min, row.vref) if value is not None
        ),
        "vref_max": max_or_blank(
            value for row in rows for value in (row.vref_max, row.vref) if value is not None
        ),
        "fps_avg": mean_or_blank(row.fps for row in rows),
        "fps_peak": max_or_blank(
            value for row in rows for value in (row.peak_fps, row.fps) if value is not None
        ),
        "writes_avg": mean_or_blank(row.writes_per_sec for row in rows),
        "writes_peak": max_or_blank(
            value
            for row in rows
            for value in (row.peak_writes_per_sec, row.writes_per_sec)
            if value is not None
        ),
        "passes_avg": mean_or_blank(row.passes_per_sec for row in rows),
        "passes_peak": max_or_blank(
            value
            for row in rows
            for value in (row.peak_passes_per_sec, row.passes_per_sec)
            if value is not None
        ),
        "brownouts_max": max_int_or_blank(row.brownouts for row in rows),
        "loop_overruns_max": max_int_or_blank(row.loop_overruns for row in rows),
        "midi_drops_max": max_int_or_blank(row.midi_drops for row in rows),
        "max_loop_us": max_int_or_blank(row.max_loop_us for row in rows),
    }


def build_report(rows: List[Row], source: pathlib.Path, *, markdown: bool) -> str:
    if not rows:
        return "No structured rows found in capture.\n"

    by_phase: Dict[str, List[Row]] = defaultdict(list)
    for row in rows:
        by_phase[row.phase or "unknown"].append(row)

    overall = summarize_phase_rows(rows)
    started = min(row.host_ts_iso for row in rows)
    ended = max(row.host_ts_iso for row in rows)
    record_types = ", ".join(sorted({row.record_type for row in rows if row.record_type}))
    resets = ", ".join(sorted({row.reset for row in rows if row.reset})) or "-"

    if markdown:
        lines = [
            "# Bench Summary",
            "",
            f"- Source: `{source}`",
            f"- Rows: `{len(rows)}`",
            f"- Record types: `{record_types or '-'}`",
            f"- Started: `{started}`",
            f"- Ended: `{ended}`",
            f"- Duration: `{overall['duration_s']} s`",
            f"- Reset causes seen: `{resets}`",
            "",
            "## Overall",
            "",
            f"- VREF min/max: `{overall['vref_min']} / {overall['vref_max']}`",
            f"- Avg/peak FPS: `{overall['fps_avg']} / {overall['fps_peak']}`",
            f"- Avg/peak writes/sec: `{overall['writes_avg']} / {overall['writes_peak']}`",
            f"- Avg/peak passes/sec: `{overall['passes_avg']} / {overall['passes_peak']}`",
            f"- Max brownouts: `{overall['brownouts_max']}`",
            f"- Max loop overruns: `{overall['loop_overruns_max']}`",
            f"- Max MIDI drops: `{overall['midi_drops_max']}`",
            f"- Max loop time (us): `{overall['max_loop_us']}`",
            "",
            "## Per Phase",
            "",
            "| Phase | Samples | Duration s | Mode | Lock | VREF min | VREF max | Avg FPS | Peak FPS | Avg writes/s | Peak writes/s | Brownouts max | Max loop us |",
            "|-------|---------|------------|------|------|----------|----------|---------|----------|--------------|---------------|---------------|-------------|",
        ]
        for phase in sorted(by_phase):
            summary = summarize_phase_rows(by_phase[phase])
            lines.append(
                f"| `{phase}` | {summary['samples']} | {summary['duration_s']} | {summary['mode']} | "
                f"{summary['lock']} | {summary['vref_min']} | {summary['vref_max']} | "
                f"{summary['fps_avg']} | {summary['fps_peak']} | {summary['writes_avg']} | "
                f"{summary['writes_peak']} | {summary['brownouts_max']} | {summary['max_loop_us']} |"
            )
        lines.append("")
        return "\n".join(lines)

    lines = [
        "Bench Summary",
        f"source: {source}",
        f"rows: {len(rows)}",
        f"record_types: {record_types or '-'}",
        f"started: {started}",
        f"ended: {ended}",
        f"duration_s: {overall['duration_s']}",
        f"reset_causes: {resets}",
        "",
        "Overall",
        f"  vref_min: {overall['vref_min']}",
        f"  vref_max: {overall['vref_max']}",
        f"  fps_avg: {overall['fps_avg']}",
        f"  fps_peak: {overall['fps_peak']}",
        f"  writes_avg: {overall['writes_avg']}",
        f"  writes_peak: {overall['writes_peak']}",
        f"  passes_avg: {overall['passes_avg']}",
        f"  passes_peak: {overall['passes_peak']}",
        f"  brownouts_max: {overall['brownouts_max']}",
        f"  loop_overruns_max: {overall['loop_overruns_max']}",
        f"  midi_drops_max: {overall['midi_drops_max']}",
        f"  max_loop_us: {overall['max_loop_us']}",
        "",
        "Per phase",
    ]
    for phase in sorted(by_phase):
        summary = summarize_phase_rows(by_phase[phase])
        lines.extend(
            [
                f"  {phase}",
                f"    samples: {summary['samples']}",
                f"    duration_s: {summary['duration_s']}",
                f"    mode: {summary['mode']}",
                f"    lock: {summary['lock']}",
                f"    vref_min: {summary['vref_min']}",
                f"    vref_max: {summary['vref_max']}",
                f"    fps_avg: {summary['fps_avg']}",
                f"    fps_peak: {summary['fps_peak']}",
                f"    writes_avg: {summary['writes_avg']}",
                f"    writes_peak: {summary['writes_peak']}",
                f"    brownouts_max: {summary['brownouts_max']}",
                f"    max_loop_us: {summary['max_loop_us']}",
            ]
        )
    lines.append("")
    return "\n".join(lines)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    rows = load_rows(args.csv_path)
    report = build_report(rows, args.csv_path, markdown=args.format == "markdown")

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    else:
        sys.stdout.write(report)
        if not report.endswith("\n"):
            sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

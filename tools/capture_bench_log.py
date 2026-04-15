#!/usr/bin/env python3
"""Capture MN42 bench telemetry from a serial port into raw and CSV logs."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import pathlib
import shlex
import sys
import time
from typing import Dict, Iterable, Optional


CSV_COLUMNS = [
    "host_ts_iso",
    "host_ts_epoch_ms",
    "label",
    "port",
    "baud",
    "record_type",
    "phase",
    "mode",
    "lock",
    "uptime_ms",
    "brightness",
    "brownouts",
    "fps",
    "writes_per_sec",
    "passes_per_sec",
    "peak_fps",
    "peak_writes_per_sec",
    "peak_passes_per_sec",
    "vref",
    "vref_min",
    "vref_max",
    "frames",
    "phase_frames",
    "pixel_writes",
    "random_writes",
    "passes",
    "random_passes",
    "max_loop_us",
    "midi_drops",
    "loop_overruns",
    "reset",
    "raw_line",
]


def parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture MN42 bench serial logs into raw text and structured CSV."
    )
    parser.add_argument(
        "--port",
        default="auto",
        help="Serial port path or 'auto' to pick the first likely device.",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Serial baud rate. Default: 115200",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=0.0,
        help="Optional capture duration in seconds. Default: run until Ctrl-C.",
    )
    parser.add_argument(
        "--label",
        default="bench",
        help="Short label stamped into each CSV row.",
    )
    parser.add_argument(
        "--csv",
        type=pathlib.Path,
        default=None,
        help="Structured CSV output path. Defaults under logs/.",
    )
    parser.add_argument(
        "--raw",
        type=pathlib.Path,
        default=None,
        help="Raw line log output path. Defaults under logs/.",
    )
    parser.add_argument(
        "--no-echo",
        action="store_true",
        help="Do not mirror serial lines to stdout while capturing.",
    )
    parser.add_argument(
        "--command",
        action="append",
        default=[],
        help="Serial command to send after opening the port. Repeatable.",
    )
    return parser.parse_args(list(argv) if argv is not None else None)


def default_output_paths(label: str) -> tuple[pathlib.Path, pathlib.Path]:
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    root = pathlib.Path("logs")
    return (
        root / f"{label}-{stamp}.csv",
        root / f"{label}-{stamp}.log",
    )


def ensure_parent(path: pathlib.Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def coerce_value(value: str) -> str:
    return value.strip()


def parse_key_values(payload: str) -> Dict[str, str]:
    parsed: Dict[str, str] = {}
    for token in shlex.split(payload):
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        parsed[key] = coerce_value(value)
    return parsed


def parse_structured_line(line: str) -> Optional[Dict[str, str]]:
    if line.startswith("[HEALTH] "):
        parsed = parse_key_values(line[len("[HEALTH] ") :])
        parsed["record_type"] = "HEALTH"
        return parsed
    if line.startswith("[BURN] ") and " uptime=" in line:
        parsed = parse_key_values(line[len("[BURN] ") :])
        parsed["record_type"] = "BURN"
        return parsed
    return None


def to_csv_row(
    parsed: Dict[str, str],
    *,
    raw_line: str,
    label: str,
    port: str,
    baud: int,
    host_now: dt.datetime,
) -> Dict[str, str]:
    row = {column: "" for column in CSV_COLUMNS}
    row["host_ts_iso"] = host_now.isoformat(timespec="milliseconds")
    row["host_ts_epoch_ms"] = str(int(host_now.timestamp() * 1000))
    row["label"] = label
    row["port"] = port
    row["baud"] = str(baud)
    row["record_type"] = parsed.get("record_type", "")
    row["phase"] = parsed.get("phase", "")
    row["mode"] = parsed.get("mode", "")
    row["lock"] = parsed.get("lock", "")
    row["uptime_ms"] = parsed.get("uptime", "").removesuffix("ms")
    row["brightness"] = parsed.get("brightness", "")
    row["brownouts"] = parsed.get("brownouts", "")
    row["fps"] = parsed.get("fps", "")
    row["writes_per_sec"] = parsed.get("writesPerSec", "")
    row["passes_per_sec"] = parsed.get("passesPerSec", "")
    row["peak_fps"] = parsed.get("peakFps", "")
    row["peak_writes_per_sec"] = parsed.get("peakWritesPerSec", "")
    row["peak_passes_per_sec"] = parsed.get("peakPassesPerSec", "")
    row["vref"] = parsed.get("vref", "")
    row["vref_min"] = parsed.get("vrefMin", "")
    row["vref_max"] = parsed.get("vrefMax", "")
    row["frames"] = parsed.get("frames", "")
    row["phase_frames"] = parsed.get("phaseFrames", "")
    row["pixel_writes"] = parsed.get("pixelWrites", "")
    row["random_writes"] = parsed.get("randomWrites", "")
    row["passes"] = parsed.get("passes", "")
    row["random_passes"] = parsed.get("randomPasses", "")
    row["max_loop_us"] = parsed.get("maxLoopUs", "")
    row["midi_drops"] = parsed.get("midiDrops", "")
    row["loop_overruns"] = parsed.get("loopOverruns", "")
    row["reset"] = parsed.get("reset", "")
    row["raw_line"] = raw_line
    return row


def auto_detect_port() -> str:
    try:
        from serial.tools import list_ports
    except ImportError as exc:  # pragma: no cover - exercised in live use
        raise RuntimeError("pyserial is required for auto port detection") from exc

    preferred = []
    fallback = []
    for port in list_ports.comports():
        device = port.device or ""
        description = (port.description or "").lower()
        if any(token in device for token in ("usbmodem", "ttyACM", "ttyUSB")) or any(
            token in description for token in ("teensy", "usb serial", "cdc")
        ):
            preferred.append(device)
        else:
            fallback.append(device)

    candidates = sorted(preferred or fallback)
    if not candidates:
        raise RuntimeError("no serial ports found")
    return candidates[0]


def open_serial(port: str, baud: int):
    try:
        import serial
    except ImportError as exc:  # pragma: no cover - exercised in live use
        raise RuntimeError(
            "pyserial is required. Install it with: python3 -m pip install pyserial"
        ) from exc

    return serial.Serial(port=port, baudrate=baud, timeout=0.25)


def main(argv: Optional[Iterable[str]] = None) -> int:
    args = parse_args(argv)

    csv_path, raw_path = default_output_paths(args.label)
    if args.csv is not None:
        csv_path = args.csv
    if args.raw is not None:
        raw_path = args.raw

    ensure_parent(csv_path)
    ensure_parent(raw_path)

    port = args.port
    if port == "auto":
        port = auto_detect_port()

    start = time.monotonic()
    captured_rows = 0
    raw_lines = 0

    with open_serial(port, args.baud) as ser, csv_path.open("w", newline="", encoding="utf-8") as csv_fp, raw_path.open(
        "w", encoding="utf-8"
    ) as raw_fp:
        writer = csv.DictWriter(csv_fp, fieldnames=CSV_COLUMNS)
        writer.writeheader()

        sys.stderr.write(
            f"Capturing serial from {port} @ {args.baud} into {csv_path} and {raw_path}\n"
        )
        sys.stderr.flush()

        if args.command:
            time.sleep(0.5)
            for command in args.command:
                payload = command.rstrip("\r\n") + "\n"
                ser.write(payload.encode("utf-8"))
                ser.flush()
                sys.stderr.write(f"Sent command: {command}\n")
            sys.stderr.flush()

        try:
            while True:
                if args.duration > 0 and (time.monotonic() - start) >= args.duration:
                    break

                raw = ser.readline()
                if not raw:
                    continue

                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                if not line:
                    continue

                host_now = dt.datetime.now(dt.timezone.utc).astimezone()
                raw_fp.write(line + "\n")
                raw_fp.flush()
                raw_lines += 1

                if not args.no_echo:
                    print(line, flush=True)

                parsed = parse_structured_line(line)
                if parsed is None:
                    continue

                writer.writerow(
                    to_csv_row(
                        parsed,
                        raw_line=line,
                        label=args.label,
                        port=port,
                        baud=args.baud,
                        host_now=host_now,
                    )
                )
                csv_fp.flush()
                captured_rows += 1
        except KeyboardInterrupt:
            sys.stderr.write("Capture interrupted by user.\n")
        finally:
            sys.stderr.write(
                f"Capture complete: {raw_lines} raw lines, {captured_rows} structured rows.\n"
            )
            sys.stderr.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

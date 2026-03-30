#!/usr/bin/env python3
"""Generate reproducible signal-shape PNGs for docs and wiki pages."""

from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


WIDTH = 960
HEIGHT = 480
PAD_X = 72
PAD_Y = 56
BG = "#fcfcf8"
GRID = "#ddd7cb"
AXIS = "#58544e"
INPUT = "#9da3a6"
OUTPUT = "#d4632b"
TITLE = "#1d1b19"
SUBTITLE = "#645d54"
PANEL_BG = "#f4efe3"
PANEL_BORDER = "#d8cfbf"
ACCENT = "#b34f1d"
BOX_FILL = "#fffdf8"
BOX_STROKE = "#cbbca2"
OUT_DIRS = [
    Path("docs/assets/signal-shapes"),
    Path("wiki/assets/signal-shapes"),
]


def clamp01(value: float) -> float:
    return max(0.0, min(1.0, value))


def normalize(values: list[float]) -> list[float]:
    lo = min(values)
    hi = max(values)
    if math.isclose(lo, hi):
        return [0.0 for _ in values]
    return [(value - lo) / (hi - lo) for value in values]


def lowpass(values: list[float], alpha: float) -> list[float]:
    out: list[float] = []
    acc = values[0]
    for value in values:
        acc += alpha * (value - acc)
        out.append(acc)
    return out


def highpass(values: list[float], alpha: float) -> list[float]:
    out: list[float] = []
    prev_in = values[0]
    prev_out = 0.0
    for value in values:
        current = alpha * (prev_out + value - prev_in)
        out.append(abs(current))
        prev_in = value
        prev_out = current
    return out


def generate_input_signal(count: int) -> list[float]:
    values: list[float] = []
    for idx in range(count):
        t = idx / (count - 1)
        pulses = (
            0.85 * math.exp(-((t - 0.16) / 0.05) ** 2)
            + 0.48 * math.exp(-((t - 0.36) / 0.04) ** 2)
            + 0.72 * math.exp(-((t - 0.62) / 0.07) ** 2)
            + 0.38 * math.exp(-((t - 0.84) / 0.03) ** 2)
        )
        ripple = 0.05 * math.sin(18 * math.pi * t) + 0.03 * math.sin(47 * math.pi * t)
        floor = 0.03 + 0.02 * math.sin(2 * math.pi * t)
        values.append(max(0.0, pulses + ripple + floor))
    return normalize(values)


def build_shapes(base: list[float]) -> dict[str, list[float]]:
    rng = random.Random(42)
    lp = normalize(lowpass(base, 0.12))
    hp = normalize(highpass(base, 0.78))
    bp = normalize(lowpass(hp, 0.16))

    random_shape: list[float] = []
    jitter = 0.0
    for value in base:
        jitter += 0.25 * (rng.uniform(-1.0, 1.0) - jitter)
        random_shape.append(clamp01(value * (0.75 + 0.45 * jitter) + 0.15 * abs(jitter)))

    return {
        "LINEAR": base,
        "OPPOSITE_LINEAR": [1.0 - value for value in base],
        "EXPONENTIAL": normalize([value**2.15 for value in base]),
        "LOWPASS": lp,
        "HIGHPASS": hp,
        "BANDPASS": bp,
        "RANDOM": normalize(random_shape),
    }


def scale_point(index: int, value: float, count: int, width: int, height: int) -> tuple[float, float]:
    usable_w = width - (PAD_X * 2)
    usable_h = height - (PAD_Y * 2)
    x = PAD_X + (index / (count - 1)) * usable_w
    y = height - PAD_Y - (value * usable_h)
    return x, y


def draw_chart(
    draw: ImageDraw.ImageDraw,
    width: int,
    height: int,
    title: str,
    subtitle: str,
    base: list[float],
    shaped: list[float],
    title_font: ImageFont.ImageFont,
    body_font: ImageFont.ImageFont,
) -> None:
    draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=20, fill=BG, outline=PANEL_BORDER, width=2)

    draw.text((PAD_X, 18), title, fill=TITLE, font=title_font)
    draw.text((PAD_X, 18 + title_font.size + 6), subtitle, fill=SUBTITLE, font=body_font)

    top = PAD_Y
    left = PAD_X
    right = width - PAD_X
    bottom = height - PAD_Y

    for frac in (0.0, 0.25, 0.5, 0.75, 1.0):
        y = bottom - ((bottom - top) * frac)
        draw.line((left, y, right, y), fill=GRID, width=1)
        label = f"{int(frac * 100)}%"
        draw.text((16, y - 8), label, fill=SUBTITLE, font=body_font)

    for frac in (0.0, 0.25, 0.5, 0.75, 1.0):
        x = left + ((right - left) * frac)
        draw.line((x, top, x, bottom), fill=GRID, width=1)

    draw.line((left, top, left, bottom), fill=AXIS, width=2)
    draw.line((left, bottom, right, bottom), fill=AXIS, width=2)
    draw.text((right - 32, bottom + 10), "time", fill=SUBTITLE, font=body_font)
    draw.text((10, top - 26), "level", fill=SUBTITLE, font=body_font)

    count = len(base)
    base_points = [scale_point(idx, value, count, width, height) for idx, value in enumerate(base)]
    shaped_points = [scale_point(idx, value, count, width, height) for idx, value in enumerate(shaped)]

    for start, end in zip(base_points, base_points[1:]):
        draw.line((*start, *end), fill=INPUT, width=4)
    for start, end in zip(shaped_points, shaped_points[1:]):
        draw.line((*start, *end), fill=OUTPUT, width=6)

    legend_y = height - 28
    draw.line((PAD_X, legend_y, PAD_X + 38, legend_y), fill=INPUT, width=4)
    draw.text((PAD_X + 48, legend_y - 10), "reference input", fill=SUBTITLE, font=body_font)
    draw.line((PAD_X + 210, legend_y, PAD_X + 248, legend_y), fill=OUTPUT, width=6)
    draw.text((PAD_X + 258, legend_y - 10), "illustrative output", fill=SUBTITLE, font=body_font)


def save_individual_charts(base: list[float], shapes: dict[str, list[float]], title_font: ImageFont.ImageFont, body_font: ImageFont.ImageFont) -> None:
    subtitles = {
        "LINEAR": "Direct response to the same contour.",
        "OPPOSITE_LINEAR": "Inverts the contour so loud becomes low.",
        "EXPONENTIAL": "Suppresses small motion and exaggerates peaks.",
        "LOWPASS": "Smooths spikes into steadier movement.",
        "HIGHPASS": "Favors fast changes over steady sustain.",
        "BANDPASS": "Focuses on a narrower band of movement.",
        "RANDOM": "Adds unstable, generative variation on purpose.",
    }

    for name, values in shapes.items():
        image = Image.new("RGB", (WIDTH, HEIGHT), BG)
        draw = ImageDraw.Draw(image)
        draw_chart(draw, WIDTH, HEIGHT, name, subtitles[name], base, values, title_font, body_font)
        for out_dir in OUT_DIRS:
            out_dir.mkdir(parents=True, exist_ok=True)
            image.save(out_dir / f"{name.lower()}.png")


def save_overview(base: list[float], shapes: dict[str, list[float]], title_font: ImageFont.ImageFont, body_font: ImageFont.ImageFont) -> None:
    card_w = 560
    card_h = 320
    cols = 2
    rows = 4
    gap = 24
    outer = 36
    width = (card_w * cols) + gap + (outer * 2)
    height = (card_h * rows) + (gap * (rows - 1)) + (outer * 2) + 90
    image = Image.new("RGB", (width, height), PANEL_BG)
    draw = ImageDraw.Draw(image)

    draw.text((outer, 18), "Envelope response shape overview", fill=TITLE, font=title_font)
    draw.text(
        (outer, 18 + title_font.size + 8),
        "Illustrative time-domain responses using the same input contour for each mode.",
        fill=SUBTITLE,
        font=body_font,
    )

    order = [
        "LINEAR",
        "OPPOSITE_LINEAR",
        "EXPONENTIAL",
        "LOWPASS",
        "HIGHPASS",
        "BANDPASS",
        "RANDOM",
    ]

    for idx, name in enumerate(order):
        col = idx % cols
        row = idx // cols
        x = outer + (col * (card_w + gap))
        y = outer + 72 + (row * (card_h + gap))
        panel = Image.new("RGB", (card_w, card_h), BG)
        panel_draw = ImageDraw.Draw(panel)
        draw_chart(
            panel_draw,
            card_w,
            card_h,
            name,
            "Signal-shape sketch for docs and wiki reference.",
            base,
            shapes[name],
            title_font,
            body_font,
        )
        image.paste(panel, (x, y))

    for out_dir in OUT_DIRS:
        out_dir.mkdir(parents=True, exist_ok=True)
        image.save(out_dir / "filter-shapes-overview.png")


def draw_box(
    draw: ImageDraw.ImageDraw,
    xy: tuple[int, int, int, int],
    title: str,
    lines: list[str],
    title_font: ImageFont.ImageFont,
    body_font: ImageFont.ImageFont,
) -> None:
    draw.rounded_rectangle(xy, radius=22, fill=BOX_FILL, outline=BOX_STROKE, width=3)
    x1, y1, x2, y2 = xy
    draw.text((x1 + 18, y1 + 16), title, fill=TITLE, font=title_font)
    cursor_y = y1 + 16 + title_font.size + 10
    for line in lines:
        draw.text((x1 + 18, cursor_y), line, fill=SUBTITLE, font=body_font)
        cursor_y += body_font.size + 10


def draw_arrow(draw: ImageDraw.ImageDraw, start: tuple[int, int], end: tuple[int, int], color: str) -> None:
    draw.line((*start, *end), fill=color, width=6)
    angle = math.atan2(end[1] - start[1], end[0] - start[0])
    head_len = 18
    left = (
        end[0] - head_len * math.cos(angle - math.pi / 7),
        end[1] - head_len * math.sin(angle - math.pi / 7),
    )
    right = (
        end[0] - head_len * math.cos(angle + math.pi / 7),
        end[1] - head_len * math.sin(angle + math.pi / 7),
    )
    draw.polygon([end, left, right], fill=color)


def save_signal_flow_diagram(title_font: ImageFont.ImageFont, body_font: ImageFont.ImageFont) -> None:
    width = 1520
    height = 860
    image = Image.new("RGB", (width, height), PANEL_BG)
    draw = ImageDraw.Draw(image)

    draw.text((44, 26), "MOARkNOBS-42 signal flow", fill=TITLE, font=title_font)
    draw.text(
        (44, 26 + title_font.size + 8),
        "Fast wiki view of how physical input becomes modulation, slot state, and output.",
        fill=SUBTITLE,
        font=body_font,
    )

    inputs = (44, 120, 300, 320)
    followers = (370, 120, 700, 320)
    modulation = (770, 120, 1100, 320)
    outputs = (1170, 120, 1476, 320)
    browser = (370, 430, 700, 668)
    bridge = (770, 430, 1100, 668)
    persistence = (1170, 430, 1476, 668)

    draw_box(
        draw,
        inputs,
        "Physical inputs",
        ["Buttons and pots", "Envelope/CV inputs", "MIDI in and clock"],
        title_font,
        body_font,
    )
    draw_box(
        draw,
        followers,
        "Reactive front end",
        ["Envelope followers", "Filter shape selection", "Per-slot response tuning"],
        title_font,
        body_font,
    )
    draw_box(
        draw,
        modulation,
        "Modulation buses",
        ["SEF direct follow", "ARG blend/compare", "LFO route layer"],
        title_font,
        body_font,
    )
    draw_box(
        draw,
        outputs,
        "Live outputs",
        ["Slot values and profiles", "MIDI, LEDs, arpeggiator", "OLED and telemetry streams"],
        title_font,
        body_font,
    )
    draw_box(
        draw,
        browser,
        "Browser app",
        ["Stages edits locally", "Applies config over WebSerial", "Shows diff, monitor, and scope"],
        title_font,
        body_font,
    )
    draw_box(
        draw,
        bridge,
        "OSC / MIDI bridge",
        ["Converts serial telemetry", "Publishes OSC and virtual MIDI", "Forwards validated commands back"],
        title_font,
        body_font,
    )
    draw_box(
        draw,
        persistence,
        "Device memory",
        ["Profiles A-D", "EEPROM snapshots", "Rollback and recovery paths"],
        title_font,
        body_font,
    )

    draw_arrow(draw, (300, 220), (370, 220), ACCENT)
    draw_arrow(draw, (700, 220), (770, 220), ACCENT)
    draw_arrow(draw, (1100, 220), (1170, 220), ACCENT)
    draw_arrow(draw, (535, 430), (535, 320), AXIS)
    draw_arrow(draw, (935, 430), (935, 320), AXIS)
    draw_arrow(draw, (1170, 548), (1100, 548), AXIS)
    draw_arrow(draw, (700, 548), (770, 548), AXIS)
    draw_arrow(draw, (1100, 548), (1170, 548), AXIS)

    draw.text((612, 350), "configure", fill=SUBTITLE, font=body_font)
    draw.text((988, 350), "telemetry / cmd", fill=SUBTITLE, font=body_font)

    for out_dir in OUT_DIRS:
        out_dir.mkdir(parents=True, exist_ok=True)
        image.save(out_dir / "system-signal-flow.png")


def main() -> None:
    base = generate_input_signal(240)
    shapes = build_shapes(base)
    title_font = ImageFont.load_default(size=22)
    body_font = ImageFont.load_default(size=16)
    save_individual_charts(base, shapes, title_font, body_font)
    save_overview(base, shapes, title_font, body_font)
    save_signal_flow_diagram(title_font, body_font)
    print("Generated signal-shape PNGs in docs/ and wiki/ asset folders.")


if __name__ == "__main__":
    main()

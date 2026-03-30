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
INK_DARK = "#111418"
OLED_BG = "#050806"
OLED_FG = "#f4fff6"
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


def hex_to_rgb(value: str) -> tuple[int, int, int]:
    value = value.lstrip("#")
    return tuple(int(value[i : i + 2], 16) for i in (0, 2, 4))


def lerp_color(a: str, b: str, amount: float) -> tuple[int, int, int]:
    start = hex_to_rgb(a)
    end = hex_to_rgb(b)
    amount = clamp01(amount)
    return tuple(int(round(s + (e - s) * amount)) for s, e in zip(start, end))


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


def compute_arg_value(method: str, a: int, b: int) -> int:
    if method == "PLUS":
        result = a + b
    elif method == "AVG":
        result = (a + b) / 2
    elif method == "MAXX":
        result = max(a, b)
    elif method == "XABS":
        result = abs(a - b)
    elif method == "MULT":
        result = (a * b) / 127
    elif method == "XORR":
        result = a ^ b
    else:
        raise ValueError(f"unsupported ARG method: {method}")
    return int(max(0, min(127, round(result))))


def generate_arg_inputs(count: int) -> tuple[list[int], list[int]]:
    a_values: list[int] = []
    b_values: list[int] = []
    for idx in range(count):
        t = idx / (count - 1)
        a_signal = (
            0.82 * math.exp(-((t - 0.18) / 0.07) ** 2)
            + 0.56 * math.exp(-((t - 0.53) / 0.10) ** 2)
            + 0.34 * math.exp(-((t - 0.84) / 0.04) ** 2)
            + 0.04 * math.sin(10 * math.pi * t)
        )
        b_signal = (
            0.28
            + 0.46 * math.exp(-((t - 0.30) / 0.05) ** 2)
            + 0.66 * math.exp(-((t - 0.69) / 0.08) ** 2)
            + 0.08 * math.sin(6 * math.pi * t + 0.8)
        )
        a_values.append(int(round(clamp01(a_signal) * 127)))
        b_values.append(int(round(clamp01(b_signal) * 127)))
    return a_values, b_values


def draw_arg_line_card(
    draw: ImageDraw.ImageDraw,
    x: int,
    y: int,
    width: int,
    height: int,
    method: str,
    formula: str,
    title_font: ImageFont.ImageFont,
    body_font: ImageFont.ImageFont,
) -> None:
    draw.rounded_rectangle((x, y, x + width, y + height), radius=20, fill=BG, outline=PANEL_BORDER, width=2)
    draw.text((x + 20, y + 16), method, fill=TITLE, font=title_font)
    draw.text((x + 20, y + 16 + title_font.size + 4), formula, fill=SUBTITLE, font=body_font)

    left = x + 58
    top = y + 86
    right = x + width - 28
    bottom = y + height - 82
    usable_w = right - left
    usable_h = bottom - top

    a_values, b_values = generate_arg_inputs(120)
    c_values = [compute_arg_value(method, a, b) for a, b in zip(a_values, b_values)]

    for frac in (0.0, 0.25, 0.5, 0.75, 1.0):
        yy = bottom - usable_h * frac
        draw.line((left, yy, right, yy), fill=GRID, width=1)
        draw.text((x + 18, yy - 8), f"{int(frac * 127)}", fill=SUBTITLE, font=body_font)

    time_ticks = [0, 25, 50, 75, 100]
    for tick in time_ticks:
        frac = tick / 100.0
        xx = left + usable_w * frac
        draw.line((xx, top, xx, bottom), fill=GRID, width=1)
        label = str(tick)
        label_x = xx - (len(label) * 4)
        draw.text((label_x, bottom + 8), label, fill=SUBTITLE, font=body_font)

    draw.line((left, top, left, bottom), fill=AXIS, width=2)
    draw.line((left, bottom, right, bottom), fill=AXIS, width=2)
    draw.text((left - 24, top - 22), "val", fill=TITLE, font=body_font)
    draw.text((right - 22, bottom + 26), "time", fill=TITLE, font=body_font)

    series = [
        ("A in", "#6b8798", a_values),
        ("B in", "#c24c6d", b_values),
        ("C out", "#d4632b", c_values),
    ]
    for _, color, values in series:
        points = []
        steps = len(values)
        for idx, value in enumerate(values):
            px = left + (idx / (steps - 1)) * usable_w
            py = bottom - (value / 127.0) * usable_h
            points.append((px, py))
        for start, end in zip(points, points[1:]):
            draw.line((*start, *end), fill=color, width=4)

    legend_x = x + 24
    legend_y = y + height - 52
    for idx, (label, color, _) in enumerate(series):
        xx = legend_x + idx * 112
        draw.line((xx, legend_y + 7, xx + 22, legend_y + 7), fill=color, width=4)
        draw.text((xx + 30, legend_y), label, fill=SUBTITLE, font=body_font)


def draw_arg_heatmap_card(
    draw: ImageDraw.ImageDraw,
    x: int,
    y: int,
    width: int,
    height: int,
    method: str,
    formula: str,
    title_font: ImageFont.ImageFont,
    body_font: ImageFont.ImageFont,
) -> None:
    draw_arg_line_card(draw, x, y, width, height, method, formula, title_font, body_font)


def save_arg_matrix_overview(title_font: ImageFont.ImageFont, body_font: ImageFont.ImageFont) -> None:
    width = 1400
    height = 1020
    image = Image.new("RGB", (width, height), PANEL_BG)
    draw = ImageDraw.Draw(image)

    draw.text((40, 24), "ARG matrix overview", fill=TITLE, font=title_font)
    draw.text(
        (40, 24 + title_font.size + 8),
        "Each card shows the same A input over time, the same B input over time, and the resulting C slot value for one ARG method.",
        fill=SUBTITLE,
        font=body_font,
    )

    methods = [
        ("PLUS", "A + B"),
        ("AVG", "(A + B) / 2"),
        ("MAXX", "max(A, B)"),
        ("XABS", "abs(A - B)"),
        ("MULT", "(A * B) / 127"),
        ("XORR", "A ^ B"),
    ]
    card_w = 420
    card_h = 410
    gap_x = 28
    gap_y = 28
    origin_y = 92
    for idx, (method, formula) in enumerate(methods):
        row = idx // 3
        col = idx % 3
        x = 40 + col * (card_w + gap_x)
        y = origin_y + row * (card_h + gap_y)
        draw_arg_heatmap_card(draw, x, y, card_w, card_h, method, formula, title_font, body_font)

    draw.rounded_rectangle((40, 930, width - 40, 980), radius=16, fill=BG, outline=PANEL_BORDER, width=2)
    draw.text(
        (58, 944),
        "Use PLUS/AVG/MAXX first for legible blends, then XABS for contrast. MULT and XORR are the fast route into stranger territory.",
        fill=SUBTITLE,
        font=body_font,
    )

    for out_dir in OUT_DIRS:
        out_dir.mkdir(parents=True, exist_ok=True)
        image.save(out_dir / "arg-matrix-overview.png")


def draw_wave_icon(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], color: str, phase: float = 0.0) -> None:
    x1, y1, x2, y2 = box
    mid_y = (y1 + y2) / 2
    amp = (y2 - y1) * 0.32
    points = []
    for idx in range(40):
        t = idx / 39
        x = x1 + t * (x2 - x1)
        y = mid_y - math.sin((t + phase) * 2 * math.pi) * amp
        points.append((x, y))
    for start, end in zip(points, points[1:]):
        draw.line((*start, *end), fill=color, width=4)


def draw_led_bars(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int]) -> None:
    x1, y1, x2, y2 = box
    count = 8
    gap = 10
    bar_w = (x2 - x1 - gap * (count - 1)) / count
    levels = [0.25, 0.42, 0.63, 0.84, 1.0, 0.76, 0.51, 0.34]
    for idx, level in enumerate(levels):
        bx1 = x1 + idx * (bar_w + gap)
        bx2 = bx1 + bar_w
        by2 = y2
        by1 = y2 - level * (y2 - y1)
        draw.rounded_rectangle((bx1, by1, bx2, by2), radius=8, fill=lerp_color("#593726", OUTPUT, level))


def draw_swing_grid(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int]) -> None:
    x1, y1, x2, y2 = box
    baseline = y1 + (y2 - y1) * 0.55
    draw.line((x1, baseline, x2, baseline), fill=GRID, width=2)
    steps = 8
    spacing = (x2 - x1) / steps
    for idx in range(steps):
        x = x1 + idx * spacing
        height = 22 if idx % 2 == 0 else 34
        shift = 0 if idx % 2 == 0 else 10
        draw.line((x + shift, baseline - height, x + shift, baseline + 6), fill=OUTPUT if idx % 2 else AXIS, width=4)
    draw.text((x1, y2 + 6), "straight beats stay put, offbeats lean right", fill=SUBTITLE, font=ImageFont.load_default(size=14))


def draw_gain_trim(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int]) -> None:
    x1, y1, x2, y2 = box
    center_x = x1 + (x2 - x1) * 0.28
    draw.rounded_rectangle((x1, y1, x1 + 46, y2), radius=10, outline=AXIS, width=2)
    draw.rounded_rectangle((x1 + 74, y1, x1 + 120, y2), radius=10, outline=AXIS, width=2)
    draw.rectangle((x1 + 8, y2 - 56, x1 + 38, y2 - 4), fill="#6b8798")
    draw.rectangle((x1 + 82, y2 - 92, x1 + 112, y2 - 4), fill=OUTPUT)
    draw.text((x1, y2 + 6), "same follower, more bite after trim", fill=SUBTITLE, font=ImageFont.load_default(size=14))


def save_lfo_route_overview(title_font: ImageFont.ImageFont, body_font: ImageFont.ImageFont) -> None:
    width = 1560
    height = 660
    image = Image.new("RGB", (width, height), PANEL_BG)
    draw = ImageDraw.Draw(image)

    draw.text((40, 24), "LFO route overview", fill=TITLE, font=title_font)
    draw.text(
        (40, 24 + title_font.size + 8),
        "Factory defaults route LFO1 to LED brightness and arp swing, while LFO2 trims envelope-follower gain.",
        fill=SUBTITLE,
        font=body_font,
    )

    cards = [
        ("LedBrightness", "Most legible first route", "Visible pulse on the LED layer"),
        ("ArpSwing", "Changes timing feel", "Offbeats drift while the groove stays readable"),
        ("EfGainTrim", "Changes source intensity", "Reactive paths push harder or softer"),
    ]
    card_w = 468
    card_h = 500
    gap = 28
    for idx, (name, kicker, body) in enumerate(cards):
        x = 40 + idx * (card_w + gap)
        y = 104
        draw.rounded_rectangle((x, y, x + card_w, y + card_h), radius=24, fill=BG, outline=PANEL_BORDER, width=2)
        draw.text((x + 24, y + 18), name, fill=TITLE, font=title_font)
        draw.text((x + 24, y + 18 + title_font.size + 4), kicker, fill=SUBTITLE, font=body_font)
        draw_wave_icon(draw, (x + 24, y + 86, x + card_w - 24, y + 166), ACCENT, phase=0.08 * idx)
        draw.text((x + 24, y + 184), body, fill=SUBTITLE, font=body_font)

        preview = (x + 24, y + 230, x + card_w - 24, y + 380)
        if name == "LedBrightness":
            draw_led_bars(draw, preview)
        elif name == "ArpSwing":
            draw_swing_grid(draw, preview)
        else:
            draw_gain_trim(draw, preview)

        draw.rounded_rectangle((x + 24, y + 410, x + card_w - 24, y + 468), radius=14, fill=PANEL_BG, outline=PANEL_BORDER, width=1)
        explanation = {
            "LedBrightness": "Best first demo: you can see the cycle instantly.",
            "ArpSwing": "Best musical demo: it moves feel, not just value.",
            "EfGainTrim": "Best advanced demo: it changes the source itself.",
        }[name]
        draw.text((x + 38, y + 428), explanation, fill=SUBTITLE, font=body_font)

    for out_dir in OUT_DIRS:
        out_dir.mkdir(parents=True, exist_ok=True)
        image.save(out_dir / "lfo-route-overview.png")


def save_lfo_oled_preview(title_font: ImageFont.ImageFont, body_font: ImageFont.ImageFont) -> None:
    width = 980
    height = 560
    image = Image.new("RGB", (width, height), PANEL_BG)
    draw = ImageDraw.Draw(image)

    draw.text((40, 24), "OLED LFO diagnostic preview", fill=TITLE, font=title_font)
    draw.text(
        (40, 24 + title_font.size + 8),
        "Current firmware exposes live LFO values on the debug diagnostics page rather than a dedicated LFO editor page.",
        fill=SUBTITLE,
        font=body_font,
    )

    panel = (112, 118, 880, 502)
    draw.rounded_rectangle(panel, radius=34, fill="#d5d8d1", outline=BOX_STROKE, width=3)
    px = 160
    py = 156
    pw = 672
    ph = 308
    draw.rounded_rectangle((px, py, px + pw, py + ph), radius=12, fill=OLED_BG, outline="#7f877f", width=3)
    draw.rectangle((px + 8, py + 8, px + pw - 8, py + ph - 8), outline=OLED_FG, width=2)

    oled_font = ImageFont.load_default(size=18)
    lines = [
        "DBG BPM:120.0 CLK:ON",
        "E0 B0.08 G1.00 V92",
        "E1 B0.05 G0.96 V41",
        "E2 B0.06 G1.04 V12",
        "E3 B0.04 G1.00 V00",
        "E4 B0.05 G0.98 V77",
        "E5 B0.05 G1.02 V18",
        "L1:0.63 L2:0.22",
    ]
    line_y = py + 18
    for line in lines:
        draw.text((px + 20, line_y), line, fill=OLED_FG, font=oled_font)
        line_y += 34

    draw.rounded_rectangle((112, 514, 880, 544), radius=10, fill=BG, outline=PANEL_BORDER, width=1)
    draw.text(
        (130, 521),
        "Labels and line order match DisplayManager diagnostic page `kDiagnosticPageDebug`.",
        fill=SUBTITLE,
        font=body_font,
    )

    for out_dir in OUT_DIRS:
        out_dir.mkdir(parents=True, exist_ok=True)
        image.save(out_dir / "lfo-oled-preview.png")


def main() -> None:
    base = generate_input_signal(240)
    shapes = build_shapes(base)
    title_font = ImageFont.load_default(size=22)
    body_font = ImageFont.load_default(size=16)
    save_individual_charts(base, shapes, title_font, body_font)
    save_overview(base, shapes, title_font, body_font)
    save_signal_flow_diagram(title_font, body_font)
    save_arg_matrix_overview(title_font, body_font)
    save_lfo_route_overview(title_font, body_font)
    save_lfo_oled_preview(title_font, body_font)
    print("Generated documentation PNGs in docs/ and wiki/ asset folders.")


if __name__ == "__main__":
    main()

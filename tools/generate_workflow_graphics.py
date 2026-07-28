#!/usr/bin/env python3
"""Generate workflow diagrams and annotated UI screenshots for docs/wiki."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw, ImageFont


BG = "#fcfcf8"
PANEL_BG = "#f4efe3"
PANEL_BORDER = "#d8cfbf"
TITLE = "#1d1b19"
SUBTITLE = "#645d54"
AXIS = "#58544e"
ACCENT = "#d4632b"
MINT = "#19c37d"
BLUE = "#6ea8fe"
PURPLE = "#7e5de8"
CORAL = "#f25f5c"
CYAN = "#4ecdc4"
GOLD = "#ff9f43"
ROSE = "#c24c6d"

DOC_UI_DIR = Path("docs/assets/ui")
WIKI_UI_DIR = Path("wiki/assets/ui")
DOC_FLOW_DIR = Path("docs/assets/workflows")
WIKI_FLOW_DIR = Path("wiki/assets/workflows")


def title_font(size: int = 24):
    return ImageFont.load_default(size=size)


def body_font(size: int = 16):
    return ImageFont.load_default(size=size)


def ensure_dirs() -> None:
    for out_dir in (DOC_UI_DIR, WIKI_UI_DIR, DOC_FLOW_DIR, WIKI_FLOW_DIR):
        out_dir.mkdir(parents=True, exist_ok=True)


def save_ui(image: Image.Image, filename: str) -> None:
    image.save(DOC_UI_DIR / filename)
    image.save(WIKI_UI_DIR / filename)


def save_flow(image: Image.Image, filename: str) -> None:
    image.save(DOC_FLOW_DIR / filename)
    image.save(WIKI_FLOW_DIR / filename)


def draw_round_panel(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int], fill: str = BG) -> None:
    draw.rounded_rectangle(box, radius=22, fill=fill, outline=PANEL_BORDER, width=2)


def rgba(color: str, alpha: int) -> tuple[int, int, int, int]:
    red, green, blue = ImageColor.getrgb(color)
    return (red, green, blue, alpha)


def dashed_line(
    draw: ImageDraw.ImageDraw,
    start: tuple[int, int],
    end: tuple[int, int],
    color,
    width: int = 2,
    dash: int = 10,
    gap: int = 7,
) -> None:
    sx, sy = start
    ex, ey = end
    dx = ex - sx
    dy = ey - sy
    distance = math.hypot(dx, dy)
    if distance == 0:
        return
    step_x = dx / distance
    step_y = dy / distance
    pos = 0.0
    while pos < distance:
        seg_start = pos
        seg_end = min(pos + dash, distance)
        x1 = sx + step_x * seg_start
        y1 = sy + step_y * seg_start
        x2 = sx + step_x * seg_end
        y2 = sy + step_y * seg_end
        draw.line((x1, y1, x2, y2), fill=color, width=width)
        pos += dash + gap


def dashed_box(
    draw: ImageDraw.ImageDraw,
    box: tuple[int, int, int, int],
    color,
    width: int = 2,
    dash: int = 10,
    gap: int = 7,
) -> None:
    x1, y1, x2, y2 = box
    dashed_line(draw, (x1, y1), (x2, y1), color, width, dash, gap)
    dashed_line(draw, (x2, y1), (x2, y2), color, width, dash, gap)
    dashed_line(draw, (x2, y2), (x1, y2), color, width, dash, gap)
    dashed_line(draw, (x1, y2), (x1, y1), color, width, dash, gap)


def draw_callout(
    draw: ImageDraw.ImageDraw,
    target: tuple[int, int, int, int],
    label_box: tuple[int, int, int, int],
    text: str,
    color: str,
    font,
    *,
    dashed_target: bool = False,
    line_width: int = 3,
) -> None:
    tx1, ty1, tx2, ty2 = target
    lx1, ly1, lx2, ly2 = label_box
    if dashed_target:
        dashed_box(draw, target, rgba(color, 255), width=max(1, line_width - 1))
    else:
        draw.rounded_rectangle(target, radius=18, outline=rgba(color, 255), width=line_width)
    draw.rounded_rectangle(label_box, radius=14, fill=rgba("#111418", 244), outline=rgba(color, 255), width=2)
    draw.text((lx1 + 12, ly1 + 10), text, fill="#fff8ee", font=font)
    label_cx = (lx1 + lx2) // 2
    label_cy = (ly1 + ly2) // 2
    target_cx = (tx1 + tx2) // 2
    target_cy = (ty1 + ty2) // 2
    if label_cx < tx1:
        start = (lx2, label_cy)
        end = (tx1, target_cy)
    elif label_cx > tx2:
        start = (lx1, label_cy)
        end = (tx2, target_cy)
    elif label_cy < ty1:
        start = (label_cx, ly2)
        end = (target_cx, ty1)
    else:
        start = (label_cx, ly1)
        end = (target_cx, ty2)
    if dashed_target:
        dashed_line(draw, start, end, rgba(color, 255), width=2, dash=8, gap=6)
    else:
        draw.line((*start, *end), fill=rgba(color, 255), width=2)
    draw.ellipse((end[0] - 3, end[1] - 3, end[0] + 3, end[1] + 3), fill=rgba(color, 255))


def draw_heading(draw: ImageDraw.ImageDraw, image: Image.Image, heading: str, subheading: str) -> None:
    tf = title_font(24)
    bf = body_font(16)
    draw.text((40, 26), heading, fill=TITLE, font=tf)
    draw.text((40, 26 + tf.size + 10), subheading, fill=SUBTITLE, font=bf)


def annotate_configurator_top() -> None:
    src = DOC_UI_DIR / "configurator-sim.png"
    base = Image.open(src).convert("RGBA")
    crop = base.crop((0, 0, 1600, 930))
    left_gutter = 60
    right_gutter = 420
    canvas = Image.new("RGBA", (left_gutter + crop.width + right_gutter, crop.height + 140), PANEL_BG)
    canvas.paste(crop, (left_gutter, 90))
    draw = ImageDraw.Draw(canvas)
    draw_heading(
        draw,
        canvas,
        "Configurator top half",
        "Identity, apply controls, and recovery/profile tools from the simulator-driven UI.",
    )

    ox, oy = left_gutter, 90
    bf = body_font(16)

    gutter_x = ox + crop.width + 40
    label_w = 320
    label_h = 48
    right_callouts = [
        ((ox + 1110, oy + 10, ox + 1525, oy + 58), (gutter_x, 112, gutter_x + label_w, 112 + label_h), "Connection + identity", ACCENT),
        ((ox + 1080, oy + 56, ox + 1572, oy + 126), (gutter_x, 176, gutter_x + label_w, 176 + label_h), "Apply / Rollback", GOLD),
        ((ox + 18, oy + 150, ox + 1515, oy + 892), (gutter_x, 270, gutter_x + label_w, 270 + label_h), "Recovery & Profiles", MINT),
    ]
    for target, label_box, text, color in right_callouts:
        draw_callout(draw, target, label_box, text, color, bf)
    save_ui(canvas, "configurator-top-annotated.png")


def annotate_configurator_bottom() -> None:
    src = DOC_UI_DIR / "configurator-workbench-sim.png"
    crop = Image.open(src).convert("RGBA")
    left_gutter = 320
    right_gutter = 420
    canvas = Image.new("RGBA", (left_gutter + crop.width + right_gutter, crop.height + 140), PANEL_BG)
    canvas.paste(crop, (left_gutter, 90))
    draw = ImageDraw.Draw(canvas)
    draw_heading(
        draw,
        canvas,
        "Configurator workbench",
        "Scrolled lower work area with live slots, selected-slot editing, the utility rail, and staged diff.",
    )

    ox, oy = left_gutter, 90
    bf = body_font(16)
    gutter_x = ox + crop.width + 40
    label_w = 320
    label_h = 48

    right_callouts = [
        ((ox + 431, oy + 0, ox + 880, oy + 1014), (gutter_x, 140, gutter_x + label_w, 140 + label_h), "Selected Slot", PURPLE),
        ((ox + 886, oy + 0, ox + 1211, oy + 118), (gutter_x, 206, gutter_x + label_w, 206 + label_h), "Utility rail", CORAL),
        ((ox + 886, oy + 132, ox + 1211, oy + 620), (gutter_x, 272, gutter_x + label_w, 272 + label_h), "Staged Diff", CYAN),
    ]
    left_callouts = [
        ((ox + 0, oy + 0, ox + 425, oy + 650), (24, 160, 24 + 220, 160 + label_h), "Live Slots", BLUE),
    ]
    for target, label_box, text, color in right_callouts + left_callouts:
        draw_callout(draw, target, label_box, text, color, bf)
    save_ui(canvas, "configurator-workbench-annotated.png")


def annotate_slot_tile() -> None:
    src = DOC_UI_DIR / "slot-tile-sim.png"
    base = Image.open(src).convert("RGBA")
    canvas = Image.new("RGBA", (base.width + 420, base.height + 160), PANEL_BG)
    canvas.paste(base, (40, 100))
    draw = ImageDraw.Draw(canvas)
    draw_heading(
        draw,
        canvas,
        "Live slot tile",
        "Selected tile, type badge, and browser-only IM/PK behavior in the simulator UI.",
    )

    ox, oy = 40, 100
    bf = body_font(16)
    label_x = ox + base.width + 42
    draw_callout(draw, (ox + 21, oy + 41, ox + 86, oy + 110), (label_x, 112, label_x + 172, 160), "Selected tile", PURPLE, bf, dashed_target=True, line_width=2)
    draw_callout(draw, (ox + 31, oy + 52, ox + 60, oy + 66), (label_x, 176, label_x + 132, 224), "Slot ID", BLUE, bf, dashed_target=True, line_width=2)
    draw_callout(draw, (ox + 34, oy + 72, ox + 53, oy + 84), (label_x, 240, label_x + 146, 288), "Type code", GOLD, bf, dashed_target=True, line_width=2)
    draw_callout(draw, (ox + 38, oy + 88, ox + 73, oy + 103), (label_x, 304, label_x + 130, 352), "PK badge", MINT, bf, dashed_target=True, line_width=2)
    draw_callout(draw, (ox + 123, oy + 88, ox + 157, oy + 103), (label_x, 368, label_x + 130, 416), "IM badge", CORAL, bf, dashed_target=True, line_width=2)
    draw_callout(draw, (ox + 30, oy + 87, ox + 180, oy + 104), (label_x - 58, 44, label_x + 210, 92), "Browser-only mode badge", ROSE, bf, dashed_target=True, line_width=2)
    save_ui(canvas, "slot-tile-annotated.png")


def draw_box(draw, box, heading, lines, fill=BG):
    draw_round_panel(draw, box, fill=fill)
    tf = title_font(22)
    bf = body_font(16)
    x1, y1, x2, y2 = box
    draw.text((x1 + 18, y1 + 16), heading, fill=TITLE, font=tf)
    y = y1 + 16 + tf.size + 10
    for line in lines:
      draw.text((x1 + 18, y), line, fill=SUBTITLE, font=bf)
      y += bf.size + 10


def arrow(draw, start, end, color=AXIS):
    draw.line((*start, *end), fill=color, width=5)
    angle = math.atan2(end[1] - start[1], end[0] - start[0])
    head_len = 14
    left = (
        end[0] - head_len * math.cos(angle - math.pi / 7),
        end[1] - head_len * math.sin(angle - math.pi / 7),
    )
    right = (
        end[0] - head_len * math.cos(angle + math.pi / 7),
        end[1] - head_len * math.sin(angle + math.pi / 7),
    )
    draw.polygon([end, left, right], fill=color)


def connectivity_decision() -> None:
    image = Image.new("RGB", (1320, 760), PANEL_BG)
    draw = ImageDraw.Draw(image)
    draw_heading(draw, image, "Configurator vs bridge", "Use the browser alone for direct setup. Add the bridge when you need OSC or a DAW-facing MIDI port.")
    draw_box(draw, (50, 130, 600, 640), "Configurator path", [
        "Direct USB + WebSerial",
        "Best for setup, monitoring,",
        "profiles, staged edits, and",
        "one-board bench work.",
        "",
        "Choose this when:",
        "• browser config is enough",
        "• no OSC routing is needed",
        "• no virtual MIDI port is needed",
    ])
    draw_box(draw, (720, 130, 1270, 640), "Bridge path", [
        "Node bridge between the board",
        "and other host software.",
        "",
        "Choose this when:",
        "• you need OSC in Max/Pd/TouchOSC",
        "• you need MN42 Bridge in a DAW",
        "• you want bridge-backed app access",
        "• the host setup is part of the rig",
    ])
    draw.text((315, 668), "USB + browser", fill=SUBTITLE, font=body_font(16))
    draw.text((954, 668), "USB + bridge + host", fill=SUBTITLE, font=body_font(16))
    arrow(draw, (600, 385), (720, 385), ACCENT)
    save_flow(image, "connectivity-decision-overview.png")


def bridge_routing() -> None:
    image = Image.new("RGB", (1440, 760), PANEL_BG)
    draw = ImageDraw.Draw(image)
    draw_heading(draw, image, "Bridge routing overview", "The bridge listens to firmware serial telemetry, then fans that data out to OSC, virtual MIDI, and the browser-facing bridge path.")
    boxes = {
        "mn42": (50, 200, 290, 390),
        "bridge": (390, 170, 720, 430),
        "osc": (820, 120, 1180, 270),
        "daw": (820, 320, 1180, 470),
        "app": (820, 520, 1180, 670),
    }
    draw_box(draw, boxes["mn42"], "MN42 hardware", ["Serial telemetry", "Slot + envelope data", "Incoming live-control commands"])
    draw_box(draw, boxes["bridge"], "Node bridge", ["Reads serial", "Publishes OSC", "Creates MN42 Bridge MIDI port", "Forwards validated commands back"])
    draw_box(draw, boxes["osc"], "OSC host", ["/mn42/slots", "/mn42/envelopes", "/mn42/cmd"])
    draw_box(draw, boxes["daw"], "DAW / virtual MIDI", ["MN42 Bridge", "Ch 1 CC 0..41", "Ch 2 CC 0..5"])
    draw_box(draw, boxes["app"], "Browser app via bridge", ["Bridge-backed configurator", "When direct WebSerial is not", "the workflow you want"])
    arrow(draw, (290, 295), (390, 295), ACCENT)
    arrow(draw, (720, 245), (820, 195), ACCENT)
    arrow(draw, (720, 325), (820, 395), ACCENT)
    arrow(draw, (720, 385), (820, 595), ACCENT)
    save_flow(image, "bridge-routing-overview.png")


def validation_gates() -> None:
    image = Image.new("RGB", (1420, 860), PANEL_BG)
    draw = ImageDraw.Draw(image)
    draw_heading(draw, image, "Validation gates", "From software gate to board status assignment, each stage answers whether the unit is only powered, actually validated, or demo-ready.")
    x = 90
    steps = [
        ("1. Target", "Pick bring-up, demo, or release-readiness."),
        ("2. Software gate", "Run firmware/app/bridge/docs checks first."),
        ("3. Handshake", "Boot, enumerate, HELLO, identify the device."),
        ("4. Intended path", "Prove configurator or bridge workflow."),
        ("5. Bench behavior", "Buttons, LEDs, pots, profile baseline."),
        ("6. Stress + recovery", "Session stability and at least one recovery path."),
        ("7. Status", "Mark bring-up validated, demo-ready, or candidate."),
    ]
    y = 140
    for idx, (heading, text) in enumerate(steps):
        box = (x, y + idx * 92, x + 1220, y + idx * 92 + 68)
        draw_round_panel(draw, box)
        draw.text((box[0] + 18, box[1] + 14), heading, fill=TITLE, font=title_font(20))
        draw.text((box[0] + 240, box[1] + 18), text, fill=SUBTITLE, font=body_font(16))
        if idx < len(steps) - 1:
            arrow(draw, (700, box[3]), (700, box[3] + 22), ACCENT)
    save_flow(image, "validation-gates-overview.png")


def combo_map() -> None:
    image = Image.new("RGB", (1320, 760), PANEL_BG)
    draw = ImageDraw.Draw(image)
    draw_heading(draw, image, "Control-button combo map", "Learn the recovery and profile moves first, then add arp and reactive-control combos once the basic deck vocabulary feels stable.")
    button_boxes = []
    for idx in range(6):
        x1 = 100 + idx * 180
        y1 = 220
        box = (x1, y1, x1 + 120, y1 + 120)
        draw_round_panel(draw, box, fill="#1b2026")
        draw.text((x1 + 28, y1 + 42), f"Ctrl{idx}", fill="#fff8ee", font=title_font(20))
        button_boxes.append(box)

    labels = [
        ((100, 430, 410, 490), "Recovery first:\nCtrl0 + Ctrl1 + Ctrl2"),
        ((460, 430, 760, 490), "Profile memory:\nCtrl1 + Ctrl2"),
        ((810, 430, 1130, 490), "Arp control:\nCtrl2 + Ctrl4"),
        ((350, 560, 670, 620), "Reactive control:\nCtrl0 + Ctrl1 / Ctrl0 + Ctrl2"),
        ((720, 560, 1070, 620), "Mapping shortcuts:\nCtrl4 + Ctrl5 and friends"),
    ]
    for box, text in labels:
        draw_round_panel(draw, box)
        draw.text((box[0] + 18, box[1] + 14), text, fill=SUBTITLE, font=body_font(16))
    for idx in [0, 1, 2]:
        draw.rounded_rectangle(button_boxes[idx], radius=18, outline=ACCENT if idx == 0 else MINT, width=4)
    draw.rounded_rectangle(button_boxes[4], radius=18, outline=PURPLE, width=4)
    draw.rounded_rectangle(button_boxes[5], radius=18, outline=BLUE, width=4)
    arrow(draw, (160, 340), (250, 430), ACCENT)
    arrow(draw, (340, 340), (560, 430), MINT)
    arrow(draw, (880, 340), (940, 430), PURPLE)
    arrow(draw, (970, 340), (900, 560), BLUE)
    save_flow(image, "combo-map-overview.png")


def getting_started_pipeline() -> None:
    image = Image.new("RGB", (1380, 500), PANEL_BG)
    draw = ImageDraw.Draw(image)
    draw_heading(draw, image, "Getting started pipeline", "From dependencies to first handshake, this is the shortest path from a clone to a live board/app session.")
    boxes = [
        ("Install", "Python deps\nBridge/App deps"),
        ("Build", "pio run -d firmware\n-e teensy40_main"),
        ("HELLO", "Verify serial\nidentity reply"),
        ("App", "Open WebSerial UI\nand connect"),
        ("Bridge", "Optional OSC / MIDI\nhost path"),
    ]
    x = 50
    y = 180
    width = 230
    for idx, (h, text) in enumerate(boxes):
        box = (x + idx * 260, y, x + idx * 260 + width, y + 160)
        draw_box(draw, box, h, text.split("\n"))
        if idx < len(boxes) - 1:
            arrow(draw, (box[2], y + 80), (box[2] + 30, y + 80), ACCENT)
    save_flow(image, "getting-started-pipeline.png")


def webserial_workflow() -> None:
    image = Image.new("RGB", (1460, 520), PANEL_BG)
    draw = ImageDraw.Draw(image)
    draw_heading(draw, image, "WebSerial app workflow", "The browser connects, reads manifest/config, stages edits locally, then promotes staged state only after a valid ACK.")
    steps = [
        ("Connect", "WebSerial session"),
        ("Manifest", "identity + schema"),
        ("Config", "liveConfig + stagedConfig"),
        ("Stage", "validate + diff"),
        ("Apply", "ACK or resynchronize"),
    ]
    x = 60
    y = 190
    for idx, (heading, text) in enumerate(steps):
        box = (x + idx * 270, y, x + idx * 270 + 220, y + 150)
        draw_box(draw, box, heading, text.split("\n"))
        if idx < len(steps) - 1:
            arrow(draw, (box[2], y + 75), (box[2] + 40, y + 75), ACCENT)
    save_flow(image, "webserial-workflow-overview.png")


def main() -> None:
    ensure_dirs()
    annotate_configurator_top()
    annotate_configurator_bottom()
    annotate_slot_tile()
    connectivity_decision()
    bridge_routing()
    validation_gates()
    combo_map()
    getting_started_pipeline()
    webserial_workflow()
    print("Generated workflow graphics and annotated UI screenshots.")


if __name__ == "__main__":
    main()

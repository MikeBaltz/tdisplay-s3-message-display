"""Draws the graduation-gift GIF pack, sized exactly for the display (320 x 170).

Writes gifs/*.gif at the top of the project. Send them from the control page
like any other GIF. The congratulations GIF includes GIFT_NAME from
src/gift_config.h (left out while it is still the "[NAME]" placeholder).

Usage, from the firmware folder:
    python3 tools/make_gift_gifs.py [--fonts DIR]
Needs Pillow, and MPR-Medium.ttf / MPR-ExtraBold.ttf (M PLUS Rounded 1c, see
make_fonts.py) in DIR, by default this tools folder.
"""
import argparse
import math
import pathlib
import random
import re

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT_DIR = ROOT.parent / "gifs"
W, H, S = 320, 170, 3  # drawn at 3x, then scaled down for smooth edges

BG = (14, 12, 24)
CARD = (29, 24, 48)
WHITE = (255, 255, 255)
SOFT = (207, 199, 234)
MUTED = (154, 144, 190)
GLASS = (217, 210, 242)
AGAR = (232, 169, 78)
SHINE = (246, 211, 150)
CREAM = (255, 243, 220)
ROSE = (242, 126, 149)
LILAC = (158, 132, 245)
MINT = (140, 222, 176)
WARM_TEXT = (255, 221, 168)
INK = (40, 30, 60)
COLONIES = [(-0.30, -0.22, 0.17, CREAM), (0.30, -0.36, 0.10, ROSE), (0.40, 0.12, 0.14, LILAC),
            (-0.08, 0.40, 0.12, CREAM), (-0.47, 0.22, 0.08, ROSE), (0.08, 0.00, 0.07, LILAC),
            (0.30, 0.46, 0.06, ROSE)]
FONTS = {}


def font(kind, size):
    key = (kind, size)
    if key not in FONTS:
        FONTS[key] = ImageFont.truetype(str(FONT_DIR / f"MPR-{kind}.ttf"), size * S)
    return FONTS[key]


def canvas():
    image = Image.new("RGB", (W * S, H * S), BG)
    return image, ImageDraw.Draw(image)


def circle(d, x, y, r, fill):
    if r <= 0:
        return
    d.ellipse([(x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S], fill=fill)


def text(d, x, y, value, size, fill, kind="ExtraBold"):
    d.text((x * S, y * S), value, font=font(kind, size), fill=fill, anchor="mm")


def ease_out_back(t):
    t = min(max(t, 0.0), 1.0)
    c = 1.7
    return 1 + (c + 1) * (t - 1) ** 3 + c * (t - 1) ** 2


def ease_in_out(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def dish(d, x, y, r, background=BG, colony_scale=None):
    """The logo dish. colony_scale(i) -> 0..1 grows colony i (None = all full)."""
    circle(d, x, y, r, GLASS)
    circle(d, x, y, r - 2.5 * r / 26, background)
    agar = r - 4.5 * r / 26
    circle(d, x, y, agar, AGAR)
    ro, width = agar - 2.5 * r / 26, 2.0 * r / 26
    d.arc([(x - ro) * S, (y - ro) * S, (x + ro) * S, (y + ro) * S], 195, 255, fill=SHINE,
          width=max(1, int(width * S)))
    for index, (dx, dy, cr, colour) in enumerate(COLONIES):
        k = 1.0 if colony_scale is None else colony_scale(index)
        circle(d, x + dx * r, y + dy * r, cr * r * k, colour)


def sparkle(d, x, y, size, fill):
    """A four-pointed twinkle."""
    if size <= 0.3:
        return
    a, b = size, size * 0.28
    d.polygon([((x) * S, (y - a) * S), ((x + b) * S, (y - b) * S), ((x + a) * S, (y) * S),
               ((x + b) * S, (y + b) * S), ((x) * S, (y + a) * S), ((x - b) * S, (y + b) * S),
               ((x - a) * S, (y) * S), ((x - b) * S, (y - b) * S)], fill=fill)


def darker(colour, amount=0.72):
    return tuple(int(c * amount) for c in colour)


def microbe(d, x, y, r, colour, phase, look=(0.0, 0.0), blink=False, tail=True, smile=True):
    """A friendly microbe: wobbly body, a wavy flagellum, eyes and a smile."""
    if tail:
        points = []
        for i in range(14):
            u = i / 13
            tx = x - r * 0.85 - u * r * 1.5
            ty = y + r * 0.25 + math.sin(phase * 2 + u * 6) * r * 0.22 * u
            points.append((tx * S, ty * S))
        d.line(points, fill=darker(colour), width=max(1, int(r * 0.16 * S)), joint="curve")
    wobble = 1 + 0.06 * math.sin(phase * 2)
    rx, ry = r * wobble, r / wobble
    d.ellipse([(x - rx) * S, (y - ry) * S, (x + rx) * S, (y + ry) * S], fill=darker(colour, 0.85))
    d.ellipse([(x - rx * 0.93) * S, (y - ry * 0.95) * S, (x + rx * 0.93) * S, (y + ry * 0.88) * S],
              fill=colour)
    circle(d, x - rx * 0.4, y - ry * 0.45, r * 0.16, tuple(min(255, c + 40) for c in colour))
    eye_r, ex = r * 0.2, r * 0.32
    for side in (-1, 1):
        cx, cy = x + side * ex, y - r * 0.08
        if blink:
            d.line([((cx - eye_r) * S, cy * S), ((cx + eye_r) * S, cy * S)], fill=INK,
                   width=max(1, int(r * 0.09 * S)))
        else:
            circle(d, cx, cy, eye_r, WHITE)
            circle(d, cx + look[0] * eye_r * 0.4, cy + look[1] * eye_r * 0.4, eye_r * 0.55, INK)
    if smile:
        sr = r * 0.28
        d.arc([(x - sr) * S, (y + r * 0.05 - sr * 0.6) * S, (x + sr) * S, (y + r * 0.05 + sr) * S],
              20, 160, fill=INK, width=max(1, int(r * 0.08 * S)))
    for side in (-1, 1):  # rosy cheeks
        circle(d, x + side * r * 0.58, y + r * 0.2, r * 0.11, (255, 160, 180))


def heart(d, x, y, size, fill):
    pts = []
    for i in range(60):
        t = i / 60 * 2 * math.pi
        hx = 16 * math.sin(t) ** 3
        hy = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append(((x + hx * size / 16) * S, (y - hy * size / 16) * S))
    d.polygon(pts, fill=fill)


def save(name, frames, durations):
    """One shared palette for all frames, no dithering: no flicker, small files."""
    small = [f.resize((W, H), Image.LANCZOS) for f in frames]
    sheet = Image.new("RGB", (W, H * len(small)))
    for i, frame in enumerate(small):
        sheet.paste(frame, (0, i * H))
    palette = sheet.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    quantized = [f.quantize(palette=palette, dither=Image.Dither.NONE) for f in small]
    path = OUT_DIR / name
    quantized[0].save(path, save_all=True, append_images=quantized[1:], duration=durations,
                      loop=0, optimize=False, disposal=1)
    print(f"wrote {path} ({len(frames)} frames, {path.stat().st_size // 1024} KB)")


# ------------------------------------------------------------------ the pack --

def congrats(name):
    frames, durations = [], []
    n = 46
    rng = random.Random(3)
    stars = [(rng.uniform(20, 300), rng.uniform(12, 100), rng.uniform(0, 6.28)) for _ in range(9)]
    stars = [s for s in stars if abs(s[0] - 160) > 52]
    for f in range(n):
        image, d = canvas()
        t = f / n
        dish(d, 160, 54, 40, colony_scale=lambda i: ease_out_back((f - 2 - i * 2.2) / 6))
        if f > 18:
            for sx, sy, sp in stars:
                sparkle(d, sx, sy, 4.5 * max(0, math.sin(sp + t * 12)), WARM_TEXT)
        k = ease_out_back((f - 17) / 8)
        if k > 0:
            text(d, 160, 118 + (1 - k) * 12, "Congratulations!", 26, WHITE)
        if name and f > 22:
            k2 = ease_in_out((f - 22) / 6)
            text(d, 160, 148, name, 18, tuple(int(BG[c] + (WARM_TEXT[c] - BG[c]) * k2) for c in range(3)))
        frames.append(image)
        durations.append(1600 if f == n - 1 else 60)
    save("congrats.gif", frames, durations)


def cap_toss():
    frames, durations = [], []
    n = 48
    rng = random.Random(7)
    colours = [CREAM, ROSE, LILAC, AGAR, MINT]
    confetti = [(rng.uniform(0, W), rng.uniform(0, H + 20), rng.choice(colours), rng.uniform(0, 6.28),
                 rng.choice([1, 2])) for _ in range(34)]
    for f in range(n):
        image, d = canvas()
        t = f / n
        for cx, cy, colour, spin, laps in confetti:  # whole laps per loop: seamless
            y = (cy + t * laps * (H + 20)) % (H + 20) - 10
            x = cx + math.sin(spin + t * 6.28 * 2) * 6
            a = spin + t * 6.28 * 3
            w, h = 3.2 * abs(math.cos(a)) + 0.6, 1.8
            d.polygon([((x - w) * S, (y - h) * S), ((x + w) * S, (y - h * 0.4) * S),
                       ((x + w) * S, (y + h) * S), ((x - w) * S, (y + h * 0.4) * S)], fill=colour)
        # The cap: up, a spin at the top, back down.
        height = math.sin(math.pi * t)
        cy = 120 - 78 * height
        angle = 2 * math.pi * ease_in_out(t)
        cx = 160
        rx, ry = 34, 13
        board = [(cx + rx * math.cos(angle + q * math.pi / 2), cy + ry * math.sin(angle + q * math.pi / 2))
                 for q in range(4)]
        d.polygon([((cx - 18) * S, (cy + 2) * S), ((cx + 18) * S, (cy + 2) * S),
                   ((cx + 15) * S, (cy + 16) * S), ((cx - 15) * S, (cy + 16) * S)], fill=(44, 36, 74))
        d.polygon([(px * S, py * S) for px, py in board], fill=(58, 48, 98))
        d.line([(px * S, py * S) for px, py in board + board[:1]], fill=(98, 84, 150), width=S)
        circle(d, cx, cy, 2.6, AGAR)
        tip = board[0]
        tassel_end = (tip[0] + 2, tip[1] + 16 + 3 * math.sin(t * 20))
        d.line([(cx * S, cy * S), (tip[0] * S, tip[1] * S), (tassel_end[0] * S, tassel_end[1] * S)],
               fill=AGAR, width=int(1.6 * S))
        circle(d, tassel_end[0], tassel_end[1] + 2, 2.4, AGAR)
        d.rounded_rectangle([60 * S, 136 * S, 260 * S, 164 * S], 14 * S, fill=CARD)
        text(d, 160, 150, "Hooray, graduate!", 18, WHITE)
        frames.append(image)
        durations.append(60)
    save("cap_toss.gif", frames, durations)


def microbe_party():
    frames, durations = [], []
    n = 40
    # Evenly spaced round the dish, all at the same pace, so nobody bumps into anyone.
    cast = [(ROSE, 15, 0.0, 2), (LILAC, 13, math.pi / 2, 3), (CREAM, 12, math.pi, 2),
            (MINT, 12, 3 * math.pi / 2, 3)]
    for f in range(n):
        image, d = canvas()
        t = f / n
        circle(d, 160, 85, 80, GLASS)
        circle(d, 160, 85, 77, BG)
        circle(d, 160, 85, 73, AGAR)
        d.arc([(160 - 66) * S, (85 - 66) * S, (160 + 66) * S, (85 + 66) * S], 195, 250, fill=SHINE,
              width=int(4 * S))
        for colour, r, offset, laps in cast:
            a = offset + t * 2 * math.pi
            x = 160 + math.cos(a) * 40
            y = 86 + math.sin(a) * 30 - abs(math.sin(t * 2 * math.pi * laps + offset)) * 7
            blink = (f + int(offset * 7)) % n in (5, 6)
            microbe(d, x, y, r, colour, a * 3, look=(math.cos(a + 1.5), 0.3), blink=blink)
        for i in range(5):  # little music notes
            nt = (t + i / 5) % 1
            nx, ny = 40 + i * 60 + math.sin(nt * 6.28) * 6, 150 - nt * 130
            if 0.05 < nt < 0.9:
                circle(d, nx, ny, 3, MUTED)
                d.line([((nx + 2.6) * S, ny * S), ((nx + 2.6) * S, (ny - 10) * S)], fill=MUTED, width=S)
        frames.append(image)
        durations.append(70)
    save("microbe_party.gif", frames, durations)


def good_morning():
    frames, durations = [], []
    n = 44
    for f in range(n):
        image, d = canvas()
        t = f / n
        rise = ease_in_out(f / 18)
        sun_y = 150 - 70 * rise
        for ray in range(10):
            a = ray * math.pi / 5 + t * math.pi / 5 * 2
            r1, r2 = 27, 35 + 3 * math.sin(t * 12.56 + ray)
            d.line([((230 + math.cos(a) * r1) * S, (sun_y + math.sin(a) * r1) * S),
                    ((230 + math.cos(a) * r2) * S, (sun_y + math.sin(a) * r2) * S)],
                   fill=AGAR, width=int(3.2 * S))
        circle(d, 230, sun_y, 22, SHINE)
        circle(d, 230, sun_y, 18, AGAR)
        d.rectangle([0, 132 * S, W * S, H * S], fill=BG)  # sun sets behind the horizon
        d.ellipse([-60 * S, 128 * S, (W + 60) * S, 260 * S], fill=AGAR)
        for x, r, colour in [(40, 5, CREAM), (120, 3, ROSE), (285, 4, LILAC), (180, 2.5, CREAM)]:
            circle(d, x, 142, r, colour)
        stretch = max(0.0, math.sin(min(1.0, f / 26) * math.pi))  # a big yawn as the sun comes up
        microbe(d, 82, 112 - 4 * stretch, 18, LILAC, t * 6.28 * 2, blink=f < 16, look=(0.6, -0.3),
                smile=f >= 16)
        if f < 16:
            circle(d, 82, 118, 4 + 2 * stretch, INK)  # yawning mouth
        text(d, 112, 32, "Good morning!", 24, WHITE)
        frames.append(image)
        durations.append(1500 if f == n - 1 else 65)
    save("good_morning.gif", frames, durations)


def miss_you():
    frames, durations = [], []
    n = 44
    for f in range(n):
        image, d = canvas()
        t = f / n
        meet = ease_in_out(f / 22)
        bob = math.sin(t * 6.28 * 2) * 3
        microbe(d, 60 + 66 * meet, 82 + bob, 22, ROSE, t * 12.56, look=(1, 0), tail=False)
        microbe(d, 260 - 66 * meet, 82 - bob, 22, LILAC, t * 12.56 + 1, look=(-1, 0), tail=False)
        k = ease_out_back((f - 22) / 7)
        if k > 0:
            heart(d, 160, 42 - 6 * math.sin(max(0, f - 22) / 4), 13 * k, ROSE)
            for i, (hx, hy) in enumerate([(128, 36), (194, 30), (176, 18)]):
                sparkle(d, hx, hy, 3.5 * max(0, math.sin(f / 3 + i * 2)), WARM_TEXT)
        text(d, 160, 140, "Miss you!", 24, WHITE)
        frames.append(image)
        durations.append(1500 if f == n - 1 else 60)
    save("miss_you.gif", frames, durations)


def good_luck():
    frames, durations = [], []
    n = 40
    for f in range(n):
        image, d = canvas()
        t = f / n
        spin = t * math.pi / 2  # a quarter turn per loop looks like a full one: seamless
        cx, cy = 160, 60
        d.line([(cx * S, cy * S), ((cx + 3) * S, (cy + 22) * S), ((cx + 9) * S, (cy + 38) * S)],
               fill=darker(MINT), width=int(3 * S), joint="curve")
        for q in range(4):  # four heart-shaped leaves with a gap between them
            a = spin + q * math.pi / 2
            lx, ly = cx + math.cos(a) * 15, cy + math.sin(a) * 15
            for side in (-1, 1):
                circle(d, lx + math.cos(a + side * math.pi / 2) * 5.5, ly + math.sin(a + side * math.pi / 2) * 5.5,
                       8.5, MINT)
            tip = (cx + math.cos(a) * 4, cy + math.sin(a) * 4)
            d.polygon([(tip[0] * S, tip[1] * S),
                       ((lx + math.cos(a + math.pi / 2) * 12) * S, (ly + math.sin(a + math.pi / 2) * 12) * S),
                       ((lx + math.cos(a - math.pi / 2) * 12) * S, (ly + math.sin(a - math.pi / 2) * 12) * S)],
                      fill=MINT)
            d.line([(cx * S, cy * S), ((cx + math.cos(a) * 20) * S, (cy + math.sin(a) * 20) * S)],
                   fill=darker(MINT, 0.85), width=S)
        circle(d, cx, cy, 10, MINT)
        circle(d, cx, cy, 3, darker(MINT, 0.8))
        for q in range(4):
            a = spin + q * math.pi / 2 + math.pi / 4
            circle(d, cx + math.cos(a) * 40, cy + math.sin(a) * 40, 3.4, [CREAM, ROSE, LILAC, AGAR][q])
        for i, (sx, sy) in enumerate([(96, 30), (226, 44), (110, 92), (214, 96)]):
            sparkle(d, sx, sy, 5 * max(0, math.sin(t * 6.28 * 2 + i * 1.6)), WARM_TEXT)
        text(d, 160, 132, "Good luck!", 26, WHITE)
        text(d, 160, 156, "You've got this", 15, MUTED, kind="Medium")
        frames.append(image)
        durations.append(70)
    save("good_luck.gif", frames, durations)


def gift_name():
    config = (ROOT / "src" / "gift_config.h").read_text(encoding="utf-8")
    match = re.search(r'GIFT_NAME\[\]\s*=\s*"([^"]*)"', config)
    name = match.group(1) if match else ""
    return "" if name.startswith("[") else name


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fonts", default=str(pathlib.Path(__file__).resolve().parent))
    FONT_DIR = pathlib.Path(parser.parse_args().fonts)
    OUT_DIR.mkdir(exist_ok=True)
    congrats(gift_name())
    cap_toss()
    microbe_party()
    good_morning()
    miss_you()
    good_luck()

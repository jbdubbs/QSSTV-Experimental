#!/usr/bin/env python3
"""Draws the Google Play feature graphic (1024 x 500) into packaging/android/store/feature-graphic.png.

  python3 -I packaging/android/store/make-feature-graphic.py [output.png]

Needs Pillow and the Noto Sans variable font (or another font: FONT=/path/to/font.ttf). Everything on it is drawn here or taken from
the app icon (packaging/linux/icons), so there is nothing to license: the colour bars are the generic television test signal.
"""
import math, os, random, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "feature-graphic.png")
W, H, S = 1024, 500, 2            # the size Play wants; everything is drawn at S times that and scaled down
random.seed(7)

def font(size, weight="Regular"):
    for path in (os.environ.get("FONT"), "/usr/share/fonts/google-noto-vf/NotoSans[wght].ttf",
                 "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf"):
        if path and os.path.exists(path):
            f = ImageFont.truetype(path, size * S)
            try: f.set_variation_by_name(weight)
            except Exception: pass
            return f
    raise SystemExit("no font found: set FONT=/path/to/font.ttf")

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

# --- background: deep teal to near black, diagonal
bg = Image.new("RGB", (W * S, H * S))
px = bg.load()
c0, c1 = (9, 52, 66), (4, 14, 20)
for y in range(H * S):
    for x in range(W * S):
        px[x, y] = lerp(c0, c1, min(1.0, (x / (W * S) * 0.55 + y / (H * S) * 0.45)))

# --- the waterfall of the program: glowing sine traces, the way two SSTV tones look on a spectrum
glow = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
g = ImageDraw.Draw(glow)
for k, (amp, lam, ph, x0, a) in enumerate([(70, 190, 0.0, 700, 170), (45, 150, 1.7, 790, 120), (95, 260, 3.1, 610, 90), (30, 120, 4.4, 880, 80)]):
    pts = [((x0 + amp * math.sin(2 * math.pi * y / lam + ph)) * S, y * S) for y in range(-10, H + 11)]
    g.line(pts, fill=(77, 208, 225, a), width=3 * S, joint="curve")
sharp = glow.copy()
glow = glow.filter(ImageFilter.GaussianBlur(7 * S))
bg = Image.alpha_composite(bg.convert("RGBA"), glow)
bg = Image.alpha_composite(bg, sharp)
d = ImageDraw.Draw(bg, "RGBA")
for _ in range(330):    # faint noise specks, like the band between two stations
    x, y = random.randrange(W * S), random.randrange(H * S)
    d.rectangle([x, y, x + S, y + S], fill=(160, 220, 230, random.randrange(10, 45)))

# --- the picture being received: colour bars on top, the rest not yet there, a bright line where the picture is
fx, fy, fw, fh = 632, 100, 336, 252
frame = Image.new("RGBA", (fw * S, fh * S), (0, 0, 0, 255))
fd = ImageDraw.Draw(frame)
bars = [(192, 192, 192), (192, 192, 0), (0, 192, 192), (0, 192, 0), (192, 0, 192), (192, 0, 0), (0, 0, 192)]
done = int(fh * 0.70)
bw = fw / 7
for i, c in enumerate(bars):
    fd.rectangle([round(i * bw * S), 0, round((i + 1) * bw * S), int(done * 0.70 * S)], fill=c)
rev = [(0, 0, 192), (19, 19, 19), (192, 0, 192), (19, 19, 19), (0, 192, 192), (19, 19, 19), (192, 192, 192)]
for i, c in enumerate(rev):
    fd.rectangle([round(i * bw * S), int(done * 0.70 * S), round((i + 1) * bw * S), int(done * 0.80 * S)], fill=c)
for i, c in enumerate([(0, 33, 76), (255, 255, 255), (50, 0, 106), (19, 19, 19)]):
    fd.rectangle([round(i * fw * 0.2 * S), int(done * 0.80 * S), round((i + 1) * fw * 0.2 * S), done * S], fill=c)
fd.rectangle([int(fw * 0.8 * S), int(done * 0.80 * S), fw * S, done * S], fill=(19, 19, 19))
for y in range(done * S, fh * S):    # not received yet: dark with a little static
    for x in range(0, fw * S, 3):
        v = random.randrange(8, 38)
        fd.point((x, y), fill=(v, v, v + 6, 255))
fd.rectangle([0, done * S - S, fw * S, done * S + S], fill=(255, 255, 255, 255))     # the line that is being drawn right now
fglow = frame.crop((0, done * S - 40, fw * S, done * S + 40)).filter(ImageFilter.GaussianBlur(10 * S))
mask = Image.new("L", (fw * S, fh * S), 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, fw * S - 1, fh * S - 1], radius=16 * S, fill=255)
frame.alpha_composite(Image.merge("RGBA", (*fglow.split()[:3], fglow.split()[3].point(lambda v: v // 2))), (0, done * S - 40))
frame.putalpha(mask)
shadow = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
ImageDraw.Draw(shadow).rounded_rectangle([fx * S, (fy + 10) * S, (fx + fw) * S, (fy + fh + 10) * S], radius=18 * S, fill=(0, 0, 0, 170))
bg = Image.alpha_composite(bg, shadow.filter(ImageFilter.GaussianBlur(14 * S)))
ImageDraw.Draw(bg).rounded_rectangle([(fx - 5) * S, (fy - 5) * S, (fx + fw + 5) * S, (fy + fh + 5) * S], radius=20 * S, fill=(236, 244, 247, 255))
bg.alpha_composite(frame, (fx * S, fy * S))
d = ImageDraw.Draw(bg, "RGBA")
d.text(((fx + fw // 2) * S, (fy + fh + 38) * S), "Martin 1  ·  Scottie  ·  PD  ·  Robot  ·  DRM", font=font(17, "Medium"), fill=(190, 225, 232, 230), anchor="mm")

# --- the icon, the name, what it is
icon = Image.open(os.path.join(ROOT, "packaging", "linux", "icons", "qsstv-experimental-512.png")).convert("RGBA").resize((128 * S, 128 * S), Image.LANCZOS)
m = Image.new("L", icon.size, 0)
ImageDraw.Draw(m).rounded_rectangle([0, 0, icon.size[0] - 1, icon.size[1] - 1], radius=28 * S, fill=255)
icon.putalpha(m)
bg.alpha_composite(icon, (64 * S, 66 * S))
d = ImageDraw.Draw(bg, "RGBA")
d.text((64 * S, 268 * S), "QSSTV-Experimental", font=font(50, "Bold"), fill=(255, 255, 255, 255), anchor="ls")
d.text((66 * S, 312 * S), "Slow-scan TV over amateur radio", font=font(28, "Medium"), fill=(190, 232, 240, 255), anchor="ls")
d.text((66 * S, 352 * S), "Send and receive pictures with your phone.", font=font(24), fill=(170, 205, 214, 255), anchor="ls")
x = 66
for label in ("Analog SSTV", "Digital DRM", "No ads"):
    f = font(20, "Medium"); w = d.textlength(label, font=f) / S + 32
    d.rounded_rectangle([x * S, 396 * S, (x + w) * S, 436 * S], radius=20 * S, outline=(77, 208, 225, 255), width=2 * S)
    d.text(((x + w / 2) * S, 416 * S), label, font=f, fill=(220, 244, 248, 255), anchor="mm")
    x += w + 14

out = bg.convert("RGB").resize((W, H), Image.LANCZOS)     # no alpha channel: Play rejects a feature graphic that has one
out.save(OUT, optimize=True)
print(OUT, out.size)

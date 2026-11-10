#!/usr/bin/env python3
"""Makes the Android launcher icon and the Google Play icon from the app icon (#204).

  python3 -I packaging/android/make-launcher-icon.py                  writes the files below
  python3 -I packaging/android/make-launcher-icon.py --preview p.png  only draws a preview of the adaptive icon under four masks

The app icon (packaging/linux/icons/qsstv-experimental-512.png) is a Q on coloured noise with rounded, transparent corners. Android puts
an icon with transparent corners on a white plate and shrinks it, so the launcher showed whitespace around it. What is made here:

  res/drawable-nodpi/ic_launcher_background.png   the adaptive icon layer: the picture over the whole 108 dp canvas, no edge of its own,
                                                  so every launcher mask (circle, squircle, rounded square) cuts the picture itself
  res/mipmap-*dpi/ic_launcher.png                 the old square icons, now without the transparent corners (below minSdk, kept as the
                                                  default resource)
  store/icon-512.png                              the Google Play icon: full square, Play rounds it

res/mipmap-anydpi-v26/ic_launcher.xml (written by hand) puts the layer into an <adaptive-icon>. Needs Pillow only.
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "packaging", "linux", "icons", "qsstv-experimental-512.png")
RES = os.path.join(HERE, "res")
STORE = os.path.join(HERE, "store")

CORNER = 54          # px of the 512 icon: the transparent rounded corners (radius ~51) with the anti-aliased edge
OUTLINE = 7          # px: the orange and cyan edge around the white Q
LAYER_DP, VISIBLE_DP, SAFE_DP = 108, 72, 66   # Android's adaptive icon: the canvas, what the biggest mask shows, what every mask shows
PX_PER_DP = 4        # the layer is drawn for xxxhdpi
LAYERS = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}   # the old square icons

def clean_square():
    """The app icon as a full square: each transparent corner is covered with the mirror image of the noise next to it."""
    src = Image.open(SRC).convert("RGBA")
    w, h = src.size
    out = src.copy()
    c = CORNER
    for dx, sx in ((0, c), (w - c, w - 2 * c)):
        for dy in (0, h - c):
            out.paste(ImageOps.mirror(src.crop((sx, dy, sx + c, dy + c))), (dx, dy))
    out.putalpha(255)
    return out

def q_reach(img):
    """How far from the middle the Q reaches (its tail), in px of the source icon: the white of the letter plus its coloured edge."""
    w, h = img.size
    px = img.load()
    far = 0.0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y][:3]
            if min(r, g, b) >= 250:
                far = max(far, math.hypot(x + 0.5 - w / 2, y + 0.5 - h / 2))
    return far + OUTLINE

def adaptive_layer(img):
    """The picture, sized so the whole Q is inside the 66 dp every mask shows, and mirrored outward to fill the 108 dp canvas."""
    size = LAYER_DP * PX_PER_DP
    scale = SAFE_DP / 2 * PX_PER_DP / q_reach(img)
    n = round(img.width * scale)
    pic = img.convert("RGB").resize((n, n), Image.LANCZOS)
    tiles = Image.new("RGB", (3 * n, 3 * n))       # 3 x 3 tiles, each one mirrored against its neighbors, the middle one as it is
    for j in range(3):
        for i in range(3):
            t = pic if i == 1 else ImageOps.mirror(pic)
            t = t if j == 1 else ImageOps.flip(t)
            tiles.paste(t, (i * n, j * n))
    off = n + (n - size) // 2
    return tiles.crop((off, off, off + size, off + size))

def mask(kind, size):
    """The shape a launcher cuts out of the visible part, as an L image."""
    m = Image.new("L", (size * 4, size * 4), 0)
    d = ImageDraw.Draw(m)
    full = (0, 0, size * 4 - 1, size * 4 - 1)
    if kind == "circle":
        d.ellipse(full, fill=255)
    elif kind == "rounded":
        d.rounded_rectangle(full, radius=size * 4 * 0.2, fill=255)
    elif kind == "squircle":
        r = size * 2
        d.polygon([(r + r * math.copysign(abs(math.cos(a)) ** 0.5, math.cos(a)),
                    r + r * math.copysign(abs(math.sin(a)) ** 0.5, math.sin(a)))
                   for a in (k * math.pi / 180 for k in range(360))], fill=255)
    else:
        d.rectangle(full, fill=255)
    return m.resize((size, size), Image.LANCZOS)

def preview(layer, path):
    """The layer under a circle, a squircle, a rounded square and a square, on gray, with the 66 dp safe zone drawn thin."""
    vis = VISIBLE_DP * PX_PER_DP
    off = (layer.width - vis) // 2
    shown = layer.crop((off, off, off + vis, off + vis))
    gap = 24
    sheet = Image.new("RGB", (4 * vis + 5 * gap, vis + 2 * gap), (150, 150, 150))
    for k, kind in enumerate(("circle", "squircle", "rounded", "square")):
        tile = Image.new("RGB", (vis, vis), (150, 150, 150))
        tile.paste(shown, (0, 0), mask(kind, vis))
        d = ImageDraw.Draw(tile)
        m = (vis - SAFE_DP * PX_PER_DP) // 2
        d.ellipse((m, m, vis - m, vis - m), outline=(255, 255, 255), width=1)
        sheet.paste(tile, (gap + k * (vis + gap), gap))
    sheet.save(path)

def main():
    img = clean_square()
    layer = adaptive_layer(img)
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        preview(layer, sys.argv[2])
        return
    os.makedirs(os.path.join(RES, "drawable-nodpi"), exist_ok=True)
    layer.save(os.path.join(RES, "drawable-nodpi", "ic_launcher_background.png"), optimize=True)
    for name, px in LAYERS.items():
        os.makedirs(os.path.join(RES, "mipmap-" + name), exist_ok=True)
        img.convert("RGB").resize((px, px), Image.LANCZOS).save(os.path.join(RES, "mipmap-" + name, "ic_launcher.png"), optimize=True)
    img.save(os.path.join(STORE, "icon-512.png"), optimize=True)   # RGBA with alpha 255 everywhere: Play asks for a 32-bit PNG

if __name__ == "__main__":
    main()

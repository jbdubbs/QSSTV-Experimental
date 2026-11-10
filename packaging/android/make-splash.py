#!/usr/bin/env python3
"""Makes the start-up screen graphic of the Android app from the splash graphic of the desktop program.

  python3 -I packaging/android/make-splash.py                  writes the two PNGs below
  python3 -I packaging/android/make-splash.py --preview p.png  only draws them on the light and the dark window colour

The desktop splash (src/icons/qsstvsplash.png, 736 x 330) is black text and the noise bar on opaque white. The phone's start-up window is
the program's light or dark window colour, so the graphic is cut to what is drawn on it and the white is taken away:

  res/drawable-nodpi/splash_logo.png         for the light window: the black text stays black
  res/drawable-night-nodpi/splash_logo.png   for the dark window: the black text is white; the orange text and the noise are as they are

res/drawable/splash_icon.xml (written by hand) sets how big it is shown. Needs Pillow only.
"""
import os, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "src", "icons", "qsstvsplash.png")
RES = os.path.join(HERE, "res")

INK = 37                      # the grey of the black text at full strength in the desktop splash
LIGHT_INK = 245               # what the text becomes on the dark window
PAD = 3                       # px of nothing around the graphic
LIGHT_WINDOW, DARK_WINDOW = (0xef, 0xef, 0xf4), (0x1f, 0x20, 0x24)    # res/values/colors.xml, res/values-night/colors.xml

def make(dark):
    src = Image.open(SRC).convert("RGB")
    w, h = src.size
    px = src.load()
    drawn = [(x, y) for y in range(h) for x in range(w) if min(px[x, y]) < 250]
    x0, x1 = min(p[0] for p in drawn), max(p[0] for p in drawn)
    y0, y1 = min(p[1] for p in drawn), max(p[1] for p in drawn)
    # the noise bar: the rows (at the bottom) where the whole width is drawn
    bar_rows = [y for y in range(y0, y1 + 1) if sum(1 for x in range(x0, x1 + 1) if min(px[x, y]) < 250) >= 0.95 * (x1 - x0 + 1)]
    bar_y0, bar_y1 = bar_rows[0], bar_rows[-1]

    def deficit(x, y):                                  # how far from white a pixel is
        return 255 - min(px[x, y])

    def coloured(x, y, ink_x, ink_y):
        """A pixel on the white that is part colour: the colour of the ink next to it, as much of it as the pixel is far from white."""
        d, ink = deficit(x, y), deficit(ink_x, ink_y)
        if d >= ink * 0.96:
            return px[x, y] + (255,)                    # the ink itself
        return (px[ink_x, ink_y] + (round(255 * d / ink),)) if d > 4 else (0, 0, 0, 0)

    out = Image.new("RGBA", (x1 - x0 + 1 + 2 * PAD, y1 - y0 + 1 + 2 * PAD), (0, 0, 0, 0))
    op = out.load()
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            r, g, b = px[x, y]
            if y in bar_rows:
                if bar_y0 < y < bar_y1 and x0 < x < x1:
                    res = (r, g, b, 255)                # inside the bar: exactly as it is
                else:                                   # its one-pixel edge, soft on the white: the colour of the pixel inside it
                    res = coloured(x, y, min(max(x, x0 + 1), x1 - 1), min(max(y, bar_y0 + 1), bar_y1 - 1))
            elif max(r, g, b) - min(r, g, b) < 12:      # the black text and its soft edge: how much ink there is is how dark the pixel is
                a = min(1.0, (255 - (r + g + b) / 3) / (255 - INK))
                v = LIGHT_INK if dark else INK
                res = (v, v, v, round(a * 255)) if a > 0.02 else (0, 0, 0, 0)
            else:                                       # the orange text: the colour of the strongest pixel next to it
                near = [(x + i, y + j) for i in range(-2, 3) for j in range(-2, 3)
                        if 0 <= x + i < w and 0 <= y + j < h and y + j < bar_y0 - 2 and max(px[x + i, y + j]) - min(px[x + i, y + j]) >= 12]
                ink = max(near, key=lambda p: deficit(*p))
                res = coloured(x, y, *ink)
            op[x - x0 + PAD, y - y0 + PAD] = res
    return out

def preview(path):
    pics = [(make(False), LIGHT_WINDOW), (make(True), DARK_WINDOW)]
    w, h = pics[0][0].size
    sheet = Image.new("RGB", (2 * w + 60, h + 40))
    for k, (pic, bg) in enumerate(pics):
        tile = Image.new("RGB", (w + 20, h + 40), bg)
        tile.paste(pic, (10, 20), pic)
        sheet.paste(tile, (k * (w + 40), 0))
    sheet.save(path)

def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        preview(sys.argv[2])
        return
    for folder, dark in (("drawable-nodpi", False), ("drawable-night-nodpi", True)):
        os.makedirs(os.path.join(RES, folder), exist_ok=True)
        pic = make(dark)
        pic.save(os.path.join(RES, folder, "splash_logo.png"), optimize=True)
    print("size %d x %d px: shown 180 dp wide it is %.1f dp high" % (pic.width, pic.height, 180 * pic.height / pic.width))

if __name__ == "__main__":
    main()

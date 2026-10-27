#!/usr/bin/env python3
"""Helpers for run.sh.

  imgtool.py make CARD OUTDIR   write stored<o>.png (the card's pixels as a camera would store them for EXIF orientation o)
                                and orient<o>.jpg (the same pixels carrying the Orientation tag) for o = 6, 8 and 3
  imgtool.py psnr REF GOT       luma PSNR in dB of two pictures (GOT is scaled to REF's size if it differs)
  imgtool.py upright FILE REF   luma PSNR of FILE after applying its own EXIF orientation, against REF (checks "make")
"""
import math
import os
import sys
from PIL import Image, ImageChops, ImageOps

# EXIF orientation -> how the card must be turned to get the pixels a camera stores for it (display = the inverse)
STORE = {6: Image.Transpose.ROTATE_90, 8: Image.Transpose.ROTATE_270, 3: Image.Transpose.ROTATE_180}


def psnr(ref, got):
    a = Image.open(ref).convert("L")
    b = Image.open(got).convert("L")
    if b.size != a.size:
        b = b.resize(a.size, Image.Resampling.LANCZOS)
    hist = ImageChops.difference(a, b).histogram()
    mse = sum(n * i * i for i, n in enumerate(hist)) / (a.width * a.height)
    return 99.0 if mse == 0 else 10 * math.log10(255 * 255 / mse)


def main():
    cmd = sys.argv[1]
    if cmd == "make":
        card = Image.open(sys.argv[2]).convert("RGB")
        for o, t in STORE.items():
            stored = card.transpose(t)
            stored.save(os.path.join(sys.argv[3], "stored%d.png" % o))
            exif = Image.Exif()
            exif[0x0112] = o
            stored.save(os.path.join(sys.argv[3], "orient%d.jpg" % o), "JPEG", quality=95, exif=exif)
    elif cmd == "psnr":
        print("%.2f" % psnr(sys.argv[2], sys.argv[3]))
    elif cmd == "upright":
        im = ImageOps.exif_transpose(Image.open(sys.argv[2]))
        tmp = sys.argv[2] + ".upright.png"
        im.convert("RGB").save(tmp)
        print("%.2f" % psnr(sys.argv[3], tmp))
        os.remove(tmp)
    else:
        sys.exit("unknown command " + cmd)


main()

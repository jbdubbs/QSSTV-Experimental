#!/usr/bin/env python3
"""Compare a decoded Robot picture with the test picture at the centres of its patches (issue #207).

  check_rx.py decoded.png [--gray]

The test picture is made by this script too (check_rx.py --make out.png): 8 gray steps, 8 colour bars and 4 solid colours, 320 x 240.
Every patch of the decoded picture (scaled to 320 x 240) must be within 12 levels per channel of what was sent (--gray: of its
luma only). Exit status 1 and a list of the patches that are off otherwise. Needs python3, numpy and Pillow.
"""
import sys
sys.dont_write_bytecode = True
import numpy as np
from PIL import Image

GRAY = [0, 36, 73, 109, 146, 182, 219, 255]
BARS = [(255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0), (255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0)]
SOLID = [(230, 100, 40), (60, 120, 200), (128, 128, 128), (200, 200, 60)]


def make():
    im = np.zeros((240, 320, 3), np.uint8)
    for i in range(8):
        im[0:80, i * 40:(i + 1) * 40] = GRAY[i]
        im[80:160, i * 40:(i + 1) * 40] = BARS[i]
    for i in range(4):
        im[160:240, i * 80:(i + 1) * 80] = SOLID[i]
    return im


def patches():
    for i in range(8):
        yield 'gray %d' % GRAY[i], (40, i * 40 + 20), (GRAY[i],) * 3
    for i in range(8):
        yield 'bar %s' % (BARS[i],), (120, i * 40 + 20), BARS[i]
    for i in range(4):
        yield 'solid %s' % (SOLID[i],), (200, i * 80 + 40), SOLID[i]


def main():
    if sys.argv[1] == '--make':
        Image.fromarray(make()).save(sys.argv[2]); return
    gray = '--gray' in sys.argv
    d = np.asarray(Image.open(sys.argv[1]).convert('RGB').resize((320, 240), Image.BILINEAR)).astype(int)
    bad = []
    for name, (y, x), want in patches():
        got = d[y - 4:y + 5, x - 4:x + 5].reshape(-1, 3).mean(axis=0)
        if gray:
            g = 0.299 * got[0] + 0.587 * got[1] + 0.114 * got[2]; w = 0.299 * want[0] + 0.587 * want[1] + 0.114 * want[2]
            err = abs(g - w)
        else:
            err = np.abs(got - np.array(want)).max()
        if err > 12:
            bad.append('%s: got %s' % (name, got.round().astype(int).tolist()))
    for b in bad: print('    off: ' + b)
    sys.exit(1 if bad else 0)


main()

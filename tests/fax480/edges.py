#!/usr/bin/env python3
"""edges.py picture.png: size, the phasing lines and where the steps of the test picture (0, 96, 192 and 255 at a quarter, a half and three quarters
of the line) are.  prints: width height white_rows_at_top first_row_of_the_picture offset_ms"""
import sys, numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert('L'), float); H, W = a.shape
white = int((a[:, W // 8:W - W // 8].mean(axis=1) > 240).argmin())      # rows at the top that are white all over
first = int(np.where(a[:, 20] < 30)[0][0])                              # the first row where the leftmost step is black
def edge(v, mid, exp):
    for j in range(exp - 30, exp + 30):
        if v[j] < mid <= v[j + 1]: return j + (mid - v[j]) / (v[j + 1] - v[j]) + 0.5 - exp
    return float('nan')
v = a[first + 100:first + 120].mean(axis=0)
m = np.nanmean([edge(v, 48, W // 4), edge(v, 144, W // 2), edge(v, 223.5, 3 * W // 4)])
print("%d %d %d %d %.3f" % (W, H, white, first, m * 0.512))

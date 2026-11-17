#!/usr/bin/env python3
"""edges.py picture.png pixel_ms: where the red, green and blue steps (at 1/4, 1/2 and 3/4 of the width) are, in px and ms, mean over three rows"""
import sys, numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert('RGB'), float); H, W = a.shape[:2]
def edge(v, exp):
    mid = (v.min() + v.max()) / 2
    for j in range(max(exp - 30, 0), min(exp + 30, len(v) - 1)):
        if v[j] < mid <= v[j + 1]: return j + (mid - v[j]) / (v[j + 1] - v[j]) + 0.5 - exp
    return float('nan')
m = np.nanmean([edge(a[r - 3:r + 3, :, c].mean(axis=0), (W // 4, W // 2, 3 * W // 4)[c]) for r in (H // 8, H // 2, H - H // 8) for c in range(3)])
print("%d %d %.3f %.3f" % (W, H, m, m * float(sys.argv[2])))

#!/usr/bin/env python3
"""Where the transmitter puts the segments of a PD line (#212).

measure.py WAV MODE   (WAV from qsstv-experimental --encode of a picture that is black left of PIXEL and white from it)
Prints, in ms after the end of a sync pulse, the start of the first and the second luminance segment (the instant the
black-to-white step crosses 1700 Hz, minus the step's pixel offset), and the distance between the two, and exits 1 when
the transmitter is more than TOL ms off the standard (porch 2.08 ms, four segments of Y ms).
"""
import sys, struct, numpy as np
from scipy.signal import hilbert

SPEC = {"PD50": (320, 91.520), "PD90": (320, 170.240), "PD120": (640, 121.600)}
PIXEL = 100
TOL = 0.06   # ms; the old table was 0.14 ms off between the two luminance segments

raw = open(sys.argv[1], "rb").read()
ch, fs = struct.unpack("<HI", raw[22:28])
x = np.frombuffer(raw[44:44 + (len(raw) - 44) // (2 * ch) * 2 * ch], dtype=np.int16).astype(float)
x = x[::ch]
npx, ymsec = SPEC[sys.argv[2]]
ph = np.unwrap(np.angle(hilbert(x)))
f = np.convolve(np.diff(ph) * fs / (2 * np.pi), np.ones(5) / 5, "same")
m = (f > 1100) & (f < 1300)
e = np.flatnonzero(np.diff(m.astype(int)))
runs = [(e[i], e[i + 1]) for i in range(len(e) - 1) if m[e[i] + 1] and 0.017 < (e[i + 1] - e[i]) / fs < 0.023 and e[i] / fs > 1.8]
y1, y2 = [], []
for a, b in zip(runs[3:-3], runs[4:-2]):
    seg = f[a[1]:b[0]]
    up = np.flatnonzero((seg[:-1] < 1700) & (seg[1:] >= 1700)) / fs * 1000
    if len(up) >= 2:
        y1.append(up[0]); y2.append(up[1])
y1, y2 = np.median(y1), np.median(y2)
px = ymsec / npx
want1 = 2.08 + PIXEL * px
want2 = 2.08 + 3 * ymsec + PIXEL * px
print("%s: Y1 step %.3f ms (standard %.3f)  Y2 step %.3f ms (standard %.3f)  Y2-Y1 %.3f (standard %.3f)" % (sys.argv[2], y1, want1, y2, want2, y2 - y1, 3 * ymsec))
bad = abs(y1 - want1) > TOL + 0.05 or abs(y2 - want2) > TOL + 0.05 or abs((y2 - y1) - 3 * ymsec) > TOL
print("FAIL" if bad else "ok")
sys.exit(1 if bad else 0)

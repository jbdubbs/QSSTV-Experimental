#!/usr/bin/env python3
"""measure.py transmission.wav: the start tone and the line period of a FAX480 transmission
prints:  start-tone-seconds  first-sync-seconds  period-ms  syncs
The start tone is the 1500/2300 Hz square wave, the sync is the 1200 Hz pulse that begins every line."""
import sys, struct, numpy as np
from scipy.signal import hilbert
raw = open(sys.argv[1], 'rb').read(); ch, fs = struct.unpack("<HI", raw[22:28])
x = np.frombuffer(raw[44:44 + (len(raw) - 44) // (2 * ch) * 2 * ch], dtype=np.int16).astype(float)[::ch]
a = hilbert(x); f = np.diff(np.unwrap(np.angle(a))) * fs / (2 * np.pi)
k = int(fs * 0.0008); f = np.convolve(f, np.ones(k) / k, 'same')
sync = np.abs(f - 1200) < 150
runs = np.diff(np.r_[0, sync.astype(int), 0]); st = np.where(runs == 1)[0]; en = np.where(runs == -1)[0]
pulses = [(s, e) for s, e in zip(st, en) if 0.003 <= (e - s) / fs <= 0.0075]       # 5.12 ms (the smoothing of the frequency makes it read about 4.6)
# the start tone: the first of 100 tones in a row that alternate between 1500 and 2300 Hz and last about 2 ms (the preamble and the VIS code that
# come before it have tones of 100 ms and 30 ms)
lab = np.where(np.abs(f - 1500) < 150, 1, np.where(np.abs(f - 2300) < 150, 2, 0))
ch_ = np.where(np.diff(lab) != 0)[0] + 1
bounds = np.r_[0, ch_, len(lab)]
segs = [(lab[b0], b0, b1) for b0, b1 in zip(bounds[:-1], bounds[1:]) if lab[b0] in (1, 2) and b1 - b0 > fs * 0.0004]   # (the frequency passes the ones between them)
t0 = None
for i in range(len(segs) - 100):
    if all(segs[i + j][0] != segs[i + j + 1][0] and 0.0012 < (segs[i + j][2] - segs[i + j][1]) / fs < 0.0026 for j in range(99)):
        t0 = segs[i][1] / fs; break
pulses = [(s_, e_) for s_, e_ in pulses if s_ / fs > t0 + 4.9]     # the pulses of the lines
first = pulses[0][0] / fs
p = np.array([s_ for s_, e_ in pulses]) / fs
# period of the lines: the pulses that are one line apart
d = np.diff(p); d = d[(d > 0.26) & (d < 0.28)]
slope = np.polyfit(np.arange(len(p)), p, 1)[0]
print("%.4f %.4f %.4f %d" % (first - t0, first, slope * 1000, len(p)))

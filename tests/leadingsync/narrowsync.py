#!/usr/bin/env python3
"""narrowsync.py FILE.wav : for a narrow-mode transmission (24 bit VIS code of 22 ms bits at 1900/2100 Hz), the length (ms) of the 1900 Hz run
that follows the end of the code, which is the sync pulse of the first line (9 ms in MP-N, 8 ms in MC-N), or "nan". The code ends 22 ms * 25
after the end of the 100 ms break at 2100 Hz that follows the 300 ms leader. Needs python3 with numpy; reads the encoder's WAV files with tests/fade/wavtool.py."""
import os, sys
import numpy as np
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fade'))
from wavtool import read_wav  # noqa: E402


def tones(path, t0=0.0, t1=8.0, step=0.001):
    x, sr = read_wav(path)
    x = x[int(t0 * sr):int(t1 * sr)]
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(len(x), 1 / sr)
    X[(fr < 900) | (fr > 2600)] = 0
    x = np.fft.irfft(X, len(x))
    n = len(x)
    h = np.zeros(n)
    h[0] = 1
    h[1:(n + 1) // 2] = 2
    if n % 2 == 0:
        h[n // 2] = 1
    ph = np.unwrap(np.angle(np.fft.ifft(np.fft.fft(x) * h)))
    f = np.append(np.diff(ph), 0) * sr / (2 * np.pi)
    k = int(step * sr)
    return np.array([np.median(f[i:i + k]) for i in range(0, len(f) - k, k)]), step


def sync_after_code(path):
    f, step = tones(path)
    cl = np.where(np.abs(f - 2100) < 60, 'B', np.where(np.abs(f - 1900) < 60, 'L', '.'))
    runs, i = [], 0
    while i < len(cl):
        j = i
        while j < len(cl) and cl[j] == cl[i]:
            j += 1
        runs.append((cl[i], i * step, (j - i) * step * 1000))
        i = j
    for a in range(1, len(runs)):
        if runs[a][0] == 'B' and runs[a][2] >= 80 and runs[a - 1][0] == 'L' and runs[a - 1][2] >= 250:
            end = runs[a][1] + runs[a][2] / 1000 + 0.022 * 25          # the end of the code
            j = int((end + 0.0015) / step)
            k = j
            while k < len(f) and abs(f[k] - 1900) < 60:
                k += 1
            return (k * step - end) * 1000 + 0.5 * step * 1000
    return None


if __name__ == '__main__':
    for p in sys.argv[1:]:
        v = sync_after_code(p)
        print('nan' if v is None else '%.1f' % v)

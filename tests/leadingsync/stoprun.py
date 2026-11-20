#!/usr/bin/env python3
"""stoprun.py FILE.wav : length (ms) of the 1200 Hz run that follows the data bits of the VIS code (the stop bit, plus the sync pulse of
the first line when the transmitter sends it right after the stop bit). Prints "nan" when no VIS code is found in the first 5 s.
Needs python3 with numpy. Reads the encoder's WAV files (unfinished sizes, stereo) with tests/fade/wavtool.py."""
import os, sys
import numpy as np
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fade'))
from wavtool import read_wav  # noqa: E402


def classify(path, t0=0.0, t1=5.0):
    x, sr = read_wav(path)
    x = x[int(t0 * sr):int(t1 * sr)]
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(len(x), 1 / sr)
    X[(fr < 900) | (fr > 2400)] = 0
    x = np.fft.irfft(X, len(x))
    n = len(x)
    S = np.fft.fft(x)
    h = np.zeros(n)
    h[0] = 1
    h[1:(n + 1) // 2] = 2
    if n % 2 == 0:
        h[n // 2] = 1
    ph = np.unwrap(np.angle(np.fft.ifft(S * h)))
    f = np.append(np.diff(ph), 0) * sr / (2 * np.pi)
    k = int(0.002 * sr)
    step = k // 4
    fm = np.array([np.median(f[i:i + k]) for i in range(0, len(f) - k, step)])
    cls = np.where(np.abs(fm - 1900) < 150, 'L', np.where(np.abs(fm - 1200) < 70, 'S', np.where(np.abs(fm - 1100) < 45, '0',
          np.where(np.abs(fm - 1300) < 45, '1', '.'))))
    runs, i = [], 0
    while i < len(cls):
        j = i
        while j < len(cls) and cls[j] == cls[i]:
            j += 1
        runs.append((cls[i], i * step / sr, (j - i) * step / sr * 1000))
        i = j
    return [r for r in runs if r[2] >= 8]


def stop_run(path):
    r = classify(path)
    for i in range(1, len(r)):
        if r[i][0] == 'S' and r[i][2] >= 24 and r[i - 1][0] == 'L' and r[i - 1][2] >= 90:     # the start bit after the leader
            seen = False
            for j in range(i + 1, len(r)):
                if r[j][0] in '01':
                    seen = True
                if seen and r[j][0] == 'S' and r[j][2] >= 24:
                    return r[j][2]
            return None
    return None


if __name__ == '__main__':
    for p in sys.argv[1:]:
        v = stop_run(p)
        print('nan' if v is None else '%.1f' % v)

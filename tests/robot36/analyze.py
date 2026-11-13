#!/usr/bin/env python3
"""Measure the timing of a Robot 36 transmission in a wav file against the MMSSTV / Robot spec (issue #207).

  analyze.py file.wav [--lines N]

Prints, for the first N lines after the VIS code, the start of the 1200 Hz sync pulse, the duration of the 1500/2300 Hz
separator and of the 1900 Hz porch after it, and the line period. Spec (MMSSTV LineR36): sync 9 ms, porch 3 ms (1500 Hz),
Y 88 ms, separator 4.5 ms (1500 Hz even line, 2300 Hz odd line), porch 1.5 ms (1900 Hz), chroma 44 ms; line 150 ms.
Needs python3 and numpy.
"""
import argparse, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fade'))
import numpy as np
from wavtool import read_wav


def inst_freq(x, rate):
    """instantaneous frequency (Hz) from the analytic signal, light smoothing"""
    n = len(x)
    X = np.fft.fft(x)
    h = np.zeros(n); h[0] = 1; h[1:(n + 1) // 2] = 2
    if n % 2 == 0: h[n // 2] = 1
    a = np.fft.ifft(X * h)
    ph = np.unwrap(np.angle(a))
    f = np.diff(ph) * rate / (2 * np.pi)
    k = max(1, int(rate * 0.0003))
    return np.convolve(f, np.ones(k) / k, mode='same')


def runs(mask):
    """[(start, end)) of the True runs"""
    d = np.diff(np.concatenate(([0], mask.astype(np.int8), [0])))
    return list(zip(np.where(d == 1)[0], np.where(d == -1)[0]))


def analyze(path, lines=8, quiet=False):
    x, rate = read_wav(path)
    f = inst_freq(x, rate)
    ms = 1000.0 / rate
    # sync pulses: 1200 Hz runs between 7 and 12 ms (the VIS stop bit is 30 ms, the leader tones are longer / other frequency)
    pulses = [(s, e) for s, e in runs((f > 1120) & (f < 1280)) if 6 <= (e - s) * ms <= 12]
    res = {'rate': rate, 'syncs': [], 'sep': [], 'porch': [], 'period': []}
    for i, (s, e) in enumerate(pulses[:lines]):
        res['syncs'].append((s * ms, (e - s) * ms))
        # what follows the next sync is the 1500 Hz porch then Y; the separator is BEFORE the sync of the next line in our
        # cyclic line order only when a line is Y,sep,porch,chroma,sync: look at the 1500/2300 run right before the 1900 run
    # separator and porch: with a flat picture (Y near 1900 Hz, chroma far from it, tests/robot36/run.sh) the separator is the
    # only run of 1500 / 2300 Hz between the Y and the chroma. Nominally it starts 91 ms after the end of the sync pulse.
    for (s, e) in pulses[:lines]:
        a = int(e + 88 * rate / 1000.0)
        w = f[a:a + int(rate * 0.012)]
        tone = 1500 if w.min() < 1700 and (w < 1700).sum() > (w > 2100).sum() else 2300
        m = np.abs(w - tone) < 120
        r = runs(m)
        if not r: continue
        s0, e0 = max(r, key=lambda t: t[1] - t[0])
        c = np.where(np.abs(w[e0:] - 1900) > 120)[0]          # first sample of the chroma after the separator
        res['sep'].append((tone, (e0 - s0) * ms))
        res['porch'].append(((a + s0) * ms, (c[0] * ms) if len(c) else float('nan')))
    ps = [s for s, _ in res['syncs']]
    res['period'] = list(np.diff(ps))
    if not quiet:
        print('rate %d Hz, %d sync pulses found' % (rate, len(pulses)))
        for i, (t, d) in enumerate(res['syncs']):
            print('  sync %2d at %9.2f ms, %.2f ms long' % (i, t, d))
        print('  periods ms:', ' '.join('%.2f' % p for p in res['period']))
        for (t, d), (tone, sd) in list(zip(res['porch'], res['sep']))[:lines]:
            print('  separator %d Hz %.2f ms, then 1900 Hz porch %.2f ms (separator at %.2f ms)' % (tone, sd, d, t))
    return res


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('wav'); ap.add_argument('--lines', type=int, default=8)
    a = ap.parse_args()
    analyze(a.wav, a.lines)

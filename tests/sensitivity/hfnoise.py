#!/usr/bin/env python3
"""Weak-signal helpers for the sensitivity test. Needs python3, numpy and Pillow.

  hfnoise.py noise in.wav out.wav --snr dB [--seed N] [--lead S] [--tail S] [--length S] [--qsb PERIOD DEPTH_DB]
      Adds noise like the audio of an SSB receiver: white Gaussian noise limited to 300-2700 Hz. --snr is the signal power
      (the transmission has a constant envelope) over the noise power in 2.4 kHz, the way HF signal reports are given, so
      the same dB figure holds for every mode. --lead S puts S seconds of noise in front of the signal and --tail S behind
      it (a receiver that hunts before the picture comes). --length S cuts the result to S seconds. --qsb fades the signal
      (not the noise) slowly: raised cosine of PERIOD seconds, DEPTH_DB deep.

  hfnoise.py corr clean.png decoded.png [--px N] [--rows N]
      How much of a received picture can be recognized: the share of the rows (those with some detail in the clean picture)
      whose luminance correlates with the clean row at better than 0.25 (noise alone gives 0 +- 0.06 over 320 pixels). The
      picture may be moved sideways (--px, default 8) and up or down (--rows, default 48) as a whole, because a picture
      that was started late sits higher than it should; the best move is used. Prints "share 0.xx (moved dx px, dy rows)".

The encoder leaves the RIFF/data sizes unfinished; the WAV reader of tests/fade/wavtool.py copes with that.
"""
import argparse
import os
import sys

import numpy as np

sys.dont_write_bytecode = True      # importing tests/fade/wavtool.py must not leave a __pycache__ in the repo
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fade'))
from wavtool import read_wav, write_wav, signal_rms  # noqa: E402


def band_noise(n, rate, lo, hi, rng):
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1.0 / rate)
    spec[(f < lo) | (f > hi)] = 0
    y = np.fft.irfft(spec, n)
    return y / np.sqrt(np.mean(y ** 2))          # unit power in the 2.4 kHz band


def cmd_noise(a):
    x, rate = read_wav(a.infile)
    rms = signal_rms(x)
    if a.qsb:
        period, depth = a.qsb
        floor = 10 ** (-depth / 20.0)
        t = np.arange(len(x)) / float(rate)
        x = x * (floor + (1 - floor) * 0.5 * (1 + np.cos(2 * np.pi * t / period)))
    x = np.concatenate([np.zeros(int(a.lead * rate)), x, np.zeros(int(a.tail * rate))])
    rng = np.random.default_rng(a.seed)
    y = x + band_noise(len(x), rate, 300.0, 2700.0, rng) * rms * 10 ** (-a.snr / 20.0)
    if a.length:
        y = y[:int(a.length * rate)]
    peak = np.max(np.abs(y))
    if peak > 30000:
        y *= 30000 / peak
    write_wav(a.outfile, y, rate)


def luma(path):
    from PIL import Image
    return np.asarray(Image.open(path).convert('L'), dtype=np.float64)


def row_corr(c, d):
    cc = c - c.mean(axis=1, keepdims=True)
    dd = d - d.mean(axis=1, keepdims=True)
    den = np.sqrt((cc ** 2).sum(axis=1) * (dd ** 2).sum(axis=1))
    return np.where(den > 0, (cc * dd).sum(axis=1) / np.where(den > 0, den, 1), 0.0)


def cmd_corr(a):
    c, d = luma(a.clean), luma(a.decoded)
    if c.shape != d.shape:
        print('share 0.00 (size differs)')
        return 0
    detail = c.std(axis=1) > 12.0
    if not detail.any():
        print('share 1.00')
        return 0
    h = len(c)
    # vertical move first, from the row means (cheap), then the sideways move with the whole rows
    pc = c.mean(axis=1) - c.mean()
    pd = d.mean(axis=1) - d.mean()
    best_dy, best_score = 0, -1e18
    for dy in range(-a.rows, a.rows + 1):
        lo, hi = max(0, dy), min(h, h + dy)
        s = float(np.dot(pc[lo:hi], pd[lo - dy:hi - dy])) / max(1, hi - lo)
        if s > best_score:
            best_dy, best_score = dy, s
    best = (-1.0, 0)
    for dx in range(-a.px, a.px + 1):
        m = np.roll(d, dx, axis=1)
        lo, hi = max(0, best_dy), min(h, h + best_dy)
        r = row_corr(c[lo:hi], m[lo - best_dy:hi - best_dy])
        share = float(np.sum((r > 0.25) & detail[lo:hi])) / max(1, int(detail.sum()))
        if share > best[0]:
            best = (share, dx)
    print('share %.2f (moved %d px, %d rows)' % (best[0], best[1], best_dy))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    n = sub.add_parser('noise')
    n.add_argument('infile')
    n.add_argument('outfile')
    n.add_argument('--snr', type=float, required=True)
    n.add_argument('--seed', type=int, default=1)
    n.add_argument('--lead', type=float, default=0.0)
    n.add_argument('--tail', type=float, default=0.0)
    n.add_argument('--length', type=float, default=0.0)
    n.add_argument('--qsb', type=float, nargs=2, metavar=('PERIOD', 'DEPTH_DB'), default=None)
    n.set_defaults(fn=cmd_noise)
    c = sub.add_parser('corr')
    c.add_argument('clean')
    c.add_argument('decoded')
    c.add_argument('--px', type=int, default=8)
    c.add_argument('--rows', type=int, default=48)
    c.set_defaults(fn=cmd_corr)
    a = ap.parse_args()
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()

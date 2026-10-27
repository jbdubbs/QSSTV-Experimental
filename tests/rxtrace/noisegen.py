#!/usr/bin/env python3
"""Synthetic stand-in for a noisy HF band (issue #135): white noise with sync-like tone bursts at random times.

  noisegen.py out.wav [--minutes 20] [--rate 48000] [--sigma 1500] [--bursts-per-s 18] [--burst-gain 3] [--seed 1]

A recording of a real band (tests/rxtrace/capture.sh) gave about 16 pulses a second that pass the receiver's width test.
Their times are what makes the receiver start pictures on noise: four of them in a row on the line grid of any mode are
enough. Each burst here is a 1200 Hz tone of 4 to 25 ms (the widths a sync pulse may have) at a Poisson random time, on
white noise of rms --sigma (int16 units); --burst-gain is the burst amplitude in sigmas. Needs python3 with numpy.
"""
import argparse
import os
import sys

import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fade'))
import wavtool  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('out')
    ap.add_argument('--minutes', type=float, default=20)
    ap.add_argument('--rate', type=int, default=48000)
    ap.add_argument('--sigma', type=float, default=1500)
    ap.add_argument('--bursts-per-s', type=float, default=18)
    ap.add_argument('--burst-gain', type=float, default=3)
    ap.add_argument('--seed', type=int, default=1)
    a = ap.parse_args()
    rng = np.random.default_rng(a.seed)
    n = int(a.minutes * 60 * a.rate)
    x = rng.standard_normal(n) * a.sigma
    t = 0.0
    total = a.minutes * 60
    while True:
        t += rng.exponential(1.0 / a.bursts_per_s)
        if t >= total:
            break
        width = rng.uniform(0.004, 0.025)
        i0, k = int(t * a.rate), int(width * a.rate)
        if i0 + k >= n:
            break
        env = np.sin(np.pi * np.arange(k) / k) ** 0.3       # near flat top, soft edges
        f = 1200 + rng.uniform(-30, 30)
        x[i0:i0 + k] += a.burst_gain * a.sigma * env * np.sin(2 * np.pi * f * np.arange(k) / a.rate + rng.uniform(0, 6.28))
    wavtool.write_wav(a.out, x, a.rate)


if __name__ == '__main__':
    main()

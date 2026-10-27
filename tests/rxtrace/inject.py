#!/usr/bin/env python3
"""Mix clean SSTV transmissions into a recording of noise (issue #135): is a real signal still received when it starts
while the receiver is held by a noise picture?

  inject.py NOISE.wav OUT.wav --at SECONDS:SIGNAL.wav [--at ...] [--snr dB]

Each SIGNAL.wav (a tests/fade style recording: --encode output, or `wavtool.py cut` of it) is added to NOISE at SECONDS. It is
scaled so that its rms is --snr dB (default 15) above the rms of the noise around it, and resampled to the rate of NOISE.
Needs python3 with numpy; reuses read_wav/write_wav/signal_rms of tests/fade/wavtool.py.
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
    ap.add_argument('noise')
    ap.add_argument('out')
    ap.add_argument('--at', action='append', default=[], metavar='SECONDS:SIGNAL.wav')
    ap.add_argument('--snr', type=float, default=15.0)
    a = ap.parse_args()
    x, rate = wavtool.read_wav(a.noise)
    y = x.copy()
    for spec in a.at:
        sec, path = spec.split(':', 1)
        t0 = int(float(sec) * rate)
        s, srate = wavtool.read_wav(path)
        if srate != rate:
            s = np.interp(np.arange(int(len(s) * rate / float(srate))) * srate / float(rate), np.arange(len(s)), s)
        seg = x[max(0, t0 - 10 * rate):t0 + len(s)]
        noise_rms = np.sqrt(np.mean(seg ** 2))
        s = s * (noise_rms * 10 ** (a.snr / 20.0) / wavtool.signal_rms(s))
        n = min(len(s), len(y) - t0)
        if n > 0:
            y[t0:t0 + n] += s[:n]
        print('%s at %.1f s: %.1f s, signal rms %.0f, noise rms %.0f' % (os.path.basename(path), t0 / float(rate), len(s) / float(rate), wavtool.signal_rms(s), noise_rms))
    wavtool.write_wav(a.out, y, rate)


if __name__ == '__main__':
    main()

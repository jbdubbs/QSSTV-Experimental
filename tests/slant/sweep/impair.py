#!/usr/bin/env python3
"""impair.py in.wav out.wav [--clock D] [--snr dB] [--seed N] [--lead S] [--qsb PERIOD DEPTH_DB]

Turns a clean transmission into what a receiver with a wrong sound clock and a noisy channel hears.
--clock D: the receiver's sound card runs D (a fraction, e.g. 0.01 = 1 %) faster than it thinks (the recording is
           played 1/(1+D) as long as the transmission); negative D = slower. Cubic interpolation.
--snr dB:  band-limited (300-2700 Hz) Gaussian noise, SNR re the signal power in 2.4 kHz.
--lead S:  S seconds of noise before the signal and 10 s after it.
--qsb P DEPTH: slow fading of the signal (raised cosine, period P s, DEPTH dB deep), the noise stays.
Writes a mono 16 bit WAV at the same rate. The encoder's WAV has an unfinished header; the reader copes with that.
"""
import sys, struct, argparse
import numpy as np
from scipy.ndimage import map_coordinates

def read_wav(fn):
    d = open(fn, 'rb').read()
    pos = 12
    ch, rate = 1, 48000
    while pos + 8 <= len(d):
        tag = d[pos:pos+4]
        sz = struct.unpack('<I', d[pos+4:pos+8])[0]
        if tag == b'fmt ':
            ch = struct.unpack('<H', d[pos+10:pos+12])[0]
            rate = struct.unpack('<I', d[pos+12:pos+16])[0]
        elif tag == b'data':
            body = d[pos+8:]
            n = len(body) // 2
            x = np.frombuffer(body[:n*2], dtype='<i2').astype(np.float64)
            if ch > 1:
                x = x[:len(x)//ch*ch].reshape(-1, ch)[:, 0]
            return x, rate
        pos += 8 + sz + (sz & 1)
    raise SystemExit('no data chunk')

def write_wav(fn, x, rate):
    x = np.clip(np.round(x), -32768, 32767).astype('<i2')
    with open(fn, 'wb') as f:
        n = len(x) * 2
        f.write(b'RIFF' + struct.pack('<I', 36 + n) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16))
        f.write(b'data' + struct.pack('<I', n) + x.tobytes())

def band_noise(n, rate, rng):
    w = rng.standard_normal(n)
    W = np.fft.rfft(w)
    f = np.fft.rfftfreq(n, 1.0 / rate)
    W[(f < 300) | (f > 2700)] = 0
    y = np.fft.irfft(W, n)
    return y / np.sqrt(np.mean(y ** 2))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('inp'); ap.add_argument('out')
    ap.add_argument('--clock', type=float, default=0.0)
    ap.add_argument('--snr', type=float, default=None)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--lead', type=float, default=1.0)
    ap.add_argument('--qsb', type=float, nargs=2, default=None)
    a = ap.parse_args()
    x, rate = read_wav(a.inp)
    if a.clock != 0.0:
        m = int(len(x) / (1.0 + a.clock))
        pos = np.arange(m) * (1.0 + a.clock)
        x = map_coordinates(x, [pos], order=3, mode='nearest')
    amp = np.sqrt(np.mean(x ** 2))
    lead = int(a.lead * rate); tail = int(10 * rate)
    sig = np.concatenate([np.zeros(lead), x, np.zeros(tail)])
    if a.qsb:
        per, depth = a.qsb
        t = np.arange(len(sig)) / rate
        g = 10 ** (-depth / 20.0)
        env = g + (1 - g) * 0.5 * (1 + np.cos(2 * np.pi * t / per))
        sig = sig * env
    if a.snr is not None:
        rng = np.random.default_rng(a.seed)
        sig = sig + amp * 10 ** (-a.snr / 20.0) * band_noise(len(sig), rate, rng)
    write_wav(a.out, sig, rate)

main()

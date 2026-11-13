#!/usr/bin/env python3
"""Write a Robot transmission (R24, R36, R72, BW8 = MMSSTV RM8, BW12 = RM12) the way MMSSTV does (issue #207): a direct port of
TMmsstv::LineR24, LineR36, LineR72, LineRM (Main.cpp), GetRY and ColorToFreq (ComLib.cpp) from the MMSSTV sources, with the usual
header (leader, break, leader, VIS code).

  mmsstv_robot.py MODE picture.png out.wav [--rate 48000] [--no-first-sync]

The picture is scaled to 320 x 240 (R24: 320 x 120). Needs python3, numpy and Pillow.
MMSSTV differences from a plain full-range encoder: Y = 16 + 0.2568R + 0.5041G + 0.0979B (studio swing, black is 1550 Hz,
white 2234 Hz), R-Y and B-Y = 128 + 0.4392R - 0.3678G - 0.0714B and 128 - 0.1482R - 0.2910G + 0.4392B, frequency 1500 + d * 800 / 256,
the separator is 4.5 ms and the porch 1.5 ms, every line starts with its sync pulse (9 ms) and porch (3 ms).
"""
import argparse, struct
import numpy as np
from PIL import Image


class Mod:
    def __init__(self, rate):
        self.rate = rate; self.phase = 0.0; self.out = []; self.carry = 0.0
    def write(self, hz, ms):
        """tone of `ms`; the sample count keeps the fraction (MMSSTV does the same with its tick counter)"""
        n = ms * self.rate / 1000.0 + self.carry
        k = int(round(n)); self.carry = n - k
        if k <= 0: return
        ph = self.phase + 2 * np.pi * hz / self.rate * np.arange(1, k + 1)
        self.out.append(np.sin(ph)); self.phase = ph[-1] % (2 * np.pi)
    def samples(self):
        return np.concatenate(self.out)


def color_to_freq(d):
    return 1500 + int(d * (2300 - 1500) / 256)


def get_ry(r, g, b):
    y = 16.0 + 0.256773 * r + 0.504097 * g + 0.097900 * b
    ry = 128.0 + 0.439187 * r - 0.367766 * g - 0.071421 * b
    by = 128.0 - 0.148213 * r - 0.290974 * g + 0.439187 * b
    return [int(np.clip(v, 0, 255)) for v in (y, ry, by)]


def write_vis(m, code):
    m.write(1200, 30)
    for i in range(8):
        m.write(1100 if (code >> i) & 1 else 1300, 30)
    m.write(1200, 30)


VIS = {'R36': 0x88, 'R24': 0x84, 'R72': 0x0C, 'BW8': 0x82, 'BW12': 0x86}


def encode(mode, png, rate=48000, first_sync=True):
    h = 120 if mode == 'R24' else 240
    im = np.asarray(Image.open(png).convert('RGB').resize((320, h), Image.LANCZOS)).astype(float)
    m = Mod(rate)
    m.write(1900, 300); m.write(1200, 10); m.write(1900, 300)
    write_vis(m, VIS[mode])
    ryby = lambda line: list(zip(*[get_ry(*im[line, x]) for x in range(320)]))
    if mode in ('BW8', 'BW12'):
        tw = 58.89709 if mode == 'BW8' else 92.0
        for line in range(0, 240, 2):                      # LineRM: two rows averaged into one line
            y0 = ryby(line)[0]; y1 = ryby(line + 1)[0]
            m.write(1200, 6); m.write(1500, 2.0)
            for x in range(320): m.write(color_to_freq((y0[x] + y1[x]) // 2), tw / 320.0)
        m.write(1500, 100)
        return m.samples()
    for line in range(h):
        yy, ry, by = ryby(line)
        if mode == 'R24':
            m.write(1200, 6); m.write(1500, 2)
            for x in range(320): m.write(color_to_freq(yy[x]), 92.0 / 320.0)
            m.write(1500, 3.0); m.write(1900, 1.0)
            for x in range(320): m.write(color_to_freq(ry[x]), 46.0 / 320.0)
            m.write(2300, 3.0); m.write(1900, 1.0)
            for x in range(320): m.write(color_to_freq(by[x]), 46.0 / 320.0)
        elif mode == 'R72':
            m.write(1200, 9); m.write(1500, 3)
            for x in range(320): m.write(color_to_freq(yy[x]), 138.0 / 320.0)
            m.write(1500, 4.5); m.write(1900, 1.5)
            for x in range(320): m.write(color_to_freq(ry[x]), 69.0 / 320.0)
            m.write(2300, 4.5); m.write(1900, 1.5)
            for x in range(320): m.write(color_to_freq(by[x]), 69.0 / 320.0)
        else:
            if first_sync or line:
                m.write(1200, 9); m.write(1500, 3)
            else:
                m.write(1500, 3)
            for x in range(320): m.write(color_to_freq(yy[x]), 88.0 / 320.0)
            m.write(2300 if line & 1 else 1500, 4.5); m.write(1900, 1.5)
            for x in range(320): m.write(color_to_freq(by[x] if line & 1 else ry[x]), 44.0 / 320.0)
    m.write(1500, 100)
    return m.samples()


def write_wav(path, x, rate):
    d = (np.clip(x, -1, 1) * 0.5 * 32767).astype('<i2').tobytes()
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(d)) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16))
        f.write(b'data' + struct.pack('<I', len(d)) + d)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('mode', choices=sorted(VIS)); ap.add_argument('png'); ap.add_argument('wav'); ap.add_argument('--rate', type=int, default=48000)
    ap.add_argument('--no-first-sync', action='store_true')
    a = ap.parse_args()
    write_wav(a.wav, encode(a.mode, a.png, a.rate, not a.no_first_sync), a.rate)

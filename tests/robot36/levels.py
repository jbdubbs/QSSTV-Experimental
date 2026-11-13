#!/usr/bin/env python3
"""Check the tone frequencies of a flat-colour Robot transmission of ours against what MMSSTV sends (issue #207).

  levels.py MODE file.wav R G B

The wav is our own --encode --mode MODE of a picture of one colour. MMSSTV sends Y = 16 + 0.2568R + 0.5041G + 0.0979B,
R-Y / B-Y = 128 + ..., at 1500 + level * 800 / 256 Hz (mmsstv_robot.get_ry). Prints the measured and the expected frequency of
every part of the line and exits with status 1 when one is more than 15 Hz off. Needs python3 and numpy.
"""
import os, sys
sys.dont_write_bytecode = True
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here); sys.path.insert(0, os.path.join(here, '..', 'fade'))
import numpy as np
from analyze import inst_freq, runs
from wavtool import read_wav
from mmsstv_robot import get_ry

# (name, start ms, end ms) after the end of a sync pulse, in our line order (sync at the end of the line)
SEG = {
    'R36': [('Y', 3, 91), ('C', 97, 141)],
    'R72': [('Y', 3, 141), ('R-Y', 147, 216), ('B-Y', 222, 291)],
    'R24': [('Y', 2, 94), ('R-Y', 98, 144), ('B-Y', 148, 194)],
    'BW8': [('Y', 2, 60.9)],
    'BW12': [('Y', 2, 94)],
}


def hz(level):
    return 1500 + level * 800 / 256.0


def main():
    mode = sys.argv[1]
    path = sys.argv[2]; rgb = [int(v) for v in sys.argv[3:6]]
    x, rate = read_wav(path)
    f = inst_freq(x, rate)
    pulses = [(s, e) for s, e in runs((f > 1120) & (f < 1280)) if 4.5 <= (e - s) * 1000.0 / rate <= 12]
    y, ry, by = get_ry(*rgb)
    expect = {'Y': hz(y), 'R-Y': hz(ry), 'B-Y': hz(by), 'C': None}
    bad = 0
    for k in (3, 4):                                    # two consecutive lines (R36 alternates R-Y and B-Y)
        e = pulses[k][1]
        out = []
        for name, a, b in SEG[mode]:
            w = f[e + int((a + 0.2 * (b - a)) * rate / 1000): e + int((a + 0.8 * (b - a)) * rate / 1000)]
            m = float(np.median(w))
            if name == 'C':
                want = min((hz(ry), hz(by)), key=lambda v: abs(v - m)); name = 'R-Y' if want == hz(ry) else 'B-Y'
            else:
                want = expect[name]
            ok = abs(m - want) <= 15
            bad += not ok
            out.append('%s %.0f Hz (MMSSTV %.0f)%s' % (name, m, want, '' if ok else ' <-- OFF'))
        print('  line %d: %s' % (k, ', '.join(out)))
    sys.exit(1 if bad else 0)


main()

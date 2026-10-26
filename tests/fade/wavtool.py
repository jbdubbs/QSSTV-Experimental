#!/usr/bin/env python3
"""WAV and PNG helpers for the fading-signal test (issue #129). Needs python3, numpy and Pillow.

  wavtool.py fade in.wav out.wav (--start S | --start-frac F) --hold H [--ramp R] [--snr dB] [--floor-db dB]
                                 [--repeat N] [--gap S]
      The signal sinks into the noise and comes back. It is multiplied by a gain that falls (raised cosine, R seconds)
      from 1 to --floor-db below, stays there H seconds, and rises again. White noise --snr dB below the signal's rms
      is added to the whole file, so the dropout is noise, not silence (silence would hide every false-sync problem)
      and the picture outside the fade stays decodable. --start-frac is a fraction of the file's length.

  wavtool.py join out.wav A.wav FRACTION GAP B.wav [--snr dB]
      The first FRACTION of A, then GAP seconds of noise (a transmission that stops for good), then all of B.

  wavtool.py rows clean.png decoded.png TOP BOTTOM
      Compare two pictures row by row; print the share of rows that agree in the top TOP and the bottom BOTTOM
      fraction of the picture ("top 1.00 bottom 0.98"). Each end may be moved up to 8 px sideways and 6 rows to fit.

The encoder leaves the RIFF/data sizes unfinished, so the data chunk is read to the end of the file. Output is 16 bit
PCM mono at the input's rate.
"""
import argparse
import struct
import sys

import numpy as np


def read_wav(path):
    d = open(path, 'rb').read()
    if d[:4] != b'RIFF' or d[8:12] != b'WAVE':
        sys.exit('%s: not a WAV file' % path)
    pos, fmt = 12, None
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack('<I', d[pos + 4:pos + 8])[0]
        body = pos + 8
        if cid == b'fmt ':
            tag, ch, rate, _, _, bits = struct.unpack('<HHIIHH', d[body:body + 16])
            fmt = (tag, ch, rate, bits)
        elif cid == b'data':
            if fmt is None:
                sys.exit('%s: data chunk before the fmt chunk' % path)
            tag, ch, rate, bits = fmt
            if tag != 1 or bits != 16:
                sys.exit('%s: only 16 bit PCM is supported (tag %d, %d bits)' % (path, tag, bits))
            end = len(d) if size in (0, 0xFFFFFFFF) or body + size > len(d) else body + size
            x = np.frombuffer(d[body:end - ((end - body) % (2 * ch))], dtype='<i2').astype(np.float64)
            return x.reshape(-1, ch).mean(axis=1), rate
        pos = body + size + (size & 1)
    sys.exit('%s: no data chunk' % path)


def write_wav(path, x, rate):
    pcm = np.clip(np.round(x), -32768, 32767).astype('<i2').tobytes()
    hdr = b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16)
    open(path, 'wb').write(hdr + b'data' + struct.pack('<I', len(pcm)) + pcm)


def signal_rms(x):
    active = np.abs(x) > 0.05 * np.abs(x).max()
    return np.sqrt(np.mean(x[active] ** 2)) if active.any() else 1.0


def fade_gain(n, rate, start, hold, ramp, floor_db, repeat, gap):
    """Gain per sample: 1, falling to the floor over `ramp`, `hold` at the floor, back up over `ramp`."""
    t = np.arange(n) / float(rate)
    floor = 10 ** (-floor_db / 20.0)
    g = np.ones(n)
    for k in range(repeat):
        t0 = start + k * (2 * ramp + hold + gap)
        t1, t2, t3 = t0 + ramp, t0 + ramp + hold, t0 + 2 * ramp + hold
        m = (t >= t0) & (t < t1)
        g[m] = 1 - (1 - floor) * 0.5 * (1 - np.cos(np.pi * (t[m] - t0) / ramp))
        g[(t >= t1) & (t < t2)] = floor
        m = (t >= t2) & (t < t3)
        g[m] = 1 - (1 - floor) * 0.5 * (1 + np.cos(np.pi * (t[m] - t2) / ramp))
    return g


def cmd_fade(a):
    x, rate = read_wav(a.infile)
    start = a.start if a.start is not None else a.start_frac * len(x) / rate
    g = fade_gain(len(x), rate, start, a.hold, a.ramp, a.floor_db, a.repeat, a.gap)
    noise = np.random.default_rng(a.seed).standard_normal(len(x)) * signal_rms(x) * 10 ** (-a.snr / 20.0)
    write_wav(a.outfile, x * g + noise, rate)


def cmd_join(a):
    xa, ra = read_wav(a.a)
    xb, rb = read_wav(a.b)
    if ra != rb:
        sys.exit('the two files have different sample rates (%d, %d)' % (ra, rb))
    xa = xa[:int(len(xa) * a.fraction)]
    rng = np.random.default_rng(a.seed)
    sigma = signal_rms(xa) * 10 ** (-a.snr / 20.0)
    quiet = rng.standard_normal(int(a.gap * ra)) * sigma
    write_wav(a.outfile, np.concatenate([xa + rng.standard_normal(len(xa)) * sigma, quiet, xb]), ra)


def cmd_rows(a):
    from PIL import Image
    c = np.asarray(Image.open(a.clean).convert('RGB'), dtype=np.float64)
    d = np.asarray(Image.open(a.decoded).convert('RGB'), dtype=np.float64)
    if c.shape != d.shape:
        print('size differs')
        return 1
    h = len(c)

    def best_share(r0, r1):
        """Share of the rows r0..r1 that agree, with the decoded picture moved to fit them best.

        A picture that follows a lost sync is received a few pixels sideways, and a Robot 36 picture that went through
        noise can lose whole line pairs (a fade or not: see the notes in run.sh). Neither is what is tested here (is it
        still one picture, with its top and bottom there), so each end is fitted on its own, within 8 px and 6 rows,
        small moves first. Rows with no counterpart after the move are not judged."""
        if r1 <= r0:
            return 1.0, 0, 0
        best = (-1.0, 0, 0)
        for dx, dy in sorted(((dx, dy) for dx in range(-8, 9) for dy in range(-6, 7)), key=lambda m: abs(m[0]) + abs(m[1])):
            rows = [r for r in range(r0, r1) if 0 <= r - dy < h]
            if not rows:
                continue
            m = np.roll(d, dx, axis=1)
            err = np.mean(np.abs(c[rows] - m[[r - dy for r in rows]]), axis=(1, 2))
            share = float(np.mean(err < 25.0))                     # mean abs error per row, of 255
            if share > best[0] + 1e-9:
                best = (share, dx, dy)
        return best

    out = []
    for name, (r0, r1) in (('top', (0, int(h * a.top))), ('bottom', (h - int(h * a.bottom), h))):
        share, dx, dy = best_share(r0, r1)
        out.append('%s %.2f' % (name, share) + (' (moved %d px, %d rows)' % (dx, dy) if dx or dy else ''))
    print(' '.join(out))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    f = sub.add_parser('fade')
    f.add_argument('infile')
    f.add_argument('outfile')
    g = f.add_mutually_exclusive_group(required=True)
    g.add_argument('--start', type=float, help='seconds into the file where the fall begins')
    g.add_argument('--start-frac', type=float, help='the same, as a fraction of the file length')
    f.add_argument('--hold', type=float, required=True, help='seconds at the bottom of the fade')
    f.add_argument('--ramp', type=float, default=1.5, help='seconds each way (default 1.5)')
    f.add_argument('--snr', type=float, default=25.0, help='noise, dB below the signal rms, in the whole file (default 25)')
    f.add_argument('--floor-db', type=float, default=60.0, help='how far the signal falls (default 60 dB)')
    f.add_argument('--repeat', type=int, default=1)
    f.add_argument('--gap', type=float, default=10.0, help='seconds between repeated fades (default 10)')
    f.add_argument('--seed', type=int, default=1)
    f.set_defaults(fn=cmd_fade)
    j = sub.add_parser('join')
    j.add_argument('outfile')
    j.add_argument('a')
    j.add_argument('fraction', type=float)
    j.add_argument('gap', type=float)
    j.add_argument('b')
    j.add_argument('--snr', type=float, default=25.0)
    j.add_argument('--seed', type=int, default=1)
    j.set_defaults(fn=cmd_join)
    r = sub.add_parser('rows')
    r.add_argument('clean')
    r.add_argument('decoded')
    r.add_argument('top', type=float)
    r.add_argument('bottom', type=float)
    r.set_defaults(fn=cmd_rows)
    a = ap.parse_args()
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()

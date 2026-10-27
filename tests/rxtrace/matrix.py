#!/usr/bin/env python3
"""Tabulate replay.sh runs: noise pictures saved against the time the receiver is held up by noise (issue #135).

  matrix.py VARDIR HOURS NAME [NAME ...]

VARDIR/NAME/NAME_s<sens>_m<min>/trace.tsv are the traces of replay.sh -b NAME=... runs on a recording of HOURS hours.
Per cell: pictures started (false triggers), saved, the time spent on them (DETECT to SAVE), the longest one,
the share of the recording that is, and the freezes that resumed (resyncs). Every picture counts as noise: use it on a
recording without a real signal.
"""
import glob
import os
import re
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import summarize  # noqa: E402

names = ('Low', 'Normal', 'High', 'DX')


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    vardir, hours, variants = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
    print('%-10s %-7s %4s %8s %6s %10s %8s %7s %8s' % ('variant', 'sens', 'min%', 'triggers', 'saved', 'busy s', 'longest', 'busy %', 'resyncs'))
    for v in variants:
        cells = sorted(glob.glob(os.path.join(vardir, v, v + '_s*_m*')))
        for c in cells:
            m = re.search(r'_s(\d)_m(\d+)$', c)
            tr = os.path.join(c, 'trace.tsv')
            if not os.path.exists(tr):
                continue
            pics = summarize.pictures(summarize.parse(tr))
            done = [p for p in pics if p['save'] is not None]
            busy = sum(summarize.num(p['save'], 'age_s') for p in done)
            saved = sum(1 for p in done if p['save'].get('result') == 'saved')
            longest = max([summarize.num(p['save'], 'age_s') for p in done] or [0])
            resyncs = sum(summarize.num(p['save'], 'resyncs') for p in done)
            print('%-10s %-7s %4s %8d %6d %10.0f %8.0f %6.1f%% %8.0f' % (v, names[int(m.group(1))], m.group(2), len(pics), saved, busy, longest, 100 * busy / (hours * 3600), resyncs))


if __name__ == '__main__':
    main()

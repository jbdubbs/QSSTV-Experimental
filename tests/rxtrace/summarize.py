#!/usr/bin/env python3
"""Summarize a receiver trace (src/sstv/rxtrace.h, QSSTV_RX_TRACE) from tests/rxtrace/capture.sh (issue #135).

  summarize.py trace.tsv [--pics DIR] [--min-hits N]

One line per picture the receiver started (DETECT .. SAVE), how every display freeze ended, and the totals: pictures per
mode, saved and discarded, the time the receiver spent on them, and the sync-like pulse rate and level per 10 minutes.

A picture is called "real" when it was started by a VIS code, or when its sync pulses came on at least 0.4 of the lines
with --min-hits (default 20) or more of them. That is a rule of thumb for reading the table, not something the
receiver uses; look at the saved pictures to be sure.

Stdlib only.
"""
import argparse
import collections
import glob
import os
import statistics
import sys


def parse(path):
    events = []
    for line in open(path, errors='replace'):
        p = line.rstrip('\n').split('\t')
        if len(p) < 3:
            continue
        d = {}
        for f in p[2:]:
            if '=' in f:
                k, v = f.split('=', 1)
                d[k] = v
        events.append((p[0], p[1], d))
    return events


def num(d, k, default=0.0):
    try:
        return float(d.get(k, default))
    except ValueError:
        return default


def pictures(events):
    pics, cur = [], None
    for utc, ev, d in events:
        if ev == 'DETECT':
            if cur is not None:
                cur['end'] = 'no SAVE'
                pics.append(cur)
            cur = {'utc': utc, 'detect': d, 'events': [], 'save': None, 'end': None}
        elif cur is not None and ev in ('FREEZE', 'HIT', 'RESYNC', 'LOST', 'RETRACE', 'VIS'):
            cur['events'].append((utc, ev, d))
        elif cur is not None and ev == 'SAVE':
            cur['save'] = d
            cur['end'] = d.get('cause', '?')
            pics.append(cur)
            cur = None
    if cur is not None:
        cur['end'] = 'running at the end of the trace'
        pics.append(cur)
    return pics


def real(p, min_hits):
    s = p['save']
    if s is None:
        return False
    if p['detect'].get('via') == 'vis' and num(p['detect'], 'vis_age_s', 99) < 10:
        return True
    return num(s, 'hits') >= min_hits and num(s, 'hits_per_line') >= 0.4


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('trace')
    ap.add_argument('--pics', help='directory with the pictures the receiver saved (checked against the trace)')
    ap.add_argument('--min-hits', type=int, default=20)
    a = ap.parse_args()

    ev = parse(a.trace)
    if not ev:
        sys.exit('%s: no events' % a.trace)
    pics = pictures(ev)
    print('trace %s: %d events, %s .. %s' % (a.trace, len(ev), ev[0][0], ev[-1][0]))

    print('\n== pictures (DETECT .. SAVE)')
    hdr = ('start (UTC)', 'mode', 'via', 'age s', 'lines%', 'result', 'ends by', 'hits', 'far', 'frz', 'rsy', 'gap s', 'h/line', 'real?')
    rows = []
    for p in pics:
        d, s = p['detect'], p['save']
        if s is None:
            rows.append((p['utc'][5:19], d.get('mode', '?'), d.get('via', '?'), '-', '-', '-', p['end'], '-', '-', '-', '-', '-', '-', '-'))
            continue
        rows.append((p['utc'][5:19], d.get('mode', '?'), d.get('via', '?'), '%.0f' % num(s, 'age_s'), '%.0f' % num(s, 'percent'), s.get('result', '?'),
                     s.get('cause', '?'), '%d' % num(s, 'hits'), '%d' % num(s, 'far_hits'), '%d' % num(s, 'freezes'), '%d' % num(s, 'resyncs'),
                     '%.1f' % num(s, 'max_gap_s'), '%.2f' % num(s, 'hits_per_line'), 'real' if real(p, a.min_hits) else 'noise'))
    w = [max(len(str(r[i])) for r in rows + [hdr]) for i in range(len(hdr))]
    print('  '.join(str(h).ljust(w[i]) for i, h in enumerate(hdr)))
    for r in rows:
        print('  '.join(str(c).ljust(w[i]) for i, c in enumerate(r)))

    done = [p for p in pics if p['save'] is not None]
    saved = [p for p in done if p['save'].get('result') == 'saved']
    print('\n== totals')
    print('pictures started: %d  saved: %d  discarded: %d  unfinished: %d' % (len(pics), len(saved), len(done) - len(saved), len(pics) - len(done)))
    nr = [p for p in done if not real(p, a.min_hits)]
    nr_saved = [p for p in nr if p['save'].get('result') == 'saved']
    print('looked like real signals: %d   like noise: %d, of which saved (false positives in the gallery): %d' % (len(done) - len(nr), len(nr), len(nr_saved)))
    busy = sum(num(p['save'], 'age_s') for p in nr)
    first = ev[0][2].get('t')
    print('receiver time spent on noise pictures: %.0f s (%.0f min)' % (busy, busy / 60))
    by = collections.Counter(p['detect'].get('mode', '?') for p in nr)
    print('noise triggers by mode: ' + (', '.join('%s %d' % kv for kv in by.most_common()) or 'none'))
    byv = collections.Counter(p['detect'].get('via', '?') for p in pics)
    print('started by: ' + ', '.join('%s %d' % kv for kv in byv.most_common()))
    ends = collections.Counter(p['end'] for p in nr)
    print('noise pictures end by: ' + (', '.join('%s %d' % kv for kv in ends.most_common()) or '-'))
    if nr:
        ages = [num(p['save'], 'age_s') for p in nr]
        print('noise picture age s: min %.0f  median %.0f  max %.0f' % (min(ages), statistics.median(ages), max(ages)))
        pc = [num(p['save'], 'percent') for p in nr]
        print('noise picture lines%%: min %.0f  median %.0f  max %.0f' % (min(pc), statistics.median(pc), max(pc)))

    print('\n== display freezes and how they ended')
    out = collections.Counter()
    gaps = collections.defaultdict(list)
    for p in pics:
        evs = p['events']
        for i, (utc, e, d) in enumerate(evs):
            if e != 'FREEZE':
                continue
            kind = 'real' if real(p, a.min_hits) else 'noise'
            far = 0
            res = None
            for utc2, e2, d2 in evs[i + 1:]:
                if e2 == 'HIT':
                    far += 1
                if e2 in ('RESYNC', 'LOST', 'RETRACE', 'FREEZE'):
                    res = (e2, d2)
                    break
            if res is None:
                res = (p['end'] or 'end', {})
            name = res[0]
            out[(kind, name)] += 1
            if name == 'RESYNC':
                gaps[kind].append(num(res[1], 'frozen_s'))
    for (kind, name), n in sorted(out.items()):
        extra = ''
        if name == 'RESYNC' and gaps[kind]:
            extra = '   frozen for min %.1f / median %.1f / max %.1f s' % (min(gaps[kind]), statistics.median(gaps[kind]), max(gaps[kind]))
        print('%-5s freeze -> %-9s %4d%s' % (kind, name, n, extra))
    if not out:
        print('no freezes')
    hits = collections.Counter()
    for p in pics:
        for utc, e, d in p['events']:
            if e == 'HIT':
                hits[('real' if real(p, a.min_hits) else 'noise', 'resync' if d.get('resync') == '1' else 'no resync')] += 1
    if hits:
        print('far pulses (more than 2 lines from the last one): ' + ', '.join('%s/%s %d' % (k[0], k[1], n) for k, n in sorted(hits.items())))

    vis = [(u, d) for u, e, d in ev if e == 'VIS']
    if vis:
        print('\n== VIS codes seen: %d (%d accepted for the active mode set)' % (len(vis), sum(1 for u, d in vis if d.get('accepted') == '1')))
        for u, d in vis[:20]:
            print('  %s %s accepted=%s state=%s' % (u[5:19], d.get('mode'), d.get('accepted'), d.get('state')))

    stats = [(u, d) for u, e, d in ev if e == 'PULSE-STATS']
    if stats:
        print('\n== sync-like pulses per minute and input level, per 10 minutes')
        print('(starts = filtered 1200 Hz energy above the on-threshold, accepted = of those with a plausible width)')
        for proc in ('W', 'N'):
            sel = [(u, d) for u, d in stats if d.get('proc') == proc]
            if not sel:
                continue
            print('processor %s (%s)' % (proc, 'wide: 1200 Hz sync' if proc == 'W' else 'narrow: 1900 Hz sync'))
            for i in range(0, len(sel), 10):
                chunk = sel[i:i + 10]
                acc = [num(d, 'accepted') / num(d, 'seconds', 60) * 60 for u, d in chunk]
                st = [num(d, 'starts') for u, d in chunk]
                lv = [num(d, 'level_mean') for u, d in chunk]
                pk = [num(d, 'level_peak') for u, d in chunk]
                gate = [num(d, 'gate_open_pct') for u, d in chunk]
                states = collections.Counter(d.get('state') for u, d in chunk)
                print('  %s  accepted/min %5.0f (%4.0f..%4.0f)  starts/min %5.0f  level mean %6.0f peak %6.0f  gate open %5.1f%%  %s'
                      % (chunk[0][0][11:16], statistics.mean(acc), min(acc), max(acc), statistics.mean(st), statistics.mean(lv), max(pk), statistics.mean(gate),
                         ', '.join('%s %d' % kv for kv in states.most_common(2))))

    if a.pics:
        pngs = sorted(glob.glob(os.path.join(a.pics, '*')))
        print('\n== pictures on disk: %d (the trace says %d saved)' % (len(pngs), len(saved)))
        if len(pngs) != len(saved):
            print('  MISMATCH: compare %s with the SAVE lines' % a.pics)


if __name__ == '__main__':
    main()

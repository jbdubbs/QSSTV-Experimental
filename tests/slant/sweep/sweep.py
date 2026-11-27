#!/usr/bin/env python3
"""sweep.py --clean DIR --out results.tsv [--modes S2,R36] [--clocks 0,0.001,...] [--snrs none,10,0] [--seeds 1,2] [--workers 4]
        [--autoslant true] [--qsb P D]

For every (mode, clock error, SNR, seed): impairs the clean WAV (impair.py), decodes it with QSSTV-Experimental (run1.sh) and writes one
TSV row: mode clock snr seed pictures valid rows slope shift_px   (shift_px = remaining slant of the test line across the picture).
A case "works" when valid=1, rows >= 50 % of the picture and |shift_px| <= 2.
"""
import argparse, os, subprocess, sys, tempfile, concurrent.futures as cf

HERE = os.path.dirname(os.path.abspath(__file__))

def one(args, mode, clock, snr, seed):
    fd, wav = tempfile.mkstemp(suffix='.wav', dir=args.tmp)
    os.close(fd)
    try:
        cmd = [os.path.join(HERE, 'impair.py'), os.path.join(args.clean, mode + '.wav'), wav, '--clock', str(clock), '--seed', str(seed)]
        if snr != 'none':
            cmd += ['--snr', snr]
        if args.qsb:
            cmd += ['--qsb', args.qsb[0], args.qsb[1]]
        subprocess.run(cmd, check=True)
        env = dict(os.environ)
        r = subprocess.run([os.path.join(HERE, 'run1.sh'), wav, args.autoslant], capture_output=True, text=True, env=env)
        return mode, clock, snr, seed, r.stdout.strip().split('\n')[-1]
    finally:
        os.remove(wav)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--clean', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--tmp', default=None)
    ap.add_argument('--modes', default='S2,M2,R36,PD120')
    ap.add_argument('--clocks', default='0,0.0005,-0.0005,0.001,-0.001,0.002,-0.002,0.005,-0.005,0.01,-0.01,0.02,-0.02')
    ap.add_argument('--snrs', default='none')
    ap.add_argument('--seeds', default='1')
    ap.add_argument('--workers', type=int, default=4)
    ap.add_argument('--autoslant', default='true')
    ap.add_argument('--qsb', nargs=2, default=None)
    args = ap.parse_args()
    jobs = [(m, c, s, sd) for m in args.modes.split(',') for c in args.clocks.split(',') for s in args.snrs.split(',') for sd in args.seeds.split(',')]
    with open(args.out, 'w') as f, cf.ThreadPoolExecutor(args.workers) as ex:
        f.write('mode\tclock\tsnr\tseed\tpictures\tvalid\trows\tslope\tshift_px\n')
        futs = [ex.submit(one, args, *j) for j in jobs]
        for fu in futs:
            m, c, s, sd, res = fu.result()
            f.write('\t'.join([m, c, s, sd, res]) + '\n')
            f.flush()

main()

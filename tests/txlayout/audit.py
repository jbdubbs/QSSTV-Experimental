#!/usr/bin/env python3
"""Where the transmitter puts the segments of a line, against MMSSTV's (Main.cpp LineMRT, LineSCT, LineSC2180, LineP, LineMP, LineMR, LineMC).

audit.py MODE WAV W H  (WAV from qsstv-experimental --encode of a picture whose R, G and B steps from 0 to 255 at W/4, W/2 and 3W/4)
Builds the line MMSSTV 1.13 would send for the same picture and compares the instant of every transition in a line, taken from the
sync pulse's falling edge, with the one in the WAV. Exits 1 when one is more than TOL ms off. The method itself reads up to 0.1 ms
off on MMSSTV's own waveform, hence the tolerance.
"""
import sys, os, struct, numpy as np
from scipy.signal import hilbert
mode, wavf, W, H = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
raw = open(wavf,'rb').read()
ch, fs = struct.unpack("<HI", raw[22:28])
x = np.frombuffer(raw[44:44+(len(raw)-44)//(2*ch)*2*ch], dtype=np.int16).astype(float)[::ch]
# test image (same as generated): R steps at W/4, G at W/2, B at 3W/4
xs = np.arange(W)
R = np.where(xs >= W//4, 255, 0); G = np.where(xs >= W//2, 255, 0); B = np.where(xs >= 3*W//4, 255, 0)
def ry(R,G,B):
    Y = 16 + (0.256773*R + 0.504097*G + 0.097900*B)
    RY = 128 + (0.439187*R - 0.367766*G - 0.071421*B)
    BY = 128 + (-0.148213*R - 0.290974*G + 0.439187*B)
    return [np.clip(v,16,240).astype(int) if False else np.clip(v,0,255) for v in (Y,RY,BY)]
def fw(v): return 1500 + np.asarray(v,float)*800/256
def fn(v): return 2044 + np.asarray(v,float)*256/256
def seg(lab,f,ms): return (lab,float(f),ms)
def px(lab, vals, tw, nar=False):
    f = fn(vals) if nar else fw(vals)
    return [(lab+"@%d"%i, float(f[i]), tw/len(vals)) for i in range(len(vals))]
def line(mode):
    s=[]
    Y,RY,BY = ry(R,G,B)
    n = mode.endswith('-N')
    if mode in ('M1','M2'):
        tw = {'M1':146.432,'M2':73.216}[mode]
        s += [seg('sync',1200,4.862), seg('porch',1500,.572)] + px('G',G,tw) + [seg('sepGB',1500,.572)] + px('B',B,tw) + [seg('sepBR',1500,.572)] + px('R',R,tw) + [seg('tail',1500,.572)]
    elif mode in ('S1','S2','SDX'):
        tw = {'S1':138.24,'S2':88.064,'SDX':345.6}[mode]
        s += [seg('sepG',1500,1.5)] + px('G',G,tw) + [seg('sepB',1500,1.5)] + px('B',B,tw) + [seg('sync',1200,9), seg('porch',1500,1.5)] + px('R',R,tw)
    elif mode.startswith('SC2'):
        S,tw = {'SC2-180':(5.5437,235.0),'SC2-120':(5.52248,156.5),'SC2-60':(5.5006,78.128)}[mode]
        s += [seg('sync',1200,S), seg('porch',1500,.5)] + px('R',R,tw)+px('G',G,tw)+px('B',B,tw)
    elif mode in ('P3','P5','P7'):
        S,P,C = {'P3':(5.208,1.042,133.333),'P5':(7.813,1.562375,200.0),'P7':(10.417,2.083,266.667)}[mode]
        s += [seg('sync',1200,S), seg('porch',1500,P)] + px('R',R,C)+[seg('sepRG',1500,P)]+px('G',G,C)+[seg('sepGB',1500,P)]+px('B',B,C)+[seg('tail',1500,P)]
    elif mode.startswith('MP') and not n:
        tw = float(mode[2:])  # placeholder replaced below
    return s
TW = {'MP73':140,'MP115':223,'MP140':270,'MP175':340,'MR73':138,'MR90':171,'MR115':220,'MR140':269,'MR175':337,
      'ML180':176.5,'ML240':236.5,'ML280':277.5,'ML320':317.5,'MP73-N':140,'MP110-N':212,'MP140-N':270,
      'MC110-N':140,'MC140-N':180,'MC180-N':232}
def line_yc(mode, vals_next=None):
    Y,RY,BY = ry(R,G,B); n = mode.endswith('-N'); tw = TW[mode]
    sy, lo = (1900,2044) if n else (1200,1500)
    s=[]
    if mode.startswith('MP'):
        s += [seg('sync',sy,9.0), seg('porch',lo,1.0)] + px('Y1',Y,tw,n)+px('RY',RY,tw,n)+px('BY',BY,tw,n)+px('Y2',Y,tw,n)
    elif mode.startswith('MR') or mode.startswith('ML'):
        ty=tw/len(Y); tc=ty/2
        s += [seg('sync',1200,9), seg('porch',1500,1)] + px('Y',Y,tw)
        s += [seg('lp1',float(fw(Y[-1])),.1)] + px('RY',RY,tw/2) + [seg('lp2',float(fw(RY[-1])),.1)] + px('BY',BY,tw/2) + [seg('lp3',float(fw(BY[-1])),.1)]
    elif mode.startswith('MC'):
        s += [seg('sync',1900,8.0), seg('porch',2044,0.5)] + px('R',R,tw,True)+px('G',G,tw,True)+px('B',B,tw,True)
    return s
def line_other(mode):
    """Robot 24/36/72, B/W 8/12 and PD (MMSSTV Main.cpp LineR24, LineR36, LineR72, LineRM, LineP). Robot 36 has a line type per parity (returns both)."""
    Y,RY,BY = ry(R,G,B)
    if mode=='R24':
        return [[seg('sync',1200,6), seg('porch',1500,2)] + px('Y',Y,92) + [seg('sepY',1500,3), seg('porchc',1900,1)] + px('RY',RY,46) + [seg('sepRB',2300,3), seg('porchb',1900,1)] + px('BY',BY,46)]
    if mode=='R72':
        return [[seg('sync',1200,9), seg('porch',1500,3)] + px('Y',Y,138) + [seg('sepY',1500,4.5), seg('porchc',1900,1.5)] + px('RY',RY,69) + [seg('sepRB',2300,4.5), seg('porchb',1900,1.5)] + px('BY',BY,69)]
    if mode=='R36':
        return [[seg('sync',1200,9), seg('porch',1500,3)] + px('Y',Y,88) + [seg('sepY',1500 if par==0 else 2300,4.5), seg('porchc',1900,1.5)] + px('C',RY if par==0 else BY,44) for par in (0,1)]
    if mode in ('BW8','BW12'):
        tw = {'BW8':58.89709,'BW12':92.0}[mode]
        return [[seg('sync',1200,6), seg('porch',1500,2)] + px('Y',Y,tw)]
    if mode.startswith('PD'):
        tw = {'PD50':91.52,'PD90':170.24,'PD120':121.6,'PD160':195.584,'PD180':183.04,'PD240':244.48,'PD290':228.8}[mode]
        return [[seg('sync',1200,20), seg('porch',1500,2.08)] + px('Y1',Y,tw) + px('RY',RY,tw) + px('BY',BY,tw) + px('Y2',Y,tw)]
    return None
OTHER = line_other(mode)
if OTHER:
    PAR = int(os.environ.get('AUDIT_PARITY','0'))
    LINES = OTHER if PAR==0 else OTHER[::-1]
    nl = LINES[0]
else:
    LINES = None
    nl = line_yc(mode) if mode in TW else line(mode)
if not nl: sys.exit("no spec for "+mode)
total = sum(d for _,_,d in nl)
fs_s = fs
# spec trace for 4 identical lines
def build(segs, nrep):
    t=0; edges=[]; out=[]
    for r in range(nrep):
        for lab,f,d in (LINES[r % len(LINES)] if LINES else segs):
            out.append((t,t+d,f,lab+("" if r==0 else "#%d"%r))); t+=d
    return out
spec = build(nl, 4)
# measured crop
nar = mode.endswith('-N') or mode.startswith('MC')
mid = len(x)//2
crop_len = int((total*6+100)/1000*fs)
x0 = mid - crop_len//2
xc = x[x0:x0+crop_len]
ph = np.unwrap(np.angle(hilbert(xc)))
f = np.convolve(np.diff(ph)*fs/(2*np.pi), np.ones(5)/5, 'same'); f = np.append(f, f[-1])
t = np.arange(len(f))/fs*1000
# find sync leading edge in measured
thr = 1990 if nar else 1350
m = f < thr
# sustained >=6ms runs
e = np.flatnonzero(np.diff(m.astype(int)))
cand=[]
for a,b in zip(e[:-1], e[1:]):
    if m[a+1] and (b-a)/fs*1000 > 0.6*[d for l,_,d in nl if l=="sync"][0] and a > 0.7*total/1000*fs and t[a] < t[-1]-total*1.9:
        cand.append(a)
if not cand: sys.exit("no sync found")
ts_meas = None
def cross(tt0, lo, hi, lvl, win):
    # first crossing of lvl between lo and hi in [tt0-win, tt0+win] ms
    i0=int((tt0-win)/1000*fs); i1=int((tt0+win)/1000*fs)
    seg=f[max(i0,0):i1]; up = hi>lo
    s = seg-lvl
    idx = np.flatnonzero((s[:-1]<0)&(s[1:]>=0)) if up else np.flatnonzero((s[:-1]>=0)&(s[1:]<0))
    if len(idx)==0: return None
    # choose the one nearest the window centre
    cs=[(max(i0,0)+i + (-s[i])/(s[i+1]-s[i]))/fs*1000 for i in idx]
    return min(cs, key=lambda c:abs(c-tt0))
# sync edge: previous level = first spec seg before sync (use spec to define), simply use 'sync' seg of spec
# find the spec sync of rep 1
srep = [sp for sp in spec if sp[3].startswith('sync')]
sp0 = srep[1]
# preceding level
idx0 = spec.index(sp0); prev = spec[idx0-1]
lvl = 1972 if nar else 1350
tm = cross(t[cand[0]], prev[2], sp0[2], lvl, 1.5)
ref = tm - sp0[0]   # measured time of spec t=0
rows=[]
for i in range(1,len(spec)):
    p, c = spec[i-1], spec[i]
    if abs(c[2]-p[2]) < 120: continue
    te = c[0]+ref
    if te < t[0]+total*0.1 or te > t[-1]-total*0.1: continue
    if not (srep[1][0]-total*0.9 <= c[0] <= srep[1][0]+total*0.95): continue
    lvl = (p[2]+c[2])/2
    tmx = cross(te, p[2], c[2], lvl, 1.2)
    rows.append((c[3], c[0]-sp0[0], None if tmx is None else tmx-te, c[2]))
# level check: mean freq of mid of long segments
lev=[]
for i,(a,b,fr,lab) in enumerate(spec):
    if b-a >= 1.5 and srep[1][0]-total*0.9 <= a <= srep[1][0]+total*0.95 and not lab.startswith(('sync')):
        ia=int((a+ref+ (b-a)*0.3)/1000*fs); ib=int((a+ref+(b-a)*0.7)/1000*fs)
        if 0<=ia<ib<len(f): lev.append((lab, a-sp0[0], f[ia:ib].mean()-fr))
dl = [abs(r[2]) for r in rows if r[2] is not None]
miss=[r for r in rows if r[2] is None]
print("%-8s line %.3f ms  edges %d  max|d|=%.3f mean=%.3f  missing=%d" % (mode, total, len(rows), max(dl) if dl else -1, np.mean(dl) if dl else -1, len(miss)))
TOL=0.16
sys.exit(1 if (not dl or max(dl)>TOL or miss) else 0)

#!/usr/bin/env python3
"""roundtrip.py MODE W H PIXMS KIND RXW RXH ROW0 qsstv: our transmission of MODE, received in Auto as sent (the VIS code starts the picture) and with everything
before the end of the VIS code cut off (the pulses find it): the mode the receiver names, the size of the picture, and where the steps of the test picture
(three steps at a quarter, a half and three quarters of the line: red, green and blue for KIND rgb, luma 96, 192 and 255 for KIND gray) are against where they
were sent, in ms. KIND may end in ':nopulse' (AVT has no pulses to find it by).
prints:  MODE detected_mode RXWxRXH vis_offset_ms pulse_offset_ms"""
import sys, os, subprocess, struct, tempfile, glob, re, shutil, wave, numpy as np
from PIL import Image
mode, W, H, pms, kind, RXW, RXH, ROW0, q = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), float(sys.argv[4]), sys.argv[5], int(sys.argv[6]), int(sys.argv[7]), int(sys.argv[8]), sys.argv[9]
nopulse = kind.endswith(':nopulse'); kind = kind.split(':')[0]
T = tempfile.mkdtemp(dir=os.environ.get('SCR'))
def env(home, **kw): return dict(os.environ, QT_QPA_PLATFORM='offscreen', HOME=home, XDG_CONFIG_HOME=home + '/.config', XDG_DATA_HOME=home + '/.local/share', **kw)
im = Image.new('RGB', (W, H)); p = im.load()
for x in range(W):
    if kind == 'gray':
        v = 0 if x < W // 4 else (96 if x < W // 2 else (192 if x < 3 * W // 4 else 255)); c = (v, v, v)
    else:
        c = (255 if x >= W // 4 else 0, 255 if x >= W // 2 else 0, 255 if x >= 3 * W // 4 else 0)
    for y in range(H): p[x, y] = c
im.save(T + '/s.png')
h0 = T + '/h0'; os.makedirs(h0)
subprocess.run([q, '--encode', T + '/s.png', '--mode', mode, '--wav-out', T + '/w.wav'], env=env(h0), capture_output=True, timeout=900)
def decode(wav, tag):
    h = T + '/h_' + tag; os.makedirs(h); o = T + '/o_' + tag; os.makedirs(o); tr = T + '/tr_' + tag
    subprocess.run([q, '--batch', '-o', o, wav], env=env(h, QSSTV_RX_TRACE=tr), capture_output=True, timeout=900)
    f = glob.glob(o + '/*.png'); return (f[0] if f else None), (open(tr).read() if os.path.exists(tr) else '')
def edge(v, exp, mid):
    for j in range(max(exp - 40, 0), min(exp + 40, len(v) - 1)):
        if v[j] < mid <= v[j + 1]: return j + (mid - v[j]) / (v[j + 1] - v[j]) + 0.5 - exp
    return float('nan')
def measure(png):
    if not png: return None, None, None
    name = re.search(r'_1_(.+)\.png$', png); det = name.group(1) if name else '?'
    img = Image.open(png); w, h = img.size
    if kind == 'gray':
        a = np.asarray(img.convert('L'), float)
        r = ROW0 + (h - ROW0) // 2
        v = a[r - 3:r + 3].mean(axis=0)
        e = [edge(v, w // 4, 48), edge(v, w // 2, 144), edge(v, 3 * w // 4, 223.5)]
    else:
        a = np.asarray(img.convert('RGB'), float)
        r = h // 2
        e = [edge(a[r - 3:r + 3, :, c].mean(axis=0), (w // 4, w // 2, 3 * w // 4)[c], (a[r - 3:r + 3, :, c].mean(axis=0).min() + a[r - 3:r + 3, :, c].mean(axis=0).max()) / 2) for c in range(3)]
    return det, "%dx%d" % (w, h), float(np.nanmean(e)) * pms
png, tr = decode(T + '/w.wav', 'n')
det, size, o1 = measure(png)
o2 = None; det2 = None
vis = [l for l in tr.splitlines() if '\tVIS\t' in l and 'accepted=1' in l]
if png and vis and not nopulse:
    end = float(re.search(r'end=([\d.]+)', vis[0]).group(1))
    raw = open(T + '/w.wav', 'rb').read(); ch, fs = struct.unpack('<HI', raw[22:28])
    d = np.frombuffer(raw[44:44 + (len(raw) - 44) // (2 * ch) * 2 * ch], dtype=np.int16).reshape(-1, ch)
    wv = wave.open(T + '/c.wav', 'wb'); wv.setnchannels(ch); wv.setsampwidth(2); wv.setframerate(fs); wv.writeframes(d[int((end + 0.02) * fs):].tobytes()); wv.close()
    png2, tr2 = decode(T + '/c.wav', 'c'); det2, size2, o2 = measure(png2)
    if det2 != det: o2 = None
f = lambda v: "nan" if v is None or np.isnan(v) else "%.3f" % v
print("%s %s %s %s %s" % (mode, det or "none", size or "none", f(o1), f(o2) if not nopulse else "-"))
shutil.rmtree(T)

#!/usr/bin/env python3
"""AVT 90 the way MMSSTV transmits it (Main.cpp LineAVT and the end of SendHeader; the SSTV Handbook, 4.2.5, describes the same header):
leader, VIS 0x44 (MMSSTV sends it 3 times), 32 words of a 1900 Hz pulse and 16 bits of 9.7646 ms (1600 Hz zero, 2200 Hz one; the byte of mode and
count, then its inverse), 0.305 ms, then 240 lines of red, green and blue of 125 ms each, no gaps and no sync pulse.

  avtgen.py out.wav [DROP [REPEATS [LEAD [visonly]]]]   DROP: the first words of the header are lost (a receiver that joins late); REPEATS: how often the
                                       leader and the VIS code are sent (MMSSTV: 3)
The picture is a red, a green and a blue step at a quarter, a half and three quarters of the line."""
import sys, numpy as np, wave
fs=48000; out=[]; ph=0.0; carry=0.0
def tone(hz,ms):
    global ph,carry
    n=ms*fs/1000.0+carry; k=int(round(n)); carry=n-k
    if k<=0: return
    p=ph+2*np.pi*hz/fs*np.arange(1,k+1); out.append(0.5*np.sin(p)); ph=p[-1]%(2*np.pi)
drop=int(sys.argv[2]) if len(sys.argv)>2 else 0
repeats=int(sys.argv[3]) if len(sys.argv)>3 else 1
lead=float(sys.argv[4]) if len(sys.argv)>4 else 0     # seconds of noise before the transmission
visonly=len(sys.argv)>5 and sys.argv[5]=='visonly'   # a VIS code that no header follows
W,H=256,240
if lead>0:
    rng=np.random.default_rng(1); out.append(0.05*rng.standard_normal(int(lead*fs)))
for r in range(repeats):
    tone(1900,300);tone(1200,10);tone(1900,300);tone(1200,30)
    d=0x44
    for i in range(8): tone(1100 if d&1 else 1300,30); d>>=1
    tone(1200,30)
sd=0x5fa0
for i in range(0 if visonly else 32):
    if i<drop:
        tone(1000,17*9.7646)   # a word that was lost
        sd=((sd&0xff00)-0x0100)|((sd&0x00ff)+0x0001); continue
    tone(1900,9.7646); d=sd
    for n in range(16): tone(1600 if d&0x8000 else 2200,9.7646); d=(d<<1)&0xffff
    sd=((sd&0xff00)-0x0100)|((sd&0x00ff)+0x0001)
if not visonly: tone(0.0001,0.30514375)
def fw(v): return 1500+v*800/256
xs=np.arange(W)
R=np.where(xs>=W//4,255,0);G=np.where(xs>=W//2,255,0);B=np.where(xs>=3*W//4,255,0)
for y in range(0 if visonly else H):
    for ch in (R,G,B):
        for x in range(W): tone(fw(ch[x]),125.0/W)
x=np.concatenate(out); 
w=wave.open(sys.argv[1],'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(fs); w.writeframes((x*32000).astype(np.int16).tobytes()); w.close()

#!/usr/bin/env python3
"""pulseonly.py MODE W H PIXMS qsstv: our transmission of MODE (a red, a green and a blue step at a quarter, a half and three quarters of the line),
decoded as sent (the VIS code starts the picture) and with everything before the end of the VIS code cut (the pulses alone find it).
prints:  MODE vis_offset_ms pulse_offset_ms   (the steps from where they were sent; nan: no picture)"""
import sys, os, subprocess, struct, tempfile, glob, re, numpy as np, shutil
from PIL import Image
mode,W,H,pms,q=sys.argv[1],int(sys.argv[2]),int(sys.argv[3]),float(sys.argv[4]),sys.argv[5]
T=tempfile.mkdtemp(dir=os.environ.get('SCR'))
env=dict(os.environ,QT_QPA_PLATFORM='offscreen')
def run(args,home):
    e=dict(env,HOME=home,XDG_CONFIG_HOME=home+'/.config',XDG_DATA_HOME=home+'/.local/share')
    return subprocess.run(args,env=e,capture_output=True,text=True,timeout=900)
im=Image.new('RGB',(W,H)); p=im.load()
for x in range(W):
    c=(255 if x>=W//4 else 0,255 if x>=W//2 else 0,255 if x>=3*W//4 else 0)
    for y in range(H): p[x,y]=c
im.save(T+'/s.png')
h0=T+'/h0'; os.makedirs(h0)
run([q,'--encode',T+'/s.png','--mode',mode,'--wav-out',T+'/w.wav'],h0)
def decode(wav,tag):
    h=T+'/h_'+tag; os.makedirs(h); o=T+'/o_'+tag; os.makedirs(o)
    tr=T+'/tr_'+tag
    e=dict(env,QSSTV_RX_TRACE=tr)
    ee=dict(e,HOME=h,XDG_CONFIG_HOME=h+'/.config',XDG_DATA_HOME=h+'/.local/share')
    subprocess.run([q,'--batch','-o',o,wav],env=ee,capture_output=True,timeout=900)
    f=glob.glob(o+'/*.png'); t=open(tr).read() if os.path.exists(tr) else ''
    return (f[0] if f else None),t
def offset(png):
    if not png: return None
    a=np.asarray(Image.open(png).convert('RGB'),float); h,w=a.shape[:2]
    if (w,h)!=(W,H): a=np.asarray(Image.open(png).convert('RGB').resize((W,H)),float)
    def edge(v,exp):
        mid=(v.min()+v.max())/2
        for j in range(max(exp-40,0),min(exp+40,len(v)-1)):
            if v[j]<mid<=v[j+1]: return j+(mid-v[j])/(v[j+1]-v[j])+0.5-exp
        return float('nan')
    r=H//2
    e=[edge(a[r-3:r+3,:,c].mean(axis=0),(W//4,W//2,3*W//4)[c]) for c in range(3)]
    return np.nanmean(e)*pms
png,tr=decode(T+'/w.wav','n')
vis=[l for l in tr.splitlines() if '\tVIS\t' in l and 'accepted=1' in l]
via=re.search(r'via=(\S+)',[l for l in tr.splitlines() if '\tDETECT\t' in l][0]).group(1) if '\tDETECT\t' in tr else '?'
o1=offset(png)
if not vis: print("%s nan nan"%mode); shutil.rmtree(T); sys.exit()
end=float(re.search(r'end=([\d.]+)',vis[0]).group(1))
raw=open(T+'/w.wav','rb').read(); ch,fs=struct.unpack('<HI',raw[22:28])
data=np.frombuffer(raw[44:44+(len(raw)-44)//(2*ch)*2*ch],dtype=np.int16).reshape(-1,ch)
cut=data[int((end+0.02)*fs):]
import wave
w=wave.open(T+'/c.wav','wb'); w.setnchannels(ch); w.setsampwidth(2); w.setframerate(fs); w.writeframes(cut.tobytes()); w.close()
png2,tr2=decode(T+'/c.wav','c')
via2=re.search(r'via=(\S+)',[l for l in tr2.splitlines() if '\tDETECT\t' in l][0]).group(1) if '\tDETECT\t' in tr2 else '?'
o2=offset(png2)
print("%s %s %s"%(mode,"nan" if o1 is None else "%.3f"%o1,"nan" if o2 is None else "%.3f"%o2))
shutil.rmtree(T)

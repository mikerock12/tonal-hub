#!/usr/bin/env python3
"""Turn the supplied Santa clip into the seamless corner animation of the Christmas cover.

The source is an opaque 944x944 video with a checkerboard painted into its pixels, a
drifting camera and no loop point. This script, run once by hand:
  1. removes the checkerboard (alpha + soft ground shadows) and stabilises every frame;
  2. keeps a sharp, motionless plate for tree, sleigh, reindeer and toys, so only Santa
     moves and the background needs no loop treatment at all;
  3. adds tree lights whose blink counts are whole numbers per loop;
  4. closes the loop at matched poses, blending along optical flow around that point;
  5. writes resources/seasonal/santa.webp and the Android copy.

Tools (not product dependencies): numpy, Pillow, opencv-python-headless, ffmpeg on PATH.
Usage: python packaging/generate-christmas-santa.py clip.mp4 work-directory
"""
import math
import random
import shutil
import subprocess
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
N = 944
LOOP_START, LOOP_END, SEAM = 83, 164, 6   # measured: matching poses during a slow part of the motion
OUT_SIDE = 384
QUALITY, ALPHA_QUALITY = 74, 85


def edges(sig):
    d=np.abs(np.diff(sig)); idx=np.where(d>12)[0]
    grp=np.split(idx,np.where(np.diff(idx)>3)[0]+1)
    return np.array([(a*d[a]).sum()/d[a].sum()+.5 for a in grp if len(a)])
def build_checker():
    g=MEDIAN.astype(np.float32).mean(2)
    ex=edges(g[3:8,:].mean(0)); ey=edges(g[:,3:8].mean(1))
    ix=np.searchsorted(ex,np.arange(N)); iy=np.searchsorted(ey,np.arange(N))
    par=(ix[None,:]+iy[:,None])%2
    v0=g[:40,:40][par[:40,:40]==0].mean(); v1=g[:40,:40][par[:40,:40]==1].mean()
    B=np.where(par==0,v0,v1).astype(np.float32)
    bx=np.zeros(N,bool); by=np.zeros(N,bool)
    for e in ex: bx[max(0,int(e)-2):int(e)+3]=True
    for e in ey: by[max(0,int(e)-2):int(e)+3]=True
    band=bx[None,:]|by[:,None]
    return B,band,float(min(v0,v1)),float(max(v0,v1))
def matte(rgb):
    """returns float rgba (straight) in source coordinates"""
    f=rgb.astype(np.float32); mx=f.max(2); mn=f.min(2); chroma=mx-mn; lum=f.mean(2)
    tol=np.where(B>230,6.5,10.)
    cand=((np.abs(lum-B)<tol)&(chroma<9))|(band&(chroma<9)&(lum>LO-12)&(lum<HI+8))
    n,lab,st,_=cv2.connectedComponentsWithStats(cand.astype(np.uint8),connectivity=4)
    # A real gap spans several squares; isolated white squares inside fur do not.
    grayhits=np.bincount(lab[(B<230)&(~band)&cand],minlength=n)
    # A real gap spans both square shades; foam or fur only ever matches the white ones.
    good=[i for i in range(1,n) if st[i,4]>=230 and grayhits[i]>=.05*st[i,4]]
    bg=np.isin(lab,good)
    k=lambda r:cv2.getStructuringElement(cv2.MORPH_ELLIPSE,(2*r+1,2*r+1))
    ratio=lum/np.maximum(B,1)
    strong=(~bg)&((chroma>=16)|(ratio<.42)|((B<230)&(lum>B+22)))
    s=cv2.morphologyEx(strong.astype(np.uint8),cv2.MORPH_CLOSE,k(6))
    s[bg&(cv2.erode(bg.astype(np.uint8),k(1))>0)]=0
    inv=(1-s).astype(np.uint8); n,lab,st,_=cv2.connectedComponentsWithStats(inv,connectivity=4)
    hits=np.bincount(lab[bg],minlength=n)
    s=(~np.isin(lab,[i for i in range(1,n) if hits[i]>=.3*st[i,4]])).astype(np.uint8)
    # Peel neutral checker flakes left on the contour, geodesically from outside.
    peel=((chroma<6)&(lum>LO-6)).astype(np.uint8)
    out=(1-s)
    for _ in range(14):
        grown=cv2.dilate(out,k(1))&(peel|out)
        if (grown==out).all(): break
        out=grown
    s=(1-out).astype(np.uint8)
    s=cv2.morphologyEx(s,cv2.MORPH_OPEN,k(1))
    n,lab,st,_=cv2.connectedComponentsWithStats(s,connectivity=8)
    s=np.isin(lab,1+np.where(st[1:,4]>=60)[0]).astype(np.float32)
    sm=cv2.GaussianBlur(s,(0,0),1.5)
    a=np.clip((sm-.55)/.3,0,1)
    # Soft shadow from checker-normalised darkening, ignoring square seams.
    w=((~band)&(s==0)&(chroma<22)).astype(np.float32)
    num=cv2.GaussianBlur((1-ratio)*w,(0,0),5); den=cv2.GaussianBlur(w,(0,0),5)
    sh=np.where(den>.05,num/np.maximum(den,1e-3),0)
    sh=np.clip((sh-.022)*1.25,0,.72)
    alpha=a+(1-a)*sh
    outc=np.where(alpha[...,None]>1e-3,(f*a[...,None])/np.maximum(alpha[...,None],1e-3),0)
    return np.dstack([outc,alpha*255.]),a


def stabilise(frames):
    """Affine drift of every frame relative to the first loop frame, from the static objects."""
    rgb = frames[LOOP_START]
    grey = cv2.cvtColor(rgb, cv2.COLOR_RGB2GRAY)
    mask = (((rgb.max(2).astype(int) - rgb.min(2)) > 40) * 255).astype(np.uint8)
    mask[200:944, 200:560] = 0  # Santa's sweep
    mask[120:520, 180:640] = 0
    mask = cv2.dilate(mask, np.ones((9, 9), np.uint8))
    small = lambda a: cv2.resize(a, None, fx=.5, fy=.5, interpolation=cv2.INTER_AREA)
    target, weights = small(grey).astype(np.float32), small(mask)
    warps = np.zeros((len(frames), 2, 3), np.float32)
    for i in range(40, 200):
        w = np.eye(2, 3, dtype=np.float32) if i == 40 else warps[i - 1].copy()
        if i > 40:
            w[:, 2] *= .5
        _, w = cv2.findTransformECC(target, small(cv2.cvtColor(frames[i], cv2.COLOR_RGB2GRAY)).astype(np.float32), w,
                                    cv2.MOTION_AFFINE, (cv2.TERM_CRITERIA_COUNT | cv2.TERM_CRITERIA_EPS, 60, 1e-5), weights, 3)
        w = w.copy()
        w[:, 2] /= .5
        warps[i] = w
    warps[200:] = warps[199]  # the camera has settled by then
    return warps



clip, work = Path(sys.argv[1]), Path(sys.argv[2])
raw = work / "frames"
raw.mkdir(parents=True, exist_ok=True)
subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(clip), "-map", "0:v:0", str(raw / "%04d.png")], check=True)
F = [np.asarray(Image.open(f).convert("RGB")) for f in sorted(raw.glob("*.png"))]
MEDIAN = np.median(np.stack(F[::3]), axis=0).astype(np.uint8)
B, band, LO, HI = build_checker()
Wp = stabilise(F)
A, Bn, K = LOOP_START, LOOP_END, SEAM
L = Bn - A
k=lambda r:cv2.getStructuringElement(cv2.MORPH_ELLIPSE,(2*r+1,2*r+1))
def rgba_pm(i):
    """stabilised, premultiplied float RGBA, edges decontaminated"""
    r,a=matte(F[i])
    core=(cv2.erode((a>.98).astype(np.uint8),k(2))>0).astype(np.float32)
    w=cv2.GaussianBlur(core,(0,0),2.5)
    fill=cv2.GaussianBlur(r[...,:3]*core[...,None],(0,0),2.5)/np.maximum(w[...,None],1e-3)
    edge=((a>0)&(core==0))[...,None]&(w[...,None]>.02)
    col=np.where(edge,fill,r[...,:3]); al=r[...,3:4]/255
    # solid share keeps its colour, shadow share stays black
    pm=np.dstack([col*(a[...,None]),al*255])
    return cv2.warpAffine(pm,Wp[i],(N,N),flags=cv2.INTER_LINEAR,borderMode=cv2.BORDER_CONSTANT)
idx=list(range(A-K,Bn+K+1))
C={i:rgba_pm(i).astype(np.float16) for i in idx}
extra=[i for i in range(44,196,6)]
stack=np.stack([(C[i] if i in C else rgba_pm(i).astype(np.float16)) for i in extra]).astype(np.float32)
med=np.median(stack,axis=0); del stack
ref=rgba_pm(83).astype(np.float32)
# plate: sharp reference frame, Santa area replaced by temporal median
d=np.abs(ref-med).max(2)
m=(cv2.GaussianBlur(d,(0,0),3)>26).astype(np.uint8)
m=cv2.morphologyEx(m,cv2.MORPH_OPEN,k(3));n,lab,st,_=cv2.connectedComponentsWithStats(m)
big=1+int(np.argmax(st[1:,4]));m=(lab==big).astype(np.uint8);m=cv2.dilate(m,k(9))
mf=cv2.GaussianBlur(m.astype(np.float32),(0,0),4)[...,None]
plate=ref*(1-mf)+med*mf
# per-frame Santa isolation
def isolate(c):
    c=c.astype(np.float32)
    dd=np.abs(cv2.GaussianBlur(c,(0,0),1.6)-cv2.GaussianBlur(plate,(0,0),1.6)).max(2)
    mm=(dd>34).astype(np.uint8)
    mm=cv2.morphologyEx(mm,cv2.MORPH_OPEN,k(3))
    mm=cv2.morphologyEx(mm,cv2.MORPH_CLOSE,k(9))
    n,lab,st,_=cv2.connectedComponentsWithStats(mm)
    big=1+int(np.argmax(st[1:,4]))
    keep=[i for i in range(1,n) if i==big or (st[i,4]>900 and abs(st[i,0]+st[i,2]/2-st[big,0]-st[big,2]/2)<260)]
    mm=np.isin(lab,keep).astype(np.uint8)
    inv=(1-mm).astype(np.uint8);n,lab,st,_=cv2.connectedComponentsWithStats(inv,connectivity=4)
    mm=(~np.isin(lab,[i for i in range(1,n) if st[i,4]>2500])).astype(np.uint8)
    mm=cv2.dilate(mm,k(2))
    sm=cv2.GaussianBlur(mm.astype(np.float32),(0,0),2.2)[...,None]
    return sm
# twinkle lights from plate
lum=plate[...,:3].mean(2)
top=lum-cv2.morphologyEx(lum,cv2.MORPH_OPEN,k(5))
tree=np.zeros((N,N),np.uint8);cv2.fillPoly(tree,[np.array([[470,0],[790,620],[180,680],[300,300]],np.int32)],1)
spot=(top>70)&(tree>0)&(plate[...,3]>230)
n,lab,st,cen=cv2.connectedComponentsWithStats(spot.astype(np.uint8))
rnd=random.Random(1225)
cand=[(float(cen[i][0]),float(cen[i][1]),int(st[i,4])) for i in range(1,n) if st[i,4]>=5]
rnd.shuffle(cand);lights=[]
for x,y,sz in cand:
    if all((x-a)**2+(y-b)**2>17**2 for a,b,_ in lights): lights.append((x,y,sz))
    if len(lights)>=120: break
pal=[(255,214,130)]*5+[(255,90,70),(120,255,150),(120,190,255),(255,240,220)]
Ls=[]
for x,y,sz in lights:
    Ls.append(dict(x=x,y=y,c=np.array(rnd.choice(pal),np.float32),cyc=rnd.choice([3,4,5,6,7,8]),ph=rnd.random(),r=rnd.uniform(5.5,9)))
gy,gx=np.mgrid[-34:35,-34:35].astype(np.float32);rr=gx*gx+gy*gy
def stamp(P,x,y,arr,mode):
    x0,y0=int(round(x)),int(round(y));h=34
    xa,ya=max(0,x0-h),max(0,y0-h);xb,yb=min(N,x0+h+1),min(N,y0+h+1)
    s=arr[ya-y0+h:yb-y0+h,xa-x0+h:xb-x0+h]
    if mode=='add': P[ya:yb,xa:xb,:3]+=s
    else: P[ya:yb,xa:xb,:3]*=s
sx,sy=0,0
ys,xs=np.where((plate[:70,:,3]>200)&(plate[:70,:,0]>200)&(plate[:70,:,2]<170))
sx,sy=float(xs.mean()),float(ys.mean())
def lit(t):
    P=plate.copy()
    al=P[...,3:4]/255
    for l in Ls:
        v=.5+.5*math.cos(2*math.pi*(l['cyc']*t+l['ph']));v=v**2.4
        r=l['r']
        dim=1-(1-v)*.62*np.exp(-rr/(2*(r*.62)**2))
        stamp(P,l['x'],l['y'],dim[...,None],'mul')
        core=np.exp(-rr/(2*(r*.42)**2));halo=np.exp(-rr/(2*(r*1.5)**2))
        flare=np.exp(-(gx*gx)/(2*1.1**2))*np.exp(-(np.abs(gy))/(r*.9))+np.exp(-(gy*gy)/(2*1.1**2))*np.exp(-(np.abs(gx))/(r*.9))
        stamp(P,l['x'],l['y'],(v*(core*170+halo*46+flare*90))[...,None]*(l['c']/255),'add')
    # breathing star on top
    v=.55+.45*math.cos(2*math.pi*2*t)
    halo=np.exp(-rr/(2*16.**2));fl=np.exp(-(gx*gx)/(2*1.6**2))*np.exp(-np.abs(gy)/15)+np.exp(-(gy*gy)/(2*1.6**2))*np.exp(-np.abs(gx)/15)
    stamp(P,sx,sy,(v*(halo*90+fl*150))[...,None]*(np.array((255,226,150),np.float32)/255),'add')
    P[...,:3]*=(P[...,3:4]>0)
    P[...,:3]=np.minimum(P[...,:3],P[...,3:4])
    return P
def frame(i,t,c=None):
    c=(C[i].astype(np.float32) if c is None else c)
    P=lit(t)
    sm=isolate(c)
    # moving ground shadow on the plate
    sh=np.clip(c[...,3]-plate[...,3],0,255)*(1-sm[...,0]);sh[:640]=0;sh[:,:150]=0;sh[:,600:]=0
    sh=cv2.GaussianBlur(sh,(0,0),1.5)[...,None]/255
    P=P*(1-sh)
    P[...,3:4]+=sh*255
    return c*sm+P*(1-sm)
def save(pm,idx):
    small=cv2.resize(pm,(OUT_SIDE,OUT_SIDE),interpolation=cv2.INTER_AREA)
    a=np.maximum(small[...,3:4],1e-3)/255
    rgb=np.clip(small[...,:3]/a,0,255)
    return Image.fromarray(np.dstack([rgb,np.clip(small[...,3:],0,255)]).astype(np.uint8),'RGBA')
# seam: symmetric blend centred on the matched poses (k=0), where pairs agree best
def morph(k):
    q=(k+K+1)/(2*K+2); w=q*q*(3-2*q)
    a=C[Bn+k].astype(np.float32);b=C[A+k].astype(np.float32)
    ga=cv2.cvtColor(np.clip(a[...,:3],0,255).astype(np.uint8),cv2.COLOR_RGB2GRAY);gb=cv2.cvtColor(np.clip(b[...,:3],0,255).astype(np.uint8),cv2.COLOR_RGB2GRAY)
    dis=cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM);dis.setFinestScale(1);dis.setPatchSize(16);dis.setPatchStride(4);dis.setVariationalRefinementIterations(10)
    fl=dis.calc(ga,gb,None);fr=dis.calc(gb,ga,None)
    fl=cv2.GaussianBlur(fl,(0,0),4);fr=cv2.GaussianBlur(fr,(0,0),4)
    yy,xx=np.mgrid[0:N,0:N].astype(np.float32)
    wa=cv2.remap(a,xx+fr[...,0]*w,yy+fr[...,1]*w,cv2.INTER_LINEAR)
    wb=cv2.remap(b,xx+fl[...,0]*(1-w),yy+fl[...,1]*(1-w),cv2.INTER_LINEAR)
    return wa*(1-w)+wb*w
rendered=[]
for j in range(L):
    i=A+j;t=j/L
    kk=j if j<=K else (j-L if j>=L-K else None)
    rendered.append(save(frame(i,t,morph(kk)) if kk is not None else frame(i,t),j))

durations = [42, 42, 41] * (L // 3) + [42] * (L % 3)  # 24 fps in whole milliseconds
target = ROOT / "resources" / "seasonal" / "santa.webp"
rendered[0].save(target, "WEBP", save_all=True, append_images=rendered[1:], duration=durations, loop=0,
                 quality=QUALITY, alpha_quality=ALPHA_QUALITY, method=6, minimize_size=False, allow_mixed=False)
shutil.copy2(target, ROOT / "android" / "app" / "src" / "main" / "assets" / "seasonal" / "santa.webp")
print(f"{L} frames, {L / 24:.2f} s, {target.stat().st_size} bytes")

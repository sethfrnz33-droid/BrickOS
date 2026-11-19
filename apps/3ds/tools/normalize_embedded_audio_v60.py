#!/usr/bin/env python3
"""Offline preparation only; production builds use the PCM files already embedded.

This script levels all 14 user-provided clips already stored in the project,
with a transparent soft limiter. The supplied burn-music upload replaces 07.pcm.
No runtime dependencies or SD copying are necessary.
"""
from pathlib import Path
import numpy as np
import soundfile as sf
import argparse
from scipy.signal import resample_poly
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'art/event_audio'
parser=argparse.ArgumentParser()
parser.add_argument('--song',type=Path,required=True,help='Original user-supplied music MP3')
src=parser.parse_args().song
assert src.exists(),src
original,sr=sf.read(src,always_2d=True,dtype='float32')
mono=original.mean(axis=1)
from math import gcd
g=gcd(sr,22050)
converted=resample_poly(mono,22050//g,sr//g).astype(np.float64)
assert 25*22050<len(converted)<36*22050
# Ensure each replayed 30-second loop joins without a sharp click.
fades=min(3500,len(converted)//6)
converted[:fades]*=np.linspace(0,1,fades)
converted[-fades:]*=np.linspace(1,0,fades)
# Retain the original 13 other clips. The previous versions are relevelled,
# but the conversion is idempotent only for the initial v5.9 source set.
for i in range(14):
    if i==7:x=converted
    else:x=np.fromfile(out/f'{i:02d}.pcm',dtype='<i2').astype(np.float64)/32768.0
    assert x.size>100
    # Match the perceived level by bringing the whole-clip RMS toward 0.26.
    # tanh acts as a sample-wise limiter so loud transients don't clip.
    lo,hi=0.1,25.0
    for _ in range(34):
        gain=(lo+hi)/2
        y=.935*np.tanh(gain*x/.8)
        rms=float(np.sqrt(np.mean(y*y)))
        if rms<.27:lo=gain
        else:hi=gain
    y=.935*np.tanh(((lo+hi)/2)*x/.8)
    peak=float(np.max(np.abs(y)));rms=float(np.sqrt(np.mean(y*y)))
    assert 0.16<rms<=0.37 and peak<=.936
    pcm=(np.clip(y,-.936,.936)*32767).astype('<i2')
    pcm.tofile(out/f'{i:02d}.pcm')
    print(f'{i:02d}: {len(y)/22050:.2f} s, RMS={rms:.3f}, peak={peak:.3f}, PCM={len(pcm)*2:,} bytes')

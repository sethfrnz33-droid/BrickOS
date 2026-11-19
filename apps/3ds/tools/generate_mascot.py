#!/usr/bin/env python3
"""Generate the 3DS mascot sprites, 48px HOME icon, banner texture, and original PCM voice.
Only Python standard library is required.  All art comes from the bundled 64px source pixels.
The mouth seam intersects the lip line, not the nose or an arbitrary forehead cut.
"""
from pathlib import Path
import base64, zlib, struct, math, random
OUT = Path(__file__).resolve().parents[1]
SEED = OUT/'art/mascot/seed.txt'
W=H=64
rgba=bytearray(zlib.decompress(base64.b64decode(''.join(SEED.read_text().split()))))
assert len(rgba)==W*H*4, 'Mascot source dimensions invalid'

def png(w,h,pixels,path):
 def chunk(name,payload):return struct.pack('>I',len(payload))+name+payload+struct.pack('>I',zlib.crc32(name+payload)&0xffffffff)
 raw=b''.join(b'\x00'+bytes(pixels[y*w*4:(y+1)*w*4]) for y in range(h))
 payload=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
 path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(payload)

def pixel(x,y):return rgba[4*(y*W+x):4*(y*W+x)+4]
def setpixel(out,x,y,p):out[4*(y*W+x):4*(y*W+x)+4]=p

def inside_polygon(x,y,points):
 inside=False;j=len(points)-1
 for i in range(len(points)):
  a,b=points[i],points[j]
  if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:inside=not inside
  j=i
 return inside

# Coordinates derive from the approved full-resolution lip cut at y=951/1254.
scale=W/1254.;points=[(round(x*scale),round(y*scale)) for x,y in [
 (352,951),(873,951),(864,980),(855,1010),(829,1031),(812,1064),
 (782,1097),(751,1123),(708,1140),(676,1169),(548,1169),
 (525,1148),(468,1135),(430,1104),(405,1079),(387,1034),(370,998)]]
head=bytearray(rgba);jaw=bytearray(len(rgba));mouth=bytearray(len(rgba))
for y in range(H):
 for x in range(W):
  p=4*(y*W+x)
  if inside_polygon(x,y,points):
   jaw[p:p+4]=rgba[p:p+4];head[p:p+4]=b'\x00\x00\x00\x00'
  if (24<=x<=39) and (y in range(48,53)):
   mouth[p:p+4]=(72,28,31,255) if y<51 else (121,47,52,255)

# NN 2x sprite scaling, no blurring at the 3DS low resolution.
def scale2(pixels):
 out=bytearray(128*128*4)
 for y in range(128):
  for x in range(128):
   i=4*((y//2)*64+(x//2));o=4*(y*128+x);out[o:o+4]=pixels[i:i+4]
 return out
p=OUT/'art/mascot'
for name,layer in [('head',head),('jaw',jaw),('mouth',mouth),('full',rgba)]:
 png(128,128,scale2(layer),p/(name+'.png'))
# 48x48 smdh icon, correctly sized to have a padded silhouette.
icon=bytearray(48*48*4)
for y in range(48):
 for x in range(48):
  sx=round((x-2)*64/44);sy=round((y-2)*64/44)
  if 0<=sx<64 and 0<=sy<64:icon[4*(y*48+x):4*(y*48+x)+4]=pixel(sx,sy)
png(48,48,icon,OUT/'icon.png')
# Independent transparent texture for HOME Menu banner, NOT the 48px tile.
texture=bytearray(256*128*4)
for y in range(128):
 for x in range(256):
  sx=(x-64)//2;sy=y//2
  if 0<=sx<64 and 0<=sy<64:
   q=4*(y*256+x);texture[q:q+4]=pixel(sx,sy)
png(256,128,texture,OUT/'home-banner/mascot_banner.png')
# Voice: originally synthesized gibberish (vowels, no words or borrowed samples).
sample_rate=22050
syllables=[('a',0.16,165),('e',0.12,173),('o',0.14,155),('a',0.11,191),('u',0.18,148),('e',0.13,181),('a',0.15,162)]
formants={'a':(690,1180),'e':(440,1650),'o':(490,890),'u':(360,890)}
voice=[]
for form,duration,pitch in syllables:
 f1,f2=formants[form];phase=0.;n=int(duration*sample_rate)
 for i in range(n):
  t=i/sample_rate;fade=max(0,min(1,t/.021,(duration-t)/.04))
  f0=pitch*(1+.12*math.sin(2*math.pi*7*t));phase+=2*math.pi*f0/sample_rate
  v=sum(math.sin(h*phase+.3*h)*math.exp(-((f0*h-f1)/300)**2)*(.45/h)+math.sin(h*phase)*math.exp(-((f0*h-f2)/430)**2)*(.25/h) for h in range(1,23))
  voice.append(int(max(-1,min(1,v*1.15*fade))*14500))
 voice.extend([0]*int(.018*sample_rate))
(OUT/'art/mascot/voice.pcm').write_bytes(struct.pack('<%dh'%len(voice),*voice))
print('Generated mascot layers, SMDH icon, HOME banner texture and',len(voice),'voice samples')

# Real textured 2.5D GLB for the separate 3DS HOME Menu banner.
# HOME Menu icon.png is the SMDH tile; the CGFX banner is independent.
import json
banner_png=(OUT/'home-banner/mascot_banner.png').read_bytes()
verts=struct.pack('<12f',-1.0,-0.5,0, 1.0,-0.5,0, 1.0,0.5,0, -1.0,0.5,0)
uvs=struct.pack('<8f',0.,1.,1.,1.,1.,0.,0.,0.)
faces=struct.pack('<6H',0,1,2,0,2,3)
payload=verts+uvs+faces
while len(payload)%4:payload+=b'\0'
image_offset=len(payload);payload+=banner_png
while len(payload)%4:payload+=b'\0'
scene={
 'asset':{'version':'2.0','generator':'Brick Tuner Suite mascot asset generator'},
 'scene':0,'scenes':[{'nodes':[0]}],
 'nodes':[{'name':'Brick ECU mascot','mesh':0,'rotation':[0,0,0,1]}],
 'meshes':[{'name':'Brick ECU Mascot Billboard','primitives':[{'attributes':{'POSITION':0,'TEXCOORD_0':1},'indices':2,'material':0,'mode':4}]}],
 'accessors':[
  {'bufferView':0,'componentType':5126,'count':4,'type':'VEC3','min':[-1.,-.5,0.],'max':[1.,.5,0.]},
  {'bufferView':1,'componentType':5126,'count':4,'type':'VEC2'},
  {'bufferView':2,'componentType':5123,'count':6,'type':'SCALAR'}],
 'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':len(verts),'target':34962},
                {'buffer':0,'byteOffset':len(verts),'byteLength':len(uvs),'target':34962},
                {'buffer':0,'byteOffset':len(verts)+len(uvs),'byteLength':len(faces),'target':34963},
                {'buffer':0,'byteOffset':image_offset,'byteLength':len(banner_png)}],
 'buffers':[{'byteLength':len(payload)}],
 'textures':[{'sampler':0,'source':0}],
 'samplers':[{'magFilter':9728,'minFilter':9728,'wrapS':33071,'wrapT':33071}],
 'images':[{'bufferView':3,'mimeType':'image/png'}],
 'materials':[{'name':'Transparent mascot','doubleSided':True,'alphaMode':'BLEND',
               'pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicFactor':0.,'roughnessFactor':1.}}]
}
js=json.dumps(scene,separators=(',',':')).encode('utf8');js+=b' '*((-len(js))%4)
header=struct.pack('<4sII',b'glTF',2,12+8+len(js)+8+len(payload))
glb=header+struct.pack('<I4s',len(js),b'JSON')+js+struct.pack('<I4s',len(payload),b'BIN\0')+payload
(OUT/'home-banner/mascot_banner.glb').write_bytes(glb)
print('Generated mascot 2.5D HOME GLB:',len(glb),'bytes')

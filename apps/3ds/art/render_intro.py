#!/usr/bin/env python3
"""Offline artwork generator: rasterize the user's E46 glTF into low-memory 3DS sprite frames.
Only needed when regenerating the included PNG frames; `make` uses the PNGs directly.
"""
from pathlib import Path
import math
import cv2
import numpy as np
import trimesh
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT/'home-banner'/'e46_gutted_original.glb'
OUT = ROOT/'art'/'intro_car'
OUT.mkdir(parents=True, exist_ok=True)
N=16; W,H=224,128; AA=2
scene=trimesh.load(str(SRC),force='scene')

parts=[]
for node in scene.graph.nodes_geometry:
    trans,name=scene.graph[node]
    mesh=scene.geometry[name]
    pts=trimesh.transform_points(mesh.vertices,trans)
    faces=np.array(mesh.faces)
    uv=np.asarray(mesh.visual.uv) if hasattr(mesh.visual,'uv') and mesh.visual.uv is not None else None
    mat=mesh.visual.material
    img=getattr(mat,'baseColorTexture',None)
    if img is not None:
        # Mipmapped source image is plenty at 224x128 pixels.
        tex=np.asarray(img.convert('RGB').resize((256,256),Image.Resampling.LANCZOS))
        coords=uv[faces].mean(axis=1)
        tx=np.clip((coords[:,0]%1*255).astype(int),0,255)
        ty=np.clip(((1-coords[:,1]%1)*255).astype(int),0,255)
        base=tex[ty,tx].astype(np.float32)
    else:
        raw=getattr(mat,'baseColorFactor',None)
        color=np.array(raw if raw is not None else [85,90,94,255],dtype=np.float32)[:3]
        base=np.tile(color,(len(faces),1)).astype(np.float32)
    parts.append((pts, faces, base))
print('Faces',sum(len(f) for _,f,_ in parts))

# Camera looks down on car from above, 25 degrees from horizon.
# Model up = +Y, longitudinal = +Z.
cam=np.array([0.,3.4,8.3]);target=np.array([0.,0.35,0.]);
forward=(target-cam)/np.linalg.norm(target-cam)
right=np.cross(forward,[0,1,0]);right/=np.linalg.norm(right)
up=np.cross(right,forward)
look=np.vstack([right,up,forward]).T
light=np.array([-.55,.85,.65]);light/=np.linalg.norm(light)

for n in range(N):
    angle=(2*math.pi*n/N)+math.radians(32)
    ca,sa=np.cos(angle),np.sin(angle)
    rot=np.array([[ca,0,sa],[0,1,0],[-sa,0,ca]])
    triangles=[]
    for xyz, faces, color in parts:
        world=xyz@rot.T
        rel=world-cam
        coords=rel@look
        z=coords[:,2]
        # Perspective factor, moderately telephoto to avoid tiny nose shots.
        scale=690/(z+1.0)
        # Model toward screen center; camera pitched to show the top/roof.
        sx=(W*AA*.5)+coords[:,0]*scale
        sy=(H*AA*.54)-coords[:,1]*scale
        p=np.stack([sx,sy],axis=1)
        v1=world[faces[:,1]]-world[faces[:,0]]
        v2=world[faces[:,2]]-world[faces[:,0]]
        norm=np.cross(v1,v2)
        ln=np.linalg.norm(norm,axis=1)+1e-10
        norm/=ln[:,None]
        lit=np.clip(.54+.58*(norm@light),.35,1.06)
        col=np.clip(color*lit[:,None]+11,0,255).astype(np.uint8)
        depth=z[faces].mean(axis=1)
        for i,face in enumerate(faces):
            pts2=p[face]
            if np.min(z[face])<.1 or np.max(pts2[:,0])<0 or np.min(pts2[:,0])>=W*AA or np.max(pts2[:,1])<0 or np.min(pts2[:,1])>=H*AA:
                continue
            triangles.append((float(depth[i]),pts2.astype(np.int32),col[i]))
    # Back-to-front draw by increasing depth; camera looks forward (positive z).
    triangles.sort(key=lambda x:x[0],reverse=True)
    canvas=np.zeros((H*AA,W*AA,4),dtype=np.uint8)
    for _,tri,c in triangles:
        cv2.fillConvexPoly(canvas,tri,(int(c[0]),int(c[1]),int(c[2]),255),lineType=cv2.LINE_AA)
    rgba=cv2.resize(canvas,(W,H),interpolation=cv2.INTER_AREA)
    Image.fromarray(rgba,'RGBA').save(OUT/f'{n:02d}.png',optimize=True)

# Contact sheet for checking angles (internal development asset).
imgs=[Image.open(OUT/f'{i:02d}.png') for i in range(N)]
contact=Image.new('RGB',(W*4,H*4),(11,16,21))
for i,im in enumerate(imgs):contact.paste(im,((i%4)*W,(i//4)*H),im)
contact.save(ROOT/'intro_car_preview.png')
print('Saved',N,'frames to',OUT)

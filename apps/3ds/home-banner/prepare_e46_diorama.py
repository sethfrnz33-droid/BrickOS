#!/usr/bin/env python3
"""Create v4.9 HOME Menu 3D scene from the proven v4.8 single-sided E46.

Adds a low-poly more-closed 3DS console above the car, a physical cable from the
driver window, and a thicker static front-facing 'BRICK ECU TUNER SUITE' title.
Only used to regenerate the included GLBs; CIA build consumes the ready GLBs.
Requires numpy, trimesh and Pillow.
"""
from __future__ import annotations
import io, json, math, struct
from pathlib import Path
import numpy as np
import trimesh
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent


def read_glb(path):
    binary=Path(path).read_bytes()
    magic, version, total=struct.unpack_from('<III',binary,0)
    assert magic==0x46546C67 and version==2 and total==len(binary)
    pos=12; doc=None; buffer=None
    while pos<total:
        count,ct=struct.unpack_from('<I4s',binary,pos);pos+=8
        data=binary[pos:pos+count];pos+=count
        if ct==b'JSON':doc=json.loads(data)
        elif ct==b'BIN\x00':buffer=data
    assert doc is not None and buffer is not None and len(doc['buffers'])==1
    return doc,bytearray(buffer)


def write_glb(path,doc,buf):
    doc['buffers'][0]['byteLength']=len(buf)
    text=json.dumps(doc,separators=(',',':'),ensure_ascii=True).encode()
    text+=b' ' * (-len(text)%4)
    buf.extend(b'\x00'*(-len(buf)%4))
    blob=(struct.pack('<I4s',len(text),b'JSON')+text+
          struct.pack('<I4s',len(buf),b'BIN\x00')+buf)
    Path(path).write_bytes(struct.pack('<III',0x46546C67,2,len(blob)+12)+blob)


def put_view(doc,buf,data,target=None):
    buf.extend(b'\x00'*(-len(buf)%4))
    start=len(buf);buf.extend(data)
    view={'buffer':0,'byteOffset':start,'byteLength':len(data)}
    if target is not None:view['target']=target
    idx=len(doc['bufferViews']);doc['bufferViews'].append(view)
    return idx


def put_accessor(doc,view,count,component_type,gltf_type,vmin=None,vmax=None):
    a={'bufferView':view,'byteOffset':0,'componentType':component_type,'count':count,'type':gltf_type}
    if vmin is not None:a['min']=[float(x) for x in vmin]
    if vmax is not None:a['max']=[float(x) for x in vmax]
    idx=len(doc['accessors']);doc['accessors'].append(a);return idx


def material(doc,name,color,alpha='OPAQUE',texture=None):
    pbr={'baseColorFactor':[float(x) for x in color],
         'metallicFactor':0.0,'roughnessFactor':0.9}
    if texture is not None:pbr['baseColorTexture']={'index':texture}
    obj={'name':name,'pbrMetallicRoughness':pbr,'doubleSided':False,'alphaMode':alpha}
    idx=len(doc['materials']);doc['materials'].append(obj);return idx


def add_mesh(doc,buf,vertices,faces,matidx,name,uvs=None):
    v=np.asarray(vertices,dtype=np.float32)
    f=np.asarray(faces,dtype=np.uint16)
    if not len(v) or not len(f):return None
    # Smooth normals are fine for the tiny Nintendo console; cylindrical cable
    # benefits from smooth normals. This avoids per-face vertex duplication.
    geom=trimesh.Trimesh(vertices=v,faces=f,process=False)
    vn=np.asarray(geom.vertex_normals,dtype='<f4')
    vp=put_view(doc,buf,v.astype('<f4').tobytes(),34962)
    npv=put_view(doc,buf,vn.tobytes(),34962)
    ip=put_view(doc,buf,f.astype('<u2').reshape(-1).tobytes(),34963)
    pos=put_accessor(doc,vp,len(v),5126,'VEC3',v.min(axis=0),v.max(axis=0))
    norm=put_accessor(doc,npv,len(v),5126,'VEC3')
    ind=put_accessor(doc,ip,f.size,5123,'SCALAR')
    attr={'POSITION':pos,'NORMAL':norm}
    if uvs is not None:
        uv=put_view(doc,buf,np.asarray(uvs,dtype='<f4').tobytes(),34962)
        attr['TEXCOORD_0']=put_accessor(doc,uv,len(v),5126,'VEC2')
    prim={'attributes':attr,'indices':ind,'material':matidx,'mode':4}
    idx=len(doc['meshes']);doc['meshes'].append({'name':name,'primitives':[prim]})
    return idx


def make_box(center,extents):
    m=trimesh.creation.box(extents=extents)
    m.apply_translation(center)
    return m


def tube(points,radius=0.018,sides=7):
    pts=np.asarray(points,dtype=float)
    verts=[];faces=[]
    for i,p in enumerate(pts):
        tangent=pts[min(len(pts)-1,i+1)]-pts[max(0,i-1)]
        tangent/=max(np.linalg.norm(tangent),1e-9)
        a=np.cross(tangent,[0,0,1])
        if np.linalg.norm(a)<1e-5:a=np.cross(tangent,[0,1,0])
        a/=np.linalg.norm(a)
        b=np.cross(tangent,a)
        for q in range(sides):
            theta=2*math.pi*q/sides
            verts.append(p+radius*(a*math.cos(theta)+b*math.sin(theta)))
    for i in range(len(pts)-1):
        for j in range(sides):
            a=i*sides+j;c=i*sides+(j+1)%sides
            b=(i+1)*sides+j;d=(i+1)*sides+(j+1)%sides
            faces += [[a,b,c],[b,d,c]]
    # end caps
    verts.extend([pts[0],pts[-1]])
    for q in range(sides):
        faces.append([len(verts)-2,(q+1)%sides,q])
        t=(len(pts)-1)*sides
        faces.append([len(verts)-1,t+q,t+(q+1)%sides])
    return trimesh.Trimesh(vertices=verts,faces=faces,process=False)


def cylinder_button(cx,cy,cz,radius,depth=0.012):
    cyl=trimesh.creation.cylinder(radius=radius,height=depth,sections=10)
    cyl.apply_translation([cx,cy,cz]);return cyl


def label_image():
    # Compact power-of-two RGBA texture.  The text is NOT part of the car's
    # spin rig: it remains visible throughout the 360-degree rotation.
    out=Image.new('RGBA',(256,48),(0,0,0,0))
    d=ImageDraw.Draw(out)
    font_candidates=[
        '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',
        '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
        '/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf']
    font=None
    for p in font_candidates:
        if Path(p).exists():font=ImageFont.truetype(p,24);break
    if font is None:font=ImageFont.load_default()
    line='BRICK ECU TUNER SUITE'
    box=d.textbbox((0,0),line,font=font,stroke_width=1)
    width=box[2]-box[0];height=box[3]-box[1]
    # Center as one title, cream with a dark outline and slight shadow.
    x=(256-width)//2; y=(48-height)//2-box[1]
    d.text((x+2,y+2),line,font=font,fill=(0,0,0,135),stroke_width=2,stroke_fill=(0,0,0,160))
    d.text((x,y),line,font=font,fill=(245,235,211,255),stroke_width=2,stroke_fill=(24,22,26,255))
    return out


def create(input_file,output_file,include_console=True):
    doc,buf=read_glb(input_file)
    assert len(doc['scenes'])==1 and len(doc['animations'])==1
    rig=next(i for i,x in enumerate(doc['nodes']) if x.get('name')=='E46_SPIN_ROOT')
    # User reported that the car was tiny and appeared submerged.  Make it
    # about 43% larger and lift it, while preserving all 5,673 triangles.
    doc['nodes'][rig]['scale']=[5.0,5.0,5.0]
    doc['nodes'][rig]['translation']=[-1.85,-1.85,0.0]
    doc['nodes'][rig]['children']=list(doc['nodes'][rig].get('children',[]))
    # Present the car in a 3/4 side angle when the HOME Menu first opens,
    # instead of a narrow near-head-on view. Preserve 8-second full rotation.
    spin=doc['animations'][0]['samplers'][0]
    rot=doc['accessors'][spin['output']]
    view=doc['bufferViews'][rot['bufferView']]
    offset=view.get('byteOffset',0)+rot.get('byteOffset',0)
    for frame in range(rot['count']):
        angle=math.radians(55.0 + 45.0*frame)
        struct.pack_into('<4f',buf,offset+16*frame,
                         0.0,math.sin(angle/2),0.0,math.cos(angle/2))
    doc['meshes']=list(doc['meshes'])
    doc['nodes']=list(doc['nodes'])
    groups=[]
    if include_console:
        # Two real hinged shells, screens, tactile buttons, speaker details,
        # plus a tube that physically joins the driver's side to the 3DS.
        shell=[
            make_box((1.84,0.28,0.67),(.68,.48,.055)),
            make_box((1.84,-.13,.73),(.74,.54,.062)),
            make_box((1.84,.07,.69),(.68,.042,.082)),
        ]
        dark=[
            make_box((1.84,.29,.697),(.56,.34,.012)),
            make_box((1.84,-.12,.768),(.43,.29,.011)),
            make_box((1.60,-.30,.775),(.120,.030,.010)),
            make_box((1.60,-.30,.776),(.030,.120,.010)),
            make_box((2.10,-.08,.766),(.14,.035,.010)),
        ]
        screens=[
            make_box((1.84,.30,.706),(.49,.25,.005)),
            make_box((1.84,-.10,.776),(.34,.21,.005)),
        ]
        controls=[
            cylinder_button(2.13,-.24,.775,.030),
            cylinder_button(2.18,-.33,.775,.026),
            cylinder_button(2.06,-.33,.775,.026),
            cylinder_button(2.12,-.42,.775,.022),
            cylinder_button(1.56,-.43,.778,.015),
            cylinder_button(1.64,-.43,.778,.015),
            cylinder_button(1.84,.54,.698,.018),
        ]
        wire=tube([
            (-0.10,0.34,1.20),(-0.02,0.44,1.42),
            (0.12,0.52,1.66),(0.34,0.57,1.93),
            (0.52,0.60,2.18),(0.72,0.56,2.38),
            (0.86,0.46,2.50),
        ],radius=.020,sides=7)
        groups=[('3DS Ice-grey shell',shell,[.71,.76,.81,1.0]),
                ('Dark screen bezels',dark,[.08,.11,.15,1.0]),
                ('Twin glass displays',screens,[.22,.62,.81,1.0]),
                ('Controls and buttons',controls,[.28,.34,.40,1.0]),
                ('Wired connection', [wire],[.13,.14,.18,1.0])]
    for name,parts,color in groups:
        mesh=trimesh.util.concatenate(parts)
        # Handheld sits above the car and stays visibly cabled to the driver window.
        if name!='Wired connection':
            mesh.apply_translation([-1.00,0.08,1.05])
        matidx=material(doc,'BTS '+name,color)
        midx=add_mesh(doc,buf,mesh.vertices,mesh.faces,matidx,name)
        nid=len(doc['nodes']);doc['nodes'].append({'name':name,'mesh':midx})
        doc['nodes'][rig]['children'].append(nid)

    # Static title mesh facing the HOME Menu camera (camera at +Z).
    im=label_image();f=io.BytesIO();im.save(f,format='PNG',optimize=True)
    img_view=put_view(doc,buf,f.getvalue())
    imageid=len(doc['images']);doc['images'].append({'name':'BRICK ECU TUNER SUITE title','mimeType':'image/png','bufferView':img_view})
    tid=len(doc['textures']);doc['textures'].append({'name':'BTS title','source':imageid,'sampler':0})
    white=material(doc,'BTS Floating Title',[1,1,1,1],alpha='BLEND',texture=tid)
    # Thicker fixed title strip, kept lower and front-facing so it reads clearly.
    verts=np.array([[-12.0,5.80,0],[12.0,5.80,0],[12.0,9.00,0],[-12.0,9.00,0]],dtype=float)
    faces=np.array([[0,1,2],[0,2,3]],dtype=int)
    uv=np.array([[0,1],[1,1],[1,0],[0,0]],dtype=float)
    name='BRICK ECU TUNER SUITE - stationary title'
    meshid=add_mesh(doc,buf,verts,faces,white,name,uvs=uv)
    title_node=len(doc['nodes']);doc['nodes'].append({'name':name,'mesh':meshid})
    doc['scenes'][0]['nodes'].append(title_node)

    doc['asset']['generator']='Brick Tuner Suite v5.0 - E46 + overhead handheld cable diorama'
    write_glb(output_file,doc,buf)
    return dict(file=str(output_file),bytes=Path(output_file).stat().st_size,
                meshes=len(doc['meshes']),images=len(doc['images']),
                console=include_console,triangles=sum(
                    len(x['primitives']) for x in doc['meshes']))


if __name__=='__main__':
    for label,src,console in [
        ('e46_diorama_full.glb','e46_home_banner_lite_single.glb',True),
        ('e46_diorama_tiny.glb','e46_home_banner_tiny_single.glb',True),
        ('e46_diorama_safe.glb','e46_home_banner_tiny_single.glb',False),
    ]:
        print(create(ROOT/src,ROOT/label,include_console=console))

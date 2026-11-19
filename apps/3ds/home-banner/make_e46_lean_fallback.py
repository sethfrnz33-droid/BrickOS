#!/usr/bin/env python3
"""Generate last-resort E46 + handheld CGFX-budget variant.

Only two dense rim material primitives are reduced using VTK quadric decimation;
the car body, windows, paint, lights, tyres, handheld, cable, title and spin
animation remain unchanged. The input v4.9 GLB stays intact.

Not required during a normal CIA build; a prepared output GLB ships with it.
"""
from pathlib import Path
import json
import numpy as np
import trimesh
import vtk
from vtk.util.numpy_support import numpy_to_vtk,numpy_to_vtkIdTypeArray,vtk_to_numpy
from prepare_e46_diorama import read_glb,write_glb,put_view,put_accessor

ROOT=Path(__file__).resolve().parent

def get_accessor(doc,buf,idx):
    a=doc['accessors'][idx];v=doc['bufferViews'][a['bufferView']]
    kind={5126:'<f4',5123:'<u2',5125:'<u4'}[a['componentType']]
    dim={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    offset=v.get('byteOffset',0)+a.get('byteOffset',0)
    return np.frombuffer(buf,dtype=kind,count=a['count']*dim,offset=offset).copy().reshape(a['count'],dim)

def write_accessor(doc,buf,arr,typ,component=5126,target=34962):
    arr=np.asarray(arr)
    dt={5126:'<f4',5123:'<u2',5125:'<u4'}[component]
    arr=arr.astype(dt)
    v=put_view(doc,buf,arr.tobytes(),target)
    idx=put_accessor(doc,v,len(arr),component,typ,arr.min(axis=0) if typ!='SCALAR' else None,arr.max(axis=0) if typ!='SCALAR' else None)
    return idx

def simplify_primitive(doc,buf,prim,keep=.45):
    xyz=get_accessor(doc,buf,prim['attributes']['POSITION']).astype('float32')
    uv=get_accessor(doc,buf,prim['attributes']['TEXCOORD_0']).astype('float32')
    indices=get_accessor(doc,buf,prim['indices']).ravel().reshape(-1,3)
    points=vtk.vtkPoints();points.SetData(numpy_to_vtk(xyz,deep=1))
    cell=vtk.vtkCellArray()
    cells=np.column_stack((np.full(len(indices),3,dtype=np.int64),indices)).ravel()
    cell.SetCells(len(indices),numpy_to_vtkIdTypeArray(cells,deep=1))
    poly=vtk.vtkPolyData();poly.SetPoints(points);poly.SetPolys(cell)
    uvd=vtk.vtkFloatArray();uvd.SetName('TCoords');uvd.SetNumberOfComponents(2)
    uvd.SetNumberOfTuples(len(uv))
    for i,(u,v) in enumerate(uv):uvd.SetTuple2(i,float(u),float(v))
    poly.GetPointData().SetTCoords(uvd)
    cleaner=vtk.vtkCleanPolyData();cleaner.SetInputData(poly)
    cleaner.ToleranceIsAbsoluteOn();cleaner.SetAbsoluteTolerance(0.00001)
    cleaner.Update()
    d=vtk.vtkQuadricDecimation();d.SetInputConnection(cleaner.GetOutputPort())
    d.SetTargetReduction(1-keep)
    d.AttributeErrorMetricOn();d.SetTCoordsWeight(0.4)
    d.Update();geo=d.GetOutput()
    npos=vtk_to_numpy(geo.GetPoints().GetData()).copy().astype('<f4')
    nidx=vtk_to_numpy(geo.GetPolys().GetData()).reshape(-1,4)[:,1:].copy().astype('<u2')
    if geo.GetPointData().GetTCoords() is None:
        raise RuntimeError('VTK unexpectedly discarded UVs')
    nuv=vtk_to_numpy(geo.GetPointData().GetTCoords()).copy().astype('<f4')
    normals=trimesh.Trimesh(vertices=npos,faces=nidx,process=False).vertex_normals.astype('<f4')
    assert len(nidx)<len(indices) and len(nidx)>100
    prim['attributes']['POSITION']=write_accessor(doc,buf,npos,'VEC3')
    prim['attributes']['NORMAL']=write_accessor(doc,buf,normals,'VEC3')
    prim['attributes']['TEXCOORD_0']=write_accessor(doc,buf,nuv,'VEC2')
    prim['indices']=write_accessor(doc,buf,nidx.reshape(-1,1),'SCALAR',5123,34963)
    return len(indices),len(nidx)

if __name__=='__main__':
    doc,buf=read_glb(ROOT/'e46_diorama_tiny.glb')
    for idx in (18,21):
        old,new=simplify_primitive(doc,buf,doc['meshes'][0]['primitives'][idx],keep=.46)
        print('wheel primitive',idx,'triangles',old,'->',new)
    out=ROOT/'e46_diorama_lean.glb'
    write_glb(out,doc,buf)
    print(out,'size',out.stat().st_size)

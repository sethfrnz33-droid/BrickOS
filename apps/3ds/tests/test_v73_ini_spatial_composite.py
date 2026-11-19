#!/usr/bin/env python3
"""Spatial TunerStudio UI regression using the actual Proteus F7 INI.

No hardware control: validate real panel hierarchy, real gauge and indicator
declarations, consistent interactive regions, and read-only chart sourcing.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
ini = (ROOT / "firmware/esp32/tests/fixtures/proteus_f7_generated.ini").read_text(errors="replace")
src = (ROOT / "apps/3ds/source/main.c").read_text()

def csv(text):
    cells=[]; buffer=[]; in_quote=False; depth=0
    for ch in text:
        if ch=='"': in_quote=not in_quote
        if not in_quote:
            if ch in '[({': depth+=1
            elif ch in '])}': depth-=1
            if ch==';' and depth==0: break
            if ch==',' and depth==0:
                cells.append(''.join(buffer).strip().strip('"'))
                buffer=[]
                continue
        buffer.append(ch)
    cells.append(''.join(buffer).strip().strip('"'))
    return cells

dialog=""
section=""
panels={}
indicators={}
readouts={}
controls={}
for raw in ini.splitlines():
    row=raw.strip()
    if row.startswith('[') and row.endswith(']'):
        section=row[1:-1];continue
    if section!="UserDefined" or not row or row[0] in '#;' or '=' not in row:continue
    key,value=map(str.strip,row.split('=',1))
    fields=csv(value)
    if key in ('dialog','indicatorPanel','readoutPanel') and fields:
        dialog=fields[0]
    elif key=='panel' and fields:
        panels.setdefault(dialog,[]).append((fields[0], fields[1] if len(fields)>1 else ""))
    elif key=='indicator' and len(fields)>=3:
        indicators.setdefault(dialog,[]).append((fields[0],fields[1],fields[2]))
    elif key=='readout' and fields:
        readouts.setdefault(dialog,[]).append(fields[0])
    elif key in ('field','gauge','commandButton') and fields:
        controls.setdefault(dialog,[]).append((key,fields))

assert ("secondVeTableControls","West") in panels["secondVeTableDialog"]
assert ("secondVeTableTbl","East") in panels["secondVeTableDialog"]
assert ("secondVeBias","") in panels["secondVeTableControls"]
assert ("secondVeTableActivePanel","") in panels["secondVeTableControls"]
assert ("secondVeTableReadout","") in panels["secondVeTableControls"]
assert any("isSecondVeTableActive" in expr for expr,*_ in indicators["secondVeTableActivePanel"])
assert ("{ isSecondVeTableActive }","Primary VE table","Second VE table") in indicators["secondVeTableActivePanel"]
assert "veValueGauge" in readouts["secondVeTableReadout"]
assert ("outputsDialogLeft","West") in panels["outputsDialog"]
assert ("outputsDialogRight","East") in panels["outputsDialog"]
assert {"mainRelayDialog","fuelPumpDialog","tachSettings"}.issubset(set(name for name,_ in panels["outputsDialogLeft"]))
assert {"startStopDialog","starterRelayDialog","speedoSettings"}.issubset(set(name for name,_ in panels["outputsDialogRight"]))
assert any(fields[0]=="Auto Calibrate ETB 1" for kind,fields in controls["tpsNum1EtbCalibration"] if kind=="commandButton")
assert any(fields[0]=="Auto Calibrate ETB 2" for kind,fields in controls["tpsNum2"] if kind=="commandButton")
assert re.search(r'panel\s*=\s*vvtAdvancedModeSettings,\s*North',ini)
assert re.search(r'panel\s*=\s*vvtAdvancedModeDialog,\s*South',ini)

required=(
 "BTS_REGION_NORTH","BTS_REGION_WEST","BTS_REGION_CENTER",
 "BTS_REGION_EAST","BTS_REGION_SOUTH",
 'bts_ini_region(a[1])',
 'else if(!strcasecmp(key,"indicatorPanel")',
 'else if(!strcasecmp(key,"readoutPanel")',
 'else if(!strcasecmp(key,"indicator")',
 'else if(!strcasecmp(key,"readout")',
 'add_ini_indicator_result(refs[i].ref)',
 'if(!results[j].region)results[j].region=refs[i].region',
 'draw_ini_spatial_top()',
 'draw_ini_spatial_plot(r,',
 'TASK_PANEL_PREVIEW',
 'result==v->length&&result<=EDIT_CAP&&ui_page==PAGE_DETAIL',
 'read_bytes(page,off,task_calibration,len)',
 'channel_live_value((int)(ch-channels),&value)',
 'bts_next_region_field(-1)',
 'bts_next_region_field(1)',
 'if(spatial){',
 'return bts_effective_region(i)==region',
)
for fragment in required:
    if fragment not in src:
        raise AssertionError(f"Missing spatial UI contract: {fragment}")
assert 'if(!strchr("SVOARCBKe",op)' in (ROOT / "firmware/esp32/main/main.c").read_text()
print("PASS original INI spatial layout: Outputs, Second VE, TPS/PPS/ETB and VVT")
print("PASS INI positions, state indicators, gauges, real read-only plots, spatial navigation")

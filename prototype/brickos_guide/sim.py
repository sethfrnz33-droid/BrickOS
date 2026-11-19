"""Offline, generated sample data for demonstrating diagnostics and revisions."""
from __future__ import annotations
from .core import GuidedEngine,SessionError
from .ini import parse_ini

DEMO_INI='''[TunerStudio]
signature = "rusEFI SAMPLE.only-for-offline-tests.101"
[Constants]
page = 1
engineCylinders = scalar, U08, 10, "count", 1, 0
idleRpmPid = scalar, F32, 20, "pid", 1, 0
veTable = array, U16, 30, [3x3], "%", 0.01, 0
page = 2
stftEnabled = bits, U08, 2, [0:0], "off", "on"
[OutputChannels]
rpm = scalar, U16, 0, "rpm", 1, 0
lambdaCurrentlyGood = bits, U08, 2, [0:0], "no", "yes"
Gego = scalar, F32, 4, "%", 100, 0
'''

def frame(ts,revision='revision-A',**values):
    return {'time_ms':ts,'revision':revision,
      'readings':{k:{'value':v,'age_ms':30,'valid':True} for k,v in values.items()}}

def demo():
    e=GuidedEngine()
    e.load_definition(DEMO_INI,parse_ini(DEMO_INI).signature,'revision-A')
    token=e.attach()['token']
    for i in range(12):
        e.ingest(frame(1000+i*100,rpm=850+(i%3-1)*18,coolant_c=82,map_kpa=44,
           idle_active=True,idle_target_rpm=850,idle_command_pct=25,
           lambda_actual=1.0,lambda_target=1.0,lambda_valid=True,
           stft_b1=1.09,ltft_b1=1.03,closed_loop=True,cranking=False,
           spark_deg=12.0,knock_active=False,ecu_fault=False),token)
    return e

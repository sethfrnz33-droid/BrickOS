"""Deterministic diagnostic experts; no control outputs and no automatic tuning changes.

No numeric fuel/spark tuning recommendations. Conclusions are evidence classifications
with explicit alternative explanations and missing validation requirements.
"""
from __future__ import annotations
from collections import deque
from statistics import pstdev
from .model import Finding, Sample

class EvidenceWindow:
    def __init__(self, capacity: int=128):
        self.samples: deque[Sample] = deque(maxlen=capacity)
        self.excluded: dict[str,int] = {}
    def append(self, s: Sample):
        self.samples.append(s)
    def values(self, key: str, revision: str | None = None, limit: int=30):
        frames = (s for s in self.samples if revision is None or s.revision == revision)
        result = [s.get(key) for s in frames]
        return [float(v) for v in result if isinstance(v, (float,int)) and not isinstance(v,bool)][-limit:]
    def count(self):
        return len(self.samples)

class Expert:
    name = 'base'
    def analyze(self, sample: Sample, window: EvidenceWindow):
        return []

class FoundationExpert(Expert):
    name = 'foundation'
    def analyze(self, s, w):
        out=[]
        for key, what in [('rpm','engine speed'),('coolant_c','coolant temperature'),
                          ('map_kpa','manifold pressure')]:
            if s.get(key) is None:
                out.append(Finding('CHANNEL_UNTRUSTED_'+key.upper(),'blocker',
                    f'{what.title()} reading unavailable',
                    'The channel is missing, invalid or stale; dependent analysis cannot use it.',
                    (f'Verify {key} scale, source and timestamp',), (key,), self.name))
        if s.get('ecu_fault') is True:
            out.append(Finding('ECU_REPORTED_FAULT','blocker','ECU fault reported',
                'Investigate the ECU-reported fault before interpreting calibration behavior.',
                ('Record the ECU diagnostic code and cause',),('ecu_fault',),self.name))
        return out

class StartExpert(Expert):
    name='starting'
    def analyze(self,s,w):
        crank = s.get('cranking')
        if crank is not True:
            return []
        missing = s.issues(('rpm','sync_valid','battery_v'))
        if missing:
            return [Finding('START_DIAGNOSTICS_MISSING','prerequisite',
              'Cranking event has incomplete telemetry', 'Starting behavior needs a trustworthy RPM, sync and battery record.',
              tuple('Verify '+m for m in missing),tuple(missing),self.name)]
        if not s.get('sync_valid'):
            return [Finding('TRIGGER_SYNC_NOT_VERIFIED','blocker',
              'Trigger synchronization not established',
              'A valid trigger/spark reference is a prerequisite for ignition or starting-fuel analysis.',
              ('Verify physical trigger and ignition reference with qualified supervision',),
              ('cranking','sync_valid'),self.name)]
        return [Finding('CRANK_EVENT_CAPTURED','information','Cranking evidence recorded',
            'The event is recorded for later comparison; one event cannot calibrate temperature-dependent starting.',
            ('Collect independent verified startup histories',),('cranking','rpm','battery_v'),self.name)]

class IdleExpert(Expert):
    name='idle'
    def analyze(self,s,w):
        if s.get('idle_active') is not True:
            return []
        need=s.issues(('rpm','idle_target_rpm','idle_command_pct'))
        if need:
            return [Finding('IDLE_SIGNALS_MISSING','prerequisite','Idle analysis needs more signals',
              'Idle target, actuator command and measured RPM are needed to distinguish controller behavior.',
              tuple('Verify '+n for n in need),tuple(need),self.name)]
        rpms=w.values('rpm',s.revision)
        if len(rpms) < 8:
            return [Finding('IDLE_MORE_EVIDENCE','evidence','Idle sample coverage incomplete',
                'A short observation cannot reliably characterize oscillation or stability.',
                ('Gather additional valid idle observations',),('rpm',),self.name)]
        variation=pstdev(rpms)
        if variation > 90:
            return [Finding('IDLE_VARIABILITY','diagnostic','Idle speed is varying',
               'Investigate airflow control, combustion, external loads and sensor validity before changing PID parameters.',
               ('Compare actuator command and RPM timing','Check disturbances and fueling context'),
               ('rpm','idle_command_pct','idle_target_rpm'),self.name)]
        return [Finding('IDLE_STABILITY_OBSERVED','information','Idle behavior appears stable in this window',
              'This is observational evidence only, not validation of all idle settings.',(),('rpm','idle_command_pct'),self.name)]

class FuelExpert(Expert):
    name='fuel'
    def analyze(self,s,w):
        if s.get('cranking') is True or s.get('fuel_cut') is True:
            return []
        mandatory=s.issues(('lambda_actual','lambda_target','lambda_valid'))
        if mandatory:
            return [Finding('FUEL_EVIDENCE_UNAVAILABLE','prerequisite','Fuel learning evidence incomplete',
               'No VE conclusion is possible without trusted measured/target lambda and sensor health.',
               tuple('Verify '+n for n in mandatory),tuple(mandatory),self.name)]
        if s.get('lambda_valid') is not True:
            return [Finding('LAMBDA_NOT_VALID','blocker','Wideband reading is not valid',
              'Fuel analysis is suspended until lambda validity is established.',
              ('Check lambda sensor and controller status',),('lambda_valid',),self.name)]
        if s.get('accel_enrichment') is True:
            return [Finding('TRANSIENT_NOT_VE','information','Transient fueling event excluded from VE',
                'Acceleration enrichment changes mixture independently of steady-state VE.',(),('accel_enrichment',),self.name)]
        trims = [s.get('stft_b1'),s.get('stft_b2'),s.get('ltft_b1'),s.get('ltft_b2')]
        present = [x for x in trims if x is not None]
        if present and s.get('closed_loop') is True and any(abs(float(x)-1.0) > 0.03 for x in present):
            return [Finding('FUEL_CORRECTION_MASKING','diagnostic','Fuel trims are masking base-model error',
                'A measured lambda close to target during closed-loop correction does not prove correct VE.',
                ('Verify bank-by-bank trim conventions and correction application order',),
                ('lambda_actual','lambda_target','stft_b1','ltft_b1'),self.name)]
        return [Finding('FUEL_OBSERVATION','information','Valid fueling observation collected',
            'Reading is usable for descriptive logging; this prototype computes no VE edits.',(),
            ('lambda_actual','lambda_target'),self.name)]

class SparkExpert(Expert):
    name='smart_spark'
    def analyze(self,s,w):
        if s.get('knock_active') is True:
            return [Finding('KNOCK_REPORTED','blocker','Knock-related intervention reported',
                'Investigate the indication and its sensor/configuration validity. Do not advance spark.',
                ('Review ECU knock configuration and diagnostic data',),('knock_active',),self.name)]
        if s.get('spark_deg') is None:
            return [Finding('SPARK_CHANNEL_MISSING','prerequisite','Actual spark reading unavailable',
                'Spark analysis needs actual applied advance and relevant corrections.',
                ('Locate actual spark output channel in the matched INI',),('spark_deg',),self.name)]
        return [Finding('MBT_NOT_PROVEN','evidence','Best-torque ignition is not established',
            'MAP, lambda and motion observations cannot independently demonstrate MBT; absence of knock is not permission to advance.',
            ('Obtain independently validated torque evidence under qualified supervision',),('spark_deg',),self.name)]

class MotionExpert(Expert):
    name='motion'
    def analyze(self,s,w):
        if s.get('imu_accel_ms2') is None:
            return []
        if s.get('vehicle_speed_kph') is None:
            return [Finding('MOTION_SPEED_MISSING','evidence','Vehicle-speed comparison unavailable',
                'IMU data alone does not establish engine torque.',('Validate speed sensor and IMU orientation',),
                ('imu_accel_ms2',),self.name)]
        return [Finding('MOTION_CORROBORATION_ONLY','information','Motion measurements available',
               'Grade, orientation, drag, drivetrain and measurement uncertainty preclude standalone MBT conclusions.',
               ('Validate mounting, speed scale and grade compensation',),
               ('imu_accel_ms2','vehicle_speed_kph'),self.name)]

EXPERTS=(FoundationExpert(),StartExpert(),IdleExpert(),FuelExpert(),SparkExpert(),MotionExpert())
PRIORITIES={'blocker':0,'prerequisite':1,'diagnostic':2,'evidence':3,'information':4}

def ranked_findings(s: Sample, window: EvidenceWindow):
    seen={}
    for expert in EXPERTS:
        for finding in expert.analyze(s,window):
            seen[finding.code]=finding
    return sorted(seen.values(),key=lambda f:(PRIORITIES.get(f.severity,99),f.expert,f.code))

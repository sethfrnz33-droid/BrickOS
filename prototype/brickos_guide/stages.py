"""Milestone planner with prerequisite-based actionable guidance, never completion by sample count alone."""
from __future__ import annotations
from dataclasses import dataclass

@dataclass(frozen=True)
class Stage:
    key: str
    title: str
    requires: tuple[str,...]
    evidence: tuple[str,...]
    description: str

STAGES = (
    Stage('definition','ECU / INI identity',(),('definition_match',),'Match the definition to ECU firmware and configuration structure.'),
    Stage('hardware','Hardware and sensors',('definition',),('sensor_plausibility','sensor_reference'),'Confirm actual hardware, reference scales and diagnostics.'),
    Stage('trigger','Trigger and ignition reference',('hardware',),('trigger_reference','sync_evidence'),'Establish synchronized crank/cam and real spark reference.'),
    Stage('start','First-start diagnostics',('trigger',),('start_history',),'Capture startup events and distinguish faults from calibration.'),
    Stage('idle','Idle airflow and feedback',('start',),('idle_air','idle_stability'),'Observe idle airflow authority, speed tracking and load disturbances.'),
    Stage('warmup','Warmup / after-start',('idle',),('temperature_coverage',),'Compare independent start/warmup events at verified temperatures.'),
    Stage('steady_fuel','Steady-state fuel model',('hardware','warmup'),('lambda_health','fuel_pressure','ve_coverage'),'Classify legitimate base-model errors and closed-loop corrections.'),
    Stage('fuel_trims','STFT and LTFT',('steady_fuel',),('trim_bank_verification',),'Separate bank-specific fuel feedback from underlying VE observations.'),
    Stage('transient','Transient fueling',('steady_fuel',),('transient_events',),'Analyze changing load and temporary enrichment separately from VE.'),
    Stage('smart_spark','Ignition evidence',('steady_fuel',),('ignition_reference','spark_observations'),'Track actual spark and knock; MBT requires independent torque evidence.'),
    Stage('advanced','Optional advanced features',('hardware',),('active_feature_inventory',),'Assess only installed and activated VVT, DBW, flex or boost features.'),
    Stage('dyno','Professional instrumented validation',('steady_fuel','smart_spark'),('external_torque_data','supervised_validation'),'Correlate independent measured performance with ECU data.'),
    Stage('release','Calibration review',('hardware','start','idle','steady_fuel'),('revision_audit','unresolved_findings'),'Summarize validated and unvalidated systems without forced flash saves.'),
)

def stage_plan(verified: set[str], observed: set[str], active: set[str] | None = None):
    active=active or {s.key for s in STAGES}
    result=[]
    for s in STAGES:
        enabled=s.key in active
        deps=[r for r in s.requires if r not in verified]
        missing=[e for e in s.evidence if e not in observed]
        state=('not_applicable' if not enabled else
               'blocked' if deps else
               'verified' if s.key in verified and not missing else
               'needs_evidence' if missing else 'ready_for_review')
        result.append({'key':s.key,'title':s.title,'status':state,
                       'blockers':deps,'missing_evidence':missing,
                       'description':s.description})
    return result

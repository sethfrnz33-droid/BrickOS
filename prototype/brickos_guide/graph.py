"""Cycle-tolerant dependency invalidation and deterministic task sorting."""
from __future__ import annotations
from collections import deque

class DependencyGraph:
    def __init__(self):
        self.downstream: dict[str, set[str]] = {}
        self.reasons: dict[tuple[str,str],str] = {}
    def add(self, upstream: str, dependent: str, why: str):
        if upstream == dependent:
            return
        self.downstream.setdefault(upstream,set()).add(dependent)
        self.reasons[(upstream,dependent)] = why
    def impacted(self, changed: set[str]) -> list[str]:
        visited = set(changed)
        q = deque(sorted(changed))
        results = set()
        while q:
            for next_name in sorted(self.downstream.get(q.popleft(), ())):
                if next_name not in visited:
                    visited.add(next_name)
                    results.add(next_name)
                    q.append(next_name)
        return sorted(results)
    def json(self):
        return [{'source': k, 'target': v, 'reason': self.reasons[(k,v)]}
                for k in sorted(self.downstream) for v in sorted(self.downstream[k])]

def default_graph():
    g = DependencyGraph()
    for src,dst,why in [
        ('ecu_identity','all_evidence','ECU definition must match'),
        ('injector_model','ve_evidence','Fuel modeling affects VE evidence'),
        ('lambda_calibration','ve_evidence','Measurement validity affects fueling evidence'),
        ('fuel_pressure','ve_evidence','Fuel differential pressure changes injector output'),
        ('ve_evidence','transient_evidence','Steady-state baseline required'),
        ('idle_air_model','idle_pid_evidence','Airflow baseline affects loop behavior'),
        ('coolant_sensor','warmup_evidence','Temperature axis must be valid'),
        ('coolant_sensor','idle_evidence','Target and warmup dependence'),
        ('spark_strategy','spark_evidence','Active correction path changed'),
        ('vehicle_speed_scale','motion_evidence','Speed/IMU comparison invalidated'),
        ('motion_mount','motion_evidence','Mounting orientation affects acceleration')]:
        g.add(src,dst,why)
    return g

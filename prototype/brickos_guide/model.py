"""Typed data contracts: missing and stale channels are never silently zero."""
from __future__ import annotations
from dataclasses import dataclass, asdict
from typing import Any

@dataclass(frozen=True)
class Reading:
    value: float | bool | None
    age_ms: int = 0
    valid: bool = True
    source: str = 'simulator'
    def trustworthy(self, freshness_ms: int = 500) -> bool:
        return self.value is not None and self.valid and 0 <= self.age_ms <= freshness_ms

@dataclass(frozen=True)
class Finding:
    code: str
    severity: str
    title: str
    explanation: str
    missing: tuple[str, ...] = ()
    evidence: tuple[str, ...] = ()
    expert: str = ''
    def json(self):
        d = asdict(self)
        d['missing'] = list(self.missing)
        d['evidence'] = list(self.evidence)
        return d

@dataclass(frozen=True)
class Decision:
    action: str
    status: str
    reason: str
    applicable: bool = True

@dataclass
class Sample:
    time_ms: int
    revision: str
    readings: dict[str, Reading]
    def get(self, name: str, freshness_ms: int = 500):
        value = self.readings.get(name)
        return value.value if value and value.trustworthy(freshness_ms) else None
    def issues(self, fields):
        return tuple(n for n in fields if self.get(n) is None)
    def json(self):
        return {'time_ms': self.time_ms, 'revision': self.revision,
                'readings': {n: asdict(v) for n,v in self.readings.items()}}
    @staticmethod
    def from_json(obj: dict[str, Any]) -> 'Sample':
        if not isinstance(obj, dict) or 'time_ms' not in obj or 'revision' not in obj:
            raise ValueError('time_ms and revision required')
        vals = obj.get('readings')
        if not isinstance(vals, dict):
            raise ValueError('readings must be an object')
        readings = {}
        for k, v in vals.items():
            if not isinstance(v, dict):
                raise ValueError(f'{k} must be an explicit Reading object')
            value = v.get('value')
            if value is not None and not isinstance(value, (int,float,bool)):
                raise ValueError(f'{k} invalid measurement')
            readings[str(k)] = Reading(value=value, age_ms=int(v.get('age_ms',0)),
                                       valid=bool(v.get('valid',True)),
                                       source=str(v.get('source','simulator')))
        return Sample(int(obj['time_ms']),str(obj['revision']),readings)

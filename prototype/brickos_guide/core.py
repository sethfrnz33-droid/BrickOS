"""Simulated BrickOS coordinator: INI identity, lease, evidence, priority, audit.

No physical ECU transport. No calibration write operation exists in this module.
"""
from __future__ import annotations
from dataclasses import asdict
import secrets
import time
from .ini import IniInventory, parse_ini
from .model import Sample
from .experts import EvidenceWindow, ranked_findings
from .graph import default_graph
from .registry import field_coverage
from .stages import stage_plan

class SessionError(RuntimeError):
    pass

class GuidedEngine:
    SCHEMA='0.1-prototype'
    LEASE_SECONDS=3.0
    def __init__(self, clock=None):
        self._clock = clock or time.monotonic
        self.inventory: IniInventory | None=None
        self.ecu_identity: str | None=None
        self.revision: str | None=None
        self.state='OFF'
        self.window=EvidenceWindow()
        self.graph=default_graph()
        self._controller_token: str | None=None
        self._lease_until=0.0
        self._session_id=None
        self.findings=[]
        self.audit=[]
        self.invalidated=set()
        self.last_timestamp=None
        self.verified_stages=set()
        self.stage_evidence=set()

    def _event(self, code: str, detail: str=''):
        self.audit.append({'code':code,'detail':detail,'at_ms':round(self._clock()*1000)})
        self.audit=self.audit[-128:]

    def load_definition(self, ini: str, ecu_signature: str, revision: str):
        inv=parse_ini(ini)
        if not ecu_signature or not revision or not inv.signature or inv.signature!=ecu_signature:
            self._revoke('DEFINITION_MISMATCH')
            self.state='INCOMPATIBLE'
            raise SessionError('ECU-reported signature does not match parsed INI signature')
        self._revoke('NEW_DEFINITION')
        self.inventory=inv
        self.ecu_identity=ecu_signature
        self.revision=revision
        self.state='VERIFIED_READ_ONLY'
        self.window=EvidenceWindow()
        self.findings=[]
        self.invalidated.clear()
        self.last_timestamp=None
        self.verified_stages=set()
        self.stage_evidence={'definition_match'}
        self._event('DEFINITION_LOADED',inv.fingerprint)
        return inv.summary()

    def _revoke(self,reason):
        if self._controller_token is not None:
            self._event('LEASE_REVOKED',reason)
        self._controller_token=None
        self._session_id=None
        self._lease_until=0.0

    def attach(self):
        if self.state not in ('VERIFIED_READ_ONLY','OBSERVING','SUSPENDED') or self.inventory is None:
            raise SessionError('No verified ECU definition')
        self._revoke('REPLACED_SESSION')
        self._controller_token=secrets.token_urlsafe(32)
        self._session_id=secrets.token_hex(8)
        self._lease_until=self._clock()+self.LEASE_SECONDS
        self.state='OBSERVING'
        self._event('SESSION_ATTACHED',self._session_id)
        return {'session_id':self._session_id,'token':self._controller_token,
                'lease_seconds':self.LEASE_SECONDS,'write_enabled':False}

    def heartbeat(self,token: str):
        self._require_session(token)
        self._lease_until=self._clock()+self.LEASE_SECONDS
        return {'state':self.state,'lease_seconds':self.LEASE_SECONDS}

    def _require_session(self,token: str):
        self.tick()
        if not isinstance(token,str) or self._controller_token is None or not secrets.compare_digest(token,self._controller_token):
            raise SessionError('Controller authorization missing or expired')

    def tick(self):
        if self._controller_token and self._clock() >= self._lease_until:
            self._revoke('HEARTBEAT_EXPIRED')
            self.state='SUSPENDED'
            self._event('OBSERVATION_SUSPENDED','heartbeat expired')

    def detach(self,token: str):
        self._require_session(token)
        self._revoke('DETACHED')
        self.state='VERIFIED_READ_ONLY'
        return {'state':self.state}

    def ingest(self, obj: dict, token: str):
        self._require_session(token)
        sample=Sample.from_json(obj)
        if sample.revision!=self.revision:
            self._revoke('CALIBRATION_REVISION_CHANGED')
            self.state='SUSPENDED'
            raise SessionError('Calibration revision changed; evidence invalidated')
        if self.last_timestamp is not None and sample.time_ms<=self.last_timestamp:
            raise ValueError('Sample timestamps must strictly increase')
        self.last_timestamp=sample.time_ms
        self.window.append(sample)
        if sample.get('lambda_valid') is True: self.stage_evidence.add('lambda_health')
        if sample.get('sync_valid') is True: self.stage_evidence.add('sync_evidence')
        if sample.get('idle_active') is True: self.stage_evidence.add('idle_stability')
        self.findings=ranked_findings(sample,self.window)
        self._event('SAMPLE_ASSESSED',str(sample.time_ms))
        return self.report()

    def calibration_changed(self,changed_fields: set[str],new_revision: str):
        """Notification from manual editor/simulator. No writes occur here."""
        if not new_revision or new_revision == self.revision:
            raise ValueError('Provide the updated calibration revision')
        touched=self.graph.impacted(set(changed_fields))
        self.invalidated.update(touched)
        self.revision=new_revision
        self._revoke('MANUAL_CALIBRATION_EDIT')
        self.state='SUSPENDED'
        self._event('EVIDENCE_INVALIDATED',','.join(touched))
        return touched

    def report(self):
        self.tick()
        primary=self.findings[0].json() if self.findings else None
        return {'schema_version':self.SCHEMA,'state':self.state,
                'ecu_signature':self.ecu_identity,'definition_hash':self.inventory.fingerprint if self.inventory else None,
                'configuration_revision':self.revision,
                'session_id':self._session_id,'sample_count':self.window.count(),
                'controller_connected':bool(self._controller_token),
                'write_enabled':False,'flash_burn_enabled':False,
                'primary_recommendation':primary,
                'findings':[f.json() for f in self.findings],
                'stale_evidence':sorted(self.invalidated),
                'inventory_summary':self.inventory.summary() if self.inventory else None,
                'coverage':field_coverage(self.inventory) if self.inventory else None,
                'stages':stage_plan(self.verified_stages,self.stage_evidence)}

    def field_inventory(self,page=0,limit=100):
        if self.inventory is None:
            return {'items':[],'total':0}
        if not 0<=page<=10000 or not 1<=limit<=100:
            raise ValueError('bad pagination')
        start=page*limit
        return {'items':[v.json() for v in self.inventory.fields[start:start+limit]],
                'unknown':self.inventory.unknown if page==0 else [],
                'total':len(self.inventory.fields), 'page':page}

import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from brickos_guide.ini import parse_ini, remove_comment
from brickos_guide.graph import DependencyGraph, default_graph
from brickos_guide.core import GuidedEngine,SessionError
from brickos_guide.sim import DEMO_INI,frame,demo

class FakeClock:
    def __init__(self): self.now=10.
    def __call__(self): return self.now
    def advance(self, seconds): self.now+=seconds

def engine(clock=None):
    e=GuidedEngine(clock)
    e.load_definition(DEMO_INI,parse_ini(DEMO_INI).signature,'revision-A')
    return e

def default_values(**kwargs):
    v=dict(rpm=850,coolant_c=80,map_kpa=42,lambda_actual=1.,lambda_target=1.,
           lambda_valid=True,spark_deg=12.,idle_active=True,idle_target_rpm=850,
           idle_command_pct=21.,closed_loop=False,knock_active=False,cranking=False)
    v.update(kwargs)
    return v

class IniTests(unittest.TestCase):
    def test_page_identity(self):
        inv=parse_ini(DEMO_INI)
        self.assertIn('Constants:1:veTable',[f.key for f in inv.fields])
        self.assertIn('Constants:2:stftEnabled',[f.key for f in inv.fields])
    def test_signature(self): self.assertEqual(parse_ini(DEMO_INI).signature,'rusEFI SAMPLE.only-for-offline-tests.101')
    def test_comments_in_quotes(self): self.assertEqual(remove_comment('x = "a;b" ; ignored'),'x = "a;b"')
    def test_unknown_preserved(self): self.assertEqual(parse_ini('[Constants]\nfunny = wizard, 4').unknown[0]['reason'],'unknown_definition_kind')
    def test_directives_unprocessed(self): self.assertEqual(parse_ini('#if SPECIAL\n[Constants]\nx = scalar, U08, 0').directives[0]['status'],'not_evaluated')
    def test_sections_do_not_confuse_fields(self): self.assertEqual(len(parse_ini('[Menu]\ntitle = "x"\n[OutputChannels]\nrpm = scalar, U16, 0').fields),1)
    def test_digest_changes(self): self.assertNotEqual(parse_ini(DEMO_INI).fingerprint,parse_ini(DEMO_INI+'\n').fingerprint)

class GraphTests(unittest.TestCase):
    def test_transitive(self): self.assertEqual(default_graph().impacted({'injector_model'}),['transient_evidence','ve_evidence'])
    def test_cycle(self):
        g=DependencyGraph();g.add('A','B','x');g.add('B','A','y')
        self.assertEqual(g.impacted({'A'}),['B'])
    def test_reason(self): self.assertIn('Fuel modeling',next(x['reason'] for x in default_graph().json() if x['source']=='injector_model'))

class CoordinatorTests(unittest.TestCase):
    def test_mismatched_signature_rejects(self):
        with self.assertRaises(SessionError): GuidedEngine().load_definition(DEMO_INI,'wrong','rev')
    def test_no_attach_before_definition(self):
        with self.assertRaises(SessionError): GuidedEngine().attach()
    def test_attach_readonly(self):
        e=engine(); ticket=e.attach()
        self.assertFalse(ticket['write_enabled']);self.assertFalse(e.report()['flash_burn_enabled'])
    def test_bad_token(self):
        e=engine();e.attach()
        with self.assertRaises(SessionError): e.ingest(frame(1,**default_values()),'bad')
    def test_lease_expires(self):
        c=FakeClock();e=engine(c);t=e.attach()['token'];c.advance(3.1)
        with self.assertRaises(SessionError):e.heartbeat(t)
        self.assertEqual(e.report()['state'],'SUSPENDED')
    def test_lease_renew(self):
        c=FakeClock();e=engine(c);t=e.attach()['token'];c.advance(2.)
        e.heartbeat(t);c.advance(2.)
        self.assertEqual(e.report()['state'],'OBSERVING')
    def test_out_of_order_sample(self):
        e=engine();t=e.attach()['token'];e.ingest(frame(100,**default_values()),t)
        with self.assertRaises(ValueError):e.ingest(frame(100,**default_values()),t)
    def test_revision_change_blocks(self):
        e=engine();t=e.attach()['token']
        with self.assertRaises(SessionError):e.ingest(frame(1,revision='different',**default_values()),t)
        self.assertFalse(e.report()['controller_connected'])
    def test_manual_change_invalidation(self):
        e=engine();e.attach(); changed=e.calibration_changed({'injector_model'},'revision-B')
        self.assertEqual(changed,['transient_evidence','ve_evidence'])
        self.assertEqual(e.report()['state'],'SUSPENDED')
    def test_inventory_pagination(self):
        e=engine();self.assertEqual(len(e.field_inventory(0,1)['items']),1)
        with self.assertRaises(ValueError):e.field_inventory(-1,1)
    def test_detach(self):
        e=engine();t=e.attach()['token'];e.detach(t)
        self.assertEqual(e.report()['state'],'VERIFIED_READ_ONLY')

class ExpertTests(unittest.TestCase):
    def test_sensor_invalid_blocks(self):
        e=engine();t=e.attach()['token'];s=frame(1,**default_values())
        s['readings']['coolant_c']['valid']=False
        codes=[x['code'] for x in e.ingest(s,t)['findings']]
        self.assertIn('CHANNEL_UNTRUSTED_COOLANT_C',codes)
    def test_stale_sensor_blocks(self):
        e=engine();t=e.attach()['token'];s=frame(1,**default_values())
        s['readings']['rpm']['age_ms']=800
        self.assertEqual(e.ingest(s,t)['primary_recommendation']['code'],'CHANNEL_UNTRUSTED_RPM')
    def test_crank_no_sync(self):
        e=engine();t=e.attach()['token']; data=default_values(cranking=True,sync_valid=False,battery_v=12.)
        codes=[x['code'] for x in e.ingest(frame(1,**data),t)['findings']]
        self.assertIn('TRIGGER_SYNC_NOT_VERIFIED',codes)
    def test_closed_loop_trim_mask(self):
        e=engine();t=e.attach()['token'];data=default_values(closed_loop=True,stft_b1=1.12)
        self.assertIn('FUEL_CORRECTION_MASKING',[x['code'] for x in e.ingest(frame(1,**data),t)['findings']])
    def test_accel_enrichment_excludes_ve(self):
        e=engine();t=e.attach()['token'];data=default_values(accel_enrichment=True)
        self.assertIn('TRANSIENT_NOT_VE',[x['code'] for x in e.ingest(frame(1,**data),t)['findings']])
    def test_knock_signal_blocks(self):
        e=engine();t=e.attach()['token'];data=default_values(knock_active=True)
        self.assertIn('KNOCK_REPORTED',[x['code'] for x in e.ingest(frame(1,**data),t)['findings']])
    def test_mbt_not_proven(self):
        e=engine();t=e.attach()['token']
        self.assertIn('MBT_NOT_PROVEN',[x['code'] for x in e.ingest(frame(1,**default_values()),t)['findings']])
    def test_idle_variability(self):
        e=engine();t=e.attach()['token']
        for i in range(12):
            values=default_values(rpm=650 if i%2==0 else 1050)
            result=e.ingest(frame(1+100*i,**values),t)
        self.assertIn('IDLE_VARIABILITY',[x['code'] for x in result['findings']])
    def test_imu_only(self):
        e=engine();t=e.attach()['token'];data=default_values(imu_accel_ms2=0.2)
        self.assertIn('MOTION_SPEED_MISSING',[x['code'] for x in e.ingest(frame(1,**data),t)['findings']])
    def test_simulator(self):
        r=demo().report()
        self.assertEqual(r['sample_count'],12)
        self.assertFalse(r['write_enabled'])
        self.assertEqual(r['primary_recommendation']['code'],'FUEL_CORRECTION_MASKING')

if __name__=='__main__': unittest.main()

class PlanningTests(unittest.TestCase):
    def test_unverified_rule_not_writable(self):
        from brickos_guide.registry import field_coverage
        f=field_coverage(parse_ini(DEMO_INI))
        self.assertFalse(f['complete_semantic_coverage'])
        self.assertTrue(all(not x['write_allowed'] for x in f['entries']))
    def test_all_stages_present(self):
        from brickos_guide.stages import stage_plan
        p=stage_plan(set(),set())
        self.assertGreaterEqual(len(p),13)
        self.assertEqual(p[0]['status'],'needs_evidence')
        self.assertEqual(p[1]['status'],'blocked')
    def test_loaded_definition_provides_first_stage_evidence(self):
        e=engine();r=e.report()
        self.assertEqual(r['stages'][0]['status'],'ready_for_review')
        self.assertTrue(r['coverage']['total_fields'] > 0)
    def test_stage_is_not_completed_by_samples(self):
        e=demo()
        self.assertNotIn('verified',[s['status'] for s in e.report()['stages']])

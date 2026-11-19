"""Tests of the read-only INI research scanner; synthetic fixture only."""
import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('ini_research_scan', ROOT / 'ini_research_scan.py')
MOD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MOD)


SAMPLE = r'''
[TunerStudio]
signature = "TEST generated INI v1"
defaultRuntimeRecordPerSec = 100;
[Constants]
nPages = 2
pageSize = 32, 16
endianness = little
page = 1
veTable = array, U16, 0, [2x2], "%", 0.1, 0
veDisplay = array, U16, 0, [2x2], "%", 0.1, 0, noMsqSave
idleTarget = scalar, U16, 8, "rpm", 1, 0
oddType = bits, U08, , 10, [0:2], "no", "yes"
page = 2
ltft = array, F32, 0, [2], "%", 100, -1, noMsqSave
[OutputChannels]
ochBlockSize = 12
RPMValue = scalar, U16, 0, "RPM", 1, 0
engine = bits, U08, 2, [0:1]
[TableEditor]
table = veTbl, veMap, "VE", 1
[Menu]
subMenu = veDialog, "VE"
[UserDefined]
commandButton = "Dangerous action", cmd_test
[ControllerCommands]
cmd_test = "Z\x00"
'''


class IniResearchScannerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.path = pathlib.Path(self.temporary.name) / 'test.ini'
        self.path.write_bytes(SAMPLE.encode('cp1252'))

    def test_preserves_anomalies_without_reinterpreting_offsets(self):
        result = MOD.scan(self.path)
        self.assertFalse(result['live_ecu_signature_verified'])
        self.assertTrue(result['read_only'])
        self.assertEqual(result['page_sizes_bytes'], [32, 16])
        self.assertEqual(result['config']['ochBlockSize'], 12)
        self.assertEqual(result['counts']['calibration_fields_parsed'], 4)
        self.assertEqual(result['counts']['unparsed_calibration_declarations'], 1)
        self.assertEqual(result['counts']['output_fields'], 2)
        self.assertEqual(result['counts']['table_editors'], 1)
        self.assertEqual(result['counts']['menu_submenus'], 1)
        self.assertEqual(result['counts']['command_buttons'], 1)
        self.assertEqual(result['anomalies'][0]['field'], 'oddType')
        self.assertEqual(result['same_address_array_groups'][0]['names'], ['veTable', 'veDisplay'])

    def test_page_overflow_is_reported_without_loading_or_writing_ecu(self):
        self.path.write_text(SAMPLE.replace('idleTarget = scalar, U16, 8',
                                            'idleTarget = scalar, U16, 32'), encoding='cp1252')
        result = MOD.scan(self.path)
        self.assertTrue(any(x.get('reason') == 'field overflows page' for x in result['anomalies']))

    def test_declared_commands_are_inventoried_not_executed(self):
        report = MOD.scan(self.path)
        self.assertEqual(report['command_buttons'][0]['entry'], 'commandButton = "Dangerous action", cmd_test')
        self.assertTrue(report['read_only'])


if __name__ == '__main__':
    unittest.main()

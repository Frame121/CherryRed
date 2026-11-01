import importlib.util
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / 'tools' / 'audit_firmware_dependencies.py'
spec = importlib.util.spec_from_file_location('firmware_audit', SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class FirmwareAuditTests(unittest.TestCase):
    def test_detects_declared_reference_and_compressed_candidate(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); src = root / 'src'; fw = root / 'fw'
            (src / 'amdgpu').mkdir(parents=True); fw.mkdir()
            (src / 'amdgpu' / 'gpu.c').write_text('MODULE_FIRMWARE("amdgpu/gc_11_0_2_pfp.bin");\n')
            (fw / 'gc_11_0_2_pfp.bin.zst').write_bytes(b'fixture')
            result = module.scan(src, fw, ('gc_11_0_2_',))
            self.assertEqual(result['firmware_reference_count'], 1)
            row = result['entries'][0]
            self.assertTrue(row['declared_module_firmware'])
            self.assertTrue(row['local_file_present'])
            self.assertIn('not proof', row['evidence_limit'])

    def test_macro_reference_without_local_payload_is_not_claimed_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); src = root / 'src'; fw = root / 'fw'
            src.mkdir(); fw.mkdir()
            (src / 'vcn.c').write_text('#define FW_NAME "amdgpu/vcn_4_0_4.bin"\n')
            row = module.scan(src, fw)['entries'][0]
            self.assertFalse(row['declared_module_firmware'])
            self.assertFalse(row['local_file_present'])

    def test_missing_roots_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaises(ValueError):
                module.scan(root / 'missing', root)

if __name__ == '__main__':
    unittest.main()

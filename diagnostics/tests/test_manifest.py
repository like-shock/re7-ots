import importlib.util, pathlib, unittest, os
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('extract_manifest',ROOT/'tools/extract_manifest.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class ManifestTests(unittest.TestCase):
 def test_original_has_all_requested_sites(self):
  path=pathlib.Path(os.environ.get('RE7_CT_FILE',str(ROOT.parent/'original/re7.CT')))
  if not path.exists():self.skipTest('Original CT is not redistributed. Set RE7_CT_FILE for re-extraction test.')
  raw=path.read_bytes();m=mod.extract(raw)
  self.assertEqual(len(m['patches']),78)
  cam=next(x for x in m['patches'] if x['name']=='CAM_WRITES')
  self.assertEqual(cam['delta'],-26);self.assertEqual(len(cam['expected'].split()),31)
  self.assertEqual(m['source_sha256'],mod.EXPECTED_HASH)
  self.assertEqual(sum(x['group']=='fullbright' for x in m['patches']),7)
 def test_rejects_wrong_input(self):
  with self.assertRaises(ValueError):mod.extract(b'<CheatTable/>')
if __name__=='__main__':unittest.main()

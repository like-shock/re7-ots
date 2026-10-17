import pathlib,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class ProbeLifecycleTests(unittest.TestCase):
 def test_fail_closed_and_never_free_published_trampolines(self):
  with tempfile.TemporaryDirectory() as d:
   exe=str(pathlib.Path(d)/'lifecycle')
   subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(ROOT/'tests/probe_lifecycle_host.c'),str(ROOT/'src/probe_lifecycle.c'),'-o',exe],check=True)
   self.assertIn('PASS',subprocess.check_output([exe],text=True))
